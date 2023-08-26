# Default imports
import math

# Imports for the configuration
from pydantic import Field

# Imports for Migen
from migen import *
from migen.genlib.cdc import MultiReg
from litex.build.generic_platform import Pins, IOStandard
from litex.soc.interconnect.csr import AutoCSR, CSRStorage
from litex.soc.integration.doc import AutoDoc, ModuleDoc
from litex.soc.integration.soc import SoC


class PwmPdmModule(Module, AutoDoc, AutoCSR):
    """Pulse Width Modulation

    Provides the minimal hardware to do Pulse Width Modulation.

    Pulse Width Modulation can be useful for various purposes: dim leds, regulate a fan, control
    an oscillator. Software can configure the PWM width and period and enable/disable it.
    """
    def __init__(self, pwm=None, clock_domain="sys", with_csr=True,
        default_enable = 0,
        default_width  = 0,
        default_period = 0):

        # AutoDoc implementation
        self.intro = ModuleDoc(self.__class__.__doc__)

        if pwm is None:
            self.pwm = pwm = Signal()
        self.enable = Signal(reset=default_enable)
        self.width  = Signal(32, reset=default_width)
        self.period = Signal(32, reset=default_period)

        # Local registers for PWM
        counter = Signal(32, reset_less=True)

        # Local regisers for PDM
        error   = Signal(16, reset_less=True)
        error_0 = Signal(16)
        error_1 = Signal(16)

        # sync = getattr(self.sync, clock_domain)
        self.sync += [
            If(self.enable,
                If(self.period != 0,
                    # PWM mode
                    counter.eq(counter + 1),
                    If(counter < self.width,
                        pwm.eq(1)
                    ).Else(
                        pwm.eq(0)
                    ),
                    If(counter >= (self.period - 1),
                        counter.eq(0)
                    )
                ).Else(
                    # PDM mode
                    error_0.eq(error - self.width[:16]),
                    error_1.eq(error - self.width[:16] + (2**16 - 1)),
                    If(
                        self.width[:16] > error,
                        pwm.eq(1),
                        error.eq(error_1)
                    ).Else(
                        pwm.eq(0),
                        error.eq(error_0)
                    )
                )
            ).Else(
                # Inactive
                counter.eq(0),
                pwm.eq(0)
            )
        ]

        if with_csr:
            self.add_csr(clock_domain)

    def add_csr(self, clock_domain):
        self._enable = CSRStorage(description="""PWM Enable.\n
            Write ``1`` to enable PWM.""",
            reset = self.enable.reset)
        self._width  = CSRStorage(32, reset_less=True, description="""PWM Width.\n
            Defines the *Duty cycle* of the PWM. PWM is active high for *Width* ``{cd}_clk`` cycles and
            active low for *Period - Width* ``{cd}_clk`` cycles.""".format(cd=clock_domain),
            reset = self.width.reset)
        self._period = CSRStorage(32, reset_less=True, description="""PWM Period.\n
            Defines the *Period* of the PWM in ``{cd}_clk`` cycles.""".format(cd=clock_domain),
            reset = self.period.reset)

        n = 0 if clock_domain == "sys" else 2
        self.specials += [
            MultiReg(self._enable.storage, self.enable, n=n),
            MultiReg(self._width.storage,  self.width,  n=n),
            MultiReg(self._period.storage, self.period, n=n),
        ]

    @classmethod
    def add_mmio_write_registers(cls, mmio, pwm_config: 'PWM_ModuleConfig'):
        """
        Adds the storage registers to the MMIO.
        NOTE: Storage registers are meant to be written by LinuxCNC and contain
        the flags and configuration for the module.
        """
        # Don't create the registers when the config is empty (no PWM/PDM
        # defined in this case)
        if not pwm_config:
            return

        mmio.pwm_enable = CSRStorage(
            size=int(math.ceil(float(len(pwm_config.instances))/32))*32,
            name='gpio_out',
            description="Register containing the bits to be written to the GPIO out pins.", 
            write_from_dev=True
        )

        # Speed and acceleration settings for the next movement segment
        for index in range(len(pwm_config.instances)):
            setattr(
                mmio,
                f'pwm_{index}_period', 
                CSRStorage(
                    size=32, 
                    description=f'pwm_{index}_period', 
                    name=f'pwm_{index}_period', 
                    write_from_dev=False
                )
            )
            setattr(
                mmio,
                f'pwm_{index}_width', 
                CSRStorage(
                    size=32, 
                    description=f'pwm_{index}_width',
                    name=f'pwm_{index}_width',
                    write_from_dev=False
                )
            )

    @classmethod
    def create_from_config(cls, soc: SoC, watchdog, pwm_config: 'PWM_ModuleConfig'):
        """
        Adds the module as defined in the configuration to the SoC.
        NOTE: the configuration must be a list and should contain all the module at
        once. Otherwise naming conflicts will occur.
        """
        # Don't create the module when the config is empty (no stepgens 
        # defined in this case)
        if not pwm_config:
            return

        # Add module and create the pads
        soc.platform.add_extension([
            ("pwm", index, Pins(pwm_instance.pin), IOStandard(pwm_instance.io_standard))
            for index, pwm_instance 
            in enumerate(pwm_config.instances)
        ])
        soc.pwm_outputs = [pad for pad in soc.platform.request_all("pwm").l]

        # Turn the PWM off when the card is reset or watchdog has bitten
        # Connect to the reset mechanism
        soc.sync += [
            soc.MMIO_inst.pwm_enable.we.eq(0),
            If(
                soc.MMIO_inst.reset.storage | soc.MMIO_inst.watchdog_has_bitten.status,
                soc.MMIO_inst.pwm_enable.dat_w.eq(0x0),
                soc.MMIO_inst.pwm_enable.we.eq(1)
            )
        ]

        # Create the generators
        for index in range(len(pwm_config.instances)):
            # Add the PWM-module to the platform
            _pwm = PwmPdmModule(soc.pwm_outputs[index], with_csr=False)
            soc.submodules += _pwm
            soc.comb += [
                _pwm.enable.eq(soc.MMIO_inst.pwm_enable.storage[index] & ~watchdog.has_bitten),
                _pwm.period.eq(getattr(soc.MMIO_inst, f'pwm_{index}_period').storage),
                _pwm.width.eq(getattr(soc.MMIO_inst, f'pwm_{index}_width').storage)
            ]
