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

from litexcnc.config.modules.watchdog import (
    WatchdogModuleConfig,
    WatchdogFunctionBaseConfig,
    WatchdogFunctionEnableConfig,
    WatchdogFunctionHeartBeatConfig
)


class WatchDogFunctionBase(Module, AutoCSR):
    """Base class for watchdog functions."""

    def __init__(self, pad, clock_frequency: int, config: WatchdogFunctionBaseConfig):
        self.watchdog_has_bitten = Signal()
        self.watchdog_has_estop = Signal()
        self.watchdog_is_enabled = Signal()
        self.watchdog_ok_in = Signal()

    @classmethod
    def create_function_from_config(cls, soc, watchdog: 'WatchDogModule', config):
        
        function_name = f"watchdog_function_{str(uuid.uuid4()).replace('-', '_')}"
        soc.platform.add_extension([
            (function_name, 0, Pins(config.pin), IOStandard(config.io_standard))
        ])

        watchdog_function = cls(
            soc.platform.request(function_name, 0),
            soc. clock_frequency,
            config
        )
        watchdog.submodules += watchdog_function
        watchdog.comb += [
            watchdog_function.watchdog_has_estop.eq(watchdog.has_estop),
            watchdog_function.watchdog_has_bitten.eq(watchdog.has_bitten),
            watchdog_function.watchdog_is_enabled.eq(watchdog.enable),
            watchdog_function.watchdog_ok_in.eq(watchdog.ok_in),
        ]

        return watchdog_function    


class WatchDogEnableFunction(WatchDogFunctionBase):
    """Function which put an enable signal on a pin"""

    def __init__(self, pad, clock_frequency: int, config: WatchdogFunctionEnableConfig):
        super().__init__(pad, clock_frequency, config)
        self.comb += [
            pad.eq(
                (
                    self.watchdog_is_enabled & 
                    self.watchdog_ok_in & 
                    ~self.watchdog_has_bitten & 
                    ~self.watchdog_has_estop
                ) ^ config.invert_output
            )
        ]


class WatchDogHeartBeatFunction(WatchDogFunctionBase):
    """Function which putd a PWM pattern on a pin"""

    HEARTBEAT = [
        0.08235294117647059, 0.08235294117647059, 0.08235294117647059, 0.08235294117647059, 0.08235294117647059,
        0.08235294117647059, 0.08235294117647059, 0.08235294117647059, 0.08235294117647059, 0.08235294117647059, 
        0.08235294117647059, 0.08235294117647059, 0.08235294117647059, 0.08627450980392157, 0.09019607843137255, 
        0.09803921568627451, 0.10980392156862745, 0.13333333333333333, 0.16470588235294117, 0.21176470588235294, 
        0.2784313725490196, 0.3607843137254902, 0.4588235294117647, 0.5686274509803921, 0.6862745098039216,
        0.796078431372549, 0.8941176470588236, 0.9647058823529412, 1.0, 0.996078431372549,
        0.9490196078431372, 0.8627450980392157, 0.7490196078431373, 0.615686274509804, 0.4745098039215686,
        0.3411764705882353, 0.22745098039215686, 0.13333333333333333, 0.06666666666666667, 0.023529411764705882, 
        0.00392156862745098, 0.0, 0.00784313725490196, 0.0196078431372549, 0.03529411764705882,
        0.050980392156862744, 0.06274509803921569, 0.07058823529411765, 0.07450980392156863, 0.0784313725490196,
        0.08235294117647059, 0.08235294117647059, 0.08235294117647059, 0.08235294117647059, 0.08235294117647059,
        0.08235294117647059, 0.08235294117647059, 0.08235294117647059, 0.08235294117647059, 0.08235294117647059,
        0.08235294117647059, 0.08235294117647059, 0.08235294117647059, 0.08235294117647059]
    SINUS = [
        0.5, 0.54900857, 0.597545161, 0.645142339, 0.691341716,
        0.735698368, 0.777785117, 0.817196642, 0.853553391, 0.886505227,
        0.915734806, 0.940960632, 0.961939766, 0.978470168, 0.99039264,
        0.997592363, 1, 0.997592363, 0.99039264, 0.978470168,
        0.961939766, 0.940960632, 0.915734806, 0.886505227, 0.853553391,
        0.817196642, 0.777785117, 0.735698368, 0.691341716, 0.645142339,
        0.597545161, 0.54900857, 0.5, 0.45099143, 0.402454839,
        0.354857661, 0.308658284, 0.264301632, 0.222214883, 0.182803358,
        0.146446609, 0.113494773, 0.084265194, 0.059039368, 0.038060234,
        0.021529832, 0.00960736, 0.002407637, 0, 0.002407637,
        0.00960736, 0.021529832, 0.038060234, 0.059039368, 0.084265194, 
        0.113494773, 0.146446609, 0.182803358, 0.222214883, 0.264301632,
        0.308658284, 0.354857661, 0.402454839, 0.45099143
    ]

    def __init__(self, pad, clock_frequency: int, config: WatchdogFunctionHeartBeatConfig):
        super().__init__(pad, clock_frequency, config)

        pwm_period = int(clock_frequency/config.pwm_frequency)
        waveform = {
            "heartbeat": self.HEARTBEAT,
            "sinus": self.SINUS
        }[config.waveform]
        heartbeat = Array([int(element*pwm_period) for element in waveform])
        pwm_width = Signal(32)
        pwm_counter = Signal(32, reset_less=True)
        beat_period = int(clock_frequency/(config.speed*len(waveform)))  # 70 bpm, with 40 MHz clock
        beat_counter = Signal(32)
        index_counter = Signal(32)

        self.sync += [
            If(
                ~self.watchdog_is_enabled | self.watchdog_has_bitten,
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

    class WatchdogData():
        def __init__(self, mmio) -> None:
            self.data_in=mmio.watchdog_data
            self.data_out=mmio.watchdog_status

    class EStopConfig():
        def __init__(self, soc, config) -> None:
            
            self.pads = None
            self.mask = None
            self.num_estop = len(config.estop)
            self.is_defined = False

            if config.estop:
                # Create the extension
                soc.platform.add_extension([
                    ("estop", index, Pins(estop.pin), IOStandard(estop.io_standard))
                    for index, estop 
                    in enumerate(config.estop)
                ])
                self.pads = soc.platform.request_all("estop")
                self.mask = sum([(1<<index)*(not estop.trigger) for index, estop in enumerate(config.estop)])
                self.is_defined = True
                

    @property
    def has_bitten(self):
        return self.data_out.status[0]
    
    @property
    def has_estop(self):
        if self.estop_config.is_defined:
            return self.data_out.status[1:1+self.estop_config.num_estop] != 0
        return False
    
    @property
    def fault_out(self):
        return self.data_out.status != 0
    
    @property
    def ok_out(self):
        return self.data_out.status == 0
    
    @property
    def time_out(self):
        return self.data_in.storage[0:29]

    @property
    def enable(self):
        return self.data_in.storage[31]
    
    @property
    def reset(self):
        return self.data_in.storage[30]
    
    @property
    def ok_in(self):
        return self.data_in.storage[29]
    
    @staticmethod
    def _to_signal(obj):
        return obj.raw_bits() if isinstance(obj, Record) else obj
    
    def __init__(self, watchdog_data: WatchdogData, estop_config: EStopConfig, clock_domain="sys", default_timeout = 0, with_csr=True):
        # Parameters for the watchdog:
        # - the bit enable sets whether the watchdog is enabled. This is written when the 
        #   timeout is read for the first-time from the driver to FPGA
        # - timeout: counter which is decreased at each clock-cycle. When it is at 0, the 
        #   watchdog will get very angry and bite.
        self.estop_config = estop_config
        self.data_in = watchdog_data.data_in
        self.data_out = watchdog_data.data_out
        self.internal_reset = Signal()

        # Procedure of the watchdog
        sync = getattr(self.sync, clock_domain)
        sync += [
            If(
                self.internal_reset,
                self.data_out.status.eq(0),
                self.data_in.storage.eq(0),
            ).Elif(
                self.enable,
                If(self.time_out > 0,
                    # Still some time left before freaking out
                    self.time_out.eq(self.time_out-1),
                    self.data_out.status[0].eq(0),
                ).Else(
                    # The dog got angry, and will bite
                    self.data_out.status[0].eq(1),
                )
            ),
        ]

        if estop_config.is_defined:
            self.estop = Signal(len(estop_config.pads))
            self.specials += MultiReg(self._to_signal(estop_config.pads), self.estop)
            self.sync += [
                If(
                    self.reset & (self.estop ^ ~0b1),
                    self.data_out.status[1:1+len(estop_config.pads)].eq(0)
                ).Elif(
                    self.enable,
                    self.data_out.status[1:1+len(estop_config.pads)].eq(self.data_out.status[1:1+len(estop_config.pads)] | (self.estop ^ estop_config.mask))
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
            # fields=[
            #     CSRField("timeout", size=30, offset=0, description="The length of the step pulse in clock cycles", access=CSRAccess.ReadWrite),
            #     CSRField("reset", size=1, offset=30, description="The length of the step pulse in clock cycles", access=CSRAccess.ReadWrite),
            #     CSRField("enable", size=1, offset=31, description="The length of the step pulse in clock cycles", access=CSRAccess.ReadWrite),
            # ],
            32,
            description="Watchdog data.\nByte containing the enable flag (bit 31), reset flag (bit 30) and the "
            "timeout (bit 29 - 0) in cpu cycles.", 
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
        import math
        mmio.watchdog_status = CSRStatus(
            size=int(math.ceil(float(len(config.estop)+1)/32))*32,
            name='watchdog_status',
            description="Register containing the state of the EStops and the Watchdog has bitten flag."
        )
        # mmio.watchdog_has_bitten = CSRStatus(
        #     size=1, 
        #     description="Watchdog has bitten.\nFlag which is set when timeout has occurred.", 
        #     name='watchdog_has_bitten'
        # )

    @classmethod
    def create_from_config(cls, soc, config: 'WatchdogModuleConfig'):
        
        # Create the watchdog
        soc.platform.add_extension([
            ("estop", index, Pins(estop.pin), IOStandard(estop.io_standard))
            for index, estop 
            in enumerate([gpio for gpio in config.estop])
        ])
        watchdog = WatchDogModule(
            watchdog_data=WatchDogModule.WatchdogData(soc.MMIO_inst),
            estop_config=WatchDogModule.EStopConfig(soc, config),
            with_csr=False)
        soc.submodules += watchdog
        soc.sync+=[
            # Watchdog input (fixed values)
            watchdog.internal_reset.eq(soc.MMIO_inst.reset.storage)
        ]

        # Add functions
        for function in config.functions:
            function.create_from_config(soc, watchdog, function)

        return watchdog
        