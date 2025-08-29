#include "include/SW_MPI.h"
#include "include/filefuncs.h"          // for LogError, BaseName, DirName
#include "include/generic.h"            // for Bool, swTRUE, isnull, swFALSE
#include "include/myMemory.h"           // for Mem_Malloc, Mem_ReAlloc, Str...
#include "include/SW_Control.h"         // for runSims
#include "include/SW_Domain.h"          // for SW_DOM_calc_ncSuid
#include "include/SW_Files.h"           // for eLog
#include "include/SW_Main_lib.h"        // for sw_write_warnings, sw_init_logs
#include "include/SW_Markov.h"          // for SW_MKV_construct, allocateMKV
#include "include/SW_netCDF_General.h"  // for vNCprog, SW_NC_open_par, SW_...
#include "include/SW_netCDF_Input.h"    // for ForEachNCInKey, numVarsInKey
#include "include/SW_netCDF_Output.h"   // for SW_NCOUT_write_output, SW_NC...
#include "include/SW_Output.h"          // for ForEachOutKey
#include "include/SW_Output_outarray.h" // for SW_OUT_construct_outarray
#include "include/SW_Weather.h"         // for SW_WTH_allocateAllWeather
#include "include/Times.h"              // for diff_walltime, SW_WT_ReportTime

#include <math.h>       // for ceil
#include <mpi.h>        // for MPI_DOUBLE, MPI_INT, MPI_COM...
#include <netcdf.h>     // for nc_close, NC_NOWRITE, NC_NOERR
#include <netcdf_par.h> // for nc_var_par_access, NC_COLLEC...
#include <signal.h>     // for signal, SIGINT, SIGTERM, sig...
#include <stdio.h>      // for FILE, snprintf, fprintf, FIL...
#include <stdlib.h>     // for size_t, NULL, free
#include <string.h>     // for memcpy, memset, strcmp, strlen

/* =================================================== */
/*                  Local Definitions                  */
/* --------------------------------------------------- */

#define FAIL_ALLOC_DESIG 0
#define FAIL_ROOT_SETUP 1
#define FAIL_ROOT_SEND 2
#define FAIL_ALLOC_SUIDS 3
#define FAIL_ALLOC_TSUIDS 4

#define REQ_OUT_LOG 0
#define REQ_OUT_NC 1
#define REQ_OUT_BOTH 2

#define COMP_COMPLETE 0

/* =================================================== */
/*             Local Function Definitions              */
/* --------------------------------------------------- */

/**
@brief Handle OpenMPI error if one occurs

@param[in] rank Process number known to MPI for the current process (aka rank)
@param[in] mpiError Result value from an MPI function call that
    returned an error

@sideeffect Exit all instances of the MPI program
*/
static void errorMPI(int rank, int mpiError) {
    char errorStr[MAX_FILENAMESIZE] = {'\0'};
    int errorLen = 0;

    MPI_Error_string(mpiError, errorStr, &errorLen);

    SW_MPI_Fail(rank, SW_MPI_FAIL_MPI, errorStr);

    MPI_Abort(MPI_COMM_WORLD, mpiError);
}

/**
@brief Deallocate helper memory that was allocated during the call to
    `SW_MPI_proc_workload()`

@param[in] numActiveSites Number of active sites that will be simulated
@param[in,out] activeSuids A list of domain SUIDs that was activated
    by the program and/or user given the progress input file; return
    the deallocation of this
@param[in,out] activeTSuids A list of translated domain SUIDs that was activated
    by the program and/or user given the progress input file; return the
    deallocation of this
@param[in,out] nSuidsAssign An array of size [n_ranks] holding the number of
suids to assign to each process; return the deallocation of this
*/
static void deallocProcHelpers(
    size_t numActiveSites,
    size_t ***activeSuids,
    size_t ***activeTSuids,
    size_t **nSuidsAssign
) {
    const int num2D = 4;
    const int num1D = 2;
    int var;
    size_t pair;
    int inKey;

    void **oneDimFree[] = {(void **) nSuidsAssign};

    void ***twoDimFree[] = {(void ***) activeSuids};

    for (var = 0; var < num1D; var++) {
        if (!isnull((void *) (*oneDimFree[var]))) {
            free((*oneDimFree[var]));
            (*oneDimFree[var]) = NULL;
        }
    }

    for (var = 0; var < num2D; var++) {
        if (!isnull(*twoDimFree[var])) {
            for (pair = 0; pair < numActiveSites; pair++) {
                if (!isnull((*twoDimFree[var])[pair])) {
                    free((*twoDimFree[var])[pair]);
                    (*twoDimFree[var])[pair] = NULL;
                }
            }
            free((void *) (*twoDimFree[var]));
            (*twoDimFree[var]) = NULL;
        }
    }

    ForEachNCInKey(inKey) {
        if (!isnull(activeTSuids[inKey])) {
            for (pair = 0; pair < numActiveSites; pair++) {
                if (!isnull(activeTSuids[inKey][pair])) {
                    free((void *) activeTSuids[inKey][pair]);
                    activeTSuids[inKey][pair] = NULL;
                }
            }

            free((void *) activeTSuids[inKey]);
            activeTSuids[inKey] = NULL;
        }
    }
}

/**
@brief Helper function to `reorder_output()` to calculate how many elements
    are to be rearranged based on time size, layer, and pft sizes
*/
static size_t calc_num_out_vals(size_t timeSize, IntUS nsl, IntUS npft) {
    size_t numElem = 1;

    /* Fill in time dimension */
    if (timeSize > 0) {
        numElem *= timeSize;
    }

    /* Fill in vertical (if present) */
    if (nsl > 0) {
        numElem *= nsl;
    }

    /* Fill in pft (if present) */
    if (npft > 0) {
        numElem *= npft;
    }

    return numElem;
}

/**
@brief When receiving input from a compute process, it is not in the
    order we need it to be to be output properly by the output function
    due to the possibility of writing more than one site/grid cell;
    reorder output so it can properly be output by the netCDF
    output function

@param[in,out] OutDom Struct of type SW_OUT_DOM that holds output
    information that do not change throughout simulation runs
@param[in] timeSizes An array of size two to hold the time sizes for every
    output file for a specific output period
@param[in] numOutFiles Number of output files for each
    output key/period
@param[in] numInputs Number of inputs expected/received
@param[out] src_p_OUT Source of accumulated output values throughout
    simulation runs
@param[out] dest_p_OUT Destination of accumulated output values throughout
    simulation runs
*/
static void reorder_output(
    SW_OUT_DOM *OutDom,
    size_t *timeSizes[],
    unsigned int numOutFiles,
    size_t numInputs,
    double *src_p_OUT[][SW_OUTNPERIODS],
    double *dest_p_OUT[][SW_OUTNPERIODS]
) {
    int outKey;
    OutPeriod pd;
    int pdIndex;
    size_t numElem;
    size_t elemIndex;
    size_t destIndices[SW_OUTNKEYS][SW_OUTNPERIODS] = {{0}};
    size_t destOffIndex;
    size_t timeSize;
    size_t totalTimeSize;
    size_t pOUTIndex;
    size_t vertSize;
    size_t pftSize;
    size_t elem;
    size_t oneSiteSize;
    unsigned int file;
    int varNum;
    size_t input;

    ForEachOutKey(outKey) {
        for (pdIndex = 0; pdIndex < OutDom->used_OUTNPERIODS; pdIndex++) {
            pd = OutDom->timeSteps[outKey][pdIndex];

            if (pd != eSW_NoTime) {
                totalTimeSize = 0;

                for (file = 0; file < numOutFiles; file++) {
                    timeSize = timeSizes[pd][file];

                    oneSiteSize = OutDom->nrow_OUT[pd] *
                                  (OutDom->ncol_OUT[outKey] + ncol_TimeOUT[pd]);

                    for (varNum = 0; varNum < OutDom->nvar_OUT[outKey];
                         varNum++) {
                        numElem = calc_num_out_vals(
                            timeSize,
                            OutDom->nsl_OUT[outKey][varNum],
                            OutDom->npft_OUT[outKey][varNum]
                        );

                        if (!OutDom->netCDFOutput
                                 .reqOutputVars[outKey][varNum]) {
                            continue; // Skip variable iteration
                        }

                        pOUTIndex =
                            OutDom->netCDFOutput.iOUToffset[outKey][pd][varNum];

                        if (totalTimeSize > 0) {
                            // 1 if no soil layers
                            vertSize = (OutDom->nsl_OUT[outKey][varNum] > 0) ?
                                           OutDom->nsl_OUT[outKey][varNum] :
                                           1;

                            // 1 if no vegtypes
                            pftSize = (OutDom->npft_OUT[outKey][varNum] > 0) ?
                                          OutDom->npft_OUT[outKey][varNum] :
                                          1;
                            pOUTIndex +=
                                iOUTnc(totalTimeSize, 0, 0, vertSize, pftSize);
                        }

                        for (input = 0; input < numInputs; input++) {
                            elemIndex = (input * oneSiteSize) + pOUTIndex;

                            for (elem = 0; elem < numElem; elem++) {
                                destOffIndex = destIndices[outKey][pd];

                                dest_p_OUT[outKey][pd][destOffIndex] =
                                    src_p_OUT[outKey][pd][elemIndex];

                                destIndices[outKey][pd]++;
                                elemIndex++;
                            }
                        }
                    }

                    totalTimeSize += timeSize;
                }
            }
        }
    }
}

/**
@brief Allocate room to store a list of domain SUIDs

@param[in] numActiveSites Number of active sites that will be
    simulated
@param[out] activeSuids A list of domain SUIDs that was activated
    by the program and/or user given the progress input file
@param[out] LogInfo Holds information on warnings and errors
*/
static void allocateActiveSuids(
    size_t numActiveSites, size_t ***activeSuids, LOG_INFO *LogInfo
) {
    const int nElemPerSuid = 2;
    size_t domIndex;

    *activeSuids = (size_t **) Mem_Malloc(
        sizeof(size_t *) * numActiveSites, "allocateActiveSuids", LogInfo
    );
    if (LogInfo->stopRun) {
        return;
    }
    for (domIndex = 0; domIndex < numActiveSites; domIndex++) {
        (*activeSuids)[domIndex] = NULL;
    }

    for (domIndex = 0; domIndex < numActiveSites; domIndex++) {
        (*activeSuids)[domIndex] = (size_t *) Mem_Malloc(
            sizeof(size_t) * nElemPerSuid, "allocateActiveSuids", LogInfo
        );
        if (LogInfo->stopRun) {
            return;
        }

        (*activeSuids)[domIndex][0] = (*activeSuids)[domIndex][1] = 0;
    }
}

/**
@brief Allocate room to store a list of translated domain SUIDs

@param[in] numActiveSites Number of active sites that will be
    simulated
@param[out] activeTSuids A list of translated domain SUIDs that was activated
    by the program and/or user given the progress input file
@param[out] LogInfo Holds information on warnings and errors
*/
static void allocateActiveTSuids(
    size_t numActiveSites, size_t ***activeTSuids, LOG_INFO *LogInfo
) {
    const size_t nIndexVals = 2;
    size_t site;
    size_t col;

    *activeTSuids = (size_t **) Mem_Malloc(
        sizeof(size_t *) * numActiveSites, "allocateActiveTSuids", LogInfo
    );
    for (site = 0; site < numActiveSites; site++) {
        (*activeTSuids)[site] = NULL;
    }

    for (site = 0; site < numActiveSites; site++) {
        (*activeTSuids)[site] = (size_t *) Mem_Malloc(
            sizeof(size_t) * nIndexVals, "allocateActiveTSuids", LogInfo
        );
        if (LogInfo->stopRun) {
            return;
        }

        for (col = 0; col < nIndexVals; col++) {
            (*activeTSuids)[site][col] = 0;
        }
    }
}

/**
@brief Calculate the number of suids that will be distributed to
all processes; this will happen by attempting to divide the number
of active sites evently across all process/ranks. If the division results
in a remainder, then an extra site will be assigned to reach rank until
there are no more remainder sites left

@param[in] numActiveSites Number of active sites that will be simulated
@param[in] worldSize Total number of processes that the MPI run has created
@param[out] nSuids An array that holds the total number of suids each respective
rank/process will receive/handle
@param[out] LogInfo Holds information on warnings and errors
*/
static void calcNumSites(
    size_t numActiveSites, size_t worldSize, size_t **nSuids, LOG_INFO *LogInfo
) {
    size_t siteIndex;
    size_t numProcSites = numActiveSites / worldSize;
    size_t overflowSites = numActiveSites % worldSize;

    *nSuids = (size_t *) Mem_Malloc(
        sizeof(size_t) * worldSize, "calcNumSites()", LogInfo
    );
    if (LogInfo->stopRun) {
        return;
    }

    for (siteIndex = 0; siteIndex < worldSize; siteIndex++) {
        (*nSuids)[siteIndex] = 0;
    }

    for (siteIndex = 0; siteIndex < worldSize; siteIndex++) {
        (*nSuids)[siteIndex] = numProcSites;

        if (overflowSites > 0) {
            (*nSuids)[siteIndex]++;
            overflowSites--;
        }
    }
}

/**
@brief Get or send a process domain designation to get ready for
    simulation

@param[in] rank Process number known to MPI for the current process (aka rank)
@param[in] nSuidsAssign An array of size [n_ranks] holding the number of
suids to assign to each process
@param[in] activeSuids A list of domain SUIDs that was activated
by the program and/or user given the progress input file
@param[in] activeTSuids A list of translated domain SUIDs that was activated
by the program and/or user given the progress input file
@param[in] worldSize Total number of processes that the MPI run has created
@param[in] useIndexFile Specifies to create/use an index file
@param[out] SW_Domain Struct of type SW_DOMAIN holding constant
    temporal/spatial information for a set of simulation runs; this function
    call will update it with allocations to hold domain suids and
    the number of suids to simulate
@param[out] LogInfo Holds information on warnings and errors
*/
static void assignProcs(
    int rank,
    size_t nSuidsAssign[],
    size_t **activeSuids,
    size_t ***activeTSuids,
    int worldSize,
    const Bool useIndexFile[],
    SW_DOMAIN *SW_Domain,
    LOG_INFO *LogInfo
) {
    const int recvSuidCount = 1;
    const int coordPairSize = 2;
    size_t pair;
    size_t activePairIndex;
    int destRank;
    int inKey;
    size_t numPairs;

    MPI_Request nullReq = MPI_REQUEST_NULL;

    // Send nSuid information so processes know how many suids
    // to allocate for
    SW_MPI_Scatter(
        MPI_COMM_WORLD,
        nSuidsAssign,
        SW_MPI_ROOT,
        1,
        recvSuidCount,
        &SW_Domain->nProcSuids
    );
    allocateActiveSuids(
        SW_Domain->nProcSuids, &SW_Domain->domSuids[eSW_InDomain], LogInfo
    );
    if (LogInfo->stopRun) {
        goto reportError;
    }

    ForEachNCInKey(inKey) {
        if (inKey > eSW_InDomain && useIndexFile[inKey]) {
            allocateActiveTSuids(
                SW_Domain->nProcSuids, &SW_Domain->domSuids[inKey], LogInfo
            );
            if (LogInfo->stopRun) {
                goto reportError;
            }
        }
    }

reportError:
    if (SW_MPI_setup_fail(LogInfo->stopRun, MPI_COMM_WORLD)) {
        return;
    }

    // Copy root process' information
    if (rank == SW_MPI_ROOT) {
        for (pair = 0; pair < nSuidsAssign[SW_MPI_ROOT]; pair++) {
            ForEachNCInKey(inKey) {
                if (!isnull(SW_Domain->domSuids[inKey])) {
                    if (inKey > eSW_InDomain) {
                        SW_Domain->domSuids[inKey][pair][0] =
                            (useIndexFile[inKey]) ?
                                activeTSuids[inKey][pair][0] :
                                activeSuids[pair][0];
                        SW_Domain->domSuids[inKey][pair][1] =
                            (useIndexFile[inKey]) ?
                                activeTSuids[inKey][pair][1] :
                                activeSuids[pair][1];
                    } else {
                        SW_Domain->domSuids[eSW_InDomain][pair][0] =
                            activeSuids[pair][0];
                        SW_Domain->domSuids[eSW_InDomain][pair][1] =
                            activeSuids[pair][1];
                    }
                }
            }
        }
    }

    // Communicate suids to all ranks
    activePairIndex = (rank == 0) ? nSuidsAssign[SW_MPI_ROOT] : 0;
    for (destRank = 1; destRank < worldSize; destRank++) {
        if (rank == SW_MPI_ROOT || destRank == rank) {
            numPairs =
                (rank == 0) ? nSuidsAssign[destRank] : SW_Domain->nProcSuids;

            for (pair = 0; pair < numPairs; pair++) {
                ForEachNCInKey(inKey) {
                    if ((inKey > eSW_InDomain && !useIndexFile[inKey]) ||
                        isnull(SW_Domain->domSuids[inKey])) {

                        continue;
                    }

                    if (rank == SW_MPI_ROOT) {
                        SW_MPI_Send(
                            SW_MPI_SIZE_T,
                            (!isnull(activeTSuids[inKey])) ?
                                activeTSuids[inKey][activePairIndex] :
                                activeSuids[activePairIndex],
                            coordPairSize,
                            destRank,
                            swTRUE,
                            0,
                            &nullReq
                        );
                    } else {
                        SW_MPI_Recv(
                            SW_MPI_SIZE_T,
                            SW_Domain->domSuids[inKey][pair],
                            coordPairSize,
                            SW_MPI_ROOT,
                            swTRUE,
                            0,
                            &nullReq
                        );
                    }
                }

                activePairIndex++;
            }
        }
    }
}

/**
@brief Wrapper for MPI function to free a type and throw a
    warning if a free throws an error

@param[in,out] type Custom MPI type to attempt to free
@param[out] LogInfo Holds information on warnings and errors
*/
static void free_type(MPI_Datatype *type, LOG_INFO *LogInfo) {
    int res = MPI_SUCCESS;

    res = MPI_Type_free(type);

    if (res != MPI_SUCCESS) {
        LogError(LogInfo, LOGWARN, "Could not free a custom MPI type.");
    }
}

/**
@brief Helper to calculate the contiguous reading/writings available
    based on the domain SUIDs

This function must result in a number of writes between
[1, num compute processes]

@param[in] suids A list of domain SUIDs whose data will be distributed
    as the next batch of input; can be domain or translated (index)
    SUIDs
@param[in] numDomSuids Number of domain SUIDs that were given
@param[in] sDom Specifies the program's domain is site-oriented
@param[in] useSuccFlags A flag specifying if the function should take the
    success flags of simulation runs into account to make contiguous writes
    of the same value (pass or fail)
@param[in] succFlags A list of flags specifying if runs for SUIDs are to be
    used (NULL if not used)
@param[out] numWrites The number of writes that must be performed
    by the calling function to output all simulated information
    for the sites
@param[out] starts A list of calculated start values for when dealing
    with the netCDF library
@param[out] counts A list of calculated count values for when dealing
    with the netCDF library
*/
static void get_contiguous_counts(
    size_t suids[][2],
    size_t numDomSuids,
    Bool sDom,
    size_t nSuidsLeft,
    Bool useSuccFlags,
    const Bool *succFlags,
    size_t *numWrites,
    size_t starts[N_SUID_ASSIGN][2],
    size_t counts[N_SUID_ASSIGN][2]
) {
    // Intial state
    // prevX = x[0]
    // prevY = y[0]
    // start = [prevX, 0] or [prevY, prevX]
    // count = [1, 1]
    // number of values
    // Note: currYX and prevYX below are used for either Y (gridded)
    // or X (sites)
    size_t prevYX = suids[0][0]; // NOLINT(clang-analyzer-core.NullDereference)
    size_t prevX = suids[0][1];  // NOLINT(clang-analyzer-core.NullDereference)
    size_t writeIndex = 0;
    size_t suidIndex;
    int numContVals = 1;
    size_t *suid;
    int xIndex = (sDom) ? 0 : 1;
    Bool prevFlag = swTRUE;
    Bool currFlag = swTRUE;

    starts[0][0] = prevYX;
    starts[0][1] = prevX;

    counts[0][0] = (sDom) ? numDomSuids : 1;
    counts[0][1] = (sDom) ? 1 : N_SUID_ASSIGN; // NOLINT(bugprone-branch-clone)

    if (useSuccFlags) {
        currFlag = succFlags[0];
    }

    // Loop through selected domain SUIDs
    for (suidIndex = 1; suidIndex < numDomSuids && nSuidsLeft > 0;
         suidIndex++) {
        suid = suids[suidIndex];

        if (useSuccFlags) {
            prevFlag = currFlag;
            currFlag = succFlags[suidIndex];
        }

        // Check if x is not prevX + 1 or y is not prevYX + 1;
        // This means we found a place that is not contiguous
        // in the domain SUIDs
        if (((sDom && suid[0] != prevYX + 1) ||
             (!sDom && (suid[0] != prevYX || suid[1] != prevX + 1))) ||
            (useSuccFlags && prevFlag != currFlag)) {

            // Set the count for X to number of SUIDs
            counts[writeIndex][xIndex] = numContVals;

            writeIndex++;

            // Prepare next start by using this SUID's values
            // so we can start the contiguous site/gridcell finding
            // from this point
            numContVals = 0;

            prevYX = suids[suidIndex][0];
            prevX = (sDom) ? 0 : suids[suidIndex][1];

            starts[writeIndex][0] = prevYX;
            starts[writeIndex][1] = prevX;
            counts[writeIndex][0] = (sDom) ? numDomSuids : 1;
            prevFlag = currFlag;

            nSuidsLeft--;
        }
        prevYX = suid[0];
        prevX = (sDom) ? 0 : suid[1];

        numContVals++;
    }

    counts[writeIndex][xIndex] = numContVals;

    *numWrites = writeIndex + 1;
}

/**
@brief Calculate contiguous read/write indices for every input netCDF key
    including a key specifically for program's domain

@note The idea behind doing this is the experimentally determined fact that
    each read/write of data is more worth the time when the action gathers more
    data at once; this means if we do < N reads/writes sequentially, where N is
the number of sites/gridcells, this should save time when reading/writing
    comparatively

@param[in] nSuids Number of SUIDs that will be assigned
@param[in] nSuidsLeft Number of SUIDs that are to be assigned; this
    is only used when an I/O process is on the last assignments of
    SUIDs (i.e., we attempt to assign more SUIDs than we have left)
@param[in] useIndexFile Specifies to create/use an index file
@param[in] readInVars Specifies which variables are to be read-in as input
@param[in] distSUIDs A list of domain SUIDs whose data will be distributed
    as the next batch of input
@param[in] distTSUIDs A list of domain translated SUIDs whose data will be
    distributed as the next batch of input
@param[in] sDoms A list of flags that specify the domain type of every
    netCDF input key (if used); specifies all other keys in `start` and
    `count` including the program's domain
@param[in] spatialVars1D Specifies if the input key "eSW_InSpatial" inputs
    are 1- or 2-dimensional; this will impact the order of "start" and "count"
    values
@param[out] numWrites A list of values that specify how many reads/writes a
    specific start-count group would need to cover all SUIDs; this should be
    between [1, nSuids]; the best case being 1 and worst being nSuids
@param[out] starts A list of size SW_NINKEYSNC specifying the start
    indices used when reading/writing using the netCDF library;
    default size is `nSuids` but as mentioned in `numWrites`, it would
    be best to not fill this array
@param[out] counts A list of size SW_NINKEYSNC specifying the count
    indices used when reading/writing using the netCDF library;
    default size is `nSuids` but as mentioned in `numWrites`, it would
    be best to not fill this array; counts match placements with `start`
    indices for each key
*/
static void calculate_contiguous_allkeys(
    size_t nSuids,
    size_t nSuidsLeft,
    const Bool useIndexFile[],
    Bool *readInVars[],
    size_t distSUIDs[SW_NINKEYSNC][N_SUID_ASSIGN][2],
    Bool sDoms[],
    size_t numWrites[],
    size_t starts[SW_NINKEYSNC][N_SUID_ASSIGN][2],
    size_t counts[SW_NINKEYSNC][N_SUID_ASSIGN][2]
) {
    int inKey;
    Bool useIndex;
    Bool useTranslated;
    size_t suid;

    ForEachNCInKey(inKey) {
        useIndex = (inKey != eSW_InDomain) ? useIndexFile[inKey] : swFALSE;

        if (inKey == eSW_InDomain || readInVars[inKey][0]) {
            if (inKey == eSW_InDomain || useIndex) {
                useTranslated = (Bool) (inKey != eSW_InDomain && useIndex);

                get_contiguous_counts(
                    (useTranslated) ? distSUIDs[inKey] :
                                      distSUIDs[eSW_InDomain],
                    nSuids,
                    sDoms[inKey],
                    nSuidsLeft,
                    swFALSE,
                    NULL,
                    &numWrites[inKey],
                    starts[inKey],
                    counts[inKey]
                );
            } else {
                numWrites[inKey] = numWrites[eSW_InDomain];

                for (suid = 0; suid < numWrites[inKey]; suid++) {
                    starts[inKey][suid][0] = starts[eSW_InDomain][suid][0];
                    starts[inKey][suid][1] = starts[eSW_InDomain][suid][1];

                    counts[inKey][suid][0] = counts[eSW_InDomain][suid][0];
                    counts[inKey][suid][1] = counts[eSW_InDomain][suid][1];
                }
            }
        }
    }
}

/**
@brief Find the active sites within the provided domain so we do not
try to simulate/assign to compute processes

@param[in] rank Process number known to MPI for the current process (aka rank)
@param[in,out] SW_Domain Struct of type SW_DOMAIN holding constant
    temporal/spatial information for a set of simulation runs
@param[out] activeSuids A list of domain SUIDs that was activated
    by the program and/or user given the progress input file
@param[out] numActiveSites Number of active sites that will be simulated
@param[out] LogInfo Holds information on warnings and errors
*/
void find_active_sites(
    int rank,
    SW_DOMAIN *SW_Domain,
    size_t ***activeSuids,
    size_t *numActiveSites,
    LOG_INFO *LogInfo
) {
    int suid = 0;
    signed char *prog = NULL;
    int progVarID = SW_Domain->netCDFInput.ncDomVarIDs[vNCprog];
    Bool sDom = SW_Domain->netCDFInput.siteDoms[eSW_InDomain];
    size_t numSites =
        (sDom) ? SW_Domain->nDimS : SW_Domain->nDimY * SW_Domain->nDimX;
    size_t progIndex;
    int progFileID = SW_Domain->SW_PathInputs.ncDomFileIDs[vNCprog];
    size_t count[] = {0, 0};
    size_t start[] = {0, 0};

    *numActiveSites = 0;

    if (rank == SW_MPI_ROOT) {
        prog = (signed char *) Mem_Malloc(
            sizeof(signed char) * numSites, "find_active_sites", LogInfo
        );

        count[0] = (sDom) ? SW_Domain->nDimS : SW_Domain->nDimY;
        count[1] = (sDom) ? 0 : SW_Domain->nDimX;
    }

    if (SW_MPI_setup_fail(LogInfo->stopRun, MPI_COMM_WORLD)) {
        return;
    }

    /* Read all progress values - set the parallel access to
       independent so all processes but the root can read 0 values */
    if (nc_get_vara_schar(progFileID, progVarID, start, count, prog) !=
        NC_NOERR) {

        LogError(
            LogInfo,
            LOGERROR,
            "Could not read all of the progress variable values."
        );

        goto freeMem;
    }

    SW_NC_toggle_par_access(progFileID, progVarID, NC_COLLECTIVE, LogInfo);
    if (LogInfo->stopRun || rank > SW_MPI_ROOT) {
        goto freeMem;
    }

    /* Go through the entirety of the progress values and keep track of
       how many are ready to be run */
    for (progIndex = 0; progIndex < numSites; progIndex++) {
        *numActiveSites += (prog[progIndex] == PRGRSS_READY) ? 1 : 0;
    }

    allocateActiveSuids(*numActiveSites, activeSuids, LogInfo);
    if (LogInfo->stopRun) {
        goto freeMem;
    }

    /* Go through the progress values again and calculate/store
       the active domain SUIDs */
    for (progIndex = 0; progIndex < numSites; progIndex++) {
        if (prog[progIndex] == PRGRSS_READY) {
            SW_DOM_calc_ncSuid(SW_Domain, progIndex, (*activeSuids)[suid]);
            suid++;
        }
    }

freeMem:
    if (!isnull(prog)) {
        free((void *) prog);
    }
}

/**
@brief Get translated SUIDs and store them for workload distribution

@param[in,out] SW_Domain Struct of type SW_DOMAIN holding constant
    temporal/spatial information for a set of simulation runs
@param[in] activeSuids A list of domain SUIDs that was activated
    by the program and/or user given the progress input file
@param[in] activeTSuids A list of translated domain SUIDs that was activated
    by the program and/or user given the progress input file
@param[in] numActiveSites Number of active sites that will be simulated
@param[out] LogInfo Holds information on warnings and errors
*/
static void get_activated_tsuids(
    SW_DOMAIN *SW_Domain,
    size_t **activeSuids,
    size_t ***activeTSuids,
    size_t numActiveSites,
    LOG_INFO *LogInfo
) {
    Bool inSDom;
    Bool sProgDom = SW_Domain->netCDFInput.siteDoms[eSW_InDomain];
    size_t nSites;
    unsigned int *sxIndexVals = NULL;
    unsigned int *yIndexVals = NULL;
    Bool **readInVars = SW_Domain->netCDFInput.readInVars;
    Bool *useIndexFile = SW_Domain->netCDFInput.useIndexFile;
    size_t numPosKeys = eSW_LastInKey;
    int index;
    int site;
    int inKey;
    int fileID = -1;
    int varID;
    size_t *indexCell;
    size_t *domSuid;
    size_t offset;
    const int indexFile = 0;

    for (index = 0; index < (int) numPosKeys; index++) {
        activeTSuids[index] = NULL;
    }

    ForEachNCInKey(inKey) {
        if (inKey == eSW_InDomain || !readInVars[inKey][0] ||
            !useIndexFile[inKey]) {
            continue;
        }

        fileID = SW_Domain->SW_PathInputs.openInFileIDs[inKey][indexFile][0];

        inSDom = SW_Domain->netCDFInput.siteDoms[inKey];
        nSites =
            (sProgDom) ? SW_Domain->nDimS : SW_Domain->nDimX * SW_Domain->nDimY;

        sxIndexVals = (unsigned int *) Mem_Malloc(
            sizeof(unsigned int) * nSites, "get_activated_tsuids", LogInfo
        );
        if (LogInfo->stopRun) {
            goto freeMem;
        }

        if (!inSDom) {
            yIndexVals = (unsigned int *) Mem_Malloc(
                sizeof(unsigned int) * nSites, "get_activated_tsuids", LogInfo
            );
            if (LogInfo->stopRun) {
                goto freeMem;
            }
        }

        varID = -1;
        SW_NC_get_vals(
            fileID,
            &varID,
            (inSDom) ? "site_index" : "x_index",
            sxIndexVals,
            LogInfo
        );
        if (LogInfo->stopRun) {
            goto freeMem;
        }

        if (!inSDom) {
            varID = -1;
            SW_NC_get_vals(fileID, &varID, "y_index", yIndexVals, LogInfo);
            if (LogInfo->stopRun) {
                goto freeMem;
            }
        }

        allocateActiveTSuids(numActiveSites, &activeTSuids[inKey], LogInfo);
        if (LogInfo->stopRun) {
            goto freeMem;
        }

        for (site = 0; site < (int) numActiveSites; site++) {
            domSuid = activeSuids[site];
            indexCell = activeTSuids[inKey][site];

            /*
                Translate a domain suid for a site into a translated suid
                since we are using an index file
                e.g., [0, 3] -> [1, 0]
             */
            if (inSDom) {
                indexCell[0] = sxIndexVals[domSuid[0]];
            } else {
                offset = (sProgDom) ?
                             domSuid[0] :
                             (domSuid[0] * SW_Domain->nDimX) + domSuid[1];

                indexCell[0] = yIndexVals[offset];
                indexCell[1] = sxIndexVals[offset];
            }
        }

        if (!isnull(sxIndexVals)) {
            free(sxIndexVals);
            sxIndexVals = NULL;
        }

        if (!isnull(yIndexVals)) {
            free(yIndexVals);
            yIndexVals = NULL;
        }

        nc_close(fileID);
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
@brief Free allocated log memory in I/O processes

@param[in,out] logs A list of LOG_INFO instances that will be used for
    getting log information from compute processes
*/
static void free_logs(FILE ***logfps) {
    if (!isnull(*logfps)) {
        free((void *) *logfps);
        *logfps = NULL;
    }
}

/* =================================================== */
/*             Global Function Definitions             */
/* --------------------------------------------------- */

/**
@brief Intiialize the MPI program by getting basic information about
the rank, world size, and processor/node name, while setting the
MPI handler method to return from a function call rather than
crashing the program and finally initializing MPI information in SW_DOMAIN

@param[in] argc Number of command-line provided inputs
@param[in] argv List of command-line provided inputs
@param[out] rank Process number known to MPI for the current process (aka rank)
@param[out] worldSize Total number of processes that the MPI run has created
@param[out] procName Name of the processor/node the current processes is
running on
@param[out] desig Designation instance that holds information about
    assigning a process to a job
@param[out] datatypes A list of custom MPI datatypes that will be created based
on various program-defined structs
*/
void SW_MPI_initialize(
    int *argc,
    char ***argv,
    int *rank,
    int *worldSize,
    char *procName,
    SW_MPI_DESIGNATE *desig,
    MPI_Datatype datatypes[]
) {
    int procNameSize = 0;
    int type;
    int inKey;

    MPI_Init(argc, argv);

    MPI_Comm_rank(MPI_COMM_WORLD, rank);
    MPI_Comm_size(MPI_COMM_WORLD, worldSize);

    MPI_Get_processor_name(procName, &procNameSize);

    MPI_Comm_set_errhandler(MPI_COMM_WORLD, MPI_ERRORS_RETURN);

    for (type = 0; type < SW_MPI_NTYPES; type++) {
        datatypes[type] = MPI_DATATYPE_NULL;
    }

    ForEachNCInKey(inKey) { desig->domTSuids[inKey] = NULL; }

    desig->domSuids = NULL;
    desig->nSuids = 0;
    desig->nCompProcs = 0;
    desig->useTSuids = swFALSE;
    desig->procJob = SW_MPI_PROC_COMP;
    desig->groupComm = MPI_COMM_NULL;
    desig->ioCompComm = MPI_COMM_NULL;
    desig->rootCompComm = MPI_COMM_NULL;

    desig->nTotCompProcs = 0;
    desig->nTotIOProcs = 0;
    desig->ioRank = 0;
}

/**
@brief Conclude the program run by finalizing/freeing anything that's
been initialized/created through MPI within the program run

@param[in] procJob Process job designation used when using MPI
@param[in,out] LogInfo Holds information on warnings and errors
*/
void SW_MPI_finalize(int procJob, LOG_INFO *LogInfo) {
    if (procJob == SW_MPI_PROC_IO) {
        free_logs(&LogInfo->logfps);
    }

    MPI_Finalize();
}

/**
@brief Free communicators and types when finishing the program

@param[in,out] desig Designation instance that holds information about
    assigning a process to a job
@param[in,out] types A list of custom MPI datatypes used throughout the program
@param[out] LogInfo Holds information on warnings and errors; this function
    will use this but no errors will be reported
*/
void SW_MPI_free_comms_types(
    SW_MPI_DESIGNATE *desig, MPI_Datatype types[], LOG_INFO *LogInfo
) {
    const int numComms = 3;
    int comm;
    int type;
    MPI_Comm *comms[] = {
        &desig->rootCompComm, &desig->groupComm, &desig->ioCompComm
    };

    for (type = 0; type < SW_MPI_NTYPES; type++) {
        if (types[type] != MPI_DATATYPE_NULL) {
            free_type(&types[type], LogInfo);
        }
    }

    for (comm = 0; comm < numComms; comm++) {
        if (*comms[comm] != MPI_COMM_NULL) {

            MPI_Comm_free(comms[comm]);
        }
    }
}

/**
@brief Trigger an abort error when a fatal error occurs

Options for failing the program
    - NetCDF read/write error
    - Too many simulation errors
    - OpenMPI error

Moving forward, an option for further development of this part of the
program is to allow for a hardcoded number of netCDF read errors

@param[in] rank Process number known to MPI for the current process (aka rank)
@param[in] failType Reason why the program failed
@param[in] mpiErrStr String representation of MPI error (not used
    if MPI did not cause the error)
*/
void SW_MPI_Fail(int rank, int failType, char *mpiErrStr) {
    const char *ncFail = "SOILWAT2 failed due to a netCDF error.";
    const char *compFail =
        "SOILWAT2 failed due to too many errors during simulations.";
    const char *mpiFailAdd = "SOILWAT2 failed due to an OpenMPI problem:";
    char mpiFail[FILENAME_MAX] = "\0";

    char *failStr;

    switch (failType) {
    case SW_MPI_FAIL_NETCDF:
        failStr = (char *) ncFail;
        break;
    case SW_MPI_FAIL_COMP_ERR:
        failStr = (char *) compFail;
        break;
    default: // SW_MPI_FAIL_MPI
        snprintf(
            mpiFail,
            sizeof(mpiFail),
            "%s \"%s\".",
            (char *) mpiFailAdd,
            mpiErrStr
        );
        failStr = mpiFail;
        break;
    }

    if (failType != SW_MPI_FAIL_MPI) {
        (void
        ) fprintf(stderr, "An error occured: %s (rank %d)\n", failStr, rank);
    } else {
        (void) fprintf(stderr, "An error occured: %s\n", failStr);
    }
}

/**
@brief Deconstruct MPI-related information in domain

@param[in,out] SW_Domain Struct of type SW_DOMAIN holding constant
    temporal/spatial information for a set of simulation runs
*/
void SW_MPI_deconstruct(SW_DOMAIN *SW_Domain) {
    size_t suid;
    int inKey;

    ForEachNCInKey(inKey) {
        if (!isnull(SW_Domain->domSuids[inKey])) {
            for (suid = 0; suid < SW_Domain->nProcSuids; suid++) {
                if (!isnull(SW_Domain->domSuids[inKey][suid])) {
                    free((void *) SW_Domain->domSuids[inKey][suid]);
                    SW_Domain->domSuids[inKey][suid] = NULL;
                }
            }

            free((void *) SW_Domain->domSuids[inKey]);
            SW_Domain->domSuids[inKey] = NULL;
        }
    }
}

/**
@brief Wrapper function to `MPI_Allreduce()`

@param[in] datatype Custom MPI datatype that will be sent
@param[in] src Location of memory that will be read from to give
    `MPI_Allreduce()` this process' information
@param[out] dest Location of memory that will be written from to by
    `MPI_Allreduce()`
@param[in] count Number of items to recv from the buffer
@param[in] op MPI operation to be performed
@param[in] comm MPI communicator to broadcast a message to
*/
void SW_MPI_Allreduce(
    MPI_Datatype datatype,
    void *src,
    void *dest,
    int count,
    MPI_Op op,
    MPI_Comm comm
) {
    int mpiRes = MPI_SUCCESS;

    mpiRes = MPI_Allreduce(src, dest, count, datatype, op, comm);

    if (mpiRes != MPI_SUCCESS) {
        errorMPI(-1, mpiRes);
    }
}

/**
@brief Wrapper function to the Open MPI function MPI_Scatter

@param[in] comm MPI communicator to scatter information across
@param[in] buffer Source buffer than information will be scattered from
@param[in] src Source rank that will do the scattering
@param[in] sendCount The number of elements that will be scattered from
"buffer"
@param[in] recvCount The number of elements that each process will receive
@param[out] dest Destination buffer that will be filled with received
information
*/
void SW_MPI_Scatter(
    MPI_Comm comm,
    void *buffer,
    int src,
    int sendCount,
    int recvCount,
    void *dest
) {
    int mpiRes = MPI_Scatter(
        buffer,
        sendCount,
        SW_MPI_SIZE_T,
        dest,
        recvCount,
        SW_MPI_SIZE_T,
        src,
        comm
    );

    if (mpiRes != MPI_SUCCESS) {
        errorMPI(-1, mpiRes);
    }
}

/**
@brief Wrapper function to the Open MPI function MPI_Barrier

@param[in] comm MPI communicator to create a barrier for
*/
void SW_MPI_Barrier(MPI_Comm comm) {
    int mpiRes = MPI_Barrier(comm);

    if (mpiRes != MPI_SUCCESS) {
        errorMPI(-1, mpiRes);
    }
}

/**
@brief Broadcast a message/type to all processes

@param[in] datatype Custom MPI datatype that will be sent
@param[in] buffer Location of memory that will be written to
@param[in] count Number of items to recv from the buffer
@param[in] srcRank Source rank that information will be sent from
with any message tag values
@param[in] comm MPI communicator to broadcast a message to
*/
void SW_MPI_Bcast(
    MPI_Datatype datatype, void *buffer, int count, int srcRank, MPI_Comm comm
) {
    int mpiRes = MPI_SUCCESS;

    mpiRes = MPI_Bcast(buffer, count, datatype, srcRank, comm);

    if (mpiRes != MPI_SUCCESS) {
        errorMPI(-1, mpiRes);
    }
}

/**
@brief Wrapper for MPI-provided function `MPI_Send()` and reduces
the ambiguity of how the information gets sent (no longer buffer or
synchronous)

@param[in] datatype Custom MPI datatype that will be sent
@param[in] buffer Location of memory that holds the memory that
we will be sending
@param[in] count Number of items to send from the buffer
@param[in] destRank Destination rank that information will be sent to
@param[in] sync Specifies if the send should be synchronous or
asynchronous
@param[in] tag Unique identifier for a message that does not interact
with any message tag values
@param[in,out] request Type MPI_Request that holds information on a
previous asynchronous send
*/
void SW_MPI_Send(
    MPI_Datatype datatype,
    void *buffer,
    int count,
    int destRank,
    Bool sync,
    int tag,
    MPI_Request *request
) {
    int result = 0;

    if (sync) {
        result =
            MPI_Ssend(buffer, count, datatype, destRank, tag, MPI_COMM_WORLD);
    } else {
        if (*request != MPI_REQUEST_NULL) {
            result = MPI_Wait(request, MPI_STATUS_IGNORE);
            if (result != MPI_SUCCESS) {
                goto error;
            }

            result = MPI_Request_free(request);
            if (result != MPI_SUCCESS) {
                goto error;
            }
        }

        result = MPI_Issend(
            buffer, count, datatype, destRank, tag, MPI_COMM_WORLD, request
        );
    }

error: {
    if (result != MPI_SUCCESS) {
        errorMPI(-1, result);
    }
}
}

/**
@brief Wrapper for MPI-provided function `MPI_Recv()` and reduces
the ambiguity of how the information gets sent (no longer buffer or
synchronous)

@param[in] datatype Custom MPI datatype that will be sent
@param[in] buffer Location of memory that will be written to
@param[in] count Number of items to recv from the buffer
@param[in] srcRank Source rank that information will be received from
@param[in] sync Specifies if the receive should be synchronous or
asynchronous
@param[in] tag Unique identifier for a message that does not interact
with any message tag values
@param[in,out] request Type MPI_Request that holds information on a
previous asynchronous receive
*/
void SW_MPI_Recv(
    MPI_Datatype datatype,
    void *buffer,
    int count,
    int srcRank,
    Bool sync,
    int tag,
    MPI_Request *request
) {
    int result = 0;

    if (sync) {
        result = MPI_Recv(
            buffer,
            count,
            datatype,
            srcRank,
            tag,
            MPI_COMM_WORLD,
            MPI_STATUS_IGNORE
        );
    } else {
        if (*request != MPI_REQUEST_NULL) {
            result = MPI_Wait(request, MPI_STATUS_IGNORE);
            if (result != MPI_SUCCESS) {
                goto error;
            }

            result = MPI_Request_free(request);
            if (result != MPI_SUCCESS) {
                goto error;
            }
        }

        result = MPI_Irecv(
            buffer, count, datatype, srcRank, tag, MPI_COMM_WORLD, request
        );
    }

error: {
    if (result != MPI_SUCCESS) {
        errorMPI(-1, result);
    }
}
}

/**
@brief Before we proceed to the next important section of the program,
we must do a check-in with all processes to make sure no errors occurred

@param[in] stopRun A flag specifying if an error occurred and stop the run
@param[in] comm MPI communicator to broadcast a message to
*/
Bool SW_MPI_setup_fail(Bool stopRun, MPI_Comm comm) {
    int fail = (stopRun) ? 1 : 0;
    int failProgram = 0;

    SW_MPI_Allreduce(MPI_INT, &fail, &failProgram, 1, MPI_SUM, comm);

    return (Bool) (failProgram != 0);
}

/**
@brief Report any log information that has been created throughout the
program run either through I/O processes or the root process

Assuming setup did not fail, average domain simulation information from
compute processes; if setup did fail, report any log information to the
general log file the root process holds

@param[in] rank Process number known to MPI for the current process (aka rank)
@param[in] size Number of processors (world size) within the
    communicator MPI_COMM_WORLD
@param[in] wtType Custom MPI datatype for SW_WALLTIME
@param[in] SW_WallTime Struct of type SW_WALLTIME that holds timing
    information for the program run
@param[in] SW_Domain Struct of type SW_DOMAIN holding constant
    temporal/spatial information for a set of simulation runs
@param[in] failedSetup Specifies if the setup process failed
@param[in] LogInfo Holds information on warnings and errors
*/
void SW_MPI_report_log(
    int rank,
    int size,
    MPI_Datatype wtType,
    SW_WALLTIME *SW_WallTime,
    SW_DOMAIN *SW_Domain,
    Bool failedSetup,
    LOG_INFO *LogInfo
) {
    SW_MPI_DESIGNATE *desig = &SW_Domain->SW_Designation;
    MPI_Datatype logType = SW_Domain->datatypes[eSW_MPI_Log];
    MPI_Request req = MPI_REQUEST_NULL;
    int numRanks = 0;
    double prevMean = 0.0;
    int destProcJob = SW_MPI_PROC_COMP;
    int destRank;
    Bool reportLog = (Bool) ((LogInfo->stopRun || LogInfo->numWarnings > 0 ||
                              LogInfo->numDomainWarnings > 0 ||
                              LogInfo->numDomainErrors > 0) &&
                             failedSetup);
    Bool destReport = swFALSE;
    char warnHeader[MAX_FILENAMESIZE] = "\0";
    FILE *tempFilePtr = LogInfo->logfp;

    if (rank == SW_MPI_ROOT) {
        if (reportLog) {
            if (!isnull(LogInfo->logfps)) {
                tempFilePtr = LogInfo->logfp;
                LogInfo->logfp = LogInfo->logfps[0];
            }

            sw_write_warnings("(Rank 0) ", LogInfo);
            LogInfo->logfp = tempFilePtr;
        }

        SW_WallTime->nTimedRuns = 0;
        SW_WallTime->nUntimedRuns = 0;

        /* Get timing information to average in root processes */
        for (destRank = 1; destRank < size; destRank++) {
            SW_WALLTIME rankWT;
            LOG_INFO procLog;

            SW_MPI_Recv(MPI_INT, &destProcJob, 1, destRank, swTRUE, 0, &req);
            SW_MPI_Recv(wtType, &rankWT, 1, destRank, swTRUE, 0, &req);

            SW_WallTime->totIOCompTime += rankWT.totIOCompTime;

            if (!failedSetup) {
                if (destProcJob == SW_MPI_PROC_COMP) {
                    SW_MPI_Recv(
                        SW_MPI_SIZE_T,
                        &rankWT.nTimedRuns,
                        1,
                        destRank,
                        swTRUE,
                        0,
                        &req
                    );
                    SW_MPI_Recv(
                        SW_MPI_SIZE_T,
                        &rankWT.nUntimedRuns,
                        1,
                        destRank,
                        swTRUE,
                        0,
                        &req
                    );

                    numRanks++;

                    prevMean = SW_WallTime->timeMean;
                    SW_WallTime->timeMean = get_running_mean(
                        numRanks, SW_WallTime->timeMean, rankWT.timeMean
                    );
                    SW_WallTime->timeMax = get_running_mean(
                        numRanks, SW_WallTime->timeMax, rankWT.timeMax
                    );
                    SW_WallTime->timeMin = get_running_mean(
                        numRanks, SW_WallTime->timeMin, rankWT.timeMin
                    );
                    SW_WallTime->timeSS = get_running_sqr(
                        prevMean, SW_WallTime->timeMean, rankWT.timeMean
                    );

                    SW_WallTime->nTimedRuns += rankWT.nTimedRuns;
                    SW_WallTime->nUntimedRuns += rankWT.nUntimedRuns;
                } else {
                    SW_WallTime->totIOTime += rankWT.totIOTime;
                }
            }

            SW_MPI_Recv(MPI_INT, &destReport, 1, destRank, swTRUE, 0, &req);

            if (destReport) {
                // Get logs that need to be reported to general
                // log file
                sw_init_logs(LogInfo->logfp, &procLog);

                SW_MPI_Recv(logType, &procLog, 1, destRank, swTRUE, 0, &req);

                snprintf(warnHeader, MAX_FILENAMESIZE, "(Rank %d) ", destRank);

                sw_write_warnings(warnHeader, &procLog);

                LogInfo->numDomainErrors += procLog.numDomainErrors;
                LogInfo->numDomainWarnings += procLog.numDomainWarnings;
            }
        }

        if (!failedSetup) {
            SW_WT_ReportTime(*SW_WallTime, LogInfo);
        }
    } else {
        /* Send process job to root process */
        SW_MPI_Send(MPI_INT, &desig->procJob, 1, SW_MPI_ROOT, swTRUE, 0, &req);
        SW_MPI_Send(wtType, SW_WallTime, 1, SW_MPI_ROOT, swTRUE, 0, &req);

        if (desig->procJob == SW_MPI_PROC_COMP && !failedSetup) {
            /* Send timing information to the root process to average it */
            /* TODO: Find the reason why sending SW_WALLTIME with
               n(Un)TimedRuns across nodes in an HPC environment results in
               a floating-point exception */
            SW_MPI_Send(
                SW_MPI_SIZE_T,
                &SW_WallTime->nTimedRuns,
                1,
                SW_MPI_ROOT,
                swTRUE,
                0,
                &req
            );
            SW_MPI_Send(
                SW_MPI_SIZE_T,
                &SW_WallTime->nUntimedRuns,
                1,
                SW_MPI_ROOT,
                swTRUE,
                0,
                &req
            );
        }

        // Send information to the root process
        SW_MPI_Send(MPI_INT, &reportLog, 1, SW_MPI_ROOT, swTRUE, 0, &req);

        if (reportLog) {
            SW_MPI_Send(logType, LogInfo, 1, SW_MPI_ROOT, swTRUE, 0, &req);
        }
    }
}

/*
@brief Get/send the main log information from compute processes to their
    assigned I/O process once the simulations are finished; mainly useful
    if anything causes a fatal error

@param[in] desig Designation instance that holds information about
    assigning a process to a job
@param[in] logType Custom MPI type that mimics LOG_INFO
@param[in] LogInfo Holds information on warnings and errors
*/
void SW_MPI_write_main_logs(
    SW_MPI_DESIGNATE *desig, MPI_Datatype logType, LOG_INFO *LogInfo
) {
    MPI_Request nullReq = MPI_REQUEST_NULL;

    int rank;
    int destRank;

    if (desig->procJob == SW_MPI_PROC_IO) {
        LogInfo->logfp = LogInfo->logfps[0];
        sw_write_warnings("", LogInfo);

        for (rank = 0; rank < desig->nCompProcs; rank++) {
            LOG_INFO main_log;
            destRank = desig->ranks[rank];

            sw_init_logs(LogInfo->logfp, &main_log);
            SW_MPI_Recv(logType, &main_log, 1, destRank, swTRUE, 0, &nullReq);

            if (main_log.stopRun || main_log.numWarnings > 0) {
                main_log.logfp = LogInfo->logfps[rank + 1];
                sw_write_warnings("", &main_log);
            }
        }
    } else {
        SW_MPI_Send(logType, LogInfo, 1, desig->ioRank, swTRUE, 0, &nullReq);
    }
}

/**
@brief Assign processes to compute or I/O jobs

@param[in] rank Process number known to MPI for the current process (aka rank)
@param[in] worldSize Number of processes that was created that make up
    the world
@param[in] SW_Domain Struct of type SW_DOMAIN holding constant
    temporal/spatial information for a set of simulation runs
@param[out] LogInfo Holds information on warnings and errors
*/
void SW_MPI_proc_workload(
    int rank, int worldSize, SW_DOMAIN *SW_Domain, LOG_INFO *LogInfo
) {
    const int twoDomVarIDs = 2;
    size_t **activeTSuids[SW_NINKEYSNC] = {NULL};
    size_t **activeSuids = NULL;
    size_t *nSuidsAssign = NULL;
    size_t *numActiveSites = &SW_Domain->nActiveSuids;

    // Spread the index file creation flags across the world;
    // necessary if we use translated SUIDs and we have not
    // all processes have received them yet
    SW_MPI_Bcast(
        MPI_INT,
        SW_Domain->netCDFInput.ncDomVarIDs,
        twoDomVarIDs,
        SW_MPI_ROOT,
        MPI_COMM_WORLD
    );

    find_active_sites(rank, SW_Domain, &activeSuids, numActiveSites, LogInfo);
    if (SW_MPI_setup_fail(LogInfo->stopRun, MPI_COMM_WORLD)) {
        goto freeMem;
    }

    if (rank == SW_MPI_ROOT) {
        if (*numActiveSites == 0) {
            LogError(LogInfo, LOGERROR, "No active sites to simulate.");
        } else if (*numActiveSites < (size_t) worldSize) {
            LogError(
                LogInfo,
                LOGERROR,
                "Fewer active sites (%d) were found than spawned processes "
                "(%d).",
                *numActiveSites,
                worldSize
            );
        }
        if (LogInfo->stopRun) {
            goto checkForError;
        }

        get_activated_tsuids(
            SW_Domain, activeSuids, activeTSuids, *numActiveSites, LogInfo
        );
        if (LogInfo->stopRun) {
            goto checkForError;
        }

        calcNumSites(*numActiveSites, worldSize, &nSuidsAssign, LogInfo);
    }

checkForError:
    if (SW_MPI_setup_fail(LogInfo->stopRun, MPI_COMM_WORLD)) {
        goto freeMem;
    }

    assignProcs(
        rank,
        nSuidsAssign,
        activeSuids,
        activeTSuids,
        worldSize,
        SW_Domain->netCDFInput.useIndexFile,
        SW_Domain,
        LogInfo
    );

freeMem:
    deallocProcHelpers(
        *numActiveSites, &activeSuids, activeTSuids, &nSuidsAssign
    );
}

/**
@brief Organize output data when completing a simulation run into
    a bigger output buffer to later send to an I/O process

Process designation: Compute

@param[in] runNum Current run number the compute process is on,
    should be between [0, N_SUID_ASSIGN - 1]
@param[in] OutDom Struct of type SW_OUT_DOM that holds output
    information that do not change throughout simulation runs
@param[in] src_p_OUT Source of accumulated output values throughout
    simulation runs
@param[out] dest_p_OUT Destination of accumulated output values throughout
    simulation runs
*/
void SW_MPI_store_outputs(
    size_t runNum,
    SW_OUT_DOM *OutDom,
    double *src_p_OUT[][SW_OUTNPERIODS],
    double *dest_p_OUT[][SW_OUTNPERIODS]
) {
    size_t startIndex;
    size_t oneSiteSize;
    int outKey;
    OutPeriod timeStep;
    int ipd;

    ForEachOutKey(outKey) {
        for (ipd = 0; ipd < OutDom->used_OUTNPERIODS; ipd++) {
            timeStep = OutDom->timeSteps[outKey][ipd];

            if (OutDom->use[outKey] && timeStep != eSW_NoTime) {
                oneSiteSize =
                    OutDom->nrow_OUT[timeStep] *
                    (OutDom->ncol_OUT[outKey] + ncol_TimeOUT[timeStep]);
                startIndex = oneSiteSize * runNum;

                memcpy(
                    &dest_p_OUT[outKey][timeStep][startIndex],
                    src_p_OUT[outKey][timeStep],
                    sizeof(double) * oneSiteSize
                );
            }
        }
    }
}

/**
@brief Send information to I/O processes when a compute
    process is some with their assigned workload

This function will send three pieces of information
    1) Simulation run statuses
        - Flag results collected throughout the simulation runs
        - Specifies if a specific run failed for succeeded
        - Used to write output and update the progress file properly
            * If a simulation run failed, do not output it and report it
    2) Request type
        - Three different types of requests
            * REQ_OUT_NC - Only send output information to be written to file
            * REQ_OUT_LOG - Only send log information to be written to log
                            files; only occurs when all simulations fail
            * REQ_OUT_BOTH - Both take action above
    3) Source rank
        - Rank of the process that sent information to I/O process
            * I/O process is not aware of the sender by default due to
                MPI_ANY_SOURCE

Further improvement upon this functionality could be sending outputs
for the current batch of outputs while doing other operations
(asynchronous)

Process designation: Compute

@param[in] OutDom Struct of type SW_OUT_DOM that holds output
    information that do not change throughout simulation runs
@param[in] rank Process number known to MPI for the current process (aka rank)
@param[in] numInputs Number of inputs that were sent to an I/O process
@param[in] ioRank Destination rank of the I/O process to send to
@param[in] reqTypeMPI Custom MPI type that mimics SW_MPI_REQUEST
@param[in] logType Custom MPI type that mimics LOG_INFO
@param[in] runStatuses A list of run statuses for each simulation
    (success or fail)
@param[in] reportLog A flag specifying that something was reported within
    the simulations and must be sent
@param[in] logs A list of LOG_INFO instances to send to the I/O process
    if the `reportLog` flag is turned on
@param[in] p_OUT Array of accumulated output values throughout
    simulation runs
*/
void SW_MPI_send_results(
    SW_OUT_DOM *OutDom,
    int rank,
    size_t numInputs,
    int ioRank,
    MPI_Datatype reqTypeMPI,
    MPI_Datatype logType,
    const Bool runStatuses[],
    Bool reportLog,
    LOG_INFO logs[],
    double *p_OUT[][SW_OUTNPERIODS]
) {
    MPI_Request nullReq = MPI_REQUEST_NULL;
    int reqType = REQ_OUT_NC;
    Bool succRun = swFALSE;
    int run;
    SW_MPI_REQUEST req;
    int outKey;
    int timeStep;
    int ipd;
    size_t sendSize = 0;

    for (run = 0; run < N_SUID_ASSIGN; run++) {
        req.runStatus[run] = runStatuses[run];
        succRun = (Bool) (succRun || runStatuses[run]);
    }

    if (succRun && reportLog) {
        reqType = REQ_OUT_BOTH;
    } else if (reportLog) {
        reqType = REQ_OUT_LOG;
    }

    req.sourceRank = rank;
    req.requestType = reqType;

    SW_MPI_Send(reqTypeMPI, &req, 1, ioRank, swTRUE, 0, &nullReq);

    if (reportLog) {
        SW_MPI_Send(
            logType, logs, (int) numInputs, ioRank, swTRUE, 0, &nullReq
        );
    }

    if (succRun) {
        ForEachOutKey(outKey) {
            for (ipd = 0; ipd < OutDom->used_OUTNPERIODS; ipd++) {
                timeStep = OutDom->timeSteps[outKey][ipd];

                if (OutDom->use[outKey] && timeStep != eSW_NoTime) {
                    sendSize =
                        OutDom->nrow_OUT[timeStep] *
                        (OutDom->ncol_OUT[outKey] + ncol_TimeOUT[timeStep]);
                    sendSize *= numInputs;

                    SW_MPI_Send(
                        MPI_DOUBLE,
                        p_OUT[outKey][timeStep],
                        (int) sendSize,
                        ioRank,
                        swTRUE,
                        0,
                        &nullReq
                    );

                    memset(
                        p_OUT[outKey][timeStep], 0, sizeof(double) * sendSize
                    );
                }
            }
        }
    }
}

/**
@brief Handle the receiving of inputs from an I/O process

This function will receive up to N_SUID_ASSIGN inputs from the
assigned I/O process

Further improvement upon this functionality could be getting inputs
for the next batch of inputs during simulation runs (asynchronous)

Process designation: Compute

@param[in] getWeather Specifies if we get weather from I/O process;
    if not, we copy it from the SW_RUN template
@param[in] n_years Number of years of weather to receive if we do
    get weather from the I/O process
@param[in] desig Designation instance that holds information about
    assigning a process to a job
@param[in] inputType Custom MPI data type for sending SW_RUN_INPUTS
@param[in] weathHistType Custom MPI type for transfering data for
    SW_WEATHER_HIST
@param[out] inputs A list of SW_RUN_INPUTS that will be filled by an
    I/O process
@param[out] numInputs Number of inputs that were sent to this process
@param[out] extraFailCheck Specifies if, when a compute process is done with
    all workloads, the compute process should take part in an extra call to
    `SW_MPI_setup_fail()`
*/
void SW_MPI_get_inputs(
    Bool getWeather,
    unsigned int n_years,
    SW_MPI_DESIGNATE *desig,
    MPI_Datatype inputType,
    MPI_Datatype weathHistType,
    SW_RUN_INPUTS inputs[],
    size_t *numInputs,
    Bool *extraFailCheck
) {
    MPI_Request nullReq = MPI_REQUEST_NULL;
    size_t input;

    SW_MPI_Recv(
        SW_MPI_SIZE_T, numInputs, 1, desig->ioRank, swTRUE, 0, &nullReq
    );

    for (input = 0; input < *numInputs; input++) {
        SW_MPI_Recv(
            inputType, &inputs[input], 1, desig->ioRank, swTRUE, 0, &nullReq
        );
    }

    if (*numInputs == COMP_COMPLETE) {
        SW_MPI_Recv(
            MPI_INT, extraFailCheck, 1, desig->ioRank, swTRUE, 0, &nullReq
        );
    }

    if (getWeather) {
        for (input = 0; input < *numInputs; input++) {
            SW_MPI_Recv(
                weathHistType,
                inputs[input].weathRunAllHist,
                (int) n_years,
                desig->ioRank,
                swTRUE,
                0,
                &nullReq
            );
        }
    }
}

/**
@brief Transfer suids from the domain of a process to a smaller list
of suids that will be used for simulating sites

@param[in] domSuids A list of domain suids for each input key, including
those that are translated
@param[in] useIndexFile A list of size SW_NINKEYSNC that specifies if an input
key is to use an index file
@param[in,out] readIndex Specifies the index in which the reading from
"domSuids" should take place; return the updated index for the next call to
this function
@param[in,out] nSuidsLeft Specifies how many suids there are left to simulate
for a process; return an updated value of how many suids are left after the
transfers
@param[out] simSuids Resulting list of transferred domain/translated domain
suids for each input key
@param[out] nSuids Specifies the number of suids to be simulated this cycle
*/
void SW_MPI_get_sim_suids(
    size_t **domSuids[],
    Bool useIndexFile[],
    size_t *readIndex,
    size_t *nSuidsLeft,
    size_t simSuids[SW_NINKEYSNC][N_SUID_ASSIGN][2],
    size_t *nSuids
) {
    const int xsCoordIndex = 0;
    const int yCoordIndex = 1;

    int inKey;
    size_t suid;
    size_t *destSuid;
    size_t *srcSuid;
    size_t readStart = *readIndex;

    *nSuids = (*nSuidsLeft < N_SUID_ASSIGN) ? *nSuidsLeft : N_SUID_ASSIGN;

    ForEachNCInKey(inKey) {
        for (suid = 0; suid < *nSuids; suid++) {
            destSuid = simSuids[inKey][suid];
            srcSuid = (useIndexFile[inKey] && inKey > eSW_InDomain) ?
                          domSuids[inKey][readStart + suid] :
                          domSuids[eSW_InDomain][readStart + suid];

            destSuid[xsCoordIndex] = srcSuid[xsCoordIndex];
            destSuid[yCoordIndex] = srcSuid[yCoordIndex];
        }
    }

    *readIndex += *nSuids;
    *nSuidsLeft -= *nSuids;
}

/**
@brief Wrapper function to read inputs when SWMPI is enabled

@param[in,out] sw Comprehensive struct of type SW_RUN containing
all information in the simulation
@param[in,out] SW_Domain Struct of type SW_DOMAIN holding constant
temporal/spatial information for a set of simulation runs
@param[in] tempVals A list that holds the maximum amount of elements
of all input keys; this array is returned with values in it, but is not
used outside of reading netCDF inputs
@param[in,out] readIndex Specifies the index in which the reading from
"domSuids" should take place; return the updated index for the next call to
this function
@param[out] simSuids Resulting list of transferred domain/translated domain
suids for each input key
@param[out] nSuids Specifies the number of suids to be simulated this cycle
@param[out] starts A list of size SW_NINKEYSNC specifying the start
    indices used when reading/writing using the netCDF library;
    default size is `nSuids` but as mentioned in `numWrites`, it would
    be best to not fill this array
@param[out] counts A list of size SW_NINKEYSNC specifying the count
    indices used when reading/writing using the netCDF library;
    default size is `nSuids` but as mentioned in `numWrites`, it would
    be best to not fill this array; counts match placements with `start`
    indices for each key
@param[out] numReads A list of size SW_NINKEYSNC specifying how many reads
for a specific input key is necessary
@param[out] tempSoils A list that holds temporary storage for soils of
type SW_SOIL_RUN_INPUTS of size [num sites] (1 if not SWMPI)
@param[out] runInputs A list of SW_RUN_INPUTS structs that will be filled
with input data from netCDFs
@param[out] SW_WallTime Struct of type SW_WALLTIME that holds timing
    information for the program run including partitioning into
    I/O (SWNETCDF) and compute (SWNETCDF, SWMPI) times
@param[out] LogInfo Holds information on warnings and errors
*/
void SW_MPI_read_inputs(
    SW_RUN *sw,
    SW_DOMAIN *SW_Domain,
    double *tempVals,
    size_t *readIndex,
    size_t simSuids[SW_NINKEYSNC][N_SUID_ASSIGN][2],
    size_t *nSuids,
    size_t starts[SW_NINKEYSNC][N_SUID_ASSIGN][2],
    size_t counts[SW_NINKEYSNC][N_SUID_ASSIGN][2],
    size_t numReads[],
    SW_SOIL_RUN_INPUTS *tempSoils,
    SW_RUN_INPUTS *runInputs,
    SW_WALLTIME *SW_WallTime,
    LOG_INFO *LogInfo
) {
    int inKey;
    size_t suid;
    size_t origNSuids = SW_Domain->nProcSuids;
    WallTimeSpec tsr;
    Bool ok_tsr = swFALSE;

    SW_MPI_get_sim_suids(
        SW_Domain->domSuids,
        SW_Domain->netCDFInput.useIndexFile,
        readIndex,
        &SW_Domain->nProcSuids,
        simSuids,
        nSuids
    );

    calculate_contiguous_allkeys(
        *nSuids,
        origNSuids,
        SW_Domain->netCDFInput.useIndexFile,
        SW_Domain->netCDFInput.readInVars,
        simSuids,
        SW_Domain->netCDFInput.siteDoms,
        numReads,
        starts,
        counts
    );

    if (origNSuids == 0) {
        ForEachNCInKey(inKey) {
            for (suid = 0; suid < N_SUID_ASSIGN; suid++) {
                counts[inKey][suid][0] = counts[inKey][suid][1] = 0;
            }
        }
    }

    set_walltime(&tsr, &ok_tsr);
    SW_NCIN_read_inputs(
        sw,
        SW_Domain,
        NULL,
        starts,
        counts,
        SW_Domain->SW_PathInputs.openInFileIDs,
        numReads,
        *nSuids,
        tempVals,
        simSuids[eSW_InDomain],
        tempSoils,
        runInputs,
        LogInfo
    );
    SW_WT_TimeRun(tsr, ok_tsr, TIME_IO, SW_WallTime);
}

/**
@brief Wrapper function to calculate output starts/counts and
    output all values

@param[in] SW_PathOutputs Struct of type SW_PATH_OUTPUTS which
    holds basic information about output files and values
@param[in] progFileID Identifier of the progress netCDF file
@param[in] progVarID Identifier of the progress variable
@param[in] main_p_OUT Array of accumulated output values throughout
    simulation all runs
@param[in] temp_p_OUT Array of accumulated output values throughout
    simulation all runs; only used if N_SUID_ASSIGN > 1
@param[in] distSUIDs A list of domain SUIDs whose data will be distributed
    as the next batch of input
@param[in] numSuids Number of SUIDs that will be assigned, this should be
    the product of the <number of compute processors for the I/O process>
    and N_SUID_ASSIGN
@param[in] siteDom Specifies that the programs domain has sites, otherwise
    it is gridded
@param[in] OutDom Struct of type SW_OUT_DOM that holds output
    information that do not change throughout simulation runs
@param[in] succFlags Accumulator array of flags specifying how respective
    simulation runs went
@param[out] starts A list of size SW_NINKEYSNC specifying the start
    indices used when reading/writing using the netCDF library;
    default size is `nSuids` but as mentioned in `numWrites`, it would
    be best to not fill this array
@param[out] counts A list of size SW_NINKEYSNC specifying the count
    indices used when reading/writing using the netCDF library;
    default size is `nSuids` but as mentioned in `numWrites`, it would
    be best to not fill this array; counts match placements with `start`
    indices for each key
@param[out] SW_WallTime Struct of type SW_WALLTIME that holds timing
    information for the program run including partitioning into
    I/O (SWNETCDF) and compute (SWNETCDF, SWMPI) times
@param[out] LogInfo Holds information on warnings and errors
*/
void SW_MPI_write_outputs(
    SW_PATH_OUTPUTS *SW_PathOutputs,
    int progFileID,
    int progVarID,
    double *main_p_OUT[][SW_OUTNPERIODS],
    double *temp_p_OUT[][SW_OUTNPERIODS],
    size_t distSUIDs[][2],
    size_t numSuids,
    Bool siteDom,
    SW_OUT_DOM *OutDom,
    Bool succFlags[],
    size_t starts[][2],
    size_t counts[][2],
    SW_WALLTIME *SW_WallTime,
    LOG_INFO *LogInfo
) {
    size_t numWrites = 0;
    size_t write;
    size_t numSites;
    size_t mark;
    size_t maxNumWrites = 0;
    signed char succMark[N_SUID_ASSIGN] = {PRGRSS_READY};
    Bool considerSuccFlags = swTRUE;
    Bool useTempOut;
    WallTimeSpec tsr;
    Bool ok_tsr = swFALSE;

    if (numSuids > 0) {
        get_contiguous_counts(
            distSUIDs,
            numSuids,
            siteDom,
            numSuids,
            considerSuccFlags,
            succFlags,
            &numWrites,
            starts,
            counts
        );
    }
    SW_MPI_Allreduce(
        SW_MPI_SIZE_T, &numWrites, &maxNumWrites, 1, MPI_MAX, MPI_COMM_WORLD
    );

    useTempOut = (Bool) (N_SUID_ASSIGN > 1 && numSuids > 1);

    for (write = numWrites; write < maxNumWrites; write++) {
        starts[write][0] = starts[write][1] = 0;

        // NOLINTNEXTLINE(clang-analyzer-core.NullDereference)
        counts[write][0] = counts[write][1] = 0;
    }

    if (useTempOut) {
        reorder_output(
            OutDom,
            SW_PathOutputs->outTimeSizes,
            SW_PathOutputs->numOutFiles,
            numSuids,
            main_p_OUT,
            temp_p_OUT
        );
    }

    // Output accumulated output
    set_walltime(&tsr, &ok_tsr);
    SW_NCOUT_write_output(
        OutDom,
        useTempOut ? temp_p_OUT : main_p_OUT,
        SW_PathOutputs->numOutFiles,
        NULL,
        NULL,
        maxNumWrites,
        numWrites,
        starts,
        counts,
        SW_PathOutputs->openOutFileIDs,
        SW_PathOutputs->ncOutVarIDs,
        siteDom,
        succFlags,
        SW_PathOutputs->outTimeSizes,
        LogInfo
    );
    SW_WT_TimeRun(tsr, ok_tsr, TIME_IO, SW_WallTime);
    if (SW_MPI_setup_fail(LogInfo->stopRun, MPI_COMM_WORLD)) {
        return;
    }

    // Update progress file statuses
    for (write = 0; write < maxNumWrites; write++) {
        // NOLINTNEXTLINE(clang-analyzer-core.NullDereference)
        numSites = (siteDom) ? counts[write][0] : counts[write][1];

        for (mark = 0; mark < numSites; mark++) {
            // NOLINTBEGIN(clang-analyzer-core.NullDereference)
            succMark[mark] = succFlags[mark] ? PRGRSS_DONE : PRGRSS_FAIL;
            // NOLINTEND(clang-analyzer-core.NullDereference)
        }

        SW_NCIN_set_progress(
            progFileID,
            progVarID,
            starts[write],
            counts[write],
            (write < numWrites) ? succMark : NULL,
            LogInfo
        );
        if (SW_MPI_setup_fail(LogInfo->stopRun, MPI_COMM_WORLD)) {
            return;
        }
    }
}

/**
@brief Setup the process directly before running simulations by
    - Determining how many input-simulation-output cycles there will be
    - Initializing instances of SW_RUN_INPUTS
    - Copying weather if necessary
    - Creating output arrays to store output values

@param[in,out] sw_template Template SW_RUN for the function to use as a
    reference for local versions of SW_RUN
@param[in,out] runInputs A list of SW_RUN_INPUTS structs that will
be filled with template data and possibly have weather copied into it
@param[in] OutDom Struct of type SW_OUT_DOM that holds output
    information that do not change throughout simulation runs
@param[in] numCyclesProc Number of input-sim-output cycles the current
process will take to complete
@param[in] copyWeather Flag specifying if template weather should be copied
into each instance of SW_RUN_INPUTS within "runInputs"
@param[in] n_years Number of years of weather to receive if we do
    get weather from the I/O process
@param[out] tempOut Struct of type SW_OUT_RUN that holds temporary output
information; return this with allocated p_OUT
@param[out] extraFailCheck Specifies if finished processes will have to
take part of another input-sim-output cycle
@param[out] LogInfo Holds information on warnings and errors
*/
void SW_MPI_setup_inputs(
    SW_RUN *sw_template,
    SW_RUN_INPUTS *runInputs,
    SW_OUT_DOM *OutDom,
    int numCyclesProc,
    Bool copyWeather,
    IntU n_years,
    SW_OUT_RUN *tempOut,
    Bool *extraFailCheck,
    LOG_INFO *LogInfo
) {
    SW_PATH_OUTPUTS dummyOuts;
    size_t suid;
    int maxNumCyclesWorld = numCyclesProc;

    SW_MPI_Allreduce(
        MPI_INT, &numCyclesProc, &maxNumCyclesWorld, 1, MPI_MAX, MPI_COMM_WORLD
    );
    *extraFailCheck = (Bool) (numCyclesProc < maxNumCyclesWorld);

    for (suid = 0; suid < N_SUID_ASSIGN; suid++) {
        memcpy(&runInputs[suid], &sw_template->RunIn, sizeof(SW_RUN_INPUTS));

        if (!copyWeather) {
            SW_WTH_allocateAllWeather(
                &runInputs[suid].weathRunAllHist, n_years, LogInfo
            );
            if (LogInfo->stopRun) {
                return;
            }
        } else {
            runInputs[suid].weathRunAllHist = NULL;
        }
    }

    SW_OUT_deconstruct_outarray(&sw_template->OutRun);

    SW_OUT_construct_outarray(
        N_SUID_ASSIGN, OutDom, &sw_template->OutRun, LogInfo
    );

    if (N_SUID_ASSIGN > 1) {
        SW_OUT_init_ptrs(tempOut, &dummyOuts);

        SW_OUT_construct_outarray(N_SUID_ASSIGN, OutDom, tempOut, LogInfo);
    }
}
