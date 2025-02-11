/********************************************************/
/********************************************************/
/*	Source file: SW_Markov.h
 Type: header
 Application: SOILWAT - soilwater dynamics simulator
 Purpose: Definitions for Markov-generated weather
 functions.
 History:
 (9/11/01) -- INITIAL CODING - cwb
 */
/********************************************************/
/********************************************************/
#ifndef SW_MARKOV_H
#define SW_MARKOV_H

#include "include/SW_datastructs.h" // for SW_MARKOV_INPUTS, LOG_INFO
#include "include/SW_Defines.h"     // for TimeInt

#ifdef __cplusplus
extern "C" {
#endif

/* =================================================== */
/*             Global Function Declarations            */
/* --------------------------------------------------- */
void SW_MKV_construct(unsigned long rng_seed, SW_MARKOV_INPUTS *SW_MarkovIn);

void copyMKV(SW_MARKOV_INPUTS *dest_MKV, SW_MARKOV_INPUTS *template_MKV);

Bool SW_MKV_read_prob(
    char *txtInFiles[], SW_MARKOV_INPUTS *SW_MarkovIn, LOG_INFO *LogInfo
);

Bool SW_MKV_read_cov(
    char *txtInFiles[], SW_MARKOV_INPUTS *SW_MarkovIn, LOG_INFO *LogInfo
);

void SW_MKV_setup(
    SW_MARKOV_INPUTS *SW_MarkovIn,
    unsigned long Weather_rng_seed,
    unsigned int Weather_genWeathMethod,
    char *txtInFiles[],
    LOG_INFO *LogInfo
);

void SW_MKV_today(
    SW_MARKOV_INPUTS *SW_MarkovIn,
    TimeInt doy0,
    TimeInt year,
    double *tmax,
    double *tmin,
    double *rain,
    LOG_INFO *LogInfo
);


#ifdef __cplusplus
}
#endif

#endif
