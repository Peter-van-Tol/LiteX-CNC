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

#include <litexcnc.h>

#define LITEXCNC_PWM_NAME "litexcnc_pwm"

#define LITEXCNC_PWM_VERSION_MAJOR 1
#define LITEXCNC_PWM_VERSION_MINOR 0
#define LITEXCNC_PWM_VERSION_PATCH 0

#define MAX_INSTANCES 4

/** The ID of the component, only used when the component is used as stand-alone */
int comp_id;

/*******************************************************************************
 * STUCTS
 ******************************************************************************/

/** Structure of an PWM instance */
typedef struct {
    /** Structure defining the HAL pin and params*/
      struct {
        /** Structure defining the HAL pins */
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
        /** Structure defining the HAL params */
        struct {
            hal_float_t scale_recip;    /* Reciprocal of the scale, not exported */
            hal_float_t period_recip;   /* Reciprocal of the width, not exported */
        } param;

    } hal;
    /** This struct holds all old values (memoization) */
    struct {
        double scale;
        double pwm_freq;
    } memo;
} litexcnc_pwm_instance_t;


/** Structure of the PWM module */
typedef struct {
    int num_instances;                  /** Number of pwm instances */
    litexcnc_pwm_instance_t *instances; /** Structure containing the data on the pwm instances */
    /** This struct holds pointers to data from the FPGA */
    struct {
        uint32_t *clock_frequency;
    } data;
} litexcnc_pwm_t;


/** Structure of the data which is send to the FPGA */
#pragma pack(push,4)
typedef struct {
    // Input pins
    uint32_t period;
    uint32_t width;
} litexcnc_pwm_data_t;
#pragma pack(pop)


/*******************************************************************************
 * FUNCTIONS
 ******************************************************************************/
 
/*******************************************************************************
 * Function which is called when a user adds the component using `loadrt 
 * litexcnc_gpio`. It will initialize the component with LinuxCNC and registers
 * the exposed init, read and write functions with LitexCNC so they can be used
 * by the driver. 
 ******************************************************************************/
int rtapi_app_main(void);

/*******************************************************************************
 * Function which is called when the realtime application is stopped (i.e. when
 * LinuxCNC is stopped).
 ******************************************************************************/
void rtapi_app_exit(void);


/*******************************************************************************
 * Initializes the component for the given FPGA.
 *
 * @param instance Pointer to the struct with the module instance data. This 
 * function will initialise this struct.
 * @param fpga_name The name of the FPGA, used to set the pin names correctly.
 * @param config The configuration of the FPGA, used to set the pin names. 
 ******************************************************************************/
size_t litexcnc_pwm_init(
    litexcnc_module_instance_t **instance, 
    litexcnc_t *litexcnc,
    uint8_t **config
);


/*******************************************************************************
 * Returns the required buffer size for the config (not used for GPIO)
 *
 * @param instance The structure containing the data on the module instance
 ******************************************************************************/
size_t required_config_buffer(void *instance);


/*******************************************************************************
 * Returns the required buffer size for the write buffer
 *
 * @param instance The structure containing the data on the module instance
 ******************************************************************************/
size_t required_write_buffer(void *instance);


/*******************************************************************************
 * Returns the required buffer size for the read buffer
 *
 * @param instance The structure containing the data on the module instance
 ******************************************************************************/
size_t required_read_buffer(void *instance);


/*******************************************************************************
 * Prepares the data to be written out to the device
 *
 * @param instance The structure containing the data on the module instance
 * @param data Pointer to the array where the data should be written to. NOTE:
 *     the pointer should moved to the next element, so the next module can 
 *     append its data. All data in LitexCNC is 4-bytes wide. 
 * @param period Period in nano-seconds of a cycle (not used for PWM)
 ******************************************************************************/
int litexcnc_pwm_prepare_write(void *instance, uint8_t **data, int period);


/*******************************************************************************
 * Processes the data which has been received from a device
 *
 * @param instance The structure containing the data on the module instance
 * @param data Pointer to the array where the data is contained in. NOTE:
 *     the pointer should moved to the next element, so the next module can 
 *     process its data. All data in LitexCNC is 4-bytes wide. 
 * @param period Period in nano-seconds of a cycle (not used for PWM)
 ******************************************************************************/
int litexcnc_pwm_process_read(void *instance, uint8_t** data, int period);

#endif
