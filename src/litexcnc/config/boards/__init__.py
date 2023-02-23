import sys
if sys.version_info[:2] >= (3, 8):
    # TODO: Import directly (no need for conditional) when `python_requires = >= 3.8`
    from importlib.metadata import entry_points  # pragma: no cover
else:
    from importlib_metadata import entry_points  # pragma: no cover


GROUP = "litexcnc.boards"

entries = entry_points()
if hasattr(entries, "select"):
    # The select method was introduced in importlib_metadata 3.9 (and Python 3.10)
    # and the previous dict interface was declared deprecated
    modules = entries.select(group=GROUP)  # type: ignore
else:
    # TODO: Once Python 3.10 becomes the oldest version supported, this fallback and
    #       conditional statement can be removed.
    modules = (extension for extension in entries.get(GROUP, []))  # type: ignore
for module in modules:
    module.load()
