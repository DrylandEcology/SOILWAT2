/********************************************************/
/********************************************************/
/*	Source file: Veg_Prod.c
Type: module
Application: SOILWAT - soilwater dynamics simulator
Purpose: Read / write and otherwise manage the model's
vegetation production parameter information.
History:

(8/28/01) -- INITIAL CODING - cwb

11/16/2010	(drs) added LAIforest, biofoliage_for, lai_conv_for,
TypeGrassOrShrub, TypeForest to SW_VEGPROD lai_live/biolive/total_agb include
now LAIforest, respectively biofoliage_for updated SW_VPD_read(), SW_VPD_init(),
and _echo_inits() increased length of char outstr[1000] to outstr[1500] because
of increased echo

02/22/2011	(drs) added scan for litter_for to SW_VPD_read()

02/22/2011	(drs) added litter_for to SW_VegProd->litter and to
SW_VegProd->tot_agb

02/22/2011	(drs) if TypeGrassOrShrub is turned off, then its biomass,
litter, etc. values are set to 0

08/22/2011	(drs) use variable veg_height [MAX_MONTHS] from SW_VEGPROD
instead of static canopy_ht

09/08/2011	(drs) adapted SW_VPD_read() and SW_VPD_init() to reflect that
now each vegetation type has own elements

09/08/2011	(drs) added input in SW_VPD_read() of tanfunc_t
tr_shade_effects, and RealD shade_scale and shade_deadmax (they were previously
hidden as constants in code in SW_Flow_lib.h)

09/08/2011	(drs) moved all input of hydraulic redistribution variables from
SW_Site.c to SW_VPD_read() for each vegetation type

09/08/2011	(drs) added input in SW_VPD_read() of RealD veg_intPPT_a,
veg_intPPT_b, veg_intPPT_c, veg_intPPT_d (they were previously hidden as
constants in code in SW_Flow_lib.h)

09/09/2011	(drs) added input in SW_VPD_read() of RealD
EsTpartitioning_param (it were previously hidden as constant in code in
SW_Flow_lib.h)

09/09/2011	(drs) added input in SW_VPD_read() of RealD Es_param_limit (it
was previously hidden as constant in code in SW_Flow_lib.h)

09/13/2011	(drs) added input in SW_VPD_read() of RealD litt_intPPT_a,
litt_intPPT_b, litt_intPPT_c, litt_intPPT_d (they were previously hidden as
constants in code in SW_Flow_lib.h)

09/13/2011	(drs) added input in SW_VPD_read() of RealD
canopy_height_constant and updated SW_VPD_init() (as option: if > 0 then
constant canopy height (cm) and overriding cnpy-tangens=f(biomass))

09/15/2011	(drs) added input in SW_VPD_read() of RealD albedo

09/26/2011	(drs)	added calls to Times.c:interpolate_monthlyValues() in
SW_VPD_init() for each monthly input variable; replaced monthly loop with a
daily loop for additional daily variables; adjusted _echo_inits()

10/17/2011	(drs)	in SW_VPD_init(): v->veg[SW_TREES].total_agb_daily[doy]
= v->veg[SW_TREES].litter_daily[doy] + v->veg[SW_TREES].biolive_daily[doy]
instead of = v->veg[SW_TREES].litter_daily[doy] +
v->veg[SW_TREES].biomass_daily[doy] to adjust for better scaling of potential
bare-soil evaporation

02/04/2012	(drs) 	added input in SW_VPD_read() of RealD SWPcrit

01/29/2013	(clk)	changed the read in to account for the extra fractional
component in total vegetation, bare ground added the variable RealF
help_bareGround as a place holder for the sscanf call.

01/31/2013	(clk)	changed the read in to account for the albedo for bare
ground, storing the input in bare_cov.albedo changed _echo_inits() to now
display the bare ground components in logfile.log

06/27/2013	(drs)	closed open files if LogError() with LOGERROR is called
in SW_VPD_read()

07/09/2013	(clk)	added initialization of all the values of the new
vegtype variable forb and forb.cov.fCover
*/
/********************************************************/
/********************************************************/


/* =================================================== */
/*                INCLUDES / DEFINES                   */
/* --------------------------------------------------- */
#include "include/SW_VegProd.h"     // for BIO_INDEX, SW_VPD_alloc_outptrs
#include "include/filefuncs.h"      // for LogError, CloseFile, GetALine
#include "include/generic.h"        // for LOGERROR, Bool, LOGWARN, RealF, GT
#include "include/myMemory.h"       // for Mem_Calloc, Mem_Malloc
#include "include/SW_datastructs.h" // for SW_VEGPROD, LOG_INFO, SW_VEGPROD...
#include "include/SW_Defines.h"     // for ForEachVegType, NVEGTYPES, SW_TREES
#include "include/SW_Files.h"       // for eVegProd
#include "include/SW_Weather.h"     // for deallocateClimateStructs, alloca...
#include "include/Times.h"          // for interpolate_monthlyValues, Jan, Dec
#include <math.h>                   // for log, pow
#include <stdio.h>                  // for sscanf, printf, NULL, FILE
#include <stdlib.h>                 // for free
#include <string.h>                 // for memset


/* =================================================== */
/*                  Global Variables                   */
/* --------------------------------------------------- */

// key2veg must be in the same order as the indices to vegetation types defined
// in SW_Defines.h
const char *const key2veg[NVEGTYPES] = {"Trees", "Shrubs", "Forbs", "Grasses"};

/* =================================================== */
/*             Global Function Definitions             */
/* --------------------------------------------------- */

/**
@brief Reads file for SW_VegProd

@param[in,out] SW_VegProd Struct of type SW_VEGPROD describing surface
    cover conditions in the simulation
@param[in] InFiles Array of program in/output files
@param[out] LogInfo Holds information on warnings and errors
*/
void SW_VPD_read(SW_VEGPROD *SW_VegProd, char *InFiles[], LOG_INFO *LogInfo) {
    /* =================================================== */

    const char *const lineErrStrings[] = {
        "vegetation type components",
        "vegetation type components",
        "albedo values",
        "canopy xinflec",
        "canopy yinflec",
        "canopy range",
        "canopy slope",
        "canopy height constant option",
        "interception parameter kSmax(veg)",
        "interception parameter kdead(veg)",
        "litter interception parameter kSmax(litter)",
        "parameter for partitioning of bare-soil evaporation and transpiration",
        "parameter for Parameter for scaling and limiting bare soil ",
        "evaporation rate",
        "shade scale",
        "shade max dead biomass",
        "shade xinflec",
        "shade yinflec",
        "shade range",
        "shade slope",
        "hydraulic redistribution: flag",
        "hydraulic redistribution: maxCondroot",
        "hydraulic redistribution: swpMatric50",
        "hydraulic redistribution: shapeCond",
        "critical soil water potentials: flag",
        "CO2 Biomass Coefficient 1",
        "CO2 Biomass Coefficient 2",
        "CO2 WUE Coefficient 1",
        "CO2 WUE Coefficient 2"
    };

    FILE *f;
    TimeInt mon = Jan;
    int x, k, lineno = 0, index;
    // last case line number before monthly biomass densities
    const int line_help = 28;
    RealF help_veg[NVEGTYPES], help_bareGround = 0., litt, biom, pctl, laic;
    RealF *monBioVals[] = {&litt, &biom, &pctl, &laic};
    char *MyFileName, inbuf[MAX_FILENAMESIZE];
    char vegStrs[NVEGTYPES][20] = {{'\0'}}, bareGroundStr[20] = {'\0'};
    char *startOfErrMsg;
    char vegMethodStr[20] = {'\0'};
    const int numMonthVals = 4;
    int expectedNumInVals;

    MyFileName = InFiles[eVegProd];
    f = OpenFile(MyFileName, "r", LogInfo);
    if (LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

    while (GetALine(f, inbuf, MAX_FILENAMESIZE)) {
        lineno++;

        if (lineno >= 2 && lineno <= 28) {
            x = sscanf(
                inbuf,
                "%19s %19s %19s %19s %19s",
                vegStrs[SW_GRASS],
                vegStrs[SW_SHRUB],
                vegStrs[SW_TREES],
                vegStrs[SW_FORBS],
                bareGroundStr
            );

            startOfErrMsg = (lineno >= 25) ? (char *) "Not enough arguments" :
                                             (char *) "Invalid record in";

            expectedNumInVals = (lineno >= 4) ? NVEGTYPES : NVEGTYPES + 1;

            if (x < expectedNumInVals) {
                LogError(
                    LogInfo,
                    LOGERROR,
                    "%s %s in %s\n",
                    startOfErrMsg,
                    lineErrStrings[lineno - 1],
                    MyFileName
                );
                goto closeFile;
            }

            ForEachVegType(k) {
                help_veg[k] = sw_strtof(vegStrs[k], MyFileName, LogInfo);
                if (LogInfo->stopRun) {
                    goto closeFile;
                }
            }

            if (x == NVEGTYPES + 1) {
                help_bareGround = sw_strtof(bareGroundStr, MyFileName, LogInfo);
                if (LogInfo->stopRun) {
                    goto closeFile;
                }
            }
        }

        if (lineno - 1 < line_help) { /* Compare to `line_help` in base0 */
            switch (lineno) {
            case 1:
                x = sscanf(inbuf, "%19s", vegMethodStr);
                if (x != 1) {
                    LogError(
                        LogInfo,
                        LOGERROR,
                        "invalid record in %s in %s\n",
                        lineErrStrings[0],
                        MyFileName
                    );
                    goto closeFile;
                }

                SW_VegProd->veg_method =
                    sw_strtoi(vegMethodStr, MyFileName, LogInfo);
                if (LogInfo->stopRun) {
                    goto closeFile;
                }
                break;

            /* fractions of vegetation types */
            case 2:
                ForEachVegType(k) {
                    SW_VegProd->veg[k].cov.fCover = help_veg[k];
                }
                SW_VegProd->bare_cov.fCover = help_bareGround;
                break;

            /* albedo */
            case 3:
                ForEachVegType(k) {
                    SW_VegProd->veg[k].cov.albedo = help_veg[k];
                }
                SW_VegProd->bare_cov.albedo = help_bareGround;
                break;

            /* canopy height */
            case 4:
                ForEachVegType(k) {
                    SW_VegProd->veg[k].cnpy.xinflec = help_veg[k];
                }
                break;

            case 5:
                ForEachVegType(k) {
                    SW_VegProd->veg[k].cnpy.yinflec = help_veg[k];
                }
                break;

            case 6:
                ForEachVegType(k) {
                    SW_VegProd->veg[k].cnpy.range = help_veg[k];
                }
                break;

            case 7:
                ForEachVegType(k) {
                    SW_VegProd->veg[k].cnpy.slope = help_veg[k];
                }
                break;

            case 8:
                ForEachVegType(k) {
                    SW_VegProd->veg[k].canopy_height_constant = help_veg[k];
                }
                break;

            /* vegetation interception parameters */
            case 9:
                ForEachVegType(k) {
                    SW_VegProd->veg[k].veg_kSmax = help_veg[k];
                }
                break;

            case 10:
                ForEachVegType(k) {
                    SW_VegProd->veg[k].veg_kdead = help_veg[k];
                }
                break;

            /* litter interception parameters */
            case 11:
                ForEachVegType(k) {
                    SW_VegProd->veg[k].lit_kSmax = help_veg[k];
                }
                break;

            /* parameter for partitioning of bare-soil evaporation and
             * transpiration */
            case 12:
                ForEachVegType(k) {
                    SW_VegProd->veg[k].EsTpartitioning_param = help_veg[k];
                }
                break;

            /* Parameter for scaling and limiting bare soil evaporation rate */
            case 13:
                ForEachVegType(k) {
                    SW_VegProd->veg[k].Es_param_limit = help_veg[k];
                }
                break;

            /* shade effects */
            case 14:
                ForEachVegType(k) {
                    SW_VegProd->veg[k].shade_scale = help_veg[k];
                }
                break;

            case 15:
                ForEachVegType(k) {
                    SW_VegProd->veg[k].shade_deadmax = help_veg[k];
                }
                break;

            case 16:
                ForEachVegType(k) {
                    SW_VegProd->veg[k].tr_shade_effects.xinflec = help_veg[k];
                }
                break;

            case 17:
                ForEachVegType(k) {
                    SW_VegProd->veg[k].tr_shade_effects.yinflec = help_veg[k];
                }
                break;

            case 18:
                ForEachVegType(k) {
                    SW_VegProd->veg[k].tr_shade_effects.range = help_veg[k];
                }
                break;

            case 19:
                ForEachVegType(k) {
                    SW_VegProd->veg[k].tr_shade_effects.slope = help_veg[k];
                }
                break;

            /* Hydraulic redistribution */
            case 20:
                ForEachVegType(k) {
                    SW_VegProd->veg[k].flagHydraulicRedistribution =
                        (Bool) EQ(help_veg[k], 1.);
                }
                break;

            case 21:
                ForEachVegType(k) {
                    SW_VegProd->veg[k].maxCondroot = help_veg[k];
                }
                break;

            case 22:
                ForEachVegType(k) {
                    SW_VegProd->veg[k].swpMatric50 = help_veg[k];
                }
                break;

            case 23:
                ForEachVegType(k) {
                    SW_VegProd->veg[k].shapeCond = help_veg[k];
                }
                break;

            /* Critical soil water potential */
            case 24:
                ForEachVegType(k) {
                    SW_VegProd->veg[k].SWPcrit = -10. * help_veg[k];
                    SW_VegProd->critSoilWater[k] =
                        help_veg[k]; // for use with get_swa for properly
                                     // partitioning available soilwater
                }
                get_critical_rank(SW_VegProd);
                break;

            /* CO2 Biomass Power Equation */
            // Coefficient 1
            case 25:
                ForEachVegType(k) {
                    SW_VegProd->veg[k].co2_bio_coeff1 = help_veg[k];
                }
                break;

            // Coefficient 2
            case 26:
                ForEachVegType(k) {
                    SW_VegProd->veg[k].co2_bio_coeff2 = help_veg[k];
                }
                break;

            /* CO2 WUE Power Equation */
            // Coefficient 1
            case 27:
                ForEachVegType(k) {
                    SW_VegProd->veg[k].co2_wue_coeff1 = help_veg[k];
                }
                break;

            // Coefficient 2
            case 28:
                ForEachVegType(k) {
                    SW_VegProd->veg[k].co2_wue_coeff2 = help_veg[k];
                }
                break;

            default:
                break;
            }

        } else {
            if (lineno == line_help + 1 || lineno == line_help + 1 + 12 ||
                lineno == line_help + 1 + 12 * 2 ||
                lineno == line_help + 1 + 12 * 3) {
                mon = Jan;
            }

            x = sscanf(
                inbuf,
                "%19s %19s %19s %19s",
                vegStrs[0],
                vegStrs[1],
                vegStrs[2],
                vegStrs[3]
            );

            if (x < NVEGTYPES) {
                LogError(
                    LogInfo,
                    LOGERROR,
                    "ERROR: invalid record %d in %s\n",
                    mon + 1,
                    MyFileName
                );
                goto closeFile;
            }

            for (index = 0; index < numMonthVals; index++) {
                *(monBioVals[index]) =
                    sw_strtof(vegStrs[index], MyFileName, LogInfo);
                if (LogInfo->stopRun) {
                    goto closeFile;
                }
            }

            if (lineno > line_help + 12 * 3 && lineno <= line_help + 12 * 4) {
                SW_VegProd->veg[SW_FORBS].litter[mon] = litt;
                SW_VegProd->veg[SW_FORBS].biomass[mon] = biom;
                SW_VegProd->veg[SW_FORBS].pct_live[mon] = pctl;
                SW_VegProd->veg[SW_FORBS].lai_conv[mon] = laic;
            } else if (lineno > line_help + 12 * 2 &&
                       lineno <= line_help + 12 * 3) {
                SW_VegProd->veg[SW_TREES].litter[mon] = litt;
                SW_VegProd->veg[SW_TREES].biomass[mon] = biom;
                SW_VegProd->veg[SW_TREES].pct_live[mon] = pctl;
                SW_VegProd->veg[SW_TREES].lai_conv[mon] = laic;
            } else if (lineno > line_help + 12 &&
                       lineno <= line_help + 12 * 2) {
                SW_VegProd->veg[SW_SHRUB].litter[mon] = litt;
                SW_VegProd->veg[SW_SHRUB].biomass[mon] = biom;
                SW_VegProd->veg[SW_SHRUB].pct_live[mon] = pctl;
                SW_VegProd->veg[SW_SHRUB].lai_conv[mon] = laic;
            } else if (lineno > line_help && lineno <= line_help + 12) {
                SW_VegProd->veg[SW_GRASS].litter[mon] = litt;
                SW_VegProd->veg[SW_GRASS].biomass[mon] = biom;
                SW_VegProd->veg[SW_GRASS].pct_live[mon] = pctl;
                SW_VegProd->veg[SW_GRASS].lai_conv[mon] = laic;
            }

            mon++;
        }
    }

    if (mon < Dec) {
        LogError(
            LogInfo,
            LOGWARN,
            "No Veg Production"
            " values after month %d\n",
            mon + 1
        );
    }

    SW_VPD_fix_cover(SW_VegProd, LogInfo);

closeFile: { CloseFile(&f, LogInfo); }
}

/**
@brief Check that sum of land cover is 1; adjust if not.

@param[in,out] SW_VegProd SW_VegProd Struct of type SW_VEGPROD describing
    surface cover conditions in the simulation
@param[out] LogInfo Holds information on warnings and errors

@sideeffect
- Adjusts `SW_VegProd->bare_cov.fCover` and `SW_VegProd->veg[k].cov.fCover`
  to sum to 1.
- Print a warning that values are adjusted and notes with the new values.
*/
void SW_VPD_fix_cover(SW_VEGPROD *SW_VegProd, LOG_INFO *LogInfo) {
    int k;
    RealD fraction_sum = 0.;

    fraction_sum = SW_VegProd->bare_cov.fCover;
    ForEachVegType(k) { fraction_sum += SW_VegProd->veg[k].cov.fCover; }

    if (!EQ_w_tol(fraction_sum, 1.0, 1e-4)) {
        // inputs are never more precise than at most 3-4 digits

        LogError(
            LogInfo,
            LOGWARN,
            "Fractions of land cover components were normalized:\n"
            "\tSum of fractions was %.4f instead of 1.0. "
            "New coefficients are:",
            fraction_sum
        );

        SW_VegProd->bare_cov.fCover /= fraction_sum;
        LogError(
            LogInfo,
            LOGWARN,
            "Bare ground fraction = %.4f",
            SW_VegProd->bare_cov.fCover
        );

        ForEachVegType(k) {
            SW_VegProd->veg[k].cov.fCover /= fraction_sum;
            LogError(
                LogInfo,
                LOGWARN,
                "%s fraction = %.4f",
                key2veg[k],
                SW_VegProd->veg[k].cov.fCover
            );
        }
    }
}

/**
@brief Initialize all possible pointers in SW_VEGPROD to NULL

@param[in,out] SW_VegProd SW_VegProd SW_VegProd Struct of type SW_VEGPROD
    describing surface cover conditions in the simulation
*/
void SW_VPD_init_ptrs(SW_VEGPROD *SW_VegProd) {
    OutPeriod pd;

    // Initialize output structures:
    ForEachOutPeriod(pd) {
        SW_VegProd->p_accu[pd] = NULL;
        SW_VegProd->p_oagg[pd] = NULL;
    }
}

/**
@brief Constructor for SW_VegProd->

@param[out] SW_VegProd SW_VegProd Struct of type SW_VEGPROD describing surface
    cover conditions in the simulation
*/
void SW_VPD_construct(SW_VEGPROD *SW_VegProd) {
    /* =================================================== */

    // Clear the module structure:
    memset(SW_VegProd, 0, sizeof(SW_VEGPROD));
}

/**
@brief Allocate dynamic memory for output pointers in the SW_VEGPROD struct

@param[out] SW_VegProd SW_VegProd Struct of type SW_VEGPROD describing
    surface cover conditions in the simulation
@param[out] LogInfo Holds information on warnings and errors
*/
void SW_VPD_alloc_outptrs(SW_VEGPROD *SW_VegProd, LOG_INFO *LogInfo) {
    OutPeriod pd;

    // Allocate output structures:
    ForEachOutPeriod(pd) {
        SW_VegProd->p_accu[pd] = (SW_VEGPROD_OUTPUTS *) Mem_Calloc(
            1, sizeof(SW_VEGPROD_OUTPUTS), "SW_VPD_alloc_outptrs()", LogInfo
        );

        if (LogInfo->stopRun) {
            return; // Exit function prematurely due to error
        }
        if (pd > eSW_Day) {
            SW_VegProd->p_oagg[pd] = (SW_VEGPROD_OUTPUTS *) Mem_Calloc(
                1, sizeof(SW_VEGPROD_OUTPUTS), "SW_VPD_alloc_outptrs()", LogInfo
            );

            if (LogInfo->stopRun) {
                return; // Exit function prematurely due to error
            }
        }
    }
}

void SW_VPD_init_run(
    SW_VEGPROD *SW_VegProd,
    SW_WEATHER *SW_Weather,
    SW_MODEL *SW_Model,
    LOG_INFO *LogInfo
) {

    TimeInt year;
    int k;

    /* Set co2-multipliers to default */
    for (year = 0; year < MAX_NYEAR; year++) {
        ForEachVegType(k) {
            SW_VegProd->veg[k].co2_multipliers[BIO_INDEX][year] = 1.;
            SW_VegProd->veg[k].co2_multipliers[WUE_INDEX][year] = 1.;
        }
    }

    if (SW_VegProd->veg_method > 0) {
        estimateVegetationFromClimate(
            SW_VegProd, SW_Weather->allHist, SW_Model, LogInfo
        );
    }
}

/**
@brief Deconstructor for SW_VegProd->

@param[out] SW_VegProd Struct of type SW_VEGPROD describing surface
    cover conditions in the simulation
*/
void SW_VPD_deconstruct(SW_VEGPROD *SW_VegProd) {
    OutPeriod pd;

    // De-allocate output structures:
    ForEachOutPeriod(pd) {
        if (pd > eSW_Day && !isnull(SW_VegProd->p_oagg[pd])) {
            free(SW_VegProd->p_oagg[pd]);
            SW_VegProd->p_oagg[pd] = NULL;
        }

        if (!isnull(SW_VegProd->p_accu[pd])) {
            free(SW_VegProd->p_accu[pd]);
            SW_VegProd->p_accu[pd] = NULL;
        }
    }
}

/**
@brief Applies CO2 effects to supplied biomass data.

Two biomass parameters are needed so that we do not have a compound effect
on the biomass.

@param new_biomass  The resulting biomass after applying the multiplier.
@param biomass      The biomass to be modified (representing the value under
    reference conditions (i.e., 360 ppm CO<SUB>2</SUB>, currently).
@param multiplier   The biomass multiplier for this PFT.

@sideeffect new_biomass Updated biomass.
*/
void apply_biomassCO2effect(
    double new_biomass[], const double biomass[], double multiplier
) {
    int i;
    for (i = 0; i < 12; i++) {
        new_biomass[i] = (biomass[i] * multiplier);
    }
}

/**
@brief Update vegetation parameters for new year

@param[in,out] SW_VegProd SW_VegProd Struct of type SW_VEGPROD describing
    surface cover conditions in the simulation
@param[in] SW_Model Struct of type SW_MODEL holding basic time information
    about the simulation
*/
void SW_VPD_new_year(SW_VEGPROD *SW_VegProd, SW_MODEL *SW_Model) {
    /* ================================================== */
    /*
    * History:
    *     Originally included in the FORTRAN model.
    *
    *     20-Oct-03 (cwb) removed the calculation of
    *        lai_corr and changed the lai_conv value of 80
    *        as found in the prod.in file.  The conversion
    *        factor is now simply a divisor rather than
    *        an equation.  Removed the following code:
    lai_corr = SW_VegProd->lai_conv[m] * (1. - pstem) + aconst * pstem;
    lai_standing    = SW_VegProd->biomass[m] / lai_corr;
    where pstem = 0.3,
    aconst = 464.0,
    conv_stcr = 3.0;
    *
    */

    TimeInt doy; /* base1 */
    TimeInt simyear = SW_Model->simyear;
    int k;

    // Interpolation is to be in base1 in `interpolate_monthlyValues()`
    Bool interpAsBase1 = swTRUE;

    /* Monthly biomass after CO2 effects */
    double biomass_after_CO2[MAX_MONTHS];


    // Grab the real year so we can access CO2 data
    ForEachVegType(k) {
        if (GT(SW_VegProd->veg[k].cov.fCover, 0.)) {
            if (k == SW_TREES) {
                // CO2 effects on tree biomass restricted to percent live
                // biomass, i.e., total tree biomass is constant while live
                // biomass is increasing
                apply_biomassCO2effect(
                    biomass_after_CO2,
                    SW_VegProd->veg[k].pct_live,
                    SW_VegProd->veg[k].co2_multipliers[BIO_INDEX][simyear]
                );

                interpolate_monthlyValues(
                    biomass_after_CO2,
                    interpAsBase1,
                    SW_Model->cum_monthdays,
                    SW_Model->days_in_month,
                    SW_VegProd->veg[k].pct_live_daily
                );
                interpolate_monthlyValues(
                    SW_VegProd->veg[k].biomass,
                    interpAsBase1,
                    SW_Model->cum_monthdays,
                    SW_Model->days_in_month,
                    SW_VegProd->veg[k].biomass_daily
                );

            } else {
                // CO2 effects on biomass applied to total biomass, i.e.,
                // total and live biomass are increasing
                apply_biomassCO2effect(
                    biomass_after_CO2,
                    SW_VegProd->veg[k].biomass,
                    SW_VegProd->veg[k].co2_multipliers[BIO_INDEX][simyear]
                );

                interpolate_monthlyValues(
                    biomass_after_CO2,
                    interpAsBase1,
                    SW_Model->cum_monthdays,
                    SW_Model->days_in_month,
                    SW_VegProd->veg[k].biomass_daily
                );
                interpolate_monthlyValues(
                    SW_VegProd->veg[k].pct_live,
                    interpAsBase1,
                    SW_Model->cum_monthdays,
                    SW_Model->days_in_month,
                    SW_VegProd->veg[k].pct_live_daily
                );
            }

            // Interpolation of remaining variables from monthly to daily values
            interpolate_monthlyValues(
                SW_VegProd->veg[k].litter,
                interpAsBase1,
                SW_Model->cum_monthdays,
                SW_Model->days_in_month,
                SW_VegProd->veg[k].litter_daily
            );
            interpolate_monthlyValues(
                SW_VegProd->veg[k].lai_conv,
                interpAsBase1,
                SW_Model->cum_monthdays,
                SW_Model->days_in_month,
                SW_VegProd->veg[k].lai_conv_daily
            );
        }
    }

    for (doy = 1; doy <= MAX_DAYS; doy++) {
        ForEachVegType(k) {
            if (GT(SW_VegProd->veg[k].cov.fCover, 0.)) {
                /* vegetation height = 'veg_height_daily' is used for
                 * 'snowdepth_scale'; historically, also for 'vegcov' */
                if (GT(SW_VegProd->veg[k].canopy_height_constant, 0.)) {
                    SW_VegProd->veg[k].veg_height_daily[doy] =
                        SW_VegProd->veg[k].canopy_height_constant;

                } else {
                    SW_VegProd->veg[k].veg_height_daily[doy] = tanfunc(
                        SW_VegProd->veg[k].biomass_daily[doy],
                        SW_VegProd->veg[k].cnpy.xinflec,
                        SW_VegProd->veg[k].cnpy.yinflec,
                        SW_VegProd->veg[k].cnpy.range,
                        SW_VegProd->veg[k].cnpy.slope
                    );
                }

                /* live biomass = 'biolive_daily' is used for
                 * canopy-interception, transpiration, bare-soil evaporation,
                 * and hydraulic redistribution */
                SW_VegProd->veg[k].biolive_daily[doy] =
                    SW_VegProd->veg[k].biomass_daily[doy] *
                    SW_VegProd->veg[k].pct_live_daily[doy];

                /* dead biomass = 'biodead_daily' is used for
                 * canopy-interception and transpiration */
                SW_VegProd->veg[k].biodead_daily[doy] =
                    SW_VegProd->veg[k].biomass_daily[doy] -
                    SW_VegProd->veg[k].biolive_daily[doy];

                /* live leaf area index = 'lai_live_daily' is used for E-T
                 * partitioning */
                SW_VegProd->veg[k].lai_live_daily[doy] =
                    SW_VegProd->veg[k].biolive_daily[doy] /
                    SW_VegProd->veg[k].lai_conv_daily[doy];

                /* compound leaf area index = 'bLAI_total_daily' is used for
                 * canopy-interception */
                SW_VegProd->veg[k].bLAI_total_daily[doy] =
                    SW_VegProd->veg[k].lai_live_daily[doy] +
                    SW_VegProd->veg[k].veg_kdead *
                        SW_VegProd->veg[k].biodead_daily[doy] /
                        SW_VegProd->veg[k].lai_conv_daily[doy];

                /* total above-ground biomass = 'total_agb_daily' is used for
                 * bare-soil evaporation */
                if (k == SW_TREES) {
                    SW_VegProd->veg[k].total_agb_daily[doy] =
                        SW_VegProd->veg[k].litter_daily[doy] +
                        SW_VegProd->veg[k].biolive_daily[doy];
                } else {
                    SW_VegProd->veg[k].total_agb_daily[doy] =
                        SW_VegProd->veg[k].litter_daily[doy] +
                        SW_VegProd->veg[k].biomass_daily[doy];
                }

            } else {
                SW_VegProd->veg[k].lai_live_daily[doy] = 0.;
                SW_VegProd->veg[k].bLAI_total_daily[doy] = 0.;
                SW_VegProd->veg[k].biolive_daily[doy] = 0.;
                SW_VegProd->veg[k].biodead_daily[doy] = 0.;
                SW_VegProd->veg[k].total_agb_daily[doy] = 0.;
            }
        }
    }
}

/**
@brief Sum up values across vegetation types

@param[in] x Array of size \ref NVEGTYPES
@param[in] layerno Current layer which is being worked with
@return Sum across `*x`
*/
RealD sum_across_vegtypes(RealD x[][MAX_LAYERS], LyrIndex layerno) {
    unsigned int k;
    RealD sum = 0.;

    ForEachVegType(k) { sum += x[k][layerno]; }

    return sum;
}

/**
@brief Text output for VegProd.

@param[in] VegProd_veg Biomass [g/m2] per vegetation type as
    observed in total vegetation
@param[in] VegProd_bare_cov Bare-ground cover of plot that is not
    occupied by vegetation
*/
void echo_VegProd(VegType VegProd_veg[], CoverType VegProd_bare_cov) {
    /* ================================================== */

    int k;

    printf("\n==============================================\n"
           "Vegetation Production Parameters\n");

    ForEachVegType(k) {
        printf(
            "%s component\t= %1.2f\n"
            "\tAlbedo\t= %1.2f\n"
            "\tHydraulic redistribution flag\t= %d",
            key2veg[k],
            VegProd_veg[k].cov.fCover,
            VegProd_veg[k].cov.albedo,
            VegProd_veg[k].flagHydraulicRedistribution
        );
    }

    printf(
        "Bare Ground component\t= %1.2f\n"
        "\tAlbedo\t= %1.2f\n",
        VegProd_bare_cov.fCover,
        VegProd_bare_cov.albedo
    );
}

/**
@brief Determine vegetation type of decreasingly ranked the critical SWP

@param[in,out] SW_VegProd Struct of type SW_VEGPROD describing surface
    cover conditions in the simulation

@sideeffect Sets `SW_VegProd->rank_SWPcrits[]` based on
`SW_VegProd->critSoilWater[]`
*/
void get_critical_rank(SW_VEGPROD *SW_VegProd) {
    /*----------------------------------------------------------
            Get proper order for rank_SWPcrits
    ----------------------------------------------------------*/
    int i, outerLoop, innerLoop;
    float key;

    // need two temp arrays equal to critSoilWater since we dont want to alter
    // the original at all
    RealF tempArray[NVEGTYPES], tempArrayUnsorted[NVEGTYPES];

    ForEachVegType(i) {
        tempArray[i] = SW_VegProd->critSoilWater[i];
        tempArrayUnsorted[i] = SW_VegProd->critSoilWater[i];
    }

    // insertion sort to rank the veg types and store them in their proper order
    for (outerLoop = 1; outerLoop < NVEGTYPES; outerLoop++) {
        key = tempArray[outerLoop]; // set key equal to critical value
        innerLoop = outerLoop - 1;
        while (innerLoop >= 0 && tempArray[innerLoop] < key) {
            // code to switch values
            tempArray[innerLoop + 1] = tempArray[innerLoop];
            innerLoop = innerLoop - 1;
        }
        tempArray[innerLoop + 1] = key;
    }

    // loops to compare sorted v unsorted array and find proper index
    for (outerLoop = 0; outerLoop < NVEGTYPES; outerLoop++) {
        for (innerLoop = 0; innerLoop < NVEGTYPES; innerLoop++) {
            if (tempArray[outerLoop] == tempArrayUnsorted[innerLoop]) {
                SW_VegProd->rank_SWPcrits[outerLoop] = innerLoop;
                // set value to something impossible so if a duplicate a
                // different index is picked next
                tempArrayUnsorted[innerLoop] = SW_MISSING;
                break;
            }
        }
    }
    /*printf("%d = %f\n", SW_VegProd->rank_SWPcrits[0],
    SW_VegProd->critSoilWater[SW_VegProd->rank_SWPcrits[0]]); printf("%d =
    %f\n", SW_VegProd->rank_SWPcrits[1],
    SW_VegProd->critSoilWater[SW_VegProd->rank_SWPcrits[1]]); printf("%d =
    %f\n", SW_VegProd->rank_SWPcrits[2],
    SW_VegProd->critSoilWater[SW_VegProd->rank_SWPcrits[2]]); printf("%d =
    %f\n\n", SW_VegProd->rank_SWPcrits[3],
    SW_VegProd->critSoilWater[SW_VegProd->rank_SWPcrits[3]]);*/
    /*----------------------------------------------------------
            End of rank_SWPcrits
    ----------------------------------------------------------*/
}

/**
@brief Wrapper function for estimating natural vegetation. First, climate is
calculated and averaged, then values are estimated

@param[in,out] SW_VegProd Structure holding all values for vegetation cover of
    simulation
@param[in,out] Weather_hist Array containing all historical data of a site
@param[in] SW_Model Struct of type SW_MODEL holding basic time information
    about the simulation
@param[in] LogInfo Holds information on warnings and errors
*/
void estimateVegetationFromClimate(
    SW_VEGPROD *SW_VegProd,
    SW_WEATHER_HIST **Weather_hist,
    SW_MODEL *SW_Model,
    LOG_INFO *LogInfo
) {

    int numYears = SW_Model->endyr - SW_Model->startyr + 1, k,
        bareGroundIndex = 7;

    SW_CLIMATE_YEARLY climateOutput;
    SW_CLIMATE_CLIM climateAverages;

    // NOTE: 8 = number of types, 5 = (number of types) - grasses

    double coverValues[8] =
        {SW_MISSING,
         SW_MISSING,
         SW_MISSING,
         SW_MISSING,
         0.0,
         SW_MISSING,
         0.0,
         0.0},
           shrubLimit = .2;

    double SumGrassesFraction = SW_MISSING, C4Variables[3], grassOutput[3],
           RelAbundanceL0[8], RelAbundanceL1[5];

    Bool fillEmptyWithBareGround = swTRUE, warnExtrapolation = swTRUE;
    Bool inNorthHem = swTRUE;
    Bool fixBareGround = swTRUE;

    if (SW_Model->latitude < 0.0) {
        inNorthHem = swFALSE;
    }

    // Allocate climate structs' memory
    allocateClimateStructs(numYears, &climateOutput, &climateAverages, LogInfo);
    if (LogInfo->stopRun) {
        // Deallocate climate structs' memory before error
        deallocateClimateStructs(&climateOutput, &climateAverages);
        return; // Exit function prematurely due to error
    }

    calcSiteClimate(
        Weather_hist,
        SW_Model->cum_monthdays,
        SW_Model->days_in_month,
        numYears,
        SW_Model->startyr,
        inNorthHem,
        &climateOutput
    );

    averageClimateAcrossYears(&climateOutput, numYears, &climateAverages);

    if (SW_VegProd->veg_method == 1) {

        C4Variables[0] = climateAverages.minTemp7thMon_C;
        C4Variables[1] = climateAverages.ddAbove65F_degday;
        C4Variables[2] = climateAverages.frostFree_days;

        estimatePotNatVegComposition(
            climateAverages.meanTemp_C,
            climateAverages.PPT_cm,
            climateAverages.meanTempMon_C,
            climateAverages.PPTMon_cm,
            coverValues,
            shrubLimit,
            SumGrassesFraction,
            C4Variables,
            fillEmptyWithBareGround,
            inNorthHem,
            warnExtrapolation,
            fixBareGround,
            grassOutput,
            RelAbundanceL0,
            RelAbundanceL1,
            LogInfo
        );

        if (LogInfo->stopRun) {
            // Deallocate climate structs' memory before error
            deallocateClimateStructs(&climateOutput, &climateAverages);
            return; // Exit function prematurely due to error
        }

        ForEachVegType(k) { SW_VegProd->veg[k].cov.fCover = RelAbundanceL1[k]; }

        SW_VegProd->bare_cov.fCover = RelAbundanceL0[bareGroundIndex];
    }

    // Deallocate climate structs' memory
    deallocateClimateStructs(&climateOutput, &climateAverages);
}

/**
@brief Calculate the composition (land cover) representing a potential natural
vegetation based on climate relationships

The function returns relative abundance/land cover values that completely cover
the surface (i.e., they sum to 1) of a site specified by long-term climate
and/or fixed input values.

Some of the land cover/vegetation types, i.e., trees, annual grasses, and
bare-ground are not estimated from climate relationships; they are either set
to 0, or alternatively fixed at the value of the input argument(s).

The remaining vegetation types, i.e., shrubs, C3 grasses, C4 grasses, forbs,
and succulents, are estimated from climate relationships using equations
developed by Paruelo & Lauenroth 1996 @cite paruelo1996EA, or alternatively
fixed at the value of the input argument(s). If values for dailyC4vars are
provided, then equations developed by Teeri & Stowe 1976 @cite teeri1976O are
used to limit the occurrence of C4 grasses.

The relative abundance values of the the vegetation types that can be estimated
and are not fixed by inputs, are estimated in two steps: (i) as if they cover
the entire surface; (ii) scaled to the proportion of the surface that is not
fixed by inputs.

The equations developed by Paruelo & Lauenroth 1996 @cite paruelo1996EA are
based on sites with MAT from 2 C to 21.2 C and MAP from 117 to 1011 mm. If
warn_extrapolation is set to TRUE, then inputs are checked against supported
ranges, i.e., if MAT is below 1 C, then it is reset to 1 C with a warning. If
other inputs exceed their ranges, then a warning is issued and the code
proceeds.

`calcSiteClimate()` and `averageClimateAcrossYears()` can be used to calculate
climate variables required as inputs.`

@param[in] meanTemp_C Value containing the long-term average of yearly
    temperatures [C]
@param[in] PPT_cm Value containing the long-term average of yearly
    precipitation [cm]
@param[in] meanTempMon_C Array of size MAX_MONTHS containing long-term average
    monthly mean temperatures [C]
@param[in] PPTMon_cm Array of size MAX_MONTHS containing sum of monthly mean
    precipitation [cm]
@param[in] inputValues Array of size eight that contains values input by user
    for each component of cover.
    A value of SW_MISSING indicates the respective component's value will be
    estimated. If an element is not SW_MISSING, a value from 0-1 indicates
    the component cover is fixed and will not be estimated.
    The elements of compositions are:
        -# Succulents
        -# Forbs
        -# C3 Grasses
        -# C4 Grasses
        -# Grass Annuals
        -# Shrubs
        -# Trees
        -# Bare ground
@param[in] shrubLimit Shrub cover lower than shrubLimit selects the "grassland"
    equation to determine C3 grass cover; shrub cover larger than shrubLimit
    selects the "shrubland" equation
    (default value of 0.2; page 1213 of Paruelo & Lauenroth 1996).
@param[in] SumGrassesFraction Value holding sum of grasses, if not SW_MISSING,
    the sum of grasses is fixed and if a grass component is not fixed,
    it will be estimated relative to this value
@param[in] C4Variables Array of size three holding C4 variables after being
    averaged by `averageClimateAcrossYears()`. The elements are:
        -# July precipitation,
        -# mean temperature of dry quarter,
        -# mean minimum temperature of February
@param[in] fillEmptyWithBareGround Bool value specifying whether or not to fill
    gaps in values with bare ground
@param[in] inNorthHem Bool value specifying if the current site is in the
    northern hemisphere
@param[in] warnExtrapolation Bool value specifying whether or not to warn the
    user when extrapolation happens
@param[in] fixBareGround Bool value specifying if bare ground input value is
    fixed
@param[out] grassOutput Array of size three holding estimated grass values. The
    elements are:
    -# C3 grasses,
    -# C4 grasses,
    -# annual grasses
@param[out] RelAbundanceL0 Array of size eight holding all estimated values.
    The elements are:
        -# Succulents,
        -# Forbs,
        -# C3 grasses,
        -# C4 grasses,
        -# annual grasses,
        -# Shrubs,
        -# Trees,
        -# Bare ground
@param[out] RelAbundanceL1 Array of size five holding all estimated values
    aside from grasses (not including sum of grasses). The elements are:
        -# trees,
        -# shrubs
        -# sum of forbs and succulents
        -# overall sum of grasses
        -# bare ground
@param[out] LogInfo Holds information on warnings and errors

@note This function uses equations developed by
Paruelo & Lauenroth (1996) @cite paruelo1996EA and,
for C4 grasses, an equation by Teeri & Stowe (1976) @cite teeri1976O.
*/
void estimatePotNatVegComposition(
    double meanTemp_C,
    double PPT_cm,
    double meanTempMon_C[],
    const double PPTMon_cm[],
    double inputValues[],
    double shrubLimit,
    double SumGrassesFraction,
    double C4Variables[],
    Bool fillEmptyWithBareGround,
    Bool inNorthHem,
    Bool warnExtrapolation,
    Bool fixBareGround,
    double *grassOutput,
    double *RelAbundanceL0,
    double *RelAbundanceL1,
    LOG_INFO *LogInfo
) {

    const int nTypes = 8;
    int winterMonths[3], summerMonths[3];

    // Indices both single value and arrays
    int index, succIndex = 0, forbIndex = 1, C3Index = 2, C4Index = 3,
               grassAnn = 4, shrubIndex = 5, treeIndex = 6, bareGround = 7,
               grassEstimSize = 0, overallEstimSize = 0, julyMin = 0,
               degreeAbove65 = 1, frostFreeDays = 2, estimIndicesNotNA = 0,
               grassesEstim[3], overallEstim[nTypes], iFixed[nTypes],
               iFixedSize = 0;
    int isetIndices[3] = {grassAnn, treeIndex, bareGround};

    const char *txt_isetIndices[] = {"annual grasses", "trees", "bare ground"};

    // Totals of different areas of variables
    double totalSumGrasses = 0., inputSumGrasses = 0., tempDiffJanJul,
           summerMAP = 0., winterMAP = 0., C4Species = SW_MISSING, C3Grassland,
           C3Shrubland, estimGrassSum = 0, finalVegSum = 0., estimCoverSum = 0.,
           tempSumGrasses = 0., estimCover[nTypes], initialVegSum = 0.,
           tempSwapValue, fixedValuesSum = 0;

    Bool fixSumGrasses = (Bool) (!missing(SumGrassesFraction));
    Bool isGrassIndex = swFALSE, tempShrubBool;


    // Land cover/vegetation types that are not estimated
    // (trees, annual grasses, and bare-ground):
    // set to 0 if input is `SW_MISSING`
    for (index = 0; index < 3; index++) {
        if (missing(inputValues[isetIndices[index]])) {
            inputValues[isetIndices[index]] = 0.;

            LogError(
                LogInfo,
                LOGWARN,
                "No equation for requested cover type '%s': cover set to 0.\n",
                txt_isetIndices[index]
            );
        }
    }

    // Loop through inputValues and get the total
    for (index = 0; index < nTypes; index++) {
        if (!missing(inputValues[index])) {
            initialVegSum += inputValues[index];
        }
    }

    // Check if grasses are fixed
    if (fixSumGrasses) {
        // Set SumGrassesFraction
        // If SumGrassesFraction < 0, set to zero, otherwise keep at value
        cutZeroInf(SumGrassesFraction);
        // Get sum of input grass values and set to inputSumGrasses
        for (index = C3Index; index <= grassAnn; index++) {
            if (!missing(inputValues[index])) {
                inputSumGrasses += inputValues[index];
            }
        }

        // Get totalSumGrasses
        totalSumGrasses = SumGrassesFraction - inputSumGrasses;

        // Check if totalSumGrasses is less than zero
        if (totalSumGrasses < 0) {
            LogError(
                LogInfo,
                LOGERROR,
                "'estimate_PotNatVeg_composition': "
                "User defined grass values including C3, C4, and annuals "
                "sum to more than user defined total grass cover."
            );
            return; // Exit function prematurely due to error
        }
        // Find indices to estimate related to grasses
        // (i.e., C3, C4 and annual grasses)
        for (index = C3Index; index < grassAnn; index++) {
            if (missing(inputValues[index])) {
                grassesEstim[grassEstimSize] = index;
                grassEstimSize++;
            }
        }

        // Check if totalSumGrasses is greater than zero
        if (totalSumGrasses > 0) {

            // Check if there is only one grass index to be estimated
            if (grassEstimSize == 1) {

                // Set element to SumGrassesFraction - inputSumGrasses
                inputValues[grassesEstim[0]] =
                    SumGrassesFraction - inputSumGrasses;

                // Set totalSumGrasses to zero
                totalSumGrasses = 0.;
            }
        } else {
            // Otherwise, totalSumGrasses is zero or below
            for (index = 0; index < grassEstimSize; index++) {
                // Set all found ids to estimate to zero
                inputValues[grassesEstim[index]] = 0.;
            }
        }
    }

    // Initialize overallEstim and add fixed indices to `iFixed`
    for (index = 0; index < nTypes; index++) {
        if (!missing(inputValues[index])) {
            iFixed[iFixedSize] = index;
            iFixedSize++;
            estimCover[index] = inputValues[index];
            estimIndicesNotNA++;
        } else {
            overallEstim[overallEstimSize] = index;
            overallEstimSize++;
            estimCover[index] = 0.;
        }
    }

    uniqueIndices(
        isetIndices, iFixed, 3, iFixedSize, iFixed, &iFixedSize, LogInfo
    );
    if (LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

    // Set boolean value to true if grasses still need to be estimated
    if (!EQ(totalSumGrasses, 0.)) {
        initialVegSum += totalSumGrasses;
    }

    if (GT(initialVegSum, 1.)) {
        LogError(
            LogInfo,
            LOGERROR,
            "'estimate_PotNatVeg_composition': "
            "User defined relative abundance values sum to more than "
            "1 = full land cover."
        );
        return; // Exit function prematurely due to error
    }

    // Check if number of elements to estimate is less than or equal to 1
    // Or `initialVegSum` is 1 and we do not have to estimate any grasses
    if (overallEstimSize <= 1) {
        if (overallEstimSize == 0) {
            // Check if we want to fill gaps in data with bare ground
            if (fillEmptyWithBareGround) {
                // Set estimCover at index `bareGround` to 1 - (all values
                // execpt at index `bareGround`)
                inputValues[bareGround] =
                    1 - (initialVegSum - estimCover[bareGround]);
            } else if (initialVegSum < 1) {
                LogError(
                    LogInfo,
                    LOGERROR,
                    "'estimate_PotNatVeg_composition': "
                    "User defined relative abundance values are all fixed, "
                    "but their sum is smaller than 1 = full land cover."
                );
                return; // Exit function prematurely due to error
            }
        } else if (overallEstimSize == 1) {
            estimCover[overallEstim[0]] = 1 - initialVegSum;
        }
    } else {

        if (PPT_cm * 10 <= 1) {
            for (index = 0; index < nTypes - 1; index++) {
                estimCover[index] = 0.;
            }
            estimCover[bareGround] = 1.;
        } else {
            // Set months of winter and summer (northern/southern hemisphere)
            // and get their three month values in precipitation and temperature
            if (inNorthHem) {
                for (index = 0; index < 3; index++) {
                    winterMonths[index] = (index + 11) % MAX_MONTHS;
                    summerMonths[index] = (index + 5);
                    summerMAP += PPTMon_cm[summerMonths[index]];
                    winterMAP += PPTMon_cm[winterMonths[index]];
                }
            } else {
                for (index = 0; index < 3; index++) {
                    summerMonths[index] = (index + 11) % MAX_MONTHS;
                    winterMonths[index] = (index + 5);
                    summerMAP += PPTMon_cm[summerMonths[index]];
                    winterMAP += PPTMon_cm[winterMonths[index]];
                }
            }
            // Set summer and winter precipitations in mm
            summerMAP /= PPT_cm;
            winterMAP /= PPT_cm;

            // Get the difference between July and Janurary
            tempDiffJanJul = cutZeroInf(
                meanTempMon_C[summerMonths[1]] - meanTempMon_C[winterMonths[1]]
            );

            if (warnExtrapolation) {
                if (meanTemp_C < 1) {
                    LogError(
                        LogInfo,
                        LOGWARN,
                        "Equations used outside supported range"
                        "(2 - 21.2 C): MAT = %2f, C reset to 1C",
                        meanTemp_C
                    );

                    meanTemp_C = 1;
                }

                if (meanTemp_C > 21.2) {
                    LogError(
                        LogInfo,
                        LOGWARN,
                        "Equations used outside supported range"
                        "(2 - 21.2 C): MAT = %2f C",
                        meanTemp_C
                    );
                }

                if (PPT_cm * 10 < 117 || PPT_cm * 10 > 1011) {
                    LogError(
                        LogInfo,
                        LOGWARN,
                        "Equations used outside supported range"
                        "(117 - 1011 mm): MAP = %3f mm",
                        PPT_cm * 10
                    );
                }
            }

            // Paruelo & Lauenroth (1996): shrub climate-relationship:
            if (PPT_cm * 10 < 1) {
                estimCover[shrubIndex] = 0.;
            } else {
                estimCover[shrubIndex] = cutZeroInf(
                    1.7105 - (.2918 * log(PPT_cm * 10)) + (1.5451 * winterMAP)
                );
            }

            // Paruelo & Lauenroth (1996): C4-grass climate-relationship:
            if (meanTemp_C <= 0) {
                estimCover[C4Index] = 0;
            } else {
                estimCover[C4Index] = cutZeroInf(
                    -0.9837 + (.000594 * (PPT_cm * 10)) + (1.3528 * summerMAP) +
                    (.2710 * log(meanTemp_C))
                );

                // This equations give percent species/vegetation -> use to
                // limit Paruelo's C4 equation, i.e., where no C4 species => C4
                // abundance == 0
                if (!missing(C4Variables[julyMin])) {
                    if (C4Variables[frostFreeDays] <= 0) {
                        C4Species = 0;
                    } else {
                        C4Species = cutZeroInf(
                            ((1.6 * (C4Variables[julyMin] * 9 / 5 + 32)) +
                             (.0086 * (C4Variables[degreeAbove65] * 9 / 5)) -
                             (8.98 * log(C4Variables[frostFreeDays])) - 22.44) /
                            100
                        );
                    }
                    if (EQ(C4Species, 0.)) {
                        estimCover[C4Index] = 0;
                    }
                }
            }

            // Paruelo & Lauenroth (1996): C3-grass climate-relationship:
            if (winterMAP <= 0) {
                C3Grassland = C3Shrubland = 0;
            } else {
                C3Grassland = cutZeroInf(
                    1.1905 - .02909 * meanTemp_C + .1781 * log(winterMAP) -
                    .2383
                );

                C3Shrubland = cutZeroInf(
                    1.1905 - .02909 * meanTemp_C + .1781 * log(winterMAP) -
                    .4766
                );
            }

            tempShrubBool = (Bool) (!missing(estimCover[shrubIndex]) &&
                                    estimCover[shrubIndex] >= shrubLimit);

            if (tempShrubBool) {
                estimCover[C3Index] = C3Shrubland;
            } else {
                estimCover[C3Index] = C3Grassland;
            }

            // Paruelo & Lauenroth (1996): forb climate-relationship:
            if (PPT_cm * 10 < 1 || meanTemp_C <= 0) {
                estimCover[forbIndex] = 0.;
            } else {
                estimCover[forbIndex] = cutZeroInf(
                    -.2035 + (.07975 * log(PPT_cm * 10)) -
                    (.0623 * log(meanTemp_C))
                );
            }

            // Paruelo & Lauenroth (1996): succulent climate-relationship:
            if (tempDiffJanJul <= 0 || winterMAP <= 0) {
                estimCover[succIndex] = 0.;
            } else {
                estimCover[succIndex] = cutZeroInf(
                    -1 + ((1.20246 * pow(tempDiffJanJul, -.0689)) *
                          (pow(winterMAP, -.0322)))
                );
            }

            // Check if fillEmptyWithBareGround is FALSE and there's less than
            // or equal to one indices to estimate
            if (!fillEmptyWithBareGround && estimIndicesNotNA <= 1) {
                if (PPT_cm * 10 < 600) {
                    estimCover[shrubIndex] += 1.;
                }
                if (meanTemp_C < 10) {
                    estimCover[C3Index] += 1.;
                }
                if (meanTemp_C >= 10 && PPT_cm * 10 > 600) {
                    estimCover[C4Index] += 1.;
                }
            }

            if (fixSumGrasses && GT(totalSumGrasses, 0.)) {
                for (index = 0; index < grassEstimSize; index++) {
                    estimGrassSum += estimCover[grassesEstim[index]];
                }

                // If estimGrassSum is 0, make it 1. to prevent dividing by zero
                estimGrassSum = (EQ(estimGrassSum, 0.)) ? 1. : estimGrassSum;

                if (GT(estimGrassSum, 0.)) {
                    for (index = 0; index < grassEstimSize; index++) {
                        estimCover[grassesEstim[index]] *=
                            (totalSumGrasses / estimGrassSum);
                    }
                } else if (grassEstimSize > 0) {
                    for (index = 0; index < grassEstimSize; index++) {
                        estimCover[grassesEstim[index]] =
                            (totalSumGrasses / grassEstimSize);
                    }

                    LogError(
                        LogInfo,
                        LOGWARN,
                        "'estimate_PotNatVeg_composition': "
                        "Total grass cover set, but no grass cover estimated; "
                        "requested cover evenly divided among grass types."
                    );
                }
            }

            if (fixSumGrasses) {
                // Add grasses to `iFixed` array
                uniqueIndices(
                    iFixed,
                    grassesEstim,
                    iFixedSize,
                    grassEstimSize,
                    iFixed,
                    &iFixedSize,
                    LogInfo
                );

                if (LogInfo->stopRun) {
                    return; // Exit function prematurely due to error
                }

                // Remove them from the `estimIndices` array
                for (index = 0; index < overallEstimSize; index++) {
                    do {
                        isGrassIndex = (Bool) (overallEstim[index] == C3Index ||
                                               overallEstim[index] == C4Index ||
                                               overallEstim[index] == grassAnn);

                        if (isGrassIndex && index + 1 != overallEstimSize) {
                            tempSwapValue = overallEstim[overallEstimSize - 1];
                            overallEstim[overallEstimSize - 1] =
                                overallEstim[index];
                            overallEstim[index] = tempSwapValue;
                            overallEstimSize--;
                        }
                    } while (index != overallEstimSize - 1 && isGrassIndex);
                }

                isGrassIndex = (Bool) (overallEstim[index - 1] == C3Index ||
                                       overallEstim[index - 1] == C4Index ||
                                       overallEstim[index - 1] == grassAnn);

                if (isGrassIndex) {
                    overallEstimSize--;
                }
            }

            // Get final estimated vegetation sum
            for (index = 0; index < nTypes; index++) {
                if (missing(inputValues[index])) {
                    finalVegSum += estimCover[index];
                } else {
                    finalVegSum += inputValues[index];
                    fixedValuesSum += inputValues[index];
                }
            }

            // Include fixed grass sum if not missing
            if (fixSumGrasses && grassEstimSize > 0) {
                fixedValuesSum += totalSumGrasses;
            }

            // Check if the final estimated vegetation sum is equal to one
            if (!EQ(finalVegSum, 1.)) {
                for (index = 0; index < overallEstimSize; index++) {
                    estimCoverSum += estimCover[overallEstim[index]];
                }
                if (estimCoverSum > 0) {
                    for (index = 0; index < overallEstimSize; index++) {
                        estimCover[overallEstim[index]] *=
                            (1 - fixedValuesSum) / estimCoverSum;
                    }
                } else {
                    if (fillEmptyWithBareGround && !fixBareGround) {
                        inputValues[bareGround] = 1.;
                        for (index = 0; index < nTypes - 1; index++) {
                            inputValues[bareGround] -= estimCover[index];
                        }
                    } else {
                        LogError(
                            LogInfo,
                            LOGERROR,
                            "'estimate_PotNatVeg_composition': "
                            "The estimated vegetation cover values are 0, "
                            "the user fixed relative abundance values sum to "
                            "less than 1, "
                            "and bare-ground is fixed. "
                            "Thus, the function cannot compute "
                            "complete land cover composition."
                        );
                        return; // Exit function prematurely due to error
                    }
                }
            }
        }
    }

    // Fill in all output arrays (grassOutput, RelAbundanceL0, RelAbundanceL1)
    for (index = 0; index < nTypes; index++) {
        // Check if inputValues at index is missing or if current index is
        // bare ground if bare ground is fixed
        if (!missing(inputValues[index])) {

            // Reset estimated value to fixed value that was input
            estimCover[index] = inputValues[index];
        }

        RelAbundanceL0[index] = estimCover[index];
    }

    grassOutput[0] = (missing(inputValues[C3Index])) ? estimCover[C3Index] :
                                                       inputValues[C3Index];
    grassOutput[1] = (missing(inputValues[C4Index])) ? estimCover[C4Index] :
                                                       inputValues[C4Index];
    grassOutput[2] = (missing(inputValues[grassAnn])) ? estimCover[grassAnn] :
                                                        inputValues[grassAnn];

    tempSumGrasses += grassOutput[0];
    tempSumGrasses += grassOutput[1];
    tempSumGrasses += grassOutput[2];

    if (tempSumGrasses > 0) {
        for (index = 0; index < 3; index++) {
            grassOutput[index] /= (fixSumGrasses && overallEstimSize <= 1) ?
                                      SumGrassesFraction :
                                      tempSumGrasses;
        }
    }

    RelAbundanceL1[0] = estimCover[treeIndex];
    RelAbundanceL1[1] = estimCover[shrubIndex];
    RelAbundanceL1[2] = estimCover[forbIndex] + estimCover[succIndex];

    if (fixSumGrasses && grassEstimSize > 0) {
        RelAbundanceL1[3] = SumGrassesFraction;
    } else {
        RelAbundanceL1[3] = tempSumGrasses;
    }

    RelAbundanceL1[4] = inputValues[bareGround];
}

/**
@brief Helper function to `estimatePotNatVegComposition()` that doesn't allow a
value to go below zero

@param testValue A value of type double holding a value that is to be tested to
    see if it is below zero

@return A value that is either above or equal to zero
*/
double cutZeroInf(double testValue) {
    return (LT(testValue, 0.)) ? 0. : testValue;
}

/**
@brief Helper function to `estimatePotNatVegComposition()` that gets unique
indices from two input arrays

@param[in] arrayOne First array to search through to get indices inside of it
@param[in] arrayTwo Second array to search through to get indices inside of it
@param[in] arrayOneSize Size of first array
@param[in] arrayTwoSize Size of second array
@param[out] finalIndexArray Array of size finalIndexArraySize that holds all
    unique indices from both arrays
@param[in,out] finalIndexArraySize Value holding the size of finalIndexArray
    both before and after the function is run
@param[out] LogInfo Holds information on warnings and errors
*/
void uniqueIndices(
    const int arrayOne[],
    const int arrayTwo[],
    int arrayOneSize,
    int arrayTwoSize,
    int *finalIndexArray,
    int *finalIndexArraySize,
    LOG_INFO *LogInfo
) {

    int index, finalArrayIndex = 0, nTypes = 8,
               tempSize = arrayOneSize + arrayTwoSize + finalArrayIndex,
               tempIndex = 0;
    int *tempArray, *tempArraySeen;

    tempArray =
        (int *) Mem_Malloc(sizeof(int) * tempSize, "uniqueIndices()", LogInfo);
    if (LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

    tempArraySeen =
        (int *) Mem_Malloc(sizeof(int) * nTypes, "uniqueIndices()", LogInfo);
    if (LogInfo->stopRun) {
        free(tempArray);
        return; // Exit function prematurely due to error
    }

    memset(tempArray, 0, sizeof(int) * tempSize);
    memset(tempArraySeen, 0, sizeof(int) * nTypes);

    for (index = 0; index < tempSize; index++) {
        // Initalize the "seen" version of tempArray
        if (index < nTypes) {
            tempArraySeen[index] = 0;
        }

        // Add all elements of of finalArrayIndex, arrayOne and arrayTWo
        // into "tempArray"
        if (index < finalArrayIndex) {
            tempArray[tempIndex] = finalIndexArray[index];
            tempIndex++;
        }
        if (index < arrayOneSize) {
            tempArray[tempIndex] = arrayOne[index];
            tempIndex++;
        }
        if (index < arrayTwoSize) {
            tempArray[tempIndex] = arrayTwo[index];
            tempIndex++;
        }
    }

    // Loop through `tempArray` and search for unique indices
    for (index = 0; index < tempSize; index++) {
        // Check if we have found the current index in question
        if (tempArraySeen[tempArray[index]] == 0) {
            finalIndexArray[finalArrayIndex] = tempArray[index];
            finalArrayIndex++;
            tempArraySeen[tempArray[index]] = tempArray[index];
        }
    }

    *finalIndexArraySize = finalArrayIndex;

    free(tempArray);
    free(tempArraySeen);
}
