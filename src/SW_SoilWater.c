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
 10/04/2010	(drs) added snowMAUS snow accumulation, sublimation and melt algorithm: Trnka, M., Kocmánková, E., Balek, J., Eitzinger, J., Ruget, F., Formayer, H., Hlavinka, P., Schaumberger, A., Horáková, V., Mozny, M. & Zalud, Z. (2010) Simple snow cover model for agrometeorological applications. Agricultural and Forest Meteorology, 150, 1115-1127.
 replaced SW_SWC_snow_accumulation, SW_SWC_snow_sublimation, and SW_SWC_snow_melt with SW_SWC_adjust_snow (temp_min, temp_max, *ppt, *rain, *snow, *snowmelt)
 10/19/2010	(drs) replaced snowMAUS simulation with SWAT2K routines: Neitsch S, Arnold J, Kiniry J, Williams J. 2005. Soil and water assessment tool (SWAT) theoretical documentation. version 2005. Blackland Research Center, Texas Agricultural Experiment Station: Temple, TX.
 10/25/2010	(drs)	in SW_SWC_water_flow(): replaced test that "swc can't be adjusted on day 1 of year 1" to "swc can't be adjusted on start day of first year of simulation"
 01/04/2011	(drs) added parameter '*snowloss' to function SW_SWC_adjust_snow()
 08/22/2011	(drs) added function RealD SW_SnowDepth( RealD SWE, RealD snowdensity)
 01/20/2012	(drs)	in function 'SW_SnowDepth': catching division by 0 if snowdensity is 0
 02/03/2012	(drs)	added function 'RealD SW_SWC_SWCres(RealD sand, RealD clay, RealD porosity)': which calculates 'Brooks-Corey' residual volumetric soil water based on Rawls & Brakensiek (1985)
 05/25/2012  (DLM) edited SW_SWC_read(void) function to get the initial values for soil temperature from SW_Site
 04/16/2013	(clk)	Changed SW_SWC_vol2bars() to SW_SWCbulk2SWPmatric(), and changed the code to incorporate the fraction of gravel content in the soil
 theta1 = ... / (1. - fractionVolBulk_gravel)
 Changed SW_SWC_bars2vol() to SW_SWPmatric2VWCBulk(), and changed the code to incorporate the fraction of gravel content in the soil
 t = ... * (1. - fractionVolBulk_gravel)
 Changed SW_SWC_SWCres() to SW_VWCBulkRes(), and changed the code to incorporate the fraction of gravel content in the soil
 res = ... * (1. - fractionVolBulk_gravel)
 Updated the use of these three function in all files
 06/24/2013	(rjm)	made temp_snow a module-level static variable (instead of function-level): otherwise it will not get reset to 0 between consecutive calls as a dynamic library
 need to set temp_snow to 0 in function SW_SWC_construct()
 06/26/2013	(rjm)	closed open files at end of functions SW_SWC_read(), _read_hist() or if LogError() with LOGFATAL is called
 */
/********************************************************/
/********************************************************/

/* =================================================== */
/* =================================================== */
/*                INCLUDES / DEFINES                   */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "include/generic.h"
#include "include/filefuncs.h"
#include "include/myMemory.h"
#include "include/SW_Defines.h"
#include "include/SW_Files.h"
#include "include/SW_Model.h" // externs SW_Model
#include "include/SW_Site.h" // externs SW_Site
#include "include/SW_Flow.h"
#include "include/SW_SoilWater.h"
#include "include/SW_VegProd.h" // externs SW_VegProd
#ifdef SWDEBUG
  #include "include/SW_Weather.h"   // externs SW_Weather
#endif
#ifdef RSOILWAT
  #include "rSW_SoilWater.h" // for onSet_SW_SWC_hist()
  #include "SW_R_lib.h" // externs `useFiles`
#endif



/* =================================================== */
/*                  Global Variables                   */
/* --------------------------------------------------- */

SW_SOILWAT SW_Soilwat;



/* =================================================== */
/*                  Local Variables                    */
/* --------------------------------------------------- */
static char *MyFileName;
static RealD temp_snow;



/* =================================================== */
/*             Local Function Definitions              */
/* --------------------------------------------------- */

static void _clear_hist(void) {
	TimeInt d;
	LyrIndex z;
	for (d = 0; d < MAX_DAYS; d++) {
		for(z=0; z<MAX_LAYERS; z++)
		{
			SW_Soilwat.hist.swc[d][z] = SW_MISSING;
			SW_Soilwat.hist.std_err[d][z] = SW_MISSING;
		}
	}
}

static void _reset_swc(void) {
	LyrIndex lyr;

	/* reset swc */
	ForEachSoilLayer(lyr)
	{
		SW_Soilwat.swcBulk[Today][lyr] = SW_Soilwat.swcBulk[Yesterday][lyr] = SW_Site.lyr[lyr]->swcBulk_init;
		SW_Soilwat.drain[lyr] = 0.;
	}

	/* reset the snowpack */
	SW_Soilwat.snowpack[Today] = SW_Soilwat.snowpack[Yesterday] = 0.;

	/* reset deep drainage */
	if (SW_Site.deepdrain) {
		SW_Soilwat.swcBulk[Today][SW_Site.deep_lyr] = 0.;
  }
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
static double FXW_phi_to_theta(
	double phi,
	double *swrcp
) {
	double tmp, res, S_e, C_f;

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
  \brief Interpolate, Truncate, Project (ITP) method (\cite oliveira2021ATMS) to solve FXW at a given water content

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

  [Additional info on the ITP method can be found here](https://en.wikipedia.org/wiki/ITP_method)


  \param[in] theta Volumetric soil water content of the matric soil [cm / cm]
  \param[in] *swrcp Vector of FXW SWRC parameters

  \return Water tension [bar] corresponding to theta
*/

static double itp_FXW_for_phi(double theta, double *swrcp) {
	double
		tol2 = 2e-9, // tolerance convergence
		a = 0., // lower bound of bracket: a < b && f(a) < 0
		b = FXW_h0, // upper bound of bracket: b > a && f(b) > 0
		diff_ba = FXW_h0, diff_hf,
		x_f, x_t, x_itp,
		y_a, y_b, y_itp,
		k1, k2,
		x_half,
		r,
		delta,
		sigma,
		phi;
	int
		j = 0, n0, n_max;

	#ifdef SWDEBUG
	short debug = 0;
	#endif


	// Evaluate at starting bracket (and checks)
	y_a = theta - FXW_phi_to_theta(a, swrcp);
	y_b = theta - FXW_phi_to_theta(b, swrcp);


	// Set hyper-parameters
	k1 = 3.174603e-08; // 0 < k1 = 0.2 / (b - a) < inf
	/*
		k1 = 2. converges in about 31-33 iterations
		k1 = 0.2 converges in 28-30 iterations
		k1 = 2.e-2 converges in 25-27 iterations
		k1 = 2.e-3 converges in 22-24 iterations
		k1 = 2.e-4 converges in about 20 iterations but fails for some
		k1 = 2.e-6 converges in about 14 iterations or near n_max or fails
		k1 = 0.2 / (b - a) = 3.174603e-08 (suggested value) converges
				in about 8 iterations (for very dry soils) or near n_max = 53 or fails
	*/
	k1 = 2.e-3;
	k2 = 2.; // 1 <= k2 < (3 + sqrt(5))/2
	n0 = 1; // 0 < n0 < inf

	// Upper limit of iterations before convergence
	n_max = (int) ceil(log2(diff_ba / tol2)) + n0;

	#ifdef SWDEBUG
	if (debug) {
		swprintf(
			"\nitp_FXW_for_phi(theta=%f): init: "
			"f(a)=%f, f(b)=%f, n_max=%d, k1=%f\n",
			theta, y_a, y_b, n_max, k1
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
			swprintf(
				"j=%d:"
				"\ta=%f, b=%f, y_a=%f, y_b=%f,\n"
				"\tr=%f, delta=%f, x_half=%f, x_f=%f, x_t=%f (s=%.0f),\n"
				"\tx_itp=%f, y_itp=%f\n",
				j,
				a, b, y_a, y_b,
				r, delta, x_half, x_f, x_t, sigma,
				x_itp, y_itp
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

	if (j > n_max + 1 || fabs(diff_ba) > tol2 || LT(phi, 0.) || GT(phi, FXW_h0)) {
		// Error if not converged or if `phi` physically impossible
		LogError(
			logfp,
			LOGERROR,
			"itp_FXW_for_phi(theta = %f): convergence failed at phi = %.9f [cm H2O] "
			"after %d iterations with b - a = %.9f - %.9f = %.9f !<= tol2 = %.9f.",
			theta, phi, j, b, a, b - a, tol2
		);

		phi = SW_MISSING;

	} else {
		// convert [cm of H2O at 4 C] to [bar]
		phi /= 1019.716;
	}


	#ifdef SWDEBUG
	if (debug) {
		if (!missing(phi)) {
			swprintf(
				"itp_FXW_for_phi() = phi = %f [bar]: converged in j=%d/%d steps\n",
				phi, j, n_max
			);
		} else {
			swprintf(
				"itp_FXW_for_phi(): failed to converged in j=%d/%d steps\n",
				j, n_max
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
void SW_WaterBalance_Checks(void)
{
  SW_SOILWAT *sw = &SW_Soilwat;
	SW_WEATHER *w = &SW_Weather;

  IntUS i, k;
  int debugi[N_WBCHECKS] = {1, 1, 1, 1, 1, 1, 1, 1, 1}; // print output for each check yes/no
  char flag[16];
  RealD
    Etotal, Etotalsurf, Etotalint, Eponded, Elitter, Esnow, Esoil = 0., Eveg = 0.,
    Ttotal = 0., Ttotalj[MAX_LAYERS],
    percolationIn[MAX_LAYERS + 1], percolationOut[MAX_LAYERS + 1],
    hydraulicRedistribution[MAX_LAYERS],
    infiltration, deepDrainage, runoff, runon, snowmelt, rain, arriving_water,
    intercepted, int_veg_total = 0.,
    delta_surfaceWater,
    delta_swc_total = 0., delta_swcj[MAX_LAYERS];
  RealD lhs, rhs, wbtol = 1e-9;

  static RealD surfaceWater_yesterday;
  static Bool debug = swFALSE;


  // re-init static variables on first day of each simulation
  // to prevent carry-over
  if (SW_Model.year == SW_Model.startyr && SW_Model.doy == SW_Model.firstdoy) {
    surfaceWater_yesterday = 0.;
  }

  // Sum up variables
  ForEachSoilLayer(i)
  {
    percolationIn[i + 1] = sw->drain[i];
    percolationOut[i] = sw->drain[i];

    delta_swcj[i] = sw->swcBulk[Today][i] - sw->swcBulk[Yesterday][i];
    delta_swc_total += delta_swcj[i];

    Ttotalj[i] = hydraulicRedistribution[i] = 0.; // init

    ForEachVegType(k) {
      Ttotal += sw->transpiration[k][i];
      Ttotalj[i] += sw->transpiration[k][i];
      hydraulicRedistribution[i] += sw->hydred[k][i];
    }
  }

  ForEachEvapLayer(i)
  {
    Esoil += sw->evaporation[i];
  }

  ForEachVegType(k)
  {
    Eveg += sw->evap_veg[k];
    int_veg_total += sw->int_veg[k];
  }

  // Get evaporation values
  Elitter = sw->litter_evap;
  Eponded = sw->surfaceWater_evap;
  Esnow = w->snowloss;
  Etotalint = Eveg + Elitter;
  Etotalsurf = Etotalint + Eponded;
  Etotal = Etotalsurf + Esoil + Esnow;

  // Get other water flux values
  infiltration = w->soil_inf;
  deepDrainage = sw->swcBulk[Today][SW_Site.deep_lyr]; // see issue #137

  percolationIn[0] = infiltration;
  percolationOut[SW_Site.n_layers] = deepDrainage;

  runoff = w->snowRunoff + w->surfaceRunoff;
  runon = w->surfaceRunon;
  snowmelt = w->snowmelt;
  rain = w->now.rain;

  arriving_water = rain + snowmelt + runon;

  // Get state change values
  intercepted = sw->litter_int + int_veg_total;

  delta_surfaceWater = sw->surfaceWater - surfaceWater_yesterday;
  surfaceWater_yesterday = sw->surfaceWater;


  //--- Water balance checks (there are # checks n = N_WBCHECKS)
  if (!sw->is_wbError_init) {
    for (i = 0; i < N_WBCHECKS; i++) {
      debug = (debug || debugi[i])? swTRUE: swFALSE;
    }
  }

  if (debug) {
    snprintf(flag, sizeof flag, "WB (%d-%d)", SW_Model.year, SW_Model.doy);
  }


  // AET <= PET
  if (!sw->is_wbError_init) {
    sw->wbErrorNames[0] = Str_Dup("AET <= PET");
  }
  if (!LE(sw->aet, sw->pet))
  {
    sw->wbError[0]++;
    if (debugi[0]) swprintf("%s: aet=%f, pet=%f\n",
      flag, sw->aet, sw->pet);
  }

  // AET == E(total) + T(total)
  if (!sw->is_wbError_init) {
    sw->wbErrorNames[1] = Str_Dup("AET == Etotal + Ttotal");
  }
  rhs = Etotal + Ttotal;
  if (!EQ_w_tol(sw->aet, rhs, wbtol))
  {
    sw->wbError[1]++;
    if (debugi[1]) swprintf("%s: AET(%f) == %f == Etotal(%f) + Ttotal(%f)\n",
      flag, sw->aet, rhs, Etotal, Ttotal);
  }

  // T(total) = sum of T(veg-type i from soil layer j)
  // doesn't make sense here because Ttotal is the sum of Tvegij
  if (!sw->is_wbError_init) {
    sw->wbErrorNames[2] = Str_Dup("T(total) = sum of T(veg-type i from soil layer j)");
  }
  sw->wbError[2] += 0;

  // E(total) = E(total bare-soil) + E(ponded water) + E(total litter-intercepted) +
  //            + E(total veg-intercepted) + E(snow sublimation)
  if (!sw->is_wbError_init) {
    sw->wbErrorNames[3] = Str_Dup("Etotal == Esoil + Eponded + Eveg + Elitter + Esnow");
  }
  rhs = Esoil + Eponded + Eveg + Elitter + Esnow;
  if (!EQ_w_tol(Etotal, rhs, wbtol))
  {
    sw->wbError[3]++;
    if (debugi[3]) swprintf("%s: Etotal(%f) == %f == Esoil(%f) + Eponded(%f) + Eveg(%f) + Elitter(%f) + Esnow(%f)\n",
      flag, Etotal, rhs, Esoil, Eponded, Eveg, Elitter, Esnow);
  }

  // E(total surface) = E(ponded water) + E(total litter-intercepted) +
  //                    + E(total veg-intercepted)
  if (!sw->is_wbError_init) {
    sw->wbErrorNames[4] = Str_Dup("Esurf == Eponded + Eveg + Elitter");
  }
  rhs = Eponded + Eveg + Elitter;
  if (!EQ_w_tol(Etotalsurf, rhs, wbtol))
  {
    sw->wbError[4]++;
    if (debugi[4]) swprintf("%s: Esurf(%f) == %f == Eponded(%f) + Eveg(%f) + Elitter(%f)\n",
      flag, Etotalsurf, rhs, Eponded, Eveg, Elitter);
  }


  //--- Water cycling checks
  // infiltration = [rain + snowmelt + runon] - (runoff + intercepted + delta_surfaceWater + Eponded)
  if (!sw->is_wbError_init) {
    sw->wbErrorNames[5] = Str_Dup("inf == rain + snowmelt + runon - (runoff + intercepted + delta_surfaceWater + Eponded)");
  }
  rhs = arriving_water - (runoff + intercepted + delta_surfaceWater + Eponded);
  if (!EQ_w_tol(infiltration, rhs, wbtol))
  {
    sw->wbError[5]++;
    if (debugi[5]) swprintf("%s: inf(%f) == %f == rain(%f) + snowmelt(%f) + runon(%f) - (runoff(%f) + intercepted(%f) + delta_surfaceWater(%f) + Eponded(%f))\n",
      flag, infiltration, rhs, rain, snowmelt, runon, runoff, intercepted, delta_surfaceWater, Eponded);
  }

  // E(soil) + Ttotal = infiltration - (deepDrainage + delta(swc))
  if (!sw->is_wbError_init) {
    sw->wbErrorNames[6] = Str_Dup("Ttotal + Esoil = inf - (deepDrainage + delta_swc)");
  }
  lhs = Ttotal + Esoil;
  rhs = infiltration - (deepDrainage + delta_swc_total);
  if (!EQ_w_tol(lhs, rhs, wbtol))
  {
    sw->wbError[6]++;
    if (debugi[6]) swprintf("%s: Ttotal(%f) + Esoil(%f) == %f == %f == inf(%f) - (deepDrainage(%f) + delta_swc(%f))\n",
      flag, Ttotal, Esoil, lhs, rhs, infiltration, deepDrainage, delta_swc_total);
  }

  // for every soil layer j: delta(swc) =
  //   = infiltration/percolationIn + hydraulicRedistribution -
  //     (percolationOut/deepDrainage + transpiration + evaporation)
  if (!sw->is_wbError_init) {
    sw->wbErrorNames[7] = Str_Dup("delta_swc[i] == perc_in[i] + hydred[i] - (perc_out[i] + Ttot[i] + Esoil[i]))");
  }
  ForEachSoilLayer(i)
  {
    rhs = percolationIn[i] + hydraulicRedistribution[i] -
      (percolationOut[i] + Ttotalj[i] + sw->evaporation[i]);
    if (!EQ_w_tol(delta_swcj[i], rhs, wbtol))
    {
      sw->wbError[7]++;
      if (debugi[7]) swprintf("%s sl=%d: delta_swc(%f) == %f == perc_in(%f) + hydred(%f) - (perc_out(%f) + Ttot(%f) + Esoil(%f))\n",
        flag, i, delta_swcj[i], rhs, percolationIn[i], hydraulicRedistribution[i],
        percolationOut[i], Ttotalj[i], sw->evaporation[i]);
    }
  }

  // for every soil layer j: swc_min <= swc <= swc_sat
  if (!sw->is_wbError_init) {
    sw->wbErrorNames[8] = Str_Dup("swc[i] => swc_min[i] && swc[i] <= swc_sat[i]");
  }
  ForEachSoilLayer(i)
  {
    if (
      LT(sw->swcBulk[Today][i], SW_Site.lyr[i]->swcBulk_min) ||
      GT(sw->swcBulk[Today][i], SW_Site.lyr[i]->swcBulk_saturated)
    ) {
      sw->wbError[8]++;
      if (debugi[8]) {
        swprintf(
          "%s sl=%d: swc_min(%f) <= swc(%f) <= swc_sat(%f)\n",
          flag, i,
          SW_Site.lyr[i]->swcBulk_min,
          sw->swcBulk[Today][i],
          SW_Site.lyr[i]->swcBulk_saturated
        );
      }
    }
  }


  // Setup only once
  if (!sw->is_wbError_init) {
    sw->is_wbError_init = swTRUE;
  }
}
#endif


/**
@brief Constructor for soilWater Content.
*/
void SW_SWC_construct(void) {
	/* =================================================== */
	OutPeriod pd;

	// Clear memory before setting it
	if (!isnull(SW_Soilwat.hist.file_prefix)) {
		Mem_Free(SW_Soilwat.hist.file_prefix);
		SW_Soilwat.hist.file_prefix = NULL;
	}

	// Clear the module structure:
	memset(&SW_Soilwat, 0, sizeof(SW_SOILWAT));

	// Allocate output structures:
	ForEachOutPeriod(pd)
	{
		SW_Soilwat.p_accu[pd] = (SW_SOILWAT_OUTPUTS *) Mem_Calloc(1,
			sizeof(SW_SOILWAT_OUTPUTS), "SW_SWC_construct()");
		if (pd > eSW_Day) {
			SW_Soilwat.p_oagg[pd] = (SW_SOILWAT_OUTPUTS *) Mem_Calloc(1,
				sizeof(SW_SOILWAT_OUTPUTS), "SW_SWC_construct()");
		}
	}
}

void SW_SWC_deconstruct(void)
{
	OutPeriod pd;

	// De-allocate output structures:
	ForEachOutPeriod(pd)
	{
		if (pd > eSW_Day && !isnull(SW_Soilwat.p_oagg[pd])) {
			Mem_Free(SW_Soilwat.p_oagg[pd]);
			SW_Soilwat.p_oagg[pd] = NULL;
		}

		if (!isnull(SW_Soilwat.p_accu[pd])) {
			Mem_Free(SW_Soilwat.p_accu[pd]);
			SW_Soilwat.p_accu[pd] = NULL;
		}
	}

	if (!isnull(SW_Soilwat.hist.file_prefix)) {
		Mem_Free(SW_Soilwat.hist.file_prefix);
		SW_Soilwat.hist.file_prefix = NULL;
	}

	if (!isnull(SW_Soilwat.hist.file_prefix)) {
		Mem_Free(SW_Soilwat.hist.file_prefix);
		SW_Soilwat.hist.file_prefix = NULL;
	}

	#ifdef SWDEBUG
	IntU i;

	for (i = 0; i < N_WBCHECKS; i++)
	{
		if (!isnull(SW_Soilwat.wbErrorNames[i])) {
			Mem_Free(SW_Soilwat.wbErrorNames[i]);
			SW_Soilwat.wbErrorNames[i] = NULL;
		}
	}
	#endif
}
/**
@brief Adjust SWC according to historical (measured) data if available, compute
    water flow, and check if swc is above threshold for "wet" condition.
*/
void SW_SWC_water_flow(void) {
	/* =================================================== */


	LyrIndex i;
  #ifdef SWDEBUG
  int debug = 0;
  #endif

	/* if there's no swc observation for today,
	 * it shows up as SW_MISSING.  The input must
	 * define historical swc for at least the top
	 * layer to be recognized.
	 * IMPORTANT: swc can't be adjusted on day 1 of first year of simulation.
	 10/25/2010	(drs)	in SW_SWC_water_flow(): replaced test that "swc can't be adjusted on day 1 of year 1" to "swc can't be adjusted on start day of first year of simulation"
	 */

	if (SW_Soilwat.hist_use && !missing( SW_Soilwat.hist.swc[SW_Model.doy-1][1])) {

		if (!(SW_Model.doy == SW_Model.startstart && SW_Model.year == SW_Model.startyr)) {

      #ifdef SWDEBUG
      if (debug) swprintf("\n'SW_SWC_water_flow': adjust SWC from historic inputs.\n");
      #endif
      SW_SWC_adjust_swc(SW_Model.doy);

		} else {
			LogError(logfp, LOGWARN, "Attempt to set SWC on start day of first year of simulation disallowed.");
		}

	} else {
    #ifdef SWDEBUG
    if (debug) swprintf("\n'SW_SWC_water_flow': call 'SW_Water_Flow'.\n");
    #endif
		SW_Water_Flow();
	}

  #ifdef SWDEBUG
  if (debug) swprintf("\n'SW_SWC_water_flow': check water balance.\n");
  SW_WaterBalance_Checks();

  if (debug) swprintf("\n'SW_SWC_water_flow': determine wet soil layers.\n");
  #endif
	ForEachSoilLayer(i)
		SW_Soilwat.is_wet[i] = (Bool) (GE( SW_Soilwat.swcBulk[Today][i],
				SW_Site.lyr[i]->swcBulk_wet));
}

/**
@brief Sets up the structures that will hold the available soil water partitioned
    among vegetation types and propagate the swa_master structure for use in get_dSWAbulk().
    Must be call after `SW_SWC_water_flow()` is executed.
*/
/***********************************************************/
void calculate_repartitioned_soilwater(void){
  // this will run for every day of every year
  SW_SOILWAT *v = &SW_Soilwat;
	LyrIndex i;
  RealD val = SW_MISSING;
  int j, k;
  float curr_crit_val, new_crit_val;

  ForEachSoilLayer(i){
    val = v->swcBulk[Today][i];
    ForEachVegType(j){
      if(SW_VegProd.veg[j].cov.fCover != 0)
        v->swa_master[j][j][i] = fmax(0., val - SW_Site.lyr[i]->swcBulk_atSWPcrit[j]);
      else
        v->swa_master[j][j][i] = 0.;
      v->dSWA_repartitioned_sum[j][i] = 0.; // need to reset to 0 each time
    }

    // need to check which other critical value each veg_type has access to aside from its own
    //(i.e. if shrub=-3.9 then it also has access to -3.5 and -2.0)
    // go through each veg type
    for(j=0; j<NVEGTYPES; j++){
      curr_crit_val = SW_VegProd.critSoilWater[j]; // get critical value for current veg type
      // go through each critical value to see which ones need to be set for each veg_type
      for(k=0; k<NVEGTYPES; k++){
        if(k == j){
          // dont need to check for its own critical value
        }
        else{
          new_crit_val = SW_VegProd.critSoilWater[k];
          if(curr_crit_val < new_crit_val){ // need to store this value since it has access to it
            v->swa_master[j][k][i] = v->swa_master[k][k][i]; // itclp(veg_type, new_critical_value, layer, timeperiod)
          }
          if(curr_crit_val > new_crit_val){ // need to set this value to 0 since it does not have access to it
            v->swa_master[j][k][i] = 0.; // itclp(veg_type, new_critical_value, layer, timeperiod)
          }
          if(curr_crit_val == new_crit_val){
            // do nothing, dont want to swap values
          }
        }
      }
    }
    get_dSWAbulk(i); // call function to get repartioned swa values
  }
}


/**
@brief This function calculates the repartioned soilwater.
      soilwater for each vegtype is calculated based on size of the critical soilwater based on the input files.
      This goes through the ranked critical values, starting at the deepest and moving up
      The deepest veg type has access to the available soilwater of each veg type above so start at bottom move up.
@param i Integer value for soil layer
*/
/***********************************************************/
void get_dSWAbulk(int i){
  SW_SOILWAT *v = &SW_Soilwat;
	int j,kv,curr_vegType,curr_crit_rank_index,kv_veg_type,prev_crit_veg_type,greater_veg_type;
	float crit_val, prev_crit_val, smallestCritVal, vegFractionSum, newFraction;
	float veg_type_in_use; // set to current veg type fraction value to avoid multiple if loops. should just need 1 instead of 3 now.
	float inner_loop_veg_type; // set to inner loop veg type
	smallestCritVal = SW_VegProd.critSoilWater[SW_VegProd.rank_SWPcrits[0]];
	RealF dSWA_bulk[NVEGTYPES * NVEGTYPES][NVEGTYPES * NVEGTYPES][MAX_LAYERS];
	RealF dSWA_bulk_repartioned[NVEGTYPES * NVEGTYPES][NVEGTYPES * NVEGTYPES][MAX_LAYERS];

  // need to initialize to 0
  for(curr_vegType = 0; curr_vegType < NVEGTYPES; curr_vegType++){
		for(kv = 0; kv < NVEGTYPES; kv++){
      dSWA_bulk_repartioned[curr_vegType][kv][i] = 0.;
      dSWA_bulk[curr_vegType][kv][i] = 0.;
    }
  }

	// loop through each veg type to get dSWAbulk
	for(curr_vegType = (NVEGTYPES - 1); curr_vegType >= 0; curr_vegType--){ // go through each veg type and recalculate if necessary. starts at smallest
    v->dSWA_repartitioned_sum[curr_vegType][i] = 0.;
    curr_crit_rank_index = SW_VegProd.rank_SWPcrits[curr_vegType]; // get rank index for start of next loop
		veg_type_in_use = SW_VegProd.veg[curr_crit_rank_index].cov.fCover; // set veg type fraction here

		for(kv = curr_vegType; kv>=0; kv--){
			crit_val = SW_VegProd.critSoilWater[SW_VegProd.rank_SWPcrits[kv]]; // get crit value at current index
			kv_veg_type = SW_VegProd.rank_SWPcrits[kv]; // get index for veg_type. dont want to access v->swa_master at rank_SWPcrits index
			if(kv != 0){
				prev_crit_val = SW_VegProd.critSoilWater[SW_VegProd.rank_SWPcrits[kv-1]]; // get crit value for index lower
				prev_crit_veg_type = SW_VegProd.rank_SWPcrits[kv-1]; // get veg type that belongs to the corresponding critical value
			}
			else{
				prev_crit_val = SW_VegProd.critSoilWater[SW_VegProd.rank_SWPcrits[kv]]; // get crit value for index lower
				prev_crit_veg_type = SW_VegProd.rank_SWPcrits[kv]; // get veg type that belongs to the corresponding critical value
			}
			if(veg_type_in_use == 0){ // [0=tree(-2.0,off), 1=shrub(-3.9,on), 2=grass(-3.5,on), 3=forb(-2.0,on)]
				dSWA_bulk[curr_crit_rank_index][kv_veg_type][i] = 0.; // set to 0 to ensure no absent values
				v->swa_master[curr_crit_rank_index][kv_veg_type][i] = 0.;
				dSWA_bulk_repartioned[curr_crit_rank_index][kv_veg_type][i] = 0.;
			}
			else{ // check if need to recalculate for veg types in use
				if(crit_val < prev_crit_val){ // if true then we need to recalculate
					if(v->swa_master[curr_crit_rank_index][kv_veg_type][i] == 0){
						dSWA_bulk[curr_crit_rank_index][kv_veg_type][i] = 0.;
					}
					else{
						dSWA_bulk[curr_crit_rank_index][kv_veg_type][i] =
							v->swa_master[curr_crit_rank_index][kv_veg_type][i] - v->swa_master[curr_crit_rank_index][prev_crit_veg_type][i];
						}
				}
				else if(crit_val == prev_crit_val){ // critical values equal just set to itself
					dSWA_bulk[curr_crit_rank_index][kv_veg_type][i] = v->swa_master[curr_crit_rank_index][kv_veg_type][i];
				}
				else{
					// do nothing if crit val >. this will be handled later
				}
				/* ##############################################################
					below if else blocks are for redistributing dSWAbulk values
				############################################################## */
				if(curr_vegType == (NVEGTYPES - 1) && kv == (NVEGTYPES - 1) && prev_crit_val != crit_val) // if largest critical value and only veg type with that value just set it to dSWAbulk
					dSWA_bulk_repartioned[curr_crit_rank_index][kv_veg_type][i] = dSWA_bulk[curr_crit_rank_index][kv_veg_type][i];
				else{ // all values other than largest well need repartitioning
					if(crit_val == smallestCritVal){ // if smallest value then all veg_types have access to it so just need to multiply by its fraction
						dSWA_bulk_repartioned[curr_crit_rank_index][kv_veg_type][i] =
							dSWA_bulk[curr_crit_rank_index][kv_veg_type][i] * veg_type_in_use;
							// multiply by fraction for index of curr_vegType not kv
					}
					else{ // critical values that more than one veg type have access to but less than all veg types
						vegFractionSum = 0; // set to 0 each time to make sure it gets correct value
						// need to calculute new fraction for these since sum of them no longer adds up to 1. do this by adding fractions values
						// of veg types who have access and then dividing these fraction values by the total sum. keeps the ratio and adds up to 1
						for(j = (NVEGTYPES - 1); j >= 0; j--){ // go through all critical values and sum the fractions of the veg types who have access
							inner_loop_veg_type = SW_VegProd.veg[j].cov.fCover; // set veg type fraction here

							if(SW_VegProd.critSoilWater[j] <= crit_val)
								vegFractionSum += inner_loop_veg_type;
						}
						newFraction = veg_type_in_use / vegFractionSum; // divide veg fraction by sum to get new fraction value
						dSWA_bulk_repartioned[curr_crit_rank_index][kv_veg_type][i] =
							dSWA_bulk[curr_crit_rank_index][kv_veg_type][i] * newFraction;
					}
				}
			}
		}
		// setting all the veg types above current to 0 since they do not have access to those
		// ex: if forb=-2.0 grass=-3.5 & shrub=-3.9 then need to set grass and shrub to 0 for forb
		for(j = curr_vegType + 1; j < NVEGTYPES; j++){
			greater_veg_type = SW_VegProd.rank_SWPcrits[j];
			if(SW_VegProd.critSoilWater[SW_VegProd.rank_SWPcrits[j-1]] > SW_VegProd.critSoilWater[SW_VegProd.rank_SWPcrits[j]]){
				dSWA_bulk[curr_crit_rank_index][greater_veg_type][i] = 0.;
				dSWA_bulk_repartioned[curr_crit_rank_index][greater_veg_type][i] = 0.;
			}
		}
	}

	for(curr_vegType = 0; curr_vegType < NVEGTYPES; curr_vegType++){
		for(kv = 0; kv < NVEGTYPES; kv++){
			if(SW_VegProd.veg[curr_vegType].cov.fCover == 0.)
				v->dSWA_repartitioned_sum[curr_vegType][i] = 0.;
			else
				v->dSWA_repartitioned_sum[curr_vegType][i] += dSWA_bulk_repartioned[curr_vegType][kv][i];
		}
	}

    /*int x = 0;
		printf("dSWAbulk_repartition forb[%d,0,%d]: %f\n",x,i,dSWA_bulk_repartioned[x][0][i]);
		printf("dSWAbulk_repartition forb[%d,1,%d]: %f\n",x,i,dSWA_bulk_repartioned[x][1][i]);
		printf("dSWAbulk_repartition forb[%d,2,%d]: %f\n",x,i,dSWA_bulk_repartioned[x][2][i]);
		printf("dSWAbulk_repartition forb[%d,3,%d]: %f\n\n",x,i,dSWA_bulk_repartioned[x][3][i]);
		printf("dSWA_repartitioned_sum[%d][%d]: %f\n\n", x,i, v->dSWA_repartitioned_sum[x][i]);


		printf("---------------------\n\n");*/
}
/**
@brief Copies today's values so that the values for swcBulk and snowpack become yesterday's values.
*/
void SW_SWC_end_day(void) {
	/* =================================================== */
	SW_SOILWAT *v = &SW_Soilwat;
	LyrIndex i;

	ForEachSoilLayer(i)
		v->swcBulk[Yesterday][i] = v->swcBulk[Today][i];

	v->snowpack[Yesterday] = v->snowpack[Today];

}

void SW_SWC_init_run(void) {

	SW_Soilwat.soiltempError = swFALSE;

	#ifdef SWDEBUG
		SW_Soilwat.is_wbError_init = swFALSE;
	#endif

	temp_snow = 0.; // module-level snow temperature

  _reset_swc();
}

/**
@brief init first doy swc, either by the computed init value or by the last day of last
      year, which is also, coincidentally, Yesterday
*/
void SW_SWC_new_year(void) {

	LyrIndex lyr;
	TimeInt year = SW_Model.year;

  if (SW_Site.reset_yr) {
    _reset_swc();

  } else {
    /* update swc */
    ForEachSoilLayer(lyr)
    {
      SW_Soilwat.swcBulk[Today][lyr] = SW_Soilwat.swcBulk[Yesterday][lyr];
    }

    /* update snowpack */
    SW_Soilwat.snowpack[Today] = SW_Soilwat.snowpack[Yesterday];
  }

	/* update historical (measured) values, if needed */
	if (SW_Soilwat.hist_use && year >= SW_Soilwat.hist.yr.first) {
		#ifndef RSOILWAT
			_read_swc_hist(year);
		#else
			if (useFiles) {
				_read_swc_hist(year);
			} else {
				onSet_SW_SWC_hist();
			}
		#endif
	}
}

/**
@brief Like all of the other functions, read() reads in the setup parameters. See
      _read_swc_hist() for reading historical files.
*/
void SW_SWC_read(void) {
	/* =================================================== */
	/* HISTORY
	 *  1/25/02 - cwb - removed unused records of logfile and
	 *          start and end days.  also removed the SWTIMES dy
	 *          structure element.
	 */

	SW_SOILWAT *v = &SW_Soilwat;
	FILE *f;
	int lineno = 0, nitems = 4;
// gets the soil temperatures from where they are read in the SW_Site struct for use later
// SW_Site.c must call it's read function before this, or it won't work
	LyrIndex i;
	ForEachSoilLayer(i)
		v->avgLyrTemp[i] = SW_Site.lyr[i]->avgLyrTemp;

	MyFileName = SW_F_name(eSoilwat);
	f = OpenFile(MyFileName, "r");

	while (GetALine(f, inbuf)) {
		switch (lineno) {
		case 0:
			v->hist_use = (atoi(inbuf)) ? swTRUE : swFALSE;
			break;
		case 1:
			v->hist.file_prefix = (char *) Str_Dup(inbuf);
			break;
		case 2:
			v->hist.yr.first = yearto4digit(atoi(inbuf));
			break;
		case 3:
			v->hist.method = atoi(inbuf);
			break;
		}
		lineno++;
	}
	if(!v->hist_use) {
		CloseFile(&f);
		return;
	}
	if (lineno < nitems) {
		CloseFile(&f);
		LogError(logfp, LOGFATAL, "%s : Insufficient parameters specified.", MyFileName);
	}
	if (v->hist.method < 1 || v->hist.method > 2) {
		CloseFile(&f);
		LogError(logfp, LOGFATAL, "%s : Invalid swc adjustment method.", MyFileName);
	}
	v->hist.yr.last = SW_Model.endyr;
	v->hist.yr.total = v->hist.yr.last - v->hist.yr.first + 1;
	CloseFile(&f);
}

/**
@brief Read a file containing historical swc measurements.  Enter a year with a four
      digit year number.  This is appended to the swc prefix to make the input file name.

@param year Four digit number for desired year, measured in years.
*/
void _read_swc_hist(TimeInt year) {
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
	SW_SOILWAT *v = &SW_Soilwat;
	FILE *f;
	int x, lyr, recno = 0, doy;
	RealF swc, st_err;
	char fname[MAX_FILENAMESIZE];

	snprintf(fname, MAX_FILENAMESIZE, "%s.%4d", v->hist.file_prefix, year);

	if (!FileExists(fname)) {
		LogError(logfp, LOGWARN, "Historical SWC file %s not found.", fname);
		return;
	}

	f = OpenFile(fname, "r");

	_clear_hist();

	while (GetALine(f, inbuf)) {
		recno++;
		x = sscanf(inbuf, "%d %d %f %f", &doy, &lyr, &swc, &st_err);
		if (x < 4) {
			CloseFile(&f);
			LogError(logfp, LOGFATAL, "%s : Incomplete layer data at record %d\n   Should be DOY LYR SWC STDERR.", fname, recno);
		}
		if (x > 4) {
			CloseFile(&f);
			LogError(logfp, LOGFATAL, "%s : Too many input fields at record %d\n   Should be DOY LYR SWC STDERR.", fname, recno);
		}
		if (doy < 1 || doy > MAX_DAYS) {
			CloseFile(&f);
			LogError(logfp, LOGFATAL, "%s : Day of year out of range at record %d", fname, recno);
		}
		if (lyr < 1 || lyr > MAX_LAYERS) {
			CloseFile(&f);
			LogError(logfp, LOGFATAL, "%s : Layer number out of range (%d > %d), record %d\n", fname, lyr, MAX_LAYERS, recno);
		}

		v->hist.swc[doy - 1][lyr - 1] = swc;
		v->hist.std_err[doy - 1][lyr - 1] = st_err;

	}
	CloseFile(&f);
}

/**
@brief Adjusts swc based on day of the year.

@param doy Day of the year, measured in days.
*/
void SW_SWC_adjust_swc(TimeInt doy) {
	/* =================================================== */
	/* 01/07/02 (cwb) added final loop to guarantee swc > swcBulk_min
	 */

	SW_SOILWAT *v = &SW_Soilwat;
	RealD lower, upper;
	LyrIndex lyr;
	TimeInt dy = doy - 1;

	switch (SW_Soilwat.hist.method) {
	case SW_Adjust_Avg:
		ForEachSoilLayer(lyr)
		{
			v->swcBulk[Today][lyr] += v->hist.swc[dy][lyr];
			v->swcBulk[Today][lyr] /= 2.;
		}
		break;

	case SW_Adjust_StdErr:
		ForEachSoilLayer(lyr)
		{
			upper = v->hist.swc[dy][lyr] + v->hist.std_err[dy][lyr];
			lower = v->hist.swc[dy][lyr] - v->hist.std_err[dy][lyr];
			if (GT(v->swcBulk[Today][lyr], upper))
				v->swcBulk[Today][lyr] = upper;
			else if (LT(v->swcBulk[Today][lyr], lower))
				v->swcBulk[Today][lyr] = lower;
		}
		break;

	default:
		LogError(logfp, LOGFATAL, "%s : Invalid SWC adjustment method.", SW_F_name(eSoilwat));
	}

	/* this will guarantee that any method will not lower swc */
	/* below the minimum defined for the soil layers          */
	ForEachSoilLayer(lyr){
		v->swcBulk[Today][lyr] = fmax(v->swcBulk[Today][lyr], SW_Site.lyr[lyr]->swcBulk_min);
  }

}

/**
@brief Calculates todays snowpack, partitioning of ppt into rain and snow, snowmelt and snowloss.

Equations based on SWAT2K routines. @cite Neitsch2005

@param temp_min Daily minimum temperature (C)
@param temp_max Daily maximum temperature (C)
@param ppt Daily precipitation (cm)
@param *rain Daily rainfall (cm)
@param *snow Daily snow-water equivalent of snowfall (cm)
@param *snowmelt  Daily snow-water equivalent of snowmelt (cm)

@sideeffect *rain Updated daily rainfall (cm)
@sideeffect *snow Updated snow-water equivalent of snowfall (cm)
@sideeffect *snowmelt Updated snow-water equivalent of daily snowmelt (cm)
*/




void SW_SWC_adjust_snow(RealD temp_min, RealD temp_max, RealD ppt, RealD *rain,
	RealD *snow, RealD *snowmelt) {

/*************************************************************************************************
History:

10/04/2010	(drs) added snowMAUS snow accumulation, sublimation and melt algorithm: Trnka, M., Kocmánková, E., Balek, J., Eitzinger, J., Ruget, F., Formayer, H., Hlavinka, P., Schaumberger, A., Horáková, V., Mozny, M. & Zalud, Z. (2010) Simple snow cover model for agrometeorological applications. Agricultural and Forest Meteorology, 150, 1115-1127.
replaced SW_SWC_snow_accumulation, SW_SWC_snow_sublimation, and SW_SWC_snow_melt with SW_SWC_adjust_snow
10/19/2010	(drs) replaced snowMAUS simulation with SWAT2K routines.

    **************************************************************************************************/

	RealD *snowpack = &SW_Soilwat.snowpack[Today],
		doy = SW_Model.doy, temp_ave,
		Rmelt, SnowAccu = 0., SnowMelt = 0.;

	static RealD snow_cov = 1.;
	temp_ave = (temp_min + temp_max) / 2.;

	/* snow accumulation */
	if (LE(temp_ave, SW_Site.TminAccu2)) {
		SnowAccu = ppt;
	} else {
		SnowAccu = 0.;
	}
	*rain = fmax(0., ppt - SnowAccu);
	*snow = fmax(0., SnowAccu);
	*snowpack += SnowAccu;

	/* snow melt */
	Rmelt = (SW_Site.RmeltMax + SW_Site.RmeltMin) / 2. + sin((doy - 81.) / 58.09) * (SW_Site.RmeltMax - SW_Site.RmeltMin) / 2.;
  temp_snow = temp_snow * (1 - SW_Site.lambdasnow) + temp_ave * SW_Site.lambdasnow;
	if (GT(temp_snow, SW_Site.TmaxCrit)) {
		SnowMelt = fmin( *snowpack, Rmelt * snow_cov * ((temp_snow + temp_max)/2. - SW_Site.TmaxCrit) );
	} else {
		SnowMelt = 0.;
	}
	if (GT(*snowpack, 0.)) {
		*snowmelt = fmax(0., SnowMelt);
		*snowpack = fmax(0., *snowpack - *snowmelt );
	} else {
		*snowmelt = 0.;
	}
}

/**
@brief Snow loss through sublimation and other processes

Equations based on SWAT2K routines. @cite Neitsch2005

@param pet Potential evapotranspiration rate, measured in cm per day.
@param *snowpack Snow-water equivalent of single layer snowpack, measured in cm.

@sideeffect *snowpack Updated snow-water equivalent of single layer snowpack, measured in cm.

@return snowloss Snow loss through sublimation and other processes, measuered in cm.

*/
RealD SW_SWC_snowloss(RealD pet, RealD *snowpack) {
	RealD snowloss;
	static RealD cov_soil = 0.5;

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

RealD SW_SnowDepth(RealD SWE, RealD snowdensity) {
	/*---------------------
	 08/22/2011	(drs)	calculates depth of snowpack
	 Input:	SWE: snow water equivalents (cm = 10kg/m2)
	 snowdensity (kg/m3)
	 Output: snow depth (cm)
	 ---------------------*/
	if (GT(snowdensity, 0.)) {
		return SWE / snowdensity * 10. * 100.;
	} else {
		return 0.;
	}
}


/**
  @brief Convert soil water content to soil water potential using
      specified soil water retention curve (SWRC)

  SOILWAT2 convenience wrapper for `SWRC_SWCtoSWP()`.

  See #swrc2str() for implemented SWRCs.

  @param[in] swcBulk Soil water content in the layer [cm]
  @param[in] *lyr Soil information including
    SWRC type, SWRC parameters,
    coarse fragments (e.g., gravel), and soil layer width.

  @return Soil water potential [-bar]
*/
RealD SW_SWRC_SWCtoSWP(RealD swcBulk, SW_LAYER_INFO *lyr) {
  return SWRC_SWCtoSWP(
    swcBulk,
    lyr->swrc_type,
    lyr->swrcp,
    lyr->fractionVolBulk_gravel,
    lyr->width,
    LOGFATAL
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
    SOILWAT2 uses `LOGFATAL` and fails but
    other applications may want to warn only (`LOGWARN`) and return.

  @return Soil water potential [-bar]
*/
double SWRC_SWCtoSWP(
	double swcBulk,
	unsigned int swrc_type,
	double *swrcp,
	double gravel,
	double width,
	const int errmode
) {
	double res = SW_MISSING;

	if (LT(swcBulk, 0.) || EQ(gravel, 1.) || LE(width, 0.)) {
		LogError(
			logfp,
			errmode,
			"SWRC_SWCtoSWP(): invalid SWC = %.4f (must be >= 0)\n",
			swcBulk
		);

		return res;
	}

	switch (swrc_type) {
		case sw_Campbell1974:
			res = SWRC_SWCtoSWP_Campbell1974(
				swcBulk, swrcp, gravel, width,
				errmode
			);
			break;

		case sw_vanGenuchten1980:
			res = SWRC_SWCtoSWP_vanGenuchten1980(
				swcBulk, swrcp, gravel, width,
				errmode
			);
			break;

		case sw_FXW:
			res = SWRC_SWCtoSWP_FXW(
				swcBulk, swrcp, gravel, width,
				errmode
			);
			break;

		default:
			LogError(
				logfp,
				errmode,
				"SWRC (type %d) is not implemented.",
				swrc_type
			);
			break;
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
    are the inverse of each other
    for `(phi, theta)` between `(swrcp[0], `theta` at `swrcp[0]`)`
    and `(infinity, 0)`.

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
    SOILWAT2 uses `LOGFATAL` and fails but
    other applications may want to warn only (`LOGWARN`) and return.

  @return Soil water potential [-bar]
*/
double SWRC_SWCtoSWP_Campbell1974(
	double swcBulk,
	double *swrcp,
	double gravel,
	double width,
	const int errmode
) {
	// assume that we have soil moisture
	double theta, tmp, res;

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
				logfp,
				errmode,
				"SWRC_SWCtoSWP_Campbell1974(): invalid value of\n"
				"\t(theta / theta(saturated)) ^ b = (%f / %f) ^ %f =\n"
				"\t= %f (must be > 0)\n",
				theta, swrcp[1], swrcp[2], tmp
			);

			return SW_MISSING;
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
    are the inverse of each other
    for `(phi, theta)` between `(0, theta_sat)` and `(infinity, theta_min)`.

  @param[in] swcBulk Soil water content in the layer [cm]
  @param[in] *swrcp Vector of SWRC parameters
  @param[in] gravel Coarse fragments (> 2 mm; e.g., gravel)
    of the whole soil [m3/m3]
  @param[in] width Soil layer width [cm]
  @param[in] errmode An error code passed to `LogError()`.
    SOILWAT2 uses `LOGFATAL` and fails but
    other applications may want to warn only (`LOGWARN`) and return.

  @return Soil water potential [-bar]
*/
double SWRC_SWCtoSWP_vanGenuchten1980(
	double swcBulk,
	double *swrcp,
	double gravel,
	double width,
	const int errmode
) {
	double res, tmp, theta;

	// convert bulk SWC [cm] to theta = matric VWC [cm / cm]
	theta = swcBulk / (width * (1. - gravel));

	// calculate if theta in ]theta_min, theta_sat]
	if (GT(theta, swrcp[0])) {
		if (LT(theta, swrcp[1])) {
			// calculate inverse of normalized theta
			tmp = (swrcp[1] - swrcp[0]) / (theta - swrcp[0]);

			// calculate tension [cm of H20]
			tmp = powe(tmp, 1. / (1. - 1. / swrcp[3])); // tmp values are >= 1
			res = pow(-1. + tmp, 1. / swrcp[3]) / swrcp[2]; // `pow()` because x >= 0

			// convert [cm of H2O at 4 C; value from `soilDB::KSSL_VG_model()`] to [bar]
			res /= 1019.716;

		} else if (EQ(theta, swrcp[1])) {
			// theta is theta_sat
			res = 0;

		} else {
			// theta is > theta_sat
			LogError(
				logfp,
				errmode,
				"SWRC_SWCtoSWP_vanGenuchten1980(): invalid value of\n"
				"\ttheta = %f (must be <= theta_sat = %f)\n",
				theta, swrcp[1]
			);

			res = SW_MISSING;
		}

	} else {
		// theta is <= theta_min
		LogError(
			logfp,
			errmode,
			"SWRC_SWCtoSWP_vanGenuchten1980(): invalid value of\n"
			"\ttheta = %f (must be > theta_min = %f)\n",
			theta, swrcp[0]
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
    are the inverse of each other
    for `(phi, theta)` between `(0, theta_sat)` and `(6.3 x 10^6 cm, 0)`.


  @param[in] swcBulk Soil water content in the layer [cm]
  @param[in] *swrcp Vector of SWRC parameters
  @param[in] gravel Coarse fragments (> 2 mm; e.g., gravel)
    of the whole soil [m3/m3]
  @param[in] width Soil layer width [cm]
  @param[in] errmode An error code passed to `LogError()`.
    SOILWAT2 uses `LOGFATAL` and fails but
    other applications may want to warn only (`LOGWARN`) and return.

  @return Soil water potential [-bar]
*/
double SWRC_SWCtoSWP_FXW(
	double swcBulk,
	double *swrcp,
	double gravel,
	double width,
	const int errmode
) {
	double res, theta;

	// convert bulk SWC [cm] to theta = matric VWC [cm / cm]
	theta = swcBulk / (width * (1. - gravel));

	// calculate if theta in [0, theta_sat]
	if (GE(theta, 0.)) {
		if (LT(theta, swrcp[0])) {
			// calculate tension = phi [bar]
			res = itp_FXW_for_phi(theta, swrcp);

		} else if (EQ(theta, swrcp[0])) {
			// theta is theta_sat
			res = 0.;

		} else {
			// theta is > theta_sat
			LogError(
				logfp,
				errmode,
				"SWRC_SWCtoSWP_FXW(): invalid value of\n"
				"\ttheta = %f (must be <= theta_sat = %f)\n",
				theta, swrcp[0]
			);

			res = SW_MISSING;
		}

	} else {
		// theta is < 0
		LogError(
			logfp,
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
  @param[in] *lyr Soil information including
    SWRC type, SWRC parameters,
    coarse fragments (e.g., gravel), and soil layer width.

  @return Soil water content in the layer [cm]
*/
RealD SW_SWRC_SWPtoSWC(RealD swpMatric, SW_LAYER_INFO *lyr) {
  return SWRC_SWPtoSWC(
    swpMatric,
    lyr->swrc_type,
    lyr->swrcp,
    lyr->fractionVolBulk_gravel,
    lyr->width,
    LOGFATAL
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
    SOILWAT2 uses `LOGFATAL` and fails but
    other applications may want to warn only (`LOGWARN`) and return.

  @return Soil water content in the layer [cm]
*/
double SWRC_SWPtoSWC(
	double swpMatric,
	unsigned int swrc_type,
	double *swrcp,
	double gravel,
	double width,
	const int errmode
) {
	double res = SW_MISSING;

	if (LT(swpMatric, 0.)) {
		LogError(
			logfp,
			errmode,
			"SWRC_SWPtoSWC(): invalid SWP = %.4f (must be >= 0)\n",
			swpMatric
		);

		return res;
	}

	switch (swrc_type) {
		case sw_Campbell1974:
			res = SWRC_SWPtoSWC_Campbell1974(swpMatric, swrcp, gravel, width);
			break;

		case sw_vanGenuchten1980:
			res = SWRC_SWPtoSWC_vanGenuchten1980(swpMatric, swrcp, gravel, width);
			break;

		case sw_FXW:
			res = SWRC_SWPtoSWC_FXW(swpMatric, swrcp, gravel, width);
			break;

		default:
			LogError(
				logfp,
				errmode,
				"SWRC (type %d) is not implemented.",
				swrc_type
			);
			break;
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
    are the inverse of each other
    for `(phi, theta)` between `(swrcp[0], `theta` at `swrcp[0]`)`
    and `(infinity, 0)`.

  @param[in] swpMatric Soil water potential [-bar]
  @param[in] *swrcp Vector of SWRC parameters
  @param[in] gravel Coarse fragments (> 2 mm; e.g., gravel)
    of the whole soil [m3/m3]
  @param[in] width Soil layer width [cm]

  @return Soil water content in the layer [cm]
*/
double SWRC_SWPtoSWC_Campbell1974(
	double swpMatric,
	double *swrcp,
	double gravel,
	double width
) {
	double phi, res;

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
    are the inverse of each other
    for `(phi, theta)` between `(0, theta_sat)` and `(infinity, theta_min)`.

  @param[in] swpMatric Soil water potential [-bar]
  @param[in] *swrcp Vector of SWRC parameters
  @param[in] gravel Coarse fragments (> 2 mm; e.g., gravel)
    of the whole soil [m3/m3]
  @param[in] width Soil layer width [cm]

  @return Soil water content in the layer [cm]
*/
double SWRC_SWPtoSWC_vanGenuchten1980(
	double swpMatric,
	double *swrcp,
	double gravel,
	double width
) {
	double phi, tmp, res;

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
  FXW Soil Water Retention Curve
  \cite fredlund1994CGJa, \cite wang2018wrr

  Parameters are explained in `SWRC_check_parameters_for_FXW()`.

  @note
   `SWRC_SWPtoSWC_FXW()` and `SWRC_SWCtoSWP_FXW()`
    are the inverse of each other
    for `(phi, theta)` between `(0, theta_sat)` and `(6.3 x 10^6 cm, 0)`.

  @param[in] swpMatric Soil water potential [-bar]
  @param[in] *swrcp Vector of SWRC parameters
  @param[in] gravel Coarse fragments (> 2 mm; e.g., gravel)
    of the whole soil [m3/m3]
  @param[in] width Soil layer width [cm]

  @return Soil water content in the layer [cm]
*/
double SWRC_SWPtoSWC_FXW(
	double swpMatric,
	double *swrcp,
	double gravel,
	double width
) {
	double phi, res;

	// convert SWP [-bar] to phi [cm of H2O at 4 C;
	// value from `soilDB::KSSL_VG_model()`]
	phi = swpMatric * 1019.716;

	res = FXW_phi_to_theta(phi, swrcp);

	// convert matric theta [cm / cm] to bulk SWC [cm]
	return (1. - gravel) * width * res;
}




/**
@brief This routine sets the known memory refs in this module
     so they can be  checked for leaks, etc.
*/

#ifdef DEBUG_MEM
#include "include/myMemory.h"
/*======================================================*/
void SW_SWC_SetMemoryRefs( void) {
	/* when debugging memory problems, use the bookkeeping
	 code in myMemory.c
	 This routine sets the known memory refs in this module
	 so they can be  checked for leaks, etc.  Includes
	 malloc-ed memory in SOILWAT.  All refs will have been
	 cleared by a call to ClearMemoryRefs() before this, and
	 will be checked via CheckMemoryRefs() after this, most
	 likely in the main() function.
	 */

	/*  NoteMemoryRef(SW_Soilwat.hist.file_prefix); */

}

#endif