/********************************************************/
/********************************************************/
/*  Source file: SW_datastructs.h
  Type: header
  Purpose: Contain all structs SOILWAT2 uses (SW_SOILWAT, SW_WEATHER, etc.)
	to prevent circular dependencies when requesting a custom datatype,
	which used to be contained in respective header files.
 */
/********************************************************/
/********************************************************/

#ifndef DATATYPES_H
#define DATATYPES_H

#include "include/SW_Defines.h"
#include "external/pcg/pcg_basic.h"

// Array-based output:
#if defined(RSOILWAT) || defined(STEPWAT)
#define SW_OUTARRAY
#endif

// Text-based output:
#if defined(SOILWAT) || defined(STEPWAT)
#define SW_OUTTEXT
#endif

#define SW_NFILES 23 // For `InFiles`

/* =================================================== */
/*                   Carbon structs                    */
/* --------------------------------------------------- */

/**
 * @brief The main structure holding all CO2-related data.
 */
typedef struct {
  int
    use_wue_mult,                      /**< A boolean integer indicating if WUE multipliers should be calculated. */
    use_bio_mult;                      /**< A boolean integer indicating if biomass multipliers should be calculated. */

  char
    scenario[64];                      /**< A 64-char array holding the scenario name for which we are extracting CO2 data from the carbon.in file. */

  double
    ppm[MAX_NYEAR];                 /**< A 1D array holding atmospheric CO2 concentration values (units ppm) that are indexed by calendar year.
										Is typically only populated for the years that are being simulated. `ppm[index]` is the CO2 value for the calendar year `index + 1` */

} SW_CARBON;

/* =================================================== */
/*                  Flowlib structs                    */
/* --------------------------------------------------- */

// this structure is for keeping track of the variables used in the soil_temperature function (mainly the regressions)
typedef struct {

	double depths[MAX_LAYERS],  //soil layer depths of SoilWat soil
	       depthsR[MAX_ST_RGR],//evenly spaced depths of soil temperature layer profile
		   	 fcR[MAX_ST_RGR],//field capacity of soil temperature layer profile, i.e., at `depthsR[]`
		   	 wpR[MAX_ST_RGR], //wilting point of soil temperature layer profile, i.e., at `depthsR[]`
		   	 bDensityR[MAX_ST_RGR],//bulk density of the whole soil of soil temperature layer profile, i.e., at `depthsR[]`
		   	 oldsFusionPool_actual[MAX_LAYERS],
             oldavgLyrTempR[MAX_ST_RGR];//yesterdays soil temperature of soil temperature layer profile, i.e., at `depthsR[]`; Note: index 0 is surface temperature

	double tlyrs_by_slyrs[MAX_ST_RGR][MAX_LAYERS + 1]; // array of soil depth correspondance between soil profile layers and soil temperature layers; last column has negative values and indicates use of deepest soil layer values copied for deeper soil temperature layers

	Bool soil_temp_init;  // simply keeps track of whether or not the values for the soil_temperature function have been initialized.
	Bool do_once_at_soiltempError;
	Bool fusion_pool_init;  // simply keeps track of whether or not the values for the soil fusion (thawing/freezing) section of the soil_temperature function have been initialized.

	double delta_time; // last successful time step in seconds; start out with 1 day

	/*unsigned int x1BoundsR[MAX_ST_RGR],
	             x2BoundsR[MAX_ST_RGR],
				 x1Bounds[MAX_LAYERS],
				 x2Bounds[MAX_LAYERS];*/
} ST_RGR_VALUES;

/* =================================================== */
/*                FlowlibPET struct                    */
/* --------------------------------------------------- */
typedef struct {
	double
		memoized_G_o[MAX_DAYS][TWO_DAYS],
		msun_angles[MAX_DAYS][7],
		memoized_int_cos_theta[MAX_DAYS][TWO_DAYS],
		memoized_int_sin_beta[MAX_DAYS][TWO_DAYS];
} SW_ATMD;

/* =================================================== */
/*                    Model structs                    */
/* --------------------------------------------------- */

typedef struct {
	TimeInt /* controlling dates for model run */
	startyr, /* beginning year for model run */
	endyr, /* ending year for model run */
	startstart, /* startday in start year */
	endend, /* end day in end year */
	daymid, /* mid year depends on hemisphere */
	/* current year dates */
	firstdoy, /* start day for this year */
	lastdoy, /* 366 if leapyear or endend if endyr */
	doy, week, month, year, simyear, /* current model time */
	_prevweek, /* check for new week */
  	_prevmonth, /* check for new month */
  	_prevyear; /* check for new year */
	/* however, week and month are base0 because they
	 * are used as array indices, so take care.
	 * doy and year are base1. */
	/* simyear = year + addtl_yr */

	TimeInt
		days_in_month[MAX_MONTHS], /* number of days per month for "current" year */
		cum_monthdays[MAX_MONTHS]; /* monthly cumulative number of days for "current" year */

  int addtl_yr; /**< An integer representing how many years in the future we are simulating. Currently, only used to support rSFSW2 functionality where scenario runs are based on an 'ambient' run plus number of years in the future*/

	/* first day of new week/month is checked for
	 * printing and summing weekly/monthly values */
	Bool newperiod[SW_OUTNPERIODS];
	Bool isnorth;

	#ifdef STEPWAT
	/* Variables from GlobalType (STEPWAT2) used in SOILWAT2 */
	IntUS runModelIterations, runModelYears;
	#endif

} SW_MODEL;

/* =================================================== */
/*                 Output text structs                 */
/* --------------------------------------------------- */

typedef struct {
	Bool make_soil[SW_OUTNPERIODS], make_regular[SW_OUTNPERIODS];

	#ifdef STEPWAT
	// average/sd across iteration/repetitions
	FILE *fp_reg_agg[SW_OUTNPERIODS];
	char buf_reg_agg[SW_OUTNPERIODS][OUTSTRLEN];
	// output file for variables with values for each soil layer
	FILE *fp_soil_agg[SW_OUTNPERIODS];
	char buf_soil_agg[SW_OUTNPERIODS][MAX_LAYERS * OUTSTRLEN];
	#endif

	// if SOILWAT: "regular" output file
	// if STEPWAT: "regular" output file; new file for each iteration/repetition
	FILE *fp_reg[SW_OUTNPERIODS];
	char buf_reg[SW_OUTNPERIODS][OUTSTRLEN];
	// if SOILWAT: output file for variables with values for each soil layer
	// if STEPWAT: new file for each iteration/repetition of STEPWAT
	FILE *fp_soil[SW_OUTNPERIODS];
	char buf_soil[SW_OUTNPERIODS][MAX_LAYERS * OUTSTRLEN];

} SW_FILE_STATUS;

/* =================================================== */
/*                     Site structs                    */
/* --------------------------------------------------- */


typedef struct {

	Bool reset_yr, /* 1: reset values at start of each year */
		deepdrain, /* 1: allow drainage into deepest layer  */
		use_soil_temp; /* whether or not to do soil_temperature calculations */

	unsigned int type_soilDensityInput; /* Encodes whether `soilDensityInput` represent matric density (type = SW_MATRIC = 0) or bulk density (type = SW_BULK = 1) */

	LyrIndex n_layers, /* total number of soil layers */
		n_transp_rgn, /* soil layers are grouped into n transp. regions */
		n_evap_lyrs, /* number of layers in which evap is possible */
		n_transp_lyrs[NVEGTYPES], /* layer index of deepest transp. region       */
		deep_lyr; /* index of deep drainage layer if deepdrain, 0 otherwise */

	RealD
		slow_drain_coeff, /* low soil water drainage coefficient   */
		pet_scale,	/* changes relative effect of PET calculation */
		longitude,	/* longitude of the site (radians)        */
		latitude,	/* latitude of the site (radians)        */
		altitude,	/* altitude a.s.l (m) of the site */
		slope,		/* slope of the site (radians): between 0 (horizontal) and pi / 2 (vertical) */
		aspect,		/* aspect of the site (radians): A value of \ref SW_MISSING indicates no data, ie., treat it as if slope = 0; South facing slope: aspect = 0, East = -pi / 2, West = pi / 2, North = ±pi */
					/* SWAT2K model parameters : Neitsch S, Arnold J, Kiniry J, Williams J. 2005. Soil and water assessment tool (SWAT) theoretical documentation. version 2005. Blackland Research Center, Texas Agricultural Experiment Station: Temple, TX. */
		TminAccu2,	/* Avg. air temp below which ppt is snow ( C) */
		TmaxCrit,	/* Snow temperature at which snow melt starts ( C) */
		lambdasnow,	/* Relative contribution of avg. air temperature to todays snow temperture vs. yesterday's snow temperature (0-1) */
		RmeltMin,	/* Minimum snow melt rate on winter solstice (cm/day/C) */
		RmeltMax,	/* Maximum snow melt rate on summer solstice (cm/day/C) */
		t1Param1,	/* Soil temperature constants */
		t1Param2,	/* t1Params are the parameters for the avg daily temperature at the top of the soil (T1) equation */
		t1Param3,
		csParam1,	/* csParams are the parameters for the soil thermal conductivity (cs) equation */
		csParam2,
		shParam,	/* shParam is the parameter for the specific heat capacity equation */
		bmLimiter,	/* bmLimiter is the biomass limiter constant, for use in the T1 equation */
		Tsoil_constant, 	/* soil temperature at a depth where soil temperature is (mostly) constant in time; for instance, approximated as the mean air temperature */
		stDeltaX,		/* for the soil_temperature function, deltaX is the distance between profile points (default: 15) */
		stMaxDepth,		/* for the soil_temperature function, the maxDepth of the interpolation function */
		percentRunoff,	/* the percentage of surface water lost daily */
		percentRunon;	/* the percentage of water that is added to surface gained daily */

	unsigned int stNRGR; /* number of interpolations, for the soil_temperature function */
	/* params for tanfunc rate calculations for evap and transp. */
	/* tanfunc() creates a logistic-type graph if shift is positive,
	 * the graph has a negative slope, if shift is 0, slope is positive.
	 */
	tanfunc_t evap, transp;

	/* Soil water retention curve (SWRC), see `SW_LAYER_INFO` */
	unsigned int
		site_swrc_type,
		site_ptf_type;

	char
		site_swrc_name[64],
		site_ptf_name[64];

	Bool site_has_swrcp; /**< Are `swrcp` already (TRUE) or not yet estimated (FALSE)? */

	/* transpiration regions  shallow, moderately shallow,  */
	/* deep and very deep. units are in layer numbers. */
	LyrIndex _TranspRgnBounds[MAX_TRANSP_REGIONS];
	RealD
		_SWCInitVal, /* initialization value for swc */
		_SWCWetVal, /* value for a "wet" day,       */
		_SWCMinVal; /* lower bound on swc.          */

	/* bulk = relating to the whole soil, i.e., matric + rock/gravel/coarse fragments */
	/* matric = relating to the < 2 mm fraction of the soil, i.e., sand, clay, and silt */

	RealD
		/* Inputs */
		width[MAX_LAYERS], /* width of the soil layer (cm) */
		soilDensityInput[MAX_LAYERS], /* soil density [g / cm3]: either of the matric component or bulk soil */
		evap_coeff[MAX_LAYERS], /* prop. of total soil evap from this layer */
		transp_coeff[NVEGTYPES][MAX_LAYERS], /* prop. of total transp from this layer    */
		fractionVolBulk_gravel[MAX_LAYERS], /* gravel content (> 2 mm) as volume-fraction of bulk soil (g/cm3) */
		fractionWeightMatric_sand[MAX_LAYERS], /* sand content (< 2 mm & > . mm) as weight-fraction of matric soil (g/g) */
		fractionWeightMatric_clay[MAX_LAYERS], /* clay content (< . mm & > . mm) as weight-fraction of matric soil (g/g) */
		impermeability[MAX_LAYERS], /* fraction of how impermeable a layer is (0=permeable, 1=impermeable)    */
		avgLyrTempInit[MAX_LAYERS], /* initial soil temperature for each soil layer */

		/* Derived soil characteristics */
		soilMatric_density[MAX_LAYERS], /* matric soil density of the < 2 mm fraction, i.e., gravel component excluded, (g/cm3) */
		soilBulk_density[MAX_LAYERS], /* bulk soil density of the whole soil, i.e., including rock/gravel component, (g/cm3) */
		swcBulk_fieldcap[MAX_LAYERS], /* Soil water content (SWC) corresponding to field capacity (SWP = -0.033 MPa) [cm] */
		swcBulk_wiltpt[MAX_LAYERS], /* SWC corresponding to wilting point (SWP = -1.5 MPa) [cm] */
		swcBulk_halfwiltpt[MAX_LAYERS], /* Adjusted half-wilting point used as SWC limit for bare-soil evaporation */
		swcBulk_min[MAX_LAYERS], /* Minimal SWC [cm] */
		swcBulk_wet[MAX_LAYERS], /* SWC considered "wet" [cm] */
		swcBulk_init[MAX_LAYERS], /* Initial SWC for first day of simulation [cm] */
		swcBulk_atSWPcrit[NVEGTYPES][MAX_LAYERS], /* SWC corresponding to critical SWP for transpiration */

		/* Saxton et al. 2006 */
		swcBulk_saturated[MAX_LAYERS]; /* saturated bulk SWC [cm] */
		// currently, not used;
		//Saxton2006_K_sat_matric, /* saturated matric conductivity [cm / day] */
		//Saxton2006_K_sat_bulk, /* saturated bulk conductivity [cm / day] */
		//Saxton2006_fK_gravel, /* gravel-correction factor for conductivity [1] */
		//Saxton2006_lambda; /* Slope of logarithmic tension-moisture curve */


	/* Soil water retention curve (SWRC) */
	unsigned int
		swrc_type[MAX_LAYERS], /**< Type of SWRC (see #swrc2str) */
		ptf_type[MAX_LAYERS]; /**< Type of PTF (see #ptf2str) */

	/*
		Note: We loop over SWRC_PARAM_NMAX for every soil layer in
			 `swrcp` but we need to loop over soil layers for every
			 vegetation type in `my_transp_rng`
	*/
	RealD swrcp[MAX_LAYERS][SWRC_PARAM_NMAX]; /**< Parameters of SWRC: parameter interpretation varies with selected SWRC, see `SWRC_check_parameters()` */
	LyrIndex my_transp_rgn[NVEGTYPES][MAX_LAYERS]; /* which transp zones from Site am I in? */

} SW_SITE;

/* =================================================== */
/*                    VegProd structs                  */
/* --------------------------------------------------- */

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
    /** Monthly live biomass in percent of aboveground biomass;
      user input from file `Input/veg.in` */
    pct_live[MAX_MONTHS],
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
    use_SWA;

  RealD
    // storing values in same order as defined in STEPWAT2/rgroup.in (0=tree, 1=shrub, 2=grass, 3=forb)
    critSoilWater[NVEGTYPES];

  int
    // `rank_SWPcrits[k]` hold the vegetation type at rank `k` of decreasingly
    // sorted critical SWP values
    rank_SWPcrits[NVEGTYPES],
    veg_method;

  SW_VEGPROD_OUTPUTS
    /** output accumulator: summed values for each output time period */
    *p_accu[SW_OUTNPERIODS],
    /** output aggregator: mean or sum for each output time periods */
    *p_oagg[SW_OUTNPERIODS];
} SW_VEGPROD;

/* =================================================== */
/*                     Time struct                     */
/* --------------------------------------------------- */

typedef struct {
	TimeInt first, last, total;
} SW_TIMES;

/* =================================================== */
/*                   Weather structs                   */
/* --------------------------------------------------- */

typedef struct {
	/* Weather values of the current simulation day */
	RealD temp_avg, temp_max, temp_min, ppt, rain, cloudCover, windSpeed, relHumidity,
    shortWaveRad, actualVaporPressure;
} SW_WEATHER_NOW;

typedef struct {
	/* Daily weather values for one year */
	RealD temp_max[MAX_DAYS], temp_min[MAX_DAYS], temp_avg[MAX_DAYS], ppt[MAX_DAYS],
    cloudcov_daily[MAX_DAYS], windspeed_daily[MAX_DAYS], r_humidity_daily[MAX_DAYS],
    shortWaveRad[MAX_DAYS], actualVaporPressure[MAX_DAYS];
	// RealD temp_month_avg[MAX_MONTHS], temp_year_avg; // currently not used
} SW_WEATHER_HIST;

/* accumulators for output values hold only the */
/* current period's values (eg, weekly or monthly) */
typedef struct {
	RealD temp_max, temp_min, temp_avg, ppt, rain, snow, snowmelt, snowloss, /* 20091015 (drs) ppt is divided into rain and snow */
	snowRunoff, surfaceRunoff, surfaceRunon, soil_inf, et, aet, pet, surfaceAvg, surfaceMax, surfaceMin;
} SW_WEATHER_OUTPUTS;

/**
 @brief Annual time-series of climate variables

 Output of the function `calcSiteClimate()`

 @note 2D array dimensions represent month (1st D) and year (2nd D); 1D array dimension represents year.
 @note Number of years is variable and determined at runtime.
 */
typedef struct {
    RealD **PPTMon_cm,                      /**< 2D array containing monthly amount precipitation [cm]*/
    *PPT_cm,                                /**< Array containing annual precipitation amount [cm]*/
    *PPT7thMon_mm,                          /**< Array containing July precipitation amount in July (northern hemisphere)
                                               or January (southern hemisphere) [mm]*/

    **meanTempMon_C,                        /**< 2D array containing monthly mean average daily air temperature [C]*/
    **maxTempMon_C,                         /**< 2D array containing monthly mean max daily air temperature [C]*/
    **minTempMon_C,                         /**< 2D array containing monthly mean min daily air temperature [C]*/
    *meanTemp_C,                            /**< Array containing annual mean temperatures [C]*/
    *meanTempDriestQtr_C,                   /**< Array containing the average temperatureof the driest quarter of the year [C]*/
    *minTemp2ndMon_C,                       /**< Array containing the mean daily minimum temperature in August (southern hemisphere)
                                             or February (northern hemisphere) [C]*/
    *minTemp7thMon_C,                       /**< Array containing minimum July temperatures in July (northern hisphere)
                                             or Janurary (southern hemisphere) [C]*/

    *frostFree_days,                        /**< Array containing the maximum consecutive days without frost [days]*/
    *ddAbove65F_degday;                     /**< Array containing the amount of degree days [C x day] above 65 F*/
} SW_CLIMATE_YEARLY;

/**
 @brief A structure holding all variables that are output to the function `averageClimateAcrossYears()` #SW_CLIMATE_YEARLY

 @note Values are across-year averages of #SW_CLIMATE_YEARLY and 1D array dimension represents month.
 The exceptions are `sdC4` and `sdCheatgrass` which represent across-year standard devations and the 1D array dimension
 represents different variables, see `averageClimateAcrossYears()`.
 */
typedef struct {
    RealD *meanTempMon_C,                   /**< Array of size MAX_MONTHS containing sum of monthly mean temperatures [C]*/
    *maxTempMon_C,                          /**< Array of size MAX_MONTHS containing sum of monthly maximum temperatures [C]*/
    *minTempMon_C,                          /**< Array of size MAX_MONTHS containing sum of monthly minimum temperatures [C]*/
    *PPTMon_cm,                             /**< Array of size MAX_MONTHS containing sum of monthly mean precipitation [cm]*/
    *sdC4,                                  /**< Array of size three holding the standard deviations of: 0) minimum July (northern hisphere)
                                             or Janurary (southern hemisphere) temperature [C],
                                             1) frost free days [days], 2) number of days above 65F [C x day]*/

    *sdCheatgrass,                          /**< Array of size three holding: 0) the standard deviations of July (northern hisphere)
                                             or Janurary (southern hemisphere) [cm],
                                             1) mean temperature of dry quarter [C], 2) mean minimum temperature of February
                                             (northern hemisphere) or August (southern hemisphere) [C]*/
    meanTemp_C,                             /**< Value containing the average of yearly temperatures [C]*/
    PPT_cm,                                 /**< Value containing the average of yearly precipitation [cm]*/
    PPT7thMon_mm,                           /**< Value containing average precipitation in July (northern hemisphere)
                                              or January (southern hemisphere) [mm]*/
    meanTempDriestQtr_C,                    /**< Value containing average of mean temperatures in the driest quarters of years [C]*/
    minTemp2ndMon_C,                        /**< Value containing average of minimum temperatures in August (southern hemisphere) or
                                             February (northern hemisphere) [C]*/
    ddAbove65F_degday,                      /**< Value containing average of total degrees above 65F (18.33C) throughout the year [C x day]*/
    frostFree_days,                         /**< Value containing average of most consectutive days in a year without frost [days]*/
    minTemp7thMon_C;                        /**< Value containing the average of lowest temperature in July (northern hisphere)
                                             or Janurary (southern hemisphere) [C]*/
} SW_CLIMATE_CLIM;

typedef struct {
    RealD **meanMonthlyTemp_C, **maxMonthlyTemp_C, **minMonthlyTemp_C, **monthlyPPT_cm,
    *annualPPT_cm, *meanAnnualTemp_C, *JulyMinTemp, *frostFreeDays_days, *ddAbove65F_degday,
    *JulyPPT_mm, *meanTempDriestQuarter_C, *minTempFebruary_C;
} SW_CLIMATE_CALC;

typedef struct {
    RealD *meanMonthlyTempAnn, *maxMonthlyTempAnn, *minMonthlyTempAnn, *meanMonthlyPPTAnn,
    *sdC4, *sdCheatgrass, MAT_C, MAP_cm, JulyPPTAnn_mm, meanTempDriestQuarterAnn_C, minTempFebruaryAnn_C,
    ddAbove65F_degdayAnn, frostFreeAnn, JulyMinTempAnn;
} SW_CLIMATE_AVERAGES;

typedef struct {

	Bool
		use_snow,
		use_weathergenerator_only;
			// swTRUE: use weather generator and ignore weather inputs

	unsigned int
		generateWeatherMethod;
			// see `generateMissingWeather()`
			// 0 : pass through missing values
			// 1 : LOCF (temp) + 0 (ppt)
			// 2 : weather generator (previously, `use_weathergenerator`)

	int rng_seed; // initial state for `mark

	RealD pct_snowdrift, pct_snowRunoff;
  RealD
    scale_precip[MAX_MONTHS],
    scale_temp_max[MAX_MONTHS],
    scale_temp_min[MAX_MONTHS],
    scale_skyCover[MAX_MONTHS],
    scale_wind[MAX_MONTHS],
    scale_rH[MAX_MONTHS],
    scale_actVapPress[MAX_MONTHS],
    scale_shortWaveRad[MAX_MONTHS];
	char name_prefix[MAX_FILENAMESIZE - 5]; // subtract 4-digit 'year' file type extension
	RealD snowRunoff, surfaceRunoff, surfaceRunon, soil_inf, surfaceAvg;
	RealD snow, snowmelt, snowloss, surfaceMax, surfaceMin;
	RealD temp_snow; // Snow temperature

  Bool use_cloudCoverMonthly, use_windSpeedMonthly, use_humidityMonthly;
  Bool dailyInputFlags[MAX_INPUT_COLUMNS];

  unsigned int dailyInputIndices[MAX_INPUT_COLUMNS],
 			   n_input_forcings, // Number of input columns found in weath.YYYY
    desc_rsds; /**< Description of units and definition of daily inputs of observed shortwave radiation, see `solar_radiation()` */

	/* This section is required for computing the output quantities.  */
	SW_WEATHER_OUTPUTS
		*p_accu[SW_OUTNPERIODS], // output accumulator: summed values for each time period
		*p_oagg[SW_OUTNPERIODS]; // output aggregator: mean or sum for each time periods


  /* Daily weather record */
  SW_WEATHER_HIST **allHist; /**< Daily weather values; array of length `n_years` of pointers to struct #SW_WEATHER_HIST where the first represents values for calendar year `startYear` */
  unsigned int n_years; /**< Length of `allHist`, i.e., number of years of daily weather */
  unsigned int startYear; /**< Calendar year corresponding to first year of `allHist` */

  SW_WEATHER_NOW now; /**< Weather values of the current simulation day */

} SW_WEATHER;

/* =================================================== */
/*                   Soilwat structs                   */
/* --------------------------------------------------- */

/* parameters for historical (measured) swc values */
typedef struct {
	int method; /* method: 1=average; 2=hist+/- stderr */
	SW_TIMES yr;
	char *file_prefix; /* prefix to historical swc filenames */
	RealD swc[MAX_DAYS][MAX_LAYERS], std_err[MAX_DAYS][MAX_LAYERS];

} SW_SOILWAT_HIST;

/* accumulators for output values hold only the */
/* current period's values (eg, weekly or monthly) */
typedef struct {
	RealD wetdays[MAX_LAYERS], vwcBulk[MAX_LAYERS], /* soil water content cm/cm */
	vwcMatric[MAX_LAYERS], swcBulk[MAX_LAYERS], /* soil water content cm/layer */
	swpMatric[MAX_LAYERS], /* soil water potential */
	swaBulk[MAX_LAYERS], /* available soil water cm/layer, swc-(wilting point) */
	SWA_VegType[NVEGTYPES][MAX_LAYERS],
	swaMatric[MAX_LAYERS],
	transp_total[MAX_LAYERS], transp[NVEGTYPES][MAX_LAYERS],
	evap_baresoil[MAX_LAYERS], /* bare-soil evaporation [cm/layer] */
	lyrdrain[MAX_LAYERS],
	hydred_total[MAX_LAYERS], hydred[NVEGTYPES][MAX_LAYERS], /* hydraulic redistribution cm/layer */
	surfaceWater, surfaceWater_evap,
	total_evap, evap_veg[NVEGTYPES],
	litter_evap,
	total_int, int_veg[NVEGTYPES], litter_int,
			snowpack,
			snowdepth,
			et,
			aet, tran, esoil, ecnw, esurf, esnow,
			pet, H_oh, H_ot, H_gh, H_gt,
			deep,
			avgLyrTemp[MAX_LAYERS], // average soil temperature in celcius for each layer
            lyrFrozen[MAX_LAYERS],
            minLyrTemperature[MAX_LAYERS], // Holds the minimum temperature estimation of each layer
            maxLyrTemperature[MAX_LAYERS]; // Holds the maximum temperature estimation of each layer
} SW_SOILWAT_OUTPUTS;

#ifdef SWDEBUG
  #define N_WBCHECKS 9 // number of water balance checks
#endif

typedef struct {
	/* current daily soil water related values */
	Bool is_wet[MAX_LAYERS]; /* swc sufficient to count as wet today */
	RealD swcBulk[TWO_DAYS][MAX_LAYERS],
		SWA_VegType[TWO_DAYS][MAX_LAYERS],
		snowpack[TWO_DAYS], /* swe of snowpack, if accumulation flag set */
		snowdepth,
		transpiration[NVEGTYPES][MAX_LAYERS],
		evap_baresoil[MAX_LAYERS], /* bare-soil evaporation [cm/layer] */
		drain[MAX_LAYERS], /** drain[i] = total net (saturated + unsaturated) percolation [cm/day] from layer i into layer i + 1; last value is equal to deep drainage */
		hydred[NVEGTYPES][MAX_LAYERS], /* hydraulic redistribution cm/layer */
		surfaceWater, surfaceWater_evap,
		pet, H_oh, H_ot, H_gh, H_gt,
		aet,
		litter_evap, evap_veg[NVEGTYPES],
		litter_int, int_veg[NVEGTYPES], // todays intercepted rain by litter and by vegetation
		avgLyrTemp[MAX_LAYERS],
        lyrFrozen[MAX_LAYERS],
        minLyrTemperature[MAX_LAYERS], // Holds the minimum temperature estimation of each layer
        maxLyrTemperature[MAX_LAYERS]; // Holds the maximum temperature estimation of each layer

	RealD
		veg_int_storage[NVEGTYPES], // storage of intercepted rain by the vegetation
		litter_int_storage, // storage of intercepted rain by the litter layer
		standingWater[TWO_DAYS]; /* water on soil surface if layer below is saturated */

	RealF swa_master[NVEGTYPES][NVEGTYPES][MAX_LAYERS]; // veg_type, crit_val, layer
	RealF dSWA_repartitioned_sum[NVEGTYPES][MAX_LAYERS];

	Bool soiltempError; // soil temperature error indicator
	#ifdef SWDEBUG
	int wbError[N_WBCHECKS]; /* water balance and water cycling error indicators (currently 8)
	    0, no error detected; > 0, number of errors detected */
  char *wbErrorNames[N_WBCHECKS];
  Bool is_wbError_init;
  #endif

	SW_SOILWAT_OUTPUTS
		*p_accu[SW_OUTNPERIODS], // output accumulator: summed values for each time period
		*p_oagg[SW_OUTNPERIODS]; // output aggregator: mean or sum for each time periods
	Bool hist_use;
	SW_SOILWAT_HIST hist;

} SW_SOILWAT;

typedef struct {
	FILE *logfp;
	// This is the pointer to the log file.

	Bool logged;
	// Indicates logfile written to via LogError.

	char *errorMsg,              // Holds the message for a fatal error
		 *warningMsgs[MAX_MSGS]; // Holds up to MAX_MSGS warning messages to report

	int numWarnings;        // Number of warnings to be written out

	Bool stopRun;           // Specifies if an error has occurred and
                            // the program needs to stop early (backtrack)
} LOG_INFO;

typedef struct {
	char *InFiles[SW_NFILES];
	char _ProjDir[FILENAME_MAX];
	char weather_prefix[FILENAME_MAX];
	char output_prefix[FILENAME_MAX];
} PATH_INFO;

/* =================================================== */
/*                    Sky structs                      */
/* --------------------------------------------------- */

typedef struct {
    RealD cloudcov     [MAX_MONTHS], /* monthly cloud cover (frac) */
          windspeed    [MAX_MONTHS], /* windspeed (m/s) */
          r_humidity   [MAX_MONTHS], /* relative humidity (%) */
          snow_density [MAX_MONTHS], /* snow density (kg/m3) */
          n_rain_per_day[MAX_MONTHS]; /* number of precipitation events per month (currently used in interception functions) */

    RealD snow_density_daily	[MAX_DAYS+1];	/* interpolated daily snow density (kg/m3) */

} SW_SKY;

/* =================================================== */
/*                  VegEstab structs                   */
/* --------------------------------------------------- */

typedef struct {

	/* see COMMENT-1 below for more information on these vars */

	/* THESE VARIABLES CAN CHANGE VALUE IN THE MODEL */
	TimeInt estab_doy, /* day of establishment for this plant */
			germ_days, /* elapsed days since germination with no estab */
			drydays_postgerm, /* did sprout get too dry for estab? */
			wetdays_for_germ, /* keep track of consecutive wet days */
			wetdays_for_estab;
	Bool germd, /* has this plant germinated yet?  */
		no_estab; /* if swTRUE, can't attempt estab for remainder of year */

	/* THESE VARIABLES DO NOT CHANGE DURING THE NORMAL MODEL RUN */
	char sppFileName[MAX_FILENAMESIZE]; /* Store the file Name and Path, Mostly for Rsoilwat */
	char sppname[MAX_SPECIESNAMELEN + 1]; /* one set of parms per species */
	unsigned int vegType; /**< Vegetation type of species (see "Indices to vegetation types") */
	TimeInt min_pregerm_days, /* first possible day of germination */
			max_pregerm_days, /* last possible day of germination */
			min_wetdays_for_germ, 	/* number of consecutive days top layer must be */
									/* "wet" in order for germination to occur. */
			max_drydays_postgerm, 	/* maximum number of consecutive dry days after */
									/* germination before establishment can no longer occur. */
			min_wetdays_for_estab, 	/* minimum number of consecutive days the top layer */
									/* must be "wet" in order to establish */
			min_days_germ2estab, 	/* minimum number of days to wait after germination */
									/* and seminal roots wet before check for estab. */
			max_days_germ2estab; 	/* maximum number of days after germination to wait */
									/* for establishment */

	unsigned int estab_lyrs;	/* estab could conceivably need more than one layer */
								/* swc is averaged over these top layers to compare to */
								/* the converted value from min_swc_estab */
	RealF bars[2], 			/* read from input, saved for reporting */
			min_swc_germ, 	/* wetting point required for germination converted from */
							/* bars to cm per layer for efficiency in the loop */
			min_swc_estab, 	/* same as min_swc_germ but for establishment */
							/* this is the average of the swc of the first estab_lyrs */
			min_temp_germ, 	/* min avg daily temp req't for germination */
			max_temp_germ, 	/* max temp for germ in degC */
			min_temp_estab, /* min avg daily temp req't for establishment */
			max_temp_estab; /* max temp for estab in degC */

} SW_VEGESTAB_INFO;

typedef struct {
	TimeInt *days;	/* only output the day of estab for each species in the input */
					/* this array is allocated via `SW_VegEstab_construct()` */
					/* each day in the array corresponds to the ordered species list */
} SW_VEGESTAB_OUTPUTS;

typedef struct {
  Bool use;      /* if swTRUE use establishment parms and chkestab() */
  IntU count;  /* number of species to check */
  SW_VEGESTAB_INFO **parms;  /* dynamic array of parms for each species */
  SW_VEGESTAB_OUTPUTS  /* only yearly element will be used */
		*p_accu[SW_OUTNPERIODS], // output accumulator: summed values for each time period
		*p_oagg[SW_OUTNPERIODS]; // output aggregator: mean or sum for each time periods

} SW_VEGESTAB;

/* =================================================== */
/*                   Markov struct                     */
/* --------------------------------------------------- */

typedef struct {

	/* pointers to arrays of probabilities for each day saves some space */
	/* by not being allocated if markov weather not requested by user */
	/* alas, multi-dimensional arrays aren't so convenient */
  RealD
    *wetprob, /* probability of being wet today given a wet yesterday */
    *dryprob, /* probability of being wet today given a dry yesterday */
    *avg_ppt, /* mean precip (cm) of wet days */
    *std_ppt, /* std dev. for precip of wet days */
    *cfxw, /*correction factor for tmax for wet days */
    *cfxd, /*correction factor for tmax for dry days */
    *cfnw, /*correction factor for tmin for wet days */
    *cfnd, /*correction factor for tmin for dry days */
    u_cov[MAX_WEEKS][2], /* mean weekly maximum and minimum temperature in degree Celsius */
    v_cov[MAX_WEEKS][2][2]; /* covariance matrix */
  int ppt_events; /* number of ppt events generated this year */
  pcg32_random_t markov_rng; // used by STEPWAT2

} SW_MARKOV;

/* =================================================== */
/*                 Output struct/enums                 */
/* --------------------------------------------------- */

typedef enum {
	eSW_Off, eSW_Sum, eSW_Avg, eSW_Fnl
} OutSum;

/* these are the code analog of the above */
/* see also key2str[] in Output.c */
/* take note of boundary conditions in ForEach...() loops below */
typedef enum {
	eSW_NoKey = -1,
	/* weather/atmospheric quantities */
	eSW_AllWthr, /* includes all weather vars */
	eSW_Temp,
	eSW_Precip,
	eSW_SoilInf,
	eSW_Runoff,
	/* soil related water quantities */
	eSW_AllH2O,
	eSW_VWCBulk,
	eSW_VWCMatric,
	eSW_SWCBulk,
	eSW_SWABulk,
	eSW_SWAMatric,
	eSW_SWA,
	eSW_SWPMatric,
	eSW_SurfaceWater,
	eSW_Transp,
	eSW_EvapSoil,
	eSW_EvapSurface,
	eSW_Interception,
	eSW_LyrDrain,
	eSW_HydRed,
	eSW_ET,
	eSW_AET,
	eSW_PET, /* really belongs in wth, but for historical reasons we'll keep it here */
	eSW_WetDays,
	eSW_SnowPack,
	eSW_DeepSWC,
	eSW_SoilTemp,
    eSW_Frozen,
	/* vegetation quantities */
	eSW_AllVeg,
	eSW_Estab,
	// vegetation other */
	eSW_CO2Effects,
	eSW_Biomass,
	eSW_LastKey /* make sure this is the last one */
} OutKey;

typedef struct {
	OutKey mykey;
	ObjType myobj;
	OutSum sumtype;
	Bool
		use,		// TRUE if output is requested
		has_sl;	// TRUE if output key/type produces output for each soil layer
	TimeInt
		first, last, 			/* first/last doy of current year, i.e., updated for each year */
		first_orig, last_orig; /* first/last doy that were originally requested */

	#if defined(RSOILWAT)
	char *outfile; /* name of output */ //could probably be removed
	#endif
} SW_OUTPUT;

typedef struct {
	TimeInt tOffset; /* 1 or 0 means we're writing previous or current period */


	// Variables describing output periods:
	/** `timeSteps` is the array that keeps track of the output time periods that
		are required for `text` and/or `array`-based output for each output key. */
	OutPeriod timeSteps[SW_OUTNKEYS][SW_OUTNPERIODS];

	/** The number of different time steps/periods that are used/requested
			Note: Under STEPWAT2, this may be larger than the sum of `use_OutPeriod`
				because it also incorporates information from `timeSteps_SXW`. */
	IntUS used_OUTNPERIODS;

	/** TRUE if time step/period is active for any output key. */
	Bool use_OutPeriod[SW_OUTNPERIODS];


	// Variables describing size and names of output
	/** names of output columns for each output key; number is an expensive guess */
	char *colnames_OUT[SW_OUTNKEYS][5 * NVEGTYPES + MAX_LAYERS];

	/** number of output columns for each output key */
	IntUS ncol_OUT[SW_OUTNKEYS];

	Bool print_IterationSummary;
	Bool print_SW_Output;
	char sw_outstr[MAX_LAYERS * OUTSTRLEN];

	#if defined(RSOILWAT) || defined(STEPWAT)
	/** \brief A 2-dim array of pointers to output arrays.

	The variable p_OUT used by rSOILWAT2 for output and by STEPWAT2 for
	mean aggregation.
	*/
	RealD *p_OUT[SW_OUTNKEYS][SW_OUTNPERIODS];

	size_t nrow_OUT[SW_OUTNPERIODS];
	size_t irow_OUT[SW_OUTNPERIODS];
	#endif

	#ifdef STEPWAT
	RealD *p_OUTsd[SW_OUTNKEYS][SW_OUTNPERIODS];

	char sw_outstr_agg[MAX_LAYERS * OUTSTRLEN];

	/** `timeSteps_SXW` is the array that keeps track of the output time periods
		that are required for `SXW` in-memory output for each output key.
		Compare with `timeSteps` */
	OutPeriod timeSteps_SXW[SW_OUTNKEYS][SW_OUTNPERIODS];

	/** `storeAllIterations` is set to TRUE if STEPWAT2 is called with `-i` flag
		 if TRUE, then write to disk the SOILWAT2 output
		for each STEPWAT2 iteration/repeat to separate files */
	Bool storeAllIterations;

	/** `prepare_IterationSummary` is set to TRUE if STEPWAT2 is called with
		 `-o` flag; if TRUE, then calculate/write to disk the running mean and sd
		across iterations/repeats */
	Bool prepare_IterationSummary;

	/* Variable from ModelType (STEPWAT2) used in SOILWAT2 */
	IntUS currIter;

	/* Variables from SXW_t (STEPWAT2) used in SOILWAT2 */
	// transpXXX: monthly sum of soilwat's transpiration by soil layer
	// * these are dynamic arrays that are indexed by Ilp()
	RealD transpTotal[MAX_LAYERS][MAX_MONTHS], // total transpiration, i.e., sum across vegetation types
		  transpVeg[NVEGTYPES][MAX_LAYERS][MAX_MONTHS]; // transpiration as contributed by vegetation types
	RealF swc[MAX_LAYERS][MAX_MONTHS]; // monthly mean SWCbulk for each soil layer

	// fixed monthly array:
	RealF ppt_monthly[MAX_MONTHS];  // monthly sum of soilwat's precipitation
	RealF temp_monthly[MAX_MONTHS];  // monthly mean soilwat's air temperature

	// annual values:
	RealF temp,   // annual mean soilwat's air temperature
		  ppt,    // annual sum of soilwat's precipitation
		  aet;    // annual sum of soilwat's evapotranspiration
	#endif
} SW_GEN_OUT;

/* =================================================== */
/*                 Comprehensive struct                */
/* --------------------------------------------------- */

typedef struct {
	SW_VEGPROD VegProd;
	SW_WEATHER Weather;
	SW_SOILWAT SoilWat;
	SW_MODEL Model;
	SW_SITE Site;
	SW_VEGESTAB VegEstab;
	SW_SKY Sky;
	SW_CARBON Carbon;
	ST_RGR_VALUES StRegValues;
	SW_FILE_STATUS FileStatus;
	SW_MARKOV Markov;
	SW_OUTPUT Output[SW_OUTNKEYS];

	SW_ATMD AtmDemand;
	SW_GEN_OUT GenOutput;

} SW_ALL;

/* =================================================== */
/*                 Output struct/enums                 */
/* --------------------------------------------------- */

typedef struct {

	#ifdef SW_OUTTEXT
	void (*pfunc_text)(OutPeriod, SW_ALL*); /* pointer to output routine for text output */
	#endif

	#if defined(RSOILWAT)
	void (*pfunc_mem)(OutPeriod, SW_ALL*); /* pointer to output routine for array output */

	#elif defined(STEPWAT)
	void (*pfunc_agg)(OutPeriod, SW_ALL*); /* pointer to output routine for aggregated output across STEPWAT iterations */
	void (*pfunc_SXW)(OutPeriod, SW_ALL*); /* pointer to output routine for STEPWAT in-memory output */
	#endif
} SW_OUTPUT_POINTERS;

/* =================================================== */
/*                 Block info struct                   */
/* --------------------------------------------------- */

 /* blockinfo is a structure that contains the memory log information
 * for one allocated memory block.  Every allocated memory block has
 * a corresponding blockinfo structure in the memory log.
 */
typedef struct BLOCKINFO {
	struct BLOCKINFO *pbiNext;
	byte *pb; /* start of block    */
	size_t size; /* length of block   */
	flag fReferenced; /* Ever referenced?  */
} blockinfo; /* Naming:  bi, *pbi */

#endif // DATATYPES_H
