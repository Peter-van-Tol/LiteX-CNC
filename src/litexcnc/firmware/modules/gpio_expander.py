import math

from litex.soc.integration.doc import AutoDoc, ModuleDoc
from migen import If, FSM, Module, NextState, NextValue, Record, ResetInserter, Signal


class GpioExpander74HCT595Module(Module, AutoDoc):
    """Port expansion with 74HCT595.
    """
    pads_layout = [("serial_data", 1), ("clock", 1), ("latch", 1), ("clear", 1)]

    def __init__(self, length: int, pads=None):
        
        # Create pins for the output
        self.serial_data = Signal()
        self.clock = Signal()
        self.latch = Signal()
        self.clear = Signal()
        self.enable = Signal()

        # Register with the data which will be clocked out
        self.data = Signal(length)
        # self.data = Signal(8, reset=0x0F)
        self.data_counter = Signal(math.ceil(math.log2(length)), reset=length-1)
        self.counter = Signal(math.ceil(math.log2(500+1)), reset=500)

        # Store the pads and connect them to the signals
        if pads is None:
            pads = Record(self.pads_layout)
        self.pads = pads
        self.comb += [
            self.pads.serial_data.eq(self.serial_data),
            self.pads.clock.eq(self.clock),
            self.pads.latch.eq(self.latch),
            self.pads.clear.eq(self.clear),
        ]

        # Create a state machine
        fsm = FSM(reset_state="IDLE")
        fsm = ResetInserter()(fsm)
        self.submodules += fsm
        # - reset the state machine when it is not enabled
        self.comb += fsm.reset.eq(~self.enable)

        fsm.act(
            "IDLE",
            self.clear.eq(1),
            self.clock.eq(1),
            self.latch.eq(0),
            If(self.enable,
                NextState("ENABLE"),
                NextValue(self.counter, self.counter.reset),
                NextValue(self.data_counter, self.data_counter.reset)
            ),
        )
        fsm.act(
            "ENABLE",
            self.clear.eq(0),
            self.latch.eq(0),
            NextState("SET_DATA"),
        )
        fsm.act(
            "SET_DATA",
            self.clear.eq(0),
            self.clock.eq(0),
            self.latch.eq(0),
            NextValue(self.serial_data, self.data >> (self.data_counter)),
            NextValue(self.counter, self.counter-1),
            If(self.counter == 0,
                NextState("SET_CLOCK"),
                NextValue(self.counter, self.counter.reset),
            )
        )
        fsm.act(
            "SET_CLOCK",
            self.clear.eq(0),
            self.clock.eq(1),
            self.latch.eq(0),
            NextValue(self.counter, self.counter-1),
            If(self.counter == 0,
                If(self.data_counter == 0,
                    NextValue(self.data_counter, self.data_counter.reset),
                    NextState('LATCH')
                ).Else(
                    NextValue(self.data_counter, self.data_counter-1),
                    NextState("SET_DATA"),
                ),
                NextValue(self.counter, self.counter.reset)
            )
        )
        fsm.act(
            "LATCH",
            self.clear.eq(0),
            self.clock.eq(0),
            self.latch.eq(1),
            NextValue(self.counter, self.counter-1),
            If(self.counter == 0,
                NextState("SET_DATA"),
                NextValue(self.counter, self.counter.reset),
                self.latch.eq(0),
            )
        )


if __name__ == "__main__":
    from migen import *
    from migen.fhdl import *

    def test_portexpander(port_expander: GpioExpander74HCT595Module):
        i = 0
        # Setup the stepgen
        yield (port_expander.data.eq(0b10111011))
        yield (port_expander.enable.eq(1))

        while(1):

            serial_data = (yield port_expander.serial_data)
            clock = (yield port_expander.clock)
            latch = (yield port_expander.latch)
            clear = (yield port_expander.clear)
        
            print(f"{i},{clock},{serial_data},{latch},{clear}")
            yield
            i+=1
            if i > 150:
                break

    stepgen = GpioExpander74HCT595Module(length=8)
    print("\nRunning Sim...\n")
    run_simulation(stepgen, test_portexpander(stepgen))
