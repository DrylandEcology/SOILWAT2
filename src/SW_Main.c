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
#include "include/SW_Files.h"
#include "include/SW_Defines.h"
#include "include/SW_Control.h"
#include "include/SW_Site.h"
#include "include/SW_Weather.h"
#include "include/SW_Output.h"
#include "include/SW_Output_outtext.h"
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
	SW_ALL sw_template;
	SW_DOMAIN SW_Domain;
	SW_OUTPUT_POINTERS SW_OutputPtrs[SW_OUTNKEYS];
	LOG_INFO LogInfo;
	Bool EchoInits;

    unsigned long userSUID;

    // Initialize logs and pointer objects
    sw_init_logs(stdout, &LogInfo);

    SW_F_init_ptrs(SW_Domain.PathInfo.InFiles);
    SW_CTL_init_ptrs(&sw_template);

    // Obtain user input from the command line
    sw_init_args(argc, argv, &EchoInits, &SW_Domain.PathInfo.InFiles[eFirst],
                 &userSUID, &LogInfo);
    if(LogInfo.stopRun) {
        goto finishProgram;
    }

	// Print version if not in quiet mode
	if (!LogInfo.QuietMode) {
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
	SW_OUT_set_ncol(sw_template.Site.n_layers, sw_template.Site.n_evap_lyrs,
                    sw_template.VegEstab.count, sw_template.GenOutput.ncol_OUT);
	SW_OUT_set_colnames(sw_template.Site.n_layers, sw_template.VegEstab.parms,
						sw_template.GenOutput.ncol_OUT, sw_template.GenOutput.colnames_OUT,
						&LogInfo);
    if(LogInfo.stopRun) {
        goto finishProgram;
    }
	SW_OUT_create_files(&sw_template.FileStatus, sw_template.Output, sw_template.Site.n_layers,
	                    SW_Domain.PathInfo.InFiles, &sw_template.GenOutput, &LogInfo); // only used with SOILWAT2
    if(LogInfo.stopRun) {
        goto closeFiles;
    }

	if(EchoInits) {
		_echo_all_inputs(&sw_template);
	}

  // run simulations: loop over simulation set
    SW_CTL_RunSimSet(&sw_template, SW_OutputPtrs, &SW_Domain, &LogInfo);

    closeFiles: {
        // finish-up output
        SW_OUT_close_files(&sw_template.FileStatus, &sw_template.GenOutput, &LogInfo); // not used with rSOILWAT2
    }

    finishProgram: {
        // de-allocate all memory
        SW_F_deconstruct(SW_Domain.PathInfo.InFiles);
        SW_CTL_clear_model(swTRUE, &sw_template);

        sw_write_warnings("(main) ", &LogInfo);
        sw_wrapup_logs(&LogInfo);
        sw_fail_on_error(&LogInfo);
    }

	return 0;
}
/*********** End of Main() *******************/
