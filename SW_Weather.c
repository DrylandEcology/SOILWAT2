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
#include "SW_Model.h" // externs SW_Model
#include "SW_SoilWater.h"
#include "SW_Markov.h"

#include "SW_Weather.h"
#ifdef RSOILWAT
  #include "../rSW_Weather.h"
#endif

#ifdef STEPWAT
  #include "../ST_globals.h" // externs `SuperGlobals
#endif



/* =================================================== */
/*                  Global Variables                   */
/* --------------------------------------------------- */

SW_WEATHER SW_Weather;

/* =================================================== */
/*                  Local Variables                    */
/* --------------------------------------------------- */

static char *MyFileName;


/* =================================================== */
/*             Local Function Definitions              */
/* --------------------------------------------------- */

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

/**
 @brief Reads in all weather data through all years and stores them in global SW_Weather's `allHist`
 
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
 
 @param[out] allHist 2D array holding all weather data gathered
 @param[in] startYear Start year of the simulation
 @param[in] n_years Number of years in simulation
 
 @note Function requires SW_MKV_today, SW_Weather.scale_temp_max and SW_Weather.scale_temp_min
 
 */

void readAllWeather(SW_WEATHER_HIST **allHist, int startYear, unsigned int n_years) {
    
    int day, yearDays, monthDays, month, currentMonDays, year;
    unsigned int yearIndex;
    
    double yesterdayPPT = 0., yesterdayMin = 0., yesterdayMax = 0.;
    
    Bool weth_found = swFALSE, no_missing = swTRUE;
    
    for(yearIndex = 0; yearIndex < n_years; yearIndex++) {
        year = yearIndex + startYear;
        yearDays = isleapyear(year) ? 366 : 365;
        monthDays = 31;
        month = 0;
        currentMonDays = 0;
        
        if(!SW_Weather.use_weathergenerator_only) {
            #ifdef RSOILWAT
            weth_found = onSet_WTH_DATA_YEAR(year, allHist[yearIndex]);
            #else
            weth_found = _read_weather_hist(year, allHist[yearIndex]);
            #endif
        }
        for(day = 0; day < yearDays; day++) {
            if(currentMonDays == monthDays) {
                month++;
                monthDays = (month == Feb) ? (isleapyear(year) ? 29 : 28) : Time_days_in_month(month);
                currentMonDays = 0;
            }
            currentMonDays++;
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
            if (!weth_found) {
                if(SW_Weather.use_weathergenerator) {
                    // no weather input file for current year ==> use weather generator
                allHist[yearIndex]->ppt[day] = yesterdayPPT;
                SW_MKV_today(day, &allHist[yearIndex]->temp_max[day],
                             &allHist[yearIndex]->temp_min[day], &allHist[yearIndex]->ppt[day]);
                } else {
                    LogError(logfp, LOGWARN, "Weather was not found and user specification "
                             "for using weather generator is off. Cannot generate "
                             "weather for day %d of year %d\n", day, year);
                }

            } else {
                // weather input file for current year available

                no_missing = (Bool) (!missing(allHist[yearIndex]->temp_max[day]) &&
                                        !missing(allHist[yearIndex]->temp_min[day]) &&
                                        !missing(allHist[yearIndex]->ppt[day]));
                if(!no_missing) {
                    // some of today's values are missing

                    if (SW_Weather.use_weathergenerator) {
                        // if weather generator is turned on then use it for all values
                        allHist[yearIndex]->ppt[day] = yesterdayPPT;
                        SW_MKV_today(day, &allHist[yearIndex]->temp_max[day],
                                     &allHist[yearIndex]->temp_min[day], &allHist[yearIndex]->ppt[day]);

                    } else {
                        // impute missing values with 0 for precipitation and
                        // with LOCF for temperature (i.e., last-observation-carried-forward)
                        allHist[yearIndex]->temp_max[day] = (!missing(allHist[yearIndex]->temp_max[day])) ?
                                allHist[yearIndex]->temp_max[day] : yesterdayMax;
                        
                        allHist[yearIndex]->temp_min[day] = (!missing(allHist[yearIndex]->temp_min[day])) ?
                                allHist[yearIndex]->temp_min[day] : yesterdayMin;
                        
                        allHist[yearIndex]->ppt[day] = (!missing(allHist[yearIndex]->ppt[day])) ?
                                allHist[yearIndex]->ppt[day] : 0.;
                    }
                }
            }
            /* scale the weather according to monthly factors */
            allHist[yearIndex]->temp_max[day] += SW_Weather.scale_temp_max[month];
            allHist[yearIndex]->temp_min[day] += SW_Weather.scale_temp_min[month];
            allHist[yearIndex]->temp_avg[day] = (allHist[yearIndex]->temp_max[day] +
                                                 allHist[yearIndex]->temp_min[day]) / 2.;
            
            allHist[yearIndex]->ppt[day] *= SW_Weather.scale_precip[month];
            
            yesterdayPPT = allHist[yearIndex]->ppt[day];
            yesterdayMax = allHist[yearIndex]->temp_max[day];
            yesterdayMin = allHist[yearIndex]->temp_min[day];
        }
        
    }
}


/* =================================================== */
/*             Global Function Definitions             */
/* --------------------------------------------------- */


/**
@brief Clears weather history.
@note Used by rSOILWAT2
*/
void _clear_hist_weather(SW_WEATHER_HIST *yearWeather) {
	/* --------------------------------------------------- */
	TimeInt d;

	for (d = 0; d < MAX_DAYS; d++)
        yearWeather->ppt[d] = yearWeather->temp_max[d] = yearWeather->temp_min[d] = SW_MISSING;
}


/* =================================================== */
/* =================================================== */
/*             Global Function Definitions             */
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
    SW_Weather.n_years = 0;
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
    deallocateAllWeather();
}

/**
 @brief Helper function to SW_WTH_deconstruct to deallocate allHist array.
 */

void deallocateAllWeather(void) {
    unsigned int year;
    
    if(!isnull(SW_Weather.allHist)) {
        for(year = 0; year < SW_Weather.n_years; year++) {
            free(SW_Weather.allHist[year]);
        }
        
        free(SW_Weather.allHist);
        
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
    TimeInt day = SW_Model.doy - 1, year = SW_Model.year - SW_Model.startyr;

#ifdef STEPWAT
	/*
	 TimeInt doy = SW_Model.doy;
	 Bool is_warm;
	 Bool is_growingseason = swFALSE;
	 */
#endif

    /* get the daily weather from allHist */
    wn->temp_max[Today] = w->allHist[year]->temp_max[day];
    wn->temp_min[Today] = w->allHist[year]->temp_min[day];
    wn->ppt[Today] = w->allHist[year]->ppt[day];

    wn->temp_avg[Today] = w->allHist[year]->temp_avg[day];

    w->snow = w->snowmelt = w->snowloss = 0.;
    w->snowRunoff = w->surfaceRunoff = w->surfaceRunon = w->soil_inf = 0.;

    if (w->use_snow)
    {
        SW_SWC_adjust_snow(wn->temp_min[Today], wn->temp_max[Today], wn->ppt[Today],
          &wn->rain[Today], &w->snow, &w->snowmelt);
    } else {
        wn->rain[Today] = wn->ppt[Today];
    }
}

/**
@brief Reads in file for SW_Weather.
*/
void SW_WTH_setup(void) {
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
			x = atoi(inbuf);
			w->yr.first = (x < 0) ? SW_Model.startyr : yearto4digit(x);
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
      "%s : Model year (%d) starts before weather files (%d) "
        "and the Markov weather generator is turned off. \n"
        "Please synchronize the years or "
        "activate the weather generator "
        "(and set up input files `mkv_prob.in` and `mkv_covar.in`).",
      MyFileName, SW_Model.startyr, w->yr.first
    );
	}
	/* else we assume weather files match model run years */
}

void SW_WTH_read(void) {
    
    #ifdef STEPWAT
    SW_Weather.n_years = SuperGlobals.runModelYears;
    #else
    SW_Weather.n_years = SW_Model.endyr - SW_Model.startyr + 1;
    #endif
    unsigned int year;

    deallocateAllWeather();
    
    SW_Weather.allHist = (SW_WEATHER_HIST **)malloc(sizeof(SW_WEATHER_HIST *) * SW_Weather.n_years);
    
    for(year = 0; year < SW_Weather.n_years; year++) {
        
        SW_Weather.allHist[year] = (SW_WEATHER_HIST *)malloc(sizeof(SW_WEATHER_HIST));
    }

    readAllWeather(SW_Weather.allHist, SW_Model.startyr, SW_Weather.n_years);
    
}


/** @brief Read the historical (observed) weather file for a simulation year.

    The naming convection of the weather input files:
      `[weather-data path/][weather-file prefix].[year]`

    Format of a input file (white-space separated values):
      `doy maxtemp(&deg;C) mintemp (&deg;C) precipitation (cm)`

    @note Used by rSOILWAT2

    @param year Current year within the simulation
    @param yearWeather Current year's weather array that is to be filled by function

    @return `swTRUE`/`swFALSE` if historical daily meteorological inputs are
      successfully/unsuccessfully read in.
*/
Bool _read_weather_hist(TimeInt year, SW_WEATHER_HIST *yearWeather) {
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

	FILE *f;
	int x, lineno = 0, doy;
	// TimeInt mon, j, k = 0;
	RealF tmpmax, tmpmin, ppt;
	// RealF acc = 0.0;

	char fname[MAX_FILENAMESIZE];

	sprintf(fname, "%s.%4d", SW_Weather.name_prefix, year);

	_clear_hist_weather(yearWeather); // clear values before returning

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
        yearWeather->temp_max[doy] = tmpmax;
        yearWeather->temp_min[doy] = tmpmin;
        yearWeather->temp_avg[doy] = (tmpmax + tmpmin) / 2.0;
        yearWeather->ppt[doy] = ppt;

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
