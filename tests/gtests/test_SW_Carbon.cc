#include "include/SW_Carbon.h"           // for SW_CBN_construct, SW_CBN_read
#include "include/SW_datastructs.h"      // for SW_CARBON_INPUTS
#include "include/SW_Defines.h"          // for TimeInt, ForEachVegType
#include "include/SW_Main_lib.h"         // for sw_fail_on_error
#include "include/SW_VegProd.h"          // for BIO_INDEX, WUE_INDEX
#include "tests/gtests/sw_testhelpers.h" // for CarbonFixtureTest
#include "gtest/gtest.h"                 // for Message, Test, CmpHelperGT
#include <stdio.h>                       // for snprintf

namespace {
// Test the SW_Carbon constructor 'SW_CBN_construct'
TEST(CarbonTest, CarbonConstructor) {
    int x;
    SW_CARBON_INPUTS SW_CarbonIn;

    SW_CBN_construct(&SW_CarbonIn); // does not allocate memory

    // Test type (and existence)
    EXPECT_EQ(typeid(x), typeid(SW_CarbonIn.use_wue_mult));
    EXPECT_EQ(typeid(x), typeid(SW_CarbonIn.use_bio_mult));
}

// Test reading yearly CO2 data from disk file
TEST_F(CarbonFixtureTest, CarbonInReadInputFile) {
    TimeInt year;
    TimeInt const n_years = SW_Run.ModelIn.endyr - SW_Run.ModelIn.startyr + 1;
    double sum_CO2;
    int **dummyExistArr = NULL;

    SW_CBN_deconstruct(&SW_Run.CarbonIn);

    // Test if CO2-effects are turned off -> no CO2 concentration data are read
    // from file
    SW_CBN_construct(&SW_Run.CarbonIn);
    SW_Run.CarbonIn.use_wue_mult = 0;
    SW_Run.CarbonIn.use_bio_mult = 0;

    SW_CBN_alloc_ppm_existing_years(
        n_years, &SW_Run.CarbonIn.ppm, dummyExistArr, &LogInfo
    );
    sw_fail_on_error(&LogInfo); // exit test program if unexpected error

    SW_CBN_read(
        &SW_Run.CarbonIn,
        SW_Run.ModelSim.addtl_yr,
        SW_Run.ModelIn.startyr,
        SW_Run.ModelIn.endyr,
        SW_Domain.SW_PathInputs.txtInFiles,
        SW_Run.VegProdIn.vegYear,
        &LogInfo
    );
    sw_fail_on_error(&LogInfo); // exit test program if unexpected error

    sum_CO2 = 0.;
    for (year = 0; year < n_years; year++) {
        sum_CO2 += SW_Run.CarbonIn.ppm[year];
    }
    EXPECT_DOUBLE_EQ(sum_CO2, 0.);

    SW_CBN_deconstruct(&SW_Run.CarbonIn);

    // Test if CO2-effects are turned on -> CO2 concentration data are read from
    // file
    SW_CBN_construct(&SW_Run.CarbonIn);
    (void) snprintf(
        SW_Run.CarbonIn.scenario,
        sizeof SW_Run.CarbonIn.scenario,
        "%s",
        "CMIP5_historical|CMIP5_RCP85"
    );

    SW_Run.CarbonIn.use_wue_mult = 1;
    SW_Run.CarbonIn.use_bio_mult = 1;
    SW_Run.ModelSim.addtl_yr = 0;

    SW_CBN_read(
        &SW_Run.CarbonIn,
        SW_Run.ModelSim.addtl_yr,
        SW_Run.ModelIn.startyr,
        SW_Run.ModelIn.endyr,
        SW_Domain.SW_PathInputs.txtInFiles,
        SW_Run.VegProdIn.vegYear,
        &LogInfo
    );
    sw_fail_on_error(&LogInfo); // exit test program if unexpected error

    for (year = 0; year < n_years; year++) {
        EXPECT_GT(SW_Run.CarbonIn.ppm[year], 0.);
    }
}

// Test the calculation of CO2-effect multipliers
TEST_F(CarbonFixtureTest, CarbonInCO2multipliers) {
    TimeInt yrIdx;
    TimeInt const n_years = SW_Run.ModelIn.endyr - SW_Run.ModelIn.startyr + 1;
    int k;

    SW_CBN_deconstruct(&SW_Run.CarbonIn);

    SW_CBN_construct(&SW_Run.CarbonIn);
    SW_VPD_init_run(&SW_Run, &LogInfo);
    sw_fail_on_error(&LogInfo); // exit test program if unexpected error

    (void) snprintf(
        SW_Run.CarbonIn.scenario,
        sizeof SW_Run.CarbonIn.scenario,
        "%s",
        "CMIP5_historical|CMIP5_RCP85"
    );
    SW_Run.CarbonIn.use_wue_mult = 1;
    SW_Run.CarbonIn.use_bio_mult = 1;
    SW_Run.ModelSim.addtl_yr = 0;

    SW_CBN_read(
        &SW_Run.CarbonIn,
        SW_Run.ModelSim.addtl_yr,
        SW_Run.ModelIn.startyr,
        SW_Run.ModelIn.endyr,
        SW_Domain.SW_PathInputs.txtInFiles,
        SW_Run.VegProdIn.vegYear,
        &LogInfo
    );
    sw_fail_on_error(&LogInfo); // exit test program if unexpected error

    SW_CBN_init_run(
        SW_Run.VegProdIn.veg,
        SW_Run.VegProdSim.veg,
        &SW_Run.CarbonIn,
        SW_Run.ModelIn.startyr,
        SW_Run.ModelIn.endyr,
        &LogInfo
    );
    sw_fail_on_error(&LogInfo); // exit test program if unexpected error

    for (yrIdx = 0; yrIdx < n_years; yrIdx++) {
        ForEachVegType(k) {
            EXPECT_GT(
                SW_Run.VegProdSim.veg[k].co2_multipliers[BIO_INDEX][yrIdx], 0.
            );
            EXPECT_GT(
                SW_Run.VegProdSim.veg[k].co2_multipliers[WUE_INDEX][yrIdx], 0.
            );
        }
    }

    SW_VPD_deconstruct(&SW_Run.VegProdSim);
}
} // namespace
