========
Ethernet
========

Ethernet has been the first and most straight forward method to communicate with the
FPGA, using the Gigabit Ethernet connection on the board. The host running LinuxCNC
must also be equipped with Gigabit Ethernet for this setup to work correctly. This means
a Raspberry Pi 3 will unfortunately not work.

Configuration
=============

Below are examples of the ethernet configuration.

.. tabs::

    .. code-tab:: json
        :caption: Minimal
        
        "connection": {
            "connection_type": "etherbone",
            "ip_address": "10.0.0.10",
            "mac_address": "0x10e2d5000000"
        }

    .. code-tab:: json
        :caption: All options
        
        "connection": {
            "connection_type": "etherbone",
            "ip_address": "10.0.0.10",
            "mac_address": "0x10e2d5000000",
            "rx_delay": 0,
            "tx_delay": 0,
            "with_hw_init_reset": false
        }

The configuration of the IP and MAC address are in most cases enough. For description
of the more advanced connections options ``rx_delay``, ``tx_delay``, and ``with_hw_init_reset``,
please refer to the documentation of `LiteEth <https://github.com/enjoy-digital/liteeth>`_.

HAL
===

Use the following connection string for ethernet:

.. tabs::

    .. code-tab::
        :caption: Default
        
        loadrt litexcnc connections="eth:<IP-address>"

    .. code-tab:: 
        :caption: Custom port
        
        loadrt litexcnc connections="eth:<IP-address>:<port>"


The ``<IP-address>`` should be replaced with the configured IP-address of the card. The ``<port>``
can normally omitted, in which case port 1234 is being used. When a custom port number is used
one has to define this field as well.
