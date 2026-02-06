#include "include/SW_MPI.h"
#include "include/filefuncs.h"          // for LogError, BaseName, DirName
#include "include/generic.h"            // for Bool, swTRUE, isnull, swFALSE
#include "include/myMemory.h"           // for Mem_Malloc, Mem_ReAlloc, Str...
#include "include/SW_Control.h"         // for runSims
#include "include/SW_Domain.h"          // for SW_DOM_calc_ncSuid
#include "include/SW_Files.h"           // for eLog
#include "include/SW_Main_lib.h"        // for sw_write_warnings, sw_init_logs
#include "include/SW_Markov.h"          // for SW_MKV_construct, allocateMKV
#include "include/SW_netCDF_General.h"  // for SW_NC_open_par, SW_...
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
@param[in] nActiveSites Number of active sites the process controls
@param[in,out] SW_WallTime Struct of type SW_WALLTIME that holds timing
    information for the program run; on the root process, return
    an updated version containing overall (possibly averaged) timing
    information
@param[in] LogInfo Holds information on warnings and errors
*/
void SW_MPI_get_end_info(
    int rank,
    int size,
    size_t nActiveSites,
    SW_WALLTIME *SW_WallTime,
    LOG_INFO *LogInfo
) {
    SW_WALLTIME overallTiming;
    const size_t numReduceVals = 8;
    const size_t maxTimeIndex = 2;
    const size_t numWarnErr = 2;
    const size_t maxDoubleIndex = 5;
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

    if (nActiveSites == 0) {
        // Do not include processes that have no site simulation
        // in averages
        SW_WallTime->timeMax = 0.;
        SW_WallTime->timeMin = 0.;
        SW_WallTime->timeMean = 0.;
        SW_WallTime->totCompTime = 0.;
        SW_WallTime->totInputTime = 0.;
        SW_WallTime->totOutputTime = 0.;
        SW_WallTime->nTimedRuns = 0;
        SW_WallTime->nUntimedRuns = 0;
    }

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
