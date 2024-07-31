#!/bin/bash


#-------------------------------------------------------------------------------
# Check text-based SOILWAT2
make CPPFLAGS='-DSWDEBUG' tidy-bin 2>/dev/null


#-------------------------------------------------------------------------------
# Check nc-based SOILWAT2
make CPPFLAGS='-DSWNETCDF -DSWUDUNITS -DSWDEBUG' tidy-bin 2>/dev/null


#-------------------------------------------------------------------------------
# Check SOILWAT2 library for rSOILWAT2
make CPPFLAGS='-DRSOILWAT' CFLAGS='-Iexternal/Rmock' tidy-bin 2>/dev/null


#-------------------------------------------------------------------------------
# Check SOILWAT2 library for STEPWAT2
make CPPFLAGS='-DSTEPWAT' tidy-bin 2>/dev/null


#-------------------------------------------------------------------------------
# Check SOILWAT2 tests with all extra flags
make CPPFLAGS='-DSW2_SpinupEvaluation -DSW2_PET_Test__petfunc_by_temps -DSW2_SolarPosition_Test__hourangles_by_lat_and_doy -DSW2_SolarPosition_Test__hourangles_by_lats' tidy-test 2>/dev/null


#-------------------------------------------------------------------------------
