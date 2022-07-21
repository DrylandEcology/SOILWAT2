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
    TEST(AverageClimateAcrossYearsTest, FileAndValuesOfOne) {
        
        // This test relies on allHist from `SW_WEATHER` being already filled
        SW_CLIMATE_CALC climateOutput;
        SW_CLIMATE_AVERAGES climateAverage;
        
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
            for(int year = 0; year < 31; year++) {
                climateOutput.monthlyPPT_cm[month][year] = 0.;
                climateOutput.meanMonthlyTemp_C[month][year] = 0.;
                climateOutput.minMonthlyTemp_C[month][year] = 0.;
                climateOutput.maxMonthlyTemp_C[month][year] = 0.;
            }
        }
        // 1980 is start year of the simulation
        calcSiteClimate(SW_Weather.allHist, 31, 1980, &climateOutput);
        averageClimateAcrossYears(&climateOutput, 31, &climateAverage);
        
        EXPECT_NEAR(climateAverage.meanMonthlyTempAnn[0], -9.325551, tol6);
        EXPECT_NEAR(climateAverage.maxMonthlyTempAnn[0], -2.714381, tol6);
        EXPECT_NEAR(climateAverage.minMonthlyTempAnn[0], -15.936722, tol6);
        EXPECT_NEAR(climateAverage.meanMonthlyPPTAnn[0], 6.867419, tol6);
        
        EXPECT_NEAR(climateAverage.MAP_cm, 62.817419, tol6);
        EXPECT_NEAR(climateAverage.MAT_C, 4.154009, tol6);
        EXPECT_NEAR(climateAverage.JulyPPTAnn_mm, 35.729032, tol6);
        EXPECT_NEAR(climateAverage.meanTempDriestQuarterAnn_C, 11.524859, tol6);
        EXPECT_NEAR(climateAverage.minTempFebruaryAnn_C, -13.904599, tol6);
        EXPECT_NEAR(climateAverage.ddAbove65F_degdayAnn, 21.168032, tol6);
        EXPECT_NEAR(climateAverage.frostFreeAnn, 90.612903, tol6);
        EXPECT_NEAR(climateAverage.JulyMinTempAnn, 3.078387, tol6);
        
        // Standard deviation of C4 variables
        EXPECT_NEAR(climateAverage.sdC4[0], 1.785535, tol6);
        EXPECT_NEAR(climateAverage.sdC4[1], 14.091788, tol6);
        EXPECT_NEAR(climateAverage.sdC4[2], 19.953560, tol6);
        
        // Standard deviation of cheatgrass variables
        EXPECT_NEAR(climateAverage.sdCheatgrass[0], 21.598367, tol6);
        EXPECT_NEAR(climateAverage.sdCheatgrass[1], 7.171922, tol6);
        EXPECT_NEAR(climateAverage.sdCheatgrass[2], 2.618434, tol6);
        
        for(int year = 0; year < 31; year++) {
            for(int month = 0; month < MAX_MONTHS; month++) {
                climateOutput.monthlyPPT_cm[month][year] = 1.;
                climateOutput.meanMonthlyTemp_C[month][year] = 1.;
                climateOutput.minMonthlyTemp_C[month][year] = 1.;
                climateOutput.maxMonthlyTemp_C[month][year] = 1.;
            }
            climateOutput.annualPPT_cm[year] = 1.;
            climateOutput.meanAnnualTemp_C[year] = 1.;
        }
        
        // Reset values
        for(int year = 0; year < 31; year++) {
            for(int month = 0; month < MAX_MONTHS; month++) {
                climateOutput.monthlyPPT_cm[month][year] = 0.;
                climateOutput.meanMonthlyTemp_C[month][year] = 0.;
                climateOutput.minMonthlyTemp_C[month][year] = 0.;
                climateOutput.maxMonthlyTemp_C[month][year] = 0.;
            }
            climateOutput.annualPPT_cm[year] = 0.;
            climateOutput.meanAnnualTemp_C[year] = 0.;
        }
        // Tests for one year of simulation
        calcSiteClimate(SW_Weather.allHist, 1, 1980, &climateOutput);
        
        averageClimateAcrossYears(&climateOutput, 1, &climateAverage);
        
        EXPECT_NEAR(climateAverage.meanMonthlyTempAnn[0], -8.432581, tol6);
        EXPECT_NEAR(climateAverage.maxMonthlyTempAnn[0], -2.562581, tol6);
        EXPECT_NEAR(climateAverage.minMonthlyTempAnn[0], -14.302581, tol6);
        EXPECT_NEAR(climateAverage.meanMonthlyPPTAnn[0], 15.1400001, tol6);
        EXPECT_NEAR(climateAverage.MAP_cm, 59.27, tol1);
        EXPECT_NEAR(climateAverage.MAT_C, 4.524863, tol1);
        EXPECT_NEAR(climateAverage.JulyPPTAnn_mm, 18.299999, tol6);
        EXPECT_NEAR(climateAverage.meanTempDriestQuarterAnn_C, 0.936387, tol6);
        EXPECT_NEAR(climateAverage.minTempFebruaryAnn_C, -12.822068, tol6);
        EXPECT_NEAR(climateAverage.ddAbove65F_degdayAnn, 13.546000, tol6);
        EXPECT_NEAR(climateAverage.frostFreeAnn, 92, tol6);
        EXPECT_NEAR(climateAverage.JulyMinTempAnn, 2.809999, tol6);
        
        // Standard deviation of C4 variables of one year
        EXPECT_TRUE(isnan(climateAverage.sdC4[0]));
        EXPECT_TRUE(isnan(climateAverage.sdC4[1]));
        EXPECT_TRUE(isnan(climateAverage.sdC4[2]));

        // Standard deviation of cheatgrass variables of one year
        EXPECT_TRUE(isnan(climateAverage.sdCheatgrass[0]));
        EXPECT_TRUE(isnan(climateAverage.sdCheatgrass[1]));
        EXPECT_TRUE(isnan(climateAverage.sdCheatgrass[2]));
        
        for(int year = 0; year < 31; year++) {
            for(int day = 0; day < 366; day++) {
                SW_Weather.allHist[year]->temp_max[day] = 1.;
                SW_Weather.allHist[year]->temp_min[day] = 1.;
                SW_Weather.allHist[year]->temp_avg[day] = 1.;
                SW_Weather.allHist[year]->ppt[day] = 1.;
            }
        }

        // Start of tests with all `allHist` inputs of 1
        calcSiteClimate(SW_Weather.allHist, 2, 1980, &climateOutput);
        
        averageClimateAcrossYears(&climateOutput, 2, &climateAverage);
        
        EXPECT_DOUBLE_EQ(climateAverage.meanMonthlyTempAnn[0], 1.);
        EXPECT_DOUBLE_EQ(climateAverage.maxMonthlyTempAnn[0], 1.);
        EXPECT_DOUBLE_EQ(climateAverage.minMonthlyTempAnn[0], 1.);
        EXPECT_DOUBLE_EQ(climateAverage.meanMonthlyPPTAnn[0], 31.);
        EXPECT_DOUBLE_EQ(climateAverage.JulyPPTAnn_mm, 310.);
        EXPECT_DOUBLE_EQ(climateAverage.meanTempDriestQuarterAnn_C, 1.);
        EXPECT_DOUBLE_EQ(climateAverage.minTempFebruaryAnn_C, 1.);
        EXPECT_DOUBLE_EQ(climateAverage.ddAbove65F_degdayAnn, 0.);
        EXPECT_DOUBLE_EQ(climateAverage.frostFreeAnn, 365.5);
        EXPECT_DOUBLE_EQ(climateAverage.JulyMinTempAnn, 1.);
        // MAP_cm is expected to be 365.5 because we are running a leap year
        // and nonleap year where the number of days average to 365.5
        EXPECT_DOUBLE_EQ(climateAverage.MAP_cm, 365.5);
        EXPECT_DOUBLE_EQ(climateAverage.MAT_C, 1.);
        
        // Standard deviation of C4 variables of one year
        EXPECT_DOUBLE_EQ(climateAverage.sdC4[0], 0.);
        EXPECT_NEAR(climateAverage.sdC4[1], .7071067, tol6);
        EXPECT_DOUBLE_EQ(climateAverage.sdC4[2], 0.);

        // Standard deviation of cheatgrass variables of one year
        EXPECT_DOUBLE_EQ(climateAverage.sdCheatgrass[0], 0.);
        EXPECT_DOUBLE_EQ(climateAverage.sdCheatgrass[1], 0.);
        EXPECT_DOUBLE_EQ(climateAverage.sdCheatgrass[2], 0.);
        
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

    TEST(CalcSiteClimateTest, FileAndValuesOfOne) {

        // This test relies on allHist from `SW_WEATHER` being already filled

        SW_CLIMATE_CALC climateOutput;
        
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
        
        double *freeArray[9] = {climateOutput.JulyMinTemp, climateOutput.annualPPT_cm,
            climateOutput.frostFreeDays_days, climateOutput.ddAbove65F_degday, climateOutput.JulyPPT_mm,
            climateOutput.meanTempDriestQuarter_C, climateOutput.minTempFebruary_C, climateOutput.meanAnnualTemp_C};

        for(int month = 0; month < MAX_MONTHS; month++) {
            climateOutput.monthlyPPT_cm[month] = new double[31];
            climateOutput.meanMonthlyTemp_C[month] = new double[31];
            climateOutput.minMonthlyTemp_C[month] = new double[31];
            climateOutput.maxMonthlyTemp_C[month] = new double[31];
            for(int year = 0; year < 31; year++) {
                climateOutput.monthlyPPT_cm[month][year] = 0.;
                climateOutput.meanMonthlyTemp_C[month][year] = 0.;
                climateOutput.minMonthlyTemp_C[month][year] = 0.;
                climateOutput.maxMonthlyTemp_C[month][year] = 0.;
            }
        }

        SW_WTH_read();

        // 1980 is start year of the simulation
        calcSiteClimate(SW_Weather.allHist, 31, 1980, &climateOutput);

        // Average of average temperature of January in 1980
        EXPECT_NEAR(climateOutput.meanMonthlyTemp_C[0][0], -8.432581, tol6);

        // Average of max temperature in Januaray 1980
        EXPECT_NEAR(climateOutput.maxMonthlyTemp_C[0][0], -2.562581, tol6);

        // Average of min temperature in Januaray 1980
        EXPECT_NEAR(climateOutput.minMonthlyTemp_C[0][0], -14.302581, tol6);

        // Average January precipitation in 1980
        EXPECT_NEAR(climateOutput.monthlyPPT_cm[0][0], 15.14, tol6);

        // Average temperature of three driest month of first year
        EXPECT_NEAR(climateOutput.meanTempDriestQuarter_C[0], .936387, tol6);

        // Average precipiation of first year of simulation
        EXPECT_NEAR(climateOutput.annualPPT_cm[0], 59.27, tol6);

        // Average temperature of first year of simulation
        EXPECT_NEAR(climateOutput.meanAnnualTemp_C[0], 4.5248633, tol6);

        // First year's July minimum temperature
        EXPECT_NEAR(climateOutput.JulyMinTemp[0], 2.810000, tol6);

        // First year's number of most consecutive frost free days
        EXPECT_EQ(climateOutput.frostFreeDays_days[0], 92);

        // Sum of all temperature above 65F (18.333C) in first year
        EXPECT_NEAR(climateOutput.ddAbove65F_degday[0], 13.546000, tol6);

        // Total precipitation in July of first year
        EXPECT_NEAR(climateOutput.JulyPPT_mm[0], 18.300000, tol6);

        // Smallest temperature in all February first year
        EXPECT_NEAR(climateOutput.minTempFebruary_C[0], -12.822069, tol6);

        for(int year = 0; year < 2; year++) {
            for(int day = 0; day < 366; day++) {
                SW_Weather.allHist[year]->temp_max[day] = 1.;
                SW_Weather.allHist[year]->temp_min[day] = 1.;
                SW_Weather.allHist[year]->temp_avg[day] = 1.;
                SW_Weather.allHist[year]->ppt[day] = 1.;
            }
        }

        calcSiteClimate(SW_Weather.allHist, 2, 1980, &climateOutput);

        // Start of leap year tests (startYear = 1980)

        // Average of average temperature of January in 1980
        EXPECT_DOUBLE_EQ(climateOutput.meanMonthlyTemp_C[0][0], 1.);

        // Average of max temperature in Januaray 1980
        EXPECT_DOUBLE_EQ(climateOutput.maxMonthlyTemp_C[0][0], 1.);

        // Average of min temperature in Januaray 1980
        EXPECT_DOUBLE_EQ(climateOutput.minMonthlyTemp_C[0][0], 1.);

        // Average January precipitation in 1980
        EXPECT_DOUBLE_EQ(climateOutput.monthlyPPT_cm[0][0], 31.);

        // Average temperature of three driest month of first year
        EXPECT_DOUBLE_EQ(climateOutput.meanTempDriestQuarter_C[0], 1.);

        // Average precipiation of first year of simulation
        EXPECT_DOUBLE_EQ(climateOutput.annualPPT_cm[0], 366.);

        // Average temperature of first year of simulation
        EXPECT_DOUBLE_EQ(climateOutput.meanAnnualTemp_C[0], 1.);

        // First year's July minimum temperature
        EXPECT_DOUBLE_EQ(climateOutput.JulyMinTemp[0], 1.);

        // First year's number of most consecutive frost free days
        EXPECT_DOUBLE_EQ(climateOutput.frostFreeDays_days[0], 366);

        // Sum of all temperature above 65F (18.333C) in first year
        EXPECT_DOUBLE_EQ(climateOutput.ddAbove65F_degday[0], 0);

        // Total precipitation in July of first year
        EXPECT_DOUBLE_EQ(climateOutput.JulyPPT_mm[0], 310.);

        // Smallest temperature in all February first year
        EXPECT_NEAR(climateOutput.minTempFebruary_C[0], 1., tol6);

        // Start of nonleap year tests (startYear = 1981)

        calcSiteClimate(SW_Weather.allHist, 2, 1981, &climateOutput);

        // Average of average temperature of January in 1981
        EXPECT_DOUBLE_EQ(climateOutput.meanMonthlyTemp_C[0][0], 1.);

        // Average of max temperature in Januaray 1981
        EXPECT_DOUBLE_EQ(climateOutput.maxMonthlyTemp_C[0][0], 1.);

        // Average of min temperature in Januaray 1981
        EXPECT_DOUBLE_EQ(climateOutput.minMonthlyTemp_C[0][0], 1.);

        // Average January precipitation in 1980
        EXPECT_DOUBLE_EQ(climateOutput.monthlyPPT_cm[0][0], 31.);

        // Average temperature of three driest month of first year
        EXPECT_DOUBLE_EQ(climateOutput.meanTempDriestQuarter_C[0], 1.);

        // Average precipiation of first year of simulation
        EXPECT_DOUBLE_EQ(climateOutput.annualPPT_cm[0], 365.);

        // Average temperature of first year of simulation
        EXPECT_DOUBLE_EQ(climateOutput.meanAnnualTemp_C[0], 1.);

        // First year's July minimum temperature
        EXPECT_DOUBLE_EQ(climateOutput.JulyMinTemp[0], 1.);

        // First year's number of most consecutive frost free days
        EXPECT_DOUBLE_EQ(climateOutput.frostFreeDays_days[0], 365);

        // Sum of all temperature above 65F (18.333C) in first year
        EXPECT_DOUBLE_EQ(climateOutput.ddAbove65F_degday[0], 0);

        // Total precipitation in July of first year
        EXPECT_DOUBLE_EQ(climateOutput.JulyPPT_mm[0], 310.);

        // Smallest temperature in all February first year
        EXPECT_NEAR(climateOutput.minTempFebruary_C[0], 1., tol6);

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
        for(int index = 0; index < 9; index++) {
            free(freeArray[index]);
        }

    }

    TEST(AverageTemperatureOfDriestQuarterTest, OneAndTwoYear) {

        double monthlyPPT[MAX_MONTHS] = {.5, .5, .1, .4, .9, 1.0, 1.2, 6.5, 7.5, 1.2, 4., .6};
        double monthlyTemp[MAX_MONTHS] = {-3.2, -.4, 1.2, 3.5, 7.5, 4.5, 6.5, 8.2, 2.0, 3., .1, -.3};
        double result[2]; // 2 = max number of years in test

        double **monthlyPPT_cm;
        monthlyPPT_cm = new double*[MAX_MONTHS];

        double **meanMonthlyTemp_C = new double*[MAX_MONTHS];

        for(int month = 0; month < MAX_MONTHS; month++) {
            monthlyPPT_cm[month] = new double[2];
            meanMonthlyTemp_C[month] = new double[2];
            for(int year = 0; year < 2; year++) {

                monthlyPPT_cm[month][year] = monthlyPPT[month];
                meanMonthlyTemp_C[month][year] = monthlyTemp[month];
            }
        }
        // 1980 is start year of the simulation
        findDriestQtr(result, 1, meanMonthlyTemp_C, monthlyPPT_cm);

        // Value 1.433333... is the average temperature of the driest quarter of the year
        // In this case, the driest quarter is February-April
        EXPECT_DOUBLE_EQ(result[0], 1.4333333333333333);

        findDriestQtr(result, 2, meanMonthlyTemp_C, monthlyPPT_cm);

        EXPECT_DOUBLE_EQ(result[0], 1.4333333333333333);

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
