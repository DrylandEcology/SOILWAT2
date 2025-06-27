#include "include/generic.h"             // for swFALSE, Bool, GT, fmax
#include "include/rands.h"               // for RandNorm, RandSeed, RandUni...
#include "include/SW_datastructs.h"      // for LOG_INFO, SW_ST_SIM
#include "include/SW_Defines.h"          // for MAX_LAYERS, sw_random_t
#include "include/SW_Flow_lib.h"         // for surface_temperature_under_snow
#include "include/SW_Main_lib.h"         // for sw_fail_on_error, sw_init_logs
#include "tests/gtests/sw_testhelpers.h" // for length, missing
#include "gmock/gmock.h"                 // for HasSubstr, MakePredicateFor...
#include "gtest/gtest.h"                 // for Test, EXPECT_EQ, CmpHelperGT
#include <math.h>                        // for fmax, ceil, fminf
#include <stdio.h>                       // for NULL


using ::testing::HasSubstr;

namespace {
// Test the function 'surface_temperature_under_snow'
TEST(SWFlowTempTest, SWFlowTempSurfaceTemperatureUnderSnow) {

    // declare inputs and output
    double snow;
    double airTempAvg;
    double tSoilAvg;

    // test when snow is 0 and airTempAvg > 0
    snow = 0, airTempAvg = 10;

    tSoilAvg = surface_temperature_under_snow(airTempAvg, snow);

    // When there is snow, the return is 0
    EXPECT_EQ(0, tSoilAvg);


    // test when snow is > 0 and airTempAvg is >= 0
    snow = 1, airTempAvg = 0;

    tSoilAvg = surface_temperature_under_snow(airTempAvg, snow);

    // When there is snow and meanTempAir >= 0, the return is -2.0
    EXPECT_EQ(-2.0, tSoilAvg);

    // test when snow > 0 and airTempAvg < 0
    snow = 1, airTempAvg = -10;

    tSoilAvg = surface_temperature_under_snow(airTempAvg, snow);

    // When there snow == 1 airTempAvg = -10
    EXPECT_EQ(-4.55, tSoilAvg);

    //
    snow = 6.7, airTempAvg = 0;

    tSoilAvg = surface_temperature_under_snow(airTempAvg, snow);

    // When there is snow > 6.665 and meanTempAir >= 0, the return is -2.0
    EXPECT_EQ(-2.0, tSoilAvg);
}

TEST(SWFlowTempTest, SWFlowTempSurfaceTemperature) {
    // Initialize logs and silence warn/error reporting
    LOG_INFO LogInfo;
    sw_init_logs(NULL, &LogInfo);

    unsigned int km;

    /* Output to check */
    double TempSurface1[3];
    double TempSurface2[3];

    /* Inputs to vary */
    unsigned int methodSurfaceTemperature;
    double biomass;
    double minTempAir;
    double meanTempAir;
    double maxTempAir;
    double H_gt;

    /* Inputs held constant for this set of tests */
    double const pet = 5.0;
    double const aet = 4.0;
    double const snow = 0;

    /* Parameters */
    double const bmLimiter = 300.;
    double const t1Param1 = 15.;
    double const t1Param2 = -4.;
    double const t1Param3 = 600.;


    /* Set variable inputs to reasonable values */
    minTempAir = 0.;
    meanTempAir = 5.;
    maxTempAir = 10.;
    H_gt = 100.;


    /* Expect that output does not change if biomass > cap = 1146 */
    methodSurfaceTemperature = 1;

    biomass = 1200.;
    surface_temperature(
        &TempSurface1[0],
        &TempSurface1[1],
        &TempSurface1[2],
        methodSurfaceTemperature,
        snow,
        minTempAir,
        meanTempAir,
        maxTempAir,
        H_gt,
        pet,
        aet,
        biomass,
        bmLimiter,
        t1Param1,
        t1Param2,
        t1Param3,
        &LogInfo
    );
    sw_fail_on_error(&LogInfo); // exit test program if unexpected error
    EXPECT_EQ(LogInfo.numWarnings, 0) << LogInfo.warningMsgs[0];

    biomass = 2000.;
    surface_temperature(
        &TempSurface2[0],
        &TempSurface2[1],
        &TempSurface2[2],
        methodSurfaceTemperature,
        snow,
        minTempAir,
        meanTempAir,
        maxTempAir,
        H_gt,
        pet,
        aet,
        biomass,
        bmLimiter,
        t1Param1,
        t1Param2,
        t1Param3,
        &LogInfo
    );
    sw_fail_on_error(&LogInfo); // exit test program if unexpected error
    EXPECT_EQ(LogInfo.numWarnings, 0) << LogInfo.warningMsgs[0];

    EXPECT_DOUBLE_EQ(TempSurface1[0], TempSurface2[0]);
    EXPECT_DOUBLE_EQ(TempSurface1[1], TempSurface2[1]);
    EXPECT_DOUBLE_EQ(TempSurface1[2], TempSurface2[2]);


    /* Expect warning minTempSurface > maxTempSurface if low air temp range */
    biomass = 500.;
    minTempAir = 4.;
    meanTempAir = 5.;
    maxTempAir = 6.;

    for (km = 0; km <= 1; km++) {
        methodSurfaceTemperature = km;
        surface_temperature(
            &TempSurface1[0],
            &TempSurface1[1],
            &TempSurface1[2],
            methodSurfaceTemperature,
            snow,
            minTempAir,
            meanTempAir,
            maxTempAir,
            H_gt,
            pet,
            aet,
            biomass,
            bmLimiter,
            t1Param1,
            t1Param2,
            t1Param3,
            &LogInfo
        );
        sw_fail_on_error(&LogInfo); // exit test program if unexpected error

        EXPECT_THAT(
            LogInfo.warningMsgs[0], HasSubstr("minTempSurface > maxTempSurface")
        );
        sw_init_logs(NULL, &LogInfo);
    }


    /* Expect warning meanTempSurface outside min-max range for method 0 */
    methodSurfaceTemperature = 0;
    biomass = 500.;
    minTempAir = 3.;
    meanTempAir = 5.;
    maxTempAir = 7.;

    surface_temperature(
        &TempSurface1[0],
        &TempSurface1[1],
        &TempSurface1[2],
        methodSurfaceTemperature,
        snow,
        minTempAir,
        meanTempAir,
        maxTempAir,
        H_gt,
        pet,
        aet,
        biomass,
        bmLimiter,
        t1Param1,
        t1Param2,
        t1Param3,
        &LogInfo
    );
    sw_fail_on_error(&LogInfo); // exit test program if unexpected error

    EXPECT_THAT(LogInfo.warningMsgs[0], HasSubstr("outside min-max range"));
    sw_init_logs(NULL, &LogInfo);
}

// Test the soil temperature initialization function 'soil_temperature_setup'
TEST(SWFlowTempTest, SWFlowTempSoilTemperatureInit) {
    SW_SOIL_RUN_INPUTS SW_SoilRunIn;
    SW_ST_SIM SW_StRegSimVals;
    SW_ST_init_run(&SW_StRegSimVals);

    LOG_INFO LogInfo;
    // Initialize logs and silence warn/error reporting
    sw_init_logs(NULL, &LogInfo);

    // declare inputs and output
    double const deltaX = 15.0;
    double const theMaxDepth = 990.0;
    double const sTconst = 4.15;
    double acc = 0.0;
    unsigned int nlyrs;
    unsigned int iStart;
    unsigned int const nRgr = 65;
    Bool ptr_stError = swFALSE;
    sw_random_t STInit_rng;
    RandSeed(0u, 0u, &STInit_rng);

    // *****  Test when nlyrs = 1  ***** //
    unsigned int i = 0;
    nlyrs = 1;
    const double width[] = {20};
    double sTempInit[] = {1};
    double bDensity[] = {RandNorm(1., 0.5, &STInit_rng)};
    double fc[] = {RandNorm(1.5, 0.5, &STInit_rng)};
    double wp[1];
    wp[0] = fc[0] - 0.6; // wp will always be less than fc

    SW_SoilRunIn.width[0] = SW_SoilRunIn.depths[0] = width[0];
    /// test standard conditions
    soil_temperature_setup(
        &SW_StRegSimVals,
        bDensity,
        SW_SoilRunIn.width,
        sTempInit,
        sTconst,
        nlyrs,
        fc,
        wp,
        deltaX,
        theMaxDepth,
        nRgr,
        SW_SoilRunIn.depths,
        &ptr_stError,
        &SW_StRegSimVals.soil_temp_init,
        &LogInfo
    );
    sw_fail_on_error(&LogInfo); // exit test program if unexpected error
    EXPECT_FALSE(ptr_stError) << LogInfo.warningMsgs[0];

    // Structure Tests
    EXPECT_EQ(
        sizeof(SW_StRegSimVals.tlyrs_by_slyrs),
        sizeof(double) * MAX_ST_RGR * (MAX_LAYERS + 1)
    );

    iStart = (unsigned int) ceil(SW_SoilRunIn.depths[nlyrs - 1] / deltaX);
    for (i = iStart; i < nRgr + 1; i++) {
        EXPECT_EQ(SW_StRegSimVals.tlyrs_by_slyrs[i][nlyrs], -deltaX);
        // Values should be equal to -deltaX when i > the depth of the soil
        // profile/deltaX and j is == nlyrs
    }

    // Other init test
    // sum of inputs width = maximum depth; in my example 20
    EXPECT_EQ(SW_SoilRunIn.depths[nlyrs - 1], 20);

    // nRgr = (MaxDepth/deltaX) - 1
    EXPECT_EQ((SW_StRegSimVals.depthsR[nRgr] / deltaX) - 1, nRgr);

    // *****  Test when nlyrs = MAX_LAYERS (SW_Defines.h)  ***** //
    /// generate inputs using a for loop
    nlyrs = MAX_LAYERS;
    const double width2[] = {5,  5,  5,  10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                             10, 10, 10, 10, 10, 10, 20, 20, 20, 20, 20, 20};
    double sTempInit2[] = {1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 3, 3,
                           3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4};
    double *bDensity2 = new double[nlyrs];
    double *fc2 = new double[nlyrs];
    double *wp2 = new double[nlyrs];

    for (i = 0; i < nlyrs; i++) {
        bDensity2[i] = RandNorm(1., 0.5, &STInit_rng);
        fc2[i] = RandNorm(1.5, 0.5, &STInit_rng);
        wp2[i] = fc2[i] - 0.6; // wp will always be less than fc
    }

    for (i = 0; i < nlyrs; i++) {
        SW_SoilRunIn.width[i] = width2[i];
        acc += width2[i];
        SW_SoilRunIn.depths[i] = acc;
    }

    soil_temperature_setup(
        &SW_StRegSimVals,
        bDensity2,
        SW_SoilRunIn.width,
        sTempInit2,
        sTconst,
        nlyrs,
        fc2,
        wp2,
        deltaX,
        theMaxDepth,
        nRgr,
        SW_SoilRunIn.depths,
        &ptr_stError,
        &SW_StRegSimVals.soil_temp_init,
        &LogInfo
    );
    sw_fail_on_error(&LogInfo); // exit test program if unexpected error
    EXPECT_FALSE(ptr_stError) << LogInfo.warningMsgs[0];

    // Structure Tests
    EXPECT_EQ(
        sizeof(SW_StRegSimVals.tlyrs_by_slyrs),
        sizeof(double) * MAX_ST_RGR * (MAX_LAYERS + 1)
    );

    iStart = (unsigned int) ceil(SW_SoilRunIn.depths[nlyrs - 1] / deltaX);
    for (i = iStart; i < nRgr + 1; i++) {
        EXPECT_EQ(SW_StRegSimVals.tlyrs_by_slyrs[i][nlyrs], -deltaX);
        // Values should be equal to -deltaX when i > the depth of the soil
        // profile/deltaX and j is == nlyrs
    }

    // Other init test
    // sum of inputs width = maximum depth; in my example 295
    EXPECT_EQ(SW_SoilRunIn.depths[nlyrs - 1], 295);

    // nRgr = (MaxDepth/deltaX) - 1
    EXPECT_EQ((SW_StRegSimVals.depthsR[nRgr] / deltaX) - 1, nRgr);

    delete[] bDensity2;
    delete[] fc2;
    delete[] wp2;
}

// Death tests for soil_temperature_setup function
TEST(SWFlowTempTest, SWFlowTempSoilTemperatureInitDeathTest) {
    SW_ST_SIM SW_StRegSimVals;
    SW_SOIL_RUN_INPUTS SW_SoilRunIn;
    SW_ST_init_run(&SW_StRegSimVals);

    LOG_INFO LogInfo;
    // Initialize logs and silence warn/error reporting
    sw_init_logs(NULL, &LogInfo);

    // *****  Test when nlyrs = MAX_LAYERS (SW_Defines.h)  ***** //
    double const deltaX = 15.0;
    double const sTconst = 4.15;
    double acc = 0.0;
    unsigned int nlyrs;
    unsigned int const nRgr = 65;
    unsigned int i = 0;
    Bool ptr_stError = swFALSE;
    nlyrs = MAX_LAYERS;
    const double width2[] = {5,  5,  5,  10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                             10, 10, 10, 10, 10, 10, 20, 20, 20, 20, 20, 20};
    double sTempInit2[] = {1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 3, 3,
                           3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4};
    double *bDensity2 = new double[nlyrs];
    double *fc2 = new double[nlyrs];
    double *wp2 = new double[nlyrs];
    sw_random_t STInitDeath_rng;
    RandSeed(0u, 0u, &STInitDeath_rng);

    for (i = 0; i < nlyrs; i++) {
        bDensity2[i] = RandNorm(1., 0.5, &STInitDeath_rng);
        fc2[i] = RandNorm(1.5, 0.5, &STInitDeath_rng);
        wp2[i] = fc2[i] - 0.6; // wp will always be less than fc
        SW_SoilRunIn.width[i] = width2[i];
        acc += width2[i];
        SW_SoilRunIn.depths[i] = acc;
    }

    /// test when theMaxDepth is less than soil layer depth - function should
    /// fail
    double const theMaxDepth2 = 70.0;

    // We expect an error when max depth < last layer
    soil_temperature_setup(
        &SW_StRegSimVals,
        bDensity2,
        SW_SoilRunIn.width,
        sTempInit2,
        sTconst,
        nlyrs,
        fc2,
        wp2,
        deltaX,
        theMaxDepth2,
        nRgr,
        SW_SoilRunIn.depths,
        &ptr_stError,
        &SW_StRegSimVals.soil_temp_init,
        &LogInfo
    );
    // expect error: don't exit test program via `sw_fail_on_error(&LogInfo)`
    EXPECT_TRUE(ptr_stError) << LogInfo.warningMsgs[0];

    // Detect failure by error message
    EXPECT_THAT(
        LogInfo.errorMsg,
        HasSubstr("SOIL_TEMP FUNCTION ERROR: soil temperature max depth")
    );

    delete[] wp2;
    delete[] fc2;
    delete[] bDensity2;
}

// Test lyrSoil_to_lyrTemp, lyrSoil_to_lyrTemp_temperature via
// soil_temperature_setup function
TEST(SWFlowTempTest, SWFlowTempSoilLayerInterpolationFunctions) {
    SW_SOIL_RUN_INPUTS SW_SoilRunIn;
    SW_ST_SIM SW_StRegSimVals;
    SW_ST_init_run(&SW_StRegSimVals);

    LOG_INFO LogInfo;
    // Initialize logs and silence warn/error reporting
    sw_init_logs(NULL, &LogInfo);

    // declare inputs and output
    double const deltaX = 15.0;
    double const theMaxDepth = 990.0;
    double const sTconst = 4.15;
    double acc = 0.0;
    unsigned int nlyrs;
    unsigned int iStart;
    unsigned int const nRgr = 65;
    Bool ptr_stError = swFALSE;

    sw_random_t SLIF_rng;
    RandSeed(0u, 0u, &SLIF_rng);

    // *****  Test when nlyrs = 1  ***** //
    unsigned int i = 0;
    nlyrs = 1;
    double tmp;
    const double width[] = {20};
    double sTempInit[] = {1};
    tmp = RandNorm(1.5, 0.5, &SLIF_rng);
    double bDensity[] = {fmax(tmp, 0.1)};
    tmp = RandNorm(1.5, 0.5, &SLIF_rng);
    double fc[] = {fmax(tmp, 0.1)};
    double wp[1];

    wp[0] = fmax(fc[0] - 0.6, .1); // wp will always be less than fc

    SW_SoilRunIn.width[0] = SW_SoilRunIn.depths[0] = width[0];
    soil_temperature_setup(
        &SW_StRegSimVals,
        bDensity,
        SW_SoilRunIn.width,
        sTempInit,
        sTconst,
        nlyrs,
        fc,
        wp,
        deltaX,
        theMaxDepth,
        nRgr,
        SW_SoilRunIn.depths,
        &ptr_stError,
        &SW_StRegSimVals.soil_temp_init,
        &LogInfo
    );
    sw_fail_on_error(&LogInfo); // exit test program if unexpected error
    EXPECT_FALSE(ptr_stError) << LogInfo.warningMsgs[0];

    // lyrSoil_to_lyrTemp tests: This function is used in soil_temperature_setup
    // to transfer the soil layer values of bdensity, fc, and wp, to the
    // "temperature layer" which are contained in bdensityR, fcR, and wpR. Thus
    // we check these values.
    for (i = 0; i < nRgr + 1; i++) {
        // all Values should be greater than 0
        EXPECT_GT(SW_StRegSimVals.bDensityR[i], 0);
        EXPECT_GT(SW_StRegSimVals.fcR[i], 0);
        EXPECT_GT(SW_StRegSimVals.wpR[i], 0);
    }

    iStart = (unsigned int) ceil(SW_SoilRunIn.depths[nlyrs - 1] / deltaX);
    for (i = iStart; i < nRgr + 1; i++) {
        // The TempLayer values that are at depths greater than the max
        // SoilLayer depth should be uniform
        EXPECT_EQ(
            SW_StRegSimVals.bDensityR[i], SW_StRegSimVals.bDensityR[i - 1]
        );
        EXPECT_EQ(SW_StRegSimVals.fcR[i], SW_StRegSimVals.fcR[i - 1]);
        EXPECT_EQ(SW_StRegSimVals.wpR[i], SW_StRegSimVals.wpR[i - 1]);
    }

    // lyrSoil_to_lyrTemp_temperature tests
    // surface temperature is initialized to missing because not used
    EXPECT_TRUE(missing(SW_StRegSimVals.oldavgLyrTempR[0]));

    double maxvalR = 0.;
    for (i = 1; i < nRgr + 1; i++) {
        // Values interpolated into sTempInitR should be realistic
        EXPECT_GT(SW_StRegSimVals.oldavgLyrTempR[i], -100);

        // Values interpolated into sTempInitR should be realistic
        EXPECT_LT(SW_StRegSimVals.oldavgLyrTempR[i], 100);

        if (GT(SW_StRegSimVals.oldavgLyrTempR[i], maxvalR)) {
            maxvalR = SW_StRegSimVals.oldavgLyrTempR[i];
        }
    }

    // Maximum interpolated sTempInitR value should be less than or equal to
    // maximum in sTempInit2 (sTconst = last layer)
    EXPECT_LE(maxvalR, sTconst);

    // Temperature in last interpolated layer should equal sTconst
    EXPECT_EQ(SW_StRegSimVals.oldavgLyrTempR[nRgr + 1], sTconst);

    // *****  Test when nlyrs = MAX_LAYERS (SW_Defines.h)  ***** //
    /// generate inputs using a for loop
    nlyrs = MAX_LAYERS;
    const double width2[] = {5,  5,  5,  10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                             10, 10, 10, 10, 10, 10, 20, 20, 20, 20, 20, 20};
    double sTempInit2[] = {1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 3, 3,
                           3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4};
    double *bDensity2 = new double[nlyrs];
    double *fc2 = new double[nlyrs];
    double *wp2 = new double[nlyrs];

    for (i = 0; i < nlyrs; i++) {
        // note: fmax() may be our macro that evaluates arguments twice
        tmp = RandNorm(1., 0.5, &SLIF_rng);
        bDensity2[i] = fmax(tmp, 0.1);
        tmp = RandNorm(1.5, 0.5, &SLIF_rng);
        fc2[i] = fmax(tmp, 0.1);
        wp2[i] = fmax(fc2[i] - 0.6, 0.1); // wp will always be less than fc
        EXPECT_GT(bDensity2[i], 0);
        EXPECT_GT(fc2[i], 0);
        EXPECT_GT(wp2[i], 0);
    }

    acc = 0.0;
    for (i = 0; i < nlyrs; i++) {
        SW_SoilRunIn.width[i] = width2[i];
        acc += width2[i];
        SW_SoilRunIn.depths[i] = acc;
    }

    soil_temperature_setup(
        &SW_StRegSimVals,
        bDensity2,
        SW_SoilRunIn.width,
        sTempInit2,
        sTconst,
        nlyrs,
        fc2,
        wp2,
        deltaX,
        theMaxDepth,
        nRgr,
        SW_SoilRunIn.depths,
        &ptr_stError,
        &SW_StRegSimVals.soil_temp_init,
        &LogInfo
    );
    sw_fail_on_error(&LogInfo); // exit test program if unexpected error
    EXPECT_FALSE(ptr_stError) << LogInfo.warningMsgs[0];

    // lyrSoil_to_lyrTemp tests
    for (i = 0; i < nRgr + 1; i++) {
        // all Values should be greater than 0
        EXPECT_GT(SW_StRegSimVals.bDensityR[i], 0);
        EXPECT_GT(SW_StRegSimVals.fcR[i], 0);
        EXPECT_GT(SW_StRegSimVals.wpR[i], 0);
    }

    iStart = (unsigned int) ceil(SW_SoilRunIn.depths[nlyrs - 1] / deltaX);
    for (i = iStart; i < nRgr + 1; i++) {
        // The TempLayer values that are at depths greater than the max
        // SoilLayer depth should be uniform
        EXPECT_EQ(
            SW_StRegSimVals.bDensityR[i], SW_StRegSimVals.bDensityR[i - 1]
        );
        EXPECT_EQ(SW_StRegSimVals.fcR[i], SW_StRegSimVals.fcR[i - 1]);
        EXPECT_EQ(SW_StRegSimVals.wpR[i], SW_StRegSimVals.wpR[i - 1]);
    }

    // lyrSoil_to_lyrTemp_temperature tests
    EXPECT_TRUE(missing(SW_StRegSimVals.oldavgLyrTempR[0])
    ); // surface temperature is initialized to missing because not used
    maxvalR = 0.;
    for (i = 1; i <= nRgr + 1; i++) {
        // Values interpolated into sTempInitR should be realistic
        EXPECT_GT(SW_StRegSimVals.oldavgLyrTempR[i], -200);

        // Values interpolated into sTempInitR should be realistic
        EXPECT_LT(SW_StRegSimVals.oldavgLyrTempR[i], 200);

        if (GT(SW_StRegSimVals.oldavgLyrTempR[i], maxvalR)) {
            maxvalR = SW_StRegSimVals.oldavgLyrTempR[i];
        }
    }

    // Maximum interpolated sTempInitR value should be less than or equal to
    // maximum in sTempInit2 (sTconst = last layer)
    EXPECT_LE(maxvalR, sTconst);

    // Temperature in last interpolated layer should equal sTconst
    EXPECT_EQ(SW_StRegSimVals.oldavgLyrTempR[nRgr + 1], sTconst);

    delete[] bDensity2;
    delete[] fc2;
    delete[] wp2;
}

// Test set layer to frozen or unfrozen 'set_frozen_unfrozen'
TEST(SWFlowTempTest, SWFlowTempSetFrozenUnfrozen) {
    double lyrFrozen[MAX_LAYERS] = {0};

    // declare inputs and output
    // *****  Test when nlyrs = 1  ***** //
    /// ***** Test that soil freezes ***** ///
    unsigned int nlyrs = 1;
    double meanTempSoil[] = {-5};
    double swc[] = {1.5};
    double swc_sat[] = {1.8};
    const double width[] = {5};

    set_frozen_unfrozen(nlyrs, meanTempSoil, swc, swc_sat, width, lyrFrozen);


    // Soil should freeze when
    // meanTempSoil is <= -1 AND swc is > swc_sat - width * .13
    EXPECT_EQ(1, lyrFrozen[0]);

    /// ***** Test that soil does not freeze ***** ///
    double sTemp2[] = {0};

    set_frozen_unfrozen(nlyrs, sTemp2, swc, swc_sat, width, lyrFrozen);

    // Soil should NOT freeze when meanTempSoil is > -1
    EXPECT_EQ(0, lyrFrozen[0]);

    // *****  Test when nlyrs = MAX_LAYERS (SW_Defines.h)  ***** //
    nlyrs = MAX_LAYERS;
    const double width2[] = {5,  5,  5,  10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                             10, 10, 10, 10, 10, 10, 20, 20, 20, 20, 20, 20};
    double *sTemp3 = new double[nlyrs];
    double *sTemp4 = new double[nlyrs];
    double *swc2 = new double[nlyrs];
    double *swc_sat2 = new double[nlyrs];

    unsigned int i = 0;
    for (i = 0; i < nlyrs; i++) {
        sTemp3[i] = -5;
        sTemp4[i] = 0;
        // set swc to a high value so will be > swc_sat - width * .13
        swc2[i] = 5;
        swc_sat2[i] = 1;
        // run
        set_frozen_unfrozen(nlyrs, sTemp3, swc2, swc_sat2, width2, lyrFrozen);
        // Test
        EXPECT_EQ(1, lyrFrozen[i]);
        // run
        set_frozen_unfrozen(nlyrs, sTemp4, swc2, swc_sat2, width2, lyrFrozen);
        // Test
        EXPECT_EQ(0, lyrFrozen[i]);
    }

    delete[] sTemp3;
    delete[] sTemp4;
    delete[] swc2;
    delete[] swc_sat2;
}

// Test soil temperature today function 'soil_temperature_today'
TEST(SWFlowTempTest, SWFlowTempSoilTemperatureTodayFunction) {

    // declare inputs and output
    double delta_time = 86400.;
    double const deltaX = 15.0;
    double const T1 = 20.0;
    double const sTconst = 4.16;
    double const csParam1 = 0.00070;
    double const csParam2 = 0.000030;
    double const shParam = 0.18;
    double const surface_range = 1.;
    unsigned int const nRgr = 65;
    unsigned int const year = 1980;
    unsigned int const doy = 1;
    Bool ptr_stError = swFALSE;

    sw_random_t STTF_rng;
    RandSeed(0u, 0u, &STTF_rng);

    // declare input in for loop for non-error causing conditions;
    /// don't use RandNorm for fcR, wpR, vwcR, and bDensityR because will
    /// trigger error causing condtions

    double *sTempR = new double[nRgr + 2];
    double *sTempInitR = new double[nRgr + 2];
    double *wpR = new double[nRgr + 2];
    double *fcR = new double[nRgr + 2];
    double *vwcR = new double[nRgr + 2];
    double *bDensityR = new double[nRgr + 2];
    double *temperatureRangeR = new double[nRgr + 2];
    double *depthsR = new double[nRgr + 2];
    unsigned int i = 0;
    for (i = 0; i <= nRgr + 1; i++) {
        sTempR[i] = RandNorm(1.5, 1, &STTF_rng);
        sTempInitR[i] = RandNorm(1.5, 1, &STTF_rng);
        fcR[i] = 2.1;
        wpR[i] = 1.5; // wp will always be less than fc
        vwcR[i] = 1.6;
        bDensityR[i] = 1.5;
    }

    soil_temperature_today(
        &delta_time,
        deltaX,
        T1,
        sTconst,
        nRgr,
        sTempR,
        sTempInitR,
        vwcR,
        wpR,
        fcR,
        bDensityR,
        csParam1,
        csParam2,
        shParam,
        &ptr_stError,
        surface_range,
        temperatureRangeR,
        depthsR,
        year,
        doy
    );
    EXPECT_FALSE(ptr_stError);

    // Check that values that are set, are set right.
    EXPECT_EQ(sTempR[0], T1);
    EXPECT_EQ(sTempR[nRgr + 1], sTconst);

    // Check that when  ptr_stError is FALSE, sTempR values are realisitic and
    // pass check in code (between -100 and 100)
    for (i = 0; i <= nRgr + 1; i++) {
        EXPECT_LT(sTempR[i], 100);
        EXPECT_GT(sTempR[i], -100);
    }

    // test that the ptr_stError is FALSE when it is supposed to
    double *sTempR2 = new double[nRgr + 2];
    double *sTempInitR3 = new double[nRgr + 2];

    for (i = 0; i <= nRgr + 1; i++) {
        sTempR2[i] = RandNorm(150, 1, &STTF_rng);
        sTempInitR3[i] = RandNorm(150, 1, &STTF_rng);
    }

    soil_temperature_today(
        &delta_time,
        deltaX,
        T1,
        sTconst,
        nRgr,
        sTempR2,
        sTempInitR3,
        vwcR,
        wpR,
        fcR,
        bDensityR,
        csParam1,
        csParam2,
        shParam,
        &ptr_stError,
        surface_range,
        temperatureRangeR,
        depthsR,
        year,
        doy
    );
    EXPECT_TRUE(ptr_stError);

    double *array_list[] = {
        sTempR2,
        sTempInitR3,
        sTempR,
        sTempInitR,
        wpR,
        fcR,
        vwcR,
        bDensityR,
        temperatureRangeR,
        depthsR
    };
    for (i = 0; i < sw_length(array_list); i++) {
        delete[] array_list[i];
    }
}

// Test main soil temperature function 'soil_temperature'
// AND lyrTemp_to_lyrSoil_temperature as this function
// is only called in the soil_temperature function
TEST(SWFlowTempTest, SWFlowTempMainSoilTemperatureFunction_Lyr01) {
    SW_ST_SIM SW_StRegSimVals;
    SW_ST_init_run(&SW_StRegSimVals);

    SW_SITE_INPUTS SW_SiteIn;
    SW_SITE_SIM SW_SiteSim;
    SW_SOIL_RUN_INPUTS SW_SoilRunIn;
    SW_SITE_RUN_INPUTS SW_SiteRunIn;

    LOG_INFO LogInfo;
    // Initialize logs and silence warn/error reporting
    sw_init_logs(NULL, &LogInfo);

    double lyrFrozen[MAX_LAYERS] = {0};

    unsigned int k;
    unsigned int const year = 1980;
    unsigned int const doy = 1;
    const unsigned int methodSurfaceTemperature = 0; // default prior to v8.1.0

    // *****  Test when nlyrs = 1  ***** //
    unsigned int const nlyrs = 1;
    unsigned int const nRgr = 65;
    double meanTempAir = 25.0;
    double const pet = 5.0;
    double const aet = 4.0;
    double biomass = 100.;
    double meanTempSurface = 15.;
    double const bmLimiter = 300.;
    double const t1Param1 = 15.;
    double const t1Param2 = -4.;
    double const t1Param3 = 600.;
    double const csParam1 = 0.00070;
    double const csParam2 = 0.00030;
    double const shParam = 0.18;
    double const sTconst = 4.15;
    double const deltaX = 15;
    double const theMaxDepth = 990.;
    double snow = 1;
    double const maxTempAir = 10.1;
    double const minTempAir = -5.0;
    double const H_gt = 300.0;
    double maxTempSurface = 10.6;
    double minTempSurface = -6.8;
    Bool ptr_stError = swFALSE;

    double swc[] = {1.0};
    double swc_sat[] = {1.5};
    double bDensity[] = {1.8};
    const double width[] = {20};
    double meanTempSoil[1];
    double minTempSoil[] = {10.0};
    double maxTempSoil[] = {1.0};

    SW_SiteRunIn.n_layers = nlyrs;
    SW_SiteSim.stNRGR = nRgr;

    SW_SiteSim.soilBulk_density[0] = 1.8;
    SW_SoilRunIn.width[0] = SW_SoilRunIn.depths[0] = width[0];
    SW_SoilRunIn.avgLyrTempInit[0] = 5.0;
    SW_SiteRunIn.Tsoil_constant = 4.15;
    SW_SiteSim.swcBulk_fieldcap[0] = 2.6;
    SW_SiteSim.swcBulk_wiltpt[0] = 1.0;
    SW_SiteIn.stDeltaX = 15;
    SW_SiteIn.stMaxDepth = 990.;

    SW_SiteSim.swcBulk_saturated[0] = 1.5;


    SW_ST_setup_run(
        &SW_StRegSimVals,
        &SW_SoilRunIn,
        &SW_SiteIn,
        &SW_SiteSim,
        SW_SiteRunIn.Tsoil_constant,
        &ptr_stError,
        &SW_StRegSimVals.soil_temp_init,
        meanTempAir,
        swc,
        SW_SiteRunIn.n_layers,
        &meanTempSurface,
        meanTempSoil,
        lyrFrozen,
        &LogInfo
    );
    sw_fail_on_error(&LogInfo); // exit test program if unexpected error
    EXPECT_FALSE(ptr_stError) << LogInfo.warningMsgs[0];

    soil_temperature(
        &SW_StRegSimVals,
        &minTempSurface,
        &meanTempSurface,
        &maxTempSurface,
        minTempSoil,
        meanTempSoil,
        maxTempSoil,
        lyrFrozen,
        methodSurfaceTemperature,
        snow,
        minTempAir,
        meanTempAir,
        maxTempAir,
        H_gt,
        pet,
        aet,
        biomass,
        swc,
        swc_sat,
        bDensity,
        SW_SoilRunIn.width,
        SW_SoilRunIn.depths,
        nlyrs,
        bmLimiter,
        t1Param1,
        t1Param2,
        t1Param3,
        csParam1,
        csParam2,
        shParam,
        sTconst,
        deltaX,
        theMaxDepth,
        nRgr,
        year,
        doy,
        &ptr_stError,
        &LogInfo
    );
    sw_fail_on_error(&LogInfo); // exit test program if unexpected error
    EXPECT_FALSE(ptr_stError) << LogInfo.warningMsgs[0];

    // Expect that surface temp equals surface_temperature_under_snow() because
    // snow > 0
    EXPECT_EQ(
        meanTempSurface, surface_temperature_under_snow(meanTempAir, snow)
    );
    EXPECT_NE(
        meanTempSurface,
        meanTempAir + t1Param2 * (biomass - bmLimiter) / t1Param3
    );
    EXPECT_NE(
        meanTempSurface,
        meanTempAir +
            (t1Param1 * pet * (1. - (aet / pet)) * (1. - (biomass / bmLimiter)))
    );

    // Test surface temp equals equation when biomass < blimititer & snow = 0
    snow = 0;

    for (k = 0; k < nlyrs; k++) {
        meanTempSoil[k] = SW_SoilRunIn.avgLyrTempInit[k];
    }

    soil_temperature(
        &SW_StRegSimVals,
        &minTempSurface,
        &meanTempSurface,
        &maxTempSurface,
        minTempSoil,
        meanTempSoil,
        maxTempSoil,
        lyrFrozen,
        methodSurfaceTemperature,
        snow,
        minTempAir,
        meanTempAir,
        maxTempAir,
        H_gt,
        pet,
        aet,
        biomass,
        swc,
        swc_sat,
        bDensity,
        SW_SoilRunIn.width,
        SW_SoilRunIn.depths,
        nlyrs,
        bmLimiter,
        t1Param1,
        t1Param2,
        t1Param3,
        csParam1,
        csParam2,
        shParam,
        sTconst,
        deltaX,
        theMaxDepth,
        nRgr,
        year,
        doy,
        &ptr_stError,
        &LogInfo
    );
    sw_fail_on_error(&LogInfo); // exit test program if unexpected error
    EXPECT_FALSE(ptr_stError) << LogInfo.warningMsgs[0];

    EXPECT_EQ(
        meanTempSurface,
        meanTempAir +
            (t1Param1 * pet * (1. - (aet / pet)) * (1. - (biomass / bmLimiter)))
    );
    EXPECT_NE(
        meanTempSurface,
        meanTempAir + t1Param2 * (biomass - bmLimiter) / t1Param3
    );
    EXPECT_NE(
        meanTempSurface, surface_temperature_under_snow(meanTempAir, snow)
    );

    // Test surface temp equals equation when biomass > blimititer & snow = 0
    biomass = 305;

    for (k = 0; k < nlyrs; k++) {
        meanTempSoil[k] = SW_SoilRunIn.avgLyrTempInit[k];
    }

    soil_temperature(
        &SW_StRegSimVals,
        &minTempSurface,
        &meanTempSurface,
        &maxTempSurface,
        minTempSoil,
        meanTempSoil,
        maxTempSoil,
        lyrFrozen,
        methodSurfaceTemperature,
        snow,
        minTempAir,
        meanTempAir,
        maxTempAir,
        H_gt,
        pet,
        aet,
        biomass,
        swc,
        swc_sat,
        bDensity,
        SW_SoilRunIn.width,
        SW_SoilRunIn.depths,
        nlyrs,
        bmLimiter,
        t1Param1,
        t1Param2,
        t1Param3,
        csParam1,
        csParam2,
        shParam,
        sTconst,
        deltaX,
        theMaxDepth,
        nRgr,
        year,
        doy,
        &ptr_stError,
        &LogInfo
    );
    sw_fail_on_error(&LogInfo); // exit test program if unexpected error
    EXPECT_FALSE(ptr_stError) << LogInfo.warningMsgs[0];

    EXPECT_EQ(
        meanTempSurface,
        meanTempAir + t1Param2 * (biomass - bmLimiter) / t1Param3
    );
    EXPECT_NE(
        meanTempSurface,
        meanTempAir +
            (t1Param1 * pet * (1. - (aet / pet)) * (1. - (biomass / bmLimiter)))
    );
    EXPECT_NE(
        meanTempSurface, surface_temperature_under_snow(meanTempAir, snow)
    );

    // checks for  lyrTemp_to_lyrSoil_temperature
    int const resultValue = sizeof(meanTempSoil) / sizeof(meanTempSoil[0]);

    // when the number of soil layers is 1, meanTempSoil should have length 1
    EXPECT_EQ(1, resultValue);
    EXPECT_GT(meanTempSoil[0], -100); // Sense check
    EXPECT_LT(meanTempSoil[0], 100);  // Sense check

    // Expect that sTempInitR is updated to sTempR for the next day
    for (k = 0; k <= nRgr + 1; k++) {
        // sw_printf("\n k %u, newoldtempR %f", k,
        // SW_StRegSimVals.oldavgLyrTempR[k]);
        EXPECT_NE(SW_StRegSimVals.oldavgLyrTempR[k], SW_MISSING);
    }

    // ptr_stError should be set to TRUE if soil_temperature_today fails (i.e.
    // unrealistic temp values)

    meanTempAir = 1500.;

    SW_ST_setup_run(
        &SW_StRegSimVals,
        &SW_SoilRunIn,
        &SW_SiteIn,
        &SW_SiteSim,
        SW_SiteRunIn.Tsoil_constant,
        &ptr_stError,
        &SW_StRegSimVals.soil_temp_init,
        meanTempAir,
        swc,
        SW_SiteRunIn.n_layers,
        &meanTempSurface,
        meanTempSoil,
        lyrFrozen,
        &LogInfo
    );
    sw_fail_on_error(&LogInfo); // exit test program if unexpected error
    EXPECT_FALSE(ptr_stError) << LogInfo.warningMsgs[0];

    soil_temperature(
        &SW_StRegSimVals,
        &minTempSurface,
        &meanTempSurface,
        &maxTempSurface,
        minTempSoil,
        meanTempSoil,
        maxTempSoil,
        lyrFrozen,
        methodSurfaceTemperature,
        snow,
        minTempAir,
        meanTempAir,
        maxTempAir,
        H_gt,
        pet,
        aet,
        biomass,
        swc,
        swc_sat,
        bDensity,
        SW_SoilRunIn.width,
        SW_SoilRunIn.depths,
        nlyrs,
        bmLimiter,
        t1Param1,
        t1Param2,
        t1Param3,
        csParam1,
        csParam2,
        shParam,
        sTconst,
        deltaX,
        theMaxDepth,
        nRgr,
        year,
        doy,
        &ptr_stError,
        &LogInfo
    );
    sw_fail_on_error(&LogInfo); // exit test program if unexpected error
    // Check that error has occurred as indicated by ptr_stError
    EXPECT_TRUE(ptr_stError) << LogInfo.warningMsgs[0];
}

// Test main soil temperature function 'soil_temperature'
// AND lyrTemp_to_lyrSoil_temperature as this function
// is only called in the soil_temperature function
TEST(SWFlowTempTest, SWFlowTempMainSoilTemperatureFunction_LyrMAX) {
    SW_SITE_INPUTS SW_SiteIn;
    SW_SITE_SIM SW_SiteSim;
    SW_SITE_RUN_INPUTS SW_SiteRunIn;
    SW_SOIL_RUN_INPUTS SW_SoilRunIn;
    SW_ST_SIM SW_StRegSimVals;
    SW_ST_init_run(&SW_StRegSimVals);

    LOG_INFO LogInfo;
    // Initialize logs and silence warn/error reporting
    sw_init_logs(NULL, &LogInfo);

    double lyrFrozen[MAX_LAYERS] = {0};

    const unsigned int methodSurfaceTemperature = 0; // default prior to v8.1.0

    // *****  Test when nlyrs = MAX_LAYERS  ***** //
    sw_random_t soilTemp_rng;
    RandSeed(0u, 0u, &soilTemp_rng);


    unsigned int i;
    unsigned int k;
    unsigned int const year = 1980;
    unsigned int const doy = 1;

    // intialize values
    unsigned int const nRgr = 65;
    double const meanTempAir = 25.0;
    double const pet = 5.0;
    double const aet = 4.0;
    double biomass = 100.;
    double meanTempSurface = 15.;
    double const bmLimiter = 300.;
    double const t1Param1 = 15.;
    double const t1Param2 = -4.;
    double const t1Param3 = 600.;
    double const csParam1 = 0.00070;
    double const csParam2 = 0.00030;
    double const shParam = 0.18;
    double const sTconst = 4.15;
    double const deltaX = 15;
    double const theMaxDepth = 990.;
    double snow = 1;
    double const maxTempAir = 10.1;
    double const minTempAir = -5.0;
    double const H_gt = 300.0;
    double maxTempSurface = 10.6;
    double minTempSurface = -6.8;
    double acc = 0.;
    Bool ptr_stError = swFALSE;

    unsigned int const nlyrs2 = MAX_LAYERS;
    const double width2[] = {5,  5,  5,  10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
                             10, 10, 10, 10, 10, 10, 20, 20, 20, 20, 20, 20};
    double const sTempInit3[] = {1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 3, 3,
                                 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4};
    double sTemp3[MAX_LAYERS];
    double bDensity2[] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
                          1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};

    // we don't need soil texture, but we need SWC(sat), SWC(field capacity),
    // and SWC(wilting point)
    double *swc2 = new double[nlyrs2];
    double *swc_sat2 = new double[nlyrs2];
    double *minTempSoil = new double[nlyrs2];
    double *maxTempSoil = new double[nlyrs2];

    SW_SiteRunIn.n_layers = nlyrs2;
    SW_SiteSim.stNRGR = nRgr;

    acc = 0.;

    for (i = 0; i < nlyrs2; i++) {
        SW_SoilRunIn.avgLyrTempInit[i] = sTempInit3[i];
        // SWC(wilting point): width > swc_wp > 0
        SW_SiteSim.swcBulk_wiltpt[i] = 0.1 * width2[i];
        // SWC(field capacity): width > swc_fc > swc_wp
        SW_SiteSim.swcBulk_fieldcap[i] =
            fmin(width2[i], SW_SiteSim.swcBulk_wiltpt[i] + 0.15 * width2[i]);
        // SWC(saturation): width > swc_sat > swc_fc
        SW_SiteSim.swcBulk_saturated[i] =
            fmin(width2[i], SW_SiteSim.swcBulk_fieldcap[i] + 0.2 * width2[i]);
        // SWC: swc_sat >= SWC > 0; here, swc_fc >= SWC >= swc_wp
        swc2[i] = RandUniFloatRange(
            (float) SW_SiteSim.swcBulk_wiltpt[i],
            (float) SW_SiteSim.swcBulk_fieldcap[i],
            &soilTemp_rng
        );

        SW_SiteSim.soilBulk_density[i] = 1.;
        SW_SoilRunIn.width[i] = width2[i];
        acc += width2[i];
        SW_SoilRunIn.depths[i] = acc;
        SW_SiteSim.swcBulk_fieldcap[0] = 2.6;
        SW_SiteSim.swcBulk_wiltpt[0] = 1.0;
        SW_SiteIn.stDeltaX = 15;
        SW_SiteIn.stMaxDepth = 990.;

        // sw_printf("\n i %u, bDensity %f, swc_sat %f, fc %f, swc %f,  wp %f",
        //   i, bDensity2[i],  swc_sat2[i], fc2[i], swc2[i], wp2[i] );
    }

    SW_SiteRunIn.Tsoil_constant = 4.15;

    SW_ST_setup_run(
        &SW_StRegSimVals,
        &SW_SoilRunIn,
        &SW_SiteIn,
        &SW_SiteSim,
        SW_SiteRunIn.Tsoil_constant,
        &ptr_stError,
        &SW_StRegSimVals.soil_temp_init,
        meanTempAir,
        swc2,
        SW_SiteRunIn.n_layers,
        &meanTempSurface,
        sTemp3,
        lyrFrozen,
        &LogInfo
    );
    sw_fail_on_error(&LogInfo); // exit test program if unexpected error
    EXPECT_FALSE(ptr_stError) << LogInfo.warningMsgs[0];


    // Test surface temp equals surface_temperature_under_snow() because snow >
    // 0
    snow = 5;

    soil_temperature(
        &SW_StRegSimVals,
        &minTempSurface,
        &meanTempSurface,
        &maxTempSurface,
        minTempSoil,
        sTemp3,
        maxTempSoil,
        lyrFrozen,
        methodSurfaceTemperature,
        snow,
        minTempAir,
        meanTempAir,
        maxTempAir,
        H_gt,
        pet,
        aet,
        biomass,
        swc2,
        swc_sat2,
        bDensity2,
        SW_SoilRunIn.width,
        SW_SoilRunIn.depths,
        nlyrs2,
        bmLimiter,
        t1Param1,
        t1Param2,
        t1Param3,
        csParam1,
        csParam2,
        shParam,
        sTconst,
        deltaX,
        theMaxDepth,
        nRgr,
        year,
        doy,
        &ptr_stError,
        &LogInfo
    );
    sw_fail_on_error(&LogInfo); // exit test program if unexpected error
    EXPECT_FALSE(ptr_stError) << LogInfo.warningMsgs[0];

    EXPECT_EQ(
        meanTempSurface, surface_temperature_under_snow(meanTempAir, snow)
    );

    EXPECT_NE(
        meanTempSurface,
        meanTempAir + t1Param2 * (biomass - bmLimiter) / t1Param3
    );

    EXPECT_NE(
        meanTempSurface,
        meanTempAir +
            (t1Param1 * pet * (1. - (aet / pet)) * (1. - (biomass / bmLimiter)))
    );

    // Test surface temp equals equatin when biomass < blimititer & snow = 0
    snow = 0;
    biomass = 100;
    for (k = 0; k < nlyrs2; k++) {
        sTemp3[k] = sTempInit3[k];
    }

    soil_temperature(
        &SW_StRegSimVals,
        &minTempSurface,
        &meanTempSurface,
        &maxTempSurface,
        minTempSoil,
        sTemp3,
        maxTempSoil,
        lyrFrozen,
        methodSurfaceTemperature,
        snow,
        minTempAir,
        meanTempAir,
        maxTempAir,
        H_gt,
        pet,
        aet,
        biomass,
        swc2,
        swc_sat2,
        bDensity2,
        SW_SoilRunIn.width,
        SW_SoilRunIn.depths,
        nlyrs2,
        bmLimiter,
        t1Param1,
        t1Param2,
        t1Param3,
        csParam1,
        csParam2,
        shParam,
        sTconst,
        deltaX,
        theMaxDepth,
        nRgr,
        year,
        doy,
        &ptr_stError,
        &LogInfo
    );
    sw_fail_on_error(&LogInfo); // exit test program if unexpected error
    EXPECT_FALSE(ptr_stError) << LogInfo.warningMsgs[0];

    EXPECT_EQ(
        meanTempSurface,
        meanTempAir +
            (t1Param1 * pet * (1. - (aet / pet)) * (1. - (biomass / bmLimiter)))
    );
    EXPECT_NE(
        meanTempSurface,
        meanTempAir + t1Param2 * (biomass - bmLimiter) / t1Param3
    );
    EXPECT_NE(
        meanTempSurface, surface_temperature_under_snow(meanTempAir, snow)
    );

    // Test surface temp equals equation when biomass < blimititer & snow = 0
    biomass = 305;
    for (k = 0; k < nlyrs2; k++) {
        sTemp3[k] = SW_SoilRunIn.avgLyrTempInit[k];
    }

    soil_temperature(
        &SW_StRegSimVals,
        &minTempSurface,
        &meanTempSurface,
        &maxTempSurface,
        minTempSoil,
        sTemp3,
        maxTempSoil,
        lyrFrozen,
        methodSurfaceTemperature,
        snow,
        minTempAir,
        meanTempAir,
        maxTempAir,
        H_gt,
        pet,
        aet,
        biomass,
        swc2,
        swc_sat2,
        bDensity2,
        SW_SoilRunIn.width,
        SW_SoilRunIn.depths,
        nlyrs2,
        bmLimiter,
        t1Param1,
        t1Param2,
        t1Param3,
        csParam1,
        csParam2,
        shParam,
        sTconst,
        deltaX,
        theMaxDepth,
        nRgr,
        year,
        doy,
        &ptr_stError,
        &LogInfo
    );
    sw_fail_on_error(&LogInfo); // exit test program if unexpected error
    EXPECT_FALSE(ptr_stError) << LogInfo.warningMsgs[0];

    EXPECT_EQ(
        meanTempSurface,
        meanTempAir + t1Param2 * (biomass - bmLimiter) / t1Param3
    );
    EXPECT_NE(
        meanTempSurface,
        meanTempAir +
            (t1Param1 * pet * (1. - (aet / pet)) * (1. - (biomass / bmLimiter)))
    );
    EXPECT_NE(
        meanTempSurface, surface_temperature_under_snow(meanTempAir, snow)
    );

    // checks for  lyrTemp_to_lyrSoil_temperature
    int const resultValue2 = sizeof(sTemp3) / sizeof(sTemp3[0]);

    // when the number of soil layers is MAX_LAYERS, length of sTemp3 should
    // be MAX_LAYERS

    EXPECT_EQ(MAX_LAYERS, resultValue2);

    for (k = 0; k < nlyrs2; k++) {
        // sw_printf("\n k %u, sTemp3 %f", k, sTemp3[k]);
        EXPECT_GT(sTemp3[k], -100); // Sense check
        EXPECT_LT(sTemp3[k], 100);  // Sense check&
    }

    // Expect that sTempInitR is updated to sTempR for the next day
    for (k = 0; k <= nRgr + 1; k++) {
        // sw_printf("\n k %u, newoldtempR %f", k,
        // SW_Run.StRegSimVals.oldavgLyrTempR[k]);
        EXPECT_NE(SW_StRegSimVals.oldavgLyrTempR[k], SW_MISSING);
    }

    double *array_list[] = {swc2, swc_sat2, minTempSoil, maxTempSoil};
    for (i = 0; i < sw_length(array_list); i++) {
        delete[] array_list[i];
    }
}

// Test that main soil temperature functions fails when it is supposed to
TEST(SWFlowTempTest, SWFlowTempMainSoilTemperatureFunctionDeathTest) {
    SW_ST_SIM SW_StRegSimVals;
    SW_ST_init_run(&SW_StRegSimVals);

    LOG_INFO LogInfo;
    // Initialize logs and silence warn/error reporting
    sw_init_logs(NULL, &LogInfo);

    const unsigned int methodSurfaceTemperature = 0; // default prior to v8.1.0

    double lyrFrozen[MAX_LAYERS] = {0};
    double depths[MAX_LAYERS] = {0};

    unsigned int const nlyrs = 1;
    unsigned int const nRgr = 65;
    unsigned int const year = 1980;
    unsigned int const doy = 1;
    double const meanTempAir = 25.0;
    double const pet = 5.0;
    double const aet = 4.0;
    double const biomass = 100.;
    double meanTempSurface = 15.;
    double const bmLimiter = 300.;
    double const t1Param1 = 15.;
    double const t1Param2 = -4.;
    double const t1Param3 = 600.;
    double const csParam1 = 0.00070;
    double const csParam2 = 0.00030;
    double const shParam = 0.18;
    double const sTconst = 4.15;
    double const deltaX = 15;
    double const theMaxDepth = 990.;
    double const snow = 1;
    double const maxTempAir = 10.1;
    double const minTempAir = -5.0;
    double const H_gt = 300.0;
    double maxTempSurface = 10.6;
    double minTempSurface = -6.8;
    Bool ptr_stError = swFALSE;

    double swc[] = {1.0};
    double swc_sat[] = {1.5};
    double bDensity[] = {1.8};
    double width[] = {20};
    double meanTempSoil[1];
    double minTempSoil[] = {10.0};
    double maxTempSoil[] = {1.0};

    // Should fail when soil_temperature was not initialized
    soil_temperature(
        &SW_StRegSimVals,
        &minTempSurface,
        &meanTempSurface,
        &maxTempSurface,
        minTempSoil,
        meanTempSoil,
        maxTempSoil,
        lyrFrozen,
        methodSurfaceTemperature,
        snow,
        minTempAir,
        meanTempAir,
        maxTempAir,
        H_gt,
        pet,
        aet,
        biomass,
        swc,
        swc_sat,
        bDensity,
        width,
        depths,
        nlyrs,
        bmLimiter,
        t1Param1,
        t1Param2,
        t1Param3,
        csParam1,
        csParam2,
        shParam,
        sTconst,
        deltaX,
        theMaxDepth,
        nRgr,
        year,
        doy,
        &ptr_stError,
        &LogInfo
    );
    // expect error: don't exit test program via `sw_fail_on_error(&LogInfo)`
    EXPECT_TRUE(ptr_stError) << LogInfo.warningMsgs[0];

    // Detect failure by error message
    EXPECT_THAT(
        LogInfo.errorMsg,
        HasSubstr("SOILWAT2 ERROR soil temperature module was not initialized")
    );
}
} // namespace
