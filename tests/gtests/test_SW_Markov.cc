#include "include/generic.h"        // for GT, fmin
#include "include/myMemory.h"       // for Str_Dup
#include "include/SW_datastructs.h" // for LOG_INFO, SW_MARKOV, SW_NFILES
#include "include/SW_Defines.h"     // for sw_random_t
#include "include/SW_Files.h"       // for SW_F_deconstruct, eMarkovCov
#include "include/SW_Main_lib.h"    // for sw_fail_on_error, sw_init_logs
#include "include/SW_Markov.h"      // for SW_MKV_deconstruct, SW_MKV_init_...
#include "gmock/gmock.h"            // for HasSubstr, MakePredicateFormatte...
#include "gtest/gtest.h"            // for Test, Message, TestPartResult, Po...
#include <stdio.h>                  // for NULL

using ::testing::HasSubstr;

// NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables)
extern void (*test_mvnorm)(double *, double *, double, double, double, double, double, sw_random_t *, LOG_INFO *);
extern void (*test_temp_correct_wetdry)(
    double *, double *, double, double, double, double, double
);

// NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables)

namespace {
// Test the SW_MARKOV constructor 'SW_MKV_construct'
TEST(WeatherGeneratorTest, WeatherGeneratorConstructor) {
    SW_MARKOV SW_Markov;

    LOG_INFO LogInfo;
    // Initialize logs and silence warn/error reporting
    sw_init_logs(NULL, &LogInfo);

    int const rng_seed = 8;

    SW_MKV_init_ptrs(&SW_Markov);
    SW_MKV_construct(rng_seed, &SW_Markov);
    allocateMKV(&SW_Markov, &LogInfo); // allocates memory
    sw_fail_on_error(&LogInfo);        // exit test program if unexpected error

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
    // Initialize logs and silence warn/error reporting
    sw_init_logs(NULL, &LogInfo);

    SW_PATH_INPUTS SW_PathInput;
    SW_F_init_ptrs(&SW_PathInput);

    SW_PathInput.InFiles[eMarkovCov] = Str_Dup("Input/mkv_covar.in", &LogInfo);
    sw_fail_on_error(&LogInfo); // exit test program if unexpected error
    SW_PathInput.InFiles[eMarkovProb] = Str_Dup("Input/mkv_prob.in", &LogInfo);
    sw_fail_on_error(&LogInfo); // exit test program if unexpected error

    int rng_seed;
    // Turn on Markov weather generator
    unsigned int const generateWeatherMethod = 2;

    short k;
    short const n = 18;
    short const seed = 42;
    short const year = 1980;
    double tmax;
    double tmin;
    double ppt;
    double *tmax0 = new double[n];
    double *tmin0 = new double[n];
    double *ppt0 = new double[n];


    //--- Generate some weather values with fixed seed ------

    // Initialize weather generator and read input files mkv_cover and mkv_prob
    rng_seed = seed;
    SW_MKV_init_ptrs(&SW_Markov);
    SW_MKV_setup(
        &SW_Markov,
        rng_seed,
        generateWeatherMethod,
        SW_PathInput.InFiles,
        &LogInfo
    );
    sw_fail_on_error(&LogInfo); // exit test program if unexpected error

    ppt = 0.; // `SW_MKV_today()` uses incoming value of `ppt`

    for (k = 0; k < n; k++) {
        SW_MKV_today(&SW_Markov, k, year, &tmax0[k], &tmin0[k], &ppt, &LogInfo);
        sw_fail_on_error(&LogInfo); // exit test program if unexpected error
        ppt0[k] = ppt;
    }

    // Reset weather generator
    SW_MKV_deconstruct(&SW_Markov);


    //--- Expect that generated weather is different with time-varying seed ----
    // Initialize weather generator and read input files mkv_cover and mkv_prob
    rng_seed = 0;
    SW_MKV_init_ptrs(&SW_Markov);
    SW_MKV_setup(
        &SW_Markov,
        rng_seed,
        generateWeatherMethod,
        SW_PathInput.InFiles,
        &LogInfo
    );
    sw_fail_on_error(&LogInfo); // exit test program if unexpected error

    ppt = 0.; // `SW_MKV_today()` uses incoming value of `ppt`

    for (k = 0; k < n; k++) {
        SW_MKV_today(&SW_Markov, k, year, &tmax, &tmin, &ppt, &LogInfo);
        sw_fail_on_error(&LogInfo); // exit test program if unexpected error

        EXPECT_NE(tmax, tmax0[k]);
        EXPECT_NE(tmin, tmin0[k]);
        // ppt is different on wet days
        if (GT(ppt, 0.)) {
            EXPECT_NE(ppt, ppt0[k]);
        }
    }

    // Reset weather generator
    SW_MKV_deconstruct(&SW_Markov);


    //--- Expect that generated weather is reproducible with same seed ------
    // Initialize weather generator and read input files mkv_cover and mkv_prob
    rng_seed = seed;
    SW_MKV_init_ptrs(&SW_Markov);
    SW_MKV_setup(
        &SW_Markov,
        rng_seed,
        generateWeatherMethod,
        SW_PathInput.InFiles,
        &LogInfo
    );
    sw_fail_on_error(&LogInfo); // exit test program if unexpected error

    ppt = 0.; // `SW_MKV_today()` uses incoming value of `ppt`

    for (k = 0; k < n; k++) {
        SW_MKV_today(&SW_Markov, k, year, &tmax, &tmin, &ppt, &LogInfo);
        sw_fail_on_error(&LogInfo); // exit test program if unexpected error

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

    SW_F_deconstruct(&SW_PathInput);
}

// Test drawing multivariate normal variates for daily maximum/minimum temp
TEST(WeatherGeneratorTest, WeatherGeneratormvnorm) {
    SW_MARKOV SW_Markov;

    LOG_INFO LogInfo;
    // Initialize logs and silence warn/error reporting
    sw_init_logs(NULL, &LogInfo);

    int const rng_seed = 9;
    short k;
    short const n = 3;
    double tmax = 0.;
    double tmin = 0.;
    double tval;

    SW_MKV_init_ptrs(&SW_Markov);
    SW_MKV_construct(rng_seed, &SW_Markov); // initialize markov_rng
    allocateMKV(&SW_Markov, &LogInfo);      // allocates memory

    for (k = 0; k < n; k++) {
        // Create temperature values: here with n = 3: -10, 0, +10
        tval = -10. + 10. * k;

        // Case: wtmax = wtmin, variance = 0, covar = 0 ==> input = output
        (test_mvnorm)(
            &tmax,
            &tmin,
            tval,
            tval,
            0.,
            0.,
            0.,
            &SW_Markov.markov_rng,
            &LogInfo
        );
        sw_fail_on_error(&LogInfo); // exit test program if unexpected error
        EXPECT_DOUBLE_EQ(tmax, tval);
        EXPECT_DOUBLE_EQ(tmin, tval);
        EXPECT_DOUBLE_EQ(tmin, tmax);

        // Case: wtmax = wtmin, variance = 0, covar > 0 ==> input = output
        (test_mvnorm)(
            &tmax,
            &tmin,
            tval,
            tval,
            0.,
            0.,
            1.,
            &SW_Markov.markov_rng,
            &LogInfo
        );
        sw_fail_on_error(&LogInfo); // exit test program if unexpected error
        EXPECT_DOUBLE_EQ(tmax, tval);
        EXPECT_DOUBLE_EQ(tmin, tval);
        EXPECT_DOUBLE_EQ(tmin, tmax);

        // Case: wtmax > wtmin, variance > 0, covar > 0 ==> tmin <= tmax
        (test_mvnorm)(
            &tmax,
            &tmin,
            tval + 1.,
            tval,
            1.,
            1.,
            1.,
            &SW_Markov.markov_rng,
            &LogInfo
        );
        sw_fail_on_error(&LogInfo); // exit test program if unexpected error
        EXPECT_LE(tmin, tmax);

        // Case: wtmax < wtmin, variance > 0, covar > 0 ==> tmin == tmax
        (test_mvnorm)(
            &tmax,
            &tmin,
            tval - 1.,
            tval,
            1.,
            1.,
            1.,
            &SW_Markov.markov_rng,
            &LogInfo
        );
        sw_fail_on_error(&LogInfo); // exit test program if unexpected error
        EXPECT_DOUBLE_EQ(tmin, tmax);

        SW_MKV_deconstruct(&SW_Markov);
    }
}

TEST(WeatherGeneratorTest, WeatherGeneratormvnormDeathTest) {
    SW_MARKOV SW_Markov;

    LOG_INFO LogInfo;
    // Initialize logs and silence warn/error reporting
    sw_init_logs(NULL, &LogInfo);

    int const rng_seed = 11;
    double tmax = 0.;
    double tmin = 0.;

    SW_MKV_init_ptrs(&SW_Markov);
    SW_MKV_construct(rng_seed, &SW_Markov); // initialize markov_rng
    allocateMKV(&SW_Markov, &LogInfo);      // allocates memory

    // Case: (wT_covar ^ 2 / wTmax_var) > wTmin_var --> LOGERROR
    (test_mvnorm)(
        &tmax, &tmin, 0., 0., 1., 1., 2., &SW_Markov.markov_rng, &LogInfo
    );
    // expect error: don't exit test program via `sw_fail_on_error(&LogInfo)`

    // Detect failure by error message
    EXPECT_THAT(LogInfo.errorMsg, HasSubstr("Bad covariance matrix"));

    SW_MKV_deconstruct(&SW_Markov);
}

// Test correcting daily temperatures for wet/dry days
TEST(WeatherGeneratorTest, WeatherGeneratorWetDryTemperatureCorrection) {
    SW_MARKOV SW_Markov;

    LOG_INFO LogInfo;
    // Initialize logs and silence warn/error reporting
    sw_init_logs(NULL, &LogInfo);

    int const rng_seed = 13;
    double tmax = 0.;
    double tmin = 0.;
    double const t0 = 0.;
    double const t10 = 10.;
    double const wet = 1.;
    double const dry = 0.;
    double const cf0 = 0.;
    double const cf_pos = 5.;
    double const cf_neg = -5.;

    SW_MKV_init_ptrs(&SW_Markov);
    SW_MKV_construct(rng_seed, &SW_Markov); // initialize markov_rng
    allocateMKV(&SW_Markov, &LogInfo);      // allocates memory

    // Case: tmax = tmin; wet; cf_*_wet = 0 ==> input = output
    tmax = t0;
    tmin = t0;
    (test_temp_correct_wetdry)(&tmax, &tmin, wet, cf0, cf_pos, cf0, cf_pos);
    EXPECT_DOUBLE_EQ(tmin, tmax);

    // Case: tmax > tmin; wet; cf_*_wet != 0 ==> input + cf_*_wet = output
    tmax = t10;
    tmin = t0;
    (test_temp_correct_wetdry)(
        &tmax, &tmin, wet, cf_neg, cf_pos, cf_neg, cf_pos
    );
    EXPECT_DOUBLE_EQ(tmax, t10 + cf_neg);
    EXPECT_DOUBLE_EQ(tmin, t0 + cf_neg);
    EXPECT_LE(tmin, tmax);

    // Case: tmax > tmin; dry; cf_*_dry != 0 ==> input + cf_*_dry = output
    tmax = t10;
    tmin = t0;
    (test_temp_correct_wetdry)(
        &tmax, &tmin, dry, cf_neg, cf_pos, cf_neg, cf_pos
    );
    EXPECT_DOUBLE_EQ(tmax, t10 + cf_pos);
    EXPECT_DOUBLE_EQ(tmin, t0 + cf_pos);
    EXPECT_LE(tmin, tmax);

    // Case: tmax < tmin; wet; cf_*_wet > 0 ==> tmin <= tmax
    tmax = t0;
    tmin = t10;
    (test_temp_correct_wetdry)(
        &tmax, &tmin, wet, cf_pos, cf_pos, cf_pos, cf_pos
    );
    EXPECT_DOUBLE_EQ(tmax, t0 + cf_pos);
    EXPECT_DOUBLE_EQ(tmin, fmin(tmax, t10 + cf_pos));
    EXPECT_LE(tmin, tmax);

    SW_MKV_deconstruct(&SW_Markov);
}
} // namespace
