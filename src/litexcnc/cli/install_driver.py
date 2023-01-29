"""
This file contains the command to compile the driver

"""
import glob
import os
import subprocess
import sys
import click

# Import the driver module.
from litexcnc import driver
from litexcnc.driver.halcompile import find_modinc

@click.command()
def cli():

    # Find modules
    click.echo(click.style("INFO", fg="blue") + ": Retrieving modules to compile...")
    modules = []
    # - default modules
    for module in glob.glob(f"{os.path.dirname(os.path.abspath(driver.__file__))}/modules/litexcnc_*.c"):
        module = module[len(f"{os.path.dirname(os.path.abspath(driver.__file__))}/"):]
        click.echo(f"... found '{module}'")
        modules.append(module)
    # - external modules(TODO)
    ...

    # Find modules
    click.echo(click.style("INFO", fg="blue") + ": Retrieving board drivers to compile...")
    boards = []
    # - default modules
    for board in glob.glob(f"{os.path.dirname(os.path.abspath(driver.__file__))}/boards/litexcnc_*.c"):
        board = board[len(f"{os.path.dirname(os.path.abspath(driver.__file__))}/"):]
        click.echo(f"... found '{board}'")
        boards.append(board)
    # - external modules(TODO)
    
    print(f'{sys.executable} halcompile.py --install litexcnc.c stepgen/pos2vel.c {" ".join(modules)}  {" ".join(boards)}',)


    # Compile the driver
    click.echo(click.style("INFO", fg="blue") + ": Compiling LitexCNC driver...")
    ret = subprocess.call(
        f'{sys.executable} halcompile.py --install litexcnc.c stepgen/pos2vel.c {" ".join(modules)}  {" ".join(boards)}',
        cwd=os.path.dirname(os.path.abspath(driver.__file__)),
        shell=True
    )
    if ret:
        click.echo(click.style("Error", fg="red") + ": Compilation of the driver failed.")
        return

    # Done!
    click.echo(click.style("INFO", fg="blue") + ": LitexCNC driver installed")
