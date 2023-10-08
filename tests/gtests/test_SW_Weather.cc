#include <gmock/gmock.h>
#include <assert.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <float.h>
#include <math.h>
#include <memory.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "include/SW_Weather.h"
#include "tests/gtests/sw_testhelpers.h"
#include "include/SW_Markov.h"
#include "include/SW_Model.h"
#include "include/Times.h"
#include "include/SW_Flow_lib_PET.h"
#include "include/SW_Sky.h"

using ::testing::HasSubstr;

namespace {

    TEST_F(WeatherFixtureTest, WeatherDefaultValues) {

        // Testing to fill allHist from `SW_Weather`
        SW_SKY_read(PathInfo.InFiles, &SW_All.Sky, &LogInfo);

        readAllWeather(
          SW_All.Weather.allHist,
          1980,
          SW_All.Weather.n_years,
          SW_All.Weather.use_weathergenerator_only,
          SW_All.Weather.name_prefix,
          SW_All.Weather.use_cloudCoverMonthly,
          SW_All.Weather.use_humidityMonthly,
          SW_All.Weather.use_windSpeedMonthly,
          SW_All.Weather.n_input_forcings,
          SW_All.Weather.dailyInputIndices,
          SW_All.Weather.dailyInputFlags,
          SW_All.Sky.cloudcov,
          SW_All.Sky.windspeed,
          SW_All.Sky.r_humidity,
          SW_All.Model.cum_monthdays,
          SW_All.Model.days_in_month,
          &LogInfo
        );

        // Test first day of first year in `allHist` to make sure correct
        // temperature max/min/avg and precipitation values
        EXPECT_NEAR(SW_All.Weather.allHist[0]->temp_max[0], -0.520000, tol6);
        EXPECT_NEAR(SW_All.Weather.allHist[0]->temp_avg[0], -8.095000, tol6);
        EXPECT_NEAR(SW_All.Weather.allHist[0]->temp_min[0], -15.670000, tol6);
        EXPECT_NEAR(SW_All.Weather.allHist[0]->ppt[0], .220000, tol6);
    }

    TEST_F(WeatherFixtureTest, WeatherNoMemoryLeakIfDecreasedNumberOfYears) {

        // Default number of years is 31
        EXPECT_EQ(SW_All.Weather.n_years, 31);

        // Decrease number of years
        SW_All.Model.startyr = 1981;
        SW_All.Model.endyr = 1982;

        // Real expectation is that there is no memory leak for `allHist`
        SW_WTH_read(&SW_All.Weather, &SW_All.Sky, &SW_All.Model, &LogInfo);

        EXPECT_EQ(SW_All.Weather.n_years, 2);
    }

    TEST_F(WeatherFixtureTest, WeatherSomeMissingValuesDays) {

        SW_All.Weather.generateWeatherMethod = 2;

        // Change directory to get input files with some missing data
        strcpy(SW_All.Weather.name_prefix, "Input/data_weather_missing/weath");

        SW_MKV_setup(&SW_All.Markov, SW_All.Weather.rng_seed,
                     SW_All.Weather.generateWeatherMethod,
                     PathInfo.InFiles, &LogInfo);

        SW_WTH_read(&SW_All.Weather, &SW_All.Sky, &SW_All.Model, &LogInfo);
        SW_WTH_finalize_all_weather(&SW_All.Markov, &SW_All.Weather,
        SW_All.Model.cum_monthdays, SW_All.Model.days_in_month, &LogInfo);


        // Expect that missing input values (from 1980) are filled by the weather generator
        EXPECT_FALSE(missing(SW_All.Weather.allHist[0]->temp_max[0]));
        EXPECT_FALSE(missing(SW_All.Weather.allHist[0]->temp_max[1]));
        EXPECT_FALSE(missing(SW_All.Weather.allHist[0]->temp_min[0]));
        EXPECT_FALSE(missing(SW_All.Weather.allHist[0]->temp_min[2]));
        EXPECT_FALSE(missing(SW_All.Weather.allHist[0]->ppt[0]));
        EXPECT_FALSE(missing(SW_All.Weather.allHist[0]->ppt[3]));



    }

    TEST_F(WeatherFixtureTest, WeatherSomeMissingValuesYears) {

        int year, day;
        SW_All.Weather.generateWeatherMethod = 2;

        // Change directory to get input files with some missing data
        strcpy(SW_All.Weather.name_prefix, "Input/data_weather_missing/weath");

        SW_MKV_setup(&SW_All.Markov, SW_All.Weather.rng_seed,
                     SW_All.Weather.generateWeatherMethod,
                     PathInfo.InFiles, &LogInfo);

        SW_All.Model.startyr = 1981;
        SW_All.Model.endyr = 1982;

        SW_WTH_read(&SW_All.Weather, &SW_All.Sky, &SW_All.Model, &LogInfo);
        SW_WTH_finalize_all_weather(&SW_All.Markov, &SW_All.Weather,
        SW_All.Model.cum_monthdays, SW_All.Model.days_in_month, &LogInfo);


        // Check everyday's value and test if it's `MISSING`
        for(year = 0; year < 2; year++) {
            for(day = 0; day < 365; day++) {
                EXPECT_TRUE(!missing(SW_All.Weather.allHist[year]->temp_max[day]));
            }
        }




    }

    TEST_F(WeatherFixtureTest, WeatherWeatherGeneratorOnly) {

        int year, day;

        SW_All.Weather.generateWeatherMethod = 2;
        SW_All.Weather.use_weathergenerator_only = swTRUE;

        SW_MKV_setup(&SW_All.Markov, SW_All.Weather.rng_seed,
                     SW_All.Weather.generateWeatherMethod,
                     PathInfo.InFiles, &LogInfo);

        // Change directory to get input files with some missing data
        strcpy(SW_All.Weather.name_prefix, "Input/data_weather_nonexisting/weath");

        SW_WTH_read(&SW_All.Weather, &SW_All.Sky, &SW_All.Model, &LogInfo);
        SW_WTH_finalize_all_weather(&SW_All.Markov, &SW_All.Weather,
        SW_All.Model.cum_monthdays, SW_All.Model.days_in_month, &LogInfo);

        // Check everyday's value and test if it's `MISSING`
        for(year = 0; year < 31; year++) {
            for(day = 0; day < 365; day++) {
                EXPECT_TRUE(!missing(SW_All.Weather.allHist[year]->temp_max[day]));
            }
        }
    }


    TEST_F(WeatherFixtureTest, ReadAllWeatherTooManyMissingForLOCFDeathTest) {

        // Error: too many missing values and weather generator turned off

        // Change to directory without input files
        strcpy(SW_All.Weather.name_prefix, "Input/data_weather_nonexisting/weath");

        // Set LOCF (temp) + 0 (PPT) method
        SW_All.Weather.generateWeatherMethod = 1;

        SW_All.Model.startyr = 1981;
        SW_All.Model.endyr = 1981;

        SW_WTH_read(&SW_All.Weather, &SW_All.Sky, &SW_All.Model, &LogInfo);

        SW_WTH_finalize_all_weather(
          &SW_All.Markov, &SW_All.Weather,
          SW_All.Model.cum_monthdays, SW_All.Model.days_in_month, &LogInfo
        );

        // Detect failure by error message
        EXPECT_THAT(LogInfo.errorMsg,
                    HasSubstr("more than 3 days missing in year 1981"));
    }

    TEST_F(WeatherFixtureTest, ClimateVariableClimateFromDefaultWeather) {

        // This test relies on allHist from `SW_WEATHER` being already filled
        SW_CLIMATE_YEARLY climateOutput;
        SW_CLIMATE_CLIM climateAverages;

        Bool inNorthHem = swTRUE;

        // Allocate memory
            // 31 = number of years used in test
        allocateClimateStructs(31, &climateOutput, &climateAverages, &LogInfo);


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

        calcSiteClimate(SW_All.Weather.allHist, SW_All.Model.cum_monthdays,
            SW_All.Model.days_in_month, 31, 1980, inNorthHem, &climateOutput);

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

        // This test relies on allHist from `SW_WEATHER` being already filled
        SW_CLIMATE_YEARLY climateOutput;
        SW_CLIMATE_CLIM climateAverages;

        Bool inNorthHem = swTRUE;

        // Allocate memory
            // 1 = number of years used in test
        allocateClimateStructs(1, &climateOutput, &climateAverages, &LogInfo);

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

        calcSiteClimate(SW_All.Weather.allHist, SW_All.Model.cum_monthdays,
            SW_All.Model.days_in_month, 1, 1980, inNorthHem, &climateOutput);
        averageClimateAcrossYears(&climateOutput, 1, &climateAverages);

        // Expect that aggregated values across one year are identical
        // to values of that one year
        EXPECT_DOUBLE_EQ(
          climateAverages.meanTempMon_C[Jan],
          climateOutput.meanTempMon_C[Jan][0]
        );
        EXPECT_DOUBLE_EQ(
          climateAverages.maxTempMon_C[Jan],
          climateOutput.maxTempMon_C[Jan][0]
        );
        EXPECT_DOUBLE_EQ(
          climateAverages.minTempMon_C[Jan],
          climateOutput.minTempMon_C[Jan][0]
        );
        EXPECT_DOUBLE_EQ(
          climateAverages.PPTMon_cm[Jan],
          climateOutput.PPTMon_cm[Jan][0]
        );
        EXPECT_DOUBLE_EQ(
          climateAverages.PPT_cm,
          climateOutput.PPT_cm[0]
        );
        EXPECT_DOUBLE_EQ(
          climateAverages.meanTemp_C,
          climateOutput.meanTemp_C[0]
        );

        // Climate variables used for C4 grass cover
        EXPECT_DOUBLE_EQ(
          climateAverages.minTemp7thMon_C,
          climateOutput.minTemp7thMon_C[0]
        );
        EXPECT_DOUBLE_EQ(
          climateAverages.frostFree_days,
          climateOutput.frostFree_days[0]
        );
        EXPECT_DOUBLE_EQ(
          climateAverages.ddAbove65F_degday,
          climateOutput.ddAbove65F_degday[0]
        );

        // Climate variables used for cheatgrass cover
        EXPECT_DOUBLE_EQ(
          climateAverages.PPT7thMon_mm,
          climateOutput.PPT7thMon_mm[0]
        );
        EXPECT_DOUBLE_EQ(
          climateAverages.meanTempDriestQtr_C,
          climateOutput.meanTempDriestQtr_C[0]
        );
        EXPECT_DOUBLE_EQ(
          climateAverages.minTemp2ndMon_C,
          climateOutput.minTemp2ndMon_C[0]
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
        EXPECT_TRUE(isnan(climateAverages.sdC4[0]));
        EXPECT_TRUE(isnan(climateAverages.sdC4[1]));
        EXPECT_TRUE(isnan(climateAverages.sdC4[2]));

        // Climate variables used for cheatgrass cover
        // (stdev of one value is undefined)
        EXPECT_NEAR(climateAverages.PPT7thMon_mm, 18.299999, tol6);
        EXPECT_NEAR(climateAverages.meanTempDriestQtr_C, 0.936387, tol6);
        EXPECT_NEAR(climateAverages.minTemp2ndMon_C, -12.822068, tol6);
        EXPECT_TRUE(isnan(climateAverages.sdCheatgrass[0]));
        EXPECT_TRUE(isnan(climateAverages.sdCheatgrass[1]));
        EXPECT_TRUE(isnan(climateAverages.sdCheatgrass[2]));


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

        // This test relies on allHist from `SW_WEATHER` being already filled
        SW_CLIMATE_YEARLY climateOutput;
        SW_CLIMATE_CLIM climateAverages;

        // "South" and not "North" to reduce confusion when calling `calcSiteClimate()`
        Bool inSouthHem = swFALSE;

        // Allocate memory
            // 31 = number of years used in test
        allocateClimateStructs(31, &climateOutput, &climateAverages, &LogInfo);


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

        calcSiteClimate(SW_All.Weather.allHist, SW_All.Model.cum_monthdays,
            SW_All.Model.days_in_month, 31, 1980, inSouthHem, &climateOutput);

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
         * Expect similar output to rSOILWAT2 before v6.0.0 (e.g., v5.3.1), identical otherwise
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
        SW_WEATHER_HIST **allHist;

        Bool inNorthHem = swTRUE;

        // Allocate memory
        allocateClimateStructs(2, &climateOutput, &climateAverages, &LogInfo);

        allHist = (SW_WEATHER_HIST **)malloc(sizeof(SW_WEATHER_HIST *) * 2);

        for (int year = 0; year < 2; year++) {
            allHist[year] = (SW_WEATHER_HIST *)malloc(sizeof(SW_WEATHER_HIST));
        }


        // ------ Check climate variables for constant weather = 1 ------
        // Years 1980 (leap) + 1981 (nonleap)

        // Expect that min/avg/max of weather variables are 1
        // Expect that sums of weather variables correspond to number of days
        // Expect that the average annual number of days is 365.5 (1 leap + 1 nonleap)
        // Expect that the average number of days in February is 28.5 (1 leap + 1 nonleap)

        // Set all weather values to 1
        for(int year = 0; year < 2; year++) {
            for(int day = 0; day < 366; day++) {
                allHist[year]->temp_max[day] = 1.;
                allHist[year]->temp_min[day] = 1.;
                allHist[year]->temp_avg[day] = 1.;
                allHist[year]->ppt[day] = 1.;
            }
        }

        // --- Annual time-series of climate variables ------
        calcSiteClimate(allHist, SW_All.Model.cum_monthdays,
            SW_All.Model.days_in_month, 2, 1980, inNorthHem, &climateOutput);

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
        averageClimateAcrossYears(&climateOutput, 2, &climateAverages);

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

        for (int year = 0; year < 2; year++) {
            free(allHist[year]);
        }
        free(allHist);

    }


    TEST_F(WeatherFixtureTest, ClimateVariableAverageTemperatureOfDriestQuarterTest) {

        double monthlyPPT[MAX_MONTHS] = {.5, .5, .1, .4, .9, 1.0, 1.2, 6.5, 7.5, 1.2, 4., .6};
        double monthlyTemp[MAX_MONTHS] = {-3.2, -.4, 1.2, 3.5, 7.5, 4.5, 6.5, 8.2, 2.0, 3., .1, -.3};
        double result[2]; // 2 = max number of years in test

        int month, year;

        double **PPTMon_cm;
        PPTMon_cm = new double*[MAX_MONTHS];

        double **meanTempMon_C = new double*[MAX_MONTHS];

        Bool inNorthHem = swTRUE;

        for(month = 0; month < MAX_MONTHS; month++) {
            PPTMon_cm[month] = new double[2];
            meanTempMon_C[month] = new double[2];
            for(year = 0; year < 2; year++) {

                PPTMon_cm[month][year] = monthlyPPT[month];
                meanTempMon_C[month][year] = monthlyTemp[month];
            }
        }


        // ------ Test for one year ------
        findDriestQtr(1, inNorthHem, result, meanTempMon_C, PPTMon_cm);

        // Value 1.433333... is the average temperature of the driest quarter of the year
        // In this case, the driest quarter is February-April
        EXPECT_NEAR(result[0], 1.4333333333333333, tol9);


        // ------ Test for two years ------
        findDriestQtr(2, inNorthHem, result, meanTempMon_C, PPTMon_cm);

        EXPECT_NEAR(result[0], 1.4333333333333333, tol9);
        EXPECT_NEAR(result[1], 1.4333333333333333, tol9);



        // ------ Test when there are multiple driest quarters ------
        for (month = 0; month < MAX_MONTHS; month++) {
            for(year = 0; year < 2; year++) {
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
        for(int month = 0; month < MAX_MONTHS; month++) {
            delete[] PPTMon_cm[month];
            delete[] meanTempMon_C[month];
        }

        delete[] PPTMon_cm;
        delete[] meanTempMon_C;

    }

    TEST_F(WeatherFixtureTest, WeatherReadInitialization) {

        SW_WTH_read(&SW_All.Weather, &SW_All.Sky,
                    &SW_All.Model, &LogInfo);

        EXPECT_FLOAT_EQ(SW_All.Weather.allHist[0]->temp_max[0], -.52);
    }

    TEST_F(WeatherFixtureTest, WeatherMonthlyInputPrioritization) {
        /*
           This section covers the correct prioritization of monthly input values
           instead of daily read-in values
         */

         // Initialize any variables
         int yearIndex = 0, midJanDay = 14;

         /* Test if monthly values are not being used */
         SW_WTH_setup(&SW_All.Weather, PathInfo.InFiles,
                      PathInfo.weather_prefix, &LogInfo);

         // Read in all weather
         SW_WTH_read(&SW_All.Weather, &SW_All.Sky,
                     &SW_All.Model, &LogInfo);

         // Test the middle of January in year 1980 and see if it's not equal to SW_All.Sky.r_humidity[0],
         // SW_All.Sky.cloudcov[0], and SW_All.Sky.windspeed[0]
         // Note: Daily interpolated values in the middle of a month are equal to the
         // original monthly values from which they were interpolated
         EXPECT_NEAR(SW_All.Weather.allHist[yearIndex]->r_humidity_daily[midJanDay], SW_All.Sky.r_humidity[0], tol6);
         EXPECT_NEAR(SW_All.Weather.allHist[yearIndex]->cloudcov_daily[midJanDay], SW_All.Sky.cloudcov[0], tol6);
         EXPECT_NEAR(SW_All.Weather.allHist[yearIndex]->windspeed_daily[midJanDay], SW_All.Sky.windspeed[0], tol6);
     }

     TEST_F(WeatherFixtureTest, WeatherInputDailyGridMet) {

         /*
            This section tests humidity-related values that can be averaged,
            max/min relative humidity, and the calculation of actual vapor pressure,
            based on the average relative humidity value, is calculated reasonably.

            * This section uses the test directory "*_gridmet".
          */

         double result, expectedResult;
         int yearIndex = 0, year = 1980, midJanDay = 14;

        /* Test correct priority is being given to input values from DAYMET */
         SW_WTH_setup(&SW_All.Weather, PathInfo.InFiles,
                      PathInfo.weather_prefix, &LogInfo);

             // Switch directory to gridmet input folder
         strcpy(SW_All.Weather.name_prefix, "Input/data_weather_gridmet/weath");

        // Turn off monthly flags
        SW_All.Weather.use_cloudCoverMonthly = swFALSE;
        SW_All.Weather.use_windSpeedMonthly = swFALSE;
        SW_All.Weather.use_humidityMonthly = swFALSE;

             // Manually edit index/flag arrays in SW_WEATHER to make test as
             // realistic as possible
             // Note: Indices are based on the directory:
             // Input/data_weather_gridmet/weath.1980
         SW_All.Weather.dailyInputIndices[WIND_SPEED] = 3;
         SW_All.Weather.dailyInputIndices[REL_HUMID_MAX] = 4;
         SW_All.Weather.dailyInputIndices[REL_HUMID_MIN] = 5;
         SW_All.Weather.dailyInputIndices[SHORT_WR] = 6;
         SW_All.Weather.dailyInputFlags[WIND_SPEED] = swTRUE;
         SW_All.Weather.dailyInputFlags[REL_HUMID_MAX] = swTRUE;
         SW_All.Weather.dailyInputFlags[REL_HUMID_MIN] = swTRUE;
         SW_All.Weather.dailyInputFlags[SHORT_WR] = swTRUE;
         SW_All.Weather.n_input_forcings = 7;
         SW_All.Weather.desc_rsds = 1; // gridMET rsds is flux density over 24 hours

         // Reset daily weather values
         _clear_hist_weather(SW_All.Weather.allHist[0]);

             // Using the new inputs folder, read in year = 1980
         _read_weather_hist(
             year,
             SW_All.Weather.allHist[0],
             SW_All.Weather.name_prefix,
             SW_All.Weather.n_input_forcings,
             SW_All.Weather.dailyInputIndices,
             SW_All.Weather.dailyInputFlags,
             &LogInfo
         );

         result = SW_All.Weather.allHist[yearIndex]->r_humidity_daily[0];

             // Get expected average from Input/data_weather_gridmet/weath.1980 day 1
             // hursmax_pct and hursmin_pct
         expectedResult = (74.17 + 31.42) / 2.;

             // Test if the average of relative humidity has been calculated
             // instead of being based on huss for the first day of 1980
         EXPECT_NEAR(result, expectedResult, tol3);

         // Expect that daily relative humidity is derived from hursmax_pct and hursmin_pct
         // (and is not interpolated from mean monthly values)
         EXPECT_NEAR(
             SW_All.Weather.allHist[yearIndex]->r_humidity_daily[midJanDay],
             (88.35 + 34.35) / 2.,
             tol6
         );

         EXPECT_NE(
             SW_All.Weather.allHist[yearIndex]->r_humidity_daily[midJanDay],
             SW_All.Sky.r_humidity[0]
         );

         result = SW_All.Weather.allHist[yearIndex]->actualVaporPressure[0];

             // Get expected result from Input/data_weather_gridmet/weath.1980 day 1
             // hursmax_pct, hursmin_pct, Tmax_C, and Tmin_C
         expectedResult = actualVaporPressure2(74.17, 31.42, -3.18, -12.32);

             // Based on the max/min of relative humidity, test if actual vapor pressure
             // was calculated reasonably
         EXPECT_NEAR(result, expectedResult, tol6);


         // We have observed radiation and missing cloud cover
         EXPECT_FALSE(missing(SW_All.Weather.allHist[yearIndex]->shortWaveRad[0]));
         EXPECT_TRUE(missing(SW_All.Weather.allHist[yearIndex]->cloudcov_daily[0]));


         // Make sure calculations and set input values are within reasonable range
         checkAllWeather(&SW_All.Weather, &LogInfo);
     }

     TEST_F(WeatherFixtureTest, WeatherInputDayMet) {

         /*
            This section covers the assurance that if a daily value is provided
            (actual vapor pressure in this case), aside from temperature and precipitation,
            it is correctly set. Along with reasonable calculation of a separate daily value
            that is dependent on said daily variable (relative humidity in this case).

            * This scenario only happens with humidity-related variables.
            * This section uses the test directory "*_daymet".
          */

         double result, expectedResult, tempSlope;
         int yearIndex = 0, year = 1980, midJanDay = 14;

         /* Test correct priority is being given to input values from DAYMET */
         SW_WTH_setup(&SW_All.Weather, PathInfo.InFiles,
                      PathInfo.weather_prefix, &LogInfo);

                 // Switch directory to daymet input folder
         strcpy(SW_All.Weather.name_prefix, "Input/data_weather_daymet/weath");

        // Turn off monthly flags
        SW_All.Weather.use_cloudCoverMonthly = swFALSE;
        SW_All.Weather.use_windSpeedMonthly = swFALSE;
        SW_All.Weather.use_humidityMonthly = swFALSE;

                 // Manually edit index/flag arrays in SW_WEATHER to make test as
                 // realistic as possible
                 // Note: Indices are based on the directory:
                 // Input/data_weather_daymet/weath.1980
         SW_All.Weather.dailyInputIndices[ACTUAL_VP] = 3;
         SW_All.Weather.dailyInputIndices[SHORT_WR] = 4;
         SW_All.Weather.dailyInputFlags[ACTUAL_VP] = swTRUE;
         SW_All.Weather.dailyInputFlags[SHORT_WR] = swTRUE;
         SW_All.Weather.n_input_forcings = 5;
         SW_All.Weather.desc_rsds = 2; // DayMet rsds is flux density over daylight period

         // Reset daily weather values
         _clear_hist_weather(SW_All.Weather.allHist[0]);

                 // Using the new inputs folder, read in year = 1980
         _read_weather_hist(
             year,
             SW_All.Weather.allHist[0],
             SW_All.Weather.name_prefix,
             SW_All.Weather.n_input_forcings,
             SW_All.Weather.dailyInputIndices,
             SW_All.Weather.dailyInputFlags,
             &LogInfo
         );

         result = SW_All.Weather.allHist[yearIndex]->actualVaporPressure[0];

                 // Get expected result from Input/data_weather_daymet/weath.1980 day 1
                 // vp_kPa
         expectedResult = .3;

                 // Test if actual vapor pressure value was set to the first days
                 // input value from Input/data_weather_daymet/weath.1980 day 1
         EXPECT_NEAR(result, expectedResult, tol6);

         result = SW_All.Weather.allHist[yearIndex]->r_humidity_daily[0];

                 // Get expected result from Input/data_weather_daymet/weath.1980 day 1
                 // Tmax_C = -.37, Tmin_C = -9.2, and vp_kPa = .3
         expectedResult = (-.37 - 9.2) / 2.;
         expectedResult = svp(expectedResult, &tempSlope);
         expectedResult = .3 / expectedResult;

                 // Based on actual vapor pressure, test if relative humidity
                 // was calculated reasonably
         EXPECT_NEAR(result, expectedResult, tol6);

         // Expect that daily relative humidity is derived from vp_kPa, Tmax_C, and Tmin_C
         // (and is not interpolated from mean monthly values)
         expectedResult = (-.81 - 9.7) / 2.;
         expectedResult = svp(expectedResult, &tempSlope);
         expectedResult = .29 / expectedResult;

         EXPECT_NEAR(
             SW_All.Weather.allHist[yearIndex]->r_humidity_daily[midJanDay],
             expectedResult,
             tol6
         );

         EXPECT_NE(
             SW_All.Weather.allHist[yearIndex]->r_humidity_daily[midJanDay],
             SW_All.Sky.r_humidity[0]
         );

         // We have observed radiation and missing cloud cover
         EXPECT_FALSE(missing(SW_All.Weather.allHist[yearIndex]->shortWaveRad[0]));
         EXPECT_TRUE(missing(SW_All.Weather.allHist[yearIndex]->cloudcov_daily[0]));


         // Make sure calculations and set input values are within reasonable range
         checkAllWeather(&SW_All.Weather, &LogInfo);
     }

     TEST_F(WeatherFixtureTest, WeatherInputMACA) {

         /*
            This section assures that a variable aside from humidity-related ones,
            wind speed in this case, is calculated reasonably based on its components.

             * This section uses the test directory "*_maca".
          */

         double result, expectedResult;
         int yearIndex = 0, year = 1980, midJanDay = 14;

         /* Test correct priority is being given to input values from MACA */

         SW_WTH_setup(&SW_All.Weather, PathInfo.InFiles,
                      PathInfo.weather_prefix, &LogInfo);

                 // Switch directory to daymet input folder
         strcpy(SW_All.Weather.name_prefix, "Input/data_weather_maca/weath");

        // Turn off monthly flags
        SW_All.Weather.use_cloudCoverMonthly = swFALSE;
        SW_All.Weather.use_windSpeedMonthly = swFALSE;
        SW_All.Weather.use_humidityMonthly = swFALSE;

                 // Manually edit index/flag arrays in SW_WEATHER to make test as
                 // realistic as possible
                 // Note: Indices are based on the directory:
                 // Input/data_weather_maca/weath.1980
         SW_All.Weather.dailyInputIndices[WIND_EAST] = 3;
         SW_All.Weather.dailyInputIndices[WIND_NORTH] = 4;
         SW_All.Weather.dailyInputIndices[REL_HUMID_MAX] = 5;
         SW_All.Weather.dailyInputIndices[REL_HUMID_MIN] = 6;
         SW_All.Weather.dailyInputIndices[SHORT_WR] = 7;
         SW_All.Weather.dailyInputFlags[WIND_EAST] = swTRUE;
         SW_All.Weather.dailyInputFlags[WIND_NORTH] = swTRUE;
         SW_All.Weather.dailyInputFlags[REL_HUMID_MAX] = swTRUE;
         SW_All.Weather.dailyInputFlags[REL_HUMID_MIN] = swTRUE;
         SW_All.Weather.dailyInputFlags[SHORT_WR] = swTRUE;
         SW_All.Weather.n_input_forcings = 8;
         SW_All.Weather.desc_rsds = 1; // MACA rsds is flux density over 24 hours

         // Reset daily weather values
         _clear_hist_weather(SW_All.Weather.allHist[0]);

                 // Using the new inputs folder, read in year = 1980
         _read_weather_hist(
             year,
             SW_All.Weather.allHist[0],
             SW_All.Weather.name_prefix,
             SW_All.Weather.n_input_forcings,
             SW_All.Weather.dailyInputIndices,
             SW_All.Weather.dailyInputFlags,
             &LogInfo
         );

         result = SW_All.Weather.allHist[yearIndex]->windspeed_daily[0];

                 // Get expected result from Input/data_weather_maca/weath.1980 day 1
                 // uas_mPERs = 3.31 and vas_mPERs = -.85
         expectedResult = sqrt(squared(3.31) + squared(-.85));

                 // Test if wind speed was calculated within reasonable range to
                 // the expected result
         EXPECT_NEAR(result, expectedResult, tol6);

         // Expect that daily wind speed is derived from uas_mPERs and vas_mPERs
         // (and is not interpolated from mean monthly values)
         expectedResult = sqrt(squared(2.82) + squared(-.4));

         EXPECT_NEAR(
             SW_All.Weather.allHist[yearIndex]->windspeed_daily[midJanDay],
             expectedResult,
             tol6
         );

         EXPECT_NE(
             SW_All.Weather.allHist[yearIndex]->windspeed_daily[midJanDay],
             SW_All.Sky.windspeed[0]
         );

         // Expect that daily relative humidity is derived from hursmax_pct and hursmin_pct
         // (and is not interpolated from mean monthly values)
         EXPECT_NEAR(
             SW_All.Weather.allHist[yearIndex]->r_humidity_daily[midJanDay],
             (80.55 + 32.28) / 2.,
             tol6
         );

         EXPECT_NE(
             SW_All.Weather.allHist[yearIndex]->r_humidity_daily[midJanDay],
             SW_All.Sky.r_humidity[0]
         );

         // We have observed radiation and missing cloud cover
         EXPECT_FALSE(missing(SW_All.Weather.allHist[yearIndex]->shortWaveRad[0]));
         EXPECT_TRUE(missing(SW_All.Weather.allHist[yearIndex]->cloudcov_daily[0]));


         // Make sure calculations and set input values are within reasonable range
         checkAllWeather(&SW_All.Weather, &LogInfo);
     }

     TEST_F(WeatherFixtureTest, WeatherDailyLOCFInputValues) {

         /*
            Since SOILWAT2 now has the ability to deal with more than
            maximum/minimum temperature and precipitation, part of the weather
            generator can now perform LOCF on these values (generator method = 1).

            We want to make sure when the weather generator method is equal to 1,
            LOCF is performed on these variables and not ignored
         */
        int numDaysLOCFTolerance = 366, yearIndex = 0, day;
        double cloudCovTestVal = .5, actVapPressTestVal = 4.23, windSpeedTestVal = 2.12;

        // Setup and read in weather
        SW_WTH_setup(&SW_All.Weather, PathInfo.InFiles,
                     PathInfo.weather_prefix, &LogInfo);

        // Turn off flags for monthly values along with daily flags
        // so all daily variables aside from max/min temperature and precipiation
        // are set to SW_MISSING
        SW_All.Weather.use_cloudCoverMonthly = swFALSE;
        SW_All.Weather.use_humidityMonthly = swFALSE;
        SW_All.Weather.use_windSpeedMonthly = swFALSE;

        SW_WTH_read(&SW_All.Weather, &SW_All.Sky, &SW_All.Model, &LogInfo);

        // Setup values/flags for `generateMissingWeather()` to deal with
        SW_All.Weather.generateWeatherMethod = 1;
        SW_All.Weather.allHist[yearIndex]->cloudcov_daily[0] = cloudCovTestVal;
        SW_All.Weather.allHist[yearIndex]->actualVaporPressure[0] = actVapPressTestVal;
        SW_All.Weather.allHist[yearIndex]->windspeed_daily[0] = windSpeedTestVal;

        generateMissingWeather(&SW_All.Markov, SW_All.Weather.allHist,
                               1980, 1,
                               SW_All.Weather.generateWeatherMethod,
                               numDaysLOCFTolerance, &LogInfo);

        // Test to see if the first year of cloud cover, actual vapor pressure and
        // wind speed has been filled with cloudCovTestVal, actVapPressTestVal,
        // and windSpeedTestVal, respectively
        for(day = 0; day < MAX_DAYS; day++){
            EXPECT_EQ(SW_All.Weather.allHist[yearIndex]->cloudcov_daily[day], cloudCovTestVal);
            EXPECT_EQ(SW_All.Weather.allHist[yearIndex]->actualVaporPressure[day], actVapPressTestVal);
            EXPECT_EQ(SW_All.Weather.allHist[yearIndex]->windspeed_daily[day], windSpeedTestVal);
        }
     }

    TEST_F(WeatherFixtureTest, WeatherDailyInputWrongColumnNumberDeathTest) {
         /*
            This section covers number of flags and the testing of reasonable results (`checkAllWeather()`).

            An incorrect number of "n_input_forcings" within SW_WEATHER relative to the number of input columns
            read in should result in a crash.

            If an input or calculated value is out of range (e.g., range = [-100, 100] C for min/max temperature),
            `checkAllWeather()` should result in a crash.
          */

        // Initialize any variables
        TimeInt year = 1980;

        /* Not the same number of flags as columns */
        // Run weather functions and expect an failure (error)

        SW_WTH_read(&SW_All.Weather, &SW_All.Sky, &SW_All.Model, &LogInfo);

        // Set SW_WEATHER's n_input_forcings to a number that is
        // not the columns being read in
        SW_All.Weather.n_input_forcings = 0;

        _read_weather_hist(
            year,
            SW_All.Weather.allHist[0],
            SW_All.Weather.name_prefix,
            SW_All.Weather.n_input_forcings,
            SW_All.Weather.dailyInputIndices,
            SW_All.Weather.dailyInputFlags,
            &LogInfo
        );

        // Detect failure by error message
        EXPECT_THAT(LogInfo.errorMsg, HasSubstr("Incomplete record 1"));
    }


    TEST_F(WeatherFixtureTest, WeatherDailyInputBadTemperatureDeathTest) {
         /* Check for value(s) that are not within reasonable range these
            tests will make use of `checkAllWeather()` */

         // Edit SW_WEATHER_HIST values from their original value

        SW_WTH_read(&SW_All.Weather, &SW_All.Sky, &SW_All.Model, &LogInfo);

        // Make temperature unreasonable (not within [-100, 100])
        SW_All.Weather.allHist[0]->temp_max[0] = -102.;

        checkAllWeather(&SW_All.Weather, &LogInfo);

        // Detect failure by error message
        EXPECT_THAT(LogInfo.errorMsg,
                    HasSubstr("Daily input value for minimum temperature is"\
                              " greater than daily input value for maximum"\
                              " temperature"));
    }

    TEST_F(WeatherFixtureTest, WeatherDailyInputBadPrecipitationDeathTest) {
         /* Check for value(s) that are not within reasonable range these
            tests will make use of `checkAllWeather()` */

         // Edit SW_WEATHER_HIST values from their original value

        SW_WTH_read(&SW_All.Weather, &SW_All.Sky, &SW_All.Model, &LogInfo);

        // Make precipitation unresonable (< 0)
        SW_All.Weather.allHist[0]->ppt[0] = -1.;

        checkAllWeather(&SW_All.Weather, &LogInfo);

        // Detect failure by error message
        EXPECT_THAT(LogInfo.errorMsg, HasSubstr("Invalid daily precipitation value"));
    }

    TEST_F(WeatherFixtureTest, WeatherDailyInputBadHumidityDeathTest) {
         /* Check for value(s) that are not within reasonable range these
            tests will make use of `checkAllWeather()` */

         // Edit SW_WEATHER_HIST values from their original value

        SW_WTH_read(&SW_All.Weather, &SW_All.Sky, &SW_All.Model, &LogInfo);

        // Make relative humidity unreasonable (< 0%)
        SW_All.Weather.allHist[0]->r_humidity_daily[0] = -.1252;

        checkAllWeather(&SW_All.Weather, &LogInfo);

        // Detect failure by error message
        EXPECT_THAT(LogInfo.errorMsg,
                    HasSubstr("relative humidity value did not fall in the range"));
    }
}
