/********************************************************/
/********************************************************/
/*	Source file: Veg_Estab.h
        Type: header
        Application: SOILWAT - soilwater dynamics simulator
        Purpose: Supports Veg_Estab.c routines.
        History:
        (8/28/01) -- INITIAL CODING - cwb

        currently not used.
*/
/********************************************************/
/********************************************************/

#ifndef SW_VEGESTAB_H
#define SW_VEGESTAB_H

#include "include/generic.h"        // for Bool, IntU
#include "include/SW_datastructs.h" // for SW_VEGESTAB, SW_VEGESTAB_INFO, SW_...
#include "include/SW_Defines.h"     // for LyrIndex, TimeInt


#ifdef __cplusplus
extern "C" {
#endif


/* indices to bars[] */
#define SW_GERM_BARS 0
#define SW_ESTAB_BARS 1

/* =================================================== */
/*             Global Function Declarations            */
/* --------------------------------------------------- */
void SW_VES_read(
    SW_VEGESTAB_INPUTS *SW_VegEstabIn,
    SW_VEGESTAB_SIM *SW_VegEstabSim,
    SW_VEGESTAB_OUTPUTS *ves_p_accu,
    SW_VEGESTAB_OUTPUTS *ves_p_oagg,
    char *txtInFiles[],
    char *SW_ProjDir,
    LOG_INFO *LogInfo
);

void SW_VES_read2(
    SW_VEGESTAB_INPUTS *SW_VegEstabIn,
    SW_VEGESTAB_SIM *SW_VegEstabSim,
    SW_VEGESTAB_OUTPUTS *ves_p_accu,
    SW_VEGESTAB_OUTPUTS *ves_p_oagg,
    Bool use_VegEstab,
    Bool consider_InputFlag,
    char *txtInFiles[],
    char *SW_ProjDir,
    LOG_INFO *LogInfo
);

void SW_VES_init_ptrs(
    SW_VEGESTAB_INPUTS *SW_VegEstabIn,
    SW_VEGESTAB_SIM *SW_VegEstabSim,
    SW_VEGESTAB_OUTPUTS *ves_p_accu,
    SW_VEGESTAB_OUTPUTS *ves_p_oagg
);

void SW_VES_construct(
    SW_VEGESTAB_INPUTS *SW_VegEstabIn, SW_VEGESTAB_SIM *SW_VegEstabSim
);

void SW_VegEstab_alloc_outptrs(
    SW_VEGESTAB_OUTPUTS *ves_p_accu,
    SW_VEGESTAB_OUTPUTS *ves_p_oagg,
    IntU count,
    LOG_INFO *LogInfo
);

void SW_VES_deconstruct(
    SW_VEGESTAB_INPUTS *SW_VegEstabIn,
    SW_VEGESTAB_SIM *SW_VegEstabSim,
    SW_VEGESTAB_OUTPUTS *ves_p_accu,
    SW_VEGESTAB_OUTPUTS *ves_p_oagg
);

void SW_VES_init_run(
    SW_VEGESTAB_INFO_INPUTS *parmsIn,
    SW_SITE_INPUTS *SW_SiteIn,
    SW_SITE_SIM *SW_SiteSim,
    LyrIndex n_transp_lyrs[],
    IntU count,
    LOG_INFO *LogInfo
);

void SW_VES_checkestab(
    SW_VEGESTAB_INFO_INPUTS *parmsIn,
    SW_VEGESTAB_INFO_SIM *parmsSim,
    double avgTemp,
    double swcBulk[][MAX_LAYERS],
    TimeInt doy,
    TimeInt firstdoy,
    IntU count
);

void SW_VES_new_year(IntU count);

void spp_init(
    SW_VEGESTAB_INFO_INPUTS *parmsIn,
    unsigned int sppnum,
    SW_SITE_INPUTS *SW_SiteIn,
    SW_SITE_SIM *SW_SiteSim,
    LyrIndex n_transp_lyrs[],
    LOG_INFO *LogInfo
);

IntU new_species(
    SW_VEGESTAB_INPUTS *SW_VegEstabIn,
    SW_VEGESTAB_SIM *SW_VegEstabSim,
    LOG_INFO *LogInfo
);

void echo_VegEstab(
    const double width[],
    SW_VEGESTAB_INFO_INPUTS *parmsIn,
    IntU count,
    LOG_INFO *LogInfo
);


/* COMMENT-1
 * There are a lot of things to keep track of during the period of
 * possible germination to possible establishment, and the original
 * fortran variables were badly named (effect of 8 char limit?)
 * Here are the current variables and their fortran counterparts
 * in case somebody has to go back to the old code.


 max_drydays_postgerm          ------  numwait
 min_days_germ2estab           ------  minint
 max_days_germ2estab           ------  maxint
 wet_days_before_germ         ------  mingdys
 max_days_4germ                ------  maxgwait
 num_wet_days_for_estab        ------  nestabdy
 num_wet_days_for_germ         ------  ngermdy

 min_temp_germ                 ------  tmpmin
 max_temp_germ                 ------  tmpmax

 wet_swc_germ[]                ------  wetger
 wet_swc_estab[]               ------  wetest
 roots_wet                     ------  rootswet

 */


#ifdef __cplusplus
}
#endif

#endif
