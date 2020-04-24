#!/bin/bash

# Runs several tests for SOILWAT2
# - is currently set up for use with macports
# - expects clang as default compiler


#--- List of (builtin and macport) compilers
declare -a port_compilers=("none" "mp-gcc9" "mp-clang-9.0")
declare -a ccs=("clang" "gcc" "clang")
declare -a cxxs=("clang++" "g++" "clang++")

ncomp=${#ccs[@]}


#--- Loop through tests and compilers
for ((k = 0; k < ncomp; k++)); do
  echo $'\n'$'\n'$'\n'Test SOILWAT2 with ${port_compilers[k]} ------------------

  echo $'\n'Set compiler ...
  if [ "${port_compilers[k]}" != "none" ]; then
    sudo port select --set ${ccs[k]} ${port_compilers[k]}
  fi

  echo $'\n'`"${ccs[k]}" --version`
  echo $'\n'`"${cxxs[k]}" --version`


  echo $'\n'$'\n'Run binary with ${port_compilers[k]} ...
  CC=${ccs[k]} CXX=${cxxs[k]} make clean bin bint_run
  CC=${ccs[k]} CXX=${cxxs[k]} make clean bin_debug_severe bint_run
  if command -v valgrind >/dev/null 2>&1; then
    CC=${ccs[k]} CXX=${cxxs[k]} make bind_valgrind bint_run
  fi

  echo $'\n'$'\n'Run tests with ${port_compilers[k]} ...
  CC=${ccs[k]} CXX=${cxxs[k]} make clean test test_run
  CC=${ccs[k]} CXX=${cxxs[k]} make clean test_severe test_run

  echo $'\n'$'\n'Run sanitizer on tests with ${port_compilers[k]}: exclude known leaks ...
  CC=${ccs[k]} CXX=${cxxs[k]} ASAN_OPTIONS=detect_leaks=1 LSAN_OPTIONS=suppressions=.LSAN_suppr.txt make clean test_severe test_run


  echo $'\n'$'\n'Unset compiler ...
  if [ "${port_compilers[k]}" != "none" ]; then
    sudo port select --set ${ccs[k]} none
  fi
done

echo Complete!$'\n'


#--- Cleanup
unset -v ccs
unset -v cxxs
unset -v port_compilers
