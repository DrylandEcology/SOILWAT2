/**
 * @file   SW_Carbon.c
 * @author Zachary Kramer
 * @brief  Contains functions, constants, and variables that deal with the
 *         effect of CO2 on transpiration and biomass.
 *
 * @date   7 February 2017
 */

#include <stdio.h>
#include <stdlib.h>
#include "generic.h"
#include "filefuncs.h"
#include "SW_Defines.h"
#include "SW_Times.h"
#include "SW_Files.h"
#include "SW_Carbon.h"
#include "SW_Site.h"
#include "SW_VegProd.h"

/* =================================================== */
/*                  Global Variables                   */
/* --------------------------------------------------- */

RealD co2_multipliers[2][5000];  // Declared here, externed elsewhere
int use_future_bio_mult;
int use_future_sto_mult;
int use_retro_bio_mult;
int use_retro_sto_mult;
double co2_biomass_mult;
double co2_wue_mult;
int RCP;

/* =================================================== */
/*                Module-Level Variables               */
/* --------------------------------------------------- */

static char *MyFileName;
int test = 0;
SW_SITE SW_Site;                 // Declared here, externed elsewhere
SW_VEGPROD SW_VegProd;           // Declared here, externed elsewhere


/* =================================================== */
/* =================================================== */
/*             Private Function Definitions            */
/* --------------------------------------------------- */

/* =================================================== */
/* =================================================== */
/*             Public Function Definitions             */
/* --------------------------------------------------- */

/**
 * Calculates the multipliers for biomass and the transpiration
 * equation. If a multiplier is disabled, its value is set to 1. All the years
 * in carbon.in are calculated. RSOILWAT will pass in the settings directly.
 * SOILWAT will read siteparam.in for settings.
 */
void SW_Carbon_Get(void) {
  int i;
  /* Initialize variables */
  FILE *f;
  double ppm;
  int year;
  int x = 0;
  int cur_RCP = 0;

  /* Set variables */
  #ifndef RSOILWAT
  SW_SITE *settings = &SW_Site;
  use_retro_bio_mult = settings->use_retro_bio_mult;
  use_retro_sto_mult = settings->use_retro_sto_mult;
  use_future_bio_mult = settings->use_future_bio_mult;
  use_future_sto_mult = settings->use_future_sto_mult;
  RCP = settings->RCP;
  #endif

  SW_VEGPROD *data  = &SW_VegProd;
  MyFileName = SW_F_name(eCarbon);
  f          = OpenFile(MyFileName, "r");

  /* Read carbon.in */
  while (GetALine(f, inbuf)) {
    x = sscanf(inbuf, "%d %lf", &year, &ppm);

    /* Ensure we are using the correct RCP */
    if (cur_RCP != RCP) {
      if (year == 0) cur_RCP = (int) ppm; // In this specific case, ppm is the RCP num
    } else {
      /* Calculate multipliers */
      co2_multipliers[0][year] = 1.0;
      co2_multipliers[1][year] = 1.0;
      if (year <= 2010) {
        if (use_retro_bio_mult) co2_multipliers[0][year] = data->co2_biomass_1  * (ppm)  + data->co2_biomass_2;
        if (use_retro_sto_mult) co2_multipliers[1][year] = data->co2_stomatal_1 * (ppm)  + data->co2_stomatal_2;
      } else if (year > 2010) {
        if (use_future_bio_mult) co2_multipliers[0][year] = data->co2_biomass_1  * (ppm)  + data->co2_biomass_2;
        if (use_future_sto_mult) co2_multipliers[1][year] = data->co2_stomatal_1 * (ppm)  + data->co2_stomatal_2;
      }
    }
  }
  #ifdef RSOILWAT
  if (test) {
    Rprintf("\nPrinting multipliers...\n\n");
    for (i = 1950; i < 2100; i++) {
      Rprintf("Year: %d\n", i);
      Rprintf("Bio: %f\n", co2_multipliers[0][i]);
      Rprintf("Sto: %f\n\n", co2_multipliers[1][i]);
    }
    Rprintf("Done!\n");
  }
  #endif
}


/**
 * Applies CO2 effects to supplied biomass data. Two parameters are needed so that
 * we do not have a compound effect on the biomass.
 * @param new_biomass  the resulting biomass after CO2 effects calculated
 * @param biomass      the biomass to be modified
 */
void apply_CO2(double new_biomass[], double biomass[]) {
  int i;
  if (co2_biomass_mult == 0) co2_biomass_mult = 1.0;  // The start-up year has a multiplier of 0 since it has not calculated the multipliers yet
  for (i = 0; i < 12; i++) {
    new_biomass[i] = (biomass[i] * co2_biomass_mult);
  }
}
