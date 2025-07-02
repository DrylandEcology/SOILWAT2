#include "include/SW_Defines.h"         // for MAX_LAYERS, LyrIndex
#include "include/SW_DerivedMetrics.h"  // for metric_DDD, metric_WDD, metri...
#include "gtest/gtest.h"                // for Message, Test, CmpHelperFloat...


namespace {
TEST(SWDerivedMetrics, CWD) {
    double const x1 = 1.5;
    double const x2 = 0.33;

    // Expect that CWD calculates the difference
    EXPECT_DOUBLE_EQ(metric_CWD(x1, x2), x1 - x2);
}

TEST(SWDerivedMetrics, TotalSWA) {
    double swcBulk[MAX_LAYERS] = {1.};
    double baseSWC[MAX_LAYERS];
    double layerWeights[MAX_LAYERS];
    LyrIndex const n_layers = 1;
    double totalSWC;

    // Expect that 0 <= TotalSWA <= sum(swcBulk)
    baseSWC[0] = swcBulk[0] / 4;
    layerWeights[0] = 1.;
    totalSWC = swcBulk[0];

    EXPECT_GE(metric_totalSWA(swcBulk, baseSWC, layerWeights, n_layers), 0.);
    EXPECT_LE(
        metric_totalSWA(swcBulk, baseSWC, layerWeights, n_layers), totalSWC
    );

    // Expect that TotalSWC == 0 if baseSWC > swcBulk
    baseSWC[0] = swcBulk[0] * 4;

    EXPECT_DOUBLE_EQ(
        metric_totalSWA(swcBulk, baseSWC, layerWeights, n_layers), 0.
    );

    // Expect that TotalSWC == 0 if layerWeights == 0
    baseSWC[0] = swcBulk[0] / 4;
    layerWeights[0] = 0.;

    EXPECT_DOUBLE_EQ(
        metric_totalSWA(swcBulk, baseSWC, layerWeights, n_layers), 0.
    );
}

TEST(SWDerivedMetrics, DDD) {
    double const tmean = 25.;
    double baseTmean;
    double swe;
    double const baseSWE = 0.;
    double swcBulk[MAX_LAYERS];
    double baseSWC[MAX_LAYERS] = {0.25};
    double layerWeights[MAX_LAYERS]{1.};
    LyrIndex const n_layers = 1;
    double gdd;

    // Expect that 0 <= ddd <= (total/growing) degree-days
    baseTmean = 5.;
    swe = 0.;
    swcBulk[0] = baseSWC[0] / 2.;
    gdd = tmean - baseTmean;

    EXPECT_GE(
        metric_DDD(
            tmean,
            baseTmean,
            swe,
            baseSWE,
            swcBulk,
            baseSWC,
            layerWeights,
            n_layers
        ),
        0.
    );
    EXPECT_LE(
        metric_DDD(
            tmean,
            baseTmean,
            swe,
            baseSWE,
            swcBulk,
            baseSWC,
            layerWeights,
            n_layers
        ),
        gdd
    );

    // Expect that ddd == 0 if tmean < baseTmean
    baseTmean = tmean + 1.;

    EXPECT_DOUBLE_EQ(
        metric_DDD(
            tmean,
            baseTmean,
            swe,
            baseSWE,
            swcBulk,
            baseSWC,
            layerWeights,
            n_layers
        ),
        0.
    );

    // Expect that ddd == 0 if swe > baseSWE
    baseTmean = 5.;
    swe = baseSWE + 1.;

    EXPECT_DOUBLE_EQ(
        metric_DDD(
            tmean,
            baseTmean,
            swe,
            baseSWE,
            swcBulk,
            baseSWC,
            layerWeights,
            n_layers
        ),
        0.
    );

    // Expect that ddd == 0 if swa > 0
    baseTmean = 5.;
    swe = 0.;
    swcBulk[0] = baseSWC[0] * 2.;

    EXPECT_DOUBLE_EQ(
        metric_DDD(
            tmean,
            baseTmean,
            swe,
            baseSWE,
            swcBulk,
            baseSWC,
            layerWeights,
            n_layers
        ),
        0.
    );
}

TEST(SWDerivedMetrics, WDD) {
    double const tmean = 25.;
    double baseTmean;
    double swe;
    double const baseSWE = 0.;
    double swcBulk[MAX_LAYERS];
    double baseSWC[MAX_LAYERS] = {0.25};
    double layerWeights[MAX_LAYERS]{1.};
    LyrIndex const n_layers = 1;
    double gdd;

    // Expect that 0 <= wdd <= (total/growing) degree-days
    baseTmean = 5.;
    swe = 0.;
    swcBulk[0] = baseSWC[0] * 2.;
    gdd = tmean - baseTmean;

    EXPECT_GE(
        metric_WDD(
            tmean,
            baseTmean,
            swe,
            baseSWE,
            swcBulk,
            baseSWC,
            layerWeights,
            n_layers
        ),
        0.
    );
    EXPECT_LE(
        metric_WDD(
            tmean,
            baseTmean,
            swe,
            baseSWE,
            swcBulk,
            baseSWC,
            layerWeights,
            n_layers
        ),
        gdd
    );

    // Expect that wdd == 0 if tmean < baseTmean
    baseTmean = tmean + 1.;

    EXPECT_DOUBLE_EQ(
        metric_WDD(
            tmean,
            baseTmean,
            swe,
            baseSWE,
            swcBulk,
            baseSWC,
            layerWeights,
            n_layers
        ),
        0.
    );

    // Expect that wdd == 0 if swe > baseSWE
    baseTmean = 5.;
    swe = baseSWE + 1.;

    EXPECT_DOUBLE_EQ(
        metric_WDD(
            tmean,
            baseTmean,
            swe,
            baseSWE,
            swcBulk,
            baseSWC,
            layerWeights,
            n_layers
        ),
        0.
    );

    // Expect that wdd == 0 if swa == 0
    baseTmean = 5.;
    swe = 0.;
    swcBulk[0] = baseSWC[0] / 2.;

    EXPECT_DOUBLE_EQ(
        metric_WDD(
            tmean,
            baseTmean,
            swe,
            baseSWE,
            swcBulk,
            baseSWC,
            layerWeights,
            n_layers
        ),
        0.
    );
}

} // namespace
