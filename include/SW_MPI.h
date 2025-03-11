#ifndef SWMPI_H
#define SWMPI_H

#include "include/SW_datastructs.h"

#include <mpi.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =================================================== */
/*                  Local Definitions                  */
/* --------------------------------------------------- */

typedef enum {
    eSW_MPI_Domain,
    eSW_MPI_Spinup,
    eSW_MPI_Inputs,
    eSW_MPI_WallTime,
    eSW_MPI_OutDomIO
} MPIType;
/* =================================================== */
/*             Global Function Declarations            */
/* --------------------------------------------------- */

void SW_MPI_initialize(
    int *argc, char ***argv, int *rank, int *worldSize, char *procName
);

void SW_MPI_finalize();

void SW_MPI_create_types(MPI_Datatype datatypes[], LOG_INFO *LogInfo);

#ifdef __cplusplus
}
#endif

#endif // SWMPI_H
