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

    RealD res = SW_VWCBulkRes(fractionGravel, sand, clay, porosity);
    // when clay > .6, we expect res == SW_MISSING since this isn't within reasonable
    // range
    EXPECT_DOUBLE_EQ(res, SW_MISSING);

    // Reset to previous global states
    Reset_SOILWAT2_after_UnitTest();

    clay = .5;
    sand = .04;

    res = SW_VWCBulkRes(fractionGravel, sand, clay, porosity);
    // when sand < .05, we expect res == SW_MISSING since this isn't within reasonable
    // range
    EXPECT_DOUBLE_EQ(res, SW_MISSING);
    // Reset to previous global states
    Reset_SOILWAT2_after_UnitTest();

    sand = .4;
    porosity = .4;

    res = SW_VWCBulkRes(fractionGravel, sand, clay, porosity);
    // when sand == .4, clay == .5, porosity == .4 and fractionGravel ==.1,
    // we expect res == .088373829599999967
    EXPECT_DOUBLE_EQ(res, .088373829599999967);
    // Reset to previous global states
    Reset_SOILWAT2_after_UnitTest();

    porosity = .1;

    res = SW_VWCBulkRes(fractionGravel, sand, clay, porosity);
    // when sand == .4, clay == .5, porosity == .1 and fractionGravel ==.1,
    // we expect res == 0
    EXPECT_DOUBLE_EQ(res, 0);
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

    SW_SWC_adjust_snow(temp_min, temp_max, ppt, &rain, &snow, &snowmelt);
    // when average temperature >= SW_Site.TminAccu2, we expect rain == ppt
    EXPECT_EQ(rain, 1);
    // when average temperature >= SW_Site.TminAccu2, we expect snow == 0
    EXPECT_EQ(snow, 0);
    // when temp_snow <= SW_Site.TmaxCrit, we expect snowmelt == 0
    EXPECT_EQ(snowmelt, 0);
    // Reset to previous global states
    Reset_SOILWAT2_after_UnitTest();

    SW_Site.TminAccu2 = 6;

    SW_SWC_adjust_snow(temp_min, temp_max, ppt, &rain, &snow, &snowmelt);
    // when average temperature < SW_Site.TminAccu2, we expect rain == 0
    EXPECT_EQ(rain, 0);
    // when average temperature < SW_Site.TminAccu2, we expect snow == ppt
    EXPECT_EQ(snow, 1);
    // when temp_snow > SW_Site.TmaxCrit, we expect snowmelt == fmax(0, *snowpack - *snowmelt )
    EXPECT_EQ(snowmelt, 0);
    // Reset to previous global states
    Reset_SOILWAT2_after_UnitTest();

    temp_max = 22;

    SW_SWC_adjust_snow(temp_min, temp_max, ppt, &rain, &snow, &snowmelt);
    // when average temperature >= SW_Site.TminAccu2, we expect rain == ppt
    EXPECT_EQ(rain, 1);
    // when average temperature >= SW_Site.TminAccu2, we expect snow == 0
    EXPECT_EQ(snow, 0);
    // when temp_snow > SW_Site.TmaxCrit, we expect snowmelt == 0
    EXPECT_EQ(snowmelt, 0);
  }

  // Test the 'SW_SoilWater' function 'SW_SWCbulk2SWPmatric'
  TEST(SWSoilWaterTest, SWSWCbulk2SWPmatric){
    RealD fractionGravel = 0.2;
    RealD swcBulk = 0;
    LyrIndex n = 1;

    RealD res = SW_SWCbulk2SWPmatric(fractionGravel, swcBulk, n);
    // when swc is 0, we expect res == 0
    EXPECT_EQ(res, 0.0);
    Reset_SOILWAT2_after_UnitTest();

    swcBulk = SW_MISSING;

    res = SW_SWCbulk2SWPmatric(fractionGravel, swcBulk, n);
    // when swc is SW_MISSING, we expect res == 0
    EXPECT_EQ(res, 0.0);
    Reset_SOILWAT2_after_UnitTest();

    // set variables for clean test
    swcBulk = 4;
    SW_Site.lyr[n] -> width = 1;
    SW_Site.lyr[n] -> psisMatric = 1;
    SW_Site.lyr[n] -> thetasMatric = 1;
    SW_Site.lyr[n] -> bMatric = 1;

    res = SW_SWCbulk2SWPmatric(fractionGravel, swcBulk, n);
    RealD actualExpectDiff = fabs(res - .00013310902);
    // test main function calculations of swp
    EXPECT_LT(actualExpectDiff, .0002);
    // Reset to previous global states
    Reset_SOILWAT2_after_UnitTest();

    fractionGravel = 1;

    res = SW_SWCbulk2SWPmatric(fractionGravel, swcBulk, n);
    // when fractionGravel == 1, we expect the main equation in
    // the function to not work correctly and thus return INFINITY
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

    // when swcBulk < 0, we expect the program to fail and write to log
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

    // set gravel fractions on the interval [.0, .8], step .05
    for (i = 0; i <= 16; i++){
      fractionGravel = i / 20.;
      tExpect = p * (1 - fractionGravel);

      t = SW_SWPmatric2VWCBulk(fractionGravel, swpMatric, n);
      actualExpectDiff = fabs(t - tExpect);

      // when fractionGravel is between [.0, .8], we expect t = p * (1 - fractionGravel)
      EXPECT_LT(actualExpectDiff, 0.0000001);

    }
    Reset_SOILWAT2_after_UnitTest();

    fractionGravel = 1;

    t = SW_SWPmatric2VWCBulk(fractionGravel, swpMatric, n);
    // when fractionGravel is 1, we expect t == 0
    EXPECT_EQ(t, 0);
    // Reset to previous global states
    Reset_SOILWAT2_after_UnitTest();
  }
}
