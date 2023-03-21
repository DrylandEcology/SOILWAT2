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

#include "include/generic.h"
#include "include/myMemory.h"
#include "include/filefuncs.h"
#include "include/rands.h"
#include "include/Times.h"
#include "include/SW_Defines.h"
#include "include/SW_Times.h"
#include "include/SW_Files.h"
#include "include/SW_Carbon.h"
#include "include/SW_Site.h"
#include "include/SW_VegProd.h"
#include "include/SW_VegEstab.h"
#include "include/SW_Model.h"
#include "include/SW_SoilWater.h"
#include "include/SW_Weather.h"
#include "include/SW_Markov.h"
#include "include/SW_Sky.h"
#include "include/SW_Control.h"

#include "tests/gtests/sw_testhelpers.h"



namespace {
  /* Test daily water balance and water cycling:
       i) Call function 'SW_CTL_main' which calls 'SW_CTL_run_current_year' for each year
          which calls 'SW_SWC_water_flow' for each day
      ii) Summarize checks added to debugging code of 'SW_SWC_water_flow' (which is
          compiled if flag 'SWDEBUG' is defined)
  */
  TEST(WaterBalanceTest, Example1) { // default run == 'testing' example1
    int i;

    // Run the simulation
    SW_CTL_main(&SW_All);

    // Collect and output from daily checks
    for (i = 0; i < N_WBCHECKS; i++) {
      EXPECT_EQ(0, SW_Soilwat.wbError[i]) <<
        "Water balance error in test " <<
        i << ": " << (char*)SW_Soilwat.wbErrorNames[i];
    }

    // Reset to previous global state
    Reset_SOILWAT2_after_UnitTest();
  }


  TEST(WaterBalanceTest, WithSoilTemperature) {
    int i;

    // Turn on soil temperature simulations
    SW_Site.use_soil_temp = swTRUE;

    // Run the simulation
    SW_CTL_main(&SW_All);

    // Collect and output from daily checks
    for (i = 0; i < N_WBCHECKS; i++) {
      EXPECT_EQ(0, SW_Soilwat.wbError[i]) <<
        "Water balance error in test " <<
        i << ": " << (char*)SW_Soilwat.wbErrorNames[i];
    }

    // Reset to previous global state
    Reset_SOILWAT2_after_UnitTest();
  }


  TEST(WaterBalanceTest, WithPondedWaterRunonRunoff) {
    int i;

    // Turn on impermeability of first soil layer, runon, and runoff
    SW_Site.lyr[0]->impermeability = 0.95;
    SW_Site.percentRunoff = 0.5;
    SW_Site.percentRunon = 1.25;

    // Run the simulation
    SW_CTL_main(&SW_All);

    // Collect and output from daily checks
    for (i = 0; i < N_WBCHECKS; i++) {
      EXPECT_EQ(0, SW_Soilwat.wbError[i]) <<
        "Water balance error in test " <<
        i << ": " << (char*)SW_Soilwat.wbErrorNames[i];
    }

    // Reset to previous global state
    Reset_SOILWAT2_after_UnitTest();
  }



  TEST(WaterBalanceTest, WithWeatherGeneratorOnly) {
    int i;

    // Turn on Markov weather generator (and turn off use of historical weather)
    SW_All.Weather.generateWeatherMethod = 2;
    SW_All.Weather.use_weathergenerator_only = swTRUE;

    // Read Markov weather generator input files (they are not normally read)
    SW_MKV_setup(&SW_All.Weather);

    // Point to nonexisting weather data
    strcpy(SW_All.Weather.name_prefix, "Input/data_weather_nonexisting/weath");

    // Prepare weather data
    SW_WTH_read(&SW_All.Weather);
    SW_WTH_finalize_all_weather(&SW_All.Weather);

    // Run the simulation
    SW_CTL_main(&SW_All);

    // Collect and output from daily checks
    for (i = 0; i < N_WBCHECKS; i++) {
      EXPECT_EQ(0, SW_Soilwat.wbError[i]) <<
        "Water balance error in test " <<
        i << ": " << (char*)SW_Soilwat.wbErrorNames[i];
    }

    // Reset to previous global state
    Reset_SOILWAT2_after_UnitTest();
  }


  TEST(WaterBalanceTest, WithWeatherGeneratorForSomeMissingValues) {
    int i;

    // Turn on Markov weather generator
    SW_All.Weather.generateWeatherMethod = 2;

    // Point to partial weather data
    strcpy(SW_All.Weather.name_prefix, "Input/data_weather_missing/weath");

    // Read Markov weather generator input files (they are not normally read)
    SW_MKV_setup(&SW_All.Weather);

    // Prepare weather data
    SW_WTH_read(&SW_All.Weather);
    SW_WTH_finalize_all_weather(&SW_All.Weather);

    // Run the simulation
    SW_CTL_main(&SW_All);

    // Collect and output from daily checks
    for (i = 0; i < N_WBCHECKS; i++) {
      EXPECT_EQ(0, SW_Soilwat.wbError[i]) <<
        "Water balance error in test " <<
        i << ": " << (char*)SW_Soilwat.wbErrorNames[i];
    }

    // Reset to previous global state
    Reset_SOILWAT2_after_UnitTest();
  }


  TEST(WaterBalanceTest, WithHighGravelVolume) {
    int i;
    LyrIndex s;

    // Set high gravel volume in all soil layers
    ForEachSoilLayer(s)
    {
      SW_Site.lyr[s]->fractionVolBulk_gravel = 0.99;
    }

    // Re-calculate soils
    SW_SIT_init_run(&SW_All.VegProd);

    // Run the simulation
    SW_CTL_main(&SW_All);

    // Collect and output from daily checks
    for (i = 0; i < N_WBCHECKS; i++) {
      EXPECT_EQ(0, SW_Soilwat.wbError[i]) <<
        "Water balance error in test " <<
        i << ": " << (char*)SW_Soilwat.wbErrorNames[i];
    }

    // Reset to previous global state
    Reset_SOILWAT2_after_UnitTest();
  }


  TEST(WaterBalanceTest, WithVegetationFromClimate1) {
    int i;

    // Select method to estimate vegetation from long-term climate
    SW_All.VegProd.veg_method = 1;

    // Re-calculate vegetation
    SW_VPD_init_run(&SW_All.VegProd, &SW_All.Weather);

    // Run the simulation
    SW_CTL_main(&SW_All);

    // Collect and output from daily checks
    for (i = 0; i < N_WBCHECKS; i++) {
      EXPECT_EQ(0, SW_Soilwat.wbError[i]) <<
        "Water balance error in test " <<
        i << ": " << (char*)SW_Soilwat.wbErrorNames[i];
    }

    // Reset to previous global state
    Reset_SOILWAT2_after_UnitTest();
  }

  TEST(WaterBalanceTest, WithSWRCvanGenuchten1980) {
    int i;

    // Set SWRC and PTF (and SWRC parameter input filename)
    strcpy(SW_Site.site_swrc_name, (char *) "vanGenuchten1980");
    SW_Site.site_swrc_type = encode_str2swrc(SW_Site.site_swrc_name);
    strcpy(SW_Site.site_ptf_name, (char *) "Rosetta3");
    SW_Site.site_ptf_type = encode_str2ptf(SW_Site.site_ptf_name);
    SW_Site.site_has_swrcp = swTRUE;

    Mem_Free(InFiles[eSWRCp]);
    InFiles[eSWRCp] = Str_Dup("Input/swrc_params_vanGenuchten1980.in");

    // Read SWRC parameter input file (which is not read by default)
    SW_SWRC_read();

    // Update soils
    SW_SIT_init_run(&SW_All.VegProd);

    // Run the simulation
    SW_CTL_main(&SW_All);

    // Collect and output from daily checks
    for (i = 0; i < N_WBCHECKS; i++) {
      EXPECT_EQ(0, SW_Soilwat.wbError[i]) <<
        "Water balance error in test " <<
        i << ": " << (char*)SW_Soilwat.wbErrorNames[i];
    }

    // Reset to previous global state
    Reset_SOILWAT2_after_UnitTest();
  }



  TEST(WaterBalanceTest, WithSWRCFXW) {
    int i;

    // Set SWRC and PTF (and SWRC parameter input filename)
    strcpy(SW_Site.site_swrc_name, (char *) "FXW");
    SW_Site.site_swrc_type = encode_str2swrc(SW_Site.site_swrc_name);
    strcpy(SW_Site.site_ptf_name, (char *) "neuroFX2021");
    SW_Site.site_ptf_type = encode_str2ptf(SW_Site.site_ptf_name);
    SW_Site.site_has_swrcp = swTRUE;

    Mem_Free(InFiles[eSWRCp]);
    InFiles[eSWRCp] = Str_Dup("Input/swrc_params_FXW.in");

    // Read SWRC parameter input file (which is not read by default)
    SW_SWRC_read();

    // Update soils
    SW_SIT_init_run(&SW_All.VegProd);

    // Run the simulation
    SW_CTL_main(&SW_All);

    // Collect and output from daily checks
    for (i = 0; i < N_WBCHECKS; i++) {
      EXPECT_EQ(0, SW_Soilwat.wbError[i]) <<
        "Water balance error in test " <<
        i << ": " << (char*)SW_Soilwat.wbErrorNames[i];
    }

    // Reset to previous global state
    Reset_SOILWAT2_after_UnitTest();
  }


  TEST(WaterBalanceTest, WithGRIDMET) {
    int i;

    // Point to gridMET weather data
    strcpy(SW_All.Weather.name_prefix, "Input/data_weather_gridmet/weath");

    // Adjust simulation years: we have 2 years of gridMET inputs
    SW_Model.startyr = 1980;
    SW_Model.endyr = 1981;

    // Describe daily gridMET inputs
    SW_All.Weather.use_cloudCoverMonthly = swFALSE;
    SW_All.Weather.use_windSpeedMonthly = swFALSE;
    SW_All.Weather.use_humidityMonthly = swFALSE;

    SW_All.Weather.dailyInputIndices[WIND_SPEED] = 3;
    SW_All.Weather.dailyInputIndices[REL_HUMID_MAX] = 4;
    SW_All.Weather.dailyInputIndices[REL_HUMID_MIN] = 5;
    SW_All.Weather.dailyInputIndices[SHORT_WR] = 6;
    SW_All.Weather.dailyInputFlags[REL_HUMID_MAX] = swTRUE;
    SW_All.Weather.dailyInputFlags[REL_HUMID_MIN] = swTRUE;
    SW_All.Weather.dailyInputFlags[WIND_SPEED] = swTRUE;
    SW_All.Weather.dailyInputFlags[SHORT_WR] = swTRUE;
    SW_All.Weather.n_input_forcings = 7;
    SW_All.Weather.desc_rsds = 1; // gridMET rsds is flux density over 24 hours

    // Prepare weather data
    SW_WTH_read(&SW_All.Weather);
    SW_WTH_finalize_all_weather(&SW_All.Weather);

    // Run the simulation
    SW_CTL_main(&SW_All);

    // Collect and output from daily checks
    for (i = 0; i < N_WBCHECKS; i++) {
      EXPECT_EQ(0, SW_Soilwat.wbError[i]) <<
        "Water balance error in test " <<
        i << ": " << (char*)SW_Soilwat.wbErrorNames[i];
    }

    // Reset to previous global state
    Reset_SOILWAT2_after_UnitTest();
  }


} // namespace
