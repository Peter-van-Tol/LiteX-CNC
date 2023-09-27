
import os
import sys
import platform
import subprocess
import tarfile
import tempfile
from pathlib import Path
import click
import requests

@click.command()
@click.option('--user', is_flag=True)
@click.option(
    '-a', '--architecture', 'arch',
    type=click.Choice(['arm', 'arm64', 'x64'], case_sensitive=False),
    help="Use specific architecture. Leave empty for auto-detect")
@click.option(
    '-os', '--os', 'os_',
    type=click.Choice(['darwin', 'linux', 'windows'], case_sensitive=False),
    help="Use specific os. Leave empty for auto-detect")
def cli(user, arch, os_):
    """Installs the Toolchain for ECP5 (oss-cad-suite)"""
    if not os_:
        os_ = platform.system().lower()
        click.echo(click.style("INFO", fg="blue") + f": Auto-detected os {os_}.")
    if not arch:
        if platform.machine().startswith('arm'):
            arch = 'arm64' if sys.maxsize > 2**32 else 'arm'
        else:
            arch = 'i386' if sys.maxsize > 2**32 else 'x64'
        click.echo(click.style("INFO", fg="blue") + f": Auto-detected architecture {arch}.")
    # Check whether the combination if supported by oss-cad-suite
    if os_ == 'darwin':
        if not arch.lower() in ['arm64', 'x64']:
            click.echo(click.style("ERROR", fg="red") + f": Darwin only supports arm64 and x64 as architecture.")
    if os_ == 'windows':
        if not arch.lower() in ['x64']:
            click.echo(click.style("ERROR", fg="red") + f": Windows only supports x64 as architecture.")
    # Download and install the selected files
    with tempfile.TemporaryDirectory() as tempdirname:
        # Download the toolchain from the source
        click.echo(click.style("INFO", fg="blue") + f": Downloading OSS-CAD-Suite ...")
        download = os.path.join(tempdirname, f'oss-cad-suite-{os_}-{arch}-20220227.tgz')
        response = requests.get(
            f'https://github.com/YosysHQ/oss-cad-suite-build/releases/download/2022-12-07/oss-cad-suite-{os_}-{arch}-20221207.tgz'
        )
        response.raise_for_status()
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
        os.environ["PATH"] += f':{target}/oss-cad-suite/bin'

        # Done!
        click.echo(click.style("INFO", fg="blue") + f": OSS-CAD-Suite successfully installed")
