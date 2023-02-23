"""
This file contains the command to compile the driver

"""
import glob
import os
import re
import shutil
import subprocess
import sys
import tempfile
import click

# Import the driver module.
from litexcnc import driver
from litexcnc.config.modules import ModuleBaseModel
from litexcnc.firmware.soc import LitexCNC_Firmware
from litexcnc import driver
from litexcnc.driver.halcompile import find_modinc


@click.command()
def cli():

    with tempfile.TemporaryDirectory() as temp_dir:
        # Copy the files to the temp directory
        # - main driver
        click.echo(click.style("INFO", fg="blue") + ": Retrieving main driver to compile...")
        driver_folder = os.path.dirname(driver.__file__)
        with os.scandir(driver_folder) as entries:
            for entry in entries:
                if os.path.isfile(entry) and not entry == '__init__.py':
                    click.echo(f"Copying file '{os.path.basename(entry)}'")
                    shutil.copy2(
                        os.path.join(driver_folder, entry),
                        os.path.join(temp_dir, os.path.basename(entry))
                    )
        with os.scandir(os.path.join(driver_folder, 'stepgen')) as entries:
            os.mkdir(os.path.join(temp_dir, 'stepgen'))
            for entry in entries:
                if os.path.isfile(entry) and not entry == '__init__.py':
                    click.echo(f"Copying file '{os.path.basename(entry)}'")
                    shutil.copy2(
                        os.path.join(driver_folder, 'stepgen', entry), 
                        os.path.join(temp_dir, 'stepgen', os.path.basename(entry))
                    )
        # - modules
        click.echo(click.style("INFO", fg="blue") + ": Retrieving modules to compile...")
        modules = []
        for module in ModuleBaseModel.__subclasses__():
            for file in module.driver_files:
                click.echo(f"Copying file '{os.path.basename(file)}'")
                shutil.copy2(file, os.path.join(temp_dir, os.path.basename(file)))
                if re.search("litexcnc_.*\.c", os.path.basename(file)):
                    modules.append(os.path.basename(file))
        # - board drivers
        click.echo(click.style("INFO", fg="blue") + ": Retrieving board drivers to compile...")
        boards = []
        for board in LitexCNC_Firmware.__subclasses__():
            for file in board.driver_files:
                click.echo(f"Copying file '{os.path.basename(file)}'")
                shutil.copy2(file, os.path.join(temp_dir, os.path.basename(file)))
                if re.search("litexcnc_.*\.c", os.path.basename(file)):
                    modules.append(os.path.basename(file))
        # Compile the driver
        click.echo(click.style("INFO", fg="blue") + ": Compiling LitexCNC driver...")
        ret = subprocess.call(
            f'{sys.executable} halcompile.py --install litexcnc.c stepgen/pos2vel.c {" ".join(modules)}  {" ".join(boards)}',
            cwd=temp_dir,
            shell=True
        )
        if ret:
            click.echo(click.style("Error", fg="red") + ": Compilation of the driver failed.")
            return
    # Done!
    click.echo(click.style("INFO", fg="blue") + ": LitexCNC driver installed")
