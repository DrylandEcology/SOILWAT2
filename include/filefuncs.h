/* filefuncs.h -- contains definitions related to */
/* some generic file management functions

 * REQUIRES: generic.h
 */

/* Chris Bennett @ LTER-CSU 6/15/2000            */

#ifndef FILEFUNCS_H
#define FILEFUNCS_H

#include "include/generic.h"        // for Bool
#include "include/SW_datastructs.h" // for LOG_INFO
#include <stdio.h>                  // for FILE

#ifdef __cplusplus
extern "C" {
#endif


/* =================================================== */
/*             Global Function Declarations            */
/* --------------------------------------------------- */
FILE *OpenFile(const char *name, const char *mode, LOG_INFO *LogInfo);

void CloseFile(FILE **f, LOG_INFO *LogInfo);

Bool GetALine(FILE *f, char buf[], int numChars);

void DirName(const char *p, char *outString);

const char *BaseName(const char *p);

Bool FileExists(const char *name);

Bool DirExists(const char *d);

Bool ChDir(const char *d);

void MkDir(const char *dname, LOG_INFO *LogInfo);

Bool RemoveFiles(const char *fspec, Bool clearDir, LOG_INFO *LogInfo);

Bool CopyFile(const char *from, const char *to, LOG_INFO *LogInfo);

void LogError(LOG_INFO *LogInfo, const int mode, const char *fmt, ...);

void sw_message(const char *msg);

unsigned long int sw_strtoul(
    const char *str, const char *errMsg, LOG_INFO *LogInfo
);

long int sw_strtol(const char *str, const char *errMsg, LOG_INFO *LogInfo);

int sw_strtoi(const char *str, const char *errMsg, LOG_INFO *LogInfo);

double sw_strtod(const char *str, const char *errMsg, LOG_INFO *LogInfo);

int key_to_id(const char *key, const char **possibleKeys, int numPossKeys);

void set_hasKey(
    int keyID, const char **possibleKeys, Bool *hasKeys, LOG_INFO *LogInfo
);

void check_requiredKeys(
    const Bool *hasKeys,
    const Bool *requiredKeys,
    const char **possibleKeys,
    int numKeys,
    LOG_INFO *LogInfo
);


#ifdef __cplusplus
}
#endif

#endif
