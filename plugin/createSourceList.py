import os

# Directory to search for source files
source_dir = os.path.join(os.path.dirname(__file__), 'source')

# Extensions to include
extensions = ('.cpp', '.h')

def list_files(directory):
    files = []
    for root, _, filenames in os.walk(directory):
        for filename in filenames:
            if filename.endswith(extensions):
                files.append(os.path.relpath(os.path.join(root, filename), os.path.dirname(__file__)).replace('\\', '/'))
    return files

source_files = list_files(source_dir)

with open(os.path.join(os.path.dirname(__file__), 'sourceListResult.cmake'), 'w') as f:
    f.write('set(PLUGIN_SOURCES\n')
    for file in source_files:
        f.write(f'    {file}\n')
    f.write(')\n')
