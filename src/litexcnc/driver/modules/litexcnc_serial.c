/********************************************************************
* Description:  litexcnc_serial.c
*               A Litex-CNC component for serial communication (UART)
*               with FIFO buffers and RS485 support.
*
* Author: Based on LitexCNC architecture
* License: GPL Version 2
*    
* Copyright (c) 2025 All rights reserved.
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

#include "hal.h"
#include "rtapi.h"
#include "rtapi_app.h"
#include "rtapi_string.h"

#include "litexcnc_serial.h"

/** 
 * An array holding all instance for the module. As each board normally has a 
 * single instance of a type, this number coincides with the number of boards
 * which are supported by LitexCNC
 */
static litexcnc_serial_t *instances[MAX_INSTANCES];
static int num_instances = 0;

/**
 * Parameter which contains the registration of this module with LitexCNC 
 */
static litexcnc_module_registration_t *registration;


int register_serial_module(void) {
    registration = (litexcnc_module_registration_t *)hal_malloc(sizeof(litexcnc_module_registration_t));
    registration->id = 0x73657269; /** The string `seri` in hex (serial) */
    rtapi_snprintf(registration->name, sizeof(registration->name), "serial");
    registration->initialize = &litexcnc_serial_init;
    registration->required_write_buffer = &required_write_buffer;
    registration->required_read_buffer  = &required_read_buffer;
    return litexcnc_register_module(registration);
}
EXPORT_SYMBOL_GPL(register_serial_module);


int rtapi_app_main(void) {
    // Show some information on the module being loaded
    LITEXCNC_PRINT_NO_DEVICE(
        "Loading Litex Serial module version %u.%u.%u\n", 
        LITEXCNC_SERIAL_VERSION_MAJOR, 
        LITEXCNC_SERIAL_VERSION_MINOR, 
        LITEXCNC_SERIAL_VERSION_PATCH
    );

    // Initialize the module
    comp_id = hal_init(LITEXCNC_SERIAL_NAME);
    if(comp_id < 0) return comp_id;

    // Register the module with LitexCNC (NOTE: LitexCNC should be loaded first)
    int result = register_serial_module();
    if (result < 0) return result;

    // Report serial module is ready to be used
    hal_ready(comp_id);

    return 0;
}


void rtapi_app_exit(void) {
    hal_exit(comp_id);
    LITEXCNC_PRINT_NO_DEVICE("LitexCNC Serial module driver unloaded\n");
}


size_t required_write_buffer(void *instance) {
    litexcnc_serial_t *serial = (litexcnc_serial_t *) instance;
    
    // Calculate buffer size for all instances:
    // - divisor (4 bytes)
    // - config (4 bytes) 
    // - TX FIFO data (8 x 4 = 32 bytes)
    // - TX control (4 bytes)
    // - RX control (4 bytes)
    // Total per instance: 4 + 4 + 32 + 4 + 4 = 48 bytes
    return serial->num_instances * 48;
}


size_t required_read_buffer(void *instance) {
    litexcnc_serial_t *serial = (litexcnc_serial_t *) instance;
    
    // Calculate buffer size for all instances:
    // - RX FIFO data (8 x 4 = 32 bytes)
    // - TX pointer register (4 bytes)
    // - RX pointer register (4 bytes)
    // - Status register (4 bytes)
    // Total per instance: 32 + 4 + 4 + 4 = 44 bytes
    return serial->num_instances * 44;
}


int litexcnc_serial_prepare_write(void *instance, uint8_t **data, int period) {
    litexcnc_serial_t *serial = (litexcnc_serial_t *) instance;
    
    // Prepare write data for each serial port instance
    for (size_t i = 0; i < serial->num_instances; i++) {
        litexcnc_serial_instance_t *inst = &serial->instances[i];
        
        // 1. Write divisor (4 bytes)
        uint32_t divisor = *(inst->hal.pin.divisor);
        memcpy(*data, &divisor, sizeof(uint32_t));
        *data += 4;
        
        // 2. Write config (4 bytes)
        // Pack configuration: [7:6]=stop_bits, [5]=parity_type, [4]=parity_enable, [3:0]=data_bits
        uint32_t config = 0;
        config |= (*(inst->hal.pin.data_bits) & 0x0F);              // bits [3:0]
        config |= (*(inst->hal.pin.parity_enable) ? (1 << 4) : 0);  // bit [4]
        config |= (*(inst->hal.pin.parity_type) ? (1 << 5) : 0);    // bit [5]
        config |= ((*(inst->hal.pin.stop_bits) & 0x03) << 6);       // bits [7:6]
        memcpy(*data, &config, sizeof(uint32_t));
        *data += 4;
        
        // 3. Write TX FIFO data (32 bytes = 8 x 4 bytes)
        for (size_t j = 0; j < SERIAL_FIFO_REGISTERS; j++) {
            uint32_t tx_word = *(inst->hal.pin.tx_data[j]);
            memcpy(*data, &tx_word, sizeof(uint32_t));
            *data += 4;
        }
        
        // 4. Write TX control (4 bytes)
        uint32_t tx_ctrl = 0;
        tx_ctrl |= (*(inst->hal.pin.tx_load) ? 1 : 0);   // bit 0
        tx_ctrl |= (*(inst->hal.pin.tx_clear) ? 2 : 0);  // bit 1
        memcpy(*data, &tx_ctrl, sizeof(uint32_t));
        *data += 4;
        
        // 5. Write RX control (4 bytes)
        uint32_t rx_ctrl = 0;
        rx_ctrl |= (*(inst->hal.pin.rx_unload) ? 1 : 0);  // bit 0
        rx_ctrl |= (*(inst->hal.pin.rx_clear) ? 2 : 0);   // bit 1
        memcpy(*data, &rx_ctrl, sizeof(uint32_t));
        *data += 4;
        
        // Auto-reset load/unload/clear flags after sending
        *(inst->hal.pin.tx_load) = 0;
        *(inst->hal.pin.tx_clear) = 0;
        *(inst->hal.pin.rx_unload) = 0;
        *(inst->hal.pin.rx_clear) = 0;
    }
    
    return 0;
}


int litexcnc_serial_process_read(void *instance, uint8_t** data, int period) {
    litexcnc_serial_t *serial = (litexcnc_serial_t *) instance;
    
    // Process read data for each serial port instance
    for (size_t i = 0; i < serial->num_instances; i++) {
        litexcnc_serial_instance_t *inst = &serial->instances[i];
        
        // 1. Read RX FIFO data (32 bytes = 8 x 4 bytes)
        for (size_t j = 0; j < SERIAL_FIFO_REGISTERS; j++) {
            uint32_t rx_word;
            memcpy(&rx_word, *data, sizeof(uint32_t));
            *(inst->hal.pin.rx_data[j]) = rx_word;
            *data += 4;
        }
        
        // 2. Read TX pointer register (4 bytes)
        uint32_t tx_ptr;
        memcpy(&tx_ptr, *data, sizeof(uint32_t));
        *(inst->hal.pin.tx_write_ptr) = (tx_ptr >> 0) & 0xFF;   // bits [7:0]
        *(inst->hal.pin.tx_read_ptr) = (tx_ptr >> 8) & 0xFF;    // bits [15:8]
        *(inst->hal.pin.tx_level) = (tx_ptr >> 16) & 0xFF;      // bits [23:16]
        *data += 4;
        
        // 3. Read RX pointer register (4 bytes)
        uint32_t rx_ptr;
        memcpy(&rx_ptr, *data, sizeof(uint32_t));
        *(inst->hal.pin.rx_write_ptr) = (rx_ptr >> 0) & 0xFF;   // bits [7:0]
        *(inst->hal.pin.rx_read_ptr) = (rx_ptr >> 8) & 0xFF;    // bits [15:8]
        *(inst->hal.pin.rx_level) = (rx_ptr >> 16) & 0xFF;      // bits [23:16]
        *data += 4;
        
        // 4. Read status register (4 bytes)
        uint32_t status;
        memcpy(&status, *data, sizeof(uint32_t));
        *(inst->hal.pin.tx_full) = (status & (1 << 0)) ? 1 : 0;
        *(inst->hal.pin.tx_empty) = (status & (1 << 1)) ? 1 : 0;
        *(inst->hal.pin.rx_full) = (status & (1 << 2)) ? 1 : 0;
        *(inst->hal.pin.rx_empty) = (status & (1 << 3)) ? 1 : 0;
        *(inst->hal.pin.tx_active) = (status & (1 << 4)) ? 1 : 0;
        *(inst->hal.pin.data_enable) = (status & (1 << 5)) ? 1 : 0;
        *(inst->hal.pin.parity_error) = (status & (1 << 6)) ? 1 : 0;
        *(inst->hal.pin.frame_error) = (status & (1 << 7)) ? 1 : 0;
        *data += 4;
    }
    
    return 0;
}


static int litexcnc_serial_init_instance(
    litexcnc_serial_instance_t *serial_inst, 
    litexcnc_t *litexcnc, 
    size_t index
) {
    int r;
    char base_name[HAL_NAME_LEN + 1];   // i.e. <board_name>.<board_index>.serial.<index>
    char name[HAL_NAME_LEN + 1];        // i.e. <base_name>.<pin_name>
    
    // Create base name for pins
    LITEXCNC_CREATE_BASENAME("serial", index);
    
    // Configuration pins
    LITEXCNC_CREATE_HAL_PIN("divisor", u32, HAL_IN, &(serial_inst->hal.pin.divisor));
    LITEXCNC_CREATE_HAL_PIN("data-bits", u32, HAL_IN, &(serial_inst->hal.pin.data_bits));
    LITEXCNC_CREATE_HAL_PIN("parity-enable", bit, HAL_IN, &(serial_inst->hal.pin.parity_enable));
    LITEXCNC_CREATE_HAL_PIN("parity-type", bit, HAL_IN, &(serial_inst->hal.pin.parity_type));
    LITEXCNC_CREATE_HAL_PIN("stop-bits", u32, HAL_IN, &(serial_inst->hal.pin.stop_bits));
    
    // TX data pins (8 registers)
    for (size_t i = 0; i < SERIAL_FIFO_REGISTERS; i++) {
        rtapi_snprintf(name, sizeof(name), "%s.tx-data-%zu", base_name, i);
        //r= LITEXCNC_CREATE_HAL_PIN(name,u32,HAL_IN,&(serial_inst->hal.pin.tx_data[i])
        r = hal_pin_u32_new(name, HAL_IN, &(serial_inst->hal.pin.tx_data[i]), litexcnc->fpga->comp_id);
        if (r < 0) {
            LITEXCNC_ERR_NO_DEVICE("Error adding pin '%s', aborting\n", name);
            return r;
        }
    }
    
    // TX control pins
    LITEXCNC_CREATE_HAL_PIN("tx-load", bit, HAL_IN, &(serial_inst->hal.pin.tx_load));
    LITEXCNC_CREATE_HAL_PIN("tx-clear", bit, HAL_IN, &(serial_inst->hal.pin.tx_clear));
    
    // RX control pins
    LITEXCNC_CREATE_HAL_PIN("rx-unload", bit, HAL_IN, &(serial_inst->hal.pin.rx_unload));
    LITEXCNC_CREATE_HAL_PIN("rx-clear", bit, HAL_IN, &(serial_inst->hal.pin.rx_clear));
    
    // RX data pins (8 registers)
    for (size_t i = 0; i < SERIAL_FIFO_REGISTERS; i++) {
        rtapi_snprintf(name, sizeof(name), "%s.rx-data-%zu", base_name, i);
        r = hal_pin_u32_new(name, HAL_OUT, &(serial_inst->hal.pin.rx_data[i]), litexcnc->fpga->comp_id);
        if (r < 0) {
            LITEXCNC_ERR_NO_DEVICE("Error adding pin '%s', aborting\n", name);
            return r;
        }
    }
    
    // TX FIFO status pins
    LITEXCNC_CREATE_HAL_PIN("tx-write-ptr", u32, HAL_OUT, &(serial_inst->hal.pin.tx_write_ptr));
    LITEXCNC_CREATE_HAL_PIN("tx-read-ptr", u32, HAL_OUT, &(serial_inst->hal.pin.tx_read_ptr));
    LITEXCNC_CREATE_HAL_PIN("tx-level", u32, HAL_OUT, &(serial_inst->hal.pin.tx_level));
    
    // RX FIFO status pins
    LITEXCNC_CREATE_HAL_PIN("rx-write-ptr", u32, HAL_OUT, &(serial_inst->hal.pin.rx_write_ptr));
    LITEXCNC_CREATE_HAL_PIN("rx-read-ptr", u32, HAL_OUT, &(serial_inst->hal.pin.rx_read_ptr));
    LITEXCNC_CREATE_HAL_PIN("rx-level", u32, HAL_OUT, &(serial_inst->hal.pin.rx_level));
    
    // Status pins
    LITEXCNC_CREATE_HAL_PIN("tx-full", bit, HAL_OUT, &(serial_inst->hal.pin.tx_full));
    LITEXCNC_CREATE_HAL_PIN("tx-empty", bit, HAL_OUT, &(serial_inst->hal.pin.tx_empty));
    LITEXCNC_CREATE_HAL_PIN("rx-full", bit, HAL_OUT, &(serial_inst->hal.pin.rx_full));
    LITEXCNC_CREATE_HAL_PIN("rx-empty", bit, HAL_OUT, &(serial_inst->hal.pin.rx_empty));
    LITEXCNC_CREATE_HAL_PIN("tx-active", bit, HAL_OUT, &(serial_inst->hal.pin.tx_active));
    LITEXCNC_CREATE_HAL_PIN("data-enable", bit, HAL_OUT, &(serial_inst->hal.pin.data_enable));
    LITEXCNC_CREATE_HAL_PIN("parity-error", bit, HAL_OUT, &(serial_inst->hal.pin.parity_error));
    LITEXCNC_CREATE_HAL_PIN("frame-error", bit, HAL_OUT, &(serial_inst->hal.pin.frame_error));
    
    // Parameters
    LITEXCNC_CREATE_HAL_PARAM("baudrate", u32, HAL_RW, &(serial_inst->hal.param.baudrate));
    LITEXCNC_CREATE_HAL_PARAM("clock-freq", u32, HAL_RO, &(serial_inst->hal.param.clock_freq));
    
    // Set default values
    *(serial_inst->hal.pin.divisor) = 434;        // Default for 115200 @ 50MHz
    *(serial_inst->hal.pin.data_bits) = 8;        // 8 data bits
    *(serial_inst->hal.pin.parity_enable) = 0;    // No parity
    *(serial_inst->hal.pin.parity_type) = 0;      // Even parity (when enabled)
    *(serial_inst->hal.pin.stop_bits) = 1;        // 1 stop bit
    serial_inst->hal.param.baudrate = 115200;     // Default baudrate
    
    return 0;
}


size_t litexcnc_serial_init(
    litexcnc_module_instance_t **module, 
    litexcnc_t *litexcnc,
    uint8_t **config
) {
    // Create structure in memory
    (*module) = (litexcnc_module_instance_t *)hal_malloc(sizeof(litexcnc_module_instance_t));
    (*module)->prepare_write = &litexcnc_serial_prepare_write;
    (*module)->process_read = &litexcnc_serial_process_read;
    (*module)->instance_data = hal_malloc(sizeof(litexcnc_serial_t));
    
    // Cast from void to correct type and store it
    litexcnc_serial_t *serial = (litexcnc_serial_t *) (*module)->instance_data;
    instances[num_instances] = serial;
    num_instances++;
    
    // Read number of serial port instances from config
    serial->num_instances = *(*config);
    (*config)++;
    
    LITEXCNC_PRINT_NO_DEVICE("Creating %d serial port instance(s)\n", serial->num_instances);
    
    // Allocate memory for instances
    serial->instances = (litexcnc_serial_instance_t *)hal_malloc(
        serial->num_instances * sizeof(litexcnc_serial_instance_t)
    );
    if (serial->instances == NULL) {
        LITEXCNC_ERR_NO_DEVICE("Out of memory!\n");
        return -ENOMEM;
    }
    
    // Store clock frequency pointer
    serial->data.clock_frequency = &(litexcnc->clock_frequency);
    // Initialize each serial port instance
    for (size_t i = 0; i < serial->num_instances; i++) {
        int r = litexcnc_serial_init_instance(&serial->instances[i], litexcnc, i);
        if (r < 0) {
            return r;
        }
        
        // Set clock frequency parameter
        serial->instances[i].hal.param.clock_freq = *(serial->data.clock_frequency);
    }
    
    // Success
    return 0;
}
