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
#include "include/SW_datastructs.h" // for SW_CARBON_INPUTS, LOG_INFO, SW_MODEL
#include "include/SW_Defines.h"     // for ForEachVegType, MAX_FILENAMESIZE
#include "include/SW_Files.h"       // for eCarbon
#include "include/SW_VegProd.h"     // for BIO_INDEX, WUE_INDEX
#include <math.h>                   // for pow
#include <stdio.h>                  // for sscanf, FILE
#include <string.h>                 // for strcmp, memset

/* =================================================== */
/*             Global Function Definitions             */
/* --------------------------------------------------- */

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

void SW_CBN_deconstruct(void) {}

/**
@brief Reads yearly carbon data from disk file 'Input/carbon.in'

@param[in,out] SW_CarbonIn Struct of type SW_CARBON_INPUTS holding all
CO2-related data
@param[in] addtl_yr Represents how many years in the future we are simulating
@param[in] startYr Start year of the simulation
@param[in] endYr End year of the simulation
@param[in] txtInFiles Array of program in/output files
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
    int addtl_yr,
    TimeInt startYr,
    TimeInt endYr,
    char *txtInFiles[],
    LOG_INFO *LogInfo
) {
#ifdef SWDEBUG
    short debug = 0;
#endif

    // For efficiency, don't read carbon.in if neither multiplier is being used
    // We can do this because SW_VPD_construct already populated the multipliers
    // with default values
    if (!SW_CarbonIn->use_bio_mult && !SW_CarbonIn->use_wue_mult) {
#ifdef SWDEBUG
        if (debug) {
            sw_printf("'SW_CBN_read': CO2-effects are turned off; don't read "
                      "CO2-concentration data from file.\n");
        }
#endif
        return; // Exit function prematurely due to error
    }

    /* Reading carbon.in */
    FILE *f;
    char scenario[64];
    char yearStr[5];
    char ppmStr[20];
    int year;
    int scanRes;
    int simstartyr = (int) startYr + addtl_yr;
    int simendyr = (int) endYr + addtl_yr;

    // The following variables must be initialized to show if they've been
    // changed or not
    double ppm = 1.;
    int existing_years[MAX_NYEAR] = {0};
    short fileWasEmpty = 1;
    char *MyFileName;
    char inbuf[MAX_FILENAMESIZE];

    MyFileName = txtInFiles[eCarbon];
    f = OpenFile(MyFileName, "r", LogInfo);
    if (LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

#ifdef SWDEBUG
    if (debug) {
        sw_printf(
            "'SW_CBN_read': start reading CO2-concentration data from file.\n"
        );
    }
#endif

    while (GetALine(f, inbuf, MAX_FILENAMESIZE)) {
#ifdef SWDEBUG
        if (debug) {
            sw_printf("\ninbuf = %s", inbuf);
        }
#endif

        fileWasEmpty = 0;

        // Read the year standalone because if it's 0 it marks a change in the
        // scenario, in which case we'll need to read in a string instead of an
        // int
        scanRes = sscanf(inbuf, "%4s", yearStr);

        if (scanRes < 1) {
            LogError(
                LogInfo,
                LOGERROR,
                "Not enough values when reading in the year from %s.",
                MyFileName
            );
            goto closeFile;
        }

        year = sw_strtoi(yearStr, MyFileName, LogInfo);
        if (LogInfo->stopRun) {
            goto closeFile;
        }

        // Find scenario
        if (year == 0) {
            scanRes = sscanf(inbuf, "%4s %63s", yearStr, scenario);

            if (scanRes < 2) {
                LogError(
                    LogInfo,
                    LOGERROR,
                    "Not enough values when reading in the year and "
                    "scenario from %s.",
                    MyFileName
                );
                goto closeFile;
            }

            year = sw_strtoi(yearStr, MyFileName, LogInfo);
            if (LogInfo->stopRun) {
                goto closeFile;
            }

            /* Silence clang-tidy clang-analyzer-deadcode.DeadStores */
            (void) year;

            continue; // Skip to the ppm values
        }
        if (strcmp(scenario, SW_CarbonIn->scenario) != 0) {
            continue; // Keep searching for the right scenario
        }
        if ((year < simstartyr) || (year > simendyr)) {
            continue; // We aren't using this year
        }

        scanRes = sscanf(inbuf, "%4s %19s", yearStr, ppmStr);

        if (scanRes < 2) {
            LogError(
                LogInfo,
                LOGERROR,
                "Not enough values when reading in the year and "
                "ppm from %s.",
                MyFileName
            );
            goto closeFile;
        }

        year = sw_strtoi(yearStr, MyFileName, LogInfo);
        if (LogInfo->stopRun) {
            goto closeFile;
        }

        ppm = sw_strtod(ppmStr, MyFileName, LogInfo);
        if (LogInfo->stopRun) {
            goto closeFile;
        }

        if (year < 0) {
            LogError(
                LogInfo,
                LOGERROR,
                "(SW_Carbon) Year %d in scenario"
                " '%.64s' is negative; only positive values"
                " are allowed.\n",
                year,
                SW_CarbonIn->scenario
            );
            goto closeFile;
        }

        SW_CarbonIn->ppm[year] = ppm;
#ifdef SWDEBUG
        if (debug) {
            sw_printf(
                "  ==> SW_CarbonIn->ppm[%d] = %3.2f",
                year,
                SW_CarbonIn->ppm[year]
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
            LogError(
                LogInfo,
                LOGERROR,
                "(SW_Carbon) Year %d in scenario"
                " '%.64s' is entered more than once; only one"
                " entry is allowed.\n",
                year,
                SW_CarbonIn->scenario
            );
            goto closeFile;
        }
        existing_years[year] = 1;
    }

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
        goto closeFile;
    }

    // A scenario must be found in order for ppm to have a positive value
    if (EQ(ppm, -1.)) {
        LogError(
            LogInfo,
            LOGERROR,
            "(SW_Carbon) The scenario '%.64s'"
            " was not found in carbon.in\n",
            SW_CarbonIn->scenario
        );
        goto closeFile;
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
                SW_CarbonIn->scenario
            );
            goto closeFile;
        }
    }

closeFile: { CloseFile(&f, LogInfo); }
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
@param[in] SW_CarbonIn Struct of type SW_CARBON_INPUTS holding all CO2-related
data
@param[in] addtl_yr Represents how many years in the future we are simulating
@param[in] startYr Start year of the simulation
@param[in] endYr End year of the simulation
@param[out] LogInfo Holds information on warnings and errors
*/
void SW_CBN_init_run(
    VegType VegProd_veg[],
    SW_CARBON_INPUTS *SW_CarbonIn,
    int addtl_yr,
    TimeInt startYr,
    TimeInt endYr,
    LOG_INFO *LogInfo
) {
#ifdef SWDEBUG
    short debug = 0;
#endif

    int k;
    TimeInt year;
    TimeInt simendyr = endYr + addtl_yr;
    double ppm;

    if (!SW_CarbonIn->use_bio_mult && !SW_CarbonIn->use_wue_mult) {
        return;
    }

    // Only iterate through the years that we know will be used
    for (year = startYr + addtl_yr; year <= simendyr; year++) {
        ppm = SW_CarbonIn->ppm[year];

        if (LT(ppm, 0.)) // CO2 concentration must not be negative values
        {
            LogError(
                LogInfo,
                LOGERROR,
                "(SW_CarbonIn) No CO2 ppm data was"
                " provided for year %d\n",
                year
            );
            return; // Exit function prematurely due to error
        }

        // Calculate multipliers per PFT
        if (SW_CarbonIn->use_bio_mult) {
            ForEachVegType(k) {
                VegProd_veg[k].co2_multipliers[BIO_INDEX][year] =
                    VegProd_veg[k].co2_bio_coeff1 *
                    pow(ppm, VegProd_veg[k].co2_bio_coeff2);
            }
        }

#ifdef SWDEBUG
        if (debug) {
            sw_printf(
                "Shrub: use%d: bio_mult[%d] = %1.3f / coeff1 = %1.3f / coeff2 "
                "= %1.3f / ppm = %3.2f\n",
                SW_CarbonIn->use_bio_mult,
                year,
                VegProd_veg[SW_SHRUB].co2_multipliers[BIO_INDEX][year],
                VegProd_veg[SW_SHRUB].co2_bio_coeff1,
                VegProd_veg[SW_SHRUB].co2_bio_coeff2,
                ppm
            );
        }
#endif

        if (SW_CarbonIn->use_wue_mult) {
            ForEachVegType(k) {
                VegProd_veg[k].co2_multipliers[WUE_INDEX][year] =
                    VegProd_veg[k].co2_wue_coeff1 *
                    pow(ppm, VegProd_veg[k].co2_wue_coeff2);
            }
        }
    }
}
