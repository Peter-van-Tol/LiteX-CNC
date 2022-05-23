# Imports for defining the board
from liteeth.phy.ecp5rgmii import LiteEthPHYRGMII
from litex.soc.integration.soc_core import *
from litex_boards.targets.colorlight_5a_75x import _CRG
from litex_boards.platforms import colorlight_5a_75b, colorlight_5a_75e

from ..soc import LitexCNC_Firmware


class ColorLightBase(SoCMini):

    def __init__(
            self,
            board: str,
            revision: str,
            config: 'LitexCNC_Firmware',
            sys_clk_freq=int(50e6)
            ):

        # Get the correct board type
        board = board.lower()
        assert board in ["5a-75b", "5a-75e"]
        if board == "5a-75b":
            platform = colorlight_5a_75b.Platform(revision=revision)
        elif board == "5a-75e":
            platform = colorlight_5a_75e.Platform(revision=revision)

        # SoCMini ----------------------------------------------------------------------------------
        SoCMini.__init__(self, platform, clk_freq=sys_clk_freq,
            ident          = config.board_name,
            ident_version  = True,
        )
        # CRG --------------------------------------------------------------------------------------
        self.submodules.crg = _CRG(platform, sys_clk_freq)
        
        # Etherbone --------------------------------------------------------------------------------
        self.submodules.ethphy = LiteEthPHYRGMII(
            clock_pads = self.platform.request("eth_clocks"),
            pads       = self.platform.request("eth"),
            **{key: value for key, value in config.ethphy.dict(exclude={'module'}).items() if value is not None})
        self.add_etherbone(
            phy=self.ethphy,
            mac_address=config.etherbone.mac_address,
            ip_address=str(config.etherbone.ip_address),
            buffer_depth=255
        )


class ColorLight_5A_75B_V6_1(ColorLightBase):
    def __init__(self, config: 'LitexCNC_Firmware', sys_clk_freq=int(50e6)):
        super().__init__("5a-75b", "6.1", config, sys_clk_freq)


class ColorLight_5A_75B_V7_0(ColorLightBase):
    def __init__(self, config: 'LitexCNC_Firmware', sys_clk_freq=int(50e6)):
        super().__init__("5a-75b", "7.0", config, sys_clk_freq)


class ColorLight_5A_75B_V8_0(ColorLightBase):
    def __init__(self, config: 'LitexCNC_Firmware', sys_clk_freq=int(50e6)):
        super().__init__("5a-75b", "8.0", config, sys_clk_freq)


class ColorLight_5A_75E_V6_0(ColorLightBase):
    def __init__(self, config: 'LitexCNC_Firmware', sys_clk_freq=int(50e6)):
        super().__init__("5a-75e", "6.0", config, sys_clk_freq)


class ColorLight_5A_75E_V7_1(ColorLightBase):
    def __init__(self, config: 'LitexCNC_Firmware', sys_clk_freq=int(50e6)):
        super().__init__("5a-75e", "7.1", config, sys_clk_freq)
