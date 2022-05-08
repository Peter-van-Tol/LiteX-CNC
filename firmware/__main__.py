import binascii
import click
from os import path

from litex.soc.integration.builder import Builder

# Local imports
from .soc import LitexCNC_Firmware

@click.command()
@click.argument('config', type=click.File('r'))
def build_soc(config):
    """Builds the litexCNC firmware with the given configuration."""

    # Load configuration
    firmware_config = LitexCNC_Firmware.parse_raw(' '.join(config.readlines()))

    # Generate the fingerprint
    with open(config.name, 'rb') as config_binary:
        fingerprint = binascii.crc32(config_binary.read())
        print("Generated fingerprint (CRC): ", fingerprint)

    # Generate the firmware
    soc = firmware_config.generate(fingerprint)
    
    # Build the SOC
    builder = Builder(
        soc, 
        output_dir=path.splitext(path.basename(config.name))[0],
        csr_csv=path.join(path.splitext(path.basename(config.name))[0], "csr.csv")
    )
    builder.build(run=False)


if __name__ == '__main__':
    build_soc()
