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

#include "<litexcnc.h>"
#include "litexcnc_debug.h"


static char *config_file[MAX_DEBUG_BOARDS];
RTAPI_MP_ARRAY_STRING(config_file, MAX_DEBUG_BOARDS, "Path to the config-file for the given board.")

// This keeps track of the component id. Required for setup and tear down.
static int comp_id;

static int boards_count = 0;
static struct rtapi_list_head board_num;
static struct rtapi_list_head ifnames;
static litexcnc_debug_t boards[MAX_DEBUG_BOARDS];


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


static int litexcnc_debug_read(litexcnc_fpga_t *this) {
    litexcnc_debug_t *board = this->private;
    
    // TODO: no support for read in debug mode
}


static int litexcnc_debug_write(litexcnc_fpga_t *this) {
    litexcnc_debug_t *board = this->private;
    for (size_t i=0; i<this->write_buffer_size; i+=4) {
        LITEXCNC_PRINT_NO_DEVICE("%02X %02X %02X %02X\n",
            (unsigned char)this->write_buffer[i+0],
            (unsigned char)this->write_buffer[i+1],
            (unsigned char)this->write_buffer[i+2],
            (unsigned char)this->write_buffer[i+3]);
    }
}


static int init_board(litexcnc_debug_t *board, const char *config_file) {
  
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

    // Free up memory
    json_object_put(config);

    // Connect the functions for reading and writing the data to the device
    rtapi_snprintf(board->fpga.name, sizeof(board->fpga.name), "test.0");
    //rtapi_snprintf(board->fpga.name, sizeof(board->fpga.name), "hm2_%.*s.%d", (int)strlen(llio_name), llio_name, llio_idx(llio_name));
    board->fpga.comp_id = comp_id;
    board->fpga.read    = litexcnc_debug_read;
    board->fpga.write   = litexcnc_debug_write;
    board->fpga.private = board;

    // Register the board with the main function
    size_t ret = litexcnc_register(&board->fpga, config_file);
    if (ret != 0) {
        rtapi_print("board fails LitexCNC registration\n");
        return ret;
    }
    boards_count++;

    return 0;

fail_freememory:
	json_object_put(config);
fail_without_disconnect:
    return -1;
}


static int close_board(litexcnc_debug_t *board) {
    return 0;
}


int rtapi_app_main(void) {
    RTAPI_INIT_LIST_HEAD(&ifnames);
    RTAPI_INIT_LIST_HEAD(&board_num);

    int ret, i;

    LITEXCNC_PRINT_NO_DEVICE("Loading litexCNC debug driver version " LITEXCNC_DEBUG_VERSION "\n");

    // STEP 1: Initialize component
    ret = hal_init(LITEXCNC_DEBUG_NAME);
    if (ret < 0)
        return ret;
    comp_id = ret;

    // STEP 2: Initialize the board(s)
    for(i = 0, ret = 0; ret == 0 && i<MAX_DEBUG_BOARDS && config_file[i] && *config_file[i]; i++) {
        ret = init_board(&boards[i], config_file[i]);
        if(ret < 0) return ret;
    }

    // Report the board as ready
    hal_ready(comp_id);
    return 0;

error:
    // Close all the boards
    for(i = 0; i<MAX_DEBUG_BOARDS && config_file[i] && config_file[i][0]; i++)
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
    for(int i = 0; i<MAX_DEBUG_BOARDS && config_file[i] && config_file[i][0]; i++)
        close_board(&boards[i]);
    // Free up the used memory
    dict_free(&board_num);
    dict_free(&ifnames);
    // Report the board as unloaded
    hal_exit(comp_id);
    LITEXCNC_PRINT_NO_DEVICE("LitexCNC debug driver unloaded \n");
}
