#ifndef SW_FLOW_H
#define SW_FLOW_H

#include "include/SW_VegProd.h" // externs key2veg
#include "include/SW_Weather.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =================================================== */
/*             Global Function Declarations            */
/* --------------------------------------------------- */
void SW_FLW_init_run(void);
void SW_Water_Flow(SW_VEGPROD* SW_VegProd, SW_WEATHER* SW_Weather);


#ifdef __cplusplus
}
#endif

#endif
