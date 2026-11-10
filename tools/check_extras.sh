#!/bin/bash

#------ . ------
# Extra SOILWAT2 checks and figures that evaluate specific features
#
# Run this script with `tools/check_extras.sh`
#------ . ------


echo $'\n'"Additional checks ..."

echo $'\n'"Spinup evaluation"
CPPFLAGS=-DSW2_SpinupEvaluation make clean test > /dev/null 2>&1
bin/sw_test --gtest_filter=*SpinupEvaluation*
Rscript tools/rscripts/Rscript__SW2_SpinupEvaluation.R


echo $'\n'"Soil temperature evaluation"
make clean bin_run > /dev/null 2>&1
Rscript tools/rscripts/Rscript__SW2_SoilTemperature.R


echo $'\n'"Effects of meteorological variables on PET"
CPPFLAGS=-DSW2_PET_Test__petfunc_by_temps make clean test > /dev/null 2>&1
bin/sw_test --gtest_filter=*PETPetfuncByTemps*
Rscript tools/rscripts/Rscript__SW2_PET_Test__petfunc_by_temps.R


echo $'\n'"Numbers of daylight hours and of sunrises/sunsets for each latitude and day of year"
CPPFLAGS=-DSW2_SolarPosition_Test__hourangles_by_lat_and_doy make clean test > /dev/null 2>&1
bin/sw_test --gtest_filter=*SolarPosHourAnglesByLatAndDoy*
Rscript tools/rscripts/Rscript__SW2_SolarPosition_Test__hourangles_by_lat_and_doy.R


echo $'\n'"Horizontal and tilted first sunrise and last sunset hour angles for each latitude and a combination of slopes/aspects/day of year"
CPPFLAGS=-DSW2_SolarPosition_Test__hourangles_by_lats make clean test > /dev/null 2>&1
bin/sw_test --gtest_filter=*SolarPosHourAnglesByLats*
Rscript tools/rscripts/Rscript__SW2_SolarPosition_Test__hourangles_by_lats.R


echo $'\n'"Compile alternative implementations of sun hour angles (strict warnings)"
source "$(dirname "${BASH_SOURCE[0]}")/compile_flags.sh"
flagsStrict="$(debug_flags "O1") $(compile_flags "CC" "sanitizer-no")"

for swFlag in sun_hourangles_Allen2006 sun_hourangles_Duffie2013_2205; do
    if CPPFLAGS="-D${swFlag}" SW2_FLAGS="${flagsStrict}" make clean all > /dev/null 2>&1; then
        echo "  ${swFlag}: compiled successfully"
    else
        echo "  ${swFlag}: FAILED to compile"
    fi
done
make clean > /dev/null 2>&1

echo $'\n'
