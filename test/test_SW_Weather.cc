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
    TEST(AverageAcrossYearsTest, FileValues) {
        
        // This test relies on allHist from `SW_WEATHER` being already filled
        
        double meanMonthlyTempAnn[MAX_MONTHS];
        double maxMonthlyTempAnn[MAX_MONTHS];
        double minMonthlyTempAnn[MAX_MONTHS];
        double meanMonthlyPPTAnn[MAX_MONTHS];
        double JulyMinTemp[31];
        double frostFreeDays_days[31];
        double ddAbove65F_degday[31];
        double JulyPPT_mm[31];
        double meanTempDriestQuarter_C[31];
        double minTempFebruary_C[31];
        double sdCheatgrass[3];
        double sdC4[3];
        double annualPPT_cm[31];
        double meanAnnualTemp_C[31];
        
        double MAP_cm;
        double MAT_C;
        
        double **meanMonthlyPPT_cm;
        meanMonthlyPPT_cm = new double*[MAX_MONTHS];
        
        double **meanMonthlyTemp_C = new double*[MAX_MONTHS];
        
        double **minMonthlyTemp_C = new double*[MAX_MONTHS];
        
        double **maxMonthlyTemp_C = new double*[MAX_MONTHS];
        
        for(int month = 0; month < MAX_MONTHS; month++) {
            meanMonthlyPPT_cm[month] = new double[31];
            meanMonthlyTemp_C[month] = new double[31];
            minMonthlyTemp_C[month] = new double[31];
            maxMonthlyTemp_C[month] = new double[31];
            for(int year = 0; year < 31; year++) {
                
                meanMonthlyPPT_cm[month][year] = 0.;
                meanMonthlyTemp_C[month][year] = 0.;
                minMonthlyTemp_C[month][year] = 0.;
                maxMonthlyTemp_C[month][year] = 0.;
                annualPPT_cm[year] = 0.;
                meanAnnualTemp_C[year] = 0.;
            }
        }
        // 1980 is start year of the simulation
        calcSiteClimate(SW_Weather.allHist, 31, 1980, meanMonthlyTemp_C, maxMonthlyTemp_C, minMonthlyTemp_C,
                        meanMonthlyPPT_cm, annualPPT_cm, meanAnnualTemp_C, JulyMinTemp, frostFreeDays_days, ddAbove65F_degday,
                        JulyPPT_mm, meanTempDriestQuarter_C, minTempFebruary_C);
        
        averageClimateAcrossYears(meanMonthlyTemp_C, maxMonthlyTemp_C, minMonthlyTemp_C,
            meanMonthlyPPT_cm, 31, JulyMinTemp, frostFreeDays_days, ddAbove65F_degday,
            JulyPPT_mm, meanTempDriestQuarter_C, minTempFebruary_C, annualPPT_cm,
            meanAnnualTemp_C, meanMonthlyTempAnn, maxMonthlyTempAnn, minMonthlyTempAnn,
            meanMonthlyPPTAnn, sdC4, sdCheatgrass, &MAT_C, &MAP_cm);
        
        EXPECT_NEAR(meanMonthlyTempAnn[0], -9.325551, tol6);
        EXPECT_NEAR(maxMonthlyTempAnn[0], -2.714381, tol6);
        EXPECT_NEAR(minMonthlyTempAnn[0], -15.936722, tol6);
        EXPECT_NEAR(meanMonthlyPPTAnn[0], 0.221530, tol6);
        
        EXPECT_NEAR(meanAnnualTemp_C[0], 4.524863, tol6);
        EXPECT_NEAR(annualPPT_cm[0], 59.2700004, tol6);
        EXPECT_NEAR(MAP_cm, 62.817419, tol6);
        EXPECT_NEAR(MAT_C, 4.154009, tol6);
        
        // Standard deviation of C4 variables
        EXPECT_NEAR(sdC4[0], 1.785535, tol6);
        EXPECT_NEAR(sdC4[1], 14.091788, tol6);
        EXPECT_NEAR(sdC4[2], 19.953560, tol6);
        
        // Standard deviation of cheatgrass variables
        EXPECT_NEAR(sdCheatgrass[0], 21.598367, tol6);
        EXPECT_NEAR(sdCheatgrass[1], 7.171922, tol6);
        EXPECT_NEAR(sdCheatgrass[2], 2.618434, tol6);
        // Reset values
        for(int year = 0; year < 31; year++) {
            for(int month = 0; month < MAX_MONTHS; month++) {
                meanMonthlyPPT_cm[month][year] = 0.;
                meanMonthlyTemp_C[month][year] = 0.;
                minMonthlyTemp_C[month][year] = 0.;
                maxMonthlyTemp_C[month][year] = 0.;
            }
            annualPPT_cm[year] = 0.;
            meanAnnualTemp_C[year] = 0.;
        }
        // Tests for one year of simulation
        calcSiteClimate(SW_Weather.allHist, 1, 1980, meanMonthlyTemp_C, maxMonthlyTemp_C, minMonthlyTemp_C,
                        meanMonthlyPPT_cm, annualPPT_cm, meanAnnualTemp_C, JulyMinTemp, frostFreeDays_days, ddAbove65F_degday,
                        JulyPPT_mm, meanTempDriestQuarter_C, minTempFebruary_C);
        
        averageClimateAcrossYears(meanMonthlyTemp_C, maxMonthlyTemp_C, minMonthlyTemp_C,
            meanMonthlyPPT_cm, 1, JulyMinTemp, frostFreeDays_days, ddAbove65F_degday,
            JulyPPT_mm, meanTempDriestQuarter_C, minTempFebruary_C, annualPPT_cm,
            meanAnnualTemp_C, meanMonthlyTempAnn, maxMonthlyTempAnn, minMonthlyTempAnn,
            meanMonthlyPPTAnn, sdC4, sdCheatgrass, &MAT_C, &MAP_cm);
        
        EXPECT_NEAR(meanMonthlyTempAnn[0], -8.432581, tol6);
        EXPECT_NEAR(maxMonthlyTempAnn[0], -2.562581, tol6);
        EXPECT_NEAR(minMonthlyTempAnn[0], -14.302581, tol6);
        EXPECT_NEAR(meanMonthlyPPTAnn[0], 0.488387, tol6);
        EXPECT_NEAR(MAP_cm, 59.27, tol1);
        EXPECT_NEAR(MAT_C, 4.524863, tol1);
        
        
        for(int month = 0; month < MAX_MONTHS; month++) {
            delete[] meanMonthlyPPT_cm[month];
            delete[] meanMonthlyTemp_C[month];
            delete[] minMonthlyTemp_C[month];
            delete[] maxMonthlyTemp_C[month];
        }
        
        delete[] meanMonthlyPPT_cm;
        delete[] meanMonthlyTemp_C;
        delete[] minMonthlyTemp_C;
        delete[] maxMonthlyTemp_C;
        
    }

    TEST(CalcSiteClimateTest, FileValues) {
        
        // This test relies on allHist from `SW_WEATHER` being already filled
        
        double JulyMinTemp[31]; // 31 = Number of years in the simulation
        double frostFreeDays_days[31];
        double ddAbove65F_degday[31];
        double JulyPPT_mm[31];
        double meanTempDriestQuarter_C[31];
        double minTempFebruary_C[31];
        double annualPPT_cm[31];
        double meanAnnualTemp_C[31];
        
        double **meanMonthlyPPT_cm;
        meanMonthlyPPT_cm = new double*[MAX_MONTHS];
        
        double **meanMonthlyTemp_C;
        meanMonthlyTemp_C = new double*[MAX_MONTHS];
        
        double **minMonthlyTemp_C;
        minMonthlyTemp_C = new double*[MAX_MONTHS];
        
        double **maxMonthlyTemp_C;
        maxMonthlyTemp_C = new double*[MAX_MONTHS];
        
        for(int month = 0; month < MAX_MONTHS; month++) {
            meanMonthlyPPT_cm[month] = new double[31];
            meanMonthlyTemp_C[month] = new double[31];
            minMonthlyTemp_C[month] = new double[31];
            maxMonthlyTemp_C[month] = new double[31];
            
            for(int year = 0; year < 31; year++) {
                
                meanMonthlyPPT_cm[month][year] = 0.;
                meanMonthlyTemp_C[month][year] = 0.;
                minMonthlyTemp_C[month][year] = 0.;
                maxMonthlyTemp_C[month][year] = 0.;
                annualPPT_cm[year] = 0.;
                meanAnnualTemp_C[year] = 0.;
                minTempFebruary_C[year] = 0.;
            }
        }
        
        SW_WTH_read();
        
        // 1980 is start year of the simulation
        calcSiteClimate(SW_Weather.allHist, 31, 1980, meanMonthlyTemp_C, maxMonthlyTemp_C, minMonthlyTemp_C,
                        meanMonthlyPPT_cm, annualPPT_cm, meanAnnualTemp_C, JulyMinTemp, frostFreeDays_days,
                        ddAbove65F_degday, JulyPPT_mm, meanTempDriestQuarter_C, minTempFebruary_C);
        
        // Average of average temperature of January in 1980
        EXPECT_NEAR(meanMonthlyTemp_C[0][0], -8.432581, tol6);
        
        // Average of max temperature in Januaray 1980
        EXPECT_NEAR(maxMonthlyTemp_C[0][0], -2.562581, tol6);
        
        // Average of min temperature in Januaray 1980
        EXPECT_NEAR(minMonthlyTemp_C[0][0], -14.302581, tol6);
        
        // Average January precipitation in 1980
        EXPECT_NEAR(meanMonthlyPPT_cm[0][0], 0.488387, tol6);
        
        // Average temperature of three driest month of first year
        EXPECT_NEAR(meanTempDriestQuarter_C[0], .936387, tol6);
        
        // Average precipiation of first year of simulation
        EXPECT_NEAR(annualPPT_cm[0], 59.27, tol6);
        
        // Average temperature of first year of simulation
        EXPECT_NEAR(meanAnnualTemp_C[0], 4.5248633, tol6);
        
        // First year's July minimum temperature
        EXPECT_NEAR(JulyMinTemp[0], 2.810000, tol6);
        
        // First year's number of most consecutive frost free days
        EXPECT_EQ(frostFreeDays_days[0], 92);
        
        // Sum of all temperature above 65F (18.333C) in first year
        EXPECT_NEAR(ddAbove65F_degday[0], 13.546000, tol6);
        
        // Total precipitation in July of first year
        EXPECT_NEAR(JulyPPT_mm[0], 18.300000, tol6);
        
        // Smallest temperature in all February first year
        EXPECT_NEAR(minTempFebruary_C[0], -12.822069, tol6);
        
        for(int year = 0; year < 2; year++) {
            for(int day = 0; day < 366; day++) {
                SW_Weather.allHist[year]->temp_max[day] = 1.;
                SW_Weather.allHist[year]->temp_min[day] = 1.;
                SW_Weather.allHist[year]->temp_avg[day] = 1.;
                SW_Weather.allHist[year]->ppt[day] = 1.;
            }
        }
        
        calcSiteClimate(SW_Weather.allHist, 2, 1980, meanMonthlyTemp_C, maxMonthlyTemp_C, minMonthlyTemp_C,
                        meanMonthlyPPT_cm, annualPPT_cm, meanAnnualTemp_C, JulyMinTemp, frostFreeDays_days, ddAbove65F_degday,
                        JulyPPT_mm, meanTempDriestQuarter_C, minTempFebruary_C);
        
        // Start of leap year tests (startYear = 1980)
        
        // Average of average temperature of January in 1980
        EXPECT_EQ(meanMonthlyTemp_C[0][0], 1.);
        
        // Average of max temperature in Januaray 1980
        EXPECT_EQ(maxMonthlyTemp_C[0][0], 1.);
        
        // Average of min temperature in Januaray 1980
        EXPECT_EQ(minMonthlyTemp_C[0][0], 1.);
        
        // Average January precipitation in 1980
        EXPECT_EQ(meanMonthlyPPT_cm[0][0], 1.);
        
        // Average temperature of three driest month of first year
        EXPECT_EQ(meanTempDriestQuarter_C[0], 1.);
        
        // Average precipiation of first year of simulation
        EXPECT_EQ(annualPPT_cm[0], 366.);
        
        // Average temperature of first year of simulation
        EXPECT_EQ(meanAnnualTemp_C[0], 1.);
        
        // First year's July minimum temperature
        EXPECT_EQ(JulyMinTemp[0], 1.);
        
        // First year's number of most consecutive frost free days
        EXPECT_EQ(frostFreeDays_days[0], 0);
        
        // Sum of all temperature above 65F (18.333C) in first year
        EXPECT_EQ(ddAbove65F_degday[0], 0);
        
        // Total precipitation in July of first year
        EXPECT_EQ(JulyPPT_mm[0], 310.);
        
        // Smallest temperature in all February first year
        EXPECT_NEAR(minTempFebruary_C[0], 1., tol6);
        
        // Start of nonleap year tests (startYear = 1981)
        
        calcSiteClimate(SW_Weather.allHist, 2, 1981, meanMonthlyTemp_C, maxMonthlyTemp_C, minMonthlyTemp_C,
                        meanMonthlyPPT_cm, annualPPT_cm, meanAnnualTemp_C, JulyMinTemp, frostFreeDays_days, ddAbove65F_degday,
                        JulyPPT_mm, meanTempDriestQuarter_C, minTempFebruary_C);
        
        // Average of average temperature of January in 1981
        EXPECT_EQ(meanMonthlyTemp_C[0][0], 1.);
        
        // Average of max temperature in Januaray 1981
        EXPECT_EQ(maxMonthlyTemp_C[0][0], 1.);
        
        // Average of min temperature in Januaray 1981
        EXPECT_EQ(minMonthlyTemp_C[0][0], 1.);
        
        // Average January precipitation in 1980
        EXPECT_EQ(meanMonthlyPPT_cm[0][0], 1.);
        
        // Average temperature of three driest month of first year
        EXPECT_EQ(meanTempDriestQuarter_C[0], 1.);
        
        // Average precipiation of first year of simulation
        EXPECT_EQ(annualPPT_cm[0], 365.);
        
        // Average temperature of first year of simulation
        EXPECT_EQ(meanAnnualTemp_C[0], 1.);
        
        // First year's July minimum temperature
        EXPECT_EQ(JulyMinTemp[0], 1.);
        
        // First year's number of most consecutive frost free days
        EXPECT_EQ(frostFreeDays_days[0], 0);
        
        // Sum of all temperature above 65F (18.333C) in first year
        EXPECT_EQ(ddAbove65F_degday[0], 0);
        
        // Total precipitation in July of first year
        EXPECT_EQ(JulyPPT_mm[0], 310.);
        
        // Smallest temperature in all February first year
        EXPECT_NEAR(minTempFebruary_C[0], 1., tol6);
        
        for(int month = 0; month < MAX_MONTHS; month++) {
            delete[] meanMonthlyPPT_cm[month];
            delete[] meanMonthlyTemp_C[month];
            delete[] minMonthlyTemp_C[month];
            delete[] maxMonthlyTemp_C[month];
        }
        
        delete[] meanMonthlyPPT_cm;
        delete[] meanMonthlyTemp_C;
        delete[] minMonthlyTemp_C;
        delete[] maxMonthlyTemp_C;
        
    }

    TEST(AverageTemperatureOfDriestQuarterTest, OneAndTwoYear) {
        
        double monthlyPPT[MAX_MONTHS] = {.5, .5, .1, .4, .9, 1.0, 1.2, 6.5, 7.5, 1.2, 4., .6};
        double monthlyTemp[MAX_MONTHS] = {-3.2, -.4, 1.2, 3.5, 7.5, 4.5, 6.5, 8.2, 2.0, 3., .1, -.3};
        double result[2]; // 2 = max number of years in test
        
        double **meanMonthlyPPT_cm;
        meanMonthlyPPT_cm = new double*[MAX_MONTHS];
        
        double **meanMonthlyTemp_C = new double*[MAX_MONTHS];
        
        for(int month = 0; month < MAX_MONTHS; month++) {
            meanMonthlyPPT_cm[month] = new double[2];
            meanMonthlyTemp_C[month] = new double[2];
            for(int year = 0; year < 2; year++) {
                
                meanMonthlyPPT_cm[month][year] = monthlyPPT[month];
                meanMonthlyTemp_C[month][year] = monthlyTemp[month];
            }
        }
        // 1980 is start year of the simulation
        findDriestQtr(result, 1, meanMonthlyTemp_C, meanMonthlyPPT_cm);
        
        // Value 1.433333... is the average temperature of the driest quarter of the year
        // In this case, the driest quarter is February-April
        EXPECT_DOUBLE_EQ(result[0], 1.4333333333333333);
        
        findDriestQtr(result, 2, meanMonthlyTemp_C, meanMonthlyPPT_cm);
        
        EXPECT_DOUBLE_EQ(result[0], 1.4333333333333333);
        
        for(int month = 0; month < MAX_MONTHS; month++) {
            delete[] meanMonthlyPPT_cm[month];
            delete[] meanMonthlyTemp_C[month];
        }
        
        delete[] meanMonthlyPPT_cm;
        delete[] meanMonthlyTemp_C;
        
    }

    TEST(WeatherReadTest, Initialization) {
        
        SW_WTH_read();
        
        EXPECT_FLOAT_EQ(SW_Weather.allHist[0]->temp_max[0], -.52);
        
    }


}
