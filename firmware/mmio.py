import binascii
from os import major

from random import setstate
from typing import List
import math
from packaging.version import Version

# Import from litex
from migen.fhdl.module import Module
from litex.soc.interconnect.csr import AutoCSR, CSRStatus, CSRStorage, CSRConstant
from migen import *

# Local imports
from . import __version__
from .encoder import EncoderModule
from .gpio import GPIO_Out, GPIO_In
from .stepgen import StepgenModule
from typing import TYPE_CHECKING
if TYPE_CHECKING:
    from .soc import LitexCNC_Firmware

# Convert the version of the firmware to 
version = Version(__version__)


class MMIO(Module, AutoCSR):

    def __init__(self, config: 'LitexCNC_Firmware', fingerprint):
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
            size=32,
            reset= (version.major << 16) + (version.minor << 8) + version.micro,
            description="Version of the firmware. The major and minor parts of the version must "
            "be equal between firmware and driver. If there is a difference, the user must update "
            "either the driver or firmware. A difference in patch is allowable, as the data protocol "
            "in this case is not altered. A difference in patch can happen for example when a small "
            "bug has been fixed in the driver or firmware." 
        )
        self.fingerprint = CSRStatus(
            size=32,
            reset=fingerprint,
            description="The CRC of the configuration file used to create this firmware. Used to "
            "ensure the driver uses the same configuration file for initiating the communication."
        )
        self.reset = CSRStorage(
            size=1, 
            description="Reset.\nWhile True (set to 1) the card is being forced in reset-mode. In "
            "reset-mode the position and speed of the steppers is reset to the starting position. "
            "When starting the driver, the first step is to reset it, to prevent erronous behaviour "
            "of the steppers.", 
            name='reset'
        )

        # OUTPUT (as seen from the PC!)
        # - Watchdog
        self.watchdog_data = CSRStorage(
            size=32, 
            description="Watchdog data.\nByte containing the enabled flag (bit 31) and the time (bit 30 - 0)."
            "out in cpu cycles.", 
            name='watchdog_data',
            write_from_dev=True
        )
        # - GPIO
        GPIO_Out.add_mmio_write_registers(self, config.gpio_out)
        # - PWM
        if config.pwm:
            for index, _ in enumerate(config.pwm):
                setattr(self, f'pwm_{index}_enable', CSRStorage(size=32, description=f'pwm_{index}_enable', name=f'pwm_{index}_enable', write_from_dev=False))
                setattr(self, f'pwm_{index}_period', CSRStorage(size=32, description=f'pwm_{index}_period', name=f'pwm_{index}_period', write_from_dev=False))
                setattr(self, f'pwm_{index}_width', CSRStorage(size=32, description=f'pwm_{index}_width', name=f'pwm_{index}_width', write_from_dev=False))
        # - Stepgen
        StepgenModule.add_mmio_write_registers(self, config.stepgen)
        # - Encoder (new style!)
        EncoderModule.add_mmio_write_registers(self, config.encoders)

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
        GPIO_In.add_mmio_read_registers(self, config.gpio_in)
        StepgenModule.add_mmio_read_registers(self, config.stepgen)
        EncoderModule.add_mmio_read_registers(self, config.encoders)
