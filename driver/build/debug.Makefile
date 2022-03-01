obj-m += litexcnc_debug.o
include /opt/linuxcnc/src/Makefile.modinc
EXTRA_CFLAGS += -I/workspace/driver
LDFLAGS += -ljson-c
VPATH=../