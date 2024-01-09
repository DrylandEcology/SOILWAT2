#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "include/SW_Domain.h"
#include "include/filefuncs.h"
#include "include/SW_Files.h"
#include "include/SW_Times.h"
#include "include/myMemory.h"
#include "include/generic.h"

#if defined(SWNETCDF)
#include "include/SW_netCDF.h"
#endif

/* =================================================== */
/*                   Local Defines                     */
/* --------------------------------------------------- */

#define NUM_DOM_IN_KEYS 13 // Number of possible keys within `domain.in`

/* =================================================== */
/*             Private Function Declarations           */
/* --------------------------------------------------- */

/* =================================================== */
/*             Global Function Definitions             */
/* --------------------------------------------------- */

/**
 * @brief Calculate the suid for the start gridcell/site position
 *
 * @param[in] SW_Domain Struct of type SW_DOMAIN holding constant
 *  temporal/spatial information for a set of simulation runs
 * @param[in] suid Unique identifier for a simulation run
 * @param[out] ncSuid Unique indentifier of the first suid to run
 *  in relation to netCDFs
*/
void SW_DOM_calc_ncSuid(SW_DOMAIN* SW_Domain, unsigned long suid,
                             unsigned long ncSuid[]) {

    if(strcmp(SW_Domain->DomainType, "s") == 0) {
        ncSuid[0] = suid;
        ncSuid[1] = 0;
    } else {
        ncSuid[0] = suid / SW_Domain->nDimX;
        ncSuid[1] = suid % SW_Domain->nDimX;
    }
}

/**
 * @brief Calculate the number of suids in the given domain
 *
 * @param[in,out] SW_Domain Struct of type SW_DOMAIN holding constant
 *  temporal/spatial information for a set of simulation runs
*/
void SW_DOM_calc_nSUIDs(SW_DOMAIN* SW_Domain) {
    SW_Domain->nSUIDs = (strcmp(SW_Domain->DomainType, "s") == 0) ?
        SW_Domain->nDimS :
        SW_Domain->nDimX * SW_Domain->nDimY;
}

/**
 * @brief Check progress in domain
 *
 * @param[in] progFileID Identifier of the progress netCDF file
 * @param[in] progVarID Identifier of the progress variable
 * @param[in] ncSuid Current simulation unit identifier for which is used
 *  to get data from netCDF
 * @param[in,out] LogInfo Holds information dealing with logfile output
 *
 * @return
 * TRUE if simulation for \p ncSuid has not been completed yet;
 * FALSE if simulation for \p ncSuid has been completed (i.e., skip).
*/
Bool SW_DOM_CheckProgress(int progFileID, int progVarID,
                          unsigned long ncSuid[], LOG_INFO* LogInfo) {
    #if defined(SWNETCDF)
    return SW_NC_check_progress(progFileID, progVarID, ncSuid, LogInfo);
    #else
    (void) progFileID;
    (void) progVarID;
    (void) ncSuid;
    (void) LogInfo;
    #endif

    // return TRUE (due to lack of capability to track progress)
    return swTRUE;
}

/**
 * @brief Create an empty progress netCDF
 *
 * @param[in] SW_Domain Struct of type SW_DOMAIN holding constant
 *  temporal/spatial information for a set of simulation runs
 * @param[in] LogInfo Holds information dealing with logfile output
*/
void SW_DOM_CreateProgress(SW_DOMAIN* SW_Domain, LOG_INFO* LogInfo) {
    #if defined(SWNETCDF)
    SW_NC_create_progress(SW_Domain, LogInfo);
    #else
    (void) SW_Domain;
    (void) LogInfo;
    #endif
}

/**
 * @brief Read `domain.in` and report any problems encountered when doing so
 *
 * @param[in,out] SW_Domain Struct of type SW_DOMAIN holding constant
 *      temporal/spatial information for a set of simulation runs
 * @param[in] LogInfo Holds information on warnings and errors
*/
void SW_DOM_read(SW_DOMAIN* SW_Domain, LOG_INFO* LogInfo) {

    static const char *possibleKeys[NUM_DOM_IN_KEYS] =
            {"Domain", "nDimX", "nDimY", "nDimS",
            "StartYear", "EndYear", "StartDoy", "EndDoy",
            "crs_bbox", "xmin_bbox", "ymin_bbox", "xmax_bbox", "ymax_bbox"};
    static const Bool requiredKeys[NUM_DOM_IN_KEYS] =
            {swTRUE, swTRUE, swTRUE, swTRUE,
            swTRUE, swTRUE, swFALSE, swFALSE,
            swTRUE, swTRUE, swTRUE, swTRUE, swTRUE};
    Bool hasKeys[NUM_DOM_IN_KEYS] = {swFALSE};

    FILE *f;
    int y, keyID;
    char inbuf[LARGE_VALUE], *MyFileName;
    char key[10], value[LARGE_VALUE]; // 10 - Max key size

    MyFileName = SW_Domain->PathInfo.InFiles[eDomain];
	f = OpenFile(MyFileName, "r", LogInfo);

    // Set SW_DOMAIN
    while(GetALine(f, inbuf, LARGE_VALUE)) {
        sscanf(inbuf, "%9s %s", key, value);

        keyID = key_to_id(key, possibleKeys, NUM_DOM_IN_KEYS);
        set_hasKey(keyID, possibleKeys, hasKeys, LogInfo); // no error, only warnings possible

        switch(keyID) {
            case 0: // Domain type
                if(strcmp(value, "xy") != 0 && strcmp(value, "s") != 0) {
                    LogError(LogInfo, LOGERROR, "%s: Incorrect domain type %s."\
                             " Please select from \"xy\" and \"s\".",
                             MyFileName, value);
                    return; // Exit function prematurely due to error
                }
                strcpy(SW_Domain->DomainType, value);
                break;
            case 1: // Number of X slots
                SW_Domain->nDimX = atoi(value);
                break;
            case 2: // Number of Y slots
                SW_Domain->nDimY = atoi(value);
                break;
            case 3: // Number of S slots
                SW_Domain->nDimS = atoi(value);
                break;
            case 4: // Start year
                y = atoi(value);

                if (y < 0) {
                    CloseFile(&f, LogInfo);
                    LogError(LogInfo, LOGERROR,
                             "%s: Negative start year (%d)", MyFileName, y);
                    return; // Exit function prematurely due to error
                }
                SW_Domain->startyr = yearto4digit((TimeInt) y);
                break;
            case 5: // End year
                y = atoi(value);

                if (y < 0) {
                    CloseFile(&f, LogInfo);
                    LogError(LogInfo, LOGERROR,
                             "%s: Negative ending year (%d)", MyFileName, y);
                    return; // Exit function prematurely due to error
                }
                SW_Domain->endyr = yearto4digit((TimeInt) y);
                break;
            case 6: // Start day of year
                SW_Domain->startstart = atoi(value);
                break;
            case 7: // End day of year
                SW_Domain->endend = atoi(value);
                break;
            case 8: // CRS box
                // Re-scan and get the entire value (including spaces)
                sscanf(inbuf, "%9s %27[^\n]", key, value);
                strcpy(SW_Domain->crs_bbox, value);
                break;
            case 9: // Minimum x coordinate
                SW_Domain->min_x = atof(value);
                break;
            case 10: // Minimum y coordinate
                SW_Domain->min_y = atof(value);
                break;
            case 11: // Maximum x coordinate
                SW_Domain->max_x = atof(value);
                break;
            case 12: // Maximum y coordinate
                SW_Domain->max_y = atof(value);
                break;
            case KEY_NOT_FOUND: // Unknown key
                LogError(LogInfo, LOGWARN, "%s: Ignoring an unknown key, %s",
                         MyFileName, key);
                break;
        }
    }

    CloseFile(&f, LogInfo);


    // Check if all required input was provided
    check_requiredKeys(hasKeys, requiredKeys, possibleKeys, NUM_DOM_IN_KEYS, LogInfo);
    if(LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

    if (SW_Domain->endyr < SW_Domain->startyr) {
          LogError(LogInfo, LOGERROR, "%s: Start Year > End Year", MyFileName);
          return; // Exit function prematurely due to error
    }

    // Check if start day of year was not found
    keyID = key_to_id("StartDoy", possibleKeys, NUM_DOM_IN_KEYS);
    if(!hasKeys[keyID]) {
        LogError(LogInfo, LOGWARN, "Domain.in: Missing Start Day - using 1\n");
        SW_Domain->startstart = 1;
    }

    // Check end day of year
    keyID = key_to_id("EndDoy", possibleKeys, NUM_DOM_IN_KEYS);
    if (SW_Domain->endend == 365 || !hasKeys[keyID]) {
        // Make sure last day is correct if last year is a leap year and
        // last day is last day of that year
        SW_Domain->endend = Time_get_lastdoy_y(SW_Domain->endyr);
    }
    if (!hasKeys[keyID]) {
          LogError(LogInfo, LOGWARN,
                  "Domain.in: Missing End Day - using %d\n", SW_Domain->endend);
    }

    // Check bounding box coordinates
    if (GT(SW_Domain->min_x, SW_Domain->max_x)) {
        LogError(LogInfo, LOGERROR, "Domain.in: bbox x-axis min > max.");
        return; // Exit function prematurely due to error
    }

    if (GT(SW_Domain->min_y, SW_Domain->max_y)) {
        LogError(LogInfo, LOGERROR, "Domain.in: bbox y-axis min > max.");
        return; // Exit function prematurely due to error
    }
}

/**
 * @brief Mark completion status of simulation run
 *
 * @param[in] isFailure Did simulation run fail or succeed?
 * @param[in] domType Type of domain in which simulations are running
 *  (gridcell/sites)
 * @param[in] progFileID Identifier of the progress netCDF file
 * @param[in] progVarID Identifier of the progress variable
 * @param[in] ncSuid Unique indentifier of the first suid to run
 *  in relation to netCDFs
 * @param[in,out] LogInfo
*/
void SW_DOM_SetProgress(Bool isFailure, const char* domType, int progFileID,
                        int progVarID, unsigned long ncSuid[],
                        LOG_INFO* LogInfo) {

    #if defined(SWNETCDF)
    SW_NC_set_progress(isFailure, domType, progFileID, progVarID, ncSuid, LogInfo);
    #else
    (void) isFailure;
    (void) progFileID;
    (void) progVarID;
    (void) ncSuid;
    (void) LogInfo;
    (void) domType;
    #endif
}

/**
 * @brief Calculate range of suids to run simulations for
 *
 * @param[in,out] SW_Domain Struct of type SW_DOMAIN holding constant
 *  temporal/spatial information for a set of simulation runs
 * @param[in] userSUID Simulation Unit Identifier requested by the user (base1);
 *            0 indicates that all simulations units within domain are requested
 * @param[out] LogInfo Holds information on warnings and errors
*/
void SW_DOM_SimSet(SW_DOMAIN* SW_Domain, unsigned long userSUID,
                   LOG_INFO* LogInfo) {

    Bool progFound;
    unsigned long
      *startSimSet = &SW_Domain->startSimSet,
      *endSimSet = &SW_Domain->endSimSet,
      startSuid[2]; // 2 -> [y, x] or [0, s]
    int progFileID = 0; // Value does not matter if SWNETCDF is not defined
    int progVarID = 0; // Value does not matter if SWNETCDF is not defined

    #if defined(SWNETCDF)
    progFileID = SW_Domain->netCDFInfo.ncFileIDs[vNCprog];
    progVarID = SW_Domain->netCDFInfo.ncVarIDs[vNCprog];
    #endif

    if(userSUID > 0) {
        if(userSUID > SW_Domain->nSUIDs) {
            LogError(LogInfo, LOGERROR,
                "User requested simulation unit (suid = %lu) "
                "does not exist in simulation domain (n = %lu).",
                userSUID, SW_Domain->nSUIDs
            );
            return; // Exit function prematurely due to error
        }

        *startSimSet = userSUID - 1;
        *endSimSet = userSUID;
    } else {
        #if defined(SOILWAT)
        if(LogInfo->printProgressMsg) {
            sw_message("is identifying the simulation set ...");
        }
        #endif

        *endSimSet = SW_Domain->nSUIDs;
        for(*startSimSet = 0; *startSimSet < *endSimSet; (*startSimSet)++) {
            SW_DOM_calc_ncSuid(SW_Domain, *startSimSet, startSuid);

            progFound = SW_DOM_CheckProgress(progFileID, progVarID,
                                             startSuid, LogInfo);

            if(progFound || LogInfo->stopRun) {
                return; // Found start suid or error occurred
            }
        }
    }
}

void SW_DOM_deepCopy(SW_DOMAIN* source, SW_DOMAIN* dest, LOG_INFO* LogInfo) {
    memcpy(dest, source, sizeof (*dest));

    SW_F_deepCopy(&dest->PathInfo, &source->PathInfo, LogInfo);

    #if defined(SWNETCDF)
    SW_NC_deepCopy(&dest->netCDFInfo, &source->netCDFInfo, LogInfo);
    #endif
}

void SW_DOM_init_ptrs(SW_DOMAIN* SW_Domain) {
    SW_F_init_ptrs(SW_Domain->PathInfo.InFiles);

    #if defined(SWNETCDF)
    SW_NC_init_ptrs(&SW_Domain->netCDFInfo);
    #endif
}

void SW_DOM_deconstruct(SW_DOMAIN* SW_Domain) {
    SW_F_deconstruct(SW_Domain->PathInfo.InFiles);

    #if defined(SWNETCDF)
    SW_NC_deconstruct(&SW_Domain->netCDFInfo);
    SW_NC_close_files(&SW_Domain->netCDFInfo);
    #endif
}


/* =================================================== */
/*             Local Function Definitions              */
/* --------------------------------------------------- */
