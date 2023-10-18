====================
Welcome to LiteX-CNC
====================

This project aims to make a generic CNC firmware and driver for FPGA cards which are supported by LiteX.
Configuration of the board and driver is done using json-files. The supported boards are the Colorlight
boards 5A-75B and 5A-75E, as these are fully supported with the open source toolchain.

.. info::
    At this moment it is **strongly** recommended to use the ``11-add-external-extensions-to-linuxcnc``
    version of the software. This version has essential upgrades in both safety and usability. See the
    `documentation <https://litex-cnc.readthedocs.io/en/11-add-external-extensions-to-litexcnc/>`_ of
    this version on how to obtain and use it.

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

    pip install litexcnc[cli]

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
    litexcnc build_firmware
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

Installing Litex and toolchain
------------------------------

Both Litex and the toolchain (OSS-CAD-suite) will be installed by default be installed in the ``/opt``
folder. Optionally the flag ``--user`` can be supplied to both ``install_litex`` and ``install_toolchain``, in
which case the building environment is installed in ``HOME``-directory.

Litex can be installed using:

.. code-block:: shell

    litexcnc install_litex

The toolchain can be installed using:

.. code-block:: shell

    litexcnc install_toolchain

The command ``install_toolchain`` automatically detects which operating system (Darwin, Linux, or Windows)
and architecture (arm, arm64 or x64) is used. The version is shown in the terminal while downloading the
software. In case the detection is erronous, the correct OS and architecture can be chosen by using the
``--os`` and ``-architecture`` options of the command.

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

After building the firmware, all files will reside in the ``.\<FGPA_NAME\gateware`` directory. The ``.svf`` 
in this directory can be flashed to your FPGA using a program such as OpenOCD (part of the OSS-CAD-suite
which is by default installed as part of the toolchain). An example of such a command is:

.. code-block:: shell

    openocd \
        -f interface/raspberrypi-native-mod.cfg \
        -c "transport select jtag" \
        -f fpga/lattice_ecp5.cfg \
        -c "init; svf quiet progress colorlight_5a_75e.svf; exit"

.. info::
    You can use the GPIO of your Raspberry Pi to flash the FPGA. The version of OpenOCD included with the
    toolchain however, does not support teh GPIO. A good guide on how to install OpenOCD with support for
    GPIO on your Rasberry Pi can be found `here <https://catleytech.com/?p=2679>`_.

By default, the ``.svf``-file is not retained in th flash of the FPGA. When the card is power-cycled, the
previous program will run again. This makes it possible to test new version and features before making them
permanent. To make the program reside in the flash of the FPGA, the bit-file has to be converted with the
``convert_bit_to_flash`` tool (NOTE: this command requires the ``.bit``-file, not the previously used 
``.svf``-file):

.. code-block:: shell

    litexcnc convert_bit_to_flash colorlight_5a_75e.bit colorlight_5a_75e.flash

The created ``.flash`` file can now be flashed to the FPGA using the same method as used before.

Usage in HAL
============
Typically main litexcnc driver is loaded first:

.. code-block::

    loadrt litexcnc

After loading the main driver, the board-driver can be loaded. At this moment only ethernet cards 
are supported using the ``litexcnc_eth`` board-driver. All the board-driver modules accept a load-time 
modparam of type string array, named ``connections``. This array has one ip-addreess string for each 
board the driver should use. The default port the driver will connect to is ``1234``. When another port
should be used, the port can be supplied in the ``connections``, i.e. ``eth:10.0.0.10:456``.

.. code-block:: shell

    loadrt litexcnc_eth connections="eth:10.0.0.10"

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
