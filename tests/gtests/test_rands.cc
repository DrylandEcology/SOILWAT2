#include "include/rands.h"          // for RandSeed, RandBeta, RandUniFloat...
#include "include/SW_datastructs.h" // for LOG_INFO
#include "include/SW_Defines.h"     // for sw_random_t, SW_MISSING
#include "include/SW_Main_lib.h"    // for sw_fail_on_error, sw_init_logs
#include "gmock/gmock.h"            // for HasSubstr, MakePredicateFormatte...
#include "gtest/gtest.h"            // for Test, Message, AssertionResult, T...
#include <stdio.h>                  // for NULL

using ::testing::HasSubstr;

namespace {
// This tests the uniform random number generator
TEST(RNGTest, RNGUnifZeroToOneOutput) {
    sw_random_t rng71;
    sw_random_t rng71b;
    sw_random_t rng11;
    sw_random_t rng12;
    int i;
    int const n = 10;
    double const min = 0.;
    double const max = 1.;
    double x71;
    double x71b;
    double x11;
    double x12;

    // Seed rngs
    RandSeed(7u, 1u, &rng71);
    RandSeed(7u, 1u, &rng71b); // same state & same sequence as rng71
    RandSeed(1u, 1u, &rng11);  // different state but same sequence as rng71
    RandSeed(1u, 2u, &rng12);  // same state but different sequence as rng11


    for (i = 0; i < n; i++) {
        // Produce random numbers and check that within bounds of [min, max)
        x71 = RandUni(&rng71);
        EXPECT_GE(x71, min);
        EXPECT_LT(x71, max);

        x71b = RandUni(&rng71b);
        EXPECT_GE(x71b, min);
        EXPECT_LT(x71b, max);

        x11 = RandUni(&rng11);
        EXPECT_GE(x11, min);
        EXPECT_LT(x11, max);

        x12 = RandUni(&rng12);
        EXPECT_GE(x12, min);
        EXPECT_LT(x12, max);

        // Check that rngs with identical state & sequence produce same output
        EXPECT_DOUBLE_EQ(x71, x71b);

        // Check that rngs with different state and/or different sequence
        // produce different output
        EXPECT_TRUE(x71 != x11);
        EXPECT_TRUE(x71 != x12);
        EXPECT_TRUE(x11 != x12);
    }
}

TEST(RNGTest, RNGUnifFloatRangeOutput) {
    sw_random_t rng71;
    sw_random_t rng71b;
    sw_random_t rng11;
    sw_random_t rng12;
    int i;
    int const n = 10;
    float const lowVal = 7.5;
    float const highVal = 77.7;
    double x71;
    double x71b;
    double x11;
    double x12;
    double x0;

    // Seed rngs
    RandSeed(7u, 1u, &rng71);
    RandSeed(7u, 1u, &rng71b); // same state & same sequence as rng71
    RandSeed(1u, 1u, &rng11);  // different state but same sequence as rng71
    RandSeed(1u, 2u, &rng12);  // same state but different sequence as rng11


    for (i = 0; i < n; i++) {
        // Check that random numbers are within bounds of [lowVal, highVal)
        x71 = RandUniFloatRange(lowVal, highVal, &rng71);
        EXPECT_GE(x71, lowVal);
        EXPECT_LT(x71, highVal);

        x71b = RandUniFloatRange(lowVal, highVal, &rng71b);
        EXPECT_GE(x71b, lowVal);
        EXPECT_LT(x71b, highVal);

        x11 = RandUniFloatRange(lowVal, highVal, &rng11);
        EXPECT_GE(x11, lowVal);
        EXPECT_LT(x11, highVal);

        x12 = RandUniFloatRange(lowVal, highVal, &rng12);
        EXPECT_GE(x12, lowVal);
        EXPECT_LT(x12, highVal);

        // Check that rngs with identical state & sequence produce same output
        EXPECT_DOUBLE_EQ(x71, x71b);

        // Check that rngs with different state and/or different sequence
        // produce different output
        EXPECT_TRUE(x71 != x11);
        EXPECT_TRUE(x71 != x12);
        EXPECT_TRUE(x11 != x12);
    }

    // Check that order of lowVal/highVal doesn't matter
    x0 = RandUniFloatRange(highVal, lowVal, &rng11);
    EXPECT_GE(x0, lowVal);
    EXPECT_LT(x0, highVal);

    // Check that result is lowVal if min == max
    EXPECT_FLOAT_EQ(highVal, RandUniFloatRange(highVal, highVal, &rng11));
    EXPECT_FLOAT_EQ(lowVal, RandUniFloatRange(lowVal, lowVal, &rng11));
}

TEST(RNGTest, RNGUnifIntRangeOutput) {
    sw_random_t rng71;
    sw_random_t rng71b;
    sw_random_t rng11;
    sw_random_t rng12;
    int i;
    int const n = 10;
    int const min = 7;
    int const max = 123;
    long int x71;
    long int x71b;
    long int x11;
    long int x12;
    long int x0;

    // Seed rngs
    RandSeed(7u, 1u, &rng71);
    RandSeed(7u, 1u, &rng71b); // same state & same sequence as rng71
    RandSeed(1u, 1u, &rng11);  // different state but same sequence as rng71
    RandSeed(1u, 2u, &rng12);  // same state but different sequence as rng11


    for (i = 0; i < n; i++) {
        // Produce random numbers and check that within bounds of [min, max]
        x71 = RandUniIntRange(min, max, &rng71);
        EXPECT_GE(x71, min);
        EXPECT_LE(x71, max);

        x71b = RandUniIntRange(min, max, &rng71b);
        EXPECT_GE(x71b, min);
        EXPECT_LE(x71b, max);

        x11 = RandUniIntRange(min, max, &rng11);
        EXPECT_GE(x11, min);
        EXPECT_LE(x11, max);

        x12 = RandUniIntRange(min, max, &rng12);
        EXPECT_GE(x12, min);
        EXPECT_LE(x12, max);

        // Check that rngs with identical state & sequence produce same output
        EXPECT_DOUBLE_EQ(x71, x71b);

        // Check that rngs with different state and/or different sequence
        // produce different output
        EXPECT_TRUE(x71 != x11);
        EXPECT_TRUE(x71 != x12);
        EXPECT_TRUE(x11 != x12);
    }


    // Check that order of min/max doesn't matter
    x0 = RandUniIntRange(max, min, &rng11);
    EXPECT_GE(x0, min);
    EXPECT_LE(x0, max);

    // Check that result is min if min == max
    EXPECT_EQ(max, RandUniIntRange(max, max, &rng11));
    EXPECT_EQ(min, RandUniIntRange(min, min, &rng11));
}

// This tests the normal random number generator
TEST(RNGTest, RNGNormMeanSD) {
    sw_random_t rng71;
    sw_random_t rng71b;
    sw_random_t rng11;
    sw_random_t rng12;
    int i;
    int const n = 10;
    int const f = 9999;
    double const mean = 0.;
    double const sd = 1.;
    double const unlikely[2] = {mean - f * sd, mean + f * sd};
    double x71[2] = {-SW_MISSING, SW_MISSING};
    double x71b[2] = {-SW_MISSING, SW_MISSING};
    double x11[2] = {-SW_MISSING, SW_MISSING};
    double x12[2] = {-SW_MISSING, SW_MISSING};

    // Seed rngs
    RandSeed(7u, 1u, &rng71);
    RandSeed(7u, 1u, &rng71b); // same state & same sequence as rng71
    RandSeed(1u, 1u, &rng11);  // different state but same sequence as rng71
    RandSeed(1u, 2u, &rng12);  // same state but different sequence as rng11


    for (i = 0; i < n; i++) {
        // Produce random numbers and check that within likely bounds
        x71[1] = RandNorm(mean, sd, &rng71);
        EXPECT_GT(x71[1], unlikely[0]);
        EXPECT_LT(x71[1], unlikely[1]);

        x71b[1] = RandNorm(mean, sd, &rng71b);
        EXPECT_GT(x71b[1], unlikely[0]);
        EXPECT_LT(x71b[1], unlikely[1]);

        x11[1] = RandNorm(mean, sd, &rng11);
        EXPECT_GT(x11[1], unlikely[0]);
        EXPECT_LT(x11[1], unlikely[1]);

        x12[1] = RandNorm(mean, sd, &rng12);
        EXPECT_GT(x12[1], unlikely[0]);
        EXPECT_LT(x12[1], unlikely[1]);


        // Check that rngs with identical state & sequence produce same output
        EXPECT_DOUBLE_EQ(x71[1], x71b[1]);

        // Check that rngs with different state and/or different sequence
        // produce different output
        EXPECT_TRUE(x71[1] != x11[1]);
        EXPECT_TRUE(x71[1] != x12[1]);
        EXPECT_TRUE(x11[1] != x12[1]);

        // Check that random numbers are different from previous call
        EXPECT_TRUE(x71[1] != x71[0]);
        EXPECT_TRUE(x71b[1] != x71b[0]);
        EXPECT_TRUE(x11[1] != x11[0]);
        EXPECT_TRUE(x12[1] != x12[0]);


        // Update arrays
        x71[0] = x71[1];
        x71b[0] = x71b[1];
        x11[0] = x11[1];
        x12[0] = x12[1];
    }
}

// This tests the beta random number generator
TEST(RNGTest, RNGBetaZeroToOneOutput) {

    LOG_INFO LogInfo;
    // Initialize logs and silence warn/error reporting
    sw_init_logs(NULL, &LogInfo);

    sw_random_t ZeroToOne_rng;
    RandSeed(0u, 0u, &ZeroToOne_rng);

    EXPECT_LT(RandBeta(0.5, 2, &ZeroToOne_rng, &LogInfo), 1);
    sw_fail_on_error(&LogInfo); // exit test program if unexpected error

    EXPECT_LT(RandBeta(1, 3, &ZeroToOne_rng, &LogInfo), 1);
    sw_fail_on_error(&LogInfo); // exit test program if unexpected error

    EXPECT_GT(RandBeta(1, 4, &ZeroToOne_rng, &LogInfo), 0);
    sw_fail_on_error(&LogInfo); // exit test program if unexpected error

    EXPECT_GT(RandBeta(0.25, 1, &ZeroToOne_rng, &LogInfo), 0);
    sw_fail_on_error(&LogInfo); // exit test program if unexpected error


    sw_random_t rng71;
    sw_random_t rng71b;
    sw_random_t rng11;
    sw_random_t rng12;
    int i;
    int const n = 10;
    double const a = 0.25;
    double const b = 2.;
    double const min = 0.;
    double const max = 1.;
    double x71;
    double x71b;
    double x11;
    double x12;

    // Seed rngs
    RandSeed(7u, 1u, &rng71);
    RandSeed(7u, 1u, &rng71b); // same state & same sequence as rng71
    RandSeed(1u, 1u, &rng11);  // different state but same sequence as rng71
    RandSeed(1u, 2u, &rng12);  // same state but different sequence as rng11


    for (i = 0; i < n; i++) {
        // Produce random numbers and check that within bounds of [min, max]
        x71 = RandBeta(a, b, &rng71, &LogInfo);
        sw_fail_on_error(&LogInfo); // exit test program if unexpected error
        EXPECT_GE(x71, min);
        EXPECT_LE(x71, max);

        x71b = RandBeta(a, b, &rng71b, &LogInfo);
        sw_fail_on_error(&LogInfo); // exit test program if unexpected error
        EXPECT_GE(x71b, min);
        EXPECT_LE(x71b, max);

        x11 = RandBeta(a, b, &rng11, &LogInfo);
        sw_fail_on_error(&LogInfo); // exit test program if unexpected error
        EXPECT_GE(x11, min);
        EXPECT_LE(x11, max);

        x12 = RandBeta(a, b, &rng12, &LogInfo);
        sw_fail_on_error(&LogInfo); // exit test program if unexpected error
        EXPECT_GE(x12, min);
        EXPECT_LE(x12, max);

        // Check that rngs with identical state & sequence produce same output
        EXPECT_DOUBLE_EQ(x71, x71b);

        // Check that rngs with different state and/or different sequence
        // produce different output
        EXPECT_TRUE(x71 != x11);
        EXPECT_TRUE(x71 != x12);
        EXPECT_TRUE(x11 != x12);
    }
}

TEST(RNGTest, RNGBetaErrorsDeathTest) {

    LOG_INFO LogInfo;
    // Initialize logs and silence warn/error reporting
    sw_init_logs(NULL, &LogInfo);

    sw_random_t error_rng;
    RandSeed(0u, 0u, &error_rng);
    RandBeta(-0.5, 2, &error_rng, &LogInfo);
    // expect error: don't exit test program via `sw_fail_on_error(&LogInfo)`
    EXPECT_THAT(LogInfo.errorMsg, HasSubstr("AA <= 0.0"));

    RandBeta(1, -3, &error_rng, &LogInfo);
    // expect error: don't exit test program via `sw_fail_on_error(&LogInfo)`
    EXPECT_THAT(LogInfo.errorMsg, HasSubstr("BB <= 0.0"));

    RandBeta(-1, -3, &error_rng, &LogInfo);
    // expect error: don't exit test program via `sw_fail_on_error(&LogInfo)`
    EXPECT_THAT(LogInfo.errorMsg, HasSubstr("AA <= 0.0"));
}
} // namespace
