/********************************************************/
/********************************************************/
/*	Source file: Weather.c
 Type: class
 Application: SOILWAT - soilwater dynamics simulator
 Purpose: Read / write and otherwise manage the model's
 weather-related information.
 History:
 (8/28/01) -- INITIAL CODING - cwb
 12/02 - IMPORTANT CHANGE - cwb
 refer to comments in Times.h regarding base0
 20090916 (drs) changed  in SW_WTH_new_day() the following code:
 wn->temp_avg[Today]     = (tmpmax + tmpmin) / 2.;
 to wn->temp_avg[Today]     = (wn->temp_max[Today] + wn->temp_min[Today]) / 2.;
 so that monthly scaling factors also influence average T and not only Tmin and Tmax
 20091009 (drs) moved call to SW_SWC_adjust_snow () to SW_Flow.c
 and split it up into snow accumulation and snow melt
 20091014 (drs) added pct_snowdrift as input to weathsetup.in
 20091015 (drs) ppt is divided into rain and snow, added snowmelt
 20101004 (drs) moved call to SW_SWC_adjust_snow() from SW_Flow.c back to SW_WTH_new_day(), see comment 20091009
 01/04/2011	(drs) added parameter '*snowloss' to function SW_SWC_adjust_snow() call and updated function SW_WTH_new_year()
 01/05/2011	(drs) removed unused variables rain and snow from SW_WTH_new_day()
 02/16/2011	(drs) added pct_runoff read from input file to SW_WTH_read()
 02/19/2011	(drs) to track 'runoff', updated functions SW_WTH_new_year(), SW_WTH_new_day()
 moved soil_inf from SW_Soilwat to SW_Weather
 09/30/2011	(drs)	weather name prefix no longer read in from file weathersetup.in with function SW_WTH_read(), but extracted from SW_Files.c:SW_WeatherPrefix()
 01/13/2011	(drs)	function '_read_hist' didn't close opened files: after reaching OS-limit of openend connections, no files could be read any more -> added 'fclose(f);' to close open connections after use
 06/01/2012  (DLM) edited _read_hist() function to calculate the yearly avg air temperature & the monthly avg air temperatures...
 11/30/2010	(clk) added appopriate lines for 'surfaceRunoff' in SW_WTH_new_year() and SW_WTH_new_day();
 06/21/2013	(DLM)	variable 'tail' got too large by 1 and leaked memory when accessing array runavg_list[] in function _runavg_temp(): changed test from '(tail < SW_Weather.days_in_runavg)' to '(tail < (SW_Weather.days_in_runavg-1))'
 in function _clear_hist_weather() temp_max was tested twice, one should be temp_min
 06/24/2013	(rjm) added function void SW_WTH_clear_runavg_list(void) to free memory
 06/24/2013	(rjm) 	made 'tail' and 'firsttime' a module-level static variable (instead of function-level): otherwise it will not get reset to 0 between consecutive calls as a dynamic library
 need to set these variables to 0 resp TRUE in function SW_WTH_construct()
 06/27/2013	(drs)	closed open files if LogError() with LOGFATAL is called in SW_WTH_read(), _read_hist()
 08/26/2013 (rjm) removed extern SW_OUTPUT never used.
 */
/********************************************************/
/********************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "generic.h"
#include "filefuncs.h"
#include "myMemory.h"
#include "Times.h"
#include "SW_Defines.h"
#include "SW_Files.h"
#include "SW_Model.h"
#include "SW_SoilWater.h"
#include "SW_Markov.h"

#include "SW_Weather.h"
#ifdef RSOILWAT
  #include "../rSW_Weather.h"
#endif

/* =================================================== */
/*                  Global Variables                   */
/* --------------------------------------------------- */
extern SW_MODEL SW_Model;

SW_WEATHER SW_Weather; /* declared here, externed elsewhere */

/** `swTRUE`/`swFALSE` if historical daily meteorological inputs
    are available/not available for the current simulation year
*/
Bool weth_found;


/* =================================================== */
/*                Module-Level Variables               */
/* --------------------------------------------------- */
static char *MyFileName;


/* =================================================== */
/* =================================================== */
/*             Private Function Definitions            */
/* --------------------------------------------------- */

static void _update_yesterday(void);

/**
@brief Clears weather history.
*/
void _clear_hist_weather(void) {
	/* --------------------------------------------------- */
	SW_WEATHER_HIST *wh = &SW_Weather.hist;
	TimeInt d;

	for (d = 0; d < MAX_DAYS; d++)
		wh->ppt[d] = wh->temp_max[d] = wh->temp_min[d] = SW_MISSING;
}


static void _todays_weth(RealD *tmax, RealD *tmin, RealD *ppt) {
	/* --------------------------------------------------- */
	/* If use_weathergenerator = swFALSE and no weather file found, we won't
	 * get this far because the new_year() will fail, so if
	 * no weather file found and we make it here, use_weathergenerator = swTRUE
	 * and we call mkv_today().  Otherwise, we're using this
	 * year's weather file and this logic sets today's value
	 * to yesterday's if today's is missing.  This may not
	 * always be most desirable, especially for ppt, so its
	 * default is 0.
	 */
	SW_WEATHER *w = &SW_Weather;
	TimeInt doy = SW_Model.doy - 1;
	Bool no_missing = swTRUE;

	if (!weth_found) {
		// no weather input file for current year ==> use weather generator
		*ppt = w->now.ppt[Yesterday]; /* reqd for markov */
		SW_MKV_today(doy, tmax, tmin, ppt);

	} else {
		// weather input file for current year available

		no_missing = (Bool) (!missing(w->hist.temp_max[doy]) &&
									!missing(w->hist.temp_min[doy]) &&
									!missing(w->hist.ppt[doy]));

		if (no_missing) {
			// all values available
			*tmax = w->hist.temp_max[doy];
			*tmin = w->hist.temp_min[doy];
			*ppt = w->hist.ppt[doy];

		} else {
			// some of today's values are missing

			if (SW_Weather.use_weathergenerator) {
				// if weather generator is turned on then use it for all values
				*ppt = w->now.ppt[Yesterday]; /* reqd for markov */
				SW_MKV_today(doy, tmax, tmin, ppt);

			} else {
				// impute missing values with 0 for precipitation and
				// with LOCF for temperature (i.e., last-observation-carried-forward)
				*tmax = (!missing(w->hist.temp_max[doy])) ? w->hist.temp_max[doy] : w->now.temp_max[Yesterday];
				*tmin = (!missing(w->hist.temp_min[doy])) ? w->hist.temp_min[doy] : w->now.temp_min[Yesterday];
				*ppt = (!missing(w->hist.ppt[doy])) ? w->hist.ppt[doy] : 0.;
			}
		}
	}

}

/* =================================================== */
/* =================================================== */
/*             Public Function Definitions             */
/* --------------------------------------------------- */

/**
@brief Constructor for SW_Weather.
*/
void SW_WTH_construct(void) {
	/* =================================================== */
	OutPeriod pd;

	// Clear the module structure:
	memset(&SW_Weather, 0, sizeof(SW_Weather));

	// Allocate output structures:
	ForEachOutPeriod(pd)
	{
		SW_Weather.p_accu[pd] = (SW_WEATHER_OUTPUTS *) Mem_Calloc(1,
			sizeof(SW_WEATHER_OUTPUTS), "SW_WTH_construct()");
		if (pd > eSW_Day) {
			SW_Weather.p_oagg[pd] = (SW_WEATHER_OUTPUTS *) Mem_Calloc(1,
				sizeof(SW_WEATHER_OUTPUTS), "SW_WTH_construct()");
		}
	}
}

/**
@brief Deconstructor for SW_Weather and SW_Markov (if used)
*/
void SW_WTH_deconstruct(void)
{
	OutPeriod pd;

	// De-allocate output structures:
	ForEachOutPeriod(pd)
	{
		if (pd > eSW_Day && !isnull(SW_Weather.p_oagg[pd])) {
			Mem_Free(SW_Weather.p_oagg[pd]);
			SW_Weather.p_oagg[pd] = NULL;
		}

		if (!isnull(SW_Weather.p_accu[pd])) {
			Mem_Free(SW_Weather.p_accu[pd]);
			SW_Weather.p_accu[pd] = NULL;
		}
	}

	if (SW_Weather.use_weathergenerator) {
		SW_MKV_deconstruct();
	}
}

/**
@brief Initialize weather variables for a simulation run

  They are used as default if weather for the first day is missing
*/
void SW_WTH_init_run(void) {
	/* setup today's weather because it's used as a default
	 * value when weather for the first day is missing.
	 * Notice that temps of 0. are reasonable for January
	 * (doy=1) and are below the critical temps for freezing
	 * and with ppt=0 there's nothing to freeze.
	 */
	SW_Weather.now.temp_max[Today] = SW_Weather.now.temp_min[Today] = 0.;
	SW_Weather.now.ppt[Today] = SW_Weather.now.rain[Today] = 0.;
	SW_Weather.snow = SW_Weather.snowmelt = SW_Weather.snowloss = 0.;
	SW_Weather.snowRunoff = 0.;
	SW_Weather.surfaceRunoff = SW_Weather.surfaceRunon = 0.;
	SW_Weather.soil_inf = 0.;
}


/** @brief Sets up daily meteorological inputs for current simulation year

  Meteorological inputs are required for each day; they can either be
  observed and provided via weather input files or they can be generated
  by a weather generator (which has separate input requirements).

  SOILWAT2 handles three scenarios of missing data:
    1. Some individual days are missing (set to the missing value)
    2. An entire year is missing (file `weath.xxxx` for year `xxxx` is absent)
    3. No daily weather input files are available

  SOILWAT2 may be set up such that the weather generator is exclusively:
    - Set the weather generator to exclusive use
  or
    1. Turn on the weather generator
    2. Set the "first year to begin historical weather" to a year after
       the last simulated year

  @sideeffect
    - if historical daily meteorological inputs are successfully located,
      then \ref weth_found is set to `swTRUE`
    - otherwise, \ref weth_found is `swFALSE`
*/
void SW_WTH_new_year(void) {

	if (
		SW_Weather.use_weathergenerator_only ||
		SW_Model.year < SW_Weather.yr.first
	) {
		weth_found = swFALSE;

	} else {
		#ifdef RSOILWAT
		weth_found = onSet_WTH_DATA_YEAR(SW_Model.year);
		#else
		weth_found = _read_weather_hist(SW_Model.year);
		#endif
	}

	if (!weth_found && !SW_Weather.use_weathergenerator) {
		LogError(
		  logfp,
		  LOGFATAL,
		  "Markov Simulator turned off and weather file not found for year %d",
		  SW_Model.year
		);
	}
}

/**
@brief Updates 'yesterday'.
*/
void SW_WTH_end_day(void) {
	/* =================================================== */
	_update_yesterday();
}

/**
@brief Guarantees that today's weather will not be invalid via -_todays_weth().
*/
void SW_WTH_new_day(void) {
	/* =================================================== */
	/*  History
	 *
	 *  20-Jul-2002 -- added growing season computation to
	 *                 facilitate the steppe/soilwat interface.
	 *  06-Dec-2002 -- modified the seasonal computation to
	 *                 account for n-s hemispheres.
	 *	16-Sep-2009 -- (drs) scaling factors were only applied to Tmin and Tmax
	 *					but not to Taverage -> corrected
	 *	09-Oct-2009	-- (drs) commented out snow adjustement, because call moved to SW_Flow.c
	 * 	20091015 (drs) ppt is divided into rain and snow
	 */

	SW_WEATHER *w = &SW_Weather;
	SW_WEATHER_2DAYS *wn = &SW_Weather.now;
	RealD tmpmax, tmpmin, ppt;
	TimeInt month = SW_Model.month;

#ifdef STEPWAT
	/*
	 TimeInt doy = SW_Model.doy;
	 Bool is_warm;
	 Bool is_growingseason = swFALSE;
	 */
#endif

	/* get the plain unscaled values */
	_todays_weth(&tmpmax, &tmpmin, &ppt);

	/* scale the weather according to monthly factors */
	wn->temp_max[Today] = tmpmax + w->scale_temp_max[month];
	wn->temp_min[Today] = tmpmin + w->scale_temp_min[month];

	wn->temp_avg[Today] = (wn->temp_max[Today] + wn->temp_min[Today]) / 2.;

	ppt *= w->scale_precip[month];

	wn->ppt[Today] = wn->rain[Today] = ppt;
	w->snow = w->snowmelt = w->snowloss = 0.;
	w->snowRunoff = w->surfaceRunoff = w->surfaceRunon = w->soil_inf = 0.;

	if (w->use_snow)
	{
		SW_SWC_adjust_snow(wn->temp_min[Today], wn->temp_max[Today], wn->ppt[Today],
		  &wn->rain[Today], &w->snow, &w->snowmelt);
  }
}

/**
@brief Reads in file for SW_Weather.
*/
void SW_WTH_read(void) {
	/* =================================================== */
	SW_WEATHER *w = &SW_Weather;
	const int nitems = 17;
	FILE *f;
	int lineno = 0, month, x;
	RealF sppt, stmax, stmin;
	RealF sky, wind, rH;

	MyFileName = SW_F_name(eWeather);
	f = OpenFile(MyFileName, "r");

	while (GetALine(f, inbuf)) {
		switch (lineno) {
		case 0:
			w->use_snow = itob(atoi(inbuf));
			break;
		case 1:
			w->pct_snowdrift = atoi(inbuf);
			break;
		case 2:
			w->pct_snowRunoff = atoi(inbuf);
			break;

		case 3:
			x = atoi(inbuf);
			if (x > 1) {
				w->use_weathergenerator_only = w->use_weathergenerator = swTRUE;
			} else {
				w->use_weathergenerator_only = swFALSE;
				w->use_weathergenerator = itob(x);
			}
			break;

		case 4:
			w->yr.first = yearto4digit(atoi(inbuf));
			break;

		default:
			if (lineno == 5 + MAX_MONTHS)
				break;

			x = sscanf(
				inbuf,
				"%d %f %f %f %f %f %f",
				&month, &sppt, &stmax, &stmin, &sky, &wind, &rH
			);

			if (x != 7) {
				CloseFile(&f);
				LogError(logfp, LOGFATAL, "%s : Bad record %d.", MyFileName, lineno);
			}

			month--; // convert to base0
			w->scale_precip[month] = sppt;
			w->scale_temp_max[month] = stmax;
			w->scale_temp_min[month] = stmin;
			w->scale_skyCover[month] = sky;
			w->scale_wind[month] = wind;
			w->scale_rH[month] = rH;
		}

		lineno++;
	}

	SW_WeatherPrefix(w->name_prefix);
	CloseFile(&f);

	if (lineno < nitems) {
		LogError(logfp, LOGFATAL, "%s : Too few input lines.", MyFileName);
	}

	w->yr.last = SW_Model.endyr;
	w->yr.total = w->yr.last - w->yr.first + 1;

	if (!w->use_weathergenerator && SW_Model.startyr < w->yr.first) {
    LogError(
      logfp,
      LOGFATAL,
      "%s : Model year (%d) starts before weather files (%d)"
        " and use_Markov=swFALSE.\nPlease synchronize the years"
        " or set up the Markov weather files",
      MyFileName, SW_Model.startyr, w->yr.first
    );
	}
	/* else we assume weather files match model run years */
}


/** @brief Read the historical (observed) weather file for a simulation year.

    The naming convection of the weather input files:
      `[weather-data path/][weather-file prefix].[year]`

    Format of a input file (white-space separated values):
      `doy maxtemp(&deg;C) mintemp (&deg;C) precipitation (cm)`

    @param year

    @return `swTRUE`/`swFALSE` if historical daily meteorological inputs are
      successfully/unsuccessfully read in.
*/
Bool _read_weather_hist(TimeInt year) {
	/* =================================================== */
	/* Read the historical (measured) weather files.
	 * Format is
	 * day-of-month, month number, year, doy, mintemp, maxtemp, ppt
	 *
	 * I dislike the inclusion of the first three columns
	 * but this is the old format.  If a new format emerges
	 * these columns will likely be removed.
	 *
	 * temps are in degrees C, ppt is in cm,
	 *
	 * 26-Jan-02 - changed format of the input weather files.
	 *
	 */

	SW_WEATHER_HIST *wh = &SW_Weather.hist;
	FILE *f;
	int x, lineno = 0, doy;
	// TimeInt mon, j, k = 0;
	RealF tmpmax, tmpmin, ppt;
	// RealF acc = 0.0;

	char fname[MAX_FILENAMESIZE];

	sprintf(fname, "%s.%4d", SW_Weather.name_prefix, year);

	_clear_hist_weather(); // clear values before returning

	if (NULL == (f = fopen(fname, "r")))
		return swFALSE;

	while (GetALine(f, inbuf)) {
		lineno++;
		x = sscanf(inbuf, "%d %f %f %f", &doy, &tmpmax, &tmpmin, &ppt);
		if (x < 4) {
			CloseFile(&f);
			LogError(logfp, LOGFATAL, "%s : Incomplete record %d (doy=%d).", fname, lineno, doy);
		}
		if (x > 4) {
			CloseFile(&f);
			LogError(logfp, LOGFATAL, "%s : Too many values in record %d (doy=%d).", fname, lineno, doy);
		}
		if (doy < 1 || doy > MAX_DAYS) {
			CloseFile(&f);
			LogError(logfp, LOGFATAL, "%s : Day of year out of range, line %d.", fname, lineno);
		}

		/* --- Make the assignments ---- */
		doy--; // base1 -> base0
		wh->temp_max[doy] = tmpmax;
		wh->temp_min[doy] = tmpmin;
		wh->temp_avg[doy] = (tmpmax + tmpmin) / 2.0;
		wh->ppt[doy] = ppt;

    // Calculate annual average temperature based on historical input, i.e.,
    // the `temp_year_avg` calculated here is prospective and unsuitable when
    // the weather generator is used to generate values for the
    // current simulation year, e.g., STEPWAT2
		/*
		if (!missing(tmpmax) && !missing(tmpmin)) {
			k++;
			acc += wh->temp_avg[doy];
		}
		*/
	} /* end of input lines */

  // Calculate annual average temperature based on historical input
	// wh->temp_year_avg = acc / (k + 0.0);

  // Calculate monthly average temperature based on historical input
  // the `temp_month_avg` calculated here is prospective and unsuitable when
  // the weather generator is used to generate values for the
  // current simulation year, e.g., STEPWAT2
  /*
	for (mon = Jan; mon <= Dec; i++) {
	  k = Time_days_in_month(mon);

		acc = 0.0;
		for (j = 0; j < k; j++)
		{
			acc += wh->temp_avg[j + x];
		}
		wh->temp_month_avg[i] = acc / (k + 0.0);
	}
  */

	fclose(f);
	return swTRUE;
}

static void _update_yesterday(void) {
	/* --------------------------------------------------- */
	/* save today's temp values as yesterday */
	/* this must be done after all calculations are
	 * finished for the day and before today's weather
	 * is read from the file.  Assumes Today's weather
	 * is always validated (non-missing).
	 */
	SW_WEATHER_2DAYS *wn = &SW_Weather.now;

	wn->temp_max[Yesterday] = wn->temp_max[Today];
	wn->temp_min[Yesterday] = wn->temp_min[Today];
	wn->temp_avg[Yesterday] = wn->temp_avg[Today];

	wn->ppt[Yesterday] = wn->ppt[Today];
	wn->rain[Yesterday] = wn->rain[Today];
}

#ifdef DEBUG_MEM
#include "myMemory.h"
/*======================================================*/
void SW_WTH_SetMemoryRefs( void) {
	/* when debugging memory problems, use the bookkeeping
	 code in myMemory.c
	 This routine sets the known memory refs in this module
	 so they can be  checked for leaks, etc.  All refs will
	 have been cleared by a call to ClearMemoryRefs() before
	 this, and will be checked via CheckMemoryRefs() after
	 this, most likely in the main() function.
	 */
}

#endif
