#include "include/generic.h"             // for swFALSE, swTRUE
#include "include/SW_Control.h"          // for SW_CTL_main
#include "include/SW_Main_lib.h"         // for sw_fail_on_error
#include "include/SW_VegEstab.h"         // for SW_VES_read2
#include "tests/gtests/sw_testhelpers.h" // for VegEstabFixtureTest
#include "gtest/gtest.h"                 // for Message, AssertionResult

namespace {
// Run a simulation with vegetation establishment turn on
TEST_F(VegEstabFixtureTest, SimulateWithVegEstab) {
    // Turn on vegetation establishment and process inputs (but ignore use flag)
    SW_VES_read2(
        &SW_Run.VegEstab,
        swTRUE,
        swFALSE,
        SW_Domain.SW_PathInputs.InFiles,
        SW_Domain.SW_PathInputs.SW_ProjDir,
        &LogInfo
    );

    // Expect that vegetation establishment is turn on and contains species
    EXPECT_TRUE(SW_Run.VegEstab.use);
    EXPECT_GT(SW_Run.VegEstab.count, 0);

    // Run the simulation
    SW_CTL_main(&SW_Run, &SW_Domain.OutDom, &LogInfo);
    sw_fail_on_error(&LogInfo); // exit test program if unexpected error


    // Expect valid 'day of year' 1-366 output for each species from the
    // vegetation establishment calculations
    // note: estab_doy == 0 means no establishment
    for (unsigned int i = 0; i < SW_Run.VegEstab.count; i++) {
        EXPECT_GE(SW_Run.VegEstab.parms[i]->estab_doy, 0);
        EXPECT_LE(SW_Run.VegEstab.parms[i]->estab_doy, 366);
    }
}
} // namespace
