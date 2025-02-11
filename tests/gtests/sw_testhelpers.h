#include "include/generic.h"        // for EQ, swTRUE
#include "include/SW_Control.h"     // for SW_RUN_deepCopy, SW_CTL_clear_model
#include "include/SW_datastructs.h" // for LOG_INFO, SW_RUN, SW_DOMAIN, SW_...
#include "include/SW_Defines.h"     // for SW_OUTNKEYS, SW_MISSING
#include "include/SW_Domain.h"      // for SW_DOM_deconstruct, SW_DOM_deepCopy
#include "include/SW_Main_lib.h"    // for sw_fail_on_error, sw_init_logs
#include "gtest/gtest.h"            // for Test
#include <string.h>                 // for memcpy, NULL


extern SW_RUN template_SW_Run;
extern SW_DOMAIN template_SW_Domain;


// get length of an array
#define sw_length(array) (sizeof(array) / sizeof(*(array)))

static const double tol0 = 1e-0, tol1 = 1e-1, tol2 = 1e-2, tol3 = 1e-3,
                    tol6 = 1e-6, tol9 = 1e-9;


/* SOILWAT2's macro `missing` uses `isfinite` which is C99; however,
   unit tests are compiled with C++ and the corresponding
   `std::isfinite` is C++11
   -> avoiding "error: use of undeclared identifier 'isfinite';
      did you mean 'std::isfinite'?"
*/
#undef missing
#define missing(x) (EQ(fabs((x)), SW_MISSING) || !std::isfinite((x)))


/* Functions for tests */

void create_test_soillayers(
    unsigned int nlayers,
    SW_VEGPROD_INPUTS *SW_VegProdIn,
    SW_SITE *SW_Site,
    LOG_INFO *LogInfo
);

void setup_SW_Site_for_tests(SW_SITE *SW_Site);

int setup_testGlobalSoilwatTemplate();
void teardown_testGlobalSoilwatTemplate();

/* AllTestFixture is our base test fixture class inheriting from
 * `::testing::Test` */
/* Note: don't use text fixtures with death tests in thread-safe mode,
   use class `AllTestStruct` (inside the death assertion) instead (see below).
   This is because each thread-safe death assertion is run from scratch and
   any code inside the death test and before the `EXPECT_DEATH` assertion is
   run twice (including setting up of a test fixture).
   See https://google.github.io/googletest/reference/assertions.html#death
*/
class AllTestFixture : public ::testing::Test {
  protected:
    SW_RUN SW_Run;
    SW_DOMAIN SW_Domain;
    LOG_INFO LogInfo;

    // Deep copy global test variables
    // (that were set up by `setup_testGlobalSoilwatTemplate()`) to
    // test fixture local variables
    void SetUp() override {
        sw_init_logs(NULL, &LogInfo);

        SW_DOM_deepCopy(&template_SW_Domain, &SW_Domain, &LogInfo);
        sw_fail_on_error(&LogInfo);

        SW_RUN_deepCopy(
            &template_SW_Run,
            &SW_Run,
            &template_SW_Domain.OutDom,
            swTRUE,
            &LogInfo
        );
        sw_fail_on_error(&LogInfo);
    }

    // Free allocated memory in test fixture local variables
    void TearDown() override {
        SW_DOM_deconstruct(&SW_Domain);
        SW_CTL_clear_model(swTRUE, &SW_Run);
    }
};

using CarbonFixtureTest = AllTestFixture;

using SiteFixtureTest = AllTestFixture;

using VegEstabFixtureTest = AllTestFixture;

using VegProdFixtureTest = AllTestFixture;

using WeatherFixtureTest = AllTestFixture;

using WaterBalanceFixtureTest = AllTestFixture;

using SpinUpFixtureTest = AllTestFixture;
