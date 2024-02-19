# Default imports
import math

# Imports for Migen
from migen import *
from migen.genlib.cdc import MultiReg
from litex.build.generic_platform import IOStandard, Pins, Subsignal
from litex.soc.interconnect.csr import CSRStatus, CSRStorage
from litex.soc.integration.doc import AutoDoc, ModuleDoc


class ShiftOutModule(Module, AutoDoc):

    def __init__(self, data_width, cycle_counter, pads=None):
        
        self.enable = Signal()
        self.enable_delayed = Signal()
        self.data = Signal(data_width)
        self.data_counter = Signal(max=data_width)
        self.counter = Signal(max=cycle_counter)

        # Signals for the shift register
        self.clock = Signal()
        self.serial = Signal()
        self.latch = Signal()
        self.clear = Signal(reset=1)

        # Connect the signals to the pads
        if pads is not None:
            self.specials += [
                MultiReg(pads.clock, self.clock),
                MultiReg(pads.data, self.serial),
                MultiReg(pads.latch, self.latch),
                MultiReg(pads.clear, self.clear)
            ]

        # Create a finite state machine to shift out the data
        fsm = FSM(reset_state="IDLE")
        fsm = ResetInserter()(fsm)
        self.submodules += fsm

        fsm.act(
            # Waiting for new data to shift out and watching the enable signal
            # to become low.
            "IDLE",
            If(
                ~self.enable,
                NextValue(self.counter, 2 * cycle_counter)
                NextState("DISABLE"),
            ).Elif(
                data_counter > 0,
                NextState("SHIFT_OUT")
            )
        )

        fsm.act(
            # Shifting the data out
            "SHIFT_OUT",
            If(
                self.counter == 0,
                If(
                    self.data_counter == 0,
                    NextValue(self.serial, 0),
                    NextValue(self.counter, cycle_counter),
                    NextState("LATCH")
                ).Else(
                    NextValue(self.data_counter, self.data_counter-1),
                    NextValue(self.clear, 1),
                    NextValue(self.serial, self.data[0]),
                    NextValue(self.data, self.data >> 1),
                    NextValue(self.counter, cycle_counter),
                )
            ).Else(
                If(
                    self.counter > cycle_counter,
                    NextValue(self.clock, 0),
                    NextValue(self.data, self.data[0]),
                ).Else(
                    NextValue(self.clock, 1),
                    NextValue(self.data, self.data[0]),
                ),
                NextValue(self.counter, self.counter-1)
            )
        )

        fsm.act(
            # Latching the data
            "LATCH",
            If(
                # Don't latch when the enable signal is low
                ~self.enable,
                NextValue(self.counter, cycle_counter),
                NextState("DISABLE")
            ).Elif(
                # Latch is finished, proceed to idle state
                self.counter == 0,
                NextState("IDLE"),
            ).Else(
                If(
                    self.counter > cycle_counter,
                    NextValue(self.latch, 1)
                ).Else(
                    NextValue(self.latch, 0)
                )
                NextValue(self.counter, self.counter-1)
            )
        )

        fsm.act(
            # Disabling the shift register and clearing the data
            "DISABLE",
            If(
                self.counter == 0,
                NextValue(self.latch, 1),
                NextValue(self.clear, 1),
                If(
                    self.enable,
                    NextState("IDLE"),
                )
            ).Else(
                If(
                    self.counter > cycle_counter,
                    NextValue(self.latch, 0),
                    NextValue(self.clear, 0)
                ).Else(
                    NextValue(self.latch, 1),
                    NextValue(self.clear, 0)
                )
                NextValue(self.counter, self.counter-1)
            )            
        )


    @classmethod
    def add_mmio_write_registers(cls, mmio, config: 'ShiftOutModuleConfig'):
        """
        Adds the storage registers to the MMIO.

        NOTE: Storage registers are meant to be written by LinuxCNC and contain
        the flags and configuration for the module.
        """
        # Don't create the registers when the config is empty (no shift_out
        # defined in this case)
        if not config:
            return

        for index in range(len(config.instances)):
            setattr(
                mmio,
                f'shift_out_{index}_data', 
                CSRStorage(
                    size=32, 
                    description=f'The data to be shifted out for channel {index}.', 
                    name=f'shift_out_{index}_data', 
                    write_from_dev=False
                )
            )

    @classmethod
    def create_from_config(cls, soc: SoC, watchdog, config: 'ShiftOutModuleConfig'):
        """
        Adds the module as defined in the configuration to the SoC.

        NOTE: the configuration must be a list and should contain all the module at
        once. Otherwise naming conflicts will occur.
        """
        # Don't create the module when the config is empty (no shift_out 
        # defined in this case)
        if not config:
            return
        
        for index, config in enumerate(config.instances):
            # Create the instance, using subsignals for the different pins
            soc.platform.add_extension([
                ("shift_out", 
                 index,
                 Subsignal("clock", Pins(config.pin_clock), IOStandard(config.io_standard)),
                 Subsignal("data", Pins(config.pin_data), IOStandard(config.io_standard)),
                 Subsignal("latch", Pins(config.pin_latch), IOStandard(config.io_standard))
                )
            ])

            # Add the PWM-module to the platform
            _shift_out = ShiftOutModule(
                data_width=config.data_width,
                cycle_counter=soc.clock_frequency // (config.frequency * 2),
                pads=soc.platform.request("shift_out", index)
            )
            soc.submodules += _shift_out

            soc.sync += [
                If(
                    soc.MMIO_inst.reset.storage | soc.MMIO_inst.watchdog_has_bitten.status,
                    # Stop the shift register from shifting
                    _shift_out.enable.eq(0),
                ).Else(
                    # (Re-)start the shift register
                    _shift_out.enable.eq(1),
                    # Wait until the data has been received
                    If(
                        getattr(soc.MMIO_inst, f'shift_out_{index}_data').re,
                        _shift_out.data_counter.eq(config.data_width),
                        _shift_out.data.eq(getattr(soc.MMIO_inst, f'shift_out_{index}_data').storage)                       
                    )
                )
            ]


if __name__ == "__main__":
    from migen import *
    from migen.fhdl import *

    def test_shift_out(shift_out: ShiftOutModule):
        i = 0
        # Setup the stepgen
        yield (shift_out.enable.eq(1))
        print("Clock;Serial;Latch;Clear")

        while(1):
            # After 10 cycles set the data
            if i == 10:
                yield (shift_out.data_counter.eq(4))
                yield (shift_out.data.eq(0b1010))
            
            # In each step, print out the state of each parameter
            clock = (yield shift_out.clock)
            serial = (yield shift_out.serial)
            latch = (yield shift_out.latch)
            clear = (yield shift_out.clear)
            print(f"{clock};{serial};{latch};{clear}")

            # Wait for the next cycle
            yield
            i+=1

            # Stop after 100 cycles, should be enough for a nibble to be shifted out
            if i > 100000:
                break

    stepgen = ShiftOutModule(data_width=4, cycle_counter=3, pads=None)
    print("\nRunning Sim...\n")
    # print(verilog.convert(stepgen, stepgen.ios, "pre_scaler"))
    run_simulation(stepgen, test_shift_out(stepgen))
