#include "gtest/gtest.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "../generic.h"
#include "../SW_Sky.h"
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
#include "../Times.h"
#include "sw_testhelpers.h"

namespace{
  // Test the 'Times.c' function 'interpolate_monthlyValues'
  TEST(TimesTest, interpolateMonthlyValues){
    SW_SKY SW_Sky;
    SW_SKY *interpolate = &SW_Sky;

    unsigned int i;

    // function with monthlyValues all 10
    for (i = 0; i < length(interpolate -> cloudcov); i++){
      interpolate -> cloudcov[i] = 10;
    }
    interpolate_monthlyValues(interpolate -> cloudcov, interpolate -> cloudcov_daily);

    // test final conditional
    EXPECT_EQ(interpolate -> cloudcov_daily[1], 10.0);
    // should always be zero, regardless of input since dailyValues is never
    // changed
    EXPECT_EQ(interpolate -> cloudcov_daily[0], 0.0);
    // test top conditional
    EXPECT_EQ(interpolate -> cloudcov_daily[15], 10.0);
    // test middle conditional
    EXPECT_EQ(interpolate -> cloudcov_daily[16], 10.0);

    // Reset to previous global states
    Reset_SOILWAT2_after_UnitTest();
  }
}
