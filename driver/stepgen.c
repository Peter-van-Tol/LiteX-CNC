


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
    data_general.apply_time = litexcnc->wallclock->memo.wallclock_ticks + (double) 0.9 * period * litexcnc->clock_frequency * 0.000000001;
    litexcnc->stepgen.memo.apply_time = data_general.apply_time;

    // Convert the general data to the correct byte order
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

        // NOTE: The acceleration is not limited by this component. The physical maximum 
        // acceleration is applied within the FPGA
        if (!instance->hal.pin.enable) {
            // When the stepgen is not enabled, the speed is held at 0.
            LITEXCNC_PRINT_NO_DEVICE("Disabled\n");
            speed_new = 0;
        } else if (instance->hal.param.position_mode) {
            // Position mode
            // Estimate the error when the current speed is maintained. This calculation is
            // only based on the old and new position command. Any errors will be corrected
            // in a later step
            float est_error = *(instance->hal.pin.position_cmd) - (instance->memo.position_cmd + instance->memo.velocity_cmd * period * 1e-9);
            // - Convert the error to an acceleration rate
            float acc1 = 2 * est_error * litexcnc->stepgen.data.recip_dt * litexcnc->stepgen.data.recip_dt;

            // At what speed does the commanded position move => Wrong caclulation, is average, should be end (based on acc1)
            float velocity_cmd = instance->memo.velocity_cmd + acc1 * period * 1e-9;
            // instance->memo.velocity_cmd = (*(instance->hal.pin.position_cmd) - instance->memo.position_cmd) * litexcnc->stepgen.data.recip_dt;

            // Determine the error at the end of this loop (defined as the difference between
            // the postion where we should be, which is the previous position command, and the
            // predicted position). We try to mitigate this error as fast as possible, however
            // this error cannot be solved within one loop, as this would lead to oscillations.
            float est_error_start = instance->memo.position_cmd - *(instance->hal.pin.position_prediction);
            float est_error_end   = *(instance->hal.pin.position_cmd) - (*(instance->hal.pin.position_prediction) + 0.5 * (velocity_cmd + *(instance->hal.pin.speed_prediction)) * period * 1e-9);  
            float acc2 = (instance->memo.velocity_cmd  - *(instance->hal.pin.speed_prediction)) * litexcnc->stepgen.data.recip_dt + est_error_end * litexcnc->stepgen.data.recip_dt * litexcnc->stepgen.data.recip_dt;
            if (*(instance->hal.pin.debug)) {
                LITEXCNC_PRINT_NO_DEVICE("%.6f, %.6f, %.6f, %.6f, %.6f\n", acc1, instance->memo.velocity_cmd, velocity_cmd, est_error_end, acc2);
            }
            if ((acc1 == 0) && ((est_error_start * est_error_end) < 0)) {
                // The standard accelaration would flip the sign (thus does not go fast enough)
                // so we have to speed up the deceleration. Because the cycle time is fixed, this
                // can only be done when the basic component is zero (commanded speed is constant)
                acc2 = ((est_error_start - est_error_end) / est_error_start) * acc2;
            } 
            
            // Store for next loop
            instance->memo.velocity_cmd = velocity_cmd;

            // Sum the two contributions of the accelarion
            instance->data.acceleration = acc1 + acc2;

            // Check the bounds of the acceleration if it is set by the user
            if ((instance->hal.param.maxaccel != 0) && (instance->data.acceleration > instance->hal.param.maxaccel)) {
                instance->data.acceleration = instance->hal.param.maxaccel;
            } else if ((instance->hal.param.maxaccel != 0) && (instance->data.acceleration < (-1 * instance->hal.param.maxaccel))) {
                instance->data.acceleration = -1 * instance->hal.param.maxaccel;
            }

            // To prevent rounding errors the acceleration is here already casted to integer (format
            // used by the FPGA) and then back to float to store the value which is actually used by
            // this stepgen (required to calculate the correct location)
            data_instance.acceleration = ((float) (fabs(instance->data.acceleration) * instance->hal.param.position_scale * litexcnc->clock_frequency_recip * litexcnc->clock_frequency_recip) * (1LL << (instance->data.pick_off_acc)));
            // Catch the special case the accelaration_int is 0. In this case the target speed is applied
            // immediately, which requires the speed new to be modified because the speed profile is rectangular
            // instead of triangular.
            if (data_instance.acceleration == 0) {
                speed_new = *(instance->hal.pin.speed_prediction) + 0.5 * instance->data.acceleration * period * 1e-9;
                instance->data.acceleration = 0;
            } else {
                speed_new = *(instance->hal.pin.speed_prediction) + instance->data.acceleration * period * 1e-9;
                instance->data.acceleration = (float) data_instance.acceleration * instance->data.scale_recip * litexcnc->clock_frequency * litexcnc->clock_frequency / (1LL << instance->data.pick_off_acc);
            }

            // Store this position command for the following loop
            instance->memo.position_cmd = *(instance->hal.pin.position_cmd);
        } else {
            // Speed mode
            speed_new = *(instance->hal.pin.velocity_cmd);
            data_instance.acceleration = instance->hal.param.maxaccel;
            // TODO: error when max accel is not set when in speed mode
            // LITEXCNC_PRINT_NO_DEVICE("Speed commanded: %0.2f units / s \n", speed_new);
        }

        // Limit the speed to the maximum speed
        if (speed_new > instance->hal.param.maxvel) {
            speed_new = instance->hal.param.maxvel;
        } else if (speed_new < (-1 * instance->hal.param.maxvel)) {
            speed_new = -1 * instance->hal.param.maxvel;
        }

        // Store the speed. For the speed_float we calculate it based on the int-value which
        // is send to the FPGA to account for any rounding errors
        instance->data.speed = (speed_new * instance->hal.param.position_scale * litexcnc->clock_frequency_recip) * (1LL << instance->data.pick_off_vel);
        instance->data.speed_float = (float) instance->data.speed * instance->data.scale_recip * litexcnc->clock_frequency / (1LL << instance->data.pick_off_vel);

        // Now the new speed has been calculated, convert it from units/s to steps per clock-cycle
        data_instance.speed_target = htobe32(instance->data.speed + 0x80000000);
        data_instance.acceleration = htobe32(data_instance.acceleration);

        if (*(instance->hal.pin.debug)) {
            LITEXCNC_PRINT_NO_DEVICE("%llu, %.6f, %.6f, %.6f, %.6f, %.6f, %.6f, %.6f\n", 
                litexcnc->wallclock->memo.wallclock_ticks,
                *(instance->hal.pin.position_cmd),
                *(instance->hal.pin.position_fb), 
                *(instance->hal.pin.position_prediction),
                *(instance->hal.pin.speed_fb),
                *(instance->hal.pin.speed_prediction),
                instance->data.speed_float,
                instance->data.acceleration
            );
        }

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
            instance->data.scale_recip = 1.0 / instance->hal.param.position_scale;
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
        *data += 4;  // The data read is 32 bit-wide. The buffer is 8-bit wide
        // Convert the received position to HAL pins for counts and floating-point position
        *(instance->hal.pin.counts) = instance->data.position >> instance->data.pick_off_pos;
        // Check: why is a half step subtracted from the position. Will case a possible problem 
        // when the power is cycled -> will lead to a moving reference frame  
        *(instance->hal.pin.position_fb) = (double)(instance->data.position-(1LL<<(instance->data.pick_off_pos-1))) * instance->data.scale_recip / (1LL << instance->data.pick_off_pos);
        *(instance->hal.pin.speed_fb) = (double) litexcnc->clock_frequency * instance->data.speed * instance->data.scale_recip / (1LL << instance->data.pick_off_vel);

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
        // - check whether the difference in target speed and current speed can be bridged 
        //   within 1 period.
        uint64_t next_apply_time = litexcnc->wallclock->memo.wallclock_ticks + (double) 0.9 * period * litexcnc->clock_frequency * 0.000000001;
        // - calculate when the acceleration phase is over.
        uint64_t accelaration_stopped = litexcnc->wallclock->memo.wallclock_ticks;
        if (instance->data.acceleration != 0) {
            // Prevent division by zero, only apply acceleration when it is defined
            accelaration_stopped += (uint64_t) ((fabs(*(instance->hal.pin.speed_fb) - instance->data.speed_float) / instance->data.acceleration) * litexcnc->clock_frequency);
        }
        
        if (*(instance->hal.pin.debug)) {
            rtapi_print("%llu, %llu, %llu, ",
                litexcnc->wallclock->memo.wallclock_ticks,
                next_apply_time,
                accelaration_stopped);
        }

        if (accelaration_stopped < next_apply_time) {
            if (*(instance->hal.pin.debug)) {
                rtapi_print("long, ");
            }
            // The stepgen was not fed for a while, it kept moving forward. In this case the speed will be
            // equal to the commanded speed 
            *(instance->hal.pin.speed_prediction) = instance->data.speed_float;
            // Calculate the position based the received position + acceleration fase + constant speed fase
            *(instance->hal.pin.position_prediction) += 0.5 * (*(instance->hal.pin.speed_fb) + *(instance->hal.pin.speed_prediction)) * (accelaration_stopped - litexcnc->wallclock->memo.wallclock_ticks) * litexcnc->clock_frequency_recip;
            *(instance->hal.pin.position_prediction) += *(instance->hal.pin.speed_prediction) * (next_apply_time - accelaration_stopped) * litexcnc->clock_frequency_recip;
            
            if (*(instance->hal.pin.debug)) {
                rtapi_print("%.6f, %.6f, %.6f, %.6f \n",
                    *(instance->hal.pin.speed_fb),
                    instance->data.speed_float,
                    *(instance->hal.pin.speed_prediction),
                    *(instance->hal.pin.position_prediction));
            }
        } else {
            if (*(instance->hal.pin.debug)) {
                rtapi_print("long, ");
            }
            // The stepgen on the FPGA is still accelerating (most likely the loop is a litle shorter)
            int8_t acc_dir = (*(instance->hal.pin.speed_fb) < instance->data.speed_float)?1:-1;
            *(instance->hal.pin.speed_prediction) += (float) acc_dir * instance->data.acceleration * (next_apply_time - litexcnc->wallclock->memo.wallclock_ticks) * litexcnc->clock_frequency_recip;
            *(instance->hal.pin.position_prediction) += 0.5 * (*(instance->hal.pin.speed_fb) + *(instance->hal.pin.speed_prediction)) *  (next_apply_time - litexcnc->wallclock->memo.wallclock_ticks) * litexcnc->clock_frequency_recip;
            
            if (*(instance->hal.pin.debug)) {
                rtapi_print("%.6f, %.6f, %.6f, %.6f \n",
                    *(instance->hal.pin.speed_fb),
                    instance->data.speed_float,
                    *(instance->hal.pin.speed_prediction),
                    *(instance->hal.pin.position_prediction));
            }
        }
    }
}
