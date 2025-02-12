try:
    from typing import Literal
except ImportError:
    from typing_extensions import Literal

from pydantic import Field

# Imports for defining the board
from litex.soc.integration.soc_core import *
from litex_boards.targets import (
    sipeed_tang_nano_20k,
)

from litexcnc.config.connections import EtherboneConnection, SPIboneConnection
from litexcnc.firmware.connections.spi import add_spi
from .platform_tangnano20k import Platform_TangNano20K
from .. import ConfigBase


PLATFORM = {
    "tangnano20k": Platform_TangNano20K,
}
CRG = {
    "tangnano20k": lambda platform, clock_frequency: sipeed_tang_nano_20k._CRG(platform, clock_frequency),
}


class Config(ConfigBase):

    family: Literal["sipeed"] = "sipeed"
    board_type: Literal["TangNano20k"] = "TangNano20k"
    clock_frequency: int = Field(
        int(27e6),
        description="The requested clock frequency of the FPGA. For TangNano20k this must be 27 MHz."  
    )
    connection: SPIboneConnection = Field(
        ...,
        description="The configuration of the connection to the board. Only SPIboneConnection is "
        "supported for the TangNano."
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
        self.submodules.crg = CRG[config.board_type.lower()](self.platform, config.clock_frequency)
