#ifndef SWNETCDF_GEN_H
#define SWNETCDF_GEN_H

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

/** Domain netCDF index within `InFilesNC` and `varNC` (SW_NETCDF_OUT) */
#define vNCdom 0

/** Progress netCDF index within `InFilesNC` and `varNC` (SW_NETCDF_OUT) */
#define vNCprog 1

#define MAX_NUM_DIMS 5

/** Number of possible keys within `attributes_nc.in` */
#define NUM_ATT_IN_KEYS 33

#define MAX_ATTVAL_SIZE 256

/* =================================================== */
/*             Global Function Declarations            */
/* --------------------------------------------------- */

void SW_NC_get_dimlen_from_dimid(
    int ncFileID, int dimID, size_t *dimVal, LOG_INFO *LogInfo
);

void SW_NC_get_dim_identifier(
    int ncFileID, const char *dimName, int *dimID, LOG_INFO *LogInfo
);

void SW_NC_check(
    SW_DOMAIN *SW_Domain, int ncFileID, const char *fileName, LOG_INFO *LogInfo
);

void SW_NC_get_single_val(
    int ncFileID,
    int *varID,
    const char *varName,
    size_t index[],
    void *value,
    const char *type,
    LOG_INFO *LogInfo
);

void SW_NC_write_att(
    const char *attName,
    void *attVal,
    int varID,
    int ncFileID,
    size_t numVals,
    int ncType,
    LOG_INFO *LogInfo
);

void SW_NC_write_string_att(
    const char *attName,
    const char *attStr,
    int varID,
    int ncFileID,
    LOG_INFO *LogInfo
);

void SW_NC_write_string_vals(
    int ncFileID, int varID, const char *const varVals[], LOG_INFO *LogInfo
);

Bool SW_NC_dimExists(const char *targetDim, int ncFileID);

Bool SW_NC_varExists(int ncFileID, const char *varName);

void SW_NC_write_vals(
    int *varID,
    int ncFileID,
    const char *varName,
    void *values,
    size_t start[],
    size_t count[],
    const char *type,
    LOG_INFO *LogInfo
);

void SW_NC_get_str_att_val(
    int ncFileID,
    const char *varName,
    const char *attName,
    char *strVal,
    LOG_INFO *LogInfo
);

void SW_NC_create_netCDF_dim(
    const char *dimName,
    unsigned long size,
    const int *ncFileID,
    int *dimID,
    LOG_INFO *LogInfo
);

void SW_NC_get_var_identifier(
    int ncFileID, const char *varName, int *varID, LOG_INFO *LogInfo
);

void SW_NC_get_dimlen_from_dimname(
    int ncFileID, const char *dimName, size_t *dimVal, LOG_INFO *LogInfo
);

void SW_NC_create_full_var(
    int *ncFileID,
    const char *domType,
    int newVarType,
    size_t timeSize,
    size_t vertSize,
    size_t pftSize,
    const char *varName,
    const char *attNames[],
    const char *attVals[],
    unsigned int numAtts,
    Bool hasConsistentSoilLayerDepths,
    double lyrDepths[],
    double *startTime,
    unsigned int baseCalendarYear,
    unsigned int startYr,
    OutPeriod pd,
    int deflateLevel,
    const char *latName,
    const char *lonName,
    LOG_INFO *LogInfo
);

void SW_NC_dealloc_outputkey_var_info(SW_OUT_DOM *OutDom, IntUS k);

void SW_NC_create_netCDF_var(
    int *varID,
    const char *varName,
    int *dimIDs,
    const int *ncFileID,
    int varType,
    int numDims,
    size_t chunkSizes[],
    int deflateLevel,
    LOG_INFO *LogInfo
);

void SW_NC_create_template(
    const char *domType,
    const char *domFile,
    const char *fileName,
    int *newFileID,
    Bool isInput,
    const char *freq,
    LOG_INFO *LogInfo
);

void SW_NC_deconstruct(SW_NETCDF_OUT *SW_netCDFOut);

void SW_NC_deepCopy(
    SW_NETCDF_OUT *source_output,
    SW_NETCDF_IN *source_input,
    SW_NETCDF_OUT *dest_output,
    SW_NETCDF_IN *dest_input,
    LOG_INFO *LogInfo
);

void SW_NC_read(
    SW_NETCDF_IN *SW_netCDFIn,
    SW_NETCDF_OUT *SW_netCDFOut,
    SW_PATH_INPUTS *SW_PathInputs,
    TimeInt startYr,
    TimeInt endYr,
    LOG_INFO *LogInfo
);

void SW_NC_alloc_unitssw(char ***units_sw, int nVar, LOG_INFO *LogInfo);

void SW_NC_alloc_uconv(sw_converter_t ***uconv, int nVar, LOG_INFO *LogInfo);

void SW_NC_alloc_req(Bool **reqOutVar, int nVar, LOG_INFO *LogInfo);

void SW_NC_alloc_vars(
    char ****keyVars, int nVar, int numAtts, LOG_INFO *LogInfo
);

void SW_NC_create_units_converters(
    sw_converter_t **uconv[],
    char **units_sw[],
    Bool *reqVars[],
    char ***varInfo[],
    Bool out_use[],
    int numVars[],
    int varNameInd,
    int varUnitsInd,
    LOG_INFO *LogInfo
);

#ifdef __cplusplus
}
#endif

#endif // SWNETCDF_GEN_H
