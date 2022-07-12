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
 @param[in] meanMonthlyTemp 2D array containing all mean average monthly air temperature (deg;C) with dimensions
 of row (months) size MAX_MONTHS and columns (years) of size numYears
 @param[in] maxMonthlyTemp 2D array containing all mean max monthly air temperature (deg;C) with dimensions
 of row (months) size MAX_MONTHS and columns (years) of size numYears
 @param[in] minMonthlyTemp 2D array containing all mean min monthly air temperature (deg;C) with dimensions
 of row (months) size MAX_MONTHS and columns (years) of size numYears
 @param[out] meanMonthlyAvg Array containing sum of monthly mean temperatures
 @param[out] meanMonthlyMax Array containing sum of monthly maximum temperatures
 @param[out] meanMonthlyMin Array containing sum of monthly minimum temperatures
 @param[out] meanMonthlyPPT Array containing sum of monthly mean precipitation
 @param[out] MAT_C Value containing the sum of daily temperatures
 @param[out] MAP_cm Value containing the sum of daily precipitation
 @param[out] PPT_cm Array containing annual precipitation amount [cm]
 @param[out] Temp_C Array containing annual temperatures [C]
 @param[in] numYears Number of years we want to average across
 */

void averageClimateAcrossYears(double **meanMonthlyTemp, double **maxMonthlyTemp,
        double **minMonthlyTemp, double **meanMonthlyPPT, double *meanMonthlyTempAnn,
        double *maxMonthlyTempAnn, double *minMonthlyTempAnn, double *meanMonthlyPPTAnn,
        double *MAP_cm, double *MAT_C, double MMT_C[], double MMP_cm[], int numYears) {
    
    int month, numLeapYears = (numYears / 4) + 1;
    double numDaysInSimulation = (numYears * 365.) + numLeapYears;
    double avgDaysInSimulation = numDaysInSimulation / numYears;
    
    for(month = 0; month < MAX_MONTHS; month++) {
        meanMonthlyTempAnn[month] = mean(meanMonthlyTemp[month], numYears);
        maxMonthlyTempAnn[month] = mean(maxMonthlyTemp[month], numYears);
        minMonthlyTempAnn[month] = mean(minMonthlyTemp[month], numYears);
        meanMonthlyPPTAnn[month] = mean(meanMonthlyPPT[month], numYears);
    }
    
    *MAP_cm = mean(MMP_cm, numYears);
    *MAT_C = mean(MMT_C, numYears) / avgDaysInSimulation;
}

/**
 @brief Calculate climate variable from daily weather
 @param[in] all_hist Array containing all historical data of a site
 @param[out] meanMonthlyTemp 2D array containing all mean average monthly air temperature (deg;C) with
 dimensions of row (months) size MAX_MONTHS and columns (years) of size numYears
 @param[out] maxMonthlyTemp 2D array containing all mean max monthly air temperature (deg;C) with
 dimensions of row (months) size MAX_MONTHS and columns (years) of size numYears
 @param[out] minMonthlyTemp 2D array containing all mean min monthly air temperature (deg;C) with
 dimensions of row (months) size MAX_MONTHS and columns (years) of size numYears
 @param[out] meanMonthlyPPT 2D array containing all mean average monthly precipitation (cm) with dimensions
 of row (months) size MAX_MONTHS and columns (years) of size numYears
 @param[out] MMP_cm Array containing annual precipitation amount [cm]
 @param[out] MMT_C Array containing annual temperatures [C]
 @param[out] JulyMinTemp Array of size numYears holding minimum July temperatures for each year
 @param[out] frostFreeDays Array of size numYears holding the maximum consecutive days in a year without frost for every year
 @param[out] degreeAbove65 Array of size numYears holding the number of days in the year that is above 65F for every year
 @param[out] sdC4 Array of size three holding the standard deviations of minimum July temperature (0), frost free days (1), number of days above 65F (2)
 @param[out] PPTJuly Array of size numYears holding mean July precipitation (mm) for each year
 @param[out] meanTempDryQuarter Array of size numYears holding the average temperature of the driest quarter of the year for every year
 @param[out] minTempFebruary Array of size numYears holding the mean minimum temperature in february for every year
 @param[out] sdCheatgrass Array of size 3 holding the standard deviations of July precipitation (0), mean
 temperature of dry quarter (1), mean minimum temperature of February (2)
 @param[in] numYears Number of years covered in the simulation
 @param[in] startYear Start year of simulation
 
 */

void calcSiteClimate(SW_WEATHER_HIST **allHist, double **meanMonthlyTemp, double **maxMonthlyTemp,
    double **minMonthlyTemp, double **meanMonthlyPPT, double *MMP_cm, double *MMT_C,
    double *JulyMinTemp, int *frostFreeDays, double *degreeAbove65, double *sdC4,
    double *PPTJuly, double *meanTempDryQuarter, double *minTempFebruary,
    double *sdCheatgrass, int numYears, int startYear) {
    
    
    int month, yearIndex, year, day, numDaysYear, numDaysMonth, currMonDay,
    consecNonFrost, currentNonFrost, July = 6, February = 1;
    
    double currentTempMin, currentTempMean, totalAbove65, currentJulyMin, JulyPPT,
    prevFrostMean, frostMean = 0, frostSqr = 0;
    
    for(month = 0; month < MAX_MONTHS; month++) {
        memset(meanMonthlyTemp[month], 0., sizeof(double) * numYears);
        memset(maxMonthlyTemp[month], 0., sizeof(double) * numYears);
        memset(minMonthlyTemp[month], 0., sizeof(double) * numYears);
        memset(meanMonthlyPPT[month], 0., sizeof(double) * numYears);
    }
    
    for(yearIndex = 0; yearIndex < numYears; yearIndex++) {
        year = yearIndex + startYear;
        Time_new_year(year);
        numDaysYear = isleapyear(year) ? 366 : 365;
        month = 0;
        currMonDay = 0;
        numDaysMonth = 31;
        currentJulyMin = 999;
        totalAbove65 = 0;
        currentNonFrost = 0;
        consecNonFrost = 0;
        JulyPPT = 0;
        
        for(day = 0; day < numDaysYear; day++) {
            currMonDay++;
            meanMonthlyTemp[month][yearIndex] += allHist[yearIndex]->temp_avg[day];
            maxMonthlyTemp[month][yearIndex] += allHist[yearIndex]->temp_max[day];
            minMonthlyTemp[month][yearIndex] += allHist[yearIndex]->temp_min[day];
            meanMonthlyPPT[month][yearIndex] += allHist[yearIndex]->ppt[day];
            
            MMP_cm[yearIndex] += allHist[yearIndex]->ppt[day];
            MMT_C[yearIndex] += allHist[yearIndex]->temp_avg[day];

            currentTempMin = allHist[yearIndex]->temp_min[day];
            currentTempMean = allHist[yearIndex]->temp_avg[day];
            
            if(month == July){
                currentJulyMin = (currentTempMin < currentJulyMin) ?
                                            currentTempMin : currentJulyMin;
                JulyPPT += allHist[yearIndex]->ppt[day] * 10;
            }

            if(currentTempMin > 0.0) {
                currentNonFrost++;
            } else if(currentNonFrost > consecNonFrost){
                consecNonFrost = currentNonFrost;
                currentNonFrost = 0;
            } else {
                currentNonFrost = 0;
            }

            if(month == February) {
                minTempFebruary[yearIndex] += allHist[yearIndex]->temp_min[day];
            }

            if(currMonDay == numDaysMonth) {
                // Take the average of the current months values for current year
                meanMonthlyTemp[month][yearIndex] /= numDaysMonth;
                maxMonthlyTemp[month][yearIndex] /= numDaysMonth;
                minMonthlyTemp[month][yearIndex] /= numDaysMonth;
                meanMonthlyPPT[month][yearIndex] /= numDaysMonth;
                
                if(month == Feb) minTempFebruary[yearIndex] /= numDaysMonth;
                
                month++;
                if(month != Dec + 1) numDaysMonth = Time_days_in_month(month);
                currMonDay = 0;
            }
            
            currentTempMean -= 18.333;
            totalAbove65 += (currentTempMean > 0.0) ? currentTempMean : 0.;
            
        }
        
        JulyMinTemp[yearIndex] = currentJulyMin;
        PPTJuly[yearIndex] = JulyPPT;
        degreeAbove65[yearIndex] = totalAbove65;
        frostFreeDays[yearIndex] = consecNonFrost;
        
        prevFrostMean = frostMean;
        
        // Get the running values needed for running standard deviation for frostFreeDays
        frostMean = get_running_mean(yearIndex + 1, prevFrostMean, consecNonFrost);
        frostSqr += get_running_sqr(prevFrostMean, frostMean, consecNonFrost);
    }
    
    findDriestQtr(meanMonthlyTemp, meanMonthlyPPT, meanTempDryQuarter, numYears);
    
    // Calculate and set standard deviation of C4 variables (frostFreeDays is a running sd)
    sdC4[0] = standardDeviation(JulyMinTemp, numYears);
    sdC4[1] = final_running_sd(numYears, frostSqr);
    sdC4[2] = standardDeviation(degreeAbove65, numYears);

    // Calculate and set the standard deviation of cheatgrass variables
    sdCheatgrass[0] = standardDeviation(PPTJuly, numYears);
    sdCheatgrass[1] = standardDeviation(meanTempDryQuarter, numYears);
    sdCheatgrass[2] = standardDeviation(minTempFebruary, numYears);
}

/**
 @brief Helper function to calcsiteClimate to find the driest quarter of the year's average temperature
 
 @param[in] meanMonthlyTemp 2D array holding sum of monthly temperature for every month and year
 @param[in] meanMonthlyPPT 2D array holding sum of monthly precipitation for every month and year
 @param[out] meanTempDryQuarter 2D array holding sum of monthly temperature for every month and year
 @param[in] numYears Number of years in the simulation run
 @param[in] startYear Start year of the simulation
 */
void findDriestQtr(double **meanMonthlyTemp, double **meanMonthlyPPT, double *meanTempDryQuarter,
                   int numYears) {
    
    int yearIndex, month, prevMonth, nextMonth;
    
    double defaultVal = 999., driestThreeMonPPT, driestMeanTemp,
    currentQtrPPT, currentQtrTemp;
    
    for(yearIndex = 0; yearIndex < numYears; yearIndex++) {
        driestThreeMonPPT = defaultVal;
        driestMeanTemp = defaultVal;

        for(month = 0; month < MAX_MONTHS; month++) {

            prevMonth = (month == 0) ? 11 : month - 1;
            nextMonth = (month == 11) ? 0 : month + 1;
            
            currentQtrPPT = (meanMonthlyPPT[prevMonth % 12][yearIndex]) +
                            (meanMonthlyPPT[month % 12][yearIndex]) +
                            (meanMonthlyPPT[nextMonth % 12][yearIndex]);
            
            currentQtrTemp = (meanMonthlyTemp[prevMonth % 12][yearIndex]) +
                             (meanMonthlyTemp[month % 12][yearIndex]) +
                             (meanMonthlyTemp[nextMonth % 12][yearIndex]);
            
            if(currentQtrPPT < driestThreeMonPPT) {
                driestMeanTemp = currentQtrTemp;
                driestThreeMonPPT = currentQtrPPT;
            }
            
        }
        
        meanTempDryQuarter[yearIndex] = driestMeanTemp / 3;
        
    }
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
