#!/bin/bash

# Execute SOILWAT2 via `mpirun` or `srun` if mpi-based
# Execute SOILWAT2 directly if not mpi-based

# run as `./tools/run_bin.sh` [OPTIONS]
# supported options
#   -n,-np,--ntasks=<number>

# note: consider cleaning previous build artifacts, e.g., `make clean_build`

# or via makefile
# run as `make clean run_bin`


#--- Command line arguments
nTasks=""

while [ $# -gt 0 ]; do
    case "$1" in
        --ntasks=*) nTasks="${1#*=}" ;;
        -n|-np)
            shift
            if [ $# -eq 0 ]; then
                echo "Error: Option -n requires a value."
                exit 1
            fi
            nTasks="$1"
            ;;

        *) echo "Argument ""$1"" is not implemented."; exit 1 ;;
    esac
    shift
done


# Check if SOILWAT2 capabilities include MPI
# (Using grep instead of [[ ... =~ ... ]] because of newlines in output)
if bin/SOILWAT2 -v 2>&1 | grep -qE 'Capabilities:.*MPI' ; then

    if command -v srun > /dev/null 2>&1 ; then
        if [[ -z "${nTasks}" ]]; then
            srun bin/SOILWAT2 -d ./tests/example -f files.in -r
        else
            srun -n "${nTasks}" bin/SOILWAT2 -d ./tests/example -f files.in -r
        fi

    elif command -v mpirun > /dev/null 2>&1 ; then
        if [[ -z "${nTasks}" ]]; then
            mpirun bin/SOILWAT2 -d ./tests/example -f files.in -r
        else
            mpirun -n "${nTasks}" bin/SOILWAT2 -d ./tests/example -f files.in -r
        fi

    else
        echo "Cannot execute mpi-based SOILWAT2: neither srun nor mpirun are available."
        exit 1
    fi

else
    bin/SOILWAT2 -d ./tests/example -f files.in -r
fi
