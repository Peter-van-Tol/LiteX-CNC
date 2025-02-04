===
PWM
===

The module ``PMW`` is used to generate PWM (pulse width modulation) or PDM (pulse density modulation)
signals. It is meant as a drop-in replacement for the `LinuxCNC PWMGEN component <https://linuxcnc.org/docs/html/man/man9/pwmgen.9.html>`_ ,
although this module only supports a single output.

  | **Difference betweeen PDM and PWM**
  | Pulse-width modulation (PWM) is a special case of PDM where the switching frequency is fixed 
  | and althe pulses corresponding to one sample are contiguous in the digital signal. For a 50% 
  | voltage with a resolution of 8-bits, a PWM waveform will turn on for 128 clock cycles and then 
  | off for the remaining 128 cycles. With PDM and the same clock rate the signal would alternate 
  | between on and off every other cycle. The average is 50% for both waveforms, but the PDM signal 
  | switches more often. For 100% or 0% level, they are the same.

From the definition above it is important to not that although the resolution of PDM is better, the frequency
is much higher. When using PDM it is important that the hardware can handle these frequencies, which may be
up to 50% of the board frequency.

Configuration
=============

The code-block belows gives an example for the configuration of ``PWM``.

.. code-block:: json

  ...
  "modules": [
    ...,
    {
      "module_type": "pwm",
      "instances": [
        {"pin": "j2:0"},
        {
          "pin":"j2:1",
          "name": "optional_name_input"
        },
        ...,
        {"pin": "j2:5"}
      ]
    },
    ...
  ]
  ...


Defining the pin is required in the configuration. Optionally one can give the pin a name which
will be used as an alias in HAL. When no name is given, no entry in the file containnig the
aliases will be generated. 

.. warning::
  When *inserting* new pins in the list and the firmware is re-compiled, this will lead to a renumbering
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

<board-name>.pwm.<n>.enable / <board-name>.pwm.<name>.enable (HAL_BIT)
    Enables PWM generator N - when false, the physical output pins are low.
<board-name>.pwm.<n>.value / <board-name>.pwm.<name>.value (HAL_FLOAT)
    Commanded value. When value = 0.0, duty cycle is 0%, and when value = ±scale, duty cycle is
    ± 100%. (Subject to min-dc and max-dc limitations.)
<board-name>.pwm.<n>.scale / <board-name>.pwm.<name>.scale (HAL_FLOAT)
    ..
<board-name>.pwm.<n>.offset / <board-name>.pwm.<name>.offset (HAL_FLOAT)
    These parameters provide a scale and offset from the value pin to the actual duty cycle. 
    The duty cycle is calculated according to duty_cycle = (value/scale) + offset, with 1.0
    meaning 100%.
<board-name>.pwm.<n>.pwm_freq / <board-name>.pwm.<name>.pwm_freq (HAL_FLOAT)
    PWM frequency in Hz. The upper limit is half of the frequency of the FPGA, and values above 
    that limit will be changed to the limit. A value of zero produces Pulse Density Modulation 
    (PDM) instead of Pulse Width Modulation (PWM).
<board-name>.pwm.<n>.min_dc / <board-name>.pwm.<name>.min_dc (HAL_FLOAT)
    The minimum duty cycle. A value of 1.0 corresponds to 100%. Note that when the pwm generator
    is disabled, the outputs are constantly low, regardless of the setting of min-dc.
<board-name>.pwm.<n>.max_dc / <board-name>.pwm.<name>.mac_dc (HAL_FLOAT)
    The maximum duty cycle. A value of 1.0 corresponds to 100%. This can be useful when using
    transistor drivers with bootstrapped power supplies, since the supply requires some low
    time to recharge. The maximum duty cycle must be lower then the minimum duty cycle. If the 
    maximum duty cycle is lower then the minimum duty cycle, it will be changed to this limit.

Output pins
-----------

<board-name>.pwm.<n>.curr_period / <board-name>.pwm.<name>.curr_period (HAL_INT)
    The current PWM period in clock-cycles (DEBUG)
<board-name>.pwm.<n>.curr_width / <board-name>.pwm.<name>.curr_width (HAL_INT)
    The current PWM width in clock-cycles (DEBUG)

Parameters
----------

The module ``PWM`` does not have parameters.

Example
-------

In the example below a spindle is wired to the HAL using PWM. The direction of the
spindle rotation is set using GPIO.

.. code-block::

    loadrt threads name1=servo-thread period1=10000000
    loadrt litexcnc connections="<connnection_string>"
    
    # Add the functions to the HAL
    addf <board-name>.read test-thread
    ...
    addf <board-name>.write test-thread

    # Connect the spindle with the PWM generator
    net spindle-speed-cmd spindle.0.speed-out => <board-name>.pwm.0.value
    net spindle-on spindle.0.on => <board-name>.pwm.0.enable
    # Set the spindle's top speed in RPM (assuming a Chinese High-Speed spindle)
    setp pwmgen.0.scale 24000
    # Connect the direction of the spindle (in this case named pins are used)
    net spindle-fwd spindle.0.forward => <board-name>.gpio.spindle-fwd.out
    net spindle-rev spindle.0.reverse => <board-name>.gpio.spindle-rev.out

Break-out boards
================

There is currently no dedicated break-out board available for PWM. As an alternative
the break-out board for th `12 channel sourcing output <https://github.com/Peter-van-Tol/HUB-75-boards/tree/main/HUB75-Sourcing_output>`_
can be used, although the frequency has to be limited to suit the requirements of the
opto-couplers.
