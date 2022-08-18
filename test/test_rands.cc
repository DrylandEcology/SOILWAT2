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
#include "../pcg/pcg_basic.h"


namespace {
  // This tests the uniform random number generator
  TEST(RNGUnifTest, ZeroToOneOutput) {
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
      EXPECT_EQ(x71, x71b);

      // Check that rngs with different state and/or different sequence produce
      // different output
      EXPECT_TRUE(x71 != x11);
      EXPECT_TRUE(x71 != x12);
      EXPECT_TRUE(x11 != x12);
    }
  }

  TEST(RNGUnifTest, FloatRangeOutput) {
    pcg32_random_t rng0, rng1, rng2;
    int i, n = 10;
    float min = 7.5, max = 77.7;
    float x0, x1, x2;

    RandSeed(7u, 1u, &rng0);
    RandSeed(1u, 1u, &rng1); // different seed & different stream than rng0
    RandSeed(1u, 2u, &rng2); // same seed, but different stream than rng1

    for (i = 0; i < n; i++) {
      // Produce random numbers and check that within bounds of [min, max)
      x0 = RandUniFloatRange(min, max, &rng0);
      EXPECT_GE(x0, min);
      EXPECT_LT(x0, max);

      x1 = RandUniFloatRange(min, max, &rng1);
      EXPECT_GE(x1, min);
      EXPECT_LT(x1, max);

      x2 = RandUniFloatRange(min, max, &rng2);
      EXPECT_GE(x2, min);
      EXPECT_LT(x2, max);

      // Check that random number sequences are different among streams,
      // even if they were initiated with the same seed
      EXPECT_GT(fabs(x0 - x1), 0.);
      EXPECT_GT(fabs(x0 - x2), 0.);
      EXPECT_GT(fabs(x1 - x2), 0.);
    }

    // Check that order of min/max doesn't matter
    x0 = RandUniFloatRange(max, min, &rng0);
    EXPECT_GE(x0, min);
    EXPECT_LT(x0, max);

    // Check that result is min if min == max
    EXPECT_FLOAT_EQ(max, RandUniFloatRange(max, max, &rng0));
    EXPECT_FLOAT_EQ(min, RandUniFloatRange(min, min, &rng0));
  }


  TEST(RNGUnifTest, IntRangeOutput) {
    pcg32_random_t rng0, rng1, rng2;
    int i, n = 10;
    int min = 7, max = 123;
    int x0, x1, x2;
    int ne01 = 0, ne02 = 0, ne12 = 0; // counter of differences among streams

    RandSeed(7u, 1u, &rng0);
    RandSeed(1u, 1u, &rng1); // different seed & different stream than rng0
    RandSeed(1u, 2u, &rng2); // same seed, but different stream than rng1

    for (i = 0; i < n; i++) {
      // Produce random numbers and check that within bounds of [min, max]
      x0 = RandUniIntRange(min, max, &rng0);
      EXPECT_GE(x0, min);
      EXPECT_LE(x0, max);

      x1 = RandUniIntRange(min, max, &rng1);
      EXPECT_GE(x1, min);
      EXPECT_LE(x1, max);

      x2 = RandUniIntRange(min, max, &rng2);
      EXPECT_GE(x2, min);
      EXPECT_LE(x2, max);

      // Count differences among streams
      ne01 += (x0 == x1) ? 0 : 1;
      ne02 += (x0 == x2) ? 0 : 1;
      ne12 += (x1 == x2) ? 0 : 1;
    }

    // Check that random number sequences are different among streams,
    // at least some of the time, even if they were initiated with the same seed
    EXPECT_GT(ne01, 0);
    EXPECT_GT(ne02, 0);
    EXPECT_GT(ne12, 0);



    // Check that order of min/max doesn't matter
    x0 = RandUniIntRange(max, min, &rng0);
    EXPECT_GE(x0, min);
    EXPECT_LE(x0, max);

    // Check that result is min if min == max
    EXPECT_EQ(max, RandUniIntRange(max, max, &rng0));
    EXPECT_EQ(min, RandUniIntRange(min, min, &rng0));
  }



  // This tests the normal random number generator
  TEST(RNGNormTest, MeanSD) {
    pcg32_random_t rng0, rng1, rng2;
    int i, n = 10, f = 9999;
    double mean = 0., sd = 1.,
      unlikely[2] = {mean - f * sd, mean + f * sd};
    double
      x0[2] = {unlikely[0], unlikely[1]},
      x1[2] = {unlikely[0], unlikely[1]},
      x2[2] = {unlikely[0], unlikely[1]};

    RandSeed(7u, 1u, &rng0);
    RandSeed(1u, 1u, &rng1); // different seed & different stream than rng0
    RandSeed(1u, 2u, &rng2); // same seed, but different stream than rng1

    for (i = 0; i < n; i++) {
      // Produce random numbers and check that within likely bounds
      x0[1] = RandNorm(mean, sd, &rng0);
      EXPECT_GT(x0[1], unlikely[0]);
      EXPECT_LT(x0[1], unlikely[1]);

      x1[1] = RandNorm(mean, sd, &rng1);
      EXPECT_GT(x1[1], unlikely[0]);
      EXPECT_LT(x1[1], unlikely[1]);

      x2[1] = RandNorm(mean, sd, &rng2);
      EXPECT_GT(x2[1], unlikely[0]);
      EXPECT_LT(x2[1], unlikely[1]);

      // Check that random number sequences are different among streams,
      // even if they were initiated with the same seed
      EXPECT_GT(fabs(x0[1] - x1[1]), 0.);
      EXPECT_GT(fabs(x0[1] - x2[1]), 0.);
      EXPECT_GT(fabs(x1[1] - x2[1]), 0.);

      // Check that random number is different from previous one
      EXPECT_GT(fabs(x0[1] - x0[0]), 0.);
      EXPECT_GT(fabs(x1[1] - x1[0]), 0.);
      EXPECT_GT(fabs(x2[1] - x2[0]), 0.);

      // Update arrays
      x0[0] = x0[1];
      x1[0] = x1[1];
      x2[0] = x2[1];
    }
  }




  // This tests the beta random number generator
  TEST(RNGBetaTest, ZeroToOneOutput) {
    pcg32_random_t ZeroToOne_rng;
    RandSeed(0u, 0u, &ZeroToOne_rng);
    EXPECT_LT(RandBeta(0.5, 2, &ZeroToOne_rng), 1);
    EXPECT_LT(RandBeta(1, 3, &ZeroToOne_rng), 1);
    EXPECT_GT(RandBeta(1, 4, &ZeroToOne_rng), 0);
    EXPECT_GT(RandBeta(0.25, 1, &ZeroToOne_rng), 0);
  }

  TEST(RNGBetaDeathTest, Errors) {
    pcg32_random_t error_rng;
    RandSeed(0u, 0u, &error_rng);
    EXPECT_DEATH_IF_SUPPORTED(RandBeta(-0.5, 2, &error_rng), "AA <= 0.0");
    EXPECT_DEATH_IF_SUPPORTED(RandBeta(1, -3, &error_rng), "BB <= 0.0");
    EXPECT_DEATH_IF_SUPPORTED(RandBeta(-1, -3, &error_rng), "AA <= 0.0");
  }

} // namespace
