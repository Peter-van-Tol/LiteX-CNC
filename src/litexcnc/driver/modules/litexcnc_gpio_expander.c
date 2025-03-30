/********************************************************************
* Description:  litexcnc_gpio_expander.c
*               A Litex-CNC component for port expanders for GPIO.
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

#include "litexcnc_gpio_expander.h"

/** 
 * An array holding all instance for the module. As each boarf normally have a 
 * single instance of a type, this number coincides with the number of boards
 * which are supported by LitexCNC
 */
 static litexcnc_gpio_expander_t *instances[MAX_INSTANCES];
 static int num_instances = 0;

 /**
 * Parameter which contains the registration of this module woth LitexCNC 
 */
static litexcnc_module_registration_t *registration;

int register_gpio_expander_module(void) {
    registration = (litexcnc_module_registration_t *)hal_malloc(sizeof(litexcnc_module_registration_t));
    registration->id = 0x00657870; /** The string `exp` in hex */
    rtapi_snprintf(registration->name, sizeof(registration->name), "gpio_expander");
    registration->initialize = &litexcnc_gpio_expander_init;
    registration->required_write_buffer = &required_write_buffer;
    registration->required_read_buffer  = &required_read_buffer;
    return litexcnc_register_module(registration);
}
EXPORT_SYMBOL_GPL(register_gpio_expander_module);


size_t required_write_buffer(void *instance) {
    static litexcnc_gpio_expander_t *gpio_expanders;
    size_t required_write_buffer;
    gpio_expanders = (litexcnc_gpio_expander_t *) instance;
    for(size_t i = 0; i < gpio_expanders->num_instances; i++) {
        required_write_buffer += gpio_expanders->instances[i].required_write_buffer;
    }
    return required_write_buffer;
}


size_t required_read_buffer(void *instance) {
    static litexcnc_gpio_expander_t *gpio_expanders;
    size_t required_read_buffer;
    gpio_expanders = (litexcnc_gpio_expander_t *) instance;
    for(size_t i = 0; i < gpio_expanders->num_instances; i++) {
        required_read_buffer += gpio_expanders->instances[i].required_read_buffer;
    }
    return required_read_buffer;
}


int litexcnc_gpio_expander_74hct595_prepare_write(void *instance, uint8_t **data, int period) {
    static litexcnc_gpio_expander_instance_t *expander;
    expander = (litexcnc_gpio_expander_instance_t *) instance;

    // Safeguard, don't do anything when there are no output pins defined
    if (expander->num_output_pins == 0) {
        return 0;
    }

    // Process all the bytes
    static unsigned char mask;
    mask = 0x80;
    for (size_t i=expander->required_write_buffer*8; i>0; i--) {
        // The counter i can have a value outside the range of possible pins. We only
        // should add data from existing pins
        if (i <= expander->num_output_pins) {
            *(*data) |= (*(expander->output_pins[i-1].hal.pin.out) ^ expander->output_pins[i-1].hal.param.invert_output)?mask:0;
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


int litexcnc_gpio_expander_prepare_write(void *instance, uint8_t **data, int period) {
    static litexcnc_gpio_expander_t *gpio_expanders;
    gpio_expanders = (litexcnc_gpio_expander_t *) instance;

    // Safeguard, don't do anything when there are no output pins defined
    if (gpio_expanders->num_instances == 0) {
        return 0;
    }

    for(size_t i = 0; i < gpio_expanders->num_instances; i++) {
        litexcnc_gpio_expander_instance_t expander = gpio_expanders->instances[i];
        if (expander.prepare_write != NULL) {
            expander.prepare_write(&expander, data, period);
        }
    }

    return 0;
}


int litexcnc_gpio_expander_process_read(void *instance, uint8_t** data, int period) {
    static litexcnc_gpio_expander_t *gpio_expanders;
    gpio_expanders = (litexcnc_gpio_expander_t *) instance;

    // Safeguard, don't do anything when there are no output pins defined
    if (gpio_expanders->num_instances == 0) {
        return 0;
    }

    for(size_t i = 0; i < gpio_expanders->num_instances; i++) {
        // TODO
    }

    return 0;
}


static int litexcnc_gpio_init_out(litexcnc_gpio_output_pin_t *gpio_instance, litexcnc_t *litexcnc, size_t chain_index, size_t pin_index) {

    int r;
    char base[HAL_NAME_LEN + 1];   // i.e. gpio.<gpio_name>
    char base_name[HAL_NAME_LEN + 1];   // i.e. <board_name>.<board_index>.gpio.<chain_index>.<gpio_name>
    char name[HAL_NAME_LEN + 1];        // i.e. <base_name>.<pin_name>
        
    // Basename for the pins
    rtapi_snprintf(base, sizeof(base), "gpio.%02zu", chain_index);
    LITEXCNC_CREATE_BASENAME(base, pin_index);
    // Pins and params for the output
    LITEXCNC_CREATE_HAL_PIN("out", bit, HAL_IN, &(gpio_instance->hal.pin.out));
    LITEXCNC_CREATE_HAL_PARAM("invert-output", bit, HAL_RW, &(gpio_instance->hal.param.invert_output));

    // Indicate success         
    return 0;
}


static int litexcnc_gpio_init_in(litexcnc_gpio_input_pin_t *gpio_instance, litexcnc_t *litexcnc, size_t chain_index, size_t index) {

    int r;
    char base[HAL_NAME_LEN + 1];   // i.e. gpio.<gpio_name>
    char base_name[HAL_NAME_LEN + 1];   // i.e. <board_name>.<board_index>.gpio.<chain_index>.<gpio_name>
    char name[HAL_NAME_LEN + 1];        // i.e. <base_name>.<pin_name>
        
    // Basename for the pins
    rtapi_snprintf(base, sizeof(base), "gpio.%02zu", chain_index);
    LITEXCNC_CREATE_BASENAME(base, pin_index);
    // Pins and params for the output
    LITEXCNC_CREATE_HAL_PIN("in", bit, HAL_OUT, &(gpio_instance->hal.pin.in))
    LITEXCNC_CREATE_HAL_PIN("in-not", bit, HAL_OUT, &(gpio_instance->hal.pin.in_not))

    // Indicate success         
    return 0;
}


/* 
 * Configuration of a 74hct595 shift register. This register only has outputs and shifts
 * data out. The number of pins are in the first (and only) byte of the configuration.
 */
size_t litexcnc_gpio_expander_74hct595_init(
    litexcnc_gpio_expander_instance_t *expander,
    litexcnc_t *litexcnc,
    uint8_t **config,
    uint8_t config_length,
    size_t chain_index
) {
    // Declarations
    size_t i;

    // Configuration of the expander
    expander->num_input_pins = 0;
    expander->required_read_buffer = 0;
    expander->process_read = NULL;
    expander->num_output_pins = *(*config);
    expander->required_write_buffer = (((expander->num_output_pins)>>5) + ((expander->num_output_pins & 0x1F)?1:0)) * 4;
    expander->prepare_write = &(litexcnc_gpio_expander_74hct595_prepare_write);
    *config += 1;

    // Create space in th memory for the pins
    expander->output_pins = (litexcnc_gpio_output_pin_t *)hal_malloc(expander->num_output_pins * sizeof(litexcnc_gpio_output_pin_t));
    if (expander->output_pins == NULL) {
        LITEXCNC_ERR_NO_DEVICE("Out of memory!\n");
        return -ENOMEM;
    }

    // Create the pins
    for (i=0; i<expander->num_output_pins; i++) {
        litexcnc_gpio_init_out(
            &expander->output_pins[i],
            litexcnc,
            chain_index,
            i
        );
    }
}


size_t litexcnc_gpio_expander_init(litexcnc_module_instance_t **module, litexcnc_t *litexcnc, uint8_t **config) {

    // Declarations
    size_t bytes_read;
    uint8_t expander_type;
    uint8_t expander_config_length;

    // Create structure in memory
    (*module) = (litexcnc_module_instance_t *)hal_malloc(sizeof(litexcnc_module_instance_t));
    (*module)->prepare_write = &litexcnc_gpio_expander_prepare_write;
    (*module)->process_read = &litexcnc_gpio_expander_process_read;
    (*module)->instance_data = hal_malloc(sizeof(litexcnc_gpio_expander_t));

    // Cast from void to correct type and store it
    litexcnc_gpio_expander_t *gpio_expanders = (litexcnc_gpio_expander_t *) (*module)->instance_data;
    instances[num_instances] = gpio_expanders;
    num_instances++;

    // Store the amount of GPIO expander instances on this board and create space in memory
    gpio_expanders->num_instances = *(*config);
    (*config) += 1;
    bytes_read++;
    gpio_expanders->instances = (litexcnc_gpio_expander_instance_t *)hal_malloc(gpio_expanders->num_instances * sizeof(litexcnc_gpio_expander_instance_t));
    if (gpio_expanders->instances == NULL) {
        LITEXCNC_ERR_NO_DEVICE("Out of memory!\n");
        return -ENOMEM;
    }
    
    // Configure each of the port expanders
    for (size_t i; i<gpio_expanders->num_instances; i++) {
        // Read the config type and the number of configuration bytes the expander unit has
        expander_type = *(*config) >> 4;
        expander_config_length = *(*config) & 0xF;
        (*config) += 1;
        bytes_read++;
        // TODO: currently there is only 1 supported expander, makes live easier
        litexcnc_gpio_expander_74hct595_init(
            &gpio_expanders->instances[i], litexcnc, config, expander_config_length, i
        );
    }

    // Align with WORD boundary
    while (bytes_read & 0b11) {
        (*config) += 1;
        bytes_read++;
    }

    return 0;
}

 