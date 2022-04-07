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

#ifndef __INCLUDE_LITEXCNC_PWM_H__
#define __INCLUDE_LITEXCNC_PWM_H__

#include "litexcnc.h"

// Defines the structure of the PWM instance
typedef struct {
    struct {

        struct {
            hal_bit_t *enable;          /* pin for enable signal */
            hal_float_t *value;		    /* command value */
            hal_float_t *scale;		    /* pin: scaling from value to duty cycle */
            hal_float_t *offset;        /* pin: offset: this is added to duty cycle */
            hal_bit_t *dither_pwm;	    /* 0 = pure PWM, 1 = dithered PWM */
            hal_float_t *pwm_freq;      /* pin: PWM frequency */
            hal_float_t *min_dc;	    /* pin: minimum duty cycle */
            hal_float_t *max_dc;	    /* pin: maximum duty cycle */
            hal_float_t *curr_dc;	    /* pin: current duty cycle */
            hal_float_t *curr_pwm_freq; /* pin: current pwm frequency */
            hal_u32_t *curr_period;     /* The current PWM period in clock-cycles*/ 
            hal_u32_t *curr_width;      /* The current PWM width in clock-cycles*/ 
        } pin;

        struct {
            hal_float_t scale_recip;    /* Reciprocal of the scale, not exported */
            hal_float_t period_recip;   /* Reciprocal of the width, not exported */
        } param;

    } hal;

    // This struct holds all old values (memoization) 
    struct {
        double scale;
        double pwm_freq;
    } memo;
    
} litexcnc_pwm_pin_t;


// Defines the PWM, contains a collection of PWM instances
typedef struct {
    // Input pins
    int num_instances;
    litexcnc_pwm_pin_t *instances;
    

} litexcnc_pwm_t;

// Defines the data-package for sending the settings for a single PWM generator. The
// order of this package MUST coincide with the order in the MMIO definition.
typedef struct {
    // Input pins
    uint32_t enable;
    uint32_t period;
    uint32_t width;
} litexcnc_pwm_data_t;
#define LITEXCNC_PWM_DATA_SIZE sizeof(litexcnc_pwm_data_t)
#define LITEXCNC_BOARD_PWM_DATA_WRITE_SIZE(litexcnc) LITEXCNC_PWM_DATA_SIZE*litexcnc->pwm.num_instances
#define LITEXCNC_BOARD_PWM_DATA_READ_SIZE(litexcnc) 0 // PWM does not send data back

// Functions for creating, reading and writing PWM pins
int litexcnc_pwm_init(litexcnc_t *litexcnc, json_object *config);
uint8_t litexcnc_pwm_prepare_write(litexcnc_t *litexcnc, uint8_t **data);
uint8_t litexcnc_pwm_process_read(litexcnc_t *litexcnc, uint8_t** data);

#endif