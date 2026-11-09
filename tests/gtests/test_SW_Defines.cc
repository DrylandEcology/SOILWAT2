#include "include/SW_Defines.h"          // for SW_MISSING, deg_to_rad, ...
#include "tests/gtests/sw_testhelpers.h" // for missing
#include "gtest/gtest.h"                 // for AssertionResult, Message, Test
#include <cmath>                         // for exp, INFINITY, NAN, M_PI
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

TEST(SWDefines, SWDefinesAngleConversions) {
    int k;
    double const angles[] = {-180., -90., -45., 0., 30., 45., 90., 180., 360.};

    // Conversion constants are the doubles closest to pi / 180 and 180 / pi
    EXPECT_EQ(deg_to_rad, M_PI / 180.);
    EXPECT_EQ(rad_to_deg, 180. / M_PI);

    for (k = 0; k < 9; k++) {
        // Round trip degrees -> radians -> degrees (within 4 ulps)
        EXPECT_DOUBLE_EQ(angles[k] * deg_to_rad * rad_to_deg, angles[k])
            << "angle = " << angles[k];

        // Same result as conversion with (exact) pi
        EXPECT_EQ(angles[k] * deg_to_rad, angles[k] * (M_PI / 180.))
            << "angle = " << angles[k];
    }

    EXPECT_EQ(90. * deg_to_rad, swPI_half);
    EXPECT_EQ(180. * deg_to_rad, swPI);
}
} // namespace
