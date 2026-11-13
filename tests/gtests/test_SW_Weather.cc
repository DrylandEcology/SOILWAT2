#include "include/generic.h"             // for swTRUE, swFALSE, Bool, squared
#include "include/SW_datastructs.h"      // for SW_CLIMATE_CLIM, SW_CLIMATE...
#include "include/SW_Defines.h"          // for MAX_MONTHS, SHORT_WR, REL_H...
#include "include/SW_Flow_lib_PET.h"     // for svp, actualVaporPressure2, ...
#include "include/SW_Main_lib.h"         // for sw_fail_on_error
#include "include/SW_Markov.h"           // for SW_MKV_setup
#include "include/SW_Sky.h"              // for SW_SKY_read
#include "include/SW_Weather.h"          // for SW_WTH_read, checkYearlyWeather
#include "include/Times.h"               // for Jan, Feb, Dec
#include "tests/gtests/sw_testhelpers.h" // for WeatherFixtureTest, tol6
#include "gmock/gmock.h"                 // for HasSubstr, MakePredicateFor...
#include "gtest/gtest.h"                 // for Test, Message, TestPartResul...
#include <cmath>                         // for isnan, sqrt, fabs, fmax
#include <stdio.h>                       // for snprintf, NULL
#include <string.h>                      // for memcpy


using ::testing::HasSubstr;

namespace {
TEST_F(WeatherFixtureTest, WeatherDefaultValues) {

    // Testing to fill allHist from `SW_Weather`
    SW_SKY_read(
        SW_Domain.SW_PathInputs.txtInFiles, &SW_Run.RunIn.SkyRunIn, &LogInfo
    );
    sw_fail_on_error(
        ROOT_PROC, &LogInfo
    ); // exit test program if unexpected error

    readAllWeather(
        SW_Run.RunIn.weathRunAllHist,
        1980,
        SW_Run.WeatherIn->n_years,
        SW_Run.WeatherIn->use_weathergenerator_only,
        SW_Run.WeatherIn->name_prefix,
        SW_Run.WeatherIn->use_cloudCoverMonthly,
        SW_Run.WeatherIn->use_humidityMonthly,
        SW_Run.WeatherIn->use_windSpeedMonthly,
        SW_Run.WeatherIn->n_input_forcings,
        SW_Run.WeatherIn->dailyInputIndices,
        SW_Run.WeatherIn->dailyInputFlags,
        SW_Run.WeatherIn->fixWeatherData,
        SW_Run.WeatherIn->isRelHumidInputOverIce,
        SW_Run.RunIn.SkyRunIn.cloudcov,
        SW_Run.RunIn.SkyRunIn.windspeed,
        SW_Run.RunIn.SkyRunIn.r_humidity,
        SW_Run.RunIn.ModelRunIn.elevation,
        SW_Domain.SW_ConstInfo.ModelSim.cum_monthdays,
        SW_Domain.SW_ConstInfo.ModelSim.days_in_month,
        &LogInfo
    );
    sw_fail_on_error(
        ROOT_PROC, &LogInfo
    ); // exit test program if unexpected error

    // Test first day of first year in `allHist` to make sure correct
    // temperature max/min/avg and precipitation values
    EXPECT_NEAR(SW_Run.RunIn.weathRunAllHist[0].temp_max[0], -0.520000, tol6);
    EXPECT_NEAR(SW_Run.RunIn.weathRunAllHist[0].temp_avg[0], -8.095000, tol6);
    EXPECT_NEAR(SW_Run.RunIn.weathRunAllHist[0].temp_min[0], -15.670000, tol6);
    EXPECT_NEAR(SW_Run.RunIn.weathRunAllHist[0].ppt[0], .220000, tol6);
    EXPECT_NEAR(
        SW_Run.RunIn.weathRunAllHist[0].cloudcov_daily[0], 66.483871, tol6
    );
    EXPECT_NEAR(
        SW_Run.RunIn.weathRunAllHist[0].windspeed_daily[0], 1.300000, tol6
    );
    EXPECT_NEAR(
        SW_Run.RunIn.weathRunAllHist[0].r_humidity_daily[0], 61.000000, tol6
    );
    EXPECT_TRUE(missing(SW_Run.RunIn.weathRunAllHist[0].actualVaporPressure[0])
    );
    EXPECT_TRUE(missing(SW_Run.RunIn.weathRunAllHist[0].shortWaveRad[0]));
}

TEST_F(WeatherFixtureTest, WeatherNoMemoryLeakIfDecreasedNumberOfYears) {

    // Default number of years is 31
    EXPECT_EQ(SW_Run.WeatherIn->n_years, 31);

    // Decrease number of years
    SW_Run.ModelIn->startyr = 1981;
    SW_Run.ModelIn->endyr = 1982;

    // Real expectation is that there is no memory leak for `allHist`
    SW_WTH_read(
        SW_Run.WeatherIn,
        &SW_Run.RunIn.weathRunAllHist,
        &SW_Run.RunIn.SkyRunIn,
        SW_Run.ModelIn,
        SW_Run.RunIn.ModelRunIn.elevation,
        swTRUE,
        SW_Domain.SW_ConstInfo.ModelSim.cum_monthdays,
        SW_Domain.SW_ConstInfo.ModelSim.days_in_month,
        &LogInfo
    );
    sw_fail_on_error(
        ROOT_PROC, &LogInfo
    ); // exit test program if unexpected error

    EXPECT_EQ(SW_Run.WeatherIn->n_years, 2);
}

TEST_F(WeatherFixtureTest, WeatherSomeMissingValuesDays) {
#if defined(SW_WEEKDAYS) && SW_WEEKDAYS == PENTAD
    GTEST_SKIP() << "No example weather generator inputs with pentad weeks";
#endif

    const TimeInt n_years = SW_Run.ModelIn->endyr - SW_Run.ModelIn->startyr + 1;

    SW_Run.WeatherIn->generateWeatherMethod = wgMKV;

    // Change directory to get input files with some missing data
    (void) snprintf(
        SW_Run.WeatherIn->name_prefix,
        sizeof SW_Run.WeatherIn->name_prefix,
        "%s",
        "Input/data_weather_missing/weath"
    );

    SW_MKV_setup(
        &SW_Run.MarkovIn,
        SW_Run.WeatherIn->rng_seed,
        SW_Run.WeatherIn->generateWeatherMethod,
        SW_Domain.SW_PathInputs.txtInFiles,
        &LogInfo
    );
    sw_fail_on_error(
        ROOT_PROC, &LogInfo
    ); // exit test program if unexpected error

    SW_WTH_read(
        SW_Run.WeatherIn,
        &SW_Run.RunIn.weathRunAllHist,
        &SW_Run.RunIn.SkyRunIn,
        SW_Run.ModelIn,
        SW_Run.RunIn.ModelRunIn.elevation,
        swTRUE,
        SW_Domain.SW_ConstInfo.ModelSim.cum_monthdays,
        SW_Domain.SW_ConstInfo.ModelSim.days_in_month,
        &LogInfo
    );
    sw_fail_on_error(
        ROOT_PROC, &LogInfo
    ); // exit test program if unexpected error

    SW_WTH_finalize_yearly_weather(
        &SW_Run.MarkovIn,
        SW_Run.WeatherIn,
        SW_Run.RunIn.weathRunAllHist,
        &SW_Run.WeatherSim,
        SW_Domain.SW_ConstInfo.ModelSim.cum_monthdays,
        SW_Domain.SW_ConstInfo.ModelSim.days_in_month,
        SW_Domain.SW_ConstInfo.ModelSim.year,
        n_years,
        template_SW_Domain.startstart,
        SW_Domain.endend,
        SW_Domain.startyr,
        SW_Domain.endyr,
        SW_Run.WeatherSim.trivialScaling,
        &LogInfo
    );
    sw_fail_on_error(
        ROOT_PROC, &LogInfo
    ); // exit test program if unexpected error


    // Expect that missing input values (from 1980) are filled by the weather
    // generator
    EXPECT_FALSE(missing(SW_Run.RunIn.weathRunAllHist[0].temp_max[0]));
    EXPECT_FALSE(missing(SW_Run.RunIn.weathRunAllHist[0].temp_max[1]));
    EXPECT_FALSE(missing(SW_Run.RunIn.weathRunAllHist[0].temp_min[0]));
    EXPECT_FALSE(missing(SW_Run.RunIn.weathRunAllHist[0].temp_min[2]));
    EXPECT_FALSE(missing(SW_Run.RunIn.weathRunAllHist[0].ppt[0]));
    EXPECT_FALSE(missing(SW_Run.RunIn.weathRunAllHist[0].ppt[3]));
    EXPECT_FALSE(missing(SW_Run.RunIn.weathRunAllHist[0].cloudcov_daily[0]));
    EXPECT_FALSE(missing(SW_Run.RunIn.weathRunAllHist[0].cloudcov_daily[3]));
    EXPECT_FALSE(missing(SW_Run.RunIn.weathRunAllHist[0].windspeed_daily[0]));
    EXPECT_FALSE(missing(SW_Run.RunIn.weathRunAllHist[0].windspeed_daily[3]));
    EXPECT_FALSE(missing(SW_Run.RunIn.weathRunAllHist[0].r_humidity_daily[0]));
    EXPECT_FALSE(missing(SW_Run.RunIn.weathRunAllHist[0].r_humidity_daily[3]));
    EXPECT_FALSE(missing(SW_Run.RunIn.weathRunAllHist[0].actualVaporPressure[0])
    );
    EXPECT_FALSE(missing(SW_Run.RunIn.weathRunAllHist[0].actualVaporPressure[3])
    );

    EXPECT_TRUE(missing(SW_Run.RunIn.weathRunAllHist[0].shortWaveRad[0]));
    EXPECT_TRUE(missing(SW_Run.RunIn.weathRunAllHist[0].shortWaveRad[0]));
}

TEST_F(WeatherFixtureTest, WeatherSomeMissingValuesYears) {
#if defined(SW_WEEKDAYS) && SW_WEEKDAYS == PENTAD
    GTEST_SKIP() << "No example weather generator inputs with pentad weeks";
#endif

    int year;
    int day;
    TimeInt n_years;
    SW_Run.WeatherIn->generateWeatherMethod = wgMKV;

    // Change directory to get input files with some missing data
    (void) snprintf(
        SW_Run.WeatherIn->name_prefix,
        sizeof SW_Run.WeatherIn->name_prefix,
        "%s",
        "Input/data_weather_missing/weath"
    );

    SW_MKV_setup(
        &SW_Run.MarkovIn,
        SW_Run.WeatherIn->rng_seed,
        SW_Run.WeatherIn->generateWeatherMethod,
        SW_Domain.SW_PathInputs.txtInFiles,
        &LogInfo
    );
    sw_fail_on_error(
        ROOT_PROC, &LogInfo
    ); // exit test program if unexpected error

    SW_Run.ModelIn->startyr = 1981;
    SW_Run.ModelIn->endyr = 1982;
    n_years = SW_Run.ModelIn->endyr - SW_Run.ModelIn->startyr + 1;

    SW_WTH_read(
        SW_Run.WeatherIn,
        &SW_Run.RunIn.weathRunAllHist,
        &SW_Run.RunIn.SkyRunIn,
        SW_Run.ModelIn,
        SW_Run.RunIn.ModelRunIn.elevation,
        swTRUE,
        SW_Domain.SW_ConstInfo.ModelSim.cum_monthdays,
        SW_Domain.SW_ConstInfo.ModelSim.days_in_month,
        &LogInfo
    );
    sw_fail_on_error(
        ROOT_PROC, &LogInfo
    ); // exit test program if unexpected error

    SW_WTH_finalize_yearly_weather(
        &SW_Run.MarkovIn,
        SW_Run.WeatherIn,
        SW_Run.RunIn.weathRunAllHist,
        &SW_Run.WeatherSim,
        SW_Domain.SW_ConstInfo.ModelSim.cum_monthdays,
        SW_Domain.SW_ConstInfo.ModelSim.days_in_month,
        SW_Domain.SW_ConstInfo.ModelSim.year,
        n_years,
        template_SW_Domain.startstart,
        SW_Domain.endend,
        SW_Domain.startyr,
        SW_Domain.endyr,
        SW_Run.WeatherSim.trivialScaling,
        &LogInfo
    );
    sw_fail_on_error(
        ROOT_PROC, &LogInfo
    ); // exit test program if unexpected error


    // Check everyday's value and check that it is not `MISSING`
    for (year = 0; year < 2; year++) {
        for (day = 0; day < 365; day++) {
            EXPECT_FALSE(
                missing(SW_Run.RunIn.weathRunAllHist[year].temp_max[day])
            );
        }
    }
}

TEST_F(WeatherFixtureTest, WeatherWeatherGeneratorOnly) {
#if defined(SW_WEEKDAYS) && SW_WEEKDAYS == PENTAD
    GTEST_SKIP() << "No example weather generator inputs with pentad weeks";
#endif

    int year;
    int day;
    const TimeInt n_years = SW_Run.ModelIn->endyr - SW_Run.ModelIn->startyr + 1;

    SW_Run.WeatherIn->generateWeatherMethod = wgMKV;
    SW_Run.WeatherIn->use_weathergenerator_only = swTRUE;

    SW_MKV_setup(
        &SW_Run.MarkovIn,
        SW_Run.WeatherIn->rng_seed,
        SW_Run.WeatherIn->generateWeatherMethod,
        SW_Domain.SW_PathInputs.txtInFiles,
        &LogInfo
    );
    sw_fail_on_error(
        ROOT_PROC, &LogInfo
    ); // exit test program if unexpected error

    // Change directory to get input files with some missing data
    (void) snprintf(
        SW_Run.WeatherIn->name_prefix,
        sizeof SW_Run.WeatherIn->name_prefix,
        "%s",
        "Input/data_weather_nonexisting/weath"
    );

    SW_WTH_read(
        SW_Run.WeatherIn,
        &SW_Run.RunIn.weathRunAllHist,
        &SW_Run.RunIn.SkyRunIn,
        SW_Run.ModelIn,
        SW_Run.RunIn.ModelRunIn.elevation,
        swTRUE,
        SW_Domain.SW_ConstInfo.ModelSim.cum_monthdays,
        SW_Domain.SW_ConstInfo.ModelSim.days_in_month,
        &LogInfo
    );
    sw_fail_on_error(
        ROOT_PROC, &LogInfo
    ); // exit test program if unexpected error

    SW_WTH_finalize_yearly_weather(
        &SW_Run.MarkovIn,
        SW_Run.WeatherIn,
        SW_Run.RunIn.weathRunAllHist,
        &SW_Run.WeatherSim,
        SW_Domain.SW_ConstInfo.ModelSim.cum_monthdays,
        SW_Domain.SW_ConstInfo.ModelSim.days_in_month,
        SW_Domain.SW_ConstInfo.ModelSim.year,
        n_years,
        template_SW_Domain.startstart,
        SW_Domain.endend,
        SW_Domain.startyr,
        SW_Domain.endyr,
        SW_Run.WeatherSim.trivialScaling,
        &LogInfo
    );
    sw_fail_on_error(
        ROOT_PROC, &LogInfo
    ); // exit test program if unexpected error

    // Check everyday's value and check that it is not `MISSING`
    for (year = 0; year < 31; year++) {
        for (day = 0; day < 365; day++) {
            EXPECT_FALSE(
                missing(SW_Run.RunIn.weathRunAllHist[year].temp_max[day])
            );
        }
    }
}

TEST_F(WeatherFixtureTest, ReadAllWeatherTooManyMissingForLOCFDeathTest) {

    const TimeInt year = 1981;

    TimeInt n_years;
    // Error: too many missing values and weather generator turned off

    // Change to directory without input files
    (void) snprintf(
        SW_Run.WeatherIn->name_prefix,
        sizeof SW_Run.WeatherIn->name_prefix,
        "%s",
        "Input/data_weather_nonexisting/weath"
    );

    // Set LOCF (temp) + 0 (PPT) method
    SW_Run.WeatherIn->generateWeatherMethod = wgLOCF;

    SW_Run.ModelIn->startyr = year;
    SW_Run.ModelIn->endyr = year;
    n_years = SW_Run.ModelIn->endyr - SW_Run.ModelIn->startyr + 1;

    SW_WTH_read(
        SW_Run.WeatherIn,
        &SW_Run.RunIn.weathRunAllHist,
        &SW_Run.RunIn.SkyRunIn,
        SW_Run.ModelIn,
        SW_Run.RunIn.ModelRunIn.elevation,
        swTRUE,
        SW_Domain.SW_ConstInfo.ModelSim.cum_monthdays,
        SW_Domain.SW_ConstInfo.ModelSim.days_in_month,
        &LogInfo
    );
    sw_fail_on_error(
        ROOT_PROC, &LogInfo
    ); // exit test program if unexpected error

    SW_WTH_finalize_yearly_weather(
        &SW_Run.MarkovIn,
        SW_Run.WeatherIn,
        SW_Run.RunIn.weathRunAllHist,
        &SW_Run.WeatherSim,
        SW_Domain.SW_ConstInfo.ModelSim.cum_monthdays,
        SW_Domain.SW_ConstInfo.ModelSim.days_in_month,
        year,
        n_years,
        template_SW_Domain.startstart,
        SW_Domain.endend,
        SW_Domain.startyr,
        SW_Domain.endyr,
        SW_Run.WeatherSim.trivialScaling,
        &LogInfo
    );
    // expect error: don't exit test program via `sw_fail_on_error(ROOT_PROC,
    // &LogInfo)`

    // Detect failure by error message
    EXPECT_THAT(
        LogInfo.errorMsg, HasSubstr("more than 3 days missing in year 1981")
    );
}

TEST_F(WeatherFixtureTest, ClimateVariableClimateFromDefaultWeather) {

    // This test relies on allHist from `SW_WEATHER_INPUTS` being already filled
    SW_CLIMATE_YEARLY climateOutput;
    SW_CLIMATE_CLIM climateAverages;

    Bool const inNorthHem = swTRUE;

    // Allocate memory
    // 31 = number of years used in test
    allocateClimateStructs(31, &climateOutput, &climateAverages, &LogInfo);
    sw_fail_on_error(
        ROOT_PROC, &LogInfo
    ); // exit test program if unexpected error


    // ------ Check climate variables for default weather ------
    // 1980 is first year out of 31 years of default weather

    // --- Annual time-series of climate variables ------
    // Here, check values for 1980

    /* Expect identical output to rSOILWAT2 (e.g., v5.3.1)
     * NOTE: Command uses deprecated calc_SiteClimate (rSOILWAT >= v.6.0.0)
     ```{r}
       rSOILWAT2:::calc_SiteClimate_old(
         weatherList = rSOILWAT2::get_WeatherHistory(
           rSOILWAT2::sw_exampleData
         )[1],
         do_C4vars = TRUE,
         do_Cheatgrass_ClimVars = TRUE
       )
     ```
    */

    calcSiteClimate(
        SW_Run.RunIn.weathRunAllHist,
        SW_Domain.SW_ConstInfo.ModelSim.cum_monthdays,
        SW_Domain.SW_ConstInfo.ModelSim.days_in_month,
        31,
        1980,
        inNorthHem,
        &climateOutput
    );

    EXPECT_NEAR(climateOutput.meanTempMon_C[Jan][0], -8.432581, tol6);
    EXPECT_NEAR(climateOutput.maxTempMon_C[Jan][0], -2.562581, tol6);
    EXPECT_NEAR(climateOutput.minTempMon_C[Jan][0], -14.302581, tol6);
    EXPECT_NEAR(climateOutput.PPTMon_cm[Jan][0], 15.1400001, tol6);
    EXPECT_NEAR(climateOutput.PPT_cm[0], 59.27, tol1);
    EXPECT_NEAR(climateOutput.meanTemp_C[0], 4.524863, tol1);

    // Climate variables used for C4 grass cover
    // (stdev of one value is undefined)
    EXPECT_DOUBLE_EQ(climateOutput.minTemp7thMon_C[0], 2.81);
    EXPECT_NEAR(climateOutput.frostFree_days[0], 92, tol6);
    EXPECT_NEAR(climateOutput.ddAbove65F_degday[0], 13.546000, tol6);


    // Climate variables used for cheatgrass cover
    // (stdev of one value is undefined)
    EXPECT_NEAR(climateOutput.PPT7thMon_mm[0], 18.299999, tol6);
    EXPECT_NEAR(climateOutput.meanTempDriestQtr_C[0], 0.936387, tol6);
    EXPECT_NEAR(climateOutput.minTemp2ndMon_C[0], -12.822068, tol6);


    /* --- Long-term variables (aggregated across years) ------
     * Expect identical output to rSOILWAT2 (e.g., v5.3.1)
     * NOTE: Command uses deprecated calc_SiteClimate (rSOILWAT >= v.6.0.0)
     ```{r}
       rSOILWAT2:::calc_SiteClimate_old(
         weatherList = rSOILWAT2::get_WeatherHistory(
           rSOILWAT2::sw_exampleData
         ),
         do_C4vars = TRUE,
         do_Cheatgrass_ClimVars = TRUE
       )
     ```
    */

    averageClimateAcrossYears(&climateOutput, 31, &climateAverages);

    EXPECT_NEAR(climateAverages.meanTempMon_C[Jan], -9.325551, tol6);
    EXPECT_NEAR(climateAverages.maxTempMon_C[Jan], -2.714381, tol6);
    EXPECT_NEAR(climateAverages.minTempMon_C[Jan], -15.936722, tol6);
    EXPECT_NEAR(climateAverages.PPTMon_cm[Jan], 6.867419, tol6);

    EXPECT_NEAR(climateAverages.PPT_cm, 62.817419, tol6);
    // Note: rSOILWAT2 v5.3.1 returns incorrect meanTemp_C = 4.153896
    //   which is long-term daily average (but not long-term annual average)
    EXPECT_NEAR(climateAverages.meanTemp_C, 4.154009, tol6);

    // Climate variables used for C4 grass cover
    EXPECT_NEAR(climateAverages.minTemp7thMon_C, 3.078387, tol6);
    EXPECT_NEAR(climateAverages.frostFree_days, 90.612903, tol6);
    EXPECT_NEAR(climateAverages.ddAbove65F_degday, 21.168032, tol6);

    EXPECT_NEAR(climateAverages.sdC4[0], 1.785535, tol6);
    EXPECT_NEAR(climateAverages.sdC4[1], 14.091788, tol6);
    EXPECT_NEAR(climateAverages.sdC4[2], 19.953560, tol6);

    // Climate variables used for cheatgrass cover
    EXPECT_NEAR(climateAverages.PPT7thMon_mm, 35.729032, tol6);
    EXPECT_NEAR(climateAverages.meanTempDriestQtr_C, 11.524859, tol6);
    EXPECT_NEAR(climateAverages.minTemp2ndMon_C, -13.904599, tol6);

    EXPECT_NEAR(climateAverages.sdCheatgrass[0], 21.598367, tol6);
    EXPECT_NEAR(climateAverages.sdCheatgrass[1], 7.171922, tol6);
    EXPECT_NEAR(climateAverages.sdCheatgrass[2], 2.618434, tol6);


    // ------ Reset and deallocate
    deallocateClimateStructs(&climateOutput, &climateAverages);
}

TEST_F(WeatherFixtureTest, ClimateVariableClimateFromOneYearWeather) {

    // This test relies on allHist from `SW_WEATHER_INPUTS` being already filled
    SW_CLIMATE_YEARLY climateOutput;
    SW_CLIMATE_CLIM climateAverages;

    Bool const inNorthHem = swTRUE;

    // Allocate memory
    // 1 = number of years used in test
    allocateClimateStructs(1, &climateOutput, &climateAverages, &LogInfo);
    sw_fail_on_error(
        ROOT_PROC, &LogInfo
    ); // exit test program if unexpected error

    // ------ Check climate variables for one year of default weather ------

    /* Expect identical output to rSOILWAT2 (e.g., v5.3.1)
     * NOTE: Command uses deprecated calc_SiteClimate (rSOILWAT >= v.6.0.0)
     ```{r}
       rSOILWAT2:::calc_SiteClimate_old(
         weatherList = rSOILWAT2::get_WeatherHistory(
           rSOILWAT2::sw_exampleData
         )[1],
         do_C4vars = TRUE,
         do_Cheatgrass_ClimVars = TRUE
       )
     ```
    */

    calcSiteClimate(
        SW_Run.RunIn.weathRunAllHist,
        SW_Domain.SW_ConstInfo.ModelSim.cum_monthdays,
        SW_Domain.SW_ConstInfo.ModelSim.days_in_month,
        1,
        1980,
        inNorthHem,
        &climateOutput
    );
    averageClimateAcrossYears(&climateOutput, 1, &climateAverages);

    // Expect that aggregated values across one year are identical
    // to values of that one year
    EXPECT_DOUBLE_EQ(
        climateAverages.meanTempMon_C[Jan], climateOutput.meanTempMon_C[Jan][0]
    );
    EXPECT_DOUBLE_EQ(
        climateAverages.maxTempMon_C[Jan], climateOutput.maxTempMon_C[Jan][0]
    );
    EXPECT_DOUBLE_EQ(
        climateAverages.minTempMon_C[Jan], climateOutput.minTempMon_C[Jan][0]
    );
    EXPECT_DOUBLE_EQ(
        climateAverages.PPTMon_cm[Jan], climateOutput.PPTMon_cm[Jan][0]
    );
    EXPECT_DOUBLE_EQ(climateAverages.PPT_cm, climateOutput.PPT_cm[0]);
    EXPECT_DOUBLE_EQ(climateAverages.meanTemp_C, climateOutput.meanTemp_C[0]);

    // Climate variables used for C4 grass cover
    EXPECT_DOUBLE_EQ(
        climateAverages.minTemp7thMon_C, climateOutput.minTemp7thMon_C[0]
    );
    EXPECT_DOUBLE_EQ(
        climateAverages.frostFree_days, climateOutput.frostFree_days[0]
    );
    EXPECT_DOUBLE_EQ(
        climateAverages.ddAbove65F_degday, climateOutput.ddAbove65F_degday[0]
    );

    // Climate variables used for cheatgrass cover
    EXPECT_DOUBLE_EQ(
        climateAverages.PPT7thMon_mm, climateOutput.PPT7thMon_mm[0]
    );
    EXPECT_DOUBLE_EQ(
        climateAverages.meanTempDriestQtr_C,
        climateOutput.meanTempDriestQtr_C[0]
    );
    EXPECT_DOUBLE_EQ(
        climateAverages.minTemp2ndMon_C, climateOutput.minTemp2ndMon_C[0]
    );


    EXPECT_NEAR(climateAverages.meanTempMon_C[Jan], -8.432581, tol6);
    EXPECT_NEAR(climateAverages.maxTempMon_C[Jan], -2.562581, tol6);
    EXPECT_NEAR(climateAverages.minTempMon_C[Jan], -14.302581, tol6);
    EXPECT_NEAR(climateAverages.PPTMon_cm[Jan], 15.1400001, tol6);
    EXPECT_NEAR(climateAverages.PPT_cm, 59.27, tol1);
    EXPECT_NEAR(climateAverages.meanTemp_C, 4.524863, tol1);

    // Climate variables used for C4 grass cover
    // (stdev of one value is undefined)
    EXPECT_DOUBLE_EQ(climateAverages.minTemp7thMon_C, 2.81);
    EXPECT_NEAR(climateAverages.frostFree_days, 92, tol6);
    EXPECT_NEAR(climateAverages.ddAbove65F_degday, 13.546000, tol6);
    EXPECT_TRUE(std::isnan(climateAverages.sdC4[0]));
    EXPECT_TRUE(std::isnan(climateAverages.sdC4[1]));
    EXPECT_TRUE(std::isnan(climateAverages.sdC4[2]));

    // Climate variables used for cheatgrass cover
    // (stdev of one value is undefined)
    EXPECT_NEAR(climateAverages.PPT7thMon_mm, 18.299999, tol6);
    EXPECT_NEAR(climateAverages.meanTempDriestQtr_C, 0.936387, tol6);
    EXPECT_NEAR(climateAverages.minTemp2ndMon_C, -12.822068, tol6);
    EXPECT_TRUE(std::isnan(climateAverages.sdCheatgrass[0]));
    EXPECT_TRUE(std::isnan(climateAverages.sdCheatgrass[1]));
    EXPECT_TRUE(std::isnan(climateAverages.sdCheatgrass[2]));


    // ------ Reset and deallocate
    deallocateClimateStructs(&climateOutput, &climateAverages);
}

TEST_F(WeatherFixtureTest, ClimateFromDefaultWeatherSouth) {

    /* ==================================================================
     Values for these SOILWAT2 tests and in rSOILWAT2 v6.0.0 for
     southern hemisphere are different than versions preceeding v6.0.0.

     rSOILWAT2 previously calculated climate variable values for southern
     hemisphere differently than SOILWAT2 now does. To briefly explain,
     rSOILWAT2 would have instances within the north/south adjustment where
     July 1st - 3rd would be ignored resulting in different values from
     missing data for a specific year. SOILWAT2 modified the north/south
     algorithm and the southern year now properly starts on July 1st
     resulting in different data values for a year.
       ================================================================= */

    // This test relies on allHist from `SW_WEATHER_INPUTS` being already filled
    SW_CLIMATE_YEARLY climateOutput;
    SW_CLIMATE_CLIM climateAverages;

    // "South" and not "North" to reduce confusion when calling
    // `calcSiteClimate()`
    Bool const inSouthHem = swFALSE;

    // Allocate memory
    // 31 = number of years used in test
    allocateClimateStructs(31, &climateOutput, &climateAverages, &LogInfo);
    sw_fail_on_error(
        ROOT_PROC, &LogInfo
    ); // exit test program if unexpected error


    // ------ Check climate variables for default weather ------
    // 1980 is first year out of 31 years of default weather

    // --- Annual time-series of climate variables ------
    // Here, check values for 1980

    /* Expect similar output to rSOILWAT2 before v6.0.0 (e.g., v5.3.1)
     * NOTE: Command uses deprecated calc_SiteClimate (rSOILWAT >= v.6.0.0)
     ```{r}
       rSOILWAT2:::calc_SiteClimate_old(
         weatherList = rSOILWAT2::get_WeatherHistory(
           rSOILWAT2::sw_exampleData
         )[1],
         do_C4vars = TRUE,
         do_Cheatgrass_ClimVars = TRUE,
         latitude = -10
       )
    ```
    */

    calcSiteClimate(
        SW_Run.RunIn.weathRunAllHist,
        SW_Domain.SW_ConstInfo.ModelSim.cum_monthdays,
        SW_Domain.SW_ConstInfo.ModelSim.days_in_month,
        31,
        1980,
        inSouthHem,
        &climateOutput
    );

    EXPECT_NEAR(climateOutput.meanTempMon_C[Jan][0], -8.432581, tol6);
    EXPECT_NEAR(climateOutput.maxTempMon_C[Jan][0], -2.562581, tol6);
    EXPECT_NEAR(climateOutput.minTempMon_C[Jan][0], -14.302581, tol6);
    EXPECT_NEAR(climateOutput.PPTMon_cm[Jan][0], 15.1400001, tol6);
    EXPECT_NEAR(climateOutput.PPT_cm[0], 59.27, tol6);
    EXPECT_NEAR(climateOutput.meanTemp_C[0], 4.524863, tol6);

    // Climate variables used for C4 grass cover
    // (stdev of one value is undefined)
    EXPECT_NEAR(climateOutput.minTemp7thMon_C[1], -16.98, tol6);
    EXPECT_NEAR(climateOutput.frostFree_days[1], 78, tol6);
    EXPECT_NEAR(climateOutput.ddAbove65F_degday[1], 16.458001, tol6);


    // Climate variables used for cheatgrass cover
    // (stdev of one value is undefined)
    EXPECT_DOUBLE_EQ(climateOutput.PPT7thMon_mm[1], 22.2);
    EXPECT_NEAR(climateOutput.meanTempDriestQtr_C[0], 0.936387, tol6);
    EXPECT_NEAR(climateOutput.minTemp2ndMon_C[1], 5.1445161, tol6);


    /* --- Long-term variables (aggregated across years) ------
     * Expect similar output to rSOILWAT2 before v6.0.0 (e.g., v5.3.1),
     identical otherwise
     * NOTE: Command uses deprecated calc_SiteClimate (rSOILWAT >= v.6.0.0)
     ```{r}
       rSOILWAT2:::calc_SiteClimate_old(
         weatherList = rSOILWAT2::get_WeatherHistory(
           rSOILWAT2::sw_exampleData
         ),
         do_C4vars = TRUE,
         do_Cheatgrass_ClimVars = TRUE,
         latitude = -10
       )
     ```
    */

    averageClimateAcrossYears(&climateOutput, 31, &climateAverages);

    EXPECT_NEAR(climateAverages.meanTempMon_C[Jan], -9.325551, tol6);
    EXPECT_NEAR(climateAverages.maxTempMon_C[Jan], -2.714381, tol6);
    EXPECT_NEAR(climateAverages.minTempMon_C[Jan], -15.936722, tol6);
    EXPECT_NEAR(climateAverages.PPTMon_cm[Jan], 6.867419, tol6);

    EXPECT_NEAR(climateAverages.PPT_cm, 62.817419, tol6);
    // Note: rSOILWAT2 v5.3.1 returns incorrect meanTemp_C = 4.153896
    //   which is long-term daily average (but not long-term annual average)
    EXPECT_NEAR(climateAverages.meanTemp_C, 4.154009, tol6);

    // Climate variables used for C4 grass cover
    EXPECT_NEAR(climateAverages.minTemp7thMon_C, -27.199333, tol6);
    EXPECT_NEAR(climateAverages.frostFree_days, 72.599999, tol6);
    EXPECT_NEAR(climateAverages.ddAbove65F_degday, 21.357533, tol6);

    EXPECT_NEAR(climateAverages.sdC4[0], 5.325365, tol6);
    EXPECT_NEAR(climateAverages.sdC4[1], 9.586628, tol6);
    EXPECT_NEAR(climateAverages.sdC4[2], 19.550419, tol6);

    // Climate variables used for cheatgrass cover
    EXPECT_NEAR(climateAverages.PPT7thMon_mm, 65.916666, tol6);
    EXPECT_NEAR(climateAverages.meanTempDriestQtr_C, 11.401228, tol6);
    EXPECT_NEAR(climateAverages.minTemp2ndMon_C, 6.545577, tol6);

    EXPECT_NEAR(climateAverages.sdCheatgrass[0], 35.285408, tol6);
    EXPECT_NEAR(climateAverages.sdCheatgrass[1], 7.260851, tol6);
    EXPECT_NEAR(climateAverages.sdCheatgrass[2], 1.639639, tol6);


    // ------ Reset and deallocate
    deallocateClimateStructs(&climateOutput, &climateAverages);
}

TEST_F(WeatherFixtureTest, ClimateVariableClimateFromConstantWeather) {

    SW_CLIMATE_YEARLY climateOutput;
    SW_CLIMATE_CLIM climateAverages;
    SW_WEATHER_HIST *allHist = NULL;

    unsigned int const n_years = 2;

    Bool const inNorthHem = swTRUE;

    // Allocate memory
    allocateClimateStructs(n_years, &climateOutput, &climateAverages, &LogInfo);
    sw_fail_on_error(
        ROOT_PROC, &LogInfo
    ); // exit test program if unexpected error

    SW_WTH_allocateAllWeather(&allHist, n_years, &LogInfo);
    sw_fail_on_error(
        ROOT_PROC, &LogInfo
    ); // exit test program if unexpected error


    // ------ Check climate variables for constant weather = 1 ------
    // Years 1980 (leap) + 1981 (nonleap)

    // Expect that min/avg/max of weather variables are 1
    // Expect that sums of weather variables correspond to number of days
    // Expect that the average annual number of days is 365.5 (1 leap + 1
    // nonleap) Expect that the average number of days in February is 28.5 (1
    // leap + 1 nonleap)

    // Set all weather values to 1
    for (unsigned int year = 0; year < n_years; year++) {
        for (int day = 0; day < 366; day++) {
            allHist[year].temp_max[day] = 1.;
            allHist[year].temp_min[day] = 1.;
            allHist[year].temp_avg[day] = 1.;
            allHist[year].ppt[day] = 1.;
        }
    }

    // --- Annual time-series of climate variables ------
    calcSiteClimate(
        allHist,
        SW_Domain.SW_ConstInfo.ModelSim.cum_monthdays,
        SW_Domain.SW_ConstInfo.ModelSim.days_in_month,
        n_years,
        1980,
        inNorthHem,
        &climateOutput
    );

    EXPECT_DOUBLE_EQ(climateOutput.meanTempMon_C[Jan][0], 1.);
    EXPECT_DOUBLE_EQ(climateOutput.maxTempMon_C[Jan][0], 1.);
    EXPECT_DOUBLE_EQ(climateOutput.minTempMon_C[Jan][0], 1.);
    EXPECT_DOUBLE_EQ(climateOutput.PPTMon_cm[Jan][0], 31.);
    EXPECT_DOUBLE_EQ(climateOutput.PPTMon_cm[Feb][0], 29.);
    EXPECT_DOUBLE_EQ(climateOutput.PPTMon_cm[Feb][1], 28.);
    EXPECT_DOUBLE_EQ(climateOutput.PPT_cm[0], 366.);
    EXPECT_DOUBLE_EQ(climateOutput.PPT_cm[1], 365.);
    EXPECT_DOUBLE_EQ(climateOutput.meanTemp_C[0], 1.);

    // Climate variables used for C4 grass cover
    // (stdev of one value is undefined)
    EXPECT_DOUBLE_EQ(climateOutput.minTemp7thMon_C[0], 1.);
    EXPECT_DOUBLE_EQ(climateOutput.frostFree_days[0], 366.);
    EXPECT_DOUBLE_EQ(climateOutput.frostFree_days[1], 365.);
    EXPECT_DOUBLE_EQ(climateOutput.ddAbove65F_degday[0], 0.);


    // Climate variables used for cheatgrass cover
    // (stdev of one value is undefined)
    EXPECT_DOUBLE_EQ(climateOutput.PPT7thMon_mm[0], 310.);
    EXPECT_DOUBLE_EQ(climateOutput.meanTempDriestQtr_C[0], 1.);
    EXPECT_DOUBLE_EQ(climateOutput.minTemp2ndMon_C[0], 1.);


    // --- Long-term variables (aggregated across years) ------
    averageClimateAcrossYears(&climateOutput, n_years, &climateAverages);

    EXPECT_DOUBLE_EQ(climateAverages.meanTempMon_C[Jan], 1.);
    EXPECT_DOUBLE_EQ(climateAverages.maxTempMon_C[Jan], 1.);
    EXPECT_DOUBLE_EQ(climateAverages.minTempMon_C[Jan], 1.);
    EXPECT_DOUBLE_EQ(climateAverages.PPTMon_cm[Jan], 31.);
    EXPECT_DOUBLE_EQ(climateAverages.PPTMon_cm[Feb], 28.5);
    EXPECT_DOUBLE_EQ(climateAverages.PPTMon_cm[Dec], 31);
    EXPECT_DOUBLE_EQ(climateAverages.PPT_cm, 365.5);
    EXPECT_DOUBLE_EQ(climateAverages.meanTemp_C, 1.);

    // Climate variables used for C4 grass cover
    EXPECT_DOUBLE_EQ(climateAverages.minTemp7thMon_C, 1.);
    EXPECT_DOUBLE_EQ(climateAverages.frostFree_days, 365.5);
    EXPECT_DOUBLE_EQ(climateAverages.ddAbove65F_degday, 0.);
    EXPECT_DOUBLE_EQ(climateAverages.sdC4[0], 0.);
    EXPECT_NEAR(climateAverages.sdC4[1], .7071067, tol6); // sd(366, 365)
    EXPECT_DOUBLE_EQ(climateAverages.sdC4[2], 0.);

    // Climate variables used for cheatgrass cover
    EXPECT_DOUBLE_EQ(climateAverages.PPT7thMon_mm, 310.);
    EXPECT_DOUBLE_EQ(climateAverages.meanTempDriestQtr_C, 1.);
    EXPECT_DOUBLE_EQ(climateAverages.minTemp2ndMon_C, 1.);
    EXPECT_DOUBLE_EQ(climateAverages.sdCheatgrass[0], 0.);
    EXPECT_DOUBLE_EQ(climateAverages.sdCheatgrass[1], 0.);
    EXPECT_DOUBLE_EQ(climateAverages.sdCheatgrass[2], 0.);


    // ------ Reset and deallocate
    deallocateClimateStructs(&climateOutput, &climateAverages);
    deallocateAllWeather(&allHist);
}

TEST_F(
    WeatherFixtureTest, ClimateVariableAverageTemperatureOfDriestQuarterTest
) {

    double const monthlyPPT[MAX_MONTHS] = {
        .5, .5, .1, .4, .9, 1.0, 1.2, 6.5, 7.5, 1.2, 4., .6
    };
    double const monthlyTemp[MAX_MONTHS] = {
        -3.2, -.4, 1.2, 3.5, 7.5, 4.5, 6.5, 8.2, 2.0, 3., .1, -.3
    };
    double result[2]; // 2 = max number of years in test

    int month;
    int year;

    double **PPTMon_cm;
    PPTMon_cm = new double *[MAX_MONTHS];

    double **meanTempMon_C = new double *[MAX_MONTHS];

    Bool const inNorthHem = swTRUE;

    for (month = 0; month < MAX_MONTHS; month++) {
        PPTMon_cm[month] = new double[2];
        meanTempMon_C[month] = new double[2];
        for (year = 0; year < 2; year++) {

            PPTMon_cm[month][year] = monthlyPPT[month];
            meanTempMon_C[month][year] = monthlyTemp[month];
        }
    }


    // ------ Test for one year ------
    findDriestQtr(1, inNorthHem, result, meanTempMon_C, PPTMon_cm);

    // Value 1.433333... is the average temperature of the driest quarter of the
    // year In this case, the driest quarter is February-April
    EXPECT_NEAR(result[0], 1.4333333333333333, tol9);


    // ------ Test for two years ------
    findDriestQtr(2, inNorthHem, result, meanTempMon_C, PPTMon_cm);

    EXPECT_NEAR(result[0], 1.4333333333333333, tol9);
    EXPECT_NEAR(result[1], 1.4333333333333333, tol9);


    // ------ Test when there are multiple driest quarters ------
    for (month = 0; month < MAX_MONTHS; month++) {
        for (year = 0; year < 2; year++) {
            PPTMon_cm[month][year] = 1.;
        }
    }

    findDriestQtr(1, inNorthHem, result, meanTempMon_C, PPTMon_cm);

    // Expect that the driest quarter that occurs first
    // among all driest quarters is used
    // Here, the first driest quarter is centered on Jan with Dec-Feb
    // with average temp of -1.3 C
    EXPECT_NEAR(result[0], -1.3, tol9);


    // ------ Clean up
    for (month = 0; month < MAX_MONTHS; month++) {
        delete[] PPTMon_cm[month];
        delete[] meanTempMon_C[month];
    }

    delete[] PPTMon_cm;
    delete[] meanTempMon_C;
}

TEST_F(WeatherFixtureTest, WeatherReadInitialization) {

    SW_WTH_read(
        SW_Run.WeatherIn,
        &SW_Run.RunIn.weathRunAllHist,
        &SW_Run.RunIn.SkyRunIn,
        SW_Run.ModelIn,
        SW_Run.RunIn.ModelRunIn.elevation,
        swTRUE,
        SW_Domain.SW_ConstInfo.ModelSim.cum_monthdays,
        SW_Domain.SW_ConstInfo.ModelSim.days_in_month,
        &LogInfo
    );
    sw_fail_on_error(
        ROOT_PROC, &LogInfo
    ); // exit test program if unexpected error

    EXPECT_FLOAT_EQ(SW_Run.RunIn.weathRunAllHist[0].temp_max[0], -.52);
}

TEST_F(WeatherFixtureTest, WeatherMonthlyInputPrioritization) {
    /*
       This section covers the correct prioritization of monthly input values
       instead of daily read-in values
     */

    // Initialize any variables
    int const yearIndex = 0;
    int const midJanDay = 14;

    /* Test if monthly values are not being used */
    SW_WTH_setup(
        SW_Run.WeatherIn,
        SW_Domain.SW_PathInputs.txtInFiles,
        SW_Domain.SW_PathInputs.txtWeatherPrefix,
        &LogInfo
    );
    sw_fail_on_error(
        ROOT_PROC, &LogInfo
    ); // exit test program if unexpected error

    // Read in all weather
    SW_WTH_read(
        SW_Run.WeatherIn,
        &SW_Run.RunIn.weathRunAllHist,
        &SW_Run.RunIn.SkyRunIn,
        SW_Run.ModelIn,
        SW_Run.RunIn.ModelRunIn.elevation,
        swTRUE,
        SW_Domain.SW_ConstInfo.ModelSim.cum_monthdays,
        SW_Domain.SW_ConstInfo.ModelSim.days_in_month,
        &LogInfo
    );
    sw_fail_on_error(
        ROOT_PROC, &LogInfo
    ); // exit test program if unexpected error

    // Test the middle of January in year 1980 and see if it's not equal to
    // SW_Run.RunIn.SkyRunIn.r_humidity[0],
    // SW_Run.RunIn.SkyRunIn.cloudcov[0], and
    // SW_Run.RunIn.SkyRunIn.windspeed[0] Note: Daily interpolated values in
    // the middle of a month are equal to the original monthly values from which
    // they were interpolated
    EXPECT_NEAR(
        SW_Run.RunIn.weathRunAllHist[yearIndex].r_humidity_daily[midJanDay],
        SW_Run.RunIn.SkyRunIn.r_humidity[0],
        tol6
    );
    EXPECT_NEAR(
        SW_Run.RunIn.weathRunAllHist[yearIndex].cloudcov_daily[midJanDay],
        SW_Run.RunIn.SkyRunIn.cloudcov[0],
        tol6
    );
    EXPECT_NEAR(
        SW_Run.RunIn.weathRunAllHist[yearIndex].windspeed_daily[midJanDay],
        SW_Run.RunIn.SkyRunIn.windspeed[0],
        tol6
    );
}

TEST_F(WeatherFixtureTest, WeatherMonthlyScalingParameters) {
    sw_init_logs(NULL, &LogInfo);

    SW_WEATHER_INPUTS testWeather;
    SW_WEATHER_HIST *allHist = NULL;

    unsigned int const nYears = 1;
    TimeInt const testYear = SW_Run.WeatherIn->startYear;
    TimeInt days_in_month[MAX_MONTHS];
    TimeInt cum_monthdays[MAX_MONTHS];

    Time_init_model(days_in_month);
    Time_new_year(testYear, days_in_month, cum_monthdays);


    //--- Check scale_temp_max ------
    deallocateAllWeather(&allHist);
    SW_WTH_allocateAllWeather(&allHist, nYears, &LogInfo);
    sw_fail_on_error(
        ROOT_PROC, &LogInfo
    ); // exit test program if unexpected error
    memcpy(&allHist[0], &SW_Run.RunIn.weathRunAllHist[0], sizeof(allHist[0]));

    memcpy(&testWeather, SW_Run.WeatherIn, sizeof(testWeather));
    testWeather.scale_temp_max[0] = 2;

    scaleYearlyWeather(
        allHist,
        testYear,
        nYears,
        testWeather.scale_temp_max,
        testWeather.scale_temp_min,
        testWeather.scale_precip,
        testWeather.scale_skyCover,
        testWeather.scale_wind,
        testWeather.scale_rH,
        testWeather.scale_actVapPress,
        testWeather.scale_shortWaveRad,
        cum_monthdays,
        days_in_month
    );

    EXPECT_NE(
        SW_Run.RunIn.weathRunAllHist[0].temp_max[0], allHist[0].temp_max[0]
    );
    EXPECT_DOUBLE_EQ(SW_Run.RunIn.weathRunAllHist[0].ppt[0], allHist[0].ppt[0]);


    //--- Check scale_temp_min ------
    deallocateAllWeather(&allHist);
    SW_WTH_allocateAllWeather(&allHist, nYears, &LogInfo);
    sw_fail_on_error(
        ROOT_PROC, &LogInfo
    ); // exit test program if unexpected error
    memcpy(&allHist[0], &SW_Run.RunIn.weathRunAllHist[0], sizeof(allHist[0]));

    memcpy(&testWeather, SW_Run.WeatherIn, sizeof(testWeather));
    testWeather.scale_temp_min[0] = 2;

    scaleYearlyWeather(
        allHist,
        testYear,
        nYears,
        testWeather.scale_temp_max,
        testWeather.scale_temp_min,
        testWeather.scale_precip,
        testWeather.scale_skyCover,
        testWeather.scale_wind,
        testWeather.scale_rH,
        testWeather.scale_actVapPress,
        testWeather.scale_shortWaveRad,
        cum_monthdays,
        days_in_month
    );

    EXPECT_NE(
        SW_Run.RunIn.weathRunAllHist[0].temp_min[0], allHist[0].temp_min[0]
    );
    EXPECT_DOUBLE_EQ(SW_Run.RunIn.weathRunAllHist[0].ppt[0], allHist[0].ppt[0]);


    //--- Check scale_precip ------
    deallocateAllWeather(&allHist);
    SW_WTH_allocateAllWeather(&allHist, nYears, &LogInfo);
    sw_fail_on_error(
        ROOT_PROC, &LogInfo
    ); // exit test program if unexpected error
    memcpy(&allHist[0], &SW_Run.RunIn.weathRunAllHist[0], sizeof(allHist[0]));

    memcpy(&testWeather, SW_Run.WeatherIn, sizeof(testWeather));
    testWeather.scale_precip[0] = 2;

    scaleYearlyWeather(
        allHist,
        testYear,
        nYears,
        testWeather.scale_temp_max,
        testWeather.scale_temp_min,
        testWeather.scale_precip,
        testWeather.scale_skyCover,
        testWeather.scale_wind,
        testWeather.scale_rH,
        testWeather.scale_actVapPress,
        testWeather.scale_shortWaveRad,
        cum_monthdays,
        days_in_month
    );

    EXPECT_NE(SW_Run.RunIn.weathRunAllHist[0].ppt[0], allHist[0].ppt[0]);
    EXPECT_DOUBLE_EQ(
        SW_Run.RunIn.weathRunAllHist[0].temp_max[0], allHist[0].temp_max[0]
    );


    //--- Check scale_skyCover ------
    deallocateAllWeather(&allHist);
    SW_WTH_allocateAllWeather(&allHist, nYears, &LogInfo);
    sw_fail_on_error(
        ROOT_PROC, &LogInfo
    ); // exit test program if unexpected error
    memcpy(&allHist[0], &SW_Run.RunIn.weathRunAllHist[0], sizeof(allHist[0]));

    memcpy(&testWeather, SW_Run.WeatherIn, sizeof(testWeather));
    testWeather.scale_skyCover[0] = 2;

    scaleYearlyWeather(
        allHist,
        testYear,
        nYears,
        testWeather.scale_temp_max,
        testWeather.scale_temp_min,
        testWeather.scale_precip,
        testWeather.scale_skyCover,
        testWeather.scale_wind,
        testWeather.scale_rH,
        testWeather.scale_actVapPress,
        testWeather.scale_shortWaveRad,
        cum_monthdays,
        days_in_month
    );

    EXPECT_NE(
        SW_Run.RunIn.weathRunAllHist[0].cloudcov_daily[0],
        allHist[0].cloudcov_daily[0]
    );
    EXPECT_DOUBLE_EQ(SW_Run.RunIn.weathRunAllHist[0].ppt[0], allHist[0].ppt[0]);


    //--- Check scale_wind ------
    deallocateAllWeather(&allHist);
    SW_WTH_allocateAllWeather(&allHist, nYears, &LogInfo);
    sw_fail_on_error(
        ROOT_PROC, &LogInfo
    ); // exit test program if unexpected error
    memcpy(&allHist[0], &SW_Run.RunIn.weathRunAllHist[0], sizeof(allHist[0]));

    memcpy(&testWeather, SW_Run.WeatherIn, sizeof(testWeather));
    testWeather.scale_wind[0] = 2;

    scaleYearlyWeather(
        allHist,
        testYear,
        nYears,
        testWeather.scale_temp_max,
        testWeather.scale_temp_min,
        testWeather.scale_precip,
        testWeather.scale_skyCover,
        testWeather.scale_wind,
        testWeather.scale_rH,
        testWeather.scale_actVapPress,
        testWeather.scale_shortWaveRad,
        cum_monthdays,
        days_in_month
    );

    EXPECT_NE(
        SW_Run.RunIn.weathRunAllHist[0].windspeed_daily[0],
        allHist[0].windspeed_daily[0]
    );
    EXPECT_DOUBLE_EQ(SW_Run.RunIn.weathRunAllHist[0].ppt[0], allHist[0].ppt[0]);


    //--- Check scale_rH ------
    deallocateAllWeather(&allHist);
    SW_WTH_allocateAllWeather(&allHist, nYears, &LogInfo);
    sw_fail_on_error(
        ROOT_PROC, &LogInfo
    ); // exit test program if unexpected error
    memcpy(&allHist[0], &SW_Run.RunIn.weathRunAllHist[0], sizeof(allHist[0]));

    memcpy(&testWeather, SW_Run.WeatherIn, sizeof(testWeather));
    testWeather.scale_rH[0] = 2;

    scaleYearlyWeather(
        allHist,
        testYear,
        nYears,
        testWeather.scale_temp_max,
        testWeather.scale_temp_min,
        testWeather.scale_precip,
        testWeather.scale_skyCover,
        testWeather.scale_wind,
        testWeather.scale_rH,
        testWeather.scale_actVapPress,
        testWeather.scale_shortWaveRad,
        cum_monthdays,
        days_in_month
    );

    EXPECT_NE(
        SW_Run.RunIn.weathRunAllHist[0].r_humidity_daily[0],
        allHist[0].r_humidity_daily[0]
    );
    EXPECT_DOUBLE_EQ(SW_Run.RunIn.weathRunAllHist[0].ppt[0], allHist[0].ppt[0]);


    //--- Check scale_actVapPress ------
    deallocateAllWeather(&allHist);
    SW_WTH_allocateAllWeather(&allHist, nYears, &LogInfo);
    sw_fail_on_error(
        ROOT_PROC, &LogInfo
    ); // exit test program if unexpected error
    memcpy(&allHist[0], &SW_Run.RunIn.weathRunAllHist[0], sizeof(allHist[0]));

    memcpy(&testWeather, SW_Run.WeatherIn, sizeof(testWeather));
    testWeather.scale_actVapPress[0] = 2;

    scaleYearlyWeather(
        allHist,
        testYear,
        nYears,
        testWeather.scale_temp_max,
        testWeather.scale_temp_min,
        testWeather.scale_precip,
        testWeather.scale_skyCover,
        testWeather.scale_wind,
        testWeather.scale_rH,
        testWeather.scale_actVapPress,
        testWeather.scale_shortWaveRad,
        cum_monthdays,
        days_in_month
    );

    EXPECT_NE(
        SW_Run.RunIn.weathRunAllHist[0].actualVaporPressure[0],
        allHist[0].actualVaporPressure[0]
    );
    EXPECT_DOUBLE_EQ(SW_Run.RunIn.weathRunAllHist[0].ppt[0], allHist[0].ppt[0]);


    // ------ Deallocate
    deallocateAllWeather(&allHist);
}

TEST_F(WeatherFixtureTest, WeatherScalingHumidityConsistency) {
    // Actual vapor pressure (used by solar_radiation() and petfunc())
    // and relative humidity remain consistent if temperature,
    // relative humidity, or vapor pressure are scaled
    sw_init_logs(NULL, &LogInfo);

    SW_WEATHER_INPUTS testWeather;
    SW_WEATHER_HIST *allHist = NULL;

    unsigned int const nYears = 1;
    int const day = 0;          // January (scaling is applied to month 0)
    int const dayUnscaled = 40; // February (no scaling)
    TimeInt const testYear = SW_Run.WeatherIn->startYear;
    TimeInt days_in_month[MAX_MONTHS];
    TimeInt cum_monthdays[MAX_MONTHS];
    SW_WEATHER_HIST const *refHist = &SW_Run.RunIn.weathRunAllHist[0];
    double tempSlope;
    int k;

    Time_init_model(days_in_month);
    Time_new_year(testYear, days_in_month, cum_monthdays);

    ASSERT_FALSE(missing(refHist->r_humidity_daily[day]));
    ASSERT_FALSE(missing(refHist->actualVaporPressure[day]));
    ASSERT_FALSE(missing(refHist->temp_avg[day]));

    // k = 0: scale_temp_max and scale_temp_min (+2 C)
    // k = 1: scale_rH (-10 %)
    // k = 2: scale_actVapPress (x 0.8)
    // k = 3: all of the above (order: temperature, relative humidity,
    //        vapor pressure)
    for (k = 0; k < 4; k++) {
        deallocateAllWeather(&allHist);
        SW_WTH_allocateAllWeather(&allHist, nYears, &LogInfo);
        sw_fail_on_error(
            ROOT_PROC, &LogInfo
        ); // exit test program if unexpected error
        memcpy(&allHist[0], refHist, sizeof(allHist[0]));

        memcpy(&testWeather, SW_Run.WeatherIn, sizeof(testWeather));
        if (k == 0) {
            testWeather.scale_temp_max[0] = 2.;
            testWeather.scale_temp_min[0] = 2.;
        } else if (k == 1) {
            testWeather.scale_rH[0] = -10.;
        } else if (k == 2) {
            testWeather.scale_actVapPress[0] = 0.8;
        } else {
            testWeather.scale_temp_max[0] = 2.;
            testWeather.scale_temp_min[0] = 2.;
            testWeather.scale_rH[0] = -10.;
            testWeather.scale_actVapPress[0] = 0.8;
        }

        scaleYearlyWeather(
            allHist,
            testYear,
            nYears,
            testWeather.scale_temp_max,
            testWeather.scale_temp_min,
            testWeather.scale_precip,
            testWeather.scale_skyCover,
            testWeather.scale_wind,
            testWeather.scale_rH,
            testWeather.scale_actVapPress,
            testWeather.scale_shortWaveRad,
            cum_monthdays,
            days_in_month
        );

        if (k == 0) {
            // Relative humidity is held constant under temperature scaling
            EXPECT_DOUBLE_EQ(
                allHist[0].r_humidity_daily[day], refHist->r_humidity_daily[day]
            );
            EXPECT_NEAR(
                allHist[0].temp_avg[day], refHist->temp_avg[day] + 2., tol9
            );

        } else if (k == 1) {
            EXPECT_NEAR(
                allHist[0].r_humidity_daily[day],
                fmax(0., refHist->r_humidity_daily[day] - 10.),
                tol9
            );

        } else if (k == 2) {
            EXPECT_NEAR(
                allHist[0].actualVaporPressure[day],
                0.8 * refHist->actualVaporPressure[day],
                tol9
            );

        } else {
            // Relative humidity: shifted, then held constant under
            // temperature scaling, then follows scaled vapor pressure
            EXPECT_NEAR(
                allHist[0].r_humidity_daily[day],
                fmin(
                    100., 0.8 * fmax(0., refHist->r_humidity_daily[day] - 10.)
                ),
                tol9
            );
        }

        // Days in months without scaling remain unchanged
        EXPECT_DOUBLE_EQ(
            allHist[0].actualVaporPressure[dayUnscaled],
            refHist->actualVaporPressure[dayUnscaled]
        );
        EXPECT_DOUBLE_EQ(
            allHist[0].r_humidity_daily[dayUnscaled],
            refHist->r_humidity_daily[dayUnscaled]
        );

        // Vapor pressure changes (and thus the humidity seen by
        // solar_radiation() and petfunc())
        EXPECT_GT(
            fabs(
                allHist[0].actualVaporPressure[day] -
                refHist->actualVaporPressure[day]
            ),
            tol6
        );

        // Vapor pressure and relative humidity are consistent
        EXPECT_NEAR(
            allHist[0].actualVaporPressure[day],
            allHist[0].r_humidity_daily[day] / 100. *
                svp(allHist[0].temp_avg[day], swFALSE, &tempSlope),
            tol9
        );
    }

    // ------ Deallocate
    deallocateAllWeather(&allHist);
}

TEST_F(WeatherFixtureTest, WeatherInputDewpointOnly) {
    // Dew point temperature as the only daily humidity input
    sw_init_logs(NULL, &LogInfo);

    SW_WEATHER_HIST *allHist = NULL;
    double ***tempWeatherHist = NULL;
    Bool inputFlags[MAX_INPUT_COLUMNS];
    Bool fixWeatherData[NFIXWEATHER];
    int k;
    int doy;

    double const tmax[] = {5., 25., -2.};
    double const tmin[] = {-5., 10., -12.};
    double const tdew[] = {-8., 8., -10.};

    for (k = 0; k < MAX_INPUT_COLUMNS; k++) {
        inputFlags[k] = swFALSE;
    }
    for (k = 0; k < NFIXWEATHER; k++) {
        fixWeatherData[k] = swFALSE;
    }
    inputFlags[TEMP_MAX] = swTRUE;
    inputFlags[TEMP_MIN] = swTRUE;
    inputFlags[PPT] = swTRUE;
    inputFlags[TEMP_DEWPOINT] = swTRUE;


    // Daily flag of dew point temperature is kept if humidity is not monthly
    check_and_update_dailyInputFlags(
        swFALSE, swFALSE, swFALSE, inputFlags, &LogInfo
    );
    sw_fail_on_error(ROOT_PROC, &LogInfo);
    EXPECT_TRUE(inputFlags[TEMP_DEWPOINT]);


    // Set up one year of daily weather with values for three days
    SW_WTH_allocateAllWeather(&allHist, 1, &LogInfo);
    sw_fail_on_error(ROOT_PROC, &LogInfo);
    allocate_temp_weather(1, 1, &tempWeatherHist, &LogInfo);
    if (LogInfo.stopRun != 0u) {
        deallocateAllWeather(&allHist);
    }
    sw_fail_on_error(ROOT_PROC, &LogInfo);
    clear_hist_weather(1, &allHist[0], tempWeatherHist[0]);

    for (doy = 0; doy < 3; doy++) {
        tempWeatherHist[0][TEMP_MAX][doy] = tmax[doy];
        tempWeatherHist[0][TEMP_MIN][doy] = tmin[doy];
        tempWeatherHist[0][PPT][doy] = 0.;
        tempWeatherHist[0][TEMP_DEWPOINT][doy] = tdew[doy];
    }

    SW_WTH_setWeatherValues(
        1980,
        1,
        inputFlags,
        fixWeatherData,
        swFALSE,
        tempWeatherHist,
        SW_Run.RunIn.ModelRunIn.elevation,
        0,
        allHist,
        &LogInfo
    );
    deallocate_temp_weather(1, &tempWeatherHist);
    if (LogInfo.stopRun != 0u) {
        deallocateAllWeather(&allHist);
    }
    sw_fail_on_error(ROOT_PROC, &LogInfo);

    for (doy = 0; doy < 3; doy++) {
        // Actual vapor pressure from dew point temperature
        EXPECT_NEAR(
            allHist[0].actualVaporPressure[doy],
            actualVaporPressure3(tdew[doy]),
            tol9
        );

        // Relative humidity from actual vapor pressure (relative to water)
        EXPECT_NEAR(
            allHist[0].r_humidity_daily[doy],
            relativeHumidity1(
                allHist[0].actualVaporPressure[doy],
                (tmax[doy] + tmin[doy]) / 2.
            ),
            tol9
        );
    }

    deallocateAllWeather(&allHist);


    // Daily flag of dew point temperature is turned off if humidity is monthly
    inputFlags[TEMP_DEWPOINT] = swTRUE;
    check_and_update_dailyInputFlags(
        swFALSE, swTRUE, swFALSE, inputFlags, &LogInfo
    );
    sw_fail_on_error(ROOT_PROC, &LogInfo);
    EXPECT_FALSE(inputFlags[TEMP_DEWPOINT]);
}

TEST_F(WeatherFixtureTest, WeatherInputHumidityPrecedence) {
    // Order of precedence of humidity inputs for actual vapor pressure:
    // actual vapor pressure, dewpoint temperature, specific humidity,
    // min/max relative humidity, relative humidity;
    // dewpoint temperature and specific humidity are used only on days
    // when they are not missing
    sw_init_logs(NULL, &LogInfo);

    SW_WEATHER_HIST *allHist = NULL;
    double ***tempWeatherHist = NULL;
    Bool inputFlags[MAX_INPUT_COLUMNS];
    Bool fixWeatherData[NFIXWEATHER];
    int k;
    int doy;
    double const elevation = SW_Run.RunIn.ModelRunIn.elevation;
    double expVP;

    // Day 0: all inputs; day 1: missing dewpoint; day 2: missing dewpoint
    // and specific humidity; day 3: missing dewpoint, specific humidity,
    // and min/max relative humidity
    double const tmax[] = {25., 25., 25., 25.};
    double const tmin[] = {5., 5., 5., 5.};
    double const tdew[] = {2., SW_MISSING, SW_MISSING, SW_MISSING};
    double const huss[] = {5., 5., SW_MISSING, SW_MISSING};
    double const rhmax[] = {90., 90., 90., SW_MISSING};
    double const rhmin[] = {30., 30., 30., SW_MISSING};
    double const rh[] = {50., 50., 50., 50.};
    int const nDays = 4;

    for (k = 0; k < MAX_INPUT_COLUMNS; k++) {
        inputFlags[k] = swFALSE;
    }
    for (k = 0; k < NFIXWEATHER; k++) {
        fixWeatherData[k] = swFALSE;
    }
    inputFlags[TEMP_MAX] = swTRUE;
    inputFlags[TEMP_MIN] = swTRUE;
    inputFlags[PPT] = swTRUE;
    inputFlags[TEMP_DEWPOINT] = swTRUE;
    inputFlags[SPEC_HUMID] = swTRUE;
    inputFlags[REL_HUMID_MAX] = swTRUE;
    inputFlags[REL_HUMID_MIN] = swTRUE;
    inputFlags[REL_HUMID] = swTRUE;

    SW_WTH_allocateAllWeather(&allHist, 1, &LogInfo);
    sw_fail_on_error(ROOT_PROC, &LogInfo);
    allocate_temp_weather(1, 1, &tempWeatherHist, &LogInfo);
    if (LogInfo.stopRun != 0u) {
        deallocateAllWeather(&allHist);
    }
    sw_fail_on_error(ROOT_PROC, &LogInfo);
    clear_hist_weather(1, &allHist[0], tempWeatherHist[0]);

    for (doy = 0; doy < nDays; doy++) {
        tempWeatherHist[0][TEMP_MAX][doy] = tmax[doy];
        tempWeatherHist[0][TEMP_MIN][doy] = tmin[doy];
        tempWeatherHist[0][PPT][doy] = 0.;
        tempWeatherHist[0][TEMP_DEWPOINT][doy] = tdew[doy];
        tempWeatherHist[0][SPEC_HUMID][doy] = huss[doy];
        tempWeatherHist[0][REL_HUMID_MAX][doy] = rhmax[doy];
        tempWeatherHist[0][REL_HUMID_MIN][doy] = rhmin[doy];
        tempWeatherHist[0][REL_HUMID][doy] = rh[doy];
    }

    SW_WTH_setWeatherValues(
        1980,
        1,
        inputFlags,
        fixWeatherData,
        swFALSE,
        tempWeatherHist,
        elevation,
        0,
        allHist,
        &LogInfo
    );
    deallocate_temp_weather(1, &tempWeatherHist);
    if (LogInfo.stopRun != 0u) {
        deallocateAllWeather(&allHist);
    }
    sw_fail_on_error(ROOT_PROC, &LogInfo);

    for (doy = 0; doy < nDays; doy++) {
        switch (doy) {
        case 0:
            expVP = actualVaporPressure3(tdew[doy]);
            break;
        case 1:
            expVP = actualVaporPressure4(huss[doy], elevation);
            break;
        case 2:
            expVP = actualVaporPressure2(
                rhmax[doy], rhmin[doy], tmax[doy], tmin[doy], swFALSE
            );
            break;
        default:
            expVP = actualVaporPressure1(
                rh[doy], (tmax[doy] + tmin[doy]) / 2., swFALSE
            );
            break;
        }

        EXPECT_NEAR(allHist[0].actualVaporPressure[doy], expVP, tol9);

        // Relative humidity from actual vapor pressure (relative to water)
        EXPECT_NEAR(
            allHist[0].r_humidity_daily[doy],
            fmin(100., relativeHumidity1(expVP, (tmax[doy] + tmin[doy]) / 2.)),
            tol9
        );
    }

    deallocateAllWeather(&allHist);
}

// Helper: set up daily input flags (all off) and weather fixes (all off)
void initHumidityTestFlags(
    Bool inputFlags[MAX_INPUT_COLUMNS], Bool fixWeatherData[NFIXWEATHER]
) {
    int k;

    for (k = 0; k < MAX_INPUT_COLUMNS; k++) {
        inputFlags[k] = swFALSE;
    }
    for (k = 0; k < NFIXWEATHER; k++) {
        fixWeatherData[k] = swFALSE;
    }
    inputFlags[TEMP_MAX] = swTRUE;
    inputFlags[TEMP_MIN] = swTRUE;
    inputFlags[PPT] = swTRUE;
}

TEST_F(WeatherFixtureTest, WeatherHumiditySupersaturatedScaling) {
    // Vapor pressure can exceed saturation at mean air temperature
    // (here, from max/min relative humidity); relative humidity is limited
    // to 100%; scaling changes vapor pressure continuously
    sw_init_logs(NULL, &LogInfo);

    SW_WEATHER_HIST *allHist = NULL;
    SW_WEATHER_HIST *refHist = NULL;
    double ***tempWeatherHist = NULL;
    Bool inputFlags[MAX_INPUT_COLUMNS];
    Bool fixWeatherData[NFIXWEATHER];
    TimeInt days_in_month[MAX_MONTHS];
    TimeInt cum_monthdays[MAX_MONTHS];
    double scaleTmax[MAX_MONTHS];
    double scaleTmin[MAX_MONTHS];
    double scalePPT[MAX_MONTHS];
    double scaleSky[MAX_MONTHS];
    double scaleWind[MAX_MONTHS];
    double scaleRH[MAX_MONTHS];
    double scaleVP[MAX_MONTHS];
    double scaleRSDS[MAX_MONTHS];
    double tempSlope;
    double rHuncapped;
    double vp0;
    int k;
    int m;
    int const day = 0; // January
    double const tmax = 30.;
    double const tmin = 10.;
    double const tavg = (tmax + tmin) / 2.;

    Time_init_model(days_in_month);
    Time_new_year(1980, days_in_month, cum_monthdays);

    initHumidityTestFlags(inputFlags, fixWeatherData);
    inputFlags[REL_HUMID_MAX] = swTRUE;
    inputFlags[REL_HUMID_MIN] = swTRUE;

    SW_WTH_allocateAllWeather(&allHist, 1, &LogInfo);
    sw_fail_on_error(ROOT_PROC, &LogInfo);
    SW_WTH_allocateAllWeather(&refHist, 1, &LogInfo);
    if (LogInfo.stopRun != 0u) {
        deallocateAllWeather(&allHist);
    }
    sw_fail_on_error(ROOT_PROC, &LogInfo);
    allocate_temp_weather(1, 1, &tempWeatherHist, &LogInfo);
    if (LogInfo.stopRun != 0u) {
        deallocateAllWeather(&allHist);
        deallocateAllWeather(&refHist);
    }
    sw_fail_on_error(ROOT_PROC, &LogInfo);
    clear_hist_weather(1, &refHist[0], tempWeatherHist[0]);

    tempWeatherHist[0][TEMP_MAX][day] = tmax;
    tempWeatherHist[0][TEMP_MIN][day] = tmin;
    tempWeatherHist[0][PPT][day] = 0.;
    tempWeatherHist[0][REL_HUMID_MAX][day] = 100.;
    tempWeatherHist[0][REL_HUMID_MIN][day] = 90.;

    SW_WTH_setWeatherValues(
        1980,
        1,
        inputFlags,
        fixWeatherData,
        swFALSE,
        tempWeatherHist,
        SW_Run.RunIn.ModelRunIn.elevation,
        0,
        refHist,
        &LogInfo
    );
    deallocate_temp_weather(1, &tempWeatherHist);
    if (LogInfo.stopRun != 0u) {
        deallocateAllWeather(&allHist);
        deallocateAllWeather(&refHist);
    }
    sw_fail_on_error(ROOT_PROC, &LogInfo);

    // Supersaturated vapor pressure is kept; relative humidity is limited
    vp0 = actualVaporPressure2(100., 90., tmax, tmin, swFALSE);
    EXPECT_NEAR(refHist[0].actualVaporPressure[day], vp0, tol9);
    EXPECT_GT(vp0, svp(tavg, swFALSE, &tempSlope));
    EXPECT_DOUBLE_EQ(refHist[0].r_humidity_daily[day], 100.);
    rHuncapped = relativeHumidity1(vp0, tavg);


    // k = 0: small temperature scaling (+0.001 C)
    // k = 1: temperature scaling (+2 C)
    // k = 2: relative humidity scaling (+10 %)
    // k = 3: relative humidity scaling (-10 %)
    // k = 4: vapor pressure scaling (x 0.8)
    for (k = 0; k < 5; k++) {
        for (m = 0; m < MAX_MONTHS; m++) {
            scaleTmax[m] = 0.;
            scaleTmin[m] = 0.;
            scalePPT[m] = 1.;
            scaleSky[m] = 0.;
            scaleWind[m] = 1.;
            scaleRH[m] = 0.;
            scaleVP[m] = 1.;
            scaleRSDS[m] = 1.;
        }

        switch (k) {
        case 0:
            scaleTmax[0] = scaleTmin[0] = 0.001;
            break;
        case 1:
            scaleTmax[0] = scaleTmin[0] = 2.;
            break;
        case 2:
            scaleRH[0] = 10.;
            break;
        case 3:
            scaleRH[0] = -10.;
            break;
        default:
            scaleVP[0] = 0.8;
            break;
        }

        memcpy(&allHist[0], &refHist[0], sizeof(allHist[0]));

        scaleYearlyWeather(
            allHist,
            1980,
            1,
            scaleTmax,
            scaleTmin,
            scalePPT,
            scaleSky,
            scaleWind,
            scaleRH,
            scaleVP,
            scaleRSDS,
            cum_monthdays,
            days_in_month
        );

        switch (k) {
        case 0:
        case 1:
            // Relative humidity (incl. supersaturation) is held constant
            EXPECT_NEAR(
                allHist[0].actualVaporPressure[day],
                vp0 * svp(tavg + scaleTmax[0], swFALSE, &tempSlope) /
                    svp(tavg, swFALSE, &tempSlope),
                tol9
            );
            EXPECT_NEAR(
                relativeHumidity1(
                    allHist[0].actualVaporPressure[day],
                    allHist[0].temp_avg[day]
                ),
                rHuncapped,
                tol6
            );
            EXPECT_DOUBLE_EQ(allHist[0].r_humidity_daily[day], 100.);
            break;

        case 2:
            // Supersaturation is not increased further
            EXPECT_NEAR(allHist[0].actualVaporPressure[day], vp0, tol9);
            EXPECT_DOUBLE_EQ(allHist[0].r_humidity_daily[day], 100.);
            break;

        case 3:
            // Shift of (uncapped) relative humidity
            EXPECT_NEAR(
                allHist[0].actualVaporPressure[day],
                (rHuncapped - 10.) / 100. * svp(tavg, swFALSE, &tempSlope),
                tol9
            );
            EXPECT_NEAR(
                allHist[0].r_humidity_daily[day],
                fmin(100., rHuncapped - 10.),
                tol9
            );
            break;

        default:
            EXPECT_NEAR(allHist[0].actualVaporPressure[day], 0.8 * vp0, tol9);
            EXPECT_NEAR(
                allHist[0].r_humidity_daily[day],
                fmin(100., 0.8 * rHuncapped),
                tol9
            );
            break;
        }

        // Small scaling results in small changes (no discontinuity)
        if (k == 0) {
            EXPECT_LT(
                fabs(allHist[0].actualVaporPressure[day] / vp0 - 1.), 1e-4
            );
        }
    }

    deallocateAllWeather(&allHist);
    deallocateAllWeather(&refHist);
}

TEST_F(WeatherFixtureTest, WeatherHumidityImputationLOCF) {
    // A day with observed vapor pressure but missing maximum air temperature:
    // LOCF imputes air temperature, keeps the observed vapor pressure, and
    // calculates relative humidity from it (instead of LOCF of relative
    // humidity); subsequent temperature scaling keeps relative humidity
    // constant, i.e., scales the observed vapor pressure
    sw_init_logs(NULL, &LogInfo);

    SW_WEATHER_HIST *allHist = NULL;
    double ***tempWeatherHist = NULL;
    Bool inputFlags[MAX_INPUT_COLUMNS];
    Bool fixWeatherData[NFIXWEATHER];
    TimeInt days_in_month[MAX_MONTHS];
    TimeInt cum_monthdays[MAX_MONTHS];
    double scaleTmax[MAX_MONTHS];
    double scaleTmin[MAX_MONTHS];
    double scalePPT[MAX_MONTHS];
    double scaleSky[MAX_MONTHS];
    double scaleWind[MAX_MONTHS];
    double scaleRH[MAX_MONTHS];
    double scaleVP[MAX_MONTHS];
    double scaleRSDS[MAX_MONTHS];
    double tempSlope;
    double expRH;
    int doy;
    int m;
    int const dayGap = 2; // January
    double const tmax = 25.;
    double const tmin = 5.;
    double const tavg = (tmax + tmin) / 2.;
    double const vpGap = 0.5;
    double const vpOther = 1.5;
    unsigned int const nDays = Time_get_lastdoy_y(1980);

    Time_init_model(days_in_month);
    Time_new_year(1980, days_in_month, cum_monthdays);

    initHumidityTestFlags(inputFlags, fixWeatherData);
    inputFlags[ACTUAL_VP] = swTRUE;
    inputFlags[CLOUD_COV] = swTRUE;
    inputFlags[WIND_SPEED] = swTRUE;
    inputFlags[SHORT_WR] = swTRUE;

    SW_WTH_allocateAllWeather(&allHist, 1, &LogInfo);
    sw_fail_on_error(ROOT_PROC, &LogInfo);
    allocate_temp_weather(1, 1, &tempWeatherHist, &LogInfo);
    if (LogInfo.stopRun != 0u) {
        deallocateAllWeather(&allHist);
    }
    sw_fail_on_error(ROOT_PROC, &LogInfo);
    clear_hist_weather(1, &allHist[0], tempWeatherHist[0]);

    for (doy = 0; doy < (int) nDays; doy++) {
        tempWeatherHist[0][TEMP_MAX][doy] = tmax;
        tempWeatherHist[0][TEMP_MIN][doy] = tmin;
        tempWeatherHist[0][PPT][doy] = 0.;
        tempWeatherHist[0][CLOUD_COV][doy] = 50.;
        tempWeatherHist[0][WIND_SPEED][doy] = 2.;
        tempWeatherHist[0][SHORT_WR][doy] = 200.;
        tempWeatherHist[0][ACTUAL_VP][doy] = vpOther;
    }
    tempWeatherHist[0][TEMP_MAX][dayGap] = SW_MISSING;
    tempWeatherHist[0][ACTUAL_VP][dayGap] = vpGap;

    SW_WTH_setWeatherValues(
        1980,
        1,
        inputFlags,
        fixWeatherData,
        swFALSE,
        tempWeatherHist,
        SW_Run.RunIn.ModelRunIn.elevation,
        0,
        allHist,
        &LogInfo
    );
    deallocate_temp_weather(1, &tempWeatherHist);
    if (LogInfo.stopRun != 0u) {
        deallocateAllWeather(&allHist);
    }
    sw_fail_on_error(ROOT_PROC, &LogInfo);

    // Relative humidity cannot be calculated without air temperature
    EXPECT_TRUE(missing(allHist[0].r_humidity_daily[dayGap]));
    EXPECT_DOUBLE_EQ(allHist[0].actualVaporPressure[dayGap], vpGap);

    generateMissingWeather(
        &SW_Run.MarkovIn,
        allHist,
        &SW_Run.WeatherSim,
        1980,
        1,
        swFALSE,
        swFALSE,
        wgLOCF,
        3,
        &LogInfo
    );
    if (LogInfo.stopRun != 0u) {
        deallocateAllWeather(&allHist);
    }
    sw_fail_on_error(ROOT_PROC, &LogInfo);

    // Observed vapor pressure is kept; relative humidity is consistent
    expRH = relativeHumidity1(vpGap, tavg);
    EXPECT_DOUBLE_EQ(allHist[0].temp_max[dayGap], tmax);
    EXPECT_DOUBLE_EQ(allHist[0].actualVaporPressure[dayGap], vpGap);
    EXPECT_NEAR(allHist[0].r_humidity_daily[dayGap], expRH, tol9);
    EXPECT_GT(
        fabs(
            allHist[0].r_humidity_daily[dayGap] -
            allHist[0].r_humidity_daily[dayGap - 1]
        ),
        1.
    );

    // Temperature scaling keeps relative humidity constant
    for (m = 0; m < MAX_MONTHS; m++) {
        scaleTmax[m] = 0.01;
        scaleTmin[m] = 0.01;
        scalePPT[m] = 1.;
        scaleSky[m] = 0.;
        scaleWind[m] = 1.;
        scaleRH[m] = 0.;
        scaleVP[m] = 1.;
        scaleRSDS[m] = 1.;
    }

    scaleYearlyWeather(
        allHist,
        1980,
        1,
        scaleTmax,
        scaleTmin,
        scalePPT,
        scaleSky,
        scaleWind,
        scaleRH,
        scaleVP,
        scaleRSDS,
        cum_monthdays,
        days_in_month
    );

    EXPECT_NEAR(
        allHist[0].actualVaporPressure[dayGap],
        vpGap * svp(tavg + 0.01, swFALSE, &tempSlope) /
            svp(tavg, swFALSE, &tempSlope),
        tol9
    );
    EXPECT_NEAR(
        relativeHumidity1(
            allHist[0].actualVaporPressure[dayGap], allHist[0].temp_avg[dayGap]
        ),
        expRH,
        tol6
    );

    deallocateAllWeather(&allHist);
}

TEST_F(WeatherFixtureTest, WeatherHumidityUnusedRelHumidInput) {
    // Relative humidity inputs are checked only on days when they are used,
    // i.e., when no humidity input of higher precedence is available
    sw_init_logs(NULL, &LogInfo);

    SW_WEATHER_HIST *allHist = NULL;
    double ***tempWeatherHist = NULL;
    Bool inputFlags[MAX_INPUT_COLUMNS];
    Bool fixWeatherData[NFIXWEATHER];
    int k;
    int doy;
    double const tmax = 25.;
    double const tmin = 5.;

    initHumidityTestFlags(inputFlags, fixWeatherData);
    inputFlags[ACTUAL_VP] = swTRUE;
    inputFlags[REL_HUMID] = swTRUE;

    // k = 0: invalid relative humidity on a day with vapor pressure (unused)
    // k = 1: invalid relative humidity on a day without vapor pressure (used)
    for (k = 0; k < 2; k++) {
        sw_init_logs(NULL, &LogInfo);

        SW_WTH_allocateAllWeather(&allHist, 1, &LogInfo);
        sw_fail_on_error(ROOT_PROC, &LogInfo);
        allocate_temp_weather(1, 1, &tempWeatherHist, &LogInfo);
        if (LogInfo.stopRun != 0u) {
            deallocateAllWeather(&allHist);
        }
        sw_fail_on_error(ROOT_PROC, &LogInfo);
        clear_hist_weather(1, &allHist[0], tempWeatherHist[0]);

        for (doy = 0; doy < 2; doy++) {
            tempWeatherHist[0][TEMP_MAX][doy] = tmax;
            tempWeatherHist[0][TEMP_MIN][doy] = tmin;
            tempWeatherHist[0][PPT][doy] = 0.;
        }
        // Day 0: invalid relative humidity
        tempWeatherHist[0][REL_HUMID][0] = 120.;
        tempWeatherHist[0][ACTUAL_VP][0] = (k == 0) ? 1. : SW_MISSING;
        // Day 1: missing vapor pressure -> relative humidity is used
        tempWeatherHist[0][REL_HUMID][1] = 50.;
        tempWeatherHist[0][ACTUAL_VP][1] = SW_MISSING;

        SW_WTH_setWeatherValues(
            1980,
            1,
            inputFlags,
            fixWeatherData,
            swFALSE,
            tempWeatherHist,
            SW_Run.RunIn.ModelRunIn.elevation,
            0,
            allHist,
            &LogInfo
        );
        deallocate_temp_weather(1, &tempWeatherHist);

        if (k == 0) {
            EXPECT_FALSE(LogInfo.stopRun);
            EXPECT_DOUBLE_EQ(allHist[0].actualVaporPressure[0], 1.);
            EXPECT_NEAR(
                allHist[0].actualVaporPressure[1],
                actualVaporPressure1(50., (tmax + tmin) / 2., swFALSE),
                tol9
            );
        } else {
            EXPECT_TRUE(LogInfo.stopRun);
        }

        deallocateAllWeather(&allHist);
    }
}

// Helper: allocate and clear one year of weather for humidity tests
void allocHumidityTestWeather(
    SW_WEATHER_HIST **allHist, double ****tempWeatherHist, LOG_INFO *LogInfo
) {
    SW_WTH_allocateAllWeather(allHist, 1, LogInfo);
    sw_fail_on_error(ROOT_PROC, LogInfo);
    allocate_temp_weather(1, 1, tempWeatherHist, LogInfo);
    if (LogInfo->stopRun != 0u) {
        deallocateAllWeather(allHist);
    }
    sw_fail_on_error(ROOT_PROC, LogInfo);
    clear_hist_weather(1, &(*allHist)[0], (*tempWeatherHist)[0]);
}

TEST_F(WeatherFixtureTest, WeatherHumidityPrecedenceAllInputs) {
    // Vapor pressure outranks dewpoint temperature, which outranks specific
    // humidity, which outranks min/max relative humidity; invalid min/max
    // relative humidity (> 100%) is not checked on days when an input of
    // higher precedence is used (vapor pressure or specific humidity)
    sw_init_logs(NULL, &LogInfo);

    SW_WEATHER_HIST *allHist = NULL;
    double ***tempWeatherHist = NULL;
    Bool inputFlags[MAX_INPUT_COLUMNS];
    Bool fixWeatherData[NFIXWEATHER];
    double slope;
    int doy;
    double const elevation = SW_Run.RunIn.ModelRunIn.elevation;
    double const tmax = 25.;
    double const tmin = 5.;
    double const vp[] = {1., SW_MISSING, SW_MISSING, SW_MISSING};
    double const tdew[] = {2., 2., SW_MISSING, SW_MISSING};
    double const huss[] = {5., 5., 5., SW_MISSING};
    double const rhmax[] = {120., 120., 120., 90.};
    double const rhmin[] = {30., 30., 30., 30.};
    int const nDays = 4;

    ASSERT_FALSE(missing(elevation));

    initHumidityTestFlags(inputFlags, fixWeatherData);
    inputFlags[ACTUAL_VP] = swTRUE;
    inputFlags[TEMP_DEWPOINT] = swTRUE;
    inputFlags[SPEC_HUMID] = swTRUE;
    inputFlags[REL_HUMID_MAX] = swTRUE;
    inputFlags[REL_HUMID_MIN] = swTRUE;

    allocHumidityTestWeather(&allHist, &tempWeatherHist, &LogInfo);

    for (doy = 0; doy < nDays; doy++) {
        tempWeatherHist[0][TEMP_MAX][doy] = tmax;
        tempWeatherHist[0][TEMP_MIN][doy] = tmin;
        tempWeatherHist[0][PPT][doy] = 0.;
        tempWeatherHist[0][ACTUAL_VP][doy] = vp[doy];
        tempWeatherHist[0][TEMP_DEWPOINT][doy] = tdew[doy];
        tempWeatherHist[0][SPEC_HUMID][doy] = huss[doy];
        tempWeatherHist[0][REL_HUMID_MAX][doy] = rhmax[doy];
        tempWeatherHist[0][REL_HUMID_MIN][doy] = rhmin[doy];
    }

    SW_WTH_setWeatherValues(
        1980,
        1,
        inputFlags,
        fixWeatherData,
        swFALSE,
        tempWeatherHist,
        elevation,
        0,
        allHist,
        &LogInfo
    );
    deallocate_temp_weather(1, &tempWeatherHist);

    EXPECT_FALSE(LogInfo.stopRun);
    EXPECT_DOUBLE_EQ(allHist[0].actualVaporPressure[0], vp[0]);
    EXPECT_NEAR(
        allHist[0].actualVaporPressure[1], svp(tdew[1], swFALSE, &slope), tol9
    );
    EXPECT_NEAR(
        allHist[0].actualVaporPressure[2],
        actualVaporPressure4(huss[2], elevation),
        tol9
    );
    EXPECT_NEAR(
        allHist[0].actualVaporPressure[3],
        actualVaporPressure2(rhmax[3], rhmin[3], tmax, tmin, swFALSE),
        tol9
    );

    deallocateAllWeather(&allHist);
}

TEST_F(WeatherFixtureTest, WeatherHumidityElevationOnlyIfSpecHumidUsed) {
    // Missing elevation is an error only on days when specific humidity is
    // used to calculate actual vapor pressure
    sw_init_logs(NULL, &LogInfo);

    SW_WEATHER_HIST *allHist = NULL;
    double ***tempWeatherHist = NULL;
    Bool inputFlags[MAX_INPUT_COLUMNS];
    Bool fixWeatherData[NFIXWEATHER];
    int k;

    initHumidityTestFlags(inputFlags, fixWeatherData);
    inputFlags[ACTUAL_VP] = swTRUE;
    inputFlags[SPEC_HUMID] = swTRUE;

    // k = 0: vapor pressure available (specific humidity unused)
    // k = 1: vapor pressure missing (specific humidity used)
    for (k = 0; k < 2; k++) {
        sw_init_logs(NULL, &LogInfo);
        allocHumidityTestWeather(&allHist, &tempWeatherHist, &LogInfo);

        tempWeatherHist[0][TEMP_MAX][0] = 25.;
        tempWeatherHist[0][TEMP_MIN][0] = 5.;
        tempWeatherHist[0][PPT][0] = 0.;
        tempWeatherHist[0][ACTUAL_VP][0] = (k == 0) ? 1. : SW_MISSING;
        tempWeatherHist[0][SPEC_HUMID][0] = 5.;

        SW_WTH_setWeatherValues(
            1980,
            1,
            inputFlags,
            fixWeatherData,
            swFALSE,
            tempWeatherHist,
            SW_MISSING, // elevation
            0,
            allHist,
            &LogInfo
        );
        deallocate_temp_weather(1, &tempWeatherHist);

        if (k == 0) {
            EXPECT_FALSE(LogInfo.stopRun);
            EXPECT_DOUBLE_EQ(allHist[0].actualVaporPressure[0], 1.);
        } else {
            EXPECT_TRUE(LogInfo.stopRun);
        }

        deallocateAllWeather(&allHist);
    }
}

TEST_F(WeatherFixtureTest, WeatherHumidityImputationRelHumidLOCF) {
    // (k = 0, 1) Relative humidity input on a day with missing maximum air
    //   temperature: after LOCF of air temperature, actual vapor pressure is
    //   calculated with the input convention of relative humidity
    // (k = 2) Missing vapor pressure input on a day with air temperature:
    //   LOCF of actual vapor pressure; relative humidity is derived from it
    //   and limited to 100%
    sw_init_logs(NULL, &LogInfo);

    SW_WEATHER_HIST *allHist = NULL;
    double ***tempWeatherHist = NULL;
    Bool inputFlags[MAX_INPUT_COLUMNS];
    Bool fixWeatherData[NFIXWEATHER];
    Bool isOverIce;
    double slope;
    double tavg;
    int k;
    int doy;
    int const dayGap = 2;
    double const rh = 60.;
    double const vp = 1.5;
    unsigned int const nDays = Time_get_lastdoy_y(1980);

    for (k = 0; k < 3; k++) {
        sw_init_logs(NULL, &LogInfo);

        isOverIce = (k == 1) ? swTRUE : swFALSE;

        initHumidityTestFlags(inputFlags, fixWeatherData);
        inputFlags[CLOUD_COV] = swTRUE;
        inputFlags[WIND_SPEED] = swTRUE;
        inputFlags[SHORT_WR] = swTRUE;
        if (k < 2) {
            inputFlags[REL_HUMID] = swTRUE;
        } else {
            inputFlags[ACTUAL_VP] = swTRUE;
        }

        allocHumidityTestWeather(&allHist, &tempWeatherHist, &LogInfo);

        for (doy = 0; doy < (int) nDays; doy++) {
            // Below 0 C (k < 2) to distinguish ice from liquid water
            tempWeatherHist[0][TEMP_MAX][doy] = (k < 2) ? 2. : 25.;
            tempWeatherHist[0][TEMP_MIN][doy] = (k < 2) ? -12. : 5.;
            tempWeatherHist[0][PPT][doy] = 0.;
            tempWeatherHist[0][CLOUD_COV][doy] = 50.;
            tempWeatherHist[0][WIND_SPEED][doy] = 2.;
            tempWeatherHist[0][SHORT_WR][doy] = 200.;
            tempWeatherHist[0][REL_HUMID][doy] = rh;
            tempWeatherHist[0][ACTUAL_VP][doy] = vp;
        }

        if (k < 2) {
            tempWeatherHist[0][TEMP_MAX][dayGap] = SW_MISSING;
        } else {
            // Colder day: LOCF vapor pressure exceeds saturation
            tempWeatherHist[0][TEMP_MAX][dayGap] = 8.;
            tempWeatherHist[0][TEMP_MIN][dayGap] = 2.;
            tempWeatherHist[0][ACTUAL_VP][dayGap] = SW_MISSING;
        }

        SW_WTH_setWeatherValues(
            1980,
            1,
            inputFlags,
            fixWeatherData,
            isOverIce,
            tempWeatherHist,
            SW_Run.RunIn.ModelRunIn.elevation,
            0,
            allHist,
            &LogInfo
        );
        deallocate_temp_weather(1, &tempWeatherHist);
        if (LogInfo.stopRun != 0u) {
            deallocateAllWeather(&allHist);
        }
        sw_fail_on_error(ROOT_PROC, &LogInfo);

        EXPECT_TRUE(missing(allHist[0].actualVaporPressure[dayGap]));

        generateMissingWeather(
            &SW_Run.MarkovIn,
            allHist,
            &SW_Run.WeatherSim,
            1980,
            1,
            swFALSE,
            isOverIce,
            wgLOCF,
            3,
            &LogInfo
        );
        if (LogInfo.stopRun != 0u) {
            deallocateAllWeather(&allHist);
        }
        sw_fail_on_error(ROOT_PROC, &LogInfo);

        tavg = allHist[0].temp_avg[dayGap];

        if (k < 2) {
            EXPECT_NEAR(tavg, -5., tol9);
            EXPECT_NEAR(
                allHist[0].actualVaporPressure[dayGap],
                rh / 100. * svp(tavg, isOverIce, &slope),
                tol9
            );
            // Same as on days without imputation
            EXPECT_NEAR(
                allHist[0].actualVaporPressure[dayGap],
                allHist[0].actualVaporPressure[dayGap - 1],
                tol9
            );
            // Relative humidity is relative to liquid water internally
            EXPECT_NEAR(
                allHist[0].r_humidity_daily[dayGap],
                fmin(
                    100.,
                    100. * allHist[0].actualVaporPressure[dayGap] /
                        svp(tavg, swFALSE, &slope)
                ),
                tol9
            );

        } else {
            EXPECT_NEAR(tavg, 5., tol9);
            ASSERT_GT(vp, svp(tavg, swFALSE, &slope));
            EXPECT_DOUBLE_EQ(allHist[0].actualVaporPressure[dayGap], vp);
            EXPECT_DOUBLE_EQ(allHist[0].r_humidity_daily[dayGap], 100.);
        }

        deallocateAllWeather(&allHist);
    }
}

TEST_F(WeatherFixtureTest, WeatherAllocateAllWeatherMissing) {
    // Newly allocated daily weather is missing (not uninitialized)
    sw_init_logs(NULL, &LogInfo);

    SW_WEATHER_HIST *allHist = NULL;
    unsigned int const nYears = 2;
    unsigned int yearIndex;
    int doy;

    SW_WTH_allocateAllWeather(&allHist, nYears, &LogInfo);
    sw_fail_on_error(ROOT_PROC, &LogInfo);

    for (yearIndex = 0; yearIndex < nYears; yearIndex++) {
        for (doy = 0; doy < MAX_DAYS; doy++) {
            EXPECT_TRUE(missing(allHist[yearIndex].temp_max[doy]));
            EXPECT_TRUE(missing(allHist[yearIndex].temp_min[doy]));
            EXPECT_TRUE(missing(allHist[yearIndex].temp_avg[doy]));
            EXPECT_TRUE(missing(allHist[yearIndex].ppt[doy]));
            EXPECT_TRUE(missing(allHist[yearIndex].cloudcov_daily[doy]));
            EXPECT_TRUE(missing(allHist[yearIndex].windspeed_daily[doy]));
            EXPECT_TRUE(missing(allHist[yearIndex].r_humidity_daily[doy]));
            EXPECT_TRUE(missing(allHist[yearIndex].shortWaveRad[doy]));
            EXPECT_TRUE(missing(allHist[yearIndex].actualVaporPressure[doy]));
        }
    }

    deallocateAllWeather(&allHist);
}

TEST_F(WeatherFixtureTest, WeatherSetValuesNoCarryOver) {
    // SW_WTH_setWeatherValues() does not keep values of mean air temperature
    // or shortwave radiation that are in `yearlyWeather` from previous use
    // (e.g., previous year in nc-mode) on days with missing air temperature
    // or without shortwave radiation inputs; thus, actual vapor pressure is
    // not calculated from relative humidity and a stale mean air temperature
    sw_init_logs(NULL, &LogInfo);

    SW_WEATHER_HIST *allHist = NULL;
    double ***tempWeatherHist = NULL;
    Bool inputFlags[MAX_INPUT_COLUMNS];
    Bool fixWeatherData[NFIXWEATHER];
    int doy;
    int const dayGap = 2;
    unsigned int const nDays = Time_get_lastdoy_y(1980);

    initHumidityTestFlags(inputFlags, fixWeatherData);
    inputFlags[CLOUD_COV] = swTRUE;
    inputFlags[WIND_SPEED] = swTRUE;
    inputFlags[REL_HUMID] = swTRUE;

    allocHumidityTestWeather(&allHist, &tempWeatherHist, &LogInfo);

    for (doy = 0; doy < (int) nDays; doy++) {
        // Values from a previous use of `allHist`
        allHist[0].temp_avg[doy] = 20.;
        allHist[0].shortWaveRad[doy] = 0.;

        tempWeatherHist[0][TEMP_MAX][doy] = 25.;
        tempWeatherHist[0][TEMP_MIN][doy] = 5.;
        tempWeatherHist[0][PPT][doy] = 0.;
        tempWeatherHist[0][CLOUD_COV][doy] = 50.;
        tempWeatherHist[0][WIND_SPEED][doy] = 2.;
        tempWeatherHist[0][REL_HUMID][doy] = 60.;
    }

    tempWeatherHist[0][TEMP_MAX][dayGap] = SW_MISSING;

    SW_WTH_setWeatherValues(
        1980,
        1,
        inputFlags,
        fixWeatherData,
        swFALSE,
        tempWeatherHist,
        SW_Run.RunIn.ModelRunIn.elevation,
        0,
        allHist,
        &LogInfo
    );
    deallocate_temp_weather(1, &tempWeatherHist);
    if (LogInfo.stopRun != 0u) {
        deallocateAllWeather(&allHist);
    }
    sw_fail_on_error(ROOT_PROC, &LogInfo);

    // Day with missing maximum air temperature
    EXPECT_TRUE(missing(allHist[0].temp_avg[dayGap]));
    EXPECT_TRUE(missing(allHist[0].actualVaporPressure[dayGap]));
    EXPECT_TRUE(missing(allHist[0].shortWaveRad[dayGap]));

    // Day with complete inputs
    EXPECT_DOUBLE_EQ(allHist[0].temp_avg[dayGap - 1], 15.);
    EXPECT_FALSE(missing(allHist[0].actualVaporPressure[dayGap - 1]));
    EXPECT_TRUE(missing(allHist[0].shortWaveRad[dayGap - 1]));

    deallocateAllWeather(&allHist);
}

TEST_F(WeatherFixtureTest, WeatherInputGridMET) {

    /*
       This section tests humidity-related values that can be averaged,
       max/min relative humidity, and the calculation of actual vapor pressure,
       based on the average relative humidity value, is calculated reasonably.

       * This section uses the test directory "*_gridmet".
     */

    double result;
    double expectedResult;
    int const yearIndex = 0;
    int const year = 1980;
    int const midJanDay = 14;
    int doy;
    double snowpack[TWO_DAYS] = {0.};
    double ***tempWeatherHist = NULL;

    /* Test correct priority is being given to input values from DAYMET */
    SW_WTH_setup(
        SW_Run.WeatherIn,
        SW_Domain.SW_PathInputs.txtInFiles,
        SW_Domain.SW_PathInputs.txtWeatherPrefix,
        &LogInfo
    );
    sw_fail_on_error(
        ROOT_PROC, &LogInfo
    ); // exit test program if unexpected error

    // Switch directory to gridmet input folder
    (void) snprintf(
        SW_Run.WeatherIn->name_prefix,
        sizeof SW_Run.WeatherIn->name_prefix,
        "%s",
        "Input/data_weather_gridmet/weath"
    );

    // Turn off monthly flags
    SW_Run.WeatherIn->use_cloudCoverMonthly = swFALSE;
    SW_Run.WeatherIn->use_windSpeedMonthly = swFALSE;
    SW_Run.WeatherIn->use_humidityMonthly = swFALSE;

    // Manually edit index/flag arrays in SW_WEATHER_INPUTS to make test as
    // realistic as possible
    // Note: Indices are based on the directory:
    // Input/data_weather_gridmet/weath.1980
    SW_Run.WeatherIn->dailyInputIndices[TEMP_MAX] = 0;
    SW_Run.WeatherIn->dailyInputIndices[TEMP_MIN] = 1;
    SW_Run.WeatherIn->dailyInputIndices[PPT] = 2;
    SW_Run.WeatherIn->dailyInputIndices[CLOUD_COV] = 0;
    SW_Run.WeatherIn->dailyInputIndices[WIND_SPEED] = 3;
    SW_Run.WeatherIn->dailyInputIndices[WIND_EAST] = 0;
    SW_Run.WeatherIn->dailyInputIndices[WIND_NORTH] = 0;
    SW_Run.WeatherIn->dailyInputIndices[REL_HUMID] = 0;
    SW_Run.WeatherIn->dailyInputIndices[REL_HUMID_MAX] = 4;
    SW_Run.WeatherIn->dailyInputIndices[REL_HUMID_MIN] = 5;
    SW_Run.WeatherIn->dailyInputIndices[SPEC_HUMID] = 0;
    SW_Run.WeatherIn->dailyInputIndices[TEMP_DEWPOINT] = 0;
    SW_Run.WeatherIn->dailyInputIndices[ACTUAL_VP] = 0;
    SW_Run.WeatherIn->dailyInputIndices[SHORT_WR] = 6;

    SW_Run.WeatherIn->dailyInputFlags[TEMP_MAX] = swTRUE;
    SW_Run.WeatherIn->dailyInputFlags[TEMP_MIN] = swTRUE;
    SW_Run.WeatherIn->dailyInputFlags[PPT] = swTRUE;
    SW_Run.WeatherIn->dailyInputFlags[CLOUD_COV] = swFALSE;
    SW_Run.WeatherIn->dailyInputFlags[WIND_SPEED] = swTRUE;
    SW_Run.WeatherIn->dailyInputFlags[WIND_EAST] = swFALSE;
    SW_Run.WeatherIn->dailyInputFlags[WIND_NORTH] = swFALSE;
    SW_Run.WeatherIn->dailyInputFlags[REL_HUMID] = swFALSE;
    SW_Run.WeatherIn->dailyInputFlags[REL_HUMID_MAX] = swTRUE;
    SW_Run.WeatherIn->dailyInputFlags[REL_HUMID_MIN] = swTRUE;
    SW_Run.WeatherIn->dailyInputFlags[SPEC_HUMID] = swFALSE;
    SW_Run.WeatherIn->dailyInputFlags[TEMP_DEWPOINT] = swFALSE;
    SW_Run.WeatherIn->dailyInputFlags[ACTUAL_VP] = swFALSE;
    SW_Run.WeatherIn->dailyInputFlags[SHORT_WR] = swTRUE;

    SW_Run.WeatherIn->n_input_forcings = 7;
    SW_Run.WeatherIn->desc_rsds =
        1; // gridMET rsds is flux density over 24 hours

    // Allocate temporary location for weather
    allocate_temp_weather(1, 1, &tempWeatherHist, &LogInfo);
    sw_fail_on_error(ROOT_PROC, &LogInfo);

    // Reset daily weather values
    clear_hist_weather(1, &SW_Run.RunIn.weathRunAllHist[0], tempWeatherHist[0]);

    // Using the new inputs folder, read in year = 1980
    read_weather_hist(
        year,
        tempWeatherHist[0],
        SW_Run.WeatherIn->name_prefix,
        SW_Run.WeatherIn->n_input_forcings,
        SW_Run.WeatherIn->dailyInputIndices,
        SW_Run.WeatherIn->dailyInputFlags,
        &LogInfo
    );
    if (LogInfo.stopRun != 0u) {
        deallocate_temp_weather(1, &tempWeatherHist);
    }
    sw_fail_on_error(
        ROOT_PROC, &LogInfo
    ); // exit test program if unexpected error

    // Transfer values from temporary to permanent location (`SW_WEATHER_HIST`)
    SW_WTH_setWeatherValues(
        SW_Run.ModelIn->startyr,
        1,
        SW_Run.WeatherIn->dailyInputFlags,
        SW_Run.WeatherIn->fixWeatherData,
        SW_Run.WeatherIn->isRelHumidInputOverIce,
        tempWeatherHist,
        SW_Run.RunIn.ModelRunIn.elevation,
        0,
        &SW_Run.RunIn.weathRunAllHist[0],
        &LogInfo
    );
    deallocate_temp_weather(1, &tempWeatherHist);
    sw_fail_on_error(
        ROOT_PROC, &LogInfo
    ); // exit test program if unexpected error

    // Check that weather contains reasonable values
    checkYearlyWeather(
        SW_Run.RunIn.weathRunAllHist,
        SW_Domain.startstart,
        SW_Domain.endend,
        SW_Domain.startyr,
        SW_Domain.endyr,
        SW_Run.ModelIn->startyr,
        1,
        &LogInfo
    );
    sw_fail_on_error(
        ROOT_PROC, &LogInfo
    ); // exit test program if unexpected error


    // Check that no value is missing
    for (doy = 1; doy < MAX_DAYS; doy++) {
        SW_WTH_new_day(
            SW_Run.WeatherIn,
            &SW_Run.WeatherSim,
            SW_Run.RunIn.weathRunAllHist,
            SW_Run.SiteIn,
            snowpack,
            SW_Run.RunIn.ModelRunIn.isnorth,
            doy,
            yearIndex,
            MAX_DAYS,
            &LogInfo
        );
        sw_fail_on_error(
            ROOT_PROC, &LogInfo
        ); // exit test program if unexpected error
    }


    // Check relative humidity [0, 100] % ---
    // Expect that relative humidity is calculated from actual vapor pressure
    // (which is calculated from hursmax, hursmin, Tmax, and Tmin) and
    // mean air temperature, i.e., consistent with actual vapor pressure
    // and relative to liquid water

    // Check on day 1 (values from Input/data_weather_gridmet/weath.1980)
    // Relative humidity from vapor pressure based on hursmax (74.17) and
    // hursmin (31.42)
    EXPECT_NEAR(
        SW_Run.RunIn.weathRunAllHist[yearIndex].r_humidity_daily[0],
        fmin(
            100.,
            relativeHumidity1(
                SW_Run.RunIn.weathRunAllHist[yearIndex].actualVaporPressure[0],
                SW_Run.RunIn.weathRunAllHist[yearIndex].temp_avg[0]
            )
        ),
        tol6
    );

    // Check on day 15 (values from Input/data_weather_gridmet/weath.1980)
    // Relative humidity from vapor pressure based on hursmax (88.35) and
    // hursmin (34.35)
    EXPECT_NEAR(
        SW_Run.RunIn.weathRunAllHist[yearIndex].r_humidity_daily[midJanDay],
        fmin(
            100.,
            relativeHumidity1(
                SW_Run.RunIn.weathRunAllHist[yearIndex]
                    .actualVaporPressure[midJanDay],
                SW_Run.RunIn.weathRunAllHist[yearIndex].temp_avg[midJanDay]
            )
        ),
        tol6
    );

    // Check that value on day 15 is not interpolated from mean monthly values
    EXPECT_NE(
        SW_Run.RunIn.weathRunAllHist[yearIndex].r_humidity_daily[midJanDay],
        SW_Run.RunIn.SkyRunIn.r_humidity[0]
    );


    // Check vapor pressure
    result = SW_Run.RunIn.weathRunAllHist[yearIndex].actualVaporPressure[0];

    // Get expected result from Input/data_weather_gridmet/weath.1980 day 1
    // hursmax_pct, hursmin_pct, Tmax_C, and Tmin_C
    expectedResult = actualVaporPressure2(74.17, 31.42, -3.18, -12.32, swFALSE);

    // Based on the max/min of relative humidity, test if actual vapor pressure
    // was calculated reasonably
    EXPECT_NEAR(result, expectedResult, tol6);


    // We have observed radiation and missing cloud cover
    EXPECT_FALSE(missing(SW_Run.RunIn.weathRunAllHist[yearIndex].shortWaveRad[0]
    ));
    EXPECT_TRUE(
        missing(SW_Run.RunIn.weathRunAllHist[yearIndex].cloudcov_daily[0])
    );
}

TEST_F(WeatherFixtureTest, WeatherInputDaymet) {

    /*
       This section covers the assurance that if a daily value is provided
       (actual vapor pressure in this case), aside from temperature and
       precipitation, it is correctly set. Along with reasonable calculation of
       a separate daily value that is dependent on said daily variable (relative
       humidity in this case).

       * This scenario only happens with humidity-related variables.
       * This section uses the test directory "*_daymet".
     */

    double result;
    double expectedResult;
    int const yearIndex = 0;
    int const year = 1980;
    int const midJanDay = 14;
    double ***tempWeatherHist = NULL;

    /* Test correct priority is being given to input values from DAYMET */
    SW_WTH_setup(
        SW_Run.WeatherIn,
        SW_Domain.SW_PathInputs.txtInFiles,
        SW_Domain.SW_PathInputs.txtWeatherPrefix,
        &LogInfo
    );
    sw_fail_on_error(
        ROOT_PROC, &LogInfo
    ); // exit test program if unexpected error

    // Switch directory to daymet input folder
    (void) snprintf(
        SW_Run.WeatherIn->name_prefix,
        sizeof SW_Run.WeatherIn->name_prefix,
        "%s",
        "Input/data_weather_daymet/weath"
    );

    // Turn off monthly flags
    SW_Run.WeatherIn->use_cloudCoverMonthly = swFALSE;
    SW_Run.WeatherIn->use_windSpeedMonthly = swFALSE;
    SW_Run.WeatherIn->use_humidityMonthly = swFALSE;

    // Manually edit index/flag arrays in SW_WEATHER_INPUTS to make test as
    // realistic as possible
    // Note: Indices are based on the directory:
    // Input/data_weather_daymet/weath.1980
    SW_Run.WeatherIn->dailyInputIndices[TEMP_MAX] = 0;
    SW_Run.WeatherIn->dailyInputIndices[TEMP_MIN] = 1;
    SW_Run.WeatherIn->dailyInputIndices[PPT] = 2;
    SW_Run.WeatherIn->dailyInputIndices[CLOUD_COV] = 0;
    SW_Run.WeatherIn->dailyInputIndices[WIND_SPEED] = 0;
    SW_Run.WeatherIn->dailyInputIndices[WIND_EAST] = 0;
    SW_Run.WeatherIn->dailyInputIndices[WIND_NORTH] = 0;
    SW_Run.WeatherIn->dailyInputIndices[REL_HUMID] = 0;
    SW_Run.WeatherIn->dailyInputIndices[REL_HUMID_MAX] = 0;
    SW_Run.WeatherIn->dailyInputIndices[REL_HUMID_MIN] = 0;
    SW_Run.WeatherIn->dailyInputIndices[SPEC_HUMID] = 0;
    SW_Run.WeatherIn->dailyInputIndices[TEMP_DEWPOINT] = 0;
    SW_Run.WeatherIn->dailyInputIndices[ACTUAL_VP] = 3;
    SW_Run.WeatherIn->dailyInputIndices[SHORT_WR] = 4;

    SW_Run.WeatherIn->dailyInputFlags[TEMP_MAX] = swTRUE;
    SW_Run.WeatherIn->dailyInputFlags[TEMP_MIN] = swTRUE;
    SW_Run.WeatherIn->dailyInputFlags[PPT] = swTRUE;
    SW_Run.WeatherIn->dailyInputFlags[CLOUD_COV] = swFALSE;
    SW_Run.WeatherIn->dailyInputFlags[WIND_SPEED] = swFALSE;
    SW_Run.WeatherIn->dailyInputFlags[WIND_EAST] = swFALSE;
    SW_Run.WeatherIn->dailyInputFlags[WIND_NORTH] = swFALSE;
    SW_Run.WeatherIn->dailyInputFlags[REL_HUMID] = swFALSE;
    SW_Run.WeatherIn->dailyInputFlags[REL_HUMID_MAX] = swFALSE;
    SW_Run.WeatherIn->dailyInputFlags[REL_HUMID_MIN] = swFALSE;
    SW_Run.WeatherIn->dailyInputFlags[SPEC_HUMID] = swFALSE;
    SW_Run.WeatherIn->dailyInputFlags[TEMP_DEWPOINT] = swFALSE;
    SW_Run.WeatherIn->dailyInputFlags[ACTUAL_VP] = swTRUE;
    SW_Run.WeatherIn->dailyInputFlags[SHORT_WR] = swTRUE;

    SW_Run.WeatherIn->n_input_forcings = 5;
    // DayMet rsds is flux density over daylight period
    SW_Run.WeatherIn->desc_rsds = 2;

    // Allocate temporary location for weather
    allocate_temp_weather(1, 1, &tempWeatherHist, &LogInfo);
    sw_fail_on_error(ROOT_PROC, &LogInfo);

    // Reset daily weather values
    clear_hist_weather(1, &SW_Run.RunIn.weathRunAllHist[0], tempWeatherHist[0]);

    // Using the new inputs folder, read in year = 1980
    read_weather_hist(
        year,
        tempWeatherHist[0],
        SW_Run.WeatherIn->name_prefix,
        SW_Run.WeatherIn->n_input_forcings,
        SW_Run.WeatherIn->dailyInputIndices,
        SW_Run.WeatherIn->dailyInputFlags,
        &LogInfo
    );
    if (LogInfo.stopRun != 0u) {
        deallocate_temp_weather(1, &tempWeatherHist);
    }
    sw_fail_on_error(
        ROOT_PROC, &LogInfo
    ); // exit test program if unexpected error

    // Transfer values from temporary to permanent location (`SW_WEATHER_HIST`)
    SW_WTH_setWeatherValues(
        SW_Run.ModelIn->startyr,
        1,
        SW_Run.WeatherIn->dailyInputFlags,
        SW_Run.WeatherIn->fixWeatherData,
        SW_Run.WeatherIn->isRelHumidInputOverIce,
        tempWeatherHist,
        SW_Run.RunIn.ModelRunIn.elevation,
        0,
        &SW_Run.RunIn.weathRunAllHist[0],
        &LogInfo
    );
    deallocate_temp_weather(1, &tempWeatherHist);
    sw_fail_on_error(
        ROOT_PROC, &LogInfo
    ); // exit test program if unexpected error


    // Check that weather contains reasonable values
    checkYearlyWeather(
        SW_Run.RunIn.weathRunAllHist,
        SW_Domain.startstart,
        SW_Domain.endend,
        SW_Domain.startyr,
        SW_Domain.endyr,
        SW_Run.ModelIn->startyr,
        1,
        &LogInfo
    );
    sw_fail_on_error(
        ROOT_PROC, &LogInfo
    ); // exit test program if unexpected error


    // Check vapor pressure
    result = SW_Run.RunIn.weathRunAllHist[yearIndex].actualVaporPressure[0];

    // Get expected result from Input/data_weather_daymet/weath.1980 day 1
    // vp_kPa
    expectedResult = .3;

    // Test if actual vapor pressure value was set to the first days
    // input value from Input/data_weather_daymet/weath.1980 day 1
    EXPECT_NEAR(result, expectedResult, tol6);


    // Check relative humidity [0, 100] % ---
    // Expect that relative humidity is calculated from vp, tmax, and tmin

    // Check on day 1 (values from Input/data_weather_daymet/weath.1980)
    // Calculate relative humidity from vp (0.3), tmax (-.37), and tmin (-9.2)
    EXPECT_NEAR(
        SW_Run.RunIn.weathRunAllHist[yearIndex].r_humidity_daily[0],
        relativeHumidity1(0.30, (-0.37 - 9.2) / 2.),
        tol6
    );

    // Check on day 15 (values from Input/data_weather_daymet/weath.1980)
    // Calculate relative humidity from vp (0.29), tmax (-.81), and tmin (-9.7)
    EXPECT_NEAR(
        SW_Run.RunIn.weathRunAllHist[yearIndex].r_humidity_daily[midJanDay],
        relativeHumidity1(0.29, (-0.81 - 9.7) / 2.),
        tol6
    );

    // Check that value on day 15 is not interpolated from mean monthly values
    EXPECT_NE(
        SW_Run.RunIn.weathRunAllHist[yearIndex].r_humidity_daily[midJanDay],
        SW_Run.RunIn.SkyRunIn.r_humidity[0]
    );


    // We have observed radiation and missing cloud cover
    EXPECT_FALSE(missing(SW_Run.RunIn.weathRunAllHist[yearIndex].shortWaveRad[0]
    ));
    EXPECT_TRUE(
        missing(SW_Run.RunIn.weathRunAllHist[yearIndex].cloudcov_daily[0])
    );
}

TEST_F(WeatherFixtureTest, WeatherInputMACAtype1) {

    /*
       This section assures that a variable aside from humidity-related ones,
       wind speed in this case, is calculated reasonably based on its
       components.

        * This section uses the test directory "*_maca-type1" (hursmin, hursmax)
     */

    double result;
    double expectedResult;
    int const yearIndex = 0;
    int const year = 1980;
    int const midJanDay = 14;
    int doy;
    double snowpack[TWO_DAYS] = {0.};
    double ***tempWeatherHist = NULL;

    /* Test correct priority is being given to input values from MACA */

    SW_WTH_setup(
        SW_Run.WeatherIn,
        SW_Domain.SW_PathInputs.txtInFiles,
        SW_Domain.SW_PathInputs.txtWeatherPrefix,
        &LogInfo
    );
    sw_fail_on_error(
        ROOT_PROC, &LogInfo
    ); // exit test program if unexpected error

    // Switch directory to the input folder of the
    // first type of MACA (hursmin, hursmax)
    (void) snprintf(
        SW_Run.WeatherIn->name_prefix,
        sizeof SW_Run.WeatherIn->name_prefix,
        "%s",
        "Input/data_weather_maca-type1/weath"
    );

    // Turn off monthly flags
    SW_Run.WeatherIn->use_cloudCoverMonthly = swFALSE;
    SW_Run.WeatherIn->use_windSpeedMonthly = swFALSE;
    SW_Run.WeatherIn->use_humidityMonthly = swFALSE;

    // Manually edit index/flag arrays in SW_WEATHER_INPUTS to make test as
    // realistic as possible
    // Note: Indices are based on the directory:
    // Input/data_weather_maca-type1/weath.1980
    SW_Run.WeatherIn->dailyInputIndices[TEMP_MAX] = 0;
    SW_Run.WeatherIn->dailyInputIndices[TEMP_MIN] = 1;
    SW_Run.WeatherIn->dailyInputIndices[PPT] = 2;
    SW_Run.WeatherIn->dailyInputIndices[CLOUD_COV] = 0;
    SW_Run.WeatherIn->dailyInputIndices[WIND_SPEED] = 0;
    SW_Run.WeatherIn->dailyInputIndices[WIND_EAST] = 3;
    SW_Run.WeatherIn->dailyInputIndices[WIND_NORTH] = 4;
    SW_Run.WeatherIn->dailyInputIndices[REL_HUMID] = 0;
    SW_Run.WeatherIn->dailyInputIndices[REL_HUMID_MAX] = 5;
    SW_Run.WeatherIn->dailyInputIndices[REL_HUMID_MIN] = 6;
    SW_Run.WeatherIn->dailyInputIndices[SPEC_HUMID] = 0;
    SW_Run.WeatherIn->dailyInputIndices[TEMP_DEWPOINT] = 0;
    SW_Run.WeatherIn->dailyInputIndices[ACTUAL_VP] = 0;
    SW_Run.WeatherIn->dailyInputIndices[SHORT_WR] = 7;

    SW_Run.WeatherIn->dailyInputFlags[TEMP_MAX] = swTRUE;
    SW_Run.WeatherIn->dailyInputFlags[TEMP_MIN] = swTRUE;
    SW_Run.WeatherIn->dailyInputFlags[PPT] = swTRUE;
    SW_Run.WeatherIn->dailyInputFlags[CLOUD_COV] = swFALSE;
    SW_Run.WeatherIn->dailyInputFlags[WIND_SPEED] = swFALSE;
    SW_Run.WeatherIn->dailyInputFlags[WIND_EAST] = swTRUE;
    SW_Run.WeatherIn->dailyInputFlags[WIND_NORTH] = swTRUE;
    SW_Run.WeatherIn->dailyInputFlags[REL_HUMID] = swFALSE;
    SW_Run.WeatherIn->dailyInputFlags[REL_HUMID_MAX] = swTRUE;
    SW_Run.WeatherIn->dailyInputFlags[REL_HUMID_MIN] = swTRUE;
    SW_Run.WeatherIn->dailyInputFlags[SPEC_HUMID] = swFALSE;
    SW_Run.WeatherIn->dailyInputFlags[TEMP_DEWPOINT] = swFALSE;
    SW_Run.WeatherIn->dailyInputFlags[ACTUAL_VP] = swFALSE;
    SW_Run.WeatherIn->dailyInputFlags[SHORT_WR] = swTRUE;

    SW_Run.WeatherIn->n_input_forcings = 8;
    SW_Run.WeatherIn->desc_rsds = 1; // MACA rsds is flux density over 24 hours

    // Allocate temporary location for weather
    allocate_temp_weather(1, 1, &tempWeatherHist, &LogInfo);
    sw_fail_on_error(ROOT_PROC, &LogInfo);

    // Reset daily weather values
    clear_hist_weather(1, &SW_Run.RunIn.weathRunAllHist[0], tempWeatherHist[0]);

    // Using the new inputs folder, read in year = 1980
    read_weather_hist(
        year,
        tempWeatherHist[0],
        SW_Run.WeatherIn->name_prefix,
        SW_Run.WeatherIn->n_input_forcings,
        SW_Run.WeatherIn->dailyInputIndices,
        SW_Run.WeatherIn->dailyInputFlags,
        &LogInfo
    );
    if (LogInfo.stopRun != 0u) {
        deallocate_temp_weather(1, &tempWeatherHist);
    }
    sw_fail_on_error(
        ROOT_PROC, &LogInfo
    ); // exit test program if unexpected error

    // Transfer values from temporary to permanent location (`SW_WEATHER_HIST`)
    SW_WTH_setWeatherValues(
        year,
        1,
        SW_Run.WeatherIn->dailyInputFlags,
        SW_Run.WeatherIn->fixWeatherData,
        SW_Run.WeatherIn->isRelHumidInputOverIce,
        tempWeatherHist,
        SW_Run.RunIn.ModelRunIn.elevation,
        0,
        &SW_Run.RunIn.weathRunAllHist[0],
        &LogInfo
    );
    deallocate_temp_weather(1, &tempWeatherHist);
    sw_fail_on_error(
        ROOT_PROC, &LogInfo
    ); // exit test program if unexpected error

    // Check that weather contains reasonable values
    checkYearlyWeather(
        SW_Run.RunIn.weathRunAllHist,
        SW_Domain.startstart,
        SW_Domain.endend,
        SW_Domain.startyr,
        SW_Domain.endyr,
        year,
        1,
        &LogInfo
    );
    sw_fail_on_error(
        ROOT_PROC, &LogInfo
    ); // exit test program if unexpected error


    // Check that no value is missing
    for (doy = 1; doy < MAX_DAYS; doy++) {
        SW_WTH_new_day(
            SW_Run.WeatherIn,
            &SW_Run.WeatherSim,
            SW_Run.RunIn.weathRunAllHist,
            SW_Run.SiteIn,
            snowpack,
            SW_Run.RunIn.ModelRunIn.isnorth,
            doy,
            yearIndex,
            MAX_DAYS,
            &LogInfo
        );
        sw_fail_on_error(
            ROOT_PROC, &LogInfo
        ); // exit test program if unexpected error
    }


    // Check wind speed
    result = SW_Run.RunIn.weathRunAllHist[yearIndex].windspeed_daily[0];

    // Get expected result from Input/data_weather_maca-type1/weath.1980 day 1
    // uas_mPERs = 3.31 and vas_mPERs = -.85
    expectedResult = sqrt(squared(3.31) + squared(-.85));

    // Test if wind speed was calculated within reasonable range to
    // the expected result
    EXPECT_NEAR(result, expectedResult, tol6);

    // Expect that daily wind speed is derived from uas_mPERs and vas_mPERs
    // (and is not interpolated from mean monthly values)
    expectedResult = sqrt(squared(2.82) + squared(-.4));

    EXPECT_NEAR(
        SW_Run.RunIn.weathRunAllHist[yearIndex].windspeed_daily[midJanDay],
        expectedResult,
        tol6
    );

    EXPECT_NE(
        SW_Run.RunIn.weathRunAllHist[yearIndex].windspeed_daily[midJanDay],
        SW_Run.RunIn.SkyRunIn.windspeed[0]
    );


    // Check relative humidity [0, 100] % ---
    // Expect that relative humidity is calculated from actual vapor pressure
    // (which is calculated from hursmax, hursmin, Tmax, and Tmin) and
    // mean air temperature, i.e., consistent with actual vapor pressure
    // and relative to liquid water

    // Check on day 1 (values from Input/data_weather_maca-type1/weath.1980)
    // Relative humidity from vapor pressure based on hursmax (83.82) and
    // hursmin (33.27)
    EXPECT_NEAR(
        SW_Run.RunIn.weathRunAllHist[yearIndex].r_humidity_daily[0],
        fmin(
            100.,
            relativeHumidity1(
                SW_Run.RunIn.weathRunAllHist[yearIndex].actualVaporPressure[0],
                SW_Run.RunIn.weathRunAllHist[yearIndex].temp_avg[0]
            )
        ),
        tol6
    );

    // Check on day 15 (values from Input/data_weather_maca-type1/weath.1980)
    // Relative humidity from vapor pressure based on hursmax (80.55) and
    // hursmin (32.28)
    EXPECT_NEAR(
        SW_Run.RunIn.weathRunAllHist[yearIndex].r_humidity_daily[midJanDay],
        fmin(
            100.,
            relativeHumidity1(
                SW_Run.RunIn.weathRunAllHist[yearIndex]
                    .actualVaporPressure[midJanDay],
                SW_Run.RunIn.weathRunAllHist[yearIndex].temp_avg[midJanDay]
            )
        ),
        tol6
    );

    // Check that value on day 15 is not interpolated from mean monthly values
    EXPECT_NE(
        SW_Run.RunIn.weathRunAllHist[yearIndex].r_humidity_daily[midJanDay],
        SW_Run.RunIn.SkyRunIn.r_humidity[0]
    );


    // We have observed radiation and missing cloud cover
    EXPECT_FALSE(missing(SW_Run.RunIn.weathRunAllHist[yearIndex].shortWaveRad[0]
    ));
    EXPECT_TRUE(
        missing(SW_Run.RunIn.weathRunAllHist[yearIndex].cloudcov_daily[0])
    );
}

TEST_F(WeatherFixtureTest, WeatherInputMACAtype2) {

    /*
       This section assures that a variable aside from humidity-related ones,
       wind speed in this case, is calculated reasonably based on its
       components.

        * This section uses the test directory "*_maca-type2" (huss)
     */

    double result;
    double expectedResult;
    const double elevation = SW_Run.RunIn.ModelRunIn.elevation;
    int const yearIndex = 0;
    int const year = 1980;
    int const midJanDay = 14;
    int doy;
    double snowpack[TWO_DAYS] = {0.};
    double ***tempWeatherHist = NULL;

    /* Test correct priority is being given to input values from MACA */

    SW_WTH_setup(
        SW_Run.WeatherIn,
        SW_Domain.SW_PathInputs.txtInFiles,
        SW_Domain.SW_PathInputs.txtWeatherPrefix,
        &LogInfo
    );
    sw_fail_on_error(
        ROOT_PROC, &LogInfo
    ); // exit test program if unexpected error

    // Switch directory to the input folder of the
    // second type of MACA (huss)
    (void) snprintf(
        SW_Run.WeatherIn->name_prefix,
        sizeof SW_Run.WeatherIn->name_prefix,
        "%s",
        "Input/data_weather_maca-type2/weath"
    );

    // Turn off monthly flags
    SW_Run.WeatherIn->use_cloudCoverMonthly = swFALSE;
    SW_Run.WeatherIn->use_windSpeedMonthly = swFALSE;
    SW_Run.WeatherIn->use_humidityMonthly = swFALSE;

    // Manually edit index/flag arrays in SW_WEATHER_INPUTS to make test as
    // realistic as possible
    // Note: Indices are based on the directory:
    // Input/data_weather_maca-type1/weath.1980
    SW_Run.WeatherIn->dailyInputIndices[TEMP_MAX] = 0;
    SW_Run.WeatherIn->dailyInputIndices[TEMP_MIN] = 1;
    SW_Run.WeatherIn->dailyInputIndices[PPT] = 2;
    SW_Run.WeatherIn->dailyInputIndices[CLOUD_COV] = 0;
    SW_Run.WeatherIn->dailyInputIndices[WIND_SPEED] = 0;
    SW_Run.WeatherIn->dailyInputIndices[WIND_EAST] = 3;
    SW_Run.WeatherIn->dailyInputIndices[WIND_NORTH] = 4;
    SW_Run.WeatherIn->dailyInputIndices[REL_HUMID] = 0;
    SW_Run.WeatherIn->dailyInputIndices[REL_HUMID_MAX] = 0;
    SW_Run.WeatherIn->dailyInputIndices[REL_HUMID_MIN] = 0;
    SW_Run.WeatherIn->dailyInputIndices[SPEC_HUMID] = 5;
    SW_Run.WeatherIn->dailyInputIndices[TEMP_DEWPOINT] = 0;
    SW_Run.WeatherIn->dailyInputIndices[ACTUAL_VP] = 0;
    SW_Run.WeatherIn->dailyInputIndices[SHORT_WR] = 6;

    SW_Run.WeatherIn->dailyInputFlags[TEMP_MAX] = swTRUE;
    SW_Run.WeatherIn->dailyInputFlags[TEMP_MIN] = swTRUE;
    SW_Run.WeatherIn->dailyInputFlags[PPT] = swTRUE;
    SW_Run.WeatherIn->dailyInputFlags[CLOUD_COV] = swFALSE;
    SW_Run.WeatherIn->dailyInputFlags[WIND_SPEED] = swFALSE;
    SW_Run.WeatherIn->dailyInputFlags[WIND_EAST] = swTRUE;
    SW_Run.WeatherIn->dailyInputFlags[WIND_NORTH] = swTRUE;
    SW_Run.WeatherIn->dailyInputFlags[REL_HUMID] = swFALSE;
    SW_Run.WeatherIn->dailyInputFlags[REL_HUMID_MAX] = swFALSE;
    SW_Run.WeatherIn->dailyInputFlags[REL_HUMID_MIN] = swFALSE;
    SW_Run.WeatherIn->dailyInputFlags[SPEC_HUMID] = swTRUE;
    SW_Run.WeatherIn->dailyInputFlags[TEMP_DEWPOINT] = swFALSE;
    SW_Run.WeatherIn->dailyInputFlags[ACTUAL_VP] = swFALSE;
    SW_Run.WeatherIn->dailyInputFlags[SHORT_WR] = swTRUE;

    SW_Run.WeatherIn->n_input_forcings = 7;
    SW_Run.WeatherIn->desc_rsds = 1; // MACA rsds is flux density over 24 hours

    // Request weather input fixes
    SW_Run.WeatherIn->fixWeatherData[idFixPERCENT] = swTRUE;

    // Allocate temporary location for weather
    allocate_temp_weather(1, 1, &tempWeatherHist, &LogInfo);
    sw_fail_on_error(ROOT_PROC, &LogInfo);

    // Reset daily weather values
    clear_hist_weather(1, &SW_Run.RunIn.weathRunAllHist[0], tempWeatherHist[0]);

    // Using the new inputs folder, read in year = 1980
    read_weather_hist(
        year,
        tempWeatherHist[0],
        SW_Run.WeatherIn->name_prefix,
        SW_Run.WeatherIn->n_input_forcings,
        SW_Run.WeatherIn->dailyInputIndices,
        SW_Run.WeatherIn->dailyInputFlags,
        &LogInfo
    );
    if (LogInfo.stopRun != 0u) {
        deallocate_temp_weather(1, &tempWeatherHist);
    }
    sw_fail_on_error(
        ROOT_PROC, &LogInfo
    ); // exit test program if unexpected error

    // Transfer values from temporary to permanent location (`SW_WEATHER_HIST`)
    SW_WTH_setWeatherValues(
        SW_Run.ModelIn->startyr,
        1,
        SW_Run.WeatherIn->dailyInputFlags,
        SW_Run.WeatherIn->fixWeatherData,
        SW_Run.WeatherIn->isRelHumidInputOverIce,
        tempWeatherHist,
        SW_Run.RunIn.ModelRunIn.elevation,
        0,
        &SW_Run.RunIn.weathRunAllHist[0],
        &LogInfo
    );
    deallocate_temp_weather(1, &tempWeatherHist);
    sw_fail_on_error(
        ROOT_PROC, &LogInfo
    ); // exit test program if unexpected error

    // Check that weather contains reasonable values
    checkYearlyWeather(
        SW_Run.RunIn.weathRunAllHist,
        SW_Domain.startstart,
        SW_Domain.endend,
        SW_Domain.startyr,
        SW_Domain.endyr,
        SW_Run.ModelIn->startyr,
        1,
        &LogInfo
    );
    sw_fail_on_error(
        ROOT_PROC, &LogInfo
    ); // exit test program if unexpected error


    // Check that no value is missing
    for (doy = 1; doy < MAX_DAYS; doy++) {
        SW_WTH_new_day(
            SW_Run.WeatherIn,
            &SW_Run.WeatherSim,
            SW_Run.RunIn.weathRunAllHist,
            SW_Run.SiteIn,
            snowpack,
            SW_Run.RunIn.ModelRunIn.isnorth,
            doy,
            yearIndex,
            MAX_DAYS,
            &LogInfo
        );
        sw_fail_on_error(
            ROOT_PROC, &LogInfo
        ); // exit test program if unexpected error
    }


    // Check wind speed
    result = SW_Run.RunIn.weathRunAllHist[yearIndex].windspeed_daily[0];

    // Get expected result from Input/data_weather_maca-type1/weath.1980 day 1
    // uas_mPERs = 3.31 and vas_mPERs = -.85
    expectedResult = sqrt(squared(3.31) + squared(-.85));

    // Test if wind speed was calculated within reasonable range to
    // the expected result
    EXPECT_NEAR(result, expectedResult, tol6);

    // Expect that daily wind speed is derived from uas_mPERs and vas_mPERs
    // (and is not interpolated from mean monthly values)
    expectedResult = sqrt(squared(2.82) + squared(-.4));

    EXPECT_NEAR(
        SW_Run.RunIn.weathRunAllHist[yearIndex].windspeed_daily[midJanDay],
        expectedResult,
        tol6
    );

    EXPECT_NE(
        SW_Run.RunIn.weathRunAllHist[yearIndex].windspeed_daily[midJanDay],
        SW_Run.RunIn.SkyRunIn.windspeed[0]
    );


    // Check relative humidity [0, 100] % ---
    // Expect that relative humidity is calculated from huss at mean
    // temperature (for consistency with vapor pressure, see below)

    // Check on day 1 (values from Input/data_weather_gridmet/weath.1980)
    // Calculate relative humidity from huss (1.92), tmax (-.01), tmin (-11.99)
    EXPECT_NEAR(
        SW_Run.RunIn.weathRunAllHist[yearIndex].r_humidity_daily[0],
        relativeHumidity1(
            actualVaporPressure4(1.92, elevation), (-0.01 - 11.99) / 2.
        ),
        tol6
    );

    // Check on day 15 (values from Input/data_weather_gridmet/weath.1980)
    // Calculate relative humidity from huss (1.30), tmax (-4.31), tmin (-17.34)
    EXPECT_NEAR(
        SW_Run.RunIn.weathRunAllHist[yearIndex].r_humidity_daily[midJanDay],
        relativeHumidity1(
            actualVaporPressure4(1.30, elevation), (-4.31 - 17.34) / 2.
        ),
        tol6
    );

    // Check that value on day 15 is not interpolated from mean monthly values
    EXPECT_NE(
        SW_Run.RunIn.weathRunAllHist[yearIndex].r_humidity_daily[midJanDay],
        SW_Run.RunIn.SkyRunIn.r_humidity[0]
    );


    // Check actual vapor pressure [kPa] ---
    // Expect that actual vapor pressure is calculated directly from huss
    // (and not from relative humidity that was estimated from huss)
    EXPECT_NEAR(
        SW_Run.RunIn.weathRunAllHist[yearIndex].actualVaporPressure[0],
        actualVaporPressure4(1.92, elevation),
        tol6
    );

    EXPECT_NEAR(
        SW_Run.RunIn.weathRunAllHist[yearIndex].actualVaporPressure[midJanDay],
        actualVaporPressure4(1.30, elevation),
        tol6
    );

    // Expect that relative humidity and actual vapor pressure are consistent,
    // i.e., vapor pressure as calculated by petfunc() from relative humidity
    // equals actual vapor pressure as used by solar_radiation()
    // (1980 is a leap year)
    for (doy = 0; doy < MAX_DAYS; doy++) {
        double const rH =
            SW_Run.RunIn.weathRunAllHist[yearIndex].r_humidity_daily[doy];
        double tempSlope;

        if (rH < 100.) {
            EXPECT_NEAR(
                rH / 100. *
                    svp(SW_Run.RunIn.weathRunAllHist[yearIndex].temp_avg[doy],
                        swFALSE,
                        &tempSlope),
                SW_Run.RunIn.weathRunAllHist[yearIndex]
                    .actualVaporPressure[doy],
                tol6
            );
        }
    }


    // We have observed radiation and missing cloud cover
    EXPECT_FALSE(missing(SW_Run.RunIn.weathRunAllHist[yearIndex].shortWaveRad[0]
    ));
    EXPECT_TRUE(
        missing(SW_Run.RunIn.weathRunAllHist[yearIndex].cloudcov_daily[0])
    );
}

TEST_F(WeatherFixtureTest, WeatherDailyLOCFInputValues) {

    /*
       Since SOILWAT2 now has the ability to deal with more than
       maximum/minimum temperature and precipitation, part of the weather
       generator can now perform LOCF on these values (generator method = 1).

       We want to make sure when the weather generator method is equal to 1,
       LOCF is performed on these variables and not ignored
    */
    int const numDaysLOCFTolerance = 366;
    int const yearIndex = 0;
    int day;
    double const cloudCovTestVal = .5;
    double const actVapPressTestVal = 4.23;
    double const windSpeedTestVal = 2.12;

    // Setup and read in weather
    SW_WTH_setup(
        SW_Run.WeatherIn,
        SW_Domain.SW_PathInputs.txtInFiles,
        SW_Domain.SW_PathInputs.txtWeatherPrefix,
        &LogInfo
    );
    sw_fail_on_error(
        ROOT_PROC, &LogInfo
    ); // exit test program if unexpected error

    // Turn off flags for monthly values along with daily flags
    // so all daily variables aside from max/min temperature and precipiation
    // are set to SW_MISSING
    SW_Run.WeatherIn->use_cloudCoverMonthly = swFALSE;
    SW_Run.WeatherIn->use_humidityMonthly = swFALSE;
    SW_Run.WeatherIn->use_windSpeedMonthly = swFALSE;

    SW_WTH_read(
        SW_Run.WeatherIn,
        &SW_Run.RunIn.weathRunAllHist,
        &SW_Run.RunIn.SkyRunIn,
        SW_Run.ModelIn,
        SW_Run.RunIn.ModelRunIn.elevation,
        swTRUE,
        SW_Domain.SW_ConstInfo.ModelSim.cum_monthdays,
        SW_Domain.SW_ConstInfo.ModelSim.days_in_month,
        &LogInfo
    );
    sw_fail_on_error(
        ROOT_PROC, &LogInfo
    ); // exit test program if unexpected error

    // Setup values/flags for `generateMissingWeather()` to deal with
    SW_Run.WeatherIn->generateWeatherMethod = wgLOCF;
    SW_Run.RunIn.weathRunAllHist[yearIndex].cloudcov_daily[0] = cloudCovTestVal;
    SW_Run.RunIn.weathRunAllHist[yearIndex].actualVaporPressure[0] =
        actVapPressTestVal;
    SW_Run.RunIn.weathRunAllHist[yearIndex].windspeed_daily[0] =
        windSpeedTestVal;

    generateMissingWeather(
        &SW_Run.MarkovIn,
        SW_Run.RunIn.weathRunAllHist,
        &SW_Run.WeatherSim,
        1980,
        1,
        SW_Run.WeatherIn->use_humidityMonthly,
        SW_Run.WeatherIn->isRelHumidInputOverIce,
        SW_Run.WeatherIn->generateWeatherMethod,
        numDaysLOCFTolerance,
        &LogInfo
    );
    sw_fail_on_error(
        ROOT_PROC, &LogInfo
    ); // exit test program if unexpected error

    // Test to see if the first year of cloud cover, actual vapor pressure and
    // wind speed has been filled with cloudCovTestVal, actVapPressTestVal,
    // and windSpeedTestVal, respectively
    for (day = 0; day < MAX_DAYS; day++) {
        EXPECT_EQ(
            SW_Run.RunIn.weathRunAllHist[yearIndex].cloudcov_daily[day],
            cloudCovTestVal
        );
        EXPECT_EQ(
            SW_Run.RunIn.weathRunAllHist[yearIndex].actualVaporPressure[day],
            actVapPressTestVal
        );
        EXPECT_EQ(
            SW_Run.RunIn.weathRunAllHist[yearIndex].windspeed_daily[day],
            windSpeedTestVal
        );
    }
}

TEST_F(WeatherFixtureTest, WeatherDailyInputWrongColumnNumberDeathTest) {
    /*
       This section covers number of flags and the testing of reasonable results
       (`checkYearlyWeather()`).

       An incorrect number of "n_input_forcings" within SW_WEATHER_INPUTS
       relative to the number of input columns read in should result in a crash.

       If an input or calculated value is out of range (e.g., range = [-100,
       100] C for min/max temperature), `checkYearlyWeather()` should result in
       a crash.
     */

    // Initialize any variables
    TimeInt const year = 1980;
    double ***tempWeatherHist = NULL;

    /* Not the same number of flags as columns */
    // Run weather functions and expect an failure (error)

    // Allocate temporary location for weather
    allocate_temp_weather(1, 1, &tempWeatherHist, &LogInfo);
    sw_fail_on_error(ROOT_PROC, &LogInfo);

    // Reset daily weather values
    clear_hist_weather(1, &SW_Run.RunIn.weathRunAllHist[0], tempWeatherHist[0]);

    SW_WTH_read(
        SW_Run.WeatherIn,
        &SW_Run.RunIn.weathRunAllHist,
        &SW_Run.RunIn.SkyRunIn,
        SW_Run.ModelIn,
        SW_Run.RunIn.ModelRunIn.elevation,
        swTRUE,
        SW_Domain.SW_ConstInfo.ModelSim.cum_monthdays,
        SW_Domain.SW_ConstInfo.ModelSim.days_in_month,
        &LogInfo
    );
    if (LogInfo.stopRun != 0u) {
        deallocate_temp_weather(1, &tempWeatherHist);
    }
    sw_fail_on_error(
        ROOT_PROC, &LogInfo
    ); // exit test program if unexpected error

    // Set SW_WEATHER_INPUTS's n_input_forcings to a number that is
    // not the columns being read in
    SW_Run.WeatherIn->n_input_forcings = 0;

    read_weather_hist(
        year,
        tempWeatherHist[0],
        SW_Run.WeatherIn->name_prefix,
        SW_Run.WeatherIn->n_input_forcings,
        SW_Run.WeatherIn->dailyInputIndices,
        SW_Run.WeatherIn->dailyInputFlags,
        &LogInfo
    );
    // expect error: don't exit test program via `sw_fail_on_error(ROOT_PROC,
    // &LogInfo)`

    // Detect failure by error message
    EXPECT_THAT(LogInfo.errorMsg, HasSubstr("Incomplete record 1"));

    deallocate_temp_weather(1, &tempWeatherHist);
}

TEST_F(WeatherFixtureTest, WeatherDailyInputBadTemperatureDeathTest) {
    /* Check for value(s) that are not within reasonable range these
       tests will make use of `checkYearlyWeather()` */

    // Edit SW_WEATHER_HIST values from their original value

    SW_WTH_read(
        SW_Run.WeatherIn,
        &SW_Run.RunIn.weathRunAllHist,
        &SW_Run.RunIn.SkyRunIn,
        SW_Run.ModelIn,
        SW_Run.RunIn.ModelRunIn.elevation,
        swTRUE,
        SW_Domain.SW_ConstInfo.ModelSim.cum_monthdays,
        SW_Domain.SW_ConstInfo.ModelSim.days_in_month,
        &LogInfo
    );
    sw_fail_on_error(
        ROOT_PROC, &LogInfo
    ); // exit test program if unexpected error

    // Make temperature unreasonable (not within [-100, 100])
    SW_Run.RunIn.weathRunAllHist[0].temp_max[0] = -102.;

    checkYearlyWeather(
        SW_Run.RunIn.weathRunAllHist,
        SW_Domain.startstart,
        SW_Domain.endend,
        SW_Domain.startyr,
        SW_Domain.endyr,
        SW_Run.ModelIn->startyr,
        SW_Run.WeatherIn->n_years,
        &LogInfo
    );
    // expect error: don't exit test program via `sw_fail_on_error(ROOT_PROC,
    // &LogInfo)`

    // Detect failure by error message
    EXPECT_THAT(
        LogInfo.errorMsg, HasSubstr("Daily minimum > maximum air temperature")
    );
}

TEST_F(WeatherFixtureTest, WeatherDailyInputBadPrecipitationDeathTest) {
    /* Check for value(s) that are not within reasonable range these
       tests will make use of `checkYearlyWeather()` */

    // Edit SW_WEATHER_HIST values from their original value

    SW_WTH_read(
        SW_Run.WeatherIn,
        &SW_Run.RunIn.weathRunAllHist,
        &SW_Run.RunIn.SkyRunIn,
        SW_Run.ModelIn,
        SW_Run.RunIn.ModelRunIn.elevation,
        swTRUE,
        SW_Domain.SW_ConstInfo.ModelSim.cum_monthdays,
        SW_Domain.SW_ConstInfo.ModelSim.days_in_month,
        &LogInfo
    );
    sw_fail_on_error(
        ROOT_PROC, &LogInfo
    ); // exit test program if unexpected error

    // Make precipitation unresonable (< 0)
    SW_Run.RunIn.weathRunAllHist[0].ppt[0] = -1.;

    checkYearlyWeather(
        SW_Run.RunIn.weathRunAllHist,
        SW_Domain.startstart,
        SW_Domain.endend,
        SW_Domain.startyr,
        SW_Domain.endyr,
        SW_Run.ModelIn->startyr,
        SW_Run.WeatherIn->n_years,
        &LogInfo
    );
    // expect error: don't exit test program via `sw_fail_on_error(ROOT_PROC,
    // &LogInfo)`

    // Detect failure by error message
    EXPECT_THAT(
        LogInfo.errorMsg, HasSubstr("Invalid daily precipitation amount")
    );
}

TEST_F(WeatherFixtureTest, WeatherDailyInputBadHumidityDeathTest) {
    /* Check for value(s) that are not within reasonable range these
       tests will make use of `checkYearlyWeather()` */

    // Edit SW_WEATHER_HIST values from their original value

    SW_WTH_read(
        SW_Run.WeatherIn,
        &SW_Run.RunIn.weathRunAllHist,
        &SW_Run.RunIn.SkyRunIn,
        SW_Run.ModelIn,
        SW_Run.RunIn.ModelRunIn.elevation,
        swTRUE,
        SW_Domain.SW_ConstInfo.ModelSim.cum_monthdays,
        SW_Domain.SW_ConstInfo.ModelSim.days_in_month,
        &LogInfo
    );
    sw_fail_on_error(
        ROOT_PROC, &LogInfo
    ); // exit test program if unexpected error

    // Make relative humidity unreasonable (< 0%)
    SW_Run.RunIn.weathRunAllHist[0].r_humidity_daily[0] = -.1252;

    checkYearlyWeather(
        SW_Run.RunIn.weathRunAllHist,
        SW_Domain.startstart,
        SW_Domain.endend,
        SW_Domain.startyr,
        SW_Domain.endyr,
        SW_Run.ModelIn->startyr,
        SW_Run.WeatherIn->n_years,
        &LogInfo
    );
    // expect error: don't exit test program via `sw_fail_on_error(ROOT_PROC,
    // &LogInfo)`

    // Detect failure by error message
    EXPECT_THAT(LogInfo.errorMsg, HasSubstr("relative humidity outside"));
}
} // namespace
