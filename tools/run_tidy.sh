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
