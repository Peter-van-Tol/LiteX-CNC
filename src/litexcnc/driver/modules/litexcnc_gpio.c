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
#include "hal.h"
#include "rtapi.h"
#include "rtapi_app.h"
#include "rtapi_string.h"

#include "litexcnc_gpio.h"

/** 
 * An array holding all instance for the module. As each boarf normally have a 
 * single instance of a type, this number coincides with the number of boards
 * which are supported by LitexCNC
 */
static litexcnc_gpio_t *instances[MAX_INSTANCES];
static int num_instances = 0;

/**
 * Parameter which contains the registration of this module woth LitexCNC 
 */
static litexcnc_module_registration_t *registration;

int register_gpio_module(void) {
    registration = (litexcnc_module_registration_t *)hal_malloc(sizeof(litexcnc_module_registration_t));
    registration->id = 0x6770696f; /** The string `gpio` in hex */
    rtapi_snprintf(registration->name, sizeof(registration->name), "gpio");
    registration->initialize = &litexcnc_gpio_init;
    registration->required_write_buffer = &required_write_buffer;
    registration->required_read_buffer  = &required_read_buffer;
    return litexcnc_register_module(registration);
}
EXPORT_SYMBOL_GPL(register_gpio_module);


int rtapi_app_main(void) {
    // Show some information on the module being loaded
    LITEXCNC_PRINT_NO_DEVICE(
        "Loading Litex GPIO module version %u.%u.%u\n", 
        LITEXCNC_GPIO_VERSION_MAJOR, 
        LITEXCNC_GPIO_VERSION_MINOR, 
        LITEXCNC_GPIO_VERSION_PATCH
    );

    // Initialize the module
    comp_id = hal_init(LITEXCNC_GPIO_NAME);
    if(comp_id < 0) return comp_id;

    // Register the module with LitexCNC (NOTE: LitexCNC should be loaded first)
    int result = register_gpio_module();
    if (result<0) return result;

    // Report GPIO is ready to be used
    hal_ready(comp_id);

    return 0;
}


void rtapi_app_exit(void) {
    hal_exit(comp_id);
    LITEXCNC_PRINT_NO_DEVICE("LitexCNC GPIO module driver unloaded \n");
}


size_t required_write_buffer(void *instance) {
    static litexcnc_gpio_t *gpio;
    gpio = (litexcnc_gpio_t *) instance;
    return (((gpio->num_output_pins)>>5) + ((gpio->num_output_pins & 0x1F)?1:0)) * 4;
}


size_t required_read_buffer(void *instance) {
    static litexcnc_gpio_t *gpio;
    gpio = (litexcnc_gpio_t *) instance;
    return (((gpio->num_input_pins)>>5) + ((gpio->num_input_pins & 0x1F)?1:0)) * 4;
}


int litexcnc_gpio_prepare_write(void *instance, uint8_t **data, int period) {
    static litexcnc_gpio_t *gpio;
    gpio = (litexcnc_gpio_t *) instance;

    // Safeguard, don't do anything when there are no output pins defined
    if (gpio->num_output_pins == 0) {
        return 0;
    }

    // Process all the bytes
    static unsigned char mask;
    mask = 0x80;
    for (size_t i=required_write_buffer(instance)*8; i>0; i--) {
        // The counter i can have a value outside the range of possible pins. We only
        // should add data from existing pins
        if (i <= gpio->num_output_pins) {
            *(*data) |= (*(gpio->output_pins[i-1].hal.pin.out) ^ gpio->output_pins[i-1].hal.param.invert_output)?mask:0;
        }
        // Modify the mask for the next. When the mask is zero (happens in case of a 
        // roll-over), we should proceed to the next byte and reset the mask.
        mask >>= 1;
        if (!mask) {
            mask = 0x80;  // Reset the mask
            (*data)++; // Proceed the buffer to the next element
        }
    }

    // Return succes
    return 0;
}


int litexcnc_gpio_process_read(void *instance, uint8_t** data, int period){
    static litexcnc_gpio_t *gpio;
    gpio = (litexcnc_gpio_t *) instance;

    // Safeguard, don't do anything when there are no output pins defined
    if (gpio->num_input_pins == 0) {
        return 0;
    }
    
    // Process all the bytes
    static uint8_t mask;
    mask = 0x80;
    for (size_t i=required_read_buffer(instance)*8; i>0; i--) {
        // The counter i can have a value outside the range of possible pins. We only
        // should add data to existing pins
        if (i <= gpio->num_input_pins) {
            if (*(*data) & mask) {
                // GPIO active
                *(gpio->input_pins[i-1].hal.pin.in) = 1;
                *(gpio->input_pins[i-1].hal.pin.in_not) = 0;
            } else {
                // GPIO inactive
                *(gpio->input_pins[i-1].hal.pin.in) = 0;
                *(gpio->input_pins[i-1].hal.pin.in_not) = 1;
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


static int litexcnc_gpio_init_out(litexcnc_gpio_output_pin_t *gpio_instance, litexcnc_t *litexcnc, size_t index) {

    int r;
    char base_name[HAL_NAME_LEN + 1];   // i.e. <board_name>.<board_index>.gpio.<gpio_name>
    char name[HAL_NAME_LEN + 1];        // i.e. <base_name>.<pin_name>
        
    // Basename for the pins
    LITEXCNC_CREATE_BASENAME("gpio", index);
    // Pins and params for the output
    LITEXCNC_CREATE_HAL_PIN("out", bit, HAL_IN, &(gpio_instance->hal.pin.out));
    LITEXCNC_CREATE_HAL_PARAM("invert_output", bit, HAL_RW, &(gpio_instance->hal.param.invert_output));

    // Indicate success         
    return 0;
}


static int litexcnc_gpio_init_in(litexcnc_gpio_input_pin_t *gpio_instance, litexcnc_t *litexcnc, size_t index) {

    int r;
    char base_name[HAL_NAME_LEN + 1];   // i.e. <board_name>.<board_index>.gpio.<gpio_name>
    char name[HAL_NAME_LEN + 1];        // i.e. <base_name>.<pin_name>
        
    // Basename for the pins
    LITEXCNC_CREATE_BASENAME("gpio", index);
    // Pins and params for the output
    LITEXCNC_CREATE_HAL_PIN("in", bit, HAL_OUT, &(gpio_instance->hal.pin.in))
    LITEXCNC_CREATE_HAL_PIN("in-not", bit, HAL_OUT, &(gpio_instance->hal.pin.in_not))

    // Indicate success         
    return 0;
}

size_t litexcnc_gpio_init(litexcnc_module_instance_t **module, litexcnc_t *litexcnc, uint8_t **config) {

    // Create structure in memory
    (*module) = (litexcnc_module_instance_t *)hal_malloc(sizeof(litexcnc_module_instance_t));
    (*module)->prepare_write = &litexcnc_gpio_prepare_write;
    (*module)->process_read = &litexcnc_gpio_process_read;
    (*module)->instance_data = hal_malloc(sizeof(litexcnc_gpio_t));
    
    // Cast from void to correct type and store it
    litexcnc_gpio_t *gpio = (litexcnc_gpio_t *) (*module)->instance_data;
    instances[num_instances] = gpio;
    num_instances++;
    
    // Store the amount of GPIO-out instances on this board and create space in memory
    // - output
    gpio->num_output_pins = *(*config);
    gpio->output_pins = (litexcnc_gpio_output_pin_t *)hal_malloc(gpio->num_output_pins * sizeof(litexcnc_gpio_output_pin_t));
    if (gpio->output_pins == NULL) {
        LITEXCNC_ERR_NO_DEVICE("Out of memory!\n");
        return -ENOMEM;
    }
    (*config)++;
    // - input
    gpio->num_input_pins = *(*config);
    gpio->input_pins = (litexcnc_gpio_input_pin_t *)hal_malloc(gpio->num_input_pins * sizeof(litexcnc_gpio_input_pin_t));
    if (gpio->input_pins == NULL) {
        LITEXCNC_ERR_NO_DEVICE("Out of memory!\n");
        return -ENOMEM;
    }
    (*config)++;

    // Initialize the input pins
    size_t total_pins = gpio->num_input_pins + gpio->num_output_pins;
    size_t buffer_size = (((total_pins + 16)>>5) + (((total_pins + 16) & 0x1F)?1:0)) * 4 - 2;
    
    // Process the remainder  bytes
    uint8_t mask = 0x80;
    size_t input_counter = gpio->num_input_pins;
    size_t output_counter = gpio->num_output_pins;
    for (size_t i=(buffer_size*8); i>0; i--) {
        // The counter i can have a value outside the range of possible pins. We only
        // should add data to existing pins
        if (i <= total_pins) {
            if (*(*config) & mask) {
                // GPIO output
                output_counter--;
                litexcnc_gpio_init_out(&(gpio->output_pins[output_counter]), litexcnc, i-1);
            } else {
                // GPIO input
                input_counter--;
                litexcnc_gpio_init_in(&(gpio->input_pins[input_counter]), litexcnc, i-1);
            }
        }
        // Modify the mask for the next. When the mask is zero (happens in case of a 
        // roll-over), we should proceed to the next byte and reset the mask.
        mask >>= 1;
        if (!mask) {
            mask = 0x80;  // Reset the mask
            (*config)++; // Proceed the buffer to the next element
        }
    }

    // Succes!
    return 0;
}
