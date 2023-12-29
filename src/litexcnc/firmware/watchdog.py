#
# This file is part of LiteX-CNC.
#
# Copyright (c) 2022 Peter van Tol <petertgvantolATgmailDOTcom>
# SPDX-License-Identifier: MIT

from litex.soc.interconnect.csr import *

# Imports for Migen
from migen import *
from migen.genlib.cdc import MultiReg
from litex.build.generic_platform import Pins, IOStandard
from litex.soc.interconnect.csr import AutoCSR, CSRStorage

class WatchDogModule(Module, AutoCSR):
    """
    Pet this, otherwise it will bite.
    """
    def __init__(self, enable_out, timeout=Signal(31), clock_domain="sys", default_timeout = 0, with_csr=True):
        # Parameters for the watchdog:
        # - the bit enable sets whether the watchdog is enabled. This is written when the 
        #   timeout is read for the first-time from the driver to FPGA
        # - timeout: counter which is decreased at each clock-cycle. When it is at 0, the 
        #   watchdog will get very angry and bite.
        self.enable  = Signal()
        self.timeout = timeout
        self.has_bitten = Signal()
        
        if enable_out is None:
            enable_out = Signal()

        # Procedure of the watchdog
        sync = getattr(self.sync, clock_domain)
        sync += [
            If(self.enable,
                If(self.timeout > 0,
                    # Still some time left before freaking out
                    self.timeout.eq(self.timeout-1),
                    self.has_bitten.eq(0),
                    enable_out.eq(1)
                ).Else(
                    # The dog got angry, and will bite
                    self.has_bitten.eq(1),
                    enable_out.eq(0)
                )
            ).Else(
                # Sleeping dogs don't bite, but the board is also not active
                self.has_bitten.eq(0),
                enable_out.eq(0)
            )
        ]

        if with_csr:
            self.add_csr(clock_domain)

    def add_csr(self, clock_domain):
        self._enable = CSRStorage(
            description="Watchdog Enable.\n Write ``1`` to enable Watchdog.",
            reset = self.enable.reset
        )
        self._timeout  = CSRStorage(
            31, 
            reset_less=True,
            description="Watchdog timeout.\nThe amount of clock cycles remaining after which the "
            "dog will bite. When the watchdog bites, the has_bitten signal is set.",
            reset = self.timeout.reset)
        self._has_bitten = CSRStorage(
            description="Watchdog has bitten.\nFlag which is set when timeout has occurred",
            reset = self.has_bitten.reset)

        n = 0 if clock_domain == "sys" else 2
        self.specials += [
            MultiReg(self._enable.storage, self.enable, n=n),
            MultiReg(self._timeout.storage, self.timeout,  n=n),
            MultiReg(self._has_bitten.storage, self.has_bitten, n=n),
        ]

    @classmethod
    def add_mmio_write_registers(cls, mmio, config: 'WatchdogModuleConfig'):
        """
        Adds the storage registers to the MMIO.

        NOTE: Storage registers are meant to be written by LinuxCNC and contain
        the flags and configuration for the module.
        """
        mmio.watchdog_data = CSRStorage(
            size=32, 
            description="Watchdog data.\nByte containing the enabled flag (bit 31) and the time (bit 30 - 0)."
            "out in cpu cycles.", 
            name='watchdog_data',
            write_from_dev=True
        )

    @classmethod
    def add_mmio_read_registers(cls, mmio, config: 'WatchdogModuleConfig'):
        """
        Adds the status registers to the MMIO.

        NOTE: Status registers are meant to be read by LinuxCNC and contain
        the current status of the module.
        """
        mmio.watchdog_has_bitten = CSRStatus(
            size=1, 
            description="Watchdog has bitten.\nFlag which is set when timeout has occurred.", 
            name='watchdog_has_bitten'
        )

    @classmethod
    def create_from_config(cls, soc, config: 'WatchdogModuleConfig'):
        
        # Create a pad for the enabled watchdog
        soc.enable_out = None
        if config.pin is not None:
            soc.platform.add_extension([
                ("watchdog", 0, Pins(config.pin), IOStandard(config.io_standard))
            ])
            soc.enable_out = soc.platform.request_all("watchdog").l[0]

        # Create the watchdog
        watchdog = WatchDogModule(
            soc.enable_out,
            timeout=soc.MMIO_inst.watchdog_data.storage[:31],
            with_csr=False)
        soc.submodules += watchdog
        soc.sync+=[
            # Watchdog input (fixed values)
            watchdog.enable.eq(soc.MMIO_inst.watchdog_data.storage[31]),
            # Watchdog output (status whether the dog has bitten)
            soc.MMIO_inst.watchdog_has_bitten.status.eq(watchdog.has_bitten),
            soc.MMIO_inst.watchdog_has_bitten.we.eq(True)
        ]

        return watchdog
        