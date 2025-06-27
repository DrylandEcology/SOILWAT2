#ifndef SW_FLOW_H
#define SW_FLOW_H

#include "include/SW_datastructs.h" // for SW_RUN, SW_SOILWAT, LOG_INFO

#ifdef __cplusplus
extern "C" {
#endif

/* =================================================== */
/*             Global Function Declarations            */
/* --------------------------------------------------- */
void SW_FLW_init_run(SW_SOILWAT_SIM *SW_SoilWatSim);

void SW_Water_Flow(SW_RUN *sw, LOG_INFO *LogInfo);


#ifdef __cplusplus
}
#endif

#endif
