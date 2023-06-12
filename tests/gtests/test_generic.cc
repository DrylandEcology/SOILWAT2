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
#include "include/SW_Defines.h"
#include "tests/gtests/sw_testhelpers.h"


namespace {
  const unsigned int N = 9;
  unsigned int k;
  double
    x[N] = {-4., -3., -2., -1., 0., 1., 2., 3., 4.},
    // m calculated in R with `for (k in seq_along(x)) print(mean(x[1:k]))`
    m[N] = {-4, -3.5, -3, -2.5, -2, -1.5, -1, -0.5, 0},
    // sd calculated in R with `for (k in seq_along(x)) print(sd(x[1:k]))`
    sd[N] = {SW_MISSING, 0.7071068, 1., 1.290994, 1.581139, 1.870829, 2.160247,
      2.44949, 2.738613};
  float tol = 1e-6;

  TEST_F(AllTest, GenericRunningMean) {
    double m_at_k = 0.;

    for (k = 0; k < N; k++)
    {
      m_at_k = get_running_mean(k + 1, m_at_k, x[k]);
      EXPECT_DOUBLE_EQ(m_at_k, m[k]);
    }
  }

  TEST_F(AllTest, GenericRunningSD) {
    double ss, sd_at_k;

    for (k = 0; k < N; k++)
    {
      if (k == 0)
      {
        ss = get_running_sqr(0., m[k], x[k]);

      } else {
        ss += get_running_sqr(m[k - 1], m[k], x[k]);

        sd_at_k = final_running_sd(k + 1, ss); // k is base0
        EXPECT_NEAR(sd_at_k, sd[k], tol);
      }
    }
  }

    TEST_F(AllTest, GenericUnexpectedAndExpectedCasesSD) {
        double value[1] = {5.};
        double values[5] = {5.4, 3.4, 7.6, 5.6, 1.8};
        double oneValMissing[5] = {5.4, SW_MISSING, 7.6, 5.6, 1.8};

        double standardDev = standardDeviation(value, 1);

        // Testing that one value for a standard deviation is `NAN`
        EXPECT_TRUE(isnan(standardDev));

        standardDev = standardDeviation(value, 0);

        // Testing that no value put into standard deviation is `NAN`
        EXPECT_DOUBLE_EQ(standardDev, 0.);

        standardDev = standardDeviation(values, 5);

        // Testing the standard deviation function on a normal set of data
        EXPECT_NEAR(standardDev, 2.22441, tol);

        standardDev = standardDeviation(oneValMissing, 5);

        // Testing the standard deviation function on a normal set of data with
        // one value set to SW_MISSING
        EXPECT_NEAR(standardDev, 2.413848, tol);
    }

    TEST_F(AllTest, GenericUnexpectedAndExpectedCasesMean) {

        double result;
        double values[5] = {1.8, 2.2, 10., 13.5, 3.2};
        double oneValMissing[5] = {4.3, 2.6, SW_MISSING, 17.1, 32.4};

        result = mean(values, 0);

        // Testing that a set of size zero returns `NAN` for a mean
        EXPECT_TRUE(isnan(result));

        result = mean(values, 5);

        // Testing the mean function on a normal set of data
        EXPECT_FLOAT_EQ(result, 6.14);

        result = mean(oneValMissing, 5);

        // Testing the mean function on a normal set of data
        EXPECT_FLOAT_EQ(result, 14.1);

    }

} // namespace
