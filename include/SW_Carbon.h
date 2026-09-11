/**
@file   SW_Carbon.h
@author Zachary Kramer, Charles Duso
@brief  Defines functions, constants, and variables that deal with the
effects of CO2 on transpiration and biomass.
@date   23 January 2017
*/
#ifndef CARBON
#define CARBON

#include "include/SW_datastructs.h" // for SW_CARBON_INPUTS, LOG_INFO, VegT...
#include "include/SW_Defines.h"     // for TimeInt


#ifdef __cplusplus
extern "C" {
#endif


/* =================================================== */
/*             Global Function Declarations            */
/* --------------------------------------------------- */

void SW_CBN_init_ptrs(SW_CARBON_INPUTS *SW_CarbonIn);

void SW_CBN_alloc_ppm(TimeInt n_years, double **ppm, LOG_INFO *LogInfo);

void SW_CBN_construct(SW_CARBON_INPUTS *SW_CarbonIn);

void SW_CBN_deconstruct(SW_CARBON_INPUTS *SW_CarbonIn);

void SW_CBN_setup(
    SW_CARBON_INPUTS *SW_CarbonIn,
    TimeInt startYr,
    TimeInt endYr,
    char *txtInFiles[],
    TimeInt vegYear,
    LOG_INFO *LogInfo
);

void SW_CBN_read(
    SW_CARBON_INPUTS *SW_CarbonIn,
    TimeInt startYr,
    TimeInt endYr,
    char *txtInFiles[],
    TimeInt vegYear,
    LOG_INFO *LogInfo
);

void SW_CBN_init_run(
    VegTypeIn *vegIn,
    VegTypeSim *vegSim,
    SW_CARBON_INPUTS *SW_CarbonIn,
    TimeInt startYr,
    TimeInt endYr,
    LOG_INFO *LogInfo
);


#ifdef __cplusplus
}
#endif

#endif
