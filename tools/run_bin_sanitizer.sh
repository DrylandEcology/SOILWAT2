#!/bin/bash

# run as `./tools/run_bin_sanitizer.sh`
# note: consider cleaning previous build artifacts, e.g., `make clean_build`

# or via makefile
# run as `CC=clang make clean bin_sanitizer` [OPTIONS]
# supported options
#   -n,-np,--ntasks=<number>

# if runtime error "Library not loaded", then
# try to run, for example, with `DYLD_INSERT_LIBRARIES="path/to/libclang_rt.asan_osx_dynamic.dylib" bin/SOILWAT2 -d ./tests/example -f files.in`

#--- Import functions
myDir=$(dirname ${BASH_SOURCE[0]}) # directory of this script

source "${myDir}/compile_flags.sh"


#--- Command line arguments
nTasks=""

while [ $# -gt 0 ]; do
    case "$1" in
        --ntasks=*) nTasks="${1#*=}" ;;
        -n|-np) nTasks="$2"; shift ;;

        *) echo "Argument ""$1"" is not implemented."; exit 1 ;;
    esac
    shift
done


#--- flags
flags0=$(debug_flags "O1")
flags1=$(compile_flags "CC" "sanitizer-yes")


#--- make
# Note: # Apple clang does not support "AddressSanitizer: detect_leaks" (at least as of clang-1200.0.32.29)
ASAN_OPTIONS=detect_leaks=1:strict_string_checks=1:detect_stack_use_after_return=1:check_initialization_order=1:strict_init_order=1 LSAN_OPTIONS=suppressions=../.LSAN_suppr.txt SW2_FLAGS=""$flags0" "$flags1"" SW_NTASKS="${nTasks}" make bin_run
