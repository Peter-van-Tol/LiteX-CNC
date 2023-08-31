try:
    from typing import Literal
except ImportError:
    from typing_extensions import Literal

# Imports from Pydantic to create the config
from pydantic import Field

# Local imports
from . import Etherbone, EthPhy, EtherboneBoard


class ColorLight_5A_75X(EtherboneBoard):
    """
    Configuration for Colorlight 5A-75B and 5A-75E cards:

    """
    board_type: Literal[
        '5A-75B v6.1',
        '5A-75B v7.0',
        '5A-75B v8.0',
        '5A-75E v6.0',
        '5A-75E v7.1'
    ]
    ethphy: EthPhy = Field(
        ...
    )
    etherbone: Etherbone = Field(
        ...,
    )

    def _generate_soc(self):
        """Returns the board on which LitexCNC firmware is designed for.
        """
        # Deferred imports to prevent importing Litex while installing the driver
        from litexcnc.firmware.boards.etherbone.colorlight import ColorLightBase
        # Split the board and the revision from the board type and create the SoC
        # based on the board type.
        board, revision = self.board_type.split('v')
        return ColorLightBase(
            board=board.strip().lower(),
            revision=revision.strip().lower(),
            config=self
        )
