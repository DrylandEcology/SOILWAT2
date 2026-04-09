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

 11/02/2010	(drs) added 5 snow parameters to SW_SITE to be read in from
 siteparam.in

 10/19/2010	(drs) added HydraulicRedistribution flag, and maxCondroot,
 swp50, and shapeCond parameters to SW_SIT_read()  and echo_inputs()

 07/20/2011	(drs) updated _read_layers() to read impermeability values from
 each soil layer from soils.in file added calculation for saturated swc in
 water_eqn() updated echo_inputs() to print impermeability and saturated swc
 values

 09/08/2011	(drs) moved all hydraulic redistribution parameters to
 SW_VegProd.h struct VegType

 09/15/2011	(drs)	deleted albedo from SW_SIT_read() and echo_inputs():
 moved it to SW_VegProd.h to make input vegetation type dependent

 02/03/2012	(drs)	if input of SWCmin < 0 then estimate SWCmin with
 'SW_SWC_SWCres' for each soil layer

 02/04/2012	(drs)	included SW_VegProd.h and created global variable extern
 SW_VegProd: to access vegetation-specific SWPcrit

 02/04/2012	(drs)	added calculation of swc at SWPcrit for each vegetation
 type and layer to function 'init_site_info()' added vwc/swc at SWPcrit to
 'echo_inputs()'

 05/24/2012  (DLM) edited SW_SIT_read(void) function to be able to read in Soil
 Temperature constants from siteparam.in file

 05/24/2012  (DLM) edited echo_inputs(void) function to echo the Soil
 Temperature constants to the logfile

 05/25/2012  (DLM) edited _read_layers( void) function to read in the initial
 soil temperature for each layer

 05/25/2012  (DLM) edited echo_inputs( void) function to echo the read in soil
 temperatures for each layer

 05/30/2012  (DLM) edited _read_layers & echo_inputs functions to read in/echo
 the deltaX parameter

 05/31/2012  (DLM) edited _read_layers & echo_inputs functions to read in/echo
 stMaxDepth & use_soil_temp variables

 05/31/2012  (DLM) edited init_site_info(void) to check if stMaxDepth & stDeltaX
 values are usable, if not it resets them to the defaults (180 & 15).

 11/06/2012	(clk)	In SW_SIT_read(void), added lines to read in aspect and
 slope from siteparam.in

 11/06/2012	(clk)	In echo_inputs(void), added lines to echo aspect and
 slope to logfile

 11/30/2012	(clk)	In SW_SIT_read(void), added lines to read in
 percentRunoff from siteparam.in

 11/30/2012	(clk)	In echo_inputs(void), added lines to echo percentRunoff
 to logfile

 04/16/2013	(clk)	changed the water_eqn to use the fraction of gravel
 content in the calculation Added the function calculate_soilBulkDensity() which
 is used to calculate the bulk density of the soil from the inputed matric
 density. Using eqn 20 from Saxton 2006 Needed to change the input from soils.in
 to save to soilMatric_density instead of soilBulk_density Changed read_layers()
 to do a few different things First, it now reads in a value for
 fractionVolBulk_gravel from soils.in Secondly, it calls the
 calculate_soilBulkDensity function for each layer Lastly, since fieldcap and
 wiltpt were removed from soils.in, those values are now calculated within
 read_layers()

 05/16/2013	(drs)	fixed in init_site_info() the check of transpiration
 region validity: it gave error if only one layer was present

 06/24/2013	(rjm)	added function void SW_SIT_clear_layers(void) to free
 allocated soil layers

 06/27/2013	(drs)	closed open files if LogError() with LOGERROR is called
 in SW_SIT_read(), _read_layers()

 07/09/2013	(clk)	added the initialization of all the new variables

 06/05/2016 (ctd) Modified threshold for condition involving gravel in
 _read_layers() function - as per Caitlin's request. Also, added print
 statements to notify the user that values may be invalid if the gravel content
 does not follow parameters of Corey-Brooks equation.
 */


/********************************************************/
/********************************************************/

/* =================================================== */
/*                INCLUDES / DEFINES                   */
/* --------------------------------------------------- */

#include "include/SW_Site.h"        // for N_PTFs, N_SWRCs, sw_Campbell1974
#include "include/filefuncs.h"      // for LogError, CloseFile, GetALine
#include "include/generic.h"        // for LOGERROR, swFALSE, LOGWARN, MAX, R...
#include "include/myMemory.h"       // for Str_Dup
#include "include/SW_datastructs.h" // for LOG_INFO, SW_SITE, SW_VEGPROD_INPUTS
#include "include/SW_Defines.h"     // for LyrIndex, ForEachSoilLayer, ForE...
#include "include/SW_Files.h"       // for eSite, eLayers, eSWRCp
#include "include/SW_Main_lib.h"    // for sw_init_logs
#include "include/SW_SoilWater.h"   // for SW_SWRC_SWCtoSWP, SW_SWRC_SWPtoSWC
#include "include/SW_VegProd.h"     // for key2veg, get_critical_rank, sum_...
#include <limits.h>                 // for UINT_MAX
#include <math.h>                   // for fmod, round
#include <stdio.h>                  // for sw_printf, sscanf, FILE, NULL, stdout
#include <stdlib.h>                 // for free, strod, strtol
#include <string.h>                 // for memset

/* =================================================== */
/*                  Global Variables                   */
/* --------------------------------------------------- */

/** Character representation of implemented Soil Water Retention Curves (SWRC)

@note Code maintenance:
    - Values must exactly match those provided in `siteparam.in`.
    - Order must exactly match "indices of `swrc2str`"
    - See details in section \ref swrc_ptf
*/
const char *const swrc2str[N_SWRCs] = {
    "Campbell1974", "vanGenuchten1980", "FXW"
};


/** Index to saturated hydraulic conductivity parameter for each SWRC

@note Code maintenance:
    - Order must exactly match "indices of `swrc2str`" (see also \ref swrc2str)
    - See details in section \ref swrc_ptf
*/
const unsigned int swrcp2ksat[N_SWRCs] = {3, 4, 4};


/** Character representation of implemented Pedotransfer Functions (PTF)

@note Code maintenance:
    - Values must exactly match those provided in `siteparam.in`.
    - Order must exactly match "indices of `ptf2str`"
    - See details in section \ref swrc_ptf
    - `rSOILWAT2` may implemented additional PTFs
*/
const char *const ptf2str[N_PTFs] = {"Cosby1984AndOthers", "Cosby1984"};

/* =================================================== */
/*             Local Function Definitions              */
/* --------------------------------------------------- */

/** Check validity of soil properties

@param[in] SW_SoilsRunIn Struct of type SW_SOIL_RUN_INPUTS describing
the simulated site's input soil information
@param[in] layerno Current layer which is being worked with
@param[out] LogInfo Holds information on warnings and errors

@return A logical value indicating if soil properties passed the checks.
*/
static Bool SW_check_soil_properties(
    SW_SOIL_RUN_INPUTS *SW_SoilsRunIn, LyrIndex layerno, LOG_INFO *LogInfo
) {

    int k;
    double fval = 0;
    char *errtype = NULL;
    Bool res = swTRUE;


    if (LE(SW_SoilsRunIn->width[layerno], 0.)) {
        res = swFALSE;
        fval = SW_SoilsRunIn->width[layerno];
        errtype = Str_Dup("layer width", LogInfo);

    } else if (LT(SW_SoilsRunIn->soilDensityInput[layerno], 0.) ||
               GT(SW_SoilsRunIn->soilDensityInput[layerno], 2.65)) {
        res = swFALSE;
        fval = SW_SoilsRunIn->soilDensityInput[layerno];
        errtype = Str_Dup("soil density", LogInfo);

    } else if (LT(SW_SoilsRunIn->fractionVolBulk_gravel[layerno], 0.) ||
               GE(SW_SoilsRunIn->fractionVolBulk_gravel[layerno], 1.)) {
        res = swFALSE;
        fval = SW_SoilsRunIn->fractionVolBulk_gravel[layerno];
        errtype = Str_Dup("gravel content", LogInfo);

    } else if (LT(SW_SoilsRunIn->fractionWeightMatric_sand[layerno], 0.) ||
               GT(SW_SoilsRunIn->fractionWeightMatric_sand[layerno], 1.)) {
        res = swFALSE;
        fval = SW_SoilsRunIn->fractionWeightMatric_sand[layerno];
        errtype = Str_Dup("sand proportion", LogInfo);

    } else if (LT(SW_SoilsRunIn->fractionWeightMatric_clay[layerno], 0.) ||
               GT(SW_SoilsRunIn->fractionWeightMatric_clay[layerno], 1.)) {
        res = swFALSE;
        fval = SW_SoilsRunIn->fractionWeightMatric_clay[layerno];
        errtype = Str_Dup("clay proportion", LogInfo);

    } else if (GT(SW_SoilsRunIn->fractionWeightMatric_sand[layerno] +
                      SW_SoilsRunIn->fractionWeightMatric_clay[layerno],
                  1.)) {
        res = swFALSE;
        fval = SW_SoilsRunIn->fractionWeightMatric_sand[layerno] +
               SW_SoilsRunIn->fractionWeightMatric_clay[layerno];
        errtype = Str_Dup("sand+clay proportion", LogInfo);

    } else if (LT(SW_SoilsRunIn->impermeability[layerno], 0.) ||
               GT(SW_SoilsRunIn->impermeability[layerno], 1.)) {
        res = swFALSE;
        fval = SW_SoilsRunIn->impermeability[layerno];
        errtype = Str_Dup("impermeability", LogInfo);

    } else if (LT(SW_SoilsRunIn->fractionWeight_om[layerno], 0.) ||
               GT(SW_SoilsRunIn->fractionWeight_om[layerno], 1.)) {
        res = swFALSE;
        fval = SW_SoilsRunIn->fractionWeight_om[layerno];
        errtype = Str_Dup("organic matter content", LogInfo);

    } else if (LT(SW_SoilsRunIn->evap_coeff[layerno], 0.) ||
               GT(SW_SoilsRunIn->evap_coeff[layerno], 1.)) {
        res = swFALSE;
        fval = SW_SoilsRunIn->evap_coeff[layerno];
        errtype = Str_Dup("bare-soil evaporation coefficient", LogInfo);

    } else {
        ForEachVegType(k) {
            if (LT(SW_SoilsRunIn->transp_coeff[k][layerno], 0.) ||
                GT(SW_SoilsRunIn->transp_coeff[k][layerno], 1.)) {

                res = swFALSE;
                fval = SW_SoilsRunIn->transp_coeff[k][layerno];
                errtype = Str_Dup("transpiration coefficient", LogInfo);
                break;
            }
        }
    }

    if (!res && !isnull(errtype)) {
        LogError(
            LogInfo,
            LOGERROR,
            "'%s' has an invalid value (%f) in layer %d.",
            errtype,
            fval,
            layerno + 1
        );
        free(errtype); // Free temporary string
    }

    return res;
}

/** A realistic lower limit for minimum `theta`

@note Currently, -30 MPa
    (based on limited test runs across western US including hot
    deserts) lower than "air-dry" = hygroscopic point
    (-10. MPa; Porporato et al. 2001)
    not as extreme as "oven-dry" (-1000. MPa; Fredlund et al. 1994)
*/
static double lower_limit_of_theta_min(
    unsigned int swrc_type,
    double *swrcp,
    double gravel,
    double width,
    LOG_INFO *LogInfo
) {
    double res =
        SWRC_SWPtoSWC(300., swrc_type, swrcp, gravel, width, LOGERROR, LogInfo);

    // convert bulk [cm] to matric [cm / cm]
    return res / ((1. - gravel) * width);
}

/**
@brief User-input based minimum volumetric water content

This function returns minimum soil water content `theta_min` determined
by user input via #SW_SITE_INPUTS.SWCMinVal as
    * a fixed VWC value,
    * a fixed SWP value, or
    * a realistic lower limit from `lower_limit_of_theta_min()`,
      unless legacy mode (`ptf` equal to "Cosby1984AndOthers")
      is used (reproducing behavior prior to v7.0.0),
      then calculated as maximum of
      `lower_limit_of_theta_min()` and `PTF_RawlsBrakensiek1985()` with
      pedotransfer function by Rawls & Brakensiek 1985 \cite rawls1985WmitE
      (independently of the selected SWRC) if there is no organic matter

@param[in] ui_sm_min User input of requested minimum soil moisture,
    see SWCMinVal
@param[in] gravel Coarse fragments (> 2 mm; e.g., gravel)
    of the whole soil [m3/m3]
@param[in] width Soil layer width [cm]
@param[in] sand Sand content of the matric soil (< 2 mm fraction) [g/g]
@param[in] clay Clay content of the matric soil (< 2 mm fraction) [g/g]
@param[in] fom Proportion by weight of organic matter to bulk soil [g g-1]
@param[in] swcBulk_sat Saturated water content of the bulk soil [cm]
@param[in] swrc_type Identification number of selected SWRC
@param[in] *swrcp Vector of SWRC parameters
@param[in] legacy_mode If true then legacy behavior (see details)
@param[in] SWCMinVal Lower bound on swc.

@return Minimum volumetric water content of the matric soil [cm / cm]
*/
static double ui_theta_min(
    double ui_sm_min,
    double gravel,
    double width,
    double sand,
    double clay,
    double fom,
    double swcBulk_sat,
    unsigned int swrc_type,
    double *swrcp,
    Bool legacy_mode,
    double SWCMinVal,
    LOG_INFO *LogInfo
) {
    double vwc_min = SW_MISSING;
    double tmp_vwcmin;

    if (LT(ui_sm_min, 0.0)) {
        /* user input: request to estimate minimum theta */

        /* realistic lower limit for theta_min */
        vwc_min =
            lower_limit_of_theta_min(swrc_type, swrcp, gravel, width, LogInfo);
        if (LogInfo->stopRun) {
            return SW_MISSING; // Exit function prematurely due to error
        }

        if (legacy_mode) {
            /* residual theta estimated with Rawls & Brakensiek (1985) PTF */
            PTF_RawlsBrakensiek1985(
                &tmp_vwcmin,
                sand,
                clay,
                fom,
                swcBulk_sat / ((1. - gravel) * width),
                LogInfo
            );
            if (LogInfo->stopRun) {
                return SW_MISSING; // Exit function prematurely due to error
            }

            /* if `PTF_RawlsBrakensiek1985()` was successful, then take max */
            if (!missing(tmp_vwcmin)) {
                vwc_min = fmax(vwc_min, tmp_vwcmin);
            }
        }

    } else if (GE(SWCMinVal, 1.0)) {
        /* user input: fixed (matric) SWP value; unit(SWCMinVal) == -bar */
        vwc_min =
            SWRC_SWPtoSWC(
                SWCMinVal, swrc_type, swrcp, gravel, width, LOGERROR, LogInfo
            ) /
            ((1. - gravel) * width);

    } else {
        /* user input: fixed matric VWC; unit(SWCMinVal) == cm/cm */
        vwc_min = SWCMinVal / width;
    }

    return vwc_min;
}

/** Extract saturated hydraulic conductivity from SWRC parameters

@param[in] swrc_type Identification number of selected SWRC
@param[in] *swrcp Vector of SWRC parameters

@return Saturated hydraulic conductivity [cm day-1]
*/
static double SWRC_get_ksat(unsigned int swrc_type, double *swrcp) {
    return swrcp[swrcp2ksat[swrc_type]];
}

/** Proportional contribution of the thickness of a soil layer to a depth limit

@param[in] depthLimit Limit of soil depth to be represented by weights
@param[in] i Soil layer index
@param[in] depths Array of soil layer (bottom) depths
@param[in] n_layers Number of soil layers
*/
static double soilLayerWeight(
    double depthLimit, LyrIndex i, double const depths[], LyrIndex n_layers
) {
    double w = 0.;

    if (i >= n_layers) {
        return w;
    }

    if (depths[i] <= depthLimit) {
        w = 1.;
    } else if (i == 0) {
        w = depthLimit / depths[i];
    } else if (depths[i] > depthLimit && depths[i - 1] < depthLimit) {
        w = (depthLimit - depths[i - 1]) / (depths[i] - depths[i - 1]);
    }

    return w;
}


#ifdef SWDEBUG
// since `soilLayerWeight` is static we cannot do unit tests unless we set
// it up as an externed function pointer
// NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables)
double (*test_soilLayerWeight)(double, LyrIndex, double const *, LyrIndex) =
    &soilLayerWeight;
// NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables)
#endif


/* =================================================== */
/*             Global Function Definitions             */
/* --------------------------------------------------- */


/**
@brief Translate a SWRC name into a SWRC type number

See #swrc2str and `siteparam.in`.
Throws an error if `SWRC` is not implemented.

@param[in] *swrc_name Name of a SWRC
@param[out] LogInfo Holds information on warnings and errors

@return Internal identification number of selected SWRC
*/
unsigned int encode_str2swrc(char *swrc_name, LOG_INFO *LogInfo) {
    unsigned int k;

    for (k = 0; k < N_SWRCs && Str_CompareI(swrc_name, (char *) swrc2str[k]);
         k++)
        ;

    if (k == N_SWRCs) {
        LogError(LogInfo, LOGERROR, "SWRC '%s' is not implemented.", swrc_name);
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

    for (k = 0; k < N_PTFs && Str_CompareI(ptf_name, (char *) ptf2str[k]); k++)
        ;

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
@param[out] *swrcpMineralSoil Vector of SWRC parameters to be estimated
    for the mineral soil component
@param[in] sand Sand content of the matric soil (< 2 mm fraction) [g/g]
@param[in] clay Clay content of the matric soil (< 2 mm fraction) [g/g]
@param[in] gravel Coarse fragments (> 2 mm; e.g., gravel)
    of the whole soil [m3/m3]
@param[in] bdensity Density of the whole soil
    (matric soil plus coarse fragments) [g/cm3];
    accepts #SW_MISSING if not used by selected PTF
@param[out] LogInfo Holds information on warnings and errors
*/
void SWRC_PTF_estimate_parameters(
    unsigned int ptf_type,
    double *swrcpMineralSoil,
    double sand,
    double clay,
    double gravel,
    double bdensity,
    LOG_INFO *LogInfo
) {

    /* Initialize swrcp[] to 0 */
    memset(swrcpMineralSoil, 0, SWRC_PARAM_NMAX * sizeof(swrcpMineralSoil[0]));

    if (ptf_type == sw_Cosby1984AndOthers || ptf_type == sw_Cosby1984) {
        SWRC_PTF_Cosby1984_for_Campbell1974(swrcpMineralSoil, sand, clay);

    } else {
        LogError(LogInfo, LOGERROR, "PTF is not implemented in SOILWAT2.");
        return; // Exit function prematurely due to error
    }

    /**********************************/
    /* TODO: remove once PTFs are implemented that utilize gravel */
    /* avoiding `error: unused parameter 'gravel' [-Werror=unused-parameter]` */
    (void) gravel;

    /* TODO: remove once PTFs are implemented that utilize bdensity */
    /* avoiding `error: unused parameter 'gravel' [-Werror=unused-parameter]` */
    (void) bdensity;
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

@param[out] *swrcpMineralSoil Vector of SWRC parameters to be estimated
    for the mineral soil component
@param[in] sand Sand content of the matric soil (< 2 mm fraction) [g/g]
@param[in] clay Clay content of the matric soil (< 2 mm fraction) [g/g]
*/
void SWRC_PTF_Cosby1984_for_Campbell1974(
    double *swrcpMineralSoil, double sand, double clay
) {
    /* Table 4 */
    /* Original equations formulated with percent sand (%) and clay (%),
      here re-formulated for fraction of sand and clay */

    /* swrcpMineralSoil[0] = psi_saturated:
        originally formulated as function of silt
        here re-formulated as function of clay */
    swrcpMineralSoil[0] = powe(10.0, -1.58 * sand - 0.63 * clay + 2.17);

    /* swrcpMineralSoil[1] = theta_saturated:
        originally with units [100 * cm / cm]
        here re-formulated with units [cm / cm] */
    swrcpMineralSoil[1] = -0.142 * sand - 0.037 * clay + 0.505;

    /* swrcpMineralSoil[2] = b */
    swrcpMineralSoil[2] = -0.3 * sand + 15.7 * clay + 3.10;

    /* swrcpMineralSoil[3] = K_saturated:
        originally with units [inches / day]
        here re-formulated with units [cm / day] */
    swrcpMineralSoil[3] =
        2.54 * 24. * powe(10.0, 1.26 * sand - 0.64 * clay - 0.60);
}

/**
@brief Saturated soil water content

See #ptf2str for implemented PTFs.
See #swrc2str for implemented SWRCs.

Saturated volumetric water content is usually estimated as one of the
SWRC parameters; this is what this function returns.

For historical reasons, if `swrc_name` is "Campbell1974", then a
`ptf_name` of "Cosby1984AndOthers" and zero organic matter will reproduce
`SOILWAT2` legacy mode (`SOILWAT2` prior to v7.0.0) and
return saturated soil water content estimated by
Saxton et al. 2006 (\cite Saxton2006) PTF instead; `ptf_name` of
"Cosby1984" will return saturated soil water content estimated by Cosby et
al. 1984 (\cite Cosby1984) PTF.

The arguments `ptf_type`, `sand`, `clay`, and `fom` are utilized only if
`ptf_name` is "Cosby1984AndOthers" (see #ptf2str).

@param[in] swrc_type Identification number of selected SWRC
@param[in] *swrcp Vector of SWRC parameters
@param[in] gravel Coarse fragments (> 2 mm; e.g., gravel)
    of the whole soil [m3/m3]
@param[in] width Soil layer width [cm]
@param[in] ptf_type Identification number of selected PTF
@param[in] sand Sand content of the matric soil (< 2 mm fraction) [g/g]
@param[in] clay Clay content of the matric soil (< 2 mm fraction) [g/g]
@param[in] fom Proportion by weight of organic matter to bulk soil [g g-1]
@param[out] LogInfo Holds information on warnings and errors

@return Estimated saturated water content of the bulk soil [cm]
*/
double SW_swcBulk_saturated(
    unsigned int swrc_type,
    const double *swrcp,
    double gravel,
    double width,
    unsigned int ptf_type,
    double sand,
    double clay,
    double fom,
    LOG_INFO *LogInfo
) {
    double theta_sat = SW_MISSING;

    switch (swrc_type) {
    case sw_Campbell1974:
        if (ptf_type == sw_Cosby1984AndOthers) {
            // Cosby1984AndOthers (backwards compatible)
            PTF_Saxton2006(&theta_sat, sand, clay, fom, LogInfo);
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
            LOGERROR,
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

The arguments `sand`, `clay`, `fom`, and `swcBulk_sat`
are utilized only in legacy mode, i.e., `ptf` equal to
"Cosby1984AndOthers".

@param[in] swrc_type Identification number of selected SWRC
@param[in] *swrcp Vector of SWRC parameters
@param[in] gravel Coarse fragments (> 2 mm; e.g., gravel)
    of the whole soil [m3/m3]
@param[in] width Soil layer width [cm]
@param[in] ptf_type Identification number of selected PTF
@param[in] ui_sm_min User input of requested minimum soil moisture,
    see #SW_SITE_INPUTS.SWCMinVal
@param[in] sand Sand content of the matric soil (< 2 mm fraction) [g/g]
@param[in] clay Clay content of the matric soil (< 2 mm fraction) [g/g]
@param[in] fom Proportion by weight of organic matter to bulk soil [g g-1]
@param[in] swcBulk_sat Saturated water content of the bulk soil [cm]
@param[in] SWCMinVal Lower bound on swc.
@param[out] LogInfo Holds information on warnings and errors

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
    double fom,
    double swcBulk_sat,
    double SWCMinVal,
    LOG_INFO *LogInfo
) {
    double theta_min_sim;
    double theta_min_theoretical = SW_MISSING;

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
            LOGERROR,
            "`SW_swcBulk_minimum()`: SWRC (type %d) is not implemented.",
            swrc_type
        );
        return SW_MISSING; // Exit function prematurely due to error
        break;
    }

    /* `theta_min` based on user input `ui_sm_min` */
    theta_min_sim = ui_theta_min(
        ui_sm_min,
        gravel,
        width,
        sand,
        clay,
        fom,
        swcBulk_sat,
        swrc_type,
        swrcp,
        // legacy mode? i.e., consider PTF_RawlsBrakensiek1985()
        (ptf_type == sw_Cosby1984AndOthers) ? swTRUE : swFALSE,
        SWCMinVal,
        LogInfo
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
        if (Str_CompareI(swrc_name, (char *) "Campbell1974") == 0 &&
            (Str_CompareI(ptf_name, (char *) "Cosby1984AndOthers") == 0 ||
             Str_CompareI(ptf_name, (char *) "Cosby1984") == 0)) {
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
@param[out] LogInfo Holds information on warnings and errors

@return A logical value indicating if parameters passed the checks.
*/
Bool SWRC_check_parameters(
    unsigned int swrc_type, double *swrcp, LOG_INFO *LogInfo
) {
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
            LOGERROR,
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
@param[out] LogInfo Holds information on warnings and errors

@return A logical value indicating if parameters passed the checks.
*/
Bool SWRC_check_parameters_for_Campbell1974(double *swrcp, LOG_INFO *LogInfo) {
    Bool res = swTRUE;

    if (!isfinite(swrcp[0]) || LE(swrcp[0], 0.0)) {
        res = swFALSE;
        LogError(
            LogInfo,
            LOGWARN,
            "SWRC_check_parameters_for_Campbell1974(): invalid value of "
            "psi(saturated, matric, [cm]) = %f (must > 0)\n",
            swrcp[0]
        );
    }

    if (!isfinite(swrcp[1]) || LE(swrcp[1], 0.0) || GT(swrcp[1], 1.0)) {
        res = swFALSE;
        LogError(
            LogInfo,
            LOGWARN,
            "SWRC_check_parameters_for_Campbell1974(): invalid value of "
            "theta(saturated, matric, [cm/cm]) = %f (must be within 0-1)\n",
            swrcp[1]
        );
    }

    if (!isfinite(swrcp[2]) || ZRO(swrcp[2])) {
        res = swFALSE;
        LogError(
            LogInfo,
            LOGWARN,
            "SWRC_check_parameters_for_Campbell1974(): invalid value of "
            "beta = %f (must be != 0)\n",
            swrcp[2]
        );
    }

    if (!isfinite(swrcp[3]) || LE(swrcp[3], 0.0)) {
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

See `SWRC_SWCtoSWP_vanGenuchten1980()` and
`SWRC_SWPtoSWC_vanGenuchten1980()` for implementation of van Genuchten's 1980
SWRC (\cite vanGenuchten1980).

"vanGenuchten1980" has five parameters (four are used for the SWRC):
    - `swrcp[0]` (`theta_r`): residual volumetric water content
      of the matric component [cm/cm]
    - `swrcp[1]` (`theta_s`): saturated volumetric water content
      of the matric component [cm/cm]
    - `swrcp[2]` (`alpha`): related to the inverse of air entry suction [cm-1]
    - `swrcp[3]` (`n`): measure of the pore-size distribution [-]
    - `swrcp[4]` (`K_sat`): saturated hydraulic conductivity `[cm / day]`

@param[in] *swrcp Vector of SWRC parameters
@param[out] LogInfo Holds information on warnings and errors

@return A logical value indicating if parameters passed the checks.
*/
Bool SWRC_check_parameters_for_vanGenuchten1980(
    double *swrcp, LOG_INFO *LogInfo
) {
    Bool res = swTRUE;

    if (!isfinite(swrcp[0]) || LE(swrcp[0], 0.0) || GT(swrcp[0], 1.)) {
        res = swFALSE;
        LogError(
            LogInfo,
            LOGWARN,
            "SWRC_check_parameters_for_vanGenuchten1980(): invalid value of "
            "theta(residual, matric, [cm/cm]) = %f (must be within 0-1)\n",
            swrcp[0]
        );
    }

    if (!isfinite(swrcp[1]) || LE(swrcp[1], 0.0) || GT(swrcp[1], 1.)) {
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
            swrcp[0],
            swrcp[1]
        );
    }

    if (!isfinite(swrcp[2]) || LE(swrcp[2], 0.0)) {
        res = swFALSE;
        LogError(
            LogInfo,
            LOGWARN,
            "SWRC_check_parameters_for_vanGenuchten1980(): invalid value of "
            "alpha([1 / cm]) = %f (must > 0)\n",
            swrcp[2]
        );
    }

    if (!isfinite(swrcp[3]) || LE(swrcp[3], 1.0)) {
        res = swFALSE;
        LogError(
            LogInfo,
            LOGWARN,
            "SWRC_check_parameters_for_vanGenuchten1980(): invalid value of "
            "n = %f (must be > 1)\n",
            swrcp[3]
        );
    }

    if (!isfinite(swrcp[4]) || LE(swrcp[4], 0.0)) {
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
@param[out] LogInfo Holds information on warnings and errors

@return A logical value indicating if parameters passed the checks.
*/
Bool SWRC_check_parameters_for_FXW(double *swrcp, LOG_INFO *LogInfo) {
    Bool res = swTRUE;

    if (!isfinite(swrcp[0]) || LE(swrcp[0], 0.0) || GT(swrcp[0], 1.)) {
        res = swFALSE;
        LogError(
            LogInfo,
            LOGWARN,
            "SWRC_check_parameters_for_FXW(): invalid value of "
            "theta(saturated, matric, [cm/cm]) = %f (must be within 0-1)\n",
            swrcp[0]
        );
    }

    if (!isfinite(swrcp[1]) || LE(swrcp[1], 0.0)) {
        res = swFALSE;
        LogError(
            LogInfo,
            LOGWARN,
            "SWRC_check_parameters_for_FXW(): invalid value of "
            "alpha([1 / cm]) = %f (must be > 0)\n",
            swrcp[1]
        );
    }

    if (!isfinite(swrcp[2]) || LE(swrcp[2], 1.0) || GT(swrcp[2], 10.)) {
        res = swFALSE;
        LogError(
            LogInfo,
            LOGWARN,
            "SWRC_check_parameters_for_FXW(): invalid value of "
            "n = %f (must be within 1-10)\n",
            swrcp[2]
        );
    }

    if (!isfinite(swrcp[3]) || LE(swrcp[3], 0.0) || GT(swrcp[3], 1.5)) {
        res = swFALSE;
        LogError(
            LogInfo,
            LOGWARN,
            "SWRC_check_parameters_for_FXW(): invalid value of "
            "m = %f (must be within 0-1.5)\n",
            swrcp[3]
        );
    }

    if (!isfinite(swrcp[4]) || LE(swrcp[4], 0.0)) {
        res = swFALSE;
        LogError(
            LogInfo,
            LOGWARN,
            "SWRC_check_parameters_for_FXW(): invalid value of "
            "K_sat = %f (must be > 0)\n",
            swrcp[4]
        );
    }

    if (!isfinite(swrcp[5]) || LE(swrcp[5], 0.)) {
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

@param[out] *theta_sat Estimated saturated volumetric water content
    of the matric soil [cm/cm]
@param[in] sand Sand content of the matric soil (< 2 mm fraction) [g/g]
@param[in] clay Clay content of the matric soil (< 2 mm fraction) [g/g]
@param[in] fom Proportion by weight of organic matter to bulk soil [g g-1]
@param[out] LogInfo Holds information on warnings and errors
*/
void PTF_Saxton2006(
    double *theta_sat, double sand, double clay, double fom, LOG_INFO *LogInfo
) {
    double theta_33;
    double theta_33t;
    double theta_S33;
    double theta_S33t;
    double om = 100. * fom; /* equations use percent and not proportion OM */

    if (fom > 0.08) {
        LogError(
            LogInfo,
            LOGERROR,
            "PTF_Saxton2006(): invalid value of "
            "organic matter content = %f (must be within 0-0.08)\n",
            fom
        );
        return; // Exit function prematurely due to error
    }

    /* Eq. 2: 33 kPa moisture */
    theta_33t = +0.299 - 0.251 * sand + 0.195 * clay + 0.011 * fom +
                0.006 * sand * om - 0.027 * clay * om + 0.452 * sand * clay;

    theta_33 =
        theta_33t + (1.283 * squared(theta_33t) - 0.374 * theta_33t - 0.015);

    /* Eq. 3: SAT-33 kPa moisture */
    theta_S33t = +0.078 + 0.278 * sand + 0.034 * clay + 0.022 * om -
                 0.018 * sand * om - 0.027 * clay * om - 0.584 * sand * clay;

    theta_S33 = theta_S33t + (0.636 * theta_S33t - 0.107);


    /* Eq. 5: saturated moisture */
    *theta_sat = theta_33 + theta_S33 - 0.097 * sand + 0.043;

    if (LE(*theta_sat, 0.) || GT(*theta_sat, 1.)) {
        LogError(
            LogInfo,
            LOGERROR,
            "PTF_Saxton2006(): invalid value of "
            "theta(saturated, [cm / cm]) = %f (must be within 0-1)\n",
            *theta_sat
        );
        return; // Exit function prematurely due to error
    }


// currently, unused and defunct code:
// (e.g., SW_Site.lyr[n] assignments don't work here anymore!):
#ifdef UNUSED_SAXTON2006
    double R_w, alpha, theta_1500, theta_1500t;

    /* Eq. 1: 1500 kPa moisture */
    theta_1500t = +0.031 - 0.024 * sand + 0.487 * clay + 0.006 * fom +
                  0.005 * sand * om - 0.013 * clay * om + 0.068 * sand * clay;

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
        24. / 10. * 1930. *
        pow(theta_S - theta_33, 3. - SW_Site.lyr[n]->Saxton2006_lambda);


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
@param[in] fom Proportion by weight of organic matter to bulk soil [g g-1]
@param[in] porosity Pore space of the matric soil (< 2 mm fraction) [cm3/cm3]
@param[out] *theta_min Estimated residual volumetric water content
    of the matric soil [cm/cm]
@param[out] LogInfo Holds information on warnings and errors
*/
void PTF_RawlsBrakensiek1985(
    double *theta_min,
    double sand,
    double clay,
    double fom,
    double porosity,
    LOG_INFO *LogInfo
) {
    if (GE(clay, 0.05) && LE(clay, 0.6) && GE(sand, 0.05) && LE(sand, 0.7) &&
        GE(porosity, 0.1) && LT(porosity, 1.) && ZRO(fom)) {
        sand *= 100.;
        clay *= 100.;
        /* Note: the equation requires sand and clay in units of [100 * g / g];
                porosity, however, must be in units of [cm3 / cm3]
        */
        *theta_min = fmax(
            0.,
            -0.0182482 + 0.00087269 * sand + 0.00513488 * clay +
                0.02939286 * porosity - 0.00015395 * squared(clay) -
                0.0010827 * sand * porosity -
                0.00018233 * squared(clay) * squared(porosity) +
                0.00030703 * squared(clay) * porosity -
                0.0023584 * squared(porosity) * clay
        );

    } else {
        *theta_min = SW_MISSING;

        if (!ZRO(fom)) {
            LogError(
                LogInfo,
                LOGERROR,
                "PTF_RawlsBrakensiek1985(): fom = %f (must be 0)",
                fom
            );
        } else {
            LogError(
                LogInfo,
                LOGWARN,
                "PTF_RawlsBrakensiek1985(): "
                "sand = %f (must be in 0.05-0.7), "
                "clay = %f (must be in 0.05-0.6), and/or "
                "porosity = %f (must be in 0.1-0.1)"
                "out of valid range",
                sand,
                clay,
                porosity
            );
        }
    }
}

/** Interpolate organic matter parameter between fibric and sapric
characteristics

Parameters of the organic component are interpolate between
fibric peat characteristics assumed at surface and sapric peat at depth.

@param[in] pomFibric Parameter of fibric peat (surface conditions)
@param[in] pomSapric Parameter of sapric peat (at depth `depthSapric`)
@param[in] depthSapric Soil depth [`cm`] at which soil properties reach
    values of sapric peat
@param[in] depthT Depth at top of soil layer [`cm`]
@param[in] depthB Depth at bottom of soil layer [`cm`]

@return Parameter value of the organic component
*/
static double interpolateFibricSapric(
    double pomFibric,
    double pomSapric,
    double depthSapric,
    double depthT,
    double depthB
) {
    double res;
    double b;

    if (ZRO(depthSapric)) {
        res = pomSapric;

    } else {

        b = (pomSapric - pomFibric) / depthSapric;

        if (pomFibric > pomSapric) {
            res = 0.5 * (fmax(pomSapric, pomFibric + b * depthT) +
                         fmax(pomSapric, pomFibric + b * depthB));

        } else {
            res = 0.5 * (fmin(pomSapric, pomFibric + b * depthT) +
                         fmin(pomSapric, pomFibric + b * depthB));
        }
    }

    return res;
}

/** Calculate bulk soil SWRC parameters

Parameters of the bulk soil are calculated as the weighted average of
parameters of the organic and mineral components.

Parameters of the organic component are interpolate between
fibric peat characteristics assumed at surface and sapric peat at depth.

Bulk soil saturated hydraulic conductivity accounts for connected pathways
that only consist of organic matter above a threshold value of organic matter.

@param[in] swrc_type Identification number of selected SWRC
@param[out] *swrcp Vector of SWRC parameters of the bulk soil
@param[in] *swrcpMS Vector of SWRC parameters of the mineral component
@param[in] swrcpOM Array of length two of vectors of SWRC parameters
    of fibric peat (surface conditions) and
    of sapric peat (at depth `depthSapric`)
@param[in] fom Proportion by weight of organic matter to bulk soil
@param[in] depthSapric Soil depth [`cm`] at which soil properties reach
    values of sapric peat
@param[in] depthT Depth at top of soil layer [`cm`]
@param[in] depthB Depth at bottom of soil layer [`cm`]
*/
void SWRC_bulkSoilParameters(
    unsigned int swrc_type,
    double *swrcp,
    const double *swrcpMS,
    double swrcpOM[][SWRC_PARAM_NMAX],
    double fom,
    double depthSapric,
    double depthT,
    double depthB
) {
    unsigned int k;
    unsigned int iksat = swrcp2ksat[swrc_type];
    double pOM;
    double fperc;
    double unconnectedKsat;

    static const double percBeta = 0.139; /* percolation exponent */
    static const double fthreshold = 0.5; /* percolation threshold */

    if (fom > 0.) {
        /* Has organic matter:
           interpolate between organic and mineral components */

        for (k = 0; k < SWRC_PARAM_NMAX; k++) {
            /* Interpolate organic parameter from surface to depth conditions */
            pOM = interpolateFibricSapric(
                swrcpOM[0][k], swrcpOM[1][k], depthSapric, depthT, depthB
            );

            if (k == iksat) {
                /* ksat: account for effects of connected flow pathways */

                if (fom >= fthreshold) {
                    /* (1 - fperc) is the sum of the fraction of mineral soil
                       and the fraction of non-percolating organic component */
                    fperc = fom * pow(1. - fthreshold, -percBeta) *
                            pow(fom - fthreshold, percBeta);
                } else {
                    fperc = 0.;
                }

                /* non-connected fraction assumes conductivities
                   through mineral and organic components in series */
                unconnectedKsat =
                    (fom < 1.) ?
                        1. / ((1. - fom) / swrcpMS[k] + (fom - fperc) / pOM) :
                        0.;

                swrcp[k] = fperc * pOM + (1. - fperc) * unconnectedKsat;

            } else {
                /* other swrcp: weighted average of organic and mineral param */
                swrcp[k] = fom * pOM + (1. - fom) * swrcpMS[k];
            }
        }

    } else {
        /* No organic matter: bulk soil corresponds to mineral components */
        for (k = 0; k < SWRC_PARAM_NMAX; k++) {
            swrcp[k] = swrcpMS[k];
        }
    }
}

/**
@brief Estimate potential evaporation coefficients from soil texture

This function is based on R code from `rSW2data::calc_BareSoilEvapCoefs()`
as of 2025-Dec-03.
It represents the soil texture influence based on a small re-analysis of data
from Wythers et al. 1999 @cite wythers1999SSSAJ and
uses a maximum depth of 15 cm based on Torres et al. 2010 @cite torres2010HSJSH.

Soil restrictions (impermeability) reduce proportionally the increments of
the coefficients at and below the depth of the restriction.

@param[out] evco Estimated potential evaporation coefficients [0-1]
@param[in] depth Depth at bottom of soil layer [`cm`]
@param[in] width Width (thickness) of soil layer [`cm`]
@param[in] sand Sand content of the matric soil (< 2 mm fraction) [g/g]
@param[in] clay Clay content of the matric soil (< 2 mm fraction) [g/g]
@param[in] impermeability Relative restriction (impermeability) by layer [0-1].
@param[in] n_layers Number of soil layers

*/
void estimate_evco(
    double evco[],
    const double depth[],
    const double width[],
    const double sand[],
    const double clay[],
    const double impermeability[],
    LyrIndex n_layers
) {
    const double maxDepth = 15.;
    LyrIndex s;
    LyrIndex sMax;
    double wmSand = 0.;
    double wmClay = 0.;
    double depthEs;
    double cec[MAX_LAYERS] = {0.};
    double sec = 0.;
    double tmpi;
    double nDigitsRound = 1e4;

    memset(evco, 0, sizeof evco[0] * n_layers);

    // Find count of soil layers that occur within maxDepth
    for (sMax = 0; sMax < n_layers && LT(depth[sMax], maxDepth); sMax++) {
    }

    // Only most shallow soil layer within maxDepth
    if (sMax == 0) {
        evco[0] = 1.;
        return;
    }

    // Depth-weighted mean sand and clay content
    for (s = 0; s <= sMax; s++) {
        wmSand += width[s] * sand[s];
        wmClay += width[s] * clay[s];
    }

    wmSand /= depth[sMax];
    wmClay /= depth[sMax];

    // Equation from re-analysis to estimate depth
    depthEs = fmin(
        maxDepth, 4.1984 + 0.6695 * squared(wmSand) + 168.7603 * squared(wmClay)
    );

    // Equation with consistency to "previous" cumulative evco
    for (s = 0; s <= sMax; s++) {
        cec[s] = 1. - exp(-5. * depth[s] / depthEs);
    }
    cec[sMax] = 1.;

    // Finalize potential evcos
    tmpi = impermeability[0];
    evco[0] = round(nDigitsRound * cec[0] * (1. - tmpi)) / nDigitsRound;
    sec += evco[0];
    for (s = 1; s <= sMax; s++) {
        tmpi = fmax(tmpi, impermeability[s]);
        evco[s] = round(nDigitsRound * (cec[s] - cec[s - 1]) * (1. - tmpi)) /
                  nDigitsRound;
        sec += evco[s];
    }

    for (s = 0; s <= sMax; s++) {
        evco[s] /= sec;
    }
}

/**
@brief Estimate soil density of the whole soil (bulk).

Based on equation 20 from Saxton. @cite Saxton2006.

Similarly, estimate soil bulk density from `theta_sat` with
`2.65 * (1. - theta_sat * (1. - fractionGravel))`.
*/
double calculate_soilBulkDensity(double matricDensity, double fractionGravel) {
    return matricDensity * (1. - fractionGravel) + fractionGravel * 2.65;
}

/**
@brief Estimate soil density of the matric soil component.

Based on equation 20 from Saxton. @cite Saxton2006
*/
double calculate_soilMatricDensity(
    double bulkDensity, double fractionGravel, LOG_INFO *LogInfo
) {
    double res;

    if (EQ(fractionGravel, 1.)) {
        res = 0.;
    } else {
        res = (bulkDensity - fractionGravel * 2.65) / (1. - fractionGravel);

        if (LT(res, 0.)) {
            LogError(
                LogInfo,
                LOGERROR,
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

@param[in] evap_coeff Struct list of type SW_LAYER_INFO holding information
    about every soil layer in the simulation
@param[in] n_layers Number of layers of soil within the simulation run
*/
LyrIndex nlayers_bsevap(double *evap_coeff, LyrIndex n_layers) {
    LyrIndex s;
    LyrIndex n = 0;

    ForEachSoilLayer(s, n_layers) {
        if (GT(evap_coeff[s], 0.0)) {
            n++;
        } else {
            break;
        }
    }

    return n;
}

/**
@brief Estimate rooting profile with equation and vegetation type parameters

The root fraction for every vegetation type is accumulated across the
thickness (width) of each soil layer.
Soil restrictions (impermeability) reduce proportionally the increments of
the rooting profile at and below the depth of the restriction.
Root fractions for each vegetation type are relative to the soil profile, i.e.,
they sum to one across the soil depth. They are adjusted proportionally if
a shallow soil depth or soil restrictions prevent roots from achieving their
maximum depth.

An equation by Zeng 2001 @cite zeng2001JH represents the cumulative
rooting profile

\f[Y = 1 - \frac{1}{2} * (exp ^ {(-\alpha * d)} + exp ^ {(-\beta * d)})\f]

where Y is the cumulative root fraction from the surface to soil depth d,
\f$d_{max}\f$ is the depth of the rooting zone, and \f$\alpha\f$ and \f$\beta\f$
are two shape parameters.

@param[out] trco Estimated potential transpiration coefficients [0-1]
@param[in] depth Depth at bottom of soil layer [`cm`]
@param[in] impermeability Relative restriction (impermeability) by layer [0-1].
@param[in] veg Array of structure VegTypeIn with vegetation inputs including
    root profile parameters \f$\alpha\f$, \f$\beta\f$, and \f$d_{max}\f$.
@param[in] n_layers Number of soil layers
*/
void estimate_trco(
    double trco[][MAX_LAYERS],
    const double depth[],
    const double impermeability[],
    const VegTypeIn veg[],
    LyrIndex n_layers
) {
    LyrIndex s;
    unsigned int k;
    double tmp1;
    double tmp2;
    double p1;
    double p2;
    double p3;
    double d;
    double sumTrCo;
    double sumTrCoR;
    double tmpTrCo[MAX_LAYERS];
    double tmpi;
    double tmpd;
    double nDigitsRound = 1e4;

    memset(trco, 0, sizeof trco[0] * NVEGTYPES);

    ForEachVegType(k) {
        memset(tmpTrCo, 0, sizeof tmpTrCo);
        sumTrCo = 0.;
        tmp1 = 0.;
        tmpi = 0.;
        p1 = veg->rootProfileParam[k][0]; /* shape parameter */
        p2 = veg->rootProfileParam[k][1]; /* shape parameter */
        p3 = veg->rootProfileParam[k][2]; /* maximum rooting depth */

        ForEachSoilLayer(s, n_layers) {
            tmpd = 1e-2 * depth[s]; /* convert depth from [cm] to [m] */
            d = fmin(p3, tmpd);
            tmp2 = 1. - 0.5 * (exp(-p1 * d) + exp(-p2 * d));
            tmpi = fmax(tmpi, impermeability[s]);
            tmpTrCo[s] = fmax(0., (tmp2 - tmp1) * (1. - tmpi));
            sumTrCo += tmpTrCo[s];
            if (p3 < tmpd || EQ(tmpi, 1.)) {
                break;
            }
            tmp1 = tmp2;
        }

        /* Proportional adjustment if restricted soil depth or impermeability
            prevented roots from achieving maximum depth, i.e., sum(trco) = 1 */
        if (GT(sumTrCo, 0.)) {
            sumTrCoR = 0.;
            ForEachSoilLayer(s, n_layers) {
                trco[k][s] =
                    round(nDigitsRound * tmpTrCo[s] / sumTrCo) / nDigitsRound;
                sumTrCoR += trco[k][s];
            }
            if (!EQ(sumTrCoR, 1.)) {
                trco[k][0] += 1. - sumTrCoR;
            }
        }
    }
}

/**
@brief Count soil layers with roots potentially extracting water for
    transpiration

Since v8.3.0, layers with no roots are not counted towards n_transp_lyrs, e.g.,
the absence of roots in the shallowest soil layer no longer forces
n_transp_lyrs to be zero.
Previously, the count stopped at the first layer without roots.

@param[in] n_layers Number of layers of soil within the simulation run
@param[in] transp_coeff Prop. of total transp from this layer
@param[out] n_transp_lyrs Number of soil layers with roots
    per plant functional type
*/
void nlayers_vegroots(
    LyrIndex n_layers,
    LyrIndex n_transp_lyrs[],
    double transp_coeff[][MAX_LAYERS]
) {
    LyrIndex s;
    int k;

    ForEachVegType(k) {
        n_transp_lyrs[k] = 0;

        ForEachSoilLayer(s, n_layers) {
            if (GT(transp_coeff[k][s], 0.0)) {
                n_transp_lyrs[k]++;
            } else {
                continue;
            }
        }
    }
}

/**
@brief Set `deep_lyr` to indicate that deep drainage is being simulated

@param[in,out] SW_SiteSim Struct of type SW_SITE_SIM describing the simulated
site's simulation values
@param[in] n_layers Number of layers of soil within the simulation run
@param[in] deepdrain A flag specifying if we allow drainage into deepest layer
*/
void add_deepdrain_layer(
    SW_SITE_SIM *SW_SiteSim, LyrIndex n_layers, Bool deepdrain
) {

    if (deepdrain) {
        /* total percolation from last layer == deep drainage */
        SW_SiteSim->deep_lyr = n_layers - 1; /* deep_lyr is base0 */
    } else {
        SW_SiteSim->deep_lyr = 0;
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


void SW_SOIL_construct(SW_SOIL_RUN_INPUTS *SW_Soils) {
    memset(SW_Soils, 0, sizeof(SW_SOIL_RUN_INPUTS));
}

/**
@brief Initialized memory for SW_SiteIn/SW_SiteSim

/note that an initializer that is called during execution (better called clean()
or something) will need to free all allocated memory first before clearing
structure.
*/
void SW_SIT_construct(
    SW_SITE_INPUTS *SW_SiteIn, SW_SITE_SIM *SW_SiteSim, LyrIndex *n_layers
) {
    /* =================================================== */

    memset(SW_SiteIn, 0, sizeof(SW_SITE_INPUTS));
    memset(SW_SiteSim, 0, sizeof(SW_SITE_SIM));
    SW_SIT_init_counts(SW_SiteSim, n_layers);
}

/**
@brief Reads in file for input values.

@param[in,out] SW_SiteIn Struct of type SW_SITE_INPUTS
@param[in,out] SW_SiteRunIn Struct of type SW_SITE_RUN_INPUTS
@param[in] txtInFiles Array of program in/output files
@param[out] SW_CarbonIn Struct of type SW_CARBON_INPUTS holding all CO2-related
    data
@param[out] hasConsistentSoilLayerDepths  Holds the specification if the
    input soil layers have the same depth throughout all inputs (only used
    when dealing with nc inputs)
@param[out] LogInfo Holds information on warnings and errors
*/
void SW_SIT_read(
    SW_SITE_INPUTS *SW_SiteIn,
    SW_SITE_RUN_INPUTS *SW_SiteRunIn,
    char *txtInFiles[],
    SW_CARBON_INPUTS *SW_CarbonIn,
    Bool *hasConsistentSoilLayerDepths,
    LOG_INFO *LogInfo
) {
    /* =================================================== */
    /* 5-Feb-2002 (cwb) Removed rgntop requirement in
     *    transpiration regions section of input
     */
#ifdef SWDEBUG
    int debug = 0;
#endif

    FILE *f;
    const int nLinesWithoutTR = 52; /* Number of inputs without tr regions */
    int lineno = 0;
    int x;
    double rgnlow = 0; /* lower depth of region */
    int region = 0;    /* transp region definition number */
    Bool too_many_regions = swFALSE;
    char inbuf[MAX_FILENAMESIZE];
    int intRes;
    int resSNP;
    double doubleRes;
    char rgnStr[2][10] = {{'\0'}};

    Bool doDoubleConv;
    Bool strLine;

    /* note that Files.read() must be called prior to this. */
    char *MyFileName = txtInFiles[eSite];

    f = OpenFile(MyFileName, "r", LogInfo);
    if (LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

    while (GetALine(f, inbuf, MAX_FILENAMESIZE)) {
        doubleRes = SW_MISSING;
        intRes = SW_MISSING;

        /* Identify lines with inputs as a string, double or integer
            lineno with strings: 44, 50, 51
            lineno with doubles: 0-2, 5-8, 10-38, 49
            lineno with ints: 3, 4, 9, 39-43, 45-48, 52
        */

        strLine = (Bool) (lineno == 44 || lineno == 50 || lineno == 51);

        if (!strLine && lineno <= nLinesWithoutTR) {
            doDoubleConv =
                (Bool) ((lineno >= 0 && lineno <= 2) ||
                        (lineno >= 5 && lineno <= 8) ||
                        (lineno >= 10 && lineno <= 38) || lineno == 49);

            if (doDoubleConv) {
                doubleRes = sw_strtod(inbuf, MyFileName, LogInfo);
            } else {
                intRes = sw_strtoi(inbuf, MyFileName, LogInfo);
            }

            if (LogInfo->stopRun) {
                goto closeFile;
            }
        }

        switch (lineno) {
        /* Soil water content initialization, minimum, and wet condition */
        case 0:
            SW_SiteIn->SWCMinVal = doubleRes;
            break;
        case 1:
            SW_SiteIn->SWCInitVal = doubleRes;
            break;
        case 2:
            SW_SiteIn->SWCWetVal = doubleRes;
            break;

        /* Diffuse recharge and runoff/runon */
        case 3:
            SW_SiteIn->reset_yr = itob(intRes);
            break;
        case 4:
            SW_SiteIn->deepdrain = itob(intRes);
            break;
        case 5:
            SW_SiteIn->percentRunoff = doubleRes;
            break;
        case 6:
            SW_SiteIn->percentRunon = doubleRes;
            break;

        /* Energy, albedo and atmospheric demand */
        case 7:
            SW_SiteIn->pet_scale = doubleRes;
            break;
        case 8:
            SW_SiteIn->z_0g = doubleRes;
            break;
        case 9:
            SW_SiteIn->methodAlbedo = (unsigned int) intRes;
            break;
        case 10:
            SW_SiteIn->alpha_snow_max = doubleRes;
            break;
        case 11:
            SW_SiteRunIn->alpha_soil_dry = doubleRes;
            break;
        case 12:
            SW_SiteRunIn->alpha_soil_sat = doubleRes;
            break;
        case 13:
            SW_SiteRunIn->paramSoilAlbedoDarkening = doubleRes;
            break;

        /* Snow processes */
        case 14:
            SW_SiteIn->TminAccu2 = doubleRes;
            break;
        case 15:
            SW_SiteIn->TmaxCrit = doubleRes;
            break;
        case 16:
            SW_SiteIn->lambdasnow = doubleRes;
            break;
        case 17:
            SW_SiteIn->RmeltMin = doubleRes;
            break;
        case 18:
            SW_SiteIn->RmeltMax = doubleRes;
            break;
        case 19:
            SW_SiteIn->snowFractionalCoverMeltingFactor = doubleRes;
            break;

        /* Hyrdaulic conductivity */
        case 20:
            SW_SiteIn->slow_drain_coeff = doubleRes;
            break;

        /* Evaporation parameters */
        case 21:
            SW_SiteIn->evap.xinflec = doubleRes;
            break;
        case 22:
            SW_SiteIn->evap.slope = doubleRes;
            break;
        case 23:
            SW_SiteIn->evap.yinflec = doubleRes;
            break;
        case 24:
            SW_SiteIn->evap.range = doubleRes;
            break;

        /* Transpiration parameters */
        case 25:
            SW_SiteIn->transp.xinflec = doubleRes;
            break;
        case 26:
            SW_SiteIn->transp.slope = doubleRes;
            break;
        case 27:
            SW_SiteIn->transp.yinflec = doubleRes;
            break;
        case 28:
            SW_SiteIn->transp.range = doubleRes;
            break;

        /* Surface and soil temperature */
        case 29:
            SW_SiteIn->bmLimiter = doubleRes;
            break;
        case 30:
            SW_SiteIn->t1Param1 = doubleRes;
            break;
        case 31:
            SW_SiteIn->t1Param2 = doubleRes;
            break;
        case 32:
            SW_SiteIn->t1Param3 = doubleRes;
            break;
        case 33:
            SW_SiteIn->csParam1 = doubleRes;
            break;
        case 34:
            SW_SiteIn->csParam2 = doubleRes;
            break;
        case 35:
            SW_SiteIn->shParam = doubleRes;
            break;
        case 36:
            SW_SiteRunIn->Tsoil_constant = doubleRes;
            break;
        case 37:
            SW_SiteIn->stDeltaX = doubleRes;
            break;
        case 38:
            SW_SiteIn->stMaxDepth = doubleRes;
            break;
        case 39:
            SW_SiteIn->use_soil_temp = itob(intRes);
            break;
        case 40:
            SW_SiteIn->methodSurfaceTemperature = (unsigned int) intRes;
            break;
        case 41:
            SW_SiteIn->methodMaxDepthSoilTemperature = intRes;
            break;

        /* CO2 settings */
        case 42:
            SW_CarbonIn->use_bio_mult = itob(intRes);
            break;
        case 43:
            SW_CarbonIn->use_wue_mult = itob(intRes);
            break;
        case 44:
            resSNP = snprintf(
                SW_CarbonIn->scenario, sizeof SW_CarbonIn->scenario, "%s", inbuf
            );
            if (resSNP < 0 ||
                (unsigned) resSNP >= (sizeof SW_CarbonIn->scenario)) {
                LogError(
                    LogInfo,
                    LOGERROR,
                    "Atmospheric [CO2] scenario name is too long: '%s'.",
                    inbuf
                );
                goto closeFile;
            }
#ifdef SWDEBUG
            if (debug) {
                sw_printf(
                    "'SW_SIT_read': scenario = %s\n", SW_CarbonIn->scenario
                );
            }
#endif
            break;

        /* Soil characterization */
        case 45:
            *hasConsistentSoilLayerDepths = itob(intRes);
            break;

        case 46:
            SW_SiteIn->type_soilDensityInput = (unsigned int) intRes;
            break;

        case 47:
            SW_SiteIn->methodEvCo = (unsigned int) intRes;
            break;

        case 48:
            SW_SiteIn->methodTrCo = (unsigned int) intRes;
            break;

        case 49:
            SW_SiteIn->depthSapric = doubleRes;
            break;

        /* Soil water retention curve */
        case 50:
            resSNP = snprintf(
                SW_SiteIn->site_swrc_name,
                sizeof SW_SiteIn->site_swrc_name,
                "%s",
                inbuf
            );
            if (resSNP < 0 ||
                (unsigned) resSNP >= (sizeof SW_SiteIn->site_swrc_name)) {
                LogError(
                    LogInfo, LOGERROR, "SWRC name is too long: '%s'.", inbuf
                );
                goto closeFile;
            }
            SW_SiteIn->site_swrc_type =
                encode_str2swrc(SW_SiteIn->site_swrc_name, LogInfo);
            if (LogInfo->stopRun) {
                goto closeFile;
            }
            break;
        case 51:
            resSNP = snprintf(
                SW_SiteIn->site_ptf_name,
                sizeof SW_SiteIn->site_ptf_name,
                "%s",
                inbuf
            );
            if (resSNP < 0 ||
                (unsigned) resSNP >= (sizeof SW_SiteIn->site_ptf_name)) {
                LogError(
                    LogInfo, LOGERROR, "PTF name is too long: '%s'.", inbuf
                );
                goto closeFile;
            }
            SW_SiteIn->site_ptf_type = encode_str2ptf(SW_SiteIn->site_ptf_name);
            break;
        case 52:
            if (lineno != nLinesWithoutTR) {
                LogError(
                    LogInfo,
                    LOGERROR,
                    "Programmer: code and '%s' appear misaligned.",
                    MyFileName
                );
                goto closeFile;
            }
            SW_SiteIn->inputsProvideSWRCp = itob(intRes);
            break;

        default:
            x = sscanf(inbuf, "%9s %9s", rgnStr[0], rgnStr[1]);

            if (x == 2) {
                region = sw_strtoi(rgnStr[0], MyFileName, LogInfo);
                if (LogInfo->stopRun) {
                    goto closeFile;
                }

                rgnlow = sw_strtod(rgnStr[1], MyFileName, LogInfo);
                if (LogInfo->stopRun) {
                    goto closeFile;
                }
            }

            if (x < 2 || region < 1 || rgnlow < 0) {
                LogError(
                    LogInfo,
                    LOGERROR,
                    "%s: Bad input for transpiration regions on line %d.",
                    MyFileName,
                    lineno
                );
                goto closeFile;
            }

            if (region > MAX_TRANSP_REGIONS ||
                SW_SiteIn->n_transp_rgn >= MAX_TRANSP_REGIONS) {
                too_many_regions = swTRUE;
                goto Label_End_Read;
            }

            SW_SiteIn->TranspRgnDepths[region - 1] = rgnlow;
            SW_SiteIn->n_transp_rgn++;
        }

        lineno++;
    }

Label_End_Read:
    if (too_many_regions) {
        LogError(
            LogInfo,
            LOGERROR,
            "Maximum number of transpiration regions exceeded (max = %d)",
            MAX_TRANSP_REGIONS
        );
        goto closeFile;
    }

    checkSiteParameters(SW_SiteIn, LogInfo);
    checkSiteRunParameters(SW_SiteRunIn, LogInfo);

closeFile: { CloseFile(&f, LogInfo); }
}

void checkSiteParameters(SW_SITE_INPUTS *SW_SiteIn, LOG_INFO *LogInfo) {

    /* Checks: Soil water content initialization, minimum, and wet condition */
    /* No checks: different magnitudes and signs have been used historically
    to identify different units and methods for estimating these parameters */


    /* Checks: Diffuse recharge and runoff/runon */
    if (LT(SW_SiteIn->percentRunoff, 0.) || GT(SW_SiteIn->percentRunoff, 1.)) {
        LogError(
            LogInfo,
            LOGERROR,
            "Proportion of ponded surface water removed as daily"
            "runoff = %f (value ranges between 0 and 1)",
            SW_SiteIn->percentRunoff
        );
        return;
    }

    if (LT(SW_SiteIn->percentRunon, 0.)) {
        LogError(
            LogInfo,
            LOGERROR,
            "Proportion of water that arrives at surface added "
            "as daily runon = %f (value ranges between 0 and +inf)",
            SW_SiteIn->percentRunon
        );
        return;
    }


    /* Checks: Energy, albedo and atmospheric demand */
    if (LT(SW_SiteIn->pet_scale, 0.)) {
        LogError(
            LogInfo,
            LOGERROR,
            "Scaling factor for potential evapotranspiration = %f (value >= 0)",
            SW_SiteIn->pet_scale
        );
        return;
    }

    if (LE(SW_SiteIn->z_0g, 0.)) {
        LogError(
            LogInfo,
            LOGERROR,
            "Ground roughness length (z_0g) = %f (value > 0)",
            SW_SiteIn->z_0g
        );
        return;
    }

    if (!(SW_SiteIn->methodAlbedo == albedoFixed ||
          SW_SiteIn->methodAlbedo == albedoDynamic1)) {
        LogError(
            LogInfo,
            LOGERROR,
            "Method for calculating albedo (methodAlbedo) = %d "
            "(implemented methods are 0: constant, 1: composite)",
            SW_SiteIn->methodAlbedo
        );
        return;
    }

    if (LT(SW_SiteIn->alpha_snow_max, 0.) ||
        GT(SW_SiteIn->alpha_snow_max, 1.)) {
        LogError(
            LogInfo,
            LOGERROR,
            "Maximum snow albedo (alpha_snow_max) = %f "
            "(value ranges between 0 and 1)",
            SW_SiteIn->alpha_snow_max
        );
        return;
    }

    /* Checks: Snow simulation */
    if (LT(SW_SiteIn->lambdasnow, 0.) || GT(SW_SiteIn->lambdasnow, 1.)) {
        LogError(
            LogInfo,
            LOGERROR,
            "Mixing parameter of snow temperature and air temperature "
            "(lambdasnow) = %f (value ranges between 0 and 1)",
            SW_SiteIn->lambdasnow
        );
        return;
    }

    if (LT(SW_SiteIn->RmeltMin, 0.) ||
        GT(SW_SiteIn->RmeltMin, SW_SiteIn->RmeltMax)) {
        LogError(
            LogInfo,
            LOGERROR,
            "Minimum melt rate (RmeltMin) = %f "
            "(value ranges in between 0 and RmeltMax = %f)",
            SW_SiteIn->RmeltMin,
            SW_SiteIn->RmeltMax
        );
        return;
    }

    if (LT(SW_SiteIn->RmeltMax, 0.) || GT(SW_SiteIn->RmeltMax, 1.)) {
        LogError(
            LogInfo,
            LOGERROR,
            "Maximum melt rate (RmeltMax) = %f (value ranges between 0 and 1)",
            SW_SiteIn->RmeltMax
        );
        return;
    }

    if (LE(SW_SiteIn->snowFractionalCoverMeltingFactor, 0.)) {
        LogError(
            LogInfo,
            LOGERROR,
            "Snow fractional cover melting factor = %f (value > 0)",
            SW_SiteIn->snowFractionalCoverMeltingFactor
        );
        return;
    }


    /* Checks: Hydraulic conductivity */
    /* Checks: Evaporation parameters */
    /* Checks: Transpiration parameters */
    /* Checks: Surface and soil temperature */
    /* Checks: CO2 settings */
    /* Checks: Soil characterization */

    if (LT(SW_SiteIn->depthSapric, 0.)) {
        LogError(
            LogInfo,
            LOGERROR,
            "Depth at which organic matter has characteristics of "
            "sapric peat = %f (value ranges between 0 and +inf)",
            SW_SiteIn->depthSapric
        );
        return;
    }


    /* Checks: Soil water retention curve */
    /* checked by SW_SIT_init_run() */


    /* Checks: Transpiration regions */
    if (SW_SiteIn->n_transp_rgn < 1) {
        LogError(
            LogInfo,
            LOGERROR,
            "At least one transpiration region must be specified."
        );
        return;
    }

    for (LyrIndex r = 1; r < SW_SiteIn->n_transp_rgn; r++) {
        if (SW_SiteIn->TranspRgnDepths[r - 1] >=
            SW_SiteIn->TranspRgnDepths[r]) {
            LogError(
                LogInfo,
                LOGERROR,
                "Discontinuity/reversal in transpiration regions."
            );
            return;
        }
    }
}

void checkSiteRunParameters(
    SW_SITE_RUN_INPUTS *SW_SiteRunIn, LOG_INFO *LogInfo
) {
    if (LT(SW_SiteRunIn->alpha_soil_dry, 0.) ||
        GT(SW_SiteRunIn->alpha_soil_dry, 1.)) {
        LogError(
            LogInfo,
            LOGERROR,
            "Albedo of dry soil (alpha_soil_dry) = %f "
            "(value ranges between 0 and 1)",
            SW_SiteRunIn->alpha_soil_dry
        );
        return;
    }

    if (LT(SW_SiteRunIn->alpha_soil_sat, 0.) ||
        GT(SW_SiteRunIn->alpha_soil_sat, 1.)) {
        LogError(
            LogInfo,
            LOGERROR,
            "Albedo of saturated soil (alpha_soil_sat) = %f "
            "(value ranges between 0 and 1)",
            SW_SiteRunIn->alpha_soil_sat
        );
        return;
    }

    if (LT(SW_SiteRunIn->paramSoilAlbedoDarkening, 0.)) {
        LogError(
            LogInfo,
            LOGERROR,
            "Soil albedo darkening parameter (paramSoilAlbedoDarkening) = %f "
            "(value >= 0)",
            SW_SiteRunIn->paramSoilAlbedoDarkening
        );
        return;
    }
}

/** Reads soil layers and soil properties from input file

@param[in,out] SW_SoilRunIn Struct of type SW_SOIL_RUN_INPUTS describing
    the simulated site's input values
@param[in,out] n_layers Number of layers of soil within the simulation run
@param[in] txtInFiles Array of program in/output files
@param[out] LogInfo Holds information on warnings and errors

@note Previously, the function was static and named `_read_layers()`.
*/
void SW_LYR_read(
    SW_SOIL_RUN_INPUTS *SW_SoilRunIn,
    LyrIndex *n_layers,
    char *txtInFiles[],
    LOG_INFO *LogInfo
) {
    /* =================================================== */
    /* 5-Feb-2002 (cwb) removed dmin requirement in input file */

    FILE *f;
    LyrIndex lyrno;
    int x;
    int k;
    int index;
    double dmin = 0.0;
    double dmax;
    double evco;
    double trco_veg[NVEGTYPES];
    double psand;
    double pclay;
    double soildensity;
    double imperm;
    double soiltemp;
    double pfragments;
    double psom;
    char inbuf[MAX_FILENAMESIZE];
    const int numSoilVariables = 15;
    char inDoubleStrs[15][20] = {{'\0'}};
    double *inDoubleVals[15] = {
        &dmax,
        &soildensity,
        &pfragments,
        &psand,
        &pclay,
        &psom,
        &imperm,
        &soiltemp,
        &evco,
        NULL, // trco_veg[NVEGTYPES]
        NULL,
        NULL,
        NULL,
        NULL,
        NULL
    };

    ForEachVegType(k) {
        inDoubleVals[k + numSoilVariables - NVEGTYPES] = &trco_veg[k];
    }

    /* note that Files.read() must be called prior to this. */
    char *MyFileName = txtInFiles[eLayers];

    f = OpenFile(MyFileName, "r", LogInfo);
    if (LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

    while (GetALine(f, inbuf, MAX_FILENAMESIZE)) {
        lyrno = (*n_layers)++;

        x = sscanf(
            inbuf,
            "%19s %19s %19s %19s %19s %19s %19s %19s %19s %19s %19s %19s %19s "
            "%19s %19s",
            inDoubleStrs[0],
            inDoubleStrs[1],
            inDoubleStrs[2],
            inDoubleStrs[3],
            inDoubleStrs[4],
            inDoubleStrs[5],
            inDoubleStrs[6],
            inDoubleStrs[7],
            inDoubleStrs[8],
            inDoubleStrs[9],
            inDoubleStrs[10],
            inDoubleStrs[11],
            inDoubleStrs[12],
            inDoubleStrs[13],
            inDoubleStrs[14]
        );

        /* Check that we have correct number of values per layer */
        /* Adjust number if new variables are added */
        if (x != numSoilVariables) {
            LogError(
                LogInfo,
                LOGERROR,
                "%s : Incomplete record %d.\n",
                MyFileName,
                lyrno + 1
            );
            goto closeFile;
        }

        /* Convert strings to doubles */
        for (index = 0; index < numSoilVariables; index++) {
            *(inDoubleVals[index]) =
                sw_strtod(inDoubleStrs[index], MyFileName, LogInfo);
            if (LogInfo->stopRun) {
                goto closeFile;
            }
        }

        SW_SoilRunIn->depths[lyrno] = dmax;
        SW_SoilRunIn->width[lyrno] = dmax - dmin;

        /* checks for valid values now carried out by `SW_SIT_init_run()` */

        dmin = dmax;
        SW_SoilRunIn->soilDensityInput[lyrno] = soildensity;

        SW_SoilRunIn->fractionVolBulk_gravel[lyrno] = pfragments;
        SW_SoilRunIn->fractionWeightMatric_sand[lyrno] = psand;
        SW_SoilRunIn->fractionWeightMatric_clay[lyrno] = pclay;
        SW_SoilRunIn->fractionWeight_om[lyrno] = psom;

        SW_SoilRunIn->impermeability[lyrno] = imperm;
        SW_SoilRunIn->avgLyrTempInit[lyrno] = soiltemp;

        SW_SoilRunIn->evap_coeff[lyrno] = evco;

        ForEachVegType(k) {
            SW_SoilRunIn->transp_coeff[k][lyrno] = trco_veg[k];
        }

        if (lyrno >= MAX_LAYERS) {
            LogError(
                LogInfo,
                LOGERROR,
                "%s : Too many layers specified (%d).\n"
                "Maximum number of layers is %d\n",
                MyFileName,
                lyrno + 1,
                MAX_LAYERS
            );
            goto closeFile;
        }
    }

closeFile: { CloseFile(&f, LogInfo); }
}

/**
@brief Creates soil layers based on function arguments (instead of reading
them from an input file as _read_layers() does)

@param[in,out] SW_VegProdIn Struct of type SW_VEGPROD_INPUTS describing surface
    cover conditions in the simulation
@param[in,out] SW_SiteIn Struct of type SW_SITE_INPUTS describing the
    site's input values
@param[in,out] SW_SiteSim Struct of type SW_SITE_SIM describing the
    site's simulation values
@param[in,out] SW_SiteRunIn Struct of type SW_SITE_RUN_INPUTS describing the
    site's input values
@param[in,out] SW_SoilRunIn Struct of type SW_SOIL_RUN_INPUTS describing
    the simulated site's input values
@param[in,out] veg Array of size NVEGTYPES of type VegType describing
    all NVEGTYPES vegetation types through simulation-specific inputs
@param[in] nlyrs The number of soil layers to create.
@param[in] dmax Array of size \p nlyrs for depths [cm] of each soil layer
    measured from the surface
@param[in] bd Array of size \p nlyrs of soil bulk density [g/cm3]
@param[in] f_gravel Array of size \p nlyrs for volumetric gravel content [v/v]
@param[in] evco Array of size \p nlyrs with bare-soil evaporation coefficients
    [0, 1] that sum up to 1.
@param[in] trco_grassC3 Array of size \p nlyrs with transpiration coefficients
    for C3-grasses [0, 1] that sum up to 1.
@param[in] trco_grassC4 Array of size \p nlyrs with transpiration coefficients
    for C4-grasses [0, 1] that sum up to 1.
@param[in] trco_shrub Array of size \p nlyrs with transpiration coefficients
    for shrubs [0, 1] that sum up to 1.
@param[in] trco_treeNL Array of size \p nlyrs with transpiration coefficients
    for needle-leaved trees [0, 1] that sum up to 1.
@param[in] trco_treeBL Array of size \p nlyrs with transpiration coefficients
    for broad-leaved trees [0, 1] that sum up to 1.
@param[in] trco_forbs Array of size \p nlyrs with transpiration coefficients
    for forbs [0, 1] that sum up to 1.
@param[in] psand Array of size \p nlyrs for sand content of the
    soil matrix [g3 / g3]
@param[in] pclay Array of size \p nlyrs for clay content of the
    soil matrix [g3 / g3]
@param[in] imperm Array of size \p nlyrs with impermeability coefficients [0, 1]
@param[in] soiltemp Array of size \p nlyrs with initial soil temperature [C]
@param[in] pom Array of size \p nlyrs for organic matter [g3 / g3]
@param nRegions The number of transpiration regions to create. Must be between
    1 and \ref MAX_TRANSP_REGIONS.
@param[in] regionLowerBounds Array of size \p nRegions containing the lower
    depth [cm] of each region in ascending (in value) order. If you think about
    this from the perspective of soil, it would mean the shallowest bound is at
    `lowerBounds[0]`.
@param[out] n_layers Number of soil layers that were created
@param[out] LogInfo Holds information on warnings and errors

@sideeffect After deleting any previous data in the soil layer array
    SW_Site.lyr, it creates new soil layers based on the argument inputs.

@note
- This function is a modified version of the function _read_layers() in
  SW_Site.c.
- Soil moisture values must be properly initialized before running a
  simulation after this function has set soil layers, e.g.,
  SW_SWC_init_run()
*/
void set_soillayers(
    SW_VEGPROD_INPUTS *SW_VegProdIn,
    SW_SITE_INPUTS *SW_SiteIn,
    SW_SITE_RUN_INPUTS *SW_SiteRunIn,
    SW_SITE_SIM *SW_SiteSim,
    SW_SOIL_RUN_INPUTS *SW_SoilRunIn,
    VegTypeIn veg[],
    LyrIndex nlyrs,
    const double *dmax,
    const double *bd,
    const double *f_gravel,
    const double *evco,
    const double *trco_treeNL,
    const double *trco_treeBL,
    const double *trco_shrub,
    const double *trco_forbs,
    const double *trco_grassC3,
    const double *trco_grassC4,
    const double *psand,
    const double *pclay,
    const double *imperm,
    const double *soiltemp,
    const double *pom,
    LyrIndex nRegions,
    const double *regionLowerBounds,
    LyrIndex *n_layers,
    LOG_INFO *LogInfo
) {

    double dmin = 0.0;

    LyrIndex lyrno;
    unsigned int i;
    unsigned int k;

    // De-allocate and delete previous soil layers and reset counters
    SW_SIT_init_counts(SW_SiteSim, n_layers);

    // Create new soil
    for (i = 0; i < nlyrs; i++) {
        // Increment the number of soil layers
        lyrno = (*n_layers)++;

        SW_SoilRunIn->width[lyrno] = dmax[i] - dmin;
        dmin = dmax[i];
        SW_SoilRunIn->soilDensityInput[lyrno] = bd[i];
        SW_SiteIn->type_soilDensityInput = SW_BULK;
        SW_SoilRunIn->fractionVolBulk_gravel[lyrno] = f_gravel[i];
        SW_SoilRunIn->evap_coeff[lyrno] = evco[i];

        ForEachVegType(k) {
            switch (k) {
            case SW_TREENL:
                SW_SoilRunIn->transp_coeff[k][lyrno] = trco_treeNL[i];
                break;
            case SW_TREEBL:
                SW_SoilRunIn->transp_coeff[k][lyrno] = trco_treeBL[i];
                break;
            case SW_SHRUB:
                SW_SoilRunIn->transp_coeff[k][lyrno] = trco_shrub[i];
                break;
            case SW_FORBS:
                SW_SoilRunIn->transp_coeff[k][lyrno] = trco_forbs[i];
                break;
            case SW_GRASS3:
                SW_SoilRunIn->transp_coeff[k][lyrno] = trco_grassC3[i];
                break;
            case SW_GRASS4:
                SW_SoilRunIn->transp_coeff[k][lyrno] = trco_grassC4[i];
                break;
            default:
                break;
            }
        }

        SW_SoilRunIn->fractionWeightMatric_sand[lyrno] = psand[i];
        SW_SoilRunIn->fractionWeightMatric_clay[lyrno] = pclay[i];
        SW_SoilRunIn->impermeability[lyrno] = imperm[i];
        SW_SoilRunIn->avgLyrTempInit[lyrno] = soiltemp[i];
        SW_SoilRunIn->fractionWeight_om[lyrno] = pom[i];
    }

    // Set transpiration region input information
    SW_SiteSim->n_transp_rgn = nRegions;

    for (i = 0; i < nRegions; i++) {
        SW_SiteIn->TranspRgnDepths[i] = regionLowerBounds[i];
    }

    // Re-initialize site parameters based on new soil layers
    SW_SIT_init_run(
        SW_VegProdIn,
        SW_SiteIn,
        SW_SiteRunIn,
        SW_SiteSim,
        SW_SoilRunIn,
        veg,
        *n_layers,
        LogInfo
    );
}

/**
@brief Translates transpiration regions depths to soil layers

@param[out] n_transp_rgn The size of array \p TranspRgnBounds
    between 1 and \ref MAX_TRANSP_REGIONS
    (currently, shallow, moderate, deep, very deep).
@param[out] TranspRgnBounds Array of size \ref MAX_TRANSP_REGIONS
    that identifies the deepest soil layer (base1) that belongs to
    each of the transpiration regions.
@param[in] nRegions The size of array \p TranspRgnDepths. Must be
    between 1 and \ref MAX_TRANSP_REGIONS.
@param[in] TranspRgnDepths Array of size \p nRegions containing the lower
    depth [cm] from the soil surface (sorted shallowest to deepest depth).
@param[in] n_layers Number of layers of soil within the simulation run
@param[in] width The width of the layers (cm).
@param[in] transp_coeff Transpiration coefficients,
    an array of size \ref NVEGTYPES by \ref MAX_LAYERS
@param[out] LogInfo Holds information on warnings and errors
*/
void derive_TranspRgnBounds(
    LyrIndex *n_transp_rgn,
    LyrIndex TranspRgnBounds[],
    const LyrIndex nRegions,
    const double TranspRgnDepths[],
    const LyrIndex n_layers,
    const double width[],
    double transp_coeff[][MAX_LAYERS],
    LOG_INFO *LogInfo
) {
    unsigned int i;
    unsigned int j;
    double totalDepth = 0;
    LyrIndex layer;
    LyrIndex UNDEFINED_LAYER = 999;

    /* ------------- Error checking --------------- */
    if (nRegions < 1 || nRegions > MAX_TRANSP_REGIONS) {
        LogError(
            LogInfo,
            LOGERROR,
            "derive_TranspRgnBounds: invalid number of regions (%d)\n",
            nRegions
        );
        return; // Exit function prematurely due to error
    }

    /* --------------- Clear out the array ------------------ */
    for (i = 0; i < MAX_TRANSP_REGIONS; ++i) {
        // Setting bounds to a ridiculous number so we know how many get set.
        TranspRgnBounds[i] = UNDEFINED_LAYER;
    }

    /* ----------------- Derive Regions ------------------- */
    // Loop through the regions the user wants to derive
    layer = 0; // SW_Site.lyr is base0-indexed
    totalDepth = 0;
    for (i = 0; i < nRegions && layer < n_layers; ++i) {
        // Find the last soil layer that is completely contained within a region
        // It becomes the bound.
        while (layer < n_layers && LE(totalDepth, TranspRgnDepths[i]) &&
               LE((totalDepth + width[layer]), TranspRgnDepths[i]) &&
               sum_across_vegtypes(transp_coeff, layer) > 0) {
            totalDepth += width[layer];
            layer++;
            TranspRgnBounds[i] = layer; // TranspRgnBounds is base1
        }
    }

    /* -------------- Check for duplicates -------------- */
    for (i = 0; i < nRegions - 1; ++i) {
        // If there is a duplicate bound we will remove it by left shifting the
        // array, overwriting the duplicate.
        if (TranspRgnBounds[i] == TranspRgnBounds[i + 1]) {
            for (j = i + 1; j < nRegions - 1; ++j) {
                TranspRgnBounds[j] = TranspRgnBounds[j + 1];
            }
            TranspRgnBounds[MAX_TRANSP_REGIONS - 1] = UNDEFINED_LAYER;
        }
    }

    /* -------------- Derive n_transp_rgn --------------- */
    *n_transp_rgn = 0;
    while (*n_transp_rgn < MAX_TRANSP_REGIONS &&
           TranspRgnBounds[*n_transp_rgn] != UNDEFINED_LAYER) {
        (*n_transp_rgn)++;
    }
}

/** Obtain soil water retention curve parameters from disk

The first set (row) of parameters represent fibric peat;
the second set of parameters represent sapric peat;
the remaining rows represent parameters of the mineral soil in each soil layer.

@param[in,out] SW_SiteSim Struct of type SW_SITE_SIM describing the simulated
site's simulation values
@param[in] n_layers Number of layers of soil within the simulation run
@param[in] txtInFiles Array of program in/output files
@param[in] inputsProvideSWRCp Are SWRC parameters obtained from
    input files (TRUE) or estimated with a PTF (FALSE)
@param[out] swrcpMineralSoil SWRC parameters of the mineral soil component
@param[out] swrcpOM Array of length two of vectors of SWRC parameters
    of fibric peat (surface conditions) and
    of sapric peat (at depth `depthSapric`)
@param[out] LogInfo Holds information on warnings and errors
*/
void SW_SWRC_read(
    SW_SITE_SIM *SW_SiteSim,
    LyrIndex n_layers,
    char *txtInFiles[],
    Bool inputsProvideSWRCp,
    double swrcpMineralSoil[][SWRC_PARAM_NMAX],
    double swrcpOM[][SWRC_PARAM_NMAX],
    LOG_INFO *LogInfo
) {
    FILE *f;
    LyrIndex lyrno = 0;
    LyrIndex k;
    /* Indicator if inputs are for organic or mineral soil components */
    Bool isMineral = swFALSE;
    int x;
    int index;
    double tmp_swrcp[SWRC_PARAM_NMAX];
    char inbuf[MAX_FILENAMESIZE];
    char swrcpDoubleStrs[SWRC_PARAM_NMAX][20] = {{'\0'}};

    /* note that Files.read() must be called prior to this. */
    char *MyFileName = txtInFiles[eSWRCp];

    f = OpenFile(MyFileName, "r", LogInfo);
    if (LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

    while (GetALine(f, inbuf, MAX_FILENAMESIZE)) {
        /* PTF used for mineral swrcp: read only organic parameters from disk */
        if (isMineral && !inputsProvideSWRCp) {
            goto closeFile;
        }

        x = sscanf(
            inbuf,
            "%19s %19s %19s %19s %19s %19s",
            swrcpDoubleStrs[0],
            swrcpDoubleStrs[1],
            swrcpDoubleStrs[2],
            swrcpDoubleStrs[3],
            swrcpDoubleStrs[4],
            swrcpDoubleStrs[5]
        );

        /* Note: `SW_SIT_init_run()` will check for valid values */

        /* Check that we have n = `SWRC_PARAM_NMAX` values per layer */
        if (x != SWRC_PARAM_NMAX) {
            LogError(
                LogInfo,
                LOGERROR,
                "%s : Bad number of SWRC parameters %d -- must be %d.\n",
                "Site",
                x,
                SWRC_PARAM_NMAX
            );
            goto closeFile;
        }

        /* Convert strings to doubles */
        for (index = 0; index < SWRC_PARAM_NMAX; index++) {
            tmp_swrcp[index] =
                sw_strtod(swrcpDoubleStrs[index], MyFileName, LogInfo);
            if (LogInfo->stopRun) {
                goto closeFile;
            }
        }

        /* Check that we are within `SW_SiteIn.n_layers` */
        if (isMineral && lyrno >= n_layers) {
            LogError(
                LogInfo,
                LOGERROR,
                "%s : Number of layers with SWRC parameters (%d) "
                "must match number of soil layers (%d)\n",
                "Site",
                lyrno + 1,
                n_layers
            );
            goto closeFile;
        }

        /* Copy values into structure */
        for (k = 0; k < SWRC_PARAM_NMAX; k++) {
            if (isMineral) {
                swrcpMineralSoil[lyrno][k] = tmp_swrcp[k];
            } else {
                swrcpOM[lyrno][k] = tmp_swrcp[k];
            }
        }

        lyrno++;

        if (!isMineral && lyrno > 1) {
            /* Fibric and sapric peat are completed.
            Now: reset and restart for swrcp of the mineral component */
            isMineral = swTRUE;
            lyrno = 0;
        }
    }

    SW_SiteSim->site_has_swrcpMineralSoil =
        (Bool) (isMineral && inputsProvideSWRCp);

closeFile: { CloseFile(&f, LogInfo); }
}

/**
@brief Update necessary values for site-related values

@param[in] methodMaxDepthSoilTemperature Method for soil temperature
at maximum depth:
        0 (user provided value);
        1 (dynamically calculated from a moving long-term mean annual air
           temperature, see `nYearsDynamicLong` from veg.in)
@param[in] newTsoil_constant New soil temperature at maximum depth
@param[out] Tsoil_constant Soil temperature at a depth where soil temperature
is (mostly) constant in time; for instance, approximated as the mean air
temperature
*/
void SW_SIT_new_year(
    unsigned int methodMaxDepthSoilTemperature,
    double newTsoil_constant,
    double *Tsoil_constant
) {
    if (methodMaxDepthSoilTemperature == 1) {
        *Tsoil_constant = newTsoil_constant;
    }
}

/**
@brief Derive and check soil properties from inputs

Bulk refers to the whole soil,
i.e., including the rock/gravel component (coarse fragments),
whereas matric refers to the < 2 mm fraction.

Internally, SOILWAT2 calculates based on bulk soil, i.e., the whole
soil. However, sand and clay inputs are expected to represent the soil
matric, i.e., the < 2 mm fraction.

sand + clay + silt must equal one.
Fraction of silt is calculated: 1 - (sand + clay).

@param[in,out] SW_VegProdIn Struct of type SW_VEGPROD_INPUTS describing surface
    cover conditions in the simulation
@param[in,out] SW_SiteIn Struct of type SW_SITE_INPUTS describing the
    site's input values
@param[in,out] SW_SiteRunIn Struct of type SW_SITE_RUN_ INPUTS describing the
    site's input values
@param[in,out] SW_SiteSim Struct of type SW_SITE_SIM describing the
    site's simulation values
@param[in,out] SW_SoilRunIn Struct of type SW_SOIL_RUN_INPUTS describing
    the simulated site's input values
@param[in,out] vegIn A struct of type VegTypeIn holding arrays of size
NVEGTYPES to hold static simulation data for each simulation type
@param[in] n_layers Number of layers of soil within the simulation run
@param[out] LogInfo Holds information on warnings and errors

@sideeffect Values stored in global variable `SW_Site`.
*/
void SW_SIT_init_run(
    SW_VEGPROD_INPUTS *SW_VegProdIn,
    SW_SITE_INPUTS *SW_SiteIn,
    SW_SITE_RUN_INPUTS *SW_SiteRunIn,
    SW_SITE_SIM *SW_SiteSim,
    SW_SOIL_RUN_INPUTS *SW_SoilRunIn,
    VegTypeIn *vegIn,
    LyrIndex n_layers,
    LOG_INFO *LogInfo
) {
    /* =================================================== */
    /* potentially this routine can be called whether the
     * layer data came from a file or a function call which
     * still requires initialization.
     */
    /* 5-Mar-2002 (cwb) added normalization for ev and tr coefficients */
    /* 1-Oct-03 (cwb) removed sum_evap_coeff and sum_transp_coeff  */

#ifdef SWDEBUG
    int debug = 0;
#endif

    LyrIndex s;
    LyrIndex r;
    LyrIndex curregion;
    int k;
    int flagswpcrit = 0;
    double evsum = 0.;
    double trsum_veg[NVEGTYPES] = {0.};
    double tmp;
    double acc = 0.0;
    double tmp_stNRGR;

    double default_stMaxDepth = 990.;
    double default_stDeltaX = 15.;
    unsigned int default_stNRGR =
        (unsigned int) (default_stMaxDepth / default_stDeltaX - 1);

    Bool hasOM = swFALSE;


    /* Check that we have a suitable number of soil layers */
    if (n_layers == 0) {
        LogError(LogInfo, LOGERROR, "The soil profile has 0 layers.");
    }

    if (n_layers > MAX_LAYERS) {
        LogError(
            LogInfo,
            LOGERROR,
            "The soil profile has too many layers (n = %d > max = %d).",
            n_layers,
            MAX_LAYERS
        );
    }

    /* Check Site Run inputs */
    checkSiteRunParameters(SW_SiteRunIn, LogInfo);
    if (LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

    /* Transpiration: use equations to estimate trco */
    if (SW_SiteIn->methodTrCo == 1) {
        estimate_trco(
            SW_SoilRunIn->transp_coeff,
            SW_SoilRunIn->depths,
            SW_SoilRunIn->impermeability,
            vegIn,
            n_layers
        );
    }

    /* Transpiration: count number of layers with potential for transpiration */
    nlayers_vegroots(
        n_layers, SW_SiteSim->n_transp_lyrs, SW_SoilRunIn->transp_coeff
    );

    /* Transpiration: transpiration regions by soil layers */
    derive_TranspRgnBounds(
        &SW_SiteSim->n_transp_rgn,
        SW_SiteSim->TranspRgnBounds,
        SW_SiteIn->n_transp_rgn,
        SW_SiteIn->TranspRgnDepths,
        n_layers,
        SW_SoilRunIn->width,
        SW_SoilRunIn->transp_coeff,
        LogInfo
    );
    if (LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }


    /* Evaporation from soil: estimate potential evaporation coefficients */
    if (SW_SiteIn->methodEvCo == 1) {
        estimate_evco(
            SW_SoilRunIn->evap_coeff,
            SW_SoilRunIn->depths,
            SW_SoilRunIn->width,
            SW_SoilRunIn->fractionWeightMatric_sand,
            SW_SoilRunIn->fractionWeightMatric_clay,
            SW_SoilRunIn->impermeability,
            n_layers
        );
    }

    /* Evaporation from soil: count layers with potential for Es */
    SW_SiteSim->n_evap_lyrs =
        nlayers_bsevap(SW_SoilRunIn->evap_coeff, n_layers);


    /* Manage deep drainage */
    add_deepdrain_layer(SW_SiteSim, n_layers, SW_SiteIn->deepdrain);


    /* Check compatibility between selected SWRC and PTF */
    if (!SW_SiteSim->site_has_swrcpMineralSoil) {
        if (!check_SWRC_vs_PTF(
                SW_SiteIn->site_swrc_name, SW_SiteIn->site_ptf_name
            )) {
            LogError(
                LogInfo,
                LOGERROR,
                "Selected PTF '%s' is incompatible with selected SWRC '%s'\n",
                SW_SiteIn->site_ptf_name,
                SW_SiteIn->site_swrc_name
            );
            return; // Exit function prematurely due to error
        }
    }

    /* Check if there is organic matter in soil layers */
    for (s = 0; s < n_layers && !hasOM; s++) {
        if (GT(SW_SoilRunIn->fractionWeight_om[s], 0.)) {
            hasOM = swTRUE;
        }
    }

    /* Check parameters of organic SWRC */
    if (hasOM) {
        if (!SWRC_check_parameters(
                SW_SiteIn->site_swrc_type, SW_SiteIn->swrcpOM[0], LogInfo
            )) {
            LogError(
                LogInfo,
                LOGERROR,
                "Checks of parameters for SWRC '%s' in fibric peat failed.",
                swrc2str[SW_SiteIn->site_swrc_type]
            );
            return; // Exit function prematurely due to error
        }

        if (!SWRC_check_parameters(
                SW_SiteIn->site_swrc_type, SW_SiteIn->swrcpOM[1], LogInfo
            )) {
            LogError(
                LogInfo,
                LOGERROR,
                "Checks of parameters for SWRC '%s' in sapric peat failed.",
                swrc2str[SW_SiteIn->site_swrc_type]
            );
            return; // Exit function prematurely due to error
        }
    }


    /* Loop over soil layers check variables and calculate parameters */
    ForEachSoilLayer(s, n_layers) {
        // copy depths of soil layer profile
        acc += SW_SoilRunIn->width[s];
        SW_SoilRunIn->depths[s] = acc;

        /* Copy site-level SWRC/PTF information to each layer:
           We currently allow specifying one SWRC/PTF for a site for all layers;
           remove in the future if we allow SWRC/PTF to vary by soil layer
        */
        SW_SiteSim->swrc_type[s] = SW_SiteIn->site_swrc_type;
        SW_SiteSim->ptf_type[s] = SW_SiteIn->site_ptf_type;


        /* Check soil properties for valid values */
        if (!SW_check_soil_properties(SW_SoilRunIn, s, LogInfo)) {
            if (LogInfo->stopRun) {
                return; // Exit function prematurely due to error
            }

            LogError(
                LogInfo,
                LOGERROR,
                "Invalid soil properties in layer %d.\n",
                s + 1
            );

            return; // Exit function prematurely due to error
        }


        /* Update soil density depending on inputs */
        switch (SW_SiteIn->type_soilDensityInput) {

        case SW_BULK:
            SW_SiteSim->soilBulk_density[s] = SW_SoilRunIn->soilDensityInput[s];

            SW_SiteSim->soilMatric_density[s] = calculate_soilMatricDensity(
                SW_SiteSim->soilBulk_density[s],
                SW_SoilRunIn->fractionVolBulk_gravel[s],
                LogInfo
            );
            if (LogInfo->stopRun) {
                return; // Exit function prematurely due to error
            }

            break;

        case SW_MATRIC:
            SW_SiteSim->soilMatric_density[s] =
                SW_SoilRunIn->soilDensityInput[s];

            SW_SiteSim->soilBulk_density[s] = calculate_soilBulkDensity(
                SW_SiteSim->soilMatric_density[s],
                SW_SoilRunIn->fractionVolBulk_gravel[s]
            );

            break;

        default:
            LogError(
                LogInfo,
                LOGERROR,
                "Soil density type not recognized",
                SW_SiteIn->type_soilDensityInput
            );
            return; // Exit function prematurely due to error
        }


        if (!SW_SiteSim->site_has_swrcpMineralSoil) {
            /* Use pedotransfer function PTF
               to estimate parameters of soil water retention curve (SWRC) for
               the mineral component of the layer.
               If `site_has_swrcpMineralSoil`, then parameters have already
               been obtained from disk by `SW_SWRC_read()`
            */
            SWRC_PTF_estimate_parameters(
                SW_SiteSim->ptf_type[s],
                SW_SoilRunIn->swrcpMineralSoil[s],
                SW_SoilRunIn->fractionWeightMatric_sand[s],
                SW_SoilRunIn->fractionWeightMatric_clay[s],
                SW_SoilRunIn->fractionVolBulk_gravel[s],
                SW_SiteSim->soilBulk_density[s],
                LogInfo
            );
            if (LogInfo->stopRun) {
                return; // Exit function prematurely due to error
            }
        }

        /* Check parameters of mineral soil SWRC */
        if (!SWRC_check_parameters(
                SW_SiteSim->swrc_type[s],
                SW_SoilRunIn->swrcpMineralSoil[s],
                LogInfo
            )) {
            LogError(
                LogInfo,
                LOGERROR,
                "Checks of parameters for SWRC '%s' "
                "in the mineral component of layer %d failed.",
                swrc2str[SW_SiteSim->swrc_type[s]],
                s + 1
            );
            return; // Exit function prematurely due to error
        }

        /* Calculate bulk soil SWRCp from organic and mineral soil components */
        SWRC_bulkSoilParameters(
            SW_SiteSim->swrc_type[s],
            SW_SiteSim->swrcp[s],
            SW_SoilRunIn->swrcpMineralSoil[s],
            SW_SiteIn->swrcpOM,
            SW_SoilRunIn->fractionWeight_om[s],
            SW_SiteIn->depthSapric,
            (s > 0) ? SW_SoilRunIn->depths[s - 1] : 0,
            SW_SoilRunIn->depths[s]
        );

        /* Check parameters of bulk soil SWRC */
        if (!SWRC_check_parameters(
                SW_SiteSim->swrc_type[s], SW_SiteSim->swrcp[s], LogInfo
            )) {
            LogError(
                LogInfo,
                LOGERROR,
                "Checks of parameters for SWRC '%s' "
                "in the bulk soil layer %d failed.",
                swrc2str[SW_SiteSim->swrc_type[s]],
                s + 1
            );
            return; // Exit function prematurely due to error
        }

        /* Extract ksat from swrcp */
        SW_SiteSim->ksat[s] =
            SWRC_get_ksat(SW_SiteSim->swrc_type[s], SW_SiteSim->swrcp[s]);

        /* Calculate SWC at field capacity and at wilting point */
        SW_SiteSim->swcBulk_fieldcap[s] =
            SW_SWRC_SWPtoSWC(0.333, SW_SoilRunIn, SW_SiteSim, s, LogInfo);
        if (LogInfo->stopRun) {
            return; // Exit function prematurely due to error
        }

        SW_SiteSim->swcBulk_wiltpt[s] =
            SW_SWRC_SWPtoSWC(15., SW_SoilRunIn, SW_SiteSim, s, LogInfo);
        if (LogInfo->stopRun) {
            return; // Exit function prematurely due to error
        }


        /* Calculate lower SWC limit of bare-soil evaporation
            as `max(0.5 * wiltpt, SWC@hygroscopic)`

            Notes:
                - `0.5 * wiltpt` is the E_soil limit from FAO-56
                  (Allen et al. 1998) which may correspond to
                  unrealistically extreme SWP
                - `SWC at hygroscopic point`
                  (-10 MPa; e.g., Porporato et al. 2001)
                  describes "air-dry" soil
                - Also make sure that `>= swc_min`, see below
        */
        SW_SiteSim->swcBulk_halfwiltpt[s] = fmax(
            0.5 * SW_SiteSim->swcBulk_wiltpt[s],
            SW_SWRC_SWPtoSWC(100., SW_SoilRunIn, SW_SiteSim, s, LogInfo)
        );
        if (LogInfo->stopRun) {
            return; // Exit function prematurely due to error
        }


        /* Extract or estimate additional properties */
        SW_SiteSim->swcBulk_saturated[s] = SW_swcBulk_saturated(
            SW_SiteSim->swrc_type[s],
            SW_SiteSim->swrcp[s],
            SW_SoilRunIn->fractionVolBulk_gravel[s],
            SW_SoilRunIn->width[s],
            SW_SiteSim->ptf_type[s],
            SW_SoilRunIn->fractionWeightMatric_sand[s],
            SW_SoilRunIn->fractionWeightMatric_clay[s],
            SW_SoilRunIn->fractionWeight_om[s],
            LogInfo
        );
        if (LogInfo->stopRun) {
            return; // Exit function prematurely due to error
        }

        SW_SiteSim->swcBulk_min[s] = SW_swcBulk_minimum(
            SW_SiteSim->swrc_type[s],
            SW_SiteSim->swrcp[s],
            SW_SoilRunIn->fractionVolBulk_gravel[s],
            SW_SoilRunIn->width[s],
            SW_SiteSim->ptf_type[s],
            SW_SiteIn->SWCMinVal,
            SW_SoilRunIn->fractionWeightMatric_sand[s],
            SW_SoilRunIn->fractionWeightMatric_clay[s],
            SW_SoilRunIn->fractionWeight_om[s],
            SW_SiteSim->swcBulk_saturated[s],
            SW_SiteIn->SWCMinVal,
            LogInfo
        );
        if (LogInfo->stopRun) {
            return; // Exit function prematurely due to error
        }


        /* Calculate wet limit of SWC for what inputs defined as wet */
        SW_SiteSim->swcBulk_wet[s] =
            GE(SW_SiteIn->SWCWetVal, 1.0) ?
                SW_SWRC_SWPtoSWC(
                    SW_SiteIn->SWCWetVal, SW_SoilRunIn, SW_SiteSim, s, LogInfo
                ) :
                SW_SiteIn->SWCWetVal * SW_SoilRunIn->width[s];
        if (LogInfo->stopRun) {
            return; // Exit function prematurely due to error
        }

        /* Calculate initial SWC based on inputs */
        SW_SiteSim->swcBulk_init[s] =
            GE(SW_SiteIn->SWCInitVal, 1.0) ?
                SW_SWRC_SWPtoSWC(
                    SW_SiteIn->SWCInitVal, SW_SoilRunIn, SW_SiteSim, s, LogInfo
                ) :
                SW_SiteIn->SWCInitVal * SW_SoilRunIn->width[s];
        if (LogInfo->stopRun) {
            return; // Exit function prematurely due to error
        }

        /* test validity of calculated values */
        if (LT(SW_SiteSim->swcBulk_init[s], SW_SiteSim->swcBulk_min[s])) {
            LogError(
                LogInfo,
                LOGERROR,
                "Soil layer %d: swcBulk_init (%f cm) <= "
                "swcBulk_min (%f cm) but should be larger",
                s + 1,
                SW_SiteSim->swcBulk_init[s],
                SW_SiteSim->swcBulk_min[s]
            );
            return; // Exit function prematurely due to error
        }

        if (LT(SW_SiteSim->swcBulk_wiltpt[s], SW_SiteSim->swcBulk_min[s])) {
            LogError(
                LogInfo,
                LOGERROR,
                "Soil layer %d: swcBulk_wiltpt (%f cm) <= "
                "swcBulk_min (%f cm) but should be larger",
                s + 1,
                SW_SiteSim->swcBulk_wiltpt[s],
                SW_SiteSim->swcBulk_min[s]
            );
            return; // Exit function prematurely due to error
        }

        if (LT(SW_SiteSim->swcBulk_halfwiltpt[s], SW_SiteSim->swcBulk_min[s])) {
            LogError(
                LogInfo,
                LOGWARN,
                "Soil layer %d: calculated "
                "swcBulk_halfwiltpt (%f cm / %f MPa) <= "
                "swcBulk_min (%f cm / %f MPa); therefore, "
                "swcBulk_halfwiltpt was set to the value of swcBulk_min",
                s + 1,
                SW_SiteSim->swcBulk_halfwiltpt[s],
                -0.1 * SW_SWRC_SWCtoSWP(
                           SW_SiteSim->swcBulk_halfwiltpt[s],
                           SW_SoilRunIn,
                           SW_SiteSim,
                           s,
                           LogInfo
                       ),
                SW_SiteSim->swcBulk_min[s],
                -0.1 * SW_SWRC_SWCtoSWP(
                           SW_SiteSim->swcBulk_min[s],
                           SW_SoilRunIn,
                           SW_SiteSim,
                           s,
                           LogInfo
                       )
            );
            if (LogInfo->stopRun) {
                return; // Exit function prematurely due to error
            }

            SW_SiteSim->swcBulk_halfwiltpt[s] = SW_SiteSim->swcBulk_min[s];
        }

        if (LE(SW_SiteSim->swcBulk_wet[s], SW_SiteSim->swcBulk_min[s])) {
            LogError(
                LogInfo,
                LOGERROR,
                "Soil layer %d: calculated swcBulk_wet (%f cm) <= "
                "swcBulk_min (%f cm)",
                s + 1,
                SW_SiteSim->swcBulk_wet[s],
                SW_SiteSim->swcBulk_min[s]
            );
            return; // Exit function prematurely due to error
        }


#ifdef SWDEBUG
        if (debug) {
            sw_printf(
                "L[%d] swcmin=%f = swpmin=%f\n",
                s,
                SW_SiteSim->swcBulk_min[s],
                SW_SWRC_SWCtoSWP(
                    SW_SiteSim->swcBulk_min[s],
                    SW_SoilRunIn,
                    SW_SiteSim,
                    s,
                    LogInfo
                )
            );
            if (LogInfo->stopRun) {
                return; // Exit function prematurely due to error
            }

            sw_printf(
                "L[%d] SWC(HalfWiltpt)=%f = swp(hw)=%f\n",
                s,
                SW_SiteSim->swcBulk_halfwiltpt[s],
                SW_SWRC_SWCtoSWP(
                    SW_SiteSim->swcBulk_halfwiltpt[s],
                    SW_SoilRunIn,
                    SW_SiteSim,
                    s,
                    LogInfo
                )
            );
            if (LogInfo->stopRun) {
                return; // Exit function prematurely due to error
            }
        }
#endif


        /* Calculate helper values for metric_*() functionality */
        SW_SiteSim->baseSWC30bar[s] =
            SW_SWRC_SWPtoSWC(30., SW_SoilRunIn, SW_SiteSim, s, LogInfo);
        if (LogInfo->stopRun) {
            return; // Exit function prematurely due to error
        }
        SW_SiteSim->baseSWC39bar[s] =
            SW_SWRC_SWPtoSWC(39., SW_SoilRunIn, SW_SiteSim, s, LogInfo);
        if (LogInfo->stopRun) {
            return; // Exit function prematurely due to error
        }
        SW_SiteSim->slWeight100[s] =
            soilLayerWeight(100., s, SW_SoilRunIn->depths, n_layers);

        /* sum ev and tr coefficients for later */
        evsum += SW_SoilRunIn->evap_coeff[s];

        ForEachVegType(k) {
            trsum_veg[k] += SW_SoilRunIn->transp_coeff[k][s];
            /* calculate soil water content at SWPcrit for each vegetation type
             */
            SW_SiteSim->swcBulk_atSWPcrit[k][s] = SW_SWRC_SWPtoSWC(
                vegIn->SWPcrit[k], SW_SoilRunIn, SW_SiteSim, s, LogInfo
            );
            if (LogInfo->stopRun) {
                return; // Exit function prematurely due to error
            }

            if (LT(SW_SiteSim->swcBulk_atSWPcrit[k][s],
                   SW_SiteSim->swcBulk_min[s])) {
                flagswpcrit++;

                // lower SWcrit [-bar] to SWP-equivalent of swBulk_min
                tmp = fmin(
                    vegIn->SWPcrit[k],
                    SW_SWRC_SWCtoSWP(
                        SW_SiteSim->swcBulk_min[s],
                        SW_SoilRunIn,
                        SW_SiteSim,
                        s,
                        LogInfo
                    )
                );
                if (LogInfo->stopRun) {
                    return; // Exit function prematurely due to error
                }

                LogError(
                    LogInfo,
                    LOGWARN,
                    "Soil layer %d - vegtype %d: "
                    "calculated swcBulk_atSWPcrit (%f cm / %f MPa) "
                    "<= `swcBulk_min` (%f cm / %f MPa); thus, "
                    "SWcrit was adjusted to %f MPa "
                    "(and swcBulk_atSWPcrit in every soil layer is "
                    "re-calculated)",
                    s + 1,
                    k + 1,
                    SW_SiteSim->swcBulk_atSWPcrit[k][s],
                    -0.1 * vegIn->SWPcrit[k],
                    SW_SiteSim->swcBulk_min[s],
                    -0.1 * SW_SWRC_SWCtoSWP(
                               SW_SiteSim->swcBulk_min[s],
                               SW_SoilRunIn,
                               SW_SiteSim,
                               s,
                               LogInfo
                           ),
                    -0.1 * tmp
                );
                if (LogInfo->stopRun) {
                    return; // Exit function prematurely due to error
                }

                vegIn->SWPcrit[k] = tmp;
            }

            /* Identify the transpiration region that contains
               the current soil layer s.
               Note: TranspRgnBounds, curregion, and my_transp_rgn are base1;
               s is base0.
            */
            curregion = 0;
            ForEachTranspRegion(r, SW_SiteSim->n_transp_rgn) {
                if (s < SW_SiteSim->TranspRgnBounds[r] &&
                    SW_SiteSim->TranspRgnBounds[r] <= MAX_LAYERS) {

                    if (ZRO(SW_SoilRunIn->transp_coeff[k][s])) {
                        continue; /* skip this layer */
                    }

                    curregion = r + 1; // convert to base1
                    break;
                }
            }

            // if curregion == 0, then no transpiration region or no roots
            SW_SiteSim->my_transp_rgn[k][s] = curregion;
        }
    } /*end ForEachSoilLayer */

    SW_SiteSim->site_has_swrcpMineralSoil = swTRUE;


    /* Re-calculate `swcBulk_atSWPcrit` if it was below `swcBulk_min`
       for any vegetation x soil layer combination using adjusted `SWPcrit`
    */
    if (flagswpcrit) {
        ForEachSoilLayer(s, n_layers) {
            ForEachVegType(k) {
                /* calculate soil water content at adjusted SWPcrit */
                SW_SiteSim->swcBulk_atSWPcrit[k][s] = SW_SWRC_SWPtoSWC(
                    vegIn->SWPcrit[k], SW_SoilRunIn, SW_SiteSim, s, LogInfo
                );
                if (LogInfo->stopRun) {
                    return; // Exit function prematurely due to error
                }

                if (LT(SW_SiteSim->swcBulk_atSWPcrit[k][s],
                       SW_SiteSim->swcBulk_min[s])) {
                    LogError(
                        LogInfo,
                        LOGERROR,
                        "Soil layer %d - vegtype %d: "
                        "calculated swcBulk_atSWPcrit (%f cm) "
                        "<= swcBulk_min (%f cm) "
                        "despite adjusted SWcrit (%f MPa)",
                        s + 1,
                        k + 1,
                        SW_SiteSim->swcBulk_atSWPcrit[k][s],
                        SW_SiteSim->swcBulk_min[s],
                        -0.1 * vegIn->SWPcrit[k]
                    );
                    return; // Exit function prematurely due to error
                }
            }
        }

        /* Update values for `get_swa()` */
        ForEachVegType(k) {
            SW_VegProdIn->critSoilWater[k] = -0.1 * vegIn->SWPcrit[k];
        }
        get_critical_rank(SW_VegProdIn);
    }

    /* normalize the evap and transp coefficients separately
     * to avoid obfuscation in the above loop
     * inputs are not more precise than at most 3-4 digits */
    if (!EQ_w_tol(evsum, 1.0, 1e-4)) {

        ForEachEvapLayer(s, SW_SiteSim->n_evap_lyrs) {
            SW_SoilRunIn->evap_coeff[s] /= evsum;
        }

        LogError(
            LogInfo,
            LOGWARN,
            "Normalization was applied to evaporation coefficients (evco): "
            "sum across soil profile was %.4f (now is 1.0).",
            evsum
        );
    }

    ForEachVegType(k) {
        // inputs are not more precise than at most 3-4 digits
        if (!EQ_w_tol(trsum_veg[k], 1.0, 1e-4)) {

            ForEachSoilLayer(s, n_layers) {
                if (GT(SW_SoilRunIn->transp_coeff[k][s], 0.)) {
                    SW_SoilRunIn->transp_coeff[k][s] /= trsum_veg[k];
                }
            }

            LogError(
                LogInfo,
                LOGWARN,
                "Normalization was applied to rooting profile (trco) for '%s': "
                "sum across soil profile was %.4f (now is 1.0).",
                key2veg[k],
                trsum_veg[k]
            );
        }
    }

    // getting the number of regressions, for use in the soil_temperature
    // function
    tmp_stNRGR = (SW_SiteIn->stMaxDepth / SW_SiteIn->stDeltaX);
    if (tmp_stNRGR > (double) UINT_MAX) {
        SW_SiteSim->stNRGR = MAX_ST_RGR + 1;
    } else {
        SW_SiteSim->stNRGR = (unsigned int) tmp_stNRGR - 1;
    }

    Bool too_many_RGR = (Bool) (SW_SiteSim->stNRGR + 1 >= MAX_ST_RGR);

    if (!EQ(fmod(SW_SiteIn->stMaxDepth, SW_SiteIn->stDeltaX), 0.0) ||
        too_many_RGR) {

        if (too_many_RGR) {
            // because we will use loops such `for (i = 0; i <= nRgr + 1; i++)`
            LogError(
                LogInfo,
                LOGWARN,
                "User provided too many soil temperature layers "
                "(%d > max = %d). ",
                "Simulation continues with adjusted values: "
                "profile depth = %.1f [cm] (from %.1f); "
                "layer width = %.1f [cm] (from %.1f); "
                "with n = %d layers (from %d).",
                SW_SiteSim->stNRGR,
                MAX_ST_RGR,
                default_stMaxDepth,
                SW_SiteIn->stMaxDepth,
                default_stDeltaX,
                SW_SiteIn->stDeltaX,
                default_stNRGR,
                SW_SiteSim->stNRGR
            );
        } else {
            // because we don't deal with partial layers
            LogError(
                LogInfo,
                LOGWARN,
                "User provided inconsistent soil temperature profile "
                "(profile depth %.1f was not divisible by layer width %.1f). "
                "Simulation continues with adjusted values: "
                "profile depth = %.1f [cm] (from %.1f); "
                "layer width = %.1f [cm] (from %.1f); "
                "with n = %d layers (from %d).",
                SW_SiteIn->stMaxDepth,
                SW_SiteIn->stDeltaX,
                default_stMaxDepth,
                SW_SiteIn->stMaxDepth,
                default_stDeltaX,
                SW_SiteIn->stDeltaX,
                default_stNRGR,
                SW_SiteSim->stNRGR
            );
        }

        // resets it to the default values
        SW_SiteIn->stMaxDepth = default_stMaxDepth;
        SW_SiteIn->stDeltaX = default_stDeltaX;
        SW_SiteSim->stNRGR = default_stNRGR;
    }
}

/**
@brief Reset counts of `SW_Site` to zero

@param[out] SW_SiteSim Struct of type SW_SITE_SIM describing the simulated
site's values used during simulation
@param[out] n_layers Number of layers of soil within the simulation run;
return zeroed value
*/
void SW_SIT_init_counts(SW_SITE_SIM *SW_SiteSim, LyrIndex *n_layers) {
    int k;

    // Reset counts
    *n_layers = 0;
    SW_SiteSim->n_evap_lyrs = 0;
    SW_SiteSim->deep_lyr = 0;

    ForEachVegType(k) { SW_SiteSim->n_transp_lyrs[k] = 0; }

    SW_SiteSim->site_has_swrcpMineralSoil = swFALSE;
}

/**
@brief Print site-parameters and soil characteristics.

@param[in] SW_SiteIn Struct of type SW_SITE_INPUTS describing the simulated
site's input values
@param[in] SW_SiteSim Struct of type SW_SITE_SIM describing the simulated site's
    simulation values
@param[in] ModelRunIn Struct of type SW_MODEL_RUN_INPUTS holding basic input
    time information about the simulation
@param[in] SW_SoilRunIn Struct of type SW_SOIL_RUN_INPUTS describing
    the simulated site's input values
@param[in] n_layers Number of layers of soil within the simulation run
@param[in] Tsoil_constant Soil temperature at a depth where soil temperature
    is (mostly) constant in time
*/
void echo_inputs(
    SW_SITE_INPUTS *SW_SiteIn,
    SW_SITE_SIM *SW_SiteSim,
    SW_MODEL_RUN_INPUTS *ModelRunIn,
    SW_SOIL_RUN_INPUTS *SW_SoilRunIn,
    LyrIndex n_layers,
    double Tsoil_constant
) {
    /* =================================================== */
    LyrIndex i;
    int k;
    LOG_INFO LogInfo;
    FILE *logInitPtr = NULL;

#if !defined(RSOILWAT)
    logInitPtr = stdout;
#endif

    sw_init_logs(logInitPtr, &LogInfo);

    sw_printf("\n\n=====================================================\n"
              "Site Related Parameters:\n"
              "---------------------\n");
    sw_printf("  Site File: 'siteparam.in'\n");
    sw_printf(
        "  Reset SWC values each year: %s\n",
        (SW_SiteIn->reset_yr) ? "swTRUE" : "swFALSE"
    );
    sw_printf(
        "  Use deep drainage reservoir: %s\n",
        (SW_SiteIn->deepdrain) ? "swTRUE" : "swFALSE"
    );
    sw_printf("  Slow Drain Coefficient: %5.4f\n", SW_SiteIn->slow_drain_coeff);
    sw_printf("  PET Scale: %5.4f\n", SW_SiteIn->pet_scale);
    sw_printf(
        "  Runoff: proportion of surface water lost: %5.4f\n",
        SW_SiteIn->percentRunoff
    );
    sw_printf(
        "  Runon: proportion of new surface water gained: %5.4f\n",
        SW_SiteIn->percentRunon
    );
    sw_printf(
        "  Longitude (degree): %4.2f\n", ModelRunIn->longitude * rad_to_deg
    );
    sw_printf(
        "  Latitude (degree): %4.2f\n", ModelRunIn->latitude * rad_to_deg
    );
    sw_printf("  Altitude (m a.s.l.): %4.2f \n", ModelRunIn->elevation);
    sw_printf("  Slope (degree): %4.2f\n", ModelRunIn->slope * rad_to_deg);
    sw_printf("  Aspect (degree): %4.2f\n", ModelRunIn->aspect * rad_to_deg);

    sw_printf(
        "\nSnow simulation parameters (SWAT2K model):\n----------------------\n"
    );
    sw_printf(
        "  Avg. air temp below which ppt is snow ( C): %5.4f\n",
        SW_SiteIn->TminAccu2
    );
    sw_printf(
        "  Snow temperature at which snow melt starts ( C): %5.4f\n",
        SW_SiteIn->TmaxCrit
    );
    sw_printf(
        "  Relative contribution of avg. air temperature to todays snow "
        "temperture vs. yesterday's snow temperature (0-1): %5.4f\n",
        SW_SiteIn->lambdasnow
    );
    sw_printf(
        "  Minimum snow melt rate on winter solstice (cm/day/C): %5.4f\n",
        SW_SiteIn->RmeltMin
    );
    sw_printf(
        "  Maximum snow melt rate on summer solstice (cm/day/C): %5.4f\n",
        SW_SiteIn->RmeltMax
    );

    sw_printf("\nSoil Temperature Constants:\n----------------------\n");
    sw_printf("  Biomass Limiter constant: %5.4f\n", SW_SiteIn->bmLimiter);
    sw_printf("  T1Param1: %5.4f\n", SW_SiteIn->t1Param1);
    sw_printf("  T1Param2: %5.4f\n", SW_SiteIn->t1Param2);
    sw_printf("  T1Param3: %5.4f\n", SW_SiteIn->t1Param3);
    sw_printf("  csParam1: %5.4f\n", SW_SiteIn->csParam1);
    sw_printf("  csParam2: %5.4f\n", SW_SiteIn->csParam2);
    sw_printf("  shParam: %5.4f\n", SW_SiteIn->shParam);
    sw_printf("  Tsoil_constant: %5.4f\n", Tsoil_constant);
    sw_printf("  deltaX: %5.4f\n", SW_SiteIn->stDeltaX);
    sw_printf("  max depth: %5.4f\n", SW_SiteIn->stMaxDepth);
    sw_printf(
        "  Make soil temperature calculations: %s\n",
        (SW_SiteIn->use_soil_temp) ? "swTRUE" : "swFALSE"
    );
    sw_printf(
        "  Number of regressions for the soil temperature function: %d\n",
        SW_SiteSim->stNRGR
    );

    sw_printf("\nLayer Related Values:\n----------------------\n");
    sw_printf("  Soils File: 'soils.in'\n");
    sw_printf("  Number of soil layers: %d\n", n_layers);
    sw_printf("  Number of evaporation layers: %d\n", SW_SiteSim->n_evap_lyrs);

    ForEachVegType(k) {
        sw_printf(
            "  Number of %s transpiration layers: %d\n",
            key2veg[k],
            SW_SiteSim->n_transp_lyrs[k]
        );
    }
    sw_printf(
        "  Number of transpiration regions: %d\n", SW_SiteSim->n_transp_rgn
    );

    sw_printf("\nLayer Specific Values:\n----------------------\n");
    sw_printf("\n  Layer information on a per centimeter depth basis:\n");
    sw_printf("  Lyr  Width   BulkD   %%Gravel   %%Sand   %%Clay"
              "   %%OM   evCo   Imperm");
    sw_printf("   Sat   FieldC   WiltPt   Wet   Min   Init");
    ForEachVegType(k) { sw_printf("   VWC at SWPcrit(%s)", key2veg[k]); }
    ForEachVegType(k) { sw_printf("   trCo(%s)", key2veg[k]); }
    ForEachVegType(k) { sw_printf("   trRgn(%s)", key2veg[k]); }
    sw_printf("\n");

    ForEachSoilLayer(i, n_layers) {
        sw_printf(
            "  %3d %6.2f %6.2f %6.2f %6.2f %6.2f %6.2f %6.2f %6.2f",
            i + 1,
            SW_SoilRunIn->width[i],
            SW_SiteSim->soilBulk_density[i],
            SW_SoilRunIn->fractionVolBulk_gravel[i],
            SW_SoilRunIn->fractionWeightMatric_sand[i],
            SW_SoilRunIn->fractionWeightMatric_clay[i],
            SW_SoilRunIn->fractionWeight_om[i],
            SW_SoilRunIn->evap_coeff[i],
            SW_SoilRunIn->impermeability[i]
        );
        sw_printf(
            " %6.2f %6.2f %6.2f %6.2f %6.2f %6.2f",
            SW_SiteSim->swcBulk_saturated[i] / SW_SoilRunIn->width[i],
            SW_SiteSim->swcBulk_fieldcap[i] / SW_SoilRunIn->width[i],
            SW_SiteSim->swcBulk_wiltpt[i] / SW_SoilRunIn->width[i],
            SW_SiteSim->swcBulk_wet[i] / SW_SoilRunIn->width[i],
            SW_SiteSim->swcBulk_min[i] / SW_SoilRunIn->width[i],
            SW_SiteSim->swcBulk_init[i] / SW_SoilRunIn->width[i]
        );
        ForEachVegType(k) {
            sw_printf(
                " %6.2f",
                SW_SiteSim->swcBulk_atSWPcrit[i][k] / SW_SoilRunIn->width[i]
            );
        }
        ForEachVegType(k) {
            sw_printf(" %6.2f", SW_SoilRunIn->transp_coeff[k][i]);
        }
        ForEachVegType(k) {
            sw_printf(" %6u", SW_SiteSim->my_transp_rgn[k][i]);
        }
        sw_printf("\n");
    }

    sw_printf("\n\nSoil Water Retention Curve:\n---------------------------\n");
    sw_printf(
        "  SWRC type: %d (%s)\n",
        SW_SiteIn->site_swrc_type,
        SW_SiteIn->site_swrc_name
    );
    sw_printf(
        "  PTF type: %d (%s)\n",
        SW_SiteIn->site_ptf_type,
        SW_SiteIn->site_ptf_name
    );

    sw_printf(
        "  Lyr     Param1     Param2     Param3     Param4     Param5     "
        "Param6\n"
    );
    ForEachSoilLayer(i, n_layers) {
        sw_printf(
            "  %3d%11.4f%11.4f%11.4f%11.4f%11.4f%11.4f\n",
            i + 1,
            SW_SiteSim->swrcp[i][0],
            SW_SiteSim->swrcp[i][1],
            SW_SiteSim->swrcp[i][2],
            SW_SiteSim->swrcp[i][3],
            SW_SiteSim->swrcp[i][4],
            SW_SiteSim->swrcp[i][5]
        );
    }


    sw_printf("\n------------ End of Site Parameters ------------------\n");
}
