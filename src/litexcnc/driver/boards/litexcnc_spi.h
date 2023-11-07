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

#define LITEXCNC_SPIDEV_NAME    "litexcnc_spidev"
#define LITEXCNC_SPIDEV_VERSION "1.0.1"
#define MAX_SPI_BOARDS 4

#include <litexcnc.h>

typedef struct {

    struct {
        struct {
            hal_bit_t debug;  // Indicates the communication is in debug mode
        } param;
    } hal;

    // Connection with SPI (in reality this is a file-descriptor)
    int connection;

    // Definition of the FPGA (containing pins, steppers, PWM, ec.)
    litexcnc_fpga_t fpga;

} litexcnc_spi_t;


static int initialize_driver(char *connection_string, int comp_id);

#endif