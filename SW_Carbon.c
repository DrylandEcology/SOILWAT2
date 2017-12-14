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
#include "SW_Defines.h"
#include "SW_Times.h"
#include "SW_Files.h"
#include "SW_Carbon.h"
#include "SW_Site.h"
#include "SW_VegProd.h"
#include "SW_Model.h"
#ifdef RSOILWAT
  #include <R.h>
  #include <Rinternals.h>
#endif

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


/* A description on how these 'onGet' and 'onSet' functions work...
 * Summary: onGet instantiates the S4 'swCarbon' class and copies the values of the C variable 'SW_Carbon' into a R variable of the S4 'swCarbon' class.
 *          onSet copies the values of the R variable (of the S4 'swCarbon' class) into the C variable 'SW_Carbon'
 * 1) An S4 class is described and generated in rSOILWAT2/R
 * 2) This class needs to be instantiatied, which is done here
 * 3) The object that gets returned here eventually gets inserted into swRunScenariosData
 * 4) Data of the object is then modified with class functions in R (e.g. rSOILWAT2::swCarbon_Scenario(swRUnScenariosData[[1]]) <- "RCP85")
 * 5) The 'onSet' function is used to extract the latest data of the object (e.g. when SOILWAT2 begins modeling the real years)
 */
#ifdef RSOILWAT

/**
 * @brief Instantiate the 'swCarbon' class and copies the values of the C variable
 * 'SW_Carbon' into a R variable of the S4 'swCarbon' class.
 * @return An instance of the swCarbon class.
 */
SEXP onGet_SW_CARBON(void) {
  // Create access variables
  SEXP class, object,
    CarbonUseBio, CarbonUseWUE, Scenario, DeltaYear, CO2ppm, CO2ppm_Names,
    cCO2ppm_Names;
  char *cCO2ppm[] = {"Year", "CO2ppm"};
  char *cSW_CARBON[] = {"CarbonUseBio", "CarbonUseWUE", "Scenario", "DeltaYear", "CO2ppm"};
  int i, year, n_sim;
  double *vCO2ppm;

  SW_CARBON *c = &SW_Carbon;

  // Grab our S4 carbon class as an object
  PROTECT(class  = MAKE_CLASS("swCarbon"));
  PROTECT(object = NEW_OBJECT(class));

  // Copy values from C object 'SW_Carbon' into new S4 object
  PROTECT(CarbonUseBio = NEW_INTEGER(1));
  INTEGER(CarbonUseBio)[0] = c->use_bio_mult;
  SET_SLOT(object, install(cSW_CARBON[0]), CarbonUseBio);

  PROTECT(CarbonUseWUE = NEW_INTEGER(1));
  INTEGER(CarbonUseWUE)[0] = c->use_wue_mult;
  SET_SLOT(object, install(cSW_CARBON[1]), CarbonUseWUE);

  PROTECT(Scenario = NEW_STRING(1));
  SET_STRING_ELT(Scenario, 0, mkChar(c->scenario));
  SET_SLOT(object, install(cSW_CARBON[2]), Scenario);

  PROTECT(DeltaYear = NEW_INTEGER(1));
  INTEGER(DeltaYear)[0] = SW_Model.addtl_yr;
  SET_SLOT(object, install(cSW_CARBON[3]), DeltaYear);

  n_sim = SW_Model.endyr - SW_Model.startyr + 1;
  PROTECT(CO2ppm = allocMatrix(REALSXP, n_sim, 2));
  vCO2ppm = REAL(CO2ppm);
  for (i = 0, year = SW_Model.startyr; i < n_sim; i++, year++)
  {
    vCO2ppm[i + n_sim * 0] = year;
    vCO2ppm[i + n_sim * 1] = c->ppm[year];
  }
  PROTECT(CO2ppm_Names = allocVector(VECSXP, 2));
  PROTECT(cCO2ppm_Names = allocVector(STRSXP, 2));
  for (i = 0; i < 2; i++)
    SET_STRING_ELT(cCO2ppm_Names, i, mkChar(cCO2ppm[i]));
  SET_VECTOR_ELT(CO2ppm_Names, 1, cCO2ppm_Names);
  setAttrib(CO2ppm, R_DimNamesSymbol, CO2ppm_Names);
  SET_SLOT(object, install(cSW_CARBON[4]), CO2ppm);

  UNPROTECT(9);

  return object;
}


/**
 * @brief Populate the SW_CARBON structure with the values of swCarbon.
 *
 * Extract slots of the swCarbon class:
 *   1. CarbonUseBio - Whether or not to use the biomass multiplier.
 *   2. CarbonUseWUE - Whether or not to use the WUE multiplier.
 *   3. DeltaYear - How many years in the future we are simulating.
 *   4. Scenario - Scenario name of the CO2 concentration time series.
 *   5. CO2ppm - a vector of length 2 where the first element is the vector of
 *               years and the second element is the CO2 values.
 *
 * @param object An instance of the swCarbon class.
 */
void onSet_swCarbon(SEXP object) {
  SW_CARBON *c = &SW_Carbon;

  // Extract the slots from our object into our structure
  c->use_bio_mult = INTEGER(GET_SLOT(object, install("CarbonUseBio")))[0];
  c->use_wue_mult = INTEGER(GET_SLOT(object, install("CarbonUseWUE")))[0];
  SW_Model.addtl_yr = INTEGER(GET_SLOT(object, install("DeltaYear")))[0];  // This is needed for output 100% of the time
  strcpy(c->scenario, CHAR(STRING_ELT(GET_SLOT(object, install("Scenario")), 0)));

  // If CO2 is not being used, we can run without extracting ppm data
  if (!c->use_bio_mult && !c->use_wue_mult)
  {
    return;
  }

  // Only extract the CO2 values that will be used
  TimeInt year;
  unsigned int i, n_input, n_sim, debug = 0;

  SEXP CO2ppm;
  double *values;

  year = SW_Model.startyr + SW_Model.addtl_yr; // real calendar year when simulation begins
  n_sim = SW_Model.endyr - SW_Model.startyr + 1;
  PROTECT(CO2ppm = GET_SLOT(object, install("CO2ppm")));
  n_input = nrows(CO2ppm);
  values = REAL(CO2ppm);

  // Locate index of first year for which we need CO2 data
  for (i = 1; year != (unsigned int) values[i - 1 + n_input * 0] && i < MAX_NYEAR; i++) {}

  if (debug) {
    swprintf("'onSet_swCarbon': year = %d, n_sim = %d, n_input = %d, i = %d\n",
      year, n_sim, n_input, i);
  }

  // Check that we have enough data
  if (i - 1 + n_sim > n_input)
  {
    LogError(logfp, LOGFATAL, "CO2ppm object does not contain data for every year");
  }

  // Copy CO2 concentration values to SOILWAT variable
  for (; i <= n_input && year < MAX_NYEAR; i++, year++)
  {
    c->ppm[year] = values[i - 1 + n_input * 1];  // R's index is 1-based

    if (debug) {
      swprintf("ppm[year = %d] = %3.2f <-> S4[i = %d] = %3.2f\n",
        year, c->ppm[year], i, values[i - 1 + n_input * 1]);
    }
  }

  UNPROTECT(1);
}
#endif


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
  short debug = 0;
  SW_CARBON  *c   = &SW_Carbon;

  // For efficiency, don't read carbon.in if neither multiplier is being used
  // We can do this because SW_CBN_CONSTRUCT already populated the multipliers with default values
  if (!c->use_bio_mult && !c->use_wue_mult)
  {
    if (debug) {
      swprintf("'SW_CBN_read': CO2-effects are turned off; don't read CO2-concentration data from file.\n");
    }
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

  if (debug) {
    swprintf("'SW_CBN_read': start reading CO2-concentration data from file.\n");
  }

  while (GetALine(f, inbuf)) {
    if (debug) swprintf("\ninbuf = %s", inbuf);

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
    if (debug) swprintf("  ==> c->ppm[%d] = %3.2f", year, c->ppm[year]);

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
  TimeInt year,
    simendyr = SW_Model.endyr + SW_Model.addtl_yr;
  double ppm;
  SW_CARBON  *c  = &SW_Carbon;
  SW_VEGPROD *v  = &SW_VegProd;
  short debug = 0;

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
    if (c->use_bio_mult)
    {
      v->grass.co2_multipliers[BIO_INDEX][year] = v->grass.co2_bio_coeff1 * pow(ppm, v->grass.co2_bio_coeff2);
      v->shrub.co2_multipliers[BIO_INDEX][year] = v->shrub.co2_bio_coeff1 * pow(ppm, v->shrub.co2_bio_coeff2);
      v->tree.co2_multipliers[BIO_INDEX][year] = v->tree.co2_bio_coeff1 * pow(ppm, v->tree.co2_bio_coeff2);
      v->forb.co2_multipliers[BIO_INDEX][year] = v->forb.co2_bio_coeff1 * pow(ppm, v->forb.co2_bio_coeff2);
    }

    if (debug) {
      swprintf("Shrub: use%d: bio_mult[%d] = %1.3f / coeff1 = %1.3f / coeff2 = %1.3f / ppm = %3.2f\n",
        c->use_bio_mult, year, v->shrub.co2_multipliers[BIO_INDEX][year],
        v->shrub.co2_bio_coeff1, v->shrub.co2_bio_coeff2, ppm);
    }

    if (c->use_wue_mult)
    {
      v->grass.co2_multipliers[WUE_INDEX][year] = v->grass.co2_wue_coeff1 * pow(ppm, v->grass.co2_wue_coeff2);
      v->shrub.co2_multipliers[WUE_INDEX][year] = v->shrub.co2_wue_coeff1 * pow(ppm, v->shrub.co2_wue_coeff2);
      v->tree.co2_multipliers[WUE_INDEX][year] = v->tree.co2_wue_coeff1 * pow(ppm, v->tree.co2_wue_coeff2);
      v->forb.co2_multipliers[WUE_INDEX][year] = v->forb.co2_wue_coeff1 * pow(ppm, v->forb.co2_wue_coeff2);
    }
  }
}
