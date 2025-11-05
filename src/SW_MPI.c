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
#include <stdio.h>      // for FILE, snprintf, fprintf, FIL...
#include <stdlib.h>     // for size_t, NULL, free
#include <string.h>     // for memcpy, memset, strcmp, strlen

/* =================================================== */
/*                  Local Definitions                  */
/* --------------------------------------------------- */

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
@brief When storing output as simulations occur, the resulting output
    is not in the order we need it to be to be output properly by the
    output function due to the possibility of writing more than one
    site/grid cell; reorder output so it can properly be output by
    the netCDF output function

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
@brief Helper to calculate the contiguous reading/writings available
    based on the domain SUIDs

This function must result in a number of writes between
[1, N_SUID_ASSIGN]

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
    counts[0][1] = (sDom) ? 0 : N_SUID_ASSIGN; // NOLINT(bugprone-branch-clone)

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
*/
void SW_MPI_initialize(int *argc, char ***argv, int *rank, int *worldSize) {
    MPI_Init(argc, argv);

    MPI_Comm_rank(MPI_COMM_WORLD, rank);
    MPI_Comm_size(MPI_COMM_WORLD, worldSize);

    MPI_Comm_set_errhandler(MPI_COMM_WORLD, MPI_ERRORS_RETURN);
}

/**
@brief Conclude the program run by finalizing/freeing anything that's
been initialized/created through MPI within the program run
*/
void SW_MPI_finalize(void) { MPI_Finalize(); }

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
@brief Wrapper function to `MPI_Reduce()`

@param[in] src Location of memory that will be read from to give
    `MPI_Allreduce()` this process' information
@param[out] dest Location of memory that will be written from to by
    `MPI_Allreduce()`
@param[in] count Number of items to recv from the buffer
@param[in] datatype Custom MPI datatype that will be sent
@param[in] op MPI operation to be performed
@param[in] root Rank that is going to receive the reduction
@param[in] comm MPI communicator to broadcast a message to
*/
void SW_MPI_Reduce(
    void *src,
    void *dest,
    int count,
    MPI_Datatype datatype,
    MPI_Op op,
    int root,
    MPI_Comm comm
) {
    int mpiRes = MPI_SUCCESS;

    mpiRes = MPI_Reduce(src, dest, count, datatype, op, root, comm);

    if (mpiRes != MPI_SUCCESS) {
        errorMPI(-1, mpiRes);
    }
}

/**
@brief Wrapper function to `MPI_Allreduce()`

@param[in] src Location of memory that will be read from to give
    `MPI_Allreduce()` this process' information
@param[out] dest Location of memory that will be written from to by
    `MPI_Allreduce()`
@param[in] count Number of items to recv from the buffer
@param[in] datatype Custom MPI datatype that will be sent
@param[in] op MPI operation to be performed
@param[in] comm MPI communicator to broadcast a message to
*/
void SW_MPI_Allreduce(
    void *src,
    void *dest,
    int count,
    MPI_Datatype datatype,
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
@param[in] sendCount The number of elements that will be scattered from
"buffer", can be an array of counts if the calling function specifies for
"vectorized"
@param[in] sendType MPI type of the values being sent
@param[in] recvCount The number of elements that each process will receive
@param[in] recvType MPI type of the values being received
@param[in] src Source rank that will do the scattering
@param[in] vectorized Specifies if the function should use the vectorized
version of `MPI_Scatter()` - `MPI_Scatterv()`
@param[in] displacements An array of size [number spawned ranks] specifying
where to start the sending of each assigned chunk of suids to a process
@param[out] dest Destination buffer that will be filled with received
information
*/
void SW_MPI_Scatter(
    MPI_Comm comm,
    void *buffer,
    int *sendCount,
    MPI_Datatype sendType,
    int recvCount,
    MPI_Datatype recvType,
    int src,
    Bool vectorized,
    int **displacements,
    void *dest
) {
    int mpiRes;

    if (vectorized) {
        mpiRes = MPI_Scatterv(
            buffer,
            sendCount,
            *displacements,
            sendType,
            dest,
            recvCount,
            recvType,
            src,
            comm
        );
    } else {
        mpiRes = MPI_Scatter(
            buffer, *sendCount, sendType, dest, recvCount, recvType, src, comm
        );
    }

    if (mpiRes != MPI_SUCCESS) {
        if (!isnull(*displacements)) {
            free((void *) *displacements);
        }

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
@brief Before we proceed to the next important section of the program,
we must do a check-in with all processes to make sure no errors occurred

@param[in] stopRun A flag specifying if an error occurred and stop the run
@param[in] comm MPI communicator to broadcast a message to
*/
Bool SW_MPI_setup_fail(Bool stopRun, MPI_Comm comm) {
    int fail = (stopRun) ? 1 : 0;
    int failProgram = 0;

    SW_MPI_Allreduce(&fail, &failProgram, 1, MPI_INT, MPI_SUM, comm);

    return (Bool) (failProgram != 0);
}

/**
@brief Gather timing and # warning/error information from all processes,
average what is necessary and wall time for use outside of the function

@param[in] rank Process number known to MPI for the current process (aka rank)
@param[in] size Number of processors (world size) within the
    communicator MPI_COMM_WORLD
@param[in,out] SW_WallTime Struct of type SW_WALLTIME that holds timing
    information for the program run; on the root process, return
    an updated version containing overall (possibly averaged) timing
    information
@param[in] LogInfo Holds information on warnings and errors
*/
void SW_MPI_get_end_info(
    int rank, int size, SW_WALLTIME *SW_WallTime, LOG_INFO *LogInfo
) {
    SW_WALLTIME overallTiming;
    const size_t numReduceVals = 8;
    const size_t maxTimeIndex = 2;
    const size_t numWarnErr = 2;
    const size_t maxDoubleIndex = 4;
    size_t redVal;
    size_t warnErr;

    size_t totWarnErr = 0;

    size_t *warnErrSrc[] = {
        &LogInfo->numDomainErrors, &LogInfo->numDomainWarnings
    };

    void *reduceVals[] = {
        (void *) &SW_WallTime->timeMax,
        (void *) &SW_WallTime->timeMin,
        (void *) &SW_WallTime->timeMean,
        (void *) &SW_WallTime->totCompTime,
        (void *) &SW_WallTime->totInputTime,
        (void *) &SW_WallTime->totOutputTime,
        (void *) &SW_WallTime->nTimedRuns,
        (void *) &SW_WallTime->nUntimedRuns
    };

    void *destVals[] = {
        (void *) &overallTiming.timeMax,
        (void *) &overallTiming.timeMin,
        (void *) &overallTiming.timeMean,
        (void *) &overallTiming.totCompTime,
        (void *) &overallTiming.totInputTime,
        (void *) &overallTiming.totOutputTime,
        (void *) &overallTiming.nTimedRuns,
        (void *) &overallTiming.nUntimedRuns
    };

    if (rank == ROOT_PROC) {
        Mem_Copy(&overallTiming, SW_WallTime, sizeof(SW_WALLTIME));
    }

    /* Gather wall time information */
    for (redVal = 0; redVal < numReduceVals; redVal++) {
        SW_MPI_Reduce(
            reduceVals[redVal],
            destVals[redVal],
            1,
            (redVal <= maxDoubleIndex) ? MPI_DOUBLE : SW_MPI_SIZE_T,
            MPI_SUM,
            ROOT_PROC,
            MPI_COMM_WORLD
        );

        if (redVal <= maxTimeIndex) {
            *((double *) destVals[redVal]) /= size;
        }
    }

    /* Gather all counts of warnings/errors */
    for (warnErr = 0; warnErr < numWarnErr; warnErr++) {
        SW_MPI_Reduce(
            warnErrSrc[warnErr],
            &totWarnErr,
            1,
            SW_MPI_SIZE_T,
            MPI_SUM,
            ROOT_PROC,
            MPI_COMM_WORLD
        );
        if (rank == ROOT_PROC) {
            *(warnErrSrc[warnErr]) = totWarnErr;
        }
    }

    if (rank == ROOT_PROC) {
        Mem_Copy(SW_WallTime, &overallTiming, sizeof(SW_WALLTIME));
    }
}

/**
@brief Organize output data when completing a simulation run into
    a bigger output buffer to later reorganize and output

@param[in] runNum Current run number the process is on,
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
@brief Transfer suids from the domain of a process to a smaller list
of suids that will be used for simulating sites

@param[in] readInVars An array of arrays specifying which variables should
be read in; only used here to specify if we read ANY variables in a key
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
    Bool *readInVars[],
    size_t *domSuids[],
    const Bool useIndexFile[],
    size_t *readIndex,
    unsigned int *nSuidsLeft,
    size_t simSuids[SW_NINKEYSNC][N_SUID_ASSIGN][2],
    unsigned int *nSuids
) {
    const int xsCoordIndex = 0;
    const int yCoordIndex = 1;
    const int useKeyIndex = 0;

    int inKey;
    size_t suid;
    size_t *destSuid;
    size_t readStart = *readIndex;
    size_t ysVal;
    size_t xVal;
    size_t *domKeySuids;
    size_t suidIndex;
    Bool useKey;

    *nSuids = (*nSuidsLeft < N_SUID_ASSIGN) ? *nSuidsLeft : N_SUID_ASSIGN;

    ForEachNCInKey(inKey) {
        useKey = readInVars[inKey][useKeyIndex];

        if (useKey) {
            for (suid = 0; suid < *nSuids; suid++) {
                destSuid = simSuids[inKey][suid];
                domKeySuids = (inKey > eSW_InDomain && useIndexFile[inKey]) ?
                                  domSuids[inKey] :
                                  domSuids[eSW_InDomain];

                suidIndex = (readStart + suid) * 2;

                ysVal = domKeySuids[suidIndex];
                xVal = domKeySuids[suidIndex + 1];

                destSuid[xsCoordIndex] = ysVal;
                destSuid[yCoordIndex] = xVal;
            }
        }
    }

    *readIndex += *nSuids;
    *nSuidsLeft -= *nSuids;
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
    maximum of N_SUID_ASSIGN
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
    I/O and compute times
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
        &numWrites, &maxNumWrites, 1, SW_MPI_SIZE_T, MPI_MAX, MPI_COMM_WORLD
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
    SW_WT_TimeRun(tsr, ok_tsr, TIME_IO_OUT, SW_WallTime);
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

        set_walltime(&tsr, &ok_tsr);
        SW_NCIN_set_progress(
            progFileID,
            progVarID,
            starts[write],
            counts[write],
            (write < numWrites) ? succMark : NULL,
            LogInfo
        );
        SW_WT_TimeRun(tsr, ok_tsr, TIME_IO_OUT, SW_WallTime);
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
@param[in] readWeather Flag specifying if template weather should be copied
into each instance of SW_RUN_INPUTS within "runInputs"
@param[in] n_years Number of years of weather to read in
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
    Bool readWeather,
    IntU n_years,
    SW_OUT_RUN *tempOut,
    Bool *extraFailCheck,
    LOG_INFO *LogInfo
) {
    SW_PATH_OUTPUTS dummyOuts;
    size_t suid;
    int maxNumCyclesWorld = numCyclesProc;

    SW_MPI_Allreduce(
        &numCyclesProc, &maxNumCyclesWorld, 1, MPI_INT, MPI_MAX, MPI_COMM_WORLD
    );
    *extraFailCheck = (Bool) (numCyclesProc < maxNumCyclesWorld);

    for (suid = 0; suid < N_SUID_ASSIGN; suid++) {
        memcpy(&runInputs[suid], &sw_template->RunIn, sizeof(SW_RUN_INPUTS));

        runInputs[suid].weathRunAllHist = NULL;

        SW_WTH_allocateAllWeather(
            &runInputs[suid].weathRunAllHist, n_years, LogInfo
        );
        if (LogInfo->stopRun) {
            return;
        }

        if (!readWeather) {
            memcpy(
                runInputs[suid].weathRunAllHist,
                sw_template->RunIn.weathRunAllHist,
                sizeof(SW_WEATHER_HIST) * n_years
            );
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

/**
@brief Counterpart to SW_MPI_setup_inputs to deallocate any memory

@param[in,out] runInputs A list of SW_RUN_INPUTS structs that will
be deallocated
@param[in,out] OutRun Struct of type SW_OUT_RUN that holds output
information that may change throughout simulation runs
@param[in,out] tempOut Struct of type SW_OUT_RUN that holds temporary output
information; return this with deallocated p_OUT
*/
void SW_MPI_dealloc_inputs(
    SW_RUN_INPUTS runInputs[], SW_OUT_RUN *OutRun, SW_OUT_RUN *tempOut
) {
    size_t suid;

    for (suid = 0; suid < N_SUID_ASSIGN; suid++) {
        SW_WTH_deconstruct(&runInputs[suid].weathRunAllHist);
    }

    SW_OUT_deconstruct_outarray(OutRun);

    if (N_SUID_ASSIGN > 1) {
        SW_OUT_deconstruct_outarray(tempOut);
    }
}
