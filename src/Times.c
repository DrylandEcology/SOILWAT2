/********************************************************/
/********************************************************/
/*	Source file: Times.c
 Type: module

 Purpose: Collection of time-oriented functions.
 Allows the parent program (the "model") to maintain
 a separate clock internally, but the library can
 also work off the system clock (real time).

 History:
 (8/28/01) -- INITIAL CODING - cwb

 12/02 - IMPORTANT CHANGE - cwb
 refer to comments in Times.h regarding base0

 19-Sep-03 (cwb) Imported a bunch of new routines
 and added the facility for model time.

 03/12/2010	(drs) "365:366" -> Time_get_lastdoy_y(TimeInt year) {return
 isleapyear(year) ? 366 : 365; }

 09/26/2011	(drs)	added function interpolate_monthlyValues():
 interpolating a record with monthly values and outputs a record with daily
 values
 */
/********************************************************/
/********************************************************/

/* =================================================== */
/*                INCLUDES / DEFINES                   */
/* --------------------------------------------------- */

#include "include/Times.h"          // for Jan, Dec, Feb, NoMonth, NoDay
#include "include/filefuncs.h"      // for sw_message
#include "include/generic.h"        // for Bool, GE, final_running_sd, get_...
#include "include/SW_datastructs.h" // for SW_WALLTIME, LOG_INFO
#include "include/SW_Defines.h"     // for TimeInt, WallTimeSpec, MAX_DAYS
#include <stdio.h>                  // for fprintf, FILE, NULL, stdout
#include <string.h>                 // for NULL, memcpy
#include <time.h>                   // for time, difftime, gmtime, strftime


/* =================================================== */
/*                  Local Variables                    */
/* --------------------------------------------------- */

/* mark February to catch use cases without properly setting arrays via
  a call to Time_new_year() */
const TimeInt monthdays[MAX_MONTHS] = {
    31, NoDay, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
};

/* =================================================== */
/*             Global Function Definitions             */
/* --------------------------------------------------- */

/**
@brief Setup time array.

@param[out] days_in_month Number of days per month for "current" year
*/
void Time_init_model(TimeInt days_in_month[]) {
    // called by `SW_MDL_construct()`

    memcpy(days_in_month, monthdays, sizeof(TimeInt) * MAX_MONTHS);
}

/**
@brief Prepares information for a new year -- considering leap/noleap years.

This function must be called prior to using Time_days_in_month(),
doy2month(), doy2mday(), or interpolate_monthlyValues().

@param[in] year A (Gregorian) calendar year; not abbreviated.
@param[out] days_in_month Number of days per month for "current" year
@param[out] cum_monthdays Monthly cumulative number of days for "current" year
*/
void Time_new_year(
    TimeInt year, TimeInt days_in_month[], TimeInt cum_monthdays[]
) {

    // called by `SW_MDL_new_year()`

    /* set the year's month-days arrays depending on leap/noleap year */
    TimeInt m;

    days_in_month[Feb] = isleapyear(year) ? 29 : 28;

    cum_monthdays[Jan] = days_in_month[Jan];
    for (m = Feb; m < NoMonth; m++) {
        cum_monthdays[m] = days_in_month[m] + cum_monthdays[m - 1];
    }
}

/**
@brief Determine how many days a month has.

@param[in] month Number of month (base0) [Jan-Dec = 0-11]
@param[in] days_in_month Number of days per month for "current" year

@return Number of days [1-366].
*/
TimeInt Time_days_in_month(TimeInt month, TimeInt days_in_month[]) {
    // called by `average_for()`

    return days_in_month[month];
}

/**
@brief Determine last day of the year, checks for leapyear.

@return 366 or 365
*/
TimeInt Time_get_lastdoy_y(TimeInt year) {
    return isleapyear(year) ? 366 : 365;
}

/**
@brief Determine month of the year

@param[in] doy Day of the year (base1) [1-366].
@param[in] cum_monthdays Monthly cumulative number of days for "current" year

@return Month (base0) [Jan-Dec = 0-11].
*/
TimeInt doy2month(const TimeInt doy, TimeInt cum_monthdays[]) {
    TimeInt mon;

    for (mon = Jan; mon < Dec && doy > cum_monthdays[mon]; mon++)
        ;
    return mon;
}

/**
@brief Determine day of the month

@param doy Day of the year (base1) [1-366].
@param[in] cum_monthdays Monthly cumulative number of days for "current" year
@param[in] days_in_month Number of days per month for "current" year

@return Day of the month [1-31].
*/
TimeInt doy2mday(
    const TimeInt doy, TimeInt cum_monthdays[], TimeInt days_in_month[]
) {

    TimeInt new_doy0 = doy2month(doy, cum_monthdays) - 1;

    return (doy <= days_in_month[Jan]) ? doy : doy - cum_monthdays[new_doy0];
}

/**
@brief Determine 7-day period ("week") of the year

@param doy Day of the year (base1) [1-366].
@return Week number (base0) [0-51].
*/
TimeInt doy2week(TimeInt doy) { return ((TimeInt) (((doy) -1) / WKDAYS)); }

/**
@brief Formatting values as (Gregorian) calendar year.

To handle legacy Y2K problems, it maps values of 0-50 to 2000-2050,
values of 51-100 to 1951-2000, and passes through all other values unchanged.
Useful for formatting user inputs.

@return A value corresponding to a (Gregorian) calendar year.
*/
TimeInt yearto4digit(TimeInt yr) {
    // called by SW_MDL_read(), SW_WTH_read(), SW_SWC_read()
    TimeInt year = yr;

    if (yr <= 100) {
        year = (yr < 50) ? 2000 + yr : 1900 + yr;
    }

    return year;
}

/**
@brief Is a (Gregorian) calendar year a leap year?

@param year A (Gregorian) calendar year.
@return A Boolean TRUE/FALSE.
*/
Bool isleapyear(const TimeInt year) {
    TimeInt t = (year / 100) * 100;

    return (Bool) (((year % 4) == 0) && (((t) != year) || ((year % 400) == 0)));
}

/**
@brief Linear interpolation of monthly value; monthly values are assumed to
representative for the 15th of a month

@author drs
@date 09/22/2011

@param[in] monthlyValues Array with values for each month
@param[in] interpAsBase1 Boolean value specifying if "dailyValues" should be
    base1 or base0
@param[in] cum_monthdays Monthly cumulative number of days for "current" year
@param[in] days_in_month Number of days per month for "current" year
@param[out] dailyValues Array with linearly interpolated values for each day;
    `dailyValues` must be at least of length `MAX_DAYS` if `interpAsBase1` is
    FALSE , and length `MAX_DAYS + 1` if `interpAsBase1` is TRUE.

@note If `interpAsBase1` is TRUE, then `dailyValues[0]` is ignored (with a
value of 0) because a `base1` index for "day of year" (doy) is used, i.e., the
value on the first day of year (`doy = 1`) is located in `dailyValues[1]`. If
`interpAsBase1` is FALSE, then `dailyValues[0]` is utilized because a `base0`
index for "day of year" (doy) is used, i.e., the value on the first day of year
(`doy = 0`) is located in `dailyValues[0]`.
*/
void interpolate_monthlyValues(
    double monthlyValues[],
    Bool interpAsBase1,
    TimeInt cum_monthdays[],
    TimeInt days_in_month[],
    double dailyValues[]
) {
    unsigned int doy, mday, month, month2 = NoMonth, nmdays;
    unsigned int startdoy = 1, endDay = MAX_DAYS, doyOffset = 0;
    double sign = 1.;

    // Check if we are interpolating values as base1
    if (!interpAsBase1) {

        // Make `dailyValues` base0
        startdoy = 0;
        endDay = MAX_DAYS - 1;
        doyOffset = 1;
    }

    for (doy = startdoy; doy <= endDay; doy++) {
        mday = doy2mday(doy + doyOffset, cum_monthdays, days_in_month);
        month = doy2month(doy + doyOffset, cum_monthdays);

        if (mday == 15) {
            dailyValues[doy] = monthlyValues[month];

        } else {
            if (mday >= 15) {
                month2 = (month == Dec) ? Jan : month + 1;
                sign = 1;
                nmdays = days_in_month[month];

            } else {
                month2 = (month == Jan) ? Dec : month - 1;
                sign = -1;
                nmdays = days_in_month[month2];
            }

            dailyValues[doy] =
                monthlyValues[month] +
                sign * (monthlyValues[month2] - monthlyValues[month]) *
                    (mday - 15.) / nmdays;
        }
    }
}

/* Wall time functionality

  Ordered by preference, if available, utilize (defined in time.h)
    * (not implemented here) POSIX: clock_gettime(CLOCK_MONOTONIC, .)
    * C11 or later: timespec_get(., TIME_UTC)
    * time()
*/

void set_walltime(WallTimeSpec *ts, Bool *ok) {
    /*
      #if defined(_POSIX_C_SOURCE) && _POSIX_C_SOURCE >= 199309L
        int err = clock_gettime(CLOCK_MONOTONIC, ts);
        *ok = (err != 0) ? swTRUE : swFALSE;
    */

#if SW_TIMESPEC == 1
    /* C11 or later */
    int err = timespec_get(ts, TIME_UTC);
    *ok = (err != 0) ? swTRUE : swFALSE;

#else
    *ts = time(NULL);
    *ok = (*ts != (time_t) (-1)) ? swTRUE : swFALSE;
#endif
}

double diff_walltime(WallTimeSpec start, Bool ok_start) {
    WallTimeSpec end;
    Bool ok;
    double d = -1.; /* time lapsed since start [seconds] */

    if (ok_start) {
        set_walltime(&end, &ok);

        if (ok) {
#if SW_TIMESPEC == 1
            /* C11 or later */
            d = difftime(end.tv_sec, start.tv_sec) +
                (end.tv_nsec - start.tv_nsec) / 1000000000.;

#else
            d = difftime(end, start);
#endif
        }
    }

    return d;
}

void SW_WT_StartTime(SW_WALLTIME *wt) {
    set_walltime(&wt->timeStart, &wt->has_walltime);

    // unrealistic large value (1e12 s are c. 3.6 years)
    wt->wallTimeLimit = 1e12;

    wt->timeSimSet = -1;

    wt->timeMean = 0;
    wt->timeSS = 0;
    wt->timeSD = 0;
    wt->timeMin = 1e9; // unrealistic large value
    wt->timeMax = 0;

    wt->nTimedRuns = 0;
    wt->nUntimedRuns = 0;
}

/* Assumes that all values have been initialized */
void SW_WT_TimeRun(WallTimeSpec ts, Bool ok_ts, SW_WALLTIME *wt) {
    double ut = diff_walltime(ts, ok_ts); // negative if time failed
    double new_mean = 0;

    if (GE(ut, 0.)) {
        wt->nTimedRuns++;

        new_mean = get_running_mean(wt->nTimedRuns, wt->timeMean, ut);

        wt->timeSS += get_running_sqr(wt->timeMean, new_mean, ut);

        wt->timeMean = new_mean;

        wt->timeMin = fmin(ut, wt->timeMin);
        wt->timeMax = fmax(ut, wt->timeMax);


    } else {
        wt->nUntimedRuns++;
    }
}

/** Write time report

Reports on total wall time, number of simulations timed in the simulation set,
average time for a simulation run and variation among simulation runs
(standard deviation, SD; min, minimum; max, maximum),
as well as the proportion of wall time spent during the loop over the
simulation set relative to the total wall time.

Time report is written to
    + `logfile`, if quiet mode is active (`-q` flag) and `logfile` is not `NULL`
    + `stdout`, if not quiet mode

Time is not reported at all if quiet mode and `logfile` is `NULL`.

@param[in] wt Object with timing information.
@param[in] LogInfo Holds information on warnings and errors
*/
void SW_WT_ReportTime(SW_WALLTIME wt, LOG_INFO *LogInfo) {
    double total_time = 0;
    unsigned long nSims = wt.nTimedRuns + wt.nUntimedRuns;
    int fprintRes = 0;

    FILE *logfp = LogInfo->QuietMode ? LogInfo->logfp : stdout;

    if (isnull(logfp)) {
        return;
    }

    fprintRes = fprintf(logfp, "Time report\n");
    if (fprintRes < 0) {
        goto wrapUpErrMsg;
    }

    // negative if failed
    total_time = diff_walltime(wt.timeStart, wt.has_walltime);

    if (GE(total_time, 0.)) {
        fprintRes = fprintf(
            logfp, "    * Total wall time: %.2f [seconds]\n", total_time
        );
    } else {
        fprintRes = fprintf(logfp, "    * Wall time failed.\n");
    }
    if (fprintRes < 0) {
        goto wrapUpErrMsg;
    }

    if (nSims > 1) {
        fprintRes =
            fprintf(logfp, "    * Number of simulation runs: %lu", nSims);
        if (fprintRes < 0) {
            goto wrapUpErrMsg;
        }

        if (wt.nUntimedRuns > 0) {
            fprintRes = fprintf(
                logfp,
                " total (%lu timed | %lu untimed)",
                wt.nTimedRuns,
                wt.nUntimedRuns
            );
            if (fprintRes < 0) {
                goto wrapUpErrMsg;
            }
        }

        fprintRes = fprintf(logfp, "\n");
        if (fprintRes < 0) {
            goto wrapUpErrMsg;
        }

        if (wt.nTimedRuns > 0) {
            fprintRes = fprintf(
                logfp,
                "    * Variation among simulation runs: "
                "%.3f mean (%.3f SD, %.3f-%.3f min-max) [seconds]\n",
                wt.timeMean,
                final_running_sd(wt.nTimedRuns, wt.timeSS),
                wt.timeMin,
                wt.timeMax
            );
            if (fprintRes < 0) {
                goto wrapUpErrMsg;
            }
        }
    }

    if (GT(total_time, 0.) && GE(wt.timeSimSet, 0.)) {
        fprintRes = fprintf(
            logfp,
            "    * Wall time for simulation set: %.2f %% [percent of total "
            "wall time]\n",
            100. * wt.timeSimSet / total_time
        );
    }

wrapUpErrMsg: {
    if (fprintRes < 0) {
        sw_message("Failed to write whole time report.");
    }
}
}

/**
@brief Current date and time in UTC formatted according to ISO 8601

@param[out] timeString Character array that returns the formatted time.
@param[in] stringLength Length of timeString (should be at least 21).
*/
void timeStringISO8601(char *timeString, int stringLength) {
    time_t t = time(NULL);
    if (strftime(timeString, stringLength, "%FT%TZ", gmtime(&t)) == 0) {
        timeString[0] = '\0';
    }
}
