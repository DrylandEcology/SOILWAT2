#!/bin/bash


#------ . ------
# Collection of functions that are useful for various checks of SOILWAT2
#
# These functions are used by other scripts and they can be called directly.
# For instance,
#     bash tools/check_functionality.sh check_SOILWAT2 "CC=" "CXX=" "txt" "tests/example/Output_ref" "false"
#     bash tools/check_functionality.sh run_fresh_sw2 "CC=" noflags[@] "txt" "bin_run"
#------ . ------



#------ FUNCTIONS --------------------------------------------------------------


#--- Function to check if a command exists
# $1 name of command
exists() {
  command -v "$1" > /dev/null 2>&1
}


#--- Function to run example SOILWAT2 simulation with fresh inputs
# $1 Compiler, e.g., CC=clang, CXX=g++, or (if not specify a compiler) CC=
# $2 Array of compiler flags, e.g., CPPFLAGS='-DDEBUG'
# $3 SOILWAT2 mode: txt or nc
# $4 Make target to run
run_fresh_sw2() {
  local res="" status=""

  local compiler="$1"
  local -a mflags=("${!2}") # copy content of array
  local mode="$3"
  local target="$4"

  if [ "${compiler}" != "CC=" ] && [ "${compiler}" != "CXX=" ]; then
    if [ ${#mflags[@]} -eq 0 ] || [ -z "${mflags[0]}" ]; then
      # array is either empty or its first value is an empty string
      mflags=("${compiler}")
    else
      mflags+=("${compiler}")
    fi
  fi

  if [ "${mode}" = "nc" ]; then
    mflags+=("CPPFLAGS=-DSWNC")
  fi

  res=$(make clean_example)

  res=$(make "${mflags[@]}" clean "${target}" 2>&1)
  status=$?

  if [[ "$res" == *"Domain netCDF template has been created"* ]]; then
    # Re-run after copying domain template as domain
    mv tests/example/Input_nc/domain_template.nc tests/example/Input_nc/domain.nc > /dev/null 2>&1
    res=$(make "${mflags[@]}" "${target}" 2>&1)
    status=$?
  fi

  if [ $status -eq 0 ]; then
    echo "${res}"
  else
    echo "make failed: ${res}"
  fi
}


#--- Function to compare output against reference
compare_output_with_R () {
  if exists Rscript ; then
    local mode="$1"
    local dirOutRef="$2"

    if [ "${mode}" = "txt" ]; then
      Rscript \
        -e 'dor <- as.character(commandArgs(TRUE)[[1L]])' \
        -e 'fnames <- list.files(path = dor, pattern = ".csv$")' \
        -e 'res <- vapply(fnames, function(fn) isTRUE(all.equal(utils::read.csv(file.path(dor, fn)), utils::read.csv(file.path("tests/example/Output", fn)))), FUN.VALUE = NA)' \
        -e 'if (!all(res)) for (k in which(!res)) cat(shQuote(fnames[[k]]), "and reference differ beyond tolerance.\n")' \
        "${dirOutRef}"

    else
      Rscript \
        -e 'dor <- as.character(commandArgs(TRUE)[[1L]])' \
        -e 'fnames <- list.files(path = dor, pattern = ".nc$")' \
        -e 'res <- vapply(fnames, function(fn) {'\
        -e '    nc1 <- RNetCDF::open.nc(file.path(dor, fn)); on.exit(RNetCDF::close.nc(nc1), add = TRUE)' \
        -e '    nc2 <- RNetCDF::open.nc(file.path("tests/example/Output", fn)); on.exit(RNetCDF::close.nc(nc2), add = TRUE)' \
        -e '    isTRUE(all.equal(RNetCDF::read.nc(nc1), RNetCDF::read.nc(nc2)))' \
        -e '  }, FUN.VALUE = NA)' \
        -e 'if (!all(res)) for (k in which(!res)) cat(shQuote(fnames[[k]]), "and reference differ beyond tolerance.\n")' \
        "${dirOutRef}"
    fi

  else
    echo "R is not installed: cannot compare output against reference with tolerance."
  fi
}

compare_output_against_reference () {
  local mode="$1"
  local dirOutRef="$2"
  local verbosity="$3"

  if diff  -q -x "\.DS_Store" -x "\.gitignore" tests/example/Output/ "${dirOutRef}"/ > /dev/null 2>&1; then
    echo "Simulation: success: output reproduces reference exactly."

  else
    local res=$(compare_output_with_R "${mode}" "${dirOutRef}")
    if [ "${res}" ]; then
      echo "Simulation: failure: output deviates beyond tolerance from reference:"
      echo "${res}"
    else
      echo "Simulation: success: output reproduces reference within tolerance but not exactly"
    fi

    if [ "${verbosity}" = true ]; then
      local res=$(diff  -qs -x "\.DS_Store" -x "\.gitignore" tests/example/Output/ "${dirOutRef}"/)
      echo "${res}" | awk '{ if($NF == "differ") print }'
    fi
  fi
}

#--- Function to check of sanitizers are supported
# $1 Text string
check_has_sanitizer() {
  if [[ "$1" == *"detect_leaks is not supported"* ]]; then
    echo "Sanitizers (detect_leaks) are not supported."
    return 1
  else
    return 0
  fi
}


#--- Function to check for errors (but avoid false hits)
# $1 Text string
# Returns lines that contain "failed", "abort", "trap", "fault", " not ", or "error"
# Exclusions
#   * flags: "Werror", "Wno-error", "default",
#   * unit test names: "*Test.Errors", "RNGBetaErrorsDeathTest",
#   * program message: "Domain netCDF template has been created" (skip entire check)
check_error() {
  if [[ "$1" != *"Domain netCDF template has been created"* ]]; then
    echo "$1" | awk 'tolower($0) ~ /[^-.w]error|failed|abort|trap|[^e]fault|( not )/ && !/RNGBetaErrorsDeathTest/ && !/WarningsAndErrors/ && !/FailOnErrorDeath/'
  fi
}

#--- Function to report on errors and return whether any were found
# $1 Text string to check for errors
# $2 Verbosity (true, false)
# Returns 0 if no errors found; returns 1 if any errors were found
report_if_error() {
  local x="$1"
  local verbosity="$2"

  check_has_sanitizer "${x}"
  local has_sanitizer=$?

  if [ $has_sanitizer -eq 0 ]; then
    local res=$(check_error "${x}")

    if [ "${res}" ]; then
      echo "Target: failure:"
      if [ "${verbosity}" = true ]; then
        echo "${x}"
      else
        echo "${res}"
      fi
      return 1 # return non-zero if errors were found
    else
      echo "Target: success."
      return 0 # return zero if no errors were found
    fi

  else
    return 0 # return zero if sanitizer is not available
  fi
}


#--- Function to check for leaks (but avoid false hits)
# $1 Text string
# Returns lines that contain "leak"
# (besides name of the command, report of no leaks, or
# any unit test name: "WeatherNoMemoryLeakIfDecreasedNumberOfYears")
check_leaks() {
  echo "$1" | awk 'tolower($0) ~ /leak/ && !/leaks Report/ && !/ 0 leaks/ && !/debuggable/ && !/NoMemoryLeak/ && !/leaks.sh/'
}

#--- Function to report on leaks and return whether any were found
# $1 Text string to check for leaks
# $2 Verbosity (true, false)
# Returns 0 if no leak found; returns 1 if leaks were found
report_if_leak() {
  local x="$1"
  local verbosity="$2"

  check_has_sanitizer "${x}"
  local has_sanitizer=$?

  if [ $has_sanitizer -eq 0 ]; then
    local res=$(check_leaks "${x}")

    if [ "${res}" ]; then
      echo "Target: failure: leaks detected:"
      if [ "${verbosity}" = true ]; then
        echo "${x}"
      else
        echo "${res}"
      fi
      return 1 # return non-zero if leaks were found

    else
      echo "Target: success: no leaks detected"
      return 0 # return zero if no leak found
    fi

  else
    return 0 # return zero if sanitizer is not available
  fi
}


#--- Function that compile, run, and check several SOILWAT2 targets
# $1 CC compiler, e.g., CC=clang
# $2 CXX compiler, e.g., CXX=clang++
# $3 SOILWAT2 output mode, i.e., "txt" or "nc"
# $4 Path to the reference output
# $5 Should error messages be verbose, i.e., "true" or "false"
check_SOILWAT2() {
  local ccomp="$1"
  local cxxcomp="$2"
  local mode="$3"
  local dirOutRefBase="$4"
  local verbosity="$5"

  local res="" status="" has_sanitizers=""
  local -a aflags=()

  local dirOutRef="${dirOutRefBase}-${mode}"


  echo $'\n'\
--------------------------------------------------$'\n'\
"Compile ""${mode}""-based library"$'\n'\
--------------------------------------------------

  echo $'\n'"Target 'lib' for SOILWAT2 ..."
  res=$(run_fresh_sw2 "${ccomp}" aflags[@] "${mode}" lib)
  report_if_error "${res}" "${verbosity}"

  if [ "${mode}" = "txt" ]; then
    echo $'\n'"Target 'lib' for rSOILWAT2 ..."
    aflags=("CPPFLAGS=-DRSOILWAT" "CFLAGS=-Iexternal/Rmock")
    res=$(run_fresh_sw2 "${ccomp}" aflags[@] "${mode}" libr)
    aflags=()
    report_if_error "${res}" "${verbosity}"

    echo $'\n'"Target 'lib' for STEPWAT2 ..."
    aflags=("CPPFLAGS=-DSTEPWAT")
    res=$(run_fresh_sw2 "${ccomp}" aflags[@] "${mode}" lib)
    aflags=()
    report_if_error "${res}" "${verbosity}"
  fi


  echo $'\n'\
--------------------------------------------------$'\n'\
"Run ""${mode}""-based example simulation"$'\n'\
--------------------------------------------------

  echo $'\n'"Target 'bin_run' ..."
  res=$(run_fresh_sw2 "${ccomp}" aflags[@] "${mode}" bin_run)

  report_if_error "${res}" "${verbosity}"
  status=$?

  if [ $status -eq 0 ]; then
    if [ ! -d "${dirOutRef}" ]; then
      echo $'\n'"Save output from example simulation as reference for future comparisons"
      cp -r tests/example/Output "${dirOutRef}"
    fi

    compare_output_against_reference "${mode}" "${dirOutRef}" "${verbosity}"
  fi

  echo $'\n'"Target 'bin_debug_severe' ..."
  res=$(run_fresh_sw2 "${ccomp}" aflags[@] "${mode}" bin_debug_severe)

  report_if_error "${res}" "${verbosity}"
  status=$?
  if [ $status -eq 0 ]; then
    compare_output_against_reference "${mode}" "${dirOutRef}" "${verbosity}"
  fi


  echo $'\n'"Target 'bin_sanitizer' ..."
  res=$(run_fresh_sw2 "${ccomp}" aflags[@] "${mode}" bin_sanitizer)
  report_if_error "${res}" "${verbosity}"
  has_sanitizers=$?


  if exists leaks ; then
    echo $'\n'"Target: 'bin_leaks' ..."
    res=$(run_fresh_sw2 "${ccomp}" aflags[@] "${mode}" all)

    report_if_error "${res}" "${verbosity}"
    status=$?
    if [ $status -eq 0 ]; then

      res=$(make bin_leaks 2>&1)
      report_if_leak "${res}" "${verbosity}"
    fi

  else
    echo "Target: skipped: 'leaks' command not available."
  fi



  echo $'\n'$'\n'\
--------------------------------------------------$'\n'\
"Run ""${mode}""-based tests"$'\n'\
--------------------------------------------------

  echo $'\n'"Target 'test_run' ..."
  res=$(run_fresh_sw2 "${cxxcomp}" aflags[@] "${mode}" test_run)
  report_if_error "${res}" "${verbosity}"


  echo $'\n'"Target 'test_rep3rnd' ..."
  res=$(run_fresh_sw2 "${cxxcomp}" aflags[@] "${mode}" test_rep3rnd)
  report_if_error "${res}" "${verbosity}"


  if [ $has_sanitizers -eq 0 ]; then
    echo $'\n'"Target 'test_severe' ..."
    res=$(run_fresh_sw2 "${cxxcomp}" aflags[@] "${mode}" test_severe)
    report_if_error "${res}" "${verbosity}"
    has_sanitizers=$?
  else
    echo "Target: skipped: 'test_severe' not operational for current compiler."
  fi


  if [ $has_sanitizers -eq 0 ]; then
    # CXX=clang++ ASAN_OPTIONS=detect_leaks=1 LSAN_OPTIONS=suppressions=.LSAN_suppr.txt make clean test_severe test_run
    # https://github.com/google/sanitizers/wiki/AddressSanitizer
    echo $'\n'"Target 'test_sanitizer' ..."
    res=$(run_fresh_sw2 "${cxxcomp}" aflags[@] "${mode}" test_sanitizer)
    report_if_error "${res}" "${verbosity}"
  else
    echo "Target: skipped: 'test_sanitizer' not operational for current compiler."
  fi


  if exists leaks ; then

    echo $'\n'"Target: 'test_leaks' ..."
    res=$(run_fresh_sw2 "${cxxcomp}" aflags[@] "${mode}" test)

    report_if_error "${res}" "${verbosity}"
    status=$?
    if [ $status -eq 0 ]; then
      res=$(make test_leaks 2>&1)
      report_if_leak "${res}" "${verbosity}"
    fi

  else
    echo "Target: skipped: 'leaks' command not available."
  fi
}



#---- In case we want to execute a function directly from this script
# Check if there was an argument and if it is an existing function
if [ ! -z "$1" ]; then
  if declare -f "$1"> /dev/null ; then
    # call arguments verbatim
    "$@"
  else
    echo "'$1' is not an implemented function." >&2
    exit 1
  fi
fi
