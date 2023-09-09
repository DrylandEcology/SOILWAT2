#include <gmock/gmock.h>
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
#include "external/pcg/pcg_basic.h"
#include "include/SW_Main_lib.h"

#include "tests/gtests/sw_testhelpers.h"

using ::testing::HasSubstr;


namespace {
  // This tests the uniform random number generator
  TEST(RNGTest, RNGUnifZeroToOneOutput) {
    pcg32_random_t rng71, rng71b, rng11, rng12;
    int i, n = 10;
    double min = 0., max = 1.;
    double x71, x71b, x11, x12;

    // Seed rngs
    RandSeed(7u, 1u, &rng71);
    RandSeed(7u, 1u, &rng71b); // same state & same sequence as rng71
    RandSeed(1u, 1u, &rng11); // different state but same sequence as rng71
    RandSeed(1u, 2u, &rng12); // same state but different sequence as rng11


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

      // Check that rngs with different state and/or different sequence produce
      // different output
      EXPECT_TRUE(x71 != x11);
      EXPECT_TRUE(x71 != x12);
      EXPECT_TRUE(x11 != x12);
    }
  }

  TEST(RNGTest, RNGUnifFloatRangeOutput) {
    pcg32_random_t rng71, rng71b, rng11, rng12;
    int i, n = 10;
    float min = 7.5, max = 77.7;
    double x71, x71b, x11, x12, x0;

    // Seed rngs
    RandSeed(7u, 1u, &rng71);
    RandSeed(7u, 1u, &rng71b); // same state & same sequence as rng71
    RandSeed(1u, 1u, &rng11); // different state but same sequence as rng71
    RandSeed(1u, 2u, &rng12); // same state but different sequence as rng11


    for (i = 0; i < n; i++) {
      // Produce random numbers and check that within bounds of [min, max)
      x71 = RandUniFloatRange(min, max, &rng71);
      EXPECT_GE(x71, min);
      EXPECT_LT(x71, max);

      x71b = RandUniFloatRange(min, max, &rng71b);
      EXPECT_GE(x71b, min);
      EXPECT_LT(x71b, max);

      x11 = RandUniFloatRange(min, max, &rng11);
      EXPECT_GE(x11, min);
      EXPECT_LT(x11, max);

      x12 = RandUniFloatRange(min, max, &rng12);
      EXPECT_GE(x12, min);
      EXPECT_LT(x12, max);

      // Check that rngs with identical state & sequence produce same output
      EXPECT_DOUBLE_EQ(x71, x71b);

      // Check that rngs with different state and/or different sequence produce
      // different output
      EXPECT_TRUE(x71 != x11);
      EXPECT_TRUE(x71 != x12);
      EXPECT_TRUE(x11 != x12);
    }

    // Check that order of min/max doesn't matter
    x0 = RandUniFloatRange(max, min, &rng11);
    EXPECT_GE(x0, min);
    EXPECT_LT(x0, max);

    // Check that result is min if min == max
    EXPECT_FLOAT_EQ(max, RandUniFloatRange(max, max, &rng11));
    EXPECT_FLOAT_EQ(min, RandUniFloatRange(min, min, &rng11));
  }


  TEST(RNGTest, RNGUnifIntRangeOutput) {
    pcg32_random_t rng71, rng71b, rng11, rng12;
    int i, n = 10;
    int min = 7, max = 123;
    double x71, x71b, x11, x12, x0;

    // Seed rngs
    RandSeed(7u, 1u, &rng71);
    RandSeed(7u, 1u, &rng71b); // same state & same sequence as rng71
    RandSeed(1u, 1u, &rng11); // different state but same sequence as rng71
    RandSeed(1u, 2u, &rng12); // same state but different sequence as rng11


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

      // Check that rngs with different state and/or different sequence produce
      // different output
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
    pcg32_random_t rng71, rng71b, rng11, rng12;
    int i, n = 10, f = 9999;
    double
      mean = 0., sd = 1.,
      unlikely[2] = {mean - f * sd, mean + f * sd};
    double
      x71[2] = {-SW_MISSING, SW_MISSING},
      x71b[2] = {-SW_MISSING, SW_MISSING},
      x11[2] = {-SW_MISSING, SW_MISSING},
      x12[2] = {-SW_MISSING, SW_MISSING};

    // Seed rngs
    RandSeed(7u, 1u, &rng71);
    RandSeed(7u, 1u, &rng71b); // same state & same sequence as rng71
    RandSeed(1u, 1u, &rng11); // different state but same sequence as rng71
    RandSeed(1u, 2u, &rng12); // same state but different sequence as rng11


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

      // Check that rngs with different state and/or different sequence produce
      // different output
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
    sw_init_logs(NULL, &LogInfo); // Initialize logs and silence warn/error reporting

    pcg32_random_t ZeroToOne_rng;
    RandSeed(0u, 0u, &ZeroToOne_rng);
    EXPECT_LT(RandBeta(0.5, 2, &ZeroToOne_rng, &LogInfo), 1);
    EXPECT_LT(RandBeta(1, 3, &ZeroToOne_rng, &LogInfo), 1);
    EXPECT_GT(RandBeta(1, 4, &ZeroToOne_rng, &LogInfo), 0);
    EXPECT_GT(RandBeta(0.25, 1, &ZeroToOne_rng, &LogInfo), 0);


    pcg32_random_t rng71, rng71b, rng11, rng12;
    int i, n = 10;
    double
      a = 0.25, b = 2.,
      min = 0., max = 1.,
      x71, x71b, x11, x12;

    // Seed rngs
    RandSeed(7u, 1u, &rng71);
    RandSeed(7u, 1u, &rng71b); // same state & same sequence as rng71
    RandSeed(1u, 1u, &rng11); // different state but same sequence as rng71
    RandSeed(1u, 2u, &rng12); // same state but different sequence as rng11


    for (i = 0; i < n; i++) {
      // Produce random numbers and check that within bounds of [min, max]
      x71 = RandBeta(a, b, &rng71, &LogInfo);
      EXPECT_GE(x71, min);
      EXPECT_LE(x71, max);

      x71b = RandBeta(a, b, &rng71b, &LogInfo);
      EXPECT_GE(x71b, min);
      EXPECT_LE(x71b, max);

      x11 = RandBeta(a, b, &rng11, &LogInfo);
      EXPECT_GE(x11, min);
      EXPECT_LE(x11, max);

      x12 = RandBeta(a, b, &rng12, &LogInfo);
      EXPECT_GE(x12, min);
      EXPECT_LE(x12, max);

      // Check that rngs with identical state & sequence produce same output
      EXPECT_DOUBLE_EQ(x71, x71b);

      // Check that rngs with different state and/or different sequence produce
      // different output
      EXPECT_TRUE(x71 != x11);
      EXPECT_TRUE(x71 != x12);
      EXPECT_TRUE(x11 != x12);
    }
  }

  TEST(RNGTest, RNGBetaErrorsDeathTest) {

    LOG_INFO LogInfo;
    sw_init_logs(NULL, &LogInfo); // Initialize logs and silence warn/error reporting

    pcg32_random_t error_rng;
    RandSeed(0u, 0u, &error_rng);
    RandBeta(-0.5, 2, &error_rng, &LogInfo);
    EXPECT_THAT(LogInfo.errorMsg, HasSubstr("AA <= 0.0"));

    RandBeta(1, -3, &error_rng, &LogInfo);
    EXPECT_THAT(LogInfo.errorMsg, HasSubstr("BB <= 0.0"));

    RandBeta(-1, -3, &error_rng, &LogInfo);
    EXPECT_THAT(LogInfo.errorMsg, HasSubstr("AA <= 0.0"));
  }

} // namespace
