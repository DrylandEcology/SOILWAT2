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
        
        double meanMonthlyAvgAnn[MAX_MONTHS];
        double meanMonthlyMaxAnn[MAX_MONTHS];
        double meanMonthlyMinAnn[MAX_MONTHS];
        double meanMonthlyPPTAnn[MAX_MONTHS];
        double JulyMinTemp[31];
        int frostFreeDays[31];
        double degreeAbove65[31];
        double PPTJuly[31];
        double meanTempDryQuarter[31];
        double minTempFebruary[31];
        double sdCheatgrass[3];
        double sdC4[3];
        double MMP_cm[31];
        double MMT_C[31];
        
        double MAP_cm;
        double MAT_C;
        
        double AnnTemp_C[31];
        double AnnPPT_cm[31];
        
        double **arrayPPTYear;
        arrayPPTYear = new double*[MAX_MONTHS];
        
        double **arrayTempAvgYear = new double*[MAX_MONTHS];
        
        double **arrayTempMinYear = new double*[MAX_MONTHS];
        
        double **arrayTempMaxYear = new double*[MAX_MONTHS];
        
        for(int month = 0; month < MAX_MONTHS; month++) {
            arrayPPTYear[month] = new double[31];
            arrayTempAvgYear[month] = new double[31];
            arrayTempMinYear[month] = new double[31];
            arrayTempMaxYear[month] = new double[31];
            for(int year = 0; year < 31; year++) {
                
                arrayPPTYear[month][year] = 0.;
                arrayTempAvgYear[month][year] = 0.;
                arrayTempMinYear[month][year] = 0.;
                arrayTempMaxYear[month][year] = 0.;
            }
        }
        // 1980 is start year of the simulation
        calcSiteClimate(SW_Weather.allHist, arrayTempAvgYear, arrayTempMaxYear, arrayTempMinYear,
                        arrayPPTYear, MMP_cm, MMT_C, JulyMinTemp, frostFreeDays, degreeAbove65, sdC4,
                        PPTJuly, meanTempDryQuarter, minTempFebruary, sdCheatgrass, 31, 1980);
        
        averageAcrossYears(arrayTempAvgYear, arrayTempMaxYear, arrayTempMinYear, arrayPPTYear,
                        meanMonthlyAvgAnn, meanMonthlyMaxAnn, meanMonthlyMinAnn, meanMonthlyPPTAnn,
                        &MAP_cm, &MAT_C, AnnTemp_C, AnnPPT_cm, 31);
        
        EXPECT_NEAR(meanMonthlyAvgAnn[0], -9.325551, tol6);
        EXPECT_NEAR(meanMonthlyMaxAnn[0], -2.714381, tol6);
        EXPECT_NEAR(meanMonthlyMinAnn[0], -15.936722, tol6);
        EXPECT_NEAR(meanMonthlyPPTAnn[0], 0.221530, tol6);
        
        for(int month = 0; month < MAX_MONTHS; month++) {
            delete[] arrayPPTYear[month];
            delete[] arrayTempAvgYear[month];
            delete[] arrayTempMinYear[month];
            delete[] arrayTempMaxYear[month];
        }
        
        delete[] arrayPPTYear;
        delete[] arrayTempAvgYear;
        delete[] arrayTempMinYear;
        delete[] arrayTempMaxYear;
        
    }

    TEST(calcSiteClimateTest, FileValues) {
        
        // This test relies on allHist from `SW_WEATHER` being already filled
        
        double JulyMinTemp[31];
        int frostFreeDays[31];
        double degreeAbove65[31];
        double PPTJuly[31];
        double meanTempDryQuarter[31];
        double minTempFebruary[31];
        double sdCheatgrass[3];
        double sdC4[3];
        double MMP_cm[31];
        double MMT_C[31];
        
        double **arrayPPTYear;
        arrayPPTYear = new double*[MAX_MONTHS];
        
        double **arrayTempAvgYear;
        arrayTempAvgYear = new double*[MAX_MONTHS];
        
        double **arrayTempMinYear;
        arrayTempMinYear = new double*[MAX_MONTHS];
        
        double **arrayTempMaxYear;
        arrayTempMaxYear = new double*[MAX_MONTHS];
        
        for(int month = 0; month < MAX_MONTHS; month++) {
            arrayPPTYear[month] = new double[31];
            arrayTempAvgYear[month] = new double[31];
            arrayTempMinYear[month] = new double[31];
            arrayTempMaxYear[month] = new double[31];
            
            for(int year = 0; year < 31; year++) {
                
                arrayPPTYear[month][year] = 0.;
                arrayTempAvgYear[month][year] = 0.;
                arrayTempMinYear[month][year] = 0.;
                arrayTempMaxYear[month][year] = 0.;
            }
        }
        // 1980 is start year of the simulation
        calcSiteClimate(SW_Weather.allHist, arrayTempAvgYear, arrayTempMaxYear, arrayTempMinYear,
                        arrayPPTYear, MMP_cm, MMT_C, JulyMinTemp, frostFreeDays, degreeAbove65, sdC4,
                        PPTJuly, meanTempDryQuarter, minTempFebruary, sdCheatgrass, 31, 1980);
        
        EXPECT_NEAR(arrayTempAvgYear[0][0], -8.432581, tol6);
        
        for(int month = 0; month < MAX_MONTHS; month++) {
            delete[] arrayPPTYear[month];
            delete[] arrayTempAvgYear[month];
            delete[] arrayTempMinYear[month];
            delete[] arrayTempMaxYear[month];
        }
        
        delete[] arrayPPTYear;
        delete[] arrayTempAvgYear;
        delete[] arrayTempMinYear;
        delete[] arrayTempMaxYear;
        
    }

    TEST(DriestQuarterTest, OneYear) {
        
        double monthlyPPT[MAX_MONTHS] = {.5, .5, .1, .4, .9, 1.0, 1.2, 6.5, 7.5, 1.2, 4., .6};
        double monthlyTemp[MAX_MONTHS] = {-3.2, -.4, 1.2, 3.5, 7.5, 4.5, 6.5, 8.2, 2.0, 3., .1, -.3};
        double result[31]; // 31 = number of years in simulation
        
        double **arrayPPTYear;
        arrayPPTYear = new double*[MAX_MONTHS];
        
        double **arrayTempAvgYear = new double*[MAX_MONTHS];
        
        for(int month = 0; month < MAX_MONTHS; month++) {
            arrayPPTYear[month] = new double[31];
            arrayTempAvgYear[month] = new double[31];
            for(int year = 0; year < 31; year++) {
                
                arrayPPTYear[month][year] = monthlyPPT[month];
                arrayTempAvgYear[month][year] = monthlyTemp[month];
            }
        }
        // 1980 is start year of the simulation
        findDriestQtr(arrayTempAvgYear, arrayPPTYear, result, 31, 1980);
        
        EXPECT_DOUBLE_EQ(result[0], 1.4333333333333333);
        
        for(int month = 0; month < MAX_MONTHS; month++) {
            delete[] arrayPPTYear[month];
            delete[] arrayTempAvgYear[month];
        }
        
        delete[] arrayPPTYear;
        delete[] arrayTempAvgYear;
        
    }

    TEST(WeatherReadTest, Initialization) {
        
        SW_WTH_read();
        
        EXPECT_FLOAT_EQ(SW_Weather.allHist[0]->temp_max[0], -.52);
        
    }


}
