from dataclasses import dataclass
import socket
import struct
import time

from litexcnc.tools.csr_map import CsrMap
from litexcnc.tools.alias_map import AliasMap

# generates the Etherbone packet header for a given address, write count, and read count
def etherbone_header(address: int, write_count: int = 0, read_count: int = 0) -> bytearray:
    packet = bytearray(16)
    packet[0:10] = bytes((0x4E, 0x6F, 0x10, 0x44, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0F))
    packet[10] = write_count & 0xFF
    packet[11] = read_count & 0xFF
    packet[12:16] = struct.pack(">I", address & 0xFFFFFFFF)
    return packet

# generates a read packet for the given address and count of 32-bit words
def etherbone_read(address: int, count: int) -> bytes:
    packet = etherbone_header(0, read_count=count)
    packet.extend(
        struct.pack(
            ">" + "I" * count,
            *[address + (4 * index) for index in range(count)]
        )
    )
    return bytes(packet)

# generates a write packet for the given address and list of 32-bit values
def etherbone_write(address: int, values: list[int]) -> bytes:
    packet = etherbone_header(address, write_count=len(values))
    for value in values:
        packet.extend(struct.pack(">I", value & 0xFFFFFFFF))
    return bytes(packet)

def etherbone_unpack(payload: bytes) -> list[int]:
    if len(payload) < 16:
        raise Exception(f"short Etherbone response: {len(payload)} bytes")

    # skip the 16-byte header and unpack the rest of the payload as 32-bit words
    data = payload[16:]

    if len(data) % 4 != 0:
        raise Exception("Etherbone response payload is not 32-bit aligned")

    if not data:
        return []
    return list(struct.unpack(">" + "I" * (len(data) // 4), data))

def decode_u32(value: int) -> str:
    packed = struct.pack(">I", value & 0xFFFFFFFF)
    try:
        text = packed.rstrip(b"\x00").decode("ascii")
    except UnicodeDecodeError:
        return ""
    if text and all(32 <= ord(char) < 127 for char in text):
        return text
    return ""

def decode_version(value: int) -> tuple[int, int, int]:
    major = (value >> 16) & 0xFF
    minor = (value >> 8) & 0xFF
    patch = value & 0xFF
    return (major, minor, patch)

def decode_module_config(value: int) -> tuple[int, int]:
    module_data_size = value & 0xFFFF
    num_modules = (value >> 16) & 0xFF
    return (module_data_size, num_modules)

def encode_watchdog_data(enabled: bool, timeout_cycles: int) -> int:
    if enabled:
        return 0x8000000 | (timeout_cycles & 0x7FFFFFFF)
    else:
        return 0x0

@dataclass
class ProbeResult:
    magic: int
    version: tuple[int, int, int]
    clock_frequency: int
    module_data_size: int
    num_modules: int
    board_name: str

    def __str__(self) -> str:
        version_str = ".".join(str(part) for part in self.version)
        return (
            f"Magic:\t\t\t0x{self.magic:08X}\n"
            f"Version:\t\t{version_str}\n"
            f"Clock Frequency:\t{self.clock_frequency} Hz ({self.clock_frequency / 1e6:.1f} MHz)\n"
            f"Module Data Size:\t{self.module_data_size} bytes\n"
            f"Number of Modules:\t{self.num_modules}\n"
            f"Board Name:\t\t{self.board_name or '<unprintable>'}"
        )

@dataclass
class PWMState:
    index: int
    enabled: bool
    clock_frequency: int
    period_cycles: int
    width_cycles: int

    def __str__(self) -> str:
        if self.period_cycles == 0 or self.width_cycles == 0:
            return (
                f"PWM {self.index}:\n"
                f"  Enabled: {self.enabled}\n"
            )
        else:
            return (
                f"PWM {self.index}:\n"
                f"  Enabled: {self.enabled}\n"
                f"  Frequency: {self.clock_frequency / self.period_cycles:.2f} Hz\n"
                f"  Period: {self.clock_frequency / self.period_cycles:.2f} ms ({self.period_cycles} cycles)\n"
                f"  Width: {self.clock_frequency / self.width_cycles:.2f} ms ({self.width_cycles} cycles)\n"
                f"  Duty Cycle: {self.width_cycles / self.period_cycles:.2%}\n"
            )

@dataclass
class EtherboneClient:
    host: str
    csr_map: CsrMap
    alias_map: AliasMap
    port: int = 1234
    local_port: int = 1234
    timeout: float = 1.0

    def __post_init__(self) -> None:
        self._socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self._socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self._socket.bind(("", self.local_port))
        self._socket.settimeout(self.timeout)
        self._socket.connect((self.host, self.port))

    def close(self) -> None:
        self._socket.close()

    def __enter__(self):
        return self

    def __exit__(self):
        self.close()

    def read_words(self, address: int, count: int) -> list[int]:
        self._socket.send(etherbone_read(address, count))
        response = self._socket.recv(16 + (count * 4))

        words = etherbone_unpack(response)
        if len(words) != count:
            raise Exception(f"expected {count} words, got {len(words)}")

        return words

    def write_words(self, address: int, values: list[int]) -> None:
        self._socket.send(etherbone_write(address, values))

    def read_register(self, register_name: str) -> list[int]:
        address, length, _ = self.csr_map.resolve(register_name)
        return self.read_words(address, length)

    def write_register(self, register_name: str, values: list[int]) -> None:
        address, length, _ = self.csr_map.resolve(register_name)
        if len(values) != length:
            raise Exception(f"expected {length} values for register {register_name}, got {len(values)}")
        self.write_words(address, values)

    def probe(self) -> ProbeResult:
        magic = self.read_register("MMIO_inst_magic")[0]
        version = decode_version(self.read_register("MMIO_inst_version")[0])
        freq = self.read_register("MMIO_inst_clock_frequency")[0]
        module_data_size, num_modules = decode_module_config(self.read_register("MMIO_inst_module_config")[0])
        names = [self.read_register(f"MMIO_inst_name{i}") for i in range(1, 5)]
        names = [decode_u32(name[0]) for name in names]
        board_name = "".join(names).rstrip("\x00")

        return ProbeResult(
            magic=magic,
            version=version,
            clock_frequency=freq,
            module_data_size=module_data_size,
            num_modules=num_modules,
            board_name=board_name
        )

    def gpio_config(self):
        config_data = self.read_register("MMIO_inst_gpio_config_data")
        return config_data

    def gpio_get(self, pin: str | int) -> int:
        gpio_config = self.gpio_config()
        index, _ = self.alias_map.resolve(pin)
        if gpio_config[index] == 0:
            gpio_data = self.read_register("MMIO_inst_gpio_in")
        else:
            gpio_data = self.read_register("MMIO_inst_gpio_out")
        return gpio_data[index]

    def gpio_set(self, pin: str | int, value: int) -> None:
        gpio_config = self.gpio_config()
        bit_index, _ = self.alias_map.resolve(pin)
        if gpio_config[bit_index] == 0:
            raise Exception(f"GPIO pin {pin} is configured as input, cannot set value")

        word_index = bit_index // 32
        bit_mask = 1 << (bit_index % 32)

        gpio_data = self.read_register("MMIO_inst_gpio_out")

        # set the bit
        if value:
            gpio_data[word_index] |= bit_mask
        else:
            gpio_data[word_index] &= ~bit_mask

        self.write_register("MMIO_inst_gpio_out", gpio_data)

    def watchdog_disable(self):
        self.write_register("MMIO_inst_watchdog_data", [encode_watchdog_data(False, 0)])

    def watchdog_pet(self, timeout_cycles: int):
        self.write_register("MMIO_inst_watchdog_data", [encode_watchdog_data(True, timeout_cycles)])

    def watchdog_pet_loop(self, interval_ms: int, timeout_cycles: int | None = None):
        clock_frequency = self.probe().clock_frequency

        if timeout_cycles is None:
            # calculate required timeout cycles based on the interval and clock frequency
            timeout_cycles = int((interval_ms / 1000) * clock_frequency * 2)
        else:
            # ensure the timeout cycles is at least twice the interval cycles
            min_timeout_cycles = int((interval_ms / 1000) * clock_frequency * 2)
            if timeout_cycles < min_timeout_cycles:
                raise Exception(f"timeout_cycles must be at least {min_timeout_cycles} for the given interval_ms")

        while True:
            self.watchdog_pet(timeout_cycles)
            time.sleep(interval_ms / 1000)

    def pwm_list(self) -> list[PWMState]:
        output: list[PWMState] = []
        current_index = 0
        clock_frequency = self.probe().clock_frequency
        while self.csr_map.registers.get(f"MMIO_inst_pwm_{current_index}_period") is not None:
            enabled = self.pwm_get_enabled(current_index)
            period_cycles = self.pwm_get_period(current_index)
            width_cycles = self.pwm_get_width(current_index)
            output.append(PWMState(
                index=current_index,
                enabled=enabled,
                clock_frequency=clock_frequency,
                period_cycles=period_cycles,
                width_cycles=width_cycles
            ))
            current_index += 1
        return output

    def pwm_enable(self, pwm_index: int, enable: bool):
        pwm_config = self.read_register("MMIO_inst_pwm_enable")

        word_index = pwm_index // 32
        bit_mask = 1 << (pwm_index % 32)

        if enable:
            pwm_config[word_index] |= bit_mask
        else:
            pwm_config[word_index] &= ~bit_mask

        self.write_register("MMIO_inst_pwm_enable", pwm_config)

    def pwm_get_enabled(self, pwm_index: int) -> bool:
        pwm_config = self.read_register("MMIO_inst_pwm_enable")

        word_index = pwm_index // 32
        bit_mask = 1 << (pwm_index % 32)

        return bool(pwm_config[word_index] & bit_mask)

    def pwm_get_period(self, pwm_index: int) -> int:
        return self.read_register(f"MMIO_inst_pwm_{pwm_index}_period")[0]

    def pwm_set_period(self, pwm_index: int, period_cycles: int):
        self.write_register(f"MMIO_inst_pwm_{pwm_index}_period", [period_cycles])

    def pwm_set_period_us(self, pwm_index: int, period_us: float):
        clock_frequency = self.probe().clock_frequency
        period_cycles = int((period_us / 1_000_000) * clock_frequency)
        self.pwm_set_period(pwm_index, period_cycles)

    def pwm_get_width(self, pwm_index: int) -> int:
        return self.read_register(f"MMIO_inst_pwm_{pwm_index}_width")[0]

    def pwm_set_width(self, pwm_index: int, duty_cycles: int):
        self.write_register(f"MMIO_inst_pwm_{pwm_index}_width", [duty_cycles])

    def pwm_set_width_us(self, pwm_index: int, duty_us: float):
        clock_frequency = self.probe().clock_frequency
        duty_cycles = int((duty_us / 1_000_000) * clock_frequency)
        self.pwm_set_width(pwm_index, duty_cycles)

    def pwm_get_frequency(self, pwm_index: int) -> float:
        clock_frequency = self.probe().clock_frequency
        period_cycles = self.pwm_get_period(pwm_index)
        if period_cycles == 0:
            raise Exception(f"PWM {pwm_index} period is zero, cannot calculate frequency")
        return clock_frequency / period_cycles

    def pwm_set_frequency(self, pwm_index: int, frequency_hz: float):
        clock_frequency = self.probe().clock_frequency
        period_cycles = int(clock_frequency / frequency_hz)
        self.pwm_set_period(pwm_index, period_cycles)

    def pwm_get_duty_cycle(self, pwm_index: int) -> float:
        period_cycles = self.pwm_get_period(pwm_index)
        duty_cycles = self.pwm_get_width(pwm_index)
        if period_cycles == 0:
            raise Exception(f"PWM {pwm_index} period is zero, cannot calculate duty cycle")
        return duty_cycles / period_cycles

    def pwm_set_duty_cycle(self, pwm_index: int, duty_cycle: float):
        if not (0.0 <= duty_cycle <= 100.0):
            raise Exception("duty_cycle must be between 0.0 and 100.0")

        period_cycles = self.pwm_get_period(pwm_index)
        duty_cycles = int(period_cycles * duty_cycle / 100.0)
        self.pwm_set_width(pwm_index, duty_cycles)