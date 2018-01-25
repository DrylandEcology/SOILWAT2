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
#include "SW_Sky.h"

#include "SW_Weather.h"
#ifdef RSOILWAT
  #include "../rSW_Weather.h"
#endif

/* =================================================== */
/*                  Global Variables                   */
/* --------------------------------------------------- */
extern SW_MODEL SW_Model;
extern SW_MARKOV SW_Markov;

#ifdef RSOILWAT
extern Bool collectInData;
#endif

SW_WEATHER SW_Weather; /* declared here, externed elsewhere */

Bool weth_found; /* swTRUE=success reading this years weather file */
RealD *runavg_list; /* used in run_tmp_avg() */

/* =================================================== */
/*                Module-Level Variables               */
/* --------------------------------------------------- */
static char *MyFileName;
static TimeInt tail;
static Bool firsttime;
static int wthdataIndex;

/* =================================================== */
/* =================================================== */
/*             Private Function Definitions            */
/* --------------------------------------------------- */

static void _update_yesterday(void);

void _clear_hist_weather(void) {
	/* --------------------------------------------------- */
	SW_WEATHER_HIST *wh = &SW_Weather.hist;
	TimeInt d;
	int i;
	SW_WEATHER_OUTPUTS *wo[7] = { &SW_Weather.dysum, &SW_Weather.wksum, &SW_Weather.mosum, &SW_Weather.yrsum, &SW_Weather.wkavg, &SW_Weather.moavg, &SW_Weather.yravg };

	for (d = 0; d < MAX_DAYS; d++)
		wh->ppt[d] = wh->temp_max[d] = wh->temp_min[d] = WTH_MISSING;

	for (i = 0; i < 7; i++)
		memset(wo[i], 0, sizeof(SW_WEATHER_OUTPUTS));
}

static void _clear_runavg(void) {
	/* --------------------------------------------------- */
	TimeInt i;

	for (i = 0; i < SW_Weather.days_in_runavg; i++)
		runavg_list[i] = WTH_MISSING;
}

void SW_WTH_clear_runavg_list(void) {
	free(runavg_list);
	runavg_list = NULL;
}

#ifdef STEPWAT
static RealD _runavg_temp(RealD avg) {
	/* --------------------------------------------------- */
	int i, cnt, numdays;
	RealD sum = 0.;

	runavg_list[tail] = avg;
	numdays = (SW_Model.doy < SW_Weather.days_in_runavg) ? SW_Model.doy : SW_Weather.days_in_runavg;

	for (i = 0, cnt = 0; i < numdays; i++, cnt++) {
		if (!missing(runavg_list[i])) {
			sum += runavg_list[i];
		}
	}
	tail = (tail < (SW_Weather.days_in_runavg - 1)) ? tail + 1 : 0;
	return ((cnt) ? sum / cnt : WTH_MISSING);
}
#endif

static void _todays_weth(RealD *tmax, RealD *tmin, RealD *ppt) {
	/* --------------------------------------------------- */
	/* If use_markov=swFALSE and no weather file found, we won't
	 * get this far because the new_year() will fail, so if
	 * no weather file found and we make it here, use_markov=swTRUE
	 * and we call mkv_today().  Otherwise, we're using this
	 * year's weather file and this logic sets today's value
	 * to yesterday's if today's is missing.  This may not
	 * always be most desirable, especially for ppt, so its
	 * default is 0.
	 */
	SW_WEATHER *w = &SW_Weather;
	TimeInt doy = SW_Model.doy - 1;

	if (!weth_found) {
		*ppt = w->now.ppt[Yesterday]; /* reqd for markov */
		SW_MKV_today(doy, tmax, tmin, ppt);

	} else {
		*tmax = (!missing(w->hist.temp_max[doy])) ? w->hist.temp_max[doy] : w->now.temp_max[Yesterday];
		*tmin = (!missing(w->hist.temp_min[doy])) ? w->hist.temp_min[doy] : w->now.temp_min[Yesterday];
		*ppt = (!missing(w->hist.ppt[doy])) ? w->hist.ppt[doy] : 0.;
	}

}

/* =================================================== */
/* =================================================== */
/*             Public Function Definitions             */
/* --------------------------------------------------- */
void SW_WTH_construct(void) {
	/* =================================================== */
	tail = 0;
	firsttime = swTRUE;
	/* clear the module structure */
	memset(&SW_Weather, 0, sizeof(SW_WEATHER));
	SW_Markov.ppt_events = 0;
	wthdataIndex = 0;
}

void SW_WTH_init(void) {
	/* =================================================== */
	/* nothing to initialize */
	/* this is a stub to make all objects more consistent */

}

void SW_WTH_new_year(void) {
	/* =================================================== */
	SW_WEATHER *w = &SW_Weather;
	SW_WEATHER_2DAYS *wn = &SW_Weather.now;
	TimeInt year = SW_Model.year;

	_clear_runavg();
	memset(&SW_Weather.yrsum, 0, sizeof(SW_WEATHER_OUTPUTS));

	if (year < SW_Weather.yr.first) {
		weth_found = swFALSE;
	} else {
#ifdef RSOILWAT
		weth_found = swFALSE;
		rSW_WTH_new_year2(year);
		wthdataIndex++;
#else
		weth_found = _read_weather_hist(year);
#endif
	}

	if (!weth_found && !SW_Weather.use_markov) {
		LogError(logfp, LOGFATAL, "Markov Simulator turned off and weather file found not for year %d", year);
	}

	/* setup today's weather because it's used as a default
	 * value when weather for the first day is missing.
	 * Notice that temps of 0. are reasonable for January
	 * (doy=1) and are below the critical temps for freezing
	 * and with ppt=0 there's nothing to freeze.
	 */
	if (!weth_found && firsttime) {
		wn->temp_max[Today] = wn->temp_min[Today] = wn->ppt[Today] = wn->rain[Today] = 0.;
		w->snow = w->snowmelt = w->snowloss = 0.;
		// wn->gsppt = 0.;

		w->snowRunoff = w->surfaceRunoff = w->surfaceRunon = w->soil_inf = 0.;
	}

	firsttime = swFALSE;
}

void SW_WTH_end_day(void) {
	/* =================================================== */
	_update_yesterday();
}

void SW_WTH_new_day(void) {
	/* =================================================== */
	/* guarantees that today's weather will not be invalid
	 * via _todays_weth()
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
	// wn->temp_run_avg[Today] = _runavg_temp(wn->temp_avg[Today]);

	ppt *= w->scale_precip[month];

	wn->ppt[Today] = wn->rain[Today] = ppt;
	w->snow = w->snowmelt = w->snowloss = 0.;
	w->snowRunoff = w->surfaceRunoff = w->surfaceRunon = w->soil_inf = 0.;

	if (w->use_snow)
	{
		SW_SWC_adjust_snow(wn->temp_min[Today], wn->temp_max[Today], wn->ppt[Today],
		  &wn->rain[Today], &w->snow, &w->snowmelt);
  }

#ifdef STEPWAT
	/* This is a nice idea but doesn't work as I'd like, so
	 * I'll go back to letting STEPPE handle it for now.

	 is_warm = (wn->temp_run_avg[Today] > 1.0);

	 is_growingseason = (doy >= m->startstart && doy <= m->daymid)
	 ? is_warm
	 : (is_growingseason && is_warm);
	 if (doy >= m->startstart && doy <= m->daymid && is_growingseason)
	 wn->gsppt = wn->gsppt + wn->ppt[Today];
	 else if (is_growingseason)
	 wn->gsppt += wn->ppt[Today];
	 */
#endif

}

void SW_WTH_read(void) {
	/* =================================================== */
	SW_WEATHER *w = &SW_Weather;
	const int nitems = 18;
	FILE *f;
	int lineno = 0, month, x;
	RealF sppt, stmax, stmin;
	RealF sky,wind,rH,transmissivity;

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
			w->use_markov = itob(atoi(inbuf));
			break;
		case 4:
			w->yr.first = YearTo4Digit(atoi(inbuf));
			break;
		case 5:
			w->days_in_runavg = atoi(inbuf);
			runavg_list = (RealD *) Mem_Calloc(w->days_in_runavg, sizeof(RealD), "SW_WTH_read()");
			break;
		default:
			if (lineno == 6 + MAX_MONTHS)
				break;
			x = sscanf(inbuf, "%d %f %f %f %f %f %f %f", &month, &sppt, &stmax, &stmin,&sky,&wind,&rH,&transmissivity);
			if (x < 4) {
				CloseFile(&f);
				LogError(logfp, LOGFATAL, "%s : Bad record %d.", MyFileName, lineno);
			}
			w->scale_precip[month - 1] = sppt;
			w->scale_temp_max[month - 1] = stmax;
			w->scale_temp_min[month - 1] = stmin;
			w->scale_skyCover[month - 1] = sky;
			w->scale_wind[month - 1] = wind;
			w->scale_rH[month - 1] = rH;
			w->scale_transmissivity[month - 1] = transmissivity;
		}
		lineno++;
	}
	SW_WeatherPrefix(w->name_prefix);
	CloseFile(&f);
	if (lineno < nitems - 1) {
		LogError(logfp, LOGFATAL, "%s : Too few input lines.", MyFileName);
	}
	w->yr.last = SW_Model.endyr;
	w->yr.total = w->yr.last - w->yr.first + 1;
	if (w->use_markov) {
		SW_MKV_construct();
		if (!SW_MKV_read_prob()) {
			LogError(logfp, LOGFATAL, "%s: Markov weather requested but could not open %s", MyFileName, SW_F_name(eMarkovProb));
		}
		if (!SW_MKV_read_cov()) {
			LogError(logfp, LOGFATAL, "%s: Markov weather requested but could not open %s", MyFileName, SW_F_name(eMarkovCov));
		}
	} else if (SW_Model.startyr < w->yr.first) {
		LogError(logfp, LOGFATAL, "%s : Model year (%d) starts before weather files (%d)"
				" and use_Markov=swFALSE.\nPlease synchronize the years"
				" or set up the Markov weather files", MyFileName, SW_Model.startyr, w->yr.first);
	}
	/* else we assume weather files match model run years */

	/* required for PET */
	SW_SKY_read();

	#ifdef RSOILWAT
	if(!collectInData)
	#endif
		SW_SKY_init(w->scale_skyCover, w->scale_wind, w->scale_rH, w->scale_transmissivity);
}


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
	int x, lineno = 0, k = 0, i, j, doy;
	RealF tmpmax, tmpmin, ppt, acc = 0.0;

	char fname[MAX_FILENAMESIZE];

	sprintf(fname, "%s.%4d", SW_Weather.name_prefix, year);

	if (NULL == (f = fopen(fname, "r")))
		return swFALSE;

	_clear_hist_weather();

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
		doy--;
		wh->temp_max[doy] = tmpmax;
		wh->temp_min[doy] = tmpmin;
		wh->temp_avg[doy] = (tmpmax + tmpmin) / 2.0;
		wh->ppt[doy] = ppt;

		/* Reassign if invalid values are found.  The values are
		 * either valid or WTH_MISSING.  If they were not
		 * present in the file, we wouldn't get this far because
		 * sscanf() would return too few items.
		 */
		if (missing(tmpmax)) {
			wh->temp_max[doy] = WTH_MISSING;
			LogError(logfp, LOGWARN, "%s : Missing max temp on doy=%d.", fname, doy + 1);
		}
		if (missing(tmpmin)) {
			wh->temp_min[doy] = WTH_MISSING;
			LogError(logfp, LOGWARN, "%s : Missing min temp on doy=%d.", fname, doy + 1);
		}
		if (missing(ppt)) {
			wh->ppt[doy] = 0.;
			LogError(logfp, LOGWARN, "%s : Missing PPT on doy=%d.", fname, doy + 1);
		}

		if (!missing(tmpmax) && !missing(tmpmin)) {
			k++;
			acc += wh->temp_avg[doy];
		}
	} /* end of input lines */

	wh->temp_year_avg = acc / (k + 0.0);

	x = 0;
	for (i = 0; i < MAX_MONTHS; i++) {
		k = 31;
		if (i == 8 || i == 3 || i == 5 || i == 10)
			k = 30; // september, april, june, & november all have 30 days...
		else if (i == 1) {
			k = 28; // february has 28 days, except if it's a leap year, in which case it has 29 days...
			if (isleapyear(year))
				k = 29;
		}

		acc = 0.0;
		for (j = 0; j < k; j++)
			acc += wh->temp_avg[j + x];
		wh->temp_month_avg[i] = acc / (k + 0.0);
		x += k;
	}

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
	// wn->temp_run_avg[Yesterday] = wn->temp_run_avg[Today];

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

	NoteMemoryRef(runavg_list);

}

#endif
