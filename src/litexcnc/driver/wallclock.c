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

    // Allocate memory
    litexcnc->wallclock = (litexcnc_wallclock_t *)hal_malloc(sizeof(litexcnc_wallclock_t));
    
    // Create pins
    // - wallclock_ticks_msb
    r = hal_pin_u32_newf(HAL_OUT, &(litexcnc->wallclock->hal.pin.wallclock_ticks_msb), litexcnc->fpga->comp_id, "%s.wallclock.ticks_msb", litexcnc->fpga->name);
    if (r < 0) {
        LITEXCNC_ERR_NO_DEVICE("Error adding pin '%s.wallclock.ticks_msb', aborting\n", litexcnc->fpga->name);
        r = -EINVAL;
        return r;
    }
    // - wallclock_ticks_lsb
    r = hal_pin_u32_newf(HAL_IO, &(litexcnc->wallclock->hal.pin.wallclock_ticks_lsb), litexcnc->fpga->comp_id, "%s.wallclock.ticks_lsb", litexcnc->fpga->name); 
    if (r < 0) {
        LITEXCNC_ERR_NO_DEVICE("Error adding pin '%s.wallclock.ticks_lsb', aborting\n", litexcnc->fpga->name);
        r = -EINVAL;
        return r;
    }

    return r;

}

uint8_t litexcnc_wallclock_prepare_write(litexcnc_t *litexcnc, uint8_t **data) {
    // This function is deliberately empty, as the wall clock is not written.
}

uint8_t litexcnc_wallclock_process_read(litexcnc_t *litexcnc, uint8_t** data) {

    // Get the full value (fool-proof way ;) )
    uint64_t ticks;
    memcpy(&ticks , *data, sizeof ticks);
    litexcnc->wallclock->memo.wallclock_ticks = be64toh(ticks);
    // Write the MSB value to the HAL pins
    uint32_t msb;
    memcpy(&msb, *data, sizeof msb);
    *(litexcnc->wallclock->hal.pin.wallclock_ticks_msb) = be32toh(msb);
    (*data)+=4;
    // Write the MSB value to the HAL pins
    uint32_t lsb;
    memcpy(&lsb, *data, sizeof lsb);
    *(litexcnc->wallclock->hal.pin.wallclock_ticks_lsb) = be32toh(lsb);
    (*data)+=4;
}


