# Imports for creating a json-definition
from typing import List
from pydantic import BaseModel, Field

# Imports for creating a LiteX/Migen module
from litex.soc.interconnect.csr import *
from migen import *
from litex.soc.integration.soc import SoC
from litex.soc.integration.doc import AutoDoc, ModuleDoc
from litex.build.generic_platform import *


class StepgenConfig(BaseModel):
    step_pin: str = Field(
        description="The pin on the FPGA-card for the step signal."
    )
    dir_pin: str = Field(
        None,
        description="The pin on the FPGA-card for the dir signal."
    )
    name: str = Field(
        None,
        description="The name of the stepgen as used in LinuxCNC HAL-file (optional). "
    )
    soft_stop: bool = Field(
        False,
        description="When False, the stepgen will directly stop when the stepgen is "
        "disabled. When True, the stepgen will stop the machine with respect to the "
        "acceleration limits and then be disabled. Default value: False."
    )
    io_standard: str = Field(
        "LVCMOS33",
        description="The IO Standard (voltage) to use for the pins."
    )


class StepgenCounter(Module, AutoDoc):

    def __init__(self) -> None:

        self.intro = ModuleDoc("""
        Simple counter which counts down as soon as the Signal
        `counter` has a value larger then 0. Designed for the
        several timing components of the StepgenModule.
        """)

        # Create a 32 bit counter which counts down
        self.counter = Signal(32)
        self.sync += If(
            self.counter > 0,
            self.counter.eq(self.counter - 1)
        )


class StepgenModule(Module, AutoDoc):
    pads_layout = [("step", 1), ("dir", 1)]

    def __init__(self, pads, pick_off, soft_stop) -> None:

        self.intro = ModuleDoc("""

        Timing parameters:
        There are five timing parameters which control the output waveform.
        No step type uses all five, and only those which will be used are
        exported to HAL.  The values of these parameters are in nano-seconds,
        so no recalculation is needed when changing thread periods.  In
        the timing diagrams that follow, they are identfied by the
        following numbers:
        (1): 'steplen' = length of the step pulse
        (2): 'stepspace' = minimum space between step pulses, space is dependent
        on the commanded speed. The check whether the minimum step space is obeyed
        is done in the driver
        (3): 'dirhold_time' = minimum delay after a step pulse before a direction
        change - may be longer
        (4): 'dir_setup_time' = minimum delay after a direction change and before
        the next step - may be longer

                   _____         _____               _____
        STEP  ____/     \_______/     \_____________/     \______
                  |     |       |     |             |     |
        Time      |-(1)-|--(2)--|-(1)-|--(3)--|-(4)-|-(1)-|
                                              |__________________
        DIR   ________________________________/

        Improvements on LinuxCNC stepgen.c:
        - When the machine is at rest and starts a commanded move, it can be moved
          the opposite way. This means that the dir-signal is toggled and thus a wait
          time is applied before the step-pin is toggled.
        - When changing direction between two steps, it is not necessary to wait. That's
          why there are signals for DDS (1+3+4) and for wait. Only when a step is
          commanded during the DDS period, the stepgen is temporarily paused by setting
          the wait-Signal HIGH.
        """
        )
        # Require to test working with Verilog, basically creates extra signals not
        # connected to any pads.
        if pads is None:
            pads = Record(self.pads_layout)

        # Values which determine the spacing of the step. These
        # are used to reset the counters.
        # - signals
        self.steplen = Signal(32)
        self.dir_hold_time = Signal(32)
        self.dir_setup_time = Signal(32)
        # - counters
        self.steplen_counter = StepgenCounter()
        self.dir_hold_counter = StepgenCounter()
        self.dir_setup_counter = StepgenCounter()
        self.submodules += [
            self.steplen_counter,
            self.dir_hold_counter,
            self.dir_setup_counter
        ]
        self.hold_dds = Signal()
        self.wait = Signal()
        self.reset = Signal()

        # Output parameters
        self.step = Signal()
        self.step_prev = Signal()
        self.dir = Signal(reset=True)

        # Main parameters for position, speed and acceleration
        self.enable = Signal()
        self.position = Signal(64)
        self.speed = Signal(32, reset=0x80000000)
        self.speed_target = Signal(32, reset=0x80000000)
        self.accel_counter1 = Signal(32)
        self.accel_counter2 = Signal(32)
        self.max_acceleration = Signal(32)

        # Link step and dir
        self.comb += [
            pads.dir.eq(self.step),
            pads.dir.eq(self.dir),
        ]

        # Optionally, use a different clock domain
        sync = self.sync

        # Determine the next speed, while taking into account acceleration limits if
        # applied. The speed is not updated when the direction has changed and we are
        # still waiting for the dir_setup to time out.
        sync += If(
            ~self.reset & ~self.wait,
            # When the machine is not enabled, the speed is clamped to 0. This results in a
            # deceleration when the machine is disabled while the machine is running,
            # preventing possible damage.
            If(
                ~self.enable,
                self.speed_target.eq(0x80000000)
            ),
            If(
                self.max_acceleration == 0,
                # Case: no maximum acceleration defined, directly apply the requested speed
                self.speed.eq(self.speed_target)
            ).Else(
                # Case: obey the maximum acceleration / deceleration
                If(
                    # Accelerate, difference between actual speed and target speed is too
                    # large to bridge within one clock-cycle
                    self.speed_target > (self.speed + self.max_acceleration[16:32]),
                    # The counters are again a fixed point arithmetric. Every loop we keep
                    # the fraction and add the integer part to the speed. The fraction is
                    # used as a starting point for the next loop.
                    self.accel_counter2.eq(self.accel_counter1 + self.max_acceleration),
                    self.speed.eq(self.speed + self.accel_counter2[16:32]),
                    self.accel_counter1.eq(self.accel_counter2[0:16])
                ).Elif(
                    # Decelerate, difference between actual speed and target speed is too
                    # large to bridge within one clock-cycle
                    self.speed_target < (self.speed - self.max_acceleration[16:32]),
                    # The counters are again a fixed point arithmetric. Every loop we keep
                    # the fraction and add the integer part to the speed. However, we have
                    # keep in mind we are subtracting now every loop
                    self.accel_counter2.eq(self.accel_counter1 - self.max_acceleration),
                    If(
                        self.accel_counter2[16:32] != 0,
                        self.speed.eq(self.speed - (0xFFFF - self.accel_counter2[16:32] + 1))
                    ),
                    self.accel_counter1.eq(self.accel_counter2[0:16])
                ).Else(
                    # Small difference between speed and target speed, gap can be bridged within
                    # one clock cycle.
                    self.speed.eq(self.speed_target)
                )
            )
        )

        # Reset algorithm.
        # NOTE: RESETTING the stepgen will not adhere the speed limit and will bring the stepgen
        # to an abrupt standstill
        sync += If(
            self.reset,
            self.speed_target.eq(0x80000000),
            self.speed.eq(0x80000000),
            self.position.eq(0),
        )

        # Update the position
        if soft_stop:
            sync += If(
                # Only check we are not waiting for the dir_setup. When the system is disabled, the
                # speed is set to 0 (with respect to acceleration limits) and the machine will be
                # stopped when disabled.
                ~self.reset & ~self.wait,
                self.position.eq(self.position + self.speed - 0x80000000)
            )
        else:
            sync += If(
                # Check whether the system is enabled and we are not waiting for the dir_setup
                ~self.reset & self.enable & ~self.wait,
                self.position.eq(self.position + self.speed - 0x80000000)
            )

        # Translate the position to steps by looking at the n'th bit (pick-off)
        sync += If(
            self.position[pick_off] != self.step_prev,
            # Corner-case: The machine is at rest and starts to move in the opposite
            # direction. Wait with stepping the machine until the dir setup time has
            # passed.
            If(
                ~self.hold_dds,
                # The relevant bit has toggled, make a step to the next position by
                # resetting the counters
                self.step_prev.eq(self.position[pick_off]),
                self.steplen_counter.counter.eq(self.steplen),
                self.dir_hold_counter.counter.eq(self.steplen + self.dir_hold_time),
                self.dir_setup_counter.counter.eq(self.steplen + self.dir_hold_time + self.dir_setup_time),
                self.wait.eq(False)
            ).Else(
                self.wait.eq(True)
            )
        )
        # Reset the DDS flag when dir_setup_counter has lapsed
        sync += If(
            self.dir_setup_counter.counter == 0,
            self.hold_dds.eq(0)
        )

        # Convert the parameters to output of step and dir
        # - step
        sync += If(
            self.steplen_counter.counter > 0,
            self.step.eq(1)
        ).Else(
            self.step.eq(0)
        )
        # - dir
        sync += If(
            self.dir != (self.speed[31]),
            # Enable the Hold DDS, but wait with changing the dir pin until the
            # dir_hold_counter has been elapsed
            self.hold_dds.eq(1),
            # Corner-case: The machine is at rest and starts to move in the opposite
            # direction. In this case the dir pin is toggled, while a step can follow
            # suite. We wait in this case the minimum dir_setup_time
            If(
                self.dir_setup_counter.counter == 0,
                self.dir_setup_counter.counter.eq(self.dir_setup_time)
            ),
            If(
                self.dir_hold_counter.counter == 0,
                self.dir.eq(self.speed[31])
            )
        )

        # Create the outputs
        self.ios = {self.step, self.dir}


    @classmethod
    def add_mmio_read_registers(cls, mmio, config: List[StepgenConfig]):
        """
        Adds the status registers to the MMIO.

        NOTE: Status registers are meant to be read by LinuxCNC and contain
        the current status of the stepgen.
        """
        # Don't create the registers when the config is empty (no stepgens
        # defined in this case)
        if not config:
            return

        for index, _ in enumerate(config):
            setattr(
                mmio,
                f'stepgen_{index}_position',
                CSRStatus(
                    size=64,
                    name=f'stepgen_{index}_position',
                    description=f'stepgen_{index}_position',
                )
            )
            setattr(
                mmio,
                f'stepgen_{index}_speed',
                CSRStatus(
                    size=32,
                    description=f'stepgen_{index}_speed',
                    name=f'stepgen_{index}_speed'
                )
            )

    @classmethod
    def add_mmio_write_registers(cls, mmio, config: List[StepgenConfig]):
        """
        Adds the storage registers to the MMIO.

        NOTE: Storage registers are meant to be written by LinuxCNC and contain
        the flags and configuration for the module.
        """
        # Don't create the registers when the config is empty (no encoders
        # defined in this case)
        if not config:
            return
        
        # General data - equal for each stepgen
        mmio.stepgen_steplen = CSRStorage(
            size=32,
            name=f'stepgen_steplen',
            description=f'The length of the step pulse in clock cycles',
            write_from_dev=False
        )
        mmio.stepgen_dir_hold_time = CSRStorage(
            size=32,
            name=f'stepgen_dir_hold_time',
            description=f'The minimum delay (in clock cycles) after a step pulse before '
            'a direction change - may be longer',
            write_from_dev=False
        )
        mmio.stepgen_dir_setup_time = CSRStorage(
            size=32,
            name=f'stepgen_dir_setup_time',
            description=f'The minimum delay (in clock cycles) after a direction change '
            'and before the next step - may be longer',
            write_from_dev=False
        )
        mmio.stepgen_apply_time = CSRStorage(
            size=64,
            name=f'stepgen_apply_time',
            description=f'The time at which the current settings (as stored in stepgen_#_speed_target '
            'and stepgen_#_max_acceleration will be applied and thus a new segment will be started.',
            write_from_dev=True
        )

        # Speed and acceleration settings for the next movement segment
        for index, _ in enumerate(config):
            setattr(
                mmio,
                f'stepgen_{index}_speed_target',
                CSRStorage(
                    size=32,
                    reset=0x80000000,  # Very important, as this is threated as 0
                    name=f'stepgen_{index}_speed_target',
                    description=f'The target speed for stepper {index}.',
                    write_from_dev=False
                )
            )
            setattr(
                mmio,
                f'stepgen_{index}_max_acceleration',
                CSRStorage(
                    size=32,
                    name=f'stepgen_{index}_max_acceleration',
                    description=f'The maximum acceleration for stepper {index}. The storage contains a '
                    'fixed point value, with 16 bits before and 16 bits after the point. Each '
                    'clock cycle, this value will be added or subtracted from the stepgen speed '
                    'until the target speed is acquired.',
                    write_from_dev=False
                )
            )

    @classmethod
    def create_from_config(cls, soc: SoC, watchdog, config: List[StepgenConfig]):
        """
        Adds the module as defined in the configuration to the SoC.

        NOTE: the configuration must be a list and should contain all the module at
        once. Otherwise naming conflicts will occur.
        """
        # Don't create the module when the config is empty (no stepgens 
        # defined in this case)
        if not config:
            return

        # NOTE: all the syncs are bundled with in a If(), so we don't add directly to
        # the sync here, but instead use this list
        sync = []

        for index, stepgen_config in enumerate(config):
            soc.platform.add_extension([
                ("stepgen", index,
                    Subsignal("step", Pins(stepgen_config.step_pin), IOStandard(stepgen_config.io_standard)),
                    Subsignal("dir", Pins(stepgen_config.dir_pin), IOStandard(stepgen_config.io_standard))
                )
            ])
            # Create the stepgen and add to the system
            stepgen = cls(
                pads=soc.platform.request('stepgen', index),
                pick_off=28,
                soft_stop=stepgen_config.soft_stop
            )
            soc.submodules += stepgen
            # Connect all the memory
            soc.comb += [
                # Data from MMIO to stepgen
                stepgen.reset.eq(soc.MMIO_inst.reset.storage),
                stepgen.enable.eq(~watchdog.has_bitten),
                stepgen.steplen.eq(soc.MMIO_inst.stepgen_steplen.storage),
                stepgen.dir_hold_time.eq(soc.MMIO_inst.stepgen_dir_hold_time.storage),
                stepgen.dir_setup_time.eq(soc.MMIO_inst.stepgen_dir_setup_time.storage)
            ]
            soc.sync += [
                # Position and feedback from stepgen to MMIO
                getattr(soc.MMIO_inst, f'stepgen_{index}_position').status.eq(stepgen.position),
                getattr(soc.MMIO_inst, f'stepgen_{index}_speed').status.eq(stepgen.speed),
            ]
            sync.extend([
                stepgen.speed_target.eq(getattr(soc.MMIO_inst, f'stepgen_{index}_speed_target').storage),
                stepgen.max_acceleration.eq(getattr(soc.MMIO_inst, f'stepgen_{index}_max_acceleration').storage),
            ])

        # Add all the speed targets and the max accelarion in the protected sync
        soc.sync += If(
            soc.MMIO_inst.wall_clock.status >= soc.MMIO_inst.stepgen_apply_time.storage,
            sync,
            # soc.MMIO_inst.stepgen_apply_time.storage.eq(0)
        )      


if __name__ == "__main__":
    from migen import *
    from migen.fhdl import *

    def test_stepgen(stepgen):
        i = 0
        # Setup the stepgen
        yield(stepgen.enable.eq(1))
        yield(stepgen.speed_target.eq(0x80000000 - int(2**28 / 128)))
        yield(stepgen.steplen.eq(16))
        yield(stepgen.dir_hold_time.eq(16))
        yield(stepgen.dir_setup_time.eq(32))

        while(1):
            if i == 390:
                yield(stepgen.speed_target.eq(0x80000000 + int(2**28 / 128)))
            position = (yield stepgen.position[:64])
            step = (yield stepgen.step)
            dir = (yield stepgen.dir)
            counter = (yield stepgen.dir_hold_counter.counter)
            print("position = %d @step %d @dir %d @dir_counter %d @clk %d"%(position, step, dir, counter, i))
            yield
            i+=1
            if i > 1000:
                break

    stepgen = StepgenModule(pick_off=28, soft_stop=True)
    print("\nRunning Sim...\n")
    # print(verilog.convert(stepgen, stepgen.ios, "pre_scaler"))
    run_simulation(stepgen, test_stepgen(stepgen))
