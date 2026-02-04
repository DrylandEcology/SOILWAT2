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
#include "include/SW_VegProd.h" // for BIO_INDEX
#include "include/filefuncs.h"  // for LogError, CloseFile, GetALine
#include "include/generic.h"    // for LOGERROR, Bool, LOGWARN, GT
#include "include/myMemory.h"   // for Mem_Calloc, Mem_Malloc
#include "include/SW_datastructs.h" // for SW_VEGPROD_INPUTS, LOG_INFO, SW_VEGPROD_INPUTS...
#include "include/SW_Defines.h" // for ForEachVegType, NVEGTYPES, SW_SHRUB
#include "include/SW_Files.h"   // for eVegProd
#include "include/SW_Weather.h" // for deallocateClimateStructs, alloca...
#include "include/Times.h"      // for interpolate_monthlyValues, Jan, Dec
#include <math.h>               // for log, pow
#include <stdio.h>              // for sscanf, NULL, FILE
#include <stdlib.h>             // for free
#include <string.h>             // for memset


/* =================================================== */
/*                  Global Variables                   */
/* --------------------------------------------------- */

// key2veg must be in the same order as the indices to vegetation types defined
// in SW_Defines.h
const char *const key2veg[NVEGTYPES] = {
    "treeNL", "treeBL", "shrub", "forbs", "grassC3", "grassC4"
};

/* =================================================== */
/*             Local Function Definitions              */
/* --------------------------------------------------- */

/**
@brief Allocate co2_multipliers for the current simulation run

@param[in] veg Array of size NVEGTYPES of type VegType describing
all NVEGTYPES vegetation types through simulation-specific inputs;
return struct with allocated co2_multiplier arrays
Return arrays with allocated memory
@param[in] n_years Number of years in simulation
@param[out] LogInfo Holds information on warnings and errors
*/
static void alloc_co2(VegTypeSim veg[], size_t n_years, LOG_INFO *LogInfo) {
    int vegIndex;
    int co2Arr;
    const int nco2Arr = 2;

    ForEachVegType(vegIndex) {
        for (co2Arr = 0; co2Arr < nco2Arr; co2Arr++) {
            veg[vegIndex].co2_multipliers[co2Arr] = (double *) Mem_Malloc(
                sizeof(double) * n_years, "alloc_co2", LogInfo
            );
            if (LogInfo->stopRun) {
                return;
            }
        }
    }
}

/**
@brief Moving window of long-term mean temperature

See also calc_veg_predictor_vals().

@param[in] yearIndex Index value specifying which place the year is in the
    simulation process (i.e., [0, n years) )
@param[in] nYearsDynamicLong Number of years over which long-term
    predictors are summarized
@param[in] SW_VegProdSim Struct of type SW_VEGPROD_SIM that holds information
    used and/or modified mainly during simulation runs
@param[out] annTempLongAvg Calculated long-term mean temperature across the
    moving window defined by \p nYearsDynamicLong
*/
static void calc_annTempLongAvg(
    TimeInt yearIndex,
    TimeInt nYearsDynamicLong,
    SW_VEGPROD_SIM *SW_VegProdSim,
    double *annTempLongAvg
) {
    IntU termIndex = SW_VegProdSim->longIndex;

    if (yearIndex == 0) {
        *annTempLongAvg = 0.;
    }

    /* Check if we have enough values to make a full average for the
       term in which we are taking the average of */
    if (yearIndex + 1 > nYearsDynamicLong) {
        /* Calculate the new average by converting the average into a sum,
            removing the oldest value, adding the newest value, and
            converting the sum into an average
            This method simplies how we calculate the moving window
            average to be more constant in computation time */
        *annTempLongAvg *= nYearsDynamicLong;
        *annTempLongAvg -= SW_VegProdSim->annTemp[termIndex];
        *annTempLongAvg += SW_VegProdSim->annTemp[yearIndex];
        *annTempLongAvg /= nYearsDynamicLong;

        SW_VegProdSim->longIndex++;

    } else {
        /* Since we do not have enough years to compute the whole average,
            we keep a running average until we have enough years
            Do this by converting the running average into a sum
            (number of years currently within the sum), add the new
            value, and retake the average with the new number
            of years in the sum (yearIndex + 1) */
        *annTempLongAvg *= (yearIndex);
        *annTempLongAvg += SW_VegProdSim->annTemp[yearIndex];
        *annTempLongAvg /= (yearIndex + 1);
    }
}

/* =================================================== */
/*             Global Function Definitions             */
/* --------------------------------------------------- */

/**
@brief Allocate dynamic arrays to hold up to n years worth of yearly history
of values

@param[in] n_years Number of years in simulation
@param[in] annTempOnly Specifies if this function is to only allocate
annual temperature arrays
@param[out] SW_VegProdSim Struct of type SW_VEGPROD_SIM that holds information
used and/or modified mainly during simulation runs; dynamic arrays will be
updated within this struct
@param[out] LogInfo Holds information on warnings and errors
*/
void alloc_nyear_arrays(
    TimeInt n_years,
    Bool annTempOnly,
    SW_VEGPROD_SIM *SW_VegProdSim,
    LOG_INFO *LogInfo
) {
    int index;
    const int numArrays = (annTempOnly) ? 1 : 11;
    double **allocArray[] = {
        &SW_VegProdSim->annTemp,
        &SW_VegProdSim->annTempPrecipCorr,
        &SW_VegProdSim->annIsotherm,
        &SW_VegProdSim->annPrecip,
        &SW_VegProdSim->annWaterDef,
        &SW_VegProdSim->annSeasonPrecip,
        &SW_VegProdSim->annPrecipDriestMon,
        &SW_VegProdSim->annWetDegDays,
        &SW_VegProdSim->annTempWarmestMon,
        &SW_VegProdSim->annTempColdestMon,
        &SW_VegProdSim->annPrecipWettestMon
    };

    for (index = 0; index < numArrays; index++) {
        *(allocArray[index]) = (double *) Mem_Malloc(
            sizeof(double) * n_years, "alloc_nyear_arrays()", LogInfo
        );
        if (LogInfo->stopRun) {
            return;
        }

        memset(*(allocArray[index]), 0, n_years * sizeof(double));
    }
}

/**
@brief Given an unknown variable with max size of MAX_LAYERS, either calculate
a weighted % of contents in the first 3cm (or first layer, whichever is
deepest), or a weighted % across the whole soil profile (i.e., through
all layers)

@param[in] vals A list of size MAX_LAYERS containing values to calculate
the weighted % through the first 3cm (or first layer), or throughout the
soil profile (up to # of layers created)
@param[in] depths Depths of soil layers (cm)
@param[in] widths The width of the layers (cm).
@param[in] n_layers Number of layers of soil within the simulation run
@param[in] first3cm A flag specifying if the function should calculate
the first 3cm (or first layer) of values or the entire soil layer

@return Resulting weighted % or first layer value
*/
double calc_perc_var_in_soil_profile(
    const double vals[],
    double depths[],
    const double widths[],
    LyrIndex n_layers,
    Bool first3cm
) {
    LyrIndex soilLyr;
    double widthWeight;
    double totDepth = (first3cm) ? 3.0 : depths[n_layers - 1];
    double result = 0.;

    if (first3cm && GE(depths[0], 3.0)) {
        // Set result to be the first layer value
        result = vals[0];
    } else {
        ForEachSoilLayer(soilLyr, n_layers) {
            widthWeight = widths[soilLyr];

            if (first3cm) {
                // weight = layer thickness / 3cm
                // layer thickness = if (bottom of layer <= 3 cm) then
                // (bottom - top depth of layer) else (3 cm - top depth of
                // layer)
                if (LE(depths[soilLyr], 3.0)) {
                    widthWeight = widths[soilLyr];
                } else {
                    widthWeight =
                        3.0 - ((soilLyr > 0) ? depths[soilLyr - 1] : 0);
                }
            }
            widthWeight /= totDepth;

            result += vals[soilLyr] * widthWeight;

            if (first3cm && GE(depths[soilLyr], 3.0)) {
                break;
            }
        }
    }

    return result;
}

/**
@brief Calculate total soil available water holding capacity (awhc)

@param[in] swcBulk_fieldcap Soil water content (SWC) corresponding
    to field capacity (SWP = -0.033 MPa) [cm]
@param[in] swcBulk_wiltpt SWC corresponding to wilting point
    (SWP = -1.5 MPa) [cm]
@param[in] n_layers Number of layers of soil within the simulation run

@return Resulting value of total soil available water holding capacity
*/
double calc_awhc(
    double swcBulk_fieldcap[], double swcBulk_wiltpt[], LyrIndex n_layers
) {
    double awhc = 0.;
    LyrIndex lyr;

    ForEachSoilLayer(lyr, n_layers) {
        awhc += fmax(0., swcBulk_fieldcap[lyr] - swcBulk_wiltpt[lyr]);
    }

    return awhc;
}

/**
@brief Wrapper function to calculate constant that will be used for
dynamic vegetation calculations for the simulation, these calculations
include

    - % values for either the first 3cm (or first layer) (for clay and organic
      matter) or the whole soil profile (for sand and gravel)
    - Total soil available water holding capacity
    - Explicitly determine the maximum depth of soils for the current site

@param[in,out] SW_SoilSim Struct of type SW_SOIL_SIM holding constant soil
content information that will be used during simulations
@param[in] SW_SoilRunIn Struct of type SW_SOIL_RUN_INPUTS describing
    the simulated site's input values
@param[in] SW_SiteSim Struct of type SW_SITE_SIM describing the simulated site's
    simulation values
@param[in] n_layers Number of layers of soil within the simulation run
*/
void calc_const_dynamic_veg_info(
    SW_SOIL_SIM *SW_SoilSim,
    SW_SOIL_RUN_INPUTS *SW_SoilRunIn,
    SW_SITE_SIM *SW_SiteSim,
    LyrIndex n_layers
) {
    int index;
    const int numValArrays = 4;
    double *vals[] = {
        SW_SoilRunIn->fractionWeightMatric_sand,
        SW_SoilRunIn->fractionVolBulk_gravel,
        SW_SoilRunIn->fractionWeightMatric_clay,
        SW_SoilRunIn->fractionWeight_om
    };

    double *resDest[] = {
        &SW_SoilSim->percSand,
        &SW_SoilSim->percCoarseFrag,
        &SW_SoilSim->surfaceClay,
        &SW_SoilSim->surfaceOM
    };

    Bool first3cmFlags[] = {swFALSE, swFALSE, swTRUE, swTRUE};

    for (index = 0; index < numValArrays; index++) {
        *(resDest[index]) = calc_perc_var_in_soil_profile(
            vals[index],
            SW_SoilRunIn->depths,
            SW_SoilRunIn->width,
            n_layers,
            first3cmFlags[index]
        );
    }

    SW_SoilSim->totAWHC = calc_awhc(
        SW_SiteSim->swcBulk_fieldcap, SW_SiteSim->swcBulk_wiltpt, n_layers
    );

    SW_SoilSim->soilDepth = SW_SoilRunIn->depths[n_layers - 1];
}

/**
@brief Calculate and store yearly values within SW_VEGPROD_SIM's dynamic
arrays that hold a yearly history of annual climate information

Note: \p SW_ModelSim and \p SW_WeathHist must represent the same year.
In particular, SW_MDL_new_year() must have been called to correctly set up
\p SW_ModelSim for the year.

@param[in] SW_WeathHist Array containing all historical data of a site
@param[in] SW_ModelSim Struct of type SW_MODEL_SIM holding basic
intermediate time information about the simulation run
@param[in] yearIndex Index value specifying which place the year is in the
simulation process including spinup (i.e., [0, n years) )
@param[in] annTempOnly Specifies if this function is to only allocate
annual temperature arrays
@param[out] SW_VegProdSim Struct of type SW_VEGPROD_SIM that holds information
used and/or modified mainly during simulation runs; dynamic arrays will have a
new value for this year
*/
void calc_yearly_hist_vals(
    SW_WEATHER_HIST *SW_WeathHist,
    SW_MODEL_SIM *SW_ModelSim,
    TimeInt yearIndex,
    Bool annTempOnly,
    SW_VEGPROD_SIM *SW_VegProdSim
) {
    double meanTemp[MAX_MONTHS] = {0.};
    double meanTempAnn = 0.;
    double maxMonTemp[MAX_MONTHS] = {0.};
    double minMonTemp[MAX_MONTHS] = {0.};
    double totPrecip[MAX_MONTHS] = {0.};
    double isoThermSum[MAX_MONTHS] = {0.};

    double warmMonTemp = 0.;
    double dryMonPrecip = 0.;
    double wetMonPrecip = 0.;
    double watDef = 0.;
    double wetDD = 0.;
    double isothermCalc = 0.;
    double precipSD = 0.;
    double coldestMonTemp = 0.;

    double monPrecipAvg = 0.;

    TimeInt doy;
    TimeInt mon = 0;
    TimeInt nDaysYr = SW_ModelSim->cum_monthdays[MAX_MONTHS - 1];

    for (doy = 0; doy < SW_ModelSim->lastdoy; doy++) {
        // Check if this day is a new month
        if (doy == SW_ModelSim->cum_monthdays[mon]) {
            // Increment monthly index to move to the next month
            mon++;
        }

        // Add to the sum of mean temperature and precipitation of
        // current month, convert precip from cm -> mm
        meanTemp[mon] += SW_WeathHist->temp_avg[doy];

        if (!annTempOnly) {
            totPrecip[mon] += SW_WeathHist->ppt[doy] * 10;

            // Add to annual sum of precipitation
            SW_VegProdSim->annPrecip[yearIndex] += SW_WeathHist->ppt[doy] * 10;

            // Sum maximum and minimum temperature of each month
            // to find the hottest and coldest monthly temperature later
            maxMonTemp[mon] += SW_WeathHist->temp_max[doy];
            minMonTemp[mon] += SW_WeathHist->temp_min[doy];
        }
    }

    // If we only want to calculate annual mean daily temperature,
    // so do here and return from the function
    if (annTempOnly) {
        for (mon = 0; mon < MAX_MONTHS; mon++) {
            meanTempAnn += meanTemp[mon];
        }

        // Set annual mean temperature (corrected for number of days by month)
        SW_VegProdSim->annTemp[yearIndex] = meanTempAnn / nDaysYr;
        return;
    }

    // Loop through all months
    for (mon = 0; mon < MAX_MONTHS; mon++) {
        meanTempAnn += meanTemp[mon];

        maxMonTemp[mon] /= SW_ModelSim->days_in_month[mon];
        minMonTemp[mon] /= SW_ModelSim->days_in_month[mon];
        meanTemp[mon] /= SW_ModelSim->days_in_month[mon];

        // Find/store the maximum temperature month
        if (GT(maxMonTemp[mon], warmMonTemp) || mon == 0) {
            SW_VegProdSim->annTempWarmestMon[yearIndex] = maxMonTemp[mon];
            warmMonTemp = maxMonTemp[mon];
        }

        // Find/store the minimum temperature month
        if (LT(minMonTemp[mon], coldestMonTemp) || mon == 0) {
            SW_VegProdSim->annTempColdestMon[yearIndex] = minMonTemp[mon];
            coldestMonTemp = minMonTemp[mon];
        }

        // Find/store driest (lowest precip) month
        if (LT(totPrecip[mon], dryMonPrecip) || mon == 0) {
            SW_VegProdSim->annPrecipDriestMon[yearIndex] = totPrecip[mon];
            dryMonPrecip = totPrecip[mon];
        }

        // Find/store precip of the wettest (highest precip) month
        if (GT(totPrecip[mon], wetMonPrecip) || mon == 0) {
            SW_VegProdSim->annPrecipWettestMon[yearIndex] = totPrecip[mon];
            wetMonPrecip = totPrecip[mon];
        }

        // Sum to get annual water deficit and monthly wet-degree days
        watDef = (2 * meanTemp[mon]) - totPrecip[mon];
        wetDD = (30 * meanTemp[mon]) - totPrecip[mon];
        SW_VegProdSim->annWaterDef[yearIndex] +=
            GT(meanTemp[mon] * 2, totPrecip[mon]) ? watDef : 0.;
        SW_VegProdSim->annWetDegDays[yearIndex] +=
            LT(meanTemp[mon] * 2, totPrecip[mon]) ? wetDD : 0.;

        // Set monthly isothermality calculation
        isoThermSum[mon] = maxMonTemp[mon] - minMonTemp[mon];
    }

    // Set annual mean temperature (corrected for number of days by month)
    SW_VegProdSim->annTemp[yearIndex] = meanTempAnn / nDaysYr;

    // Set isothermality
    // mean for every month[((max temp - min temp) values)] /
    // (max temp of hottest month - min temp of coldest month)
    isothermCalc = mean(isoThermSum, MAX_MONTHS);
    isothermCalc /=
        (SW_VegProdSim->annTempWarmestMon[yearIndex] -
         SW_VegProdSim->annTempColdestMon[yearIndex]);
    SW_VegProdSim->annIsotherm[yearIndex] = isothermCalc * 100;

    // Set precip seasonality - coefficient of variation
    precipSD = standardDeviation(totPrecip, MAX_MONTHS);
    monPrecipAvg = mean(totPrecip, MAX_MONTHS);
    SW_VegProdSim->annSeasonPrecip[yearIndex] = precipSD / monPrecipAvg;

    // Set the temperature-precipitation correlation
    SW_VegProdSim->annTempPrecipCorr[yearIndex] =
        correlation_coefficient(meanTemp, totPrecip, MAX_MONTHS);
}

/**
@brief Using values calculated from `calc_yearly_hist_vals()`,
calculate the predictor values for dynamic vegetation calculations

@param[in] yearIndex Index value specifying which place the year is in the
simulation process (i.e., [0, n years) )
@param[in] nYearsDynamicShort Number of years over which short-term vegetation
predictors are summarized (as anomaly to long-term predictors)
@param[in] nYearsDynamicLong Number of years over which long-term vegetation
predictors are summarized
@param[out] SW_VegProdSim Struct of type SW_VEGPROD_SIM that holds information
used and/or modified mainly during simulation runs; averages and anomalies
will have updated values for this year
*/
void calc_veg_predictor_vals(
    TimeInt yearIndex,
    TimeInt nYearsDynamicShort,
    TimeInt nYearsDynamicLong,
    SW_VEGPROD_SIM *SW_VegProdSim
) {
    // Initialize variables
    TimeInt termLength;
    int var;
    IntU termIndex;
    int valIndex;
    size_t longTermIndex;
    size_t shortTermIndex;

    double longTermVal;
    double shortTermVal;

    const int numAvgs = 18;
    const int numAnom = 3;
    const int numRateAnom = 5;
    const int shortAvgIndex = 11;

    double *termAvgs[] = {
        &SW_VegProdSim->annTempLongAvg,
        &SW_VegProdSim->annTempPrecipLongAvg,
        &SW_VegProdSim->annIsothermLongAvg,
        &SW_VegProdSim->annWaterDefLongAvg,
        &SW_VegProdSim->annSeasonPrecipLongAvg,
        &SW_VegProdSim->annPrecipDriestMonLongAvg,
        &SW_VegProdSim->annWetDegDaysLongAvg,
        &SW_VegProdSim->annTempWarmestMonLongAvg,
        &SW_VegProdSim->annTempColdestMonLongAvg,
        &SW_VegProdSim->annPrecipWettestMonLongAvg,
        &SW_VegProdSim->annPrecipLongAvg,

        &SW_VegProdSim->annIsothermShortAvg,
        &SW_VegProdSim->annTempPrecipShortAvg,
        &SW_VegProdSim->annSeasonPrecipShortAvg,
        &SW_VegProdSim->annPrecipShortAvg,
        &SW_VegProdSim->annWetDegDaysShortAvg,
        &SW_VegProdSim->annWaterDefShortAvg,
        &SW_VegProdSim->annPrecipDriestMonShortAvg
    };

    double *termHist[] = {
        SW_VegProdSim->annTemp,
        SW_VegProdSim->annTempPrecipCorr,
        SW_VegProdSim->annIsotherm,
        SW_VegProdSim->annWaterDef,
        SW_VegProdSim->annSeasonPrecip,
        SW_VegProdSim->annPrecipDriestMon,
        SW_VegProdSim->annWetDegDays,
        SW_VegProdSim->annTempWarmestMon,
        SW_VegProdSim->annTempColdestMon,
        SW_VegProdSim->annPrecipWettestMon,
        SW_VegProdSim->annPrecip
    };

    // Index the value arrays short-term averages should use
    // to simplify this function to use one array to house all averages
    int shortValIndex[] = {2, 1, 4, 10, 6, 3, 5};

    double *anomVals[] = {
        &SW_VegProdSim->anomIsotherm,
        &SW_VegProdSim->anomTempPrecipCorr,
        &SW_VegProdSim->anomWaterDef
    };

    double *rateAnomVals[] = {
        &SW_VegProdSim->rateAnomSeasonPrecip,
        &SW_VegProdSim->rateAnomPrecip,
        &SW_VegProdSim->rateAnomWetDegDays,
        &SW_VegProdSim->rateAnomWaterDef,
        &SW_VegProdSim->rateAnomPrecipDriestMon
    };

    double *anomCalcVals[] = {// Isothermality anomaly vals
                              &SW_VegProdSim->annIsothermLongAvg,
                              &SW_VegProdSim->annIsothermShortAvg,

                              // Precip-temp anomaly vals
                              &SW_VegProdSim->annTempPrecipLongAvg,
                              &SW_VegProdSim->annTempPrecipShortAvg,

                              // Water deficit anomaly vals
                              &SW_VegProdSim->annWaterDefLongAvg,
                              &SW_VegProdSim->annWaterDefShortAvg
    };

    double *rateAnomCalcVals[] = {
        // Seasonality precip rate of anomaly vals
        &SW_VegProdSim->annSeasonPrecipLongAvg,
        &SW_VegProdSim->annSeasonPrecipShortAvg,

        // Precip rate of anomaly vals
        &SW_VegProdSim->annPrecipLongAvg,
        &SW_VegProdSim->annPrecipShortAvg,

        // Wet-degree days rate of anomaly vals
        &SW_VegProdSim->annWetDegDaysLongAvg,
        &SW_VegProdSim->annWetDegDaysShortAvg,

        // Water deficit rate of anomaly vals
        &SW_VegProdSim->annWaterDefLongAvg,
        &SW_VegProdSim->annWaterDefShortAvg,

        // Precip of the driest month rate of anomaly vals
        &SW_VegProdSim->annPrecipDriestMonLongAvg,
        &SW_VegProdSim->annPrecipDriestMonShortAvg
    };

    if (yearIndex == 0) {
        for (var = 0; var < numAvgs; var++) {
            *termAvgs[var] = 0.;
        }
    }

    for (var = 0; var < numAvgs; var++) {
        if (var < shortAvgIndex) {
            termLength = nYearsDynamicLong;
            termIndex = SW_VegProdSim->longIndex;
            valIndex = var;
        } else {
            termLength = nYearsDynamicShort;
            termIndex = SW_VegProdSim->shortIndex;
            valIndex = shortValIndex[var - shortAvgIndex];
        }

        /* Check if we have enough values to make a full average for the
           term in which we are taking the average of */
        if (yearIndex + 1 > termLength) {
            /*
                Calculate the new average by converting the average into a sum,
                removing the oldest value, adding the newest value, and
                converting the sum into an average
                This method simplies how we calculate the moving window
                average to be more constant in computation time
            */
            *termAvgs[var] *= termLength;
            *termAvgs[var] -= termHist[valIndex][termIndex];
            *termAvgs[var] += termHist[valIndex][yearIndex];
            *termAvgs[var] /= termLength;
        } else {
            /*
                Since we do not have enough years to compute the whole average,
                we keep a running average until we have enough years
                Do this by converting the running average into a sum
                (number of years currently within the sum), add the new
                value, and retake the average with the new number
                of years in the sum (yearIndex + 1)
            */
            *termAvgs[var] *= (yearIndex);
            *termAvgs[var] += termHist[valIndex][yearIndex];
            *termAvgs[var] /= (yearIndex + 1);
        }
    }

    // Calculate anomaly values
    for (var = 0; var < numAnom; var++) {
        longTermIndex = (size_t) (var) * 2;
        shortTermIndex = (size_t) (var) * 2 + 1;

        longTermVal = *(anomCalcVals[longTermIndex]);
        shortTermVal = *(anomCalcVals[shortTermIndex]);

        *anomVals[var] = longTermVal - shortTermVal;
    }

    // Calculate rate of anomaly values
    for (var = 0; var < numRateAnom; var++) {
        longTermIndex = (size_t) (var) * 2;
        shortTermIndex = (size_t) (var) * 2 + 1;

        longTermVal = *rateAnomCalcVals[longTermIndex];
        shortTermVal = *rateAnomCalcVals[shortTermIndex];

        *rateAnomVals[var] = 0.;
        if (!ZRO(longTermVal)) {
            *rateAnomVals[var] = (longTermVal - shortTermVal) / longTermVal;
        }
    }

    if (yearIndex + 1 > nYearsDynamicShort) {
        SW_VegProdSim->shortIndex++;
    }

    if (yearIndex + 1 > nYearsDynamicLong) {
        SW_VegProdSim->longIndex++;
    }
}

/**
@brief Calculate vegetation cover

@param[in] ss Struct of type SW_SOIL_SIM (soil sim -> ss) that holds constant
    predictor values for calculating updated vegetation values
@param[in] vps Struct of type SW_VEGPROD_SIM (VegProd sim -> vps) with
    short- and long-term climate values
@param[out] RelAbundanceL0 Array of size seven with calculated cover values.
    The elements are:
        -# needle-leaved tree "treeNL",
        -# broad-leaved tree "treeBL",
        -# shrub,
        -# forbs,
        -# C3-grass "grassC3",
        -# C4-grass "grassC4",
        -# bare ground
*/
void calc_CONUS_vegcov_2025(
    SW_SOIL_SIM *ss, SW_VEGPROD_SIM *vps, double *RelAbundanceL0
) {
    double ecoregionForest;
    double totalHerbaceousCoverNonForest;
    double totalHerbaceousCoverForest;
    double totalTreeCoverNonForest;
    double totalTreeCoverForest;
    double shrubCover;
    double bareGroundCover;
    double GrassC3CoverProportion;
    double GrassC4CoverProportion;
    double forbCoverProportion;
    double broadLeavedTreeCoverForestProportion;
    double needleLeavedTreeCoverForestProportion;
    double broadLeavedTreeCoverNonForestProportion;
    double needleLeavedTreeCoverNonForestProportion;
    double sumTreesForest;
    double scaledBroadLeavedTreeCoverForestProportion;
    double scaledNeedleLeavedTreeCoverForestProportion;
    double sumTreesNonForest;
    double scaledBroadLeavedTreeCoverNonForestProportion;
    double scaledNeedleLeavedTreeCoverNonForestProportion;
    double totalHerbaceousCover;
    double totalTreeCoverCover;
    double scaledBroadLeavedTreeCoverProportion;
    double scaledNeedleLeavedTreeCoverProportion;
    double sumTotal;
    double finalTotalHerbaceousCover;
    double finalTotalTreeCoverCover;
    double finalShrubCover;
    double finalBareGroundCover;
    double finalGrassC3Cover;
    double finalGrassC4Cover;
    double finalForbCover;
    double finalBroadLeavedTreeCover;
    double finalNeedleLeavedTreeCover;
    double sumHerbaceous;
    double scaledGrassC3CoverProportion;
    double scaledGrassC4CoverProportion;
    double scaledForbCoverProportion;

    double tempVal;

    /* Naming scheme of predictor variables
        * Predictor types
            * lt = longterm: long-term average conditions of annual values,
              e.g., mean across 30 years
            * st = shortterm: short-term average conditions of annual values,
              e.g., mean across 3 years
            * a = anomaly: difference between long-term average conditions
              and short-term average conditions, i.e., lt - st
            * ra = anomaly%: relative difference between long-term average
              conditions and short-term average conditions relative to
              long-term conditions, i.e., (lt - st) / lt
            * c = constant: conditions that do not change over time

        * Centering and scaling
            * z = prefix if predictor is centered and scaled,
              i.e., (predictor - centering) / scaling
            * o = prefix if predictor is on original scale
    */

    double annTemp = vps->annTempLongAvg;
    double annSeasonPrecip = vps->annSeasonPrecipLongAvg;
    double annIsotherm = vps->annIsothermLongAvg;
    double annWatDef = vps->annWaterDefLongAvg;
    double weighMeanSand = ss->percSand;
    double weighMeanCoarseFrag = ss->percCoarseFrag;
    double awhc = ss->totAWHC;
    double anomCorTempPrecip = vps->anomTempPrecipCorr;
    double annCorTempPrecip = vps->annTempPrecipLongAvg;
    double anomIsotherm = vps->anomIsotherm;
    double anomWatDef = vps->anomWaterDef;
    double annPrecip = vps->annPrecipLongAvg;
    double anomRateSeasonPrecip = vps->rateAnomSeasonPrecip;
    double annPrecipDriestMon = vps->annPrecipDriestMonLongAvg;
    double percClay = ss->surfaceClay;
    double anomRatePrecip = vps->rateAnomPrecip;
    double annWDD = vps->annWetDegDaysLongAvg;
    double anomRateWDD = vps->rateAnomWetDegDays;
    double percSOC = ss->surfaceOM * .58;

    double oltTempWarmestMonth = vps->annTempWarmestMonLongAvg;
    double oltTempColdestMonth = vps->annTempColdestMonLongAvg;
    double oltPrecipWettestMonth = vps->annPrecipWettestMonLongAvg;
    double oltWaterDeficit = vps->annWaterDefLongAvg;
    double oltCorPrTas = vps->annTempPrecipLongAvg;
    double oltIsothermality = vps->annIsothermLongAvg;

    double zltTempMean = (annTemp - 10.275203571) / 4.912309147;
    double zltPrecipSeasonality = (annSeasonPrecip - 0.923249309) / 0.245954382;
    double zltIsothermality = (annIsotherm - 38.120111845) / 5.019479015;
    double zltWaterDeficit = (annWatDef - 99.631248729) / 85.941823498;
    double zMeanSand = (weighMeanSand - 47.706485501) / 16.730875594;
    double zMeanCoarseFragments =
        (weighMeanCoarseFrag - 12.799273363) / 11.332548324;
    double zAWHC = (awhc - 13.671423701) / 5.155757156;
    double zstaCorPrTas = (anomCorTempPrecip - 0.012171065) / 0.139613922;
    double zstaIsothermality = (anomIsotherm - 0.538807833) / 1.422356333;
    double zstaWaterDeficit = (anomWatDef + 0.119596687) / 0.424434636;
    double zltPrecip = (annPrecip - 613.900118155) / 502.187690606;
    double zltCorPrTas = (annCorTempPrecip + 0.120988193) / 0.410662268;
    double zstraPrecipSeasonality =
        (anomRateSeasonPrecip + 0.025697534) / 0.132964252;
    double zSurfaceSOC = (percSOC - 3.681945502) / 6.405262851;
    double zltPrecipDriestMonth =
        (annPrecipDriestMon - 5.000260635) / 8.205443958;
    double zSurfaceClay = (percClay - 18.489433548) / 9.078669938;
    double zstraPrecip = (anomRatePrecip - 0.030312573) / 0.168767355;
    double zltWDD = (annWDD - 1762.977520092) / 1160.20756048;
    double zstraWDD = (anomRateWDD - 0.02989113) / 0.243425185;

    double zltTempMeanSqd = zltTempMean * zltTempMean;
    double zltPrecipSqd = zltPrecip * zltPrecip;
    double zltCorPrTasSqd = zltCorPrTas * zltCorPrTas;
    double zltIsothermalitySqd = zltIsothermality * zltIsothermality;
    double zstaIsothermalitySqd = zstaIsothermality * zstaIsothermality;
    double zstraPrecipSeasonalitySqd =
        zstraPrecipSeasonality * zstraPrecipSeasonality;
    double zstaCorPrTasSqd = zstaCorPrTas * zstaCorPrTas;
    double zMeanSandSqd = zMeanSand * zMeanSand;
    double zMeanCoarseFragmentsSqd =
        zMeanCoarseFragments * zMeanCoarseFragments;
    double zSurfaceSOCSqd = zSurfaceSOC * zSurfaceSOC;
    double zAWHCSqd = zAWHC * zAWHC;
    double zstaWaterDeficitSqd = zstaWaterDeficit * zstaWaterDeficit;
    double zSurfaceClaySqd = zSurfaceClay * zSurfaceClay;
    double zltPrecipSeasonalitySqd =
        zltPrecipSeasonality * zltPrecipSeasonality;
    double zstraWDDSqd = zstraWDD * zstraWDD;


    /* 2.1 Ecoregion classification model */
    /* Predictor variables of the ecoregion model are on the original scale */
    tempVal = 9.872597456 + -0.299906791 * oltTempWarmestMonth +
              0.245551132 * oltTempColdestMonth +
              0.010607279 * oltPrecipWettestMonth +
              -0.062058523 * oltWaterDeficit + -2.786336969 * oltCorPrTas +
              0.054028905 * oltIsothermality + -0.007599899 * ss->soilDepth +
              0.033478424 * ss->percSand + 0.031037682 * ss->percCoarseFrag +
              0.272601351 * percSOC;
    ecoregionForest = 1 / (1 + exp(-tempVal));

    /* Predictor variables of cover models are centered & scaled 'z*' */
    /* 2.2 Level 1 functional group cover models */
    /* 2.2.1 Total Herbaceous Cover – non-forest */
    tempVal =
        3.276248186 + 0.333755104 * zltTempMean +
        0.000988410 * zltPrecipSeasonality + -0.093566592 * zltIsothermality +
        -0.401713218 * zltWaterDeficit + -0.101054822 * zMeanSand +
        -0.059075423 * zMeanCoarseFragments + 0.097488751 * zAWHC +
        0.013584831 * zstaCorPrTas + -0.011392877 * zstaIsothermality +
        0.035960786 * zstaWaterDeficit + -0.217970279 * zltPrecipSqd +
        0.367790702 * zltCorPrTasSqd + -0.031527015 * zltIsothermalitySqd +
        -0.001997141 * zstraPrecipSeasonalitySqd +
        -0.022758623 * zstaCorPrTasSqd + 0.014352225 * zMeanSandSqd +
        0.025503657 * zMeanCoarseFragmentsSqd + 0.062557753 * zSurfaceSOCSqd +
        -0.048139022 * zAWHCSqd + 0.002192693 * zltWaterDeficit * zstaCorPrTas +
        0.014972237 * zltIsothermality * zstaWaterDeficit +
        -0.051575876 * zstaWaterDeficit * zltPrecip +
        0.038675358 * zltPrecipSeasonality * zstaWaterDeficit +
        -0.009761302 * zstaCorPrTas * zstaWaterDeficit +
        0.289340507 * zltIsothermality * zltPrecip +
        0.078632789 * zltPrecipSeasonality * zltIsothermality +
        0.132202861 * zltIsothermality * zltCorPrTas +
        -0.009488915 * zltIsothermality * zstaCorPrTas +
        0.063991677 * zltTempMean * zltIsothermality +
        0.020965402 * zltPrecipSeasonality * zstaIsothermality +
        0.237331817 * zltPrecip * zltCorPrTas +
        0.009654127 * zltCorPrTas * zstraPrecipSeasonality +
        0.011241877 * zstaCorPrTas * zstraPrecipSeasonality +
        -0.185452900 * zltTempMean * zltCorPrTas +
        0.042165264 * zMeanSand * zAWHC +
        -0.014331054 * zMeanSand * zMeanCoarseFragments;
    totalHerbaceousCoverNonForest = fmax(0., exp(tempVal) - 2);

    /* 2.2.2 Total Herbaceous Cover – forest */
    tempVal = 3.191837402 + -0.122792350 * zltPrecip +
              0.116138373 * zltPrecipDriestMonth + 0.073364801 * zltCorPrTas +
              -0.223502453 * zltIsothermality + 0.059416398 * zSurfaceClay +
              -0.143976298 * zMeanSand + 0.093798717 * zAWHC +
              -0.082685833 * zstaIsothermality +
              -0.026084114 * zstaWaterDeficit + 0.067376300 * zltCorPrTasSqd +
              0.025575815 * zltIsothermalitySqd +
              0.010298554 * zstaIsothermalitySqd +
              -0.001326385 * zstaWaterDeficitSqd + 0.075308377 * zMeanSandSqd +
              0.073306773 * zAWHCSqd +
              0.019965778 * zstaIsothermality * zstaWaterDeficit +
              -0.016757681 * zltCorPrTas * zstaWaterDeficit +
              -0.010148936 * zstaWaterDeficit * zltTempMean +
              0.092170440 * zltPrecip * zltIsothermality +
              0.016099673 * zltPrecip * zstaIsothermality +
              0.019360302 * zstaIsothermality * zstraPrecip +
              -0.007884072 * zltPrecipDriestMonth * zstaIsothermality +
              -0.006048882 * zstaIsothermality * zstaCorPrTas +
              -0.070468595 * zstaIsothermality * zltTempMean +
              -0.013811905 * zltPrecip * zstaCorPrTas +
              0.030103657 * zltPrecipDriestMonth * zstraPrecip +
              -0.001131342 * zstraPrecip * zstaCorPrTas +
              0.029138569 * zltTempMean * zstraPrecip +
              -0.081971171 * zltPrecipDriestMonth * zltCorPrTas +
              -0.015253376 * zltPrecipDriestMonth * zltTempMean +
              -0.039321175 * zltCorPrTas * zstaCorPrTas +
              0.042655226 * zltCorPrTas * zltTempMean +
              0.013823818 * zltTempMean * zstaCorPrTas +
              -0.020024507 * zAWHC * zSurfaceSOC +
              0.078265961 * zSurfaceClay * zAWHC +
              0.081708191 * zAWHC * zMeanCoarseFragments +
              0.185762180 * zMeanSand * zAWHC +
              -0.025554099 * zSurfaceSOC * zMeanCoarseFragments +
              -0.038284034 * zMeanSand * zSurfaceSOC;
    totalHerbaceousCoverForest = fmax(0., exp(tempVal) - 2);

    /* 2.2.3 Total Tree Cover – non-forest */
    tempVal = 2.58245786 + 1.14190425 * zltPrecip +
              -0.15075425 * zltPrecipSeasonality +
              0.03572512 * zltWaterDeficit + -0.07413619 * zMeanSand +
              -0.31894087 * zAWHC;
    totalTreeCoverNonForest = fmax(0., exp(tempVal) - 2);

    /* 2.2.4 Total Tree Cover – forest */
    tempVal = 3.28887888 + 0.10058372 * zltTempMean + 0.07165316 * zltPrecip +
              0.12712928 * zltPrecipDriestMonth + 0.03173495 * zSurfaceSOC +
              0.06648011 * zAWHC + -0.17846554 * zstraPrecip +
              -0.02914362 * zstaIsothermalitySqd +
              -0.04902481 * zSurfaceClaySqd +
              0.11841332 * zltPrecip * zstaIsothermality +
              0.11243677 * zltPrecipDriestMonth * zltCorPrTas +
              0.02314517 * zltTempMean * zltPrecipDriestMonth +
              -0.16107089 * zltTempMean * zltCorPrTas +
              -0.03108354 * zSurfaceSOC * zSurfaceClay +
              0.04845871 * zSurfaceSOC * zMeanCoarseFragments;
    totalTreeCoverForest = fmax(0., exp(tempVal) - 2);

    /* 2.2.5 shrub cover – CONUS-wide */
    tempVal = 2.939339967 + 0.145466528 * zltPrecip +
              -0.106416302 * zltPrecipSeasonality + -0.216540564 * zltCorPrTas +
              0.091558229 * zMeanSand + 0.007762789 * zMeanCoarseFragments +
              -0.083296458 * zltCorPrTasSqd + -0.056281606 * zMeanSandSqd +
              -0.006510544 * zAWHCSqd +
              0.048231968 * zltWDD * zstaIsothermality +
              -0.030802083 * zstaIsothermality * zltIsothermality +
              0.117940292 * zltIsothermality * zltTempMean +
              0.037905068 * zltPrecip * zstraPrecipSeasonality +
              0.045575111 * zltCorPrTas * zltTempMean;
    shrubCover = fmax(0., exp(tempVal) - 2);

    /* 2.2.6 bare ground cover – CONUS-wide */
    tempVal =
        2.746284299 + 0.262457983 * zltTempMean +
        0.087718972 * zltIsothermality + -0.715375616 * zltWDD +
        -0.267155829 * zMeanCoarseFragments + -0.064084039 * zstaIsothermality +
        0.037782133 * zstraWDD + -0.079708661 * zltTempMeanSqd +
        -0.036639124 * zltIsothermalitySqd + -0.002739534 * zltCorPrTasSqd +
        -0.076610337 * zltPrecip + 0.003781266 * zstraWDDSqd +
        0.133413710 * zltPrecip * zltWDD + -0.106746867 * zltWDD * zltCorPrTas +
        0.125888447 * zltIsothermality * zltCorPrTas;
    bareGroundCover = fmax(0., exp(tempVal) - 2);

    /* 2.3 Level 2 functional group cover models */
    /* 2.3.1 The proportion of total herbaceous that is C3 grass – CONUS-wide */
    tempVal =
        3.904167492 + -0.284822539 * zltTempMean + -0.387430439 * zltCorPrTas +
        -0.264775838 * zltIsothermality + -0.168662971 * zltIsothermalitySqd +
        -0.294089719 * zltCorPrTas * zltIsothermality + -0.009509765 * zltWDD;
    GrassC3CoverProportion = fmax(0., exp(tempVal) - 2);

    /* 2.3.2 The proportion of total herbaceous that is C4 grass – CONUS-wide */
    tempVal = 2.41145985 + 0.48381716 * zltTempMean + 1.02026843 * zltCorPrTas +
              0.54331054 * zltIsothermality + 0.05180567 * zstaCorPrTasSqd;
    GrassC4CoverProportion = fmax(0., exp(tempVal) - 2);

    /* 2.3.3 The proportion of total herbaceous that is forbs – CONUS-wide */
    tempVal = 3.514178452 + 0.248393795 * zltPrecip +
              -0.052267180 * zltPrecipSeasonality + -0.050423003 * zltCorPrTas +
              -0.020802257 * zltIsothermality + 0.041673226 * zMeanSand +
              0.059813143 * zMeanCoarseFragments +
              -0.035820999 * zstraPrecipSeasonality +
              0.051567741 * zltPrecipSeasonalitySqd +
              0.014241935 * zstraPrecipSeasonalitySqd +
              0.005274193 * zstaCorPrTasSqd +
              -0.037700614 * zltTempMean * zltWDD +
              -0.041194253 * zltCorPrTas * zltIsothermality +
              0.060107858 * zltIsothermality * zltTempMean +
              -0.092114033 * zMeanSand * zAWHC +
              -0.053246622 * zMeanSand * zMeanCoarseFragments +
              0.022969730 * zltPrecipSeasonality * zltTempMean +
              0.004823926 * zstaIsothermalitySqd +
              0.011303772 * zstaCorPrTas * zstraWDD;
    forbCoverProportion = fmax(0., exp(tempVal) - 2);

    /* 2.3.4 The proportion of total tree that is broad-leaved – forest */
    tempVal = 3.400837432 + 0.119928190 * zltTempMean +
              0.254698982 * zltPrecipDriestMonth + 0.415003665 * zSurfaceClay +
              0.005289910 * zMeanSand + -0.118297218 * zSurfaceSOC +
              0.216869470 * zAWHC + 0.127567513 * zstraPrecip +
              -0.030975228 * zstaCorPrTas + -0.136571036 * zltCorPrTasSqd +
              0.026270176 * zltIsothermality * zstaIsothermality +
              -0.218615897 * zltIsothermality * zltCorPrTas +
              -0.013504372 * zstaIsothermality * zltPrecip +
              -0.079997868 * zltTempMean * zstraPrecip +
              -0.001941377 * zltPrecipDriestMonth * zltCorPrTas +
              -0.108094550 * zltTempMean * zltCorPrTas +
              0.056873963 * zltTempMean * zstaCorPrTas +
              -0.084394634 * zSurfaceSOC * zAWHC +
              -0.011095426 * zSurfaceSOC * zMeanCoarseFragments +
              0.126127030 * zSurfaceClay * zMeanCoarseFragments +
              -0.249606357 * zMeanSand * zMeanCoarseFragments;
    broadLeavedTreeCoverForestProportion = fmax(0., exp(tempVal) - 2);

    /* 2.3.5 The proportion of total tree that is needle-leaved – forest */
    tempVal = 4.37205983 + -0.21286237 * zltPrecipDriestMonth +
              0.12039825 * zMeanSand + 0.07954909 * zSurfaceSOC +
              0.03631508 * zltTempMeanSqd +
              0.06724832 * zltCorPrTas * zltIsothermality +
              -0.08652516 * zltPrecipDriestMonth * zltCorPrTas +
              -0.04245934 * zltPrecipDriestMonth * zltTempMean;
    needleLeavedTreeCoverForestProportion = fmax(0., exp(tempVal) - 2);

    /* 2.3.6 The proportion of total tree that is broad-leaved – non-forest */
    tempVal = 3.10338252 + 0.28241315 * zltTempMean + 0.80500002 * zltPrecip +
              0.05186862 * zAWHC + -0.03647871 * zltCorPrTasSqd +
              -0.06790977 * zstaCorPrTasSqd + 0.18569895 * zMeanSandSqd +
              0.52842267 * zltTempMean * zltIsothermality +
              -0.30139958 * zAWHC * zMeanCoarseFragments;
    broadLeavedTreeCoverNonForestProportion = fmax(0., exp(tempVal) - 2);

    /* 2.3.7 The proportion of total tree that is needle-leaved – non-forest */
    tempVal = 4.52324174 + -0.18954119 * zltTempMean + -0.13086877 * zAWHC +
              -0.03177446 * zltIsothermalitySqd +
              -0.25163832 * zltTempMean * zltWaterDeficit +
              -0.24377773 * zltTempMean * zltIsothermality +
              -0.32422844 * zltTempMean * zltCorPrTas;
    needleLeavedTreeCoverNonForestProportion = fmax(0., exp(tempVal) - 2);


    /* 3 Scale level 2 cover variables by group */
    /* 3.1 For components of total herbaceous cover */
    sumHerbaceous =
        GrassC3CoverProportion + GrassC4CoverProportion + forbCoverProportion;
    scaledGrassC3CoverProportion = GrassC3CoverProportion / sumHerbaceous;
    scaledGrassC4CoverProportion = GrassC4CoverProportion / sumHerbaceous;
    scaledForbCoverProportion = forbCoverProportion / sumHerbaceous;

    /* 3.2 For components of total tree cover – forest */
    sumTreesForest = broadLeavedTreeCoverForestProportion +
                     needleLeavedTreeCoverForestProportion;

    scaledBroadLeavedTreeCoverForestProportion =
        broadLeavedTreeCoverForestProportion / sumTreesForest;

    scaledNeedleLeavedTreeCoverForestProportion =
        needleLeavedTreeCoverForestProportion / sumTreesForest;

    /* 3.3 For components of total tree cover – non-forest */
    sumTreesNonForest = broadLeavedTreeCoverNonForestProportion +
                        needleLeavedTreeCoverNonForestProportion;

    scaledBroadLeavedTreeCoverNonForestProportion =
        broadLeavedTreeCoverNonForestProportion / sumTreesNonForest;

    scaledNeedleLeavedTreeCoverNonForestProportion =
        needleLeavedTreeCoverNonForestProportion / sumTreesNonForest;

    /* 4 Combine across ecoregions */
    totalHerbaceousCover =
        ecoregionForest * totalHerbaceousCoverForest +
        (1 - ecoregionForest) * totalHerbaceousCoverNonForest;

    totalTreeCoverCover = ecoregionForest * totalTreeCoverForest +
                          (1 - ecoregionForest) * totalTreeCoverNonForest;

    scaledBroadLeavedTreeCoverProportion =
        ecoregionForest * scaledBroadLeavedTreeCoverForestProportion +
        (1 - ecoregionForest) * scaledBroadLeavedTreeCoverNonForestProportion;

    scaledNeedleLeavedTreeCoverProportion =
        ecoregionForest * scaledNeedleLeavedTreeCoverForestProportion +
        (1 - ecoregionForest) * scaledNeedleLeavedTreeCoverNonForestProportion;

    /* 5 Final predictions */
    sumTotal = totalHerbaceousCover + totalTreeCoverCover + shrubCover +
               bareGroundCover;

    finalTotalHerbaceousCover = totalHerbaceousCover / sumTotal;
    finalTotalTreeCoverCover = totalTreeCoverCover / sumTotal;
    finalShrubCover = shrubCover / sumTotal;
    finalBareGroundCover = bareGroundCover / sumTotal;

    finalGrassC3Cover =
        scaledGrassC3CoverProportion * finalTotalHerbaceousCover;
    finalGrassC4Cover =
        scaledGrassC4CoverProportion * finalTotalHerbaceousCover;
    finalForbCover = scaledForbCoverProportion * finalTotalHerbaceousCover;

    finalBroadLeavedTreeCover =
        scaledBroadLeavedTreeCoverProportion * finalTotalTreeCoverCover;
    finalNeedleLeavedTreeCover =
        scaledNeedleLeavedTreeCoverProportion * finalTotalTreeCoverCover;

    RelAbundanceL0[0] = finalNeedleLeavedTreeCover;
    RelAbundanceL0[1] = finalBroadLeavedTreeCover;
    RelAbundanceL0[2] = finalShrubCover;
    RelAbundanceL0[3] = finalForbCover;
    RelAbundanceL0[4] = finalGrassC3Cover;
    RelAbundanceL0[5] = finalGrassC4Cover;
    RelAbundanceL0[6] = finalBareGroundCover;
}

/**
@brief Calculate across-year predictors and update vegetation

    - first year: use annual predictors based on current year
    - later years: use annual predictors based on previous year(s)
      (skip second year because the same as first year)

@param[in] SW_SoilSim Struct of type SW_SOIL_SIM holding constant soil content
information that will be used during simulations
@param[in] yearIndex Index value specifying which place the year is in the
simulation process including spinup (i.e., [0, n years) )
@param[in] nYearsDynamicShort Number of years over which short-term vegetation
predictors are summarized (as anomaly to long-term predictors)
@param[in] nYearsDynamicLong Number of years over which long-term vegetation
predictors are summarized
@param[in] annTempOnly Specifies if this function is to only allocate
annual temperature arrays
@param[out] SW_VegProdSim Struct of type SW_VEGPROD_SIM that holds information
used and/or modified mainly during simulation runs; dynamic arrays will have a
new value for this year
@param[out] SW_VegProdRunIn Struct of type SW_VEGPROD_RUN_INPUTS that
    holds run-specific input information about vegetation production
*/
void update_veg_yearly(
    SW_SOIL_SIM *SW_SoilSim,
    TimeInt yearIndex,
    TimeInt nYearsDynamicShort,
    TimeInt nYearsDynamicLong,
    Bool annTempOnly,
    SW_VEGPROD_SIM *SW_VegProdSim,
    SW_VEGPROD_RUN_INPUTS *SW_VegProdRunIn
) {
    double RelAbundanceL0[7];

    if (yearIndex == 1) {
        return; /* skip second year because the same as first year */
    }

    /* Use values from previous year after first */
    yearIndex = (yearIndex == 0) ? 0 : yearIndex - 1;

    if (annTempOnly) {
        // Calculate long-term mean temperature
        calc_annTempLongAvg(
            yearIndex,
            nYearsDynamicLong,
            SW_VegProdSim,
            &SW_VegProdSim->annTempLongAvg
        );

    } else {
        // Calculate vegetation predictor variables
        calc_veg_predictor_vals(
            yearIndex, nYearsDynamicShort, nYearsDynamicLong, SW_VegProdSim
        );

        // Update vegetation values
        calc_CONUS_vegcov_2025(SW_SoilSim, SW_VegProdSim, RelAbundanceL0);

        SW_VegProdRunIn->veg[SW_TREENL].cov.fCover = RelAbundanceL0[0];
        SW_VegProdRunIn->veg[SW_TREEBL].cov.fCover = RelAbundanceL0[1];
        SW_VegProdRunIn->veg[SW_SHRUB].cov.fCover = RelAbundanceL0[2];
        SW_VegProdRunIn->veg[SW_FORBS].cov.fCover = RelAbundanceL0[3];
        SW_VegProdRunIn->veg[SW_GRASS3].cov.fCover = RelAbundanceL0[4];
        SW_VegProdRunIn->veg[SW_GRASS4].cov.fCover = RelAbundanceL0[5];

        SW_VegProdRunIn->bare_cov.fCover = RelAbundanceL0[6];
    }
}

/**
@brief Reads file for SW_VegProdIn

@param[in,out] SW_VegProdIn Struct of type SW_VEGPROD_INPUTS describing surface
    cover conditions in the simulation
@param[in,out] SW_VegProdRunIn Struct of type SW_VEGPROD_RUN_INPUTS that
    holds run-specific input information about vegetation production
@param[in] txtInFiles Array of program in/output files
@param[out] LogInfo Holds information on warnings and errors
*/
void SW_VPD_read(
    SW_VEGPROD_INPUTS *SW_VegProdIn,
    SW_VEGPROD_RUN_INPUTS *SW_VegProdRunIn,
    char *txtInFiles[],
    LOG_INFO *LogInfo
) {
    /* =================================================== */

    const char *const lineErrStrings[] = {
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
        "rooting profile: shape parameter 'a'",
        "rooting profile: shape parameter 'b'",
        "rooting profile: depth of rooting zone",
        "hydraulic redistribution: flag",
        "hydraulic redistribution: maxCondroot",
        "hydraulic redistribution: swpMatric50",
        "hydraulic redistribution: shapeCond",
        "critical soil water potentials",
        "CO2 Biomass Coefficient 1",
        "CO2 Biomass Coefficient 2",
        "CO2 WUE Coefficient 1",
        "CO2 WUE Coefficient 2",
        "Spatial reference of biomass inputs (are inputs as if 100% cover)",
        "year of vegetation inputs"
    };

    FILE *f;
    TimeInt mon = Jan;
    int x;
    int k;
    int lineno = 0;
    int index;
    // last case line number before monthly biomass densities
    const int line_help = 35;
    double help_veg[NVEGTYPES];
    double help_bareGround = 0.;
    double litt;
    double biom;
    double pctl;
    double laic;
    double *monBioVals[] = {&litt, &biom, &pctl, &laic};
    char *MyFileName;
    char inbuf[MAX_FILENAMESIZE];
    char vegStrs[NVEGTYPES][20] = {{'\0'}};
    char bareGroundStr[20] = {'\0'};
    const int numMonthVals = 4;
    int expectedNumInVals;

    MyFileName = txtInFiles[eVegProd];
    f = OpenFile(MyFileName, "r", LogInfo);
    if (LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

    while (GetALine(f, inbuf, MAX_FILENAMESIZE)) {
        lineno++;

        if (lineno <= line_help) {
            if ((lineno >= 1 && lineno <= 3) || lineno == 34 || lineno == 35) {

                x = sscanf(inbuf, "%19s", vegStrs[0]);
                expectedNumInVals = 1;

            } else {
                // Inputs must match order of veg types 0..NVEGTYPES
                x = sscanf(
                    inbuf,
                    "%19s %19s %19s %19s %19s %19s %19s",
                    vegStrs[SW_TREENL],
                    vegStrs[SW_TREEBL],
                    vegStrs[SW_SHRUB],
                    vegStrs[SW_FORBS],
                    vegStrs[SW_GRASS3],
                    vegStrs[SW_GRASS4],
                    bareGroundStr
                );

                expectedNumInVals = (lineno >= 6) ? NVEGTYPES : NVEGTYPES + 1;

                ForEachVegType(k) {
                    help_veg[k] = sw_strtod(vegStrs[k], MyFileName, LogInfo);
                    if (LogInfo->stopRun) {
                        goto closeFile;
                    }
                }

                if (x == NVEGTYPES + 1) {
                    help_bareGround =
                        sw_strtod(bareGroundStr, MyFileName, LogInfo);
                    if (LogInfo->stopRun) {
                        goto closeFile;
                    }
                }
            }

            if (x != expectedNumInVals) {
                LogError(
                    LogInfo,
                    LOGERROR,
                    "Invalid input %s in '%s': "
                    "found n = %d values but expected %d on input line %d",
                    lineErrStrings[lineno],
                    MyFileName,
                    x,
                    expectedNumInVals,
                    lineno
                );
                goto closeFile;
            }

            switch (lineno) {
            /* method for vegetation parameters */
            case 1:
                SW_VegProdIn->veg_method =
                    sw_strtoi(vegStrs[0], MyFileName, LogInfo);
                if (LogInfo->stopRun) {
                    goto closeFile;
                }

                if (SW_VegProdIn->veg_method < 0 ||
                    SW_VegProdIn->veg_method > 2) {

                    LogError(
                        LogInfo, LOGERROR, "'veg_method' must be 0, 1 or 2."
                    );
                }

                break;

            /* Number of years for long-term dynamic vegetation */
            case 2:
                SW_VegProdIn->nYearsDynamicLong =
                    sw_strtoi(vegStrs[0], MyFileName, LogInfo);
                if (LogInfo->stopRun) {
                    goto closeFile;
                }

                if (SW_VegProdIn->nYearsDynamicLong == 0) {
                    LogError(
                        LogInfo, LOGERROR, "'nYearsDynamicLong' must be > 0."
                    );
                    goto closeFile;
                }
                break;

            /* Number of years for short-term dynamic vegetation */
            case 3:
                SW_VegProdIn->nYearsDynamicShort =
                    sw_strtoi(vegStrs[0], MyFileName, LogInfo);
                if (LogInfo->stopRun) {
                    goto closeFile;
                }

                if (SW_VegProdIn->nYearsDynamicShort == 0) {
                    LogError(
                        LogInfo, LOGERROR, "'nYearsDynamicShort' must be > 0."
                    );
                } else if (SW_VegProdIn->nYearsDynamicShort >=
                           SW_VegProdIn->nYearsDynamicLong) {
                    LogError(
                        LogInfo,
                        LOGERROR,
                        "'nYearsDynamicShort' must be < 'nYearsDynamicLong'."
                    );
                }
                if (LogInfo->stopRun) {
                    goto closeFile;
                }
                break;

            /* fractions of vegetation types */
            case 4:
                ForEachVegType(k) {
                    SW_VegProdRunIn->veg[k].cov.fCover = help_veg[k];
                }
                SW_VegProdRunIn->bare_cov.fCover = help_bareGround;
                break;

            /* albedo */
            case 5:
                ForEachVegType(k) {
                    SW_VegProdIn->veg[k].cov.albedo = help_veg[k];
                }
                SW_VegProdIn->bare_cov.albedo = help_bareGround;
                break;

            /* canopy height */
            case 6:
                ForEachVegType(k) {
                    SW_VegProdIn->veg[k].cnpy.xinflec = help_veg[k];
                }
                break;

            case 7:
                ForEachVegType(k) {
                    SW_VegProdIn->veg[k].cnpy.yinflec = help_veg[k];
                }
                break;

            case 8:
                ForEachVegType(k) {
                    SW_VegProdIn->veg[k].cnpy.range = help_veg[k];
                }
                break;

            case 9:
                ForEachVegType(k) {
                    SW_VegProdIn->veg[k].cnpy.slope = help_veg[k];
                }
                break;

            case 10:
                ForEachVegType(k) {
                    SW_VegProdIn->veg[k].canopy_height_constant = help_veg[k];
                }
                break;

            /* vegetation interception parameters */
            case 11:
                ForEachVegType(k) {
                    SW_VegProdIn->veg[k].veg_kSmax = help_veg[k];
                }
                break;

            case 12:
                ForEachVegType(k) {
                    SW_VegProdIn->veg[k].veg_kdead = help_veg[k];
                }
                break;

            /* litter interception parameters */
            case 13:
                ForEachVegType(k) {
                    SW_VegProdIn->veg[k].lit_kSmax = help_veg[k];
                }
                break;

            /* parameter for partitioning of bare-soil evaporation and
             * transpiration */
            case 14:
                ForEachVegType(k) {
                    SW_VegProdIn->veg[k].EsTpartitioning_param = help_veg[k];
                }
                break;

            /* Parameter for scaling and limiting bare soil evaporation rate */
            case 15:
                ForEachVegType(k) {
                    SW_VegProdIn->veg[k].Es_param_limit = help_veg[k];
                }
                break;

            /* shade effects */
            case 16:
                ForEachVegType(k) {
                    SW_VegProdIn->veg[k].shade_scale = help_veg[k];
                }
                break;

            case 17:
                ForEachVegType(k) {
                    SW_VegProdIn->veg[k].shade_deadmax = help_veg[k];
                }
                break;

            case 18:
                ForEachVegType(k) {
                    SW_VegProdIn->veg[k].tr_shade_effects.xinflec = help_veg[k];
                }
                break;

            case 19:
                ForEachVegType(k) {
                    SW_VegProdIn->veg[k].tr_shade_effects.yinflec = help_veg[k];
                }
                break;

            case 20:
                ForEachVegType(k) {
                    SW_VegProdIn->veg[k].tr_shade_effects.range = help_veg[k];
                }
                break;

            case 21:
                ForEachVegType(k) {
                    SW_VegProdIn->veg[k].tr_shade_effects.slope = help_veg[k];
                }
                break;

            /* Rooting profile parameters */
            case 22:
                ForEachVegType(k) {
                    SW_VegProdIn->veg[k].rootProfileParam[0] = help_veg[k];
                }
                break;

            case 23:
                ForEachVegType(k) {
                    SW_VegProdIn->veg[k].rootProfileParam[1] = help_veg[k];
                }
                break;

            case 24:
                ForEachVegType(k) {
                    SW_VegProdIn->veg[k].rootProfileParam[2] = help_veg[k];
                }
                break;

            /* Hydraulic redistribution */
            case 25:
                ForEachVegType(k) {
                    SW_VegProdIn->veg[k].flagHydraulicRedistribution =
                        (Bool) EQ(help_veg[k], 1.);
                }
                break;

            case 26:
                ForEachVegType(k) {
                    SW_VegProdIn->veg[k].maxCondroot = help_veg[k];
                }
                break;

            case 27:
                ForEachVegType(k) {
                    SW_VegProdIn->veg[k].swpMatric50 = help_veg[k];
                }
                break;

            case 28:
                ForEachVegType(k) {
                    SW_VegProdIn->veg[k].shapeCond = help_veg[k];
                }
                break;

            /* Critical soil water potential */
            case 29:
                ForEachVegType(k) {
                    SW_VegProdIn->veg[k].SWPcrit = -10. * help_veg[k];
                    // for use with get_swa for properly partitioning swa
                    SW_VegProdIn->critSoilWater[k] = help_veg[k];
                }
                get_critical_rank(SW_VegProdIn);
                break;

            /* CO2 Biomass Power Equation */
            // Coefficient 1
            case 30:
                ForEachVegType(k) {
                    SW_VegProdIn->veg[k].co2_bio_coeff1 = help_veg[k];
                }
                break;

            // Coefficient 2
            case 31:
                ForEachVegType(k) {
                    SW_VegProdIn->veg[k].co2_bio_coeff2 = help_veg[k];
                }
                break;

            /* CO2 WUE Power Equation */
            // Coefficient 1
            case 32:
                ForEachVegType(k) {
                    SW_VegProdIn->veg[k].co2_wue_coeff1 = help_veg[k];
                }
                break;

            // Coefficient 2
            case 33:
                ForEachVegType(k) {
                    SW_VegProdIn->veg[k].co2_wue_coeff2 = help_veg[k];
                }
                break;

            /* Spatial reference of biomass inputs */
            case 34:
                SW_VegProdIn->isBiomAsIf100Cover =
                    sw_strtoi(vegStrs[0], MyFileName, LogInfo) ? swTRUE :
                                                                 swFALSE;
                if (LogInfo->stopRun) {
                    goto closeFile;
                }
                break;

            /* Calendar year corresponding to vegetation inputs */
            case 35:
                SW_VegProdIn->vegYear =
                    (TimeInt) sw_strtoi(vegStrs[0], MyFileName, LogInfo);
                if (LogInfo->stopRun) {
                    goto closeFile;
                }
                break;

            default:
                break;
            }

        } else {
            /* Mean monthly vegetation inputs */

            if (lineno >= line_help + 1 &&
                ((lineno - (line_help + 1)) % 12) == 0) {
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

            if (x != numMonthVals || lineno > line_help + 12 * NVEGTYPES) {
                LogError(
                    LogInfo,
                    LOGERROR,
                    "Vegetation inputs contain invalid row %d: '%s'",
                    lineno,
                    inbuf
                );
                goto closeFile;
            }

            for (index = 0; index < numMonthVals; index++) {
                *(monBioVals[index]) =
                    sw_strtod(vegStrs[index], MyFileName, LogInfo);
                if (LogInfo->stopRun) {
                    goto closeFile;
                }
            }

            // Inputs must match order of veg types 0..NVEGTYPES
            ForEachVegType(k) {
                if (lineno > line_help + 12 * k &&
                    lineno <= line_help + 12 * (k + 1)) {
                    break;
                }
            }

            SW_VegProdRunIn->veg[k].litter[mon] = litt;
            SW_VegProdRunIn->veg[k].biomass[mon] = biom;
            SW_VegProdRunIn->veg[k].pct_live[mon] = pctl;
            SW_VegProdRunIn->veg[k].lai_conv[mon] = laic;

            mon++;
        }
    }

    if (mon < Dec) {
        LogError(
            LogInfo, LOGWARN, "Veg values missing after month %d", mon + 1
        );
    }

closeFile: { CloseFile(&f, LogInfo); }
}

/**
@brief Normalize fractional cover inputs

Fractional cover of bare ground and of each vegetation type are normalized
so that the sum across all cover components is exactly 1 (full land cover).

Warn user if the sum deviates from 1 by more than a tolerance of 1e-4.

@param[in,out] SW_VegProdRunIn Struct of type SW_VEGPROD_RUN_INPUTS that
    holds run-specific input information about vegetation production
@param[out] LogInfo Holds information on warnings and errors

@sideeffect
- Adjusted `SW_VegProdRunIn->bare_cov.fCover` and
`SW_VegProdRunIn->veg[k].cov.fCover`
*/
void fixVegCoverInputs(
    SW_VEGPROD_RUN_INPUTS *SW_VegProdRunIn, LOG_INFO *LogInfo
) {
    int k;
    double fraction_sum = 0.;
    double tolerance = 1e-4; // txt-input precision may be at most 3-4 digits
    char msg[MAX_LOG_SIZE] = {'\0'};
    char buf[MAX_LOG_SIZE] = {'\0'};
    char *writePtr = NULL;
    char *writeEndPtr = NULL;
    size_t writeSize;
    int bufSize;
    Bool fullBuffer = swFALSE;
    int countFullBuffer = 0;


    /* Normalize cover */
    fraction_sum = SW_VegProdRunIn->bare_cov.fCover;
    ForEachVegType(k) { fraction_sum += SW_VegProdRunIn->veg[k].cov.fCover; }

    SW_VegProdRunIn->bare_cov.fCover /= fraction_sum;
    ForEachVegType(k) { SW_VegProdRunIn->veg[k].cov.fCover /= fraction_sum; }


    /* Generate warning if adjustment > tolerance */
    if (!EQ_w_tol(fraction_sum, 1.0, tolerance)) {
        writePtr = msg;
        writeEndPtr = msg + sizeof msg - 1;
        writeSize = MAX_LOG_SIZE;

        bufSize = snprintf(
            buf,
            sizeof buf,
            "Normalized fractional land cover inputs: "
            "previous sum = %.6f instead of 1.0. "
            "Updated cover: bare ground = %.4f",
            fraction_sum,
            SW_VegProdRunIn->bare_cov.fCover
        );
        fullBuffer = sw_memccpy_inc(
            (void **) &writePtr, writeEndPtr, (void *) buf, '\0', &writeSize
        );
        countFullBuffer += (fullBuffer || bufSize >= MAX_LOG_SIZE) ? 1 : 0;

        ForEachVegType(k) {
            bufSize = snprintf(
                buf,
                sizeof buf,
                ", %s = %.4f",
                key2veg[k],
                SW_VegProdRunIn->veg[k].cov.fCover
            );
            fullBuffer = sw_memccpy_inc(
                (void **) &writePtr, writeEndPtr, (void *) buf, '\0', &writeSize
            );
            countFullBuffer += (fullBuffer || bufSize >= MAX_LOG_SIZE) ? 1 : 0;
        }

        LogError(LogInfo, LOGWARN, "%s", msg);

        if (countFullBuffer > 0) {
            reportFullBuffer(LOGERROR, LogInfo);
        }
    }
}

/**
@brief Constructor for VegProd input and output information

@param[out] SW_VegProdIn VegProdIn Struct of type SW_VEGPROD_INPUTS describing
surface cover conditions in the simulation
@param[out] SW_VegProdRunIn Struct of type SW_VEGPROD_RUN_INPUTS that
    holds run-specific input information about vegetation production
@param[out] vp_p_oagg A list of output structs of type SW_VEGPROD_OUTPUTS
    to accumulate output
@param[out] vp_p_accu A list of output structs of type SW_VEGPROD_OUTPUTS
    to aggregate output
*/
void SW_VPD_construct(
    SW_VEGPROD_INPUTS *SW_VegProdIn,
    SW_VEGPROD_RUN_INPUTS *SW_VegProdRunIn,
    SW_VEGPROD_OUTPUTS vp_p_oagg[],
    SW_VEGPROD_OUTPUTS vp_p_accu[]
) {
    /* =================================================== */
    OutPeriod pd;

    // Clear the module structure:
    memset(SW_VegProdIn, 0, sizeof(SW_VEGPROD_INPUTS));
    memset(SW_VegProdRunIn, 0, sizeof(SW_VEGPROD_RUN_INPUTS));

    ForEachOutPeriod(pd) {
        memset(&vp_p_oagg[pd], 0, sizeof(SW_VEGPROD_OUTPUTS));
        memset(&vp_p_accu[pd], 0, sizeof(SW_VEGPROD_OUTPUTS));
    }
}

/**
@brief Deconstructor for the SW_VegProd suite of structs if needed

@param[in,out] SW_VegProdSim Struct of type SW_VEGPROD_SIM that holds
information used and/or modified mainly during simulation runs; return with
deallocated and NULL pointers
*/
void SW_VPD_deconstruct(SW_VEGPROD_SIM *SW_VegProdSim) {
    int index;
    int co2Arr;
    const int nCO2Arrs = 2;
    const int numArrays = 11;
    double **allocArray[] = {
        &SW_VegProdSim->annTemp,
        &SW_VegProdSim->annTempPrecipCorr,
        &SW_VegProdSim->annIsotherm,
        &SW_VegProdSim->annPrecip,
        &SW_VegProdSim->annWaterDef,
        &SW_VegProdSim->annSeasonPrecip,
        &SW_VegProdSim->annPrecipDriestMon,
        &SW_VegProdSim->annWetDegDays,
        &SW_VegProdSim->annTempWarmestMon,
        &SW_VegProdSim->annTempColdestMon,
        &SW_VegProdSim->annPrecipWettestMon
    };

    for (index = 0; index < numArrays; index++) {
        if (!isnull(*(allocArray[index]))) {
            free((void *) *(allocArray[index]));
            *(allocArray[index]) = NULL;
        }
    }

    ForEachVegType(index) {
        for (co2Arr = 0; co2Arr < nCO2Arrs; co2Arr++) {
            if (!isnull(SW_VegProdSim->veg[index].co2_multipliers[co2Arr])) {
                free((void *) SW_VegProdSim->veg[index].co2_multipliers[co2Arr]
                );
                SW_VegProdSim->veg[index].co2_multipliers[co2Arr] = NULL;
            }
        }
    }
}

/**
@brief Initialize all possible pointers in SW_VEGPROD_SIM to NULL

@param[out] SW_VegProdSim Struct of type SW_VEGPROD_SIM that holds information
used and/or modified mainly during simulation runs; dynamic arrays will be
initialized to NULl
*/
void SW_VPD_init_ptrs(SW_VEGPROD_SIM *SW_VegProdSim) {
    int index;
    const int numArrays = 11;
    double **allocArray[] = {
        &SW_VegProdSim->annTemp,
        &SW_VegProdSim->annTempPrecipCorr,
        &SW_VegProdSim->annIsotherm,
        &SW_VegProdSim->annPrecip,
        &SW_VegProdSim->annWaterDef,
        &SW_VegProdSim->annSeasonPrecip,
        &SW_VegProdSim->annPrecipDriestMon,
        &SW_VegProdSim->annWetDegDays,
        &SW_VegProdSim->annTempWarmestMon,
        &SW_VegProdSim->annTempColdestMon,
        &SW_VegProdSim->annPrecipWettestMon
    };

    for (index = 0; index < numArrays; index++) {
        *(allocArray[index]) = NULL;
    }

    ForEachVegType(index) {
        SW_VegProdSim->veg[index].co2_multipliers[0] = NULL;
        SW_VegProdSim->veg[index].co2_multipliers[1] = NULL;
    }
}

void SW_VPD_init_run(SW_RUN *sw, LOG_INFO *LogInfo) {
    TimeInt year;
    TimeInt n_years = sw->ModelIn.endyr - sw->ModelIn.startyr + 1;
    int k;
    LyrIndex n_layers = sw->RunIn.SiteRunIn.n_layers;
    Bool inNorthHem = sw->RunIn.ModelRunIn.isnorth;
    int veg_method = sw->VegProdIn.veg_method;
    Bool allocAnnTemp = (Bool) (sw->SiteIn.methodMaxDepthSoilTemperature == 1);
    Bool annTempOnly =
        (Bool) (allocAnnTemp && veg_method != VEG_METHOD_DYN_EST);

    alloc_co2(sw->VegProdSim.veg, n_years, LogInfo);
    if (LogInfo->stopRun) {
        return;
    }

    /* Set co2-multipliers to default */
    for (year = 0; year < n_years; year++) {
        ForEachVegType(k) {
            sw->VegProdSim.veg[k].co2_multipliers[BIO_INDEX][year] = 1.;
            sw->VegProdSim.veg[k].co2_multipliers[WUE_INDEX][year] = 1.;
        }
    }

    sw->VegProdSim.shortIndex = sw->VegProdSim.longIndex = 0;

    if (veg_method == VEG_METHOD_LONG_EST) {
        /* static veg: estimated from simulation-wide climate */
        estimateVegetationFromClimate(
            &sw->RunIn.VegProdRunIn,
            sw->RunIn.weathRunAllHist,
            &sw->ModelIn,
            &sw->ModelSim,
            inNorthHem,
            veg_method,
            LogInfo
        );
        if (LogInfo->stopRun) {
            return; // Exit function prematurely due to error
        }
    }

    if (veg_method == VEG_METHOD_DYN_EST) {
        /* dynamic veg: init arrays here; calculated by SW_VPD_new_year() */
        calc_const_dynamic_veg_info(
            &sw->SoilSim, &sw->RunIn.SoilRunIn, &sw->SiteSim, n_layers
        );
    }

    if (veg_method == VEG_METHOD_DYN_EST || allocAnnTemp) {
        /* Number of years for dynamic vegetation: spinup + simulation years */
        if (sw->ModelIn.SW_SpinUp.duration > 0) {
            n_years += sw->ModelIn.SW_SpinUp.duration;
        }

        alloc_nyear_arrays(n_years, annTempOnly, &sw->VegProdSim, LogInfo);
        if (LogInfo->stopRun) {
            return; // Exit function prematurely due to error
        }
    }

    if (veg_method != VEG_METHOD_DYN_EST) {
        fixVegCoverInputs(&sw->RunIn.VegProdRunIn, LogInfo);
        checkVegetation(&sw->RunIn.VegProdRunIn, LogInfo);
    }
}

/**
@brief Validate vegetation values

Check cover and monthly biomass values (if cover > 0)

@param[in] SW_VegProdRunIn Struct of type SW_VEGPROD_RUN_INPUTS that
    holds run-specific input information about vegetation production
@param[out] LogInfo Holds information on warnings and errors
*/
void checkVegetation(
    SW_VEGPROD_RUN_INPUTS *SW_VegProdRunIn, LOG_INFO *LogInfo
) {
    unsigned int k;
    unsigned int mon;
    double totalCover = SW_VegProdRunIn->bare_cov.fCover;


    if (totalCover < 0 || GT(totalCover, 1.)) {
        LogError(
            LogInfo,
            LOGERROR,
            "bare-ground cover (%.6f) is outside 0-1",
            totalCover
        );
        return;
    }


    ForEachVegType(k) {

        if (SW_VegProdRunIn->veg[k].cov.fCover < 0 ||
            GT(SW_VegProdRunIn->veg[k].cov.fCover, 1.)) {
            LogError(
                LogInfo,
                LOGERROR,
                "%s cover (%.4f) is outside 0-1",
                key2veg[k],
                SW_VegProdRunIn->veg[k].cov.fCover
            );
            return;
        }

        totalCover += SW_VegProdRunIn->veg[k].cov.fCover;

        /* Don't check biomass values if zero cover */
        if (ZRO(SW_VegProdRunIn->veg[k].cov.fCover)) {
            continue;
        }

        for (mon = 0; mon < MAX_MONTHS; mon++) {

            if (SW_VegProdRunIn->veg[k].litter[mon] < 0) {
                LogError(
                    LogInfo,
                    LOGERROR,
                    "%s litter (%.4f) is negative in month %d.",
                    key2veg[k],
                    SW_VegProdRunIn->veg[k].litter[mon],
                    mon + 1
                );
                return;
            }

            if (SW_VegProdRunIn->veg[k].biomass[mon] < 0) {
                LogError(
                    LogInfo,
                    LOGERROR,
                    "%s biomass (%.4f) is negative in month %d.",
                    key2veg[k],
                    SW_VegProdRunIn->veg[k].biomass[mon],
                    mon + 1
                );
                return;
            }

            if (SW_VegProdRunIn->veg[k].pct_live[mon] < 0 ||
                SW_VegProdRunIn->veg[k].pct_live[mon] > 1) {
                LogError(
                    LogInfo,
                    LOGERROR,
                    "%s pct_live (%.4f) not within [0,1] in month %d.",
                    key2veg[k],
                    SW_VegProdRunIn->veg[k].pct_live[mon],
                    mon + 1
                );
                return;
            }

            if (SW_VegProdRunIn->veg[k].lai_conv[mon] < 0) {
                LogError(
                    LogInfo,
                    LOGERROR,
                    "%s lai_conv (%.4f) is negative in month %d.",
                    key2veg[k],
                    SW_VegProdRunIn->veg[k].lai_conv[mon],
                    mon + 1
                );
                return;
            }
        }
    }

    if (totalCover < 0 || GT(totalCover, 1.)) {
        LogError(
            LogInfo,
            LOGERROR,
            "sum of cover components (%.6f) is outside 0-1",
            totalCover
        );
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

@param[in] SW_YearWeathHist Array containing all historical data of a site
@param[in] SW_ModelSim Struct of type SW_MODEL_SIM holding basic intermediate
    time information about the simulation run
@param[in] SW_VegProdSim Struct of type SW_VEGPROD_SIM that holds
information used and/or modified mainly during simulation runs
@param[in] SW_SoilSim Struct of type SW_SOIL_SIM holding constant soil content
information that will be used during simulations
@param[in] isBiomAsIf100Cover Spatial reference of biomass inputs
    (are inputs as if 100% cover)
        - false (0): values as is (at given cover)
        - true (1), values as if cover was 100%
@param[in] veg_method The requested method to estimate vegetation values,
    see SW_VEGPROD_INPUTS.veg_method
@param[in] startYearWeather First year of the weather data
@param[in] nYearsDynamicShort Number of years over which short-term vegetation
predictors are summarized (as anomaly to long-term predictors)
@param[in] nYearsDynamicLong Number of years over which long-term vegetation
predictors are summarized
@param[in] methodMaxDepthSoilTemperature Method for soil temperature at
maximum depth:
        0 (user provided value);
        1 (dynamically calculated from a moving long-term mean annual air
           temperature, see `nYearsDynamicLong` from veg.in)
@param[out] SW_VegProdRunIn Struct of type SW_VEGPROD_RUN_INPUTS that
    holds run-specific input information about vegetation production
@param[out] vegSim Array of size NVEGTYPES of type VegTypeSim describing
    all NVEGTYPES vegetation types through values used purely during simulation
@param[out] vegIn Array of size NVEGTYPES of type VegTypeIn describing
    all NVEGTYPES vegetation types through static simulation values (cannot
    change between simulation runs)
@param[out] LogInfo Holds information on warnings and errors
*/
void SW_VPD_new_year(
    SW_WEATHER_HIST *SW_YearWeathHist,
    SW_MODEL_SIM *SW_ModelSim,
    SW_VEGPROD_SIM *SW_VegProdSim,
    SW_SOIL_SIM *SW_SoilSim,
    Bool isBiomAsIf100Cover,
    int veg_method,
    TimeInt startYearWeather,
    TimeInt nYearsDynamicShort,
    TimeInt nYearsDynamicLong,
    unsigned int methodMaxDepthSoilTemperature,
    SW_VEGPROD_RUN_INPUTS *SW_VegProdRunIn,
    VegTypeSim vegSim[],
    VegTypeIn vegIn[],
    LOG_INFO *LogInfo
) {
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
    lai_corr = VegProdIn->lai_conv[m] * (1. - pstem) + aconst * pstem;
    lai_standing    = VegProdIn->biomass[m] / lai_corr;
    where pstem = 0.3,
    aconst = 464.0,
    conv_stcr = 3.0;
    *
    */

    TimeInt doy; /* base1 */
    TimeInt yearIdx = SW_ModelSim->yearIdx;
    TimeInt yearIdxSpinSim = SW_ModelSim->yearIdxSpinSim;
    TimeInt weatherYearIndex = SW_ModelSim->year - startYearWeather;
    int k;
    int mon;
    Bool allocAnnTemp = (Bool) (methodMaxDepthSoilTemperature == 1);
    Bool annTempOnly =
        (Bool) (allocAnnTemp && veg_method != VEG_METHOD_DYN_EST);

    VegTypeRunIn *vegRunIn = SW_VegProdRunIn->veg; // array of NVEGTYPES

    // Interpolation is to be in base1 in `interpolate_monthlyValues()`
    Bool interpAsBase1 = swTRUE;

    /* Monthly biomass after CO2 effects */
    double biomass_after_CO2[MAX_MONTHS];

    /* Monthly biomass at 100% cover */
    double biomassAsIf100Cover[MAX_MONTHS];
    double litterAsIf100Cover[MAX_MONTHS];


    /* Update dynamic vegetation or boundary conditions of soil temperature */
    if ((allocAnnTemp || veg_method == VEG_METHOD_DYN_EST)) {
        /* Calculate annual predictor with weather of current year */
        calc_yearly_hist_vals(
            &SW_YearWeathHist[weatherYearIndex],
            SW_ModelSim,
            yearIdxSpinSim,
            annTempOnly,
            SW_VegProdSim
        );

        /* Calculate across-year predictors and update vegetation */
        update_veg_yearly(
            SW_SoilSim,
            yearIdxSpinSim,
            nYearsDynamicShort,
            nYearsDynamicLong,
            annTempOnly,
            SW_VegProdSim,
            SW_VegProdRunIn
        );

        if (veg_method == VEG_METHOD_DYN_EST) {
            checkVegetation(SW_VegProdRunIn, LogInfo);
        }
    }


    /* Calculate daily vegetation from monthly & adjust for aCO2 */
    ForEachVegType(k) {
        if (GT(vegRunIn[k].cov.fCover, 0.)) {

            /* Scale biomass to as if 100% cover unless provided as inputs */
            for (mon = 0; mon < MAX_MONTHS; mon++) {
                biomassAsIf100Cover[mon] =
                    isBiomAsIf100Cover ?
                        vegRunIn[k].biomass[mon] :
                        (vegRunIn[k].biomass[mon] / vegRunIn[k].cov.fCover);

                litterAsIf100Cover[mon] =
                    isBiomAsIf100Cover ?
                        vegRunIn[k].litter[mon] :
                        (vegRunIn[k].litter[mon] / vegRunIn[k].cov.fCover);
            }

            if (k == SW_TREENL || k == SW_TREEBL) {
                // CO2 effects on tree biomass restricted to percent live
                // biomass, i.e., total tree biomass is constant while live
                // biomass is increasing
                apply_biomassCO2effect(
                    biomass_after_CO2,
                    vegRunIn[k].pct_live,
                    vegSim[k].co2_multipliers[BIO_INDEX][yearIdx]
                );

                interpolate_monthlyValues(
                    biomass_after_CO2,
                    interpAsBase1,
                    SW_ModelSim->cum_monthdays,
                    SW_ModelSim->days_in_month,
                    vegSim[k].pct_live_daily
                );
                interpolate_monthlyValues(
                    biomassAsIf100Cover,
                    interpAsBase1,
                    SW_ModelSim->cum_monthdays,
                    SW_ModelSim->days_in_month,
                    vegSim[k].biomass_daily
                );

            } else {
                // CO2 effects on biomass applied to total biomass, i.e.,
                // total and live biomass are increasing
                apply_biomassCO2effect(
                    biomass_after_CO2,
                    biomassAsIf100Cover,
                    vegSim[k].co2_multipliers[BIO_INDEX][yearIdx]
                );

                interpolate_monthlyValues(
                    biomass_after_CO2,
                    interpAsBase1,
                    SW_ModelSim->cum_monthdays,
                    SW_ModelSim->days_in_month,
                    vegSim[k].biomass_daily
                );
                interpolate_monthlyValues(
                    vegRunIn[k].pct_live,
                    interpAsBase1,
                    SW_ModelSim->cum_monthdays,
                    SW_ModelSim->days_in_month,
                    vegSim[k].pct_live_daily
                );
            }

            // Interpolation of remaining variables from monthly to daily values
            interpolate_monthlyValues(
                litterAsIf100Cover,
                interpAsBase1,
                SW_ModelSim->cum_monthdays,
                SW_ModelSim->days_in_month,
                vegSim[k].litter_daily
            );
            interpolate_monthlyValues(
                vegRunIn[k].lai_conv,
                interpAsBase1,
                SW_ModelSim->cum_monthdays,
                SW_ModelSim->days_in_month,
                vegSim[k].lai_conv_daily
            );
        }
    }


    /* Calculate additional daily vegetation variables */
    for (doy = 1; doy <= MAX_DAYS; doy++) {
        ForEachVegType(k) {
            if (GT(vegRunIn[k].cov.fCover, 0.)) {
                /* vegetation height = 'veg_height_daily' is used for
                 * 'snowdepth_scale'; historically, also for 'vegcov' */
                if (GT(vegIn[k].canopy_height_constant, 0.)) {
                    vegSim[k].veg_height_daily[doy] =
                        vegIn[k].canopy_height_constant;

                } else {
                    vegSim[k].veg_height_daily[doy] = tanfunc(
                        vegSim[k].biomass_daily[doy],
                        vegIn[k].cnpy.xinflec,
                        vegIn[k].cnpy.yinflec,
                        vegIn[k].cnpy.range,
                        vegIn[k].cnpy.slope
                    );
                }

                /* live biomass = 'biolive_daily' is used for
                 * canopy-interception, transpiration, bare-soil evaporation,
                 * and hydraulic redistribution */
                vegSim[k].biolive_daily[doy] = vegSim[k].biomass_daily[doy] *
                                               vegSim[k].pct_live_daily[doy];

                /* dead biomass = 'biodead_daily' is used for
                 * canopy-interception and transpiration */
                vegSim[k].biodead_daily[doy] =
                    vegSim[k].biomass_daily[doy] - vegSim[k].biolive_daily[doy];

                /* live leaf area index = 'lai_live_daily' is used for E-T
                 * partitioning */
                vegSim[k].lai_live_daily[doy] = vegSim[k].biolive_daily[doy] /
                                                vegSim[k].lai_conv_daily[doy];

                /* compound leaf area index = 'bLAI_total_daily' is used for
                 * canopy-interception */
                vegSim[k].bLAI_total_daily[doy] =
                    vegSim[k].lai_live_daily[doy] +
                    vegIn[k].veg_kdead * vegSim[k].biodead_daily[doy] /
                        vegSim[k].lai_conv_daily[doy];

                /* total above-ground biomass = 'total_agb_daily' is used for
                 * bare-soil evaporation */
                if (k == SW_TREENL || k == SW_TREEBL) {
                    vegSim[k].total_agb_daily[doy] =
                        vegSim[k].litter_daily[doy] +
                        vegSim[k].biolive_daily[doy];
                } else {
                    vegSim[k].total_agb_daily[doy] =
                        vegSim[k].litter_daily[doy] +
                        vegSim[k].biomass_daily[doy];
                }

            } else {
                /* No cover -> set all daily vegetation variables to 0 */
                vegSim[k].litter_daily[doy] = 0.;
                vegSim[k].biomass_daily[doy] = 0.;
                vegSim[k].pct_live_daily[doy] = 0.;
                vegSim[k].veg_height_daily[doy] = 0.;
                vegSim[k].lai_conv_daily[doy] = 0.;
                vegSim[k].lai_live_daily[doy] = 0.;
                vegSim[k].bLAI_total_daily[doy] = 0.;
                vegSim[k].biolive_daily[doy] = 0.;
                vegSim[k].biodead_daily[doy] = 0.;
                vegSim[k].total_agb_daily[doy] = 0.;
            }
        }
    }
}

/**
@brief Sum up values across vegetation types

@param[in] x Array of size \ref NVEGTYPES by \ref MAX_LAYERS
@param[in] layerno Current layer which is being worked with
@return Sum across `*x`
*/
double sum_across_vegtypes(double x[][MAX_LAYERS], LyrIndex layerno) {
    unsigned int k;
    double sum = 0.;

    ForEachVegType(k) { sum += x[k][layerno]; }

    return sum;
}

/**
@brief Text output for VegProd.

@param[in] SW_VegProdRunIn Struct of type SW_VEGPROD_RUN_INPUTS that
    holds run-specific input information about vegetation production
@param[in] SW_VegProdIn Struct of type SW_VEGPROD_INPUTS that
    holds static simulation values (cannot change depending on the simulation)
*/
void echo_VegProd(
    SW_VEGPROD_RUN_INPUTS *SW_VegProdRunIn, SW_VEGPROD_INPUTS *SW_VegProdIn
) {
    /* ================================================== */

    int k;

    sw_printf("\n==============================================\n"
              "Vegetation Production Parameters\n");

    sw_printf("Component   Cover   Albedo   HydRed\n");

    ForEachVegType(k) {
        sw_printf(
            "%s   %1.2f   %1.2f   %d\n",
            key2veg[k],
            SW_VegProdRunIn->veg[k].cov.fCover,
            SW_VegProdIn->veg[k].cov.albedo,
            SW_VegProdIn->veg[k].flagHydraulicRedistribution
        );
    }

    sw_printf(
        "BareGround   %1.2f   %1.2f   NA\n",
        SW_VegProdRunIn->bare_cov.fCover,
        SW_VegProdIn->bare_cov.albedo
    );
}

/**
@brief Determine vegetation type of decreasingly ranked the critical SWP

@param[in,out] SW_VegProdIn Struct of type SW_VEGPROD_INPUTS describing surface
    cover conditions in the simulation

@sideeffect Sets `SW_VegProdIn->rank_SWPcrits[]` based on
`SW_VegProdIn->critSoilWater[]`
*/
void get_critical_rank(SW_VEGPROD_INPUTS *SW_VegProdIn) {
    /*----------------------------------------------------------
            Get proper order for rank_SWPcrits
    ----------------------------------------------------------*/
    int i;
    int outerLoop;
    int innerLoop;
    double key;

    // need two temp arrays equal to critSoilWater since we dont want to alter
    // the original at all
    double tempArray[NVEGTYPES];
    double tempArrayUnsorted[NVEGTYPES];

    ForEachVegType(i) {
        tempArray[i] = SW_VegProdIn->critSoilWater[i];
        tempArrayUnsorted[i] = SW_VegProdIn->critSoilWater[i];
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
                SW_VegProdIn->rank_SWPcrits[outerLoop] = innerLoop;
                // set value to something impossible so if a duplicate a
                // different index is picked next
                tempArrayUnsorted[innerLoop] = SW_MISSING;
                break;
            }
        }
    }
    /*printf("%d = %f\n", VegProdIn->rank_SWPcrits[0],
    VegProdIn->critSoilWater[VegProdIn->rank_SWPcrits[0]]); printf("%d =
    %f\n", VegProdIn->rank_SWPcrits[1],
    VegProdIn->critSoilWater[VegProdIn->rank_SWPcrits[1]]); printf("%d =
    %f\n", VegProdIn->rank_SWPcrits[2],
    VegProdIn->critSoilWater[VegProdIn->rank_SWPcrits[2]]); printf("%d =
    %f\n\n", VegProdIn->rank_SWPcrits[3],
    VegProdIn->critSoilWater[VegProdIn->rank_SWPcrits[3]]);*/
    /*----------------------------------------------------------
            End of rank_SWPcrits
    ----------------------------------------------------------*/
}

/**
@brief Wrapper function for estimating natural vegetation. First, climate is
calculated and averaged, then values are estimated

@param[in,out] SW_VegProdRunIn Struct of type SW_VEGPROD_RUN_INPUTS that
    holds run-specific input information about vegetation production
@param[in,out] Weather_hist Array containing all historical data of a site
@param[in] SW_ModelIn Struct of type SW_MODEL_INPUTS holding basic input
    time information about the simulation
@param[in] SW_ModelSim Struct of type SW_MODEL_SIM holding basic intermediate
    time information about the simulation run
@param[in] inNorthHem Bool value specifying if the current site is in the
    northern hemisphere
@param[in] veg_method The requested method to estimate vegetation values,
    see SW_VEGPROD_INPUTS.veg_method
@param[in] LogInfo Holds information on warnings and errors
*/
void estimateVegetationFromClimate(
    SW_VEGPROD_RUN_INPUTS *SW_VegProdRunIn,
    SW_WEATHER_HIST *Weather_hist,
    SW_MODEL_INPUTS *SW_ModelIn,
    SW_MODEL_SIM *SW_ModelSim,
    Bool inNorthHem,
    int veg_method,
    LOG_INFO *LogInfo
) {

    if (veg_method <= VEG_METHOD_FILE) {
        return;
    }

    if (veg_method == VEG_METHOD_LONG_EST) {

        unsigned int numYears = SW_ModelIn->endyr - SW_ModelIn->startyr + 1;
        unsigned int k;
        unsigned int bareGroundIndex = 7;

        SW_CLIMATE_YEARLY climateOutput;
        SW_CLIMATE_CLIM climateAverages;

        // NOTE: 8 = number of types, 5 = (number of types) - grasses

        double coverValues[8] = {
            SW_MISSING,
            SW_MISSING,
            SW_MISSING,
            SW_MISSING,
            0.0,
            SW_MISSING,
            0.0,
            0.0
        };
        double shrubLimit = .2;

        double SumGrassesFraction = SW_MISSING;
        double C4Variables[3];
        double grassOutput[3];
        double RelAbundanceL0[8];
        double RelAbundanceL1[5]; // NVEGTYPES (v1): 4 + 1
        double RelAbundanceL2[7]; // NVEGTYPES (v2): 6 + 1

        Bool fillEmptyWithBareGround = swTRUE;
        Bool warnExtrapolation = swTRUE;
        Bool fixBareGround = swTRUE;

        // Allocate climate structs' memory
        allocateClimateStructs(
            numYears, &climateOutput, &climateAverages, LogInfo
        );
        if (LogInfo->stopRun) {
            // Deallocate climate structs' memory before error
            deallocateClimateStructs(&climateOutput, &climateAverages);
            return; // Exit function prematurely due to error
        }

        calcSiteClimate(
            Weather_hist,
            SW_ModelSim->cum_monthdays,
            SW_ModelSim->days_in_month,
            numYears,
            SW_ModelIn->startyr,
            inNorthHem,
            &climateOutput
        );

        averageClimateAcrossYears(&climateOutput, numYears, &climateAverages);

        if (veg_method == VEG_METHOD_LONG_EST) {

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
                RelAbundanceL2,
                LogInfo
            );

            if (LogInfo->stopRun) {
                // Deallocate climate structs' memory before error
                deallocateClimateStructs(&climateOutput, &climateAverages);
                return; // Exit function prematurely due to error
            }

            ForEachVegType(k) {
                SW_VegProdRunIn->veg[k].cov.fCover = RelAbundanceL2[k];
            }

            SW_VegProdRunIn->bare_cov.fCover = RelAbundanceL0[bareGroundIndex];

            // Deallocate climate structs' memory
            deallocateClimateStructs(&climateOutput, &climateAverages);

        } else {
            LogError(
                LogInfo,
                LOGERROR,
                "Requested 'veg_method = %d' is not implemented.",
                veg_method
            );
        }
    }
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
@param[out] RelAbundanceL1 Array of size five representing the types and order
    used by SOILWAT2 previous to v8.3.0 (vegetation type "v1"):
        -# trees,
        -# shrubs
        -# forbs (here, sum of forbs and succulents)
        -# grasses (here, sum of annual grasses, C3 grasses, C4 grasses)
        -# bare ground
@param[out] RelAbundanceL2 Array of size seven representing the types and order
    used by SOILWAT2 since v8.3.0 (vegetation type "v2"):
        -# treeNL (here, treated as if equivalent to "trees"),
        -# treeBL (here, set to 0),
        -# shrub
        -# forbs (here, sum of forbs and succulents)
        -# grassC3 (here, sum of annual grasses and C3 grasses)
        -# grassC4
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
    double *RelAbundanceL2,
    LOG_INFO *LogInfo
) {

    const int nTypes = 8;
    int winterMonths[3];
    int summerMonths[3];

    // Indices both single value and arrays
    int index;
    int succIndex = 0;
    int forbIndex = 1;
    int C3Index = 2;
    int C4Index = 3;
    int grassAnn = 4;
    int shrubIndex = 5;
    int treeIndex = 6;
    int bareGround = 7;
    int grassEstimSize = 0;
    int overallEstimSize = 0;
    int julyMin = 0;
    int degreeAbove65 = 1;
    int frostFreeDays = 2;
    int estimIndicesNotNA = 0;
    int grassesEstim[3];
    int overallEstim[nTypes];
    int iFixed[nTypes];
    int iFixedSize = 0;
    int tempSwapValue;
    int isetIndices[3] = {grassAnn, treeIndex, bareGround};

    const char *txt_isetIndices[] = {"annual grasses", "trees", "bare ground"};

    // Totals of different areas of variables
    double totalSumGrasses = 0.;
    double inputSumGrasses = 0.;
    double meanMonTemp = 0.;
    double tempDiffJanJul;
    double totalMonPPT = 0.;
    double summerMAP = 0.;
    double winterMAP = 0.;
    double C4Species = SW_MISSING;
    double C3Grassland;
    double C3Shrubland;
    double estimGrassSum = 0;
    double finalVegSum = 0.;
    double estimCoverSum = 0.;
    double tempSumGrasses = 0.;
    double estimCover[nTypes];
    double initialVegSum = 0.;
    double fixedValuesSum = 0;

    Bool fixSumGrasses = (Bool) (!missing(SumGrassesFraction));
    Bool isGrassIndex = swFALSE;
    Bool tempShrubBool;


    // Land cover/vegetation types that are not estimated
    // (trees, annual grasses, and bare-ground):
    // set to 0 if input is `SW_MISSING`
    for (index = 0; index < 3; index++) {
        if (missing(inputValues[isetIndices[index]])) {
            inputValues[isetIndices[index]] = 0.;

            LogError(
                LogInfo,
                LOGWARN,
                "No equation for requested cover type '%s': cover set to 0.",
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

            // Check consistency between monthly and annual precipitation
            totalMonPPT = 0;
            for (index = 0; index < MAX_MONTHS; index++) {
                totalMonPPT += PPTMon_cm[index];
            }

            if (totalMonPPT < 0.95 * PPT_cm || totalMonPPT > 1.05 * PPT_cm) {
                LogError(
                    LogInfo,
                    LOGERROR,
                    "'estimate_PotNatVeg_composition': "
                    "annual and monthly precipitation disagree beyond "
                    "a 5%% tolerance (annual = %f, sum(monthly) = %f)",
                    PPT_cm,
                    totalMonPPT
                );
                return; // Exit function prematurely due to error
            }

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

            // Proportion of summer and winter precip to total precipitation
            summerMAP /= totalMonPPT;
            winterMAP /= totalMonPPT;


            // Check consistency between monthly and annual temperature
            meanMonTemp = mean(meanTempMon_C, MAX_MONTHS);

            if (meanMonTemp < meanTemp_C - 0.5 ||
                meanMonTemp > meanTemp_C + 0.5) {
                LogError(
                    LogInfo,
                    LOGERROR,
                    "'estimate_PotNatVeg_composition': "
                    "annual and monthly temperature disagree beyond "
                    "a 0.5 degC tolerance (annual = %f, mean(monthly) = %f)",
                    meanTemp_C,
                    meanMonTemp
                );
                return; // Exit function prematurely due to error
            }


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

    grassOutput[0] = RelAbundanceL0[C3Index];
    grassOutput[1] = RelAbundanceL0[C4Index];
    grassOutput[2] = RelAbundanceL0[grassAnn];

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

    /* RelAbundanceL1 Array of size five representing the types and order
       used by SOILWAT2 previous to v8.3.0 (vegetation type "v1") */
    RelAbundanceL1[0] = RelAbundanceL0[treeIndex];
    RelAbundanceL1[1] = RelAbundanceL0[shrubIndex];
    RelAbundanceL1[2] = RelAbundanceL0[forbIndex] + RelAbundanceL0[succIndex];
    RelAbundanceL1[3] = RelAbundanceL0[C3Index] + RelAbundanceL0[C4Index] +
                        RelAbundanceL0[grassAnn];
    RelAbundanceL1[4] = RelAbundanceL0[bareGround];

    /* RelAbundanceL2 Array of size seven representing the types and order
    used by SOILWAT2 since v8.3.0 (vegetation type "v2") */
    /* treeNL */
    RelAbundanceL2[0] = RelAbundanceL0[treeIndex];
    /* treeBL */
    RelAbundanceL2[1] = 0.;
    /* shrub */
    RelAbundanceL2[2] = RelAbundanceL0[shrubIndex];
    /* forbs (here, sum of forbs and succulents) */
    RelAbundanceL2[3] = RelAbundanceL0[forbIndex] + RelAbundanceL0[succIndex];
    /* grassC3 (here, sum of annual grasses and C3 grasses) */
    RelAbundanceL2[4] = RelAbundanceL0[C3Index] + RelAbundanceL0[grassAnn];
    /* grassC4 */
    RelAbundanceL2[5] = RelAbundanceL0[C4Index];
    /* bare ground */
    RelAbundanceL2[6] = RelAbundanceL0[bareGround];
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

    int index;
    int finalArrayIndex = 0;
    int nTypes = 8;
    int tempSize = arrayOneSize + arrayTwoSize + finalArrayIndex;
    int tempIndex = 0;
    int *tempArray;
    int *tempArraySeen;

    tempArray =
        (int *) Mem_Malloc(sizeof(int) * tempSize, "uniqueIndices", LogInfo);
    if (LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

    tempArraySeen =
        (int *) Mem_Malloc(sizeof(int) * nTypes, "uniqueIndices", LogInfo);
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
