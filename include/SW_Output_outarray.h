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

/** iOUT returns the index to the `i`-th column for time period `pd` in an
  output array that is organized by columns where `i` is base0 and
  `pd` is `OutPeriod`. The index order has to match up with column names as
  defined by `SW_OUT_set_colnames`.
*/
#define iOUT(i, pd, GenOutput) \
            ((GenOutput.irow_OUT[(pd)]) + (GenOutput.nrow_OUT[(pd)]) * \
            (ncol_TimeOUT[(pd)] + (i)))

/** iOUT2 returns the index to the `i`-th (soil layer) column
  within the `k`-th (vegetation type) column block for time period `pd` in an
  output array that is organized by columns where `i` and `k` are base0 and
  `pd` is `OutPeriod`. The index order has to match up with column names as
  defined by `SW_OUT_set_colnames`.
*/
#define iOUT2(i, k, pd, GenOutput, n_layers) \
              ((GenOutput.irow_OUT[(pd)]) + (GenOutput.nrow_OUT[(pd)]) * (ncol_TimeOUT[(pd)] + \
               (i) + (n_layers) * (k)))


/* =================================================== */
/*            Externed Global Variables                */
/* --------------------------------------------------- */

extern const IntUS ncol_TimeOUT[SW_OUTNPERIODS];

/* =================================================== */
/*             Global Function Declarations            */
/* --------------------------------------------------- */
void SW_OUT_set_nrow(SW_MODEL* SW_Model, Bool use_OutPeriod[],
					           size_t nrow_OUT[]);
void SW_OUT_deconstruct_outarray(SW_GEN_OUT *GenOutput);

#ifdef RSOILWAT
void get_outvalleader(SW_MODEL* SW_Model, OutPeriod pd,
	size_t irow_OUT[], size_t nrow_OUT[], TimeInt tOffset, RealD *p);
#endif

#ifdef STEPWAT
void do_running_agg(RealD *p, RealD *psd, size_t k, IntU n, RealD x);
void setGlobalSTEPWAT2_OutputVariables(SW_OUTPUT* SW_Output, SW_GEN_OUT *GenOutput,
									   LOG_INFO *LogInfo);
#endif


#ifdef __cplusplus
}
#endif


#endif
