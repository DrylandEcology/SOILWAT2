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

/* =================================================== */
/*             Global Function Declarations            */
/* --------------------------------------------------- */

void SW_MPI_initialize(
    int *argc, char ***argv, int *rank, int *worldSize, char *procName
);

void SW_MPI_finalize();

#ifdef __cplusplus
}
#endif

#endif // SWMPI_H
