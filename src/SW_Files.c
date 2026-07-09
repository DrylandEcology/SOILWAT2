/********************************************************/
/********************************************************/
/*	Source file: Files.c
 Type: module
 Application: SOILWAT - soilwater dynamics simulator
 Purpose: Read / write and otherwise manage the model's
 parameter file information.
 History:

 8/28/01 -- INITIAL CODING - cwb

 1/24/02 -- added facility for logfile

 10-May-02 -- Added conditionals for interfacing STEPPE.

 09/30/2011	(drs)	added function SW_WeatherPrefix(): so that SW_Weather
 can access local variable txtWeatherPrefix that is read in now in SW_F_read()
 new module-level variable static char txtWeatherPrefix[FILENAME_MAX]; read in
 in function SW_F_read() from file files.in line 6

 09/30/2011	(drs)	added function SW_OutputPrefix(): so that SW_Output can
 access local variable outputPrefix that is read in now in SW_F_read() new
 module-level variable static char outputPrefix[FILENAME_MAX]; read in in
 function SW_F_read() from file files.in line 12: / for same directory, or e.g.,
 Output/
 */
/********************************************************/
/********************************************************/

/* =================================================== */
/* =================================================== */
/*                INCLUDES / DEFINES                   */
/* --------------------------------------------------- */
#include "include/SW_Files.h"       // for eLog, eOutputDaily, eOutputDaily...
#include "include/filefuncs.h"      // for CloseFile, LogError, DirExists
#include "include/generic.h"        // for LOGERROR, isnull, IntUS, LOGWARN
#include "include/myMemory.h"       // for Str_Dup
#include "include/SW_datastructs.h" // for LOG_INFO, SW_NFILES, SW_PATH_INPUTS
#include "include/SW_Defines.h"     // for MAX_FILENAMESIZE
#include "include/SW_Main_lib.h"    // for sw_write_warnings
#include <stdio.h>                  // for FILENAME_MAX, NULL, FILE, stderr
#include <stdlib.h>                 // for free
#include <string.h>                 // for memccpy, strcmp, strlen, memcpy

#if defined(SWNETCDF)
#include "include/SW_netCDF_Input.h"

#if defined(SWMPI)
#include "include/SW_MPI.h"
#endif
#endif

/* =================================================== */
/*             Local Function Definitions              */
/* --------------------------------------------------- */

#if defined(SOILWAT)
/**
@brief Helper function to create a logfile name and create said file

@param[in] rank Process number known to MPI for the current process (aka rank)
@param[in] fileName Provided name of the logfile to be created
@param[out] LogInfo Holds information on warnings and errors
*/
static FILE *create_logfile(int rank, const char *fileName, LOG_INFO *LogInfo) {

#if defined(SWMPI)
    char *fileNamePtr = NULL;

    char logBuffer[LARGE_VALUE] = "\0";
    char dir[MAX_FILENAMESIZE] = "\0";
    const char *baseName = BaseName(fileName);

    DirName(fileName, dir);

    snprintf(logBuffer, sizeof logBuffer, "%srank_%d_%s", dir, rank, baseName);

    fileNamePtr = logBuffer;
#else
    const char *fileNamePtr = fileName;
    (void) rank;
    (void) fileName;
#endif

    return OpenFile(fileNamePtr, "w", LogInfo);
}
#endif

/* =================================================== */
/*             Global Function Definitions             */
/* --------------------------------------------------- */

/**
@brief Removes files from the specified directory.

If in txt-mode, then all files in the specified directory are removed.
If in nc-mode, then csv-files (pattern *.csv) are removed.

@param[in] outDir Name of the directory to clean
@param[out] LogInfo Holds information on warnings and errors
*/
void SW_F_CleanOutDir(char *outDir, LOG_INFO *LogInfo) {
    /* AKT 08/28/2016 */

    char inbuf[FILENAME_MAX] = {'\0'};
    Bool clearDir = swTRUE;
    const int maxDepth = 10;

#if defined(SWNETCDF)
    clearDir = swFALSE;
    (void) snprintf(inbuf, FILENAME_MAX, "%s*.csv", outDir);
#else
    (void) snprintf(inbuf, FILENAME_MAX, "%s", outDir);
#endif

    if (!RemoveFiles(inbuf, clearDir, maxDepth, LogInfo)) {
        LogError(
            LogInfo,
            LOGWARN,
            "Couldn't remove CSV files the directory '%s'.\n",
            outDir
        );
    }
}

/**
@brief Read `first` input file `eFirst` that contains names of the remaining
input files.

@param[in] rank Process number known to MPI for the current process (aka rank)
@param[in,out] SW_PathInputs Struct of type SW_PATH_INPUTS which
holds basic information about input files and values
@param[out] LogInfo Holds information on warnings and errors

@note If input file `eFirst` changes, particularly if the locations of the
`txtWeatherPrefix` and/or `outputPrefix` change; then update the hard-coded
line numbers.

@sideeffect
Update values of variables within SW_PATH_INPUTS:
    - `txtWeatherPrefix`
    - `outputPrefix`
    - `txtInFiles`
    - `logfp` for SOILWAT2-standalone
*/
void SW_F_read(int rank, SW_PATH_INPUTS *SW_PathInputs, LOG_INFO *LogInfo) {
#ifdef SWDEBUG
    int debug = 0;
#endif

    FILE *f;
    int lineno = 0;
    int fileno = 0;
    int resSNP;
    char buf[FILENAME_MAX];
    char inbuf[MAX_FILENAMESIZE];
    char logDir[MAX_FILENAMESIZE];

    char *MyFileName = SW_PathInputs->txtInFiles[eFirst];
    f = OpenFile(MyFileName, "r", LogInfo);
    if (LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

    while (GetALine(f, inbuf, MAX_FILENAMESIZE)) {

#ifdef SWDEBUG
        if (debug) {
            sw_printf(
                "'SW_F_read': line = %d/%d: %s\n", lineno, eEndFile, inbuf
            );
        }
#endif

        switch (lineno) {
        case eWeather:
            resSNP = snprintf(
                SW_PathInputs->txtWeatherPrefix,
                sizeof SW_PathInputs->txtWeatherPrefix,
                "%s",
                inbuf
            );
            if (resSNP < 0 ||
                (unsigned) resSNP >= (sizeof SW_PathInputs->txtWeatherPrefix)) {
                LogError(
                    LogInfo,
                    LOGERROR,
                    "weather prefix is too long: '%s'.",
                    inbuf
                );
                goto closeFile;
            }
            break;
        default:
            if (++fileno == SW_NINFILES) {
                break;
            }

            if (!isnull(SW_PathInputs->txtInFiles[fileno])) {
                free(SW_PathInputs->txtInFiles[fileno]);
            }

            resSNP = snprintf(
                buf, sizeof buf, "%s%s", SW_PathInputs->SW_ProjDir, inbuf
            );
            if (resSNP < 0 || (unsigned) resSNP >= (sizeof buf)) {
                LogError(
                    LogInfo, LOGERROR, "input file name is too long: '%s'.", buf
                );
                goto closeFile;
            }

            SW_PathInputs->txtInFiles[fileno] = Str_Dup(buf, LogInfo);
            if (LogInfo->stopRun) {
                goto closeFile;
            }
        }

        lineno++;
    }

    if (fileno < SW_NINFILES - 1) {
        LogError(
            LogInfo, LOGERROR, "Too few files (%d) in %s", fileno, MyFileName
        );
        goto closeFile;
    }

#ifdef SOILWAT
    if (0 == strcmp(SW_PathInputs->txtInFiles[eLog], "stdout")) {
        LogInfo->logfp = stdout;
    } else if (0 == strcmp(SW_PathInputs->txtInFiles[eLog], "stderr")) {
        LogInfo->logfp = stderr;
    } else {
        DirName(SW_PathInputs->txtInFiles[eLog], logDir);

        if (!DirExists(logDir) && rank == ROOT_PROC) {
            MkDir(logDir, LogInfo);
            if (LogInfo->stopRun) {
                goto closeFile;
            }
        }

#if defined(SWMPI)
        // Make sure the directory is created before we attempt
        // to write to it
        SW_MPI_Barrier(MPI_COMM_WORLD);
#endif

        LogInfo->logfp =
            create_logfile(rank, SW_PathInputs->txtInFiles[eLog], LogInfo);
    }
#else
    (void) rank;
    (void) logDir;
#endif

closeFile: { CloseFile(&f, LogInfo); }
}

void SW_F_deepCopy(
    SW_PATH_INPUTS *source, SW_PATH_INPUTS *dest, LOG_INFO *LogInfo
) {
    unsigned int file;

    memcpy(dest, source, sizeof(*dest));

    SW_F_init_ptrs(dest);

    for (file = 0; file < SW_NFILES; file++) {
        dest->txtInFiles[file] = Str_Dup(source->txtInFiles[file], LogInfo);

        if (LogInfo->stopRun) {
            return; // Exit prematurely due to error
        }
    }

#if defined(SWNETCDF)
    int k;
    int varNum;
    unsigned int numFiles = source->ncNumWeatherInFiles;

    ForEachNCInKey(k) {
        if (!isnull(source->ncInFiles[k])) {
            SW_NCIN_alloc_file_information(
                numVarsInKey[k],
                k,
                &dest->ncInFiles[k],
                &dest->ncWeatherInFiles,
                LogInfo
            );

            if (LogInfo->stopRun) {
                return; /* Exit function prematurely due to error */
            }

            for (varNum = 0; varNum < numVarsInKey[k]; varNum++) {
                if (!isnull(source->ncInFiles[k][varNum])) {
                    dest->ncInFiles[k][varNum] =
                        Str_Dup(source->ncInFiles[k][varNum], LogInfo);
                    if (LogInfo->stopRun) {
                        return; /* Exit function prematurely due to error */
                    }
                }
            }
        }
    }

    dest->ncNumWeatherInFiles = source->ncNumWeatherInFiles;

    if (!isnull(source->ncWeatherInFiles)) {
        for (varNum = 0; varNum < numVarsInKey[eSW_InWeather]; varNum++) {

            if (!isnull(source->ncWeatherInFiles[varNum])) {
                SW_NCIN_alloc_weath_input_info(
                    &dest->ncWeatherInFiles,
                    &dest->ncWeatherInStartEndYrs,
                    numFiles,
                    varNum,
                    LogInfo
                );
                if (LogInfo->stopRun) {
                    return; /* Exit function prematurely due to error */
                }

                for (file = 0; file < numFiles; file++) {
                    if (!isnull(source->ncWeatherInFiles[varNum][file])) {
                        dest->ncWeatherInFiles[varNum][file] = Str_Dup(
                            source->ncWeatherInFiles[varNum][file], LogInfo
                        );
                        if (LogInfo->stopRun) {
                            return; /* Exit prematurely due to error */
                        }
                    }
                }
            }
        }

        if (!isnull(source->ncWeatherInStartEndYrs)) {
            for (file = 0; file < numFiles; file++) {
                dest->ncWeatherInStartEndYrs[file][0] =
                    source->ncWeatherInStartEndYrs[file][0];
                dest->ncWeatherInStartEndYrs[file][1] =
                    source->ncWeatherInStartEndYrs[file][1];
            }
        }
    }
#endif
}

/**
@brief Initialize all input files to NULL (`txtInFiles`)

@param[in,out] SW_PathInputs Struct of type SW_PATH_INPUTS which
holds basic information about input files and values
*/
void SW_F_init_ptrs(SW_PATH_INPUTS *SW_PathInputs) {
    int file;

    SW_PathInputs->SW_ProjDir[0] = '\0';

    // Initialize `InFile` pointers to NULL
    for (file = 0; file < SW_NFILES; file++) {
        SW_PathInputs->txtInFiles[file] = NULL;
    }

#if defined(SWNETCDF)
    int k;

    ForEachNCInKey(k) {
        SW_PathInputs->ncInFiles[k] = NULL;
        SW_PathInputs->inVarIDs[k] = NULL;
        SW_PathInputs->inVarTypes[k] = NULL;
        SW_PathInputs->scaleAndAddFactVals[k] = NULL;
        SW_PathInputs->missValFlags[k] = NULL;
        SW_PathInputs->doubleMissVals[k] = NULL;
        SW_PathInputs->openInFileIDs[k] = NULL;
    }

    SW_PathInputs->ncWeatherInFiles = NULL;
    SW_PathInputs->ncWeatherInStartEndYrs = NULL;
    SW_PathInputs->ncWeatherStartEndIndices = NULL;
    SW_PathInputs->numSoilVarLyrs = NULL;
    SW_PathInputs->numDaysInYear = NULL;
#endif
}

/**
@brief Constructor for SW_PATH_INPUTS (except the `first file`)

File names of input files, e.g., those provided by the `first file`, are now
interpreted as being relative to the execution path, i.e.,
the directory provided via the `-d` option.
Compared to previous versions, this function no longer sets `SW_ProjDir` to
the directory part of the file name of the `first file`.

@param[in,out] SW_PathInputs Struct of type SW_PATH_INPUTS which
holds basic information about input files and values
*/
void SW_F_construct(SW_PATH_INPUTS *SW_PathInputs) {
#if defined(SWNETCDF)
    int domVar;

    for (domVar = 0; domVar < SW_NVARDOM; domVar++) {
        SW_PathInputs->ncDomFileIDs[domVar] = -1;
    }

    SW_PathInputs->ncNumWeatherInFiles = 0;
#else
    (void) SW_PathInputs;
#endif
}

/**
@brief Deconstructor for the struct SW_PATH_INPUTS.

@param[in,out] SW_PathInputs Struct of type SW_PATH_INPUTS which
holds basic information about output files and values
*/
void SW_F_deconstruct(SW_PATH_INPUTS *SW_PathInputs) {
    IntUS i;

    for (i = 0; i < SW_NFILES; i++) {
        if (!isnull(SW_PathInputs->txtInFiles[i])) {
            free(SW_PathInputs->txtInFiles[i]);
            SW_PathInputs->txtInFiles[i] = NULL;
        }
    }

#if defined(SWNETCDF)

    unsigned int numFiles = SW_PathInputs->ncNumWeatherInFiles;
    unsigned int file;
    int k;
    int varNum;

    SW_NCIN_close_files(SW_PathInputs);

    ForEachNCInKey(k) {
        if (!isnull(SW_PathInputs->ncInFiles[k])) {
            for (varNum = 0; varNum < numVarsInKey[k]; varNum++) {
                if (!isnull(SW_PathInputs->ncInFiles[k][varNum])) {
                    free(SW_PathInputs->ncInFiles[k][varNum]);
                    SW_PathInputs->ncInFiles[k][varNum] = NULL;
                }
            }

            free((void *) SW_PathInputs->ncInFiles[k]);
            SW_PathInputs->ncInFiles[k] = NULL;
        }

        if (!isnull(SW_PathInputs->inVarTypes[k])) {
            free((void *) SW_PathInputs->inVarTypes[k]);
            SW_PathInputs->inVarTypes[k] = NULL;
        }

        if (!isnull(SW_PathInputs->inVarIDs[k])) {
            free((void *) SW_PathInputs->inVarIDs[k]);
            SW_PathInputs->inVarIDs[k] = NULL;
        }

        if (!isnull(SW_PathInputs->scaleAndAddFactVals[k])) {
            for (varNum = 0; varNum < numVarsInKey[k]; varNum++) {
                if (!isnull(SW_PathInputs->scaleAndAddFactVals[k][varNum])) {
                    free((void *) SW_PathInputs->scaleAndAddFactVals[k][varNum]
                    );
                    SW_PathInputs->scaleAndAddFactVals[k][varNum] = NULL;
                }
            }

            free((void *) SW_PathInputs->scaleAndAddFactVals[k]);
            SW_PathInputs->scaleAndAddFactVals[k] = NULL;
        }

        if (!isnull(SW_PathInputs->missValFlags[k])) {
            for (varNum = 0; varNum < numVarsInKey[k]; varNum++) {
                if (!isnull(SW_PathInputs->missValFlags[k][varNum])) {
                    free((void *) SW_PathInputs->missValFlags[k][varNum]);
                    SW_PathInputs->missValFlags[k][varNum] = NULL;
                }
            }

            free((void *) SW_PathInputs->missValFlags[k]);
            SW_PathInputs->missValFlags[k] = NULL;
        }

        if (!isnull(SW_PathInputs->doubleMissVals[k])) {
            for (varNum = 0; varNum < numVarsInKey[k]; varNum++) {
                if (!isnull(SW_PathInputs->doubleMissVals[k][varNum])) {
                    free((void *) SW_PathInputs->doubleMissVals[k][varNum]);
                    SW_PathInputs->doubleMissVals[k][varNum] = NULL;
                }
            }

            free((void *) SW_PathInputs->doubleMissVals[k]);
            SW_PathInputs->doubleMissVals[k] = NULL;
        }

        if (!isnull(SW_PathInputs->openInFileIDs[k])) {
            for (varNum = 0; varNum < numVarsInKey[k]; varNum++) {
                if (!isnull(SW_PathInputs->openInFileIDs[k][varNum])) {
                    free((void *) SW_PathInputs->openInFileIDs[k][varNum]);
                    SW_PathInputs->openInFileIDs[k][varNum] = NULL;
                }
            }

            free((void *) SW_PathInputs->openInFileIDs[k]);
            SW_PathInputs->openInFileIDs[k] = NULL;
        }
    }

    if (!isnull(SW_PathInputs->ncWeatherStartEndIndices)) {
        for (file = 0; file < numFiles; file++) {
            if (!isnull(SW_PathInputs->ncWeatherStartEndIndices[file])) {
                free((void *) SW_PathInputs->ncWeatherStartEndIndices[file]);
                SW_PathInputs->ncWeatherStartEndIndices[file] = NULL;
            }
        }

        free((void *) SW_PathInputs->ncWeatherStartEndIndices);
        SW_PathInputs->ncWeatherStartEndIndices = NULL;
    }

    if (!isnull(SW_PathInputs->ncWeatherInFiles)) {
        for (varNum = 0; varNum < numVarsInKey[eSW_InWeather]; varNum++) {
            if (!isnull(SW_PathInputs->ncWeatherInFiles[varNum])) {
                for (file = 0; file < numFiles; file++) {
                    if (!isnull(SW_PathInputs->ncWeatherInFiles[varNum][file]
                        )) {
                        free((void *)
                                 SW_PathInputs->ncWeatherInFiles[varNum][file]);
                        SW_PathInputs->ncWeatherInFiles[varNum][file] = NULL;
                    }
                }

                free((void *) SW_PathInputs->ncWeatherInFiles[varNum]);
                SW_PathInputs->ncWeatherInFiles[varNum] = NULL;
            }
        }

        free((void *) SW_PathInputs->ncWeatherInFiles);
        SW_PathInputs->ncWeatherInFiles = NULL;
    }

    if (!isnull(SW_PathInputs->ncWeatherInStartEndYrs)) {
        for (file = 0; file < numFiles; file++) {
            if (!isnull(SW_PathInputs->ncWeatherInStartEndYrs[file])) {
                free((void *) SW_PathInputs->ncWeatherInStartEndYrs[file]);
                SW_PathInputs->ncWeatherInStartEndYrs[file] = NULL;
            }
        }

        free((void *) SW_PathInputs->ncWeatherInStartEndYrs);
        SW_PathInputs->ncWeatherInStartEndYrs = NULL;
    }

    if (!isnull(SW_PathInputs->numSoilVarLyrs)) {
        free((void *) SW_PathInputs->numSoilVarLyrs);
        SW_PathInputs->numSoilVarLyrs = NULL;
    }

    if (!isnull(SW_PathInputs->numDaysInYear)) {
        free((void *) SW_PathInputs->numDaysInYear);
        SW_PathInputs->numDaysInYear = NULL;
    }
#endif
}

/**
@brief Check if logging should throw a fatal error due to too many errors
thrown from simulation runs

@param[in] SW_Domain Struct of type SW_DOMAIN holding constant
temporal/spatial information for a set of simulation runs
@param[out] main_LogInfo Main log information from the domain-level
*/
void SW_F_check_fatal_log(SW_DOMAIN *SW_Domain, LOG_INFO *main_LogInfo) {
#if defined(SWNETCDF)
    size_t totFailedSites = main_LogInfo->numDomainErrors;

#if defined(SWMPI)
    SW_MPI_Allreduce(
        &main_LogInfo->numDomainErrors,
        &totFailedSites,
        1,
        SW_MPI_SIZE_T,
        MPI_SUM,
        MPI_COMM_WORLD
    );
#endif

    if (totFailedSites >= SW_Domain->nErrBeforeFail) {
        LogError(
            main_LogInfo,
            LOGERROR,
            "Limit for allowed simulation errors reached "
            "(%zu sites failed out of %zu allowed).",
            totFailedSites,
            SW_Domain->nErrBeforeFail
        );

        if (SW_Domain->rank == ROOT_PROC) {
            SW_MSG_ROOT(
                "Simulation ending early due to reaching the error "
                "user-provided limit.",
                SW_Domain->rank
            );
        }
    }
#else
    (void) SW_Domain;
    (void) main_LogInfo;
#endif
}

/**
@brief Go through all simulation logs and report them as needed

@param[in] simLogs A list of simulation logs (LOG_INFO) to be reported
@param[in] nSims Number of simulations that have been run
*/
void SW_F_report_logs(LOG_INFO *simLogs, size_t nSims) {
    size_t site;

    for (site = 0; site < nSims; site++) {
        if (simLogs[site].stopRun || simLogs[site].numWarnings > 0) {
            sw_write_warnings("", &simLogs[site]);
        }
    }
}

/**
@brief Increment the total number of warnings/errors in the main
instance of LOG_INFO

@param[in] simLog Log that has been gone through a simulation run
@param[out] runStatus Returns PRGRSS_FAIL (failed) if the respective
log information pertains to a failed site, otherwise, this value will
not be modified
@param[out] main_LogInfo Main log information from the domain-level
*/
void SW_F_handle_log_counts(
    LOG_INFO *simLog,
    signed char *runStatus, // NOLINT(readability-non-const-parameter)
    LOG_INFO *main_LogInfo
) {
    IntU numNewWarns = simLog->numWarnings - simLog->prevNumWarns;

    /* Report errors and warnings for suid */
    if (simLog->numWarnings > 0) {
        if (!simLog->loggedWarn) {
            // Counter of simulation units with warnings
            main_LogInfo->numDomainWarnings++;

            simLog->loggedWarn = swTRUE;
        }

        main_LogInfo->numSimWarnings += numNewWarns;
        simLog->prevNumWarns = simLog->numWarnings;
    }

    if (simLog->stopRun && !simLog->loggedError) {
        // Counter of simulation units with error
        main_LogInfo->numDomainErrors++;
#if defined(SWNETCDF)
        *runStatus = PRGRSS_FAIL;
#endif
        simLog->loggedError = swTRUE;
    }

#if !defined(SWNETCDF)
    (void) runStatus;
#endif
}

/**
@brief Wrapper function to check all site log information, track it
and error if necessary

@param[in] fatalError A flag specifying if a domain-level error occurred
so we don't attempt to overwrite it
@param[in,out] SW_Domain Struct of type SW_DOMAIN holding constant
temporal/spatial information for a set of simulation runs
@param[in] siteLogs A list of LOG_INFO of size [n active sites] that will
be returned with any site-specific errors/warnings
@param[out] main_LogInfo The main LOG_INFO instance for the program
*/
void SW_F_check_site_logs(
    Bool fatalError,
    SW_DOMAIN *SW_Domain,
    LOG_INFO *siteLogs,
    LOG_INFO *main_LogInfo
) {
    const size_t nSites = SW_Domain->nActiveSuidsProc;

    size_t site;
    size_t siteIdx = 0;

    for (site = 0; site < nSites; site++) {
#if defined(SWNETCDF)
        siteIdx = SW_Domain->actSiteIdx[eSW_InDomain][site];
#endif

        SW_F_handle_log_counts(
            &siteLogs[site],
            &SW_Domain->netCDFInput.progVals[siteIdx],
            main_LogInfo
        );
    }

    if (!fatalError) {
        SW_F_check_fatal_log(SW_Domain, main_LogInfo);
    }
}
