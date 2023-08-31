/********************************************************************
* Description:  stepgen.c
*               A Litex-CNC component that converts a speed command
*               to steps made by the FPGA.
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
#include <inttypes.h>
#include <math.h>

#include "hal.h"
#include "rtapi.h"
#include "rtapi_app.h"
#include "rtapi_string.h"

#include "litexcnc_stepgen.h"

/** 
 * An array holding all instance for the module. As each boarf normally have a 
 * single instance of a type, this number coincides with the number of boards
 * which are supported by LitexCNC
 */
static litexcnc_stepgen_t *instances[MAX_INSTANCES];
static int num_instances = 0;

/**
 * Parameter which contains the registration of this module woth LitexCNC 
 */
static litexcnc_module_registration_t *registration;

int register_stepgen_module(void) {
    registration = (litexcnc_module_registration_t *)hal_malloc(sizeof(litexcnc_module_registration_t));
    registration->id = 0x73746570; /** The string `step` in hex */
    rtapi_snprintf(registration->name, sizeof(registration->name), "step");
    registration->initialize = &litexcnc_stepgen_init;
    registration->required_config_buffer = &required_config_buffer;
    registration->required_write_buffer  = &required_write_buffer;
    registration->required_read_buffer   = &required_read_buffer;
    return litexcnc_register_module(registration);
}
EXPORT_SYMBOL_GPL(register_stepgen_module);


int rtapi_app_main(void) {
    // Show some information on the module being loaded
    LITEXCNC_PRINT_NO_DEVICE(
        "Loading Litex Stepgen module driver version %u.%u.%u\n", 
        LITEXCNC_STEPGEN_VERSION_MAJOR, 
        LITEXCNC_STEPGEN_VERSION_MINOR, 
        LITEXCNC_STEPGEN_VERSION_PATCH
    );

    // Initialize the module
    comp_id = hal_init(LITEXCNC_STEPGEN_NAME);
    if(comp_id < 0) return comp_id;

    // Register the module with LitexCNC (NOTE: LitexCNC should be loaded first)
    int result = register_stepgen_module();
    if (result<0) return result;

    // Report GPIO is ready to be used
    hal_ready(comp_id);

    return 0;
}


void rtapi_app_exit(void) {
    hal_exit(comp_id);
    LITEXCNC_PRINT_NO_DEVICE("LitexCNC Stepgen module driver unloaded \n");
}


size_t required_config_buffer(void *module) {
    static litexcnc_stepgen_t *stepgen_module;
    stepgen_module = (litexcnc_stepgen_t *) module;
    // Safeguard for empty modules
    if (stepgen_module->num_instances == 0) {
        return 0;
    }
    return sizeof(litexcnc_stepgen_config_data_t);
}


size_t required_write_buffer(void *module) {
    static litexcnc_stepgen_t *stepgen_module;
    stepgen_module = (litexcnc_stepgen_t *) module;
    // Safeguard for empty modules
    if (stepgen_module->num_instances == 0) {
        return 0;
    }
    return sizeof(litexcnc_stepgen_general_write_data_t) + stepgen_module->num_instances * sizeof(litexcnc_stepgen_instance_write_data_t);
}


size_t required_read_buffer(void *module) {
    static litexcnc_stepgen_t *stepgen_module;
    stepgen_module = (litexcnc_stepgen_t *) module;
    return stepgen_module->num_instances * sizeof(litexcnc_stepgen_instance_read_data_t);
}


int litexcnc_stepgen_config(void *module, uint8_t **data, int period) {
    
    static litexcnc_stepgen_t *stepgen;
    stepgen = (litexcnc_stepgen_t *) module;

    // Initialize the averaging of the FPGA wall-clock. This is the first loop, so
    // the array is filled with 'ideal' data based on the period.
    stepgen->data.period_s = 1e-9 * period;
    stepgen->data.period_s_recip = 1.0f / stepgen->data.period_s;
    stepgen->data.cycles_per_period = stepgen->data.period_s * (*(stepgen->data.clock_frequency));

    // Set the pick-offs. At this moment it is fixed, but is easy to make it configurable
    int8_t shift = 0;
    while (*(stepgen->data.clock_frequency) / (1 << (shift + 1)) > stepgen->hal.param.max_driver_freq) {
        shift++;
    }
    stepgen->data.pick_off_pos = 32;
    stepgen->data.pick_off_vel = stepgen->data.pick_off_pos + shift;
    stepgen->data.pick_off_acc = stepgen->data.pick_off_vel + 8;
    stepgen->data.max_frequency = (float) *(stepgen->data.clock_frequency) / (1 << (shift + 1));

    // Timings
    // ===============
    // All stepgens will use the same values for steplen, dir_hold_time and dir_setup_time.
    // The maximum value is governing, so we start with the value 0. For each instance
    // it is checked whether the value has changed. If it has changed, the time is
    // converted to cycles.
    // NOTE: all timings are in nano-seconds (1E-9), so the timing is multiplied with
    // the clock-frequency and divided by 1E9. However, this might lead to issues
    // with roll-over of the 32-bit integer. 
    // TODO: Make the timings settings per stepgen unit
    litexcnc_stepgen_config_data_t config_data = {0};
    uint32_t stepspace_cycles = 0;
    uint32_t steplen_cycles = 0;
    uint32_t dirhold_cycles = 0;
    uint32_t dirsetup_cycles = 0;

    for (size_t i=0; i<stepgen->num_instances; i++) {
        // Get pointer to the stepgen instance
        litexcnc_stepgen_instance_t *instance = &(stepgen->instances[i]);

        // Calculate the timings
        // - steplen
        instance->data.steplen_cycles = ceil((float) instance->hal.param.steplen * (*(stepgen->data.clock_frequency)) * 1e-9);
        instance->memo.steplen = instance->hal.param.steplen; 
        if (instance->data.steplen_cycles > steplen_cycles) {steplen_cycles = instance->data.steplen_cycles;};
        // - stepspace
        instance->data.stepspace_cycles = ceil((float) instance->hal.param.stepspace * (*(stepgen->data.clock_frequency)) * 1e-9);
        instance->memo.stepspace = instance->hal.param.stepspace; 
        if (instance->data.stepspace_cycles > stepspace_cycles) {stepspace_cycles = instance->data.stepspace_cycles;};
        // - dir_hold_time
        instance->data.dirhold_cycles = ceil((float) instance->hal.param.dir_hold_time * (*(stepgen->data.clock_frequency)) * 1e-9);
        instance->memo.dir_hold_time = instance->hal.param.dir_hold_time; 
        if (instance->data.dirhold_cycles > dirhold_cycles) {dirhold_cycles = instance->data.dirhold_cycles;};
        // - dir_setup_time
        instance->data.dirsetup_cycles = ceil((float) instance->hal.param.dir_setup_time * (*(stepgen->data.clock_frequency)) * 1e-9);
        instance->memo.dir_setup_time = instance->hal.param.dir_setup_time; 
        if (instance->data.dirsetup_cycles > dirsetup_cycles) {dirsetup_cycles = instance->data.dirsetup_cycles;};
    }

    // Calculate the maximum frequency for stepgen (in if statement to prevent division)
    if ((stepgen->memo.stepspace_cycles != stepspace_cycles) || (stepgen->memo.steplen_cycles != steplen_cycles)) {
        stepgen->data.max_frequency = fmin(stepgen->data.max_frequency, (double) (*(stepgen->data.clock_frequency)) / (steplen_cycles + stepspace_cycles));
        stepgen->memo.steplen_cycles = steplen_cycles;
        stepgen->memo.stepspace_cycles = stepspace_cycles;
    }

    // Convert the general data to the correct byte order
    // - check whether the parameters fits in the space
    if (steplen_cycles >= 1 << 11) {
        LITEXCNC_ERR("Parameter `steplen` too large and is clipped. Consider lowering the frequency of the FPGA.\n", stepgen->data.fpga_name);
        steplen_cycles = (1 << 11) - 1;
    }
    if (dirhold_cycles >= 1 << 11) {
        LITEXCNC_ERR("Parameter `dir_hold_time` too large and is clipped. Consider lowering the frequency of the FPGA.\n", stepgen->data.fpga_name);
        dirhold_cycles = (1 << 11) - 1;
    }
    if (dirsetup_cycles >= 1 << 13) {
        LITEXCNC_ERR("Parameter `dir_setup_time` too large and is clipped. Consider lowering the frequency of the FPGA.\n", stepgen->data.fpga_name);
        dirsetup_cycles = (1 << 13) - 1;
    }
    // - convert the timings to the data to be sent to the FPGA
    config_data.timings = htobe32((steplen_cycles << 22) + (dirhold_cycles << 12) + (dirsetup_cycles << 0));

    // Put the data on the data-stream and advance the pointer
    memcpy(*data, &config_data, required_config_buffer(stepgen));
    *data += required_config_buffer(stepgen);

    return 0;
}


int litexcnc_stepgen_prepare_write(void *module, uint8_t **data, int period) {
    
    static litexcnc_stepgen_t *stepgen;
    stepgen = (litexcnc_stepgen_t *) module;

    // Declarations
    static litexcnc_stepgen_general_write_data_t data_general;
    static litexcnc_stepgen_instance_t *instance;
    static litexcnc_stepgen_instance_write_data_t instance_data;
    static float vel_cmd;
    static float match_time;
    static float avg_v;
    static float est_out;
    static float est_cmd;
    static float est_err;
    static float sign;
    static float dv;
    static float dp;

    // Check whether there are stepgen instances. If no instances, no need to write any
    // data (NOTE: when this guard is not in place, the apply_time would be written out
    // to the FPGA, which leads to a mismatch in data alignment)
    if (!(stepgen->num_instances)) {
        return 0;
    }

    // STEP 1: Timing
    // ==============
    // Put the data on the data-stream and advance the pointer
    data_general.apply_time = htobe64(stepgen->memo.apply_time);
    memcpy(*data, &data_general, sizeof(litexcnc_stepgen_general_write_data_t));
    *data += sizeof(litexcnc_stepgen_general_write_data_t);

    // STEP 2: Speed per stepgen
    // =========================
    for (size_t i=0; i<stepgen->num_instances; i++) {
        // Get pointer to the stepgen instance
        instance = &(stepgen->instances[i]);

        // Throw error when timings are changed
        // - steplen
        if (instance->hal.param.steplen != instance->memo.steplen) {
            LITEXCNC_ERR("Cannot change parameter `steplen` after configuration of the FPGA. Change is cancelled.\n", stepgen->data.fpga_name);
            instance->hal.param.steplen = instance->memo.steplen;
        }
        // - stepspace
        if (instance->hal.param.stepspace != instance->memo.stepspace) {
            LITEXCNC_ERR("Cannot change parameter `stepspace` after configuration of the FPGA. Change is cancelled.\n", stepgen->data.fpga_name);
            instance->hal.param.stepspace = instance->memo.stepspace;
        }
        // - dir_hold_time
        if (instance->hal.param.dir_hold_time != instance->memo.dir_hold_time) {
            LITEXCNC_ERR("Cannot change parameter `dir_hold_time` after configuration of the FPGA. Change is cancelled.\n", stepgen->data.fpga_name);
            instance->hal.param.dir_hold_time = instance->memo.dir_hold_time;
        }
        // - dir_setup_time
        if (instance->hal.param.dir_setup_time != instance->memo.dir_setup_time) {
            LITEXCNC_ERR("Cannot change parameter `dir_setup_time` after configuration of the FPGA. Change is cancelled.\n", stepgen->data.fpga_name);
            instance->hal.param.dir_setup_time = instance->memo.dir_setup_time;
        }

        // Recalculate the reciprocal of the position scale if it has changed
        if (instance->hal.param.position_scale != instance->memo.position_scale) {
            // Prevent division by zero
            if ((instance->hal.param.position_scale > -1e-20) && (instance->hal.param.position_scale < 1e-20)) {
                // Value too small, take a safe value
                instance->hal.param.position_scale = 1.0;
            }
            instance->data.scale_recip = 1.0 / instance->hal.param.position_scale;
            instance->memo.position_scale = instance->hal.param.position_scale; 
            // Calculate the scales for speed and acceleration
            instance->data.fpga_speed_scale = (float) (instance->hal.param.position_scale * (*(stepgen->data.clock_frequency_recip))) * (1LL << stepgen->data.pick_off_vel);
            instance->data.fpga_speed_scale_inv = (float) (*(stepgen->data.clock_frequency)) * instance->data.scale_recip / (1LL << stepgen->data.pick_off_vel);
            instance->data.fpga_acc_scale = (float) (instance->hal.param.position_scale * (*(stepgen->data.clock_frequency_recip)) * (*(stepgen->data.clock_frequency_recip))) * (1LL << (stepgen->data.pick_off_acc));
            instance->data.fpga_acc_scale_inv =  (float) instance->data.scale_recip * (*(stepgen->data.clock_frequency)) * (*(stepgen->data.clock_frequency)) / (1LL << stepgen->data.pick_off_acc);;
        }

        // Check the limits on the speed of the stepgen
        if (instance->hal.param.max_velocity <= 0.0) {
            // Negative speed is limited as zero
            instance->hal.param.max_velocity = 0.0;
        } else {
            // Maximum speed is positive and no zero, compare with maximum frequency
	        if (instance->hal.param.max_velocity > (stepgen->data.max_frequency * fabs(instance->hal.param.position_scale))) {
                // Limit speed to the maximum. This will lead to joint follow error when the higher speeds are commanded
                float max_speed_desired = instance->hal.param.max_velocity;
                instance->hal.param.max_velocity = stepgen->data.max_frequency * fabs(instance->hal.param.position_scale);
		        // Maximum speed is too high, complain about it and modify the value
		        if (!instance->memo.error_max_speed_printed) {
		            LITEXCNC_ERR_NO_DEVICE(
			            "STEPGEN: Channel %zu: The requested maximum velocity of %.2f units/sec is too high.\n",
			            i, 
                        max_speed_desired);
		            LITEXCNC_ERR_NO_DEVICE(
			            "STEPGEN: The maximum possible velocity is %.2f units/second\n",
			            instance->hal.param.max_velocity);
		            instance->memo.error_max_speed_printed = true;
		        }
	        }
        }

        // Determine the commanded speed
        if (*(instance->hal.pin.velocity_mode)) { 
            // When in velocity mode, use the commanded velocity directly
            vel_cmd = *(instance->hal.pin.velocity_cmd);
            // TODO: maybe create a 'artificial' memo value for the position, so the speeds
            // are consistent when changing from velocity mode to position mode.
        } else {
            // When not in velocity mode, convert the commanded position to a velocity
            /* Determine the velocity to go to the next point */ 
            vel_cmd = (*(instance->hal.pin.position_cmd) - instance->memo.position_cmd) * stepgen->data.period_s_recip;
            /* Determine how long the match would take and calc output position at the end of the match */
            match_time = fabs((vel_cmd - *(instance->hal.pin.speed_prediction)) / instance->hal.param.max_acceleration);
            avg_v = (vel_cmd + *(instance->hal.pin.speed_prediction)) * 0.5f;
            est_out = *(instance->hal.pin.position_prediction) + avg_v * match_time;
            /* Calculate the expected command position at that time */
            est_cmd = *(instance->hal.pin.position_cmd) + vel_cmd * (match_time - 1.5 * stepgen->data.period_s);
            est_err = est_out - est_cmd;
            /* Check whether the error can be compensated for or that we are at maximum acceleration */
            if (match_time < stepgen->data.period_s) {
                if (fabs(est_err) > 1e-6) {
                    /* Try to correct position error. Errors which are very small can eb accepted */
                    vel_cmd -= 0.5 * est_err * stepgen->data.period_s_recip;
                }
            } else {
                /* Determine which side we have to accelerate */
                sign = (vel_cmd > *(instance->hal.pin.speed_prediction)) ? 1.0f : -1.0f;
                dv = -2.0 * sign * instance->hal.param.max_acceleration * stepgen->data.period_s;
                dp = dv * match_time;
                /* Decide which way to ramp */
                if (fabs(est_err + dp * 2.0) < fabs(est_err)) {
                    sign = -sign;
                }
                /* Do it */
                vel_cmd = *(instance->hal.pin.speed_prediction) + sign * instance->hal.param.max_acceleration * stepgen->data.period_s;
            }
            // Store the results for the next step
            instance->memo.position_cmd = *(instance->hal.pin.position_cmd);
        } 

        // Limit the speed to the maximum speed (both phases)
        if (vel_cmd > instance->hal.param.max_velocity) {
            vel_cmd = instance->hal.param.max_velocity;
        } else if (vel_cmd < (-1 * instance->hal.param.max_velocity)) {
            vel_cmd = -1 * instance->hal.param.max_velocity;
        }

        // Limit the acceleration to the maximum acceleration (both phases). The acceleration
        // should be positive
        if (*(instance->hal.pin.acceleration_cmd) < 0) {
            *(instance->hal.pin.acceleration_cmd) = -1 * *(instance->hal.pin.acceleration_cmd);
        }
        if ((*(instance->hal.pin.acceleration_cmd) == 0) || (*(instance->hal.pin.acceleration_cmd) > instance->hal.param.max_acceleration)) {
            *(instance->hal.pin.acceleration_cmd) = instance->hal.param.max_acceleration;
        }

        // Check whether the acceleration command has changed
        if (*(instance->hal.pin.acceleration_cmd) != instance->memo.acceleration) {
            instance->memo.acceleration = *(instance->hal.pin.acceleration_cmd);
            instance->data.acceleration_recip = 1.0f / *(instance->hal.pin.acceleration_cmd);
        }

        // The data being send to the FPGA (as calculated) in units and seconds
        instance->data.flt_speed = vel_cmd;
        instance->data.flt_acc   = *(instance->hal.pin.acceleration_cmd);
        instance->data.flt_time  = fabs((vel_cmd - *(instance->hal.pin.speed_prediction)) * instance->data.acceleration_recip);

        // Calculate the time spent accelerating in steps and clock cycles
        instance->data.fpga_speed = (int64_t) (instance->data.flt_speed * instance->data.fpga_speed_scale) + 0x80000000;
        instance->data.fpga_acc = instance->data.flt_acc * instance->data.fpga_acc_scale;
        instance->data.fpga_time = instance->data.flt_time * (*(stepgen->data.clock_frequency));

        // Convert the integers used and scale it to the FPGA
        instance_data.speed_target = htobe32(instance->data.fpga_speed);
        instance_data.acceleration = htobe32(instance->data.fpga_acc);

        // Put the data on the data-stream and advance the pointer
        memcpy(*data, &instance_data, sizeof(litexcnc_stepgen_instance_write_data_t));
        *data += sizeof(litexcnc_stepgen_instance_write_data_t);

        if (*(instance->hal.pin.debug)) {
            LITEXCNC_PRINT_NO_DEVICE("Stepgen: data sent to FPGA %" PRIu64 ", %" PRIu64 ", %" PRIu32 ", %" PRIu32 ", %" PRIu32 "\n", 
                *(stepgen->data.wallclock_ticks),
                stepgen->memo.apply_time,
                instance->data.fpga_speed,
                instance->data.fpga_acc,
                instance->data.fpga_time
            );
        }
    }

    return 0;
}


int litexcnc_stepgen_process_read(void *module, uint8_t **data, int period) {
    
    static litexcnc_stepgen_t *stepgen;
    stepgen = (litexcnc_stepgen_t *) module;

    // Declarations
    static uint64_t next_apply_time;
    static int32_t loop_cycles;
    static litexcnc_stepgen_instance_t *instance;
    //  - parameters for retrieving data from FPGA
    static int64_t pos;
    static uint32_t speed;
    // - parameters for determining the position end start of next loop
    static uint64_t min_time;
    static uint64_t max_time;
    static float fraction;
    static float speed_end;

    // Check for the first cycle and calculate some fake timings. This has to be done at
    // this location, because in the init the wallclock_ticks is still zero and this would
    // lead to an underflow.
    if (stepgen->memo.apply_time == 0) {
        stepgen->memo.apply_time = - 0.1 * stepgen->data.cycles_per_period + *(stepgen->data.wallclock_ticks) ;
    }

    // The next apply time is basically chosen so that the next loop starts exactly when it
    // should (according to the timing of the previous loop).
    next_apply_time = 0.75 * stepgen->data.cycles_per_period + *(stepgen->data.wallclock_ticks) ;

    // Receive and process the data for all the stepgens
    for (size_t i=0; i<stepgen->num_instances; i++) {
        // Get pointer to the stepgen instance
        instance = &(stepgen->instances[i]);

        // Recalculate the reciprocal of the position scale if it has changed
        if (instance->hal.param.position_scale != instance->memo.position_scale) {
            // Prevent division by zero
            if ((instance->hal.param.position_scale > -1e-20) && (instance->hal.param.position_scale < 1e-20)) {
		        // Value too small, take a safe value
		        instance->hal.param.position_scale = 1.0;
	        }
            instance->data.scale_recip = 1.0 / instance->hal.param.position_scale;
            instance->memo.position_scale = instance->hal.param.position_scale; 
            // Calculate the scales for speed and acceleration
            instance->data.fpga_pos_scale_inv = (float) instance->data.scale_recip / (1LL << stepgen->data.pick_off_pos);
            instance->data.fpga_speed_scale = (float) (instance->hal.param.position_scale * (*(stepgen->data.clock_frequency_recip))) * (1LL << stepgen->data.pick_off_vel);
            instance->data.fpga_speed_scale_inv = 1.0f / instance->data.fpga_speed_scale;
            instance->data.fpga_acc_scale = (float) (instance->hal.param.position_scale * (*(stepgen->data.clock_frequency_recip)) * (*(stepgen->data.clock_frequency_recip))) * (1LL << (stepgen->data.pick_off_acc));
            instance->data.fpga_acc_scale_inv =  (float) instance->data.scale_recip * (*(stepgen->data.clock_frequency)) * (*(stepgen->data.clock_frequency)) / (1LL << stepgen->data.pick_off_acc);;
        }

        // Store the old data
        instance->memo.position = instance->data.position;
        // Read data and proceed the buffer
        memcpy(&pos, *data, sizeof pos);
        instance->data.position = be64toh(pos);
        *data += 8;  // The data read is 64 bit-wide. The buffer is 8-bit wide
        memcpy(&speed, *data, sizeof speed);
        instance->data.speed = (int64_t) be32toh(speed) -  0x80000000;
        *data += 4;  // The data read is 32 bit-wide. The buffer is 8-bit wide
        // Convert the received position to HAL pins for counts and floating-point position
        *(instance->hal.pin.counts) = instance->data.position >> stepgen->data.pick_off_pos;
        // Check: why is a half step subtracted from the position. Will case a possible problem 
        // when the power is cycled -> will lead to a moving reference frame  
        // *(instance->hal.pin.position_fb) = (double)(instance->data.position-(1LL<<(stepgen->data.pick_off_pos-1))) * instance->data.scale_recip / (1LL << stepgen->data.pick_off_pos);
        *(instance->hal.pin.position_fb) = (double) instance->data.position * instance->data.fpga_pos_scale_inv;
        *(instance->hal.pin.speed_fb) = (double) instance->data.speed * instance->data.fpga_speed_scale_inv;

        /* -------------------
         * Predict the position and speed at the theoretical end of the start of the 
         * update period. The prediction is based on:
         *    - if there is a pending apply time (apply_time > wall_clock) the movement until that
         *      apply time based on the position, speed and acceleration as read from the FPGA.
         *    - any movement (with respect to speed and acceleration) which happens until the next
         *      apply time, which is typically equal to the period of the function.
         *
         * This function is placed under read, as it uses the output from the previous cycle. If this
         * was to be placed under the write cycle, errors might occur if the input variables such
         * as the acceleration would change between read and write.
         * ------------------- 
         */
        // - start with the current speed and position
        *(instance->hal.pin.speed_prediction) = *(instance->hal.pin.speed_fb);
        *(instance->hal.pin.position_prediction) =  *(instance->hal.pin.position_fb);
        
        // Add the different phases to the speed and position prediction
        if (*(instance->hal.pin.debug)) {
            rtapi_print("Timings: %.6f, %" PRIu64 ", %" PRIu64 ", %" PRIu32 ", %" PRIu64 "\n",
                stepgen->data.period_s,
                *(stepgen->data.wallclock_ticks),
                stepgen->memo.apply_time,
                instance->data.fpga_time,
                next_apply_time
            );
        }
        if (*(stepgen->data.wallclock_ticks) <= stepgen->memo.apply_time + instance->data.fpga_time) {
            min_time = *(stepgen->data.wallclock_ticks);
            if (stepgen->memo.apply_time > min_time) {
                min_time = stepgen->memo.apply_time;
            }
            max_time = stepgen->memo.apply_time + instance->data.fpga_time;
            if (next_apply_time < max_time) {
                max_time = next_apply_time;
            }
            if ((stepgen->memo.apply_time + instance->data.fpga_time - min_time) <= 0) {
                fraction = 1.0;
            } else {
                fraction = (float) (max_time - min_time) / (stepgen->memo.apply_time + instance->data.fpga_time - min_time);
            }
            speed_end = (1.0 - fraction) * *(instance->hal.pin.speed_prediction) + fraction * instance->data.flt_speed;
            *(instance->hal.pin.position_prediction) += 0.5 * (*(instance->hal.pin.speed_prediction) + speed_end) * (max_time - min_time) * (*(stepgen->data.clock_frequency_recip));
            *(instance->hal.pin.speed_prediction) = speed_end;
        }
        if (next_apply_time > stepgen->memo.apply_time + instance->data.fpga_time) {
            // Some constant speed should be added
            *(instance->hal.pin.speed_prediction) = instance->data.flt_speed;
            *(instance->hal.pin.position_prediction) += instance->data.flt_speed * (next_apply_time - (stepgen->memo.apply_time + instance->data.fpga_time)) * (*(stepgen->data.clock_frequency_recip));
        }
        if (*(instance->hal.pin.debug)) {
            rtapi_print("Stepgen speed feedback result: %" PRIu64 ", %" PRIu64 ", %.6f, %.6f, %.6f, %.6f \n",
                *(stepgen->data.wallclock_ticks),
                next_apply_time,
                *(instance->hal.pin.speed_fb),
                *(instance->hal.pin.speed_prediction),
                *(instance->hal.pin.position_fb),
                *(instance->hal.pin.position_prediction)
            );
        }
    }

    // Push the apply for the write loop
    stepgen->memo.apply_time = next_apply_time;

    return 0;
}


size_t litexcnc_stepgen_init(litexcnc_module_instance_t **module, litexcnc_t *litexcnc, uint8_t **config) {

    int r;
    char base_name[HAL_NAME_LEN + 1];   // i.e. <board_name>.<board_index>.pwm.<pwm_index>
    char name[HAL_NAME_LEN + 1];        // i.e. <base_name>.<pin_name>

    // Create structure in memory
    (*module) = (litexcnc_module_instance_t *)hal_malloc(sizeof(litexcnc_module_instance_t));
    (*module)->prepare_write    = &litexcnc_stepgen_prepare_write;
    (*module)->process_read     = &litexcnc_stepgen_process_read;
    (*module)->configure_module = &litexcnc_stepgen_config;
    (*module)->instance_data = hal_malloc(sizeof(litexcnc_stepgen_t));
        
    // Cast from void to correct type and store it
    litexcnc_stepgen_t *stepgen = (litexcnc_stepgen_t *) (*module)->instance_data;
    instances[num_instances] = stepgen;
    num_instances++;

    // Store pointers to data from FPGA required by the process
    stepgen->data.fpga_name = litexcnc->fpga->name;
    stepgen->data.clock_frequency = &(litexcnc->clock_frequency);
    stepgen->data.clock_frequency_recip = &(litexcnc->clock_frequency_recip);
    stepgen->data.wallclock_ticks = &(litexcnc->wallclock->memo.wallclock_ticks);

    // Create shared HAL pins
    rtapi_snprintf(base_name, sizeof(base_name), "%s.stepgen", litexcnc->fpga->name);
    // NOTE: This parameter is disabled at this moment, because the pick-off is determined
    // in the formware based on a fixed value of 400 kHz and this information is not being
    // relayed back to the driver. This parameter will be REMOVED in a later stage. The
    // pick-off will be sent as config data from the FPGA to LinuxCNC. 
    //LITEXCNC_CREATE_HAL_PARAM("max-driver-freq", float, HAL_RW, &(stepgen->hal.param.max_driver_freq));
    stepgen->hal.param.max_driver_freq = 400e3;

    // Store the amount of stepgen instances on this board and allocate HAL shared memory
    stepgen->num_instances = be32toh(*(uint32_t*)*config);
    stepgen->instances = (litexcnc_stepgen_instance_t *)hal_malloc(stepgen->num_instances * sizeof(litexcnc_stepgen_instance_t));
    if (stepgen->instances == NULL) {
        LITEXCNC_ERR_NO_DEVICE("Out of memory!\n");
        return -ENOMEM;
    }
    (*config) += 4;

    // Create the pins and params in the HAL
    for (size_t i=0; i<stepgen->num_instances; i++) {
        litexcnc_stepgen_instance_t *instance = &(stepgen->instances[i]);
        
        // Create the basename
        LITEXCNC_CREATE_BASENAME("stepgen", i);

        // Create the params
        LITEXCNC_CREATE_HAL_PARAM("frequency", float, HAL_RO, &(instance->hal.param.frequency));
        LITEXCNC_CREATE_HAL_PARAM("max-acceleration", float, HAL_RW, &(instance->hal.param.max_acceleration));
        LITEXCNC_CREATE_HAL_PARAM("max-velocity", float, HAL_RW, &(instance->hal.param.max_velocity));
        LITEXCNC_CREATE_HAL_PARAM("position-scale", float, HAL_RW, &(instance->hal.param.position_scale));
        LITEXCNC_CREATE_HAL_PARAM("steplen", u32, HAL_RW, &(instance->hal.param.steplen));
        LITEXCNC_CREATE_HAL_PARAM("stepspace", u32, HAL_RW, &(instance->hal.param.stepspace));
        LITEXCNC_CREATE_HAL_PARAM("dir-setup-time", u32, HAL_RW, &(instance->hal.param.dir_setup_time));
        LITEXCNC_CREATE_HAL_PARAM("dir-hold-time", u32, HAL_RW, &(instance->hal.param.dir_hold_time));

        // Create the pins
        LITEXCNC_CREATE_HAL_PIN("counts", u32, HAL_OUT, &(instance->hal.pin.counts));
        LITEXCNC_CREATE_HAL_PIN("position-feedback", float, HAL_OUT, &(instance->hal.pin.position_fb));
        LITEXCNC_CREATE_HAL_PIN("position-prediction", float, HAL_OUT, &(instance->hal.pin.position_prediction));
        LITEXCNC_CREATE_HAL_PIN("velocity-feedback", float, HAL_OUT, &(instance->hal.pin.speed_fb));
        LITEXCNC_CREATE_HAL_PIN("velocity-prediction", float, HAL_OUT, &(instance->hal.pin.speed_prediction));
        LITEXCNC_CREATE_HAL_PIN("enable", bit, HAL_IN, &(instance->hal.pin.enable));
        LITEXCNC_CREATE_HAL_PIN("velocity-mode", bit, HAL_IN, &(instance->hal.pin.velocity_mode));
        LITEXCNC_CREATE_HAL_PIN("position-cmd", float, HAL_IN, &(instance->hal.pin.position_cmd));
        LITEXCNC_CREATE_HAL_PIN("velocity-cmd", float, HAL_IN, &(instance->hal.pin.velocity_cmd));
        LITEXCNC_CREATE_HAL_PIN("acceleration-cmd", float, HAL_IN, &(instance->hal.pin.acceleration_cmd));
        LITEXCNC_CREATE_HAL_PIN("debug", bit, HAL_IN, &(instance->hal.pin.debug));
    }

    return 0;
}
