import click

from litexcnc.tools import bit_to_flash

@click.command()
@click.argument('bit', type=click.Path(exists=True, readable=True, dir_okay=False, resolve_path=True))
@click.argument('svf', type=click.Path(exists=False, writable=True, dir_okay=False, resolve_path=True))
def cli(bit, svf):
    """Converts the generated .bit-file to a svf-file which is stored in flash"""
    click.echo(click.style("INFO", fg="blue") + f": Converting {bit} to flash...")
    bit_to_flash(bit, svf)
    click.echo(click.style("INFO", fg="blue") + f": Conversion complete, output stored in {svf}!")
