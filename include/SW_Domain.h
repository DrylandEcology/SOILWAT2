#ifndef SWDOMAIN_H
#define SWDOMAIN_H

#include "include/generic.h"        // for Bool
#include "include/SW_datastructs.h" // for SW_DOMAIN, SW_DOMAIN
#include "include/SW_Defines.h"     // for LyrIndex

#ifdef __cplusplus
extern "C" {
#endif

/* =================================================== */
/*                  Local Definitions                  */
/* --------------------------------------------------- */
void SW_DOM_calc_ncSuid(
    SW_DOMAIN *SW_Domain, unsigned long suid, unsigned long ncSuid[]
);

void SW_DOM_calc_nSUIDs(SW_DOMAIN *SW_Domain);

Bool SW_DOM_CheckProgress(
    int progFileID, int progVarID, unsigned long ncSuid[], LOG_INFO *LogInfo
);

void SW_DOM_CreateProgress(SW_DOMAIN *SW_Domain, LOG_INFO *LogInfo);

void SW_DOM_construct(unsigned long rng_seed, SW_DOMAIN *SW_Domain);

void SW_DOM_read(SW_DOMAIN *SW_Domain, LOG_INFO *LogInfo);

void SW_DOM_SetProgress(
    Bool isFailure,
    const char *domType,
    int progFileID,
    int progVarID,
    unsigned long ncSuid[],
    LOG_INFO *LogInfo
);

void SW_DOM_SimSet(
    SW_DOMAIN *SW_Domain, unsigned long userSUID, LOG_INFO *LogInfo
);

void SW_DOM_deepCopy(SW_DOMAIN *source, SW_DOMAIN *dest, LOG_INFO *LogInfo);

void SW_DOM_init_ptrs(SW_DOMAIN *SW_Domain);

void SW_DOM_deconstruct(SW_DOMAIN *SW_Domain);

void SW_DOM_soilProfile(
    SW_NETCDF_IN *SW_netCDFIn,
    SW_PATH_INPUTS *SW_PathInputs,
    Bool hasConsistentSoilLayerDepths,
    LyrIndex *nMaxSoilLayers,
    LyrIndex *nMaxEvapLayers,
    double depthsAllSoilLayers[],
    LyrIndex default_n_layers,
    LyrIndex default_n_evap_lyrs,
    const double default_depths[],
    LOG_INFO *LogInfo
);


#ifdef __cplusplus
}
#endif

#endif // SWDOMAIN_H
