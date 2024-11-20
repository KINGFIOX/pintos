import os

c_extensions = (".c", ".h")

directories = ["src/devices/", "src/examples/", "src/kernel/", "src/filesys/"]

for directory in directories:
    for root, dirs, files in os.walk(directory):
        for file in files:
            if file.endswith(c_extensions):
                os.system("clang-format -i -style=file " + root + "/" + file)