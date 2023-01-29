/********************************************************************
* Description:  watchdog.h
*               A Litex-CNC component that keeps track on the number 
*               of cycles which have occurred since the start of the
*               device.
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
#ifndef __INCLUDE_LITEXCNC_WALLCLOCK_H__
#define __INCLUDE_LITEXCNC_WALLCLOCK_H__

// Defines the Watchdog. In contrast to the other components, the watchdog is
// a singleton: exactly one exist on each FPGA-card
typedef struct {
    struct {

        struct {
            hal_u32_t *wallclock_ticks_msb;  /* The most significant 4 bytes of the wall clock */
            hal_u32_t *wallclock_ticks_lsb;  /* The least significant 4 bytes of the wall clock */
        } pin;

        struct {
            // Empty
        } param;

    } hal;

    // This struct holds all old values (memoization) 
    struct {
        uint64_t wallclock_ticks; /* Combined MSB + LSB, should be in sync with the hal pins */
    } memo;

} litexcnc_wallclock_t;

// - write 
// NOTE: The wall clock is a read-only 
#define LITEXCNC_WALLCLOCK_DATA_WRITE_SIZE 0
// - read
#pragma pack(push,4)
typedef struct {
    // Input pins
    uint64_t count;
} litexcnc_wallclock_data_read_t;
#pragma pack(pop)
#define LITEXCNC_WALLCLOCK_DATA_READ_SIZE sizeof(litexcnc_wallclock_data_read_t)

// Functions for creating, reading and writing wall-clock pins
int litexcnc_wallclock_init(litexcnc_t *litexcnc);
uint8_t litexcnc_wallclock_config(litexcnc_t *litexcnc, uint8_t **data, long period);
uint8_t litexcnc_wallclock_prepare_write(litexcnc_t *litexcnc, uint8_t **data);
uint8_t litexcnc_wallclock_process_read(litexcnc_t *litexcnc, uint8_t** data);

#endif