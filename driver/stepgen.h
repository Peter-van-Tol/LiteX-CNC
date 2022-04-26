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

#include "litexcnc.h"

#define PICKOFF 28 
#define POSITION_MODE 0

// Defines the structure of the PWM instance
typedef struct {
    struct {

        struct {
            hal_u32_t *counts;                /* The current position, in counts */
            hal_float_t *position_fb;         /* The current position, in length units (see parameter position-scale). The resolution of position-fb is much finer than a single step. If you need to see individual steps, use counts. */ 
            hal_float_t *position_prediction; /* The predicted position, in length units (see parameter position-scale), at the start of the next position command execution */ 
            hal_float_t *speed_fb;            /* The current speed, in length units per second (see parameter position-scale). */ 
            hal_float_t *speed_prediction;    /* The predicted speed, in length units per second (see parameter position-scale), at the start of the next position command execution */ 
            hal_bit_t *enable;                /* Enables output steps - when false, no steps are generated and is the hardware disabled */
            hal_float_t *velocity_cmd;        /* Commanded velocity, in length units per second (see parameter position-scale). */
            hal_float_t *position_cmd;        /* Commanded position, in length units (see parameter position-scale). */ 
        } pin;

        struct {
            hal_float_t frequency;      /* The current step rate, in steps per second */ 
            hal_float_t maxaccel;       /* The acceleration/deceleration limit, in length units per second squared. */ 
            hal_float_t maxvel;         /* The maximum allowable velocity, in length units per second. */ 
            hal_float_t position_scale; /* The scaling for position feedback, position command, and velocity command, in steps per length unit. */ 
            hal_u32_t steplen;          /* The length of the step pulses, in nanoseconds. Measured from rising edge to falling edge. */
            hal_u32_t stepspace;        /* The minimum space between step pulses, in nanoseconds. Measured from falling edge to rising edge. The actual time depends on the step rate and can be much longer. Is used to calculate the maximum stepping frequency */ 
            hal_u32_t dir_setup_time;   /* The minimum setup time from direction to step, in nanoseconds periods. Measured from change of direction to rising edge of step. */
            hal_u32_t dir_hold_time;    /* The minimum hold time of direction after step, in nanoseconds. Measured from falling edge of step to change of direction */
            hal_bit_t position_mode;    /* Flag indicating this stepgen should work in position mode, default is True */
        } param;

    } hal;

    // This struct holds all old values (memoization) 
    struct {
        int64_t position;
        hal_float_t position_cmd;
        hal_float_t position_scale;
        hal_u32_t steplen;
        hal_u32_t stepspace;
        hal_u32_t dir_setup_time;
        hal_u32_t dir_hold_time;  
        bool error_max_speed_printed;
    } memo;

    // This struct contains data, both calculated and direct received from the FPGA
    struct {
        int64_t position;
        int32_t speed;
        float scale_recip;
        hal_u32_t steplen_cycles;
        hal_u32_t stepspace_cycles;
        hal_u32_t dirsetup_cycles;
        hal_u32_t dirhold_cycles; 
    } data;
    
} litexcnc_stepgen_pin_t;


// Defines the PWM, contains a collection of PWM instances
typedef struct {
    // Input pins
    int num_instances;
    litexcnc_stepgen_pin_t *instances;

    struct {
        long period;
    } memo;
    
    // Struct containing pre-calculated values
    struct {
        float cycles_per_ns; 
        float max_frequency;
        float recip_dt;
    } data;

} litexcnc_stepgen_t;


// Defines the data-package for sending the settings for a single step generator. The
// order of this package MUST coincide with the order in the MMIO definition.
// - write
typedef struct {
    uint32_t steplen;
    uint32_t dir_hold_time;
    uint32_t dir_setup_time;
    uint64_t apply_time;
} litexcnc_stepgen_general_write_data_t;
#define LITEXCNC_STEPGEN_GENERAL_WRITE_DATA_SIZE sizeof(litexcnc_stepgen_general_write_data_t)
typedef struct {
    uint32_t speed_target;
    uint32_t max_acceleration;
} litexcnc_stepgen_instance_write_data_t;
#define LITEXCNC_STEPGEN_INSTANCE_WRITE_DATA_SIZE sizeof(litexcnc_stepgen_instance_write_data_t)
#define LITEXCNC_BOARD_STEPGEN_DATA_WRITE_SIZE(litexcnc) sizeof(litexcnc_stepgen_general_write_data_t) + LITEXCNC_STEPGEN_INSTANCE_WRITE_DATA_SIZE*litexcnc->stepgen.num_instances
// - read
typedef struct {
    int64_t position;
    uint32_t speed;
} litexcnc_stepgen_instance_read_data_t;
#define LITEXCNC_BOARD_STEPGEN_DATA_READ_SIZE(litexcnc) sizeof(litexcnc_stepgen_instance_read_data_t)*litexcnc->stepgen.num_instances // PWM does not send data back


// Functions for creating, reading and writing stepgen pins
int litexcnc_stepgen_init(litexcnc_t *litexcnc, json_object *config);
uint8_t litexcnc_stepgen_prepare_write(litexcnc_t *litexcnc, uint8_t **data, long period);
uint8_t litexcnc_stepgen_process_read(litexcnc_t *litexcnc, uint8_t** data, long period);

#endif