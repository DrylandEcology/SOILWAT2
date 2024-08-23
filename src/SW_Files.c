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
 can access local variable weather_prefix that is read in now in SW_F_read() new
 module-level variable static char weather_prefix[FILENAME_MAX]; read in in
 function SW_F_read() from file files.in line 6

 09/30/2011	(drs)	added function SW_OutputPrefix(): so that SW_Output can
 access local variable output_prefix that is read in now in SW_F_read() new
 module-level variable static char output_prefix[FILENAME_MAX]; read in in
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
`weather_prefix` and/or `output_prefix` change; then update the hard-coded
line numbers.

@sideeffect
Update values of variables within SW_PATH_INPUTS:
    - `weather_prefix`
    - `output_prefix`
    - `InFiles`
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

    char *MyFileName = SW_PathInputs->InFiles[eFirst];
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
                SW_PathInputs->weather_prefix,
                sizeof SW_PathInputs->weather_prefix,
                "%s",
                inbuf
            );
            if (resSNP < 0 ||
                (unsigned) resSNP >= (sizeof SW_PathInputs->weather_prefix)) {
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
                SW_PathInputs->output_prefix,
                sizeof SW_PathInputs->output_prefix,
                "%s",
                inbuf
            );
            if (resSNP < 0 ||
                (unsigned) resSNP >= (sizeof SW_PathInputs->output_prefix)) {
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
            SW_PathInputs->InFiles[eOutputDaily] = Str_Dup(inbuf, LogInfo);
            if (LogInfo->stopRun) {
                goto closeFile;
            }
            ++fileno;
            SW_CSV_F_INIT(SW_PathInputs->InFiles[eOutputDaily], LogInfo);
            break;
        case 21:
            SW_PathInputs->InFiles[eOutputWeekly] = Str_Dup(inbuf, LogInfo);
            if (LogInfo->stopRun) {
                goto closeFile;
            }
            ++fileno;
            SW_CSV_F_INIT(SW_PathInputs->InFiles[eOutputWeekly], LogInfo);
            // printf("filename: %s \n",InFiles[eOutputWeekly]);
            break;
        case 22:
            SW_PathInputs->InFiles[eOutputMonthly] = Str_Dup(inbuf, LogInfo);
            if (LogInfo->stopRun) {
                goto closeFile;
            }
            ++fileno;
            SW_CSV_F_INIT(SW_PathInputs->InFiles[eOutputMonthly], LogInfo);
            // printf("filename: %s \n",InFiles[eOutputMonthly]);
            break;
        case 23:
            SW_PathInputs->InFiles[eOutputYearly] = Str_Dup(inbuf, LogInfo);
            if (LogInfo->stopRun) {
                goto closeFile;
            }
            ++fileno;
            SW_CSV_F_INIT(SW_PathInputs->InFiles[eOutputYearly], LogInfo);
            break;
        case 24:
            SW_PathInputs->InFiles[eOutputDaily_soil] = Str_Dup(inbuf, LogInfo);
            if (LogInfo->stopRun) {
                goto closeFile;
            }
            ++fileno;
            SW_CSV_F_INIT(SW_PathInputs->InFiles[eOutputDaily_soil], LogInfo);
            // printf("filename: %s \n",InFiles[eOutputDaily]);
            break;
        case 25:
            SW_PathInputs->InFiles[eOutputWeekly_soil] =
                Str_Dup(inbuf, LogInfo);
            if (LogInfo->stopRun) {
                goto closeFile;
            }
            ++fileno;
            SW_CSV_F_INIT(SW_PathInputs->InFiles[eOutputWeekly_soil], LogInfo);
            // printf("filename: %s \n",InFiles[eOutputWeekly]);
            break;
        case 26:
            SW_PathInputs->InFiles[eOutputMonthly_soil] =
                Str_Dup(inbuf, LogInfo);
            if (LogInfo->stopRun) {
                goto closeFile;
            }
            ++fileno;
            SW_CSV_F_INIT(SW_PathInputs->InFiles[eOutputMonthly_soil], LogInfo);
            // printf("filename: %s \n",InFiles[eOutputMonthly]);
            break;
        case 27:
            SW_PathInputs->InFiles[eOutputYearly_soil] =
                Str_Dup(inbuf, LogInfo);
            if (LogInfo->stopRun) {
                goto closeFile;
            }
            ++fileno;
            SW_CSV_F_INIT(SW_PathInputs->InFiles[eOutputYearly_soil], LogInfo);
            break;

        default:
            if (++fileno == SW_NFILES) {
                break;
            }

            if (!isnull(SW_PathInputs->InFiles[fileno])) {
                free(SW_PathInputs->InFiles[fileno]);
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

            SW_PathInputs->InFiles[fileno] = Str_Dup(buf, LogInfo);
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

    if (!DirExists(SW_PathInputs->output_prefix)) {
        MkDir(SW_PathInputs->output_prefix, LogInfo);
        if (LogInfo->stopRun) {
            goto closeFile;
        }
    }

#ifdef SOILWAT
    if (0 == strcmp(SW_PathInputs->InFiles[eLog], "stdout")) {
        LogInfo->logfp = stdout;
    } else if (0 == strcmp(SW_PathInputs->InFiles[eLog], "stderr")) {
        LogInfo->logfp = stderr;
    } else {
        LogInfo->logfp = OpenFile(SW_PathInputs->InFiles[eLog], "w", LogInfo);
    }
#endif

closeFile: { CloseFile(&f, LogInfo); }
}

void SW_F_deepCopy(
    SW_PATH_INPUTS *source, SW_PATH_INPUTS *dest, LOG_INFO *LogInfo
) {
    int file;

    memcpy(dest, source, sizeof(*dest));

    SW_F_init_ptrs(dest->InFiles);

    for (file = 0; file < SW_NFILES; file++) {
        dest->InFiles[file] = Str_Dup(source->InFiles[file], LogInfo);

        if (LogInfo->stopRun) {
            return; // Exit prematurely due to error
        }
    }
}

/**
@brief Initialize all input files to NULL (`InFiles`)

@param[in,out] SW_PathInputs Struct of type SW_PATH_INPUTS which
holds basic information about input files and values
*/
void SW_F_init_ptrs(SW_PATH_INPUTS *SW_PathInputs) {
    int file;

    // Initialize `InFile` pointers to NULL
    for (file = 0; file < SW_NFILES; file++) {
        SW_PathInputs->InFiles[file] = NULL;
    }
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

    const char *firstfile = SW_PathInputs->InFiles[eFirst];
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
}

/**
@brief Deconstructor for the struct SW_PATH_INPUTS.

@param[in,out] SW_PathInputs Struct of type SW_PATH_INPUTS which
holds basic information about output files and values
*/
void SW_F_deconstruct(SW_PATH_INPUTS *SW_PathInputs) {
    IntUS i;

    for (i = 0; i < SW_NFILES; i++) {
        if (!isnull(SW_PathInputs->InFiles[i])) {
            free(SW_PathInputs->InFiles[i]);
            SW_PathInputs->InFiles[i] = NULL;
        }
    }
}
