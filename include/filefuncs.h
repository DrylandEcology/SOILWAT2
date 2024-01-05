/* filefuncs.h -- contains definitions related to */
/* some generic file management functions

 * REQUIRES: generic.h
 */

/* Chris Bennett @ LTER-CSU 6/15/2000            */

#ifndef FILEFUNCS_H
#define FILEFUNCS_H

#include "include/generic.h"
#include "include/SW_datastructs.h"

#ifdef __cplusplus
extern "C" {
#endif


/* =================================================== */
/*             Global Function Declarations            */
/* --------------------------------------------------- */
FILE * OpenFile(const char *name, const char *mode, LOG_INFO* LogInfo);
void CloseFile(FILE **f, LOG_INFO* LogInfo);
Bool GetALine(FILE *f, char buf[], int numChars);
void DirName(const char *p, char *outString);
const char *BaseName(const char *p);
Bool FileExists(const char *f);
Bool DirExists(const char *d);
Bool ChDir(const char *d);
Bool MkDir(const char *dname, LOG_INFO* LogInfo);
Bool RemoveFiles(const char *fspec, LOG_INFO* LogInfo);
Bool CopyFile(const char *from, const char *to, LOG_INFO* LogInfo);
void LogError(LOG_INFO* LogInfo, const int mode, const char *fmt, ...);

int key_to_id(const char* key, const char **possibleKeys,
                     int numPossKeys);
void set_hasKey(int keyID, const char **possibleKeys, Bool *hasKeys, LOG_INFO* LogInfo);
void check_requiredKeys(Bool *hasKeys, const Bool *requiredKeys, const char **possibleKeys, int numKeys, LOG_INFO* LogInfo);


#ifdef __cplusplus
}
#endif

#endif
