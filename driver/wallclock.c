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
#include <stdio.h>
#include <json-c/json.h>

#include "rtapi.h"
#include "rtapi_app.h"

#include "litexcnc.h"
#include "wallclock.h"


int litexcnc_wallclock_init(litexcnc_t *litexcnc, json_object *config) {
    
    // Declarations
    int r = 0;

    // Create pins
    // - wallclock_ticks_msb
    rtapi_snprintf(name, sizeof(name), "%s.wallclock.ticks_msb", litexcnc->fpga->name); 
    r = hal_pin_bit_new(name, HAL_IO, &(litexcnc->wallclock->hal.pin.wallclock_ticks_msb), litexcnc->fpga->comp_id);
    if (r < 0) {
        LITEXCNC_ERR_NO_DEVICE("Error adding pin '%s', aborting\n", name);
        r = -EINVAL;
        return r;
    }
    // - wallclock_ticks_lsb
    rtapi_snprintf(name, sizeof(name), "%s.wallclock.ticks_lsb", litexcnc->fpga->name); 
    r = hal_pin_bit_new(name, HAL_IO, &(litexcnc->wallclock->hal.pin.wallclock_ticks_lsb), litexcnc->fpga->comp_id);
    if (r < 0) {
        LITEXCNC_ERR_NO_DEVICE("Error adding pin '%s', aborting\n", name);
        r = -EINVAL;
        return r;
    }

}

uint8_t litexcnc_wallclock_prepare_write(litexcnc_t *litexcnc, uint8_t **data) {
    // This function is deliberately empty, as the wall clock is not written.
}

uint8_t litexcnc_wallclock_process_read(litexcnc_t *litexcnc, uint8_t** data) {
    // Store the value in the memo
    litexcnc->wallclock->hal.memo.wallclock_ticks = htobe64(*(uint64_t*)data);
    // Write the values to the HAL pins
    *(litexcnc->wallclock->hal.pin.wallclock_ticks_msb) = htobe32(*(uint32_t*)data);
    (*data)+=4; 
    *(litexcnc->wallclock->hal.pin.wallclock_ticks_lsb) = htobe32(*(uint32_t*)data);
    (*data)+=4; 
}


