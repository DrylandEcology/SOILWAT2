/********************************************************/
/********************************************************/
/*  Source file: SW_VegProd.h
 Type: header
 Application: SOILWAT - soilwater dynamics simulator
 Purpose: Support routines and definitions for
 vegetation production parameter information.
 History:

 (8/28/01) -- INITIAL CODING - cwb

 11/16/2010	(drs) added LAIforest, biofoliage_for, lai_conv_for,
 TypeGrassOrShrub, TypeForest to SW_VEGPROD

 02/22/2011	(drs) added variable litter_for to SW_VEGPROD

 08/22/2011	(drs) added variable veg_height [MAX_MONTHS] to SW_VEGPROD

 09/08/2011	(drs) struct SW_VEGPROD now contains an element each for the
 types grasses, shrubs, and trees, as well as a variable describing the relative
 composition of vegetation by grasses, shrubs, and trees (changed from Bool to
 RealD types) the earlier SW_VEGPROD variables describing the vegetation is now
 contained in an additional struct VegType

 09/08/2011	(drs) added variables tanfunc_t tr_shade_effects, and RealD
 shade_scale and shade_deadmax to struct VegType (they were previously hidden in
 code in SW_Flow_lib.h)

 09/08/2011	(drs) moved all hydraulic redistribution variables from
 SW_Site.h to struct VegType

 09/08/2011	(drs) added variables RealD veg_intPPT_a, veg_intPPT_b,
 veg_intPPT_c, veg_intPPT_d to struct VegType for parameters in intercepted rain
 = (a + b*veg) + (c+d*veg) * ppt; Grasses+Shrubs: veg=vegcov, Trees: veg=LAI,
 which were previously hidden in code in SW_Flow_lib.c

 09/09/2011	(drs) added variable RealD EsTpartitioning_param to struct
 VegType as parameter for partitioning of bare-soil evaporation and
 transpiration as in Es = exp(-param*LAI)

 09/09/2011	(drs) added variable RealD Es_param_limit to struct VegType as
 parameter for for scaling and limiting bare soil evaporation rate: if totagb >
 Es_param_limit then no bare-soil evaporation

 09/13/2011	(drs) added variables RealD litt_intPPT_a, litt_intPPT_b,
 litt_intPPT_c, litt_intPPT_d to struct VegType for parameters in litter
 intercepted rain = (a + b*litter) + (c+d*litter) * ppt, which were previously
 hidden in code in SW_Flow_lib.c

 09/13/2011	(drs)	added variable RealD canopy_height_constant to struct
 VegType; if > 0 then constant canopy height [cm] and overriding
 cnpy-tangens=f(biomass)

 09/15/2011	(drs)	added variable RealD albedo to struct VegType
 (previously in SW_Site.h struct SW_SITE)

 09/26/2011	(drs)	added a daily variable for each monthly input in struct
 VegType: RealD litter_daily, biomass_daily, pct_live_daily, veg_height_daily,
 lai_conv_daily, lai_conv_daily, lai_live_daily, pct_cover_daily, vegcov_daily,
 biolive_daily, biodead_daily, total_agb_daily each of [MAX_DAYS]

 09/26/2011	(dsr)	removed monthly variables RealD veg_height, lai_live,
 pct_cover, vegcov, biolive, biodead, total_agb each [MAX_MONTHS] from struct
 VegType because replaced with daily records

 02/04/2012	(drs)	added variable RealD SWPcrit to struct VegType: critical
 soil water potential below which vegetation cannot sustain transpiration

 01/29/2013	(clk) added variable RealD bare_cov.fCover to now allow for bare
 ground as a part of the total vegetation.

 01/31/2013	(clk)	added varilabe RealD bare_cov.albedo instead of creating
 a bare_cov VegType, because only need albedo and not the other data members

 04/09/2013	(clk) changed the variable name swp50 to swpMatric50. Therefore
 also updated the use of swp50 to swpMatric50 in SW_VegProd.c and SW_Flow.c.

 07/09/2013	(clk)	add the variables forb and forb.cov.fCover to SW_VEGPROD
 */
/********************************************************/
/********************************************************/

#ifndef SW_VEGPROD_H
#define SW_VEGPROD_H

#include "include/generic.h"        // for Bool, RealD
#include "include/SW_datastructs.h" // for SW_VEGPROD, SW_MODEL, SW_WEATHER_HIST
#include "include/SW_Defines.h"     // for LyrIndex, NVEGTYPES, MAX_LAYERS,

#ifdef __cplusplus
extern "C" {
#endif

/** An integer representing the index of the biomass multipliers in the \
  VegType#co2_multipliers 2D array. */
#define BIO_INDEX 0

/** An integer representing the index of the WUE multipliers in the \
  VegType#co2_multipliers 2D array. */
#define WUE_INDEX 1

/* =================================================== */
/*            Externed Global Variables                */
/* --------------------------------------------------- */
extern const char *const key2veg[NVEGTYPES];


/* =================================================== */
/*             Global Function Declarations            */
/* --------------------------------------------------- */
void SW_VPD_read(SW_VEGPROD *SW_VegProd, char *InFiles[], LOG_INFO *LogInfo);

void SW_VPD_new_year(SW_VEGPROD *SW_VegProd, SW_MODEL *SW_Model);

void SW_VPD_fix_cover(SW_VEGPROD *SW_VegProd, LOG_INFO *LogInfo);

void SW_VPD_init_ptrs(SW_VEGPROD *SW_VegProd);

void SW_VPD_construct(SW_VEGPROD *SW_VegProd);

void SW_VPD_alloc_outptrs(SW_VEGPROD *SW_VegProd, LOG_INFO *LogInfo);

void estimateVegetationFromClimate(
    SW_VEGPROD *SW_VegProd,
    SW_WEATHER_HIST **Weather_hist,
    SW_MODEL *SW_Model,
    LOG_INFO *LogInfo
);

void estimatePotNatVegComposition(
    double meanTemp_C,
    double PPT_cm,
    double meanTempMon_C[],
    double PPTMon_cm[],
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
);

double cutZeroInf(double testValue);

void uniqueIndices(
    int arrayOne[],
    int arrayTwo[],
    int arrayOneSize,
    int arrayTwoSize,
    int *finalIndexArray,
    int *finalIndexArraySize,
    LOG_INFO *LogInfo
);

void SW_VPD_init_run(
    SW_VEGPROD *SW_VegProd,
    SW_WEATHER *SW_Weather,
    SW_MODEL *SW_Model,
    LOG_INFO *LogInfo
);

void SW_VPD_deconstruct(SW_VEGPROD *SW_VegProd);

void apply_biomassCO2effect(
    double *new_biomass, double *biomass, double multiplier
);

RealD sum_across_vegtypes(RealD x[][MAX_LAYERS], LyrIndex layerno);

void echo_VegProd(VegType VegProd_veg[], CoverType VegProd_bare_cov);

void get_critical_rank(SW_VEGPROD *SW_VegProd);

#ifdef __cplusplus
}
#endif

#endif
