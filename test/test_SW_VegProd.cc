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

        SW_CLIMATE_YEARLY climateOutput;
        SW_CLIMATE_CLIM climateAverages;

        double inputValues[8] = {SW_MISSING, SW_MISSING, SW_MISSING, SW_MISSING,
                                SW_MISSING, SW_MISSING, SW_MISSING, SW_MISSING};
        double shrubLimit = .2;

        // Array holding only grass values
        double grassOutput[3]; // 3 = Number of grass variables

        // Array holding all values from the estimation
        double RelAbundanceL0[8]; // 8 = Number of types

        // Array holding all values from estimation minus grasses
        double RelAbundanceL1[5]; // 5 = Number of types minus grasses

        double SumGrassesFraction = -SW_MISSING;
        double C4Variables[3];
        double RelAbundanceL0Expected[8] = {0.0, 0.2608391, 0.4307062,
                                                0.0, 0.0, 0.3084547, 0.0, 0.0};
        double RelAbundanceL1Expected[5] = {0.0, 0.3084547, 0.2608391, 0.4307062, 0.0};

        Bool fillEmptyWithBareGround = swTRUE;
        Bool inNorth = swTRUE;
        Bool warnExtrapolation = swTRUE;

        int deallocate = 0;
        int allocate = 1;
        int index;

        // Reset "SW_Weather.allHist"
        SW_WTH_read();

        // Allocate arrays needed for `calcSiteClimate()` and `averageClimateAcrossYears()`
        allocDeallocClimateStructs(allocate, 31, &climateOutput, &climateAverages);

        // Calculate climate of the site and add results to "climateOutput"
        calcSiteClimate(SW_Weather.allHist, 31, 1980, &climateOutput);

        // Average values from "climateOutput" and put them in "climateAverages"
        averageClimateAcrossYears(&climateOutput, 31, &climateAverages);

        // Set C4 results, standard deviations are not needed for estimating vegetation
        C4Variables[0] = climateAverages.minTempJuly_C;
        C4Variables[1] = climateAverages.ddAbove65F_degday;
        C4Variables[2] = climateAverages.frostFree_days;

        // Estimate vegetation based off calculated variables and "inputValues"
        esimatePotNatVegComposition(climateAverages.meanTemp_C, climateAverages.PPT_cm,
            climateAverages.meanTempMon_C, climateAverages.PPTMon_cm, inputValues, shrubLimit,
            SumGrassesFraction, C4Variables, fillEmptyWithBareGround, inNorth, warnExtrapolation,
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

        // Test with half of input values not "SW_MISSING"
        inputValues[0] = .376;
        inputValues[1] = SW_MISSING;
        inputValues[2] = .096;
        inputValues[3] = SW_MISSING;
        inputValues[4] = SW_MISSING;
        inputValues[5] = .1098;
        inputValues[6] = .0372;
        inputValues[7] = SW_MISSING;

        RelAbundanceL0Expected[0] = 0.3760;
        RelAbundanceL0Expected[1] = 0.3810;
        RelAbundanceL0Expected[2] = 0.0960;
        RelAbundanceL0Expected[3] = 0.0000;
        RelAbundanceL0Expected[4] = 0.0000;
        RelAbundanceL0Expected[5] = 0.1098;
        RelAbundanceL0Expected[6] = 0.0372;
        RelAbundanceL0Expected[7] = 0.0000;

        RelAbundanceL1Expected[0] = 0.0372;
        RelAbundanceL1Expected[1] = 0.1098;
        RelAbundanceL1Expected[2] = 0.7570;
        RelAbundanceL1Expected[3] = 0.0960;
        RelAbundanceL1Expected[4] = 0.0000;

        esimatePotNatVegComposition(climateAverages.meanTemp_C, climateAverages.PPT_cm,
            climateAverages.meanTempMon_C, climateAverages.PPTMon_cm, inputValues, shrubLimit,
            SumGrassesFraction, C4Variables, fillEmptyWithBareGround, inNorth, warnExtrapolation,
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

        // Test with all input values not "SW_MISSING"
        inputValues[0] = .1098;
        inputValues[1] = .1098;
        inputValues[2] = .1098;
        inputValues[3] = .1098;
        inputValues[4] = .1098;
        inputValues[5] = .1098;
        inputValues[6] = .1098;
        inputValues[7] = .1098;

        RelAbundanceL0Expected[0] = 0.1098;
        RelAbundanceL0Expected[1] = 0.1098;
        RelAbundanceL0Expected[2] = 0.1098;
        RelAbundanceL0Expected[3] = 0.1098;
        RelAbundanceL0Expected[4] = 0.1098;
        RelAbundanceL0Expected[5] = 0.1098;
        RelAbundanceL0Expected[6] = 0.1098;
        RelAbundanceL0Expected[7] = 0.2314;

        RelAbundanceL1Expected[0] = 0.1098;
        RelAbundanceL1Expected[1] = 0.1098;
        RelAbundanceL1Expected[2] = 0.2196;
        RelAbundanceL1Expected[3] = 0.3294;
        RelAbundanceL1Expected[4] = 0.2314;

        esimatePotNatVegComposition(climateAverages.meanTemp_C, climateAverages.PPT_cm,
            climateAverages.meanTempMon_C, climateAverages.PPTMon_cm, inputValues, shrubLimit,
            SumGrassesFraction, C4Variables, fillEmptyWithBareGround, inNorth, warnExtrapolation,
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

        // Deallocate structs
        allocDeallocClimateStructs(deallocate, 31, &climateOutput, &climateAverages);
    }

    TEST(EstimateVegetationTest, FullVegetation) {

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

        double SumGrassesFraction = -SW_MISSING;
        double C4Variables[3];
        double RelAbundanceL0Expected[8] = {0.0567, 0.2317, .0392,
            .0981, .3218, .0827, .1293, .0405};
        double RelAbundanceL1Expected[5] = {.1293, .0827, .2884, .4591, .0405};

        Bool fillEmptyWithBareGround = swTRUE;
        Bool inNorth = swTRUE;
        Bool warnExtrapolation = swTRUE;

        int deallocate = 0;
        int allocate = 1;
        int index;

        // Reset "SW_Weather.allHist"
        SW_WTH_read();

        // Allocate arrays needed for `calcSiteClimate()` and `averageClimateAcrossYears()`
        allocDeallocClimateStructs(allocate, 31, &climateOutput, &climateAverages);

        // Calculate climate of the site and add results to "climateOutput"
        calcSiteClimate(SW_Weather.allHist, 31, 1980, &climateOutput);

        // Average values from "climateOutput" and put them in "climateAverages"
        averageClimateAcrossYears(&climateOutput, 31, &climateAverages);

        // Set C4 results, standard deviations are not needed for estimating vegetation
        C4Variables[0] = climateAverages.minTempJuly_C;
        C4Variables[1] = climateAverages.ddAbove65F_degday;
        C4Variables[2] = climateAverages.frostFree_days;

        // Estimate vegetation based off calculated variables and "inputValues"
        esimatePotNatVegComposition(climateAverages.meanTemp_C, climateAverages.PPT_cm,
            climateAverages.meanTempMon_C, climateAverages.PPTMon_cm, inputValues, shrubLimit,
            SumGrassesFraction, C4Variables, fillEmptyWithBareGround, inNorth, warnExtrapolation,
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

        // Test when a couple input values are not "SW_MISSING"
        inputValues[0] = .5;
        inputValues[1] = SW_MISSING;
        inputValues[2] = .5;
        inputValues[3] = SW_MISSING;
        inputValues[4] = SW_MISSING;
        inputValues[5] = SW_MISSING;
        inputValues[6] = SW_MISSING;
        inputValues[7] = SW_MISSING;

        RelAbundanceL0Expected[0] = .5;
        RelAbundanceL0Expected[1] = 0.;
        RelAbundanceL0Expected[2] = .5;
        RelAbundanceL0Expected[3] = 0.;
        RelAbundanceL0Expected[4] = 0.;
        RelAbundanceL0Expected[5] = 0.;
        RelAbundanceL0Expected[6] = 0.;
        RelAbundanceL0Expected[7] = 0.;

        RelAbundanceL1Expected[0] = 0.;
        RelAbundanceL1Expected[1] = 0.;
        RelAbundanceL1Expected[2] = .5;
        RelAbundanceL1Expected[3] = .5;
        RelAbundanceL1Expected[4] = 0.;

        esimatePotNatVegComposition(climateAverages.meanTemp_C, climateAverages.PPT_cm,
            climateAverages.meanTempMon_C, climateAverages.PPTMon_cm, inputValues, shrubLimit,
            SumGrassesFraction, C4Variables, fillEmptyWithBareGround, inNorth, warnExtrapolation,
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

        // Deallocate structs
        allocDeallocClimateStructs(deallocate, 31, &climateOutput, &climateAverages);
    }

} // namespace
