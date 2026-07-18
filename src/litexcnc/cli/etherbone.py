"""Minimal direct Etherbone access to a LiteX-CNC FPGA board.

This is intentionally small and low-level: it talks to the board over UDP
Etherbone and reads or writes raw 32-bit words. It can also use a generated
LiteX `csr.csv` file to resolve CSR names to addresses and a generated
`alias.hal` file to resolve GPIO names to bit positions.
"""

from __future__ import annotations

import csv
import re
import socket
import struct
import time
from dataclasses import dataclass
from collections import defaultdict
from pathlib import Path

import click


class IntAuto(click.ParamType):
    name = "int"

    def convert(self, value, param, ctx):
        try:
            return int(value, 0)
        except ValueError as exc:
            self.fail(f"{value!r} is not a valid integer", param, ctx)


INT_AUTO = IntAuto()


class CsrMap:
    def __init__(self, csv_path: str | Path):
        self.path = Path(csv_path)
        self.registers: dict[str, dict[str, int | str]] = {}
        self.constants: dict[str, str | int | None] = {}
        self._load()

    def _load(self) -> None:
        with self.path.open(newline="") as csv_file:
            reader = csv.reader(csv_file)
            for row in reader:
                if not row or row[0].startswith("#"):
                    continue
                kind = row[0]
                if kind == "csr_register":
                    name = row[1]
                    self.registers[name] = {
                        "addr": int(row[2], 0),
                        "size": int(row[3], 0),
                        "type": row[4],
                    }
                elif kind == "constant":
                    value = row[2]
                    if value in {"None", "none", "null", "NULL"}:
                        parsed: str | int | None = None
                    else:
                        try:
                            parsed = int(value, 0)
                        except ValueError:
                            parsed = value
                    self.constants[row[1]] = parsed

    def resolve(self, value: str | int) -> tuple[int, int, str]:
        if isinstance(value, int):
            return value, 1, f"0x{value:08X}"
        if value in self.registers:
            register = self.registers[value]
            return int(register["addr"]), int(register["size"]), value
        return int(value, 0), 1, value


class AliasMap:
    PIN_PATTERN = re.compile(r"^(.+)\.gpio\.(\d+)\.out$")

    def __init__(self, alias_path: str | Path):
        self.path = Path(alias_path)
        self.index_to_aliases: dict[int, list[str]] = defaultdict(list)
        self.alias_to_index: dict[str, int] = {}
        self.short_alias_to_indices: dict[str, list[int]] = defaultdict(list)
        self._load()

    @classmethod
    def from_csr_path(cls, csr_path: str | Path) -> "AliasMap | None":
        alias_path = Path(csr_path).with_name("alias.hal")
        if not alias_path.exists():
            return None
        return cls(alias_path)

    def _load(self) -> None:
        with self.path.open() as alias_file:
            for raw_line in alias_file:
                line = raw_line.strip()
                if not line or line.startswith("#"):
                    continue
                parts = line.split()
                if len(parts) != 4 or parts[0] != "alias" or parts[1] != "pin":
                    continue
                match = self.PIN_PATTERN.match(parts[2])
                if not match:
                    continue
                index = int(match.group(2))
                alias_name = parts[3].rsplit(".", 1)[0]
                self.index_to_aliases[index].append(alias_name)
                self.alias_to_index[alias_name.lower()] = index
                short_alias = alias_name.rsplit(".", 1)[-1]
                self.short_alias_to_indices[short_alias.lower()].append(index)

    def resolve(self, token: str | int) -> tuple[int, str]:
        if isinstance(token, int):
            return token, f"{token}"
        token_lower = token.lower()
        if token_lower in self.alias_to_index:
            return self.alias_to_index[token_lower], token
        if token_lower in self.short_alias_to_indices:
            indices = self.short_alias_to_indices[token_lower]
            if len(indices) == 1:
                return indices[0], token
            raise click.ClickException(
                f"alias {token!r} is ambiguous; use a full alias name or a numeric index"
            )
        return int(token, 0), token

    def labels_for(self, index: int) -> list[str]:
        return self.index_to_aliases.get(index, [])

    def iter_pins(self):
        for index in sorted(self.index_to_aliases):
            yield index, self.index_to_aliases[index]


def _pack_header(address: int, write_count: int = 0, read_count: int = 0) -> bytearray:
    packet = bytearray(16)
    packet[0:10] = bytes((0x4E, 0x6F, 0x10, 0x44, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0F))
    packet[10] = write_count & 0xFF
    packet[11] = read_count & 0xFF
    packet[12:16] = struct.pack(">I", address & 0xFFFFFFFF)
    return packet


def _pack_write(address: int, values: list[int]) -> bytes:
    packet = _pack_header(address, write_count=len(values))
    for value in values:
        packet.extend(struct.pack(">I", value & 0xFFFFFFFF))
    return bytes(packet)


def _pack_read(address: int, count: int) -> bytes:
    packet = _pack_header(0, read_count=count)
    packet.extend(
        struct.pack(
            ">" + "I" * count,
            *[address + (4 * index) for index in range(count)]
        )
    )
    return bytes(packet)


def _unpack_words(payload: bytes) -> list[int]:
    if len(payload) < 16:
        raise click.ClickException(f"short Etherbone response: {len(payload)} bytes")
    data = payload[16:]
    if len(data) % 4 != 0:
        raise click.ClickException("Etherbone response payload is not 32-bit aligned")
    if not data:
        return []
    return list(struct.unpack(">" + "I" * (len(data) // 4), data))


def _decode_u32(value: int) -> str:
    packed = struct.pack(">I", value & 0xFFFFFFFF)
    try:
        text = packed.rstrip(b"\x00").decode("ascii")
    except UnicodeDecodeError:
        return ""
    if text and all(32 <= ord(char) < 127 for char in text):
        return text
    return ""


@dataclass
class EtherboneClient:
    host: str
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

    def __exit__(self, exc_type, exc, tb):
        self.close()

    def write_words(self, address: int, values: list[int]) -> None:
        self._socket.send(_pack_write(address, values))

    def read_words(self, address: int, count: int) -> list[int]:
        self._socket.send(_pack_read(address, count))
        response = self._socket.recv(16 + (count * 4))
        words = _unpack_words(response)
        if len(words) != count:
            raise click.ClickException(f"expected {count} words, got {len(words)}")
        return words


def _runtime_settings(ctx: click.Context) -> dict[str, str | int | float | None]:
    settings = ctx.ensure_object(dict)
    return {
        "host": settings.get("host"),
        "port": settings.get("port", 1234),
        "local_port": settings.get("local_port", 1234),
        "timeout": settings.get("timeout", 1.0),
        "csr": settings.get("csr"),
        "aliases": settings.get("aliases"),
    }


def _require_setting(value, name: str):
    if value is None:
        raise click.ClickException(f"{name} is required on the etherbone group")
    return value


@click.group(name="etherbone")
@click.option("--host", help="Board IP address or hostname")
@click.option("--port", default=1234, show_default=True, type=int, help="Etherbone UDP port")
@click.option("--local-port", default=1234, show_default=True, type=int, help="Local UDP port to bind for replies")
@click.option("--timeout", default=1.0, show_default=True, type=float, help="Socket timeout in seconds")
@click.option("--csr", type=click.Path(exists=True, dir_okay=False, resolve_path=True), help="Generated csr.csv file")
@click.option("--aliases", type=click.Path(exists=True, dir_okay=False, resolve_path=True), help="Generated alias.hal file")
@click.pass_context
def cli(ctx, host, port, local_port, timeout, csr, aliases):
    """Raw Etherbone access to the FPGA board."""
    ctx.ensure_object(dict)
    ctx.obj.update({
        "host": host,
        "port": port,
        "local_port": local_port,
        "timeout": timeout,
        "csr": csr,
        "aliases": aliases,
    })


def _load_csr_map(csr: str | None) -> CsrMap | None:
    if not csr:
        return None
    return CsrMap(csr)


def _load_alias_map(csr: str | None, aliases: str | None) -> AliasMap | None:
    if aliases:
        return AliasMap(aliases)
    if csr:
        return AliasMap.from_csr_path(csr)
    return None


def _resolve_target(csr_map: CsrMap | None, target: str | int) -> tuple[int, int, str]:
    if csr_map is None:
        if isinstance(target, int):
            return target, 1, f"0x{target:08X}"
        return INT_AUTO.convert(target, None, None), 1, target
    return csr_map.resolve(target)


def _parse_gpio_value(value: str) -> int:
    normalized = value.strip().lower()
    if normalized in {"1", "on", "high", "true", "set"}:
        return 1
    if normalized in {"0", "off", "low", "false", "clear", "unset"}:
        return 0
    raise click.ClickException("gpio values must be one of 1, 0, on, off, high, or low")


def _resolve_gpio_bit(pin: str, alias_map: AliasMap | None) -> int:
    if alias_map is not None:
        return alias_map.resolve(pin)[0]
    try:
        return int(pin, 0)
    except ValueError as exc:
        raise click.ClickException("named GPIO pins require alias.hal; numeric pins must be decimal or hex") from exc


def _gpio_register_address(csr_map: CsrMap | None) -> int:
    if csr_map is None:
        raise click.ClickException("gpio commands require --csr")
    address, _, _ = csr_map.resolve("MMIO_inst_gpio_out")
    return address


def _read_gpio_words(client: EtherboneClient, csr_map: CsrMap) -> list[int]:
    address = _gpio_register_address(csr_map)
    return client.read_words(address, 4)


def _write_gpio_words(client: EtherboneClient, csr_map: CsrMap, words: list[int]) -> None:
    address = _gpio_register_address(csr_map)
    client.write_words(address, words)


def _watchdog_value(timeout_cycles: int) -> int:
    if timeout_cycles < 1:
        raise click.ClickException("timeout-cycles must be at least 1")
    return 0x80000000 | (timeout_cycles & 0x7FFFFFFF)


def _pet_watchdog_loop(
    host: str,
    port: int,
    local_port: int,
    timeout: float,
    csr: str,
    timeout_cycles: int,
    interval_ms: float,
    count: int,
) -> None:
    csr_map = CsrMap(csr)
    watchdog_addr, _, _ = csr_map.resolve("MMIO_inst_watchdog_data")
    value = _watchdog_value(timeout_cycles)
    interval_seconds = interval_ms / 1000.0

    click.echo(
        f"watchdog: writing 0x{value:08X} to MMIO_inst_watchdog_data at 0x{watchdog_addr:08X} every {interval_ms:g} ms"
    )

    with EtherboneClient(host=host, port=port, local_port=local_port, timeout=timeout) as client:
        refreshes = 0
        while count == 0 or refreshes < count:
            client.write_words(watchdog_addr, [value])
            refreshes += 1
            if count == 0 or refreshes < count:
                time.sleep(interval_seconds)


@cli.command()
@click.pass_context
def probe(ctx: click.Context):
    """Read the FPGA identity registers and print a short summary."""
    settings = _runtime_settings(ctx)
    host = _require_setting(settings["host"], "--host")
    port = settings["port"]
    local_port = settings["local_port"]
    timeout = settings["timeout"]
    csr = _require_setting(settings["csr"], "--csr")
    csr_map = _load_csr_map(csr)
    if csr_map is None:
        raise click.ClickException("probe requires --csr so register names can be resolved")

    with EtherboneClient(host=host, port=port, local_port=local_port, timeout=timeout) as client:
        magic_addr, _, _ = csr_map.resolve("MMIO_inst_magic")
        version_addr, _, _ = csr_map.resolve("MMIO_inst_version")
        freq_addr, _, _ = csr_map.resolve("MMIO_inst_clock_frequency")
        module_addr, _, _ = csr_map.resolve("MMIO_inst_module_config")
        name1_addr, _, _ = csr_map.resolve("MMIO_inst_name1")
        name2_addr, _, _ = csr_map.resolve("MMIO_inst_name2")
        name3_addr, _, _ = csr_map.resolve("MMIO_inst_name3")
        name4_addr, _, _ = csr_map.resolve("MMIO_inst_name4")

        magic = client.read_words(magic_addr, 1)[0]
        version = client.read_words(version_addr, 1)[0]
        clock_frequency = client.read_words(freq_addr, 1)[0]
        module_config = client.read_words(module_addr, 1)[0]
        name_words = client.read_words(name1_addr, 4)

    board_name = "".join(_decode_u32(word) for word in name_words).rstrip("\x00")
    version_major = (version >> 16) & 0xFF
    version_minor = (version >> 8) & 0xFF
    version_patch = version & 0xFF
    module_data_size = module_config & 0xFFFF
    num_modules = (module_config >> 16) & 0xFF

    click.echo(f"magic:         0x{magic:08X}")
    click.echo(f"version:       {version_major}.{version_minor}.{version_patch}")
    click.echo(f"clock:         {clock_frequency} Hz")
    click.echo(f"board name:    {board_name or '<unprintable>'}")
    click.echo(f"modules:       {num_modules}")
    click.echo(f"module bytes:  {module_data_size}")
    click.echo(f"csr map:       {csr_map.path}")


@cli.group()
def gpio():
    """Inspect and toggle GPIO outputs using alias names."""


@gpio.command(name="list")
@click.pass_context
def gpio_list(ctx: click.Context):
    """List GPIO aliases and bit positions."""
    settings = _runtime_settings(ctx)
    csr = _require_setting(settings["csr"], "--csr")
    aliases = settings["aliases"]
    alias_map = _load_alias_map(csr, aliases)
    if alias_map is None:
        raise click.ClickException("no alias.hal found next to csr.csv")

    for index, labels in alias_map.iter_pins():
        label_text = ", ".join(labels)
        click.echo(f"{index:03d}: {label_text}")


@gpio.command(name="get")
@click.pass_context
@click.argument("pin")
def gpio_get(ctx: click.Context, pin: str):
    """Read a single GPIO output bit by alias or bit index."""
    settings = _runtime_settings(ctx)
    host = _require_setting(settings["host"], "--host")
    port = settings["port"]
    local_port = settings["local_port"]
    timeout = settings["timeout"]
    csr = _require_setting(settings["csr"], "--csr")
    aliases = settings["aliases"]
    csr_map = _load_csr_map(csr)
    alias_map = _load_alias_map(csr, aliases)
    if csr_map is None:
        raise click.ClickException("gpio get requires --csr")
    bit_index = _resolve_gpio_bit(pin, alias_map)
    word_index = bit_index // 32
    bit_mask = 1 << (bit_index % 32)

    with EtherboneClient(host=host, port=port, local_port=local_port, timeout=timeout) as client:
        words = _read_gpio_words(client, csr_map)

    value = 1 if (words[word_index] & bit_mask) else 0
    labels = alias_map.labels_for(bit_index) if alias_map else []
    label_text = f" ({', '.join(labels)})" if labels else ""
    click.echo(f"bit {bit_index:03d}{label_text}: {value}")


@gpio.command(name="set")
@click.pass_context
@click.argument("pin")
@click.argument("value")
def gpio_set(ctx: click.Context, pin: str, value: str):
    """Set a single GPIO output bit by alias or bit index."""
    settings = _runtime_settings(ctx)
    host = _require_setting(settings["host"], "--host")
    port = settings["port"]
    local_port = settings["local_port"]
    timeout = settings["timeout"]
    csr = _require_setting(settings["csr"], "--csr")
    aliases = settings["aliases"]
    csr_map = _load_csr_map(csr)
    alias_map = _load_alias_map(csr, aliases)
    if csr_map is None:
        raise click.ClickException("gpio set requires --csr")
    bit_index = _resolve_gpio_bit(pin, alias_map)
    bit_value = _parse_gpio_value(value)
    word_index = bit_index // 32
    bit_mask = 1 << (bit_index % 32)

    with EtherboneClient(host=host, port=port, local_port=local_port, timeout=timeout) as client:
        words = _read_gpio_words(client, csr_map)
        if bit_value:
            words[word_index] |= bit_mask
        else:
            words[word_index] &= ~bit_mask
        _write_gpio_words(client, csr_map, words)

    labels = alias_map.labels_for(bit_index) if alias_map else []
    label_text = f" ({', '.join(labels)})" if labels else ""
    click.echo(f"bit {bit_index:03d}{label_text}: {bit_value}")

@cli.command()
@click.pass_context
@click.option("--address", type=INT_AUTO, help="CSR address to read from")
@click.option("--register", type=str, help="CSR register name to read from")
@click.option("--count", default=1, show_default=True, type=click.IntRange(min=1), help="Number of 32-bit words to read")
def read(ctx: click.Context, address: int | None, register: str | None, count: int):
    """Read raw 32-bit words from the board."""
    settings = _runtime_settings(ctx)
    host = _require_setting(settings["host"], "--host")
    port = settings["port"]
    local_port = settings["local_port"]
    timeout = settings["timeout"]
    csr = settings["csr"]
    if (address is None) == (register is None):
        raise click.ClickException("provide exactly one of --address or --register")

    csr_map = _load_csr_map(csr)
    if register is not None and csr_map is None:
        raise click.ClickException("--register requires --csr")

    target = register if register is not None else address
    resolved_address, resolved_count, _ = _resolve_target(csr_map, target)
    if register is not None:
        count = max(count, resolved_count)

    with EtherboneClient(host=host, port=port, local_port=local_port, timeout=timeout) as client:
        words = client.read_words(resolved_address, count)

    for index, word in enumerate(words):
        current_address = resolved_address + (index * 4)
        click.echo(f"0x{current_address:08X}: 0x{word:08X}")


@cli.command()
@click.pass_context
@click.option("--address", type=INT_AUTO, help="CSR address to write to")
@click.option("--register", type=str, help="CSR register name to write to")
@click.argument("values", nargs=-1, type=INT_AUTO, required=True)
def write(ctx: click.Context, address: int | None, register: str | None, values: tuple[int, ...]):
    """Write raw 32-bit words to the board."""
    settings = _runtime_settings(ctx)
    host = _require_setting(settings["host"], "--host")
    port = settings["port"]
    local_port = settings["local_port"]
    timeout = settings["timeout"]
    csr = settings["csr"]
    if (address is None) == (register is None):
        raise click.ClickException("provide exactly one of --address or --register")

    csr_map = _load_csr_map(csr)
    if register is not None and csr_map is None:
        raise click.ClickException("--register requires --csr")

    target = register if register is not None else address
    resolved_address, _, _ = _resolve_target(csr_map, target)

    with EtherboneClient(host=host, port=port, local_port=local_port, timeout=timeout) as client:
        client.write_words(resolved_address, list(values))

    click.echo(f"Wrote {len(values)} word(s) to 0x{resolved_address:08X}")


@cli.command(name="pet-watchdog")
@click.pass_context
@click.option("--timeout-cycles", default=1_000_000, show_default=True, type=int, help="Watchdog timeout value written to the board")
@click.option("--interval-ms", default=50.0, show_default=True, type=float, help="How often to refresh the watchdog")
@click.option("--count", default=0, show_default=True, type=int, help="Number of refreshes to send, 0 means run forever")
def pet_watchdog(ctx: click.Context, timeout_cycles: int, interval_ms: float, count: int):
    """Pet the watchdog in a loop."""
    settings = _runtime_settings(ctx)
    host = _require_setting(settings["host"], "--host")
    port = settings["port"]
    local_port = settings["local_port"]
    timeout = settings["timeout"]
    csr = _require_setting(settings["csr"], "--csr")
    _pet_watchdog_loop(host, port, local_port, timeout, csr, timeout_cycles, interval_ms, count)