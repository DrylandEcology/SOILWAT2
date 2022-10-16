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




static void assert_decreasing_SWPcrit(void);
static void assert_decreasing_SWPcrit(void)
{
	int rank, vegtype;

	for (rank = 0; rank < NVEGTYPES - 1; rank++)
	{
		vegtype = SW_VegProd.rank_SWPcrits[rank];

		/*
		swprintf("Rank=%d is vegtype=%d with SWPcrit=%f\n",
			rank, vegtype,
			SW_VegProd.critSoilWater[vegtype]);
		*/

		// Check that SWPcrit of `vegtype` is larger or equal to
		// SWPcrit of the vegetation type with the next larger rank
		ASSERT_GE(
			SW_VegProd.critSoilWater[vegtype],
			SW_VegProd.critSoilWater[SW_VegProd.rank_SWPcrits[rank + 1]]);
	}
}


namespace {
  SW_VEGPROD *v = &SW_VegProd;
  int k;

  // Test the SW_VEGPROD constructor 'SW_VPD_construct'
  TEST(VegTest, Constructor) {
    SW_VPD_construct();
    SW_VPD_init_run();

    ForEachVegType(k) {
      EXPECT_DOUBLE_EQ(1., v->veg[k].co2_multipliers[BIO_INDEX][0]);
      EXPECT_DOUBLE_EQ(1., v->veg[k].co2_multipliers[BIO_INDEX][MAX_NYEAR - 1]);

      EXPECT_DOUBLE_EQ(1., v->veg[k].co2_multipliers[WUE_INDEX][0]);
      EXPECT_DOUBLE_EQ(1., v->veg[k].co2_multipliers[WUE_INDEX][MAX_NYEAR - 1]);
    }

    // Reset to previous global state
    Reset_SOILWAT2_after_UnitTest();
  }


  // Test the application of the biomass CO2-effect
  TEST(VegTest, BiomassCO2effect) {
    int i;
    double x;
    double biom1[12], biom2[12];

    for (i = 0; i < 12; i++) {
      biom1[i] = i + 1.;
    }

    // One example
    x = v->veg[SW_GRASS].co2_multipliers[BIO_INDEX][SW_Model.startyr + SW_Model.addtl_yr];
    apply_biomassCO2effect(biom2, biom1, x);

    for (i = 0; i < 12; i++) {
      EXPECT_DOUBLE_EQ(biom2[i], biom1[i] * x);
    }

    // Reset to previous global state
    Reset_SOILWAT2_after_UnitTest();
  }


  // Test summing values across vegetation types
  TEST(VegTest, Summing) {
    double x0[NVEGTYPES] = {0.};
    double x1[NVEGTYPES] = {0.25, 0.25, 0.25, 0.25};

    EXPECT_DOUBLE_EQ(sum_across_vegtypes(x0), 0.);
    EXPECT_DOUBLE_EQ(sum_across_vegtypes(x1), 1.);
  }


	// Check `get_critical_rank`
	TEST(VegTest, rank) {
		int k;
		// Check `get_critical_rank` for normal inputs, e.g., -2.0, -2.0, -3.5, -3.9
		get_critical_rank();
		assert_decreasing_SWPcrit();


		// Check `get_critical_rank` for constant values
		ForEachVegType(k)
		{
			SW_VegProd.critSoilWater[k] = 0.;
		}

		get_critical_rank();
		assert_decreasing_SWPcrit();


		// Check `get_critical_rank` for increasing values
		ForEachVegType(k)
		{
			SW_VegProd.critSoilWater[k] = k;
		}

		get_critical_rank();
		assert_decreasing_SWPcrit();


		// Check `get_critical_rank` for decreasing values
		ForEachVegType(k)
		{
			SW_VegProd.critSoilWater[k] = NVEGTYPES - k;
		}

		get_critical_rank();
		assert_decreasing_SWPcrit();


		// Reset to previous global state
		Reset_SOILWAT2_after_UnitTest();
	}

    TEST(EstimateVegetationTest, NotFullVegetation) {

        /*  ================================================================
            This block of tests deals with input values to
            `estimatePotNatVegComposition()` that do not add up to 1
            ================================================================  */


        SW_CLIMATE_YEARLY climateOutput;
        SW_CLIMATE_CLIM climateAverages;

        double inputValues[8] = {SW_MISSING, SW_MISSING, SW_MISSING, SW_MISSING,
                                0.0, SW_MISSING, 0.0, 0.0};
        double shrubLimit = .2;

        // Array holding only grass values
        double grassOutput[3]; // 3 = Number of grass variables

        // Array holding all values from the estimation
        double RelAbundanceL0[8]; // 8 = Number of types

        // Array holding all values from estimation minus grasses
        double RelAbundanceL1[5]; // 5 = Number of types minus grasses

        double SumGrassesFraction = SW_MISSING;
        double C4Variables[3];

        Bool fillEmptyWithBareGround = swTRUE;
        Bool warnExtrapolation = swTRUE;
        Bool inNorthHem = swTRUE;

        int deallocate = 0;
        int allocate = 1;
        int index;

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
        int forbIndexL1 = 1;
        int shrubIndexL1 = 2;
        int grassesIndexL1 = 3;
        int bareGroundL1 = 4;

        double RelAbundanceL0Expected[8] = {0.0, 0.2608391, 0.4307062,
                                                0.0, 0.0, 0.3084547, 0.0, 0.0};
        double RelAbundanceL1Expected[5] = {0.0, 0.3084547, 0.2608391, 0.4307062, 0.0};

        // Reset "SW_Weather.allHist"
        SW_WTH_read();

        // Allocate arrays needed for `calcSiteClimate()` and `averageClimateAcrossYears()`
        allocDeallocClimateStructs(allocate, 31, &climateOutput, &climateAverages);

        // Calculate climate of the site and add results to "climateOutput"
        calcSiteClimate(SW_Weather.allHist, 31, 1980, inNorthHem, &climateOutput);

        // Average values from "climateOutput" and put them in "climateAverages"
        averageClimateAcrossYears(&climateOutput, 31, &climateAverages);

        // Set C4 results, standard deviations are not needed for estimating vegetation
        C4Variables[0] = climateAverages.minTemp7thMon_C;
        C4Variables[1] = climateAverages.ddAbove65F_degday;
        C4Variables[2] = climateAverages.frostFree_days;

        // Estimate vegetation based off calculated variables and "inputValues"
        estimatePotNatVegComposition(climateAverages.meanTemp_C, climateAverages.PPT_cm,
            climateAverages.meanTempMon_C, climateAverages.PPTMon_cm, inputValues, shrubLimit,
            SumGrassesFraction, C4Variables, fillEmptyWithBareGround, inNorthHem, warnExtrapolation,
            grassOutput, RelAbundanceL0, RelAbundanceL1);

        /*  ===============================================================
                     Test when all input values are "SW_MISSING"
            ===============================================================  */

        /* Expect identical output to rSOILWAT2 (e.g., v5.3.1)
         * NOTE: Command uses deprecated estimate_PotNatVeg_composition (rSOILWAT >= v.6.0.0)
         ```{r}
           rSOILWAT2::estimate_PotNatVeg_composition_old(
             MAP_mm =  10 * clim1[["MAP_cm"]], MAT_C = clim1[["MAT_C"]],
             mean_monthly_ppt_mm = 10 * clim1[["meanMonthlyPPTcm"]],
             mean_monthly_Temp_C = clim1[["meanMonthlyTempC"]]
           )
         ```
         */

        // Loop through RelAbundanceL0 and test results
        for(index = 0; index < 8; index++) {
            EXPECT_NEAR(RelAbundanceL0[index], RelAbundanceL0Expected[index], tol6);
        }

        // Loop through RelAbundanceL1 and test results
        for(index = 0; index < 5; index++) {
            EXPECT_NEAR(RelAbundanceL1[index], RelAbundanceL1Expected[index], tol6);
        }
        
        EXPECT_DOUBLE_EQ(grassOutput[0], 1.);
        EXPECT_DOUBLE_EQ(grassOutput[1], 0.);
        EXPECT_DOUBLE_EQ(grassOutput[2], 0.);

        /*  ===============================================================
                     Test with half of input values not "SW_MISSING"
            ===============================================================  */
        inputValues[succIndex] = .376;
        inputValues[forbIndex] = SW_MISSING;
        inputValues[C3Index] = .096;
        inputValues[C4Index] = SW_MISSING;
        inputValues[grassAnn] = SW_MISSING;
        inputValues[shrubIndex] = .1098;
        inputValues[treeIndex] = .0372;
        inputValues[bareGround] = SW_MISSING;

        /* Expect identical output to rSOILWAT2 (e.g., v5.3.1)
         * NOTE: Command uses deprecated estimate_PotNatVeg_composition (rSOILWAT >= v.6.0.0)
         ```{r}
           rSOILWAT2::estimate_PotNatVeg_composition_old(
             MAP_mm =  10 * clim1[["MAP_cm"]], MAT_C = clim1[["MAT_C"]],
             mean_monthly_ppt_mm = 10 * clim1[["meanMonthlyPPTcm"]],
             mean_monthly_Temp_C = clim1[["meanMonthlyTempC"]],
             Succulents_Fraction = .376, fix_succulents = TRUE,
             C3_Fraction = .096, fix_C3grasses = TRUE,
             Shrubs_Fraction = .1098, fix_shrubs = TRUE,
             Trees_Fraction = .0372, fix_trees = TRUE
           )
         ```
         */

        RelAbundanceL0Expected[succIndex] = 0.3760;
        RelAbundanceL0Expected[forbIndex] = 0.3810;
        RelAbundanceL0Expected[C3Index] = 0.0960;
        RelAbundanceL0Expected[C4Index] = 0.0000;
        RelAbundanceL0Expected[grassAnn] = 0.0000;
        RelAbundanceL0Expected[shrubIndex] = 0.1098;
        RelAbundanceL0Expected[treeIndex] = 0.0372;
        RelAbundanceL0Expected[bareGround] = 0.0000;

        RelAbundanceL1Expected[treeIndexL1] = 0.0372;
        RelAbundanceL1Expected[forbIndexL1] = 0.1098;
        RelAbundanceL1Expected[shrubIndexL1] = 0.7570;
        RelAbundanceL1Expected[grassesIndexL1] = 0.0960;
        RelAbundanceL1Expected[bareGroundL1] = 0.0000;

        estimatePotNatVegComposition(climateAverages.meanTemp_C, climateAverages.PPT_cm,
            climateAverages.meanTempMon_C, climateAverages.PPTMon_cm, inputValues, shrubLimit,
            SumGrassesFraction, C4Variables, fillEmptyWithBareGround, inNorthHem, warnExtrapolation,
            grassOutput, RelAbundanceL0, RelAbundanceL1);
        
        // Loop through RelAbundanceL0 and test results
        for(index = 0; index < 8; index++) {
            EXPECT_NEAR(RelAbundanceL0[index], RelAbundanceL0Expected[index], tol6);
        }

        // Loop through RelAbundanceL1 and test results
        for(index = 0; index < 5; index++) {
            EXPECT_NEAR(RelAbundanceL1[index], RelAbundanceL1Expected[index], tol6);
        }

        EXPECT_DOUBLE_EQ(grassOutput[0], 1.);
        EXPECT_DOUBLE_EQ(grassOutput[1], 0.);
        EXPECT_DOUBLE_EQ(grassOutput[2], 0.);

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
           rSOILWAT2::estimate_PotNatVeg_composition_old(
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
             BareGround_Fraction = .1098, fix_BareGround = TRUE
           )
         ```
         */

        RelAbundanceL0Expected[succIndex] = 0.1098;
        RelAbundanceL0Expected[forbIndex] = 0.1098;
        RelAbundanceL0Expected[C3Index] = 0.1098;
        RelAbundanceL0Expected[C4Index] = 0.1098;
        RelAbundanceL0Expected[grassAnn] = 0.1098;
        RelAbundanceL0Expected[shrubIndex] = 0.1098;
        RelAbundanceL0Expected[treeIndex] = 0.1098;
        RelAbundanceL0Expected[bareGround] = 0.2314;

        RelAbundanceL1Expected[treeIndexL1] = 0.1098;
        RelAbundanceL1Expected[forbIndexL1] = 0.1098;
        RelAbundanceL1Expected[shrubIndexL1] = 0.2196;
        RelAbundanceL1Expected[grassesIndexL1] = 0.3294;
        RelAbundanceL1Expected[bareGroundL1] = 0.2314;

        estimatePotNatVegComposition(climateAverages.meanTemp_C, climateAverages.PPT_cm,
            climateAverages.meanTempMon_C, climateAverages.PPTMon_cm, inputValues, shrubLimit,
            SumGrassesFraction, C4Variables, fillEmptyWithBareGround, inNorthHem, warnExtrapolation,
            grassOutput, RelAbundanceL0, RelAbundanceL1);

        // Loop through RelAbundanceL0 and test results. Since initial values
        // do not add to one and we fill empty with bare ground, bare ground should be higher
        // than the other values (in this case, .2314)
        for(index = 0; index < 8; index++) {
            EXPECT_NEAR(RelAbundanceL0[index], RelAbundanceL0Expected[index], tol6);
        }

        // Loop through RelAbundanceL1 and test results
        for(index = 0; index < 5; index++) {
            EXPECT_NEAR(RelAbundanceL1[index], RelAbundanceL1Expected[index], tol6);
        }

        EXPECT_NEAR(grassOutput[0], .333333, tol6);
        EXPECT_NEAR(grassOutput[1], .333333, tol6);
        EXPECT_NEAR(grassOutput[2], .333333, tol6);

        /*  ===============================================================
         Test with `inNorthHem` to be false, same input values as previous test
         except for trees and bare ground which are both .0549
            ===============================================================  */

        /* Expect identical output to rSOILWAT2 (e.g., v5.3.1)
         * NOTE: Command uses deprecated estimate_PotNatVeg_composition (rSOILWAT >= v.6.0.0)
         ```{r}
           rSOILWAT2::estimate_PotNatVeg_composition_old(
             MAP_mm =  10 * clim1[["MAP_cm"]], MAT_C = clim1[["MAT_C"]],
             mean_monthly_ppt_mm = 10 * clim1[["meanMonthlyPPTcm"]],
             mean_monthly_Temp_C = clim1[["meanMonthlyTempC"]],
             Succulents_Fraction = .1098, fix_succulents = TRUE,
             C3_Fraction = .1098, fix_C3grasses = TRUE,
             Shrubs_Fraction = .1098, fix_shrubs = TRUE,
             Trees_Fraction = 0.0549, fix_trees = TRUE,
             Annuals_Fraction = .1098, fix_annuals = TRUE,
             C4_Fraction = .1098, fix_C4grasses = TRUE,
             Forbs_Fraction = .1098, fix_forbs = TRUE,
             BareGround_Fraction = .0549, fix_BareGround = TRUE,
             isNorth = FALSE
           )
         ```
         */

        RelAbundanceL0Expected[succIndex] = 0.1098;
        RelAbundanceL0Expected[forbIndex] = 0.1098;
        RelAbundanceL0Expected[C3Index] = 0.1098;
        RelAbundanceL0Expected[C4Index] = 0.1098;
        RelAbundanceL0Expected[grassAnn] = 0.1098;
        RelAbundanceL0Expected[shrubIndex] = 0.1098;
        RelAbundanceL0Expected[treeIndex] = 0.0549;
        RelAbundanceL0Expected[bareGround] = 0.2863;

        RelAbundanceL1Expected[treeIndexL1] = 0.0549;
        RelAbundanceL1Expected[forbIndexL1] = 0.1098;
        RelAbundanceL1Expected[shrubIndexL1] = 0.2196;
        RelAbundanceL1Expected[grassesIndexL1] = 0.3294;
        RelAbundanceL1Expected[bareGroundL1] = 0.2863;

        inNorthHem = swFALSE;
        fillEmptyWithBareGround = swTRUE;

        inputValues[treeIndex] = .0549;
        inputValues[bareGround] = .0549;

        estimatePotNatVegComposition(climateAverages.meanTemp_C, climateAverages.PPT_cm,
            climateAverages.meanTempMon_C, climateAverages.PPTMon_cm, inputValues, shrubLimit,
            SumGrassesFraction, C4Variables, fillEmptyWithBareGround, inNorthHem, warnExtrapolation,
            grassOutput, RelAbundanceL0, RelAbundanceL1);

        // Loop through RelAbundanceL0 and test results.
        for(index = 0; index < 8; index++) {
            EXPECT_NEAR(RelAbundanceL0[index], RelAbundanceL0Expected[index], tol6);
        }

        // Loop through RelAbundanceL1 and test results
        for(index = 0; index < 5; index++) {
            EXPECT_NEAR(RelAbundanceL1[index], RelAbundanceL1Expected[index], tol6);
        }

        EXPECT_NEAR(grassOutput[0], .333333, tol6);
        EXPECT_NEAR(grassOutput[1], .333333, tol6);
        EXPECT_NEAR(grassOutput[2], .333333, tol6);

        /*  ===============================================================
         Test with `SumGrassesFraction` being fixed, all input of previous tests
         are halved to .0549
            ===============================================================  */

        /* Expect identical output to rSOILWAT2 (e.g., v5.3.1)
         * NOTE: Command uses deprecated estimate_PotNatVeg_composition (rSOILWAT >= v.6.0.0)
         ```{r}
           rSOILWAT2::estimate_PotNatVeg_composition_old(
             MAP_mm =  10 * clim1[["MAP_cm"]], MAT_C = clim1[["MAT_C"]],
             mean_monthly_ppt_mm = 10 * clim1[["meanMonthlyPPTcm"]],
             mean_monthly_Temp_C = clim1[["meanMonthlyTempC"]],
             Succulents_Fraction = .0549, fix_succulents = TRUE,
             Shrubs_Fraction = .0549, fix_shrubs = TRUE,
             Trees_Fraction = .0549, fix_trees = TRUE,
             Forbs_Fraction = .0549, fix_forbs = TRUE,
             SumGrasses_Fraction = .7255, fix_sumgrasses = TRUE,
             BareGround_Fraction = .0549, fix_BareGround = TRUE
           )
         ```
         */

        inputValues[succIndex] = .0549;
        inputValues[forbIndex] = .0549;
        inputValues[C3Index] = SW_MISSING;
        inputValues[C4Index] = SW_MISSING;
        inputValues[grassAnn] = SW_MISSING;
        inputValues[shrubIndex] = .0549;
        inputValues[treeIndex] = .0549;
        inputValues[bareGround] = .0549;

        RelAbundanceL0Expected[succIndex] = .0549;
        RelAbundanceL0Expected[forbIndex] = .0549;
        RelAbundanceL0Expected[C3Index] = 0.7255;
        RelAbundanceL0Expected[C4Index] = 0.0;
        RelAbundanceL0Expected[grassAnn] = 0.0;
        RelAbundanceL0Expected[shrubIndex] = .0549;
        RelAbundanceL0Expected[treeIndex] = .0549;
        RelAbundanceL0Expected[bareGround] = 0.0549;

        RelAbundanceL1Expected[treeIndexL1] = .0549;
        RelAbundanceL1Expected[forbIndexL1] = .0549;
        RelAbundanceL1Expected[shrubIndexL1] = 0.1098;
        RelAbundanceL1Expected[grassesIndexL1] = 0.7255;
        RelAbundanceL1Expected[bareGroundL1] = 0.0549;

        fillEmptyWithBareGround = swTRUE;
        inNorthHem = swTRUE;
        SumGrassesFraction = .7255;

        estimatePotNatVegComposition(climateAverages.meanTemp_C, climateAverages.PPT_cm,
            climateAverages.meanTempMon_C, climateAverages.PPTMon_cm, inputValues, shrubLimit,
            SumGrassesFraction, C4Variables, fillEmptyWithBareGround, inNorthHem, warnExtrapolation,
            grassOutput, RelAbundanceL0, RelAbundanceL1);

        // Loop through RelAbundanceL0 and test results.
        for(index = 0; index < 8; index++) {
            EXPECT_NEAR(RelAbundanceL0[index], RelAbundanceL0Expected[index], tol6);
        }

        // Loop through RelAbundanceL1 and test results
        for(index = 0; index < 5; index++) {
            EXPECT_NEAR(RelAbundanceL1[index], RelAbundanceL1Expected[index], tol6);
        }

        EXPECT_NEAR(grassOutput[0], 1., tol6);
        EXPECT_NEAR(grassOutput[1], 0.0, tol6);
        EXPECT_NEAR(grassOutput[2], 0.0, tol6);

        // Deallocate structs
        allocDeallocClimateStructs(deallocate, 31, &climateOutput, &climateAverages);
    }

    TEST(EstimateVegetationTest, FullVegetation) {

        /*  ================================================================
                   This block of tests deals with input values to
                   `estimatePotNatVegComposition()` that add up to 1
            ================================================================  */

        SW_CLIMATE_YEARLY climateOutput;
        SW_CLIMATE_CLIM climateAverages;

        double inputValues[8] = {.0567, .2317, .0392, .0981,
                                .3218, .0827, .1293, .0405};
        double shrubLimit = .2;

        // Array holding only grass values
        double grassOutput[3]; // 3 = Number of grass variables

        // Array holding all values from the estimation
        double RelAbundanceL0[8]; // 8 = Number of types

        // Array holding all values from estimation minus grasses
        double RelAbundanceL1[5]; // 5 = Number of types minus grasses

        double SumGrassesFraction = SW_MISSING;
        double C4Variables[3];
        double RelAbundanceL0Expected[8] = {0.0567, 0.2317, .0392,
            .0981, .3218, .0827, .1293, .0405};
        double RelAbundanceL1Expected[5] = {.1293, .0827, .2884, .4591, .0405};

        Bool fillEmptyWithBareGround = swTRUE;
        Bool inNorthHem = swTRUE;
        Bool warnExtrapolation = swTRUE;

        int deallocate = 0;
        int allocate = 1;
        int index;

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
        int forbIndexL1 = 1;
        int shrubIndexL1 = 2;
        int grassesIndexL1 = 3;
        int bareGroundL1 = 4;

        // Reset "SW_Weather.allHist"
        SW_WTH_read();

        // Allocate arrays needed for `calcSiteClimate()` and `averageClimateAcrossYears()`
        allocDeallocClimateStructs(allocate, 31, &climateOutput, &climateAverages);

        // Calculate climate of the site and add results to "climateOutput"
        calcSiteClimate(SW_Weather.allHist, 31, 1980, inNorthHem, &climateOutput);

        // Average values from "climateOutput" and put them in "climateAverages"
        averageClimateAcrossYears(&climateOutput, 31, &climateAverages);

        // Set C4 results, standard deviations are not needed for estimating vegetation
        C4Variables[0] = climateAverages.minTemp7thMon_C;
        C4Variables[1] = climateAverages.ddAbove65F_degday;
        C4Variables[2] = climateAverages.frostFree_days;

        // Estimate vegetation based off calculated variables and "inputValues"
        estimatePotNatVegComposition(climateAverages.meanTemp_C, climateAverages.PPT_cm,
            climateAverages.meanTempMon_C, climateAverages.PPTMon_cm, inputValues, shrubLimit,
            SumGrassesFraction, C4Variables, fillEmptyWithBareGround, inNorthHem, warnExtrapolation,
            grassOutput, RelAbundanceL0, RelAbundanceL1);

        // All values in "RelAbundanceL0" should be exactly the same as "inputValues"
        for(index = 0; index < 8; index++) {
            EXPECT_NEAR(RelAbundanceL0[index], RelAbundanceL0Expected[index], tol6);
        }

        // All values in "RelAbundanceL1" should be exactly the same as "inputValues"
        for(index = 0; index < 5; index++) {
            EXPECT_NEAR(RelAbundanceL1[index], RelAbundanceL1Expected[index], tol6);
        }

        EXPECT_NEAR(grassOutput[0], 0.08538445, tol6);
        EXPECT_NEAR(grassOutput[1], 0.21367894, tol6);
        EXPECT_NEAR(grassOutput[2], 0.70093662, tol6);

        /*  ===============================================================
                  Test when a couple input values are not "SW_MISSING"
            ===============================================================  */

        /* Expect identical output to rSOILWAT2 (e.g., v5.3.1)
         * NOTE: Command uses deprecated estimate_PotNatVeg_composition (rSOILWAT >= v.6.0.0)
         ```{r}
           rSOILWAT2::estimate_PotNatVeg_composition_old(
             MAP_mm =  10 * clim1[["MAP_cm"]], MAT_C = clim1[["MAT_C"]],
             mean_monthly_ppt_mm = 10 * clim1[["meanMonthlyPPTcm"]],
             mean_monthly_Temp_C = clim1[["meanMonthlyTempC"]],
             Succulents_Fraction = .5, fix_succulents = TRUE,
             C3_Fraction = .5, fix_C3grasses = TRUE
           )
         ```
         */

        inputValues[succIndex] = .5;
        inputValues[forbIndex] = SW_MISSING;
        inputValues[C3Index] = .5;
        inputValues[C4Index] = SW_MISSING;
        inputValues[grassAnn] = SW_MISSING;
        inputValues[shrubIndex] = SW_MISSING;
        inputValues[treeIndex] = SW_MISSING;
        inputValues[bareGround] = SW_MISSING;

        RelAbundanceL0Expected[succIndex] = .5;
        RelAbundanceL0Expected[forbIndex] = 0.;
        RelAbundanceL0Expected[C3Index] = .5;
        RelAbundanceL0Expected[C4Index] = 0.;
        RelAbundanceL0Expected[grassAnn] = 0.;
        RelAbundanceL0Expected[shrubIndex] = 0.;
        RelAbundanceL0Expected[treeIndex] = 0.;
        RelAbundanceL0Expected[bareGround] = 0.;

        RelAbundanceL1Expected[treeIndexL1] = 0.;
        RelAbundanceL1Expected[forbIndexL1] = 0.;
        RelAbundanceL1Expected[shrubIndexL1] = .5;
        RelAbundanceL1Expected[grassesIndexL1] = .5;
        RelAbundanceL1Expected[bareGroundL1] = 0.;

        estimatePotNatVegComposition(climateAverages.meanTemp_C, climateAverages.PPT_cm,
            climateAverages.meanTempMon_C, climateAverages.PPTMon_cm, inputValues, shrubLimit,
            SumGrassesFraction, C4Variables, fillEmptyWithBareGround, inNorthHem, warnExtrapolation,
            grassOutput, RelAbundanceL0, RelAbundanceL1);

        // Loop through RelAbundanceL0 and test results
        for(index = 0; index < 8; index++) {
            EXPECT_DOUBLE_EQ(RelAbundanceL0[index], RelAbundanceL0Expected[index]);
        }

        // Loop through RelAbundanceL1 and test results
        for(index = 0; index < 5; index++) {
            EXPECT_DOUBLE_EQ(RelAbundanceL1[index], RelAbundanceL1Expected[index]);
        }

        EXPECT_DOUBLE_EQ(grassOutput[0], 1.);
        EXPECT_DOUBLE_EQ(grassOutput[1], 0.);
        EXPECT_DOUBLE_EQ(grassOutput[2], 0.);

        /*  ===============================================================
         Test with `fillEmptyWithBareGround` set to false, same input values
         as previous test except for bare ground, which is now .2314
            ===============================================================  */

        /* Expect identical output to rSOILWAT2 (e.g., v5.3.1)
         * NOTE: Command uses deprecated estimate_PotNatVeg_composition (rSOILWAT >= v.6.0.0)
         ```{r}
           rSOILWAT2::estimate_PotNatVeg_composition_old(
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
             fill_empty_with_BareGround = TRUE
           )
         ```
         */

        inputValues[succIndex] = 0.1098;
        inputValues[forbIndex] = 0.1098;
        inputValues[C3Index] = 0.1098;
        inputValues[C4Index] = 0.1098;
        inputValues[grassAnn] = 0.1098;
        inputValues[shrubIndex] = 0.1098;
        inputValues[treeIndex] = 0.1098;
        inputValues[bareGround] = 0.2314;

        fillEmptyWithBareGround = swFALSE;

        RelAbundanceL0Expected[succIndex] = 0.1098;
        RelAbundanceL0Expected[forbIndex] = 0.1098;
        RelAbundanceL0Expected[C3Index] = 0.1098;
        RelAbundanceL0Expected[C4Index] = 0.1098;
        RelAbundanceL0Expected[grassAnn] = 0.1098;
        RelAbundanceL0Expected[shrubIndex] = 0.1098;
        RelAbundanceL0Expected[treeIndex] = 0.1098;
        RelAbundanceL0Expected[bareGround] = 0.2314;

        RelAbundanceL1Expected[treeIndexL1] = 0.1098;
        RelAbundanceL1Expected[forbIndexL1] = 0.1098;
        RelAbundanceL1Expected[shrubIndexL1] = 0.2196;
        RelAbundanceL1Expected[grassesIndexL1] = 0.3294;
        RelAbundanceL1Expected[bareGroundL1] = 0.2314;

        estimatePotNatVegComposition(climateAverages.meanTemp_C, climateAverages.PPT_cm,
            climateAverages.meanTempMon_C, climateAverages.PPTMon_cm, inputValues, shrubLimit,
            SumGrassesFraction, C4Variables, fillEmptyWithBareGround, inNorthHem, warnExtrapolation,
            grassOutput, RelAbundanceL0, RelAbundanceL1);

        // Loop through RelAbundanceL0 and test results.
        for(index = 0; index < 8; index++) {
            EXPECT_NEAR(RelAbundanceL0[index], RelAbundanceL0Expected[index], tol6);
        }

        // Loop through RelAbundanceL1 and test results
        for(index = 0; index < 5; index++) {
            EXPECT_NEAR(RelAbundanceL1[index], RelAbundanceL1Expected[index], tol6);
        }

        EXPECT_NEAR(grassOutput[0], .333333, tol6);
        EXPECT_NEAR(grassOutput[1], .333333, tol6);
        EXPECT_NEAR(grassOutput[2], .333333, tol6);

        // Deallocate structs
        allocDeallocClimateStructs(deallocate, 31, &climateOutput, &climateAverages);
    }

} // namespace
