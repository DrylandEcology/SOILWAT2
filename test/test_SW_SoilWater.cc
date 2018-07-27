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
  float tol = 1e-6;

  // Test the 'SW_SoilWater' function 'SW_VWCBulkRes'
  TEST(SWSoilWaterTest, SWVWCBulkRes){
    //declare mock INPUTS
    RealD fractionGravel = .1, clay = .7, sand = .2, porosity = 1;

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
  }

  // Test the 'SW_SoilWater' function 'SW_SWCbulk2SWPmatric'
  TEST(SWSoilWaterTest, SWSWCbulk2SWPmatric){
    RealD fractionGravel = 0.2, swcBulk = 0;
    LyrIndex n = 1;

    // test 0 for swc
    RealD res = SW_SWCbulk2SWPmatric(fractionGravel, swcBulk, n);
    EXPECT_EQ(res, 0.0);
    Reset_SOILWAT2_after_UnitTest();

    // test missing for swc
    swcBulk = 999;

    res = SW_SWCbulk2SWPmatric(fractionGravel, swcBulk, n);
    EXPECT_EQ(res, 0.0);

    // test swp val math
    swcBulk = 4;
    SW_Site.lyr[n] -> width = 1;
    SW_Site.lyr[n] -> psisMatric = 1;
    SW_Site.lyr[n] -> thetasMatric = 1;
    SW_Site.lyr[n] -> bMatric = 1;

    res = SW_SWCbulk2SWPmatric(fractionGravel, swcBulk, n);
    RealD resExpect = .00013310902; // did math by hand to get this value
    RealD actualExpectDiff = fabs(res - resExpect);
    EXPECT_LT(actualExpectDiff, .0002);
    // Reset to previous global states
    Reset_SOILWAT2_after_UnitTest();
  }

  // Test the 'SW_SoilWater' function 'SW_SWPmatric2VWCBulk'
  TEST(SWSoilWaterTest, SWSWPmatric2VWCBulk){
    // set up mock variables
    RealD p, tExpect, t, fractionGravel;
    RealD swpMatric = 15.0, psisMatric = 18.608013, binverseMatric = 0.188608,
    thetaMatric = 41.37;
    int i;
    LyrIndex n = 0;
    SW_Site.lyr[n]->thetasMatric = thetaMatric;
    SW_Site.lyr[n]->psisMatric = psisMatric;
    SW_Site.lyr[n]->bMatric = binverseMatric;
    p = 0.11656662532982573; // done by hand

    // run tests for gravel fractions on the interval [.0, .8], step .05
    for (i = 0; i <= 16; i++){
      fractionGravel = i / 20.;
      tExpect = p * (1 - fractionGravel);

      t = SW_SWPmatric2VWCBulk(fractionGravel, swpMatric, n);
      // Tolerance for error since division with RealD introcuces some error
      EXPECT_NEAR(t, tExpect, tol);
    }
    // Reset to previous global states
    Reset_SOILWAT2_after_UnitTest();

    // test for when fractionGravel is 1, should return 0
    fractionGravel = 1;
    t = SW_SWPmatric2VWCBulk(fractionGravel, swpMatric, n);
    EXPECT_EQ(t, 0);
    // Reset to previous global states
    Reset_SOILWAT2_after_UnitTest();
  }
}
