#
# This file is part of LiteX-CNC.
#
# Copyright (c) 2022 Peter van Tol <petertgvantolATgmailDOTcom>
# SPDX-License-Identifier: MIT

import uuid
from litex.soc.interconnect.csr import *

# Imports for Migen
from migen import *
from migen.genlib.cdc import MultiReg
from litex.build.generic_platform import Pins, IOStandard
from litex.soc.interconnect.csr import AutoCSR, CSRStorage

from litexcnc.config.modules.watchdog import WatchdogModuleConfig, WatchdogFunctionEnableConfig


class WatchDogFunctionBase(Module, AutoCSR):

    @classmethod
    def create_function_from_config(cls, soc, watchdog: 'WatchDogModule', config):
        
        function_name = f"watchdog_function_{str(uuid.uuid4()).replace('-', '_')}"
        soc.platform.add_extension([
            (function_name, 0, Pins(config.pin), IOStandard(config.io_standard))
        ])

        watchdog_function = cls(
            soc.platform.request(function_name, 0),
            config
        )
        watchdog.submodules += watchdog_function
        watchdog.comb += [
            watchdog_function.watchdog_has_bitten.eq(watchdog.has_bitten)
        ]

        return watchdog_function    

class WatchDogEnableFunction(WatchDogFunctionBase):

    def __init__(self, pad, config: WatchdogFunctionEnableConfig):
        self.watchdog_has_bitten = Signal()
        self.comb += [
            pad.eq((~self.watchdog_has_bitten) ^ config.invert_output)
        ]


class WatchDogHeartBeatFunction(WatchDogFunctionBase):

    def __init__(self, pad, config):
        self.watchdog_has_bitten = Signal()

        heartbeat = Array([
            3281, 3281, 3281, 3281, 3281, 3281, 3281, 3281, 3281, 3281,
            3281, 3281, 3281, 3437, 3593, 3906, 4375, 5312, 6562, 8437,
            11093, 14375, 18281, 22656, 27343, 31718, 35625, 38437, 39843, 39687,
            37812, 34375, 29843, 24531, 18906, 13593, 9062, 5312, 2656, 937,
            156, 0, 312, 781, 1406, 2031, 2500, 2812, 2968, 3125,
            3281, 3281, 3281, 3281, 3281, 3281, 3281, 3281, 3281, 3281, 
            3281, 3281, 3281, 3281])
        pwm_period = 40000
        pwm_width = Signal(32)
        pwm_counter = Signal(32, reset_less=True)
        beat_period = 536000  # 70 bpm, with 40 MHz clock
        beat_counter = Signal(32)
        index_counter = Signal(32)

        self.sync += [
            If(
                self.watchdog_has_bitten,
                pad.eq(0 ^ config.invert_output)
            ).Else(
                # PWM mode
                pwm_counter.eq(pwm_counter + 1),
                beat_counter.eq(beat_counter + 1),
                If(pwm_counter < pwm_width,
                    pad.eq(1 ^ config.invert_output)
                ).Else(
                    pad.eq(0 ^ config.invert_output)
                ),
                If(pwm_counter >= (pwm_period - 1),
                    pwm_counter.eq(0)
                ),
                If(beat_counter >= (beat_period),
                    beat_counter.eq(0),
                    If(index_counter == 63, 
                       index_counter.eq(0),
                    ).Else(
                       index_counter.eq(index_counter+1),
                    )
                ),
                pwm_width.eq(heartbeat[index_counter])
            )
        ]


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
            If(
                self.enable,
                If(self.timeout > 0,
                    # Still some time left before freaking out
                    self.timeout.eq(self.timeout-1),
                    self.has_bitten.eq(0),
                ).Else(
                    # The dog got angry, and will bite
                    self.has_bitten.eq(1),
                )
            ).Else(
                # Sleeping dogs don't bite, but the board is also not active
                self.has_bitten.eq(0),
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
        
        # Create the watchdog
        watchdog = WatchDogModule(
            timeout=soc.MMIO_inst.watchdog_data.storage[:31],
            with_csr=False)
        soc.submodules += watchdog
        soc.sync+=[
            # Watchdog input (fixed values)
            watchdog.enable.eq(soc.MMIO_inst.watchdog_data.storage[31]),
            # Watchdog output (status whether the dog has bitten)
            soc.MMIO_inst.watchdog_has_bitten.status.eq(watchdog.has_bitten),
            soc.MMIO_inst.watchdog_has_bitten.we.eq(True),
            If(
                soc.MMIO_inst.reset.storage,
                soc.MMIO_inst.watchdog_data.storage.eq(0)
            )
        ]

        # Add functions
        for function in config.functions:
            function.create_from_config(soc, watchdog, function)

        return watchdog
        