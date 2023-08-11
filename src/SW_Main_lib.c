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
#include "include/SW_Main_lib.h"
#include "include/myMemory.h"

/* =================================================== */
/*             Local Function Definitions              */
/* --------------------------------------------------- */

/** @brief Print usage and command line options
*/
static void sw_print_usage(void) {
	swprintf(
		"Ecosystem water simulation model SOILWAT2\n"
		"More details at https://github.com/Burke-Lauenroth-Lab/SOILWAT2\n"
		"Usage: ./SOILWAT2 [-d startdir] [-f files.in] [-e] [-q] [-v] [-h] [-i 1]\n"
		"  -d : operate (chdir) in startdir (default=.)\n"
		"  -f : name of main input file (default=files.in)\n"
		"       a preceeding path applies to all input files\n"
		"  -e : echo initial values from site and estab to logfile\n"
		"  -q : quiet mode, don't print message to check logfile\n"
		"  -v : print version information\n"
		"  -h : print this help information\n"
		"  -i : input a simulation unique identifier (suid)\n"
	);
}



/* =================================================== */
/*             Global Function Definitions             */
/* --------------------------------------------------- */

/** @brief Print version information
*/
void sw_print_version(void) {
	swprintf(
		"SOILWAT2 version: %s\n",
		SW2_VERSION
	);
	swprintf(
		"Compiled        : by %s, on %s, on %s %s\n",
		USERNAME, HOSTNAME, BUILD_DATE, BUILD_TIME
	);
}



/**
@brief Initializes arguments and sets indicators/variables based on results.

@param argc Argument C.
@param argv Argument V.
@param[out] QuietMode Flag to control if the program version is displayed
@param[out] EchoInits Flag to control if inputs are to be output to the user
@param[out] suid Simulation Unique Identifer, aka simulation run ID within
	a group of simulation runs
@param[out] _firstfile First file name to be filled in the program run
@param[in] LogInfo Holds information dealing with logfile output

@sideeffect argv Updated argument V.
*/
void sw_init_args(int argc, char **argv, Bool *QuietMode,
	Bool *EchoInits, int* suid, char **_firstfile, LOG_INFO* LogInfo) {

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
	char str[1024];
	char const *opts[] = { "-d", "-f", "-e", "-q", "-v", "-h", "-i" }; /* valid options */
	int valopts[] = { 1, 1, 0, 0, 0, 0, 1 }; /* indicates options with values */
	/* 0=none, 1=required, -1=optional */
	int i, /* looper through all cmdline arguments */
	a, /* current valid argument-value position */
	op, /* position number of found option */
	nopts = sizeof(opts) / sizeof(char *);

	/* Defaults */
	*_firstfile = Str_Dup(DFLT_FIRSTFILE, LogInfo);
	*QuietMode = *EchoInits = swFALSE;
	*suid = 0;

	a = 1;
	for (i = 1; i <= nopts; i++) {
		if (a >= argc)
			break;

		/* figure out which option by its position 0-(nopts-1) */
		for (op = 0; op < nopts; op++) {
			if (strncmp(opts[op], argv[a], 2) == 0)
				break; /* found it, move on */
		}

		if (op >= nopts) {
      sw_print_usage();
      sw_error(-1, "\nInvalid option %s\n", argv[a]);

		} else {
			// Use `valopts[op]` in else-branch to avoid
			// `warning: array subscript 6 is above array bounds of 'int[6]' [-Warray-bounds]`

			*str = '\0';
			/* extract value part of option-value pair */
			if (valopts[op]) {
				if ('\0' != argv[a][2]) { /* no space betw opt-value */
					strcpy(str, (argv[a] + 2));

				} else if ('-' != *argv[a + 1]) { /* space betw opt-value */
					strcpy(str, argv[++a]);

				} else if (0 < valopts[op]) { /* required opt-val not found */
					sw_print_usage();
					sw_error(-1, "\nIncomplete option %s\n", opts[op]);
				} /* opt-val not required */
			}

			/* tell us what to do here                   */
			/* set indicators/variables based on results */
			switch (op) {
				case 0: /* -d */
					if (!ChDir(str)) {
						LogError(LogInfo, LOGFATAL, "Invalid project directory (%s)", str);
					}
					break;

				case 1: /* -f */
					free(*_firstfile);
					*_firstfile = Str_Dup(str, LogInfo);
					break;

				case 2: /* -e */
					*EchoInits = swTRUE;
					break;

				case 3: /* -q */
					*QuietMode = swTRUE;
					break;

				case 4: /* -v */
					sw_print_version();
					sw_error(-1, "");
					break;

				case 5: /* -h */
					sw_print_usage();
					sw_error(-1, "");
					break;

				case 6: /* -i */
					*suid = atoi(str) - 1; // Turn suid to base 0

					// Check if the user input a value less than or equal to 0
					if(*suid < 0) {
						LogError(LogInfo, LOGFATAL, "Suid input must be" \
								 " greater than 0.");
					}
					break;

				default:
					LogError(
						LogInfo,
						LOGFATAL,
						"Programmer: bad option in main:sw_init_args:switch"
					);
			}

			a++; /* move to next valid arg-value position */
		}
	} /* end for(i) */

}


void sw_check_log(Bool QuietMode, LOG_INFO* LogInfo) {
	/* =================================================== */
	/* function to be called by atexit() so it's the last
	 * to execute before termination.  This is the place to
	 * do any cleanup or progress reporting.
	 */
	if (LogInfo->logfp != stdout && LogInfo->logfp != stderr) {
		CloseFile(&LogInfo->logfp, LogInfo);
		if (LogInfo->logged && !QuietMode) {
			sw_error(0, "\nCheck logfile for error or status messages.\n");
		}
	}

}
