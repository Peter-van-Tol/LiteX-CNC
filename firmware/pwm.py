# Imports for creating a json-definition
from pydantic import BaseModel, Field
# Imports for creating a LiteX/Migen module
from litex.soc.interconnect.csr import *
from migen import *
from migen.genlib.cdc import MultiReg


class PWM(BaseModel):
    pin: str = Field(
        description="The pin on the FPGA-card."
    )
    name: str = Field(
        None,
        description="The name of the pin as used in LinuxCNC HAL-file (optional). "
    )
    io_standard: str = Field(
        "LVCMOS33",
        description="The IO Standard (voltage) to use for the pin."
    )


class PwmPdmModule(Module, AutoCSR):
    """Pulse Width Modulation

    Provides the minimal hardware to do Pulse Width Modulation.

    Pulse Width Modulation can be useful for various purposes: dim leds, regulate a fan, control
    an oscillator. Software can configure the PWM width and period and enable/disable it.
    """
    def __init__(self, pwm=None, clock_domain="sys", with_csr=True,
        default_enable = 0,
        default_width  = 0,
        default_period = 0):
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
                        self.width[:16] >= error,
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
