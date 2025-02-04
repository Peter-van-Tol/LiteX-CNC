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

#include <rtapi_slab.h>
#include <rtapi_list.h>

#include "hal.h"
#include "rtapi.h"
#include "rtapi_app.h"
#include "rtapi_string.h"

#include "etherbone.h"
#include "litexcnc_eth.h"

static char *connection_string[MAX_ETH_BOARDS];
RTAPI_MP_ARRAY_STRING(connection_string, MAX_ETH_BOARDS, "Connection string.")

// This keeps track of the component id. Required for setup and tear down.
static int comp_id;

static int boards_count = 0;
static struct rtapi_list_head board_num;
static struct rtapi_list_head ifnames;
static litexcnc_eth_t* boards[MAX_ETH_BOARDS];


// Create a dictionary structure to store card information and being able
// to retrieve the data from the list by the key (char array)
struct dict {
    struct rtapi_list_head list;
    char key[16];
    int value;
};

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
// This function has been created for a complete dictionary lookup. Not used at this moment,
// but might be handy in future to retrieve certain FPGA's by their name.
static int *dict_lookup(struct rtapi_list_head *head, const char *name) {
    struct rtapi_list_head *ptr;
    rtapi_list_for_each(ptr, head) {
        struct dict *ent = rtapi_list_entry(ptr, struct dict, list);
        if(strncmp(name, ent->key, sizeof(ent->key)) == 0) return &ent->value;
    }
    struct dict *ent = rtapi_kzalloc(sizeof(struct dict), RTAPI_GPF_KERNEL);
    strncpy(ent->key, name, sizeof(ent->key));
    rtapi_list_add(&ent->list, head);
    return &ent->value;
}
#pragma GCC diagnostic pop

static void dict_free(struct rtapi_list_head *head) {
    struct rtapi_list_head *orig_head = head;
    for(head = head->next; head != orig_head;) {
        struct rtapi_list_head *ptr = head;
        head = head->next;
        struct dict *ent = rtapi_list_entry(ptr, struct dict, list);
        rtapi_list_del(ptr);
        rtapi_kfree(ent);
    }
}


/**
 * Parameter which contains the registration of this board with LitexCNC 
 */
static litexcnc_driver_registration_t *registration;

int register_eth_driver(void) {
    registration = (litexcnc_driver_registration_t *)hal_malloc(sizeof(litexcnc_driver_registration_t));
    rtapi_snprintf(registration->name, sizeof(registration->name), "eth");
    registration->initialize_driver = *initialize_driver;
    // registration->initialize = &litexcnc_gpio_init;
    return litexcnc_register_driver(registration);
}
EXPORT_SYMBOL_GPL(register_eth_driver);


static int litexcnc_eth_read_n_bits(litexcnc_fpga_t *this, size_t address, uint8_t *data, size_t size) {
    /*
     * This function read N bits of data from the FPGA
     */
    litexcnc_eth_t *board = this->private;
    return eb_read8(
        board->connection, 
        address, 
        data, 
        size,
        board->hal.param.debug
    );
}

static int litexcnc_eth_write_n_bits(litexcnc_fpga_t *this, size_t address, uint8_t *data, size_t size) {
    /*
     * This function sends N bits of data to the FPGA.
     */
    litexcnc_eth_t *board = this->private;
    eb_write8(
        board->connection, 
        address,
        data, 
        size,
        board->hal.param.debug
    );

    // Indicate success (writing does not give back a status)
    return 0;
}

static int litexcnc_eth_read(litexcnc_fpga_t *this) {
    litexcnc_eth_t *board = this->private;
    static int r;
    
    // This is essential as the colorlight card crashes when two packets come close to each other.
	// This prevents crashes in the litex eth core. 
	// Also turn of mDNS request from linux to the colorlight card. (avahi-daemon)
	eb_wait_for_tx_buffer_empty(board->connection);

    // Read the data (etherbone.h)
    // - send request
    r = eb_send(
        board->connection,
        board->read_request_buffer,
        this->read_buffer_size);
    if (r < 0) {
        fprintf(stderr, "Could not write addresses to read to device `%s`, error code %d", this->name, r);
        return -1;
    }
    // - get response
    int count = eb_recv(
        board->connection, 
        this->read_buffer,
        this->read_buffer_size);
    // - check size is expexted size
    if (count != this->read_buffer_size) {
        fprintf(stderr, "Unexpected read length: %d, expected %zu\n", count, this->read_buffer_size);
        return -1;
    }
    
    // Successful read
    return 0;
}

static int litexcnc_eth_write(litexcnc_fpga_t *this) {
    litexcnc_eth_t *board = this->private;
    static int r;
    
    // This is essential as the colorlight card crashes when two packets come close
    // to each other. This prevents crashes in the litex eth core. 
	// Also turn of mDNS request from linux to the colorlight card. (avahi-daemon)
	eb_wait_for_tx_buffer_empty(board->connection);

    // Write the data (etberbone.h)
    r = eb_send(
        board->connection,
        this->write_buffer,
        this->write_buffer_size);
    if (r < 0) {
        fprintf(stderr, "Could not write data to device `%s`, error code %d", this->name, r);
        return -1;
    }

    // If we missed a paket earlier with timeout AND this packet arrives later, there 
    // can be a queue of packet. Test here if anoter packet is ready ( no delay) and 
    // discard that packet to avoid such a queue.
	//eb_discard_pending_packet(board->connection, this->write_buffer_size);

    return r;
}


static int connect_board(litexcnc_eth_t *board, char *connection_string) {
    
    char port_default[5] = "1234";
    char *port_ptr;

    // Check whether the connection string contains a colon (:), which indicates
    // the port number. If a port number is specified, it is split from the 
    // connection string and stored separately
    port_ptr = strchr(connection_string, ':'); // Find first ',' starting from 'p'
    if (port_ptr != NULL) {
        *port_ptr = '\0';          // Replace ':' with a null terminator
        ++port_ptr;                // Move port pointer forward
    } else {
        port_ptr = port_default;
    }

    board->connection = eb_connect(connection_string, port_ptr, 1);
    if (!board->connection) {
        rtapi_print_msg(RTAPI_MSG_ERR,"LitexCNC-eth: ERROR: failed to connect to board on '%s:%s'\n", connection_string, port_ptr);
        return -1;
    }
    rtapi_print("LitexCNC-eth: connected to board on '%s:%s'\n", connection_string, port_ptr);

    return 0;
}


static int litexcnc_init_buffers(litexcnc_fpga_t *this) {
    litexcnc_eth_t *board = this->private;

    // Initialize the buffers with headers
    size_t words;
    //  - WRITE BUFFER
    memcpy(board->fpga.write_buffer, etherbone_header, sizeof(etherbone_header));
    // - size (in WORD-count, bitshift to divide by 4)
    words =  (board->fpga.write_buffer_size - board->fpga.read_header_size) >> 2;
    board->fpga.write_buffer[10] = words;
    // - address
    uint32_t address = htobe32(board->fpga.write_base_address);
    memcpy(&board->fpga.write_buffer[12], &address, sizeof(address));

    // - REQUEST BUFFER 
    uint8_t *read_request_buffer = rtapi_kmalloc(board->fpga.read_buffer_size, RTAPI_GFP_KERNEL);
    memcpy(read_request_buffer, etherbone_header, sizeof(etherbone_header));
    // - size (in WORD-count, bitshift to divide by 4)
    words = (board->fpga.read_buffer_size - board->fpga.read_header_size) >> 2; 
    read_request_buffer[11] = words; 
    // - addresses
    uint32_t addresses[words];
    for (size_t i=0; i<words; i++) {
        addresses[i] = htobe32(board->fpga.read_base_address + (i << 2));
    }
    memcpy(&read_request_buffer[16], addresses, words * 4);
    // Store the created buffer
    board->read_request_buffer = read_request_buffer;
    
    return 0;
}


static int close_connection(litexcnc_eth_t *board) {
    eb_disconnect(&board->connection);
    return 0;
}


static int initialize_driver(char *connection_string, int comp_id) {
    size_t ret;
    boards[boards_count] = (litexcnc_eth_t *)hal_malloc(sizeof(litexcnc_eth_t));
    ret = connect_board(boards[boards_count], connection_string);
    if (ret < 0) return ret;
    // Create an FPGA instance
    boards[boards_count]->fpga.comp_id           = comp_id;
    boards[boards_count]->fpga.read_n_bits       = litexcnc_eth_read_n_bits;
    boards[boards_count]->fpga.read              = litexcnc_eth_read;
    boards[boards_count]->fpga.read_header_size  = 16;
    boards[boards_count]->fpga.write_n_bits      = litexcnc_eth_write_n_bits;
    boards[boards_count]->fpga.write             = litexcnc_eth_write;
    boards[boards_count]->fpga.write_header_size = 16;
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


int rtapi_app_main(void) {
    LITEXCNC_ERR_NO_DEVICE("ERROR: Direct usage of the module `litexcnc_eth` is not supported\n");
    LITEXCNC_ERR_NO_DEVICE("This is caused by the following loadrt-commands in your HAL-file:\n");
    LITEXCNC_ERR_NO_DEVICE("    loadrt litexcnc\n");
    LITEXCNC_ERR_NO_DEVICE("    loadrt litexcnc_eth connection_string=\"%s\"\n", connection_string[0]);
    LITEXCNC_ERR_NO_DEVICE("Please use the folllowing single command in your hal-file instead:\n");
    LITEXCNC_ERR_NO_DEVICE("    loadrt litexcnc connections=\"eth:%s\"\n", connection_string[0]);
    LITEXCNC_ERR_NO_DEVICE("For more information, see: https://github.com/Peter-van-Tol/LiteX-CNC/issues/32 \n");
    LITEXCNC_ERR_NO_DEVICE("Stopping LinuxCNC now!\n");
    return -1;
}

// Include other c-files, because LinuxCNC Makefile cannot handle loose files
#include "etherbone.c"
