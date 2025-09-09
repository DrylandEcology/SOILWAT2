#ifndef SWMPI_H
#define SWMPI_H

#include "include/generic.h"        // for Bool, IntU
#include "include/SW_datastructs.h" // for LOG_INFO, SW_DOMAIN, SW_MPI_DESI...
#include "include/SW_Defines.h"     // for SW_OUTNPERIODS
#include <limits.h>                 // for UCHAR_MAX, UINT_MAX, ULONG_MAX
#include <mpi.h>                    // for MPI_UNSIGNED_LONG, MPI_Datatype
#include <stddef.h>                 // for size_t
#include <stdint.h>                 // for SIZE_MAX

#ifdef __cplusplus
extern "C" {
#endif

/* =================================================== */
/*                  Local Definitions                  */
/* --------------------------------------------------- */

#if SIZE_MAX == UCHAR_MAX
#define SW_MPI_SIZE_T MPI_UNSIGNED_CHAR
#elif SIZE_MAX == USHRT_MAX
#define SW_MPI_SIZE_T MPI_UNSIGNED_SHORT
#elif SIZE_MAX == UINT_MAX
#define SW_MPI_SIZE_T MPI_UNSIGNED
#elif SIZE_MAX == ULONG_MAX
#define SW_MPI_SIZE_T MPI_UNSIGNED_LONG
#else // SIZE_MAX == ULLONG_MAX
#define SW_MPI_SIZE_T MPI_UNSIGNED_LONG_LONG
#endif

typedef enum {
    eSW_MPI_Domain,
    eSW_MPI_Spinup,
    eSW_MPI_Inputs,
    eSW_MPI_Designate,
    eSW_MPI_WallTime,
    eSW_MPI_OutDomIO,
    eSW_MPI_VegEstabIn,
    eSW_MPI_Req,
    eSW_MPI_Log,
    eSW_MPI_WeathHist
} MPIType;

#define SW_MPI_PROC_COMP 0
#define SW_MPI_PROC_IO 1

/**
 * @brief Number of iterations of output gathered by an I/O process before
 *        outputing all values
 * @note An iteration is defined as the product of number of compute processes
 *       and #N_SUID_ASSIGN number of outputs gathered<br>
 * E.g., #N_ITER_BEFORE_OUT = 3, #N_SUID_ASSIGN = 4, n comp procs = 2
 *  - Iter 1: SUIDs 0-7
 *  - Iter 2: SUIDs 8-15
 *  - Iter 3: SUIDs 16-23 \n
 * Write output values gathered in iter 1-3 (SUIDs 0-23)
 *
 * @note This constant defaults to 1 but can be overwritten by the user
 *       when compiling the program, i.e., ... -DN_ITER_BEFORE_OUT=[n
 *       iterations] ...
 */
#ifndef N_ITER_BEFORE_OUT
#define N_ITER_BEFORE_OUT 1
#endif

// Reasons to fail the MPI program
#define SW_MPI_FAIL_NETCDF 1
#define SW_MPI_FAIL_COMP_ERR 2
#define SW_MPI_FAIL_MPI 3

/* =================================================== */
/*             Global Function Declarations            */
/* --------------------------------------------------- */

void SW_MPI_initialize(int *argc, char ***argv, int *rank, int *worldSize);

void SW_MPI_finalize();

void SW_MPI_Fail(int rank, int failType, char *mpiErrStr);

void SW_MPI_deconstruct(SW_DOMAIN *SW_Domain);

void SW_MPI_Reduce(
    void *src,
    void *dest,
    int count,
    MPI_Datatype datatype,
    MPI_Op op,
    int root,
    MPI_Comm comm
);

void SW_MPI_Allreduce(
    void *src,
    void *dest,
    int count,
    MPI_Datatype datatype,
    MPI_Op op,
    MPI_Comm comm
);

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
);

void SW_MPI_Barrier(MPI_Comm comm);

void SW_MPI_Bcast(
    MPI_Datatype datatype, void *buffer, int count, int srcRank, MPI_Comm comm
);

void SW_MPI_domain_info(SW_DOMAIN *SW_Domain, int rank, LOG_INFO *LogInfo);

Bool SW_MPI_setup_fail(Bool stopRun, MPI_Comm comm);

void SW_MPI_get_end_info(
    int rank, int size, SW_WALLTIME *SW_WallTime, LOG_INFO *LogInfo
);

void SW_MPI_proc_workload(
    int rank, int worldSize, SW_DOMAIN *SW_Domain, LOG_INFO *LogInfo
);

void SW_MPI_store_outputs(
    size_t runNum,
    SW_OUT_DOM *OutDom,
    double *src_p_OUT[][SW_OUTNPERIODS],
    double *dest_p_OUT[][SW_OUTNPERIODS]
);

void SW_MPI_get_sim_suids(
    Bool *readInVars[],
    size_t *domSuids[],
    const Bool useIndexFile[],
    size_t *readIndex,
    size_t *nSuidsLeft,
    size_t simSuids[SW_NINKEYSNC][N_SUID_ASSIGN][2],
    size_t *nSuids
);

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
    LOG_INFO *siteLogs,
    LOG_INFO *LogInfo
);

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
);

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
);

void SW_MPI_handle_IO(
    int rank,
    SW_RUN *sw,
    SW_DOMAIN *SW_Domain,
    Bool *setupFail,
    SW_WALLTIME *SW_WallTime,
    LOG_INFO *LogInfo
);

#ifdef __cplusplus
}
#endif

#endif // SWMPI_H
