#!/bin/bash


#-------------------------------------------------------------------------------
# Our goal is to include headers only if actually used, and
# include all headers that are used.
#-------------------------------------------------------------------------------


#-------------------------------------------------------------------------------
#--- Ignore the following messages reported by `include-what-you-use` ----------
#
# src/generic.c should add these lines:
# #include <_ctype.h>              // for isspace, tolower, toupper
#
# src/generic.c should remove these lines:
# - #include <ctype.h>  // lines 14-14
#
#
# src/filefuncs.c should add these lines:
# #include <__stdarg_va_arg.h>         // for va_end, va_start
#
# src/filefuncs.c should remove these lines:
# - #include <stdarg.h>  // lines 11-11
#
#
# src/SW_netCDF.c should add these lines:
# #include "converter.h"                   // for cv_convert_doubles, cv_free
#
#
# include/generic.h should remove these lines:
# - #include <R.h>  // lines 68-68
#
#-------------------------------------------------------------------------------


#-------------------------------------------------------------------------------
# Check text-based SOILWAT2
make clean
make -k CC=include-what-you-use CFLAGS="-Xiwyu --error_always"

# Check text-based SOILWAT2 with debug flag
make clean
make -k CC=include-what-you-use CPPFLAGS="-DSWDEBUG" CFLAGS="-Xiwyu --error_always"

# Check text-based SOILWAT2 tests
make clean
make -k CXX=include-what-you-use CXXFLAGS="-Xiwyu --error_always" test


#-------------------------------------------------------------------------------
# Check nc-based SOILWAT2
make clean
make -k CC=include-what-you-use CPPFLAGS="-DSWNETCDF -DSWUDUNITS" CFLAGS="-Xiwyu --error_always"

# Check nc-based SOILWAT2 with debug flag
make clean
make -k CC=include-what-you-use CPPFLAGS="-DSWNETCDF -DSWUDUNITS -DSWDEBUG" CFLAGS="-Xiwyu --error_always"

# Check nc-based SOILWAT2 tests
make clean
make -k CXX=include-what-you-use CPPFLAGS="-DSWNETCDF -DSWUDUNITS" CXXFLAGS="-Xiwyu --error_always" test


#-------------------------------------------------------------------------------
# Check SOILWAT2 library for rSOILWAT2
make clean
make -k CC=include-what-you-use CPPFLAGS="-DRSOILWAT" CFLAGS="-Xiwyu --error_always -Iexternal/Rmock" libr

#-------------------------------------------------------------------------------
# Check SOILWAT2 library for STEPWAT2
make clean
make -k CC=include-what-you-use CPPFLAGS="-DSTEPWAT" CFLAGS="-Xiwyu --error_always" lib


#-------------------------------------------------------------------------------
# Check SOILWAT2 with extra flags
make clean
make -k CXX=include-what-you-use CPPFLAGS="-DSW2_SpinupEvaluation" CXXFLAGS="-Xiwyu --error_always" test

make clean
make -k CXX=include-what-you-use CPPFLAGS="-DSW2_PET_Test__petfunc_by_temps" CXXFLAGS="-Xiwyu --error_always" test

make clean
make -k CXX=include-what-you-use CPPFLAGS="-DSW2_SolarPosition_Test__hourangles_by_lat_and_doy" CXXFLAGS="-Xiwyu --error_always" test

make clean
make -k CXX=include-what-you-use CPPFLAGS="-DSW2_SolarPosition_Test__hourangles_by_lats" CXXFLAGS="-Xiwyu --error_always" test

#-------------------------------------------------------------------------------
