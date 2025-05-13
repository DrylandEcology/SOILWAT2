#!/bin/bash

# Execute SOILWAT2 via `mpirun` or `srun` if mpi-based
# Execute SOILWAT2 directly if not mpi-based

# run as `./tools/run_bin.sh`
# note: consider cleaning previous build artifacts, e.g., `make clean_build`

# or via makefile
# run as `make clean run_bin`


# Check if SOILWAT2 capabilities include MPI
# (Using grep instead of [[ ... =~ ... ]] because of newlines in output)
if bin/SOILWAT2 -v 2>&1 | grep -qE 'Capabilities:.*MPI' ; then

    if command -v srun > /dev/null 2>&1 ; then
        srun -n 2 bin/SOILWAT2 -d ./tests/example -f files.in -r

    elif command -v mpirun > /dev/null 2>&1 ; then
        mpirun -n 2 bin/SOILWAT2 -d ./tests/example -f files.in -r

    else
        echo "Cannot execute mpi-based SOILWAT2: neither srun nor mpirun are available."
        exit 1
    fi

else
    bin/SOILWAT2 -d ./tests/example -f files.in -r
fi
