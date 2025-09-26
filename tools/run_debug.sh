#!/bin/bash

# run as `./tools/run_debug.sh` [OPTIONS]
# supported options
#   -n,-np,--ntasks=<number>

# note: consider cleaning previous build artifacts, e.g., `make clean_build`

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
debug_flags="-g -O0 -DSWDEBUG"
instr_flags="-fstack-protector-all"

SW2_FLAGS=""$debug_flags" "$instr_flags"" SW_NTASKS="${nTasks}" make bin_run
