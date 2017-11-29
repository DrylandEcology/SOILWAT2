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

#include "../generic.h"
#include "../myMemory.h"
#include "../filefuncs.h"
#include "../rands.h"
#include "../Times.h"
#include "../SW_Defines.h"
#include "../SW_Times.h"
#include "../SW_Files.h"
#include "../SW_Carbon.h"
#include "../SW_Site.h"
#include "../SW_VegProd.h"
#include "../SW_VegEstab.h"
#include "../SW_Model.h"
#include "../SW_SoilWater.h"
#include "../SW_Weather.h"
#include "../SW_Markov.h"
#include "../SW_Sky.h"


extern SW_CARBON SW_Carbon;
extern SW_MODEL SW_Model;
extern SW_VEGPROD SW_VegProd;



namespace {
  SW_CARBON *c = &SW_Carbon;
  SW_VEGPROD *v  = &SW_VegProd;


  // Test the SW_Carbon constructor 'SW_CBN_construct'
  TEST(CarbonTest, Constructor) {
    int x;

    SW_CBN_construct();

    // Test type (and existence)
    EXPECT_EQ(typeid(x), typeid(c->use_wue_mult));
    EXPECT_EQ(typeid(x), typeid(c->use_bio_mult));
  }


  // Test reading yearly CO2 data from disk file
  TEST(CarbonTest, ReadInputFile) {
    TimeInt year,
      simendyr = SW_Model.endyr + SW_Model.addtl_yr;
    double sum_CO2;

    // Test if CO2-effects are turned off -> no CO2 concentration data are read from file
    SW_CBN_construct();
    c->use_wue_mult = 0;
    c->use_bio_mult = 0;

    SW_CBN_read();

    sum_CO2 = 0.;
    for (year = 0; year < MAX_NYEAR; year++) {
      sum_CO2 += c->ppm[year];
    }
    EXPECT_DOUBLE_EQ(sum_CO2, 0.);

    // Test if CO2-effects are turned on -> CO2 concentration data are read from file
    SW_CBN_construct();
    strcpy(c->scenario, "RCP85");
    c->use_wue_mult = 1;
    c->use_bio_mult = 1;
    SW_Model.addtl_yr = 0;

    SW_CBN_read();

    for (year = SW_Model.startyr + SW_Model.addtl_yr; year <= simendyr; year++) {
      EXPECT_GT(c->ppm[year], 0.);
    }
  }


  // Test the calculation of CO2-effect multipliers
  TEST(CarbonTest, CO2multipliers) {
    TimeInt year,
      simendyr = SW_Model.endyr + SW_Model.addtl_yr;

    SW_CBN_construct();
    strcpy(c->scenario, "RCP85");
    c->use_wue_mult = 1;
    c->use_bio_mult = 1;
    SW_Model.addtl_yr = 0;

    SW_CBN_read();
    calculate_CO2_multipliers();

    for (year = SW_Model.startyr + SW_Model.addtl_yr; year <= simendyr; year++) {
      EXPECT_GT(v->forb.co2_multipliers[BIO_INDEX][year], 0.);
      EXPECT_GT(v->grass.co2_multipliers[BIO_INDEX][year], 0.);
      EXPECT_GT(v->shrub.co2_multipliers[BIO_INDEX][year], 0.);
      EXPECT_GT(v->tree.co2_multipliers[BIO_INDEX][year], 0.);

      EXPECT_GT(v->forb.co2_multipliers[WUE_INDEX][year], 0.);
      EXPECT_GT(v->grass.co2_multipliers[WUE_INDEX][year], 0.);
      EXPECT_GT(v->shrub.co2_multipliers[WUE_INDEX][year], 0.);
      EXPECT_GT(v->tree.co2_multipliers[WUE_INDEX][year], 0.);
    }
  }

} // namespace
