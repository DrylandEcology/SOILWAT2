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

#include "include/generic.h"
#include "include/myMemory.h"
#include "include/filefuncs.h"
#include "include/rands.h"
#include "include/Times.h"
#include "include/SW_Defines.h"
#include "include/SW_Times.h"
#include "include/SW_Files.h"
#include "include/SW_Carbon.h"
#include "include/SW_Site.h"
#include "include/SW_VegProd.h"
#include "include/SW_VegEstab.h"
#include "include/SW_Model.h"
#include "include/SW_SoilWater.h"
#include "include/SW_Weather.h"
#include "include/SW_Markov.h"
#include "include/SW_Sky.h"

#include "tests/gtests/sw_testhelpers.h"



extern void (*test_mvnorm)(RealD *, RealD *, RealD, RealD, RealD, RealD, RealD,
                           pcg32_random_t*, LOG_INFO*);
extern void (*test_temp_correct_wetdry)(RealD *, RealD *, RealD, RealD, RealD, RealD, RealD);


namespace {
  // Test the SW_MARKOV constructor 'SW_MKV_construct'
  TEST(WeatherGeneratorTest, WeatherGeneratorConstructor) {
    SW_MARKOV SW_Markov;

    LOG_INFO LogInfo;
    silent_tests(&LogInfo);

    int rng_seed = 8;

    SW_MKV_construct(rng_seed, &SW_Markov, &LogInfo); // allocates memory

    // Check that at least first array elements are initialized to zero
    EXPECT_DOUBLE_EQ(0., SW_Markov.wetprob[0]);
    EXPECT_DOUBLE_EQ(0., SW_Markov.dryprob[0]);
    EXPECT_DOUBLE_EQ(0., SW_Markov.avg_ppt[0]);
    EXPECT_DOUBLE_EQ(0., SW_Markov.std_ppt[0]);
    EXPECT_DOUBLE_EQ(0., SW_Markov.cfxw[0]);
    EXPECT_DOUBLE_EQ(0., SW_Markov.cfxd[0]);
    EXPECT_DOUBLE_EQ(0., SW_Markov.cfnw[0]);
    EXPECT_DOUBLE_EQ(0., SW_Markov.cfnd[0]);

    SW_MKV_deconstruct(&SW_Markov);
  }


  // Check seeding of RNG for weather generator
  TEST(WeatherGeneratorTest, WeatherGeneratorRNGSeeding) {
    SW_MARKOV SW_Markov;

    LOG_INFO LogInfo;
    silent_tests(&LogInfo);

    char *InFiles[SW_NFILES];
    char *InFiles_nc[SW_NFILESNC];
    for (short file = 0; file < SW_NFILES; file++) {
      InFiles[file] = NULL;
    }

    // Initialize `InFiles_nc` but it is not in use
    for(short file = 0; file < SW_NFILESNC; file++) {
      InFiles_nc[file] = NULL;
    }

    InFiles[eMarkovCov] = Str_Dup("Input/mkv_covar.in", &LogInfo);
    InFiles[eMarkovProb] = Str_Dup("Input/mkv_prob.in", &LogInfo);

    int
      rng_seed,
      // Turn on Markov weather generator
      generateWeatherMethod = 2;

    short k, n = 18, seed = 42, year = 1980;
    RealD
      tmax, *tmax0 = new double[n],
      tmin, *tmin0 = new double[n],
      ppt, *ppt0 = new double[n];



    //--- Generate some weather values with fixed seed ------

    // Initialize weather generator and read input files mkv_cover and mkv_prob
    rng_seed = seed;
    SW_MKV_setup(&SW_Markov, rng_seed, generateWeatherMethod, InFiles, &LogInfo);

    ppt = 0.; // `SW_MKV_today()` uses incoming value of `ppt`

    for (k = 0; k < n; k++) {
      SW_MKV_today(&SW_Markov, k, year, &tmax0[k], &tmin0[k], &ppt, &LogInfo);
      ppt0[k] = ppt;
    }

    // Reset weather generator
    SW_MKV_deconstruct(&SW_Markov);


    //--- Expect that generated weather is different with time-varying seed ----
    // Initialize weather generator and read input files mkv_cover and mkv_prob
    rng_seed = 0;
    SW_MKV_setup(&SW_Markov, rng_seed, generateWeatherMethod, InFiles, &LogInfo);

    ppt = 0.; // `SW_MKV_today()` uses incoming value of `ppt`

    for (k = 0; k < n; k++) {
      SW_MKV_today(&SW_Markov, k, year, &tmax, &tmin, &ppt, &LogInfo);

      EXPECT_NE(tmax, tmax0[k]);
      EXPECT_NE(tmin, tmin0[k]);
      if (GT(ppt, 0.)) {
        EXPECT_NE(ppt, ppt0[k]); // ppt is different on wet days
      }
    }

    // Reset weather generator
    SW_MKV_deconstruct(&SW_Markov);


    //--- Expect that generated weather is reproducible with same seed ------
    // Initialize weather generator and read input files mkv_cover and mkv_prob
    rng_seed = seed;
    SW_MKV_setup(&SW_Markov, rng_seed, generateWeatherMethod, InFiles, &LogInfo);

    ppt = 0.; // `SW_MKV_today()` uses incoming value of `ppt`

    for (k = 0; k < n; k++) {
      SW_MKV_today(&SW_Markov, k, year, &tmax, &tmin, &ppt, &LogInfo);

      EXPECT_DOUBLE_EQ(tmax, tmax0[k]);
      EXPECT_DOUBLE_EQ(tmin, tmin0[k]);
      EXPECT_DOUBLE_EQ(ppt, ppt0[k]);
    }


    // Reset weather generator
    SW_MKV_deconstruct(&SW_Markov);


    // Deallocate arrays
    delete[] tmax0;
    delete[] tmin0;
    delete[] ppt0;

    SW_F_deconstruct(InFiles, InFiles_nc);
  }


  // Test drawing multivariate normal variates for daily maximum/minimum temp
  TEST(WeatherGeneratorTest, WeatherGeneratormvnorm) {
    SW_MARKOV SW_Markov;

    LOG_INFO LogInfo;
    silent_tests(&LogInfo);

    int rng_seed = 9;
    short k, n = 3;
    RealD tmax = 0., tmin = 0., tval;

    SW_MKV_construct(rng_seed, &SW_Markov, &LogInfo); // initialize markov_rng

    for (k = 0; k < n; k++) {
      // Create temperature values: here with n = 3: -10, 0, +10
      tval = -10. + 10. * k;

      // Case: wtmax = wtmin, variance = 0, covar = 0 ==> input = output
      (test_mvnorm)(&tmax, &tmin, tval, tval, 0., 0., 0.,
                    &SW_Markov.markov_rng, &LogInfo);
      EXPECT_DOUBLE_EQ(tmax, tval);
      EXPECT_DOUBLE_EQ(tmin, tval);
      EXPECT_DOUBLE_EQ(tmin, tmax);

      // Case: wtmax = wtmin, variance = 0, covar > 0 ==> input = output
      (test_mvnorm)(&tmax, &tmin, tval, tval, 0., 0., 1.,
                    &SW_Markov.markov_rng, &LogInfo);
      EXPECT_DOUBLE_EQ(tmax, tval);
      EXPECT_DOUBLE_EQ(tmin, tval);
      EXPECT_DOUBLE_EQ(tmin, tmax);

      // Case: wtmax > wtmin, variance > 0, covar > 0 ==> tmin <= tmax
      (test_mvnorm)(&tmax, &tmin, tval + 1., tval, 1., 1., 1.,
                    &SW_Markov.markov_rng, &LogInfo);
      EXPECT_LE(tmin, tmax);

      // Case: wtmax < wtmin, variance > 0, covar > 0 ==> tmin == tmax
      (test_mvnorm)(&tmax, &tmin, tval - 1., tval, 1., 1., 1.,
                    &SW_Markov.markov_rng, &LogInfo);
      EXPECT_DOUBLE_EQ(tmin, tmax);
    }

    SW_MKV_deconstruct(&SW_Markov);
  }

  TEST(WeatherGeneratorDeathTest, WeatherGeneratormvnormDeathTest) {
    SW_MARKOV SW_Markov;

    LOG_INFO LogInfo;
    silent_tests(&LogInfo);

    int rng_seed = 11;
    RealD tmax = 0., tmin = 0.;

    SW_MKV_construct(rng_seed, &SW_Markov, &LogInfo); // initialize markov_rng

    // Case: (wT_covar ^ 2 / wTmax_var) > wTmin_var --> LOGFATAL
    EXPECT_DEATH_IF_SUPPORTED(
      (test_mvnorm)(&tmax, &tmin, 0., 0., 1., 1., 2.,
                                &SW_Markov.markov_rng, &LogInfo),
      "Bad covariance matrix"
    );

    SW_MKV_deconstruct(&SW_Markov);
  }


  // Test correcting daily temperatures for wet/dry days
  TEST(WeatherGeneratorTest, WeatherGeneratorWetDryTemperatureCorrection) {
    SW_MARKOV SW_Markov;

    LOG_INFO LogInfo;
    silent_tests(&LogInfo);

    int rng_seed = 13;
    RealD
      tmax = 0., tmin = 0., t0 = 0., t10 = 10.,
      wet = 1., dry = 0.,
      cf0 = 0., cf_pos = 5., cf_neg = -5.;

    SW_MKV_construct(rng_seed, &SW_Markov, &LogInfo); // initialize markov_rng

    // Case: tmax = tmin; wet; cf_*_wet = 0 ==> input = output
    tmax = t0;
    tmin = t0;
    (test_temp_correct_wetdry)(&tmax, &tmin, wet, cf0, cf_pos, cf0, cf_pos);
    EXPECT_DOUBLE_EQ(tmin, tmax);

    // Case: tmax > tmin; wet; cf_*_wet != 0 ==> input + cf_*_wet = output
    tmax = t10;
    tmin = t0;
    (test_temp_correct_wetdry)(&tmax, &tmin, wet, cf_neg, cf_pos, cf_neg, cf_pos);
    EXPECT_DOUBLE_EQ(tmax, t10 + cf_neg);
    EXPECT_DOUBLE_EQ(tmin, t0 + cf_neg);
    EXPECT_LE(tmin, tmax);

    // Case: tmax > tmin; dry; cf_*_dry != 0 ==> input + cf_*_dry = output
    tmax = t10;
    tmin = t0;
    (test_temp_correct_wetdry)(&tmax, &tmin, dry, cf_neg, cf_pos, cf_neg, cf_pos);
    EXPECT_DOUBLE_EQ(tmax, t10 + cf_pos);
    EXPECT_DOUBLE_EQ(tmin, t0 + cf_pos);
    EXPECT_LE(tmin, tmax);

    // Case: tmax < tmin; wet; cf_*_wet > 0 ==> tmin <= tmax
    tmax = t0;
    tmin = t10;
    (test_temp_correct_wetdry)(&tmax, &tmin, wet, cf_pos, cf_pos, cf_pos, cf_pos);
    EXPECT_DOUBLE_EQ(tmax, t0 + cf_pos);
    EXPECT_DOUBLE_EQ(tmin, fmin(tmax, t10 + cf_pos));
    EXPECT_LE(tmin, tmax);

    SW_MKV_deconstruct(&SW_Markov);
  }

} // namespace
