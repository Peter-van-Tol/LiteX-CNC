================
SPI using spidev
================

The Rapsberry Pi has a SPI interface on its GPIO header. With the ``spidev`` driver the
FPGA can be used by using only 4 wires (``MOSI``, ``MISO``, ``CLCK``, and ``CS``). This
driver is designed to work with a variety of boards, as long as they expose the ``spidev``
device.

.. image:: images/Raspberry-Pi-GPIO-Header-with-Photo.png
   :width: 600
   :alt: HUB-75 sinking input PCB - front

.. info::
    Specifically for the Raspberry Pi there is also the driver ``pigpio`` is available. This
    driver offers a better performance. It is faster and has less jitter, leading to a servo
    period which can be a factor 2 smaller (or even more). The configuration of the FPGA is
    independent of which SPI driver is used.

.. info::
    SPI uses 4 separate connections to communicate with the target device. These connections
    are the serial clock (``CLK``), Master Input Slave Output (``MISO``), Master Output Slave
    Input (``MOSI``) and Chip Select (``CS``):

    - The clock pin sends pulses at a regular frequency, the speed at which the Raspberry Pi
      and SPI device agree to transfer data to each other. For the ADC, clock pulses are sampled
      on their rising edge, on the transition from low to high.
    - The MISO pin is a data pin used for the master (in this case the Raspberry Pi) to receive
      data from the FPGA. Data is read from the bus after every clock pulse.
    - The MOSI pin sends data from the Raspberry Pi to the ADC. The FPGA will take the value of
      the bus on the rising edge of the clock. This means the value must be set before the clock
      is pulsed.
    - Finally, the Chip Select line chooses which particular SPI device is in use. If there are
      multiple SPI devices, they can all share the same CLK, MOSI, and MISO. However, only the
      selected device has the Chip Select line set low, while all other devices have their CS
      lines set high. A high Chip Select line tells the SPI device to ignore all of the commands
      and traffic on the rest of the bus.

Configuration
=============

The configuration of the FPGA consist of the definition of the pins where ``MOSI``, ``MISO``,
``CLCK``, and ``CS`` are connected to. Below is the example used for the HUB75HAT.

.. code-block:: json

    "connection": {
        "connection_type": "spi",
            "mosi": "j5:14",
            "miso": "j5:12",
            "clk":  "j5:15",
            "cs_n": "j5:11"
    }

.. info::
    The FPGA is defined as slave. This means that the pins ``MOSI``, ``MISO``, and ``CS``
    are inputs. This might require modification of the buffers on the card. 

HAL
===

Use the following connection string for spidev:

.. code-block::

    loadrt litexcnc connections="spidev:\dev\spidev0.0"

This will load spidev interface 0 with chip select 0. The available ``spidev`` devices 
can be listed with the code below.

.. code-block:: shell

    ls /dev/spidev*.*
