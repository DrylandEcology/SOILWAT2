#!/bin/bash

#------ . ------
# All SOILWAT2 output tests
#
# Run this script with `tools/allOutputChecks.sh [OPTIONS]`
# supported options
#   -t,--outTag <outputTag>     `check_SOILWAT2` compares output against "Output_<outputTag>"
#   --outTag=<outputTag>
#
#   -n,-np <number>             Number of parallel processes in mpi-mode SOILWAT2.
#   --ntasks=<number>
#
#   -w,-wt <number>             Maximum walltime per SOILWAT2 run.
#   --walltime=<number>
#
#------ . ------


#--- Command line arguments
outTag="ref"
nTasks="1"
walltime="600" # seconds

while [ $# -gt 0 ]; do
    case "$1" in
        --outTag=*) outTag="${1#*=}" ;;
        -t|--outTag) outTag="$2"; shift ;;

        --ntasks=*) nTasks="${1#*=}" ;;
        -n|-np) nTasks="$2"; shift ;;

        --walltime=*) walltime="${1#*=}" ;;
        -w|-wt) walltime="$2"; shift ;;

        *) echo "Option ""$1"" is not implemented."; exit 1 ;;
    esac
    shift
done

pathReferenceOutput="tests/example/Output_""${outTag}"

if [ ! -d "${pathReferenceOutput}""-txt" ]; then
    echo "Note: Reference output does not yet exist: ""${pathReferenceOutput}"
fi


#--- Check if we have a parallel setup
doParallelSOILWAT2=false
pCC=""
pCXX=""

myDir=$(dirname ${BASH_SOURCE[0]}) # directory of this script
source "${myDir}/hasMPICC.sh"
useMPICC=$(has_mpicc && echo "yes" || echo "no")

if [ $(nc-config --has-parallel4) = "yes" ] || [ "${useMPICC}" = "yes" ]; then
    doParallelSOILWAT2=true

    if [ "${useMPICC}" = "yes" ] ; then
        pCC="mpicc"
        if command -v mpic++ > /dev/null 2>&1 ; then
            pCXX="mpic++"
        elif command -v mpicxx > /dev/null 2>&1 ; then
            pCXX="mpicxx"
        else
            pCXX=""
        fi
    fi
fi


#--- Checks and tests
echo $'\n'\
==================================================$'\n'\
"All output tests ..."$'\n'\
==================================================

echo $'\n'\
==================================================$'\n'\
"SOILWAT2 (txt, nc): tests and example output with default compiler ..."$'\n'\
--------------------------------------------------
SW2_TIMEOUT="${walltime}" tools/check_functionality.sh check_SOILWAT2 "CC=" "CXX=" "txt" "" "${pathReferenceOutput}" "false"
SW2_TIMEOUT="${walltime}" tools/check_functionality.sh check_SOILWAT2 "CC=" "CXX=" "nc" "" "${pathReferenceOutput}" "false"


echo $'\n'\
==================================================$'\n'\
"SOILWAT2 (txt, nc): tests and example output with clang ..."$'\n'\
--------------------------------------------------
if command -v clang > /dev/null 2>&1 ; then
    SW2_TIMEOUT="${walltime}" tools/check_functionality.sh check_SOILWAT2 "CC=clang" "CXX=clang++" "txt" "" "${pathReferenceOutput}" "false"
    SW2_TIMEOUT="${walltime}" tools/check_functionality.sh check_SOILWAT2 "CC=clang" "CXX=clang++" "nc" "" "${pathReferenceOutput}" "false"
else
    echo "Skip checks with clang."
fi

echo $'\n'\
==================================================$'\n'\
"SOILWAT2 (txt, nc): tests and example output with gcc ..."$'\n'\
--------------------------------------------------
if command -v gcc > /dev/null 2>&1 ; then
    SW2_TIMEOUT="${walltime}" tools/check_functionality.sh check_SOILWAT2 "CC=gcc" "CXX=g++" "txt" "" "${pathReferenceOutput}" "false"
    SW2_TIMEOUT="${walltime}" tools/check_functionality.sh check_SOILWAT2 "CC=gcc" "CXX=g++" "nc" "" "${pathReferenceOutput}" "false"
else
    echo "Skip checks with gcc."
fi


echo $'\n'\
==================================================$'\n'\
"SOILWAT2 (mpi): tests and example output with ""${pCC}"" ..."$'\n'\
--------------------------------------------------
if $doParallelSOILWAT2 ; then
    SW2_TIMEOUT="${walltime}" tools/check_functionality.sh check_SOILWAT2 "CC=${pCC}" "CXX=${pCXX}" "mpi" "1" "${pathReferenceOutput}" "false"
else
    echo "Skip checks with mpi."
fi


echo $'\n'\
==================================================$'\n'\
"ncTestRuns with nc-based SOILWAT2 ..."$'\n'\
--------------------------------------------------
tools/check_ncTestRuns.sh clean all --mode=nc

if $doParallelSOILWAT2 ; then
    rm -r tests/ncTestRuns/results/testRuns-NC
    mv tests/ncTestRuns/results/testRuns tests/ncTestRuns/results/testRuns-NC
fi


echo $'\n'\
==================================================$'\n'\
"ncTestRuns with mpi-based SOILWAT2 ..."$'\n'\
--------------------------------------------------
if $doParallelSOILWAT2 ; then
    echo $'\n'"MPI-enabled SOILWAT2 ......"
    make clean CC="${pCC}" CPPFLAGS=-DSWMPI all > /dev/null 2>&1
    tools/check_ncTestRuns.sh clean all --mode=mpi --ntasks="${nTasks}"
    rm -r tests/ncTestRuns/results/testRuns-MPI
    mv tests/ncTestRuns/results/testRuns tests/ncTestRuns/results/testRuns-MPI
else
    echo "Skip ncTestRuns with mpi-enabled SOILWAT2."
fi


echo $'\n'\
==================================================$'\n'\
"Compare ncTestRuns between nc-based an mpi-based SOILWAT2 ..."$'\n'\
--------------------------------------------------
if $doParallelSOILWAT2 ; then
    echo $'\n'"Compare ncTestRuns: nc vs. mpi ..."
    tools/check_functionality.sh compare_ncTestRunSets \
        "tests/ncTestRuns/results/testRuns-NC" \
        "tests/ncTestRuns/results/testRuns-MPI" \
        "false"
fi


echo $'\n'\
==================================================$'\n'\
"Consistency of output across all modes ..."$'\n'\
--------------------------------------------------
tools/check_outputModes.sh


echo $'\n'\
==================================================$'\n'\
"Extra checks ..."$'\n'\
--------------------------------------------------
tools/check_extras.sh

echo $'\n'\
==================================================$'\n'\
"Completed all output checks!"$'\n'\
==================================================
