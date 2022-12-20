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

#include "sw_testhelpers.h" // get the re-defined `missing`


namespace {

  TEST(SWDefinesTest, MissingValues) {
    // SOILWAT2 missing value
    EXPECT_TRUE(missing(SW_MISSING));

    // isfinite()
    EXPECT_TRUE(missing(NAN));
    EXPECT_TRUE(missing(INFINITY));
    EXPECT_TRUE(missing(exp(800)));

    // non-missing examples
    EXPECT_FALSE(missing(0.0));
    EXPECT_FALSE(missing(DBL_MIN / 2.0));
    EXPECT_FALSE(missing(1.0));
  }

} // namespace
