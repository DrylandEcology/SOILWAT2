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
#include "include/generic.h"
#include "include/filefuncs.h"
#include "include/myMemory.h"
#include "include/Times.h"
#include "include/SW_Defines.h"
#include "include/SW_Files.h"
#include "include/SW_Model.h" // externs SW_Model
#include "include/SW_SoilWater.h"
#include "include/SW_Markov.h"

#include "include/SW_Weather.h"



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

/**
 @brief Takes averages through the number of years of the calculated values from calc_SiteClimate

 @param[in] climateOutput Structure of type SW_CLIMATE_YEARLY that holds all output from `calcSiteClimate()`
 like monthly/yearly temperature and precipitation values
 @param[in] numYears Number of years represented within simulation
 @param[out] climateAverages Structure of type SW_CLIMATE_CLIM that holds averages and
 standard deviations output by `averageClimateAcrossYears()`
 */

void averageClimateAcrossYears(SW_CLIMATE_YEARLY *climateOutput, int numYears,
                               SW_CLIMATE_CLIM *climateAverages) {

    int month;

    // Take long-term average of monthly mean, maximum and minimum temperature
    // and precipitation throughout "numYears"
    for(month = 0; month < MAX_MONTHS; month++) {
        climateAverages->meanTempMon_C[month] = mean(climateOutput->meanTempMon_C[month], numYears);
        climateAverages->maxTempMon_C[month] = mean(climateOutput->maxTempMon_C[month], numYears);
        climateAverages->minTempMon_C[month] = mean(climateOutput->minTempMon_C[month], numYears);
        climateAverages->PPTMon_cm[month] = mean(climateOutput->PPTMon_cm[month], numYears);
    }

    // Take the long-term average of yearly precipitation and temperature,
    // C4 and Cheatgrass variables
    climateAverages->PPT_cm = mean(climateOutput->PPT_cm, numYears);
    climateAverages->meanTemp_C = mean(climateOutput->meanTemp_C, numYears);
    climateAverages->PPT7thMon_mm = mean(climateOutput->PPT7thMon_mm, numYears);
    climateAverages->meanTempDriestQtr_C = mean(climateOutput->meanTempDriestQtr_C, numYears);
    climateAverages->minTemp2ndMon_C = mean(climateOutput->minTemp2ndMon_C, numYears);
    climateAverages->ddAbove65F_degday = mean(climateOutput->ddAbove65F_degday, numYears);
    climateAverages->frostFree_days = mean(climateOutput->frostFree_days, numYears);
    climateAverages->minTemp7thMon_C = mean(climateOutput->minTemp7thMon_C, numYears);

    // Calculate and set standard deviation of C4 variables
    climateAverages->sdC4[0] = standardDeviation(climateOutput->minTemp7thMon_C, numYears);
    climateAverages->sdC4[1] = standardDeviation(climateOutput->frostFree_days, numYears);
    climateAverages->sdC4[2] = standardDeviation(climateOutput->ddAbove65F_degday, numYears);

    // Calculate and set the standard deviation of cheatgrass variables
    climateAverages->sdCheatgrass[0] = standardDeviation(climateOutput->PPT7thMon_mm, numYears);
    climateAverages->sdCheatgrass[1] = standardDeviation(climateOutput->meanTempDriestQtr_C, numYears);
    climateAverages->sdCheatgrass[2] = standardDeviation(climateOutput->minTemp2ndMon_C, numYears);
}

/**
 @brief Calculate monthly and annual time series of climate variables from daily weather while adjusting as
 needed depending on if the site in question is in the northern or southern hemisphere.

 This function has two different types of years:

 - The first one being "calendar" years. Which refers to the days and months that show up on a calendar (Jan-Dec).

 - The second type being "adjusted" years. This type is used when the site location is in the southern hemisphere
 and the year starts in July instead of January. For example, the adjusted year 1980 starts half a year earlier on
 July 1st 1979, and ends on June 30th, 1980 (see example below). If our data starts in 1980, then we don't have
 values for the first six months of adjusted year, 1980. Therefore, we start calculating the climate variables in the
 following adjusted year, 1981. And both the first and last six months of data are not used.

 Calendar year vs. adjusted year:
 | Calendar year | Year North | Start N | End N | Year South | Start S | End S |
 |:------------------:|:---------------:|:---------:|:--------:|:---------------:|:---------:|:---------:|
 | 1980 | 1980 | 1980 Jan 1 | 1980 Dec 31 | 1980 | 1979 July 1 | 1980 June 30 |
 | 1981 | 1981 | 1981 Jan 1 | 1981 Dec 31 | 1981 | 1980 July 1 | 1981 June 30 |
 | 1982 | 1982 | 1982 Jan 1 | 1982 Dec 31 | 1982 | 1981 July 1 | 1982 June 30 |
 |    ...    |   ...   |        ...         |           ...        |    ...   |       ...           |         ...           |
 | 2020 | 2020 | 2020 Jan 1 | 2020 Dec 31 | 2020 | 2020 July 1 | 2021 June 30 |

 @param[in] allHist Array containing all historical data of a site
 @param[in] numYears Number of years represented within simulation
 @param[in] startYear Calendar year corresponding to first year of `allHist`
 @param[in] inNorthHem Boolean value specifying if site is in northern hemisphere
 @param[out] climateOutput Structure of type SW_CLIMATE_YEARLY that holds all output from `calcSiteClimate()`
 like monthly/yearly temperature and precipitation values
 */

void calcSiteClimate(SW_WEATHER_HIST **allHist, int numYears, int startYear,
                     Bool inNorthHem, SW_CLIMATE_YEARLY *climateOutput) {

    int month, yearIndex, year, day, numDaysYear, currMonDay;
    int numDaysMonth, adjustedDoy, adjustedYear = 0, secondMonth, seventhMonth,
    adjustedStartYear, calendarYearDays;

    double currentTempMin, currentTempMean, totalAbove65, current7thMonMin, PPT7thMon,
    consecNonFrost, currentNonFrost;

    // Initialize accumulated value arrays to all zeros
    for(month = 0; month < MAX_MONTHS; month++) {
        memset(climateOutput->meanTempMon_C[month], 0., sizeof(double) * numYears);
        memset(climateOutput->maxTempMon_C[month], 0., sizeof(double) * numYears);
        memset(climateOutput->minTempMon_C[month], 0., sizeof(double) * numYears);
        memset(climateOutput->PPTMon_cm[month], 0., sizeof(double) * numYears);
    }
    memset(climateOutput->PPT_cm, 0., sizeof(double) * numYears);
    memset(climateOutput->meanTemp_C, 0., sizeof(double) * numYears);
    memset(climateOutput->minTemp2ndMon_C, 0., sizeof(double) * numYears);
    memset(climateOutput->minTemp7thMon_C, 0., sizeof(double) * numYears);

    calcSiteClimateLatInvariants(allHist, numYears, startYear, climateOutput);

    // Set starting conditions that are dependent on north/south before main loop is entered
    if(inNorthHem) {
        secondMonth = Feb;
        seventhMonth = Jul;
        numDaysMonth = Time_days_in_month(Jan);
        adjustedStartYear = 0;
    } else {
        secondMonth = Aug;
        seventhMonth = Jan;
        numDaysMonth = Time_days_in_month(Jul);
        adjustedStartYear = 1;
        climateOutput->minTemp7thMon_C[0] = SW_MISSING;
        climateOutput->PPT7thMon_mm[0] = SW_MISSING;
        climateOutput->ddAbove65F_degday[0] = SW_MISSING;
        climateOutput->frostFree_days[0] = SW_MISSING;
    }

    // Loop through all years of data starting at "adjustedStartYear"
    for(yearIndex = adjustedStartYear; yearIndex < numYears; yearIndex++) {
        year = yearIndex + startYear;
        Time_new_year(year);
        numDaysYear = Time_get_lastdoy_y(year);
        month = (inNorthHem) ? Jan : Jul;
        currMonDay = 0;
        current7thMonMin = SW_MISSING;
        totalAbove65 = 0;
        currentNonFrost = 0;
        consecNonFrost = 0;
        PPT7thMon = 0;

        if(!inNorthHem) {
            // Get calendar year days only when site is in southern hemisphere
            // To deal with number of days in data
            calendarYearDays = Time_get_lastdoy_y(year - 1);
        }

        for(day = 0; day < numDaysYear; day++) {
            if(inNorthHem) {
                adjustedDoy = day;
                adjustedYear = yearIndex;
            } else {
                // Adjust year and day to meet southern hemisphere requirements
                adjustedDoy = (calendarYearDays == 366) ? day + 182 : day + 181;
                adjustedDoy = adjustedDoy % calendarYearDays;

                if(adjustedDoy == 0) {
                    adjustedYear++;
                    Time_new_year(year);
                }
            }

            if(month == Jul && adjustedYear >= numYears - 1 && !inNorthHem) {
                // Set all current accumulated values to SW_MISSING to prevent
                // a zero going into the last year of the respective arrays
                // Last six months of data is ignored and do not go into
                // a new year of data for "adjusted years"
                PPT7thMon = SW_MISSING;
                totalAbove65 = SW_MISSING;
                currentNonFrost = SW_MISSING;
                consecNonFrost = SW_MISSING;
                currMonDay = SW_MISSING;
                break;
            }

            currMonDay++;
            currentTempMin = allHist[adjustedYear]->temp_min[adjustedDoy];
            currentTempMean = allHist[adjustedYear]->temp_avg[adjustedDoy];

            // Part of code that deals with gathering seventh month information
            if(month == seventhMonth){
                current7thMonMin = (currentTempMin < current7thMonMin) ?
                                            currentTempMin : current7thMonMin;
                PPT7thMon += allHist[adjustedYear]->ppt[adjustedDoy] * 10;
            }

            // Part of code dealing with consecutive amount of days without frost
            if(currentTempMin > 0.0) {
                currentNonFrost += 1.;
            } else if(currentNonFrost > consecNonFrost){
                consecNonFrost = currentNonFrost;
                currentNonFrost = 0.;
            } else {
                currentNonFrost = 0.;
            }

            // Gather minimum temperature of second month of year
            if(month == secondMonth) {
                climateOutput->minTemp2ndMon_C[yearIndex] += allHist[adjustedYear]->temp_min[adjustedDoy];
            }

            // Once we have reached the end of the month days,
            // handle it by getting information about the next month
            if(currMonDay == numDaysMonth) {
                if(month == secondMonth) climateOutput->minTemp2ndMon_C[yearIndex] /= numDaysMonth;

                month = (month + 1) % 12;
                numDaysMonth = Time_days_in_month(month);
                currMonDay = 0;
            }

            // Accumulate information of degrees above 65ºF
                // Check if "currentTempMean" is SW_MISSING
                // When "calendarYearDays" is the number of days in a leap year
                // (e.g. adjustedYear = 1985, calendarYearDays = 366 for previous year)
                // It gets to data that is SW_MISSING, so we want to ignore that
            if(!missing(currentTempMean)) {
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
        climateOutput->frostFree_days[yearIndex] = (consecNonFrost > 0) ? consecNonFrost : currentNonFrost;
    }

    findDriestQtr(numYears, inNorthHem, climateOutput->meanTempDriestQtr_C,
                  climateOutput->meanTempMon_C, climateOutput->PPTMon_cm);
}

/**
 @brief Helper function to `calcSiteClimate()`. Manages all information that is independant of the site
 being in the northern/southern hemisphere.

 @param[in] allHist Array containing all historical data of a site
 @param[in] numYears Number of years represented within simulation
 @param[in] startYear Calendar year corresponding to first year of `allHist`
 @param[out] climateOutput Structure of type SW_CLIMATE_YEARLY that holds all output from `calcSiteClimate()`
 like monthly/yearly temperature and precipitation values
 */

void calcSiteClimateLatInvariants(SW_WEATHER_HIST **allHist, int numYears, int startYear,
                                  SW_CLIMATE_YEARLY *climateOutput) {

    int month = Jan, numDaysMonth = Time_days_in_month(month), yearIndex,
    day, numDaysYear, currMonDay, year;

    for(yearIndex = 0; yearIndex < numYears; yearIndex++) {
        year = yearIndex + startYear;
        Time_new_year(year);
        numDaysYear = Time_get_lastdoy_y(year);
        currMonDay = 0;
        for(day = 0; day < numDaysYear; day++) {
            currMonDay++;
            climateOutput->meanTempMon_C[month][yearIndex] += allHist[yearIndex]->temp_avg[day];
            climateOutput->maxTempMon_C[month][yearIndex] += allHist[yearIndex]->temp_max[day];
            climateOutput->minTempMon_C[month][yearIndex] += allHist[yearIndex]->temp_min[day];
            climateOutput->PPTMon_cm[month][yearIndex] += allHist[yearIndex]->ppt[day];
            climateOutput->PPT_cm[yearIndex] += allHist[yearIndex]->ppt[day];
            climateOutput->meanTemp_C[yearIndex] += allHist[yearIndex]->temp_avg[day];

            if(currMonDay == numDaysMonth) {
                climateOutput->meanTempMon_C[month][yearIndex] /= numDaysMonth;
                climateOutput->maxTempMon_C[month][yearIndex] /= numDaysMonth;
                climateOutput->minTempMon_C[month][yearIndex] /= numDaysMonth;

                month = (month + 1) % MAX_MONTHS;
                numDaysMonth = Time_days_in_month(month);
                currMonDay = 0;
            }
        }
        climateOutput->meanTemp_C[yearIndex] /= numDaysYear;
    }

}

/**
 @brief Helper function to `calcSiteClimate()` to find the average temperature during the driest quarter of the year

 @param[in] numYears Number of years represented within simulation
 @param[in] inNorthHem Boolean value specifying if site is in northern hemisphere
 @param[out] meanTempDriestQtr_C Array of size numYears holding the average temperature of the
 driest quarter of the year for every year
 @param[out] meanTempMon_C 2D array containing monthly means average daily air temperature (deg;C) with
 dimensions of row (months) size MAX_MONTHS and columns (years) of size numYears
 @param[out] PPTMon_cm 2D array containing monthly amount precipitation (cm) with dimensions
 of row (months) size MAX_MONTHS and columns (years) of size numYears
 */
void findDriestQtr(int numYears, Bool inNorthHem, double *meanTempDriestQtr_C,
                   double **meanTempMon_C, double **PPTMon_cm) {

    int yearIndex, month, prevMonth, nextMonth, adjustedMonth = 0,
    numQuarterMonths = 3, endNumYears = (inNorthHem) ? numYears : numYears - 1;

    // NOTE: These variables are the same throughout the program if site is in
    // northern hempisphere
    // The main purpose of these are to easily control the correct year when
    // dealing with adjusted years in the southern hempisphere
    int adjustedYearZero = 0, adjustedYearOne = 0, adjustedYearTwo = 0;

    double driestThreeMonPPT, driestMeanTemp, currentQtrPPT, currentQtrTemp;

    for(yearIndex = 0; yearIndex < endNumYears; yearIndex++) {
        driestThreeMonPPT = SW_MISSING;
        driestMeanTemp = SW_MISSING;

        adjustedYearZero = adjustedYearOne = adjustedYearTwo = yearIndex;

        for(month = 0; month < MAX_MONTHS; month++) {

            if(inNorthHem) {
                adjustedMonth = month;

                prevMonth = (adjustedMonth == Jan) ? Dec : adjustedMonth - 1;
                nextMonth = (adjustedMonth == Dec) ? Jan : adjustedMonth + 1;
            } else {
                driestQtrSouthAdjMonYears(month, &adjustedYearZero, &adjustedYearOne,
                                    &adjustedYearTwo, &adjustedMonth, &prevMonth, &nextMonth);
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
            if(currentQtrPPT < driestThreeMonPPT) {
                // Make current temperature/precipitation the new lowest
                driestMeanTemp = currentQtrTemp;
                driestThreeMonPPT = currentQtrPPT;
            }

        }

        meanTempDriestQtr_C[yearIndex] = driestMeanTemp / numQuarterMonths;

    }

    if(!inNorthHem) meanTempDriestQtr_C[numYears - 1] = SW_MISSING;

}


/**
 @brief Helper function to `findDriestQtr()` to find adjusted months and years when the current site is in
 the southern hempisphere.

 When site is in the southern hemisphere, the year starts in July and ends in June of the next calendar year, so
 months and years need to be adjusted to wrap around years to get accurate driest quarters in a year.
 See `calcSiteClimate()` documentation for more explanation on adjusted months and adjusted/calendar years.

 @param[in] month Current month of the year [January, December]
 @param[in,out] adjustedYearZero First adjusted year that is paired with previous month of year
 @param[in,out] adjustedYearOne Second adjusted year that is paired with current month of year
 @param[in,out] adjustedYearTwo Third adjusted year that is paired with next month of year
 @param[in,out] adjustedMonth Adjusted month starting at July going to June of next calendar year
 @param[in,out] prevMonth Month preceding current input month
 @param[in,out] nextMonth Month following current input month
 */
void driestQtrSouthAdjMonYears(int month, int *adjustedYearZero, int *adjustedYearOne,
                           int *adjustedYearTwo, int *adjustedMonth, int *prevMonth,
                           int *nextMonth)
{
    *adjustedMonth = month + Jul;
    *adjustedMonth %= MAX_MONTHS;

    // Adjust prevMonth, nextMonth and adjustedYear(s) to the respective adjustedMonth
    switch(*adjustedMonth) {
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
 @brief Reads in all weather data

 Reads in weather data from disk (if available) for all years and
 stores values in global SW_Weather's `allHist`.
 If missing, set values to `SW_MISSING`.

 @param[out] allHist 2D array holding all weather data gathered
 @param[in] startYear Start year of the simulation
 @param[in] n_years Number of years in simulation
 @param[in] use_weathergenerator_only A boolean; if `swFALSE`, code attempts to
   read weather files from disk.
 @param[in] weather_prefix File name of weather data without extension.

*/
void readAllWeather(
  SW_WEATHER_HIST **allHist,
  int startYear,
  unsigned int n_years,
  Bool use_weathergenerator_only,
  char weather_prefix[]
) {
    unsigned int yearIndex;

    for(yearIndex = 0; yearIndex < n_years; yearIndex++) {

        // Set all daily weather values to missing
        _clear_hist_weather(allHist[yearIndex]);

        // Read daily weather values from disk
        if (!use_weathergenerator_only) {

            _read_weather_hist(
              startYear + yearIndex,
              allHist[yearIndex],
              weather_prefix
            );
        }
    }
}



/**
  @brief Impute missing values and scale with monthly parameters

  Finalize weather values after they have been read in via
  `readAllWeather()` or `SW_WTH_read()`
  (the latter also handles (re-)allocation).
*/
void finalizeAllWeather(SW_WEATHER *w) {
  // Impute missing values
  generateMissingWeather(
    w->allHist,
    w->startYear,
    w->n_years,
    w->generateWeatherMethod,
    3 // optLOCF_nMax (TODO: make this user input)
  );


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
    w->scale_shortWaveRad
  );

  // Make sure all input, scaled, generated, and calculated daily weather values
  // are within reason
  checkAllWeather(w);
}


void SW_WTH_finalize_all_weather(void) {
  finalizeAllWeather(&SW_Weather);
}


/**
  @brief Apply temperature, precipitation, cloud cover, relative humidity, and wind speed
         scaling to daily weather values

  @param[in,out] allHist 2D array holding all weather data
  @param[in] startYear Start year of the simulation (and `allHist`)
  @param[in] n_years Number of years in simulation (length of `allHist`)
  @param[in] scale_temp_max Array of monthly, additive scaling parameters to
    modify daily maximum air temperature [C]
  @param[in] scale_temp_min Array of monthly, additive scaling parameters to
    modify daily minimum air temperature [C]
  @param[in] scale_precip Array of monthly, multiplicative scaling parameters to
    modify daily precipitation [-]
 @param[in] scale_skyCover Array of monthly, additive scaling parameters to modify daily sky cover [%]
 @param[in] scale_wind Array of monthly, multiplicitive scaling parameters to modify daily wind speed [-]
 @param[in] scale_rH Array of monthly, additive scaling parameters to modify daily relative humidity [%]
 @param[in] scale_actVapPress Array of monthly, multiplicitive scaling parameters to modify daily actual vapor pressure [-]
 @param[in] scale_shortWaveRad Array of monthly, multiplicitive scaling parameters to modify daily shortwave radiation [%]

  @note Daily average air temperature is re-calculated after scaling
    minimum and maximum air temperature.

  @note Missing values in `allHist` remain unchanged.
*/
void scaleAllWeather(
  SW_WEATHER_HIST **allHist,
  int startYear,
  unsigned int n_years,
  double *scale_temp_max,
  double *scale_temp_min,
  double *scale_precip,
  double *scale_skyCover,
  double *scale_wind,
  double *scale_rH,
  double *scale_actVapPress,
  double *scale_shortWaveRad
) {

  int year, month;
  unsigned int yearIndex, numDaysYear, day;

  Bool trivial = swTRUE;

  // Check if we have any non-trivial scaling parameter
  for (month = 0; trivial && month < MAX_MONTHS; month++) {
    trivial = (Bool) (
      ZRO(scale_temp_max[month]) &&
      ZRO(scale_temp_min[month]) &&
      EQ(scale_precip[month], 1.)
    );
  }

  if (!trivial) {
    // Apply scaling parameters to each day of `allHist`
    for (yearIndex = 0; yearIndex < n_years; yearIndex++) {
      year = yearIndex + startYear;
      Time_new_year(year); // required for `doy2month()`
      numDaysYear = Time_get_lastdoy_y(year);

      for (day = 0; day < numDaysYear; day++) {
        month = doy2month(day + 1);

        if (!missing(allHist[yearIndex]->temp_max[day])) {
          allHist[yearIndex]->temp_max[day] += scale_temp_max[month];
        }

        if (!missing(allHist[yearIndex]->temp_min[day])) {
          allHist[yearIndex]->temp_min[day] += scale_temp_min[month];
        }

        if (!missing(allHist[yearIndex]->ppt[day])) {
          allHist[yearIndex]->ppt[day] *= scale_precip[month];
        }

        if(!missing(allHist[yearIndex]->cloudcov_daily[day])) {
            allHist[yearIndex]->cloudcov_daily[day] = fmin(
            100.,
            fmax(0.0, scale_skyCover[month] + allHist[yearIndex]->cloudcov_daily[day])
            );
        }

        if(!missing(allHist[yearIndex]->windspeed_daily[day])) {
            allHist[yearIndex]->windspeed_daily[day] = fmax(
            0.0,
            scale_wind[month] * allHist[yearIndex]->windspeed_daily[day]
            );
        }

        if(!missing(allHist[yearIndex]->r_humidity_daily[day])) {
            allHist[yearIndex]->r_humidity_daily[day] = fmin(
            100.,
            fmax(0.0, scale_rH[month] + allHist[yearIndex]->r_humidity_daily[day])
            );
        }

        if(!missing(allHist[yearIndex]->actualVaporPressure[day])) {
            allHist[yearIndex]->actualVaporPressure[day] = fmax(
            0.0,
            scale_actVapPress[month] * allHist[yearIndex]->actualVaporPressure[day]
            );
        }

        if(!missing(allHist[yearIndex]->shortWaveRad[day])) {
            allHist[yearIndex]->shortWaveRad[day] = fmax(
            0.0,
            scale_shortWaveRad[month] * allHist[yearIndex]->shortWaveRad[day]
            );
        }

        /* re-calculate average air temperature */
        if (
          !missing(allHist[yearIndex]->temp_max[day]) &&
          !missing(allHist[yearIndex]->temp_min[day])
        ) {
          allHist[yearIndex]->temp_avg[day] =
            (allHist[yearIndex]->temp_max[day] + allHist[yearIndex]->temp_min[day]) / 2.;
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
     1. Some individual days are missing (values correspond to #SW_MISSING)
     2. An entire year is missing (file `weath.xxxx` for year `xxxx` is absent)
     3. No daily weather input files are available

  Available methods to generate weather:
     1. Pass through (`method` = 0)
     2. Imputation by last-value-carried forward "LOCF" (`method` = 1)
        - for minimum and maximum temperature
        - precipitation is set to 0
        - error if more than `optLOCF_nMax` days per calendar year are missing
     3. First-order Markov weather generator (`method` = 2)

  The user can specify that SOILWAT2 generates all weather without reading
  any historical weather data files from disk
  (see `weathsetup.in`: use weather generator for all weather).

  @note `SW_MKV_today()` is called if `method` = 2
  (i.e., the weather generator is used);
  this requires that appropriate structures are initialized.

  @param[in,out] allHist 2D array holding all weather data
  @param[in] startYear Start year of the simulation
  @param[in] n_years Number of years in simulation
  @param[in] method Number to identify which method to apply to generate
    missing values (see details).
  @param[in] optLOCF_nMax Maximum number of missing days per year (e.g., 5)
    before imputation by `LOCF` throws an error.
*/
void generateMissingWeather(
  SW_WEATHER_HIST **allHist,
  int startYear,
  unsigned int n_years,
  unsigned int method,
  unsigned int optLOCF_nMax
) {

  int year;
  unsigned int yearIndex, numDaysYear, day, iMissing;

  double yesterdayPPT = 0., yesterdayMin = 0., yesterdayMax = 0.;

  Bool any_missing, missing_Tmax, missing_Tmin, missing_PPT;


  // Pass through method: return early
  if (method == 0) return;


  // Error out if method not implemented
  if (method > 2) {
    LogError(
      logfp,
      LOGFATAL,
      "generateMissingWeather(): method = %u is not implemented.\n",
      method
    );
  }


  // Loop over years
  for (yearIndex = 0; yearIndex < n_years; yearIndex++) {
    year = yearIndex + startYear;
    numDaysYear = Time_get_lastdoy_y(year);
    iMissing = 0;

    for (day = 0; day < numDaysYear; day++) {
      missing_Tmax = (Bool) missing(allHist[yearIndex]->temp_max[day]);
      missing_Tmin = (Bool) missing(allHist[yearIndex]->temp_min[day]);
      missing_PPT = (Bool) missing(allHist[yearIndex]->ppt[day]);

      any_missing = (Bool) (missing_Tmax || missing_Tmin || missing_PPT);

      if (any_missing) {
        // some of today's values are missing

        if (method == 2) {
          // Weather generator
          allHist[yearIndex]->ppt[day] = yesterdayPPT;
          SW_MKV_today(
            day,
            &allHist[yearIndex]->temp_max[day],
            &allHist[yearIndex]->temp_min[day],
            &allHist[yearIndex]->ppt[day]
          );

        } else if (method == 1) {
          // LOCF (temp) + 0 (PPT)
          allHist[yearIndex]->temp_max[day] = missing_Tmax ?
            yesterdayMax :
            allHist[yearIndex]->temp_max[day];

          allHist[yearIndex]->temp_min[day] = missing_Tmin ?
            yesterdayMin :
            allHist[yearIndex]->temp_min[day];

          allHist[yearIndex]->ppt[day] = missing_PPT ?
            0. :
            allHist[yearIndex]->ppt[day];


          // Throw an error if too many values per calendar year are missing
          iMissing++;

          if (iMissing > optLOCF_nMax) {
            LogError(
              logfp,
              LOGFATAL,
              "generateMissingWeather(): more than %u days missing in year %u "
              "and weather generator turned off.\n",
              optLOCF_nMax,
              year
            );
          }
        }


        // Re-calculate average air temperature
        allHist[yearIndex]->temp_avg[day] = (
          allHist[yearIndex]->temp_max[day] + allHist[yearIndex]->temp_min[day]
        ) / 2.;
      }

      yesterdayPPT = allHist[yearIndex]->ppt[day];
      yesterdayMax = allHist[yearIndex]->temp_max[day];
      yesterdayMin = allHist[yearIndex]->temp_min[day];
    }
  }
}

/**
 @brief Check weather through all years/days within simultation and make sure all input values are reasonable
 after possible weather generation and scaling. If a value is to be found unreasonable, the function will execute a program crash.

 @param weather Struct of type SW_WEATHER holding all relevant information pretaining to weather input data
 */
void checkAllWeather(SW_WEATHER *weather) {

    // Initialize any variables
    TimeInt year, doy, numDaysInYear;
    SW_WEATHER_HIST **weathHist = weather->allHist;

    double dailyMinTemp, dailyMaxTemp;

    // Check if minimum or maximum temperature, or precipitation flags are 0
    if(!weather->has_temp2 || !weather->has_ppt) {
        // Fail
        LogError(logfp, LOGFATAL, "Temperature and/or precipitation flag(s) are unset.");
    }

    // Loop through `allHist` years
    for(year = 0; year < weather->n_years; year++) {
        numDaysInYear = Time_get_lastdoy_y(year);

        // Loop through `allHist` days
        for(doy = 0; doy < numDaysInYear; doy++) {

            dailyMaxTemp = weathHist[year]->temp_max[doy];
            dailyMinTemp = weathHist[year]->temp_min[doy];

            // Check if minimum temp greater than maximum temp
            if(dailyMinTemp > dailyMaxTemp) {

                // Fail
                LogError(logfp, LOGFATAL, "Daily input value for minumum temperature"
                         " is greater than daily input value for maximum temperature (minimum = %f, maximum = %f)"
                         " on day %d of year %d.", dailyMinTemp, dailyMaxTemp, doy + 1, year + weather->startYear);
            }
            // Otherwise, check if maximum or minimum temp, or
            // dew point temp is not [-100, 100]
            else if((dailyMinTemp > 100. || dailyMinTemp < -100.) ||
                    (dailyMaxTemp > 100. || dailyMaxTemp < -100.)) {

                // Fail
                LogError(logfp, LOGFATAL, "Daily minimum and/or maximum temperature on "
                         "day %d of year %d do not fit in the range of [-100, 100] C.",
                         doy, year + weather->startYear);
            }
            // Otherwise, check if precipitation is less than 0cm
            else if(weathHist[year]->ppt[doy] < 0) {

                // Fail
                LogError(logfp, LOGFATAL, "Invalid daily precipitation value: %f cm (< 0) on day %d of year %d.",
                         weathHist[year]->ppt[doy], doy + 1, year + weather->startYear);
            }
            // Otherwise, check if relative humidity is less than 0% or greater than 100%
            else if(weathHist[year]->r_humidity_daily[doy] < 0. ||
                    weathHist[year]->r_humidity_daily[doy] > 100.) {

                // Fail
                LogError(logfp, LOGFATAL, "Invalid daily/calculated relative humidity value did"
                         " not fall in the range [0, 100] % (relative humidity = %f). ",
                         weathHist[year]->r_humidity_daily[doy]);
            }
            // Otherwise, check if cloud cover was input and
            // if the value is less than 0% or greater than 100%
            else if(weathHist[year]->cloudcov_daily[doy] < 0. ||
                    weathHist[year]->cloudcov_daily[doy] > 100.) {

                // Fail
                LogError(logfp, LOGFATAL, "Invalid daily/calculated cloud cover value did"
                         " not fall in the range [0, 100] % (cloud cover = %f). ",
                         weathHist[year]->cloudcov_daily[doy]);
            }
            // Otherwise, check if wind speed is less than 0 m/s
            else if(weathHist[year]->windspeed_daily[doy] < 0.) {

                // Fail
                LogError(logfp, LOGFATAL, "Invalid daily wind speed value is less than zero."
                         "(wind speed = %f) on day %d of year %d. ",
                         weathHist[year]->windspeed_daily[doy], doy + 1, year + weather->startYear);
            }
            // Otherwise, check if radiation if less than 0 W * m^-2
            else if(weathHist[year]->shortWaveRad[doy] < 0.) {

                // Fail
                LogError(logfp, LOGFATAL, "Invalid daily shortwave radiation value is less than zero."
                         "(shortwave radation = %f) on day %d of year %d. ",
                         weathHist[year]->shortWaveRad[doy], doy + 1, year + weather->startYear);
            }
            // Otherwise, check if actual vapor pressure is less than 0 kPa
            else if(weathHist[year]->actualVaporPressure[doy] < 0.) {

                // Fail
                LogError(logfp, LOGFATAL, "Invalid daily actual vapor pressure value is less than zero."
                         "(actual vapor pressure = %f) on day %d of year %d. ",
                         weathHist[year]->actualVaporPressure[doy], doy + 1, year + weather->startYear);
            }
        }
    }
}


/**
@brief Clears weather history.
@note Used by rSOILWAT2
*/
void _clear_hist_weather(SW_WEATHER_HIST *yearWeather) {
	/* --------------------------------------------------- */
	TimeInt d;

	for (d = 0; d < MAX_DAYS; d++) {
      yearWeather->ppt[d] = SW_MISSING;
      yearWeather->temp_max[d] = SW_MISSING;
      yearWeather->temp_min[d] = SW_MISSING;
      yearWeather->temp_avg[d] = SW_MISSING;
      yearWeather->cloudcov_daily[d] = SW_MISSING;
      yearWeather->windspeed_daily[d] = SW_MISSING;
      yearWeather->r_humidity_daily[d] = SW_MISSING;
  }
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

	if (SW_Weather.generateWeatherMethod == 2) {
		SW_MKV_deconstruct();
	}

    deallocateAllWeather(&SW_Weather);
}


/**
  @brief Allocate memory for `allHist` for `w` based on `n_years`
*/
void allocateAllWeather(SW_WEATHER *w) {
  unsigned int year;

  w->allHist = (SW_WEATHER_HIST **)malloc(sizeof(SW_WEATHER_HIST *) * w->n_years);

  for (year = 0; year < w->n_years; year++) {

      w->allHist[year] = (SW_WEATHER_HIST *)malloc(sizeof(SW_WEATHER_HIST));
  }
}


/**
 @brief Helper function to SW_WTH_deconstruct to deallocate `allHist` of `w`.
 */

void deallocateAllWeather(SW_WEATHER *w) {
    unsigned int year;

    if(!isnull(w->allHist)) {
        for(year = 0; year < w->n_years; year++) {
            free(w->allHist[year]);
        }

        free(w->allHist);
        w->allHist = NULL;
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
	SW_Weather.now.temp_max = SW_Weather.now.temp_min = 0.;
	SW_Weather.now.ppt = SW_Weather.now.rain = 0.;
	SW_Weather.snow = SW_Weather.snowmelt = SW_Weather.snowloss = 0.;
	SW_Weather.snowRunoff = 0.;
	SW_Weather.surfaceRunoff = SW_Weather.surfaceRunon = 0.;
	SW_Weather.soil_inf = 0.;
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
    SW_WEATHER_NOW *wn = &SW_Weather.now;

    /* Indices to today's weather record in `allHist` */
    TimeInt
      day = SW_Model.doy - 1,
      yearIndex = SW_Model.year - SW_Weather.startYear;

/*
#ifdef STEPWAT
	 TimeInt doy = SW_Model.doy;
	 Bool is_warm;
	 Bool is_growingseason = swFALSE;
#endif
*/

    /* get the daily weather from allHist */
    if (
      missing(w->allHist[yearIndex]->temp_avg[day]) ||
      missing(w->allHist[yearIndex]->ppt[day]) ||
      missing(w->allHist[yearIndex]->cloudcov_daily[day]) ||
      missing(w->allHist[yearIndex]->windspeed_daily[day]) ||
      missing(w->allHist[yearIndex]->r_humidity_daily[day])
    ) {
      LogError(
        logfp,
        LOGFATAL,
        "Missing weather data (day %u - %u) during simulation.",
        SW_Model.year,
        SW_Model.doy
      );
    }

    wn->temp_max = w->allHist[yearIndex]->temp_max[day];
    wn->temp_min = w->allHist[yearIndex]->temp_min[day];
    wn->ppt = w->allHist[yearIndex]->ppt[day];
    wn->cloudCover = w->allHist[yearIndex]->cloudcov_daily[day];
    wn->windSpeed = w->allHist[yearIndex]->windspeed_daily[day];
    wn->relHumidity = w->allHist[yearIndex]->r_humidity_daily[day];
    wn->shortWaveRad = w->allHist[yearIndex]->shortWaveRad[day];
    wn->actualVaporPressure = w->allHist[yearIndex]->actualVaporPressure[day];

    wn->temp_avg = w->allHist[yearIndex]->temp_avg[day];

    w->snow = w->snowmelt = w->snowloss = 0.;
    w->snowRunoff = w->surfaceRunoff = w->surfaceRunon = w->soil_inf = 0.;

    if (w->use_snow)
    {
        SW_SWC_adjust_snow(wn->temp_min, wn->temp_max, wn->ppt,
          &wn->rain, &w->snow, &w->snowmelt);
    } else {
        wn->rain = wn->ppt;
    }
}

/**
@brief Reads in file for SW_Weather.
*/
void SW_WTH_setup(void) {
	/* =================================================== */
	SW_WEATHER *w = &SW_Weather;
	const int nitems = 34;
	FILE *f;
	int lineno = 0, month, x, columnNum, varArrIndex = 0;
    int tempCompIndex = 3, windCompIndex = 7, hursCompIndex = 9;
    Bool componentFlag;
	RealF sppt, stmax, stmin;
	RealF sky, wind, rH, actVP, shortWaveRad;

    Bool *inputFlags[MAX_INPUT_COLUMNS] = {&w->use_cloudCoverMonthly, &w->use_windSpeedMonthly,
        &w->use_relHumidityMonthly, &w->has_temp2, &w->has_ppt, &w->has_cloudCover, &w->has_sfcWind,
        &w->has_windComp, &w->has_hurs, &w->has_hurs2, &w->has_huss, &w->has_tdps, &w->has_vp, &w->has_rsds};

    int varIndices[MAX_INPUT_COLUMNS];

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
			w->use_weathergenerator_only = swFALSE;

			switch (x) {
				case 0:
					// As is
					w->generateWeatherMethod = 0;
					break;

				case 1:
					// weather generator
					w->generateWeatherMethod = 2;
					break;

				case 2:
					// weather generatory only
					w->generateWeatherMethod = 2;
					w->use_weathergenerator_only = swTRUE;
					break;

				case 3:
					// LOCF (temp) + 0 (PPT)
					w->generateWeatherMethod = 1;
					break;

				default:
					CloseFile(&f);
					LogError(
						logfp,
						LOGFATAL,
						"%s : Bad missing weather method %d.",
						MyFileName,
						x
					);
			}
			break;

		case 4:
			w->rng_seed = atoi(inbuf);
			break;

        case 5:
            w->use_cloudCoverMonthly = itob(atoi(inbuf));
            break;

        case 6:
            w->use_windSpeedMonthly = itob(atoi(inbuf));
            break;

        case 7:
            w->use_relHumidityMonthly = itob(atoi(inbuf));
            break;

        case 8:
            w->has_temp2 = itob(atoi(inbuf));
            break;

        case 9:
            if(w->has_temp2) {
                w->has_temp2 = itob(atoi(inbuf));
            }
            break;

        case 10:
            w->has_ppt = itob(atoi(inbuf));
            break;

        case 11:
            w->has_cloudCover = itob(atoi(inbuf));
            break;

        case 12:
            w->has_sfcWind = itob(atoi(inbuf));
            break;

        case 13:
            w->has_windComp = itob(atoi(inbuf));
            break;

        case 14:
            if(w->has_windComp) {
                w->has_windComp = itob(atoi(inbuf));
            }
            break;

        case 15:
            w->has_hurs = itob(atoi(inbuf));
            break;

        case 16:
            w->has_hurs2 = itob(atoi(inbuf));
            break;

        case 17:
            if(w->has_hurs2) {
                w->has_hurs2 = itob(atoi(inbuf));
            }
            break;

        case 18:
            w->has_huss  = itob(atoi(inbuf));
            break;

        case 19:
            w->has_tdps  = itob(atoi(inbuf));
            break;

        case 20:
            w->has_vp  = itob(atoi(inbuf));
            break;

        case 21:
            w->has_rsds  = itob(atoi(inbuf));
            break;

		default:
			if (lineno == 5 + MAX_MONTHS)
				break;

			x = sscanf(
				inbuf,
				"%d %f %f %f %f %f %f %f %f",
				&month, &sppt, &stmax, &stmin, &sky, &wind, &rH, &actVP, &shortWaveRad
			);

			if (x != 9) {
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
            w->scale_actVapPress[month] = actVP;
            w->scale_shortWaveRad[month] = shortWaveRad;
		}

		lineno++;
	}

    // Check if monthly flags have been chosen to override daily flags
    w->has_sfcWind = (w->use_windSpeedMonthly) ? swFALSE : w->has_sfcWind;
    w->has_windComp = (w->use_windSpeedMonthly) ? swFALSE : w->has_windComp;

    w->has_hurs = (w->use_relHumidityMonthly) ? swFALSE : w->has_hurs;
    w->has_hurs2 = (w->use_relHumidityMonthly) ? swFALSE : w->has_hurs2;

    w->has_cloudCover = (w->use_cloudCoverMonthly) ? swFALSE : w->has_cloudCover;

	SW_WeatherPrefix(w->name_prefix);
	CloseFile(&f);

	if (lineno < nitems) {
		LogError(logfp, LOGFATAL, "%s : Too few input lines.", MyFileName);
	}

    // Calculate value indices for `allHist`

    // Default n_input_forcings to 0
    w->n_input_forcings = 0;

        // Loop through MAX_INPUT_COLUMNS
    for(columnNum = 3; columnNum < MAX_INPUT_COLUMNS; columnNum++)
    {
        varIndices[varArrIndex] = 0;

        // Check if current flag is in relation to component variables
        if(&inputFlags[columnNum] == &inputFlags[tempCompIndex] ||
           &inputFlags[columnNum] == &inputFlags[windCompIndex] ||
           &inputFlags[columnNum] == &inputFlags[hursCompIndex]) {

            // Set component flag
            componentFlag = swTRUE;
            varIndices[varArrIndex + 1] = 0;
        } else {
            componentFlag = swFALSE;
        }

        // Check if current flag is set
        if(*inputFlags[columnNum]) {

            // Set current index to "n_input_forcings"
            varIndices[varArrIndex] = w->n_input_forcings;

            // Check if flag is meant for components variables
            if(componentFlag) {

                // Set next index to n_input_focings + 1
                varIndices[varArrIndex + 1] = w->n_input_forcings + 1;

                // Increment "varArrIndex" by two
                varArrIndex += 2;

                // Increment "n_input_forcings" by two
                w->n_input_forcings += 2;
            } else {
            // Otherwise, current flag is not meant for components

                // Increment "varArrIndex" by one
                varArrIndex++;

                // Increment "n_input_forcings" by one
                w->n_input_forcings++;
            }
        } else {
            // Otherwise, flag was not set, deal with "varArrayIndex"

            // Check if current flag is for component variables
            if(componentFlag) {
                // Increment "varArrIndex" by two
                varArrIndex += 2;
            } else {
                // Increment "varArrIndex" by one
                varArrIndex++;
            }
        }
    }

    // Set variable indices in SW_WEATHER
    // Note: SW_WEATHER index being set is guaranteed to have the respective value
    // at the given index of `varIndices` (e.g., w->windComp1_index will only be at index 5 in `varIndices`)
    w->tempComp1_index = varIndices[0], w->tempComp2_index = varIndices[1], w->ppt_index = varIndices[2];
    w->cloudCover_index = varIndices[3], w->sfcWind_index = varIndices[4], w->windComp1_index = varIndices[5];
    w->windComp2_index = varIndices[6], w->hurs_index = varIndices[7], w->hurs_comp1_index = varIndices[8];
    w->hurs_comp2_index = varIndices[9], w->huss_index = varIndices[10], w->tdps_index = varIndices[11];
    w->vp_index = varIndices[12], w->rsds_index = varIndices[13];
}


/**
  @brief (Re-)allocate `allHist` and read daily meteorological input from disk

  The weather generator is not run and daily values are not scaled with
  monthly climate parameters, see `SW_WTH_finalize_all_weather()` instead.
*/
void SW_WTH_read(void) {

    // Deallocate (previous, if any) `allHist`
    // (using value of `SW_Weather.n_years` previously used to allocate)
    // `SW_WTH_construct()` sets `n_years` to zero
    deallocateAllWeather(&SW_Weather);

    // Update number of years and first calendar year represented
    SW_Weather.n_years = SW_Model.endyr - SW_Model.startyr + 1;
    SW_Weather.startYear = SW_Model.startyr;

    // Allocate new `allHist` (based on current `SW_Weather.n_years`)
    allocateAllWeather(&SW_Weather);

    // Read daily meteorological input from disk (if available)
    readAllWeather(
      SW_Weather.allHist,
      SW_Weather.startYear,
      SW_Weather.n_years,
      SW_Weather.use_weathergenerator_only,
      SW_Weather.name_prefix
    );
}



/** @brief Read the historical (observed) weather file for a simulation year.

    The naming convection of the weather input files:
      `[weather-data path/][weather-file prefix].[year]`

    Format of a input file (white-space separated values):
      `doy maxtemp(&deg;C) mintemp (&deg;C) precipitation (cm)`

    @note Used by rSOILWAT2

    @param year Current year within the simulation
    @param yearWeather Current year's weather array that is to be filled by function
    @param weather_prefix File name of weather data without extension.
*/
void _read_weather_hist(
  TimeInt year,
  SW_WEATHER_HIST *yearWeather,
  char weather_prefix[]
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
    SW_SKY *sky = &SW_Sky;
    SW_WEATHER *weath = &SW_Weather;
	int x, lineno = 0, doy;
	// TimeInt mon, j, k = 0;
	// RealF acc = 0.0;
    RealF weathInput[MAX_INPUT_COLUMNS + 2];

    int maxTempIndex = weath->tempComp1_index, minTempIndex = weath->tempComp2_index,
    precipIndex = weath->ppt_index, cloudcovIndex = weath->cloudCover_index,
    windSpeedIndex = weath->sfcWind_index, windComp1Index = weath->windComp1_index,
    windComp2Index = weath->windComp2_index, relHumIndex = weath->huss_index,
    vaporPressIndex = weath->vp_index, hursComp1Index = weath->hurs_comp1_index,
    hursComp2Index = weath->hurs_comp2_index, hussIndex = weath->huss_index,
    dewPointIndex = weath->tdps_index;

    double es, e, relHum, averageTemp = 0.;

    /* Interpolation is to be in base0 in `interpolate_monthlyValues()` */
    Bool interpAsBase1 = swFALSE;

	char fname[MAX_FILENAMESIZE];

  // Create file name: `[weather-file prefix].[year]`
	snprintf(fname, MAX_FILENAMESIZE, "%s.%4d", weather_prefix, year);

	if (NULL == (f = fopen(fname, "r")))
		return;

    // Update yearly day/month information needed when interpolating
    // cloud cover, wind speed, and relative humidity if necessary
    Time_new_year(year);

    if(weath->use_cloudCoverMonthly) {
        interpolate_monthlyValues(sky->cloudcov, interpAsBase1, yearWeather->cloudcov_daily);
    }

    if(weath->use_windSpeedMonthly) {
        interpolate_monthlyValues(sky->windspeed, interpAsBase1, yearWeather->windspeed_daily);
    }

    if(weath->use_relHumidityMonthly) {
        interpolate_monthlyValues(sky->r_humidity, interpAsBase1, yearWeather->r_humidity_daily);
    }

	while (GetALine(f, inbuf)) {
		lineno++;
		x = sscanf(inbuf, "%d %f %f %f %f %f %f %f %f %f %f %f %f %f %f",
                   &doy, &weathInput[0], &weathInput[1], &weathInput[2], &weathInput[3],
                   &weathInput[4], &weathInput[5], &weathInput[6], &weathInput[7],
                   &weathInput[8], &weathInput[9], &weathInput[10], &weathInput[11],
                   &weathInput[12], &weathInput[13]);

		if (x != weath->n_input_forcings + 1) {
			CloseFile(&f);
			LogError(logfp, LOGFATAL, "%s : Incomplete record %d (doy=%d).", fname, lineno, doy);
		}
		if (x > 15) {
			CloseFile(&f);
			LogError(logfp, LOGFATAL, "%s : Too many values in record %d (doy=%d).", fname, lineno, doy);
		}
		if (doy < 1 || doy > MAX_DAYS) {
			CloseFile(&f);
			LogError(logfp, LOGFATAL, "%s : Day of year out of range, line %d.", fname, lineno);
		}

		/* --- Make the assignments ---- */
		doy--; // base1 -> base0
        yearWeather->temp_max[doy] = weathInput[maxTempIndex];
        yearWeather->temp_min[doy] = weathInput[minTempIndex];
        yearWeather->ppt[doy] = weathInput[precipIndex];

        // Calculate average air temperature if min/max not missing
        if (!missing(weathInput[maxTempIndex]) && !missing(weathInput[minTempIndex])) {

          yearWeather->temp_avg[doy] = (weathInput[maxTempIndex] +
                                        weathInput[minTempIndex]) / 2.0;

          averageTemp = yearWeather->temp_avg[doy];
        }

        if(!weath->use_cloudCoverMonthly && weath->has_cloudCover) {
            yearWeather->cloudcov_daily[doy] = weathInput[cloudcovIndex];
        }

        if(!weath->use_windSpeedMonthly) {
            if(weath->has_sfcWind) {
                yearWeather->windspeed_daily[doy] = weathInput[windSpeedIndex];

            } else if(weath->has_windComp) {
                yearWeather->windspeed_daily[doy] = sqrt(squared(weathInput[windComp1Index]) +
                                                    squared(weathInput[windComp2Index]));

            }
        }

        if(!weath->use_relHumidityMonthly) {
            if(weath->has_hurs) {
                yearWeather->r_humidity_daily[doy] = weathInput[relHumIndex];

            } else if(weath->has_vp) {
                yearWeather->r_humidity_daily[doy] = weathInput[relHumIndex]; // Temporary

            } else if(weath->has_hurs2) {
                yearWeather->r_humidity_daily[doy] = (weathInput[hursComp1Index] +
                                                      weathInput[hursComp2Index]) / 2;

            } else if(weath->has_huss) {
                es = (6.112 * exp(17.67 * averageTemp)) / (averageTemp + 243.5);

                e = (weathInput[hussIndex] * 1013.25) /
                    (.378 * weathInput[hussIndex] + .622);

                relHum = e / es;
                relHum = max(0., relHum);

                yearWeather->r_humidity_daily[doy] = min(1., relHum);

            }
        }

        if(!weath->has_vp) {
            if(weath->has_tdps) {
                yearWeather->actualVaporPressure[doy] =
                                actualVaporPressure3(weathInput[dewPointIndex]);

            } else if(!weath->use_relHumidityMonthly && !weath->has_hurs) {

                yearWeather->actualVaporPressure[doy] = weathInput[dewPointIndex]; // Temporary
            } else if(weath->has_hurs2 && weath->has_temp2) {
                yearWeather->actualVaporPressure[doy] =
                                actualVaporPressure2(weathInput[hursComp2Index],
                                                     weathInput[hursComp1Index],
                                                     weathInput[maxTempIndex],
                                                     weathInput[minTempIndex]);

            } else {
                yearWeather->actualVaporPressure[doy] =
                                actualVaporPressure1(yearWeather->r_humidity_daily[doy],
                                                     averageTemp);

            }
        } else {
            yearWeather->actualVaporPressure[doy] = weathInput[vaporPressIndex];
        }


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
}

/**
 @brief Calculate actual vapor pressure based on relative humidity and mean temperature

 Calculation is based on Allen et al. 2006 @cite allen2006AaFM which is
 based on Majumdar et al. 1972 and updated by ASCE-EWRI 2005.

 @param hurs Daily mean relative humidity [%]
 @param tmean Daily mean air temperature [C]

 @return Calculated actual vapor pressure [kPa]
 */
double actualVaporPressure1(double hurs, double tmean) {
    // Allen et al. 2005 eqs 7 and 14
    return (hurs / 100.) * 0.6108 * exp((17.27 * tmean) / (tmean + 237.3));
}

/**
 @brief Calculate actual vapor pressure based on temperature and relative humidity components (min/max)

 Calculation is based on Allen et al. 2006 @cite allen2006AaFM which is
 based on Majumdar et al. 1972 and updated by ASCE-EWRI 2005.

 @param hursMax Daily maximum relative humidity [%]
 @param hursMin Daily minimum relative humidity [%]
 @param maxTemp Daily minimum air temperature [C]
 @param minTemp Daily maximum air temperature [C]

 @return Calculated actual vapor pressure [kPa]
 */
double actualVaporPressure2(double hursMax, double hursMin, double maxTemp, double minTemp) {
    // Allen et al. 2005 eqs 7 and 11
    double satVapPressureMax = .6108 * exp((17.27 * maxTemp) / maxTemp + 237.3);
    double satVapPressureMin = .6108 * exp((17.27 * minTemp) / minTemp + 237.3);

    double relHumVapPressMax = satVapPressureMin * (hursMax / 100);
    double relHumVapPressMin = satVapPressureMax * (hursMin / 100);

    return (relHumVapPressMax + relHumVapPressMin) / 2;
}

/**
 @brief Calculate actual vapor pressure based on dew point temperature

 Calculation is based on Allen et al. 2006 @cite allen2006AaFM which is
 based on Majumdar et al. 1972 and updated by ASCE-EWRI 2005.

 @param tdps 2m dew point temperature [C]

 @return Calculated actual vapor pressure [kPa]
 */
double actualVaporPressure3(double tdps) {
    // Allen et al. 2005 eqs 7 and 8
    return .6108 * exp((17.27 * tdps) / (tdps + 237.3));
}

void allocateClimateStructs(int numYears, SW_CLIMATE_YEARLY *climateOutput,
                            SW_CLIMATE_CLIM *climateAverages) {

    int month;

    climateOutput->PPTMon_cm = (double **)malloc(sizeof(double *) * MAX_MONTHS);
    climateOutput->meanTempMon_C = (double **)malloc(sizeof(double *) * MAX_MONTHS);
    climateOutput->maxTempMon_C = (double **)malloc(sizeof(double *) * MAX_MONTHS);
    climateOutput->minTempMon_C = (double **)malloc(sizeof(double *) * MAX_MONTHS);

    for(month = 0; month < MAX_MONTHS; month++) {
        climateOutput->PPTMon_cm[month] = (double *)malloc(sizeof(double) * numYears);
        climateOutput->meanTempMon_C[month] = (double *)malloc(sizeof(double) * numYears);
        climateOutput->maxTempMon_C[month] = (double *)malloc(sizeof(double) * numYears);
        climateOutput->minTempMon_C[month] = (double *)malloc(sizeof(double) * numYears);
    }

    climateOutput->PPT_cm = (double *)malloc(sizeof(double) * numYears);
    climateOutput->PPT7thMon_mm = (double *)malloc(sizeof(double) * numYears);
    climateOutput->meanTemp_C = (double *)malloc(sizeof(double) * numYears);
    climateOutput->meanTempDriestQtr_C = (double *)malloc(sizeof(double) * numYears);
    climateOutput->minTemp2ndMon_C = (double *)malloc(sizeof(double) * numYears);
    climateOutput->minTemp7thMon_C = (double *)malloc(sizeof(double) * numYears);
    climateOutput->frostFree_days = (double *)malloc(sizeof(double) * numYears);
    climateOutput->ddAbove65F_degday = (double *)malloc(sizeof(double) * numYears);
    climateAverages->meanTempMon_C = (double *)malloc(sizeof(double) * MAX_MONTHS);
    climateAverages->maxTempMon_C = (double *)malloc(sizeof(double) * MAX_MONTHS);
    climateAverages->minTempMon_C = (double *)malloc(sizeof(double) * MAX_MONTHS);
    climateAverages->PPTMon_cm = (double *)malloc(sizeof(double) * MAX_MONTHS);
    climateAverages->sdC4 = (double *)malloc(sizeof(double) * 3);
    climateAverages->sdCheatgrass = (double *)malloc(sizeof(double) * 3);
}

void deallocateClimateStructs(SW_CLIMATE_YEARLY *climateOutput,
                            SW_CLIMATE_CLIM *climateAverages) {

    int month;

    free(climateOutput->PPT_cm);
    free(climateOutput->PPT7thMon_mm);
    free(climateOutput->meanTemp_C);
    free(climateOutput->meanTempDriestQtr_C);
    free(climateOutput->minTemp2ndMon_C);
    free(climateOutput->minTemp7thMon_C);
    free(climateOutput->frostFree_days);
    free(climateOutput->ddAbove65F_degday);
    free(climateAverages->meanTempMon_C);
    free(climateAverages->maxTempMon_C);
    free(climateAverages->minTempMon_C);
    free(climateAverages->PPTMon_cm);
    free(climateAverages->sdC4);
    free(climateAverages->sdCheatgrass);

    for(month = 0; month < MAX_MONTHS; month++) {
        free(climateOutput->PPTMon_cm[month]);
        free(climateOutput->meanTempMon_C[month]);
        free(climateOutput->maxTempMon_C[month]);
        free(climateOutput->minTempMon_C[month]);
    }

    free(climateOutput->PPTMon_cm);
    free(climateOutput->meanTempMon_C);
    free(climateOutput->maxTempMon_C);
    free(climateOutput->minTempMon_C);
}

#ifdef DEBUG_MEM
#include "include/myMemory.h"
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