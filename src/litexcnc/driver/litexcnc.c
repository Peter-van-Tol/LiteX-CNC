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
#include <time.h>
#include <dlfcn.h>

#include <rtapi_slab.h>
#include <rtapi_ctype.h>

#include "rtapi.h"
#include "rtapi_app.h"
#include "rtapi_string.h"
#include "rtapi_math.h"

#include "config.h"
#include "hal.h"

// Between version 2.8 and 2.9 the definition of LINELEN has moved. Below
// is a check whether 2.9 is installed. If so, the header is imported to
// get the correct value of LINELEN.
#ifndef LINELEN
#include "linuxcnc.h"
#endif /* LINELEN */

#include "litexcnc.h"


#ifdef MODULE_INFO
MODULE_INFO(linuxcnc, "component:litexcnc:Board driver for FPGA boards supported by litex.");
MODULE_INFO(linuxcnc, "funct:read:1:Read all registers.");
MODULE_INFO(linuxcnc, "funct:write:1:Write all registers, and pet the watchdog to keep it from biting.");
MODULE_INFO(linuxcnc, "author:Peter van Tol petertgvantolATgmailDOTcom");
MODULE_INFO(linuxcnc, "license:GPL");
MODULE_LICENSE("GPL");
#endif // MODULE_INFO

static char *extra_modules[MAX_EXTRAS];
RTAPI_MP_ARRAY_STRING(extra_modules, MAX_EXTRAS, "Extra modules to load.")
static char *extra_drivers[MAX_EXTRAS];
RTAPI_MP_ARRAY_STRING(extra_drivers, MAX_EXTRAS, "Extra drivers to load.")
static char *connections[MAX_CONNECTIONS];
RTAPI_MP_ARRAY_STRING(connections, MAX_CONNECTIONS, "Connections to make.")

// This keeps track of all the litexcnc instances that have been registered by drivers
struct rtapi_list_head litexcnc_list;
struct rtapi_list_head litexcnc_modules;
struct rtapi_list_head litexcnc_drivers;

// This keeps track of all default modules, so they can be unloaded later
void *loaded_modules[32];
size_t loaded_modules_count;

// This keeps track of all default drivers (i.e. ethernet, USB, UART), so they can
// be unloaded later. NOTE: only ethernet is supported at this moment.
void *loaded_drivers[1];
size_t loaded_drivers_count;

// This keeps track of the component id. Required for setup and tear down.
static int comp_id;

static void litexcnc_config(void* void_litexcnc, long period) {
    litexcnc_t *litexcnc = void_litexcnc;

    // Safeguard for situation without config
    if (litexcnc->fpga->config_buffer_size == 0) {
        return;
    }

    // Clear buffer
    uint8_t *config_buffer = rtapi_kmalloc(litexcnc->fpga->config_buffer_size, RTAPI_GFP_KERNEL);
    memset(config_buffer, 0, litexcnc->fpga->config_buffer_size);
    
    // Configure all the functions
    uint8_t* pointer = config_buffer;

    // Configure the general settings
    // litexcnc_config_header_t config_data;
    // ...
    // memcpy(pointer, &config_data, sizeof(litexcnc_config_header_t));
    // pointer += sizeof(litexcnc_config_header_t);

    // Configure all the functions
    // - default
    // litexcnc_watchdog_config(litexcnc, &pointer, period);
    // litexcnc_wallclock_config(litexcnc, &pointer, period);
    // - custom
    for (size_t i=0; i<litexcnc->num_modules; i++) {
        litexcnc_module_instance_t *module = litexcnc->modules[i];
        if (module->configure_module != NULL) {
            module->configure_module(module->instance_data, &pointer, period);
        }
    }
    
    // Write the data to the FPGA'
    litexcnc->fpga->write_n_bits(
        litexcnc->fpga, 
        litexcnc->fpga->config_base_address, 
        config_buffer, 
        litexcnc->fpga->config_buffer_size
    );
}


static void litexcnc_read(void* void_litexcnc, long period) {
    litexcnc_t *litexcnc = void_litexcnc;

    // The first loop no data is read, as it is used for sending the configuration to the 
    // FPGA. The configuration is written in the `litexcnc_write` function. 
    if (!litexcnc->read_loop_has_run) {
        litexcnc->read_loop_has_run = true;
        return;
    }

    // Clear buffer (except for the header)
    memset(
        litexcnc->fpga->read_buffer + litexcnc->fpga->read_header_size, 
        0, 
        litexcnc->fpga->read_buffer_size - litexcnc->fpga->read_header_size
    );
    
    // Read the state from the FPGA
    litexcnc->fpga->read(litexcnc->fpga);

    // TODO: don't process the read data in case the read has failed.

    // Process the read data for the different compenents
    uint8_t* pointer = litexcnc->fpga->read_buffer + litexcnc->fpga->read_header_size;
    // - default
    litexcnc_watchdog_process_read(litexcnc, &pointer);
    litexcnc_wallclock_process_read(litexcnc, &pointer);
    // - custom
    for (size_t i=0; i<litexcnc->num_modules; i++) {
        litexcnc_module_instance_t *module = litexcnc->modules[i];
        if (module->process_read != NULL) {
            module->process_read(module->instance_data, &pointer, period);
        }
    }
}

static void litexcnc_write(void *void_litexcnc, long period) {
    litexcnc_t *litexcnc = void_litexcnc;

    // Check whether the write has been initialized AND the read and write functions
    // are in the recommended order (first read, then write). In the first loop the
    // we don't write any data to the FPGA, but it is configured. This is required,
    // because the configuration requires the period to be known, which prevents the
    // configuration to be performed before the HAL-loop starts
    if (!litexcnc->write_loop_has_run) {
        // Check whether the read cycle has been run, if not, the order is not correct
        if (!litexcnc->read_loop_has_run) {
            LITEXCNC_WARN("Read and write functions in incorrect order. Recommended order is read first, then write.\n", litexcnc->fpga->name);
        }
        // Configure the FPGA and set flag that the write function has been done once
        litexcnc_config(void_litexcnc, period);
        litexcnc->write_loop_has_run = true;
        return;
    }

    // Clear buffer (except for the header)
    memset(
        litexcnc->fpga->write_buffer + litexcnc->fpga->write_header_size, 
        0, 
        litexcnc->fpga->write_buffer_size - litexcnc->fpga->write_header_size
    );

    // Process all functions
    uint8_t* pointer = litexcnc->fpga->write_buffer + litexcnc->fpga->write_header_size;
    // - default
    litexcnc_watchdog_prepare_write(litexcnc, &pointer, period);
    litexcnc_wallclock_prepare_write(litexcnc, &pointer);
    // - custom
    for (size_t i=0; i<litexcnc->num_modules; i++) {
        litexcnc_module_instance_t *module = litexcnc->modules[i];
        if (module->prepare_write != NULL) {
            module->prepare_write(module->instance_data, &pointer, period);
        }
    }

    // Write the data to the FPGA
    litexcnc->fpga->write(litexcnc->fpga);
}


static void litexcnc_cleanup(litexcnc_t *litexcnc) {
    // clean up the Pins, if they're initialized
    // if (litexcnc->pin != NULL) rtapi_kfree(litexcnc->pin);

    // clean up the Modules
    // TODO
}


size_t retrieve_module_from_registration(litexcnc_module_registration_t **registration, uint32_t module_id){
    struct rtapi_list_head *ptr;
    rtapi_list_for_each(ptr, &litexcnc_modules) {
        (*registration) = rtapi_list_entry(ptr, litexcnc_module_registration_t, list);
        if ((*registration)->id == module_id) {return 0;}
    }
    return -1;
}


size_t retrieve_driver_from_registration(litexcnc_driver_registration_t **registration, char *name){
    struct rtapi_list_head *ptr;
    rtapi_list_for_each(ptr, &litexcnc_drivers) {
        (*registration) = rtapi_list_entry(ptr, litexcnc_driver_registration_t, list);
        if (strcmp((*registration)->name, name) == 0) {return 0;}
    }
    return -1;
}


int litexcnc_reset(litexcnc_fpga_t *fpga) {

    size_t i;
    int r;
    uint32_t reset_flag;
    uint32_t reset_status;
    uint8_t *reset_buffer = rtapi_kmalloc(LITEXCNC_RESET_HEADER_SIZE, RTAPI_GFP_KERNEL);

    // Raise flag
    reset_flag = htobe32(0x01);
    reset_status = htobe32(0x00); // Make sure the loop below is run at least once
    i = 0;
    while (reset_flag != reset_status) {
        if (i > MAX_RESET_RETRIES) {
            LITEXCNC_ERR_NO_DEVICE("Reset of the card failed after %d times\n", MAX_RESET_RETRIES);
            return -1;
        }
        // Write the reset flag to the FPGA
        memcpy(reset_buffer, &reset_flag, LITEXCNC_RESET_HEADER_SIZE);
        r = fpga->write_n_bits(
            fpga, 
            fpga->reset_base_address, 
            reset_buffer, 
            sizeof(reset_flag)
        );
        r = fpga->read_n_bits(
            fpga, 
            fpga->reset_base_address, 
            reset_buffer, 
            sizeof(reset_flag)
        );
        reset_status = *(uint32_t *)reset_buffer;
        i++;
    }

    // Lower flag
    reset_flag = htobe32(0x00);
    i = 0;
    while (reset_flag != reset_status) {
        if (i > MAX_RESET_RETRIES) {
            LITEXCNC_ERR_NO_DEVICE("Reset of the card failed after %d times\n", MAX_RESET_RETRIES);
            return -1;
        }
        // Write the reset flag to the FPGA
        memcpy(reset_buffer, &reset_flag, LITEXCNC_RESET_HEADER_SIZE);
        r = fpga->write_n_bits(
            fpga, 
            fpga->reset_base_address, 
            reset_buffer, 
            sizeof(reset_flag)
        );
        r = fpga->read_n_bits(
            fpga, 
            fpga->reset_base_address, 
            reset_buffer, 
            sizeof(reset_flag)
        );
        reset_status = *(uint32_t *)reset_buffer;
        i++;
    }

    return 0;
}


EXPORT_SYMBOL_GPL(litexcnc_register);
int litexcnc_register(litexcnc_fpga_t *fpga) {
    int r;
    size_t i;
    litexcnc_t *litexcnc;

    // Allocate memory for the litexcnc instance
    litexcnc = rtapi_kmalloc(sizeof(litexcnc_t), RTAPI_GFP_KERNEL);
    if (litexcnc == NULL) {
        LITEXCNC_PRINT_NO_DEVICE("out of memory!\n");
        return -ENOMEM;
    }
    memset(litexcnc, 0, sizeof(litexcnc_t));

    // Set initial values
    litexcnc->write_loop_has_run = false;
    litexcnc->read_loop_has_run = false;

    // Store the FPGA on it
    litexcnc->fpga = fpga;
    
    // Add it to the list
    rtapi_list_add_tail(&litexcnc->list, &litexcnc_list);

    // Read 6 DWORD data from the FPGA, retrieving:
    // - magic
    // - version + config data length
    // - name (4 DWORD / 16 charachters)
    uint8_t *header_buffer = rtapi_kmalloc(sizeof(litexcnc_header_data_read_t), RTAPI_GFP_KERNEL);
    r = litexcnc->fpga->read_n_bits(litexcnc->fpga, 0x0, header_buffer, LITEXCNC_HEADER_DATA_READ_SIZE);
    if (r < 0) {
        LITEXCNC_ERR_NO_DEVICE("Could not read from card, please check connection?\n");
        return r;
    }
    litexcnc_header_data_read_t header_data;
    memcpy(&header_data, header_buffer, sizeof(litexcnc_header_data_read_t));

    // =======================
    // READ AND PROCESS HEADER
    // =======================
    // Verify the data
    // - magic number
    header_data.magic = be32toh(header_data.magic);
    if (header_data.magic != 0x18052022) {
        LITEXCNC_ERR_NO_DEVICE("Invalid magic received '%08X', is this a LitexCNC card?\n", header_data.magic);
        return -1;
    }
    // - version
    if ((header_data.version_major != LITEXCNC_VERSION_MAJOR) || (header_data.version_major != LITEXCNC_VERSION_MINOR ))  {
        // Incompatible versiheader_dataon
        LITEXCNC_ERR_NO_DEVICE(
            "Version of firmware (%u.%u.%u) is incompatible with the version of the driver (%u.%u.%u) \n",
            header_data.version_major, header_data.version_minor, header_data.version_patch, 
            LITEXCNC_VERSION_MAJOR, LITEXCNC_VERSION_MINOR, LITEXCNC_VERSION_PATCH);
        return -1;
    } else if (header_data.version_patch != LITEXCNC_VERSION_PATCH) {
        // Warn that patch version is different
        LITEXCNC_PRINT_NO_DEVICE(
            "INFO: Version of firmware (%u.%u.%u) is different with the version of the driver (%u.%u.%u). Communication is still possible, although one of these could use an update for the best experience. \n",
            header_data.version_major, header_data.version_minor, header_data.version_patch, 
            LITEXCNC_VERSION_MAJOR, LITEXCNC_VERSION_MINOR, LITEXCNC_VERSION_PATCH);
    }

    // Parse the name from the received data and check validity
    for (i = 0; i < HAL_NAME_LEN+1; i ++) {
        if (header_data.name[i] == '\0') break;
        if (!isprint(header_data.name[i])) {
            LITEXCNC_ERR_NO_DEVICE("Invalid board name (contains non-printable character)\n");
            return -EINVAL;
        }
    }
    if (i == HAL_NAME_LEN+1) {
        LITEXCNC_ERR_NO_DEVICE("Invalid board name (not NULL terminated)\n");
        return -EINVAL;
    }
    if (i == 0) {
        LITEXCNC_ERR_NO_DEVICE("Invalid board name (zero length)\n");
        return -EINVAL;

    }
    memcpy(&litexcnc->fpga->name, header_data.name, i);

    // Store the received clock speed
    litexcnc->clock_frequency = be32toh(header_data.clock_frequency);
    litexcnc->clock_frequency_recip = 1.0f / litexcnc->clock_frequency;


    // ==================================
    // READ AND PROCESS CONFIG OF MODULES
    // ==================================
    LITEXCNC_PRINT_NO_DEVICE("Setting up modules...\n");
    LITEXCNC_PRINT_NO_DEVICE("Reading %u bytes\n", be16toh(header_data.module_data_size));
    uint8_t *config_buffer = rtapi_kmalloc(be16toh(header_data.module_data_size), RTAPI_GFP_KERNEL);
    r = litexcnc->fpga->read_n_bits(litexcnc->fpga, LITEXCNC_HEADER_DATA_READ_SIZE, config_buffer, be16toh(header_data.module_data_size));
    if (r < 0) {
        LITEXCNC_ERR_NO_DEVICE("Could not read from card, please check connection?\n");
        return r;
    }

    // LITEXCNC_PRINT_NO_DEVICE("Read header data:\n");
    // for (size_t i=0; i<(be16toh(header_data.module_data_size)); i+=4) {
    //     LITEXCNC_PRINT_NO_DEVICE("%02X %02X %02X %02X\n",
    //         (unsigned char)config_buffer[i+0],
    //         (unsigned char)config_buffer[i+1],
    //         (unsigned char)config_buffer[i+2],
    //         (unsigned char)config_buffer[i+3]);
    // }

    // Default modules
    // - watchdog
    LITEXCNC_PRINT_NO_DEVICE(" - Watchdog\n");
    if (litexcnc_watchdog_init(litexcnc) < 0) {
        LITEXCNC_ERR_NO_DEVICE("Watchdog init failed\n");
        return r;
    }
    litexcnc->fpga->write_buffer_size += LITEXCNC_WATCHDOG_DATA_WRITE_SIZE;
    litexcnc->fpga->read_buffer_size += LITEXCNC_WATCHDOG_DATA_READ_SIZE;
    
    // - wallclock
    LITEXCNC_PRINT_NO_DEVICE(" - Wallclock\n");
    if (litexcnc_wallclock_init(litexcnc) < 0) {
        LITEXCNC_ERR_NO_DEVICE("Wallclock init failed\n");
        return r;
    }
    litexcnc->fpga->write_buffer_size += LITEXCNC_WALLCLOCK_DATA_WRITE_SIZE;
    litexcnc->fpga->read_buffer_size += LITEXCNC_WALLCLOCK_DATA_READ_SIZE;

    // - custom modules
    litexcnc_module_registration_t *registration;
    litexcnc->num_modules = header_data.num_modules;
    litexcnc->modules = (litexcnc_module_instance_t**) hal_malloc(litexcnc->num_modules * sizeof(litexcnc_module_instance_t*));;
    for (i = 0; i < litexcnc->num_modules; i ++) {
        // Get module from registration
        r = retrieve_module_from_registration(&registration, be32toh(*(uint32_t*)config_buffer));
        if (r<0) {
            LITEXCNC_ERR_NO_DEVICE("Unknown module id: '%08x'\n", be32toh(*(uint32_t*)config_buffer));
            return -EINVAL;
        }
        config_buffer += 4;
        LITEXCNC_PRINT_NO_DEVICE(" - %s ...", registration->name);
        // Create an instance of this module
        r = registration->initialize(&litexcnc->modules[i], litexcnc, &config_buffer);
        if (r<0) {
            LITEXCNC_ERR_NO_DEVICE("Failed to instantiate module: '%s'\n", registration->name);
            return -EINVAL;
        }
        // Calculate the required buffers for the module
        if (registration->required_config_buffer != NULL) {
            litexcnc->fpga->config_buffer_size += registration->required_config_buffer(litexcnc->modules[i]->instance_data);
        }
        if (registration->required_write_buffer != NULL) {
            litexcnc->fpga->write_buffer_size += registration->required_write_buffer(litexcnc->modules[i]->instance_data);
        }
        if (registration->required_read_buffer != NULL) {
            litexcnc->fpga->read_buffer_size += registration->required_read_buffer(litexcnc->modules[i]->instance_data);
        }
        // Done and go to the next module
        rtapi_print(" done!\n");
    }


    // ==============
    // CREATE BUFFERS
    // ==============
    LITEXCNC_PRINT_NO_DEVICE("Creating read and write buffers...\n");
    
    // - determine the addresses
    litexcnc->fpga->init_base_address = 0x0;
    litexcnc->fpga->reset_base_address = litexcnc->fpga->init_base_address + LITEXCNC_HEADER_DATA_READ_SIZE + be16toh(header_data.module_data_size);
    litexcnc->fpga->config_base_address = litexcnc->fpga->reset_base_address + LITEXCNC_RESET_HEADER_SIZE;
    litexcnc->fpga->write_base_address = litexcnc->fpga->config_base_address + litexcnc->fpga->config_buffer_size;
    litexcnc->fpga->read_base_address = litexcnc->fpga->write_base_address + litexcnc->fpga->write_buffer_size;

    LITEXCNC_PRINT_NO_DEVICE("Base addresses: init: %08X, reset: %08X, config: %08X, write: %08X, read: %08X \n",
        (unsigned int) litexcnc->fpga->init_base_address,
        (unsigned int) litexcnc->fpga->reset_base_address,
        (unsigned int) litexcnc->fpga->config_base_address,
        (unsigned int) litexcnc->fpga->write_base_address,
        (unsigned int) litexcnc->fpga->read_base_address
    );
    
    // - write buffer
    LITEXCNC_PRINT_NO_DEVICE(" - Write buffer: %zu bytes\n", litexcnc->fpga->write_buffer_size);
    litexcnc->fpga->write_buffer_size += litexcnc->fpga->write_header_size;
    uint8_t *write_buffer = rtapi_kmalloc(litexcnc->fpga->write_buffer_size, RTAPI_GFP_KERNEL);
    if (litexcnc == NULL) {
        LITEXCNC_PRINT_NO_DEVICE("out of memory!\n");
        r = -ENOMEM;
        goto fail1;
    }
    memset(write_buffer, 0, litexcnc->fpga->write_buffer_size);
    litexcnc->fpga->write_buffer = write_buffer;

    // - read buffer
    LITEXCNC_PRINT_NO_DEVICE(" - Read buffer: %zu bytes\n", litexcnc->fpga->read_buffer_size);
    litexcnc->fpga->read_buffer_size += litexcnc->fpga->read_header_size;
    uint8_t *read_buffer = rtapi_kmalloc(litexcnc->fpga->read_buffer_size, RTAPI_GFP_KERNEL);
    if (litexcnc == NULL) {
        LITEXCNC_PRINT_NO_DEVICE("out of memory!\n");
        r = -ENOMEM;
        goto fail1;
    }
    memset(read_buffer, 0, litexcnc->fpga->read_buffer_size);
    litexcnc->fpga->read_buffer = read_buffer;

    
    // ================
    // EXPORT FUNCTIONS
    // ================
    LITEXCNC_PRINT_NO_DEVICE("Exporting functions...\n");
    // - read function
    char name[HAL_NAME_LEN + 1];
    rtapi_snprintf(name, sizeof(name), "%s.read", litexcnc->fpga->name);
    r = hal_export_funct(name, litexcnc_read, litexcnc, 1, 0, litexcnc->fpga->comp_id);
    if (r != 0) {
        LITEXCNC_ERR("error %d exporting read function %s\n", litexcnc->fpga->name, r, name);
        r = -EINVAL;
        goto fail1;
    }

    // - write function
    rtapi_snprintf(name, sizeof(name), "%s.write", litexcnc->fpga->name);
    r = hal_export_funct(name, litexcnc_write, litexcnc, 1, 0, litexcnc->fpga->comp_id);
    if (r != 0) {
        LITEXCNC_ERR("error %d exporting read function %s\n", litexcnc->fpga->name, r, name);
        r = -EINVAL;
        goto fail1;
    }


    // ==========
    // RESET FPGA
    // ==========
    r = litexcnc_reset(litexcnc->fpga);
    if (r != 0) {
        goto fail1;
    }

    // Succes
    return 0;

fail1:
    litexcnc_cleanup(litexcnc);  // undoes the rtapi_kmallocs from hm2_parse_module_descriptors()

fail0:
    rtapi_list_del(&litexcnc->list);
    rtapi_kfree(litexcnc);
    return r;
}


EXPORT_SYMBOL_GPL(litexcnc_register_module);
size_t litexcnc_register_module(litexcnc_module_registration_t *registration) {
    rtapi_list_add_tail(&registration->list, &litexcnc_modules);
    LITEXCNC_PRINT_NO_DEVICE("Registered module %s\n", registration->name);
    return 0;
}


EXPORT_SYMBOL_GPL(litexcnc_register_driver);
size_t litexcnc_register_driver(litexcnc_driver_registration_t *registration) {
    rtapi_list_add_tail(&registration->list, &litexcnc_drivers);
    LITEXCNC_PRINT_NO_DEVICE("Registered driver %s\n", registration->name);
    return 0;
}


size_t register_module(char *name) {
    int result;

    //  Load the DLL of the module
    char module_path[LINELEN+1];
    snprintf(module_path, LINELEN, "%s/litexcnc_%s.so", EMC2_RTLIB_DIR, name);
    void *module = loaded_modules[loaded_modules_count] = dlopen(module_path, RTLD_GLOBAL | RTLD_NOW);
    if(!module) {
        rtapi_print_msg(RTAPI_MSG_ERR, "Cannot load default module '%s': %s\n", name, dlerror());
        return -1;
    }

    // Find the function which initializes the module
    char register_name[LINELEN+1];
    snprintf(register_name, LINELEN, "register_%s_module", name);
    int (*register_module)(void) = dlsym(module, register_name);
    if(!register_module) {
        rtapi_print_msg(RTAPI_MSG_ERR, "%s: dlsym: %s\n", name, dlerror());
        dlclose(module);
        return -1;
    }

    // Execute the function to start the module
    if ((result=register_module()) < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR, "%s: rtapi_app_main: %s (%d)\n",
            name, strerror(-result), result);
        dlclose(module);
        return result;
    }

    // succes, get ready for the next one
    loaded_modules_count++;
    return 0;
}

size_t register_driver(char *name) {
    int result;

    //  Load the DLL of the module
    char driver_path[LINELEN+1];
    snprintf(driver_path, LINELEN, "%s/litexcnc_%s.so", EMC2_RTLIB_DIR, name);
    void *driver = loaded_drivers[loaded_drivers_count] = dlopen(driver_path, RTLD_GLOBAL | RTLD_NOW);
    if(!driver) {
        rtapi_print_msg(RTAPI_MSG_ERR, "Cannot load default board driver '%s': %s\n", name, dlerror());
        return -1;
    }

    // Find the function which initializes the module
    char register_name[LINELEN+1];
    snprintf(register_name, LINELEN, "register_%s_driver", name);
    int (*register_driver)(void) = dlsym(driver, register_name);
    if(!register_driver) {
        rtapi_print_msg(RTAPI_MSG_ERR, "%s: dlsym: %s\n", name, dlerror());
        dlclose(driver);
        return -1;
    }

    // Execute the function to start the module
    if ((result=register_driver()) < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR, "%s: rtapi_app_main: %s (%d)\n",
            name, strerror(-result), result);
        dlclose(driver);
        return result;
    }

    // succes, get ready for the next one
    loaded_drivers_count++;
    return 0;
}


int rtapi_app_main(void) {

    size_t i;
    int ret;

    LITEXCNC_PRINT_NO_DEVICE("Loading Litex CNC driver version %u.%u.%u\n", LITEXCNC_VERSION_MAJOR, LITEXCNC_VERSION_MINOR, LITEXCNC_VERSION_PATCH);

    comp_id = hal_init(LITEXCNC_NAME);
    if(comp_id < 0) return comp_id;
    
    RTAPI_INIT_LIST_HEAD(&litexcnc_list);
    RTAPI_INIT_LIST_HEAD(&litexcnc_modules);
    RTAPI_INIT_LIST_HEAD(&litexcnc_drivers);

    // Load default modules
    int result;
    LITEXCNC_PRINT_NO_DEVICE("Loading and registering default modules:\n");
    LITEXCNC_LOAD_MODULE("gpio")
    LITEXCNC_LOAD_MODULE("pwm")
    LITEXCNC_LOAD_MODULE("encoder")
    LITEXCNC_LOAD_MODULE("stepgen")
    for(i = 0, ret = 0; ret == 0 && i<MAX_EXTRAS && extra_modules[i] && *extra_modules[i]; i++) {
        LITEXCNC_LOAD_MODULE(extra_modules[i])
    }

    // Connect to the boards
    if (connections[0]) {
        LITEXCNC_PRINT_NO_DEVICE("Setting up board drivers: \n");
        char *conn_str_ptr;
        for(i = 0, ret = 0; ret == 0 && i<MAX_EXTRAS && connections[i] && *connections[i]; i++) {
            // Check whether the connection contains a colon (:), which indicates
            // the split between driver type and the connection string.
            conn_str_ptr = strchr(connections[i], ':'); // Find first ':' starting from 'p'
            if (conn_str_ptr != NULL) {
                *conn_str_ptr = '\0';          // Replace ':' with a null terminator
                ++conn_str_ptr;                // Move port pointer forward
            } else {
                // TODO: error
            }
            // Try to load the module
            litexcnc_driver_registration_t *registration;
            ret = retrieve_driver_from_registration(&registration, connections[i]);
            if (ret < 0) {
                // The driver is not loaded yet -> load the driver
                ret = register_driver(connections[i]); 
                if (ret<0) return ret;
                ret = retrieve_driver_from_registration(&registration, connections[i]);
                if (ret<0) {
                    LITEXCNC_ERR_NO_DEVICE("Error, could not find driver %s after registration.\n", connections[i]);
                    return ret;
                }
            }
            // Connect with the board
            ret = registration->initialize_driver(conn_str_ptr, comp_id);
            if (ret<0) {
                LITEXCNC_ERR_NO_DEVICE("Failed to initialize the driver.\n");
                return ret;
            }
        }
    }

    // Report ready to rumble
    hal_ready(comp_id);
    return 0;
}


void rtapi_app_exit(void) {

    // Reset the FPGA to its known state (stop coolant, turn of LED's, etc).
    // It is assumed that when the card is reset, it will be reset to a safe
    // state.
    struct rtapi_list_head *ptr;
    rtapi_list_for_each(ptr, &litexcnc_list) {
        litexcnc_t* board = rtapi_list_entry(ptr, litexcnc_t, list);
        litexcnc_reset(board->fpga);
        // Unload driver
        if (board->fpga->terminate) {
            board->fpga->terminate(board->fpga);
        }
    }

    // Exit the component
    hal_exit(comp_id);
    LITEXCNC_PRINT_NO_DEVICE("LitexCNC driver unloaded \n");
}


// Include all other files. This makes separation in different files possible,
// while still compiling a single file. NOTE: the #include directive just copies
// the whole contents of that file into this source-file.
#include "watchdog.c"
#include "wallclock.c"
