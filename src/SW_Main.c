/********************************************************/
/********************************************************/
/*  Application: SOILWAT - soilwater dynamics simulator
 *  Source file: Main.c
 *  Type: main module
 *  Purpose: Contains the main loops and initializations.
 *
 06/24/2013	(rjm)	included "SW_Site.h" and "SW_Weather.h";
 added calls at end of main() to SW_SIT_clear_layers() and SW_WTH_clear_runavg_list() to free memory
 */
/********************************************************/
/********************************************************/

/* =================================================== */
/*                INCLUDES / DEFINES                   */
/* --------------------------------------------------- */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#ifdef __BCC__
#include <dir.h>
#else
#include <unistd.h>
#endif
#include "include/generic.h"
#include "include/filefuncs.h"
#include "include/Times.h"
#include "include/SW_Files.h"
#include "include/SW_Defines.h"
#include "include/SW_Control.h"
#include "include/SW_Site.h"
#include "include/SW_Weather.h"
#include "include/SW_Output.h"
#include "include/SW_Main_lib.h"
#include "include/SW_Domain.h"
#include "include/SW_Model.h"

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
	SW_ALL sw_template;
	SW_DOMAIN SW_Domain;
	SW_OUTPUT_POINTERS SW_OutputPtrs[SW_OUTNKEYS];
	LOG_INFO LogInfo;
	Bool EchoInits = swFALSE;

    unsigned long userSUID;
    int nMaxSoilLayers = 0;

    // Start overall wall time
    SW_WT_StartTime(&SW_WallTime);

    // Initialize logs and pointer objects
    sw_init_logs(stdout, &LogInfo);

    SW_DOM_init_ptrs(&SW_Domain);
    SW_CTL_init_ptrs(&sw_template);

    // Obtain user input from the command line
    sw_init_args(argc, argv, &EchoInits, &SW_Domain.PathInfo.InFiles[eFirst],
                 &userSUID, &SW_WallTime.wallTimeLimit, &LogInfo);
    if(LogInfo.stopRun) {
        goto finishProgram;
    }

    // SOILWAT2: do print progress to console unless user requests quiet
    LogInfo.printProgressMsg = (Bool)(!LogInfo.QuietMode);

    if (LogInfo.printProgressMsg) {
      sw_message("started.");
      sw_print_version();
    }

    // setup and construct domain
    SW_CTL_setup_domain(userSUID, &SW_Domain, &LogInfo);
    if(LogInfo.stopRun) {
        goto finishProgram;
    }

    // setup and construct model template (independent of inputs)
    SW_CTL_setup_model(&sw_template, SW_OutputPtrs, &LogInfo);
    if(LogInfo.stopRun) {
        goto finishProgram;
    }


    SW_MDL_get_ModelRun(&sw_template.Model, &SW_Domain, NULL, &LogInfo);
    if(LogInfo.stopRun) {
        goto finishProgram;
    }

	// read user inputs
	SW_CTL_read_inputs_from_disk(&sw_template, &SW_Domain.PathInfo, &LogInfo);
    if(LogInfo.stopRun) {
        goto finishProgram;
    }

    #if defined(SWNETCDF)
    SW_NC_check_input_files(&SW_Domain, &LogInfo);
    nMaxSoilLayers = SW_NC_get_nMaxSoilLayers(sw_template.Site.n_layers);
    #else
    nMaxSoilLayers = sw_template.Site.n_layers;
    #endif

	// finalize daily weather
	SW_WTH_finalize_all_weather(&sw_template.Markov, &sw_template.Weather,
                                sw_template.Model.cum_monthdays,
								sw_template.Model.days_in_month, &LogInfo);
    if(LogInfo.stopRun) {
        goto finishProgram;
    }

	// initialize simulation run (based on user inputs)
	SW_CTL_init_run(&sw_template, &LogInfo);
    if(LogInfo.stopRun) {
        goto finishProgram;
    }

    // initialize output
    SW_OUT_setup_output(
        sw_template.Site.n_layers,
        sw_template.Site.n_evap_lyrs,
        &sw_template.VegEstab,
        &sw_template.GenOutput,
        &LogInfo
    );

    if(LogInfo.stopRun) {
        goto finishProgram;
    }

    #if defined(SWNETCDF)
    SW_NC_read_out_vars(
        sw_template.Output,
        &sw_template.GenOutput,
        SW_Domain.PathInfo.InFiles,
        sw_template.VegEstab.parms,
        &LogInfo
    );
    if(LogInfo.stopRun) {
      goto finishProgram;
    }
    #endif // SWNETCDF

    SW_OUT_create_files(
        &sw_template.FileStatus,
        &SW_Domain,
        sw_template.Output,
        &sw_template.GenOutput,
        nMaxSoilLayers,

        SW_Domain.startyr,
        SW_Domain.endyr,
        sw_template.Site.depths,

        &LogInfo
    );
    if(LogInfo.stopRun) {
        goto closeFiles;
    }

	if(EchoInits) {
		_echo_all_inputs(&sw_template);
	}

  // run simulations: loop over simulation set
    SW_CTL_RunSimSet(&sw_template, SW_OutputPtrs, &SW_Domain, &SW_WallTime, &LogInfo);

    closeFiles: {
        // finish-up output
        SW_OUT_close_files(&sw_template.FileStatus, &sw_template.GenOutput, &LogInfo); // not used with rSOILWAT2
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
