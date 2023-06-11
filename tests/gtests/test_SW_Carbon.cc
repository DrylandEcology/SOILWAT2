#include "gtest/gtest.h"
#include <assert.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <float.h>
#include <math.h>
#include <memory.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include <typeinfo>  // for 'typeid'

#include "include/generic.h"
#include "include/myMemory.h"
#include "include/filefuncs.h"
#include "include/rands.h"
#include "include/Times.h"
#include "include/SW_Defines.h"
#include "include/SW_Times.h"
#include "include/SW_Files.h"
#include "include/SW_Carbon.h"
#include "include/SW_Site.h"
#include "include/SW_VegProd.h"
#include "include/SW_VegEstab.h"
#include "include/SW_Model.h"
#include "include/SW_SoilWater.h"
#include "include/SW_Weather.h"
#include "include/SW_Markov.h"
#include "include/SW_Sky.h"

#include "tests/gtests/sw_testhelpers.h"



namespace {
  // Test the SW_Carbon constructor 'SW_CBN_construct'
  TEST_F(AllTest, CarbonConstructor) {
    int x;

    SW_CBN_construct(&SW_All.Carbon);

    // Test type (and existence)
    EXPECT_EQ(typeid(x), typeid(SW_All.Carbon.use_wue_mult));
    EXPECT_EQ(typeid(x), typeid(SW_All.Carbon.use_bio_mult));
  }


  // Test reading yearly CO2 data from disk file
  TEST_F(AllTest, ReadCarbonInputFile) {
    TimeInt year, simendyr = SW_All.Model.endyr + SW_All.Model.addtl_yr;
    double sum_CO2;

    // Test if CO2-effects are turned off -> no CO2 concentration data are read from file
    SW_CBN_construct(&SW_All.Carbon);
    SW_All.Carbon.use_wue_mult = 0;
    SW_All.Carbon.use_bio_mult = 0;

    SW_CBN_read(&SW_All.Carbon, &SW_All.Model, PathInfo.InFiles, &LogInfo);

    sum_CO2 = 0.;
    for (year = 0; year < MAX_NYEAR; year++) {
      sum_CO2 += SW_All.Carbon.ppm[year];
    }
    EXPECT_DOUBLE_EQ(sum_CO2, 0.);

    // Test if CO2-effects are turned on -> CO2 concentration data are read from file
    SW_CBN_construct(&SW_All.Carbon);
    strcpy(SW_All.Carbon.scenario, "RCP85");
    SW_All.Carbon.use_wue_mult = 1;
    SW_All.Carbon.use_bio_mult = 1;
    SW_All.Model.addtl_yr = 0;

    SW_CBN_read(&SW_All.Carbon, &SW_All.Model, PathInfo.InFiles, &LogInfo);

    for (year = SW_All.Model.startyr + SW_All.Model.addtl_yr; year <= simendyr; year++) {
      EXPECT_GT(SW_All.Carbon.ppm[year], 0.);
    }
  }


  // Test the calculation of CO2-effect multipliers
  TEST_F(AllTest, CO2multipliers) {
    TimeInt year, simendyr = SW_All.Model.endyr + SW_All.Model.addtl_yr;
    int k;

    SW_CBN_construct(&SW_All.Carbon);
    strcpy(SW_All.Carbon.scenario, "RCP85");
    SW_All.Carbon.use_wue_mult = 1;
    SW_All.Carbon.use_bio_mult = 1;
    SW_All.Model.addtl_yr = 0;

    SW_CBN_read(&SW_All.Carbon, &SW_All.Model, PathInfo.InFiles, &LogInfo);
    SW_CBN_init_run(SW_All.VegProd.veg, &SW_All.Model, &SW_All.Carbon, &LogInfo);

    for (year = SW_All.Model.startyr + SW_All.Model.addtl_yr; year <= simendyr; year++) {
      ForEachVegType(k) {
        EXPECT_GT(SW_All.VegProd.veg[k].co2_multipliers[BIO_INDEX][year], 0.);
        EXPECT_GT(SW_All.VegProd.veg[k].co2_multipliers[WUE_INDEX][year], 0.);
      }
    }
  }

} // namespace
