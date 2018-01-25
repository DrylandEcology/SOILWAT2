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

#include "../SW_Flow_lib.h"

#include "sw_testhelpers.h"

extern SW_MODEL SW_Model;
extern SW_VEGPROD SW_VegProd;
SW_VEGPROD *v = &SW_VegProd;
int k;


namespace {

  // Test the veg interception function 'veg_intercepted_water'
  TEST(SWFlowTest, VegInterceptedWater) {

    ForEachVegType(k)
    {
    // declare inputs
    double x;
    double ppt = 5.0, scale = 1.0;
    double pptleft = 5.0, wintveg = 0.0;
    double a = v -> veg[k].veg_intPPT_a, b = v -> veg[k].veg_intPPT_b,
    c = v -> veg[k].veg_intPPT_c, d = v -> veg[k].veg_intPPT_d;

    // Test expectation when x("vegcov") is zero
    x = 0.0;

    veg_intercepted_water(&pptleft, &wintveg, ppt, x, scale, a, b, c, d);

    EXPECT_EQ(0, wintveg); // When there is no veg, interception should be 0
    EXPECT_EQ(pptleft, ppt); /* When there is no interception, ppt before interception
    should equal ppt left after interception */

    // Test expectations when ppt is 0
    ppt = 0.0, x = 5.0;

    veg_intercepted_water(&pptleft, &wintveg, ppt, x, scale, a, b, c, d);

    EXPECT_EQ(0, wintveg);  // When there is no ppt, interception should be 0
    EXPECT_EQ(pptleft, ppt); /* When there is no interception, ppt before interception
    should equal ppt left after interception */


    // Test expectations when there is both veg cover and precipitation
    ppt = 5.0, x = 5.0;

    veg_intercepted_water(&pptleft, &wintveg, ppt, x, scale, a, b, c, d);

    EXPECT_GT(wintveg, 0); // interception by veg should be greater than 0
    EXPECT_LE(wintveg, MAX_WINTSTCR(x)); // interception by veg should be less than or equal to MAX_WINTSTCR (vegcov * .1)
    EXPECT_LE(wintveg, ppt); // interception by veg should be less than or equal to ppt
    EXPECT_GE(pptleft, 0); // The pptleft (for soil) should be greater than or equal to 0

    // Reset to previous global state
    Reset_SOILWAT2_after_UnitTest();
  }
}

// Test the litter interception function 'litter_intercepted_water'
TEST(SWFlowTest, LitterInterceptedWater) {

  ForEachVegType(k)
  {
  // declare inputs
  double scale, blitter,pptleft = 5.0;
  double wintlit;
  double a = v->veg[k].litt_intPPT_a, b = v->veg[k].litt_intPPT_b,
  c = v->veg[k].litt_intPPT_c, d = v->veg[k].litt_intPPT_d;

  // Test expectation when scale (cover) is zero
  pptleft = 5.0, scale = 0.0, blitter = 5.0;

  litter_intercepted_water(&pptleft, &wintlit, blitter, scale, a, b, c, d);

  EXPECT_EQ(0, wintlit); // When scale is 0, interception should be 0


  // Test expectations when blitter is 0
  pptleft = 5.0, scale = 0.5, blitter = 0.0;

  litter_intercepted_water(&pptleft, &wintlit, blitter, scale, a, b, c, d);

  EXPECT_EQ(0, wintlit); // When there is no blitter, interception should be 0


  // Test expectations when pptleft is 0
  pptleft = 0.0, scale = 0.5, blitter = 5.0;

  litter_intercepted_water(&pptleft, &wintlit, blitter, scale, a, b, c, d);

  EXPECT_EQ(0, pptleft); // When there is no ppt, pptleft should be 0
  EXPECT_EQ(0, wintlit); // When there is no blitter, interception should be 0


  // Test expectations when there pptleft, scale, and blitter are greater than 0
  pptleft = 5.0, scale = 0.5, blitter = 5.0;

  litter_intercepted_water(&pptleft, &wintlit, blitter, scale, a, b, c, d);

  EXPECT_GT(wintlit, 0); // interception by litter should be greater than 0
  EXPECT_LE(wintlit, pptleft); // interception by lit should be less than or equal to ppt
  EXPECT_LE(wintlit, MAX_WINTLIT); /* interception by lit should be less than
  or equal to MAX_WINTLIT (blitter * .2) */
  EXPECT_GE(pptleft, 0); // The pptleft (for soil) should be greater than or equal to 0

  // Reset to previous global state
  Reset_SOILWAT2_after_UnitTest();
  }
}



// Test infiltration under high water function, 'infiltrate_water_high'
  TEST(SWFlowTest, InfiltrateWaterHigh){

    // declare inputs
    double pptleft = 5.0, standingWater, drainout;

    // ***** Tests when nlyrs = 1 ***** //
    ///  provide inputs
    unsigned int nlyrs = 1;
    double swc[1] = {0.8}, swcfc[1] = {1.1}, swcsat[1] = {1.7}, impermeability[1] = {0.}, drain[1];

    infiltrate_water_high(swc, drain, &drainout, pptleft, nlyrs, swcfc, swcsat,
          impermeability, &standingWater);

    EXPECT_GE(drain[0], 0); // drainage should be greater than or equal to 0 when soil layers are 1
    EXPECT_LE(swc[0], swcsat[0]); // swc should be less than or equal to swcsat
    EXPECT_DOUBLE_EQ(drainout, drain[0]); // drainout and drain should be equal when we have one layer


    /* Test when impermeability is greater than 0 and large precipitation. Also
    set swc to swcsat, so water is not added to the first layer */
    impermeability[0] = 1;
    pptleft = 20.0;
    swc[0] = swcsat[0];

    infiltrate_water_high(swc, drain, &drainout, pptleft, nlyrs, swcfc, swcsat,
          impermeability, &standingWater);

    EXPECT_DOUBLE_EQ(0., drain[0]); //When impermeability is 1, drainage should be 0
    EXPECT_GT(standingWater, 0.); /* When impermeability is 1, standingWater
    should > 0 if ppt > 0 */

    // Reset to previous global states
    Reset_SOILWAT2_after_UnitTest();

    // *****  Test when nlyrs = MAX_LAYERS (SW_Defines.h)  ***** //
    /// generate inputs using a for loop
    int i;
    nlyrs = MAX_LAYERS;
    double swc2[nlyrs], swcfc2[nlyrs], swcsat2[nlyrs], impermeability2[nlyrs], drain2[nlyrs];

    for (i = 0; i < MAX_LAYERS; i++) {
      swc2[i] = RandNorm(1.,0.5);
      swcfc2[i] = RandNorm(1,.5);
      swcsat2[i] = swcfc2[i] + .1; // swcsat will always be greater than swcfc in each layer
      impermeability2[i] = 0.0;
    }

    infiltrate_water_high(swc2, drain2, &drainout, pptleft, nlyrs, swcfc2, swcsat2,
          impermeability2, &standingWater);

    EXPECT_EQ(drainout, drain2[MAX_LAYERS - 1]); /* drainout and drain should be
    equal in the last layer */

    for (i = 0; i < MAX_LAYERS; i++) {
      EXPECT_LE(swc2[i], swcsat2[i]); // swc should be less than or equal to swcsat
      EXPECT_GE(drain[i], -1./100000000.); /* drainage should be greater than or
      equal to 0 or a very small value like 0 */
    }

    /// Test when impermeability is greater than 0 and large precipitation
    double impermeability3[nlyrs];
    pptleft = 20.0;

    for (i = 0; i < MAX_LAYERS; i++) {
      impermeability3[i] = 1.0;
    }

    infiltrate_water_high(swc2, drain2, &drainout, pptleft, nlyrs, swcfc2, swcsat2,
               impermeability3, &standingWater);

    EXPECT_GT(standingWater, 0.); //When impermeability is 1, standingWater should > 0 if ppt > 0

    for (i = 0; i < MAX_LAYERS; i++) {
      EXPECT_EQ(0, drain2[i]); //When impermeability is 1, drainage should be 0
    }

    // Reset to previous global states
    Reset_SOILWAT2_after_UnitTest();
  }

}
