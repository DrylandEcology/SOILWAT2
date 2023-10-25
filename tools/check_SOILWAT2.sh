#!/bin/bash

# Runs several tests for SOILWAT2
# - is currently set up for use with macports
# - expects clang as default compiler


# Notes:
# - googletests (July 2023) requires a C++14 compliant compilers,
#   gcc >= 7.3.1 or clang >= 7.0.0,
#   and POSIX API (e.g., `_POSIX_C_SOURCE=200809L`)
#   which is not enabled by default on all systems
#   (https://google.github.io/googletest/platforms.html)


#------ USER SETTINGS ----------------------------------------------------------

#--- Name of reference output
dir_out_ref="tests/example/Output_ref"


#--- Verbosity on failure
#  * on error: if true, print entire output
#  * on error: if false (default), print only selected parts that include the error message
verbosity_on_error=false # options: true false


#------ SETTINGS ---------------------------------------------------------------

#--- List of (builtin and macport) compilers

declare -a port_compilers=(
  "default compiler"
  "mp-gcc10" "mp-gcc11" "mp-gcc12" "mp-gcc13"
  "mp-clang-10" "mp-clang-11" "mp-clang-12" "mp-clang-13" "mp-clang-14" "mp-clang-15" "mp-clang-16" "mp-clang-17"
)
declare -a ccs=(
  "clang"
  "gcc" "gcc" "gcc" "gcc"
  "clang" "clang" "clang" "clang" "clang" "clang" "clang" "clang"
)
declare -a cxxs=(
  "clang++"
  "g++" "g++" "g++" "g++"
  "clang++" "clang++" "clang++" "clang++" "clang++" "clang++" "clang++" "clang++"
)


ncomp=${#ccs[@]}



#------ FUNCTIONS --------------------------------------------------------------

#--- Function to compare output against reference
compare_output_with_tolerance () {
  if command -v Rscript >/dev/null 2>&1; then
    Rscript \
      -e 'dir_out_ref <- as.character(commandArgs(TRUE)[[1L]])' \
      -e 'fnames <- list.files(path = dir_out_ref, pattern = ".csv$")' \
      -e 'res <- vapply(fnames, function(fn) isTRUE(all.equal(read.csv(file.path(dir_out_ref, fn)), read.csv(file.path("tests/example/Output", fn)))), FUN.VALUE = NA)' \
      -e 'if (!all(res)) for (k in which(!res)) cat(shQuote(fnames[[k]]), "and reference differ beyond tolerance.\n")' \
      "${dir_out_ref}"
  else
    echo "R is not installed: cannot compare output against reference with tolerance."
  fi
}

compare_output_against_reference () {
  if diff  -q -x "\.DS_Store" -x "\.gitignore" tests/example/Output/ "${dir_out_ref}"/ >/dev/null 2>&1; then
    echo "Simulation: success: output reproduces reference exactly."
  else
    res=$(compare_output_with_tolerance)
    if [ "${res}" ]; then
      echo "Simulation: failure: output deviates beyond tolerance from reference:"
      echo "${res}"
    else
      echo "Simulation: success: output reproduces reference within tolerance but not exactly:"
    fi

    diff  -qs -x "\.DS_Store" -x "\.gitignore" tests/example/Output/ "${dir_out_ref}"/
  fi
}


#--- Function to check for errors (but avoid false hits)
# $1 Text string
# Returns lines that contain "failed", "abort", "trap", "fault", " not ", or "error"
# ("error" besides "Werror", "Wno-error", or
# any unit test name: "*Test.Errors", "RNGBetaErrorsDeathTest")
check_error() {
  echo "$1" | awk 'tolower($0) ~ /[^-.w]error|failed|abort|trap|fault|( not )/ && !/RNGBetaErrorsDeathTest/ && !/WarningsAndErrors/ && !/FailOnErrorDeath/'
}

#--- Function to check for leaks (but avoid false hits)
# $1 Text string
# Returns lines that contain "leak"
# (besides name of the command, report of no leaks, or
# any unit test name: "WeatherNoMemoryLeakIfDecreasedNumberOfYears")
check_leaks() {
  echo "$1" | awk 'tolower($0) ~ /leak/ && !/leaks Report/ && !/ 0 leaks/ && !/debuggable/ && !/NoMemoryLeak/ && !/leaks.sh/'
}


#--- Function to check if a command exists
# $1 name of command
exists() {
  command -v "$1" >/dev/null 2>&1
}


#------ MAIN -------------------------------------------------------------------

#--- Loop through tests and compilers
for ((k = 0; k < ncomp; k++)); do
  echo $'\n'$'\n'$'\n'\
       ==================================================$'\n'\
       ${k}") Test SOILWAT2 with compiler "\'"${port_compilers[k]}"\'$'\n'\
       ==================================================

  echo $'\n'"Set compiler ..."
  res=""
  if [ "${port_compilers[k]}" = "default compiler" ]; then
    res=$(sudo port select --set ${ccs[k]} none)
  else
    res=$(sudo port select --set ${ccs[k]} ${port_compilers[k]})
  fi

  if ( echo "${res}" | grep "failed" ); then
    continue
  fi

  CC=${ccs[k]} CXX=${cxxs[k]} make compiler_version


  echo $'\n'$'\n'\
       --------------------------------------------------$'\n'\
       ${k}"-a) Run example simulation with "\'"${port_compilers[k]}"\'$'\n'\
       --------------------------------------------------

  echo $'\n'"Target 'bin_run' ..."
  res=$(CC=${ccs[k]} make clean bin_run 2>&1)

  res_error=$(check_error "${res}")
  if [ "${res_error}" ]; then
    echo "Target: failure:"
    if [ "${verbosity_on_error}" = true ]; then
      echo "${res}"
    else
      echo "${res_error}"
    fi

  else
    echo "Target: success."

    if [ ${k} -eq 0 ]; then
      if [ ! -d "${dir_out_ref}" ]; then
        echo $'\n'"Save default testing output as reference for future comparisons"
        cp -r tests/example/Output "${dir_out_ref}"
      fi
    fi

    compare_output_against_reference
  fi

  echo $'\n'"Target 'bin_debug_severe' ..."
  res=$(CC=${ccs[k]} make clean bin_debug_severe 2>&1)

  res_error=$(check_error "${res}")
  if [ "${res_error}" ]; then
    echo "Target: failure:"
    if [ "${verbosity_on_error}" = true ]; then
      echo "${res}"
    else
      echo "${res_error}"
    fi

  else
    echo "Target: success."
    compare_output_against_reference
  fi


  if exists valgrind ; then
    echo $'\n'"Target 'bind_valgrind' ..."
    res=$(CC=${ccs[k]} CXX=${cxxs[k]} make clean bind_valgrind)
  fi


  echo $'\n'$'\n'\
       --------------------------------------------------$'\n'\
       ${k}"-b) Run sanitizers on example simulation with "\'"${port_compilers[k]}"\'$'\n'\
       --------------------------------------------------

  echo $'\n'"Target 'bin_sanitizer' ..."
  res=$(CC=${ccs[k]} make clean bin_sanitizer 2>&1)

  res_error=$(check_error "${res}")
  if [ "${res_error}" ]; then
    echo "Target: failure:"
    if [ "${verbosity_on_error}" = true ]; then
      echo "${res}"
    else
      echo "${res_error}"
    fi

  else
    echo "Target: success."
  fi



  if exists leaks ; then

    echo $'\n'"Target: 'bin_leaks' ..."
    res=$(CC=${ccs[k]} make clean all 2>&1)

    res_error=$(check_error "${res}")
    if [ "${res_error}" ]; then
      echo "Target: failure:"
      if [ "${verbosity_on_error}" = true ]; then
        echo "${res}"
      else
        echo "${res_error}"
      fi

    else
      res=$(make bin_leaks 2>&1)

      res_leaks=$(check_leaks "${res}")
      if [ "${res_leaks}" ]; then
        echo "Target: failure: leaks detected:"
        if [ "${verbosity_on_error}" = true ]; then
          echo "${res}"
        else
          echo "${res_leaks}"
        fi

      else
        echo "Target: success: no leaks detected"
      fi
    fi

  else
    echo "Target: skipped: 'leaks' command not found."
  fi



  echo $'\n'$'\n'\
       --------------------------------------------------$'\n'\
       ${k}"-c) Run tests with "\'"${port_compilers[k]}"\'$'\n'\
       --------------------------------------------------

  echo $'\n'"Target 'test_run' ..."
  res=$(CXX=${cxxs[k]} make clean test_run 2>&1)

  res_error=$(check_error "${res}")
  if [ "${res_error}" ]; then
    echo "Target: failure:"
    if [ "${verbosity_on_error}" = true ]; then
      echo "${res}"
    else
      echo "${res_error}"
    fi

  else
    echo "Target: success."
  fi


  echo $'\n'"Target 'test_severe' ..."
  res=$(CXX=${cxxs[k]} make clean test_severe 2>&1)

  res_error=$(check_error "${res}")
  if [ "${res_error}" ]; then
    echo "Target: failure:"
    if [ "${verbosity_on_error}" = true ]; then
      echo "${res}"
    else
      echo "${res_error}"
    fi

    do_target_test_sanitizer=false

  else
    echo "Target: success."

    do_target_test_sanitizer=true
  fi



  echo $'\n'$'\n'\
       --------------------------------------------------$'\n'\
       ${k}"-d) Run sanitizers on tests with "\'"${port_compilers[k]}"\'$'\n'\
       --------------------------------------------------

  if [ "${do_target_test_sanitizer}" = true ]; then
    # CXX=${cxxs[k]} ASAN_OPTIONS=detect_leaks=1 LSAN_OPTIONS=suppressions=.LSAN_suppr.txt make clean test_severe test_run
    # https://github.com/google/sanitizers/wiki/AddressSanitizer
    echo $'\n'"Target 'test_sanitizer' ..."
    res=$(CXX=${cxxs[k]} make clean test_sanitizer 2>&1)

    res_error=$(check_error "${res}")
    if [ "${res_error}" ]; then
      echo "Target: failure:"
      if [ "${verbosity_on_error}" = true ]; then
        echo "${res}"
      else
        echo "${res_error}"
      fi

    else
      echo "Target: success."
    fi

  else
    echo "Target: skipped: 'test_sanitizer' not operational for current compiler."
  fi


  if exists leaks ; then

    echo $'\n'"Target: 'test_leaks' ..."
    res=$(CXX=${cxxs[k]} make clean test 2>&1)

    res_error=$(check_error "${res}")
    if [ "${res_error}" ]; then
      echo "Target: failure:"
      if [ "${verbosity_on_error}" = true ]; then
        echo "${res}"
      else
        echo "${res_error}"
      fi

    else
      res=$(make test_leaks 2>&1)

      res_leaks=$(check_leaks "${res}")
      if [ "${res_leaks}" ]; then
        echo "Target: failure: leaks detected:"
        if [ "${verbosity_on_error}" = true ]; then
          echo "${res}"
        else
          echo "${res_leaks}"
        fi

      else
        echo "Target: success: no leaks detected"
      fi
    fi

  else
    echo "Target: skipped: 'leaks' command not found."
  fi


  echo $'\n'$'\n'\
       --------------------------------------------------$'\n'\
       "Unset compiler"$'\n'\
       --------------------------------------------------

  sudo port select --set ${ccs[k]} none

done


#------ CLEANUP ----------------------------------------------------------------
unset -v ccs
unset -v cxxs
unset -v port_compilers

echo $'\n'$'\n'\
     --------------------------------------------------$'\n'\
     "Complete!"$'\n'\
     --------------------------------------------------$'\n'
