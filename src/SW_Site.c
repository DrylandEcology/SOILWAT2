/********************************************************/
/********************************************************/
/*	Application: SOILWAT - soilwater dynamics simulator
 Source file: Site.c
 Type: module
 Purpose: Read / write and otherwise manage the
 site specific information.  See also the
 Layer module.
 History:
 (8/28/01) -- INITIAL CODING - cwb
 (10/12/2009) - (drs) added altitude
 11/02/2010	(drs) added 5 snow parameters to SW_SITE to be read in from siteparam.in
 10/19/2010	(drs) added HydraulicRedistribution flag, and maxCondroot, swp50, and shapeCond parameters to SW_SIT_read()  and _echo_inputs()
 07/20/2011	(drs) updated _read_layers() to read impermeability values from each soil layer from soils.in file
 added calculation for saturated swc in water_eqn()
 updated _echo_inputs() to print impermeability and saturated swc values
 09/08/2011	(drs) moved all hydraulic redistribution parameters to SW_VegProd.h struct VegType
 09/15/2011	(drs)	deleted albedo from SW_SIT_read() and _echo_inputs(): moved it to SW_VegProd.h to make input vegetation type dependent
 02/03/2012	(drs)	if input of SWCmin < 0 then estimate SWCmin with 'SW_SWC_SWCres' for each soil layer
 02/04/2012	(drs)	included SW_VegProd.h and created global variable extern SW_VegProd: to access vegetation-specific SWPcrit
 02/04/2012	(drs)	added calculation of swc at SWPcrit for each vegetation type and layer to function 'init_site_info()'
 added vwc/swc at SWPcrit to '_echo_inputs()'
 05/24/2012  (DLM) edited SW_SIT_read(void) function to be able to read in Soil Temperature constants from siteparam.in file
 05/24/2012  (DLM) edited _echo_inputs(void) function to echo the Soil Temperature constants to the logfile
 05/25/2012  (DLM) edited _read_layers( void) function to read in the initial soil temperature for each layer
 05/25/2012  (DLM) edited _echo_inputs( void) function to echo the read in soil temperatures for each layer
 05/30/2012  (DLM) edited _read_layers & _echo_inputs functions to read in/echo the deltaX parameter
 05/31/2012  (DLM) edited _read_layers & _echo_inputs functions to read in/echo stMaxDepth & use_soil_temp variables
 05/31/2012  (DLM) edited init_site_info(void) to check if stMaxDepth & stDeltaX values are usable, if not it resets them to the defaults (180 & 15).
 11/06/2012	(clk)	In SW_SIT_read(void), added lines to read in aspect and slope from siteparam.in
 11/06/2012	(clk)	In _echo_inputs(void), added lines to echo aspect and slope to logfile
 11/30/2012	(clk)	In SW_SIT_read(void), added lines to read in percentRunoff from siteparam.in
 11/30/2012	(clk)	In _echo_inputs(void), added lines to echo percentRunoff to logfile
 04/16/2013	(clk)	changed the water_eqn to use the fraction of gravel content in the calculation
 Added the function calculate_soilBulkDensity() which is used to calculate the bulk density of the soil from the inputed matric density. Using eqn 20 from Saxton 2006
 Needed to change the input from soils.in to save to soilMatric_density instead of soilBulk_density
 Changed read_layers() to do a few different things
 First, it now reads in a value for fractionVolBulk_gravel from soils.in
 Secondly, it calls the calculate_soilBulkDensity function for each layer
 Lastly, since fieldcap and wiltpt were removed from soils.in, those values are now calculated within read_layers()
 05/16/2013	(drs)	fixed in init_site_info() the check of transpiration region validity: it gave error if only one layer was present
 06/24/2013	(rjm)	added function void SW_SIT_clear_layers(void) to free allocated soil layers
 06/27/2013	(drs)	closed open files if LogError() with LOGFATAL is called in SW_SIT_read(), _read_layers()
 07/09/2013	(clk)	added the initialization of all the new variables
 06/05/2016 (ctd) Modified threshold for condition involving gravel in _read_layers() function - as per Caitlin's request.
 									Also, added print statements to notify the user that values may be invalid if the gravel content does not follow
									parameters of Corey-Brooks equation.
 */
/********************************************************/
/********************************************************/

/* =================================================== */
/*                INCLUDES / DEFINES                   */
/* --------------------------------------------------- */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "include/generic.h"
#include "include/filefuncs.h"
#include "include/myMemory.h"
#include "include/SW_VegProd.h"

#include "include/SW_Carbon.h"
#include "include/SW_Files.h"
#include "include/SW_Site.h"
#include "include/SW_SoilWater.h"
#include "include/SW_Main_lib.h"



/* =================================================== */
/*                  Global Variables                   */
/* --------------------------------------------------- */

/** Character representation of implemented Soil Water Retention Curves (SWRC)

	@note Code maintenance:
		- Values must exactly match those provided in `siteparam.in`.
		- Order must exactly match "indices of `swrc2str`"
		- See details in section \ref swrc_ptf
*/
char const *swrc2str[N_SWRCs] = {
	"Campbell1974",
	"vanGenuchten1980",
	"FXW"
};

/** Character representation of implemented Pedotransfer Functions (PTF)

	@note Code maintenance:
		- Values must exactly match those provided in `siteparam.in`.
		- Order must exactly match "indices of `ptf2str`"
		- See details in section \ref swrc_ptf
		- `rSOILWAT2` may implemented additional PTFs
*/
char const *ptf2str[N_PTFs] = {
	"Cosby1984AndOthers",
	"Cosby1984"
};

/* =================================================== */
/*             Local Function Definitions              */
/* --------------------------------------------------- */

/** Check validity of soil properties

	@param[in] SW_Site Struct of type SW_SITE describing the simulated site
	@param[in] layerno Current layer which is being worked with
	@param[in] LogInfo Holds information dealing with logfile output

	@return A logical value indicating if soil properties passed the checks.
*/
static Bool SW_check_soil_properties(SW_SITE *SW_Site,
		LyrIndex layerno, LOG_INFO* LogInfo) {

	int k;
	RealD fval = 0;
	const char *errtype = "\0";
	Bool res = swTRUE;


	if (LE(SW_Site->width[layerno], 0.)) {
		res = swFALSE;
		fval = SW_Site->width[layerno];
		errtype = Str_Dup("layer width", LogInfo);

	} else if (
		LT(SW_Site->soilDensityInput[layerno], 0.) ||
		GT(SW_Site->soilDensityInput[layerno], 2.65)
	) {
		res = swFALSE;
		fval = SW_Site->soilDensityInput[layerno];
		errtype = Str_Dup("soil density", LogInfo);

	} else if (
		LT(SW_Site->fractionVolBulk_gravel[layerno], 0.) ||
		GE(SW_Site->fractionVolBulk_gravel[layerno], 1.)
	) {
		res = swFALSE;
		fval = SW_Site->fractionVolBulk_gravel[layerno];
		errtype = Str_Dup("gravel content", LogInfo);

	} else if (
		LE(SW_Site->fractionWeightMatric_sand[layerno], 0.) ||
		GE(SW_Site->fractionWeightMatric_sand[layerno], 1.)
	) {
		res = swFALSE;
		fval = SW_Site->fractionWeightMatric_sand[layerno];
		errtype = Str_Dup("sand proportion", LogInfo);

	} else if (
		LE(SW_Site->fractionWeightMatric_clay[layerno], 0.) ||
		GE(SW_Site->fractionWeightMatric_clay[layerno], 1.)
	) {
		res = swFALSE;
		fval = SW_Site->fractionWeightMatric_clay[layerno];
		errtype = Str_Dup("clay proportion", LogInfo);

	} else if (
		GE(SW_Site->fractionWeightMatric_sand[layerno] +
						SW_Site->fractionWeightMatric_clay[layerno], 1.)
	) {
		res = swFALSE;
		fval = SW_Site->fractionWeightMatric_sand[layerno] +
									SW_Site->fractionWeightMatric_clay[layerno];
		errtype = Str_Dup("sand+clay proportion", LogInfo);

	} else if (
		LT(SW_Site->impermeability[layerno], 0.) ||
		GT(SW_Site->impermeability[layerno], 1.)
	) {
		res = swFALSE;
		fval = SW_Site->impermeability[layerno];
		errtype = Str_Dup("impermeability", LogInfo);

	} else if (
		LT(SW_Site->evap_coeff[layerno], 0.) ||
		GT(SW_Site->evap_coeff[layerno], 1.)
	) {
		res = swFALSE;
		fval = SW_Site->evap_coeff[layerno];
		errtype = Str_Dup("bare-soil evaporation coefficient", LogInfo);

	} else {
		ForEachVegType(k) {
			if (LT(SW_Site->transp_coeff[k][layerno], 0.) ||
									GT(SW_Site->transp_coeff[k][layerno], 1.)) {

				res = swFALSE;
				fval = SW_Site->transp_coeff[k][layerno];
				errtype = Str_Dup("transpiration coefficient", LogInfo);
				break;
			}
		}
	}

	if (!res) {
		LogError(
			LogInfo,
			LOGFATAL,
			"'%s' has an invalid value (%5.4f) in layer %d.\n",
			errtype, fval, layerno + 1
		);
	}

	return res;
}




/** A realistic lower limit for minimum `theta`

@note Currently, -30 MPa
		(based on limited test runs across western US including hot deserts)
		lower than "air-dry" = hygroscopic point (-10. MPa; Porporato et al. 2001)
		not as extreme as "oven-dry" (-1000. MPa; Fredlund et al. 1994)
*/
static double lower_limit_of_theta_min(
	LOG_INFO* LogInfo,
	unsigned int swrc_type,
	double *swrcp,
	double gravel,
	double width
) {
	double res = SWRC_SWPtoSWC(300., swrc_type, swrcp,
							   gravel, width, LOGFATAL, LogInfo);

	// convert bulk [cm] to matric [cm / cm]
	return res / ((1. - gravel) * width);
}

/**
	@brief User-input based minimum volumetric water content

	This function returns minimum soil water content `theta_min` determined
	by user input via #SW_SITE._SWCMinVal as
		* a fixed VWC value,
		* a fixed SWP value, or
		* a realistic lower limit from `lower_limit_of_theta_min()`,
			unless legacy mode (`ptf` equal to "Cosby1984AndOthers") is used
			(reproducing behavior prior to v7.0.0), then calculated as
			maximum of `lower_limit_of_theta_min()` and `PTF_RawlsBrakensiek1985()`
			with pedotransfer function by Rawls & Brakensiek 1985 \cite rawls1985WmitE
			(independently of the selected SWRC).

	@param[in] ui_sm_min User input of requested minimum soil moisture,
		see _SWCMinVal
	@param[in] gravel Coarse fragments (> 2 mm; e.g., gravel)
		of the whole soil [m3/m3]
	@param[in] width Soil layer width [cm]
	@param[in] sand Sand content of the matric soil (< 2 mm fraction) [g/g]
	@param[in] clay Clay content of the matric soil (< 2 mm fraction) [g/g]
	@param[in] swcBulk_sat Saturated water content of the bulk soil [cm]
	@param[in] swrc_type Identification number of selected SWRC
	@param[in] *swrcp Vector of SWRC parameters
	@param[in] legacy_mode If true then legacy behavior (see details)
	@param[in] _SWCMinVal Lower bound on swc.

	@return Minimum volumetric water content of the matric soil [cm / cm]
*/
static double ui_theta_min(
	LOG_INFO* LogInfo,
	double ui_sm_min,
	double gravel,
	double width,
	double sand,
	double clay,
	double swcBulk_sat,
	unsigned int swrc_type,
	double *swrcp,
	Bool legacy_mode,
	RealD _SWCMinVal
) {
	double vwc_min = SW_MISSING, tmp_vwcmin;

	if (LT(ui_sm_min, 0.0)) {
		/* user input: request to estimate minimum theta */

		/* realistic lower limit for theta_min */
		vwc_min = lower_limit_of_theta_min(LogInfo, swrc_type,
										   swrcp, gravel, width);

		if (legacy_mode) {
			/* residual theta estimated with Rawls & Brakensiek (1985) PTF */
			PTF_RawlsBrakensiek1985(
				&tmp_vwcmin,
				sand,
				clay,
				swcBulk_sat / ((1. - gravel) * width),
				LogInfo
			);

			/* if `PTF_RawlsBrakensiek1985()` was successful, then take max */
			if (!missing(tmp_vwcmin)) {
				vwc_min = fmax(vwc_min, tmp_vwcmin);
			}
		}

	} else if (GE(_SWCMinVal, 1.0)) {
		/* user input: fixed (matric) SWP value; unit(_SWCMinVal) == -bar */
		vwc_min = SWRC_SWPtoSWC(
			_SWCMinVal,
			swrc_type,
			swrcp,
			gravel,
			width,
			LOGFATAL,
			LogInfo
		) / ((1. - gravel) * width);

	} else {
		/* user input: fixed matric VWC; unit(_SWCMinVal) == cm/cm */
		vwc_min = _SWCMinVal / width;
	}

	return vwc_min;
}



/* =================================================== */
/*             Global Function Definitions             */
/* --------------------------------------------------- */


/**
	@brief Translate a SWRC name into a SWRC type number

	See #swrc2str and `siteparam.in`.
	Throws an error if `SWRC` is not implemented.

	@param[in] *swrc_name Name of a SWRC
	@param[in] LogInfo Holds information dealing with logfile output

	@return Internal identification number of selected SWRC
*/
unsigned int encode_str2swrc(char *swrc_name, LOG_INFO* LogInfo) {
	unsigned int k;

	for (
		k = 0;
		k < N_SWRCs && Str_CompareI(swrc_name, (char *)swrc2str[k]);
		k++
	);

	if (k == N_SWRCs) {
		LogError(
			LogInfo,
			LOGFATAL,
			"SWRC '%s' is not implemented.",
			swrc_name
		);
	}

	return k;
}

/**
	@brief Translate a PTF name into a PTF type number

	See #ptf2str and `siteparam.in`.

	@param[in] *ptf_name Name of a PTF

	@return Internal identification number of selected PTF;
		#SW_MISSING if not implemented.
*/
unsigned int encode_str2ptf(char *ptf_name) {
	unsigned int k;

	for (
		k = 0;
		k < N_PTFs && Str_CompareI(ptf_name, (char *)ptf2str[k]);
		k++
	);

	if (k == N_PTFs) {
		k = (unsigned int) SW_MISSING;
	}

	return k;
}



/**
	@brief Estimate parameters of selected soil water retention curve (SWRC)
		using selected pedotransfer function (PTF)

	See #ptf2str for implemented PTFs.

	@param[in] ptf_type Identification number of selected PTF
	@param[out] *swrcp Vector of SWRC parameters to be estimated
	@param[in] sand Sand content of the matric soil (< 2 mm fraction) [g/g]
	@param[in] clay Clay content of the matric soil (< 2 mm fraction) [g/g]
	@param[in] gravel Coarse fragments (> 2 mm; e.g., gravel)
		of the whole soil [m3/m3]
	@param[in] bdensity Density of the whole soil
		(matric soil plus coarse fragments) [g/cm3];
		accepts #SW_MISSING if not used by selected PTF
	@param[in] LogInfo Holds information dealing with logfile output

*/
void SWRC_PTF_estimate_parameters(
	unsigned int ptf_type,
	double *swrcp,
	double sand,
	double clay,
	double gravel,
	double bdensity,
	LOG_INFO* LogInfo
) {

	/* Initialize swrcp[] to 0 */
	memset(swrcp, 0., SWRC_PARAM_NMAX * sizeof(swrcp[0]));

	if (ptf_type == sw_Cosby1984AndOthers) {
		SWRC_PTF_Cosby1984_for_Campbell1974(swrcp, sand, clay);

	} else if (ptf_type == sw_Cosby1984) {
		SWRC_PTF_Cosby1984_for_Campbell1974(swrcp, sand, clay);

	} else {
		LogError(
			LogInfo,
			LOGFATAL,
			"PTF is not implemented in SOILWAT2."
		);
	}

	/**********************************/
	/* TODO: remove once PTFs are implemented that utilize gravel */
	/* avoiding `error: unused parameter 'gravel' [-Werror=unused-parameter]` */
	if (gravel < 0.) {};

	/* TODO: remove once PTFs are implemented that utilize bdensity */
	/* avoiding `error: unused parameter 'gravel' [-Werror=unused-parameter]` */
	if (bdensity < 0.) {};
	/**********************************/
}


/**
	@brief Estimate Campbell's 1974 SWRC parameters
		using Cosby et al. 1984 multivariate PTF

	Estimation of four SWRC parameter values `swrcp`
	based on sand, clay, and (silt).

	Parameters are explained in `SWRC_check_parameters_for_Campbell1974()`.

	Multivariate PTFs are from Cosby et al. 1984 (\cite Cosby1984) Table 4;
	Cosby et al. 1984 provided also univariate PTFs in Table 5
	but they are not used here.

	See `SWRC_SWCtoSWP_Campbell1974()` and `SWRC_SWPtoSWC_Campbell1974()`
	for implementation of Campbell's 1974 SWRC (\cite Campbell1974).

	@param[out] *swrcp Vector of SWRC parameters to be estimated
	@param[in] sand Sand content of the matric soil (< 2 mm fraction) [g/g]
	@param[in] clay Clay content of the matric soil (< 2 mm fraction) [g/g]
*/
void SWRC_PTF_Cosby1984_for_Campbell1974(
	double *swrcp,
	double sand, double clay
) {
	/* Table 4 */
	/* Original equations formulated with percent sand (%) and clay (%),
	  here re-formulated for fraction of sand and clay */

	/* swrcp[0] = psi_saturated: originally formulated as function of silt
	   here re-formulated as function of clay */
	swrcp[0] = powe(10.0, -1.58 * sand - 0.63 * clay + 2.17);
	/* swrcp[1] = theta_saturated: originally with units [100 * cm / cm]
	   here re-formulated with units [cm / cm] */
	swrcp[1] = -0.142 * sand - 0.037 * clay + 0.505;
	/* swrcp[2] = b */
	swrcp[2] = -0.3 * sand + 15.7 * clay + 3.10;
	/* swrcp[3] = K_saturated: originally with units [inches / day]
	   here re-formulated with units [cm / day] */
	swrcp[3] = 2.54 * 24. * powe(10.0, 1.26 * sand - 6.4 * clay - 0.60);
}


/**
	@brief Saturated soil water content

	See #ptf2str for implemented PTFs.
	See #swrc2str for implemented SWRCs.

	Saturated volumetric water content is usually estimated as one of the
	SWRC parameters; this is what this function returns.

	For historical reasons, if `swrc_name` is "Campbell1974", then a
	`ptf_name` of "Cosby1984AndOthers" will reproduce `SOILWAT2` legacy mode
	(`SOILWAT2` prior to v7.0.0) and return saturated soil water content estimated
	by Saxton et al. 2006 (\cite Saxton2006) PTF instead;
	`ptf_name` of "Cosby1984" will return saturated soil water content estimated
	by Cosby et al. 1984 (\cite Cosby1984) PTF.

	The arguments `ptf_type`, `sand`, and `clay` are utilized only if
	`ptf_name` is "Cosby1984AndOthers" (see #ptf2str).

	@param[in] swrc_type Identification number of selected SWRC
	@param[in] *swrcp Vector of SWRC parameters
	@param[in] gravel Coarse fragments (> 2 mm; e.g., gravel)
		of the whole soil [m3/m3]
	@param[in] width Soil layer width [cm]
	@param[in] ptf_type Identification number of selected PTF
	@param[in] sand Sand content of the matric soil (< 2 mm fraction) [g/g]
	@param[in] clay Clay content of the matric soil (< 2 mm fraction) [g/g]
	@param[in] LogInfo Holds information dealing with logfile output

	@return Estimated saturated water content of the bulk soil [cm]
*/
double SW_swcBulk_saturated(
	unsigned int swrc_type,
	double *swrcp,
	double gravel,
	double width,
	unsigned int ptf_type,
	double sand,
	double clay,
	LOG_INFO* LogInfo
) {
	double theta_sat = SW_MISSING;

	switch (swrc_type) {
		case sw_Campbell1974:
			if (ptf_type == sw_Cosby1984AndOthers) {
				// Cosby1984AndOthers (backwards compatible)
				PTF_Saxton2006(&theta_sat, sand, clay, LogInfo);
			} else {
				theta_sat = swrcp[1];
			}
			break;

		case sw_vanGenuchten1980:
			theta_sat = swrcp[1];
			break;

		case sw_FXW:
			theta_sat = swrcp[0];
			break;

		default:
			LogError(
				LogInfo,
				LOGFATAL,
				"`SW_swcBulk_saturated()`: SWRC (type %d) is not implemented.",
				swrc_type
			);
			break;
	}

	// Convert from matric [cm/cm] to bulk [cm]
	return theta_sat * width * (1. - gravel);
}


/**
	@brief Lower limit of simulated soil water content

	See #ptf2str for implemented PTFs.
	See #swrc2str for implemented SWRCs.

	SWRCs provide a theoretical minimum/residual volumetric water content;
	however, water potential at that minimum value is infinite. Instead, we
	use a realistic lower limit that does not crash the simulation.

	The lower limit is determined via `ui_theta_min()` using user input
	and is strictly larger than the theoretical SWRC value.

	The arguments `sand`, `clay`, and `swcBulk_sat`
	are utilized only in legacy mode, i.e., `ptf` equal to "Cosby1984AndOthers".

	@param[in] swrc_type Identification number of selected SWRC
	@param[in] *swrcp Vector of SWRC parameters
	@param[in] gravel Coarse fragments (> 2 mm; e.g., gravel)
		of the whole soil [m3/m3]
	@param[in] width Soil layer width [cm]
	@param[in] ptf_type Identification number of selected PTF
	@param[in] ui_sm_min User input of requested minimum soil moisture,
		see #SW_SITE._SWCMinVal
	@param[in] sand Sand content of the matric soil (< 2 mm fraction) [g/g]
	@param[in] clay Clay content of the matric soil (< 2 mm fraction) [g/g]
	@param[in] swcBulk_sat Saturated water content of the bulk soil [cm]
	@param[in] _SWCMinVal Lower bound on swc.
	@param[in] LogInfo Holds information dealing with logfile output

	@return Minimum water content of the bulk soil [cm]
*/
double SW_swcBulk_minimum(
	unsigned int swrc_type,
	double *swrcp,
	double gravel,
	double width,
	unsigned int ptf_type,
	double ui_sm_min,
	double sand,
	double clay,
	double swcBulk_sat,
	RealD _SWCMinVal,
	LOG_INFO* LogInfo
) {
	double theta_min_sim, theta_min_theoretical = SW_MISSING;

	/* `theta_min` based on theoretical SWRC */
	switch (swrc_type) {
		case sw_Campbell1974: // phi = infinity at theta_min
			theta_min_theoretical = 0.;
			break;

		case sw_vanGenuchten1980: // phi = infinity at theta_min
			theta_min_theoretical = swrcp[0];
			break;

		case sw_FXW: // phi = 6.3 x 10^6 cm at theta_min
			theta_min_theoretical = 0.;
			break;

		default:
			LogError(
				LogInfo,
				LOGFATAL,
				"`SW_swcBulk_minimum()`: SWRC (type %d) is not implemented.",
				swrc_type
			);
			break;
	}

	/* `theta_min` based on user input `ui_sm_min` */
	theta_min_sim = ui_theta_min(
		LogInfo,
		ui_sm_min,
		gravel,
		width,
		sand,
		clay,
		swcBulk_sat,
		swrc_type,
		swrcp,
		// `(Bool) ptf_type == sw_Cosby1984AndOthers` doesn't work for unit test:
		//   error: "no known conversion from 'bool' to 'Bool'"
		ptf_type == sw_Cosby1984AndOthers ? swTRUE : swFALSE,
		_SWCMinVal
	);

	/* `theta_min_sim` must be strictly larger than `theta_min_theoretical` */
	theta_min_sim = fmax(theta_min_sim, theta_min_theoretical + D_DELTA);

	/* Convert from matric [cm/cm] to bulk [cm] */
	return theta_min_sim * width * (1. - gravel);
}



/**
	@brief Check whether selected PTF and SWRC are compatible

	See #ptf2str for implemented PTFs.
	See #swrc2str for implemented SWRCs.

	@param[in] *swrc_name Name of selected SWRC
	@param[in] *ptf_name Name of selected PTF

	@return A logical value indicating if SWRC and PTF are compatible.
*/
Bool check_SWRC_vs_PTF(char *swrc_name, char *ptf_name) {
	Bool res = swFALSE;

	if (!missing((double) encode_str2ptf(ptf_name))) {
		if (
			Str_CompareI(swrc_name, (char *) "Campbell1974") == 0 &&
			(
				Str_CompareI(ptf_name, (char *) "Cosby1984AndOthers") == 0 ||
				Str_CompareI(ptf_name, (char *) "Cosby1984") == 0
			)
		) {
			res = swTRUE;
		}
	}

	return res;
}


/**
	@brief Check Soil Water Retention Curve (SWRC) parameters

	See #swrc2str for implemented SWRCs.

	@param[in] swrc_type Identification number of selected SWRC
	@param[in] *swrcp Vector of SWRC parameters
	@param[in] LogInfo Holds information dealing with logfile output

	@return A logical value indicating if parameters passed the checks.
*/
Bool SWRC_check_parameters(unsigned int swrc_type, double *swrcp,
						   LOG_INFO* LogInfo) {
	Bool res = swFALSE;

	switch (swrc_type) {
		case sw_Campbell1974:
			res = SWRC_check_parameters_for_Campbell1974(swrcp, LogInfo);
			break;

		case sw_vanGenuchten1980:
			res = SWRC_check_parameters_for_vanGenuchten1980(swrcp, LogInfo);
			break;

		case sw_FXW:
			res = SWRC_check_parameters_for_FXW(swrcp, LogInfo);
			break;

		default:
			LogError(
				LogInfo,
				LOGFATAL,
				"Selected SWRC (type %d) is not implemented.",
				swrc_type
			);
			break;
	}

	return res;
}



/**
	@brief Check Campbell's 1974 SWRC parameters

	See `SWRC_SWCtoSWP_Campbell1974()` and `SWRC_SWPtoSWC_Campbell1974()`
	for implementation of Campbell's 1974 SWRC (\cite Campbell1974).

	Campbell1974 has four parameters (three are used for the SWRC):
		- `swrcp[0]` (`psisMatric`): air-entry suction [cm]
		- `swrcp[1]` (previously named `thetasMatric`):
			saturated volumetric water content for the matric component [cm/cm]
		- `swrcp[2]` (`bMatric`): slope of the linear log-log retention curve [-]
		- `swrcp[3]` (`K_sat`): saturated hydraulic conductivity `[cm / day]`

	@param[in] *swrcp Vector of SWRC parameters
	@param[in] LogInfo Holds information dealing with logfile output

	@return A logical value indicating if parameters passed the checks.
*/
Bool SWRC_check_parameters_for_Campbell1974(double *swrcp,
										    LOG_INFO* LogInfo) {
	Bool res = swTRUE;

	if (LE(swrcp[0], 0.0)) {
		res = swFALSE;
		LogError(
			LogInfo,
			LOGWARN,
			"SWRC_check_parameters_for_Campbell1974(): invalid value of "
			"psi(saturated, matric, [cm]) = %f (must > 0)\n",
			swrcp[1]
		);
	}

	if (LE(swrcp[1], 0.0) || GT(swrcp[1], 1.0)) {
		res = swFALSE;
		LogError(
			LogInfo,
			LOGWARN,
			"SWRC_check_parameters_for_Campbell1974(): invalid value of "
			"theta(saturated, matric, [cm/cm]) = %f (must be within 0-1)\n",
			swrcp[1]
		);
	}

	if (ZRO(swrcp[2])) {
		res = swFALSE;
		LogError(
			LogInfo,
			LOGWARN,
			"SWRC_check_parameters_for_Campbell1974(): invalid value of "
			"beta = %f (must be != 0)\n",
			swrcp[2]
		);
	}

	if (LE(swrcp[3], 0.0)) {
		res = swFALSE;
		LogError(
			LogInfo,
			LOGWARN,
			"SWRC_check_parameters_for_Campbell1974(): invalid value of "
			"K_sat = %f (must be > 0)\n",
			swrcp[3]
		);
	}

	return res;
}

/**
	@brief Check van Genuchten 1980 SWRC parameters

	See `SWRC_SWCtoSWP_vanGenuchten1980()` and `SWRC_SWPtoSWC_vanGenuchten1980()`
	for implementation of van Genuchten's 1980 SWRC (\cite vanGenuchten1980).

	"vanGenuchten1980" has five parameters (four are used for the SWRC):
		- `swrcp[0]` (`theta_r`): residual volumetric water content
			of the matric component [cm/cm]
		- `swrcp[1]` (`theta_s`): saturated volumetric water content
			of the matric component [cm/cm]
		- `swrcp[2]` (`alpha`): related to the inverse of air entry suction [cm-1]
		- `swrcp[3]` (`n`): measure of the pore-size distribution [-]
		- `swrcp[4]` (`K_sat`): saturated hydraulic conductivity `[cm / day]`

	@param[in] *swrcp Vector of SWRC parameters
	@param[in] LogInfo Holds information dealing with logfile output

	@return A logical value indicating if parameters passed the checks.
*/
Bool SWRC_check_parameters_for_vanGenuchten1980(double *swrcp,
												LOG_INFO* LogInfo) {
	Bool res = swTRUE;

	if (LE(swrcp[0], 0.0) || GT(swrcp[0], 1.)) {
		res = swFALSE;
		LogError(
			LogInfo,
			LOGWARN,
			"SWRC_check_parameters_for_vanGenuchten1980(): invalid value of "
			"theta(residual, matric, [cm/cm]) = %f (must be within 0-1)\n",
			swrcp[0]
		);
	}

	if (LE(swrcp[1], 0.0) || GT(swrcp[1], 1.)) {
		res = swFALSE;
		LogError(
			LogInfo,
			LOGWARN,
			"SWRC_check_parameters_for_vanGenuchten1980(): invalid value of "
			"theta(saturated, matric, [cm/cm]) = %f (must be within 0-1)\n",
			swrcp[1]
		);
	}

	if (LE(swrcp[1], swrcp[0])) {
		res = swFALSE;
		LogError(
			LogInfo,
			LOGWARN,
			"SWRC_check_parameters_for_vanGenuchten1980(): invalid values for "
			"theta(residual, matric, [cm/cm]) = %f and "
			"theta(saturated, matric, [cm/cm]) = %f "
			"(must be theta_r < theta_s)\n",
			swrcp[0], swrcp[1]
		);
	}

	if (LE(swrcp[2], 0.0)) {
		res = swFALSE;
		LogError(
			LogInfo,
			LOGWARN,
			"SWRC_check_parameters_for_vanGenuchten1980(): invalid value of "
			"alpha([1 / cm]) = %f (must > 0)\n",
			swrcp[2]
		);
	}

	if (LE(swrcp[3], 1.0)) {
		res = swFALSE;
		LogError(
			LogInfo,
			LOGWARN,
			"SWRC_check_parameters_for_vanGenuchten1980(): invalid value of "
			"n = %f (must be > 1)\n",
			swrcp[3]
		);
	}

	if (LE(swrcp[4], 0.0)) {
		res = swFALSE;
		LogError(
			LogInfo,
			LOGWARN,
			"SWRC_check_parameters_for_vanGenuchten1980(): invalid value of "
			"K_sat = %f (must be > 0)\n",
			swrcp[4]
		);
	}

	return res;
}


/**
	@brief Check FXW SWRC parameters

	See `SWRC_SWCtoSWP_FXW()` and `SWRC_SWPtoSWC_FXW()`
	for implementation of FXW's SWRC
	(\cite fredlund1994CGJa, \cite wang2018wrr).

	"FXWJD" is a synonym for the FXW SWRC (\cite wang2018wrr).

	FXW has six parameters (four are used for the SWRC):
		- `swrcp[0]` (`theta_s`): saturated volumetric water content
			of the matric component [cm/cm]
		- `swrcp[1]` (`alpha`): shape parameter [cm-1]
		- `swrcp[2]` (`n`): shape parameter [-]
		- `swrcp[3]` (`m`): shape parameter [-]
		- `swrcp[4]` (`K_sat`): saturated hydraulic conductivity [cm / day]
		- `swrcp[5]` (`L`): tortuosity/connectivity parameter [-]

	FXW SWRC does not include a non-zero residual water content parameter;
	instead, it assumes a pressure head #FXW_h0 of 6.3 x 10^6 cm
	(equivalent to c. -618 MPa) at zero water content
	and a "residual" pressure head #FXW_hr of 1500 cm
	(\cite fredlund1994CGJa, \cite wang2018wrr, \cite rudiyanto2021G).

	Acceptable parameter values are partially based on
	Table 1 in \cite wang2022WRRa.

	@param[in] *swrcp Vector of SWRC parameters
	@param[in] LogInfo Holds information dealing with logfile output

	@return A logical value indicating if parameters passed the checks.
*/
Bool SWRC_check_parameters_for_FXW(double *swrcp, LOG_INFO* LogInfo) {
	Bool res = swTRUE;

	if (LE(swrcp[0], 0.0) || GT(swrcp[0], 1.)) {
		res = swFALSE;
		LogError(
			LogInfo,
			LOGWARN,
			"SWRC_check_parameters_for_FXW(): invalid value of "
			"theta(saturated, matric, [cm/cm]) = %f (must be within 0-1)\n",
			swrcp[0]
		);
	}

	if (LE(swrcp[1], 0.0)) {
		res = swFALSE;
		LogError(
			LogInfo,
			LOGWARN,
			"SWRC_check_parameters_for_FXW(): invalid value of "
			"alpha([1 / cm]) = %f (must be > 0)\n",
			swrcp[1]
		);
	}

	if (LE(swrcp[2], 1.0) || GT(swrcp[2], 10.)) {
		res = swFALSE;
		LogError(
			LogInfo,
			LOGWARN,
			"SWRC_check_parameters_for_FXW(): invalid value of "
			"n = %f (must be within 1-10)\n",
			swrcp[2]
		);
	}

	if (LE(swrcp[3], 0.0) || GT(swrcp[3], 1.5)) {
		res = swFALSE;
		LogError(
			LogInfo,
			LOGWARN,
			"SWRC_check_parameters_for_FXW(): invalid value of "
			"m = %f (must be within 0-1.5)\n",
			swrcp[3]
		);
	}

	if (LE(swrcp[4], 0.0)) {
		res = swFALSE;
		LogError(
			LogInfo,
			LOGWARN,
			"SWRC_check_parameters_for_FXW(): invalid value of "
			"K_sat = %f (must be > 0)\n",
			swrcp[4]
		);
	}

	if (LE(swrcp[5], 0.)) {
		res = swFALSE;
		LogError(
			LogInfo,
			LOGWARN,
			"SWRC_check_parameters_for_FXW(): invalid value of "
			"L = %f (must be > 0)\n",
			swrcp[5]
		);
	}

	return res;
}


/**
	@brief Saxton et al. 2006 PTFs \cite Saxton2006
		to estimate saturated soil water content

	@param[in] sand Sand content of the matric soil (< 2 mm fraction) [g/g]
	@param[in] clay Clay content of the matric soil (< 2 mm fraction) [g/g]
	@param[out] *theta_sat Estimated saturated volumetric water content
		of the matric soil [cm/cm]
	@param[in] LogInfo Holds information dealing with logfile output
*/
void PTF_Saxton2006(
	double *theta_sat,
	double sand,
	double clay,
	LOG_INFO* LogInfo
) {
	double
		OM = 0.,
		theta_33, theta_33t, theta_S33, theta_S33t;

	/* Eq. 2: 33 kPa moisture */
	theta_33t =
		+ 0.299 \
		- 0.251 * sand \
		+ 0.195 * clay \
		+ 0.011 * OM \
		+ 0.006 * sand * OM \
		- 0.027 * clay * OM \
		+ 0.452 * sand * clay;

	theta_33 =
		theta_33t + (1.283 * squared(theta_33t) - 0.374 * theta_33t - 0.015);

	/* Eq. 3: SAT-33 kPa moisture */
	theta_S33t =
		+ 0.078 \
		+ 0.278 * sand \
		+ 0.034 * clay \
		+ 0.022 * OM \
		- 0.018 * sand * OM \
		- 0.027 * clay * OM \
		- 0.584 * sand * clay;

	theta_S33 = theta_S33t + (0.636 * theta_S33t - 0.107);


	/* Eq. 5: saturated moisture */
	*theta_sat = theta_33 + theta_S33 - 0.097 * sand + 0.043;

	if (
		LE(*theta_sat, 0.) ||
		GT(*theta_sat, 1.)
	) {
		LogError(
			LogInfo,
			LOGFATAL,
			"PTF_Saxton2006(): invalid value of "
			"theta(saturated, [cm / cm]) = %f (must be within 0-1)\n",
			*theta_sat
		);
	}



// currently, unused and defunct code:
// (e.g., SW_Site.lyr[n] assignments don't work here anymore!):
#ifdef UNUSED_SAXTON2006
	double R_w, alpha, theta_1500, theta_1500t;

	/* Eq. 1: 1500 kPa moisture */
	theta_1500t =
		+ 0.031 \
		- 0.024 * sand \
		+ 0.487 * clay \
		+ 0.006 * OM \
		+ 0.005 * sand * OM \
		- 0.013 * clay * OM \
		+ 0.068 * sand * clay;

	theta_1500 = theta_1500t + (0.14 * theta_1500t - 0.02);


	/* Eq. 18: slope of logarithmic tension-moisture curve */
	SW_Site.lyr[n]->Saxton2006_lambda =
		(log(theta_33) - log(theta_1500)) / (log(1500.) - log(33.));


	/* Eq. 19: Gravel volume <-> weight fraction */
	alpha = SW_Site.lyr[n]->soilMatric_density / 2.65;

	if (GT(fractionGravel, 0.)) {
		R_w = fractionGravel / (alpha + fractionGravel * (1. - alpha));
	} else {
		R_w = 0.;
	}


	/* Eq. 16: saturated conductivity [cm / day] */
	SW_Site.lyr[n]->Saxton2006_K_sat_matric =
		24. / 10. * 1930. \
		* pow(theta_S - theta_33, 3. - SW_Site.lyr[n]->Saxton2006_lambda);


	/* Eq. 22: saturated conductivity in bulk soils */
	SW_Site.lyr[n]->Saxton2006_K_sat_bulk =
		SW_Site.lyr[n]->Saxton2006_K_sat_matric;

	if (GT(fractionGravel, 0.)) {
		SW_Site.lyr[n]->Saxton2006_fK_gravel =
			(1. - R_w) / (1. - R_w * (1. - 1.5 * alpha));

		SW_Site.lyr[n]->Saxton2006_K_sat_bulk *=
			SW_Site.lyr[n]->Saxton2006_fK_gravel;

	} else {
		SW_Site.lyr[n]->Saxton2006_fK_gravel = 1.;
	}
#endif
}



/**
	@brief Rawls and Brakensiek 1985 PTFs \cite rawls1985WmitE
		to estimate residual soil water content for the Brooks-Corey SWRC
		\cite brooks1964a

	@note This function was previously named "SW_VWCBulkRes".
	@note This function is only well-defined for `clay` values in 5-60%,
		`sand` values in 5-70%, and `porosity` must in 10-<100%.

	@param[in] sand Sand content of the matric soil (< 2 mm fraction) [g/g]
	@param[in] clay Clay content of the matric soil (< 2 mm fraction) [g/g]
	@param[in] porosity Pore space of the matric soil (< 2 mm fraction) [cm3/cm3]
	@param[out] *theta_min Estimated residual volumetric water content
		of the matric soil [cm/cm]
	@param[in] LogInfo Holds information dealing with logfile output
*/
void PTF_RawlsBrakensiek1985(
	double *theta_min,
	double sand,
	double clay,
	double porosity,
	LOG_INFO* LogInfo
) {
	if (
		GE(clay, 0.05) && LE(clay, 0.6) &&
		GE(sand, 0.05) && LE(sand, 0.7) &&
		GE(porosity, 0.1) && LT(porosity, 1.)
	) {
		sand *= 100.;
		clay *= 100.;
		/* Note: the equation requires sand and clay in units of [100 * g / g];
			porosity, however, must be in units of [cm3 / cm3]
		*/
		*theta_min = fmax(
			0.,
			- 0.0182482 \
			+ 0.00087269 * sand \
			+ 0.00513488 * clay \
			+ 0.02939286 * porosity \
			- 0.00015395 * squared(clay) \
			- 0.0010827 * sand * porosity \
			- 0.00018233 * squared(clay) * squared(porosity) \
			+ 0.00030703 * squared(clay) * porosity \
			- 0.0023584 * squared(porosity) * clay
		);

	} else {
			LogError(
			LogInfo,
			LOGWARN,
			"`PTF_RawlsBrakensiek1985()`: sand, clay, or porosity out of valid range."
		);

		*theta_min = SW_MISSING;
	}
}



/**
  @brief Estimate soil density of the whole soil (bulk).

  Based on equation 20 from Saxton. @cite Saxton2006.

  Similarly, estimate soil bulk density from `theta_sat` with
  `2.65 * (1. - theta_sat * (1. - fractionGravel))`.
*/
RealD calculate_soilBulkDensity(RealD matricDensity, RealD fractionGravel) {
	return matricDensity * (1. - fractionGravel) + fractionGravel * 2.65;
}


/**
  @brief Estimate soil density of the matric soil component.

  Based on equation 20 from Saxton. @cite Saxton2006
*/
RealD calculate_soilMatricDensity(RealD bulkDensity, RealD fractionGravel,
								  LOG_INFO* LogInfo) {
	double res;

	if (EQ(fractionGravel, 1.)) {
		res = 0.;
	} else {
		res = (bulkDensity - fractionGravel * 2.65) / (1. - fractionGravel);

		if (LT(res, 0.)) {
			LogError(
				LogInfo,
				LOGFATAL,
				"bulkDensity (%f) is lower than expected "
				"(density of coarse fragments = %f [g/cm3] "
				"based on %f [%%] coarse fragments).\n",
				bulkDensity,
				fractionGravel * 2.65,
				fractionGravel
			);
		}
	}

	return res;
}




/**
  @brief Count soil layers with bare-soil evaporation potential

  The count stops at first layer with 0.

  @param[in] evap_coeff Struct list of type SW_LAYER_INFO holding information about
	every soil layer in the simulation
  @param[in] n_layers Number of layers of soil within the simulation run
*/
LyrIndex nlayers_bsevap(RealD *evap_coeff, LyrIndex n_layers) {
	LyrIndex s, n = 0;

	ForEachSoilLayer(s, n_layers)
	{
		if (GT(evap_coeff[s], 0.0))
		{
			n++;
		} else{
			break;
		}
	}

	return n;
}

/**
  @brief Count soil layers with roots potentially extracting water for
         transpiration

  The count stops at first layer with 0 per vegetation type.

  @param[in] n_layers Number of layers of soil within the simulation run
  @param[in] transp_coeff Prop. of total transp from this layer
  @param[out] n_transp_lyrs Index of the deepest transp. region
*/
void nlayers_vegroots(LyrIndex n_layers, LyrIndex n_transp_lyrs[],
					  RealD transp_coeff[][MAX_LAYERS]) {
	LyrIndex s;
	int k;

	ForEachVegType(k)
	{
		n_transp_lyrs[k] = 0;

		ForEachSoilLayer(s, n_layers)
		{
			if (GT(transp_coeff[k][s], 0.0)) {
				n_transp_lyrs[k]++;
			} else {
				break;
			}
		}
	}
}


/**
  @brief Set `deep_lyr` to indicate that deep drainage is being simulated

  @param[in,out] SW_Site Struct of type SW_SITE describing the simulated site
*/
void add_deepdrain_layer(SW_SITE* SW_Site) {

	if (SW_Site->deepdrain) {
		/* total percolation from last layer == deep drainage */
		SW_Site->deep_lyr = SW_Site->n_layers - 1; /* deep_lyr is base0 */
	} else {
		SW_Site->deep_lyr = 0;
	}
}

/*static void _clear_layer(LyrIndex n) {
 ---------------------------------------------------

 memset(SW_Site.lyr[n], 0, sizeof(SW_LAYER_INFO));

 }*/

/* =================================================== */
/* =================================================== */
/*             Global Function Definitions             */
/* --------------------------------------------------- */

/**
	@brief Initialized memory for SW_Site
	/note that an initializer that is called during execution
			(better called clean() or something) will need to free all allocated
			memory first before clearing structure.
*/
void SW_SIT_construct(SW_SITE* SW_Site) {
	/* =================================================== */

	memset(SW_Site, 0, sizeof(SW_SITE));
	SW_SIT_init_counts(SW_Site);
}

/**
@brief Reads in file for input values.

@param[in,out] SW_Site Struct of type SW_SITE describing the simulated site
@param[in] InFiles Array of program in/output files
@param[out] SW_Carbon Struct of type SW_CARBON holding all CO2-related data
@param[in] LogInfo Holds information dealing with logfile output
*/
void SW_SIT_read(SW_SITE* SW_Site, char *InFiles[],
				 SW_CARBON* SW_Carbon, LOG_INFO* LogInfo) {
	/* =================================================== */
	/* 5-Feb-2002 (cwb) Removed rgntop requirement in
	 *    transpiration regions section of input
	 */
	FILE *f;
	int lineno = 0, x,
		rgnlow, /* lower layer of region */
		region; /* transp region definition number */
	#ifdef SWDEBUG
	int debug = 0;
	#endif
	LyrIndex r;
	Bool too_many_regions = swFALSE;
	RealD tmp;
	char inbuf[MAX_FILENAMESIZE];

	/* note that Files.read() must be called prior to this. */
	char *MyFileName = InFiles[eSite];

	f = OpenFile(MyFileName, "r", LogInfo);

	while (GetALine(f, inbuf)) {
		switch (lineno) {
		case 0:
			SW_Site->_SWCMinVal = atof(inbuf);
			break;
		case 1:
			SW_Site->_SWCInitVal = atof(inbuf);
			break;
		case 2:
			SW_Site->_SWCWetVal = atof(inbuf);
			break;
		case 3:
			SW_Site->reset_yr = itob(atoi(inbuf));
			break;
		case 4:
			SW_Site->deepdrain = itob(atoi(inbuf));
			break;
		case 5:
			SW_Site->pet_scale = atof(inbuf);
			break;
		case 6:
			SW_Site->percentRunoff = atof(inbuf);
			break;
		case 7:
			SW_Site->percentRunon = atof(inbuf);
			break;
		case 8:
			SW_Site->TminAccu2 = atof(inbuf);
			break;
		case 9:
			SW_Site->TmaxCrit = atof(inbuf);
			break;
		case 10:
			SW_Site->lambdasnow = atof(inbuf);
			break;
		case 11:
			SW_Site->RmeltMin = atof(inbuf);
			break;
		case 12:
			SW_Site->RmeltMax = atof(inbuf);
			break;
		case 13:
			SW_Site->slow_drain_coeff = atof(inbuf);
			break;
		case 14:
			SW_Site->evap.xinflec = atof(inbuf);
			break;
		case 15:
			SW_Site->evap.slope = atof(inbuf);
			break;
		case 16:
			SW_Site->evap.yinflec = atof(inbuf);
			break;
		case 17:
			SW_Site->evap.range = atof(inbuf);
			break;
		case 18:
			SW_Site->transp.xinflec = atof(inbuf);
			break;
		case 19:
			SW_Site->transp.slope = atof(inbuf);
			break;
		case 20:
			SW_Site->transp.yinflec = atof(inbuf);
			break;
		case 21:
			SW_Site->transp.range = atof(inbuf);
			break;
		case 22:
			// longitude is currently not used by the code, but may be used in the future
			// it is present in the `siteparam.in` input file to completely document
			// site location
			SW_Site->longitude = atof(inbuf) * deg_to_rad;
			break;
		case 23:
			SW_Site->latitude = atof(inbuf) * deg_to_rad;
			break;
		case 24:
			SW_Site->altitude = atof(inbuf);
			break;
		case 25:
			SW_Site->slope = atof(inbuf) * deg_to_rad;
			break;
		case 26:
			tmp = atof(inbuf);
			SW_Site->aspect = missing(tmp) ? tmp : tmp * deg_to_rad;
			break;
		case 27:
			SW_Site->bmLimiter = atof(inbuf);
			break;
		case 28:
			SW_Site->t1Param1 = atof(inbuf);
			break;
		case 29:
			SW_Site->t1Param2 = atof(inbuf);
			break;
		case 30:
			SW_Site->t1Param3 = atof(inbuf);
			break;
		case 31:
			SW_Site->csParam1 = atof(inbuf);
			break;
		case 32:
			SW_Site->csParam2 = atof(inbuf);
			break;
		case 33:
			SW_Site->shParam = atof(inbuf);
			break;
		case 34:
			SW_Site->Tsoil_constant = atof(inbuf);
			break;
		case 35:
			SW_Site->stDeltaX = atof(inbuf);
			break;
		case 36:
			SW_Site->stMaxDepth = atof(inbuf);
			break;
		case 37:
			SW_Site->use_soil_temp = itob(atoi(inbuf));
			break;
		case 38:
			SW_Carbon->use_bio_mult = itob(atoi(inbuf));
			#ifdef SWDEBUG
			if (debug) swprintf("'SW_SIT_read': use_bio_mult = %d\n", SW_Carbon->use_bio_mult);
			#endif
			break;
		case 39:
			SW_Carbon->use_wue_mult = itob(atoi(inbuf));
			#ifdef SWDEBUG
			if (debug) swprintf("'SW_SIT_read': use_wue_mult = %d\n", SW_Carbon->use_wue_mult);
			#endif
			break;
		case 40:
			strcpy(SW_Carbon->scenario, inbuf);
			#ifdef SWDEBUG
			if (debug) swprintf("'SW_SIT_read': scenario = %s\n", SW_Carbon->scenario);
			#endif
			break;
		case 41:
			SW_Site->type_soilDensityInput = atoi(inbuf);
			break;
		case 42:
			strcpy(SW_Site->site_swrc_name, inbuf);
			SW_Site->site_swrc_type =
							encode_str2swrc(SW_Site->site_swrc_name, LogInfo);
			break;
		case 43:
			strcpy(SW_Site->site_ptf_name, inbuf);
			SW_Site->site_ptf_type = encode_str2ptf(SW_Site->site_ptf_name);
			break;
		case 44:
			SW_Site->site_has_swrcp = itob(atoi(inbuf));
			break;

		default:
			if (lineno > 44 + MAX_TRANSP_REGIONS)
				break; /* skip extra lines */

			if (MAX_TRANSP_REGIONS < SW_Site->n_transp_rgn) {
				too_many_regions = swTRUE;
				goto Label_End_Read;
			}
			x = sscanf(inbuf, "%d %d", &region, &rgnlow);
			if (x < 2 || region < 1 || rgnlow < 1) {
				CloseFile(&f, LogInfo);
				LogError(LogInfo, LOGFATAL, "%s : Bad record %d.\n", MyFileName, lineno);
			}
			SW_Site->_TranspRgnBounds[region - 1] = (LyrIndex) (rgnlow - 1);
			SW_Site->n_transp_rgn++;
		}

		lineno++;
	}

	Label_End_Read:

	CloseFile(&f, LogInfo);

	if (LT(SW_Site->percentRunoff, 0.) || GT(SW_Site->percentRunoff, 1.)) {
		LogError(
			LogInfo,
			LOGFATAL,
			"%s : proportion of ponded surface water removed as daily"
			"runoff = %f (value ranges between 0 and 1)\n",
			MyFileName, SW_Site->percentRunoff
		);
	}

	if (LT(SW_Site->percentRunon, 0.)) {
		LogError(
			LogInfo,
			LOGFATAL,
			"%s : proportion of water that arrives at surface added "
			"as daily runon = %f (value ranges between 0 and +inf)\n",
			MyFileName, SW_Site->percentRunon
		);
	}

	if (too_many_regions) {
		LogError(
			LogInfo,
			LOGFATAL,
			"%s : Number of transpiration regions"
			" exceeds maximum allowed (%d > %d)\n",
			MyFileName, SW_Site->n_transp_rgn, MAX_TRANSP_REGIONS
		);
	}

	/* check for any discontinuities (reversals) in the transpiration regions */
	for (r = 1; r < SW_Site->n_transp_rgn; r++) {
		if (SW_Site->_TranspRgnBounds[r - 1] >= SW_Site->_TranspRgnBounds[r]) {
			LogError(
				LogInfo,
				LOGFATAL,
				"%s : Discontinuity/reversal in transpiration regions.\n",
				InFiles[eSite]
			);
		}
	}
}



/** Reads soil layers and soil properties from input file

	@param[in,out] SW_Site Struct of type SW_SITE describing the simulated site
	@param[in] InFiles Array of program in/output files
	@param[in] LogInfo Holds information dealing with logfile output

	@note Previously, the function was static and named `_read_layers()`.
*/
void SW_LYR_read(SW_SITE* SW_Site, char *InFiles[], LOG_INFO* LogInfo) {
	/* =================================================== */
	/* 5-Feb-2002 (cwb) removed dmin requirement in input file */

	FILE *f;
	LyrIndex lyrno;
	int x, k;
	RealF dmin = 0.0, dmax, evco, trco_veg[NVEGTYPES], psand, pclay, soildensity, imperm,
		soiltemp, f_gravel;
	char inbuf[MAX_FILENAMESIZE];

	/* note that Files.read() must be called prior to this. */
	char *MyFileName = InFiles[eLayers];

	f = OpenFile(MyFileName, "r", LogInfo);

	while (GetALine(f, inbuf)) {
		lyrno = SW_Site->n_layers++;

		x = sscanf(
			inbuf,
			"%f %f %f %f %f %f %f %f %f %f %f %f",
			&dmax,
			&soildensity,
			&f_gravel,
			&evco,
			&trco_veg[SW_GRASS], &trco_veg[SW_SHRUB], &trco_veg[SW_TREES], &trco_veg[SW_FORBS],
			&psand,
			&pclay,
			&imperm,
			&soiltemp
		);

		/* Check that we have 12 values per layer */
		/* Adjust number if new variables are added */
		if (x != 12) {
			CloseFile(&f, LogInfo);
			LogError(
				LogInfo,
				LOGFATAL,
				"%s : Incomplete record %d.\n",
				MyFileName, lyrno + 1
			);
		}

		SW_Site->width[lyrno] = dmax - dmin;

		/* checks for valid values now carried out by `SW_SIT_init_run()` */

		dmin = dmax;
		SW_Site->fractionVolBulk_gravel[lyrno] = f_gravel;
		SW_Site->soilDensityInput[lyrno] = soildensity;
		SW_Site->evap_coeff[lyrno] = evco;

		ForEachVegType(k)
		{
			SW_Site->transp_coeff[k][lyrno] = trco_veg[k];
		}

		SW_Site->fractionWeightMatric_sand[lyrno] = psand;
		SW_Site->fractionWeightMatric_clay[lyrno] = pclay;
		SW_Site->impermeability[lyrno] = imperm;
		SW_Site->avgLyrTempInit[lyrno] = soiltemp;

		if (lyrno >= MAX_LAYERS) {
			CloseFile(&f, LogInfo);
			LogError(
				LogInfo,
				LOGFATAL,
				"%s : Too many layers specified (%d).\n"
				"Maximum number of layers is %d\n",
				MyFileName, lyrno + 1, MAX_LAYERS
			);
		}
	}

	CloseFile(&f, LogInfo);
}




/**
  @brief Creates soil layers based on function arguments (instead of reading
    them from an input file as _read_layers() does)

  @param[in,out] SW_VegProd Struct of type SW_VEGPROD describing surface
	cover conditions in the simulation
  @param[in,out] SW_Site Struct of type SW_SITE describing the simulated site
  @param[in] nlyrs The number of soil layers to create.
  @param[in] dmax Array of size \p nlyrs for depths [cm] of each soil layer
    measured from the surface
  @param[in] bd Array of size \p nlyrs of soil bulk density [g/cm3]
  @param[in] f_gravel Array of size \p nlyrs for volumetric gravel content [v/v]
  @param[in] evco Array of size \p nlyrs with bare-soil evaporation coefficients
    [0, 1] that sum up to 1.
  @param[in] trco_grass Array of size \p nlyrs with transpiration coefficients
    for grasses [0, 1] that sum up to 1.
  @param[in] trco_shrub Array of size \p nlyrs with transpiration coefficients
    for shrubs [0, 1] that sum up to 1.
  @param[in] trco_tree Array of size \p nlyrs with transpiration coefficients
    for trees [0, 1] that sum up to 1.
  @param[in] trco_forb Array of size \p nlyrs with transpiration coefficients
    for forbs [0, 1] that sum up to 1.
  @param[in] psand Array of size \p nlyrs for sand content of the
    soil matrix [g3 / g3]
  @param[in] pclay Array of size \p nlyrs for clay content of the
    soil matrix [g3 / g3]
  @param[in] imperm Array of size \p nlyrs with impermeability coefficients
    [0, 1]
  @param[in] soiltemp Array of size \p nlyrs with initial soil temperature [C]
  @param nRegions The number of transpiration regions to create. Must be between
    1 and \ref MAX_TRANSP_REGIONS.
  @param[in] regionLowerBounds Array of size \p nRegions containing the lower
    depth [cm] of each region in ascending (in value) order. If you think about
    this from the perspective of soil, it would mean the shallowest bound is at
    `lowerBounds[0]`.
  @param[in] LogInfo Holds information dealing with logfile output

  @sideeffect After deleting any previous data in the soil layer array
    SW_Site.lyr, it creates new soil layers based on the argument inputs.

  @note
    - This function is a modified version of the function _read_layers() in
      SW_Site.c.
    - Soil moisture values must be properly initialized before running a
      simulation after this function has set soil layers, e.g.,
      SW_SWC_init_run()
*/
void set_soillayers(SW_VEGPROD* SW_VegProd, SW_SITE* SW_Site,
	LyrIndex nlyrs, RealF *dmax, RealF *bd, RealF *f_gravel, RealF *evco,
	RealF *trco_grass, RealF *trco_shrub, RealF *trco_tree, RealF *trco_forb,
	RealF *psand, RealF *pclay, RealF *imperm, RealF *soiltemp, int nRegions,
	RealD *regionLowerBounds,
	LOG_INFO* LogInfo)
{

  RealF dmin = 0.0;

  LyrIndex lyrno;
  unsigned int i, k;

  // De-allocate and delete previous soil layers and reset counters
  SW_SIT_init_counts(SW_Site);

  // Create new soil
  for (i = 0; i < nlyrs; i++)
  {
    // Increment the number of soil layers
    lyrno = SW_Site->n_layers++;

    SW_Site->width[lyrno] = dmax[i] - dmin;
    dmin = dmax[i];
    SW_Site->soilDensityInput[lyrno] = bd[i];
    SW_Site->type_soilDensityInput = SW_BULK;
    SW_Site->fractionVolBulk_gravel[lyrno] = f_gravel[i];
    SW_Site->evap_coeff[lyrno] = evco[i];

    ForEachVegType(k)
    {
      switch (k)
      {
        case SW_TREES:
          SW_Site->transp_coeff[k][lyrno] = trco_tree[i];
          break;
        case SW_SHRUB:
          SW_Site->transp_coeff[k][lyrno] = trco_shrub[i];
          break;
        case SW_FORBS:
          SW_Site->transp_coeff[k][lyrno] = trco_forb[i];
          break;
        case SW_GRASS:
          SW_Site->transp_coeff[k][lyrno] = trco_grass[i];
          break;
      }
    }

    SW_Site->fractionWeightMatric_sand[lyrno] = psand[i];
    SW_Site->fractionWeightMatric_clay[lyrno] = pclay[i];
    SW_Site->impermeability[lyrno] = imperm[i];
    SW_Site->avgLyrTempInit[lyrno] = soiltemp[i];
  }


  // Guess soil transpiration regions
  derive_soilRegions(SW_Site, nRegions, regionLowerBounds, LogInfo);

  // Re-initialize site parameters based on new soil layers
  SW_SIT_init_run(SW_VegProd, SW_Site, LogInfo);
}



/**
  @brief Resets soil regions based on input parameters.

  @param[in,out] SW_Site Struct of type SW_SITE describing the simulated site
  @param[in] nRegions The number of transpiration regions to create. Must be between
    1 and \ref MAX_TRANSP_REGIONS.
  @param[in] regionLowerBounds Array of size \p nRegions containing the lower
    depth [cm] of each region in ascending (in value) order. If you think about
    this from the perspective of soil, it would mean the shallowest bound is at
    `lowerBounds[0]`.
  @param[in] LogInfo Holds information dealing with logfile output

  @sideeffect
    \ref SW_SITE._TranspRgnBounds and \ref SW_SITE.n_transp_rgn will be
    derived from the input and from the soil information.

  @note
  - \p nRegions does NOT determine how many regions will be derived. It only
    defines the size of the \p regionLowerBounds array. For example, if your
    input parameters are `(4, { 10, 20, 40 })`, but there is a soil layer from
    41 to 60 cm, it will be placed in `_TranspRgnBounds[4]`.
*/
void derive_soilRegions(SW_SITE* SW_Site, int nRegions,
						RealD *regionLowerBounds, LOG_INFO* LogInfo){
	int i, j;
	RealD totalDepth = 0;
	LyrIndex layer, UNDEFINED_LAYER = 999;

	/* ------------- Error checking --------------- */
	if(nRegions < 1 || nRegions > MAX_TRANSP_REGIONS){
		LogError(LogInfo, LOGFATAL, "derive_soilRegions: invalid number of regions (%d)\n", nRegions);
		return;
	}

	/* --------------- Clear out the array ------------------ */
	for(i = 0; i < MAX_TRANSP_REGIONS; ++i){
		// Setting bounds to a ridiculous number so we
		// know how many get set.
		SW_Site->_TranspRgnBounds[i] = UNDEFINED_LAYER;
	}

	/* ----------------- Derive Regions ------------------- */
	// Loop through the regions the user wants to derive
	layer = 0; // SW_Site.lyr is base0-indexed
	totalDepth = 0;
	for(i = 0; i < nRegions; ++i){
		SW_Site->_TranspRgnBounds[i] = layer;
		// Find the layer that pushes us out of the region.
		// It becomes the bound.
		while(totalDepth < regionLowerBounds[i] &&
		      layer < SW_Site->n_layers &&
		      sum_across_vegtypes(SW_Site->transp_coeff, layer)) {
			totalDepth += SW_Site->width[layer];
			SW_Site->_TranspRgnBounds[i] = layer;
			layer++;
		}
	}

	/* -------------- Check for duplicates -------------- */
	for(i = 0; i < nRegions - 1; ++i){
		// If there is a duplicate bound we will remove it by left shifting the
		// array, overwriting the duplicate.
		if(SW_Site->_TranspRgnBounds[i] == SW_Site->_TranspRgnBounds[i + 1]){
			for(j = i + 1; j < nRegions - 1; ++j){
				SW_Site->_TranspRgnBounds[j] = SW_Site->_TranspRgnBounds[j + 1];
			}
			SW_Site->_TranspRgnBounds[MAX_TRANSP_REGIONS - 1] = UNDEFINED_LAYER;
		}
	}

	/* -------------- Derive n_transp_rgn --------------- */
	SW_Site->n_transp_rgn = 0;
	while(SW_Site->n_transp_rgn < MAX_TRANSP_REGIONS &&
	      SW_Site->_TranspRgnBounds[SW_Site->n_transp_rgn] != UNDEFINED_LAYER) {
		SW_Site->n_transp_rgn++;
	}
}



/** Obtain soil water retention curve parameters from disk
 *
 * @param[in,out] SW_Site Struct of type SW_SITE describing the simulated site
 * @param[in] InFiles Array of program in/output files
 * @param[in] LogInfo Holds information dealing with logfile output
 *
*/
void SW_SWRC_read(SW_SITE* SW_Site, char *InFiles[], LOG_INFO* LogInfo) {

	/* Don't read values from disk if they will be estimated via a PTF */
	if (!SW_Site->site_has_swrcp) return;

	FILE *f;
	LyrIndex lyrno = 0, k;
	int x;
	RealF tmp_swrcp[SWRC_PARAM_NMAX];
	char inbuf[MAX_FILENAMESIZE];

	/* note that Files.read() must be called prior to this. */
	char *MyFileName = InFiles[eSWRCp];

	f = OpenFile(MyFileName, "r", LogInfo);

	while (GetALine(f, inbuf)) {
		x = sscanf(
			inbuf,
			"%f %f %f %f %f %f",
			&tmp_swrcp[0],
			&tmp_swrcp[1],
			&tmp_swrcp[2],
			&tmp_swrcp[3],
			&tmp_swrcp[4],
			&tmp_swrcp[5]
		);

		/* Note: `SW_SIT_init_run()` will call function to check for valid values */

		/* Check that we have n = `SWRC_PARAM_NMAX` values per layer */
		if (x != SWRC_PARAM_NMAX) {
			CloseFile(&f, LogInfo);
			LogError(
				LogInfo,
				LOGFATAL,
				"%s : Bad number of SWRC parameters %d -- must be %d.\n",
				"Site", x, SWRC_PARAM_NMAX
			);
		}

		/* Check that we are within `SW_Site.n_layers` */
		if (lyrno >= SW_Site->n_layers) {
			CloseFile(&f, LogInfo);
			LogError(
				LogInfo,
				LOGFATAL,
				"%s : Number of layers with SWRC parameters (%d) "
				"must match number of soil layers (%d)\n",
				"Site", lyrno + 1, SW_Site->n_layers
			);
		}

		/* Copy values into structure */
		for (k = 0; k < SWRC_PARAM_NMAX; k++) {
			SW_Site->swrcp[lyrno][k] = tmp_swrcp[k];
		}

		lyrno++;
	}

	CloseFile(&f, LogInfo);
}


/**
	@brief Derive and check soil properties from inputs

	Bulk refers to the whole soil,
	i.e., including the rock/gravel component (coarse fragments),
	whereas matric refers to the < 2 mm fraction.

	Internally, SOILWAT2 calculates based on bulk soil, i.e., the whole soil.
	However, sand and clay inputs are expected to represent the soil matric,
	i.e., the < 2 mm fraction.

	sand + clay + silt must equal one.
	Fraction of silt is calculated: 1 - (sand + clay).

	@param[in,out] SW_VegProd Struct of type SW_VEGPROD describing surface
		cover conditions in the simulation
	@param[in,out] SW_Site Struct of type SW_SITE describing the simulated site
	@param[in] LogInfo Holds information dealing with logfile output

	@sideeffect Values stored in global variable `SW_Site`.
*/
void SW_SIT_init_run(SW_VEGPROD* SW_VegProd, SW_SITE* SW_Site, LOG_INFO* LogInfo) {
	/* =================================================== */
	/* potentially this routine can be called whether the
	 * layer data came from a file or a function call which
	 * still requires initialization.
	 */
	/* 5-Mar-2002 (cwb) added normalization for ev and tr coefficients */
	/* 1-Oct-03 (cwb) removed sum_evap_coeff and sum_transp_coeff  */

	LyrIndex s, r, curregion;
	int k, flagswpcrit = 0;
	RealD
		evsum = 0., trsum_veg[NVEGTYPES] = {0.}, tmp;

	#ifdef SWDEBUG
	int debug = 0;
	#endif


	/* Determine number of layers with potential for
		 bare-soil evaporation and transpiration */
	SW_Site->n_evap_lyrs = nlayers_bsevap(SW_Site->evap_coeff, SW_Site->n_layers);
	nlayers_vegroots(SW_Site->n_layers, SW_Site->n_transp_lyrs, SW_Site->transp_coeff);


	/* Manage deep drainage */
	add_deepdrain_layer(SW_Site);


	/* Check compatibility between selected SWRC and PTF */
	if (!SW_Site->site_has_swrcp) {
		if (!check_SWRC_vs_PTF(SW_Site->site_swrc_name, SW_Site->site_ptf_name)) {
			LogError(
				LogInfo,
				LOGFATAL,
				"Selected PTF '%s' is incompatible with selected SWRC '%s'\n",
				SW_Site->site_ptf_name,
				SW_Site->site_swrc_name
			);
		}
	}


	/* Loop over soil layers check variables and calculate parameters */
	ForEachSoilLayer(s, SW_Site->n_layers)
	{

		/* Copy site-level SWRC/PTF information to each layer:
		   We currently allow specifying one SWRC/PTF for a site for all layers;
		   remove in the future if we allow SWRC/PTF to vary by soil layer
		*/
		SW_Site->swrc_type[s] = SW_Site->site_swrc_type;
		SW_Site->ptf_type[s] = SW_Site->site_ptf_type;


		/* Check soil properties for valid values */
		if (!SW_check_soil_properties(SW_Site, s, LogInfo)) {
			LogError(
				LogInfo,
				LOGFATAL,
				"Invalid soil properties in layer %d.\n",
				s + 1
			);
		}


		/* Update soil density depending on inputs */
		switch (SW_Site->type_soilDensityInput) {

			case SW_BULK:
				SW_Site->soilBulk_density[s] = SW_Site->soilDensityInput[s];

				SW_Site->soilMatric_density[s] = calculate_soilMatricDensity(
					SW_Site->soilBulk_density[s],
					SW_Site->fractionVolBulk_gravel[s],
					LogInfo
				);

				break;

			case SW_MATRIC:
				SW_Site->soilMatric_density[s] = SW_Site->soilDensityInput[s];

				SW_Site->soilBulk_density[s] = calculate_soilBulkDensity(
					SW_Site->soilMatric_density[s],
					SW_Site->fractionVolBulk_gravel[s]
				);

				break;

		default:
			LogError(
				LogInfo,
				LOGFATAL,
				"Soil density type not recognized",
				SW_Site->type_soilDensityInput
			);
		}




		if (!SW_Site->site_has_swrcp) {
			/* Use pedotransfer function PTF
			   to estimate parameters of soil water retention curve (SWRC) for layer.
			   If `has_swrcp`, then parameters already obtained from disk
			   by `SW_SWRC_read()`
			*/
			SWRC_PTF_estimate_parameters(
				SW_Site->ptf_type[s],
				SW_Site->swrcp[s],
				SW_Site->fractionWeightMatric_sand[s],
				SW_Site->fractionWeightMatric_clay[s],
				SW_Site->fractionVolBulk_gravel[s],
				SW_Site->soilBulk_density[s],
				LogInfo
			);
		}

		/* Check parameters of selected SWRC */
		if (!SWRC_check_parameters(SW_Site->swrc_type[s],
									SW_Site->swrcp[s], LogInfo)) {
			LogError(
				LogInfo,
				LOGFATAL,
				"Checks of parameters for SWRC '%s' in layer %d failed.",
				swrc2str[SW_Site->swrc_type[s]],
				s + 1
			);
		}

		/* Calculate SWC at field capacity and at wilting point */
		SW_Site->swcBulk_fieldcap[s] = SW_SWRC_SWPtoSWC(0.333, SW_Site, s, LogInfo);
		SW_Site->swcBulk_wiltpt[s] = SW_SWRC_SWPtoSWC(15., SW_Site, s, LogInfo);

		/* Calculate lower SWC limit of bare-soil evaporation
			as `max(0.5 * wiltpt, SWC@hygroscopic)`
			Notes:
				- `0.5 * wiltpt` is the E_soil limit from FAO-56 (Allen et al. 1998)
					which may correspond to unrealistically extreme SWP
				- `SWC at hygroscopic point` (-10 MPa; e.g., Porporato et al. 2001)
					describes "air-dry" soil
				- Also make sure that `>= swc_min`, see below
		*/
		SW_Site->swcBulk_halfwiltpt[s] = fmax(
			0.5 * SW_Site->swcBulk_wiltpt[s],
			SW_SWRC_SWPtoSWC(100., SW_Site, s, LogInfo)
		);


		/* Extract or estimate additional properties */
		SW_Site->swcBulk_saturated[s] = SW_swcBulk_saturated(
			SW_Site->swrc_type[s],
			SW_Site->swrcp[s],
			SW_Site->fractionVolBulk_gravel[s],
			SW_Site->width[s],
			SW_Site->ptf_type[s],
			SW_Site->fractionWeightMatric_sand[s],
			SW_Site->fractionWeightMatric_clay[s],
			LogInfo
		);

		SW_Site->swcBulk_min[s] = SW_swcBulk_minimum(
			SW_Site->swrc_type[s],
			SW_Site->swrcp[s],
			SW_Site->fractionVolBulk_gravel[s],
			SW_Site->width[s],
			SW_Site->ptf_type[s],
			SW_Site->_SWCMinVal,
			SW_Site->fractionWeightMatric_sand[s],
			SW_Site->fractionWeightMatric_clay[s],
			SW_Site->swcBulk_saturated[s],
			SW_Site->_SWCMinVal,
			LogInfo
		);


		/* Calculate wet limit of SWC for what inputs defined as wet */
		SW_Site->swcBulk_wet[s] = GE(SW_Site->_SWCWetVal, 1.0) ?
			SW_SWRC_SWPtoSWC(SW_Site->_SWCWetVal, SW_Site, s, LogInfo) :
			SW_Site->_SWCWetVal * SW_Site->width[s];

		/* Calculate initial SWC based on inputs */
		SW_Site->swcBulk_init[s] = GE(SW_Site->_SWCInitVal, 1.0) ?
			SW_SWRC_SWPtoSWC(SW_Site->_SWCInitVal, SW_Site, s, LogInfo) :
			SW_Site->_SWCInitVal * SW_Site->width[s];


		/* test validity of values */
		if (LT(SW_Site->swcBulk_init[s], SW_Site->swcBulk_min[s])) {
			LogError(
				LogInfo, LOGFATAL,
				"%s : Layer %d\n"
				"  calculated `swcBulk_init` (%.4f cm) <= `swcBulk_min` (%.4f cm).\n"
				"  Recheck parameters and try again.\n",
				"Site", s + 1, SW_Site->swcBulk_init[s], SW_Site->swcBulk_min[s]
			);
		}

		if (LT(SW_Site->swcBulk_wiltpt[s], SW_Site->swcBulk_min[s])) {
			LogError(
				LogInfo, LOGFATAL,
				"%s : Layer %d\n"
				"  calculated `swcBulk_wiltpt` (%.4f cm) <= `swcBulk_min` (%.4f cm).\n"
				"  Recheck parameters and try again.\n",
				"Site", s + 1, SW_Site->swcBulk_wiltpt[s], SW_Site->swcBulk_min[s]
			);
		}

		if (LT(SW_Site->swcBulk_halfwiltpt[s], SW_Site->swcBulk_min[s])) {
			LogError(
				LogInfo, LOGWARN,
				"%s : Layer %d\n"
				"  calculated `swcBulk_halfwiltpt` (%.4f cm / %.2f MPa)\n"
				"          <= `swcBulk_min` (%.4f cm / %.2f MPa).\n"
				"  `swcBulk_halfwiltpt` was set to `swcBulk_min`.\n",
				"Site", s + 1,
				SW_Site->swcBulk_halfwiltpt[s],
				-0.1 * SW_SWRC_SWCtoSWP(SW_Site->swcBulk_halfwiltpt[s], SW_Site, s, LogInfo),
				SW_Site->swcBulk_min[s],
				-0.1 * SW_SWRC_SWCtoSWP(SW_Site->swcBulk_min[s], SW_Site, s, LogInfo)
			);

			SW_Site->swcBulk_halfwiltpt[s] = SW_Site->swcBulk_min[s];
		}

		if (LE(SW_Site->swcBulk_wet[s], SW_Site->swcBulk_min[s])) {
			LogError(
				LogInfo, LOGFATAL,
				"%s : Layer %d\n"
				"  calculated `swcBulk_wet` (%.4f cm) <= `swcBulk_min` (%.4f cm).\n"
				"  Recheck parameters and try again.\n",
				"Site", s + 1, SW_Site->swcBulk_wet[s], SW_Site->swcBulk_min[s]
			);
		}


		#ifdef SWDEBUG
		if (debug) {
			swprintf(
				"L[%d] swcmin=%f = swpmin=%f\n",
				s,
				SW_Site->swcBulk_min[s],
				SW_SWRC_SWCtoSWP(SW_Site->swcBulk_min[s], SW_Site, s, LogInfo)
			);

			swprintf(
				"L[%d] SWC(HalfWiltpt)=%f = swp(hw)=%f\n",
				s,
				SW_Site->swcBulk_halfwiltpt[s],
				SW_SWRC_SWCtoSWP(SW_Site->swcBulk_halfwiltpt[s], SW_Site, s, LogInfo)
			);
		}
		#endif


		/* sum ev and tr coefficients for later */
		evsum += SW_Site->evap_coeff[s];

		ForEachVegType(k)
		{
			trsum_veg[k] += SW_Site->transp_coeff[k][s];
			/* calculate soil water content at SWPcrit for each vegetation type */
			SW_Site->swcBulk_atSWPcrit[k][s] = SW_SWRC_SWPtoSWC(
				SW_VegProd->veg[k].SWPcrit,
				SW_Site,
				s,
				LogInfo
			);

			if (LT(SW_Site->swcBulk_atSWPcrit[k][s], SW_Site->swcBulk_min[s])) {
				flagswpcrit++;

				// lower SWcrit [-bar] to SWP-equivalent of swBulk_min
				tmp = fmin(
					SW_VegProd->veg[k].SWPcrit,
					SW_SWRC_SWCtoSWP(SW_Site->swcBulk_min[s], SW_Site, s, LogInfo)
				);

				LogError(
					LogInfo, LOGWARN,
					"%s : Layer %d - vegtype %d\n"
					"  calculated `swcBulk_atSWPcrit` (%.4f cm / %.4f MPa)\n"
					"          <= `swcBulk_min` (%.4f cm / %.4f MPa).\n"
					"  `SWcrit` adjusted to %.4f MPa "
					"(and swcBulk_atSWPcrit in every layer will be re-calculated).\n",
					"Site", s + 1, k + 1,
					SW_Site->swcBulk_atSWPcrit[k][s],
					-0.1 * SW_VegProd->veg[k].SWPcrit,
					SW_Site->swcBulk_min[s],
					-0.1 * SW_SWRC_SWCtoSWP(SW_Site->swcBulk_min[s], SW_Site, s, LogInfo),
					-0.1 * tmp
				);

				SW_VegProd->veg[k].SWPcrit = tmp;
			}

			/* Find which transpiration region the current soil layer
			 * is in and check validity of result. Region bounds are
			 * base1 but s is base0.*/
			curregion = 0;
			ForEachTranspRegion(r, SW_Site->n_transp_rgn)
			{
				if (s < SW_Site->_TranspRgnBounds[r]) {
					if (ZRO(SW_Site->transp_coeff[k][s]))
						break; /* end of transpiring layers */
					curregion = r + 1;
					break;
				}
			}

			if (curregion || SW_Site->_TranspRgnBounds[curregion] == 0) {
				SW_Site->my_transp_rgn[k][s] = curregion;
				SW_Site->n_transp_lyrs[k] = max(SW_Site->n_transp_lyrs[k], s);

			} else if (s == 0) {
				LogError(LogInfo, LOGFATAL, ": Top soil layer must be included\n"
						"  in %s tranpiration regions.\n", key2veg[k]);
			} else if (r < SW_Site->n_transp_rgn) {
				LogError(LogInfo, LOGFATAL, ": Transpiration region %d \n"
						"  is deeper than the deepest layer with a\n"
						"  %s transpiration coefficient > 0 (%d).\n"
						"  Please fix the discrepancy and try again.\n",
						r + 1, key2veg[k], s
				);
			} else {
				SW_Site->my_transp_rgn[k][s] = 0;
			}
		}
	} /*end ForEachSoilLayer */


	/* Re-calculate `swcBulk_atSWPcrit`
			if it was below `swcBulk_min` for any vegetation x soil layer combination
			using adjusted `SWPcrit`
	*/
	if (flagswpcrit) {
		ForEachSoilLayer(s, SW_Site->n_layers)
		{
			ForEachVegType(k)
			{
				/* calculate soil water content at adjusted SWPcrit */
				SW_Site->swcBulk_atSWPcrit[k][s] = SW_SWRC_SWPtoSWC(
					SW_VegProd->veg[k].SWPcrit,
					SW_Site,
					s,
					LogInfo
				);

				if (LT(SW_Site->swcBulk_atSWPcrit[k][s],
												SW_Site->swcBulk_min[s])) {
					LogError(
						LogInfo, LOGFATAL,
						"%s : Layer %d - vegtype %d\n"
						"  calculated `swcBulk_atSWPcrit` (%.4f cm)\n"
						"          <= `swcBulk_min` (%.4f cm).\n"
						"  even with adjusted `SWcrit` (%.4f MPa).\n",
						"Site", s + 1, k + 1,
						SW_Site->swcBulk_atSWPcrit[k][s],
						SW_Site->swcBulk_min[s],
						-0.1 * SW_VegProd->veg[k].SWPcrit
					);
				}
			}
		}

		/* Update values for `get_swa()` */
		ForEachVegType(k)
		{
			SW_VegProd->critSoilWater[k] = -0.1 * SW_VegProd->veg[k].SWPcrit;
		}
		get_critical_rank(SW_VegProd);
	}

	/* normalize the evap and transp coefficients separately
	 * to avoid obfuscation in the above loop */
	if (!EQ_w_tol(evsum, 1.0, 1e-4)) { // inputs are not more precise than at most 3-4 digits
		LogError(
			LogInfo,
			LOGWARN,
			"%s : Evaporation coefficients were normalized:\n"
			"\tSum of coefficients was %.4f, but must be 1.0. "
			"New coefficients are:",
			"Site",
			evsum
		);

		ForEachEvapLayer(s, SW_Site->n_evap_lyrs)
		{
			SW_Site->evap_coeff[s] /= evsum;
			LogError(
				LogInfo,
				LOGWARN,
				"  Layer %2d : %.4f",
				s + 1, SW_Site->evap_coeff[s]
			);
		}
	}

	ForEachVegType(k)
	{
		// inputs are not more precise than at most 3-4 digits
		if (!EQ_w_tol(trsum_veg[k], 1.0, 1e-4)) {
			LogError(
				LogInfo,
				LOGWARN,
				"%s : Transpiration coefficients were normalized for %s:\n"
				"\tSum of coefficients was %.4f, but must be 1.0. "
				"New coefficients are:",
				"Site", key2veg[k], trsum_veg[k]
			);

			ForEachSoilLayer(s, SW_Site->n_layers)
			{
				if (GT(SW_Site->transp_coeff[k][s], 0.))
				{
					SW_Site->transp_coeff[k][s] /= trsum_veg[k];
					LogError(
						LogInfo,
						LOGWARN,
						"  Layer %2d : %.4f",
						s + 1,
						SW_Site->transp_coeff[k][s]
					);
				}
			}
		}
	}

	// getting the number of regressions, for use in the soil_temperature function
	SW_Site->stNRGR = (SW_Site->stMaxDepth / SW_Site->stDeltaX) - 1;
	Bool too_many_RGR = (Bool) (SW_Site->stNRGR + 1 >= MAX_ST_RGR);

	if (!EQ(fmod(SW_Site->stMaxDepth, SW_Site->stDeltaX), 0.0) || too_many_RGR) {

		if (too_many_RGR)
		{ // because we will use loops such `for (i = 0; i <= nRgr + 1; i++)`
			LogError(
				LogInfo,
				LOGWARN,
				"\nSOIL_TEMP FUNCTION ERROR: the number of regressions is > the "
				"maximum number of regressions.  resetting max depth, deltaX, nRgr "
				"values to 180, 15, & 11 respectively\n"
			);
		}
		else
		{ // because we don't deal with partial layers
			LogError(
				LogInfo,
				LOGWARN,
				"\nSOIL_TEMP FUNCTION ERROR: max depth is not evenly divisible by "
				"deltaX (ie the remainder != 0).  resetting max depth, deltaX, nRgr "
				"values to 180, 15, & 11 respectively\n"
			);
		}

		// resets it to the default values
		SW_Site->stMaxDepth = 180.0;
		SW_Site->stNRGR = 11;
		SW_Site->stDeltaX = 15.0;
	}
}


/**
	@brief Reset counts of `SW_Site` to zero

	@param[out] SW_Site Struct of type SW_SITE describing the simulated site
*/
void SW_SIT_init_counts(SW_SITE* SW_Site) {
	int k;

	// Reset counts
	SW_Site->n_layers = 0;
	SW_Site->n_evap_lyrs = 0;
	SW_Site->deep_lyr = 0;
	SW_Site->n_transp_rgn = 0;

	ForEachVegType(k)
	{
		SW_Site->n_transp_lyrs[k] = 0;
	}
}

/**
@brief Print site-parameters and soil characteristics.

@param[in] SW_Site Struct of type SW_SITE describing the simulated site
*/
void _echo_inputs(SW_SITE* SW_Site) {
	/* =================================================== */
	LyrIndex i;
	LOG_INFO LogInfo;
	sw_init_logs(&LogInfo);

	printf("\n\n=====================================================\n"
			"Site Related Parameters:\n"
			"---------------------\n");
	printf("  Site File: 'siteparam.in'\n");
	printf("  Reset SWC values each year: %s\n", (SW_Site->reset_yr) ? "swTRUE" : "swFALSE");
	printf("  Use deep drainage reservoir: %s\n", (SW_Site->deepdrain) ? "swTRUE" : "swFALSE");
	printf("  Slow Drain Coefficient: %5.4f\n", SW_Site->slow_drain_coeff);
	printf("  PET Scale: %5.4f\n", SW_Site->pet_scale);
	printf("  Runoff: proportion of surface water lost: %5.4f\n", SW_Site->percentRunoff);
	printf("  Runon: proportion of new surface water gained: %5.4f\n", SW_Site->percentRunon);
	printf("  Longitude (degree): %4.2f\n", SW_Site->longitude * rad_to_deg);
	printf("  Latitude (degree): %4.2f\n", SW_Site->latitude * rad_to_deg);
	printf("  Altitude (m a.s.l.): %4.2f \n", SW_Site->altitude);
	printf("  Slope (degree): %4.2f\n", SW_Site->slope * rad_to_deg);
	printf("  Aspect (degree): %4.2f\n", SW_Site->aspect * rad_to_deg);

	printf("\nSnow simulation parameters (SWAT2K model):\n----------------------\n");
	printf("  Avg. air temp below which ppt is snow ( C): %5.4f\n", SW_Site->TminAccu2);
	printf("  Snow temperature at which snow melt starts ( C): %5.4f\n", SW_Site->TmaxCrit);
	printf("  Relative contribution of avg. air temperature to todays snow temperture vs. yesterday's snow temperature (0-1): %5.4f\n", SW_Site->lambdasnow);
	printf("  Minimum snow melt rate on winter solstice (cm/day/C): %5.4f\n", SW_Site->RmeltMin);
	printf("  Maximum snow melt rate on summer solstice (cm/day/C): %5.4f\n", SW_Site->RmeltMax);

	printf("\nSoil Temperature Constants:\n----------------------\n");
	printf("  Biomass Limiter constant: %5.4f\n", SW_Site->bmLimiter);
	printf("  T1Param1: %5.4f\n", SW_Site->t1Param1);
	printf("  T1Param2: %5.4f\n", SW_Site->t1Param2);
	printf("  T1Param3: %5.4f\n", SW_Site->t1Param3);
	printf("  csParam1: %5.4f\n", SW_Site->csParam1);
	printf("  csParam2: %5.4f\n", SW_Site->csParam2);
	printf("  shParam: %5.4f\n", SW_Site->shParam);
	printf("  Tsoil_constant: %5.4f\n", SW_Site->Tsoil_constant);
	printf("  deltaX: %5.4f\n", SW_Site->stDeltaX);
	printf("  max depth: %5.4f\n", SW_Site->stMaxDepth);
	printf("  Make soil temperature calculations: %s\n", (SW_Site->use_soil_temp) ? "swTRUE" : "swFALSE");
	printf("  Number of regressions for the soil temperature function: %d\n", SW_Site->stNRGR);

	printf("\nLayer Related Values:\n----------------------\n");
	printf("  Soils File: 'soils.in'\n");
	printf("  Number of soil layers: %d\n", SW_Site->n_layers);
	printf("  Number of evaporation layers: %d\n", SW_Site->n_evap_lyrs);
	printf("  Number of forb transpiration layers: %d\n", SW_Site->n_transp_lyrs[SW_FORBS]);
	printf("  Number of tree transpiration layers: %d\n", SW_Site->n_transp_lyrs[SW_TREES]);
	printf("  Number of shrub transpiration layers: %d\n", SW_Site->n_transp_lyrs[SW_SHRUB]);
	printf("  Number of grass transpiration layers: %d\n", SW_Site->n_transp_lyrs[SW_GRASS]);
	printf("  Number of transpiration regions: %d\n", SW_Site->n_transp_rgn);

	printf("\nLayer Specific Values:\n----------------------\n");
	printf("\n  Layer information on a per centimeter depth basis:\n");
	printf(			"  Lyr Width   BulkD 	%%Gravel    FieldC   WiltPt   %%Sand  %%Clay VWC at Forb-critSWP 	VWC at Tree-critSWP	VWC at Shrub-critSWP	VWC at Grass-critSWP	EvCo   	TrCo_Forb   TrCo_Tree  TrCo_Shrub  TrCo_Grass   TrRgn_Forb    TrRgn_Tree   TrRgn_Shrub   TrRgn_Grass   Wet     Min      Init     Saturated    Impermeability\n");
	printf(			"       (cm)   (g/cm^3)  (prop)    (cm/cm)  (cm/cm)   (prop) (prop)  (cm/cm)			(cm/cm)                (cm/cm)            		(cm/cm)         (prop)    (prop)      (prop)     (prop)    (prop)        (int)           (int) 	      	(int) 	    (int) 	    (cm/cm)  (cm/cm)  (cm/cm)  (cm/cm)      (frac)\n");
	printf(			"  --- -----   ------    ------     ------   ------   -----  ------   ------                	-------			------            		------          ------    ------      ------      ------   ------       ------   	 -----	        -----       -----   	 ----     ----     ----    ----         ----\n");
	ForEachSoilLayer(i, SW_Site->n_layers)
	{
		printf("  %3d %5.1f %9.5f %6.2f %8.5f %8.5f %6.2f %6.2f %6.2f %6.2f %6.2f %6.2f %9.2f %9.2f %9.2f %9.2f %9.2f %10d %10d %15d %15d %15.4f %9.4f %9.4f %9.4f %9.4f\n",
				i + 1, SW_Site->width[i], SW_Site->soilBulk_density[i], SW_Site->fractionVolBulk_gravel[i], SW_Site->swcBulk_fieldcap[i] / SW_Site->width[i],
				SW_Site->swcBulk_wiltpt[i] / SW_Site->width[i], SW_Site->fractionWeightMatric_sand[i], SW_Site->fractionWeightMatric_clay[i],
				SW_Site->swcBulk_atSWPcrit[i][SW_FORBS] / SW_Site->width[i], SW_Site->swcBulk_atSWPcrit[i][SW_TREES] / SW_Site->width[i],
				SW_Site->swcBulk_atSWPcrit[i][SW_SHRUB] / SW_Site->width[i], SW_Site->swcBulk_atSWPcrit[i][SW_GRASS] / SW_Site->width[i], SW_Site->evap_coeff[i],
				SW_Site->transp_coeff[SW_FORBS][i], SW_Site->transp_coeff[SW_TREES][i], SW_Site->transp_coeff[SW_SHRUB][i], SW_Site->transp_coeff[i][SW_GRASS], SW_Site->my_transp_rgn[SW_FORBS][i],
				SW_Site->my_transp_rgn[SW_TREES][i], SW_Site->my_transp_rgn[SW_SHRUB][i], SW_Site->my_transp_rgn[SW_GRASS][i], SW_Site->swcBulk_wet[i] / SW_Site->width[i],
				SW_Site->swcBulk_min[i] / SW_Site->width[i], SW_Site->swcBulk_init[i] / SW_Site->width[i], SW_Site->swcBulk_saturated[i] / SW_Site->width[i],
				SW_Site->impermeability[i]);

	}
	printf("\n  Actual per-layer values:\n");
	printf("  Lyr Width  BulkD	 %%Gravel   FieldC   WiltPt %%Sand  %%Clay	SWC at Forb-critSWP     SWC at Tree-critSWP	SWC at Shrub-critSWP	SWC at Grass-critSWP	 Wet    Min      Init  Saturated	SoilTemp\n");
	printf(			"       (cm)  (g/cm^3)	(prop)    (cm)     (cm)  (prop) (prop)   (cm)    	(cm)        		(cm)            (cm)            (cm)   (cm)      (cm)     (cm)		(celcius)\n");
	printf(			"  --- -----  -------	------   ------   ------ ------ ------   ------        	------            	------          ----   		----     ----     ----    ----		----\n");

	ForEachSoilLayer(i, SW_Site->n_layers)
	{
		printf("  %3d %5.1f %9.5f %6.2f %8.5f %8.5f %6.2f %6.2f %7.4f %7.4f %7.4f %7.4f %7.4f %7.4f %8.4f %7.4f %5.4f\n", i + 1, SW_Site->width[i],
				SW_Site->soilBulk_density[i], SW_Site->fractionVolBulk_gravel[i], SW_Site->swcBulk_fieldcap[i], SW_Site->swcBulk_wiltpt[i], SW_Site->fractionWeightMatric_sand[i],
				SW_Site->fractionWeightMatric_clay[i], SW_Site->swcBulk_atSWPcrit[i][SW_FORBS], SW_Site->swcBulk_atSWPcrit[i][SW_TREES], SW_Site->swcBulk_atSWPcrit[i][SW_SHRUB],
				SW_Site->swcBulk_atSWPcrit[i][SW_GRASS], SW_Site->swcBulk_wet[i], SW_Site->swcBulk_min[i], SW_Site->swcBulk_init[i], SW_Site->swcBulk_saturated[i], SW_Site->avgLyrTempInit[i]);
	}

	printf("\n  Water Potential values:\n");
	printf("  Lyr       FieldCap         WiltPt            Forb-critSWP     Tree-critSWP     Shrub-critSWP    Grass-critSWP    Wet            Min            Init\n");
	printf(			"            (bars)           (bars)            (bars)           (bars)           (bars)           (bars)           (bars)         (bars)         (bars)\n");
	printf(			"  ---       -----------      ------------      -----------      -----------      -----------      -----------      -----------    -----------    --------------    --------------\n");

	ForEachSoilLayer(i, SW_Site->n_layers)
	{
		printf(
			"  %3d   %15.4f   %15.4f  %15.4f %15.4f  %15.4f  %15.4f  %15.4f   %15.4f   %15.4f\n",
			i + 1,
			SW_SWRC_SWCtoSWP(SW_Site->swcBulk_fieldcap[i], SW_Site, i, &LogInfo),
			SW_SWRC_SWCtoSWP(SW_Site->swcBulk_wiltpt[i], SW_Site, i, &LogInfo),
			SW_SWRC_SWCtoSWP(SW_Site->swcBulk_atSWPcrit[i][SW_FORBS], SW_Site, i, &LogInfo),
			SW_SWRC_SWCtoSWP(SW_Site->swcBulk_atSWPcrit[i][SW_TREES], SW_Site, i, &LogInfo),
			SW_SWRC_SWCtoSWP(SW_Site->swcBulk_atSWPcrit[i][SW_SHRUB], SW_Site, i, &LogInfo),
			SW_SWRC_SWCtoSWP(SW_Site->swcBulk_atSWPcrit[i][SW_SHRUB], SW_Site, i, &LogInfo),
			SW_SWRC_SWCtoSWP(SW_Site->swcBulk_atSWPcrit[i][SW_GRASS], SW_Site, i, &LogInfo),
			SW_SWRC_SWCtoSWP(SW_Site->swcBulk_min[i], SW_Site, i, &LogInfo),
			SW_SWRC_SWCtoSWP(SW_Site->swcBulk_init[i], SW_Site, i, &LogInfo)
		);
	}

	printf(
		"\nSoil Water Retention Curve:\n---------------------------\n"
	);
	printf(
		"  SWRC type: %d (%s)\n",
		SW_Site->site_swrc_type,
		SW_Site->site_swrc_name
	);
	printf(
		"  PTF type: %d (%s)\n",
		SW_Site->site_ptf_type,
		SW_Site->site_ptf_name
	);

	printf(
		"  Lyr     Param1     Param2     Param3     Param4     Param5     Param6\n"
	);
	ForEachSoilLayer(i, SW_Site->n_layers)
	{
		printf(
			"  %3d%11.4f%11.4f%11.4f%11.4f%11.4f%11.4f\n",
			i + 1,
			SW_Site->swrcp[i][0],
			SW_Site->swrcp[i][1],
			SW_Site->swrcp[i][2],
			SW_Site->swrcp[i][3],
			SW_Site->swrcp[i][4],
			SW_Site->swrcp[i][5]
		);
	}



	printf(
		"\n------------ End of Site Parameters ------------------\n"
	);
}

#ifdef DEBUG_MEM
#include "include/myMemory.h"
/*======================================================*/
void SW_SIT_SetMemoryRefs( void) {
	/* when debugging memory problems, use the bookkeeping
	 code in myMemory.c
	 This routine sets the known memory refs in this module
	 so they can be  checked for leaks, etc.  All refs will
	 have been cleared by a call to ClearMemoryRefs() before
	 this, and will be checked via CheckMemoryRefs() after
	 this, most likely in the main() function.
	 */
	LyrIndex l;

	NoteMemoryRef(SW_Site.lyr);
	ForEachSoilLayer(l) {
		NoteMemoryRef(SW_Site.lyr[l]);
	}
}

#endif
