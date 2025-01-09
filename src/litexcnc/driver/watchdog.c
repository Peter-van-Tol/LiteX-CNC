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
#include <inttypes.h>
#include <stdio.h>

#include "rtapi.h"
#include "rtapi_app.h"
#include "litexcnc.h"

#include "watchdog.h"

int litexcnc_watchdog_init(litexcnc_t *litexcnc, uint32_t num_estops) {
    
    // Declarations
    int r = 0;
    char base_name[HAL_NAME_LEN + 1];   // i.e. <board_name>.<board_index>.gpio.<gpio_name>
    char name[HAL_NAME_LEN + 1];        // i.e. <base_name>.<pin_name>

    // Allocate memory
    litexcnc->watchdog = (litexcnc_watchdog_t *)hal_malloc(sizeof(litexcnc_watchdog_t));

    // Create pins and params
    LITEXCNC_CREATE_BASENAME_NO_INDEX("watchdog");
    // - pins
    LITEXCNC_CREATE_HAL_PIN("fpga-timeout", bit, HAL_OUT, &(litexcnc->watchdog->hal.pin.fpga_timeout));
    LITEXCNC_CREATE_HAL_PIN("comm-timeout", bit, HAL_OUT, &(litexcnc->watchdog->hal.pin.comm_timeout));
    LITEXCNC_CREATE_HAL_PIN("reset", bit, HAL_IN, &(litexcnc->watchdog->hal.pin.reset));
    LITEXCNC_CREATE_HAL_PIN("ok-in", bit, HAL_IN, &(litexcnc->watchdog->hal.pin.ok_in));
    LITEXCNC_CREATE_HAL_PIN("fault-in", bit, HAL_IN, &(litexcnc->watchdog->hal.pin.fault_in));
    LITEXCNC_CREATE_HAL_PIN("ok-out", bit, HAL_OUT, &(litexcnc->watchdog->hal.pin.ok_out));
    LITEXCNC_CREATE_HAL_PIN("fault-out", bit, HAL_OUT, &(litexcnc->watchdog->hal.pin.fault_out));
    // - params
    LITEXCNC_CREATE_HAL_PARAM("timeout-ns", u32, HAL_RW, &(litexcnc->watchdog->hal.param.timeout_ns));
    LITEXCNC_CREATE_HAL_PARAM("timeout-cycles", u32, HAL_RO, &(litexcnc->watchdog->hal.param.timeout_cycles));

    // Set default value
    *(litexcnc->watchdog->hal.pin.fault_in) = false;
    *(litexcnc->watchdog->hal.pin.ok_in) = true;
    *(litexcnc->watchdog->hal.pin.fault_out) = true;
    *(litexcnc->watchdog->hal.pin.ok_out) = false;

    // Create the EStops
    litexcnc->watchdog->num_estops = num_estops;
    if (num_estops) {
        litexcnc->watchdog->estop_pins = (litexcnc_estop_pin_t *)hal_malloc(sizeof(litexcnc_estop_pin_t)*num_estops);
        for (size_t i=0; i<num_estops; i++) {
            LITEXCNC_CREATE_BASENAME("watchdog.estop", i);
            LITEXCNC_CREATE_HAL_PIN("fault-out", bit, HAL_OUT, &(litexcnc->watchdog->estop_pins[i].hal.pin.fault_out))
            LITEXCNC_CREATE_HAL_PIN("ok-out", bit, HAL_OUT, &(litexcnc->watchdog->estop_pins[i].hal.pin.ok_out))
            *(litexcnc->watchdog->estop_pins[i].hal.pin.fault_out) = 0;
            *(litexcnc->watchdog->estop_pins[i].hal.pin.ok_out) = 1;
        }
    }

    // Success
    return 0;
}

uint8_t litexcnc_watchdog_prepare_write(litexcnc_t *litexcnc, uint8_t **data, long period) {


    if (!litexcnc->watchdog->hal.param.timeout_ns) {
        LITEXCNC_WARN(
            "Watchdog timeout not set. Using default value %"PRIu32" ns (3 times period).",
            litexcnc->fpga->name,
            (uint32_t) litexcnc->watchdog->hal.param.timeout_ns
        );
        litexcnc->watchdog->hal.param.timeout_ns = 3 * period;
    }

    // Recalculate timeout_cycles only required when timeout_ns changed
    if (litexcnc->watchdog->hal.param.timeout_ns != litexcnc->watchdog->memo.timeout_ns ) {
        // Store value to detect future scale changes
        litexcnc->watchdog->memo.timeout_ns = litexcnc->watchdog->hal.param.timeout_ns;
        // Validate new value (give warning when it is to close to the period of the thread)
        if (litexcnc->watchdog->hal.param.timeout_ns < (1.5 * period)) {
            LITEXCNC_WARN(
                "Watchdog timeout (%"PRIu32" ns) is dangerously short compared to litexcnc_write() period (%ld ns)\n",
                litexcnc->fpga->name,
                (uint32_t) litexcnc->watchdog->hal.param.timeout_ns,
                period
            );
        }
        // Convert the time to cycles
        litexcnc->watchdog->hal.param.timeout_cycles = litexcnc->watchdog->memo.timeout_ns * ((double) litexcnc->clock_frequency / (double) 1e9) - 1;
        // Limit the value to 0x7FFFFFFF
        if (litexcnc->watchdog->hal.param.timeout_cycles > 0x1FFFFFFF) {
            litexcnc->watchdog->hal.param.timeout_cycles = 0x1FFFFFFF;
            litexcnc->watchdog->memo.timeout_ns = (litexcnc->watchdog->hal.param.timeout_cycles + 1) / ((double) litexcnc->clock_frequency / (double) 1e9);
            LITEXCNC_ERR_NO_DEVICE("Requested watchdog timeout is out of range, setting it to max: %"PRIu32" ns\n", (uint32_t) litexcnc->watchdog->memo.timeout_ns);
        }
    }

    // Latch the results - sense ok_in and fault_in
    if (*(litexcnc->watchdog->hal.pin.ok_out)) {
        *(litexcnc->watchdog->hal.pin.ok_out) = (
            (*(litexcnc->watchdog->hal.pin.ok_in)) & 
            (~*(litexcnc->watchdog->hal.pin.fault_in))
        );
        // Negate the signal
        *(litexcnc->watchdog->hal.pin.fault_out) = *(litexcnc->watchdog->hal.pin.ok_out) ? false : true;
    }

    // Should the watchdog be reset
    uint8_t should_reset = (*(litexcnc->watchdog->hal.pin.reset) & ~litexcnc->watchdog->memo.reset);
    litexcnc->watchdog->memo.reset = *(litexcnc->watchdog->hal.pin.reset);
    if (should_reset & *(litexcnc->watchdog->hal.pin.ok_in) & (~*(litexcnc->watchdog->hal.pin.fault_in))) {
        *(litexcnc->watchdog->hal.pin.fpga_timeout) = false;
        *(litexcnc->watchdog->hal.pin.comm_timeout) = false;
        *(litexcnc->watchdog->hal.pin.fault_out) = false;
        *(litexcnc->watchdog->hal.pin.ok_out) = true;
        for (size_t i=0; i<litexcnc->watchdog->num_estops; i++) {
            *(litexcnc->watchdog->estop_pins[i].hal.pin.fault_out) = 0;
            *(litexcnc->watchdog->estop_pins[i].hal.pin.ok_out) = 1;
        }
    }

    // Store the parameter on the FPGA (also set the enable bit)
    litexcnc_watchdog_data_write_t output;
    output.timeout_cycles = htobe32(
        litexcnc->watchdog->hal.param.timeout_cycles + 
        (*(litexcnc->watchdog->hal.pin.fpga_timeout) ? 0 : 0x80000000) +
        (should_reset ? 0x40000000 : 0) +
        (*(litexcnc->watchdog->hal.pin.ok_out) ? 0x20000000 : 0)
    );
    
    // Copy the data to the output and advance the pointer  
    memcpy(*data, &output, LITEXCNC_WATCHDOG_DATA_WRITE_SIZE);
    
    // LITEXCNC_PRINT_NO_DEVICE(
    //     "%02X\n", 
    //     (~*(litexcnc->watchdog->hal.pin.fpga_timeout) ? 0x80000000: 0) +
    //     (should_reset ? 0x40000000 : 0) +
    //     ((~*(litexcnc->watchdog->hal.pin.fault_in) | *(litexcnc->watchdog->hal.pin.ok_in)) ? 0x20000000 : 0)
    // );
    // LITEXCNC_PRINT_NO_DEVICE("%02X %02X %02X %02X\n",
    //     (unsigned char)*(*data+0),
    //     (unsigned char)*(*data+1),
    //     (unsigned char)*(*data+2),
    //     (unsigned char)*(*data+3)
    // );

    *data += LITEXCNC_WATCHDOG_DATA_WRITE_SIZE;

    // Success
    return 0;
}


size_t required_read_buffer(litexcnc_watchdog_t *watchdog) {
    return (((watchdog->num_estops+1)>>5) + (((watchdog->num_estops+1) & 0x1F)?1:0)) * 4;
}


uint8_t litexcnc_watchdog_process_read(litexcnc_t *litexcnc, uint8_t** data) {
    
    // Process all the EStops
    static uint8_t mask;
    static bool estop_active;
    estop_active = false;
    mask = 0x80;
    for (size_t i=required_read_buffer(litexcnc->watchdog)*8; i>1; i--) {
        // The counter i can have a value outside the range of possible pins. We only
        // should add data to existing pins
        if (i <= litexcnc->watchdog->num_estops+1) {
            if (*(*data) & mask) {
                // ESTOP active
                *(litexcnc->watchdog->estop_pins[i-2].hal.pin.fault_out) = 1;
                *(litexcnc->watchdog->estop_pins[i-2].hal.pin.ok_out) = 0;
                estop_active = true; 
            }
        }
        // Modify the mask for the next. When the mask is zero (happens in case of a 
        // roll-over), we should proceed to the next byte and reset the mask.
        mask >>= 1;
        if (!mask) {
            mask = 0x80;  // Reset the mask
            (*data)++; // Proceed the buffer to the next element
        }
    }

    // Check whether the watchdog did bite
    if ((*(*data) & mask) && (*(litexcnc->watchdog->hal.pin.fpga_timeout)==0)) {
        LITEXCNC_ERR_NO_DEVICE("Watchdog has bitten.");
        *(litexcnc->watchdog->hal.pin.fpga_timeout) = 1;
    }
    (*data)++;

    // Latch the results
    if (*(litexcnc->watchdog->hal.pin.ok_out)) {
        *(litexcnc->watchdog->hal.pin.ok_out) = (
            (*(litexcnc->watchdog->hal.pin.ok_in)) & 
            (~*(litexcnc->watchdog->hal.pin.fault_in)) & 
            (~*(litexcnc->watchdog->hal.pin.fpga_timeout)) &
            (~estop_active)
        );
        // Negate the signal
        *(litexcnc->watchdog->hal.pin.fault_out) = *(litexcnc->watchdog->hal.pin.ok_out) ? false : true;
    }

    // Success
    return 0;
}

uint8_t litexcnc_watchdog_process_read_error(litexcnc_t *litexcnc, long period) {

    if ((*(litexcnc->fpga->hal.pin.read_errors) * period) > litexcnc->watchdog->hal.param.timeout_ns) {
        *(litexcnc->watchdog->hal.pin.ok_out) = false;
        *(litexcnc->watchdog->hal.pin.fault_out) = true;
        *(litexcnc->watchdog->hal.pin.comm_timeout) = true;   
    }

}

