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
 09/30/2011	(drs)	added function SW_WeatherPrefix(): so that SW_Weather can access local variable weather_prefix that is read in now in SW_F_read()
 new module-level variable static char weather_prefix[FILENAME_MAX]; read in in function SW_F_read() from file files.in line 6
 09/30/2011	(drs)	added function SW_OutputPrefix(): so that SW_Output can access local variable output_prefix that is read in now in SW_F_read()
 new module-level variable static char output_prefix[FILENAME_MAX]; read in in function SW_F_read() from file files.in line 12: / for same directory, or e.g., Output/
 */
/********************************************************/
/********************************************************/

/* =================================================== */
/* =================================================== */
/*                INCLUDES / DEFINES                   */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "include/filefuncs.h"
#include "include/myMemory.h"
#include "include/SW_Files.h"


/* =================================================== */
/*             Global Function Definitions             */
/* --------------------------------------------------- */

/**
@brief Removes old output and/or create the output directories if needed.

@param *s Name of the first file to read for filenames, or NULL. If NULL, then read
	from DFLT_FIRSTFILE or whichever filename was set previously.
@param[in,out] LogInfo Holds information dealing with logfile output

@sideeffect *s Updated name of the first file to read for filenames, or NULL. If NULL, then read
	from DFLT_FIRSTFILE or whichever filename was set previously.
*/
void SW_CSV_F_INIT(const char *s, LOG_INFO* LogInfo)
{
	/* AKT 08/28/2016
	 *  remove old output and/or create the output directories if needed */
	/* borrow inbuf for filenames */

	char inbuf[MAX_FILENAMESIZE], dirString[FILENAME_MAX];

	DirName(s, dirString);

	if (DirExists(dirString))
	{
		strcpy(inbuf, s);
		if (!RemoveFiles(inbuf, LogInfo))
		{
			LogError(LogInfo, LOGWARN, "Can't remove old csv output file: %s\n", s);
		}
	}
	else if (!MkDir(dirString, LogInfo))
	{
		LogError(LogInfo, LOGERROR, "Can't make output path for csv file: %s\n", dirString);
	}
}


/** Read `first` input file `eFirst` that contains names of the remaining input files.

	@param[in,out] PathInfo truct holding all information about the programs path/files
	@param[in,out] LogInfo Holds information dealing with logfile output

    @note If input file `eFirst` changes, particularly if the locations of the
      `weather_prefix` and/or `output_prefix` change; then update the hard-coded line
      numbers.

    @sideeffect Update values of variables within PATH_INFO:
      - `weather_prefix`
      - `output_prefix`
      - `InFiles`
      - `logfp` for SOILWAT2-standalone
  */
void SW_F_read(PATH_INFO* PathInfo, LOG_INFO* LogInfo) {
	FILE *f;
	int lineno = 0, fileno = 0;
	char buf[FILENAME_MAX], inbuf[MAX_FILENAMESIZE];
  #ifdef SWDEBUG
  int debug = 0;
  #endif

	char *MyFileName = PathInfo->InFiles[eFirst];
	f = OpenFile(MyFileName, "r", LogInfo);
    if(LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

	while (GetALine(f, inbuf)) {

    #ifdef SWDEBUG
    if (debug) swprintf("'SW_F_read': line = %d/%d: %s\n", lineno, eEndFile, inbuf);
    #endif

		switch (lineno) {
		case 7:
			strcpy(PathInfo->weather_prefix, inbuf);
			break;
		case 15:
			strcpy(PathInfo->output_prefix, inbuf);
			break;
		case 17:
			PathInfo->InFiles[eOutputDaily] = Str_Dup(inbuf, LogInfo);
            if(LogInfo->stopRun) {
                CloseFile(&f, LogInfo);
                return; // Exit function prematurely due to error
            }
			++fileno;
			SW_CSV_F_INIT(PathInfo->InFiles[eOutputDaily], LogInfo);
			break;
		case 18:
			PathInfo->InFiles[eOutputWeekly] = Str_Dup(inbuf, LogInfo);
            if(LogInfo->stopRun) {
                CloseFile(&f, LogInfo);
                return; // Exit function prematurely due to error
            }
			++fileno;
			SW_CSV_F_INIT(PathInfo->InFiles[eOutputWeekly], LogInfo);
			//printf("filename: %s \n",InFiles[eOutputWeekly]);
			break;
		case 19:
			PathInfo->InFiles[eOutputMonthly] = Str_Dup(inbuf, LogInfo);
            if(LogInfo->stopRun) {
                CloseFile(&f, LogInfo);
                return; // Exit function prematurely due to error
            }
			++fileno;
			SW_CSV_F_INIT(PathInfo->InFiles[eOutputMonthly], LogInfo);
			//printf("filename: %s \n",InFiles[eOutputMonthly]);
			break;
		case 20:
			PathInfo->InFiles[eOutputYearly] = Str_Dup(inbuf, LogInfo);
            if(LogInfo->stopRun) {
                CloseFile(&f, LogInfo);
                return; // Exit function prematurely due to error
            }
			++fileno;
			SW_CSV_F_INIT(PathInfo->InFiles[eOutputYearly], LogInfo);
			break;
		case 21:
			PathInfo->InFiles[eOutputDaily_soil] = Str_Dup(inbuf, LogInfo);
            if(LogInfo->stopRun) {
                CloseFile(&f, LogInfo);
                return; // Exit function prematurely due to error
            }
			++fileno;
			SW_CSV_F_INIT(PathInfo->InFiles[eOutputDaily_soil], LogInfo);
			//printf("filename: %s \n",InFiles[eOutputDaily]);
			break;
		case 22:
			PathInfo->InFiles[eOutputWeekly_soil] = Str_Dup(inbuf, LogInfo);
            if(LogInfo->stopRun) {
                CloseFile(&f, LogInfo);
                return; // Exit function prematurely due to error
            }
			++fileno;
			SW_CSV_F_INIT(PathInfo->InFiles[eOutputWeekly_soil], LogInfo);
			//printf("filename: %s \n",InFiles[eOutputWeekly]);
			break;
		case 23:
			PathInfo->InFiles[eOutputMonthly_soil] = Str_Dup(inbuf, LogInfo);
            if(LogInfo->stopRun) {
                CloseFile(&f, LogInfo);
                return; // Exit function prematurely due to error
            }
			++fileno;
			SW_CSV_F_INIT(PathInfo->InFiles[eOutputMonthly_soil], LogInfo);
			//printf("filename: %s \n",InFiles[eOutputMonthly]);
			break;
		case 24:
			PathInfo->InFiles[eOutputYearly_soil] = Str_Dup(inbuf, LogInfo);
            if(LogInfo->stopRun) {
                CloseFile(&f, LogInfo);
                return; // Exit function prematurely due to error
            }
			++fileno;
			SW_CSV_F_INIT(PathInfo->InFiles[eOutputYearly_soil], LogInfo);
			break;

		default:
			if (++fileno == SW_NFILES)
				break;

			if (!isnull(PathInfo->InFiles[fileno])) {
				Mem_Free(PathInfo->InFiles[fileno]);
			}

			strcpy(buf, PathInfo->_ProjDir);
			strcat(buf, inbuf);
			PathInfo->InFiles[fileno] = Str_Dup(buf, LogInfo);
            if(LogInfo->stopRun) {
                CloseFile(&f, LogInfo);
                return; // Exit function prematurely due to error
            }
		}

        // Check if something went wrong in `SW_CSV_F_INIT()`
        if(LogInfo->stopRun) {
            CloseFile(&f, LogInfo);
            return; // Exit function prematurely due to error
        }

		lineno++;
	}

    CloseFile(&f, LogInfo);

	if (fileno < eEndFile - 1) {
        LogError(LogInfo, LOGERROR, "Too few files (%d) in %s", fileno, MyFileName);
        return; // Exit function prematurely due to error
	}

#ifdef SOILWAT
	if (0 == strcmp(PathInfo->InFiles[eLog], "stdout")) {
		LogInfo->logfp = stdout;
	} else if (0 == strcmp(PathInfo->InFiles[eLog], "stderr")) {
		LogInfo->logfp = stderr;
	} else {
		LogInfo->logfp =
				OpenFile(PathInfo->InFiles[eLog], "w", LogInfo);
	}
#endif

}

/**
 * @brief Initialize all input files to NULL (`InFiles`)
 *
 * @brief[in,out] InFiles Array of program in/output files
*/
void SW_F_init_ptrs(char *InFiles[]) {
	int file;

	// Initialize `InFile` pointers to NULL
	for(file = 0; file < SW_NFILES; file++) {
		InFiles[file] = NULL;
	}
}

/**
@brief Determines string length of file being read in combined with _ProjDir.

@param[in] *firstfile File to be read in.
@param[out] _ProjDir Project directory
@param[in,out] LogInfo Holds information dealing with logfile output

@sideeffect
	- *firstfile File to be read in.
	- *c Counter parameter for the firstfile.
	- *p Parameter for length of project directory plus *c
*/
void SW_F_construct(const char *firstfile, char _ProjDir[],
					LOG_INFO *LogInfo) {
	/* =================================================== */
	/* 10-May-02 (cwb) enhancement allows model to be run
	 *    in one directory while getting its input from another.
	 *    This was done mostly in support of STEPWAT but
	 *    it could be useful in a standalone run.
	 */
	char *c, *p, dirString[FILENAME_MAX];
	char *local_firstfile = Str_Dup(firstfile, LogInfo);
    if(LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

	DirName(local_firstfile, dirString);

	if ((c = dirString)) {
		strcpy(_ProjDir, c);
		c = local_firstfile;
		p = c + strlen(_ProjDir);
		while (*p)
			*(c++) = *(p++);
		*c = '\0';
	} else
		_ProjDir[0] = '\0';

	free(local_firstfile);

}

/**
@brief Deconstructor for each of the SW_NFILES.

@param[in,out] InFiles Array of program in/output files
*/
void SW_F_deconstruct(char *InFiles[]) {
	IntUS i;

	for (i = 0; i < SW_NFILES; i++)
	{
		if (!isnull(InFiles[i])) {
			Mem_Free(InFiles[i]);
			InFiles[i] = NULL;
		}
	}
}
