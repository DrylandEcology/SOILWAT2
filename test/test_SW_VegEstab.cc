#include "gtest/gtest.h"

#include "../generic.h" // for `Bool`, `swTRUE`, `swFALSE`
#include "../SW_Control.h" // for `SW_CTL_main()`
#include "../SW_VegEstab.h" // for `SW_VegEstab`, `SW_VES_read2()`

#include "sw_testhelpers.h" // for `Reset_SOILWAT2_after_UnitTest()`



namespace {
  // Run a simulation with vegetation establishment turn on
  TEST(VegEstabTest, SimulateWithVegEstab) {
    // Turn on vegetation establishment and process inputs (but ignore use flag)
    SW_VES_read2(swTRUE, swFALSE);

    // Expect that vegetation establishment is turn on and contains species
    EXPECT_TRUE(SW_VegEstab.use);
    EXPECT_GT(SW_VegEstab.count, 0);

    // Run the simulation
    SW_CTL_main();

    // Expect valid 'day of year' 1-366 output for each species from the
    // vegetation establishment calculations
    // note: estab_doy == 0 means no establishment
    for (unsigned int i = 0; i < SW_VegEstab.count; i++) {
      EXPECT_GE(SW_VegEstab.parms[i]->estab_doy, 0);
      EXPECT_LE(SW_VegEstab.parms[i]->estab_doy, 366);
    }

    // Reset to previous global state
    Reset_SOILWAT2_after_UnitTest();
  }

} // namespace
