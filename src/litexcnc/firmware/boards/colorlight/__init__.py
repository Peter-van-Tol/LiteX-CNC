try:
    from typing import List, Literal, Optional, Union
except ImportError:
    from typing_extensions import List, Literal, Optional, Union

from pydantic import Field, validator

# Imports for defining the board
from litex.soc.integration.soc_core import *
from litex_boards.targets import (
    colorlight_5a_75x,
    colorlight_i5
)

from litexcnc.config.connections import EtherboneConnection, SPIboneConnection
from litexcnc.firmware.connections.spi import add_spi
from .etherbone import add_etherbone
from .platform_5a_75x import Platform_5A_75B, Platform_5A_75E
from .platform_ix import Platform_i5, Platform_i9
from .. import ConfigBase

# Mapping between board type and the Platform to be used
PLATFORM = {
    "5a-75b": Platform_5A_75B,
    "5a-75e": Platform_5A_75E,
    "i5": Platform_i5,
    "i9": Platform_i9
}

CRG = {
    "5a-75b": lambda platform, clock_frequency: colorlight_5a_75x._CRG(platform, clock_frequency, with_rst=False),
    "5a-75e": lambda platform, clock_frequency: colorlight_5a_75x._CRG(platform, clock_frequency, with_rst=False),
    "i5": lambda platform, clock_frequency: colorlight_i5._CRG(platform, clock_frequency),
    "i9": lambda platform, clock_frequency: colorlight_i5._CRG(platform, clock_frequency),
}

REVISIOMS = {
    "5A-75B": Platform_5A_75B.REVISIONS,
    "5A-75E": Platform_5A_75E.REVISIONS,
    "i5": Platform_i5.REVISIONS,
    "i5": Platform_5A_75E.REVISIONS,
}

EXTENSION_BOARDS = {
    "5A-75B": ("HUB75HAT", ),
}


class Config(ConfigBase):

    family: Literal["colorlight"] = "colorlight"
    board_type: Literal[
        "5A-75B",
        "5A-75E",
        "i5",
        "i9"
    ]
    revision: str = Field(
        ...,
        description="The revision of the board. Supported revisions are: \n"
        "- 5A-75B: 6.1, 7.0, and 8.0;\n"
        "- 5A-75B: 6.0, 7.1, and 8.2;\n"
        "- 5i: 7.0;\n"
        "- 9i: 7.2;\n"
    )
    extension_board: Optional[str] = Field(
        None,
        description="The extension board used. Currently only HUB75HAT is supported for the 5A-75B."
    )
    clock_frequency: int = Field(
        50e6,
        description="The requested clock frequency of the FPGA. Lower this frequency in case of timing errors."  
    )
    connection: Union[EtherboneConnection, SPIboneConnection, List[Union[EtherboneConnection, SPIboneConnection]]] = Field(
        ...,
        description="The configuration of the connection to the board."
    )

    @validator('revision')
    def check_revision(cls, v, values, **kwargs):
        if not v in REVISIOMS[values['board_type']]:
            raise ValueError(f"Unsupported revison '{v}' for board '{values['board_type']}. Supported revisons are {', '.join(REVISIOMS[values['board_type']])}")
        return v
    
    @validator('extension_board')
    def check_extension_board(cls, v, values, **kwargs):
        if not v:
            return v
        if values['board_type'] not in EXTENSION_BOARDS:
            raise ValueError(f"No extension boards are supported for '{values['board_type']}'")
        if not v in EXTENSION_BOARDS[values['board_type']]:
            raise ValueError(f"Unsupported extension board '{v}' for board '{values['board_type']}. Supported revisons are {', '.join(REVISIOMS[values['board_type']])}")
        return v
    
    def generate_soc(self, name):
        """Returns the board on which LitexCNC firmware is designed for.
        """
        return SoC(
            self,
            name
        )


class SoC(SoCMini):

    CONNECTION_MAPPING = {
        "etherbone": add_etherbone,
        "spi": add_spi
    }

    def __init__(
            self,
            config: Config,
            name: str
            ):

        # Get the correct platform
        platform = PLATFORM[config.board_type.lower()](config.revision, config.extension_board)
        
        # SoCMini ----------------------------------------------------------------------------------
        self.clock_frequency = config.clock_frequency
        SoCMini.__init__(self, platform, clk_freq=config.clock_frequency,
            ident          = name,
            ident_version  = True,
        )

        # Connectivity -----------------------------------------------------------------------------
        connections = config.connection
        if not isinstance(config.connection, list):
            connections = [connections]
        for connection in connections:
            if connection.connection_type not in self.CONNECTION_MAPPING:
                raise KeyError(f"Unsupported connection type '{connection.connection_type}'")
            self.CONNECTION_MAPPING[connection.connection_type](self, connection)

        # CRG --------------------------------------------------------------------------------------
        self.submodules.crg = CRG[config.board_type.lower()](self.platform, config.clock_frequency)


__all__ = [
    'Config',
    'SoC'
]
