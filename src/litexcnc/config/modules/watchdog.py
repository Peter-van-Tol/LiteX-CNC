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


class WatchdogModuleConfig(ModuleInstanceBaseModel):
    """
    Model describing a pin of the GPIO.
    """
    pin: str = Field(
        None,
        description="The pin on the FPGA-card."
    )
    io_standard: str = Field(
        "LVCMOS33",
        description="The IO Standard (voltage) to use for the pin."
    )
    
    def add_mmio_config_registers(self, mmio):
        # The Encoder does not require any config, so this function is
        # not implemented.
        return

    def add_mmio_write_registers(self, mmio):
        # Deferred imports to prevent importing Litex while installing the driver
        from litexcnc.firmware.watchdog import WatchDogModule
        WatchDogModule.add_mmio_write_registers(mmio, self)

    def add_mmio_read_registers(self, mmio):
        # Deferred imports to prevent importing Litex while installing the driver
        from litexcnc.firmware.watchdog import WatchDogModule
        WatchDogModule.add_mmio_read_registers(mmio, self)

    def create_from_config(self, soc):
        # Deferred imports to prevent importing Litex while installing the driver
        from litexcnc.firmware.watchdog import WatchDogModule
        return WatchDogModule.create_from_config(soc, self)
