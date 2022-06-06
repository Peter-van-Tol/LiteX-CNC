


#include "litexcnc.h"
#include "stepgen.h"

// Declaration of parameters used in litexcnc_stepgen_prepare_write
double max_speed_desired;
double speed_new;
double speed_avg;
double match_time;
double pos_avg;
double pos_error;



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
            r = hal_param_bit_newf(HAL_RW, &(instance->hal.param.debug), litexcnc->fpga->comp_id, "%s.debug", base_name);
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


uint8_t litexcnc_stepgen_prepare_write(litexcnc_t *litexcnc, uint8_t **data, long period) {

    // Check whether the period of the thread has changed
    if (litexcnc->stepgen.memo.period != period) { 
        // - Calculate the reciprocal once here, to avoid multiple divides later
        litexcnc->stepgen.data.recip_dt = 1.0 / (period * 0.000000001);
        litexcnc->stepgen.memo.period = period;
    }

    // STEP 1: Timings
    // ===============
    // Check whether any of the parameter of the stepgens has changed. All stepgens
    // will use the same values for steplen, dir_hold_time and dir_setup_time. The
    // maximum value is governing, so we start with the value 0. For each instance
    // it is checked whether the value has changed. If it has changed, the time is
    // converted to cycles.
    // NOTE: all timings are in nano-seconds (1E-9), so the timing is multiplied with
    // the clock-frequency and divided by 1E9. However, this might lead to issues
    // with roll-over of the 32-bit integer. 
    litexcnc_stepgen_general_write_data_t data_general = {0,0,0,0};
    uint32_t stepspace_cycles = 0; // Separate variable, as it is not part of the data-package
    for (size_t i=0; i<litexcnc->stepgen.num_instances; i++) {
        // Get pointer to the stepgen instance
        litexcnc_stepgen_pin_t *instance = &(litexcnc->stepgen.instances[i]);

        // Recalculate the timings
        // - steplen
        if (instance->hal.param.steplen != instance->memo.steplen) {
            instance->data.steplen_cycles = instance->hal.param.steplen * litexcnc->stepgen.data.cycles_per_ns + 0.5;
            instance->memo.steplen = instance->hal.param.steplen; 
        }
        if (instance->data.steplen_cycles > data_general.steplen) {data_general.steplen = instance->data.steplen_cycles;};
        // - stepspace
        if (instance->hal.param.stepspace != instance->memo.stepspace) {
            instance->data.stepspace_cycles = instance->hal.param.stepspace * litexcnc->stepgen.data.cycles_per_ns + 0.5;
            instance->memo.stepspace = instance->hal.param.stepspace; 
        }
        if (instance->data.stepspace_cycles > stepspace_cycles) {stepspace_cycles = instance->data.stepspace_cycles;};
        // - dir_hold_time
        if (instance->hal.param.dir_hold_time != instance->memo.dir_hold_time) {
            instance->data.dirhold_cycles = instance->hal.param.dir_hold_time * litexcnc->stepgen.data.cycles_per_ns + 0.5;
            instance->memo.dir_hold_time = instance->hal.param.dir_hold_time; 
        }
        if (instance->data.dirhold_cycles > data_general.dir_hold_time) {data_general.dir_hold_time = instance->data.dirhold_cycles;};
        // - dir_setup_time
        if (instance->hal.param.dir_setup_time != instance->memo.dir_setup_time) {
            instance->data.dirsetup_cycles = instance->hal.param.dir_setup_time * litexcnc->stepgen.data.cycles_per_ns + 0.5;
            instance->memo.dir_setup_time = instance->hal.param.dir_setup_time; 
        }
        if (instance->data.dirsetup_cycles > data_general.dir_setup_time) {data_general.dir_setup_time = instance->data.dirsetup_cycles;};
    }

    // Calculate the maximum frequency for stepgen
    litexcnc->stepgen.data.max_frequency = (double) litexcnc->clock_frequency / (data_general.steplen + stepspace_cycles);

    // Determine the apply time
    data_general.apply_time = litexcnc->wallclock->memo.wallclock_ticks + (double) 0.9 * period * litexcnc->clock_frequency * 0.000000001;

    // Convert the general data to the correct byte order
    data_general.steplen = htobe32(data_general.steplen);
    data_general.dir_hold_time = htobe32(data_general.dir_hold_time);
    data_general.dir_setup_time = htobe32(data_general.dir_setup_time);
    data_general.apply_time = htobe64(data_general.apply_time);

    // Put the data on the data-stream and advance the pointer
    memcpy(*data, &data_general, LITEXCNC_STEPGEN_GENERAL_WRITE_DATA_SIZE);
    *data += LITEXCNC_STEPGEN_GENERAL_WRITE_DATA_SIZE;

    // STEP 2: Speed per stepgen
    // =========================
    double desired_speed;
    for (size_t i=0; i<litexcnc->stepgen.num_instances; i++) {
        litexcnc_stepgen_instance_write_data_t data_instance;
        // Get pointer to the stepgen instance
        litexcnc_stepgen_pin_t *instance = &(litexcnc->stepgen.instances[i]);

        // Check the limits on the speed of the setpgen
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

        if (instance->hal.param.debug) {
            LITEXCNC_PRINT_NO_DEVICE("%llu, %.6f, %.6f, %.6f, %.6f, %.6f", 
                litexcnc->wallclock->memo.wallclock_ticks,
                *(instance->hal.pin.position_cmd),
                *(instance->hal.pin.position_fb), 
                *(instance->hal.pin.position_prediction),
                *(instance->hal.pin.speed_fb),
                *(instance->hal.pin.speed_prediction)
            );
        }

        // NOTE: The acceleration is not limited by this component. The physical maximum 
        // acceleration is applied within the FPGA
        if (instance->hal.param.position_mode) {
            // Position mode (currently the only supported method)
            // Estimate the required (average) speed for getting to the commanded position, required time to
            // accelerate to that speed.
            speed_avg = (*(instance->hal.pin.position_cmd) - *(instance->hal.pin.position_prediction)) * litexcnc->stepgen.data.recip_dt;
            if (instance->hal.param.debug) {
                rtapi_print(", Avg. speed : %.6f", speed_avg);
            }
            match_time = fabs(speed_avg - *(instance->hal.pin.speed_prediction)) / instance->hal.param.maxaccel * 1.0e9;
            if (instance->hal.param.debug) {
                rtapi_print(", match time : %.6f", match_time); 
            }
            if (match_time < litexcnc->clock_frequency_recip) {
                // Constant speed
                speed_new = speed_avg;
                if (instance->hal.param.debug) {
                    rtapi_print(" , constant speed %.2f", speed_new);
                }
            }
            else if (!instance->hal.pin.catching_up && (match_time <= (0.5 * period))) {
                // The average speed can be obtained during one period. Now determine at which 
                // position we would end up if this average speed was used. There will be a difference 
                // between the commanded position and the 'average' position we have to compensate for.
                pos_avg = *(instance->hal.pin.position_prediction) \
                        + (
                            0.5 * ( *(instance->hal.pin.speed_prediction) + speed_avg) * match_time \
                            + (period - match_time) * speed_avg) \
                        * 1e-9;
                pos_error = *(instance->hal.pin.position_cmd) - pos_avg;
                // Determine which the 'extra' speed required to solve for this position error. 
                // The error can only be fully compensated when the match_time is less then half
                // of the period, otherwise the acceleration constraint cannot be met.
                speed_new = speed_avg + pos_error * 2 / match_time;
                if (instance->hal.param.debug) {
                    rtapi_print(" , matching speed to %.2f", speed_new);
                }
            } else {
                // Set the flag that we are catching up with the error
                // TODO: why can this not be a pointer?
                instance->hal.pin.catching_up = 1;
                // The whole period we must accelerate to even get to the average speed. Apply 
                // maximum acceleration. If this happens multiple periods in a row, this might
                // get into following errors.
                float commanded_speed = (*(instance->hal.pin.position_cmd) - instance->memo.position_cmd) * litexcnc->stepgen.data.recip_dt;
                float error = *(instance->hal.pin.position_cmd) - *(instance->hal.pin.position_prediction);
                int32_t sign = (commanded_speed>0)?1:-1;
                float squared = commanded_speed * commanded_speed + sign * 1.95 * instance->hal.param.maxaccel * error;
                // The new speed is calculated based on the error on the start of the period. However,
                // the commanded speed is defined at the end of the period. So we have to compensate for
                // the acceleration during the period. We also compensate for the difference between the
                // commanded speed and actual speed of the previous loop.
                float difference = instance->data.speed_float - *(instance->hal.pin.speed_prediction);
                if (fabs(difference) > (0.025 * period * 0.000000001 * instance->hal.param.maxaccel)) {
                    if (difference > 0) {
                        difference = 0.025 * period * 0.000000001 * instance->hal.param.maxaccel;
                    } else {
                        difference = -0.025 * period * 0.000000001 * instance->hal.param.maxaccel;
                    }
                }
                sign = (squared<0)?-1*sign:sign;
                speed_new = sign * sqrt(fabs(squared)) + difference;
                // TODO: check whether this overshoots the error in the next step (because the fixed addition
                // 0f the acceleration of one period). If that is the case, modify the speed to minimize the
                // error and end the catching up state.
                if (fabs(commanded_speed - speed_new) < (period * 0.000000001 * instance->hal.param.maxaccel)) {
                    float position_new = *(instance->hal.pin.position_prediction) + 0.5 * (*(instance->hal.pin.speed_prediction) + commanded_speed) * period * 0.000000001;
                    float error_new = *(instance->hal.pin.position_cmd) - position_new;
                    if (fabs(error_new) <= fabs(error)) {
                        speed_new = commanded_speed;
                        instance->hal.pin.catching_up = 0;
                    }
                } else {
                     speed_new += (speed_new<commanded_speed?1:-1) * period * 0.000000001 * instance->hal.param.maxaccel;
                }
                if (instance->hal.param.debug) {
                    rtapi_print(" , full accelaration to %.2f", speed_new);
                }
            }
            if (instance->hal.param.debug) {
                rtapi_print("\n");
            }
            // LITEXCNC_PRINT_NO_DEVICE("Speed for position command: %d units / s \n", speed_new);
            // Store this position command for the following loop
            instance->memo.position_cmd = *(instance->hal.pin.position_cmd);
        } else {
            // Speed mode
            speed_new = *(instance->hal.pin.velocity_cmd);
            // LITEXCNC_PRINT_NO_DEVICE("Speed commanded: %0.2f units / s \n", speed_new);
        }
        // Limit the speed to the maximum speed
        if (speed_new > instance->hal.param.maxvel) {
            speed_new = instance->hal.param.maxvel;
        } else if (speed_new < -1 * instance->hal.param.maxvel) {
            speed_new = -1 * instance->hal.param.maxvel;
        }
        // Store the speed
        instance->data.speed_float = speed_new;
        instance->data.speed = (speed_new * instance->hal.param.position_scale * litexcnc->clock_frequency_recip) * (1 << PICKOFF);
        
        // Now the new speed has been calculated, convert it from units/s to steps per clock-cycle
        data_instance.speed_target = htobe32((speed_new * instance->hal.param.position_scale * litexcnc->clock_frequency_recip) * (1 << PICKOFF) + 0x80000000);
        data_instance.max_acceleration = htobe32(((float) (instance->hal.param.maxaccel * instance->hal.param.position_scale * litexcnc->clock_frequency_recip * litexcnc->clock_frequency_recip) * ((uint64_t) 1 << (PICKOFF + 16))));
        
        // Put the data on the data-stream and advance the pointer
        memcpy(*data, &data_instance, LITEXCNC_STEPGEN_INSTANCE_WRITE_DATA_SIZE);
        *data += LITEXCNC_STEPGEN_INSTANCE_WRITE_DATA_SIZE;
    }
}


uint8_t litexcnc_stepgen_process_read(litexcnc_t *litexcnc, uint8_t** data, long period) {

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
            instance->data.scale_recip = (1.0 / (1L << PICKOFF)) / instance->hal.param.position_scale;
            instance->memo.position_scale = instance->hal.param.position_scale; 
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
        // LITEXCNC_PRINT_NO_DEVICE("Speed feedback %d \n", instance->data.speed);
        *data += 4;  // The data read is 32 bit-wide. The buffer is 8-bit wide
        // Convert the received position to HAL pins for counts and floating-point position
        *(instance->hal.pin.counts) = instance->data.position >> PICKOFF;
        // Check: why is a half step subtracted from the position. Will case a possible problem 
        // when the power is cycled -> will lead to a moving reference frame  
        *(instance->hal.pin.position_fb) = (double)(instance->data.position-(1<<(PICKOFF-1))) * instance->data.scale_recip;
        *(instance->hal.pin.speed_fb) = (double) litexcnc->clock_frequency * instance->data.speed * instance->data.scale_recip;

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
        // - pending apply time
        // TODO: expected to be non-relevant though
        // - check whether the difference in target speed and current speed can be bridged 
        //   within 1 period.
	    double match_time = fabs(*(instance->hal.pin.speed_fb) - instance->data.speed_float) / instance->hal.param.maxaccel * 1e9;
        if (match_time <= (0.9 * period)) {
            // The predicted speed at the end of the period is equal to the commanded velocity
            *(instance->hal.pin.speed_prediction) = instance->data.speed_float;
            // The position is calculated based on the average speed during the acceleration fase (the match_time) and 
            // commanded speed during the constant speed fase
            *(instance->hal.pin.position_prediction) +=  0.5 * (*(instance->hal.pin.speed_fb) + *(instance->hal.pin.speed_prediction)) * (match_time * 0.000000001);
            *(instance->hal.pin.position_prediction) +=  *(instance->hal.pin.speed_prediction) * (((0.9 * period) - match_time) * 0.000000001);
        } else {
            // The speed is equal to the current velocity + acceleration / deceleration
            if (*(instance->hal.pin.speed_fb) < instance->data.speed_float) {
                // Acceleration, current speed is lower then the commanded speed
                *(instance->hal.pin.speed_prediction) += (0.9 * period * 0.000000001) * instance->hal.param.maxaccel;
            } else {
                // Deceleration, current speed is larger then the commanded speed
                *(instance->hal.pin.speed_prediction) -= (0.9 * period * 0.000000001) * instance->hal.param.maxaccel;
            }
            // The position can be determined based on the average speed (area under the trapezoidal speed curve)
            *(instance->hal.pin.position_prediction) +=  0.5 * (*(instance->hal.pin.speed_fb) + *(instance->hal.pin.speed_prediction)) * (0.9 * period * 0.000000001);
        }
    }
}
