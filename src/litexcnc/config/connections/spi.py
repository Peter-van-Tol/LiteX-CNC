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
