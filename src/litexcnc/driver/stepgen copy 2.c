


#include "litexcnc.h"
#include "stepgen.h"

// Declaration of parameters used in litexcnc_stepgen_prepare_write
double max_speed_desired;
double speed_new;
double speed_avg;
double match_time;
double pos_avg;
double pos_error;

// double speed_new;



int litexcnc_stepgen_init(litexcnc_t *litexcnc, json_object *config) {

    // Declarations
    int r = 0;
    struct json_object *stepgen_config;
    struct json_object *instance_config;
    struct json_object *instance_config_name_field;
    char base_name[HAL_NAME_LEN + 1];   // i.e. <board_name>.<board_index>.stepgen.<stepgen_name>
    
    // Parse the contents of the config-json
    if (json_object_object_get_ex(config, "stepgen", &stepgen_config)) {
        // Store the amount of stepgen instances on this board
        litexcnc->stepgen.num_instances = json_object_array_length(stepgen_config);
        litexcnc->config.num_stepgen_instances = litexcnc->stepgen.num_instances;
        // Allocate the module-global HAL shared memory
        litexcnc->stepgen.instances = (litexcnc_stepgen_pin_t *)hal_malloc(litexcnc->stepgen.num_instances * sizeof(litexcnc_stepgen_pin_t));
        if (litexcnc->stepgen.instances == NULL) {
            LITEXCNC_ERR_NO_DEVICE("Out of memory!\n");
            r = -ENOMEM;
            return r;
        }
        // Create the pins and params in the HAL
        for (size_t i=0; i<litexcnc->stepgen.num_instances; i++) {
            instance_config = json_object_array_get_idx(stepgen_config, i);
            
            // Get pointer to the stepgen instance
            litexcnc_stepgen_pin_t *instance = &(litexcnc->stepgen.instances[i]);

            // Set the pick-offs. At this moment it is fixed, but is easy to make it configurable
            instance->data.pick_off_pos = 32;
            instance->data.pick_off_vel = 40;
            instance->data.pick_off_acc = 48;

            // Create the basename
            if (json_object_object_get_ex(instance_config, "name", &instance_config_name_field)) {
                rtapi_snprintf(base_name, sizeof(base_name), "%s.stepgen.%s", litexcnc->fpga->name, json_object_get_string(instance_config_name_field));
            } else {
                rtapi_snprintf(base_name, sizeof(base_name), "%s.stepgen.%02d", litexcnc->fpga->name, i);
            }

            // Create the params
            // - Frequency
            r = hal_param_float_newf(HAL_RO, &(instance->hal.param.frequency), litexcnc->fpga->comp_id, "%s.frequency", base_name);
            if (r < 0) { goto fail_params; }
            // - Maximum acceleration
            r = hal_param_float_newf(HAL_RW, &(instance->hal.param.maxaccel), litexcnc->fpga->comp_id, "%s.maxaccel", base_name);
            if (r != 0) { goto fail_params; }
            // - Maximum velocity
            r = hal_param_float_newf(HAL_RW, &(instance->hal.param.maxvel), litexcnc->fpga->comp_id, "%s.maxvel", base_name);
            if (r != 0) { goto fail_params; }
            // - Scale of position
            r = hal_param_float_newf(HAL_RW, &(instance->hal.param.position_scale), litexcnc->fpga->comp_id, "%s.position-scale", base_name);
            if (r != 0) { goto fail_params; }
            // - Timing - steplen
            r = hal_param_u32_newf(HAL_RW, &(instance->hal.param.steplen), litexcnc->fpga->comp_id, "%s.steplen", base_name);
            if (r != 0) { goto fail_params; }
            // - Timing - stepspace
            r = hal_param_u32_newf(HAL_RW, &(instance->hal.param.stepspace), litexcnc->fpga->comp_id, "%s.stepspace", base_name);
            if (r != 0) { goto fail_params; }
            // - Timing - dirsetup
            r = hal_param_u32_newf(HAL_RW, &(instance->hal.param.dir_setup_time), litexcnc->fpga->comp_id, "%s.dir-setup-time", base_name);
            if (r != 0) { goto fail_params; }
            // - Timing - dirhold
            r = hal_param_u32_newf(HAL_RW, &(instance->hal.param.dir_hold_time), litexcnc->fpga->comp_id, "%s.dir-hold-time", base_name);
            if (r != 0) { goto fail_params; }
            // - Position mode (including setting the default mode, )
            r = hal_param_bit_newf(HAL_RW, &(instance->hal.param.position_mode), litexcnc->fpga->comp_id, "%s.position-mode", base_name);
            if (r != 0) { goto fail_params; }
            instance->hal.param.position_mode = 1;
            // - Position mode (including setting the default mode, )
            r = hal_pin_bit_newf(HAL_IN, &(instance->hal.pin.debug), litexcnc->fpga->comp_id, "%s.debug", base_name);
            if (r != 0) { goto fail_params; }

            // Create the pins
            // - counts
            r = hal_pin_u32_newf(HAL_OUT, &(instance->hal.pin.counts), litexcnc->fpga->comp_id, "%s.counts", base_name);
            if (r != 0) { goto fail_pins; }
            // - position_fb
            r = hal_pin_float_newf(HAL_OUT, &(instance->hal.pin.position_fb), litexcnc->fpga->comp_id, "%s.position-feedback", base_name);
            if (r != 0) { goto fail_pins; }
            // - position_prediction
            r = hal_pin_float_newf(HAL_OUT, &(instance->hal.pin.position_prediction), litexcnc->fpga->comp_id, "%s.position_prediction", base_name);
            if (r != 0) { goto fail_pins; }
            // - speed_fb
            r = hal_pin_float_newf(HAL_OUT, &(instance->hal.pin.speed_fb), litexcnc->fpga->comp_id, "%s.speed-feedback", base_name);
            if (r != 0) { goto fail_pins; }
            // - speed_prediction
            r = hal_pin_float_newf(HAL_OUT, &(instance->hal.pin.speed_prediction), litexcnc->fpga->comp_id, "%s.speed-prediction", base_name);
            if (r != 0) { goto fail_pins; }
            // - enable
            r = hal_pin_bit_newf(HAL_IN, &(instance->hal.pin.enable), litexcnc->fpga->comp_id, "%s.enable", base_name);
            if (r != 0) { goto fail_pins; }
            // - velocity_cmd
            r = hal_pin_float_newf(HAL_IN, &(instance->hal.pin.velocity_cmd), litexcnc->fpga->comp_id, "%s.velocity-cmd", base_name);
            if (r != 0) { goto fail_pins; }
            // - position_cmd
            r = hal_pin_float_newf(HAL_IN, &(instance->hal.pin.position_cmd), litexcnc->fpga->comp_id, "%s.position-cmd", base_name);
            if (r != 0) { goto fail_pins; }
        }
    }

    // Pre-calculate the amount of cycles per nanp-second. Required to convert the 
    // timings to amount of clock-cycles
    litexcnc->stepgen.data.cycles_per_ns = litexcnc->clock_frequency * 1E-9; 

    return r;

    
fail_params:
    if (r < 0) {
        LITEXCNC_ERR_NO_DEVICE("Error adding stepgen params, aborting\n");
        return r;
    }

fail_pins:
    if (r < 0) {
        LITEXCNC_ERR_NO_DEVICE("Error adding stepgen pins, aborting\n");
        return r;
    }

}

uint8_t litexcnc_stepgen_config(litexcnc_t *litexcnc, uint8_t **data, long period) {

    // Calculate the reciprocal once here, to avoid multiple divides later
    litexcnc->stepgen.data.recip_dt = 1.0 / (period * 0.000000001);
    litexcnc->stepgen.memo.period = period;

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
    litexcnc_stepgen_config_data_t config_data = {0,0,0};
    uint32_t stepspace_cycles = 0; // Separate variable, as it is not part of the data-package

    for (size_t i=0; i<litexcnc->stepgen.num_instances; i++) {
        // Get pointer to the stepgen instance
        litexcnc_stepgen_pin_t *instance = &(litexcnc->stepgen.instances[i]);

        // Calculate the timings
        // - steplen
        instance->data.steplen_cycles = instance->hal.param.steplen * litexcnc->stepgen.data.cycles_per_ns + 0.5;
        instance->memo.steplen = instance->hal.param.steplen; 
        if (instance->data.steplen_cycles > config_data.steplen) {config_data.steplen = instance->data.steplen_cycles;};
        // - stepspace
        instance->data.stepspace_cycles = instance->hal.param.stepspace * litexcnc->stepgen.data.cycles_per_ns + 0.5;
        instance->memo.stepspace = instance->hal.param.stepspace; 
        if (instance->data.stepspace_cycles > stepspace_cycles) {stepspace_cycles = instance->data.stepspace_cycles;};
        // - dir_hold_time
        instance->data.dirhold_cycles = instance->hal.param.dir_hold_time * litexcnc->stepgen.data.cycles_per_ns + 0.5;
        instance->memo.dir_hold_time = instance->hal.param.dir_hold_time; 
        if (instance->data.dirhold_cycles > config_data.dir_hold_time) {config_data.dir_hold_time = instance->data.dirhold_cycles;};
        // - dir_setup_time
        instance->data.dirsetup_cycles = instance->hal.param.dir_setup_time * litexcnc->stepgen.data.cycles_per_ns + 0.5;
        instance->memo.dir_setup_time = instance->hal.param.dir_setup_time; 
        if (instance->data.dirsetup_cycles > config_data.dir_setup_time) {config_data.dir_setup_time = instance->data.dirsetup_cycles;};
    }

    // Calculate the maximum frequency for stepgen (in if statement to prevent division)
    if ((litexcnc->stepgen.memo.stepspace_cycles != stepspace_cycles) || (litexcnc->stepgen.memo.steplen != config_data.steplen)) {
        litexcnc->stepgen.data.max_frequency = (double) litexcnc->clock_frequency / (config_data.steplen + stepspace_cycles);
        litexcnc->stepgen.memo.steplen = config_data.steplen;
        litexcnc->stepgen.memo.stepspace_cycles = stepspace_cycles;
    }

    // Convert the general data to the correct byte order
    config_data.steplen = htobe32(config_data.steplen);
    config_data.dir_hold_time = htobe32(config_data.dir_hold_time);
    config_data.dir_setup_time = htobe32(config_data.dir_setup_time);

    // Put the data on the data-stream and advance the pointer
    memcpy(*data, &config_data, LITEXCNC_STEPGEN_CONFIG_DATA_SIZE);
    *data += LITEXCNC_STEPGEN_CONFIG_DATA_SIZE;

}

uint8_t litexcnc_stepgen_prepare_write(litexcnc_t *litexcnc, uint8_t **data, long period) {


    // STEP 1: Timings
    // ===============
    // The timings cannot be changed during runtime. Notify user when they are changed.
    for (size_t i=0; i<litexcnc->stepgen.num_instances; i++) {
        // Get pointer to the stepgen instance
        litexcnc_stepgen_pin_t *instance = &(litexcnc->stepgen.instances[i]);

        // Recalculate the timings
        // - steplen
        if (instance->hal.param.steplen != instance->memo.steplen) {
            LITEXCNC_ERR("Cannot change parameter `steplen` after configuration of the FPGA. Change is cancelled.\n", litexcnc->fpga->name);
            instance->hal.param.steplen = instance->memo.steplen;
        }
        // - stepspace
        if (instance->hal.param.stepspace != instance->memo.stepspace) {
            LITEXCNC_ERR("Cannot change parameter `stepspace` after configuration of the FPGA. Change is cancelled.\n", litexcnc->fpga->name);
            instance->hal.param.stepspace = instance->memo.stepspace;
        }
        // - dir_hold_time
        if (instance->hal.param.dir_hold_time != instance->memo.dir_hold_time) {
            LITEXCNC_ERR("Cannot change parameter `dir_hold_time` after configuration of the FPGA. Change is cancelled.\n", litexcnc->fpga->name);
            instance->hal.param.dir_hold_time = instance->memo.dir_hold_time;
        }
        // - dir_setup_time
        if (instance->hal.param.dir_setup_time != instance->memo.dir_setup_time) {
            LITEXCNC_ERR("Cannot change parameter `dir_setup_time` after configuration of the FPGA. Change is cancelled.\n", litexcnc->fpga->name);
            instance->hal.param.dir_setup_time = instance->memo.dir_setup_time;
        }
    }

    // Determine the apply time and store it for the position prediction
    litexcnc_stepgen_general_write_data_t data_general = {0};
    data_general.apply_time = litexcnc->stepgen.memo.apply_time;
    
    // Convert the general data to the correct byte order
    data_general.apply_time = htobe64(data_general.apply_time);

    // Put the data on the data-stream and advance the pointer
    memcpy(*data, &data_general, LITEXCNC_STEPGEN_GENERAL_WRITE_DATA_SIZE);
    *data += LITEXCNC_STEPGEN_GENERAL_WRITE_DATA_SIZE;

    // STEP 2: Speed per stepgen
    // =========================
    double desired_speed;
    for (size_t i=0; i<litexcnc->stepgen.num_instances; i++) {
        litexcnc_stepgen_instance_write_data_t data_instance = {0,0,0,0,0};
        // Get pointer to the stepgen instance
        litexcnc_stepgen_pin_t *instance = &(litexcnc->stepgen.instances[i]);

        // Check the limits on the speed of the stepgen
        if (instance->hal.param.maxvel <= 0.0) {
            // Negative speed is limited as zero
            instance->hal.param.maxvel = 0.0;
        } else {
            // Maximum speed is positive and no zero, compare with maximum frequency
            max_speed_desired = instance->hal.param.maxvel;
	        if (max_speed_desired > (litexcnc->stepgen.data.max_frequency * fabs(instance->hal.param.position_scale))) {
                instance->hal.param.maxvel = litexcnc->stepgen.data.max_frequency * fabs(instance->hal.param.position_scale);
		        // Maximum speed is too high, complain about it and modify the value
		        if (!instance->memo.error_max_speed_printed) {
		            LITEXCNC_ERR_NO_DEVICE(
			            "STEPGEN: Channel %d: The requested maximum velocity of %.2f units/sec is too high.\n",
			            i, max_speed_desired);
		            LITEXCNC_ERR_NO_DEVICE(
			            "STEPGEN: The maximum possible velocity is %.2f units/second\n",
			            instance->hal.param.maxvel);
		            instance->memo.error_max_speed_printed = true;
		        }
	        }
        }

        // Check whether the max acceleration has been set. Calculate the reciprocal as it used
        // very often in the code.
        if (instance->hal.param.maxaccel != instance->memo.maxaccel) {
            // Make sure the maximum accelaration is 0 of positive
            if (instance->hal.param.maxaccel <= 0) {
                instance->memo.maxaccel = 0;
            }
            // TODO: limit acceleration to the thread period.
            // TODO: make sure not to divide by zero, at this moment uncaught.
            instance->memo.maxaccel = instance->hal.param.maxaccel;
            instance->data.acc_recip = 1.0 / instance->hal.param.maxaccel;
        }

        // NOTE: The acceleration is not limited by this component. The physical maximum 
        // acceleration is applied within the FPGA
        if (!instance->hal.pin.enable) {
            // When the stepgen is not enabled, the speed is held at 0.
            LITEXCNC_PRINT_NO_DEVICE("Disabled\n");
            speed_new = 0;
        } else if (instance->hal.param.position_mode) {
            // Get the period in seconds. This period may be stretched when the acceleration
            // limits are not met.

            /*
            ======================================
            */
            double pos_cmd, vel_cmd, curr_pos, curr_vel;
            double match_ac, match_time;
            double est_out, est_cmd, est_err;
            double new_vel, avg_v, dv, dp;

            float period_s_recip = 1.0 / litexcnc->stepgen.memo.period_s;

            /* Calculate position command in counts */
            pos_cmd = *(instance->hal.pin.position_cmd) * instance->hal.param.position_scale;
            /* Calculate velocity command in counts/sec */
            vel_cmd = (pos_cmd - instance->memo.position_cmd) * period_s_recip;
            instance->memo.position_cmd = pos_cmd;
            /* Get the position in counts. We use the predcicted position */
            curr_pos = *(instance->hal.pin.position_prediction) * instance->hal.param.position_scale;
            /* Get the velocity in counts/sec. We use the predicted velocity */
            curr_vel = *(instance->hal.pin.speed_prediction) * instance->hal.param.position_scale;
            
            /* At this point we have good values for pos_cmd, curr_pos, vel_cmd, curr_vel, 
               max_freq and max_ac, all in counts, counts/sec, or counts/sec^2.  Now we just
               have to do something useful with them.
            */
            /* Determine which way we need to ramp to match velocity */
            if (vel_cmd > curr_vel) {
                match_ac = instance->hal.param.maxaccel * instance->hal.param.position_scale;
            } else {
                match_ac = -1 * instance->hal.param.maxaccel * instance->hal.param.position_scale;
            }
            /* determine how long the match would take */
            match_time = (vel_cmd - curr_vel) / match_ac;
            /* calc output position at the end of the match */
            avg_v = (vel_cmd + curr_vel) * 0.5;
            est_out = curr_pos + avg_v * match_time;
            /* calculate the expected command position at that time */
            /* Original code had a factor of 1.5 in the equation, don't know why, but causes
               the movement to lag a bit. Replaced with a factor of 1.0 to see whether this
               improves settling down on the spot. 
               est_cmd = pos_cmd + vel_cmd * (match_time - 1.5 * litexcnc->stepgen.memo.period_s);
            */
            est_cmd = pos_cmd + vel_cmd * (match_time - 1.0 * litexcnc->stepgen.memo.period_s);
            /* calculate error at that time */
            est_err = est_out - est_cmd;

            if (*(instance->hal.pin.debug)) {
                LITEXCNC_PRINT_NO_DEVICE("Errors %.6f, %.6f, %.6f, %.6f, %.6f, %.6f, %.6f, %.6f\n",
                    avg_v,
                    vel_cmd,
                    curr_pos,
                    curr_vel,
                    match_time,
                    est_out,
                    est_cmd,
                    est_err
                );
            }

            if (match_time < litexcnc->stepgen.memo.period_s) {
                /* We can match velocity in one period */
                if (fabs(est_err) < 0.0001) {
                    /* after match the position error will be acceptable */
                    /* so we just do the velocity match */
                    new_vel = vel_cmd;
                } else {
                    /* try to correct position error. Re-calculate the match-time, as it is used
                       by the feedback loop. Don't do this and the position will be off (especially
                       under acceleration / deceleration) and hunting will occur. Don't forget to 
                       take the absolute value, otherwise in some corner cases this might cause a
                       roll-over in the process (with all the mayhem you can imagine).
                    */
                    // new_vel = vel_cmd - 0.5 * est_err * period_s_recip;
                    new_vel = vel_cmd - est_err / (litexcnc->stepgen.memo.period_s - match_time);
                    match_time = fabs((new_vel - curr_vel) / match_ac);
                    /* apply accel limits */
                    if (new_vel > (curr_vel + instance->hal.param.maxaccel * instance->hal.param.position_scale * litexcnc->stepgen.memo.period_s)) {
                        new_vel = curr_vel + instance->hal.param.maxaccel * instance->hal.param.position_scale * litexcnc->stepgen.memo.period_s;
                        match_time = litexcnc->stepgen.memo.period_s;
                    } else if (new_vel < (curr_vel - instance->hal.param.maxaccel * instance->hal.param.position_scale * litexcnc->stepgen.memo.period_s)) {
                        new_vel = curr_vel - instance->hal.param.maxaccel * instance->hal.param.position_scale * litexcnc->stepgen.memo.period_s;
                        match_time = litexcnc->stepgen.memo.period_s;
                    }
                }
            } else {
                /* calculate change in final position if we ramp in the
                opposite direction for one period */
                dv = -2.0 * match_ac * litexcnc->stepgen.memo.period_s;
                dp = dv * match_time;
                /* decide which way to ramp */
                if (fabs(est_err + dp * 2.0) < fabs(est_err)) {
                    match_ac = -match_ac;
                }
                /* and do it */
                new_vel = curr_vel + match_ac * litexcnc->stepgen.memo.period_s;
                match_time = litexcnc->stepgen.memo.period_s;
            }

            if (*(instance->hal.pin.debug)) {
                LITEXCNC_PRINT_NO_DEVICE("Position method %.6f, %.6f, %.6f, %.6f, %.6f, %.6f\n",
                    pos_cmd,
                    vel_cmd,
                    curr_vel,
                    new_vel,
                    match_ac,
                    match_time
                );
            }

            new_vel *= instance->data.scale_recip;
            /*
            ======================================
            */

            // Store it on the data-frame
            instance->data.acc1 = instance->hal.param.maxaccel;
            instance->data.time1 = match_time;
            instance->data.speed1 = new_vel;
            instance->data.acc2 = 0;
            instance->data.time2 = litexcnc->stepgen.memo.period_s;
            instance->data.speed2 = new_vel;

        } else {
            // Velocity mode
            // If possible, the speed is gradually changed within one period. When the desired
            // change in the speed is too large, the maximum acceleration is applied (and thus)
            // the acceleration pushed into the next phase.
            float acc_time = fabs(*(instance->hal.pin.velocity_cmd) - *(instance->hal.pin.speed_prediction)) * instance->data.acc_recip;
            if (acc_time < period * 1e9) {
                instance->data.acc1 = fabs(*(instance->hal.pin.velocity_cmd) - *(instance->hal.pin.velocity_cmd)) *  litexcnc->stepgen.data.recip_dt;
                instance->data.time1 = period * 1e9;
            } else {
                instance->data.acc1 = instance->hal.param.maxaccel;
                instance->data.time1 = acc_time;
            }
            instance->data.speed1 = *(instance->hal.pin.velocity_cmd);
            instance->data.acc2 = 0;
            instance->data.speed2 = *(instance->hal.pin.velocity_cmd);
            instance->data.time2 = instance->data.time1;
            // Store this position command for the following loop
            instance->memo.velocity_cmd = *(instance->hal.pin.velocity_cmd);
        }

        // Limit the speed to the maximum speed (both phases)
        if (instance->data.speed1 > instance->hal.param.maxvel) {
            instance->data.speed1 = instance->hal.param.maxvel;
        } else if (instance->data.speed1 < (-1 * instance->hal.param.maxvel)) {
            instance->data.speed1 = -1 * instance->hal.param.maxvel;
        }
        if (instance->data.speed2 > instance->hal.param.maxvel) {
            instance->data.speed2 = instance->hal.param.maxvel;
        } else if (instance->data.speed2 < (-1 * instance->hal.param.maxvel)) {
            instance->data.speed2 = -1 * instance->hal.param.maxvel;
        }

        // Store the integer values on the fpga
        instance->data.fpga_speed1 = (int32_t) (instance->data.speed1 * instance->data.fpga_speed_scale) + 0x80000000;
        instance->data.fpga_acc1   = fabs(instance->data.acc1) * instance->data.fpga_acc_scale;
        instance->data.fpga_time1  = instance->data.time1 * litexcnc->clock_frequency;
        instance->data.fpga_speed2 = (int32_t) (instance->data.speed2 * instance->data.fpga_speed_scale) + 0x80000000;
        instance->data.fpga_acc2   = fabs(instance->data.acc2) * instance->data.fpga_acc_scale;
        instance->data.fpga_time2  = instance->data.time2 * litexcnc->clock_frequency;

        data_instance.speed_target1 = htobe32(instance->data.fpga_speed1);
        data_instance.acceleration1 = htobe32(instance->data.fpga_acc1);
        data_instance.part1_cycles  = htobe32(instance->data.fpga_time1);
        data_instance.speed_target2 = htobe32(instance->data.fpga_speed2);
        data_instance.acceleration2 = htobe32(instance->data.fpga_acc2);

        if (*(instance->hal.pin.debug)) {
            LITEXCNC_PRINT_NO_DEVICE("%llu, %llu, %.6f, %.6f, %.6f, %.6f, %.6f, %.6f, %.6f, %.6f\n", 
                litexcnc->wallclock->memo.wallclock_ticks,
                litexcnc->stepgen.memo.apply_time,
                *(instance->hal.pin.position_cmd),
                *(instance->hal.pin.position_prediction),
                instance->data.speed1,
                instance->data.acc1,
                instance->data.time1,
                instance->data.speed2,
                instance->data.acc2,
                instance->data.time2
            );
            LITEXCNC_PRINT_NO_DEVICE("%X, %X, %X, %X, %X\n", 
                instance->data.fpga_speed1,
                instance->data.fpga_acc1,
                instance->data.fpga_time1,
                instance->data.fpga_speed2,
                instance->data.fpga_acc2
            );
        }

        // Put the data on the data-stream and advance the pointer
        memcpy(*data, &data_instance, LITEXCNC_STEPGEN_INSTANCE_WRITE_DATA_SIZE);
        *data += LITEXCNC_STEPGEN_INSTANCE_WRITE_DATA_SIZE;
    }
}

size_t pos = 0;
double sum = 0;
#define AVG_ARR_SIZE 10
#define AVG_ARR_SIZE_RECIP 1.0 / AVG_ARR_SIZE;
double arrNumbers[AVG_ARR_SIZE] = {0};

double movingAvg(double *ptrArrNumbers, double *ptrSum, size_t pos, size_t len, double nextNum)
{
  //Subtract the oldest number from the prev sum, add the new number
  *ptrSum = *ptrSum - ptrArrNumbers[pos] + nextNum;
  //Assign the nextNum to the position in the array
  ptrArrNumbers[pos] = nextNum;
  //return the average
  return *ptrSum * AVG_ARR_SIZE_RECIP;
}

uint8_t litexcnc_stepgen_process_read(litexcnc_t *litexcnc, uint8_t** data, long period) {

    // Initialisation for the previous wall clock
    if (litexcnc->stepgen.memo.prev_wall_clock == 0) {
        // Setup teh wall clock to be equal to the current wall-clock minus
        // the idealised timing
        litexcnc->stepgen.memo.period_s = 1e-9 * period;
        litexcnc->stepgen.memo.cycles_per_period = litexcnc->stepgen.memo.period_s * litexcnc->clock_frequency;
        litexcnc->stepgen.memo.prev_wall_clock = litexcnc->wallclock->memo.wallclock_ticks - litexcnc->stepgen.memo.cycles_per_period;
        litexcnc->stepgen.memo.apply_time = litexcnc->stepgen.memo.prev_wall_clock + 0.9 * litexcnc->stepgen.memo.cycles_per_period;
        // Initialize the running average
        for (size_t i=0; i<AVG_ARR_SIZE; i++){
            arrNumbers[i] = (double) litexcnc->stepgen.memo.period_s;
            sum += litexcnc->stepgen.memo.period_s;
        }
    }

    rtapi_print("Period_s: %.6f \n",
        litexcnc->stepgen.memo.period_s
    );

    // The next apply time is basically chosen so that the next loop starts exactly when it
    // should (according to the timing of the previous loop). When due to latency excursion
    // this timing cannot be met (outside the bounds or already in the past) this variable
    // is modified. An additional 0.5 is added to prevent rounding errors accumulate over
    // time.
    uint64_t next_apply_time = (double) litexcnc->stepgen.memo.apply_time + litexcnc->stepgen.memo.period_s * litexcnc->clock_frequency + 0.5;

    // Determine how many clicks have been passed from the previous loop
    int32_t loop_cycles = litexcnc->wallclock->memo.wallclock_ticks - litexcnc->stepgen.memo.prev_wall_clock;

    // Check whether the loop-cycles are within the limits
    if (loop_cycles < (0.9 * litexcnc->stepgen.memo.cycles_per_period)) {
        // The previous loop was way too short
        next_apply_time += (loop_cycles - 0.9 * litexcnc->stepgen.memo.cycles_per_period);
        loop_cycles = 0.9 * litexcnc->stepgen.memo.cycles_per_period;
    } else if (loop_cycles > (1.1 * litexcnc->stepgen.memo.cycles_per_period)) {
        // The previous loop was way too long, possibly a latency excursion
        next_apply_time += (loop_cycles - 1.1 * litexcnc->stepgen.memo.cycles_per_period);
        loop_cycles = 1.1 * litexcnc->stepgen.memo.cycles_per_period;
    }

    // Check whether the nex_apply_time is within the expected range. When outside of the range, 
    // the value is clipped and a warning is shown to the user. The warning is only shown once.
    if (next_apply_time < litexcnc->wallclock->memo.wallclock_ticks + 0.81 * litexcnc->stepgen.memo.cycles_per_period) {
        rtapi_print("Apply time exceeding limits: %llu, %llu, %llu \n",
            litexcnc->wallclock->memo.wallclock_ticks,
            litexcnc->stepgen.memo.apply_time,
            next_apply_time
        );
        next_apply_time = litexcnc->wallclock->memo.wallclock_ticks + 0.81 * litexcnc->stepgen.memo.cycles_per_period;
        // Show warning
        if (!litexcnc->stepgen.data.warning_apply_time_exceeded_shown) {
            LITEXCNC_ERR_NO_DEVICE("Apply time exceeded limits.");
            litexcnc->stepgen.data.warning_apply_time_exceeded_shown = true;
        }
    }
    if (next_apply_time > litexcnc->wallclock->memo.wallclock_ticks + 0.99 * litexcnc->stepgen.memo.cycles_per_period){
        rtapi_print("Apply time exceeding limits: %llu, %llu, %llu \n",
            litexcnc->wallclock->memo.wallclock_ticks,
            litexcnc->stepgen.memo.apply_time,
            next_apply_time
        );        
        next_apply_time > litexcnc->wallclock->memo.wallclock_ticks + 0.99 * litexcnc->stepgen.memo.cycles_per_period;
        // Show warning
        if (!litexcnc->stepgen.data.warning_apply_time_exceeded_shown) {
            LITEXCNC_ERR_NO_DEVICE("Apply time exceeded limits.");
            litexcnc->stepgen.data.warning_apply_time_exceeded_shown = true;
        }
    }

    // Calculate the period in seconds to use for the next step and store the wall clock for
    // next loop
    litexcnc->stepgen.memo.period_s = movingAvg(
        arrNumbers, 
        &sum, 
        pos, 
        AVG_ARR_SIZE, 
        (double) loop_cycles * litexcnc->clock_frequency_recip);
    pos++;
    if (pos >= AVG_ARR_SIZE) {
      pos = 0;
    }
    litexcnc->stepgen.memo.prev_wall_clock = litexcnc->wallclock->memo.wallclock_ticks;

    // Receive and process the data for all the stepgens
    for (size_t i=0; i<litexcnc->stepgen.num_instances; i++) {
        // Get pointer to the stepgen instance
        litexcnc_stepgen_pin_t *instance = &(litexcnc->stepgen.instances[i]);

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
            instance->data.fpga_speed_scale = (float) (instance->hal.param.position_scale * litexcnc->clock_frequency_recip) * (1LL << instance->data.pick_off_vel);
            instance->data.fpga_speed_scale_inv = (float) litexcnc->clock_frequency * instance->data.scale_recip / (1LL << instance->data.pick_off_vel);
            instance->data.fpga_acc_scale = (float) (instance->hal.param.position_scale * litexcnc->clock_frequency_recip * litexcnc->clock_frequency_recip) * (1LL << (instance->data.pick_off_acc));
            instance->data.fpga_acc_scale_inv =  (float) instance->data.scale_recip * litexcnc->clock_frequency * litexcnc->clock_frequency / (1LL << instance->data.pick_off_acc);;
        }

        // Store the old data
        instance->memo.position = instance->data.position;
        // Read data and proceed the buffer
        int64_t pos;
        memcpy(&pos, *data, sizeof pos);
        instance->data.position = be64toh(pos);
        // LITEXCNC_PRINT_NO_DEVICE("Pos feedback %d \n", pos);
        *data += 8;  // The data read is 64 bit-wide. The buffer is 8-bit wide
        uint32_t speed;
        memcpy(&speed, *data, sizeof speed);
        instance->data.speed = (int64_t) be32toh(speed) -  0x80000000;
        *data += 4;  // The data read is 32 bit-wide. The buffer is 8-bit wide
        // TODO: this is temporary to investigate why the second phase is not working
        int64_t tmp_apply_time;
        memcpy(&tmp_apply_time, *data, sizeof tmp_apply_time);
        tmp_apply_time = be64toh(tmp_apply_time);
        *data += 8;  // The data read is 64 bit-wide. The buffer is 8-bit wide
        // Convert the received position to HAL pins for counts and floating-point position
        *(instance->hal.pin.counts) = instance->data.position >> instance->data.pick_off_pos;
        // Check: why is a half step subtracted from the position. Will case a possible problem 
        // when the power is cycled -> will lead to a moving reference frame  
        // *(instance->hal.pin.position_fb) = (double)(instance->data.position-(1LL<<(instance->data.pick_off_pos-1))) * instance->data.scale_recip / (1LL << instance->data.pick_off_pos);
        *(instance->hal.pin.position_fb) = (double) instance->data.position * instance->data.scale_recip / (1LL << instance->data.pick_off_pos);
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
        uint64_t min_time;
        uint64_t max_time;
        float fraction;
        float speed_end;

        if (*(instance->hal.pin.debug)) {
            rtapi_print("Timings: %.6f, %llu, %llu, %lu, %lu, %llu",
                litexcnc->stepgen.memo.period_s,
                litexcnc->wallclock->memo.wallclock_ticks,
                litexcnc->stepgen.memo.apply_time,
                instance->data.fpga_time1,
                instance->data.fpga_time2,
                next_apply_time
            );
        }
        if (litexcnc->wallclock->memo.wallclock_ticks <= litexcnc->stepgen.memo.apply_time + instance->data.fpga_time1) {
            min_time = litexcnc->wallclock->memo.wallclock_ticks;
            if (litexcnc->stepgen.memo.apply_time > min_time) {
                min_time = litexcnc->stepgen.memo.apply_time;
            }
            max_time = litexcnc->stepgen.memo.apply_time + instance->data.fpga_time1;
            if (next_apply_time < max_time) {
                max_time = next_apply_time;
            }
            if ((litexcnc->stepgen.memo.apply_time + instance->data.fpga_time1 - min_time) <= 0) {
                fraction = 1.0;
            } else {
                fraction = (float) (max_time - min_time) / (litexcnc->stepgen.memo.apply_time + instance->data.fpga_time1 - min_time);
            }
            speed_end = (1.0 - fraction) * *(instance->hal.pin.speed_prediction) + fraction * instance->data.speed1;
            *(instance->hal.pin.position_prediction) += 0.5 * (*(instance->hal.pin.speed_prediction) + speed_end) * (max_time - min_time) * litexcnc->clock_frequency_recip;
            *(instance->hal.pin.speed_prediction) = speed_end;
            if (*(instance->hal.pin.debug)) {
                rtapi_print("Predict position - phase 1:  %.6f, %.6f, %.6f, %llu, %llu \n", *(instance->hal.pin.position_prediction), *(instance->hal.pin.speed_prediction), fraction, min_time, max_time);
            }
        }
        if ((litexcnc->wallclock->memo.wallclock_ticks <= litexcnc->stepgen.memo.apply_time + instance->data.fpga_time2) && (litexcnc->stepgen.memo.apply_time + instance->data.fpga_time1 < next_apply_time)) {
            min_time = litexcnc->wallclock->memo.wallclock_ticks;
            if ((litexcnc->stepgen.memo.apply_time + instance->data.fpga_time1) > min_time) {
                *(instance->hal.pin.speed_prediction) = instance->data.speed1;
                min_time = litexcnc->stepgen.memo.apply_time + instance->data.fpga_time1;
            }
            max_time = litexcnc->stepgen.memo.apply_time + instance->data.fpga_time2;
            if (next_apply_time < max_time) {
                max_time = next_apply_time;
            }
            if ((litexcnc->stepgen.memo.apply_time + instance->data.fpga_time2 - min_time) <= 0) {
                fraction = 1.0;
            } else {
                fraction = (float) (max_time - min_time) / (litexcnc->stepgen.memo.apply_time + instance->data.fpga_time2 - min_time);
            }
            speed_end = (1.0 - fraction) * *(instance->hal.pin.speed_prediction) + fraction * instance->data.speed2;
            *(instance->hal.pin.position_prediction) += 0.5 * (*(instance->hal.pin.speed_prediction) + speed_end) * (max_time - min_time) * litexcnc->clock_frequency_recip;
            *(instance->hal.pin.speed_prediction) = speed_end;
            if (*(instance->hal.pin.debug)) {
                rtapi_print("Predict position - phase 2: %.6f, %.6f, %.6f, %llu, %llu, %llu, %lu, %lu \n", *(instance->hal.pin.position_prediction), *(instance->hal.pin.speed_prediction), fraction, max_time,  min_time, (litexcnc->stepgen.memo.apply_time + instance->data.fpga_time2), instance->data.fpga_time1, instance->data.fpga_time2);
            }
        }
        if (next_apply_time > litexcnc->stepgen.memo.apply_time + instance->data.fpga_time2) {
            // Some constant speed should be added
            *(instance->hal.pin.speed_prediction) = instance->data.speed2;
            *(instance->hal.pin.position_prediction) += instance->data.speed2 * (next_apply_time - (litexcnc->stepgen.memo.apply_time + instance->data.fpga_time2)) * litexcnc->clock_frequency_recip;
            if (*(instance->hal.pin.debug)) {
                rtapi_print("Predict position - constant speed: %.6f, %.6f, %llu\n", *(instance->hal.pin.position_prediction), *(instance->hal.pin.speed_prediction), litexcnc->stepgen.memo.apply_time + instance->data.fpga_time2);
            }
        }
        if (*(instance->hal.pin.debug)) {
            rtapi_print("Speed feedback result: %.6f, %.6f, %X, %.6f, %.6f, %X, %.6f, %.6f \n",
                instance->data.speed1,
                instance->data.speed2,
                instance->data.speed, 
                *(instance->hal.pin.speed_fb),
                *(instance->hal.pin.speed_prediction),
                instance->data.position,
                *(instance->hal.pin.position_fb),
                *(instance->hal.pin.position_prediction)
            );
        }
    }

    // Push the apply for the write loop
    litexcnc->stepgen.memo.apply_time = next_apply_time;
}
