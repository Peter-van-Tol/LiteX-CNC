# Default imports
import math
import os
try:
    from typing import ClassVar, List, Literal, Union
except ImportError:
    # Imports for Python <3.8
    from typing import ClassVar, List
    from typing_extensions import Literal, Union

# Imports for the configuration
from pydantic import BaseModel, Field, validator

# Import of the basemodel, required to register this module
from . import ModuleBaseModel, ModuleInstanceBaseModel

OUTPUT_TYPE_ENUM = {
    'single': 0,
    'pwmdirection': 1,
    'updown': 2
}

class PWM_SinglePin(BaseModel):
    """Single output. A single output pin, pwm, whose duty cycle is
    determined by the input value for positive inputs, and which is off (or
    at min-dc) for negative inputs.

    Suitable for single ended circuits.
    """
    output_type: Literal['single'] = 'single'
    pwm: str = Field(
        description="The pin on the FPGA-card on which the PWM is being output."
    )
    io_standard: str = Field(
        "LVCMOS33",
        description="The IO Standard (voltage) to use for the pin."
    )

    def convert_to_signal(self):
        """
        Creates the pins for this layout.
        """
        # Deferred imports to prevent importing Litex while installing the driver
        from litex.build.generic_platform import IOStandard, Pins, Subsignal
        return (
            Subsignal("pwm", Pins(self.pwm), IOStandard(self.io_standard)),
        )
    
    @staticmethod
    def output_pwm(pads, value, direction, invert):
        # Deferred imports to prevent importing Litex while installing the driver
        from migen import If
        return (
            If(
                direction == 0,
                pads.pwm.eq(value ^ invert),
            ).Else(
                pads.pwm.eq(0 ^ invert),
            ), 
        )
    
    @validator("pwm", always=True)
    @classmethod
    def user_button_not_allowed(cls, v: str):
        if v.startswith("user_btn"):
            raise ValueError("The user Button is no valid pin for PWM outputs.")
        return v


class PWM_PWMDirectionPins(BaseModel):
    """
    Two output pins, pwm and dir. The duty cycle on **pwm** varies as a function of the input 
    value. The **dir** is low for positive inputs and high for negative inputs.
    """
    output_type: Literal['pwmdirection'] = 'pwmdirection'
    pwm: str = Field(
        description="The pin on the FPGA-card on which the PWM is outputted."
    )
    direction: str = Field(
        description="he pin on the FPGA-card with the direction signal. This pin is low "
        "for positive inputs and high for negative inputs."
    )
    io_standard: str = Field(
        "LVCMOS33",
        description="The IO Standard (voltage) to use for the pins."
    )

    def convert_to_signal(self):
        """
        Creates the pins for this layout.
        """
        # Deferred imports to prevent importing Litex while installing the driver
        from litex.build.generic_platform import IOStandard, Pins, Subsignal
        return (
            Subsignal("pwm", Pins(self.pwm), IOStandard(self.io_standard)),
            Subsignal("direction", Pins(self.direction), IOStandard(self.io_standard)),
        )

    @staticmethod
    def output_pwm(pads, value, direction, invert):
        return (
            pads.pwm.eq(value ^ invert),
            pads.direction.eq(direction ^ invert),
        )
    
    @validator("pwm", "direction", always=True)
    @classmethod
    def user_button_not_allowed(cls, v: str):
        if v.startswith("user_btn"):
            raise ValueError("The user Button is no valid pin for PWM outputs.")
        return v


class PWM_UpDownPins(BaseModel):
    """Two output pins, up and down. For positive inputs, the PWM/PDM waveform appears on up,
    while down is low. For negative inputs, the waveform appears on down, while up is low. 
    Suitable for driving the two sides of an H-bridge to generate a bipolar output.
    """
    output_type: Literal['updown'] = 'updown'
    up: str = Field(
        description="The pin on the FPGA-card on which the PWM is outputted when the value "
        "is positive."
    )
    down: str = Field(
        description="The pin on the FPGA-card on which the PWM is outputted when the value "
        "is negative."
    )
    io_standard: str = Field(
        "LVCMOS33",
        description="The IO Standard (voltage) to use for the pins."
    )

    def convert_to_signal(self):
        """
        Creates the pins for this layout.
        """
        # Deferred imports to prevent importing Litex while installing the driver
        from litex.build.generic_platform import IOStandard, Pins, Subsignal
        return (
            Subsignal("up", Pins(self.up), IOStandard(self.io_standard)),
            Subsignal("down", Pins(self.down), IOStandard(self.io_standard)),
        )

    @staticmethod
    def output_pwm(pads, value, direction, invert):
        from migen import If
        return (
            If(
                direction == 0,
                pads.up.eq(value ^ invert),
                pads.down.eq(0 ^ invert),
            ).Else(
                pads.up.eq(0 ^ invert),
                pads.down.eq(value ^ invert),
            ),
        )

    @validator("up", "down", always=True)
    @classmethod
    def user_button_not_allowed(cls, v: str):
        if v.startswith("user_btn"):
            raise ValueError("The user Button is no valid pin for PWM outputs.")
        return v


class PWM_Instance(ModuleInstanceBaseModel):
    """
    Model describing a pin of the GPIO.
    """
    pins: Union[PWM_SinglePin, PWM_PWMDirectionPins, PWM_UpDownPins] = Field(
        description="The configuration of the pins on which the PWM is outputted."
    )
    name: str = Field(
        None,
        description="The base-name of the pins and params as used in LinuxCNC HAL-file "
        "(optional). When not specified, the standard pins like litexcnc.pwm.x will be "
        "created."
    )
    hal_pins: ClassVar[List[str]] = [
        'curr-dc',
        'curr-period',
        'curr-pwm-freq',
        'curr-width',
        'direction',
        'dither-pwm',
        'enable',
        'max-dc',
        'min-dc',
        'offset',
        'pwm-freq',
        'scale',
        'value'
    ]
    hal_params: ClassVar[List[str]] = [
        "invert-output",
        "type"
    ]


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
        """
        The config data is packed as follows:
        - first byte: the number of PWM instances
        - the following bytes contain the data on the type of PWM

        The data tightly packed with the config. Each WORD can take 16 instances of PWM, however
        due to the first byte being used for the number of PWM, the first WORD # can only take 12.
        """
        if len(self.instances) <= 12:
            return 4
        return 4 + int(math.ceil(float(len(self.instances-12))/32))*32


    def store_config(self, mmio):
        # Deferred imports to prevent importing Litex while installing the driver
        value = len(self.instances) << ((self.config_size - 1) * 8)
        for index, instance in enumerate(self.instances, start=1):
            value += (OUTPUT_TYPE_ENUM[instance.pins.output_type] << (self.config_size - 1) * 8 - index * 2)
        from litex.soc.interconnect.csr import CSRStatus
        mmio.pwm_config_data =  CSRStatus(
            size=self.config_size*8,
            reset=value,
            description=f"The config of the PWM module."
        )
