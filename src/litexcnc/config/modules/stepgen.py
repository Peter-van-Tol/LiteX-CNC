# Imports for creating a json-definition
import os
try:
    from typing import ClassVar, List, Literal, Union
except ImportError:
    # Imports for Python <3.8
    from typing import ClassVar, List, Union
    from typing_extensions import Literal

# Imports for the configuration
from pydantic import BaseModel, Field

# Import of the basemodel, required to register this module
from . import ModuleBaseModel, ModuleInstanceBaseModel


class StepGenPinoutStepDirBaseConfig(ModuleInstanceBaseModel):
    ...
    
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

    def convert_to_signal(self):
        """
        Creates the pins for this layout.
        """
        # Deferred imports to prevent importing Litex while installing the driver
        from litex.build.generic_platform import IOStandard, Pins, Subsignal
        return (
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

    def convert_to_signal(self):
        """
        Creates the pins for this layout.
        """
        # Deferred imports to prevent importing Litex while installing the driver
        from litex.build.generic_platform import IOStandard, Pins, Subsignal
        return (
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
        return 4

    def store_config(self, mmio):
        # Deferred imports to prevent importing Litex while installing the driver
        from litex.soc.interconnect.csr import CSRStatus
        mmio.stepgen_config_data =  CSRStatus(
            size=self.config_size*8,
            reset=len(self.instances),
            description=f"The config of the Stepgen module."
        )

