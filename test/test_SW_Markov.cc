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

#include "../generic.h"
#include "../myMemory.h"
#include "../filefuncs.h"
#include "../rands.h"
#include "../Times.h"
#include "../SW_Defines.h"
#include "../SW_Times.h"
#include "../SW_Files.h"
#include "../SW_Carbon.h"
#include "../SW_Site.h"
#include "../SW_VegProd.h"
#include "../SW_VegEstab.h"
#include "../SW_Model.h"
#include "../SW_SoilWater.h"
#include "../SW_Weather.h"
#include "../SW_Markov.h"
#include "../SW_Sky.h"

#include "sw_testhelpers.h"



extern void (*test_mvnorm)(RealD *, RealD *, RealD, RealD, RealD, RealD, RealD);
extern void (*test_temp_correct_wetdry)(RealD *, RealD *, RealD, RealD, RealD, RealD, RealD);


namespace {
  SW_MARKOV *m = &SW_Markov;

  // Test the SW_MARKOV constructor 'SW_MKV_construct'
  TEST(WGTest, Constructor) {
    SW_MKV_construct();

    // Check that at least first array elements are initialized to zero
    EXPECT_DOUBLE_EQ(0., m->wetprob[0]);
    EXPECT_DOUBLE_EQ(0., m->dryprob[0]);
    EXPECT_DOUBLE_EQ(0., m->avg_ppt[0]);
    EXPECT_DOUBLE_EQ(0., m->std_ppt[0]);
    EXPECT_DOUBLE_EQ(0., m->cfxw[0]);
    EXPECT_DOUBLE_EQ(0., m->cfxd[0]);
    EXPECT_DOUBLE_EQ(0., m->cfnw[0]);
    EXPECT_DOUBLE_EQ(0., m->cfnd[0]);

    // Reset to previous global state
    // Reset_SOILWAT2_after_UnitTest();
    SW_MKV_deconstruct();
  }


  // Check seeding of RNG for weather generator
  TEST(WGTest, Seeding) {
    short k, n = 18, seed = 42;
    RealD
      tmax, *tmax0 = new double[n],
      tmin, *tmin0 = new double[n],
      ppt, *ppt0 = new double[n];

    // Turn on Markov weather generator
    SW_Weather.generateWeatherMethod = 2;


    //--- Generate some weather values with fixed seed ------

    // Initialize weather generator
    SW_Weather.rng_seed = seed;
    SW_MKV_setup();
    ppt = 0.; // `SW_MKV_today()` uses incoming value of `ppt`

    for (k = 0; k < n; k++) {
      SW_MKV_today(k, &tmax0[k], &tmin0[k], &ppt);
      ppt0[k] = ppt;
    }

    // Reset weather generator
    SW_MKV_deconstruct();


    //--- Expect that generated weather is different with time-varying seed ----
    // Re-initialize weather generator
    SW_Weather.rng_seed = 0;
    SW_MKV_setup();
    ppt = 0.; // `SW_MKV_today()` uses incoming value of `ppt`

    for (k = 0; k < n; k++) {
      SW_MKV_today(k, &tmax, &tmin, &ppt);

      EXPECT_NE(tmax, tmax0[k]);
      EXPECT_NE(tmin, tmin0[k]);
      if (GT(ppt, 0.)) {
        EXPECT_NE(ppt, ppt0[k]); // ppt is different on wet days
      }
    }

    // Reset weather generator
    SW_MKV_deconstruct();


    //--- Expect that generated weather is reproducible with same seed ------
    // Re-initialize weather generator
    SW_Weather.rng_seed = seed;
    SW_MKV_setup();
    ppt = 0.; // `SW_MKV_today()` uses incoming value of `ppt`

    for (k = 0; k < n; k++) {
      SW_MKV_today(k, &tmax, &tmin, &ppt);

      EXPECT_DOUBLE_EQ(tmax, tmax0[k]);
      EXPECT_DOUBLE_EQ(tmin, tmin0[k]);
      EXPECT_DOUBLE_EQ(ppt, ppt0[k]);
    }


    // Deallocate arrays
    delete[] tmax0;
    delete[] tmin0;
    delete[] ppt0;

    // Reset to previous global state
    Reset_SOILWAT2_after_UnitTest();
  }


  // Test drawing multivariate normal variates for daily maximum/minimum temp
  TEST(WGTest, mvnorm) {
    short k, n = 3;
    RealD tmax = 0., tmin = 0., tval;

    SW_MKV_construct(); // initialize markov_rng

    for (k = 0; k < n; k++) {
      // Create temperature values: here with n = 3: -10, 0, +10
      tval = -10. + 10. * k;

      // Case: wtmax = wtmin, variance = 0, covar = 0 ==> input = output
      (test_mvnorm)(&tmax, &tmin, tval, tval, 0., 0., 0.);
      EXPECT_DOUBLE_EQ(tmax, tval);
      EXPECT_DOUBLE_EQ(tmin, tval);
      EXPECT_DOUBLE_EQ(tmin, tmax);

      // Case: wtmax = wtmin, variance = 0, covar > 0 ==> input = output
      (test_mvnorm)(&tmax, &tmin, tval, tval, 0., 0., 1.);
      EXPECT_DOUBLE_EQ(tmax, tval);
      EXPECT_DOUBLE_EQ(tmin, tval);
      EXPECT_DOUBLE_EQ(tmin, tmax);

      // Case: wtmax > wtmin, variance > 0, covar > 0 ==> tmin <= tmax
      (test_mvnorm)(&tmax, &tmin, tval + 1., tval, 1., 1., 1.);
      EXPECT_LE(tmin, tmax);

      // Case: wtmax < wtmin, variance > 0, covar > 0 ==> tmin == tmax
      (test_mvnorm)(&tmax, &tmin, tval - 1., tval, 1., 1., 1.);
      EXPECT_DOUBLE_EQ(tmin, tmax);
    }

    // Reset to previous global state
    // Reset_SOILWAT2_after_UnitTest();
    SW_MKV_deconstruct();
  }

  TEST(WGDeathTest, mvnorm) {
    RealD tmax = 0., tmin = 0.;

    SW_MKV_construct(); // initialize markov_rng

    // Case: (wT_covar ^ 2 / wTmax_var) > wTmin_var --> LOGFATAL
    EXPECT_DEATH_IF_SUPPORTED(
      (test_mvnorm)(&tmax, &tmin, 0., 0., 1., 1., 2.),
      "@ generic.c LogError"
    );

    // Reset to previous global state
    // Reset_SOILWAT2_after_UnitTest();
    SW_MKV_deconstruct();
  }


  // Test correcting daily temperatures for wet/dry days
  TEST(WGTest, WetDryTemperatureCorrection) {
    RealD
      tmax = 0., tmin = 0., t0 = 0., t10 = 10.,
      wet = 1., dry = 0.,
      cf0 = 0., cf_pos = 5., cf_neg = -5.;

    SW_MKV_construct(); // initialize markov_rng

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

    // Reset to previous global state
    // Reset_SOILWAT2_after_UnitTest();
    SW_MKV_deconstruct();
  }

} // namespace
