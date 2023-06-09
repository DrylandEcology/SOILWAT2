#!/bin/bash

# run as `./tools/run_debug.sh`
# note: consider cleaning previous build artifacts, e.g., `make clean_build`

debug_flags="-g -O0 -DSWDEBUG"
instr_flags="-fstack-protector-all"

SW2_FLAGS=""$debug_flags" "$instr_flags"" make bin_run
