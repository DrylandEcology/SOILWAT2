/********************************************************/
/********************************************************/
/*	Source file: Sky.c
Type: header - used by Weather.c
Application: SOILWAT - soilwater dynamics simulator
Purpose: Support definitions for Sky.c, Weather.c, etc.

History:

(8/28/01) -- INITIAL CODING - cwb

(10/12/2009) - (drs) added pressure

01/12/2010 (drs) removed pressure (used for snow sublimation)

08/22/2011 (drs) added monthly parameter 'snow_density' to struct SW_SKY
to estimate snow depth

09/26/2011 (drs) added a daily variable for each monthly input in struct
SW_SKY: double cloudcov_daily, windspeed_daily, r_humidity_daily,
transmission_daily, snow_density_daily each of [MAX_DAYS]
*/
/********************************************************/
/********************************************************/

#ifndef SW_SKY_H
#define SW_SKY_H

#include "include/SW_datastructs.h" // for LOG_INFO, SW_MODEL, SW_SKY_INPUTS
#include "include/SW_Defines.h"     // for MAX_MONTHS

#ifdef __cplusplus
extern "C" {
#endif

/* =================================================== */
/*             Global Function Declarations            */
/* --------------------------------------------------- */
void SW_SKY_read(
    char *txtInFiles[], SW_SKY_INPUTS *SW_SkyIn, LOG_INFO *LogInfo
);
void SW_SKY_new_year(
    SW_MODEL *SW_Model,
    double snow_density[MAX_MONTHS],
    double snow_density_daily[MAX_MONTHS]
);
void SW_SKY_init_run(SW_SKY_INPUTS *SW_SkyIn, LOG_INFO *LogInfo);
void checkSky(SW_SKY_INPUTS *SW_SkyIn, LOG_INFO *LogInfo);

#ifdef __cplusplus
}
#endif

#endif
