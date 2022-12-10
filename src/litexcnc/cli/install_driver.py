"""
This file contains the command to compile the driver

"""
import os
import subprocess
import sys
import click

# Import the driver module.
from litexcnc import driver

@click.command()
def cli():
    
    # apt-get update and install libjson-c-dev
    click.echo(click.style("INFO", fg="blue") + ": Installing libjson-c-dev...")
    ret = subprocess.call(
        'apt-get update && apt-get install -y libjson-c-dev', 
        shell=True
    )
    if ret:
        click.echo(click.style("Error", fg="red") + ": Installation of libjson-c-dev failed.")

    # compile the driver
    click.echo(click.style("INFO", fg="blue") + ": Compiling LitexCNC driver...")
    ret = subprocess.call(
        f'{sys.executable} halcompile.py --install litexcnc.c litexcnc_eth.c litexcnc_debug.c',
        cwd=os.path.dirname(os.path.abspath(driver.__file__)),
        shell=True
    )
    if ret:
        click.echo(click.style("Error", fg="red") + ": Compilation of the driver failed.")
        return

    # Done!
    click.echo(click.style("INFO", fg="blue") + ": LitexCNC driver installed")

    

