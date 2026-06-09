/* =================================================== */
/*                INCLUDES / DEFINES                   */
/* --------------------------------------------------- */
#include "include/SW_netCDF_General.h" // for SW_NC_get_dimlen_from_dimid, ...
#include "include/filefuncs.h"         // for LogError, FileExists, CloseFile
#include "include/generic.h"           // for Bool, swFALSE, LOGERROR, swTRUE
#include "include/myMemory.h"          // for Str_Dup, Mem_Malloc
#include "include/SW_datastructs.h"    // for LOG_INFO, SW_NETCDF_OUT, SW_DOMAIN
#include "include/SW_Defines.h"        // for MAX_FILENAMESIZE, OutPeriod
#include "include/SW_Domain.h"         // for SW_DOM_calc_suid_from_subdom
#include "include/SW_Files.h"          // for eNCSysInfo
#include "include/SW_netCDF_Input.h"   // for
#include "include/SW_netCDF_Output.h"  // for outTimes
#include "include/SW_Output.h"         // for ForEachOutKey
#include "include/SW_VegProd.h"        // for VEG_METHOD_DYN_EST
#include "include/SW_Weather.h"        // for wgMKV
#include "include/Times.h"             // for isleapyear, timeStringISO8601
#include <netcdf.h>                    // for NC_NOERR, nc_close, NC_DOUBLE
#include <stdio.h>                     // for size_t, NULL, snprintf, sscanf
#include <stdlib.h>                    // for free, strtod
#include <string.h>                    // for strcmp, strlen, strstr, memcpy

#if defined(SWMPI)
#include "include/SW_MPI.h" // for MPI_Barrier, MPI_Comm, MPI_INF...
#include <netcdf_par.h>     // for nc_open_par
#endif

/* =================================================== */
/*                   Local Defines                     */
/* --------------------------------------------------- */

/**
@brief Given one (sites) or two lists (gridded) of translated suids,
use those values to determine the upper left and bottom right corner
of a rectangular form they form

@param[in] isSimDomDiscrete Is input domain domain discrete (site-based)?
    Otherwise, the input domain is gridded.
@param[in] yIndices A list of size <nSites> holding latitude index
values from an index file
@param[in] xsIndices A list of size <nSites> holding longitude or site index
values from an index file (NULL if domain type is made of sites)
@param[in] nSites Number of sites the subdomain of a process contains,
active or not
@param[in] nActiveSites Number of active sites the subdomain of the process
@param[in] domIndices A list of size nSites to hold the indices of active sites
in a process' subdomain
@param[out] resIndices Resulting indices relative to the translated
suid subdomain
@param[out] keyStart Resulting start values for the current key
@param[out] keyCount Resulting count values for the current key
*/
static void calc_rect_from_indices(
    Bool inDomDiscrete,
    const IntU *yIndices,
    const IntU *xsIndices,
    size_t nSites,
    size_t nActiveSites,
    const size_t *domIndices,
    size_t *resIndices,
    size_t keyStart[],
    size_t keyCount[]
) {
    size_t site;
    size_t resIndex = 0;
    size_t upLeftRow = inDomDiscrete ? 0 : yIndices[0];
    size_t upLeftCol = xsIndices[0];
    size_t botRightRow = inDomDiscrete ? 0 : yIndices[0];
    size_t botRightCol = xsIndices[0];
    size_t rowSize;
    size_t nCols;
    size_t siteIdx;

    size_t rowIndex;
    size_t colIndex;

    for (site = 0; site < nSites; site++) {
        upLeftRow = (!inDomDiscrete && yIndices[site] < upLeftRow) ?
                        yIndices[site] :
                        upLeftRow;
        upLeftCol = (xsIndices[site] < upLeftCol) ? xsIndices[site] : upLeftCol;

        botRightRow = (!inDomDiscrete && yIndices[site] > botRightRow) ?
                          yIndices[site] :
                          botRightRow;
        botRightCol =
            (xsIndices[site] > botRightCol) ? xsIndices[site] : botRightCol;
    }

    nCols = botRightCol - upLeftCol + 1;

    keyCount[0] = inDomDiscrete ? botRightCol - upLeftCol + 1 :
                                  botRightRow - upLeftRow + 1;
    keyCount[1] = inDomDiscrete ? 0 : botRightCol - upLeftCol + 1;

    keyStart[0] = inDomDiscrete ? upLeftCol : upLeftRow;
    keyStart[1] = inDomDiscrete ? 0 : upLeftCol;

    for (site = 0; site < nActiveSites; site++) {
        siteIdx = domIndices[site];
        rowSize = botRightCol - upLeftCol + 1;

        colIndex = xsIndices[siteIdx];

        if (!inDomDiscrete) {
            rowIndex = yIndices[siteIdx];
        }

        resIndex =
            (inDomDiscrete) ? colIndex : (rowIndex - upLeftRow) * rowSize;
        resIndex =
            resIndex + (inDomDiscrete ? 0 : (colIndex - upLeftCol) % nCols);

        resIndices[site] = resIndex;
    }
}

/**
@brief Get translated SUID bounds for the base program domain for
every activated input key

@param[in,out] SW_Domain Struct of type SW_DOMAIN holding constant
    temporal/spatial information for a set of simulation runs
@param[out] LogInfo Holds information on warnings and errors
*/
static void get_tsuid_bnds(SW_DOMAIN *SW_Domain, LOG_INFO *LogInfo) {
    const int indexFile = 0;
    const size_t nActiveSites = SW_Domain->nActiveSuidsProc;
    const size_t ysSize = SW_Domain->domCounts[eSW_InDomain][0];
    const size_t xSize = SW_Domain->domCounts[eSW_InDomain][1];

    Bool inDomDiscrete;
    Bool simDomDiscrete = SW_Domain->isSimDomDiscrete;
    size_t nSites;
    unsigned int *sxIndexVals = NULL;
    unsigned int *yIndexVals = NULL;
    Bool **readInVars = SW_Domain->netCDFInput.readInVars;
    Bool *useIndexFile = SW_Domain->netCDFInput.useIndexFile;
    int inKey;
    int fileID = -1;
    int varID;

    ForEachNCInKey(inKey) {
        if (inKey == eSW_InDomain || !readInVars[inKey][0] ||
            !useIndexFile[inKey]) {

            SW_Domain->domCounts[inKey][0] =
                SW_Domain->domCounts[eSW_InDomain][0];
            SW_Domain->domCounts[inKey][1] =
                SW_Domain->domCounts[eSW_InDomain][1];

            SW_Domain->domStartIndex[inKey][0] =
                SW_Domain->domStartIndex[eSW_InDomain][0];
            SW_Domain->domStartIndex[inKey][1] =
                SW_Domain->domStartIndex[eSW_InDomain][1];

            continue;
        }

        fileID = SW_Domain->SW_PathInputs.openInFileIDs[inKey][indexFile][0];

        inDomDiscrete = SW_Domain->netCDFInput.isInDomDiscrete[inKey];
        nSites = ysSize * (simDomDiscrete ? 1 : xSize);

        sxIndexVals = (unsigned int *) Mem_Malloc(
            sizeof(unsigned int) * nSites, "get_tsuid_bnds", LogInfo
        );
        checkJumpToLabel(LogInfo->stopRun, freeMem);

        if (!inDomDiscrete) {
            yIndexVals = (unsigned int *) Mem_Malloc(
                sizeof(unsigned int) * nSites, "get_tsuid_bnds", LogInfo
            );
            checkJumpToLabel(LogInfo->stopRun, freeMem);
        }

        SW_Domain->actSiteIdx[inKey] = (size_t *) Mem_Malloc(
            sizeof(size_t) * nActiveSites, "get_tsuid_bnds", LogInfo
        );
        checkJumpToLabel(LogInfo->stopRun, freeMem);

        varID = -1;
        SW_NC_get_vals(
            fileID,
            &varID,
            (inDomDiscrete) ? "site_index" : "x_index",
            SW_Domain->domStartIndex[eSW_InDomain],
            SW_Domain->domCounts[eSW_InDomain],
            SW_NC_NO_CONV_TO_DOUBLE,
            sxIndexVals,
            LogInfo
        );
        checkJumpToLabel(LogInfo->stopRun, freeMem);

        if (!inDomDiscrete) {
            varID = -1;
            SW_NC_get_vals(
                fileID,
                &varID,
                "y_index",
                SW_Domain->domStartIndex[eSW_InDomain],
                SW_Domain->domCounts[eSW_InDomain],
                SW_NC_NO_CONV_TO_DOUBLE,
                yIndexVals,
                LogInfo
            );
            checkJumpToLabel(LogInfo->stopRun, freeMem);
        }

        calc_rect_from_indices(
            inDomDiscrete,
            yIndexVals,
            sxIndexVals,
            nSites,
            nActiveSites,
            SW_Domain->actSiteIdx[eSW_InDomain],
            SW_Domain->actSiteIdx[inKey],
            SW_Domain->domStartIndex[inKey],
            SW_Domain->domCounts[inKey]
        );

        if (!isnull(sxIndexVals)) {
            free(sxIndexVals);
            sxIndexVals = NULL;
        }

        if (!isnull(yIndexVals)) {
            free(yIndexVals);
            yIndexVals = NULL;
        }
    }

freeMem:
    if (!isnull(sxIndexVals)) {
        free(sxIndexVals);
    }

    if (!isnull(yIndexVals)) {
        free(yIndexVals);
    }

    if (fileID > -1) {
        nc_close(fileID);
    }
}

/**
@brief Get the number of active sites within the provided domain so we
do not attempt to allocate memory for deactivated sites later in the
program

@param[in,out] SW_Domain Struct of type SW_DOMAIN holding constant
    temporal/spatial information for a set of simulation runs
@param[out] LogInfo Holds information on warnings and errors
*/
static void find_active_sites(SW_DOMAIN *SW_Domain, LOG_INFO *LogInfo) {
    int activeSite = 0;
    int progVarID = SW_Domain->netCDFInput.ncDomVarIDs[vNCprogStatus];
    Bool isSimDomDiscrete = SW_Domain->isSimDomDiscrete;
    size_t numSites = SW_Domain->nSitesInSubDom;
    size_t progIndex;
    int progFileID = SW_Domain->SW_PathInputs.ncDomFileIDs[vNCprogStatus];
    size_t *counts = SW_Domain->domCounts[eSW_InDomain];
    size_t *starts = SW_Domain->domStartIndex[eSW_InDomain];

    size_t *numActiveSites = &SW_Domain->nActiveSuidsProc;
    signed char **progVals = &SW_Domain->netCDFInput.progVals;

    *progVals = (signed char *) Mem_Malloc(
        sizeof(signed char) * numSites, "find_active_sites", LogInfo
    );
    checkReturn(LogInfo->stopRun);

#if defined(SWMPI)
    SW_NC_toggle_par_access(progFileID, progVarID, NC_COLLECTIVE, LogInfo);
    if (SW_MPI_setup_fail(LogInfo->stopRun, MPI_COMM_WORLD)) {
        return;
    }
#endif

    /* Read all progress values - set the parallel access to
       independent so all processes but the root can read 0 values */
    if (nc_get_vara_schar(progFileID, progVarID, starts, counts, *progVals) !=
        NC_NOERR) {

        LogError(
            LogInfo,
            LOGERROR,
            "Could not read all of the progress variable values."
        );
    }
    checkReturn(LogInfo->stopRun);

    /* Go through the entirety of the progress values and keep track of
       how many are ready to be run */
    *numActiveSites = 0;
    for (progIndex = 0; progIndex < numSites; progIndex++) {
        // NOLINTNEXTLINE(clang-analyzer-core.NullDereference)
        *numActiveSites += ((*progVals)[progIndex] == PRGRSS_READY) ? 1 : 0;
    }

    SW_Domain->actSiteIdx[eSW_InDomain] = (size_t *) Mem_Malloc(
        sizeof(size_t) * *numActiveSites, "find_active_sites", LogInfo
    );
    checkReturn(LogInfo->stopRun);

    SW_Domain->globDomSuids = (size_t **) Mem_Malloc(
        sizeof(size_t *) * *numActiveSites, "find_active_sites", LogInfo
    );
    checkReturn(LogInfo->stopRun);

    for (progIndex = 0; progIndex < *numActiveSites; progIndex++) {
        SW_Domain->globDomSuids[progIndex] = NULL;
    }

    for (progIndex = 0; progIndex < *numActiveSites && !LogInfo->stopRun;
         progIndex++) {
        SW_Domain->globDomSuids[progIndex] = (size_t *) Mem_Malloc(
            sizeof(size_t) * NC_DIMS, "find_active_sites", LogInfo
        );
    }
    checkReturn(LogInfo->stopRun);

    for (progIndex = 0; progIndex < numSites; progIndex++) {
        if ((*progVals)[progIndex] == PRGRSS_READY) {
            SW_Domain->actSiteIdx[eSW_InDomain][activeSite] = progIndex;

            SW_DOM_calc_suid_from_subdom(
                isSimDomDiscrete,
                SW_Domain->domStartIndex[eSW_InDomain][0],
                SW_Domain->domStartIndex[eSW_InDomain][1],
                SW_Domain->actSiteIdx[eSW_InDomain][activeSite],
                (isSimDomDiscrete) ? SW_Domain->domCounts[eSW_InDomain][0] :
                                     SW_Domain->domCounts[eSW_InDomain][1],
                SW_Domain->globDomSuids[activeSite]
            );

            activeSite++;
        }
    }

#if defined(SWMPI)
    SW_MPI_Allreduce(
        &SW_Domain->nActiveSuidsProc,
        &SW_Domain->nActiveSuidsTot,
        1,
        SW_MPI_SIZE_T,
        MPI_SUM,
        MPI_COMM_WORLD
    );
#else
    SW_Domain->nActiveSuidsTot = SW_Domain->nActiveSuidsProc;
#endif

    SW_Domain->nErrBeforeFail = (size_t) ceil((double) SW_Domain->nActiveSuidsTot *
                                          (SW_Domain->maxPercSimErrors / 100.));
}

/* =================================================== */
/*             Local Function Definitions              */
/* --------------------------------------------------- */

/**
@brief Get a double value from an attribute

@param[in] ncFileID Identifier of the open netCDF file to test
@param[in] varName Name of the variable to access
@param[in] attName Name of the attribute to access
@param[out] attVal String buffer to hold the resulting value
@param[out] LogInfo Holds information on warnings and errors
*/
static void get_double_att_val(
    int ncFileID,
    const char *varName,
    const char *attName,
    double *attVal,
    LOG_INFO *LogInfo
) {

    int varID = 0;
    int attCallRes;
    SW_NC_get_var_identifier(ncFileID, varName, &varID, LogInfo);
    if (LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

    attCallRes = nc_get_att_double(ncFileID, varID, attName, attVal);
    if (attCallRes == NC_ENOTATT) {
        LogError(
            LogInfo,
            LOGERROR,
            "Missing attribute %s of variable %s.",
            attName,
            varName
        );
    } else if (attCallRes != NC_NOERR) {
        LogError(
            LogInfo,
            LOGERROR,
            "No access to attribute %s of variable %s.",
            attName,
            varName
        );
    }
}

/**
@brief Overwrite specific global attributes into a new file

@param[in] ncFileID Identifier of the open netCDF file to write all information
@param[in] isSimDomDiscrete Is simulation domain discrete (site-based)?
    Otherwise, the simulation domain is gridded.
@param[in] freqAtt Value of a global attribute "frequency"
    * fixed (no time): "fx"
    * has time: "day", "week", "month", or "year"
@param[in] isInputFile Specifies if the file being written to is input
@param[in,out] LogInfo Holds information dealing with logfile output
*/
static void update_netCDF_global_atts(
    const int *ncFileID,
    Bool isSimDomDiscrete,
    const char *freqAtt,
    Bool isInputFile,
    LOG_INFO *LogInfo
) {

    char sourceStr[40]; // 40 - valid size of the SOILWAT2 global `SW2_VERSION`
                        // + "SOILWAT2"
    char creationDateStr[21]; // 21 - valid size to hold a string of format
                              // YYYY-MM-DDTHH:MM:SSZ

    int attNum;
    // Use "featureType" only if isSimDomDiscrete
    const int numGlobAtts = isSimDomDiscrete ? 5 : 4;
    const char *attNames[] = {
        "source", "creation_date", "product", "frequency", "featureType"
    };

    const char *productStr = (isInputFile) ? "model-input" : "model-output";
    const char *featureTypeStr;
    if (isSimDomDiscrete) {
        featureTypeStr = (strcmp(freqAtt, "fx") == 0) ? "point" : "timeSeries";
    } else {
        featureTypeStr = "";
    }

    const char *attVals[] = {
        sourceStr, creationDateStr, productStr, freqAtt, featureTypeStr
    };

    // Fill `sourceStr` and `creationDateStr`
    (void) snprintf(sourceStr, 40, "SOILWAT2%s", SW2_VERSION);
    timeStringISO8601(creationDateStr, sizeof creationDateStr);

    // Write out the necessary global attributes that are listed above
    for (attNum = 0; attNum < numGlobAtts; attNum++) {
        SW_NC_write_string_att(
            attNames[attNum], attVals[attNum], NC_GLOBAL, *ncFileID, LogInfo
        );
        if (LogInfo->stopRun) {
            return; // Exit function prematurely due to error
        }
    }
}

/**
@brief Read in the following user-provided system specifications
    1) File system block size
    2) Allocated memory (RAM) for the program

@param[in,out] SW_Domain Struct of type SW_DOMAIN holding constant
temporal/spatial information for a set of simulation runs
@param[out] LogInfo Holds information on warnings and errors
*/
static void read_system_info(SW_DOMAIN *SW_Domain, LOG_INFO *LogInfo) {
    static const char *possKeys[] = {"FileSystemStripeSize", "AvailableMem"};
    static const Bool reqKeys[] = {swTRUE, swTRUE};
    const int nKeys = 2;

    const char *sysFileName = SW_Domain->SW_PathInputs.txtInFiles[eNCSysInfo];
    const int numPossKeys = 2;

    Bool hasKeys[] = {swFALSE, swFALSE};
    char inbuf[LARGE_VALUE] = "\0";
    char value[LARGE_VALUE] = "\0";
    char key[21] = "\0"; // 20 = "FileSystemStripeSize" + "\0"
    int scanRes;
    int keyID;
    size_t sizetVal;

    FILE *sysInfoFile = NULL;

    sysInfoFile = OpenFile(sysFileName, "r", LogInfo);
    if (LogInfo->stopRun) {
        LogError(
            LogInfo,
            LOGERROR,
            "Could not open the required file %s",
            sysFileName
        );
        return; // Exit function prematurely due to error
    }

    while (GetALine(sysInfoFile, inbuf, LARGE_VALUE)) {
        scanRes = sscanf(inbuf, "%20s %s", key, value);

        if (scanRes < 2) {
            LogError(
                LogInfo,
                LOGERROR,
                "Not enough values for a valid key-value pair in %s.",
                sysFileName
            );
            goto closeFile;
        }

        keyID = key_to_id(key, possKeys, numPossKeys);
        sizetVal = sw_strtosizet(value, sysFileName, LogInfo);
        if (LogInfo->stopRun) {
            goto closeFile;
        }

        if (keyID <= 1) {
            hasKeys[keyID] = swTRUE;
        }

        switch (keyID) {
        case 0:
            SW_Domain->fileSystemStripeSize = sizetVal * KB_TO_BYTES;
            break;
        case 1:
            SW_Domain->availMemory = sizetVal * GB_TO_BYTES;
            break;
        default:
            LogError(
                LogInfo,
                LOGWARN,
                "Ignoring unknown key in %s - %s",
                sysFileName,
                key
            );
            break;
        }
    }

    // Check if all required input was provided
    check_requiredKeys(hasKeys, reqKeys, possKeys, nKeys, LogInfo);

closeFile:
    CloseFile(&sysInfoFile, LogInfo);
}

/**
@brief Helper function to `calc_temporal_chunks()` to calculate the size that
each output variable will take up given a single time step

@param[in] netCDFOut Constant netCDF output file information
@param[in] OutDom Struct of type SW_OUT_DOM that holds output
    information that do not change throughout simulation runs
@param[out] baseSizes Array of size SW_OUTNKEYS x SW_OUTNPERIODS x
    <n vars in key> to hold the base size of a variable; returns allocated
    and filled
@param[out] nP_OUT Total number of bytes to be written out within each
    output key/period given one site
@param[out] totSize Total calculated size of variables
@param[out] LogInfo Holds information on warnings and errors
*/
static void calc_out_var_sizes(
    SW_NETCDF_OUT *netCDFOut,
    SW_OUT_DOM *OutDom,
    size_t nSites,
    size_t *baseSizes[][SW_OUTNPERIODS],
    size_t nP_OUT[][SW_OUTNPERIODS],
    size_t *totSize,
    LOG_INFO *LogInfo
) {
    const int dimIndex = 0;

    int outKey;
    OutPeriod outPd;

    char *outDims;

    IntU dim;
    IntUS var;

    size_t baseSize;
    size_t strLen;

    ForEachOutKey(outKey) {
        ForEachOutPeriod(outPd) { OutDom->nrow_OUT[outKey][outPd] = 1; }
        if (!OutDom->use[outKey]) {
            continue;
        }

        ForEachOutPeriod(outPd) {
            if (!OutDom->use_OutPeriod[outPd]) {
                continue;
            }

            baseSizes[outKey][outPd] = (size_t *) Mem_Calloc(
                OutDom->nvar_OUT[outKey],
                sizeof(size_t),
                "calc_out_var_sizes()",
                LogInfo
            );
            if (LogInfo->stopRun) {
                return;
            }

            for (var = 0; var < OutDom->nvar_OUT[outKey]; var++) {
                if (netCDFOut->reqOutputVars[outKey][var]) {
                    outDims = netCDFOut->outputVarInfo[outKey][var][dimIndex];
                    strLen = strlen(outDims);

                    dim = 0;
                    baseSize = nSites;
                    while (dim < strLen) {
                        switch (outDims[dim]) {
                        case 'T':
                            // Do nothing, assume 1 day, week, month
                            // or year for now
                            break;
                        case 'Z':
                            baseSize *= OutDom->nsl_OUT[outKey][var];
                            break;
                        case 'V':
                            baseSize *= OutDom->npft_OUT[outKey][var];
                            break;
                        default:
                            // Ignore unknown dimension
                            break;
                        }
                        dim++;
                    }

                    baseSizes[outKey][outPd][var] = baseSize * sizeof(double);
                    nP_OUT[outKey][outPd] += baseSize;
                    *totSize += baseSize * sizeof(double);
                }
            }
        }
    }
}

/**
@brief Calculate the maximum amount of time steps within an output key/period

@param[in] SW_Domain Struct of type SW_DOMAIN holding constant
temporal/spatial information for a set of simulation runs
@param[out] maxTimeSteps Maximum timesteps that will be run throughout the
simulation throughout all years
*/
static void calc_max_timestep_sizes(
    SW_DOMAIN *SW_Domain, size_t maxTimeSteps[]
) {
    int outPd;

    TimeInt numDaysInMonth[MAX_MONTHS] = {0};
    TimeInt cumDaysInMonth[MAX_MONTHS] = {0};

    ForEachOutPeriod(outPd) {
        maxTimeSteps[outPd] = SW_NCOUT_calc_timeSize(
            SW_Domain,
            SW_Domain->startyr,
            SW_Domain->endyr + 1,
            outTimes[outPd],
            outPd,
            numDaysInMonth,
            cumDaysInMonth
        );
    }
}

/**
@brief Helper function to `calc_temporal_chunks()` to calculate an optimal
temporal size given the amount of memory we have available and minimum
stripe size (this should be HPC only)

@param[in,out] SW_Domain Struct of type SW_DOMAIN holding constant
    temporal/spatial information for a set of simulation runs;
    returns with updated temporal chunking information
@param[in] maxTimeStepSizes Maximum timesteps that will be run throughout the
simulation throughout all years
@param[in] baseSizes Array of size SW_OUTNKEYS x SW_OUTNPERIODS x
    <n vars in key> to hold the base size of a variable
@param[in] stripeSize User-reported filesystem stripe size
@param[in] outputMem Total amount of memory put towards output-related items
@param[out] minStripe A value specifying if the minimum stripe for
    every variable was hit (swTRUE) or if not every variable can get
    the minimum stripe (swFALSE)
@param[out] totSize Total calculated size of variables
*/
static void calc_temporal_with_stripe(
    SW_DOMAIN *SW_Domain,
    const size_t maxTimeStepSizes[],
    size_t *baseSizes[][SW_OUTNPERIODS],
    size_t stripeSize,
    size_t outputMem,
    Bool *minStripe,
    size_t *totSize
) {
    SW_OUT_DOM *OutDom = &SW_Domain->OutDom;

    int outKey;
    OutPeriod outPd;

    size_t necTimeStepsToMeetStripe;
    size_t pdSize;
    size_t minTimeSize = 0;

    IntUS var;

    ForEachOutKey(outKey) {
        if (!OutDom->use[outKey]) {
            continue;
        }

        ForEachOutPeriod(outPd) {
            pdSize = minTimeSize = 0;
            if (!OutDom->use_OutPeriod[outPd]) {
                continue;
            }

            for (var = 0; var < OutDom->nvar_OUT[outKey]; var++) {
                if (OutDom->netCDFOutput.reqOutputVars[outKey][var]) {
                    necTimeStepsToMeetStripe = (size_t) ceil(
                        ((double) stripeSize) /
                        ((double) baseSizes[outKey][outPd][var])
                    );

                    pdSize += baseSizes[outKey][outPd][var];

                    /*
                        Determine the maximum number of time steps to reach
                        the minimum stripe size so we can try to meet that
                       minimum for all variables. If we determine the minimum,
                       smaller variables can have less temporal chunking to
                       potentially not meet the minimum stripe size whereas
                       larger sizes variables could meet the minimum stripe size
                    */
                    if (minTimeSize == 0 ||
                        necTimeStepsToMeetStripe > minTimeSize) {

                        minTimeSize = necTimeStepsToMeetStripe;
                    }
                }
            }

            for (var = 0; var < OutDom->nvar_OUT[outKey] && *minStripe; var++) {
                *minStripe = (Bool) (*totSize + baseSizes[outKey][outPd][var] *
                                                    minTimeSize <=
                                         outputMem ||
                                     minTimeSize <= maxTimeStepSizes[outPd]);
            }

            OutDom->nrow_OUT[outKey][outPd] = minTimeSize;
            *totSize += (pdSize * minTimeSize);
        }
        if (!*minStripe) {
            break;
        }
    }

    ForEachOutKey(outKey) {
        ForEachOutPeriod(outPd) {
            if (OutDom->nrow_OUT[outKey][outPd] > maxTimeStepSizes[outPd]) {
                OutDom->nrow_OUT[outKey][outPd] = maxTimeStepSizes[outPd];
            }
        }
    }
}

/**
@brief Helper function to `calc_temporal_chunks()` to distribute remaining
estimated memory between each output key/period (for HPC and personal computers)

@param[in,out] SW_DomainStruct of type SW_DOMAIN holding constant
    temporal/spatial information for a set of simulation runs; return
    with updated values for `nrow_OUT`
@param[in] baseSizes Array of size SW_OUTNKEYS x SW_OUTNPERIODS x
    <n vars in key> to hold the base size of a variable;
@param[in] outputMem Total amount of memory put towards output-related items
@param[in] totSize Total amount of estimated memory used so far in the
calculation
*/
static void calc_temporal_general(
    SW_DOMAIN *SW_Domain,
    size_t *baseSizes[][SW_OUTNPERIODS],
    size_t outputMem,
    size_t totSize
) {
    SW_OUT_DOM *OutDom = &SW_Domain->OutDom;

    TimeInt numDaysInMonth[MAX_MONTHS] = {0};
    TimeInt cumDaysInMonth[MAX_MONTHS] = {0};

    int outKey;
    OutPeriod outPd;
    OutPeriod currOutPd;

    IntUS var;
    OutPeriod pd;

    Bool allVarsMaxed;

    size_t outSizes[SW_OUTNKEYS][SW_OUTNPERIODS] = {{0}};
    size_t outSizesOneDay[SW_OUTNKEYS][SW_OUTNPERIODS] = {{0}};
    Bool varsMaxSteps[SW_OUTNKEYS][SW_OUTNPERIODS] = {{swFALSE}};

    TimeInt maxTimeSteps[SW_OUTNPERIODS] = {0};

    double writeOutsInAYear[] = {
        MAX_DAYS, MAX_WEEKS, MAX_MONTHS, 1 /* Year */
    };

    double nWritesAYearCurrPd;
    double nWritesAYearNextPd;

    ForEachOutPeriod(pd) {
        maxTimeSteps[pd] = SW_NCOUT_calc_timeSize(
            SW_Domain,
            SW_Domain->startyr,
            SW_Domain->endyr + 1,
            outTimes[pd],
            pd,
            numDaysInMonth,
            cumDaysInMonth
        );
    }

    while (totSize < outputMem) {
        ForEachOutPeriod(outPd) {
            if (!OutDom->use_OutPeriod[outPd]) {
                continue;
            }

            ForEachOutKey(outKey) {
                if (!OutDom->use[outKey] || OutDom->nvar_OUT[outKey] == 0) {
                    continue;
                }

                outSizes[outKey][outPd] = outSizesOneDay[outKey][outPd] = 0;
                for (var = 0; var < OutDom->nvar_OUT[outKey]; var++) {
                    if (OutDom->netCDFOutput.reqOutputVars[outKey][var]) {
                        outSizesOneDay[outKey][outPd] +=
                            baseSizes[outKey][outPd][var];
                        outSizes[outKey][outPd] +=
                            (baseSizes[outKey][outPd][var] *
                             OutDom->nrow_OUT[outKey][outPd]);
                    }
                }

                varsMaxSteps[outKey][outPd] =
                    (Bool) (OutDom->nrow_OUT[outKey][outPd] ==
                            maxTimeSteps[outPd]);
            }
        }

        ForEachOutKey(outKey) {
            if (!OutDom->use[outKey] || OutDom->nvar_OUT[outKey] == 0) {
                continue;
            }

            currOutPd = eSW_Day;
            ForEachOutPeriod(outPd) {
                if (!OutDom->use_OutPeriod[outPd] ||
                    varsMaxSteps[outKey][outPd]) {

                    if (outPd == currOutPd) {
                        currOutPd++;
                    }

                    continue;
                }

                nWritesAYearCurrPd =
                    writeOutsInAYear[currOutPd] /
                    ((double) OutDom->nrow_OUT[outKey][currOutPd]);
                nWritesAYearNextPd = writeOutsInAYear[outPd] /
                                     ((double) OutDom->nrow_OUT[outKey][outPd]);

                if (GT(nWritesAYearNextPd, nWritesAYearCurrPd)) {
                    currOutPd = outPd;
                }
            }

            if (totSize + outSizesOneDay[outKey][currOutPd] > outputMem) {
                return;
            }

            OutDom->nrow_OUT[outKey][currOutPd]++;
            totSize += outSizesOneDay[outKey][currOutPd];
        }

        allVarsMaxed = swTRUE;
        ForEachOutKey(outKey) {
            if (!OutDom->use[outKey] || OutDom->nvar_OUT[outKey] == 0) {
                continue;
            }

            ForEachOutPeriod(outPd) {
                if (!OutDom->use_OutPeriod[outPd]) {
                    continue;
                }

                allVarsMaxed =
                    (Bool) (allVarsMaxed && OutDom->nrow_OUT[outKey][outPd] ==
                                                maxTimeSteps[outPd]);
            }
        }
        if (allVarsMaxed) {
            return;
        }
    }
}

/**
@brief Helper function to `calc_temporal_chunks()` in SW_netCDF_General.c.
When running the temporal calculations in SWNC and (mainly) SWMPI mode,
all processes need to agree on a temporal size as not all processes will
have the same size spatial chunks also while trying to hit the user-provided
striping size; the steps of this function are as follows

    1) Go through all active output keys/output periods/output variables,
       sum the chunk size across all processes, divide by the size of the
       world and determine number of values in the current key/period is
       needed for meeting the stripe size

    2) Check if the number of values used to match the stripe size is
       a) too much memory use or b) too little memory use

    3) Go through all active output keys/output periods attempting to
       adjust the number of values according to the respective operation:

       a) Too much memory will cause a gradual decrease in the timesteps
          size to get within the available memory calculation

       b) Too little memory use will attempt an increase in the timestep size
          but will attempt to do it by increasing by what would be increments
          of the stipe size, e.g., base stripe size = 1MB, but if memory allows,
          we attempt to increase the timesteps to match 2MB, 3MB, etc. as
          best as possible

    4) Calculate new total(s) of memory through these adjustments

    5) If the new memory after all the increases is too much, we gradually
       decrease each key/period timestep size and will stop once the
       memory is under the available amount

    6) Set final temporal timestep sizes with the maximum being the maximum
       amount of timesteps generated in the entire simulation

       a) If SWMPI mode, agree on temporal sizes one more time

       b) Truncate a temporal dimension chunk size if we are attempting
          to create a chunk size greater than MAX_CHUNK_MEM, then if in
          SWMPI mode, share it across all MPI ranks by taking the average

@param[in] worldSize Total number of processes that the MPI run has created
@param[in] OutDom Struct of type SW_OUT_DOM that holds output
    information that do not change throughout simulation runs
@param[in] netCDFOut Constant netCDF output file information
@param[in] maxTimeStepSizes Maximum timesteps that will be run throughout the
simulation throughout all years
@param[in] nSites Total number of sites in subdomain
@param[in] baseSizes Array of size SW_OUTNKEYS x SW_OUTNPERIODS x
    <n vars in key> to hold the base size of a variable; returns allocated
    and filled
@param[in] availMem Available output memory
@param[in] stripeSize User-reported filesystem stripe size
@param[in,out] procTempChunkSize Array of size SW_OUTNKEYS x SW_OUTNPERIODS
    holding the current process' temporal chunk sizes for each enabled
    output key and period; return with agreed upon sizes between all processess
*/
static void get_temporal_chunk_size(
    int worldSize,
    SW_DOMAIN *SW_Domain,
    SW_NETCDF_OUT *netCDFOut,
    const size_t maxTimeStepSizes[],
    size_t nSites,
    size_t *baseSizes[][SW_OUTNPERIODS],
    size_t availMem,
    size_t stripeSize,
    size_t procTempChunkSize[][SW_OUTNPERIODS]
) {
    const size_t maxChunkMemory = MAX_CHUNK_MEM * KB_TO_BYTES * KB_TO_BYTES;

    SW_OUT_DOM *OutDom = &SW_Domain->OutDom;

    int outKey;
    int nextPd;
    OutPeriod outPd;
    int var;

    Bool allSizes1 = swFALSE;

    int maxedKeyPd;
    int nEnabledKeyPd = 0;

    size_t tempSum;
    size_t totMem = 0;
    size_t minTimestepsForStripe;
    size_t maxSize;
    size_t choseSize;
    size_t baseSize;
    size_t tempChunkSize;
    size_t chunkMem;
    Bool tooMuchMem;

    size_t chosenTempChunkSize[SW_OUTNKEYS][SW_OUTNPERIODS] = {{0}};
    size_t baseTempChunkSize[SW_OUTNKEYS][SW_OUTNPERIODS] = {{0}};

    ForEachOutKey(outKey) {
        if (OutDom->use[outKey]) {
            ForEachOutPeriod(outPd) {
                if (OutDom->use_OutPeriod[outPd]) {
                    nEnabledKeyPd++;
                }
            }
        }
    }

    // Part (1)
    ForEachOutKey(outKey) {
        ForEachOutPeriod(outPd) { chosenTempChunkSize[outKey][outPd] = 1; }

        if (!OutDom->use[outKey]) {
            continue;
        }

        ForEachOutPeriod(outPd) {
            if (!OutDom->use_OutPeriod[outPd]) {
                continue;
            }

#if defined(SWMPI)
            SW_MPI_Allreduce(
                &procTempChunkSize[outKey][outPd],
                &tempSum,
                1,
                SW_MPI_SIZE_T,
                MPI_SUM,
                MPI_COMM_WORLD
            );
#else
            tempSum = procTempChunkSize[outKey][outPd];
#endif

            chosenTempChunkSize[outKey][outPd] =
                baseTempChunkSize[outKey][outPd] = tempSum / worldSize;

            if (stripeSize > 0) {
                minTimestepsForStripe = (size_t
                ) ceil((double) stripeSize * worldSize / (double) tempSum);
                chosenTempChunkSize[outKey][outPd] *= minTimestepsForStripe;
                maxSize = maxTimeStepSizes[outPd];

                if (chosenTempChunkSize[outKey][outPd] > maxSize) {
                    chosenTempChunkSize[outKey][outPd] = maxSize;
                }
            }

            for (var = 0; var < OutDom->nvar_OUT[outKey]; var++) {
                if (netCDFOut->reqOutputVars[outKey][var]) {
                    totMem += baseSizes[outKey][outPd][var] *
                              chosenTempChunkSize[outKey][outPd];
                }
            }
        }
    }

    // Part (2)
    tooMuchMem = (Bool) (totMem > availMem);

    // Part (3)
    while (!allSizes1) {
        totMem = 0;
        allSizes1 = tooMuchMem;
        maxedKeyPd = 0;

        ForEachOutKey(outKey) {
            if (!OutDom->use[outKey]) {
                continue;
            }

            nextPd = eSW_Day;
            ForEachOutPeriod(outPd) {
                if (!OutDom->use_OutPeriod[outPd]) {
                    continue;
                }

                if (!OutDom->use_OutPeriod[nextPd] ||
                    (tooMuchMem && chosenTempChunkSize[outKey][outPd] >
                                       chosenTempChunkSize[outKey][nextPd]) ||
                    (!tooMuchMem && chosenTempChunkSize[outKey][outPd] <
                                        chosenTempChunkSize[outKey][nextPd])) {

                    nextPd = outPd;
                }
            }

            if (chosenTempChunkSize[outKey][nextPd] == 1) {
                continue;
            }

            maxSize = maxTimeStepSizes[nextPd];
            choseSize = chosenTempChunkSize[outKey][nextPd];
            baseSize = baseTempChunkSize[outKey][nextPd];
            if (tooMuchMem) {
                // Part (3a)
                chosenTempChunkSize[outKey][nextPd]--;
            } else if (!tooMuchMem && choseSize + baseSize <= maxSize) {
                // Part (3b)
                chosenTempChunkSize[outKey][nextPd] += baseSize;
            }

            allSizes1 =
                (Bool) (allSizes1 && chosenTempChunkSize[outKey][nextPd] == 1);
        }

        // Part (4)
        ForEachOutKey(outKey) {
            if (!OutDom->use[outKey]) {
                continue;
            }

            ForEachOutPeriod(outPd) {
                if (!OutDom->use_OutPeriod[outPd]) {
                    continue;
                }

                if (chosenTempChunkSize[outKey][outPd] ==
                    maxTimeStepSizes[outPd]) {

                    maxedKeyPd++;
                }

                for (var = 0; var < OutDom->nvar_OUT[outKey]; var++) {
                    if (netCDFOut->reqOutputVars[outKey][var]) {
                        totMem += baseSizes[outKey][outPd][var] *
                                  chosenTempChunkSize[outKey][outPd];
                    }
                }
            }
        }


        if ((maxedKeyPd == nEnabledKeyPd || tooMuchMem) && totMem <= availMem) {
            goto setSizes;
        }

        // Part (5)
        if (!tooMuchMem && totMem > availMem) {
            while (totMem > availMem) {
                ForEachOutKey(outKey) {
                    if (!OutDom->use[outKey]) {
                        continue;
                    }

                    nextPd = eSW_Day;
                    ForEachOutPeriod(outPd) {
                        if (!OutDom->use_OutPeriod[outPd]) {
                            continue;
                        }

                        if (!OutDom->use_OutPeriod[nextPd] ||
                            (chosenTempChunkSize[outKey][outPd] >
                             chosenTempChunkSize[outKey][nextPd])) {

                            nextPd = outPd;
                        }
                    }


                    if (chosenTempChunkSize[outKey][nextPd] > 1) {
                        chosenTempChunkSize[outKey][nextPd]--;

                        for (var = 0; var < OutDom->nvar_OUT[outKey]; var++) {
                            if (netCDFOut->reqOutputVars[outKey][var]) {
                                totMem -= baseSizes[outKey][nextPd][var];
                            }
                        }
                    }
                }
            }
            goto setSizes;
        }
    }

setSizes:
    // Part (6)
    ForEachOutKey(outKey) {
        ForEachOutPeriod(outPd) {
#if defined(SWMPI)
            // Part (6a)
            SW_MPI_Allreduce(
                &chosenTempChunkSize[outKey][outPd],
                &tempSum,
                1,
                SW_MPI_SIZE_T,
                MPI_SUM,
                MPI_COMM_WORLD
            );
            chosenTempChunkSize[outKey][outPd] = tempSum / worldSize;
#endif

            if (chosenTempChunkSize[outKey][outPd] > maxTimeStepSizes[outPd]) {
                tempChunkSize = maxTimeStepSizes[outPd];
            } else {
                tempChunkSize = chosenTempChunkSize[outKey][outPd];
            }

            // Part (6b)
            chunkMem = nSites * tempChunkSize * sizeof(double);

            if (chunkMem > maxChunkMemory) {
                tempChunkSize = maxChunkMemory / (nSites * sizeof(double));
                tempChunkSize = (tempChunkSize == 0) ? 1 : tempChunkSize;
            }

#if defined(SWMPI)
            SW_MPI_Allreduce(
                &tempChunkSize,
                &tempSum,
                1,
                SW_MPI_SIZE_T,
                MPI_SUM,
                MPI_COMM_WORLD
            );
            procTempChunkSize[outKey][outPd] = tempSum / worldSize;
#else
            procTempChunkSize[outKey][outPd] = tempChunkSize;
#endif
        }
    }
}

/**
@brief Calculate an optimal output chunking size given the available memory.
This function will try to calculate it so that at least the stripe size
(on an HPC) or as large of a temporal size as possible (for personal
computers)

This function holds an algorithm attempting to calculate an optimal temporal
chunking for outputs. The main steps of this function are as follows:

    0) Calculate the estimated memory for each output variable

    1) Stripe > 0 (`calc_temporal_with_stripe()`)
        a) This option is mainly for parallel filesystem environments. It
           is not necessary to be turned off for personal computers, but
           will not provide much if any benefit
        b) Taking variable sizes into account,
           attempt to get every active output key/period to a minimum of the
           stripe size (a warning will be thrown if not possible)

    2) General calculation, stripe is 0 (`calc_temporal_general()`)
        a) Find the maximum temporal chunking size for each
           output key/period. It will prioritize the calculation of
           output periods that are output more often
        b) If this function should not be called if stripe > 0 so
           we can attempt to align with the stripe size as much as possible

    3) Process chunk size agreements (mainly MPI but is also for NC/NETCDF)
        a) When in SWMPI mode, all processes may not calculate the same
           temporal chunking size for their respective spatial chunks
        b) Attempt to find a good compromise (average of all calculated
           sizes and adjusting) for all processes to agree

Future development idea(s):
    1) Use a dynamic "nrow_OUT" such that after each write, it updates
       to write a full chunk every time rather than possibly having
       chunk overlap when writing values

@param[in] worldSize Total number of processes that the MPI run has created
(only relevant with SWMPI enabled)
@param[in,out] SW_Domain Struct of type SW_DOMAIN holding constant
    temporal/spatial information for a set of simulation runs;
    returns with updated temporal chunking information
@param[in] outputMem Total amount of memory put towards output-related items
@param[in] availMem Available output memory
@param[out] LogInfo Holds information on warnings and errors
*/
static void calc_temporal_chunks(
    int worldSize,
    SW_DOMAIN *SW_Domain,
    size_t outputMem,
    size_t availMem,
    LOG_INFO *LogInfo
) {
    const size_t nSitesProc = SW_Domain->nSitesInSubDom;
    const size_t stripeSize = SW_Domain->fileSystemStripeSize;

    SW_NETCDF_OUT *netCDFOut = &SW_Domain->OutDom.netCDFOutput;
    SW_OUT_DOM *OutDom = &SW_Domain->OutDom;
    SW_OUT_RUN *OutRun = &SW_Domain->SW_ConstInfo.OutRun;

    size_t maxTimeSteps[SW_OUTNPERIODS] = {0};
    size_t *baseSizes[SW_OUTNKEYS][SW_OUTNPERIODS] = {{NULL}};
    size_t totSize = 0;

    Bool minStripe = swTRUE;

    int outKey;
    OutPeriod outPd;

    calc_max_timestep_sizes(SW_Domain, maxTimeSteps);

    calc_out_var_sizes(
        netCDFOut,
        OutDom,
        nSitesProc,
        baseSizes,
        OutRun->nP_OUT,
        &totSize,
        LogInfo
    );
    if (LogInfo->stopRun) {
        goto freeMem;
    }

    if (stripeSize > 0) {
        calc_temporal_with_stripe(
            SW_Domain,
            maxTimeSteps,
            baseSizes,
            stripeSize,
            outputMem,
            &minStripe,
            &totSize
        );
    } else {
        calc_temporal_general(SW_Domain, baseSizes, outputMem, totSize);
    }

    if (!minStripe) {
        LogError(
            LogInfo,
            LOGWARN,
            "Could not meet minimum striping for output values. "
            "There will be output variables with theoretically less "
            "efficient temporal chunkings."
        );
    }

    get_temporal_chunk_size(
        worldSize,
        SW_Domain,
        netCDFOut,
        maxTimeSteps,
        nSitesProc,
        baseSizes,
        availMem,
        stripeSize,
        OutDom->nrow_OUT
    );

    /*
        Make the temporal chunks for each output key/period the size of
        the number of output rows (timesteps) that we will store/write out

        If an individual chunk is larger than the expected output file size,
        it will be handled when creating the output file itself and will just
        write out a smaller number of values at once
    */
    ForEachOutKey(outKey) {
        ForEachOutPeriod(outPd) {
            OutDom->netCDFOutput.fileTimeChunk[outKey][outPd] =
                OutDom->nrow_OUT[outKey][outPd];
        }
    }

freeMem:
    ForEachOutKey(outKey) {
        ForEachOutPeriod(outPd) {
            if (!isnull(baseSizes[outKey][outPd])) {
                free((void *) baseSizes[outKey][outPd]);
                baseSizes[outKey][outPd] = NULL;
            }
        }
    }
}

/* =================================================== */
/*             Global Function Definitions             */
/* --------------------------------------------------- */

#if defined(SWMPI)
/**
@brief Toggle the parallel access pattern for a variable

@param[in] ncFileID Identifier of the open netCDF file where the variable is
located
@param[in] ncVarID Identifier of the netCDF variable that will have it's
parallel access pattern updated
@param[in] newAccess Updated parallel access pattern to update the variable
to (either NC_INDEPENDENT or NC_COLLECTIVE)
@param[out] LogInfo Holds information on warnings and errors
*/
void SW_NC_toggle_par_access(
    int ncFileID, int ncVarID, int newAccess, LOG_INFO *LogInfo
) {
    if (nc_var_par_access(ncFileID, ncVarID, newAccess) != NC_NOERR) {
        LogError(
            LogInfo,
            LOGERROR,
            "Failed to toggle a variable's parallel access pattern to %s.",
            (newAccess == NC_COLLECTIVE) ? "collective" : "independent"
        );
    }
}
#endif

/**
@brief Gets the type of an attribute

@param[in] ncFileID File identifier of the file to get information from
@param[in] varID Variable identifier to get the attribute value from
@param[in] attName Attribute name to get the type of
@param[out] attType Resulting attribute type gathered
@param[out] LogInfo Holds information on warnings and errors
*/
void SW_NC_get_att_type(
    int ncFileID,
    int varID,
    const char *attName,
    nc_type *attType,
    LOG_INFO *LogInfo
) {
    if (nc_inq_atttype(ncFileID, varID, attName, attType) != NC_NOERR) {
        LogError(
            LogInfo, LOGERROR, "Failed to read type of attribute '%s'.", attName
        );
    }
}

/**
@brief Get a dimension value from a given netCDF file

@param[in] ncFileID Identifier of the open netCDF file to access
@param[in] dimID Identifier of the dimension to access
@param[out] dimVal String buffer to hold the resulting value
@param[out] LogInfo Holds information on warnings and errors
*/
void SW_NC_get_dimlen_from_dimid(
    int ncFileID, int dimID, size_t *dimVal, LOG_INFO *LogInfo
) {

    if (nc_inq_dimlen(ncFileID, dimID, dimVal) != NC_NOERR) {
        LogError(LogInfo, LOGERROR, "Failed to read dimension of %d.", dimID);
    }
}

/**
@brief Get the dimension identifiers of a given variable within
a netCDF file

@param[in] ncFileID Identifier of the open netCDF file to access
@param[in] varID Variable identifier within the given netCDF
(-1 if it is unknown what the ID is before the call to this function)
@param[in] varName Name of the variable to get the dimension sizes of
@param[out] dimIDs Resulting IDs of the dimensions for the given variable
@param[out] nDims Number of dimensions the variable has
@param[out] LogInfo Holds information on warnings and errors
*/
void SW_NC_get_vardimids(
    int ncFileID,
    int varID,
    char *varName,
    int dimIDs[],
    int *nDims,
    LOG_INFO *LogInfo
) {
    if (varID == -1 && nc_inq_varid(ncFileID, varName, &varID) != NC_NOERR) {
        LogError(
            LogInfo,
            LOGERROR,
            "Failed to read dimension identifiers of variable 'site'."
        );
        return;
    }

    if (nc_inq_varndims(ncFileID, varID, nDims) != NC_NOERR) {
        LogError(
            LogInfo,
            LOGERROR,
            "Failed to access number of dimensions for the variable '%s'.",
            varName
        );
        return;
    }

    if (nc_inq_vardimid(ncFileID, varID, dimIDs) != NC_NOERR) {
        LogError(
            LogInfo,
            LOGERROR,
            "Failed to read dimension identifiers of variable 'site'."
        );
    }
}

/**
@brief Get a dimension identifier within a given netCDF

@param[in] ncFileID Identifier of the open netCDF file to access
@param[in] dimName Name of the new dimension
@param[out] dimID Identifier of the dimension
@param[out] LogInfo Holds information on warnings and errors
*/
void SW_NC_get_dim_identifier(
    int ncFileID, const char *dimName, int *dimID, LOG_INFO *LogInfo
) {

    if (nc_inq_dimid(ncFileID, dimName, dimID) != NC_NOERR) {
        LogError(
            LogInfo,
            LOGERROR,
            "Failed to read identifier of dimension %s.",
            dimName
        );
    }
}

/**
@brief Check that the constant content is consistent between
domain.in and a given netCDF file

If ncFileID is negative, then the netCDF fileName will be temporarily
opened for read-access.

@param[in] SW_Domain Struct of type SW_DOMAIN holding constant
    temporal/spatial information for a set of simulation runs
@param[in,out] ncFileID Identifier of the open netCDF file to check
@param[in] fileName Name of netCDF file to test (used for error messages)
@param[in] openInPar Specifyies if the file opened is to be opened for
parallel access
@param[in] openMode Specifies the mode we open a netCDF file perminantly for
the program run
@param[out] LogInfo Holds information on warnings and errors
*/
void SW_NC_check(
    SW_DOMAIN *SW_Domain,
    int *ncFileID,
    const char *fileName,
    Bool openInPar,
    int openMode,
    LOG_INFO *LogInfo
) {

    Bool fileWasClosed = (Bool) (*ncFileID < 0);
    Bool geoIsPrimCRS =
        SW_Domain->OutDom.netCDFOutput.primary_crs_is_geographic;

    /* Get latitude/longitude or x/y names that were read-in from domain
       input file */
    char *readinYName = (geoIsPrimCRS) ?
                            SW_Domain->OutDom.netCDFOutput.geo_YAxisName :
                            SW_Domain->OutDom.netCDFOutput.proj_YAxisName;
    char *readinXName = (geoIsPrimCRS) ?
                            SW_Domain->OutDom.netCDFOutput.geo_XAxisName :
                            SW_Domain->OutDom.netCDFOutput.proj_XAxisName;

    if (fileWasClosed) {
#if defined(SWMPI)
        if (openInPar) {
            SW_NC_open_par(
                fileName, openMode, MPI_COMM_WORLD, ncFileID, LogInfo
            );
        } else {
#endif
            SW_NC_open(fileName, openMode, ncFileID, LogInfo);
            (void) openInPar;
#if defined(SWMPI)
        }
#endif
        if (LogInfo->stopRun) {
            return; /* Exit function prematurely due to error */
        }
    }

    SW_CRS *crs_geogsc = &SW_Domain->OutDom.netCDFOutput.crs_geogsc;
    SW_CRS *crs_projsc = &SW_Domain->OutDom.netCDFOutput.crs_projsc;
    char *siteName = SW_Domain->OutDom.netCDFOutput.siteName;
    char *strAttVal = NULL;
    double doubleAttVal;
    const char *geoCRS = crs_geogsc->crs_name;
    const char *projCRS = crs_projsc->crs_name;
    Bool geoCRSExists = SW_NC_varExists(*ncFileID, geoCRS);
    Bool projCRSExists = SW_NC_varExists(*ncFileID, projCRS);
    const Bool isInDomDiscrete = SW_NC_dimExists(siteName, *ncFileID);
    Bool dimMismatch = swFALSE;
    size_t latDimVal = 0;
    size_t lonDimVal = 0;
    size_t SDimVal = 0;

    const char *strAttsToComp[] = {"long_name", "grid_mapping_name", "crs_wkt"};
    const char *doubleAttsToComp[] = {
        "longitude_of_prime_meridian", "semi_major_axis", "inverse_flattening"
    };

    const char *strProjAttsToComp[] = {"datum", "units"};
    const char *doubleProjAttsToComp[] = {
        "longitude_of_central_meridian",
        "latitude_of_projection_origin",
        "false_easting",
        "false_northing"
    };
    const char *stdParallel = "standard_parallel";
    const double stdParVals[] = {
        crs_projsc->standard_parallel[0], crs_projsc->standard_parallel[1]
    };

    const char *geoStrAttVals[] = {
        crs_geogsc->long_name,
        crs_geogsc->grid_mapping_name,
        crs_geogsc->crs_wkt
    };
    const double geoDoubleAttVals[] = {
        crs_geogsc->longitude_of_prime_meridian,
        crs_geogsc->semi_major_axis,
        crs_geogsc->inverse_flattening
    };
    const char *projStrAttVals[] = {
        crs_projsc->long_name,
        crs_projsc->grid_mapping_name,
        crs_projsc->crs_wkt
    };
    const double projDoubleAttVals[] = {
        crs_projsc->longitude_of_prime_meridian,
        crs_projsc->semi_major_axis,
        crs_projsc->inverse_flattening
    };

    const char *strProjAttVals[] = {
        SW_Domain->OutDom.netCDFOutput.crs_projsc.datum,
        SW_Domain->OutDom.netCDFOutput.crs_projsc.units
    };
    const double doubleProjAttVals[] = {
        crs_projsc->longitude_of_central_meridian,
        crs_projsc->latitude_of_projection_origin,
        crs_projsc->false_easting,
        crs_projsc->false_northing,
    };

    const int numNormAtts = 3;
    const int numProjStrAtts = 2;
    const int numProjDoubleAtts = 4;
    double projStdParallel[2]; // Compare to standard_parallel is projected CRS
    int attNum;

    const char *attFailMsg =
        "The attribute '%s' of the variable '%s' "
        "within the file %s does not match the one in the domain input "
        "file. Please make sure these match.";

    /*
       Make sure the domain types are consistent
    */
    if (SW_Domain->isSimDomDiscrete != isInDomDiscrete) {
        LogError(
            LogInfo,
            LOGERROR,
            "The existing file ('%s') has a domain type '%s'; "
            "however, the current simulation uses a domain type '%s'. "
            "Please make sure these match.",
            fileName,
            isInDomDiscrete ? "s" : "xy",
            SW_Domain->isSimDomDiscrete ? "s" : "xy"
        );
        return; // Exit function prematurely due to error
    }

    /*
       Make sure the dimensions of the netCDF file is consistent with the
       domain input file
    */
    if (isInDomDiscrete) {
        SW_NC_get_dimlen_from_dimname(*ncFileID, siteName, &SDimVal, LogInfo);
        if (LogInfo->stopRun) {
            return; // Exit function prematurely due to error
        }

        dimMismatch = (Bool) (SDimVal != SW_Domain->nDimS);
    } else {
        SW_NC_get_dimlen_from_dimname(
            *ncFileID, readinYName, &latDimVal, LogInfo
        );
        if (LogInfo->stopRun) {
            return; // Exit function prematurely due to error
        }
        SW_NC_get_dimlen_from_dimname(
            *ncFileID, readinXName, &lonDimVal, LogInfo
        );
        if (LogInfo->stopRun) {
            return; // Exit function prematurely due to error
        }

        dimMismatch = (Bool) (latDimVal != SW_Domain->nDimY ||
                              lonDimVal != SW_Domain->nDimX);
    }

    if (dimMismatch) {
        LogError(
            LogInfo,
            LOGERROR,
            "The size of the dimensions in %s do "
            "not match the domain input file's. Please make sure "
            "these match.",
            fileName
        );
        return; // Exit function prematurely due to error
    }

    /*
       Make sure the geographic CRS information is consistent with the
       domain input file - both string and double values
    */
    if (geoCRSExists) {
        for (attNum = 0; attNum < numNormAtts; attNum++) {
            SW_NC_get_str_att_val(
                *ncFileID, geoCRS, strAttsToComp[attNum], &strAttVal, LogInfo
            );
            if (LogInfo->stopRun) {
                goto wrapUp; // Exit function prematurely due to error
            }

            // `isnull()` should not be necessary here, it is only to
            // silence Clang Tidy
            if (isnull(strAttVal) ||
                strcmp(geoStrAttVals[attNum], strAttVal) != 0) {
                LogError(
                    LogInfo,
                    LOGERROR,
                    attFailMsg,
                    strAttsToComp[attNum],
                    geoCRS,
                    fileName
                );
                goto wrapUp; // Exit function prematurely due to error
            }
        }

        for (attNum = 0; attNum < numNormAtts; attNum++) {
            get_double_att_val(
                *ncFileID,
                geoCRS,
                doubleAttsToComp[attNum],
                &doubleAttVal,
                LogInfo
            );
            if (LogInfo->stopRun) {
                goto wrapUp; // Exit function prematurely due to error
            }

            if (doubleAttVal != geoDoubleAttVals[attNum]) {
                LogError(
                    LogInfo,
                    LOGERROR,
                    attFailMsg,
                    doubleAttsToComp[attNum],
                    geoCRS,
                    fileName
                );
                goto wrapUp; // Exit function prematurely due to error
            }
        }
    } else {
        LogError(
            LogInfo,
            LOGERROR,
            "A geographic CRS was not found in %s. "
            "Please make sure one is provided.",
            fileName
        );
        goto wrapUp; // Exit function prematurely due to error
    }

    /*
       Test all projected CRS attributes - both string and double values -
       if applicable
    */
    if (!geoIsPrimCRS && projCRSExists) {
        // Normal attributes (same tested for in crs_geogsc)
        for (attNum = 0; attNum < numNormAtts; attNum++) {
            SW_NC_get_str_att_val(
                *ncFileID, projCRS, strAttsToComp[attNum], &strAttVal, LogInfo
            );
            if (LogInfo->stopRun) {
                goto wrapUp; // Exit function prematurely due to error
            }

            if (strcmp(projStrAttVals[attNum], strAttVal) != 0) {
                LogError(
                    LogInfo,
                    LOGERROR,
                    attFailMsg,
                    strAttsToComp[attNum],
                    projCRS,
                    fileName
                );
                goto wrapUp; // Exit function prematurely due to error
            }
        }

        for (attNum = 0; attNum < numNormAtts; attNum++) {
            get_double_att_val(
                *ncFileID,
                projCRS,
                doubleAttsToComp[attNum],
                &doubleAttVal,
                LogInfo
            );
            if (LogInfo->stopRun) {
                goto wrapUp; // Exit function prematurely due to error
            }

            if (doubleAttVal != projDoubleAttVals[attNum]) {
                LogError(
                    LogInfo,
                    LOGERROR,
                    attFailMsg,
                    doubleAttsToComp[attNum],
                    projCRS,
                    fileName
                );
                goto wrapUp; // Exit function prematurely due to error
            }
        }

        // Projected CRS-only attributes
        for (attNum = 0; attNum < numProjStrAtts; attNum++) {
            SW_NC_get_str_att_val(
                *ncFileID,
                projCRS,
                strProjAttsToComp[attNum],
                &strAttVal,
                LogInfo
            );
            if (LogInfo->stopRun) {
                goto wrapUp; // Exit function prematurely due to error
            }

            if (strcmp(strAttVal, strProjAttVals[attNum]) != 0) {
                LogError(
                    LogInfo,
                    LOGERROR,
                    attFailMsg,
                    strProjAttsToComp[attNum],
                    projCRS,
                    fileName
                );
                goto wrapUp; // Exit function prematurely due to error
            }
        }

        for (attNum = 0; attNum < numProjDoubleAtts; attNum++) {
            get_double_att_val(
                *ncFileID,
                projCRS,
                doubleProjAttsToComp[attNum],
                &doubleAttVal,
                LogInfo
            );
            if (LogInfo->stopRun) {
                goto wrapUp; // Exit function prematurely due to error
            }

            if (doubleAttVal != doubleProjAttVals[attNum]) {
                LogError(
                    LogInfo,
                    LOGERROR,
                    attFailMsg,
                    doubleProjAttsToComp[attNum],
                    projCRS,
                    fileName
                );
                goto wrapUp; // Exit function prematurely due to error
            }
        }

        // Test for standard_parallel
        get_double_att_val(
            *ncFileID, projCRS, stdParallel, projStdParallel, LogInfo
        );
        if (LogInfo->stopRun) {
            goto wrapUp; // Exit function prematurely due to error
        }

        if (projStdParallel[0] != stdParVals[0] ||
            projStdParallel[1] != stdParVals[1]) {

            LogError(
                LogInfo, LOGERROR, attFailMsg, stdParallel, projCRS, fileName
            );
        }
    }


wrapUp:
    if (!isnull(strAttVal)) {
        free((void *) strAttVal);
    }
}

void SW_NC_get_single_val(
    int ncFileID,
    int *varID,
    const char *varName,
    const size_t index[],
    void *value,
    LOG_INFO *LogInfo
) {

    if (*varID < 0 && varName != NULL) {
        SW_NC_get_var_identifier(ncFileID, varName, varID, LogInfo);
        if (LogInfo->stopRun) {
            return; // Exit function prematurely due to error
        }
    }

    if (nc_get_var1(ncFileID, *varID, index, value) != NC_NOERR) {
        LogError(
            LogInfo, LOGERROR, "Failed to read value of variable %s.", varName
        );
    }
}

void SW_NC_write_att(
    const char *attName,
    void *attVal,
    int varID,
    int ncFileID,
    size_t numVals,
    int ncType,
    LOG_INFO *LogInfo
) {
    if (nc_put_att(
            ncFileID, varID, attName, (nc_type) ncType, numVals, attVal
        ) != NC_NOERR) {
        LogError(LogInfo, LOGERROR, "Failed to write attribute %s.", attName);
    }
}

/**
@brief Write a global attribute (text) to a netCDF file

@param[in] attName Name of the attribute to create
@param[in] attStr Attribute string to write out
@param[in] varID Identifier of the variable to add the attribute to
    (Note: NC_GLOBAL is acceptable and is a global attribute of the netCDF
file)
@param[in] ncFileID Identifier of the open netCDF file to write the
attribute
@param[out] LogInfo Holds information on warnings and errors
*/
void SW_NC_write_string_att(
    const char *attName,
    const char *attStr,
    int varID,
    int ncFileID,
    LOG_INFO *LogInfo
) {
    if (nc_put_att_text(ncFileID, varID, attName, strlen(attStr), attStr) !=
        NC_NOERR) {
        LogError(
            LogInfo, LOGERROR, "Failed to write global attribute %s", attName
        );
    }
}

/**
@brief Checks to see if a given netCDF has a specific dimension

@param[in] targetDim Dimension name to test for
@param[in] ncFileID Identifier of the open netCDF file to test

@return Whether or not the given dimension name exists in the netCDF file
*/
Bool SW_NC_dimExists(const char *targetDim, int ncFileID) {

    int dimID; // Not used

    // Attempt to get the dimension identifier
    int inquireRes = nc_inq_dimid(ncFileID, targetDim, &dimID);

    return (Bool) (inquireRes == NC_NOERR);
}

/**
@brief Check if a variable exists within a given netCDF and does not
throw an error if anything goes wrong

@param[in] ncFileID Identifier of the open netCDF file to test
@param[in] varName Name of the variable to test for

@return Whether or not the given variable name exists in the netCDF file
*/
Bool SW_NC_varExists(int ncFileID, const char *varName) {
    int varID;

    int inquireRes = nc_inq_varid(ncFileID, varName, &varID);

    return (Bool) (inquireRes != NC_ENOTVAR);
}

/*
@brief Write a dummy value to a newly created netCDF file so that
the first write does not occur during the first simulation run;
this function should only be called when there is no deflate
activated

@param[in] ncFileID Identifier of the open netCDF file to write to
@param[in] varType Type of the first variable being written to the file
@param[in] varID Identifier of the variable to write to
*/
static void writeDummyVal(int ncFileID, int varType, int varID) {
    size_t start[MAX_NUM_DIMS] = {0};
    size_t count[MAX_NUM_DIMS] = {1, 1, 1, 1, 1};
    double doubleFill[] = {NC_FILL_DOUBLE};
    unsigned char byteFill[] = {(unsigned char) NC_FILL_BYTE};

    switch (varType) {
    case NC_DOUBLE:
        nc_put_vara_double(ncFileID, varID, start, count, &doubleFill[0]);
        break;
    case NC_BYTE:
        nc_put_vara_ubyte(ncFileID, varID, start, count, &byteFill[0]);
        break;
    default:
        /* No other types should be expected */
        break;
    }
}

/**
@brief Write values to the given netCDF file of any type

@param[in,out] varID Identifier corresponding to varName.
    If negative, then will be queried using varName and returned.
@param[in] ncFileID Identifier of the open netCDF file
@param[in] varName Name of the variable to write;
    not used, unless varID is negative.
@param[in] values Value(s) to write out
@param[in] start Starting indices to write to for the given variable
@param[in] count Number of values to write out
@param[in] type Intended variable type that is being written to
@param[out] LogInfo Holds information on warnings and errors
*/
void SW_NC_write_vals(
    int *varID,
    int ncFileID,
    const char *varName,
    void *values,
    size_t start[],
    size_t count[],
    const char *type,
    LOG_INFO *LogInfo
) {

    if (*varID < 0 && varName != NULL) {
        SW_NC_get_var_identifier(ncFileID, varName, varID, LogInfo);
        if (LogInfo->stopRun) {
            return; // Exit function prematurely due to error
        }
    }

    if (nc_put_vara(ncFileID, *varID, start, count, values) != NC_NOERR) {
        LogError(
            LogInfo,
            LOGERROR,
            "Failed to write values of type %s to variable %s.",
            type,
            (varName != NULL) ? varName : ""
        );
    }
}

/**
@brief Get a string value from an attribute

@param[in] ncFileID Identifier of the open netCDF file to test
@param[in] varName Name of the variable to access
@param[in] attName Name of the attribute to access
@param[out] strVal String buffer to hold the resulting value
@param[out] LogInfo Holds information on warnings and errors
*/
void SW_NC_get_str_att_val(
    int ncFileID,
    const char *varName,
    const char *attName,
    char **strVal,
    LOG_INFO *LogInfo
) {
    const int firstStr = 0;
    int varID = 0;
    nc_type attType = NC_CHAR;
    int attCallRes;
    int attLenCallRes;
    size_t attLen = 0;
    size_t strLen;

    size_t strIndex;
    char **strAtts = NULL;

    SW_NC_get_var_identifier(ncFileID, varName, &varID, LogInfo);
    if (LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

    SW_NC_get_att_type(ncFileID, varID, attName, &attType, LogInfo);
    if (LogInfo->stopRun) {
        return;
    }

    attLenCallRes = nc_inq_attlen(ncFileID, varID, attName, &attLen);
    if (attLenCallRes == NC_ENOTATT) {
        LogError(
            LogInfo,
            LOGERROR,
            "No attribute %s of variable %s.",
            attName,
            varName
        );
    } else if (attLenCallRes != NC_NOERR) {
        LogError(
            LogInfo, LOGERROR, "Failed to read length of attribute %s.", attName
        );
    }
    if (LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

    if (!isnull(*strVal)) {
        free((void *) *strVal);
        *strVal = NULL;
    }

    if (attType == NC_CHAR) {
        *strVal = (char *) Mem_Malloc(
            sizeof(char) * attLen + 1, "SW_NC_get_str_att_val", LogInfo
        );
    } else if (attType == NC_STRING) {
        strAtts = (char **) Mem_Malloc(
            sizeof(char *) * attLen, "SW_NC_get_str_att_val", LogInfo
        );
    } else if (attType != NC_CHAR) {
        LogError(
            LogInfo,
            LOGERROR,
            "Type of attribute '%s' must be characters or a string.",
            attName
        );
    }
    if (LogInfo->stopRun) {
        return;
    }

    if (attType == NC_CHAR) {
        attCallRes = nc_get_att_text(ncFileID, varID, attName, *strVal);

        (*strVal)[attLen] = '\0';
    } else {
        attCallRes = nc_get_att_string(ncFileID, varID, attName, strAtts);
    }
    if (attCallRes != NC_NOERR) {
        LogError(
            LogInfo,
            LOGERROR,
            "Failed to read attribute %s of variable %s.",
            attName,
            varName
        );
    }

    if (!isnull(strAtts)) {
        strLen = strlen(strAtts[firstStr]) + 1;

        *strVal = (char *) Mem_Malloc(
            sizeof(char) * strLen, "SW_NC_get_str_att_val", LogInfo
        );

        for (strIndex = 0; strIndex < strLen; strIndex++) {
            (*strVal)[strIndex] = strAtts[firstStr][strIndex];
        }

        nc_free_string(attLen, strAtts);

        free((void *) strAtts);
    }
}

/**
@brief Create a dimension within a netCDF file

@param[in] dimName Name of the new dimension
@param[in] size Value/size of the dimension
@param[in] ncFileID Domain netCDF file ID
@param[in,out] dimID Dimension ID
@param[out] LogInfo Holds information on warnings and errors
*/
void SW_NC_create_netCDF_dim(
    const char *dimName,
    size_t size,
    const int *ncFileID,
    int *dimID,
    LOG_INFO *LogInfo
) {

    if (nc_def_dim(*ncFileID, dimName, size, dimID) != NC_NOERR) {
        LogError(
            LogInfo, LOGERROR, "Failed to create dimension '%s'.", dimName
        );
    }
}

/**
@brief Get a variable identifier within a given netCDF

@param[in] ncFileID Identifier of the open netCDF file to access
@param[in] varName Name of the new variable
@param[out] varID Identifier of the variable
@param[out] LogInfo Holds information on warnings and errors
*/
void SW_NC_get_var_identifier(
    int ncFileID, const char *varName, int *varID, LOG_INFO *LogInfo
) {

    int callRes = nc_inq_varid(ncFileID, varName, varID);

    if (callRes == NC_ENOTVAR) {
        LogError(LogInfo, LOGERROR, "Could not find variable %s.", varName);
    }
}

/**
@brief Get a dimension value from a given netCDF file

@param[in] ncFileID Identifier of the open netCDF file to access
@param[in] dimName Name of the dimension to access
@param[out] dimVal String buffer to hold the resulting value
@param[out] LogInfo Holds information on warnings and errors
*/
void SW_NC_get_dimlen_from_dimname(
    int ncFileID, const char *dimName, size_t *dimVal, LOG_INFO *LogInfo
) {

    int dimID = 0;

    SW_NC_get_dim_identifier(ncFileID, dimName, &dimID, LogInfo);
    if (LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

    SW_NC_get_dimlen_from_dimid(ncFileID, dimID, dimVal, LogInfo);
}

/**
@brief Create a new variable by calculating the dimensions
and writing attributes

@param[in] ncFileID Identifier of the netCDF file
@param[in] isSimDomDiscrete Is simulation domain discrete (site-based)?
    Otherwise, the simulation domain is gridded.
@param[in] newVarType Type of the variable to create
@param[in] timeSize Size of "time" dimension
@param[in] vertSize Size of "vertical" dimension
@param[in] pftSize Size of "pft" dimension
@param[in] latSChunkSize Size of the latitude or site dimension chunk
@param[in] lonChunkSize Size of the longitude dimension chunk
@param[in] timeChunkSize Size of the temporal dimension chunk
@param[in] varName Name of variable to write
@param[in] attNames Attribute names that the new variable will contain
@param[in] attVals Attribute values that the new variable will contain
@param[in] numAtts Number of attributes being sent in
@param[in] hasConsistentSoilLayerDepths Flag indicating if all simulation
    run within domain have identical soil layer depths
    (though potentially variable number of soil layers)
@param[in] posVerticalInBnds Position of vertical coordinate values
    relative to bounds
@param[in] lyrDepths Depths of soil layers (cm)
@param[in] posTimeInBnds Position of time coordinate values relative to
bounds
@param[in,out] startTime Start number of days when dealing with
    years between netCDF files (returns updated value)
@param[in] baseCalendarYear First year of the entire simulation
@param[in] startYr Start year of the simulation
@param[in] pd Current output netCDF period
@param[in] deflateLevel Level of deflation that will be used for the created
variable
@param[in] yName User-provided latitude/y name
@param[in] xName User-provided longitude/x name
@param[in] coordAttIndex Specifies the coordinate attribute location
within the provided `attNames`/`attVals` (if there isn't an attribute
of this name, it's value should be -1)
@param[in] siteName User-provided site dimension/variable "site" name
@param[in] useDefaultChunking A flag specifying if, when creating the
variable, to use the default chunk sizes or program-provided sizes
@param[in] addFillValueAttribute Create a `"_FillValue"` attribute of
type and default value based on \p newVarType.
@param[in,out] LogInfo Holds information dealing with logfile output
*/
void SW_NC_create_full_var(
    int *ncFileID,
    Bool isSimDomDiscrete,
    int newVarType,
    size_t timeSize,
    size_t vertSize,
    size_t pftSize,
    size_t latSChunkSize,
    size_t lonChunkSize,
    size_t timeChunkSize,
    const char *varName,
    const char *attNames[],
    const char *attVals[],
    unsigned int numAtts,
    Bool hasConsistentSoilLayerDepths,
    int posVerticalInBnds,
    double lyrDepths[],
    int posTimeInBnds,
    double *startTime,
    unsigned int baseCalendarYear,
    unsigned int startYr,
    OutPeriod pd,
    int deflateLevel,
    const char *yName,
    const char *xName,
    const char *siteName,
    const int coordAttIndex,
    Bool useDefaultChunking,
    Bool addFillValueAttribute,
    LOG_INFO *LogInfo
) {

    int dimArrSize = 0;
    int varID = 0;
    unsigned int index;
    int dimIDs[MAX_NUM_DIMS];
    unsigned int numConstDims = (isSimDomDiscrete) ? 1 : 2;
    const int timeIdxInChunkArr = 0;
    const char *thirdDim = (isSimDomDiscrete) ? siteName : yName;
    const char *constDimNames[] = {thirdDim, xName};
    const char *timeVertVegDimNames[] = {"time", "vertical", "pft"};
    const char *timeVertVegVarNames[] = {"time", "vertical", "pft_label"};
    char *dimName;
    size_t timeVertVegVals[] = {timeSize, vertSize, pftSize};
    unsigned int numTimeVertVegVals = 3;
    size_t varVal = 0;
    size_t chunkSizes[MAX_NUM_DIMS] = {1, 1, 1, 1, 1};
    char coordValBuf[MAX_FILENAMESIZE] = "";
    char *writePtr = coordValBuf;
    char *endWritePtr = writePtr + sizeof coordValBuf - 1;
    size_t writeSize = MAX_FILENAMESIZE;
    char finalCoordVal[MAX_FILENAMESIZE];
    Bool fullBuffer = swFALSE;
    void *fillValue = NULL;
    char byteFillVal = NC_FILL_BYTE;
    double doubleFillVal = NC_FILL_DOUBLE;
    int chunkIndex = 0;

    for (index = 0; index < numTimeVertVegVals; index++) {
        dimName = (char *) timeVertVegDimNames[index];
        varVal = timeVertVegVals[index];
        if (varVal > 0) {
            if (!SW_NC_dimExists(dimName, *ncFileID)) {
                SW_NCOUT_create_output_dimVar(
                    dimName,
                    varVal,
                    *ncFileID,
                    &dimIDs[dimArrSize],
                    hasConsistentSoilLayerDepths,
                    posVerticalInBnds,
                    lyrDepths,
                    posTimeInBnds,
                    startTime,
                    baseCalendarYear,
                    startYr,
                    pd,
                    deflateLevel,
                    LogInfo
                );
            } else {
                SW_NC_get_dim_identifier(
                    *ncFileID, dimName, &dimIDs[dimArrSize], LogInfo
                );
            }
            if (LogInfo->stopRun) {
                return; // Exit function prematurely due to error
            }

            /* Update coordinates attribute */
            fullBuffer = sw_memccpy_inc(
                (void **) &writePtr, endWritePtr, (void *) " ", '\0', &writeSize
            );
            if (fullBuffer) {
                goto reportFullBuffer;
            }

            fullBuffer = sw_memccpy_inc(
                (void **) &writePtr,
                endWritePtr,
                (void *) timeVertVegVarNames[index],
                '\0',
                &writeSize
            );
            if (fullBuffer) {
                goto reportFullBuffer;
            }

            dimArrSize++;
        }
    }

    for (index = 0; index < numConstDims; index++) {
        SW_NC_get_dim_identifier(
            *ncFileID, constDimNames[index], &dimIDs[dimArrSize], LogInfo
        );
        if (LogInfo->stopRun) {
            return; // Exit function prematurely due to error
        }

        dimArrSize++;
    }

    if (coordAttIndex > -1 && !isnull(attVals[coordAttIndex])) {
        (void) snprintf(
            finalCoordVal,
            MAX_FILENAMESIZE,
            "%s%s",
            attVals[coordAttIndex],
            coordValBuf
        );

        free((void *) ((char *) attVals[coordAttIndex]));
        attVals[coordAttIndex] = Str_Dup(finalCoordVal, LogInfo);
        if (LogInfo->stopRun) {
            return;
        }
    }

    for (index = 0; index < MAX_NUM_DIMS - numConstDims; index++) {
        if (index < numTimeVertVegVals) {
            varVal = timeVertVegVals[index];

            if (varVal > 0) {
                if (index == timeIdxInChunkArr && timeChunkSize <= timeSize) {
                    varVal = timeChunkSize;
                }

                chunkSizes[chunkIndex] = varVal;

                chunkIndex++;
            }
        }
    }
    chunkSizes[chunkIndex] = latSChunkSize;
    chunkSizes[chunkIndex + 1] = (isSimDomDiscrete) ? 1 : lonChunkSize;

    SW_NC_create_netCDF_var(
        &varID,
        varName,
        dimIDs,
        ncFileID,
        newVarType,
        dimArrSize,
        (useDefaultChunking) ? NULL : chunkSizes,
        deflateLevel,
        LogInfo
    );
    if (LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

    for (index = 0; index < numAtts; index++) {
        if (attVals[index] != NULL && strlen(attVals[index]) > 0) {
            SW_NC_write_string_att(
                attNames[index], attVals[index], varID, *ncFileID, LogInfo
            );
            if (LogInfo->stopRun) {
                return; // Exit function prematurely due to error
            }
        }
    }

    if (addFillValueAttribute) {
        switch (newVarType) {
        case NC_BYTE:
            fillValue = (void *) &byteFillVal;
            break;
        case NC_DOUBLE:
            fillValue = (void *) &doubleFillVal;
            break;
        default:
            LogError(
                LogInfo,
                LOGERROR,
                "Selected type of _FillValue '%d' is not implemented.",
                newVarType
            );
            break;
        }
        SW_NC_write_att(
            "_FillValue", fillValue, varID, *ncFileID, 1, newVarType, LogInfo
        );
        if (LogInfo->stopRun) {
            return; // Exit function prematurely due to error
        }
    }

    if (deflateLevel == 0) {
        /* Write a dummy value so that the first write is not in the sim
           loop; otherwise, the first simulation loop takes an order of
           magnitude longer than following simulations */
        writeDummyVal(*ncFileID, newVarType, varID);
    }

reportFullBuffer:
    if (fullBuffer) {
        reportFullBuffer(LOGERROR, LogInfo);
    }
}

/**
@brief Copy domain netCDF as a template

@param[in] isSimDomDiscrete Is simulation domain discrete (site-based)?
    Otherwise, the simulation domain is gridded.
@param[in] domFile Name of the domain netCDF
@param[in] fileName Name of the netCDF file to create
@param[in] newFileID Identifier of the netCDF file to create
@param[in] isInput Specifies if the created file will be input or output
@param[in] freq Value of the global attribute "frequency"
@param[in] parOpen Specifies if the file is to be opened for parallel
access or not, if SWMPI is not enabled, this argument is not used
@param[out] LogInfo  Holds information dealing with logfile output
*/
void SW_NC_create_template(
    Bool isSimDomDiscrete,
    const char *domFile,
    const char *fileName,
    int *newFileID,
    Bool isInput,
    const char *freq,
    Bool parOpen,
    LOG_INFO *LogInfo
) {


    CopyFile(domFile, fileName, LogInfo);
    if (LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

#if defined(SWMPI)
    if (parOpen) {
        SW_NC_open_par(fileName, NC_WRITE, MPI_COMM_WORLD, newFileID, LogInfo);
    } else {
#endif

        SW_NC_open(fileName, NC_WRITE, newFileID, LogInfo);

        (void) parOpen;
#if defined(SWMPI)
    }
#endif

    if (LogInfo->stopRun) {
        return; /* Exit function prematurely due to error */
    }

    update_netCDF_global_atts(
        newFileID, isSimDomDiscrete, freq, isInput, LogInfo
    );
}

/**
@brief Create a variable within a netCDF file

@param[out] varID Variable ID within the netCDF
@param[in] varName Name of the new variable
@param[in] dimIDs Dimensions of the variable
@param[in] ncFileID Domain netCDF file ID
@param[in] varType The type in which the new variable will be
@param[in] numDims Number of dimensions the new variable will hold
@param[in] chunkSizes Custom chunk sizes for the variable being created
@param[in] deflateLevel Level of deflation that will be used for the created
variable
@param[out] LogInfo Holds information on warnings and errors
*/
void SW_NC_create_netCDF_var(
    int *varID,
    const char *varName,
    int *dimIDs,
    const int *ncFileID,
    int varType,
    int numDims,
    size_t chunkSizes[],
    int deflateLevel,
    LOG_INFO *LogInfo
) {

    // Deflate information
    int shuffle = 1; // 0 or 1
    int deflate = 1; // 0 or 1

    if (nc_def_var(*ncFileID, varName, varType, numDims, dimIDs, varID) !=
        NC_NOERR) {
        LogError(LogInfo, LOGERROR, "Failed to create variable '%s'.", varName);
        return; // Exit prematurely due to error
    }

    if (!isnull(chunkSizes)) {
        if (nc_def_var_chunking(*ncFileID, *varID, NC_CHUNKED, chunkSizes) !=
            NC_NOERR) {

            LogError(
                LogInfo, LOGERROR, "Failed to chunk variable '%s'.", varName
            );
            return; // Exit prematurely due to error
        }
    }

    // Do not compress the CRS variables
    // Run the deflate function even if deflate is 0
    // to create default chunking when delation is turned on or off
    if (strcmp(varName, "crs_geogsc") != 0 &&
        strcmp(varName, "crs_projsc") != 0 && varType != NC_STRING &&
        numDims > 0) {

        if (nc_def_var_deflate(
                *ncFileID, *varID, shuffle, deflate, deflateLevel
            ) != NC_NOERR) {
            LogError(
                LogInfo, LOGERROR, "Failed to deflate the variable %s", varName
            );
        }
    }
}

/**
@brief Deconstruct netCDF-related information

@param[in,out] SW_netCDFOut Constant netCDF output file information
*/
void SW_NC_deconstruct(SW_NETCDF_OUT *SW_netCDFOut) {
    SW_NCOUT_deconstruct(SW_netCDFOut);
}

/**
@brief Deep copy a source of input/output netCDF information

@param[in] nSites Number of sites to allocate/deep copy
@param[in] source_output Source output netCDF information to copy
@param[in] source_input Source input netCDF information to copy
@param[out] dest_output Destination output netCDF information to be copied
into from it's source counterpart
@param[out] dest_input Destination input netCDF information to be copied
into from it's source counterpart
@param[out] LogInfo Holds information on warnings and errors
*/
void SW_NC_deepCopy(
    size_t nSites,
    SW_NETCDF_OUT *source_output,
    SW_NETCDF_IN *source_input,
    SW_NETCDF_OUT *dest_output,
    SW_NETCDF_IN *dest_input,
    LOG_INFO *LogInfo
) {

    memcpy(dest_output, source_output, sizeof(*dest_output));
    memcpy(dest_input, source_input, sizeof(*dest_input));

    SW_NCOUT_init_ptrs(dest_output);
    SW_NCIN_init_ptrs(dest_input);

    SW_NCOUT_deepCopy(source_output, dest_output, LogInfo);
    if (LogInfo->stopRun) {
        return; /* Exit function prematurely due to error */
    }

    SW_NCIN_deepCopy(nSites, source_input, dest_input, LogInfo);
}

/**
@brief Read input files for netCDF related actions

@param[in,out] SW_Domain Struct of type SW_DOMAIN holding constant
temporal/spatial information for a set of simulation runs
@param[out] LogInfo Holds information on warnings and errors
*/
void SW_NC_read(SW_DOMAIN *SW_Domain, LOG_INFO *LogInfo) {
    // Read CRS and attributes for netCDFs
    SW_NCOUT_read_atts(
        SW_Domain->startyr,
        &SW_Domain->OutDom.netCDFOutput,
        &SW_Domain->SW_PathInputs,
        LogInfo
    );
    checkReturn(LogInfo->stopRun);

    SW_NCIN_read_input_vars(
        &SW_Domain->netCDFInput,
        &SW_Domain->OutDom.netCDFOutput,
        &SW_Domain->SW_PathInputs,
        SW_Domain->startyr,
        SW_Domain->endyr,
        LogInfo
    );
    checkReturn(LogInfo->stopRun);

    read_system_info(SW_Domain, LogInfo);
}

/**
@brief Allocate memory for internal SOILWAT2 units

@param[out] units_sw Array of text representations of SOILWAT2 units
@param[in] nVar Number of variables available for current output key
@param[out] LogInfo Holds information on warnings and errors
*/
void SW_NC_alloc_unitssw(char ***units_sw, int nVar, LOG_INFO *LogInfo) {

    *units_sw = NULL;

    if (nVar > 0) {

        // Initialize the variable within SW_OUT_DOM
        *units_sw = (char **) Mem_Malloc(
            sizeof(char *) * nVar, "SW_NC_alloc_unitssw", LogInfo
        );
        if (LogInfo->stopRun) {
            return; // Exit function prematurely due to error
        }

        for (int index = 0; index < nVar; index++) {
            (*units_sw)[index] = NULL;
        }
    }
}

/**
@brief Allocate memory for udunits2 unit converter

@param[out] uconv Array of pointers to udunits2 unit converter
@param[in] nVar Number of variables available for current output key
@param[out] LogInfo Holds information on warnings and errors
*/
void SW_NC_alloc_uconv(sw_converter_t ***uconv, int nVar, LOG_INFO *LogInfo) {

    *uconv = NULL;

    if (nVar > 0) {

        *uconv = (sw_converter_t **) Mem_Malloc(
            sizeof(sw_converter_t *) * nVar, "SW_NC_alloc_uconv", LogInfo
        );
        if (LogInfo->stopRun) {
            return; // Exit function prematurely due to error
        }

        for (int index = 0; index < nVar; index++) {
            (*uconv)[index] = NULL;
        }
    }
}

/**
@brief Allocate information about whether or not a variable should be
output/input

@param[out] reqOutVar Specifies the number of variables that can be output
    for a given output key
@param[in] nVar Number of variables available for current output key
@param[out] LogInfo Holds information on warnings and errors
*/
void SW_NC_alloc_req(Bool **reqOutVar, int nVar, LOG_INFO *LogInfo) {

    *reqOutVar = NULL;

    if (nVar > 0) {

        // Initialize the variable within SW_OUT_DOM which specifies if a
        // variable is to be written out or not
        *reqOutVar = (Bool *) Mem_Malloc(
            sizeof(Bool) * nVar, "SW_NC_alloc_outReq", LogInfo
        );
        if (LogInfo->stopRun) {
            return; // Exit function prematurely due to error
        }

        for (int index = 0; index < nVar; index++) {
            (*reqOutVar)[index] = swFALSE;
        }
    }
}

/**
@brief Allocate memory for information in regards to output/input variables

@param[out] keyVars Holds all information about output variables
    in netCDFs (e.g., output variable name)
@param[in] nVar Number of variables available for current output key
@param[in] numAtts Number of attributes that the variable(s) will contain
@param[out] LogInfo Holds information on warnings and errors
*/
void SW_NC_alloc_vars(
    char ****keyVars, int nVar, int numAtts, LOG_INFO *LogInfo
) {

    *keyVars = NULL;

    if (nVar > 0) {

        int index;
        int varNum;
        int attNum;

        // Allocate all memory for the variable information in the current
        // output key
        *keyVars = (char ***) Mem_Malloc(
            sizeof(char **) * nVar, "SW_NC_alloc_vars", LogInfo
        );
        if (LogInfo->stopRun) {
            return; // Exit function prematurely due to error
        }

        for (index = 0; index < nVar; index++) {
            (*keyVars)[index] = NULL;
        }

        for (index = 0; index < nVar; index++) {
            (*keyVars)[index] = (char **) Mem_Malloc(
                sizeof(char *) * numAtts, "SW_NC_alloc_vars", LogInfo
            );
            if (LogInfo->stopRun) {
                for (varNum = 0; varNum < index; varNum++) {
                    free((void *) (*keyVars)[varNum]);
                    (*keyVars)[varNum] = NULL;
                }
                free((void *) *keyVars);
                return; // Exit function prematurely due to error
            }

            for (attNum = 0; attNum < numAtts; attNum++) {
                (*keyVars)[index][attNum] = NULL;
            }
        }
    }
}

/**
@brief Generalized function to get values of any type from a netCDF
files

If `start` and/or `count` are NULL, then the function will read the entire
variable, otherwise it will read in the provided `count` worth of values

@param[in] ncFileID Identifier of the open netCDF file to test
@param[in] varID Variable identifier within the given netCDF
@param[in] varName Name of the variable to access
@param[in] start Starting indices for each dimension of variable to read
@param[in] count Number of values to read in each direction of every
dimension
@param[in] destValToDouble A flag specifying if the read in value(s) should
be converted to double
@param[out] values Value(s) to write in
@param[out] LogInfo Holds information on warnings and errors
*/
void SW_NC_get_vals(
    int ncFileID,
    int *varID,
    const char *varName,
    const size_t *start,
    const size_t *count,
    Bool destConvToDouble,
    void *values,
    LOG_INFO *LogInfo
) {
    int res;

    if (*varID < 0 && !isnull(varName)) {
        SW_NC_get_var_identifier(ncFileID, varName, varID, LogInfo);
        if (LogInfo->stopRun) {
            return; /* Exit function prematurely due to unknown variable */
        }
    }

    if (isnull(start) || isnull(count)) {
        res = (destConvToDouble) ?
                  nc_get_var_double(ncFileID, *varID, (double *) values) :
                  nc_get_var(ncFileID, *varID, values);
    } else {
        res = (destConvToDouble) ?
                  nc_get_vara_double(
                      ncFileID, *varID, start, count, (double *) values
                  ) :
                  nc_get_vara(ncFileID, *varID, start, count, values);
    }

    if (res != NC_NOERR) {
        LogError(
            LogInfo,
            LOGERROR,
            "Failed to read values of variable '%s'.",
            varName
        );
    }
}

void SW_NC_open(
    const char *ncFileName, int openMode, int *fileID, LOG_INFO *LogInfo
) {
    if (nc_open(ncFileName, openMode, fileID) != NC_NOERR) {
        LogError(LogInfo, LOGERROR, "Failed to open file '%s'.", ncFileName);
    }
}

#if defined(SWMPI)
void SW_NC_open_par(
    const char *fileName, int mode, MPI_Comm comm, int *id, LOG_INFO *LogInfo
) {
    if (nc_open_par(fileName, mode, comm, MPI_INFO_NULL, id) != NC_NOERR) {
        LogError(
            LogInfo,
            LOGERROR,
            "Failed to open file '%s' for parallel I/O.",
            fileName
        );
    }

    SW_MPI_Barrier(comm);
}
#endif

/**
@brief Calculate the number of active sites exist in a process' subdomain
and get the translated subrectangle/subdomain to go along with them

@param[in] SW_Domain Struct of type SW_DOMAIN holding constant
    temporal/spatial information for a set of simulation runs
@param[out] LogInfo Holds information on warnings and errors
*/
void SW_NC_proc_sites(SW_DOMAIN *SW_Domain, LOG_INFO *LogInfo) {
    find_active_sites(SW_Domain, LogInfo);
    checkReturn(LogInfo->stopRun);

    get_tsuid_bnds(SW_Domain, LogInfo);
}

/**
@brief Calculate (in the order of priority)
    1) Temporal chunk size for each output key based on available
       memory and file system block size
        a) File system block size will be attempted to be met for
           each output key as minimum temporal chunk size, then
           input will be calculated
    2) Number of years of weather to read in at once given the
       rest of the available memory (at least one year is required)

The calculation will attempt to find an optimal way to organize the
available RAM into the following organization
    | Functional Mem | Input Mem | Output Mem |
with minimal waste

@param[in] worldSize Total number of processes that the MPI run has created
(only relevant with SWMPI enabled)
@param[in,out] SW_Domain Struct of type SW_DOMAIN holding constant
    temporal/spatial information for a set of simulation runs; return
    with updated temporal chunking sizes for each output key/period
@param[out] LogInfo Holds information on warnings and errors
*/
void SW_NC_calc_read_write_sizes(
    int worldSize, SW_DOMAIN *SW_Domain, LOG_INFO *LogInfo
) {
    // Initialize variables
    const int veg_method = SW_Domain->SW_ConstInfo.VegProdIn.veg_method;
    const IntU methodMaxDepthSoilTemperature =
        SW_Domain->SW_ConstInfo.SiteIn.methodMaxDepthSoilTemperature;
    const Bool allocTempOnly = (Bool) (methodMaxDepthSoilTemperature == 1);
    const IntU nAllocTempOnly = 1;

    const size_t domSize = sizeof(SW_DOMAIN) - sizeof(SW_DOMAIN_CONST);
    const size_t logSize = sizeof(LOG_INFO);
    const size_t weathYearSize = sizeof(SW_WEATHER_HIST);

    const IntU nDynMarkovVars = 8;
    const IntU nDynVegProdInfo = 11;

#if defined(SWDEBUG)
    const size_t maxWBStrSize = MAX_LOG_SIZE;
#endif

    size_t totalDomSize = 0;
    size_t totDomSiteSizes = 0;

    size_t availMem = SW_Domain->availMemory;
    size_t outputMem;

    TimeInt n_years = SW_Domain->endyr - SW_Domain->startyr + 1;

    size_t ppmSize = n_years * sizeof(double);
    size_t filePrefSize =
        (strlen(SW_Domain->SW_ConstInfo.SoilWatIn.hist.file_prefix) + 1) *
        sizeof(char);

    // Calculate the total size used by statically sized structs
    // Per site
    size_t perSiteSize = sizeof(SW_RUN) + logSize;

    // Information for all sites
    size_t domainInfoSize =
        domSize + SW_DOM_calc_dyn_mem(SW_Domain) + ppmSize + filePrefSize;

    if (SW_Domain->SW_ConstInfo.WeatherIn.generateWeatherMethod == wgMKV) {
        // Dynamically allocated markov arrays in SW_MARKOV_INPUTS
        perSiteSize += (((size_t) nDynMarkovVars) * MAX_DAYS * sizeof(double));
    }

    if (veg_method == VEG_METHOD_DYN_EST || allocTempOnly) {
        if (veg_method == VEG_METHOD_DYN_EST) {
            // All dynamically allocated variables in SW_VEGPROD_SIM
            perSiteSize +=
                (((size_t) nDynVegProdInfo) * n_years * sizeof(double));
        } else {
            // Only the annual temperature variable in SW_VEGPROD_SIM
            perSiteSize +=
                (((size_t) nAllocTempOnly) * n_years * sizeof(double));
        }
    }

    perSiteSize += weathYearSize;

#if defined(SWDEBUG)
    // Assume sizes for each water balance check string to be
    // at most MAX_LOG_SIZE
    perSiteSize += (N_WBCHECKS * maxWBStrSize * sizeof(char));
#endif

#if defined(SWMPI)
    // Get the memory usage for all sites
    SW_MPI_Allreduce(
        &domainInfoSize,
        &totalDomSize,
        1,
        SW_MPI_SIZE_T,
        MPI_SUM,
        MPI_COMM_WORLD
    );

    SW_MPI_Allreduce(
        &perSiteSize,
        &totDomSiteSizes,
        1,
        SW_MPI_SIZE_T,
        MPI_SUM,
        MPI_COMM_WORLD
    );
#else
    totalDomSize = domainInfoSize;
    totDomSiteSizes = perSiteSize;
#endif

    if (totalDomSize + totDomSiteSizes > availMem) {
        LogError(
            LogInfo,
            LOGERROR,
            "Estimated minimum memory usage is %f GB, where only "
            "%zu is available.",
            (totalDomSize + totDomSiteSizes) / GB_TO_BYTES,
            availMem
        );
        return;
    }

    // Multiply "outputMem" by set fraction to estimate the rest of memory
    // used
    // NOTE: This constant may be modified given an insufficient amount
    // or surplus of memory; making it dynamic could be a useful update in
    // the future
    availMem -= (size_t) ((double) availMem / OUT_MEM_DIV);

#if defined(SWMPI)
    availMem /= worldSize;
#endif

    // Allocate half of the remaining memory to outputs
    outputMem = (availMem - totalDomSize - totDomSiteSizes) / 2;

    calc_temporal_chunks(worldSize, SW_Domain, outputMem, outputMem, LogInfo);
}
