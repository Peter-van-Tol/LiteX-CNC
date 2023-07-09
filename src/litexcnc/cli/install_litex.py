"""
This file contains the command to download and install Litex
"""
import os
from pathlib import Path
import subprocess
import sys
import click
import requests

@click.command()
@click.option('--user', is_flag=True)
@click.option('--directory', '-d')
def cli(user, directory):
    """Installs Litex from https://github.com/enjoy-digital/litex in HOME"""

    # Create the directory
    target = directory
    if user:
        target = str(Path.home())
    target = os.path.join(target, 'litex')
    
    if not os.path.exists(target):
        os.mkdir(target)

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
    if ret == 0:
        # Success
        click.echo(click.style("INFO", fg="blue") + f": Successfully installed Litex in '{target}'")
    else: 
        click.echo(click.style("ERROR", fg="red") + ": An error has occurred during installation of Litex.")
        click.echo("Check the error message from the process. Possibly rerun the command with elevated permissions if the error is 'Permission denied'")    
