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
void SW_MKV_init_ptrs(SW_MARKOV* SW_Markov);
void SW_MKV_construct(unsigned long rng_seed, SW_MARKOV* SW_Markov,
					  LOG_INFO* LogInfo);
void SW_MKV_deconstruct(SW_MARKOV* SW_Markov);
Bool SW_MKV_read_prob(char *InFiles[], SW_MARKOV* SW_Markov,
					  LOG_INFO* LogInfo);
Bool SW_MKV_read_cov(char *InFiles[], SW_MARKOV* SW_Markov, LOG_INFO* LogInfo);
void SW_MKV_setup(SW_MARKOV* SW_Markov, unsigned long Weather_rng_seed,
	int Weather_genWeathMethod, char *InFiles[], LOG_INFO* LogInfo);
void SW_MKV_today(SW_MARKOV* SW_Markov, TimeInt doy0, TimeInt year,
			RealD *tmax, RealD *tmin, RealD *rain, LOG_INFO* LogInfo);

#ifdef DEBUG_MEM
void SW_MKV_SetMemoryRefs( void);
#endif


#ifdef __cplusplus
}
#endif

#endif

