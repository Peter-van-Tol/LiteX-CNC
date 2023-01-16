/********************************************************************
* Description:  gpio.c
*               A Litex-CNC component for general purpose input and 
*               outputs (GPIO). 
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

#include "rtapi.h"
#include "rtapi_app.h"
#include "litexcnc.h"

#include "gpio.h"


static int litexcnc_gpio_out_init(litexcnc_t *litexcnc, cJSON *config) {

    int r;
    size_t i;
    const cJSON *gpio_config = NULL;
    const cJSON *gpio_instance_config = NULL;
    const cJSON *gpio_instance_name = NULL;
    char base_name[HAL_NAME_LEN + 1];   // i.e. <board_name>.<board_index>.gpio.<gpio_name>
    char name[HAL_NAME_LEN + 1];        // i.e. <base_name>.<pin_name>

    gpio_config = cJSON_GetObjectItemCaseSensitive(config, "gpio_out");
    if (cJSON_IsArray(gpio_config)) {
        // Store the amount of GPIO-out instances on this board
        litexcnc->gpio.num_output_pins = cJSON_GetArraySize(gpio_config);
        litexcnc->config.num_gpio_outputs = litexcnc->gpio.num_output_pins;
        
        // Allocate the module-global HAL shared memory
        litexcnc->gpio.output_pins = (litexcnc_gpio_output_pin_t *)hal_malloc(litexcnc->gpio.num_output_pins * sizeof(litexcnc_gpio_output_pin_t));
        if (litexcnc->gpio.output_pins == NULL) {
            LITEXCNC_ERR_NO_DEVICE("Out of memory!\n");
            r = -ENOMEM;
            return r;
        }
        
        // Create the pins and params in the HAL
        i = 0;
        cJSON_ArrayForEach(gpio_instance_config, gpio_config) {
            // Basename for the pins
            gpio_instance_name = cJSON_GetObjectItemCaseSensitive(gpio_instance_config, "name");
            if (cJSON_IsString(gpio_instance_name) && (gpio_instance_name->valuestring != NULL)) {
                rtapi_snprintf(base_name, sizeof(base_name), "%s.gpio.%s", litexcnc->fpga->name, gpio_instance_name->valuestring);
            } else {
                rtapi_snprintf(base_name, sizeof(base_name), "%s.gpio.%02zu", litexcnc->fpga->name, i);
            }
            
            // Pins for the output
            rtapi_snprintf(name, sizeof(name), "%s.out", base_name); 
            r = hal_pin_bit_new(name, HAL_IN, &(litexcnc->gpio.output_pins[i].hal.pin.out), litexcnc->fpga->comp_id);
            if (r < 0) { goto fail_pins; }
            
            // Parameter for inverting the output
            rtapi_snprintf(name, sizeof(name), "%s.invert_output", base_name); 
            r = hal_param_bit_new(name, HAL_RW, &(litexcnc->gpio.output_pins[i].hal.param.invert_output), litexcnc->fpga->comp_id);
            if (r < 0) { goto fail_params; }
            
            // Increase counter to proceed to the next GPIO
            i++;
        }
    }

    return 0;

fail_pins:
    LITEXCNC_ERR_NO_DEVICE("Error adding pin '%s', aborting\n", name);
    return r;

fail_params:
    LITEXCNC_ERR_NO_DEVICE("Error adding param '%s', aborting\n", name);
    return r;
}

static int litexcnc_gpio_in_init(litexcnc_t *litexcnc, cJSON *config) {
    
    int r;
    size_t i;
    const cJSON *gpio_config = NULL;
    const cJSON *gpio_instance_config = NULL;
    const cJSON *gpio_instance_name = NULL;
    char base_name[HAL_NAME_LEN + 1];   // i.e. <board_name>.<board_index>.gpio.<gpio_name>
    char name[HAL_NAME_LEN + 1];        // i.e. <base_name>.<pin_name>

    gpio_config = cJSON_GetObjectItemCaseSensitive(config, "gpio_in");
    if (cJSON_IsArray(gpio_config)) {
        // Store the amount of GPIO-out instances on this board
        litexcnc->gpio.num_input_pins = cJSON_GetArraySize(gpio_config);
        litexcnc->config.num_gpio_inputs = litexcnc->gpio.num_input_pins;
        
        // Allocate the module-global HAL shared memory
        litexcnc->gpio.input_pins = (litexcnc_gpio_input_pin_t *)hal_malloc(litexcnc->gpio.num_input_pins * sizeof(litexcnc_gpio_input_pin_t));
        if (litexcnc->gpio.input_pins == NULL) {
            LITEXCNC_ERR_NO_DEVICE("out of memory!\n");
            r = -ENOMEM;
            return r;
        }

        // Create the pins and params in the HAL
        i = 0;
        cJSON_ArrayForEach(gpio_instance_config, gpio_config) {
            // Basename for the pins
            gpio_instance_name = cJSON_GetObjectItemCaseSensitive(gpio_instance_config, "name");
            if (cJSON_IsString(gpio_instance_name) && (gpio_instance_name->valuestring != NULL)) {
                rtapi_snprintf(base_name, sizeof(base_name), "%s.gpio.%s", litexcnc->fpga->name, gpio_instance_name->valuestring);
            } else {
                rtapi_snprintf(base_name, sizeof(base_name), "%s.gpio.%02zu", litexcnc->fpga->name, i);
            }
            
            // Pin for the input
            rtapi_snprintf(name, sizeof(name), "%s.in", base_name); 
            r = hal_pin_bit_new(name, HAL_IN, &(litexcnc->gpio.input_pins[i].hal.pin.in), litexcnc->fpga->comp_id);
            if (r < 0) { goto fail_pins; }
            
            // Pin for inverted input
            rtapi_snprintf(name, sizeof(name), "%s.in-not", base_name); 
            r = hal_pin_bit_new(name, HAL_OUT, &(litexcnc->gpio.input_pins[i].hal.pin.in_not), litexcnc->fpga->comp_id);
            if (r < 0) { goto fail_pins; }
            
            // Increase counter to proceed to the next GPIO
            i++;
        }
    }

    return 0;

fail_pins:
    LITEXCNC_ERR_NO_DEVICE("Error adding pin '%s', aborting\n", name);
    return r;

// NOTE: Include the code below in case params areadded to the GPIO In
// fail_params:
//     LITEXCNC_ERR_NO_DEVICE("Error adding param '%s', aborting\n", name);
//     return r; 
}


int litexcnc_gpio_init(litexcnc_t *litexcnc, cJSON *config) {
    int r;

    r = litexcnc_gpio_in_init(litexcnc, config);
    if (r<0) {
        return r;
    }
    r = litexcnc_gpio_out_init(litexcnc, config);
    if (r<0) {
        return r;
    }

    return r;
}


uint8_t litexcnc_gpio_prepare_write(litexcnc_t *litexcnc, uint8_t **data) {

    if (litexcnc->gpio.num_output_pins == 0) {
        return 0;
    }

    // Process all the bytes
    static uint8_t mask;
    mask = 0x80;
    for (size_t i=LITEXCNC_BOARD_GPIO_DATA_WRITE_SIZE(litexcnc)*8; i>0; i--) {
        // The counter i can have a value outside the range of possible pins. We only
        // should add data from existing pins
        if (i <= litexcnc->gpio.num_output_pins) {
            *(*data) |= (*(litexcnc->gpio.output_pins[i-1].hal.pin.out) ^ litexcnc->gpio.output_pins[i-1].hal.param.invert_output)?mask:0;
        }
        // Modify the mask for the next. When the mask is zero (happens in case of a 
        // roll-over), we should proceed to the next byte and reset the mask.
        mask >>= 1;
        if (!mask) {
            mask = 0x80;  // Reset the mask
            (*data)++; // Proceed the buffer to the next element
        }
    }

    return 0;
}


uint8_t litexcnc_gpio_process_read(litexcnc_t *litexcnc, uint8_t** data) {

    if (litexcnc->gpio.num_input_pins == 0) {
        return 0;
    }

    // Process all the bytes
    static uint8_t mask;
    mask = 0x80;
    for (size_t i=LITEXCNC_BOARD_GPIO_DATA_READ_SIZE(litexcnc)*8; i>0; i--) {
        // The counter i can have a value outside the range of possible pins. We only
        // should add data to existing pins
        if (i <= litexcnc->gpio.num_input_pins) {
            if (*(*data) & mask) {
                // GPIO active
                *(litexcnc->gpio.input_pins[i-1].hal.pin.in) = 1;
                *(litexcnc->gpio.input_pins[i-1].hal.pin.in_not) = 0;
            } else {
                // GPIO inactive
                *(litexcnc->gpio.input_pins[i-1].hal.pin.in) = 0;
                *(litexcnc->gpio.input_pins[i-1].hal.pin.in_not) = 1;
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

    return 0;
}
