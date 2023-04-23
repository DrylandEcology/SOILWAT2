#ifndef SW_FLOW_H
#define SW_FLOW_H

#include "include/SW_datastructs.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =================================================== */
/*             Global Function Declarations            */
/* --------------------------------------------------- */
void SW_FLW_init_run(void);
void SW_Water_Flow(SW_VEGPROD* SW_VegProd, SW_WEATHER* SW_Weather,
				   SW_SOILWAT* SW_SoilWat, SW_MODEL* SW_Model,
				   SW_SITE* SW_Site);


#ifdef __cplusplus
}
#endif

#endif
