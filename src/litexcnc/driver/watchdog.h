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

/** Structure of an output pin */
typedef struct {
    /** Structure defining the HAL pin and params*/
    struct {
        /** Structure defining the HAL pins */
        struct {
            hal_bit_t *fault_out;  /** HIGH when the EStop is active */
            hal_bit_t *ok_out;     /** HIGH when the EStop is not active */
        } pin;
        /** Structure defining the HAL params */
        struct {
        } param;
    } hal;
} litexcnc_estop_pin_t;

// Defines the Watchdog. In contrast to the other components, the watchdog is
// a singleton: exactly one exist on each FPGA-card
typedef struct {
    size_t num_estops;
    litexcnc_estop_pin_t *estop_pins;    /** Structure containing the data on the input pins */

    struct {

        struct {
            hal_bit_t *fpga_timeout;   /* Pin which is set when the watchdog has timed out */
            hal_bit_t *comm_timeout;   /* Pin which is set when the communication has timed out */
            hal_bit_t *reset;          /* Pin to indicate the watchdog should be reset. Acts on rising edge.*/
            hal_bit_t *ok_in;          /* Pin to indicate previous latches (estop_latch) are working fine.*/
            hal_bit_t *fault_in;       /* Pin to indicate previous latches (estop_latch) have an error.*/
            hal_bit_t *ok_out;         /* Pin to indicates the system up to and including this latch is OK.*/
            hal_bit_t *fault_out;      /* Pin to indicates the system up to and including this latch is OK.*/
        } pin;

        struct {
            hal_u32_t  timeout_ns;     /* Timeout in nano-seconds */
            hal_u32_t  timeout_cycles; /* Timeout in clock cycles (not expoerted) */
        } param;

    } hal;

    // This struct holds all old values (memoization) 
    struct {
        uint32_t timeout_ns;
        hal_bit_t reset;
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
int litexcnc_watchdog_init(litexcnc_t *litexcnc, uint32_t num_estops);
uint8_t litexcnc_watchdog_config(litexcnc_t *litexcnc, uint8_t **data, long period);
uint8_t litexcnc_watchdog_prepare_write(litexcnc_t *litexcnc, uint8_t **data, long period);
uint8_t litexcnc_watchdog_process_read(litexcnc_t *litexcnc, uint8_t **data);
uint8_t litexcnc_watchdog_process_read_error(litexcnc_t *litexcnc, long period);

#endif
