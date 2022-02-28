from typing import List

# Import from litex
from migen.fhdl.module import Module
from litex.soc.interconnect.csr import AutoCSR, CSRStatus, CSRStorage

# Local imports
from .gpio import GPIO


class MMIO(Module, AutoCSR):

    def __init__(self, gpio_in: List[GPIO], gpio_out: List[GPIO]):
        self.gpio_in = CSRStatus(size=32, description="gpio_in", name='gpio_in')
        self.gpio_out = CSRStorage(size=32, description="gpio_out", name='gpio_out', write_from_dev=False)