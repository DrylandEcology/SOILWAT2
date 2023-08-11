#include "gtest/gtest.h"

#include "include/generic.h" // for `Bool`, `swTRUE`, `swFALSE`
#include "include/SW_Control.h" // for `SW_CTL_main()`
#include "include/SW_VegEstab.h" // for `SW_VegEstab`, `SW_VES_read2()`
#include "include/SW_SoilWater.h"

#include "tests/gtests/sw_testhelpers.h" // for `Reset_SOILWAT2_after_UnitTest()`



namespace {
  // Run a simulation with vegetation establishment turn on
  TEST_F(VegEstabFixtureTest, SimulateWithVegEstab) {
    // Turn on vegetation establishment and process inputs (but ignore use flag)
    SW_VES_read2(&SW_All.VegEstab, swTRUE, swFALSE, PathInfo.InFiles,
                 PathInfo._ProjDir, &LogInfo);

    // Expect that vegetation establishment is turn on and contains species
    EXPECT_TRUE(SW_All.VegEstab.use);
    EXPECT_GT(SW_All.VegEstab.count, 0);

    // Run the simulation
    SW_CTL_main(&SW_All, &SW_OutputPtrs, &LogInfo);

    // Expect valid 'day of year' 1-366 output for each species from the
    // vegetation establishment calculations
    // note: estab_doy == 0 means no establishment
    for (unsigned int i = 0; i < SW_All.VegEstab.count; i++) {
      EXPECT_GE(SW_All.VegEstab.parms[i]->estab_doy, 0);
      EXPECT_LE(SW_All.VegEstab.parms[i]->estab_doy, 366);
    }
  }

} // namespace
