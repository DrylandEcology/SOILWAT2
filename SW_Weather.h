/********************************************************/
/********************************************************/
/*  Source file: SW_Weather.h
 Type: header
 Application: SOILWAT - soilwater dynamics simulator
 Purpose: Support definitions/declarations for
 weather-related information.
 History:
 (8/28/01) -- INITIAL CODING - cwb
 20091014 (drs) added pct_snowdrift as input to weathsetup.in
 20091015 (drs) ppt is divided into rain and snow, added snowmelt
 01/04/2011	(drs) added variable 'snowloss' to SW_WEATHER_2DAYS and to SW_WEATHER_OUTPUTS
 02/16/2011	(drs) added variable 'pct_runoff' to SW_WEATHER as input to weathsetup.in
 02/19/2011	(drs) added variable 'runoff' to SW_WEATHER_2DAYS and to SW_WEATHER_OUTPUTS
 moved soil_inf from SW_Soilwat to SW_Weather (added to SW_WEATHER and to SW_WEATHER_OUTPUTS)
 06/01/2012  (DLM) added temp_year_avg variable to SW_WEATHER_HIST struct & temp_month_avg[MAX_MONTHS] variable
 11/30/2012	(clk) added variable 'surfaceRunoff' to SW_WEATHER and SW_WEATHER_OUTPUTS
 changed 'runoff' to 'snowRunoff' to better distinguish between surface runoff and snowmelt runoff

 */
/********************************************************/
/********************************************************/

#ifndef SW_WEATHER_H
#define SW_WEATHER_H

#include "SW_Times.h"
#include "SW_Defines.h"

#ifdef __cplusplus
extern "C" {
#endif



/*  all temps are in degrees C, all precip is in cm */
/*  in fact, all water variables are in cm throughout
 *  the model.  this facilitates additions and removals
 *  as they're always in the right units.
 */

typedef struct {
	/* comes from markov weather day-to-day */
	RealD temp_avg, temp_max, temp_min, ppt, rain;
} SW_WEATHER_2DAYS;

typedef struct {
	/* comes from historical weather files */
	RealD temp_max[MAX_DAYS], temp_min[MAX_DAYS], temp_avg[MAX_DAYS], ppt[MAX_DAYS];
	// RealD temp_month_avg[MAX_MONTHS], temp_year_avg; // currently not used
} SW_WEATHER_HIST;

/* accumulators for output values hold only the */
/* current period's values (eg, weekly or monthly) */
typedef struct {
	RealD temp_max, temp_min, temp_avg, ppt, rain, snow, snowmelt, snowloss, /* 20091015 (drs) ppt is divided into rain and snow */
	snowRunoff, surfaceRunoff, surfaceRunon, soil_inf, et, aet, pet, surfaceAvg, surfaceMax, surfaceMin;
} SW_WEATHER_OUTPUTS;

/**
 A structure holding all variables that are output to the function `calcSiteClimate()` and is input to the function `averageClimateAcrossYears()`
 
 @note Arrays are to be of size `numYears` which is variable (31 years when testing)
 @note Arrays hold values for all simulation years
 */
typedef struct {
    RealD **meanMonthlyTemp_C,              /**< 2D array containing monthly mean average daily air temperature (deg;C)*/
    **maxMonthlyTemp_C,                     /**< 2D array containing monthly mean max daily air temperature (deg;C)*/
    **minMonthlyTemp_C,                     /**< 2D array containing monthly mean min daily air temperature (deg;C)*/
    **monthlyPPT_cm,                        /**< 2D array containing monthly amount precipitation (cm)*/
    *annualPPT_cm,                          /**< Array containing annual precipitation amount [cm]*/
    *meanAnnualTemp_C,                      /**< Array containing annual mean temperatures [C]*/
    *JulyMinTemp,                           /**< Array containing minimum July temperatures [C] */
    *frostFreeDays_days,                    /**< Array containing the maximum consecutive days in a year without frost*/
    *ddAbove65F_degday,                     /**< Array containing the amount of degree days [C x day] above 65 F */
    *JulyPPT_mm,                            /**< Array containing July precipitation amount (mm) for all years */
    *meanTempDriestQuarter_C,               /**< Array containing the average temperature of the driest quarter of the year*/
    *minTempFebruary_C;                     /**< Array containing the mean minimum temperature in february*/
} SW_CLIMATE_CALC;

/**
 A structure holding all variables that are output to the function `averageClimateAcrossYears()`
 
 @note If a description mentions a size, it is not variable and has to be the specified size
 @note Averages are taken across all simulation years
 */
typedef struct {
    RealD *meanMonthlyTempAnn,              /**< Array of size MAX_MONTHS containing sum of monthly mean temperatures*/
    *maxMonthlyTempAnn,                     /**< Array of size MAX_MONTHS containing sum of monthly maximum temperatures*/
    *minMonthlyTempAnn,                     /**< Array of size MAX_MONTHS containing sum of monthly minimum temperatures*/
    *meanMonthlyPPTAnn,                     /**< Array of size MAX_MONTHS containing sum of monthly mean precipitation*/
    *sdC4,                                  /**< Array of size three holding the standard deviations of minimum July temperature (0),
                                             frost free days (1), number of days above 65F (2)*/
    *sdCheatgrass,                          /**< Array of size three holding the standard deviations of July precipitation (0), mean
                                              temperature of dry quarter (1), mean minimum temperature of February (2)*/
    MAT_C,                                  /**< Value containing the average of yearly temperatures*/
    MAP_cm,                                 /**< Value containing the average of yearly precipitation*/
    JulyPPTAnn_mm,                          /**< Value containing average of July precipitation (mm)*/
    meanTempDriestQuarterAnn_C,             /**< Value containing average of mean temperatures in the driest quarters of years*/
    minTempFebruaryAnn_C,                   /**< Value containing average of minimum temperatures in February*/
    ddAbove65F_degdayAnn,                   /**< Value containing average of total degrees above 65F (18.33C) throughout the year*/
    frostFreeAnn,                           /**< Value containing average of most consectutive days in a year without frost*/
    JulyMinTempAnn;                         /**< Value containing the average of lowest temperature in July*/
} SW_CLIMATE_AVERAGES;

typedef struct {

	Bool
		use_weathergenerator_only,
			// swTRUE: set use_weathergenerator = swTRUE and ignore weather inputs
		use_weathergenerator,
			// swTRUE: use weather generator for missing weather input (values/files)
			// swFALSE: fail if any weather input is missing (values/files)
		use_snow;
	RealD pct_snowdrift, pct_snowRunoff;
    unsigned int n_years;
	SW_TIMES yr;
  RealD
    scale_precip[MAX_MONTHS],
    scale_temp_max[MAX_MONTHS],
    scale_temp_min[MAX_MONTHS],
    scale_skyCover[MAX_MONTHS],
    scale_wind[MAX_MONTHS],
    scale_rH[MAX_MONTHS];
	char name_prefix[MAX_FILENAMESIZE - 5]; // subtract 4-digit 'year' file type extension
	RealD snowRunoff, surfaceRunoff, surfaceRunon, soil_inf, surfaceAvg;
	RealD snow, snowmelt, snowloss, surfaceMax, surfaceMin;

	/* This section is required for computing the output quantities.  */
	SW_WEATHER_OUTPUTS
		*p_accu[SW_OUTNPERIODS], // output accumulator: summed values for each time period
		*p_oagg[SW_OUTNPERIODS]; // output aggregator: mean or sum for each time periods
	SW_WEATHER_HIST hist;
    SW_WEATHER_HIST **allHist;
	SW_WEATHER_2DAYS now;

} SW_WEATHER;



/* =================================================== */
/*            Externed Global Variables                */
/* --------------------------------------------------- */
extern SW_WEATHER SW_Weather;


/* =================================================== */
/*             Global Function Declarations            */
/* --------------------------------------------------- */
void SW_WTH_setup(void);
void SW_WTH_read(void);
Bool _read_weather_hist(TimeInt year, SW_WEATHER_HIST *yearWeather);
void averageClimateAcrossYears(SW_CLIMATE_CALC *climateOutput, int numYears,
                               SW_CLIMATE_AVERAGES *climateAverages);
void calcSiteClimate(SW_WEATHER_HIST **allHist, int numYears, int startYear,
                     SW_CLIMATE_CALC *climateOutput);
void findDriestQtr(double *meanTempDriestQuarter_C, int numYears, double **meanMonthlyTemp_C,
                   double **meanMonthlyPPT_cm);
void readAllWeather(SW_WEATHER_HIST **allHist, int startYear, unsigned int n_years);
void deallocateAllWeather(void);
void _clear_hist_weather(SW_WEATHER_HIST *yearWeather);
void SW_WTH_init_run(void);
void SW_WTH_construct(void);
void SW_WTH_deconstruct(void);
void SW_WTH_new_day(void);
void SW_WTH_sum_today(void);


#ifdef DEBUG_MEM
void SW_WTH_SetMemoryRefs(void);
#endif


#ifdef __cplusplus
}
#endif

#endif
