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

#include "cJSON/cJSON.h"

#define STEPGEN_WALLCLOCK_BUFFER 10
#define STEPGEN_WALLCLOCK_BUFFER_RECIP 1.0 / STEPGEN_WALLCLOCK_BUFFER

// Defines the structure of the PWM instance
typedef struct {
    struct {

        struct {
            hal_u32_t   *counts;              /* The current position, in counts */
            hal_float_t *position_fb;         /* The current position, in length units (see parameter position-scale). The resolution of position-fb is much finer than a single step. If you need to see individual steps, use counts. */ 
            hal_float_t *position_prediction; /* The predicted position, in length units (see parameter position-scale), at the start of the next position command execution */ 
            hal_float_t *speed_fb;            /* The current speed, in length units per second (see parameter position-scale). */ 
            hal_float_t *speed_prediction;    /* The predicted speed, in length units per second (see parameter position-scale), at the start of the next position command execution */ 
            hal_bit_t   *enable;              /* Enables output steps - when false, no steps are generated and is the hardware disabled */
            hal_float_t *velocity_cmd;        /* Commanded velocity, in length units per second (see parameter position-scale). */
            hal_float_t *acceleration_cmd;    /* Commanded acceleration, in length units per second squared (see parameter position-scale). */
            hal_bit_t   *debug;               /* Flag indicating whether all positional data will be printed to the command line */
            hal_float_t *period_s;            /* The calculated period (averaged over 10 cycles) based on the FPGA wall clock */ 
            hal_float_t *period_s_recip;      /* The reciprocal of the calculated period. Calculated here once, to prevent slow division on multiple locations */ 
        } pin;

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
        hal_float_t maxaccel;       
        hal_float_t maxvel;
        bool error_max_speed_printed;
    } memo;

    // This struct contains data, both calculated and direct received from the FPGA
    struct {
        int64_t position;
        int32_t speed;
        float acceleration;
        float speed_float;
        float scale_recip;
        float acc_recip;
        hal_u32_t steplen_cycles;
        hal_u32_t stepspace_cycles;
        hal_u32_t dirsetup_cycles;
        hal_u32_t dirhold_cycles;
        size_t pick_off_pos;
        size_t pick_off_vel;
        size_t pick_off_acc;
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
    
} litexcnc_stepgen_pin_t;


typedef struct {

    struct {
        hal_float_t *period_s;            /* The calculated period (averaged over 10 cycles) based on the FPGA wall clock */ 
        hal_float_t *period_s_recip;      /* The reciprocal of the calculated period. Calculated here once, to prevent slow division on multiple locations */ 
    } pin;

    // struct {

    // } param;
    
} litexcnc_stepgen_hal_t;

// Defines the PWM, contains a collection of PWM instances
typedef struct {
    // Input pins
    int num_instances;
    litexcnc_stepgen_pin_t *instances;
    litexcnc_stepgen_hal_t *hal;

    struct {
        long period;
        float period_s;
        float period_s_recip;
        float cycles_per_period;
        uint32_t steplen_cycles;
        uint32_t stepspace_cycles;
        uint64_t apply_time;
        uint64_t prev_wall_clock;
    } memo;
    
    // Struct containing pre-calculated values
    struct {
        float max_frequency;
        bool warning_apply_time_exceeded_shown;
        // Data for calculating the average period_s
        size_t wallclock_buffer_pos;
        float wallclock_buffer_sum;
        float wallclock_buffer[STEPGEN_WALLCLOCK_BUFFER];
    } data;

} litexcnc_stepgen_t;


// Defines the data-package for sending the settings for a single step generator. The
// order of this package MUST coincide with the order in the MMIO definition.
// - config
#pragma pack(push, 4)
typedef struct {
    uint32_t stepdata;
} litexcnc_stepgen_config_data_t;
#pragma pack(pop)
#define LITEXCNC_STEPGEN_CONFIG_DATA_SIZE sizeof(litexcnc_stepgen_config_data_t)
// - write
#pragma pack(push,4)
typedef struct {
    uint64_t apply_time;
} litexcnc_stepgen_general_write_data_t;
#pragma pack(pop)
#define LITEXCNC_STEPGEN_GENERAL_WRITE_DATA_SIZE sizeof(litexcnc_stepgen_general_write_data_t)
#pragma pack(push,4)
typedef struct {
    uint32_t speed_target;
    uint32_t acceleration;
} litexcnc_stepgen_instance_write_data_t;
#pragma pack(pop)
#define LITEXCNC_STEPGEN_INSTANCE_WRITE_DATA_SIZE sizeof(litexcnc_stepgen_instance_write_data_t)
#define LITEXCNC_BOARD_STEPGEN_DATA_WRITE_SIZE(litexcnc) ((litexcnc->stepgen.num_instances?sizeof(litexcnc_stepgen_general_write_data_t):0) + LITEXCNC_STEPGEN_INSTANCE_WRITE_DATA_SIZE*litexcnc->stepgen.num_instances)
// - read
#pragma pack(push,4)
typedef struct {
    int64_t position;
    uint32_t speed;
} litexcnc_stepgen_instance_read_data_t;
#pragma pack(pop)
#define LITEXCNC_BOARD_STEPGEN_DATA_READ_SIZE(litexcnc) litexcnc->stepgen.num_instances*sizeof(litexcnc_stepgen_instance_read_data_t)


// Functions for creating, reading and writing stepgen pins
int litexcnc_stepgen_init(litexcnc_t *litexcnc, cJSON *config);
uint8_t litexcnc_stepgen_config(litexcnc_t *litexcnc, uint8_t **data, long period);
uint8_t litexcnc_stepgen_prepare_write(litexcnc_t *litexcnc, uint8_t **data, long period);
uint8_t litexcnc_stepgen_process_read(litexcnc_t *litexcnc, uint8_t** data, long period);

#endif