
#include <stdio.h>
#include <json-c/json.h>

#include "rtapi.h"
#include "rtapi_app.h"

#include "litexcnc.h"
#include "gpio.h"


static int litexcnc_gpio_out_init(litexcnc_t *litexcnc, json_object *config) {

    int r;
    struct json_object *gpio;
    struct json_object *gpio_pin;
    struct json_object *gpio_pin_name;

    char name[HAL_NAME_LEN + 1];
    
    if (json_object_object_get_ex(config, "gpio_out", &gpio)) {
        litexcnc->gpio.num_output_pins = json_object_array_length(gpio);
        // Allocate the module-global HAL shared memory
        litexcnc->gpio.output_pins = (litexcnc_gpio_output_pin_t *)hal_malloc(litexcnc->gpio.num_output_pins * sizeof(litexcnc_gpio_output_pin_t));
        if (litexcnc->gpio.output_pins == NULL) {
            LITEXCNC_ERR_NO_DEVICE("Out of memory!\n");
            r = -ENOMEM;
            return r;
        }
        // Create the pins and params in the HAL
        for (size_t i=0; i<litexcnc->gpio.num_output_pins; i++) {
            gpio_pin = json_object_array_get_idx(gpio, i);
            // Pin for the output
            if (json_object_object_get_ex(gpio_pin, "name", &gpio_pin_name)) {
                rtapi_snprintf(name, sizeof(name), "%s.gpio.%s.out", litexcnc->fpga->name, json_object_get_string(gpio_pin_name));
            } else {
                rtapi_snprintf(name, sizeof(name), "%s.gpio.%02d.out", litexcnc->fpga->name, i);
            }
            r = hal_pin_bit_new(name, HAL_IN, &(litexcnc->gpio.output_pins[i].hal.pin.out), litexcnc->fpga->comp_id);
            if (r < 0) {
                LITEXCNC_ERR_NO_DEVICE("Error adding pin '%s', aborting\n", name);
                return r;
            }
            // Parameter for inverting the output
            if (json_object_object_get_ex(gpio_pin, "name", &gpio_pin_name)) {
                rtapi_snprintf(name, sizeof(name), "%s.gpio.%s.invert_output", litexcnc->fpga->name, json_object_get_string(gpio_pin_name));
            } else {
                rtapi_snprintf(name, sizeof(name), "%s.gpio.%02d.invert_output", litexcnc->fpga->name, i);
            }
            r = hal_param_bit_new(name, HAL_RW, &(litexcnc->gpio.output_pins[i].hal.param.invert_output), litexcnc->fpga->comp_id);
            if (r < 0) {
                LITEXCNC_ERR_NO_DEVICE("Error adding param '%s', aborting\n", name);
                return r;
            }
        }
        // Free up the memory
        json_object_put(gpio_pin_name);
        json_object_put(gpio_pin);
        json_object_put(gpio);
    }

    return 0;
}

static int litexcnc_gpio_in_init(litexcnc_t *litexcnc, json_object *config) {
    
    int r;
    struct json_object *gpio;
    struct json_object *gpio_pin;
    struct json_object *gpio_pin_name;

    char name[HAL_NAME_LEN + 1];
    char name_inverted[HAL_NAME_LEN + 1];
    
    if (json_object_object_get_ex(config, "gpio_in", &gpio)) {
        litexcnc->gpio.num_input_pins = json_object_array_length(gpio);
        // Allocate the module-global HAL shared memory
        litexcnc->gpio.input_pins = (litexcnc_gpio_input_pin_t *)hal_malloc(litexcnc->gpio.num_input_pins * sizeof(litexcnc_gpio_input_pin_t));
        if (litexcnc->gpio.input_pins == NULL) {
            LITEXCNC_ERR_NO_DEVICE("out of memory!\n");
            r = -ENOMEM;
            return r;
        }
        // Create the pins in the HAL
        for (size_t i=0; i<litexcnc->gpio.num_input_pins; i++) {
            gpio_pin = json_object_array_get_idx(gpio, i);
            // Normal pin
            if (json_object_object_get_ex(gpio_pin, "name", &gpio_pin_name)) {
                rtapi_snprintf(name, sizeof(name), "%s.gpio.%s.in", litexcnc->fpga->name, json_object_get_string(gpio_pin_name));
                rtapi_snprintf(name_inverted, sizeof(name_inverted), "%s.gpio.%s.in-not", litexcnc->fpga->name, json_object_get_string(gpio_pin_name));
            } else {
                rtapi_snprintf(name, sizeof(name), "%s.gpio.%02d.in", litexcnc->fpga->name, i);
                rtapi_snprintf(name_inverted, sizeof(name_inverted), "%s.gpio.%02d.in-not", litexcnc->fpga->name, i);
            }
            r = hal_pin_bit_new(name, HAL_OUT, &(litexcnc->gpio.input_pins[i].hal.pin.in), litexcnc->fpga->comp_id);
            if (r < 0) {
                LITEXCNC_ERR_NO_DEVICE("error adding pin '%s', aborting\n", name);
                return r;
            }
            r = hal_pin_bit_new(name_inverted, HAL_OUT, &(litexcnc->gpio.input_pins[i].hal.pin.in_not), litexcnc->fpga->comp_id);
            if (r < 0) {
                LITEXCNC_ERR_NO_DEVICE("error adding pin '%s', aborting\n", name);
                return r;
            }
        }
        // Free up the memory
        json_object_put(gpio_pin_name);
        json_object_put(gpio_pin);
        json_object_put(gpio);
    }

    return 0;
    
}

int litexcnc_gpio_init(litexcnc_t *litexcnc, json_object *config) {
    int r;

    r = litexcnc_gpio_in_init(litexcnc, config);
    if (r<0) {
        return r;
    }
    r = litexcnc_gpio_out_init(litexcnc, config);
    if (r<0) {
        return r;
    }

    return r;
}

uint8_t litexcnc_gpio_prepare_write(litexcnc_t *litexcnc, uint8_t **data) {

    if (litexcnc->gpio.num_output_pins == 0) {
        return 1;
    }

    // Process all the bytes
    uint8_t mask = 0x80;
    for (size_t i=LITEXCNC_BOARD_GPIO_DATA_WRITE_SIZE(litexcnc)*8; i>0; i--) {
        // The counter i can have a value outside the range of possible pins. We only
        // should add data from existing pins
        if (i <= litexcnc->gpio.num_output_pins) {
            *(*data) |= (*(litexcnc->gpio.output_pins[i-1].hal.pin.out) ^ litexcnc->gpio.output_pins[i-1].hal.param.invert_output)?mask:0;
        }
        // Modify the mask for the next. When the mask is zero (happens in case of a 
        // roll-over), we should proceed to the next byte and reset the mask.
        mask >>= 1;
        if (!mask) {
            mask = 0x80;  // Reset the mask
            (*data)++; // Proceed the buffer to the next element
        }
    }

}


uint8_t litexcnc_gpio_process_read(litexcnc_t *litexcnc, uint8_t** data) {

    if (litexcnc->gpio.num_input_pins == 0) {
        return 1;
    }

    // Process all the bytes
    uint8_t mask = 0x80;
    for (size_t i=LITEXCNC_BOARD_GPIO_DATA_READ_SIZE(litexcnc)*8; i>0; i--) {
        // The counter i can have a value outside the range of possible pins. We only
        // should add data to existing pins
        if (i <= litexcnc->gpio.num_input_pins) {
            if (*(*data) & mask) {
                // GPIO active
                *(litexcnc->gpio.input_pins[i-1].hal.pin.in) = 1;
                *(litexcnc->gpio.input_pins[i-1].hal.pin.in_not) = 0;
            } else {
                // GPIO inactive
                *(litexcnc->gpio.input_pins[i-1].hal.pin.in) = 0;
                *(litexcnc->gpio.input_pins[i-1].hal.pin.in_not) = 1;
            }
        }
        // Modify the mask for the next. When the mask is zero (happens in case of a 
        // roll-over), we should proceed to the next byte and reset the mask.
        mask >>= 1;
        if (!mask) {
            mask = 0x80;  // Reset the mask
            (*data)++; // Proceed the buffer to the next element
        }
    }
}
