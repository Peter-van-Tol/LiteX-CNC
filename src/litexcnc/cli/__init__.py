"""
This module contains all the commands provided to the user. These commands are
amongst others used by the graphical interface to configure the cards, but can
also be used directly from the command line. 

This file contains the multi-command which contains all the commands published
in this folder.
"""
import os
import click

class LitexCncCLI(click.MultiCommand):
    """Click multi-command for LitexCNC which searches all ``.py``-files in the
    same directory for commands. Additional commands should be placed in this 
    directory and contain the following code in order to be picked up by this
    class:

    .. code-block:: python

        import click
        from . import LitexCncCLI

        @click.command(cls=LitexCncCLI)
        def cli():
            pass
    
    For more information, see the `Click documentation <https://click.palletsprojects.com/en/8.1.x/commands/>`_ 

    """
    # This is the folder this __init__.py file resides in. This is the same folder
    # in which all commands are searched for.
    PLUGIN_FOLDER = os.path.dirname(__file__)

    def list_commands(self, ctx):
        """Lists all commands in this directory. Excludes all files starting
        with __ (double underscore or dunder)"""
        commands = []
        for filename in os.listdir(self.PLUGIN_FOLDER):
            if filename.endswith('.py') and not filename.startswith('__'):
                commands.append(filename[:-3])
        commands.sort()
        return commands

    def get_command(self, ctx, name):
        ns = {}
        fn = os.path.join(self.PLUGIN_FOLDER, name + '.py')
        try:
            with open(fn) as f:
                code = compile(f.read(), fn, 'exec')
                eval(code, ns, ns.update({"__file__": __file__}))
        except FileNotFoundError:
            # Return nothing, click will display a nice message for us
            return
        return ns['cli']

cli = LitexCncCLI(help='Command-line tools for LitxCNC.')

__all__ = [
    'cli'
]
