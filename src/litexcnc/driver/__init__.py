from pathlib import Path

# Get all .c-files and .h-files. For finer granularity one can also draft this
# list by hand, but this is not recommended.
TYPES = ('**/*.c', '**/*.h') # the tuple of file types
FILES = []
for type_ in TYPES:
    for file in Path(__file__).parent.glob(type_):
        if not file.name == 'config.h':
            FILES.append(file)
