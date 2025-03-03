/**
@file   SW_Carbon.h
@author Zachary Kramer, Charles Duso
@brief  Defines functions, constants, and variables that deal with the
effects of CO2 on transpiration and biomass.
@date   23 January 2017
*/
#ifndef CARBON
#define CARBON

#include "include/SW_datastructs.h" // for SW_CarbonIn, SW_Model, LOG_INFO


#ifdef __cplusplus
extern "C" {
#endif


/* =================================================== */
/*             Global Function Declarations            */
/* --------------------------------------------------- */
void SW_CBN_construct(SW_CARBON_INPUTS *SW_CarbonIn);

void SW_CBN_deconstruct(void);

void SW_CBN_read(
    SW_CARBON_INPUTS *SW_CarbonIn,
    int addtl_yr,
    TimeInt startYr,
    TimeInt endYr,
    char *txtInFiles[],
    LOG_INFO *LogInfo
);

void SW_CBN_init_run(
    VegType VegProd_veg[],
    SW_CARBON_INPUTS *SW_CarbonIn,
    int addtl_yr,
    TimeInt startYr,
    TimeInt endYr,
    LOG_INFO *LogInfo
);


#ifdef __cplusplus
}
#endif

#endif
