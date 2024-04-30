#!/bin/bash

#------ . ------
# Compare SOILWAT2 output among text-based, nc-based, and rSOILWAT2 runs
#
# Run this script with `./tools/run_compare_outputs.sh`
#------ . ------


echo "Run text-based SOILWAT2 on example simulation ..."
res=$(rm -r tests/example/Output tests/example/Output_txt 2>&1)
res=$(make clean bin_run 2>&1)
res=$(cp -R tests/example/Output tests/example/Output_txt 2>&1)

echo "Run nc-based SOILWAT2 on example simulation ..."
res=$(rm -r tests/example/Output tests/example/Output_nc 2>&1)
res=$(rm tests/example/Input_nc/domain.nc 2>&1)
res=$(CPPFLAGS='-DSWNETCDF -DSWUDUNITS' make clean bin_run 2>&1)
res=$(mv tests/example/Input_nc/domain_template.nc tests/example/Input_nc/domain.nc 2>&1)
res=$(bin/SOILWAT2 -d ./tests/example -f files.in 2>&1)
res=$(cp -R tests/example/Output tests/example/Output_nc 2>&1)
res=$(rm tests/example/Input_nc/domain.nc tests/example/Input_nc/progress.nc 2>&1)

echo "Compare output among text-based, nc-based, and rSOILWAT2 runs:"
res=$(Rscript tools/check__output_txt_vs_r_vs_nc.R 2>&1)

echo "${res}"

# Fail if not success
if [[ "${res}" != *"Success"* ]]; then
  exit 1
fi
