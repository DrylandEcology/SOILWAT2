#include "gtest/gtest.h"
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

#include "../SW_Weather.h"
#include "sw_testhelpers.h"
#include "../SW_Markov.h"
#include "../SW_Model.h"

namespace {

    TEST(ReadAllWeatherTest, DefaultValues) {
        
        // Testing to fill allHist from `SW_Weather`
        readAllWeather(SW_Weather.allHist, 1980, SW_Weather.n_years);
        
        // Test first day of first year in `allHist` to make sure correct
        // temperature max/min/avg and precipitation values
        EXPECT_NEAR(SW_Weather.allHist[0]->temp_max[0], -0.520000, tol6);
        EXPECT_NEAR(SW_Weather.allHist[0]->temp_avg[0], -8.095000, tol6);
        EXPECT_NEAR(SW_Weather.allHist[0]->temp_min[0], -15.670000, tol6);
        EXPECT_NEAR(SW_Weather.allHist[0]->ppt[0], .220000, tol6);

    }

    TEST(ReadAllWeatherTest, NoMemoryLeakIfDecreasedNumberOfYears) {

        // Default number of years is 31
        EXPECT_EQ(SW_Weather.n_years, 31);

        // Decrease number of years
        SW_Model.startyr = 1981;
        SW_Model.endyr = 1982;

        // Real expectation is that there is no memory leak for `allHist`
        SW_WTH_read();

        EXPECT_EQ(SW_Weather.n_years, 2);


        Reset_SOILWAT2_after_UnitTest();
    }

    TEST(ReadAllWeatherTest, SomeMissingValuesDays) {

        SW_Weather.generateWeatherMethod = 2;

        // Change directory to get input files with some missing data
        strcpy(SW_Weather.name_prefix, "Input/data_weather_missing/weath");

        SW_MKV_setup();

        SW_WTH_read();

        // With the use of 1980's missing values, test a few days of the year
        // to make sure they are filled using the weather generator
        EXPECT_TRUE(!missing(SW_Weather.allHist[0]->temp_max[0]));
        EXPECT_TRUE(!missing(SW_Weather.allHist[0]->temp_max[1]));
        EXPECT_TRUE(!missing(SW_Weather.allHist[0]->temp_max[2]));
        EXPECT_TRUE(!missing(SW_Weather.allHist[0]->temp_max[3]));
        EXPECT_TRUE(!missing(SW_Weather.allHist[0]->temp_max[4]));
        EXPECT_TRUE(!missing(SW_Weather.allHist[0]->temp_max[365]));
        
        Reset_SOILWAT2_after_UnitTest();
        
    }

    TEST(ReadAllWeatherTest, SomeMissingValuesYears) {
        
        int year, day;
        SW_Weather.generateWeatherMethod = 2;

        // Change directory to get input files with some missing data
        strcpy(SW_Weather.name_prefix, "Input/data_weather_missing/weath");

        SW_MKV_setup();
        
        SW_Model.startyr = 1981;
        SW_Model.endyr = 1982;
        
        SW_WTH_read();
        
        // Check everyday's value and test if it's `MISSING`
        for(year = 0; year < 2; year++) {
            for(day = 0; day < 365; day++) {
                EXPECT_TRUE(!missing(SW_Weather.allHist[year]->temp_max[day]));
            }
        }
        
        Reset_SOILWAT2_after_UnitTest();
        
    }

    TEST(ReadAllWeatherTest, WeatherGeneratorOnly) {
        
        int year, day;

        SW_Weather.generateWeatherMethod = 2;
        SW_Weather.use_weathergenerator_only = swTRUE;
        
        SW_MKV_setup();
        
        // Change directory to get input files with some missing data
        strcpy(SW_Weather.name_prefix, "Input/data_weather_nonexisting/weath");
        
        SW_WTH_read();
        
        // Check everyday's value and test if it's `MISSING`
        for(year = 0; year < 31; year++) {
            for(day = 0; day < 365; day++) {
                EXPECT_TRUE(!missing(SW_Weather.allHist[year]->temp_max[day]));
            }
        }
        
        Reset_SOILWAT2_after_UnitTest();
        
    }

    TEST(ReadAllWeatherDeathTest, TooManyMissingForLOCF) {

        // Change to directory without input files
        strcpy(SW_Weather.name_prefix, "Input/data_weather_nonexisting/weath");

        // Set LOCF (temp) + 0 (PPT) method
        SW_Weather.generateWeatherMethod = 1;

        SW_Model.startyr = 1981;
        SW_Model.endyr = 1981;

        // Error: too many missing values and weather generator turned off
        EXPECT_DEATH_IF_SUPPORTED(
          SW_WTH_read(),
          ""
        );

        Reset_SOILWAT2_after_UnitTest();

    }

    TEST(ClimateVariableTest, ClimateFromDefaultWeather) {

        // This test relies on allHist from `SW_WEATHER` being already filled
        SW_CLIMATE_YEARLY climateOutput;
        SW_CLIMATE_CLIM climateAverages;
        
        int deallocate = 0;
        int allocate = 1;

        Bool isNorth = swTRUE;

        // Allocate memory
            // 31 = number of years used in test
        allocDeallocClimateStructs(allocate, 31, &climateOutput, &climateAverages);


        // ------ Check climate variables for default weather ------
        // 1980 is first year out of 31 years of default weather

        // --- Annual time-series of climate variables ------
        // Here, check values for 1980

        // Expect identical output to rSOILWAT2 (e.g., v5.3.1)
        // ```{r}
        //   rSOILWAT2::calc_SiteClimate(
        //     weatherList = rSOILWAT2::get_WeatherHistory(
        //       rSOILWAT2::sw_exampleData
        //     )[1],
        //     do_C4vars = TRUE,
        //     do_Cheatgrass_ClimVars = TRUE
        //   )
        // ```

        calcSiteClimate(SW_Weather.allHist, 31, 1980, &climateOutput, isNorth);

        EXPECT_NEAR(climateOutput.meanTempMon_C[Jan][0], -8.432581, tol6);
        EXPECT_NEAR(climateOutput.maxTempMon_C[Jan][0], -2.562581, tol6);
        EXPECT_NEAR(climateOutput.minTempMon_C[Jan][0], -14.302581, tol6);
        EXPECT_NEAR(climateOutput.PPTMon_cm[Jan][0], 15.1400001, tol6);
        EXPECT_NEAR(climateOutput.PPT_cm[0], 59.27, tol1);
        EXPECT_NEAR(climateOutput.meanTemp_C[0], 4.524863, tol1);

        // Climate variables used for C4 grass cover
        // (stdev of one value is undefined)
        EXPECT_NEAR(climateOutput.minTemp7thMon_C[0], 2.809999, tol6);
        EXPECT_NEAR(climateOutput.frostFree_days[0], 92, tol6);
        EXPECT_NEAR(climateOutput.ddAbove65F_degday[0], 13.546000, tol6);


        // Climate variables used for cheatgrass cover
        // (stdev of one value is undefined)
        EXPECT_NEAR(climateOutput.PPT7thMon_mm[0], 18.299999, tol6);
        EXPECT_NEAR(climateOutput.meanTempDriestQtr_C[0], 0.936387, tol6);
        EXPECT_NEAR(climateOutput.minTemp2ndMon_C[0], -12.822068, tol6);


        // --- Long-term variables (aggregated across years) ------
        // Expect identical output to rSOILWAT2 (e.g., v5.3.1)
        // ```{r}
        //   rSOILWAT2::calc_SiteClimate(
        //     weatherList = rSOILWAT2::get_WeatherHistory(
        //       rSOILWAT2::sw_exampleData
        //     ),
        //     do_C4vars = TRUE,
        //     do_Cheatgrass_ClimVars = TRUE
        //   )
        // ```

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
        allocDeallocClimateStructs(deallocate, 31, &climateOutput, &climateAverages);
    }



    TEST(ClimateVariableTest, ClimateFromOneYearWeather) {

        // This test relies on allHist from `SW_WEATHER` being already filled
        SW_CLIMATE_YEARLY climateOutput;
        SW_CLIMATE_CLIM climateAverages;
        
        int deallocate = 0;
        int allocate = 1;

        Bool isNorth = swTRUE;

        // Allocate memory
            // 1 = number of years used in test
        allocDeallocClimateStructs(allocate, 1, &climateOutput, &climateAverages);

        // ------ Check climate variables for one year of default weather ------

        // Expect identical output to rSOILWAT2 (e.g., v5.3.1)
        // ```{r}
        //   rSOILWAT2::calc_SiteClimate(
        //     weatherList = rSOILWAT2::get_WeatherHistory(
        //       rSOILWAT2::sw_exampleData
        //     )[1],
        //     do_C4vars = TRUE,
        //     do_Cheatgrass_ClimVars = TRUE
        //   )
        // ```

        calcSiteClimate(SW_Weather.allHist, 1, 1980, &climateOutput, isNorth);
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
        EXPECT_NEAR(climateAverages.minTemp7thMon_C, 2.809999, tol6);
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
        allocDeallocClimateStructs(deallocate, 1, &climateOutput, &climateAverages);

    }

    TEST(ClimateVariableTest, ClimateFromDefaultWeatherSouth) {

        // This test relies on allHist from `SW_WEATHER` being already filled
        SW_CLIMATE_YEARLY climateOutput;
        SW_CLIMATE_CLIM climateAverages;

        int deallocate = 0;
        int allocate = 1;

        Bool isNorth = swFALSE;

        // Allocate memory
            // 31 = number of years used in test
        allocDeallocClimateStructs(allocate, 31, &climateOutput, &climateAverages);


        // ------ Check climate variables for default weather ------
        // 1980 is first year out of 31 years of default weather

        // --- Annual time-series of climate variables ------
        // Here, check values for 1980

        // Expect identical output to rSOILWAT2 (e.g., v5.3.1)
        // ```{r}
        //   rSOILWAT2::calc_SiteClimate(
        //     weatherList = rSOILWAT2::get_WeatherHistory(
        //       rSOILWAT2::sw_exampleData
        //     )[1],
        //     do_C4vars = TRUE,
        //     do_Cheatgrass_ClimVars = TRUE
        //   )
        // ```

        calcSiteClimate(SW_Weather.allHist, 31, 1980, &climateOutput, isNorth);

        EXPECT_NEAR(climateOutput.meanTempMon_C[Jan][0], -8.432581, tol6);
        EXPECT_NEAR(climateOutput.maxTempMon_C[Jan][0], -2.562581, tol6);
        EXPECT_NEAR(climateOutput.minTempMon_C[Jan][0], -14.302581, tol6);
        EXPECT_NEAR(climateOutput.PPTMon_cm[Jan][0], 15.1400001, tol6);
        EXPECT_NEAR(climateOutput.PPT_cm[0], 59.27, tol6);
        EXPECT_NEAR(climateOutput.meanTemp_C[0], 4.524863, tol6);

        // Climate variables used for C4 grass cover
        // (stdev of one value is undefined)
        EXPECT_NEAR(climateOutput.minTemp7thMon_C[1], -16.98, tol6);
        EXPECT_NEAR(climateOutput.frostFree_days[1], 78, tol6); // 79
        EXPECT_NEAR(climateOutput.ddAbove65F_degday[1], 16.458001, tol6); // 16.965000


        // Climate variables used for cheatgrass cover
        // (stdev of one value is undefined)
        EXPECT_NEAR(climateOutput.PPT7thMon_mm[1], 22.199999, tol6); // 22.19999
        EXPECT_NEAR(climateOutput.meanTempDriestQtr_C[0], 0.936387, tol6); // 15.8733906
        EXPECT_NEAR(climateOutput.minTemp2ndMon_C[1], 5.1445161, tol6); // 5.3467742


        // --- Long-term variables (aggregated across years) ------
        // Expect identical output to rSOILWAT2 (e.g., v5.3.1)
        // ```{r}
        //   rSOILWAT2::calc_SiteClimate(
        //     weatherList = rSOILWAT2::get_WeatherHistory(
        //       rSOILWAT2::sw_exampleData
        //     ),
        //     do_C4vars = TRUE,
        //     do_Cheatgrass_ClimVars = TRUE
        //   )
        // ```

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
        EXPECT_NEAR(climateAverages.minTemp7thMon_C, -27.199333, tol6); // -27.146999
        EXPECT_NEAR(climateAverages.frostFree_days, 72.599999, tol6); // 72.6333333
        EXPECT_NEAR(climateAverages.ddAbove65F_degday, 21.357533, tol6); // 21.2880665

        EXPECT_NEAR(climateAverages.sdC4[0], 5.325365, tol6);
        EXPECT_NEAR(climateAverages.sdC4[1], 9.586628, tol6); // 9.4229482
        EXPECT_NEAR(climateAverages.sdC4[2], 19.550419, tol6); // 19.589081

        // Climate variables used for cheatgrass cover
        EXPECT_NEAR(climateAverages.PPT7thMon_mm, 65.916666, tol6);
        EXPECT_NEAR(climateAverages.meanTempDriestQtr_C, 11.401228, tol6);
        EXPECT_NEAR(climateAverages.minTemp2ndMon_C, 6.545577, tol6);

        EXPECT_NEAR(climateAverages.sdCheatgrass[0], 35.285408, tol6);
        EXPECT_NEAR(climateAverages.sdCheatgrass[1], 7.260851, tol6);
        EXPECT_NEAR(climateAverages.sdCheatgrass[2], 1.639639, tol6);


        // ------ Reset and deallocate
        allocDeallocClimateStructs(deallocate, 31, &climateOutput, &climateAverages);
    }


    TEST(ClimateVariableTest, ClimateFromConstantWeather) {

        SW_CLIMATE_YEARLY climateOutput;
        SW_CLIMATE_CLIM climateAverages;
        SW_WEATHER_HIST **allHist;

        int allocate = 1;
        int deallocate = 0;

        Bool isNorth = swTRUE;
        
        // Allocate memory
        allocDeallocClimateStructs(allocate, 2, &climateOutput, &climateAverages);

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
        calcSiteClimate(allHist, 2, 1980, &climateOutput, isNorth);

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
        allocDeallocClimateStructs(deallocate, 2, &climateOutput, &climateAverages);

        for (int year = 0; year < 2; year++) {
            free(allHist[year]);
        }
        free(allHist);

    }


    TEST(ClimateVariableTest, AverageTemperatureOfDriestQuarterTest) {

        double monthlyPPT[MAX_MONTHS] = {.5, .5, .1, .4, .9, 1.0, 1.2, 6.5, 7.5, 1.2, 4., .6};
        double monthlyTemp[MAX_MONTHS] = {-3.2, -.4, 1.2, 3.5, 7.5, 4.5, 6.5, 8.2, 2.0, 3., .1, -.3};
        double result[2]; // 2 = max number of years in test

        int month, year;

        double **PPTMon_cm;
        PPTMon_cm = new double*[MAX_MONTHS];

        double **meanTempMon_C = new double*[MAX_MONTHS];

        Bool isNorth = swTRUE;

        for(month = 0; month < MAX_MONTHS; month++) {
            PPTMon_cm[month] = new double[2];
            meanTempMon_C[month] = new double[2];
            for(year = 0; year < 2; year++) {

                PPTMon_cm[month][year] = monthlyPPT[month];
                meanTempMon_C[month][year] = monthlyTemp[month];
            }
        }


        // ------ Test for one year ------
        findDriestQtr(result, 1, meanTempMon_C, PPTMon_cm, isNorth);

        // Value 1.433333... is the average temperature of the driest quarter of the year
        // In this case, the driest quarter is February-April
        EXPECT_NEAR(result[0], 1.4333333333333333, tol9);


        // ------ Test for two years ------
        findDriestQtr(result, 2, meanTempMon_C, PPTMon_cm, isNorth);

        EXPECT_NEAR(result[0], 1.4333333333333333, tol9);
        EXPECT_NEAR(result[1], 1.4333333333333333, tol9);



        // ------ Test when there are multiple driest quarters ------
        for (month = 0; month < MAX_MONTHS; month++) {
            for(year = 0; year < 2; year++) {
                PPTMon_cm[month][year] = 1.;
            }
        }

        findDriestQtr(result, 1, meanTempMon_C, PPTMon_cm, isNorth);

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

    TEST(WeatherReadTest, Initialization) {

        SW_WTH_read();

        EXPECT_FLOAT_EQ(SW_Weather.allHist[0]->temp_max[0], -.52);

    }


}
