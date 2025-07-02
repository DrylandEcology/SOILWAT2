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
#include "include/SW_Defines.h" // for SW_MPI_ROOT
#include "include/SW_MPI.h"     // for SW_MPI_setup_fail, SW_MPI_PROC_IO
#include <mpi.h>                // for MPI_COMM_WORLD
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
    Bool setupFailed = swTRUE;
    Bool endQuietly = swFALSE;

    int rank = 0;
    int size = 0;
    char procName[FILENAME_MAX] = "\0";

    size_t userSUID;

    // Start overall wall time
    SW_WT_StartTime(&SW_WallTime);

#if defined(SWMPI)
    SW_MPI_initialize(
        &argc,
        &argv,
        &rank,
        &size,
        procName,
        &SW_Domain.SW_Designation,
        SW_Domain.datatypes
    );
#endif

    // Initialize logs and pointer objects
    sw_init_logs(stdout, &LogInfo);

    SW_DOM_init_ptrs(&SW_Domain);
    SW_CTL_init_ptrs(&sw_template);

#if defined(SWMPI)
    SW_MPI_create_types(SW_Domain.datatypes, &LogInfo);
    if (SW_MPI_setup_fail(LogInfo.stopRun, MPI_COMM_WORLD)) {
        goto finishProgram;
    }
#endif

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
#if defined(SWMPI)
    if (endQuietly || SW_MPI_setup_fail(LogInfo.stopRun, MPI_COMM_WORLD)) {
        goto finishProgram;
    }
#else
    if (endQuietly || LogInfo.stopRun) {
        goto finishProgram;
    }
#endif

    // SOILWAT2: do print progress to console unless user requests quiet
    LogInfo.printProgressMsg = (Bool) (!LogInfo.QuietMode);

    if (LogInfo.printProgressMsg) {
        SW_MSG_ROOT("started.", rank);
        if (rank == 0) {
            sw_print_version();
        }
    }

    // setup and construct domain
    SW_CTL_setup_domain(
        rank, userSUID, renameDomainTemplateNC, &SW_Domain, &LogInfo
    );
    if (LogInfo.stopRun) {
#if defined(SWMPI)
        goto setupProgramData;
#else
        goto finishProgram;
#endif
    }

    // setup and construct model template (independent of inputs)
    SW_CTL_setup_model(&sw_template, &SW_Domain.OutDom, swTRUE, &LogInfo);
    if (LogInfo.stopRun) {
#if defined(SWMPI)
        goto setupProgramData;
#else
        goto finishProgram;
#endif
    }
#if defined(SWMPI)
    if (rank > SW_MPI_ROOT) {
        goto setupProgramData;
    }
#endif

    SW_MDL_get_ModelRun(&sw_template.ModelIn, &SW_Domain, NULL, &LogInfo);
    if (LogInfo.stopRun) {
#if defined(SWMPI)
        goto setupProgramData;
#else
        goto finishProgram;
#endif
    }

    // read user inputs
    SW_CTL_read_inputs_from_disk(
        &sw_template,
        &SW_Domain,
        &SW_Domain.hasConsistentSoilLayerDepths,
        &LogInfo
    );
    if (LogInfo.stopRun) {
#if defined(SWMPI)
        goto setupProgramData;
#else
        goto finishProgram;
#endif
    }

#if defined(SWNETCDF)
    SW_NCIN_check_input_config(
        &SW_Domain.netCDFInput,
        SW_Domain.hasConsistentSoilLayerDepths,
        sw_template.SiteIn.inputsProvideSWRCp,
        &LogInfo
    );
    if (LogInfo.stopRun) {
#if defined(SWMPI)
        goto setupProgramData;
#else
        goto finishProgram;
#endif
    }

    SW_NCIN_precalc_lookups(&SW_Domain, &sw_template.WeatherIn, &LogInfo);
    if (LogInfo.stopRun) {
#if defined(SWMPI)
        goto setupProgramData;
#else
        goto finishProgram;
#endif
    }

    SW_NCIN_create_indices(&SW_Domain, &LogInfo);
    if (LogInfo.stopRun) {
#if defined(SWMPI)
        goto setupProgramData;
#else
        goto finishProgram;
#endif
    };

    SW_NCIN_check_input_files(&SW_Domain, &LogInfo);
    if (LogInfo.stopRun) {
#if defined(SWMPI)
        goto setupProgramData;
#else
        goto finishProgram;
#endif
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
            NULL,
            swFALSE, // Does not matter
            &LogInfo
        );
        if (LogInfo.stopRun) {
#if defined(SWMPI)
            goto setupProgramData;
#else
        goto finishProgram;
#endif
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
        sw_template.RunIn.SiteRunIn.n_layers,
        sw_template.SiteSim.n_evap_lyrs,
        sw_template.RunIn.SoilRunIn.depths,
        &LogInfo
    );
    if (LogInfo.stopRun) {
#if defined(SWMPI)
        goto setupProgramData;
#else
        goto finishProgram;
#endif
    }

#if defined(SWMPI)
setupProgramData:
#endif
    sw_setup_prog_data(
        rank, size, procName, prepareFiles, &sw_template, &SW_Domain, &LogInfo
    );
#if defined(SWMPI)
    if (SW_MPI_setup_fail(LogInfo.stopRun, MPI_COMM_WORLD)) {
        goto finishProgram;
    }
#else
    if (LogInfo.stopRun) {
        goto finishProgram;
    }
#endif

    if (rank == 0) {
        SW_OUT_create_files(&sw_template.SW_PathOutputs, &SW_Domain, &LogInfo);
    }

#if defined(SWMPI)
    if (SW_MPI_setup_fail(LogInfo.stopRun, MPI_COMM_WORLD) || prepareFiles) {
        if (prepareFiles && LogInfo.printProgressMsg) {
            SW_MSG_ROOT("completed simulation preparations.", rank);
        }

        goto closeFiles;
    }

    if (SW_Domain.SW_Designation.procJob == SW_MPI_PROC_IO) {
        SW_MPI_open_files(
            rank,
            &SW_Domain.SW_Designation,
            &SW_Domain.SW_PathInputs,
            &SW_Domain.netCDFInput,
            &sw_template.SW_PathOutputs,
            &SW_Domain.OutDom,
            &LogInfo
        );
    }
    if (SW_MPI_setup_fail(LogInfo.stopRun, MPI_COMM_WORLD)) {
        goto closeFiles;
    }
#else
    if (LogInfo.stopRun || prepareFiles) {
        if (prepareFiles && LogInfo.printProgressMsg) {
            SW_MSG_ROOT("completed simulation preparations.", rank);
        }

        goto closeFiles;
    }
#endif

    if (EchoInits && rank == 0) {
        echo_all_inputs(&sw_template, &SW_Domain.OutDom, &LogInfo);
    }

    // run simulations: loop over simulation set
    SW_CTL_RunSims(
        rank, &sw_template, &SW_Domain, &setupFailed, &SW_WallTime, &LogInfo
    );

closeFiles: {
#if defined(SWMPI)
    if (SW_Domain.SW_Designation.procJob == SW_MPI_PROC_IO) {
#endif
        // finish-up output (not used with rSOILWAT2)
        SW_OUT_close_files(
            &sw_template.SW_PathOutputs, &SW_Domain.OutDom, &LogInfo
        );
#if defined(SWMPI)
    }
#endif
}

finishProgram: {
    // de-allocate all memory
    SW_DOM_deconstruct(&SW_Domain); // Includes closing netCDF files if needed
    SW_CTL_clear_model(swTRUE, &sw_template);

    sw_finalize_program(
        rank, size, &SW_Domain, &SW_WallTime, setupFailed, endQuietly, &LogInfo
    );
    if (!endQuietly && LogInfo.printProgressMsg) {
        SW_MSG_ROOT("ended.", rank);
    }
}

    return 0;
}

/*********** End of Main() *******************/
