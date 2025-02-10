from liteeth.phy.ecp5rgmii import LiteEthPHYRGMII


def add_etherbone(soc, connection):
    """Adds Etherbone to the Colorlight board.

    Args:
        soc (ColorLight): The FPGA to add the connection to
        connection (EtherboneConnection): The configuration of the Etherbone connection
    """
    
    soc.submodules.ethphy = LiteEthPHYRGMII(
        clock_pads = soc.platform.request("eth_clocks"),
        pads       = soc.platform.request("eth"),
        **{key: value 
           for key, value
           in connection.dict(exclude={'module'}).items()
           if value is not None
           and key in ['tx_delay', 'rx_delay', 'with_hw_init_reset']
        }
    )
    soc.add_etherbone(
        phy=soc.ethphy,
        mac_address=connection.mac_address,
        ip_address=str(connection.ip_address),
        buffer_depth=255,
        data_width=32
    )
