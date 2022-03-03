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

#include "../generic.h"
#include "../myMemory.h"
#include "../filefuncs.h"
#include "../rands.h"
#include "../Times.h"
#include "../SW_Defines.h"
#include "../SW_Times.h"
#include "../SW_Files.h"
#include "../SW_Carbon.h"
#include "../SW_Site.h"
#include "../SW_VegProd.h"
#include "../SW_VegEstab.h"
#include "../SW_Model.h"
#include "../SW_SoilWater.h"
#include "../SW_Weather.h"
#include "../SW_Markov.h"
#include "../SW_Sky.h"
#include "../SW_Control.h"

#include "sw_testhelpers.h"



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
    SW_CTL_main();

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
    SW_CTL_main();

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
    SW_CTL_main();

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
    SW_Soilwat.hist_use = swFALSE;
    SW_Weather.yr.first = SW_Model.endyr + 1;
    SW_Weather.use_weathergenerator = swTRUE;

    // Read Markov weather generator input files (they are not normally read)
    SW_MKV_setup();

    // Run the simulation
    SW_CTL_main();

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
    SW_Weather.use_weathergenerator = swTRUE;

    // Read Markov weather generator input files (they are not normally read)
    SW_MKV_setup();

    // Point to partial weather data
    strcpy(SW_Weather.name_prefix, "Input/data_weather_missing/weath");

    // Run the simulation
    SW_CTL_main();

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
    SW_SIT_init_run();

    // Run the simulation
    SW_CTL_main();

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

    // Set SWRC and PDF (and SWRC parameter input filename)
    strcpy(SW_Site.site_swrc_name, (char *) "vanGenuchten1980");
    SW_Site.site_swrc_type = encode_str2swrc(SW_Site.site_swrc_name);
    strcpy(SW_Site.site_pdf_name, (char *) "NoPDF");
    SW_Site.site_pdf_type = encode_str2pdf(SW_Site.site_pdf_name);

    Mem_Free(InFiles[eSWRCp]);
    InFiles[eSWRCp] = Str_Dup("Input/swrc_params_vanGenuchten1980.in");

    // Read SWRC parameter input file (which is not read by default)
    SW_SWRC_read();

    // Update soils
    SW_SIT_init_run();

    // Run the simulation
    SW_CTL_main();

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
