#!/bin/bash

# run as `./tools/run_test_sanitizer.sh`

# note: consider cleaning previous build artifacts, e.g., `make clean_test`

#--- Import functions
myDir=$(dirname ${BASH_SOURCE[0]}) # directory of this script

source "${myDir}/compile_flags.sh"


#--- flags
flags0=$(debug_flags "O1")
flags1=$(compile_flags "CXX" "sanitizer-yes")


# Note: # Apple clang does not support "AddressSanitizer: detect_leaks" (at least as of clang-1200.0.32.29)
ASAN_OPTIONS=detect_leaks=1:strict_string_checks=1:detect_stack_use_after_return=1:check_initialization_order=1:strict_init_order=1 LSAN_OPTIONS=suppressions=../.LSAN_suppr.txt SW2_FLAGS=""$flags0" "$flags1"" make test_run
