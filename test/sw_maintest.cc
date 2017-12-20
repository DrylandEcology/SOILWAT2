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

#include "../generic.h"
#include "../myMemory.h"
#include "../filefuncs.h"
#include "../rands.h"
#include "../Times.h"
#include "../SW_Defines.h"
#include "../SW_Times.h"
#include "../SW_Files.h"
#include "../SW_Carbon.h"
#include "../SW_Site.h"
#include "../SW_VegProd.h"
#include "../SW_VegEstab.h"
#include "../SW_Model.h"
#include "../SW_SoilWater.h"
#include "../SW_Weather.h"
#include "../SW_Markov.h"
#include "../SW_Sky.h"

#include "../SW_Control.h"

#include "sw_testhelpers.h"


/* The unit test code is using the SOILWAT2-standalone input files from testing/ as
   example input.
   The paths are relative to the unit-test executable which is located at the top level
   of the SOILWAT2 repository
*/
const char * dir_test = "./testing";
const char * masterfile_test = "files.in"; // relative to 'dir_test'


// Global variables which are defined in SW_Main_lib.c:
extern char errstr[];
extern FILE *logfp;
extern int logged;
extern Bool QuietMode, EchoInits;
extern char _firstfile[];



int main(int argc, char **argv) {
  int res;
  /*--- Imitate 'SW_Main.c/main()': we need to initialize and take down SOILWAT2 variables
    because SOILWAT2 uses (global) states. This is otherwise not comptable with the c++
    approach used by googletest.
  */
  logged = FALSE;
  logfp = stdout;

  // Emulate 'init_args()'
  if (!ChDir(dir_test)) {
    swprintf("Invalid project directory (%s)", dir_test);
  }
  strcpy(_firstfile, masterfile_test);
  QuietMode = TRUE;
  EchoInits = FALSE;

  // Initialize SOILWAT2 variables and read values from example input file
  Reset_SOILWAT2_after_UnitTest();

  //--- Setup and call unit tests
  ::testing::InitGoogleTest(&argc, argv);
  res = RUN_ALL_TESTS();

  //--- Take down SOILWAT2 variables
  SW_SIT_clear_layers();
  SW_WTH_clear_runavg_list();

  //--- Return output of 'RUN_ALL_TESTS()', see https://github.com/google/googletest/blob/master/googletest/docs/FAQ.md#my-compiler-complains-about-ignoring-return-value-when-i-call-run_all_tests-why
  return res;
}

