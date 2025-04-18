#ifndef SWNETCDF_OUT_H
#define SWNETCDF_OUT_H

#include "include/generic.h"        // for Bool, IntUS
#include "include/SW_datastructs.h" // for SW_DOMAIN, SW_NETCDF_OUT, S...
#include "include/SW_Defines.h"     // for OutPeriod, SW_OUTNPERIODS, SW_OUTN...
#include <stdio.h>                  // for size_t

#ifdef __cplusplus
extern "C" {
#endif

/* =================================================== */
/*                  Local Definitions                  */
/* --------------------------------------------------- */

/** Maximum number of attributes an output variable may have */
#define MAX_NATTS 7

/* =================================================== */
/*             Global Function Declarations            */
/* --------------------------------------------------- */

void SW_NCOUT_create_output_dimVar(
    char *name,
    unsigned int size,
    int ncFileID,
    int *dimID,
    Bool hasConsistentSoilLayerDepths,
    double lyrDepths[],
    double *startTime,
    unsigned int baseCalendarYear,
    unsigned int startYr,
    OutPeriod pd,
    int deflateLevel,
    LOG_INFO *LogInfo
);

void SW_NCOUT_read_out_vars(
    SW_OUT_DOM *OutDom,
    char *txtInFiles[],
    SW_VEGESTAB_INFO_INPUTS *parmsIn,
    LOG_INFO *LogInfo
);

void SW_NCOUT_alloc_files(
    char ***ncOutFiles, unsigned int numFiles, LOG_INFO *LogInfo
);

void SW_NCOUT_alloc_varids(int **ncVarIDs, IntUS numVars, LOG_INFO *LogInfo);

void SW_NCOUT_init_ptrs(SW_NETCDF_OUT *SW_netCDFOut);

void SW_NCOUT_alloc_output_var_info(SW_OUT_DOM *OutDom, LOG_INFO *LogInfo);

void SW_NCOUT_alloc_outputkey_var_info(
    SW_OUT_DOM *OutDom, int key, LOG_INFO *LogInfo
);

void SW_NCOUT_dealloc_outputkey_var_info(SW_OUT_DOM *OutDom, IntUS k);

void SW_NCOUT_create_output_files(
    const char *domFile,
    const char *domType,
    const char *outputPrefix,
    SW_DOMAIN *SW_Domain,
    OutPeriod timeSteps[][SW_OUTNPERIODS],
    IntUS used_OUTNPERIODS,
    IntUS nvar_OUT[],
    IntUS nsl_OUT[][SW_OUTNMAXVARS],
    IntUS npft_OUT[][SW_OUTNMAXVARS],
    Bool hasConsistentSoilLayerDepths,
    double lyrDepths[],
    int strideOutYears,
    unsigned int startYr,
    unsigned int endYr,
    int baseCalendarYear,
    size_t outFileTimeSizes[][2],
    unsigned int *numFilesPerKey,
    char **ncOutFileNames[][SW_OUTNPERIODS],
    int *ncOutVarIDs[],
    LOG_INFO *LogInfo
);

void SW_NCOUT_create_units_converters(SW_OUT_DOM *OutDom, LOG_INFO *LogInfo);

void SW_NCOUT_write_output(
    SW_OUT_DOM *OutDom,
    double *p_OUT[][SW_OUTNPERIODS],
    unsigned int numFilesPerKey,
    char **ncOutFileNames[][SW_OUTNPERIODS],
    const size_t ncSuid[],
    int numWrites,
    size_t **starts,
    size_t **counts,
    int *openOutFileIDs[][SW_OUTNPERIODS],
    int *outVarIDs[],
    Bool siteDom,
    size_t timeSizes[][2],
    LOG_INFO *LogInfo
);

void SW_NCOUT_deconstruct(SW_NETCDF_OUT *SW_netCDFOut);

void SW_NCOUT_deepCopy(
    SW_NETCDF_OUT *source_output, SW_NETCDF_OUT *dest_output, LOG_INFO *LogInfo
);

void SW_NCOUT_read_atts(
    SW_NETCDF_OUT *SW_netCDFOut,
    SW_PATH_INPUTS *SW_PathInputs,
    LOG_INFO *LogInfo
);

#ifdef __cplusplus
}
#endif

#endif // SWNETCDF_OUT_H
