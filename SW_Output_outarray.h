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

#ifdef __cplusplus
extern "C" {
#endif


/** iOUT returns the index to the `i`-th column for time period `pd` in an
  output array that is organized by columns where `i` is base0 and
  `pd` is `OutPeriod`. The index order has to match up with column names as
  defined by `SW_OUT_set_colnames`.
*/
#define iOUT(i, pd) (irow_OUT[(pd)] + nrow_OUT[(pd)] * (ncol_TimeOUT[(pd)] + (i)))

/** iOUT2 returns the index to the `i`-th (soil layer) column
  within the `k`-th (vegetation type) column block for time period `pd` in an
  output array that is organized by columns where `i` and `k` are base0 and
  `pd` is `OutPeriod`. The index order has to match up with column names as
  defined by `SW_OUT_set_colnames`.
*/
#define iOUT2(i, k, pd) (irow_OUT[(pd)] + nrow_OUT[(pd)] * \
	(ncol_TimeOUT[(pd)] + (i) + SW_Site.n_layers * (k)))



// Function declarations
void SW_OUT_set_nrow(void);
void SW_OUT_deconstruct_outarray(void);

#ifdef RSOILWAT
void get_outvalleader(RealD *p, OutPeriod pd);
#endif

#ifdef STEPWAT
void do_running_agg(RealD *p, RealD *psd, size_t k, IntU n, RealD x);
void setGlobalSTEPWAT2_OutputVariables(void);
#endif


#ifdef __cplusplus
}
#endif


#endif
