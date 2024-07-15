#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "include/filefuncs.h"
#include "include/generic.h"

#include "tests/gtests/sw_testhelpers.h"


/* The unit test code is using the SOILWAT2-standalone input files from
   tests/example/ as example inputs.
   The paths are relative to the unit-test executable which is located at bin/
   of the SOILWAT2 repository
*/
const char *dir_test = "./tests/example";

/* Naming scheme for unit tests
   https://google.github.io/googletest/faq.html
   https://google.github.io/googletest/advanced.html

    - no underscore "_" in names of test suites or tests
    - non-death tests are identified by a test suit name that ends with "*Test"
    - death tests are identified by a test suit name that ends with "*DeathTest"
*/


/* Error handling
   - Function calls that have a `LogInfo` argument and are expected to work
     without an error must immediately fail the test program by calling
     `sw_fail_on_error(&LogInfo)`.

   - Function calls that have a `LogInfo` argument and are expected to produce
     an error must clarify this in the comment and check the `stopRun` and/or
     `errorMsg` content of `LogInfo`.
*/


int main(int argc, char **argv) {
    int res;
    /*--- Imitate 'SW_Main.c/main()' */

    // Emulate 'sw_init_args()'
    if (!ChDir(dir_test)) {
        sw_printf("Invalid project directory (%s)", dir_test);
    }

    res = setup_testGlobalSoilwatTemplate();

    if (res != 0) {
        // Setup failed
        goto finishProgram; // Exit function prematurely due to error
    }


    //--- Setup unit tests
    ::testing::InitGoogleTest(&argc, argv);
    ::testing::InitGoogleMock(&argc, argv);

    // Set death tests to be "threadsafe" instead of "fast", i.e.,
    // "re-executes the binary to cause just the single death test under
    // consideration to be run"
    // (https://google.github.io/googletest/reference/assertions.html#death)
    // See code example in header for `AllTestStruct`
    GTEST_FLAG_SET(death_test_style, "threadsafe");

    // Run unit tests
    res = RUN_ALL_TESTS();


finishProgram:
    teardown_testGlobalSoilwatTemplate();

    //--- Return output of 'RUN_ALL_TESTS()'
    // (https://google.github.io/googletest/primer.html#writing-the-main-function)
    // It returns 0 if all tests are successful, or 1 otherwise.
    return res;
}
