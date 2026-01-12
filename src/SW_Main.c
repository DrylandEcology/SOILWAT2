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
#include "include/filefuncs.h"      // for sw_message via SW_MSG_ROOT
#include "include/generic.h"        // for Bool, swFALSE, swTRUE
#include "include/SW_Control.h"     // for SW_CTL_RunSimSet, SW_CTL_clear...
#include "include/SW_datastructs.h" // for LOG_INFO, SW_DOMAIN, SW_RUN
#include "include/SW_Defines.h"     // for ROOT_PROC
#include "include/SW_Domain.h"      // for SW_DOM_deconstruct, SW_DOM_ini...
#include "include/SW_Files.h"       // for eFirst
#include "include/SW_Main_lib.h"    // for sw_fail_on_error, sw_init_args
#include "include/SW_Model.h"       // for SW_MDL_get_ModelRun
#include "include/SW_Output.h"      // for SW_OUT_close_files, SW_OUT_cre...
#include "include/SW_Weather.h"     // for SW_WTH_finalize_all_weather
#include "include/Times.h"          // for SW_WT_ReportTime, SW_WT_StartTime
#include <stdio.h>                  // for NULL, FILENAME_MAX, size_t, stdout


#if defined(SWNETCDF)
#include "include/SW_netCDF_Input.h" // for SW_NCIN_check_input_config
#endif

#if defined(SWMPI)
#include "include/SW_MPI.h" // for SW_MPI_setup_fail
#include <mpi.h>            // for MPI_COMM_WORLD
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
    Bool endQuietly = swFALSE;

    int rank = 0;
    int size = 0;

    size_t userSUID;

    // Start overall wall time
    SW_WT_StartTime(&SW_WallTime);

#if defined(SWMPI)
    SW_MPI_initialize(&argc, &argv, &rank, &size);
#endif

    // Initialize logs and pointer objects
    sw_init_logs(stdout, &LogInfo);
    formatLogStage(LogInfo.logStage, sizeof LogInfo.logStage, "setup");

    SW_DOM_init_ptrs(&SW_Domain);
    SW_CTL_init_ptrs(&sw_template);

    // Obtain user input from the command line
    sw_init_args(
        argc,
        argv,
        rank,
        &EchoInits,
        &SW_Domain.SW_PathInputs.txtInFiles[eFirst],
        &userSUID,
        &SW_WallTime.wallTimeLimit,
        &renameDomainTemplateNC,
        &prepareFiles,
        &endQuietly,
        &LogInfo
    );
    checkJumpToLabel(endQuietly || LogInfo.stopRun, finishProgram);

    // SOILWAT2: do print progress to console unless user requests quiet
    LogInfo.printProgressMsg = (Bool) (!LogInfo.QuietMode);

    if (LogInfo.printProgressMsg) {
        SW_MSG_ROOT("started.", rank);
        if (rank == ROOT_PROC) {
            sw_print_version();
        }
    }

    // setup and construct domain
    SW_CTL_setup_domain(
        rank, userSUID, renameDomainTemplateNC, &SW_Domain, &LogInfo
    );
    checkJumpToLabel(LogInfo.stopRun, finishProgram);

    // setup and construct model template (independent of inputs)
    SW_CTL_setup_model(&sw_template, &SW_Domain.OutDom, swTRUE, &LogInfo);
    checkJumpToLabel(LogInfo.stopRun, finishProgram);

    SW_MDL_get_ModelRun(&sw_template.ModelIn, &SW_Domain, NULL, &LogInfo);
    checkJumpToLabel(LogInfo.stopRun, finishProgram);

    // read user inputs
    SW_CTL_read_inputs_from_disk(
        &sw_template,
        &SW_Domain,
        &SW_Domain.hasConsistentSoilLayerDepths,
        &LogInfo
    );
    checkJumpToLabel(LogInfo.stopRun, finishProgram);

#if defined(SWNETCDF)
    SW_NCIN_check_input_config(
        &SW_Domain.netCDFInput,
        SW_Domain.hasConsistentSoilLayerDepths,
        sw_template.SiteIn.inputsProvideSWRCp,
        (Bool) (sw_template.SiteIn.methodEvCo == 0),
        (Bool) (sw_template.SiteIn.methodTrCo == 0),
        &LogInfo
    );
    checkJumpToLabel(LogInfo.stopRun, finishProgram);

    SW_NCIN_precalc_lookups(rank, &SW_Domain, &sw_template.WeatherIn, &LogInfo);
    checkJumpToLabel(LogInfo.stopRun, finishProgram);
#endif

    // identify domain-wide soil profile information
    SW_DOM_soilProfile(
        &SW_Domain.netCDFInput,
        &SW_Domain.SW_PathInputs,
        SW_Domain.hasConsistentSoilLayerDepths,
        &SW_Domain.nMaxSoilLayers,
        SW_Domain.depthsAllSoilLayers,
        sw_template.RunIn.SiteRunIn.n_layers,
        sw_template.RunIn.SoilRunIn.depths,
        &LogInfo
    );
    checkJumpToLabel(LogInfo.stopRun, finishProgram);

    sw_setup_prog_data(
        rank, size, prepareFiles, &sw_template, &SW_Domain, &LogInfo
    );
    checkJumpToLabel(LogInfo.stopRun, finishProgram);

    SW_OUT_create_files(
        rank, &sw_template.SW_PathOutputs, &SW_Domain, &LogInfo
    );
    checkJumpToLabel(LogInfo.stopRun, closeFiles);

    if (prepareFiles) {
        if (LogInfo.printProgressMsg) {
            SW_MSG_ROOT("completed simulation preparations.", rank);
        }
        goto closeFiles;
    }

    if (EchoInits && rank == ROOT_PROC) {
        echo_all_inputs(&sw_template, &SW_Domain.OutDom, &LogInfo);
    }

    // run simulations: loop over simulation set
    SW_CTL_RunSimSet(
        rank, size, &sw_template, &SW_Domain, &SW_WallTime, &LogInfo
    );

closeFiles: {
    // finish-up output (not used with rSOILWAT2)
    SW_OUT_close_files(
        &sw_template.SW_PathOutputs, &SW_Domain.OutDom, &LogInfo
    );
}

finishProgram: {
    formatLogStage(LogInfo.logStage, sizeof LogInfo.logStage, "wrapup");

    // de-allocate all memory
    SW_DOM_deconstruct(&SW_Domain); // Includes closing netCDF files if needed
    SW_CTL_clear_model(swTRUE, &sw_template);

    sw_finalize_program(rank, size, &SW_WallTime, endQuietly, &LogInfo);
    if (!endQuietly && LogInfo.printProgressMsg) {
        SW_MSG_ROOT("ended.", rank);
    }
}

    return 0;
}

/*********** End of Main() *******************/
