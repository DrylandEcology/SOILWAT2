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

#ifdef RSOILWAT
  #include <R.h>    // for error(), and warning()
#endif

/* =================================================== */
/*             Local Function Definitions              */
/* --------------------------------------------------- */

/** @brief Print usage and command line options
*/
static void sw_print_usage(void) {
	swprintf(
		"Ecosystem water simulation model SOILWAT2\n"
		"More details at https://github.com/Burke-Lauenroth-Lab/SOILWAT2\n"
		"Usage: ./SOILWAT2 [-d startdir] [-f files.in] [-e] [-q] [-v] [-h] [-s 1]\n"
		"  -d : operate (chdir) in startdir (default=.)\n"
		"  -f : name of main input file (default=files.in)\n"
		"       a preceeding path applies to all input files\n"
		"  -e : echo initial values from site and estab to logfile\n"
		"  -q : quiet mode (print version, print error, message to check logfile)\n"
		"  -v : print version information\n"
		"  -h : print this help information\n"
        "  -s : input start Simulation Unit Identifier (suid)\n"
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

@param[in] argc Number (count) of command line arguments.
@param[in] argv Values of command line arguments.
@param[out] EchoInits Flag to control if inputs are to be output to the user
@param[out] _firstfile First file name to be filled in the program run
@param[out] userSUID Simulation Unit Identifier that the user sent in
@param[out] LogInfo Holds information dealing with logfile output
*/
void sw_init_args(int argc, char **argv, Bool *EchoInits,
                  char **_firstfile, int *userSUID, LOG_INFO* LogInfo) {

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
	char const *opts[] = { "-d", "-f", "-e", "-q", "-v", "-h", "-s" }; /* valid options */
	int valopts[] = { 1, 1, 0, 0, 0, 0, 0 }; /* indicates options with values */
	/* 0=none, 1=required, -1=optional */
	int i, /* looper through all cmdline arguments */
	a, /* current valid argument-value position */
	op, /* position number of found option */
	nopts = sizeof(opts) / sizeof(char *);

	/* Defaults */
	*_firstfile = Str_Dup(DFLT_FIRSTFILE, LogInfo);
    if(LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

	*EchoInits = swFALSE;
    *userSUID = -1; // Default user suid to -1 (not input)

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
      LogError(LogInfo, LOGERROR, "\nInvalid option %s\n", argv[a]);
      return; // Exit function prematurely due to error

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
					LogError(LogInfo, LOGERROR, "\nIncomplete option %s\n", opts[op]);
                    return; // Exit function prematurely due to error
				} /* opt-val not required */
			}

			/* tell us what to do here                   */
			/* set indicators/variables based on results */
			switch (op) {
				case 0: /* -d */
					if (!ChDir(str)) {
						LogError(LogInfo, LOGERROR, "Invalid project directory (%s)", str);
                        return; // Exit function prematurely due to error
					}
					break;

				case 1: /* -f */
					free(*_firstfile);
					*_firstfile = Str_Dup(str, LogInfo);
                    if(LogInfo->stopRun) {
                        return; // Exit function prematurely due to error
                    }
					break;

				case 2: /* -e */
					*EchoInits = swTRUE;
					break;

				case 3: /* -q */
					LogInfo->QuietMode = swTRUE;
					break;

				case 4: /* -v */
					sw_print_version();
					LogError(LogInfo, LOGERROR, "");
                    if(LogInfo->stopRun) {
                        return; // Exit function prematurely due to error
                    }
					break;

				case 5: /* -h */
					sw_print_usage();
					LogError(LogInfo, LOGERROR, "");
                    if(LogInfo->stopRun) {
                        return; // Exit function prematurely due to error
                    }
					break;

                case 6: /* -s */
                    *userSUID = atoi(str) - 1; /* Make suid base0 */

                    if(*userSUID < 0) {
                        LogError(LogInfo, LOGERROR,
                                 "Input suid was less than 1."
                                 " Please input a value >= 1.");
                        return; // Exit function prematurely due to error
                    }
                    break;

				default:
					LogError(
						LogInfo,
						LOGERROR,
						"Programmer: bad option in main:sw_init_args:switch"
					);

                    return; // Exit function prematurely due to error
			}

			a++; /* move to next valid arg-value position */
		}
	} /* end for(i) */

}

/**
 @brief Deal with error that occurred during runtime

  On error, for SOILWAT2, then print the error message to `stderr`
  (unless `QuietMode` is TRUE) and exit with `EXIT_FAILURE`;
  for rSOILWAT2, then issue an error with the error message.

  @param[in,out] LogInfo Holds information dealing with logfile output
*/
void sw_fail_on_error(LOG_INFO* LogInfo) {
    #ifdef RSOILWAT
    if(LogInfo->stopRun) {
        error(LogInfo->errorMsg);
    }

    #else
    if(LogInfo->stopRun) {
        if(!LogInfo->QuietMode) {
            fprintf(stderr, "%s", LogInfo->errorMsg);
        }
        exit(EXIT_FAILURE);
    }
    #endif
}

/**
 * @brief Initialize values within LOG_INFO
 *
 * @param[in] logInitPtr Log file pointer that LOG_INFO will be initialized to
 * @param[in,out] LogInfo Holds information dealing with log file output
*/
void sw_init_logs(FILE* logInitPtr, LOG_INFO* LogInfo) {

	LogInfo->logfp = logInitPtr;

	LogInfo->errorMsg[0] = '\0';

	LogInfo->stopRun = swFALSE;
	LogInfo->numWarnings = 0;
	LogInfo->QuietMode = swFALSE;
}

/**
 * @brief Write warnings that have been accumulated throughout the program/
 * simulation run
 *
 * @param[in,out] LogInfo Holds information dealing with logfile output
*/
void sw_write_warnings(LOG_INFO* LogInfo) {

	int warnMsgNum, warningUpperBound = LogInfo->numWarnings;
	Bool tooManyWarns = swFALSE, QuietMode;
	char tooManyWarnsStr[MAX_LOG_SIZE];
    char tooManyStrFmt[] = "There were a total of %d warnings and"\
                          " only %d were printed.\n";

  QuietMode = (Bool)(LogInfo->QuietMode || isnull(LogInfo->logfp));

	if(warningUpperBound > MAX_MSGS) {
		warningUpperBound = MAX_MSGS;
		tooManyWarns = swTRUE;

        snprintf(tooManyWarnsStr, MAX_LOG_SIZE, tooManyStrFmt,
                 LogInfo->numWarnings, MAX_MSGS);
	}

	#ifdef RSOILWAT
	/* rSOILWAT2: don't issue `warnings()` if quiet */
  if (!QuietMode) {
	    for(warnMsgNum = 0; warnMsgNum < warningUpperBound; warnMsgNum++) {
          warning(LogInfo->warningMsgs[warnMsgNum]);
      }

      if(tooManyWarns) {
          warning(tooManyWarnsStr);
      }
  }
	#else
	/* SOILWAT2: do print warnings and don't notify user if quiet */
  if (!isnull(LogInfo->logfp)) {
      for(warnMsgNum = 0; warnMsgNum < warningUpperBound; warnMsgNum++) {
          fprintf(LogInfo->logfp, "%s\n", LogInfo->warningMsgs[warnMsgNum]);
      }

      if(tooManyWarns) {
          fprintf(LogInfo->logfp, "%s", tooManyWarnsStr);
      }

      if(LogInfo->stopRun) {
          /* Write error message to log file here;
             later (sw_fail_on_error()), we will write it to stderr and crash */
          fprintf(LogInfo->logfp, "%s", LogInfo->errorMsg);
      }

      fflush(LogInfo->logfp);
      // Close logfile (but not if it is stdout or stderr)
      if (LogInfo->logfp == stdout || LogInfo->logfp == stderr) {
          CloseFile(&LogInfo->logfp, LogInfo);
      }
  }

  // Notify the user that there are messages in the logfile (unless QuietMode)
  if(
    (LogInfo->stopRun || LogInfo->numWarnings > 0) &&
    !QuietMode &&
    LogInfo->logfp != stdout &&
    LogInfo->logfp != stderr
  ) {
      fprintf(stderr, "\nCheck logfile for warnings and error messages.\n");
  }
	#endif
}
