#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "include/SW_Domain.h"
#include "include/filefuncs.h"
#include "include/SW_Files.h"
#include "include/SW_Times.h"
#include "include/myMemory.h"
#include "include/generic.h"

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
        ncSuid[0] = 0;
        ncSuid[1] = suid;
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
 * @param[in] domainType Type of domain in which simulations are running
 *  (gridcell/sites)
 * @param[in] ncSuid Current simulation unit identifier for which progress
 * should be checked.
 *
 * @return
 * TRUE if simulation for \p ncSuid has not been completed yet;
 * FALSE if simulation for \p ncSuid has been completed (i.e., skip).
*/
Bool SW_DOM_CheckProgress(char* domainType, unsigned long ncSuid[]) {
    (void) domainType;
    (void) ncSuid;

    // return TRUE (due to lack of capability to track progress)
    return swTRUE;
}

/**
 * @brief Create an empty progress netCDF
 *
 * @param[in] SW_Domain Struct of type SW_DOMAIN holding constant
 *  temporal/spatial information for a set of simulation runs
*/
void SW_DOM_CreateProgress(SW_DOMAIN* SW_Domain) {
    (void) SW_Domain;
}

/**
 * @brief Read `domain.in` and report any problems encountered when doing so
 *
 * @param[in,out] SW_Domain Struct of type SW_DOMAIN holding constant
 *      temporal/spatial information for a set of simulation runs
 * @param[in] LogInfo Holds information dealing with logfile output
*/
void SW_DOM_read(SW_DOMAIN* SW_Domain, LOG_INFO* LogInfo) {

    static const char *possibleKeys[] =
            {"Domain", "nDimX", "nDimY", "nDimS",
            "StartYear", "EndYear", "StartDoy", "EndDoy",
            "crs_bbox", "xmin_bbox", "ymin_bbox", "xmax_bbox", "ymax_bbox"};

    FILE *f;
    int y, keyID;
    char inbuf[MAX_FILENAMESIZE], *MyFileName;
    TimeInt tempdoy;
    char key[10], value[12]; // 10 - Max key size, 12 - max value characters
    Bool fstartdy = swFALSE, fenddy = swFALSE;

    MyFileName = SW_Domain->PathInfo.InFiles[eDomain];
	f = OpenFile(MyFileName, "r", LogInfo);

    // Set SW_DOMAIN
    while(GetALine(f, inbuf)) {
        sscanf(inbuf, "%s %s", key, value);

        keyID = key_to_id(key, possibleKeys, NUM_DOM_IN_KEYS);
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
                fstartdy = swTRUE;
                break;
            case 7: // End day of year
                tempdoy = atoi(value);
                SW_Domain->endend = (tempdoy == 365) ?
                                Time_get_lastdoy_y(SW_Domain->endyr) : 365;
                fenddy = swTRUE;
                break;
            case 8: // CRS box
                SW_Domain->crs_bbox = Str_Dup(value, LogInfo);
                if(LogInfo->stopRun) {
                    return; // Exit function prematurely due to error
                }
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


	if (SW_Domain->endyr < SW_Domain->startyr) {
        LogError(LogInfo, LOGERROR, "%s: Start Year > End Year", MyFileName);
        return; // Exit function prematurely due to error
	}

    // Check if start day of year was not found
    if(!fstartdy) {
        LogError(LogInfo, LOGWARN, "Domain.in: Missing Start Day - using 1\n");
        SW_Domain->startstart = 1;
    }

    // Check if end day of year was not found
	if (!fenddy) {
		SW_Domain->endend = Time_get_lastdoy_y(SW_Domain->endyr);
        LogError(LogInfo, LOGWARN,
                "Domain.in: Missing End Day - using %d\n", SW_Domain->endend);
	}
}

/**
 * @brief Mark a completed suid in progress netCDF
 *
 * @param[in] domainType Type of domain in which simulations are running
 *  (gridcell/sites)
 * @param[in] ncSuid Unique indentifier of the first suid to run
 *  in relation to netCDFs
*/
void SW_DOM_SetProgress(char* domainType, unsigned long ncSuid[]) {
    (void) domainType;
    (void) ncSuid;
}

/**
 * @brief Calculate range of suids to run simulations for
 *
 * @param[in,out] SW_Domain Struct of type SW_DOMAIN holding constant
 *  temporal/spatial information for a set of simulation runs
 * @param[in] userSUID Simulation Unit Identifier requested by the user (base1);
 *            0 indicates that all simulations units within domain are requested
 * @param[in,out] LogInfo Holds information dealing with logfile output
*/
void SW_DOM_SimSet(SW_DOMAIN* SW_Domain, unsigned long userSUID,
                   LOG_INFO* LogInfo) {

    unsigned long
      *startSimSet = &SW_Domain->startSimSet,
      *endSimSet = &SW_Domain->endSimSet,
      startSuid[2]; // 2 -> [y, x] or [0, s]

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
        *endSimSet = SW_Domain->nSUIDs;
        for(*startSimSet = 0; *startSimSet < *endSimSet; (*startSimSet)++) {
            SW_DOM_calc_ncSuid(SW_Domain, *startSimSet, startSuid);

            if(SW_DOM_CheckProgress(SW_Domain->DomainType, startSuid)) {
                return; // Found start suid
            }
        }
    }
}

void SW_DOM_deepCopy(SW_DOMAIN* source, SW_DOMAIN* dest, LOG_INFO* LogInfo) {
    memcpy(dest, source, sizeof (*dest));

    SW_F_deepCopy(&dest->PathInfo, &source->PathInfo, LogInfo);
}

/* =================================================== */
/*             Local Function Definitions              */
/* --------------------------------------------------- */
