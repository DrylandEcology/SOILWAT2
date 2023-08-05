/**
 * @file   SW_Carbon.h
 * @author Zachary Kramer, Charles Duso
 * @brief  Defines functions, constants, and variables that deal with the effects of CO2 on transpiration and biomass.
 * @date   23 January 2017
 */
#ifndef CARBON
#define CARBON

#include "include/SW_datastructs.h"


#ifdef __cplusplus
extern "C" {
#endif


/* =================================================== */
/*             Global Function Declarations            */
/* --------------------------------------------------- */
void SW_CBN_construct(SW_CARBON* SW_Carbon);
void SW_CBN_deconstruct(void);
void SW_CBN_read(SW_CARBON* SW_Carbon, SW_MODEL* SW_Model, char *InFiles_csv[],
                 LOG_INFO* LogInfo);
void SW_CBN_init_run(VegType VegProd_veg[], SW_MODEL* SW_Model,
    SW_CARBON* SW_Carbon, LOG_INFO* LogInfo);


#ifdef __cplusplus
}
#endif

#endif
