from ..boards.colorlight import ColorLightBase
from ..boards.rv901t import RV901T

def _add_etherbone_colorlight(soc, connection):
    
    from liteeth.phy.ecp5rgmii import LiteEthPHYRGMII
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

def _add_etherbone_rv901t(soc, connection):
    from liteeth.phy.s6rgmii import LiteEthPHYRGMII
        
    soc.ethphy = LiteEthPHYRGMII(
        clock_pads = soc.platform.request("eth_clocks", 0),
        pads       = soc.platform.request("eth", 0),
        **{key: value 
           for key, value
           in connection.dict(exclude={'module'}).items()
           if value is not None
           and key in ['tx_delay', 'rx_delay', 'with_hw_init_reset']
        }
    )
    soc.submodules += soc.ethphy
    soc.add_etherbone(
        phy=soc.ethphy,
        mac_address=connection.mac_address,
        ip_address=str(connection.ip_address),
        buffer_depth=255,
        data_width=32
    )

    # Timing constraints
    soc.platform.add_period_constraint(soc.ethphy.crg.cd_eth_rx.clk, 1e9/125e6)
    soc.platform.add_false_path_constraints(soc.submodules.crg.cd_sys.clk, soc.ethphy.crg.cd_eth_rx.clk)

ETHERBONE_PLATFORM = {
    ColorLightBase: _add_etherbone_colorlight, 
    RV901T: _add_etherbone_rv901t
}


def add_etherbone(soc, connection):
    """
    Adds etherbone to the board
    """
    if type(soc) not in ETHERBONE_PLATFORM:
        raise KeyError(f"Unknown platform {type(soc)}")
    ETHERBONE_PLATFORM[type(soc)](soc, connection)
