#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "include/SW_netCDF.h"
#include "include/SW_Files.h"
#include "include/myMemory.h"
#include "include/filefuncs.h"

/**
 * @brief Read input files for netCDF related actions
 *
 * @param[in] PathInfo Struct holding all information about the programs path/files
 * @param[in] LogInfo Holds information dealing with logfile output
*/
void SW_NC_read_files(PATH_INFO* PathInfo, LOG_INFO* LogInfo) {

    FILE *f;
    char inbuf[MAX_FILENAMESIZE], *MyFileName;
    char key[15], value[MAX_FILENAMESIZE]; // 15 - Max key size
    int keyID;

    MyFileName = PathInfo->InFiles_csv[eNCIn];
	f = OpenFile(MyFileName, "r", LogInfo);

    // Get domain file name
    while(GetALine(f, inbuf)) {
        sscanf(inbuf, "%s %s", key, value);

        keyID = nc_key_to_ID(key);
        switch(keyID) {
            case DOMAIN_NC:
                PathInfo->InFiles_nc[DOMAIN_NC] = Str_Dup(value, LogInfo);
                break;
            default:
                LogError(LogInfo, LOGFATAL, "Ignoring unknown key in %s, %s",
                         MyFileName, key);
                break;
        }
    }
}

/**
 * @brief Helper function to `SW_NC_read_files()` to determine which input
 *        key has been read in
 * @param[in] key Read-in key from domain input files file
*/
int nc_key_to_ID(char* key) {
    static const char *possibleKeys[] = {"Domain"};
    int keyIndex, result, sameString = 0;

    for(keyIndex = 0; keyIndex < SW_NFILESNC; keyIndex++) {
        result = strcmp(key, possibleKeys[keyIndex]);

        if(result == sameString) {
            return keyIndex;
        }
    }

    return KEY_NOT_FOUND;
}
    }
}
