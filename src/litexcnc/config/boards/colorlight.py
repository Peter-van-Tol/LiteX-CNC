try:
    from typing import Literal
except ImportError:
    from typing_extensions import Literal
from itertools import zip_longest

from litexcnc.firmware.soc import LitexCNC_Firmware


class ColorLight_5A_75X(LitexCNC_Firmware):
    """
    Configuration for Colorlight 5A-75B and 5A-75E cards:

    """
    board_type: Literal[
        '5A-75B v6.1',
        '5A-75B v7.0',
        '5A-75B v8.0',
        '5A-75E v6.0',
        '5A-75E v7.1',
        '5A-75E v8.2',
        '5A-75B v6.1 (HUB75HAT)',
        '5A-75B v7.0 (HUB75HAT)',
        '5A-75B v8.0 (HUB75HAT)',
        'i5 v7.0',
        'i5 v7.0 (EXT_BOARD)'
        'i9 v7.2',
        'i9 v7.2 (EXT_BOARD)'
    ]

    def _generate_soc(self):
        """Returns the board on which LitexCNC firmware is designed for.
        """
        # Deferred imports to prevent importing Litex while installing the driver
        from litexcnc.firmware.boards.colorlight import ColorLight
        # Split the board and the revision from the board type and create the SoC
        # based on the board type.
        board, revision, ext_board = [x[0] for x in zip_longest(self.board_type.split(' '), [None]*3, fillvalue=None)]
        # Remove brackets
        if revision.startswith("v"):
            revision = revision[1:]
        if ext_board and ext_board.startswith("("):
            ext_board = ext_board[ext_board.find("(")+1:ext_board.rfind(")")]
        return ColorLight(
            board=board.strip().lower(),
            revision=revision.strip().lower(),
            ext_board=ext_board,
            config=self
        )
