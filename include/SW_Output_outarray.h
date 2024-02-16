/********************************************************/
/********************************************************/
/*  Source file: SW_Output_outarray.h
  Type: header
  Purpose: Support for SW_Output_outarray.c
  Application: SOILWAT - soilwater dynamics simulator
  Purpose: define functions to deal with array outputs; currently, used
    by rSOILWAT2 and STEPWAT2

  History:
  2018 June 15 (drs) moved functions from `SW_Output.c`
 */
/********************************************************/
/********************************************************/

#ifndef SW_OUTPUT_ARRAY_H
#define SW_OUTPUT_ARRAY_H

#include "include/SW_datastructs.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =================================================== */
/*                    Local Defines                    */
/* --------------------------------------------------- */



/** @brief Position in an output array `p_OUT[OutKey][OutPeriod]`

  The position is specified by
      - `varId` the current (`i`-th) variable within current output group `OutKey`
        (soil layers and vegetation types do not count towards "variables")
      - `timeId` the current time index (e.g., `GenOutput->irow_OUT[OutPeriod]`)
      - `slId` the current (`k`-th) soil layer; set to 0 if no soil layers
      - `ptfId` the current (`n`-th) vegetation type; set to 0 if no vegetation

  The correct dimension of the output array `p_OUT[OutKey][OutPeriod]`
  is inferred from
      - `nTime` the total number of time steps in the current `OutPeriod`
        (e.g., `GenOutput->nrow_OUT[OutPeriod]`)
      - `nSl` the total number of soil layers (e.g., `Site.n_layers`);
        set to 1 if no soil layers
      - `nPTF` the total number of vegetation types (e.g., `NVEGTYPES`);
        set to 1 if no vegetation
      - `ncol_TimeOUT_pd` the number of header (time) variables for the current
        `OutPeriod` (e.g., `ncol_TimeOUT[OutPeriod]`)

  Positions are consecutive along
      1. vegetation types (if present), then
      2. soil layers (if present), then
      3. time steps (of current output period `OutPeriod`), then
      4. variables within output group `OutKey`

  Thus, values for all soil layers and all vegetation types are contiguous
  at each time step.
*/
#define iOUTnc(varId, timeId, slId, ptfId, nTime, nSl, nPTF, ncol_TimeOUT_pd) \
            ((ptfId) + (nPTF) * ((slId) + (nSl) * ((timeId) + (nTime) * ((varId) + (ncol_TimeOUT_pd)))))


/** @brief Position in an output array `p_OUT[OutKey][OutPeriod]`

  The position is specified by
      - `varid` the `i`-th variable within current output group `OutKey`
        (see `SW_OUT_set_colnames`).
      - `timeid` the current time index (e.g., `GenOutput->irow_OUT[OutPeriod]`)

  The correct dimensions of the output array `p_OUT[OutKey][OutPeriod]`
  are inferred from
      - `nrow_OUT_pd` the number of time steps in the current `OutPeriod`
        (e.g., `GenOutput->nrow_OUT[OutPeriod]`)
      - `ncol_TimeOUT_pd` the number of header (time) variables for the current
        `OutPeriod` (e.g., `ncol_TimeOUT[OutPeriod]`)

  Positions are consecutive along consecutive `timeid` values for a given
  variable and output time step.
*/
#define iOUT(varid, timeid, nrow_OUT_pd, ncol_TimeOUT_pd) \
            ((timeid) + (nrow_OUT_pd) * ((ncol_TimeOUT_pd) + (varid)))



/** iOUT2 returns the index to the `i`-th (soil layer) column
  within the `k`-th (vegetation type) column block for time period `pd` in an
  output array that is organized by columns where `i` and `k` are base0 and
  `pd` is `OutPeriod`. The index order has to match up with column names as
  defined by `SW_OUT_set_colnames`.
*/
#define iOUT2(i, k, pd, irow_OUT, nrow_OUT, n_layers) \
              ((irow_OUT)[(pd)] + (nrow_OUT)[(pd)] * (ncol_TimeOUT[(pd)] + \
               (i) + (n_layers) * (k)))


#if defined(SWNETCDF)
    #undef iOUT
    #undef iOUT2
#else
    #undef iOUTnc
#endif


/* =================================================== */
/*            Externed Global Variables                */
/* --------------------------------------------------- */

extern const IntUS ncol_TimeOUT[SW_OUTNPERIODS];

/* =================================================== */
/*             Global Function Declarations            */
/* --------------------------------------------------- */
void SW_OUT_set_nrow(SW_MODEL* SW_Model, Bool use_OutPeriod[],
					           size_t nrow_OUT[]);

void SW_OUT_construct_outarray(SW_GEN_OUT *GenOutput, SW_OUTPUT* SW_Output,
									   LOG_INFO *LogInfo);
void SW_OUT_deconstruct_outarray(SW_GEN_OUT *GenOutput);

#ifdef RSOILWAT
void get_outvalleader(SW_MODEL* SW_Model, OutPeriod pd,
	size_t irow_OUT[], size_t nrow_OUT[], TimeInt tOffset, RealD *p);
#endif

#if defined(STEPWAT)
void do_running_agg(RealD *p, RealD *psd, size_t k, IntU n, RealD x);
#endif



#ifdef __cplusplus
}
#endif


#endif
