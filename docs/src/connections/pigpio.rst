================
SPI using pigpio
================

The Rapsberry Pi has a SPI interface on its GPIO header. With the ``spidev`` driver the
FPGA can be used by using only 4 wires (``MOSI``, ``MISO``, ``CLK``, and ``CS``). This
driver is designed specifically for the Raspberry Pi for boosting speed and reducing
jitter. It relies on the library ``pigpio``, which is dynamically loaded when the module
is laoded by Litex-CNC.

.. image:: images/Raspberry-Pi-GPIO-Header-with-Photo.png
   :width: 600
   :alt: HUB-75 sinking input PCB - front

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

Use the following connection string for ``pigpio``:

.. code-block::

    loadrt litexcnc connections="pigpio:<CS_channel>:<speed>"

Only the SPI 0 is supported. The second SPI device on the Rapsberry Pi is currently not supported
by the driver. The ``CS_channel`` must be either ``0`` or ``1``, because SPI 0 has only two channels.

To determine the speed of the SPI communication, one can use the component
``litexcnc_pigpio_speed_test``. This component will increase the speed in 500 kHz steps until
identification of the Litex-CNC firmware is not correctly received any longer. For the speed
test one must use ``halcmd``:

.. code-block::

    loadrt litexcnc_pigpio_speed_test

Will, for example, give the following output:

.. code-block::

    sps=20977.7: 11 bytes @ 1000000 bps (loops=10000, average time=47.670 us, maximum time=703.812 us)
    sps=31240.3: 11 bytes @ 1500000 bps (loops=10000, average time=32.010 us, maximum time=735.044 us)
    sps=40977.2: 11 bytes @ 2000000 bps (loops=10000, average time=24.404 us, maximum time=582.933 us)
    sps=50786.4: 11 bytes @ 2500000 bps (loops=10000, average time=19.690 us, maximum time=102.997 us)
    sps=60441.5: 11 bytes @ 3000000 bps (loops=10000, average time=16.545 us, maximum time=123.978 us)
    sps=69732.0: 11 bytes @ 3500000 bps (loops=10000, average time=14.341 us, maximum time=101.089 us)
    sps=77709.7: 11 bytes @ 4000000 bps (loops=10000, average time=12.868 us, maximum time=92.030 us)
    sps=87851.0: 11 bytes @ 4500000 bps (loops=10000, average time=11.383 us, maximum time=50.068 us)
    Failed transmission at 5000000 Hz

In the case above, a speed of 4,000,000 is recommended.

.. info::
    Because ``litexcnc_pigpio_speed_test`` is doing the test when the module is loaded, the
    loading can take too much time for ``halcmd``, which will give the message ``Waiting for component 
    'litexcnc_pigpio_speed_test' to become ready.....Waited 3 seconds for master.  giving up.``. This 
    behavior is expected, and the message can be safely ignored.

.. warning::
    At this moment there is a bug in ``litexcnc_pigpio_speed_test``, causing it to hang as soon as the
    test is finished. You can close ``halcmd`` by pressing ``CTRL+Z``. This however will prevent closing
    the connection to the GPIO as it should. Restart of the Raspberry Pi is required to close these
    resources. Not restarting would lead to an increase in jitter; running the test twice without restart
    shows constant average time, but a severe increases the maximum tim of the communication.

