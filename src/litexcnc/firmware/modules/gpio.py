

# Default imports
import math
import os
from typing import ClassVar, List, Literal, Union
from typing_extensions import Annotated

# Imports for the configuration
from pydantic import Field

# Imports for Migen
from migen import *
from migen.genlib.cdc import MultiReg
from litex.build.generic_platform import *
from litex.soc.interconnect.csr import CSRStatus, CSRStorage
from litex.soc.integration.doc import AutoDoc, ModuleDoc

# Import of the basemodel, required to register this module
from . import ModuleBaseModel, ModuleInstanceBaseModel


class GPIO_Module(Module, AutoDoc):
    """Module for creating output signals"""

    def __init__(self, mmio, pads_out=None, pads_in=None) -> None:
        # AutoDoc implementation
        self.intro = ModuleDoc("Module for GPIO signals.")
        
        # Implementation of GPIO_out, the output signal has the same width
        # as the number of GPIO out (they share the same register)
        if pads_out is not None:
            pads_out = self._to_signal(pads_out)
            self.comb += pads_out.eq(mmio.gpio_out.storage)

        # Implementation of GPIO_in, the input signal has the same width
        # as the number of GPIO in (they share the same register)
        if pads_in is not None:
            pads_in = self._to_signal(pads_in)
            self.specials += MultiReg(pads_in, mmio.gpio_in.status)

    @classmethod
    def create_from_config(cls, soc, config: 'GPIO_ModuleConfig'):
        """
        Creates the GPIO from the config file.
        """
        # Don't create the registers when the config is empty (no GPIO 
        # defined in this case)
        if not config:
            return
        
        # Add the pins for the GPIO
        # - output
        pads_out = None
        if any([instance.direction == "out" for instance in config.instances]):
            soc.platform.add_extension([
                ("gpio_out", index, Pins(gpio.pin), IOStandard(gpio.io_standard))
                for index, gpio 
                in enumerate([gpio for gpio in config.instances if gpio.direction == "out"])
            ])
            pads_out = soc.platform.request_all("gpio_out")
        # - input
        pads_in = None
        if any([instance.direction == "in" for instance in config.instances]):
            soc.platform.add_extension([
                ("gpio_in", index, Pins(gpio.pin), IOStandard(gpio.io_standard))
                for index, gpio 
                in enumerate([gpio for gpio in config.instances if gpio.direction == "in"])
            ])
            pads_in = soc.platform.request_all("gpio_in")

        # Create the GPIO module
        gpio = cls(
            soc.MMIO_inst,
            pads_out,
            pads_in
        )
        soc.submodules += gpio

    @classmethod
    def add_mmio_write_registers(cls, mmio, config: 'GPIO_ModuleConfig'):
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
            size=int(math.ceil(float(sum(1 for instance in config.instances if instance.direction == "out"))/32))*32,
            name='gpio_out',
            description="Register containing the bits to be written to the GPIO out pins.", 
            write_from_dev=False
        )


    @classmethod
    def add_mmio_read_registers(cls, mmio, config: 'GPIO_ModuleConfig'):
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
            size=int(math.ceil(float(sum(1 for instance in config.instances if instance.direction == "in"))/32))*32,
            name='gpio_in',
            description="Register containing the bits to be written to the GPIO in pins."
        )

    @staticmethod
    def _to_signal(obj):
        return obj.raw_bits() if isinstance(obj, Record) else obj


class GPIO_PinBase(ModuleInstanceBaseModel):
    """
    Model describing a pin of the GPIO.
    """
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


class GPIO_PinIn(GPIO_PinBase):
    direction: Literal['in']
    pins: ClassVar[List[str]] = [
        'in',
        'in-not',
    ]


class GPIO_PinOut(GPIO_PinBase):
    direction: Literal['out']
    pins: ClassVar[List[str]] = [
        'out',
    ]
    params: ClassVar[List[str]] = [
        'invert_output',
    ]


GPIO_Pin = Annotated[
    Union[GPIO_PinIn, GPIO_PinOut],
    Field(direction="direction")]


class GPIO_ModuleConfig(ModuleBaseModel):
    """
    Module describing the GPIO module
    """
    module_type: Literal['gpio'] = 'gpio'
    module_id: ClassVar[int] = 0x6770696f  # The string `gpio` in hex, must be equal to litexcnc_gpio.h
    driver_files: ClassVar[List[str]] = [
        os.path.dirname(__file__) + '/../../driver/modules/litexcnc_gpio.c',
        os.path.dirname(__file__) + '/../../driver/modules/litexcnc_gpio.h'
    ]
    instances: List[GPIO_Pin] = Field(
        [],
        item_type=GPIO_Pin,
        unique_items=True,
    )

    def create_from_config(self, soc, _):
        GPIO_Module.create_from_config(soc, self)
    
    def add_mmio_config_registers(self, mmio):
        # The GPIO does not require any config, so this function is
        # not implemented.
        return

    def add_mmio_write_registers(self, mmio):
        GPIO_Module.add_mmio_write_registers(mmio, self)

    def add_mmio_read_registers(self, mmio):
        GPIO_Module.add_mmio_read_registers(mmio, self)

    @property
    def config_size(self):
        """Calculates the number DWORDS required to store the direction data
        of the GPIO register. The first two bytes are used for (hence 16 (2*8)
        is added to the number of instances):
        - number of inputs
        - number of outputs

        The number of DWORDS is finally converted in number of bytes.
        """
        return (((len(self.instances)+16)>>5) + (1 if ((len(self.instances)+16) & 0x1F) else 0)) * 4

    def store_config(self, mmio):
        config = 0
        # Create identifiers for each pin
        for index, instance in enumerate(self.instances):
            if instance.direction == "out":
                config |= (1 << index)
        # Number of output pins
        config += sum(1 for instance in self.instances if instance.direction == "out") << (self.config_size * 8 - 8)
        # Number of input pins
        config += sum(1 for instance in self.instances if instance.direction == "in") << (self.config_size * 8 - 16)
        # Create the config
        mmio.gpio_config_data =  CSRStatus(
            size=self.config_size*8,
            reset=config,
            description=f"The config of the GPIO module."
        )

