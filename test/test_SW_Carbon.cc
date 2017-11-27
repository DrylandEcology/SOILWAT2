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


extern SW_CARBON SW_Carbon;


namespace {
  SW_CARBON *c = &SW_Carbon;

  // Test the SW_Carbon constructor 'SW_CBN_construct'
  TEST(CarbonTest, Constructor) {
    EXPECT_EQ(1., c->co2_multipliers[BIO_INDEX][0].grass);
    EXPECT_EQ(1., c->co2_multipliers[BIO_INDEX][0].shrub);
    EXPECT_EQ(1., c->co2_multipliers[BIO_INDEX][0].tree);
    EXPECT_EQ(1., c->co2_multipliers[BIO_INDEX][0].forb);
    EXPECT_EQ(1., c->co2_multipliers[BIO_INDEX][MAX_CO2_YEAR - 1].grass);
    EXPECT_EQ(1., c->co2_multipliers[BIO_INDEX][MAX_CO2_YEAR - 1].shrub);
    EXPECT_EQ(1., c->co2_multipliers[BIO_INDEX][MAX_CO2_YEAR - 1].tree);
    EXPECT_EQ(1., c->co2_multipliers[BIO_INDEX][MAX_CO2_YEAR - 1].forb);

    EXPECT_EQ(1., c->co2_multipliers[WUE_INDEX][0].grass);
    EXPECT_EQ(1., c->co2_multipliers[WUE_INDEX][0].shrub);
    EXPECT_EQ(1., c->co2_multipliers[WUE_INDEX][0].tree);
    EXPECT_EQ(1., c->co2_multipliers[WUE_INDEX][0].forb);
    EXPECT_EQ(1., c->co2_multipliers[WUE_INDEX][MAX_CO2_YEAR - 1].grass);
    EXPECT_EQ(1., c->co2_multipliers[WUE_INDEX][MAX_CO2_YEAR - 1].shrub);
    EXPECT_EQ(1., c->co2_multipliers[WUE_INDEX][MAX_CO2_YEAR - 1].tree);
    EXPECT_EQ(1., c->co2_multipliers[WUE_INDEX][MAX_CO2_YEAR - 1].forb);

    EXPECT_EQ(1., c->co2_bio_mult.grass);
    EXPECT_EQ(1., c->co2_bio_mult.shrub);
    EXPECT_EQ(1., c->co2_bio_mult.tree);
    EXPECT_EQ(1., c->co2_bio_mult.forb);
    EXPECT_EQ(1., c->co2_wue_mult.grass);
    EXPECT_EQ(1., c->co2_wue_mult.shrub);
    EXPECT_EQ(1., c->co2_wue_mult.tree);
    EXPECT_EQ(1., c->co2_wue_mult.forb);
  }

  // Test reading yearly CO2 data from disk file

  // Test the calculation of CO2-effect multipliers

  // Test the application of the biomass CO2-effect


} // namespace
