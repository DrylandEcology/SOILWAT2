#ifndef SWDOMAIN_H
#define SWDOMAIN_H

#include "include/generic.h"        // for Bool
#include "include/SW_datastructs.h" // for SW_DOMAIN, LOG_INFO, SW_NETCDF_IN
#include "include/SW_Defines.h"     // for LyrIndex
#include <stddef.h>                 // for size_t

#ifdef __cplusplus
extern "C" {
#endif

/* =================================================== */
/*                  Local Definitions                  */
/* --------------------------------------------------- */
void SW_DOM_calc_ncSuid(SW_DOMAIN *SW_Domain, size_t suid, size_t ncSuid[]);

void SW_DOM_calc_suid_from_subdom(
    Bool sDom,
    size_t startYS,
    size_t startX,
    size_t actSiteIdx,
    size_t nCols,
    size_t ncSuid[]
);

void SW_DOM_calc_nSUIDs(SW_DOMAIN *SW_Domain);

Bool SW_DOM_CheckProgress(
    int progFileID, int progVarID, size_t ncSuid[], LOG_INFO *LogInfo
);

void SW_DOM_CreateProgress(SW_DOMAIN *SW_Domain, LOG_INFO *LogInfo);

void SW_DOM_construct(size_t rng_seed, SW_DOMAIN *SW_Domain);

void SW_DOM_read(SW_DOMAIN *SW_Domain, LOG_INFO *LogInfo);

void SW_DOM_SetProgress(
    Bool isFailure,
    int progFileID,
    int progVarID,
    size_t start[],
    size_t count[],
    LOG_INFO *LogInfo
);

void SW_DOM_SimSet(
    int rank,
    int worldSize,
    TimeInt runSimDayLen,
    SW_DOMAIN *SW_Domain,
    LOG_INFO *LogInfo
);

void SW_DOM_deepCopy(SW_DOMAIN *source, SW_DOMAIN *dest, LOG_INFO *LogInfo);

void SW_DOM_init_ptrs(SW_DOMAIN *SW_Domain);

void SW_DOM_deconstruct(SW_DOMAIN *SW_Domain);

void SW_DOM_soilProfile(
    SW_NETCDF_IN *SW_netCDFIn,
    SW_PATH_INPUTS *SW_PathInputs,
    Bool hasConsistentSoilLayerDepths,
    LyrIndex *nMaxSoilLayers,
    double depthsAllSoilLayers[],
    LyrIndex default_n_layers,
    const double default_depths[],
    LOG_INFO *LogInfo
);


#ifdef __cplusplus
}
#endif

#endif // SWDOMAIN_H
