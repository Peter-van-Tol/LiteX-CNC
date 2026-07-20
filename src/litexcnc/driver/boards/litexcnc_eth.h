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
#ifndef __INCLUDE_LITEXCNC_ETH_H__
#define __INCLUDE_LITEXCNC_ETH_H__

#define LITEXCNC_ETH_NAME    "litexcnc_eth"
#define LITEXCNC_ETH_VERSION "1.0.0"
#define MAX_ETH_BOARDS 4

#include "etherbone.h"
#include <litexcnc.h>

typedef struct {

    struct {
        struct {
            hal_bit_t debug;  // Indicates the communication is in debug mode
        } param;
    } hal;

    // Connection by etherbone, required for sending/receiving data.
    struct eb_connection* connection;

    // Buffer for requesting a read from the device
    uint8_t *read_request_buffer;
    size_t read_request_header_size;
    size_t read_request_buffer_size;

    // Definition of the FPGA (containing pins, steppers, PWM, ec.)
    litexcnc_fpga_t fpga;
} litexcnc_eth_t;


static int initialize_driver(char *connection_string, int comp_id);

#endif