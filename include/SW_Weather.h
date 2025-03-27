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

 01/04/2011	(drs) added variable 'snowloss' to SW_WEATHER_SIM and to
 SW_WEATHER_OUTPUTS

 02/16/2011	(drs) added variable 'pct_runoff' to SW_WEATHER as input to
 weathsetup.in

 02/19/2011	(drs) added variable 'runoff' to SW_WEATHER_SIM and to
 SW_WEATHER_OUTPUTS moved soil_inf from SW_Soilwat to SW_Weather (added to
 SW_WEATHER and to SW_WEATHER_OUTPUTS)

 06/01/2012  (DLM) added temp_year_avg variable to SW_WEATHER_HIST struct &
 temp_month_avg[MAX_MONTHS] variable

 11/30/2012	(clk) added variable 'surfaceRunoff' to SW_WEATHER and
 SW_WEATHER_OUTPUTS changed 'runoff' to 'snowRunoff' to better distinguish
 between surface runoff and snowmelt runoff

 */
/********************************************************/
/********************************************************/

#ifndef SW_WEATHER_H
#define SW_WEATHER_H

#include "include/generic.h"        // for Bool
#include "include/SW_datastructs.h" // for SW_WEATHER, SW_SKY_INPUTS, SW_MODEL, LOG_...
#include "include/SW_Defines.h" // for TimeInt

#ifdef __cplusplus
extern "C" {
#endif


/*  all temps are in degrees C, all precip is in cm */
/*  in fact, all water variables are in cm throughout
 *  the model.  this facilitates additions and removals
 *  as they're always in the right units.
 */

/* =================================================== */
/*             Global Function Declarations            */
/* --------------------------------------------------- */
void SW_WTH_setup(
    SW_WEATHER_INPUTS *SW_WeatherIn,
    char *txtInFiles[],
    char *txtWeatherPrefix,
    LOG_INFO *LogInfo
);

void SW_WTH_read(
    SW_WEATHER_INPUTS *SW_WeatherIn,
    SW_WEATHER_HIST **allHist,
    SW_SKY_INPUTS *SW_SkyIn,
    SW_MODEL_INPUTS *SW_ModelIn,
    double elevation,
    Bool readTextInputs,
    TimeInt cum_monthdays[],
    TimeInt days_in_month[],
    LOG_INFO *LogInfo
);

void set_dailyInputIndices(
    const Bool dailyInputFlags[MAX_INPUT_COLUMNS],
    unsigned int dailyInputIndices[MAX_INPUT_COLUMNS],
    unsigned int *n_input_forcings
);

void check_and_update_dailyInputFlags(
    Bool use_cloudCoverMonthly,
    Bool use_humidityMonthly,
    Bool use_windSpeedMonthly,
    Bool *dailyInputFlags,
    LOG_INFO *LogInfo
);

void averageClimateAcrossYears(
    SW_CLIMATE_YEARLY *climateOutput,
    unsigned int numYears,
    SW_CLIMATE_CLIM *climateAverages
);

void calcSiteClimate(
    SW_WEATHER_HIST *allHist,
    TimeInt cum_monthdays[],
    TimeInt days_in_month[],
    unsigned int numYears,
    unsigned int startYear,
    Bool inNorthHem,
    SW_CLIMATE_YEARLY *climateOutput
);

void calcSiteClimateLatInvariants(
    SW_WEATHER_HIST *allHist,
    TimeInt cum_monthdays[],
    TimeInt days_in_month[],
    unsigned int numYears,
    unsigned int startYear,
    SW_CLIMATE_YEARLY *climateOutput
);

void findDriestQtr(
    unsigned int numYears,
    Bool inNorthHem,
    double *meanTempDriestQtr_C,
    double **meanTempMon_C,
    double **PPTMon_cm
);

void driestQtrSouthAdjMonYears(
    unsigned int month,
    unsigned int *adjustedYearZero,
    unsigned int *adjustedYearOne,
    unsigned int *adjustedYearTwo,
    unsigned int *adjustedMonth,
    unsigned int *prevMonth,
    unsigned int *nextMonth
);

void initializeClimatePtrs(
    SW_CLIMATE_YEARLY *climateOutput, SW_CLIMATE_CLIM *climateAverages
);

void initializeMonthlyClimatePtrs(SW_CLIMATE_YEARLY *climateOutput);

void allocateClimateStructs(
    unsigned int numYears,
    SW_CLIMATE_YEARLY *climateOutput,
    SW_CLIMATE_CLIM *climateAverages,
    LOG_INFO *LogInfo
);

void deallocateClimateStructs(
    SW_CLIMATE_YEARLY *climateOutput, SW_CLIMATE_CLIM *climateAverages
);

void read_weather_hist(
    TimeInt year,
    double **yearWeather,
    char txtWeatherPrefix[],
    unsigned int n_input_forcings,
    const unsigned int *dailyInputIndices,
    const Bool *dailyInputFlags,
    LOG_INFO *LogInfo
);

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
);

void SW_WTH_setWeatherValues(
    TimeInt startYear,
    TimeInt nYears,
    const Bool *inputFlags,
    double ***tempWeather,
    double elevation,
    TimeInt doyOffset,
    SW_WEATHER_HIST *yearlyWeather,
    LOG_INFO *LogInfo
);

void allocate_temp_weather(
    TimeInt nYears,
    int extraStorMult,
    double ****fullWeathHist,
    LOG_INFO *LogInfo
);

void deallocate_temp_weather(TimeInt nYears, double ****fullWeathHist);

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
);

void finalizeAllWeather(
    SW_MARKOV_INPUTS *SW_MarkovIn,
    SW_WEATHER_INPUTS *w,
    SW_WEATHER_HIST *allHist,
    TimeInt cum_monthdays[],
    TimeInt days_in_month[],
    LOG_INFO *LogInfo
);

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
);

void generateMissingWeather(
    SW_MARKOV_INPUTS *SW_MarkovIn,
    SW_WEATHER_HIST *allHist,
    unsigned int startYear,
    unsigned int n_years,
    unsigned int method,
    unsigned int optLOCF_nMax,
    LOG_INFO *LogInfo
);

void checkAllWeather(
    SW_WEATHER_INPUTS *weather, SW_WEATHER_HIST *weathHist, LOG_INFO *LogInfo
);

void SW_WTH_allocateAllWeather(
    SW_WEATHER_HIST **allHist, unsigned int n_years, LOG_INFO *LogInfo
);

void initializeAllWeatherPtrs(SW_WEATHER_HIST **allHist, unsigned int n_years);

void deallocateAllWeather(SW_WEATHER_HIST **allHist);

void clear_hist_weather(
    int extraStorMult, SW_WEATHER_HIST *yearWeather, double **fullWeathHist
);

void SW_WTH_finalize_all_weather(
    SW_MARKOV_INPUTS *SW_MarkovIn,
    SW_WEATHER_INPUTS *SW_WeatherIn,
    SW_WEATHER_HIST *allHist,
    TimeInt cum_monthdays[],
    TimeInt days_in_month[],
    LOG_INFO *LogInfo
);

void SW_WTH_init_run(SW_WEATHER_SIM *SW_WeatherSim);

void SW_WTH_construct(
    SW_WEATHER_INPUTS *SW_WeatherIn,
    SW_WEATHER_SIM *SW_WeatherSim,
    SW_WEATHER_OUTPUTS weath_p_accu[],
    SW_WEATHER_OUTPUTS weath_p_oagg[]
);

void SW_WTH_init_ptrs(SW_WEATHER_HIST **allHist);

void SW_WTH_deconstruct(SW_WEATHER_HIST **allHist);

void SW_WTH_new_day(
    SW_WEATHER_INPUTS *SW_WeatherIn,
    SW_WEATHER_SIM *SW_WeatherSim,
    SW_WEATHER_HIST *allHist,
    SW_SITE_INPUTS *SW_SiteIn,
    double snowpack[],
    TimeInt doy,
    TimeInt year,
    LOG_INFO *LogInfo
);

void SW_WTH_sum_today(void);


#ifdef __cplusplus
}
#endif

#endif
