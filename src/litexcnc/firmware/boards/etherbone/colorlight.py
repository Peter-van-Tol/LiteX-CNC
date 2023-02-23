# Imports for defining the board
from liteeth.phy.ecp5rgmii import LiteEthPHYRGMII
from litex.soc.integration.soc_core import *
from litex_boards.targets.colorlight_5a_75x import _CRG
from litex_boards.platforms import colorlight_5a_75b, colorlight_5a_75e


class ColorLightBase(SoCMini):

    def __init__(
            self,
            board: str,
            revision: str,
            config: 'LitexCNC_Firmware',
            ):

        # Get the correct board type
        board = board.lower()
        assert board in ["5a-75b", "5a-75e"]
        if board == "5a-75b":
            platform = colorlight_5a_75b.Platform(revision=revision)
        elif board == "5a-75e":
            platform = colorlight_5a_75e.Platform(revision=revision)


        # SoCMini ----------------------------------------------------------------------------------
        self.clock_frequency = config.clock_frequency
        SoCMini.__init__(self, platform, clk_freq=config.clock_frequency,
            ident          = config.board_name,
            ident_version  = True,
        )

        # CRG --------------------------------------------------------------------------------------
        self.submodules.crg = _CRG(self.platform, config.clock_frequency, with_rst=False)
        
        # Etherbone --------------------------------------------------------------------------------
        self.submodules.ethphy = LiteEthPHYRGMII(
            clock_pads = self.platform.request("eth_clocks"),
            pads       = self.platform.request("eth"),
            **{key: value for key, value in config.ethphy.dict(exclude={'module'}).items() if value is not None})
        self.add_etherbone(
            phy=self.ethphy,
            mac_address=config.etherbone.mac_address,
            ip_address=str(config.etherbone.ip_address),
            buffer_depth=255,
            data_width=32
        )
