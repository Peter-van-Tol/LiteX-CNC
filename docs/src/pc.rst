.. _InstallPC: Installing LinuxCNC and LitexCNC on a PC

Installing LinuxCNC and LitexCNC on a PC (x64)
==============================================

This sections describes the steps required to prepare your PC for running LinuxCNC. LinuxCNC is easy
to install, as the image has largely already been created by the team of LinuxCNC. You can download
the image containing LinuxCNC from their `downloads <https://linuxcnc.org/downloads/>`_ page.

From the downloads page, download the `LinuxCNC 2.9.2 Debian 12 Bookworm PREEMPT-RT ISO <https://www.linuxcnc.org/iso/LinuxCNC_2.9.2-amd64.hybrid.iso>`_.
The Debian 12 Bookworm ISO will install a full Debian system with the required realtime kernel and
the linuxcnc-uspace application. It uses a PREEMPT-RT patched kernel which is close to mainstream
Linux but does not, in some cases, give quite such good realtime performance as the previous RTAI
kernel. It is very often more than good enough. It should probably be the first version tried. This
is compatible with the LitexCNC boards.

Installing LitexCNC
===================

After installing LinuxCNC, LitexCNC can be installed with the following commands:

.. code-block:: bash
    
    sudo apt-get update && sudo apt-get -y upgrade
    sudo apt-get install python3-pip python3-wheel python3-venv
    python3 -m venv ~/.local --system-site-packages
    echo -e "" >> ~/.bashrc
    echo -e "# set PATH so it includes user's private bin if it exists" >> ~/.bashrc
    echo -e "if [ -d \"$HOME/.local/bin\" ] ; then" >> ~/.bashrc
    echo -e "    PATH=\"$HOME/.local/bin:$PATH\"" >> ~/.bashrc
    echo -e "fi" >> ~/.bashrc
    . ~/.bashrc
    pip install litexcnc
    litexcnc install_toolchain --user -a x64
    sudo env "PATH=$PATH" litexcnc install_driver
    . ~/.bashrc

Building your first firmware
============================

You can test the installation by building one of the `example configurations <https://litex-cnc.readthedocs.io/en/latest/examples/index.html>`_.
In the example below, it is assumed that the downloaded firmware is ``5a-75b_v8.0_i24o32.json``. Please
select the configuration which is compatible with your LED card.

The firmware can be built using the following command:

.. code-block:: bash
    
    litexcnc build_firmware 5a-75b_v8.0_i24o32.json --build

Flashing the firmware
=====================

In order to flash the firmware, you need a programmer. A programmer is a device which translates
the USB communication to TTL levels. There are many different models available, ranging from very
cheap (FT232R) to very expensive professional programmers (Altera USB Blaster). For this example
I recommend a programmer with the FT232R chipset, because this device is compatible ``litexcnc``
and cheap.

TODO: add image of pin header

The connection between the programmer and the JTAG-header can be done using jumper wires. For this
purpose a pin header should be soldered on the LED-card. The table below gives the required connections.

.. list-table:: Connections between FT232R and 5A-75B/5A-75E
   :widths: 33 33 33
   :header-rows: 1

   * - Programmer pin (FT232R)
     - Function
     - JTAG Header pin (5A-75B/5A-75E)
   * - RXD
     - TDI
     - J32
   * - TXD
     - TCK
     - J27
   * - RTS
     - TDO
     - J30
   * - CTS
     - TMS
     - J31

Besides the pins the ground (GND) of the programmer and the LED card should be also connected.

.. warning::
    Some JTAG programmers support both 3.3V and 5.0V by selecting the voltage with a jumper. Ensure
    you will always use 3.3 V outputs. A VOLTAGE OF 5.0V WILL DESTROY YOUR FPGA!.

TODO: add image of finished wiring.

After making the connections, the LED card can be flashed with the command:

.. code-block:: bash
    
    cd 5a-75b_v8.0_i24o32/gateware
    litexcnc flash_firmware --programmer ft232r colorlight.svf

This will flash the LED card with a temporary program. When the power is cycled, the FPGA will
start with the previous loaded program. To make the firmware permanent, one can modify the
command:

.. code-block:: bash
    
    litexcnc flash_firmware --programmer ft232r --permanent colorlight.svf

Configuring the network
=======================

...
