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

/** Specifies the number of input variables per key a user
can provide; This number includes a variable for an index
file after and including the input key `inSpatial`.
These numbers must match up with `possVarNames` */
static const int numVarsInKey[] = {
    2,  /* inDomain */
    3,  /* inSpatial */
    4,  /* inTopo */
    22, /* inSoil */
    2,  /* inSite */
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
#define INXDIM 8
#define INYAXIS 9
#define INYDIM 10
#define INZAXIS 11
#define INTAXIS 12
#define INSTPATRN 13
#define INVAXIS 14

/* =================================================== */
/*             Global Function Declarations            */
/* --------------------------------------------------- */

void SW_NCIN_soilProfile(
    SW_NETCDF_IN *SW_netCDFIn,
    Bool hasConsistentSoilLayerDepths,
    LyrIndex *nMaxSoilLayers,
    LyrIndex *nMaxEvapLayers,
    double depthsAllSoilLayers[],
    const size_t numSoilVarLyrs[],
    LyrIndex default_n_layers,
    const double default_depths[],
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
    SW_RUN *sw, SW_DOMAIN *SW_Domain, const size_t ncSUID[], LOG_INFO *LogInfo
);

void SW_NCIN_check_input_config(
    SW_NETCDF_IN *SW_netCDFIn,
    Bool hasConsistentSoilLayerDepths,
    Bool inputsProvideSWRCp,
    LOG_INFO *LogInfo
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

void SW_NCIN_precalc_lookups(
    SW_DOMAIN *SW_Domain, SW_WEATHER *SW_Weather, LOG_INFO *LogInfo
);

void SW_NCIN_create_indices(SW_DOMAIN *SW_Domain, LOG_INFO *LogInfo);

#ifdef __cplusplus
}
#endif

#endif // SWNETCDF_IN_H
