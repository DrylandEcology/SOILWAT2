#!/bin/bash

# Evaluate if mpicc is the correct compiler for mpi-based SOILWAT2
#
# Check if the mpicc wrapper is available and
# if the associated compiler (clang or gcc) is on the PATH
#
# Return 0 if correct else 1
#
# If mpicc is available but associated with an unavailable compiler,
# then consider defining OMPI_CC such that mpicc uses the desired
# compiler, e.g., `export OMPIC_CC=gcc`
has_mpicc() {
    local res
    local compName

    # Get compiler used by mpicc
    res=$(mpicc --show 2>/dev/null)

    if [[ -z "$res" ]]; then
        return 1
    fi

    # Extract compiler name (first word)
    compName=$(echo "$res" | awk '{print $1}')

    # Check if compiler is in the PATH
    if command -v "$compName" >/dev/null 2>&1; then
        return 0
    else
        return 1
    fi
}
