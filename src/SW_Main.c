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
#include "include/generic.h" // externs `QuietMode`, `EchoInits`
#include "include/filefuncs.h" // externs `_firstfile`, `inbuf`
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

	logged = swFALSE;
	atexit(sw_check_log);
	logfp = stdout;

	sw_init_args(argc, argv);

	// Print version if not in quiet mode
	if (!QuietMode) {
		sw_print_version();
	}

  // setup and construct model (independent of inputs)
	SW_CTL_setup_model(_firstfile, &sw);

	// read user inputs
	SW_CTL_read_inputs_from_disk(&sw);

	// finalize daily weather
	SW_WTH_finalize_all_weather(&sw.Weather);

	// initialize simulation run (based on user inputs)
	SW_CTL_init_run(&sw);

  // initialize output
	SW_OUT_set_ncol(sw.Site.n_layers, sw.Site.n_evap_lyrs);
	SW_OUT_set_colnames(sw.Site.n_layers);
	SW_OUT_create_files(sw.Site.n_layers); // only used with SOILWAT2

  // run simulation: loop through each year
	SW_CTL_main(&sw);

  // finish-up output
	SW_OUT_close_files(); // not used with rSOILWAT2

	// de-allocate all memory
	SW_CTL_clear_model(swTRUE, &sw);

	return 0;
}
/*********** End of Main() *******************/
