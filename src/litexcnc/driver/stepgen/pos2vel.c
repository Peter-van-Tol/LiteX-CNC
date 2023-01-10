/********************************************************************
* Description:  pos2vel.c
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

/** This component is designed to convert the position commands from
    motmod to speed commands which can be used by Litex-CNC. It is
    basically the same function as in `stepgen`-component of LinuxCNC,
    where the following improvements are made:
    - this component works completely in units and seconds. The LinuxCNC
      works with pulses. However this does not improve functionality
      because all counters are floats.
    - some functions for determining the estimated error have been improved,
      see the commands in that section.

    The result of this component is the HAL-pin `velocity_cmd`. This pin
    can be connected to two relevant inputs of LitexCNC stepgen `velocity_cmd1`
    and `velocity_cmd2`, as this component only uses one phases.
*/
#include "rtapi.h"         /* RTAPI realtime OS API */
#include "rtapi_app.h"     /* RTAPI realtime module decls */
#include "rtapi_string.h"
#include "rtapi_math.h"
#include "hal.h"	       /* HAL public API decls */

#include "pos2vel.h"

/* Module information */
MODULE_AUTHOR("Peter van Tol");
MODULE_DESCRIPTION("A simple component to convert position commands to speed commands suitable for Litex-CNC");
MODULE_LICENSE("GPL");
RTAPI_MP_INT(number, "Number of position converters");
RTAPI_MP_ARRAY_STRING(names, MAX_STEPGEN,"Position converter names");


int rtapi_app_main(void) {
    int retval, i;

    // Prevent user from setting both number and names
    if(number && names[0]) {
        rtapi_print_msg(RTAPI_MSG_ERR,"number= and names= are mutually exclusive\n");
        return -EINVAL;
    }

    // When user does not supply number or names, use the default number of converters
    if (!number && !names[0]) number = default_num_stepgen;

    // Convert the names to a number and vice-versa
    if (!number) {
        number = 0;
        for (i = 0; i < MAX_STEPGEN; i++) {
            if ( (names[i] == NULL) || (*names[i] == 0) ){
                break;
            }
            number = i + 1;
        }
    }

    // Test for number of converters
    if ((number <= 0) || (number > MAX_STEPGEN)) {
        rtapi_print_msg(RTAPI_MSG_ERR,
            "POS2VEL: ERROR: invalid number of converters: %d\n", number);
        return -1;
    }

    // Now have good config info, connect to the HAL
    comp_id = hal_init("pos2vel");
    if (comp_id < 0) {
	    rtapi_print_msg(RTAPI_MSG_ERR, "POS2VEL: ERROR: hal_init() failed\n");
	    return -1;
    }

    // Create memory allocation
    // - base
    pos2vel = hal_malloc(sizeof(hal_pos2vel_t));
    if (pos2vel == 0) {
        rtapi_print_msg(RTAPI_MSG_ERR, "POS2VEL: ERROR: hal_malloc() failed\n");
        hal_exit(comp_id);
        return -1;
    }
    // - instances
    pos2vel->data.num_instances = number;
    pos2vel->instances = (hal_pos2vel_instance_t *)hal_malloc(pos2vel->data.num_instances * sizeof(hal_pos2vel_instance_t));
    if (pos2vel->instances == 0) {
        rtapi_print_msg(RTAPI_MSG_ERR, "POS2VEL: ERROR: hal_malloc() failed\n");
        hal_exit(comp_id);
        return -1;
    }

    // Export the pins and functions
    // - base
    retval = hal_pin_float_new("pos2vel.period_s", HAL_IN, &(pos2vel->hal.pin.period_s), comp_id);
    if (retval != 0) {
        rtapi_print_msg(RTAPI_MSG_ERR, "POS2VEL: ERROR: converter export pin `period_s` failed\n");
        hal_exit(comp_id);
	    return retval;
    }
    retval = hal_pin_float_new("pos2vel.period_s_recip", HAL_IN, &(pos2vel->hal.pin.period_s_recip), comp_id);
    if (retval != 0) {
        rtapi_print_msg(RTAPI_MSG_ERR, "POS2VEL: ERROR: converter export pin `period_s_recip` failed\n");
        hal_exit(comp_id);
	    return retval;
    }
    // - instances
    for (i=0; i<pos2vel->data.num_instances; i++) {
        // Get pointer to the stepgen instance
        hal_pos2vel_instance_t *instance = &(pos2vel->instances[i]);
        // Export the pins
        char buf[HAL_NAME_LEN + 1];
        if(names[0]) {
            // Using the name of the converter
            rtapi_snprintf(buf, sizeof(buf), "pos2vel.%s", names[i]);
        } else {
            // Convert the number of the converter to a string
            rtapi_snprintf(buf, sizeof(buf), "pos2vel.%d", i);
        }
	    retval = export_converter_instance(instance, buf); 
        // Check whether successful. If not, bail out.
        if (retval != 0) {
            rtapi_print_msg(RTAPI_MSG_ERR, "POS2VEL: ERROR: converter `%s` var export failed\n", buf);
            hal_exit(comp_id);
            return -1;
        }
    }
    // - function
    retval = hal_export_funct("pos2vel.convert", convert, pos2vel, 1, 0, comp_id);

    // Report ready
    rtapi_print_msg(RTAPI_MSG_INFO, "PID: installed %zu pos2vel converters\n", pos2vel->data.num_instances);
    hal_ready(comp_id);
    return 0;
}

void rtapi_app_exit(void) {
    hal_exit(comp_id);
}

static int export_converter_instance(hal_pos2vel_instance_t *instance, char *prefix) {
    int retval;

    // - hal pins
    retval = hal_pin_float_newf(HAL_IN, &(instance->hal.pin.position_feedback), comp_id, "%s.position-feedback", prefix);
    if (retval != 0) {
	    return retval;
    }
    retval = hal_pin_float_newf(HAL_IN, &(instance->hal.pin.position_cmd), comp_id, "%s.position-cmd", prefix);
    if (retval != 0) {
	    return retval;
    }
    retval = hal_pin_float_newf(HAL_IN, &(instance->hal.pin.velocity_feedback), comp_id, "%s.velocity-feedback", prefix);
    if (retval != 0) {
	    return retval;
    }
    retval = hal_pin_float_newf(HAL_OUT, &(instance->hal.pin.velocity_cmd), comp_id, "%s.velocity-cmd", prefix);
    if (retval != 0) {
	    return retval;
    }
    retval = hal_pin_bit_newf(HAL_IN, &(instance->hal.pin.debug), comp_id, "%s.debug", prefix);
    if (retval != 0) {
	    return retval;
    }

    // hal - param
    retval = hal_param_float_newf(HAL_RW, &(instance->hal.param.max_acceleration), comp_id, "%s.max-acceleration", prefix);
    if (retval != 0) {
	    return retval;
    }

    return 0;
}

static void convert(void *void_pos2vel, long period) {
    hal_pos2vel_t *pos2vel = void_pos2vel;
    static float vel_cmd;
    static float match_time;
    static float avg_v;
    static float est_out;
    static float est_cmd;
    static float est_err;
    static float sign;
    static float dv;
    static float dp;

    // Check we need to use default value for period_s
    if (*(pos2vel->hal.pin.period_s) == 0) {
        pos2vel->data.period_s = period * 1e-9;
    } else {
        pos2vel->data.period_s = *(pos2vel->hal.pin.period_s);
    }

    // Check whether we need to calculate the reciprocal of the period
    if (*(pos2vel->hal.pin.period_s_recip) == 0) {
        if (pos2vel->data.period_s != pos2vel->memo.period_s) {
            pos2vel->data.period_s_recip = 1.0f / pos2vel->data.period_s;
            pos2vel->memo.period_s = pos2vel->data.period_s;
        }
    } else {
        pos2vel->data.period_s_recip = *(pos2vel->hal.pin.period_s_recip);
    }

    // Run conversion for all the instances
    for (size_t i=0; i<pos2vel->data.num_instances; i++) {
        hal_pos2vel_instance_t *instance = &(pos2vel->instances[i]);

        /* Determine the velocity to go to the next point */ 
        vel_cmd = (*(instance->hal.pin.position_cmd) - instance->memo.position_cmd) * pos2vel->data.period_s_recip;

        /* Determine how long the match would take and calc output position at the end of the match */
        match_time = fabs((vel_cmd - *(instance->hal.pin.velocity_feedback)) / instance->hal.param.max_acceleration);
        avg_v = (vel_cmd + *(instance->hal.pin.velocity_feedback)) * 0.5f;
        est_out = *(instance->hal.pin.position_feedback) + avg_v * match_time;

        /* Calculate the expected command position at that time */
        /* Original code had a factor of 1.5 in the equation, don't know why, but causes
            the movement to lag a bit. Replaced with a factor of 1.0 to see whether this
            improves settling down on the spot. 
            est_cmd = pos_cmd + vel_cmd * (match_time - 1.5 * pos2vel->data.period_s);
        */
        est_cmd = *(instance->hal.pin.position_cmd) + vel_cmd * (match_time - 1.0f * pos2vel->data.period_s);
        est_err = est_out - est_cmd;

        /* Determine whether the velocity can be mathced within one period or not */
        if (match_time < pos2vel->data.period_s) {
            /* We can match velocity in one period */
            if (fabs(est_err) < 1e-6) {
                /* after match the position error will be acceptable */
                /* so we just do the velocity match */
                *(instance->hal.pin.velocity_cmd) = vel_cmd;
            } else {
                /* Try to correct position error. The match-time is re-calculated by
                   LitexCNC, so no need to re-calculate it here.
                     NOTE:  acceleration and velocity limits are applied by LitexCNC stepgen.

                   new_vel = vel_cmd - 0.5 * est_err * period_s_recip; <= Original LinuxCNC code
                */
                *(instance->hal.pin.velocity_cmd) = vel_cmd - est_err / (pos2vel->data.period_s - match_time);
            }
        } else {
            sign = (vel_cmd > *(instance->hal.pin.velocity_feedback)) ? 1.0f : -1.0f;
            /* calculate change in final position if we ramp in the
            opposite direction for one period */
            dv = -2.0 * sign * instance->hal.param.max_acceleration * pos2vel->data.period_s;
            dp = dv * match_time;
            /* decide which way to ramp */
            if (fabs(est_err + dp * 2.0) < fabs(est_err)) {
                sign = -sign;
            }
            /* and do it */
            *(instance->hal.pin.velocity_cmd) = *(instance->hal.pin.velocity_feedback) + sign * instance->hal.param.max_acceleration * pos2vel->data.period_s;
        }

        // Print out debug information when requested
        if (*(instance->hal.pin.debug)) {
            rtapi_print("POS2VEL %.6f, %.6f, %.6f, %.6f, %.6f \n",
                *(instance->hal.pin.position_feedback),
                *(instance->hal.pin.position_cmd),
                *(instance->hal.pin.velocity_feedback),
                *(instance->hal.pin.velocity_cmd),
                est_err
            );
        }

        // Store the position command for next loop
        instance->memo.position_cmd = *(instance->hal.pin.position_cmd);
    }
}
