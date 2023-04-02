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
/*            Externed Global Variables                */
/* --------------------------------------------------- */
extern SW_CARBON SW_Carbon;


/* =================================================== */
/*             Global Function Declarations            */
/* --------------------------------------------------- */
void SW_CBN_construct(void);
void SW_CBN_deconstruct(void);
void SW_CBN_read(void);
void SW_CBN_init_run(SW_VEGPROD* SW_VegProd);


#ifdef __cplusplus
}
#endif

#endif
