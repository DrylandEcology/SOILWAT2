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
#include <signal.h>                 // for sig_atomic_t
#include <stddef.h>                 // for size_t

#ifdef __cplusplus
extern "C" {
#endif

extern volatile sig_atomic_t runSims;

/* =================================================== */
/*             Global Function Declarations            */
/* --------------------------------------------------- */
void SW_CTL_init_ptrs(SW_DOMAIN *SW_Domain, SW_RUN *sw);

void SW_RUN_deepCopy(
    SW_RUN *source, SW_RUN *dest, Bool copyWeatherHist, LOG_INFO *LogInfo
);

void SW_CTL_setup_domain(
    int rank,
    int worldSize,
    Bool renameDomainTemp,
    TimeInt runSimDayLen,
    SW_DOMAIN *SW_Domain,
    LOG_INFO *LogInfo
);

void SW_CTL_setup_model(
    SW_RUN *sw, SW_OUT_DOM *OutDom, Bool zeroOutInfo, LOG_INFO *LogInfo
);

void SW_CTL_clear_model(Bool full_reset, SW_RUN *sw);

void SW_CTL_init_run(SW_RUN *sw, LOG_INFO *siteLog, LOG_INFO *main_LogInfo);

void SW_CTL_read_inputs_from_disk(
    SW_RUN *sw,
    SW_DOMAIN *SW_Domain,
    Bool *hasConsistentSoilLayerDepths,
    LOG_INFO *LogInfo
);

void SW_CTL_sim_sites(
    SW_RUN *sw_template,
    SW_DOMAIN *SW_Domain,
    SW_RUN *SW_Runs,
    Bool initYear,
    LOG_INFO *siteLogs,
    LOG_INFO *main_LogInfo
);

void SW_CTL_run_daily_timesteps(
    int rank,
    SW_RUN *sw_template,
    TimeInt startDay,
    TimeInt endDay,
    double *tempVals,
    SW_SOIL_RUN_INPUTS *newSoils,
    SW_DOMAIN *SW_Domain,
    SW_RUN *SW_Runs,
    LOG_INFO *siteLogs,
    SW_WALLTIME *SW_WallTime,
    LOG_INFO *main_LogInfo
);

void SW_CTL_RunSimSet(
    int rank,
    int worldSize,
    SW_RUN *sw_template,
    SW_DOMAIN *SW_Domain,
    SW_WALLTIME *SW_WallTime,
    LOG_INFO *main_LogInfo
);

void SW_CTL_run_current_day(SW_RUN *sw, SW_OUT_DOM *OutDom, LOG_INFO *LogInfo);

void SW_CTL_run_spinup(SW_DOMAIN *SW_Domain, SW_RUN *sw, LOG_INFO *LogInfo);

void SW_CTL_run_sw(
    size_t runNum,
    SW_RUN_INPUTS *runInputs,
    SW_RUN *sw_template,
    SW_DOMAIN *SW_Domain,
    size_t ncSuid[],
    Bool copyWeather,
    size_t count[][2],
    double *tempVals,
    SW_WALLTIME *SW_WallTime,
    LOG_INFO *LogInfo
);


#ifdef __cplusplus
}
#endif

#endif
