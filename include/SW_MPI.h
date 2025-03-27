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

#define N_ITER_BEFORE_OUT 1

/* =================================================== */
/*             Global Function Declarations            */
/* --------------------------------------------------- */

void SW_MPI_initialize(
    int *argc, char ***argv, int *rank, int *worldSize, char *procName
);

void SW_MPI_finalize();

void SW_MPI_Send(
    MPI_Datatype datatype,
    void *buffer,
    int count,
    int destRank,
    Bool sync,
    int tag,
    MPI_Request *request
);

void SW_MPI_Recv(
    MPI_Datatype datatype,
    void *buffer,
    int count,
    int srcRank,
    Bool sync,
    int tag,
    MPI_Request *request
);

void SW_MPI_create_types(MPI_Datatype datatypes[], LOG_INFO *LogInfo);

void SW_MPI_setup(
    int rank,
    int worldSize,
    char *procName,
    SW_DOMAIN *SW_Domain,
    SW_RUN *sw_template,
    LOG_INFO *LogInfo
);

void SW_MPI_template_info(
    int rank,
    SW_MPI_DESIGNATE *des,
    SW_RUN *SW_Run,
    MPI_Datatype inRunType,
    MPI_Datatype spinupType,
    MPI_Datatype vegEstabType,
    LOG_INFO *LogInfo
);

void SW_MPI_domain_info(SW_DOMAIN *SW_Domain, int rank, LOG_INFO *LogInfo);

void SW_MPI_ncout_info(
    int rank, MPI_Comm comm, SW_OUT_DOM *OutDom, LOG_INFO *LogInfo
);

void SW_MPI_open_files(
    int rank,
    SW_MPI_DESIGNATE *desig,
    SW_PATH_INPUTS *pathInputs,
    SW_NETCDF_IN *netCDFIn,
    SW_PATH_OUTPUTS *pathOutputs,
    SW_OUT_DOM *OutDom,
    LOG_INFO *LogInfo
);

Bool SW_MPI_check_setup_status(Bool stopRun, MPI_Comm comm);

void SW_MPI_root_find_active_sites(
    SW_DOMAIN *SW_Domain,
    unsigned long ***activeSuids,
    int *numActiveSites,
    LOG_INFO *LogInfo
);

void SW_MPI_get_activated_tsuids(
    SW_DOMAIN *SW_Domain,
    unsigned long **activeSuids,
    unsigned long ****activeTSuids,
    unsigned long numActiveSites,
    LOG_INFO *LogInfo
);

void SW_MPI_process_types(
    SW_DOMAIN *SW_Domain,
    char *procName,
    int worldSize,
    int rank,
    LOG_INFO *LogInfo
);

void SW_MPI_handle_IO(
    int rank, SW_RUN *sw, SW_DOMAIN *SW_Domain, LOG_INFO *LogInfo
);

#ifdef __cplusplus
}
#endif

#endif // SWMPI_H
