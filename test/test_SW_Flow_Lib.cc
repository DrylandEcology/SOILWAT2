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


namespace {

  // Test the grass interception function 'grass_intercepted_water'
  TEST(SWFlowTest, GrassInterceptedWater) {

    // declare inputs
    double vegcov;
    double ppt = 5.0, scale = 1.0;
    double pptleft = 5.0, wintgrass = 0.0;
    double a = 0.0182, b = 0.0065, c = 0.0019, d = 0.0054;

    // Test expectation when grass veg cov is zero
    vegcov = 0.0;

    grass_intercepted_water(&pptleft, &wintgrass, ppt, vegcov, scale, a, b, c, d);

    EXPECT_EQ(0, wintgrass);
    EXPECT_EQ(pptleft, ppt);

    // Test expectations when ppt is 0
    ppt = 0.0, vegcov = 5.0;

    grass_intercepted_water(&pptleft, &wintgrass, ppt, vegcov, scale, a, b, c, d);

    EXPECT_EQ(0, wintgrass);
    EXPECT_EQ(pptleft, ppt);


    // Test expectations when there is both grass cover and precipitation
    ppt = 5.0, vegcov = 5.0;

    grass_intercepted_water(&pptleft, &wintgrass, ppt, vegcov, scale, a, b, c, d);

    EXPECT_GT(wintgrass, 0); // interception by grass should be greater than 0
    EXPECT_LE(wintgrass, MAX_WINTSTCR); // interception by grass should be less than or equal to MAX_WINTSTCR (vegcov * .1)
    EXPECT_LE(wintgrass, ppt); // interception by grass should be less than or equal to ppt
    EXPECT_GE(pptleft, 0); // The pptleft (for soil) should be greater than or equal to 0

    // Reset to previous global state
    Reset_SOILWAT2_after_UnitTest();
}

// Test infiltration under high water function, 'infiltrate_water_high'
  TEST(SWFlowTest, InfiltrateWaterHigh){

    // declare inputs
    int MIN_LAYERS = 0;
    double pptleft = 3.0, standingWater, drainout;

    // ***** Tests when nlyrs = 1 ***** //
    ///  provide inputs
    unsigned int nlyrs = 1;
    double swc[MIN_LAYERS] , swcfc[MIN_LAYERS], swcsat[MIN_LAYERS], impermeability[MIN_LAYERS], drain[MIN_LAYERS];
    swc[MIN_LAYERS] = 0.8, swcfc[MIN_LAYERS] = 1.1, swcsat[MIN_LAYERS] = 1.7, impermeability[MIN_LAYERS] = 0;

    infiltrate_water_high(swc, drain, &drainout, pptleft, nlyrs, swcfc, swcsat,
          impermeability, &standingWater);

    EXPECT_GE(drain[MIN_LAYERS], 0); // drainage should be greater than or equal to 0 when soil layers are 1
    EXPECT_LE(swc[MIN_LAYERS], swcsat[MIN_LAYERS]); // swc should be less than or equal to swcsat
    EXPECT_EQ(drainout, drain[MIN_LAYERS]); // drainout and drain should be equal when we have one layer

    /// Test when impermeability is greater than 0
    impermeability[0] = 1;

    infiltrate_water_high(swc, drain, &drainout, pptleft, nlyrs, swcfc, swcsat,
          impermeability, &standingWater);

    EXPECT_EQ(0, drain[MIN_LAYERS]);

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
     swcsat2[i] = swcfc2[i] + .1;
     impermeability2[i] = 0.0;
    }

    infiltrate_water_high(swc2, drain2, &drainout, pptleft, nlyrs, swcfc2, swcsat2,
          impermeability2, &standingWater);

    EXPECT_EQ(drainout, drain2[24]); // drainout and drain should be equal in the last layer

    for (i = 0; i < MAX_LAYERS; i++) {
      EXPECT_LE(swc2[i], swcsat2[i]); // swc should be less than or equal to swcsat
      EXPECT_GE(drain[i], 0); // drainage should be greater than or equal to 0 when soil layers are 1
          }

    /// Test when impermeability is greater than 0
    double impermeability3[nlyrs];

    for (i = 0; i < MAX_LAYERS; i++) {
      impermeability3[i] = 1.0;
    }

    infiltrate_water_high(swc2, drain2, &drainout, pptleft, nlyrs, swcfc2, swcsat2,
               impermeability3, &standingWater);

    for (i = 0; i < MAX_LAYERS; i++) {
      EXPECT_EQ(0, drain2[i]);
    }

    // Reset to previous global states
    Reset_SOILWAT2_after_UnitTest();
  }

}
