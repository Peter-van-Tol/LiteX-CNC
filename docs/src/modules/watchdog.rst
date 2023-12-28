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
   Changed in Litex-CNC version 1.2. 

The configuration of the Watchdog consist of an optional Enable-pin. The Enable-pin is
active high as long as the watchdog is happy (communications are working, watchdog did
not bite). The enable pin can for example be used to disconnect all pins (high-Z) when
the watchdog bites; this can ensure a safe-state of the machine when the communications
are lost. 

.. code-block:: json

   ...
   "watchdog": {
      "pin":"ena:0"
   },
   ...

One can opt not to define a Enable-pin, however this is not recommended as there is a
risk of the machine staying active after disruption of the communications. Even when the
enable pin is not defined, the configuration block is required, however it remains empty.

.. code-block:: json

   ...
   "watchdog": {
   },
   ...


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
    loadrt litexcnc
    loadrt litexcnc_eth config_file="<path-to-configuration.json>"
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
