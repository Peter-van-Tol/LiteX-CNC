========
Watchdog
========

The watchdog must be petted by LinuxCNC periodically or it will bite. When the watchdog bites, all 
the board’s I/O pins are pulled low and the stepgen will stop (optionally the stepgen can stop while
respecting the acceleration limits, see description). Encoder instances keep counting quadrature pulses.

Resetting the watchdog resumes normal operation of the FPGA.

.. info::
   The Watchdog is a module which must be present in the configuration of the FPGA. It is therefore
   not part of the ``modules`` section; it is part of the card configuration.

Configuration
=============

.. info::
   Changed in Litex-CNC version 2.0. 

The configuration of the watchdog consist of an optional list of Watchdog functions. These
functions are active when the watchdog is enabled and did has not bitten. Examples of these
functions are an Enable-pin, a LED showing a heartbeat (or other pattern) and a chargepump.

One can opt not to define functions, however this is not recommended as there is a
risk of the machine staying active after disruption of the communications. Even when the
functions are not defined, the configuration block is required, however it remains empty.

.. code-block:: json

   ...
   "watchdog": {
   },
   ...

Enable-pin
----------
The configuration of the Watchdog consist of an optional Enable-pin. The Enable-pin is
active high as long as the watchdog is happy (communications are working, watchdog did
not bite). The enable pin can for example be used to disconnect all pins (high-Z) when
the watchdog bites; this can ensure a safe-state of the machine when the communications
are lost. 

.. code-block:: json

   ...
   "watchdog": {
      "functions": [
         {
            "function_type": "enable",
            "pin": "ena:0"
         }
      ]
   },
   ...

**Configuration options**
function_type (str) - required
    Identifier for the function. Must be ``enable`` to implement this function.
pin (str) - required
    The pin on which the enable signal will be put.
invert_output (str) - optional
    When set to True, the output signal will be inverted. Default value is False. When
    using the built-in User LED, this option must be set to True.

Heartbeat
---------
The heatbeat can be used to indicate the board is active and the watchdog is enabled and
did not bite yet. There are two built-in waveforms ``heartbeat`` and ``sinus``. Optionally,
one can define a custom waveform by providing a list of floats between 0 and 1. Below gives
a basic configuration for the heartbeat function, using the built-in LED.

.. code-block:: json

   ...
   "watchdog": {
      "functions": [
         {
            "function_type": "heartbeat",
            "pin": "user_led:1",
            "invert_output": true,
            "waveform": "heartbeat",
            "speed": 0.857
         }
      ]
   },
   ...

**Configuration options**
function_type (str) - required
    Identifier for the function. Must be ``heartbeat`` to implement this function.
pin (str) - required
    The pin on which the heart beat signal (PWM) will be put.
invert_output (str) - optional
    When set to True, the output signal will be inverted. Default value is False. When
    using the built-in User LED, this option must be set to True.
pwm_frequency (float) - optional
    The PWM frequency of the signal. Default value is 1 kHz.
waveform (str | List[float]) - optional
    The waveform to be put on the pin. The available built-in waveforms are ``heartbeat`` 
    and ``sinus``. Alternatively, a list of floats between 0 and 1 can be given to create
    custom patterns
speed (float) - optional
    The speed of the waveform in Hz. A value of 1 Hz means that the waveform is repeated
    every second.

Charge pump
-----------
The Charge Pump signal is a signal which is active if the controller (watchdog) is enabled
and has not bitten. This signal can be used eg. for external control of a safety circuitry. 

...Work-in-progress..


Input pins
==========

.. csv-table:: Input pins
   :header: "Name", "Type", "Description"
   :widths: auto
   
   "<board-name>.watchdog.timeout_ns", "integer", "The time out (in ns) after which the watchdog will bite. It is recommended to set the watchdog at least 1.5 times the period of the servo-thread to give some leeway. If set too tight, this will lead to a watchdog which bites as soon as there is a latency excursion."


Output pins
===========

.. csv-table:: Output pins
   :header: "Name", "Type", "Description"
   :widths: auto
   
   "<board-name>.watchdog.has_bitten", "hal_bit (i/o)", "Flag indicating that the watchdog has not been petted on time and that it has bitten. should be set to False or 0 to restart the working of the FPGA"
   "<board-name>.watchdog.timeout_cycles", "integer", "The number of cycles of the FPGA before the watchdog bites (DEBUG)."

Example
=======

.. code-block::

    loadrt threads name1=servo-thread period1=10000000
    loadrt litexcnc connections="<connnection_string>"
    loadrt estop_latch
    
    # Add the functions to the HAL
    addf <board-name>.read test-thread
    ...
    addf estop-latch.0 servo-thread
    ...
    addf <board-name>.write test-thread

    # Setup the watchdog (assuming servo-thread period of 1000000 ns (1 kHz))
    setp EMCO5.watchdog.timeout_ns 1500000

    # Tie the watchdog into the E-STOP chain
    net estop-loopout iocontrol.0.emc-enable-in <= estop-latch.0.ok-out
    net estop-loopin iocontrol.0.user-enable-out => estop-latch.0.ok-in
    net estop-reset iocontrol.0.user-request-enable => estop-latch.0.reset
    net remote-estop estop-latch.0.fault-in <= <board-name>.watchdog.has_bitten

    # More sources for E-stop (such as a GPIO-in) can be added to this E-Stop circuit.
