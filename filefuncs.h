/* filefuncs.h -- contains definitions related to */
/* some generic file management functions

 * REQUIRES: generic.h
 */

/* Chris Bennett @ LTER-CSU 6/15/2000            */

#ifndef FILEFUNCS_H
#define FILEFUNCS_H

#include "generic.h"


/***************************************************
 * Function definitions
 ***************************************************/
FILE * OpenFile(const char *, const char *);
void CloseFile(FILE **);
Bool GetALine(FILE *f, char buf[]);
char *DirName(const char *p);
const char *BaseName(const char *p);
Bool FileExists(const char *f);
Bool DirExists(const char *d);
Bool ChDir(const char *d);
Bool MkDir(const char *d);
Bool RemoveFiles(const char *fspec);
void sw_error(int errorcode, const char *format, ...);
void LogError(FILE *fp, const int mode, const char *fmt, ...);

void stat_Output_Daily_CSV_Summary(int iteration);
void stat_Output_Weekly_CSV_Summary(int iteration);
void stat_Output_Monthly_CSV_Summary(int iteration);
void stat_Output_Yearly_CSV_Summary(int iteration);


extern char inbuf[]; /* declare in main, use anywhere */

#endif
