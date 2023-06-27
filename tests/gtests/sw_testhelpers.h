#include <cmath>
#include "gtest/gtest.h"
#include "include/SW_datastructs.h"
#include "include/SW_Control.h"
#include "include/SW_Weather.h"
#include "include/myMemory.h"
#include "include/SW_Files.h"
#include "include/SW_Main_lib.h"


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


class AllTest : public::testing::Test {
  protected:

    SW_ALL SW_All;
    PATH_INFO PathInfo;
    LOG_INFO LogInfo;
    SW_OUTPUT_POINTERS SW_OutputPtrs;

    void SetUp() override {
      // `memcpy()` does not work to copy from a global to local attributes
      // since the function does not copy dynamically allocated memory

      Bool QuietMode = swFALSE;

      // Initialize SOILWAT2 variables and read values from example input file
      LogInfo.logged = swFALSE;
      LogInfo.logfp = NULL;

      PathInfo.InFiles[eFirst] = Str_Dup(DFLT_FIRSTFILE, &LogInfo);

      SW_CTL_setup_model(&SW_All, &SW_OutputPtrs, &PathInfo, &LogInfo);
      SW_CTL_read_inputs_from_disk(&SW_All, &PathInfo, &LogInfo);

      /* Notes on messages during tests
        - `SW_F_read()`, via SW_CTL_read_inputs_from_disk(), writes the file
          "example/Output/logfile.log" to disk (based on content of "files.in")
        - we close "Output/logfile.log"
        - we set `logfp` to NULL to silence all non-error messages during tests
        - error messages go directly to stderr (which DeathTests use to match against)
      */
      sw_check_log(QuietMode, &LogInfo);
      LogInfo.logfp = NULL;

      SW_WTH_finalize_all_weather(&SW_All.Markov, &SW_All.Weather,
            SW_All.Model.cum_monthdays, SW_All.Model.days_in_month, &LogInfo);
      SW_CTL_init_run(&SW_All, &PathInfo, &LogInfo);
    }

    void TearDown() override {
      SW_CTL_clear_model(swTRUE, &SW_All, &PathInfo);
    }
};

/* Functions for unit tests */

void Reset_SOILWAT2_after_UnitTest(void);

void create_test_soillayers(unsigned int nlayers, char *InFiles[],
      SW_VEGPROD *SW_VegProd, SW_SITE *SW_Site, LOG_INFO *LogInfo);
