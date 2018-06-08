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
#include "../Times.h"
#include "sw_testhelpers.h"

namespace{
  // Test the 'Times.c' function 'interpolate_monthlyValues'
  TEST(TimesTest, interpolateMonthlyValues){
    TimeInt day = 16;
    // month which day falls under
    TimeInt month = doy2month(day);
    // day in the month the day falls under
    // example: day = 32 means that mday would be 1 for febuary 1st
    TimeInt mday = doy2mday(day);
    double monthlyValues[30];
    double dailyValues[MAX_DAYS + 1];

    unsigned int i;
    // function with monthlyValues all 10
    for (i = 0; i < length(monthlyValues); i++){
      monthlyValues[i] = 10;
    }
    double* res = interpolate_monthlyValues(monthlyValues, dailyValues);
    // test final conditional
    EXPECT_EQ(res[1], 10.0);
    // should always be zero, regardless of input
    EXPECT_EQ(res[0], 0.0);
    // test top conditional
    EXPECT_EQ(res[15], 10.0);
    // test middle conditional
    EXPECT_EQ(res[16], 10.0);

    printf("month, mday: %d, %d\n", month, mday);
    printf("dailyValues[15]: %f", res[15]);
    //printf("dailyValues: ");
    //for(i = 1; i <= MAX_DAYS; i++){
    //  printf("%d, ", doy2mday(i));
    //}
    // Reset to previous global states
    Reset_SOILWAT2_after_UnitTest();
  }
}
