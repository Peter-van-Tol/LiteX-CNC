#
# This file is part of LiteX-CNC.
#
# Copyright (c) 2022 Peter van Tol <petertgvantolATgmailDOTcom>
# SPDX-License-Identifier: MIT

from litex.soc.interconnect.csr import *
from migen import *
from migen.genlib.cdc import MultiReg
from pydantic import BaseModel, Field


class WatchDogSettings():
    ...


class WatchDogModule(Module, AutoCSR):
    """
    Pet this, otherwise it will bite.
    """
    def __init__(self, timeout=Signal(31), clock_domain="sys", default_timeout = 0, with_csr=True):
        # Parameters for the watchdog:
        # - the bit enable sets whether the watchdog is enabled. This is written when the 
        #   timeout is read for the first-time from the driver to FPGA
        # - timeout: counter which is decreased at each clock-cycle. When it is at 0, the 
        #   watchdog will get very angry and bite.
        self.enable  = Signal()
        self.timeout = timeout
        self.has_bitten = Signal()

        # Procedure of the watchdog
        sync = getattr(self.sync, clock_domain)
        sync += [
            If(self.enable,
                If(self.timeout > 0,
                    # Still some time left before freaking out
                    self.timeout.eq(self.timeout-1),
                    self.has_bitten.eq(0)
                ).Else(
                    # The dog got angry, and will bite
                    self.has_bitten.eq(1)
                )
            ).Else(
                # Sleeping dogs don't bite
                self.has_bitten.eq(0)
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
        