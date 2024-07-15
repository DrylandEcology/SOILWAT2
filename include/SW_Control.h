/********************************************************/
/********************************************************/
/*  Source file: SW_Control.h
 *  Type: header
 *  Application: SOILWAT - soilwater dynamics simulator
 *  Purpose: This module controls the flow of the model.
 *           Previously this was done in main() but to
 *           combine the model with other code (eg STEPPE)
 *           there needs to be separate callable routines
 *           for initializing, model flow, and output.
 *
 *  History:
 *     (10-May-02) -- INITIAL CODING - cwb
 */
/********************************************************/
/********************************************************/

#ifndef SW_CONTROL_H
#define SW_CONTROL_H

#include "include/generic.h"        // for Bool
#include "include/SW_datastructs.h" // for SW_RUN, LOG_INFO, SW_DOMAIN, SW_OU...

#ifdef __cplusplus
extern "C" {
#endif

/* =================================================== */
/*             Global Function Declarations            */
/* --------------------------------------------------- */
void SW_CTL_init_ptrs(SW_RUN *sw);

void SW_CTL_alloc_outptrs(SW_RUN *sw, LOG_INFO *LogInfo);

void SW_RUN_deepCopy(
    SW_RUN *source, SW_RUN *dest, SW_OUT_DOM *DomRun, LOG_INFO *LogInfo
);

void SW_CTL_setup_domain(
    unsigned long userSUID, SW_DOMAIN *SW_Domain, LOG_INFO *LogInfo
);

void SW_CTL_setup_model(
    SW_RUN *sw, SW_OUT_DOM *OutDom, Bool zeroOutInfo, LOG_INFO *LogInfo
);

void SW_CTL_clear_model(Bool full_reset, SW_RUN *sw);

void SW_CTL_init_run(SW_RUN *sw, LOG_INFO *LogInfo);

void SW_CTL_read_inputs_from_disk(
    SW_RUN *sw, SW_OUT_DOM *OutDom, PATH_INFO *PathInfo, LOG_INFO *LogInfo
);

void SW_CTL_main(SW_RUN *sw, SW_OUT_DOM *OutDom, LOG_INFO *LogInfo);

void SW_CTL_RunSimSet(
    SW_RUN *sw_template,
    SW_DOMAIN *SW_Domain,
    SW_WALLTIME *SW_WallTime,
    LOG_INFO *main_LogInfo
);

void SW_CTL_run_current_year(SW_RUN *sw, SW_OUT_DOM *OutDom, LOG_INFO *LogInfo);

void SW_CTL_run_spinup(SW_RUN *sw, SW_OUT_DOM *OutDom, LOG_INFO *LogInfo);

void SW_CTL_run_sw(
    SW_RUN *sw_template,
    SW_DOMAIN *SW_Domain,
    unsigned long ncSuid[],
    LOG_INFO *LogInfo
);


#ifdef __cplusplus
}
#endif

#endif
