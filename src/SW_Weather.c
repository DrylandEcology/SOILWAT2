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
 so that monthly scaling factors also influence average T and not only Tmin and
 Tmax

 20091009 (drs) moved call to SW_SWC_adjust_snow () to SW_Flow.c
 and split it up into snow accumulation and snow melt

 20091014 (drs) added pct_snowdrift as input to weathsetup.in

 20091015 (drs) ppt is divided into rain and snow, added snowmelt

 20101004 (drs) moved call to SW_SWC_adjust_snow() from SW_Flow.c back to
 SW_WTH_new_day(), see comment 20091009

 01/04/2011	(drs) added parameter '*snowloss' to function
 SW_SWC_adjust_snow() call and updated function SW_WTH_new_year()

 01/05/2011	(drs) removed unused variables rain and snow from
 SW_WTH_new_day()

 02/16/2011	(drs) added pct_runoff read from input file to SW_WTH_read()

 02/19/2011	(drs) to track 'runoff', updated functions SW_WTH_new_year(),
 SW_WTH_new_day() moved soil_inf from SW_Soilwat to SW_Weather

 09/30/2011	(drs)	weather name prefix no longer read in from file
 weathersetup.in with function SW_WTH_read(), but extracted from
 SW_Files.c:SW_WeatherPrefix()

 01/13/2011	(drs)	function '_read_hist' didn't close opened files: after
 reaching OS-limit of openend connections, no files could be read any more ->
 added 'fclose(f);' to close open connections after use

 06/01/2012  (DLM) edited _read_hist() function to calculate the yearly avg air
 temperature & the monthly avg air temperatures...

 11/30/2010	(clk) added appopriate lines for 'surfaceRunoff' in
 SW_WTH_new_year() and SW_WTH_new_day();

 06/21/2013	(DLM)	variable 'tail' got too large by 1 and leaked memory
 when accessing array runavg_list[] in function _runavg_temp(): changed test
 from '(tail < SW_Weather.days_in_runavg)' to '(tail <
 (SW_Weather.days_in_runavg-1))' in function clear_hist_weather() temp_max was
 tested twice, one should be temp_min

 06/24/2013	(rjm) added function void SW_WTH_clear_runavg_list(void) to free
 memory

 06/24/2013	(rjm) 	made 'tail' and 'firsttime' a module-level static
 variable (instead of function-level): otherwise it will not get reset to 0
 between consecutive calls as a dynamic library need to set these variables to 0
 resp TRUE in function SW_WTH_construct()

 06/27/2013	(drs)	closed open files if LogError() with LOGERROR is called
 in SW_WTH_read(), _read_hist()

 08/26/2013 (rjm) removed extern SW_OUTPUT never used.
 */
/********************************************************/
/********************************************************/


/* =================================================== */
/*                INCLUDES / DEFINES                   */
/* --------------------------------------------------- */
#include "include/SW_Weather.h"      // for SW_WTH_co...
#include "include/filefuncs.h"       // for LogError, CloseFile, GetALine
#include "include/generic.h"         // for Bool, mean, LOGERROR, swFALSE
#include "include/myMemory.h"        // for Mem_Malloc, Mem_Calloc
#include "include/SW_datastructs.h"  // for SW_WEATHER_HIST, LOG_INFO, SW_W...
#include "include/SW_Defines.h"      // for missing, SW_MISSING, TimeInt
#include "include/SW_Files.h"        // for eWeather
#include "include/SW_Flow_lib_PET.h" // for actualVaporPressure1, actualVap...
#include "include/SW_Markov.h"       // for SW_MKV_today
#include "include/SW_SoilWater.h"    // for SW_SWC_adjust_snow
#include "include/Times.h"           // for Time_get_lastdoy_y, Time_days_i...
#include <math.h>                    // for exp, fmin, fmax
#include <stdio.h>                   // for NULL, sscanf, FILE, fclose, fopen
#include <stdlib.h>                  // for free
#include <string.h>                  // for memset, NULL


/* Weather generation methods */
/** Markov weather generator method, see generateMissingWeather() */
static const unsigned int wgMKV = 2;

/** Weather generation method LOCF, see generateMissingWeather() */
static const unsigned int wgLOCF = 1;

/* =================================================== */
/*             Local Function Definitions              */
/* --------------------------------------------------- */

/**
@brief Takes averages through the number of years of the calculated values from
calc_SiteClimate

@param[in] climateOutput Structure of type SW_CLIMATE_YEARLY that holds all
    output from `calcSiteClimate()` like monthly/yearly temperature and
    precipitation values
@param[in] numYears Number of years represented within simulation
@param[out] climateAverages Structure of type SW_CLIMATE_CLIM that holds
averages and standard deviations output by `averageClimateAcrossYears()`
*/
void averageClimateAcrossYears(
    SW_CLIMATE_YEARLY *climateOutput,
    unsigned int numYears,
    SW_CLIMATE_CLIM *climateAverages
) {

    unsigned int month;

    // Take long-term average of monthly mean, maximum and minimum temperature
    // and precipitation throughout "numYears"
    for (month = 0; month < MAX_MONTHS; month++) {
        climateAverages->meanTempMon_C[month] =
            mean(climateOutput->meanTempMon_C[month], numYears);
        climateAverages->maxTempMon_C[month] =
            mean(climateOutput->maxTempMon_C[month], numYears);
        climateAverages->minTempMon_C[month] =
            mean(climateOutput->minTempMon_C[month], numYears);
        climateAverages->PPTMon_cm[month] =
            mean(climateOutput->PPTMon_cm[month], numYears);
    }

    // Take the long-term average of yearly precipitation and temperature,
    // C4 and Cheatgrass variables
    climateAverages->PPT_cm = mean(climateOutput->PPT_cm, numYears);
    climateAverages->meanTemp_C = mean(climateOutput->meanTemp_C, numYears);
    climateAverages->PPT7thMon_mm = mean(climateOutput->PPT7thMon_mm, numYears);
    climateAverages->meanTempDriestQtr_C =
        mean(climateOutput->meanTempDriestQtr_C, numYears);
    climateAverages->minTemp2ndMon_C =
        mean(climateOutput->minTemp2ndMon_C, numYears);
    climateAverages->ddAbove65F_degday =
        mean(climateOutput->ddAbove65F_degday, numYears);
    climateAverages->frostFree_days =
        mean(climateOutput->frostFree_days, numYears);
    climateAverages->minTemp7thMon_C =
        mean(climateOutput->minTemp7thMon_C, numYears);

    // Calculate and set standard deviation of C4 variables
    climateAverages->sdC4[0] =
        standardDeviation(climateOutput->minTemp7thMon_C, numYears);
    climateAverages->sdC4[1] =
        standardDeviation(climateOutput->frostFree_days, numYears);
    climateAverages->sdC4[2] =
        standardDeviation(climateOutput->ddAbove65F_degday, numYears);

    // Calculate and set the standard deviation of cheatgrass variables
    climateAverages->sdCheatgrass[0] =
        standardDeviation(climateOutput->PPT7thMon_mm, numYears);
    climateAverages->sdCheatgrass[1] =
        standardDeviation(climateOutput->meanTempDriestQtr_C, numYears);
    climateAverages->sdCheatgrass[2] =
        standardDeviation(climateOutput->minTemp2ndMon_C, numYears);
}

/**
@brief Calculate monthly and annual time series of climate variables from daily
weather while adjusting as needed depending on if the site in question is in
the northern or southern hemisphere.

This function has two different types of years:

- The first one being "calendar" years. Which refers to the days and months
  that show up on a calendar (Jan-Dec).

- The second type being "adjusted" years. This type is used when the site
  location is in the southern hemisphere and the year starts in July instead of
  January.
  For example, the adjusted year 1980 starts half a year earlier on
  July 1st 1979, and ends on June 30th, 1980 (see example below).
  If our data starts in 1980, then we don't have values for the first
  six months of adjusted year, 1980.
  Therefore, we start calculating the climate variables in the following
  adjusted year, 1981. And both the first and last six months of data are not
  used.

Calendar year vs. adjusted year:
| Calendar year | Year North | Start N | End N | Year South | Start S | End S |
|:-------------:|:----------:|:-------:|:-----:|:----------:|:-------:|:-----:|
| 1980 | 1980 | 1980 Jan 1 | 1980 Dec 31 | 1980 | 1979 July 1 | 1980 June 30 |
| 1981 | 1981 | 1981 Jan 1 | 1981 Dec 31 | 1981 | 1980 July 1 | 1981 June 30 |
| 1982 | 1982 | 1982 Jan 1 | 1982 Dec 31 | 1982 | 1981 July 1 | 1982 June 30 |
| ...  | ...  | ...        | ...         | ...  | ...         | ...          |
| 2020 | 2020 | 2020 Jan 1 | 2020 Dec 31 | 2020 | 2020 July 1 | 2021 June 30 |

@param[in] allHist Array containing all historical data of a site
@param[in] cum_monthdays Monthly cumulative number of days for "current" year
@param[in] days_in_month Number of days per month for "current" year
@param[in] numYears Number of years represented within simulation
@param[in] startYear Calendar year corresponding to first year of `allHist`
@param[in] inNorthHem Boolean value specifying if site is in northern hemisphere
@param[out] climateOutput Structure of type SW_CLIMATE_YEARLY that holds all
    output from `calcSiteClimate()` like monthly/yearly temperature and
    precipitation values
*/

void calcSiteClimate(
    SW_WEATHER_HIST *allHist,
    TimeInt cum_monthdays[],
    TimeInt days_in_month[],
    unsigned int numYears,
    unsigned int startYear,
    Bool inNorthHem,
    SW_CLIMATE_YEARLY *climateOutput
) {

    unsigned int month;
    unsigned int yearIndex;
    unsigned int year;
    unsigned int day;
    unsigned int numDaysYear;
    unsigned int currMonDay;
    unsigned int numDaysMonth;
    unsigned int adjustedDoy;
    unsigned int adjustedYear = 0;
    unsigned int secondMonth;
    unsigned int seventhMonth;
    unsigned int adjustedStartYear;
    unsigned int calendarYearDays;

    double currentTempMin;
    double currentTempMean;
    double totalAbove65;
    double current7thMonMin;
    double PPT7thMon;
    double consecNonFrost;
    double currentNonFrost;

    size_t size_nyrs = sizeof(double) * numYears;

    // Initialize accumulated value arrays to all zeros
    for (month = 0; month < MAX_MONTHS; month++) {
        memset(climateOutput->meanTempMon_C[month], 0, size_nyrs);
        memset(climateOutput->maxTempMon_C[month], 0, size_nyrs);
        memset(climateOutput->minTempMon_C[month], 0, size_nyrs);
        memset(climateOutput->PPTMon_cm[month], 0, size_nyrs);
    }
    memset(climateOutput->PPT_cm, 0, size_nyrs);
    memset(climateOutput->meanTemp_C, 0, size_nyrs);
    memset(climateOutput->minTemp2ndMon_C, 0, size_nyrs);
    memset(climateOutput->minTemp7thMon_C, 0, size_nyrs);

    calcSiteClimateLatInvariants(
        allHist,
        cum_monthdays,
        days_in_month,
        numYears,
        startYear,
        climateOutput
    );

    // Set starting conditions that are dependent on north/south before main
    // loop is entered
    if (inNorthHem) {
        secondMonth = Feb;
        seventhMonth = Jul;
        numDaysMonth = Time_days_in_month(Jan, days_in_month);
        adjustedStartYear = 0;
    } else {
        secondMonth = Aug;
        seventhMonth = Jan;
        numDaysMonth = Time_days_in_month(Jul, days_in_month);
        adjustedStartYear = 1;
        climateOutput->minTemp7thMon_C[0] = SW_MISSING;
        climateOutput->PPT7thMon_mm[0] = SW_MISSING;
        climateOutput->ddAbove65F_degday[0] = SW_MISSING;
        climateOutput->frostFree_days[0] = SW_MISSING;
    }

    // Loop through all years of data starting at "adjustedStartYear"
    for (yearIndex = adjustedStartYear; yearIndex < numYears; yearIndex++) {
        year = yearIndex + startYear;
        Time_new_year(year, days_in_month, cum_monthdays);
        numDaysYear = Time_get_lastdoy_y(year);
        month = (inNorthHem) ? Jan : Jul;
        currMonDay = 0;
        current7thMonMin = SW_MISSING;
        totalAbove65 = 0;
        currentNonFrost = 0;
        consecNonFrost = 0;
        PPT7thMon = 0;

        if (!inNorthHem) {
            // Get calendar year days only when site is in southern hemisphere
            // To deal with number of days in data
            calendarYearDays = Time_get_lastdoy_y(year - 1);
        }

        for (day = 0; day < numDaysYear; day++) {
            if (inNorthHem) {
                adjustedDoy = day;
                adjustedYear = yearIndex;
            } else {
                // Adjust year and day to meet southern hemisphere requirements
                adjustedDoy = (calendarYearDays == 366) ? day + 182 : day + 181;
                adjustedDoy = adjustedDoy % calendarYearDays;

                if (adjustedDoy == 0) {
                    adjustedYear++;
                    Time_new_year(year, days_in_month, cum_monthdays);
                }
            }

            if (month == Jul && adjustedYear >= numYears - 1 && !inNorthHem) {
                // Set all current accumulated values to SW_MISSING to prevent
                // a zero going into the last year of the respective arrays
                // Last six months of data is ignored and do not go into
                // a new year of data for "adjusted years"
                PPT7thMon = SW_MISSING;
                totalAbove65 = SW_MISSING;
                currentNonFrost = SW_MISSING;
                consecNonFrost = SW_MISSING;
                break;
            }

            currMonDay++;
            currentTempMin = allHist[adjustedYear].temp_min[adjustedDoy];
            currentTempMean = allHist[adjustedYear].temp_avg[adjustedDoy];

            // Part of code that deals with gathering seventh month information
            if (month == seventhMonth) {
                current7thMonMin = (currentTempMin < current7thMonMin) ?
                                       currentTempMin :
                                       current7thMonMin;
                PPT7thMon += allHist[adjustedYear].ppt[adjustedDoy] * 10;
            }

            // Part of code dealing with consecutive amount of days without
            // frost
            if (currentTempMin > 0.0) {
                currentNonFrost += 1.;
            } else if (currentNonFrost > consecNonFrost) {
                consecNonFrost = currentNonFrost;
                currentNonFrost = 0.;
            } else {
                currentNonFrost = 0.;
            }

            // Gather minimum temperature of second month of year
            if (month == secondMonth) {
                climateOutput->minTemp2ndMon_C[yearIndex] +=
                    allHist[adjustedYear].temp_min[adjustedDoy];
            }

            // Once we have reached the end of the month days,
            // handle it by getting information about the next month
            if (currMonDay == numDaysMonth) {
                if (month == secondMonth) {
                    climateOutput->minTemp2ndMon_C[yearIndex] /= numDaysMonth;
                }

                month = (month + 1) % 12;
                numDaysMonth = Time_days_in_month(month, days_in_month);
                currMonDay = 0;
            }

            // Accumulate information of degrees above 65ºF
            // Check if "currentTempMean" is SW_MISSING
            // When "calendarYearDays" is the number of days in a leap year
            // (e.g. adjustedYear = 1985, calendarYearDays = 366 for previous
            // year) It gets to data that is SW_MISSING, so we want to ignore
            // that
            if (!missing(currentTempMean)) {
                // Get degrees in Fahrenheit
                currentTempMean -= 18.333;

                // Add to total above 65ºF if high enough temperature
                totalAbove65 += (currentTempMean > 0.0) ? currentTempMean : 0.0;
            }
        }

        // Set all values
        climateOutput->minTemp7thMon_C[yearIndex] = current7thMonMin;
        climateOutput->PPT7thMon_mm[yearIndex] = PPT7thMon;
        climateOutput->ddAbove65F_degday[yearIndex] = totalAbove65;

        // The reason behind checking if consecNonFrost is greater than zero,
        // is that there is a chance all days in the year are above 32F
        climateOutput->frostFree_days[yearIndex] =
            (consecNonFrost > 0) ? consecNonFrost : currentNonFrost;
    }

    findDriestQtr(
        numYears,
        inNorthHem,
        climateOutput->meanTempDriestQtr_C,
        climateOutput->meanTempMon_C,
        climateOutput->PPTMon_cm
    );
}

/**
@brief Helper function to `calcSiteClimate()`. Manages all information that is
independant of the site being in the northern/southern hemisphere.

@param[in] allHist Array containing all historical data of a site
@param[in] cum_monthdays Monthly cumulative number of days for "current" year
@param[in] days_in_month Number of days per month for "current" year
@param[in] numYears Number of years represented within simulation
@param[in] startYear Calendar year corresponding to first year of `allHist`
@param[out] climateOutput Structure of type SW_CLIMATE_YEARLY that holds all
    output from `calcSiteClimate()` like monthly/yearly temperature and
    precipitation values
*/
void calcSiteClimateLatInvariants(
    SW_WEATHER_HIST *allHist,
    TimeInt cum_monthdays[],
    TimeInt days_in_month[],
    unsigned int numYears,
    unsigned int startYear,
    SW_CLIMATE_YEARLY *climateOutput
) {

    unsigned int month = Jan;
    unsigned int numDaysMonth = Time_days_in_month(month, days_in_month);
    unsigned int yearIndex;
    unsigned int day;
    unsigned int numDaysYear;
    unsigned int currMonDay;
    unsigned int year;

    for (yearIndex = 0; yearIndex < numYears; yearIndex++) {
        year = yearIndex + startYear;
        Time_new_year(year, days_in_month, cum_monthdays);
        numDaysYear = Time_get_lastdoy_y(year);
        currMonDay = 0;

        for (day = 0; day < numDaysYear; day++) {
            currMonDay++;
            climateOutput->meanTempMon_C[month][yearIndex] +=
                allHist[yearIndex].temp_avg[day];
            climateOutput->maxTempMon_C[month][yearIndex] +=
                allHist[yearIndex].temp_max[day];
            climateOutput->minTempMon_C[month][yearIndex] +=
                allHist[yearIndex].temp_min[day];
            climateOutput->PPTMon_cm[month][yearIndex] +=
                allHist[yearIndex].ppt[day];
            climateOutput->PPT_cm[yearIndex] += allHist[yearIndex].ppt[day];
            climateOutput->meanTemp_C[yearIndex] +=
                allHist[yearIndex].temp_avg[day];

            if (currMonDay == numDaysMonth) {
                climateOutput->meanTempMon_C[month][yearIndex] /= numDaysMonth;
                climateOutput->maxTempMon_C[month][yearIndex] /= numDaysMonth;
                climateOutput->minTempMon_C[month][yearIndex] /= numDaysMonth;

                month = (month + 1) % MAX_MONTHS;
                numDaysMonth = Time_days_in_month(month, days_in_month);
                currMonDay = 0;
            }
        }
        climateOutput->meanTemp_C[yearIndex] /= numDaysYear;
    }
}

/**
@brief Helper function to `calcSiteClimate()` to find the average temperature
during the driest quarter of the year

@param[in] numYears Number of years represented within simulation
@param[in] inNorthHem Boolean value specifying if site is in northern
    hemisphere
@param[out] meanTempDriestQtr_C Array of size numYears holding the average
    temperature of the driest quarter of the year for every year
@param[out] meanTempMon_C 2D array containing monthly means average daily air
    temperature (deg;C) with dimensions of row (months) size MAX_MONTHS and
    columns (years) of size numYears
@param[out] PPTMon_cm 2D array containing monthly amount precipitation (cm)
    with dimensions of row (months) size MAX_MONTHS and columns (years) of size
    numYears
*/
void findDriestQtr(
    unsigned int numYears,
    Bool inNorthHem,
    double *meanTempDriestQtr_C,
    double **meanTempMon_C,
    double **PPTMon_cm
) {

    unsigned int yearIndex;
    unsigned int month;
    unsigned int prevMonth;
    unsigned int nextMonth;
    unsigned int adjustedMonth = 0;
    unsigned int numQuarterMonths = 3;
    unsigned int endNumYears = (inNorthHem) ? numYears : numYears - 1;

    // NOTE: These variables are the same throughout the program if site is in
    // northern hempisphere
    // The main purpose of these are to easily control the correct year when
    // dealing with adjusted years in the southern hempisphere
    unsigned int adjustedYearZero = 0;
    unsigned int adjustedYearOne = 0;
    unsigned int adjustedYearTwo = 0;

    double driestThreeMonPPT;
    double driestMeanTemp;
    double currentQtrPPT;
    double currentQtrTemp;

    for (yearIndex = 0; yearIndex < endNumYears; yearIndex++) {
        driestThreeMonPPT = SW_MISSING;
        driestMeanTemp = SW_MISSING;

        adjustedYearZero = adjustedYearOne = adjustedYearTwo = yearIndex;

        for (month = 0; month < MAX_MONTHS; month++) {

            if (inNorthHem) {
                adjustedMonth = month;

                prevMonth = (adjustedMonth == Jan) ? Dec : adjustedMonth - 1;
                nextMonth = (adjustedMonth == Dec) ? Jan : adjustedMonth + 1;
            } else {
                driestQtrSouthAdjMonYears(
                    month,
                    &adjustedYearZero,
                    &adjustedYearOne,
                    &adjustedYearTwo,
                    &adjustedMonth,
                    &prevMonth,
                    &nextMonth
                );
            }

            // Get precipitation of current quarter
            currentQtrPPT = (PPTMon_cm[prevMonth][adjustedYearZero]) +
                            (PPTMon_cm[adjustedMonth][adjustedYearOne]) +
                            (PPTMon_cm[nextMonth][adjustedYearTwo]);

            // Get temperature of current quarter
            currentQtrTemp = (meanTempMon_C[prevMonth][adjustedYearZero]) +
                             (meanTempMon_C[adjustedMonth][adjustedYearOne]) +
                             (meanTempMon_C[nextMonth][adjustedYearTwo]);

            // Check if the current quarter precipitation is a new low
            if (currentQtrPPT < driestThreeMonPPT) {
                // Make current temperature/precipitation the new lowest
                driestMeanTemp = currentQtrTemp;
                driestThreeMonPPT = currentQtrPPT;
            }
        }

        meanTempDriestQtr_C[yearIndex] = driestMeanTemp / numQuarterMonths;
    }

    if (!inNorthHem) {
        meanTempDriestQtr_C[numYears - 1] = SW_MISSING;
    }
}

/**
@brief Helper function to `findDriestQtr()` to find adjusted months and years
when the current site is in the southern hempisphere.

When site is in the southern hemisphere, the year starts in July and ends in
June of the next calendar year, so months and years need to be adjusted to wrap
around years to get accurate driest quarters in a year. See `calcSiteClimate()`
documentation for more explanation on adjusted months and adjusted/calendar
years.

@param[in] month Current month of the year [January, December]
@param[in,out] adjustedYearZero First adjusted year that is paired with
    previous month of year
@param[in,out] adjustedYearOne Second adjusted year that is paired with current
    month of year
@param[in,out] adjustedYearTwo Third adjusted year that is paired with next
    month of year
@param[in,out] adjustedMonth Adjusted month starting at July going to June of
    next calendar year
@param[in,out] prevMonth Month preceding current input month
@param[in,out] nextMonth Month following current input month
*/
void driestQtrSouthAdjMonYears(
    unsigned int month,
    unsigned int *adjustedYearZero,
    unsigned int *adjustedYearOne,
    unsigned int *adjustedYearTwo,
    unsigned int *adjustedMonth,
    unsigned int *prevMonth,
    unsigned int *nextMonth
) {
    *adjustedMonth = month + Jul;
    *adjustedMonth %= MAX_MONTHS;

    // Adjust prevMonth, nextMonth and adjustedYear(s) to the respective
    // adjustedMonth
    switch (*adjustedMonth) {
    case Jan:
        adjustedYearOne++;
        adjustedYearTwo++;

        *prevMonth = Dec;
        *nextMonth = Feb;
        break;
    case Dec:
        adjustedYearTwo++;

        *prevMonth = Nov;
        *nextMonth = Jan;
        break;
    case Jun:
        adjustedYearTwo--;

        *prevMonth = May;
        *nextMonth = Jul;
        break;
    case Jul:
        adjustedYearZero--;

        *prevMonth = Jun;
        *nextMonth = Aug;
        break;
    default:
        // Do adjusted months normally
        *prevMonth = *adjustedMonth - 1;
        *nextMonth = *adjustedMonth + 1;
        break;
    }

    /* coerce to void to silence `-Wunused-but-set-parameter` [clang-15] */
    (void) adjustedYearZero;
    (void) adjustedYearOne;
    (void) adjustedYearTwo;
}

/* =================================================== */
/*             Global Function Definitions             */
/* --------------------------------------------------- */

/**
@brief Interpolate monthly climate values into daily values
(if the user selected the option(s) to)

@param[out] yearWeather A list (MAX_DAYS or 366) of daily weather
for a certain year
@param[in] year Current year the variable `yearWeather` holds
values for
@param[in] use_cloudCoverMonthly User-specified flag to interpolate
monthly cloud values into daily
@param[in] use_humidityMonthly User-specified flag to interpolate
monthly humidity values into daily
@param[in] use_windSpeedMonthly User-specified flag to interpolate
monthly wind speed values into daily
@param[in] cum_monthdays Monthly cumulative number of days for "current" year
@param[in,out] days_in_month Number of days per month for "current" year
@param[in] cloudcov Array of size #MAX_MONTHS holding monthly cloud cover
values to be interpolated
@param[in] windspeed Array of size #MAX_MONTHS holding monthly wind speed
values to be interpolated
@param[in] r_humidity Array of size #MAX_MONTHS holding monthly relative
humidity values to be interpolated
*/
void SW_WTH_setWeathUsingClimate(
    SW_WEATHER_HIST *yearWeather,
    unsigned int year,
    Bool use_cloudCoverMonthly,
    Bool use_humidityMonthly,
    Bool use_windSpeedMonthly,
    TimeInt cum_monthdays[],
    TimeInt days_in_month[],
    double *cloudcov,
    double *windspeed,
    double *r_humidity
) {
    /* Interpolation is to be in base0 in `interpolate_monthlyValues()` */
    Bool interpAsBase1 = swFALSE;

    // Update yearly day/month information needed when interpolating
    // cloud cover, wind speed, and relative humidity if necessary
    Time_new_year(year, days_in_month, cum_monthdays);

    if (use_cloudCoverMonthly) {
        interpolate_monthlyValues(
            cloudcov,
            interpAsBase1,
            cum_monthdays,
            days_in_month,
            yearWeather->cloudcov_daily
        );
    }

    if (use_humidityMonthly) {
        interpolate_monthlyValues(
            r_humidity,
            interpAsBase1,
            cum_monthdays,
            days_in_month,
            yearWeather->r_humidity_daily
        );
    }

    if (use_windSpeedMonthly) {
        interpolate_monthlyValues(
            windspeed,
            interpAsBase1,
            cum_monthdays,
            days_in_month,
            yearWeather->windspeed_daily
        );
    }
}

/**
@brief Takes all of the input weather values throughout
`n_years` number of years and calculates any known variables to
SW_WEATHER_HIST that was input with multiple parts

@param[in] startYear Start year of the simulation
@param[in] nYears Number of years within the simulation
@param[in] inputFlags A list of flags specifying which input variables
have been input
@param[in] tempWeather A list of all read-in variable values to
transfer/calculate to SW_WEATHER_HIST for the simulation
@param elevation Site elevation above sea level [m];
    utilized only if specific humidity is provided as input
    for calculating relative humidity
@param[out] yearlyWeather Destination for temporary/calculated values
for all years within the simulation
@param[out] LogInfo Holds information on warnings and errors
*/
void SW_WTH_setWeatherValues(
    TimeInt startYear,
    TimeInt nYears,
    const Bool *inputFlags,
    double ***tempWeather,
    double elevation,
    SW_WEATHER_HIST *yearlyWeather,
    LOG_INFO *LogInfo
) {
    TimeInt year;
    TimeInt yearIndex;
    TimeInt doy;

    Bool hasMaxMinTemp = (Bool) (inputFlags[TEMP_MAX] && inputFlags[TEMP_MIN]);
    Bool hasMaxMinRelHumid =
        (Bool) (inputFlags[REL_HUMID_MAX] && inputFlags[REL_HUMID_MIN]);
    Bool hasEastNorthWind =
        (Bool) (inputFlags[WIND_EAST] && inputFlags[WIND_NORTH]);

    // Calculate if daily input values of humidity are to be used instead of
    // being interpolated from monthly values
    Bool useHumidityDaily =
        (Bool) (hasMaxMinRelHumid || inputFlags[REL_HUMID] ||
                inputFlags[SPEC_HUMID] || inputFlags[ACTUAL_VP]);

    if (useHumidityDaily && !hasMaxMinRelHumid && !inputFlags[REL_HUMID] &&
        inputFlags[SPEC_HUMID] && missing(elevation)) {
        LogError(
            LogInfo,
            LOGERROR,
            "Elevation is missing but required to calculate relative humidity "
            "from specific humidity."
        );
        return; // Exit function prematurely due to error
    }

    for (yearIndex = 0; yearIndex < nYears; yearIndex++) {
        year = yearIndex + startYear;

        for (doy = 0; doy < MAX_DAYS; doy++) {
            // Temperature [C]
            yearlyWeather[yearIndex].temp_max[doy] =
                tempWeather[yearIndex][TEMP_MAX][doy];
            yearlyWeather[yearIndex].temp_min[doy] =
                tempWeather[yearIndex][TEMP_MIN][doy];

            // Precipitation [cm]
            yearlyWeather[yearIndex].ppt[doy] =
                tempWeather[yearIndex][PPT][doy];

            // Calculate average air temperature [C] if min/max not missing
            if (!missing(yearlyWeather[yearIndex].temp_max[doy]) &&
                !missing(yearlyWeather[yearIndex].temp_min[doy])) {

                yearlyWeather[yearIndex].temp_avg[doy] =
                    (yearlyWeather[yearIndex].temp_max[doy] +
                     yearlyWeather[yearIndex].temp_min[doy]) /
                    2.0;
            }

            if (inputFlags[CLOUD_COV]) {
                // Cloud cover [0-100 %]
                yearlyWeather[yearIndex].cloudcov_daily[doy] =
                    tempWeather[yearIndex][CLOUD_COV][doy];
            }

            if (inputFlags[WIND_SPEED]) {
                // Wind speed [m s-1]
                yearlyWeather[yearIndex].windspeed_daily[doy] =
                    tempWeather[yearIndex][WIND_SPEED][doy];

            } else if (hasEastNorthWind) {

                // Make sure wind is not averaged calculated with any instances
                // of SW_MISSING
                if (!missing(tempWeather[yearIndex][WIND_EAST][doy]) &&
                    !missing(tempWeather[yearIndex][WIND_NORTH][doy])) {

                    // Wind speed [m s-1]
                    yearlyWeather[yearIndex].windspeed_daily[doy] = sqrt(
                        squared(tempWeather[yearIndex][WIND_EAST][doy]) +
                        squared(tempWeather[yearIndex][WIND_NORTH][doy])
                    );
                } else {
                    yearlyWeather[yearIndex].windspeed_daily[doy] = SW_MISSING;
                }
            }

            // Check to see if daily humidity values are being used
            if (useHumidityDaily) {
                if (hasMaxMinRelHumid) {

                    // Make sure relative humidity is not averaged from any
                    // instances of SW_MISSING
                    if (!missing(tempWeather[yearIndex][REL_HUMID_MAX][doy]) &&
                        !missing(tempWeather[yearIndex][REL_HUMID_MIN][doy])) {

                        // Relative humidity [0-100 %]
                        yearlyWeather[yearIndex].r_humidity_daily[doy] =
                            (tempWeather[yearIndex][REL_HUMID_MAX][doy] +
                             tempWeather[yearIndex][REL_HUMID_MIN][doy]) /
                            2;
                    }

                } else if (inputFlags[REL_HUMID]) {
                    // Relative humidity [0-100 %]
                    yearlyWeather[yearIndex].r_humidity_daily[doy] =
                        tempWeather[yearIndex][REL_HUMID][doy];

                } else if (inputFlags[SPEC_HUMID]) {

                    // Make sure the calculation of relative humidity will not
                    // be executed while average temperature and/or specific
                    // humidity are holding the value "SW_MISSING"
                    if (!missing(yearlyWeather[yearIndex].temp_avg[doy]) &&
                        !missing(tempWeather[yearIndex][SPEC_HUMID][doy])) {

                        // Relative humidity [0-100 %] calculated from
                        // specific humidity [g kg-1] and temperature [C]
                        yearlyWeather[yearIndex].r_humidity_daily[doy] =
                            relativeHumidity2(
                                tempWeather[yearIndex][SPEC_HUMID][doy],
                                yearlyWeather[yearIndex].temp_avg[doy],
                                elevation
                            );

                        // Snap relative humidity in 100-150% to 100%
                        if (yearlyWeather[yearIndex].r_humidity_daily[doy] >
                                100. &&
                            yearlyWeather[yearIndex].r_humidity_daily[doy] <=
                                150.) {
                            LogError(
                                LogInfo,
                                LOGWARN,
                                "Year %d - day %d: relative humidity set to "
                                "100%%: "
                                "based on assumption that "
                                "a presumed minor mismatch in inputs "
                                "(specific humidity (%f), "
                                "temperature (%f) and elevation (%f)) "
                                "caused the calculated value (%f) to exceed "
                                "100%%.",
                                year,
                                doy,
                                tempWeather[yearIndex][SPEC_HUMID][doy],
                                yearlyWeather[yearIndex].temp_avg[doy],
                                elevation,
                                yearlyWeather[yearIndex].r_humidity_daily[doy]
                            );

                            yearlyWeather[yearIndex].r_humidity_daily[doy] =
                                100.;
                        }

                    } else {
                        // Set relative humidity to "SW_MISSING"
                        yearlyWeather[yearIndex].r_humidity_daily[doy] =
                            SW_MISSING;
                    }
                }

                // Deal with actual vapor pressure
                if (inputFlags[ACTUAL_VP]) {

                    // Actual vapor pressure [kPa]
                    yearlyWeather[yearIndex].actualVaporPressure[doy] =
                        tempWeather[yearIndex][ACTUAL_VP][doy];

                } else if (inputFlags[TEMP_DEWPOINT] &&
                           !missing(tempWeather[yearIndex][TEMP_DEWPOINT][doy]
                           )) {

                    // Actual vapor pressure [kPa] from dewpoint temperature [C]
                    yearlyWeather[yearIndex].actualVaporPressure[doy] =
                        actualVaporPressure3(
                            tempWeather[yearIndex][TEMP_DEWPOINT][doy]
                        );

                } else if (hasMaxMinTemp && hasMaxMinRelHumid) {

                    // Make sure the calculation of actual vapor pressure will
                    // not be executed while max and/or min temperature and/or
                    // relative humidity are holding the value "SW_MISSING"
                    if (!missing(yearlyWeather[yearIndex].temp_max[doy]) &&
                        !missing(yearlyWeather[yearIndex].temp_min[doy]) &&
                        !missing(tempWeather[yearIndex][REL_HUMID_MAX][doy]) &&
                        !missing(tempWeather[yearIndex][REL_HUMID_MIN][doy])) {

                        // Actual vapor pressure [kPa]
                        yearlyWeather[yearIndex].actualVaporPressure[doy] =
                            actualVaporPressure2(
                                tempWeather[yearIndex][REL_HUMID_MAX][doy],
                                tempWeather[yearIndex][REL_HUMID_MIN][doy],
                                yearlyWeather[yearIndex].temp_max[doy],
                                yearlyWeather[yearIndex].temp_min[doy]
                            );
                    } else {
                        // Set actual vapor pressure to "SW_MISSING"
                        yearlyWeather[yearIndex].actualVaporPressure[doy] =
                            SW_MISSING;
                    }

                } else if (inputFlags[REL_HUMID] || inputFlags[SPEC_HUMID]) {
                    // Make sure the daily values for relative humidity and
                    // average temperature are not SW_MISSING
                    if (!missing(yearlyWeather[yearIndex].r_humidity_daily[doy]
                        ) &&
                        !missing(yearlyWeather[yearIndex].temp_avg[doy])) {

                        // Actual vapor pressure [kPa]
                        yearlyWeather[yearIndex].actualVaporPressure[doy] =
                            actualVaporPressure1(
                                yearlyWeather[yearIndex].r_humidity_daily[doy],
                                yearlyWeather[yearIndex].temp_avg[doy]
                            );
                    } else {
                        yearlyWeather[yearIndex].actualVaporPressure[doy] =
                            SW_MISSING;
                    }
                }

                // Check if a calculation of relative humidity is available
                // using dewpoint temperature or actual vapor pressure, but only
                // if the daily value of relative humidity is "SW_MISSING"
                if (missing(yearlyWeather[yearIndex].r_humidity_daily[doy]) &&
                    (inputFlags[ACTUAL_VP] || inputFlags[TEMP_DEWPOINT])) {

                    // Make sure the calculation of relative humidity will not
                    // be executed while average temperature and/or actual vapor
                    // pressure hold the value "SW_MISSING"
                    if (!missing(yearlyWeather[yearIndex].temp_avg[doy]) &&
                        !missing(
                            yearlyWeather[yearIndex].actualVaporPressure[doy]
                        )) {

                        // Relative humidity [0-100 %]
                        yearlyWeather[yearIndex]
                            .r_humidity_daily[doy] = relativeHumidity1(
                            yearlyWeather[yearIndex].actualVaporPressure[doy],
                            yearlyWeather[yearIndex].temp_avg[doy]
                        );

                        // Snap relative humidity in 100-150% to 100%
                        if (yearlyWeather[yearIndex].r_humidity_daily[doy] >
                                100. &&
                            yearlyWeather[yearIndex].r_humidity_daily[doy] <=
                                150.) {
                            LogError(
                                LogInfo,
                                LOGWARN,
                                "Year %d - day %d: relative humidity set to "
                                "100%%: "
                                "based on assumption that "
                                "a presumed minor mismatch in inputs "
                                "(vapor pressure (%f) and temperature (%f)) "
                                "caused the calculated value (%f) to exceed "
                                "100%%.",
                                year,
                                doy,
                                yearlyWeather[yearIndex]
                                    .actualVaporPressure[doy],
                                yearlyWeather[yearIndex].temp_avg[doy],
                                yearlyWeather[yearIndex].r_humidity_daily[doy]
                            );

                            yearlyWeather[yearIndex].r_humidity_daily[doy] =
                                100.;
                        }
                    }
                }
            }

            if (inputFlags[SHORT_WR]) {
                yearlyWeather[yearIndex].shortWaveRad[doy] =
                    tempWeather[yearIndex][SHORT_WR][doy];
            }
        }
    }
}

/**
@brief Allocate temporary locations for the entirety of the
simulations weather history

@param[in] nYears Number of years within the simulation
@param[out] fullWeathHist A list of values to temporarily store
the weather history for every possible input variable
@param[out] LogInfo Holds information on warnings and errors
*/
void allocate_temp_weather(
    TimeInt nYears, double ****fullWeathHist, LOG_INFO *LogInfo
) {
    TimeInt year;
    int tempVar;

    *fullWeathHist = (double ***) Mem_Malloc(
        sizeof(double **) * nYears, "allocate_temp_weather()", LogInfo
    );
    if (LogInfo->stopRun) {
        return;
    }

    for (year = 0; year < nYears; year++) {
        (*fullWeathHist)[year] = NULL;
    }

    for (year = 0; year < nYears; year++) {
        (*fullWeathHist)[year] = (double **) Mem_Malloc(
            sizeof(double *) * MAX_INPUT_COLUMNS,
            "allocate_temp_weather()",
            LogInfo
        );
        if (LogInfo->stopRun) {
            return;
        }
        for (tempVar = 0; tempVar < MAX_INPUT_COLUMNS; tempVar++) {
            (*fullWeathHist)[year][tempVar] = NULL;
        }
    }


    for (year = 0; year < nYears; year++) {
        for (tempVar = 0; tempVar < MAX_INPUT_COLUMNS; tempVar++) {
            (*fullWeathHist)[year][tempVar] = (double *) Mem_Malloc(
                sizeof(double) * MAX_DAYS, "allocate_temp_weather()", LogInfo
            );
        }
    }
}

/**
@brief Deallocate temporary locations for the entirety of the
simulations weather history

@param[in] nYears Number of years within the simulation
@param[out] fullWeathHist A list of values to temporarily store
the weather history for every possible input variable
*/
void deallocate_temp_weather(TimeInt nYears, double ****fullWeathHist) {
    TimeInt year;
    int tempVar;

    if (!isnull(*fullWeathHist)) {
        for (year = 0; year < nYears; year++) {
            if (!isnull((*fullWeathHist)[year])) {
                for (tempVar = 0; tempVar < MAX_INPUT_COLUMNS; tempVar++) {
                    if (!isnull((*fullWeathHist)[year][tempVar])) {
                        free((void *) (*fullWeathHist)[year][tempVar]);
                        (*fullWeathHist)[year][tempVar] = NULL;
                    }
                }
                free((void *) (*fullWeathHist)[year]);
                (*fullWeathHist)[year] = NULL;
            }
        }
        free((void *) *fullWeathHist);
        *fullWeathHist = NULL;
    }
}

/**
@brief Reads in all weather data

Reads in weather data from disk (if available) for all years and
stores values in global SW_Weather's SW_WEATHER::allHist.
If missing, set values to #SW_MISSING.

@param[out] allHist 1D array holding all weather data gathered
@param[in] startYear Start year of the simulation
@param[in] n_years Number of years in simulation
@param[in] use_weathergenerator_only A boolean; if #swFALSE, code attempts to
    read weather files from disk.
@param[in] txtWeatherPrefix File name of weather data without extension.
@param[in] use_cloudCoverMonthly A boolean; if #swTRUE, function will
    interpolate mean monthly values provided by \p cloudcov to daily time series
@param[in] use_humidityMonthly A boolean; if #swTRUE, function will interpolate
    mean monthly values provided by \p r_humidity to daily time series
@param[in] use_windSpeedMonthly A boolean; if #swTRUE, function will
    interpolate mean monthly values provided by \p windspeed to daily time
series
@param[in] n_input_forcings Number of read-in columns from disk
@param[in] dailyInputIndices An array of size #MAX_INPUT_COLUMNS holding the
    calculated column number of which a certain variable resides
@param[in] dailyInputFlags An array of size #MAX_INPUT_COLUMNS holding booleans
    specifying what variable has daily input on disk
@param[in] cloudcov Array of size #MAX_MONTHS holding monthly cloud cover
    values to be interpolated
@param[in] windspeed Array of size #MAX_MONTHS holding monthly wind speed
    values to be interpolated
@param[in] r_humidity Array of size #MAX_MONTHS holding monthly relative
    humidity values to be interpolated
@param[in] elevation Site elevation above sea level [m];
    utilized only if specific humidity is provided as input for
    calculating relative humidity
@param[in] cum_monthdays Monthly cumulative number of days for "current" year
@param[in] days_in_month Number of days per month for "current" year
@param[out] LogInfo Holds information on warnings and errors
*/
void readAllWeather(
    SW_WEATHER_HIST *allHist,
    unsigned int startYear,
    unsigned int n_years,
    Bool use_weathergenerator_only,
    char txtWeatherPrefix[],
    Bool use_cloudCoverMonthly,
    Bool use_humidityMonthly,
    Bool use_windSpeedMonthly,
    unsigned int n_input_forcings,
    unsigned int *dailyInputIndices,
    Bool *dailyInputFlags,
    double *cloudcov,
    double *windspeed,
    double *r_humidity,
    double elevation,
    TimeInt cum_monthdays[],
    TimeInt days_in_month[],
    LOG_INFO *LogInfo
) {
    unsigned int yearIndex;
    unsigned int year;
    double ***tempWeatherHist = NULL;

    allocate_temp_weather(n_years, &tempWeatherHist, LogInfo);
    if (LogInfo->stopRun) {
        return;
    }

    for (yearIndex = 0; yearIndex < n_years; yearIndex++) {
        year = yearIndex + startYear;

        // Set all daily weather values to missing
        clear_hist_weather(&allHist[yearIndex], tempWeatherHist[yearIndex]);

        SW_WTH_setWeathUsingClimate(
            &allHist[yearIndex],
            year,
            use_cloudCoverMonthly,
            use_humidityMonthly,
            use_windSpeedMonthly,
            cum_monthdays,
            days_in_month,
            cloudcov,
            windspeed,
            r_humidity
        );

        // Read daily weather values from disk
        if (!use_weathergenerator_only) {
            read_weather_hist(
                year,
                tempWeatherHist[yearIndex],
                txtWeatherPrefix,
                n_input_forcings,
                dailyInputIndices,
                dailyInputFlags,
                LogInfo
            );
            if (LogInfo->stopRun) {
                goto freeTempWeather; // Exit function prematurely due to error
            }
        }
    }

    if (!use_weathergenerator_only) {
        SW_WTH_setWeatherValues(
            startYear,
            n_years,
            dailyInputFlags,
            tempWeatherHist,
            elevation,
            allHist,
            LogInfo
        );
    }

freeTempWeather:
    deallocate_temp_weather(n_years, &tempWeatherHist);
}

/**
@brief Impute missing values and scale with monthly parameters

@param[in,out] SW_MarkovIn Struct of type SW_MARKOV_INPUTS which holds values
    related to temperature and weather generator
@param[in,out] w Struct of type SW_WEATHER_INPUTS holding all relevant
    information pretaining to meteorological input data
@param[in] cum_monthdays Monthly cumulative number of days for "current" year
@param[in] days_in_month Number of days per month for "current" year
@param[out] LogInfo Holds information on warnings and errors

Finalize weather values after they have been read in via
`readAllWeather()` or `SW_WTH_read()`
(the latter also handles (re-)allocation).
*/
void finalizeAllWeather(
    SW_MARKOV_INPUTS *SW_MarkovIn,
    SW_WEATHER_INPUTS *w,
    TimeInt cum_monthdays[],
    TimeInt days_in_month[],
    LOG_INFO *LogInfo
) {

    unsigned int day;
    unsigned int yearIndex;

    // Impute missing values
    generateMissingWeather(
        SW_MarkovIn,
        w->allHist,
        w->startYear,
        w->n_years,
        w->generateWeatherMethod,
        3, // optLOCF_nMax (TODO: make this user input)
        LogInfo
    );
    if (LogInfo->stopRun) {
        return; // Prematurely exit function
    }

    // Check to see if actual vapor pressure needs to be calculated
    if (w->use_humidityMonthly) {
        for (yearIndex = 0; yearIndex < w->n_years; yearIndex++) {
            for (day = 0; day < MAX_DAYS; day++) {

                // Make sure calculation of actual vapor pressure is not
                // polluted by values of `SW_MISSING`
                if (!missing(w->allHist[yearIndex].r_humidity_daily[day]) &&
                    !missing(w->allHist[yearIndex].temp_avg[day])) {

                    w->allHist[yearIndex].actualVaporPressure[day] =
                        actualVaporPressure1(
                            w->allHist[yearIndex].r_humidity_daily[day],
                            w->allHist[yearIndex].temp_avg[day]
                        );
                }
            }
        }
    }


    // Scale with monthly additive/multiplicative parameters
    scaleAllWeather(
        w->allHist,
        w->startYear,
        w->n_years,
        w->scale_temp_max,
        w->scale_temp_min,
        w->scale_precip,
        w->scale_skyCover,
        w->scale_wind,
        w->scale_rH,
        w->scale_actVapPress,
        w->scale_shortWaveRad,
        cum_monthdays,
        days_in_month
    );

    // Make sure all input, scaled, generated, and calculated daily weather
    // values are within reason
    checkAllWeather(w, LogInfo);
}

void SW_WTH_finalize_all_weather(
    SW_MARKOV_INPUTS *SW_MarkovIn,
    SW_WEATHER_INPUTS *SW_WeatherIn,
    TimeInt cum_monthdays[],
    TimeInt days_in_month[],
    LOG_INFO *LogInfo
) {

    finalizeAllWeather(
        SW_MarkovIn, SW_WeatherIn, cum_monthdays, days_in_month, LogInfo
    );
}

/**
@brief Apply temperature, precipitation, cloud cover, relative humidity, and
wind speed scaling to daily weather values

@param[in,out] allHist 1D array holding all weather data
@param[in] startYear Start year of the simulation (and `allHist`)
@param[in] n_years Number of years in simulation (length of `allHist`)
@param[in] scale_temp_max Array of monthly, additive scaling parameters to
    modify daily maximum air temperature [C]
@param[in] scale_temp_min Array of monthly, additive scaling parameters to
    modify daily minimum air temperature [C]
@param[in] scale_precip Array of monthly, multiplicative scaling parameters to
    modify daily precipitation [-]
@param[in] scale_skyCover Array of monthly, additive scaling parameters to
    modify daily sky cover [%]
@param[in] scale_wind Array of monthly, multiplicitive scaling parameters to
    modify daily wind speed [-]
@param[in] scale_rH Array of monthly, additive scaling parameters to modify
    daily relative humidity [%]
@param[in] scale_actVapPress Array of monthly, multiplicitive scaling
    parameters to modify daily actual vapor pressure [-]
@param[in] scale_shortWaveRad Array of monthly, multiplicitive scaling
    parameters to modify daily shortwave radiation [%]
@param[in] cum_monthdays Monthly cumulative number of days for "current" year
@param[in] days_in_month Number of days per month for "current" year

@note Daily average air temperature is re-calculated after scaling
minimum and maximum air temperature.

@note Missing values in `allHist` remain unchanged.
*/
void scaleAllWeather(
    SW_WEATHER_HIST *allHist,
    unsigned int startYear,
    unsigned int n_years,
    double *scale_temp_max,
    double *scale_temp_min,
    double *scale_precip,
    double *scale_skyCover,
    const double *scale_wind,
    double *scale_rH,
    const double *scale_actVapPress,
    const double *scale_shortWaveRad,
    TimeInt cum_monthdays[],
    TimeInt days_in_month[]
) {

    unsigned int year;
    unsigned int month;
    unsigned int yearIndex;
    unsigned int numDaysYear;
    unsigned int day;

    Bool trivial = swTRUE;

    // Check if we have any non-trivial scaling parameter
    for (month = 0; trivial && month < MAX_MONTHS; month++) {
        trivial =
            (Bool) (ZRO(scale_temp_max[month]) && ZRO(scale_temp_min[month]) &&
                    EQ(scale_precip[month], 1.) && ZRO(scale_skyCover[month]) &&
                    EQ(scale_wind[month], 1.) && ZRO(scale_rH[month]) &&
                    EQ(scale_actVapPress[month], 1.) &&
                    EQ(scale_shortWaveRad[month], 1.));
    }

    if (!trivial) {
        // Apply scaling parameters to each day of `allHist`
        for (yearIndex = 0; yearIndex < n_years; yearIndex++) {
            year = yearIndex + startYear;
            Time_new_year(
                year, days_in_month, cum_monthdays
            ); // required for `doy2month()`
            numDaysYear = Time_get_lastdoy_y(year);

            for (day = 0; day < numDaysYear; day++) {
                month = doy2month(day + 1, cum_monthdays);

                if (!missing(allHist[yearIndex].temp_max[day])) {
                    allHist[yearIndex].temp_max[day] += scale_temp_max[month];
                }

                if (!missing(allHist[yearIndex].temp_min[day])) {
                    allHist[yearIndex].temp_min[day] += scale_temp_min[month];
                }

                if (!missing(allHist[yearIndex].ppt[day])) {
                    allHist[yearIndex].ppt[day] *= scale_precip[month];
                }

                if (!missing(allHist[yearIndex].cloudcov_daily[day])) {
                    allHist[yearIndex].cloudcov_daily[day] = fmin(
                        100.,
                        fmax(
                            0.0,
                            scale_skyCover[month] +
                                allHist[yearIndex].cloudcov_daily[day]
                        )
                    );
                }

                if (!missing(allHist[yearIndex].windspeed_daily[day])) {
                    allHist[yearIndex].windspeed_daily[day] = fmax(
                        0.0,
                        scale_wind[month] *
                            allHist[yearIndex].windspeed_daily[day]
                    );
                }

                if (!missing(allHist[yearIndex].r_humidity_daily[day])) {
                    allHist[yearIndex].r_humidity_daily[day] = fmin(
                        100.,
                        fmax(
                            0.0,
                            scale_rH[month] +
                                allHist[yearIndex].r_humidity_daily[day]
                        )
                    );
                }

                if (!missing(allHist[yearIndex].actualVaporPressure[day])) {
                    allHist[yearIndex].actualVaporPressure[day] = fmax(
                        0.0,
                        scale_actVapPress[month] *
                            allHist[yearIndex].actualVaporPressure[day]
                    );
                }

                if (!missing(allHist[yearIndex].shortWaveRad[day])) {
                    allHist[yearIndex].shortWaveRad[day] = fmax(
                        0.0,
                        scale_shortWaveRad[month] *
                            allHist[yearIndex].shortWaveRad[day]
                    );
                }

                /* re-calculate average air temperature */
                if (!missing(allHist[yearIndex].temp_max[day]) &&
                    !missing(allHist[yearIndex].temp_min[day])) {
                    allHist[yearIndex].temp_avg[day] =
                        (allHist[yearIndex].temp_max[day] +
                         allHist[yearIndex].temp_min[day]) /
                        2.;
                }
            }
        }
    }
}

/**
@brief Generate missing weather

Meteorological inputs are required for each day; they can either be
observed and provided via weather input files or they can be generated
such as by a weather generator (which has separate input requirements).

SOILWAT2 handles three scenarios of missing data:
    1. Some individual days are missing (values correspond to #SW_MISSING);
       if any relevant variable is missing on a day, then all relevant
       variables are imputed
    2. An entire year is missing (file `weath.xxxx` for year `xxxx` is absent)
    3. No daily weather input files are available

Available methods to generate weather:
    1. Pass through (`method` = 0)
    2. Imputation by last-value-carried forward "LOCF" (`method` = 1)
        - affected variables (all implemented):
            - minimum and maximum temperature
            - precipitation (which is set to 0 instead of "LOCF")
            - cloud cover
            - wind speed
            - relative humidity
            - downard surface shortwave radiation
            - actual vapor pressure
        - missing values are imputed individually
        - error if more than `optLOCF_nMax` days per calendar year are missing
    3. First-order Markov weather generator (`method` = 2)
        - affected variables (others are passed through as is):
            - minimum and maximum temperature
            - precipitation
        - if a day contains any missing values (of affected variables), then
          values for all of these variables are replaced by values created by
          the weather generator

The user can specify that SOILWAT2 generates all weather without reading
any historical weather data files from disk
(see `weathsetup.in`: use weather generator for all weather).

@note `SW_MKV_today()` is called if `method` = 2
(i.e., the weather generator is used);
this requires that appropriate structures are initialized.

@param[in,out] allHist 1D array holding all weather data
@param[in,out] SW_MarkovIn Struct of type SW_MARKOV_INPUTS which holds values
    related to temperature and weather generator
@param[in] startYear Start year of the simulation
@param[in] n_years Number of years in simulation
@param[in] method Number to identify which method to apply to generate
    missing values (see details).
@param[in] optLOCF_nMax Maximum number of missing days per year (e.g., 5)
    before imputation by `LOCF` throws an error.
@param[out] LogInfo Holds information on warnings and errors
*/
void generateMissingWeather(
    SW_MARKOV_INPUTS *SW_MarkovIn,
    SW_WEATHER_HIST *allHist,
    unsigned int startYear,
    unsigned int n_years,
    unsigned int method,
    unsigned int optLOCF_nMax,
    LOG_INFO *LogInfo
) {

    unsigned int year;
    unsigned int yearIndex;
    unsigned int numDaysYear;
    unsigned int day;
    unsigned int nFilledLOCF;

    double yesterdayPPT = SW_MISSING;
    double yesterdayTempMin = SW_MISSING;
    double yesterdayTempMax = SW_MISSING;
    double yesterdayCloudCov = SW_MISSING;
    double yesterdayWindSpeed = SW_MISSING;
    double yesterdayRelHum = SW_MISSING;
    double yesterdayShortWR = SW_MISSING;
    double yesterdayActVP = SW_MISSING;

    Bool any_missing;
    Bool missing_Tmax = swFALSE;
    Bool missing_Tmin = swFALSE;
    Bool missing_PPT = swFALSE;
    Bool missing_CloudCov = swFALSE;
    Bool missing_WindSpeed = swFALSE;
    Bool missing_RelHum = swFALSE;
    Bool missing_ActVP = swFALSE;
    Bool missing_ShortWR = swFALSE;


    // Pass through method: return early
    if (method == 0) {
        return;
    }


    // Error out if method not implemented
    if (method > 2) {
        LogError(
            LogInfo,
            LOGERROR,
            "generateMissingWeather(): method = %u is not implemented.\n",
            method
        );
        return; // Exit function prematurely due to error
    }


    // Loop over years
    for (yearIndex = 0; yearIndex < n_years; yearIndex++) {
        year = yearIndex + startYear;
        numDaysYear = Time_get_lastdoy_y(year);
        nFilledLOCF = 0;

        for (day = 0; day < numDaysYear; day++) {
            /* Determine variables with missing values */

            missing_Tmax = (Bool) missing(allHist[yearIndex].temp_max[day]);
            missing_Tmin = (Bool) missing(allHist[yearIndex].temp_min[day]);
            missing_PPT = (Bool) missing(allHist[yearIndex].ppt[day]);

            if (method == wgLOCF) {
                missing_CloudCov =
                    (Bool) missing(allHist[yearIndex].cloudcov_daily[day]);
                missing_WindSpeed =
                    (Bool) missing(allHist[yearIndex].windspeed_daily[day]);
                missing_RelHum =
                    (Bool) missing(allHist[yearIndex].r_humidity_daily[day]);
                missing_ShortWR =
                    (Bool) missing(allHist[yearIndex].shortWaveRad[day]);
                missing_ActVP =
                    (Bool) missing(allHist[yearIndex].actualVaporPressure[day]);
            }

            any_missing =
                (Bool) (missing_Tmax || missing_Tmin || missing_PPT ||
                        missing_CloudCov || missing_WindSpeed ||
                        missing_RelHum || missing_ShortWR || missing_ActVP);

            if (any_missing) {
                // some of today's values are missing

                if (method == wgMKV) {
                    // Markov weather generator (Tmax, Tmin, and PPT)
                    allHist[yearIndex].ppt[day] = yesterdayPPT;
                    SW_MKV_today(
                        SW_MarkovIn,
                        day,
                        year,
                        &allHist[yearIndex].temp_max[day],
                        &allHist[yearIndex].temp_min[day],
                        &allHist[yearIndex].ppt[day],
                        LogInfo
                    );
                    if (LogInfo->stopRun) {
                        return; // Exit function prematurely due to error
                    }

                } else if (method == wgLOCF) {
                    // LOCF (temp, cloud cover, wind speed, relative humidity,
                    // shortwave radiation, and actual vapor pressure) + 0 (PPT)
                    allHist[yearIndex].temp_max[day] =
                        missing_Tmax ? yesterdayTempMax :
                                       allHist[yearIndex].temp_max[day];

                    allHist[yearIndex].temp_min[day] =
                        missing_Tmin ? yesterdayTempMin :
                                       allHist[yearIndex].temp_min[day];

                    allHist[yearIndex].cloudcov_daily[day] =
                        missing_CloudCov ?
                            yesterdayCloudCov :
                            allHist[yearIndex].cloudcov_daily[day];

                    allHist[yearIndex].windspeed_daily[day] =
                        missing_WindSpeed ?
                            yesterdayWindSpeed :
                            allHist[yearIndex].windspeed_daily[day];

                    allHist[yearIndex].r_humidity_daily[day] =
                        missing_RelHum ?
                            yesterdayRelHum :
                            allHist[yearIndex].r_humidity_daily[day];

                    allHist[yearIndex].shortWaveRad[day] =
                        missing_ShortWR ? yesterdayShortWR :
                                          allHist[yearIndex].shortWaveRad[day];

                    allHist[yearIndex].actualVaporPressure[day] =
                        missing_ActVP ?
                            yesterdayActVP :
                            allHist[yearIndex].actualVaporPressure[day];

                    allHist[yearIndex].ppt[day] =
                        missing_PPT ? 0. : allHist[yearIndex].ppt[day];


                    // Throw an error if too many missing values have
                    // been replaced with non-missing values by the LOCF method
                    // per calendar year
                    if ((missing_Tmax && !missing(yesterdayTempMax)) ||
                        (missing_Tmin && !missing(yesterdayTempMin)) ||
                        (missing_PPT && !missing(allHist[yearIndex].ppt[day])
                        ) ||
                        (missing_CloudCov && !missing(yesterdayCloudCov)) ||
                        (missing_WindSpeed && !missing(yesterdayWindSpeed)) ||
                        (missing_RelHum && !missing(yesterdayRelHum)) ||
                        (missing_ShortWR && !missing(yesterdayShortWR)) ||
                        (missing_ActVP && !missing(yesterdayActVP))) {
                        nFilledLOCF++;
                    }

                    if (nFilledLOCF > optLOCF_nMax) {
                        LogError(
                            LogInfo,
                            LOGERROR,
                            "generateMissingWeather(): more than %u days "
                            "missing in year %u "
                            "and weather generator turned off.\n",
                            optLOCF_nMax,
                            year
                        );
                        return; // Exit function prematurely due to error
                    }
                }


                // Re-calculate average air temperature
                allHist[yearIndex].temp_avg[day] =
                    (allHist[yearIndex].temp_max[day] +
                     allHist[yearIndex].temp_min[day]) /
                    2.;
            }

            yesterdayPPT = allHist[yearIndex].ppt[day];

            if (method == wgLOCF) {
                yesterdayTempMax = allHist[yearIndex].temp_max[day];
                yesterdayTempMin = allHist[yearIndex].temp_min[day];
                yesterdayCloudCov = allHist[yearIndex].cloudcov_daily[day];
                yesterdayWindSpeed = allHist[yearIndex].windspeed_daily[day];
                yesterdayRelHum = allHist[yearIndex].r_humidity_daily[day];
                yesterdayShortWR = allHist[yearIndex].shortWaveRad[day];
                yesterdayActVP = allHist[yearIndex].actualVaporPressure[day];
            }
        }
    }
}

/**
@brief Check weather through all years/days within simultation and make sure
all input values are reasonable after possible weather generation and scaling.
If a value is to be found unreasonable, the function will execute a program
crash.

@param[in] weather Struct of type SW_WEATHER_INPUTS holding all relevant
information pretaining to weather input data
@param[out] LogInfo Holds information on warnings and errors
*/
void checkAllWeather(SW_WEATHER_INPUTS *weather, LOG_INFO *LogInfo) {

    // Initialize any variables
    TimeInt year;
    TimeInt doy;
    TimeInt numDaysInYear;
    SW_WEATHER_HIST *weathHist = weather->allHist;

    double dailyMinTemp;
    double dailyMaxTemp;

    // Loop through `allHist` years
    for (year = 0; year < weather->n_years; year++) {
        numDaysInYear = Time_get_lastdoy_y(year + weather->startYear);

        // Loop through `allHist` days
        for (doy = 0; doy < numDaysInYear; doy++) {

            dailyMaxTemp = weathHist[year].temp_max[doy];
            dailyMinTemp = weathHist[year].temp_min[doy];

            if (!missing(dailyMaxTemp) && !missing(dailyMinTemp) &&
                dailyMinTemp > dailyMaxTemp) {
                // Check if minimum temp greater than maximum temp

                // Fail
                LogError(
                    LogInfo,
                    LOGERROR,
                    "Daily input value for minimum temperature"
                    " is greater than daily input value for maximum "
                    "temperature (minimum = %f, maximum = %f)"
                    " on day %d of year %d.",
                    dailyMinTemp,
                    dailyMaxTemp,
                    doy + 1,
                    year + weather->startYear
                );
                // Will exit function prematurely due to error

            } else if ((!missing(dailyMaxTemp) && !missing(dailyMinTemp)) &&
                       ((dailyMinTemp > 100. || dailyMinTemp < -100.) ||
                        (dailyMaxTemp > 100. || dailyMaxTemp < -100.))) {
                // Otherwise, check if maximum or minimum temp, or
                // dew point temp is not [-100, 100]

                // Fail
                LogError(
                    LogInfo,
                    LOGERROR,
                    "Daily minimum and/or maximum temperature on "
                    "day %d of year %d do not fit in the range of [-100, 100] "
                    "C.",
                    doy,
                    year + weather->startYear
                );
                // Will exit function prematurely due to error

            } else if (!missing(weathHist[year].ppt[doy]) &&
                       weathHist[year].ppt[doy] < 0) {
                // Otherwise, check if precipitation is less than 0cm

                // Fail
                LogError(
                    LogInfo,
                    LOGERROR,
                    "Invalid daily precipitation value: %f cm (< 0) on day %d "
                    "of year %d.",
                    weathHist[year].ppt[doy],
                    doy + 1,
                    year + weather->startYear
                );
                // Will exit function prematurely due to error

            } else if (!missing(weathHist[year].r_humidity_daily[doy]) &&
                       (weathHist[year].r_humidity_daily[doy] <= 1. ||
                        weathHist[year].r_humidity_daily[doy] > 100.)) {
                // Otherwise, check if relative humidity is less than 0% or
                // greater than 100%

                if ((weathHist[year].r_humidity_daily[doy] >= 0.) &&
                    (weathHist[year].r_humidity_daily[doy] <= 1.)) {
                    LogError(
                        LogInfo,
                        LOGWARN,
                        "Daily/calculated relative humidity value (%f) is "
                        "within [0, 1] indicating a possibly incorrect unit "
                        "(expectation: value within [0, 100] %%).",
                        weathHist[year].r_humidity_daily[doy]
                    );
                } else {
                    LogError(
                        LogInfo,
                        LOGERROR,
                        "Invalid daily/calculated relative humidity value did"
                        " not fall in the range [0, 100] (relative humidity = "
                        "%f). ",
                        weathHist[year].r_humidity_daily[doy]
                    );
                    return; // Exit function prematurely due to error
                }

            } else if (!missing(weathHist[year].cloudcov_daily[doy]) &&
                       (weathHist[year].cloudcov_daily[doy] < 0. ||
                        weathHist[year].cloudcov_daily[doy] > 100.)) {
                // Otherwise, check if cloud cover was input and
                // if the value is less than 0% or greater than 100%

                // Fail
                LogError(
                    LogInfo,
                    LOGERROR,
                    "Invalid daily/calculated cloud cover value did"
                    " not fall in the range [0, 100] (cloud cover = %f). ",
                    weathHist[year].cloudcov_daily[doy]
                );
                // Will exit function prematurely due to error

            } else if (!missing(weathHist[year].windspeed_daily[doy]) &&
                       weathHist[year].windspeed_daily[doy] < 0.) {
                // Otherwise, check if wind speed is less than 0 m/s

                // Fail
                LogError(
                    LogInfo,
                    LOGERROR,
                    "Invalid daily wind speed value is less than zero."
                    "(wind speed = %f) on day %d of year %d. ",
                    weathHist[year].windspeed_daily[doy],
                    doy + 1,
                    year + weather->startYear
                );
                // Will exit function prematurely due to error

            } else if (!missing(weathHist[year].shortWaveRad[doy]) &&
                       weathHist[year].shortWaveRad[doy] < 0.) {
                // Otherwise, check if radiation if less than 0 W/m^2

                // Fail
                LogError(
                    LogInfo,
                    LOGERROR,
                    "Invalid daily shortwave radiation value is less than zero."
                    "(shortwave radation = %f) on day %d of year %d. ",
                    weathHist[year].shortWaveRad[doy],
                    doy + 1,
                    year + weather->startYear
                );
                // Will exit function prematurely due to error

            } else if (!missing(weathHist[year].actualVaporPressure[doy]) &&
                       weathHist[year].actualVaporPressure[doy] < 0.) {
                // Otherwise, check if actual vapor pressure is less than 0 kPa

                // Fail
                LogError(
                    LogInfo,
                    LOGERROR,
                    "Invalid daily actual vapor pressure value is less than "
                    "zero."
                    "(actual vapor pressure = %f) on day %d of year %d. ",
                    weathHist[year].actualVaporPressure[doy],
                    doy + 1,
                    year + weather->startYear
                );
                // Will exit function prematurely due to error
            }

            if (LogInfo->stopRun) {
                return; // Exit function prematurely due to error
            }
        }
    }
}

/**
@brief Clears weather history.
@note Used by rSOILWAT2
*/
void clear_hist_weather(SW_WEATHER_HIST *yearWeather, double **fullWeathHist) {
    /* --------------------------------------------------- */
    TimeInt d;
    int weathVar;

    for (d = 0; d < MAX_DAYS; d++) {
        if (!isnull(yearWeather)) {
            yearWeather->ppt[d] = SW_MISSING;
            yearWeather->temp_max[d] = SW_MISSING;
            yearWeather->temp_min[d] = SW_MISSING;
            yearWeather->temp_avg[d] = SW_MISSING;
            yearWeather->cloudcov_daily[d] = SW_MISSING;
            yearWeather->windspeed_daily[d] = SW_MISSING;
            yearWeather->r_humidity_daily[d] = SW_MISSING;
            yearWeather->shortWaveRad[d] = SW_MISSING;
            yearWeather->actualVaporPressure[d] = SW_MISSING;
        }

        if (!isnull(fullWeathHist)) {
            for (weathVar = 0; weathVar < MAX_INPUT_COLUMNS; weathVar++) {
                fullWeathHist[weathVar][d] = SW_MISSING;
            }
        }
    }
}

/* =================================================== */
/* =================================================== */
/*             Global Function Definitions             */
/* --------------------------------------------------- */

/**
@brief Initialize all possible pointers in SW_WEATHER_INPUTS to NULL

@param[in,out] allHist Array to NULL-out that will contain all
historical data of a site
*/
void SW_WTH_init_ptrs(SW_WEATHER_HIST **allHist) { *allHist = NULL; }

/**
@brief Constructor for SW_Weather.

@param[out] SW_WeatherIn Weather input struct (SW_WEATHER_INPUTS)
@param[out] p_accu Weather output accumulation array
@param[out] p_oagg Weather output aggregator array
@param[out] SW_WeatherSim A struct of type SW_WEATHER_SIM holding
values that are used during simulations
*/
void SW_WTH_construct(
    SW_WEATHER_INPUTS *SW_WeatherIn,
    SW_WEATHER_SIM *SW_WeatherSim,
    SW_WEATHER_OUTPUTS *p_accu,
    SW_WEATHER_OUTPUTS *p_oagg
) {
    /* =================================================== */

    // Clear the module structure:
    memset(SW_WeatherIn, 0, sizeof(SW_WEATHER_INPUTS));
    memset(SW_WeatherSim, 0, sizeof(SW_WEATHER_SIM));
    memset(p_accu, 0, sizeof(SW_WEATHER_OUTPUTS));
    memset(p_oagg, 0, sizeof(SW_WEATHER_OUTPUTS));

    SW_WeatherIn->n_years = 0;
}

/**
@brief Deconstructor for SW_Weather and SW_Markov (if used)

@param[out] allHist Array to allocate that will contain all
historical data of a site
*/
void SW_WTH_deconstruct(SW_WEATHER_HIST **allHist) {
    deallocateAllWeather(allHist);
}

/**
@brief Allocate memory for `allHist` based on `n_years`

@param[out] allHist Array containing all historical data of a site
@param[in] n_years Number of years in simulation
@param[out] LogInfo Holds information on warnings and errors
*/
void SW_WTH_allocateAllWeather(
    SW_WEATHER_HIST **allHist, unsigned int n_years, LOG_INFO *LogInfo
) {
    *allHist = (SW_WEATHER_HIST *) Mem_Malloc(
        sizeof(SW_WEATHER_HIST) * n_years,
        "SW_WTH_allocateAllWeather()",
        LogInfo
    );
    if (LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }
}

/**
@brief Helper function to SW_WTH_deconstruct to deallocate `allHist` of `w`.

@param[in,out] allHist Array containing all historical data of a site
*/
void deallocateAllWeather(SW_WEATHER_HIST **allHist) {
    if (!isnull(*allHist)) {
        free((void *) *allHist);
        *allHist = NULL;
    }
}

/**
@brief Initialize weather variables for a simulation run

  They are used as default if weather for the first day is missing

@param[out] SW_WeatherSim Struct of type SW_WEATHER_SIM holding all
    meteorological simulation data
*/
void SW_WTH_init_run(SW_WEATHER_SIM *SW_WeatherSim) {
    /* setup today's weather because it's used as a default
     * value when weather for the first day is missing.
     * Notice that temps of 0. are reasonable for January
     * (doy=1) and are below the critical temps for freezing
     * and with ppt=0 there's nothing to freeze.
     */
    SW_WeatherSim->temp_max = SW_WeatherSim->temp_min = 0.;
    SW_WeatherSim->ppt = SW_WeatherSim->rain = 0.;
    SW_WeatherSim->snow = SW_WeatherSim->snowmelt = SW_WeatherSim->snowloss =
        0.;
    SW_WeatherSim->snowRunoff = 0.;
    SW_WeatherSim->surfaceRunoff = SW_WeatherSim->surfaceRunon = 0.;
    SW_WeatherSim->soil_inf = 0.;
}

/**
@brief Guarantees that today's weather will not be invalid via -_todays_weth().

@param[in,out] SW_WeatherIn Struct of type SW_WEATHER_INPUTS holding all
simulation information pretaining to meteorological data
@param[in,out] SW_WeatherSim Struct of type SW_WEATHER_SIM holding all
    meteorological simulation data
@param[in] SW_SiteIn Struct of type SW_SITE describing the simulated site's
    input values
@param[in] snowpack[] swe of snowpack, assuming accumulation is turned on
@param[in] doy Day of the year (base1) [1-366]
@param[in] year Current year being run in the simulation
@param[out] LogInfo Holds information on warnings and errors
*/
void SW_WTH_new_day(
    SW_WEATHER_INPUTS *SW_WeatherIn,
    SW_WEATHER_SIM *SW_WeatherSim,
    SW_SITE_INPUTS *SW_SiteIn,
    double snowpack[],
    TimeInt doy,
    TimeInt year,
    LOG_INFO *LogInfo
) {
    /* =================================================== */
    /*  History
     *
     *  20-Jul-2002 -- added growing season computation to
     *                 facilitate the steppe/soilwat interface.
     *  06-Dec-2002 -- modified the seasonal computation to
     *                 account for n-s hemispheres.
     *	16-Sep-2009 -- (drs) scaling factors were only applied to Tmin and Tmax
     *					but not to Taverage -> corrected
     *	09-Oct-2009	-- (drs) commented out snow adjustement, because call
     *moved to SW_Flow.c 20091015 (drs) ppt is divided into rain and snow
     */

    /* Indices to today's weather record in `allHist` */
    TimeInt doy0 = doy - 1;
    TimeInt doy1 = doy; // Used for call to SW_SWC_adjust_snow()
    TimeInt yearIndex = year - SW_WeatherIn->startYear;

    /*
    #ifdef STEPWAT
             TimeInt doy = SW_Model.doy;
             Bool is_warm;
             Bool is_growingseason = swFALSE;
    #endif
    */

    /* get the daily weather from allHist */

    /* SOILWAT2 simulations requires non-missing values of forcing variables.
       Exceptions
         1. shortwave radiation can be missing if cloud cover is not missing
         2. cloud cover can be missing if shortwave radiation is not missing
    */
    if (missing(SW_WeatherIn->allHist[yearIndex].temp_avg[doy0]) ||
        missing(SW_WeatherIn->allHist[yearIndex].ppt[doy0]) ||
        missing(SW_WeatherIn->allHist[yearIndex].windspeed_daily[doy0]) ||
        missing(SW_WeatherIn->allHist[yearIndex].r_humidity_daily[doy0]) ||
        missing(SW_WeatherIn->allHist[yearIndex].actualVaporPressure[doy0]) ||
        (missing(SW_WeatherIn->allHist[yearIndex].shortWaveRad[doy0]) &&
         missing(SW_WeatherIn->allHist[yearIndex].cloudcov_daily[doy0]))) {
        LogError(
            LogInfo,
            LOGERROR,
            "Missing weather data (day %u - %u) during simulation: "
            "Tavg=%.2f, ppt=%.2f, wind=%.2f, rH=%.2f, vp=%.2f, rsds=%.2f / "
            "cloud=%.2f\n",
            year,
            doy,
            SW_WeatherIn->allHist[yearIndex].temp_avg[doy0],
            SW_WeatherIn->allHist[yearIndex].ppt[doy0],
            SW_WeatherIn->allHist[yearIndex].windspeed_daily[doy0],
            SW_WeatherIn->allHist[yearIndex].r_humidity_daily[doy0],
            SW_WeatherIn->allHist[yearIndex].actualVaporPressure[doy0],
            SW_WeatherIn->allHist[yearIndex].shortWaveRad[doy0],
            SW_WeatherIn->allHist[yearIndex].cloudcov_daily[doy0]
        );
        return; // Prematurely return the function
    }

    SW_WeatherSim->temp_max = SW_WeatherIn->allHist[yearIndex].temp_max[doy0];
    SW_WeatherSim->temp_min = SW_WeatherIn->allHist[yearIndex].temp_min[doy0];
    SW_WeatherSim->ppt = SW_WeatherIn->allHist[yearIndex].ppt[doy0];
    SW_WeatherSim->cloudCover =
        SW_WeatherIn->allHist[yearIndex].cloudcov_daily[doy0];
    SW_WeatherSim->windSpeed =
        SW_WeatherIn->allHist[yearIndex].windspeed_daily[doy0];
    SW_WeatherSim->relHumidity =
        SW_WeatherIn->allHist[yearIndex].r_humidity_daily[doy0];
    SW_WeatherSim->shortWaveRad =
        SW_WeatherIn->allHist[yearIndex].shortWaveRad[doy0];
    SW_WeatherSim->actualVaporPressure =
        SW_WeatherIn->allHist[yearIndex].actualVaporPressure[doy0];

    SW_WeatherSim->temp_avg = SW_WeatherIn->allHist[yearIndex].temp_avg[doy0];

    SW_WeatherSim->snow = SW_WeatherSim->snowmelt = SW_WeatherSim->snowloss =
        0.;
    SW_WeatherSim->snowRunoff = SW_WeatherSim->surfaceRunoff =
        SW_WeatherSim->surfaceRunon = SW_WeatherSim->soil_inf = 0.;

    if (SW_WeatherIn->use_snow) {
        SW_SWC_adjust_snow(
            &SW_WeatherSim->temp_snow,
            snowpack,
            SW_SiteIn,
            SW_WeatherSim->temp_min,
            SW_WeatherSim->temp_max,
            SW_WeatherSim->ppt,
            doy1,
            &SW_WeatherSim->rain,
            &SW_WeatherSim->snow,
            &SW_WeatherSim->snowmelt
        );
    } else {
        SW_WeatherSim->rain = SW_WeatherSim->ppt;
    }
}

/**
@brief Reads in file for SW_Weather.

@param[in,out] SW_WeatherIn Struct of type SW_WEATHER_INPUTS holding all
relevant information pretaining to meteorological input data
@param[in] txtInFiles Array of program in/output files
@param[out] txtWeatherPrefix File name of weather data without extension.
@param[out] LogInfo Holds information on warnings and errors
*/
void SW_WTH_setup(
    SW_WEATHER_INPUTS *SW_WeatherIn,
    char *txtInFiles[],
    char *txtWeatherPrefix,
    LOG_INFO *LogInfo
) {
    /* =================================================== */
    const int nitems = 35;
    FILE *f;
    int lineno = 0;
    int month;
    int x;
    int index;
    int resSNP;
    double sppt;
    double stmax;
    double stmin;
    double sky;
    double wind;
    double rH;
    double actVP;
    double shortWaveRad;
    char inbuf[MAX_FILENAMESIZE];
    int inBufintRes = 0;
    double inBufdoubleRes = 0.;

    char weathInputStrs[9][20] = {{'\0'}};
    double *inDoubleVals[8] = {
        &sppt, &stmax, &stmin, &sky, &wind, &rH, &actVP, &shortWaveRad
    };
    const int numInDefaultVars = 9;

    Bool doIntConv;

    Bool *dailyInputFlags = SW_WeatherIn->dailyInputFlags;

    char *MyFileName = txtInFiles[eWeather];
    f = OpenFile(MyFileName, "r", LogInfo);
    if (LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

    while (GetALine(f, inbuf, MAX_FILENAMESIZE)) {
        doIntConv = (Bool) (lineno <= 22 && (lineno != 1 && lineno != 2));

        if (lineno <= 22) {
            if (doIntConv) {
                inBufintRes = sw_strtoi(inbuf, MyFileName, LogInfo);
            } else {
                inBufdoubleRes = sw_strtod(inbuf, MyFileName, LogInfo);
            }

            if (LogInfo->stopRun) {
                goto closeFile;
            }
        }

        switch (lineno) {
        case 0:
            SW_WeatherIn->use_snow = itob(inBufintRes);
            break;
        case 1:
            SW_WeatherIn->pct_snowdrift = inBufdoubleRes;
            break;
        case 2:
            SW_WeatherIn->pct_snowRunoff = inBufdoubleRes;
            break;

        case 3:
            SW_WeatherIn->use_weathergenerator_only = swFALSE;

            switch (inBufintRes) {
            case 0:
                // As is
                SW_WeatherIn->generateWeatherMethod = 0;
                break;

            case 1:
                // weather generator
                SW_WeatherIn->generateWeatherMethod = wgMKV;
                break;

            case 2:
                // weather generatory only
                SW_WeatherIn->generateWeatherMethod = wgMKV;
                SW_WeatherIn->use_weathergenerator_only = swTRUE;
                break;

            case 3:
                // LOCF (temp) + 0 (PPT)
                SW_WeatherIn->generateWeatherMethod = wgLOCF;
                break;

            default:
                LogError(
                    LogInfo,
                    LOGERROR,
                    "%s : Requested weather generator method '%d' is not "
                    "implemented.",
                    MyFileName,
                    inBufintRes
                );
                goto closeFile;
            }
            break;

        case 4:
            SW_WeatherIn->rng_seed = inBufintRes;
            break;

        case 5:
            SW_WeatherIn->use_cloudCoverMonthly = itob(inBufintRes);
            break;

        case 6:
            SW_WeatherIn->use_windSpeedMonthly = itob(inBufintRes);
            break;

        case 7:
            SW_WeatherIn->use_humidityMonthly = itob(inBufintRes);
            break;

        case 8:
            SW_WeatherIn->dailyInputFlags[TEMP_MAX] = itob(inBufintRes);
            break;

        case 9:
            SW_WeatherIn->dailyInputFlags[TEMP_MIN] = itob(inBufintRes);
            break;

        case 10:
            SW_WeatherIn->dailyInputFlags[PPT] = itob(inBufintRes);
            break;

        case 11:
            SW_WeatherIn->dailyInputFlags[CLOUD_COV] = itob(inBufintRes);
            break;

        case 12:
            SW_WeatherIn->dailyInputFlags[WIND_SPEED] = itob(inBufintRes);
            break;

        case 13:
            SW_WeatherIn->dailyInputFlags[WIND_EAST] = itob(inBufintRes);
            break;

        case 14:
            SW_WeatherIn->dailyInputFlags[WIND_NORTH] = itob(inBufintRes);
            break;

        case 15:
            SW_WeatherIn->dailyInputFlags[REL_HUMID] = itob(inBufintRes);
            break;

        case 16:
            SW_WeatherIn->dailyInputFlags[REL_HUMID_MAX] = itob(inBufintRes);
            break;

        case 17:
            SW_WeatherIn->dailyInputFlags[REL_HUMID_MIN] = itob(inBufintRes);
            break;

        case 18:
            SW_WeatherIn->dailyInputFlags[SPEC_HUMID] = itob(inBufintRes);
            break;

        case 19:
            SW_WeatherIn->dailyInputFlags[TEMP_DEWPOINT] = itob(inBufintRes);
            break;

        case 20:
            SW_WeatherIn->dailyInputFlags[ACTUAL_VP] = itob(inBufintRes);
            break;

        case 21:
            SW_WeatherIn->dailyInputFlags[SHORT_WR] = itob(inBufintRes);
            break;

        case 22:
            SW_WeatherIn->desc_rsds = inBufintRes;
            break;


        default:
            if (lineno == 5 + MAX_MONTHS) {
                break;
            }

            x = sscanf(
                inbuf,
                "%19s %19s %19s %19s %19s %19s %19s %19s %19s",
                weathInputStrs[0],
                weathInputStrs[1],
                weathInputStrs[2],
                weathInputStrs[3],
                weathInputStrs[4],
                weathInputStrs[5],
                weathInputStrs[6],
                weathInputStrs[7],
                weathInputStrs[8]
            );

            if (x != numInDefaultVars) {
                LogError(
                    LogInfo, LOGERROR, "%s : Bad record %d.", MyFileName, lineno
                );
                goto closeFile;
            }

            for (index = 0; index < numInDefaultVars; index++) {
                if (index == 0) {
                    month =
                        sw_strtoi(weathInputStrs[index], MyFileName, LogInfo);
                } else {
                    *(inDoubleVals[index - 1]) =
                        sw_strtod(weathInputStrs[index], MyFileName, LogInfo);
                }

                if (LogInfo->stopRun) {
                    return; // Exit function prematurely due to error
                }
            }

            month--; // convert to base0
            SW_WeatherIn->scale_precip[month] = sppt;
            SW_WeatherIn->scale_temp_max[month] = stmax;
            SW_WeatherIn->scale_temp_min[month] = stmin;
            SW_WeatherIn->scale_skyCover[month] = sky;
            SW_WeatherIn->scale_wind[month] = wind;
            SW_WeatherIn->scale_rH[month] = rH;
            SW_WeatherIn->scale_actVapPress[month] = actVP;
            SW_WeatherIn->scale_shortWaveRad[month] = shortWaveRad;
        }

        lineno++;
    }

    resSNP = snprintf(
        SW_WeatherIn->name_prefix,
        sizeof SW_WeatherIn->name_prefix,
        "%s",
        txtWeatherPrefix
    );
    if (resSNP < 0 || (unsigned) resSNP >= (sizeof SW_WeatherIn->name_prefix)) {
        LogError(
            LogInfo,
            LOGERROR,
            "Weather input path name is too long: '%s'.",
            txtWeatherPrefix
        );
        return; // Exit function prematurely due to error
    }

    if (lineno < nitems) {
        LogError(LogInfo, LOGERROR, "%s : Too few input lines.", MyFileName);
        return; // Exit function prematurely due to error
    }

    // Calculate value indices for `allHist`
    set_dailyInputIndices(
        dailyInputFlags,
        SW_WeatherIn->dailyInputIndices,
        &SW_WeatherIn->n_input_forcings
    );

    check_and_update_dailyInputFlags(
        SW_WeatherIn->use_cloudCoverMonthly,
        SW_WeatherIn->use_humidityMonthly,
        SW_WeatherIn->use_windSpeedMonthly,
        dailyInputFlags,
        LogInfo
    );

closeFile: { CloseFile(&f, LogInfo); }
}

/**
@brief Set and count indices of daily inputs based on user-set flags

@param[in] dailyInputFlags An array of size #MAX_INPUT_COLUMNS
    indicating which daily input variable is active (TRUE).
@param[out] dailyInputIndices An array of size #MAX_INPUT_COLUMNS
    with the calculated column number of all possible daily input variables.
@param[out] n_input_forcings The number of active daily input variables.
*/
void set_dailyInputIndices(
    const Bool dailyInputFlags[MAX_INPUT_COLUMNS],
    unsigned int dailyInputIndices[MAX_INPUT_COLUMNS],
    unsigned int *n_input_forcings
) {
    int currFlag;

    // Default n_input_forcings to 0
    *n_input_forcings = 0;

    // Loop through MAX_INPUT_COLUMNS (# of input flags)
    for (currFlag = 0; currFlag < MAX_INPUT_COLUMNS; currFlag++) {
        // Default the current index to zero
        dailyInputIndices[currFlag] = 0;

        // Check if current flag is set
        if (dailyInputFlags[currFlag]) {

            // Set current index to current number of "n_input_forcings"
            // which is the current number of flags found
            dailyInputIndices[currFlag] = *n_input_forcings;

            // Increment "n_input_forcings"
            (*n_input_forcings)++;
        }
    }
}

/*
  * Turn off necessary flags. This happens after the calculation of
    input indices due to the fact that setting before calculating may
    result in an incorrect `n_input_forcings` in SW_WEATHER, and unexpectedly
    crash the program in `read_weather_hist()`.

  * Check if monthly flags have been chosen to override daily flags.
  * Aside from checking for purely a monthly flag, we must make sure we have
    daily flags to turn off instead of attemping to turn off flags that are
  already off.
*/
void check_and_update_dailyInputFlags(
    Bool use_cloudCoverMonthly,
    Bool use_humidityMonthly,
    Bool use_windSpeedMonthly,
    Bool *dailyInputFlags,
    LOG_INFO *LogInfo
) {
    Bool monthlyFlagPrioritized = swFALSE;

    // Check if temperature max/min flags are unevenly set (1/0) or (0/1)
    if ((dailyInputFlags[TEMP_MAX] && !dailyInputFlags[TEMP_MIN]) ||
        (!dailyInputFlags[TEMP_MAX] && dailyInputFlags[TEMP_MIN])) {

        // Fail
        LogError(
            LogInfo,
            LOGERROR,
            "Maximum/minimum temperature flags are unevenly set. "
            "Both flags for temperature must be set."
        );
        return; // Exit function prematurely due to error
    }

    // Check if minimum and maximum temperature, or precipitation flags are not
    // set
    if ((!dailyInputFlags[TEMP_MAX] && !dailyInputFlags[TEMP_MIN]) ||
        !dailyInputFlags[PPT]) {
        // Fail
        LogError(
            LogInfo,
            LOGERROR,
            "Both maximum/minimum temperature and/or precipitation flag(s) "
            "are not set. All three flags must be set."
        );
        return; // Exit function prematurely due to error
    }

    if (use_windSpeedMonthly &&
        (dailyInputFlags[WIND_SPEED] || dailyInputFlags[WIND_EAST] ||
         dailyInputFlags[WIND_NORTH])) {

        dailyInputFlags[WIND_SPEED] = swFALSE;
        dailyInputFlags[WIND_EAST] = swFALSE;
        dailyInputFlags[WIND_NORTH] = swFALSE;
        monthlyFlagPrioritized = swTRUE;
    }

    if (use_humidityMonthly) {
        if (dailyInputFlags[REL_HUMID] || dailyInputFlags[REL_HUMID_MAX] ||
            dailyInputFlags[REL_HUMID_MIN] || dailyInputFlags[SPEC_HUMID] ||
            dailyInputFlags[ACTUAL_VP]) {

            dailyInputFlags[REL_HUMID] = swFALSE;
            dailyInputFlags[REL_HUMID_MAX] = swFALSE;
            dailyInputFlags[REL_HUMID_MIN] = swFALSE;
            dailyInputFlags[SPEC_HUMID] = swFALSE;
            dailyInputFlags[ACTUAL_VP] = swFALSE;
            monthlyFlagPrioritized = swTRUE;
        }
    }

    if (use_cloudCoverMonthly && dailyInputFlags[CLOUD_COV]) {
        dailyInputFlags[CLOUD_COV] = swFALSE;
        monthlyFlagPrioritized = swTRUE;
    }

    // Check if max/min relative humidity flags are set unevenly (1/0) or (0/1)
    if ((dailyInputFlags[REL_HUMID_MAX] && !dailyInputFlags[REL_HUMID_MIN]) ||
        (!dailyInputFlags[REL_HUMID_MAX] && dailyInputFlags[REL_HUMID_MIN])) {

        // Fail
        LogError(
            LogInfo,
            LOGERROR,
            "Max/min relative humidity flags are unevenly set. "
            "Both flags for maximum/minimum relative humidity must be the "
            "same (1 or 0)."
        );
        return; // Exit function prematurely due to error
    }

    // Check if east/north wind speed flags are set unevenly (1/0) or (0/1)
    if ((dailyInputFlags[WIND_EAST] && !dailyInputFlags[WIND_NORTH]) ||
        (!dailyInputFlags[WIND_EAST] && dailyInputFlags[WIND_NORTH])) {

        // Fail
        LogError(
            LogInfo,
            LOGERROR,
            "East/north wind speed flags are unevenly set. "
            "Both flags for east/north wind speed components "
            "must be the same (1 or 0)."
        );
        return; // Exit function prematurely due to error
    }

    // Check to see if any daily flags were turned off due to a set monthly flag
    if (monthlyFlagPrioritized) {
        // Give the user a generalized note
        LogError(
            LogInfo,
            LOGWARN,
            "One or more daily flags have been turned off due to a set monthly "
            "input flag which overrides daily flags. Please see "
            "`weathsetup.in` "
            "to change this if it was not the desired action."
        );
    }
}

/**
@brief (Re-)allocate `allHist` and read daily meteorological input from disk

The weather generator is not run and daily values are not scaled with
monthly climate parameters, see `SW_WTH_finalize_all_weather()` instead.

@param[in,out] SW_WeatherIn Struct of type SW_WEATHER_INPUTS holding all
relevant information pretaining to meteorological input data
@param[in] SW_SkyIn Struct of type SW_SKY_INPUTS which describes sky conditions
    of the simulated site
@param[in] SW_ModelIn Struct of type SW_MODEL_INPUTS holding basic input
    time information about the simulation
@param[in] readTextInputs Specifies to read text weather inputs, this may
be turned off when dealing with nc inputs
@param[in] cum_monthdays Monthly cumulative number of days for "current" year
@param[in] days_in_month Number of days per month for "current" year
@param[out] LogInfo Holds information on warnings and errors
*/
void SW_WTH_read(
    SW_WEATHER_INPUTS *SW_WeatherIn,
    SW_SKY_INPUTS *SW_SkyIn,
    SW_MODEL_INPUTS *SW_ModelIn,
    Bool readTextInputs,
    TimeInt cum_monthdays[],
    TimeInt days_in_month[],
    LOG_INFO *LogInfo
) {

    // Deallocate (previous, if any) `allHist`
    // (using value of `SW_Weather.n_years` previously used to allocate)
    // `SW_WTH_construct()` sets `n_years` to zero
    deallocateAllWeather(&SW_WeatherIn->allHist);

    // Update number of years and first calendar year represented
    SW_WeatherIn->n_years = SW_ModelIn->endyr - SW_ModelIn->startyr + 1;
    SW_WeatherIn->startYear = SW_ModelIn->startyr;

    if (readTextInputs) {
        // Allocate new `allHist` (based on current `SW_Weather.n_years`)
        SW_WTH_allocateAllWeather(
            &SW_WeatherIn->allHist, SW_WeatherIn->n_years, LogInfo
        );
        if (LogInfo->stopRun) {
            return; // Exit function prematurely due to error
        }

        // Read daily meteorological input from disk (if available)
        readAllWeather(
            SW_WeatherIn->allHist,
            SW_WeatherIn->startYear,
            SW_WeatherIn->n_years,
            SW_WeatherIn->use_weathergenerator_only,
            SW_WeatherIn->name_prefix,
            SW_WeatherIn->use_cloudCoverMonthly,
            SW_WeatherIn->use_humidityMonthly,
            SW_WeatherIn->use_windSpeedMonthly,
            SW_WeatherIn->n_input_forcings,
            SW_WeatherIn->dailyInputIndices,
            SW_WeatherIn->dailyInputFlags,
            SW_SkyIn->cloudcov,
            SW_SkyIn->windspeed,
            SW_SkyIn->r_humidity,
            SW_ModelIn->elevation,
            cum_monthdays,
            days_in_month,
            LogInfo
        );
    }
}

/**
@brief Read the historical (observed) weather file for a simulation year.

The naming convention of the weather input files:
    `[weather-data path/][weather-file prefix].[year]`

Format of a input file (white-space separated values):
    `doy maxtemp(&deg;C) mintemp (&deg;C) precipitation (cm)`

@note Used by rSOILWAT2

@param year Current year within the simulation
@param yearWeather Current year's weather array that is to be filled by
    function
@param txtWeatherPrefix File name of weather data without extension.
@param n_input_forcings Number of read-in columns from disk
@param dailyInputIndices An array of size MAX_INPUT_COLUMNS holding the
    calculated column number of which a certain variable resides
@param dailyInputFlags An array of size MAX_INPUT_COLUMNS holding booleans
    specifying what variable has daily input on disk
@param[out] LogInfo Holds information on warnings and errors
*/
void read_weather_hist(
    TimeInt year,
    double **yearWeather,
    char txtWeatherPrefix[],
    unsigned int n_input_forcings,
    const unsigned int *dailyInputIndices,
    const Bool *dailyInputFlags,
    LOG_INFO *LogInfo
) {
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
    unsigned int x;
    unsigned int lineno = 0;
    unsigned int index;
    int doy = 0;
    int resSNP;
    int varNum;

    double weathInput[MAX_INPUT_COLUMNS];
    char weathInStrs[15][20];

    // Create file name: `[weather-file prefix].[year]`
    char fname[MAX_FILENAMESIZE];
    char inbuf[MAX_FILENAMESIZE];

    resSNP = snprintf(fname, sizeof fname, "%s.%4d", txtWeatherPrefix, year);

    if (resSNP < 0 || (unsigned) resSNP >= (sizeof fname)) {
        LogError(
            LogInfo,
            LOGERROR,
            "Weather input file name is too long for year = %d",
            year
        );
        return; // Exit function prematurely due to error
    }

    f = fopen(fname, "r");
    if (isnull(f)) {
        // no weather input file --> use generator
        return;
    }

    while (GetALine(f, inbuf, MAX_FILENAMESIZE)) {
        lineno++;
        x = sscanf(
            inbuf,
            "%19s %19s %19s %19s %19s %19s %19s %19s %19s %19s "
            "%19s %19s %19s %19s %19s",
            weathInStrs[0],
            weathInStrs[1],
            weathInStrs[2],
            weathInStrs[3],
            weathInStrs[4],
            weathInStrs[5],
            weathInStrs[6],
            weathInStrs[7],
            weathInStrs[8],
            weathInStrs[9],
            weathInStrs[10],
            weathInStrs[11],
            weathInStrs[12],
            weathInStrs[13],
            weathInStrs[14]
        );

        if (x != n_input_forcings + 1) {
            LogError(
                LogInfo,
                LOGERROR,
                "%s : Incomplete record %d (doy=%d).",
                fname,
                lineno,
                doy
            );
            goto closeFile;
        }
        if (x > MAX_INPUT_COLUMNS + 1) {
            LogError(
                LogInfo,
                LOGERROR,
                "%s : Too many values in record %d (doy=%d).",
                fname,
                lineno,
                doy
            );
            goto closeFile;
        }

        for (index = 0; index < n_input_forcings + 1; index++) {
            if (index == 0) {
                doy = sw_strtoi(weathInStrs[index], fname, LogInfo);
            } else {
                weathInput[index - 1] =
                    sw_strtod(weathInStrs[index], fname, LogInfo);
            }

            if (LogInfo->stopRun) {
                goto closeFile;
            }
        }

        if (doy < 1 || doy > MAX_DAYS) {
            LogError(
                LogInfo,
                LOGERROR,
                "%s : Day of year out of range, line %d.",
                fname,
                lineno
            );
            goto closeFile;
        }

        /* --- Make the assignments ---- */
        doy--; // base1 -> base0

        /* Copy the daily values for every variable into the weather
           location; do not do any special calculations */
        for (varNum = 0; varNum < MAX_INPUT_COLUMNS; varNum++) {
            if (dailyInputFlags[varNum]) {
                yearWeather[varNum][doy] =
                    weathInput[dailyInputIndices[varNum]];
            }
        }

        // Calculate annual average temperature based on historical input, i.e.,
        // the `temp_year_avg` calculated here is prospective and unsuitable
        // when the weather generator is used to generate values for the current
        // simulation year, e.g., STEPWAT2
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

closeFile: { CloseFile(&f, LogInfo); }
}

void initializeClimatePtrs(
    SW_CLIMATE_YEARLY *climateOutput, SW_CLIMATE_CLIM *climateAverages
) {

    climateOutput->PPTMon_cm = NULL;
    climateOutput->meanTempMon_C = NULL;
    climateOutput->maxTempMon_C = NULL;
    climateOutput->minTempMon_C = NULL;

    climateOutput->PPT_cm = NULL;
    climateOutput->PPT7thMon_mm = NULL;
    climateOutput->meanTemp_C = NULL;
    climateOutput->meanTempDriestQtr_C = NULL;
    climateOutput->minTemp2ndMon_C = NULL;
    climateOutput->minTemp7thMon_C = NULL;
    climateOutput->frostFree_days = NULL;
    climateOutput->ddAbove65F_degday = NULL;
    climateAverages->meanTempMon_C = NULL;
    climateAverages->maxTempMon_C = NULL;
    climateAverages->minTempMon_C = NULL;
    climateAverages->PPTMon_cm = NULL;
    climateAverages->sdC4 = NULL;
    climateAverages->sdCheatgrass = NULL;
}

void initializeMonthlyClimatePtrs(SW_CLIMATE_YEARLY *climateOutput) {
    int month;

    for (month = 0; month < MAX_MONTHS; month++) {
        climateOutput->PPTMon_cm[month] = NULL;
        climateOutput->meanTempMon_C[month] = NULL;
        climateOutput->maxTempMon_C[month] = NULL;
        climateOutput->minTempMon_C[month] = NULL;
    }
}

void allocateClimateStructs(
    unsigned int numYears,
    SW_CLIMATE_YEARLY *climateOutput,
    SW_CLIMATE_CLIM *climateAverages,
    LOG_INFO *LogInfo
) {

    unsigned int month;

    initializeClimatePtrs(climateOutput, climateAverages);

    climateOutput->PPTMon_cm = (double **) Mem_Malloc(
        sizeof(double *) * MAX_MONTHS, "allocateClimateStructs()", LogInfo
    );
    if (LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

    climateOutput->meanTempMon_C = (double **) Mem_Malloc(
        sizeof(double *) * MAX_MONTHS, "allocateClimateStructs()", LogInfo
    );
    if (LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

    climateOutput->maxTempMon_C = (double **) Mem_Malloc(
        sizeof(double *) * MAX_MONTHS, "allocateClimateStructs()", LogInfo
    );
    if (LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

    climateOutput->minTempMon_C = (double **) Mem_Malloc(
        sizeof(double *) * MAX_MONTHS, "allocateClimateStructs()", LogInfo
    );
    if (LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

    // Initialize all month pointers before they are allocated
    initializeMonthlyClimatePtrs(climateOutput);

    for (month = 0; month < MAX_MONTHS; month++) {
        climateOutput->PPTMon_cm[month] = (double *) Mem_Malloc(
            sizeof(double) * numYears, "allocateClimateStructs()", LogInfo
        );
        if (LogInfo->stopRun) {
            return; // Exit function prematurely due to error
        }
        climateOutput->meanTempMon_C[month] = (double *) Mem_Malloc(
            sizeof(double) * numYears, "allocateClimateStructs()", LogInfo
        );
        if (LogInfo->stopRun) {
            return; // Exit function prematurely due to error
        }
        climateOutput->maxTempMon_C[month] = (double *) Mem_Malloc(
            sizeof(double) * numYears, "allocateClimateStructs()", LogInfo
        );
        if (LogInfo->stopRun) {
            return; // Exit function prematurely due to error
        }
        climateOutput->minTempMon_C[month] = (double *) Mem_Malloc(
            sizeof(double) * numYears, "allocateClimateStructs()", LogInfo
        );
        if (LogInfo->stopRun) {
            return; // Exit function prematurely due to error
        }
    }

    climateOutput->PPT_cm = (double *) Mem_Malloc(
        sizeof(double) * numYears, "allocateClimateStructs()", LogInfo
    );
    if (LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

    climateOutput->PPT7thMon_mm = (double *) Mem_Malloc(
        sizeof(double) * numYears, "allocateClimateStructs()", LogInfo
    );
    if (LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

    climateOutput->meanTemp_C = (double *) Mem_Malloc(
        sizeof(double) * numYears, "allocateClimateStructs()", LogInfo
    );
    if (LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

    climateOutput->meanTempDriestQtr_C = (double *) Mem_Malloc(
        sizeof(double) * numYears, "allocateClimateStructs()", LogInfo
    );
    if (LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

    climateOutput->minTemp2ndMon_C = (double *) Mem_Malloc(
        sizeof(double) * numYears, "allocateClimateStructs()", LogInfo
    );
    if (LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

    climateOutput->minTemp7thMon_C = (double *) Mem_Malloc(
        sizeof(double) * numYears, "allocateClimateStructs()", LogInfo
    );
    if (LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

    climateOutput->frostFree_days = (double *) Mem_Malloc(
        sizeof(double) * numYears, "allocateClimateStructs()", LogInfo
    );
    if (LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

    climateOutput->ddAbove65F_degday = (double *) Mem_Malloc(
        sizeof(double) * numYears, "allocateClimateStructs()", LogInfo
    );
    if (LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

    climateAverages->meanTempMon_C = (double *) Mem_Malloc(
        sizeof(double) * MAX_MONTHS, "allocateClimateStructs()", LogInfo
    );
    if (LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

    climateAverages->maxTempMon_C = (double *) Mem_Malloc(
        sizeof(double) * MAX_MONTHS, "allocateClimateStructs()", LogInfo
    );
    if (LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

    climateAverages->minTempMon_C = (double *) Mem_Malloc(
        sizeof(double) * MAX_MONTHS, "allocateClimateStructs()", LogInfo
    );
    if (LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

    climateAverages->PPTMon_cm = (double *) Mem_Malloc(
        sizeof(double) * MAX_MONTHS, "allocateClimateStructs()", LogInfo
    );
    if (LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

    climateAverages->sdC4 = (double *) Mem_Malloc(
        sizeof(double) * 3, "allocateClimateStructs()", LogInfo
    );
    if (LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

    climateAverages->sdCheatgrass = (double *) Mem_Malloc(
        sizeof(double) * 3, "allocateClimateStructs()", LogInfo
    );
}

void deallocateClimateStructs(
    SW_CLIMATE_YEARLY *climateOutput, SW_CLIMATE_CLIM *climateAverages
) {

    int month;
    int pointer;
    const int numSinglePtrs = 14;
    const int numDoublePtrs = 4;

    double *singlePtrs[] = {
        climateOutput->PPT_cm,
        climateOutput->PPT7thMon_mm,
        climateOutput->meanTemp_C,
        climateOutput->meanTempDriestQtr_C,
        climateOutput->minTemp2ndMon_C,
        climateOutput->minTemp7thMon_C,
        climateOutput->frostFree_days,
        climateOutput->ddAbove65F_degday,
        climateAverages->meanTempMon_C,
        climateAverages->maxTempMon_C,
        climateAverages->minTempMon_C,
        climateAverages->PPTMon_cm,
        climateAverages->sdC4,
        climateAverages->sdCheatgrass
    };

    double **doublePtrs[] = {
        climateOutput->PPTMon_cm,
        climateOutput->meanTempMon_C,
        climateOutput->maxTempMon_C,
        climateOutput->minTempMon_C
    };

    // Free single pointers
    for (pointer = 0; pointer < numSinglePtrs; pointer++) {
        if (!isnull(singlePtrs[pointer])) {
            free(singlePtrs[pointer]);
            singlePtrs[pointer] = NULL;
        }
    }

    // Free double pointers
    for (pointer = 0; pointer < numDoublePtrs; pointer++) {

        if (!isnull(doublePtrs[pointer])) {
            for (month = 0; month < MAX_MONTHS; month++) {
                if (!isnull(doublePtrs[pointer][month])) {
                    free(doublePtrs[pointer][month]);
                }
            }

            free((void *) doublePtrs[pointer]);
        }
    }
}
