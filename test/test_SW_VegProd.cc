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


extern SW_MODEL SW_Model;
extern SW_VEGPROD SW_VegProd;


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

} // namespace
