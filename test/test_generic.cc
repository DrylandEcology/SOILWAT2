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
#include "../SW_Defines.h"


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

  TEST(RunningAggregatorsTest, RunningMean) {
    double m_at_k = 0.;

    for (k = 0; k < N; k++)
    {
      m_at_k = get_running_mean(k + 1, m_at_k, x[k]);
      EXPECT_DOUBLE_EQ(m_at_k, m[k]);
    }
  }

  TEST(RunningAggregatorsTest, RunningSD) {
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

} // namespace
