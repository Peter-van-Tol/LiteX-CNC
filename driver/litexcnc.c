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
#include <stdio.h>
#include <json-c/json.h>

#include <rtapi_slab.h>
#include <rtapi_ctype.h>
#include <rtapi_list.h>

#include "rtapi.h"
#include "rtapi_app.h"
#include "rtapi_string.h"
#include "rtapi_math.h"

#include "hal.h"

#include "litexcnc.h"
#include "gpio.h"



// Include all other files. This makes separation in different files possible,
// while still compiling a single file. NOTE: the #include directive just copies
// the whole contents of that file into this source-file.
#include "gpio.c"


#ifdef MODULE_INFO
MODULE_INFO(linuxcnc, "component:litexcnc:Board driver for FPGA boards supported by litex.");
MODULE_INFO(linuxcnc, "funct:read:1:Read all registers.");
MODULE_INFO(linuxcnc, "funct:write:1:Write all registers, and pet the watchdog to keep it from biting.");
MODULE_INFO(linuxcnc, "author:Peter van Tol petertgvantolATgmailDOTcom");
MODULE_INFO(linuxcnc, "license:GPL");
MODULE_LICENSE("GPL");
#endif // MODULE_INFO


// This keeps track of all the litexcnc instances that have been registered by drivers
struct rtapi_list_head litexcnc_list;

// This keeps track of the component id. Required for setup and tear down.
static int comp_id;


static void litexcnc_read(void* void_litexcnc, long period) {
    litexcnc_t *litexcnc = void_litexcnc;

    // if there are comm problems, wait for the user to fix it
    if ((*litexcnc->fpga->io_error) != 0) return;

    // Process the read data

}

static void litexcnc_write(void *void_litexcnc, long period) {
    litexcnc_t *litexcnc = void_litexcnc;

    // Prepare the write of the data
    uint8_t data[4];
    memset((uint8_t*)data,0,sizeof(data));
    uint8_t* pointer = data;
    
    litexcnc_gpio_prepare_write(litexcnc, pointer);

    // // TEMP: print the result
    // LITEXCNC_PRINT_NO_DEVICE("%02X %02X %02X %02X\n", (unsigned char)data[0], (unsigned char)data[1], (unsigned char)data[2], (unsigned char)data[3]);
    litexcnc->fpga->write(litexcnc->fpga, data, 4);

    // // if there are comm problems, wait for the user to fix it
    // if ((*litexcnc->fpga->io_error) != 0) return;
}

static void litexcnc_cleanup(litexcnc_t *litexcnc) {
    // clean up the Pins, if they're initialized
    // if (litexcnc->pin != NULL) rtapi_kfree(litexcnc->pin);

    // clean up the Modules
    // TODO
}


EXPORT_SYMBOL_GPL(litexcnc_register);
int litexcnc_register(litexcnc_fpga_t *fpga, const char *config_file) {
    int r;
    litexcnc_t *litexcnc;

    // Allocate memory for the board
    litexcnc = rtapi_kmalloc(sizeof(litexcnc_t), RTAPI_GFP_KERNEL);
    if (litexcnc == NULL) {
        LITEXCNC_PRINT_NO_DEVICE("out of memory!\n");
        return -ENOMEM;
    }
    memset(litexcnc, 0, sizeof(litexcnc_t));

    // Store the FPGA on it
    litexcnc->fpga = fpga;
    
    // Add it to the list
    rtapi_list_add_tail(&litexcnc->list, &litexcnc_list);

    // Initialize the functions
    struct json_object *config = json_object_from_file(config_file);
    if (!config) {
        LITEXCNC_ERR_NO_DEVICE("Could not load configuration file '%s'\n", config_file);
        goto fail0;
    }

    if (litexcnc_gpio_init(litexcnc, config) < 0) {
        goto fail0;
    }

    // Export functions
    // - read function
    char name[HAL_NAME_LEN + 1];
    rtapi_snprintf(name, sizeof(name), "%s.read", litexcnc->fpga->name);
    r = hal_export_funct(name, litexcnc_read, litexcnc, 1, 0, litexcnc->fpga->comp_id);
    if (r != 0) {
        LITEXCNC_ERR("error %d exporting read function %s\n", r, name);
        r = -EINVAL;
        goto fail1;
    }
    // - write function
    rtapi_snprintf(name, sizeof(name), "%s.write", litexcnc->fpga->name);
    r = hal_export_funct(name, litexcnc_write, litexcnc, 1, 0, litexcnc->fpga->comp_id);
    if (r != 0) {
        LITEXCNC_ERR("error %d exporting read function %s\n", r, name);
        r = -EINVAL;
        goto fail1;
    }

    return 0;

fail1:
    litexcnc_cleanup(litexcnc);  // undoes the rtapi_kmallocs from hm2_parse_module_descriptors()

fail0:
    rtapi_list_del(&litexcnc->list);
    rtapi_kfree(litexcnc);
    return r;
}


int rtapi_app_main(void) {
    LITEXCNC_PRINT_NO_DEVICE("Loading Litex CNC driver version %s\n", LITEXCNC_VERSION);

    comp_id = hal_init(LITEXCNC_NAME);
    if(comp_id < 0) return comp_id;
    
    RTAPI_INIT_LIST_HEAD(&litexcnc_list);

    hal_ready(comp_id);

    return 0;
}


void rtapi_app_exit(void) {
    hal_exit(comp_id);
    LITEXCNC_PRINT_NO_DEVICE("LitexCNC driver unloaded \n");
}


