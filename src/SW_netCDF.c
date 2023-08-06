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

    MyFileName = PathInfo->InFiles_csv[eNCIn];
	f = OpenFile(MyFileName, "r", LogInfo);

    // Get domain file name
    if(GetALine(f, inbuf)) {
        sscanf(inbuf, "%s %s", key, value);

        PathInfo->InFiles_nc[DOMAIN_NC] = Str_Dup(value, LogInfo);
    }
}
