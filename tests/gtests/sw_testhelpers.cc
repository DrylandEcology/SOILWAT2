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
#include "include/SW_Output.h"

#include "tests/gtests/sw_testhelpers.h"


SW_ALL template_SW_All;
SW_DOMAIN template_SW_Domain;
SW_OUTPUT_POINTERS template_SW_OutputPtrs[SW_OUTNKEYS];


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
    fprintf(stderr, "create_test_soillayers(): "
      "requested number of soil layers (n = %d) is not accepted.\n", nlayers);

    exit(-1);
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



void setup_SW_Site_for_tests(SW_SITE *SW_Site) {
    LOG_INFO LogInfo;
    sw_init_logs(NULL, &LogInfo); // Initialize logs and silence warn/error reporting

    SW_Site->deepdrain = swTRUE;

    SW_Site->_SWCMinVal = 100;
    SW_Site->_SWCWetVal = 15;
    SW_Site->_SWCInitVal = 15;

    SW_Site->stMaxDepth = 990;
    SW_Site->stDeltaX = 15;

    SW_Site->slow_drain_coeff = 0.02;

    SW_Site->site_has_swrcp = swFALSE;
    strcpy(SW_Site->site_swrc_name, (char *) "Campbell1974");
    SW_Site->site_swrc_type = encode_str2swrc(SW_Site->site_swrc_name, &LogInfo);
    strcpy(SW_Site->site_ptf_name, (char *) "Cosby1984AndOthers");
    SW_Site->site_ptf_type = encode_str2ptf(SW_Site->site_ptf_name);
}


/* Set up global variables for testing and read in values from SOILWAT2 example

  Prepares global variables `template_SW_Domain`, `template_SW_All`, and
  `template_SW_OutputPtrs`.

  The purpose is to read in text files once, and then have `AllTestFixture`
  create deep copies for each test.
*/
void setup_testGlobalSoilwatTemplate() {
    unsigned long userSUID;
    LOG_INFO LogInfo;

    // Initialize SOILWAT2 variables and read values from example input file
    sw_init_logs(NULL, &LogInfo);

    SW_DOM_init_ptrs(&template_SW_Domain);
    SW_CTL_init_ptrs(&template_SW_All);

    template_SW_Domain.PathInfo.InFiles[eFirst] = Str_Dup(DFLT_FIRSTFILE, &LogInfo);
    userSUID = 0; // 0 means no user input for suid, i.e., entire simulation domain

    SW_CTL_setup_domain(userSUID, &template_SW_Domain, &LogInfo);

    SW_CTL_setup_model(&template_SW_All, template_SW_OutputPtrs, &LogInfo);
    template_SW_All.Model.doOutput = swFALSE; /* turn off output during tests */
    SW_MDL_get_ModelRun(&template_SW_All.Model, &template_SW_Domain, NULL, &LogInfo);
    SW_CTL_alloc_outptrs(&template_SW_All, &LogInfo);  /* allocate memory for output pointers */
    SW_CTL_read_inputs_from_disk(&template_SW_All, &template_SW_Domain.PathInfo, &LogInfo);

    /* Notes on messages during tests
        - `SW_F_read()`, via SW_CTL_read_inputs_from_disk(), opens the file
        "example/Output/logfile.log" on disk (based on content of "files.in")
        - we close "Output/logfile.log"
        - we set `logfp` to NULL to silence all non-error messages during tests
        - error messages go directly to stderr (which DeathTests use to match against)
    */
    sw_wrapup_logs(&LogInfo);
    sw_fail_on_error(&LogInfo);
    sw_init_logs(NULL, &LogInfo);

    SW_WTH_finalize_all_weather(
        &template_SW_All.Markov,
        &template_SW_All.Weather,
        template_SW_All.Model.cum_monthdays,
        template_SW_All.Model.days_in_month,
        &LogInfo
    );
    sw_fail_on_error(&LogInfo);

    SW_CTL_init_run(&template_SW_All, &LogInfo);
    sw_fail_on_error(&LogInfo);

    SW_OUT_setup_output(
        template_SW_All.Site.n_layers,
        template_SW_All.Site.n_evap_lyrs,
        &template_SW_All.VegEstab,
        &template_SW_All.GenOutput,
        &LogInfo
    );
    sw_fail_on_error(&LogInfo);

}

/* Free allocated memory of global test variables
*/
void teardown_testGlobalSoilwatTemplate() {
    SW_DOM_deconstruct(&template_SW_Domain);
    SW_CTL_clear_model(swTRUE, &template_SW_All);
}
