#include "include/generic.h"             // for swFALSE, Bool, RealD, GT, fmax
#include "include/rands.h"               // for RandNorm, RandSeed, RandUni...
#include "include/SW_datastructs.h"      // for LOG_INFO, ST_RGR_VALUES
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

    // When there is snow and airTemp >= 0, the return is -2.0
    EXPECT_EQ(-2.0, tSoilAvg);

    // test when snow > 0 and airTempAvg < 0
    snow = 1, airTempAvg = -10;

    tSoilAvg = surface_temperature_under_snow(airTempAvg, snow);

    // When there snow == 1 airTempAvg = -10
    EXPECT_EQ(-4.55, tSoilAvg);

    //
    snow = 6.7, airTempAvg = 0;

    tSoilAvg = surface_temperature_under_snow(airTempAvg, snow);

    // When there is snow > 6.665 and airTemp >= 0, the return is -2.0
    EXPECT_EQ(-2.0, tSoilAvg);
}

// Test the soil temperature initialization function 'soil_temperature_setup'
TEST(SWFlowTempTest, SWFlowTempSoilTemperatureInit) {
    SW_SITE SW_Site;
    ST_RGR_VALUES SW_StRegValues;
    SW_ST_init_run(&SW_StRegValues);

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
    double width[] = {20};
    double sTempInit[] = {1};
    double bDensity[] = {RandNorm(1., 0.5, &STInit_rng)};
    double fc[] = {RandNorm(1.5, 0.5, &STInit_rng)};
    double wp[1];
    wp[0] = fc[0] - 0.6; // wp will always be less than fc

    SW_Site.n_layers = nlyrs;
    SW_Site.width[0] = SW_Site.depths[0] = width[0];
    /// test standard conditions
    soil_temperature_setup(
        &SW_StRegValues,
        bDensity,
        width,
        sTempInit,
        sTconst,
        nlyrs,
        fc,
        wp,
        deltaX,
        theMaxDepth,
        nRgr,
        SW_Site.depths,
        &ptr_stError,
        &SW_StRegValues.soil_temp_init,
        &LogInfo
    );
    sw_fail_on_error(&LogInfo); // exit test program if unexpected error

    // Structure Tests
    EXPECT_EQ(
        sizeof(SW_StRegValues.tlyrs_by_slyrs),
        sizeof(double) * MAX_ST_RGR * (MAX_LAYERS + 1)
    );

    iStart = (unsigned int) ceil(SW_Site.depths[nlyrs - 1] / deltaX);
    for (i = iStart; i < nRgr + 1; i++) {
        EXPECT_EQ(SW_StRegValues.tlyrs_by_slyrs[i][nlyrs], -deltaX);
        // Values should be equal to -deltaX when i > the depth of the soil
        // profile/deltaX and j is == nlyrs
    }

    // Other init test
    // sum of inputs width = maximum depth; in my example 20
    EXPECT_EQ(SW_Site.depths[nlyrs - 1], 20);

    // nRgr = (MaxDepth/deltaX) - 1
    EXPECT_EQ((SW_StRegValues.depthsR[nRgr] / deltaX) - 1, nRgr);

    // *****  Test when nlyrs = MAX_LAYERS (SW_Defines.h)  ***** //
    /// generate inputs using a for loop
    nlyrs = MAX_LAYERS;
    double width2[] = {5,  5,  5,  10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
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

    SW_Site.n_layers = nlyrs;
    for (i = 0; i < nlyrs; i++) {
        SW_Site.width[i] = width2[i];
        acc += width2[i];
        SW_Site.depths[i] = acc;
    }

    soil_temperature_setup(
        &SW_StRegValues,
        bDensity2,
        width2,
        sTempInit2,
        sTconst,
        nlyrs,
        fc2,
        wp2,
        deltaX,
        theMaxDepth,
        nRgr,
        SW_Site.depths,
        &ptr_stError,
        &SW_StRegValues.soil_temp_init,
        &LogInfo
    );
    sw_fail_on_error(&LogInfo); // exit test program if unexpected error

    // Structure Tests
    EXPECT_EQ(
        sizeof(SW_StRegValues.tlyrs_by_slyrs),
        sizeof(double) * MAX_ST_RGR * (MAX_LAYERS + 1)
    );

    iStart = (unsigned int) ceil(SW_Site.depths[nlyrs - 1] / deltaX);
    for (i = iStart; i < nRgr + 1; i++) {
        EXPECT_EQ(SW_StRegValues.tlyrs_by_slyrs[i][nlyrs], -deltaX);
        // Values should be equal to -deltaX when i > the depth of the soil
        // profile/deltaX and j is == nlyrs
    }

    // Other init test
    // sum of inputs width = maximum depth; in my example 295
    EXPECT_EQ(SW_Site.depths[nlyrs - 1], 295);

    // nRgr = (MaxDepth/deltaX) - 1
    EXPECT_EQ((SW_StRegValues.depthsR[nRgr] / deltaX) - 1, nRgr);

    delete[] bDensity2;
    delete[] fc2;
    delete[] wp2;
}

// Death tests for soil_temperature_setup function
TEST(SWFlowTempTest, SWFlowTempSoilTemperatureInitDeathTest) {
    SW_SITE SW_Site;
    ST_RGR_VALUES SW_StRegValues;
    SW_ST_init_run(&SW_StRegValues);

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
    double width2[] = {5,  5,  5,  10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
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
        acc += width2[i];
        SW_Site.depths[i] = acc;
    }

    /// test when theMaxDepth is less than soil layer depth - function should
    /// fail
    double const theMaxDepth2 = 70.0;

    // We expect an error when max depth < last layer
    soil_temperature_setup(
        &SW_StRegValues,
        bDensity2,
        width2,
        sTempInit2,
        sTconst,
        nlyrs,
        fc2,
        wp2,
        deltaX,
        theMaxDepth2,
        nRgr,
        SW_Site.depths,
        &ptr_stError,
        &SW_StRegValues.soil_temp_init,
        &LogInfo
    );
    // expect error: don't exit test program via `sw_fail_on_error(&LogInfo)`

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
    SW_SITE SW_Site;
    ST_RGR_VALUES SW_StRegValues;
    SW_ST_init_run(&SW_StRegValues);

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
    double width[] = {20};
    double sTempInit[] = {1};
    tmp = RandNorm(1.5, 0.5, &SLIF_rng);
    double bDensity[] = {fmax(tmp, 0.1)};
    tmp = RandNorm(1.5, 0.5, &SLIF_rng);
    double fc[] = {fmax(tmp, 0.1)};
    double wp[1];

    wp[0] = fmax(fc[0] - 0.6, .1); // wp will always be less than fc

    SW_Site.n_layers = nlyrs;
    SW_Site.width[0] = SW_Site.depths[0] = width[0];
    soil_temperature_setup(
        &SW_StRegValues,
        bDensity,
        width,
        sTempInit,
        sTconst,
        nlyrs,
        fc,
        wp,
        deltaX,
        theMaxDepth,
        nRgr,
        SW_Site.depths,
        &ptr_stError,
        &SW_StRegValues.soil_temp_init,
        &LogInfo
    );
    sw_fail_on_error(&LogInfo); // exit test program if unexpected error

    // lyrSoil_to_lyrTemp tests: This function is used in soil_temperature_setup
    // to transfer the soil layer values of bdensity, fc, and wp, to the
    // "temperature layer" which are contained in bdensityR, fcR, and wpR. Thus
    // we check these values.
    for (i = 0; i < nRgr + 1; i++) {
        // all Values should be greater than 0
        EXPECT_GT(SW_StRegValues.bDensityR[i], 0);
        EXPECT_GT(SW_StRegValues.fcR[i], 0);
        EXPECT_GT(SW_StRegValues.wpR[i], 0);
    }

    iStart = (unsigned int) ceil(SW_Site.depths[nlyrs - 1] / deltaX);
    for (i = iStart; i < nRgr + 1; i++) {
        // The TempLayer values that are at depths greater than the max
        // SoilLayer depth should be uniform
        EXPECT_EQ(SW_StRegValues.bDensityR[i], SW_StRegValues.bDensityR[i - 1]);
        EXPECT_EQ(SW_StRegValues.fcR[i], SW_StRegValues.fcR[i - 1]);
        EXPECT_EQ(SW_StRegValues.wpR[i], SW_StRegValues.wpR[i - 1]);
    }

    // lyrSoil_to_lyrTemp_temperature tests
    // surface temperature is initialized to missing because not used
    EXPECT_TRUE(missing(SW_StRegValues.oldavgLyrTempR[0]));

    double maxvalR = 0.;
    for (i = 1; i < nRgr + 1; i++) {
        // Values interpolated into sTempInitR should be realistic
        EXPECT_GT(SW_StRegValues.oldavgLyrTempR[i], -100);

        // Values interpolated into sTempInitR should be realistic
        EXPECT_LT(SW_StRegValues.oldavgLyrTempR[i], 100);

        if (GT(SW_StRegValues.oldavgLyrTempR[i], maxvalR)) {
            maxvalR = SW_StRegValues.oldavgLyrTempR[i];
        }
    }

    // Maximum interpolated sTempInitR value should be less than or equal to
    // maximum in sTempInit2 (sTconst = last layer)
    EXPECT_LE(maxvalR, sTconst);

    // Temperature in last interpolated layer should equal sTconst
    EXPECT_EQ(SW_StRegValues.oldavgLyrTempR[nRgr + 1], sTconst);

    // *****  Test when nlyrs = MAX_LAYERS (SW_Defines.h)  ***** //
    /// generate inputs using a for loop
    nlyrs = MAX_LAYERS;
    double width2[] = {5,  5,  5,  10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
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
    SW_Site.n_layers = nlyrs;
    for (i = 0; i < nlyrs; i++) {
        acc += width2[i];
        SW_Site.depths[i] = acc;
    }

    soil_temperature_setup(
        &SW_StRegValues,
        bDensity2,
        width2,
        sTempInit2,
        sTconst,
        nlyrs,
        fc2,
        wp2,
        deltaX,
        theMaxDepth,
        nRgr,
        SW_Site.depths,
        &ptr_stError,
        &SW_StRegValues.soil_temp_init,
        &LogInfo
    );
    sw_fail_on_error(&LogInfo); // exit test program if unexpected error

    // lyrSoil_to_lyrTemp tests
    for (i = 0; i < nRgr + 1; i++) {
        // all Values should be greater than 0
        EXPECT_GT(SW_StRegValues.bDensityR[i], 0);
        EXPECT_GT(SW_StRegValues.fcR[i], 0);
        EXPECT_GT(SW_StRegValues.wpR[i], 0);
    }

    iStart = (unsigned int) ceil(SW_Site.depths[nlyrs - 1] / deltaX);
    for (i = iStart; i < nRgr + 1; i++) {
        // The TempLayer values that are at depths greater than the max
        // SoilLayer depth should be uniform
        EXPECT_EQ(SW_StRegValues.bDensityR[i], SW_StRegValues.bDensityR[i - 1]);
        EXPECT_EQ(SW_StRegValues.fcR[i], SW_StRegValues.fcR[i - 1]);
        EXPECT_EQ(SW_StRegValues.wpR[i], SW_StRegValues.wpR[i - 1]);
    }

    // lyrSoil_to_lyrTemp_temperature tests
    EXPECT_TRUE(missing(SW_StRegValues.oldavgLyrTempR[0])
    ); // surface temperature is initialized to missing because not used
    maxvalR = 0.;
    for (i = 1; i <= nRgr + 1; i++) {
        // Values interpolated into sTempInitR should be realistic
        EXPECT_GT(SW_StRegValues.oldavgLyrTempR[i], -200);

        // Values interpolated into sTempInitR should be realistic
        EXPECT_LT(SW_StRegValues.oldavgLyrTempR[i], 200);

        if (GT(SW_StRegValues.oldavgLyrTempR[i], maxvalR)) {
            maxvalR = SW_StRegValues.oldavgLyrTempR[i];
        }
    }

    // Maximum interpolated sTempInitR value should be less than or equal to
    // maximum in sTempInit2 (sTconst = last layer)
    EXPECT_LE(maxvalR, sTconst);

    // Temperature in last interpolated layer should equal sTconst
    EXPECT_EQ(SW_StRegValues.oldavgLyrTempR[nRgr + 1], sTconst);

    delete[] bDensity2;
    delete[] fc2;
    delete[] wp2;
}

// Test set layer to frozen or unfrozen 'set_frozen_unfrozen'
TEST(SWFlowTempTest, SWFlowTempSetFrozenUnfrozen) {
    RealD lyrFrozen[MAX_LAYERS] = {0};

    // declare inputs and output
    // *****  Test when nlyrs = 1  ***** //
    /// ***** Test that soil freezes ***** ///
    unsigned int nlyrs = 1;
    double sTemp[] = {-5};
    double swc[] = {1.5};
    double swc_sat[] = {1.8};
    double width[] = {5};

    set_frozen_unfrozen(nlyrs, sTemp, swc, swc_sat, width, lyrFrozen);


    // Soil should freeze when sTemp is <= -1 AND swc is > swc_sat - width * .13
    EXPECT_EQ(1, lyrFrozen[0]);

    /// ***** Test that soil does not freeze ***** ///
    double sTemp2[] = {0};

    set_frozen_unfrozen(nlyrs, sTemp2, swc, swc_sat, width, lyrFrozen);

    // Soil should NOT freeze when sTemp is > -1
    EXPECT_EQ(0, lyrFrozen[0]);

    // *****  Test when nlyrs = MAX_LAYERS (SW_Defines.h)  ***** //
    nlyrs = MAX_LAYERS;
    double width2[] = {5,  5,  5,  10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
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

    // Check that values that are set, are set right.
    EXPECT_EQ(sTempR[0], T1);
    EXPECT_EQ(sTempR[nRgr + 1], sTconst);

    // Check that ptr_stError is FALSE
    EXPECT_EQ(ptr_stError, 0);

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

    // Check that ptr_stError is TRUE
    EXPECT_EQ(ptr_stError, 1);
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
    ST_RGR_VALUES SW_StRegValues;
    SW_ST_init_run(&SW_StRegValues);

    SW_SITE SW_Site;

    LOG_INFO LogInfo;
    // Initialize logs and silence warn/error reporting
    sw_init_logs(NULL, &LogInfo);

    RealD lyrFrozen[MAX_LAYERS] = {0};

    unsigned int k;
    unsigned int const year = 1980;
    unsigned int const doy = 1;

    // *****  Test when nlyrs = 1  ***** //
    unsigned int const nlyrs = 1;
    unsigned int const nRgr = 65;
    double airTemp = 25.0;
    double const pet = 5.0;
    double const aet = 4.0;
    double biomass = 100.;
    double surfaceTemp = 15.;
    double const bmLimiter = 300.;
    double const t1Param1 = 15.;
    double const t1Param2 = -4.;
    double const t1Param3 = 600.;
    double const csParam1 = 0.00070;
    double const csParam2 = 0.00030;
    double const shParam = 0.18;
    double snowdepth = 5;
    double const sTconst = 4.15;
    double const deltaX = 15;
    double const theMaxDepth = 990.;
    double const snow = 1;
    double const max_air_temp = 10.1;
    double const min_air_temp = -5.0;
    double const H_gt = 300.0;
    double surface_max = 10.6;
    double surface_min = -6.8;
    Bool ptr_stError = swFALSE;

    double swc[] = {1.0};
    double swc_sat[] = {1.5};
    double bDensity[] = {1.8};
    double width[] = {20};
    double sTemp[1];
    double min_temp[] = {10.0};
    double max_temp[] = {1.0};

    SW_Site.n_layers = nlyrs;
    SW_Site.stNRGR = nRgr;

    SW_Site.soilBulk_density[0] = 1.8;
    SW_Site.width[0] = SW_Site.depths[0] = 20;
    SW_Site.avgLyrTempInit[0] = 5.0;
    SW_Site.Tsoil_constant = 4.15;
    SW_Site.swcBulk_fieldcap[0] = 2.6;
    SW_Site.swcBulk_wiltpt[0] = 1.0;
    SW_Site.stDeltaX = 15;
    SW_Site.stMaxDepth = 990.;

    SW_Site.swcBulk_saturated[0] = 1.5;


    SW_ST_setup_run(
        &SW_StRegValues,
        &SW_Site,
        &ptr_stError,
        &SW_StRegValues.soil_temp_init,
        airTemp,
        swc,
        &surfaceTemp,
        sTemp,
        lyrFrozen,
        &LogInfo
    );
    sw_fail_on_error(&LogInfo); // exit test program if unexpected error

    soil_temperature(
        &SW_StRegValues,
        &surface_max,
        &surface_min,
        lyrFrozen,
        airTemp,
        pet,
        aet,
        biomass,
        swc,
        swc_sat,
        bDensity,
        width,
        sTemp,
        &surfaceTemp,
        nlyrs,
        bmLimiter,
        t1Param1,
        t1Param2,
        t1Param3,
        csParam1,
        csParam2,
        shParam,
        snowdepth,
        sTconst,
        deltaX,
        theMaxDepth,
        nRgr,
        snow,
        max_air_temp,
        min_air_temp,
        H_gt,
        year,
        doy,
        SW_Site.depths,
        min_temp,
        max_temp,
        &ptr_stError,
        &LogInfo
    );
    sw_fail_on_error(&LogInfo); // exit test program if unexpected error

    // Expect that surface temp equals surface_temperature_under_snow() because
    // snow > 0
    EXPECT_EQ(surfaceTemp, surface_temperature_under_snow(airTemp, snow));
    EXPECT_NE(
        surfaceTemp, airTemp + ((t1Param2 * (biomass - bmLimiter)) / t1Param3)
    );
    EXPECT_NE(
        surfaceTemp,
        airTemp +
            (t1Param1 * pet * (1. - (aet / pet)) * (1. - (biomass / bmLimiter)))
    );

    // Test surface temp equals equation when biomass < blimititer & snow = 0
    snowdepth = 0;

    for (k = 0; k < nlyrs; k++) {
        sTemp[k] = SW_Site.avgLyrTempInit[k];
    }

    soil_temperature(
        &SW_StRegValues,
        &surface_max,
        &surface_min,
        lyrFrozen,
        airTemp,
        pet,
        aet,
        biomass,
        swc,
        swc_sat,
        bDensity,
        width,
        sTemp,
        &surfaceTemp,
        nlyrs,
        bmLimiter,
        t1Param1,
        t1Param2,
        t1Param3,
        csParam1,
        csParam2,
        shParam,
        snowdepth,
        sTconst,
        deltaX,
        theMaxDepth,
        nRgr,
        snow,
        max_air_temp,
        min_air_temp,
        H_gt,
        year,
        doy,
        SW_Site.depths,
        min_temp,
        max_temp,
        &ptr_stError,
        &LogInfo
    );
    sw_fail_on_error(&LogInfo); // exit test program if unexpected error

    EXPECT_EQ(
        surfaceTemp,
        airTemp +
            (t1Param1 * pet * (1. - (aet / pet)) * (1. - (biomass / bmLimiter)))
    );
    EXPECT_NE(
        surfaceTemp, airTemp + ((t1Param2 * (biomass - bmLimiter)) / t1Param3)
    );
    EXPECT_NE(surfaceTemp, surface_temperature_under_snow(airTemp, snow));

    // Test surface temp equals equation when biomass > blimititer & snow = 0
    biomass = 305;

    for (k = 0; k < nlyrs; k++) {
        sTemp[k] = SW_Site.avgLyrTempInit[k];
    }

    soil_temperature(
        &SW_StRegValues,
        &surface_max,
        &surface_min,
        lyrFrozen,
        airTemp,
        pet,
        aet,
        biomass,
        swc,
        swc_sat,
        bDensity,
        width,
        sTemp,
        &surfaceTemp,
        nlyrs,
        bmLimiter,
        t1Param1,
        t1Param2,
        t1Param3,
        csParam1,
        csParam2,
        shParam,
        snowdepth,
        sTconst,
        deltaX,
        theMaxDepth,
        nRgr,
        snow,
        max_air_temp,
        min_air_temp,
        H_gt,
        year,
        doy,
        SW_Site.depths,
        min_temp,
        max_temp,
        &ptr_stError,
        &LogInfo
    );
    sw_fail_on_error(&LogInfo); // exit test program if unexpected error

    EXPECT_EQ(
        surfaceTemp, airTemp + ((t1Param2 * (biomass - bmLimiter)) / t1Param3)
    );
    EXPECT_NE(
        surfaceTemp,
        airTemp +
            (t1Param1 * pet * (1. - (aet / pet)) * (1. - (biomass / bmLimiter)))
    );
    EXPECT_NE(surfaceTemp, surface_temperature_under_snow(airTemp, snow));

    // checks for  lyrTemp_to_lyrSoil_temperature
    int const resultValue = sizeof(sTemp) / sizeof(sTemp[0]);

    // when the number of soil layers is 1, sTemp should have length 1
    EXPECT_EQ(1, resultValue);
    EXPECT_GT(sTemp[0], -100); // Sense check
    EXPECT_LT(sTemp[0], 100);  // Sense check
    EXPECT_EQ(0, ptr_stError); // ptr_stError should be FALSE

    // Expect that sTempInitR is updated to sTempR for the next day
    for (k = 0; k <= nRgr + 1; k++) {
        // sw_printf("\n k %u, newoldtempR %f", k,
        // SW_StRegValues.oldavgLyrTempR[k]);
        EXPECT_NE(SW_StRegValues.oldavgLyrTempR[k], SW_MISSING);
    }

    // ptr_stError should be set to TRUE if soil_temperature_today fails (i.e.
    // unrealistic temp values)

    airTemp = 1500.;

    SW_ST_setup_run(
        &SW_StRegValues,
        &SW_Site,
        &ptr_stError,
        &SW_StRegValues.soil_temp_init,
        airTemp,
        swc,
        &surfaceTemp,
        sTemp,
        lyrFrozen,
        &LogInfo
    );
    sw_fail_on_error(&LogInfo); // exit test program if unexpected error

    EXPECT_EQ(ptr_stError, swFALSE);

    soil_temperature(
        &SW_StRegValues,
        &surface_max,
        &surface_max,
        lyrFrozen,
        airTemp,
        pet,
        aet,
        biomass,
        swc,
        swc_sat,
        bDensity,
        width,
        sTemp,
        &surfaceTemp,
        nlyrs,
        bmLimiter,
        t1Param1,
        t1Param2,
        t1Param3,
        csParam1,
        csParam2,
        shParam,
        snowdepth,
        sTconst,
        deltaX,
        theMaxDepth,
        nRgr,
        snow,
        max_air_temp,
        min_air_temp,
        H_gt,
        year,
        doy,
        SW_Site.depths,
        min_temp,
        max_temp,
        &ptr_stError,
        &LogInfo
    );
    sw_fail_on_error(&LogInfo); // exit test program if unexpected error

    // Check that error has occurred as indicated by ptr_stError
    EXPECT_EQ(ptr_stError, swTRUE);
}

// Test main soil temperature function 'soil_temperature'
// AND lyrTemp_to_lyrSoil_temperature as this function
// is only called in the soil_temperature function
TEST(SWFlowTempTest, SWFlowTempMainSoilTemperatureFunction_LyrMAX) {
    SW_SITE SW_Site;
    ST_RGR_VALUES SW_StRegValues;
    SW_ST_init_run(&SW_StRegValues);

    LOG_INFO LogInfo;
    // Initialize logs and silence warn/error reporting
    sw_init_logs(NULL, &LogInfo);

    RealD lyrFrozen[MAX_LAYERS] = {0};


    // *****  Test when nlyrs = MAX_LAYERS  ***** //
    sw_random_t soilTemp_rng;
    RandSeed(0u, 0u, &soilTemp_rng);


    unsigned int i;
    unsigned int k;
    unsigned int const year = 1980;
    unsigned int const doy = 1;

    // intialize values
    unsigned int const nRgr = 65;
    double const airTemp = 25.0;
    double const pet = 5.0;
    double const aet = 4.0;
    double biomass = 100.;
    double surfaceTemp = 15.;
    double const bmLimiter = 300.;
    double const t1Param1 = 15.;
    double const t1Param2 = -4.;
    double const t1Param3 = 600.;
    double const csParam1 = 0.00070;
    double const csParam2 = 0.00030;
    double const shParam = 0.18;
    double snowdepth = 5;
    double const sTconst = 4.15;
    double const deltaX = 15;
    double const theMaxDepth = 990.;
    double const snow = 1;
    double const max_air_temp = 10.1;
    double const min_air_temp = -5.0;
    double const H_gt = 300.0;
    double surface_max = 10.6;
    double surface_min = -6.8;
    double acc = 0.;
    Bool ptr_stError = swFALSE;

    unsigned int const nlyrs2 = MAX_LAYERS;
    double width2[] = {5,  5,  5,  10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
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
    double *min_temp = new double[nlyrs2];
    double *max_temp = new double[nlyrs2];

    SW_Site.n_layers = nlyrs2;
    SW_Site.stNRGR = nRgr;

    acc = 0.;

    for (i = 0; i < nlyrs2; i++) {
        SW_Site.avgLyrTempInit[i] = sTempInit3[i];
        // SWC(wilting point): width > swc_wp > 0
        SW_Site.swcBulk_wiltpt[i] = 0.1 * width2[i];
        // SWC(field capacity): width > swc_fc > swc_wp
        SW_Site.swcBulk_fieldcap[i] =
            fmin(width2[i], SW_Site.swcBulk_wiltpt[i] + 0.15 * width2[i]);
        // SWC(saturation): width > swc_sat > swc_fc
        SW_Site.swcBulk_saturated[i] =
            fmin(width2[i], SW_Site.swcBulk_fieldcap[i] + 0.2 * width2[i]);
        // SWC: swc_sat >= SWC > 0; here, swc_fc >= SWC >= swc_wp
        swc2[i] = RandUniFloatRange(
            (float) SW_Site.swcBulk_wiltpt[i],
            (float) SW_Site.swcBulk_fieldcap[i],
            &soilTemp_rng
        );

        SW_Site.soilBulk_density[i] = 1.;
        SW_Site.width[i] = width2[i];
        acc += width2[i];
        SW_Site.depths[i] = acc;
        SW_Site.swcBulk_fieldcap[0] = 2.6;
        SW_Site.swcBulk_wiltpt[0] = 1.0;
        SW_Site.stDeltaX = 15;
        SW_Site.stMaxDepth = 990.;

        // sw_printf("\n i %u, bDensity %f, swc_sat %f, fc %f, swc %f,  wp %f",
        //   i, bDensity2[i],  swc_sat2[i], fc2[i], swc2[i], wp2[i] );
    }

    SW_Site.Tsoil_constant = 4.15;

    SW_ST_setup_run(
        &SW_StRegValues,
        &SW_Site,
        &ptr_stError,
        &SW_StRegValues.soil_temp_init,
        airTemp,
        swc2,
        &surfaceTemp,
        sTemp3,
        lyrFrozen,
        &LogInfo
    );
    sw_fail_on_error(&LogInfo); // exit test program if unexpected error


    // Test surface temp equals surface_temperature_under_snow() because snow >
    // 0
    snowdepth = 5;

    soil_temperature(
        &SW_StRegValues,
        &surface_max,
        &surface_min,
        lyrFrozen,
        airTemp,
        pet,
        aet,
        biomass,
        swc2,
        swc_sat2,
        bDensity2,
        width2,
        sTemp3,
        &surfaceTemp,
        nlyrs2,
        bmLimiter,
        t1Param1,
        t1Param2,
        t1Param3,
        csParam1,
        csParam2,
        shParam,
        snowdepth,
        sTconst,
        deltaX,
        theMaxDepth,
        nRgr,
        snow,
        max_air_temp,
        min_air_temp,
        H_gt,
        year,
        doy,
        SW_Site.depths,
        min_temp,
        max_temp,
        &ptr_stError,
        &LogInfo
    );
    sw_fail_on_error(&LogInfo); // exit test program if unexpected error

    EXPECT_EQ(surfaceTemp, surface_temperature_under_snow(airTemp, snow));

    EXPECT_NE(
        surfaceTemp, airTemp + ((t1Param2 * (biomass - bmLimiter)) / t1Param3)
    );

    EXPECT_NE(
        surfaceTemp,
        airTemp +
            (t1Param1 * pet * (1. - (aet / pet)) * (1. - (biomass / bmLimiter)))
    );

    // Test surface temp equals equatin when biomass < blimititer & snow = 0
    snowdepth = 0;
    biomass = 100;
    for (k = 0; k < nlyrs2; k++) {
        sTemp3[k] = sTempInit3[k];
    }

    soil_temperature(
        &SW_StRegValues,
        &surface_max,
        &surface_min,
        lyrFrozen,
        airTemp,
        pet,
        aet,
        biomass,
        swc2,
        swc_sat2,
        bDensity2,
        width2,
        sTemp3,
        &surfaceTemp,
        nlyrs2,
        bmLimiter,
        t1Param1,
        t1Param2,
        t1Param3,
        csParam1,
        csParam2,
        shParam,
        snowdepth,
        sTconst,
        deltaX,
        theMaxDepth,
        nRgr,
        snow,
        max_air_temp,
        min_air_temp,
        H_gt,
        year,
        doy,
        SW_Site.depths,
        min_temp,
        max_temp,
        &ptr_stError,
        &LogInfo
    );
    sw_fail_on_error(&LogInfo); // exit test program if unexpected error

    EXPECT_EQ(
        surfaceTemp,
        airTemp +
            (t1Param1 * pet * (1. - (aet / pet)) * (1. - (biomass / bmLimiter)))
    );
    EXPECT_NE(
        surfaceTemp, airTemp + ((t1Param2 * (biomass - bmLimiter)) / t1Param3)
    );
    EXPECT_NE(surfaceTemp, surface_temperature_under_snow(airTemp, snow));

    // Test surface temp equals equation when biomass < blimititer & snow = 0
    biomass = 305;
    for (k = 0; k < nlyrs2; k++) {
        sTemp3[k] = SW_Site.avgLyrTempInit[k];
    }

    soil_temperature(
        &SW_StRegValues,
        &surface_max,
        &surface_min,
        lyrFrozen,
        airTemp,
        pet,
        aet,
        biomass,
        swc2,
        swc_sat2,
        bDensity2,
        width2,
        sTemp3,
        &surfaceTemp,
        nlyrs2,
        bmLimiter,
        t1Param1,
        t1Param2,
        t1Param3,
        csParam1,
        csParam2,
        shParam,
        snowdepth,
        sTconst,
        deltaX,
        theMaxDepth,
        nRgr,
        snow,
        max_air_temp,
        min_air_temp,
        H_gt,
        year,
        doy,
        SW_Site.depths,
        min_temp,
        max_temp,
        &ptr_stError,
        &LogInfo
    );
    sw_fail_on_error(&LogInfo); // exit test program if unexpected error

    EXPECT_EQ(
        surfaceTemp, airTemp + ((t1Param2 * (biomass - bmLimiter)) / t1Param3)
    );
    EXPECT_NE(
        surfaceTemp,
        airTemp +
            (t1Param1 * pet * (1. - (aet / pet)) * (1. - (biomass / bmLimiter)))
    );
    EXPECT_NE(surfaceTemp, surface_temperature_under_snow(airTemp, snow));

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
        // SW_Run.StRegValues.oldavgLyrTempR[k]);
        EXPECT_NE(SW_StRegValues.oldavgLyrTempR[k], SW_MISSING);
    }

    double *array_list[] = {swc2, swc_sat2, min_temp, max_temp};
    for (i = 0; i < sw_length(array_list); i++) {
        delete[] array_list[i];
    }
}

// Test that main soil temperature functions fails when it is supposed to
TEST(SWFlowTempTest, SWFlowTempMainSoilTemperatureFunctionDeathTest) {
    ST_RGR_VALUES SW_StRegValues;
    SW_ST_init_run(&SW_StRegValues);

    LOG_INFO LogInfo;
    // Initialize logs and silence warn/error reporting
    sw_init_logs(NULL, &LogInfo);

    RealD lyrFrozen[MAX_LAYERS] = {0};
    RealD depths[MAX_LAYERS] = {0};

    unsigned int const nlyrs = 1;
    unsigned int const nRgr = 65;
    unsigned int const year = 1980;
    unsigned int const doy = 1;
    double const airTemp = 25.0;
    double const pet = 5.0;
    double const aet = 4.0;
    double const biomass = 100.;
    double surfaceTemp = 15.;
    double const bmLimiter = 300.;
    double const t1Param1 = 15.;
    double const t1Param2 = -4.;
    double const t1Param3 = 600.;
    double const csParam1 = 0.00070;
    double const csParam2 = 0.00030;
    double const shParam = 0.18;
    double const snowdepth = 5;
    double const sTconst = 4.15;
    double const deltaX = 15;
    double const theMaxDepth = 990.;
    double const snow = 1;
    double const max_air_temp = 10.1;
    double const min_air_temp = -5.0;
    double const H_gt = 300.0;
    double surface_max = 10.6;
    double surface_min = -6.8;
    Bool ptr_stError = swFALSE;

    double swc[] = {1.0};
    double swc_sat[] = {1.5};
    double bDensity[] = {1.8};
    double width[] = {20};
    double sTemp[1];
    double min_temp[] = {10.0};
    double max_temp[] = {1.0};

    // Should fail when soil_temperature was not initialized
    soil_temperature(
        &SW_StRegValues,
        &surface_max,
        &surface_min,
        lyrFrozen,
        airTemp,
        pet,
        aet,
        biomass,
        swc,
        swc_sat,
        bDensity,
        width,
        sTemp,
        &surfaceTemp,
        nlyrs,
        bmLimiter,
        t1Param1,
        t1Param2,
        t1Param3,
        csParam1,
        csParam2,
        shParam,
        snowdepth,
        sTconst,
        deltaX,
        theMaxDepth,
        nRgr,
        snow,
        max_air_temp,
        min_air_temp,
        H_gt,
        year,
        doy,
        depths,
        min_temp,
        max_temp,
        &ptr_stError,
        &LogInfo
    );
    // expect error: don't exit test program via `sw_fail_on_error(&LogInfo)`

    // Detect failure by error message
    EXPECT_THAT(
        LogInfo.errorMsg,
        HasSubstr("SOILWAT2 ERROR soil temperature module was not initialized")
    );
}
} // namespace
