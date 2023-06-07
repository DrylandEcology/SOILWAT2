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
#include "include/SW_Site.h"
#include "include/SW_SoilWater.h"
#include "include/SW_Weather.h"
#include "include/SW_Control.h"
#include "include/SW_Files.h"
#include "include/myMemory.h"

#include "tests/gtests/sw_testhelpers.h"

const char * filemaster_test = "files.in";

/** Initialize SOILWAT2 variables and read values from example input file
 */
void Reset_SOILWAT2_after_UnitTest(void) {
  /*--- Imitate 'SW_Main.c/main()':
    we need to initialize and take down SOILWAT2 variables
    because SOILWAT2 uses (global) states.
    This is otherwise not comptable with the c++ approach used by googletest.
  */

  LogInfo.logged = swFALSE;
  LogInfo.logfp = NULL;

  Bool EchoInits = swFALSE;

  SW_CTL_clear_model(swFALSE, &SW_All, &PathInfo);

  memset(&SW_All, 0, sizeof(SW_ALL));

  PathInfo.InFiles[eFirst] = Str_Dup(filemaster_test, &LogInfo);

  SW_CTL_setup_model(&SW_All, &SW_OutputPtrs, &PathInfo, &LogInfo);
  SW_CTL_read_inputs_from_disk(&SW_All, &PathInfo, &LogInfo, EchoInits);

  /* Notes on messages during tests
    - `SW_F_read()`, via SW_CTL_read_inputs_from_disk(), writes the file
      "example/Output/logfile.log" to disk (based on content of "files.in")
    - we close "Output/logfile.log"
    - we set `logfp` to NULL to silence all non-error messages during tests
    - error messages go directly to stderr (which DeathTests use to match against)
  */

  LogInfo.logfp = NULL;

  SW_WTH_finalize_all_weather(&SW_All.Markov, &SW_All.Weather,
        SW_All.Model.cum_monthdays, SW_All.Model.days_in_month, &LogInfo);
  SW_CTL_init_run(&SW_All, &LogInfo, &PathInfo);


  // Next functions calls from `main()` require SW_Output.c
  //   (see issue #85 'Make SW_Output.c comptabile with c++ to include in unit testing code')
  // SW_OUT_set_ncol();
  // SW_OUT_set_colnames();
  // ...
}




void create_test_soillayers(unsigned int nlayers, char *InFiles[],
      SW_VEGPROD *SW_VegProd, SW_SITE *SW_Site) {

  if (nlayers <= 0 || nlayers > MAX_LAYERS) {
    LogError(&LogInfo, LOGFATAL, "create_test_soillayers(): "
      "requested number of soil layers (n = %d) is not accepted.\n", nlayers);
  }

  RealF dmax[MAX_LAYERS] = {
    5, 6, 10, 11, 12, 20, 21, 22, 25, 30, 40, 41, 42, 50, 51, 52, 53, 54, 55,
    60, 70, 80, 90, 110, 150};
  RealF bulkd[MAX_LAYERS] = {
    1.430, 1.410, 1.390, 1.390, 1.380, 1.150, 1.130, 1.130, 1.430, 1.410,
    1.390, 1.390, 1.380, 1.150, 1.130, 1.130, 1.430, 1.410, 1.390, 1.390,
    1.380, 1.150, 1.130, 1.130, 1.400};
  RealF f_gravel[MAX_LAYERS] = {
    0.1, 0.1, 0.1, 0.1, 0.2, 0.2, 0.2, 0.2, 0.2, 0.2, 0.2, 0.2, 0.2, 0.2, 0.2,
    0.2, 0.2, 0.2, 0.2, 0.2, 0.2, 0.2, 0.2, 0.2, 0.2};
  RealF evco[MAX_LAYERS] = {
    0.813, 0.153, 0.034, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0};
  RealF trco_grass[MAX_LAYERS] = {
    0.0158, 0.0155, 0.0314, 0.0314, 0.0314, 0.0624, 0.0624, 0.0624, 0.0155,
    0.0155, 0.0314, 0.0314, 0.0314, 0.0624, 0.0624, 0.0624, 0.0155, 0.0155,
    0.0314, 0.0314, 0.0314, 0.0624, 0.0624, 0.0624, 0.0625};
  RealF trco_shrub[MAX_LAYERS] = {
    0.0413, 0.0294, 0.055, 0.0547, 0.0344, 0.0341, 0.0316, 0.0316, 0.0419,
    0.0294, 0.055, 0.0547, 0.0344, 0.0341, 0.0316, 0.0316, 0.0419, 0.0294,
    0.0550, 0.0547, 0.0344, 0.0341, 0.0316, 0.0316, 0.0625};
  RealF trco_tree[MAX_LAYERS] = {
    0.0158, 0.0155, 0.0314, 0.0314, 0.0314, 0.0624, 0.0624, 0.0624, 0.0155,
    0.0155, 0.0314, 0.0314, 0.0314, 0.0624, 0.0624, 0.0624, 0.0155, 0.0155,
    0.0314, 0.0314, 0.0314, 0.0624, 0.0624, 0.0624, 0.0625};
  RealF trco_forb[MAX_LAYERS] = {
    0.0413, 0.0294, 0.055, 0.0547, 0.0344, 0.0341, 0.0316, 0.0316, 0.0419,
    0.0294, 0.055, 0.0547, 0.0344, 0.0341, 0.0316, 0.0316, 0.0419, 0.0294,
    0.0550, 0.0547, 0.0344, 0.0341, 0.0316, 0.0316, 0.0625};
  RealF psand[MAX_LAYERS] = {
    0.51, 0.44, 0.35, 0.32, 0.31, 0.32, 0.57, 0.57, 0.51, 0.44, 0.35, 0.32,
    0.31, 0.32, 0.57, 0.57, 0.51, 0.44, 0.35, 0.32, 0.31, 0.32, 0.57, 0.57,
    0.58};
  RealF pclay[MAX_LAYERS] = {
    0.15, 0.26, 0.41, 0.45, 0.47, 0.47, 0.28, 0.28, 0.15, 0.26, 0.41, 0.45,
    0.47, 0.47, 0.28, 0.28, 0.15, 0.26, 0.41, 0.45, 0.47, 0.47, 0.28, 0.28,
    0.29};
  RealF imperm[MAX_LAYERS] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  RealF soiltemp[MAX_LAYERS] = {
    -1, -1, -1, -1, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2,
    2};

  int nRegions = 3;
  RealD regionLowerBounds[3] = {20., 50., 100.};

  set_soillayers(SW_VegProd, SW_Site, &LogInfo, nlayers, dmax,
    bulkd, f_gravel, evco, trco_grass, trco_shrub, trco_tree,
    trco_forb, psand, pclay, imperm, soiltemp, nRegions,
    regionLowerBounds, InFiles);
}
