/********************************************************************
* Description:  litexcnc_serial.h
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
#ifndef __INCLUDE_LITEXCNC_SERIAL_H__
#define __INCLUDE_LITEXCNC_SERIAL_H__

#include <litexcnc.h>

#define LITEXCNC_SERIAL_NAME "litexcnc_serial"

#define LITEXCNC_SERIAL_VERSION_MAJOR 1
#define LITEXCNC_SERIAL_VERSION_MINOR 0
#define LITEXCNC_SERIAL_VERSION_PATCH 0

#define MAX_INSTANCES 4
#define SERIAL_FIFO_DEPTH 32
#define SERIAL_FIFO_REGISTERS 8  // 8 registers x 4 bytes = 32 bytes

/** The ID of the component, only used when the component is used as stand-alone */
int comp_id;

/*******************************************************************************
 * STRUCTS
 ******************************************************************************/

/** Structure of a serial port instance */
typedef struct {
    /** Structure defining the HAL pins and params */
    struct {
        /** Structure defining the HAL pins */
        struct {
            // Configuration pins
            hal_u32_t *divisor;           /* Baud rate divisor (sys_clk / baudrate) */
            hal_u32_t *data_bits;         /* Number of data bits (5-8) */
            hal_bit_t *parity_enable;     /* Enable parity checking */
            hal_bit_t *parity_type;       /* 0=even, 1=odd */
            hal_u32_t *stop_bits;         /* Number of stop bits (1 or 2) */
            
            // TX control and data
            hal_u32_t *tx_data[SERIAL_FIFO_REGISTERS];  /* TX FIFO data registers (8x32-bit) */
            hal_bit_t *tx_load;           /* Trigger to load TX FIFO */
            hal_bit_t *tx_clear;          /* Clear TX FIFO */
            
            // RX control
            hal_bit_t *rx_unload;         /* Trigger to unload RX FIFO */
            hal_bit_t *rx_clear;          /* Clear RX FIFO */
            
            // RX data (read from FPGA)
            hal_u32_t *rx_data[SERIAL_FIFO_REGISTERS];  /* RX FIFO data registers (8x32-bit) */
            
            // Status pins (read from FPGA)
            hal_u32_t *tx_write_ptr;      /* TX FIFO write pointer */
            hal_u32_t *tx_read_ptr;       /* TX FIFO read pointer */
            hal_u32_t *tx_level;          /* TX FIFO level */
            hal_u32_t *rx_write_ptr;      /* RX FIFO write pointer */
            hal_u32_t *rx_read_ptr;       /* RX FIFO read pointer */
            hal_u32_t *rx_level;          /* RX FIFO level */
            
            hal_bit_t *tx_full;           /* TX FIFO full */
            hal_bit_t *tx_empty;          /* TX FIFO empty */
            hal_bit_t *rx_full;           /* RX FIFO full */
            hal_bit_t *rx_empty;          /* RX FIFO empty */
            hal_bit_t *tx_active;         /* Transmission active */
            hal_bit_t *data_enable;       /* RS485 DataEnable state */
            hal_bit_t *parity_error;      /* Parity error detected */
            hal_bit_t *frame_error;       /* Frame error detected */
        } pin;
        
        /** Structure defining the HAL params */
        struct {
            hal_u32_t baudrate;           /* Configured baud rate (for display) */
            hal_u32_t clock_freq;         /* FPGA clock frequency */
        } param;
    } hal;
    
    /** This struct holds data from previous cycles for change detection */
    struct {
        hal_u32_t divisor;
        hal_u32_t data_bits;
        hal_bit_t parity_enable;
        hal_bit_t parity_type;
        hal_u32_t stop_bits;
        hal_bit_t tx_load;
        hal_bit_t tx_clear;
        hal_bit_t rx_unload;
        hal_bit_t rx_clear;
    } memo;
    
} litexcnc_serial_instance_t;


/** Structure of the Serial module */
typedef struct {
    int num_instances;                      /** Number of serial port instances */
    litexcnc_serial_instance_t *instances;  /** Structure containing the data on the serial ports */
    
    /** This struct holds pointers to data from the FPGA */
    struct {
        uint32_t *clock_frequency;
    } data;
} litexcnc_serial_t;


/*******************************************************************************
 * DATA STRUCTURES FOR FPGA COMMUNICATION
 ******************************************************************************/

/** Structure for serial configuration data (write to FPGA) */
#pragma pack(push,4)
typedef struct {
    uint32_t divisor;           /* Baud rate divisor */
    uint32_t config;            /* UART mode configuration */
} litexcnc_serial_config_data_t;
#pragma pack(pop)

/** Structure for TX FIFO data (write to FPGA) */
#pragma pack(push,4)
typedef struct {
    uint32_t data[SERIAL_FIFO_REGISTERS];  /* TX FIFO data (8 x 32-bit) */
} litexcnc_serial_tx_data_t;
#pragma pack(pop)

/** Structure for TX/RX control (write to FPGA) */
#pragma pack(push,4)
typedef struct {
    uint32_t tx_ctrl;           /* TX control (bit 0: load, bit 1: clear) */
    uint32_t rx_ctrl;           /* RX control (bit 0: unload, bit 1: clear) */
} litexcnc_serial_ctrl_data_t;
#pragma pack(pop)

/** Structure for RX FIFO data (read from FPGA) */
#pragma pack(push,4)
typedef struct {
    uint32_t data[SERIAL_FIFO_REGISTERS];  /* RX FIFO data (8 x 32-bit) */
} litexcnc_serial_rx_data_t;
#pragma pack(pop)

/** Structure for FIFO pointers (read from FPGA) */
#pragma pack(push,4)
typedef struct {
    uint32_t tx_ptr;            /* TX pointers: [7:0]=write_ptr, [15:8]=read_ptr, [23:16]=level */
    uint32_t rx_ptr;            /* RX pointers: [7:0]=write_ptr, [15:8]=read_ptr, [23:16]=level */
} litexcnc_serial_ptr_data_t;
#pragma pack(pop)

/** Structure for serial status (read from FPGA) */
#pragma pack(push,4)
typedef struct {
    uint32_t status;            /* Status flags */
} litexcnc_serial_status_data_t;
#pragma pack(pop)


/*******************************************************************************
 * FUNCTIONS
 ******************************************************************************/
 
/*******************************************************************************
 * Function which is called when a user adds the component using `loadrt 
 * litexcnc_serial`. It will initialize the component with LinuxCNC and registers
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
 * @param litexcnc The litexcnc instance
 * @param config The configuration of the FPGA, used to initialize the module. 
 ******************************************************************************/
size_t litexcnc_serial_init(
    litexcnc_module_instance_t **instance, 
    litexcnc_t *litexcnc,
    uint8_t **config
);


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
 *     the pointer should be moved to the next element, so the next module can 
 *     append its data. All data in LitexCNC is 4-bytes wide. 
 * @param period Period in nano-seconds of a cycle
 ******************************************************************************/
int litexcnc_serial_prepare_write(void *instance, uint8_t **data, int period);


/*******************************************************************************
 * Processes the data which has been received from a device
 *
 * @param instance The structure containing the data on the module instance
 * @param data Pointer to the array where the data is contained in. NOTE:
 *     the pointer should be moved to the next element, so the next module can 
 *     process its data. All data in LitexCNC is 4-bytes wide. 
 * @param period Period in nano-seconds of a cycle
 ******************************************************************************/
int litexcnc_serial_process_read(void *instance, uint8_t** data, int period);

#endif
