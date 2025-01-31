/********************************************************************
* Description:  litexcnc_encoder.h
*               A Litex-CNC component that canm be used to measure 
*               position by counting the pulses generated by a 
*               quadrature encoder. 
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
#ifndef __INCLUDE_LITEXCNC_ENCODER_H__
#define __INCLUDE_LITEXCNC_ENCODER_H__

#include <litexcnc.h>

#define LITEXCNC_ENCODER_NAME "litexcnc_encoder"

#define LITEXCNC_ENCODER_VERSION_MAJOR 1
#define LITEXCNC_ENCODER_VERSION_MINOR 0
#define LITEXCNC_ENCODER_VERSION_PATCH 0

#define MAX_INSTANCES 4

/** 
 * Number of cycles used to calculate the average speed. The minimum speed of the
 * encoder which can be detected whilst using this method is:
 *    1 / (LITEXCNC_ENCODER_POSITION_AVERAGE_SIZE * ENCODER_PPR) * Frequency
 * For example, when using a 3000 PPR Encoder and remembering 8 positions and a 
 * frequency of 1 kHz, this results in 1/24 rotation per second. We have to take
 * into account that at these low speeds the resolution of the speed is detoriated.
*/
#define LITEXCNC_ENCODER_POSITION_AVERAGE_SIZE 8
#define LITEXCNC_ENCODER_POSITION_AVERAGE_RECIP 0.125

/** The ID of the component, only used when the component is used as stand-alone */
int comp_id;

/*******************************************************************************
 * STUCTS
 ******************************************************************************/

/** Structure of an encoder instance */
typedef struct {
    struct {

        struct {
            /* Position in encoder counts, reflects the current state of the registers in the
             *FPGA. Is not affected by a reset. Can roll-over when a roll-over happens in the
             *FPGA. 
             */
            hal_s32_t *raw_counts; 
            /* Position in encoder counts. Can be reset using the `reset` pin. Is compensated
             * for roll-overs in the FPGA.
             */
            hal_s32_t *counts; 
            /* When true, counts and position are reset to zero on the next rising edge of
             * Phase-Z. At the same time, index-enable is reset to zero to indicate that
             * the rising edge has occurred. 
             */             
            hal_bit_t *index_enable;
            /* When true, a rising edge has been detected on the FPGA. This flag will be active
             * until the next read action from the FPGA, when it is automatically reset.
             */
            hal_bit_t *index_pulse;
            /* Position in scaled units (see position-scale) */ 
            hal_float_t *position;
            /* When true, counts and position are reset to zero immediately. NOTE: the value on the
             * FPGA and thus raw_counts is not affected by this reset.
             */
            hal_bit_t *reset;
            /* Velocity in scaled units per second. encoder uses an algorithm that greatly reduces
             * quantization noise as compared to simply differentiating the position output. When 
             * the magnitude of the true velocity is below min-speed-estimate, the velocity output 
             * is 0.
             */
            hal_float_t *velocity;
            /* Velocity in scaled units per minute. Simply encoder.N.velocity scaled by a factor 
             * of 60 for convenience.
             */
            hal_float_t *velocity_rpm;
            /* Velocity in scaled units per minute. Simply encoder.N.velocity scaled by a factor 
             * of 60 for convenience.
             */
            hal_bit_t *overflow_occurred;
        } pin;

        struct {
            /* Scale factor, in counts per (length) unit. For example, if position-scale is 500, 
             * then 1000 counts of the encoder will be reported as a position of 2.0 units.
             */ 
            hal_float_t position_scale;
            /* Position offset in scaled units. When the encoder count is 0, this will be the
             * reported position. Can be used to create a digital potentiometer.
             */
            hal_float_t position_offset;
            /* Enables times-4 mode. When true (the default), the counter counts each edge of the
             * quadrature waveform (four counts per full cycle). When false, it only counts once 
             * per full cycle. NOTE: this function is implemented by dividing applying an integer
             * division of 4 between received data and counts.
             */
            hal_bit_t x4_mode;
        } param;

    } hal;

    // This struct contains data, both calculated and direct received from the FPGA
    struct {
        hal_float_t position_scale_recip; /** 1/position_scale, calculated once to save on floating point calculations */
    } data;

    /** This struct holds data from previous cycles, so changes can be detected */
    struct {
        hal_s32_t position_reset;
        hal_float_t position_scale;
        hal_float_t position_offset;
        hal_float_t velocity[LITEXCNC_ENCODER_POSITION_AVERAGE_SIZE];
        size_t velocity_pointer;
    } memo;


} litexcnc_encoder_instance_t;


/** Structure of the GPIO module */
typedef struct {
    int num_instances;                      /** Number of encoder instances */
    litexcnc_encoder_instance_t *instances; /** Structure containing the data on the encoders */
    /** This struct holds data equal to all instances of this module */
    struct {
        float recip_dt; /** 1/period, calculated once to save on floating point calculations */
    } data;
    /** This struct holds data from previous cycles, so changes can be detected */
    struct {
        long period; /** period of a single cycle */
        size_t velocity_pointer;
    } memo;
} litexcnc_encoder_t;

/** Structure of the data which is retrieved from the FPGA for each encoder instance */
#pragma pack(push,4)
typedef struct {
    int32_t counts;
} litexcnc_encoder_instance_read_data_t;
#pragma pack(pop)

/*******************************************************************************
 * FUNCTIONS
 ******************************************************************************/
 
/*******************************************************************************
 * Function which is called when a user adds the component using `loadrt 
 * litexcnc_gpio`. It will initialize the component with LinuxCNC and registers
 * the exposed init, read and write functions with LitexCNC so they can be used
 * by the driver. 
 ******************************************************************************/
int rtapi_app_main(void);

/*******************************************************************************
 * Function which is called when the realtime application is stopped (i.e. when
 * LinuxCNC is stopped).
 ******************************************************************************/
void rtapi_app_exit(void);


/*******************************************************************************
 * Returns the required buffer size for the write buffer
 *
 * @param instance Pointer to the struct with the module instance data. This 
 * function will initialise this struct.
 * @param fpga_name The name of the FPGA, used to set the pin names correctly.
 * @param config The configuration of the FPGA, used to set the pin names. 
 ******************************************************************************/
size_t litexcnc_encoder_init(
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
int litexcnc_encoder_prepare_write(void *instance, uint8_t **data, int period);


/*******************************************************************************
 * Processes the data which has been received from a device
 *
 * @param instance The structure containing the data on the module instance
 * @param data Pointer to the array where the data is contained in. NOTE:
 *     the pointer should moved to the next element, so the next module can 
 *     process its data. All data in LitexCNC is 4-bytes wide. 
 * @param period Period in nano-seconds of a cycle (not used for GPIO)
 ******************************************************************************/
int litexcnc_encoder_process_read(void *instance, uint8_t** data, int period);

#endif
