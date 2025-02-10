try:
    from typing import Optional
except ImportError:
    from typing_extensions import Optional

# Imports for defining the board
from litex.soc.integration.soc_core import *
from litex_boards.targets import (
    linsn_rv901t,
)

from litexcnc.firmware.connections.spi import add_spi
from .etherbone import add_etherbone
from .platform_rv901t import Platform_RV901T

PLATFORM = {
    "rv901t": Platform_RV901T,
}
CRG = {
    "rv901t": lambda platform, clock_frequency: linsn_rv901t._CRG(platform, clock_frequency),
}


class Linsn(SoCMini):

    CONNECTION_MAPPING = {
        "etherbone": add_etherbone,
        "spi": add_spi
    }

    def __init__(
            self,
            board: str,
            revision: str,
            ext_board: Optional[str],
            config: 'LitexCNC_Firmware',
            ):

        # Get the correct platform
        platform = PLATFORM[board.lower()](revision, ext_board)
        
        # SoCMini ----------------------------------------------------------------------------------
        self.clock_frequency = config.clock_frequency
        SoCMini.__init__(self, platform, clk_freq=config.clock_frequency,
            ident          = config.board_name,
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
        self.submodules.crg = CRG[board](self.platform, config.clock_frequency)


__all__ = [
    'ColorLightBase'
]


