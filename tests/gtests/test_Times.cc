#include "include/generic.h"             // for Bool, swFALSE, swTRUE, RealD
#include "include/SW_datastructs.h"      // for LOG_INFO, SW_WALLTIME
#include "include/SW_Defines.h"          // for MAX_MONTHS, TimeInt, MAX_DAYS
#include "include/SW_Main_lib.h"         // for sw_init_logs
#include "include/Times.h"               // for doy2mday, doy2month, doy2week
#include "tests/gtests/sw_testhelpers.h" // for tol9, length
#include "gtest/gtest.h"                 // for Message, TestPartResult, Test
#include <stdio.h>                       // for NULL

namespace {
TEST(TimesTest, TimesLeapYear) {
    TimeInt days_in_month[MAX_MONTHS], cum_monthdays[MAX_MONTHS];

    unsigned int k, lpadd,
        years[] = {1900, 1980, 1981, 2000}; // noleap, leap, noleap, leap years

    Bool kleap, isleap[] = {swFALSE, swTRUE, swFALSE, swTRUE};

    Time_init_model(days_in_month);

    // Loop through years and tests
    for (k = 0; k < length(years); k++) {
        Time_new_year(years[k], days_in_month, cum_monthdays);

        kleap = isleapyear(years[k]);
        lpadd = kleap ? 1 : 0;

        EXPECT_EQ(kleap, isleap[k]);
        EXPECT_EQ(Time_days_in_month(Feb, days_in_month), 28 + lpadd);
        EXPECT_EQ(Time_get_lastdoy_y(years[k]), 365 + lpadd);

        // first day of January
        EXPECT_EQ(doy2month(1, cum_monthdays), Jan);

        // last day of February
        EXPECT_EQ(doy2month(59 + lpadd, cum_monthdays), Feb);

        // first day of March
        EXPECT_EQ(doy2month(60 + lpadd, cum_monthdays), Mar);

        // last day of December
        EXPECT_EQ(doy2month(365 + lpadd, cum_monthdays), Dec);

        // first day of January
        EXPECT_EQ(doy2mday(1, cum_monthdays, days_in_month), 1);

        // last day of February
        EXPECT_EQ(
            doy2mday(59 + lpadd, cum_monthdays, days_in_month), 28 + lpadd
        );

        // first day of March
        EXPECT_EQ(doy2mday(60 + lpadd, cum_monthdays, days_in_month), 1);

        // last day of December
        EXPECT_EQ(doy2mday(365 + lpadd, cum_monthdays, days_in_month), 31);

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

TEST(TimesTest, TimesInterpolateMonthlyValues) {
    // point to the structure that contains cloud coverage monthly values
    RealD cloudcov_monthly[MAX_MONTHS];

    // `interpolate_monthlyValues()` needs an array of length `MAX_DAYS + 1`
    // if `interpAsBase1` is TRUE
    RealD cloudcov_daily[MAX_DAYS + 1];


    TimeInt days_in_month[MAX_MONTHS], cum_monthdays[MAX_MONTHS];

    Bool interpAsBase1 = swFALSE;

    unsigned int i, k, doy, lpadd,
        years[] = {1980, 1981}; // leap year, non-leap year

    Bool isMon1;

    Time_init_model(days_in_month);

    // Loop through years and tests
    for (k = 0; k < length(years); k++) {
        Time_new_year(years[k], days_in_month, cum_monthdays);
        lpadd = isleapyear(years[k]) ? 1 : 0;

        // Test: all monthlyValues equal to 10
        //   (not affected by leap/nonleap yrs)
        for (i = 0; i < MAX_MONTHS; i++) {
            cloudcov_monthly[i] = 10;
        }

        cloudcov_daily[0] = SW_MISSING;

        interpolate_monthlyValues(
            cloudcov_monthly,
            interpAsBase1,
            cum_monthdays,
            days_in_month,
            cloudcov_daily
        );

        // value for daily index 0 is 10 to make sure base0 is working correctly
        EXPECT_NEAR(cloudcov_daily[0], 10.0, tol9);

        // Expect all xintpld values to be the same (constant input)
        for (doy = 0; doy < Time_get_lastdoy_y(years[k]); doy++) {
            EXPECT_NEAR(cloudcov_daily[doy], 10.0, tol9);
        }

        // Zero the first value in "cloudcov_daily" for testing on base1
        // interpolation
        cloudcov_daily[0] = 0.;

        interpolate_monthlyValues(
            cloudcov_monthly,
            swTRUE,
            cum_monthdays,
            days_in_month,
            cloudcov_daily
        );

        // Value for daily index 0 is unchanged because we use here a base1
        // index
        EXPECT_NEAR(cloudcov_daily[0], 0, tol9);
        EXPECT_NEAR(cloudcov_daily[1], 10., tol9);

        // Test: all monthlyValues equal to 10 except December and March are 20
        //   (affected by leap/nonleap yrs)
        cloudcov_monthly[Mar] = 20;
        cloudcov_monthly[Dec] = 20;

        interpolate_monthlyValues(
            cloudcov_monthly,
            interpAsBase1,
            cum_monthdays,
            days_in_month,
            cloudcov_daily
        );


        // value for daily index 0 is ~14.5161 to make sure base0 is working
        // correctly
        EXPECT_NEAR(cloudcov_daily[0], 14.516129032, tol9);

        // Expect mid-Nov to mid-Jan and mid-Feb to mid-Apr values to vary,
        // all other are the same

        // Expect Jan 1 to Jan 15 to vary
        for (doy = 0; doy < 15; doy++) {
            EXPECT_NEAR(
                cloudcov_daily[doy],
                valXd(
                    10,
                    20,
                    -1,
                    doy2mday(doy + 1, cum_monthdays, days_in_month),
                    31
                ),
                tol9
            );
        }

        // Expect Jan 15 to Feb 15 to have same values as the constant input
        for (doy = 14; doy < 45; doy++) {
            EXPECT_NEAR(cloudcov_daily[doy], 10.0, tol9);
        }

        // Expect Feb 16 to March 15 to vary (account for leap years)
        for (doy = 45; doy < 74 + lpadd; doy++) {
            isMon1 = (Bool) (doy <= 58 + lpadd);

            EXPECT_NEAR(
                cloudcov_daily[doy],
                valXd(
                    isMon1 ? 10 : 20,
                    isMon1 ? 20 : 10,
                    isMon1 ? 1 : -1,
                    doy2mday(doy + 1, cum_monthdays, days_in_month),
                    28 + lpadd
                ),
                tol9
            ) << "year = "
              << years[k] << " doy = " << doy
              << " mday = " << doy2mday(doy + 1, cum_monthdays, days_in_month);
        }

        // Expect Apr 15 to Nov 15 to have same values as the constant input
        for (doy = 104 + lpadd; doy < 319 + lpadd; doy++) {
            EXPECT_NEAR(cloudcov_daily[doy], 10.0, tol9);
        }

        // Expect Dec 1 to Dec 31 to vary
        for (doy = 335 + lpadd; doy < 365 + lpadd; doy++) {
            isMon1 = (Bool) (doy < 349 + lpadd);
            EXPECT_NEAR(
                cloudcov_daily[doy],
                valXd(
                    20, // Dec value
                    10, // Nov or Jan value
                    isMon1 ? -1 : 1,
                    doy2mday(doy + 1, cum_monthdays, days_in_month),
                    isMon1 ? 30 : 31
                ),
                tol9
            ) << "year = "
              << years[k] << " doy = " << doy
              << " mday = " << doy2mday(doy + 1, cum_monthdays, days_in_month);
        }
    }
}

// Test time tracking
TEST(TimesTest, TimeTracking) {
    SW_WALLTIME wt;
    WallTimeSpec start;
    Bool ok;
    int k, n_runs = 10;
    LOG_INFO LogInfo;


    // Time difference between start and stop
    set_walltime(&start, &ok);
    // ... do some work
    if (ok) {
        EXPECT_GE(diff_walltime(start, ok), 0.);
    }

    EXPECT_DOUBLE_EQ(diff_walltime(start, swFALSE), -1.);


    // Time tracking
    SW_WT_StartTime(&wt);

    for (k = 0; k < n_runs; k++) {
        set_walltime(&start, &ok);
        // ... do some work
        SW_WT_TimeRun(start, ok, &wt);
    }

    // Time reporting
    sw_init_logs(NULL, &LogInfo);
    LogInfo.QuietMode = swTRUE;
    SW_WT_ReportTime(wt, &LogInfo);

    if (wt.has_walltime) {
        EXPECT_EQ(wt.nTimedRuns, n_runs);
        EXPECT_GE(wt.timeMean, 0.);
    } else {
        EXPECT_EQ(wt.nTimedRuns, 0);
    }
}
} // namespace
