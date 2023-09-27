=========
Changelog
=========

All versions in this changelog have two entries: ``driver`` and ``firmware``. The firmware and driver should
have the same version, as communication protocol might change between versions. In the firmware/driver there
is a safeguard to prevent miscommunication.

Under development
=================

These are the features which are currently under development, but not yet released.

* ``driver``:

  * ``encoder``: calculation of average speed has been correctd (#50).


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
