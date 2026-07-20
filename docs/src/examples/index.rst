.. _examples:

========
Examples
========

For the supported cards ``5A-75B`` and ``5A-75E`` some examples can be downloaded using the 
links below. 

5A-75B
======

The ``5A-75B`` has 8 HUB75-connectors, with each 6 individual pins (48 total). Besides these pins, 
there are 8 shared pins. This makes the total number of usable pins 56. The starting point for these
examples are:

* Functions are grouped.
* The number of pins is chosen to reflect the number of individual pins (6). This means that a single
  connector has a single function.
* The number of pins for input and output is aligned with the buffers. The buffers which needs to be
  modified are noted in the table.
* The shared pins are by definition configured as GPIO - out.
* Both indexed version as named versions of the configuration are available. In the named version the
  names of the pins for ``GPIO`` and ``PWM`` are equal to the physical pin number on the board. I.e.
  name ``j1:1`` points to pin number one on connector one. This naming deviates from the pin names used
  by Litex, which is 0-based.

.. csv-table::
   :header: "Board version", "IP-address", "GPIO (in/out)", "PWM", "Stepgen (type)", "Encoders (type)", "Change buffers", "Link"
   :widths: auto

   "V6.1", "10.0.0.10", "12 / 12", "6", "6 (step/dir)", "4 (A/B/Z)", "U28, U24, U23", :download:`JSON <./json/5a-75b_v6.1_i12o12p6s6e4.json>`
   "V6.1", "10.0.0.10", "12 / 12", "6", "6 (step/dir)", "6 (A/B)", "U28, U24, U23", :download:`JSON <./json/5a-75b_v6.1_i12o12p6s6e6.json>`
   "V6.1", "10.0.0.10", "24 / 30", "\-", "\-", "\-", "U28, U24, U23", :download:`JSON <./json/5a-75b_v6.1_i24o30.json>`
   "V7.0", "10.0.0.10", "12 / 12", "6", "6 (step/dir)", "4 (A/B/Z)", "U28, U24, U23", :download:`JSON <./json/5a-75b_v7.0_i12o12p6s6e4.json>`
   "V7.0", "10.0.0.10", "12 / 12", "6", "6 (step/dir)", "6 (A/B)", "U28, U24, U23", :download:`JSON <./json/5a-75b_v7.0_i12o12p6s6e6.json>`
   "V7.0", "10.0.0.10", "24 / 30", "\-", "\-", "\-", "U28, U24, U23", :download:`JSON <./json/5a-75b_v7.0_i24o30.json>`
   "V8.0", "10.0.0.10", "12 / 12", "6", "6 (step/dir)", "4 (A/B/Z)", "U28, U24, U23", :download:`JSON <./json/5a-75b_v8.0_i12o12p6s6e4.json>`
   "V8.0", "10.0.0.10", "12 / 12", "6", "6 (step/dir)", "6 (A/B)", "U28, U24, U23", :download:`JSON <./json/5a-75b_v8.0_i12o12p6s6e6.json>`
   "V8.0", "10.0.0.10", "24 / 30", "\-", "\-", "\-", "U28, U24, U23", :download:`JSON <./json/5a-75b_v8.0_i24o30.json>`

5A-75E
======

The ``5A-75E`` has 16 HUB75-connectors, with each 6 individual pins (96 total). Besides these pins, 
there are 8 shared pins. This makes the total number of usable pins 104. The starting point for these
examples are:

* Functions are grouped;
* The number of pins is chosen to reflect the number of individual pins (6). This means that a single
  connector has a single function;
* The number of pins for input and output is aligned with the buffers. The buffers which needs to be
  modified are noted in the table;
* The shared pins are by definition configured as GPIO - out.

.. note::
    For ``5A-75E`` there also exists a version 8.0. For this version the board layout 

.. csv-table::
   :header: "Board version", "IP-address", "GPIO (in/out)", "PWM", "Stepgen (type)", "Encoders (type)", "Change buffers", "Link"
   :widths: auto
   
   "V6.0", "10.0.0.10", "28 / 28", "6", "6 (step/dir diff.)", "6 (A/B/Z)", "TBD", :download:`JSON <./json/5a-75e_v6.0_i28o28p6s6e6.json>`
   "V6.0", "10.0.0.10", "48 / 56", "\-", "\-", "\-", "TBD", :download:`JSON <./json/5a-75e_v6.0_i48o56.json>`
   "V7.1", "10.0.0.10", "28 / 28", "6", "6 (step/dir diff.)", "6 (A/B/Z)", "TBD", :download:`JSON <./json/5a-75e_v7.1_i28o28p6s6e6.json>`
   "V7.1", "10.0.0.10", "48 / 56", "\-", "\-", "\-", "TBD", :download:`JSON <./json/5a-75e_v7.1_i48o56.json>`
   "V8.2", "10.0.0.10", "0 / 100", "4", "\-", "\-", "\-", :download:`JSON <./json/5a-75e_v8.2_o100p4.json>`
   "V8.2", "10.0.0.10", "0 / 104", "\-", "\-", "\-", "\-", :download:`JSON <./json/5a-75e_v8.2_o104.json>`
