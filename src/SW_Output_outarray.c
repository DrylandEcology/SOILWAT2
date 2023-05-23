/********************************************************/
/********************************************************/
/**
  @file
  @brief Output functionality for in-memory processing of outputs

  See the \ref out_algo "output algorithm documentation" for details.

  History:
  2018 June 15 (drs) moved functions from `SW_Output.c` and `rSW_Output.c`
*/
/********************************************************/
/********************************************************/


/* =================================================== */
/*                INCLUDES / DEFINES                   */
/* --------------------------------------------------- */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "include/SW_Model.h"

// externs `SW_Output`, `tOffset` `use_OutPeriod`, `used_OUTNPERIODS`,
//         `timeSteps`, `ncol_OUT`
#include "include/SW_Output_outarray.h"

#ifdef STEPWAT
#include "ST_defines.h"
#endif


/* =================================================== */
/*                  Global Variables                   */
/* --------------------------------------------------- */

/** \brief A 2-dim array of pointers to output arrays.

  \note This should be initialized to NULL because they are defined globally
    and thus have `static storage duration`.

  The variable p_OUT used by rSOILWAT2 for output and by STEPWAT2 for
  mean aggregation.
*/
RealD *p_OUT[SW_OUTNKEYS][SW_OUTNPERIODS];

/** \brief A 2-dim array of pointers to output arrays of standard deviations.

  \note This should be initialized to NULL because they are defined globally
    and thus have `static storage duration`.

  The variable p_OUTsd is used by STEPWAT2 for standard-deviation during
  aggregation. See also \ref p_OUT
*/
#define p_OUTsd
#undef p_OUTsd


#ifdef STEPWAT
extern GlobalType SuperGlobals;
RealD *p_OUTsd[SW_OUTNKEYS][SW_OUTNPERIODS];
#endif

size_t nrow_OUT[SW_OUTNPERIODS]; // number of years/months/weeks/days
size_t irow_OUT[SW_OUTNPERIODS]; // row index of current year/month/week/day output; incremented at end of each day
const IntUS ncol_TimeOUT[SW_OUTNPERIODS] = { 2, 2, 2, 1 }; // number of time header columns for each output period



/* =================================================== */
/*             Global Function Definitions             */
/* --------------------------------------------------- */

/** @brief Determine number of years/months/weeks/days used in simulation period

	@param[in] SW_Model Struct of type SW_MODEL holding basic time information
		about the simulation

	@sideeffect Set nrow_OUT using global variables SW_Model,
		SuperGlobals if compiled for STEPWAT2, and use_OutPeriod
*/
void SW_OUT_set_nrow(SW_MODEL* SW_Model)
{
	TimeInt i;
	size_t n_yrs;
	IntU startyear, endyear;
	#ifdef SWDEBUG
	int debug = 0;
	#endif

	startyear = SW_Model->startyr;

	#ifdef STEPWAT
	n_yrs = SuperGlobals.runModelYears;
	endyear = startyear + n_yrs + 1;

	#else
	n_yrs = SW_Model->endyr - SW_Model->startyr + 1;
	endyear = SW_Model->endyr;
	#endif

	nrow_OUT[eSW_Year] = n_yrs * use_OutPeriod[eSW_Year];
	nrow_OUT[eSW_Month] = n_yrs * MAX_MONTHS * use_OutPeriod[eSW_Month];
	nrow_OUT[eSW_Week] = n_yrs * MAX_WEEKS * use_OutPeriod[eSW_Week];

	nrow_OUT[eSW_Day] = 0;

	if (use_OutPeriod[eSW_Day])
	{
		if (n_yrs == 1)
		{
			nrow_OUT[eSW_Day] = SW_Model->endend - SW_Model->startstart + 1;

		} else
		{
			// Calculate the start day of first year
			nrow_OUT[eSW_Day] = Time_get_lastdoy_y(startyear) -
				SW_Model->startstart + 1;
			// and last day of last year.
			nrow_OUT[eSW_Day] += SW_Model->endend;

			// Cumulate days of years between first and last year
			for (i = startyear + 1; i < endyear; i++)
			{
				nrow_OUT[eSW_Day] += Time_get_lastdoy_y(i);
			}
		}
	}

	#ifdef SWDEBUG
	if (debug) {
		swprintf(
			"n(year) = %zu, n(month) = %zu, n(week) = %zu, n(day) = %zu\n",
			nrow_OUT[eSW_Year], nrow_OUT[eSW_Month], nrow_OUT[eSW_Week],
			nrow_OUT[eSW_Day]
		);
	}
	#endif
}

/**
@brief For each out key, the p_OUT array is set to NULL.
*/
void SW_OUT_deconstruct_outarray(void)
{
	IntUS i;
	OutKey k;

	ForEachOutKey(k) {
		for (i = 0; i < SW_OUTNPERIODS; i++) {
			Mem_Free(p_OUT[k][i]);
			p_OUT[k][i] = NULL;

			#ifdef STEPWAT
			Mem_Free(p_OUTsd[k][i]);
			p_OUTsd[k][i] = NULL;
			#endif
		}
	}

}


#ifdef RSOILWAT
/** @brief Corresponds to function `get_outstrleader` of `SOILWAT2-standalone

	@param[in] SW_Model Struct of type SW_MODEL holding basic time information
		about the simulation
	@param[in] pd Time period in simulation output (day/week/month/year)
    @param[out] p Allocated array to hold output periods for every output key
*/
void get_outvalleader(SW_MODEL* SW_Model, OutPeriod pd, RealD *p) {
	p[irow_OUT[pd] + nrow_OUT[pd] * 0] = SW_Model->simyear;

	switch (pd) {
		case eSW_Day:
			p[irow_OUT[eSW_Day] + nrow_OUT[eSW_Day] * 1] = SW_Model->doy; //base1
			break;

		case eSW_Week:
			p[irow_OUT[eSW_Week] + nrow_OUT[eSW_Week] * 1] = SW_Model->week + 1 - tOffset; // base0
			break;

		case eSW_Month:
			p[irow_OUT[eSW_Month] + nrow_OUT[eSW_Month] * 1] = SW_Model->month + 1 - tOffset; // base0
			break;

		case eSW_Year:
			break;
	}
}
#endif


#ifdef STEPWAT
/** @brief Handle the cumulative running mean and standard deviation
		@param k. The index (base0) for subsetting `p` and `psd`, e.g., as calculated by
			macro `iOUT` or `iOUT2`.
		@param n. The current iteration/repetition number (base1).
		@param x. The new value.
*/
void do_running_agg(RealD *p, RealD *psd, size_t k, IntU n, RealD x)
{
	RealD prev_val = p[k];

	p[k] = get_running_mean(n, prev_val, x);
	psd[k] = psd[k] + get_running_sqr(prev_val, p[k], x); // += didn't work with *psd
}


/** Set global STEPWAT2 output variables that aggregate across iterations/repetitions

    @param[in] SW_Output SW_OUTPUT array of size SW_OUTNKEYS which holds
		basic output information for all output keys

	Note: Compare with function `setGlobalrSOILWAT2_OutputVariables` in `rSW_Output.c`

	@sideeffect: `*p_OUT` and `*p_OUTsd` pointing to allocated arrays for
		each output period and output key.
	*/
void setGlobalSTEPWAT2_OutputVariables(SW_OUTPUT* SW_Output)
{
	IntUS i;
	size_t
		size,
		s = sizeof(RealD);
	OutKey k;

	ForEachOutKey(k) {
		for (i = 0; i < used_OUTNPERIODS; i++) {
			if (SW_Output[k].use && timeSteps[k][i] != eSW_NoTime)
			{
				size = nrow_OUT[timeSteps[k][i]] *
					(ncol_OUT[k] + ncol_TimeOUT[timeSteps[k][i]]);

				p_OUT[k][timeSteps[k][i]] = (RealD *) Mem_Calloc(size, s,
					"setGlobalSTEPWAT2_OutputVariables()");

				p_OUTsd[k][timeSteps[k][i]] = (RealD *) Mem_Calloc(size, s,
					"setGlobalSTEPWAT2_OutputVariables()");
			}
		}
	}
}

#endif
