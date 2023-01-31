from random import setstate
from typing import List
from packaging.version import Version

# Import from litex
from migen.fhdl.module import Module
from litex.soc.interconnect.csr import AutoCSR, CSRStatus, CSRStorage, CSRField
from migen import *

# Local imports
from . import __version__
from typing import TYPE_CHECKING
if TYPE_CHECKING:
    from .soc import LitexCNC_Firmware

# Convert the version of the firmware to 
version = Version(__version__)


class MMIO(Module, AutoCSR):

    def __init__(self, config: 'LitexCNC_Firmware'):
        """
        Initializes the memory registers.

        NOTE:
        The order of the registers in the memory is in the same order as defined in
        this class. This can be inspected by reviewing the generated csr.csv. The driver
        expects the information blocks in the following order:
        - WRITE:
          - Watchdog;
          - GPIO;
          - PWM;
          - StepGen;
        - READ:
          - Watchdog;
          - Wall clock;
          - GPIO;
          - StepGen;

        When the order of the MMIO is mis-aligned with respect to the driver this might
        lead to errors (writing to the wrong registers) or the FPGA being hung up (when
        writing to a read-only register).
        """
        # INITIALISATION
        self.magic = CSRStatus(
            size=32,
            reset=0x18052022,
            description="Magic code, requested by the driver to make sure it is a FPGA with an "
            "instance of LitexCNC-firmware. On a personal note: the magic code is clearly a date. "
            "This date is for me very important to me, in rememberance of my greatest support."
        )
        self.version = CSRStatus(
            fields=[
                CSRField("patch", size=8, offset=0 , reset=version.micro, description="Version number - patch"),
                CSRField("minor", size=8, offset=8 , reset=version.minor, description="Version number - minor"),
                CSRField("major", size=8, offset=16, reset=version.major, description="Version number - major")
            ],
            description="Version of the firmware. The major and minor parts of the version must "
            "be equal between firmware and driver. If there is a difference, the user must update "
            "either the driver or firmware. A difference in patch is allowable, as the data protocol "
            "in this case is not altered. A difference in patch can happen for example when a small "
            "bug has been fixed in the driver or firmware." 
        )
        self.clock_frequency = CSRStatus(
            size=32,
            reset=config.clock_frequency,
            description="Reporting the clock frequency of the FPGA."
        )
        self.module_config = CSRStatus(
            fields=[
                CSRField("module_data_size", size=16, offset=0 , reset=len(config.modules)*4+sum([module.config_size for module in config.modules]), description="Number of bytes to read for config."),
                CSRField("num_modules", size=8, offset=16 , reset=len(config.modules), description="Number of modules"),
            ],
            description="Stores information on the module (such as number)." 
        )
        self.name1 = CSRStatus(
            size=32,
            reset=int.from_bytes(config.board_name.ljust(16, '\0')[0:4].encode("ascii"), byteorder='big'),
            description="Name of the FPGA (bytes 0-3)"
        )
        self.name2 = CSRStatus(
            size=32,
            reset=int.from_bytes(config.board_name.ljust(16, '\0')[4:8].encode("ascii"), byteorder='big'),
            description="Name of the FPGA (bytes 4-7)"
        )
        self.name3 = CSRStatus(
            size=32,
            reset=int.from_bytes(config.board_name.ljust(16, '\0')[8:12].encode("ascii"), byteorder='big'),
            description="Name of the FPGA (bytes 8-11)"
        )
        self.name4 = CSRStatus(
            size=32,
            reset=int.from_bytes(config.board_name.ljust(16, '\0')[12:16].encode("ascii"), byteorder='big'),
            description="Name of the FPGA (bytes 12-15)"
        )
        # Write the configuration of the modules
        for index, module in enumerate(config.modules):
            setattr(self, f'module_{index}',
                CSRStatus(
                    name=f'module_{index}',
                    size=32,
                    reset=module.module_id,
                    description=f"The identification of {index}th module."
                )
            )
            module.store_config(self)

        # After regitering modules the board can be reset
        self.reset = CSRStorage(
            size=1, 
            description="Reset.\nWhile True (set to 1) the card is being forced in reset-mode. In "
            "reset-mode the position and speed of the steppers is reset to the starting position. "
            "When starting the driver, the first step is to reset it, to prevent erronous behaviour "
            "of the steppers.", 
            name='reset'
        )

        # Config of modules
        # self.loop_cycles = CSRStatus(
        #     size=32,
        #     description="The number of clock cycles within the FPGA is normally updated. Due to jitter "
        #     "the actual number of cycles can be more or less then this value, but it is expected to be "
        #     "close. This parameter is used by the stepgen module to start the (expected) motion for the next "
        #     "segement."
        # )
        for module in config.modules:
            module.add_mmio_config_registers(self)


        # OUTPUT (as seen from the PC!)
        # - Watchdog
        self.watchdog_data = CSRStorage(
            size=32, 
            description="Watchdog data.\nByte containing the enabled flag (bit 31) and the time (bit 30 - 0)."
            "out in cpu cycles.", 
            name='watchdog_data',
            write_from_dev=True
        )
        # - Modules
        for module in config.modules:
            module.add_mmio_write_registers(self)
 
        # INPUT (as seen from the PC!)
        # - Watchdog
        self.watchdog_has_bitten = CSRStatus(
            size=1, 
            description="Watchdog has bitten.\nFlag which is set when timeout has occurred.", 
            name='watchdog_has_bitten'
        )
        # - Wall-clock
        self.wall_clock = CSRStatus(
            size=64, 
            description="Wall-clock.\n Counter which contains the amount of clock cycles which have "
            "been passed since the start of the device. The width of the counter is 64-bits, which "
            "means that a roll-over will practically never occur during the runtime of a "
            "machine (order of magnitude centuries at 1 GHz).",  
            name='wall_clock'
        )
        # Modules
        for module in config.modules:
            module.add_mmio_read_registers(self)
