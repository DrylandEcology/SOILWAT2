#!/bin/bash

# run as `./tools/run_bin_leaks.sh`
# note: consider cleaning previous build artifacts, e.g., `make clean_build`

# or via makefile
# run as `CC=clang make clean bin_leaks`

MallocStackLogging=1 MallocStackLoggingNoCompact=1 MallocScribble=1 MallocPreScribble=1 MallocCheckHeapStart=0 MallocCheckHeapEach=0 leaks -quiet -atExit -- bin/SOILWAT2 -d ./tests/example -f files.in
