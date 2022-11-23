
import os
import subprocess
import tarfile
import tempfile
from pathlib import Path
import click
import requests

@click.command()
@click.option('--user', is_flag=True)
def cli(user):
    """Installs the Toolchain for ECP5 (oss-cad-suite)"""

    with tempfile.TemporaryDirectory() as tempdirname:
        # Download the toolchain from the source
        click.echo(click.style("INFO", fg="blue") + f": Downloading OSS-CAD-Suite ...")
        download = os.path.join(tempdirname, 'oss-cad-suite-linux-x64-20220227.tgz')
        response = requests.get(
            'https://github.com/YosysHQ/oss-cad-suite-build/releases/download/2022-02-27/oss-cad-suite-linux-x64-20220227.tgz'
        )
        open(download, 'wb').write(response.content)

        # Determine the folder to unpack the tar-ball to
        click.echo(click.style("INFO", fg="blue") + f": Unpacking ...")
        target = '/opt'
        if user:
            target = str(Path.home())
        
        # Extract the contents
        with tarfile.open(download) as archive:
            archive.extractall(target)

        # Determine which file to write the settings to
        click.echo(click.style("INFO", fg="blue") + f": Writing config ...")
        config_file = '/etc/profile'
        if user:
            config_file = f'{str(Path.home())}/.profile'
        # Determine whether it is a new file or not
        append_write = 'w'
        if os.path.exists(config_file):
            append_write = 'a'
        # Write to the file
        with open(config_file, append_write) as file_out:
            file_out.write('# set PATH so it includes oss-cad-suite \n')
            file_out.write(f'if [ -d "{target}/oss-cad-suite/bin" ] ; then \n')
            file_out.write(f'PATH="{target}/oss-cad-suite/bin:$PATH" \n')
            file_out.write('fi \n')

        # Source the file created to put OSS CAD Suite directly on path
        click.echo(click.style("INFO", fg="blue") + f": Sourcing environment variables ...")
        os.environ["PATH"] += f'{target}/oss-cad-suite/bin'

        # Done!
        click.echo(click.style("INFO", fg="blue") + f": OSS-CAD-Suite successfully installed")
