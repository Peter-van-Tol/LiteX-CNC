from litex.build.xilinx import XilinxSpartan6Platform
from litex_boards.platforms import (
    linsn_rv901t,
)

from .. import _move_user_led_and_btn_to_connector, ExtensionBoardBase


class Platform_RV901T(linsn_rv901t.Platform):
    """Platform supporting the RV901T, where the user LED and user button can be used
    in LitexCNC-firmware
    """

    def __init__(self, revision, ext_board,  toolchain="ise"):
        device = "xc6slx16-2-ftg256"
        io = linsn_rv901t._io
        connectors = linsn_rv901t._connectors
        # Move user LED and user button to connector
        io, connectors = _move_user_led_and_btn_to_connector(io, connectors)
        # Initalize the platform
        XilinxSpartan6Platform.__init__(self, device, io, connectors, toolchain=toolchain)
    