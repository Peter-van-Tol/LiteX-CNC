"""

"""
from ipaddress import IPv4Address
import os
try:
    from typing import ClassVar, List, Literal
except ImportError:
    from typing import ClassVar, List
    from typing_extensions import Literal


from pydantic import BaseModel, Field, validator


class SPIboneConnection(BaseModel):
    connection_type: Literal[
        'spi'
    ] = 'spi'
    driver_files: ClassVar[List[str]] = [
        os.path.dirname(__file__) + '/../../../driver/boards/litexcnc_spi.c',
        os.path.dirname(__file__) + '/../../../driver/boards/litexcnc_spi.h'
    ]
    mosi: str = Field(
          ...,
          description="The pin on the FPGA-card used for Master Out Slave In (MOSI)."
    )
    miso: str = Field(
          ...,
          description="The pin on the FPGA-card used for Master In Slave Out (MISO)."
    )
    clk: str = Field(
          ...,
          description="The pin on the FPGA-card used for the clock-signal."
    )
    cs_n: str = Field(
          ...,
          description="The pin on the FPGA-card used for the chip-select."
    )
    io_standard: str = Field(
        "LVCMOS33",
        description="The IO Standard (voltage) to use for the pin."
    )
