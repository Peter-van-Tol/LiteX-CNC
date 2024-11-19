========
Watchdog
========

The watchdog must be petted by LinuxCNC periodically or it will bite. When the watchdog bites, all 
the board’s I/O pins are pulled low and the stepgen will stop (optionally the stepgen can stop while
respecting the acceleration limits, see description). Encoder instances keep counting quadrature pulses.

The watchdog has a built-in ``estop_latch``, so it can be used as a part of a simple software ESTOP
chain. The pins for the watchdog have a simalar operation as the standard ``estop_latch`` from LinuxCNC.

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
active high as long as the following conditions are met:
- the watchdog did timeout (the pin ``watchdog.has_bitten`` is ``false``)
- the internal ``estop_latch`` is not faulted (the pin ``watchdog.ok-out`` is ``true``)

The enable pin can for example be used to disconnect all pins (high-Z) when
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
The heartbeat can be used to indicate the board is active and the communication is OK. The
heartbeat will continue to beat as long as there is communication, i.e. it will also beat
when the internal ``estop_latch`` has faulted.

There are two built-in waveforms ``heartbeat`` and ``sinus``. Optionally, one can define
a custom waveform by providing a list of floats between 0 and 1. Below gives a basic
configuration for the heartbeat function, using the built-in LED.

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
invert-output (str) - optional
    When set to True, the output signal will be inverted. Default value is False. When
    using the built-in User LED, this option must be set to True.
pwm-frequency (float) - optional
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

Operation
=========
The initial state is "Faulted". When faulted, the ``out-ok`` output is ``false``, the ``fault-out``
output is ``true``. The heartbeat on the watchdog will beat, indicating that communication is OK. Th
enable function will be LOW when the ``ok-out`` is ``false``.

The reset command is send to the watchdog when all these conditions are true:
- ``fault-in`` is ``false``;
- ``ok-in`` is ``true``;
- ``reset`` changes from ``false`` to ``true``.

On the watchdog additional checks will be performed (not implemented yet, but think off E-Stop and
ALARMS tied into the wathdog).

When "OK", the ``out-ok`` output is ``true``, the ``fault-out`` output is ``false``. The enable
function will be HIGH.

The state changes from "OK" to "Faulted" when any of the following are true:
- ``fault-in`` is ``true``;
- ``ok-in`` is ``false``;
- the watchdog has bitten (i.e. communication timeout occurred).

To facilitate using only a single fault source, ``ok-in`` and ``fault-in`` are both set to the
non-fault-causing value when no signal is connected. For estop-latch to ever be able to signal a
fault, at least one of these inputs must be connected.

Input pins
==========

<board-name>.watchdog.timeout-ns (uint32)
   The time out (in ns) after which the watchdog will bite. It is recommended to set the watchdog
   at least 1.5 times the period of the servo-thread to give some leeway. If set too tight, this
   will lead to a watchdog which bites as soon as there is a latency excursion.
<board-name>.watchdog.ok-in hal_bit (default: true)
   Pin indicating the input is OK. Setting this pin to LOW will trip the internal ``estop_latch``.
   Normally this pin is connected to ``iocontrol.0.user-enable-out``.
<board-name>.watchdog.fault-in hal_bit (default: false)
   Pin indicating an (external) fault has occurred. Can be used to connect an external E-Stop to
   the watchdog, for example from an USB-pendant not connected to Litex-CNC FPGA. This pin can also
   be used to connect additional ``estop-latches`` when required.
<board-name>.watchdog.reset hal_bit
   Pin to reset the ``watchdog`` and the its internal ``estop_latch``. The reset command is send to
   the FPGA when this pin changes from ``false`` to ``true``. Additonally, ``fault-in`` must be ``false``
   and ``ok-in`` must be ``true``.

Output pins
===========

<board-name>.watchdog.has-bitten hal_bit
   Flag indicating that the watchdog has not been petted on time and that it has bitten.
<board-name>.watchdog.timeout-cycles
   The number of cycles of the FPGA before the watchdog bites (DEBUG)
<board-name>.watchdog.ok-out hal_bit (default: false)
   Pin indicating the chain is OK when ``true``.
<board-name>.watchdog.fault-out hal_bit (default: true)
   Pin indicating the chain is faulted when ``true``.


Example
=======

Typically, the software EStop is connected to ``ok-in``, ``iocontrol.0.user-request-enable`` is connected
to ``reset``, and ``ok-out`` is connected to ``iocontrol.0.emc-enable-in``.

.. code-block::

    loadrt threads name1=servo-thread period1=10000000
    loadrt litexcnc connections="<connnection_string>"
    loadrt estop_latch
    
    # Add the functions to the HAL
    addf <board-name>.read test-thread
    ...
    addf <board-name>.write test-thread

    # Setup the watchdog (assuming servo-thread period of 1000000 ns (1 kHz))
    setp EMCO5.watchdog.timeout-ns 1500000

    # Tie the watchdog into the E-STOP chain
    net estop-loopout iocontrol.0.emc-enable-in <= EMCO5.watchdog.ok-out
    net estop-loopin iocontrol.0.user-enable-out => EMCO5.watchdog.ok-in
    net estop-reset iocontrol.0.user-request-enable => EMCO5.watchdog.reset
