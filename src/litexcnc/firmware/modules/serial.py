# Default imports
import math

# Imports for Migen
from migen import *
from migen.genlib.cdc import MultiReg
from migen.genlib.fifo import SyncFIFO
from litex.build.generic_platform import Pins, IOStandard
from litex.soc.interconnect.csr import AutoCSR, CSRStorage, CSRStatus, CSRField
from litex.soc.integration.doc import AutoDoc, ModuleDoc

"""
Serial Port Module for LitexCNC
================================

This module provides a fully configurable UART interface with FIFO buffers
and RS485 support for LitexCNC.

Configuration Examples:
-----------------------

1. Baud Rate Configuration (serial_divisor register):
   Divisor = sys_clk_freq / baudrate
   
   For 50 MHz system clock:
   - 115200 baud: divisor = 50000000 / 115200 = 434
   - 57600 baud:  divisor = 50000000 / 57600  = 868
   - 38400 baud:  divisor = 50000000 / 38400  = 1302
   - 19200 baud:  divisor = 50000000 / 19200  = 2604
   - 9600 baud:   divisor = 50000000 / 9600   = 5208
   
   For 100 MHz system clock:
   - 115200 baud: divisor = 100000000 / 115200 = 868

2. UART Mode Configuration (serial_config register):
   Bits [3:0]  - data_bits:      5, 6, 7, or 8 (default: 8)
   Bit  [4]    - parity_enable:  0=disabled, 1=enabled (default: 0)
   Bit  [5]    - parity_type:    0=even, 1=odd (default: 0)
   Bits [7:6]  - stop_bits:      1 or 2 (default: 1)
   
   Common configurations:
   - 8N1 (8 data bits, no parity, 1 stop bit):
     data_bits=8, parity_enable=0, stop_bits=1
     Register value: 0x08
   
   - 8E1 (8 data bits, even parity, 1 stop bit):
     data_bits=8, parity_enable=1, parity_type=0, stop_bits=1
     Register value: 0x18
   
   - 7O2 (7 data bits, odd parity, 2 stop bits):
     data_bits=7, parity_enable=1, parity_type=1, stop_bits=2
     Register value: 0xB7

3. FIFO Usage:
   - TX FIFO: Write up to 32 bytes to serial_tx_fifo_0..7 registers
              (4 bytes per register), then set load bit in serial_tx_ctrl
   - RX FIFO: Set unload bit in serial_rx_ctrl to read data from FIFO
              into serial_rx_fifo_0..7 registers
   - Monitor FIFO levels via serial_tx_ptr and serial_rx_ptr registers

4. RS485 DataEnable:
   - Automatically controlled during transmission
   - Active (high) when data is being transmitted
   - Inactive (low) when idle or receiving
   - Monitor state via data_enable bit in serial_status register
"""
class SyncFIFOWithFlush(Module):
    def __init__(self, width, depth):
        self.din = Signal(width)
        self.we = Signal()
        self.re = Signal()
        self.dout = Signal(width)
        self.writable = Signal()
        self.readable = Signal()
        self.level = Signal(max=depth+1)
        self.flush = Signal()          # внешний импульс

        storage = Memory(width, depth)
        self.specials += storage
        wport = storage.get_port(write_capable=True)
        rport = storage.get_port()
        self.specials += wport, rport

        wptr = Signal(max=depth)
        rptr = Signal(max=depth)
        count = Signal(max=depth+1)

        self.comb += [
            wport.adr.eq(wptr),
            wport.dat_w.eq(self.din),
            wport.we.eq(self.we & self.writable),
            rport.adr.eq(rptr),
            self.dout.eq(rport.dat_r),
            self.writable.eq(count != depth),
            self.readable.eq(count != 0),
            self.level.eq(count)
        ]

        self.sync += [
            If(self.flush,
                wptr.eq(0),
                rptr.eq(0),
                count.eq(0),
                self.flush.eq(0)           # сбрасываем после одного такта
            ).Elif(self.we & self.writable & self.re & self.readable,
                wptr.eq(wptr + 1),
                rptr.eq(rptr + 1),
                count.eq(count)
            ).Elif(self.we & self.writable,
                wptr.eq(wptr + 1),
                count.eq(count + 1)
            ).Elif(self.re & self.readable,
                rptr.eq(rptr + 1),
                count.eq(count - 1)
            )
        ]

class ConfigurableUARTTX(Module):
    """Configurable UART Transmitter
    
    Supports runtime configuration of:
    - Baud rate (via divisor)
    - Data bits (5-8)
    - Parity (none, even, odd)
    - Stop bits (1, 2)
    """
    
    def __init__(self, tx_pad):
        # Configuration signals
        self.divisor = Signal(16)      # Baud rate divisor
        self.data_bits = Signal(4)     # Number of data bits (5-8)
        self.parity_enable = Signal()  # Enable parity
        self.parity_type = Signal()    # 0=even, 1=odd
        self.stop_bits = Signal(2)     # Number of stop bits (1 or 2)
        
        # Data interface
        self.sink_data = Signal(8)
        self.sink_valid = Signal()
        self.sink_ready = Signal()
        
        # TX state machine
        self.submodules.fsm = FSM(reset_state="IDLE")
        
        # Baud rate generator
        baud_counter = Signal(16)
        baud_tick = Signal()
        
        self.sync += [
            If(baud_counter == 0,
                baud_counter.eq(self.divisor),
                baud_tick.eq(1)
            ).Else(
                baud_counter.eq(baud_counter - 1),
                baud_tick.eq(0)
            )
        ]
        
        # TX shift register and counters
        tx_reg = Signal(8)
        bit_counter = Signal(4)
        parity_bit = Signal()
        
        self.fsm.act("IDLE",
            self.sink_ready.eq(1),
            tx_pad.eq(1),  # Idle high
            If(self.sink_valid,
                NextValue(tx_reg, self.sink_data),
                NextValue(bit_counter, 0),
                NextValue(baud_counter, self.divisor),
                NextState("START")
            )
        )
        
        self.fsm.act("START",
            tx_pad.eq(0),  # Start bit
            If(baud_tick,
                NextValue(bit_counter, 0),
                # Calculate parity
                NextValue(parity_bit, 
                    tx_reg[0] ^ tx_reg[1] ^ tx_reg[2] ^ tx_reg[3] ^
                    tx_reg[4] ^ tx_reg[5] ^ tx_reg[6] ^ tx_reg[7] ^ 
                    self.parity_type
                ),
                NextState("DATA")
            )
        )
        
        self.fsm.act("DATA",
            tx_pad.eq(tx_reg[0]),
            If(baud_tick,
                NextValue(tx_reg, tx_reg >> 1),
                NextValue(bit_counter, bit_counter + 1),
                If(bit_counter == (self.data_bits - 1),
                    If(self.parity_enable,
                        NextState("PARITY")
                    ).Else(
                        NextState("STOP")
                    )
                )
            )
        )
        
        self.fsm.act("PARITY",
            tx_pad.eq(parity_bit),
            If(baud_tick,
                NextValue(bit_counter, 0),
                NextState("STOP")
            )
        )
        
        self.fsm.act("STOP",
            tx_pad.eq(1),  # Stop bit
            If(baud_tick,
                NextValue(bit_counter, bit_counter + 1),
                If(bit_counter == (self.stop_bits - 1),
                    NextState("IDLE")
                )
            )
        )


class ConfigurableUARTRX(Module):
    """Configurable UART Receiver
    
    Supports runtime configuration of:
    - Baud rate (via divisor)
    - Data bits (5-8)
    - Parity (none, even, odd)
    - Stop bits (1, 2)
    """
    
    def __init__(self, rx_pad):
        # Configuration signals
        self.divisor = Signal(16)
        self.data_bits = Signal(4)
        self.parity_enable = Signal()
        self.parity_type = Signal()
        self.stop_bits = Signal(2)
        
        # Data interface
        self.source_data = Signal(8)
        self.source_valid = Signal()
        self.source_ready = Signal()
        
        # Error flags
        self.parity_error = Signal()
        self.frame_error = Signal()
        
        # RX synchronizer
        rx_sync = Signal()
        self.specials += MultiReg(rx_pad, rx_sync)
        
        # RX state machine
        self.submodules.fsm = FSM(reset_state="IDLE")
        
        # Baud rate generator - sample at 16x baud rate
        baud_counter = Signal(16)
        sample_tick = Signal()
        
        self.sync += [
            If(baud_counter == 0,
                baud_counter.eq(self.divisor >> 4),  # 16x oversampling
                sample_tick.eq(1)
            ).Else(
                baud_counter.eq(baud_counter - 1),
                sample_tick.eq(0)
            )
        ]
        
        # RX shift register and counters
        rx_reg = Signal(8)
        bit_counter = Signal(4)
        sample_counter = Signal(4)
        parity_bit = Signal()
        calc_parity = Signal()
        
        self.fsm.act("IDLE",
            self.source_valid.eq(0),
            self.parity_error.eq(0),
            self.frame_error.eq(0),
            If(~rx_sync,  # Start bit detected
                NextValue(sample_counter, 0),
                NextValue(baud_counter, self.divisor >> 4),
                NextState("START")
            )
        )
        
        self.fsm.act("START",
            If(sample_tick,
                NextValue(sample_counter, sample_counter + 1),
                If(sample_counter == 7,  # Sample at middle
                    If(~rx_sync,  # Valid start bit
                        NextValue(bit_counter, 0),
                        NextValue(rx_reg, 0),
                        NextState("DATA")
                    ).Else(  # False start
                        NextState("IDLE")
                    )
                )
            )
        )
        
        self.fsm.act("DATA",
            If(sample_tick,
                NextValue(sample_counter, sample_counter + 1),
                If(sample_counter == 15,  # Sample at middle
                    NextValue(sample_counter, 0),
                    NextValue(rx_reg, (rx_reg >> 1) | (rx_sync << 7)),
                    NextValue(bit_counter, bit_counter + 1),
                    If(bit_counter == (self.data_bits - 1),
                        # Calculate expected parity
                        NextValue(calc_parity,
                            rx_sync ^ rx_reg[0] ^ rx_reg[1] ^ rx_reg[2] ^ 
                            rx_reg[3] ^ rx_reg[4] ^ rx_reg[5] ^ rx_reg[6] ^ 
                            self.parity_type
                        ),
                        If(self.parity_enable,
                            NextState("PARITY")
                        ).Else(
                            NextState("STOP")
                        )
                    )
                )
            )
        )
        
        self.fsm.act("PARITY",
            If(sample_tick,
                NextValue(sample_counter, sample_counter + 1),
                If(sample_counter == 15,
                    NextValue(sample_counter, 0),
                    NextValue(parity_bit, rx_sync),
                    If(rx_sync != calc_parity,
                        NextValue(self.parity_error, 1)
                    ),
                    NextState("STOP")
                )
            )
        )
        
        self.fsm.act("STOP",
            If(sample_tick,
                NextValue(sample_counter, sample_counter + 1),
                If(sample_counter == 15,
                    If(~rx_sync,  # Framing error
                        NextValue(self.frame_error, 1)
                    ),
                    NextValue(bit_counter, bit_counter + 1),
                    If(bit_counter == (self.stop_bits - 1),
                        NextValue(self.source_data, rx_reg),
                        NextValue(self.source_valid, 1),
                        NextState("IDLE")
                    ).Else(
                        NextValue(sample_counter, 0)
                    )
                )
            )
        )


class SerialModule(Module, AutoDoc):
    """Serial Port Module with FIFO buffers
    
    Provides serial communication with configurable parameters and FIFO buffers
    for both TX and RX. Supports RS485 with DataEnable output.
    
    Features:
    - Configurable baud rate (via divisor = sys_clk_freq / baudrate)
    - Configurable data bits (5-8)
    - Configurable parity (none, even, odd)
    - Configurable stop bits (1, 2)
    - 32-byte TX and RX FIFO buffers
    - RS485 DataEnable signal
    - CSR-based configuration without recompilation
    - 8x CSR registers for TX data loading
    - 8x CSR registers for RX data reading
    """
    
    def __init__(self, pads, sys_clk_freq, mmio, fifo_depth=32):
        # AutoDoc implementation
        self.intro = ModuleDoc(self.__class__.__doc__)
        
        # Split pads
        tx_pad = pads[0] if isinstance(pads, (list, tuple)) else pads.tx
        rx_pad = pads[1] if isinstance(pads, (list, tuple)) else pads.rx
        
        # Create configurable UART TX and RX
        self.submodules.uart_tx = ConfigurableUARTTX(tx_pad)
        self.submodules.uart_rx = ConfigurableUARTRX(rx_pad)
        
        # Connect configuration from CSR
        self.comb += [
            # TX configuration
            self.uart_tx.divisor.eq(mmio.serial_divisor.storage[0:16]),
            self.uart_tx.data_bits.eq(mmio.serial_config.storage[0:4]),
            self.uart_tx.parity_enable.eq(mmio.serial_config.storage[4]),
            self.uart_tx.parity_type.eq(mmio.serial_config.storage[5]),
            self.uart_tx.stop_bits.eq(mmio.serial_config.storage[6:8]),
            
            # RX configuration
            self.uart_rx.divisor.eq(mmio.serial_divisor.storage[0:16]),
            self.uart_rx.data_bits.eq(mmio.serial_config.storage[0:4]),
            self.uart_rx.parity_enable.eq(mmio.serial_config.storage[4]),
            self.uart_rx.parity_type.eq(mmio.serial_config.storage[5]),
            self.uart_rx.stop_bits.eq(mmio.serial_config.storage[6:8]),
        ]
        
        # TX and RX FIFOs (32 bytes each)
        #self.submodules.tx_fifo = SyncFIFO(width=8, depth=fifo_depth)
        #self.submodules.rx_fifo = SyncFIFO(width=8, depth=fifo_depth)
        self.submodules.tx_fifo = SyncFIFOWithFlush(width=8, depth=fifo_depth)
        self.submodules.rx_fifo = SyncFIFOWithFlush(width=8, depth=fifo_depth)
        # TX FIFO pointers
        self.tx_write_ptr = Signal(max=fifo_depth)
        self.tx_read_ptr = Signal(max=fifo_depth)
        
        # RX FIFO pointers  
        self.rx_write_ptr = Signal(max=fifo_depth)
        self.rx_read_ptr = Signal(max=fifo_depth)
        
        # Track FIFO levels
        self.sync += [
            # TX pointers
            If(self.tx_fifo.we & ~self.tx_fifo.re,
                self.tx_write_ptr.eq(self.tx_write_ptr + 1)
            ).Elif(~self.tx_fifo.we & self.tx_fifo.re,
                self.tx_read_ptr.eq(self.tx_read_ptr + 1)
            ).Elif(self.tx_fifo.we & self.tx_fifo.re,
                self.tx_write_ptr.eq(self.tx_write_ptr + 1),
                self.tx_read_ptr.eq(self.tx_read_ptr + 1)
            ),
            
            # RX pointers
            If(self.rx_fifo.we & ~self.rx_fifo.re,
                self.rx_write_ptr.eq(self.rx_write_ptr + 1)
            ).Elif(~self.rx_fifo.we & self.rx_fifo.re,
                self.rx_read_ptr.eq(self.rx_read_ptr + 1)
            ).Elif(self.rx_fifo.we & self.rx_fifo.re,
                self.rx_write_ptr.eq(self.rx_write_ptr + 1),
                self.rx_read_ptr.eq(self.rx_read_ptr + 1)
            )
        ]
        
        # Connect TX FIFO to UART TX
        self.comb += [
            self.uart_tx.sink_data.eq(self.tx_fifo.dout),
            self.uart_tx.sink_valid.eq(self.tx_fifo.readable),
            self.tx_fifo.re.eq(self.uart_tx.sink_ready & self.uart_tx.sink_valid)
        ]
        
        # Connect UART RX to RX FIFO
        self.comb += [
            self.rx_fifo.din.eq(self.uart_rx.source_data),
            self.rx_fifo.we.eq(self.uart_rx.source_valid & self.rx_fifo.writable),
            self.uart_rx.source_ready.eq(self.rx_fifo.writable)
        ]
        
        # RS485 DataEnable signal - active during transmission
        self.data_enable = Signal()
        tx_busy = Signal()
        
        If(self.tx_fifo.readable | self.uart_tx.fsm.ongoing("START") |
        self.uart_tx.fsm.ongoing("DATA") | self.uart_tx.fsm.ongoing("PARITY") |
        self.uart_tx.fsm.ongoing("STOP"),
            tx_busy.eq(1)
        ).Else(
            tx_busy.eq(0)
        )

        self.comb += self.data_enable.eq(tx_busy)
        
        # Connect to MMIO registers for FIFO access
        # TX FIFO loading from CSR registers
        tx_fifo_we = Signal()
        tx_fifo_din = Signal(8)
        
        self.comb += [
            self.tx_fifo.din.eq(tx_fifo_din),
            self.tx_fifo.we.eq(tx_fifo_we & self.tx_fifo.writable)
        ]
        
        # State machine for loading TX FIFO from CSR registers
        tx_load_index = Signal(3)  # 0-7 for 8 registers
        tx_byte_index = Signal(2)  # 0-3 for 4 bytes per register
        tx_loading = Signal()
        
        self.sync += [
            tx_fifo_we.eq(0),
            
            If(mmio.serial_tx_ctrl.storage[0],  # Load trigger
                If(~tx_loading,
                    tx_loading.eq(1),
                    tx_load_index.eq(0),
                    tx_byte_index.eq(0)
                ).Elif(tx_loading,
                    If(self.tx_fifo.writable,
                        # Load current byte
                        Case(tx_load_index, {
                            i: tx_fifo_din.eq(getattr(mmio, f'serial_tx_fifo_{i}').storage[(tx_byte_index*8):(tx_byte_index*8+8)])
                            for i in range(8)
                        }),
                        tx_fifo_we.eq(1),
                        
                        # Move to next byte
                        If(tx_byte_index == 3,
                            tx_byte_index.eq(0),
                            If(tx_load_index == 7,
                                tx_loading.eq(0)  # Done loading
                            ).Else(
                                tx_load_index.eq(tx_load_index + 1)
                            )
                        ).Else(
                            tx_byte_index.eq(tx_byte_index + 1)
                        )
                    )
                )
            ).Else(
                tx_loading.eq(0)
            )
        ]
        
        # RX FIFO unloading to CSR registers
        rx_unload_trigger = Signal()
        rx_unload_index = Signal(3)
        rx_byte_index = Signal(2)
        rx_unloading = Signal()
        
        # Registers to hold RX data
        rx_data_regs = Array([Signal(32) for _ in range(8)])
        
        self.sync += [
            self.rx_fifo.re.eq(0),
            
            If(mmio.serial_rx_ctrl.storage[0],  # Unload trigger
                If(~rx_unloading,
                    rx_unloading.eq(1),
                    rx_unload_index.eq(0),
                    rx_byte_index.eq(0)
                ).Elif(rx_unloading,
                    If(self.rx_fifo.readable,
                        # Read current byte into register
                        rx_data_regs[rx_unload_index][(rx_byte_index*8):(rx_byte_index*8+8)].eq(self.rx_fifo.dout),
                        self.rx_fifo.re.eq(1),
                        
                        # Move to next byte
                        If(rx_byte_index == 3,
                            rx_byte_index.eq(0),
                            If(rx_unload_index == 7,
                                rx_unloading.eq(0)  # Done unloading
                            ).Else(
                                rx_unload_index.eq(rx_unload_index + 1)
                            )
                        ).Else(
                            rx_byte_index.eq(rx_byte_index + 1)
                        )
                    ).Else(
                        # No more data in FIFO
                        rx_unloading.eq(0)
                    )
                )
            ).Else(
                rx_unloading.eq(0)
            )
        ]
        
        # Connect RX data registers to status registers
        for i in range(8):
            self.comb += getattr(mmio, f'serial_rx_fifo_{i}').status.eq(rx_data_regs[i])
        
        # Update pointer status registers
        self.comb += [
            mmio.serial_tx_ptr.status[0:8].eq(self.tx_write_ptr),
            mmio.serial_tx_ptr.status[8:16].eq(self.tx_read_ptr),
            mmio.serial_tx_ptr.status[16:24].eq(self.tx_fifo.level),
            
            mmio.serial_rx_ptr.status[0:8].eq(self.rx_write_ptr),
            mmio.serial_rx_ptr.status[8:16].eq(self.rx_read_ptr),
            mmio.serial_rx_ptr.status[16:24].eq(self.rx_fifo.level),
        ]

    @classmethod
    def add_mmio_config_registers(cls, mmio, config: 'SerialModuleConfig'):
        """
        Adds configuration registers to MMIO.
        These registers define module-specific configuration.
        """
        if not config:
            return
        
        # Number of serial ports
        mmio.serial_count = CSRStatus(
            size=8,
            reset=len(config.instances),
            name='serial_count',
            description="Number of configured serial ports"
        )

    @classmethod
    def add_mmio_write_registers(cls, mmio, config: 'SerialModuleConfig'):
        """
        Adds the storage registers to the MMIO.
        
        NOTE: Storage registers are meant to be written by LinuxCNC and contain
        the flags and configuration for the module.
        """
        if not config:
            return
        
        # Baud rate divisor register
        mmio.serial_divisor = CSRStorage(
            size=32,
            name='serial_divisor',
            description="Serial port baud rate divisor. Divisor = sys_clk_freq / baudrate. "
                       "Examples: @50MHz: 115200 baud = 434, 57600 = 868, 38400 = 1302, "
                       "19200 = 2604, 9600 = 5208. @100MHz: 115200 baud = 868",
            write_from_dev=False
        )
        
        # UART mode configuration register
        mmio.serial_config = CSRStorage(
            fields=[
                CSRField("data_bits", size=4, offset=0, description="Number of data bits (5-8). Default: 8"),
                CSRField("parity_enable", size=1, offset=4, description="Enable parity bit. 0=disabled, 1=enabled"),
                CSRField("parity_type", size=1, offset=5, description="Parity type. 0=even, 1=odd"),
                CSRField("stop_bits", size=2, offset=6, description="Number of stop bits. 1 or 2"),
            ],
            name='serial_config',
            description="Serial port mode configuration (data bits, parity, stop bits)",
            write_from_dev=False
        )
        
        # TX FIFO data registers (8 registers, 4 bytes each = 32 bytes total)
        for i in range(8):
            setattr(
                mmio,
                f'serial_tx_fifo_{i}',
                CSRStorage(
                    size=32,
                    name=f'serial_tx_fifo_{i}',
                    description=f'TX FIFO data register {i} (bytes {i*4}-{i*4+3})',
                    write_from_dev=False
                )
            )
        
        # TX control register
        mmio.serial_tx_ctrl = CSRStorage(
            fields=[
                CSRField("load", size=1, offset=0, description="Write 1 to load TX FIFO from data registers"),
                CSRField("clear", size=1, offset=1, description="Write 1 to clear TX FIFO"),
            ],
            name='serial_tx_ctrl',
            description='TX FIFO control register',
            write_from_dev=False
        )
        
        # RX control register
        mmio.serial_rx_ctrl = CSRStorage(
            fields=[
                CSRField("unload", size=1, offset=0, description="Write 1 to unload RX FIFO to data registers"),
                CSRField("clear", size=1, offset=1, description="Write 1 to clear RX FIFO"),
            ],
            name='serial_rx_ctrl',
            description='RX FIFO control register',
            write_from_dev=False
        )

    @classmethod
    def add_mmio_read_registers(cls, mmio, config: 'SerialModuleConfig'):
        """
        Adds the status registers to the MMIO.
        
        NOTE: Status registers are meant to be read by LinuxCNC and contain
        the current status of the module.
        """
        if not config:
            return
        
        # RX FIFO data registers (8 registers, 4 bytes each = 32 bytes total)
        for i in range(8):
            setattr(
                mmio,
                f'serial_rx_fifo_{i}',
                CSRStatus(
                    size=32,
                    name=f'serial_rx_fifo_{i}',
                    description=f'RX FIFO data register {i} (bytes {i*4}-{i*4+3})'
                )
            )
        
        # TX FIFO pointer register
        mmio.serial_tx_ptr = CSRStatus(
            fields=[
                CSRField("write_ptr", size=8, offset=0, description="TX FIFO write pointer"),
                CSRField("read_ptr", size=8, offset=8, description="TX FIFO read pointer"),
                CSRField("level", size=8, offset=16, description="TX FIFO level (bytes available)"),
            ],
            name='serial_tx_ptr',
            description='TX FIFO pointers and level'
        )
        
        # RX FIFO pointer register
        mmio.serial_rx_ptr = CSRStatus(
            fields=[
                CSRField("write_ptr", size=8, offset=0, description="RX FIFO write pointer"),
                CSRField("read_ptr", size=8, offset=8, description="RX FIFO read pointer"),
                CSRField("level", size=8, offset=16, description="RX FIFO level (bytes available)"),
            ],
            name='serial_rx_ptr',
            description='RX FIFO pointers and level'
        )
        
        # Status register
        mmio.serial_status = CSRStatus(
            fields=[
                CSRField("tx_full", size=1, offset=0, description="TX FIFO full"),
                CSRField("tx_empty", size=1, offset=1, description="TX FIFO empty"),
                CSRField("rx_full", size=1, offset=2, description="RX FIFO full"),
                CSRField("rx_empty", size=1, offset=3, description="RX FIFO empty"),
                CSRField("tx_active", size=1, offset=4, description="Transmission active"),
                CSRField("data_enable", size=1, offset=5, description="RS485 DataEnable output state"),
                CSRField("parity_error", size=1, offset=6, description="Parity error detected"),
                CSRField("frame_error", size=1, offset=7, description="Frame error detected"),
            ],
            name='serial_status',
            description='Serial port status flags'
        )

    @classmethod
    def create_from_config(cls, soc, watchdog, config: 'SerialModuleConfig'):
        """
        Creates the serial port module from configuration.
        
        NOTE: the configuration must be provided and should contain all serial
        port instances at once to avoid naming conflicts.
        """
        if not config:
            return
        
        # Add serial port pins to platform
        for index, serial_instance in enumerate(config.instances):
            # Add TX, RX and DataEnable pins
            soc.platform.add_extension([
                ("serial", index, 
                 Pins(f"{serial_instance.tx_pin} {serial_instance.rx_pin}"),
                 IOStandard(serial_instance.io_standard)),
                ("serial_de", index,
                 Pins(serial_instance.data_enable_pin),
                 IOStandard(serial_instance.io_standard))
            ])
        
        # Create serial modules
        for index in range(len(config.instances)):
            serial_pads = soc.platform.request('serial', index)
            de_pad = soc.platform.request('serial_de', index)
            
            # Create the serial module
            serial = cls(
                pads=serial_pads,
                sys_clk_freq=soc.clock_frequency,
                mmio=soc.MMIO_inst,
                fifo_depth=32
            )
            soc.submodules += serial
            
            # Connect DataEnable output
            soc.comb += de_pad.eq(serial.data_enable)
            
            # Clear FIFOs when reset or watchdog triggers
            soc.sync += [
                If(
                    soc.MMIO_inst.reset.storage | soc.MMIO_inst.watchdog_status.status != 0,
                    # Clear TX FIFO
                    serial.tx_fifo.flush.eq(1)
                    serial.tx_fifo.din.eq(0),
                    serial.tx_fifo.we.eq(0),
                    serial.tx_fifo.re.eq(0),
                    serial.tx_write_ptr.eq(0),
                    serial.tx_read_ptr.eq(0),
                    
                    # Clear RX FIFO
                    serial.rx_fifo.flush.eq(1)
                    serial.rx_fifo.din.eq(0),
                    serial.rx_fifo.we.eq(0),
                    serial.rx_fifo.re.eq(0),
                    serial.rx_write_ptr.eq(0),
                    serial.rx_read_ptr.eq(0),
                ),
                
                # Manual clear controls
                If(soc.MMIO_inst.serial_tx_ctrl.storage[1],  # TX clear bit
                    serial.tx_write_ptr.eq(0),
                    serial.tx_read_ptr.eq(0)
                ),
                
                If(soc.MMIO_inst.serial_rx_ctrl.storage[1],  # RX clear bit
                    serial.rx_write_ptr.eq(0),
                    serial.rx_read_ptr.eq(0)
                )
            ]
            
            # Update status register
            soc.comb += [
                soc.MMIO_inst.serial_status.status[0].eq(~serial.tx_fifo.writable),     # TX full
                soc.MMIO_inst.serial_status.status[1].eq(~serial.tx_fifo.readable),     # TX empty
                soc.MMIO_inst.serial_status.status[2].eq(~serial.rx_fifo.writable),     # RX full
                soc.MMIO_inst.serial_status.status[3].eq(~serial.rx_fifo.readable),     # RX empty
                soc.MMIO_inst.serial_status.status[4].eq(
                    serial.uart_tx.fsm.ongoing("START") |
                    serial.uart_tx.fsm.ongoing("DATA") |
                    serial.uart_tx.fsm.ongoing("PARITY") |
                    serial.uart_tx.fsm.ongoing("STOP")
                ),  # TX active
                soc.MMIO_inst.serial_status.status[5].eq(serial.data_enable),           # DataEnable
                soc.MMIO_inst.serial_status.status[6].eq(serial.uart_rx.parity_error),  # Parity error
                soc.MMIO_inst.serial_status.status[7].eq(serial.uart_rx.frame_error),   # Frame error
            ]
