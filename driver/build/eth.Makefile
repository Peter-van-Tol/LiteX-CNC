obj-m += litexcnc_eth.o
include /opt/linuxcnc/src/Makefile.modinc
EXTRA_CFLAGS += -I/workspace/driver
LDFLAGS += -ljson-c
VPATH=../