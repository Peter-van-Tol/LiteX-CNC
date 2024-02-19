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
from pydantic import BaseModel, Field

# Import of the basemodel, required to register this module
from . import ModuleBaseModel, ModuleInstanceBaseModel



class ShiftOutInstanceConfig(ModuleInstanceBaseModel):
    pin_clock: str = Field(
        description="The pin on the FPGA-card."
    )
    pin_data: str = Field(
        description="The pin on the FPGA-card."
    )
    pin_latch: str = Field(
        description="The pin on the FPGA-card."
    )
    data_width: int = Field(
        ...,
        description="The width of the data to be shifted out. Maximum is 256 bits (i.e. 16 daisychained 74HC595)."
    )
    frequency: int = Field(
        1_000_000,
        description="The frequency of the clock signal in Hz. Default value is 1 MHz."
    )
    io_standard: str = Field(
        "LVCMOS33",
        description="The IO Standard (voltage) to use for the pin."
    )
    hal_pins: ClassVar[List[str]] = [
        'out',
    ]
    hal_params: ClassVar[List[str]] = [
        'invert_output',
    ]


class ShiftOutModuleConfig(ModuleBaseModel):
    """
    Module describing the PWM module
    """
    module_type: Literal['shift_out'] = 'shift_out'
    module_id: ClassVar[int] = 0x656e635f  # The string `enc_` in hex, must be equal to litexcnc_pwm.h
    driver_files: ClassVar[List[str]] = [
        os.path.dirname(__file__) + '/../../driver/modules/litexcnc_shift_out.c',
        os.path.dirname(__file__) + '/../../driver/modules/litexcnc_shift_out.h'
    ]
    instances: List[ShiftOutInstanceConfig] = Field(
        [],
        item_type=ShiftOutInstanceConfig,
        unique_items=True
    )

    def create_from_config(self, soc, watchdog):
        # Deferred imports to prevent importing Litex while installing the driver
        from litexcnc.firmware.modules.shift_out import ShiftOutModule
        ShiftOutModule.create_from_config(soc, watchdog, self)
    
    def add_mmio_config_registers(self, mmio):
        # The shift_out does not require any config, so this function is
        # not implemented.
        return

    def add_mmio_write_registers(self, mmio):
        # Deferred imports to prevent importing Litex while installing the driver
        from litexcnc.firmware.modules.shift_out import ShiftOutModule
        ShiftOutModule.add_mmio_write_registers(mmio, self)

    def add_mmio_read_registers(self, mmio):
        # The shift_out does not require any read registers, so this function is
        # not implemented.
        return

    @property
    def config_size(self):
        """Calculates the number DWORDS required to store the direction data
        of the shift_out register. The first byte is used for the number of 
        instances. The next bytes are the data length of each individual instance,
        which is limited to 256 bits (i.e. 16 daisychained 74HC595). The data
        width therefore always fits in a single byte.

        The number of bytes is rounded to the nearest DWORDS boundary. The number 
        of DWORDS is finally converted in number of bytes.
        """
        return math.ceil((1 + len(self.instances))/4) * 4
 
    def store_config(self, mmio):
        # Deferred imports to prevent importing Litex while installing the driver
        from litex.soc.interconnect.csr import CSRStatus
        # Calculate the config bits
        config = len(self.instances) << (self.config_size * 8 - 8)
        for index, instance in enumerate(self.instances):
            config += ((instance.data_width - 1) << index)
        # Create the register
        mmio.shift_out_config_data =  CSRStatus(
            size=self.config_size*8,
            reset=config,
            description=f"The config of the shift out module."
        )
