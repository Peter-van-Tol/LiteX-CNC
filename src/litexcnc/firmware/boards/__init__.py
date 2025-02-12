"""
This module contains all supported boards. Each sub-module contains a single
family of boards, for example colorlight, linsn, or sipeed. Within a family,
multiple boards can be supported.
"""

import sys
if sys.version_info[:2] >= (3, 8):
    # TODO: Import directly (no need for conditional) when `python_requires = >= 3.8`
    from importlib.metadata import entry_points  # pragma: no cover
else:
    from importlib_metadata import entry_points  # pragma: no cover
try:
    from typing import Any
except ImportError:
    from typing_extensions import Any

from abc import abstractmethod
from copy import copy
from pydantic import BaseModel, Field

# Registry which holds all the sub-classes of modules
board_registry = {}
GROUP = "litexcnc.boards"


def _move_user_led_and_btn_to_connector(io, connectors):
    """Moves the user LED and buttons to a connector type, which can be used in the
    firmware of LitexCNC

    Args:
        ios (dict[str, str]): Dictionary containing signals containg the user LED and
        button. These signals will be removed from this collection
        connectors (dict[str, str]): Dictionary containing the connectors (io LitexCNC
        can connect to). Adding two new connectors ``user_led`` and ``user_button``, to
        which the found items are added.
    """
    io = copy(io)
    connectors = copy(connectors)
    # Modify the collection
    definition_led = "-"
    definition_btn = "-"
    removed = 0
    for index in range(len(io)):
        item = io[index - removed] 
        if item[0] == 'user_led_n':
            definition_led = f"{definition_led} {' '.join(item[2].identifiers)}"
            io.remove(item)
            removed += 1
        if item[0] == 'user_btn_n':
            definition_btn = f"{definition_btn} {' '.join(item[2].identifiers)}"
            io.remove(item)
            removed += 1
    connectors.append(('user_led', definition_led))
    connectors.append(('user_btn', definition_btn))
    return io, connectors


class ConfigBase(BaseModel):
    """Base-class for defining a board
    """
    family: Any
    board_type: Any
    clock_frequency: int
    connection: Any

    def __init_subclass__(cls, **kwargs: Any) -> None:
        super().__init_subclass__(**kwargs)
        board_registry[cls.__fields__['family'].default] = cls

    @abstractmethod
    def generate_soc(self, name):
        """Generates the logic for the board."""
        raise NotImplementedError()


class ExtensionBoardBase():
    """Base-class defining an extension-board (or hat) for a FPGA. The goal is that this class
    translates the pin-out from the FPGA to the pin out of the extension-board or HAT.
    """

    @staticmethod
    def definition_to_pad(definition, connectors) -> str:
        """Small helper function to look up the pad number (for example F53 for a FPGA in 
        BGA package) from the defintion on the connectors (for example j1:6).

        Args:
            definition (str): The defintion of the pin to be translated (i.e. j1:6).
            connectors (list[tupple[str, str]]): Collection of connectors on the original FPGA board.

        Returns:
            str: The pad on the FPGA corresponding to the connector pin number
        """
        if definition == "-":
            return "-"
        header, number = definition.split(":")
        return list(filter(None, connectors[header].split(" ")))[int(number)]
    
    @classmethod
    def translate(cls, connectors):
        """This function translates the connectors on the original FPGA board to those
        present ont he extention board.

        Args:
            connectors (list[tupple[str, str]]): Collection of connectors on the original
            FPGA board.

        Raises:
            NotImplementedError: This function should be defined in derived class.
        """
        raise NotImplementedError("Function should be defined in derived class.")



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
