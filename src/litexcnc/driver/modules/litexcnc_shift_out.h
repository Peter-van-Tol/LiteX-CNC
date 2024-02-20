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
#ifndef __INCLUDE_LITEXCNC_SHIFTOUT_H__
#define __INCLUDE_LITEXCNC_SHIFTOUT_H__

#include <litexcnc.h>

#define LITEXCNC_SHIFTOUT_NAME "litexcnc_shift_out"

#define LITEXCNC_SHIFTOUT_VERSION_MAJOR 1
#define LITEXCNC_SHIFTOUT_VERSION_MINOR 0
#define LITEXCNC_SHIFTOUT_VERSION_PATCH 0

#define MAX_INSTANCES 4

/** Structure of an output pin */
typedef struct {
    /** Structure defining the HAL pin and params*/
    struct {
        /** Structure defining the HAL pins */
        struct {
            hal_bit_t *out;                 /** The desired state of the output pin */
        } pin;
        /** Structure defining the HAL params */
        struct {
            hal_bit_t invert_output;        /** When set, the output is inverted (i.e. active low) */
        } param;
    } hal;
} litexcnc_shift_out_pin_t;

/** Structure of the Shift-out instance module */
typedef struct {
    int num_pins;                            /** Number of pins */
    litexcnc_shift_out_pin_t *pins;    /** Structure containing the data on the input pins */
} litexcnc_shift_out_instance_t;

/** Structure of the Shift-out module */
typedef struct {
    int num_instances;                        /** Number of shift_out instances */
    litexcnc_shift_out_instance_t *instances; /** Structure containing the data on the shift_out instances */
} litexcnc_shift_out_t;


/*******************************************************************************
 * FUNCTIONS
 ******************************************************************************/

/*******************************************************************************
 * Returns the required buffer size for the write buffer
 *
 * @param instance Pointer to the struct with the module instance data. This 
 * function will initialise this struct.
 * @param fpga_name The name of the FPGA, used to set the pin names correctly.
 * @param config The configuration of the FPGA, used to set the pin names. 
 ******************************************************************************/
size_t litexcnc_shift_out_init(
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
// NOT USED: The shift out module does not receive data from the FPGA
// size_t required_read_buffer(void *instance);


/*******************************************************************************
 * Prepares the data to be written out to the device
 *
 * @param instance The structure containing the data on the module instance
 * @param data Pointer to the array where the data should be written to. NOTE:
 *     the pointer should moved to the next element, so the next module can 
 *     append its data. All data in LitexCNC is 4-bytes wide. 
 * @param period Period in nano-seconds of a cycle (not used for GPIO)
 ******************************************************************************/
int litexcnc_shift_out_prepare_write(void *instance, uint8_t **data, int period);


/*******************************************************************************
 * Processes the data which has been received from a device
 *
 * @param instance The structure containing the data on the module instance
 * @param data Pointer to the array where the data is contained in. NOTE:
 *     the pointer should moved to the next element, so the next module can 
 *     process its data. All data in LitexCNC is 4-bytes wide. 
 * @param period Period in nano-seconds of a cycle (not used for GPIO)
 ******************************************************************************/
// NOT USED: The shift out module does not receive data from the FPGA
// int litexcnc_shift_out_process_read(void *instance, uint8_t** data, int period);


#endif