/********************************************************************
* Description:  watchdog.h
*               A Litex-CNC component that generates will bite when
*               it doesn't get petted on time.
*
* Author: Peter van Tol <petertgvantol AT gmail DOT com>
* License: GPL Version 2
*    
* Copyright (c) 2022 All rights reserved.
*
********************************************************************/

/** This program is free software; you can redistribute it and/or
    modify it under the terms of version 2 of the GNU General
    Public License as published by the Free Software Foundation.
    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

    THE AUTHORS OF THIS LIBRARY ACCEPT ABSOLUTELY NO LIABILITY FOR
    ANY HARM OR LOSS RESULTING FROM ITS USE.  IT IS _EXTREMELY_ UNWISE
    TO RELY ON SOFTWARE ALONE FOR SAFETY.  Any machinery capable of
    harming persons must have provisions for completely removing power
    from all motors, etc, before persons enter any danger area.  All
    machinery must be designed to comply with local and national safety
    codes, and the authors of this software can not, and do not, take
    any responsibility for such compliance.

    This code was written as part of the LiteX-CNC project.
*/

#ifndef __INCLUDE_LITEXCNC_WATCHDOG_H__
#define __INCLUDE_LITEXCNC_WATCHDOG_H__

// Defines the Watchdog. In contrast to the other components, the watchdog is
// a singleton: exactly one exist on each FPGA-card
typedef struct {
    struct {

        struct {
            hal_bit_t *has_bitten;     /* Pin which is set when the watchdog has timed out */
        } pin;

        struct {
            hal_u32_t  timeout_ns;     /* Timeout in nano-seconds */
            hal_u32_t  timeout_cycles; /* Timeout in clock cycles (not expoerted) */
        } param;

    } hal;

    // This struct holds all old values (memoization) 
    struct {
        uint32_t timeout_ns;
    } memo;

} litexcnc_watchdog_t;

// Defines the data-packages for sending and receiving of the status of the 
// watchdog. The order of this package MUST coincide with the order in the 
// MMIO definition.
// - write
#pragma pack(push,4)
typedef struct {
    // Input pins
    uint32_t timeout_cycles;
} litexcnc_watchdog_data_write_t;
#pragma pack(pop)
#define LITEXCNC_WATCHDOG_DATA_WRITE_SIZE sizeof(litexcnc_watchdog_data_write_t)
// - read
#pragma pack(push,4)
typedef struct {
    // Input pins
    uint32_t has_bitten; // Flag, but all data will be send with a 4-byte width
} litexcnc_watchdog_data_read_t;
#pragma pack(pop)
#define LITEXCNC_WATCHDOG_DATA_READ_SIZE sizeof(litexcnc_watchdog_data_read_t)


// Functions for creating, reading and writing Watchdog pins
int litexcnc_watchdog_init(litexcnc_t *litexcnc);
uint8_t litexcnc_watchdog_config(litexcnc_t *litexcnc, uint8_t **data, long period);
uint8_t litexcnc_watchdog_prepare_write(litexcnc_t *litexcnc, uint8_t **data, long period);
uint8_t litexcnc_watchdog_process_read(litexcnc_t *litexcnc, uint8_t** data);

#endif