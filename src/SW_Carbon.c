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

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "include/filefuncs.h"
#include "include/SW_Carbon.h"
#include "include/SW_Files.h"
#include "include/SW_Model.h"
#include "include/SW_VegProd.h"

/* =================================================== */
/*             Global Function Definitions             */
/* --------------------------------------------------- */

/**
@brief Initializes the multipliers of the SW_CARBON structure.

@param[out] SW_Carbon Struct of type SW_CARBON holding all CO2-related data

@note The spin-up year has been known to have the multipliers equal
to 0 without this constructor.
*/
void SW_CBN_construct(SW_CARBON *SW_Carbon) {
    memset(SW_Carbon, 0, sizeof(SW_CARBON));
}

void SW_CBN_deconstruct(void) {}

/**
@brief Reads yearly carbon data from disk file 'Input/carbon.in'

@param[in,out] SW_Carbon Struct of type SW_CARBON holding all CO2-related data
@param[in] SW_Model Struct of type SW_MODEL holding basic time information
    about the simulation
@param[in] InFiles Array of program in/output files
@param[out] LogInfo Holds information on warnings and errors

Additionally, check for the following issues:
    1. Duplicate entries.
    2. Empty file.
    3. Missing scenario.
    4. Missing year.
    5. Negative year.
*/
void SW_CBN_read(
    SW_CARBON *SW_Carbon, SW_MODEL *SW_Model, char *InFiles[], LOG_INFO *LogInfo
) {
#ifdef SWDEBUG
    short debug = 0;
#endif

    // For efficiency, don't read carbon.in if neither multiplier is being used
    // We can do this because SW_VPD_construct already populated the multipliers
    // with default values
    if (!SW_Carbon->use_bio_mult && !SW_Carbon->use_wue_mult) {
#ifdef SWDEBUG
        if (debug) {
            swprintf("'SW_CBN_read': CO2-effects are turned off; don't read "
                     "CO2-concentration data from file.\n");
        }
#endif
        return; // Exit function prematurely due to error
    }

    /* Reading carbon.in */
    FILE *f;
    char scenario[64];
    int year;
    int simstartyr = (int) SW_Model->startyr + SW_Model->addtl_yr;
    int simendyr = (int) SW_Model->endyr + SW_Model->addtl_yr;

    // The following variables must be initialized to show if they've been
    // changed or not
    double ppm = 1.;
    int existing_years[MAX_NYEAR] = {0};
    short fileWasEmpty = 1;
    char *MyFileName, inbuf[MAX_FILENAMESIZE];

    MyFileName = InFiles[eCarbon];
    f = OpenFile(MyFileName, "r", LogInfo);
    if (LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

#ifdef SWDEBUG
    if (debug) {
        swprintf(
            "'SW_CBN_read': start reading CO2-concentration data from file.\n"
        );
    }
#endif

    while (GetALine(f, inbuf, MAX_FILENAMESIZE)) {
#ifdef SWDEBUG
        if (debug) {
            swprintf("\ninbuf = %s", inbuf);
        }
#endif

        fileWasEmpty = 0;

        // Read the year standalone because if it's 0 it marks a change in the
        // scenario, in which case we'll need to read in a string instead of an
        // int
        sscanf(inbuf, "%d", &year);

        // Find scenario
        if (year == 0) {
            sscanf(inbuf, "%d %63s", &year, scenario);
            continue; // Skip to the ppm values
        }
        if (strcmp(scenario, SW_Carbon->scenario) != 0) {
            continue; // Keep searching for the right scenario
        }
        if ((year < simstartyr) || (year > simendyr)) {
            continue; // We aren't using this year
        }

        sscanf(inbuf, "%d %lf", &year, &ppm);

        if (year < 0) {
            CloseFile(&f, LogInfo);
            LogError(
                LogInfo,
                LOGERROR,
                "(SW_Carbon) Year %d in scenario"
                " '%.64s' is negative; only positive values"
                " are allowed.\n",
                year,
                SW_Carbon->scenario
            );
            return; // Exit function prematurely due to error
        }

        SW_Carbon->ppm[year] = ppm;
#ifdef SWDEBUG
        if (debug) {
            swprintf(
                "  ==> SW_Carbon->ppm[%d] = %3.2f", year, SW_Carbon->ppm[year]
            );
        }
#endif

        /* Has this year already been calculated?
           If yes: Do NOT overwrite values, fail the run instead

           Use a simple binary system with the year as the index, which avoids
           using a loop. We cannot simply check if co2_multipliers[0 or
           1][year].grass != 1.0, due to floating point precision and the chance
           that a multiplier of 1.0 was actually calculated */
        if (existing_years[year] != 0) {
            CloseFile(&f, LogInfo);
            LogError(
                LogInfo,
                LOGERROR,
                "(SW_Carbon) Year %d in scenario"
                " '%.64s' is entered more than once; only one"
                " entry is allowed.\n",
                year,
                SW_Carbon->scenario
            );
            return; // Exit function prematurely due to error
        }
        existing_years[year] = 1;
    }

    CloseFile(&f, LogInfo);


    /* Error checking */

    // Must check if the file was empty before checking if the scneario was
    // found, otherwise the empty file will be masked as not being able to find
    // the scenario
    if (fileWasEmpty == 1) {
        LogError(
            LogInfo,
            LOGERROR,
            "(SW_Carbon) carbon.in was empty; for"
            " debugging purposes, SOILWAT2 read in file '%s'\n",
            MyFileName
        );
        return; // Exit function prematurely due to error
    }

    // A scenario must be found in order for ppm to have a positive value
    if (EQ(ppm, -1.)) {
        LogError(
            LogInfo,
            LOGERROR,
            "(SW_Carbon) The scenario '%.64s'"
            " was not found in carbon.in\n",
            SW_Carbon->scenario
        );
        return; // Exit function prematurely due to error
    }

    // Ensure that the desired years were calculated
    for (year = simstartyr; year <= simendyr; year++) {
        if (existing_years[year] == 0) {
            LogError(
                LogInfo,
                LOGERROR,
                "(SW_Carbon) missing CO2 data for"
                " year %d; ensure that ppm values for this year"
                " exist in scenario '%.64s'\n",
                year,
                SW_Carbon->scenario
            );
            return; // Exit function prematurely due to error
        }
    }
}

/**
@brief Calculates the multipliers of the CO2-effect for biomass and water-use
efficiency.

Multipliers are calculated per year with the equation: Coeff1 * ppm^Coeff2
Where Coeff1 and Coeff2 are provided by the VegProd input. Coefficients
assume that monthly biomass reflect values for atmospheric conditions at 360
ppm CO2. Each PFT has its own set of coefficients. If a multiplier is
disabled, its value is kept at the default value of 1.0. Multipliers are only
calculated for the years that will be simulated.

@param[in,out] VegProd_veg Array of size NVEGTYPES holding data for each
    vegetation type
@param[in] SW_Model Struct of type SW_MODEL holding basic time information
    about the simulation
@param[in] SW_Carbon Struct of type SW_CARBON holding all CO2-related data
@param[out] LogInfo Holds information on warnings and errors
*/
void SW_CBN_init_run(
    VegType VegProd_veg[],
    SW_MODEL *SW_Model,
    SW_CARBON *SW_Carbon,
    LOG_INFO *LogInfo
) {
    int k;
    TimeInt year, simendyr = SW_Model->endyr + SW_Model->addtl_yr;
    double ppm;
#ifdef SWDEBUG
    short debug = 0;
#endif

    if (!SW_Carbon->use_bio_mult && !SW_Carbon->use_wue_mult) {
        return;
    }

    // Only iterate through the years that we know will be used
    for (year = SW_Model->startyr + SW_Model->addtl_yr; year <= simendyr;
         year++) {
        ppm = SW_Carbon->ppm[year];

        if (LT(ppm, 0.)) // CO2 concentration must not be negative values
        {
            LogError(
                LogInfo,
                LOGERROR,
                "(SW_Carbon) No CO2 ppm data was"
                " provided for year %d\n",
                year
            );
            return; // Exit function prematurely due to error
        }

        // Calculate multipliers per PFT
        if (SW_Carbon->use_bio_mult) {
            ForEachVegType(k) {
                VegProd_veg[k].co2_multipliers[BIO_INDEX][year] =
                    VegProd_veg[k].co2_bio_coeff1 *
                    pow(ppm, VegProd_veg[k].co2_bio_coeff2);
            }
        }

#ifdef SWDEBUG
        if (debug) {
            swprintf(
                "Shrub: use%d: bio_mult[%d] = %1.3f / coeff1 = %1.3f / coeff2 "
                "= %1.3f / ppm = %3.2f\n",
                SW_Carbon->use_bio_mult,
                year,
                VegProd_veg[SW_SHRUB].co2_multipliers[BIO_INDEX][year],
                VegProd_veg[SW_SHRUB].co2_bio_coeff1,
                VegProd_veg[SW_SHRUB].co2_bio_coeff2,
                ppm
            );
        }
#endif

        if (SW_Carbon->use_wue_mult) {
            ForEachVegType(k) {
                VegProd_veg[k].co2_multipliers[WUE_INDEX][year] =
                    VegProd_veg[k].co2_wue_coeff1 *
                    pow(ppm, VegProd_veg[k].co2_wue_coeff2);
            }
        }
    }
}
