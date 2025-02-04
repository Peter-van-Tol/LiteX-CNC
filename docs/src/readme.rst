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

LitexCNC can be installed using pip:

.. code-block:: shell

    pip install litexcnc

.. note::
    LitexCNC requires Python 3.7 or above. The `pip`-command might refer to the Python 2 installation on
    the system, which will lead to errors in the installation process. In this case, use `pip3` in the
    command above and `python3` in all commands in the examples.

.. info::
    More information on the usage of LitexCNC on a Raspberry Pi can be found in the :ref:`RasberryPiImages`.

Installed scripts
-----------------

After installation of LitexCNC, one can setup building environment for the firmware and install the
drivers the included scripts:

.. code-block:: shell

    litexcnc install_driver
    litexcnc install_toolchain
    litexcnc build_firmware
    litexcnc flash_firmware
    litexcnc convert_bit_to_flash

.. note::
    In case the scripts ``litexcnc <command>`` cannot be found, the cause can be that the scripts are
    not on the system path. In this case the commands should be called with ``python -m litexcnc <command>``. 
    It might be necessary to replace ``python`` with the name of the python executable in which 
    litexcnc is installed (for example ``python3``)

.. note::
    Using the ``--help`` argument on each of the commands will show additional information on the 
    arguments and options of eacht command.  

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

Installing toolchain
--------------------

The toolchain can be installed using:

.. code-block:: shell

    litexcnc install_toolchain

.. note:: 
    The commmand ``install_litex`` has been deprecated. The command ``install_toolchain``
    includes Litex, OSS-CAD-suite, and OpenOCD (RaspberryPi only).

Options for the command are:

--user
    Installs Litethe toolchain for current user only. By default, the toolchain is installed
    in `\opt` (regular install) or `~` (when using --user option).
--directory
    Defines a specific directory to install the toolchain in. By default, the toolchain is
    installed in `\opt` (regular install) or `~` (when using --user option). This option can
    be used when you don't have rights to write in `\opt`.

.. info::
    The command ``install_toolchain`` automatically detects which operating system (Darwin, Linux, or Windows)
    and architecture (arm, arm64 or x64) is used. The version is shown in the terminal while downloading the
    software. In case the detection is erronous, the correct OS and architecture can be chosen by using the
    ``--os`` and ``-architecture`` options of the command.

On the RaspberryPi this command also installs OpenOCD, with the options for programming the FPGA
using the GPIO pins. Installing OpenOCD requires privileges, you might be prompted for a password
when this is required for ``sudo``.

Configuration of the FPGA
=========================

Structure of the JSON file
--------------------------

The structure of the JSON configuration file is given below. The configuration of the different modules
is described in their :doc:`relevant sections </modules/index>`.

.. code-block:: json

    "board_name": "test_PWM_GPIO",
    "board_type": "5A-75E v6.0",
    "clock_frequency": 40000000,
    "connection": {
        "connection_type": "etherbone",
        "tx_delay": 0,
        "ip_address": "10.0.0.10",
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

connection
    Settings for the connection adapter. At this moment ``etherbone`` and ``SPI`` are 
    supported. See the :doc:`connections sections </connections/index>` for more information.

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

Flashing the firmware
---------------------
After building the firmware, all files will reside in the ``.\<FGPA_NAME>\gateware`` directory. For
flashing the firmware, one can use the built-in command:

.. code-block:: shell
    litexcnc flash_firmware [OPTIONS] SVF-FILE

Options for the command are:

--permanent
    With this option the firmware will be written to flash and thus be persistent. By default, the 
    .svf-file is not retained in th flash of the FPGA. When the card is power-cycled, the previous
    program will run again. This makes it possible to test new version and features before making
    them permanent. With this option the .svf-file (more correctly, the .bit-file which resides
    in the same folder) is converted so it is programmed to flash memory.
--programmer
    By default the program uses the RaspberryPi GPIO as a programmer (see pin-out below). With this
    option another programmer can be selected. See for supported adapters the `OpenOCD documentation <https://openocd.org/doc/html/Debug-Adapter-Configuration.html#Debug-Adapter-Configuration>`_.

The default pinout of the JTAG header on the RaspberryPi using this command is:

+----------+------------+----------+----------+
| GPIO num | Header pin | Function | LED-card |
+==========+============+==========+==========+
| 16       | 36         | TCK      | J27      |
+----------+------------+----------+----------+
| 6        | 31         | TMS      | J31      |
+----------+------------+----------+----------+
| 19       | 35         | TDI      | J32      |
+----------+------------+----------+----------+
| 26       | 37         | TDO      | J30      |
+----------+------------+----------+----------+

.. info::
    There are multiple layouts used for programming with the RaspberryPi. This command uses a
    custom layout of the pins, as it is designed to be used with the `HUB75HAT <https://github.com/Peter-van-Tol/LITEXCNC-HUB75HAT>`_. 
    The layout has been designed to minimize conflicts with secondary functions of the pins,
    such as UART5, which can be used to communicate with a VFD or other device over RS489.


Usage in HAL
============
The litexcnc driver is loaded with:

.. code-block:: shell

    loadrt litexcnc_eth connections="<connection_string>"

The placeholder ``<connection_string>`` should be replaced with the `connection <./connection/index>`_
for your machine. Supported connections at this moment are Ethernet and SPI.

.. info::

    In pre-releases it was possible to use ``litexcnc_eth`` directly as a component. With the release
    of v1.0 of LitexCNC the support for this has been dropped in favour of resetting the FPGA to a
    known safe state before LinuxCNC is stopped. In case ``litexcnc_eth`` is still used directly, an
    error will be thrown, indicating the required changes.

The driver exposes two functions to the HAL:

* ``<BoardName>.<BoardNum>.read``: This reads the encoder counters, stepgen feedbacks, and GPIO input
  pins from the FPGA.
* ``<BoardName>.<BoardNum>.write``: This updates the PWM duty cycles, stepgen rates, and GPIO outputs
  on the FPGA. Any changes to configuration pins such as stepgen timing, GPIO inversions, etc, are also
  effected by this function. 

It is **strongly** recommended to have structure the functions in the HAL-file as follows:

#. Read the status from the FPGA using the ``<BoardName>.<BoardNum>.read``.
#. Add all functions which process the received data.
#. Write the new information to the FPGA using the ``<BoardName>.<BoardNum>.write``.
