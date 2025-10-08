#!/bin/bash

# run as `./tools/run_debug.sh` [OPTIONS]
# supported options
#   -n,-np,--ntasks=<number>

# note: consider cleaning previous build artifacts, e.g., `make clean_build`


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
flags0=$(debug_flags "O0")
flags1=$(compile_flags "CC" "sanitizer-no")


#--- make
SW2_FLAGS=""$flags0" "$flags1"" SW_NTASKS="${nTasks}" make bin_run
