
/* =================================================== */
/*                INCLUDES / DEFINES                   */
/* --------------------------------------------------- */
#include "include/SW_Domain.h"      // for SW_DOM_CheckProgress, SW_DOM_Cre...
#include "include/filefuncs.h"      // for LogError, CloseFile, key_to_id
#include "include/generic.h"        // for swTRUE, LOGERROR, swFALSE, Bool
#include "include/myMemory.h"       // for sw_memccpy_custom
#include "include/SW_datastructs.h" // for SW_DOMAIN, LOG_INFO
#include "include/SW_Defines.h"     // for LyrIndex, LARGE_VALUE, TimeInt
#include "include/SW_Files.h"       // for SW_F_deconstruct, SW_F_deepCopy
#include "include/SW_Output.h"      // for ForEachOutKey
#include "include/Times.h"          // for yearto4digit, Time_get_lastdoy_y
#include <stdio.h>                  // for sscanf, FILE
#include <stdlib.h>                 // for strtod, strtol
#include <string.h>                 // for strcmp, memcpy, memset

#if defined(SWNETCDF)
#include "include/SW_netCDF_General.h"
#include "include/SW_netCDF_Input.h"
#include "include/SW_netCDF_Output.h"

#if defined(SWMPI)
#include "include/SW_MPI.h"
#endif
#endif

#if defined(SOILWAT)
#include "include/rands.h" // for RandSeed
#endif


/* =================================================== */
/*                   Local Defines                     */
/* --------------------------------------------------- */

#define NUM_DOM_IN_KEYS 19 // Number of possible keys within `domain.in`

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
void SW_DOM_calc_ncSuid(SW_DOMAIN *SW_Domain, size_t suid, size_t ncSuid[]) {

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
    size_t ncSuid[], // NOLINT(readability-non-const-parameter)
    LOG_INFO *LogInfo
) {
#if defined(SWNETCDF)
    return SW_NCIN_check_progress(progFileID, progVarID, ncSuid, LogInfo);
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
    SW_NCIN_create_progress(SW_Domain, LogInfo);
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
void SW_DOM_construct(size_t rng_seed, SW_DOMAIN *SW_Domain) {

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
        0,
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
        "SpinupSeed",
        "SpatialTolerance",
        "MaxSimErrors"
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
        swTRUE,
        swTRUE,
#if defined(SWMPI)
        swTRUE
#else
        swFALSE
#endif
    };
    Bool hasKeys[NUM_DOM_IN_KEYS] = {swFALSE};

    FILE *f;
    int y;
    int keyID;
    char inbuf[LARGE_VALUE];
    char *MyFileName;
    char key[17]; /* 17 - max key size plus null-terminating character */
    char value[LARGE_VALUE];
    int intRes = 0;
    int scanRes;
    double doubleRes = 0.;
    char *errDim = NULL;

    Bool doDoubleConv;

    MyFileName = SW_Domain->SW_PathInputs.txtInFiles[eDomain];
    f = OpenFile(MyFileName, "r", LogInfo);
    if (LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

    // Set SW_DOMAIN
    while (GetALine(f, inbuf, LARGE_VALUE)) {
        scanRes = sscanf(inbuf, "%16s %s", key, value);

        if (scanRes < 2) {
            LogError(
                LogInfo, LOGERROR, "Invalid key-value pair in %s.", MyFileName
            );
            goto closeFile;
        }

        keyID = key_to_id(key, possibleKeys, NUM_DOM_IN_KEYS);

        set_hasKey(keyID, possibleKeys, hasKeys, LogInfo);
        // set_hasKey() produces never an error, only possibly warnings

        /* Make sure we are not trying to convert a string with no numerical
         * value */
        if (keyID > 0 && keyID <= 18 && keyID != 8) {

            /* Check to see if the line number contains a double or integer
             * value */
            doDoubleConv = (Bool) ((keyID >= 9 && keyID <= 12) || keyID == 17);

            if (doDoubleConv) {
                doubleRes = sw_strtod(value, MyFileName, LogInfo);
            } else {
                intRes = sw_strtoi(value, MyFileName, LogInfo);

                /* Check to see if there are any unexpected negative or
                   zero values */
                if (intRes <= 0) {
                    if (keyID >= 1 && keyID <= 3) { /* > 0 */
                        if (keyID == 1) {
                            errDim = (char *) "X";
                        } else {
                            errDim = (keyID == 2) ? (char *) "Y" : (char *) "S";
                        }
                        LogError(
                            LogInfo,
                            LOGERROR,
                            "%s: Dimension '%s' should be > 0.",
                            MyFileName,
                            errDim
                        );
                    } else if (keyID >= 4 && keyID <= 5) { /* >= 0 */
                        if (intRes < 0) {
                            LogError(
                                LogInfo,
                                LOGERROR,
                                "%s: Negative %s year (%d)",
                                MyFileName,
                                (keyID == 4) ? "start" : "end",
                                intRes
                            );
                        }
                    }
                }
                if (LogInfo->stopRun) {
                    goto closeFile;
                }
            }

            if (LogInfo->stopRun) {
                goto closeFile;
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
                goto closeFile;
            }
            (void) sw_memccpy(
                SW_Domain->DomainType, value, '\0', sizeof SW_Domain->DomainType
            );
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
            SW_Domain->startyr = yearto4digit((TimeInt) intRes);
            break;
        case 5: // End year
            SW_Domain->endyr = yearto4digit((TimeInt) intRes);
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
                goto closeFile;
            }

            (void) sw_memccpy(
                SW_Domain->crs_bbox, value, '\0', sizeof SW_Domain->crs_bbox
            );
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
                LogError(
                    LogInfo,
                    LOGERROR,
                    "%s: Incorrect Mode (%d) for spinup"
                    " Please select \"1\" or \"2\"",
                    MyFileName,
                    y
                );
                goto closeFile;
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
        case 17:
            SW_Domain->spatialTol = doubleRes;

            if (LT(SW_Domain->spatialTol, 0.0)) {
                LogError(LogInfo, LOGERROR, "Spatial tolerance must be >= 0.");
            }
            break;
        case 18:
            SW_Domain->maxSimErrors = intRes;

            if (SW_Domain->maxSimErrors <= 0) {
                LogError(
                    LogInfo, LOGERROR, "Max simulation errors must be > 0."
                );
            }
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


    // Check if all required input was provided
    check_requiredKeys(
        hasKeys, requiredKeys, possibleKeys, NUM_DOM_IN_KEYS, LogInfo
    );
    if (LogInfo->stopRun) {
        goto closeFile;
    }

    if (SW_Domain->endyr < SW_Domain->startyr) {
        LogError(LogInfo, LOGERROR, "%s: Start Year > End Year", MyFileName);
        goto closeFile;
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
        goto closeFile;
    }

    if (GT(SW_Domain->min_y, SW_Domain->max_y)) {
        LogError(LogInfo, LOGERROR, "Domain.in: bbox y-axis min > max.");
        goto closeFile;
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
    }

closeFile: { CloseFile(&f, LogInfo); }
}

/**
@brief Mark completion status of simulation run

@param[in] isFailure Did simulation run fail or succeed?
@param[in] progFileID Identifier of the progress netCDF file
@param[in] progVarID Identifier of the progress variable
@param[in] start A list of calculated start values for when dealing
    with the netCDF library; simply ncSUID if SWMPI is not enabled
@param[in] count A list of count parts used for accessing/writing to
    netCDF files; simply {1, 0} or {1, 1} if SWMPI is not enabled
@param[in,out] LogInfo Holds information on warnings and errors
*/
void SW_DOM_SetProgress(
    Bool isFailure,
    int progFileID,
    int progVarID,
    size_t start[], // NOLINT(readability-non-const-parameter)
    size_t count[], // NOLINT(readability-non-const-parameter)
    LOG_INFO *LogInfo
) {

#if defined(SWNETCDF)
    const signed char mark = (isFailure) ? PRGRSS_FAIL : PRGRSS_DONE;

    SW_NCIN_set_progress(progFileID, progVarID, start, count, &mark, LogInfo);
#else
    (void) isFailure;
    (void) progFileID;
    (void) progVarID;
    (void) start;
    (void) count;
    (void) LogInfo;
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
void SW_DOM_SimSet(SW_DOMAIN *SW_Domain, size_t userSUID, LOG_INFO *LogInfo) {

    Bool progFound;
    size_t *startSimSet = &SW_Domain->startSimSet;
    size_t *endSimSet = &SW_Domain->endSimSet;
    size_t startSuid[2]; // 2 -> [y, x] or [0, s]
    int progFileID = 0;  // Value does not matter if SWNETCDF is not defined
    int progVarID = 0;   // Value does not matter if SWNETCDF is not defined

#if defined(SWNETCDF)
    progFileID = SW_Domain->SW_PathInputs.ncDomFileIDs[vNCprog];
    progVarID = SW_Domain->netCDFInput.ncDomVarIDs[vNCprog];
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
            SW_MSG_ROOT("is identifying the simulation set ...", 0);
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

    SW_F_deepCopy(&source->SW_PathInputs, &dest->SW_PathInputs, LogInfo);
    if (LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

#if defined(SWNETCDF)
    SW_NC_deepCopy(
        &dest->OutDom.netCDFOutput,
        &dest->netCDFInput,
        &source->OutDom.netCDFOutput,
        &source->netCDFInput,
        LogInfo
    );
    if (LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }
#endif
}

void SW_DOM_init_ptrs(SW_DOMAIN *SW_Domain) {

    SW_OUTDOM_init_ptrs(&SW_Domain->OutDom);

    SW_F_init_ptrs(&SW_Domain->SW_PathInputs);

#if defined(SWNETCDF)
    SW_NCIN_init_ptrs(&SW_Domain->netCDFInput);
#endif
}

void SW_DOM_deconstruct(SW_DOMAIN *SW_Domain) {
    int k;
    int i;

    SW_F_deconstruct(
        &SW_Domain->SW_PathInputs,
        SW_Domain->netCDFInput.readInVars,
        SW_Domain->netCDFInput.useIndexFile,
        SW_Domain->SW_Designation.procJob
    );

#if defined(SWNETCDF)

    SW_NC_deconstruct(&SW_Domain->OutDom.netCDFOutput);

    ForEachOutKey(k) {
        SW_NCOUT_dealloc_outputkey_var_info(&SW_Domain->OutDom, k);
    }

    SW_NCIN_deconstruct(&SW_Domain->netCDFInput);

#if defined(SWMPI)
    SW_MPI_deconstruct(SW_Domain);
#endif
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

/**
@brief Identify soil profile information across simulation domain

nc-mode uses the same vertical size for every soil nc-output file, i.e.,
it sets nMaxEvapLayers to nMaxSoilLayers.

@param[in] SW_netCDFIn Constant netCDF input file information
@param[in] SW_PathInputs
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
    bare-soil evaporation (only used in text-mode)
@param[in] default_depths Default values of soil layer depths [cm]
@param[out] LogInfo Holds information on warnings and errors
*/
void SW_DOM_soilProfile(
    SW_NETCDF_IN *SW_netCDFIn,
    SW_PATH_INPUTS *SW_PathInputs,
    Bool hasConsistentSoilLayerDepths,
    LyrIndex *nMaxSoilLayers,
    LyrIndex *nMaxEvapLayers,
    double depthsAllSoilLayers[],
    LyrIndex default_n_layers,
    LyrIndex default_n_evap_lyrs,
    const double default_depths[],
    LOG_INFO *LogInfo
) {

#if defined(SWNETCDF)
    if (SW_netCDFIn->readInVars[eSW_InSoil][0]) {
        SW_NCIN_soilProfile(
            SW_netCDFIn,
            hasConsistentSoilLayerDepths,
            nMaxSoilLayers,
            nMaxEvapLayers,
            depthsAllSoilLayers,
            SW_PathInputs->numSoilVarLyrs,
            default_n_layers,
            default_depths,
            LogInfo
        );
    } else {
        /* nc-mode produces soil output across all soil layers */
        *nMaxEvapLayers = default_n_layers;

#else // !SWNETCDF
    /* text-mode produces soil evaporation output only for evap layers */
    *nMaxEvapLayers = default_n_evap_lyrs;
#endif

        // Assume default/template values are consistent
        *nMaxSoilLayers = default_n_layers;
        memcpy(
            depthsAllSoilLayers,
            default_depths,
            sizeof(default_depths[0]) * default_n_layers
        );

#if defined(SWNETCDF)
    }
#endif // !SWNETCDF

    /* Cast unused variables to void to silence the compiler */
#if defined(SWNETCDF)
    (void) default_n_evap_lyrs;
#else
    (void) SW_netCDFIn;
    (void) SW_PathInputs;
    (void) hasConsistentSoilLayerDepths;
    (void) LogInfo;
#endif
}

/* =================================================== */
/*             Local Function Definitions              */
/* --------------------------------------------------- */
