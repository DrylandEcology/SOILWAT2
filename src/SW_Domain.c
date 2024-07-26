
/* =================================================== */
/*                INCLUDES / DEFINES                   */
/* --------------------------------------------------- */
#include "include/SW_Domain.h"      // for SW_DOM_CheckProgress, SW_DOM_Cre...
#include "include/filefuncs.h"      // for LogError, CloseFile, key_to_id
#include "include/generic.h"        // for swTRUE, LOGERROR, swFALSE, Bool
#include "include/SW_datastructs.h" // for SW_DOMAIN, LOG_INFO
#include "include/SW_Defines.h"     // for LyrIndex, LARGE_VALUE, TimeInt
#include "include/SW_Files.h"       // for SW_F_deconstruct, SW_F_deepCopy
#include "include/SW_Output.h"      // for ForEachOutKey
#include "include/Times.h"          // for yearto4digit, Time_get_lastdoy_y
#include <stdio.h>                  // for sscanf, FILE
#include <stdlib.h>                 // for strtod, strtol
#include <string.h>                 // for strcmp, memcpy, strcpy, memset

#if defined(SWNETCDF)
#include "include/SW_netCDF.h"
#endif

#if defined(SOILWAT)
#include "include/rands.h" // for RandSeed
#endif


/* =================================================== */
/*                   Local Defines                     */
/* --------------------------------------------------- */

#define NUM_DOM_IN_KEYS 17 // Number of possible keys within `domain.in`

/* =================================================== */
/*             Private Function Declarations           */
/* --------------------------------------------------- */

/* =================================================== */
/*             Global Function Definitions             */
/* --------------------------------------------------- */

/**
@brief Calculate the suid for the start gridcell/site position

@param[in] SW_Domain Struct of type SW_DOMAIN holding constant
    temporal/spatial information for a set of simulation runs
@param[in] suid Unique identifier for a simulation run
@param[out] ncSuid Unique indentifier of the first suid to run
    in relation to netCDFs
*/
void SW_DOM_calc_ncSuid(
    SW_DOMAIN *SW_Domain, unsigned long suid, unsigned long ncSuid[]
) {

    if (strcmp(SW_Domain->DomainType, "s") == 0) {
        ncSuid[0] = suid;
        ncSuid[1] = 0;
    } else {
        ncSuid[0] = suid / SW_Domain->nDimX;
        ncSuid[1] = suid % SW_Domain->nDimX;
    }
}

/**
@brief Calculate the number of suids in the given domain

@param[in,out] SW_Domain Struct of type SW_DOMAIN holding constant
    temporal/spatial information for a set of simulation runs
*/
void SW_DOM_calc_nSUIDs(SW_DOMAIN *SW_Domain) {
    SW_Domain->nSUIDs = (strcmp(SW_Domain->DomainType, "s") == 0) ?
                            SW_Domain->nDimS :
                            SW_Domain->nDimX * SW_Domain->nDimY;
}

/**
@brief Check progress in domain

@param[in] progFileID Identifier of the progress netCDF file
@param[in] progVarID Identifier of the progress variable
@param[in] ncSuid Current simulation unit identifier for which is used
    to get data from netCDF
@param[in,out] LogInfo Holds information dealing with logfile output

@return
TRUE if simulation for \p ncSuid has not been completed yet;
FALSE if simulation for \p ncSuid has been completed (i.e., skip).
*/
Bool SW_DOM_CheckProgress(
    int progFileID,
    int progVarID,
    unsigned long ncSuid[], // NOLINT(readability-non-const-parameter)
    LOG_INFO *LogInfo
) {
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
@brief Create an empty progress netCDF

@param[in] SW_Domain Struct of type SW_DOMAIN holding constant
    temporal/spatial information for a set of simulation runs
@param[in] LogInfo Holds information dealing with logfile output
*/
void SW_DOM_CreateProgress(SW_DOMAIN *SW_Domain, LOG_INFO *LogInfo) {
#if defined(SWNETCDF)
    SW_NC_create_progress(SW_Domain, LogInfo);
#else
    (void) SW_Domain;
    (void) LogInfo;
#endif
}

/**
@brief Domain constructor for global variables which are constant between
    simulation runs.

@param[in] rng_seed Initial state for spinup RNG
@param[out] SW_Domain Struct of type SW_DOMAIN which
    holds constant temporal/spatial information for a set of simulation runs
*/
void SW_DOM_construct(unsigned long rng_seed, SW_DOMAIN *SW_Domain) {

/* Set seed of `spinup_rng`
  - SOILWAT2: set seed here
  - STEPWAT2: `main()` uses `Globals.randseed` to (re-)set for each iteration
  - rSOILWAT2: R API handles RNGs
*/
#if defined(SOILWAT)
    RandSeed(rng_seed, 1u, &SW_Domain->SW_SpinUp.spinup_rng);
#else
    (void) rng_seed; // Silence compiler flag `-Wunused-parameter`
#endif

    SW_Domain->nMaxSoilLayers = 0;
    SW_Domain->nMaxEvapLayers = 0;
    SW_Domain->hasConsistentSoilLayerDepths = swFALSE;
    memset(
        &SW_Domain->depthsAllSoilLayers,
        0.,
        sizeof(&SW_Domain->depthsAllSoilLayers[0]) * MAX_LAYERS
    );

    SW_OUTDOM_construct(&SW_Domain->OutDom);
}

/**
@brief Read `domain.in` and report any problems encountered when doing so

@param[in,out] SW_Domain Struct of type SW_DOMAIN holding constant
    temporal/spatial information for a set of simulation runs
@param[in] LogInfo Holds information on warnings and errors
*/
void SW_DOM_read(SW_DOMAIN *SW_Domain, LOG_INFO *LogInfo) {

    static const char *possibleKeys[NUM_DOM_IN_KEYS] = {
        "Domain",
        "nDimX",
        "nDimY",
        "nDimS",
        "StartYear",
        "EndYear",
        "StartDoy",
        "EndDoy",
        "crs_bbox",
        "xmin_bbox",
        "ymin_bbox",
        "xmax_bbox",
        "ymax_bbox",
        "SpinupMode",
        "SpinupScope",
        "SpinupDuration",
        "SpinupSeed"
    };
    static const Bool requiredKeys[NUM_DOM_IN_KEYS] = {
        swTRUE,
        swTRUE,
        swTRUE,
        swTRUE,
        swTRUE,
        swTRUE,
        swFALSE,
        swFALSE,
        swTRUE,
        swTRUE,
        swTRUE,
        swTRUE,
        swTRUE,
        swTRUE,
        swTRUE,
        swTRUE,
        swTRUE
    };
    Bool hasKeys[NUM_DOM_IN_KEYS] = {swFALSE};

    FILE *f;
    int y, keyID;
    char inbuf[LARGE_VALUE], *MyFileName;
    char key[15], value[LARGE_VALUE]; // 15 - Max key size
    int intRes = 0, scanRes;
    double doubleRes = 0.;

    Bool doDoubleConv;

    MyFileName = SW_Domain->PathInfo.InFiles[eDomain];
    f = OpenFile(MyFileName, "r", LogInfo);
    if (LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

    // Set SW_DOMAIN
    while (GetALine(f, inbuf, LARGE_VALUE)) {
        scanRes = sscanf(inbuf, "%14s %s", key, value);

        if (scanRes < 2) {
            LogError(
                LogInfo, LOGERROR, "Invalid key-value pair in %s.", MyFileName
            );
            return; /* Exit prematurely due to error */
        }

        keyID = key_to_id(key, possibleKeys, NUM_DOM_IN_KEYS);

        set_hasKey(keyID, possibleKeys, hasKeys, LogInfo);
        // set_hasKey() produces never an error, only possibly warnings

        /* Make sure we are not trying to convert a string with no numerical
         * value */
        if (keyID > 0 && keyID <= 16 && keyID != 8) {

            /* Check to see if the line number contains a double or integer
             * value */
            doDoubleConv = (Bool) (keyID >= 9 && keyID <= 12);

            if (doDoubleConv) {
                doubleRes = sw_strtod(value, MyFileName, LogInfo);
            } else {
                intRes = sw_strtoi(value, MyFileName, LogInfo);
            }

            if (LogInfo->stopRun) {
                return; // Exit function prematurely due to error
            }
        }

        switch (keyID) {
        case 0: // Domain type
            if (strcmp(value, "xy") != 0 && strcmp(value, "s") != 0) {
                LogError(
                    LogInfo,
                    LOGERROR,
                    "%s: Incorrect domain type %s."
                    " Please select from \"xy\" and \"s\".",
                    MyFileName,
                    value
                );
                return; // Exit function prematurely due to error
            }
            strcpy(SW_Domain->DomainType, value);
            break;
        case 1: // Number of X slots
            SW_Domain->nDimX = intRes;
            break;
        case 2: // Number of Y slots
            SW_Domain->nDimY = intRes;
            break;
        case 3: // Number of S slots
            SW_Domain->nDimS = intRes;
            break;

        case 4: // Start year
            y = intRes;

            if (y < 0) {
                CloseFile(&f, LogInfo);
                LogError(
                    LogInfo,
                    LOGERROR,
                    "%s: Negative start year (%d)",
                    MyFileName,
                    y
                );
                return; // Exit function prematurely due to error
            }
            SW_Domain->startyr = yearto4digit((TimeInt) y);
            break;
        case 5: // End year
            y = intRes;

            if (y < 0) {
                CloseFile(&f, LogInfo);
                LogError(
                    LogInfo,
                    LOGERROR,
                    "%s: Negative ending year (%d)",
                    MyFileName,
                    y
                );
                return; // Exit function prematurely due to error
            }
            SW_Domain->endyr = yearto4digit((TimeInt) y);
            break;
        case 6: // Start day of year
            SW_Domain->startstart = intRes;
            break;
        case 7: // End day of year
            SW_Domain->endend = intRes;
            break;

        case 8: // CRS box
            // Re-scan and get the entire value (including spaces)
            scanRes = sscanf(inbuf, "%9s %27[^\n]", key, value);

            if (scanRes < 2) {
                LogError(
                    LogInfo,
                    LOGERROR,
                    "Invalid key-value pair for CRS box in %s.",
                    MyFileName
                );
                return; /* Exit prematurely due to error */
            }

            strcpy(SW_Domain->crs_bbox, value);
            break;
        case 9: // Minimum x coordinate
            SW_Domain->min_x = doubleRes;
            break;
        case 10: // Minimum y coordinate
            SW_Domain->min_y = doubleRes;
            break;
        case 11: // Maximum x coordinate
            SW_Domain->max_x = doubleRes;
            break;
        case 12: // Maximum y coordinate
            SW_Domain->max_y = doubleRes;
            break;

        case 13: // Spinup Mode
            y = intRes;

            if (y != 1 && y != 2) {
                CloseFile(&f, LogInfo);
                LogError(
                    LogInfo,
                    LOGERROR,
                    "%s: Incorrect Mode (%d) for spinup"
                    " Please select \"1\" or \"2\"",
                    MyFileName,
                    y
                );
                return; // Exit function prematurely due to error
            }
            SW_Domain->SW_SpinUp.mode = y;
            break;
        case 14: // Spinup Scope
            SW_Domain->SW_SpinUp.scope = intRes;
            break;
        case 15: // Spinup Duration
            SW_Domain->SW_SpinUp.duration = intRes;

            // Set the spinup flag to true if duration > 0
            if (SW_Domain->SW_SpinUp.duration <= 0) {
                SW_Domain->SW_SpinUp.spinup = swFALSE;
            } else {
                SW_Domain->SW_SpinUp.spinup = swTRUE;
            }
            break;
        case 16: // Spinup Seed
            SW_Domain->SW_SpinUp.rng_seed = intRes;
            break;

        case KEY_NOT_FOUND: // Unknown key

        default:
            LogError(
                LogInfo,
                LOGWARN,
                "%s: Ignoring an unknown key, %s",
                MyFileName,
                key
            );
            break;
        }
    }

    CloseFile(&f, LogInfo);


    // Check if all required input was provided
    check_requiredKeys(
        hasKeys, requiredKeys, possibleKeys, NUM_DOM_IN_KEYS, LogInfo
    );
    if (LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

    if (SW_Domain->endyr < SW_Domain->startyr) {
        LogError(LogInfo, LOGERROR, "%s: Start Year > End Year", MyFileName);
        return; // Exit function prematurely due to error
    }

    // Check if start day of year was not found
    keyID = key_to_id("StartDoy", possibleKeys, NUM_DOM_IN_KEYS);
    if (!hasKeys[keyID]) {
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
        LogError(
            LogInfo,
            LOGWARN,
            "Domain.in: Missing End Day - using %d\n",
            SW_Domain->endend
        );
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

    // Check if scope value is out of range
    if (SW_Domain->SW_SpinUp.scope < 1 ||
        SW_Domain->SW_SpinUp.scope > (SW_Domain->endyr - SW_Domain->startyr)) {
        LogError(
            LogInfo,
            LOGERROR,
            "%s: Invalid Scope (N = %d) for spinup",
            MyFileName,
            SW_Domain->SW_SpinUp.scope
        );
        return; // Exit function prematurely due to error
    }
}

/**
@brief Mark completion status of simulation run

@param[in] isFailure Did simulation run fail or succeed?
@param[in] domType Type of domain in which simulations are running
    (gridcell/sites)
@param[in] progFileID Identifier of the progress netCDF file
@param[in] progVarID Identifier of the progress variable
@param[in] ncSuid Unique indentifier of the first suid to run
    in relation to netCDFs
@param[in,out] LogInfo Holds information on warnings and errors
*/
void SW_DOM_SetProgress(
    Bool isFailure,
    const char *domType,
    int progFileID,
    int progVarID,
    unsigned long ncSuid[], // NOLINT(readability-non-const-parameter)
    LOG_INFO *LogInfo
) {

#if defined(SWNETCDF)
    SW_NC_set_progress(
        isFailure, domType, progFileID, progVarID, ncSuid, LogInfo
    );
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
@brief Calculate range of suids to run simulations for

@param[in,out] SW_Domain Struct of type SW_DOMAIN holding constant
    temporal/spatial information for a set of simulation runs
@param[in] userSUID Simulation Unit Identifier requested by the user (base1);
    0 indicates that all simulations units within domain are requested
@param[out] LogInfo Holds information on warnings and errors
*/
void SW_DOM_SimSet(
    SW_DOMAIN *SW_Domain, unsigned long userSUID, LOG_INFO *LogInfo
) {

    Bool progFound;
    unsigned long *startSimSet = &SW_Domain->startSimSet,
                  *endSimSet = &SW_Domain->endSimSet,
                  startSuid[2]; // 2 -> [y, x] or [0, s]
    int progFileID = 0; // Value does not matter if SWNETCDF is not defined
    int progVarID = 0;  // Value does not matter if SWNETCDF is not defined

#if defined(SWNETCDF)
    progFileID = SW_Domain->netCDFInfo.ncFileIDs[vNCprog];
    progVarID = SW_Domain->netCDFInfo.ncVarIDs[vNCprog];
#endif

    if (userSUID > 0) {
        if (userSUID > SW_Domain->nSUIDs) {
            LogError(
                LogInfo,
                LOGERROR,
                "User requested simulation unit (suid = %lu) "
                "does not exist in simulation domain (n = %lu).",
                userSUID,
                SW_Domain->nSUIDs
            );
            return; // Exit function prematurely due to error
        }

        *startSimSet = userSUID - 1;
        *endSimSet = userSUID;
    } else {
#if defined(SOILWAT)
        if (LogInfo->printProgressMsg) {
            sw_message("is identifying the simulation set ...");
        }
#endif

        *endSimSet = SW_Domain->nSUIDs;
        for (*startSimSet = 0; *startSimSet < *endSimSet; (*startSimSet)++) {
            SW_DOM_calc_ncSuid(SW_Domain, *startSimSet, startSuid);

            progFound =
                SW_DOM_CheckProgress(progFileID, progVarID, startSuid, LogInfo);

            if (progFound || LogInfo->stopRun) {
                return; // Found start suid or error occurred
            }
        }
    }
}

void SW_DOM_deepCopy(SW_DOMAIN *source, SW_DOMAIN *dest, LOG_INFO *LogInfo) {

    memcpy(dest, source, sizeof(*dest));

    SW_OUTDOM_deepCopy(&source->OutDom, &dest->OutDom, LogInfo);

    SW_F_deepCopy(&dest->PathInfo, &source->PathInfo, LogInfo);
    if (LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

#if defined(SWNETCDF)
    SW_NC_deepCopy(&dest->netCDFInfo, &source->netCDFInfo, LogInfo);
    if (LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }
#endif
}

void SW_DOM_init_ptrs(SW_DOMAIN *SW_Domain) {

    SW_OUTDOM_init_ptrs(&SW_Domain->OutDom);

    SW_F_init_ptrs(SW_Domain->PathInfo.InFiles);

#if defined(SWNETCDF)
    SW_NC_init_ptrs(&SW_Domain->netCDFInfo);
#endif
}

void SW_DOM_deconstruct(SW_DOMAIN *SW_Domain) {
    int k, i;

    SW_F_deconstruct(SW_Domain->PathInfo.InFiles);

#if defined(SWNETCDF)

    SW_NC_deconstruct(&SW_Domain->netCDFInfo);
    SW_NC_close_files(&SW_Domain->netCDFInfo);

    ForEachOutKey(k) {
        SW_NC_dealloc_outputkey_var_info(&SW_Domain->OutDom, k);
    }
#endif
    ForEachOutKey(k) {
        for (i = 0; i < 5 * NVEGTYPES + MAX_LAYERS; i++) {
            if (!isnull(SW_Domain->OutDom.colnames_OUT[k][i])) {
                free(SW_Domain->OutDom.colnames_OUT[k][i]);
                SW_Domain->OutDom.colnames_OUT[k][i] = NULL;
            }
        }
#ifdef RSOILWAT
        if (!isnull(SW_Domain->OutDom.outfile[k])) {
            free(SW_Domain->OutDom.outfile[k]);
            SW_Domain->OutDom.outfile[k] = NULL;
        }
#endif
    }
}

/** Identify soil profile information across simulation domain

    @param[out] hasConsistentSoilLayerDepths Flag indicating if all simulation
        run within domain have identical soil layer depths
        (though potentially variable number of soil layers)
    @param[out] nMaxSoilLayers Largest number of soil layers across
        simulation domain
    @param[out] nMaxEvapLayers Largest number of soil layers from which
        bare-soil evaporation may extract water across simulation domain
    @param[out] depthsAllSoilLayers Lower soil layer depths [cm] if
        consistent across simulation domain
    @param[in] default_n_layers Default number of soil layer
    @param[in] default_n_evap_lyrs Default number of soil layer used for
        bare-soil evaporation
    @param[in] default_depths Default values of soil layer depths [cm]
    @param[out] LogInfo Holds information on warnings and errors
*/
void SW_DOM_soilProfile(
    Bool *hasConsistentSoilLayerDepths,
    LyrIndex *nMaxSoilLayers,
    LyrIndex *nMaxEvapLayers,
    double depthsAllSoilLayers[],
    LyrIndex default_n_layers,
    LyrIndex default_n_evap_lyrs,
    double default_depths[],
    LOG_INFO *LogInfo
) {

#if defined(SWNETCDF)
    SW_NC_soilProfile(
        hasConsistentSoilLayerDepths,
        nMaxSoilLayers,
        nMaxEvapLayers,
        depthsAllSoilLayers,
        default_n_layers,
        default_n_evap_lyrs,
        default_depths,
        LogInfo
    );

#else

    // Assume default/template values are consistent
    *hasConsistentSoilLayerDepths = swTRUE;
    *nMaxSoilLayers = default_n_layers;
    *nMaxEvapLayers = default_n_evap_lyrs;

    memcpy(
        depthsAllSoilLayers,
        default_depths,
        sizeof(default_depths[0]) * default_n_layers
    );

    (void) LogInfo;

#endif // !SWNETCDF
}

/* =================================================== */
/*             Local Function Definitions              */
/* --------------------------------------------------- */
