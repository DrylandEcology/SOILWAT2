#!/bin/bash

# Runs several tests for SOILWAT2
# - is currently set up for use with macports
# - expects clang as default compiler


# Notes:
# - googletests (Jan 2022) requires a C++11 compliant compilers,
#   gcc >= 5.0 or clang >= 5.0,
#   and POSIX API (e.g., `_POSIX_C_SOURCE=200809L`)
#   which is not enabled by default on all systems


#--- Name of reference output
dir_out_ref="tests/example/Output_ref"


#--- Verbosity on failure
#  * on error: if true, print entire output
#  * on error: if false (default), print only selected parts that include the error message
verbosity_on_error=false # options: true false


#--- List of (builtin and macport) compilers

declare -a port_compilers=(
  "none"
  "mp-gcc10" "mp-gcc11" "mp-gcc12"
  "mp-clang-10" "mp-clang-11" "mp-clang-12" "mp-clang-13" "mp-clang-14" "mp-clang-15" "mp-clang-16"
)
declare -a ccs=(
  "clang"
  "gcc" "gcc" "gcc"
  "clang" "clang" "clang" "clang" "clang" "clang" "clang"
)
declare -a cxxs=(
  "clang++"
  "g++" "g++" "g++"
  "clang++" "clang++" "clang++" "clang++" "clang++" "clang++" "clang++"
)


ncomp=${#ccs[@]}


#--- Function to check testing output
check_testing_output () {
  if diff  -q -x "\.DS_Store" -x "\.gitignore" tests/example/Output/ "${dir_out_ref}"/ >/dev/null 2>&1; then
    echo "Success: reference output is reproduced."
  else
    echo "Failure: reference output is not reproduced:"
    diff  -qs -x "\.DS_Store" -x "\.gitignore" tests/example/Output/ "${dir_out_ref}"/
  fi
}

#--- Function to check for errors (but avoid false hits)
# $1 Text string
# Returns lines that contain "failed", "abort", "trap", " not ", or "error"
# ("error" besides "Werror", "Wno-error", or "*Test.Errors")
check_error() {
  echo "$1" | awk 'tolower($0) ~ /[^-.w]error|failed|abort|trap|( not )/'
}


#--- Loop through tests and compilers
for ((k = 0; k < ncomp; k++)); do
  echo $'\n'$'\n'$'\n'\
       ==================================================$'\n'\
       ${k}')' Test SOILWAT2 with compiler \'${port_compilers[k]}\'$'\n'\
       ==================================================

  echo $'\n'"Set compiler ..."
  if [ "${port_compilers[k]}" != "none" ]; then
    res=$(sudo port select --set ${ccs[k]} ${port_compilers[k]})

    if ( echo ${res} | grep "failed" ); then
      continue
    fi
  fi

  CC=${ccs[k]} CXX=${cxxs[k]} make compiler_version


  echo $'\n'$'\n'\
       --------------------------------------------------$'\n'\
       ${k}'-a)' Run example simulation with ${port_compilers[k]} ...$'\n'\
       --------------------------------------------------

  echo $'\n'"Target 'bin_run' ..."
  res=$(CC=${ccs[k]} CXX=${cxxs[k]} make clean bin_run 2>&1)

  res_error=$(check_error "${res}")
  if [ "${res_error}" ]; then
    echo "Failure:"
    if [ "${verbosity_on_error}" = true ]; then
      echo "${res}"
    else
      echo "${res_error}"
    fi

  else
    echo "Success: target."

    if [ ${k} -eq 0 ]; then
      if [ ! -d "${dir_out_ref}" ]; then
        echo "Save default testing output as reference for future comparisons"
        cp -r tests/example/Output "${dir_out_ref}"
      fi
    fi

    check_testing_output
  fi

  echo $'\n'"Target 'bin_debug_severe' ..."
  res=$(CC=${ccs[k]} CXX=${cxxs[k]} make clean bin_debug_severe 2>&1)

  res_error=$(check_error "${res}")
  if [ "${res_error}" ]; then
    echo "Failure:"
    if [ "${verbosity_on_error}" = true ]; then
      echo "${res}"
    else
      echo "${res_error}"
    fi

  else
    echo "Success: target."
    check_testing_output
  fi


  if command -v valgrind >/dev/null 2>&1; then
    echo $'\n'"Target 'bind_valgrind' ..."
    res=$(CC=${ccs[k]} CXX=${cxxs[k]} make clean bind_valgrind)
  fi


  echo $'\n'$'\n'\
       --------------------------------------------------$'\n'\
       ${k}'-b)' Run sanitizer on example simulation with ${port_compilers[k]}: exclude known leaks ...$'\n'\
       --------------------------------------------------

  echo $'\n'"Target 'bin_leaks' ..."
  res=$(CC=${ccs[k]} CXX=${cxxs[k]} make clean bin_leaks 2>&1)

  res_error=$(check_error "${res}")
  if [ "${res_error}" ]; then
    echo "Failure:"
    if [ "${verbosity_on_error}" = true ]; then
      echo "${res}"
    else
      echo "${res_error}"
    fi

  else
    echo "Success: target."
  fi


  echo $'\n'$'\n'\
       --------------------------------------------------$'\n'\
       ${k}'-c)' Run tests with ${port_compilers[k]} ...$'\n'\
       --------------------------------------------------

  echo $'\n'"Target 'test_run' ..."
  res=$(CC=${ccs[k]} CXX=${cxxs[k]} make clean test_run 2>&1)

  res_error=$(check_error "${res}")
  if [ "${res_error}" ]; then
    echo "Failure:"
    if [ "${verbosity_on_error}" = true ]; then
      echo "${res}"
    else
      echo "${res_error}"
    fi

  else
    echo "Success: target."
  fi


  echo $'\n'"Target 'test_severe' ..."
  res=$(CC=${ccs[k]} CXX=${cxxs[k]} make clean test_severe 2>&1)

  res_error=$(check_error "${res}")
  if [ "${res_error}" ]; then
    echo "Failure:"
    if [ "${verbosity_on_error}" = true ]; then
      echo "${res}"
    else
      echo "${res_error}"
    fi

  else
    echo "Success: target."

    echo $'\n'$'\n'\
         --------------------------------------------------$'\n'\
         ${k}'-d)' Run sanitizer on tests with ${port_compilers[k]}: exclude known leaks ...$'\n'\
         --------------------------------------------------

    # CC=${ccs[k]} CXX=${cxxs[k]} ASAN_OPTIONS=detect_leaks=1 LSAN_OPTIONS=suppressions=.LSAN_suppr.txt make clean test_severe test_run
    # https://github.com/google/sanitizers/wiki/AddressSanitizer
    echo $'\n'"Target 'test_leaks' ..."
    res=$(CC=${ccs[k]} CXX=${cxxs[k]} make clean test_leaks 2>&1)

    res_error=$(check_error "${res}")
    if [ "${res_error}" ]; then
      echo "Failure:"
      if [ "${verbosity_on_error}" = true ]; then
        echo "${res}"
      else
        echo "${res_error}"
      fi

    else
      echo "Success: target."
    fi
  fi


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
