#include <gmock/gmock.h>
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

#include "include/generic.h"
#include "include/myMemory.h"
#include "include/filefuncs.h"
#include "include/rands.h"
#include "include/Times.h"
#include "include/SW_Defines.h"
#include "include/SW_Times.h"
#include "include/SW_Files.h"
#include "include/SW_Carbon.h"
#include "include/SW_Site.h"
#include "include/SW_VegProd.h"
#include "include/SW_VegEstab.h"
#include "include/SW_Model.h"
#include "include/SW_SoilWater.h"
#include "include/SW_Weather.h"
#include "include/SW_Markov.h"
#include "include/SW_Sky.h"
#include "include/SW_Main_lib.h"

#include "tests/gtests/sw_testhelpers.h"

using ::testing::HasSubstr;


static void assert_decreasing_SWPcrit(SW_VEGPROD* SW_VegProd);
static void assert_decreasing_SWPcrit(SW_VEGPROD* SW_VegProd)
{
	int rank, vegtype;

	for (rank = 0; rank < NVEGTYPES - 1; rank++)
	{
		vegtype = SW_VegProd->rank_SWPcrits[rank];

		/*
		swprintf("Rank=%d is vegtype=%d with SWPcrit=%f\n",
			rank, vegtype,
			SW_VegProd.critSoilWater[vegtype]);
		*/

		// Check that SWPcrit of `vegtype` is larger or equal to
		// SWPcrit of the vegetation type with the next larger rank
		ASSERT_GE(
			SW_VegProd->critSoilWater[vegtype],
			SW_VegProd->critSoilWater[SW_VegProd->rank_SWPcrits[rank + 1]]);
	}
}


// Vegetation cover: see `estimatePotNatVegComposition()`
// RelAbundanceL0 and inputValues indices
int succIndex = 0;
int forbIndex = 1;
int C3Index = 2;
int C4Index = 3;
int grassAnn = 4;
int shrubIndex = 5;
int treeIndex = 6;
int bareGround = 7;

// RelAbundanceL1 indices
int treeIndexL1 = 0;
int shrubIndexL1 = 1;
int forbIndexL1 = 2;
int grassesIndexL1 = 3;
int bareGroundL1 = 4;


static void copyL0(double outL0[], double inL0[]) {
  for (int index = 0; index < 8; index++) {
    outL0[index] = inL0[index];
  }
}

static void calcVegCoverL1FromL0(double L1[], double L0[]) {
  L1[treeIndexL1] = L0[treeIndex];
  L1[shrubIndexL1] = L0[shrubIndex];
  L1[forbIndexL1] = L0[forbIndex] + L0[succIndex];
  L1[grassesIndexL1] = L0[C3Index] + L0[C4Index] + L0[grassAnn];
  L1[bareGroundL1] = L0[bareGround];
}

static void calcGrassCoverFromL0(double grass[], double L0[]) {
  double grass_sum = L0[C3Index] + L0[C4Index] + L0[grassAnn];

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


namespace {
  int k;

  // Test the SW_VEGPROD constructor 'SW_VPD_construct'
  TEST_F(VegProdFixtureTest, VegProdConstructor) {
    // This test requires a local copy of SW_VEGPROD to avoid a memory leak
    // (see issue #205)
    // -- If using `SW_All.VegProd` or a global variable
    // (for which `SW_VPD_construct()` has already been called once, e.g.,
    // during the test fixture's `SetUp()`), then this (second) call to
    // `SW_VPD_construct()` would allocate memory a second time
    // while `SW_VPD_deconstruct()` will de-allocate memory only once
    // (the call to `SW_VPD_deconstruct()`during the test fixture's `TearDown()`
    // would see only NULL and thus not de-allocate the required second time
    // to avoid a leak)
    SW_VEGPROD SW_VegProd;

    SW_VPD_construct(&SW_VegProd, &LogInfo); // allocates memory

    SW_VPD_init_run(
      &SW_VegProd,
      &SW_All.Weather,
      &SW_All.Model,
      SW_All.Site.latitude,
      &LogInfo
    );

    ForEachVegType(k) {
      EXPECT_DOUBLE_EQ(1., SW_VegProd.veg[k].co2_multipliers[BIO_INDEX][0]);
      EXPECT_DOUBLE_EQ(1., SW_VegProd.veg[k].co2_multipliers[BIO_INDEX][MAX_NYEAR - 1]);

      EXPECT_DOUBLE_EQ(1., SW_VegProd.veg[k].co2_multipliers[WUE_INDEX][0]);
      EXPECT_DOUBLE_EQ(1., SW_VegProd.veg[k].co2_multipliers[WUE_INDEX][MAX_NYEAR - 1]);
    }

    SW_VPD_deconstruct(&SW_VegProd);
  }


  // Test the application of the biomass CO2-effect
  TEST(VegProdTest, VegProdBiomassCO2effect) {
    int i;
    double x = 1.5;
    double biom1[12], biom2[12];

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

    RealD transp_coeff[NVEGTYPES][MAX_LAYERS];


    for(vegIndex = 0; vegIndex < NVEGTYPES; vegIndex++) {
      transp_coeff[vegIndex][0] = 0.;
    }

    EXPECT_DOUBLE_EQ(sum_across_vegtypes(transp_coeff, 0), 0.);

    for(vegIndex = 0; vegIndex < NVEGTYPES; vegIndex++) {
      transp_coeff[vegIndex][0] = 0.25;
    }

    EXPECT_DOUBLE_EQ(sum_across_vegtypes(transp_coeff, 0), 1.);
  }


	// Check `get_critical_rank`
	TEST_F(VegProdFixtureTest, VegProdrank) {
		int k;
		// Check `get_critical_rank` for normal inputs, e.g., -2.0, -2.0, -3.5, -3.9
		get_critical_rank(&SW_All.VegProd);
		assert_decreasing_SWPcrit(&SW_All.VegProd);


		// Check `get_critical_rank` for constant values
		ForEachVegType(k)
		{
			SW_All.VegProd.critSoilWater[k] = 0.;
		}

		get_critical_rank(&SW_All.VegProd);
		assert_decreasing_SWPcrit(&SW_All.VegProd);


		// Check `get_critical_rank` for increasing values
		ForEachVegType(k)
		{
			SW_All.VegProd.critSoilWater[k] = k;
		}

		get_critical_rank(&SW_All.VegProd);
		assert_decreasing_SWPcrit(&SW_All.VegProd);


		// Check `get_critical_rank` for decreasing values
		ForEachVegType(k)
		{
			SW_All.VegProd.critSoilWater[k] = NVEGTYPES - k;
		}

		get_critical_rank(&SW_All.VegProd);
		assert_decreasing_SWPcrit(&SW_All.VegProd);
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
        SW_VEGPROD vegProd;

        double inputValues[8];
        double shrubLimit = .2;

        // Array holding only grass values
        double grassOutput[3]; // 3 = Number of grass variables

        // Array holding all values from the estimation
        double RelAbundanceL0[8]; // 8 = Number of types

        // Array holding all values from estimation minus grasses
        double RelAbundanceL1[5]; // 5 = Number of types minus grasses

        double SumGrassesFraction = SW_MISSING;
        double C4Variables[3];

        int veg_method = 1;
        double latitude = 90.0;

        Bool fillEmptyWithBareGround = swTRUE;
        Bool warnExtrapolation = swTRUE;
        Bool inNorthHem = swTRUE;
        Bool fixBareGround = swTRUE;

        int nTypes = 8;
        int index;


        double RelAbundanceL0Expected[8];
        double RelAbundanceL1Expected[5];
        double grassOutputExpected[3];

        SW_All.Model.startyr = 1980;
        SW_All.Model.endyr = 2010;

        // Reset "SW_All.Weather.allHist"
        SW_WTH_read(&SW_All.Weather, &SW_All.Sky, &SW_All.Model, &LogInfo);
		    finalizeAllWeather(&SW_All.Markov, &SW_All.Weather,
            SW_All.Model.cum_monthdays, SW_All.Model.days_in_month, &LogInfo);

        // Allocate arrays needed for `calcSiteClimate()` and `averageClimateAcrossYears()`
        allocateClimateStructs(31, &climateOutput, &climateAverages, &LogInfo);

        // Calculate climate of the site and add results to "climateOutput"
        calcSiteClimate(SW_All.Weather.allHist, SW_All.Model.cum_monthdays,
            SW_All.Model.days_in_month, 31, 1980, inNorthHem, &climateOutput);

        // Average values from "climateOutput" and put them in "climateAverages"
        averageClimateAcrossYears(&climateOutput, 31, &climateAverages);

        // Set C4 results, standard deviations are not needed for estimating vegetation
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
         * NOTE: Command uses deprecated estimate_PotNatVeg_composition (rSOILWAT >= v.6.0.0)
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
        estimatePotNatVegComposition(climateAverages.meanTemp_C, climateAverages.PPT_cm,
            climateAverages.meanTempMon_C, climateAverages.PPTMon_cm, inputValues, shrubLimit,
            SumGrassesFraction, C4Variables, fillEmptyWithBareGround, inNorthHem, warnExtrapolation,
            fixBareGround, grassOutput, RelAbundanceL0, RelAbundanceL1, &LogInfo);


        // Loop through RelAbundanceL0 and test results
        for(index = 0; index < nTypes; index++) {
            EXPECT_NEAR(RelAbundanceL0[index], RelAbundanceL0Expected[index], tol6);
        }

        // Loop through RelAbundanceL1 and test results
        for(index = 0; index < 5; index++) {
            EXPECT_NEAR(RelAbundanceL1[index], RelAbundanceL1Expected[index], tol6);
        }

        // Loop through grassOutput and test results
        for(index = 0; index < 3; index++) {
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
         * NOTE: Command uses deprecated estimate_PotNatVeg_composition (rSOILWAT >= v.6.0.0)
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
        estimatePotNatVegComposition(climateAverages.meanTemp_C, climateAverages.PPT_cm,
            climateAverages.meanTempMon_C, climateAverages.PPTMon_cm, inputValues, shrubLimit,
            SumGrassesFraction, C4Variables, fillEmptyWithBareGround, inNorthHem, warnExtrapolation,
            fixBareGround, grassOutput, RelAbundanceL0, RelAbundanceL1, &LogInfo);

        // Loop through RelAbundanceL0 and test results
        for(index = 0; index < 8; index++) {
            EXPECT_DOUBLE_EQ(RelAbundanceL0[index], RelAbundanceL0Expected[index]);
        }

        // Loop through RelAbundanceL1 and test results
        for(index = 0; index < 5; index++) {
            EXPECT_DOUBLE_EQ(RelAbundanceL1[index], RelAbundanceL1Expected[index]);
        }

        // Loop through grassOutput and test results
        for(index = 0; index < 3; index++) {
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
         * NOTE: Command uses deprecated estimate_PotNatVeg_composition (rSOILWAT >= v.6.0.0)
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
        estimatePotNatVegComposition(climateAverages.meanTemp_C, climateAverages.PPT_cm,
            climateAverages.meanTempMon_C, climateAverages.PPTMon_cm, inputValues, shrubLimit,
            SumGrassesFraction, C4Variables, fillEmptyWithBareGround, inNorthHem, warnExtrapolation,
            fixBareGround, grassOutput, RelAbundanceL0, RelAbundanceL1, &LogInfo);

        // Loop through RelAbundanceL0 and test results.
        for(index = 0; index < nTypes; index++) {
            EXPECT_DOUBLE_EQ(RelAbundanceL0[index], RelAbundanceL0Expected[index]);
        }

        // Loop through RelAbundanceL1 and test results
        for(index = 0; index < 5; index++) {
            EXPECT_DOUBLE_EQ(RelAbundanceL1[index], RelAbundanceL1Expected[index]);
        }

        // Loop through grassOutput and test results
        for(index = 0; index < 3; index++) {
            EXPECT_NEAR(grassOutput[index], grassOutputExpected[index], tol6);
        }



        /*  ===============================================================
             Test `estimateVegetationFromClimate()` when "veg_method" is 1
             using default values of the function:
             [SW_MISSING, SW_MISSING, SW_MISSING, SW_MISSING, 0.0, SW_MISSING, 0.0, 0.0]
            ===============================================================  */

        /* Expect identical output to rSOILWAT2 (e.g., v5.3.1)
         * NOTE: Command uses deprecated estimate_PotNatVeg_composition (rSOILWAT >= v.6.0.0)
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
        RelAbundanceL1Expected[forbIndexL1] = .2608391; // Constains forbs + succulents (L0)
        RelAbundanceL1Expected[grassesIndexL1] = .4307061;
        RelAbundanceL1Expected[bareGroundL1] = 0.;


        estimateVegetationFromClimate(&vegProd, SW_All.Weather.allHist,
                                      &SW_All.Model, veg_method, latitude, &LogInfo);

        // Loop through RelAbundanceL1 and test results
        for(index = 0; index < 4; index++) {
            EXPECT_NEAR(vegProd.veg[index].cov.fCover, RelAbundanceL1Expected[index], tol6);
        }

        EXPECT_NEAR(vegProd.bare_cov.fCover, RelAbundanceL1Expected[bareGroundL1], tol6);



        /*  ===============================================================
         Tests for southern hemisphere:

         1) Same input values as previous test except for trees and bare ground which
         are both .0549

         2) Default input values:
         [SW_MISSING, SW_MISSING, SW_MISSING, SW_MISSING, 0.0, SW_MISSING, 0.0, 0.0]
         yielding different values in southern hemisphere compared to northern hemisphere
            ===============================================================  */

        // Recalculate climate of the site in southern hemisphere and add results to "climateOutput"
        inNorthHem = swFALSE;
        calcSiteClimate(SW_All.Weather.allHist, SW_All.Model.cum_monthdays,
            SW_All.Model.days_in_month, 31, 1980, inNorthHem, &climateOutput);


        inputValues[treeIndex] = .0549;
        inputValues[bareGround] = .0549;

        /* Expect identical output to rSOILWAT2 (e.g., v5.3.1)
         * NOTE: Command uses deprecated estimate_PotNatVeg_composition (rSOILWAT >= v.6.0.0)
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
        estimatePotNatVegComposition(climateAverages.meanTemp_C, climateAverages.PPT_cm,
            climateAverages.meanTempMon_C, climateAverages.PPTMon_cm, inputValues, shrubLimit,
            SumGrassesFraction, C4Variables, fillEmptyWithBareGround, inNorthHem, warnExtrapolation,
            fixBareGround, grassOutput, RelAbundanceL0, RelAbundanceL1, &LogInfo);

        // Loop through RelAbundanceL0 and test results.
        for(index = 0; index < 8; index++) {
            EXPECT_DOUBLE_EQ(RelAbundanceL0[index], RelAbundanceL0Expected[index]);
        }

        // Loop through RelAbundanceL1 and test results
        for(index = 0; index < 5; index++) {
            EXPECT_DOUBLE_EQ(RelAbundanceL1[index], RelAbundanceL1Expected[index]);
        }

        // Loop through grassOutput and test results
        for(index = 0; index < 3; index++) {
            EXPECT_NEAR(grassOutput[index], grassOutputExpected[index], tol6);
        }



        /*  ===============================================================
         Test "C4Variables" not being defined (faked by setting july min (index zero) to SW_MISSING)
         Use southern hemisphere for clear difference in values (C4 is/isn't defined)
         Use default values:
         [SW_MISSING, SW_MISSING, SW_MISSING, SW_MISSING, 0.0, SW_MISSING, 0.0, 0.0]
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
         * NOTE: Command uses deprecated estimate_PotNatVeg_composition (rSOILWAT >= v.6.0.0)
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
        estimatePotNatVegComposition(climateAverages.meanTemp_C, climateAverages.PPT_cm,
            climateAverages.meanTempMon_C, climateAverages.PPTMon_cm, inputValues, shrubLimit,
            SumGrassesFraction, C4Variables, fillEmptyWithBareGround, inNorthHem, warnExtrapolation,
            fixBareGround, grassOutput, RelAbundanceL0, RelAbundanceL1, &LogInfo);


        // Loop through RelAbundanceL0 and test results.
        for(index = 0; index < nTypes; index++) {
            EXPECT_NEAR(RelAbundanceL0[index], RelAbundanceL0Expected[index], tol6);
        }

        // Loop through RelAbundanceL1 and test results
        for(index = 0; index < 5; index++) {
            EXPECT_NEAR(RelAbundanceL1[index], RelAbundanceL1Expected[index], tol6);
        }

        // Loop through grassOutput and test results
        for(index = 0; index < 3; index++) {
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
        int nTypes = 8;

        double inputValues[8];
        double shrubLimit = .2;

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
        Bool inNorthHem = swTRUE;
        Bool warnExtrapolation = swTRUE;
        Bool fixBareGround = swTRUE;


        // Reset "SW_All.Weather.allHist"
        SW_WTH_read(&SW_All.Weather, &SW_All.Sky, &SW_All.Model, &LogInfo);
		    finalizeAllWeather(&SW_All.Markov, &SW_All.Weather,
            SW_All.Model.cum_monthdays, SW_All.Model.days_in_month, &LogInfo);

        // Allocate arrays needed for `calcSiteClimate()` and `averageClimateAcrossYears()`
        allocateClimateStructs(31, &climateOutput, &climateAverages, &LogInfo);

        // Calculate climate of the site and add results to "climateOutput"
        calcSiteClimate(SW_All.Weather.allHist, SW_All.Model.cum_monthdays,
            SW_All.Model.days_in_month, 31, 1980, inNorthHem, &climateOutput);

        // Average values from "climateOutput" and put them in "climateAverages"
        averageClimateAcrossYears(&climateOutput, 31, &climateAverages);

        // Set C4 results, standard deviations are not needed for estimating vegetation
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
        estimatePotNatVegComposition(climateAverages.meanTemp_C, climateAverages.PPT_cm,
            climateAverages.meanTempMon_C, climateAverages.PPTMon_cm, inputValues, shrubLimit,
            SumGrassesFraction, C4Variables, fillEmptyWithBareGround, inNorthHem, warnExtrapolation,
            fixBareGround, grassOutput, RelAbundanceL0, RelAbundanceL1, &LogInfo);


        // Loop through RelAbundanceL0 and test results.
        for(index = 0; index < nTypes; index++) {
            // All values in "RelAbundanceL0" should be exactly the same as "inputValues"
            EXPECT_DOUBLE_EQ(RelAbundanceL0[index], inputValues[index]);
            EXPECT_NEAR(RelAbundanceL0[index], RelAbundanceL0Expected[index], tol3);
        }

        // Loop through RelAbundanceL1 and test results
        for(index = 0; index < 5; index++) {
            EXPECT_DOUBLE_EQ(RelAbundanceL1[index], RelAbundanceL1Expected[index]);
        }

        // Loop through grassOutput and test results
        for(index = 0; index < 3; index++) {
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
         * NOTE: Command uses deprecated estimate_PotNatVeg_composition (rSOILWAT >= v.6.0.0)
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
        estimatePotNatVegComposition(climateAverages.meanTemp_C, climateAverages.PPT_cm,
            climateAverages.meanTempMon_C, climateAverages.PPTMon_cm, inputValues, shrubLimit,
            SumGrassesFraction, C4Variables, fillEmptyWithBareGround, inNorthHem, warnExtrapolation,
            fixBareGround, grassOutput, RelAbundanceL0, RelAbundanceL1, &LogInfo);


        // Loop through RelAbundanceL0 and test results.
        for(index = 0; index < nTypes; index++) {
            EXPECT_DOUBLE_EQ(RelAbundanceL0[index], RelAbundanceL0Expected[index]);
        }

        // Loop through RelAbundanceL1 and test results
        for(index = 0; index < 5; index++) {
            EXPECT_DOUBLE_EQ(RelAbundanceL1[index], RelAbundanceL1Expected[index]);
        }

        // Loop through grassOutput and test results
        for(index = 0; index < 3; index++) {
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
         * NOTE: Command uses deprecated estimate_PotNatVeg_composition (rSOILWAT >= v.6.0.0)
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
        estimatePotNatVegComposition(climateAverages.meanTemp_C, climateAverages.PPT_cm,
            climateAverages.meanTempMon_C, climateAverages.PPTMon_cm, inputValues, shrubLimit,
            SumGrassesFraction, C4Variables, fillEmptyWithBareGround, inNorthHem, warnExtrapolation,
            fixBareGround, grassOutput, RelAbundanceL0, RelAbundanceL1, &LogInfo);

        // Loop through RelAbundanceL0 and test results.
        for(index = 0; index < nTypes; index++) {
            EXPECT_DOUBLE_EQ(RelAbundanceL0[index], RelAbundanceL0Expected[index]);
        }

        // Loop through RelAbundanceL1 and test results
        for(index = 0; index < 5; index++) {
            EXPECT_DOUBLE_EQ(RelAbundanceL1[index], RelAbundanceL1Expected[index]);
        }

        // Loop through grassOutput and test results
        for(index = 0; index < 3; index++) {
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
         * NOTE: Command uses deprecated estimate_PotNatVeg_composition (rSOILWAT >= v.6.0.0)
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
        estimatePotNatVegComposition(climateAverages.meanTemp_C, climateAverages.PPT_cm,
            climateAverages.meanTempMon_C, climateAverages.PPTMon_cm, inputValues, shrubLimit,
            SumGrassesFraction, C4Variables, fillEmptyWithBareGround, inNorthHem, warnExtrapolation,
            fixBareGround, grassOutput, RelAbundanceL0, RelAbundanceL1, &LogInfo);

        // Loop through RelAbundanceL0 and test results.
        for(index = 0; index < nTypes; index++) {
            EXPECT_DOUBLE_EQ(RelAbundanceL0[index], RelAbundanceL0Expected[index]);
        }

        // Loop through RelAbundanceL1 and test results
        for(index = 0; index < 5; index++) {
            EXPECT_DOUBLE_EQ(RelAbundanceL1[index], RelAbundanceL1Expected[index]);
        }

        // Loop through grassOutput and test results
        for(index = 0; index < 3; index++) {
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
         * NOTE: Command uses deprecated estimate_PotNatVeg_composition (rSOILWAT >= v.6.0.0)
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
        estimatePotNatVegComposition(climateAverages.meanTemp_C, climateAverages.PPT_cm,
            climateAverages.meanTempMon_C, climateAverages.PPTMon_cm, inputValues, shrubLimit,
            SumGrassesFraction, C4Variables, fillEmptyWithBareGround, inNorthHem, warnExtrapolation,
            fixBareGround, grassOutput, RelAbundanceL0, RelAbundanceL1, &LogInfo);

        // Loop through RelAbundanceL0 and test results.
        for(index = 0; index < nTypes; index++) {
            EXPECT_DOUBLE_EQ(RelAbundanceL0[index], RelAbundanceL0Expected[index]);
        }

        // Loop through RelAbundanceL1 and test results
        for(index = 0; index < 5; index++) {
            EXPECT_DOUBLE_EQ(RelAbundanceL1[index], RelAbundanceL1Expected[index]);
        }

        // Loop through grassOutput and test results
        for(index = 0; index < 3; index++) {
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
         * NOTE: Command uses deprecated estimate_PotNatVeg_composition (rSOILWAT >= v.6.0.0)
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
        estimatePotNatVegComposition(climateAverages.meanTemp_C, climateAverages.PPT_cm,
            climateAverages.meanTempMon_C, climateAverages.PPTMon_cm, inputValues, shrubLimit,
            SumGrassesFraction, C4Variables, fillEmptyWithBareGround, inNorthHem, warnExtrapolation,
            fixBareGround, grassOutput, RelAbundanceL0, RelAbundanceL1, &LogInfo);


        // Loop through RelAbundanceL0 and test results.
        for(index = 0; index < nTypes; index++) {
            EXPECT_DOUBLE_EQ(RelAbundanceL0[index], RelAbundanceL0Expected[index]);
        }

        // Loop through RelAbundanceL1 and test results
        for(index = 0; index < 5; index++) {
            EXPECT_DOUBLE_EQ(RelAbundanceL1[index], RelAbundanceL1Expected[index]);
        }

        // Loop through grassOutput and test results
        for(index = 0; index < 3; index++) {
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

        double SumGrassesFraction = SW_MISSING;
        double C4Variables[3];

        Bool fillEmptyWithBareGround = swTRUE;
        Bool inNorthHem = swTRUE;
        Bool warnExtrapolation = swTRUE;
        Bool fixBareGround = swTRUE;

        double inputValues[8] = {.0567, .5, .0392, .0981,
                                .3218, .0827, .1293, .0405};
        double shrubLimit = .2;

        // Array holding only grass values
        double grassOutput[3]; // 3 = Number of grass variables

        // Array holding all values from the estimation
        double RelAbundanceL0[8]; // 8 = Number of types

        // Array holding all values from estimation minus grasses
        double RelAbundanceL1[5]; // 5 = Number of types minus grasses

        // Allocate arrays needed for `calcSiteClimate()` and `averageClimateAcrossYears()`
        allocateClimateStructs(31, &climateOutput, &climateAverages, &LogInfo);

        /*  ===============================================================
         Test for fail when input sum is greater than one with the values:
         [.0567, .5, .0392, .0981, .3218, .0827, .1293, .0405]
            ===============================================================  */


        // Reset "SW_All.Weather.allHist"
        SW_WTH_read(&SW_All.Weather, &SW_All.Sky, &SW_All.Model, &LogInfo);
        finalizeAllWeather(&SW_All.Markov, &SW_All.Weather,
            SW_All.Model.cum_monthdays, SW_All.Model.days_in_month, &LogInfo);

        // Calculate climate of the site and add results to "climateOutput"
        calcSiteClimate(SW_All.Weather.allHist, SW_All.Model.cum_monthdays,
            SW_All.Model.days_in_month, 31, 1980, inNorthHem, &climateOutput);

        // Average values from "climateOutput" and put them in "climateAverages"
        averageClimateAcrossYears(&climateOutput, 31, &climateAverages);

        estimatePotNatVegComposition(
          climateAverages.meanTemp_C, climateAverages.PPT_cm,
                climateAverages.meanTempMon_C, climateAverages.PPTMon_cm, inputValues, shrubLimit,
                SumGrassesFraction, C4Variables, fillEmptyWithBareGround, inNorthHem, warnExtrapolation,
                fixBareGround, grassOutput, RelAbundanceL0, RelAbundanceL1, &LogInfo
        );

        // Detect failure by error message
        EXPECT_THAT(LogInfo.errorMsg,
                    HasSubstr("User defined relative abundance values sum to more"\
                    " than 1 = full land cover"));

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

        Bool fillEmptyWithBareGround = swTRUE;
        Bool inNorthHem = swTRUE;
        Bool warnExtrapolation = swTRUE;
        Bool fixBareGround = swTRUE;

        double inputValues[8];
        double shrubLimit = .2;

        // Array holding only grass values
        double grassOutput[3]; // 3 = Number of grass variables

        // Array holding all values from the estimation
        double RelAbundanceL0[8]; // 8 = Number of types

        // Array holding all values from estimation minus grasses
        double RelAbundanceL1[5]; // 5 = Number of types minus grasses

        // Allocate arrays needed for `calcSiteClimate()` and `averageClimateAcrossYears()`
        allocateClimateStructs(31, &climateOutput, &climateAverages, &LogInfo);


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
         [.0567, .25, .SW_MISSING, SW_MISSING, .0912, .0465, .1293, .0405], input sum = .6142
         SumGrassesFraction = .5, total input sum: 1.023.
         Total input sum is 1.1211 instead of 1.1142, because annual grass
         is already defined, so that value is subtracted from SumGrassesFraction and
         added to the initial input sum
            ===============================================================  */

        // Reset "SW_All.Weather.allHist"
        SW_WTH_read(&SW_All.Weather, &SW_All.Sky, &SW_All.Model, &LogInfo);
        finalizeAllWeather(&SW_All.Markov, &SW_All.Weather,
            SW_All.Model.cum_monthdays, SW_All.Model.days_in_month, &LogInfo);

        // Calculate climate of the site and add results to "climateOutput"
        calcSiteClimate(SW_All.Weather.allHist, SW_All.Model.cum_monthdays,
            SW_All.Model.days_in_month, 31, 1980, inNorthHem, &climateOutput);

        // Average values from "climateOutput" and put them in "climateAverages"
        averageClimateAcrossYears(&climateOutput, 31, &climateAverages);

        estimatePotNatVegComposition(
          climateAverages.meanTemp_C, climateAverages.PPT_cm,
                climateAverages.meanTempMon_C, climateAverages.PPTMon_cm, inputValues, shrubLimit,
                SumGrassesFraction, C4Variables, fillEmptyWithBareGround, inNorthHem, warnExtrapolation,
                fixBareGround, grassOutput, RelAbundanceL0, RelAbundanceL1, &LogInfo
        );

        // Detect failure by error message
        EXPECT_THAT(LogInfo.errorMsg,
                    HasSubstr("User defined relative abundance values sum to more"\
                    " than 1 = full land cover"));

        // Free allocated data
        deallocateClimateStructs(&climateOutput, &climateAverages);
    }

} // namespace
