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

#include "include/generic.h"
#include "include/filefuncs.h"
#include "include/SW_Files.h"
#include "include/myMemory.h"
#include "include/SW_Weather.h"
#include "include/SW_datastructs.h"

#include "tests/gtests/sw_testhelpers.h"


/* The unit test code is using the SOILWAT2-standalone input files from
   tests/example/ as example inputs.
   The paths are relative to the unit-test executable which is located at bin/
   of the SOILWAT2 repository
*/
const char * dir_test = "./tests/example";


/* Naming scheme for unit tests
   https://google.github.io/googletest/faq.html
   https://google.github.io/googletest/advanced.html

    - no underscore "_" in names of test suites or tests
    - non-death tests are identified by a test suit name that ends with "*Test"
    - death tests are identified by a test suit name that ends with "*DeathTest"
*/



int main(int argc, char **argv) {
  int res;
  /*--- Imitate 'SW_Main.c/main()' */

  // Emulate 'sw_init_args()'
  if (!ChDir(dir_test)) {
    swprintf("Invalid project directory (%s)", dir_test);
  }

  //--- Setup unit tests
  ::testing::InitGoogleTest(&argc, argv);

  // Set death tests to be "threadsafe" instead of "fast", i.e.,
  // "re-executes the binary to cause just the single death test under consideration to be run"
  // (https://google.github.io/googletest/reference/assertions.html#death)
  // See code example in header for `AllTestStruct`
  GTEST_FLAG_SET(death_test_style, "threadsafe");

  // Run unit tests
  res = RUN_ALL_TESTS();

  //--- Return output of 'RUN_ALL_TESTS()'
  // (https://google.github.io/googletest/primer.html#writing-the-main-function)
  return res;
}

