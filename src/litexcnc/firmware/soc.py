# Imports for creating the config
try:
    from typing import Any, List, get_args
except ImportError:
    from typing import Any, List
    from typing_extensions import get_args
from pydantic import BaseModel, Field, validator

# Local imports
from litexcnc.config.modules import ModuleBaseModel, module_registry
from litexcnc.config.modules.watchdog import WatchdogModuleConfig
from litexcnc.firmware.boards import ConfigBase as Board, board_registry


class LitexCNC_Firmware(BaseModel):
    name: str = Field(
        ...,
        description="The name of the controller, required for identification purposes.",
        max_length=15
    )
    board: Board = Field(
        ...,
        description="Definition of the FPGA"
    )
    watchdog: WatchdogModuleConfig = Field(
        ...,
        description="Configuration of the watchdog."
    )
    modules: List[ModuleBaseModel] = Field(
        [],
        item_type=ModuleBaseModel,
        unique_items=True
    )
    
    class Config:
        extra = "allow"

    def __init__(self, **kwargs):
        # Convert board config and module configs to instances
        kwargs['board'] = board_registry[kwargs['board']['family']](**kwargs['board'])
        kwargs['modules'] = [module_registry[module['module_type']](**module) for module in kwargs['modules']]
        super().__init__(**kwargs)

    @validator('name')
    def ascii_boardname(cls, value):
        assert value.isascii(), 'Must be ASCII string'
        return value

    def generate(self):
        """
        Function which generates the firmware (excluding communication):
        - create MMIO (memory)
        - initialize default modules ``watchdog`` and ``wallclock``
        - initialize any user defined module
        """
        # Deferred imports to prevent importing Litex while installing the driver
        from litexcnc.firmware.mmio import MMIO

        # Generate the SOC (including connection)
        soc = self.board.generate_soc(self.name)

        # Create memory mapping for IO
        soc.submodules.MMIO_inst = MMIO(config=self)

        # Create watchdog
        watchdog = self.watchdog.create_from_config(soc)

        # Create a wall-clock
        soc.sync+=[
            # Increment the wall_clock
            soc.MMIO_inst.wall_clock.status.eq(soc.MMIO_inst.wall_clock.status + 1),
            # self.MMIO_inst.wall_clock.we.eq(True)
        ]

        # Create modules
        for module in self.modules:
            module.create_from_config(soc, watchdog)

        # Return the created SoC 
        return soc
