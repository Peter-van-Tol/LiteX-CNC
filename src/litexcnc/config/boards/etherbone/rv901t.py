try:
    from typing import Literal
except ImportError:
    from typing_extensions import Literal

# Imports from Pydantic to create the config
from pydantic import Field

# Local imports
from . import Etherbone, EthPhy, EtherboneBoard


class RV901T_Config(EtherboneBoard):
    """
    Configuration for Colorlight 5A-75B and 5A-75E cards:

    """
    board_type: Literal[
        "RV901T"
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
        from litexcnc.firmware.boards.etherbone.rv901t import RV901T
        # Split the board and the revision from the board type and create the SoC
        # based on the board type.
        return RV901T(
            config=self
        )
