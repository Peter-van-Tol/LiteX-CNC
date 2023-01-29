from migen import *
from litex.build.generic_platform import *

# Imports for creating the config
from typing import List, Type
from pydantic import BaseModel, Field, validator

# Local imports
from .etherbone import Etherbone, EthPhy
from .watchdog import WatchDogModule
from .mmio import MMIO
from .modules import ModuleBaseModel, module_registry


class LitexCNC_Firmware(BaseModel):
    board_name: str = Field(
        ...,
        description="The name of the board, required for identification purposes.",
        max_length=16
    )
    baseclass: Type = Field(
        ...,
        description=""
    )
    clock_frequency: int = Field(
      50e6  
    )
    ethphy: EthPhy = Field(
        ...
    )
    etherbone: Etherbone = Field(
        ...,
    )
    modules: List[ModuleBaseModel] = Field(
        [],
        item_type=ModuleBaseModel,
        unique_items=True
    )

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
                kwargs['modules'][index] = current_module
        super().__init__(**kwargs)

    @validator('baseclass', pre=True)
    def import_baseclass(cls, value):
        components = value.split('.')
        mod = __import__(components[0])
        for comp in components[1:]:
            mod = getattr(mod, comp)
        return mod

    @validator('board_name')
    def ascii_boardname(cls, value):
        assert value.isascii(), 'Must be ASCII string'
        return value

    def generate(self):
        """
        Function which generates the firmware
        """
        class _LitexCNC_SoC(self.baseclass):

            def __init__(
                    self,
                    config: 'LitexCNC_Firmware',
                    ):
                
                # Configure the top level class
                super().__init__(config)

                # Create memory mapping for IO
                self.submodules.MMIO_inst = MMIO(config=config)

                # Create watchdog
                watchdog = WatchDogModule(timeout=self.MMIO_inst.watchdog_data.storage[:31], with_csr=False)
                self.submodules += watchdog
                self.sync+=[
                    # Watchdog input (fixed values)
                    watchdog.enable.eq(self.MMIO_inst.watchdog_data.storage[31]),
                    # Watchdog output (status whether the dog has bitten)
                    self.MMIO_inst.watchdog_has_bitten.status.eq(watchdog.has_bitten),
                    # self.MMIO_inst.watchdog_has_bitten.we.eq(True)
                ]

                # Create a wall-clock
                self.sync+=[
                    # Increment the wall_clock
                    self.MMIO_inst.wall_clock.status.eq(self.MMIO_inst.wall_clock.status + 1),
                    # self.MMIO_inst.wall_clock.we.eq(True)
                ]

                # Create modules
                for module in config.modules:
                    module.create_from_config(self, watchdog)
                
        return _LitexCNC_SoC(
            config=self)
