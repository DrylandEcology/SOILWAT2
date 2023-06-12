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
  TEST_F(AllTest, WaterBalanceExample1) { // default run == 'testing' example1
    int i;

    // Run the simulation
    SW_CTL_main(&SW_All, &SW_OutputPtrs, &PathInfo, &LogInfo);

    // Collect and output from daily checks
    for (i = 0; i < N_WBCHECKS; i++) {
      EXPECT_EQ(0, SW_All.SoilWat.wbError[i]) <<
        "Water balance error in test " <<
        i << ": " << (char*)SW_All.SoilWat.wbErrorNames[i];
    }
  }


  TEST_F(AllTest, WaterBalanceWithSoilTemperature) {
    int i;

    // Turn on soil temperature simulations
    SW_All.Site.use_soil_temp = swTRUE;

    // Run the simulation
     SW_CTL_main(&SW_All, &SW_OutputPtrs, &PathInfo, &LogInfo);

    // Collect and output from daily checks
    for (i = 0; i < N_WBCHECKS; i++) {
      EXPECT_EQ(0, SW_All.SoilWat.wbError[i]) <<
        "Water balance error in test " <<
        i << ": " << (char*)SW_All.SoilWat.wbErrorNames[i];
    }
  }


  TEST_F(AllTest, WithPondedWaterRunonRunoff) {
    int i;

    // Turn on impermeability of first soil layer, runon, and runoff
    SW_All.Site.impermeability[0] = 0.95;
    SW_All.Site.percentRunoff = 0.5;
    SW_All.Site.percentRunon = 1.25;

    // Run the simulation
     SW_CTL_main(&SW_All, &SW_OutputPtrs, &PathInfo, &LogInfo);

    // Collect and output from daily checks
    for (i = 0; i < N_WBCHECKS; i++) {
      EXPECT_EQ(0, SW_All.SoilWat.wbError[i]) <<
        "Water balance error in test " <<
        i << ": " << (char*)SW_All.SoilWat.wbErrorNames[i];
    }
  }



  TEST_F(AllTest, WaterBalanceWithWeatherGeneratorOnly) {
    int i;

    // Turn on Markov weather generator (and turn off use of historical weather)
    SW_All.Weather.generateWeatherMethod = 2;
    SW_All.Weather.use_weathergenerator_only = swTRUE;

    // Read Markov weather generator input files (they are not normally read)
    SW_MKV_setup(&SW_All.Markov, SW_All.Weather.rng_seed,
                 SW_All.Weather.generateWeatherMethod,
                 PathInfo.InFiles, &LogInfo);

    // Point to nonexisting weather data
    strcpy(SW_All.Weather.name_prefix, "Input/data_weather_nonexisting/weath");

    // Prepare weather data
    SW_WTH_read(&SW_All.Weather, &SW_All.Sky, &SW_All.Model, &LogInfo);
    SW_WTH_finalize_all_weather(&SW_All.Markov, &SW_All.Weather,
    SW_All.Model.cum_monthdays, SW_All.Model.days_in_month, &LogInfo);

    // Run the simulation
     SW_CTL_main(&SW_All, &SW_OutputPtrs, &PathInfo, &LogInfo);

    // Collect and output from daily checks
    for (i = 0; i < N_WBCHECKS; i++) {
      EXPECT_EQ(0, SW_All.SoilWat.wbError[i]) <<
        "Water balance error in test " <<
        i << ": " << (char*)SW_All.SoilWat.wbErrorNames[i];
    }
  }


  TEST_F(AllTest, WaterBalanceWithWeatherGeneratorForSomeMissingValues) {
    int i;

    // Turn on Markov weather generator
    SW_All.Weather.generateWeatherMethod = 2;

    // Point to partial weather data
    strcpy(SW_All.Weather.name_prefix, "Input/data_weather_missing/weath");

    // Read Markov weather generator input files (they are not normally read)
    SW_MKV_setup(&SW_All.Markov, SW_All.Weather.rng_seed,
                 SW_All.Weather.generateWeatherMethod,
                 PathInfo.InFiles, &LogInfo);

    // Prepare weather data
    SW_WTH_read(&SW_All.Weather, &SW_All.Sky, &SW_All.Model, &LogInfo);
    SW_WTH_finalize_all_weather(&SW_All.Markov, &SW_All.Weather,
    SW_All.Model.cum_monthdays, SW_All.Model.days_in_month, &LogInfo);

    // Run the simulation
     SW_CTL_main(&SW_All, &SW_OutputPtrs, &PathInfo, &LogInfo);

    // Collect and output from daily checks
    for (i = 0; i < N_WBCHECKS; i++) {
      EXPECT_EQ(0, SW_All.SoilWat.wbError[i]) <<
        "Water balance error in test " <<
        i << ": " << (char*)SW_All.SoilWat.wbErrorNames[i];
    }
  }


  TEST_F(AllTest, WaterBalanceWithHighGravelVolume) {
    int i;
    LyrIndex s;

    // Set high gravel volume in all soil layers
    ForEachSoilLayer(s, SW_All.Site.n_layers)
    {
      SW_All.Site.fractionVolBulk_gravel[s] = 0.99;
    }

    // Re-calculate soils
    SW_SIT_init_run(&SW_All.VegProd, &SW_All.Site, PathInfo.InFiles, &LogInfo);

    // Run the simulation
     SW_CTL_main(&SW_All, &SW_OutputPtrs, &PathInfo, &LogInfo);

    // Collect and output from daily checks
    for (i = 0; i < N_WBCHECKS; i++) {
      EXPECT_EQ(0, SW_All.SoilWat.wbError[i]) <<
        "Water balance error in test " <<
        i << ": " << (char*)SW_All.SoilWat.wbErrorNames[i];
    }
  }


  TEST_F(AllTest, WaterBalanceWithVegetationFromClimate1) {
    int i;

    // Select method to estimate vegetation from long-term climate
    SW_All.VegProd.veg_method = 1;

    // Re-calculate vegetation
    SW_VPD_init_run(&SW_All.VegProd, &SW_All.Weather, &SW_All.Model,
                    SW_All.Site.latitude, &LogInfo);

    // Run the simulation
     SW_CTL_main(&SW_All, &SW_OutputPtrs, &PathInfo, &LogInfo);

    // Collect and output from daily checks
    for (i = 0; i < N_WBCHECKS; i++) {
      EXPECT_EQ(0, SW_All.SoilWat.wbError[i]) <<
        "Water balance error in test " <<
        i << ": " << (char*)SW_All.SoilWat.wbErrorNames[i];
    }
  }

  TEST_F(AllTest, WaterBalanceWithSWRCvanGenuchten1980) {
    int i;

    // Set SWRC and PTF (and SWRC parameter input filename)
    strcpy(SW_All.Site.site_swrc_name, (char *) "vanGenuchten1980");
    SW_All.Site.site_swrc_type = encode_str2swrc(SW_All.Site.site_swrc_name, &LogInfo);
    strcpy(SW_All.Site.site_ptf_name, (char *) "Rosetta3");
    SW_All.Site.site_ptf_type = encode_str2ptf(SW_All.Site.site_ptf_name);
    SW_All.Site.site_has_swrcp = swTRUE;

    Mem_Free(PathInfo.InFiles[eSWRCp]);
    PathInfo.InFiles[eSWRCp] = Str_Dup("Input/swrc_params_vanGenuchten1980.in", &LogInfo);

    // Read SWRC parameter input file (which is not read by default)
    SW_SWRC_read(&SW_All.Site, PathInfo.InFiles, &LogInfo);

    // Update soils
    SW_SIT_init_run(&SW_All.VegProd, &SW_All.Site, PathInfo.InFiles, &LogInfo);

    // Run the simulation
     SW_CTL_main(&SW_All, &SW_OutputPtrs, &PathInfo, &LogInfo);

    // Collect and output from daily checks
    for (i = 0; i < N_WBCHECKS; i++) {
      EXPECT_EQ(0, SW_All.SoilWat.wbError[i]) <<
        "Water balance error in test " <<
        i << ": " << (char*)SW_All.SoilWat.wbErrorNames[i];
    }
  }



  TEST_F(AllTest, WaterBalanceWithSWRCFXW) {
    int i;

    // Set SWRC and PTF (and SWRC parameter input filename)
    strcpy(SW_All.Site.site_swrc_name, (char *) "FXW");
    SW_All.Site.site_swrc_type = encode_str2swrc(SW_All.Site.site_swrc_name, &LogInfo);
    strcpy(SW_All.Site.site_ptf_name, (char *) "neuroFX2021");
    SW_All.Site.site_ptf_type = encode_str2ptf(SW_All.Site.site_ptf_name);
    SW_All.Site.site_has_swrcp = swTRUE;

    Mem_Free(PathInfo.InFiles[eSWRCp]);
    PathInfo.InFiles[eSWRCp] = Str_Dup("Input/swrc_params_FXW.in", &LogInfo);

    // Read SWRC parameter input file (which is not read by default)
    SW_SWRC_read(&SW_All.Site, PathInfo.InFiles, &LogInfo);

    // Update soils
    SW_SIT_init_run(&SW_All.VegProd, &SW_All.Site, PathInfo.InFiles, &LogInfo);

    // Run the simulation
     SW_CTL_main(&SW_All, &SW_OutputPtrs, &PathInfo, &LogInfo);

    // Collect and output from daily checks
    for (i = 0; i < N_WBCHECKS; i++) {
      EXPECT_EQ(0, SW_All.SoilWat.wbError[i]) <<
        "Water balance error in test " <<
        i << ": " << (char*)SW_All.SoilWat.wbErrorNames[i];
    }
  }


  TEST_F(AllTest, WaterBalanceWithDaymet) {
    int i;

    // Point to Daymet weather data
    strcpy(SW_All.Weather.name_prefix, "Input/data_weather_daymet/weath");

    // Adjust simulation years: we have 2 years of Daymet inputs
    SW_All.Model.startyr = 1980;
    SW_All.Model.endyr = 1981;

    // Describe daily Daymet inputs
    SW_All.Weather.use_cloudCoverMonthly = swFALSE;
    SW_All.Weather.use_windSpeedMonthly = swTRUE;
    SW_All.Weather.use_humidityMonthly = swFALSE;

    SW_All.Weather.dailyInputIndices[ACTUAL_VP] = 3;
    SW_All.Weather.dailyInputIndices[SHORT_WR] = 4;
    SW_All.Weather.dailyInputFlags[ACTUAL_VP] = swTRUE;
    SW_All.Weather.dailyInputFlags[SHORT_WR] = swTRUE;
    SW_All.Weather.n_input_forcings = 5;
    SW_All.Weather.desc_rsds = 2; // Daymet rsds is flux density over daylight period

    // Prepare weather data
    SW_WTH_read(&SW_All.Weather, &SW_All.Sky, &SW_All.Model, &LogInfo);
    SW_WTH_finalize_all_weather(&SW_All.Markov, &SW_All.Weather,
    SW_All.Model.cum_monthdays, SW_All.Model.days_in_month, &LogInfo);

    // Run the simulation
    SW_CTL_main(&SW_All, &SW_OutputPtrs, &PathInfo, &LogInfo);

    // Collect and output from daily checks
    for (i = 0; i < N_WBCHECKS; i++) {
      EXPECT_EQ(0, SW_All.SoilWat.wbError[i]) <<
        "Water balance error in test " <<
        i << ": " << (char*)SW_All.SoilWat.wbErrorNames[i];
    }
  }


  TEST_F(AllTest, WaterBalanceWithGRIDMET) {
    int i;

    // Point to gridMET weather data
    strcpy(SW_All.Weather.name_prefix, "Input/data_weather_gridmet/weath");

    // Adjust simulation years: we have 2 years of gridMET inputs
    SW_All.Model.startyr = 1980;
    SW_All.Model.endyr = 1981;

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
    SW_WTH_read(&SW_All.Weather, &SW_All.Sky, &SW_All.Model, &LogInfo);
    SW_WTH_finalize_all_weather(&SW_All.Markov, &SW_All.Weather,
    SW_All.Model.cum_monthdays, SW_All.Model.days_in_month, &LogInfo);

    // Run the simulation
     SW_CTL_main(&SW_All, &SW_OutputPtrs, &PathInfo, &LogInfo);

    // Collect and output from daily checks
    for (i = 0; i < N_WBCHECKS; i++) {
      EXPECT_EQ(0, SW_All.SoilWat.wbError[i]) <<
        "Water balance error in test " <<
        i << ": " << (char*)SW_All.SoilWat.wbErrorNames[i];
    }
  }


  TEST_F(AllTest, WaterBalanceWithMACA) {
    int i;

    // Point to MACA weather data
    strcpy(SW_All.Weather.name_prefix, "Input/data_weather_maca/weath");

    // Adjust simulation years: we have 2 years of MACA inputs
    SW_All.Model.startyr = 1980;
    SW_All.Model.endyr = 1981;

    // Describe daily MACA inputs
    SW_All.Weather.use_cloudCoverMonthly = swFALSE;
    SW_All.Weather.use_windSpeedMonthly = swFALSE;
    SW_All.Weather.use_humidityMonthly = swFALSE;

    SW_All.Weather.dailyInputIndices[WIND_EAST] = 3;
    SW_All.Weather.dailyInputIndices[WIND_NORTH] = 4;
    SW_All.Weather.dailyInputIndices[REL_HUMID_MAX] = 5;
    SW_All.Weather.dailyInputIndices[REL_HUMID_MIN] = 6;
    SW_All.Weather.dailyInputIndices[SHORT_WR] = 7;
    SW_All.Weather.dailyInputFlags[WIND_EAST] = swTRUE;
    SW_All.Weather.dailyInputFlags[WIND_NORTH] = swTRUE;
    SW_All.Weather.dailyInputFlags[REL_HUMID_MAX] = swTRUE;
    SW_All.Weather.dailyInputFlags[REL_HUMID_MIN] = swTRUE;
    SW_All.Weather.dailyInputFlags[SHORT_WR] = swTRUE;
    SW_All.Weather.n_input_forcings = 8;
    SW_All.Weather.desc_rsds = 1; // MACA rsds is flux density over 24 hours

    // Prepare weather data
    SW_WTH_read(&SW_All.Weather, &SW_All.Sky, &SW_All.Model, &LogInfo);
    SW_WTH_finalize_all_weather(&SW_All.Markov, &SW_All.Weather,
    SW_All.Model.cum_monthdays, SW_All.Model.days_in_month, &LogInfo);

    // Run the simulation
    SW_CTL_main(&SW_All, &SW_OutputPtrs, &PathInfo, &LogInfo);

    // Collect and output from daily checks
    for (i = 0; i < N_WBCHECKS; i++) {
      EXPECT_EQ(0, SW_All.SoilWat.wbError[i]) <<
        "Water balance error in test " <<
        i << ": " << (char*)SW_All.SoilWat.wbErrorNames[i];
    }
  }


} // namespace
