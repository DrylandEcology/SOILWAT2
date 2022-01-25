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



namespace{
  // Test the 'SW_SoilWater' function 'SW_VWCBulkRes'
  TEST(SWSoilWaterTest, VWCBulkRes){
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
  TEST(SWSoilWaterTest, SWCdjustSnow){
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
  TEST(SWSoilWaterTest, SWCbulk2SWPmatric){
    // Note: function `SW_SWCbulk2SWPmatric` accesses `SW_Site.lyr[n]`

    RealD tol = 1e-2; // pedotransfer functions are not very exact
    RealD fractionGravel = 0.2;
    RealD swcBulk;
    RealD res;
    RealD help;
    LyrIndex n = 1;

    // when swc is 0, we expect res == 0
    res = SW_SWCbulk2SWPmatric(fractionGravel, 0., n);
    EXPECT_EQ(res, 0.0);

    // when swc is SW_MISSING, we expect res == 0
    res = SW_SWCbulk2SWPmatric(fractionGravel, SW_MISSING, n);
    EXPECT_EQ(res, 0.0);

    // if swc > field capacity, then we expect res < 0.33 bar
    res = SW_SWCbulk2SWPmatric(SW_Site.lyr[n]->fractionVolBulk_gravel,
      SW_Site.lyr[n]->swcBulk_fieldcap + 0.1, n);
    EXPECT_LT(res, 0.33 + tol);

    // if swc = field capacity, then we expect res == 0.33 bar
    res = SW_SWCbulk2SWPmatric(SW_Site.lyr[n]->fractionVolBulk_gravel,
      SW_Site.lyr[n]->swcBulk_fieldcap, n);
    EXPECT_NEAR(res, 0.33, tol);

    // if field capacity > swc > wilting point, then
    // we expect 15 bar > res > 0.33 bar
    swcBulk = (SW_Site.lyr[n]->swcBulk_fieldcap +
      SW_Site.lyr[n]->swcBulk_wiltpt) / 2;
    res = SW_SWCbulk2SWPmatric(SW_Site.lyr[n]->fractionVolBulk_gravel,
      swcBulk, n);
    EXPECT_GT(res, 0.33 - tol);
    EXPECT_LT(res, 15 + tol);

    // if swc = wilting point, then we expect res == 15 bar
    res = SW_SWCbulk2SWPmatric(SW_Site.lyr[n]->fractionVolBulk_gravel,
      SW_Site.lyr[n]->swcBulk_wiltpt, n);
    EXPECT_NEAR(res, 15., tol);

    // if swc < wilting point, then we expect res > 15 bar
    swcBulk = (SW_Site.lyr[n]->swcBulk_wiltpt) / 2;
    res = SW_SWCbulk2SWPmatric(SW_Site.lyr[n]->fractionVolBulk_gravel,
      swcBulk, n);
    EXPECT_GT(res, 15. - tol);


    // ------ meddling with internal value
    // if fractionGravel == 1: no soil volume available to hold any soil water
    // this would also lead to theta1 == 0 and division by zero
    // this situation does normally not occur because it is
    // checked during input by function `_read_layers`
    // Note: this situation is tested by the death test
    // `SWSWCbulk2SWPmatricDeathTest`: we cannot test it here because the
    // Address Sanitizer would complain with `UndefinedBehaviorSanitizer`
    // see [issue #231](https://github.com/DrylandEcology/SOILWAT2/issues/231)
    // res = SW_SWCbulk2SWPmatric(1., SW_Site.lyr[n]->swcBulk_fieldcap, n);
    // EXPECT_DOUBLE_EQ(res, 0.); // SWP "ought to be" infinity [bar]

    // if theta(sat, matric; Cosby et al. 1984) == 0: would be division by zero
    // this situation does normally not occur because it is
    // checked during input by function `water_eqn`
    help = SW_Site.lyr[n]->thetasMatric;
    SW_Site.lyr[n]->thetasMatric = 0.;
    res = SW_SWCbulk2SWPmatric(SW_Site.lyr[n]->fractionVolBulk_gravel, 0., n);
    EXPECT_DOUBLE_EQ(res, 0.); // SWP "ought to be" infinity [bar]
    SW_Site.lyr[n]->thetasMatric = help;

    // if lyr->width == 0: would be division by zero
    // this situation does normally not occur because it is
    // checked during input by function `_read_layers`
    help = SW_Site.lyr[n]->bMatric;
    SW_Site.lyr[n]->width = 0.;
     res = SW_SWCbulk2SWPmatric(SW_Site.lyr[n]->fractionVolBulk_gravel, 0., n);
    EXPECT_DOUBLE_EQ(res, 0.); // swc < width
    SW_Site.lyr[n]->width = help;


    // No need to reset to previous global states because we didn't change any
    // global states
    // Reset_SOILWAT2_after_UnitTest();
  }

  TEST(SWSoilWaterDeathTest, SWCbulk2SWPmatricDeathTest) {
    LyrIndex n = 1;
    RealD help;

    // we expect fatal errors and write to log under two situations:

    // if swc < 0: water content can physically not be negative
    EXPECT_DEATH_IF_SUPPORTED(
      SW_SWCbulk2SWPmatric(SW_Site.lyr[n]->fractionVolBulk_gravel, -1., n),
      "@ generic.c LogError"
    );

    // if theta1 == 0 (i.e., gravel == 1) && lyr->bMatric == 0:
    // would be division by NaN
    // note: this case is in normally prevented due to checks of inputs by
    // function `water_eqn` for `bMatric` and function `_read_layers` for
    // `gravelFraction`
    help = SW_Site.lyr[n]->bMatric;
    SW_Site.lyr[n]->bMatric = 0.;
    EXPECT_DEATH_IF_SUPPORTED(
      SW_SWCbulk2SWPmatric(1., SW_Site.lyr[n]->swcBulk_fieldcap, n),
      "@ generic.c LogError"
    );
    SW_Site.lyr[n]->bMatric = help;

    // Reset to previous global states
    Reset_SOILWAT2_after_UnitTest();
  }

  // Test the 'SW_SoilWater' function 'SW_SWPmatric2VWCBulk'
  TEST(SWSoilWaterTest, SWPmatric2VWCBulk){
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
      EXPECT_LT(actualExpectDiff, tol6);

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
