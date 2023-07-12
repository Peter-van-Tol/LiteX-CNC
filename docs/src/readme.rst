====================
Welcome to LiteX-CNC
====================

This project aims to make a generic CNC firmware and driver for FPGA cards which are supported by LiteX.
Configuration of the board and driver is done using json-files. The supported boards are the Colorlight
boards 5A-75B and 5A-75E, as these are fully supported with the open source toolchain.

The idea of this project was conceived by ColorCNC by *romanetz* on the official LinuxCNC and the difficulty
to obtain a MESA card.

Acknowledgements
================

This project would not be possible without:

* ColorCNC by *romanetz* (`link <https://forum.linuxcnc.org/27-driver-boards/44422-colorcnc?start=0>`_);
* HostMot2 (MESA card driver) as the structure of the driver has been adopted.

Installation
============

LitexCNC can be installed using pip (the pre-release is only available on https:/test.pypi.org):

.. code-block:: shell

    pip install --extra-index-url https://test.pypi.org/simple/ litexcnc[cli]

.. note::
    The suffix [cli] is required to install the command-line interface. Without this suffix the scripts
    referenced in this documentation will not work.

.. note::
    LitexCNC requires Python 3.7 or above. The `pip`-command might refer to the Python 2 installation on
    the system, which will lead to errors in the installation process. In this case, use `pip3` in the
    command above and `python3` in all commands in the examples.

After installation of LitexCNC, one can setup building environment for the firmware and install the
drivers the included scripts:

.. code-block:: shell

    litexcnc install_driver
    litexcnc install_litex 
    litexcnc install_toolchain

.. note::
    In case the scripts ``litexcnc <command>`` cannot be found, the cause can be that the scripts are
    not on the system path. In this case the commands should be called with ``python -m litexcnc <command>``. 
    It might be necessary to replace ``python`` with the name of the python executable in which 
    litexcnc is installed (for example ``python3``)

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
board_type
    The type of FPGA board. Available types are (case-sensistive!):
    
    * ``5A-75B v6.1``
    * ``5A-75B v7.0``
    * ``5A-75B v8.0``
    * ``5A-75E v6.0``
    * ``5A-75E v7.1``
    * ``RV901T`` 

clock_frequency
    The clock-frequency of the board. Recommended value is 40 MHz.
ethphy
    Settings for the ethernet adapter, use default value as shown in example
etherbone
    Settings for mac-address and ip-address. Change to the needs of the project.

Some example configuration are given in the :doc:`examples sections </examples/index>`.

.. note::
    Although the RV901T is also supported by Litex, the firmware cannot be automatically build with
    LitexCNC, as it requires the Xilinx-software to compile the Verilog to a bit-stream. LitexCNC can
    be used to create the Verilog and the driver will work when the bit-stream is loaded on the board.
    However, there is a gap in the toolchain not covered. There are known issues with the compantibility
    of Litex with Xilinx.

Building the firmware (bit-file)
--------------------------------

The firmare can be created based with the following command:

.. code-block:: shell

    litexcnc build_firmware "<path-to-your-configuration>" --build 

Type ``litexcnc build_firmware --help`` for more options. 

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

.. info::
    When ``sudo`` is required to install the driver, it might be required to pass the environment variables
    to the command:

    .. code-block:: shell

        sudo -E env PATH=$PATH litexcnc install_driver

Usage in HAL
============
Typically main litexcnc driver is loaded first:

.. code-block::

    loadrt litexcnc

After loading the main driver, the board-driver can be loaded. At this moment only ethernet cards 
are supported using the ``litexcnc_eth`` board-driver. All the board-driver modules accept a load-time 
modparam of type string array, named ``connection_string``. This array has one ip-addreess string for each 
board the driver should use. The default port the driver will connect to is ``1234``. When another port
should be used, the port can be supplied in the ``connection_string``, i.e. ``eth:10.0.0.10:456``.

.. code-block:: shell

    loadrt litexcnc_eth connection_string="eth:10.0.0.10"

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
