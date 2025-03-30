# Default imports
import os
try:
    from typing import ClassVar, List, Literal, Union
except ImportError:
    # Imports for Python <3.8
    from typing import ClassVar, List, Union
    from typing_extensions import Literal
from typing_extensions import Annotated
import math

# Imports for the configuration
from pydantic import BaseModel, Field, validator

# Import of the basemodel, required to register this module
from . import ModuleBaseModel, ModuleInstanceBaseModel
from litexcnc.firmware.modules.gpio_expander import(
    GpioExpander74HCT595Module
)


class GpioExpanderBase(ModuleInstanceBaseModel):
    ...

    def add_mmio_write_registers(self, mmio, index):
        raise NotImplementedError("Should be implemented in sub-class")
    
    def add_mmio_read_registers(self, mmio, index):
        raise NotImplementedError("Should be implemented in sub-class")

    @property
    def config_size(self):
        raise NotImplementedError("Should be implemented in sub-class")

    def get_config(self):
        raise NotImplementedError("Should be implemented in sub-class")
    
    def create_from_config(self, soc, index):
        raise NotImplementedError("Should be implemented in sub-class")


class ExpanderPort(BaseModel):
    pin_names: dict[int, str] = Field(
        {},
        description="The names of the pins of this port expander. The pin name "
        "will change `<BOARD_NAME>.gpio.exp.0.0.out` to `<BOARD_NAME>.gpio.<pin_name>.out. "
        "The name will ensure the pin_name is placed within the 'regular' GPIO range." 
    )

    @validator("pin_names")
    def pin_index_in_range(cls, v: dict[int, str]):
        for key in v.keys():
            if key not in (0, 1, 2, 3, 4, 5, 6, 7):
                raise KeyError("Keys should be an integer in range [0-7].")
        return v


class GpioExpander74HCT595Pins(BaseModel):
    serial_data: str = Field(
        description="The pin on the FPGA-card for the serial data signal."
    )
    clock: str = Field(
        None,
        description="The pin on the FPGA-card for the clock signal."
    )
    latch: str = Field(
        None,
        description="The pin on the FPGA-card for the latch signal."
    )
    clear: str = Field(
        None,
        description="The pin on the FPGA-card for the clear signal."
    ) 
    io_standard: str = Field(
        "LVCMOS33",
        description="The IO Standard (voltage) to use for the pins."
    )


class GpioExpander74HCT595Config(GpioExpanderBase):
    expander_type: Literal["shift_out"] = "shift_out"
    name: str = Field(
        None,
        description="The name of the expander"
    )
    pins: GpioExpander74HCT595Pins = Field(
        description="Definition of the pins used to send the signals to the "
        "shift register."
    )
    ports: list[ExpanderPort] = Field(
        [],
        min_items=1,
        description="Definition of ports (74HCT595 chips) in the chain."
    )
    CODE: ClassVar[int] = 0x01
    

    def create_from_config(self, soc, watchdog, index):
        
        from litex.build.generic_platform import Subsignal, Pins, IOStandard

        # Add the pins to the FPGA and select them
        soc.platform.add_extension([
            ("gpio_expander", index,
                Subsignal("serial_data", Pins(self.pins.serial_data), IOStandard(self.pins.io_standard)),
                Subsignal("clock", Pins(self.pins.clock), IOStandard(self.pins.io_standard)),
                Subsignal("latch", Pins(self.pins.latch), IOStandard(self.pins.io_standard)),
                Subsignal("clear", Pins(self.pins.clear), IOStandard(self.pins.io_standard)),
            )
        ])
        pads = soc.platform.request('gpio_expander', index)

        # Create the sub-module
        gpio_expander = GpioExpander74HCT595Module(
            len(self.ports)*8,
            pads,
        )
        soc.submodules += gpio_expander

        # Connect the module
        soc.comb += [
            gpio_expander.enable.eq(watchdog.ok_out),
            gpio_expander.data.eq(getattr(soc.MMIO_inst, f"gpio_expander_out_{index}").storage[:len(self.ports)*8]),
        ]
        


    def add_mmio_write_registers(self, mmio, index):
        """
        Adds the storage registers to the MMIO.
        NOTE: Storage registers are meant to be written by LinuxCNC and contain
        the flags and configuration for the module.
        """
        # Deferred imports to prevent importing Litex while installing the driver
        from litex.soc.interconnect.csr import CSRStorage
        setattr(
            mmio,
            f"gpio_expander_out_{index}",
            CSRStorage(
                size=int(math.ceil(len(self.ports)/4))*32,
                name=f"gpio_expander_out_{index}",
                description='The bits which should be written to the shift register.',
            )
        )

    def add_mmio_read_registers(self, mmio, index):
        """This function adds the read registers for the port expander. The 74HCT595 only has
        outputs and thus does not have any read registers
        """
        return
    
    @property
    def config_size(self):
        # The configuration of the port expander consist of a single byte. This byte
        # contains the number of GPIO on this expander. The amount of GPIO supported
        # is limited by the software to 128 maximum (or 16 74HCT595 chips on a single)
        # chain.
        return 1
    
    def get_config(self):
        # The configuration of the port expander consists of the pins on this expander.
        # The number of pins are calculated as the number of ports (chips) multiplied
        # by the pins on eacht port (default: 8)
        return len(self.ports) * 8


class GpioExpander74HCT165(GpioExpanderBase):
    ...

    def add_mmio_write_registers(self, mmio):
        # Deferred imports to prevent importing Litex while installing the driver
        from litexcnc.firmware.modules.gpio import GPIO_Module
        GPIO_Module.add_mmio_write_registers(mmio, self)

    def add_mmio_read_registers(self, mmio):
        # Deferred imports to prevent importing Litex while installing the driver
        ...


GpioExpanders = Union[GpioExpander74HCT595Config, GpioExpander74HCT165]


class GpioExpander_ModuleConfig(ModuleBaseModel):
    """
    Module describing the GPIO module
    """
    module_type: Literal['gpio_expander'] = 'gpio_expander'
    module_id: ClassVar[int] = 0x00657870  # The string ` exp` in hex, must be equal to litexcnc_gpio_expander.h
    driver_files: ClassVar[List[str]] = [
        os.path.dirname(__file__) + '/../../driver/modules/litexcnc_gpio_expander.c',
        os.path.dirname(__file__) + '/../../driver/modules/litexcnc_gpio_expander.h'
    ]
    instances: List[GpioExpanders] = Field(
        [],
        item_type=GpioExpanders,
        unique_items=True,
    )

    def create_from_config(self, soc, watchdog):
        for index, instance in enumerate(self.instances):
            instance.create_from_config(soc, watchdog, index)
    
    def add_mmio_config_registers(self, mmio):
        ...

    def add_mmio_write_registers(self, mmio):
        for index, instance in enumerate(self.instances):
            instance.add_mmio_write_registers(mmio, index)

    def add_mmio_read_registers(self, mmio):
        for index, instance in enumerate(self.instances):
            instance.add_mmio_read_registers(mmio, index)
            
    @property
    def config_size(self):
        """Calculates the number DWORDS required to store the data of the port expanders. Each
        port expander contains a minimum 2 bytes of configuration data:
        - 1st byte: indentification and length of the config:
            [0-4]: indentification
            [5-8]: number of bytes of configuration
        - 2nd byte and further: the configuration of the port expander

        The number of DWORDS is finally converted in number of bytes.
        """
        num_bytes = 1  # First byte is for the number of instances of port expanders
        for instance in self.instances:
            num_bytes += instance.config_size
        # Align on DWORD boundary
        return math.ceil(num_bytes / 4) * 4

    def store_config(self, mmio):
        # Deferred imports to prevent importing Litex while installing the driver
        from litex.soc.interconnect.csr import CSRStatus
        # Number of expanders
        config = len(self.instances) << ((self.config_size - 1) * 8)
        # Process the config of all instances
        shift = self.config_size - 1
        for instance in self.instances:
            shift -= 1
            instance_config = (instance.CODE << 4) + instance.config_size
            config += instance_config << (shift *8)
            shift -= instance.config_size
            config += instance.get_config() << (shift *8)
        # Create the config
        mmio.gpio_expander_config_data =  CSRStatus(
            size=self.config_size*8,
            reset=config,
            description=f"The config of the GPIO Expander module."
        )
