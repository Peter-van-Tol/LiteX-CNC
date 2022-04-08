from random import setstate
from typing import List
import math

# Import from litex
from migen.fhdl.module import Module
from litex.soc.interconnect.csr import AutoCSR, CSRStatus, CSRStorage

# Local imports
from .gpio import GPIO
from .pwm import PWM


class MMIO(Module, AutoCSR):

    def __init__(self, gpio_in: List[GPIO], gpio_out: List[GPIO], pwm: List[PWM]):
        """
        Initializes the memory registers.

        NOTE:
        The order of the registers in the memory is in the same order as defined in
        this class. This can be inspected by reviewing the generated csr.csv. The driver
        expects the information blocks in the following order:
        - READ:
          - GPIO;
          - PWM;
        - WRITE:
          - GPIO;

        When the order of the MMIO is mis-aligned with respect to the driver this might
        lead to errors (writing to the wrong registers) or the FPGA being hung up (when
        writing to a read-only register).
        """
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
        self.gpio_out = CSRStorage(size=int(math.ceil(float(len(gpio_out))/32))*32, description="gpio_out", name='gpio_out', write_from_dev=False)
        # - PWM
        for index, _ in enumerate(pwm):
            setattr(self, f'pwm_{index}_enable', CSRStorage(size=32, description=f'pwm_{index}_enable', name=f'pwm_{index}_enable', write_from_dev=False))
            setattr(self, f'pwm_{index}_period', CSRStorage(size=32, description=f'pwm_{index}_period', name=f'pwm_{index}_period', write_from_dev=False))
            setattr(self, f'pwm_{index}_width', CSRStorage(size=32, description=f'pwm_{index}_width', name=f'pwm_{index}_width', write_from_dev=False))

        # INPUT (as seen from the PC!)
        # - Watchdog
        self.watchdog_has_bitten = CSRStatus(
            size=1, 
            description="Watchdog has bitten.\nFlag which is set when timeout has occurred.", 
            name='watchdog_has_bitten'
        )
        # - GPIO
        self.gpio_in = CSRStatus(size=int(math.ceil(float(len(gpio_in))/32))*32, description="gpio_in", name='gpio_in')
