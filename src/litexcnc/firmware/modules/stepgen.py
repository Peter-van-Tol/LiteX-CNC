# Imports for creating a json-definition
import os
try:
    from typing import ClassVar, Iterable, List, Literal, Union
except ImportError:
    # Imports for Python <3.8
    from typing import ClassVar, Iterable, List, Union
    from typing_extensions import Literal
from pydantic import BaseModel, Field

# Imports for creating a LiteX/Migen module
from litex.soc.interconnect.csr import *
from migen import *
from migen.fhdl.structure import Cat, Constant
from litex.soc.integration.soc import SoC
from litex.soc.integration.doc import AutoDoc, ModuleDoc
from litex.build.generic_platform import *

# Local imports
from litexcnc.config.modules.stepgen import StepgenModuleConfig

class StepgenCounter(Module, AutoDoc):

    def __init__(self, size=32) -> None:

        self.intro = ModuleDoc("""
        Simple counter which counts down as soon as the Signal
        `counter` has a value larger then 0. Designed for the
        several timing components of the StepgenModule.
        """)

        # Create a 32 bit counter which counts down
        self.counter = Signal(size)
        self.sync += If(
            self.counter > 0,
            self.counter.eq(self.counter - 1)
        )


class StepgenModule(Module, AutoDoc):

    def __init__(self, pads, pick_off, soft_stop, create_pads) -> None:
        """
        
        NOTE: pickoff should be a three-tuple. A different pick-off for position, speed
        and acceleration is supported. When pick-off is a integer, all the pick offs will
        be the same.
        """

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
        # Store the pick-off (to prevent magic numbers later in the code)
        if isinstance(pick_off, int):
            self.pick_off_pos = pick_off
            self.pick_off_vel = pick_off
            self.pick_off_acc = pick_off
        elif isinstance(pick_off, Iterable) and not isinstance(pick_off, str):
            if len(pick_off) <  3:
                raise ValueError(f"Not enough values for `pick_off` ({len(pick_off)}), minimum length is 3.")
            self.pick_off_pos = pick_off[0]
            self.pick_off_vel = max(self.pick_off_pos, pick_off[1])
            self.pick_off_acc = max(self.pick_off_vel, pick_off[2])
        else:
            raise ValueError("`pick_off` must be either a list of pick_offs or a single integer value." )

        # Calculate constants based on the pick-off
        # - speed_reset_val: 0x8000_0000 in case of 32-bit variable, otherwise increase to set the sign bit
        self.speed_reset_val = (0x8000_0000 << (self.pick_off_acc - self.pick_off_vel)) 

        # Values which determine the spacing of the step. These
        # are used to reset the counters.
        # - signals
        self.wait = Signal()
        self.reset = Signal()

        # Main parameters for position, speed and acceleration
        self.enable = Signal()
        self.position = Signal(64 + (self.pick_off_vel - self.pick_off_pos))
        self.speed = Signal(
            32 + (self.pick_off_acc - self.pick_off_vel),
            reset=self.speed_reset_val
        )
        self.speed_target = Signal(
            32 + (self.pick_off_acc - self.pick_off_vel),
            reset=self.speed_reset_val
        )
        self.max_acceleration = Signal(32)

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
                self.speed_target.eq(self.speed_reset_val)
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
                    self.speed_target > (self.speed + self.max_acceleration),
                    # The counters are again a fixed point arithmetric. Every loop we keep
                    # the fraction and add the integer part to the speed. The fraction is
                    # used as a starting point for the next loop.
                    self.speed.eq(self.speed + self.max_acceleration),
                ).Elif(
                    # Decelerate, difference between actual speed and target speed is too
                    # large to bridge within one clock-cycle
                    self.speed_target < (self.speed - self.max_acceleration),
                    # The counters are again a fixed point arithmetric. Every loop we keep
                    # the fraction and add the integer part to the speed. However, we have
                    # keep in mind we are subtracting now every loop
                    self.speed.eq(self.speed - self.max_acceleration)
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
            # Prevent updating MMIO registers to prevent restart
            # Reset speed and position to 0
            self.speed_target.eq(self.speed_reset_val),
            self.speed.eq(self.speed_reset_val),
            self.max_acceleration.eq(0),
            self.position.eq(0),
        )

        # Update the position
        if soft_stop:
            sync += If(
                # Only check we are not waiting for the dir_setup. When the system is disabled, the
                # speed is set to 0 (with respect to acceleration limits) and the machine will be
                # stopped when disabled.
                ~self.reset & ~self.wait,
                self.position.eq(self.position + self.speed[(self.pick_off_acc - self.pick_off_vel):] - 0x8000_0000)
            )
        else:
            sync += If(
                # Check whether the system is enabled and we are not waiting for the dir_setup
                ~self.reset & self.enable & ~self.wait,
                self.position.eq(self.position + self.speed[(self.pick_off_acc - self.pick_off_vel):] - 0x8000_0000)
            )

        # Create the routine which actually handles the steps
        self.create_step_dir_routine(pads, create_pads)

    @classmethod
    def add_mmio_config_registers(cls, mmio, config: StepgenModuleConfig):
        """
        Adds the configuration registers to the MMIO. The configuration registers
        contain information on the the timings of ALL stepgen.
        TODO: in the next iteration of the stepgen timing configs should be for each
        stepgen individually.
        """
        mmio.stepgen_stepdata = CSRStorage(
            fields=[
                CSRField("steplen", size=10, offset=0, description="The length of the step pulse in clock cycles"),
                CSRField("dir_hold_time", size=10, offset=10, description="The minimum delay (in clock cycles) after a step pulse before "),
                CSRField("dir_setup_time", size=12, offset=20, description="The minimum delay (in clock cycles) after a direction change and before the next step - may be longer"),
            ],
            name=f'stepgen_stepdata',
            description=f'The length of the step pulse in clock cycles',
            write_from_dev=False
        )
    
    @classmethod
    def add_mmio_read_registers(cls, mmio, config: StepgenModuleConfig):
        """
        Adds the status registers to the MMIO.
        NOTE: Status registers are meant to be read by LinuxCNC and contain
        the current status of the stepgen.
        """
        # Don't create the registers when the config is empty (no stepgens
        # defined in this case)
        if not config:
            return

        for index, _ in enumerate(config.instances):
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
    def add_mmio_write_registers(cls, mmio, config: StepgenModuleConfig):
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
        mmio.stepgen_apply_time = CSRStorage(
            size=64,
            name=f'stepgen_apply_time',
            description=f'The time at which the current settings (as stored in stepgen_#_speed_target '
            'and stepgen_#_max_acceleration will be applied and thus a new segment will be started.',
            write_from_dev=True
        )

        # Speed and acceleration settings for the next movement segment
        for index, _ in enumerate(config.instances):
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
                    name=f'stepgen_{index}_max_acceleration1',
                    description=f'The maximum acceleration for stepper {index}. The storage contains a '
                    'fixed point value, with 16 bits before and 16 bits after the point. Each '
                    'clock cycle, this value will be added or subtracted from the stepgen speed '
                    'until the target speed is acquired.',
                    write_from_dev=False
                )
            )

    @classmethod
    def create_from_config(cls, soc: SoC, watchdog, config: StepgenModuleConfig):
        """
        Adds the module as defined in the configuration to the SoC.
        NOTE: the configuration must be a list and should contain all the module at
        once. Otherwise naming conflicts will occur.
        """
        # Don't create the module when the config is empty (no stepgens 
        # defined in this case)
        if not config:
            return

        # Determine the pick-off for the velocity. This one is based on the clock-frequency
        # and the step frequency to be obtained
        shift = 0
        while (soc.clock_frequency / (1 << (shift + 1)) > 400e3):
            shift += 1

        for index, stepgen_config in enumerate(config.instances):
            soc.platform.add_extension([
                ("stepgen", index,
                    *stepgen_config.pins.convert_to_signal()
                )
            ])
            # Create the stepgen and add to the system
            stepgen = cls(
                pads=soc.platform.request('stepgen', index),
                pick_off=(32, 32 + shift, 32 + shift + 8),
                soft_stop=stepgen_config.soft_stop,
                create_pads=stepgen_config.pins.create_pads
            )
            soc.submodules += stepgen
            # Connect all the memory
            soc.sync += [ # Aangepast
                # Data from MMIO to stepgen
                stepgen.reset.eq(soc.MMIO_inst.reset.storage),
                stepgen.enable.eq(~watchdog.has_bitten),
                stepgen.steplen.eq(soc.MMIO_inst.stepgen_stepdata.fields.steplen),
                stepgen.dir_hold_time.eq(soc.MMIO_inst.stepgen_stepdata.fields.dir_hold_time),
                stepgen.dir_setup_time.eq(soc.MMIO_inst.stepgen_stepdata.fields.dir_setup_time),
            ]
            soc.sync += [
                # Position and feedback from stepgen to MMIO
                getattr(soc.MMIO_inst, f'stepgen_{index}_position').status.eq(stepgen.position[(stepgen.pick_off_vel - stepgen.pick_off_pos):]),
                getattr(soc.MMIO_inst, f'stepgen_{index}_speed').status.eq(stepgen.speed[(stepgen.pick_off_acc - stepgen.pick_off_vel):])
            ]
            # Add speed target and the max acceleration in the protected sync
            soc.sync += [
                If(
                    soc.MMIO_inst.wall_clock.status >= soc.MMIO_inst.stepgen_apply_time.storage,
                    stepgen.speed_target.eq(Cat(Constant(0, bits_sign=(stepgen.pick_off_acc - stepgen.pick_off_vel)), getattr(soc.MMIO_inst, f'stepgen_{index}_speed_target').storage)),
                    stepgen.max_acceleration.eq(getattr(soc.MMIO_inst, f'stepgen_{index}_max_acceleration').storage),
                )
            ]
            # Add reset logic to stop the motion after reboot of LinuxCNC
            soc.sync += [
                soc.MMIO_inst.stepgen_apply_time.we.eq(0),
                If(
                    soc.MMIO_inst.reset.storage,
                    soc.MMIO_inst.stepgen_apply_time.dat_w.eq(0x80000000),
                    soc.MMIO_inst.stepgen_apply_time.we.eq(1)
                )
            ]

    def create_step_dir_routine(self, pads, create_pads):
        """
        Creates the routine for a step-dir stepper. The connection to the pads
        should be made in sub-classes
        """
        # Output parameters
        self.step_prev = Signal()
        self.step = Signal()
        self.dir = Signal(reset=True)

        # Link step and dir
        create_pads(self, pads)
        
        # - source which stores the value of the counters
        self.steplen = Signal(10)
        self.dir_hold_time = Signal(10)
        self.dir_setup_time = Signal(12)
        # - counters
        self.steplen_counter = StepgenCounter(10)
        self.dir_hold_counter = StepgenCounter(11)
        self.dir_setup_counter = StepgenCounter(13)
        self.submodules += [
            self.steplen_counter,
            self.dir_hold_counter,
            self.dir_setup_counter
        ]
        self.hold_dds = Signal()

        # Translate the position to steps by looking at the n'th bit (pick-off)
        # NOTE: to be able to simply add the velocity to the position for every timestep, the position
        # registered is widened from the default 64-buit width to 64-bit + difference in pick-off for
        # position and velocity. This meands that the bit we have to watch is also shifted by the
        # same amount. This means that although we are watching the position, we have to use the pick-off
        # for velocity
        self.sync += If(
            self.position[self.pick_off_vel] != self.step_prev,
            # Corner-case: The machine is at rest and starts to move in the opposite
            # direction. Wait with stepping the machine until the dir setup time has
            # passed.
            If(
                ~self.hold_dds,
                # The relevant bit has toggled, make a step to the next position by
                # resetting the counters
                self.step_prev.eq(self.position[self.pick_off_vel]),
                self.steplen_counter.counter.eq(self.steplen),
                self.dir_hold_counter.counter.eq(self.steplen + self.dir_hold_time),
                self.dir_setup_counter.counter.eq(self.steplen + self.dir_hold_time + self.dir_setup_time),
                self.wait.eq(False)
            ).Else(
                self.wait.eq(True)
            )
        )
        # Reset the DDS flag when dir_setup_counter has lapsed
        self.sync += If(
            self.dir_setup_counter.counter == 0,
            self.hold_dds.eq(0)
        )

        # Convert the parameters to output of step and dir
        # - step
        self.sync += If(
            self.steplen_counter.counter > 0,
            self.step.eq(1)
        ).Else(
            self.step.eq(0)
        )
        # - dir
        self.sync += If(
            self.dir != (self.speed[32 + (self.pick_off_acc - self.pick_off_vel) - 1]),
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
                self.dir.eq(self.speed[32 + (self.pick_off_acc - self.pick_off_vel) - 1])
            )
        )

        # Create the outputs
        self.ios = {self.step, self.dir}


if __name__ == "__main__":
    from migen import *
    from migen.fhdl import *

    def test_stepgen(stepgen):
        i = 0
        # Setup the stepgen
        yield(stepgen.enable.eq(1))
        # yield(stepgen.speed_target.eq(0x80000000 - int(2**28 / 128)))
        yield(stepgen.speed.eq(0x8000_0000_0000 + (0 << 16)))
        yield(stepgen.speed_test.eq(0x8000_0000 + (0x53E)))
        yield(stepgen.max_acceleration.eq(0x36F))
        yield(stepgen.steplen.eq(16))
        yield(stepgen.dir_hold_time.eq(16))
        yield(stepgen.dir_setup_time.eq(32))
        speed_prev=0

        while(1):
            # if i == 390:
            #     yield(stepgen.speed_target.eq(0x80000000 + int(2**28 / 128)))
            position = (yield stepgen.position)
            step = (yield stepgen.step)
            dir = (yield stepgen.dir)
            speed = (yield stepgen.speed_reported - 0x8000_0000)
            counter = (yield stepgen.dir_hold_counter.counter)
            if speed != speed_prev:
                print("speed = %d, position = %d @step %d @dir %d @dir_counter %d @clk %d"%(speed, position, step, dir, counter, i))
                speed_prev = speed
            yield
            i+=1
            if i > 100000:
                break

    stepgen = StepgenModule(pads=None, pick_off=32, soft_stop=True)
    print("\nRunning Sim...\n")
    # print(verilog.convert(stepgen, stepgen.ios, "pre_scaler"))
    run_simulation(stepgen, test_stepgen(stepgen))
