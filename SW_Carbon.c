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
#include "SW_Model.h"

/* =================================================== */
/*                  Global Variables                   */
/* --------------------------------------------------- */


/* =================================================== */
/*                Module-Level Variables               */
/* --------------------------------------------------- */

#ifndef RSOILWAT
static char *MyFileName;
#endif
SW_CARBON SW_Carbon;    // Declared here, externed elsewhere
SW_VEGPROD SW_VegProd;  // Declared here, externed elsewhere
SW_MODEL SW_Model;      // Declared here, externed elsewhere


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

  // Extract the REQUIRED slots from our object into our structure
  c->use_bio_mult = INTEGER(GET_SLOT(object, install("CarbonUseBio")))[0];
  c->use_wue_mult = INTEGER(GET_SLOT(object, install("CarbonUseWUE")))[0];
  c->addtl_yr = INTEGER(GET_SLOT(object, install("DeltaYear")))[0];  // This is needed for output 100% of the time

  // If CO2 is not being used, we can run without extracting ppm data
  if (!c->use_bio_mult && !c->use_wue_mult)
  {
    return;
  }

  // Only extract the ppm values that will be used
  TimeInt year, x;
  unsigned int i = 1, n;

  year = SW_Model.startyr + c->addtl_yr;
  n = nrow(GET_SLOT(object, install("CO2ppm")));

  // Locate index of first year for which we need CO2 data
  while (i <= n)
  {
    x = INTEGER(GET_SLOT(GET_SLOT(object, install("CO2ppm")), install("Year")))[i - 1]; // R's index is 1-based
    if (year == x)
    {
      continue; // we found the index
    }
  }

  // Check that we have enough data
  if (i + c->addtl_yr > n)
  {
    LogError(logfp, LOGFATAL, "%s : CO2ppm object does not contain data for every year");
  }

  // Copy CO2 concentration values to SOILWAT variable
  for (i; i <= c->addtl_yr; i++)
  {
    c->ppm[year++] = REAL(GET_SLOT(GET_SLOT(object, install("CO2ppm")), install("CO2ppm")))[i - 1];  // R's index is 1-based
  }
}
#endif

#ifndef RSOILWAT
/**
 * Calculates the multipliers for biomass and Water-use efficiency.
 * If a multiplier is disabled, its value is set to 1. All the years
 * in carbon.in are calculated. The settings are read in from siteparam.in.
 *
 * rSOILWAT2 has its own version of this function. We cannot simply use its approach,
 * because it relies on the provider of the input data to do error checking.
 */
void calculate_CO2_multipliers(void) {
  FILE *f;
  char scenario[64];
  TimeInt year;

  // The following variables must be initialized to show if they've been changed or not
  double ppm = -1.0;
  int existing_years[MAX_CO2_YEAR] = { 0 };
  short fileWasEmpty = 1;

  SW_CARBON  *c   = &SW_Carbon;
  SW_VEGPROD *v   = &SW_VegProd;
  MyFileName      = SW_F_name(eCarbon);
  f               = OpenFile(MyFileName, "r");

  // For efficiency, don't read carbon.in if neither multiplier is being used
  // We can do this because SW_CBN_CONSTRUCT already populated the multipliers with default values
  if (!c->use_bio_mult && !c->use_wue_mult)
  {
    return;
  }

  /* Calculate multipliers by reading carbon.in */

  while (GetALine(f, inbuf)) {
    fileWasEmpty = 0;

    // Read the year standalone because if it's 0 it marks a change in the scenario,
    // in which case we'll need to read in a string instead of an int
    sscanf(inbuf, "%d", &year);

    // Find scenario
    if (year == 0)
    {
      sscanf(inbuf, "%d %63s", &year, scenario);
      continue;  // Skip to the ppm values
    }
    if (strcmp(scenario, c->scenario) != 0)
    {
      continue;  // Keep searching for the right scenario
    }
    if ((year < SW_Model.startyr + c->addtl_yr) ||  (year > SW_Model.endyr + c->addtl_yr))
    {
      continue; // We aren't using this year
    }

    sscanf(inbuf, "%d %lf", &year, &ppm);

    /* Has this year already been calculated?
       If yes: Do NOT overwrite values, fail the run instead

       Use a simple binary system with the year as the index, which avoids using a loop.
       We cannot simply check if co2_multipliers[0 or 1][year].grass != 1.0, due to floating point precision
       and the chance that a multiplier of 1.0 was actually calculated */
    if (existing_years[year] != 0)
    {
      sprintf(errstr, "(SW_Carbon) Year %d in scenario '%s' is entered more than once; only one entry is allowed.\n", year, c->scenario);
      LogError(logfp, LOGFATAL, errstr);
    }
    existing_years[year] = 1;

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

  /* Error checking */

  // Must check if the file was empty before checking if the scneario was found,
  // otherwise the empty file will be masked as not being able to find the scenario
  if (fileWasEmpty == 1)
  {
    sprintf(errstr, "(SW_Carbon) carbon.in was empty; for debugging purposes, SOILWAT2 read in file '%s'\n", MyFileName);
    LogError(logfp, LOGFATAL, errstr);
  }

  if (ppm == -1)  // A scenario must be found in order for ppm to have a positive value
  {
    sprintf(errstr, "(SW_Carbon) The scenario '%s' was not found in carbon.in\n", c->scenario);
    LogError(logfp, LOGFATAL, errstr);
  }

  // Ensure that the desired years were calculated
  for (year = SW_Model.startyr + c->addtl_yr; year <= SW_Model.endyr + c->addtl_yr; year++)
  {
    if (existing_years[year] == 0)
    {
      sprintf(errstr, "(SW_Carbon) CO2 multiplier(s) for year %d were not calculated; ensure that ppm values for this year exist in scenario '%s'\n", year, c->scenario);
      LogError(logfp, LOGFATAL, errstr);
    }
  }
}
#endif

#ifdef RSOILWAT
/* In this variation, an array of doubles is provided by swCarbon.
 * The approach to handle this is too different than reading in carbon.in,
 * so a new function is needed. The specific differences we need to account for are:
 *  1) We do not need to read in a file (which is where most of the logic is in the other function)
 *  2) We do not need to search for a scenario (we are provided with the correct values for the scenario)
 *  3) We do not have explicit years (index = year)
 */
void calculate_CO2_multipliers (void) {
  TimeInt year;
  double ppm;
  SW_CARBON  *c  = &SW_Carbon;
  SW_VEGPROD *v  = &SW_VegProd;

  if (!c->use_bio_mult && !c->use_wue_mult)
  {
    return;
  }

  // Only iterate through the years that we know will be used
  for (year = SW_Model.startyr + c->addtl_yr; year <= SW_Model.endyr + c->addtl_yr; year++)
  {
    ppm = c->ppm[year];

    if (ppm < 0.1 && ppm > -0.1)  // 0 is the default value when a year doesn't have ppm data; add a huge range for float precision
    {
      sprintf(errstr, "(SW_Carbon) No CO2 ppm data was provided for year %d\n", year);
      LogError(logfp, LOGFATAL, errstr);
    }

    // Calculate multipliers per PFT
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
#endif


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
