/********************************************************/
/********************************************************/
/*  Source file: SW_Output_outarray.c
  Type: module
  Application: SOILWAT - soilwater dynamics simulator
  Purpose: define functions to deal with array outputs; currently, used
    by rSOILWAT2 and STEPWAT2

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
#include "generic.h"
#include "filefuncs.h"
#include "myMemory.h"
#include "Times.h"

#include "SW_Defines.h"
#include "SW_Model.h"

#include "SW_Output.h"
#include "SW_Output_outarray.h"

/* =================================================== */
/*                  Global Variables                   */
/* --------------------------------------------------- */
extern SW_MODEL SW_Model;

// defined in `SW_Output.c`:
extern SW_OUTPUT SW_Output[];
extern TimeInt tOffset;
extern Bool use_OutPeriod[];


// defined here:

// 2-dim array of pointers to 2-dim output arrays
// `p_OUT` used by rSOILWAT2 for output and by STEPWAT2 for mean aggregation
RealD *p_OUT[SW_OUTNKEYS][SW_OUTNPERIODS];

#ifdef STEPWAT
// `p_OUTsd` used by STEPWAT2 for standard-deviation of mean aggregation
RealD *p_OUTsd[SW_OUTNKEYS][SW_OUTNPERIODS];
/** `prepare_IterationSummary` is TRUE if STEPWAT2 is called with `-o` flag
      and if STEPWAT2 is currently not in its last iteration/repetition.
      Compare with `print_IterationSummary` defined in `SW_Output_outtext.c`
*/
Bool prepare_IterationSummary;
#endif

IntUS nrow_OUT[SW_OUTNPERIODS]; // number of years/months/weeks/days
IntUS irow_OUT[SW_OUTNPERIODS]; // row index of current year/month/week/day output; incremented at end of each day
const IntUS ncol_TimeOUT[SW_OUTNPERIODS] = { 2, 2, 2, 1 }; // number of time header columns for each output period



/* =================================================== */
/* =================================================== */
/*             Private Function Declarations            */
/* --------------------------------------------------- */


/* =================================================== */
/* =================================================== */
/*             Private Function Definitions            */
/* --------------------------------------------------- */


/* =================================================== */
/* =================================================== */
/*             Function Definitions                    */
/*             (declared in SW_Output_outarray.h)      */
/* --------------------------------------------------- */

/** @brief Determine number of used years/months/weeks/days in simulation period

		@param SW_Model
		@param use_OutPeriod
		@sideeffects Set `nrow_OUT`
*/
void SW_OUT_set_nrow(void)
{
	TimeInt i;
	int n_yrs = SW_Model.endyr - SW_Model.startyr + 1;
	#ifdef SWDEBUG
	int debug = 0;
	#endif

	nrow_OUT[eSW_Year] = n_yrs * use_OutPeriod[eSW_Year];
	nrow_OUT[eSW_Month] = n_yrs * MAX_MONTHS * use_OutPeriod[eSW_Month];
	nrow_OUT[eSW_Week] = n_yrs * MAX_WEEKS * use_OutPeriod[eSW_Week];

	nrow_OUT[eSW_Day] = 0;

	if (use_OutPeriod[eSW_Day])
	{
		if (SW_Model.startyr == SW_Model.endyr)
		{
			nrow_OUT[eSW_Day] = SW_Model.endend - SW_Model.startstart + 1;

		} else
		{
			// Calculate the start day of first year
			nrow_OUT[eSW_Day] = Time_get_lastdoy_y(SW_Model.startyr) -
				SW_Model.startstart + 1;
			// and last day of last year.
			nrow_OUT[eSW_Day] += SW_Model.endend;

			// Cumulate days of years between first and last year
			for (i = SW_Model.startyr + 1; i < SW_Model.endyr; i++)
			{
				nrow_OUT[eSW_Day] += Time_get_lastdoy_y(i);
			}
		}
	}

	#ifdef SWDEBUG
	if (debug) {
		swprintf("n(year) = %d, n(month) = %d, n(week) = %d, n(day) = %d\n",
			nrow_OUT[eSW_Year], nrow_OUT[eSW_Month], nrow_OUT[eSW_Week],
			nrow_OUT[eSW_Day]);
	}
	#endif
}


#ifdef RSOILWAT
/** @brief Corresponds to function `get_outstrleader` of `SOILWAT2-standalone`
*/
void get_outvalleader(RealD *p, OutPeriod pd) {
	p[irow_OUT[pd] + nrow_OUT[pd] * 0] = SW_Model.simyear;

	switch (pd) {
		case eSW_Day:
			p[irow_OUT[eSW_Day] + nrow_OUT[eSW_Day] * 1] = SW_Model.doy;
			break;

		case eSW_Week:
			p[irow_OUT[eSW_Week] + nrow_OUT[eSW_Week] * 1] = SW_Model.week + 1 - tOffset;
			break;

		case eSW_Month:
			p[irow_OUT[eSW_Month] + nrow_OUT[eSW_Month] * 1] = SW_Model.month + 1 - tOffset;
			break;

		case eSW_Year:
			break;
	}
}
#endif


#ifdef STEPWAT
void do_running_agg(RealD *p, RealD *psd, IntUS k, IntUS n, RealD x)
{
	RealD prev_val = p[k];

	p[k] = get_running_mean(n, prev_val, x);
	psd[k] += get_running_sqr(prev_val, p[k], x);
}
#endif
