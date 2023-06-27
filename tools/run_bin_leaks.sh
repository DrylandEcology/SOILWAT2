#!/bin/bash

# run as `./tools/run_bin_leaks.sh`
# note: consider cleaning previous build artifacts, e.g., `make clean_build`

# or via makefile
# run as `CC=clang make clean bin_leaks`
# if runtime error "Library not loaded", then
# try to run, for example, with `DYLD_INSERT_LIBRARIES="path/to/libclang_rt.asan_osx_dynamic.dylib" bin/SOILWAT2 -d ./tests/example -f files.in`

debug_flags="-g -O0 -DSWDEBUG"

warning_flags_severe_cc="\
-Wall -Wextra \
-Wpedantic \
-Werror \
-Wcast-align \
-Wmissing-declarations \
-Wredundant-decls \
-Wno-error=strict-prototypes"


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


# Note: # Apple clang does not support "AddressSanitizer: detect_leaks" (at least as of clang-1200.0.32.29)
ASAN_OPTIONS=detect_leaks=1:strict_string_checks=1:detect_stack_use_after_return=1:check_initialization_order=1:strict_init_order=1 LSAN_OPTIONS=suppressions=../.LSAN_suppr.txt SW2_FLAGS=""$debug_flags" "$warning_flags_severe_cc" "$instr_flags_severe"" make bin_run
