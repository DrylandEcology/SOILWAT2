#!/bin/bash

#------ . ------
# Compare SOILWAT2 output among text-based, nc-based, and rSOILWAT2 runs
#
# Run this script with `tools/check_outputModes.sh`
#------ . ------


#------ FUNCTIONS --------------------------------------------------------------
# Import functions
myDir=$(dirname ${BASH_SOURCE[0]}) # directory of this script

source "${myDir}/check_functionality.sh"


#------ MAIN -------------------------------------------------------------------
declare -a noflags=()

echo "Run text-based SOILWAT2 on example simulation ..."
rm -r tests/example/Output_comps-txt > /dev/null 2>&1
run_fresh_sw2 "CC=" noflags[@] "txt" "bin_run" > /dev/null 2>&1
cp -R tests/example/Output tests/example/Output_comps-txt > /dev/null 2>&1

echo "Run nc-based SOILWAT2 on example simulation ..."
rm -r tests/example/Output_comps-nc > /dev/null 2>&1
run_fresh_sw2 "CC=" noflags[@] "nc" "bin_run" > /dev/null 2>&1
cp -R tests/example/Output tests/example/Output_comps-nc > /dev/null 2>&1
clean_example_inputs > /dev/null 2>&1

unset noflags

echo "Compare output among text-based, nc-based, and rSOILWAT2 runs:"
res=$(Rscript tools/rscripts/Rscript__SW2_output_txt_vs_r_vs_nc.R 2>&1)

echo "${res}"

# Fail if not success
if [[ "${res}" != *"Success"* ]]; then
  exit 1
fi
