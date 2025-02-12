/********************************************************/
/********************************************************/
/*  Source file: SW_Model.h
 *  Type: header
 *  Application: SOILWAT - soilwater dynamics simulator
 *  Purpose: Support for the Model.c routines and any others
 *           that need to reference that module.
 *
 *  History:
 *     (8/28/01) -- INITIAL CODING - cwb
 *    12/02 - IMPORTANT CHANGE - cwb
 *          refer to comments in Times.h regarding base0
 *
 *  2/14/03 - cwb - removed the days_in_month and
 *          cum_month_days arrays to common/Times.[ch]
 */
/********************************************************/
/********************************************************/

#ifndef SW_MODEL_H
#define SW_MODEL_H

#include "include/SW_datastructs.h" // for SW_MODEL, SW_DOMAIN, LOG_INFO

#ifdef __cplusplus
extern "C" {
#endif

/* =================================================== */
/*             Global Function Declarations            */
/* --------------------------------------------------- */
void SW_MDL_read(
    SW_MODEL_INPUTS *SW_ModelIn, char *txtInFiles[], LOG_INFO *LogInfo
);

void SW_MDL_construct(SW_MODEL_SIM *SW_ModelSim);

void SW_MDL_deconstruct(void);

void SW_MDL_new_year(SW_MODEL_INPUTS *SW_ModelIn, SW_MODEL_SIM *SW_ModelSim);

void SW_MDL_new_day(SW_MODEL_SIM *SW_ModelSim);

void SW_MDL_get_ModelRun(
    SW_MODEL_INPUTS *SW_ModelIn,
    SW_DOMAIN *SW_Domain,
    char *fileNames[],
    LOG_INFO *LogInfo
);


#ifdef __cplusplus
}
#endif

#endif
