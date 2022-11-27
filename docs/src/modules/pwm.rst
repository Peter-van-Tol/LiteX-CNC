===
PWM
===



Configuration
=============


HAL
===

.. note::
    The input and output pins are seen from the module. I.e. the GPIO In module will take an
    value from the machine and will put this on its respective _output_ pins. While the GPIO
    Out module will read the value from it input pins and put the value on the physical pins.
    This might feel counter intuitive at first glance.

Input pins
----------

.. list-table:: Input pins
   :widths: auto
   :header-rows: 1

   * - Name
     - Type
     - Description
   * - | <board-name>.gpio.<n>.out
       | <board-name>.gpio.<name>.out
     - HAL_BIT
     - Drives a physical output pin.

Output pins
-----------

.. list-table:: Output pins
   :widths: auto
   :header-rows: 1

   * - Name
     - Type
     - Description
   * - | <board-name>.gpio.<n>.in
       | <board-name>.gpio.<name>.in
     - HAL_BIT
     - Tracks a physical input pin.
   * - | <board-name>.gpio.<n>.in_not
       | <board-name>.gpio.<name>.in_not
     - HAL_BIT
     - Tracks a physical input pin, but inverted.

Parameters
----------

.. list-table:: Parameters
   :widths: auto
   :header-rows: 1

   * - Name
     - Type
     - Description
   * - | <board-name>.gpio.<n>.invert_output
       | <board-name>.gpio.<name>.invert_output
     - HAL_BIT
     - Inverts an output pin.

Example
-------

...