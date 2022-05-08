from migen import *

from litex_boards.platforms import colorlight_5a_75b, colorlight_5a_75e
from litex_boards.targets.colorlight_5a_75x import _CRG

from litex.soc.cores.clock import *
from litex.soc.integration.soc_core import *
from litex.soc.cores.spi_flash import ECP5SPIFlash
from litex.soc.cores.led import LedChaser

from litex.build.generic_platform import *
from liteeth.core import LiteEthUDPIPCore
from liteeth.common import *
from liteeth.frontend.stream import *
from liteeth.phy.ecp5rgmii import LiteEthPHYRGMII

from typing import List, Type
from pydantic import BaseModel, Field, validator

from firmware.stepgen import StepGen

# Local imports
from .etherbone import Etherbone, EthPhy
from .gpio import GPIO
from .mmio import MMIO
from .pwm import PWM, PwmPdmModule
from .stepgen import StepGen, StepgenModule
from .watchdog import WatchDogModule


class LitexCNC_Firmware(BaseModel):
    baseclass: Type = Field(
        ...,
        description="The base-class for the SoC. See https://github.com/litex-hub/litex-boards/tree/master/litex_boards/targets "
        "for available base-classes and their arguments"
    )
    board: str = Field(
        ...,
        description="Board type (5a-75b or 5a-75e)."
    )
    revision: str = Field(
        ...,
        description="Revision of the board."
    )
    ethphy: EthPhy = Field(
        ...
    )
    etherbone: Etherbone = Field(
        ...,
    )
    gpio_in: List[GPIO] = Field(
        [],
        item_type=GPIO,
        max_items=32,
        unique_items=True
    )
    gpio_out: List[GPIO] = Field(
        [],
        item_type=GPIO,
        max_items=32,
        unique_items=True
    )
    pwm: List[PWM] = Field(
        [],
        item_type=PWM,
        max_items=32,
        unique_items=True
    )
    stepgen: List[StepGen] = Field(
        [],
        item_type=PWM,
        max_items=32,
        unique_items=True
    )

    @validator('baseclass', pre=True)
    def import_baseclass(cls, value):
        components = value.split('.')
        mod = __import__(components[0])
        for comp in components[1:]:
            mod = getattr(mod, comp)
        return mod


    def generate(self, fingerprint):
        """
        Function which generates the firmware
        """
        class _LitexCNC_SoC(self.baseclass):

            def __init__(
                    self,
                    board: str,
                    revision: str,
                    ethphy: EthPhy,
                    etherbone: Etherbone,
                    soc: 'LitexCNC_Firmware',
                    sys_clk_freq=int(50e6)
                    ):

                # Get the correct board type
                board = board.lower()
                assert board in ["5a-75b", "5a-75e"]
                if board == "5a-75b":
                    platform = colorlight_5a_75b.Platform(revision=revision)
                elif board == "5a-75e":
                    platform = colorlight_5a_75e.Platform(revision=revision)

                # SoCMini ----------------------------------------------------------------------------------
                SoCMini.__init__(self, platform, clk_freq=sys_clk_freq,
                    ident          = "ColorLightCNC 5A-75E",
                    ident_version  = True,
                )
                # CRG --------------------------------------------------------------------------------------
                self.submodules.crg = _CRG(platform, sys_clk_freq)
              
                # Etherbone --------------------------------------------------------------------------------
                self.submodules.ethphy = LiteEthPHYRGMII(
                    clock_pads = self.platform.request("eth_clocks"),
                    pads       = self.platform.request("eth"),
                    **{key: value for key, value in ethphy.dict(exclude={'module'}).items() if value is not None})
                self.add_etherbone(
                    phy=self.ethphy,
                    buffer_depth=etherbone.buffer_depth,
                    mac_address=etherbone.mac_address,
                    ip_address=str(etherbone.ip_address))
                
                # Create memory mapping for IO
                self.submodules.MMIO_inst = MMIO(soc=soc, fingerprint=fingerprint)

                # Create watchdog
                watchdog = WatchDogModule(timeout=self.MMIO_inst.watchdog_data.storage[:31], with_csr=False)
                self.submodules += watchdog
                self.sync+=[
                    # Watchdog input (fixed values)
                    watchdog.enable.eq(self.MMIO_inst.watchdog_data.storage[31]),
                    # Watchdog output (status whether the dog has bitten)
                    self.MMIO_inst.watchdog_has_bitten.status.eq(watchdog.has_bitten),
                    self.MMIO_inst.watchdog_has_bitten.we.eq(True)
                ]

                # Create a wall-clock
                self.sync+=[
                    # Increment the wall_clock
                    self.MMIO_inst.wall_clock.status.eq(self.MMIO_inst.wall_clock.status + 1),
                    self.MMIO_inst.wall_clock.we.eq(True)
                ]

                # Create GPIO in
                self.platform.add_extension([
                    ("gpio_in", index, Pins(gpio.pin), IOStandard(gpio.io_standard))
                    for index, gpio 
                    in enumerate(soc.gpio_in)
                ])
                self.gpio_inputs = [pad for pad in self.platform.request_all("gpio_in").l]
                self.sync += [
                    self.MMIO_inst.gpio_in.status.eq(Cat([gpio for gpio in self.gpio_inputs])),
                    self.MMIO_inst.gpio_in.we.eq(True)
                ]

                # Create GPIO out
                self.platform.add_extension([
                    ("gpio_out", index, Pins(gpio.pin), IOStandard(gpio.io_standard))
                    for index, gpio 
                    in enumerate(soc.gpio_out)
                ])
                self.gpio_outputs = [pad for pad in self.platform.request_all("gpio_out").l]
                self.sync += [
                    gpio.eq(self.MMIO_inst.gpio_out.storage[i] & ~watchdog.has_bitten)
                    for i, gpio
                    in enumerate(self.gpio_outputs)
                ]

                # Create PWM
                # - create the physical output pins
                self.platform.add_extension([
                    ("pwm", index, Pins(pwm_instance.pin), IOStandard(pwm_instance.io_standard))
                    for index, pwm_instance 
                    in enumerate(soc.pwm)
                ])
                self.pwm_outputs = [pad for pad in self.platform.request_all("pwm").l]
                # - create the generators
                for index, _ in enumerate(soc.pwm):
                    # Add the PWM-module to the platform
                    _pwm = PwmPdmModule(self.pwm_outputs[index], with_csr=False)
                    self.submodules += _pwm
                    self.sync+=[
                        _pwm.enable.eq(
                            getattr(self.MMIO_inst, f'pwm_{index}_enable').storage
                            & ~watchdog.has_bitten),
                        _pwm.period.eq(getattr(self.MMIO_inst, f'pwm_{index}_period').storage),
                        _pwm.width.eq(getattr(self.MMIO_inst, f'pwm_{index}_width').storage)
                    ]

                # Create StepGen
                # - create the physical output pins
                # -> step
                self.platform.add_extension([
                    ("stepgen_step", index, Pins(stepgen_instance.step_pin), IOStandard(stepgen_instance.io_standard)) 
                    for index, stepgen_instance 
                    in enumerate(soc.stepgen)
                ])
                self.stepgen_step_outputs = [pad for pad in self.platform.request_all("stepgen_step").l]
                # -> dir
                self.platform.add_extension([
                    ("stepgen_dir", index, Pins(stepgen_instance.dir_pin), IOStandard(stepgen_instance.io_standard)) 
                    for index, stepgen_instance 
                    in enumerate(soc.stepgen)
                ])
                self.stepgen_dir_outputs = [pad for pad in self.platform.request_all("stepgen_dir").l]
                sync_statements = []
                reset_statements = []
                for index, stepgen_config in enumerate(soc.stepgen):
                    _stepgen = StepgenModule(28, stepgen_config.soft_stop)
                    self.submodules += _stepgen
                    # Combine input
                    self.comb += [
                        _stepgen.enable.eq(~watchdog.has_bitten & ~self.MMIO_inst.reset.storage),
                        _stepgen.steplen.eq(self.MMIO_inst.stepgen_steplen.storage),
                        _stepgen.dir_hold_time.eq(self.MMIO_inst.stepgen_dir_hold_time.storage),
                        _stepgen.dir_setup_time.eq(self.MMIO_inst.stepgen_dir_setup_time.storage),
                    ]
                    # Combine output
                    self.comb += [
                        self.stepgen_step_outputs[index].eq(_stepgen.step),
                        self.stepgen_dir_outputs[index].eq(_stepgen.dir),
                        # Store data for position and speed feedbavk
                        getattr(self.MMIO_inst, f'stepgen_{index}_position').status.eq(_stepgen.position),
                        getattr(self.MMIO_inst, f'stepgen_{index}_position').we.eq(True),
                        getattr(self.MMIO_inst, f'stepgen_{index}_speed').status.eq(_stepgen.speed),
                        getattr(self.MMIO_inst, f'stepgen_{index}_speed').we.eq(True),
                    ]
                    sync_statements.extend([
                        _stepgen.speed_target.eq(getattr(self.MMIO_inst, f'stepgen_{index}_speed_target').storage),
                        _stepgen.max_acceleration.eq(getattr(self.MMIO_inst, f'stepgen_{index}_max_acceleration').storage),
                    ])
                    reset_statements.extend([
                        _stepgen.speed_target.eq(0x80000000),
                        _stepgen.position.eq(0),
                    ])
                # Apply the sync-statements when the time is ready
                self.sync += If(
                        self.MMIO_inst.reset.storage,
                        *reset_statements
                    ).Else(
                        If(
                            self.MMIO_inst.wall_clock.status >= self.MMIO_inst.stepgen_apply_time.storage,
                            # Reset the stepgen_apply_time
                            self.MMIO_inst.stepgen_apply_time.dat_w.eq(0),
                            self.MMIO_inst.stepgen_apply_time.we.eq(True),
                            # Pass the target speed to the stepgens
                            *sync_statements
                        ).Else(
                            self.MMIO_inst.stepgen_apply_time.we.eq(False),
                        )
                    )

        return _LitexCNC_SoC(
            board=self.board,
            revision=self.revision,
            ethphy= self.ethphy,
            etherbone=self.etherbone,
            soc=self)
