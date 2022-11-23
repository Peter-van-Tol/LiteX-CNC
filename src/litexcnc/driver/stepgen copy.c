


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
            float period_s = litexcnc->stepgen.memo.period_s;
            float period_s_recip = 1.0 / period_s;

            // Determine what the average speed is required during this cycle. It is assumed
            // that this speed will be constant in the next cycle, so we aim to have this speed
            // at the start of the next cycle. The speed is based on the commanded movements,
            // not the actual speeds (which may be higher or lower because of error correction)
            float speed_cmd = (*(instance->hal.pin.position_cmd) - instance->memo.position_cmd) * period_s_recip;

            // From the speed command, determine what the average acceleration should be to 
            // overcome the speed difference within one cycle. We check whether this parameter
            // obeys the acceleration limits. If these limits are exceeded, we stretch the cycle
            // by updating the variable period_s
            float acc_cmd = (speed_cmd - *(instance->hal.pin.speed_prediction)) * period_s_recip;
            if (acc_cmd > instance->hal.param.maxaccel) {
                period_s = period_s * acc_cmd * instance->data.acc_recip;
                period_s_recip = 1.0 / period_s;
                if (*(instance->hal.pin.debug)) {
                    LITEXCNC_PRINT_NO_DEVICE("Stretching %.6f, %.6f\n", 
                        period_s,
                        acc_cmd
                    );
                }
                acc_cmd = instance->hal.param.maxaccel;
            } else if (acc_cmd < (-1 * instance->hal.param.maxaccel)) {
                period_s = -1 * period_s * acc_cmd * instance->data.acc_recip;
                period_s_recip = 1.0 / period_s;
                if (*(instance->hal.pin.debug)) {
                    LITEXCNC_PRINT_NO_DEVICE("Stretching %.6f, %.6f\n", 
                        period_s,
                        acc_cmd
                    );
                }
                acc_cmd = -1 * instance->hal.param.maxaccel;
            }

            // Calculate the estimated position where we expect to end up when applying 
            // the calculated acceleration. We compare this with the commanded position 
            // to determine the estimated error.
            float pos_estimated = *(instance->hal.pin.position_prediction) + 0.5 * (*(instance->hal.pin.speed_prediction) + speed_cmd) * period_s;
            float pos_error = pos_estimated - *(instance->hal.pin.position_cmd);

            // Calculate how much error we can correct within 1 cycle with the maximum
            // acceleration. First the time is calculated where we switch from maximum
            // accelaration to maximum deceleration. Im this calculation it is assumed
            // that pos_error is negative, thus meaning we have to accelerate first to
            // compensate. Due to symmetry the maximum is also valid for the case where
            // pos_error is positive. In the latter case, we have to remember to switch 
            // signs of the acceleration and flip the switch-time.
            float switch_fraction = (0.5 + 0.5 * (acc_cmd * instance->data.acc_recip));
            float max_compensation = 0.5 * (switch_fraction * period_s * (instance->hal.param.maxaccel - acc_cmd)) * period_s;

            // Check whether we are within the bounds of the maximum compensation if the
            // maximum compensation is exceeded, we have to stretch the cycle more. When
            // the maximum compensation is not exceed, the acceleration is scaled.
            if ((pos_error > max_compensation) || (pos_error < (-1 * max_compensation))) {
                // Different tactics: determine the time to accelerate to the maximum speed
                // and determine the error after the standard period. This error has to be
                // compensated after the initial speed has been reached by extending the
                // acceleration phase with `time_ext`.
                float time_acc = fabs(speed_cmd - *(instance->hal.pin.speed_prediction)) * instance->data.acc_recip;
                // pos_estimated = *(instance->hal.pin.position_prediction) + 0.5 * (speed_cmd + *(instance->hal.pin.speed_prediction)) * time_acc + speed_cmd * (period_s - time_acc);
                pos_estimated = *(instance->hal.pin.position_prediction) + 0.5 * (speed_cmd + *(instance->hal.pin.speed_prediction)) * time_acc;
                if (time_acc < period_s) {
                    // The acceleration phase is shorter then period, take into account the
                    // constant velocity phase.
                    pos_estimated += speed_cmd * (period_s - time_acc);
                }
                pos_error = pos_estimated - *(instance->hal.pin.position_cmd);
                if (time_acc > period_s) {
                    // The acceleration time is extended outward. Assume constant velocity
                    pos_error -= speed_cmd * (time_acc - period_s);
                }
                float time_ext = sqrt(fabs(pos_error) * instance->data.acc_recip);
                if (*(instance->hal.pin.debug)) {
                    LITEXCNC_PRINT_NO_DEVICE("%.6f, %.6f, %.6f, %.6f\n",
                        pos_estimated,
                        pos_error,
                        time_acc,
                        time_ext
                    );
                }
                // Determine the signs of acceleration (start with speed up or slow down?)
                int8_t sign = 1;
                if (pos_error > 0) {
                    sign = -1;
                }
                // Store it on the data-frame
                instance->data.acc1 = sign * instance->hal.param.maxaccel;
                instance->data.time1 = time_acc + time_ext;
                instance->data.speed1 = *(instance->hal.pin.speed_prediction) + instance->data.time1 * instance->data.acc1;
                instance->data.acc2 = -1 * sign * instance->hal.param.maxaccel;
                instance->data.time2 = instance->data.time1 + time_ext;
                instance->data.speed2 = speed_cmd;
            } else {
                if (*(instance->hal.pin.debug)) {
                    LITEXCNC_PRINT_NO_DEVICE("Normal\n");
                }
                // When the compensation is positive, flip some signs
                int8_t sign = 1;
                if (pos_error > 0) {
                    sign = -1;
                    switch_fraction = 1 - switch_fraction;
                }
                // Scale the acceleration and store it in the data frame
                instance->data.acc1 = acc_cmd + sign * (instance->hal.param.maxaccel - sign * acc_cmd) * (fabs(pos_error) / max_compensation);
                instance->data.time1 = switch_fraction * period_s;
                instance->data.speed1 = *(instance->hal.pin.speed_prediction) + instance->data.time1 * instance->data.acc1;
                instance->data.acc2 = acc_cmd - sign * (instance->hal.param.maxaccel + sign * acc_cmd) * (fabs(pos_error) / max_compensation);
                instance->data.time2 = period_s;
                instance->data.speed2 = speed_cmd;
            }
            // Store this position command for the following loop
            instance->memo.position_cmd = *(instance->hal.pin.position_cmd);
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
uint32_t newAvg = 0;
uint64_t sum = 0;
uint32_t arrNumbers[10] = {0};
size_t len = sizeof(arrNumbers) / sizeof(uint32_t);

uint32_t movingAvg(uint32_t *ptrArrNumbers, uint64_t *ptrSum, size_t pos, size_t len, uint32_t nextNum)
{
  //Subtract the oldest number from the prev sum, add the new number
  *ptrSum = *ptrSum - ptrArrNumbers[pos] + nextNum;
  //Assign the nextNum to the position in the array
  ptrArrNumbers[pos] = nextNum;
  //return the average
  return *ptrSum / len;
}

uint8_t litexcnc_stepgen_process_read(litexcnc_t *litexcnc, uint8_t** data, long period) {

    // Initialisation for the previous wall clock
    if (litexcnc->stepgen.memo.prev_wall_clock == 0) {
        // Setup teh wall clock to be equal to the current wall-clock minus
        // the idealised timing
        litexcnc->stepgen.memo.period_s = 1e-9 * period;
        litexcnc->stepgen.memo.cylces_per_period = litexcnc->stepgen.memo.period_s * litexcnc->clock_frequency;
        litexcnc->stepgen.memo.prev_wall_clock = litexcnc->wallclock->memo.wallclock_ticks - litexcnc->stepgen.memo.cylces_per_period;
        litexcnc->stepgen.memo.apply_time = litexcnc->stepgen.memo.prev_wall_clock + 0.9 * litexcnc->stepgen.memo.cylces_per_period;
        // Initialize the running average
        for (size_t i=0; i<len; i++ ){
            arrNumbers[i] = litexcnc->stepgen.memo.cylces_per_period;
            sum += litexcnc->stepgen.memo.cylces_per_period;
        }
    }

    // The next apply time is basically chosen so that the next loop starts exactly when it
    // should (according to the timing of the previous loop). When due to latency excursion
    // this timing cannot be met (outside the bounds or already in the past) this variable
    // is modified.
    uint64_t next_apply_time = (double) litexcnc->stepgen.memo.apply_time + litexcnc->stepgen.memo.period_s * litexcnc->clock_frequency;

    // Determine how many clicks have been passed from the previous loop
    int32_t loop_cycles = litexcnc->wallclock->memo.wallclock_ticks - litexcnc->stepgen.memo.prev_wall_clock;

    // Check whether the loop-cycles are within the limits
    if (loop_cycles < (0.9 * litexcnc->stepgen.memo.cylces_per_period)) {
        // The previous loop was way too short
        next_apply_time += (loop_cycles - 0.9 * litexcnc->stepgen.memo.cylces_per_period);
        loop_cycles = 0.9 * litexcnc->stepgen.memo.cylces_per_period;
    } else if (loop_cycles > (1.1 * litexcnc->stepgen.memo.cylces_per_period)) {
        // The previous loop was way too long, possibly a latency excursion
        next_apply_time += (loop_cycles - 1.1 * litexcnc->stepgen.memo.cylces_per_period);
        loop_cycles = 1.1 * litexcnc->stepgen.memo.cylces_per_period;
    }

    // Calculate the period in seconds to use for the next step and store the wall clock for
    // next loop
    litexcnc->stepgen.memo.period_s = movingAvg(arrNumbers, &sum, pos, len, loop_cycles) * litexcnc->clock_frequency_recip;
    pos++;
    if (pos >= len){
      pos = 0;
    }
    // DEBU: print the calculated vaLues
    rtapi_print("Timings: %llu, %llu, %lu, %.6f, %llu\n", 
        litexcnc->wallclock->memo.wallclock_ticks, 
        litexcnc->stepgen.memo.prev_wall_clock, 
        loop_cycles,
        litexcnc->stepgen.memo.period_s,
        next_apply_time
    );
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
        if (litexcnc->wallclock->memo.wallclock_ticks < litexcnc->stepgen.memo.apply_time + instance->data.fpga_time1) {
            min_time = litexcnc->wallclock->memo.wallclock_ticks;
            if (litexcnc->stepgen.memo.apply_time > min_time) {
                min_time = litexcnc->stepgen.memo.apply_time;
            }
            max_time = litexcnc->stepgen.memo.apply_time + instance->data.fpga_time1;
            if (next_apply_time < max_time) {
                max_time = next_apply_time;
            }
            fraction = (float) (max_time - min_time) / (litexcnc->stepgen.memo.apply_time + instance->data.fpga_time1 - min_time);
            speed_end = (1.0 - fraction) * *(instance->hal.pin.speed_prediction) + fraction * instance->data.speed1;
            *(instance->hal.pin.position_prediction) += 0.5 * (*(instance->hal.pin.speed_prediction) + speed_end) * (max_time - min_time) * litexcnc->clock_frequency_recip;
            *(instance->hal.pin.speed_prediction) = speed_end;
            // if (*(instance->hal.pin.debug)) {
            //     rtapi_print("Predict position - phase 1:  %.6f, %.6f, %.6f, %llu, %llu \n", *(instance->hal.pin.position_prediction), *(instance->hal.pin.speed_prediction), fraction1, min_time, max_time);
            // }
        }
        if ((litexcnc->wallclock->memo.wallclock_ticks < litexcnc->stepgen.memo.apply_time + instance->data.fpga_time2) && (litexcnc->stepgen.memo.apply_time + instance->data.fpga_time1 < next_apply_time)) {
            min_time = litexcnc->wallclock->memo.wallclock_ticks;
            if ((litexcnc->stepgen.memo.apply_time + instance->data.fpga_time1) > min_time) {
                *(instance->hal.pin.speed_prediction) = instance->data.speed1;
                min_time = litexcnc->stepgen.memo.apply_time + instance->data.fpga_time1;
            }
            max_time = litexcnc->stepgen.memo.apply_time + instance->data.fpga_time2;
            if (next_apply_time < max_time) {
                max_time = next_apply_time;
            }
            fraction = (float) (max_time - min_time) / (litexcnc->stepgen.memo.apply_time + instance->data.fpga_time2 - min_time);
            speed_end = (1.0 - fraction) * *(instance->hal.pin.speed_prediction) + fraction * instance->data.speed2;
            *(instance->hal.pin.position_prediction) += 0.5 * (*(instance->hal.pin.speed_prediction) + speed_end) * (max_time - min_time) * litexcnc->clock_frequency_recip;
            *(instance->hal.pin.speed_prediction) = speed_end;
            // if (*(instance->hal.pin.debug)) {
            //     rtapi_print("Predict position - phase 2: %.6f, %.6f, %.6f, %llu, %llu, %llu, %lu, %lu \n", *(instance->hal.pin.position_prediction), *(instance->hal.pin.speed_prediction), fraction2, max_time,  min_time, (litexcnc->stepgen.memo.apply_time + instance->data.fpga_time2), instance->data.fpga_time1, instance->data.fpga_time2);
            // }
        }
        if (next_apply_time >= litexcnc->stepgen.memo.apply_time + instance->data.fpga_time2) {
            // Some constant speed should be added
            *(instance->hal.pin.speed_prediction) = instance->data.speed2;
            *(instance->hal.pin.position_prediction) += instance->data.speed2 * (next_apply_time - (litexcnc->stepgen.memo.apply_time + instance->data.fpga_time2)) * litexcnc->clock_frequency_recip;
            // if (*(instance->hal.pin.debug)) {
            //     rtapi_print("Predict position - constant speed: %.6f, %.6f\n", *(instance->hal.pin.position_prediction), *(instance->hal.pin.speed_prediction));
            // }
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
