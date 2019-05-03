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
  TEST(RNG_unif, ZeroToOneOutput) {
    pcg32_random_t rng0, rng1, rng2;
    int i, n = 10;
    double min = 0., max = 1.;
    double x0, x1, x2;

    RandSeed(7, &rng0);
    RandSeed(1, &rng1); // different seed & different stream than rng0
    RandSeed(1, &rng2); // same seed, but different stream than rng1

    for (i = 0; i < n; i++) {
      // Produce random numbers and check that within bounds of [min, max)
      x0 = RandUni(&rng0);
      EXPECT_PRED_FORMAT2(::testing::DoubleLE, min, x0);
      EXPECT_LT(x0, max);

      x1 = RandUni(&rng1);
      EXPECT_PRED_FORMAT2(::testing::DoubleLE, min, x1);
      EXPECT_LT(x1, max);

      x2 = RandUni(&rng2);
      EXPECT_PRED_FORMAT2(::testing::DoubleLE, min, x2);
      EXPECT_LT(x2, max);

      // Check that random number sequences are different among streams,
      // even if they were initiated with the same seed
      EXPECT_GT(fabs(x0 - x1), 0.);
      EXPECT_GT(fabs(x0 - x2), 0.);
      EXPECT_GT(fabs(x1 - x2), 0.);
    }
  }

  TEST(RNG_unif, FloatRangeOutput) {
    pcg32_random_t rng0, rng1, rng2;
    int i, n = 10;
    float min = 7.5, max = 77.7;
    float x0, x1, x2;

    RandSeed(7, &rng0);
    RandSeed(1, &rng1); // different seed & different stream than rng0
    RandSeed(1, &rng2); // same seed, but different stream than rng1

    for (i = 0; i < n; i++) {
      // Produce random numbers and check that within bounds of [min, max)
      x0 = RandUniFloatRange(min, max, &rng0);
      EXPECT_PRED_FORMAT2(::testing::FloatLE, min, x0);
      EXPECT_LT(x0, max);

      x1 = RandUniFloatRange(min, max, &rng1);
      EXPECT_PRED_FORMAT2(::testing::FloatLE, min, x1);
      EXPECT_LT(x1, max);

      x2 = RandUniFloatRange(min, max, &rng2);
      EXPECT_PRED_FORMAT2(::testing::FloatLE, min, x2);
      EXPECT_LT(x2, max);

      // Check that random number sequences are different among streams,
      // even if they were initiated with the same seed
      EXPECT_GT(fabs(x0 - x1), 0.);
      EXPECT_GT(fabs(x0 - x2), 0.);
      EXPECT_GT(fabs(x1 - x2), 0.);
    }

    RandSeed(0, &rng0);

    // Check that order of min/max doesn't matter
    x0 = RandUniFloatRange(max, min, &rng0);
    EXPECT_PRED_FORMAT2(::testing::FloatLE, min, x0);
    EXPECT_LT(x0, max);

    // Check that result is min if min == max
    EXPECT_FLOAT_EQ(max, RandUniFloatRange(max, max, &rng0));
    EXPECT_FLOAT_EQ(min, RandUniFloatRange(min, min, &rng0));
  }


  TEST(RNG_unif, IntRangeOutput) {
    pcg32_random_t rng0, rng1, rng2;
    int i, n = 10;
    int min = 7, max = 123;
    int x0, x1, x2;
    int ne01 = 0, ne02 = 0, ne12 = 0; // counter of differences among streams

    RandSeed(7, &rng0);
    RandSeed(1, &rng1); // different seed & different stream than rng0
    RandSeed(1, &rng2); // same seed, but different stream than rng1

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
      ne01 += EQ(x0, x1) ? 0 : 1;
      ne02 += EQ(x0, x2) ? 0 : 1;
      ne12 += EQ(x1, x2) ? 0 : 1;
    }

    // Check that random number sequences are different among streams,
    // at least some of the time, even if they were initiated with the same seed
    EXPECT_GT(ne01, 0);
    EXPECT_GT(ne02, 0);
    EXPECT_GT(ne12, 0);


    RandSeed(0, &rng0);

    // Check that order of min/max doesn't matter
    x0 = RandUniIntRange(max, min, &rng0);
    EXPECT_GE(x0, min);
    EXPECT_LE(x0, max);

    // Check that result is min if min == max
    EXPECT_EQ(max, RandUniIntRange(max, max, &rng0));
    EXPECT_EQ(min, RandUniIntRange(min, min, &rng0));
  }



  // This tests the normal random number generator
  TEST(RNG_norm, MeanSD) {
    pcg32_random_t rng0, rng1, rng2;
    int i, n = 10, f = 9999;
    double mean = 0., sd = 1.,
      unlikely[2] = {mean - f * sd, mean + f * sd};
    double
      x0[2] = {unlikely[0], unlikely[1]},
      x1[2] = {unlikely[0], unlikely[1]},
      x2[2] = {unlikely[0], unlikely[1]};

    RandSeed(7, &rng0);
    RandSeed(1, &rng1); // different seed & different stream than rng0
    RandSeed(1, &rng2); // same seed, but different stream than rng1

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
  TEST(RNG_beta, ZeroToOneOutput) {
    pcg32_random_t ZeroToOne_rng;
    RandSeed(0, &ZeroToOne_rng);
    EXPECT_LT(RandBeta(0.5, 2, &ZeroToOne_rng), 1);
    EXPECT_LT(RandBeta(1, 3, &ZeroToOne_rng), 1);
    EXPECT_GT(RandBeta(1, 4, &ZeroToOne_rng), 0);
    EXPECT_GT(RandBeta(0.25, 1, &ZeroToOne_rng), 0);
  }

  TEST(RNG_beta_death, Errors) {
    pcg32_random_t error_rng;
    RandSeed(0, &error_rng);
    EXPECT_DEATH(RandBeta(-0.5, 2, &error_rng), "AA <= 0.0");
    EXPECT_DEATH(RandBeta(1, -3, &error_rng), "BB <= 0.0");
    EXPECT_DEATH(RandBeta(-1, -3, &error_rng), "AA <= 0.0");
  }

} // namespace
