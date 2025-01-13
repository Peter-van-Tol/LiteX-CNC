# Imports for creating a json-definition
import math
import os
try:
    from typing import ClassVar, List, Literal, Optional, Union
except ImportError:
    # Imports for Python <3.8
    from typing import ClassVar, List, Union
    from typing_extensions import Literal

# Imports for the configuration
from pydantic import BaseModel, Field, validator

# Import of the basemodel, required to register this module
from . import ModuleBaseModel, ModuleInstanceBaseModel


class StepGenPinoutStepDirBaseConfig(ModuleInstanceBaseModel):
    index_pin: Optional[str] = Field(
        None,
        description="The pin on the FPGA-card for the index signal for the stepgen."
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
        if self.index_pin:
            return (
                Subsignal("index", Pins(self.index_pin), IOStandard(self.io_standard)),
            )
        return tuple()


    
class StepGenPinoutStepDirConfig(StepGenPinoutStepDirBaseConfig):
    stepgen_type: Literal['step_dir']
    step_pin: str = Field(
        description="The pin on the FPGA-card for the step signal."
    )
    dir_pin: str = Field(
        None,
        description="The pin on the FPGA-card for the dir signal."
    )
    io_standard: str = Field(
        "LVCMOS33",
        description="The IO Standard (voltage) to use for the pins."
    )

    @validator("step_pin", "dir_pin")
    def user_button_not_allowed(cls, v: str):
        if v.startswith("user_btn"):
            raise ValueError("The user button is no valid pin for Stepgen.")
        return v

    def convert_to_signal(self):
        """
        Creates the pins for this layout.
        """
        # Deferred imports to prevent importing Litex while installing the driver
        from litex.build.generic_platform import IOStandard, Pins, Subsignal
        return super().convert_to_signal() + (
            Subsignal("step", Pins(self.step_pin), IOStandard(self.io_standard)),
            Subsignal("dir", Pins(self.dir_pin), IOStandard(self.io_standard))
        )

    def create_pads(self, generator, pads):
        """Links the step and dir pins to the pads. This one is in a
        separate function, so the differential version only this function
        has to be overridden."""
        # Deferred imports to prevent importing Litex while installing the driver
        from litex.build.generic_platform import Record
        # Require to test working with Verilog, basically creates extra signals not
        # connected to any pads.
        if pads is None:
            pads = Record([("step", 1), ("dir", 1)])

        generator.comb += [
            pads.step.eq(generator.step),
            pads.dir.eq(generator.dir),
        ]


class StepGenPinoutStepDirDifferentialConfig(StepGenPinoutStepDirBaseConfig):
    stepgen_type: Literal['step_dir_differential']
    step_pos_pin: str = Field(
        description="The pin on the FPGA-card for the step+ signal."
    )
    step_neg_pin: str = Field(
        description="The pin on the FPGA-card for the step- signal."
    )
    dir_pos_pin: str = Field(
        None,
        description="The pin on the FPGA-card for the dir+ signal."
    )
    dir_neg_pin: str = Field(
        None,
        description="The pin on the FPGA-card for the dir- signal."
    )
    io_standard: str = Field(
        "LVCMOS33",
        description="The IO Standard (voltage) to use for the pins."
    )
    
    @validator("step_pos_pin", "step_neg_pin", "dir_pos_pin", "dir_neg_pin")
    def user_button_not_allowed(cls, v: str):
        if v.startswith("user_btn"):
            raise ValueError("The user button is no valid pin for Stepgen.")
        return v

    def convert_to_signal(self):
        """
        Creates the pins for this layout.
        """
        # Deferred imports to prevent importing Litex while installing the driver
        from litex.build.generic_platform import IOStandard, Pins, Subsignal
        return super().convert_to_signal() + (
            Subsignal("step_pos", Pins(self.step_pos_pin), IOStandard(self.io_standard)),
            Subsignal("step_neg", Pins(self.step_neg_pin), IOStandard(self.io_standard)),
            Subsignal("dir_pos", Pins(self.dir_pos_pin), IOStandard(self.io_standard)),
            Subsignal("dir_neg", Pins(self.dir_neg_pin), IOStandard(self.io_standard)),
        )

    def create_pads(self, generator, pads):
        """Links the step and dir pins to the pads. This one is in a
        separate function, so the differential version only this function
        has to be overridden."""
        # Deferred imports to prevent importing Litex while installing the driver
        from litex.build.generic_platform import Record
        # Require to test working with Verilog, basically creates extra signals not
        # connected to any pads.
        if pads is None:
            pads = Record([
                ("step_pos", 1),
                ("step_neg", 1), 
                ("dir_pos", 1),
                ("dir_neg", 1)
            ])
        # Connect pads with the pins
        generator.sync += [ # AANGEPAST
            pads.step_pos.eq(generator.step),
            pads.step_neg.eq(~generator.step),
            pads.dir_pos.eq(generator.dir),
            pads.dir_neg.eq(~generator.dir),
        ]

class StepgenInstanceConfig(BaseModel):

    name: str = Field(
        None,
        description="The name of the stepgen as used in LinuxCNC HAL-file (optional). "
    )
    pins: Union[
            StepGenPinoutStepDirConfig, 
            StepGenPinoutStepDirDifferentialConfig] = Field(
        ...,
        description="The configuration of the stepper type and pin-out."
    )
    max_frequency: int = Field(
        400e3,
        description="The guaranteed maximum frequency the stepgen can generate in Hz. "
        "The actual value can be larger then this value, as this is dependent on "
        "the clock-frequency and scaling. Choosing a smaller value, close to the "
        "limits of your drivers, gives a higher resolution in the velocity. Default "
        "value is 400,000 Hz (400 kHz)."
    )
    soft_stop: bool = Field(
        False,
        description="When False, the stepgen will directly stop when the stepgen is "
        "disabled. When True, the stepgen will stop the machine with respect to the "
        "acceleration limits and then be disabled. Default value: False."
    )
    hal_pins: ClassVar[List[str]] = [
        'counts',
        'position-feedback',
        'position-prediction',
        'velocity-feedback',
        'velocity-prediction',
        'enable',
        'velocity-mode',
        'position-cmd',
        'velocity-cmd',
        'acceleration-cmd',
        'debug'
    ]
    hal_params: ClassVar[List[str]] = [
        'frequency',
        'max-acceleration',
        'max-velocity',
        'position-scale',
        'steplen',
        'stepspace',
        'dir-setup-time',
        'dir-hold-time',
    ]

    def calculate_shift(self, clock_frequency):
        """Calculates the shift required to get the desired maximum frequency
        for the stepgen instance. The shift is maximum 4 bits, so the maximum
        frequency division is 16 times the clock frequency. 

        Args:
            clock_frequency (int): The clock frequency of the FPGA.

        Returns:
            int: The shift required to get the desired maximum frequency.
        """
        return (int(math.log2(clock_frequency / self.max_frequency)) - 1) & 0x0F

class StepgenModuleConfig(ModuleBaseModel):
    """
    Module describing the PWM module
    """
    module_type: Literal['stepgen'] = 'stepgen'
    module_id: ClassVar[int] = 0x73746570  # The string `step` in hex, must be equal to litexcnc_stepgen.h
    driver_files: ClassVar[List[str]] = [
        os.path.dirname(__file__) + '/../../driver/modules/litexcnc_stepgen.c',
        os.path.dirname(__file__) + '/../../driver/modules/litexcnc_stepgen.h'
    ]
    instances: List[StepgenInstanceConfig] = Field(
        [],
        item_type=StepgenInstanceConfig,
        unique_items=True
    )

    def create_from_config(self, soc, watchdog):
        # Deferred imports to prevent importing Litex while installing the driver
        from litexcnc.firmware.modules.stepgen import StepgenModule
        StepgenModule.create_from_config(soc, watchdog, self)
    
    def add_mmio_config_registers(self, mmio):
        # Deferred imports to prevent importing Litex while installing the driver
        from litexcnc.firmware.modules.stepgen import StepgenModule
        StepgenModule.add_mmio_config_registers(mmio, self)

    def add_mmio_write_registers(self, mmio):
        # Deferred imports to prevent importing Litex while installing the driver
        from litexcnc.firmware.modules.stepgen import StepgenModule
        StepgenModule.add_mmio_write_registers(mmio, self)

    def add_mmio_read_registers(self, mmio):
        # Deferred imports to prevent importing Litex while installing the driver
        from litexcnc.firmware.modules.stepgen import StepgenModule
        StepgenModule.add_mmio_read_registers(mmio, self)

    @property
    def config_size(self):
        """Calculates the number DWORDS required to store the shift data
        of the stepgen instances. The first byte is the number of instances,
        followed by the shifts required for each instance to get the desired
        maximum frequency.
        """
        return math.ceil((1 + len(self.instances)) / 4) * 4

    def store_config(self, mmio):
        # Deferred imports to prevent importing Litex while installing the driver
        from litex.soc.interconnect.csr import CSRStatus
        # - store the number of instances
        config = len(self.instances) << ((self.config_size - 1) * 8)
        # - calculate and store the shift for each instance plus the other settings
        clock_frequency = mmio.clock_frequency.status.reset.value
        for index, instance in enumerate(self.instances):
            config_byte = instance.calculate_shift(clock_frequency)
            config_byte += (1 if instance.pins.index_pin is not None else 0) << 7
            config += config_byte << ((self.config_size - 2 - index) * 8)
        # Store the config
        mmio.stepgen_config_data =  CSRStatus(
            size=self.config_size*8,
            reset=config,
            description=f"The config of the Stepgen module."
        )
