
import os
import sys
import platform
import subprocess
import tarfile
import tempfile
from pathlib import Path
import click
import requests


def _install_litex(target: str, user: bool) -> int:
    """Installs Litex from https://github.com/enjoy-digital/litex"""
    target = os.path.join(target, 'litex')
    
    if not os.path.exists(target):
        os.makedirs(target)

    # Download the setup file
    response = requests.get(
        'https://raw.githubusercontent.com/enjoy-digital/litex/master/litex_setup.py'
    )
    open(os.path.join(target, 'litex_setup.py'), 'wb').write(response.content)

    # Run the python file
    command = "litex_setup.py --init --install --config=standard --tag=2022.04"
    if user:
        command += " --user"
    command = f'{sys.executable} {os.path.join(target, command)}'  #  --gcc=riscv if SOC with cpu
    click.echo(click.style("INFO", fg="blue") + f": Installing Litex with the command '{command}'.")
    ret = subprocess.call(
        command, 
        cwd=target,
        shell=True
    )

    # Inform the user whether install is succes:
    if ret != 0:
        click.echo(click.style("ERROR", fg="red") + ": An error has occurred during installation of Litex.")
        click.echo("Check the error message from the process. Possibly rerun the command with elevated permissions if the error is 'Permission denied'")    
        return ret
    
    # Success
    click.echo(click.style("INFO", fg="blue") + f": Successfully installed Litex in '{target}'")
    return 0


def _install_oss_cad_suite(target: str, user: bool, arch: str = None, os_:str = None):
    """Installs the Toolchain for ECP5 (oss-cad-suite)"""
    # Determine the OS and architecture
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
    supported = True
    if arch == 'i386':
        click.echo(click.style("ERROR", fg="red") + f": OSS-Cad-Suite does not support `i386` architecture.")
        supported = False
    if os_ == 'darwin':
        if not arch.lower() in ['arm64', 'x64']:
            click.echo(click.style("ERROR", fg="red") + f": Darwin only supports arm64 and x64 as architecture.")
            supported = False
    if os_ == 'windows':
        if not arch.lower() in ['x64']:
            click.echo(click.style("ERROR", fg="red") + f": Windows only supports x64 as architecture.")
            supported = False
    if not supported:
        click.echo(click.style("ERROR", fg="red") + f": Please use a different system to build the firmware.")
        return
    
    # Download and install the selected files
    with tempfile.TemporaryDirectory() as tempdirname:
        # Download the toolchain from the source
        click.echo(click.style("INFO", fg="blue") + f": Downloading OSS-CAD-Suite ({os_}/{arch}) ...")
        download = os.path.join(tempdirname, f'oss-cad-suite-{os_}-{arch}-20220227.tgz')
        response = requests.get(
            f'https://github.com/YosysHQ/oss-cad-suite-build/releases/download/2022-12-07/oss-cad-suite-{os_}-{arch}-20221207.tgz'
        )
        response.raise_for_status()
        open(download, 'wb').write(response.content)

        # Determine the folder to unpack the tar-ball to
        click.echo(click.style("INFO", fg="blue") + f": Unpacking ...")
        
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


def _install_openocd(target: str, user: bool):
    """Installs the OpenOCD for Raspberry Pi"""
    click.echo(click.style("INFO", fg="blue") + f": Installing OpenOCD for Raspberry Pi.")

    # Removing installed version of OpenOCD from oss-cad-suite
    if os.path.exists(f"{target}/oss-cad-suite/bin/openocd"):
        os.remove(f"{target}/oss-cad-suite/bin/openocd")

    # Install pre-requisites
    click.echo(click.style("INFO", fg="blue") + f": Installing pre-requisites ...")
    if subprocess.call(
            "sudo apt-get update",
            shell=True):
        click.echo(click.style("ERROR", fg="red") + ": Cannot update system.")
        return -1
    if subprocess.call(
            "sudo apt-get install git autoconf libtool make pkg-config libusb-1.0-0 libusb-1.0-0-dev",
            shell=True):
        click.echo(click.style("ERROR", fg="red") + ": Cannot install pre-requisites.")
        return -1

    # Download and install the selected files
    with tempfile.TemporaryDirectory() as tempdirname:
        # Cloning OpenOCD
        click.echo(click.style("INFO", fg="blue") + f": Cloning OpenOCD ...")
        if subprocess.call(
                "git clone http://openocd.zylin.com/openocd", 
                cwd=tempdirname,
                shell=True
            ):
            click.echo(click.style("ERROR", fg="red") + ": Could not clone OpenOCD.")
            return -1

        # Configure OpenOCD to include support for Raspberry Pi
        click.echo(click.style("INFO", fg="blue") + f": Configuring and making OpenOCD (this make take a while) ...")
        command = f"./bootstrap && "
        command += "./configure --enable-sysfsgpio --enable-bcm2835gpio && "
        command += "make"
        if subprocess.call(
                command, 
                cwd=os.path.join(tempdirname, "openocd"),
                shell=True
            ):
            click.echo(click.style("ERROR", fg="red") + ": Could not make OpenOCD.")
            return -1

        # Installing OpenOCD
        command = "sudo make install"
        if subprocess.call(
                "sudo make install", 
                cwd=os.path.join(tempdirname, "openocd"),
                shell=True
            ):
            click.echo(click.style("ERROR", fg="red") + ": Failed to install OpenOCD.")
            return -1


@click.command()
@click.option('--user', is_flag=True)
@click.option('--directory', '-d', help="Install the toolchain in specific location")
@click.option(
    '-a', '--architecture', 'arch',
    type=click.Choice(['arm', 'arm64', 'x64'], case_sensitive=False),
    help="Use specific architecture. Leave empty for auto-detect")
@click.option(
    '-os', '--os', 'os_',
    type=click.Choice(['darwin', 'linux', 'windows'], case_sensitive=False),
    help="Use specific os. Leave empty for auto-detect")
def cli(user, directory, arch, os_):
    """Installs the Toolchain for ECP5 (oss-cad-suite)"""
    target = directory if directory else '/opt'
    if user and not directory:
        target = str(Path.home() / "toolchain")
    
    # # Install the components of the toolchain
    _install_litex(target, user)
    _install_oss_cad_suite(target, user, arch, os_)

    # When this instance is on a Raspberry Pi, install a custom version
    # of OpenOCD, which supports programming by the GPIO pins
    if os.name == 'posix' and os.path.exists("/sys/firmware/devicetree/base/model"):
        with open('/sys/firmware/devicetree/base/model', 'r') as m:
            if 'raspberry pi' in m.read().lower():
                _install_openocd(target, user)
