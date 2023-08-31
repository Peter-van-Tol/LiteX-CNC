from pathlib import Path

# Get all .c-files and .h-files. For finer granularity one can also draft this
# list by hand, but this is not recommended.
TYPES = ('**/*.c', '**/*.h') # the tuple of file types
FILES = []
EXCLUDE = [
    'config.h',
    'litexcnc_debug.c',
    'litexcnc_debug.h'
]
for type_ in TYPES:
    for file in Path(__file__).parent.glob(type_):
        if not file.name in EXCLUDE:
            FILES.append(file)
