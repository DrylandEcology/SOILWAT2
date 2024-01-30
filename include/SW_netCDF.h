#ifndef SWNETCDF_H
#define SWNETCDF_H

#include "include/SW_datastructs.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =================================================== */
/*                  Local Definitions                  */
/* --------------------------------------------------- */

#define vNCdom 0 // Domain netCDF index within `InFilesNC` and `varNC` (SW_NETCDF)
#define vNCprog 1 // Progress netCDF index within `InFilesNC` and `varNC` (SW_NETCDF)

#define DOMAIN_TEMP "Input_nc/domain_template.nc"

#define NOUT_VAR_INPUTS 10
/* =================================================== */
/*             Global Function Declarations            */
/* --------------------------------------------------- */
void SW_NC_write_output(SW_OUTPUT* SW_Output, SW_GEN_OUT* SW_GenOut,
        int numFilesPerKey, char** ncOutFileNames[][SW_OUTNPERIODS],
        size_t ncSuid[], int strideOutYears, int startYr, int endYr,
        LOG_INFO* LogInfo);
void SW_NC_create_output_files(const char* domFile, int domFileID,
        SW_OUTPUT* SW_Output, int strideOutYears, int startYr, int endYr,
        LyrIndex n_layers, int n_evap_lyrs, int* numFilesPerKey,
        double lyrDepths[], int baseCalendarYear, Bool useOutPeriods[],
        char** ncOutFileNames[][SW_OUTNPERIODS], LOG_INFO* LogInfo);
int SW_NC_get_nMaxSoilLayers(int readInNumLayers);
void SW_NC_check(SW_DOMAIN* SW_Domain, int ncFileID, const char* fileName,
                 LOG_INFO* LogInfo);
void SW_NC_create_domain_template(SW_DOMAIN* SW_Domain, LOG_INFO* LogInfo);
void SW_NC_create_template(const char* domFile, int domFileID,
    const char* fileName, int* newFileID, int newVarType,
    size_t timeSize, size_t vertSize, int vegSize,
    const char* varName, const char* attNames[], const char* attVals[],
    int numAtts, Bool isInput, const char* freq, double* startTime,
    double lyrDepths[], int baseCalendarYear, int startYr, OutPeriod pd,
    LOG_INFO* LogInfo);
void SW_NC_create_progress(SW_DOMAIN* SW_Domain, LOG_INFO* LogInfo);
void SW_NC_set_progress(Bool isFailure, const char* domType, int progFileID,
                        int progVarID, size_t ncSUID[],
                        LOG_INFO* LogInfo);
Bool SW_NC_check_progress(int progFileID, int progVarID,
                          size_t ncSUID[], LOG_INFO* LogInfo);
void SW_NC_read_inputs(SW_ALL* sw, SW_DOMAIN* SW_Domain, size_t ncSUID[],
                       LOG_INFO* LogInfo);
void SW_NC_check_input_files(SW_DOMAIN* SW_Domain, LOG_INFO* LogInfo);
void SW_NC_read(SW_NETCDF* SW_netCDF, PATH_INFO* PathInfo, LOG_INFO* LogInfo);
void SW_NC_read_out_vars(SW_OUTPUT* SW_Output, char* InFiles[],
                    SW_VEGESTAB_INFO** parms, LOG_INFO* LogInfo);
void SW_NC_init_ptrs(SW_NETCDF* SW_netCDF);
void SW_NC_deconstruct(SW_NETCDF* SW_netCDF);
void SW_NC_open_dom_prog_files(SW_NETCDF* SW_netCDF, LOG_INFO* LogInfo);
void SW_NC_close_files(SW_NETCDF* SW_netCDF);
void SW_NC_deepCopy(SW_NETCDF* source, SW_NETCDF* dest, LOG_INFO* LogInfo);
void SW_NC_init_outvars(char**** outkeyVars, OutKey currOutKey,
                                 LOG_INFO* LogInfo);
void SW_NC_init_outReq(Bool** reqOutVar, OutKey currOutKey, LOG_INFO* LogInfo);
void SW_NC_alloc_files(char*** ncOutFiles, int numFiles, LOG_INFO* LogInfo);

#ifdef __cplusplus
}
#endif

#endif // SWNETCDF_H
