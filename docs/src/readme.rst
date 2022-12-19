====================
Welcome to LiteX-CNC
====================

This project aims to make a generic CNC firmware and driver for FPGA cards which are supported by LiteX.
Configuration of the board and driver is done using json-files. The supported boards are the Colorlight
boards 5A-75B and 5A-75E, as these are fully supported with the open source toolchain.

.. note::
    Although the RV901T is also supported by Litex, the firmware cannot be automatically build with
    LitexCNC, as it requires the Xilinx-software to compile the Verilog to a bit-stream. LitexCNC can
    be used to create the Verilog and the driver will work when the bit-stream is loaded on the board.
    However, there is a gap in the toolchain not covered. There are known issues with the compantibility
    of Litex with Xilinx.

The idea of this project was conceived by ColorCNC by *romanetz* on the official LinuxCNC and the difficulty
to obtain a MESA card.

.. warning::
    At this moment this code is experimental and requires expansion. A test card has been received and a 
    test setup has been created. The modules GPIO, PWM, Stepgen are tested and are working. Expansion of
    the project with encoders, I2C, RS489, etc. is still required.

Acknowledgements
================

This project would not be possible without:

* ColorCNC by *romanetz* (`link <https://forum.linuxcnc.org/27-driver-boards/44422-colorcnc?start=0>`_);
* HostMot2 (MESA card driver) as the structure of the driver has been adopted.

Installation
============

LitexCNC can be installed using pip:

.. code-block:: shell

    pip install litexcnc[cli]


After installation of LitexCNC, one can setup building environment for the firmware using the included
scripts:

.. code-block:: shell

    litexcnc install_litex
    litexcnc install_toolchain


Both Litex and the toolchain (OSS-CAD-suite) will be installed by default be installed in the ``/opt``
folder. Optionally the flag ``--user`` can be supplied to both commands, in which case the building
environment is installed in ``HOME``-directory.

Configuration of the FPGA
=========================

Structure of the JSON file
--------------------------

The structure of the JSON configuration file is given below. The configuration of the different modules
is described in their :doc:`relevant sections </modules/index>`.

.. code-block:: json

    "board_name": "test_PWM_GPIO",
    "baseclass": "litexcnc.firmware.boards.ColorLight_5A_75E_V7_1",
    "clock_frequency": 40000000,
    "ethphy": {
        "tx_delay": 0
    },
    "etherbone": {
        "ip_address": "192.168.2.50",
        "mac_address": "0x10e2d5000000"
    },
    ... (module-config)

The definitions of the entries are:

board_name
    The name of the board. This name will be used in the HAL.
base_class
    The type of FPGA board. Available types are (case-sensistive!):
    
    * ``litexcnc.firmware.boards.ColorLight_5A_75B_V6_1`` 
    * ``litexcnc.firmware.boards.ColorLight_5A_75B_V7_0`` 
    * ``litexcnc.firmware.boards.ColorLight_5A_75B_V8_0`` 
    * ``litexcnc.firmware.boards.ColorLight_5A_75E_V6_0`` 
    * ``litexcnc.firmware.boards.ColorLight_5A_75E_V7_1`` 

clock_frequency
    The clock-frequency of the board. Recommended value is 40 MHz.
ethphy
    Settings for the ethernet adapter, use default value as shown in example
etherbone
    Settings for mac-address and ip-address. Change to the needs of the project.

Some example configuration are given in the :doc:`examples sections </examples/index>`.


Building the firmware (bit-file)
--------------------------------

The firmare can be created based with the following command:

.. code-block:: shell

    litexcnc build_firmware "<path-to-your-configuration>" --build 


Compiling the driver
--------------------

.. note::
    Compilation of the driver is only required once as long the same version of LitexCNC is used. When 
    LitexCNC is updated, please re-install the driver; the version of the firmware should always be the 
    same as the version of the driver.  An error will be produced by LinuxCNC when the versions do not
    match.

.. note::
    To install the driver, ``linuxcnc-dev`` should be installed on the system. 

The firmare can be created based with the following command:

.. code-block:: shell

    litexcnc install_driver

This script will run ``apt-get`` to install the following packages:

- ``libjson-c-dev``, which is required to read the configuration files. 

After this, the driver is installed using ``halcompile``.

Usage in HAL
============
Typically main litexcnc driver is loaded first:

.. code-block::

    loadrt litexcnc

After loading the main driver, the board-driver can be loaded. At this moment only ethernet cards 
are supported using the ``litexcnc_eth`` board-driver. All the board-driver modules accept a load-time 
modparam of type string array, named ``config_file``. This array has one config_file string for each 
board the driver should use. Each json-file is passed to and parsed by the litexcnc driver when the 
board-driver registers the board. The paths can contain spaces, so it is usually a good idea to wrap 
the whole thing in double-quotes (the " character). The comma character (,) separates members of the 
config array from each other.

.. code-block:: shell

    loadrt litexcnc_eth config_file="/workspace/examples/5a-75e.json"

The driver exposes two functions to the HAL:

* ``<BoardName>.<BoardNum>.read``: This reads the encoder counters, stepgen feedbacks, and GPIO input
  pins from the FPGA.
* ``<BoardName>.<BoardNum>.write``: This updates the PWM duty cycles, stepgen rates, and GPIO outputs
  on the FPGA. Any changes to configuration pins such as stepgen timing, GPIO inversions, etc, are also
  effected by this function. 

It is strongly recommended to have structure the functions in the HAL-file as follows:

#. Read the status from the FPGA using the ``<BoardName>.<BoardNum>.read``.
#. Add all functions which process the received data.
#. Write the new information to the FPGA using the ``<BoardName>.<BoardNum>.write``.
