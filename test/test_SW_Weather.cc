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

    TEST(ReadAllWeatherTest, SomeMissingValuesDays) {
        
        SW_Weather.use_weathergenerator = swTRUE;
        SW_MKV_setup();
        
        // Change directory to get input files with some missing data
        strcpy(SW_Weather.name_prefix, "Input/data_weather_missing/weath");
        
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
        
        deallocateAllWeather();
        
        SW_Weather.use_weathergenerator = swTRUE;
        
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
        
        SW_Weather.use_weathergenerator = swTRUE;
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

    TEST(ReadAllWeatherTest, CheckMissingForMissingYear) {

        int day;
        
        deallocateAllWeather();
        
        // Change directory to get input files with some missing data
        strcpy(SW_Weather.name_prefix, "Input/data_weather_nonexisting/weath");

        SW_Weather.use_weathergenerator = swFALSE;
        SW_Weather.use_weathergenerator_only = swFALSE;
        
        SW_Model.startyr = 1981;
        SW_Model.endyr = 1981;
        
        SW_WTH_read();

        // Check everyday's value and test if it's `MISSING`
        for(day = 0; day < 365; day++) {
            EXPECT_TRUE(missing(SW_Weather.allHist[0]->temp_max[day]));
        }

        Reset_SOILWAT2_after_UnitTest();

    }

    TEST(ClimateVariableTest, ClimateFromDefaultWeather) {

        // This test relies on allHist from `SW_WEATHER` being already filled
        SW_CLIMATE_YEARLY climateOutput;
        SW_CLIMATE_CLIM climateAverage;

        // Allocate memory
        climateOutput.JulyMinTemp = new double[31]; // 31 = Number of years in the simulation
        climateOutput.annualPPT_cm = new double[31];
        climateOutput.frostFreeDays_days = new double[31];
        climateOutput.ddAbove65F_degday = new double[31];
        climateOutput.JulyPPT_mm = new double[31];
        climateOutput.meanTempDriestQuarter_C = new double[31];
        climateOutput.minTempFebruary_C = new double[31];
        climateOutput.meanAnnualTemp_C = new double[31];
        climateOutput.monthlyPPT_cm = new double*[MAX_MONTHS];
        climateOutput.meanMonthlyTemp_C = new double*[MAX_MONTHS];
        climateOutput.minMonthlyTemp_C = new double*[MAX_MONTHS];
        climateOutput.maxMonthlyTemp_C = new double*[MAX_MONTHS];
        
        climateAverage.meanMonthlyTempAnn = new double[MAX_MONTHS];
        climateAverage.maxMonthlyTempAnn = new double[MAX_MONTHS];
        climateAverage.minMonthlyTempAnn = new double[MAX_MONTHS];
        climateAverage.meanMonthlyPPTAnn = new double[MAX_MONTHS];
        climateAverage.sdCheatgrass = new double[3];
        climateAverage.sdC4 = new double[3];
        
        double *freeArray[14] = {climateOutput.JulyMinTemp, climateOutput.annualPPT_cm,
            climateOutput.frostFreeDays_days,  climateOutput.ddAbove65F_degday,  climateOutput.JulyPPT_mm,
            climateOutput.meanTempDriestQuarter_C,  climateOutput.minTempFebruary_C,  climateOutput.meanAnnualTemp_C,
            climateAverage.meanMonthlyTempAnn,  climateAverage.maxMonthlyTempAnn,  climateAverage.minMonthlyTempAnn,
            climateAverage.meanMonthlyPPTAnn,  climateAverage.sdCheatgrass,  climateAverage.sdC4};

        for(int month = 0; month < MAX_MONTHS; month++) {
            climateOutput.monthlyPPT_cm[month] = new double[31];
            climateOutput.meanMonthlyTemp_C[month] = new double[31];
            climateOutput.minMonthlyTemp_C[month] = new double[31];
            climateOutput.maxMonthlyTemp_C[month] = new double[31];
        }


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

        calcSiteClimate(SW_Weather.allHist, 31, 1980, &climateOutput);

        EXPECT_NEAR(climateOutput.meanMonthlyTemp_C[Jan][0], -8.432581, tol6);
        EXPECT_NEAR(climateOutput.maxMonthlyTemp_C[Jan][0], -2.562581, tol6);
        EXPECT_NEAR(climateOutput.minMonthlyTemp_C[Jan][0], -14.302581, tol6);
        EXPECT_NEAR(climateOutput.monthlyPPT_cm[Jan][0], 15.1400001, tol6);
        EXPECT_NEAR(climateOutput.annualPPT_cm[0], 59.27, tol1);
        EXPECT_NEAR(climateOutput.meanAnnualTemp_C[0], 4.524863, tol1);

        // Climate variables used for C4 grass cover
        // (stdev of one value is undefined)
        EXPECT_NEAR(climateOutput.JulyMinTemp[0], 2.809999, tol6);
        EXPECT_NEAR(climateOutput.frostFreeDays_days[0], 92, tol6);
        EXPECT_NEAR(climateOutput.ddAbove65F_degday[0], 13.546000, tol6);


        // Climate variables used for cheatgrass cover
        // (stdev of one value is undefined)
        EXPECT_NEAR(climateOutput.JulyPPT_mm[0], 18.299999, tol6);
        EXPECT_NEAR(climateOutput.meanTempDriestQuarter_C[0], 0.936387, tol6);
        EXPECT_NEAR(climateOutput.minTempFebruary_C[0], -12.822068, tol6);


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

        averageClimateAcrossYears(&climateOutput, 31, &climateAverage);

        EXPECT_NEAR(climateAverage.meanMonthlyTempAnn[Jan], -9.325551, tol6);
        EXPECT_NEAR(climateAverage.maxMonthlyTempAnn[Jan], -2.714381, tol6);
        EXPECT_NEAR(climateAverage.minMonthlyTempAnn[Jan], -15.936722, tol6);
        EXPECT_NEAR(climateAverage.meanMonthlyPPTAnn[Jan], 6.867419, tol6);

        EXPECT_NEAR(climateAverage.MAP_cm, 62.817419, tol6);
        // Note: rSOILWAT2 v5.3.1 returns incorrect MAT_C = 4.153896
        //   which is long-term daily average (but not long-term annual average)
        EXPECT_NEAR(climateAverage.MAT_C, 4.154009, tol6);

        // Climate variables used for C4 grass cover
        EXPECT_NEAR(climateAverage.JulyMinTempAnn, 3.078387, tol6);
        EXPECT_NEAR(climateAverage.frostFreeAnn, 90.612903, tol6);
        EXPECT_NEAR(climateAverage.ddAbove65F_degdayAnn, 21.168032, tol6);

        EXPECT_NEAR(climateAverage.sdC4[0], 1.785535, tol6);
        EXPECT_NEAR(climateAverage.sdC4[1], 14.091788, tol6);
        EXPECT_NEAR(climateAverage.sdC4[2], 19.953560, tol6);

        // Climate variables used for cheatgrass cover
        EXPECT_NEAR(climateAverage.JulyPPTAnn_mm, 35.729032, tol6);
        EXPECT_NEAR(climateAverage.meanTempDriestQuarterAnn_C, 11.524859, tol6);
        EXPECT_NEAR(climateAverage.minTempFebruaryAnn_C, -13.904599, tol6);

        EXPECT_NEAR(climateAverage.sdCheatgrass[0], 21.598367, tol6);
        EXPECT_NEAR(climateAverage.sdCheatgrass[1], 7.171922, tol6);
        EXPECT_NEAR(climateAverage.sdCheatgrass[2], 2.618434, tol6);


        // ------ Reset and deallocate
        for(int month = 0; month < MAX_MONTHS; month++) {
            delete[] climateOutput.monthlyPPT_cm[month];
            delete[] climateOutput.meanMonthlyTemp_C[month];
            delete[] climateOutput.minMonthlyTemp_C[month];
            delete[] climateOutput.maxMonthlyTemp_C[month];
        }

        delete[] climateOutput.monthlyPPT_cm;
        delete[] climateOutput.meanMonthlyTemp_C;
        delete[] climateOutput.minMonthlyTemp_C;
        delete[] climateOutput.maxMonthlyTemp_C;

        // Free rest of allocated memory
        for(int index = 0; index < 14; index++) {
            free(freeArray[index]);
        }
    }



    TEST(ClimateVariableTest, ClimateFromOneYearWeather) {

        // This test relies on allHist from `SW_WEATHER` being already filled
        SW_CLIMATE_YEARLY climateOutput;
        SW_CLIMATE_CLIM climateAverage;

        // Allocate memory
        climateOutput.JulyMinTemp = new double[1]; // 1 = Number of years in the simulation
        climateOutput.annualPPT_cm = new double[1];
        climateOutput.frostFreeDays_days = new double[1];
        climateOutput.ddAbove65F_degday = new double[1];
        climateOutput.JulyPPT_mm = new double[1];
        climateOutput.meanTempDriestQuarter_C = new double[1];
        climateOutput.minTempFebruary_C = new double[1];
        climateOutput.meanAnnualTemp_C = new double[1];
        climateOutput.monthlyPPT_cm = new double*[MAX_MONTHS];
        climateOutput.meanMonthlyTemp_C = new double*[MAX_MONTHS];
        climateOutput.minMonthlyTemp_C = new double*[MAX_MONTHS];
        climateOutput.maxMonthlyTemp_C = new double*[MAX_MONTHS];

        climateAverage.meanMonthlyTempAnn = new double[MAX_MONTHS];
        climateAverage.maxMonthlyTempAnn = new double[MAX_MONTHS];
        climateAverage.minMonthlyTempAnn = new double[MAX_MONTHS];
        climateAverage.meanMonthlyPPTAnn = new double[MAX_MONTHS];
        climateAverage.sdCheatgrass = new double[3];
        climateAverage.sdC4 = new double[3];

        double *freeArray[14] = {climateOutput.JulyMinTemp, climateOutput.annualPPT_cm,
            climateOutput.frostFreeDays_days,  climateOutput.ddAbove65F_degday,  climateOutput.JulyPPT_mm,
            climateOutput.meanTempDriestQuarter_C,  climateOutput.minTempFebruary_C,  climateOutput.meanAnnualTemp_C,
            climateAverage.meanMonthlyTempAnn,  climateAverage.maxMonthlyTempAnn,  climateAverage.minMonthlyTempAnn,
            climateAverage.meanMonthlyPPTAnn,  climateAverage.sdCheatgrass,  climateAverage.sdC4};

        for(int month = 0; month < MAX_MONTHS; month++) {
            climateOutput.monthlyPPT_cm[month] = new double[1];
            climateOutput.meanMonthlyTemp_C[month] = new double[1];
            climateOutput.minMonthlyTemp_C[month] = new double[1];
            climateOutput.maxMonthlyTemp_C[month] = new double[1];
        }

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

        calcSiteClimate(SW_Weather.allHist, 1, 1980, &climateOutput);
        averageClimateAcrossYears(&climateOutput, 1, &climateAverage);

        // Expect that aggregated values across one year are identical
        // to values of that one year
        EXPECT_DOUBLE_EQ(
          climateAverage.meanMonthlyTempAnn[Jan],
          climateOutput.meanMonthlyTemp_C[Jan][0]
        );
        EXPECT_DOUBLE_EQ(
          climateAverage.maxMonthlyTempAnn[Jan],
          climateOutput.maxMonthlyTemp_C[Jan][0]
        );
        EXPECT_DOUBLE_EQ(
          climateAverage.minMonthlyTempAnn[Jan],
          climateOutput.minMonthlyTemp_C[Jan][0]
        );
        EXPECT_DOUBLE_EQ(
          climateAverage.meanMonthlyPPTAnn[Jan],
          climateOutput.monthlyPPT_cm[Jan][0]
        );
        EXPECT_DOUBLE_EQ(
          climateAverage.MAP_cm,
          climateOutput.annualPPT_cm[0]
        );
        EXPECT_DOUBLE_EQ(
          climateAverage.MAT_C,
          climateOutput.meanAnnualTemp_C[0]
        );

        // Climate variables used for C4 grass cover
        EXPECT_DOUBLE_EQ(
          climateAverage.JulyMinTempAnn,
          climateOutput.JulyMinTemp[0]
        );
        EXPECT_DOUBLE_EQ(
          climateAverage.frostFreeAnn,
          climateOutput.frostFreeDays_days[0]
        );
        EXPECT_DOUBLE_EQ(
          climateAverage.ddAbove65F_degdayAnn,
          climateOutput.ddAbove65F_degday[0]
        );

        // Climate variables used for cheatgrass cover
        EXPECT_DOUBLE_EQ(
          climateAverage.JulyPPTAnn_mm,
          climateOutput.JulyPPT_mm[0]
        );
        EXPECT_DOUBLE_EQ(
          climateAverage.meanTempDriestQuarterAnn_C,
          climateOutput.meanTempDriestQuarter_C[0]
        );
        EXPECT_DOUBLE_EQ(
          climateAverage.minTempFebruaryAnn_C,
          climateOutput.minTempFebruary_C[0]
        );


        EXPECT_NEAR(climateAverage.meanMonthlyTempAnn[Jan], -8.432581, tol6);
        EXPECT_NEAR(climateAverage.maxMonthlyTempAnn[Jan], -2.562581, tol6);
        EXPECT_NEAR(climateAverage.minMonthlyTempAnn[Jan], -14.302581, tol6);
        EXPECT_NEAR(climateAverage.meanMonthlyPPTAnn[Jan], 15.1400001, tol6);
        EXPECT_NEAR(climateAverage.MAP_cm, 59.27, tol1);
        EXPECT_NEAR(climateAverage.MAT_C, 4.524863, tol1);

        // Climate variables used for C4 grass cover
        // (stdev of one value is undefined)
        EXPECT_NEAR(climateAverage.JulyMinTempAnn, 2.809999, tol6);
        EXPECT_NEAR(climateAverage.frostFreeAnn, 92, tol6);
        EXPECT_NEAR(climateAverage.ddAbove65F_degdayAnn, 13.546000, tol6);
        EXPECT_TRUE(isnan(climateAverage.sdC4[0]));
        EXPECT_TRUE(isnan(climateAverage.sdC4[1]));
        EXPECT_TRUE(isnan(climateAverage.sdC4[2]));

        // Climate variables used for cheatgrass cover
        // (stdev of one value is undefined)
        EXPECT_NEAR(climateAverage.JulyPPTAnn_mm, 18.299999, tol6);
        EXPECT_NEAR(climateAverage.meanTempDriestQuarterAnn_C, 0.936387, tol6);
        EXPECT_NEAR(climateAverage.minTempFebruaryAnn_C, -12.822068, tol6);
        EXPECT_TRUE(isnan(climateAverage.sdCheatgrass[0]));
        EXPECT_TRUE(isnan(climateAverage.sdCheatgrass[1]));
        EXPECT_TRUE(isnan(climateAverage.sdCheatgrass[2]));


        // ------ Reset and deallocate
        for(int month = 0; month < MAX_MONTHS; month++) {
            delete[] climateOutput.monthlyPPT_cm[month];
            delete[] climateOutput.meanMonthlyTemp_C[month];
            delete[] climateOutput.minMonthlyTemp_C[month];
            delete[] climateOutput.maxMonthlyTemp_C[month];
        }

        delete[] climateOutput.monthlyPPT_cm;
        delete[] climateOutput.meanMonthlyTemp_C;
        delete[] climateOutput.minMonthlyTemp_C;
        delete[] climateOutput.maxMonthlyTemp_C;

        // Free rest of allocated memory
        for(int index = 0; index < 14; index++) {
            free(freeArray[index]);
        }

    }


    TEST(ClimateVariableTest, ClimateFromConstantWeather) {

        SW_CLIMATE_YEARLY climateOutput;
        SW_CLIMATE_CLIM climateAverages;
        SW_WEATHER_HIST **allHist;

        // Allocate memory
        climateOutput.JulyMinTemp = new double[2];
        climateOutput.annualPPT_cm = new double[2];
        climateOutput.frostFreeDays_days = new double[2];
        climateOutput.ddAbove65F_degday = new double[2];
        climateOutput.JulyPPT_mm = new double[2];
        climateOutput.meanTempDriestQuarter_C = new double[2];
        climateOutput.minTempFebruary_C = new double[2];
        climateOutput.meanAnnualTemp_C = new double[2];
        climateOutput.monthlyPPT_cm = new double*[MAX_MONTHS];
        climateOutput.meanMonthlyTemp_C = new double*[MAX_MONTHS];
        climateOutput.minMonthlyTemp_C = new double*[MAX_MONTHS];
        climateOutput.maxMonthlyTemp_C = new double*[MAX_MONTHS];

        climateAverage.meanMonthlyTempAnn = new double[MAX_MONTHS];
        climateAverage.maxMonthlyTempAnn = new double[MAX_MONTHS];
        climateAverage.minMonthlyTempAnn = new double[MAX_MONTHS];
        climateAverage.meanMonthlyPPTAnn = new double[MAX_MONTHS];
        climateAverage.sdCheatgrass = new double[3];
        climateAverage.sdC4 = new double[3];

        double *freeArray[14] = {climateOutput.JulyMinTemp, climateOutput.annualPPT_cm,
            climateOutput.frostFreeDays_days,  climateOutput.ddAbove65F_degday,  climateOutput.JulyPPT_mm,
            climateOutput.meanTempDriestQuarter_C,  climateOutput.minTempFebruary_C,  climateOutput.meanAnnualTemp_C,
            climateAverage.meanMonthlyTempAnn,  climateAverage.maxMonthlyTempAnn,  climateAverage.minMonthlyTempAnn,
            climateAverage.meanMonthlyPPTAnn,  climateAverage.sdCheatgrass,  climateAverage.sdC4};

        for(int month = 0; month < MAX_MONTHS; month++) {
            climateOutput.monthlyPPT_cm[month] = new double[2];
            climateOutput.meanMonthlyTemp_C[month] = new double[2];
            climateOutput.minMonthlyTemp_C[month] = new double[2];
            climateOutput.maxMonthlyTemp_C[month] = new double[2];
        }

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
        calcSiteClimate(allHist, 2, 1980, &climateOutput);

        EXPECT_DOUBLE_EQ(climateOutput.meanMonthlyTemp_C[Jan][0], 1.);
        EXPECT_DOUBLE_EQ(climateOutput.maxMonthlyTemp_C[Jan][0], 1.);
        EXPECT_DOUBLE_EQ(climateOutput.minMonthlyTemp_C[Jan][0], 1.);
        EXPECT_DOUBLE_EQ(climateOutput.monthlyPPT_cm[Jan][0], 31.);
        EXPECT_DOUBLE_EQ(climateOutput.monthlyPPT_cm[Feb][0], 29.);
        EXPECT_DOUBLE_EQ(climateOutput.monthlyPPT_cm[Feb][1], 28.);
        EXPECT_DOUBLE_EQ(climateOutput.annualPPT_cm[0], 366.);
        EXPECT_DOUBLE_EQ(climateOutput.annualPPT_cm[1], 365.);
        EXPECT_DOUBLE_EQ(climateOutput.meanAnnualTemp_C[0], 1.);

        // Climate variables used for C4 grass cover
        // (stdev of one value is undefined)
        EXPECT_DOUBLE_EQ(climateOutput.JulyMinTemp[0], 1.);
        EXPECT_DOUBLE_EQ(climateOutput.frostFreeDays_days[0], 366.);
        EXPECT_DOUBLE_EQ(climateOutput.frostFreeDays_days[1], 365.);
        EXPECT_DOUBLE_EQ(climateOutput.ddAbove65F_degday[0], 0.);


        // Climate variables used for cheatgrass cover
        // (stdev of one value is undefined)
        EXPECT_DOUBLE_EQ(climateOutput.JulyPPT_mm[0], 310.);
        EXPECT_DOUBLE_EQ(climateOutput.meanTempDriestQuarter_C[0], 1.);
        EXPECT_DOUBLE_EQ(climateOutput.minTempFebruary_C[0], 1.);


        // --- Long-term variables (aggregated across years) ------
        averageClimateAcrossYears(&climateOutput, 2, &climateAverage);

        EXPECT_DOUBLE_EQ(climateAverage.meanMonthlyTempAnn[Jan], 1.);
        EXPECT_DOUBLE_EQ(climateAverage.maxMonthlyTempAnn[Jan], 1.);
        EXPECT_DOUBLE_EQ(climateAverage.minMonthlyTempAnn[Jan], 1.);
        EXPECT_DOUBLE_EQ(climateAverage.meanMonthlyPPTAnn[Jan], 31.);
        EXPECT_DOUBLE_EQ(climateAverage.meanMonthlyPPTAnn[Feb], 28.5);
        EXPECT_DOUBLE_EQ(climateAverage.meanMonthlyPPTAnn[Dec], 31);
        EXPECT_DOUBLE_EQ(climateAverage.MAP_cm, 365.5);
        EXPECT_DOUBLE_EQ(climateAverage.MAT_C, 1.);

        // Climate variables used for C4 grass cover
        EXPECT_DOUBLE_EQ(climateAverage.JulyMinTempAnn, 1.);
        EXPECT_DOUBLE_EQ(climateAverage.frostFreeAnn, 365.5);
        EXPECT_DOUBLE_EQ(climateAverage.ddAbove65F_degdayAnn, 0.);
        EXPECT_DOUBLE_EQ(climateAverage.sdC4[0], 0.);
        EXPECT_NEAR(climateAverage.sdC4[1], .7071067, tol6); // sd(366, 365)
        EXPECT_DOUBLE_EQ(climateAverage.sdC4[2], 0.);

        // Climate variables used for cheatgrass cover
        EXPECT_DOUBLE_EQ(climateAverage.JulyPPTAnn_mm, 310.);
        EXPECT_DOUBLE_EQ(climateAverage.meanTempDriestQuarterAnn_C, 1.);
        EXPECT_DOUBLE_EQ(climateAverage.minTempFebruaryAnn_C, 1.);
        EXPECT_DOUBLE_EQ(climateAverage.sdCheatgrass[0], 0.);
        EXPECT_DOUBLE_EQ(climateAverage.sdCheatgrass[1], 0.);
        EXPECT_DOUBLE_EQ(climateAverage.sdCheatgrass[2], 0.);


        // ------ Reset and deallocate
        for(int month = 0; month < MAX_MONTHS; month++) {
            delete[] climateOutput.monthlyPPT_cm[month];
            delete[] climateOutput.meanMonthlyTemp_C[month];
            delete[] climateOutput.minMonthlyTemp_C[month];
            delete[] climateOutput.maxMonthlyTemp_C[month];
        }

        delete[] climateOutput.monthlyPPT_cm;
        delete[] climateOutput.meanMonthlyTemp_C;
        delete[] climateOutput.minMonthlyTemp_C;
        delete[] climateOutput.maxMonthlyTemp_C;

        // Free rest of allocated memory
        for(int index = 0; index < 14; index++) {
            free(freeArray[index]);
        }

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

        double **monthlyPPT_cm;
        monthlyPPT_cm = new double*[MAX_MONTHS];

        double **meanMonthlyTemp_C = new double*[MAX_MONTHS];

        for(month = 0; month < MAX_MONTHS; month++) {
            monthlyPPT_cm[month] = new double[2];
            meanMonthlyTemp_C[month] = new double[2];
            for(year = 0; year < 2; year++) {

                monthlyPPT_cm[month][year] = monthlyPPT[month];
                meanMonthlyTemp_C[month][year] = monthlyTemp[month];
            }
        }


        // ------ Test for one year ------
        findDriestQtr(result, 1, meanMonthlyTemp_C, monthlyPPT_cm);

        // Value 1.433333... is the average temperature of the driest quarter of the year
        // In this case, the driest quarter is February-April
        EXPECT_NEAR(result[0], 1.4333333333333333, tol9);


        // ------ Test for two years ------
        findDriestQtr(result, 2, meanMonthlyTemp_C, monthlyPPT_cm);

        EXPECT_NEAR(result[0], 1.4333333333333333, tol9);
        EXPECT_NEAR(result[1], 1.4333333333333333, tol9);



        // ------ Test when there are multiple driest quarters ------
        for (month = 0; month < MAX_MONTHS; month++) {
            for(year = 0; year < 2; year++) {
                monthlyPPT_cm[month][year] = 1.;
            }
        }

        findDriestQtr(result, 1, meanMonthlyTemp_C, monthlyPPT_cm);

        // Expect that the driest quarter that occurs first
        // among all driest quarters is used
        // Here, the first driest quarter is centered on Jan with Dec-Feb
        // with average temp of -1.3 C
        EXPECT_NEAR(result[0], -1.3, tol9);


        // ------ Clean up
        for(int month = 0; month < MAX_MONTHS; month++) {
            delete[] monthlyPPT_cm[month];
            delete[] meanMonthlyTemp_C[month];
        }

        delete[] monthlyPPT_cm;
        delete[] meanMonthlyTemp_C;

    }

    TEST(WeatherReadTest, Initialization) {

        SW_WTH_read();

        EXPECT_FLOAT_EQ(SW_Weather.allHist[0]->temp_max[0], -.52);

    }


}
