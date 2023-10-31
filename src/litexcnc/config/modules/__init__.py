from functools import partial
from typing import Any, ClassVar, List

import sys
if sys.version_info[:2] >= (3, 8):
    # TODO: Import directly (no need for conditional) when `python_requires = >= 3.8`
    from importlib.metadata import entry_points  # pragma: no cover
else:
    from importlib_metadata import entry_points  # pragma: no cover

# Imports for creating a json-definition
from pydantic import BaseModel

# Registry which holds all the sub-classes of modules
module_registry = {}
GROUP = "litexcnc.modules"

class ModuleInstanceBaseModel(BaseModel):
    """
    Base-class for a definition of an instance of a Module
    """
    pins: ClassVar[List[str]] = []
    params: ClassVar[List[str]] = []
    driver_files: ClassVar[List[str]] = []

class ModuleBaseModel(BaseModel):
    """
    Base-class for a definition of a Module
    """
    def __init_subclass__(cls, **kwargs: Any) -> None:
        super().__init_subclass__(**kwargs)
        module_registry[cls.__name__] = cls

    class Config:
        extra = "allow"

    def create_from_config(self, soc, watchdog):
        raise NotImplementedError("Function must be implemented in subclass")

    def add_mmio_config_registers(self, mmio):
        raise NotImplementedError("Function must be implemented in subclass")

    def add_mmio_write_registers(self, mmio):
        raise NotImplementedError("Function must be implemented in subclass")

    def add_mmio_read_registers(self, mmio):
        raise NotImplementedError("Function must be implemented in subclass")
    
    def _create_pin_alias(self, board_name, index, alias, parameter):
        """Creates an alias for a pin
        """
        return f"alias pin {board_name}.{self.module_type}.{index:02}.{parameter} {board_name}.{self.module_type}.{alias}.{parameter}"

    def _create_param_alias(self, board_name, index, alias, parameter):
        """Creates an alias for a param
        """
        return f"alias param {board_name}.{self.module_type}.{index:02}.{parameter} {board_name}.{self.module_type}.{alias}.{parameter}"

    def create_aliases(self, board_name):
        """Creates the aliases for the pins and params. Aliases can make the HAL-file 
        easier to read. To use aliases is completely optional for the user.

        Rationale: In earlier versions of LitexCNC the pin-names were determined from
        the JSON, which had to be loaded with the driver. The JSON was linked with a
        CRC-code. A small change in the JSON required re-compilation of the firmware.
        In this version the JSON is not used anymore by the driver, the capabilies of
        the FPGA are announced by the FPGA themselves. All pins therefore are numbered,
        with the alias-function from halcmd the original behavior can be emulated.
        """
        aliases = []
        for index, instance in enumerate(self.instances):
            # If name is not given, no alias will be created
            if instance.name is None:
                continue
            # Create aliases
            pin_alias = partial(self._create_pin_alias, board_name, index, instance.name)
            param_alias = partial(self._create_param_alias, board_name, index, instance.name)
            for pin in instance.hal_pins:
                aliases.append(pin_alias(pin))
            for param in instance.hal_params:
                aliases.append(param_alias(param))
        return aliases

    @property
    def config_size(self):
        raise NotImplementedError("Property must be implemented in subclass")

    def store_config(self, mmio):
        raise NotImplementedError("Function must be implemented in subclass")


entries = entry_points()
if hasattr(entries, "select"):
    # The select method was introduced in importlib_metadata 3.9 (and Python 3.10)
    # and the previous dict interface was declared deprecated
    modules = entries.select(group=GROUP)  # type: ignore
else:
    # TODO: Once Python 3.10 becomes the oldest version supported, this fallback and
    #       conditional statement can be removed.
    modules = (extension for extension in entries.get(GROUP, []))  # type: ignore
for module in modules:
    module.load()
