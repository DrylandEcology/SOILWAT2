/********************************************************/
/********************************************************/
/*  Source file: Times.h
 *  Type: header
 *
 *  Purpose: Provides a consistent definition for the
 *           time values that might be needed by various
 *           objects.
 *  History:
 *    9/11/01 -- INITIAL CODING - cwb  Originally coded to
 *          be used in SOILWAT.
 *
 *    12/02 -- IMPORTANT CHANGE -- cwb
 *          After modifying the code to integrate with STEPPE
 *          I found that the old system of starting most
 *          arrays from 1 (aka base1) was causing more problems
 *          than it was worth, so I modified all the arrays and
 *          times and other indices to start from 0 (aka base0).
 *          The basic logic is that the user will see base1
 *          numbers while the code will use base0.  Unfortunately
 *          this will require careful attention to the points
 *          at which the conversions must be made, eg, month
 *          indices read from a file will be base1 as will
 *          such times that appear in the output.
 *
 * 2/14/03 - cwb - the features of this code appear to be
 *          rather generally useful, so I took out the
 *          soilwat specific stuff and made a general
 *          codeset.
 *
 * 2/24/03 - cwb - Moved Doy2Month to a function in Times.c
 *
 *    19-Sep-03 (cwb) Imported a bunch of new routines
 *       and added the facility for model time.
 *	  09/26/2011	(drs)	added function interpolate_monthlyValues()
 */
/********************************************************/
/********************************************************/

#ifndef TIMES_H
#define TIMES_H

#include "include/SW_datastructs.h"
#include "include/SW_Defines.h"
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif


/* =================================================== */
/*                    Local Defines                    */
/* --------------------------------------------------- */

/* constants for each month; this was previously a typedef enum.
 * Note: this has to be base0 and continuous. */
#define Jan 0
#define Feb 1
#define Mar 2
#define Apr 3
#define May 4
#define Jun 5
#define Jul 6
#define Aug 7
#define Sep 8
#define Oct 9
#define Nov 10
#define Dec 11
#define NoMonth 12

#define NoDay 999
#define WKDAYS 7

extern const TimeInt monthdays[];

/* =================================================== */
/*             Global Function Declarations            */
/* --------------------------------------------------- */
void Time_init_model(TimeInt days_in_month[]);

void Time_new_year(
    TimeInt year, TimeInt days_in_month[], TimeInt cum_monthdays[]
);

TimeInt Time_days_in_month(TimeInt month, TimeInt days_in_month[]);

TimeInt Time_get_lastdoy_y(TimeInt year);

TimeInt doy2month(const TimeInt doy, TimeInt cum_monthdays[]);

TimeInt doy2mday(
    const TimeInt doy, TimeInt cum_monthdays[], TimeInt days_in_month[]
);

TimeInt doy2week(TimeInt doy);

TimeInt yearto4digit(TimeInt yr);

Bool isleapyear(const TimeInt year);

void interpolate_monthlyValues(
    double monthlyValues[],
    Bool interpAsBase1,
    TimeInt cum_monthdays[],
    TimeInt days_in_month[],
    double dailyValues[]
);

void set_walltime(WallTimeSpec *ts, Bool *ok);

double diff_walltime(WallTimeSpec start, Bool ok_start);

void SW_WT_StartTime(SW_WALLTIME *wt);

void SW_WT_TimeRun(WallTimeSpec ts, Bool ok_ts, SW_WALLTIME *wt);

void SW_WT_ReportTime(SW_WALLTIME wt, LOG_INFO *LogInfo);

void timeStringISO8601(char *timeString, int stringLength);

#ifdef __cplusplus
}
#endif

#endif
