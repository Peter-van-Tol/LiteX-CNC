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

# Registry which holds all the sub-classes of modules
board_registry = {}


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
        # TODO: refactor this
        for index in range(len(kwargs['modules'])):
            current_module = kwargs['modules'][index]
            if isinstance(current_module, dict):
                item_module_type = current_module['module_type']
                for name, subclass in module_registry.items():
                    registery_module_type = subclass.__fields__['module_type'].default
                    if item_module_type == registery_module_type:
                        current_module = subclass(**current_module) 
                        break
                else:
                    raise TypeError(f"Unknown module type `{current_module['module_type']}`. Supported module types: {' '.join([module.__fields__['module_type'].default for module in module_registry.values()])}")
                kwargs['modules'][index] = current_module
        
        # Deferred imports, otherwise the registry is not filled yet 
        from litexcnc.firmware.boards import board_registry
        kwargs['board'] = board_registry[kwargs['board']['family']](**kwargs['board'])
        super().__init__(**kwargs)

    def __init_subclass__(cls, **kwargs: Any) -> None:
        """"""
        super().__init_subclass__(**kwargs)
        for board_type in get_args(cls.__fields__['board_type'].type_):
            if board_type != 'abstract':
                board_registry[board_type] = cls

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
