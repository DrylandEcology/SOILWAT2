#!/bin/bash

# Execute swtest via `mpirun -n 2` or `srun -n 2` if mpi-based
# Execute swtest directly if not mpi-based

# run as `./tools/run_test.sh` [-v]
# supported gtest flags
#   --gtest_filter=POSITIVE_PATTERNS[-NEGATIVE_PATTERNS]
#   --gtest_shuffle
#   --gtest_repeat=[COUNT]
#   --gtest_random_seed=[NUMBER]

# note: consider cleaning previous build artifacts, e.g., `make clean_build`

# or via makefile
# run as `make clean run_test`

#--- Command line arguments
verbose=false
shuffleTest=""
repeatTest=""
filterTest=""
seedTest=""

while [ $# -gt 0 ]; do
    case "$1" in
        -v|--verbose) verbose=true; shift ;;

        --gtest_filter=*) filterTest="--gtest_filter=""${1#*=}"; shift ;;

        --gtest_random_seed=*) seedTest="--gtest_random_seed=""${1#*=}"; shift ;;

        --gtest_shuffle) shuffleTest="--gtest_shuffle"; shift ;;

        --gtest_repeat=*) repeatTest="--gtest_repeat=""${1#*=}"; shift ;;
        --gtest_repeat)
            shift
            if [ $# -eq 0 ]; then
                echo "Error: --gtest_repeat requires a value"
                exit 1
            fi
            repeatTest="--gtest_repeat=""$1"
            shift
            ;;

        *) echo "Argument ""$1"" is not implemented."; exit 1 ;;
    esac
done


# Check if SOILWAT2 capabilities include MPI
# (Using grep instead of [[ ... =~ ... ]] because of newlines in output)
if bin/SOILWAT2 -v 2>&1 | grep -qE 'Capabilities:.*MPI' ; then
    if $verbose ; then
        echo "mpi-based SOILWAT2"
    fi

    if command -v srun > /dev/null 2>&1 ; then
        if $verbose ; then
            echo "Execute via srun"
        fi
        srun -n 2 bin/sw_test "$shuffleTest" "$repeatTest" "$filterTest" "$seedTest"

    elif command -v mpirun > /dev/null 2>&1 ; then
        if $verbose ; then
            echo "Execute via mpirun"
        fi
        mpirun -n 2 bin/sw_test "$shuffleTest" "$repeatTest" "$filterTest" "$seedTest"

    else
        echo "Cannot execute mpi-based SOILWAT2 test program: neither srun nor mpirun are available."
        exit 1
    fi

else
    if $verbose ; then
        echo "Not mpi-based SOILWAT2"
    fi
    bin/sw_test "$shuffleTest" "$repeatTest" "$filterTest" "$seedTest"
fi
