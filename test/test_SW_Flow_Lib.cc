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

    double ppt = 5.0, scale = 1.0;
    double pptleft = 5.0, wintgrass = 0.0;
    double a = 0.0182, b = 0.0065, c = 0.0019, d = 0.0054;

    // Test expectation when grass veg cov is zero
    double vegcov = 0.0;

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


}
