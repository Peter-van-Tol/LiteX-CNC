try:
    from typing import List, Literal, Optional, Union
except ImportError:
    from typing_extensions import List, Literal, Optional, Union

from pydantic import Field

# Imports for defining the board
from litex.soc.integration.soc_core import *
from litex_boards.targets import (
    linsn_rv901t,
)

from litexcnc.config.connections import EtherboneConnection, SPIboneConnection
from litexcnc.firmware.connections.spi import add_spi
from .etherbone import add_etherbone
from .platform_rv901t import Platform_RV901T
from .. import ConfigBase


PLATFORM = {
    "rv901t": Platform_RV901T,
}
CRG = {
    "rv901t": lambda platform, clock_frequency: linsn_rv901t._CRG(platform, clock_frequency),
}


class Config(ConfigBase):

    family: Literal["linsn"] = "linsn"
    board_type: Literal["rv901t"] = "rv901t"
    clock_frequency: int = Field(
        50e6,
        description="The requested clock frequency of the FPGA. Lower this frequency in case of timing errors."  
    )
    connection: Union[EtherboneConnection, SPIboneConnection, List[Union[EtherboneConnection, SPIboneConnection]]] = Field(
        ...,
        description="The configuration of the connection to the board."
    )

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
        platform = PLATFORM[config.board_type.lower()]()
        
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
        self.submodules.crg = CRG[config.board_type](self.platform, config.clock_frequency)


__all__ = [
    'ColorLightBase'
]


