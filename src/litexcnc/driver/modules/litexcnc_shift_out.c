/********************************************************************
* Description:  litexcnc_shift_out.c
*               A Litex-CNC component which drives 74HCT595 or similar
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

#include "litexcnc_shift_out.h"

/** 
 * An array holding all instance for the module. As each board normally have a 
 * single instance of a type, this number coincides with the number of boards
 * which are supported by LitexCNC
 */
static litexcnc_shift_out_t *instances[MAX_INSTANCES];
static int num_instances = 0;

/**
 * Parameter which contains the registration of this module woth LitexCNC 
 */
static litexcnc_module_registration_t *registration;

int register_shift_out_module(void) {
    registration = (litexcnc_module_registration_t *)hal_malloc(sizeof(litexcnc_module_registration_t));
    registration->id = 0x2d2d2d3e; /** The string `--->` in hex */
    rtapi_snprintf(registration->name, sizeof(registration->name), "shift_out");
    registration->initialize = &litexcnc_shift_out_init;
    registration->required_write_buffer = &required_write_buffer;
    return litexcnc_register_module(registration);
}
EXPORT_SYMBOL_GPL(register_shift_out_module);


size_t litexcnc_shift_out_init(litexcnc_module_instance_t **module, litexcnc_t *litexcnc, uint8_t **config) {
  
    int r;
    char base_name[HAL_NAME_LEN + 1];   // i.e. <board_name>.<board_index>.shift_out.<pwm_index>
    char name[HAL_NAME_LEN + 1];        // i.e. <base_name>.<pin_name>

    // Create structure in memory
    (*module) = (litexcnc_module_instance_t *)hal_malloc(sizeof(litexcnc_module_instance_t));
    (*module)->prepare_write = &litexcnc_shift_out_prepare_write;
    (*module)->instance_data = hal_malloc(sizeof(litexcnc_shift_out_t));

    // Cast from void to correct type and store it
    litexcnc_shift_out_t *shift_out = (litexcnc_shift_out_t *) (*module)->instance_data;
    instances[num_instances] = shift_out;
    num_instances++;

    // Store the amount of pwm instances on this board and allocate HAL shared memory
    shift_out->num_instances = *(*config);
    shift_out->instances = (litexcnc_shift_out_instance_t *)hal_malloc(shift_out->num_instances * sizeof(litexcnc_shift_out_instance_t));
    if (shift_out->instances == NULL) {
        LITEXCNC_ERR_NO_DEVICE("Out of memory!\n");
        return -ENOMEM;
    }
    (*config)++;

    // Create the pins and params in the HAL
    for (size_t i=0; i<shift_out->num_instances; i++) {
        litexcnc_shift_out_instance_t *instance = &(shift_out->instances[i]);

        // Create space for the pins
        instance->num_pins = *(*config) + 1;
        instance->pins = (litexcnc_shift_out_pin_t *)hal_malloc(instance->num_pins * sizeof(litexcnc_shift_out_pin_t));
        (*config)++;

        // Connect the pins to HAL
        for (size_t j=0; j<instance->num_pins; j++) {
            // Create the basename
            rtapi_snprintf(base_name, sizeof(base_name), "%s.%s.%02zu.%02zu", litexcnc->fpga->name, "shift_out", i, j);
            // Create the pins and params
            LITEXCNC_CREATE_HAL_PIN("out", bit, HAL_IN, &(instance->pins[j].hal.pin.out));
            LITEXCNC_CREATE_HAL_PARAM("invert_output", bit, HAL_RW, &(instance->pins[j].hal.param.invert_output));
        }
    }

    // Align the config on the DWORD boundary
    (*config) += 4 - ((1 + shift_out->num_instances) & 0x03);

    // Succes!
    return 0;
}

size_t required_write_buffer_instance(litexcnc_shift_out_instance_t *instance) {
    return (((instance->num_pins)>>5) + ((instance->num_pins & 0x1F)?1:0)) * 4;
}



size_t required_write_buffer(void *module) {
    static litexcnc_shift_out_t *shift_out;
    shift_out = (litexcnc_shift_out_t *) module;
    /*
    The data size is calculated with:
    - a WORD containing all data. When there are more then 32 shift_out pins on
      a single instance, the width is increased with 32 bit steps;
    */
    size_t data;
    for (size_t i=0; i<shift_out->num_instances; i++) {
        litexcnc_shift_out_instance_t *instance = &(shift_out->instances[i]);
        data += required_write_buffer_instance(instance);
    }
    return data;
}


int litexcnc_shift_out_prepare_write(void *module, uint8_t **data, int period) {

    // Convert module to shift out
    static litexcnc_shift_out_t *shift_out;
    shift_out = (litexcnc_shift_out_t *) module;

    // Loop over all instances
    for (size_t i=0; i<shift_out->num_instances; i++) {
        litexcnc_shift_out_instance_t *instance = &(shift_out->instances[i]);

        // Process all the bytes
        static uint8_t mask;
        mask = 0x80;
        for (size_t i=required_write_buffer_instance(instance)*8; i>0; i--) {
            // The counter `i` can have a value outside the range of possible pins. We only
            // should add data to existing pins
            if (i <= instance->num_pins) {
                *(*data) |= (*(instance->pins[i-1].hal.pin.out) ^ instance->pins[i-1].hal.param.invert_output)?mask:0;
            }
            // Modify the mask for the next. When the mask is zero (happens in case of a 
            // roll-over), we should proceed to the next byte and reset the mask.
            mask >>= 1;
            if (!mask) {
                mask = 0x80;  // Reset the mask
                (*data)++; // Proceed the buffer to the next element
            }
        }
    }
}
