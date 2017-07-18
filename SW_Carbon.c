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
  c->use_sto_mult = INTEGER(GET_SLOT(object, install("CarbonUseSto")))[0];
  c->addtl_yr = INTEGER(GET_SLOT(object, install("DeltaYear")))[0];
  strcpy(c->scenario, CHAR(STRING_ELT(GET_SLOT(object, install("Scenario")), 0)));  // e.g. c->scenario = "RCP85"
}
#endif


/**
 * Calculates the multipliers for biomass and the transpiration
 * equation. If a multiplier is disabled, its value is set to 1. All the years
 * in carbon.in are calculated. RSOILWAT will pass in the settings directly.
 * SOILWAT will read siteparam.in for settings.
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

  // Read carbon.in
  while (GetALine(f, inbuf)) {
	// Scan for the year first, because if the year is 0 it marks a change in the scenario
	sscanf(inbuf, "%d", &year);
	  
	// We found a scenario, do we want this one?
	if (year == 0)
	{
      sscanf(inbuf, "%d %63s", &year, scenario);
	  continue;
	}

	// NO, keep searching
	if (strcmp(scenario, c->scenario) != 0)
	  continue;
  
	// YES, calculate multipliers
	sscanf(inbuf, "%d %lf", &year, &ppm);
    c->co2_multipliers[0][year] = 1.0;
    c->co2_multipliers[1][year] = 1.0;
	if (c->use_bio_mult) c->co2_multipliers[0][year] = v->co2_biomass_1  * pow(ppm, v->co2_biomass_2);
	if (c->use_sto_mult) c->co2_multipliers[1][year] = v->co2_stomatal_1 * pow(ppm, v->co2_stomatal_2);
  }
}


/**
 * Applies CO2 effects to supplied biomass data. Two parameters are needed so that
 * we do not have a compound effect on the biomass.
 * @param new_biomass  the resulting biomass after CO2 effects calculated
 * @param biomass      the biomass to be modified
 */
void apply_CO2(double new_biomass[], double biomass[]) {
  int i;
  SW_CARBON  *c  = &SW_Carbon;

  if (c->co2_biomass_mult == 0) c->co2_biomass_mult = 1.0;  // The start-up year has a multiplier of 0 since it has not calculated the multipliers yet
  for (i = 0; i < 12; i++) {
    new_biomass[i] = (biomass[i] * c->co2_biomass_mult);
  }
}
