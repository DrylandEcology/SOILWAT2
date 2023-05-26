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
 01/04/2011	(drs) added variable 'snowloss' to SW_WEATHER_NOW and to SW_WEATHER_OUTPUTS
 02/16/2011	(drs) added variable 'pct_runoff' to SW_WEATHER as input to weathsetup.in
 02/19/2011	(drs) added variable 'runoff' to SW_WEATHER_NOW and to SW_WEATHER_OUTPUTS
 moved soil_inf from SW_Soilwat to SW_Weather (added to SW_WEATHER and to SW_WEATHER_OUTPUTS)
 06/01/2012  (DLM) added temp_year_avg variable to SW_WEATHER_HIST struct & temp_month_avg[MAX_MONTHS] variable
 11/30/2012	(clk) added variable 'surfaceRunoff' to SW_WEATHER and SW_WEATHER_OUTPUTS
 changed 'runoff' to 'snowRunoff' to better distinguish between surface runoff and snowmelt runoff

 */
/********************************************************/
/********************************************************/

#ifndef SW_WEATHER_H
#define SW_WEATHER_H

#include "include/SW_Times.h"
#include "include/SW_Defines.h"

#ifdef __cplusplus
extern "C" {
#endif



/*  all temps are in degrees C, all precip is in cm */
/*  in fact, all water variables are in cm throughout
 *  the model.  this facilitates additions and removals
 *  as they're always in the right units.
 */

typedef struct {
	/* Weather values of the current simulation day */
	RealD temp_avg, temp_max, temp_min, ppt, rain, cloudCover, windSpeed, relHumidity,
    shortWaveRad, actualVaporPressure;
} SW_WEATHER_NOW;

typedef struct {
	/* Daily weather values for one year */
	RealD temp_max[MAX_DAYS], temp_min[MAX_DAYS], temp_avg[MAX_DAYS], ppt[MAX_DAYS],
    cloudcov_daily[MAX_DAYS], windspeed_daily[MAX_DAYS], r_humidity_daily[MAX_DAYS],
    shortWaveRad[MAX_DAYS], actualVaporPressure[MAX_DAYS];
	// RealD temp_month_avg[MAX_MONTHS], temp_year_avg; // currently not used
} SW_WEATHER_HIST;

/* accumulators for output values hold only the */
/* current period's values (eg, weekly or monthly) */
typedef struct {
	RealD temp_max, temp_min, temp_avg, ppt, rain, snow, snowmelt, snowloss, /* 20091015 (drs) ppt is divided into rain and snow */
	snowRunoff, surfaceRunoff, surfaceRunon, soil_inf, et, aet, pet, surfaceAvg, surfaceMax, surfaceMin;
} SW_WEATHER_OUTPUTS;

/**
 @brief Annual time-series of climate variables

 Output of the function `calcSiteClimate()`

 @note 2D array dimensions represent month (1st D) and year (2nd D); 1D array dimension represents year.
 @note Number of years is variable and determined at runtime.
 */
typedef struct {
    RealD **PPTMon_cm,                      /**< 2D array containing monthly amount precipitation [cm]*/
    *PPT_cm,                                /**< Array containing annual precipitation amount [cm]*/
    *PPT7thMon_mm,                          /**< Array containing July precipitation amount in July (northern hemisphere)
                                               or January (southern hemisphere) [mm]*/

    **meanTempMon_C,                        /**< 2D array containing monthly mean average daily air temperature [C]*/
    **maxTempMon_C,                         /**< 2D array containing monthly mean max daily air temperature [C]*/
    **minTempMon_C,                         /**< 2D array containing monthly mean min daily air temperature [C]*/
    *meanTemp_C,                            /**< Array containing annual mean temperatures [C]*/
    *meanTempDriestQtr_C,                   /**< Array containing the average temperatureof the driest quarter of the year [C]*/
    *minTemp2ndMon_C,                       /**< Array containing the mean daily minimum temperature in August (southern hemisphere)
                                             or February (northern hemisphere) [C]*/
    *minTemp7thMon_C,                       /**< Array containing minimum July temperatures in July (northern hisphere)
                                             or Janurary (southern hemisphere) [C]*/

    *frostFree_days,                        /**< Array containing the maximum consecutive days without frost [days]*/
    *ddAbove65F_degday;                     /**< Array containing the amount of degree days [C x day] above 65 F*/
} SW_CLIMATE_YEARLY;

/**
 @brief A structure holding all variables that are output to the function `averageClimateAcrossYears()` #SW_CLIMATE_YEARLY

 @note Values are across-year averages of #SW_CLIMATE_YEARLY and 1D array dimension represents month.
 The exceptions are `sdC4` and `sdCheatgrass` which represent across-year standard devations and the 1D array dimension
 represents different variables, see `averageClimateAcrossYears()`.
 */
typedef struct {
    RealD *meanTempMon_C,                   /**< Array of size MAX_MONTHS containing sum of monthly mean temperatures [C]*/
    *maxTempMon_C,                          /**< Array of size MAX_MONTHS containing sum of monthly maximum temperatures [C]*/
    *minTempMon_C,                          /**< Array of size MAX_MONTHS containing sum of monthly minimum temperatures [C]*/
    *PPTMon_cm,                             /**< Array of size MAX_MONTHS containing sum of monthly mean precipitation [cm]*/
    *sdC4,                                  /**< Array of size three holding the standard deviations of: 0) minimum July (northern hisphere)
                                             or Janurary (southern hemisphere) temperature [C],
                                             1) frost free days [days], 2) number of days above 65F [C x day]*/

    *sdCheatgrass,                          /**< Array of size three holding: 0) the standard deviations of July (northern hisphere)
                                             or Janurary (southern hemisphere) [cm],
                                             1) mean temperature of dry quarter [C], 2) mean minimum temperature of February
                                             (northern hemisphere) or August (southern hemisphere) [C]*/
    meanTemp_C,                             /**< Value containing the average of yearly temperatures [C]*/
    PPT_cm,                                 /**< Value containing the average of yearly precipitation [cm]*/
    PPT7thMon_mm,                           /**< Value containing average precipitation in July (northern hemisphere)
                                              or January (southern hemisphere) [mm]*/
    meanTempDriestQtr_C,                    /**< Value containing average of mean temperatures in the driest quarters of years [C]*/
    minTemp2ndMon_C,                        /**< Value containing average of minimum temperatures in August (southern hemisphere) or
                                             February (northern hemisphere) [C]*/
    ddAbove65F_degday,                      /**< Value containing average of total degrees above 65F (18.33C) throughout the year [C x day]*/
    frostFree_days,                         /**< Value containing average of most consectutive days in a year without frost [days]*/
    minTemp7thMon_C;                        /**< Value containing the average of lowest temperature in July (northern hisphere)
                                             or Janurary (southern hemisphere) [C]*/
} SW_CLIMATE_CLIM;

typedef struct {
    RealD **meanMonthlyTemp_C, **maxMonthlyTemp_C, **minMonthlyTemp_C, **monthlyPPT_cm,
    *annualPPT_cm, *meanAnnualTemp_C, *JulyMinTemp, *frostFreeDays_days, *ddAbove65F_degday,
    *JulyPPT_mm, *meanTempDriestQuarter_C, *minTempFebruary_C;
} SW_CLIMATE_CALC;

typedef struct {
    RealD *meanMonthlyTempAnn, *maxMonthlyTempAnn, *minMonthlyTempAnn, *meanMonthlyPPTAnn,
    *sdC4, *sdCheatgrass, MAT_C, MAP_cm, JulyPPTAnn_mm, meanTempDriestQuarterAnn_C, minTempFebruaryAnn_C,
    ddAbove65F_degdayAnn, frostFreeAnn, JulyMinTempAnn;
} SW_CLIMATE_AVERAGES;

typedef struct {

	Bool
		use_snow,
		use_weathergenerator_only;
			// swTRUE: use weather generator and ignore weather inputs

	unsigned int
		generateWeatherMethod;
			// see `generateMissingWeather()`
			// 0 : pass through missing values
			// 1 : LOCF (temp) + 0 (ppt)
			// 2 : weather generator (previously, `use_weathergenerator`)

	int rng_seed; // initial state for `mark

	RealD pct_snowdrift, pct_snowRunoff;
  RealD
    scale_precip[MAX_MONTHS],
    scale_temp_max[MAX_MONTHS],
    scale_temp_min[MAX_MONTHS],
    scale_skyCover[MAX_MONTHS],
    scale_wind[MAX_MONTHS],
    scale_rH[MAX_MONTHS],
    scale_actVapPress[MAX_MONTHS],
    scale_shortWaveRad[MAX_MONTHS];
	char name_prefix[MAX_FILENAMESIZE - 5]; // subtract 4-digit 'year' file type extension
	RealD snowRunoff, surfaceRunoff, surfaceRunon, soil_inf, surfaceAvg;
	RealD snow, snowmelt, snowloss, surfaceMax, surfaceMin;

  Bool use_cloudCoverMonthly, use_windSpeedMonthly, use_humidityMonthly;
  Bool dailyInputFlags[MAX_INPUT_COLUMNS];

  unsigned int dailyInputIndices[MAX_INPUT_COLUMNS],
 			   n_input_forcings, // Number of input columns found in weath.YYYY
    desc_rsds; /**< Description of units and definition of daily inputs of observed shortwave radiation, see `solar_radiation()` */

	/* This section is required for computing the output quantities.  */
	SW_WEATHER_OUTPUTS
		*p_accu[SW_OUTNPERIODS], // output accumulator: summed values for each time period
		*p_oagg[SW_OUTNPERIODS]; // output aggregator: mean or sum for each time periods


  /* Daily weather record */
  SW_WEATHER_HIST **allHist; /**< Daily weather values; array of length `n_years` of pointers to struct #SW_WEATHER_HIST where the first represents values for calendar year `startYear` */
  unsigned int n_years; /**< Length of `allHist`, i.e., number of years of daily weather */
  unsigned int startYear; /**< Calendar year corresponding to first year of `allHist` */

  SW_WEATHER_NOW now; /**< Weather values of the current simulation day */

} SW_WEATHER;



/* =================================================== */
/*            Externed Global Variables                */
/* --------------------------------------------------- */
extern SW_WEATHER SW_Weather;


/* =================================================== */
/*             Global Function Declarations            */
/* --------------------------------------------------- */
void SW_WTH_setup(void);
void check_and_update_dailyInputFlags(
  Bool use_cloudCoverMonthly,
  Bool use_humidityMonthly,
  Bool use_windSpeedMonthly,
  Bool *dailyInputFlags
);
void SW_WTH_read(void);
void averageClimateAcrossYears(SW_CLIMATE_YEARLY *climateOutput, int numYears,
                               SW_CLIMATE_CLIM *climateAverages);
void calcSiteClimate(SW_WEATHER_HIST **allHist, int numYears, int startYear,
                     Bool inNorthHem, SW_CLIMATE_YEARLY *climateOutput);
void calcSiteClimateLatInvariants(SW_WEATHER_HIST **allHist, int numYears, int startYear,
                         SW_CLIMATE_YEARLY *climateOutput);
void findDriestQtr(int numYears, Bool inNorthHem, double *meanTempDriestQtr_C,
                   double **meanTempMon_C, double **PPTMon_cm);
void driestQtrSouthAdjMonYears(int month, int *adjustedYearZero, int *adjustedYearOne,
                           int *adjustedYearTwo, int *adjustedMonth, int *prevMonth,
                           int *nextMonth);
void allocateClimateStructs(int numYears, SW_CLIMATE_YEARLY *climateOutput,
                            SW_CLIMATE_CLIM *climateAverages);
void deallocateClimateStructs(SW_CLIMATE_YEARLY *climateOutput,
                              SW_CLIMATE_CLIM *climateAverages);
void _read_weather_hist(
  TimeInt year,
  SW_WEATHER_HIST *yearWeather,
  char weather_prefix[],
  unsigned int n_input_forcings,
  unsigned int *dailyInputIndices,
  Bool *dailyInputFlags
);
void readAllWeather(
  SW_WEATHER_HIST **allHist,
  int startYear,
  unsigned int n_years,
  Bool use_weathergenerator_only,
  char weather_prefix[],
  Bool use_cloudCoverMonthly,
  Bool use_humidityMonthly,
  Bool use_windSpeedMonthly,
  unsigned int n_input_forcings,
  unsigned int *dailyInputIndices,
  Bool *dailyInputFlags,
  RealD *cloudcov,
  RealD *windspeed,
  RealD *r_humidity
);
void finalizeAllWeather(SW_WEATHER *w);

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
);
void generateMissingWeather(
  SW_WEATHER_HIST **allHist,
  int startYear,
  unsigned int n_years,
  unsigned int method,
  unsigned int optLOCF_nMax
);
void checkAllWeather(SW_WEATHER *weather);
void allocateAllWeather(SW_WEATHER *w);
void deallocateAllWeather(SW_WEATHER *w);
void _clear_hist_weather(SW_WEATHER_HIST *yearWeather);
void SW_WTH_finalize_all_weather(void);
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
