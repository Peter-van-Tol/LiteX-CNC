.. _examples:

========
Examples
========

For the supported cards ``5A-75B`` and ``5A-75E`` some examples can be downloaded using the 
links below. 

.. warning::
    The examples section is work-in-progress. The links are not working at this moment.

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

   "V6.1", "192.168.2.50", "12 / 14", "6", "6 (step/dir)", "6 (A/B)", "U28, U24, U23", :download:`indexed <./indexed/5a-75b_v6.1_i12o14p6s6e6.json>` / :download:`named <./named/5a-75b_v6.1_i12o14p6s6e6.json>`
   "V6.1", "192.168.2.50", "24 / 28", "6", "\-", "\-", "U28, U24, U23", :download:`indexed <./indexed/5a-75b_v6.1_i24o32.json>` / :download:`named <./named/5a-75b_v6.1_i24o32.json>`
   "V7.0", "192.168.2.50", "12 / 14", "6", "6 (step/dir)", "6 (A/B)", "U28, U24, U23", :download:`indexed <./indexed/5a-75b_v7.0_i12o14p6s6e6.json>` / :download:`named <./named/5a-75b_v7.0_i12o14p6s6e6.json>`
   "V7.0", "192.168.2.50", "24 / 28", "6", "\-", "\-", "U28, U24, U23", :download:`indexed <./indexed/5a-75b_v7.0_i24o32.json>` / :download:`named <./named/5a-75b_v7.0_i24o32.json>`
   "V8.0", "192.168.2.50", "12 / 14", "6", "6 (step/dir)", "6 (A/B)", "U28, U24, U23", :download:`indexed <./indexed/5a-75b_v8.0_i12o14p6s6e6.json>` / :download:`named <./named/5a-75b_v8.0_i12o14p6s6e6.json>`
   "V8.0", "192.168.2.50", "24 / 28", "6", "\-", "\-", "U28, U24, U23", :download:`indexed <./indexed/5a-75b_v8.0_i24o32.json>` / :download:`named <./named/5a-75b_v8.0_i24o32.json>`

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
   
   "V6.0", "192.168.2.50", "30 / 29", "9", "9 (step/dir)", "9 (A/B)", "TBD", `index <test.html>`_
   "V6.0", "192.168.2.50", "30 / 32", "6", "6 (step/dir diff.)", "6 (A/B/Z)", "TBD", `index <test.html>`_
   "V7.1", "192.168.2.50", "30 / 29", "9", "9 (step/dir)", "9 (A/B)", "TBD", `index <test.html>`_
   "V7.1", "192.168.2.50", "30 / 32", "6", "6 (step/dir diff.)", "6 (A/B/Z)", "TBD", `index <test.html>`_
