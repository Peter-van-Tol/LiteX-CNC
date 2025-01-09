.. _modules:

=======
Modules
=======

The pages below describe the different modules available to Litex-CNC. Any configuration can combine any
number of modules, as long as the following conditions are met:

* The total number of bytes in each frame is limited at 255. The total data package for each read or write
  cannot surpass this limit.
* For inputs it is required to modify the buffers of the FPGA, (`see Chubby75 <https://github.com/q3k/chubby75>`_).
  Each buffer is responsible for 8 output or input pins. Unless the buffers are replaced with wires, the choice
  for pin location is not completely free.

.. warning::
    When replacing the buffers with wires, the pins accept only 3.3 Volt! Also the total power dissipation
    of the FPGA should not be exceeded.

.. info::
    The button and LED on the board can be used in the modules by using ``user_led:1`` and ``user_button:1``
    respectively. 
    
    It should be noted that the LED is wired in such a way that a HIGH signal will turn the LED
    off. Therefore, I used the parameter safe_state to have the LED off when the board is powered on. When
    LinuxCNC is running, the behavior of the LED is controlled with the invert-output HAL param.

    The modules are designed that the ``user_button`` can only be used as an input and ``user_led`` as an
    output. When trying to compile the firmware with the ``user_button`` connected to an ouput an error
    will be generated. Connecting a ``user-button`` to an output will short the FPGA when the output is
    HIGH and the button is pressed, which might damage your FPGA.

.. toctree::
   :maxdepth: 2

   Watchdog <watchdog>
   GPIO <gpio>
   PWM <pwm>
   StepGen <stepgen>
   Encoder <encoder>
 