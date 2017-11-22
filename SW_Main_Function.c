/********************************************************/
/********************************************************/
/*  Application: SOILWAT - soilwater dynamics simulator
 *  Source file: SW_Main_Function.c
 *  Type: duplicate main module
 *  Purpose: Contains the main loops and initializations.
 *
 06/13/2016 (akt)

  This file will compile only for Stepwat and not for RSoilwat

  Added this file ‘SW_Main_Function.c’ that is similar to  as SW_Main.c except SW_Main_Function.c does not have main() function.
  Added this because of one requirement: to have Soilwat all the output in the Stepwat, while running  Stepwat  with soilwat.
  Like  running Soilwat standalone output , we wanted  this,  all the output files in stepwat as well.

  So one of the solution was to  include soilwat  SW_Main.c file for compile in Stepwat project  and from Stepwat main() function flow,
  we can call the main() function of Soilwat's  SW_Main.c .

  However, we can not include  SW_Main.c file for compile in Stepwat project,  as  any c/cpp project can have only one entry/start point
  as main() function so  any project can not have more than one main() functions.
  But somehow for this requirement need to run two main() function flow (one Soilwat standalone flow, second normal stepwat flow).
  So I had to create duplicate file  SW_Main_Function.c  from SW_Main.c , here main() function renamed so it will not give conflict
  with Stepwat main() function and did some minor cosmetic changes for not closing log file pointers, that was  shared with both
  stepwat and soilwat.

 	*/
/********************************************************/
/********************************************************/

/* =================================================== */
/*                INCLUDES / DEFINES                   */
/* --------------------------------------------------- */

#ifdef STEPWAT

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#ifdef RSOILWAT
#include <R.h>
#include <Rdefines.h>
#include <Rconfig.h>
#include <Rinternals.h>
#endif

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

/* =================================================== */
/*                  Global Declarations                */
/* externed by other routines elsewhere in the program */
/* --------------------------------------------------- */

/* see generic.h and filefuncs.h for more info on these vars */
char inbuf[1024]; /* buffer used by input statements */
char errstr[MAX_ERROR]; /* used to compose an error msg    */
FILE *logfp; /* file handle for logging messages */
int logged; /* boolean: true = we logged a msg */
/* if true, write indicator to stderr */
#ifdef RSOILWAT
extern int logFatl;
#endif
Bool QuietMode, EchoInits; /* if true, echo inits to logfile */
//function
void init_args(int argc, char **argv);

/* =================================================== */
/*                Module-Level Declarations            */
/* --------------------------------------------------- */

static void check_log(void);
static void usage(void) {
	char *s1 = "Soil water model version 2.2a (SGS-LTER Oct-2003).\n"
			"Usage: soilwat [-d startdir] [-f files.in] [-e] [-q]\n"
			"  -d : operate (chdir) in startdir (default=.)\n"
			"  -f : supply list of input files (default=files.in)\n"
			"       a preceeding path applies to all input files\n"
			"  -e : echo initial values from site and estab to logfile\n"
			"  -q : quiet mode, don't print message to check logfile.\n";
  sw_error(0, "%s", s1);
}

char _firstfile[1024];

#ifndef RSOILWAT
/************  Main() ************************/
void  main_function(int argc, char **argv) {
	/* =================================================== */

	swprintf("inside soilwat main: argc=%d argv[0]=%s ,argv[1]=%s,argv[2]=%s \n ",argc,argv[0],argv[1],argv[2]);
	logged = FALSE;
	//atexit(check_log);
	logfp = stdout; /* provides a way to inform user that something */
	/* was logged.  can be changed by code (eg init file */
	/* but must be set before init_args().  see generic.h */

	init_args(argc, argv);
	swprintf("inside soilwat main: init_args successful \n" );

	SW_CTL_init_model(_firstfile);
	SW_CTL_obtain_inputs();
	swprintf("inside soilwat main: SW_CTL_init_model successful _firstfile=%s \n",_firstfile );

	SW_CTL_main();
	swprintf("inside soilwat main: SW_CTL_main successful \n" );

	SW_SIT_clear_layers();
	swprintf("inside soilwat main: SW_SIT_clear_layers successful \n" );

	SW_WTH_clear_runavg_list();
	swprintf("inside soilwat main: SW_WTH_clear_runavg_list successful exit main \n" );

}
/*********** End of Main() *******************/

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
#endif
void init_args(int argc, char **argv) {
	/* =================================================== */
	/* to add an option:
	 *  - include it in opts[]
	 *  - set a flag in valopts indicating no value (0),
	 *    value required (1), or value optional (-1),
	 *  - then tell us what to do in the switch statement
	 *
	 * 3/1/03 - cwb - Current options are
	 *                -d=chg to work dir <opt=dir_name>
	 *                -f=chg deflt first file <opt=file.in>
	 *                -q=quiet, don't print "Check logfile"
	 *                   at end of program.
	 */
	char str[1024]
	const char *opts[] = { "-d", "-f", "-e", "-q" }; /* valid options */
	int valopts[] = { 1, 1, 0, 0 }; /* indicates options with values */
	/* 0=none, 1=required, -1=optional */
	int i, /* looper through all cmdline arguments */
	a, /* current valid argument-value position */
	op, /* position number of found option */
	nopts = sizeof(opts) / sizeof(char *);

	/* Defaults */
	strcpy(_firstfile, DFLT_FIRSTFILE);
	QuietMode = EchoInits = FALSE;

	a = 1;
	for (i = 1; i <= nopts; i++) {
		if (a >= argc)
			break;

		/* figure out which option by its position 0-(nopts-1) */
		for (op = 0; op < nopts; op++) {
			if (strncmp(opts[op], argv[a], 2) == 0)
				break; /* found it, move on */
		}
		if (op == nopts) {
      usage();
      sw_error(-1, "Invalid option %s\n", argv[a]);
		}

		*str = '\0';
		/* extract value part of option-value pair */
		if (valopts[op]) {
			if ('\0' != argv[a][2]) { /* no space betw opt-value */
				strcpy(str, (argv[a] + 2));

			} else if ('-' != *argv[a + 1]) { /* space betw opt-value */
				strcpy(str, argv[++a]);

			} else if (0 < valopts[op]) { /* required opt-val not found */
        usage();
        sw_error(-1, "Incomplete option %s\n", opts[op]);
			} /* opt-val not required */
		}

		/* tell us what to do here                   */
		/* set indicators/variables based on results */
		switch (op) {
		case 0: /* -d */
			if (!ChDir(str)) {
				LogError(logfp, LOGFATAL, "Invalid project directory (%s)", str);
			}
			break;
		case 1:
			strcpy(_firstfile, str);
			break; /* -f */
		case 2:
			EchoInits = TRUE;
			break; /* -e */
		case 3:
			QuietMode = TRUE;
			break; /* -q */
		default:
			LogError(logfp, LOGFATAL, "Programmer: bad option in main:init_args:switch");
		}

		a++; /* move to next valid arg-value position */

	} /* end for(i) */

}

#endif
