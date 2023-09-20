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
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/param.h>
#include <linux/spi/spidev.h>

#include <rtapi_slab.h>
#include <rtapi_list.h>

#include "hal.h"
#include "rtapi.h"
#include "rtapi_app.h"
#include "rtapi_string.h"

#include "litexcnc_spi.h"


static char *connection_string[MAX_SPI_BOARDS];
RTAPI_MP_ARRAY_STRING(connection_string, MAX_SPI_BOARDS, "Connection string.")

// This keeps track of the component id. Required for setup and tear down.
static int comp_id;

// List with boards using this communication
static int boards_count = 0;
static struct rtapi_list_head board_num;
static struct rtapi_list_head ifnames;
static litexcnc_spi_t* boards[MAX_SPI_BOARDS];

/**
 * Parameter which contains the registration of this board with LitexCNC 
 */
static litexcnc_driver_registration_t *registration;


/*******************************************************************************
 * Registers this SPI-driver within LitexCNC driver. Gets called from litexcnc.c
 * when a user connects to a card using the connection-string `spi:<file-descriptor>`.
 * In case a user does not connect to this type of connection, the driver is not
 * loaded at all.
 ******************************************************************************/
int register_spi_driver(void) {
    registration = (litexcnc_driver_registration_t *)hal_malloc(sizeof(litexcnc_driver_registration_t));
    rtapi_snprintf(registration->name, sizeof(registration->name), "spi");
    registration->initialize_driver = *initialize_driver;
    return litexcnc_register_driver(registration);
}
EXPORT_SYMBOL_GPL(register_spi_driver);


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
    litexcnc_spi_t *board = this->private;
    
    // TODO
}


/*******************************************************************************
 * This function reads the status registers from the FPGA
 *
 * @param this    Pointer to the FPGA to read the data from.
 ******************************************************************************/
static int litexcnc_spi_read(litexcnc_fpga_t *this) {
    litexcnc_spi_t *board = this->private;
    
    // TODO
    
    // Successful read
    return 0;
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
    /*
     * This function sends N bits of data to the FPGA.
     */
    
    // TODO

    // Indicate success (writing does not give back a status)
    return 0;
}


/*******************************************************************************
 * This function writes the status registers to the FPGA
 *
 * @param this    Pointer to the FPGA to write the data to.
 ******************************************************************************/
static int litexcnc_spi_write(litexcnc_fpga_t *this) {
    litexcnc_spi_t *board = this->private;
    
    // TODO
    
    // Successful read
    return 0;
}


static int litexcnc_init_buffers(litexcnc_fpga_t *this) {
    litexcnc_spi_t *board = this->private;

    // TODO

    return 0;
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
    boards[boards_count] = (litexcnc_spi_t *)hal_malloc(sizeof(litexcnc_spi_t));
    boards[boards_count]->connection = open(connection_string, O_RDWR);
    if (boards[boards_count]->connection < 0) {
        fprintf(stderr, "main: opening device file: %s: %s\n", connection_string, strerror(errno));
        return errno;
    }
    // ret = connect_board(boards[boards_count], connection_string);
    // if (ret < 0) return ret;
    // Create an FPGA instance
    boards[boards_count]->fpga.comp_id           = comp_id;
    boards[boards_count]->fpga.read_n_bits       = litexcnc_spi_read_n_bytes;
    boards[boards_count]->fpga.read              = litexcnc_spi_read;
    boards[boards_count]->fpga.read_header_size  = 8;
    boards[boards_count]->fpga.write_n_bits      = litexcnc_spi_write_n_bytes;
    boards[boards_count]->fpga.write             = litexcnc_spi_write;
    boards[boards_count]->fpga.write_header_size = 8;
    boards[boards_count]->fpga.private           = boards[boards_count];
    // Register the board with the main function
    ret = litexcnc_register(&boards[boards_count]->fpga);
    if (ret != 0) {
        rtapi_print("board fails LitexCNC registration\n");
        return ret;
    }
    // Create a pin to show debug messages
    ret = hal_param_bit_newf(HAL_RW, &(boards[boards_count]->hal.param.debug), comp_id, "%s.debug", boards[boards_count]->fpga.name);
    if (ret < 0) {
        LITEXCNC_ERR_NO_DEVICE("Error adding pin '%s.debug', aborting\n", boards[boards_count]->fpga.name);
        return ret;
    }
    // Create the pins for the board and write headers to the buffers
    ret = litexcnc_init_buffers(&boards[boards_count]->fpga);
    if (ret != 0) {
        rtapi_print("Failed to create pins and params for the board\n");
        return ret;
    }
    // Proceed to the next board
    boards_count++;
    return 0;
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
