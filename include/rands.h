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

#include "include/SW_datastructs.h" // for LOG_INFO
#include "include/SW_Defines.h"     // for sw_random_t
#include <stddef.h>                 // for size_t

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
void RandSeed(size_t initstate, size_t initseq, sw_random_t *pcg_rng);

double RandUni(sw_random_t *pcg_rng);

long RandUniIntRange(const long first, const long last, sw_random_t *pcg_rng);

float RandUniFloatRange(const float min, const float max, sw_random_t *pcg_rng);

double RandNorm(double mean, double stddev, sw_random_t *pcg_rng);

void RandUniList(
    long count,
    long first,
    long last,
    RandListType list[],
    sw_random_t *pcg_rng,
    LOG_INFO *LogInfo
);

double RandBeta(double aa, double bb, sw_random_t *pcg_rng, LOG_INFO *LogInfo);


#ifdef __cplusplus
}
#endif

#define RANDS_H
#endif
