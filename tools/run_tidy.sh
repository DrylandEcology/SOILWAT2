#!/bin/bash


#-------------------------------------------------------------------------------
# Check text-based SOILWAT2
make CPPFLAGS='-DSWDEBUG' tidy-bin


#-------------------------------------------------------------------------------
# Check nc-based SOILWAT2
make CPPFLAGS='-DSWNETCDF -DSWUDUNITS -DSWDEBUG' tidy-bin


#-------------------------------------------------------------------------------
# Check SOILWAT2 library for rSOILWAT2
make CPPFLAGS='-DRSOILWAT' CFLAGS='-Iexternal/Rmock' tidy-bin


#-------------------------------------------------------------------------------
# Check SOILWAT2 library for STEPWAT2
make CPPFLAGS='-DSTEPWAT' tidy-bin


#-------------------------------------------------------------------------------
