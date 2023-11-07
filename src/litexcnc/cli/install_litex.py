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
@click.option('--user', is_flag=True, help="Install Litex in your home directory")
@click.option('--directory', '-d', help="Install Litex in specific location")
def cli(user, directory):
    """Installs Litex from https://github.com/enjoy-digital/litex in HOME [DEPRECATED: use `install_toolchain` instead]"""
    click.echo(click.style("INFO", fg="blue") + f": This command is deprecated, use `install_toolchain` instead.")
