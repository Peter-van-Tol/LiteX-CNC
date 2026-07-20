/********************************************************************
* Description:  pwm.c
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
#include "hal.h"
#include "rtapi.h"
#include "rtapi_app.h"
#include "rtapi_string.h"

#include "litexcnc_pwm.h"

/** 
 * An array holding all instance for the module. As each boarf normally have a 
 * single instance of a type, this number coincides with the number of boards
 * which are supported by LitexCNC
 */
static litexcnc_pwm_t *instances[MAX_INSTANCES];
static int num_instances = 0;

/**
 * Parameter which contains the registration of this module woth LitexCNC 
 */
static litexcnc_module_registration_t *registration;

int register_pwm_module(void) {
    registration = (litexcnc_module_registration_t *)hal_malloc(sizeof(litexcnc_module_registration_t));
    registration->id = 0x70776d5f; /** The string `pwm_` in hex */
    rtapi_snprintf(registration->name, sizeof(registration->name), "pwm");
    registration->initialize = &litexcnc_pwm_init;
    registration->required_write_buffer = &required_write_buffer;
    return litexcnc_register_module(registration);
}
EXPORT_SYMBOL_GPL(register_pwm_module);


int rtapi_app_main(void) {
    // Show some information on the module being loaded
    LITEXCNC_PRINT_NO_DEVICE(
        "Loading Litex PWM module driver version %u.%u.%u\n", 
        LITEXCNC_PWM_VERSION_MAJOR, 
        LITEXCNC_PWM_VERSION_MINOR, 
        LITEXCNC_PWM_VERSION_PATCH
    );

    // Initialize the module
    comp_id = hal_init(LITEXCNC_PWM_NAME);
    if(comp_id < 0) return comp_id;

    // Register the module with LitexCNC (NOTE: LitexCNC should be loaded first)
    int result = register_pwm_module();
    if (result<0) return result;

    // Report GPIO is ready to be used
    hal_ready(comp_id);

    return 0;
}


void rtapi_app_exit(void) {
    hal_exit(comp_id);
    LITEXCNC_PRINT_NO_DEVICE("LitexCNC PWM module driver unloaded \n");
}


size_t required_enable_write_buffer(litexcnc_pwm_t *pwm_module) {
    return (((pwm_module->num_instances)>>5) + ((pwm_module->num_instances & 0x1F)?1:0)) * 4;
}


size_t required_write_buffer(void *module) {
    static litexcnc_pwm_t *pwm_module;
    pwm_module = (litexcnc_pwm_t *) module;
    /*
    The data size is calculated with:
    - a WORD containing all Enable data. When there are more then 32 PWM pins on
      a single instance, the width is increased with 32 bit steps;
    - the data of each PWM pin (size of litexcnc_pwm_data_t)
    */
    return required_enable_write_buffer(pwm_module) + (pwm_module->num_instances * sizeof(litexcnc_pwm_data_t));
}


int litexcnc_pwm_prepare_write(void *module, uint8_t **data, int period) {
    // This function translarte the input of the PWM component to:
    // - enable (Signal(): 1-bit unsigned integer / boolean, but stored in a 32-bit wide format)
    // - period (Signal(32): 32-bit unsigned integer)
    // - width  (Signal(32): 32-bit unsigned integer)
    static double duty_cycle;
    static litexcnc_pwm_t *pwm;
    pwm = (litexcnc_pwm_t *) module;


    // Process enable signal
    static uint8_t mask;
    mask = 0x80;
    for (size_t i=required_enable_write_buffer(pwm)*8; i>0; i--) {
        // The counter i can have a value outside the range of possible pins. We only
        // should add data from existing pins
        if (i <= pwm->num_instances) {
            *(*data) |= (*(pwm->instances[i-1].hal.pin.enable))?mask:0;
        }
        // Modify the mask for the next. When the mask is zero (happens in case of a 
        // roll-over), we should proceed to the next byte and reset the mask.
        mask >>= 1;
        if (!mask) {
            mask = 0x80;  // Reset the mask
            (*data)++; // Proceed the buffer to the next element
        }
    }

    // Process all instances
    for (size_t i=0; i < pwm->num_instances; i++) {
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
        litexcnc_pwm_instance_t *pwm_instance = &(pwm->instances[i]);

        // Validate duty cycle limits, both limits must be between 0.0 and 1.0 (inclusive) 
        // and max must be greater then min
        if ( *(pwm_instance->hal.pin.max_dc) > 1.0 ) {
            *(pwm_instance->hal.pin.max_dc) = *(pwm_instance->hal.pin.max_dc);
            // TODO: print message
        }
        if ( *(pwm_instance->hal.pin.min_dc) > *(pwm_instance->hal.pin.max_dc) ) {
            *(pwm_instance->hal.pin.min_dc) = *(pwm_instance->hal.pin.max_dc);
            // TODO: print message
        }
        if ( *(pwm_instance->hal.pin.min_dc) < 0.0 ) {
            *(pwm_instance->hal.pin.min_dc) = 0.0;
            // TODO: print message
        }
        if ( *(pwm_instance->hal.pin.max_dc) < *(pwm_instance->hal.pin.min_dc) ) {
            *(pwm_instance->hal.pin.max_dc) = *(pwm_instance->hal.pin.min_dc);
            // TODO: print message
        }

        // Scale calculations only required when scale changes
        if ( *(pwm_instance->hal.pin.scale) != pwm_instance->memo.scale ) {
            // Store value to detect future scale changes
            pwm_instance->memo.scale = *(pwm_instance->hal.pin.scale);
            // Validate new value (prevent division by zero)
            if ((*(pwm_instance->hal.pin.scale) < 1e-20) && (*(pwm_instance->hal.pin.scale) > -1e-20)) {
                *(pwm_instance->hal.pin.scale) = 1.0;
                // TODO: print message
            }
            // Calculate the reciprocal of the scalle
            pwm_instance->hal.param.scale_recip = 1.0 / *(pwm_instance->hal.pin.scale);
	    }

        // Calculate the duty cycle
        // - convert value command to duty cycle
        duty_cycle = *(pwm_instance->hal.pin.value) * pwm_instance->hal.param.scale_recip + *(pwm_instance->hal.pin.offset);
        // - unidirectional mode, no negative output
        if ( duty_cycle < 0.0 ) {
            duty_cycle = 0.0;
        }
        // - limit the duty-cylce 
        if ( duty_cycle > *(pwm_instance->hal.pin.max_dc) ) {
            duty_cycle = *(pwm_instance->hal.pin.max_dc);
        } else if ( duty_cycle < *(pwm_instance->hal.pin.min_dc) ) {
            duty_cycle = *(pwm_instance->hal.pin.min_dc);
        }

        if (*(pwm_instance->hal.pin.pwm_freq) != 0) {
            // PWM mode
            // - recalculate period only required when pwm_freq changes
            if ( *(pwm_instance->hal.pin.pwm_freq) < 1.0 ) {
                *(pwm_instance->hal.pin.pwm_freq) = 1.0;
                // TODO: print message
            }
            if ( *(pwm_instance->hal.pin.pwm_freq) != pwm_instance->memo.pwm_freq ) {
                // Store value to detect future scale changes
                pwm_instance->memo.scale = *(pwm_instance->hal.pin.scale);
                // Calculate the new width
                *(pwm_instance->hal.pin.curr_period) = (*(pwm->data.clock_frequency) / *(pwm_instance->hal.pin.pwm_freq)) + 0.5;
                pwm_instance->hal.param.period_recip = 1.0 / *(pwm_instance->hal.pin.curr_period);
            }
            // - convert duty-cycle to period -> round to the nearest duty cycle
            *(pwm_instance->hal.pin.curr_width) = (*(pwm_instance->hal.pin.curr_period) * duty_cycle) + 0.5;
            // - save rounded value to curr_dc pin
            if ( duty_cycle >= 0 ) {
                *(pwm_instance->hal.pin.curr_dc) = *(pwm_instance->hal.pin.curr_width) * pwm_instance->hal.param.period_recip;
            } else {
                *(pwm_instance->hal.pin.curr_dc) = -*(pwm_instance->hal.pin.curr_width) * pwm_instance->hal.param.period_recip;
            }
        } else {
            // PDM mode
            *(pwm_instance->hal.pin.curr_period) = 0;
            // In PDM mode, the duty cycle is store as a 16-bit integer which is send as the width
            *(pwm_instance->hal.pin.curr_width) = (0xFFFF * duty_cycle);
        }

        // Add the PWM generator to the data
        litexcnc_pwm_data_t output;
        output.period = htobe32(*(pwm_instance->hal.pin.curr_period));
        output.width = htobe32(*(pwm_instance->hal.pin.curr_width));

        // Copy the data to the output and advance the pointer
        memcpy(*data, &output, sizeof(litexcnc_pwm_data_t));
        *data += sizeof(litexcnc_pwm_data_t);
    }

    return 0;
}


size_t litexcnc_pwm_init(litexcnc_module_instance_t **module, litexcnc_t *litexcnc, uint8_t **config) {
  
    int r;
    char base_name[HAL_NAME_LEN + 1];   // i.e. <board_name>.<board_index>.pwm.<pwm_index>
    char name[HAL_NAME_LEN + 1];        // i.e. <base_name>.<pin_name>

    // Create structure in memory
    (*module) = (litexcnc_module_instance_t *)hal_malloc(sizeof(litexcnc_module_instance_t));
    (*module)->prepare_write = &litexcnc_pwm_prepare_write;
    (*module)->instance_data = hal_malloc(sizeof(litexcnc_pwm_t));
        
    // Cast from void to correct type and store it
    litexcnc_pwm_t *pwm = (litexcnc_pwm_t *) (*module)->instance_data;
    instances[num_instances] = pwm;
    num_instances++;

    // Store pointers to data from FPGA required by the process
    pwm->data.clock_frequency = &(litexcnc->clock_frequency);

    // Store the amount of pwm instances on this board and allocate HAL shared memory
    pwm->num_instances = be32toh(*(uint32_t*)*config);
    pwm->instances = (litexcnc_pwm_instance_t *)hal_malloc(pwm->num_instances * sizeof(litexcnc_pwm_instance_t));
    if (pwm->instances == NULL) {
        LITEXCNC_ERR_NO_DEVICE("Out of memory!\n");
        return -ENOMEM;
    }
    (*config) += 4;

    // Create the pins and params in the HAL
    for (size_t i=0; i<pwm->num_instances; i++) {
        litexcnc_pwm_instance_t *instance = &(pwm->instances[i]);
        
        // Create the basename
        LITEXCNC_CREATE_BASENAME("pwm", i);

        // Create the pins
        LITEXCNC_CREATE_HAL_PIN("enable", bit, HAL_IN, &(instance->hal.pin.enable))
        LITEXCNC_CREATE_HAL_PIN("value", float, HAL_IN, &(instance->hal.pin.value))
        LITEXCNC_CREATE_HAL_PIN("scale", float, HAL_IN, &(instance->hal.pin.scale))
        LITEXCNC_CREATE_HAL_PIN("offset", float, HAL_IN, &(instance->hal.pin.offset))
        LITEXCNC_CREATE_HAL_PIN("dither_pwm", bit, HAL_IN, &(instance->hal.pin.dither_pwm))
        LITEXCNC_CREATE_HAL_PIN("pwm_freq", float, HAL_IN, &(instance->hal.pin.pwm_freq))
        LITEXCNC_CREATE_HAL_PIN("min_dc", float, HAL_IN, &(instance->hal.pin.min_dc))
        LITEXCNC_CREATE_HAL_PIN("max_dc", float, HAL_IN, &(instance->hal.pin.max_dc))
        LITEXCNC_CREATE_HAL_PIN("curr_dc", float, HAL_OUT, &(instance->hal.pin.curr_dc))
        LITEXCNC_CREATE_HAL_PIN("curr_pwm_freq", float, HAL_OUT, &(instance->hal.pin.curr_pwm_freq))
        LITEXCNC_CREATE_HAL_PIN("curr_period", u32, HAL_OUT, &(instance->hal.pin.curr_period))
        LITEXCNC_CREATE_HAL_PIN("curr_width", u32, HAL_OUT, &(instance->hal.pin.curr_width))

        // Set default values for the instance (PWM is disabled by default: SAFETY!)
        *(instance->hal.pin.enable) = 0;
        *(instance->hal.pin.scale) = 1.0;
        *(instance->hal.pin.offset) = 0.0;
        *(instance->hal.pin.dither_pwm) = 0;
        *(instance->hal.pin.pwm_freq) = 100000.0;
        *(instance->hal.pin.min_dc) = 0.0;
        *(instance->hal.pin.max_dc) = 1.0;
    }

    // Succes!
    return 0;
}
