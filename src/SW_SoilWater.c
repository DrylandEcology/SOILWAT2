/********************************************************/
/********************************************************/
/*	Source file: SoilWater.c
 Type: module
 Application: SOILWAT - soilwater dynamics simulator
 Purpose: Read / write and otherwise manage the
 soil water values.  Includes reading input
 parameters and ordinary daily water flow.
 In addition, generally useful soilwater-
 related functions should go here.

 History:

 (8/28/01) -- INITIAL CODING - cwb

 10/04/2010	(drs) added snowMAUS snow accumulation, sublimation and melt
 algorithm: Trnka, M., Kocmánková, E., Balek, J., Eitzinger, J., Ruget, F.,
 Formayer, H., Hlavinka, P., Schaumberger, A., Horáková, V., Mozny, M. & Zalud,
 Z. (2010) Simple snow cover model for agrometeorological applications.
 Agricultural and Forest Meteorology, 150, 1115-1127. replaced
 SW_SWC_snow_accumulation, SW_SWC_snow_sublimation, and SW_SWC_snow_melt with
 SW_SWC_adjust_snow (temp_min, temp_max, *ppt, *rain, *snow, *snowmelt)

 10/19/2010	(drs) replaced snowMAUS simulation with SWAT2K routines: Neitsch
 S, Arnold J, Kiniry J, Williams J. 2005. Soil and water assessment tool (SWAT)
 theoretical documentation. version 2005. Blackland Research Center, Texas
 Agricultural Experiment Station: Temple, TX.

 10/25/2010	(drs)	in SW_SWC_water_flow(): replaced test that "swc can't be
 adjusted on day 1 of year 1" to "swc can't be adjusted on start day of first
 year of simulation"

 01/04/2011	(drs) added parameter '*snowloss' to function
 SW_SWC_adjust_snow()

 08/22/2011	(drs) added function RealD SW_SnowDepth( RealD SWE, RealD
 snowdensity)

 01/20/2012	(drs)	in function 'SW_SnowDepth': catching division by 0 if
 snowdensity is 0

 02/03/2012	(drs)	added function 'RealD SW_SWC_SWCres(RealD sand, RealD
 clay, RealD porosity)': which calculates 'Brooks-Corey' residual volumetric
 soil water based on Rawls & Brakensiek (1985)

 05/25/2012  (DLM) edited SW_SWC_read(void) function to get the initial values
 for soil temperature from SW_Site

 04/16/2013	(clk)	Changed SW_SWC_vol2bars() to SW_SWCbulk2SWPmatric(), and
 changed the code to incorporate the fraction of gravel content in the soil
 theta1 = ... / (1. - fractionVolBulk_gravel)
 Changed SW_SWC_bars2vol() to SW_SWPmatric2VWCBulk(), and changed the code to
 incorporate the fraction of gravel content in the soil t = ... * (1. -
 fractionVolBulk_gravel) Changed SW_SWC_SWCres() to SW_VWCBulkRes(), and changed
 the code to incorporate the fraction of gravel content in the soil res = ... *
 (1. - fractionVolBulk_gravel) Updated the use of these three function in all
 files

 06/24/2013	(rjm)	made temp_snow a module-level static variable (instead
 of function-level): otherwise it will not get reset to 0 between consecutive
 calls as a dynamic library need to set temp_snow to 0 in function
 SW_SWC_construct()

 06/26/2013	(rjm)	closed open files at end of functions SW_SWC_read(),
 _read_hist() or if LogError() with LOGERROR is called
 */
/********************************************************/
/********************************************************/

/* =================================================== */
/* =================================================== */
/*                INCLUDES / DEFINES                   */

#include "include/SW_SoilWater.h"   // for FXW_h0, FXW_hr, SWRC_SWCtoSWP
#include "include/filefuncs.h"      // for LogError, CloseFile, GetALine
#include "include/generic.h"        // for LOGERROR, GT, LT, fmax
#include "include/myMemory.h"       // for Mem_Calloc, Str_Dup
#include "include/SW_datastructs.h" // for LOG_INFO, SW_SOILWAT, SW_SITE
#include "include/SW_Defines.h"     // for NVEGTYPES, LyrIndex, MAX_LAYERS
#include "include/SW_Files.h"       // for eSoilwat
#include "include/SW_Flow.h"        // for SW_Water_Flow
#include "include/SW_Site.h"        // for sw_Campbell1974, sw_FXW, sw_vanG...
#include "include/SW_Times.h"       // for Today, Yesterday
#include "include/Times.h"          // for yearto4digit
#include <math.h>                   // for fabs, pow, log, ceil, copysign
#include <stdio.h>                  // for NULL, FILE, sscanf, snprintf
#include <stdlib.h>                 // for free
#include <string.h>                 // for memset


#ifdef RSOILWAT
// SW_SWC_new_year() is currently not functional in rSOILWAT2
// #include "rSW_SoilWater.h" // for onSet_SW_SWC_hist() via SW_SWC_new_year()
#endif

/* =================================================== */
/*             Local Function Definitions              */
/* --------------------------------------------------- */

static void clear_hist(
    double SoilWat_hist_swc[][MAX_LAYERS],
    double SoilWat_hist_std_err[][MAX_LAYERS]
) {
    TimeInt d;
    LyrIndex z;
    for (d = 0; d < MAX_DAYS; d++) {
        for (z = 0; z < MAX_LAYERS; z++) {
            SoilWat_hist_swc[d][z] = SW_MISSING;
            SoilWat_hist_std_err[d][z] = SW_MISSING;
        }
    }
}

static void reset_swc(SW_SOILWAT *SW_SoilWat, SW_SITE *SW_Site) {
    LyrIndex lyr;

    /* reset swc */
    ForEachSoilLayer(lyr, SW_Site->n_layers) {
        SW_SoilWat->swcBulk[Today][lyr] = SW_Site->swcBulk_init[lyr];
        SW_SoilWat->swcBulk[Yesterday][lyr] = SW_Site->swcBulk_init[lyr];
        SW_SoilWat->drain[lyr] = 0.; // deepest percolation is deep drainage
    }

    /* reset the snowpack */
    SW_SoilWat->snowpack[Today] = SW_SoilWat->snowpack[Yesterday] = 0.;
}

/**
@brief Convert soil water tension to volumetric water content using
FXW Soil Water Retention Curve
\cite fredlund1994CGJa, \cite wang2018wrr

@note
This is the internal, bare-bones version of FXW,
use `SWRC_SWPtoSWC_FXW()` instead.

@param[in] phi Pressure head (water tension) [cm of H20]
@param[in] *swrcp Vector of SWRC parameters

@return Volumetric soil water content of the matric soil [cm / cm]
*/
static double FXW_phi_to_theta(double phi, double *swrcp) {
    double tmp;
    double res;
    double S_e;
    double C_f;

    if (GE(phi, FXW_h0)) {
        res = 0.;

    } else {
        // Relative saturation function: Rudiyanto et al. 2021: eq. 3
        tmp = pow(fabs(swrcp[1] * phi), swrcp[2]); // `pow()` because phi >= 0
        S_e = powe(log(swE + tmp), -swrcp[3]);

        // Correction factor: Rudiyanto et al. 2021: eq. 4 (with hr = 1500 cm)
        // log(1. + 6.3e6 / 1500.) = 8.34307787116938;
        C_f = 1. - log(1. + phi / FXW_hr) / 8.34307787116938;

        // Calculate matric theta [cm / cm] which is within [0, theta_sat]
        // Rudiyanto et al. 2021: eq. 1
        res = swrcp[0] * S_e * C_f;
    }

    // matric theta [cm / cm]
    return res;
}

/**
@brief Interpolate, Truncate, Project (ITP) method (\cite oliveira2021ATMS) to
solve FXW at a given water content

We estimate `phi` by finding a root of
    `f(phi) = theta - FXW(phi) = 0`
with
    `a < b && f(a) < 0 && f(b) > 0`

FXW SWRC (\cite fredlund1994CGJa, \cite wang2018wrr) is defined as
`theta = theta_sat * S_e(phi) * C_f(phi)`

where
    - `phi` is water tension (pressure head) [cm of H20],
    - `theta` is volumetric water content [cm / cm],
    - `theta_sat` is saturated water content [cm / cm],
    - `S_e` is the relative saturation function [-], and
    - `C_f` is a correction factor [-].

We choose starting values to solve `f(phi)`
    - `a = 0` for which `f(a) = theta - theta_sat < 0 (for theta < theta_sat)`
    - `b = FXW_h0` for which `f(b) = theta - 0 > 0 (for theta > 0)`

[Additional info on the ITP method can be found
here](https://en.wikipedia.org/wiki/ITP_method)


@param[in] theta Volumetric soil water content of the matric soil [cm / cm]
@param[in] *swrcp Vector of FXW SWRC parameters
@param[in] *LogInfo Holds information on warnings and errors

\return Water tension [bar] corresponding to theta
*/

static double itp_FXW_for_phi(double theta, double *swrcp, LOG_INFO *LogInfo) {
#ifdef SWDEBUG
    short debug = 0;
#endif

    // tol2: tolerance convergence
    double tol2 = 2e-9;
    // a: lower bound of bracket: a < b && f(a) < 0
    double a = 0.;
    // b: upper bound of bracket: b > a && f(b) > 0
    double b = FXW_h0;
    double diff_ba = FXW_h0;
    double diff_hf;
    double x_f;
    double x_t;
    double x_itp;
    double y_a;
    double y_b;
    double y_itp;
    double k1;
    double k2;
    double x_half;
    double r;
    double delta;
    double sigma;
    double phi;
    int j = 0;
    int n0;
    int n_max;


    // Evaluate at starting bracket (and checks)
    y_a = theta - FXW_phi_to_theta(a, swrcp);
    y_b = theta - FXW_phi_to_theta(b, swrcp);


    // Set hyper-parameters
    // k1 = 3.174603e-08; // 0 < k1 = 0.2 / (b - a) < inf
    /*
    k1 = 2. converges in about 31-33 iterations
    k1 = 0.2 converges in 28-30 iterations
    k1 = 2.e-2 converges in 25-27 iterations
    k1 = 2.e-3 converges in 22-24 iterations
    k1 = 2.e-4 converges in about 20 iterations but fails for some
    k1 = 2.e-6 converges in about 14 iterations or near n_max or fails
    k1 = 0.2 / (b - a) = 3.174603e-08 (suggested value) converges
                in about 8 iterations (for very dry soils) or
                near n_max = 53 or fails
    */
    k1 = 2.e-3;
    k2 = 2.; // 1 <= k2 < (3 + sqrt(5))/2
    n0 = 1;  // 0 < n0 < inf

    // Upper limit of iterations before convergence
    n_max = (int) ceil(log2(diff_ba / tol2)) + n0;

#ifdef SWDEBUG
    if (debug) {
        sw_printf(
            "\nitp_FXW_for_phi(theta=%f): init: "
            "f(a)=%f, f(b)=%f, n_max=%d, k1=%f\n",
            theta,
            y_a,
            y_b,
            n_max,
            k1
        );
    }
#endif


    // Iteration
    do {
        x_half = (a + b) / 2.;

        // Calculate parameters
        r = (tol2 * exp2(n_max - j) - diff_ba) / 2.;
        delta = k1 * pow(diff_ba, k2);

        // Interpolation
        x_f = (y_b * a - y_a * b) / (y_b - y_a);

        // Truncation
        diff_hf = x_half - x_f;
        if (ZRO(diff_hf)) {
            sigma = 0.; // `copysign()` returns -1 or +1 but never 0
        } else {
            sigma = copysign(1., diff_hf);
        }

        x_t = (delta <= fabs(diff_hf)) ? (x_f + sigma * delta) : x_half;

        // Projection
        x_itp = (fabs(x_t - x_half) <= r) ? x_t : (x_half - sigma * r);

        // Evaluate function
        y_itp = theta - FXW_phi_to_theta(x_itp, swrcp);

#ifdef SWDEBUG
        if (debug) {
            sw_printf(
                "j=%d:"
                "\ta=%f, b=%f, y_a=%f, y_b=%f,\n"
                "\tr=%f, delta=%f, x_half=%f, x_f=%f, x_t=%f (s=%.0f),\n"
                "\tx_itp=%f, y_itp=%f\n",
                j,
                a,
                b,
                y_a,
                y_b,
                r,
                delta,
                x_half,
                x_f,
                x_t,
                sigma,
                x_itp,
                y_itp
            );
        }
#endif

        // Update interval brackets
        if (GT(y_itp, 0.)) {
            b = x_itp;
            y_b = y_itp;
            diff_ba = b - a;
        } else if (LT(y_itp, 0.)) {
            a = x_itp;
            y_a = y_itp;
            diff_ba = b - a;
        } else {
            a = x_itp;
            b = x_itp;
            diff_ba = 0.;
        }

        j++;
    } while (diff_ba > tol2 && j <= n_max);

    phi = (a + b) / 2.;

    if (j > n_max + 1 || fabs(diff_ba) > tol2 || LT(phi, 0.) ||
        GT(phi, FXW_h0)) {
        // Error if not converged or if `phi` physically impossible
        LogError(
            LogInfo,
            LOGERROR,
            "itp_FXW_for_phi(theta = %f): convergence failed at phi = %.9f [cm "
            "H2O] "
            "after %d iterations with b - a = %.9f - %.9f = %.9f !<= tol2 = "
            "%.9f.",
            theta,
            phi,
            j,
            b,
            a,
            b - a,
            tol2
        );

        phi = SW_MISSING;

    } else {
        // convert [cm of H2O at 4 C] to [bar]
        phi /= 1019.716;
    }


#ifdef SWDEBUG
    if (debug) {
        if (!missing(phi)) {
            sw_printf(
                "itp_FXW_for_phi() = phi = %f [bar]: converged in j=%d/%d "
                "steps\n",
                phi,
                j,
                n_max
            );
        } else {
            sw_printf(
                "itp_FXW_for_phi(): failed to converged in j=%d/%d steps\n",
                j,
                n_max
            );
        }
    }
#endif

    return phi;
}

/* =================================================== */
/*             Global Function Definitions             */
/* --------------------------------------------------- */

#ifdef SWDEBUG
void SW_WaterBalance_Checks(SW_RUN *sw, LOG_INFO *LogInfo) {

    LyrIndex i;
    IntUS k;
    // debugging with 'debugi': turn messages on/off for each check separately
    int debugi[N_WBCHECKS] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
    int snowChecks[3];
    char flag[16];
    double Etotal;
    double Etotalsurf;
    double Etotalint;
    double Eponded;
    double Elitter;
    double Esnow;
    double Esoil = 0.;
    double Eveg = 0.;
    double Ttotal = 0.;
    double Ttotalj[MAX_LAYERS];
    double percolationIn[MAX_LAYERS + 1];
    double percolationOut[MAX_LAYERS + 1];
    double hydraulicRedistribution[MAX_LAYERS];
    double infiltration;
    double deepDrainage;
    double runoff;
    double runon;
    double snowfall;
    double snowmelt;
    double snowpackWater;
    double snowpackRecently;
    double snowpackDepth;
    double rain;
    double arriving_water;
    double intercepted;
    double int_veg_total = 0.;
    double delta_surfaceWater;
    double delta_swc_total = 0.;
    double delta_swcj[MAX_LAYERS];
    double lhs;
    double rhs;
    double wbtol = 1e-9;

    static double surfaceWater_yesterday;
    static Bool debug = swFALSE;
    LyrIndex n_layers = sw->Site.n_layers;


    // re-init static variable on first day of each simulation run (if no
    // spinup) or on first day of spinup
    if (!sw->SoilWat.is_wbError_init) {
        surfaceWater_yesterday = 0.;
    }

    // Sum up variables
    ForEachSoilLayer(i, n_layers) {
        percolationIn[i + 1] = sw->SoilWat.drain[i];
        percolationOut[i] = sw->SoilWat.drain[i];

        delta_swcj[i] =
            sw->SoilWat.swcBulk[Today][i] - sw->SoilWat.swcBulk[Yesterday][i];
        delta_swc_total += delta_swcj[i];

        Ttotalj[i] = hydraulicRedistribution[i] = 0.; // init

        ForEachVegType(k) {
            Ttotal += sw->SoilWat.transpiration[k][i];
            Ttotalj[i] += sw->SoilWat.transpiration[k][i];
            hydraulicRedistribution[i] += sw->SoilWat.hydred[k][i];
        }
    }

    ForEachEvapLayer(i, sw->Site.n_evap_lyrs) {
        Esoil += sw->SoilWat.evap_baresoil[i];
    }

    ForEachVegType(k) {
        Eveg += sw->SoilWat.evap_veg[k];
        int_veg_total += sw->SoilWat.int_veg[k];
    }

    // Get evaporation values
    Elitter = sw->SoilWat.litter_evap;
    Eponded = sw->SoilWat.surfaceWater_evap;
    Esnow = sw->Weather.snowloss;
    Etotalint = Eveg + Elitter;
    Etotalsurf = Etotalint + Eponded;
    Etotal = Etotalsurf + Esoil + Esnow;

    // Get other water flux values
    infiltration = sw->Weather.soil_inf;
    deepDrainage = sw->SoilWat.drain[sw->Site.deep_lyr];

    percolationIn[0] = infiltration;
    percolationOut[sw->Site.n_layers] = deepDrainage;

    runoff = sw->Weather.snowRunoff + sw->Weather.surfaceRunoff;
    runon = sw->Weather.surfaceRunon;
    snowmelt = sw->Weather.snowmelt;
    rain = sw->Weather.now.rain;

    arriving_water = rain + snowmelt + runon;

    // Get snow
    snowfall = sw->Weather.snow;
    snowpackDepth = sw->SoilWat.snowdepth;
    snowpackWater = sw->SoilWat.snowpack[Today];
    snowpackRecently =
        sw->SoilWat.snowpack[Today] + sw->SoilWat.snowpack[Yesterday];

    // Get state change values
    intercepted = sw->SoilWat.litter_int + int_veg_total;

    delta_surfaceWater = sw->SoilWat.surfaceWater - surfaceWater_yesterday;
    surfaceWater_yesterday = sw->SoilWat.surfaceWater;


    //--- Water balance checks (there are # checks n = N_WBCHECKS)
    if (!sw->SoilWat.is_wbError_init) {
        for (i = 0; i < N_WBCHECKS; i++) {
            debug = (debug || debugi[i]) ? swTRUE : swFALSE;
        }
    }

    if (debug) {
        (void) snprintf(
            flag, sizeof flag, "WB (%d-%d)", sw->Model.year, sw->Model.doy
        );
    }


    // AET <= PET
    if (!sw->SoilWat.is_wbError_init) {
        sw->SoilWat.wbErrorNames[0] = Str_Dup("AET <= PET", LogInfo);

        if (LogInfo->stopRun) {
            return; // Exit function prematurely due to error
        }
    }
    if (!LE(sw->SoilWat.aet, sw->SoilWat.pet)) {
        sw->SoilWat.wbError[0]++;
        if (debugi[0]) {
            sw_printf(
                "%s: aet=%f, pet=%f\n", flag, sw->SoilWat.aet, sw->SoilWat.pet
            );
        }
    }

    // AET == E(total) + T(total)
    if (!sw->SoilWat.is_wbError_init) {
        sw->SoilWat.wbErrorNames[1] =
            Str_Dup("AET == Etotal + Ttotal", LogInfo);

        if (LogInfo->stopRun) {
            return; // Exit function prematurely due to error
        }
    }
    rhs = Etotal + Ttotal;
    if (!EQ_w_tol(sw->SoilWat.aet, rhs, wbtol)) {
        sw->SoilWat.wbError[1]++;
        if (debugi[1]) {
            sw_printf(
                "%s: AET(%f) == %f == Etotal(%f) + Ttotal(%f)\n",
                flag,
                sw->SoilWat.aet,
                rhs,
                Etotal,
                Ttotal
            );
        }
    }

    // T(total) = sum of T(veg-type i from soil layer j)
    // doesn't make sense here because Ttotal is the sum of Tvegij
    if (!sw->SoilWat.is_wbError_init) {
        sw->SoilWat.wbErrorNames[2] = Str_Dup(
            "T(total) = sum of T(veg-type i from soil layer j)", LogInfo
        );

        if (LogInfo->stopRun) {
            return; // Exit function prematurely due to error
        }
    }
    sw->SoilWat.wbError[2] += 0;

    // E(total) = E(total bare-soil) + E(ponded water) + E(total
    // litter-intercepted) +
    //            + E(total veg-intercepted) + E(snow sublimation)
    if (!sw->SoilWat.is_wbError_init) {
        sw->SoilWat.wbErrorNames[3] = Str_Dup(
            "Etotal == Esoil + Eponded + Eveg + Elitter + Esnow", LogInfo
        );
        if (LogInfo->stopRun) {
            return; // Exit function prematurely due to error
        }
    }
    rhs = Esoil + Eponded + Eveg + Elitter + Esnow;
    if (!EQ_w_tol(Etotal, rhs, wbtol)) {
        sw->SoilWat.wbError[3]++;
        if (debugi[3]) {
            sw_printf(
                "%s: Etotal(%f) == %f == Esoil(%f) + Eponded(%f) + Eveg(%f) + "
                "Elitter(%f) + Esnow(%f)\n",
                flag,
                Etotal,
                rhs,
                Esoil,
                Eponded,
                Eveg,
                Elitter,
                Esnow
            );
        }
    }

    // E(total surface) = E(ponded water) + E(total litter-intercepted) +
    //                    + E(total veg-intercepted)
    if (!sw->SoilWat.is_wbError_init) {
        sw->SoilWat.wbErrorNames[4] =
            Str_Dup("Esurf == Eponded + Eveg + Elitter", LogInfo);

        if (LogInfo->stopRun) {
            return; // Exit function prematurely due to error
        }
    }
    rhs = Eponded + Eveg + Elitter;
    if (!EQ_w_tol(Etotalsurf, rhs, wbtol)) {
        sw->SoilWat.wbError[4]++;
        if (debugi[4]) {
            sw_printf(
                "%s: Esurf(%f) == %f == Eponded(%f) + Eveg(%f) + Elitter(%f)\n",
                flag,
                Etotalsurf,
                rhs,
                Eponded,
                Eveg,
                Elitter
            );
        }
    }


    //--- Water cycling checks
    // infiltration = [rain + snowmelt + runon] - (runoff + intercepted +
    // delta_surfaceWater + Eponded)
    if (!sw->SoilWat.is_wbError_init) {
        sw->SoilWat.wbErrorNames[5] = Str_Dup(
            "inf == rain + snowmelt + runon - (runoff + intercepted + "
            "delta_surfaceWater + Eponded)",
            LogInfo
        );
        if (LogInfo->stopRun) {
            return; // Exit function prematurely due to error
        }
    }
    rhs =
        arriving_water - (runoff + intercepted + delta_surfaceWater + Eponded);
    if (!EQ_w_tol(infiltration, rhs, wbtol)) {
        sw->SoilWat.wbError[5]++;
        if (debugi[5]) {
            sw_printf(
                "%s: inf(%f) == %f == rain(%f) + snowmelt(%f) + runon(%f) - "
                "(runoff(%f) + intercepted(%f) + delta_surfaceWater(%f) + "
                "Eponded(%f))\n",
                flag,
                infiltration,
                rhs,
                rain,
                snowmelt,
                runon,
                runoff,
                intercepted,
                delta_surfaceWater,
                Eponded
            );
        }
    }

    // E(soil) + Ttotal = infiltration - (deepDrainage + delta(swc))
    if (!sw->SoilWat.is_wbError_init) {
        sw->SoilWat.wbErrorNames[6] = Str_Dup(
            "Ttotal + Esoil = inf - (deepDrainage + delta_swc)", LogInfo
        );

        if (LogInfo->stopRun) {
            return; // Exit function prematurely due to error
        }
    }
    lhs = Ttotal + Esoil;
    rhs = infiltration - (deepDrainage + delta_swc_total);
    if (!EQ_w_tol(lhs, rhs, wbtol)) {
        sw->SoilWat.wbError[6]++;
        if (debugi[6]) {
            sw_printf(
                "%s: Ttotal(%f) + Esoil(%f) == %f == %f == inf(%f) - "
                "(deepDrainage(%f) + delta_swc(%f))\n",
                flag,
                Ttotal,
                Esoil,
                lhs,
                rhs,
                infiltration,
                deepDrainage,
                delta_swc_total
            );
        }
    }

    // for every soil layer j: delta(swc) =
    //   = infiltration/percolationIn + hydraulicRedistribution -
    //     (percolationOut/deepDrainage + transpiration + baresoil_evaporation)
    if (!sw->SoilWat.is_wbError_init) {
        sw->SoilWat.wbErrorNames[7] = Str_Dup(
            "delta_swc[i] == perc_in[i] + hydred[i] - (perc_out[i] + Ttot[i] + "
            "Esoil[i]))",
            LogInfo
        );

        if (LogInfo->stopRun) {
            return; // Exit function prematurely due to error
        }
    }
    ForEachSoilLayer(i, n_layers) {
        rhs = percolationIn[i] + hydraulicRedistribution[i] -
              (percolationOut[i] + Ttotalj[i] + sw->SoilWat.evap_baresoil[i]);
        if (!EQ_w_tol(delta_swcj[i], rhs, wbtol)) {
            sw->SoilWat.wbError[7]++;
            if (debugi[7]) {
                sw_printf(
                    "%s sl=%d: delta_swc(%f) == %f == perc_in(%f) + hydred(%f) "
                    "- (perc_out(%f) + Ttot(%f) + Esoil(%f))\n",
                    flag,
                    i,
                    delta_swcj[i],
                    rhs,
                    percolationIn[i],
                    hydraulicRedistribution[i],
                    percolationOut[i],
                    Ttotalj[i],
                    sw->SoilWat.evap_baresoil[i]
                );
            }
        }
    }

    // for every soil layer j: swc_min <= swc <= swc_sat
    if (!sw->SoilWat.is_wbError_init) {
        sw->SoilWat.wbErrorNames[8] =
            Str_Dup("swc[i] => swc_min[i] && swc[i] <= swc_sat[i]", LogInfo);

        if (LogInfo->stopRun) {
            return; // Exit function prematurely due to error
        }
    }
    ForEachSoilLayer(i, n_layers) {
        if (LT(sw->SoilWat.swcBulk[Today][i], sw->Site.swcBulk_min[i]) ||
            GT(sw->SoilWat.swcBulk[Today][i], sw->Site.swcBulk_saturated[i])) {
            sw->SoilWat.wbError[8]++;
            if (debugi[8]) {
                sw_printf(
                    "%s sl=%d: swc_min(%f) <= swc(%f) <= swc_sat(%f)\n",
                    flag,
                    i,
                    sw->Site.swcBulk_min[i],
                    sw->SoilWat.swcBulk[Today][i],
                    sw->Site.swcBulk_saturated[i]
                );
            }
        }
    }

    // Consistency between snowpack, snowdepth, snowmelt, snowloss
    if (!sw->SoilWat.is_wbError_init) {
        sw->SoilWat.wbErrorNames[9] = Str_Dup("Snow consistency", LogInfo);

        if (LogInfo->stopRun) {
            return; // Exit function prematurely due to error
        }
    }

    snowChecks[0] = (ZRO(snowpackWater) && ZRO(snowpackDepth)) ||
                    (snowpackWater > 0 && snowpackDepth > 0);
    snowChecks[1] = ZRO(snowmelt) ||
                    (snowmelt > 0 && (snowpackRecently > 0 || snowfall > 0));
    snowChecks[2] =
        ZRO(Esnow) || (Esnow > 0 && (snowpackRecently > 0 || snowfall > 0));

    if (!snowChecks[0] || !snowChecks[1] || !snowChecks[2]) {
        sw->SoilWat.wbError[9]++;
        if (debugi[9]) {
            if (!snowChecks[0]) {
                sw_printf(
                    "%s: snowpack = %f, snowdepth = %f\n",
                    flag,
                    snowpackWater,
                    snowpackDepth
                );
            }
            if (!snowChecks[1]) {
                sw_printf(
                    "%s: snowmelt = %f, snowpack = %f|%f, snowfall = %f\n",
                    flag,
                    snowmelt,
                    snowpackWater,
                    snowpackRecently,
                    snowfall
                );
            }
            if (!snowChecks[2]) {
                sw_printf(
                    "%s: Esnow = %f, snowpack = %f|%f, snowfall = %f\n",
                    flag,
                    Esnow,
                    snowpackWater,
                    snowpackRecently,
                    snowfall
                );
            }
        }
    }

    // Setup only once
    if (!sw->SoilWat.is_wbError_init) {
        sw->SoilWat.is_wbError_init = swTRUE;
    }
}
#endif

/**
@brief Initialize all possible pointers in SW_SOILWAT to NULL

@param[in,out] SW_SoilWat Struct of type SW_SOILWAT containing
    soil water related values
*/
void SW_SWC_init_ptrs(SW_SOILWAT *SW_SoilWat) {
    OutPeriod pd;

    // Initialize output structures:
    ForEachOutPeriod(pd) {
        SW_SoilWat->p_accu[pd] = NULL;
        SW_SoilWat->p_oagg[pd] = NULL;
    }

    SW_SoilWat->hist.file_prefix = NULL;

#ifdef SWDEBUG
    IntU i;

    for (i = 0; i < N_WBCHECKS; i++) {
        SW_SoilWat->wbErrorNames[i] = NULL;
    }
#endif
}

/**
@brief Constructor for soilWater Content.

@param[in,out] SW_SoilWat Struct of type SW_SOILWAT containing
    soil water related values
*/
void SW_SWC_construct(SW_SOILWAT *SW_SoilWat) {
    /* =================================================== */

    /* initialize pointer */
    SW_SoilWat->hist.file_prefix = NULL;


    // Clear the module structure:
    memset(SW_SoilWat, 0, sizeof(SW_SOILWAT));
}

/**
@brief Allocate dynamic memory for output pointers in SW_SOILWAT struct

@param[out] SW_SoilWat SW_SoilWat Struct of type SW_SOILWAT containing
    soil water related values
@param[out] LogInfo Holds information on warnings and errors
*/
void SW_SWC_alloc_outptrs(SW_SOILWAT *SW_SoilWat, LOG_INFO *LogInfo) {
    OutPeriod pd;

    // Allocate output structures:
    ForEachOutPeriod(pd) {
        SW_SoilWat->p_accu[pd] = (SW_SOILWAT_OUTPUTS *) Mem_Calloc(
            1, sizeof(SW_SOILWAT_OUTPUTS), "SW_SWC_alloc_outptrs()", LogInfo
        );
        if (LogInfo->stopRun) {
            return; // Exit function prematurely due to error
        }

        if (pd > eSW_Day) {
            SW_SoilWat->p_oagg[pd] = (SW_SOILWAT_OUTPUTS *) Mem_Calloc(
                1, sizeof(SW_SOILWAT_OUTPUTS), "SW_SWC_alloc_outptrs()", LogInfo
            );

            if (LogInfo->stopRun) {
                return; // Exit function prematurely due to error
            }
        }
    }
}

void SW_SWC_deconstruct(SW_SOILWAT *SW_SoilWat) {
    OutPeriod pd;

    // De-allocate output structures:
    ForEachOutPeriod(pd) {
        if (pd > eSW_Day && !isnull(SW_SoilWat->p_oagg[pd])) {
            free(SW_SoilWat->p_oagg[pd]);
            SW_SoilWat->p_oagg[pd] = NULL;
        }

        if (!isnull(SW_SoilWat->p_accu[pd])) {
            free(SW_SoilWat->p_accu[pd]);
            SW_SoilWat->p_accu[pd] = NULL;
        }
    }

    if (!isnull(SW_SoilWat->hist.file_prefix)) {
        free(SW_SoilWat->hist.file_prefix);
        SW_SoilWat->hist.file_prefix = NULL;
    }

#ifdef SWDEBUG
    IntU i;

    for (i = 0; i < N_WBCHECKS; i++) {
        if (!isnull(SW_SoilWat->wbErrorNames[i])) {
            free(SW_SoilWat->wbErrorNames[i]);
            SW_SoilWat->wbErrorNames[i] = NULL;
        }
    }
#endif
}

/**
@brief Adjust SWC according to historical (measured) data if available, compute
water flow, and check if swc is above threshold for "wet" condition.

@param[in,out] sw Comprehensive struct of type SW_RUN containing all information
    in the simulation.
@param[out] LogInfo Holds information on warnings and errors
*/
void SW_SWC_water_flow(SW_RUN *sw, LOG_INFO *LogInfo) {
    /* =================================================== */
#ifdef SWDEBUG
    int debug = 0;
#endif

    LyrIndex i;

    /* if there's no swc observation for today,
     * it shows up as SW_MISSING.  The input must
     * define historical swc for at least the top
     * layer to be recognized.
     * IMPORTANT: swc can't be adjusted on day 1 of first year of simulation.
     10/25/2010	(drs)	in SW_SWC_water_flow(): replaced test that "swc can't be
     adjusted on day 1 of year 1" to "swc can't be adjusted on start day of
     first year of simulation"
     */

    if (sw->SoilWat.hist_use &&
        !missing(sw->SoilWat.hist.swc[sw->Model.doy - 1][1])) {

        if (!(sw->Model.doy == sw->Model.startstart &&
              sw->Model.year == sw->Model.startyr)) {

#ifdef SWDEBUG
            if (debug) {
                sw_printf(
                    "\n'SW_SWC_water_flow': adjust SWC from historic inputs.\n"
                );
            }
#endif
            SW_SWC_adjust_swc(
                sw->SoilWat.swcBulk,
                sw->Site.swcBulk_min,
                sw->Model.doy,
                sw->SoilWat.hist,
                sw->Site.n_layers,
                LogInfo
            );

            if (LogInfo->stopRun) {
                return; // Exit function prematurely due to error
            }

        } else {
            LogError(
                LogInfo,
                LOGWARN,
                "Attempt to set SWC on start day of first year of simulation "
                "disallowed."
            );
        }

    } else {
#ifdef SWDEBUG
        if (debug) {
            sw_printf("\n'SW_SWC_water_flow': call 'SW_Water_Flow'.\n");
        }
#endif
        SW_Water_Flow(sw, LogInfo);
        if (LogInfo->stopRun) {
            return; // Exit function prematurely due to error
        }
    }

#ifdef SWDEBUG
    if (debug) {
        sw_printf("\n'SW_SWC_water_flow': check water balance.\n");
    }
    SW_WaterBalance_Checks(sw, LogInfo);
    if (LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

    if (debug) {
        sw_printf("\n'SW_SWC_water_flow': determine wet soil layers.\n");
    }
#endif
    ForEachSoilLayer(i, sw->Site.n_layers) sw->SoilWat.is_wet[i] =
        (Bool) (GE(sw->SoilWat.swcBulk[Today][i], sw->Site.swcBulk_wet[i]));
}

/**
@brief Sets up the structures that will hold the available soil water

partitioned among vegetation types and propagate the swa_master structure for
use in get_dSWAbulk(). Must be call after `SW_SWC_water_flow()` is executed.

@param[in,out] SW_SoilWat Struct of type SW_SOILWAT containing
    soil water related values
@param[in] swcBulk_atSWPcrit SWC corresponding to critical SWP for transpiration
@param[in] SW_VegProd Struct of type SW_VEGPROD describing surface
    cover conditions in the simulation
@param[in] n_layers Number of layers of soil within the simulation run
*/
/***********************************************************/
void calculate_repartitioned_soilwater(
    SW_SOILWAT *SW_SoilWat,
    double swcBulk_atSWPcrit[][MAX_LAYERS],
    SW_VEGPROD *SW_VegProd,
    LyrIndex n_layers
) {

    // this will run for every day of every year
    LyrIndex i;
    double val = SW_MISSING;
    int j;
    int k;
    double curr_crit_val;
    double new_crit_val;

    ForEachSoilLayer(i, n_layers) {
        val = SW_SoilWat->swcBulk[Today][i];
        ForEachVegType(j) {
            if (SW_VegProd->veg[j].cov.fCover != 0) {
                SW_SoilWat->swa_master[j][j][i] =
                    fmax(0., val - swcBulk_atSWPcrit[j][i]);
            } else {
                SW_SoilWat->swa_master[j][j][i] = 0.;
            }
            SW_SoilWat->dSWA_repartitioned_sum[j][i] =
                0.; // need to reset to 0 each time
        }

        // need to check which other critical value each veg_type has access to
        // aside from its own
        //(i.e. if shrub=-3.9 then it also has access to -3.5 and -2.0)
        // go through each veg type
        for (j = 0; j < NVEGTYPES; j++) {
            // get critical value for current veg type
            curr_crit_val = SW_VegProd->critSoilWater[j];
            // go through each critical value to see which ones need to be set
            // for each veg_type
            for (k = 0; k < NVEGTYPES; k++) {
                if (k == j) {
                    // dont need to check for its own critical value
                } else {
                    new_crit_val = SW_VegProd->critSoilWater[k];
                    if (curr_crit_val < new_crit_val) {
                        // need to store this value since it has access to it
                        // itclp(veg_type, new_crit_value, layer, timeperiod)
                        SW_SoilWat->swa_master[j][k][i] =
                            SW_SoilWat->swa_master[k][k][i];
                    }
                    if (curr_crit_val > new_crit_val) {
                        // need to set this value to 0 since it does not have
                        // access to it itclp(veg_type, new_criti_value, layer,
                        // timeperiod)
                        SW_SoilWat->swa_master[j][k][i] = 0.;
                    }
                    if (curr_crit_val == new_crit_val) {
                        // do nothing, dont want to swap values
                    }
                }
            }
        }

        // call function to get repartioned swa values
        get_dSWAbulk(
            i,
            SW_VegProd,
            SW_SoilWat->swa_master,
            SW_SoilWat->dSWA_repartitioned_sum
        );
    }
}

/**
@brief This function calculates the repartioned soilwater.
soilwater for each vegtype is calculated based on size of the critical
soilwater based on the input files.

This goes through the ranked critical
values, starting at the deepest and moving up The deepest veg type has access to
the available soilwater of each veg type above so start at bottom move up.

@param[in] i Integer value for soil layer
@param[in] SW_VegProd Struct of type SW_VEGPROD describing surface
    cover conditions in the simulation
@param[out] swa_master Holds information of veg_type, crit_val, and layer
@param[out] dSWA_repart_sum Repartioned swa values
*/
/***********************************************************/
void get_dSWAbulk(
    unsigned int i,
    SW_VEGPROD *SW_VegProd,
    double swa_master[][NVEGTYPES][MAX_LAYERS],
    double dSWA_repart_sum[][MAX_LAYERS]
) {

    int j;
    int kv;
    int curr_vegType;
    int curr_crit_rank_index;
    int kv_veg_type;
    int prev_crit_veg_type;
    int greater_veg_type;
    double crit_val;
    double prev_crit_val;
    double smallestCritVal;
    // set to current veg type fraction value to avoid multiple if loops. should
    // just need 1 instead of 3 now.
    double veg_type_in_use;
    double vegFractionSum;
    double newFraction;
    double inner_loop_veg_type; // set to inner loop veg type
    smallestCritVal = SW_VegProd->critSoilWater[SW_VegProd->rank_SWPcrits[0]];
    double dSWA_bulk[NVEGTYPES * NVEGTYPES][NVEGTYPES * NVEGTYPES][MAX_LAYERS];
    double dSWA_bulk_repartioned[NVEGTYPES * NVEGTYPES][NVEGTYPES * NVEGTYPES]
                                [MAX_LAYERS];

    // need to initialize to 0
    for (curr_vegType = 0; curr_vegType < NVEGTYPES; curr_vegType++) {
        for (kv = 0; kv < NVEGTYPES; kv++) {
            dSWA_bulk_repartioned[curr_vegType][kv][i] = 0.;
            dSWA_bulk[curr_vegType][kv][i] = 0.;
        }
    }

    // loop through each veg type to get dSWAbulk
    for (curr_vegType = (NVEGTYPES - 1); curr_vegType >= 0; curr_vegType--) {
        // go through each veg type and recalculate if  necessary. starts at
        // smallest
        dSWA_repart_sum[curr_vegType][i] = 0.;
        // get rank index for start of next loop
        curr_crit_rank_index = SW_VegProd->rank_SWPcrits[curr_vegType];
        // set veg type fraction here

        veg_type_in_use = SW_VegProd->veg[curr_crit_rank_index].cov.fCover;
        for (kv = curr_vegType; kv >= 0; kv--) {
            // get crit value at current index
            crit_val = SW_VegProd->critSoilWater[SW_VegProd->rank_SWPcrits[kv]];
            // get index for veg_type. dont want to access
            // SW_SoilWat->swa_master at rank_SWPcrits index
            kv_veg_type = SW_VegProd->rank_SWPcrits[kv];
            if (kv != 0) {
                // get crit value for index lower
                prev_crit_val =
                    SW_VegProd
                        ->critSoilWater[SW_VegProd->rank_SWPcrits[kv - 1]];
                // get veg type that belongs to the corresponding critical value
                prev_crit_veg_type = SW_VegProd->rank_SWPcrits[kv - 1];
            } else {
                // get crit value for index lower
                prev_crit_val =
                    SW_VegProd->critSoilWater[SW_VegProd->rank_SWPcrits[kv]];
                // get veg type that belongs to the corresponding critical value
                prev_crit_veg_type = SW_VegProd->rank_SWPcrits[kv];
            }
            if (veg_type_in_use == 0) {
                // [0=tree(-2.0,off), 1=shrub(-3.9,on),
                // 2=grass(-3.5,on), 3=forb(-2.0,on)]
                // dSWA_bulk: set to 0 to ensure no absent values
                dSWA_bulk[curr_crit_rank_index][kv_veg_type][i] = 0.;
                swa_master[curr_crit_rank_index][kv_veg_type][i] = 0.;
                dSWA_bulk_repartioned[curr_crit_rank_index][kv_veg_type][i] =
                    0.;
            } else {
                // check if need to recalculate for veg types in use
                if (crit_val < prev_crit_val) {
                    // if true then we need to recalculate
                    if (swa_master[curr_crit_rank_index][kv_veg_type][i] == 0) {
                        dSWA_bulk[curr_crit_rank_index][kv_veg_type][i] = 0.;
                    } else {
                        dSWA_bulk[curr_crit_rank_index][kv_veg_type][i] =
                            swa_master[curr_crit_rank_index][kv_veg_type][i] -
                            swa_master[curr_crit_rank_index][prev_crit_veg_type]
                                      [i];
                    }
                } else if (crit_val == prev_crit_val) {
                    // critical values equal just set to itself
                    dSWA_bulk[curr_crit_rank_index][kv_veg_type][i] =
                        swa_master[curr_crit_rank_index][kv_veg_type][i];
                } else {
                    // do nothing if crit val >. this will be handled later
                }

                /* ##############################################################
                        below if else blocks are for redistributing dSWAbulk
                values
                ##############################################################
              */
                if (curr_vegType == (NVEGTYPES - 1) && kv == (NVEGTYPES - 1) &&
                    prev_crit_val != crit_val) {
                    // if largest critical value and only veg type with that
                    // value just set it to dSWAbulk
                    dSWA_bulk_repartioned[curr_crit_rank_index][kv_veg_type]
                                         [i] = dSWA_bulk[curr_crit_rank_index]
                                                        [kv_veg_type][i];
                } else {
                    // all values other than largest well need repartitioning
                    if (crit_val == smallestCritVal) {
                        // if smallest value then all veg_types have access to
                        // it so just need to multiply by its fraction
                        dSWA_bulk_repartioned[curr_crit_rank_index][kv_veg_type]
                                             [i] =
                                                 dSWA_bulk[curr_crit_rank_index]
                                                          [kv_veg_type][i] *
                                                 veg_type_in_use;
                        // multiply by fraction for index of curr_vegType not kv
                    } else {
                        // critical values that more than one veg type have
                        // access to but less than all veg types

                        // vegFractionSum: set to 0 each time to make sure it
                        // gets correct value
                        vegFractionSum = 0;
                        // need to calculute new fraction for these since sum of
                        // them no longer adds up to 1. do this by adding
                        // fractions values of veg types who have access and
                        // then dividing these fraction values by the total sum.
                        // keeps the ratio and adds up to 1
                        for (j = (NVEGTYPES - 1); j >= 0; j--) {
                            // go through all critical values and sum the
                            // fractions of the veg types who have access

                            // set veg type fraction here
                            inner_loop_veg_type = SW_VegProd->veg[j].cov.fCover;

                            if (SW_VegProd->critSoilWater[j] <= crit_val) {
                                vegFractionSum += inner_loop_veg_type;
                            }
                        }

                        // divide veg fraction by sum to get new fraction value
                        newFraction = veg_type_in_use / vegFractionSum;
                        dSWA_bulk_repartioned[curr_crit_rank_index][kv_veg_type]
                                             [i] =
                                                 dSWA_bulk[curr_crit_rank_index]
                                                          [kv_veg_type][i] *
                                                 newFraction;
                    }
                }
            }
        }
        // setting all the veg types above current to 0 since they do not have
        // access to those ex: if forb=-2.0 grass=-3.5 & shrub=-3.9 then need to
        // set grass and shrub to 0 for forb
        for (j = curr_vegType + 1; j < NVEGTYPES; j++) {
            greater_veg_type = SW_VegProd->rank_SWPcrits[j];
            if (SW_VegProd->critSoilWater[SW_VegProd->rank_SWPcrits[j - 1]] >
                SW_VegProd->critSoilWater[SW_VegProd->rank_SWPcrits[j]]) {
                dSWA_bulk[curr_crit_rank_index][greater_veg_type][i] = 0.;
                dSWA_bulk_repartioned[curr_crit_rank_index][greater_veg_type]
                                     [i] = 0.;
            }
        }
    }

    for (curr_vegType = 0; curr_vegType < NVEGTYPES; curr_vegType++) {
        for (kv = 0; kv < NVEGTYPES; kv++) {
            if (SW_VegProd->veg[curr_vegType].cov.fCover == 0.) {
                dSWA_repart_sum[curr_vegType][i] = 0.;
            } else {
                dSWA_repart_sum[curr_vegType][i] +=
                    dSWA_bulk_repartioned[curr_vegType][kv][i];
            }
        }
    }

    /*int x = 0;
                printf("dSWAbulk_repartition forb[%d,0,%d]:
       %f\n",x,i,dSWA_bulk_repartioned[x][0][i]); printf("dSWAbulk_repartition
       forb[%d,1,%d]: %f\n",x,i,dSWA_bulk_repartioned[x][1][i]);
                printf("dSWAbulk_repartition forb[%d,2,%d]:
       %f\n",x,i,dSWA_bulk_repartioned[x][2][i]); printf("dSWAbulk_repartition
       forb[%d,3,%d]: %f\n\n",x,i,dSWA_bulk_repartioned[x][3][i]);
                printf("dSWA_repartitioned_sum[%d][%d]: %f\n\n", x,i,
       SW_SoilWat->dSWA_repartitioned_sum[x][i]);


                printf("---------------------\n\n");*/
}

/**
@brief Copies today's values so that the values for swcBulk and snowpack become
yesterday's values.

@param[in,out] SW_SoilWat Struct of type SW_SOILWAT containing soil water
    related values
@param[in] n_layers Number of layers of soil within the simulation run
*/
void SW_SWC_end_day(SW_SOILWAT *SW_SoilWat, LyrIndex n_layers) {
    /* =================================================== */
    LyrIndex i;

    ForEachSoilLayer(i, n_layers) {
        SW_SoilWat->swcBulk[Yesterday][i] = SW_SoilWat->swcBulk[Today][i];
    }

    SW_SoilWat->snowpack[Yesterday] = SW_SoilWat->snowpack[Today];
}

void SW_SWC_init_run(
    SW_SOILWAT *SW_SoilWat, SW_SITE *SW_Site, double *temp_snow
) {

    SW_SoilWat->soiltempError = swFALSE;

#ifdef SWDEBUG
    SW_SoilWat->is_wbError_init = swFALSE;
#endif

    *temp_snow = 0.; // Snow temperature

    reset_swc(SW_SoilWat, SW_Site);
}

/**
@brief init first doy swc, either by the computed init value or by the last day
of last year, which is also, coincidentally, Yesterday

@param[in,out] SW_SoilWat Struct of type SW_SOILWAT containing
    soil water related values
@param[in] SW_Site Struct of type SW_SITE describing the simulated site
@param[in] year Current year being run in the simulation
@param[out] LogInfo Holds information on warnings and errors
*/
void SW_SWC_new_year(
    SW_SOILWAT *SW_SoilWat, SW_SITE *SW_Site, TimeInt year, LOG_INFO *LogInfo
) {

    LyrIndex lyr;

    if (SW_Site->reset_yr) {
        reset_swc(SW_SoilWat, SW_Site);

    } else {
        /* update swc */
        ForEachSoilLayer(lyr, SW_Site->n_layers) {
            SW_SoilWat->swcBulk[Today][lyr] =
                SW_SoilWat->swcBulk[Yesterday][lyr];
        }

        /* update snowpack */
        SW_SoilWat->snowpack[Today] = SW_SoilWat->snowpack[Yesterday];
    }

    /* update historical (measured) values, if needed */
    if (SW_SoilWat->hist_use && year >= SW_SoilWat->hist.yr.first) {
#ifndef RSOILWAT
        read_swc_hist(&SW_SoilWat->hist, year, LogInfo);
#else
        LogError(
            LogInfo,
            LOGERROR,
            "rSOILWAT2 currently does not support historical SWC inputs."
        );

        if (swFALSE) {
            // read from disk:
            read_swc_hist(&SW_SoilWat->hist, year, LogInfo);
        } else {
            // copy from R memory
            // onSet_SW_SWC_hist(LogInfo);
        }
#endif
    }
}

/**
@brief Like all of the other functions, read() reads in the setup parameters.

See read_swc_hist() for reading historical files.

@param[in,out] SW_SoilWat Struct of type SW_SOILWAT containing
    soil water related values
@param[in] endyr Ending year for model run
@param[in] txtInFiles Array of program in/output files
@param[out] LogInfo Holds information on warnings and errors
*/
void SW_SWC_read(
    SW_SOILWAT *SW_SoilWat, TimeInt endyr, char *txtInFiles[], LOG_INFO *LogInfo
) {
    /* =================================================== */
    /* HISTORY
     *  1/25/02 - cwb - removed unused records of logfile and
     *          start and end days.  also removed the SWTIMES dy
     *          structure element.
     */
    FILE *f;
    int lineno = 0;
    int nitems = 4;
    char inbuf[MAX_FILENAMESIZE];
    int inBufintRes = 0;
    Bool convertInput;

    char *MyFileName = txtInFiles[eSoilwat];
    f = OpenFile(MyFileName, "r", LogInfo);
    if (LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

    while (GetALine(f, inbuf, MAX_FILENAMESIZE)) {
        convertInput = (Bool) (lineno >= 0 && lineno <= 3 && lineno != 1);

        if (convertInput) {
            inBufintRes = sw_strtoi(inbuf, MyFileName, LogInfo);
            if (LogInfo->stopRun) {
                goto closeFile;
            }
        }

        switch (lineno) {
        case 0:
            SW_SoilWat->hist_use = (inBufintRes) ? swTRUE : swFALSE;
            break;
        case 1:
            SW_SoilWat->hist.file_prefix = Str_Dup(inbuf, LogInfo);
            if (LogInfo->stopRun) {
                goto closeFile;
            }
            break;
        case 2:
            SW_SoilWat->hist.yr.first = yearto4digit((TimeInt) inBufintRes);
            break;
        case 3:
            SW_SoilWat->hist.method = inBufintRes;
            break;
        default:
            LogError(
                LogInfo,
                LOGERROR,
                "SW_SWC_read(): incorrect format of input file '%s'.",
                MyFileName
            );
            goto closeFile;
            break;
        }
        lineno++;
    }

    if (!SW_SoilWat->hist_use) {
        goto closeFile;
    }
    if (lineno < nitems) {
        LogError(
            LogInfo,
            LOGERROR,
            "%s : Insufficient parameters specified.",
            MyFileName
        );
        goto closeFile;
    }
    if (SW_SoilWat->hist.method < 1 || SW_SoilWat->hist.method > 2) {
        LogError(
            LogInfo, LOGERROR, "%s : Invalid swc adjustment method.", MyFileName
        );
        goto closeFile;
    }
    SW_SoilWat->hist.yr.last = endyr;
    SW_SoilWat->hist.yr.total =
        SW_SoilWat->hist.yr.last - SW_SoilWat->hist.yr.first + 1;

closeFile: { CloseFile(&f, LogInfo); }
}

/**
@brief Read a file containing historical swc measurements.

Enter a year with a four digit year number. This is appended to the swc prefix
to make the input file name.

@param[in, out] SoilWat_hist Struct of type SW_SOILWAT_HIST holding parameters
    for historical (measured) swc values
@param[in] year Four digit number for desired year, measured in years.
@param[out] LogInfo Holds information on warnings and errors
*/
void read_swc_hist(
    SW_SOILWAT_HIST *SoilWat_hist, TimeInt year, LOG_INFO *LogInfo
) {
    /* =================================================== */
    /* read a file containing historical swc measurements.
     * Enter with year a four digit year number.  This is
     * appended to the swc prefix to make the input file
     * name.
     *
     *
     * 1/25/02 - cwb - removed year field from input records.
     *         This code uses GetALine() which discards comments
     *         and only one year's data per file is allowed, so
     *         and the date is part of the file name, but if you
     *         must, you can add the date as well as other data
     *         inside comments, preferably at the top of the file.
     *
     * Format of the input file is
     * "doy layer swc stderr"
     *
     * for example,
     *  90 1 1.11658 .1
     *  90 2 1.11500 .1
     *    ...
     * 185 1 2.0330  .23
     * 185 2 3.1432  .25
     *    ...
     * note that missing days or layers will not cause
     * an error in the input, but missing layers could
     * cause problems in the flow model.
     */
    FILE *f;
    int x;
    int lyr = 0;
    int recno = 0;
    int doy = 0;
    int index;
    int resSNP;
    double swc = 0.;
    double st_err = 0.;
    char fname[MAX_FILENAMESIZE];
    char inbuf[MAX_FILENAMESIZE];
    char varStrs[4][20] = {{'\0'}};
    int *inBufIntVals[] = {&doy, &lyr};
    double *inBufDoubleVals[] = {&swc, &st_err};
    const int numInValsPerType = 2;

    resSNP = snprintf(
        fname, sizeof fname, "%s.%4d", SoilWat_hist->file_prefix, year
    );

    if (resSNP < 0 || (unsigned) resSNP >= (sizeof fname)) {
        LogError(
            LogInfo,
            LOGERROR,
            "SWC-hist file name is too long for year = %d",
            year
        );
        return; // Exit function prematurely due to error
    }

    f = OpenFile(fname, "r", LogInfo);
    if (LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

    clear_hist(SoilWat_hist->swc, SoilWat_hist->std_err);

    while (GetALine(f, inbuf, MAX_FILENAMESIZE)) {
        recno++;
        x = sscanf(
            inbuf,
            "%19s %19s %19s %19s",
            varStrs[0],
            varStrs[1],
            varStrs[2],
            varStrs[3]
        );

        if (x < 4) {
            LogError(
                LogInfo,
                LOGERROR,
                "%s : Incomplete layer data at record %d\n   Should be DOY LYR "
                "SWC STDERR.",
                fname,
                recno
            );
            goto closeFile;
        }
        if (x > 4) {
            LogError(
                LogInfo,
                LOGERROR,
                "%s : Too many input fields at record %d\n   Should be DOY LYR "
                "SWC STDERR.",
                fname,
                recno
            );
            goto closeFile;
        }

        for (index = 0; index < numInValsPerType; index++) {
            *(inBufIntVals[index]) = sw_strtoi(varStrs[index], fname, LogInfo);
            if (LogInfo->stopRun) {
                goto closeFile;
            }

            *(inBufDoubleVals[index]) =
                sw_strtod(varStrs[index + 2], fname, LogInfo);
            if (LogInfo->stopRun) {
                goto closeFile;
            }
        }

        if (doy < 1 || doy > MAX_DAYS) {
            LogError(
                LogInfo,
                LOGERROR,
                "%s : Day of year out of range at record %d",
                fname,
                recno
            );
            goto closeFile;
        }
        if (lyr < 1 || lyr > MAX_LAYERS) {
            LogError(
                LogInfo,
                LOGERROR,
                "%s : Layer number out of range (%d > %d), record %d\n",
                fname,
                lyr,
                MAX_LAYERS,
                recno
            );
            goto closeFile;
        }

        SoilWat_hist->swc[doy - 1][lyr - 1] = swc;
        SoilWat_hist->std_err[doy - 1][lyr - 1] = st_err;
    }

closeFile: { CloseFile(&f, LogInfo); }
}

/**
@brief Adjusts swc based on day of the year.

@param[in,out] swcBulk Soil water content in the layer [cm]
@param[in] swcBulk_min Minimal SWC [cm]
@param[in] doy Day of the year, measured in days.
@param[in] SoilWat_hist Struct of type SW_SOILWAT_HIST holding parameters for
    historical (measured) swc values
@param[in] n_layers Total number of soil layers in simulation
@param[out] LogInfo Holds information on warnings and errors
*/
void SW_SWC_adjust_swc(
    double swcBulk[][MAX_LAYERS],
    double swcBulk_min[],
    TimeInt doy,
    SW_SOILWAT_HIST SoilWat_hist,
    LyrIndex n_layers,
    LOG_INFO *LogInfo
) {

    /* =================================================== */
    /* 01/07/02 (cwb) added final loop to guarantee swc > swcBulk_min
     */
    double lower;
    double upper;
    LyrIndex lyrIndex;
    TimeInt dy = doy - 1;

    switch (SoilWat_hist.method) {
    case SW_Adjust_Avg:
        ForEachSoilLayer(lyrIndex, n_layers) {
            swcBulk[Today][lyrIndex] += SoilWat_hist.swc[dy][lyrIndex];
            swcBulk[Today][lyrIndex] /= 2.;
        }
        break;

    case SW_Adjust_StdErr:
        ForEachSoilLayer(lyrIndex, n_layers) {
            upper = SoilWat_hist.swc[dy][lyrIndex] +
                    SoilWat_hist.std_err[dy][lyrIndex];
            lower = SoilWat_hist.swc[dy][lyrIndex] -
                    SoilWat_hist.std_err[dy][lyrIndex];
            if (GT(swcBulk[Today][lyrIndex], upper)) {
                swcBulk[Today][lyrIndex] = upper;
            } else if (LT(swcBulk[Today][lyrIndex], lower)) {
                swcBulk[Today][lyrIndex] = lower;
            }
        }
        break;

    default:
        LogError(
            LogInfo, LOGERROR, "swcsetup.in : Invalid SWC adjustment method."
        );
        return; // Exit function prematurely due to error
    }

    /* this will guarantee that any method will not lower swc */
    /* below the minimum defined for the soil layers          */
    ForEachSoilLayer(lyrIndex, n_layers) {
        swcBulk[Today][lyrIndex] =
            fmax(swcBulk[Today][lyrIndex], swcBulk_min[lyrIndex]);
    }
}

/**
@brief Calculates todays snowpack, partitioning of ppt into rain and snow,
snowmelt and snowloss.

Equations based on SWAT2K routines. @cite Neitsch2005

@param[in,out] snowpack[] swe of snowpack, assuming accumulation is turned on
@param[in,out] *temp_snow Module-level snow temperature (C)
@param[in] SW_Site Struct of type SW_SITE describing the site in question
@param[in] temp_min Daily minimum temperature (C)
@param[in] temp_max Daily maximum temperature (C)
@param[in] ppt Daily precipitation (cm)
@param[in] doy Day of the year (base1) [1-366]
@param[out] *rain Daily rainfall (cm)
@param[out] *snow Daily snow-water equivalent of snowfall (cm)
@param[out] *snowmelt  Daily snow-water equivalent of snowmelt (cm)

@sideeffect *rain Updated daily rainfall (cm)
@sideeffect *snow Updated snow-water equivalent of snowfall (cm)
@sideeffect *snowmelt Updated snow-water equivalent of daily snowmelt (cm)
*/

void SW_SWC_adjust_snow(
    double *temp_snow,
    double snowpack[],
    SW_SITE *SW_Site,
    double temp_min,
    double temp_max,
    double ppt,
    TimeInt doy,
    double *rain,
    double *snow,
    double *snowmelt
) {

    /***************************************************************************
    History:

    10/04/2010	(drs) added snowMAUS snow accumulation, sublimation and melt
    algorithm: Trnka, M., Kocmánková, E., Balek, J., Eitzinger, J., Ruget, F.,
    Formayer, H., Hlavinka, P., Schaumberger, A., Horáková, V., Mozny, M. &
    Zalud, Z. (2010) Simple snow cover model for agrometeorological
    applications. Agricultural and Forest Meteorology, 150, 1115-1127. replaced
    SW_SWC_snow_accumulation, SW_SWC_snow_sublimation, and SW_SWC_snow_melt with
    SW_SWC_adjust_snow 10/19/2010	(drs) replaced snowMAUS simulation with
    SWAT2K routines.

    ***********************************************************************/

    const double snow_cov = 1.;
    double *snowpack_today = &snowpack[Today];
    double temp_ave;
    double Rmelt;
    double SnowAccu = 0.;
    double SnowMelt = 0.;

    temp_ave = (temp_min + temp_max) / 2.;

    /* snow accumulation */
    if (LE(temp_ave, SW_Site->TminAccu2)) {
        SnowAccu = ppt;
    } else {
        SnowAccu = 0.;
    }
    *rain = fmax(0., ppt - SnowAccu);
    *snow = fmax(0., SnowAccu);
    *snowpack_today += SnowAccu;

    /* snow melt */
    Rmelt =
        (SW_Site->RmeltMax + SW_Site->RmeltMin) / 2. +
        sin((doy - 81.) / 58.09) * (SW_Site->RmeltMax - SW_Site->RmeltMin) / 2.;
    *temp_snow =
        *temp_snow * (1 - SW_Site->lambdasnow) + temp_ave * SW_Site->lambdasnow;

    if (GT(*temp_snow, SW_Site->TmaxCrit)) {
        SnowMelt = fmin(
            *snowpack_today,
            Rmelt * snow_cov *
                ((*temp_snow + temp_max) / 2. - SW_Site->TmaxCrit)
        );

    } else {
        SnowMelt = 0.;
    }

    if (GT(*snowpack_today, 0.)) {
        *snowmelt = fmax(0., SnowMelt);
        *snowpack_today = fmax(0., *snowpack_today - *snowmelt);
    } else {
        *snowmelt = 0.;
    }
}

/**
@brief Snow loss through sublimation and other processes

Equations based on SWAT2K routines. @cite Neitsch2005

@param pet Potential evapotranspiration rate, measured in cm per day.
@param *snowpack Snow-water equivalent of single layer snowpack, measured in cm.

@sideeffect *snowpack Updated snow-water equivalent of single layer snowpack,
measured in cm.

@return snowloss Snow loss through sublimation and other processes, measuered in
cm.

*/
double SW_SWC_snowloss(double pet, double *snowpack) {
    const double cov_soil = 0.5;
    double snowloss;

    if (GT(*snowpack, 0.)) {
        snowloss = fmax(0., fmin(*snowpack, cov_soil * pet));
        *snowpack = fmax(0., *snowpack - snowloss);
    } else {
        snowloss = 0.;
    }

    return snowloss;
}

/**
@brief Calculates depth of snowpack.

@param SWE snow water equivalents (cm = 10kg/m2).
@param snowdensity Density of snow, (kg/m3).

@return SW_SnowDepth Snow depth, measured in cm.
*/

double SW_SnowDepth(double SWE, double snowdensity) {
    /*---------------------
     08/22/2011	(drs)	calculates depth of snowpack
     Input:	SWE: snow water equivalents (cm = 10kg/m2)
     snowdensity (kg/m3)
     Output: snow depth (cm)
     ---------------------*/
    return (GT(snowdensity, 0.)) ? (SWE / snowdensity * 10. * 100.) : 0.;
}

/**
@brief Convert soil water content to soil water potential using
  specified soil water retention curve (SWRC)

SOILWAT2 convenience wrapper for `SWRC_SWCtoSWP()`.

See #swrc2str() for implemented SWRCs.

@param[in] swcBulk Soil water content in the layer [cm]
@param[in] SW_Site Struct of type SW_SITE describing the simulated site
@param[in] layerno Current layer which is being worked with
@param[out] LogInfo Holds information on warnings and errors

@return Soil water potential [-bar]
*/
double SW_SWRC_SWCtoSWP(
    double swcBulk, SW_SITE *SW_Site, LyrIndex layerno, LOG_INFO *LogInfo
) {
    return SWRC_SWCtoSWP(
        swcBulk,
        SW_Site->swrc_type[layerno],
        SW_Site->swrcp[layerno],
        SW_Site->soils.fractionVolBulk_gravel[layerno],
        SW_Site->soils.width[layerno],
        LOGERROR,
        LogInfo
    );
}

/**
@brief Convert soil water content to soil water potential using
  specified soil water retention curve (SWRC)

See #swrc2str() for implemented SWRCs.

The code assumes the following conditions:
    - checked by `SW_SIT_init_run()`
        - width > 0
        - fractionGravel, sand, clay, and sand + clay in [0, 1]
    - SWRC parameters checked by `SWRC_check_parameters()`.

@param[in] swcBulk Soil water content in the layer [cm]
@param[in] swrc_type Identification number of selected SWRC
@param[in] *swrcp Vector of SWRC parameters
@param[in] gravel Coarse fragments (> 2 mm; e.g., gravel)
    of the whole soil [m3/m3]
@param[in] width Soil layer width [cm]
@param[in] errmode An error code passed to `LogError()`.
    SOILWAT2 uses `LOGERROR` and fails but other applications may want to warn
    only (`LOGWARN`) and return.
@param[out] LogInfo Holds information on warnings and errors

@return Soil water potential [-bar]
*/
double SWRC_SWCtoSWP(
    double swcBulk,
    unsigned int swrc_type,
    double *swrcp,
    double gravel,
    double width,
    const int errmode,
    LOG_INFO *LogInfo
) {
    double res = SW_MISSING;

    if (LT(swcBulk, 0.) || GE(gravel, 1.) || LE(width, 0.)) {

        if (LT(swcBulk, 0.)) {
            LogError(
                LogInfo,
                errmode,
                "SWRC_SWCtoSWP(): invalid SWC = %.4f (must be >= 0)\n",
                swcBulk
            );

        } else if (GE(gravel, 1.)) {
            LogError(
                LogInfo,
                errmode,
                "SWRC_SWCtoSWP(): invalid gravel = %.4f (must be in [0, 1[)\n",
                gravel
            );

        } else {
            LogError(
                LogInfo,
                errmode,
                "SWRC_SWCtoSWP(): invalid layer width = %.4f (must be > 0)\n",
                width
            );
        }

    } else {
        switch (swrc_type) {
        case sw_Campbell1974:
            res = SWRC_SWCtoSWP_Campbell1974(
                swcBulk, swrcp, gravel, width, errmode, LogInfo
            );
            break;

        case sw_vanGenuchten1980:
            res = SWRC_SWCtoSWP_vanGenuchten1980(
                swcBulk, swrcp, gravel, width, errmode, LogInfo
            );
            break;

        case sw_FXW:
            res = SWRC_SWCtoSWP_FXW(
                swcBulk, swrcp, gravel, width, errmode, LogInfo
            );
            break;

        default:
            LogError(
                LogInfo,
                errmode,
                "SWRC (type %d) is not implemented.",
                swrc_type
            );
            break;
        }
    }

    return res;
}

/**
@brief Convert soil water content to soil water potential using
Campbell's 1974 \cite Campbell1974 Soil Water Retention Curve

Parameters are explained in `SWRC_check_parameters_for_Campbell1974()`.

@note
This function was previously named `SW_SWCbulk2SWPmatric()`.

@note
`SWRC_SWPtoSWC_Campbell1974()` and `SWRC_SWCtoSWP_Campbell1974()`
are the inverse of each other for `(phi, theta)` between
`(swrcp[0], theta at swrcp[0])` and `(infinity, 0)`.

@note
The function has a discontinuity at saturated water content for which
the matric potential at the "air-entry suction" point (see `swrcp[0]`)
is returned whereas 0 bar is returned for larger values.

@param[in] swcBulk Soil water content in the layer [cm]
@param[in] *swrcp Vector of SWRC parameters
@param[in] gravel Coarse fragments (> 2 mm; e.g., gravel)
    of the whole soil [m3/m3]
@param[in] width Soil layer width [cm]
@param[in] errmode An error code passed to `LogError()`.
    SOILWAT2 uses `LOGERROR` and fails but other applications may want
    to warn only (`LOGWARN`) and return.
@param[out] LogInfo Holds information on warnings and errors

@return Soil water potential [-bar]
*/
double SWRC_SWCtoSWP_Campbell1974(
    double swcBulk,
    double *swrcp,
    double gravel,
    double width,
    const int errmode,
    LOG_INFO *LogInfo
) {
    // assume that we have soil moisture
    double theta;
    double tmp;
    double res;

    // convert bulk SWC [cm] to theta = matric VWC [cm / cm]
    theta = swcBulk / (width * (1. - gravel));

    if (GT(theta, swrcp[1])) {
        // `theta` should not become larger than `theta_sat`;
        // however, "Cosby1984AndOthers" does not use `swrcp[1]` for `theta_sat`
        // which can lead to inconsistencies; thus,
        // we return with 0 instead of, correctly, with errmode and SW_MISSING
        res = 0.;

    } else {
        // calculate (theta / theta_s) ^ b
        tmp = powe(theta / swrcp[1], swrcp[2]);

        if (LE(tmp, 0.)) {
            LogError(
                LogInfo,
                errmode,
                "SWRC_SWCtoSWP_Campbell1974(): invalid value of\n"
                "\t(theta / theta(saturated)) ^ b = (%f / %f) ^ %f =\n"
                "\t= %f (must be > 0)\n",
                theta,
                swrcp[1],
                swrcp[2],
                tmp
            );

            return SW_MISSING; // Exit function prematurely due to error
        }

        res = swrcp[0] / tmp;
    }

    // convert [cm of H20; SOILWAT2 legacy value] to [bar]
    return res / 1024.;
}

/**
@brief Convert soil water content to soil water potential using
van Genuchten 1980 \cite vanGenuchten1980 Soil Water Retention Curve

Parameters are explained in `SWRC_check_parameters_for_vanGenuchten1980()`.

@note
`SWRC_SWPtoSWC_vanGenuchten1980()` and `SWRC_SWCtoSWP_vanGenuchten1980()`
are the inverse of each other for `(phi, theta)` between
`(0, theta_sat)` and `(infinity, theta_min)`.

@param[in] swcBulk Soil water content in the layer [cm]
@param[in] *swrcp Vector of SWRC parameters
@param[in] gravel Coarse fragments (> 2 mm; e.g., gravel)
    of the whole soil [m3/m3]
@param[in] width Soil layer width [cm]
@param[in] errmode An error code passed to `LogError()`.
    SOILWAT2 uses `LOGERROR` and fails but other applications may want to
    warn only (`LOGWARN`) and return.
@param[out] LogInfo Holds information on warnings and errors

@return Soil water potential [-bar]
*/
double SWRC_SWCtoSWP_vanGenuchten1980(
    double swcBulk,
    double *swrcp,
    double gravel,
    double width,
    const int errmode,
    LOG_INFO *LogInfo
) {
    double res;
    double tmp;
    double theta;

    // convert bulk SWC [cm] to theta = matric VWC [cm / cm]
    theta = swcBulk / (width * (1. - gravel));

    // calculate if theta in ]theta_min, theta_sat]
    if (GT(theta, swrcp[0])) {
        if (LT(theta, swrcp[1])) {
            // calculate inverse of normalized theta
            tmp = (swrcp[1] - swrcp[0]) / (theta - swrcp[0]);

            // calculate tension [cm of H20]
            tmp = powe(tmp, 1. / (1. - 1. / swrcp[3])); // tmp values are >= 1
            // `pow()` because x >= 0
            res = pow(-1. + tmp, 1. / swrcp[3]) / swrcp[2];

            // convert [cm of H2O at 4 C; value from `soilDB::KSSL_VG_model()`]
            // to [bar]
            res /= 1019.716;

        } else if (EQ(theta, swrcp[1])) {
            // theta is theta_sat
            res = 0;

        } else {
            // theta is > theta_sat
            LogError(
                LogInfo,
                errmode,
                "SWRC_SWCtoSWP_vanGenuchten1980(): invalid value of\n"
                "\ttheta = %f (must be <= theta_sat = %f)\n",
                theta,
                swrcp[1]
            );

            res = SW_MISSING;
        }

    } else {
        // theta is <= theta_min
        LogError(
            LogInfo,
            errmode,
            "SWRC_SWCtoSWP_vanGenuchten1980(): invalid value of\n"
            "\ttheta = %f (must be > theta_min = %f)\n",
            theta,
            swrcp[0]
        );

        res = SW_MISSING;
    }

    return res;
}

/**
@brief Convert soil water content to soil water potential using
FXW Soil Water Retention Curve
\cite fredlund1994CGJa, \cite wang2018wrr

Parameters are explained in `SWRC_check_parameters_for_FXW()`.

@note
`SWRC_SWPtoSWC_FXW()` and `SWRC_SWCtoSWP_FXW()`
are the inverse of each other for `(phi, theta)` between
`(0, theta_sat)` and `(6.3 x 10^6 cm, 0)`.


@param[in] swcBulk Soil water content in the layer [cm]
@param[in] *swrcp Vector of SWRC parameters
@param[in] gravel Coarse fragments (> 2 mm; e.g., gravel)
    of the whole soil [m3/m3]
@param[in] width Soil layer width [cm]
@param[in] errmode An error code passed to `LogError()`.
    SOILWAT2 uses `LOGERROR` and fails but other applications may want to
    warn only (`LOGWARN`) and return.
@param[out] LogInfo Holds information on warnings and errors

@return Soil water potential [-bar]
*/
double SWRC_SWCtoSWP_FXW(
    double swcBulk,
    double *swrcp,
    double gravel,
    double width,
    const int errmode,
    LOG_INFO *LogInfo
) {
    double res;
    double theta;

    // convert bulk SWC [cm] to theta = matric VWC [cm / cm]
    theta = swcBulk / (width * (1. - gravel));

    // calculate if theta in [0, theta_sat]
    if (GE(theta, 0.)) {
        if (LT(theta, swrcp[0])) {
            // calculate tension = phi [bar]
            res = itp_FXW_for_phi(theta, swrcp, LogInfo);

        } else if (EQ(theta, swrcp[0])) {
            // theta is theta_sat
            res = 0.;

        } else {
            // theta is > theta_sat
            LogError(
                LogInfo,
                errmode,
                "SWRC_SWCtoSWP_FXW(): invalid value of\n"
                "\ttheta = %f (must be <= theta_sat = %f)\n",
                theta,
                swrcp[0]
            );

            res = SW_MISSING;
        }

    } else {
        // theta is < 0
        LogError(
            LogInfo,
            errmode,
            "SWRC_SWCtoSWP_FXW(): invalid value of\n"
            "\ttheta = %f (must be >= 0)\n",
            theta
        );

        res = SW_MISSING;
    }

    return res;
}

/**
@brief Convert soil water potential to soil water content using
specified soil water retention curve (SWRC)

SOILWAT2 convenience wrapper for `SWRC_SWPtoSWC()`.

See #swrc2str() for implemented SWRCs.

@param[in] swpMatric Soil water potential [-bar]
@param[in] SW_Site Struct of type SW_SITE describing the simulated site
@param[in] layerno Current layer which is being worked with
@param[out] LogInfo Holds information on warnings and errors

@return Soil water content in the layer [cm]
*/
double SW_SWRC_SWPtoSWC(
    double swpMatric, SW_SITE *SW_Site, LyrIndex layerno, LOG_INFO *LogInfo
) {
    return SWRC_SWPtoSWC(
        swpMatric,
        SW_Site->swrc_type[layerno],
        SW_Site->swrcp[layerno],
        SW_Site->soils.fractionVolBulk_gravel[layerno],
        SW_Site->soils.width[layerno],
        LOGERROR,
        LogInfo
    );
}

/**
@brief Convert soil water potential to soil water content using
specified soil water retention curve (SWRC)

See #swrc2str() for implemented SWRCs.

The code assumes the following conditions:
    - checked by `SW_SIT_init_run()`
        - width > 0
        - fractionGravel, sand, clay, and sand + clay in [0, 1]
    - SWRC parameters checked by `SWRC_check_parameters()`.

@param[in] swpMatric Soil water potential [-bar]
@param[in] swrc_type Identification number of selected SWRC
@param[in] *swrcp Vector of SWRC parameters
@param[in] gravel Coarse fragments (> 2 mm; e.g., gravel)
    of the whole soil [m3/m3]
@param[in] width Soil layer width [cm]
@param[in] errmode An error code passed to `LogError()`.
    SOILWAT2 uses `LOGERROR` and fails but other applications may want to
    warn only (`LOGWARN`) and return.
@param[out] LogInfo Holds information on warnings and errors

@return Soil water content in the layer [cm]
*/
double SWRC_SWPtoSWC(
    double swpMatric,
    unsigned int swrc_type,
    double *swrcp,
    double gravel,
    double width,
    const int errmode,
    LOG_INFO *LogInfo
) {
    double res = SW_MISSING;

    if (LT(swpMatric, 0.)) {
        LogError(
            LogInfo,
            errmode,
            "SWRC_SWPtoSWC(): invalid SWP = %.4f (must be >= 0)\n",
            swpMatric
        );

    } else {
        switch (swrc_type) {
        case sw_Campbell1974:
            res = SWRC_SWPtoSWC_Campbell1974(swpMatric, swrcp, gravel, width);
            break;

        case sw_vanGenuchten1980:
            res =
                SWRC_SWPtoSWC_vanGenuchten1980(swpMatric, swrcp, gravel, width);
            break;

        case sw_FXW:
            res = SWRC_SWPtoSWC_FXW(swpMatric, swrcp, gravel, width);
            break;

        default:
            LogError(
                LogInfo,
                errmode,
                "SWRC (type %d) is not implemented.",
                swrc_type
            );
            break;
        }
    }

    return res;
}

/**
@brief Convert soil water potential to soil water content using
Campbell's 1974 \cite Campbell1974 Soil Water Retention Curve

Parameters are explained in `SWRC_check_parameters_for_Campbell1974()`.

@note
This function was previously named `SW_SWPmatric2VWCBulk()`.

@note
The function returns saturated water content if `swpMatric` is at or below
the "air-entry suction" point (see `swrcp[0]`).

@note
`SWRC_SWPtoSWC_Campbell1974()` and `SWRC_SWCtoSWP_Campbell1974()`
are the inverse of each other for `(phi, theta)` between
`(swrcp[0], theta at swrcp[0])` and `(infinity, 0)`.

@param[in] swpMatric Soil water potential [-bar]
@param[in] *swrcp Vector of SWRC parameters
@param[in] gravel Coarse fragments (> 2 mm; e.g., gravel)
    of the whole soil [m3/m3]
@param[in] width Soil layer width [cm]

@return Soil water content in the layer [cm]
*/
double SWRC_SWPtoSWC_Campbell1974(
    double swpMatric, double *swrcp, double gravel, double width
) {
    double phi;
    double res;

    // convert SWP [-bar] to phi [cm of H20; SOILWAT2 legacy value]
    phi = swpMatric * 1024.;

    if (LT(phi, swrcp[0])) {
        res = swrcp[1]; // theta_sat

    } else {
        // calculate matric theta [cm / cm]
        // within [0, theta_sat] because `phi` > swrcp[0]
        res = swrcp[1] * powe(swrcp[0] / phi, 1. / swrcp[2]);
    }

    // convert matric theta [cm / cm] to bulk SWC [cm]
    return (1. - gravel) * width * res;
}

/**
@brief Convert soil water potential to soil water content using
van Genuchten 1980 \cite vanGenuchten1980 Soil Water Retention Curve

Parameters are explained in `SWRC_check_parameters_for_vanGenuchten1980()`.

@note
`SWRC_SWPtoSWC_vanGenuchten1980()` and `SWRC_SWCtoSWP_vanGenuchten1980()`
are the inverse of each other for `(phi, theta)` between
`(0, theta_sat)` and `(infinity, theta_min)`.

@param[in] swpMatric Soil water potential [-bar]
@param[in] *swrcp Vector of SWRC parameters
@param[in] gravel Coarse fragments (> 2 mm; e.g., gravel)
    of the whole soil [m3/m3]
@param[in] width Soil layer width [cm]

@return Soil water content in the layer [cm]
*/
double SWRC_SWPtoSWC_vanGenuchten1980(
    double swpMatric, const double *swrcp, double gravel, double width
) {
    double phi;
    double tmp;
    double res;

    // convert SWP [-bar] to phi [cm of H2O at 4 C;
    // value from `soilDB::KSSL_VG_model()`]
    phi = swpMatric * 1019.716;

    tmp = powe(swrcp[2] * phi, swrcp[3]);
    tmp = powe(1. + tmp, 1. - 1. / swrcp[3]);

    // calculate matric theta [cm / cm] which is within [theta_min, theta_sat]
    res = swrcp[0] + (swrcp[1] - swrcp[0]) / tmp;

    // convert matric theta [cm / cm] to bulk SWC [cm]
    return (1. - gravel) * width * res;
}

/**
@brief Convert soil water potential to soil water content using
FXW Soil Water Retention Curve \cite fredlund1994CGJa, \cite wang2018wrr

Parameters are explained in `SWRC_check_parameters_for_FXW()`.

@note `SWRC_SWPtoSWC_FXW()` and `SWRC_SWCtoSWP_FXW()`
are the inverse of each other for `(phi, theta)` between
`(0, theta_sat)` and `(6.3 x 10^6 cm, 0)`.

@param[in] swpMatric Soil water potential [-bar]
@param[in] *swrcp Vector of SWRC parameters
@param[in] gravel Coarse fragments (> 2 mm; e.g., gravel)
    of the whole soil [m3/m3]
@param[in] width Soil layer width [cm]

@return Soil water content in the layer [cm]
*/
double SWRC_SWPtoSWC_FXW(
    double swpMatric, double *swrcp, double gravel, double width
) {
    double phi;
    double res;

    // convert SWP [-bar] to phi [cm of H2O at 4 C;
    // value from `soilDB::KSSL_VG_model()`]
    phi = swpMatric * 1019.716;

    res = FXW_phi_to_theta(phi, swrcp);

    // convert matric theta [cm / cm] to bulk SWC [cm]
    return (1. - gravel) * width * res;
}
