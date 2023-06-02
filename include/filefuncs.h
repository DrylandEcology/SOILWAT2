/* filefuncs.h -- contains definitions related to */
/* some generic file management functions

 * REQUIRES: generic.h
 */

/* Chris Bennett @ LTER-CSU 6/15/2000            */

#ifndef FILEFUNCS_H
#define FILEFUNCS_H

#include "include/generic.h"
#include "include/SW_Defines.h"

#ifdef __cplusplus
extern "C" {
#endif



/* =================================================== */
/*            Externed Global Variables                */
/* --------------------------------------------------- */
extern char inbuf[MAX_FILENAMESIZE]; /* defined in SW_Main_lib.c */
extern char _firstfile[MAX_FILENAMESIZE]; /* defined in SW_Main_lib.c */


/* =================================================== */
/*             Global Function Declarations            */
/* --------------------------------------------------- */
FILE * OpenFile(const char *, const char *);
void CloseFile(FILE **);
Bool GetALine(FILE *f, char buf[]);
char *DirName(const char *p);
const char *BaseName(const char *p);
Bool FileExists(const char *f);
Bool DirExists(const char *d);
Bool ChDir(const char *d);
Bool MkDir(const char *d);
Bool RemoveFiles(const char *fspec, LOG_INFO* LogInfo);
void sw_error(int errorcode, const char *format, ...);
void LogError(LOG_INFO* LogInfo, const int mode, const char *fmt, ...);


#ifdef __cplusplus
}
#endif

#endif
