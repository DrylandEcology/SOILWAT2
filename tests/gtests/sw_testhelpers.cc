
#include "tests/gtests/sw_testhelpers.h"
#include "include/generic.h"    // for swFALSE, swTRUE
#include "include/myMemory.h"   // for Str_Dup
#include "include/SW_Control.h" // for SW_CTL_clear_m...
#include "include/SW_Files.h"   // for eFirst
#include "include/SW_Model.h"   // for SW_MDL_get_ModelRun
#include "include/SW_Output.h"  // for SW_OUT_setup_output
#include "include/SW_Site.h"    // for encode_str2ptf, encode_str2swrc, set...
#include "include/SW_Weather.h" // for SW_WTH_finalize_all_weather
#include <stdio.h>              // for NULL, fprintf, stderr
#include <stdlib.h>             // for exit
#include <string.h>             // for strcpy


// NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables)
SW_RUN template_SW_Run;
SW_DOMAIN template_SW_Domain;

// NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables)

/**
@brief Creates soil layers based on function arguments (instead of reading
them from an input file as _read_layers() does)

For details, see set_soillayers().

@note
Soil moisture values must be properly initialized before running a
simulation after this function has set soil layers, e.g., SW_SWC_init_run()
*/
void create_test_soillayers(
    unsigned int nlayers,
    SW_VEGPROD *SW_VegProd,
    SW_SITE *SW_Site,
    LOG_INFO *LogInfo
) {

    if (nlayers <= 0 || nlayers > MAX_LAYERS) {
        (void) fprintf(
            stderr,
            "create_test_soillayers(): "
            "requested number of soil layers (n = %d) is not accepted.\n",
            nlayers
        );

        exit(-1);
    }

    double dmax[MAX_LAYERS] = {5,  6,  10, 11, 12, 20,  21, 22, 25,
                               30, 40, 41, 42, 50, 51,  52, 53, 54,
                               55, 60, 70, 80, 90, 110, 150};
    double bulkd[MAX_LAYERS] = {1.430, 1.410, 1.390, 1.390, 1.380, 1.150, 1.130,
                                1.130, 1.430, 1.410, 1.390, 1.390, 1.380, 1.150,
                                1.130, 1.130, 1.430, 1.410, 1.390, 1.390, 1.380,
                                1.150, 1.130, 1.130, 1.400};
    double f_gravel[MAX_LAYERS] = {0.1, 0.1, 0.1, 0.1, 0.2, 0.2, 0.2, 0.2, 0.2,
                                   0.2, 0.2, 0.2, 0.2, 0.2, 0.2, 0.2, 0.2, 0.2,
                                   0.2, 0.2, 0.2, 0.2, 0.2, 0.2, 0.2};
    double evco[MAX_LAYERS] = {0.813, 0.153, 0.034, 0, 0, 0, 0, 0, 0,
                               0,     0,     0,     0, 0, 0, 0, 0, 0,
                               0,     0,     0,     0, 0, 0, 0};
    double trco_grass[MAX_LAYERS] = {0.0158, 0.0155, 0.0314, 0.0314, 0.0314,
                                     0.0624, 0.0624, 0.0624, 0.0155, 0.0155,
                                     0.0314, 0.0314, 0.0314, 0.0624, 0.0624,
                                     0.0624, 0.0155, 0.0155, 0.0314, 0.0314,
                                     0.0314, 0.0624, 0.0624, 0.0624, 0.0625};
    double trco_shrub[MAX_LAYERS] = {0.0413, 0.0294, 0.055,  0.0547, 0.0344,
                                     0.0341, 0.0316, 0.0316, 0.0419, 0.0294,
                                     0.055,  0.0547, 0.0344, 0.0341, 0.0316,
                                     0.0316, 0.0419, 0.0294, 0.0550, 0.0547,
                                     0.0344, 0.0341, 0.0316, 0.0316, 0.0625};
    double trco_tree[MAX_LAYERS] = {0.0158, 0.0155, 0.0314, 0.0314, 0.0314,
                                    0.0624, 0.0624, 0.0624, 0.0155, 0.0155,
                                    0.0314, 0.0314, 0.0314, 0.0624, 0.0624,
                                    0.0624, 0.0155, 0.0155, 0.0314, 0.0314,
                                    0.0314, 0.0624, 0.0624, 0.0624, 0.0625};
    double trco_forb[MAX_LAYERS] = {0.0413, 0.0294, 0.055,  0.0547, 0.0344,
                                    0.0341, 0.0316, 0.0316, 0.0419, 0.0294,
                                    0.055,  0.0547, 0.0344, 0.0341, 0.0316,
                                    0.0316, 0.0419, 0.0294, 0.0550, 0.0547,
                                    0.0344, 0.0341, 0.0316, 0.0316, 0.0625};
    double psand[MAX_LAYERS] = {0.51, 0.44, 0.35, 0.32, 0.31, 0.32, 0.57,
                                0.57, 0.51, 0.44, 0.35, 0.32, 0.31, 0.32,
                                0.57, 0.57, 0.51, 0.44, 0.35, 0.32, 0.31,
                                0.32, 0.57, 0.57, 0.58};
    double pclay[MAX_LAYERS] = {0.15, 0.26, 0.41, 0.45, 0.47, 0.47, 0.28,
                                0.28, 0.15, 0.26, 0.41, 0.45, 0.47, 0.47,
                                0.28, 0.28, 0.15, 0.26, 0.41, 0.45, 0.47,
                                0.47, 0.28, 0.28, 0.29};
    double imperm[MAX_LAYERS] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                                 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    double soiltemp[MAX_LAYERS] = {-1, -1, -1, -1, 0, 0, 1, 1, 1, 1, 1, 1, 1,
                                   1,  1,  1,  2,  2, 2, 2, 2, 2, 2, 2, 2};
    double om[MAX_LAYERS] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                             0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

    int const nRegions = 3;
    double regionLowerBounds[3] = {20., 50., 100.};

    set_soillayers(
        SW_VegProd,
        SW_Site,
        nlayers,
        dmax,
        bulkd,
        f_gravel,
        evco,
        trco_grass,
        trco_shrub,
        trco_tree,
        trco_forb,
        psand,
        pclay,
        imperm,
        soiltemp,
        om,
        nRegions,
        regionLowerBounds,
        LogInfo
    );
}

void setup_SW_Site_for_tests(SW_SITE *SW_Site) {
    LOG_INFO LogInfo;
    // Initialize logs and silence warn/error reporting
    sw_init_logs(NULL, &LogInfo);

    SW_Site->deepdrain = swTRUE;

    SW_Site->SWCMinVal = 100;
    SW_Site->SWCWetVal = 15;
    SW_Site->SWCInitVal = 15;

    SW_Site->stMaxDepth = 990;
    SW_Site->stDeltaX = 15;

    SW_Site->slow_drain_coeff = 0.02;

    SW_Site->site_has_swrcpMineralSoil = swFALSE;
    SW_Site->inputsProvideSWRCp = swFALSE;

    (void) snprintf(
        SW_Site->site_swrc_name,
        sizeof SW_Site->site_swrc_name,
        "%s",
        "Campbell1974"
    );
    SW_Site->site_swrc_type =
        encode_str2swrc(SW_Site->site_swrc_name, &LogInfo);
    (void) snprintf(
        SW_Site->site_ptf_name,
        sizeof SW_Site->site_ptf_name,
        "%s",
        "Cosby1984AndOthers"
    );
    SW_Site->site_ptf_type = encode_str2ptf(SW_Site->site_ptf_name);

    SW_Site->swrcpOM[0][0] = 1.03;
    SW_Site->swrcpOM[1][0] = 1.01;

    SW_Site->swrcpOM[0][1] = 0.93;
    SW_Site->swrcpOM[1][1] = 0.83;

    SW_Site->swrcpOM[0][2] = 2.7;
    SW_Site->swrcpOM[1][2] = 12.0;

    SW_Site->swrcpOM[0][3] = 2419.2;
    SW_Site->swrcpOM[1][3] = 0.864;
}

/* Set up global variables for testing and read in values from SOILWAT2 example

  Prepares global variables `template_SW_Domain`, `template_SW_Run`.

  The purpose is to read in text files once, and then have `AllTestFixture`
  create deep copies for each test.

  @return 0 if successful; 1 otherwise
  (i.e., same behavior as `RUN_ALL_TESTS()`)
*/
int setup_testGlobalSoilwatTemplate() {
    int success = 0;
    unsigned long userSUID;
    LOG_INFO LogInfo;
    const Bool renameDomainTemplateNC = swTRUE;
    const Bool estVeg = swTRUE;

    // Initialize SOILWAT2 variables and read values from example input file
    sw_init_logs(NULL, &LogInfo);

    SW_DOM_init_ptrs(&template_SW_Domain);
    SW_CTL_init_ptrs(&template_SW_Run);

    template_SW_Domain.SW_PathInputs.txtInFiles[eFirst] =
        Str_Dup(DFLT_FIRSTFILE, &LogInfo);
    if (LogInfo.stopRun != 0u) {
        goto finishProgram;
    }

    // userSUID: 0 means no user input for suid, i.e., entire simulation domain
    userSUID = 0;

    SW_CTL_setup_domain(
        userSUID, renameDomainTemplateNC, &template_SW_Domain, &LogInfo
    );
    if (LogInfo.stopRun != 0u) {
        goto finishProgram;
    }

#if defined(SWNETCDF)
    /* Turn off weather inputs if SWNETCDF and nc-weather is enabled */
    if (template_SW_Domain.netCDFInput.readInVars[eSW_InWeather][0]) {
        template_SW_Domain.netCDFInput.readInVars[eSW_InWeather][0] = swFALSE;
    }
#endif

    SW_CTL_setup_model(
        &template_SW_Run, &template_SW_Domain.OutDom, swTRUE, &LogInfo
    );
    if (LogInfo.stopRun != 0u) {
        goto finishProgram;
    }
    template_SW_Run.Model.doOutput = swFALSE; /* turn off output during tests */

    SW_MDL_get_ModelRun(
        &template_SW_Run.Model, &template_SW_Domain, NULL, &LogInfo
    );
    if (LogInfo.stopRun != 0u) {
        goto finishProgram;
    }

    SW_CTL_read_inputs_from_disk(
        &template_SW_Run,
        &template_SW_Domain,
        &template_SW_Domain.hasConsistentSoilLayerDepths,
        &LogInfo
    );
    if (LogInfo.stopRun != 0u) {
        goto finishProgram;
    }

    /* Notes on messages during tests
        - `SW_F_read()`, via SW_CTL_read_inputs_from_disk(), opens the file
        "example/Output/logfile.log" on disk (based on content of "files.in")
        - we close "Output/logfile.log"
        - we set `logfp` to NULL to silence all non-error messages during tests
        - error messages go directly to stderr (which DeathTests use to match
       against)
    */
    sw_wrapup_logs(&LogInfo);
    sw_init_logs(NULL, &LogInfo);

    SW_WTH_finalize_all_weather(
        &template_SW_Run.Markov,
        &template_SW_Run.WeatherIn,
        template_SW_Run.Model.cum_monthdays,
        template_SW_Run.Model.days_in_month,
        &LogInfo
    );
    if (LogInfo.stopRun != 0u) {
        goto finishProgram;
    }

    SW_CTL_init_run(&template_SW_Run, estVeg, &LogInfo);
    if (LogInfo.stopRun != 0u) {
        goto finishProgram;
    }

    SW_OUT_setup_output(
        template_SW_Run.Site.n_layers,
        template_SW_Run.Site.n_evap_lyrs,
        &template_SW_Run.VegEstab,
        &template_SW_Domain.OutDom,
        &LogInfo
    );
    if (LogInfo.stopRun != 0u) {
        goto finishProgram;
    }

finishProgram: {
    if (LogInfo.stopRun != 0u) {
        success = 1; // failure
    }
}
    return success;
}

/* Free allocated memory of global test variables
 */
void teardown_testGlobalSoilwatTemplate() {
    SW_DOM_deconstruct(&template_SW_Domain);
    SW_CTL_clear_model(swTRUE, &template_SW_Run);
}
