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


extern SW_MODEL SW_Model;
extern SW_SOILWAT SW_Soilwat;
extern SW_SITE SW_Site;
extern SW_WEATHER SW_Weather;


namespace {
  /* Test daily water balance and water cycling:
       i) Call function 'SW_CTL_main' which calls 'SW_CTL_run_current_year' for each year
          which calls 'SW_SWC_water_flow' for each day
      ii) Summarize checks added to debugging code of 'SW_SWC_water_flow' (which is
          compiled if flag 'SWDEBUG' is defined)
  */
  TEST(WaterBalance, Example1) { // default run == 'testing' example1
    int i;

    // Run the simulation
    SW_CTL_main();

    // Collect and output from daily checks
    for (i = 0; i < N_WBCHECKS; i++) {
      EXPECT_EQ(0, SW_Soilwat.wbError[i]) << "Water balance error in test " <<
        i << ": " << (char*)SW_Soilwat.wbErrorNames[i];
    }

    // Reset to previous global state
    Reset_SOILWAT2_after_UnitTest();
  }


  TEST(WaterBalance, WithSoilTemperature) {
    int i;

    // Turn on soil temperature simulations
    SW_Site.use_soil_temp = swTRUE;

    // Run the simulation
    SW_CTL_main();

    // Collect and output from daily checks
    for (i = 0; i < N_WBCHECKS; i++) {
      EXPECT_EQ(0, SW_Soilwat.wbError[i]) << "Water balance error in test " <<
        i << ": " << (char*)SW_Soilwat.wbErrorNames[i];
    }

    // Reset to previous global state
    Reset_SOILWAT2_after_UnitTest();
  }


  TEST(WaterBalance, WithPondedWaterRunonRunoff) {
    int i;

    // Turn on impermeability of first soil layer, runon, and runoff
    SW_Site.lyr[0]->impermeability = 0.95;
    SW_Site.percentRunoff = 0.5;
    SW_Site.percentRunon = 1.25;

    // Run the simulation
    SW_CTL_main();

    // Collect and output from daily checks
    for (i = 0; i < N_WBCHECKS; i++) {
      EXPECT_EQ(0, SW_Soilwat.wbError[i]) << "Water balance error in test " <<
        i << ": " << (char*)SW_Soilwat.wbErrorNames[i];
    }

    // Reset to previous global state
    Reset_SOILWAT2_after_UnitTest();
  }



  TEST(WaterBalance, WithWeatherGeneratorOnly) {
    int i;

    // Turn on Markov weather generator (and turn off use of historical weather)
    SW_Soilwat.hist_use = swFALSE;
    SW_Weather.yr.first = SW_Model.endyr + 1;
    SW_Weather.use_markov = swTRUE;

    // Read Markov weather generator input files (they are not normally read)
    SW_MKV_setup();

    // Run the simulation
    SW_CTL_main();

    // Collect and output from daily checks
    for (i = 0; i < N_WBCHECKS; i++) {
      EXPECT_EQ(0, SW_Soilwat.wbError[i]) << "Water balance error: " << SW_Soilwat.wbErrorNames[i];
    }

    // Reset to previous global state
    Reset_SOILWAT2_after_UnitTest();
  }


  TEST(WaterBalance, WithWeatherGeneratorForSomeMissingValues) {
    int i;

    // Turn on Markov weather generator
    SW_Weather.use_markov = swTRUE;

    // Read Markov weather generator input files (they are not normally read)
    SW_MKV_setup();

    // Point to partial weather data
    strcpy(SW_Weather.name_prefix, "Input/data_weather_missing/weath");

    // Run the simulation
    SW_CTL_main();

    // Collect and output from daily checks
    for (i = 0; i < N_WBCHECKS; i++) {
      EXPECT_EQ(0, SW_Soilwat.wbError[i]) << "Water balance error: " << SW_Soilwat.wbErrorNames[i];
    }

    // Reset to previous global state
    Reset_SOILWAT2_after_UnitTest();
  }


} // namespace
