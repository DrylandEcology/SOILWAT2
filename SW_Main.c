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
#include "generic.h"
#include "filefuncs.h"
#include "SW_Defines.h"
#include "SW_Control.h"
#include "SW_Site.h"
#include "SW_Weather.h"
#include "SW_Output.h"
#include "SW_Main_lib.c"


static void check_log(void);


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

/************  Main() ************************/
int main(int argc, char **argv) {
	/* =================================================== */

	logged = swFALSE;
	atexit(check_log);
	logfp = stdout; /* provides a way to inform user that something */
	/* was logged.  can be changed by code (eg init file */
	/* but must be set before init_args().  see generic.h */

	init_args(argc, argv);

	SW_CTL_init_model(_firstfile);
	SW_CTL_obtain_inputs();

	SW_OUT_set_ncol();
	SW_OUT_set_colnames();
	SW_OUT_create_files(); // only used with SOILWAT2

	SW_CTL_main();

	SW_SIT_clear_layers();
	SW_WTH_clear_runavg_list();
	SW_OUT_close_files(); // not used with rSOILWAT2

	return 0;
}
/*********** End of Main() *******************/
