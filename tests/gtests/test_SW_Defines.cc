#include "include/SW_Defines.h"          // for SW_MISSING
#include "tests/gtests/sw_testhelpers.h" // for missing
#include "gtest/gtest.h"                 // for AssertionResult, Message, Test
#include <cmath>                         // for exp, INFINITY, NAN
#include <float.h>                       // for DBL_MIN

namespace {
TEST(SWDefines, SWDefinesMissingValues) {
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
