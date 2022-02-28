"""

"""
from pydantic import BaseModel, Field


class GPIO(BaseModel):
    pin: str = Field(
        description="The pin on the FPGA-card."
    )
    name: str = Field(
        None,
        description="The name of the pin as used in LinuxCNC HAL-file (optional). "
        "When not specified, the standard pin litexcnc.digital-in.x and "
        "litexcnc.digital-in.x.not will be created."
    )
    io_standard: str = Field(
        "LVCMOS33",
        description="The IO Standard (voltage) to use for the pin."
    )
