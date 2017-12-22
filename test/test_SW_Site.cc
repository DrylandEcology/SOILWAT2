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

#include <typeinfo>  // for 'typeid'

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


extern SW_CARBON SW_Carbon;
extern SW_MODEL SW_Model;
extern SW_VEGPROD SW_VegProd;
extern SW_SITE SW_Site;


namespace {

  // Test the water equation function 'water_eqn'
  TEST(SWSiteTest, WaterEquation) {

    //declare inputs
    RealD fractionGravel = 0.1, sand = .33, clay =.33;
    LyrIndex n = 1;

    water_eqn(fractionGravel, sand, clay, n);

    // Test swcBulk_saturated
    EXPECT_GT(SW_Site.lyr[n]->swcBulk_saturated, 0.); // The swcBulk_saturated should be greater than 0
    EXPECT_LT(SW_Site.lyr[n]->swcBulk_saturated, SW_Site.lyr[n]->width); // The swcBulk_saturated can't be greater than the width of the layer

    // Test thetasMatric
    EXPECT_GT(SW_Site.lyr[n]->thetasMatric, 30.); // Value should always be greater than 30
    EXPECT_LT(SW_Site.lyr[n]->thetasMatric, 60.); // Value should always be greater than 60
    EXPECT_DOUBLE_EQ(SW_Site.lyr[n]->thetasMatric,  44.593); // If sand is .33 and clay is .33, thetasMatric should be this value

    // Test psisMatric
    EXPECT_GT(SW_Site.lyr[n]->psisMatric, 0.); // Value should always be greater than 0
    EXPECT_LT(SW_Site.lyr[n]->psisMatric, 150.); // Value should always be greater than 150
    EXPECT_DOUBLE_EQ(SW_Site.lyr[n]->psisMatric, 27.586715750763947); // If sand is .33 and clay is .33, thetasMatric should be this value

    // Test bMatric
    EXPECT_GT(SW_Site.lyr[n]->bMatric, 2.); // Value should always be greater than 0
    EXPECT_LT(SW_Site.lyr[n]->bMatric, 200.); // Value should always be greater than 150
    EXPECT_DOUBLE_EQ(SW_Site.lyr[n]->bMatric, 8.182); // If sand is .33 and clay is .33, thetasMatric should be this value

    // Test that error will be logged when b_matric is 0
    sand = 10. + 1./3.; // So that bmatric will equal 0, even though this is a very irrealistic value
    clay = 0;

    // water_eqn(fractionGravel, sand, clay, n);
     EXPECT_DEATH(water_eqn(fractionGravel, sand, clay, n), "@ generic.c LogError");

    // Reset to previous global states
    Reset_SOILWAT2_after_UnitTest();
  }

} // namespace
