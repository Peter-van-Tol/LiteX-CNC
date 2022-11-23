/********************************************************************
* Description:  pwm.h
*               A Litex-CNC component that generates Pulse Width
*               Modulation
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
#include "pwm.h"


int litexcnc_pwm_init(litexcnc_t *litexcnc, json_object *config) {
    
    // Declarations
    int r;
    struct json_object *pwm;
    struct json_object *pwm_instance;
    struct json_object *pwm_instance_pin_name;
    char base_name[HAL_NAME_LEN + 1];   // i.e. <board_name>.<board_index>.pwm.<pwm_name>
    char name[HAL_NAME_LEN + 1];        // i.e. <base_name>.<pin_name>
    
    // Parse the contents of the config-json
    if (json_object_object_get_ex(config, "pwm", &pwm)) {
        // Store the amount of PWM instances on this board
        litexcnc->pwm.num_instances = json_object_array_length(pwm);
        litexcnc->config.num_pwm_instances = litexcnc->pwm.num_instances;
        // Allocate the module-global HAL shared memory
        litexcnc->pwm.instances = (litexcnc_pwm_pin_t *)hal_malloc(litexcnc->pwm.num_instances * sizeof(litexcnc_pwm_pin_t));
        if (litexcnc->pwm.instances == NULL) {
            LITEXCNC_ERR_NO_DEVICE("Out of memory!\n");
            r = -ENOMEM;
            return r;
        }
        // Create the pins and params in the HAL
        for (size_t i=0; i<litexcnc->pwm.num_instances; i++) {
            pwm_instance = json_object_array_get_idx(pwm, i);
            
            // Get pointer to the pwmgen instance
            litexcnc_pwm_pin_t *instance = &(litexcnc->pwm.instances[i]);
            
            // Create the basename
            if (json_object_object_get_ex(pwm_instance, "name", &pwm_instance_pin_name)) {
                rtapi_snprintf(base_name, sizeof(base_name), "%s.pwm.%s", litexcnc->fpga->name, json_object_get_string(pwm_instance_pin_name));
            } else {
                rtapi_snprintf(base_name, sizeof(base_name), "%s.pwm.%02d", litexcnc->fpga->name, i);
            }

            // Create the pins
            // - enable
            rtapi_snprintf(name, sizeof(name), "%s.%s", base_name, "enable"); 
            r = hal_pin_bit_new(name, HAL_IN, &(instance->hal.pin.enable), litexcnc->fpga->comp_id);
            if (r < 0) {
                LITEXCNC_ERR_NO_DEVICE("Error adding pin '%s', aborting\n", name);
                return r;
            }
            // - value
            rtapi_snprintf(name, sizeof(name), "%s.%s", base_name, "value"); 
            r = hal_pin_float_new(name, HAL_IN, &(instance->hal.pin.value), litexcnc->fpga->comp_id);
            if (r < 0) {
                LITEXCNC_ERR_NO_DEVICE("Error adding pin '%s', aborting\n", name);
                return r;
            }
            // - scale
            rtapi_snprintf(name, sizeof(name), "%s.%s", base_name, "scale"); 
            r = hal_pin_float_new(name, HAL_IN, &(instance->hal.pin.scale), litexcnc->fpga->comp_id);
            if (r < 0) {
                LITEXCNC_ERR_NO_DEVICE("Error adding pin '%s', aborting\n", name);
                return r;
            }
            // - offset
            rtapi_snprintf(name, sizeof(name), "%s.%s", base_name, "offset"); 
            r = hal_pin_float_new(name, HAL_IN, &(instance->hal.pin.offset), litexcnc->fpga->comp_id);
            if (r < 0) {
                LITEXCNC_ERR_NO_DEVICE("Error adding pin '%s', aborting\n", name);
                return r;
            }
            // - dither_pwm
            rtapi_snprintf(name, sizeof(name), "%s.%s", base_name, "dither_pwm"); 
            r = hal_pin_bit_new(name, HAL_IN, &(instance->hal.pin.dither_pwm), litexcnc->fpga->comp_id);
            if (r < 0) {
                LITEXCNC_ERR_NO_DEVICE("Error adding pin '%s', aborting\n", name);
                return r;
            }
            // - pwm_freq
            rtapi_snprintf(name, sizeof(name), "%s.%s", base_name, "pwm_freq"); 
            r = hal_pin_float_new(name, HAL_IN, &(instance->hal.pin.pwm_freq), litexcnc->fpga->comp_id);
            if (r < 0) {
                LITEXCNC_ERR_NO_DEVICE("Error adding pin '%s', aborting\n", name);
                return r;
            }
            // - min_dc
            rtapi_snprintf(name, sizeof(name), "%s.%s", base_name, "min_dc"); 
            r = hal_pin_float_new(name, HAL_IN, &(instance->hal.pin.min_dc), litexcnc->fpga->comp_id);
            if (r < 0) {
                LITEXCNC_ERR_NO_DEVICE("Error adding pin '%s', aborting\n", name);
                return r;
            }
            // - max_dc
            rtapi_snprintf(name, sizeof(name), "%s.%s", base_name, "max_dc"); 
            r = hal_pin_float_new(name, HAL_IN, &(instance->hal.pin.max_dc), litexcnc->fpga->comp_id);
            if (r < 0) {
                LITEXCNC_ERR_NO_DEVICE("Error adding pin '%s', aborting\n", name);
                return r;
            }
            // - curr_dc
            rtapi_snprintf(name, sizeof(name), "%s.%s", base_name, "curr_dc"); 
            r = hal_pin_float_new(name, HAL_OUT, &(instance->hal.pin.curr_dc), litexcnc->fpga->comp_id);
            if (r < 0) {
                LITEXCNC_ERR_NO_DEVICE("Error adding pin '%s', aborting\n", name);
                return r;
            }
            // - curr_pwm_freq
            rtapi_snprintf(name, sizeof(name), "%s.%s", base_name, "curr_pwm_freq"); 
            r = hal_pin_float_new(name, HAL_OUT, &(instance->hal.pin.curr_pwm_freq), litexcnc->fpga->comp_id);
            if (r < 0) {
                LITEXCNC_ERR_NO_DEVICE("Error adding pin '%s', aborting\n", name);
                return r;
            }
            // - curr_period
            rtapi_snprintf(name, sizeof(name), "%s.%s", base_name, "curr_period"); 
            r = hal_pin_u32_new(name, HAL_OUT, &(instance->hal.pin.curr_period), litexcnc->fpga->comp_id);
            if (r < 0) {
                LITEXCNC_ERR_NO_DEVICE("Error adding pin '%s', aborting\n", name);
                return r;
            }
            // - curr_width
            rtapi_snprintf(name, sizeof(name), "%s.%s", base_name, "curr_width"); 
            r = hal_pin_u32_new(name, HAL_OUT, &(instance->hal.pin.curr_width), litexcnc->fpga->comp_id);
            if (r < 0) {
                LITEXCNC_ERR_NO_DEVICE("Error adding pin '%s', aborting\n", name);
                return r;
            }

            // // Set default values for the instance (PWM is disabled by default: SAFETY!)
            *(instance->hal.pin.enable) = 0;
            *(instance->hal.pin.scale) = 1.0;
            *(instance->hal.pin.offset) = 0.0;
            *(instance->hal.pin.dither_pwm) = 0;
            *(instance->hal.pin.pwm_freq) = 100000.0;
            *(instance->hal.pin.min_dc) = 0.0;
            *(instance->hal.pin.max_dc) = 1.0;
        }
        // Free up the memory
        json_object_put(pwm_instance_pin_name);
        json_object_put(pwm_instance);
        json_object_put(pwm);
    }

    return 0;
}


uint8_t litexcnc_pwm_prepare_write(litexcnc_t *litexcnc, uint8_t **data) {
    // This function translarte the input of the PWM component to:
    // - enable (Signal(): 1-bit unsigned integer / boolean, but stored in a 32-bit wide format)
    // - period (Signal(32): 32-bit unsigned integer)
    // - width  (Signal(32): 32-bit unsigned integer)

    double duty_cycle;

    // Process all instances
    for (size_t i=0; i < litexcnc->pwm.num_instances; i++) {
        /**
        This code is based on the original pwmgen.c code by John Kasunich. Original source code
        can be found here: 
            https://github.com/LinuxCNC/linuxcnc/blob/9e28b3d8fe23fff0e08fc0f8d232c96be04404a6/src/hal/components/pwmgen.c
        
        The code by John Kasunich is licensed under GPL v2.0.

        Changes made with respect to the original code:
            - change variable names to connect to the structs of LiteX-CNC;
            - print message when parameters are out of bound and thus modified;
            - the PWM is generated in hardware, therefore the calculation of the period is modified, as
              this uses the frequency of the board.
        */
        // Get pointer to the pwmgen instance
        litexcnc_pwm_pin_t *instance = &(litexcnc->pwm.instances[i]);

        // Validate duty cycle limits, both limits must be between 0.0 and 1.0 (inclusive) 
        // and max must be greater then min
        if ( *(instance->hal.pin.max_dc) > 1.0 ) {
            *(instance->hal.pin.max_dc) = *(instance->hal.pin.max_dc);
            // TODO: print message
        }
        if ( *(instance->hal.pin.min_dc) > *(instance->hal.pin.max_dc) ) {
            *(instance->hal.pin.min_dc) = *(instance->hal.pin.max_dc);
            // TODO: print message
        }
        if ( *(instance->hal.pin.min_dc) < 0.0 ) {
            *(instance->hal.pin.min_dc) = 0.0;
            // TODO: print message
        }
        if ( *(instance->hal.pin.max_dc) < *(instance->hal.pin.min_dc) ) {
            *(instance->hal.pin.max_dc) = *(instance->hal.pin.min_dc);
            // TODO: print message
        }

        // Scale calculations only required when scale changes
        if ( *(instance->hal.pin.scale) != instance->memo.scale ) {
            // Store value to detect future scale changes
            instance->memo.scale = *(instance->hal.pin.scale);
            // Validate new value (prevent division by zero)
            if ((*(instance->hal.pin.scale) < 1e-20) && (*(instance->hal.pin.scale) > -1e-20)) {
                *(instance->hal.pin.scale) = 1.0;
                // TODO: print message
            }
            // Calculate the reciprocal of the scalle
            instance->hal.param.scale_recip = 1.0 / *(instance->hal.pin.scale);
	    }

        // Calculate the duty cycle
        // - convert value command to duty cycle
        duty_cycle = *(instance->hal.pin.value) * instance->hal.param.scale_recip + *(instance->hal.pin.offset);
        // - unidirectional mode, no negative output
        if ( duty_cycle < 0.0 ) {
            duty_cycle = 0.0;
        }
        // - limit the duty-cylce 
        if ( duty_cycle > *(instance->hal.pin.max_dc) ) {
            duty_cycle = *(instance->hal.pin.max_dc);
        } else if ( duty_cycle < *(instance->hal.pin.min_dc) ) {
            duty_cycle = *(instance->hal.pin.min_dc);
        }

        if (*(instance->hal.pin.pwm_freq) != 0) {
            // PWM mode
            // - recalculate period only required when pwm_freq changes
            if ( *(instance->hal.pin.pwm_freq) < 1.0 ) {
                *(instance->hal.pin.pwm_freq) = 1.0;
                // TODO: print message
            }
            if ( *(instance->hal.pin.pwm_freq) != instance->memo.pwm_freq ) {
                // Store value to detect future scale changes
                instance->memo.scale = *(instance->hal.pin.scale);
                // Calculate the new width
                *(instance->hal.pin.curr_period) = (litexcnc->clock_frequency / *(instance->hal.pin.pwm_freq)) + 0.5;
                instance->hal.param.period_recip = 1.0 / *(instance->hal.pin.curr_period);
            }
            // - convert duty-cycle to period -> round to the nearest duty cycle
            *(instance->hal.pin.curr_width) = (*(instance->hal.pin.curr_period) * duty_cycle) + 0.5;
            // - save rounded value to curr_dc pin
            if ( duty_cycle >= 0 ) {
                *(instance->hal.pin.curr_dc) = *(instance->hal.pin.curr_width) * instance->hal.param.period_recip;
            } else {
                *(instance->hal.pin.curr_dc) = -*(instance->hal.pin.curr_width) * instance->hal.param.period_recip;
            }
        } else {
            // PDM mode
            *(instance->hal.pin.curr_period) = 0;
            // In PDM mode, the duty cycle is store as a 16-bit integer which is send as the width
            *(instance->hal.pin.curr_width) = (0xFFFF * duty_cycle);
        }

        // Add the PWM generator to the data
        litexcnc_pwm_data_t output;
        output.enable = htobe32(*(instance->hal.pin.enable));
        output.period = htobe32(*(instance->hal.pin.curr_period));
        output.width = htobe32(*(instance->hal.pin.curr_width));

        // Copy the data to the output and advance the pointer
        memcpy(*data, &output, LITEXCNC_PWM_DATA_SIZE);
        *data += LITEXCNC_PWM_DATA_SIZE;
    }

    return 0;
}

uint8_t litexcnc_pwm_process_read(litexcnc_t *litexcnc, uint8_t **data) {
    // This function is deliberately empty as no data is read back from the board
    // to the HAL component.
}
