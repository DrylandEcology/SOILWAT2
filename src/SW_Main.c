/********************************************************/
/********************************************************/
/*  Application: SOILWAT - soilwater dynamics simulator
 *  Source file: Main.c
 *  Type: main module
 *  Purpose: Contains the main loops and initializations.
 *
 06/24/2013	(rjm)	included "SW_Site.h" and "SW_Weather.h";
 added calls at end of main() to SW_SIT_clear_layers() and
 SW_WTH_clear_runavg_list() to free memory
 */
/********************************************************/
/********************************************************/

/* =================================================== */
/*                INCLUDES / DEFINES                   */
/* --------------------------------------------------- */
#include "include/filefuncs.h"      // for sw_message
#include "include/generic.h"        // for Bool, swFALSE, swTRUE
#include "include/SW_Control.h"     // for SW_CTL_RunSimSet, SW_CTL_clear_m...
#include "include/SW_datastructs.h" // for LOG_INFO, SW_RUN, SW_DOMAIN, SW_...
#include "include/SW_Domain.h"      // for SW_DOM_deconstruct, SW_DOM_init_...
#include "include/SW_Files.h"       // for eFirst
#include "include/SW_Main_lib.h"    // for sw_fail_on_error, sw_init_args
#include "include/SW_Model.h"       // for SW_MDL_get_ModelRun
#include "include/SW_Output.h"      // for SW_OUT_close_files, SW_OUT_creat...
#include "include/SW_Weather.h"     // for SW_WTH_finalize_all_weather
#include "include/Times.h"          // for SW_WT_ReportTime, SW_WT_StartTime
#include <errno.h>                  // for errno
#include <stdio.h>                  // for NULL, stdout


#if defined(SWNETCDF)
#include "include/SW_netCDF.h"
#endif


/* =================================================== */
/*             Local Function Definitions              */
/* --------------------------------------------------- */


/* =================================================== */
/*             Global Function Definitions             */
/* --------------------------------------------------- */

/************  Main() ************************/
int main(int argc, char **argv) {
    /* =================================================== */
    SW_WALLTIME SW_WallTime;
    SW_RUN sw_template;
    SW_DOMAIN SW_Domain;
    LOG_INFO LogInfo;
    Bool EchoInits = swFALSE;

    unsigned long userSUID;
    errno = 0;

    // Start overall wall time
    SW_WT_StartTime(&SW_WallTime);

    // Initialize logs and pointer objects
    sw_init_logs(stdout, &LogInfo);

    SW_DOM_init_ptrs(&SW_Domain);
    SW_CTL_init_ptrs(&sw_template);

    // Obtain user input from the command line
    sw_init_args(
        argc,
        argv,
        &EchoInits,
        &SW_Domain.PathInfo.InFiles[eFirst],
        &userSUID,
        &SW_WallTime.wallTimeLimit,
        &SW_Domain.netCDFInfo.renameDomainTemplateNC,
        &LogInfo
    );
    if (LogInfo.stopRun) {
        goto finishProgram;
    }

    // SOILWAT2: do print progress to console unless user requests quiet
    LogInfo.printProgressMsg = (Bool) (!LogInfo.QuietMode);

    if (LogInfo.printProgressMsg) {
        sw_message("started.");
        sw_print_version();
    }

    // setup and construct domain
    SW_CTL_setup_domain(userSUID, &SW_Domain, &LogInfo);
    if (LogInfo.stopRun) {
        goto finishProgram;
    }

    // setup and construct model template (independent of inputs)
    SW_CTL_setup_model(&sw_template, &SW_Domain.OutDom, swTRUE, &LogInfo);
    if (LogInfo.stopRun) {
        goto finishProgram;
    }


    SW_MDL_get_ModelRun(&sw_template.Model, &SW_Domain, NULL, &LogInfo);
    if (LogInfo.stopRun) {
        goto finishProgram;
    }

    // read user inputs
    SW_CTL_read_inputs_from_disk(
        &sw_template, &SW_Domain.OutDom, &SW_Domain.PathInfo, &LogInfo
    );
    if (LogInfo.stopRun) {
        goto finishProgram;
    }

#if defined(SWNETCDF)
    SW_NC_check_input_files(&SW_Domain, &LogInfo);
#endif

    // finalize daily weather
    SW_WTH_finalize_all_weather(
        &sw_template.Markov,
        &sw_template.Weather,
        sw_template.Model.cum_monthdays,
        sw_template.Model.days_in_month,
        &LogInfo
    );
    if (LogInfo.stopRun) {
        goto finishProgram;
    }

    // initialize simulation run (based on user inputs)
    SW_CTL_init_run(&sw_template, &LogInfo);
    if (LogInfo.stopRun) {
        goto finishProgram;
    }

    // identify domain-wide soil profile information
    SW_DOM_soilProfile(
        &SW_Domain.hasConsistentSoilLayerDepths,
        &SW_Domain.nMaxSoilLayers,
        &SW_Domain.nMaxEvapLayers,
        SW_Domain.depthsAllSoilLayers,
        sw_template.Site.n_layers,
        sw_template.Site.n_evap_lyrs,
        sw_template.Site.depths,
        &LogInfo
    );
    if (LogInfo.stopRun) {
        goto finishProgram;
    }

    // initialize output
    SW_OUT_setup_output(
        SW_Domain.nMaxSoilLayers,
        SW_Domain.nMaxEvapLayers,
        &sw_template.VegEstab,
        &SW_Domain.OutDom,
        &LogInfo
    );
    if (LogInfo.stopRun) {
        goto finishProgram;
    }

#if defined(SWNETCDF)
    SW_NC_read_out_vars(
        &SW_Domain.OutDom,
        SW_Domain.PathInfo.InFiles,
        sw_template.VegEstab.parms,
        &LogInfo
    );
    if (LogInfo.stopRun) {
        goto finishProgram;
    }
    SW_NC_create_units_converters(&SW_Domain.OutDom, &LogInfo);
    if (LogInfo.stopRun) {
        goto finishProgram;
    }
#endif // SWNETCDF

    SW_OUT_create_files(&sw_template.FileStatus, &SW_Domain, &LogInfo);
    if (LogInfo.stopRun) {
        goto closeFiles;
    }

    if (EchoInits) {
        _echo_all_inputs(&sw_template, &SW_Domain.OutDom);
    }

    // run simulations: loop over simulation set
    SW_CTL_RunSimSet(&sw_template, &SW_Domain, &SW_WallTime, &LogInfo);

closeFiles: {
    // finish-up output (not used with rSOILWAT2)
    SW_OUT_close_files(&sw_template.FileStatus, &SW_Domain.OutDom, &LogInfo);
}

finishProgram: {
    // de-allocate all memory
    SW_DOM_deconstruct(&SW_Domain); // Includes closing netCDF files if needed
    SW_CTL_clear_model(swTRUE, &sw_template);

    sw_write_warnings("(main) ", &LogInfo);
    SW_WT_ReportTime(SW_WallTime, &LogInfo);
    sw_wrapup_logs(&LogInfo);
    sw_fail_on_error(&LogInfo);
    if (LogInfo.printProgressMsg) {
        sw_message("ended.");
    }
}

    return 0;
}

/*********** End of Main() *******************/
