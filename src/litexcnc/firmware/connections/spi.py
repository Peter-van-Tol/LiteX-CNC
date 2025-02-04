from migen import *
from migen.fhdl.specials import Tristate, TSTriple
from migen.genlib.cdc import MultiReg

from litex.build.generic_platform import Subsignal, Pins, IOStandard
from litex.soc.integration.doc import ModuleDoc, AutoDoc
from litex.soc.interconnect import wishbone


class Spi4WireDocumentation(ModuleDoc):
    """4-Wire SPI Protocol

    The 4-wire SPI protocol does not require any pins to change direction, and
    is therefore suitable for designs with level-shifters or without GPIOs that
    can change direction.
    
    While waiting for the response, the ``MISO`` line remains high.  As soon as
    a response is available, the device pulls the `MISO` line low and clocks
    out either a ``0x00`` or `0x01` byte indicating whether it's a READ or a WRITE
    that is being answered.  Note that if the operation is fast enough, the
    device will not pull the `MISO` line high and will immediately respond
    with ``0x00`` or ``0x01``.

    You can abort the operation by driving ``CS`` high.  However, if a WRITE or
    READ has already been initiated then it will not be aborted.

    .. wavedrom::
        :caption: 4-Wire SPI Operation

        { "signal": [
            ["Read",
                {  "name": 'MOSI',        "wave": 'x23...x|xxxxxx', "data": '0x01 [ADDRESS]'},
                {  "name": 'MISO',        "wave": 'x.....x|25...x', "data": '0x01 [DATA]'   },
                {  "name": 'CS',          "wave": 'x0.....|.....x', "data": '1 2 3'},
                {  "name": 'data bits',   "wave": 'xx2222x|x2222x', "data": '31:24 23:16 15:8 7:0 31:24 23:16 15:8 7:0'}
            ],
            {},
            ["Write",
                {  "name": 'MOSI',        "wave": 'x23...3...x|xx', "data": '0x00 [ADDRESS] [DATA]'},
                {  "name": 'MISO',        "wave": 'x.........1|2x', "data": '0x00'   },
                {  "name": 'CS',          "wave": 'x0.........|.x', "data": '1 2 3'},
                {  "name": 'data bits',   "wave": 'xx22222222x|xx', "data": '31:24 23:16 15:8 7:0 31:24 23:16 15:8 7:0'}
            ]
        ]}
    """

class Spi3WireDocumentation(ModuleDoc):
    """3-Wire SPI Protocol

    The 3-wire SPI protocol repurposes the ``MOSI`` line for both data input and
    data output.  The direction of the line changes immediately after the
    address (for read) or the data (for write) and the device starts writing
    ``0xFF``.

    As soon as data is available (read) or the data has been written (write),
    the device drives the ``MOSI`` line low in order to clock out ``0x00``
    or ``0x01``.  This will always happen on a byte boundary.

    You can abort the operation by driving ``CS`` high.  However, if a WRITE or
    READ has already been initiated then it will not be aborted.

    .. wavedrom::
        :caption: 3-Wire SPI Operation

        { "signal": [
            ["Read",
                {  "name": 'MOSI',        "wave": 'x23...5|55...x', "data": '0x01 [ADDRESS] 0xFF 0x01 [DATA]'},
                {  "name": 'CS',          "wave": 'x0.....|.....x', "data": '1 2 3'},
                {  "name": 'data bits',   "wave": 'xx2222x|x2222x', "data": '31:24 23:16 15:8 7:0 31:24 23:16 15:8 7:0'}
            ],
            {},
            ["Write",
                {  "name": 'MOSI',        "wave": 'x23...3...5|50', "data": '0x00 [ADDRESS] [DATA] 0xFF 0x00'},
                {  "name": 'CS',          "wave": 'x0.........|.x', "data": '1 2 3'},
                {  "name": 'data bits',   "wave": 'xx22222222x|xx', "data": '31:24 23:16 15:8 7:0 31:24 23:16 15:8 7:0'}
            ]
        ]}
        """

class Spi2WireDocumentation(ModuleDoc):
    """2-Wire SPI Protocol

    The 2-wire SPI protocol removes the ``CS`` line in favor of a sync byte.
    Note that the 2-wire protocol has no way of interrupting communication,
    so if the bus locks up the device must be reset.  The direction of the
    data line changes immediately after the address (for read) or the data
    (for write) and the device starts writing ``0xFF``.

    As soon as data is available (read) or the data has been written (write),
    the device drives the ``MOSI`` line low in order to clock out ``0x00``
    or ``0x01``.  This will always happen on a byte boundary.

    All transactions begin with a sync byte of ``0xAB``.

    .. wavedrom::
        :caption: 2-Wire SPI Operation

        { "signal": [
            ["Write",
                {  "name": 'MOSI',        "wave": '223...5|55...', "data": '0xAB 0x01 [ADDRESS] 0xFF 0x01 [DATA]'},
                {  "name": 'data bits',   "wave": 'xx2222x|x2222', "data": '31:24 23:16 15:8 7:0 31:24 23:16 15:8 7:0'}
            ],
            {},
            ["Read",
                {  "name": 'MOSI',        "wave": '223...3...5|5', "data": '0xAB 0x00 [ADDRESS] [DATA] 0xFF 0x00'},
                {  "name": 'data bits',   "wave": 'xx22222222x|x', "data": '31:24 23:16 15:8 7:0 31:24 23:16 15:8 7:0'}
            ]
        ]}
        """

class SpiWishboneBridge(Module, ModuleDoc, AutoDoc):
    """Wishbone Bridge over SPI

    This module allows for accessing a Wishbone bridge over a {}-wire protocol.
    All operations occur on byte boundaries, and are big-endian.

    The device can take a variable amount of time to respond, so the host should
    continue polling after the operation begins.  If the Wishbone bus is
    particularly busy, such as during periods of heavy processing when the
    CPU's icache is empty, responses can take many thousands of cycles.

    The bridge core is designed to run at 1/4 the system clock.
    """
    COMMAND_BITS   = 2
    COMMAND_READ   = 0x1
    COMMAND_WRITE  = 0x2
    SIZE_BITS      = 6
    ADDRESS_WIDTH  = 4
    VALUE_WIDTH    = 4 



    def __init__(self, pads, wires=4, with_tristate=True, debug_led=None):
        self.wishbone = wishbone.Interface()

        # # #
        self.__doc__ = self.__doc__.format(wires)
        if wires == 4:
            self.mod_doc = Spi4WireDocumentation()
        elif wires == 3:
            self.mod_doc = Spi3WireDocumentation()
        elif wires == 2:
            self.mod_doc = Spi2WireDocumentation()

        clk  = Signal()
        cs_n = Signal()
        mosi = Signal()
        miso = Signal()
        miso_en = Signal()

        counter   = Signal(8)
        counter2   = Signal(8)
        write_offset = Signal(5)
        write_response = Signal(1)
        command   = Signal(self.COMMAND_BITS)
        address   = Signal(32)
        num_words = Signal(5)
        value     = Signal(32)
        wr        = Signal()
        sync_byte = Signal(8)

        self.specials += [
            MultiReg(pads.clk, clk),
        ]
        if wires == 2:
            io = TSTriple()
            self.specials += io.get_tristate(pads.mosi)
            self.specials += MultiReg(io.i, mosi)
            self.comb += io.o.eq(miso)
            self.comb += io.oe.eq(miso_en)
        elif wires == 3:
            self.specials += MultiReg(pads.cs_n, cs_n),
            io = TSTriple()
            self.specials += io.get_tristate(pads.mosi)
            self.specials += MultiReg(io.i, mosi)
            self.comb += io.o.eq(miso)
            self.comb += io.oe.eq(miso_en)
        elif wires == 4:
            self.specials += MultiReg(pads.cs_n, cs_n),
            self.specials += MultiReg(pads.mosi, mosi)
            if with_tristate:
                self.specials += Tristate(pads.miso, miso, ~cs_n)
            else:
                self.comb += pads.miso.eq(miso)
        else:
            raise ValueError("`wires` must be 2, 3, or 4")

        clk_last = Signal()
        clk_rising = Signal()
        clk_falling = Signal()
        self.sync += clk_last.eq(clk)
        self.comb += clk_rising.eq(clk & ~clk_last)
        self.comb += clk_falling.eq(~clk & clk_last)

        fsm = FSM(reset_state="IDLE")
        fsm = ResetInserter()(fsm)
        self.submodules += fsm
        self.comb += fsm.reset.eq(cs_n)

        if debug_led is not None:
            self.comb += debug_led[0].eq(cs_n)

        # Connect the Wishbone bus up to our values
        self.comb += [
            self.wishbone.adr.eq(address[2:]),
            self.wishbone.dat_w.eq(value),
            self.wishbone.sel.eq(2**len(self.wishbone.sel) - 1)
        ]

        # Constantly have the counter increase, except when it's reset
        # in the IDLE state
        self.sync += If(cs_n, counter.eq(0)).Elif(clk_rising, counter.eq(counter + 1))
        self.sync += If(cs_n, counter2.eq(0))

        if wires == 2:
            fsm.act("IDLE",
                miso_en.eq(0),
                NextValue(miso, 1),
                If(clk_rising,
                    NextValue(sync_byte, Cat(mosi, sync_byte))
                ),
                If(sync_byte[0:7] == 0b101011,
                    NextState("GET_COMMAND"),
                    NextValue(counter, 0),
                    NextValue(command, mosi),
                )
            )
        elif wires == 3 or wires == 4:
            fsm.act("IDLE",
                miso_en.eq(0),
                NextValue(miso, 1),
                NextValue(write_response, 1),
                If(clk_rising,
                    NextState("GET_COMMAND"),
                    NextValue(command, mosi),
                ),
            )
        else:
            raise ValueError("invalid `wires` count: {}".format(wires))

        # Determine if it's a read or a write
        fsm.act("GET_COMMAND",
            miso_en.eq(0),
            NextValue(miso, 1),
            If(counter == self.COMMAND_BITS,
                # Write value
                If(command == self.COMMAND_WRITE,
                    NextValue(wr, 1),
                    NextState("READ_NUM_BYTES"),
                # Read value
                ).Elif(command == self.COMMAND_READ,
                    NextValue(wr, 0),
                    NextState("READ_NUM_BYTES"),
                ).Else(
                    NextState("END"),
                ),
            ),
            If(clk_rising,
                NextValue(command, Cat(mosi, command)),
            ),
        )

        fsm.act("READ_NUM_BYTES",
            miso_en.eq(0),
            If(counter == self.COMMAND_BITS + self.SIZE_BITS,
                NextState("READ_ADDRESS"),
            ),
            If(clk_rising,
                NextValue(num_words, Cat(mosi, num_words)),
            ),
        )

        fsm.act("READ_ADDRESS",
            miso_en.eq(0),
            If(counter == self.COMMAND_BITS + self.SIZE_BITS + self.ADDRESS_WIDTH * 8,
                If(wr,
                    NextState("READ_VALUE"),
                ).Else(
                    NextState("READ_WISHBONE"),
                )
            ),
            If(clk_rising,
                NextValue(address, Cat(mosi, address)),
            ),
        )

        fsm.act("READ_VALUE",
            miso_en.eq(0),
            If(counter2 == self.VALUE_WIDTH * 8,
                NextState("WRITE_WISHBONE"),
            ),
            If(clk_rising,
                NextValue(counter2, counter2 + 1),
                NextValue(value, Cat(mosi, value)),
            ),
        )

        fsm.act("WRITE_WISHBONE",
            self.wishbone.stb.eq(1),
            self.wishbone.we.eq(1),
            self.wishbone.cyc.eq(1),
            miso_en.eq(1),
            If(self.wishbone.ack | self.wishbone.err,
                NextValue(num_words, num_words - 1),
                If(
                    num_words == 1,  # NOTE: the updating of num_bytes is not sequential, it happens in the next loop
                    NextState("CHECK_BYTE_BOUNDARY")  
                ).Else(
                    NextValue(address, address + self.ADDRESS_WIDTH),
                    NextValue(counter2, 0),
                    NextState("READ_VALUE")
                )
            ),
        )

        fsm.act("READ_WISHBONE",
            self.wishbone.stb.eq(1),
            self.wishbone.we.eq(0),
            self.wishbone.cyc.eq(1),
            miso_en.eq(1),
            If(self.wishbone.ack | self.wishbone.err,
                NextValue(value, self.wishbone.dat_r),
                NextState("CHECK_BYTE_BOUNDARY")         
            ),
        )

        fsm.act("CHECK_BYTE_BOUNDARY",
            If(
                counter[0:3] == 0,
                NextValue(miso, 0),
                    If(wr,
                        NextState("WRITE_RESPONSE"),
                    ).Else(
                        NextState("WRITE_DATA"),
                    ),
            ).Else(
                NextState("WAIT_BYTE_BOUNDARY")  
            )
        )

        fsm.act("WAIT_BYTE_BOUNDARY",
            miso_en.eq(1),
            If(clk_falling,
                If(counter[0:3] == 0,
                    NextValue(miso, 0),
                    If(wr,
                        NextState("WRITE_RESPONSE"),
                    ).Else(
                        NextState("WRITE_DATA"),
                    ),
                ),
            ),
        )

        fsm.act("WRITE_DATA",
            If(
                write_response,
                NextState("WRITE_RESPONSE"),
            ).Else(
                NextValue(write_offset, self.VALUE_WIDTH * 8 - 1),
                NextState("WRITE_VALUE")
            )
        )

        # Write the "01" byte that indicates a response
        fsm.act("WRITE_RESPONSE",
            miso_en.eq(1),
            If(clk_falling,
                If(counter[0:3] == 0b111,
                    NextValue(miso, 1),
                    NextValue(write_response, 0),
                ).Elif((counter[0:3] == 0) & (write_response == 0),
                    If(wr,
                        NextState("END")
                    ).Else(
                        NextValue(write_offset, self.VALUE_WIDTH * 8 - 1),
                        NextState("WRITE_VALUE")
                    )
                ),
            ),
        )

        # Write the actual value
        fsm.act("WRITE_VALUE",
            miso_en.eq(1),
            NextValue(miso, value >> write_offset),
            If(clk_falling,
                NextValue(write_offset, write_offset - 1),
                If(write_offset == 0,
                    NextValue(num_words, num_words - 1),
                    If(
                        num_words == 1,  # NOTE: the updating of num_bytes is not sequential, it happens in the next loop
                        NextValue(miso, 0),
                        NextState("END")
                    ).Else(
                        NextValue(address, address + self.ADDRESS_WIDTH),
                        NextState("READ_WISHBONE")
                    )
                ),
            ),
        )

        if wires == 3 or wires == 4:
            fsm.act("END",
                NextValue(miso, 1),
                miso_en.eq(1),
            )
        elif wires == 2:
            fsm.act("END",
                miso_en.eq(0),
                NextValue(sync_byte, 0),
                NextState("IDLE")
            )
        else:
            raise ValueError("invalid `wires` count: {}".format(wires))


def add_spi(soc, connection):
    """
    Adds spi connection to the board
    """
    soc.platform.add_extension([
        ("spi", 0, 
            Subsignal("mosi", Pins(connection.mosi), IOStandard(connection.io_standard)),
            Subsignal("miso", Pins(connection.miso), IOStandard(connection.io_standard)),
            Subsignal("clk",  Pins(connection.clk),  IOStandard(connection.io_standard)),
            Subsignal("cs_n", Pins(connection.cs_n), IOStandard(connection.io_standard)),
        )
    ])
    spi_pads = soc.platform.request("spi")

    soc.spi_cd = ClockDomain("clk_125")
    soc.clock_domains += soc.spi_cd
    soc.comb += [
        soc.spi_cd.clk.eq(ClockSignal("sys")),
        soc.spi_cd.rst.eq(ResetSignal("sys"))
    ]
    # timing constraints
    # soc.platform.add_period_constraint(soc.spi_cd.clk, 1e9/125e6)
    # soc.platform.add_false_path_constraints(soc.crg.cd_sys.clk, soc.spi_cd.clk)

    soc.submodules.spibone = ClockDomainsRenamer("clk_125")(SpiWishboneBridge(spi_pads, debug_led=soc.platform.request("user_led_n")))
    soc.add_wb_master(soc.spibone.wishbone)


