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


extern SW_MODEL SW_Model;
extern SW_MARKOV SW_Markov;

extern void (*test_mvnorm)(RealD *, RealD *, RealD, RealD, RealD, RealD, RealD);


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
  }


  // Test drawing multivariate normal variates for daily maximum/minimum temp
  TEST(WGTest, mvnorm) {
    short k, n = 3;
    RealD tmax = 0., tmin = 0.,
      val[n] = {-10., 0., 10.};


    for (k = 0; k < n; k++) {
      // Case: wtmax = wtmin, variance = 0, covar = 0 ==> input = output
      (test_mvnorm)(&tmax, &tmin, val[k], val[k], 0., 0., 0.);
      EXPECT_DOUBLE_EQ(tmax, val[k]);
      EXPECT_DOUBLE_EQ(tmin, val[k]);
      EXPECT_DOUBLE_EQ(tmin, tmax);

      // Case: wtmax = wtmin, variance = 0, covar > 0 ==> input = output
      (test_mvnorm)(&tmax, &tmin, val[k], val[k], 0., 0., 1.);
      EXPECT_DOUBLE_EQ(tmax, val[k]);
      EXPECT_DOUBLE_EQ(tmin, val[k]);
      EXPECT_DOUBLE_EQ(tmin, tmax);

      // Case: wtmax > wtmin, variance > 0, covar > 0 ==> tmin <= tmax
      (test_mvnorm)(&tmax, &tmin, val[k] + 1., val[k], 1., 1., 1.);
      EXPECT_LE(tmin, tmax);

      // Case: wtmax < wtmin, variance > 0, covar > 0 ==> tmin == tmax
      (test_mvnorm)(&tmax, &tmin, val[k] - 1., val[k], 1., 1., 1.);
      EXPECT_DOUBLE_EQ(tmin, tmax);
    }

    // Reset to previous global state
    Reset_SOILWAT2_after_UnitTest();
  }

  TEST(WGTest, mvnormDeathTest) {
    RealD tmax = 0., tmin = 0.;

    // Case: (wT_covar ^ 2 / wTmax_var) > wTmin_var --> LOGFATAL
    EXPECT_DEATH_IF_SUPPORTED(
      (test_mvnorm)(&tmax, &tmin, 0., 0., 1., 1., 2.),
      "@ generic.c LogError");
  }

} // namespace
