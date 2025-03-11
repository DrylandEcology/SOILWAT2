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
#include <stdio.h>                  // for NULL, stdout


#if defined(SWNETCDF)
#include "include/SW_netCDF_General.h"
#include "include/SW_netCDF_Input.h"
#include "include/SW_netCDF_Output.h"
#endif

#if defined(SWMPI)
#include "include/SW_MPI.h"
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
    Bool renameDomainTemplateNC = swFALSE;
    Bool prepareFiles = swFALSE;

    int rank = 0;
    int size = 0;

#if defined(SWMPI)
    char procName[FILENAME_MAX] = "\0";
    MPI_Datatype datatypes[SW_MPI_NTYPES];
#endif

    unsigned long userSUID;

    // Start overall wall time
    SW_WT_StartTime(&SW_WallTime);

#if defined(SWMPI)
    SW_MPI_initialize(&argc, &argv, &rank, &size, procName);
#endif

    // Initialize logs and pointer objects
    sw_init_logs(stdout, &LogInfo);

    SW_DOM_init_ptrs(&SW_Domain);
    SW_CTL_init_ptrs(&sw_template);

#if defined(SWMPI)
    SW_MPI_create_types(datatypes, &LogInfo);
    if (LogInfo.stopRun) {
        goto finishProgram;
    }
#endif

    if (rank > 0) {
        goto skip;
    }

    // Obtain user input from the command line
    sw_init_args(
        argc,
        argv,
        &EchoInits,
        &SW_Domain.SW_PathInputs.txtInFiles[eFirst],
        &userSUID,
        &SW_WallTime.wallTimeLimit,
        &renameDomainTemplateNC,
        &prepareFiles,
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
    SW_CTL_setup_domain(userSUID, renameDomainTemplateNC, &SW_Domain, &LogInfo);
    if (LogInfo.stopRun) {
        goto finishProgram;
    }

    // setup and construct model template (independent of inputs)
    SW_CTL_setup_model(&sw_template, &SW_Domain.OutDom, swTRUE, &LogInfo);
    if (LogInfo.stopRun) {
        goto finishProgram;
    }

    SW_MDL_get_ModelRun(&sw_template.ModelIn, &SW_Domain, NULL, &LogInfo);
    if (LogInfo.stopRun) {
        goto finishProgram;
    }

    // read user inputs
    SW_CTL_read_inputs_from_disk(
        &sw_template,
        &SW_Domain,
        &SW_Domain.hasConsistentSoilLayerDepths,
        &LogInfo
    );
    if (LogInfo.stopRun) {
        goto finishProgram;
    }

#if defined(SWNETCDF)
    SW_NCIN_check_input_config(
        &SW_Domain.netCDFInput,
        SW_Domain.hasConsistentSoilLayerDepths,
        sw_template.SiteIn.inputsProvideSWRCp,
        &LogInfo
    );
    if (LogInfo.stopRun) {
        goto finishProgram;
    }

    SW_NCIN_precalc_lookups(&SW_Domain, &sw_template.WeatherIn, &LogInfo);
    if (LogInfo.stopRun) {
        goto finishProgram;
    }

    SW_NCIN_create_indices(&SW_Domain, &LogInfo);
    if (LogInfo.stopRun) {
        goto finishProgram;
    };

    SW_NCIN_check_input_files(&SW_Domain, &LogInfo);
    if (LogInfo.stopRun) {
        goto finishProgram;
    }
#endif

    // finalize daily weather
#if defined(SWNETCDF)
    if (!SW_Domain.netCDFInput.readInVars[eSW_InWeather][0] && !prepareFiles) {
#endif
        SW_WTH_finalize_all_weather(
            &sw_template.MarkovIn,
            &sw_template.WeatherIn,
            sw_template.RunIn.weathRunAllHist,
            sw_template.ModelSim.cum_monthdays,
            sw_template.ModelSim.days_in_month,
            &LogInfo
        );
        if (LogInfo.stopRun) {
            goto finishProgram;
        }
#if defined(SWNETCDF)
    }
#endif

    // identify domain-wide soil profile information
    SW_DOM_soilProfile(
        &SW_Domain.netCDFInput,
        &SW_Domain.SW_PathInputs,
        SW_Domain.hasConsistentSoilLayerDepths,
        &SW_Domain.nMaxSoilLayers,
        &SW_Domain.nMaxEvapLayers,
        SW_Domain.depthsAllSoilLayers,
        sw_template.SiteSim.n_layers,
        sw_template.SiteSim.n_evap_lyrs,
        sw_template.RunIn.SoilRunIn.depths,
        &LogInfo
    );
    if (LogInfo.stopRun) {
        goto finishProgram;
    }

#if defined(SWMPI)
skip:
    if (SW_MPI_check_setup_status(size, rank, &LogInfo)) {
        goto finishProgram;
    }

    SW_MPI_process_types(
        &SW_Domain, datatypes[eSW_MPI_Designate], procName, size, rank, &LogInfo
    );

    if (rank > 0) {
        goto finishProgram;
    }
#endif

    // initialize output
    SW_OUT_setup_output(
        SW_Domain.nMaxSoilLayers,
        SW_Domain.nMaxEvapLayers,
        sw_template.VegEstabSim.count,
        sw_template.VegEstabIn.parms,
        &SW_Domain.OutDom,
        &LogInfo
    );
    if (LogInfo.stopRun) {
        goto finishProgram;
    }

#if defined(SWNETCDF)
    SW_NCOUT_read_out_vars(
        &SW_Domain.OutDom,
        SW_Domain.SW_PathInputs.txtInFiles,
        sw_template.VegEstabIn.parms,
        &LogInfo
    );
    if (LogInfo.stopRun) {
        goto finishProgram;
    }

    if (!prepareFiles) {
        SW_NCOUT_create_units_converters(&SW_Domain.OutDom, &LogInfo);
        if (LogInfo.stopRun) {
            goto finishProgram;
        }
    }
#endif // SWNETCDF

    SW_OUT_create_files(&sw_template.SW_PathOutputs, &SW_Domain, &LogInfo);
    if (LogInfo.stopRun || prepareFiles) {
        if (prepareFiles) {
            sw_message("completed simulation preparations.");
        }

        goto closeFiles;
    }

    if (EchoInits) {
        echo_all_inputs(&sw_template, &SW_Domain.OutDom, &LogInfo);
    }

    // run simulations: loop over simulation set
    SW_CTL_RunSimSet(&sw_template, &SW_Domain, &SW_WallTime, &LogInfo);

closeFiles: {
    // finish-up output (not used with rSOILWAT2)
    SW_OUT_close_files(
        &sw_template.SW_PathOutputs, &SW_Domain.OutDom, &LogInfo
    );
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

#if defined(SWMPI)
    SW_MPI_finalize();
#endif

    return 0;
}

/*********** End of Main() *******************/
