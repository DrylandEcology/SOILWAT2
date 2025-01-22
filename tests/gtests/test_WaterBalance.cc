#include "include/generic.h"             // for swTRUE, swFALSE
#include "include/myMemory.h"            // for Str_Dup
#include "include/SW_Control.h"          // for SW_CTL_main, SW_CTL_run_spinup
#include "include/SW_datastructs.h"      // for N_WBCHECKS
#include "include/SW_Defines.h"          // for SHORT_WR, REL_HUMID_MAX
#include "include/SW_Files.h"            // for eSWRCp
#include "include/SW_Main_lib.h"         // for sw_fail_on_error
#include "include/SW_Markov.h"           // for SW_MKV_setup
#include "include/SW_Site.h"             // for SW_SIT_init_run, SW_SWRC_read
#include "include/SW_SoilWater.h"        // for SW_SWC_init_run
#include "include/SW_VegProd.h"          // for SW_VPD_init_run
#include "include/SW_Weather.h"          // for SW_WTH_finalize_all_weather
#include "tests/gtests/sw_testhelpers.h" // for WaterBalanceFixtureTest
#include "gtest/gtest.h"                 // for Message, EXPECT_EQ, TEST_F
#include <stdio.h>                       // for snprintf
#include <stdlib.h>                      // for free

namespace {
/* Test daily water balance and water cycling:

     i) Call function 'SW_CTL_main' which calls 'SW_CTL_run_current_year' for
   each year which calls 'SW_SWC_water_flow' for each day

    ii) Summarize checks added to debugging code of 'SW_SWC_water_flow' (which
   is compiled if flag 'SWDEBUG' is defined)
*/

// default run == 'testing' example1
TEST_F(WaterBalanceFixtureTest, WaterBalanceExample1) {
    int i;

    // Run the simulation
    SW_CTL_main(&SW_Run, &SW_Domain.OutDom, &LogInfo);
    sw_fail_on_error(&LogInfo); // exit test program if unexpected error

    // Collect and output from daily checks
    for (i = 0; i < N_WBCHECKS; i++) {
        EXPECT_EQ(0, SW_Run.SoilWat.wbError[i])
            << "Water balance error in test " << i << ": "
            << SW_Run.SoilWat.wbErrorNames[i];
    }
}

TEST_F(WaterBalanceFixtureTest, WaterBalanceWithSoilTemperature) {
    int i;

    // Turn on soil temperature simulations
    SW_Run.Site.use_soil_temp = swTRUE;

    // Run the simulation
    SW_CTL_main(&SW_Run, &SW_Domain.OutDom, &LogInfo);
    sw_fail_on_error(&LogInfo); // exit test program if unexpected error

    // Collect and output from daily checks
    for (i = 0; i < N_WBCHECKS; i++) {
        EXPECT_EQ(0, SW_Run.SoilWat.wbError[i])
            << "Water balance error in test " << i << ": "
            << SW_Run.SoilWat.wbErrorNames[i];
    }
}

TEST_F(WaterBalanceFixtureTest, WaterBalanceWithPondedWaterRunonRunoff) {
    int i;

    // Turn on impermeability of first soil layer, runon, and runoff
    SW_Run.Site.soils.impermeability[0] = 0.95;
    SW_Run.Site.percentRunoff = 0.5;
    SW_Run.Site.percentRunon = 1.25;

    // Run the simulation
    SW_CTL_main(&SW_Run, &SW_Domain.OutDom, &LogInfo);
    sw_fail_on_error(&LogInfo); // exit test program if unexpected error

    // Collect and output from daily checks
    for (i = 0; i < N_WBCHECKS; i++) {
        EXPECT_EQ(0, SW_Run.SoilWat.wbError[i])
            << "Water balance error in test " << i << ": "
            << SW_Run.SoilWat.wbErrorNames[i];
    }
}

TEST_F(WaterBalanceFixtureTest, WaterBalanceWithWeatherGeneratorOnly) {
    int i;

    // Turn on Markov weather generator (and turn off use of historical weather)
    SW_Run.Weather.generateWeatherMethod = 2;
    SW_Run.Weather.use_weathergenerator_only = swTRUE;

    // Read Markov weather generator input files (they are not normally read)
    SW_MKV_setup(
        &SW_Run.Markov,
        SW_Run.Weather.rng_seed,
        SW_Run.Weather.generateWeatherMethod,
        SW_Domain.SW_PathInputs.txtInFiles,
        &LogInfo
    );
    sw_fail_on_error(&LogInfo); // exit test program if unexpected error

    // Point to nonexisting weather data
    (void) snprintf(
        SW_Run.Weather.name_prefix,
        sizeof SW_Run.Weather.name_prefix,
        "%s",
        "Input/data_weather_nonexisting/weath"
    );

    // Prepare weather data
    SW_WTH_read(&SW_Run.Weather, &SW_Run.Sky, &SW_Run.Model, swTRUE, &LogInfo);
    sw_fail_on_error(&LogInfo); // exit test program if unexpected error

    SW_WTH_finalize_all_weather(
        &SW_Run.Markov,
        &SW_Run.Weather,
        SW_Run.Model.cum_monthdays,
        SW_Run.Model.days_in_month,
        &LogInfo
    );
    sw_fail_on_error(&LogInfo); // exit test program if unexpected error

    // Run the simulation
    SW_CTL_main(&SW_Run, &SW_Domain.OutDom, &LogInfo);
    sw_fail_on_error(&LogInfo); // exit test program if unexpected error

    // Collect and output from daily checks
    for (i = 0; i < N_WBCHECKS; i++) {
        EXPECT_EQ(0, SW_Run.SoilWat.wbError[i])
            << "Water balance error in test " << i << ": "
            << SW_Run.SoilWat.wbErrorNames[i];
    }
}

TEST_F(
    WaterBalanceFixtureTest,
    WaterBalanceWithWeatherGeneratorForSomeMissingValues
) {
    int i;

    // Turn on Markov weather generator
    SW_Run.Weather.generateWeatherMethod = 2;

    // Point to partial weather data
    (void) snprintf(
        SW_Run.Weather.name_prefix,
        sizeof SW_Run.Weather.name_prefix,
        "%s",
        "Input/data_weather_missing/weath"
    );

    // Read Markov weather generator input files (they are not normally read)
    SW_MKV_setup(
        &SW_Run.Markov,
        SW_Run.Weather.rng_seed,
        SW_Run.Weather.generateWeatherMethod,
        SW_Domain.SW_PathInputs.txtInFiles,
        &LogInfo
    );
    sw_fail_on_error(&LogInfo); // exit test program if unexpected error

    // Prepare weather data
    SW_WTH_read(&SW_Run.Weather, &SW_Run.Sky, &SW_Run.Model, swTRUE, &LogInfo);
    sw_fail_on_error(&LogInfo); // exit test program if unexpected error

    SW_WTH_finalize_all_weather(
        &SW_Run.Markov,
        &SW_Run.Weather,
        SW_Run.Model.cum_monthdays,
        SW_Run.Model.days_in_month,
        &LogInfo
    );
    sw_fail_on_error(&LogInfo); // exit test program if unexpected error

    // Run the simulation
    SW_CTL_main(&SW_Run, &SW_Domain.OutDom, &LogInfo);
    sw_fail_on_error(&LogInfo); // exit test program if unexpected error

    // Collect and output from daily checks
    for (i = 0; i < N_WBCHECKS; i++) {
        EXPECT_EQ(0, SW_Run.SoilWat.wbError[i])
            << "Water balance error in test " << i << ": "
            << SW_Run.SoilWat.wbErrorNames[i];
    }
}

TEST_F(WaterBalanceFixtureTest, WaterBalanceWithHighGravelVolume) {
    int i;
    LyrIndex s;

    // Set high gravel volume in all soil layers
    ForEachSoilLayer(s, SW_Run.Site.n_layers) {
        SW_Run.Site.soils.fractionVolBulk_gravel[s] = 0.99;
    }

    // Re-calculate soils
    SW_SIT_init_run(&SW_Run.VegProd, &SW_Run.Site, &LogInfo);
    SW_SWC_init_run(&SW_Run.SoilWat, &SW_Run.Site, &SW_Run.Weather.temp_snow);
    sw_fail_on_error(&LogInfo); // exit test program if unexpected error

    // Run the simulation
    SW_CTL_main(&SW_Run, &SW_Domain.OutDom, &LogInfo);
    sw_fail_on_error(&LogInfo); // exit test program if unexpected error

    // Collect and output from daily checks
    for (i = 0; i < N_WBCHECKS; i++) {
        EXPECT_EQ(0, SW_Run.SoilWat.wbError[i])
            << "Water balance error in test " << i << ": "
            << SW_Run.SoilWat.wbErrorNames[i];
    }
}

TEST_F(WaterBalanceFixtureTest, WaterBalanceWithOneSoilLayer) {
    int i;

    // Setup one soil layer
    create_test_soillayers(1, &SW_Run.VegProd, &SW_Run.Site, &LogInfo);
    sw_fail_on_error(&LogInfo); // exit test program if unexpected error

    // Initialize `swcBulk` based on new soil layers
    SW_SWC_init_run(&SW_Run.SoilWat, &SW_Run.Site, &SW_Run.Weather.temp_snow);

    // Run the simulation
    SW_CTL_main(&SW_Run, &SW_Domain.OutDom, &LogInfo);
    sw_fail_on_error(&LogInfo); // exit test program if unexpected error

    // Collect and output from daily checks
    for (i = 0; i < N_WBCHECKS; i++) {
        EXPECT_EQ(0, SW_Run.SoilWat.wbError[i])
            << "Water balance error in test " << i << ": "
            << SW_Run.SoilWat.wbErrorNames[i];
    }
}

TEST_F(WaterBalanceFixtureTest, WaterBalanceWithMaxSoilLayers) {
    int i;

    // Setup maximum number of soil layers
    create_test_soillayers(MAX_LAYERS, &SW_Run.VegProd, &SW_Run.Site, &LogInfo);
    sw_fail_on_error(&LogInfo); // exit test program if unexpected error

    // Initialize `swcBulk` based on new soil layers
    SW_SWC_init_run(&SW_Run.SoilWat, &SW_Run.Site, &SW_Run.Weather.temp_snow);

    // Run the simulation
    SW_CTL_main(&SW_Run, &SW_Domain.OutDom, &LogInfo);
    sw_fail_on_error(&LogInfo); // exit test program if unexpected error

    // Collect and output from daily checks
    for (i = 0; i < N_WBCHECKS; i++) {
        EXPECT_EQ(0, SW_Run.SoilWat.wbError[i])
            << "Water balance error in test " << i << ": "
            << SW_Run.SoilWat.wbErrorNames[i];
    }
}

TEST_F(WaterBalanceFixtureTest, WaterBalanceWithVegetationFromClimate1) {
    int i;

    // Select method to estimate vegetation from long-term climate
    SW_Run.VegProd.veg_method = 1;

    // Re-calculate vegetation
    SW_VPD_init_run(
        &SW_Run.VegProd, &SW_Run.Weather, &SW_Run.Model, swTRUE, &LogInfo
    );
    sw_fail_on_error(&LogInfo);

    // Run the simulation
    SW_CTL_main(&SW_Run, &SW_Domain.OutDom, &LogInfo);
    sw_fail_on_error(&LogInfo); // exit test program if unexpected error

    // Collect and output from daily checks
    for (i = 0; i < N_WBCHECKS; i++) {
        EXPECT_EQ(0, SW_Run.SoilWat.wbError[i])
            << "Water balance error in test " << i << ": "
            << SW_Run.SoilWat.wbErrorNames[i];
    }
}

TEST_F(WaterBalanceFixtureTest, WaterBalanceWithOrganicMatter) {
    unsigned int i;

    // Set PTF (Cosby1984AndOthers handles OM only up to 8%)
    (void) snprintf(
        SW_Run.Site.site_ptf_name,
        sizeof SW_Run.Site.site_ptf_name,
        "%s",
        "Cosby1984"
    );
    SW_Run.Site.site_ptf_type = encode_str2ptf(SW_Run.Site.site_ptf_name);
    SW_Run.Site.site_has_swrcpMineralSoil = swFALSE;
    SW_Run.Site.inputsProvideSWRCp = swFALSE;

    // Set organic matter > 0
    SW_Run.Site.soils.fractionWeight_om[0] = 1.;
    for (i = 1; i < SW_Run.Site.n_layers; i++) {
        SW_Run.Site.soils.fractionWeight_om[i] = 0.5;
    }

    // Update soils
    SW_SIT_init_run(&SW_Run.VegProd, &SW_Run.Site, &LogInfo);
    SW_SWC_init_run(&SW_Run.SoilWat, &SW_Run.Site, &SW_Run.Weather.temp_snow);
    sw_fail_on_error(&LogInfo); // exit test program if unexpected error

    // Two simulation years are sufficient
    SW_Run.Model.startyr = 1980;
    SW_Run.Model.endyr = 1981;

    // Run the simulation
    SW_CTL_main(&SW_Run, &SW_Domain.OutDom, &LogInfo);
    sw_fail_on_error(&LogInfo); // exit test program if unexpected error

    // Collect and output from daily checks
    for (i = 0; i < N_WBCHECKS; i++) {
        EXPECT_EQ(0, SW_Run.SoilWat.wbError[i])
            << "Water balance error in test " << i << ": "
            << SW_Run.SoilWat.wbErrorNames[i];
    }
}

TEST_F(WaterBalanceFixtureTest, WaterBalanceWithSWRCvanGenuchten1980) {
    int i;

    // Set SWRC and PTF (and SWRC parameter input filename)
    (void) snprintf(
        SW_Run.Site.site_swrc_name,
        sizeof SW_Run.Site.site_swrc_name,
        "%s",
        "vanGenuchten1980"
    );
    SW_Run.Site.site_swrc_type =
        encode_str2swrc(SW_Run.Site.site_swrc_name, &LogInfo);
    sw_fail_on_error(&LogInfo); // exit test program if unexpected error
    (void) snprintf(
        SW_Run.Site.site_ptf_name,
        sizeof SW_Run.Site.site_ptf_name,
        "%s",
        "Rosetta3"
    );
    SW_Run.Site.site_ptf_type = encode_str2ptf(SW_Run.Site.site_ptf_name);
    SW_Run.Site.site_has_swrcpMineralSoil = swFALSE;
    SW_Run.Site.inputsProvideSWRCp = swTRUE;

    free(SW_Domain.SW_PathInputs.txtInFiles[eSWRCp]);
    SW_Domain.SW_PathInputs.txtInFiles[eSWRCp] =
        Str_Dup("Input/swrc_params_vanGenuchten1980.in", &LogInfo);
    sw_fail_on_error(&LogInfo); // exit test program if unexpected error

    // Read SWRC parameter input file (which is not read by default)
    SW_SWRC_read(&SW_Run.Site, SW_Domain.SW_PathInputs.txtInFiles, &LogInfo);
    sw_fail_on_error(&LogInfo); // exit test program if unexpected error

    // Update soils
    SW_SIT_init_run(&SW_Run.VegProd, &SW_Run.Site, &LogInfo);
    SW_SWC_init_run(&SW_Run.SoilWat, &SW_Run.Site, &SW_Run.Weather.temp_snow);
    sw_fail_on_error(&LogInfo); // exit test program if unexpected error

    // Run the simulation
    SW_CTL_main(&SW_Run, &SW_Domain.OutDom, &LogInfo);
    sw_fail_on_error(&LogInfo); // exit test program if unexpected error

    // Collect and output from daily checks
    for (i = 0; i < N_WBCHECKS; i++) {
        EXPECT_EQ(0, SW_Run.SoilWat.wbError[i])
            << "Water balance error in test " << i << ": "
            << SW_Run.SoilWat.wbErrorNames[i];
    }
}

TEST_F(WaterBalanceFixtureTest, WaterBalanceWithSWRCFXW) {
    unsigned int i;

    // Set SWRC and PTF (and SWRC parameter input filename)
    (void) snprintf(
        SW_Run.Site.site_swrc_name,
        sizeof SW_Run.Site.site_swrc_name,
        "%s",
        "FXW"
    );
    SW_Run.Site.site_swrc_type =
        encode_str2swrc(SW_Run.Site.site_swrc_name, &LogInfo);
    sw_fail_on_error(&LogInfo); // exit test program if unexpected error
    (void) snprintf(
        SW_Run.Site.site_ptf_name,
        sizeof SW_Run.Site.site_ptf_name,
        "%s",
        "neuroFX2021"
    );
    SW_Run.Site.site_ptf_type = encode_str2ptf(SW_Run.Site.site_ptf_name);
    SW_Run.Site.site_has_swrcpMineralSoil = swFALSE;
    SW_Run.Site.inputsProvideSWRCp = swTRUE;

    free(SW_Domain.SW_PathInputs.txtInFiles[eSWRCp]);
    SW_Domain.SW_PathInputs.txtInFiles[eSWRCp] =
        Str_Dup("Input/swrc_params_FXW.in", &LogInfo);
    sw_fail_on_error(&LogInfo); // exit test program if unexpected error

    // Read SWRC parameter input file (which is not read by default)
    SW_SWRC_read(&SW_Run.Site, SW_Domain.SW_PathInputs.txtInFiles, &LogInfo);
    sw_fail_on_error(&LogInfo); // exit test program if unexpected error

    // FXW doesn't yet handle organic matter:
    // not all values for organic SWRC parameters have been determined
    // (see "tests/example/Input/swrc_params_FXW.in")
    for (i = 0; i < SW_Run.Site.n_layers; i++) {
        SW_Run.Site.soils.fractionWeight_om[i] = 0.;
    }

    // Update soils
    SW_SIT_init_run(&SW_Run.VegProd, &SW_Run.Site, &LogInfo);
    SW_SWC_init_run(&SW_Run.SoilWat, &SW_Run.Site, &SW_Run.Weather.temp_snow);
    sw_fail_on_error(&LogInfo); // exit test program if unexpected error

    // Run the simulation
    SW_CTL_main(&SW_Run, &SW_Domain.OutDom, &LogInfo);
    sw_fail_on_error(&LogInfo); // exit test program if unexpected error

    // Collect and output from daily checks
    for (i = 0; i < N_WBCHECKS; i++) {
        EXPECT_EQ(0, SW_Run.SoilWat.wbError[i])
            << "Water balance error in test " << i << ": "
            << SW_Run.SoilWat.wbErrorNames[i];
    }
}

TEST_F(WaterBalanceFixtureTest, WaterBalanceWithDaymet) {
    int i;

    // Point to Daymet weather data
    (void) snprintf(
        SW_Run.Weather.name_prefix,
        sizeof SW_Run.Weather.name_prefix,
        "%s",
        "Input/data_weather_daymet/weath"
    );

    // Adjust simulation years: we have 2 years of Daymet inputs
    SW_Run.Model.startyr = 1980;
    SW_Run.Model.endyr = 1981;

    // Describe daily Daymet inputs
    SW_Run.Weather.use_cloudCoverMonthly = swFALSE;
    SW_Run.Weather.use_windSpeedMonthly = swTRUE;
    SW_Run.Weather.use_humidityMonthly = swFALSE;

    SW_Run.Weather.dailyInputIndices[TEMP_MAX] = 0;
    SW_Run.Weather.dailyInputIndices[TEMP_MIN] = 1;
    SW_Run.Weather.dailyInputIndices[PPT] = 2;
    SW_Run.Weather.dailyInputIndices[CLOUD_COV] = 0;
    SW_Run.Weather.dailyInputIndices[WIND_SPEED] = 0;
    SW_Run.Weather.dailyInputIndices[WIND_EAST] = 0;
    SW_Run.Weather.dailyInputIndices[WIND_NORTH] = 0;
    SW_Run.Weather.dailyInputIndices[REL_HUMID] = 0;
    SW_Run.Weather.dailyInputIndices[REL_HUMID_MAX] = 0;
    SW_Run.Weather.dailyInputIndices[REL_HUMID_MIN] = 0;
    SW_Run.Weather.dailyInputIndices[SPEC_HUMID] = 0;
    SW_Run.Weather.dailyInputIndices[TEMP_DEWPOINT] = 0;
    SW_Run.Weather.dailyInputIndices[ACTUAL_VP] = 3;
    SW_Run.Weather.dailyInputIndices[SHORT_WR] = 4;

    SW_Run.Weather.dailyInputFlags[TEMP_MAX] = swTRUE;
    SW_Run.Weather.dailyInputFlags[TEMP_MIN] = swTRUE;
    SW_Run.Weather.dailyInputFlags[PPT] = swTRUE;
    SW_Run.Weather.dailyInputFlags[CLOUD_COV] = swFALSE;
    SW_Run.Weather.dailyInputFlags[WIND_SPEED] = swFALSE;
    SW_Run.Weather.dailyInputFlags[WIND_EAST] = swFALSE;
    SW_Run.Weather.dailyInputFlags[WIND_NORTH] = swFALSE;
    SW_Run.Weather.dailyInputFlags[REL_HUMID] = swFALSE;
    SW_Run.Weather.dailyInputFlags[REL_HUMID_MAX] = swFALSE;
    SW_Run.Weather.dailyInputFlags[REL_HUMID_MIN] = swFALSE;
    SW_Run.Weather.dailyInputFlags[SPEC_HUMID] = swFALSE;
    SW_Run.Weather.dailyInputFlags[TEMP_DEWPOINT] = swFALSE;
    SW_Run.Weather.dailyInputFlags[ACTUAL_VP] = swTRUE;
    SW_Run.Weather.dailyInputFlags[SHORT_WR] = swTRUE;

    SW_Run.Weather.n_input_forcings = 5;
    // Daymet rsds is flux density over daylight period
    SW_Run.Weather.desc_rsds = 2;

    // Prepare weather data
    SW_WTH_read(&SW_Run.Weather, &SW_Run.Sky, &SW_Run.Model, swTRUE, &LogInfo);
    sw_fail_on_error(&LogInfo); // exit test program if unexpected error

    SW_WTH_finalize_all_weather(
        &SW_Run.Markov,
        &SW_Run.Weather,
        SW_Run.Model.cum_monthdays,
        SW_Run.Model.days_in_month,
        &LogInfo
    );
    sw_fail_on_error(&LogInfo); // exit test program if unexpected error

    // Run the simulation
    SW_CTL_main(&SW_Run, &SW_Domain.OutDom, &LogInfo);
    sw_fail_on_error(&LogInfo); // exit test program if unexpected error

    // Collect and output from daily checks
    for (i = 0; i < N_WBCHECKS; i++) {
        EXPECT_EQ(0, SW_Run.SoilWat.wbError[i])
            << "Water balance error in test " << i << ": "
            << SW_Run.SoilWat.wbErrorNames[i];
    }
}

TEST_F(WaterBalanceFixtureTest, WaterBalanceWithGridMET) {
    int i;

    // Point to gridMET weather data
    (void) snprintf(
        SW_Run.Weather.name_prefix,
        sizeof SW_Run.Weather.name_prefix,
        "%s",
        "Input/data_weather_gridmet/weath"
    );

    // Adjust simulation years: we have 2 years of gridMET inputs
    SW_Run.Model.startyr = 1980;
    SW_Run.Model.endyr = 1981;

    // Describe daily gridMET inputs
    SW_Run.Weather.use_cloudCoverMonthly = swFALSE;
    SW_Run.Weather.use_windSpeedMonthly = swFALSE;
    SW_Run.Weather.use_humidityMonthly = swFALSE;

    SW_Run.Weather.dailyInputIndices[TEMP_MAX] = 0;
    SW_Run.Weather.dailyInputIndices[TEMP_MIN] = 1;
    SW_Run.Weather.dailyInputIndices[PPT] = 2;
    SW_Run.Weather.dailyInputIndices[CLOUD_COV] = 0;
    SW_Run.Weather.dailyInputIndices[WIND_SPEED] = 3;
    SW_Run.Weather.dailyInputIndices[WIND_EAST] = 0;
    SW_Run.Weather.dailyInputIndices[WIND_NORTH] = 0;
    SW_Run.Weather.dailyInputIndices[REL_HUMID] = 0;
    SW_Run.Weather.dailyInputIndices[REL_HUMID_MAX] = 4;
    SW_Run.Weather.dailyInputIndices[REL_HUMID_MIN] = 5;
    SW_Run.Weather.dailyInputIndices[SPEC_HUMID] = 0;
    SW_Run.Weather.dailyInputIndices[TEMP_DEWPOINT] = 0;
    SW_Run.Weather.dailyInputIndices[ACTUAL_VP] = 0;
    SW_Run.Weather.dailyInputIndices[SHORT_WR] = 6;

    SW_Run.Weather.dailyInputFlags[TEMP_MAX] = swTRUE;
    SW_Run.Weather.dailyInputFlags[TEMP_MIN] = swTRUE;
    SW_Run.Weather.dailyInputFlags[PPT] = swTRUE;
    SW_Run.Weather.dailyInputFlags[CLOUD_COV] = swFALSE;
    SW_Run.Weather.dailyInputFlags[WIND_SPEED] = swTRUE;
    SW_Run.Weather.dailyInputFlags[WIND_EAST] = swFALSE;
    SW_Run.Weather.dailyInputFlags[WIND_NORTH] = swFALSE;
    SW_Run.Weather.dailyInputFlags[REL_HUMID] = swFALSE;
    SW_Run.Weather.dailyInputFlags[REL_HUMID_MAX] = swTRUE;
    SW_Run.Weather.dailyInputFlags[REL_HUMID_MIN] = swTRUE;
    SW_Run.Weather.dailyInputFlags[SPEC_HUMID] = swFALSE;
    SW_Run.Weather.dailyInputFlags[TEMP_DEWPOINT] = swFALSE;
    SW_Run.Weather.dailyInputFlags[ACTUAL_VP] = swFALSE;
    SW_Run.Weather.dailyInputFlags[SHORT_WR] = swTRUE;

    SW_Run.Weather.n_input_forcings = 7;
    SW_Run.Weather.desc_rsds = 1; // gridMET rsds is flux density over 24 hours

    // Prepare weather data
    SW_WTH_read(&SW_Run.Weather, &SW_Run.Sky, &SW_Run.Model, swTRUE, &LogInfo);
    sw_fail_on_error(&LogInfo); // exit test program if unexpected error

    SW_WTH_finalize_all_weather(
        &SW_Run.Markov,
        &SW_Run.Weather,
        SW_Run.Model.cum_monthdays,
        SW_Run.Model.days_in_month,
        &LogInfo
    );
    sw_fail_on_error(&LogInfo); // exit test program if unexpected error

    // Run the simulation
    SW_CTL_main(&SW_Run, &SW_Domain.OutDom, &LogInfo);
    sw_fail_on_error(&LogInfo); // exit test program if unexpected error

    // Collect and output from daily checks
    for (i = 0; i < N_WBCHECKS; i++) {
        EXPECT_EQ(0, SW_Run.SoilWat.wbError[i])
            << "Water balance error in test " << i << ": "
            << SW_Run.SoilWat.wbErrorNames[i];
    }
}

TEST_F(WaterBalanceFixtureTest, WaterBalanceWithMACAtype1) {
    int i;

    // Switch directory to the input folder of the
    // first type of MACA (hursmin, hursmax)
    (void) snprintf(
        SW_Run.Weather.name_prefix,
        sizeof SW_Run.Weather.name_prefix,
        "%s",
        "Input/data_weather_maca-type1/weath"
    );

    // Adjust simulation years: we have 2 years of MACA inputs
    SW_Run.Model.startyr = 1980;
    SW_Run.Model.endyr = 1981;

    // Describe daily MACA inputs
    SW_Run.Weather.use_cloudCoverMonthly = swFALSE;
    SW_Run.Weather.use_windSpeedMonthly = swFALSE;
    SW_Run.Weather.use_humidityMonthly = swFALSE;

    SW_Run.Weather.dailyInputIndices[TEMP_MAX] = 0;
    SW_Run.Weather.dailyInputIndices[TEMP_MIN] = 1;
    SW_Run.Weather.dailyInputIndices[PPT] = 2;
    SW_Run.Weather.dailyInputIndices[CLOUD_COV] = 0;
    SW_Run.Weather.dailyInputIndices[WIND_SPEED] = 0;
    SW_Run.Weather.dailyInputIndices[WIND_EAST] = 3;
    SW_Run.Weather.dailyInputIndices[WIND_NORTH] = 4;
    SW_Run.Weather.dailyInputIndices[REL_HUMID] = 0;
    SW_Run.Weather.dailyInputIndices[REL_HUMID_MAX] = 5;
    SW_Run.Weather.dailyInputIndices[REL_HUMID_MIN] = 6;
    SW_Run.Weather.dailyInputIndices[SPEC_HUMID] = 0;
    SW_Run.Weather.dailyInputIndices[TEMP_DEWPOINT] = 0;
    SW_Run.Weather.dailyInputIndices[ACTUAL_VP] = 0;
    SW_Run.Weather.dailyInputIndices[SHORT_WR] = 7;

    SW_Run.Weather.dailyInputFlags[TEMP_MAX] = swTRUE;
    SW_Run.Weather.dailyInputFlags[TEMP_MIN] = swTRUE;
    SW_Run.Weather.dailyInputFlags[PPT] = swTRUE;
    SW_Run.Weather.dailyInputFlags[CLOUD_COV] = swFALSE;
    SW_Run.Weather.dailyInputFlags[WIND_SPEED] = swFALSE;
    SW_Run.Weather.dailyInputFlags[WIND_EAST] = swTRUE;
    SW_Run.Weather.dailyInputFlags[WIND_NORTH] = swTRUE;
    SW_Run.Weather.dailyInputFlags[REL_HUMID] = swFALSE;
    SW_Run.Weather.dailyInputFlags[REL_HUMID_MAX] = swTRUE;
    SW_Run.Weather.dailyInputFlags[REL_HUMID_MIN] = swTRUE;
    SW_Run.Weather.dailyInputFlags[SPEC_HUMID] = swFALSE;
    SW_Run.Weather.dailyInputFlags[TEMP_DEWPOINT] = swFALSE;
    SW_Run.Weather.dailyInputFlags[ACTUAL_VP] = swFALSE;
    SW_Run.Weather.dailyInputFlags[SHORT_WR] = swTRUE;

    SW_Run.Weather.n_input_forcings = 8;
    SW_Run.Weather.desc_rsds = 1; // MACA rsds is flux density over 24 hours

    // Prepare weather data
    SW_WTH_read(&SW_Run.Weather, &SW_Run.Sky, &SW_Run.Model, swTRUE, &LogInfo);
    sw_fail_on_error(&LogInfo); // exit test program if unexpected error

    SW_WTH_finalize_all_weather(
        &SW_Run.Markov,
        &SW_Run.Weather,
        SW_Run.Model.cum_monthdays,
        SW_Run.Model.days_in_month,
        &LogInfo
    );
    sw_fail_on_error(&LogInfo); // exit test program if unexpected error

    // Run the simulation
    SW_CTL_main(&SW_Run, &SW_Domain.OutDom, &LogInfo);
    sw_fail_on_error(&LogInfo); // exit test program if unexpected error

    // Collect and output from daily checks
    for (i = 0; i < N_WBCHECKS; i++) {
        EXPECT_EQ(0, SW_Run.SoilWat.wbError[i])
            << "Water balance error in test " << i << ": "
            << SW_Run.SoilWat.wbErrorNames[i];
    }
}

TEST_F(WaterBalanceFixtureTest, WaterBalanceWithMACAtype2) {
    int i;

    // Switch directory to the input folder of the
    // second type of MACA (huss)
    (void) snprintf(
        SW_Run.Weather.name_prefix,
        sizeof SW_Run.Weather.name_prefix,
        "%s",
        "Input/data_weather_maca-type2/weath"
    );

    // Adjust simulation years: we have 2 years of MACA inputs
    SW_Run.Model.startyr = 1980;
    SW_Run.Model.endyr = 1981;

    // Describe daily MACA inputs
    SW_Run.Weather.use_cloudCoverMonthly = swFALSE;
    SW_Run.Weather.use_windSpeedMonthly = swFALSE;
    SW_Run.Weather.use_humidityMonthly = swFALSE;

    SW_Run.Weather.dailyInputIndices[TEMP_MAX] = 0;
    SW_Run.Weather.dailyInputIndices[TEMP_MIN] = 1;
    SW_Run.Weather.dailyInputIndices[PPT] = 2;
    SW_Run.Weather.dailyInputIndices[CLOUD_COV] = 0;
    SW_Run.Weather.dailyInputIndices[WIND_SPEED] = 0;
    SW_Run.Weather.dailyInputIndices[WIND_EAST] = 3;
    SW_Run.Weather.dailyInputIndices[WIND_NORTH] = 4;
    SW_Run.Weather.dailyInputIndices[REL_HUMID] = 0;
    SW_Run.Weather.dailyInputIndices[REL_HUMID_MAX] = 0;
    SW_Run.Weather.dailyInputIndices[REL_HUMID_MIN] = 0;
    SW_Run.Weather.dailyInputIndices[SPEC_HUMID] = 5;
    SW_Run.Weather.dailyInputIndices[TEMP_DEWPOINT] = 0;
    SW_Run.Weather.dailyInputIndices[ACTUAL_VP] = 0;
    SW_Run.Weather.dailyInputIndices[SHORT_WR] = 6;

    SW_Run.Weather.dailyInputFlags[TEMP_MAX] = swTRUE;
    SW_Run.Weather.dailyInputFlags[TEMP_MIN] = swTRUE;
    SW_Run.Weather.dailyInputFlags[PPT] = swTRUE;
    SW_Run.Weather.dailyInputFlags[CLOUD_COV] = swFALSE;
    SW_Run.Weather.dailyInputFlags[WIND_SPEED] = swFALSE;
    SW_Run.Weather.dailyInputFlags[WIND_EAST] = swTRUE;
    SW_Run.Weather.dailyInputFlags[WIND_NORTH] = swTRUE;
    SW_Run.Weather.dailyInputFlags[REL_HUMID] = swFALSE;
    SW_Run.Weather.dailyInputFlags[REL_HUMID_MAX] = swFALSE;
    SW_Run.Weather.dailyInputFlags[REL_HUMID_MIN] = swFALSE;
    SW_Run.Weather.dailyInputFlags[SPEC_HUMID] = swTRUE;
    SW_Run.Weather.dailyInputFlags[TEMP_DEWPOINT] = swFALSE;
    SW_Run.Weather.dailyInputFlags[ACTUAL_VP] = swFALSE;
    SW_Run.Weather.dailyInputFlags[SHORT_WR] = swTRUE;

    SW_Run.Weather.n_input_forcings = 7;
    SW_Run.Weather.desc_rsds = 1; // MACA rsds is flux density over 24 hours

    // Prepare weather data
    SW_WTH_read(&SW_Run.Weather, &SW_Run.Sky, &SW_Run.Model, swTRUE, &LogInfo);
    sw_fail_on_error(&LogInfo); // exit test program if unexpected error

    SW_WTH_finalize_all_weather(
        &SW_Run.Markov,
        &SW_Run.Weather,
        SW_Run.Model.cum_monthdays,
        SW_Run.Model.days_in_month,
        &LogInfo
    );
    sw_fail_on_error(&LogInfo); // exit test program if unexpected error

    // Run the simulation
    SW_CTL_main(&SW_Run, &SW_Domain.OutDom, &LogInfo);
    sw_fail_on_error(&LogInfo); // exit test program if unexpected error

    // Collect and output from daily checks
    for (i = 0; i < N_WBCHECKS; i++) {
        EXPECT_EQ(0, SW_Run.SoilWat.wbError[i])
            << "Water balance error in test " << i << ": "
            << SW_Run.SoilWat.wbErrorNames[i];
    }
}

TEST_F(WaterBalanceFixtureTest, WaterBalanceWithSpinup) {
    int i;

    // Turn on spinup simulation
    SW_Run.Model.SW_SpinUp.spinup = swTRUE;
    // Set spinup variables
    SW_Run.Model.SW_SpinUp.mode = 1;
    SW_Run.Model.SW_SpinUp.duration = 5;
    SW_Run.Model.SW_SpinUp.scope = 8;

    // Run the spinup & deactivate
    SW_CTL_run_spinup(&SW_Run, &SW_Domain.OutDom, &LogInfo);

    // Run the simulation
    SW_CTL_main(&SW_Run, &SW_Domain.OutDom, &LogInfo);
    sw_fail_on_error(&LogInfo); // exit test program if unexpected error

    // Collect and output from daily checks
    for (i = 0; i < N_WBCHECKS; i++) {
        EXPECT_EQ(0, SW_Run.SoilWat.wbError[i])
            << "Water balance error in test " << i << ": "
            << SW_Run.SoilWat.wbErrorNames[i];
    }
}
} // namespace
