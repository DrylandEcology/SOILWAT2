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
#include <stdio.h>                  // for FILENAME_MAX, NULL, FILE, stderr
#include <stdlib.h>                 // for free
#include <string.h>                 // for memccpy, strcmp, strlen, memcpy

#if defined(SWNETCDF)
#include "include/SW_netCDF_General.h"
#include "include/SW_netCDF_Input.h"
#endif

/* =================================================== */
/*             Global Function Definitions             */
/* --------------------------------------------------- */

/**
@brief Removes old output and/or create the output directories if needed.

@param *s Name of the first file to read for filenames, or NULL. If NULL, then
    read from DFLT_FIRSTFILE or whichever filename was set previously.
@param[out] LogInfo Holds information on warnings and errors

@sideeffect *s Updated name of the first file to read for filenames, or NULL.
If NULL, then read from DFLT_FIRSTFILE or whichever filename was set previously.
*/
void SW_CSV_F_INIT(const char *s, LOG_INFO *LogInfo) {
    /* AKT 08/28/2016
     *  remove old output and/or create the output directories if needed */
    /* borrow inbuf for filenames */

    char inbuf[MAX_FILENAMESIZE];
    char dirString[FILENAME_MAX];

    DirName(s, dirString);

    if (DirExists(dirString)) {
        (void) sw_memccpy(inbuf, (char *) s, '\0', sizeof inbuf);
        if (!RemoveFiles(inbuf, LogInfo)) {
            LogError(
                LogInfo, LOGWARN, "Can't remove old csv output file: %s\n", s
            );
        }
    } else {
        MkDir(dirString, LogInfo);
    }
}

/**
@brief Read `first` input file `eFirst` that contains names of the remaining
input files.

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
void SW_F_read(SW_PATH_INPUTS *SW_PathInputs, LOG_INFO *LogInfo) {
#ifdef SWDEBUG
    int debug = 0;
#endif

    FILE *f;
    int lineno = 0;
    int fileno = 0;
    int resSNP;
    char buf[FILENAME_MAX];
    char inbuf[MAX_FILENAMESIZE];

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
        case 10:
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
        case 18:
            resSNP = snprintf(
                SW_PathInputs->outputPrefix,
                sizeof SW_PathInputs->outputPrefix,
                "%s",
                inbuf
            );
            if (resSNP < 0 ||
                (unsigned) resSNP >= (sizeof SW_PathInputs->outputPrefix)) {
                LogError(
                    LogInfo,
                    LOGERROR,
                    "output path name is too long: '%s'.",
                    inbuf
                );
                goto closeFile;
            }
            break;
        case 20:
            SW_PathInputs->txtInFiles[eOutputDaily] = Str_Dup(inbuf, LogInfo);
            if (LogInfo->stopRun) {
                goto closeFile;
            }
            ++fileno;
            SW_CSV_F_INIT(SW_PathInputs->txtInFiles[eOutputDaily], LogInfo);
            break;
        case 21:
            SW_PathInputs->txtInFiles[eOutputWeekly] = Str_Dup(inbuf, LogInfo);
            if (LogInfo->stopRun) {
                goto closeFile;
            }
            ++fileno;
            SW_CSV_F_INIT(SW_PathInputs->txtInFiles[eOutputWeekly], LogInfo);
            // printf("filename: %s \n",txtInFiles[eOutputWeekly]);
            break;
        case 22:
            SW_PathInputs->txtInFiles[eOutputMonthly] = Str_Dup(inbuf, LogInfo);
            if (LogInfo->stopRun) {
                goto closeFile;
            }
            ++fileno;
            SW_CSV_F_INIT(SW_PathInputs->txtInFiles[eOutputMonthly], LogInfo);
            // printf("filename: %s \n",txtInFiles[eOutputMonthly]);
            break;
        case 23:
            SW_PathInputs->txtInFiles[eOutputYearly] = Str_Dup(inbuf, LogInfo);
            if (LogInfo->stopRun) {
                goto closeFile;
            }
            ++fileno;
            SW_CSV_F_INIT(SW_PathInputs->txtInFiles[eOutputYearly], LogInfo);
            break;
        case 24:
            SW_PathInputs->txtInFiles[eOutputDaily_soil] =
                Str_Dup(inbuf, LogInfo);
            if (LogInfo->stopRun) {
                goto closeFile;
            }
            ++fileno;
            SW_CSV_F_INIT(
                SW_PathInputs->txtInFiles[eOutputDaily_soil], LogInfo
            );
            // printf("filename: %s \n",txtInFiles[eOutputDaily]);
            break;
        case 25:
            SW_PathInputs->txtInFiles[eOutputWeekly_soil] =
                Str_Dup(inbuf, LogInfo);
            if (LogInfo->stopRun) {
                goto closeFile;
            }
            ++fileno;
            SW_CSV_F_INIT(
                SW_PathInputs->txtInFiles[eOutputWeekly_soil], LogInfo
            );
            // printf("filename: %s \n",txtInFiles[eOutputWeekly]);
            break;
        case 26:
            SW_PathInputs->txtInFiles[eOutputMonthly_soil] =
                Str_Dup(inbuf, LogInfo);
            if (LogInfo->stopRun) {
                goto closeFile;
            }
            ++fileno;
            SW_CSV_F_INIT(
                SW_PathInputs->txtInFiles[eOutputMonthly_soil], LogInfo
            );
            // printf("filename: %s \n",txtInFiles[eOutputMonthly]);
            break;
        case 27:
            SW_PathInputs->txtInFiles[eOutputYearly_soil] =
                Str_Dup(inbuf, LogInfo);
            if (LogInfo->stopRun) {
                goto closeFile;
            }
            ++fileno;
            SW_CSV_F_INIT(
                SW_PathInputs->txtInFiles[eOutputYearly_soil], LogInfo
            );
            break;

        default:
            if (++fileno == SW_NFILES) {
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

        // Check if something went wrong in `SW_CSV_F_INIT()`
        if (LogInfo->stopRun) {
            goto closeFile;
        }

        lineno++;
    }

    if (fileno < eEndFile - 1) {
        LogError(
            LogInfo, LOGERROR, "Too few files (%d) in %s", fileno, MyFileName
        );
        goto closeFile;
    }

    if (!DirExists(SW_PathInputs->outputPrefix)) {
        MkDir(SW_PathInputs->outputPrefix, LogInfo);
        if (LogInfo->stopRun) {
            goto closeFile;
        }
    }

#ifdef SOILWAT
    if (0 == strcmp(SW_PathInputs->txtInFiles[eLog], "stdout")) {
        LogInfo->logfp = stdout;
    } else if (0 == strcmp(SW_PathInputs->txtInFiles[eLog], "stderr")) {
        LogInfo->logfp = stderr;
    } else {
        LogInfo->logfp =
            OpenFile(SW_PathInputs->txtInFiles[eLog], "w", LogInfo);
    }
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

    // Initialize `InFile` pointers to NULL
    for (file = 0; file < SW_NFILES; file++) {
        SW_PathInputs->txtInFiles[file] = NULL;
    }

#if defined(SWNETCDF)
    int k;

    ForEachNCInKey(k) { SW_PathInputs->ncInFiles[k] = NULL; }

    SW_PathInputs->ncWeatherInFiles = NULL;
    SW_PathInputs->ncWeatherInStartEndYrs = NULL;
    SW_PathInputs->ncWeatherStartEndIndices = NULL;
#endif
}

/**
@brief Determines string length of file being read in combined with SW_ProjDir.

@param[in,out] SW_PathInputs Struct of type SW_PATH_INPUTS which
holds basic information about input files and values
@param[out] LogInfo Holds information on warnings and errors
*/
void SW_F_construct(SW_PATH_INPUTS *SW_PathInputs, LOG_INFO *LogInfo) {
    /* =================================================== */
    /* 10-May-02 (cwb) enhancement allows model to be run
     *    in one directory while getting its input from another.
     *    This was done mostly in support of STEPWAT but
     *    it could be useful in a standalone run.
     */

    const char *firstfile = SW_PathInputs->txtInFiles[eFirst];
    char *c;
    char *p;
    char dirString[FILENAME_MAX];
    char *localfirstfile = Str_Dup(firstfile, LogInfo);
    if (LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

    DirName(localfirstfile, dirString);
    c = dirString;

    if (c) {
        (void) sw_memccpy(SW_PathInputs->SW_ProjDir, c, '\0', sizeof dirString);
        c = localfirstfile;
        p = c + strlen(SW_PathInputs->SW_ProjDir);
        while (*p) {
            *(c++) = *(p++);
        }
        *c = '\0';
    } else {
        SW_PathInputs->SW_ProjDir[0] = '\0';
    }

    free(localfirstfile);

#if defined(SWNETCDF)
    int inKey;

    ForEachNCInKey(inKey) {
        SW_PathInputs->inVarTypes[inKey] = NULL;
        SW_PathInputs->inVarIDs[inKey] = NULL;
        SW_PathInputs->hasScaleAndAddFact[inKey] = NULL;
        SW_PathInputs->scaleAndAddFactVals[inKey] = NULL;
        SW_PathInputs->missValFlags[inKey] = NULL;
        SW_PathInputs->doubleMissVals[inKey] = NULL;
    }

    SW_PathInputs->ncDomFileIDs[vNCdom] = -1;
    SW_PathInputs->ncDomFileIDs[vNCprog] = -1;
    SW_PathInputs->numSoilVarLyrs = NULL;

    SW_PathInputs->ncNumWeatherInFiles = 0;
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

        if (!isnull(SW_PathInputs->hasScaleAndAddFact[k])) {
            free((void *) SW_PathInputs->hasScaleAndAddFact[k]);
            SW_PathInputs->hasScaleAndAddFact[k] = NULL;
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

            free(SW_PathInputs->missValFlags[k]);
            SW_PathInputs->missValFlags[k] = NULL;
        }

        if (!isnull(SW_PathInputs->doubleMissVals[k])) {
            for (varNum = 0; varNum < numVarsInKey[k]; varNum++) {
                if (!isnull(SW_PathInputs->doubleMissVals[k][varNum])) {
                    free((void *) SW_PathInputs->doubleMissVals[k][varNum]);
                    SW_PathInputs->doubleMissVals[k][varNum] = NULL;
                }
            }

            free(SW_PathInputs->doubleMissVals[k]);
            SW_PathInputs->doubleMissVals[k] = NULL;
        }
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

    SW_NCIN_close_files(SW_PathInputs->ncDomFileIDs);
#endif
}
