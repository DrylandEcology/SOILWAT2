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

// Reasons to fail the MPI program
#define SW_MPI_FAIL_NETCDF 1
#define SW_MPI_FAIL_COMP_ERR 2
#define SW_MPI_FAIL_MPI 3

/* =================================================== */
/*             Global Function Declarations            */
/* --------------------------------------------------- */

void SW_MPI_initialize(int *argc, char ***argv, int *rank, int *worldSize);

void SW_MPI_finalize(void);

void SW_MPI_Fail(int rank, int failType, char *mpiErrStr);

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

Bool SW_MPI_setup_fail(Bool stopRun, MPI_Comm comm);

void SW_MPI_get_end_info(
    int rank, int size, SW_WALLTIME *SW_WallTime, LOG_INFO *LogInfo
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
    unsigned int *nSuidsLeft,
    size_t simSuids[SW_NINKEYSNC][N_SUID_ASSIGN][2],
    unsigned int *nSuids
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
    Bool readWeather,
    IntU n_years,
    SW_OUT_RUN *tempOut,
    Bool *extraFailCheck,
    LOG_INFO *LogInfo
);

void SW_MPI_dealloc_inputs(
    SW_RUN_INPUTS runInputs[], SW_OUT_RUN *OutRun, SW_OUT_RUN *tempOut
);

#ifdef __cplusplus
}
#endif

#endif // SWMPI_H
