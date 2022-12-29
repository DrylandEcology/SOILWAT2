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
 03/12/2010	(drs) "365:366" -> Time_get_lastdoy_y(TimeInt year) {return isleapyear(year) ? 366 : 365; }
 09/26/2011	(drs)	added function interpolate_monthlyValues(): interpolating a record with monthly values and outputs a record with daily values
 */
/********************************************************/
/********************************************************/

/* =================================================== */
/*                INCLUDES / DEFINES                   */
/* --------------------------------------------------- */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "include/generic.h"
#include "include/Times.h"


/* =================================================== */
/*                  Local Variables                    */
/* --------------------------------------------------- */


static TimeInt
  /* mark February to catch use cases without properly setting arrays via
    a call to Time_new_year() */
  monthdays[12] = { 31, NoDay, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

static TimeInt
  days_in_month[MAX_MONTHS], /* number of days per month for "current" year */
  cum_monthdays[MAX_MONTHS]; /* monthly cumulative number of days for "current" year */




/* =================================================== */
/*             Global Function Definitions             */
/* --------------------------------------------------- */

/**
@brief Setup time array.
*/
void Time_init_model(void) {
  // called by `SW_MDL_construct()`

	memcpy(days_in_month, monthdays, sizeof(TimeInt) * MAX_MONTHS);
}

/**
  @brief Prepares information for a new year -- considering leap/noleap years.

  This function must be called prior to using Time_days_in_month(),
  doy2month(), doy2mday(), or interpolate_monthlyValues().

  @param year A (Gregorian) calendar year; not abbreviated.
*/
void Time_new_year(TimeInt year) {
  // called by `SW_MDL_new_year()`

	/* set the year's month-days arrays depending on leap/noleap year */
	TimeInt m;

	days_in_month[Feb] = isleapyear(year) ? 29 : 28;

	cum_monthdays[Jan] = days_in_month[Jan];
	for (m = Feb; m < NoMonth; m++)
	{
		cum_monthdays[m] = days_in_month[m] + cum_monthdays[m - 1];
	}
}


/**
  @brief Determine how many days a month has.

  @param month Number of month (base0) [Jan-Dec = 0-11]

  @return Number of days [1-366].
*/
TimeInt Time_days_in_month(TimeInt month) {
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

  @param doy Day of the year (base1) [1-366].
  @return Month (base0) [Jan-Dec = 0-11].

*/
TimeInt doy2month(const TimeInt doy) {
	TimeInt mon;

	for (mon = Jan; mon < Dec && doy > cum_monthdays[mon]; mon++)
		;
	return mon;
}

/**
  @brief Determine day of the month

  @param doy Day of the year (base1) [1-366].
  @return Day of the month [1-31].
*/
TimeInt doy2mday(const TimeInt doy) {
  return (doy <= days_in_month[Jan]) ? doy : doy - cum_monthdays[doy2month(doy) - 1];
}

/**
  @brief Determine 7-day period ("week") of the year

  @param doy Day of the year (base1) [1-366].
  @return Week number (base0) [0-51].
*/
TimeInt doy2week(TimeInt doy) {
	return ((TimeInt) (((doy) - 1) / WKDAYS));
}

/**
  @brief Formatting values as (Gregorian) calendar year.

  To handle legacy Y2K problems, it maps values of 0-50 to 2000-2050,
  values of 51-100 to 1951-2000, and passes through all other values unchanged.
  Useful for formatting user inputs.

  @return A value corresponding to a (Gregorian) calendar year.
*/
TimeInt yearto4digit(TimeInt yr) {
  // called by SW_MDL_read(), SW_WTH_read(), SW_SWC_read()
	return (TimeInt) ((yr > 100) ? yr : (yr < 50) ? 2000 + yr : 1900 + yr);
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
 @brief Linear interpolation of monthly value; monthly values are assumed to representative for the 15th of a month

 @author drs
 @date 09/22/2011

 @param[in] monthlyValues Array with values for each month
 @param[in] interpAsBase1 Boolean value specifying if "dailyValues" should be base1 or base0
 @param[out] dailyValues Array with linearly interpolated values for each day

 @note Aside from cloud cover, relative humidity, and wind speed in `allHist` in SW_WEATHER, dailyValues[0]
       will always be 0 as the function does not modify it since there is no day 0 (doy is base1), furthermore
       dailyValues is only sub-setted by base1 objects in the model.

 @note When the function encounters cloud cover, relative humidity, or wind speed in `allHist` in SW_WEATHER,
       `dailyValues` will be defaulted to base0 to be consistent with minimum/maximum temperature and
       precipitation in `allHist`. The function will know when one of the three cases is met by the parameter
       "interpAsBase1".
 **/
void interpolate_monthlyValues(double monthlyValues[], Bool interpAsBase1,
                               double dailyValues[]) {
	unsigned int doy, mday, month, month2 = NoMonth, nmdays;
    unsigned int startdoy = 1, endDay = MAX_DAYS, doyOffset = 0;
	double sign = 1.;

    // Check if we are interpolating values as base1
    if(!interpAsBase1) {

        // Make `dailyValues` base0
        startdoy = 0;
        endDay = MAX_DAYS - 1;
        doyOffset = 1;
    }

	for (doy = startdoy; doy <= endDay; doy++) {
		mday = doy2mday(doy + doyOffset);
		month = doy2month(doy + doyOffset);

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

			dailyValues[doy] = monthlyValues[month]
			  + sign * (monthlyValues[month2] - monthlyValues[month])
			  * (mday - 15.) / nmdays;
		}
	}
}
