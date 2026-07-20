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

.. toctree::
   :maxdepth: 2

   Watchdog <watchdog>
   GPIO <gpio>
   PWM <pwm>
   StepGen <stepgen>
   Encoder <encoder>
 