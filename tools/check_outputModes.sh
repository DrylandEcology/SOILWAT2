#!/bin/bash

#------ . ------
# Compare SOILWAT2 output among text-based, nc-based, mpi-based, and rSOILWAT2 runs
#
# Run this script with `tools/check_outputModes.sh`
#------ . ------


#------ FUNCTIONS --------------------------------------------------------------
# Import functions
myDir=$(dirname ${BASH_SOURCE[0]}) # directory of this script

source "${myDir}/check_functionality.sh"


#--- Check if we have a parallel setup
doParallelSOILWAT2=false
pCC=""

if [ $(nc-config --has-parallel4) = "yes" ]; then
    doParallelSOILWAT2=true

    source "${myDir}/hasMPICC.sh"

    if has_mpicc ; then
        pCC="mpicc"
    fi
fi


#------ MAIN -------------------------------------------------------------------
declare -a noflags=()

echo "Run text-based SOILWAT2 on example simulation ..."
rm -r tests/example/Output_comps-txt > /dev/null 2>&1
res=$(run_fresh_sw2 "CC=" noflags[@] "txt" "bin_run" 2>&1)
if [[ "${res}" == *"make failed"* ]]; then
    echo "${res}"
else
    cp -R tests/example/Output tests/example/Output_comps-txt > /dev/null 2>&1
fi

echo "Run nc-based SOILWAT2 on example simulation ..."
rm -r tests/example/Output_comps-nc > /dev/null 2>&1
res=$(run_fresh_sw2 "CC=" noflags[@] "nc" "bin_run" 2>&1)
if [[ "${res}" == *"make failed"* ]]; then
    echo "${res}"
else
    cp -R tests/example/Output tests/example/Output_comps-nc > /dev/null 2>&1
fi
res=$(make clean_example)


echo "Run mpi-based SOILWAT2 on example simulation ..."
rm -r tests/example/Output_comps-mpi > /dev/null 2>&1
if $doParallelSOILWAT2 ; then
    noflags[0]="SW_NTASKS=2"
    res=$(run_fresh_sw2 "CC=${pCC}" noflags[@] "mpi" "bin_run" 2>&1)
    if [[ "${res}" == *"make failed"* ]]; then
        echo "${res}"
    else
        cp -R tests/example/Output tests/example/Output_comps-mpi > /dev/null 2>&1
    fi
    res=$(make clean_example)

else
    echo "Skip mpi-based SOILWAT2."
fi

unset noflags

# Compare output among text-based, nc-based, mpi-based, and rSOILWAT2 runs
res=$(Rscript tools/rscripts/Rscript__SW2_output_txt_vs_r_vs_nc.R 2>&1)

echo "${res}"

# Fail if not success
if [[ "${res}" != *"Success"* ]]; then
  exit 1
fi
