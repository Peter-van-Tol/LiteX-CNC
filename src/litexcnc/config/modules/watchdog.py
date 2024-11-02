# Default imports
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


class WatchdogFunctionEnableConfig(BaseModel):
    """Function which 
    """
    function_type: Literal['enable'] = 'enable'
    pin: str = Field(
        None,
        description="The pin on which the enable signal will be put. By default the signal "
        "is HIGH when the watchdog is happy, and LOW when the watchdog has bitten. These "
        "signals can be inverted by using the `invert_ouput` param."
    )
    invert_output: bool = Field(
        False,
        description="When set to True, the output signal will be inverted. Default value is "
        "False."
    )
    io_standard: str = Field(
        "LVCMOS33",
        description="The IO Standard (voltage) to use for the pin."
    )

    def create_from_config(self, soc, watchdog, config: 'WatchdogFunctionEnableConfig'):
        # Deferred imports to prevent importing Litex while installing the driver
        from litexcnc.firmware.watchdog import WatchDogEnableFunction
        return WatchDogEnableFunction.create_function_from_config(soc, watchdog, config)


class WatchdogFunctionHeartBeatConfig(BaseModel):
    """Function which 
    """
    function_type: Literal['heartbeat'] = 'heartbeat'
    pin: str = Field(
        None,
        description="The pin on which the heart beat signal (PWM) will be put."
    )
    invert_output: bool = Field(
        False,
        description="When set to True, the output signal will be inverted. Default value is "
        "False."
    )
    io_standard: str = Field(
        "LVCMOS33",
        description="The IO Standard (voltage) to use for the pin."
    )

    def create_from_config(self, soc, watchdog, config: 'WatchdogFunctionHeartBeatConfig'):
        # Deferred imports to prevent importing Litex while installing the driver
        from litexcnc.firmware.watchdog import WatchDogHeartBeatFunction
        return WatchDogHeartBeatFunction.create_function_from_config(soc, watchdog, config)


class WatchdogModuleConfig(ModuleInstanceBaseModel):
    """
    Model describing a pin of the GPIO.
    """
    functions: List[Union[WatchdogFunctionEnableConfig, WatchdogFunctionHeartBeatConfig]] = Field(
        [],
        description="List with functions which are added to the Watchdog"
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
