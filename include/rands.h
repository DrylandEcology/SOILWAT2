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
#include "include/SW_datastructs.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =================================================== */
/*                    Local Types                      */
/* --------------------------------------------------- */

typedef long RandListType;


/* =================================================== */
/*             Global Function Declarations            */
/* --------------------------------------------------- */
void RandSeed(
  unsigned long initstate,
  unsigned long initseq,
  sw_random_t* pcg_rng
);
double RandUni(sw_random_t* pcg_rng);
int RandUniIntRange(const long first, const long last, sw_random_t* pcg_rng);
float RandUniFloatRange(const float min, const float max, sw_random_t* pcg_rng);
double RandNorm(double mean, double stddev, sw_random_t* pcg_rng);
void RandUniList(long count, long first, long last, RandListType list[],
                 sw_random_t* pcg_rng, LOG_INFO* LogInfo);
float RandBeta(float aa, float bb, sw_random_t* pcg_rng, LOG_INFO* LogInfo);


#ifdef __cplusplus
}
#endif

#define RANDS_H
#endif
