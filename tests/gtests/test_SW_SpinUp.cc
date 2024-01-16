#include <gmock/gmock.h>
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
#include "include/SW_Main_lib.h"
#include "include/SW_Control.h"

#include "tests/gtests/sw_testhelpers.h"

using ::testing::HasSubstr;

namespace {
  // Test SpinUp with mode = 1 and scope > duration
  TEST_F(SpinUpTest, Mode1WithScopeGreaterThanDuration) {
    int i, n = 4; // n = number of soil layers to test
    RealD *prevTemp = new double[n],
          *prevMoist = new double[n];

    SW_All.Model.spinup_mode = 1;
    SW_All.Model.spinup_scope = 27;
    SW_All.Model.spinup_duration = 12;
    // Note: ^ For some reason, with mode = 1 and scope = 12, the WB check
    // for if infiltration = rhs fails on day 1 of 1980, which doesn't
    // happen with mode = 1 and scope = anything else (limited testing)

    // Turn on soil temperature simulations
    SW_All.Site.use_soil_temp = swTRUE;
    // Get current soil temp and soil moisture levels
    for (i = 0; i < n; i++) {
      prevTemp[i] = SW_All.SoilWat.avgLyrTemp[i]; // first layer
      prevMoist[i] = SW_All.SoilWat.swcBulk[Today][i]; // Today, first layer
    }
    // Turn on spinup flag
    SW_All.Model.spinup_active = swTRUE;

    // Run the simulation
    SW_CTL_run_spinup(&SW_All, &LogInfo);
    sw_fail_on_error(&LogInfo);
    // Turn off spinup flag
    SW_All.Model.spinup_active = swFALSE;
    SW_CTL_main(&SW_All, &SW_OutputPtrs, &LogInfo);
    sw_fail_on_error(&LogInfo); // exit test program if unexpected error
 
    for (i = 0; i < n; i++) {
      // Check soil temp after spinup
      EXPECT_NE(prevTemp[i], SW_All.SoilWat.avgLyrTemp[i]) <<
      "Soil temp error in test " <<
      i << ": " << SW_All.SoilWat.avgLyrTemp[i];

      // Check soil moisture after spinup
      EXPECT_NE(prevMoist[i], SW_All.SoilWat.swcBulk[Today][i]) <<
      "Soil moisture error in test " <<
      i << ": " << SW_All.SoilWat.swcBulk[Today][i];
    }

    // Deallocate arrays
    delete[] prevTemp;
    delete[] prevMoist;
  }

  // Test SpinUp with mode = 1 and scope = duration
  TEST_F(SpinUpTest, Mode1WithScopeEqualToDuration) {
    int i, n = 4; // n = number of soil layers to test
    RealD *prevTemp = new double[n],
          *prevMoist = new double[n];

    SW_All.Model.spinup_mode = 1;
    SW_All.Model.spinup_scope = 12;
    SW_All.Model.spinup_duration = 12;

    // Turn on soil temperature simulations
    SW_All.Site.use_soil_temp = swTRUE;
    // Get current soil temp and soil moisture levels
    for (i = 0; i < n; i++) {
      prevTemp[i] = SW_All.SoilWat.avgLyrTemp[i]; // first layer
      prevMoist[i] = SW_All.SoilWat.swcBulk[Today][i]; // Today, first layer
    }
    // Turn on spinup flag
    SW_All.Model.spinup_active = swTRUE;

    // Run the simulation
    SW_CTL_run_spinup(&SW_All, &LogInfo);
    sw_fail_on_error(&LogInfo);
    // Turn off spinup flag
    SW_All.Model.spinup_active = swFALSE;
    SW_CTL_main(&SW_All, &SW_OutputPtrs, &LogInfo);
    sw_fail_on_error(&LogInfo); // exit test program if unexpected error
 
    for (i = 0; i < n; i++) {
      // Check soil temp after spinup
      EXPECT_NE(prevTemp[i], SW_All.SoilWat.avgLyrTemp[i]) <<
      "Soil temp error in test " <<
      i << ": " << SW_All.SoilWat.avgLyrTemp[i];

      // Check soil moisture after spinup
      EXPECT_NE(prevMoist[i], SW_All.SoilWat.swcBulk[Today][i]) <<
      "Soil moisture error in test " <<
      i << ": " << SW_All.SoilWat.swcBulk[Today][i];
    }

    // Deallocate arrays
    delete[] prevTemp;
    delete[] prevMoist;
  }

    // Test SpinUp with mode = 1 and scope < duration
  TEST_F(SpinUpTest, Mode1WithScopeLessThanDuration) {
    int i, n = 4; // n = number of soil layers to test
    RealD *prevTemp = new double[n],
          *prevMoist = new double[n];

    SW_All.Model.spinup_mode = 1;
    SW_All.Model.spinup_scope = 5;
    SW_All.Model.spinup_duration = 12;

    // Turn on soil temperature simulations
    SW_All.Site.use_soil_temp = swTRUE;
    // Get current soil temp and soil moisture levels
    for (i = 0; i < n; i++) {
      prevTemp[i] = SW_All.SoilWat.avgLyrTemp[i]; // first layer
      prevMoist[i] = SW_All.SoilWat.swcBulk[Today][i]; // Today, first layer
    }
    // Turn on spinup flag
    SW_All.Model.spinup_active = swTRUE;

    // Run the simulation
    SW_CTL_run_spinup(&SW_All, &LogInfo);
    sw_fail_on_error(&LogInfo);
    // Turn off spinup flag
    SW_All.Model.spinup_active = swFALSE;
    SW_CTL_main(&SW_All, &SW_OutputPtrs, &LogInfo);
    sw_fail_on_error(&LogInfo); // exit test program if unexpected error
 
    for (i = 0; i < n; i++) {
      // Check soil temp after spinup
      EXPECT_NE(prevTemp[i], SW_All.SoilWat.avgLyrTemp[i]) <<
      "Soil temp error in test " <<
      i << ": " << SW_All.SoilWat.avgLyrTemp[i];

      // Check soil moisture after spinup
      EXPECT_NE(prevMoist[i], SW_All.SoilWat.swcBulk[Today][i]) <<
      "Soil moisture error in test " <<
      i << ": " << SW_All.SoilWat.swcBulk[Today][i];
    }

    // Deallocate arrays
    delete[] prevTemp;
    delete[] prevMoist;
  }

  // Test SpinUp with mode = 2 and scope > duration
  TEST_F(SpinUpTest, Mode2WithScopeGreaterThanDuration) {
    int i, n = 4; // n = number of soil layers to test
    RealD *prevTemp = new double[n],
          *prevMoist = new double[n];

    SW_All.Model.spinup_mode = 2;
    SW_All.Model.spinup_scope = 27;
    SW_All.Model.spinup_duration = 12;

    // Turn on soil temperature simulations
    SW_All.Site.use_soil_temp = swTRUE;
    // Get current soil temp and soil moisture levels
    for (i = 0; i < n; i++) {
      prevTemp[i] = SW_All.SoilWat.avgLyrTemp[i]; // first layer
      prevMoist[i] = SW_All.SoilWat.swcBulk[Today][i]; // Today, first layer
    }
    // Turn on spinup flag
    SW_All.Model.spinup_active = swTRUE;

    // Run the simulation
    SW_CTL_run_spinup(&SW_All, &LogInfo);
    sw_fail_on_error(&LogInfo);
    // Turn off spinup flag
    SW_All.Model.spinup_active = swFALSE;
    SW_CTL_main(&SW_All, &SW_OutputPtrs, &LogInfo);
    sw_fail_on_error(&LogInfo); // exit test program if unexpected error
 
    for (i = 0; i < n; i++) {
      // Check soil temp after spinup
      EXPECT_NE(prevTemp[i], SW_All.SoilWat.avgLyrTemp[i]) <<
      "Soil temp error in test " <<
      i << ": " << SW_All.SoilWat.avgLyrTemp[i];

      // Check soil moisture after spinup
      EXPECT_NE(prevMoist[i], SW_All.SoilWat.swcBulk[Today][i]) <<
      "Soil moisture error in test " <<
      i << ": " << SW_All.SoilWat.swcBulk[Today][i];
    }

    // Deallocate arrays
    delete[] prevTemp;
    delete[] prevMoist;
  }

  // Test SpinUp with mode = 2 and scope = duration
  TEST_F(SpinUpTest, Mode2WithScopeEqualToDuration) {
    int i, n = 4; // n = number of soil layers to test
    RealD *prevTemp = new double[n],
          *prevMoist = new double[n];

    SW_All.Model.spinup_mode = 2;
    SW_All.Model.spinup_scope = 12;
    SW_All.Model.spinup_duration = 12;

    // Turn on soil temperature simulations
    SW_All.Site.use_soil_temp = swTRUE;
    // Get current soil temp and soil moisture levels
    for (i = 0; i < n; i++) {
      prevTemp[i] = SW_All.SoilWat.avgLyrTemp[i]; // first layer
      prevMoist[i] = SW_All.SoilWat.swcBulk[Today][i]; // Today, first layer
    }
    // Turn on spinup flag
    SW_All.Model.spinup_active = swTRUE;

    // Run the simulation
    SW_CTL_run_spinup(&SW_All, &LogInfo);
    sw_fail_on_error(&LogInfo);
    // Turn off spinup flag
    SW_All.Model.spinup_active = swFALSE;
    SW_CTL_main(&SW_All, &SW_OutputPtrs, &LogInfo);
    sw_fail_on_error(&LogInfo); // exit test program if unexpected error
 
    for (i = 0; i < n; i++) {
      // Check soil temp after spinup
      EXPECT_NE(prevTemp[i], SW_All.SoilWat.avgLyrTemp[i]) <<
      "Soil temp error in test " <<
      i << ": " << SW_All.SoilWat.avgLyrTemp[i];

      // Check soil moisture after spinup
      EXPECT_NE(prevMoist[i], SW_All.SoilWat.swcBulk[Today][i]) <<
      "Soil moisture error in test " <<
      i << ": " << SW_All.SoilWat.swcBulk[Today][i];
    }

    // Deallocate arrays
    delete[] prevTemp;
    delete[] prevMoist;
  }

  // Test SpinUp with mode = 2 and scope < duration
  TEST_F(SpinUpTest, Mode2WithScopeLessThanDuration) {
    int i, n = 4; // n = number of soil layers to test
    RealD *prevTemp = new double[n],
          *prevMoist = new double[n];

    SW_All.Model.spinup_mode = 2;
    SW_All.Model.spinup_scope = 7;
    SW_All.Model.spinup_duration = 12;

    // Turn on soil temperature simulations
    SW_All.Site.use_soil_temp = swTRUE;
    // Get current soil temp and soil moisture levels
    for (i = 0; i < n; i++) {
      prevTemp[i] = SW_All.SoilWat.avgLyrTemp[i]; // first layer
      prevMoist[i] = SW_All.SoilWat.swcBulk[Today][i]; // Today, first layer
    }
    // Turn on spinup flag
    SW_All.Model.spinup_active = swTRUE;

    // Run the simulation
    SW_CTL_run_spinup(&SW_All, &LogInfo);
    sw_fail_on_error(&LogInfo);
    // Turn off spinup flag
    SW_All.Model.spinup_active = swFALSE;
    SW_CTL_main(&SW_All, &SW_OutputPtrs, &LogInfo);
    sw_fail_on_error(&LogInfo); // exit test program if unexpected error
 
    for (i = 0; i < n; i++) {
      // Check soil temp after spinup
      EXPECT_NE(prevTemp[i], SW_All.SoilWat.avgLyrTemp[i]) <<
      "Soil temp error in test " <<
      i << ": " << SW_All.SoilWat.avgLyrTemp[i];

      // Check soil moisture after spinup
      EXPECT_NE(prevMoist[i], SW_All.SoilWat.swcBulk[Today][i]) <<
      "Soil moisture error in test " <<
      i << ": " << SW_All.SoilWat.swcBulk[Today][i];
    }

    // Deallocate arrays
    delete[] prevTemp;
    delete[] prevMoist;
  }
} // namespace
