try:
    from typing import Literal
except ImportError:
    from typing_extensions import Literal

from litexcnc.config.connections import SPIboneConnection
from litexcnc.firmware.soc import LitexCNC_Firmware
from pydantic import Field

class TangNano(LitexCNC_Firmware):
    """
    Configuration for Colorlight 5A-75B and 5A-75E cards:
    """
    board_type: Literal[
        'TangNano20K',
    ]
    # TODO: jte SPIboneConnection is now by default 4-wire. This should be modified
    # so 3-wire is also supported.
    connection: SPIboneConnection = Field(
        ...,
        description="The configuration of the connection to the board. For the TangNano only "
        "SPIboneConnection is supported at this moment."
    )

    def _generate_soc(self):
        """Returns the board on which LitexCNC firmware is designed for.
        """
        # Deferred imports to prevent importing Litex while installing the driver
        from litexcnc.firmware.boards.sipeed import TangNanoBase
        # Split the board and the revision from the board type and create the SoC
        # based on the board type.
        return TangNanoBase(
            board=self.board_type,
            config=self
        )
