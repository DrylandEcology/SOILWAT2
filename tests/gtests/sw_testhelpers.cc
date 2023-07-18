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

#include "include/SW_Control.h"
#include "include/SW_Files.h"
#include "include/myMemory.h"

#include "tests/gtests/sw_testhelpers.h"


/**
  @brief Creates soil layers based on function arguments (instead of reading
    them from an input file as _read_layers() does)

  For details, see set_soillayers().

  @note
    - Soil moisture values must be properly initialized before running a
      simulation after this function has set soil layers, e.g.,
      SW_SWC_init_run()
*/
void create_test_soillayers(unsigned int nlayers,
      SW_VEGPROD *SW_VegProd, SW_SITE *SW_Site, LOG_INFO *LogInfo) {

  if (nlayers <= 0 || nlayers > MAX_LAYERS) {
    LogError(LogInfo, LOGFATAL, "create_test_soillayers(): "
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

  set_soillayers(SW_VegProd, SW_Site, nlayers, dmax,
    bulkd, f_gravel, evco, trco_grass, trco_shrub, trco_tree,
    trco_forb, psand, pclay, imperm, soiltemp, nRegions,
    regionLowerBounds, LogInfo);
}
