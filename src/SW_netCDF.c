#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netcdf.h>

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

/**
 * @brief Creates a new domain netCDF if one was not provided
 *
 * @param[in] SW_Domain Struct of type SW_DOMAIN holding constant
 *  temporal/spatial information for a set of simulation runs
 * @param[in] DomainName Domain netCDF name
 * @param[in] LogInfo Holds information dealing with logfile output
*/
void SW_NC_create_domain(SW_DOMAIN* SW_Domain, char* DomainName,
                         LOG_INFO* LogInfo) {

    nc_create(DomainName, NC_NETCDF4, NULL); // Don't store file ID (OPEN_NC_ID)

    nc_close(OPEN_NC_ID);
}

/**
 * @brief Write an attribute (string value) to a variable
 *
 * @param[in] attName Attribute name to write
 * @param[in] attValue String value to write
 * @param[in] varID Variable id within netCDF to write to
 * @param[in] LogInfo Holds information dealing with logfile output
*/
void write_att_str(char* attName, char* attValue, int varID, LOG_INFO* LogInfo) {

    int res = nc_put_att_text(OPEN_NC_ID, varID, attName, strlen(attValue),
                              attValue);

    if(res != NC_NOERR) {
        LogError(LogInfo, LOGWARN, "A problem occurred when writing"\
                " attribute %s.", attName);
    }
}

/**
 * @brief Write an attribute (double value) to a variable
 *
 * @param[in] attName Attribute name to write
 * @param[in] attValue String value to write
 * @param[in] attSize Number of values that will be written to the attribute
 * @param[in] varID Variable id within netCDF to write to
 * @param[in] LogInfo Holds information dealing with logfile output
*/
void write_att_val(char* attName, double attValue[], int attSize,
                   int varID, LOG_INFO* LogInfo) {

    int res = nc_put_att_double(OPEN_NC_ID, varID, attName, NC_DOUBLE,
                                attSize, attValue);

    if(res != NC_NOERR) {
        LogError(LogInfo, LOGWARN, "A problem occurred when writing the" \
                 " attribute %s.", attName);
    }
}
