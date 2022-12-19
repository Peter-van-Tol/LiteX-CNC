=========
Changelog
=========

All versions in this changelog have two entries: ``dirver`` and ``firmware``. The firmware and driver should
have the same version, as communication protocol might change between versions. In the firmware/driver there
is a safeguard to prevent miscommunication.

Version 0.9.0-alpha1
====================

This is a test version for release to PyPi. 

* ``driver``:

  * Main driver with supported modules: ``GPIO``, ``PWM``, ``StepGen``
  * Ethernet/Etherbone driver

* ``firmware``:

  * Supported modules: ``GPIO``, ``PWM``, ``StepGen``
  * Supported cards: ``5A-75B``, ``5A-75E``
