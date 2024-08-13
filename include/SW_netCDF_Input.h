#ifndef SWNETCDF_IN_H
#define SWNETCDF_IN_H

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

#define DOMAIN_TEMP "Input_nc/domain_template.nc"

/* =================================================== */
/*             Global Function Declarations            */
/* --------------------------------------------------- */

void SW_NCIN_soilProfile(
    Bool *hasConsistentSoilLayerDepths,
    LyrIndex *nMaxSoilLayers,
    LyrIndex *nMaxEvapLayers,
    double depthsAllSoilLayers[],
    LyrIndex default_n_layers,
    LyrIndex default_n_evap_lyrs,
    double default_depths[],
    LOG_INFO *LogInfo
);

void SW_NCIN_create_domain_template(
    SW_DOMAIN *SW_Domain, char *fileName, LOG_INFO *LogInfo
);

void SW_NCIN_create_progress(SW_DOMAIN *SW_Domain, LOG_INFO *LogInfo);

void SW_NCIN_set_progress(
    Bool isFailure,
    const char *domType,
    int progFileID,
    int progVarID,
    size_t ncSUID[],
    LOG_INFO *LogInfo
);

Bool SW_NCIN_check_progress(
    int progFileID, int progVarID, size_t ncSUID[], LOG_INFO *LogInfo
);

void SW_NCIN_read_inputs(
    SW_RUN *sw, SW_DOMAIN *SW_Domain, size_t ncSUID[], LOG_INFO *LogInfo
);

void SW_NCIN_check_input_files(SW_DOMAIN *SW_Domain, LOG_INFO *LogInfo);

void SW_NCIN_open_dom_prog_files(SW_NETCDF_IN *SW_netCDFIn, LOG_INFO *LogInfo);

void SW_NCIN_close_files(SW_NETCDF_IN *SW_netCDFIn);

void SW_NCIN_init_ptrs(SW_NETCDF_IN *SW_netCDFIn);

void SW_NCIN_deconstruct(SW_NETCDF_IN *SW_netCDFIn);

void SW_NCIN_deepCopy(
    SW_NETCDF_IN *source_input, SW_NETCDF_IN *dest_input, LOG_INFO *LogInfo
);

#ifdef __cplusplus
}
#endif

#endif // SWNETCDF_IN_H
