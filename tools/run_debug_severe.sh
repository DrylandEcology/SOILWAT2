#!/bin/bash

# run as `./tools/run_debug_severe.sh`
# note: consider cleaning previous build artifacts, e.g., `make clean_build`

debug_flags="-g -O0 -DSWDEBUG"

warning_flags_severe_cc="\
-Wall -Wextra \
-Wpedantic \
-Werror \
-Wcast-align \
-Wmissing-declarations \
-Wredundant-decls \
-Wstrict-prototypes"
# '-Wstrict-prototypes' is valid for C/ObjC but not for C++

instr_flags_severe="\
-fstack-protector-all \
-fsanitize=undefined \
-fsanitize=address \
-fno-omit-frame-pointer \
-fno-common"
# -fstack-protector-strong (gcc >= v4.9)
# (gcc >= 4.0) -D_FORTIFY_SOURCE: lightweight buffer overflow protection to some memory and string functions
# (gcc >= 4.8; llvm >= 3.1) -fsanitize=address: AdressSanitizer: replaces `mudflap` run time checker; https://github.com/google/sanitizers/wiki/AddressSanitizer
#   -fno-omit-frame-pointer: allows fast unwinder to work properly for ASan
#   -fno-common: allows ASan to instrument global variables
# (gcc >= 4.9; llvm >= 3.3) -fsanitize=undefined: UndefinedBehaviorSanitizer



SW2_FLAGS=""$debug_flags" "$warning_flags_severe_cc" "$instr_flags_severe"" make bin_run
