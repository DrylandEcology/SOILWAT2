#include "include/generic.h"           // for LOGERROR
#include "include/SW_Defines.h"        // for MAX_LAYERS, LyrIndex
#include "include/SW_DerivedMetrics.h" // for metric_DDD, metric_WDD, metri...
#include "include/SW_Site.h"           // for sw_vanGenuchten1980
#include "include/SW_SoilWater.h"      // for SWRC_SWPtoSWC
#include "gtest/gtest.h"               // for Message, Test, CmpHelperFloat...

// NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables)
extern double (*test_soilLayerWeightRange)(double, double, double, double);

// NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables)

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

TEST(SWDerivedMetrics, WetDay) {
    SW_SOIL_RUN_INPUTS soilRunIn{};
    SW_SITE_SIM siteSim{};
    LOG_INFO logInfo{};
    double swcBulk[MAX_LAYERS]{};
    double layerWeights[MAX_LAYERS]{};
    LyrIndex const n_layers = 2;

    // Weights of soil layers for 0-100 cm depth (as for `slWeight100`)
    auto setLayerWeights = [&]() {
        for (LyrIndex lyr = 0; lyr < n_layers; ++lyr) {
            layerWeights[lyr] = test_soilLayerWeightRange(
                0.,
                100.,
                (lyr > 0) ? soilRunIn.depths[lyr - 1] : 0.,
                soilRunIn.depths[lyr]
            );
        }
    };

    for (LyrIndex lyr = 0; lyr < n_layers; ++lyr) {
        soilRunIn.width[lyr] = (lyr == 0) ? 40. : 80.;
        soilRunIn.depths[lyr] = soilRunIn.width[lyr] +
                                ((lyr == 0) ? 0. : soilRunIn.depths[lyr - 1]);
        siteSim.swrc_type[lyr] = sw_vanGenuchten1980;
        siteSim.swrcp[lyr][0] = 0.1;
        siteSim.swrcp[lyr][1] = 0.5;
        siteSim.swrcp[lyr][2] = 0.01;
        siteSim.swrcp[lyr][3] = 1.5;
    }
    setLayerWeights();

    // Soil water at the 15-bar threshold counts as wet.
    swcBulk[0] = SWRC_SWPtoSWC(
        15.,
        siteSim.swrc_type[0],
        siteSim.swrcp[0],
        soilRunIn.fractionVolBulk_gravel[0],
        soilRunIn.width[0],
        LOGERROR,
        &logInfo
    );
    EXPECT_DOUBLE_EQ(
        metric_WetDay(
            &soilRunIn, &siteSim, swcBulk, layerWeights, n_layers, &logInfo
        ),
        1.
    );

    // Soil water drier than 15 bars does not count as wet.
    for (LyrIndex lyr = 0; lyr < n_layers; ++lyr) {
        swcBulk[lyr] = SWRC_SWPtoSWC(
            20.,
            siteSim.swrc_type[lyr],
            siteSim.swrcp[lyr],
            soilRunIn.fractionVolBulk_gravel[lyr],
            soilRunIn.width[lyr],
            LOGERROR,
            &logInfo
        );
    }
    EXPECT_DOUBLE_EQ(
        metric_WetDay(
            &soilRunIn, &siteSim, swcBulk, layerWeights, n_layers, &logInfo
        ),
        0.
    );

    // A layer extending below 100 cm counts if it overlaps the target.
    swcBulk[1] = SWRC_SWPtoSWC(
        15.,
        siteSim.swrc_type[1],
        siteSim.swrcp[1],
        soilRunIn.fractionVolBulk_gravel[1],
        soilRunIn.width[1],
        LOGERROR,
        &logInfo
    );
    EXPECT_DOUBLE_EQ(
        metric_WetDay(
            &soilRunIn, &siteSim, swcBulk, layerWeights, n_layers, &logInfo
        ),
        1.
    );

    // A layer beginning at 100 cm is outside the target depth.
    soilRunIn.width[0] = 100.;
    soilRunIn.depths[0] = 100.;
    soilRunIn.width[1] = 40.;
    soilRunIn.depths[1] = 140.;
    setLayerWeights();
    swcBulk[0] = SWRC_SWPtoSWC(
        20.,
        siteSim.swrc_type[0],
        siteSim.swrcp[0],
        soilRunIn.fractionVolBulk_gravel[0],
        soilRunIn.width[0],
        LOGERROR,
        &logInfo
    );
    swcBulk[1] = SWRC_SWPtoSWC(
        15.,
        siteSim.swrc_type[1],
        siteSim.swrcp[1],
        soilRunIn.fractionVolBulk_gravel[1],
        soilRunIn.width[1],
        LOGERROR,
        &logInfo
    );
    EXPECT_DOUBLE_EQ(
        metric_WetDay(
            &soilRunIn, &siteSim, swcBulk, layerWeights, n_layers, &logInfo
        ),
        0.
    );
}

} // namespace
