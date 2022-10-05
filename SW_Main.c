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
#include "generic.h" // externs `QuietMode`, `EchoInits`
#include "filefuncs.h" // externs `_firstfile`, `inbuf`
#include "SW_Defines.h"
#include "SW_Control.h"
#include "SW_Site.h"
#include "SW_Output.h"
#include "SW_Output_outtext.h"
#include "SW_Main_lib.h"



/* =================================================== */
/*             Local Function Definitions              */
/* --------------------------------------------------- */

static void check_log(void) {
	/* =================================================== */
	/* function to be called by atexit() so it's the last
	 * to execute before termination.  This is the place to
	 * do any cleanup or progress reporting.
	 */
	if (logfp != stdout && logfp != stderr) {
		if (logged && !QuietMode)
			sw_error(0, "\nCheck logfile for error or status messages.\n");
		CloseFile(&logfp);
	}

}


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

	logged = swFALSE;
	atexit(check_log);
	logfp = stdout;

	sw_init_args(argc, argv);

	// Print version if not in quiet mode
	if (!QuietMode) {
		sw_print_version();
	}

  // setup and construct model (independent of inputs)
	SW_CTL_setup_model(_firstfile);

	// read user inputs
	SW_CTL_read_inputs_from_disk();

	// finalize daily weather
	SW_WTH_finalize_all_weather();

	// initialize simulation run (based on user inputs)
	SW_CTL_init_run();

  // initialize output
	SW_OUT_set_ncol();
	SW_OUT_set_colnames();
	SW_OUT_create_files(); // only used with SOILWAT2

  // run simulation: loop through each year
	SW_CTL_main();

  // finish-up output
	SW_OUT_close_files(); // not used with rSOILWAT2

	// de-allocate all memory
	SW_CTL_clear_model(swTRUE);

	return 0;
}
/*********** End of Main() *******************/
