=========
Changelog
=========

All versions in this changelog have two entries: ``driver`` and ``firmware``. The firmware and driver should
have the same version, as communication protocol might change between versions. In the firmware/driver there
is a safeguard to prevent miscommunication.

Version 1.3.4
=============

An error in ``stepgen`` related to index pulses has been resolved in this version. The error prevented the
firmware from correctly setting the ``dir``-pin. Recompilation of firmware is required. The correct working
of the ``stepgen`` has been tested on a real machine, instead on synthesis. Many thanks LisovR for debugging
this module.

Version 1.3.0 to 1.3.3 are now `yanked <https://pypi.org/help/#yanked>`_ because of not working ``stepgen`` module. 

Version 1.3.3 - yanked
======================

An error in ``stepgen`` related to index pulses has been resolved in this version. No need to re-compile the
firmware, only reinstallation of the drivers is required.

Version 1.3.2 - yanked
======================

An error in ``stepgen`` has been resolved in this version. Upgrading from version 1.3.0 or 1.3.1 to this version
requires re-compilation of the firmware and re-installation of the drivers.

* ``firmware``:

  * ``stepgen``: config on the stepgen was not correctly stored on the FPGA, hence the defined stepgens
    were not detected by the driver and with more then 3 stepgens errors were generated if encoders were
    also defined in the configuration.

* ``driver``:

  * ``stepgen``: when using 3, 7 , 11, and so on stepgens, the ``encoder`` module would fail.

Version 1.3.1 - yanked
======================

Due to an upgrade of the build-system, the script ``litexcnc`` was not installed on all systems. In this version
this has been patched.

Version 1.3.0 - yanked
======================

.. info::
  For this version the configuration file has to be changed. The section ``watchdog: {}`` has been expanded
  with extra functions. See: `watchdog documentation <https://litex-cnc.readthedocs.io/en/latest/modules/watchdog.html>`_ 


* ``firmware``:

  * ``watchdog``: completly new watchdog. Functions can be added to the ``watchdog``, such as a heartbeat or
    an enable signal. The enable signal can for example be used to disable the stepper drivers when the watchdog
    has detected an error. Also E-stop can be directly be wired into the ``watchdog``, making them an integral
    part of the E-stop chain.
  * ``stepgen``: the ``max_frequency`` of the stepgen can be set. The default value is 400 kHz (equal to prior
    versions). With the ``max_frequency`` and the clock frequency of the FPGA the optimal shift is calculated
    ensuring the maximum resolution, whilst assuring the requested maximum frequency. 
  * ``pwm``: Supports two additional modes: ``PWM/direction`` and ``UP/DOWN``.
  * ``encoder``: Supports counter mode.
  * ``gpio``: Fixed bug in building firmware when all ``gpio`` are either input or output.
  * the LED and button on the 5A-75B and 5A-75E can be used in the firmware.
  * toolchain: fixed bug in detecting OS.
  * toolchain: bumped version of Litex to 2024.12.
  * toolchain: bumped version of OSS-cad-suite to 20241231.

* ``driver``:

  * ``watchdog``:  completly new watchdog. The driver watchdog is implemented as a E-stop latch, so it can be used
    within an E-stop chain in the HAL. When the watchdog has bitten, it has to be actively reset by resetting the
    E-stop latch to resume command of the FPGA. Examples on how to tie the ``watchdog`` into HAL are provided in the
    documentation.
  * ``stepgen``: Added ``index-enable`` and ``index-pulse``. When the ``index-enable`` is HIGH and a pulse is received
    on ``index-pulse`` the counter of the ``stepgen`` is reset to 0.
  * ``pwm``: the signal of PWM can be inverted.
  * all pin names are now consistently using ``-`` as seperator (i.e. ``index-enable`` instead of ``index_enable``)

Version 1.2.4
=============

Bugfix version to fix timings for stepgen.

* ``driver``:

  * ``stepgen``: Fix timings (#59). Thanks to *hmnijp* for testing the ``stepgen`` module with a scope.
  * ``stepgen``: Fix check on maximum speed obeys the maximum step frequency (#59).


Version 1.2.3
=============

Quick bugfix to solve an identation error in the ``cli``. 

Version 1.2.2
=============

Bugfix version for ``stepgen`` module, which was affected by an error in the ``encoder`` module. 

Due to an upgrade in the detection / comparison of the version of the firmware this version will show a 
warning message if older firmware is used (NOTE: the communication has not changed, thus re-compilation is
not required). In the next minor release (i.e. version 1.3) this change will correctly enforce recompilation
of the firmware. A change in minor version (i.e. 1.2 -> 1.3) indicates a modification in communication
protocol and thus requires an update of the firmware.

* ``driver``:

  * ``encoder``: counts and position of encoder can be reset (#74)
  * ``encoder``: incorrect required read buffer reported has been corrected. This affected the module ``stepgen`` (#79)
  * ``watchdog``: the watchdog is reset when the card is reset. Prevents the ``has_bitten`` message when LinuxCNC
    is restarted without power-cycling the card. (#80)
  * Fixed showing the correct version of LitexCNC in HAL.
  * Temporary fixed comparison between version of FPGA and driver. When the minor version changes in an upcoming
    release, the driver will now correctly enforce a recompilation of the firmware.

* ``cli``:

  * ``install_toolchain``: Fixed issue with detection of OS on desktop. Also ``git`` and ``openocd`` are installed
    on x64-architecture systems (#78).


Version 1.2.1
=============

Bugfix version for CLI on RPi images from LinuxCNC.

* ``cli``:

  * ``install_toolchain``: The path variable is now persisted in ``.bashrc`` instead of ``.profile``. This will
    make the toolchain available in the terminal when using from desktop. Previously only SSH was working.

Version 1.2
===========

Support for Raspberry Pi 5 added (using ``spidev`` only at this moment).

.. info::
  For this version the configuration file has to be changed. The section ``watchdog: {}`` has to be
  added to the configuration in order to compile the firmware. Optionally, one can specify an enable
  out pin. See for more information the `watchdog documentation <https://litex-cnc.readthedocs.io/en/latest/modules/watchdog.html>`_ 

* ``cli``:

  * ``install_toolchain``: OpenOCD compiled with support for the  ``libgpiod``-driver. Required for support
    of the Raspberry PI (#64).
  * ``install_toolchain``: Fixed issue with 64-bit OS on RaspberryPi.
  * ``flash_firmware``: Added configuration for ``libgpiod``-driver. Auto-detects whether a Raspberry Pi 5 is
    used and changes to the new configuration in that case (#64).

* ``driver``:

  * ``watchdog``: Watchdog requires configuration. Optional an enable out pin can be set for the Watchdog (#65).
  * ``HUB75HAT``: Corrected pinout (#68)
  * ``gpio``: Fixed issue with configurations with either all inputs or all outputs (#70)


Version 1.1.1
=============

Bugfixes for CLI-command.

* ``cli``:

    * ``install_toolchain``: Resolved issue creating nested directories.
    * ``install_toolchain``: The architecture ``i386`` is not supported by OSS-cad-suite. When this
      architecture is detected, a clear error message will be given and OSS-cad-suite is not
      installed.


Version 1.1.0
=============

Added new drivers for SPI communiction [48]. For current users this requires a minor modification in their
configuration files. The connection entry in the config-file must be altered to include the field ``connection_type``.
See for further guidelines on the parameters for the connection, including the new SPI connection, the
updates help-files.

Further minor changes:

* ``cli``:

  * ``install_litex``: DEPRECATED, the functionality has been transferred to ``install_toolchaing``.
  * ``install_toolchain``: Also installs Litex, and on a RaspberryPi also OpenOCD.
  * ``flash_firmware``: Flashes the firmware to the LED-card using OpenOCD. Designed for flashing
    with a RaspberryPi, however other adapters can aslo be selected by their name.

Version 1.0.3
=============

Bugfixes for several issues.

* ``driver``:

  * ``encoder``: calculation of average speed has been corrected (#50).

* ``cli``:

  * ``install_litex``: Solved bug which prevented installation without the option ``--user`` (#47).
  * ``install_toolchain``: Auto-detection of both architecture and os. Command line options
    are only required when installing specific versions.

Version 1.0.2
=============

On some systems the communication to the FPGA failed. This was due to the package header of the read request
being partly overwritten by another function (buffer overflow). Thanks to OJthe123 this bug has been identified
and tracked to its origins.

* ``driver``:

  * Resolved buffer-overflow error in ``litexcnc.c``.

Version 1.0.1
=============

When drafting release 1.0.0 a merge conflict occurred. This merge conflict was solved, however leading to an error
in ``module_stepgen.h``. During testing this error passed by unnoticed, because an old version of the driver
was still installed on the system. This bug-fix solves this problem.

* ``driver``:

  * Resolved error in ``module_stepgen.h``, which prevented installation of the driver.

Version 1.0.0
=============

First release!

* ``driver``:

  * Modules and boards can be extended with plugins. The available modules and boards are automatically picked up
    by the script ``litexcnc install_driver``.
  * Removed dependency on JSON-libraries. The configuration is now announced from the FPGA at initialisation
  * Main driver with supported modules: ``gpio``, ``pwm``, ``stepgen``, ``encoder``;
  * Ethernet/Etherbone driver;

* ``firmware``:

  * Modules can be extended with plugins, this requires a different approach in the configuration JSON. The configs 
    created for version 0.9 will not work in this version without modification.
  * Firmware contains configuration.
  * Supported modules: ``gpio``, ``pwm``, ``stepgen``, ``encoder``;
  * Supported cards: ``5A-75B``, ``5A-75E``

Several test releases have been made with increasing functionality and several bug-fixes. These versions have
now been superseeded by the v1.0-releases of Litex-CNC. These pre-releases differ significantly in setup and 
design philosophy from the v1.0-release as they were monolythic and difficult to expand with new modules. They
have served the purpose to remove bugs in the algorithms of the various modules.
