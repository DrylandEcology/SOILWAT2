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

08/22/2011 (drs) added monthly parameter 'snow_density' to struct SW_SKY to
estimate snow depth

09/26/2011 (drs) added a daily variable for each monthly input in struct
SW_SKY: RealD cloudcov_daily, windspeed_daily, r_humidity_daily,
transmission_daily, snow_density_daily each of [MAX_DAYS]
*/
/********************************************************/
/********************************************************/

#ifndef SW_SKY_H
#define SW_SKY_H

#include "include/SW_datastructs.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =================================================== */
/*             Global Function Declarations            */
/* --------------------------------------------------- */
void SW_SKY_read(char *InFiles[], SW_SKY *SW_Sky, LOG_INFO *LogInfo);
void SW_SKY_new_year(
    SW_MODEL *SW_Model,
    RealD snow_density[MAX_MONTHS],
    RealD snow_density_daily[MAX_MONTHS]
);

#ifdef __cplusplus
}
#endif

#endif
