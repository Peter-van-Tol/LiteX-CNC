

# Default imports
import math
import os
from typing import ClassVar, List, Literal

# Imports for the configuration
from pydantic import Field

# Imports for Migen
from migen import *
from migen.genlib.cdc import MultiReg
from litex.build.generic_platform import Pins, IOStandard
from litex.soc.interconnect.csr import AutoCSR, CSRStatus, CSRStorage
from litex.soc.integration.doc import AutoDoc, ModuleDoc
from litex.soc.integration.soc import SoC

# Import of the basemodel, required to register this module
from . import ModuleBaseModel, ModuleInstanceBaseModel


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
        # Don't create the registers when the config is empty (no encoders
        # defined in this case)
        if not pwm_config:
            return

        mmio.pwm_enable = CSRStorage(
            size=int(math.ceil(float(len(pwm_config.instances))/32))*32,
            name='gpio_out',
            description="Register containing the bits to be written to the GPIO out pins.", 
            write_from_dev=False
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


class PWM_Instance(ModuleInstanceBaseModel):
    """
    Model describing a pin of the GPIO.
    """
    pin: str = Field(
        description="The pin on the FPGA-card."
    )
    name: str = Field(
        None,
        description="The name of the pin as used in LinuxCNC HAL-file (optional). "
        "When not specified, the standard pins like litexcnc.pwm.x will be created."
    )
    io_standard: str = Field(
        "LVCMOS33",
        description="The IO Standard (voltage) to use for the pin."
    )
    pins: ClassVar[List[str]] = [
        'curr_dc',
        'curr_period',
        'curr_pwm_freq',
        'curr_width',
        'dither_pwm',
        'enable',
        'max_dc',
        'min_dc',
        'offset',
        'pwm_freq',
        'scale',
        'value'
    ]


class PWM_ModuleConfig(ModuleBaseModel):
    """
    Module describing the PWM module
    """
    module_type: Literal['pwm'] = 'pwm'
    module_id: ClassVar[int] = 0x70776d5f  # The string `pwm_` in hex, must be equal to litexcnc_pwm.h
    driver_files: ClassVar[List[str]] = [
        os.path.dirname(__file__) + '/../../driver/modules/litexcnc_pwm.c',
        os.path.dirname(__file__) + '/../../driver/modules/litexcnc_pwm.h'
    ]
    instances: List[PWM_Instance] = Field(
        [],
        item_type=PWM_Instance,
        unique_items=True
    )

    def create_from_config(self, soc, watchdog):
        PwmPdmModule.create_from_config(soc, watchdog, self)
    
    def add_mmio_config_registers(self, mmio):
        # The PWM does not require any config, so this function is
        # not implemented.
        return

    def add_mmio_write_registers(self, mmio):
        PwmPdmModule.add_mmio_write_registers(mmio, self)

    def add_mmio_read_registers(self, mmio):
        # The PWM does not require any read, so this function is
        # not implemented.
        return

    @property
    def config_size(self):
        return 4

    def store_config(self, mmio):
        mmio.pwm_config_data =  CSRStatus(
            size=self.config_size*8,
            reset=len(self.instances),
            description=f"The config of the GPIO module."
        )
