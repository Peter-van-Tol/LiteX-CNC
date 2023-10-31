import os
try:
    from typing import ClassVar, List, Literal
except ImportError:
    # Imports for Python <3.8
    from typing import ClassVar, List
    from typing_extensions import Literal
import warnings

# Imports for creating a json-definition
from pydantic import  Field, root_validator

# Import of the basemodel, required to register this module
from . import ModuleBaseModel, ModuleInstanceBaseModel


class EncoderInstanceConfig(ModuleInstanceBaseModel):
    """
    Configuration for hardware counting of quadrature encoder signals.
    """
    name: str = Field(
        None,
        description="The name of the encoder as used in LinuxCNC HAL-file (optional). "
    )
    pin_A: str = Field(
        description="The pin on the FPGA-card for Encoder A-signal."
    )
    pin_B: str = Field(
        description="The pin on the FPGA-card for Encoder B-signal."
    )
    pin_Z: str = Field(
        None,
        description="The pin on the FPGA-card for Encoder Z-signal. This pin is optional, "
    "when not set the Z-pulse register on the FPGA will not be set, but it will be created. "
    "In the driver the phase-Z bit will not be exported and the function `index-enable` will "
    "be disabled when there is no Z-signal."
    )
    min_value: int = Field(
        None,
        description="The minimum value for the encoder. Extra pulses will not cause the counter"
        "to decrease beyond this value. This value is inclusive. When the value is not defined, the "
        "minimum value is unlimited. The minimum value should be smaller then the maximum "
        "value if both are defined."
    )
    max_value: int = Field(
        None,
        description="The maximum value for the encoder. Extra pulses will not cause the counter"
        "to increase beyond this value. This value is inclusive. When the value is not defined, the "
        "maximum value is unlimited. The maximum value should be larger then the minimum "
        "value if both are defined."
    )
    reset_value: int = Field(
        0,
        description="The value to which the counter will be resetted. This is also the initial value "
        "at which the counter is instantiated. The reset value should be between the minimum value "
        "and maximum value if these are defined. Default value: 0."
        "NOTE: when the encoder has X4 set to False, the value reported in HAL will be this value "
        "divided by 4."
    )
    io_standard: str = Field(
        "LVCMOS33",
        description="The IO Standard (voltage) to use for the pins."
    )
    hal_pins: ClassVar[List[str]] = [
        "counts",
        "reset",
        "index-enable",
        "index-pulse",
        "position",
        "velocity",
        "velocity-rpm",
        "overflow-occurred"
    ]
    hal_params: ClassVar[List[str]] = [
        "position-scale",
        "x4-mode"
    ]

    @root_validator(skip_on_failure=True)
    def check_min_max_reset_value(cls, values):
        """
        Checks whether the min value is smaller then max value and that the
        reset value is larger then the minimum value, but smaller then the
        maximum value.
        """
        reset_value = values.get('reset_value')
        min_value = values.get('min_value', None)
        # Check the reset value relative to the minimum value if the latter is defined
        if min_value is not None:
            if reset_value < min_value:
                raise ValueError('Reset value should be larger then or equal to the minimum value.')
        max_value = values.get('max_value', None)
        # Check the reset value relative to the maximum value if the latter is defined
        if max_value is not None:
            if reset_value > max_value:
                raise ValueError('Reset value should be smaller then or equal to the minimum value.')
        # If both minimum and maximum values are defined, check whether they are in the
        # correct order. Technically it is possible to have minimum and maximum value
        # equal to each other, however this will result in a counter which is not working
        # (fixed at one value). In this case we warn the user that the counter won't work.
        if min_value is not None and max_value is not None:
            if max_value < min_value:
                raise ValueError('Minimum value should be smaller then the maximum value.')
            if max_value == min_value:
                warnings.warn('Minimum and maximum value are equal! The counter will not work '
                'because its value is fixed. It is recommended to change the values.')
        # Everything OK, pass on the values
        return values


class EncoderModuleConfig(ModuleBaseModel):
    """
    Module describing the PWM module
    """
    module_type: Literal['encoder'] = 'encoder'
    module_id: ClassVar[int] = 0x656e635f  # The string `enc_` in hex, must be equal to litexcnc_pwm.h
    driver_files: ClassVar[List[str]] = [
        os.path.dirname(__file__) + '/../../driver/modules/litexcnc_encoder.c',
        os.path.dirname(__file__) + '/../../driver/modules/litexcnc_encoder.h'
    ]
    instances: List[EncoderInstanceConfig] = Field(
        [],
        item_type=EncoderInstanceConfig,
        unique_items=True
    )

    def create_from_config(self, soc, watchdog):
        # Deferred imports to prevent importing Litex while installing the driver
        from litexcnc.firmware.modules.encoder import EncoderModule
        EncoderModule.create_from_config(soc, watchdog, self)
    
    def add_mmio_config_registers(self, mmio):
        # The Encoder does not require any config, so this function is
        # not implemented.
        return

    def add_mmio_write_registers(self, mmio):
        # Deferred imports to prevent importing Litex while installing the driver
        from litexcnc.firmware.modules.encoder import EncoderModule
        EncoderModule.add_mmio_write_registers(mmio, self)

    def add_mmio_read_registers(self, mmio):
        # Deferred imports to prevent importing Litex while installing the driver
        from litexcnc.firmware.modules.encoder import EncoderModule
        EncoderModule.add_mmio_read_registers(mmio, self)

    @property
    def config_size(self):
        return 4

    def store_config(self, mmio):
        # Deferred imports to prevent importing Litex while installing the driver
        from litex.soc.interconnect.csr import CSRStatus
        mmio.encoder_config_data =  CSRStatus(
            size=self.config_size*8,
            reset=len(self.instances),
            description=f"The config of the GPIO module."
        )
