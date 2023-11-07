//
//    Copyright (C) 2022 Peter van Tol
//
//    This program is free software; you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation; either version 2 of the License, or
//    (at your option) any later version.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with this program; if not, write to the Free Software
//    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
//
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <dlfcn.h>
#include <errno.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/param.h>
#include <sys/types.h>

#include <rtapi_slab.h>
#include <rtapi_list.h>

#include "hal.h"
#include "rtapi.h"
#include "rtapi_app.h"
#include "rtapi_string.h"

#include "litexcnc_pigpio.h"


static char *connection_string[MAX_SPI_BOARDS];
RTAPI_MP_ARRAY_STRING(connection_string, MAX_SPI_BOARDS, "Connection string.")

// This keeps track of the component id. Required for setup and tear down.
static int comp_id;

// List with boards using this communication
static int boards_count = 0;
static struct rtapi_list_head board_num;
static struct rtapi_list_head ifnames;
static litexcnc_pigpio_t* boards[MAX_SPI_BOARDS];

/**
 * Parameter which contains the registration of this board with LitexCNC 
 */
static litexcnc_driver_registration_t *registration;

/*
 * Parameters for SPI connection (prevent magic numbers in the code)
 **/
static uint32_t speed = 1000000;

/*
 * Definitions of the functions from `pigpio`, the functions are loaded
 * dynamically to prevent trouble with linking shared libraries.
 **/
int (*gpioInitialise)(void);
int (*spiOpen)(unsigned, unsigned, unsigned);
int (*spiXfer)(unsigned, char*, char*, unsigned);
int (*spiClose)(unsigned);
int (*gpioTerminate)(void);


/*******************************************************************************
 * Registers this SPI-driver within LitexCNC driver. Gets called from litexcnc.c
 * when a user connects to a card using the connection-string `spi:<file-descriptor>`.
 * In case a user does not connect to this type of connection, the driver is not
 * loaded at all.
 ******************************************************************************/
int register_pigpio_driver(void) {
    // Create registration
    registration = (litexcnc_driver_registration_t *)hal_malloc(sizeof(litexcnc_driver_registration_t));
    rtapi_snprintf(registration->name, sizeof(registration->name), "pigpio");
    registration->initialize_driver = *initialize_driver;

    // Dynamically load the driver
    void *pigpio = dlopen("libpigpio.so", RTLD_GLOBAL | RTLD_NOW);
    if (!pigpio) {
        fprintf(stderr, "Error while loading `pigpio`: %s\n", dlerror());
        return -1;
    }
    gpioInitialise = dlsym(pigpio, "gpioInitialise");
    spiOpen = dlsym(pigpio, "spiOpen");
    spiXfer = dlsym(pigpio, "spiXfer");
    spiClose = dlsym(pigpio, "spiClose");
    gpioTerminate = dlsym(pigpio, "gpioTerminate");

    return litexcnc_register_driver(registration);
}
EXPORT_SYMBOL_GPL(register_pigpio_driver);


/*******************************************************************************
 * This function reads N bytes of data from the FPGA starting from the given
 * address. This function is used to read one-off data from the FPGA, such as
 * during initialisation, configuration and reset.
 *
 * @param this    Pointer to the FPGA to read the data from.
 * @param address The address to start the read from.
 * @param data    The array where the read data is stored in.
 * @param N       The number of the bytes to read. Must be equal to the length 
 *                of @param data. 
 ******************************************************************************/
static int litexcnc_spi_read_n_bytes(litexcnc_fpga_t *this, size_t address, uint8_t *data, size_t N) {
    litexcnc_pigpio_t *board = this->private;
    
    // Create the required buffers
    static uint8_t tx_buf[5+2+256];
    memset((void*) tx_buf, 5, 2+N);
    static uint8_t rx_buf[5+2+256];
    memset((void*) rx_buf, 0, 5+2+N);

     // Write data to package
    // - command and size
    size_t words = N >> 2;
    tx_buf[0] = 0x40 + (words & 0x1F);
    // - address
    uint32_t address_be = htobe32(address);
    memcpy(&tx_buf[1], &address_be, 4);
    
    // Create the request and send the data 
    int ret = spiXfer(board->connection, tx_buf, rx_buf, 5 + N + 2);
	if (ret < 1) {
        LITEXCNC_ERR("Could not read from SPI device\n", this->name);
		return -1;
    }
    
    // Check whether write was successfull
    for(size_t i = 0; i < (5 + 2); i++) {
        if(rx_buf[i] == 0x01) {
            // Copy the results to the output
            memcpy(data, &rx_buf[i+1], N);
            // Indicate success
            return 0;
        }
    }

    // Writing has failed
    LITEXCNC_ERR("Read from SPI device was unsuccessful.\n", this->name);
    return -1;

}


/*******************************************************************************
 * This function reads the status registers from the FPGA. IT is relyaed to the
 * standard function litexcnc_spi_read_n_bytes.
 *
 * @param this    Pointer to the FPGA to read the data from.
 ******************************************************************************/
static int litexcnc_spi_read(litexcnc_fpga_t *this) {
    return litexcnc_spi_read_n_bytes(
        this, 
        this->read_base_address, 
        this->read_buffer, 
        this->read_buffer_size);
}


/*******************************************************************************
 * This function writes N bytes of data to the FPGA starting from the given
 * address. This function is used to write one-off data from the FPGA, such as
 * during reset.
 *
 * @param this    Pointer to the FPGA to write the data from.
 * @param address The address to start the write from.
 * @param data    The array where the data to be written stored in.
 * @param N       The number of the bytes to write. Must be equal to the length 
 *                of @param data. 
 ******************************************************************************/
static int litexcnc_spi_write_n_bytes(litexcnc_fpga_t *this, size_t address, uint8_t *data, size_t N) {
    litexcnc_pigpio_t *board = this->private;
    
    // Create the required buffers
    static uint8_t tx_buf[5+2+256];
    static uint8_t rx_buf[5+2+256];
    memset((void*) rx_buf, 0, 5+2+N);

     // Write data to package
    // - command and size
    size_t words = N >> 2;
    tx_buf[0] = 0x80 + (words & 0x1F);
    // - address
    uint32_t address_be = htobe32(address);
    memcpy(&tx_buf[1], &address_be, 4);
    // - data
    memcpy(&tx_buf[5], data, N);

    // Create the request and send the data 
    int ret = spiXfer(board->connection, tx_buf, rx_buf, 5 + N + 2);
	if (ret < 1) {
        LITEXCNC_ERR("Could not write to SPI device\n", this->name);
		return -1;
    }

    // Check whether write was successfull
    for(size_t i = 0; i < (5 + N + 2); i++) {
        if(rx_buf[i] == 0x01) {
            // Indicate success
            return 0;
        }
    }

    // Writing has failed
    LITEXCNC_ERR("Write to SPI device was unsuccessful.\n", this->name);
    return -1;
}


/*******************************************************************************
 * This function writes the status registers to the FPGA
 *
 * @param this    Pointer to the FPGA to write the data to.
 ******************************************************************************/
static int litexcnc_spi_write(litexcnc_fpga_t *this) {
    return litexcnc_spi_write_n_bytes(
        this, 
        this->write_base_address, 
        this->write_buffer, 
        this->write_buffer_size);
}


/*******************************************************************************
 * Initializes the driver for a connection to a FPGA with the given connection
 * string.
 * 
 * NOTE: the connection-string is already stripped from the `spi:` part before
 * entering this routine.
 *
 * @param connection_string The file descriptor to the SPI driver
 * @param comp_id           The id of the component which initializes the driver
 ******************************************************************************/
static int initialize_driver(char *connection_string, int comp_id) {
    size_t ret;
    boards[boards_count] = (litexcnc_pigpio_t *)hal_malloc(sizeof(litexcnc_pigpio_t));

    // Setuid is required for root acces to /dev/mem
    static uid_t euid, ruid;
    ruid = getuid();
    euid = geteuid();
#ifdef _POSIX_SAVED_IDS
    ret = seteuid(ruid);
#else
    ret = setreuid(ruid, euid);
#endif
    if (ret < 0) {
        fprintf (stderr, "Couldn't set uid.\n");
        return ret;
    }

    // Initialize GPIO
    ret = gpioInitialise();
    if (ret < 0) return ret;

    // Open the SPI channel
    //  - Check whether the connection contains a colon (:), which indicates
    //    the split between SPI channel and the speed.
    char *conn_str_ptr = strchr(connection_string, ':');
    if (conn_str_ptr != NULL) {
        *conn_str_ptr = '\0';          // Replace ':' with a null terminator
        ++conn_str_ptr;                // Move port pointer forward
        speed = atoi(conn_str_ptr);    // Convert the speed to an integer
    }
    boards[boards_count]->connection = spiOpen(atoi(connection_string), speed, 0);
    if (boards[boards_count]->connection < 0) {
        fprintf(stderr, "main: opening device file: %s: %s\n", connection_string, strerror(errno));
        gpioTerminate();
        return errno;
    }
    
    // Create an FPGA instance
    boards[boards_count]->fpga.comp_id           = comp_id;
    boards[boards_count]->fpga.read_n_bits       = litexcnc_spi_read_n_bytes;
    boards[boards_count]->fpga.read              = litexcnc_spi_read;
    boards[boards_count]->fpga.read_header_size  = 0;
    boards[boards_count]->fpga.write_n_bits      = litexcnc_spi_write_n_bytes;
    boards[boards_count]->fpga.write             = litexcnc_spi_write;
    boards[boards_count]->fpga.write_header_size = 0;
    boards[boards_count]->fpga.private           = boards[boards_count];
    boards[boards_count]->fpga.terminate         = terminate_driver;

    // Register the board with the main function
    ret = litexcnc_register(&boards[boards_count]->fpga);
    if (ret != 0) {
        rtapi_print("board fails LitexCNC registration\n");
        terminate_driver(&boards[boards_count]->fpga);
        return ret;
    }
    // Create a pin to show debug messages
    ret = hal_param_bit_newf(HAL_RW, &(boards[boards_count]->hal.param.debug), comp_id, "%s.debug", boards[boards_count]->fpga.name);
    if (ret < 0) {
        terminate_driver(&boards[boards_count]->fpga);
        LITEXCNC_ERR_NO_DEVICE("Error adding pin '%s.debug', aborting\n", boards[boards_count]->fpga.name);
        return ret;
    }
    // Proceed to the next board
    boards_count++;
    return 0;
}

/*******************************************************************************
 * This function releases the resources used by this dirver
 *
 * @param this    Pointer to the driver to terminate.
 ******************************************************************************/
static int terminate_driver(litexcnc_fpga_t *this) {
    LITEXCNC_PRINT("Unloading driver...\n", this->name);
    litexcnc_pigpio_t *board = this->private;
    spiClose(board->connection);
    gpioTerminate();
}


/*******************************************************************************
 * Main function, gets called when the module is loaded as stand-alone. This is
 * not supported; the user will get an error message and LinuxCNC is terminated.
 ******************************************************************************/
int rtapi_app_main(void) {
    LITEXCNC_ERR_NO_DEVICE("ERROR: Direct usage of the module `litexcnc_spi` is not supported\n");
    LITEXCNC_ERR_NO_DEVICE("This is caused by the following loadrt-commands in your HAL-file:\n");
    LITEXCNC_ERR_NO_DEVICE("    loadrt litexcnc\n");
    LITEXCNC_ERR_NO_DEVICE("    loadrt litexcnc_spi connection_string=\"%s\"\n", connection_string[0]);
    LITEXCNC_ERR_NO_DEVICE("Please use the folllowing single command in your hal-file instead:\n");
    LITEXCNC_ERR_NO_DEVICE("    loadrt litexcnc connections=\"spi:%s\"\n", connection_string[0]);
    LITEXCNC_ERR_NO_DEVICE("For more information, see: https://github.com/Peter-van-Tol/LiteX-CNC/issues/32 \n");
    LITEXCNC_ERR_NO_DEVICE("Stopping LinuxCNC now!\n");
    return -1;
}

// Add any required files here, because hal_compile cannot cope with loose files
