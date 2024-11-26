#!/bin/bash

#-------------------------------------------------------------------------------
# Verify that clang-tidy is available
if ! command -v clang-tidy > /dev/null 2>&1; then
    echo "clang-tidy is not available."
    exit 1
fi

# Verify that clang-tidy is at least v17
ctv=$(clang-tidy --version | egrep -o "version [0-9]+.[0-9]+.[0-9]+")
p1=$(echo "$ctv" | egrep -o "[0-9]+.[0-9]+.[0-9]+" | cut -d "." -f 1)

if [ ! "$p1" -ge 17 ]; then
    echo "We require clang-tidy version 17 or later but found" "${ctv}"
    exit 1
fi


#-------------------------------------------------------------------------------
process_clangtidy_results() {
    local status="$1"
    local res="$2"

    if [ $status -eq 0 ]; then
        # success: clang-tidy found no check failures
        return 0
    else
        # failure: clang-tidy produced check output
        echo "$res"
        exit 1
    fi
}


#-------------------------------------------------------------------------------

echo $'\n'\
--------------------------------------------------$'\n'\
"Tidy text-based SOILWAT2"$'\n'\
--------------------------------------------------

res=$(make CPPFLAGS='-DSWTXT -DSWDEBUG' tidy-bin 2>/dev/null)
status=$?
process_clangtidy_results $status "$res"


echo $'\n'\
--------------------------------------------------$'\n'\
"Tidy nc-based SOILWAT2"$'\n'\
--------------------------------------------------

res=$(make CPPFLAGS='-DSWNC -DSWDEBUG' tidy-bin 2>/dev/null)
status=$?
process_clangtidy_results $status "$res"


echo $'\n'\
--------------------------------------------------$'\n'\
"Tidy SOILWAT2 library for rSOILWAT2"$'\n'\
--------------------------------------------------

res=$(make CPPFLAGS='-DRSOILWAT' CFLAGS='-Iexternal/Rmock' tidy-bin 2>/dev/null)
status=$?
process_clangtidy_results $status "$res"


echo $'\n'\
--------------------------------------------------$'\n'\
"Tidy SOILWAT2 library for STEPWAT2"$'\n'\
--------------------------------------------------

res=$(make CPPFLAGS='-DSTEPWAT' tidy-bin 2>/dev/null)
status=$?
process_clangtidy_results $status "$res"


echo $'\n'\
--------------------------------------------------$'\n'\
"Tidy SOILWAT2 tests with all extra flags"$'\n'\
--------------------------------------------------

res=$(make CPPFLAGS='-DSW2_SpinupEvaluation -DSW2_PET_Test__petfunc_by_temps -DSW2_SolarPosition_Test__hourangles_by_lat_and_doy -DSW2_SolarPosition_Test__hourangles_by_lats' tidy-test 2>/dev/null)
status=$?
process_clangtidy_results $status "$res"


#-------------------------------------------------------------------------------
