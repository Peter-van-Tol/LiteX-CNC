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

import sys
if sys.version_info[:2] >= (3, 8):
    # TODO: Import directly (no need for conditional) when `python_requires = >= 3.8`
    from importlib.metadata import entry_points  # pragma: no cover
else:
    from importlib_metadata import entry_points  # pragma: no cover

# Import the driver module.
from litexcnc.driver import halcompile


@click.command()
@click.option('--modules', '-m', multiple=True)
def cli(modules):

    with tempfile.TemporaryDirectory() as temp_dir:
        
        driver_files_group = "litexcnc.driver_files"
        entries = entry_points()
        driver_files = {}
        if hasattr(entries, "select"):
            # The select method was introduced in importlib_metadata 3.9 (and Python 3.10)
            # and the previous dict interface was declared deprecated
            driver_folders = entries.select(group=driver_files_group)  # type: ignore
        else:
            # TODO: Once Python 3.10 becomes the oldest version supported, this fallback and
            #       conditional statement can be removed.
            driver_folders = (extension for extension in entries.get(driver_files_group, []))  # type: ignore
        for driver_folder in driver_folders:
            # Add the files to the list
            driver_files[driver_folder.name] = driver_folder.load().FILES

        # List with files for the compile-script
        files_to_compiles = []
        
        # Copy the files to the temp directory
        shutil.copy2(
            halcompile.__file__,
            os.path.join(temp_dir, os.path.basename(halcompile.__file__))
        )
        click.echo(click.style("INFO", fg="blue") + ": Retrieving default driver files to compile...")
        if not modules or 'default' in modules:
            files_to_compiles.append('litexcnc.c')
            files_to_compiles.append('pos2vel.c')
            for file in driver_files.pop('default'):
                click.echo(f"Copying file '{file.name}'")
                shutil.copy2(
                    os.path.join(file),
                    os.path.join(temp_dir, os.path.basename(file.name))
                )
                if re.search("litexcnc_.*\.c", file.name):
                    files_to_compiles.append(file.name)
        if driver_files.keys():
            click.echo(click.style("INFO", fg="blue") + ": Retrieving extra modules / boards to compile...")
            for extra in driver_files.keys():
                if not modules or extra in modules:
                    for file in driver_files[extra]:
                        click.echo(f"Copying file '{file.name}'")
                        shutil.copy2(
                            os.path.join(file),
                            os.path.join(temp_dir, os.path.basename(file.name))
                        )
                        if re.search("litexcnc_.*\.c", file.name):
                            files_to_compiles.append(file.name)

        # Compile the driver
        click.echo(click.style("INFO", fg="blue") + ": Compiling LitexCNC driver...")
        ret = subprocess.call(
            f'{sys.executable} halcompile.py --install {" ".join(files_to_compiles)}',
            cwd=temp_dir,
            shell=True
        )
        if ret:
            click.echo(click.style("Error", fg="red") + ": Compilation of the driver failed.")
            return
    # Done!
    click.echo(click.style("INFO", fg="blue") + ": LitexCNC driver installed")
