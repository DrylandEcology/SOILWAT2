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

/* Specifies the number of input variables per key a user
can provide; This number includes a variable for an index
file after and including the input key `inTopo` */
static const int numVarsInKey[] = {
    2,  /* inDomain */
    2,  /* inSpatial */
    4,  /* inTopo */
    22, /* inSoil */
    22, /* inVeg */
    15, /* inWeather */
    6   /* inClimate */
};

#define ForEachNCInKey(k) for ((k) = 0; (k) < eSW_LastInKey; (k)++)

/* Indices within `inVarInfo` for specific information of a variable */
#define INUNIT 0
#define INNCVARNAME 1
#define INVARUNITS 2
#define INDOMTYPE 3
#define INSITENAME 4
#define INCRSNAME 5
#define INGRIDMAPPING 6
#define INXAXIS 7
#define INYAXIS 8
#define INZAXIS 9
#define INTAXIS 10
#define INSTPATRN 11
#define INVAXIS 12

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

void SW_NCIN_open_dom_prog_files(
    SW_NETCDF_IN *SW_netCDFIn, SW_PATH_INPUTS *SW_PathInputs, LOG_INFO *LogInfo
);

void SW_NCIN_close_files(int ncDomFileIDs[]);

void SW_NCIN_init_ptrs(SW_NETCDF_IN *SW_netCDFIn);

void SW_NCIN_deconstruct(SW_NETCDF_IN *SW_netCDFIn);

void SW_NCIN_dealloc_inputkey_var_info(SW_NETCDF_IN *SW_netCDFIn, int key);

void SW_NCIN_deepCopy(
    SW_NETCDF_IN *source_input, SW_NETCDF_IN *dest_input, LOG_INFO *LogInfo
);

void SW_NCIN_alloc_input_var_info(SW_NETCDF_IN *SW_netCDFIn, LOG_INFO *LogInfo);

void SW_NCIN_alloc_inputkey_var_info(
    SW_NETCDF_IN *SW_netCDFIn, int key, LOG_INFO *LogInfo
);

void SW_NCIN_read_input_vars(
    SW_NETCDF_IN *SW_netCDFIn,
    SW_NETCDF_OUT *SW_netCDFOut,
    SW_PATH_INPUTS *SW_PathInputs,
    TimeInt startYr,
    TimeInt endYr,
    LOG_INFO *LogInfo
);

void SW_NCIN_alloc_file_information(
    int numInVars,
    int key,
    char ***inputFiles,
    char ****ncWeatherInFiles,
    LOG_INFO *LogInfo
);

void SW_NCIN_create_units_converters(
    SW_NETCDF_IN *SW_netCDFIn, LOG_INFO *LogInfo
);

void SW_NCIN_alloc_weath_input_info(
    char ****outWeathFileNames,
    unsigned int ***ncWeatherInStartEndYrs,
    unsigned int numWeathIn,
    int weathVar,
    LOG_INFO *LogInfo
);

void SW_NCIN_precalc_lookups(SW_DOMAIN *SW_Domain, LOG_INFO *LogInfo);

void SW_NCIN_create_indices(SW_DOMAIN *SW_Domain, LOG_INFO *LogInfo);

#ifdef __cplusplus
}
#endif

#endif // SWNETCDF_IN_H
