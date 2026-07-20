# Default imports
import os
try:
    from typing import ClassVar, List, Literal, Union
except ImportError:
    # Imports for Python <3.8
    from typing import ClassVar, List, Union
    from typing_extensions import Literal
from typing_extensions import Annotated

# Imports for the configuration
from pydantic import Field

# Import of the basemodel, required to register this module
from . import ModuleBaseModel, ModuleInstanceBaseModel


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
    hal_pins: ClassVar[List[str]] = [
        'in',
        'in-not',
    ]
    hal_params: ClassVar[List[str]] = []



class GPIO_PinOut(GPIO_PinBase):
    direction: Literal['out']
    safe_state: bool = Field(
        False,
        description="The safe state of the pin. By default the safe state is "
        "False, meaning the output is LOW. When logic negates the output, it can "
        "be required to set the pin to True, meaning when the FPGA starts with "
        "the output HIGH. When LinuxCNC is running, the behavior of the pin is "
        "governed by the `invert_output` parameter; this setting does not alter "
        "behavior when running LinuxCNC."
    )
    hal_pins: ClassVar[List[str]] = [
        'out',
    ]
    hal_params: ClassVar[List[str]] = [
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
        # Deferred imports to prevent importing Litex while installing the driver
        from litexcnc.firmware.modules.gpio import GPIO_Module
        GPIO_Module.create_from_config(soc, self)
    
    def add_mmio_config_registers(self, mmio):
        # The GPIO does not require any config, so this function is
        # not implemented.
        return

    def add_mmio_write_registers(self, mmio):
        # Deferred imports to prevent importing Litex while installing the driver
        from litexcnc.firmware.modules.gpio import GPIO_Module
        GPIO_Module.add_mmio_write_registers(mmio, self)

    def add_mmio_read_registers(self, mmio):
        # Deferred imports to prevent importing Litex while installing the driver
        from litexcnc.firmware.modules.gpio import GPIO_Module
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
        # Deferred imports to prevent importing Litex while installing the driver
        from litex.soc.interconnect.csr import CSRStatus
        # Create identifiers for each pin
        config = 0
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
