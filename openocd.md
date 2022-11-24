Just some notes for writing to the FPGA with OpenOCD.
```
# GPIO RPi -> Connected to JTAG port -> Function on JTAG port
# 18 -> J31 -> TMS
# 23 -> J27 -> TCK
# 24 -> J30 -> TDO
# 25 -> J32 -> TDI

# Results in adding this to the interface/raspberrypi-native.cfg
# Saved it as interface/raspberrypi-native-mod.cfg
tck tms tdi tdo
23  18  25  24
```

For programming the FPGA, use the following command
``` shell
openocd \
    -f interface/raspberrypi-native-mod.cfg \
    -c "transport select jtag" \
    -f fpga/lattice_ecp5.cfg \
    -c "init; svf quiet progress colorlight_5a_75e.svf; exit"
```

For programming the RV901T, use the following command
``` shell
openocd \
    -f interface/raspberrypi-native-mod.cfg \
    -c "transport select jtag" \
    -f cpld/xilinx-xc6s.cfg \
    -c "init; xc6s_program xc6s.tap; pld load 0 rv901t.bit; exit"
```
