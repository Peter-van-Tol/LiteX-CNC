"""

"""
from typing import List
import math
from pydantic import BaseModel, Field

from migen import *
from migen.genlib.cdc import MultiReg
from litex.build.generic_platform import *
from litex.soc.interconnect.csr import CSRStatus, CSRStorage
from litex.soc.integration.doc import AutoDoc, ModuleDoc



class GPIO(BaseModel):
    pin: str = Field(
        description="The pin on the FPGA-card."
    )
    name: str = Field(
        None,
        description="The name of the pin as used in LinuxCNC HAL-file (optional). "
        "When not specified, the standard pin litexcnc.digital-in.x and "
        "litexcnc.digital-in.x.not will be created."
    )
    io_standard: str = Field(
        "LVCMOS33",
        description="The IO Standard (voltage) to use for the pin."
    )


def _to_signal(obj):
    return obj.raw_bits() if isinstance(obj, Record) else obj


class GPIO_Out(Module, AutoDoc):
    """Module for creating output signals"""
    pads_layout = [("pin", 1)]

    def __init__(self, register, pads=None) -> None:
        # AutoDoc implementation
        self.intro = ModuleDoc("Module for creating output signals.")
        
        # Implementation of GPIO_out, the output signal has the same width
        # as the number of GPIO out (they share the same register)
        pads = _to_signal(pads)
        self.comb += pads.eq(register)

    @classmethod
    def create_from_config(cls, soc, config: List[GPIO]):
        """
        Creates the GPIO from the config file.
        """
        # Don't create the registers when the config is empty (no GPIO 
        # defined in this case)
        if not config:
            return
        
        # Add the pins for the GPIO out
        soc.platform.add_extension([
            ("gpio_out", index, Pins(gpio.pin), IOStandard(gpio.io_standard))
            for index, gpio 
            in enumerate(config)
        ])
        gpio_out = cls(
            soc.MMIO_inst.gpio_out.storage,
            soc.platform.request_all("gpio_out")
        )
        soc.submodules += gpio_out

    @classmethod
    def add_mmio_read_registers(cls, mmio, config: List[GPIO]):
        """
        Adds the status registers to the MMIO.

        NOTE: Status registers are meant to be read by LinuxCNC and contain
        the current status of the module.
        """
        # The GPIO only writes data to the pins, no feedback
        return
            
    @classmethod
    def add_mmio_write_registers(cls, mmio, config: List[GPIO]):
        """
        Adds the storage registers to the MMIO.

        NOTE: Storage registers are meant to be written by LinuxCNC and contain
        the flags and configuration for the module.
        """
        # Don't create the registers when the config is empty (no GPIO 
        # defined in this case)
        if not config:
            return

        mmio.gpio_out = CSRStorage(
            size=int(math.ceil(float(len(config))/32))*32,
            name='gpio_out',
            description="Register containing the bits to be written to the GPIO out pins.", 
            write_from_dev=False
        )


class GPIO_In(Module, AutoDoc):
    """Module for creating output signals"""
    pads_layout = [("pin", 1)]

    def __init__(self, register, pads=None) -> None:
        # AutoDoc implementation
        self.intro = ModuleDoc("Module for creating output signals.")
        
        # Implementation of GPIO_in, the input signal has the same width
        # as the number of GPIO in (they share the same register)
        pads = _to_signal(pads)
        self.specials += MultiReg(pads, register)

    @classmethod
    def create_from_config(cls, soc, config: List[GPIO]):
        """
        Creates the GPIO from the config file.
        """
        # Don't create the registers when the config is empty (no GPIO 
        # defined in this case)
        if not config:
            return
        
        # Add the pins for the GPIO out
        soc.platform.add_extension([
            ("gpio_in", index, Pins(gpio.pin), IOStandard(gpio.io_standard))
            for index, gpio 
            in enumerate(config)
        ])
        gpio_in = cls(
            soc.MMIO_inst.gpio_in.status,
            soc.platform.request_all("gpio_in")
        )
        soc.submodules += gpio_in

    @classmethod
    def add_mmio_read_registers(cls, mmio, config: List[GPIO]):
        """
        Adds the status registers to the MMIO.

        NOTE: Status registers are meant to be read by LinuxCNC and contain
        the current status of the module.
        """
        # Don't create the registers when the config is empty (no GPIO 
        # defined in this case)
        if not config:
            return

        mmio.gpio_in = CSRStatus(
            size=int(math.ceil(float(len(config))/32))*32,
            name='gpio_in',
            description="Register containing the bits to be written to the GPIO in pins."
        )
            
    @classmethod
    def add_mmio_write_registers(cls, mmio, config: List[GPIO]):
        """
        Adds the storage registers to the MMIO.

        NOTE: Storage registers are meant to be written by LinuxCNC and contain
        the flags and configuration for the module.
        """
        # The GPIO only reads data from the pins, no feedback
        return
