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

/**
 @brief Takes averages through the number of years of the calculated values from calc_SiteClimate
 
 @param[in] climateOutput Structure of type SW_CLIMATE_OUTPUT that holds all output from `calcSiteClimate()`
 @param[in] numYears Calendar year corresponding to first year of `allHist`
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
 @brief Calculate monthly and annual time series of climate variables from daily weather

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
 @param[in] numYears Number of years represented by `allHist`
 @param[in] startYear Calendar year corresponding to first year of `allHist`
 @param[out] climateOutput Structure of type SW_CLIMATE_CLIM that holds averages and
 standard deviations output by `averageClimateAcrossYears()`
 */

void calcSiteClimate(SW_WEATHER_HIST **allHist, int numYears, int startYear,
                     SW_CLIMATE_YEARLY *climateOutput, Bool isNorth) {

    int month, yearIndex, year, day, numDaysYear, currMonDay;
    int numDaysMonth, adjustedDoy, adjustedYear = 0, secondMonth, seventhMonth,
    adjustedStartYear, calendarYearDays;
    
    double currentTempMin, currentTempMean, totalAbove65, currentJulyMin, JulyPPT,
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
    if(isNorth) {
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
        month = (isNorth) ? Jan : Jul;
        currMonDay = 0;
        currentJulyMin = SW_MISSING;
        totalAbove65 = 0;
        currentNonFrost = 0;
        consecNonFrost = 0;
        JulyPPT = 0;

        if(!isNorth) {
            // Get calendar year days only when site is in southern hemisphere
            // To deal with number of days in data
            calendarYearDays = Time_get_lastdoy_y(year - 1);
        }

        for(day = 0; day < numDaysYear; day++) {
            if(isNorth) {
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

            if(month == Jul && adjustedYear >= numYears - 1 && !isNorth) {
                // Set all current accumulated values to SW_MISSING to prevent
                // a zero going into the last year of the respective arrays
                // Last six months of data is ignored and do not go into
                // a new year of data for "adjusted years"
                JulyPPT = SW_MISSING;
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
                currentJulyMin = (currentTempMin < currentJulyMin) ?
                                            currentTempMin : currentJulyMin;
                JulyPPT += allHist[adjustedYear]->ppt[adjustedDoy] * 10;
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
        climateOutput->minTemp7thMon_C[yearIndex] = currentJulyMin;
        climateOutput->PPT7thMon_mm[yearIndex] = JulyPPT;
        climateOutput->ddAbove65F_degday[yearIndex] = totalAbove65;
        
        // The reason behind checking if consecNonFrost is greater than zero,
        // is that there is a chance all days in the year are above 32F
        climateOutput->frostFree_days[yearIndex] = (consecNonFrost > 0) ? consecNonFrost : currentNonFrost;
    }

    findDriestQtr(climateOutput->meanTempDriestQtr_C, numYears,
                  climateOutput->meanTempMon_C, climateOutput->PPTMon_cm, isNorth);
}

/**
 @brief Helper function to `calcSiteClimate()`. Manages all information that is independant of the site
 being in the northern/southern hemisphere.
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
 @brief Helper function to calcsiteClimate to find the driest quarter of the year's average temperature
 
 @param[in] numYears Number of years represented within simulation
 @param[in] startYear Calendar year corresponding to first year of simulation
 @param[out] meanTempMon_C 2D array containing monthly means average daily air temperature (deg;C) with
 dimensions of row (months) size MAX_MONTHS and columns (years) of size numYears
 @param[out] PPTMon_cm 2D array containing monthly amount precipitation (cm) with dimensions
 of row (months) size MAX_MONTHS and columns (years) of size numYears
 @param[out] meanTempDriestQtr_C Array of size numYears holding the average temperature of the driest quarter of the year for every year
 */
void findDriestQtr(double *meanTempDriestQtr_C, int numYears, double **meanTempMon_C,
                   double **PPTMon_cm, Bool isNorth) {
    
    int yearIndex, month, prevMonth, nextMonth, adjustedMonth = 0,
    numQuarterMonths = 3, endNumYears = (isNorth) ? numYears : numYears - 1;

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

            if(isNorth) {
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

    if(!isNorth) meanTempDriestQtr_C[numYears - 1] = SW_MISSING;

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

    // Adjust prevMonth, nextMonth and adjustedYear(s) with respect to the
    // respective adjustedMonth
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
}


/* =================================================== */
/*             Global Function Definitions             */
/* --------------------------------------------------- */



/**
 @brief Reads in all weather data through all years and stores them in global SW_Weather's `allHist`

 @param[out] allHist 2D array holding all weather data gathered
 @param[in] startYear Start year of the simulation
 @param[in] n_years Number of years in simulation

*/
void readAllWeather(SW_WEATHER_HIST **allHist, int startYear, unsigned int n_years) {

    int year;
    unsigned int yearIndex, numDaysYear, day;

    Bool weth_found = swFALSE;

    for(yearIndex = 0; yearIndex < n_years; yearIndex++) {

        if(SW_Weather.use_weathergenerator_only) {
          // Set to missing for call to `generateMissingWeather()`
          _clear_hist_weather(allHist[yearIndex]);

        } else {
            year = yearIndex + startYear;

            weth_found = _read_weather_hist(year, allHist[yearIndex]);

            // Calculate average air temperature
            if (weth_found) {
              Time_new_year(year);
              numDaysYear = Time_get_lastdoy_y(year);

              for (day = 0; day < numDaysYear; day++) {
                  if (
                    !missing(allHist[yearIndex]->temp_max[day]) &&
                    !missing(allHist[yearIndex]->temp_min[day])
                  ) {
                      allHist[yearIndex]->temp_avg[day] = (
                          allHist[yearIndex]->temp_max[day] +
                          allHist[yearIndex]->temp_min[day]
                        ) / 2.;
                  }
              }
            }
        }
    }
}


/**
  @brief Apply temperature and precipitation scaling to daily weather values

  @param[in,out] allHist 2D array holding all weather data
  @param[in] startYear Start year of the simulation (and `allHist`)
  @param[in] n_years Number of years in simulation (length of `allHist`)
  @param[in] scale_temp_max Array of monthly, additive scaling parameters to
    modify daily maximum air temperature [C]
  @param[in] scale_temp_min Array of monthly, additive scaling parameters to
    modify daily minimum air temperature [C]
  @param[in] scale_precip Array of monthly, multiplicative scaling parameters to
    modify daily precipitation [-]

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
  double *scale_precip
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
      Time_new_year(year);
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

  SOILWAT2 may be set up such that weather is generated exclusively
  (i.e., without an attempt to read data from files on disk):
    - Set the weather generator to exclusive use
  or
     1. Turn on the weather generator
     2. Set the "first year to begin historical weather" to a year after
        the last simulated year


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
    Time_new_year(year);
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

    deallocateAllWeather();
}


/**
  @brief Allocate memory for `allHist` of `SW_Weather` based on `n_years`
*/
void allocateAllWeather(void) {
  unsigned int year;

  SW_Weather.allHist = (SW_WEATHER_HIST **)malloc(sizeof(SW_WEATHER_HIST *) * SW_Weather.n_years);

  for (year = 0; year < SW_Weather.n_years; year++) {

      SW_Weather.allHist[year] = (SW_WEATHER_HIST *)malloc(sizeof(SW_WEATHER_HIST));
  }
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
        SW_Weather.allHist = NULL;
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
    if (
      missing(w->allHist[year]->temp_avg[day]) ||
      missing(w->allHist[year]->ppt[day])
    ) {
      LogError(
        logfp,
        LOGFATAL,
        "Missing weather data (day %u - %u) during simulation.",
        SW_Model.year,
        SW_Model.doy
      );
    }

    wn->temp_max = w->allHist[year]->temp_max[day];
    wn->temp_min = w->allHist[year]->temp_min[day];
    wn->ppt = w->allHist[year]->ppt[day];

    wn->temp_avg = w->allHist[year]->temp_avg[day];

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

	if (SW_Weather.generateWeatherMethod != 2 && SW_Model.startyr < w->yr.first) {
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

    // Deallocate (previous, if any) `allHist`
    // (using value of `SW_Weather.n_years` previously used to allocate)
    deallocateAllWeather();

    // Determine required (new) size of new `allHist`
    #ifdef STEPWAT
    SW_Weather.n_years = SuperGlobals.runModelYears;
    #else
    SW_Weather.n_years = SW_Model.endyr - SW_Model.startyr + 1;
    #endif

    // Allocate new `allHist` (based on current `SW_Weather.n_years`)
    allocateAllWeather();


    // Read daily meteorological input from disk
    readAllWeather(SW_Weather.allHist, SW_Model.startyr, SW_Weather.n_years);


    // Impute missing values
    generateMissingWeather(
      SW_Weather.allHist,
      SW_Model.startyr,
      SW_Weather.n_years,
      SW_Weather.generateWeatherMethod,
      3 // optLOCF_nMax (TODO: make this user input)
    );


    // Scale with monthly additive/multiplicative parameters
    scaleAllWeather(
      SW_Weather.allHist,
      SW_Model.startyr,
      SW_Weather.n_years,
      SW_Weather.scale_temp_max,
      SW_Weather.scale_temp_min,
      SW_Weather.scale_precip
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

void allocDeallocClimateStructs(int action, int numYears, SW_CLIMATE_YEARLY *climateOutput,
                                SW_CLIMATE_CLIM *climateAverages) {
    
    int deallocate = 0, month;
    
    if(action == deallocate) {
        
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
    } else {
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
