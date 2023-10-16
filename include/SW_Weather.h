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

#include "include/SW_datastructs.h"

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
void SW_WTH_setup(SW_WEATHER* SW_Weather, char *InFiles[],
                  char *_weather_prefix, LOG_INFO* LogInfo);
void SW_WTH_read(SW_WEATHER* SW_Weather, SW_SKY* SW_Sky, SW_MODEL* SW_Model,
                 LOG_INFO* LogInfo);
void set_dailyInputIndices(
  Bool dailyInputFlags[MAX_INPUT_COLUMNS],
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
void averageClimateAcrossYears(SW_CLIMATE_YEARLY *climateOutput, int numYears,
                               SW_CLIMATE_CLIM *climateAverages);
void calcSiteClimate(SW_WEATHER_HIST **allHist, TimeInt cum_monthdays[],
                     TimeInt days_in_month[], int numYears, int startYear,
                     Bool inNorthHem, SW_CLIMATE_YEARLY *climateOutput);
void calcSiteClimateLatInvariants(SW_WEATHER_HIST **allHist,
    TimeInt cum_monthdays[], TimeInt days_in_month[], int numYears,
    int startYear, SW_CLIMATE_YEARLY *climateOutput);
void findDriestQtr(int numYears, Bool inNorthHem, double *meanTempDriestQtr_C,
                   double **meanTempMon_C, double **PPTMon_cm);
void driestQtrSouthAdjMonYears(int month, int *adjustedYearZero, int *adjustedYearOne,
                           int *adjustedYearTwo, int *adjustedMonth, int *prevMonth,
                           int *nextMonth);
void initializeClimatePtrs(SW_CLIMATE_YEARLY *climateOutput,
                           SW_CLIMATE_CLIM *climateAverages);
void initializeMonthlyClimatePtrs(SW_CLIMATE_YEARLY *climateOutput);
void allocateClimateStructs(int numYears, SW_CLIMATE_YEARLY *climateOutput,
                        SW_CLIMATE_CLIM *climateAverages, LOG_INFO* LogInfo);
void deallocateClimateStructs(SW_CLIMATE_YEARLY *climateOutput,
                              SW_CLIMATE_CLIM *climateAverages);
void _read_weather_hist(
  TimeInt year,
  SW_WEATHER_HIST *yearWeather,
  char weather_prefix[],
  unsigned int n_input_forcings,
  unsigned int *dailyInputIndices,
  Bool *dailyInputFlags,
  LOG_INFO* LogInfo
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
  RealD *r_humidity,
  TimeInt cum_monthdays[],
  TimeInt days_in_month[],
  LOG_INFO* LogInfo
);
void finalizeAllWeather(SW_MARKOV* SW_Markov, SW_WEATHER *w,
          TimeInt cum_monthdays[], TimeInt days_in_month[], LOG_INFO* LogInfo);

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
  double *scale_shortWaveRad,
  TimeInt cum_monthdays[],
  TimeInt days_in_month[]
);
void generateMissingWeather(
  SW_MARKOV* SW_Markov,
  SW_WEATHER_HIST **allHist,
  int startYear,
  unsigned int n_years,
  unsigned int method,
  unsigned int optLOCF_nMax,
  LOG_INFO* LogInfo
);
void checkAllWeather(SW_WEATHER *weather, LOG_INFO* LogInfo);
void allocateAllWeather(SW_WEATHER_HIST ***allHist, unsigned int n_years,
                        LOG_INFO* LogInfo);
void initializeAllWeatherPtrs(SW_WEATHER_HIST **allHist, unsigned int n_years);
void deallocateAllWeather(SW_WEATHER *w);
void _clear_hist_weather(SW_WEATHER_HIST *yearWeather);
void SW_WTH_finalize_all_weather(SW_MARKOV* SW_Markov, SW_WEATHER* SW_Weather,
              TimeInt cum_monthdays[], TimeInt days_in_month[], LOG_INFO* LogInfo);
void SW_WTH_init_run(SW_WEATHER* SW_Weather);
void SW_WTH_construct(SW_WEATHER* SW_Weather, LOG_INFO* LogInfo);
void SW_WTH_init_ptrs(SW_WEATHER* SW_Weather);
void SW_WTH_deconstruct(SW_MARKOV* SW_Markov, SW_WEATHER* SW_Weather);
void SW_WTH_new_day(SW_WEATHER* SW_Weather, SW_SITE* SW_Site, RealD snowpack[],
                    TimeInt doy, TimeInt year, LOG_INFO* LogInfo);
void SW_WTH_sum_today(void);



#ifdef __cplusplus
}
#endif

#endif
