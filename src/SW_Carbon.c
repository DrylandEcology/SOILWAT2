/**
@file   SW_Carbon.c
@author Zachary Kramer
@brief  Contains functions, constants, and variables that deal with the
effect of CO2 on transpiration and biomass.

Atmospheric carbon dioxide has been observed to affect water-use efficiency
and biomass, which is what this code attempts to simulate. The effects can
be varied by plant functional type. Most usages of the functions here are
in SW_VegProd.c and SW_Flow_lib.c.

@date   7 February 2017
*/


/* =================================================== */
/*                INCLUDES / DEFINES                   */
/* --------------------------------------------------- */
#include "include/SW_Carbon.h"      // for SW_CBN_construct, SW_CBN_deconst...
#include "include/filefuncs.h"      // for LogError, CloseFile, GetALine
#include "include/generic.h"        // for LOGERROR, EQ, LT
#include "include/myMemory.h"       // for sw_memccpy
#include "include/SW_datastructs.h" // for SW_CARBON, LOG_INFO, SW_MODEL
#include "include/SW_Defines.h"     // for ForEachVegType, MAX_FILENAMESIZE
#include "include/SW_Files.h"       // for eCarbon
#include "include/SW_VegProd.h"     // for BIO_INDEX, WUE_INDEX
#include <math.h>                   // for pow
#include <stdio.h>                  // for sscanf, FILE
#include <stdlib.h>
#include <string.h> // for strcmp, memset, strstr

/* =================================================== */
/*             Global Function Definitions             */
/* --------------------------------------------------- */

/**
@brief Allocate ppm array

@param[in] n_years Number of years in simulation
@param[in,out] ppm A 1D array holding atmospheric CO2 concentration
values (units ppm)
@param[in,out] LogInfo Holds information on warnings and errors
*/
void SW_CBN_alloc_ppm(TimeInt n_years, double **ppm, LOG_INFO *LogInfo) {
    *ppm = (double *) Mem_Calloc(n_years, sizeof(double), "alloc_ppm", LogInfo);
}

void SW_CBN_init_ptrs(SW_CARBON_INPUTS *SW_CarbonIn) {
    SW_CarbonIn->ppm = NULL;
}

/**
@brief Initializes the multipliers of the SW_CARBON_INPUTS structure.

@param[out] SW_CarbonIn Struct of type SW_CARBON_INPUTS holding all
CO2-related data

@note The spin-up year has been known to have the multipliers equal
to 0 without this constructor.
*/
void SW_CBN_construct(SW_CARBON_INPUTS *SW_CarbonIn) {
    memset(SW_CarbonIn, 0, sizeof(SW_CARBON_INPUTS));
}

/**
@brief Deallocate any information that was previously allocated

@param[in,out] SW_CarbonIn Struct of type SW_CARBON_INPUTS holding all
CO2-related data
*/
void SW_CBN_deconstruct(SW_CARBON_INPUTS *SW_CarbonIn) {
    if (!isnull(SW_CarbonIn->ppm)) {
        free((void *) SW_CarbonIn->ppm);
        SW_CarbonIn->ppm = NULL;
    }
}

void SW_CBN_setup(
    SW_CARBON_INPUTS *SW_CarbonIn,
    TimeInt startYr,
    TimeInt endYr,
    char *txtInFiles[],
    TimeInt vegYear,
    LOG_INFO *LogInfo
) {
    // For efficiency, don't read carbon.in if neither multiplier is being used
    // We can do this because SW_VPD_construct already populated the multipliers
    // with default values
    if (!SW_CarbonIn->use_bio_mult && !SW_CarbonIn->use_wue_mult) {
        return;
    }


    TimeInt n_years = endYr - startYr + 1;

    SW_CBN_alloc_ppm(n_years, &SW_CarbonIn->ppm, LogInfo);
    if (LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

    SW_CBN_read(SW_CarbonIn, startYr, endYr, txtInFiles, vegYear, LogInfo);
}

/**
@brief Reads yearly carbon data from disk file 'Input/carbon.in'

@param[in,out] SW_CarbonIn Struct of type SW_CARBON_INPUTS holding all
CO2-related data
@param[in] startYr Start year of the simulation
@param[in] endYr End year of the simulation
@param[in] txtInFiles Array of program in/output files
@param[in] vegYear Calendar year corresponding to vegetation inputs
@param[out] LogInfo Holds information on warnings and errors

Additionally, check for the following issues:
    1. Duplicate entries.
    2. Empty file.
    3. Missing scenario.
    4. Missing year.
    5. Negative year.
*/
void SW_CBN_read(
    SW_CARBON_INPUTS *SW_CarbonIn,
    TimeInt startYr,
    TimeInt endYr,
    char *txtInFiles[],
    TimeInt vegYear,
    LOG_INFO *LogInfo
) {
    FILE *f;
    char helpStr[64];
    char yearStr[5];
    char scenario[64] = {'\0'};
    char *MyFileName;
    char inbuf[MAX_FILENAMESIZE];
    TimeInt year;
    TimeInt yearBase0;
    int scanRes;
    TimeInt n_years = endYr - startYr + 1;

    // The following variables must be initialized to show if they've been
    // changed or not
    double ppm = -1.;
    int *existing_years = NULL;
    short fileWasEmpty = 1;
    Bool isAnotherScenario = swTRUE;
    SW_CarbonIn->ppmVegRef = -1.;


    MyFileName = txtInFiles[eCarbon];
    f = OpenFile(MyFileName, "r", LogInfo);
    if (LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

    existing_years =
        (int *) Mem_Calloc(n_years, sizeof(int), "SW_CBN_read()", LogInfo);
    if (LogInfo->stopRun) {
        goto cleanUp;
    }

    while (GetALine(f, inbuf, MAX_FILENAMESIZE)) {
        fileWasEmpty = 0;

        /* Implemented input formats: "0 scenarioName" or "Year [aCO2]" */
        scanRes = sscanf(inbuf, "%4s %63s", yearStr, helpStr);

        if (scanRes != 2) {
            LogError(
                LogInfo,
                LOGERROR,
                "(SW_Carbon) Expected two values but found '%s'.",
                inbuf
            );
            goto cleanUp;
        }

        year = (TimeInt) sw_strtoi(yearStr, MyFileName, LogInfo);
        if (LogInfo->stopRun) {
            goto cleanUp;
        }

        /* Identify the scenario(s) */
        if (year == 0) {
            (void) sw_memccpy(scenario, helpStr, '\0', sizeof scenario);

            /* Search for scenario in input "scenario1|scenario2" */
            isAnotherScenario =
                (Bool) (isnull(scenario) ||
                        isnull(strstr(SW_CarbonIn->scenario, scenario)));

            continue; // Skip to the ppm values
        }

        if (isAnotherScenario) {
            continue; // Keep searching for the right scenario
        }
        /* Requested scenario located - now look for requested years */
        if (!((year >= startYr && year <= endYr) || year == vegYear)) {
            continue; // We aren't using this year
        }

        /* Read aCO2 [ppm] */
        ppm = sw_strtod(helpStr, MyFileName, LogInfo);
        if (LogInfo->stopRun) {
            goto cleanUp;
        }

        /* Save aCO2 during vegetation reference year */
        if (year == vegYear) {
            SW_CarbonIn->ppmVegRef = ppm;
        }

        /* Look for sequence of simulation years */
        if ((year < startYr) || (year > endYr)) {
            continue; // We aren't using this year; prevent out-of-bounds
        }

        yearBase0 = year - startYr;

        /* Check if year already entered */
        if (existing_years[yearBase0] != 0) {
            LogError(
                LogInfo,
                LOGERROR,
                "(SW_Carbon) Duplicate year %d for scenario(s) '%.64s'.",
                year,
                SW_CarbonIn->scenario
            );
            goto cleanUp;
        }
        existing_years[yearBase0] = 1;

        SW_CarbonIn->ppm[yearBase0] = ppm;
    }


    /* Error checking */

    // Must check if the file was empty before checking if the scneario was
    // found, otherwise the empty file will be masked as not being able to find
    // the scenario
    if (fileWasEmpty == 1) {
        LogError(LogInfo, LOGERROR, "(SW_Carbon) '%s' was empty.", MyFileName);
        goto cleanUp;
    }

    // A scenario must be found in order for ppm to have a positive value
    if (EQ(ppm, -1.)) {
        LogError(
            LogInfo,
            LOGERROR,
            "(SW_Carbon) No scenario(s) '%.64s' and associated CO2 data found.",
            SW_CarbonIn->scenario
        );
        goto cleanUp;
    }

    // Ensure that the desired years were calculated
    for (yearBase0 = 0; yearBase0 < n_years; yearBase0++) {
        if (existing_years[yearBase0] == 0) {
            LogError(
                LogInfo,
                LOGERROR,
                "(SW_Carbon) No CO2 data for year %d and scenario(s) '%.64s'.",
                yearBase0 + startYr,
                SW_CarbonIn->scenario
            );
            goto cleanUp;
        }
    }

    // Check that vegetation reference aCO2 was located
    if (LE(SW_CarbonIn->ppmVegRef, 0.)) {
        LogError(
            LogInfo,
            LOGERROR,
            "(SW_Carbon) No CO2 data found for vegetation reference year %d.",
            vegYear
        );
        goto cleanUp;
    }

cleanUp: {
    CloseFile(&f, LogInfo);

    if (!isnull(existing_years)) {
        free((void *) existing_years);
    }
}
}

/**
@brief Calculate CO2-fertilization effects for biomass and water-use efficiency

Multipliers are calculated for each year and plant functional type:
\f[m(t,k) = f(t,k) / f(v,k)\f]
\f[f(t,k) = {coeff_1(k)} * {CO_2(t)} ^ {coeff_2(k)}\f]

where
    - \f$coeff_1(k)\f$ and \f$coeff_2(k)\f$ are
      coefficients for plant functional type \f$k\f$ based on a reference of
      360 ppm CO2 (which occurred just before 1995),
    - \f$CO_2(t)\f$ is atmospheric CO2 concentration in units [ppm] for
      calendar year \f$t\f$, and
    - \f$1 / f(v,k)\f$ is a correction factor to adjust for a difference between
      year of vegetation inputs \f$v\f$ and year of reference
      atmospheric CO2 concentration.

The multipliers become one during the year of vegetation inputs, i.e.,
\f$m(v,k) = 1\f$.

If a multiplier is disabled, its value is kept at the default value of 1.0.
Multipliers are only calculated for the years that will be simulated.

@param[in,out] vegIn A struct of type VegTypeIn holding arrays of size
NVEGTYPES to hold static simulation data for each simulation type
@param[in,out] vegSim A struct of type VegTypeSim holding NVEGTYPES amount of
values describing all vegetation types purely during simulations
@param[in] SW_CarbonIn Struct of type SW_CARBON_INPUTS holding all CO2-related
data
@param[in] startYr Start year of the simulation
@param[in] endYr End year of the simulation
@param[out] LogInfo Holds information on warnings and errors
*/
void SW_CBN_init_run(
    VegTypeIn *vegIn,
    VegTypeSim *vegSim,
    SW_CARBON_INPUTS *SW_CarbonIn,
    TimeInt startYr,
    TimeInt endYr,
    LOG_INFO *LogInfo
) {
#ifdef SWDEBUG
    short debug = 0;
#endif

    int k;
    TimeInt year;
    TimeInt endsimyr = endYr - startYr + 1;
    /* atmospheric CO2 concentration corresponding to vegetation input year */
    double vegCO2;
    double biomCorrectionFactor[NVEGTYPES];
    double wueCorrectionFactor[NVEGTYPES];
    double ppm;

    if (!SW_CarbonIn->use_bio_mult && !SW_CarbonIn->use_wue_mult) {
        return;
    }

    /* Adjustment for year of vegetation inputs relative to reference */
    vegCO2 = SW_CarbonIn->ppmVegRef;
    if (missing(vegCO2) || LE(vegCO2, 0.)) {
        LogError(
            LogInfo,
            LOGERROR,
            "Vegetation reference aCO2 (%f) is missing or not positive",
            vegCO2
        );
        return; // Exit function prematurely due to error
    }

    ForEachVegType(k) {
        biomCorrectionFactor[k] = 1. / (vegIn->co2_bio_coeff1[k] *
                                        pow(vegCO2, vegIn->co2_bio_coeff2[k]));

        wueCorrectionFactor[k] = 1. / (vegIn->co2_wue_coeff1[k] *
                                       pow(vegCO2, vegIn->co2_wue_coeff2[k]));
    }


    /* Calculate CO2 fertilization multipliers for each simulated year */
    for (year = 0; year < endsimyr; year++) {
        ppm = SW_CarbonIn->ppm[year];

        if (LT(ppm, 0.)) {
            LogError(
                LogInfo,
                LOGERROR,
                "Invalid CO2 ppm value was provided for year %d",
                year + startYr
            );
            return; // Exit function prematurely due to error
        }

        // Calculate multipliers per PFT
        if (SW_CarbonIn->use_bio_mult) {
            ForEachVegType(k) {
                vegSim->co2_multipliers[k][BIO_INDEX][year] =
                    biomCorrectionFactor[k] * vegIn->co2_bio_coeff1[k] *
                    pow(ppm, vegIn->co2_bio_coeff2[k]);
            }
        }

#ifdef SWDEBUG
        if (debug) {
            sw_printf(
                "Shrub: use%d: bio_mult[%d] = %1.3f / coeff1 = %1.3f / coeff2 "
                "= %1.3f / ppm = %3.2f, correctionFactor = %1.3f\n",
                SW_CarbonIn->use_bio_mult,
                year,
                vegSim.co2_multipliers[SW_SHRUB][BIO_INDEX][year],
                vegIn.co2_bio_coeff1[SW_SHRUB],
                vegIn.co2_bio_coeff2[SW_SHRUB],
                ppm,
                biomCorrectionFactor[SW_SHRUB]
            );
        }
#endif

        if (SW_CarbonIn->use_wue_mult) {
            ForEachVegType(k) {
                vegSim->co2_multipliers[k][WUE_INDEX][year] =
                    wueCorrectionFactor[k] * vegIn->co2_wue_coeff1[k] *
                    pow(ppm, vegIn->co2_wue_coeff2[k]);
            }
        }
    }
}
