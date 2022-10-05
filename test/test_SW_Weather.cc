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
        readAllWeather(
          SW_Weather.allHist,
          1980,
          SW_Weather.n_years,
          SW_Weather.use_weathergenerator_only,
          SW_Weather.name_prefix
        );

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
        SW_WTH_finalize_all_weather();


        // Expect that missing input values (from 1980) are filled by the weather generator
        EXPECT_FALSE(missing(SW_Weather.allHist[0]->temp_max[0]));
        EXPECT_FALSE(missing(SW_Weather.allHist[0]->temp_max[1]));
        EXPECT_FALSE(missing(SW_Weather.allHist[0]->temp_min[0]));
        EXPECT_FALSE(missing(SW_Weather.allHist[0]->temp_min[2]));
        EXPECT_FALSE(missing(SW_Weather.allHist[0]->ppt[0]));
        EXPECT_FALSE(missing(SW_Weather.allHist[0]->ppt[3]));
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
        SW_WTH_finalize_all_weather();


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
        SW_WTH_finalize_all_weather();

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

        SW_WTH_read();

        // Error: too many missing values and weather generator turned off
        EXPECT_DEATH_IF_SUPPORTED(
          SW_WTH_finalize_all_weather(),
          ""
        );

        Reset_SOILWAT2_after_UnitTest();

    }

    TEST(WeatherReadTest, Initialization) {

        SW_WTH_read();

        EXPECT_FLOAT_EQ(SW_Weather.allHist[0]->temp_max[0], -.52);

    }


}
