//
//    Copyright (C) 2022 Peter van Tol
//
//    This program is free software; you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation; either version 2 of the License, or
//    (at your option) any later version.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with this program; if not, write to the Free Software
//    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
//
#ifndef __INCLUDE_LITEXCNC_GPIO_H__
#define __INCLUDE_LITEXCNC_GPIO_H__

#include "litexcnc.h"

// Defines the structure of the gpio
typedef struct {
    struct {

        struct {
            hal_bit_t *in;
            hal_bit_t *in_not;
        } pin;

        struct {
            
        } param;

    } hal;
    
} litexcnc_gpio_input_pin_t;

// Defines the structure of the gpio
typedef struct {
    struct {

        struct {
            hal_bit_t *out;
        } pin;

        struct {
            hal_bit_t invert_output;
        } param;

    } hal;

} litexcnc_gpio_output_pin_t;

// Defines the GPIO, contains a collection of input and output pins
typedef struct {
    // Input pins
    int num_input_pins;
    litexcnc_gpio_input_pin_t *input_pins;
    
    // Output pins
    int num_output_pins;
    litexcnc_gpio_output_pin_t *output_pins;

} litexcnc_gpio_t;

#define LITEXCNC_BOARD_GPIO_DATA_WRITE_SIZE(litexcnc) (((litexcnc->gpio.num_output_pins)>>5) + ((litexcnc->gpio.num_output_pins & 0x1F)?1:0)) *4
#define LITEXCNC_BOARD_GPIO_DATA_READ_SIZE(litexcnc) (((litexcnc->gpio.num_input_pins)>>5) + ((litexcnc->gpio.num_input_pins & 0x1F)?1:0)) * 4

// Functions for creating, reading and writing GPIO pins
int litexcnc_gpio_init(litexcnc_t *litexcnc, json_object *config);
uint8_t litexcnc_gpio_prepare_write(litexcnc_t *litexcnc, uint8_t **data);
uint8_t litexcnc_gpio_process_read(litexcnc_t *litexcnc, uint8_t** data);

#endif