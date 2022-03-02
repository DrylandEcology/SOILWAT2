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

#include "generic.h" // externs `QuietMode`, `EchoInits`
#include "filefuncs.h" // externs `_firstfile`, `inbuf`
#include "myMemory.h"
#include "SW_Defines.h"

#include "SW_Carbon.h" // externs SW_Carbon
#include "SW_Files.h"
#include "SW_Site.h" // externs SW_Site
#include "SW_SoilWater.h"

#include "SW_VegProd.h" // externs SW_VegProd, key2veg



/* =================================================== */
/*                  Global Variables                   */
/* --------------------------------------------------- */

SW_SITE SW_Site;


/* transpiration regions  shallow, moderately shallow,  */
/* deep and very deep. units are in layer numbers. */
LyrIndex _TranspRgnBounds[MAX_TRANSP_REGIONS];


/* for these three, units are cm/cm if < 1, -bars if >= 1 */
RealD
  _SWCInitVal, /* initialization value for swc */
  _SWCWetVal, /* value for a "wet" day,       */
  _SWCMinVal; /* lower bound on swc.          */


/** Character representation of implemented Soil Water Retention Curves (SWRC)
*/
/* MAINTENANCE:
	- Values must exactly match those provided in `siteparam.in`.
	- If entries are added/removed/change, then update `check_SWRC_vs_PDF()`,
	  `SWRC_check_parameters()`, `SWRC_SWCtoSWP()`, and `SWRC_SWPtoSWC()`.
*/
char const *swrc2str[N_SWRCs] = {
	"Campbell1974",
	"vanGenuchten1980"
};

/** Character representation of implemented Pedotransfer Functions (PDF)
*/
/* MAINTENANCE:
	- Values must exactly match those provided in `siteparam.in`.
	- The first value must be "NoPDF".
	- If entries are added/removed/change, then update `check_SWRC_vs_PDF()` and
	  `SWRC_PDF_estimate_parameters()`.
*/
char const *pdf2str[N_PDFs] = {
	"NoPDF",
	"Cosby1984AndOthers",
	"Cosby1984",
	"Rosetta3"
};


/* =================================================== */
/*                  Local Variables                    */
/* --------------------------------------------------- */
static char *MyFileName;


/* =================================================== */
/*             Local Function Definitions              */
/* --------------------------------------------------- */

/** Check validity of soil properties

	@param[in] *lyr Soil information.

	@return A logical value indicating if soil properties passed the checks.
*/
static Bool SW_check_soil_properties(SW_LAYER_INFO *lyr) {
	int k;
	RealD fval = 0;
	const char *errtype = "\0";
	Bool res = swTRUE;


	if (LE(lyr->width, 0.)) {
		res = swFALSE;
		fval = lyr->width;
		errtype = Str_Dup("layer width");

	} else if (LT(lyr->soilMatric_density, 0.)) {
		res = swFALSE;
		fval = lyr->soilMatric_density;
		errtype = Str_Dup("soil density");

	} else if (
		LT(lyr->fractionVolBulk_gravel, 0.) ||
		GE(lyr->fractionVolBulk_gravel, 1.)
	) {
		res = swFALSE;
		fval = lyr->fractionVolBulk_gravel;
		errtype = Str_Dup("gravel content");

	} else if (
		LE(lyr->fractionWeightMatric_sand, 0.) ||
		GE(lyr->fractionWeightMatric_sand, 1.)
	) {
		res = swFALSE;
		fval = lyr->fractionWeightMatric_sand;
		errtype = Str_Dup("sand proportion");

	} else if (
		LE(lyr->fractionWeightMatric_clay, 0.) ||
		GE(lyr->fractionWeightMatric_clay, 1.)
	) {
		res = swFALSE;
		fval = lyr->fractionWeightMatric_clay;
		errtype = Str_Dup("clay proportion");

	} else if (
		GE(lyr->fractionWeightMatric_sand + lyr->fractionWeightMatric_clay, 1.)
	) {
		res = swFALSE;
		fval = lyr->fractionWeightMatric_sand + lyr->fractionWeightMatric_clay;
		errtype = Str_Dup("sand+clay proportion");

	} else if (
		LT(lyr->impermeability, 0.) ||
		GT(lyr->impermeability, 1.)
	) {
		res = swFALSE;
		fval = lyr->impermeability;
		errtype = Str_Dup("impermeability");

	} else if (
		LT(lyr->evap_coeff, 0.) ||
		GT(lyr->evap_coeff, 1.)
	) {
		res = swFALSE;
		fval = lyr->evap_coeff;
		errtype = Str_Dup("bare-soil evaporation coefficient");

	} else {
		ForEachVegType(k) {
			if (LT(lyr->transp_coeff[k], 0.) || GT(lyr->transp_coeff[k], 1.)) {
				res = swFALSE;
				fval = lyr->transp_coeff[k];
				errtype = Str_Dup("transpiration coefficient");
				break;
			}
		}
	}

	if (!res) {
		LogError(
			logfp,
			LOGFATAL,
			"'%s' has an invalid value (%5.4f) in layer %d.\n",
			errtype, fval, lyr->id + 1
		);
	}

	return res;
}




/* =================================================== */
/*             Global Function Definitions             */
/* --------------------------------------------------- */


/**
	@brief Translate a SWRC name into a SWRC type number

	@param[in] *swrc_name Name of a SWRC

	@return Internal identification number of selected SWRC
*/
unsigned int encode_str2swrc(char *swrc_name) {
	unsigned int k;

	for (
		k = 0;
		k < N_SWRCs && Str_CompareI(swrc_name, (char *)swrc2str[k]);
		k++
	);

	if (k == N_SWRCs) {
		LogError(
			logfp,
			LOGFATAL,
			"SWRC '%s' is not implemented.",
			swrc_name
		);
	}

	return k;
}

/**
	@brief Translate a PDF name into a PDF type number

	@param[in] *pdf_name Name of a PDF

	@return Internal identification number of selected PDF
*/
unsigned int encode_str2pdf(char *pdf_name) {
	unsigned int k;

	for (
		k = 0;
		k < N_PDFs && Str_CompareI(pdf_name, (char *)pdf2str[k]);
		k++
	);

	if (k == N_PDFs) {
		LogError(
			logfp,
			LOGFATAL,
			"PDF '%s' is not implemented.",
			pdf_name
		);
	}

	return k;
}



/**
	@brief Estimate parameters of selected soil water retention curve (SWRC)
		using selected pedotransfer function (PDF)

	See #pdf2str for implemented PDFs.

	@param[in] pdf_type Identification number of selected PDF
	@param[out] *swrcp Vector of SWRC parameters to be estimated
	@param[in] sand Sand content of the matric soil (< 2 mm fraction) [g/g]
	@param[in] clay Clay content of the matric soil (< 2 mm fraction) [g/g]
	@param[in] gravel Coarse fragments (> 2 mm; e.g., gravel)
		of the whole soil [m3/m3]
*/
void SWRC_PDF_estimate_parameters(
	unsigned int pdf_type,
	double *swrcp,
	double sand, double clay, double gravel
) {

	if (pdf_type == 0) {
		LogError(
			logfp,
			LOGNOTE,
			"`SWRC_PDF_estimate_parameters()` was called even though "
			"`pdf_type` %d was requested "
			"(which uses values from 'swrc_params.in' instead of estimation).",
			pdf_type
		);

	} else {

		/* Initialize swrcp[] to 0 */
		memset(swrcp, 0., SWRC_PARAM_NMAX * sizeof(swrcp[0]));

		if (pdf_type == 1) {
			SWRC_PDF_Cosby1984_for_Campbell1974(swrcp, sand, clay);

		} else if (pdf_type == 2) {
			SWRC_PDF_Cosby1984_for_Campbell1974(swrcp, sand, clay);

		} else {
			LogError(
				logfp,
				LOGFATAL,
				"PDF (type %d) is not implemented in SOILWAT2.",
				pdf_type
			);
		}
	}

	/**********************************/
	/* TODO: remove once other PDFs are implemented that utilize gravel */
	/* avoiding `error: unused parameter 'gravel' [-Werror=unused-parameter]` */
	if (gravel < 0.) {};
	/**********************************/
}


/**
	@brief Estimate Campbell's 1974 SWRC parameters
		using Cosby et al. 1984 multivariate PDF

	Estimation of three SWRC parameter values `swrcp`
	based on sand, clay, and (silt).

	Parameters are explained in `SWRC_check_parameters_for_Campbell1974()`.

	Multivariate PDFs are from Cosby et al. 1984 (\cite Cosby1984) Table 4;
	Cosby et al. 1984 provided also univariate PDFs in Table 5
	but they are not used here.

	See `SWRC_SWCtoSWP_Campbell1974()` and `SWRC_SWPtoSWC_Campbell1974()`
	for implementation of Campbell's 1974 SWRC (\cite Campbell1974).

	@param[out] *swrcp Vector of SWRC parameters to be estimated
	@param[in] sand Sand content of the matric soil (< 2 mm fraction) [g/g]
	@param[in] clay Clay content of the matric soil (< 2 mm fraction) [g/g]
*/
void SWRC_PDF_Cosby1984_for_Campbell1974(
	double *swrcp,
	double sand, double clay
) {
	/* Table 4 */

	/* swrcp[0] = psi_saturated: originally formulated as function of silt
	   here re-formulated as function of clay */
	swrcp[0] = powe(10.0, -1.58 * sand - 0.63 * clay + 2.17);
	/* swrcp[1] = theta_saturated: originally with units [100 * cm / cm]
	   here re-formulated with units [cm / cm] */
	swrcp[1] = -0.142 * sand - 0.037 * clay + 0.505;
	swrcp[2] = -0.3 * sand + 15.7 * clay + 3.10;
}


/**
	@brief Saturated soil water content

	See #pdf2str for implemented PDFs.
	See #swrc2str for implemented SWRCs.

	Saturated volumetric water content is usually estimated as one of the
	SWRC parameters; this is what this function returns.

	For historical reasons, if `swrc_name` is "Campbell1974", then a
	`pdf_name` of "Cosby1984AndOthers" will reproduce `SOILWAT2` legacy mode
	(`SOILWAT2` prior to v7.0.0) and return saturated soil water content estimated
	by Saxton et al. 2006 (\cite Saxton2006) PDF instead;
	`pdf_name` of "Cosby1984" will return saturated soil water content estimated
	by Cosby et al. 1984 (\cite Cosby1984) PDF.

	The arguments `pdf_type`, `sand`, and `clay` are utilized only if
	`pdf_name` is "Cosby1984AndOthers" (see #pdf2str).

	@param[in] swrc_type Identification number of selected SWRC
	@param[out] *swrcp Vector of SWRC parameters to be estimated
	@param[in] gravel Coarse fragments (> 2 mm; e.g., gravel)
		of the whole soil [m3/m3]
	@param[in] width Soil layer width [cm]
	@param[in] pdf_type Identification number of selected PDF
	@param[in] sand Sand content of the matric soil (< 2 mm fraction) [g/g]
	@param[in] clay Clay content of the matric soil (< 2 mm fraction) [g/g]

	@return Estimated saturated water content [cm]
*/
double SWRC_PDF_swcBulk_saturated(
	unsigned int swrc_type,
	double *swrcp,
	double gravel,
	double width,
	unsigned int pdf_type,
	double sand,
	double clay
) {
	double theta_sat = SW_MISSING;

	switch (swrc_type) {
		case 0: // Campbell1974
			if (pdf_type == 1) {
				// Cosby1984AndOthers
				PDF_Saxton2006(&theta_sat, sand, clay);
			} else {
				// Cosby1984
				theta_sat = swrcp[1];
			}
			break;

		case 1: // vanGenuchten1980
			theta_sat = swrcp[1];
			break;

		default:
			LogError(
				logfp,
				LOGFATAL,
				"`SWRC_PDF_swcBulk_saturated()`: SWRC (type %d) is not implemented.",
				swrc_type
			);
			break;
	}

	// Convert from [cm/cm] to [cm]
	return theta_sat * width * (1. - gravel);
}

/**
	@brief Check whether selected PDF and SWRC are compatible

	@param[in] *swrc_name Name of selected SWRC
	@param[in] *pdf_name Name of selected PDF
	@param[in] isSW2 Logical; TRUE if scope of PDF implementation is "SOILWAT2".

	@return A logical value indicating if SWRC and PDF are compatible.
*/
Bool check_SWRC_vs_PDF(char *swrc_name, char *pdf_name, Bool isSW2) {
	Bool res = swFALSE;

	if (Str_CompareI(pdf_name, (char *) "NoPDF") == 0) {
		res = swTRUE;
	} else {

		if (
			Str_CompareI(swrc_name, (char *) "Campbell1974") == 0 &&
			(
				Str_CompareI(pdf_name, (char *) "Cosby1984AndOthers") == 0 ||
				Str_CompareI(pdf_name, (char *) "Cosby1984") == 0
			)
		) {
			res = swTRUE;
		}
		else if (
			!isSW2 &&
			Str_CompareI(swrc_name, (char *) "vanGenuchten1980") == 0 &&
			// "Rosetta3" PDF is not implemented in SOILWAT2 (but in rSOILWAT2)
			Str_CompareI(pdf_name, (char *) "Rosetta3") == 0
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

	@return A logical value indicating if parameters passed the checks.
*/
Bool SWRC_check_parameters(unsigned int swrc_type, double *swrcp) {
	Bool res = swFALSE;

	switch (swrc_type) {
		case 0:
			res = SWRC_check_parameters_for_Campbell1974(swrcp);
			break;

		case 1:
			res = SWRC_check_parameters_for_vanGenuchten1980(swrcp);
			break;

		default:
			LogError(
				logfp,
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

	Campbell's 1974 SWRC uses three parameters:
		- `swrcp[0]` (`psisMatric`): saturated soil water matric potential [-bar]
		- `swrcp[1]` (previously named `thetasMatric`):
			saturated volumetric water content for the matric component [cm/cm]
		- `swrcp[2]` (`bMatric`): slope of the linear log-log retention curve [-]

	@param[in] *swrcp Vector of SWRC parameters

	@return A logical value indicating if parameters passed the checks.
*/
Bool SWRC_check_parameters_for_Campbell1974(double *swrcp) {
	Bool res = swTRUE;

	if (LE(swrcp[0], 0.0)) {
		res = swFALSE;
		LogError(
			logfp,
			LOGWARN,
			"SWRC_check_parameters_for_Campbell1974(): invalid value of "
			"psi(saturated, matric, [-bar]) = %f (must > 0)\n",
			swrcp[1]
		);
	}

	if (LE(swrcp[1], 0.0) || GT(swrcp[1], 1.0)) {
		res = swFALSE;
		LogError(
			logfp,
			LOGWARN,
			"SWRC_check_parameters_for_Campbell1974(): invalid value of "
			"theta(saturated, matric, [cm/cm]) = %f (must within 0-1)\n",
			swrcp[1]
		);
	}

	if (ZRO(swrcp[2])) {
		res = swFALSE;
		LogError(
			logfp,
			LOGWARN,
			"SWRC_check_parameters_for_Campbell1974(): invalid value of "
			"beta = %f (must be != 0)\n",
			swrcp[2]
		);
	}

	return res;
}

/**
	@brief Check van Genuchten 1980 SWRC parameters

	See `SWRC_SWCtoSWP_vanGenuchten1980()` and `SWRC_SWPtoSWC_vanGenuchten1980()`
	for implementation of van Genuchten's 1980 SWRC (\cite vanGenuchten1980).

	van Genuchten's 1980 SWRC uses four parameters:
		- `swrcp[0]` (`theta_r`): residual volumetric water content
			of the matric component [cm/cm]
		- `swrcp[1]` (`theta_s`): saturated volumetric water content
			of the matric component [cm/cm]
		- `swrcp[2]` (`alpha`): related to the inverse of air entry suction [cm-1]
		- `swrcp[3]` (`n`): measure of the pore-size distribution [-]

	@param[in] *swrcp Vector of SWRC parameters

	@return A logical value indicating if parameters passed the checks.
*/
Bool SWRC_check_parameters_for_vanGenuchten1980(double *swrcp) {
	Bool res = swTRUE;

	if (LE(swrcp[0], 0.0) || GT(swrcp[0], 1.)) {
		res = swFALSE;
		LogError(
			logfp,
			LOGWARN,
			"SWRC_check_parameters_for_vanGenuchten1980(): invalid value of "
			"theta(residual, matric, [cm/cm]) = %f (must within 0-1)\n",
			swrcp[0]
		);
	}

	if (LE(swrcp[1], 0.0) || GT(swrcp[1], 1.)) {
		res = swFALSE;
		LogError(
			logfp,
			LOGWARN,
			"SWRC_check_parameters_for_vanGenuchten1980(): invalid value of "
			"theta(saturated, matric, [cm/cm]) = %f (must within 0-1)\n",
			swrcp[1]
		);
	}

	if (LE(swrcp[1], swrcp[0])) {
		res = swFALSE;
		LogError(
			logfp,
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
			logfp,
			LOGWARN,
			"SWRC_check_parameters_for_vanGenuchten1980(): invalid value of "
			"alpha([1 / cm]) = %f (must > 0)\n",
			swrcp[2]
		);
	}

	if (LE(swrcp[3], 1.0)) {
		res = swFALSE;
		LogError(
			logfp,
			LOGWARN,
			"SWRC_check_parameters_for_vanGenuchten1980(): invalid value of "
			"n = %f (must be > 1)\n",
			swrcp[3]
		);
	}

	return res;
}


/**
	@brief Saxton et al. 2006 PDFs \cite Saxton2006
		to estimate saturated soil water content

	@param[in] sand Sand content of the matric soil (< 2 mm fraction) [g/g]
	@param[in] clay Clay content of the matric soil (< 2 mm fraction) [g/g]
	@param[out] *theta_sat Estimated saturated volumetric water content [cm/cm]
*/
void PDF_Saxton2006(
	double *theta_sat,
	double sand,
	double clay
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
			logfp,
			LOGFATAL,
			"PDF_Saxton2006(): invalid value of "
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
	@brief Rawls and Brakensiek 1985 PDFs \cite rawls1985WmitE
		to estimate residual soil water content for the Brooks-Corey SWRC
		\cite brooks1964a

	@note This function was previously named "SW_VWCBulkRes".
	@note This function is only well-defined for `clay` values in 5-60%,
		`sand` values in 5-70%, and `porosity` must in 10-<100%.

	@param[in] sand Sand content of the matric soil (< 2 mm fraction) [g/g]
	@param[in] clay Clay content of the matric soil (< 2 mm fraction) [g/g]
	@param[in] porosity Pore space of the matric soil (< 2 mm fraction) [cm3/cm3]
	@param[out] *theta_min Estimated residual volumetric water content [cm/cm]
*/
void PDF_RawlsBrakensiek1985(
	double *theta_min,
	double sand,
	double clay,
	double porosity
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
			logfp,
			LOGWARN,
			"`PDF_RawlsBrakensiek1985()`: sand, clay, or porosity out of valid range."
		);

		*theta_min = SW_MISSING;
	}
}



/**
  @brief Estimate soil density of the whole soil (bulk).

  SOILWAT2 calculates internally based on soil bulk density of the whole soil,
  i.e., including rock/gravel component. However, SOILWAT2 inputs are expected
  to represent soil (matric) density of the < 2 mm fraction.

  Based on equation 20 from Saxton. @cite Saxton2006
*/
RealD calculate_soilBulkDensity(RealD matricDensity, RealD fractionGravel) {
	/*eqn. 20 from Saxton et al. 2006  to calculate the bulk density of soil */
	return matricDensity * (1 - fractionGravel) + (fractionGravel * 2.65);
}


/**
  @brief Count soil layers with bare-soil evaporation potential

  The count stops at first layer with 0.
*/
LyrIndex nlayers_bsevap() {
	SW_SITE *v = &SW_Site;
	LyrIndex s, n = 0;

	ForEachSoilLayer(s)
	{
		if (GT(v->lyr[s]->evap_coeff, 0.0))
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
*/
void nlayers_vegroots(LyrIndex n_transp_lyrs[]) {
	SW_SITE *v = &SW_Site;
	LyrIndex s;
	int k;

	ForEachVegType(k)
	{
		n_transp_lyrs[k] = 0;

		ForEachSoilLayer(s)
		{
			if (GT(v->lyr[s]->transp_coeff[k], 0.0)) {
				n_transp_lyrs[k]++;
			} else {
				break;
			}
		}
	}
}


/**
  @brief Adds a dummy soil layer to handle deep drainage
*/
void add_deepdrain_layer(void) {

	if (SW_Site.deepdrain) {

		/* check if deep drain dummy layer was already added */
		if (SW_Site.deep_lyr == 0) {
			/* `_newlayer()` sets n_layers */
			LyrIndex s = _newlayer();
			SW_Site.lyr[s]->width = 1.0;

			/* sp->deepdrain indicates an extra (dummy) layer for deep drainage
			* has been added, so n_layers really should be n_layers -1
			* NOTE: deep_lyr is base0, n_layers is BASE1
			*/
			SW_Site.deep_lyr = --SW_Site.n_layers;
		}

	} else {

		/* no deep drainage */
		SW_Site.deep_lyr = 0;
	}
}


/**
@brief First time called with no layers so SW_Site.lyr
 not initialized yet, malloc() required.  For each
 layer thereafter realloc() is called.
*/
LyrIndex _newlayer(void) {

	SW_SITE *v = &SW_Site;
	v->n_layers++;

	v->lyr = (!v->lyr) /* if not yet defined */
		? (SW_LAYER_INFO **) Mem_Calloc(v->n_layers, sizeof(SW_LAYER_INFO *), "_newlayer()") /* malloc() it  */
		: (SW_LAYER_INFO **) Mem_ReAlloc(v->lyr, sizeof(SW_LAYER_INFO *) * (v->n_layers)); /* else realloc() */

	v->lyr[v->n_layers - 1] = (SW_LAYER_INFO *) Mem_Calloc(1, sizeof(SW_LAYER_INFO), "_newlayer()");
	v->lyr[v->n_layers - 1]->id = v->n_layers - 1;

	return v->n_layers - 1;
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
void SW_SIT_construct(void) {
	/* =================================================== */

	memset(&SW_Site, 0, sizeof(SW_Site));
	SW_SIT_init_counts();
}

/**
@brief Runs SW_SIT_clear_layers.
*/
void SW_SIT_deconstruct(void)
{
	SW_SIT_clear_layers();
}

/**
@brief Reads in file for input values.
*/
void SW_SIT_read(void) {
	/* =================================================== */
	/* 5-Feb-2002 (cwb) Removed rgntop requirement in
	 *    transpiration regions section of input
	 */
	SW_SITE *v = &SW_Site;
	SW_CARBON *c = &SW_Carbon;
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

	/* note that Files.read() must be called prior to this. */
	MyFileName = SW_F_name(eSite);

	f = OpenFile(MyFileName, "r");

	while (GetALine(f, inbuf)) {
		switch (lineno) {
		case 0:
			_SWCMinVal = atof(inbuf);
			break;
		case 1:
			_SWCInitVal = atof(inbuf);
			break;
		case 2:
			_SWCWetVal = atof(inbuf);
			break;
		case 3:
			v->reset_yr = itob(atoi(inbuf));
			break;
		case 4:
			v->deepdrain = itob(atoi(inbuf));
			break;
		case 5:
			v->pet_scale = atof(inbuf);
			break;
		case 6:
			v->percentRunoff = atof(inbuf);
			break;
		case 7:
			v->percentRunon = atof(inbuf);
			break;
		case 8:
			v->TminAccu2 = atof(inbuf);
			break;
		case 9:
			v->TmaxCrit = atof(inbuf);
			break;
		case 10:
			v->lambdasnow = atof(inbuf);
			break;
		case 11:
			v->RmeltMin = atof(inbuf);
			break;
		case 12:
			v->RmeltMax = atof(inbuf);
			break;
		case 13:
			v->slow_drain_coeff = atof(inbuf);
			break;
		case 14:
			v->evap.xinflec = atof(inbuf);
			break;
		case 15:
			v->evap.slope = atof(inbuf);
			break;
		case 16:
			v->evap.yinflec = atof(inbuf);
			break;
		case 17:
			v->evap.range = atof(inbuf);
			break;
		case 18:
			v->transp.xinflec = atof(inbuf);
			break;
		case 19:
			v->transp.slope = atof(inbuf);
			break;
		case 20:
			v->transp.yinflec = atof(inbuf);
			break;
		case 21:
			v->transp.range = atof(inbuf);
			break;
		case 22:
			// longitude is currently not used by the code, but may be used in the future
			// it is present in the `siteparam.in` input file to completely document
			// site location
			v->longitude = atof(inbuf) * deg_to_rad;
			break;
		case 23:
			v->latitude = atof(inbuf) * deg_to_rad;
			break;
		case 24:
			v->altitude = atof(inbuf);
			break;
		case 25:
			v->slope = atof(inbuf) * deg_to_rad;
			break;
		case 26:
			tmp = atof(inbuf);
			v->aspect = missing(tmp) ? tmp : tmp * deg_to_rad;
			break;
		case 27:
			v->bmLimiter = atof(inbuf);
			break;
		case 28:
			v->t1Param1 = atof(inbuf);
			break;
		case 29:
			v->t1Param2 = atof(inbuf);
			break;
		case 30:
			v->t1Param3 = atof(inbuf);
			break;
		case 31:
			v->csParam1 = atof(inbuf);
			break;
		case 32:
			v->csParam2 = atof(inbuf);
			break;
		case 33:
			v->shParam = atof(inbuf);
			break;
		case 34:
			v->Tsoil_constant = atof(inbuf);
			break;
		case 35:
			v->stDeltaX = atof(inbuf);
			break;
		case 36:
			v->stMaxDepth = atof(inbuf);
			break;
		case 37:
			v->use_soil_temp = itob(atoi(inbuf));
			break;
		case 38:
			c->use_bio_mult = itob(atoi(inbuf));
			#ifdef SWDEBUG
			if (debug) swprintf("'SW_SIT_read': use_bio_mult = %d\n", c->use_bio_mult);
			#endif
			break;
		case 39:
			c->use_wue_mult = itob(atoi(inbuf));
			#ifdef SWDEBUG
			if (debug) swprintf("'SW_SIT_read': use_wue_mult = %d\n", c->use_wue_mult);
			#endif
			break;
		case 40:
			strcpy(c->scenario, inbuf);
			#ifdef SWDEBUG
			if (debug) swprintf("'SW_SIT_read': scenario = %s\n", c->scenario);
			#endif
			break;
		case 41:
			strcpy(v->site_swrc_name, inbuf);
			v->site_swrc_type = encode_str2swrc(v->site_swrc_name);
			break;
		case 42:
			strcpy(v->site_pdf_name, inbuf);
			v->site_pdf_type = encode_str2pdf(v->site_pdf_name);
			break;

		default:
			if (lineno > 42 + MAX_TRANSP_REGIONS)
				break; /* skip extra lines */

			if (MAX_TRANSP_REGIONS < v->n_transp_rgn) {
				too_many_regions = swTRUE;
				goto Label_End_Read;
			}
			x = sscanf(inbuf, "%d %d", &region, &rgnlow);
			if (x < 2 || region < 1 || rgnlow < 1) {
				CloseFile(&f);
				LogError(logfp, LOGFATAL, "%s : Bad record %d.\n", MyFileName, lineno);
			}
			_TranspRgnBounds[region - 1] = (LyrIndex) (rgnlow - 1);
			v->n_transp_rgn++;
		}

		lineno++;
	}

	Label_End_Read:

	CloseFile(&f);

	if (LT(v->percentRunoff, 0.) || GT(v->percentRunoff, 1.)) {
		LogError(
			logfp,
			LOGFATAL,
			"%s : proportion of ponded surface water removed as daily"
			"runoff = %f (value ranges between 0 and 1)\n",
			MyFileName, v->percentRunoff
		);
	}

	if (LT(v->percentRunon, 0.)) {
		LogError(
			logfp,
			LOGFATAL,
			"%s : proportion of water that arrives at surface added "
			"as daily runon = %f (value ranges between 0 and +inf)\n",
			MyFileName, v->percentRunon
		);
	}

	if (too_many_regions) {
		LogError(
			logfp,
			LOGFATAL,
			"%s : Number of transpiration regions"
			" exceeds maximum allowed (%d > %d)\n",
			MyFileName, v->n_transp_rgn, MAX_TRANSP_REGIONS
		);
	}

	/* check for any discontinuities (reversals) in the transpiration regions */
	for (r = 1; r < v->n_transp_rgn; r++) {
		if (_TranspRgnBounds[r - 1] >= _TranspRgnBounds[r]) {
			LogError(
				logfp,
				LOGFATAL,
				"%s : Discontinuity/reversal in transpiration regions.\n",
				SW_F_name(eSite)
			);
		}
	}
}



/** Reads soil layers and soil properties from input file

		@note Previously, the function was static and named `_read_layers()`.
*/
void SW_LYR_read(void) {
	/* =================================================== */
	/* 5-Feb-2002 (cwb) removed dmin requirement in input file */

	SW_SITE *v = &SW_Site;
	FILE *f;
	LyrIndex lyrno;
	int x, k;
	RealF dmin = 0.0, dmax, evco, trco_veg[NVEGTYPES], psand, pclay, matricd, imperm,
		soiltemp, f_gravel;

	/* note that Files.read() must be called prior to this. */
	MyFileName = SW_F_name(eLayers);

	f = OpenFile(MyFileName, "r");

	while (GetALine(f, inbuf)) {
		lyrno = _newlayer();

		x = sscanf(
			inbuf,
			"%f %f %f %f %f %f %f %f %f %f %f %f",
			&dmax,
			&matricd,
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
			CloseFile(&f);
			LogError(
				logfp,
				LOGFATAL,
				"%s : Incomplete record %d.\n",
				MyFileName, lyrno + 1
			);
		}

		v->lyr[lyrno]->width = dmax - dmin;

		/* checks for valid values now carried out by `SW_SIT_init_run()` */

		dmin = dmax;
		v->lyr[lyrno]->fractionVolBulk_gravel = f_gravel;
		v->lyr[lyrno]->soilMatric_density = matricd;
		v->lyr[lyrno]->evap_coeff = evco;

		ForEachVegType(k)
		{
			v->lyr[lyrno]->transp_coeff[k] = trco_veg[k];
		}

		v->lyr[lyrno]->fractionWeightMatric_sand = psand;
		v->lyr[lyrno]->fractionWeightMatric_clay = pclay;
		v->lyr[lyrno]->impermeability = imperm;
		v->lyr[lyrno]->sTemp = soiltemp;

		if (lyrno >= MAX_LAYERS) {
			CloseFile(&f);
			LogError(
				logfp,
				LOGFATAL,
				"%s : Too many layers specified (%d).\n"
				"Maximum number of layers is %d\n",
				MyFileName, lyrno + 1, MAX_LAYERS
			);
		}
	}

	CloseFile(&f);
}




/**
  @brief Creates soil layers based on function arguments (instead of reading
    them from an input file as _read_layers() does)

  @param nlyrs The number of soil layers to create.
  @param[in] dmax Array of size \p nlyrs for depths [cm] of each soil layer
    measured from the surface
  @param[in] matricd Array of size \p nlyrs for soil density of the matric
    component, i.e., the < 2 mm fraction [g/cm3]
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

  @sideeffect After deleting any previous data in the soil layer array
    SW_Site.lyr, it creates new soil layers based on the argument inputs.

  @note
    - This function is a modified version of the function _read_layers() in
      SW_Site.c.
*/
void set_soillayers(LyrIndex nlyrs, RealF *dmax, RealF *matricd, RealF *f_gravel,
  RealF *evco, RealF *trco_grass, RealF *trco_shrub, RealF *trco_tree,
  RealF *trco_forb, RealF *psand, RealF *pclay, RealF *imperm, RealF *soiltemp,
  int nRegions, RealD *regionLowerBounds)
{

  RealF dmin = 0.0;
  SW_SITE *v = &SW_Site;

  LyrIndex lyrno;
  unsigned int i, k;

  // De-allocate and delete previous soil layers and reset counters
  SW_SIT_clear_layers();
  SW_SIT_init_counts();

  // Create new soil
  for (i = 0; i < nlyrs; i++)
  {
    // Create the next soil layer
    lyrno = _newlayer();

    v->lyr[lyrno]->width = dmax[i] - dmin;
    dmin = dmax[i];
    v->lyr[lyrno]->soilMatric_density = matricd[i];
    v->lyr[lyrno]->fractionVolBulk_gravel = f_gravel[i];
    v->lyr[lyrno]->evap_coeff = evco[i];

    ForEachVegType(k)
    {
      switch (k)
      {
        case SW_TREES:
          v->lyr[lyrno]->transp_coeff[k] = trco_tree[i];
          break;
        case SW_SHRUB:
          v->lyr[lyrno]->transp_coeff[k] = trco_shrub[i];
          break;
        case SW_FORBS:
          v->lyr[lyrno]->transp_coeff[k] = trco_forb[i];
          break;
        case SW_GRASS:
          v->lyr[lyrno]->transp_coeff[k] = trco_grass[i];
          break;
      }
    }

    v->lyr[lyrno]->fractionWeightMatric_sand = psand[i];
    v->lyr[lyrno]->fractionWeightMatric_clay = pclay[i];
    v->lyr[lyrno]->impermeability = imperm[i];
    v->lyr[lyrno]->sTemp = soiltemp[i];
  }


  // Guess soil transpiration regions
  derive_soilRegions(nRegions, regionLowerBounds);

  // Re-initialize site parameters based on new soil layers
  SW_SIT_init_run();
}



/**
  @brief Resets soil regions based on input parameters.

  @param nRegions The number of transpiration regions to create. Must be between
    1 and \ref MAX_TRANSP_REGIONS.
  @param[in] regionLowerBounds Array of size \p nRegions containing the lower
    depth [cm] of each region in ascending (in value) order. If you think about
    this from the perspective of soil, it would mean the shallowest bound is at
    `lowerBounds[0]`.

  @sideeffect
    \ref _TranspRgnBounds and \ref SW_SITE.n_transp_rgn will be
    derived from the input and from the soil information.

  @note
  - \p nRegions does NOT determine how many regions will be derived. It only
    defines the size of the \p regionLowerBounds array. For example, if your
    input parameters are `(4, { 10, 20, 40 })`, but there is a soil layer from
    41 to 60 cm, it will be placed in `_TranspRgnBounds[4]`.
*/
void derive_soilRegions(int nRegions, RealD *regionLowerBounds){
	int i, j;
	SW_SITE *v = &SW_Site;
	RealD totalDepth = 0;
	LyrIndex layer, UNDEFINED_LAYER = 999;

	/* ------------- Error checking --------------- */
	if(nRegions < 1 || nRegions > MAX_TRANSP_REGIONS){
		LogError(logfp, LOGFATAL, "derive_soilRegions: invalid number of regions (%d)\n", nRegions);
		return;
	}

	/* --------------- Clear out the array ------------------ */
	for(i = 0; i < MAX_TRANSP_REGIONS; ++i){
		// Setting bounds to a ridiculous number so we
		// know how many get set.
		_TranspRgnBounds[i] = UNDEFINED_LAYER;
	}

	/* ----------------- Derive Regions ------------------- */
	// Loop through the regions the user wants to derive
	layer = 0; // SW_Site.lyr is base0-indexed
	totalDepth = 0;
	for(i = 0; i < nRegions; ++i){
		_TranspRgnBounds[i] = layer;
		// Find the layer that pushes us out of the region.
		// It becomes the bound.
		while(totalDepth < regionLowerBounds[i] &&
		      layer < v->n_layers &&
		      sum_across_vegtypes(v->lyr[layer]->transp_coeff)) {
			totalDepth += v->lyr[layer]->width;
			_TranspRgnBounds[i] = layer;
			layer++;
		}
	}

	/* -------------- Check for duplicates -------------- */
	for(i = 0; i < nRegions - 1; ++i){
		// If there is a duplicate bound we will remove it by left shifting the
		// array, overwriting the duplicate.
		if(_TranspRgnBounds[i] == _TranspRgnBounds[i + 1]){
			for(j = i + 1; j < nRegions - 1; ++j){
				_TranspRgnBounds[j] = _TranspRgnBounds[j + 1];
			}
			_TranspRgnBounds[MAX_TRANSP_REGIONS - 1] = UNDEFINED_LAYER;
		}
	}

	/* -------------- Derive n_transp_rgn --------------- */
	v->n_transp_rgn = 0;
	while(v->n_transp_rgn < MAX_TRANSP_REGIONS &&
	      _TranspRgnBounds[v->n_transp_rgn] != UNDEFINED_LAYER) {
		v->n_transp_rgn++;
	}
}



/** Obtain soil water retention curve parameters from disk
*/
void SW_SWRC_read(void) {

	/* Don't read values from disk if they will be estimated via a PDF */
	if (SW_Site.site_pdf_type != 0) return;

	FILE *f;
	LyrIndex lyrno = 0, k;
	int x;
	RealF tmp_swrcp[SWRC_PARAM_NMAX];

	/* note that Files.read() must be called prior to this. */
	MyFileName = SW_F_name(eSWRCp);

	f = OpenFile(MyFileName, "r");

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
			CloseFile(&f);
			LogError(
				logfp,
				LOGFATAL,
				"%s : Bad number of SWRC parameters %d -- must be %d.\n",
				MyFileName, x, SWRC_PARAM_NMAX
			);
		}

		/* Check that we are within `SW_Site.n_layers` */
		if (lyrno >= SW_Site.n_layers) {
			CloseFile(&f);
			LogError(
				logfp,
				LOGFATAL,
				"%s : Number of layers with SWRC parameters (%d) "
				"must match number of soil layers (%d)\n",
				MyFileName, lyrno + 1, SW_Site.n_layers
			);
		}

		/* Copy values into structure */
		for (k = 0; k < SWRC_PARAM_NMAX; k++) {
			SW_Site.lyr[lyrno]->swrcp[k] = tmp_swrcp[k];
		}

		lyrno++;
	}

	CloseFile(&f);
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

	@sideeffect Values stored in global variable `SW_Site`.
*/
void SW_SIT_init_run(void) {
	/* =================================================== */
	/* potentially this routine can be called whether the
	 * layer data came from a file or a function call which
	 * still requires initialization.
	 */
	/* 5-Mar-2002 (cwb) added normalization for ev and tr coefficients */
	/* 1-Oct-03 (cwb) removed sum_evap_coeff and sum_transp_coeff  */

	SW_SITE *sp = &SW_Site;
	SW_LAYER_INFO *lyr;
	LyrIndex s, r, curregion;
	int k, flagswpcrit = 0;
	RealD
		evsum = 0., trsum_veg[NVEGTYPES] = {0.},
		swcmin_help1, swcmin_help2, tmp;

	#ifdef SWDEBUG
	int debug = 0;
	#endif


	/* Determine number of layers with potential for
		 bare-soil evaporation and transpiration */
	sp->n_evap_lyrs = nlayers_bsevap();
	nlayers_vegroots(sp->n_transp_lyrs);


	/* Manage deep drainage */
	add_deepdrain_layer();


	/* Check compatibility between selected SWRC and PDF */
	if (!check_SWRC_vs_PDF(sp->site_swrc_name, sp->site_pdf_name, swTRUE)) {
		LogError(
			logfp,
			LOGFATAL,
			"Selected PDF '%s' is incompatible with selected SWRC '%s'\n",
			sp->site_pdf_name,
			sp->site_swrc_name
		);
	}


	/* Loop over soil layers check variables and calculate parameters */
	ForEachSoilLayer(s)
	{
		lyr = sp->lyr[s];

		/* Copy site-level SWRC/PDF information to each layer:
		   We currently allow specifying one SWRC/PDF for a site for all layers;
		   remove in the future if we allow SWRC/PDF to vary by soil layer
		*/
		lyr->swrc_type = sp->site_swrc_type;
		lyr->pdf_type = sp->site_pdf_type;


		/* Check soil properties for valid values */
		if (!SW_check_soil_properties(lyr)) {
			LogError(
				logfp,
				LOGFATAL,
				"Invalid soil properties in layer %d.\n",
				lyr->id + 1
			);
		}


		/* Update soil density for gravel */
		lyr->soilBulk_density = calculate_soilBulkDensity(
			lyr->soilMatric_density,
			lyr->fractionVolBulk_gravel
		);


		if (lyr->pdf_type > 0) {
			/* Use pedotransfer function PDF
			   estimate parameters of soil water retention curve (SWRC) for layer.
			   If pdf_type == 0, then the parameters were already obtained from disk
			   by `SW_SWRC_read()`
			*/
			SWRC_PDF_estimate_parameters(
				lyr->pdf_type,
				lyr->swrcp,
				lyr->fractionWeightMatric_sand,
				lyr->fractionWeightMatric_clay,
				lyr->fractionVolBulk_gravel
			);
		}

		/* Check parameters of selected SWRC */
		if (!SWRC_check_parameters(lyr->swrc_type, lyr->swrcp)) {
			LogError(
				logfp,
				LOGFATAL,
				"Checks of parameters for SWRC '%s' in layer %d failed.",
				swrc2str[lyr->swrc_type],
				lyr->id + 1
			);
		}

		/* Calculate SWC at field capacity and at wilting point */
		lyr->swcBulk_fieldcap = SW_SWRC_SWPtoSWC(0.333, lyr);
		lyr->swcBulk_wiltpt = SW_SWRC_SWPtoSWC(15., lyr);

		/* Calculate lower SWC limit of bare-soil evaporation
			as `max(0.5 * wiltpt, SWC@hygroscopic)`
			Notes:
				- `0.5 * wiltpt` is the E_soil limit from FAO-56 (Allen et al. 1998)
					which may correspond to unrealistically extreme SWP
				- `SWC at hygroscopic point` (-10 MPa; e.g., Porporato et al. 2001)
					describes "air-dry" soil
				- Also make sure that `>= swc_min`, see below
		*/
		lyr->swcBulk_halfwiltpt = fmax(
			0.5 * lyr->swcBulk_wiltpt,
			SW_SWRC_SWPtoSWC(100., lyr)
		);


		/* Extract or estimate additional properties */
		lyr->swcBulk_saturated = SWRC_PDF_swcBulk_saturated(
			lyr->swrc_type,
			lyr->swrcp,
			lyr->fractionVolBulk_gravel,
			lyr->width,
			lyr->pdf_type,
			lyr->fractionWeightMatric_sand,
			lyr->fractionWeightMatric_clay
		);





		/* Compute swc wet and dry limits and init value */
		if (LT(_SWCMinVal, 0.0)) {
			/* input: estimate mininum SWC */

			/* residual VWC of Rawls & Brakensiek (1985) */
			PDF_RawlsBrakensiek1985(
				&swcmin_help1,
				lyr->fractionWeightMatric_sand,
				lyr->fractionWeightMatric_clay,
				lyr->swcBulk_saturated / ((1. - lyr->fractionVolBulk_gravel) * lyr->width)
			);

			/* Lower limit for swc_min
				Notes:
				- used in case the equation for residual SWC doesn't work or
					produces unrealistic small values
				- currently, -30 MPa
					(based on limited test runs across western US including hot deserts)
					lower than "air-dry" = hygroscopic point (-10. MPa; Porporato et al. 2001)
					not as extreme as "oven-dry" (-1000. MPa; Fredlund et al. 1994)
			*/
			swcmin_help2 = SW_SWRC_SWPtoSWC(300., lyr) / lyr->width;

			// if `SW_VWCBulkRes()` returns SW_MISSING then use `swcmin_help2`
			if (missing(swcmin_help1)) {
				lyr->swcBulk_min = swcmin_help2;

			} else {
				lyr->swcBulk_min = fmax(
					swcmin_help1 * (1. - lyr->fractionVolBulk_gravel),
					swcmin_help2
				);
			}

		} else if (GE(_SWCMinVal, 1.0)) {
			/* input: fixed SWP value as minimum SWC; unit(_SWCMinVal) == -bar */
			lyr->swcBulk_min = SW_SWRC_SWPtoSWC(_SWCMinVal, lyr) / lyr->width;

		} else {
			/* input: fixed VWC value as minimum SWC; unit(_SWCMinVal) == cm/cm */
			lyr->swcBulk_min = _SWCMinVal;
		}

		/* Convert VWC to SWC */
		lyr->swcBulk_min *= lyr->width;


		/* Calculate wet limit of SWC for what inputs defined as wet */
		lyr->swcBulk_wet = GE(_SWCWetVal, 1.0) ?
			SW_SWRC_SWPtoSWC(_SWCWetVal, lyr) :
			_SWCWetVal * lyr->width;

		/* Calculate initial SWC based on inputs */
		lyr->swcBulk_init = GE(_SWCInitVal, 1.0) ?
			SW_SWRC_SWPtoSWC(_SWCInitVal, lyr) :
			_SWCInitVal * lyr->width;


		/* test validity of values */
		if (LT(lyr->swcBulk_init, lyr->swcBulk_min)) {
			LogError(
				logfp, LOGFATAL,
				"%s : Layer %d\n"
				"  calculated `swcBulk_init` (%.4f cm) <= `swcBulk_min` (%.4f cm).\n"
				"  Recheck parameters and try again.\n",
				MyFileName, lyr->id + 1, lyr->swcBulk_init, lyr->swcBulk_min
			);
		}

		if (LT(lyr->swcBulk_wiltpt, lyr->swcBulk_min)) {
			LogError(
				logfp, LOGFATAL,
				"%s : Layer %d\n"
				"  calculated `swcBulk_wiltpt` (%.4f cm) <= `swcBulk_min` (%.4f cm).\n"
				"  Recheck parameters and try again.\n",
				MyFileName, lyr->id + 1, lyr->swcBulk_wiltpt, lyr->swcBulk_min
			);
		}

		if (LT(lyr->swcBulk_halfwiltpt, lyr->swcBulk_min)) {
			LogError(
				logfp, LOGWARN,
				"%s : Layer %d\n"
				"  calculated `swcBulk_halfwiltpt` (%.4f cm / %.2f MPa)\n"
				"          <= `swcBulk_min` (%.4f cm / %.2f MPa).\n"
				"  `swcBulk_halfwiltpt` was set to `swcBulk_min`.\n",
				MyFileName, lyr->id + 1,
				lyr->swcBulk_halfwiltpt,
				-0.1 * SW_SWRC_SWCtoSWP(lyr->swcBulk_halfwiltpt, lyr),
				lyr->swcBulk_min,
				-0.1 * SW_SWRC_SWCtoSWP(lyr->swcBulk_min, lyr)
			);

			lyr->swcBulk_halfwiltpt = lyr->swcBulk_min;
		}

		if (LE(lyr->swcBulk_wet, lyr->swcBulk_min)) {
			LogError(
				logfp, LOGFATAL,
				"%s : Layer %d\n"
				"  calculated `swcBulk_wet` (%.4f cm) <= `swcBulk_min` (%.4f cm).\n"
				"  Recheck parameters and try again.\n",
				MyFileName, lyr->id + 1, lyr->swcBulk_wet, lyr->swcBulk_min
			);
		}


		#ifdef SWDEBUG
		if (debug) {
			swprintf(
				"L[%d] swcmin=%f = swpmin=%f\n",
				s,
				lyr->swcBulk_min,
				SW_SWRC_SWCtoSWP(lyr->swcBulk_min, lyr)
			);

			swprintf(
				"L[%d] SWC(HalfWiltpt)=%f = swp(hw)=%f\n",
				s,
				lyr->swcBulk_halfwiltpt,
				SW_SWRC_SWCtoSWP(lyr->swcBulk_halfwiltpt, lyr)
			);
		}
		#endif


		/* sum ev and tr coefficients for later */
		evsum += lyr->evap_coeff;

		ForEachVegType(k)
		{
			trsum_veg[k] += lyr->transp_coeff[k];
			/* calculate soil water content at SWPcrit for each vegetation type */
			lyr->swcBulk_atSWPcrit[k] = SW_SWRC_SWPtoSWC(
				SW_VegProd.veg[k].SWPcrit,
				lyr
			);

			if (LT(lyr->swcBulk_atSWPcrit[k], lyr->swcBulk_min)) {
				flagswpcrit++;

				// lower SWcrit [-bar] to SWP-equivalent of swBulk_min
				tmp = fmin(
					SW_VegProd.veg[k].SWPcrit,
					SW_SWRC_SWCtoSWP(lyr->swcBulk_min, lyr)
				);

				LogError(
					logfp, LOGWARN,
					"%s : Layer %d - vegtype %d\n"
					"  calculated `swcBulk_atSWPcrit` (%.4f cm / %.4f MPa)\n"
					"          <= `swcBulk_min` (%.4f cm / %.4f MPa).\n"
					"  `SWcrit` adjusted to %.4f MPa "
					"(and swcBulk_atSWPcrit in every layer will be re-calculated).\n",
					MyFileName, lyr->id + 1, k + 1,
					lyr->swcBulk_atSWPcrit[k],
					-0.1 * SW_VegProd.veg[k].SWPcrit,
					lyr->swcBulk_min,
					-0.1 * SW_SWRC_SWCtoSWP(lyr->swcBulk_min, lyr),
					-0.1 * tmp
				);

				SW_VegProd.veg[k].SWPcrit = tmp;
			}

			/* Find which transpiration region the current soil layer
			 * is in and check validity of result. Region bounds are
			 * base1 but s is base0.*/
			curregion = 0;
			ForEachTranspRegion(r)
			{
				if (s < _TranspRgnBounds[r]) {
					if (ZRO(lyr->transp_coeff[k]))
						break; /* end of transpiring layers */
					curregion = r + 1;
					break;
				}
			}

			if (curregion || _TranspRgnBounds[curregion] == 0) {
				lyr->my_transp_rgn[k] = curregion;
				sp->n_transp_lyrs[k] = max(sp->n_transp_lyrs[k], s);

			} else if (s == 0) {
				LogError(logfp, LOGFATAL, "%s : Top soil layer must be included\n"
						"  in %s tranpiration regions.\n", SW_F_name(eSite), key2veg[k]);
			} else if (r < sp->n_transp_rgn) {
				LogError(logfp, LOGFATAL, "%s : Transpiration region %d \n"
						"  is deeper than the deepest layer with a\n"
						"  %s transpiration coefficient > 0 (%d) in '%s'.\n"
						"  Please fix the discrepancy and try again.\n",
						SW_F_name(eSite), r + 1, key2veg[k], s, SW_F_name(eLayers));
			} else {
				lyr->my_transp_rgn[k] = 0;
			}
		}
	} /*end ForEachSoilLayer */


	/* Re-calculate `swcBulk_atSWPcrit`
			if it was below `swcBulk_min` for any vegetation x soil layer combination
			using adjusted `SWPcrit`
	*/
	if (flagswpcrit) {
		ForEachSoilLayer(s)
		{
			lyr = sp->lyr[s];

			ForEachVegType(k)
			{
				/* calculate soil water content at adjusted SWPcrit */
				lyr->swcBulk_atSWPcrit[k] = SW_SWRC_SWPtoSWC(
					SW_VegProd.veg[k].SWPcrit,
					lyr
				);

				if (LT(lyr->swcBulk_atSWPcrit[k], lyr->swcBulk_min)) {
					LogError(
						logfp, LOGFATAL,
						"%s : Layer %d - vegtype %d\n"
						"  calculated `swcBulk_atSWPcrit` (%.4f cm)\n"
						"          <= `swcBulk_min` (%.4f cm).\n"
						"  even with adjusted `SWcrit` (%.4f MPa).\n",
						MyFileName, lyr->id + 1, k + 1,
						lyr->swcBulk_atSWPcrit[k],
						lyr->swcBulk_min,
						-0.1 * SW_VegProd.veg[k].SWPcrit
					);
				}
			}
		}

		/* Update values for `get_swa()` */
		ForEachVegType(k)
		{
			SW_VegProd.critSoilWater[k] = -0.1 * SW_VegProd.veg[k].SWPcrit;
		}
		get_critical_rank();
	}

	/* normalize the evap and transp coefficients separately
	 * to avoid obfuscation in the above loop */
	if (!EQ_w_tol(evsum, 1.0, 1e-4)) { // inputs are not more precise than at most 3-4 digits
		LogError(
			logfp,
			LOGWARN,
			"%s : Evaporation coefficients were normalized:\n"
			"\tSum of coefficients was %.4f, but must be 1.0. "
			"New coefficients are:",
			MyFileName,
			evsum
		);

		ForEachEvapLayer(s)
		{
			SW_Site.lyr[s]->evap_coeff /= evsum;
			LogError(
				logfp,
				LOGNOTE,
				"  Layer %2d : %.4f",
				SW_Site.lyr[s]->id + 1,
				SW_Site.lyr[s]->evap_coeff
			);
		}

		LogError(logfp, LOGQUIET, "");
	}

	ForEachVegType(k)
	{
		// inputs are not more precise than at most 3-4 digits
		if (!EQ_w_tol(trsum_veg[k], 1.0, 1e-4)) {
			LogError(
				logfp,
				LOGWARN,
				"%s : Transpiration coefficients were normalized for %s:\n"
				"\tSum of coefficients was %.4f, but must be 1.0. "
				"New coefficients are:",
				MyFileName, key2veg[k], trsum_veg[k]
			);

			ForEachSoilLayer(s)
			{
				if (GT(SW_Site.lyr[s]->transp_coeff[k], 0.))
				{
					SW_Site.lyr[s]->transp_coeff[k] /= trsum_veg[k];
					LogError(
						logfp,
						LOGNOTE,
						"  Layer %2d : %.4f",
						SW_Site.lyr[s]->id + 1,
						SW_Site.lyr[s]->transp_coeff[k]
					);
				}
			}

			LogError(logfp, LOGQUIET, "");
		}
	}

	// getting the number of regressions, for use in the soil_temperature function
	sp->stNRGR = (sp->stMaxDepth / sp->stDeltaX) - 1;
	Bool too_many_RGR = (Bool) (sp->stNRGR + 1 >= MAX_ST_RGR);

	if (!EQ(fmod(sp->stMaxDepth, sp->stDeltaX), 0.0) || too_many_RGR) {

		if (too_many_RGR)
		{ // because we will use loops such `for (i = 0; i <= nRgr + 1; i++)`
			LogError(
				logfp,
				LOGWARN,
				"\nSOIL_TEMP FUNCTION ERROR: the number of regressions is > the "
				"maximum number of regressions.  resetting max depth, deltaX, nRgr "
				"values to 180, 15, & 11 respectively\n"
			);
		}
		else
		{ // because we don't deal with partial layers
			LogError(
				logfp,
				LOGWARN,
				"\nSOIL_TEMP FUNCTION ERROR: max depth is not evenly divisible by "
				"deltaX (ie the remainder != 0).  resetting max depth, deltaX, nRgr "
				"values to 180, 15, & 11 respectively\n"
			);
		}

		// resets it to the default values
		sp->stMaxDepth = 180.0;
		sp->stNRGR = 11;
		sp->stDeltaX = 15.0;
	}


	if (EchoInits)
	{
		_echo_inputs();
	}
}

/**
@brief For multiple runs with the shared library, the need to remove the allocated
			soil layers arises to avoid leaks. (rjm 2013)
*/
void SW_SIT_clear_layers(void) {
	SW_SITE *s = &SW_Site;
	LyrIndex i, j;

	j = SW_Site.n_layers;

	if (s->deepdrain && s->deep_lyr > 0) {
		j++;
	}

	for (i = 0; i < j; i++) {
		free(s->lyr[i]);
		s->lyr[i] = NULL;
	}
	free(s->lyr);
	s->lyr = NULL;
}


/**
	@brief Reset counts of `SW_Site` to zero
*/
void SW_SIT_init_counts(void) {
	int k;
	SW_SITE *s = &SW_Site;

	// Reset counts
	s->n_layers = 0;
	s->n_evap_lyrs = 0;
	s->deep_lyr = 0;
	s->n_transp_rgn = 0;

	ForEachVegType(k)
	{
		s->n_transp_lyrs[k] = 0;
	}
}

/**
@brief Print site-parameters and soil characteristics.
*/
void _echo_inputs(void) {
	/* =================================================== */
	SW_SITE *s = &SW_Site;
	LyrIndex i;

	LogError(logfp, LOGNOTE, "\n\n=====================================================\n"
			"Site Related Parameters:\n"
			"---------------------\n");
	LogError(logfp, LOGNOTE, "  Site File: %s\n", SW_F_name(eSite));
	LogError(logfp, LOGNOTE, "  Reset SWC values each year: %s\n", (s->reset_yr) ? "swTRUE" : "swFALSE");
	LogError(logfp, LOGNOTE, "  Use deep drainage reservoir: %s\n", (s->deepdrain) ? "swTRUE" : "swFALSE");
	LogError(logfp, LOGNOTE, "  Slow Drain Coefficient: %5.4f\n", s->slow_drain_coeff);
	LogError(logfp, LOGNOTE, "  PET Scale: %5.4f\n", s->pet_scale);
	LogError(logfp, LOGNOTE, "  Runoff: proportion of surface water lost: %5.4f\n", s->percentRunoff);
	LogError(logfp, LOGNOTE, "  Runon: proportion of new surface water gained: %5.4f\n", s->percentRunon);
	LogError(logfp, LOGNOTE, "  Longitude (degree): %4.2f\n", s->longitude * rad_to_deg);
	LogError(logfp, LOGNOTE, "  Latitude (degree): %4.2f\n", s->latitude * rad_to_deg);
	LogError(logfp, LOGNOTE, "  Altitude (m a.s.l.): %4.2f \n", s->altitude);
	LogError(logfp, LOGNOTE, "  Slope (degree): %4.2f\n", s->slope * rad_to_deg);
	LogError(logfp, LOGNOTE, "  Aspect (degree): %4.2f\n", s->aspect * rad_to_deg);

	LogError(logfp, LOGNOTE, "\nSnow simulation parameters (SWAT2K model):\n----------------------\n");
	LogError(logfp, LOGNOTE, "  Avg. air temp below which ppt is snow ( C): %5.4f\n", s->TminAccu2);
	LogError(logfp, LOGNOTE, "  Snow temperature at which snow melt starts ( C): %5.4f\n", s->TmaxCrit);
	LogError(logfp, LOGNOTE, "  Relative contribution of avg. air temperature to todays snow temperture vs. yesterday's snow temperature (0-1): %5.4f\n", s->lambdasnow);
	LogError(logfp, LOGNOTE, "  Minimum snow melt rate on winter solstice (cm/day/C): %5.4f\n", s->RmeltMin);
	LogError(logfp, LOGNOTE, "  Maximum snow melt rate on summer solstice (cm/day/C): %5.4f\n", s->RmeltMax);

	LogError(logfp, LOGNOTE, "\nSoil Temperature Constants:\n----------------------\n");
	LogError(logfp, LOGNOTE, "  Biomass Limiter constant: %5.4f\n", s->bmLimiter);
	LogError(logfp, LOGNOTE, "  T1Param1: %5.4f\n", s->t1Param1);
	LogError(logfp, LOGNOTE, "  T1Param2: %5.4f\n", s->t1Param2);
	LogError(logfp, LOGNOTE, "  T1Param3: %5.4f\n", s->t1Param3);
	LogError(logfp, LOGNOTE, "  csParam1: %5.4f\n", s->csParam1);
	LogError(logfp, LOGNOTE, "  csParam2: %5.4f\n", s->csParam2);
	LogError(logfp, LOGNOTE, "  shParam: %5.4f\n", s->shParam);
	LogError(logfp, LOGNOTE, "  Tsoil_constant: %5.4f\n", s->Tsoil_constant);
	LogError(logfp, LOGNOTE, "  deltaX: %5.4f\n", s->stDeltaX);
	LogError(logfp, LOGNOTE, "  max depth: %5.4f\n", s->stMaxDepth);
	LogError(logfp, LOGNOTE, "  Make soil temperature calculations: %s\n", (s->use_soil_temp) ? "swTRUE" : "swFALSE");
	LogError(logfp, LOGNOTE, "  Number of regressions for the soil temperature function: %d\n", s->stNRGR);

	LogError(logfp, LOGNOTE, "\nLayer Related Values:\n----------------------\n");
	LogError(logfp, LOGNOTE, "  Soils File: %s\n", SW_F_name(eLayers));
	LogError(logfp, LOGNOTE, "  Number of soil layers: %d\n", s->n_layers);
	LogError(logfp, LOGNOTE, "  Number of evaporation layers: %d\n", s->n_evap_lyrs);
	LogError(logfp, LOGNOTE, "  Number of forb transpiration layers: %d\n", s->n_transp_lyrs[SW_FORBS]);
	LogError(logfp, LOGNOTE, "  Number of tree transpiration layers: %d\n", s->n_transp_lyrs[SW_TREES]);
	LogError(logfp, LOGNOTE, "  Number of shrub transpiration layers: %d\n", s->n_transp_lyrs[SW_SHRUB]);
	LogError(logfp, LOGNOTE, "  Number of grass transpiration layers: %d\n", s->n_transp_lyrs[SW_GRASS]);
	LogError(logfp, LOGNOTE, "  Number of transpiration regions: %d\n", s->n_transp_rgn);

	LogError(logfp, LOGNOTE, "\nLayer Specific Values:\n----------------------\n");
	LogError(logfp, LOGNOTE, "\n  Layer information on a per centimeter depth basis:\n");
	LogError(logfp, LOGNOTE,
			"  Lyr Width   BulkD 	%%Gravel    FieldC   WiltPt   %%Sand  %%Clay VWC at Forb-critSWP 	VWC at Tree-critSWP	VWC at Shrub-critSWP	VWC at Grass-critSWP	EvCo   	TrCo_Forb   TrCo_Tree  TrCo_Shrub  TrCo_Grass   TrRgn_Forb    TrRgn_Tree   TrRgn_Shrub   TrRgn_Grass   Wet     Min      Init     Saturated    Impermeability\n");
	LogError(logfp, LOGNOTE,
			"       (cm)   (g/cm^3)  (prop)    (cm/cm)  (cm/cm)   (prop) (prop)  (cm/cm)			(cm/cm)                (cm/cm)            		(cm/cm)         (prop)    (prop)      (prop)     (prop)    (prop)        (int)           (int) 	      	(int) 	    (int) 	    (cm/cm)  (cm/cm)  (cm/cm)  (cm/cm)      (frac)\n");
	LogError(logfp, LOGNOTE,
			"  --- -----   ------    ------     ------   ------   -----  ------   ------                	-------			------            		------          ------    ------      ------      ------   ------       ------   	 -----	        -----       -----   	 ----     ----     ----    ----         ----\n");
	ForEachSoilLayer(i)
	{
		LogError(logfp, LOGNOTE,
				"  %3d %5.1f %9.5f %6.2f %8.5f %8.5f %6.2f %6.2f %6.2f %6.2f %6.2f %6.2f %9.2f %9.2f %9.2f %9.2f %9.2f %10d %10d %15d %15d %15.4f %9.4f %9.4f %9.4f %9.4f\n",
				i + 1, s->lyr[i]->width, s->lyr[i]->soilBulk_density, s->lyr[i]->fractionVolBulk_gravel, s->lyr[i]->swcBulk_fieldcap / s->lyr[i]->width,
				s->lyr[i]->swcBulk_wiltpt / s->lyr[i]->width, s->lyr[i]->fractionWeightMatric_sand, s->lyr[i]->fractionWeightMatric_clay,
				s->lyr[i]->swcBulk_atSWPcrit[SW_FORBS] / s->lyr[i]->width, s->lyr[i]->swcBulk_atSWPcrit[SW_TREES] / s->lyr[i]->width,
				s->lyr[i]->swcBulk_atSWPcrit[SW_SHRUB] / s->lyr[i]->width, s->lyr[i]->swcBulk_atSWPcrit[SW_GRASS] / s->lyr[i]->width, s->lyr[i]->evap_coeff,
				s->lyr[i]->transp_coeff[SW_FORBS], s->lyr[i]->transp_coeff[SW_TREES], s->lyr[i]->transp_coeff[SW_SHRUB], s->lyr[i]->transp_coeff[SW_GRASS], s->lyr[i]->my_transp_rgn[SW_FORBS],
				s->lyr[i]->my_transp_rgn[SW_TREES], s->lyr[i]->my_transp_rgn[SW_SHRUB], s->lyr[i]->my_transp_rgn[SW_GRASS], s->lyr[i]->swcBulk_wet / s->lyr[i]->width,
				s->lyr[i]->swcBulk_min / s->lyr[i]->width, s->lyr[i]->swcBulk_init / s->lyr[i]->width, s->lyr[i]->swcBulk_saturated / s->lyr[i]->width,
				s->lyr[i]->impermeability);

	}
	LogError(logfp, LOGNOTE, "\n  Actual per-layer values:\n");
	LogError(logfp, LOGNOTE,
			"  Lyr Width  BulkD	 %%Gravel   FieldC   WiltPt %%Sand  %%Clay	SWC at Forb-critSWP     SWC at Tree-critSWP	SWC at Shrub-critSWP	SWC at Grass-critSWP	 Wet    Min      Init  Saturated	SoilTemp\n");
	LogError(logfp, LOGNOTE,
			"       (cm)  (g/cm^3)	(prop)    (cm)     (cm)  (prop) (prop)   (cm)    	(cm)        		(cm)            (cm)            (cm)   (cm)      (cm)     (cm)		(celcius)\n");
	LogError(logfp, LOGNOTE,
			"  --- -----  -------	------   ------   ------ ------ ------   ------        	------            	------          ----   		----     ----     ----    ----		----\n");

	ForEachSoilLayer(i)
	{
		LogError(logfp, LOGNOTE, "  %3d %5.1f %9.5f %6.2f %8.5f %8.5f %6.2f %6.2f %7.4f %7.4f %7.4f %7.4f %7.4f %7.4f %8.4f %7.4f %5.4f\n", i + 1, s->lyr[i]->width,
				s->lyr[i]->soilBulk_density, s->lyr[i]->fractionVolBulk_gravel, s->lyr[i]->swcBulk_fieldcap, s->lyr[i]->swcBulk_wiltpt, s->lyr[i]->fractionWeightMatric_sand,
				s->lyr[i]->fractionWeightMatric_clay, s->lyr[i]->swcBulk_atSWPcrit[SW_FORBS], s->lyr[i]->swcBulk_atSWPcrit[SW_TREES], s->lyr[i]->swcBulk_atSWPcrit[SW_SHRUB],
				s->lyr[i]->swcBulk_atSWPcrit[SW_GRASS], s->lyr[i]->swcBulk_wet, s->lyr[i]->swcBulk_min, s->lyr[i]->swcBulk_init, s->lyr[i]->swcBulk_saturated, s->lyr[i]->sTemp);
	}

	LogError(logfp, LOGNOTE, "\n  Water Potential values:\n");
	LogError(logfp, LOGNOTE, "  Lyr       FieldCap         WiltPt            Forb-critSWP     Tree-critSWP     Shrub-critSWP    Grass-critSWP    Wet            Min            Init\n");
	LogError(logfp, LOGNOTE,
			"            (bars)           (bars)            (bars)           (bars)           (bars)           (bars)           (bars)         (bars)         (bars)\n");
	LogError(logfp, LOGNOTE,
			"  ---       -----------      ------------      -----------      -----------      -----------      -----------      -----------    -----------    --------------    --------------\n");

	ForEachSoilLayer(i)
	{
		LogError(
			logfp,
			LOGNOTE,
			"  %3d   %15.4f   %15.4f  %15.4f %15.4f  %15.4f  %15.4f  %15.4f   %15.4f   %15.4f\n",
			i + 1,
			SW_SWRC_SWCtoSWP(s->lyr[i]->swcBulk_fieldcap, s->lyr[i]),
			SW_SWRC_SWCtoSWP(s->lyr[i]->swcBulk_wiltpt, s->lyr[i]),
			SW_SWRC_SWCtoSWP(s->lyr[i]->swcBulk_atSWPcrit[SW_FORBS], s->lyr[i]),
			SW_SWRC_SWCtoSWP(s->lyr[i]->swcBulk_atSWPcrit[SW_TREES], s->lyr[i]),
			SW_SWRC_SWCtoSWP(s->lyr[i]->swcBulk_atSWPcrit[SW_SHRUB], s->lyr[i]),
			SW_SWRC_SWCtoSWP(s->lyr[i]->swcBulk_atSWPcrit[SW_SHRUB], s->lyr[i]),
			SW_SWRC_SWCtoSWP(s->lyr[i]->swcBulk_atSWPcrit[SW_GRASS], s->lyr[i]),
			SW_SWRC_SWCtoSWP(s->lyr[i]->swcBulk_min, s->lyr[i]),
			SW_SWRC_SWCtoSWP(s->lyr[i]->swcBulk_init, s->lyr[i])
		);
	}

	LogError(
		logfp,
		LOGNOTE,
		"\nSoil Water Retention Curve:\n---------------------------\n"
	);
	LogError(
		logfp,
		LOGNOTE,
		"  SWRC type: %d (%s)\n",
		s->site_swrc_type,
		s->site_swrc_name
	);
	LogError(
		logfp,
		LOGNOTE,
		"  PDF type: %d (%s)\n",
		s->site_pdf_type,
		s->site_pdf_name
	);

	LogError(
		logfp,
		LOGNOTE,
		"  Lyr     Param1     Param2     Param3     Param4     Param5     Param6\n"
	);
	ForEachSoilLayer(i)
	{
		LogError(
			logfp,
			LOGNOTE,
			"  %3d%11.4f%11.4f%11.4f%11.4f%11.4f%11.4f\n",
			i + 1,
			s->lyr[i]->swrcp[0],
			s->lyr[i]->swrcp[1],
			s->lyr[i]->swrcp[2],
			s->lyr[i]->swrcp[3],
			s->lyr[i]->swrcp[4],
			s->lyr[i]->swrcp[5]
		);
	}



	LogError(
		logfp,
		LOGNOTE,
		"\n------------ End of Site Parameters ------------------\n"
	);
	//fflush(logfp);
}

#ifdef DEBUG_MEM
#include "myMemory.h"
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
