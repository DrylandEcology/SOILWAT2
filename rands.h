/* rands.h -- contains some random number generators */
/* that could be useful in any program              */
/*
 * USES: rands.c
 *
 * REQUIRES: none
 */
/* Chris Bennett @ LTER-CSU 6/15/2000            */
/*    - 5/19/2001 - split from gen_funcs.c       */

#ifndef RANDS_H

#include <stdio.h>
#include <float.h>
#include "pcg/pcg_basic.h"

#ifdef __cplusplus
extern "C" {
#endif

/***************************************************
 * Basic definitions
 ***************************************************/

/* You can choose to use a shuffled random set
 based on the compiler's rand() (RAND_FAST=1)
 or a compiler-independent version (RAND_FAST=0)
 but the speed of the "fast" version depends
 of course on the compiler.

 Some tests I ran with the GNU compiler from
 Cygwin (for Wintel) showed the FAST version to be
 better distributed, although the time was very
 nearly the same.  I'd suggest comparing the results
 of the two functions again if you use a different
 compiler.
 */

typedef long RandListType;

/***************************************************
 * Function definitions
 ***************************************************/

void RandSeed(signed long seed, pcg32_random_t* pcg_rng);
double RandUni(pcg32_random_t* pcg_rng);
int RandUniRange(const long first, const long last, pcg32_random_t* pcg_rng);
double RandNorm(double mean, double stddev, pcg32_random_t* pcg_rng);
void RandUniList(long, long, long, RandListType[], pcg32_random_t* pcg_rng);
float RandBeta(float aa, float bb, pcg32_random_t* pcg_rng);


#ifdef __cplusplus
}
#endif

#define RANDS_H
#endif
