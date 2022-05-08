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
#define LITEXCNC_ETH_VERSION "0.02"
#define MAX_ETH_BOARDS 4
#define MAX_RESET_RETRIES 5

#include "etherbone.h"

typedef struct {
    // Connection by etherbone, required for sending/receiving data.
    struct eb_connection* connection;
    // Definition of the FPGA (containing pins, steppers, PWM, ec.)
    litexcnc_fpga_t fpga;
} litexcnc_eth_t;

#endif