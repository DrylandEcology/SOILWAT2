#include "gtest/gtest.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "include/generic.h"
#include "include/SW_Sky.h"
#include "include/filefuncs.h"
#include "include/myMemory.h"
#include "include/SW_Defines.h"
#include "include/SW_Files.h"
#include "include/SW_Model.h"
#include "include/SW_Site.h"
#include "include/SW_SoilWater.h"
#include "include/SW_VegProd.h"
#include "include/SW_Site.h"
#include "include/SW_Flow_lib.h"
#include "include/Times.h"
#include "include/SW_Weather.h"
#include "tests/gtests/sw_testhelpers.h"

namespace{
  TEST(TimesTest, LeapYear) {
    unsigned int k, lpadd,
      years[] = {1900, 1980, 1981, 2000}; // noleap, leap, noleap, leap years

    Bool kleap,
      isleap[] = {swFALSE, swTRUE, swFALSE, swTRUE};

    // Loop through years and tests
    for (k = 0; k < length(years); k++) {
      Time_new_year(years[k]);
      kleap = isleapyear(years[k]);

      lpadd = kleap ? 1 : 0;

      EXPECT_EQ(kleap, isleap[k]);
      EXPECT_EQ(Time_days_in_month(Feb), 28 + lpadd);
      EXPECT_EQ(Time_get_lastdoy_y(years[k]), 365 + lpadd);

      EXPECT_EQ(doy2month(1), Jan); // first day of January
      EXPECT_EQ(doy2month(59 + lpadd), Feb); // last day of February
      EXPECT_EQ(doy2month(60 + lpadd), Mar); // first day of March
      EXPECT_EQ(doy2month(365 + lpadd), Dec); // last day of December

      EXPECT_EQ(doy2mday(1), 1); // first day of January
      EXPECT_EQ(doy2mday(59 + lpadd), 28 + lpadd); // last day of February
      EXPECT_EQ(doy2mday(60 + lpadd), 1); // first day of March
      EXPECT_EQ(doy2mday(365 + lpadd), 31); // last day of December

      EXPECT_EQ(doy2week(1), 0); // first day of first (base0) 7-day period
      EXPECT_EQ(doy2week(7), 0); // last day of first 7-day period
      EXPECT_EQ(doy2week(8), 1); // first day of second 7-day period
      EXPECT_EQ(doy2week(365 + lpadd), 52);
    }
  }


  // Test the 'Times.c' function 'xintpl_monthlyValues'
  double valXd(double v1, double v2, int sign, int mday, int delta_days) {
    return v1 + (v2 - v1) * sign * (mday - 15.) / delta_days;
  }

  TEST(TimesTest, InterpolateMonthlyValues) {
    // point to the structure that contains cloud coverage monthly values
    SW_SKY SW_Sky;
    SW_SKY *xintpl = &SW_Sky;
    SW_WEATHER_HIST xintpl_weather[1][1];
    Bool interpAsBase1 = swFALSE;

    unsigned int i, k, doy, lpadd,
      years[] = {1980, 1981}; // leap year, non-leap year

    Bool isMon1;

    // Loop through years and tests
    for (k = 0; k < length(years); k++) {
      Time_new_year(years[k]);
      lpadd = isleapyear(years[k]) ? 1 : 0;

      // Test: all monthlyValues equal to 10
      //   (not affected by leap/nonleap yrs)
      for (i = 0; i < length(xintpl -> cloudcov); i++) {
        xintpl -> cloudcov[i] = 10;
      }
        xintpl_weather[0] -> cloudcov_daily[0] = SW_MISSING;

      interpolate_monthlyValues(xintpl->cloudcov, interpAsBase1,
                                xintpl_weather[0] -> cloudcov_daily);

      // value for daily index 0 is 10 to make sure base0 is working correctly
      EXPECT_NEAR(xintpl_weather[0] -> cloudcov_daily[0], 10.0, tol9);

      // Expect all xintpld values to be the same (constant input)
      for (doy = 0; doy < Time_get_lastdoy_y(years[k]); doy++) {
        EXPECT_NEAR(xintpl_weather[0] -> cloudcov_daily[doy], 10.0, tol9);
      }

      // Zero the first value in "cloudcov_daily" for testing on base1 interpolation
      xintpl_weather[0] -> cloudcov_daily[0] = 0.;

      interpolate_monthlyValues(xintpl->cloudcov, swTRUE, xintpl_weather[0] -> cloudcov_daily);

      // Value for daily index 0 is unchanged because we use here a base1 index
      EXPECT_NEAR(xintpl_weather[0] -> cloudcov_daily[0], 0, tol9);
      EXPECT_NEAR(xintpl_weather[0] -> cloudcov_daily[1], 10., tol9);

      // Test: all monthlyValues equal to 10 except December and March are 20
      //   (affected by leap/nonleap yrs)
      xintpl -> cloudcov[Mar] = 20;
      xintpl -> cloudcov[Dec] = 20;

      interpolate_monthlyValues(xintpl -> cloudcov, interpAsBase1,
                                xintpl_weather[0] -> cloudcov_daily);

      /* for (doy = 1; doy <= Time_get_lastdoy_y(years[k]); doy++) {
        printf(
          "yr=%d/doy=%d/mday=%d: val=%.6f\n",
          years[k], doy, doy2mday(doy),
          xintpl -> cloudcov_daily[doy]
        );
      } */


      // value for daily index 0 is ~14.5161 to make sure base0 is working correctly
      EXPECT_NEAR(xintpl_weather[0] -> cloudcov_daily[0], 14.516129032, tol9);

      // Expect mid-Nov to mid-Jan and mid-Feb to mid-Apr values to vary,
      // all other are the same

      // Expect Jan 1 to Jan 15 to vary
      for (doy = 0; doy < 15; doy++) {
        EXPECT_NEAR(
          xintpl_weather[0] -> cloudcov_daily[doy],
          valXd(10, 20, -1, doy2mday(doy + 1), 31),
          tol9
        );
      }

      // Expect Jan 15 to Feb 15 to have same values as the constant input
      for (doy = 14; doy < 45; doy++) {
        EXPECT_NEAR(xintpl_weather[0] -> cloudcov_daily[doy], 10.0, tol9);
      }

      // Expect Feb 16 to March 15 to vary (account for leap years)
      for (doy = 45; doy < 74 + lpadd; doy++) {
        isMon1 = (Bool)(doy <= 58 + lpadd);
        EXPECT_NEAR(
          xintpl_weather[0] -> cloudcov_daily[doy],
          valXd(
            isMon1 ? 10 : 20,
            isMon1 ? 20 : 10,
            isMon1 ? 1 : -1,
            doy2mday(doy + 1),
            28 + lpadd
          ),
          tol9
        ) << "year = " << years[k] << " doy = " << doy << " mday = " << doy2mday(doy + 1);
      }

      // Expect Apr 15 to Nov 15 to have same values as the constant input
      for (doy = 104 + lpadd; doy < 319 + lpadd; doy++) {
        EXPECT_NEAR(xintpl_weather[0] -> cloudcov_daily[doy], 10.0, tol9);
      }

      // Expect Dec 1 to Dec 31 to vary
      for (doy = 335 + lpadd; doy < 365 + lpadd; doy++) {
        isMon1 = (Bool)(doy < 349 + lpadd);
        EXPECT_NEAR(
          xintpl_weather[0] -> cloudcov_daily[doy],
          valXd(
            20, // Dec value
            10, // Nov or Jan value
            isMon1 ? -1 : 1,
            doy2mday(doy + 1),
            isMon1 ? 30 : 31
          ),
          tol9
        ) << "year = " << years[k] << " doy = " << doy << " mday = " << doy2mday(doy + 1);
      }


      // Reset to previous global states
      Reset_SOILWAT2_after_UnitTest();
    }
  }
}
