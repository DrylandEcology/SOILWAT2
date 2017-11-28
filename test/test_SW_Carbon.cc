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
extern SW_MODEL SW_Model;



namespace {
  SW_CARBON *c = &SW_Carbon;

  // Test the SW_Carbon constructor 'SW_CBN_construct'
  TEST(CarbonTest, Constructor) {
    SW_CBN_construct();

    EXPECT_DOUBLE_EQ(1., c->co2_multipliers[BIO_INDEX][0].grass);
    EXPECT_DOUBLE_EQ(1., c->co2_multipliers[BIO_INDEX][0].shrub);
    EXPECT_DOUBLE_EQ(1., c->co2_multipliers[BIO_INDEX][0].tree);
    EXPECT_DOUBLE_EQ(1., c->co2_multipliers[BIO_INDEX][0].forb);
    EXPECT_DOUBLE_EQ(1., c->co2_multipliers[BIO_INDEX][MAX_CO2_YEAR - 1].grass);
    EXPECT_DOUBLE_EQ(1., c->co2_multipliers[BIO_INDEX][MAX_CO2_YEAR - 1].shrub);
    EXPECT_DOUBLE_EQ(1., c->co2_multipliers[BIO_INDEX][MAX_CO2_YEAR - 1].tree);
    EXPECT_DOUBLE_EQ(1., c->co2_multipliers[BIO_INDEX][MAX_CO2_YEAR - 1].forb);

    EXPECT_DOUBLE_EQ(1., c->co2_multipliers[WUE_INDEX][0].grass);
    EXPECT_DOUBLE_EQ(1., c->co2_multipliers[WUE_INDEX][0].shrub);
    EXPECT_DOUBLE_EQ(1., c->co2_multipliers[WUE_INDEX][0].tree);
    EXPECT_DOUBLE_EQ(1., c->co2_multipliers[WUE_INDEX][0].forb);
    EXPECT_DOUBLE_EQ(1., c->co2_multipliers[WUE_INDEX][MAX_CO2_YEAR - 1].grass);
    EXPECT_DOUBLE_EQ(1., c->co2_multipliers[WUE_INDEX][MAX_CO2_YEAR - 1].shrub);
    EXPECT_DOUBLE_EQ(1., c->co2_multipliers[WUE_INDEX][MAX_CO2_YEAR - 1].tree);
    EXPECT_DOUBLE_EQ(1., c->co2_multipliers[WUE_INDEX][MAX_CO2_YEAR - 1].forb);

    EXPECT_DOUBLE_EQ(1., c->co2_bio_mult.grass);
    EXPECT_DOUBLE_EQ(1., c->co2_bio_mult.shrub);
    EXPECT_DOUBLE_EQ(1., c->co2_bio_mult.tree);
    EXPECT_DOUBLE_EQ(1., c->co2_bio_mult.forb);
    EXPECT_DOUBLE_EQ(1., c->co2_wue_mult.grass);
    EXPECT_DOUBLE_EQ(1., c->co2_wue_mult.shrub);
    EXPECT_DOUBLE_EQ(1., c->co2_wue_mult.tree);
    EXPECT_DOUBLE_EQ(1., c->co2_wue_mult.forb);
  }


  // Test reading yearly CO2 data from disk file
  TEST(CarbonTest, ReadInputFile) {
    TimeInt year;
    double sum_CO2;

    // Test if CO2-effects are turned off -> no CO2 concentration data are read from file
    SW_CBN_construct();
    c->use_wue_mult = 0;
    c->use_bio_mult = 0;

    SW_CBN_read();

    sum_CO2 = 0.;
    for (year = 0; year < MAX_CO2_YEAR; year++) {
      sum_CO2 += c->ppm[year];
    }
    EXPECT_DOUBLE_EQ(sum_CO2, 0.);

    // Test if CO2-effects are turned on -> CO2 concentration data are read from file
    SW_CBN_construct();
    strcpy(c->scenario, "RCP85");
    c->use_wue_mult = 1;
    c->use_bio_mult = 1;
    c->addtl_yr = 0;

    SW_CBN_read();

    for (year = SW_Model.startyr + c->addtl_yr; year <= SW_Model.endyr + c->addtl_yr; year++) {
      EXPECT_GT(c->ppm[year], 0.);
    }
  }


  // Test the calculation of CO2-effect multipliers
  TEST(CarbonTest, CO2multipliers) {
    TimeInt year;

    SW_CBN_construct();
    strcpy(c->scenario, "RCP85");
    c->use_wue_mult = 1;
    c->use_bio_mult = 1;
    c->addtl_yr = 0;

    SW_CBN_read();
    calculate_CO2_multipliers();

    for (year = SW_Model.startyr + c->addtl_yr; year <= SW_Model.endyr + c->addtl_yr; year++) {
      EXPECT_GT(c->co2_multipliers[BIO_INDEX][year].forb, 0.);
      EXPECT_GT(c->co2_multipliers[BIO_INDEX][year].grass, 0.);
      EXPECT_GT(c->co2_multipliers[BIO_INDEX][year].shrub, 0.);
      EXPECT_GT(c->co2_multipliers[BIO_INDEX][year].tree, 0.);

      EXPECT_GT(c->co2_multipliers[WUE_INDEX][year].forb, 0.);
      EXPECT_GT(c->co2_multipliers[WUE_INDEX][year].grass, 0.);
      EXPECT_GT(c->co2_multipliers[WUE_INDEX][year].shrub, 0.);
      EXPECT_GT(c->co2_multipliers[WUE_INDEX][year].tree, 0.);
    }
  }


  // Test the application of the biomass CO2-effect
  TEST(CarbonTest, BiomassCO2effect) {
    int i;
    double x;
    double biom1[12], biom2[12];

    for (i = 0; i < 12; i++) {
      biom1[i] = i + 1.;
    }

    SW_CBN_construct();
    strcpy(c->scenario, "RCP85");
    c->use_wue_mult = 1;
    c->use_bio_mult = 1;
    c->addtl_yr = 0;

    SW_CBN_read();
    calculate_CO2_multipliers();

    // One example
    x = c->co2_multipliers[BIO_INDEX][SW_Model.startyr + c->addtl_yr].grass;
    apply_CO2(biom2, biom1, x);

    for (i = 0; i < 12; i++) {
      EXPECT_DOUBLE_EQ(biom2[i], biom1[i] * x);
    }
  }

} // namespace
