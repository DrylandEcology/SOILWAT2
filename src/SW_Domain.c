#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "include/SW_Domain.h"
#include "include/filefuncs.h"
#include "include/SW_Files.h"
#include "include/SW_Times.h"

/* =================================================== */
/*                   Local Defines                     */
/* --------------------------------------------------- */

#define NUM_DOM_IN_KEYS 8 // Number of possible keys within `domain.in`

/* =================================================== */
/*             Private Function Declarations           */
/* --------------------------------------------------- */
static int domain_inkey_to_id(char *key);

/* =================================================== */
/*             Global Function Definitions             */
/* --------------------------------------------------- */

/**
 * @brief Calculate the suid for the start gridcell/site position
 *
 * @param[in] SW_Domain Struct of type SW_DOMAIN holding constant
 *  temporal/spatial information for a set of simulation runs
 * @param[in] suid Unique identifier for a simulation run
 * @param[out] ncStartSuid Unique indentifier of the first suid to run
 *  in relation to netCDFs
*/
void SW_DOM_calc_ncStartSuid(SW_DOMAIN* SW_Domain, unsigned long suid,
                             unsigned long ncStartSuid[]) {

    if(strcmp(SW_Domain->DomainType, "s") == 0) {
        ncStartSuid[0] = 0;
        ncStartSuid[1] = SW_Domain->nDimS;
    } else {
        ncStartSuid[0] = suid / SW_Domain->nDimX;
        ncStartSuid[1] = suid % SW_Domain->nDimX;
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
 * @param[in] ncStartSuid Current simulation unit identifier for which progress
 * should be checked.
 *
 * @return
 * TRUE if simulation for \p ncStartSuid has not been completed yet;
 * FALSE if simulation for \p ncStartSuid has been completed (i.e., skip).
*/
Bool SW_DOM_CheckProgress(char* domainType, unsigned long ncStartSuid[]) {
    (void) domainType;
    (void) ncStartSuid;

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

    FILE *f;
    int y, keyID;
    char inbuf[MAX_FILENAMESIZE], *MyFileName;
    TimeInt tempdoy;
    char key[10], value[6]; // 10 - Max key size, 6 - max characters
    Bool fstartdy = swFALSE, fenddy = swFALSE;

    MyFileName = SW_Domain->PathInfo.InFiles[eDomain];
	f = OpenFile(MyFileName, "r", LogInfo);

    // Set SW_DOMAIN
    while(GetALine(f, inbuf)) {
        sscanf(inbuf, "%s %s", key, value);

        keyID = domain_inkey_to_id(key);
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
 * @param[in] ncStartSuid Unique indentifier of the first suid to run
 *  in relation to netCDFs
*/
void SW_DOM_SetProgress(char* domainType, unsigned long ncStartSuid[]) {
    (void) domainType;
    (void) ncStartSuid;
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
            SW_DOM_calc_ncStartSuid(SW_Domain, *startSimSet, startSuid);

            if(SW_DOM_CheckProgress(SW_Domain->DomainType, startSuid)) {
                return; // Found start suid
            }
        }
    }
}

/* =================================================== */
/*             Local Function Definitions              */
/* --------------------------------------------------- */

/**
 * @brief Helper function to `SW_DOM_read()` to determine which input
 *        key has been read in
 *
 * @param[in] key Read-in key from domain input file
 *
 * @return Either the ID of the key within a fixed set of possible keys,
 *         or KEY_NOT_FOUND (-1) for an unknown key
*/
static int domain_inkey_to_id(char *key) {
    static const char *possibleKeys[] =
            {"Domain", "nDimX", "nDimY", "nDimS",
            "StartYear", "EndYear", "StartDoy", "EndDoy"};

    int id, stringMatch = 0, compRes;

    for(id = 0; id < NUM_DOM_IN_KEYS; id++) {
        compRes = strcmp(key, possibleKeys[id]);

        if(compRes == stringMatch) {
            return id;
        }
    }

    return KEY_NOT_FOUND;
}
