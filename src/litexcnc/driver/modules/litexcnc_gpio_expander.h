/********************************************************************
* Description:  litexcnc_gpio_expander.h
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
#ifndef __INCLUDE_LITEXCNC_GPIO_EXPANDER_H__
#define __INCLUDE_LITEXCNC_GPIO_EXPANDER_H__

#include <litexcnc.h>
#include "litexcnc_gpio.h"

#define LITEXCNC_GPIO_EXPANDER_NAME "litexcnc_gpio_expander"

#define LITEXCNC_GPIO_EXPANDER_VERSION_MAJOR 1
#define LITEXCNC_GPIO_EXPANDER_VERSION_MINOR 0
#define LITEXCNC_GPIO__EXPANDERVERSION_PATCH 0

#define MAX_INSTANCES 4

/** The ID of the component, only used when the component is used as stand-alone */
int comp_id;

/*******************************************************************************
 * STRUCTS
 * 
 * NOTE: the structs litexcnc_gpio_input_pin_t, litexcnc_gpio_output_pin_t, and
 * litexcnc_gpio_t from the 'regular' litexcnc GPIO are used.
 ******************************************************************************/

 /** Structure of the GPIO module */
typedef struct {
    int num_input_pins;                       /** Number of input pins */
    litexcnc_gpio_input_pin_t *input_pins;    /** Structure containing the data on the input pins */
    int num_output_pins;                      /** Number of output pins */
    litexcnc_gpio_output_pin_t *output_pins;  /** Structure containing the data on the output pins */

    // These are the functions called to process the data of this instance
    int (*prepare_write)(void *instance, uint8_t **data, int period);
    int (*process_read)(void *instance, uint8_t **data, int period);

    // Pre-calculated values of the required read and write data
    size_t required_write_buffer;
    size_t required_read_buffer;
} litexcnc_gpio_expander_instance_t;

/** Structure of the PWM module */
typedef struct {
    int num_instances;                            /** Number of gpio expander instances */
    litexcnc_gpio_expander_instance_t *instances; /** Structure containing the data on the gpio expander instances */
} litexcnc_gpio_expander_t;


/*******************************************************************************
 * Returns the required buffer size for the write buffer
 *
 * @param instance Pointer to the struct with the module instance data. This 
 * function will initialise this struct.
 * @param fpga_name The name of the FPGA, used to set the pin names correctly.
 * @param config The configuration of the FPGA, used to set the pin names. 
 ******************************************************************************/
 size_t litexcnc_gpio_expander_init(
    litexcnc_module_instance_t **instance, 
    litexcnc_t *litexcnc,
    uint8_t **config
);


/*******************************************************************************
 * Returns the required buffer size for the write buffer
 *
 * @param instance The structure containing the data on the module instance
 ******************************************************************************/
size_t required_write_buffer(void *instance);


/*******************************************************************************
 * Returns the required buffer size for the read buffer
 *
 * @param instance The structure containing the data on the module instance
 ******************************************************************************/
size_t required_read_buffer(void *instance);


/*******************************************************************************
 * Prepares the data to be written out to the device
 *
 * @param instance The structure containing the data on the module instance
 * @param data Pointer to the array where the data should be written to. NOTE:
 *     the pointer should moved to the next element, so the next module can 
 *     append its data. All data in LitexCNC is 4-bytes wide. 
 * @param period Period in nano-seconds of a cycle (not used for GPIO)
 ******************************************************************************/
int litexcnc_gpio_expander_prepare_write(void *instance, uint8_t **data, int period);


/*******************************************************************************
 * Processes the data which has been received from a device
 *
 * @param instance The structure containing the data on the module instance
 * @param data Pointer to the array where the data is contained in. NOTE:
 *     the pointer should moved to the next element, so the next module can 
 *     process its data. All data in LitexCNC is 4-bytes wide. 
 * @param period Period in nano-seconds of a cycle (not used for GPIO)
 ******************************************************************************/
int litexcnc_gpio_expander_process_read(void *instance, uint8_t** data, int period);

#endif