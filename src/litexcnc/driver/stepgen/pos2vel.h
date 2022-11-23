/********************************************************************
* Description:  pos2vel.h
*               A simple component to convert position commands to speed
*               commands suitable for Litex-CNC. This component can also
*               be used for other applications as well. Please do so!
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
#ifndef __INCLUDE_POS2VEL_H__
#define __INCLUDE_POS2VEL_H__

static int comp_id;		/* component ID */

/* Create a an array containing the names all possible converters. The maximum number
   of converters is set arbitrarily to 16
*/
#define MAX_STEPGEN 16
char *names[MAX_STEPGEN] ={0,};

static int number;		             /* Number of stepgen */
static int default_num_stepgen = 3;  /* Default number of stepgen is 3 (XYZ) */ 


typedef struct {
    struct {

        struct {
            hal_float_t *position_feedback;   /* The current position, in length units (see parameter position-scale). The resolution of position-fb is much finer than a single step. If you need to see individual steps, use counts. */ 
            hal_float_t *position_cmd;        /* The commanded position, in length units (see parameter position-scale), at the start of the next position command execution */ 
            hal_float_t *velocity_feedback;   /* The current speed, in length units per second. */ 
            hal_float_t *velocity_cmd;        /* Commanded velocity, in length units per second (result of this component). */
            hal_bit_t *debug;                 /* Flag indicating whether all positional data will be printed to the command line */
        } pin;

        struct {
            hal_float_t max_acceleration;     /* The acceleration/deceleration limit, in length units per second squared. */ 
        } param;

    } hal;

    // This struct holds all old values (memoization) 
    struct {
        float position_cmd;
    } memo;

    // This struct contains data of a calculation
    struct {

    } data;
    
} hal_pos2vel_instance_t;


typedef struct {
    hal_pos2vel_instance_t *instances;

    struct {

        struct {
            hal_float_t *period_s;            /* The period of one cycle in seconds. When the period_s is zero, the value is automatically determined based on the period. */
            hal_float_t *period_s_recip;      /* The reciprocal of the cycle period. When the period_s_recip is zero, the value is automacally determined based on the period. */
        } pin;

        struct {

        } param;

    } hal;

    // This struct holds all old values (memoization) 
    struct {
        float period_s;
    } memo;

    // This struct contains data of a calculation
    struct {
        size_t num_instances;
        float period_s;
        float period_s_recip;
    } data;
    
} hal_pos2vel_t;
hal_pos2vel_t *pos2vel;

/***********************************************************************
*                   INTERNAL FUNCTIONS                                 *
************************************************************************/
static int export_converter_instance(hal_pos2vel_instance_t *instance, char *prefix);

/***********************************************************************
*                   EXPORTED FUNCTIONS                                 *
************************************************************************/
static void convert(void *arg, long period);

#endif
