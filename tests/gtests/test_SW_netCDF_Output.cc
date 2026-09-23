#if defined(SWNETCDF)
#include "include/generic.h"             // for Bool, swFALSE, swTRUE
#include "include/SW_Defines.h"          // for OutPeriod, TimeInt, eSW_Day, ...
#include "include/SW_netCDF_Output.h"    // for SW_NCOUT_calc_numTimeDays, ...
#include "include/Times.h"               // for isleapyear, Time_get_lastdoy_y
#include "tests/gtests/sw_testhelpers.h" // for sw_length
#include "gtest/gtest.h"                 // for Message, TestPartResult, Test
#include <stdio.h>                       // for NULL, size_t

namespace {
/** Relative position of coordinate values at midpoint of cells
    (see `COORDS_AT_MIDPOINT` in "SW_netCDF_Output.c") */
const int posTimeAtMidpoint = 0;

// Years with a non-leap century year, leap years, and non-leap years
const TimeInt testYears[] = {1900, 1980, 1981, 2000};

/**
@brief Number of days from January 1 to March 1 of `yr`,
    i.e., days before the first seasonal time step of a year
*/
double daysFromJan1ToMar1(TimeInt yr) {
    return 31. + ((isleapyear(yr) != 0u) ? 29. : 28.);
}

/**
@brief Number of days from January 1 of `fromYr` to January 1 of `toYr`
*/
unsigned int daysFromJan1ToJan1(TimeInt fromYr, TimeInt toYr) {
    unsigned int res = 0;
    TimeInt yr;

    for (yr = fromYr; yr < toYr; yr++) {
        res += Time_get_lastdoy_y(yr);
    }

    return res;
}

/**
@brief Number of time steps of output period `pd` for complete years
    `fromYr` to `toYr - 1` (as calculated by `SW_NCOUT_calc_timeSize()`)
*/
size_t timeSizeOfCompleteYears(OutPeriod pd, TimeInt fromYr, TimeInt toYr) {
    return (pd == eSW_Day) ? (size_t) daysFromJan1ToJan1(fromYr, toYr) :
                             (size_t) outTimes[pd] * (size_t) (toYr - fromYr);
}

// Test that time bounds are contiguous and cover complete years
TEST(NCOutTest, NCOutTimeBoundsOfCompleteYears) {
    OutPeriod pd;
    TimeInt startYr;
    size_t timeSize;
    size_t index;
    unsigned int k;
    const TimeInt numYears = 3;

    double startTime;
    double bndsVals[3 * MAX_DAYS * 2] = {0.};
    double dimVarVals[3 * MAX_DAYS] = {0.};

    for (k = 0; k < sw_length(testYears); k++) {
        startYr = testYears[k];

        ForEachOutPeriod(pd) {
            timeSize = timeSizeOfCompleteYears(pd, startYr, startYr + numYears);
            startTime = 0.;

            SW_NCOUT_calc_numTimeDays(
                timeSize,
                pd,
                startYr,
                posTimeAtMidpoint,
                swFALSE, // do not add days before the simulation start year
                bndsVals,
                dimVarVals,
                &startTime
            );

            // Bounds of consecutive time steps are contiguous
            // and time values are located at the midpoint of their bounds
            for (index = 0; index < timeSize; index++) {
                EXPECT_GT(bndsVals[index * 2 + 1], bndsVals[index * 2])
                    << "period = " << pd << ", year = " << startYr;

                if (index > 0) {
                    EXPECT_DOUBLE_EQ(
                        bndsVals[index * 2], bndsVals[(index - 1) * 2 + 1]
                    ) << "period = "
                      << pd << ", year = " << startYr;
                }

                EXPECT_DOUBLE_EQ(
                    dimVarVals[index],
                    (bndsVals[index * 2] + bndsVals[index * 2 + 1]) / 2.
                ) << "period = "
                  << pd << ", year = " << startYr;
            }

            // Time steps of daily, weekly, monthly, and yearly periods
            // cover complete calendar years;
            // seasonal time steps start with spring (March 1) and, thus,
            // are shifted by January and February of the first year
            if (pd == eSW_Season) {
                EXPECT_DOUBLE_EQ(
                    startTime,
                    (double) daysFromJan1ToJan1(startYr, startYr + numYears) -
                        daysFromJan1ToMar1(startYr) +
                        daysFromJan1ToMar1(startYr + numYears)
                ) << "year = "
                  << startYr;
            } else {
                EXPECT_DOUBLE_EQ(
                    bndsVals[(timeSize - 1) * 2 + 1],
                    (double) daysFromJan1ToJan1(startYr, startYr + numYears)
                ) << "period = "
                  << pd << ", year = " << startYr;
            }
        }
    }
}

// Test lengths of seasonal time steps
TEST(NCOutTest, NCOutTimeSeasonalSteps) {
    TimeInt startYr;
    TimeInt winterYr;
    size_t index;
    unsigned int k;
    const TimeInt numYears = 2;
    const size_t timeSize = (size_t) SW_OUTNSEASONS * numYears;

    double startTime;
    double bndsVals[SW_OUTNSEASONS * 2 * 2] = {0.};
    // Spring (Mar-May), summer (Jun-Aug), fall (Sep-Nov), winter (Dec-Feb)
    const double expDays[] = {92., 92., 91., 90.};
    double expNumDays;

    for (k = 0; k < sw_length(testYears); k++) {
        startYr = testYears[k];
        startTime = 0.;

        SW_NCOUT_calc_numTimeDays(
            timeSize,
            eSW_Season,
            startYr,
            posTimeAtMidpoint,
            swFALSE, // do not add days before the simulation start year
            bndsVals,
            NULL,
            &startTime
        );

        for (index = 0; index < timeSize; index++) {
            expNumDays = expDays[index % SW_OUTNSEASONS];

            if (index % SW_OUTNSEASONS == (size_t) eSW_Winter) {
                // February of a winter season belongs to the next year
                winterYr = (TimeInt) (startYr + index / SW_OUTNSEASONS + 1);
                expNumDays += (isleapyear(winterYr) != 0u) ? 1. : 0.;
            }

            EXPECT_DOUBLE_EQ(
                bndsVals[index * 2 + 1] - bndsVals[index * 2], expNumDays
            ) << "season index = "
              << index << ", year = " << startYr;
        }
    }
}

// Test number of days between the base calendar year and
// the first reported time step of the simulation start year
TEST(NCOutTest, NCOutTimeDaysBeforeSimulationStart) {
    OutPeriod pd;
    TimeInt startYr;
    TimeInt baseYr;
    size_t timeSize;
    unsigned int k;
    unsigned int k2;
    const TimeInt numYearStartOffset[] = {0, 10};

    double startTime;
    double expStartTime;

    for (k = 0; k < sw_length(testYears); k++) {
        startYr = testYears[k];

        for (k2 = 0; k2 < sw_length(numYearStartOffset); k2++) {
            baseYr = startYr - numYearStartOffset[k2];

            ForEachOutPeriod(pd) {
                timeSize = timeSizeOfCompleteYears(pd, baseYr, startYr);
                startTime = 0.;

                SW_NCOUT_calc_numTimeDays(
                    timeSize,
                    pd,
                    baseYr,
                    0,      // unused
                    swTRUE, // add days before the simulation start year
                    NULL,
                    NULL,
                    &startTime
                );

                // The first time step of daily, weekly, monthly, and yearly
                // periods starts on January 1 of the simulation start year
                // whereas the first seasonal time step starts on March 1
                // (January and February belong to an incomplete winter
                // that is not reported)
                expStartTime = (double) daysFromJan1ToJan1(baseYr, startYr);

                if (pd == eSW_Season) {
                    expStartTime += daysFromJan1ToMar1(startYr);
                }

                EXPECT_DOUBLE_EQ(startTime, expStartTime)
                    << "period = " << pd << ", base year = " << baseYr
                    << ", start year = " << startYr;
            }
        }
    }
}
} // namespace

#endif // SWNETCDF
