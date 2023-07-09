"""
This file contains the command to build the firmware based on a JSON-configuration.
"""
import os
import click

@click.command()
@click.argument('config', type=click.File('r'))
@click.option('-o', '--output-directory')
@click.option('--build', is_flag=True)
@click.option('-a', '--alias-file', is_flag=True)
def cli(config, output_directory, build, alias_file):
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
        output_directory = os.path.dirname(config.name)
 
    # Add the config name as folder
    output_directory = os.path.join(
        output_directory,
        os.path.splitext(os.path.basename(config.name))[0]
    )
   
    # Create folder if not exists
    if not os.path.exists(output_directory):
        os.makedirs(output_directory)

    # Load configuration
    firmware_config = LitexCNC_Firmware.parse_raw(' '.join(config.readlines()))

    # Output the aliases
    if alias_file:
        # Create a collection with aliases
        alias = []
        for module in firmware_config.modules:
            alias.extend(module.create_aliases(firmware_config.board_name))
        # Write the alias to the output file
        with open(os.path.join(output_directory, 'alias.hal'), 'wt') as alias_ouput:
            print('\n'.join(alias), file=alias_ouput)

    # Generate the firmware
    soc = firmware_config.generate()
    
    # Build the SOC
    builder = Builder(
        soc, 
        output_dir=output_directory,
        csr_csv=os.path.join(output_directory, "csr.csv")
    )
    builder.build(run=build)

    # Done!
    click.echo(click.style("INFO", fg="blue") + f": Firmware created in {output_directory}")
    if alias_file:
        click.echo(click.style("INFO", fg="blue") + f": Alias HAL-file created in {os.path.join(output_directory, 'alias.hal')}")
    