#ifndef SW_FLOW_H
#define SW_FLOW_H

#include "include/SW_datastructs.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =================================================== */
/*             Global Function Declarations            */
/* --------------------------------------------------- */
void SW_FLW_init_run(SW_SOILWAT *SW_SoilWat);

void SW_Water_Flow(SW_ALL *sw, LOG_INFO *LogInfo);


#ifdef __cplusplus
}
#endif

#endif
