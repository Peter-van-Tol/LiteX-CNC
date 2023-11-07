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
#ifndef __INCLUDE_LITEXCNC_H__
#define __INCLUDE_LITEXCNC_H__

// Solve circular dependency by forward referencing the object here
typedef struct litexcnc_struct litexcnc_t;

#include "rtapi.h"
#include <rtapi_list.h>

#include "wallclock.h"
#include "watchdog.h"

#define LITEXCNC_NAME    "litexcnc"
#define LITEXCNC_VERSION_MAJOR 1
#define LITEXCNC_VERSION_MINOR 1
#define LITEXCNC_VERSION_PATCH 0

#define MAX_RESET_RETRIES      5  

#define MAX_EXTRAS             32
#define MAX_CONNECTIONS        4

// ------------------------------------
// Definitions for printing to command line
// ------------------------------------
// - without showing which device
#define LITEXCNC_PRINT_NO_DEVICE(fmt, args...)  rtapi_print(LITEXCNC_NAME ": " fmt, ## args)
#define LITEXCNC_ERR_NO_DEVICE(fmt, args...)    rtapi_print_msg(RTAPI_MSG_ERR,  LITEXCNC_NAME ": " fmt, ## args)
#define LITEXCNC_WARN_NO_DEVICE(fmt, args...)   rtapi_print_msg(RTAPI_MSG_WARN, LITEXCNC_NAME ": " fmt, ## args)
#define LITEXCNC_INFO_NO_DEVICE(fmt, args...)   rtapi_print_msg(RTAPI_MSG_INFO, LITEXCNC_NAME ": " fmt, ## args)
#define LITEXCNC_DBG_NO_DEVICE(fmt, args...)    rtapi_print_msg(RTAPI_MSG_DBG,  LITEXCNC_NAME ": " fmt, ## args)
// - with showing which device
#define LITEXCNC_PRINT(fmt, device, args...)    rtapi_print(LITEXCNC_NAME "/%s: " fmt, device, ## args)
#define LITEXCNC_ERR(fmt, device, args...)      rtapi_print_msg(RTAPI_MSG_ERR,  LITEXCNC_NAME "/%s: " fmt, device, ## args)
#define LITEXCNC_WARN(fmt, device, args...)     rtapi_print_msg(RTAPI_MSG_WARN, LITEXCNC_NAME "/%s: " fmt, device, ## args)
#define LITEXCNC_INFO(fmt, device, args...)     rtapi_print_msg(RTAPI_MSG_INFO, LITEXCNC_NAME "/%s: " fmt, device, ## args)
#define LITEXCNC_DBG(fmt, device, args...)      rtapi_print_msg(RTAPI_MSG_DBG,  LITEXCNC_NAME "/%s: " fmt, device, ## args)

// --------------------------------
// Definitions for handling modules
// --------------------------------
#define LITEXCNC_LOAD_MODULE(name)              result = register_module(name); if (result<0) return result; 
// - Creation of the basename
#define LITEXCNC_CREATE_BASENAME(module, index)  rtapi_snprintf(base_name, sizeof(base_name), "%s.%s.%02zu", litexcnc->fpga->name, module, index);
// - Creation of a pin
#define LITEXCNC_CREATE_HAL_PIN_FN(pin_name, type, direction, parameter)     \
    rtapi_snprintf(name, sizeof(name), "%s.%s", base_name, pin_name); \
    r = hal_pin_## type ##_new(name, direction, parameter, litexcnc->fpga->comp_id); \
    if (r < 0) { LITEXCNC_ERR_NO_DEVICE("Error adding pin '%s', aborting\n", name); return r; }
#define LITEXCNC_CREATE_HAL_PIN(pin_name, type, direction, parameter) LITEXCNC_CREATE_HAL_PIN_FN(pin_name, type, direction, parameter)
// - Creation of a param
#define LITEXCNC_CREATE_HAL_PARAM_FN(pin_name, type, direction, parameter) \
    rtapi_snprintf(name, sizeof(name), "%s.%s", base_name, pin_name); \
    r = hal_param_## type ##_new(name, direction, parameter, litexcnc->fpga->comp_id); \
    if (r < 0) { LITEXCNC_ERR_NO_DEVICE("Error adding param '%s', aborting\n", name); return r; }
#define LITEXCNC_CREATE_HAL_PARAM(pin_name, type, direction, parameter) LITEXCNC_CREATE_HAL_PARAM_FN(pin_name, type, direction, parameter)           


/** 
 * This structure defines an instance of a module on a FPGA. It defines the read
 * and write functions and the data of the instance.
 */
typedef struct litexcnc_module_instance_t {
    int (*prepare_write)(void *instance, uint8_t **data, int period);
    int (*process_read)(void *instance, uint8_t **data, int period);
    int (*configure_module)(void *instance, uint8_t **data, int period);
    void *instance_data;
} litexcnc_module_instance_t;


/** 
 * This structure is used to register a module on LitexCNC. When the given 
 * module is used by a FPGA, the function inialize is called from Litex-CNC.
 */
typedef struct {
    char name[HAL_NAME_LEN+1]; /* The name of the module (for display purposes only) */
    uint32_t id;               /* The id of the module, for future use to identify boards without config file */
    size_t (*initialize)(litexcnc_module_instance_t **instance, litexcnc_t *litexcnc, uint8_t **config);
    size_t (*required_config_buffer)(void *instance);
    size_t (*required_write_buffer)(void *instance);
    size_t (*required_read_buffer)(void *instance);
    struct rtapi_list_head list;
} litexcnc_module_registration_t;



/** 
 * This structure is used to register a board on LitexCNC. When the  
 * given board is used by a FPGA, the function inialize is called
 * from Litex-CNC.
 */
typedef struct {
    char name[HAL_NAME_LEN+1]; /* The name of the module (for display purposes only) */
    int (*initialize_driver)(char *connection_string, int comp_id);
    // size_t (*connect)(litexcnc_board_instance_t **instance, litexcnc_t *litexcnc, uint8_t **config);
    struct rtapi_list_head list;
} litexcnc_driver_registration_t;


typedef struct litexcnc_fpga_struct litexcnc_fpga_t;
struct litexcnc_fpga_struct {
    char name[HAL_NAME_LEN+1];
    int comp_id;
    uint32_t version;

    // Functions to read and write data from the board
    int (*read_n_bits)(litexcnc_fpga_t *self, size_t address, uint8_t *data, size_t size);
    int (*write_n_bits)(litexcnc_fpga_t *self, size_t address, uint8_t *data, size_t size);
    // - on success these two return TRUE (not zero)
    // - on failure they return FALSE (0) and set *self->io_error (below) to TRUE
    int (*read)(litexcnc_fpga_t *self);
    int (*write)(litexcnc_fpga_t *self);
    int (*terminate)(litexcnc_fpga_t *self);
    hal_bit_t *io_error;

    // Functions which will be called during various stages
    int (*post_register)(litexcnc_fpga_t *self);

    // Addresses and buffers for reading and writing data
    // - base addresses
    size_t init_base_address;
    size_t reset_base_address;
    size_t config_base_address;
    size_t write_base_address;
    size_t read_base_address;
    // - buffers
    size_t config_buffer_size;
    uint8_t *write_buffer;
    size_t write_header_size;
    size_t write_buffer_size;
    uint8_t *read_buffer;
    size_t read_header_size;
    size_t read_buffer_size;
    
    // For the low-level driver to hang their struct on
    void *private;  
};

struct litexcnc_struct {
    litexcnc_fpga_t *fpga;
    uint32_t clock_frequency;
    float clock_frequency_recip;
    
    litexcnc_module_instance_t **modules;
    size_t num_modules;

    // The fingerprint of the FPGA and driver
    uint32_t config_fingerprint;
    uint32_t driver_version;

    // Booleans to indicate whether the loop is run for the first time
    bool write_loop_has_run;
    bool read_loop_has_run;

    // Default litexcnc modules
    litexcnc_watchdog_t *watchdog;
    litexcnc_wallclock_t *wallclock;

    struct rtapi_list_head list;
};


// Defines the data-packages for retrieving the header information and
// sending and retrieving the reset signal
// - read
#pragma pack(push,4)
typedef struct {
    // Input pins
    uint32_t magic;
    uint8_t reserved1;
    uint8_t version_major;
    uint8_t version_minor;
    uint8_t version_patch;
    uint32_t clock_frequency;
    uint8_t reserved2;
    uint8_t num_modules;
    uint16_t module_data_size;
    char name[16 + 1];
} litexcnc_header_data_read_t;
#pragma pack(pop)
//sizeof(litexcnc_header_data_read_t)
#define LITEXCNC_HEADER_DATA_READ_SIZE 32 

// - write (reset)
#pragma pack(push,4)
typedef struct {
    // Input pins
    uint32_t reset;
} litexcnc_reset_header_t;
#pragma pack(pop)
#define LITEXCNC_RESET_HEADER_SIZE sizeof(litexcnc_reset_header_t)

// - configuration of the FPGA
#pragma pack(push,4)
typedef struct {
    // Input pins
    // ...
} litexcnc_config_header_t;
#pragma pack(pop)
#define LITEXCNC_CONFIG_HEADER_SIZE sizeof(litexcnc_config_header_t) 


int litexcnc_register(litexcnc_fpga_t *fpga);
void litexcnc_unregister(litexcnc_fpga_t *fpga);
size_t litexcnc_register_module(litexcnc_module_registration_t *module);
size_t litexcnc_register_driver(litexcnc_driver_registration_t *driver);

#endif
