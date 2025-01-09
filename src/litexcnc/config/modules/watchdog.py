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


class WatchdogFunctionBaseConfig(BaseModel):
    ...


class WatchdogFunctionEnableConfig(WatchdogFunctionBaseConfig):
    """Function which turns on an enable signal when the Watchdog is enabled and happy.
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


class WatchdogFunctionHeartBeatConfig(WatchdogFunctionBaseConfig):
    """Function which gives a PWM signal on a pin when the Watchdog is enabled and happy. 
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
    pwm_frequency: float = Field(
        1000,
        description="The PWM frequency of the signal. Default value is 1 kHz."
    )
    waveform: Union[Literal["heartbeat", "sinus"], List[float]] = Field(
        "heartbeat",
        description="The waveform to be put on the pin. The available built-in waveforms are "
        "`heartbeat` and `sinus`. Alternatively, a list of floats between 0 and 1 can be "
        "given to create custom patterns."
    )
    speed: float = Field(
        1,
        description="The speed of the waveform in Hz. A value of 1 Hz means that the "
        "waveform is repeated every second."
    )
    io_standard: str = Field(
        "LVCMOS33",
        description="The IO Standard (voltage) to use for the pin."
    )

    def create_from_config(self, soc, watchdog, config: 'WatchdogFunctionHeartBeatConfig'):
        # Deferred imports to prevent importing Litex while installing the driver
        from litexcnc.firmware.watchdog import WatchDogHeartBeatFunction
        return WatchDogHeartBeatFunction.create_function_from_config(soc, watchdog, config)
    

class WatchdogEStop(BaseModel):
    pin: str = Field(
        description="The pin on the FPGA-card to which te EStop will be connected."
    )
    trigger: bool = Field(
        description="Sets the level which triggers the E-Stop. Can be either True for "
        "a HIGH signal which triggers an E-Stop or False for a low signal which triggers "
        "an E-Stop. It is recommended to use a normally closed switch contact when wiring "
        "your Estop as this makes the Estop failsafe. If the wire to the switch breaks it "
        "will Estop the machine until it is fixed ensuring safe operation. When using "
        "normally closed E-Stop, the value of the trigger should be set False."
    )
    name: str = Field(
        None,
        description="The name of the pin as used in LinuxCNC HAL-file (optional). "
        "When not specified, the standard pin litexcnc.watchdog.estop.xx.triggered "
        "will be created."
    )
    io_standard: str = Field(
        "LVCMOS33",
        description="The IO Standard (voltage) to use for the pin."
    )


class WatchdogModuleConfig(ModuleInstanceBaseModel):
    """
    Model describing a pin of the GPIO.
    """
    functions: List[Union[WatchdogFunctionEnableConfig, WatchdogFunctionHeartBeatConfig]] = Field(
        [],
        description="List with functions which are added to the Watchdog"
    )
    estop: List[WatchdogEStop] = Field(
        [],
        description="List will all E-Stops connected to the Watchdog."
    ) 
    
    
    def add_mmio_config_registers(self, mmio):
        # The Watchdog does not require any config, so this function is
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
    
    @property
    def config_size(self):
        return 4

    def store_config(self, mmio):
        # Deferred imports to prevent importing Litex while installing the driver
        from litex.soc.interconnect.csr import CSRStatus
        mmio.watchdog_config_data = CSRStatus(
            size=self.config_size*8,
            reset=len(self.estop),
            description=f"The config of the Watchdog module."
        )
