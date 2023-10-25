#!/bin/bash

# run as `./tools/run_gcov.sh`
# note: consider cleaning previous artifacts, e.g., `make clean_cov`

# note: `--coverage` is equivalent to `-fprofile-arcs -ftest-coverage`

# note: make sure that compiler and cover tool version agree, i.e.,
# `CXX=g++ make clean cov`
# `CXX=clang++ GCOV="llvm-cov-mp-14 gcov" make clean_cov cov`

GCOV="${GCOV:-gcov}"

SW2_FLAGS="--coverage" make test_run

dir_build_test="build/test"
dir_src="src"
sources_tests='SW_Main_lib.c SW_VegEstab.c SW_Control.c generic.c
					rands.c Times.c mymemory.c filefuncs.c
					SW_Files.c SW_Model.c SW_Site.c SW_SoilWater.c
					SW_Markov.c SW_Weather.c SW_Sky.c SW_Output_mock.c
					SW_VegProd.c SW_Flow_lib_PET.c SW_Flow_lib.c SW_Flow.c SW_Carbon.c'

for sf in $sources_tests
do
  ${GCOV} -r -p -s "$dir_src" -o "$dir_build_test" "$dir_src"/"$sf"
done
