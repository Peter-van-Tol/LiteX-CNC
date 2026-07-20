# Default imports
import os
try:
    from typing import ClassVar, List, Literal
except ImportError:
    # Imports for Python <3.8
    from typing import ClassVar, List
    from typing_extensions import Literal

# Imports for the configuration
from pydantic import Field

# Import of the basemodel, required to register this module
from . import ModuleBaseModel, ModuleInstanceBaseModel


class PWM_Instance(ModuleInstanceBaseModel):
    """
    Model describing a pin of the GPIO.
    """
    pin: str = Field(
        description="The pin on the FPGA-card."
    )
    name: str = Field(
        None,
        description="The name of the pin as used in LinuxCNC HAL-file (optional). "
        "When not specified, the standard pins like litexcnc.pwm.x will be created."
    )
    io_standard: str = Field(
        "LVCMOS33",
        description="The IO Standard (voltage) to use for the pin."
    )
    hal_pins: ClassVar[List[str]] = [
        'curr_dc',
        'curr_period',
        'curr_pwm_freq',
        'curr_width',
        'dither_pwm',
        'enable',
        'max_dc',
        'min_dc',
        'offset',
        'pwm_freq',
        'scale',
        'value'
    ]
    hal_pins: ClassVar[List[str]] = []


class PWM_ModuleConfig(ModuleBaseModel):
    """
    Module describing the PWM module
    """
    module_type: Literal['pwm'] = 'pwm'
    module_id: ClassVar[int] = 0x70776d5f  # The string `pwm_` in hex, must be equal to litexcnc_pwm.h
    driver_files: ClassVar[List[str]] = [
        os.path.dirname(__file__) + '/../../driver/modules/litexcnc_pwm.c',
        os.path.dirname(__file__) + '/../../driver/modules/litexcnc_pwm.h'
    ]
    instances: List[PWM_Instance] = Field(
        [],
        item_type=PWM_Instance,
        unique_items=True
    )

    def create_from_config(self, soc, watchdog):
        # Deferred imports to prevent importing Litex while installing the driver
        from litexcnc.firmware.modules.pwm import PwmPdmModule
        PwmPdmModule.create_from_config(soc, watchdog, self)
    
    def add_mmio_config_registers(self, mmio):
        # The PWM does not require any config, so this function is
        # not implemented.
        return

    def add_mmio_write_registers(self, mmio):
        # Deferred imports to prevent importing Litex while installing the driver
        from litexcnc.firmware.modules.pwm import PwmPdmModule
        PwmPdmModule.add_mmio_write_registers(mmio, self)

    def add_mmio_read_registers(self, mmio):
        # The PWM does not require any read, so this function is
        # not implemented.
        return

    @property
    def config_size(self):
        return 4

    def store_config(self, mmio):
        # Deferred imports to prevent importing Litex while installing the driver
        from litex.soc.interconnect.csr import CSRStatus
        mmio.pwm_config_data =  CSRStatus(
            size=self.config_size*8,
            reset=len(self.instances),
            description=f"The config of the GPIO module."
        )
