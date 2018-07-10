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
    // point to the structure that contains cloud coverage monthly values
    SW_SKY SW_Sky;
    SW_SKY *interpolate = &SW_Sky;

    unsigned int i;

    // set all monthlyValues all 10
    for (i = 0; i < length(interpolate -> cloudcov); i++){
      interpolate -> cloudcov[i] = 10;
    }
    interpolate -> cloudcov_daily[0] = 0;
    interpolate_monthlyValues(interpolate -> cloudcov, interpolate -> cloudcov_daily);

    // inperpolate_monthlyValues should not change index 0 because we used
    // base1 indices
    EXPECT_DOUBLE_EQ(interpolate -> cloudcov_daily[0], 0);
    EXPECT_DOUBLE_EQ(interpolate -> cloudcov_daily[1], 10.0);
    // test top conditional
    EXPECT_DOUBLE_EQ(interpolate -> cloudcov_daily[14], 10.0);
    // test middle conditional
    EXPECT_DOUBLE_EQ(interpolate -> cloudcov_daily[15], 10.0);
    EXPECT_DOUBLE_EQ(interpolate -> cloudcov_daily[365], 10.0);

    // Reset to previous global states
    Reset_SOILWAT2_after_UnitTest();

    // change first value to 20 and test the changes
    interpolate -> cloudcov[0] = 20;
    interpolate_monthlyValues(interpolate -> cloudcov, interpolate -> cloudcov_daily);
    // calculated by hand
    EXPECT_DOUBLE_EQ(interpolate -> cloudcov_daily[0], 0);
    EXPECT_DOUBLE_EQ(interpolate -> cloudcov_daily[1], 15.483870967741936);
    EXPECT_DOUBLE_EQ(interpolate -> cloudcov_daily[14], 19.6774193548387);
    EXPECT_DOUBLE_EQ(interpolate -> cloudcov_daily[13], 19.354838709677419);
    EXPECT_DOUBLE_EQ(interpolate -> cloudcov_daily[31], 14.838709677419356);
    EXPECT_DOUBLE_EQ(interpolate -> cloudcov_daily[365], 15.161290322580644);
    // test last day on leap year
    EXPECT_DOUBLE_EQ(interpolate -> cloudcov_daily[MAX_DAYS], 15.161290322580644)
    // change december monthly value to ensure meaningful final interpolation
    interpolate -> cloudcov[11] = 12;
    interpolate_monthlyValues(interpolate -> cloudcov, interpolate -> cloudcov_daily);
    EXPECT_DOUBLE_EQ(interpolate -> cloudcov_daily[365], 16.129032258064516);
    // test last day on leap year
    EXPECT_DOUBLE_EQ(interpolate -> cloudcov_daily[MAX_DAYS], 16.129032258064516);
    // Reset to previous global states
    Reset_SOILWAT2_after_UnitTest();
  }
}
