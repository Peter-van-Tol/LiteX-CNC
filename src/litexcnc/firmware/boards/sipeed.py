# Imports for defining the board
from litex.soc.integration.soc_core import *
from litex_boards.targets.sipeed_tang_nano_20k import _CRG as _CRG_20K
from litex_boards.platforms import (
    sipeed_tang_nano_20k,
)


class TangNanoBase(SoCMini):

    def __init__(
            self,
            board: str,
            config: 'LitexCNC_Firmware',
        ):

        assert board in ["TangNano20K", ], board
        if board == "TangNano20K":
            platform = sipeed_tang_nano_20k.Platform(toolchain="apicula")
            CRG = _CRG_20K

        # SoCMini ----------------------------------------------------------------------------------
        self.clock_frequency = config.clock_frequency
        SoCMini.__init__(self, platform, clk_freq=config.clock_frequency,
            ident          = config.board_name,
            ident_version  = True,
        )

        # CRG --------------------------------------------------------------------------------------
        self.crg = CRG(self.platform, config.clock_frequency)
