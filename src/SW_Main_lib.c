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
#include "include/SW_Output.h"      // for SW_OUT_setup_output
#include "include/Times.h"          // for SW_WT_ReportTime
#include <stdio.h>                  // for fprintf, stderr, fflush, stdout
#include <stdlib.h>                 // for exit, free, EXIT_FA...
#include <string.h>                 // for strncmp

#ifdef RSOILWAT
#include <R.h> // for error(), and warning() from <R_ext/Error.h>
#endif

#if defined(SWMPI)
#include "include/SW_MPI.h"
#endif

#if defined(SWNETCDF)
#include "include/SW_netCDF_Input.h"
#include "include/SW_netCDF_Output.h"
#endif

/* =================================================== */
/*             Local Function Definitions              */
/* --------------------------------------------------- */

/**
@brief Print usage and command line options
*/
static void sw_print_usage(void) {
    sw_printf(
        "Ecosystem water simulation model SOILWAT2\n"
        "More details at https://github.com/Burke-Lauenroth-Lab/SOILWAT2\n"
        "Usage: ./SOILWAT2 [-d startdir] [-f files.in] [-e] [-q] [-v] [-h] "
        "[-s 1] [-t 10] [-r]\n"
        "  -d : operate (chdir) in startdir (default=.)\n"
        "  -f : name of main input file (default=files.in)\n"
        "       a preceeding path applies to all input files\n"
        "  -e : echo initial values from site and estab to logfile\n"
        "  -q : quiet mode (print version, print error, message to check "
        "logfile)\n"
        "  -v : print version information\n"
        "  -h : print this help information\n"
        "  -s : simulate all (0) or one (> 0) simulation unit from the domain\n"
        "       (default = 0)\n"
        "  -t : wall time limit in seconds\n"
        "  -r : rename netCDF domain template file "
        "[name provided in 'Input_nc/files_nc.in']\n"
        "  -p : solely prepare domain/progress, index, and output files\n"
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
@param[out] userSUID Simulation Unit Identifier requested by the user (base1);
            0 indicates that all simulations units within domain are requested
@param[out] wallTimeLimit Terminate simulations early when
            wall time limit is reached
            (default value is set by SW_WT_StartTime())
@param[out] renameDomainTemplateNC Should a domain template netCDF file be
            automatically renamed to provided file name for domain?
@param[out] prepareFiles Should we only prepare domain/progress, index,
            and output files? If so, simulations will occur without this
            flag being turned on
@param[out] LogInfo Holds information on warnings and errors
*/
void sw_init_args(
    int argc,
    char **argv,
    int rank,
    Bool *EchoInits,
    char **firstfile,
    size_t *userSUID,
    double *wallTimeLimit,
    Bool *renameDomainTemplateNC,
    Bool *prepareFiles,
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
        "-d", "-f", "-e", "-q", "-v", "-h", "-s", "-t", "-r", "-p"
    };

    /* indicates options with values: 0=none, 1=required, -1=optional */
    int valopts[] = {1, 1, 0, 0, 0, 0, 1, 1, 0, 0};

    int i;  /* looper through all cmdline arguments */
    int a;  /* current valid argument-value position */
    int op; /* position number of found option */
    int nopts = sizeof(opts) / sizeof(char *);
    double doubleUserSUID = 0.;

    /* Defaults */
    *firstfile = Str_Dup(DFLT_FIRSTFILE, LogInfo);
    if (LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

    *EchoInits = swFALSE;
    *renameDomainTemplateNC = swFALSE;
    *userSUID = 0; // Default (if no input) is 0 (i.e., all suids)

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
            LogError(LogInfo, LOGERROR, "\nInvalid option %s\n", argv[a]);
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
                LogError(
                    LogInfo, LOGERROR, "\nIncomplete option %s\n", opts[op]
                );
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
                return; // Exit function prematurely due to error
            }
            break;

        case 1: /* -f */
            free(*firstfile);
            *firstfile = Str_Dup(str, LogInfo);
            if (LogInfo->stopRun) {
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
            if (rank == 0) {
                sw_print_version();
                LogError(LogInfo, LOGERROR, "");
                if (LogInfo->stopRun) {
                    return; // Exit function prematurely due to error
                }
            }
            break;

        case 5: /* -h */
            if (rank == 0) {
                sw_print_usage();
                LogError(LogInfo, LOGERROR, "");
                if (LogInfo->stopRun) {
                    return; // Exit function prematurely due to error
                }
            }
            break;

        case 6: /* -s */
            *userSUID = sw_strtoul(str, errMsg, LogInfo);
            if (LogInfo->stopRun) {
                return; // Exit function prematurely due to error
            }

            /* Check that user input can be represented by userSUID
             * (currently, unsigned long) */
            /* Expect that conversion of string to double results in the
             * same value as conversion of userSUID to double */
            doubleUserSUID = sw_strtod(str, errMsg, LogInfo);
            if (LogInfo->stopRun) {
                return; // Exit function prematurely due to error
            }

            if (!EQ(doubleUserSUID, (double) *userSUID)) {
                LogError(
                    LogInfo,
                    LOGERROR,
                    "User input not recognized as a simulation unit "
                    "('-s %s' vs. %lu).",
                    str,
                    *userSUID
                );
                return; // Exit function prematurely due to error
            }
            break;

        case 7: /* -t */
            *wallTimeLimit = sw_strtod(str, errMsg, LogInfo);
            if (LogInfo->stopRun) {
                return; // Exit function prematurely due to error
            }
            break;

        case 8: /* -r */
            *renameDomainTemplateNC = swTRUE;
            break;

        case 9: /* -p */
#if defined(SWNETCDF)
            *prepareFiles = swTRUE;
#else
            *prepareFiles = swFALSE;
#endif
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
        error("%s", LogInfo->errorMsg);
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
    LogInfo->numWarnings = 0;
    LogInfo->numDomainWarnings = 0;
    LogInfo->numDomainErrors = 0;

#if defined(SWMPI)
    LogInfo->logfps = NULL;
    LogInfo->numFiles = 0;
#endif
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
            warning("%s%s", header, LogInfo->warningMsgs[warnMsgNum]);
        }

        if (tooManyWarns) {
            warning("%s%s", header, tooManyWarnsStr);
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

    // If we are running with MPI, we need to close all opened log files
    // otherwise, when not using MPI, we only need to close one
#if defined(SWMPI)
    int file;

    for (file = 0; file < LogInfo->numFiles; file++) {
        logfp = LogInfo->logfps[file];
#endif
        // Close logfile (but not if it is stdout or stderr)
        if (logfp != stdout || logfp != stderr) {
            CloseFile(&logfp, LogInfo);
        }
#if defined(SWMPI)
    }
#endif

    if (rank == 0) {
        // Notify the user that there are messages in the logfile (unless
        // QuietMode)
        if ((LogInfo->numDomainErrors > 0 || LogInfo->numDomainWarnings > 0 ||
             LogInfo->stopRun || LogInfo->numWarnings > 0) &&
            !QuietMode && LogInfo->logfp != stdout &&
            LogInfo->logfp != stderr) {
            (void) fprintf(
                stderr, "\nCheck logfile for warnings and error messages.\n"
            );

            if (LogInfo->numDomainWarnings > 0) {
                (void) fprintf(
                    stderr,
                    "Simulation units with warnings: n = %lu\n",
                    LogInfo->numDomainWarnings
                );
            }

            if (LogInfo->numDomainErrors > 0) {
                (void) fprintf(
                    stderr,
                    "Simulation units with an error: n = %lu\n",
                    LogInfo->numDomainErrors
                );
            }
        }
    }
}

/**
@brief Wrapper function to setup outputs and handle MPI

@param[in] rank Process number known to MPI for the current process (aka rank)
@param[in] worldSize Total number of processes that the MPI run has created
@param[in] procName Name of the processor/node the current processes is
    running on
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
    int rank,
    int worldSize,
    const char *procName,
    Bool prepareFiles,
    SW_RUN *sw_template,
    SW_DOMAIN *SW_Domain,
    LOG_INFO *LogInfo
) {
#if defined(SWNETCDF)
    Bool doOutStuff = swTRUE;
#else
    (void) prepareFiles;
#endif
#if defined(SWMPI)
    int procJob;

    if (!prepareFiles) {
        SW_MPI_setup(
            rank, worldSize, procName, SW_Domain, sw_template, LogInfo
        );
        if (SW_MPI_setup_fail(LogInfo->stopRun, MPI_COMM_WORLD)) {
            return;
        }
        procJob = SW_Domain->SW_Designation.procJob;
        doOutStuff = (Bool) (procJob == SW_MPI_PROC_IO);
    }
#else
    (void) rank;
    (void) worldSize;
    (void) procName;
#endif

    // initialize output
    SW_OUT_setup_output(
        SW_Domain->nMaxSoilLayers,
        SW_Domain->nMaxEvapLayers,
        sw_template->VegEstabIn.count,
        sw_template->VegEstabIn.parms,
        &SW_Domain->OutDom,
        LogInfo
    );
#if defined(SWMPI)
    if (SW_MPI_setup_fail(LogInfo->stopRun, MPI_COMM_WORLD)) {
        return;
    }
#else
    if (LogInfo->stopRun) {
        return;
    }
#endif

#if defined(SWNETCDF)
#if defined(SWMPI)
    if (rank == SW_MPI_ROOT) {
#endif
        SW_NCOUT_read_out_vars(
            &SW_Domain->OutDom,
            SW_Domain->SW_PathInputs.txtInFiles,
            sw_template->VegEstabIn.parms,
            LogInfo
        );
        if (LogInfo->stopRun) {
            return;
        }
#if defined(SWMPI)
    }
    if (SW_MPI_setup_fail(LogInfo->stopRun, MPI_COMM_WORLD)) {
        return;
    }
#endif

#if defined(SWMPI)
    if (doOutStuff) {
        SW_MPI_ncout_info(
            rank,
            SW_Domain->SW_Designation.groupComm,
            &SW_Domain->OutDom,
            LogInfo
        );
    }
    if (SW_MPI_setup_fail(LogInfo->stopRun, MPI_COMM_WORLD)) {
        return;
    }
#endif

    if (!prepareFiles) {
        if (doOutStuff) {
            SW_NCOUT_create_units_converters(&SW_Domain->OutDom, LogInfo);
        }
#if defined(SWMPI)
        if (SW_MPI_setup_fail(LogInfo->stopRun, MPI_COMM_WORLD)) {
            return;
        }
#else
        if (LogInfo->stopRun) {
            return;
        }
#endif

#if defined(SWMPI)
        if (rank > SW_MPI_ROOT && doOutStuff) {
            SW_NCIN_create_units_converters(&SW_Domain->netCDFInput, LogInfo);
        }
#endif
    }
#endif // SWNETCDF
}

/**
@brief Wrapper function to finalize the program depending on if SWMPI
is enabled

@param[in] rank Process number known to MPI for the current process (aka rank)
@param[in] size Number of processors (world size) within the
    communicator MPI_COMM_WORLD
@param[in] SW_Domain Struct of type SW_DOMAIN holding constant
    temporal/spatial information for a set of simulation runs
@param[in] SW_WallTime Struct of type SW_WALLTIME that holds timing
    information for the program run
@param[in] setupFailed A flag specifying if the program was exited
    before setup was completed
@param[in] LogInfo Holds information on warnings and errors
*/
void sw_finalize_program(
    int rank,
    int size,
    SW_DOMAIN *SW_Domain,
    SW_WALLTIME *SW_WallTime,
    Bool setupFailed,
    LOG_INFO *LogInfo
) {
#if defined(SWMPI)
    int procJob = SW_Domain->SW_Designation.procJob;

    /* Report information for the following scenarios
        1) Failed during setup - error(s)/warning(s)
        2) Failed during simulation runs - simulations statistics *
        3) Successful program run - simulation statistics *

        * = All warnings/errors are reported through I/O processes
    */
    SW_MPI_report_log(
        rank,
        size,
        SW_Domain->datatypes[eSW_MPI_WallTime],
        SW_WallTime,
        SW_Domain,
        setupFailed,
        LogInfo
    );

    // Free types and communicators
    SW_MPI_free_comms_types(
        rank, &SW_Domain->SW_Designation, SW_Domain->datatypes, LogInfo
    );
#else
    sw_write_warnings("(main) ", LogInfo);
    SW_WT_ReportTime(*SW_WallTime, LogInfo);
    sw_fail_on_error(LogInfo);

    (void) size;
    (void) setupFailed;
    (void) SW_WallTime;
    (void) SW_Domain;
#endif

    sw_wrapup_logs(rank, LogInfo);

#if defined(SWMPI)
    SW_MPI_finalize(procJob, LogInfo);
#endif
}
