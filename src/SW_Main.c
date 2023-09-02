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



/* =================================================== */
/*             Local Function Definitions              */
/* --------------------------------------------------- */



/* =================================================== */
/*             Global Function Definitions             */
/* --------------------------------------------------- */

/************  Main() ************************/
/**
@brief Provides a way to inform the user that something was logged, can be changed by code
 			(eg. init file) but must be set before sw_init_args().  See generic.h
*/
int main(int argc, char **argv) {
	/* =================================================== */
	SW_ALL sw;
	SW_OUTPUT_POINTERS SW_OutputPtrs[SW_OUTNKEYS];
	LOG_INFO LogInfo;
	PATH_INFO PathInfo;
	Bool EchoInits, QuietMode;

	sw_init_logs(&LogInfo);
	SW_CTL_init_ptrs(&sw, PathInfo.InFiles);

	sw_init_args(argc, argv, &QuietMode, &EchoInits, &PathInfo.InFiles[eFirst],
				 &LogInfo);

	// Print version if not in quiet mode
	if (!QuietMode) {
		sw_print_version();
	}

  // setup and construct model (independent of inputs)
	SW_CTL_setup_model(&sw, SW_OutputPtrs, &PathInfo, &LogInfo);

	// read user inputs
	SW_CTL_read_inputs_from_disk(&sw, &PathInfo, &LogInfo);

	// finalize daily weather
	SW_WTH_finalize_all_weather(&sw.Markov, &sw.Weather, sw.Model.cum_monthdays,
								sw.Model.days_in_month, &LogInfo);

	// initialize simulation run (based on user inputs)
	SW_CTL_init_run(&sw, &LogInfo);

  // initialize output
	SW_OUT_set_ncol(sw.Site.n_layers, sw.Site.n_evap_lyrs, sw.VegEstab.count,
					sw.GenOutput.ncol_OUT);
	SW_OUT_set_colnames(sw.Site.n_layers, sw.VegEstab.parms,
						sw.GenOutput.ncol_OUT, sw.GenOutput.colnames_OUT,
						&LogInfo);
	SW_OUT_create_files(&sw.FileStatus, sw.Output, sw.Site.n_layers,
	                    PathInfo.InFiles, &sw.GenOutput, &LogInfo); // only used with SOILWAT2

	if(EchoInits) {
		_echo_all_inputs(&sw);
	}

  // run simulation: loop through each year
	SW_CTL_main(&sw, SW_OutputPtrs, &LogInfo);

  // finish-up output
	SW_OUT_close_files(&sw.FileStatus, &sw.GenOutput, &LogInfo); // not used with rSOILWAT2

	// de-allocate all memory
	SW_CTL_clear_model(swTRUE, &sw, &PathInfo);

	// Mention to the user if something was logged
	sw_write_logs(QuietMode, &LogInfo);

	return 0;
}
/*********** End of Main() *******************/
