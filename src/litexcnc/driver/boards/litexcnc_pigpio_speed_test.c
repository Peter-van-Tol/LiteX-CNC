/********************************************************************
* Description:  litexcnc_encoder.c
*               A LinuxCNC module that can be used to determine which 
*               speed the SPI communication can be run. This component
*               does not export any functions, nor pins.
*
* Author: Peter van Tol <petertgvantol AT gmail DOT com>
* License: GPL Version 2
*    
* Copyright (c) 2022 All rights reserved.
*
********************************************************************/
//
//    Copyright (C) 2022 Peter van Tol
//
//    This program is free software; you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation; either version 2 of the License, or
//    (at your option) any later version.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with this program; if not, write to the Free Software
//    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
//
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <unistd.h>
#include <sys/types.h>
#include <string.h>

#include <rtapi_slab.h>
#include <rtapi_list.h>

#include "hal.h"
#include "rtapi.h"
#include "rtapi_app.h"
#include "rtapi_string.h"

#define LITEXCNC_PIGPIO_SPEED_TEST_NAME    "litexcnc_pigpio_speedtest"
#define LITEXCNC_PIGPIO_SPEED_TEST_VERSION "1.0.0"

#define LOOPS 10000

// This keeps track of the component id. Required for setup and tear down.
static int comp_id;

/*
 * TX and RX buffers
 **/
#define BYTES 11
uint8_t tx_data[BYTES] = {
	0x41, 0x00, 0x00, 0x00, 0x00,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF
};
uint8_t rx_data[BYTES] = {
	0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00,
	0x00
};

/*
 * Definitions of the functions from `pigpio`, the functions are loaded
 * dynamically to prevent trouble with linking shared libraries.
 **/
int (*gpioInitialise)(void);
int (*spiOpen)(unsigned, unsigned, unsigned);
int (*spiXfer)(unsigned, char*, char*, unsigned);
int (*spiClose)(unsigned);
int (*gpioTerminate)(void);
double (*time_time)(void);


int rtapi_app_main(void) {
    int ret;

    comp_id = hal_init(LITEXCNC_PIGPIO_SPEED_TEST_NAME);
    if(comp_id < 0) return comp_id;

    // Dynamically load the driver
    void *pigpio = dlopen("libpigpio.so", RTLD_GLOBAL | RTLD_NOW);
    if (!pigpio) {
        fprintf(stderr, "Error while loading `pigpio`: %s\n", dlerror());
        return -1;
    }
    gpioInitialise = dlsym(pigpio, "gpioInitialise");
    spiOpen = dlsym(pigpio, "spiOpen");
    spiXfer = dlsym(pigpio, "spiXfer");
    spiClose = dlsym(pigpio, "spiClose");
    gpioTerminate = dlsym(pigpio, "gpioTerminate");
    time_time = dlsym(pigpio, "time_time");

    // Setuid is required for root acces to /dev/mem
    static uid_t euid, ruid;
    ruid = getuid();
    euid = geteuid();
#ifdef _POSIX_SAVED_IDS
    ret = seteuid(ruid);
#else
    ret = setreuid(ruid, euid);
#endif
    if (ret < 0) {
        fprintf (stderr, "Couldn't set uid.\n");
        return ret;
    }

    // Initialize GPIO
    ret = gpioInitialise();
    if (ret < 0) return ret;
    
    int fd;
    int speed = 1000000;
    int failed;
    uint32_t magic;

    while (1) {

        // Open the SPI connection
        fd = spiOpen(0, speed, 0);

        double max = 0;
        double start = time_time();

        for (size_t i=0; i<LOOPS; i++) {
            
            double start2 = time_time();
            int ret = spiXfer(fd, tx_data, rx_data, BYTES);
            if (ret < 1) {
                fprintf(stderr, "Could not write to SPI device\n");
                return -1;
            }
            double diff2 = time_time() - start2;
            if (diff2 > max) {
                max = diff2;
            }

            // Check whether write was successfull
            failed = 1;  // Assume failure
            for(size_t i = 0; i < (5 + 2); i++) {
                if(rx_data[i] == 0x01) {
                    // Copy the results to the output
                    memcpy(&magic, &rx_data[i+1], sizeof(magic));
                    magic = be32toh(magic);
                    if (magic == 0x18052022) { 
                        // Indicate success
                        failed = 0;
                        break;
                    } else {
                        break;
                    }
                }
            }

            if (failed) {
                break;
            }
        }
        
        double diff = time_time() - start;
        
        if (failed) {
            fprintf(stderr, "Failed transmission at %d Hz.", speed);
            spiClose(fd);
            // Close the connection
            gpioTerminate();
            // Report ready to rumble
            hal_ready(comp_id);
            return 0;
        }

        fprintf(
            stderr, 
            "sps=%.1f: %d bytes @ %d bps (loops=%d, average time=%.3f us, maximum time=%.3f us)\n", 
            (double)10000 / diff, BYTES, speed, 10000, diff / LOOPS * 1e6f, max * 1e6
        );

        spiClose(fd);
        speed += 500000;
    }
    
    // Close the connection
    gpioTerminate();
    
    // Report ready to rumble
    hal_ready(comp_id);
    return 0;
}

void rtapi_app_exit(void) {
    // Exit the component
    hal_exit(comp_id);
}
