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
    unsigned long userSUID;

    // `memcpy()` does not work for copying an initialized `SW_ALL`
    // because it does not copy dynamically allocated memory to which
    // members of `SW_ALL` point to
    void SetUp() override {
        // Initialize SOILWAT2 variables and read values from example input file
        sw_init_logs(NULL, &LogInfo);

        SW_F_init_ptrs(SW_Domain.PathInfo.InFiles);
        SW_CTL_init_ptrs(&SW_All);

        SW_Domain.PathInfo.InFiles[eFirst] = Str_Dup(DFLT_FIRSTFILE, &LogInfo);
        userSUID = 0; // 0 means no user input for suid, i.e., entire simulation domain

        SW_CTL_setup_domain(userSUID, &SW_Domain, &LogInfo);

        SW_CTL_setup_model(&SW_All, &SW_OutputPtrs, &LogInfo);
        SW_MDL_get_ModelRun(&SW_All.Model, &SW_Domain, NULL, &LogInfo);
        SW_CTL_alloc_outptrs(&SW_All, &LogInfo);  /* allocate memory for output pointers */
        SW_CTL_read_inputs_from_disk(&SW_All, &SW_Domain.PathInfo, &LogInfo);

        /* Notes on messages during tests
            - `SW_F_read()`, via SW_CTL_read_inputs_from_disk(), writes the file
            "example/Output/logfile.log" to disk (based on content of "files.in")
            - we close "Output/logfile.log"
            - we set `logfp` to NULL to silence all non-error messages during tests
            - error messages go directly to stderr (which DeathTests use to match against)
        */
        sw_fail_on_error(&LogInfo);
        sw_init_logs(NULL, &LogInfo);

        SW_WTH_finalize_all_weather(
            &SW_All.Markov,
            &SW_All.Weather,
            SW_All.Model.cum_monthdays,
            SW_All.Model.days_in_month,
            &LogInfo
        );

        SW_CTL_init_run(&SW_All, &LogInfo);
    }

    void TearDown() override {
      SW_F_deconstruct(SW_Domain.PathInfo.InFiles);
      SW_CTL_clear_model(swTRUE, &SW_All);
    }
};


using CarbonFixtureTest = AllTestFixture;

using SiteFixtureTest = AllTestFixture;

using VegEstabFixtureTest = AllTestFixture;

using VegProdFixtureTest = AllTestFixture;

using WeatherFixtureTest = AllTestFixture;

using WaterBalanceFixtureTest = AllTestFixture;
