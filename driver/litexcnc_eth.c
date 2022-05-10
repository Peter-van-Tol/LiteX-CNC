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
#include <json-c/json.h>

#include <rtapi_slab.h>
#include <rtapi_list.h>

#include "hal.h"
#include "rtapi.h"
#include "rtapi_app.h"
#include "rtapi_string.h"


#include "etherbone.h"
#include "litexcnc.h"
#include "litexcnc_eth.h"


static char *config_file[MAX_ETH_BOARDS];
RTAPI_MP_ARRAY_STRING(config_file, MAX_ETH_BOARDS, "Path to the config-file for the given board.")

// This keeps track of the component id. Required for setup and tear down.
static int comp_id;

static int boards_count = 0;
static struct rtapi_list_head board_num;
static struct rtapi_list_head ifnames;
static litexcnc_eth_t boards[MAX_ETH_BOARDS];


// Create a dictionary structure to store card information and being able
// to retrieve the data from the list by the key (char array)
struct dict {
    struct rtapi_list_head list;
    char key[16];
    int value;
};

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

static int litexcnc_eth_verify_config(litexcnc_fpga_t *this) {
    /*
     * This function reads the magic code (should be equal to 0x18052022) and if the
     * received magic code is valied, it stores the fingerprint on the FPGA in the
     * datastruct on the computer. The comparison between the used config for the driver
     * stored fingerprint is done by the LitexCNC driver
     */
    litexcnc_eth_t *board = this->private;

    // Create a buffer to contain both the magic number and the config fingerprint. Both
    // parameters are stored in as 32-bit unsigned integers
    uint8_t *read_buffer = rtapi_kmalloc(LITEXCNC_HEADER_DATA_READ_SIZE, RTAPI_GFP_KERNEL);

    // Read the magic and fingerprint. These are the first registers on the card
    int r = eb_read8(board->connection, 0x0, read_buffer, LITEXCNC_HEADER_DATA_READ_SIZE);
    if (r < 0){
        return r;
    }

    // Convert received data to header
    litexcnc_header_data_read_t header;
    memcpy(&header, read_buffer, sizeof header);

    // Check magic
    if (be32toh(header.magic) != 0x18052022) {
        LITEXCNC_ERR_NO_DEVICE("Invalid magic received '%08X'\n", be32toh(header.magic));
        return -1;
    }

    // Store version and fingerprint
    this->version = be32toh(header.version);
    this->fingerprint = be32toh(header.fingerprint);

    // Succesfull finish
    return 0;
}

static int litexcnc_eth_reset(litexcnc_fpga_t *this) {
    /*
     * This function resets the card to its initial state. Because this resetting is
     * very important to prevent uncommanded moves, this function will write the reset
     * code to the card, read back the value to assure it is set and then set the card
     * to working mode. Whenever a disruption of this sequence occurs, this function
     * will return an error and the component will fail to load.
     *
     * This function will retry to reset the card a maximum of MAX_RESET_RETRIES when
     * the reset fails. Failure of the reset can happen when for example a disruption
     * in communication occurs.
     */
    litexcnc_eth_t *board = this->private;

    // Create a buffer to contain both the magic number and the config fingerprint. Both
    // parameters are stored in as 32-bit unsigned integers
    uint8_t *buffer = rtapi_kmalloc(LITEXCNC_RESET_HEADER_SIZE, RTAPI_GFP_KERNEL);

    // Initialize a variables for resetting the card and the current status
    size_t i;
    uint32_t reset_flag;
    uint32_t reset_status;

    // Write reset bit to the card.
    i = 1;
    reset_flag = htobe32(0x01);
    while (reset_flag != reset_status) {
        // Check whether we didn't try too many times
        if (i > MAX_RESET_RETRIES) {
            LITEXCNC_ERR_NO_DEVICE("Reset of the card failed after %d times\n", MAX_RESET_RETRIES);
            return -1;
        }
        // Write the data to the card
        memcpy(buffer, &reset_flag, LITEXCNC_RESET_HEADER_SIZE);
        eb_write8(
            board->connection, 
            LITEXCNC_HEADER_DATA_READ_SIZE, 
            buffer, 
            LITEXCNC_RESET_HEADER_SIZE);
        // Wait for a bit before requesting the data
        usecSleep(10);
        // Read the data back
        eb_read8(
            board->connection, 
            LITEXCNC_HEADER_DATA_READ_SIZE, 
            buffer, 
            LITEXCNC_RESET_HEADER_SIZE);
        memcpy(&reset_status, buffer, LITEXCNC_RESET_HEADER_SIZE);
        // Proceed counter
        i++;
    }

    // Finish reset procedure
    i = 1;
    reset_flag = htobe32(0);
    while (reset_flag != reset_status) {
        // Check whether we didn't try too many times
        if (i > MAX_RESET_RETRIES) {
            LITEXCNC_ERR_NO_DEVICE("FPGA did not respond after reset for %d times\n", MAX_RESET_RETRIES);
            return -1;
        }
        // Write the data to the card
        memcpy(buffer, &reset_flag, LITEXCNC_RESET_HEADER_SIZE);
        eb_write8(
            board->connection, 
            LITEXCNC_HEADER_DATA_READ_SIZE, 
            buffer, 
            LITEXCNC_RESET_HEADER_SIZE);
        // Wait for a bit before requesting the data
        usecSleep(10);
        // Read the data back
        eb_read8(
            board->connection, 
            LITEXCNC_HEADER_DATA_READ_SIZE, 
            buffer, 
            LITEXCNC_RESET_HEADER_SIZE);
        memcpy(&reset_status, buffer, LITEXCNC_RESET_HEADER_SIZE);
        // Proceed counter
        i++;
    }

    // Card has been reset
    return 0;

}

static int litexcnc_eth_read(litexcnc_fpga_t *this) {
    litexcnc_eth_t *board = this->private;
    
    // This is essential as the colorlight card crashes when two packets come close to each other.
	// This prevents crashes in the litex eth core. 
	// Also turn of mDNS request from linux to the colorlight card. (avahi-daemon)
	eb_wait_for_tx_buffer_empty(board->connection);

    // Read the data (etherbone.h), the address is based now on a fixed number in order
    // to read the GPIO out, as the board is not suitable for input yet.
    eb_read8(
        board->connection, 
        this->write_buffer_size + LITEXCNC_HEADER_DATA_READ_SIZE + LITEXCNC_RESET_HEADER_SIZE, 
        this->read_buffer, 
        this->read_buffer_size);
}


static int litexcnc_eth_write(litexcnc_fpga_t *this) {
    litexcnc_eth_t *board = this->private;
    
    // This is essential as the colorlight card crashes when two packets come close
    // to each other. This prevents crashes in the litex eth core. 
	// Also turn of mDNS request from linux to the colorlight card. (avahi-daemon)
	eb_wait_for_tx_buffer_empty(board->connection);

    // Write the data (etberbone.h)
    eb_write8(
        board->connection, 
        LITEXCNC_HEADER_DATA_READ_SIZE + LITEXCNC_RESET_HEADER_SIZE, 
        this->write_buffer, 
        this->write_buffer_size);

    // If we missed a paket earlier with timeout AND this packet arrives later, there 
    // can be a queue of packet. Test here if anoter packet is ready ( no delay) and 
    // discard that packet to avoid such a queue.
	//eb_discard_pending_packet(board->connection, this->write_buffer_size);
}


static int init_board(litexcnc_eth_t *board, const char *config_file) {
  
    // Skip leading spaces from the config paths
    while( *config_file == ' ' ) {
        config_file++;
    }

    // Open the json-file for the configuration
    struct json_object *config = json_object_from_file(config_file);
    if (!config) {
        LITEXCNC_ERR_NO_DEVICE("Could not load configuration file '%s'\n", config_file);
        goto fail_without_disconnect;
    }

    // Create a connection with the board
    struct json_object *etherbone;
    if (!json_object_object_get_ex(config, "etherbone", &etherbone)) {
        LITEXCNC_ERR_NO_DEVICE("Missing required JSON key: '%s'\n", "etherbone");
        json_object_put(etherbone);
        goto fail_freememory;
    }
    struct json_object *ip_address;
    if (!json_object_object_get_ex(etherbone, "ip_address", &ip_address)) {
        LITEXCNC_ERR_NO_DEVICE("Missing required JSON key: '%s'\n", "ip_address");
        json_object_put(ip_address);
        json_object_put(etherbone);
        goto fail_freememory;
    }
    LITEXCNC_PRINT_NO_DEVICE("Connecting to board at address: %s:1234 \n", json_object_get_string(ip_address));
    board->connection = eb_connect(json_object_get_string(ip_address), "1234", 1);
    if (!board->connection) {
        rtapi_print_msg(RTAPI_MSG_ERR,"colorcnc: ERROR: failed to connect to board on ip-address '%s'\n", ip_address);
        goto fail_disconnect;
    }
    json_object_put(ip_address);
    json_object_put(etherbone);

    // Free up memory
    json_object_put(config);

    // Connect the functions for reading and writing the data to the device
    board->fpga.comp_id       = comp_id;
    board->fpga.verify_config = litexcnc_eth_verify_config;
    board->fpga.reset         = litexcnc_eth_reset;
    board->fpga.read          = litexcnc_eth_read;
    board->fpga.write         = litexcnc_eth_write;
    board->fpga.private       = board;

    // Register the board with the main function
    size_t ret = litexcnc_register(&board->fpga, config_file);
    if (ret != 0) {
        rtapi_print("board fails LitexCNC registration\n");
        return ret;
    }
    boards_count++;

    return 0;

fail_disconnect:
	eb_disconnect(&board->connection);
fail_freememory:
	json_object_put(config);
fail_without_disconnect:
    return -1;
}


static int close_board(litexcnc_eth_t *board) {
    eb_disconnect(&board->connection);
    return 0;
}


int rtapi_app_main(void) {
    RTAPI_INIT_LIST_HEAD(&ifnames);
    RTAPI_INIT_LIST_HEAD(&board_num);

    int ret, i;

    LITEXCNC_PRINT_NO_DEVICE("loading litexCNC etherbone driver version " LITEXCNC_ETH_VERSION "\n");

    // STEP 1: Initialize component
    ret = hal_init(LITEXCNC_ETH_NAME);
    if (ret < 0)
        return ret;
    comp_id = ret;

    // STEP 2: Initialize the board(s)
    for(i = 0, ret = 0; ret == 0 && i<MAX_ETH_BOARDS && config_file[i] && *config_file[i]; i++) {
        ret = init_board(&boards[i], config_file[i]);
        if(ret < 0) return ret;
    }

    // Report the board as ready
    hal_ready(comp_id);
    return 0;

error:
    // Close all the boards
    for(i = 0; i<MAX_ETH_BOARDS && config_file[i] && config_file[i][0]; i++)
        close_board(&boards[i]);
    // Free up the used memory
    dict_free(&board_num);
    dict_free(&ifnames);
    // Report the board as unloaded
    hal_exit(comp_id);
    return ret;
}


void rtapi_app_exit(void) {
    // Declaration of variables
    int i;
    // Close all the boards
    for(int i = 0; i<MAX_ETH_BOARDS && config_file[i] && config_file[i][0]; i++)
        close_board(&boards[i]);
    // Free up the used memory
    dict_free(&board_num);
    dict_free(&ifnames);
    // Report the board as unloaded
    hal_exit(comp_id);
    LITEXCNC_PRINT_NO_DEVICE("LitexCNC etherbone driver unloaded \n");
}


// Include other c-files, because LinuxCNC Makefile cannot handle loose files
#include "etherbone.c"
