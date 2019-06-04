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

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {

	/* pointers to arrays of probabilities for each day saves some space */
	/* by not being allocated if markov weather not requested by user */
	/* alas, multi-dimensional arrays aren't so convenient */
  RealD
    *wetprob, /* probability of being wet today given a wet yesterday */
    *dryprob, /* probability of being wet today given a dry yesterday */
    *avg_ppt, /* mean precip (cm) of wet days */
    *std_ppt, /* std dev. for precip of wet days */
    *cfxw, /*correction factor for tmax for wet days */
    *cfxd, /*correction factor for tmax for dry days */
    *cfnw, /*correction factor for tmin for wet days */
    *cfnd, /*correction factor for tmin for dry days */
    u_cov[MAX_WEEKS][2], /* mean weekly maximum and minimum temperature in degree Celsius */
    v_cov[MAX_WEEKS][2][2]; /* covariance matrix */
  int ppt_events; /* number of ppt events generated this year */

} SW_MARKOV;

void SW_MKV_construct(void);
void SW_MKV_deconstruct(void);
Bool SW_MKV_read_prob(void);
Bool SW_MKV_read_cov(void);
void SW_MKV_setup(void);
void SW_MKV_today(TimeInt doy0, RealD *tmax, RealD *tmin, RealD *rain);

#ifdef DEBUG_MEM
void SW_MKV_SetMemoryRefs( void);
#endif


#ifdef __cplusplus
}
#endif

#endif

