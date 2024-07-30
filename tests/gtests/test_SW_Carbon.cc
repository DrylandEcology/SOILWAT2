#include "include/SW_Carbon.h"           // for SW_CBN_construct, SW_CBN_read
#include "include/SW_datastructs.h"      // for SW_CARBON
#include "include/SW_Defines.h"          // for TimeInt, ForEachVegType
#include "include/SW_Main_lib.h"         // for sw_fail_on_error
#include "include/SW_VegProd.h"          // for BIO_INDEX, WUE_INDEX
#include "tests/gtests/sw_testhelpers.h" // for CarbonFixtureTest
#include "gtest/gtest.h"                 // for Message, Test, CmpHelperGT
#include <string.h>                      // for strcpy

namespace {
// Test the SW_Carbon constructor 'SW_CBN_construct'
TEST(CarbonTest, CarbonConstructor) {
    int x;
    SW_CARBON SW_Carbon;

    SW_CBN_construct(&SW_Carbon); // does not allocate memory

    // Test type (and existence)
    EXPECT_EQ(typeid(x), typeid(SW_Carbon.use_wue_mult));
    EXPECT_EQ(typeid(x), typeid(SW_Carbon.use_bio_mult));
}

// Test reading yearly CO2 data from disk file
TEST_F(CarbonFixtureTest, CarbonReadInputFile) {
    TimeInt year;
    TimeInt const simendyr = SW_Run.Model.endyr + SW_Run.Model.addtl_yr;
    double sum_CO2;

    // Test if CO2-effects are turned off -> no CO2 concentration data are read
    // from file
    SW_CBN_construct(&SW_Run.Carbon);
    SW_Run.Carbon.use_wue_mult = 0;
    SW_Run.Carbon.use_bio_mult = 0;

    SW_CBN_read(
        &SW_Run.Carbon, &SW_Run.Model, SW_Domain.PathInfo.InFiles, &LogInfo
    );
    sw_fail_on_error(&LogInfo); // exit test program if unexpected error

    sum_CO2 = 0.;
    for (year = 0; year < MAX_NYEAR; year++) {
        sum_CO2 += SW_Run.Carbon.ppm[year];
    }
    EXPECT_DOUBLE_EQ(sum_CO2, 0.);

    // Test if CO2-effects are turned on -> CO2 concentration data are read from
    // file
    SW_CBN_construct(&SW_Run.Carbon);
    strcpy(SW_Run.Carbon.scenario, "RCP85");
    SW_Run.Carbon.use_wue_mult = 1;
    SW_Run.Carbon.use_bio_mult = 1;
    SW_Run.Model.addtl_yr = 0;

    SW_CBN_read(
        &SW_Run.Carbon, &SW_Run.Model, SW_Domain.PathInfo.InFiles, &LogInfo
    );
    sw_fail_on_error(&LogInfo); // exit test program if unexpected error

    for (year = SW_Run.Model.startyr + SW_Run.Model.addtl_yr; year <= simendyr;
         year++) {
        EXPECT_GT(SW_Run.Carbon.ppm[year], 0.);
    }
}

// Test the calculation of CO2-effect multipliers
TEST_F(CarbonFixtureTest, CarbonCO2multipliers) {
    TimeInt year;
    TimeInt const simendyr = SW_Run.Model.endyr + SW_Run.Model.addtl_yr;
    int k;

    SW_CBN_construct(&SW_Run.Carbon);
    strcpy(SW_Run.Carbon.scenario, "RCP85");
    SW_Run.Carbon.use_wue_mult = 1;
    SW_Run.Carbon.use_bio_mult = 1;
    SW_Run.Model.addtl_yr = 0;

    SW_CBN_read(
        &SW_Run.Carbon, &SW_Run.Model, SW_Domain.PathInfo.InFiles, &LogInfo
    );
    sw_fail_on_error(&LogInfo); // exit test program if unexpected error

    SW_CBN_init_run(
        SW_Run.VegProd.veg, &SW_Run.Model, &SW_Run.Carbon, &LogInfo
    );
    sw_fail_on_error(&LogInfo); // exit test program if unexpected error

    for (year = SW_Run.Model.startyr + SW_Run.Model.addtl_yr; year <= simendyr;
         year++) {
        ForEachVegType(k) {
            EXPECT_GT(
                SW_Run.VegProd.veg[k].co2_multipliers[BIO_INDEX][year], 0.
            );
            EXPECT_GT(
                SW_Run.VegProd.veg[k].co2_multipliers[WUE_INDEX][year], 0.
            );
        }
    }
}
} // namespace
