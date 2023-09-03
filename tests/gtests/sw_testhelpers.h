#include <cmath>
#include "gtest/gtest.h"
#include "include/SW_datastructs.h"
#include "include/SW_Defines.h"
#include "include/SW_Control.h"



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

void init_silent_tests(LOG_INFO* LogInfo);

void create_test_soillayers(unsigned int nlayers,
      SW_VEGPROD *SW_VegProd, SW_SITE *SW_Site, LOG_INFO *LogInfo);

void setup_SW_Site_for_tests(SW_SITE *SW_Site);

void setup_AllTest_for_tests(
  SW_ALL *SW_All,
  PATH_INFO *PathInfo,
  LOG_INFO *LogInfo,
  SW_OUTPUT_POINTERS *SW_OutputPtrs
);


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
    PATH_INFO PathInfo;
    LOG_INFO LogInfo;
    SW_OUTPUT_POINTERS SW_OutputPtrs;

    void SetUp() override {
      setup_AllTest_for_tests(&SW_All, &PathInfo, &LogInfo, &SW_OutputPtrs);
    }

    void TearDown() override {
      SW_CTL_clear_model(swTRUE, &SW_All, &PathInfo);
    }
};


using CarbonFixtureTest = AllTestFixture;

using SiteFixtureTest = AllTestFixture;

using VegEstabFixtureTest = AllTestFixture;

using VegProdFixtureTest = AllTestFixture;

using WeatherFixtureTest = AllTestFixture;

using WaterBalanceFixtureTest = AllTestFixture;



/* AllTestStruct is like AllTestFixture but does not inherit from `::testing::Test` */

/* Note: Use class `AllTestStruct` for thread-safe death tests inside the
   assertion itself -- otherwise, multiple instances will be created
   see https://google.github.io/googletest/reference/assertions.html#death

   Example code that avoids the creation of multiple instances:
   ```
   TEST(SomeDeathTest, TestName) {
       ...; // code here will be run twice

       EXPECT_DEATH_IF_SUPPORTED({
               // code inside `EXPECT_DEATH` is run once
               AllTestStruct sw = AllTestStruct();
               ...
               function_that_should_fail(...);
           },
           "Expected failure message."
       )
   }
   ```
*/
class AllTestStruct {
  public:

    SW_ALL SW_All;
    PATH_INFO PathInfo;
    LOG_INFO LogInfo;
    SW_OUTPUT_POINTERS SW_OutputPtrs;

    AllTestStruct() {
      setup_AllTest_for_tests(&SW_All, &PathInfo, &LogInfo, &SW_OutputPtrs);
    }

    ~AllTestStruct() {
      SW_CTL_clear_model(swTRUE, &SW_All, &PathInfo);
    }
};
