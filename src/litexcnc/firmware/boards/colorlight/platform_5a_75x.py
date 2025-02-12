"""This module contains the modified platforms for the Colorlight 5A-75B and 5A-75E. These
platforms have been adapted to ensure the user LED and user button can be used directly
by LitexCNC
"""
from litex_boards.platforms import (
    colorlight_5a_75b,
    colorlight_5a_75e
)
from litex.build.lattice import LatticeECP5Platform

from .. import _move_user_led_and_btn_to_connector, ExtensionBoardBase


class HUB75HAT(ExtensionBoardBase):
    """Definition of the pinout between the 5A-75B and the three 26-pin connectors
    on the HUB75HAT. The connections have been taken KiCAD.
    """

    j1 = [
        '-',  # Lets start numbering from 1 ...
        "j4:7",
        "j1:0",
        "j1:1",
        "j1:2",
        "j1:4",
        "j1:5",
        "j1:6",
        "j2:0",
        "j2:1",
        "j7:5",
        "j7:6",
        "j8:0",
        "j8:1",
        "j8:2",
        "j8:4",
        "j8:5",
        "j8:6",
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
        "j2:2",
        "j2:4",
        "j2:5",
        "j2:6",
        "j3:0",
        "j3:1",
        "j3:2",
        "j3:4",
        "j6:2",
        "j6:4",
        "j6:5",
        "j6:6",
        "j7:0",
        "j7:1",
        "j7:2",
        "j7:4",
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
        "j3:5",
        "j3:6",
        "j4:0",
        "j4:1",
        "j4:2",
        "j4:4",
        "j4:5",
        "j4:6",
        "j5:0",
        "j5:1",
        "j5:2",
        "j5:4",
        "j5:5",
        "j5:6",
        "j6:0",
        "j6:1",
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

    @classmethod
    def translate(cls, connectors):
        
        connectors = {number: pinout for (number, pinout) in connectors}
        return [
            ("j1", " ".join([cls.definition_to_pad(pin, connectors) for pin in cls.j1])),
            ("j2", " ".join([cls.definition_to_pad(pin, connectors) for pin in cls.j2])),
            ("j3", " ".join([cls.definition_to_pad(pin, connectors) for pin in cls.j3])),
            ("spi", " ".join([cls.definition_to_pad(pin, connectors) for pin in cls.spi])),
            ("ena", " ".join([cls.definition_to_pad(pin, connectors) for pin in cls.ena])),
        ]


class Platform_5A_75B(colorlight_5a_75b.Platform):
    """Platform supporting the 5A-75B, where the user LED and user button can be used
    in LitexCNC-firmware
    """
    REVISIONS = ["6.1", "7.0", "8.0"]
    
    def __init__(self, revision, ext_board, toolchain="trellis"):
        # Select correct device, io and connectors
        assert revision in self.REVISIONS
        device = {
            "6.1": "LFE5U-25F-6BG381C",
            "7.0": "LFE5U-25F-6BG256C",
            "8.0": "LFE5U-25F-6BG256C"
        }[revision]
        io = {
            "6.1": colorlight_5a_75b._io_v6_1, 
            "7.0": colorlight_5a_75b._io_v7_0,
            "8.0": colorlight_5a_75b._io_v8_0
        }[revision]
        connectors = {
            "6.1": colorlight_5a_75b._connectors_v6_1, 
            "7.0": colorlight_5a_75b._connectors_v7_0,
            "8.0": colorlight_5a_75b._connectors_v8_0
        }[revision]
         # Apply translation from 5A-75B to the extension board (if any)
        if ext_board == "HUB75HAT":
            connectors = HUB75HAT.translate(connectors)
        # Move user LED and user button to connector
        io, connectors = _move_user_led_and_btn_to_connector(io, connectors)
        # Initalize the platform
        LatticeECP5Platform.__init__(self, device, io, connectors=connectors, toolchain=toolchain)


class Platform_5A_75E(colorlight_5a_75e.Platform):
    """Platform supporting the 5A-75B, where the user LED and user button can be used
    in LitexCNC-firmware
    """
    REVISIONS = ["6.0", "7.1", "8.2"]
    
    def __init__(self, revision, ext_board, toolchain="trellis"):
        # Select correct device, io and connectors
        assert revision in self.REVISIONS
        device = {
            "6.0": "LFE5U-25F-6BG256C",
            "7.1": "LFE5U-25F-6BG256C",
            "8.2": "LFE5U-25F-7BG256I" 
        }[revision]
        io = {
            "6.0": colorlight_5a_75e._io_v6_0, 
            "7.1": colorlight_5a_75e._io_v7_1,
            "8.2": colorlight_5a_75e._io_v6_0
        }[revision]
        connectors = {
            "6.0": colorlight_5a_75e._connectors_v6_0, 
            "7.1": colorlight_5a_75e._connectors_v7_1,
            "8.2": colorlight_5a_75e._connectors_v6_0
        }[revision]
         # Apply translation from 5A-75B to the extension board (if any)
        assert ext_board is None
        # Move user LED and user button to connector
        io, connectors = _move_user_led_and_btn_to_connector(io, connectors)
        # Initalize the platform
        LatticeECP5Platform.__init__(self, device, io, connectors=connectors, toolchain=toolchain)
