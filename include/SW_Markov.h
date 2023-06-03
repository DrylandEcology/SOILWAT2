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

#include "include/SW_datastructs.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =================================================== */
/*             Global Function Declarations            */
/* --------------------------------------------------- */
void SW_MKV_construct(unsigned long rng_seed, LOG_INFO* LogInfo,
					  SW_MARKOV* SW_Markov);
void SW_MKV_deconstruct(SW_MARKOV* SW_Markov);
Bool SW_MKV_read_prob(LOG_INFO* LogInfo, char *InFiles[],
					  SW_MARKOV* SW_Markov);
Bool SW_MKV_read_cov(LOG_INFO* LogInfo, char *InFiles[], SW_MARKOV* SW_Markov);
void SW_MKV_setup(LOG_INFO* LogInfo, SW_MARKOV* SW_Markov,
	unsigned long Weather_rng_seed, int Weather_genWeathMethod,
	char *InFiles[]);
void SW_MKV_today(SW_MARKOV* SW_Markov, TimeInt doy0, TimeInt year,
			    LOG_INFO* LogInfo, RealD *tmax, RealD *tmin, RealD *rain);

#ifdef DEBUG_MEM
void SW_MKV_SetMemoryRefs( void);
#endif


#ifdef __cplusplus
}
#endif

#endif

