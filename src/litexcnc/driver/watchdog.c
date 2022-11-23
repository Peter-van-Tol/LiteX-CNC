/********************************************************************
* Description:  watchdog.c
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
#include <stdio.h>
#include <json-c/json.h>

#include "rtapi.h"
#include "rtapi_app.h"

#include "litexcnc.h"
#include "watchdog.h"

int litexcnc_watchdog_init(litexcnc_t *litexcnc, json_object *config) {
    
    // Declarations
    int r = 0;
    uint32_t default_timeout_ns = 0;  // 5 ms default time out 
    struct json_object *watchdog;
    struct json_object *default_timeout_obj;\
    char name[HAL_NAME_LEN + 1];

    // Parse the contents of the config-json
    if (json_object_object_get_ex(config, "watchdog", &watchdog)) {
        // Get the default timeout as defined in the json
        if (json_object_object_get_ex(config, "default_timeout_ns", &default_timeout_obj)) {
            json_object_put(default_timeout_obj);
            default_timeout_ns = json_object_get_int(default_timeout_obj);
        }
        json_object_put(default_timeout_obj);
    }
    json_object_put(watchdog);

    // Warn user that we use the default time out
    if (!default_timeout_ns) {
        default_timeout_ns = 5 * 1000 * 1000;
        LITEXCNC_INFO_NO_DEVICE("Watchdog: No default timeout specified in json-configuration, using default value %i ns\n", default_timeout_ns);
    }

    // Allocate memory
    litexcnc->watchdog = (litexcnc_watchdog_t *)hal_malloc(sizeof(litexcnc_watchdog_t));

    // Create pins
    // - has_bitten
    rtapi_snprintf(name, sizeof(name), "%s.watchdog.enable", litexcnc->fpga->name); 
    r = hal_pin_bit_new(name, HAL_IO, &(litexcnc->watchdog->hal.pin.has_bitten), litexcnc->fpga->comp_id);
    if (r < 0) {
        LITEXCNC_ERR_NO_DEVICE("Error adding pin '%s', aborting\n", name);
        r = -EINVAL;
        return r;
    }

    // Create params
    // - time-out in nano-seconds (including setting the default value)
    rtapi_snprintf(name, sizeof(name), "%s.watchdog.timeout_ns", litexcnc->fpga->name); 
    r = hal_param_u32_new(name, HAL_RW, &(litexcnc->watchdog->hal.param.timeout_ns), litexcnc->fpga->comp_id);
    if (r < 0) {
        LITEXCNC_ERR_NO_DEVICE("Error adding param '%s', aborting\n", name);
        r = -EINVAL;
        return r;
    }
    litexcnc->watchdog->hal.param.timeout_ns = default_timeout_ns;
    // - time-out in cycles (read-only)
    rtapi_snprintf(name, sizeof(name), "%s.watchdog.timeout_cycles", litexcnc->fpga->name); 
    r = hal_param_u32_new(name, HAL_RO, &(litexcnc->watchdog->hal.param.timeout_cycles), litexcnc->fpga->comp_id);
    if (r < 0) {
        LITEXCNC_ERR_NO_DEVICE("Error adding param '%s', aborting\n", name);
        r = -EINVAL;
        return r;
    }

}

uint8_t litexcnc_watchdog_prepare_write(litexcnc_t *litexcnc, uint8_t **data, long period) {

    // Recalculate timeout_cycles only required when timeout_ns changed
    if (litexcnc->watchdog->hal.param.timeout_ns != litexcnc->watchdog->memo.timeout_ns ) {
        // Store value to detect future scale changes
        litexcnc->watchdog->memo.timeout_ns = litexcnc->watchdog->hal.param.timeout_ns;
        // Validate new value (give warning when it is to close to the period of the thread)
        if (litexcnc->watchdog->hal.param.timeout_ns < (1.5 * period)) {
            LITEXCNC_PRINT(
                "Watchdog timeout (%u ns) is dangerously short compared to litexcnc_write() period (%ld ns)\n",
                litexcnc->watchdog->hal.param.timeout_ns,
                period
            );
        }
        // Convert the time to cycles
        litexcnc->watchdog->hal.param.timeout_cycles = litexcnc->watchdog->memo.timeout_ns * ((double) litexcnc->clock_frequency / (double) 1e9) - 1;
        // Limit the value to 0x7FFFFFFF
        if (litexcnc->watchdog->hal.param.timeout_cycles > 0x7FFFFFFF) {
            litexcnc->watchdog->hal.param.timeout_cycles = 0x7FFFFFFF;
            litexcnc->watchdog->memo.timeout_ns = (litexcnc->watchdog->hal.param.timeout_cycles + 1) / ((double) litexcnc->clock_frequency / (double) 1e9);
            LITEXCNC_ERR_NO_DEVICE("Requested watchdog timeout is out of range, setting it to max: %u ns\n", litexcnc->watchdog->memo.timeout_ns);
        }
    }

    // Store the parameter on the FPGA (also set the enable bit)
    litexcnc_watchdog_data_write_t output;
    output.timeout_cycles = htobe32(litexcnc->watchdog->hal.param.timeout_cycles + 0x80000000); 
        
    // Copy the data to the output and advance the pointer  
    memcpy(*data, &output, LITEXCNC_WATCHDOG_DATA_WRITE_SIZE);
    *data += LITEXCNC_WATCHDOG_DATA_WRITE_SIZE;
}

uint8_t litexcnc_watchdog_process_read(litexcnc_t *litexcnc, uint8_t** data) {

    // Check whether the watchdog did bite
    if (*(*data)) {
        // LITEXCNC_ERR_NO_DEVICE("Watchdog has bitten");
        *(litexcnc->watchdog->hal.pin.has_bitten) = 1;
    }

    // Proceed the buffer to the next element (note: 4-byte Words!)
    (*data)+=4; 
}