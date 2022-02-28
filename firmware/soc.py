from migen import *

from litex_boards.platforms import colorlight_5a_75b, colorlight_5a_75e
from litex_boards.targets.colorlight_5a_75x import _CRG

from litex.soc.cores.clock import *
from litex.soc.cores.pwm import PWM
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

# Local imports
from .gpio import GPIO
from .mmio import MMIO
from .etherbone import Etherbone, EthPhy


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

    @validator('baseclass', pre=True)
    def import_baseclass(cls, value):
        components = value.split('.')
        mod = __import__(components[0])
        for comp in components[1:]:
            mod = getattr(mod, comp)
        return mod


    def generate(self):
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
                    gpio_in: List[GPIO],
                    gpio_out: List[GPIO],
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
                self.submodules.MMIO_inst = MMIO(
                    gpio_in=gpio_in,
                    gpio_out=gpio_out
                )

                # Create GPIO in
                self.platform.add_extension([
                    ("gpio_in", index, Pins(gpio.pin), IOStandard(gpio.io_standard))
                    for index, gpio 
                    in enumerate(gpio_in)
                ])
                self.gpio_inputs = [pad for pad in self.platform.request_all("gpio_in")]
                self.sync += [
                    self.MMIO_inst.gpio_in.status.eq(Cat([gpio for gpio in self.gpio_inputs ])),
                    self.MMIO_inst.gpio_in.we.eq(True)
                ]

                # Create GPIO out
                self.platform.add_extension([
                    ("gpio_out", index, Pins(gpio.pin), IOStandard(gpio.io_standard))
                    for index, gpio 
                    in enumerate(gpio_out)
                ])
                self.gpio_outputs = [pad for pad in self.platform.request_all("gpio_out")]
                self.sync += [
                    gpio.eq(self.MMIO_inst.gpio_out.storage[i])
                    for i, gpio
                    in enumerate(self.gpio_outputs)
                ]


        return _LitexCNC_SoC(
            board=self.board,
            revision=self.revision,
            ethphy= self.ethphy,
            etherbone=self.etherbone,
            gpio_in=self.gpio_in,
            gpio_out=self.gpio_out)
