#include <cmath>
#include "gtest/gtest.h"
#include "include/SW_datastructs.h"
#include "include/SW_Defines.h"
#include "include/SW_Control.h"
#include "include/SW_Main_lib.h"
#include "include/SW_Files.h"
#include "include/myMemory.h"
#include "include/SW_Weather.h"
#include "include/SW_Model.h"
#include "include/SW_Domain.h"


extern SW_ALL template_SW_All;
extern SW_DOMAIN template_SW_Domain;
extern SW_OUTPUT_POINTERS template_SW_OutputPtrs;


#define length(array) (sizeof(array) / sizeof(*(array))) //get length of an array

static const double
  tol0 = 1e-0, tol1 = 1e-1, tol2 = 1e-2, tol3 = 1e-3, tol6 = 1e-6, tol9 = 1e-9;


/* SOILWAT2's macro `missing` uses `isfinite` which is C99; however,
   unit tests are compiled with C++ and the corresponding
   `std::isfinite` is C++11
   -> avoiding "error: use of undeclared identifier 'isfinite';
      did you mean 'std::isfinite'?"
*/
#undef missing
#define missing(x)  ( EQ( fabs( (x) ), SW_MISSING ) || !std::isfinite( (x) ) )


/* Functions for tests */

void create_test_soillayers(unsigned int nlayers,
      SW_VEGPROD *SW_VegProd, SW_SITE *SW_Site, LOG_INFO *LogInfo);

void setup_SW_Site_for_tests(SW_SITE *SW_Site);

void setup_testGlobalSoilwatTemplate();
void teardown_testGlobalSoilwatTemplate();


/* AllTestFixture is our base test fixture class inheriting from `::testing::Test` */
/* Note: don't use text fixtures with death tests in thread-safe mode,
   use class `AllTestStruct` (inside the death assertion) instead (see below).
   This is because each thread-safe death assertion is run from scratch and
   any code inside the death test and before the `EXPECT_DEATH` assertion is
   run twice (including setting up of a test fixture).
   See https://google.github.io/googletest/reference/assertions.html#death
*/
class AllTestFixture : public ::testing::Test {
  protected:

    SW_ALL SW_All;
    SW_DOMAIN SW_Domain;
    LOG_INFO LogInfo;
    SW_OUTPUT_POINTERS SW_OutputPtrs;

    // Deep copy global test variables
    // (that were set up by `setup_testGlobalSoilwatTemplate()`) to
    // test fixture local variables
    void SetUp() override {
        sw_init_logs(NULL, &LogInfo);

        memcpy(&SW_OutputPtrs, &template_SW_OutputPtrs, sizeof (SW_OutputPtrs));

        SW_DOM_deepCopy(&template_SW_Domain, &SW_Domain, &LogInfo);
        sw_fail_on_error(&LogInfo);

        SW_ALL_deepCopy(&template_SW_All, &SW_All, &LogInfo);
        sw_fail_on_error(&LogInfo);
    }

    // Free allocated memory in test fixture local variables
    void TearDown() override {
        SW_DOM_deconstruct(&SW_Domain);
        SW_CTL_clear_model(swTRUE, &SW_All);
    }
};


using CarbonFixtureTest = AllTestFixture;

using SiteFixtureTest = AllTestFixture;

using VegEstabFixtureTest = AllTestFixture;

using VegProdFixtureTest = AllTestFixture;

using WeatherFixtureTest = AllTestFixture;

using WaterBalanceFixtureTest = AllTestFixture;

using SpinUpTest = AllTestFixture;