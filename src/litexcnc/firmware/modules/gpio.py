# Default imports
import math

# Imports for Migen
from migen import *
from migen.genlib.cdc import MultiReg
from litex.build.generic_platform import *
from litex.soc.interconnect.csr import CSRStatus, CSRStorage
from litex.soc.integration.doc import AutoDoc, ModuleDoc


class GPIO_Module(Module, AutoDoc):
    """Module for creating output signals"""

    def __init__(self, mmio, pads_out=None, pads_in=None) -> None:
        # AutoDoc implementation
        self.intro = ModuleDoc("Module for GPIO signals.")
        
        # Implementation of GPIO_out, the output signal has the same width
        # as the number of GPIO out (they share the same register)
        if pads_out is not None:
            pads_out = self._to_signal(pads_out)
            self.comb += pads_out.eq(mmio.gpio_out.storage)

        # Implementation of GPIO_in, the input signal has the same width
        # as the number of GPIO in (they share the same register)
        if pads_in is not None:
            pads_in = self._to_signal(pads_in)
            self.specials += MultiReg(pads_in, mmio.gpio_in.status)

    @staticmethod
    def gpio_out_safe_state(config) -> int:
        """
        Gets the safe state of the GPIO
        """
        return sum(
            [
                (1*instance.safe_state << index ) 
                for index, instance 
                in enumerate([instance for instance in config.instances if instance.direction == "out"])
            ]
        )
    
    @classmethod
    def create_from_config(cls, soc, config: 'GPIO_ModuleConfig'):
        """
        Creates the GPIO from the config file.
        """
        # Don't create the registers when the config is empty (no GPIO 
        # defined in this case)
        if not config:
            return
        
        # Add the pins for the GPIO
        # - output
        pads_out = None
        if any([instance.direction == "out" for instance in config.instances]):
            soc.platform.add_extension([
                ("gpio_out", index, Pins(gpio.pin), IOStandard(gpio.io_standard))
                for index, gpio 
                in enumerate([gpio for gpio in config.instances if gpio.direction == "out"])
            ])
            pads_out = soc.platform.request_all("gpio_out")
        # - input
        pads_in = None
        if any([instance.direction == "in" for instance in config.instances]):
            soc.platform.add_extension([
                ("gpio_in", index, Pins(gpio.pin), IOStandard(gpio.io_standard))
                for index, gpio 
                in enumerate([gpio for gpio in config.instances if gpio.direction == "in"])
            ])
            pads_in = soc.platform.request_all("gpio_in")

        # Connect to the reset mechanism
        soc.sync += [
            soc.MMIO_inst.gpio_out.we.eq(0),
            If(
                soc.MMIO_inst.reset.storage | soc.MMIO_inst.watchdog_has_bitten.status,
                soc.MMIO_inst.gpio_out.dat_w.eq(
                    cls.gpio_out_safe_state(config)
                ),
                soc.MMIO_inst.gpio_out.we.eq(1)
            )
        ]
        
        # Create the GPIO module
        gpio = cls(
            soc.MMIO_inst,
            pads_out,
            pads_in
        )
        soc.submodules += gpio 

    @classmethod
    def add_mmio_write_registers(cls, mmio, config: 'GPIO_ModuleConfig'):
        """
        Adds the storage registers to the MMIO.

        NOTE: Storage registers are meant to be written by LinuxCNC and contain
        the flags and configuration for the module.
        """
        # Don't create the registers when the config is empty (no GPIO 
        # defined in this case)
        if not config:
            return

        mmio.gpio_out = CSRStorage(
            size=int(math.ceil(float(sum(1 for instance in config.instances if instance.direction == "out"))/32))*32,
            reset=cls.gpio_out_safe_state(config), 
            name='gpio_out',
            description="Register containing the bits to be written to the GPIO out pins.", 
            write_from_dev=True
        )

    @classmethod
    def add_mmio_read_registers(cls, mmio, config: 'GPIO_ModuleConfig'):
        """
        Adds the status registers to the MMIO.

        NOTE: Status registers are meant to be read by LinuxCNC and contain
        the current status of the module.
        """
        # Don't create the registers when the config is empty (no GPIO 
        # defined in this case)
        if not config:
            return

        mmio.gpio_in = CSRStatus(
            size=int(math.ceil(float(sum(1 for instance in config.instances if instance.direction == "in"))/32))*32,
            name='gpio_in',
            description="Register containing the bits to be written to the GPIO in pins."
        )

    @staticmethod
    def _to_signal(obj):
        return obj.raw_bits() if isinstance(obj, Record) else obj

