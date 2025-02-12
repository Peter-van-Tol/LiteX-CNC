import re
from litex_boards.platforms import (
    colorlight_i5
)
from litex.build.lattice import LatticeECP5Platform

from .. import _move_user_led_and_btn_to_connector, ExtensionBoardBase


RELEVANCE = re.compile(
    r"^\s*(\|\s*.*\s*\|\s*\d{1,}\s*\|\s*\d{1,}\s*\|\s*.*\s*\|)$",
    re.M
)
IS_PAD_NUMBER = re.compile(r"^([A-Z]{1}\d{1,})$")


def _pad_or_default(text):
    """Small helper function to check whether the text is a valid pad number of an ECP5. Valid
    pad numbers are defined as <single letter><one or more numbers>. It is not checked whether
    the pad is really present on the FPGA. When the text is not a valid pad, the default value
    ``-`` is returned.
    
    Args:
        text (str): The text to be checked whether it is a valid pad.
    """
    is_pad = IS_PAD_NUMBER.search(text)
    if is_pad:
        return is_pad.group()
    return "-"


def _desciptor_to_connector(connector_name, descriptor, connectors = None):
    
    # Create a new collection of connectors if None is given. 
    # NOTE: never put {} as a default value, this gives hard to trace errors in Python. The use
    # of None is considere a safe default value.
    if connectors is None:
        connectors = {}
    # Parse the string of the connector
    pins = {0: "-"}
    for relevant in RELEVANCE.findall(descriptor):
        entries = list(map(str.strip, relevant.strip('|').split("|")))
        pins[int(entries[1])] = _pad_or_default(entries[0])
        pins[int(entries[2])] = _pad_or_default(entries[3])
    # Add the connector to the collection
    connectors[connector_name] = " ".join([pins.get(pin_number, "-") for pin_number in range(max(pins.keys())+1)])
    return connectors


class Platform_i5(colorlight_i5.Platform):
    """Platform supporting the i5, where the user LED and user button can be used
    in LitexCNC-firmware. The pinout has been re-written to the dimm-slot of the
    FPGA, so it can be easily translated to different extension boards.
    """
    REVISIONS = ["7.0"]

    # Source: https://github.com/wuxx/Colorlight-FPGA-Projects/blob/master/README.md
    v7_0_dimm = """
        | Function | Top-Pin | Bot-Pin | Function |
        |----------|---------|---------|----------|
        |   GND    |    1    |    2    |   5V     |
        |   GND    |    3    |    4    |   5V     |
        |   GND    |    5    |    6    |   5V     |
        |   GND    |    7    |    8    |   5V     |
        |   GND    |    9    |    10   |   5V     |
        |   GND    |    11   |    12   |   5V     |
        |   NC     |    13   |    14   |   NC     |
        |   ETH1_1P|    15   |    16   |   ETH2_1P|
        |   ETH1_1N|    17   |    18   |   ETH2_1N|
        |   NC     |    19   |    20   |   NC     |
        |   ETH1_2N|    21   |    22   |   ETH2_2N|
        |   ETH1_2P|    23   |    24   |   ETH2_2P|
        |   NC     |    25   |    26   |   NC     |
        |   ETH1_3P|    27   |    28   |   ETH2_3P|
        |   ETH1_3N|    29   |    30   |   ETH2_3N|
        |   NC     |    31   |    32   |   NC     |
        |   ETH1_4N|    33   |    34   |   ETH2_4N|
        |   ETH1_4P|    35   |    36   |   ETH2_4P|
        |   NC     |    37   |    38   |   NC     |
        |   GND    |    39   |    40   |   GND    |
        |          |         |         |          |
        |          |         |         |          |
        |   U16    |    41   |    42   |   R1     |
        |   NC     |    43   |    44   |   T1     |
        |   NC     |    45   |    46   |   U1     |
        |   NC     |    47   |    48   |   Y2     |
        |   K18    |    49   |    50   |   W1     |
        |   C18    |    51   |    52   |   V1     |
        |   NC     |    53   |    54   |   M1     |
        |   GND    |    55   |    56   |   GND    |
        |   T18    |    57   |    58   |   N2     |
        |   R18    |    59   |    60   |   N3     |
        |   R17    |    61   |    62   |   T2     |
        |   P17    |    63   |    64   |   M3     |
        |   M17    |    65   |    66   |   T3     |
        |   T17    |    67   |    68   |   R3     |
        |   U18    |    69   |    70   |   N4     |
        |   U17    |    71   |    72   |   M4     |
        |   P18    |    73   |    74   |   L4     |
        |   N17    |    75   |    76   |   L5     |
        |   N18    |    77   |    78   |   P16    |
        |   M18    |    79   |    80   |   J16    |
        |   L20    |    81   |    82   |   J18    |
        |   L18    |    83   |    84   |   J17    |
        |   K20    |    85   |    86   |   H18    |
        |   K19    |    87   |    88   |   H17    |
        |   J20    |    89   |    90   |   G18    |
        |   J19    |    91   |    92   |   H16    |
        |   H20    |    93   |    94   |   F18    |
        |   G20    |    95   |    96   |   G16    |
        |   G19    |    97   |    98   |   E18    |
        |   F20    |    99   |    100  |   F17    |
        |   F19    |    101  |    102  |   F16    |
        |   E20    |    103  |    104  |   E16    |
        |   GND    |    105  |    106  |   GND    |
        |   GND    |    107  |    108  |   GND    |
        |   E19    |    109  |    110  |   E17    |
        |   D20    |    111  |    112  |   D18    |
        |   D19    |    113  |    114  |   D17    |
        |   C20    |    115  |    116  |   G5     |
        |   B20    |    117  |    118  |   D16    |
        |   B19    |    119  |    120  |   F5     |
        |   B18    |    121  |    122  |   E6     |
        |   A19    |    123  |    124  |   E5     |
        |   C17    |    125  |    126  |   F4     |
        |   A18    |    127  |    128  |   E4     |
        |   D3     |    129  |    130  |   F1     |
        |   C4     |    131  |    132  |   F3     |
        |   B4     |    133  |    134  |   G3     |
        |   C3     |    135  |    136  |   H3     |
        |   E3     |    137  |    138  |   H4     |
        |   A3     |    139  |    140  |   H5     |
        |   C2     |    141  |    142  |   J4     |
        |   B1     |    143  |    144  |   J5     |
        |   C1     |    145  |    146  |   K3     |
        |   D2     |    147  |    148  |   K4     |
        |   D1     |    149  |    150  |   K5     |
        |   E2     |    151  |    152  |   B3     |
        |   E1     |    153  |    154  |   A2     |
        |   F2     |    155  |    156  |   B2     |
        |   GND    |    157  |    158  |   GND    |
        |   NC     |    159  |    160  |   NC     |
        |   NC     |    161  |    162  |   NC     |
        |   NC     |    163  |    164  |   NC     |
        |   NC     |    165  |    166  |   NC     |
        |   NC     |    167  |    168  |   NC     |
        |   NC     |    169  |    170  |   NC     |
        |   NC     |    171  |    172  |   NC     |
        |   NC     |    173  |    174  |   NC     |
        |   NC     |    175  |    176  |   NC     |
        |   NC     |    177  |    178  |   NC     |
        |   NC     |    179  |    180  |   NC     |
        |   NC     |    181  |    182  |   NC     |
        |   NC     |    183  |    184  |   NC     |
        |   NC     |    185  |    186  |   NC     |
        |   NC     |    187  |    188  |   NC     |
        |   NC     |    189  |    190  |   NC     |
        |   NC     |    191  |    192  |   NC     |
        |   NC     |    193  |    194  |   NC     |
        |   NC     |    195  |    196  |   NC     |
        |   NC     |    197  |    198  |   NC     |
        |   GND    |    199  |    200  |   GND    |
        |----------|---------|---------|----------|
    """

    def __init__(self, revision, ext_board, toolchain="trellis"):
        # Select correct device, io and connectors
        assert revision in self.REVISIONS
        device = "LFE5U-25F-6BG381C"
        io = colorlight_i5._io_v7_0
        connectors = _desciptor_to_connector("dimm", self.v7_0_dimm)
        # TODO: ext-board
        # Move user LED and user button to connector
        io, connectors = _move_user_led_and_btn_to_connector(io, connectors)
        # Initalize the platform
        LatticeECP5Platform.__init__(self, device, io, connectors=connectors, toolchain=toolchain)


class Platform_i9(colorlight_i5.Platform):
    """Platform supporting the i5, where the user LED and user button can be used
    in LitexCNC-firmware. The pinout has been re-written to the dimm-slot of the
    FPGA, so it can be easily translated to different extension boards.
    """
    REVISIONS = ["7.2"]

    # Source: https://github.com/wuxx/Colorlight-FPGA-Projects/blob/master/colorlight_i9_v7.2.md
    v7_2_dimm = """
        | Function | Top-Pin | Bot-Pin | Function |
        |----------|---------|---------|----------|
        |   GND    |    1    |    2    |   5V     |
        |   GND    |    3    |    4    |   5V     |
        |   GND    |    5    |    6    |   5V     |
        |   GND    |    7    |    8    |   5V     |
        |   GND    |    9    |    10   |   5V     |
        |   GND    |    11   |    12   |   5V     |
        |   NC     |    13   |    14   |   NC     |
        |   ETH1_1P|    15   |    16   |   ETH2_1P|
        |   ETH1_1N|    17   |    18   |   ETH2_1N|
        |   NC     |    19   |    20   |   NC     |
        |   ETH1_2N|    21   |    22   |   ETH2_2N|
        |   ETH1_2P|    23   |    24   |   ETH2_2P|
        |   NC     |    25   |    26   |   NC     |
        |   ETH1_3P|    27   |    28   |   ETH2_3P|
        |   ETH1_3N|    29   |    30   |   ETH2_3N|
        |   NC     |    31   |    32   |   NC     |
        |   ETH1_4N|    33   |    34   |   ETH2_4N|
        |   ETH1_4P|    35   |    36   |   ETH2_4P|
        |   NC     |    37   |    38   |   NC     |
        |   GND    |    39   |    40   |   GND    |
        |          |         |         |          |
        |          |         |         |          |
        |   L2     |    41   |    42   |   R1     |
        |   NC     |    43   |    44   |   T1     |
        |   NC     |    45   |    46   |   U1     |
        |   NC     |    47   |    48   |   Y2     |
        |   K18    |    49   |    50   |   W1     |
        |   C18    |    51   |    52   |   V1     |
        |   NC     |    53   |    54   |   M1     |
        |   GND    |    55   |    56   |   GND    |
        |   T18    |    57   |    58   |   N2     |
        |   R18    |    59   |    60   |   N3     |
        |   R17    |    61   |    62   |   T2     |
        |   P17    |    63   |    64   |   M3     |
        |   M17    |    65   |    66   |   T3     |
        |   T17    |    67   |    68   |   R3     |
        |   U18    |    69   |    70   |   N4     |
        |   U17    |    71   |    72   |   M4     |
        |   P18    |    73   |    74   |   L4     |
        |   N17    |    75   |    76   |   L5     |
        |   N18    |    77   |    78   |   P16    |
        |   M18    |    79   |    80   |   J16    |
        |   L20    |    81   |    82   |   J18    |
        |   L18    |    83   |    84   |   J17    |
        |   K20    |    85   |    86   |   H18    |
        |   K19    |    87   |    88   |   H17    |
        |   J20    |    89   |    90   |   G18    |
        |   J19    |    91   |    92   |   H16    |
        |   H20    |    93   |    94   |   F18    |
        |   G20    |    95   |    96   |   G16    |
        |   G19    |    97   |    98   |   E18    |
        |   F20    |    99   |    100  |   F17    |
        |   F19    |    101  |    102  |   F16    |
        |   E20    |    103  |    104  |   E16    |
        |   GND    |    105  |    106  |   GND    |
        |   GND    |    107  |    108  |   GND    |
        |   E19    |    109  |    110  |   E17    |
        |   D20    |    111  |    112  |   D18    |
        |   D19    |    113  |    114  |   D17    |
        |   C20    |    115  |    116  |   G5     |
        |   B20    |    117  |    118  |   D16    |
        |   B19    |    119  |    120  |   F5     |
        |   B18    |    121  |    122  |   E6     |
        |   A19    |    123  |    124  |   E5     |
        |   C17    |    125  |    126  |   F4     |
        |   A18    |    127  |    128  |   E4     |
        |   D3     |    129  |    130  |   F1     |
        |   C4     |    131  |    132  |   F3     |
        |   B4     |    133  |    134  |   G3     |
        |   C3     |    135  |    136  |   H3     |
        |   E3     |    137  |    138  |   H4     |
        |   A3     |    139  |    140  |   H5     |
        |   C2     |    141  |    142  |   J4     |
        |   B1     |    143  |    144  |   J5     |
        |   C1     |    145  |    146  |   K3     |
        |   D2     |    147  |    148  |   K4     |
        |   D1     |    149  |    150  |   K5     |
        |   E2     |    151  |    152  |   B3     |
        |   E1     |    153  |    154  |   A2     |
        |   F2     |    155  |    156  |   B2     |
        |   GND    |    157  |    158  |   GND    |
        |   NC     |    159  |    160  |   NC     |
        |   NC     |    161  |    162  |   NC     |
        |   NC     |    163  |    164  |   NC     |
        |   NC     |    165  |    166  |   NC     |
        |   NC     |    167  |    168  |   NC     |
        |   NC     |    169  |    170  |   NC     |
        |   NC     |    171  |    172  |   NC     |
        |   NC     |    173  |    174  |   NC     |
        |   NC     |    175  |    176  |   NC     |
        |   NC     |    177  |    178  |   NC     |
        |   NC     |    179  |    180  |   NC     |
        |   NC     |    181  |    182  |   NC     |
        |   NC     |    183  |    184  |   NC     |
        |   NC     |    185  |    186  |   NC     |
        |   NC     |    187  |    188  |   NC     |
        |   NC     |    189  |    190  |   NC     |
        |   NC     |    191  |    192  |   NC     |
        |   NC     |    193  |    194  |   NC     |
        |   NC     |    195  |    196  |   NC     |
        |   NC     |    197  |    198  |   NC     |
        |   GND    |    199  |    200  |   GND    |
        |----------|---------|---------|----------|
    """

    def __init__(self, revision, ext_board, toolchain="trellis"):
        # Select correct device, io and connectors
        assert revision in self.REVISIONS
        device = "LFE5U-45F-6BG381C"
        io = colorlight_i5._io_v7_2
        connectors = _desciptor_to_connector("dimm", self.v7_2_dimm)
        # TODO: ext-board
        # Move user LED and user button to connector
        io, connectors = _move_user_led_and_btn_to_connector(io, connectors)
        # Initalize the platform
        LatticeECP5Platform.__init__(self, device, io, connectors=connectors, toolchain=toolchain)
