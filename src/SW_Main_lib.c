/********************************************************/
/********************************************************/
/*  Application: SOILWAT - soilwater dynamics simulator
 *  Source file: Main.c
 *  Type: main module
 *  Purpose: Contains the main loops and initializations.
 *
 06/24/2013	(rjm)	included "SW_Site.h" and "SW_Weather.h";
 added calls at end of main() to SW_SIT_clear_layers() and
 SW_WTH_clear_runavg_list() to free memory
 */
/********************************************************/
/********************************************************/

/* =================================================== */
/*                INCLUDES / DEFINES                   */
/* --------------------------------------------------- */

#include "include/SW_Main_lib.h"    // for sw_fail_on_error, sw_init_args
#include "include/filefuncs.h"      // for LogError, ChDir, CloseFile, sw_m...
#include "include/generic.h"        // for LOGERROR, swFALSE, Bool, swTRUE
#include "include/myMemory.h"       // for Str_Dup
#include "include/SW_datastructs.h" // for LOG_INFO
#include "include/SW_Defines.h"     // for MAX_MSGS, MAX_LOG_SIZE, BUILD_DATE
#include "include/Times.h"          // for SW_WT_ReportTime

#if defined(RSOILWAT)
#include <R.h> // for Rf_error(), and Rf_warning() from <R_ext/Error.h>
#else

#include "include/SW_Output.h" // for SW_OUT_set_out_counts

#if defined(SWNETCDF)
#include "include/SW_netCDF_General.h"  // for SW_NCOUT_create_units_converters
#include "include/SW_netCDF_Output.h"   // for SW_NCOUT_create_units_converters
#include "include/SW_Output_outarray.h" // for SW_OUT_calc_iOUToffset
#endif

#if defined(SWMPI)
#include "include/SW_MPI.h"
#include "include/SW_netCDF_Input.h" // for SW_NCOUT_create_units_converters
#include <mpi.h>                     // for MPI_COMM_WORLD
#endif

#endif

#include <stdio.h>  // for fprintf, stderr, fflush, stdout
#include <stdlib.h> // for exit, free, EXIT_FA...
#include <string.h> // for strncmp

/* =================================================== */
/*             Local Function Definitions              */
/* --------------------------------------------------- */

/**
@brief Print usage and command line options
*/
static void sw_print_usage(void) {
    sw_printf(
        "SOILWAT2: an ecosystem water simulation model.\n"
        "More details at https://github.com/DrylandEcology/SOILWAT2\n"
        "Usage: SOILWAT2 [-d <directory>] [-f <mainFile>] [-e] [-q] [-v] [-h] "
        "[-t <number>] [-r] [-p]\n"
        "Options:\n"
        "  -d : Operate (chdir) in <directory> (default = '.').\n"
        "  -f : Main input file relative to <directory>"
        "(default = 'files.in').\n"
        "  -e : Echo inputs from text-based input files.\n"
        "  -q : Quiet mode (do not write messages to the console).\n"
        "  -v : Print version information and exit.\n"
        "  -h : Print this help information and exit.\n"
        "  -t : Set a wall time limit where <number> is in units of seconds.\n"
        "  -r : A netCDF domain template file is automatically renamed\n"
        "       to the domain file name provided in 'Input_nc/files_nc.in'.\n"
        "  -p : Prepare domain, progress, index, and output netCDF files\n"
        "       but do not run simulations.\n"
        "  -s : Run the next number of days for all sites then exit the "
        "       program.\n"
    );
}

/* =================================================== */
/*             Global Function Definitions             */
/* --------------------------------------------------- */

/**
@brief Print version information
*/
void sw_print_version(void) {
    sw_printf("SOILWAT2 version: %s\n", SW2_VERSION);

#if defined(STEPWAT)
    sw_printf("Compiled as library for STEPWAT2");

#elif defined(RSOILWAT2)
    sw_printf("Compiled as library for rSOILWAT2");

#else
    sw_printf("Capabilities: ");
#if defined(SWNETCDF)
    sw_printf("netCDF-c");
#if defined(SWUDUNITS)
    sw_printf(", udunits2");
#endif
#if defined(SWMPI)
    sw_printf(", MPI");
#endif
#else
    sw_printf("text");
#endif
#endif

    sw_printf("\n");

#if defined(SWMPI)
    sw_printf("SWMPI           : N_SUID_ASSIGN = %d\n", N_SUID_ASSIGN);
#endif

#ifndef RSOILWAT
    sw_printf(
        "Compiled        : by %s, on %s, on %s %s\n",
        USERNAME,
        HOSTNAME,
        BUILD_DATE,
        BUILD_TIME
    );
#endif
}

/**
@brief Initializes arguments and sets indicators/variables based on results.

@param[in] argc Number (count) of command line arguments.
@param[in] argv Values of command line arguments.
@param[in] rank Process number known to MPI for the current process (aka rank);
    defaults to 0 (main process) if we are running sequentially
@param[out] EchoInits Flag to control if inputs are to be output to the user
@param[out] firstfile First file name to be filled in the program run
@param[out] wallTimeLimit Terminate simulations early when
            wall time limit is reached
            (default value is set by SW_WT_StartTime())
@param[out] renameDomainTemplateNC Should a domain template netCDF file be
            automatically renamed to provided file name for domain?
@param[out] prepareFiles Should we only prepare domain/progress, index,
            and output files? If so, simulations will occur without this
            flag being turned on
@param[out] endQuietly A flag specifying if no messages should be produced,
    e.g., if SOILWAT2 was called to print help or version only.
@param[in] runSimDayLen The number of days the simulations are to be run for
@param[out] LogInfo Holds information on warnings and errors
*/
void sw_init_args(
    int argc,
    char **argv,
    int rank,
    Bool *EchoInits,
    char **firstfile,
    double *wallTimeLimit,
    Bool *renameDomainTemplateNC,
    Bool *prepareFiles,
    Bool *endQuietly,
    TimeInt *runSimDayLen,
    LOG_INFO *LogInfo
) {

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
    const char *errMsg = "command-line";

    /* valid options */
    char const *opts[] = {
        "-d", "-f", "-e", "-q", "-v", "-h", "-t", "-r", "-p", "-s"
    };

    /* indicates options with values: 0=none, 1=required, -1=optional */
    int valopts[] = {1, 1, 0, 0, 0, 0, 1, 0, 0, 1};

    int i;  /* looper through all cmdline arguments */
    int a;  /* current valid argument-value position */
    int op; /* position number of found option */
    int nopts = sizeof(opts) / sizeof(char *);

    /* Defaults */
    *firstfile = Str_Dup(DFLT_FIRSTFILE, LogInfo);
    if (LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

    *EchoInits = swFALSE;
    *renameDomainTemplateNC = swFALSE;
    *endQuietly = swFALSE;
    *runSimDayLen = 0;

    a = 1;
    for (i = 1; i <= nopts; i++) {
        if (a >= argc) {
            break;
        }

        /* figure out which option by its position 0-(nopts-1) */
        for (op = 0; op < nopts; op++) {
            if (strncmp(opts[op], argv[a], 2) == 0) {
                break; /* found it, move on */
            }
        }

        if (op >= nopts) {
            sw_print_usage();
            LogError(LogInfo, LOGERROR, "Invalid option %s", argv[a]);
            return; // Exit function prematurely due to error
        }

        // Use `valopts[op]` in else-branch to avoid
        // `warning: array subscript 6 is above array bounds of 'int[6]'
        // [-Warray-bounds]`

        *str = '\0';
        /* extract value part of option-value pair */
        if (valopts[op]) {
            if ('\0' != argv[a][2]) {
                /* no space betw opt-value */
                (void) snprintf(str, sizeof str, "%s", (argv[a] + 2));

            } else if ('-' != *argv[a + 1]) {
                /* space betw opt-value */
                (void) snprintf(str, sizeof str, "%s", argv[++a]);

            } else if (0 < valopts[op]) {
                /* required opt-val not found */
                sw_print_usage();
                LogError(LogInfo, LOGERROR, "Incomplete option %s", opts[op]);
                return; // Exit function prematurely due to error
            }
            /* opt-val not required */
        }

        /* tell us what to do here                   */
        /* set indicators/variables based on results */
        switch (op) {
        case 0: /* -d */
            if (!ChDir(str)) {
                LogError(
                    LogInfo, LOGERROR, "Invalid project directory (%s)", str
                );
            }
            break;

        case 1: /* -f */
            free(*firstfile);
            *firstfile = Str_Dup(str, LogInfo);
            break;

        case 2: /* -e */
            *EchoInits = swTRUE;
            break;

        case 3: /* -q */
            LogInfo->QuietMode = swTRUE;
            break;

        case 4: /* -v */
            if (rank == ROOT_PROC) {
                sw_print_version();
                *endQuietly = swTRUE;
                return;
            }
            break;

        case 5: /* -h */
            if (rank == ROOT_PROC) {
                sw_print_usage();
                *endQuietly = swTRUE;
                return;
            }
            break;

        case 6: /* -t */
            *wallTimeLimit = sw_strtod(str, errMsg, LogInfo);
            break;

        case 7: /* -r */
            *renameDomainTemplateNC = swTRUE;
            break;

        case 8: /* -p */
#if defined(SWNETCDF)
            *prepareFiles = swTRUE;
#else
            *prepareFiles = swFALSE;
#endif
            break;

        case 9: /* -s */
            *runSimDayLen = (TimeInt) sw_strtoi(str, errMsg, LogInfo);
            break;

        default:
            LogError(
                LogInfo,
                LOGERROR,
                "Programmer: bad option in main:sw_init_args:switch"
            );
            break;
        }
        if (LogInfo->stopRun) {
            return; // Exit function prematurely due to error
        }

        a++; /* move to next valid arg-value position */
    } /* end for(i) */
}

/**
@brief Deal with error that occurred during runtime

On error, for SOILWAT2, then print the error message to `stderr`
(unless `QuietMode` is TRUE) and exit with `EXIT_FAILURE`;
for rSOILWAT2, then issue an error with the error message.

@param[in] LogInfo Holds information on warnings and errors
*/
void sw_fail_on_error(LOG_INFO *LogInfo) {
#ifdef RSOILWAT
    if (LogInfo->stopRun) {
        Rf_error("%s", LogInfo->errorMsg);
    }

#else
    if (LogInfo->stopRun) {
        if (!LogInfo->QuietMode) {
            (void) fprintf(stderr, "%s", LogInfo->errorMsg);
        }
        if (LogInfo->printProgressMsg) {
            SW_MSG_ROOT("ended.", 0);
        }
        exit(EXIT_FAILURE);
    }
#endif
}

/**
@brief Initialize values within LOG_INFO

@param[in] logInitPtr Log file pointer that LOG_INFO will be initialized to
@param[in,out] LogInfo Holds information dealing with log file output
*/
void sw_init_logs(FILE *logInitPtr, LOG_INFO *LogInfo) {

    LogInfo->logfp = logInitPtr;

    LogInfo->errorMsg[0] = '\0';

    LogInfo->stopRun = swFALSE;
    LogInfo->QuietMode = swFALSE;
    LogInfo->printProgressMsg = swFALSE;
    LogInfo->loggedError = swFALSE;
    LogInfo->numWarnings = 0;
    LogInfo->numDomainWarnings = 0;
    LogInfo->numDomainErrors = 0;
    LogInfo->prevNumWarms = 0;

    LogInfo->logStage[0] = '\0';
    LogInfo->logSUID[0] = '\0';
    LogInfo->hasLogSUID = swFALSE;
    LogInfo->logDate[0] = '\0';
    LogInfo->hasLogDate = swFALSE;
}

/**
@brief Write warnings that have been accumulated throughout the program/
simulation run

@param[in] header String that is printed before warning and error messages;
    may be empty.
@param[in] LogInfo Holds information on warnings and errors
*/
void sw_write_warnings(const char *header, LOG_INFO *LogInfo) {

    int warnMsgNum;
    int warningUpperBound = LogInfo->numWarnings;
    Bool tooManyWarns = swFALSE;
    char tooManyWarnsStr[MAX_LOG_SIZE];


    if (warningUpperBound > MAX_MSGS) {
        warningUpperBound = MAX_MSGS;
        tooManyWarns = swTRUE;

        (void) snprintf(
            tooManyWarnsStr,
            MAX_LOG_SIZE,
            "There were a total of %d warnings and only %d were printed.\n",
            LogInfo->numWarnings,
            MAX_MSGS
        );
    }

#ifdef RSOILWAT
    /* rSOILWAT2: don't issue `warnings()` if quiet */
    Bool QuietMode = (Bool) (LogInfo->QuietMode || isnull(LogInfo->logfp));

    if (!QuietMode) {
        for (warnMsgNum = 0; warnMsgNum < warningUpperBound; warnMsgNum++) {
            Rf_warning("%s%s", header, LogInfo->warningMsgs[warnMsgNum]);
        }

        if (tooManyWarns) {
            Rf_warning("%s%s", header, tooManyWarnsStr);
        }
    }
#else
    int writeRes = 0;

    /* SOILWAT2: do print warnings and don't notify user if quiet */
    if (!isnull(LogInfo->logfp)) {
        for (warnMsgNum = 0; warnMsgNum < warningUpperBound; warnMsgNum++) {
            writeRes = fprintf(
                LogInfo->logfp, "%s%s", header, LogInfo->warningMsgs[warnMsgNum]
            );

            if (writeRes < 0) {
                goto writeErrMsg;
            }
        }

        if (tooManyWarns) {
            writeRes = fprintf(LogInfo->logfp, "%s%s", header, tooManyWarnsStr);
            if (writeRes < 0) {
                goto writeErrMsg;
            }
        }

        if (LogInfo->stopRun) {
            /* Write error message to log file here;
               later (sw_fail_on_error()), we will write it to stderr and crash
             */
            writeRes =
                fprintf(LogInfo->logfp, "%s%s", header, LogInfo->errorMsg);
            if (writeRes < 0) {
                goto writeErrMsg;
            }
        }

        writeRes = fflush(LogInfo->logfp);

    writeErrMsg: {
        if (writeRes < 0) {
            SW_MSG_ROOT(
                "Failed to write all warning/error messages to logfile.\n", 0
            );
        }
    }
    }
#endif
}

#if !defined(RSOILWAT)
/**
@brief Close logfile and notify user

Close logfile and notify user that logfile has content (unless QuietMode);
print number of simulation units with warnings and errors (if any).

@param[in] rank Process number known to MPI for the current process (aka rank);
    can only be different when SWMPI is enabled
@param[in] LogInfo Holds information on warnings and errors
*/
void sw_wrapup_logs(int rank, LOG_INFO *LogInfo) {
    Bool QuietMode = (Bool) (LogInfo->QuietMode || isnull(LogInfo->logfp));
    FILE *logfp = LogInfo->logfp;

    // Close logfile (but not if it is stdout or stderr)
    if (logfp != stdout || logfp != stderr) {
        CloseFile(&logfp, LogInfo);
    }

    if (rank == ROOT_PROC) {
        // Notify the user that there are messages in the logfile (unless
        // QuietMode)
        if ((LogInfo->numDomainErrors > 0 || LogInfo->numDomainWarnings > 0 ||
             LogInfo->stopRun || LogInfo->numWarnings > 0) &&
            !QuietMode && logfp != stdout && logfp != stderr) {
            (void) fprintf(
                stderr, "\nCheck logfile for warnings and error messages.\n"
            );

            if (LogInfo->numDomainWarnings > 0) {
                (void) fprintf(
                    stderr,
                    "Simulation units with warnings: n = %zu\n",
                    LogInfo->numDomainWarnings
                );
            }

            if (LogInfo->numDomainErrors > 0) {
                (void) fprintf(
                    stderr,
                    "Simulation units with an error: n = %zu\n",
                    LogInfo->numDomainErrors
                );
            }
        }
    }
}

/**
@brief Wrapper function to setup outputs and handle MPI

@param[in] worldSize Total number of processes that the MPI run has created
(only relevant with SWMPI enabled)
@param[in] prepareFiles Should we only prepare domain/progress, index,
    and output files? If so, simulations will occur without this
    flag being turned on
@param[in,out] sw_template Template SW_RUN for the function to use as a
    reference for local versions of SW_RUN
@param[in,out] SW_Domain Struct of type SW_DOMAIN holding constant
    temporal/spatial information for a set of simulation runs
@param[out] LogInfo Holds information on warnings and errors
*/
void sw_setup_prog_data(
    int worldSize,
    Bool prepareFiles,
    SW_RUN *sw_template,
    SW_DOMAIN *SW_Domain,
    LOG_INFO *LogInfo
) {
#if defined(SWNETCDF)
    size_t totNSites = SW_Domain->nSitesInSubDom;
    int strideYears = SW_Domain->OutDom.netCDFOutput.strideOutYears;
    TimeInt n_years = SW_Domain->endyr - SW_Domain->startyr + 1;

    checkReturn(LogInfo->stopRun);

    if (!prepareFiles) {
        SW_NC_proc_sites(SW_Domain, LogInfo);
        checkReturn(LogInfo->stopRun);
    }
#endif

    SW_OUT_set_out_counts(
        SW_Domain->nMaxSoilLayers,
        sw_template->VegEstabIn->count,
        &SW_Domain->OutDom
    );
    if (LogInfo->stopRun) {
        return;
    }

#if defined(SWNETCDF)
    SW_NCOUT_read_out_vars(
        &SW_Domain->OutDom,
        SW_Domain->SW_PathInputs.txtInFiles,
        &sw_template->VegEstabIn->parms,
        LogInfo
    );
    checkReturn(LogInfo->stopRun);

    if (!prepareFiles) {
        SW_NCOUT_create_units_converters(&SW_Domain->OutDom, LogInfo);
    }
    checkReturn(LogInfo->stopRun);

    sw_template->SW_PathOutputs->numOutFiles =
        (strideYears == -1) ?
            1 :
            (unsigned int) ceil((double) n_years / strideYears);

    // Attempt to calculate an optimal temporal chunk for output variables
    SW_NC_calc_read_write_sizes(worldSize, SW_Domain, LogInfo);

    SW_OUT_calc_iOUToffset(
        SW_Domain->OutDom.nrow_OUT,
        SW_Domain->OutDom.nvar_OUT,
        totNSites,
        SW_Domain->OutDom.use,
        SW_Domain->OutDom.nsl_OUT,
        SW_Domain->OutDom.npft_OUT,
        SW_Domain->OutDom.netCDFOutput.reqOutputVars,
        SW_Domain->OutDom.netCDFOutput.iOUToffset
    );

    //--- Sum up number of output combinations across variables - soil layers -
    // vegtypes ------
    SW_OUT_sum_ncols(SW_Domain, LogInfo);
#else
    SW_OUT_set_colnames(
        SW_Domain->nMaxSoilLayers,
        &sw_template->VegEstabIn->parms,
        SW_Domain->OutDom.ncol_OUT,
        SW_Domain->OutDom.colnames_OUT,
        LogInfo
    );
    (void) prepareFiles;
    (void) worldSize;
#endif // SWNETCDF
}

/**
@brief Wrapper function to finalize the program depending on if SWMPI
is enabled

@param[in] rank Process number known to MPI for the current process (aka rank)
@param[in] nActiveSites Number of active sites the process controls
@param[in] SW_WallTime Struct of type SW_WALLTIME that holds timing
    information for the program run
@param[in] endQuietly A flag specifying if no messages should be produced,
    e.g., if SOILWAT2 was called to print help or version only.
@param[in] LogInfo Holds information on warnings and errors
*/
void sw_finalize_program(
    int rank,
    size_t nActiveSites,
    SW_WALLTIME *SW_WallTime,
    Bool endQuietly,
    LOG_INFO *LogInfo
) {
    if (!endQuietly) {
        sw_write_warnings("", LogInfo);

#if defined(SWMPI)
        SW_MPI_get_end_info(rank, nActiveSites, SW_WallTime, LogInfo);
#endif

        if (rank == ROOT_PROC) {
            SW_WT_ReportTime(*SW_WallTime, LogInfo);
        }

        sw_wrapup_logs(rank, LogInfo);
    }

#if defined(SWMPI)
    SW_MPI_finalize();
#else
    sw_fail_on_error(LogInfo);

    (void) nActiveSites;
    (void) SW_WallTime;
#endif
}
#endif // !defined(RSOILWAT)
