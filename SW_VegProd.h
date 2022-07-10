/********************************************************/
/********************************************************/
/*  Source file: SW_VegProd.h
 Type: header
 Application: SOILWAT - soilwater dynamics simulator
 Purpose: Support routines and definitions for
 vegetation production parameter information.
 History:
 (8/28/01) -- INITIAL CODING - cwb
 11/16/2010	(drs) added LAIforest, biofoliage_for, lai_conv_for, TypeGrassOrShrub, TypeForest to SW_VEGPROD
 02/22/2011	(drs) added variable litter_for to SW_VEGPROD
 08/22/2011	(drs) added variable veg_height [MAX_MONTHS] to SW_VEGPROD
 09/08/2011	(drs) struct SW_VEGPROD now contains an element each for the types grasses, shrubs, and trees, as well as a variable describing the relative composition of vegetation by grasses, shrubs, and trees (changed from Bool to RealD types)
 the earlier SW_VEGPROD variables describing the vegetation is now contained in an additional struct VegType
 09/08/2011	(drs) added variables tanfunc_t tr_shade_effects, and RealD shade_scale and shade_deadmax to struct VegType (they were previously hidden in code in SW_Flow_lib.h)
 09/08/2011	(drs) moved all hydraulic redistribution variables from SW_Site.h to struct VegType
 09/08/2011	(drs) added variables RealD veg_intPPT_a, veg_intPPT_b, veg_intPPT_c, veg_intPPT_d to struct VegType for parameters in intercepted rain = (a + b*veg) + (c+d*veg) * ppt; Grasses+Shrubs: veg=vegcov, Trees: veg=LAI, which were previously hidden in code in SW_Flow_lib.c
 09/09/2011	(drs) added variable RealD EsTpartitioning_param to struct VegType as parameter for partitioning of bare-soil evaporation and transpiration as in Es = exp(-param*LAI)
 09/09/2011	(drs) added variable RealD Es_param_limit to struct VegType as parameter for for scaling and limiting bare soil evaporation rate: if totagb > Es_param_limit then no bare-soil evaporation
 09/13/2011	(drs) added variables RealD litt_intPPT_a, litt_intPPT_b, litt_intPPT_c, litt_intPPT_d to struct VegType for parameters in litter intercepted rain = (a + b*litter) + (c+d*litter) * ppt, which were previously hidden in code in SW_Flow_lib.c
 09/13/2011	(drs)	added variable RealD canopy_height_constant to struct VegType; if > 0 then constant canopy height [cm] and overriding cnpy-tangens=f(biomass)
 09/15/2011	(drs)	added variable RealD albedo to struct VegType (previously in SW_Site.h struct SW_SITE)
 09/26/2011	(drs)	added a daily variable for each monthly input in struct VegType: RealD litter_daily, biomass_daily, pct_live_daily, veg_height_daily, lai_conv_daily, lai_conv_daily, lai_live_daily, pct_cover_daily, vegcov_daily, biolive_daily, biodead_daily, total_agb_daily each of [MAX_DAYS]
 09/26/2011	(dsr)	removed monthly variables RealD veg_height, lai_live, pct_cover, vegcov, biolive, biodead, total_agb each [MAX_MONTHS] from struct VegType because replaced with daily records
 02/04/2012	(drs)	added variable RealD SWPcrit to struct VegType: critical soil water potential below which vegetation cannot sustain transpiration
 01/29/2013	(clk) added variable RealD bare_cov.fCover to now allow for bare ground as a part of the total vegetation.
 01/31/2013	(clk)	added varilabe RealD bare_cov.albedo instead of creating a bare_cov VegType, because only need albedo and not the other data members
 04/09/2013	(clk) changed the variable name swp50 to swpMatric50. Therefore also updated the use of swp50 to swpMatric50 in SW_VegProd.c and SW_Flow.c.
 07/09/2013	(clk)	add the variables forb and forb.cov.fCover to SW_VEGPROD
 */
/********************************************************/
/********************************************************/

#ifndef SW_VEGPROD_H
#define SW_VEGPROD_H

#include "SW_Defines.h"    /* for tanfunc_t*/
#include "Times.h" 				// for MAX_MONTHS

#ifdef __cplusplus
extern "C" {
#endif


#define BIO_INDEX 0        /**< An integer representing the index of the biomass multipliers in the VegType#co2_multipliers 2D array. */
#define WUE_INDEX 1        /**< An integer representing the index of the WUE multipliers in the VegType#co2_multipliers 2D array. */


/** Data type that describes cover attributes of a surface type */
typedef struct {
  RealD
    /** The cover contribution to the total plot [0-1];
      user input from file `Input/veg.in` */
    fCover,
    /** The surface albedo [0-1];
      user input from file `Input/veg.in` */
    albedo;
} CoverType;


/** Data type that describes a vegetation type: currently, one of
  \ref NVEGTYPES available types:
  \ref SW_TREES, \ref SW_SHRUB, \ref SW_FORBS, and \ref SW_GRASS */
typedef struct {
  /** Surface cover attributes of the vegetation type */
  CoverType cov;

  tanfunc_t
    /** Parameters to calculate canopy height based on biomass;
      user input from file `Input/veg.in` */
    cnpy;
  /** Constant canopy height: if > 0 then constant canopy height [cm] and
    overriding cnpy-tangens = f(biomass);
    user input from file `Input/veg.in` */
  RealD canopy_height_constant;

  tanfunc_t
    /** Shading effect on transpiration based on live and dead biomass;
      user input from file `Input/veg.in` */
    tr_shade_effects;

  RealD
    /** Parameter of live and dead biomass shading effects;
      user input from file `Input/veg.in` */
    shade_scale,
    /** Maximal dead biomass for shading effects;
      user input from file `Input/veg.in` */
    shade_deadmax;

  RealD
    /** Monthly litter amount [g / m2] as if this vegetation type covers 100%
      of the simulated surface;
      user input from file `Input/veg.in` */
    litter[MAX_MONTHS],
    /** Monthly aboveground biomass [g / m2] as if this vegetation type
      covers 100% of the simulated surface;
      user input from file `Input/veg.in` */
    biomass[MAX_MONTHS],
    /** Monthly aboveground biomass after CO2 effects */
    CO2_biomass[MAX_MONTHS],
    /** Monthly live biomass in percent of aboveground biomass;
      user input from file `Input/veg.in` */
    pct_live[MAX_MONTHS],
    /** Monthly live biomass in percent of aboveground biomass after
      CO2 effects */
    CO2_pct_live[MAX_MONTHS],
    /** Parameter to translate biomass to LAI = 1 [g / m2];
      user input from file `Input/veg.in` */
    lai_conv[MAX_MONTHS];

  RealD
    /** Daily litter amount [g / m2] */
    litter_daily[MAX_DAYS + 1],
    /** Daily aboveground biomass [g / m2] */
    biomass_daily[MAX_DAYS + 1],
    /** Daily live biomass in percent of aboveground biomass */
    pct_live_daily[MAX_DAYS + 1],
    /** Daily height of vegetation canopy [cm] */
    veg_height_daily[MAX_DAYS + 1],
    /** Daily parameter value to translate biomass to LAI = 1 [g / m2] */
    lai_conv_daily[MAX_DAYS + 1],
    /** Daily LAI of live biomass [m2 / m2]*/
    lai_live_daily[MAX_DAYS + 1],
    /** Daily total "compound" leaf area index [m2 / m2]*/
    bLAI_total_daily[MAX_DAYS + 1],
    /** Daily live biomass [g / m2] */
    biolive_daily[MAX_DAYS + 1],
    /** Daily dead standing biomass [g / m2] */
    biodead_daily[MAX_DAYS + 1],
    /** Daily sum of aboveground biomass & litter [g / m2] */
    total_agb_daily[MAX_DAYS + 1];

  Bool
    /** Flag for hydraulic redistribution/lift:
      1, simulate; 0, don't simulate;
      user input from file `Input/veg.in` */
    flagHydraulicRedistribution;

  RealD
    /** Parameter for hydraulic redistribution: maximum radial soil-root
      conductance of the entire active root system for water
      [cm / (-bar * day)];
      user input from file `Input/veg.in` */
    maxCondroot,
    /** Parameter for hydraulic redistribution: soil water potential [-bar]
      where conductance is reduced by 50%;
      user input from file `Input/veg.in` */
    swpMatric50,
    /** Parameter for hydraulic redistribution: shape parameter for the
      empirical relationship from van Genuchten to model relative soil-root
      conductance for water;
      user input from file `Input/veg.in` */
    shapeCond;

  RealD
    /** Critical soil water potential below which vegetation cannot sustain
      transpiration [-bar];
      user input from file `Input/veg.in` */
    SWPcrit;

  RealD
    /** Parameter for vegetation interception;
      user input from file `Input/veg.in` */
    veg_kSmax,
    /** Parameter for vegetation interception parameter;
      user input from file `Input/veg.in` */
    veg_kdead,
    /** Parameter for litter interception;
      user input from file `Input/veg.in` */
    lit_kSmax;

  RealD
    /** Parameter for partitioning potential rates of bare-soil evaporation
      and transpiration;
      user input from file `Input/veg.in` */
    EsTpartitioning_param,
    /** Parameter for scaling and limiting bare soil evaporation rate;
      user input from file `Input/veg.in` */
    Es_param_limit;

  RealD
    /** Parameter for CO2-effects on biomass;
      user input from file `Input/veg.in` */
    co2_bio_coeff1,
    /** Parameter for CO2-effects on biomass;
      user input from file `Input/veg.in` */
    co2_bio_coeff2,
    /** Parameter for CO2-effects on water-use-efficiency;
      user input from file `Input/veg.in` */
    co2_wue_coeff1,
    /** Parameter for CO2-effects on water-use-efficiency;
      user input from file `Input/veg.in` */
    co2_wue_coeff2;

  RealD
    /** Calculated multipliers for CO2-effects:
      - column \ref BIO_INDEX holds biomass multipliers
      - column \ref WUE_INDEX holds water-use-efficiency multipliers
      - rows represent years */
    co2_multipliers[2][MAX_NYEAR];

} VegType;


typedef struct {
  // biomass [g/m2] per vegetation type as observed in total vegetation
  // (reduced from 100% cover per vegtype (inputs) to actual cover (simulated))
  RealD biomass_inveg, biolive_inveg, litter_inveg;
} VegTypeOut;


typedef struct {
  // biomass [g/m2] per vegetation type as observed in total vegetation
  VegTypeOut veg[NVEGTYPES];
  // biomass [g/m2] of total vegetation
  RealD biomass_total, biolive_total, litter_total, LAI;
} SW_VEGPROD_OUTPUTS;


/** Data type to describe the surface cover of a SOILWAT2 simulation run */
typedef struct {
  /** Data for each vegetation type */
  VegType veg[NVEGTYPES];
  /** Bare-ground cover of plot that is not occupied by vegetation;
      user input from file `Input/veg.in` */
  CoverType bare_cov;

  Bool
    /** Flag that determines whether vegetation-type specific soil water
      availability should be calculated;
      user input from file `Input/outsetup.in` */
    use_SWA,
    veg_method;

  RealD
    // storing values in same order as defined in STEPWAT2/rgroup.in (0=tree, 1=shrub, 2=grass, 3=forb)
    critSoilWater[NVEGTYPES];

  int
    // `rank_SWPcrits[k]` hold the vegetation type at rank `k` of decreasingly
    // sorted critical SWP values
    rank_SWPcrits[NVEGTYPES];

  SW_VEGPROD_OUTPUTS
    /** output accumulator: summed values for each output time period */
    *p_accu[SW_OUTNPERIODS],
    /** output aggregator: mean or sum for each output time periods */
    *p_oagg[SW_OUTNPERIODS];
} SW_VEGPROD;



/* =================================================== */
/*            Externed Global Variables                */
/* --------------------------------------------------- */
extern SW_VEGPROD SW_VegProd;
extern char const *key2veg[NVEGTYPES];


/* =================================================== */
/*             Global Function Declarations            */
/* --------------------------------------------------- */
void SW_VPD_read(void);
void SW_VPD_new_year(void);
void SW_VPD_fix_cover(void);
void SW_VPD_construct(void);
void estimateVegetationFromClimate(int startYear, int endYear);
void SW_VPD_init_run(void);
void SW_VPD_deconstruct(void);
void apply_biomassCO2effect(double* new_biomass, double *biomass, double multiplier);
RealD sum_across_vegtypes(RealD *x);
void _echo_VegProd(void);
void get_critical_rank(void);


#ifdef __cplusplus
}
#endif

#endif
