/********************************************************************
* Description:  stepgen.h
*               A Litex-CNC component that generates steps
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
#ifndef __INCLUDE_LITEXCNC_STEPGEN_H__
#define __INCLUDE_LITEXCNC_STEPGEN_H__

#include <litexcnc.h>

#define LITEXCNC_STEPGEN_NAME "litexcnc_stepgen"

#define LITEXCNC_STEPGEN_VERSION_MAJOR 1
#define LITEXCNC_STEPGEN_VERSION_MINOR 0
#define LITEXCNC_STEPGEN_VERSION_PATCH 0

#define MAX_INSTANCES 4

/** The ID of the component, only used when the component is used as stand-alone */
int comp_id;

/*******************************************************************************
 * STUCTS
 ******************************************************************************/
/** Structure of an stepgen instance */
typedef struct {
    /** Structure defining the HAL pin and params*/
    struct {
        /** Structure defining the HAL pins */
        struct {
            hal_u32_t   *counts;              /* The current position, in counts */
            hal_float_t *position_cmd;        /* Commanded position, in length units (see parameter position-scale). */
            hal_float_t *position_fb;         /* The current position, in length units (see parameter position-scale). The resolution of position-fb is much finer than a single step. If you need to see individual steps, use counts. */ 
            hal_float_t *position_prediction; /* The predicted position, in length units (see parameter position-scale), at the start of the next position command execution */ 
            hal_float_t *speed_fb;            /* The current speed, in length units per second (see parameter position-scale). */ 
            hal_float_t *speed_prediction;    /* The predicted speed, in length units per second (see parameter position-scale), at the start of the next position command execution */ 
            hal_bit_t   *enable;              /* Enables output steps - when false, no steps are generated and is the hardware disabled */
            hal_bit_t   *velocity_mode;       /* Configures the component to be in velocity mode. The default mode is position mode, in which the position-cmd is translated to a required velocity */
            hal_float_t *velocity_cmd;        /* Commanded velocity, in length units per second (see parameter position-scale). */
            hal_float_t *acceleration_cmd;    /* Commanded acceleration, in length units per second squared (see parameter position-scale). */
            hal_bit_t   *debug;               /* Flag indicating whether all positional data will be printed to the command line */
        } pin;
        /** Structure defining the HAL params */
        struct {
            hal_float_t frequency;            /* The current step rate, in steps per second */ 
            hal_float_t max_acceleration;     /* The acceleration/deceleration limit, in length units per second squared. */ 
            hal_float_t max_velocity;         /* The maximum allowable velocity, in length units per second. */ 
            hal_float_t position_scale;       /* The scaling for position feedback, position command, and velocity command, in steps per length unit. */ 
            hal_u32_t   steplen;              /* The length of the step pulses, in nanoseconds. Measured from rising edge to falling edge. */
            hal_u32_t   stepspace;            /* The minimum space between step pulses, in nanoseconds. Measured from falling edge to rising edge. The actual time depends on the step rate and can be much longer. Is used to calculate the maximum stepping frequency */ 
            hal_u32_t   dir_setup_time;       /* The minimum setup time from direction to step, in nanoseconds. Measured from change of direction to rising edge of step. */
            hal_u32_t   dir_hold_time;        /* The minimum hold time of direction after step, in nanoseconds. Measured from falling edge of step to change of direction */
        } param;
    } hal;

    // This struct holds all old values (memoization) 
    struct {
        int64_t position;
        hal_float_t position_cmd;
        hal_float_t position_scale;
        hal_float_t velocity_cmd;
        hal_u32_t steplen;
        hal_u32_t stepspace;
        hal_u32_t dir_setup_time;
        hal_u32_t dir_hold_time;
        hal_float_t acceleration;
        hal_float_t maxaccel;       
        hal_float_t maxvel;
        bool error_max_speed_printed;
    } memo;
    
    // This struct contains data, both calculated and direct received from the FPGA
    struct {        
        int64_t position;
        int32_t speed;
        float acceleration;
        float acceleration_recip;
        float speed_float;
        float scale_recip;
        float acc_recip;
        hal_u32_t steplen_cycles;
        hal_u32_t stepspace_cycles;
        hal_u32_t dirsetup_cycles;
        hal_u32_t dirhold_cycles;
        // The data being send to the FPGA (as calculated)
        float flt_acc;
        float flt_speed;
        float flt_time;
        // The data being send to the FPGA (as sent)
        uint32_t fpga_acc;
        uint32_t fpga_speed;
        uint32_t fpga_time;
        // Scales for converting from float to FPGA and vice versa
        float fpga_pos_scale_inv;
        float fpga_speed_scale;
        float fpga_speed_scale_inv;
        float fpga_acc_scale;
        float fpga_acc_scale_inv;
    } data;
} litexcnc_stepgen_instance_t;

// Defines the stepgen, contains a collection of stepgen instances
typedef struct {
    // Input pins
    int num_instances;                   /** Number of stepgen instances */
    litexcnc_stepgen_instance_t *instances;   /** Structure containing the data on the stepgen instances */

    /** Structure defining the HAL pin and params*/
    struct {
        /** Structure defining the HAL pins */
        struct {
        
        } pin;
        struct{
            hal_float_t max_driver_freq;      /* The maximum frequency of the driver in Hz. Default value is 400 kHz. */
        } param;
    } hal;

    struct {
        long period;
        uint32_t steplen_cycles;
        uint32_t stepspace_cycles;
        uint64_t apply_time;
    } memo;
    
    // Struct containing pre-calculated values
    struct {
        char *fpga_name;
        uint32_t *clock_frequency;
        float *clock_frequency_recip;
        uint64_t *wallclock_ticks;
        float period_s;
        float period_s_recip;
        float cycles_per_period;
        size_t pick_off_pos;
        size_t pick_off_vel;
        size_t pick_off_acc;
        float max_frequency;
    } data;

} litexcnc_stepgen_t;


/*******************************************************************************
 * DATAPACKAGES
 ******************************************************************************/
// - CONFIG DATA
// Defines the data-package for sending the settings for a single step generator. The
// order of this package MUST coincide with the order in the MMIO definition.
#pragma pack(push, 4)
typedef struct {
    uint32_t timings;
} litexcnc_stepgen_config_data_t;
#pragma pack(pop)

// WRITE DATA
// - global config
#pragma pack(push,4)
typedef struct {
    uint64_t apply_time;
} litexcnc_stepgen_general_write_data_t;
#pragma pack(pop)
// - instance
#pragma pack(push,4)
typedef struct {
    uint32_t speed_target;
    uint32_t acceleration;
} litexcnc_stepgen_instance_write_data_t;
#pragma pack(pop)

// READ DATA
#pragma pack(push,4)
typedef struct {
    int64_t position;
    uint32_t speed;
} litexcnc_stepgen_instance_read_data_t;
#pragma pack(pop)


/*******************************************************************************
 * FUNCTIONS
 ******************************************************************************/
 
/*******************************************************************************
 * Function which is called when a user adds the component using `loadrt 
 * litexcnc_stepgen`. It will initialize the component with LinuxCNC and registers
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
 * Initializes the component for the given FPGA.
 *
 * @param instance Pointer to the struct with the module instance data. This 
 * function will initialise this struct.
 * @param fpga_name The name of the FPGA, used to set the pin names correctly.
 * @param config The configuration of the FPGA, used to set the pin names. 
 ******************************************************************************/
size_t litexcnc_stepgen_init(
    litexcnc_module_instance_t **instance, 
    litexcnc_t *litexcnc,
    uint8_t **config
);


/*******************************************************************************
 * Returns the required buffer size for the config
 *
 * @param instance The structure containing the data on the module instance
 ******************************************************************************/
size_t required_config_buffer(void *instance);


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
 * @param period Period in nano-seconds of a cycle
 ******************************************************************************/
int litexcnc_stepgen_prepare_write(void *instance, uint8_t **data, int period);


/*******************************************************************************
 * Processes the data which has been received from a device
 *
 * @param instance The structure containing the data on the module instance
 * @param data Pointer to the array where the data is contained in. NOTE:
 *     the pointer should moved to the next element, so the next module can 
 *     process its data. All data in LitexCNC is 4-bytes wide. 
 * @param period Period in nano-seconds of a cycle
 ******************************************************************************/
int litexcnc_stepgen_process_read(void *instance, uint8_t** data, int period);

#endif
