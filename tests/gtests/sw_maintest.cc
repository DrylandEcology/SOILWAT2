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

#include "include/myMemory.h"
// externs `*logfp`, `errstr`, `logged`, `QuietMode`, `EchoInits`
#include "include/generic.h"
#include "include/filefuncs.h" // externs `_firstfile`, `inbuf`
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

#include "include/SW_Control.h"

#include "tests/gtests/sw_testhelpers.h"


/* The unit test code is using the SOILWAT2-standalone input files from
   tests/example/ as example inputs.
   The paths are relative to the unit-test executable which is located at bin/
   of the SOILWAT2 repository
*/
const char * dir_test = "./tests/example";
const char * masterfile_test = "files.in"; // relative to 'dir_test'


/* Naming scheme for unit tests
   https://google.github.io/googletest/faq.html
   https://google.github.io/googletest/advanced.html

    - no underscore "_" in names of test suites or tests
    - non-death tests are identified by a test suit name that ends with "*Test"
    - death tests are identified by a test suit name that ends with "*DeathTest"
*/



int main(int argc, char **argv) {
  int res;
  /*--- Imitate 'SW_Main.c/main()':
    we need to initialize and take down SOILWAT2 variables
    because SOILWAT2 uses (global) states.
    This is otherwise not comptable with the c++ approach used by googletest.
  */
  logged = swFALSE;
  logfp = stdout;

  // Emulate 'sw_init_args()'
  if (!ChDir(dir_test)) {
    swprintf("Invalid project directory (%s)", dir_test);
  }
  strcpy(_firstfile, masterfile_test);
  QuietMode = swTRUE;
  EchoInits = swFALSE;

  // Initialize SOILWAT2 variables and read values from example input file
  Reset_SOILWAT2_after_UnitTest();


  //--- Setup unit tests
  ::testing::InitGoogleTest(&argc, argv);

  // set death tests to be "threadsafe" instead of "fast"
  GTEST_FLAG_SET(death_test_style, "threadsafe");

  // Run unit tests
  res = RUN_ALL_TESTS();


  //--- Take down SOILWAT2 variables
  SW_CTL_clear_model(swTRUE); // de-allocate all memory

  //--- Return output of 'RUN_ALL_TESTS()', see https://github.com/google/googletest/blob/master/googletest/docs/FAQ.md#my-compiler-complains-about-ignoring-return-value-when-i-call-run_all_tests-why
  return res;
}
