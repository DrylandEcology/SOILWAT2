/**
 * @file   SW_Carbon.c
 * @author Zachary Kramer
 * @brief  Contains functions, constants, and variables that deal with the effect of CO2 on transpiration and biomass.
 *
 * Atmospheric carbon dioxide has been observed to affect water-use efficiency
 * and biomass, which is what this code attempts to simulate. The effects can
 * be varied by plant functional type. Most usages of the functions here are
 * in @f SW_VegProd.c and @f SW_Flow_lib.c.
 *
 * @date   7 February 2017
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "generic.h"
#include "filefuncs.h"
#include "myMemory.h"
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

static char *MyFileName;
SW_CARBON SW_Carbon;    // Declared here, externed elsewhere
extern SW_VEGPROD SW_VegProd;
extern SW_MODEL SW_Model;


/* =================================================== */
/* =================================================== */
/*             Private Function Definitions            */
/* --------------------------------------------------- */

/* =================================================== */
/* =================================================== */
/*             Public Function Definitions             */
/* --------------------------------------------------- */

/**
 * @brief Initializes the multipliers of the SW_CARBON structure.
 * @note The spin-up year has been known to have the multipliers equal
 *       to 0 without this constructor.
 */
void SW_CBN_construct(void)
{
  memset(&SW_Carbon, 0, sizeof(SW_Carbon));
}

void SW_CBN_deconstruct(void)
{
}


/**
 * @brief Reads yearly carbon data from disk file 'Input/carbon.in'
 *
 * Additionally, check for the following issues:
 *   1. Duplicate entries.
 *   2. Empty file.
 *   3. Missing scenario.
 *   4. Missing year.
 *   5. Negative year.
 */
void SW_CBN_read(void)
{
  #ifdef SWDEBUG
  short debug = 0;
  #endif
  SW_CARBON  *c   = &SW_Carbon;

  // For efficiency, don't read carbon.in if neither multiplier is being used
  // We can do this because SW_VPD_construct already populated the multipliers with default values
  if (!c->use_bio_mult && !c->use_wue_mult)
  {
    #ifdef SWDEBUG
    if (debug) {
      swprintf("'SW_CBN_read': CO2-effects are turned off; don't read CO2-concentration data from file.\n");
    }
    #endif
    return;
  }

  /* Reading carbon.in */
  FILE *f;
  char scenario[64];
  int year,
    simstartyr = (int) SW_Model.startyr + SW_Model.addtl_yr,
    simendyr = (int) SW_Model.endyr + SW_Model.addtl_yr;

  // The following variables must be initialized to show if they've been changed or not
  double ppm = 1.;
  int existing_years[MAX_NYEAR] = {0};
  short fileWasEmpty = 1;

  MyFileName = SW_F_name(eCarbon);
  f = OpenFile(MyFileName, "r");

  #ifdef SWDEBUG
  if (debug) {
    swprintf("'SW_CBN_read': start reading CO2-concentration data from file.\n");
  }
  #endif

  while (GetALine(f, inbuf)) {
    #ifdef SWDEBUG
    if (debug) swprintf("\ninbuf = %s", inbuf);
    #endif

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
    if ((year < simstartyr) || (year > simendyr))
    {
      continue; // We aren't using this year
    }

    sscanf(inbuf, "%d %lf", &year, &ppm);

    if (year < 0)
    {
      CloseFile(&f);
      sprintf(errstr, "(SW_Carbon) Year %d in scenario '%s' is negative; only positive values are allowed.\n", year, c->scenario);
      LogError(logfp, LOGFATAL, errstr);
    }

    c->ppm[year] = ppm;
    #ifdef SWDEBUG
    if (debug) swprintf("  ==> c->ppm[%d] = %3.2f", year, c->ppm[year]);
    #endif

    /* Has this year already been calculated?
       If yes: Do NOT overwrite values, fail the run instead

       Use a simple binary system with the year as the index, which avoids using a loop.
       We cannot simply check if co2_multipliers[0 or 1][year].grass != 1.0, due to floating point precision
       and the chance that a multiplier of 1.0 was actually calculated */
    if (existing_years[year] != 0)
    {
      CloseFile(&f);
      sprintf(errstr, "(SW_Carbon) Year %d in scenario '%s' is entered more than once; only one entry is allowed.\n", year, c->scenario);
      LogError(logfp, LOGFATAL, errstr);
    }
    existing_years[year] = 1;
  }

  CloseFile(&f);


  /* Error checking */

  // Must check if the file was empty before checking if the scneario was found,
  // otherwise the empty file will be masked as not being able to find the scenario
  if (fileWasEmpty == 1)
  {
    sprintf(errstr, "(SW_Carbon) carbon.in was empty; for debugging purposes, SOILWAT2 read in file '%s'\n", MyFileName);
    LogError(logfp, LOGFATAL, errstr);
  }

  if (EQ(ppm, -1.))  // A scenario must be found in order for ppm to have a positive value
  {
    sprintf(errstr, "(SW_Carbon) The scenario '%s' was not found in carbon.in\n", c->scenario);
    LogError(logfp, LOGFATAL, errstr);
  }

  // Ensure that the desired years were calculated
  for (year = simstartyr; year <= simendyr; year++)
  {
    if (existing_years[year] == 0)
    {
      sprintf(errstr, "(SW_Carbon) missing CO2 data for year %d; ensure that ppm values for this year exist in scenario '%s'\n", year, c->scenario);
      LogError(logfp, LOGFATAL, errstr);
    }
  }
}


/**
 * @brief Calculates the multipliers of the CO2-effect for biomass and water-use efficiency.
 *
 * @description
 * Multipliers are calculated per year with the equation: Coeff1 * ppm^Coeff2
 * Where Coeff1 and Coeff2 are provided by the VegProd input. Coefficients assume that
 * monthly biomass reflect values for atmospheric conditions at 360 ppm CO2. Each PFT has
 * its own set of coefficients. If a multiplier is disabled, its value is kept at the
 * default value of 1.0. Multipliers are only calculated for the years that will
 * be simulated.
 */
void calculate_CO2_multipliers(void) {
  int k;
  TimeInt year,
    simendyr = SW_Model.endyr + SW_Model.addtl_yr;
  double ppm;
  SW_CARBON  *c  = &SW_Carbon;
  SW_VEGPROD *v  = &SW_VegProd;
  #ifdef SWDEBUG
  short debug = 0;
  #endif

  if (!c->use_bio_mult && !c->use_wue_mult)
  {
    return;
  }

  // Only iterate through the years that we know will be used
  for (year = SW_Model.startyr + SW_Model.addtl_yr; year <= simendyr; year++)
  {
    ppm = c->ppm[year];

    if (LT(ppm, 0.))  // CO2 concentration must not be negative values
    {
      sprintf(errstr, "(SW_Carbon) No CO2 ppm data was provided for year %d\n", year);
      LogError(logfp, LOGFATAL, errstr);
    }

    // Calculate multipliers per PFT
    if (c->use_bio_mult) {
      ForEachVegType(k) {
        v->veg[k].co2_multipliers[BIO_INDEX][year] = v->veg[k].co2_bio_coeff1 * pow(ppm, v->veg[k].co2_bio_coeff2);
      }
    }

    #ifdef SWDEBUG
    if (debug) {
      swprintf("Shrub: use%d: bio_mult[%d] = %1.3f / coeff1 = %1.3f / coeff2 = %1.3f / ppm = %3.2f\n",
        c->use_bio_mult, year, v->veg[SW_SHRUB].co2_multipliers[BIO_INDEX][year],
        v->veg[SW_SHRUB].co2_bio_coeff1, v->veg[SW_SHRUB].co2_bio_coeff2, ppm);
    }
    #endif

    if (c->use_wue_mult) {
      ForEachVegType(k) {
        v->veg[k].co2_multipliers[WUE_INDEX][year] = v->veg[k].co2_wue_coeff1 *
          pow(ppm, v->veg[k].co2_wue_coeff2);
      }
    }
  }
}
