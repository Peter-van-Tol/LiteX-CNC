from litex.build.gowin.platform import GowinPlatform
from litex_boards.platforms import (
    sipeed_tang_nano_20k,
)

from .. import _move_user_led_and_btn_to_connector, ExtensionBoardBase


class Platform_TangNano20K(sipeed_tang_nano_20k.Platform):
    """Platform supporting the RV901T, where the user LED and user button can be used
    in LitexCNC-firmware
    """

    def __init__(self, toolchain="apicula"):
        device = "GW2AR-LV18QN88C8/I7"
        devicename = "GW2AR-18C"
        io = sipeed_tang_nano_20k._io
        connectors = sipeed_tang_nano_20k._connectors
        # Move user LED and user button to connector
        io, connectors = _move_user_led_and_btn_to_connector(io, connectors)
        # Initalize the platform
        GowinPlatform.__init__(self, device, io, connectors, toolchain=toolchain, devicename=devicename)
        # Set toolchain options
        self.toolchain.options["use_mspi_as_gpio"]  = 1
        self.toolchain.options["use_sspi_as_gpio"]  = 1
        self.toolchain.options["use_ready_as_gpio"] = 1
        self.toolchain.options["use_done_as_gpio"]  = 1
        self.toolchain.options["rw_check_on_ram"]   = 1
