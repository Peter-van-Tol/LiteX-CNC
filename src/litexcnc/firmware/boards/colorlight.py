# Imports for defining the board
from litex.soc.integration.soc_core import *
from litex_boards.targets.colorlight_5a_75x import _CRG
from litex_boards.platforms import colorlight_5a_75b, colorlight_5a_75e


class Hub75Hat(colorlight_5a_75b.Platform):

    @staticmethod
    def definition_to_pad(definition, hub75_connectors):
        if definition == "-":
            return "-"
        header, number = definition.split(":")
        return list(filter(None, hub75_connectors[header].split(" ")))[int(number)]


    @staticmethod
    def get_connectors(revision):
        assert revision in ["6.1", "7.0", "8.0"]
        hub75_connectors = { 
            "6.1": colorlight_5a_75b._connectors_v6_1, 
            "7.0": colorlight_5a_75b._connectors_v7_0,
            "8.0": colorlight_5a_75b._connectors_v8_0}[revision]
        hub75_connectors = {number: pinout for (number, pinout) in hub75_connectors}
        j1 = [
            '-',  # Lets start numbering from 1 ...
            "j4:7",
            "j8:0",
            "j8:1",
            "j8:2",
            "j8:4",
            "j8:5",
            "j8:6",
            "j7:0",
            "j7:1",
            "j2:5",
            "j2:6",
            "j1:0",
            "j1:1",
            "j1:2",
            "j1:4",
            "j1:5",
            "j1:6",
            "-",  # GND
            "-",  # GND
            "-",  # GND
            "-",  # GND
            "-",  # +5V / GND (selectable)
            "-",  # +5V / GND (selectable)
            "-",  # +5V / GND (selectable)
            "-",  # +5V / GND (selectable)
            "-",  # +5V / GND (selectable)
        ]
        j2 = [            
            '-',  # Lets start numbering from 1 ...
            "j4:8",
            "j7:2",
            "j7:4",
            "j7:5",
            "j7:6",
            "j6:0",
            "j6:1",
            "j6:2",
            "j6:4",
            "j3:2",
            "j3:4",
            "j3:5",
            "j3:6",
            "j2:0",
            "j2:1",
            "j2:2",
            "j2:4",
            "-",  # GND
            "-",  # GND
            "-",  # GND
            "-",  # GND
            "-",  # +5V / GND (selectable)
            "-",  # +5V / GND (selectable)
            "-",  # +5V / GND (selectable)
            "-",  # +5V / GND (selectable)
            "-",  # +5V / GND (selectable)
        ]
        j3 = [            
            '-',  # Lets start numbering from 1 ...
            "j4:9",
            "j6:5",
            "j6:6",
            "j5:0",
            "j5:1",
            "j5:2",
            "j5:4",
            "j5:5",
            "j5:6",
            "j4:0",
            "j4:1",
            "j4:2",
            "j4:4",
            "j4:5",
            "j4:6",
            "j3:0",
            "j3:1",
            "-",  # GND
            "-",  # GND
            "-",  # GND
            "-",  # GND
            "-",  # +5V / GND (selectable)
            "-",  # +5V / GND (selectable)
            "-",  # +5V / GND (selectable)
            "-",  # +5V / GND (selectable)
            "-",  # +5V / GND (selectable)
        ]
        spi = [
            "j5:14",  # MOSI
            "j5:12",  # MISO
            "j5:13",  # CLOCK
            "j5:11",  # CS
        ]
        ena = ["j5:10"]
        return [
            ("j1", " ".join([Hub75Hat.definition_to_pad(pin, hub75_connectors) for pin in j1])),
            ("j2", " ".join([Hub75Hat.definition_to_pad(pin, hub75_connectors) for pin in j2])),
            ("j3", " ".join([Hub75Hat.definition_to_pad(pin, hub75_connectors) for pin in j3])),
            ("spi", " ".join([Hub75Hat.definition_to_pad(pin, hub75_connectors) for pin in spi])),
            ("ena", " ".join([Hub75Hat.definition_to_pad(pin, hub75_connectors) for pin in ena])),
        ]

    def __init__(self, revision="7.0", toolchain="trellis"):
        self.revision = revision
        device = {
            "6.1": "LFE5U-25F-6BG381C",        
            "7.0": "LFE5U-25F-6BG256C", 
            "8.0": "LFE5U-25F-6BG256C"}[revision]
        io = {
            "6.1": colorlight_5a_75b._io_v6_1, 
            "7.0": colorlight_5a_75b._io_v7_0,
            "8.0": colorlight_5a_75b._io_v8_0}[revision]
        connectors = self.get_connectors(revision)
        colorlight_5a_75b.LatticePlatform.__init__(self, device, io, connectors=connectors, toolchain=toolchain)


class ColorLightBase(SoCMini):

    def __init__(
            self,
            board: str,
            revision: str,
            config: 'LitexCNC_Firmware',
            ):

        # Get the correct board type
        board = board.lower()
        assert board in ["5a-75b", "5a-75e", "hub75hat",]
        if board == "5a-75b":
            platform = colorlight_5a_75b.Platform(revision=revision)
        elif board == "5a-75e":
            platform = colorlight_5a_75e.Platform(revision=revision)
        elif board == "hub75hat":
            platform = Hub75Hat(revision=revision)


        # SoCMini ----------------------------------------------------------------------------------
        self.clock_frequency = config.clock_frequency
        SoCMini.__init__(self, platform, clk_freq=config.clock_frequency,
            ident          = config.board_name,
            ident_version  = True,
        )

        # CRG --------------------------------------------------------------------------------------
        self.submodules.crg = _CRG(self.platform, config.clock_frequency, with_rst=False)
