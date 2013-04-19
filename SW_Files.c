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
#include "generic.h"
#include "filefuncs.h"
#include "myMemory.h"
#include "SW_Defines.h"
#include "SW_Files.h"

/* =================================================== */
/*                  Global Variables                   */
/* --------------------------------------------------- */


/* =================================================== */
/*                Module-Level Variables               */
/* --------------------------------------------------- */
static char *MyFileName;
static char *InFiles[SW_NFILES];
static char _ProjDir[FILENAME_MAX];
static char weather_prefix[FILENAME_MAX];
static char output_prefix[FILENAME_MAX];

/* =================================================== */
/* =================================================== */
/*             Private Function Definitions            */
/* --------------------------------------------------- */
static void init(const char *s) {
/* --------------------------------------------------- */
/* sets the name of the first input file. If called
 * with s==NULL the name is set to "files.in".
 *
 * 1/24/02 - replaced [re]alloc with StrDup()
 */
  char fname[MAX_FILENAMESIZE] = {'\0'};


  if (NULL == InFiles[eFirst])
    strcpy(fname,(s ? s :"files.in"));
  else if (s && strcmp(s, InFiles[eFirst]))
    strcpy(fname, s);

  if (*fname) {
    if (!isnull(InFiles[eFirst])) Mem_Free(InFiles[eFirst]);
    InFiles[eFirst] = Str_Dup(fname);
  }

}


/* =================================================== */
/* =================================================== */
/*             Public Function Definitions             */
/* --------------------------------------------------- */


void SW_F_read( const char *s) {
/* =================================================== */
/* enter with the name of the first file to read for
 * the filenames, or NULL.  If null, then read files.in
 * or whichever filename was set previously. see init().
 *
 * 1/24/02 - replaced [re]alloc with StrDup()
 *         - added facility for log-to-file. logfp depends
 *             on having executed SW_F_read().
 */

   FILE *f;
   int lineno = 0, fileno = 0;
   char buf[FILENAME_MAX];

   if (!isnull(s)) init(s);  /* init should be run by SW_F_Construct() */

   MyFileName = SW_F_name(eFirst);
   f = OpenFile(MyFileName, "r");

	while( GetALine(f, inbuf) ) {

		switch(lineno){
			case 5: strcpy(weather_prefix, inbuf); break;
			case 12: strcpy(output_prefix, inbuf); break;
		
			default:
				if (++fileno == SW_NFILES) break;
				
				if (!isnull(InFiles[fileno])) Mem_Free(InFiles[fileno]);
				strcpy(buf, _ProjDir);
				strcat(buf, inbuf);
				InFiles[fileno] = Str_Dup(buf);
		}
		
		lineno++;
	}

   if (fileno < eEndFile-1) {
     LogError(stdout, LOGFATAL, "Too few files (%d) in %s", fileno, MyFileName);
   }


   CloseFile(&f);

#ifndef STEPWAT
  if (0==strcmp(InFiles[eLog], "stdout") ) {
    logfp = stdout;
  } else if (0==strcmp(InFiles[eLog], "stderr") ) {
    logfp = stderr;
  } else {
    logfp = OpenFile(SW_F_name(eLog), "w");
  }
#endif


}


char *SW_F_name( SW_FileIndex i) {
/* =================================================== */
    return InFiles[i];

}


void SW_F_construct(const char *firstfile) {
/* =================================================== */
/* 10-May-02 (cwb) enhancement allows model to be run
 *    in one directory while getting its input from another.
 *    This was done mostly in support of STEPWAT but
 *    it could be useful in a standalone run.
 */
  char *c, *p;

  init(firstfile);

  if ((c=DirName(firstfile)) ) {
    strcpy(_ProjDir, c);
    c = (char *) firstfile;
    p = c + strlen(_ProjDir);
    while(*p) *(c++) = *(p++);
    *c = '\0';
  } else
    _ProjDir[0] = '\0';


}

void SW_WeatherPrefix(char prefix[]){
	strcpy(prefix, weather_prefix);
}

void SW_OutputPrefix(char prefix[]){
	
	if(strcmp(output_prefix, "/") == 0)
		prefix[0] = '\0';
	else
		strcpy(prefix, output_prefix);
}



#ifdef DEBUG_MEM
#include "myMemory.h"
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
