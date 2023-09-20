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


class EtherboneConnection(BaseModel):
    connection_type: Literal[
        'etherbone'
    ] = 'etherbone'
    driver_files: ClassVar[List[str]] = [
        os.path.dirname(__file__) + '/../../../driver/boards/litexcnc_eth.c',
        os.path.dirname(__file__) + '/../../../driver/boards/litexcnc_eth.h',
        os.path.dirname(__file__) + '/../../../driver/boards/etherbone.c',
        os.path.dirname(__file__) + '/../../../driver/boards/etherbone.h'
    ]
    tx_delay: float = Field(
        None,
        help_text="Optional field to set the delay on the TX line."
    )
    rx_delay: float = Field(
        None,
        help_text="Optional field to set the delay on the RX line. Note: this "
        "parameter is not available on all EthPhy devices. In those cases, leave "
        "this field empty. The ColorLite FPGA card is one of those cards which "
        "does not support this field."
    )
    with_hw_init_reset: bool = Field(
        False,
        help_text="Hardware reset."
    )
    mac_address: int = Field(
        ...,
        help_text="The mac-address for the FPGA-card"
    )
    ip_address: IPv4Address = Field(
        "192.168.0.50",
        help_text="The ip-address to communicate with the FPGA-card."
    )

    @validator('mac_address', pre=True)
    def convert_mac_address(cls, value):
        return int(value, base=16)
