import click
from os import path

from litex.soc.integration.builder import Builder

# Local imports
from .soc import LitexCNC_Firmware

@click.command()
@click.argument('input', type=click.File('r'))
def build_soc(input):
    """Builds the litexCNC firmware with the given configuration."""
    # Generate the firmware
    firmware_config = LitexCNC_Firmware.parse_raw(' '.join(input.readlines()))
    soc = firmware_config.generate()
    
    # Build the SOC
    builder = Builder(
        soc, 
        output_dir=path.splitext(path.basename(input.name))[0],
        csr_csv=path.join(path.splitext(path.basename(input.name))[0], "csr.csv")
    )
    builder.build(run=False)


if __name__ == '__main__':
    build_soc()
