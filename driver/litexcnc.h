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

#include "gpio.h"
#include "pwm.h"
#include "watchdog.h"

#define LITEXCNC_NAME    "litexcnc"
#define LITEXCNC_VERSION "0.15"


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

// ------------------------------------
// Definitions of data size
// ------------------------------------
// Basically these are the summations of all the data sizes from the
// sub-modules
#define LITEXCNC_BOARD_DATA_WRITE_SIZE(litexcnc) LITEXCNC_WATCHDOG_DATA_WRITE_SIZE + LITEXCNC_BOARD_GPIO_DATA_WRITE_SIZE(litexcnc) + LITEXCNC_BOARD_PWM_DATA_WRITE_SIZE(litexcnc)
#define LITEXCNC_BOARD_DATA_READ_SIZE(litexcnc) LITEXCNC_WATCHDOG_READ_WRITE_SIZE + LITEXCNC_BOARD_GPIO_DATA_READ_SIZE(litexcnc) + LITEXCNC_BOARD_PWM_DATA_READ_SIZE(litexcnc)

typedef struct litexcnc_fpga_struct litexcnc_fpga_t;
struct litexcnc_fpga_struct {
    char name[HAL_NAME_LEN+1];
    int comp_id;

    // Functions to read and write data from the board
    // - on success these two return TRUE (not zero)
    // - on failure they return FALSE (0) and set *self->io_error (below) to TRUE
    int (*read)(litexcnc_fpga_t *self);
    int (*write)(litexcnc_fpga_t *self);
    hal_bit_t *io_error;

    // Buffers for reading and writing data
    uint8_t *write_buffer;
    size_t write_buffer_size;
    uint8_t *read_buffer;
    size_t read_buffer_size;
    
    // For the low-level driver to hang their struct on
    void *private;  
};

struct litexcnc_struct {
    litexcnc_fpga_t *fpga;
    uint32_t clock_frequency;

    struct {
        size_t num_gpio_inputs;
        size_t num_gpio_outputs;
        size_t num_pwm_instances;
    } config;

    // the litexcnc "Components"
    litexcnc_watchdog_t *watchdog;
    litexcnc_gpio_t gpio;
    litexcnc_pwm_t pwm;


    struct rtapi_list_head list;
};


int litexcnc_register(litexcnc_fpga_t *fpga, const char *config_file);
void litexcnc_unregister(litexcnc_fpga_t *fpga);

#endif
