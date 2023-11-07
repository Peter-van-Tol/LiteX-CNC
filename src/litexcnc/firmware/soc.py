# Imports for creating the config
try:
    from typing import Any, List, Union, get_args
except ImportError:
    from typing import Any, List, Union
    from typing_extensions import get_args
from pydantic import BaseModel, Field, validator

# Local imports
from litexcnc.config.modules import ModuleBaseModel, module_registry
from litexcnc.config.connections import EtherboneConnection, SPIboneConnection

# Registry which holds all the sub-classes of modules
board_registry = {}


class LitexCNC_Firmware(BaseModel):
    board_name: str = Field(
        ...,
        description="The name of the board, required for identification purposes.",
        max_length=16
    )
    connection: Union[EtherboneConnection, SPIboneConnection, List[Union[EtherboneConnection, SPIboneConnection]]] = Field(
        ...,
        description="The configuration of the connection to the board."
    )
    clock_frequency: int = Field(
      50e6  
    )
    modules: List[ModuleBaseModel] = Field(
        [],
        item_type=ModuleBaseModel,
        unique_items=True
    )
    
    class Config:
        extra = "allow"

    def __new__(cls,  *args, **kwargs):
        # Deferred import to prevent circular imports. We need to import the boards here
        # to trigger the import mechanism and make sure that the board_registry is filled.
        from litexcnc.config import boards
        if cls is not LitexCNC_Firmware:
            return super().__new__(cls)
        if 'board_type' not in kwargs.keys():
            raise ValueError("Field `board_type` is requierd.")
        if kwargs['board_type'] not in board_registry.keys():
            raise TypeError(f"Unknown board type `{kwargs['board_type']}`. Supported board types: {' '.join(board_registry.keys())}")
        subclass = board_registry[kwargs['board_type']]
        return subclass.__new__(subclass,  *args, **kwargs)

    def __init__(self, **kwargs):
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
        super().__init__(**kwargs)

    def __init_subclass__(cls, **kwargs: Any) -> None:
        """"""
        super().__init_subclass__(**kwargs)
        for board_type in get_args(cls.__fields__['board_type'].type_):
            if board_type != 'abstract':
                board_registry[board_type] = cls

    def _generate_soc(self):
        """
        
        """
        raise NotImplementedError("This function should be implemented by a sub-class")

    @validator('board_name')
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
        from litexcnc.firmware.watchdog import WatchDogModule
        from litexcnc.firmware.mmio import MMIO
        from litexcnc.firmware.connections import add_connection

        soc = self._generate_soc()
        add_connection(soc, self)

        # Create memory mapping for IO
        soc.submodules.MMIO_inst = MMIO(config=self)

        # Create watchdog
        watchdog = WatchDogModule(timeout=soc.MMIO_inst.watchdog_data.storage[:31], with_csr=False)
        soc.submodules += watchdog
        soc.sync+=[
            # Watchdog input (fixed values)
            watchdog.enable.eq(soc.MMIO_inst.watchdog_data.storage[31]),
            # Watchdog output (status whether the dog has bitten)
            soc.MMIO_inst.watchdog_has_bitten.status.eq(watchdog.has_bitten),
            # self.MMIO_inst.watchdog_has_bitten.we.eq(True)
        ]

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
