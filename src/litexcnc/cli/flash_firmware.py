from pathlib import Path
import subprocess

import click

from litexcnc.tools import bit_to_flash

@click.command()
@click.argument('file', type=click.Path(exists=False, writable=True, dir_okay=False, resolve_path=True))
@click.option('--permanent', is_flag=True, help="Converts the .bit-file to a file which is stored in flash")
@click.option('--programmer', help="When defined, use the given programmer instead of the RPi.")
def cli(file, permanent, programmer):
    """Flashes the .svf-file to the LED-card"""
    file: Path = Path(file)
    if file.suffix != '.svf':
        click.echo(click.style("WARNING", fg="yellow") + f": {file} seems not to be a SVF-file...")
    
    # Convert the bit-file if users want to make the firmware permanent
    if permanent:
        convert_in = file.with_suffix(".bit")
        convert_out = file.with_suffix(".flash")
        click.echo(click.style("INFO", fg="blue") + f": Converting {file} to flash...")
        if not convert_in.exists():
            click.echo(click.style("ERROR", fg="red") + f": Cannot find {convert_in} ...")
            return -1
        bit_to_flash(convert_in, convert_out)
        click.echo(click.style("INFO", fg="blue") + f": Conversion complete, output stored in {convert_out}!")
        file = convert_out
    
    # Determine config
    if programmer:
        programmer = f"interface/{programmer}.cfg"
    else:
        programmer = Path(__file__).parent / "config" / "hub75hat.cfg"

    # Flash the firmware to the board using OpenOCD
    command = [
        'openocd',
        '-f', str(programmer), 
        '-c', '"transport select jtag"',
        '-f', 'fpga/lattice_ecp5.cfg',
        '-c', f'"init; svf -quiet -progress {file}; exit"'
    ]
    click.echo(click.style("INFO", fg="blue") + f": Flashing firmware to LED-card.")
    ret = subprocess.call(
        ' '.join(command), 
        cwd=file.parent,
        shell=True
    )

    # Indicate success
    if ret == 0:
        click.echo(click.style("INFO", fg="blue") + ": Successfully flashed the LED-card")
    else: 
        click.echo(click.style("ERROR", fg="red") + ": An error has occurred during flashing the firmware.")
    