#include "include/generic.h"             // for Bool, swTRUE, swFALSE, GT
#include "include/SW_datastructs.h"      // for SW_CLIMATE_CLIM, SW_CLIMATE...
#include "include/SW_Defines.h"          // for SW_MISSING, NVEGTYPES, ForE...
#include "include/SW_Main_lib.h"         // for sw_fail_on_error
#include "include/SW_Model.h"            // for SW_MDL_new_year
#include "include/SW_VegProd.h"          // for estimatePotNatVegComposition
#include "include/SW_Weather.h"          // for calcSiteClimate, SW_WTH_read
#include "include/Times.h"               // for Aug
#include "tests/gtests/sw_testhelpers.h" // for VegProdFixtureTest, tol6, tol3
#include "gmock/gmock.h"                 // for HasSubstr, MakePredicateFor...
#include "gtest/gtest.h"                 // for Test, Message, TestPartResul...
#include <stddef.h>                      // for NULL

using ::testing::HasSubstr;

namespace {

// Vegetation cover: see `estimatePotNatVegComposition()`
// RelAbundanceL0 and inputValues indices
const int succIndex = 0;
const int forbIndex = 1;
const int C3Index = 2;
const int C4Index = 3;
const int grassAnn = 4;
const int shrubIndex = 5;
const int treeIndex = 6;
const int bareGround = 7;

// RelAbundanceL1 indices
const int treeIndexL1 = 0;
const int shrubIndexL1 = 1;
const int forbIndexL1 = 2;
const int grassesIndexL1 = 3;
const int bareGroundL1 = 4;

void copyL0(double outL0[], const double inL0[]) {
    for (int index = 0; index < 8; index++) {
        outL0[index] = inL0[index];
    }
}

void calcVegCoverL1FromL0(double L1[], const double L0[]) {
    L1[treeIndexL1] = L0[treeIndex];
    L1[shrubIndexL1] = L0[shrubIndex];
    L1[forbIndexL1] = L0[forbIndex] + L0[succIndex];
    L1[grassesIndexL1] = L0[C3Index] + L0[C4Index] + L0[grassAnn];
    L1[bareGroundL1] = L0[bareGround];
}

void calcGrassCoverFromL0(double grass[], const double L0[]) {
    double const grass_sum = L0[C3Index] + L0[C4Index] + L0[grassAnn];

    if (GT(grass_sum, 0.)) {
        grass[0] = L0[C3Index] / grass_sum;
        grass[1] = L0[C4Index] / grass_sum;
        grass[2] = L0[grassAnn] / grass_sum;
    } else {
        grass[0] = 0.;
        grass[1] = 0.;
        grass[2] = 0.;
    }
}

void assert_decreasing_SWPcrit(SW_VEGPROD_INPUTS *SW_VegProdIn) {
    int rank;
    int vegtype;

    for (rank = 0; rank < NVEGTYPES - 1; rank++) {
        vegtype = SW_VegProdIn->rank_SWPcrits[rank];

        /*
        sw_printf("Rank=%d is vegtype=%d with SWPcrit=%f\n",
                rank, vegtype,
                VegProdIn.critSoilWater[vegtype]);
        */

        // Check that SWPcrit of `vegtype` is larger or equal to
        // SWPcrit of the vegetation type with the next larger rank
        ASSERT_GE(
            SW_VegProdIn->critSoilWater[vegtype],
            SW_VegProdIn->critSoilWater[SW_VegProdIn->rank_SWPcrits[rank + 1]]
        );
    }
}

double calc_veg_average(
    TimeInt yearIndex,
    double currAvg,
    const double *vals,
    IntU nTermYrs,
    IntU rmIndex
) {
    /*
        Check if the average to be taken is a running average (yearIndex + 1 <=
       # years for term) or a moving window average (yearIndex + 1 > # years for
       term)
    */
    if (yearIndex + 1 > nTermYrs) {
        currAvg *= nTermYrs;
        currAvg -= vals[rmIndex];
        currAvg += vals[yearIndex];
        currAvg /= nTermYrs;
    } else {
        currAvg *= yearIndex;
        currAvg += vals[yearIndex];
        currAvg /= (yearIndex + 1);
    }

    return currAvg;
}

// Test the SW_VEGPROD_INPUTS constructor 'SW_VPD_construct'
TEST(VegProdTest, VegProdConstructor) {
    // This test requires a local copy of SW_VEGPROD_INPUTS to avoid a memory
    // leak (see issue #205)
    // -- If using `SW_Run.VegProdIn` or a global variable
    // (for which `SW_VPD_construct()` has already been called once, e.g.,
    // during the test fixture's `SetUp()`), then this (second) call to
    // `SW_VPD_construct()` would allocate memory a second time
    // while `SW_VPD_deconstruct()` will de-allocate memory only once
    // (the call to `SW_VPD_deconstruct()`during the test fixture's `TearDown()`
    // would see only NULL and thus not de-allocate the required second time
    // to avoid a leak)
    SW_RUN sw;
    int k;
    LOG_INFO LogInfo;
    // Initialize logs and silence warn/error reporting
    sw_init_logs(NULL, &LogInfo);

    SW_VPD_construct(
        &sw.VegProdIn, &sw.RunIn.VegProdRunIn, sw.vp_p_oagg, sw.vp_p_accu
    );

    // Provide values for variables utilized by SW_VPD_init_run()
    sw.ModelIn.startyr = 1980;
    sw.ModelIn.endyr = 1981;
    sw.RunIn.SiteRunIn.n_layers = 8;
    sw.RunIn.ModelRunIn.isnorth = swTRUE;
    sw.VegProdIn.veg_method = 0;
    sw.SiteIn.methodMaxDepthSoilTemperature = 0;

    SW_VPD_init_run(&sw, &LogInfo);
    sw_fail_on_error(&LogInfo); // exit test program if unexpected error

    ForEachVegType(k) {
        EXPECT_DOUBLE_EQ(
            1., sw.VegProdSim.veg[k].co2_multipliers[BIO_INDEX][0]
        );
        EXPECT_DOUBLE_EQ(
            1., sw.VegProdSim.veg[k].co2_multipliers[BIO_INDEX][MAX_NYEAR - 1]
        );

        EXPECT_DOUBLE_EQ(
            1., sw.VegProdSim.veg[k].co2_multipliers[WUE_INDEX][0]
        );
        EXPECT_DOUBLE_EQ(
            1., sw.VegProdSim.veg[k].co2_multipliers[WUE_INDEX][MAX_NYEAR - 1]
        );
    }
}

// Test the application of the biomass CO2-effect
TEST(VegProdTest, VegProdBiomassCO2effect) {
    int i;
    double const x = 1.5;
    double biom1[12];
    double biom2[12];

    for (i = 0; i < 12; i++) {
        biom1[i] = i + 1.;
    }

    // One example
    apply_biomassCO2effect(biom2, biom1, x);

    for (i = 0; i < 12; i++) {
        EXPECT_DOUBLE_EQ(biom2[i], biom1[i] * x);
    }
}

// Test summing values across vegetation types
TEST(VegProdTest, VegProdSumming) {
    int vegIndex;

    double transp_coeff[NVEGTYPES][MAX_LAYERS];


    for (vegIndex = 0; vegIndex < NVEGTYPES; vegIndex++) {
        transp_coeff[vegIndex][0] = 0.;
    }

    EXPECT_DOUBLE_EQ(sum_across_vegtypes(transp_coeff, 0), 0.);

    for (vegIndex = 0; vegIndex < NVEGTYPES; vegIndex++) {
        transp_coeff[vegIndex][0] = 0.25;
    }

    EXPECT_DOUBLE_EQ(sum_across_vegtypes(transp_coeff, 0), 1.);
}

// Check `get_critical_rank`
TEST_F(VegProdFixtureTest, VegProdrank) {
    int k;
    // Check `get_critical_rank` for normal inputs, e.g., -2.0, -2.0, -3.5, -3.9
    get_critical_rank(&SW_Run.VegProdIn);
    assert_decreasing_SWPcrit(&SW_Run.VegProdIn);


    // Check `get_critical_rank` for constant values
    ForEachVegType(k) { SW_Run.VegProdIn.critSoilWater[k] = 0.; }

    get_critical_rank(&SW_Run.VegProdIn);
    assert_decreasing_SWPcrit(&SW_Run.VegProdIn);


    // Check `get_critical_rank` for increasing values
    ForEachVegType(k) { SW_Run.VegProdIn.critSoilWater[k] = k; }

    get_critical_rank(&SW_Run.VegProdIn);
    assert_decreasing_SWPcrit(&SW_Run.VegProdIn);


    // Check `get_critical_rank` for decreasing values
    ForEachVegType(k) { SW_Run.VegProdIn.critSoilWater[k] = NVEGTYPES - k; }

    get_critical_rank(&SW_Run.VegProdIn);
    assert_decreasing_SWPcrit(&SW_Run.VegProdIn);
}

TEST_F(VegProdFixtureTest, VegProdEstimateVegNotFullVegetation) {

    /*  ================================================================
                This block of tests deals with input values to
           `estimatePotNatVegComposition()` that do not add up to 1

     NOTE: Some tests use EXPECT_NEAR to cover for the unnecessary precision
                                    in results
        ================================================================  */


    SW_CLIMATE_YEARLY climateOutput;
    SW_CLIMATE_CLIM climateAverages;

    double inputValues[8];
    double const shrubLimit = .2;

    // Array holding only grass values
    double grassOutput[3]; // 3 = Number of grass variables

    // Array holding all values from the estimation
    double RelAbundanceL0[8]; // 8 = Number of types

    // Array holding all values from estimation minus grasses
    double RelAbundanceL1[5]; // 5 = Number of types minus grasses

    double const SumGrassesFraction = SW_MISSING;
    double C4Variables[3];

    Bool const fillEmptyWithBareGround = swTRUE;
    Bool const warnExtrapolation = swTRUE;
    Bool inNorthHem = swTRUE;
    Bool const fixBareGround = swTRUE;

    int const nTypes = 8;
    int index;


    double RelAbundanceL0Expected[8];
    double RelAbundanceL1Expected[5];
    double grassOutputExpected[3];

    SW_Run.ModelIn.startyr = 1980;
    SW_Run.ModelIn.endyr = 2010;

    SW_Run.VegProdIn.veg_method = VEG_METHOD_LONG_EST;
    SW_Run.RunIn.ModelRunIn.latitude = 90.0;

    // Reset "SW_Run.Weather.allHist"
    SW_WTH_read(
        &SW_Run.WeatherIn,
        &SW_Run.RunIn.weathRunAllHist,
        &SW_Run.RunIn.SkyRunIn,
        &SW_Run.ModelIn,
        SW_Run.RunIn.ModelRunIn.elevation,
        swTRUE,
        SW_Run.ModelSim.cum_monthdays,
        SW_Run.ModelSim.days_in_month,
        &LogInfo
    );
    sw_fail_on_error(&LogInfo); // exit test program if unexpected error

    finalizeAllWeather(
        &SW_Run.MarkovIn,
        &SW_Run.WeatherIn,
        SW_Run.RunIn.weathRunAllHist,
        SW_Run.ModelSim.cum_monthdays,
        SW_Run.ModelSim.days_in_month,
        NULL,
        swFALSE,
        &LogInfo
    );
    sw_fail_on_error(&LogInfo); // exit test program if unexpected error

    // Allocate arrays needed for `calcSiteClimate()` and
    // `averageClimateAcrossYears()`
    allocateClimateStructs(31, &climateOutput, &climateAverages, &LogInfo);
    sw_fail_on_error(&LogInfo); // exit test program if unexpected error

    // Calculate climate of the site and add results to "climateOutput"
    calcSiteClimate(
        SW_Run.RunIn.weathRunAllHist,
        SW_Run.ModelSim.cum_monthdays,
        SW_Run.ModelSim.days_in_month,
        31,
        1980,
        inNorthHem,
        &climateOutput
    );

    // Average values from "climateOutput" and put them in "climateAverages"
    averageClimateAcrossYears(&climateOutput, 31, &climateAverages);

    // Set C4 results, standard deviations are not needed for estimating
    // vegetation
    C4Variables[0] = climateAverages.minTemp7thMon_C;
    C4Variables[1] = climateAverages.ddAbove65F_degday;
    C4Variables[2] = climateAverages.frostFree_days;


    /*  ===============================================================
                 Test when all input values are "SW_MISSING"
        ===============================================================  */
    inputValues[succIndex] = SW_MISSING;
    inputValues[forbIndex] = SW_MISSING;
    inputValues[C3Index] = SW_MISSING;
    inputValues[C4Index] = SW_MISSING;
    inputValues[grassAnn] = SW_MISSING;
    inputValues[shrubIndex] = SW_MISSING;
    inputValues[treeIndex] = SW_MISSING;
    inputValues[bareGround] = SW_MISSING;

    /* Expect identical output to rSOILWAT2 (e.g., v5.3.1)
     * NOTE: Command uses deprecated estimate_PotNatVeg_composition (rSOILWAT >=
     v.6.0.0)
     ```{r}
     clim1 <- calc_SiteClimate(weatherList =
           rSOILWAT2::get_WeatherHistory(rSOILWAT2::sw_exampleData),
                                                           do_C4vars = TRUE)

       rSOILWAT2:::estimate_PotNatVeg_composition_old(
         MAP_mm =  10 * clim1[["MAP_cm"]], MAT_C = clim1[["MAT_C"]],
         mean_monthly_ppt_mm = 10 * clim1[["meanMonthlyPPTcm"]],
         mean_monthly_Temp_C = clim1[["meanMonthlyTempC"]],
         dailyC4vars = clim1[["dailyC4vars"]],
         fix_issue218 = TRUE
       )
     ```
     */

    // Set or calculate expected outputs
    copyL0(RelAbundanceL0Expected, inputValues);
    RelAbundanceL0Expected[succIndex] = 0.0;
    RelAbundanceL0Expected[forbIndex] = 0.2608391;
    RelAbundanceL0Expected[C3Index] = 0.4307061;
    RelAbundanceL0Expected[C4Index] = 0.0;
    RelAbundanceL0Expected[grassAnn] = 0.0;
    RelAbundanceL0Expected[shrubIndex] = 0.3084547;
    RelAbundanceL0Expected[treeIndex] = 0.0;
    RelAbundanceL0Expected[bareGround] = 0.0;

    calcVegCoverL1FromL0(RelAbundanceL1Expected, RelAbundanceL0Expected);
    calcGrassCoverFromL0(grassOutputExpected, RelAbundanceL0Expected);


    // Estimate vegetation
    estimatePotNatVegComposition(
        climateAverages.meanTemp_C,
        climateAverages.PPT_cm,
        climateAverages.meanTempMon_C,
        climateAverages.PPTMon_cm,
        inputValues,
        shrubLimit,
        SumGrassesFraction,
        C4Variables,
        fillEmptyWithBareGround,
        inNorthHem,
        warnExtrapolation,
        fixBareGround,
        grassOutput,
        RelAbundanceL0,
        RelAbundanceL1,
        &LogInfo
    );
    sw_fail_on_error(&LogInfo); // exit test program if unexpected error


    // Loop through RelAbundanceL0 and test results
    for (index = 0; index < nTypes; index++) {
        EXPECT_NEAR(RelAbundanceL0[index], RelAbundanceL0Expected[index], tol6);
    }

    // Loop through RelAbundanceL1 and test results
    for (index = 0; index < 5; index++) {
        EXPECT_NEAR(RelAbundanceL1[index], RelAbundanceL1Expected[index], tol6);
    }

    // Loop through grassOutput and test results
    for (index = 0; index < 3; index++) {
        EXPECT_NEAR(grassOutput[index], grassOutputExpected[index], tol6);
    }


    /*  ===============================================================
                 Test with some of input values not "SW_MISSING"
        ===============================================================  */

    // estimate cover of forbs and C4 grasses; fix all other
    inputValues[succIndex] = .376;
    inputValues[forbIndex] = SW_MISSING;
    inputValues[C3Index] = .096;
    inputValues[C4Index] = SW_MISSING;
    inputValues[grassAnn] = 0.;
    inputValues[shrubIndex] = .1098;
    inputValues[treeIndex] = .0372;
    inputValues[bareGround] = 0.;

    /* Expect identical output to rSOILWAT2 (e.g., v5.3.1)
     * NOTE: Command uses deprecated estimate_PotNatVeg_composition (rSOILWAT >=
     v.6.0.0)
     ```{r}
       clim1 <- calc_SiteClimate(weatherList =
             rSOILWAT2::get_WeatherHistory(rSOILWAT2::sw_exampleData),
                                                           do_C4vars = TRUE)

       rSOILWAT2:::estimate_PotNatVeg_composition_old(
         MAP_mm =  10 * clim1[["MAP_cm"]], MAT_C = clim1[["MAT_C"]],
         mean_monthly_ppt_mm = 10 * clim1[["meanMonthlyPPTcm"]],
         mean_monthly_Temp_C = clim1[["meanMonthlyTempC"]],
         Succulents_Fraction = .376, fix_succulents = TRUE,
         C3_Fraction = .096, fix_C3grasses = TRUE,
         Shrubs_Fraction = .1098, fix_shrubs = TRUE,
         Trees_Fraction = .0372, fix_trees = TRUE,
         dailyC4vars = clim1[["dailyC4vars"]],
         fix_issue218 = TRUE
       )
     ```
     */

    // Set or calculate expected outputs
    copyL0(RelAbundanceL0Expected, inputValues);
    RelAbundanceL0Expected[forbIndex] = .3810;
    RelAbundanceL0Expected[C4Index] = 0.;

    calcVegCoverL1FromL0(RelAbundanceL1Expected, RelAbundanceL0Expected);
    calcGrassCoverFromL0(grassOutputExpected, RelAbundanceL0Expected);


    // Estimate vegetation
    estimatePotNatVegComposition(
        climateAverages.meanTemp_C,
        climateAverages.PPT_cm,
        climateAverages.meanTempMon_C,
        climateAverages.PPTMon_cm,
        inputValues,
        shrubLimit,
        SumGrassesFraction,
        C4Variables,
        fillEmptyWithBareGround,
        inNorthHem,
        warnExtrapolation,
        fixBareGround,
        grassOutput,
        RelAbundanceL0,
        RelAbundanceL1,
        &LogInfo
    );
    sw_fail_on_error(&LogInfo); // exit test program if unexpected error

    // Loop through RelAbundanceL0 and test results
    for (index = 0; index < 8; index++) {
        EXPECT_DOUBLE_EQ(RelAbundanceL0[index], RelAbundanceL0Expected[index]);
    }

    // Loop through RelAbundanceL1 and test results
    for (index = 0; index < 5; index++) {
        EXPECT_DOUBLE_EQ(RelAbundanceL1[index], RelAbundanceL1Expected[index]);
    }

    // Loop through grassOutput and test results
    for (index = 0; index < 3; index++) {
        EXPECT_NEAR(grassOutput[index], grassOutputExpected[index], tol6);
    }


    /*  ===============================================================
                 Test with all input values not "SW_MISSING"
        ===============================================================  */

    inputValues[succIndex] = .1098;
    inputValues[forbIndex] = .1098;
    inputValues[C3Index] = .1098;
    inputValues[C4Index] = .1098;
    inputValues[grassAnn] = .1098;
    inputValues[shrubIndex] = .1098;
    inputValues[treeIndex] = .1098;
    inputValues[bareGround] = .1098;

    /* Expect identical output to rSOILWAT2 (e.g., v5.3.1)
     * NOTE: Command uses deprecated estimate_PotNatVeg_composition (rSOILWAT >=
     v.6.0.0)
     ```{r}
       clim1 <- calc_SiteClimate(weatherList =
             rSOILWAT2::get_WeatherHistory(rSOILWAT2::sw_exampleData),
                                                           do_C4vars = TRUE)

       rSOILWAT2:::estimate_PotNatVeg_composition_old(
         MAP_mm =  10 * clim1[["MAP_cm"]], MAT_C = clim1[["MAT_C"]],
         mean_monthly_ppt_mm = 10 * clim1[["meanMonthlyPPTcm"]],
         mean_monthly_Temp_C = clim1[["meanMonthlyTempC"]],
         Succulents_Fraction = .1098, fix_succulents = TRUE,
         C3_Fraction = .1098, fix_C3grasses = TRUE,
         Shrubs_Fraction = .1098, fix_shrubs = TRUE,
         Trees_Fraction = .1098, fix_trees = TRUE,
         Annuals_Fraction = .1098, fix_annuals = TRUE,
         C4_Fraction = .1098, fix_C4grasses = TRUE,
         Forbs_Fraction = .1098, fix_forbs = TRUE,
         BareGround_Fraction = .1098, fix_BareGround = TRUE,
         dailyC4vars = clim1[["dailyC4vars"]],
         fix_issue218 = TRUE
       )
     ```
     */

    // Set or calculate expected outputs
    copyL0(RelAbundanceL0Expected, inputValues);

    // RelAbundanceL0Expected[bareGround] is not .1098 because
    // fillEmptyWithBareGround = swTRUE
    RelAbundanceL0Expected[bareGround] = 0.2314;

    calcVegCoverL1FromL0(RelAbundanceL1Expected, RelAbundanceL0Expected);
    calcGrassCoverFromL0(grassOutputExpected, RelAbundanceL0Expected);


    // Estimate vegetation
    estimatePotNatVegComposition(
        climateAverages.meanTemp_C,
        climateAverages.PPT_cm,
        climateAverages.meanTempMon_C,
        climateAverages.PPTMon_cm,
        inputValues,
        shrubLimit,
        SumGrassesFraction,
        C4Variables,
        fillEmptyWithBareGround,
        inNorthHem,
        warnExtrapolation,
        fixBareGround,
        grassOutput,
        RelAbundanceL0,
        RelAbundanceL1,
        &LogInfo
    );
    sw_fail_on_error(&LogInfo); // exit test program if unexpected error

    // Loop through RelAbundanceL0 and test results.
    for (index = 0; index < nTypes; index++) {
        EXPECT_DOUBLE_EQ(RelAbundanceL0[index], RelAbundanceL0Expected[index]);
    }

    // Loop through RelAbundanceL1 and test results
    for (index = 0; index < 5; index++) {
        EXPECT_DOUBLE_EQ(RelAbundanceL1[index], RelAbundanceL1Expected[index]);
    }

    // Loop through grassOutput and test results
    for (index = 0; index < 3; index++) {
        EXPECT_NEAR(grassOutput[index], grassOutputExpected[index], tol6);
    }


    /*  ===============================================================
         Test `estimateVegetationFromClimate()` when "veg_method" is 1
         using default values of the function:
         [SW_MISSING, SW_MISSING, SW_MISSING, SW_MISSING, 0.0, SW_MISSING, 0.0,
       0.0]
        ===============================================================  */

    /* Expect identical output to rSOILWAT2 (e.g., v5.3.1)
     * NOTE: Command uses deprecated estimate_PotNatVeg_composition (rSOILWAT >=
     v.6.0.0)
     ```{r}
       clim1 <- calc_SiteClimate(weatherList =
             rSOILWAT2::get_WeatherHistory(rSOILWAT2::sw_exampleData),
                                                           do_C4vars = TRUE)

       rSOILWAT2:::estimate_PotNatVeg_composition_old(
         MAP_mm =  10 * clim1[["MAP_cm"]], MAT_C = clim1[["MAT_C"]],
         mean_monthly_ppt_mm = 10 * clim1[["meanMonthlyPPTcm"]],
         mean_monthly_Temp_C = clim1[["meanMonthlyTempC"]],
         dailyC4vars = clim1[["dailyC4vars"]],
         fix_issue218 = TRUE
       )
     ```
     */

    RelAbundanceL1Expected[treeIndexL1] = 0.;
    RelAbundanceL1Expected[shrubIndexL1] = .3084547;
    // forbIndexL1: Constains forbs + succulents (L0)
    RelAbundanceL1Expected[forbIndexL1] = .2608391;
    RelAbundanceL1Expected[grassesIndexL1] = .4307061;
    RelAbundanceL1Expected[bareGroundL1] = 0.;


    estimateVegetationFromClimate(
        &SW_Run.RunIn.VegProdRunIn,
        SW_Run.RunIn.weathRunAllHist,
        &SW_Run.ModelIn,
        &SW_Run.ModelSim,
        SW_Run.RunIn.ModelRunIn.isnorth,
        SW_Run.VegProdIn.veg_method,
        &LogInfo
    );
    sw_fail_on_error(&LogInfo); // exit test program if unexpected error

    // Loop through RelAbundanceL1 and test results
    for (index = 0; index < 4; index++) {
        EXPECT_NEAR(
            SW_Run.RunIn.VegProdRunIn.veg[index].cov.fCover,
            RelAbundanceL1Expected[index],
            tol6
        );
    }

    EXPECT_NEAR(
        SW_Run.RunIn.VegProdRunIn.bare_cov.fCover,
        RelAbundanceL1Expected[bareGroundL1],
        tol6
    );


    /*  ===============================================================
     Tests for southern hemisphere:

     1) Same input values as previous test except for trees and bare ground
     which are both .0549

     2) Default input values:
     [SW_MISSING, SW_MISSING, SW_MISSING, SW_MISSING, 0.0, SW_MISSING, 0.0, 0.0]
     yielding different values in southern hemisphere compared to northern
     hemisphere
        ===============================================================  */

    // Recalculate climate of the site in southern hemisphere and add results to
    // "climateOutput"
    inNorthHem = swFALSE;
    calcSiteClimate(
        SW_Run.RunIn.weathRunAllHist,
        SW_Run.ModelSim.cum_monthdays,
        SW_Run.ModelSim.days_in_month,
        31,
        1980,
        inNorthHem,
        &climateOutput
    );


    inputValues[treeIndex] = .0549;
    inputValues[bareGround] = .0549;

    /* Expect identical output to rSOILWAT2 (e.g., v5.3.1)
     * NOTE: Command uses deprecated estimate_PotNatVeg_composition (rSOILWAT >=
     v.6.0.0)
     ```{r}
       clim1 <- calc_SiteClimate(weatherList =
             rSOILWAT2::get_WeatherHistory(rSOILWAT2::sw_exampleData),
                                           do_C4vars = TRUE, latitude = -90)

       rSOILWAT2:::estimate_PotNatVeg_composition_old(
         MAP_mm =  10 * clim1[["MAP_cm"]], MAT_C = clim1[["MAT_C"]],
         mean_monthly_ppt_mm = 10 * clim1[["meanMonthlyPPTcm"]],
         mean_monthly_Temp_C = clim1[["meanMonthlyTempC"]],
         Succulents_Fraction = .1098, fix_succulents = TRUE,
         Forbs_Fraction = .1098, fix_forbs = TRUE,
         C3_Fraction = .1098, fix_C3grasses = TRUE,
         C4_Fraction = .1098, fix_C4grasses = TRUE,
         Annuals_Fraction = .1098, fix_annuals = TRUE,
         Shrubs_Fraction = .1098, fix_shrubs = TRUE,
         Trees_Fraction = 0.0549, fix_trees = TRUE,
         BareGround_Fraction = .0549, fix_BareGround = TRUE,
         isNorth = FALSE, dailyC4vars = clim1[["dailyC4vars"]],
         fix_issue218 = TRUE
       )
     ```
     */

    // Set or calculate expected outputs
    copyL0(RelAbundanceL0Expected, inputValues);
    RelAbundanceL0Expected[bareGround] = .2863;

    calcVegCoverL1FromL0(RelAbundanceL1Expected, RelAbundanceL0Expected);
    calcGrassCoverFromL0(grassOutputExpected, RelAbundanceL0Expected);


    // Estimate vegetation
    estimatePotNatVegComposition(
        climateAverages.meanTemp_C,
        climateAverages.PPT_cm,
        climateAverages.meanTempMon_C,
        climateAverages.PPTMon_cm,
        inputValues,
        shrubLimit,
        SumGrassesFraction,
        C4Variables,
        fillEmptyWithBareGround,
        inNorthHem,
        warnExtrapolation,
        fixBareGround,
        grassOutput,
        RelAbundanceL0,
        RelAbundanceL1,
        &LogInfo
    );
    sw_fail_on_error(&LogInfo); // exit test program if unexpected error

    // Loop through RelAbundanceL0 and test results.
    for (index = 0; index < 8; index++) {
        EXPECT_DOUBLE_EQ(RelAbundanceL0[index], RelAbundanceL0Expected[index]);
    }

    // Loop through RelAbundanceL1 and test results
    for (index = 0; index < 5; index++) {
        EXPECT_DOUBLE_EQ(RelAbundanceL1[index], RelAbundanceL1Expected[index]);
    }

    // Loop through grassOutput and test results
    for (index = 0; index < 3; index++) {
        EXPECT_NEAR(grassOutput[index], grassOutputExpected[index], tol6);
    }


    /*  ===============================================================
     Test "C4Variables" not being defined (faked by setting july min (index
     zero) to SW_MISSING) Use southern hemisphere for clear difference in values
     (C4 is/isn't defined) Use default values: [SW_MISSING, SW_MISSING,
     SW_MISSING, SW_MISSING, 0.0, SW_MISSING, 0.0, 0.0]
        ===============================================================  */

    inputValues[succIndex] = SW_MISSING;
    inputValues[forbIndex] = SW_MISSING;
    inputValues[C3Index] = SW_MISSING;
    inputValues[C4Index] = SW_MISSING;
    inputValues[grassAnn] = 0.;
    inputValues[shrubIndex] = SW_MISSING;
    inputValues[treeIndex] = 0.;
    inputValues[bareGround] = 0.;

    C4Variables[0] = SW_MISSING;

    /* Expect identical output to rSOILWAT2 (e.g., v5.3.1)
     * NOTE: Command uses deprecated estimate_PotNatVeg_composition (rSOILWAT >=
     v.6.0.0)
     ```{r}
       clim1 <- calc_SiteClimate(weatherList =
           rSOILWAT2::get_WeatherHistory(rSOILWAT2::sw_exampleData),
                                           do_C4vars = TRUE, latitude = -90)

       rSOILWAT2:::estimate_PotNatVeg_composition_old(
         MAP_mm =  10 * clim1[["MAP_cm"]], MAT_C = clim1[["MAT_C"]],
         mean_monthly_ppt_mm = 10 * clim1[["meanMonthlyPPTcm"]],
         mean_monthly_Temp_C = clim1[["meanMonthlyTempC"]],
         isNorth = FALSE,
         fix_issue218 = FALSE
       )
     ```
     */

    // Set or calculate expected outputs
    copyL0(RelAbundanceL0Expected, inputValues);
    RelAbundanceL0Expected[succIndex] = 0.;
    RelAbundanceL0Expected[forbIndex] = .22804606;
    RelAbundanceL0Expected[C3Index] = .52575060;
    RelAbundanceL0Expected[C4Index] = .15766932;
    RelAbundanceL0Expected[grassAnn] = 0.;
    RelAbundanceL0Expected[shrubIndex] = .08853402;
    RelAbundanceL0Expected[treeIndex] = 0.;
    RelAbundanceL0Expected[bareGround] = 0.;

    calcVegCoverL1FromL0(RelAbundanceL1Expected, RelAbundanceL0Expected);
    calcGrassCoverFromL0(grassOutputExpected, RelAbundanceL0Expected);


    // Estimate vegetation
    estimatePotNatVegComposition(
        climateAverages.meanTemp_C,
        climateAverages.PPT_cm,
        climateAverages.meanTempMon_C,
        climateAverages.PPTMon_cm,
        inputValues,
        shrubLimit,
        SumGrassesFraction,
        C4Variables,
        fillEmptyWithBareGround,
        inNorthHem,
        warnExtrapolation,
        fixBareGround,
        grassOutput,
        RelAbundanceL0,
        RelAbundanceL1,
        &LogInfo
    );
    sw_fail_on_error(&LogInfo); // exit test program if unexpected error


    // Loop through RelAbundanceL0 and test results.
    for (index = 0; index < nTypes; index++) {
        EXPECT_NEAR(RelAbundanceL0[index], RelAbundanceL0Expected[index], tol6);
    }

    // Loop through RelAbundanceL1 and test results
    for (index = 0; index < 5; index++) {
        EXPECT_NEAR(RelAbundanceL1[index], RelAbundanceL1Expected[index], tol6);
    }

    // Loop through grassOutput and test results
    for (index = 0; index < 3; index++) {
        EXPECT_NEAR(grassOutput[index], grassOutputExpected[index], tol6);
    }


    // Deallocate structs
    deallocateClimateStructs(&climateOutput, &climateAverages);
}

TEST_F(VegProdFixtureTest, VegProdEstimateVegFullVegetation) {

    /*  ================================================================
               This block of tests deals with input values to
               `estimatePotNatVegComposition()` that add up to 1

     NOTE: Some tests use EXPECT_NEAR to cover for the unnecessary precision
                                    in results
        ================================================================  */

    SW_CLIMATE_YEARLY climateOutput;
    SW_CLIMATE_CLIM climateAverages;

    int index;
    int const nTypes = 8;

    double inputValues[8];
    double const shrubLimit = .2;

    // Array holding only grass values
    double grassOutput[3]; // 3 = Number of grass variables

    // Array holding all values from the estimation
    double RelAbundanceL0[8]; // 8 = Number of types

    // Array holding all values from estimation minus grasses
    double RelAbundanceL1[5]; // 5 = Number of types minus grasses

    double SumGrassesFraction = SW_MISSING;
    double C4Variables[3];
    double RelAbundanceL0Expected[8];
    double RelAbundanceL1Expected[5];
    double grassOutputExpected[3];

    Bool fillEmptyWithBareGround = swTRUE;
    Bool const inNorthHem = swTRUE;
    Bool const warnExtrapolation = swTRUE;
    Bool const fixBareGround = swTRUE;


    // Reset "SW_Run.Weather.allHist"
    SW_WTH_read(
        &SW_Run.WeatherIn,
        &SW_Run.RunIn.weathRunAllHist,
        &SW_Run.RunIn.SkyRunIn,
        &SW_Run.ModelIn,
        SW_Run.RunIn.ModelRunIn.elevation,
        swTRUE,
        SW_Run.ModelSim.cum_monthdays,
        SW_Run.ModelSim.days_in_month,
        &LogInfo
    );
    sw_fail_on_error(&LogInfo); // exit test program if unexpected error

    finalizeAllWeather(
        &SW_Run.MarkovIn,
        &SW_Run.WeatherIn,
        SW_Run.RunIn.weathRunAllHist,
        SW_Run.ModelSim.cum_monthdays,
        SW_Run.ModelSim.days_in_month,
        NULL,
        swFALSE,
        &LogInfo
    );
    sw_fail_on_error(&LogInfo); // exit test program if unexpected error

    // Allocate arrays needed for `calcSiteClimate()` and
    // `averageClimateAcrossYears()`
    allocateClimateStructs(31, &climateOutput, &climateAverages, &LogInfo);
    sw_fail_on_error(&LogInfo); // exit test program if unexpected error

    // Calculate climate of the site and add results to "climateOutput"
    calcSiteClimate(
        SW_Run.RunIn.weathRunAllHist,
        SW_Run.ModelSim.cum_monthdays,
        SW_Run.ModelSim.days_in_month,
        31,
        1980,
        inNorthHem,
        &climateOutput
    );

    // Average values from "climateOutput" and put them in "climateAverages"
    averageClimateAcrossYears(&climateOutput, 31, &climateAverages);

    // Set C4 results, standard deviations are not needed for estimating
    // vegetation
    C4Variables[0] = climateAverages.minTemp7thMon_C;
    C4Variables[1] = climateAverages.ddAbove65F_degday;
    C4Variables[2] = climateAverages.frostFree_days;


    /*  ===============================================================
              Test when fixed inputs sum to 1 & all inputs are fixed
              Expect that outputs == inputs
        ===============================================================  */
    inputValues[succIndex] = .0567;
    inputValues[forbIndex] = .2317;
    inputValues[C3Index] = .0392;
    inputValues[C4Index] = .0981;
    inputValues[grassAnn] = .3218;
    inputValues[shrubIndex] = .0827;
    inputValues[treeIndex] = .1293;
    inputValues[bareGround] = .0405;

    // Set or calculate expected outputs
    copyL0(RelAbundanceL0Expected, inputValues);

    calcVegCoverL1FromL0(RelAbundanceL1Expected, RelAbundanceL0Expected);
    calcGrassCoverFromL0(grassOutputExpected, RelAbundanceL0Expected);


    // Estimate vegetation
    estimatePotNatVegComposition(
        climateAverages.meanTemp_C,
        climateAverages.PPT_cm,
        climateAverages.meanTempMon_C,
        climateAverages.PPTMon_cm,
        inputValues,
        shrubLimit,
        SumGrassesFraction,
        C4Variables,
        fillEmptyWithBareGround,
        inNorthHem,
        warnExtrapolation,
        fixBareGround,
        grassOutput,
        RelAbundanceL0,
        RelAbundanceL1,
        &LogInfo
    );
    sw_fail_on_error(&LogInfo); // exit test program if unexpected error


    // Loop through RelAbundanceL0 and test results.
    for (index = 0; index < nTypes; index++) {
        // All values in "RelAbundanceL0" should be exactly the same as
        // "inputValues"
        EXPECT_DOUBLE_EQ(RelAbundanceL0[index], inputValues[index]);
        EXPECT_NEAR(RelAbundanceL0[index], RelAbundanceL0Expected[index], tol3);
    }

    // Loop through RelAbundanceL1 and test results
    for (index = 0; index < 5; index++) {
        EXPECT_DOUBLE_EQ(RelAbundanceL1[index], RelAbundanceL1Expected[index]);
    }

    // Loop through grassOutput and test results
    for (index = 0; index < 3; index++) {
        EXPECT_DOUBLE_EQ(grassOutput[index], grassOutputExpected[index]);
    }


    /*  ===============================================================
              Test when fixed inputs sum to 1 & some inputs are not fixed
        ===============================================================  */
    inputValues[succIndex] = .5;
    inputValues[forbIndex] = SW_MISSING;
    inputValues[C3Index] = .5;
    inputValues[C4Index] = SW_MISSING;
    inputValues[grassAnn] = 0.;
    inputValues[shrubIndex] = SW_MISSING;
    inputValues[treeIndex] = 0.;
    inputValues[bareGround] = 0.;

    /* Expect identical output to rSOILWAT2 (e.g., v5.3.1)
     * NOTE: Command uses deprecated estimate_PotNatVeg_composition (rSOILWAT >=
     v.6.0.0)
     ```{r}
       clim1 <- calc_SiteClimate(weatherList =
             rSOILWAT2::get_WeatherHistory(rSOILWAT2::sw_exampleData),
                                                           do_C4vars = TRUE)

       rSOILWAT2:::estimate_PotNatVeg_composition_old(
         MAP_mm =  10 * clim1[["MAP_cm"]], MAT_C = clim1[["MAT_C"]],
         mean_monthly_ppt_mm = 10 * clim1[["meanMonthlyPPTcm"]],
         mean_monthly_Temp_C = clim1[["meanMonthlyTempC"]],
         Succulents_Fraction = .5, fix_succulents = TRUE,
         C3_Fraction = .5, fix_C3grasses = TRUE,
         dailyC4vars = clim1[["dailyC4vars"]],
         fix_issue218 = TRUE
       )
     ```
     */

    // Set or calculate expected outputs
    copyL0(RelAbundanceL0Expected, inputValues);
    RelAbundanceL0Expected[forbIndex] = 0.;
    RelAbundanceL0Expected[C4Index] = 0.;
    RelAbundanceL0Expected[shrubIndex] = 0.;

    calcVegCoverL1FromL0(RelAbundanceL1Expected, RelAbundanceL0Expected);
    calcGrassCoverFromL0(grassOutputExpected, RelAbundanceL0Expected);


    // Estimate vegetation
    estimatePotNatVegComposition(
        climateAverages.meanTemp_C,
        climateAverages.PPT_cm,
        climateAverages.meanTempMon_C,
        climateAverages.PPTMon_cm,
        inputValues,
        shrubLimit,
        SumGrassesFraction,
        C4Variables,
        fillEmptyWithBareGround,
        inNorthHem,
        warnExtrapolation,
        fixBareGround,
        grassOutput,
        RelAbundanceL0,
        RelAbundanceL1,
        &LogInfo
    );
    sw_fail_on_error(&LogInfo); // exit test program if unexpected error


    // Loop through RelAbundanceL0 and test results.
    for (index = 0; index < nTypes; index++) {
        EXPECT_DOUBLE_EQ(RelAbundanceL0[index], RelAbundanceL0Expected[index]);
    }

    // Loop through RelAbundanceL1 and test results
    for (index = 0; index < 5; index++) {
        EXPECT_DOUBLE_EQ(RelAbundanceL1[index], RelAbundanceL1Expected[index]);
    }

    // Loop through grassOutput and test results
    for (index = 0; index < 3; index++) {
        EXPECT_DOUBLE_EQ(grassOutput[index], grassOutputExpected[index]);
    }


    /*  ===============================================================
     Test with `fillEmptyWithBareGround` set to false, same input values
     as previous test except for bare ground, which is now .2314
        ===============================================================  */
    fillEmptyWithBareGround = swFALSE;

    inputValues[succIndex] = .1098;
    inputValues[forbIndex] = .1098;
    inputValues[C3Index] = .1098;
    inputValues[C4Index] = .1098;
    inputValues[grassAnn] = .1098;
    inputValues[shrubIndex] = .1098;
    inputValues[treeIndex] = .1098;
    inputValues[bareGround] = .2314;

    /* Expect identical output to rSOILWAT2 (e.g., v5.3.1)
     * NOTE: Command uses deprecated estimate_PotNatVeg_composition (rSOILWAT >=
     v.6.0.0)
     ```{r}
       clim1 <- calc_SiteClimate(weatherList =
             rSOILWAT2::get_WeatherHistory(rSOILWAT2::sw_exampleData),
                                                           do_C4vars = TRUE)

       rSOILWAT2:::estimate_PotNatVeg_composition_old(
         MAP_mm =  10 * clim1[["MAP_cm"]], MAT_C = clim1[["MAT_C"]],
         mean_monthly_ppt_mm = 10 * clim1[["meanMonthlyPPTcm"]],
         mean_monthly_Temp_C = clim1[["meanMonthlyTempC"]],
         Succulents_Fraction = .1098, fix_succulents = TRUE,
         C3_Fraction = .1098, fix_C3grasses = TRUE,
         Shrubs_Fraction = .1098, fix_shrubs = TRUE,
         Trees_Fraction = .1098, fix_trees = TRUE,
         Annuals_Fraction = .1098, fix_annuals = TRUE,
         C4_Fraction = .1098, fix_C4grasses = TRUE,
         Forbs_Fraction = .1098, fix_forbs = TRUE,
         BareGround_Fraction = 0.2314, fix_BareGround = TRUE,
         fill_empty_with_BareGround = TRUE,
         dailyC4vars = clim1[["dailyC4vars"]],
         fix_issue218 = TRUE
       )
     ```
     */

    // Set or calculate expected outputs
    copyL0(RelAbundanceL0Expected, inputValues);

    calcVegCoverL1FromL0(RelAbundanceL1Expected, RelAbundanceL0Expected);
    calcGrassCoverFromL0(grassOutputExpected, RelAbundanceL0Expected);


    // Estimate vegetation
    estimatePotNatVegComposition(
        climateAverages.meanTemp_C,
        climateAverages.PPT_cm,
        climateAverages.meanTempMon_C,
        climateAverages.PPTMon_cm,
        inputValues,
        shrubLimit,
        SumGrassesFraction,
        C4Variables,
        fillEmptyWithBareGround,
        inNorthHem,
        warnExtrapolation,
        fixBareGround,
        grassOutput,
        RelAbundanceL0,
        RelAbundanceL1,
        &LogInfo
    );
    sw_fail_on_error(&LogInfo); // exit test program if unexpected error

    // Loop through RelAbundanceL0 and test results.
    for (index = 0; index < nTypes; index++) {
        EXPECT_DOUBLE_EQ(RelAbundanceL0[index], RelAbundanceL0Expected[index]);
    }

    // Loop through RelAbundanceL1 and test results
    for (index = 0; index < 5; index++) {
        EXPECT_DOUBLE_EQ(RelAbundanceL1[index], RelAbundanceL1Expected[index]);
    }

    // Loop through grassOutput and test results
    for (index = 0; index < 3; index++) {
        EXPECT_DOUBLE_EQ(grassOutput[index], grassOutputExpected[index]);
    }


    /*  ===============================================================
     Test with `SumGrassesFraction` being fixed, all input of previous tests
     are halved to .0549
        ===============================================================  */

    SumGrassesFraction = .7255;

    inputValues[succIndex] = .0549;
    inputValues[forbIndex] = .0549;
    inputValues[C3Index] = SW_MISSING;
    inputValues[C4Index] = SW_MISSING;
    inputValues[grassAnn] = 0.;
    inputValues[shrubIndex] = .0549;
    inputValues[treeIndex] = .0549;
    inputValues[bareGround] = .0549;

    /* Expect identical output to rSOILWAT2 (e.g., v5.3.1)
     * NOTE: Command uses deprecated estimate_PotNatVeg_composition (rSOILWAT >=
     v.6.0.0)
     ```{r}
       clim1 <- calc_SiteClimate(weatherList =
             rSOILWAT2::get_WeatherHistory(rSOILWAT2::sw_exampleData),
                                                           do_C4vars = TRUE)

       rSOILWAT2:::estimate_PotNatVeg_composition_old(
         MAP_mm =  10 * clim1[["MAP_cm"]], MAT_C = clim1[["MAT_C"]],
         mean_monthly_ppt_mm = 10 * clim1[["meanMonthlyPPTcm"]],
         mean_monthly_Temp_C = clim1[["meanMonthlyTempC"]],
         Succulents_Fraction = .0549, fix_succulents = TRUE,
         Forbs_Fraction = .0549, fix_forbs = TRUE,
         Shrubs_Fraction = .0549, fix_shrubs = TRUE,
         Trees_Fraction = .0549, fix_trees = TRUE,
         SumGrasses_Fraction = .7255, fix_sumgrasses = TRUE,
         BareGround_Fraction = .0549, fix_BareGround = TRUE,
         dailyC4vars = clim1[["dailyC4vars"]],
         fix_issue218 = TRUE
       )
     ```
     */

    // Set or calculate expected outputs
    copyL0(RelAbundanceL0Expected, inputValues);
    RelAbundanceL0Expected[C3Index] = .7255;
    RelAbundanceL0Expected[C4Index] = 0.;

    calcVegCoverL1FromL0(RelAbundanceL1Expected, RelAbundanceL0Expected);
    calcGrassCoverFromL0(grassOutputExpected, RelAbundanceL0Expected);


    // Estimate vegetation
    estimatePotNatVegComposition(
        climateAverages.meanTemp_C,
        climateAverages.PPT_cm,
        climateAverages.meanTempMon_C,
        climateAverages.PPTMon_cm,
        inputValues,
        shrubLimit,
        SumGrassesFraction,
        C4Variables,
        fillEmptyWithBareGround,
        inNorthHem,
        warnExtrapolation,
        fixBareGround,
        grassOutput,
        RelAbundanceL0,
        RelAbundanceL1,
        &LogInfo
    );
    sw_fail_on_error(&LogInfo); // exit test program if unexpected error

    // Loop through RelAbundanceL0 and test results.
    for (index = 0; index < nTypes; index++) {
        EXPECT_DOUBLE_EQ(RelAbundanceL0[index], RelAbundanceL0Expected[index]);
    }

    // Loop through RelAbundanceL1 and test results
    for (index = 0; index < 5; index++) {
        EXPECT_DOUBLE_EQ(RelAbundanceL1[index], RelAbundanceL1Expected[index]);
    }

    // Loop through grassOutput and test results
    for (index = 0; index < 3; index++) {
        EXPECT_DOUBLE_EQ(grassOutput[index], grassOutputExpected[index]);
    }

    // Expect that sum of grass cover is equal to requested `SumGrassesFraction`
    EXPECT_NEAR(RelAbundanceL1[grassesIndexL1], SumGrassesFraction, tol6);


    /*  ===============================================================
     Test where one input value is fixed at 1 and 5/7 are fixed to 0,
     with the rest being SW_MISSING (C3 and C4 values), and `SumGrassesFraction`
     is set to 0
        ===============================================================  */

    SumGrassesFraction = 0.;

    inputValues[succIndex] = 0.;
    inputValues[forbIndex] = 0.;
    inputValues[C3Index] = SW_MISSING;
    inputValues[C4Index] = SW_MISSING;
    inputValues[grassAnn] = 0.;
    inputValues[shrubIndex] = 1.;
    inputValues[treeIndex] = 0.;
    inputValues[bareGround] = 0.;

    /* Expect identical output to rSOILWAT2 (e.g., v5.3.1)
     * NOTE: Command uses deprecated estimate_PotNatVeg_composition (rSOILWAT >=
     v.6.0.0)
     ```{r}
       clim1 <- calc_SiteClimate(weatherList =
             rSOILWAT2::get_WeatherHistory(rSOILWAT2::sw_exampleData),
                                                           do_C4vars = TRUE)

       rSOILWAT2:::estimate_PotNatVeg_composition_old(
         MAP_mm =  10 * clim1[["MAP_cm"]], MAT_C = clim1[["MAT_C"]],
         mean_monthly_ppt_mm = 10 * clim1[["meanMonthlyPPTcm"]],
         mean_monthly_Temp_C = clim1[["meanMonthlyTempC"]],
         Succulents_Fraction = 0, fix_succulents = TRUE,
         Forbs_Fraction = 0, fix_forbs = TRUE,
         Shrubs_Fraction = 1, fix_shrubs = TRUE,
         Trees_Fraction = 0, fix_trees = TRUE,
         SumGrasses_Fraction = 0, fix_sumgrasses = TRUE,
         BareGround_Fraction = 0, fix_BareGround = TRUE,
         dailyC4vars = clim1[["dailyC4vars"]],
         fix_issue218 = TRUE, fix_issue219 = TRUE
       )
     ```
     */

    // Set or calculate expected outputs
    copyL0(RelAbundanceL0Expected, inputValues);
    RelAbundanceL0Expected[C3Index] = 0.;
    RelAbundanceL0Expected[C4Index] = 0.;

    calcVegCoverL1FromL0(RelAbundanceL1Expected, RelAbundanceL0Expected);
    calcGrassCoverFromL0(grassOutputExpected, RelAbundanceL0Expected);


    // Estimate vegetation
    estimatePotNatVegComposition(
        climateAverages.meanTemp_C,
        climateAverages.PPT_cm,
        climateAverages.meanTempMon_C,
        climateAverages.PPTMon_cm,
        inputValues,
        shrubLimit,
        SumGrassesFraction,
        C4Variables,
        fillEmptyWithBareGround,
        inNorthHem,
        warnExtrapolation,
        fixBareGround,
        grassOutput,
        RelAbundanceL0,
        RelAbundanceL1,
        &LogInfo
    );
    sw_fail_on_error(&LogInfo); // exit test program if unexpected error

    // Loop through RelAbundanceL0 and test results.
    for (index = 0; index < nTypes; index++) {
        EXPECT_DOUBLE_EQ(RelAbundanceL0[index], RelAbundanceL0Expected[index]);
    }

    // Loop through RelAbundanceL1 and test results
    for (index = 0; index < 5; index++) {
        EXPECT_DOUBLE_EQ(RelAbundanceL1[index], RelAbundanceL1Expected[index]);
    }

    // Loop through grassOutput and test results
    for (index = 0; index < 3; index++) {
        EXPECT_DOUBLE_EQ(grassOutput[index], grassOutputExpected[index]);
    }

    // Expect that sum of grass cover is equal to requested `SumGrassesFraction`
    EXPECT_NEAR(RelAbundanceL1[grassesIndexL1], SumGrassesFraction, tol6);


    /*  ===============================================================
     Test when input sum is 1, including `SumGrassesFraction`, and
     grass needs to be estimated
        ===============================================================  */

    SumGrassesFraction = .5;

    inputValues[succIndex] = 0.;
    inputValues[forbIndex] = 0.;
    inputValues[C3Index] = SW_MISSING;
    inputValues[C4Index] = SW_MISSING;
    inputValues[grassAnn] = 0.;
    inputValues[shrubIndex] = 0.;
    inputValues[treeIndex] = 0.;
    inputValues[bareGround] = .5;

    /* Expect identical output to rSOILWAT2 (e.g., v5.3.1)
     * NOTE: Command uses deprecated estimate_PotNatVeg_composition (rSOILWAT >=
     v.6.0.0)
     ```{r}
       clim1 <- calc_SiteClimate(weatherList =
             rSOILWAT2::get_WeatherHistory(rSOILWAT2::sw_exampleData),
                                                           do_C4vars = TRUE)

       rSOILWAT2:::estimate_PotNatVeg_composition_old(
         MAP_mm =  10 * clim1[["MAP_cm"]], MAT_C = clim1[["MAT_C"]],
         mean_monthly_ppt_mm = 10 * clim1[["meanMonthlyPPTcm"]],
         mean_monthly_Temp_C = clim1[["meanMonthlyTempC"]],
         Succulents_Fraction = 0.0, fix_succulents = TRUE,
         Forbs_Fraction = 0.0, fix_forbs = TRUE,
         Shrubs_Fraction = 0.0, fix_shrubs = TRUE,
         Trees_Fraction = 0.0, fix_trees = TRUE,
         SumGrasses_Fraction = .5, fix_sumgrasses = TRUE,
         BareGround_Fraction = .5, fix_BareGround = TRUE,
         dailyC4vars = clim1[["dailyC4vars"]],
         fix_issue218 = TRUE, fix_issue219 = TRUE
       )
     ```
     */


    // Set or calculate expected outputs
    copyL0(RelAbundanceL0Expected, inputValues);
    RelAbundanceL0Expected[C3Index] = 0.5;
    RelAbundanceL0Expected[C4Index] = 0.;

    calcVegCoverL1FromL0(RelAbundanceL1Expected, RelAbundanceL0Expected);
    calcGrassCoverFromL0(grassOutputExpected, RelAbundanceL0Expected);


    // Estimate vegetation
    estimatePotNatVegComposition(
        climateAverages.meanTemp_C,
        climateAverages.PPT_cm,
        climateAverages.meanTempMon_C,
        climateAverages.PPTMon_cm,
        inputValues,
        shrubLimit,
        SumGrassesFraction,
        C4Variables,
        fillEmptyWithBareGround,
        inNorthHem,
        warnExtrapolation,
        fixBareGround,
        grassOutput,
        RelAbundanceL0,
        RelAbundanceL1,
        &LogInfo
    );
    sw_fail_on_error(&LogInfo); // exit test program if unexpected error


    // Loop through RelAbundanceL0 and test results.
    for (index = 0; index < nTypes; index++) {
        EXPECT_DOUBLE_EQ(RelAbundanceL0[index], RelAbundanceL0Expected[index]);
    }

    // Loop through RelAbundanceL1 and test results
    for (index = 0; index < 5; index++) {
        EXPECT_DOUBLE_EQ(RelAbundanceL1[index], RelAbundanceL1Expected[index]);
    }

    // Loop through grassOutput and test results
    for (index = 0; index < 3; index++) {
        EXPECT_DOUBLE_EQ(grassOutput[index], grassOutputExpected[index]);
    }

    // Expect that sum of grass cover is equal to requested `SumGrassesFraction`
    EXPECT_NEAR(RelAbundanceL1[grassesIndexL1], SumGrassesFraction, tol6);


    // Deallocate structs
    deallocateClimateStructs(&climateOutput, &climateAverages);
}

TEST_F(VegProdFixtureTest, EstimateVegInputGreaterThanOne1DeathTest) {

    /*  ================================================================
               Tests a death case of `estimatePotNatVegComposition()`
                    when input vegetation values sum to over 1
        ================================================================  */

    SW_CLIMATE_CLIM climateAverages;
    SW_CLIMATE_YEARLY climateOutput;

    double const SumGrassesFraction = SW_MISSING;
    double C4Variables[3];

    Bool const fillEmptyWithBareGround = swTRUE;
    Bool const inNorthHem = swTRUE;
    Bool const warnExtrapolation = swTRUE;
    Bool const fixBareGround = swTRUE;

    double inputValues[8] = {
        .0567, .5, .0392, .0981, .3218, .0827, .1293, .0405
    };
    double const shrubLimit = .2;

    // Array holding only grass values
    double grassOutput[3]; // 3 = Number of grass variables

    // Array holding all values from the estimation
    double RelAbundanceL0[8]; // 8 = Number of types

    // Array holding all values from estimation minus grasses
    double RelAbundanceL1[5]; // 5 = Number of types minus grasses

    // Allocate arrays needed for `calcSiteClimate()` and
    // `averageClimateAcrossYears()`
    allocateClimateStructs(31, &climateOutput, &climateAverages, &LogInfo);
    sw_fail_on_error(&LogInfo); // exit test program if unexpected error

    /*  ===============================================================
     Test for fail when input sum is greater than one with the values:
     [.0567, .5, .0392, .0981, .3218, .0827, .1293, .0405]
        ===============================================================  */


    // Reset "SW_Run.Weather.allHist"
    SW_WTH_read(
        &SW_Run.WeatherIn,
        &SW_Run.RunIn.weathRunAllHist,
        &SW_Run.RunIn.SkyRunIn,
        &SW_Run.ModelIn,
        SW_Run.RunIn.ModelRunIn.elevation,
        swTRUE,
        SW_Run.ModelSim.cum_monthdays,
        SW_Run.ModelSim.days_in_month,
        &LogInfo
    );
    sw_fail_on_error(&LogInfo); // exit test program if unexpected error

    finalizeAllWeather(
        &SW_Run.MarkovIn,
        &SW_Run.WeatherIn,
        SW_Run.RunIn.weathRunAllHist,
        SW_Run.ModelSim.cum_monthdays,
        SW_Run.ModelSim.days_in_month,
        NULL,
        swFALSE,
        &LogInfo
    );
    sw_fail_on_error(&LogInfo); // exit test program if unexpected error

    // Calculate climate of the site and add results to "climateOutput"
    calcSiteClimate(
        SW_Run.RunIn.weathRunAllHist,
        SW_Run.ModelSim.cum_monthdays,
        SW_Run.ModelSim.days_in_month,
        31,
        1980,
        inNorthHem,
        &climateOutput
    );

    // Average values from "climateOutput" and put them in "climateAverages"
    averageClimateAcrossYears(&climateOutput, 31, &climateAverages);

    estimatePotNatVegComposition(
        climateAverages.meanTemp_C,
        climateAverages.PPT_cm,
        climateAverages.meanTempMon_C,
        climateAverages.PPTMon_cm,
        inputValues,
        shrubLimit,
        SumGrassesFraction,
        C4Variables,
        fillEmptyWithBareGround,
        inNorthHem,
        warnExtrapolation,
        fixBareGround,
        grassOutput,
        RelAbundanceL0,
        RelAbundanceL1,
        &LogInfo
    );
    // expect error: don't exit test program via `sw_fail_on_error(&LogInfo)`

    // Detect failure by error message
    EXPECT_THAT(
        LogInfo.errorMsg,
        HasSubstr("User defined relative abundance values sum to more"
                  " than 1 = full land cover")
    );

    // Free allocated data
    deallocateClimateStructs(&climateOutput, &climateAverages);
}

TEST_F(VegProdFixtureTest, EstimateVegInputGreaterThanOne2DeathTest) {

    /*  ================================================================
               Tests a death case of `estimatePotNatVegComposition()`
                    when input vegetation values sum to over 1
        ================================================================  */

    SW_CLIMATE_CLIM climateAverages;
    SW_CLIMATE_YEARLY climateOutput;

    double SumGrassesFraction = SW_MISSING;
    double C4Variables[3];

    Bool const fillEmptyWithBareGround = swTRUE;
    Bool const inNorthHem = swTRUE;
    Bool const warnExtrapolation = swTRUE;
    Bool const fixBareGround = swTRUE;

    double inputValues[8];
    double const shrubLimit = .2;

    // Array holding only grass values
    double grassOutput[3]; // 3 = Number of grass variables

    // Array holding all values from the estimation
    double RelAbundanceL0[8]; // 8 = Number of types

    // Array holding all values from estimation minus grasses
    double RelAbundanceL1[5]; // 5 = Number of types minus grasses

    // Allocate arrays needed for `calcSiteClimate()` and
    // `averageClimateAcrossYears()`
    allocateClimateStructs(31, &climateOutput, &climateAverages, &LogInfo);
    sw_fail_on_error(&LogInfo); // exit test program if unexpected error


    SumGrassesFraction = .5;

    inputValues[succIndex] = .0567;
    inputValues[forbIndex] = .25;
    inputValues[C3Index] = SW_MISSING;
    inputValues[C4Index] = SW_MISSING;
    inputValues[grassAnn] = .0912;
    inputValues[shrubIndex] = .0465;
    inputValues[treeIndex] = .1293;
    inputValues[bareGround] = .0405;

    /*  ===============================================================
     Test for fail when SumGrassesFraction makes the input sum greater than one
     [.0567, .25, .SW_MISSING, SW_MISSING, .0912, .0465, .1293, .0405], input
     sum = .6142 SumGrassesFraction = .5, total input sum: 1.023. Total input
     sum is 1.1211 instead of 1.1142, because annual grass is already defined,
     so that value is subtracted from SumGrassesFraction and added to the
     initial input sum
        ===============================================================  */

    // Reset "SW_Run.Weather.allHist"
    SW_WTH_read(
        &SW_Run.WeatherIn,
        &SW_Run.RunIn.weathRunAllHist,
        &SW_Run.RunIn.SkyRunIn,
        &SW_Run.ModelIn,
        SW_Run.RunIn.ModelRunIn.elevation,
        swTRUE,
        SW_Run.ModelSim.cum_monthdays,
        SW_Run.ModelSim.days_in_month,
        &LogInfo
    );
    sw_fail_on_error(&LogInfo); // exit test program if unexpected error

    finalizeAllWeather(
        &SW_Run.MarkovIn,
        &SW_Run.WeatherIn,
        SW_Run.RunIn.weathRunAllHist,
        SW_Run.ModelSim.cum_monthdays,
        SW_Run.ModelSim.days_in_month,
        NULL,
        swFALSE,
        &LogInfo
    );
    sw_fail_on_error(&LogInfo); // exit test program if unexpected error


    // Calculate climate of the site and add results to "climateOutput"
    calcSiteClimate(
        SW_Run.RunIn.weathRunAllHist,
        SW_Run.ModelSim.cum_monthdays,
        SW_Run.ModelSim.days_in_month,
        31,
        1980,
        inNorthHem,
        &climateOutput
    );

    // Average values from "climateOutput" and put them in "climateAverages"
    averageClimateAcrossYears(&climateOutput, 31, &climateAverages);

    estimatePotNatVegComposition(
        climateAverages.meanTemp_C,
        climateAverages.PPT_cm,
        climateAverages.meanTempMon_C,
        climateAverages.PPTMon_cm,
        inputValues,
        shrubLimit,
        SumGrassesFraction,
        C4Variables,
        fillEmptyWithBareGround,
        inNorthHem,
        warnExtrapolation,
        fixBareGround,
        grassOutput,
        RelAbundanceL0,
        RelAbundanceL1,
        &LogInfo
    );
    // expect error: don't exit test program via `sw_fail_on_error(&LogInfo)`

    // Detect failure by error message
    EXPECT_THAT(
        LogInfo.errorMsg,
        HasSubstr("User defined relative abundance values sum to more"
                  " than 1 = full land cover")
    );

    // Free allocated data
    deallocateClimateStructs(&climateOutput, &climateAverages);
}

TEST_F(VegProdFixtureTest, CalcAnnClimConditions) {
    /*  ================================================================
            Tests the vegetation function `calc_yearly_hist_vals()`
            with one year's worth of data
        ================================================================  */

    TimeInt day;
    const double monInc = 2.5;
    const double precipInc = .1; // cm
    const double maxMinInc = .75;

    const double maxMonTemp = 18.25;
    const double minMonTemp = -1 * maxMinInc;

    SW_VEGPROD_SIM SW_VegProdSim;
    SW_MODEL_INPUTS SW_ModelIn;
    SW_MODEL_SIM SW_ModelSim;
    double monMaxTemp[MAX_MONTHS] = {0.};
    double monTemp[MAX_MONTHS] = {0.};
    double monMinTemp[MAX_MONTHS] = {0.};
    double monPrecip[MAX_MONTHS] = {0.};
    double monMean = 0.;
    double dailyPrecip = .1;
    int mon = 0;
    double waterDef = 0.;
    double wetDegDays = 0.;
    double isoTherm[MAX_MONTHS] = {0.};
    double isoThermVal;
    double corrVar;
    double totPrecip = 0.;
    double wetMonPrecip = 0.;
    double dryMonPrecip = 0.;
    double expAnnTemp = 0.;
    double expSeasonPrecip = 0.;

    SW_WTH_deconstruct(&SW_Run.RunIn.weathRunAllHist);
    SW_WTH_allocateAllWeather(&SW_Run.RunIn.weathRunAllHist, 1, &LogInfo);
    sw_fail_on_error(&LogInfo); // exit test program if unexpected error

    clear_hist_weather(1, SW_Run.RunIn.weathRunAllHist, NULL);

    alloc_nyear_arrays(1, swFALSE, &SW_VegProdSim, &LogInfo);
    sw_fail_on_error(&LogInfo); // exit test program if unexpected error

    SW_MDL_construct(&SW_ModelSim);
    SW_ModelSim.year = 1980;
    SW_MDL_new_year(&SW_ModelIn, &SW_ModelSim);

    /*
        Set daily values for max/avg/min temperature and precipitation
        for the function `calc_yearly_hist_vals()` to grab/organize
    */
    for (day = 0; day < MAX_DAYS; day++) {
        if (day == SW_ModelSim.cum_monthdays[mon]) {
            // Update the mean temperature to rise until August
            // or fall after August to try to replicate the yearly
            // rise and fall in temperatures throughout the months
            // Do similar for precipitation
            monMean += (mon < Aug) ? monInc : -1 * monInc;
            dailyPrecip += (mon < Aug) ? precipInc : -1 * precipInc;
            mon++;
        }

        SW_Run.RunIn.weathRunAllHist->temp_max[day] = monMean + maxMinInc;
        SW_Run.RunIn.weathRunAllHist->temp_avg[day] = monMean;
        SW_Run.RunIn.weathRunAllHist->temp_min[day] = monMean - maxMinInc;
        SW_Run.RunIn.weathRunAllHist->ppt[day] = dailyPrecip;

        monMaxTemp[mon] += SW_Run.RunIn.weathRunAllHist->temp_max[day];
        monTemp[mon] += SW_Run.RunIn.weathRunAllHist->temp_avg[day];
        monMinTemp[mon] += SW_Run.RunIn.weathRunAllHist->temp_min[day];

        monPrecip[mon] +=
            SW_Run.RunIn.weathRunAllHist->ppt[day] * 10;          // cm -> mm
        totPrecip += SW_Run.RunIn.weathRunAllHist->ppt[day] * 10; // cm -> mm
    }

    /*
        Go month by month to determine various variable values that will
        be compared against the values produced in `calc_yearly_hist_vals()`
    */
    for (mon = 0; mon < MAX_MONTHS; mon++) {
        monMaxTemp[mon] /= SW_ModelSim.days_in_month[mon];
        monTemp[mon] /= SW_ModelSim.days_in_month[mon];
        monMinTemp[mon] /= SW_ModelSim.days_in_month[mon];

        if (GT(monTemp[mon] * 2, monPrecip[mon])) {
            waterDef += (2 * monTemp[mon]) - monPrecip[mon];
        } else if (LT(monTemp[mon] * 2, monPrecip[mon])) {
            wetDegDays += (30 * monTemp[mon]) - monPrecip[mon];
        }

        if (GT(monPrecip[mon], wetMonPrecip) || mon == 0) {
            wetMonPrecip = monPrecip[mon];
        }

        if (LT(monPrecip[mon], dryMonPrecip) || mon == 0) {
            dryMonPrecip = monPrecip[mon];
        }

        isoTherm[mon] = monMaxTemp[mon] - monMinTemp[mon];
    }
    isoThermVal = mean(isoTherm, MAX_MONTHS);
    isoThermVal /= (maxMonTemp - minMonTemp);

    corrVar = correlation_coefficient(monTemp, monPrecip, MAX_MONTHS);
    expAnnTemp = mean(monTemp, MAX_MONTHS);
    expSeasonPrecip =
        standardDeviation(monPrecip, MAX_MONTHS) / mean(monPrecip, MAX_MONTHS);

    calc_yearly_hist_vals(
        SW_Run.RunIn.weathRunAllHist, &SW_ModelSim, 0, swFALSE, &SW_VegProdSim
    );

    /*
        Test all values produced and written to SW_VegProdSim (precipitation
        is calculated and tested in mm)
    */
    EXPECT_NEAR(SW_VegProdSim.annTemp[0], expAnnTemp, tol6);
    EXPECT_NEAR(SW_VegProdSim.annTempColdestMon[0], minMonTemp, tol6);
    EXPECT_NEAR(SW_VegProdSim.annTempWarmestMon[0], maxMonTemp, tol6);
    EXPECT_NEAR(SW_VegProdSim.annIsotherm[0], isoThermVal * 100, tol6);
    EXPECT_NEAR(SW_VegProdSim.annTempPrecipCorr[0], corrVar, tol6);

    EXPECT_NEAR(SW_VegProdSim.annPrecip[0], totPrecip, tol6);
    EXPECT_NEAR(SW_VegProdSim.annPrecipWettestMon[0], wetMonPrecip, tol6);
    EXPECT_NEAR(SW_VegProdSim.annPrecipDriestMon[0], dryMonPrecip, tol6);
    EXPECT_NEAR(SW_VegProdSim.annWaterDef[0], waterDef, tol6);
    EXPECT_NEAR(SW_VegProdSim.annWetDegDays[0], wetDegDays, tol6);
    EXPECT_NEAR(SW_VegProdSim.annSeasonPrecip[0], expSeasonPrecip, tol6);

    SW_VPD_deconstruct(&SW_VegProdSim);
    SW_WTH_deconstruct(&SW_Run.RunIn.weathRunAllHist);
}

TEST_F(VegProdFixtureTest, CalcVegPredictorVals) {
    /*  ================================================================
            Tests the vegetation function `calc_veg_predictor_vals()`
            with one to thirty-one year's worth of data using a running mean
            (year # < long-term average length) then a moving window mean
            (year # >= long-term average length)
        ================================================================  */

    SW_VEGPROD_SIM SW_VegProdSim;
    SW_MODEL_INPUTS SW_ModelIn;
    SW_MODEL_SIM SW_ModelSim;

    TimeInt year;
    const TimeInt numYears = 31;
    const int nYearsShort = 3;
    const int nYearsLong = 30;

    double expLongAvg = 0.;
    double expShortAvg = 0.;
    double expAnom = 0.;
    double expRateAnom = 0.;

    alloc_nyear_arrays(numYears, swFALSE, &SW_VegProdSim, &LogInfo);
    sw_fail_on_error(&LogInfo); // exit test program if unexpected error

    SW_MDL_construct(&SW_ModelSim);
    SW_ModelSim.year = 1980;
    SW_MDL_new_year(&SW_ModelIn, &SW_ModelSim);

    for (year = 0; year < numYears; year++) {
        SW_VegProdSim.annTemp[year] = (double) (year + 1);
        SW_VegProdSim.annPrecip[year] = (double) (year + 1);
        SW_VegProdSim.annPrecipDriestMon[year] = (double) (year + 1);
        SW_VegProdSim.annPrecipWettestMon[year] = (double) (year + 1);
        SW_VegProdSim.annSeasonPrecip[year] = (double) (year + 1);
        SW_VegProdSim.annWaterDef[year] = (double) (year + 1);
        SW_VegProdSim.annTempColdestMon[year] = (double) (year + 1);
        SW_VegProdSim.annTempWarmestMon[year] = (double) (year + 1);
        SW_VegProdSim.annTempPrecipCorr[year] = (double) (year + 1);
        SW_VegProdSim.annWetDegDays[year] = (double) (year + 1);
        SW_VegProdSim.annIsotherm[year] = (double) (year + 1);
    }

    SW_VegProdSim.shortIndex = SW_VegProdSim.longIndex = 0;

    for (year = 0; year < numYears; year++) {
        calc_veg_predictor_vals(year, nYearsShort, nYearsLong, &SW_VegProdSim);

        expLongAvg = calc_veg_average(
            year,
            expLongAvg,
            SW_VegProdSim.annTemp,
            nYearsLong,
            SW_VegProdSim.longIndex - 1
        );
        expShortAvg = calc_veg_average(
            year,
            expShortAvg,
            SW_VegProdSim.annTemp,
            nYearsShort,
            SW_VegProdSim.shortIndex - 1
        );

        expAnom = SW_VegProdSim.annIsothermLongAvg -
                  SW_VegProdSim.annIsothermShortAvg;

        expRateAnom = SW_VegProdSim.annPrecipLongAvg;
        expRateAnom -= SW_VegProdSim.annPrecipShortAvg;
        expRateAnom /= SW_VegProdSim.annPrecipLongAvg;

        EXPECT_NEAR(SW_VegProdSim.annTempLongAvg, expLongAvg, tol6);
        EXPECT_NEAR(SW_VegProdSim.annTempPrecipLongAvg, expLongAvg, tol6);
        EXPECT_NEAR(SW_VegProdSim.annIsothermLongAvg, expLongAvg, tol6);
        EXPECT_NEAR(SW_VegProdSim.annWaterDefLongAvg, expLongAvg, tol6);
        EXPECT_NEAR(SW_VegProdSim.annSeasonPrecipLongAvg, expLongAvg, tol6);
        EXPECT_NEAR(SW_VegProdSim.annPrecipDriestMonLongAvg, expLongAvg, tol6);
        EXPECT_NEAR(SW_VegProdSim.annWetDegDaysLongAvg, expLongAvg, tol6);
        EXPECT_NEAR(SW_VegProdSim.annTempWarmestMonLongAvg, expLongAvg, tol6);
        EXPECT_NEAR(SW_VegProdSim.annTempColdestMonLongAvg, expLongAvg, tol6);
        EXPECT_NEAR(SW_VegProdSim.annPrecipWettestMonLongAvg, expLongAvg, tol6);
        EXPECT_NEAR(SW_VegProdSim.annPrecipLongAvg, expLongAvg, tol6);

        EXPECT_NEAR(SW_VegProdSim.annIsothermShortAvg, expShortAvg, tol6);
        EXPECT_NEAR(SW_VegProdSim.annTempPrecipShortAvg, expShortAvg, tol6);
        EXPECT_NEAR(SW_VegProdSim.annSeasonPrecipShortAvg, expShortAvg, tol6);
        EXPECT_NEAR(SW_VegProdSim.annPrecipShortAvg, expShortAvg, tol6);
        EXPECT_NEAR(SW_VegProdSim.annWetDegDaysShortAvg, expShortAvg, tol6);
        EXPECT_NEAR(SW_VegProdSim.annWaterDefShortAvg, expShortAvg, tol6);
        EXPECT_NEAR(
            SW_VegProdSim.annPrecipDriestMonShortAvg, expShortAvg, tol6
        );

        EXPECT_NEAR(SW_VegProdSim.anomIsotherm, expAnom, tol6);
        EXPECT_NEAR(SW_VegProdSim.anomTempPrecipCorr, expAnom, tol6);
        EXPECT_NEAR(SW_VegProdSim.anomWaterDef, expAnom, tol6);

        EXPECT_NEAR(SW_VegProdSim.rateAnomSeasonPrecip, expRateAnom, tol6);
        EXPECT_NEAR(SW_VegProdSim.rateAnomPrecip, expRateAnom, tol6);
        EXPECT_NEAR(SW_VegProdSim.rateAnomWetDegDays, expRateAnom, tol6);
        EXPECT_NEAR(SW_VegProdSim.rateAnomWaterDef, expRateAnom, tol6);
        EXPECT_NEAR(SW_VegProdSim.rateAnomPrecipDriestMon, expRateAnom, tol6);
    }

    SW_VPD_deconstruct(&SW_VegProdSim);
}

TEST_F(VegProdFixtureTest, CalcConstCONUS2025SiteInfo) {
    /*  ================================================================
        Tests the functions `calc_const_dynamic_veg_info()` and
        `calc_awhc()` which are used in the wrapper
        `calc_perc_var_in_soil_profile()` to calculate predictor variables for
        CONUS 2025 calculations that are constant and are not updated
        every year
    ================================================================  */

    /*
        Tests the function `calc_awhc()` which is used to calculate the
        available water holding capacity for a site
    */

    SW_SOIL_RUN_INPUTS SW_SoilRunIn;
    SW_SOIL_SIM SW_SoilSim;
    SW_SITE_SIM SW_SiteSim;
    LyrIndex n_layers = 1;
    LyrIndex lyr;
    double swcBulk_fieldcap[MAX_LAYERS];
    double swcBulk_wiltpt[MAX_LAYERS];
    double awhcRes;
    double expVal;

    // AWHC 1 layer with swcBulk_fieldcap[0] - swcBulk_wiltpt[0] < 0
    swcBulk_fieldcap[0] = .123;
    swcBulk_wiltpt[0] = .246;

    awhcRes = calc_awhc(swcBulk_fieldcap, swcBulk_wiltpt, n_layers);
    expVal = 0.;

    EXPECT_EQ(awhcRes, expVal);

    // AWHC 1 layer with swcBulk_fieldcap[0] - swcBulk_wiltpt[0] > 0
    swcBulk_fieldcap[0] = .5;

    awhcRes = calc_awhc(swcBulk_fieldcap, swcBulk_wiltpt, n_layers);
    expVal = .254;

    EXPECT_EQ(awhcRes, expVal);

    // AWHC 25 layers with sum(swcBulk_fieldcap - swcBulk_wiltpt) < 0
    n_layers = MAX_LAYERS;
    for (lyr = 0; lyr < n_layers; lyr++) {
        swcBulk_fieldcap[lyr] = .123;
        swcBulk_wiltpt[lyr] = .246;
    }

    awhcRes = calc_awhc(swcBulk_fieldcap, swcBulk_wiltpt, n_layers);
    expVal = 0.;

    EXPECT_EQ(awhcRes, expVal);

    // AWHC 25 layers with sum(swcBulk_fieldcap - swcBulk_wiltpt) > 0
    for (lyr = 0; lyr < n_layers; lyr++) {
        swcBulk_fieldcap[lyr] = .5;
    }

    awhcRes = calc_awhc(swcBulk_fieldcap, swcBulk_wiltpt, n_layers);
    expVal = 6.35;

    EXPECT_NEAR(awhcRes, expVal, tol6);

    /*
        Test the function `calc_const_dynamic_veg_info()` for both
        % of material in 3cm (or first layer) and weighted average of
        material through the whole soil profile
    */

    // One soil layer - depth > 3cm
    SW_SoilRunIn.fractionWeightMatric_sand[0] = .05;
    SW_SoilRunIn.fractionVolBulk_gravel[0] = .1;
    SW_SoilRunIn.fractionWeight_om[0] = .4;
    SW_SoilRunIn.fractionWeightMatric_clay[0] = .3;

    SW_SoilRunIn.depths[0] = SW_SoilRunIn.width[0] = 4.;

    calc_const_dynamic_veg_info(&SW_SoilSim, &SW_SoilRunIn, &SW_SiteSim, 1);

    EXPECT_DOUBLE_EQ(SW_SoilSim.percSand, .05);
    EXPECT_DOUBLE_EQ(SW_SoilSim.percCoarseFrag, .1);
    EXPECT_DOUBLE_EQ(SW_SoilSim.surfaceOM, .4);
    EXPECT_DOUBLE_EQ(SW_SoilSim.surfaceClay, .3);

    // Two soil layers - first layer depth < 3cm
    SW_SoilRunIn.fractionWeightMatric_sand[1] = .5;
    SW_SoilRunIn.fractionVolBulk_gravel[1] = .16;
    SW_SoilRunIn.fractionWeight_om[1] = .41;
    SW_SoilRunIn.fractionWeightMatric_clay[1] = .33;

    SW_SoilRunIn.depths[0] = SW_SoilRunIn.width[0] = 1.5;
    SW_SoilRunIn.depths[1] = 4.5;
    SW_SoilRunIn.width[1] = 3.0;

    calc_const_dynamic_veg_info(&SW_SoilSim, &SW_SoilRunIn, &SW_SiteSim, 2);

    EXPECT_DOUBLE_EQ(SW_SoilSim.percSand, .35);
    EXPECT_DOUBLE_EQ(SW_SoilSim.percCoarseFrag, .14);
    EXPECT_DOUBLE_EQ(SW_SoilSim.surfaceOM, .405);
    EXPECT_DOUBLE_EQ(SW_SoilSim.surfaceClay, .315);
}

TEST_F(VegProdFixtureTest, VegetationTypeEquivalency) {
    int k;
    LyrIndex i;
    double tc;
    double transpiration[2] = {0., 0.};
    double ecnw[2] = {0., 0.};
    double swc[2] = {0., 0.};

    Bool const copyWeather = swTRUE;

    int const vt1 = SW_GRASS;
    int const vt2 = SW_FORBS;

    SW_RUN run_vt1;
    SW_RUN run_vt2;

    // Store default cover of vt1 and vt2 combined
    tc = SW_Run.RunIn.VegProdRunIn.veg[vt1].cov.fCover +
         SW_Run.RunIn.VegProdRunIn.veg[vt2].cov.fCover;

    // Set cover of vt1 and vt2 to 0
    SW_Run.RunIn.VegProdRunIn.veg[vt1].cov.fCover = 0.;
    SW_Run.RunIn.VegProdRunIn.veg[vt2].cov.fCover = 0.;

    // Set parameters of vt2 equal to parameters of vt1 (if not already)
    SW_Run.VegProdIn.veg[vt2].SWPcrit = SW_Run.VegProdIn.veg[vt1].SWPcrit;
    SW_Run.VegProdIn.veg[vt2].veg_kdead = SW_Run.VegProdIn.veg[vt1].veg_kdead;

    ForEachSoilLayer(i, SW_Run.RunIn.SiteRunIn.n_layers) {
        SW_Run.RunIn.SoilRunIn.transp_coeff[vt2][i] =
            SW_Run.RunIn.SoilRunIn.transp_coeff[vt1][i];
    }


    // Run with vt1
    SW_RUN_deepCopy(
        &SW_Run,
        &run_vt1,
        &SW_Domain.OutDom,
        &SW_Run.RunIn,
        copyWeather,
        &LogInfo
    );
    sw_fail_on_error(&LogInfo);

    run_vt1.RunIn.VegProdRunIn.veg[vt1].cov.fCover = tc;

    SW_CTL_init_run(&run_vt1, &LogInfo);
    sw_fail_on_error(&LogInfo);

    SW_CTL_main(&run_vt1, &SW_Domain.OutDom, &LogInfo);
    sw_fail_on_error(&LogInfo);


    // Run with vt2
    SW_RUN_deepCopy(
        &SW_Run,
        &run_vt2,
        &SW_Domain.OutDom,
        &SW_Run.RunIn,
        copyWeather,
        &LogInfo
    );
    sw_fail_on_error(&LogInfo);

    run_vt2.RunIn.VegProdRunIn.veg[vt2].cov.fCover = tc;

    SW_CTL_init_run(&run_vt2, &LogInfo);
    sw_fail_on_error(&LogInfo);

    SW_CTL_main(&run_vt2, &SW_Domain.OutDom, &LogInfo);
    sw_fail_on_error(&LogInfo);


    // Expect that relevant simulation values of vt1 and vt2 are identical
    // Note: we do not produce output (p_accu) during tests; thus, we can
    // only check for the simulated values of the last time step
    ForEachVegType(k) {
        ecnw[0] += run_vt1.SoilWatSim.evap_veg[k];
        ecnw[1] += run_vt1.SoilWatSim.evap_veg[k];

        ForEachSoilLayer(i, SW_Run.RunIn.SiteRunIn.n_layers) {
            transpiration[0] += run_vt1.SoilWatSim.transpiration[k][i];
            transpiration[1] += run_vt2.SoilWatSim.transpiration[k][i];

            swc[0] += run_vt1.SoilWatSim.swcBulk[0][i];
            swc[1] += run_vt2.SoilWatSim.swcBulk[0][i];
        }
    }

    EXPECT_DOUBLE_EQ(run_vt1.SoilWatSim.aet, run_vt2.SoilWatSim.aet);
    EXPECT_DOUBLE_EQ(
        run_vt1.SoilWatSim.surfaceWater_evap,
        run_vt2.SoilWatSim.surfaceWater_evap
    );
    EXPECT_DOUBLE_EQ(
        run_vt1.SoilWatSim.litter_evap, run_vt2.SoilWatSim.litter_evap
    );
    EXPECT_DOUBLE_EQ(ecnw[0], ecnw[1]);
    EXPECT_DOUBLE_EQ(transpiration[0], transpiration[1]);
    EXPECT_DOUBLE_EQ(swc[0], swc[1]);


    // Cleanup
    SW_CTL_clear_model(swTRUE, &run_vt1);
    SW_CTL_clear_model(swTRUE, &run_vt2);
}

} // namespace
