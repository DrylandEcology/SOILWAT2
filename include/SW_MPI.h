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

void SW_MPI_Barrier(MPI_Comm comm);

void SW_MPI_Bcast(
    MPI_Datatype datatype, void *buffer, int count, int srcRank, MPI_Comm comm
);

Bool SW_MPI_setup_fail(Bool stopRun, MPI_Comm comm);

void SW_MPI_get_end_info(
    int rank,
    int size,
    size_t nActiveSites,
    SW_WALLTIME *SW_WallTime,
    LOG_INFO *LogInfo
);

#ifdef __cplusplus
}
#endif

#endif // SWMPI_H
