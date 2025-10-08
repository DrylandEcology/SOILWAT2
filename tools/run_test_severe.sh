#!/bin/bash

# run as `./tools/run_test_severe.sh`

# note: consider cleaning previous build artifacts, e.g., `make clean_test`
# note: regular unit tests are run with `make test_run`


#--- Import functions
myDir=$(dirname ${BASH_SOURCE[0]}) # directory of this script

source "${myDir}/compile_flags.sh"


#--- flags
flags0=$(debug_flags "O1")
flags1=$(compile_flags "CXX" "sanitizer-yes")


#--- make
SW2_FLAGS=""$flags0" "$flags1"" make test_run
