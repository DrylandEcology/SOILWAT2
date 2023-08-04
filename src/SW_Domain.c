#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "include/SW_Domain.h"
#include "include/filefuncs.h"
#include "include/SW_Files.h"
#include "include/SW_Times.h"

/* =================================================== */
/*             Local Function Definitions              */
/* --------------------------------------------------- */

/**
 * @brief Read `domain.in` and report any problems encountered when doing so
 *
 * @param[in,out] SW_Model Struct of type SW_MODEL holding basic time
 *      information about the simulation
 * @param[in] InFiles Array of program in/output files
 * @param[out] SW_Domain Struct of type SW_DOMAIN holding constant
 *      temporal/spacial information for a set of simulation runs
 * @param[in] LogInfo Holds information dealing with logfile output
*/
void SW_DOM_read(SW_MODEL* SW_Model, char *InFiles[],
    SW_DOMAIN* SW_Domain, LOG_INFO* LogInfo) {

    FILE *f;
    int y, keyID;
    char inbuf[MAX_FILENAMESIZE], *MyFileName;
    TimeInt tempdoy;
    char key[10], value[6]; // 10 - Max key size, 6 - max characters
    Bool fstartdy = swFALSE, fenddy = swFALSE;

    MyFileName = InFiles[eDomain];
	f = OpenFile(MyFileName, "r", LogInfo);

    // Set SW_DOMAIN
    while(GetALine(f, inbuf)) {
        sscanf(inbuf, "%s %s", key, value);

        keyID = domain_inkey_to_id(key);
        switch(keyID) {
            case 0: // Domain type
                if(strcmp(value, "xy") != 0 && strcmp(value, "s") != 0) {
                    LogError(LogInfo, LOGFATAL, "%s: Incorrect domain type %s."\
                             " Please select from \"xy\" and \"s\".",
                             MyFileName, value);
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
                    LogError(LogInfo, LOGFATAL,
                             "%s: Negative start year (%d)", MyFileName, y);
                }
                SW_Domain->startyr = yearto4digit((TimeInt) y);
                SW_Model->addtl_yr = 0; // Could be done anywhere; SOILWAT2 runs don't need a delta year
                break;
            case 5: // End year
                y = atoi(value);

                if (y < 0) {
                    CloseFile(&f, LogInfo);
                    LogError(LogInfo, LOGFATAL,
                             "%s: Negative ending year (%d)", MyFileName, y);
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

	if (SW_Domain->endyr < SW_Domain->startyr) {
		CloseFile(&f, LogInfo);
        LogError(LogInfo, LOGFATAL, "%s: Start Year > End Year", MyFileName);
	}

    // Check if start day of year was not found
    if(!fstartdy) {
        LogError(LogInfo, LOGWARN, "Domain.in: Missing Start Day - using 1\n");
        SW_Domain->startstart = 1;
    }

	SW_Domain->startstart += ((SW_Model->isnorth) ? DAYFIRST_NORTH : DAYFIRST_SOUTH) - 1;

    // Check if end day of year was not found
	if (!fenddy) {
		SW_Domain->endend = (SW_Model->isnorth) ?
                        Time_get_lastdoy_y(SW_Domain->endyr) : DAYLAST_SOUTH;
        LogError(LogInfo, LOGWARN,
                "Domain.in: Missing End Day - using \"%d\"\n", SW_Domain->endend);
	}

	SW_Model->daymid = (SW_Model->isnorth) ? DAYMID_NORTH : DAYMID_SOUTH;

    // Copy information to SW_MODEL
    SW_Model->startyr = SW_Domain->startyr; // Copy start year
    SW_Model->endyr = SW_Domain->endyr; // Copy end year
    SW_Model->startstart = SW_Domain->startstart; // Copy start doy
    SW_Model->endend = SW_Domain->endend; // Copy end doy
}

/**
 * @brief Helper function to `SW_DOM_read()` to determine which input
 *        key has been read in
 *
 * @param[in] key Read-in key from domain input file
 *
 * @return Either the ID of the key within a fixed set of possible keys,
 *         or KEY_NOT_FOUND (-1) for an unknown key
*/
int domain_inkey_to_id(char *key) {
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
