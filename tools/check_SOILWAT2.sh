#!/bin/bash

# Runs several tests for SOILWAT2
# - is currently set up for use with macports
# - expects clang as default compiler

# Problems:
# - unit test compiled with gcc are getting stuck or throw a "stack-overflow"
#   when running with sanitizer "ASAN_OPTIONS=detect_leaks=1",
#   but binaries work correctly


# Notes:
# - googletests (Jan 2022) requires a C++11 compliant compilers,
#   gcc >= 5.0 or clang >= 5.0,
#   and POSIX API (e.g., `_POSIX_C_SOURCE=200809L`)
#   which is not enabled by default on all systems



#--- List of (builtin and macport) compilers

declare -a port_compilers=(
  "none"
  "mp-gcc49" "mp-gcc5" "mp-gcc7" "mp-gcc9" "mp-gcc10" "mp-gcc11"
  "mp-clang-3.3" "mp-clang-5.0" "mp-clang-9.0" "mp-clang-10" "mp-clang-11" "mp-clang-12" "mp-clang-13"
)
declare -a ccs=(
  "clang"
  "gcc" "gcc" "gcc" "gcc" "gcc" "gcc"
  "clang" "clang" "clang" "clang" "clang" "clang" "clang"
)
declare -a cxxs=(
  "clang++"
  "g++" "g++" "g++" "g++" "g++" "g++"
  "clang++" "clang++" "clang++" "clang++" "clang++" "clang++" "clang"
)


ncomp=${#ccs[@]}


#--- Function to check testing output
check_testing_output () {
  if diff  -q -x "\.DS_Store" -x "\.gitignore" testing/Output/ testing/Output_ref/ >/dev/null 2>&1; then
    echo $'\n'"Example output is reproduced by binary!"$'\n'$'\n'
  else
    echo $'\n'"Error: Example output is not reproduced by binary!"
    diff  -qs -x "\.DS_Store" -x "\.gitignore" testing/Output/ testing/Output_ref/
    echo $'\n'$'\n'
  fi
}


#--- Loop through tests and compilers
for ((k = 0; k < ncomp; k++)); do
  echo $'\n'$'\n'$'\n'\
       ==================================================$'\n'\
       Test SOILWAT2 with compiler \'${port_compilers[k]}\'$'\n'\
       ==================================================

  echo $'\n'Set compiler ...
  if [ "${port_compilers[k]}" != "none" ]; then
    res=$(sudo port select --set ${ccs[k]} ${port_compilers[k]})

    if ( echo ${res} | grep "failed" ); then
      continue
    fi
  fi

  echo $'\n'`"${ccs[k]}" --version`
  echo $'\n'`"${cxxs[k]}" --version`
  echo $'\n'\
       -----------------------------------------------------

  CC=${ccs[k]} CXX=${cxxs[k]} make compiler_version


  echo $'\n'$'\n'\
       --------------------------------------------------$'\n'\
       Run binary with ${port_compilers[k]} ...$'\n'\
       --------------------------------------------------

  CC=${ccs[k]} CXX=${cxxs[k]} make clean bin bint_run

  if [ ${k} -eq 0 ]; then
    # Save default testing output as reference for future comparisons
    cp -r testing/Output testing/Output_ref
  fi
  check_testing_output

  CC=${ccs[k]} CXX=${cxxs[k]} make clean bin_debug_severe bint_run
  check_testing_output

  if command -v valgrind >/dev/null 2>&1; then
    CC=${ccs[k]} CXX=${cxxs[k]} make clean bind_valgrind
  fi


  echo $'\n'$'\n'\
       --------------------------------------------------$'\n'\
       Run tests with ${port_compilers[k]} ...$'\n'\
       --------------------------------------------------

  CC=${ccs[k]} CXX=${cxxs[k]} make clean test test_run
  CC=${ccs[k]} CXX=${cxxs[k]} make clean test_severe test_run


  echo $'\n'$'\n'\
       --------------------------------------------------$'\n'\
       Run sanitizer on tests with ${port_compilers[k]}: exclude known leaks ...$'\n'\
       --------------------------------------------------

  # CC=${ccs[k]} CXX=${cxxs[k]} ASAN_OPTIONS=detect_leaks=1 LSAN_OPTIONS=suppressions=.LSAN_suppr.txt make clean test_severe test_run
  # https://github.com/google/sanitizers/wiki/AddressSanitizer
  CC=${ccs[k]} CXX=${cxxs[k]} ASAN_OPTIONS=detect_leaks=1:strict_string_checks=1:detect_stack_use_after_return=1:check_initialization_order=1:strict_init_order=1 LSAN_OPTIONS=suppressions=.LSAN_suppr.txt make clean test_severe test_run

  echo $'\n'$'\n'\
       --------------------------------------------------$'\n'\
       Unset compiler ...$'\n'\
       --------------------------------------------------

  if [ "${port_compilers[k]}" != "none" ]; then
    sudo port select --set ${ccs[k]} none
  fi

done

echo $'\n'$'\n'\
     --------------------------------------------------$'\n'\
     Complete!$'\n'\
     --------------------------------------------------$'\n'


#--- Cleanup
unset -v ccs
unset -v cxxs
unset -v port_compilers
