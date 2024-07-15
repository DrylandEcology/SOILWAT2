#!/bin/bash

#------ . ------
# Run SOILWAT2 checks with several compilers
#
# Run this script with `tools/check_withCompilers.sh`
#------ . ------

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

#--- Name of reference output (code will append -txt or -nc)
dirOutRefBase="tests/example/Output_v700"


#--- Test with all compilers (true) or with only one recent version per type (false)
use_all_compilers=false # options: true false


#--- Verbosity on failure
#  * on error: if true, print entire output
#  * on error: if false (default), print only selected parts that include the error message
verbosity_on_error=false # options: true false


#------ SETTINGS ---------------------------------------------------------------

#--- List of (builtin and macport) compilers

if [ "${use_all_compilers}" = true ]; then
    declare -a port_compilers=(
      "default compiler"
      "mp-gcc10" "mp-gcc11" "mp-gcc12" "mp-gcc13"
      "mp-clang-10" "mp-clang-11" "mp-clang-12" "mp-clang-13" "mp-clang-14" "mp-clang-15" "mp-clang-16" "mp-clang-17" "mp-clang-18"
    )
    declare -a ccs=(
      "clang"
      "gcc" "gcc" "gcc" "gcc"
      "clang" "clang" "clang" "clang" "clang" "clang" "clang" "clang" "clang"
    )
    declare -a cxxs=(
      "clang++"
      "g++" "g++" "g++" "g++"
      "clang++" "clang++" "clang++" "clang++" "clang++" "clang++" "clang++" "clang++" "clang++"
    )
else
    declare -a port_compilers=("default compiler" "mp-gcc12" "mp-clang-17")
    declare -a ccs=("clang" "gcc" "clang")
    declare -a cxxs=("clang++" "g++" "clang++")
fi

ncomp=${#ccs[@]}



#------ FUNCTIONS --------------------------------------------------------------

# Import functions
myDir=$(dirname ${BASH_SOURCE[0]}) # directory of this script

source "${myDir}/check_functionality.sh"


#------ MAIN -------------------------------------------------------------------

#--- Loop through compilers
for ((k = 0; k < ncomp; k++)); do
  echo $'\n'$'\n'$'\n'\
==================================================$'\n'\
${k}") Test SOILWAT2 with compiler "\'"${port_compilers[k]}"\'$'\n'\
==================================================

  echo $'\n'"Set compiler ..."
  res=""
  if [ "${port_compilers[k]}" = "default compiler" ]; then
    res=$(sudo port select --set "${ccs[k]}" none)
  else
    res=$(sudo port select --set "${ccs[k]}" ${port_compilers[k]})
  fi

  if ( echo "${res}" | grep "failed" ); then
    continue
  fi

  CC=${ccs[k]} CXX=${cxxs[k]} make compiler_version


  # Run checks
  check_SOILWAT2 "CC=${ccs[k]}" "CXX=${cxxs[k]}" "txt" "${dirOutRefBase}" "${verbosity_on_error}"

  check_SOILWAT2 "CC=${ccs[k]}" "CXX=${cxxs[k]}" "nc" "${dirOutRefBase}" "${verbosity_on_error}"



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
