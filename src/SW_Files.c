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
/*                  Global Variables                   */
/* --------------------------------------------------- */

char *InFiles[SW_NFILES];
char _ProjDir[FILENAME_MAX];
char weather_prefix[FILENAME_MAX];
char output_prefix[FILENAME_MAX];

/* =================================================== */
/*             Local Function Definitions              */
/* --------------------------------------------------- */

static void init(const char *s, LOG_INFO* LogInfo) {
	/* --------------------------------------------------- */
	/* sets the name of the first input file. If called
	 * with s==NULL the name is set to "files.in".
	 *
	 * 1/24/02 - replaced [re]alloc with StrDup()
	 */
	char fname[MAX_FILENAMESIZE] = { '\0' };

	if (NULL == InFiles[eFirst])
		strcpy(fname, (s ? s : DFLT_FIRSTFILE));
	else if (s && strcmp(s, InFiles[eFirst]))
		strcpy(fname, s);

	if (*fname) {
		if (!isnull(InFiles[eFirst]))
			Mem_Free(InFiles[eFirst]);
		InFiles[eFirst] = Str_Dup(fname, LogInfo);
	}

}


/* =================================================== */
/*             Global Function Definitions             */
/* --------------------------------------------------- */

/**
@brief Removes old output and/or create the output directories if needed.

@param *s Name of the first file to read for filenames, or NULL. If NULL, then read
	from DFLT_FIRSTFILE or whichever filename was set previously.
@param[in] LogInfo Holds information dealing with logfile output

@sideeffect *s Updated name of the first file to read for filenames, or NULL. If NULL, then read
	from DFLT_FIRSTFILE or whichever filename was set previously.
*/
void SW_CSV_F_INIT(const char *s, LOG_INFO* LogInfo)
{
	/* AKT 08/28/2016
	 *  remove old output and/or create the output directories if needed */
	/* borrow inbuf for filenames */

	char inbuf[MAX_FILENAMESIZE];

	if (DirExists(DirName(s)))
	{
		strcpy(inbuf, s);
		if (!RemoveFiles(inbuf, LogInfo))
		{
			LogError(LogInfo, LOGWARN, "Can't remove old csv output file: %s\n", s);
			printf("Can't remove old csv output file: %s\n", s);
		}
	}
	else if (!MkDir(DirName(s)))
	{
		LogError(LogInfo, LOGFATAL, "Can't make output path for csv file: %s\n", DirName(s));
		printf("Can't make output path for csv file: %s\n", DirName(s));
	}
}


/** Read `first` input file `eFirst` that contains names of the remaining input files.

	@param[in,out] LogInfo Holds information dealing with logfile output
	@param[in,out] PathInfo truct holding all information about the programs path/files

    @note If input file `eFirst` changes, particularly if the locations of the
      `weather_prefix` and/or `output_prefix` change; then update the hard-coded line
      numbers.

    @sideeffect Update values of variables within PATH_INFO:
      - `weather_prefix`
      - `output_prefix`
      - `InFiles`
      - `logfp` for SOILWAT2-standalone
  */
void SW_F_read(LOG_INFO* LogInfo, PATH_INFO* PathInfo) {
	FILE *f;
	int lineno = 0, fileno = 0;
	char buf[FILENAME_MAX], inbuf[MAX_FILENAMESIZE];
  #ifdef SWDEBUG
  int debug = 0;
  #endif


	if (!isnull(s))
		init(s, LogInfo); /* init should be run by SW_F_Construct() */

	char *MyFileName = SW_F_name(eFirst, PathInfo->InFiles);
	f = OpenFile(MyFileName, "r", LogInfo);

	while (GetALine(f, inbuf)) {

    #ifdef SWDEBUG
    if (debug) swprintf("'SW_F_read': line = %d/%d: %s\n", lineno, eEndFile, inbuf);
    #endif

		switch (lineno) {
		case 6:
			strcpy(PathInfo->weather_prefix, inbuf);
			break;
		case 14:
			strcpy(PathInfo->output_prefix, inbuf);
			break;
		case 16:
			PathInfo->InFiles[eOutputDaily] = Str_Dup(inbuf, LogInfo);
			++fileno;
			SW_CSV_F_INIT(PathInfo->InFiles[eOutputDaily], LogInfo);
			break;
		case 17:
			PathInfo->InFiles[eOutputWeekly] = Str_Dup(inbuf, LogInfo);
			++fileno;
			SW_CSV_F_INIT(PathInfo->InFiles[eOutputWeekly], LogInfo);
			//printf("filename: %s \n",InFiles[eOutputWeekly]);
			break;
		case 18:
			PathInfo->InFiles[eOutputMonthly] = Str_Dup(inbuf, LogInfo);
			++fileno;
			SW_CSV_F_INIT(PathInfo->InFiles[eOutputMonthly], LogInfo);
			//printf("filename: %s \n",InFiles[eOutputMonthly]);
			break;
		case 19:
			PathInfo->InFiles[eOutputYearly] = Str_Dup(inbuf, LogInfo);
			++fileno;
			SW_CSV_F_INIT(PathInfo->InFiles[eOutputYearly], LogInfo);
			break;
		case 20:
			PathInfo->InFiles[eOutputDaily_soil] = Str_Dup(inbuf, LogInfo);
			++fileno;
			SW_CSV_F_INIT(PathInfo->InFiles[eOutputDaily_soil], LogInfo);
			//printf("filename: %s \n",InFiles[eOutputDaily]);
			break;
		case 21:
			PathInfo->InFiles[eOutputWeekly_soil] = Str_Dup(inbuf, LogInfo);
			++fileno;
			SW_CSV_F_INIT(PathInfo->InFiles[eOutputWeekly_soil], LogInfo);
			//printf("filename: %s \n",InFiles[eOutputWeekly]);
			break;
		case 22:
			PathInfo->InFiles[eOutputMonthly_soil] = Str_Dup(inbuf, LogInfo);
			++fileno;
			SW_CSV_F_INIT(PathInfo->InFiles[eOutputMonthly_soil], LogInfo);
			//printf("filename: %s \n",InFiles[eOutputMonthly]);
			break;
		case 23:
			PathInfo->InFiles[eOutputYearly_soil] = Str_Dup(inbuf, LogInfo);
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
		}

		lineno++;
	}

	if (fileno < eEndFile - 1) {
		CloseFile(&f, LogInfo);
		LogError(LogInfo, LOGFATAL, "Too few files (%d) in %s", fileno, MyFileName);
	}

	CloseFile(&f, LogInfo);

#ifdef SOILWAT
	if (0 == strcmp(PathInfo->InFiles[eLog], "stdout")) {
		LogInfo->logfp = stdout;
	} else if (0 == strcmp(PathInfo->InFiles[eLog], "stderr")) {
		LogInfo->logfp = stderr;
	} else {
		LogInfo->logfp =
				OpenFile(SW_F_name(eLog, PathInfo->InFiles), "w", LogInfo);
	}
#endif

}

/**
@brief Adds FileIndex parameter i to array InFiles

@param i Index parameter.
@param[in] InFiles Array of program input files

@return InFiles Array of index parameters.
*/
char *SW_F_name(SW_FileIndex i, char *InFiles[]) {
	/* =================================================== */
	return InFiles[i];

}
/**
@brief Determines string length of file being read in combined with _ProjDir.

@param[in,out] InFiles Array of program input files
@param[out] *firstfile File to be read in.
@param[out] _ProjDir Project directory

@sideeffect
	- *firstfile File to be read in.
	- *c Counter parameter for the firstfile.
	- *p Parameter for length of project directory plus *c
*/
void SW_F_construct(char *InFiles[], const char *firstfile, char _ProjDir[]) {
	/* =================================================== */
	/* 10-May-02 (cwb) enhancement allows model to be run
	 *    in one directory while getting its input from another.
	 *    This was done mostly in support of STEPWAT but
	 *    it could be useful in a standalone run.
	 */
	char *c, *p;
	int file;

	init(firstfile, LogInfo);
	// Initialize `InFile` pointers to all NULL aside from index eFirst
	for(file = 1; file < SW_NFILES; file++) {
		InFiles[file] = NULL;
	}

	if ((c = DirName(firstfile))) {
		strcpy(_ProjDir, c);
		c = (char *) firstfile;
		p = c + strlen(_ProjDir);
		while (*p)
			*(c++) = *(p++);
		*c = '\0';
	} else
		_ProjDir[0] = '\0';

}

/**
@brief Deconstructor for each of the SW_NFILES.

@param[in,out] InFiles Array of program input files
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

/**
@brief Copies the prefix string to the weather_prefix.

@param prefix Array of chars.
@param[out] weather_prefix File name of weather data without extension.
*/
void SW_WeatherPrefix(char prefix[], char weather_prefix[]) {
	strcpy(prefix, weather_prefix);
}

#ifdef DEBUG_MEM
#include "include/myMemory.h"
/*======================================================*/
void SW_F_SetMemoryRefs( void) {
	/* when debugging memory problems, use the bookkeeping
	 code in myMemory.c
	 This routine sets the known memory refs in this module
	 so they can be  checked for leaks, etc.  Includes
	 malloc-ed memory in SOILWAT.  All refs will have been
	 cleared by a call to ClearMemoryRefs() before this, and
	 will be checked via CheckMemoryRefs() after this, most
	 likely in the main() function.
	 */
	SW_FileIndex i;

	for ( i=eFirst; i < eEndFile; i++)
	NoteMemoryRef(InFiles[i]);

}

#endif
