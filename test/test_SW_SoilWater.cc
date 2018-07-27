#include "gtest/gtest.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "../generic.h"
#include "../filefuncs.h"
#include "../myMemory.h"
#include "../SW_Defines.h"
#include "../SW_Files.h"
#include "../SW_Model.h"
#include "../SW_Site.h"
#include "../SW_SoilWater.h"
#include "../SW_VegProd.h"
#include "../SW_Site.h"
#include "../SW_Flow_lib.h"
#include "sw_testhelpers.h"

extern SW_MODEL SW_Model;
extern SW_SITE SW_Site;
extern SW_VEGPROD SW_VegProd;

namespace{
  // Test the 'SW_SoilWater' function 'SW_VWCBulkRes'
  TEST(SWSoilWaterTest, SWVWCBulkRes){
    //declare mock INPUTS
    RealD fractionGravel = .1;
    RealD clay = .7;
    RealD sand = .2;
    RealD porosity = 1;

    // test clay > .6, this is not within accepted values so it should return
    // SW_MISSING
    RealD res = SW_VWCBulkRes(fractionGravel, sand, clay, porosity);
    EXPECT_DOUBLE_EQ(res, SW_MISSING);

    // Reset to previous global states
    Reset_SOILWAT2_after_UnitTest();

    clay = .5;
    sand = .04;

    // test sand < .05, this is not within accepted values so it should return
    // SW_MISSING
    res = SW_VWCBulkRes(fractionGravel, sand, clay, porosity);
    EXPECT_DOUBLE_EQ(res, SW_MISSING);
    // Reset to previous global states
    Reset_SOILWAT2_after_UnitTest();
  }

  // Test the 'SW_SoilWater' function 'SW_SWC_adjust_snow'
  TEST(SWSoilWaterTest, SWSWCSdjustSnow){
    // setup mock variables
    SW_Site.TminAccu2 = 0;
    SW_Model.doy = 1;
    SW_Site.RmeltMax = 1;
    SW_Site.RmeltMin = 0;
    SW_Site.lambdasnow = .1;
    SW_Site.TmaxCrit = 1;

    RealD temp_min = 0, temp_max = 10, ppt = 1, rain = 1.5, snow = 1.5,
    snowmelt = 1.2;

    // Since TminAccu2 < temp_ave, we expect SnowAccu to be 0 and thus
    // rain is ppt - SnowAccu
    SW_SWC_adjust_snow(temp_min, temp_max, ppt, &rain, &snow, &snowmelt);
    EXPECT_EQ(rain, 1);
    EXPECT_EQ(snow, 0);
    EXPECT_EQ(snowmelt, 0);
    // Reset to previous global states
    Reset_SOILWAT2_after_UnitTest();

    // test TminAccu2 >= temp_ave
    SW_Site.TminAccu2 = 6;

    SW_SWC_adjust_snow(temp_min, temp_max, ppt, &rain, &snow, &snowmelt);
    EXPECT_EQ(rain, 0);
    EXPECT_EQ(snow, 1);
    EXPECT_EQ(snowmelt, 0);
    // Reset to previous global states
    Reset_SOILWAT2_after_UnitTest();

    // test temp_snow > TmaxCrit
    temp_max = 22;

    SW_SWC_adjust_snow(temp_min, temp_max, ppt, &rain, &snow, &snowmelt);
    EXPECT_EQ(rain, 1);
    EXPECT_EQ(snow, 0);
    EXPECT_EQ(snowmelt, 0);
  }

  // Test the 'SW_SoilWater' function 'SW_SWCbulk2SWPmatric'
  TEST(SWSoilWaterTest, SWSWCbulk2SWPmatric){
    RealD fractionGravel = 0.2;
    RealD swcBulk = 0;
    LyrIndex n = 1;

    // test missing and 0 for swc
    RealD res = SW_SWCbulk2SWPmatric(fractionGravel, swcBulk, n);
    EXPECT_EQ(res, 0.0);
    Reset_SOILWAT2_after_UnitTest();

    swcBulk = 999;

    res = SW_SWCbulk2SWPmatric(fractionGravel, swcBulk, n);
    EXPECT_EQ(res, 0.0);
    Reset_SOILWAT2_after_UnitTest();

    // test swp val
    swcBulk = 4;
    SW_Site.lyr[n] -> width = 1;
    SW_Site.lyr[n] -> psisMatric = 1;
    SW_Site.lyr[n] -> thetasMatric = 1;
    SW_Site.lyr[n] -> bMatric = 1;

    res = SW_SWCbulk2SWPmatric(fractionGravel, swcBulk, n);
    RealD actualExpectDiff = fabs(res - .00013310902);
    EXPECT_LT(actualExpectDiff, .0002);
    // Reset to previous global states
    Reset_SOILWAT2_after_UnitTest();

    // test fractionGravel == 1, this should cause the main equation in the function
    // to not work
    fractionGravel = 1;

    res = SW_SWCbulk2SWPmatric(fractionGravel, swcBulk, n);
    EXPECT_DOUBLE_EQ(res, INFINITY);
    // Reset to previous global states
    Reset_SOILWAT2_after_UnitTest();
  }

  TEST(SWSoilWaterDeathTest, SWSWCbulk2SWPmatricDeathTest) {
    RealD fractionGravel = 0.2, swcBulk = -1;
    LyrIndex n = 1;
    SW_Site.lyr[n] -> width = 1;
    SW_Site.lyr[n] -> psisMatric = 1;
    SW_Site.lyr[n] -> thetasMatric = 1;
    SW_Site.lyr[n] -> bMatric = 1;

    // test swcBulk < 0, should cause the program to fail and write to log
    EXPECT_DEATH_IF_SUPPORTED(SW_SWCbulk2SWPmatric(fractionGravel, swcBulk, n),
    "@ generic.c LogError");
    // Reset to previous global states
    Reset_SOILWAT2_after_UnitTest();
  }

  // Test the 'SW_SoilWater' function 'SW_SWPmatric2VWCBulk'
  TEST(SWSoilWaterTest, SWSWPmatric2VWCBulk){
    // set up mock variables
    RealD fractionGravel = .1, swpMatric = 15.0, p = 0.11656662532982573,
    psisMatric = 18.608013, binverseMatric = 0.188608, thetaMatric = 41.37;
    RealD tExpect, t, actualExpectDiff;
    int i;
    LyrIndex n = 0;
    SW_Site.lyr[n]->thetasMatric = thetaMatric;
    SW_Site.lyr[n]->psisMatric = psisMatric;
    SW_Site.lyr[n]->bMatric = binverseMatric;

    // run tests for gravel fractions on the interval [.0, .8], step .05
    for (i = 0; i <= 16; i++){
      fractionGravel = i / 20.;
      tExpect = p * (1 - fractionGravel);

      t = SW_SWPmatric2VWCBulk(fractionGravel, swpMatric, n);
      actualExpectDiff = fabs(t - tExpect);

      // Tolerance for error since division with RealD introcuces some error
      EXPECT_LT(actualExpectDiff, 0.0000001);

    }
    Reset_SOILWAT2_after_UnitTest();

    // test for when fractionGravel is 1, should return 0
    fractionGravel = 1;

    t = SW_SWPmatric2VWCBulk(fractionGravel, swpMatric, n);
    EXPECT_EQ(t, 0);
    // Reset to previous global states
    Reset_SOILWAT2_after_UnitTest();
  }
}
