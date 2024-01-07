.. _RasberryPiImages: Installing LinuxCNC and LitexCNC on a RaspberryPi

Installing LinuxCNC and LitexCNC on a RaspberryPi
=================================================

This sections describes the steps required to prepare your Raspberry-Pi for running LinuxCNC with the LitexCNC. There
are several options for creating images:

- LinuxCNC 2.9.2 running on Debian Bookworm with the PREEMPT kernel;
- LinuxCNC 2.9.2 running on Raspbian Bookworm with the RTAI kernel;
- LinuxCNC 2.8.4 running on Buster (legacy).

.. note::
    This section is currently under development. Not all images are therefore described yet.

LinuxCNC 2.9.2 running on Debian Bookworm with the PREEMPT kernel
-----------------------------------------------------------------

This version is fastest to install, as the image has largely already been created by the team of LinuxCNC. You can download
the image containing LinuxCNC from their `downloads <https://linuxcnc.org/downloads/>`_ page. The version to use depends on
the type of Raspberry Pi:

- `LinuxCNC 2.9.2 Raspberry Pi 4 <https://www.linuxcnc.org/iso/rpi-4-debian-bookworm-6.1.54-rt15-arm64-ext4-2023-11-17-1731.img.xz>`_ 
  OS based on Debian Bookworm Raspberry Pi 4 Uspace compatible with Mesa Ethernet and SPI interface boards.
- `LinuxCNC 2.9.2 Raspberry Pi 5 <https://www.linuxcnc.org/iso/rpi-5-debian-bookworm-6.1.61-rt15-arm64-ext4-2023-11-17-1520.img.xz>`_ 
  OS based on Debian Bookworm Raspberry Pi 5 Uspace compatible with Mesa Ethernet and SPI interface boards.

These .xz files are directly readable by the `Raspberry Pi imager <https://www.raspberrypi.com/software/>`_ application, although
the settings from the Raspberry Pi imager, such as WiFi and enabling SSH are not set while flashing. Headless install is therefore
not possible and a keyboard and monitor are required for configuration.

For both of these images:

- User name: cnc
- Password: cnc

Please run the following from the commandline to configure WiFi, timezones and other data:

.. code-block:: bash
    
    sudo menu-config

After configuring the RaspberryPi, LinuxCNC can be installed with the following commands:

.. code-block:: bash
    
    sudo apt-get update && sudo apt-get -y upgrade
    sudo apt-get install python3-pip python3-wheel python3-venv
    python3 -m venv ~/.local --system-site-packages
    ~/.local/bin/pip install litexcnc
    ~/.local/bin/litexcnc install_toolchain --user -a arm64
    sudo env "PATH=$PATH" ~/.local/bin/litexcnc install_driver

After installation of LitexCNC, its driver and the toolchain a reboot is required.
