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
	PATH_INFO PathInfo;
	Bool EchoInits;

    int startSimSet, endSimSet, userSuid, suid;
    int *ncStartSuid;

	sw_init_logs(stdout, &LogInfo);
	SW_CTL_init_ptrs(&sw_template, PathInfo.InFiles);

	sw_init_args(argc, argv, &EchoInits, &PathInfo.InFiles[eFirst],
                 &userSuid, &LogInfo);
    if(LogInfo.stopRun) {
        goto finishProgram;
    }

	// Print version if not in quiet mode
	if (!LogInfo.QuietMode) {
		sw_print_version();
	}

  // setup and construct model/domain/SW_ALL template (independent of inputs)
	SW_CTL_setup_model(&sw_template, SW_OutputPtrs, &PathInfo, &LogInfo);
    if(LogInfo.stopRun) {
        goto finishProgram;
    }

    SW_CTL_setup_domain(&PathInfo, userSuid, &SW_Domain, &startSimSet,
                        &endSimSet, &LogInfo);
    if(LogInfo.stopRun) {
        goto finishProgram;
    }

    SW_MDL_get_ModelRun(&sw_template.Model, &SW_Domain, NULL, &LogInfo);
    if(LogInfo.stopRun) {
        goto finishProgram;
    }

	// read user inputs
	SW_CTL_read_inputs_from_disk(&sw_template, &PathInfo, &LogInfo);
    if(LogInfo.stopRun) {
        goto finishProgram;
    }

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
	                    PathInfo.InFiles, &sw_template.GenOutput, &LogInfo); // only used with SOILWAT2
    if(LogInfo.stopRun) {
        goto closeFiles;
    }

	if(EchoInits) {
		_echo_all_inputs(&sw_template);
	}

  // run simulations: loop over simulation set
    for(suid = startSimSet; suid < endSimSet; suid++)
    {
        ncStartSuid = SW_DOM_calc_ncStartSuid(&SW_Domain, suid);
        if(!SW_DOM_CheckProgress(SW_Domain.DomainType, ncStartSuid)) {

            SW_CTL_run_sw(&sw_template, &SW_Domain, ncStartSuid, NULL,
                          SW_OutputPtrs, NULL, &LogInfo);

            sw_write_warnings(&LogInfo);
            sw_init_logs(stdout, &LogInfo); // Reset LOG_INFO for next simulation run

            if(!LogInfo.stopRun) {
                // Process output

                // Set simulation run progress
                SW_DOM_SetProgress(SW_Domain.DomainType, ncStartSuid);
            }
        }
    }

    closeFiles: {
        // finish-up output
        SW_OUT_close_files(&sw_template.FileStatus, &sw_template.GenOutput, &LogInfo); // not used with rSOILWAT2
    }

    finishProgram: {
        // de-allocate all memory
        SW_F_deconstruct(PathInfo.InFiles);
        SW_CTL_clear_model(swTRUE, &sw_template);

        sw_fail_on_error(&LogInfo);
    }

	return 0;
}
/*********** End of Main() *******************/
