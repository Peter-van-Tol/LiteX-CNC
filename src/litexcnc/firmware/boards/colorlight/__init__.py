try:
    from typing import Optional
except ImportError:
    from typing_extensions import Optional

# Imports for defining the board
from litex.soc.integration.soc_core import *
from litex_boards.targets import (
    colorlight_5a_75x,
    colorlight_i5
)

from litexcnc.firmware.connections.spi import add_spi
from .etherbone import add_etherbone
from .platform_5a_75x import Platform_5A_75B, Platform_5A_75E
from .platform_ix import Platform_i5, Platform_i9

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


class ColorLight(SoCMini):

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
