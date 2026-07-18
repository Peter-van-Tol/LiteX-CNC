"""Minimal direct Etherbone access to a LiteX-CNC FPGA board.

This is intentionally small and low-level: it talks to the board over UDP
Etherbone and reads or writes raw 32-bit words. It can also use a generated
LiteX `csr.csv` file to resolve CSR names to addresses and a generated
`alias.hal` file to resolve GPIO names to bit positions.
"""

from dataclasses import dataclass
from typing import Any

import click

from litexcnc.tools.csr_map import CsrMap
from litexcnc.tools.alias_map import AliasMap
from litexcnc.tools.etherbone_client import EtherboneClient

class BasedIntParamType(click.ParamType[int]):
    name = "integer"

    def convert(self, value: Any, param: click.Parameter | None, ctx: click.Context | None) -> int:
        if isinstance(value, int):
            return value

        try:
            if value[:2].lower() == "0x":
                return int(value[2:], 16)
            if value[:2].lower() == "0b":
                return int(value[2:], 2)
            return int(value, 10)
        except ValueError:
            self.fail(f"{value!r} is not a valid integer", param, ctx)


BASED_INT = BasedIntParamType()

@dataclass
class RuntimeSettings:
    client: EtherboneClient | None = None

def _runtime_settings(ctx: click.Context) -> RuntimeSettings:
    return ctx.ensure_object(RuntimeSettings)

def client(ctx: click.Context) -> EtherboneClient:
    c = _runtime_settings(ctx).client
    if c is None:
        raise click.ClickException("Etherbone client not initialized")
    return c


@click.group(name="etherbone")
@click.option("--host", required=True, help="Board IP address or hostname")
@click.option("--port", default=1234, show_default=True, type=int, help="Etherbone UDP port")
@click.option("--local-port", default=1234, show_default=True, type=int, help="Local UDP port to bind for replies")
@click.option("--timeout", default=1.0, show_default=True, type=float, help="Socket timeout in seconds")
@click.option("--csr", default="csr.csv", type=click.Path(exists=True, dir_okay=False, resolve_path=True), help="Generated csr.csv file")
@click.option("--alias", default="alias.hal", type=click.Path(exists=True, dir_okay=False, resolve_path=True), help="Generated alias.hal file")
@click.option("--verbose", is_flag=True, help="Enable verbose debug output")
@click.pass_context
def cli(ctx: click.Context, host: str, port: int, local_port: int, timeout: float, csr: str, alias: str, verbose: bool):
    """Raw Etherbone access to the FPGA board."""
    settings = _runtime_settings(ctx)
    settings.client = EtherboneClient(
        host=host,
        csr_map=CsrMap(csr),
        alias_map=AliasMap(alias),
        port=port,
        local_port=local_port,
        timeout=timeout,
        verbose=verbose
    )

@cli.command()
@click.pass_context
def probe(ctx: click.Context):
    """Read the FPGA identity registers and print a short summary."""
    probe_result = client(ctx).probe()

    click.echo(probe_result)


@cli.group()
def gpio():
    """Inspect and toggle GPIO outputs using alias names."""


@gpio.command(name="list")
@click.pass_context
def gpio_list(ctx: click.Context):
    """List GPIO aliases and bit positions."""
    alias_map = client(ctx).alias_map

    for index, labels in alias_map.iter_pins():
        label_text = ", ".join(labels)
        click.echo(f"{index:03d}: {label_text}")


@gpio.command(name="get")
@click.pass_context
@click.argument("pin")
def gpio_get(ctx: click.Context, pin: str):
    """Read a single GPIO output bit by alias or bit index."""
    state = client(ctx).gpio_get(pin)

    click.echo(state)


def _parse_gpio_value(value: str) -> int:
    normalized = value.strip().lower()
    if normalized in {"0x1", "1", "on", "high", "true"}:
        return 1
    if normalized in {"0x0", "0", "off", "low", "false"}:
        return 0
    raise click.ClickException("gpio values must be one of 1, 0, on, off, high, or low")


@gpio.command(name="set")
@click.pass_context
@click.argument("pin")
@click.argument("value")
def gpio_set(ctx: click.Context, pin: str, value: str):
    """Set a single GPIO output bit by alias or bit index."""
    client(ctx).gpio_set(pin, _parse_gpio_value(value))


@cli.group()
def register():
    """Read and write raw 32-bit words to CSR registers by name or address."""


@register.command(name="list")
@click.pass_context
@click.option("--address", is_flag=True, help="Show register addresses")
def register_list(ctx: click.Context, address: bool):
    """List all CSR registers by name."""
    csr_map = client(ctx).csr_map

    for name, register in csr_map.registers.items():
        if address:
            click.echo(f"{name}: 0x{register['address']:08X}")
        else:
            click.echo(name)


@register.command(name="read")
@click.pass_context
@click.argument("register", type=str, required=True)
@click.option("--format", "output_format", type=click.Choice(["dec", "hex", "bin"]), default="dec", show_default=True, help="Output format for the read words")
def register_read(ctx: click.Context, register: str, output_format: str):
    """Read a CSR register by name."""
    words = client(ctx).read_register(register)
    if output_format == "hex":
        formatted_words = [f"0x{word:08X}" for word in words]
    elif output_format == "bin":
        formatted_words = [f"0b{word:032b}" for word in words]
    else:
        formatted_words = [str(word) for word in words]

    click.echo("\n".join(formatted_words))


@register.command(name="write")
@click.pass_context
@click.argument("register", type=str, required=True)
@click.argument("value", nargs=-1, type=BASED_INT, required=True)
def register_write(ctx: click.Context, register: str, value: tuple[int]):
    """Write a CSR register by name."""
    client(ctx).write_register(register, list(value))


@cli.group()
def memory():
    """Read and write raw 32-bit words to memory by address."""


@memory.command(name="read")
@click.pass_context
@click.argument("address", type=BASED_INT, required=True)
@click.argument("count", type=BASED_INT, required=True)
@click.option("--format", "output_format", type=click.Choice(["dec", "hex", "bin"]), default="dec", show_default=True, help="Output format for the read words")
def read_words(ctx: click.Context, address: int, count: int, output_format: str):
    """Read raw 32-bit words from the board."""
    words = client(ctx).read_words(address, count)
    if output_format == "hex":
        formatted_words = [f"0x{word:08X}" for word in words]
    elif output_format == "bin":
        formatted_words = [f"0b{word:032b}" for word in words]
    else:
        formatted_words = [str(word) for word in words]
    click.echo("\n".join(formatted_words))


@memory.command(name="write")
@click.pass_context
@click.argument("address", type=BASED_INT, required=True)
@click.argument("values", nargs=-1, type=BASED_INT, required=True)
def write_words(ctx: click.Context, address: int, values: tuple[int]):
    """Write raw 32-bit words to the board."""
    client(ctx).write_words(address, list(values))


@cli.group()
def watchdog():
    """Control the watchdog."""


@watchdog.command(name="disable")
@click.pass_context
def watchdog_disable(ctx: click.Context):
    """Disable the watchdog."""
    client(ctx).watchdog_disable()


@watchdog.command(name="pet")
@click.pass_context
@click.option("--interval-ms", default=50, show_default=True, type=int, help="How often to refresh the watchdog")
@click.option("--timeout-cycles", type=BASED_INT, help="Watchdog timeout value written to the board")
def pet_watchdog(ctx: click.Context, interval_ms: int, timeout_cycles: int):
    """Pet the watchdog in a loop."""
    client(ctx).watchdog_pet_loop(interval_ms, timeout_cycles)


@cli.group()
def pwm():
    """Control the PWM generator."""


@pwm.command(name="enable")
@click.pass_context
@click.argument("pwm_index", type=BASED_INT, required=True)
def pwm_enable(ctx: click.Context, pwm_index: int):
    """Enable a PWM generator."""
    client(ctx).pwm_enable(pwm_index, True)


@pwm.command(name="disable")
@click.pass_context
@click.argument("pwm_index", type=BASED_INT, required=True)
def pwm_disable(ctx: click.Context, pwm_index: int):
    """Disable a PWM generator."""
    client(ctx).pwm_enable(pwm_index, False)


@pwm.command(name="list")
@click.pass_context
def pwm_list(ctx: click.Context):
    """List all PWM generators."""
    click.echo("\n".join(str(d) for d in client(ctx).pwm_list()))


@pwm.group(name="set")
def pwm_set():
    """Set PWM parameters."""


@pwm_set.command(name="period")
@click.pass_context
@click.argument("pwm_index", type=BASED_INT, required=True)
@click.option("--cycles", type=BASED_INT, help="PWM period in clock cycles")
@click.option("--us", type=float, help="PWM period in microseconds")
def pwm_set_period(ctx: click.Context, pwm_index: int, cycles: int | None, us: float | None):
    """Set the PWM period in clock cycles."""
    if cycles is None and us is None:
        raise click.ClickException("Either --cycles or --us must be specified")
    if cycles is not None and us is not None:
        raise click.ClickException("Only one of --cycles or --us can be specified")
    if cycles is not None:
        client(ctx).pwm_set_period(pwm_index, cycles)
    if us is not None:
        client(ctx).pwm_set_period_us(pwm_index, us)


@pwm_set.command(name="width")
@click.pass_context
@click.argument("pwm_index", type=BASED_INT, required=True)
@click.option("--cycles", type=BASED_INT, help="PWM width in clock cycles")
@click.option("--us", type=float, help="PWM width in microseconds")
def pwm_set_width(ctx: click.Context, pwm_index: int, cycles: int | None, us: float | None):
    """Set the PWM width in clock cycles."""
    if cycles is None and us is None:
        raise click.ClickException("Either --cycles or --us must be specified")
    if cycles is not None and us is not None:
        raise click.ClickException("Only one of --cycles or --us can be specified")
    if cycles is not None:
        client(ctx).pwm_set_width(pwm_index, cycles)
    if us is not None:
        client(ctx).pwm_set_width_us(pwm_index, us)


@pwm_set.command(name="frequency")
@click.pass_context
@click.argument("pwm_index", type=BASED_INT, required=True)
@click.argument("frequency_hz", type=float, required=True)
def pwm_set_frequency(ctx: click.Context, pwm_index: int, frequency_hz: float):
    """Set the PWM frequency in Hertz."""
    client(ctx).pwm_set_frequency(pwm_index, frequency_hz)


@pwm_set.command(name="duty")
@click.pass_context
@click.argument("pwm_index", type=BASED_INT, required=True)
@click.argument("duty_cycle", type=float, required=True)
def pwm_set_duty(ctx: click.Context, pwm_index: int, duty_cycle: float):
    """Set the PWM duty cycle as a percentage."""
    client(ctx).pwm_set_duty_cycle(pwm_index, duty_cycle)