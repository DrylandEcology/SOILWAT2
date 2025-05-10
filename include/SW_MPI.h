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

void SW_MPI_initialize(
    int *argc,
    char ***argv,
    int *rank,
    int *worldSize,
    char *procName,
    SW_MPI_DESIGNATE *desig,
    MPI_Datatype datatypes[]
);

void SW_MPI_finalize();

void SW_MPI_free_comms_types(
    int rank, SW_MPI_DESIGNATE *desig, MPI_Datatype types[], LOG_INFO *LogInfo
);

void SW_MPI_Fail(int rank, int failType, char *mpiErrStr);

void SW_MPI_deconstruct(SW_DOMAIN *SW_Domain);

void SW_MPI_Bcast(
    MPI_Datatype datatype, void *buffer, int count, int srcRank, MPI_Comm comm
);

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
    SW_MPI_DESIGNATE *desig,
    SW_RUN *SW_Run,
    MPI_Datatype inRunType,
    MPI_Datatype spinupType,
    MPI_Datatype vegEstabType,
    MPI_Datatype weathHistType,
    Bool getWeather,
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

void SW_MPI_close_in_files(
    int **openInFileIDs[],
    Bool **readInVars,
    Bool useIndexFile[],
    int numWeathFiles
);

void SW_MPI_close_out_files(
    int *openOutFileIDs[][SW_OUTNPERIODS], SW_OUT_DOM *OutDom, int numOutFiles
);

Bool SW_MPI_setup_fail(Bool stopRun, MPI_Comm comm);

void SW_MPI_report_log(
    int rank,
    int size,
    MPI_Datatype wtType,
    SW_WALLTIME *SW_WallTime,
    SW_DOMAIN *SW_Domain,
    Bool failedSetup,
    LOG_INFO *LogInfo
);

void SW_MPI_write_main_logs(
    SW_MPI_DESIGNATE *desig, MPI_Datatype logType, LOG_INFO *LogInfo
);

void SW_MPI_root_find_active_sites(
    SW_DOMAIN *SW_Domain,
    unsigned long ***activeSuids,
    size_t *numActiveSites,
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

void SW_MPI_store_outputs(
    int runNum,
    SW_OUT_DOM *OutDom,
    double *src_p_OUT[][SW_OUTNPERIODS],
    double *dest_p_OUT[][SW_OUTNPERIODS]
);

void SW_MPI_send_results(
    SW_OUT_DOM *OutDom,
    int rank,
    int numInputs,
    int ioRank,
    MPI_Datatype reqTypeMPI,
    MPI_Datatype logType,
    Bool runStatuses[],
    Bool reportLog,
    LOG_INFO logs[],
    double *p_OUT[][SW_OUTNPERIODS]
);

void SW_MPI_get_inputs(
    Bool getWeather,
    int n_years,
    SW_MPI_DESIGNATE *desig,
    MPI_Datatype inputType,
    MPI_Datatype weathHistType,
    SW_RUN_INPUTS inputs[],
    int *numInputs,
    Bool *estVeg,
    Bool *getEstVeg,
    Bool *extraFailCheck
);

void SW_MPI_handle_IO(
    int rank,
    SW_RUN *sw,
    SW_DOMAIN *SW_Domain,
    Bool *setupFail,
    LOG_INFO *LogInfo
);

#ifdef __cplusplus
}
#endif

#endif // SWMPI_H
