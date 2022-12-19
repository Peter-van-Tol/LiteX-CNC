"""
This file contains the command to build the firmware based on a JSON-configuration.
"""
import binascii
import os
import click

@click.command()
@click.argument('config', type=click.File('r'))
@click.option('-o', '--output-directory')
@click.option('--build', is_flag=True)
def cli(config, output_directory, build):
    """Builds the litexCNC firmware with the given configuration"""
    # Local imports, with check whether Litex is available on path
    try:
        from litex.soc.integration.builder import Builder
        from litexcnc.firmware.soc import LitexCNC_Firmware
    except ImportError as e:
        click.echo(click.style("Error", fg="red") + ": Litex is not installed. Please run 'litexcnc install_litex' first.")
        return -1
    
    # Set the default value for the folder if not set
    if not output_directory:
        output_directory = os.path.basename(config.name)
    
    # Create folder if not exists
    if not os.path.exists(output_directory):
        os.makedirs(output_directory)

    # Add the config name as folder
    output_directory = os.path.join(
        output_directory,
        os.path.splitext(config.name)[0]
    )

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
        output_dir=output_directory,
        csr_csv=os.path.join(output_directory, "csr.csv")
    )
    builder.build(run=build)

    # Done!
    click.echo(click.style("INFO", fg="blue") + f": Firmware created in {output_directory}")