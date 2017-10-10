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
#include <math.h>
#include <string.h>
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


/* =================================================== */
/*                Module-Level Variables               */
/* --------------------------------------------------- */

static char *MyFileName;
SW_CARBON SW_Carbon;    // Declared here, externed elsewhere
SW_VEGPROD SW_VegProd;  // Declared here, externed elsewhere


/* =================================================== */
/* =================================================== */
/*             Private Function Definitions            */
/* --------------------------------------------------- */

/* =================================================== */
/* =================================================== */
/*             Public Function Definitions             */
/* --------------------------------------------------- */

/**
 * Initializes the multiplier values in the SW_CARBON structure
 * @note The spin-up year has been known to have the multipliers equal
 *       to 0 without this constructor
 */
void SW_CBN_construct(void)
{
  memset(&SW_Carbon, 0, sizeof(SW_Carbon));

  SW_CARBON *c = &SW_Carbon;
  int year;

  PFTs default_values;
  default_values.grass = default_values.shrub = default_values.tree = default_values.forb = 1.0;

  for (year = 0; year < MAX_CO2_YEAR; year++)
  {
    c->co2_multipliers[BIO_INDEX][year] = default_values;
    c->co2_multipliers[WUE_INDEX][year] = default_values;
  }

  c->co2_bio_mult = default_values;
  c->co2_wue_mult = default_values;
}

/* A description on how these 'onGet' and 'onSet' functions work...
 * Summary: onGet instantiates the 'swCarbon' class and returns the object, and is only used once
 *          onSet extracts the value of the given object, and is used on both calls to SOILWAT2
 * 1) An S4 class is described and generated in rSOILWAT2/R
 * 2) This class needs to be instantiatied, which is done here
 * 3) The object that gets returned here eventually gets inserted into swRunScenariosData[[1]]
 * 4) Data of the object is then modified with class functions in R (e.g. rSOILWAT2::swCarbon_Scenario(swRUnScenariosData[[1]]) <- "RCP85")
 * 5) The 'onSet' function is used to extract the latest data of the object (e.g. when SOILWAT2 begins modeling the real years)
 */
#ifdef RSOILWAT
SEXP onGet_SW_CARBON(void) {
  // Create access variables
  SEXP class, object;

  // Grab our S4 carbon class as an object
  PROTECT(class  = MAKE_CLASS("swCarbon"));
  PROTECT(object = NEW_OBJECT(class));

  // Extract the values to our global structure
  onSet_swCarbon(object);

  UNPROTECT(2);

  return object;
}

void onSet_swCarbon(SEXP object) {
  SW_CARBON *c = &SW_Carbon;

  // Extract the slots from our object into our structure
  c->use_bio_mult = INTEGER(GET_SLOT(object, install("CarbonUseBio")))[0];
  c->use_wue_mult = INTEGER(GET_SLOT(object, install("CarbonUseWUE")))[0];
  c->addtl_yr = INTEGER(GET_SLOT(object, install("DeltaYear")))[0];
  strcpy(c->scenario, CHAR(STRING_ELT(GET_SLOT(object, install("Scenario")), 0)));  // e.g. c->scenario = "RCP85"
}
#endif

/**
 * Calculates the multipliers for biomass and Water-use efficiency.
 * If a multiplier is disabled, its value is set to 1. All the years
 * in carbon.in are calculated. rSOILWAT2 will pass in the settings directly.
 * SOILWAT2 will read siteparam.in for settings.
 */
void calculate_CO2_multipliers(void) {
  FILE *f;
  char scenario[64];
  double ppm;
  int year;

  SW_CARBON  *c  = &SW_Carbon;
  SW_VEGPROD *v  = &SW_VegProd;
  MyFileName     = SW_F_name(eCarbon);
  f              = OpenFile(MyFileName, "r");

  // For efficiency, don't read carbon.in if neither multiplier is being used
  // We can do this because SW_CBN_CONSTRUCT already populated the multipliers with default values
  if (!c->use_bio_mult && !c->use_wue_mult)
  {
    return;
  }

  /* Calculate multipliers by reading carbon.in */
  while (GetALine(f, inbuf)) {
    // Read the year standalone because if it's 0 it marks a change in the scenario,
    // in which case we'll need to read in a string instead of an int
    sscanf(inbuf, "%d", &year);

    // Find scenario
    if (year == 0)
    {
      sscanf(inbuf, "%d %63s", &year, scenario);
      continue;  // Skip to the PPM values
    }
    if (strcmp(scenario, c->scenario) != 0)
      continue;  // Keep searching for the right scenario

    sscanf(inbuf, "%d %lf", &year, &ppm);

    // Per PFT
    if (c->use_bio_mult)
    {
      c->co2_multipliers[BIO_INDEX][year].grass = v->co2_bio_coeff1.grass * pow(ppm, v->co2_bio_coeff2.grass);
      c->co2_multipliers[BIO_INDEX][year].shrub = v->co2_bio_coeff1.shrub * pow(ppm, v->co2_bio_coeff2.shrub);
      c->co2_multipliers[BIO_INDEX][year].tree = v->co2_bio_coeff1.tree * pow(ppm, v->co2_bio_coeff2.tree);
      c->co2_multipliers[BIO_INDEX][year].forb = v->co2_bio_coeff1.forb * pow(ppm, v->co2_bio_coeff2.forb);
    }
    if (c->use_wue_mult)
    {
      c->co2_multipliers[WUE_INDEX][year].grass = v->co2_wue_coeff1.grass * pow(ppm, v->co2_wue_coeff2.grass);
      c->co2_multipliers[WUE_INDEX][year].shrub = v->co2_wue_coeff1.shrub * pow(ppm, v->co2_wue_coeff2.shrub);
      c->co2_multipliers[WUE_INDEX][year].tree = v->co2_wue_coeff1.tree * pow(ppm, v->co2_wue_coeff2.tree);
      c->co2_multipliers[WUE_INDEX][year].forb = v->co2_wue_coeff1.forb * pow(ppm, v->co2_wue_coeff2.forb);
    }
  }
}

/**
 * Applies CO2 effects to supplied biomass data. Two parameters are needed so that
 * we do not have a compound effect on the biomass.
 * @param new_biomass  the resulting biomass after CO2 effects calculated
 * @param biomass      the biomass to be modified
 * @param multiplier   the biomass multiplier for this PFT
 */
void apply_CO2(double new_biomass[], double biomass[], double multiplier) {
  int i;
  for (i = 0; i < 12; i++) new_biomass[i] = (biomass[i] * multiplier);
}
