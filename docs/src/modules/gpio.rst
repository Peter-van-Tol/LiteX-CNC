====
GPIO
====

The module ``GPIO`` gives acces to the bare pins of the FPGA. The pins can be configured to be
either inputs (from machine to controller) or outputs (from controller to machine). When using
the GPIO as input requires to modify the buffers on the supported FPGA-cards.

Possible usages are:

* Limit switches.
* Push buttons for user panel.
* Control relays for Mist and Flood cooling.
* Control spindle direction.

There is no upper limit for the amount of GPIO a single board supports, other then the number of
pins on the board. Communication to and from the board is done using 32-bit wide words, which can
contain information of up to 32 GPIO pins. When more then 32 pins are used for either direction,
a second 32-bit wide word is automatically used to send or retrieve the information.  

Configuration
=============

The configuration of the GPIO consists of separate blocks for GPIO In and GPIO Out. 

.. code-block:: json

  ...
  "gpio_in": [
        {"pin":"j2:1"},
        {
          "pin":"j2:5",
          "name": "optional_name_input"
        },
        ...
        {"pin": "j3:5"},
    ],
    "gpio_out": [
        {"pin": "j15:1"},
        {
          "pin": "j15:5"
          "name": "optional_name_output"
        },
        ...
        {"pin": "j16:5"},
    ],

Defining the pin is required in the configuration. Optionally one can give the pin a name which
will be used in the HAL. When no name is supplied, the pin is numbered, starting at 0. GPIO 

.. note::
  In and GPIO out are numbered separately, so both ``<board-name>.gpio.0.out`` and ``<board-name>.gpio.0.in``
  will exist, but refer to different physical pins. Therefore it is recommended to use names for the
  pins instead. Personally, I prefer using the connector pins as names, as it is clear which pin will
  be driven or read from the name.

.. warning::
  When _inserting_ new pins in the list and the firmware is re-compiled, this will lead to a renumbering
  of the HAL-pins. When using numbers, it is therefore **strongly** recommended only to append pins to 
  prevent a complete overhaul of the HAL.

HAL
===

.. note::
    The input and output pins are seen from the module. I.e. the GPIO In module will take an
    value from the machine and will put this on its respective _output_ pins. While the GPIO
    Out module will read the value from it input pins and put the value on the physical pins.
    This might feel counter intuitive at first glance.

Input pins
----------

<board-name>.gpio.<n>.out / <board-name>.gpio.<name>.out (HAL_BIT)
    Drives a physical output pin.

Output pins
-----------

<board-name>.gpio.<n>.in / <board-name>.gpio.<name>.in (HAL_BIT)
    Tracks a physical input pin.
<board-name>.gpio.<n>.in-not / <board-name>.gpio.<name>.in-not (HAL_BIT)
    Tracks a physical input pin, but inverted.

Parameters
----------

<board-name>.gpio.<n>.invert_output / <board-name>.gpio.<name>.invert_output (HAL_BIT)
    Inverts an output pin.

Example
-------

...


Break-out boards
================



