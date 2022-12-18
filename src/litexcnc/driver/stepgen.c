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
#include "litexcnc.h"
#include "stepgen.h"

int litexcnc_stepgen_init(litexcnc_t *litexcnc, json_object *config) {

    // Declarations
    int r = 0;
    struct json_object *stepgen_config;
    struct json_object *instance_config;
    struct json_object *instance_config_name_field;
    char base_name[HAL_NAME_LEN + 1];   // i.e. <board_name>.<board_index>.stepgen.<stepgen_name>

    // Allocate the module-global HAL shared memory
    litexcnc->stepgen.hal = (litexcnc_stepgen_hal_t*)hal_malloc(sizeof(litexcnc_stepgen_hal_t));
    if (litexcnc->stepgen.hal == NULL) {
        rtapi_print("No memory");
        LITEXCNC_ERR_NO_DEVICE("Out of memory!\n");
        r = -ENOMEM;
        return r;
    }
    // - period and reciprocal
    r = hal_pin_float_newf(HAL_OUT, &(litexcnc->stepgen.hal->pin.period_s), litexcnc->fpga->comp_id, "%s.stepgen.period-s", litexcnc->fpga->name);
    if (r != 0) { goto fail_pins; }
    r = hal_pin_float_newf(HAL_OUT, &(litexcnc->stepgen.hal->pin.period_s_recip), litexcnc->fpga->comp_id, "%s.stepgen.period-s-recip", litexcnc->fpga->name);
    if (r != 0) { goto fail_pins; }
    
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
            int8_t shift = 0;
            while (litexcnc->clock_frequency / (1 << shift) > 400e3)
                shift += 1;
            instance->data.pick_off_pos = 32;
            instance->data.pick_off_vel = instance->data.pick_off_pos + shift;
            instance->data.pick_off_acc = instance->data.pick_off_vel + 8;

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
            r = hal_param_float_newf(HAL_RW, &(instance->hal.param.max_acceleration), litexcnc->fpga->comp_id, "%s.max-acceleration", base_name);
            if (r != 0) { goto fail_params; }
            // - Maximum velocity
            r = hal_param_float_newf(HAL_RW, &(instance->hal.param.max_velocity), litexcnc->fpga->comp_id, "%s.max-velocity", base_name);
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
            // - Flag setting whether debug is active
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
            r = hal_pin_float_newf(HAL_OUT, &(instance->hal.pin.speed_fb), litexcnc->fpga->comp_id, "%s.velocity-feedback", base_name);
            if (r != 0) { goto fail_pins; }
            // - speed_prediction
            r = hal_pin_float_newf(HAL_OUT, &(instance->hal.pin.speed_prediction), litexcnc->fpga->comp_id, "%s.velocity-prediction", base_name);
            if (r != 0) { goto fail_pins; }
            // - enable
            r = hal_pin_bit_newf(HAL_IN, &(instance->hal.pin.enable), litexcnc->fpga->comp_id, "%s.enable", base_name);
            if (r != 0) { goto fail_pins; }
            // - velocity_cmd
            r = hal_pin_float_newf(HAL_IN, &(instance->hal.pin.velocity_cmd), litexcnc->fpga->comp_id, "%s.velocity-cmd", base_name);
            if (r != 0) { goto fail_pins; }
            // - acceleration_cmd
            r = hal_pin_float_newf(HAL_IN, &(instance->hal.pin.acceleration_cmd), litexcnc->fpga->comp_id, "%s.acceleration-cmd", base_name);
            if (r != 0) { goto fail_pins; }
            // // - period and reciprocal
            // r = hal_pin_float_newf(HAL_OUT, &(instance->hal.pin.period_s), litexcnc->fpga->comp_id, "%s.period-s", base_name);
            // if (r != 0) { goto fail_pins; }
            // r = hal_pin_float_newf(HAL_OUT, &(instance->hal.pin.period_s_recip), litexcnc->fpga->comp_id, "%s.period-s-recip", base_name);
            // if (r != 0) { goto fail_pins; }
        }
    }
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
    
    // Initialize the averaging of the FPGA wall-clock. This is the first loop, so
    // the array is filled with 'ideal' data based on the period.
    *(litexcnc->stepgen.hal->pin.period_s) = 1e-9 * period;
    *(litexcnc->stepgen.hal->pin.period_s_recip) = 1.0f / *(litexcnc->stepgen.hal->pin.period_s);
    litexcnc->stepgen.memo.cycles_per_period = *(litexcnc->stepgen.hal->pin.period_s) * litexcnc->clock_frequency;
    // Initialize the running average
    for (size_t i=0; i<STEPGEN_WALLCLOCK_BUFFER; i++){
        litexcnc->stepgen.data.wallclock_buffer[i] = (double) *(litexcnc->stepgen.hal->pin.period_s);
        litexcnc->stepgen.data.wallclock_buffer_sum += *(litexcnc->stepgen.hal->pin.period_s);
    }

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

    for (size_t i=0; i<litexcnc->stepgen.num_instances; i++) {
        // Get pointer to the stepgen instance
        litexcnc_stepgen_pin_t *instance = &(litexcnc->stepgen.instances[i]);

        // Calculate the timings
        // - steplen
        instance->data.steplen_cycles = ceil((float) instance->hal.param.steplen * litexcnc->clock_frequency * 1e-9);
        instance->memo.steplen = instance->hal.param.steplen; 
        if (instance->data.steplen_cycles > steplen_cycles) {steplen_cycles = instance->data.steplen_cycles;};
        // - stepspace
        instance->data.stepspace_cycles = ceil((float) instance->hal.param.stepspace * litexcnc->clock_frequency * 1e-9);
        instance->memo.stepspace = instance->hal.param.stepspace; 
        if (instance->data.stepspace_cycles > stepspace_cycles) {stepspace_cycles = instance->data.stepspace_cycles;};
        // - dir_hold_time
        instance->data.dirhold_cycles = ceil((float) instance->hal.param.dir_hold_time * litexcnc->clock_frequency * 1e-9);
        instance->memo.dir_hold_time = instance->hal.param.dir_hold_time; 
        if (instance->data.dirhold_cycles > dirhold_cycles) {dirhold_cycles = instance->data.dirhold_cycles;};
        // - dir_setup_time
        instance->data.dirsetup_cycles = ceil((float) instance->hal.param.dir_setup_time * litexcnc->clock_frequency * 1e-9);
        instance->memo.dir_setup_time = instance->hal.param.dir_setup_time; 
        if (instance->data.dirsetup_cycles > dirsetup_cycles) {dirsetup_cycles = instance->data.dirsetup_cycles;};
    }

    // Calculate the maximum frequency for stepgen (in if statement to prevent division)
    if ((litexcnc->stepgen.memo.stepspace_cycles != stepspace_cycles) || (litexcnc->stepgen.memo.steplen_cycles != steplen_cycles)) {
        litexcnc->stepgen.data.max_frequency = (double) litexcnc->clock_frequency / (steplen_cycles + stepspace_cycles);
        litexcnc->stepgen.memo.steplen_cycles = steplen_cycles;
        litexcnc->stepgen.memo.stepspace_cycles = stepspace_cycles;
    }

    // Convert the general data to the correct byte order
    // - check whether the parameters fits in the space
    if (steplen_cycles >= 1 << 11) {
        LITEXCNC_ERR("Parameter `steplen` too large and is clipped. Consider lowering the frequency of the FPGA.\n", litexcnc->fpga->name);
        steplen_cycles = (1 << 11) - 1;
    }
    if (dirhold_cycles >= 1 << 11) {
        LITEXCNC_ERR("Parameter `dir_hold_time` too large and is clipped. Consider lowering the frequency of the FPGA.\n", litexcnc->fpga->name);
        dirhold_cycles = (1 << 11) - 1;
    }
    if (dirsetup_cycles >= 1 << 13) {
        LITEXCNC_ERR("Parameter `dir_setup_time` too large and is clipped. Consider lowering the frequency of the FPGA.\n", litexcnc->fpga->name);
        dirsetup_cycles = (1 << 13) - 1;
    }
    // - convert the timings to the data to be sent to the FPGA
    config_data.stepdata = htobe32(steplen_cycles << 22 + dirhold_cycles << 12+ dirsetup_cycles << 0);

    // Put the data on the data-stream and advance the pointer
    memcpy(*data, &config_data, LITEXCNC_STEPGEN_CONFIG_DATA_SIZE);
    *data += LITEXCNC_STEPGEN_CONFIG_DATA_SIZE;

}


uint8_t litexcnc_stepgen_prepare_write(litexcnc_t *litexcnc, uint8_t **data, long period) {

    // Declarations
    static litexcnc_stepgen_general_write_data_t data_general;
    static litexcnc_stepgen_pin_t *instance;
    static litexcnc_stepgen_instance_write_data_t instance_data;

    // STEP 1: Timing
    // ==============
    // Put the data on the data-stream and advance the pointer
    data_general.apply_time = htobe64(litexcnc->stepgen.memo.apply_time);
    memcpy(*data, &data_general, LITEXCNC_STEPGEN_GENERAL_WRITE_DATA_SIZE);
    *data += LITEXCNC_STEPGEN_GENERAL_WRITE_DATA_SIZE;

    // STEP 2: Speed per stepgen
    // =========================
    for (size_t i=0; i<litexcnc->stepgen.num_instances; i++) {
        // Get pointer to the stepgen instance
        instance = &(litexcnc->stepgen.instances[i]);

        // Throw error when timings are changed
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

        // Limit the speed to the maximum speed (both phases)
        if (*(instance->hal.pin.velocity_cmd) > instance->hal.param.max_velocity) {
            *(instance->hal.pin.velocity_cmd) = instance->hal.param.max_velocity;
        } else if (*(instance->hal.pin.velocity_cmd) < (-1 * instance->hal.param.max_velocity)) {
            *(instance->hal.pin.velocity_cmd) = -1 * instance->hal.param.max_velocity;
        }

        // Limit the acceleration to the maximum acceleration (both phases). The acceleration
        // should be positive
        if (*(instance->hal.pin.acceleration_cmd) < 0) {
            *(instance->hal.pin.acceleration_cmd) = -1 * *(instance->hal.pin.acceleration_cmd);
        }
        if (*(instance->hal.pin.acceleration_cmd) > instance->hal.param.max_acceleration) {
            *(instance->hal.pin.acceleration_cmd) = instance->hal.param.max_acceleration;
        }

        // The data being send to the FPGA (as calculated) in units and seconds
        instance->data.flt_speed = *(instance->hal.pin.velocity_cmd);
        instance->data.flt_acc   = *(instance->hal.pin.acceleration_cmd);
        instance->data.flt_time  = fabs((*(instance->hal.pin.velocity_cmd) - *(instance->hal.pin.speed_prediction)) / *(instance->hal.pin.acceleration_cmd));

        // Calculate the time spent accelerating in steps and clock cycles
        instance->data.fpga_speed = (int64_t) (instance->data.flt_speed * instance->data.fpga_speed_scale) + 0x80000000;
        instance->data.fpga_acc = instance->data.flt_acc * instance->data.fpga_acc_scale;
        instance->data.fpga_time = instance->data.flt_time * litexcnc->clock_frequency;

        // Convert the integers used and scale it to the FPGA
        instance_data.speed_target = htobe32(instance->data.fpga_speed);
        instance_data.acceleration = htobe32(instance->data.fpga_acc);

        // Put the data on the data-stream and advance the pointer
        memcpy(*data, &instance_data, LITEXCNC_STEPGEN_INSTANCE_WRITE_DATA_SIZE);
        *data += LITEXCNC_STEPGEN_INSTANCE_WRITE_DATA_SIZE;

        if (*(instance->hal.pin.debug)) {
            LITEXCNC_PRINT_NO_DEVICE("Stepgen: data sent to FPGA %llu, %llu, %lu, %lu, %lu \n", 
                litexcnc->wallclock->memo.wallclock_ticks,
                litexcnc->stepgen.memo.apply_time,
                instance->data.fpga_speed,
                instance->data.fpga_acc,
                instance->data.fpga_time
            );
        }
    }
}

float movingAvg(float *ptrArrNumbers, float *ptrSum, size_t pos, size_t len, float nextNum)
{
  //Subtract the oldest number from the prev sum, add the new number
  *ptrSum = *ptrSum - ptrArrNumbers[pos] + nextNum;
  //Assign the nextNum to the position in the array
  ptrArrNumbers[pos] = nextNum;
  //return the average
  return *ptrSum * STEPGEN_WALLCLOCK_BUFFER_RECIP;
}

uint8_t litexcnc_stepgen_process_read(litexcnc_t *litexcnc, uint8_t** data, long period) {

    // Declarations
    static uint64_t next_apply_time;
    static int32_t loop_cycles;
    static litexcnc_stepgen_pin_t *instance;
    //  - parameters for retrieving data from FPGA
    static int64_t pos;
    static uint32_t speed;
    static int64_t tmp_apply_time;
    // - parameters for determining the position end start of next loop
    static uint64_t min_time;
    static uint64_t max_time;
    static float fraction;
    static float speed_end;

    // Check for the first cycle and calculate some fake timings. This has to be done at
    // this location, because in the init the wallclock_ticks is still zero and this would
    // lead to an underflow.
    if (litexcnc->stepgen.memo.apply_time == 0) {
        litexcnc->stepgen.memo.prev_wall_clock = litexcnc->wallclock->memo.wallclock_ticks - litexcnc->stepgen.memo.cycles_per_period;
        litexcnc->stepgen.memo.apply_time = litexcnc->stepgen.memo.prev_wall_clock + 0.9 * litexcnc->stepgen.memo.cycles_per_period;
    }

    // The next apply time is basically chosen so that the next loop starts exactly when it
    // should (according to the timing of the previous loop). When due to latency excursion
    // this timing cannot be met (outside the bounds or already in the past) this variable
    // is modified. An additional 0.5 is added to prevent rounding errors accumulate over
    // time.
    next_apply_time = (double) litexcnc->stepgen.memo.apply_time + *(litexcnc->stepgen.hal->pin.period_s) * litexcnc->clock_frequency + 0.5;

    // Determine how many clicks have been passed from the previous loop
    loop_cycles = litexcnc->wallclock->memo.wallclock_ticks - litexcnc->stepgen.memo.prev_wall_clock;

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
        rtapi_print("Apply time exceeding limits (too short): %llu, %llu, %llu \n",
            litexcnc->wallclock->memo.wallclock_ticks,
            litexcnc->stepgen.memo.apply_time,
            next_apply_time
        );  
        next_apply_time = litexcnc->wallclock->memo.wallclock_ticks + 0.85 * litexcnc->stepgen.memo.cycles_per_period;
        rtapi_print("%llu \n", next_apply_time);
        // Show warning
        if (!litexcnc->stepgen.data.warning_apply_time_exceeded_shown) {
            LITEXCNC_ERR_NO_DEVICE("Apply time exceeded limits.");
            litexcnc->stepgen.data.warning_apply_time_exceeded_shown = true;
        }
    }
    if (next_apply_time > litexcnc->wallclock->memo.wallclock_ticks + 0.99 * litexcnc->stepgen.memo.cycles_per_period){
        rtapi_print("Apply time exceeding limits (too long): %llu, %llu, %llu \n",
            litexcnc->wallclock->memo.wallclock_ticks,
            litexcnc->stepgen.memo.apply_time,
            next_apply_time
        );     
        next_apply_time = litexcnc->wallclock->memo.wallclock_ticks + 0.95 * litexcnc->stepgen.memo.cycles_per_period;
        // Show warning
        if (!litexcnc->stepgen.data.warning_apply_time_exceeded_shown) {
            LITEXCNC_ERR_NO_DEVICE("Apply time exceeded limits.");
            litexcnc->stepgen.data.warning_apply_time_exceeded_shown = true;
        }
    }

    // Calculate the period in seconds to use for the next step and store the wall clock for
    // next loop
    *(litexcnc->stepgen.hal->pin.period_s) = movingAvg(
        litexcnc->stepgen.data.wallclock_buffer, 
        &(litexcnc->stepgen.data.wallclock_buffer_sum), 
        litexcnc->stepgen.data.wallclock_buffer_pos, 
        STEPGEN_WALLCLOCK_BUFFER, 
        (double) loop_cycles * litexcnc->clock_frequency_recip);
    *(litexcnc->stepgen.hal->pin.period_s_recip) = 1.0f /  *(litexcnc->stepgen.hal->pin.period_s);
    litexcnc->stepgen.data.wallclock_buffer_pos++;
    if (litexcnc->stepgen.data.wallclock_buffer_pos >= STEPGEN_WALLCLOCK_BUFFER) {
      litexcnc->stepgen.data.wallclock_buffer_pos = 0;
    }
    litexcnc->stepgen.memo.prev_wall_clock = litexcnc->wallclock->memo.wallclock_ticks;

    // Receive and process the data for all the stepgens
    for (size_t i=0; i<litexcnc->stepgen.num_instances; i++) {
        // Get pointer to the stepgen instance
        instance = &(litexcnc->stepgen.instances[i]);

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
            instance->data.fpga_pos_scale_inv = (float) instance->data.scale_recip / (1LL << instance->data.pick_off_pos);
            instance->data.fpga_speed_scale = (float) (instance->hal.param.position_scale * litexcnc->clock_frequency_recip) * (1LL << instance->data.pick_off_vel);
            instance->data.fpga_speed_scale_inv = 1.0f / instance->data.fpga_speed_scale;
            instance->data.fpga_acc_scale = (float) (instance->hal.param.position_scale * litexcnc->clock_frequency_recip * litexcnc->clock_frequency_recip) * (1LL << (instance->data.pick_off_acc));
            instance->data.fpga_acc_scale_inv =  (float) instance->data.scale_recip * litexcnc->clock_frequency * litexcnc->clock_frequency / (1LL << instance->data.pick_off_acc);;
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
        *(instance->hal.pin.counts) = instance->data.position >> instance->data.pick_off_pos;
        // Check: why is a half step subtracted from the position. Will case a possible problem 
        // when the power is cycled -> will lead to a moving reference frame  
        // *(instance->hal.pin.position_fb) = (double)(instance->data.position-(1LL<<(instance->data.pick_off_pos-1))) * instance->data.scale_recip / (1LL << instance->data.pick_off_pos);
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
            rtapi_print("Timings: %.6f, %llu, %llu, %lu, %llu \n",
                *(litexcnc->stepgen.hal->pin.period_s),
                litexcnc->wallclock->memo.wallclock_ticks,
                litexcnc->stepgen.memo.apply_time,
                instance->data.fpga_time,
                next_apply_time
            );
        }
        if (litexcnc->wallclock->memo.wallclock_ticks <= litexcnc->stepgen.memo.apply_time + instance->data.fpga_time) {
            min_time = litexcnc->wallclock->memo.wallclock_ticks;
            if (litexcnc->stepgen.memo.apply_time > min_time) {
                min_time = litexcnc->stepgen.memo.apply_time;
            }
            max_time = litexcnc->stepgen.memo.apply_time + instance->data.fpga_time;
            if (next_apply_time < max_time) {
                max_time = next_apply_time;
            }
            if ((litexcnc->stepgen.memo.apply_time + instance->data.fpga_time - min_time) <= 0) {
                fraction = 1.0;
            } else {
                fraction = (float) (max_time - min_time) / (litexcnc->stepgen.memo.apply_time + instance->data.fpga_time - min_time);
            }
            speed_end = (1.0 - fraction) * *(instance->hal.pin.speed_prediction) + fraction * instance->data.flt_speed;
            *(instance->hal.pin.position_prediction) += 0.5 * (*(instance->hal.pin.speed_prediction) + speed_end) * (max_time - min_time) * litexcnc->clock_frequency_recip;
            *(instance->hal.pin.speed_prediction) = speed_end;
        }
        if (next_apply_time > litexcnc->stepgen.memo.apply_time + instance->data.fpga_time) {
            // Some constant speed should be added
            *(instance->hal.pin.speed_prediction) = instance->data.flt_speed;
            *(instance->hal.pin.position_prediction) += instance->data.flt_speed * (next_apply_time - (litexcnc->stepgen.memo.apply_time + instance->data.fpga_time)) * litexcnc->clock_frequency_recip;
        }
        if (*(instance->hal.pin.debug)) {
            rtapi_print("Stepgen speed feedback result: %llu, %llu, %.6f, %.6f, %.6f, %.6f \n",
                litexcnc->wallclock->memo.wallclock_ticks,
                next_apply_time,
                *(instance->hal.pin.speed_fb),
                *(instance->hal.pin.speed_prediction),
                *(instance->hal.pin.position_fb),
                *(instance->hal.pin.position_prediction)
            );
        }
    }

    // Push the apply for the write loop
    litexcnc->stepgen.memo.apply_time = next_apply_time;
}
