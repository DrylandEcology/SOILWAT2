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
#include "generic.h"
#include "filefuncs.h"
#include "myMemory.h"
#include "SW_Defines.h"
#include "SW_Files.h"
#include "SW_Model.h"
#include "SW_Site.h"
#include "SW_Flow.h"
#include "SW_SoilWater.h"
#include "SW_VegProd.h"
#ifdef SWDEBUG
  #include "SW_Weather.h"
#endif
#ifdef RSOILWAT
  #include "../rSW_SoilWater.h" // for onSet_SW_SWC_hist()
#endif


/* =================================================== */
/*                  Global Variables                   */
/* --------------------------------------------------- */

extern SW_MODEL SW_Model;
extern SW_SITE SW_Site;
extern SW_VEGPROD SW_VegProd;
#ifdef RSOILWAT
	extern Bool useFiles;
#endif

SW_SOILWAT SW_Soilwat; /* declared here, externed elsewhere */
#ifdef SWDEBUG
  extern SW_WEATHER SW_Weather;
#endif


/* =================================================== */
/*                Module-Level Variables               */
/* --------------------------------------------------- */
static char *MyFileName;
static RealD temp_snow;


/* =================================================== */
/* =================================================== */
/*             Private Function Definitions            */
/* --------------------------------------------------- */

static void _clear_hist(void) {
	/* --------------------------------------------------- */
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



#ifdef SWDEBUG
void SW_WaterBalance_Checks(void)
{
  SW_SOILWAT *sw = &SW_Soilwat;
	SW_WEATHER *w = &SW_Weather;

  IntUS i, k;
  int debugi[N_WBCHECKS] = {1, 1, 1, 1, 1, 1, 1, 1}; // print output for each check yes/no
  char flag[15];
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
  rain = w->now.rain[Today];

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
    sprintf(flag, "WB (%d-%d)", SW_Model.year, SW_Model.doy);
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

  // Setup only once
  if (!sw->is_wbError_init) {
    sw->is_wbError_init = swTRUE;
  }
}
#endif


/* =================================================== */
/* =================================================== */
/*             Public Function Definitions             */
/* --------------------------------------------------- */

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
	v->surfaceTemp = 0;
	LyrIndex i;
	ForEachSoilLayer(i)
		v->sTemp[i] = SW_Site.lyr[i]->sTemp;

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

	sprintf(fname, "%s.%4d", v->hist.file_prefix, year);

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
  @brief Calculates the soil water potential from soil water content of the
         n-th soil layer.

  The equation and its coefficients are based on a
  paper by Cosby,Hornberger,Clapp,Ginn,  in WATER RESOURCES RESEARCH
  June 1984.  Moisture retention data was fit to the power function.

  The code assumes the following conditions:
      * checked by `SW_SIT_init_run()`
          * width > 0
          * fractionGravel, sand, clay, and sand + clay in [0, 1]
      * checked by function `water_eqn()`
          * thetasMatric > 0
          * bMatric != 0

  @param fractionGravel Fraction of soil containing gravel.
  @param swcBulk Soilwater content of the current layer (cm/layer)
  @param n Layer number to index the **lyr pointer

  @return soil water potential
**/

RealD SW_SWCbulk2SWPmatric(RealD fractionGravel, RealD swcBulk, LyrIndex n) {
/**********************************************************************

HISTORY:
    DATE:  April 2, 1992
    9/1/92  (SLC) if swc comes in as zero, set swpotentl to
    upperbnd.  (Previously, we flagged this
    as an error, and set swpotentl to zero).

    27-Aug-03 (cwb) removed the upperbnd business. Except for
    missing values, swc < 0 is impossible, so it's an error,
    and the previous limit of swp to 80 seems unreasonable.
    return 0.0 if input value is MISSING

   These are the values for each layer obtained via lyr[n]:
	 width  - width of current soil layer
	 psisMatric   - "saturation" matric potential
	 thetasMatric - saturated moisture content.
	 bMatric       - see equation below.
	 swc_lim - limit for matric potential

	 LOCAL VARIABLES:
	 theta1 - volumetric soil water content

	 DEFINED CONSTANTS:
	 barconv - conversion factor from bars to cm water.  (i.e.
	 1 bar = 1024cm water)

	 COMMENT:
	 See the routine "watreqn" for a description of how the variables
	 psisMatric, bMatric, binverseMatric, thetasMatric are initialized
	 **********************************************************************/

	SW_LAYER_INFO *lyr = SW_Site.lyr[n];
	RealD theta1, theta2, swp = .0;

	if (missing(swcBulk) || ZRO(swcBulk))
		return 0.0;

	if (GT(swcBulk, 0.0)) {
		// we have soil moisture

		// calculate matric VWC [cm / cm %] from bulk VWC
		theta1 = (swcBulk / lyr->width) * 100. / (1. - fractionGravel);

		// calculate (VWC / VWC(saturated)) ^ b
		theta2 = powe(theta1 / lyr->thetasMatric, lyr->bMatric);

		if (isnan(theta2) || ZRO(theta2)) {
			LogError(logfp, LOGFATAL, "SW_SWCbulk2SWPmatric(): Year = %d, DOY=%d, Layer = %d:\n"
					"\tinvalid value of (theta / theta(saturated)) ^ b = %f (must be != 0)\n",
					SW_Model.year, SW_Model.doy, n, theta2);
		} else {
			swp = lyr->psisMatric / theta2 / BARCONV;
		}

	} else {
		LogError(logfp, LOGFATAL, "Invalid SWC value (%.4f) in SW_SWC_swc2potential.\n"
				"    Year = %d, DOY=%d, Layer = %d\n", swcBulk, SW_Model.year, SW_Model.doy, n);
	}

	return swp;
}

/**
@brief Convert soil water potential to bulk volumetric water content.

@param fractionGravel Fraction of soil containing gravel, percentage.
@param swpMatric lyr->psisMatric calculated in water equation function
@param n Layer of soil.

@return Volumentric water content (cm H<SUB>2</SUB>O/cm SOIL).
**/



RealD SW_SWPmatric2VWCBulk(RealD fractionGravel, RealD swpMatric, LyrIndex n) {

/**
  History:
    27-Aug-03 (cwb) moved from the Site module.
**/

	SW_LAYER_INFO *lyr = SW_Site.lyr[n];
	RealD t, p;
	swpMatric *= BARCONV;
	p = powe(lyr->psisMatric / swpMatric, lyr->binverseMatric); // lyr->psisMatric calculated in water equation function | todo: check to make sure these are calculated before
  t = lyr->thetasMatric * p * 0.01 * (1 - fractionGravel);
	return (t);
}

/**
@brief Calculates 'Brooks-Corey' residual volumetric soil water.

Equations based on: Rawls WJ, Brakensiek DL (1985) Prediction of soil water properties
      for hydrological modeling, based on @cite ASCE1985

@param fractionGravel Fraction of soil consisting of gravel, percentage.
@param sand Fraction of soil consisting of sand, percentage.
@param clay Fraction of soil consisting of clay, percentage.
@param porosity Fraction of Soil porosity as the saturated VWC, percentage.

@returns Residual volumetric soil water (cm/cm)
**/

RealD SW_VWCBulkRes(RealD fractionGravel, RealD sand, RealD clay, RealD porosity) {
/*---------------------
History:
  02/03/2012	(drs)	calculates 'Brooks-Corey' residual volumetric soil water based on Rawls WJ, Brakensiek DL (1985) Prediction of soil water properties for hydrological modeling. In Watershed management in the Eighties (eds Jones EB, Ward TJ), pp. 293-299. American Society of Civil Engineers, New York.
  however, equation is only valid if (0.05 < clay < 0.6) & (0.05 < sand < 0.7)

---------------------*/

  if (clay < .05 || clay > .6 || sand < .05 || sand > .7) {
    LogError(
      logfp,
      LOGWARN,
      "Sand and/or clay values out of valid range, simulation outputs may differ."
    );
    return SW_MISSING;

  } else {
    RealD res;
    sand *= 100.;
    clay *= 100.;

    res = (1. - fractionGravel) * (
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

    return (fmax(res, 0.));
  }
}

/**
@brief This routine sets the known memory refs in this module
     so they can be  checked for leaks, etc.
*/

#ifdef DEBUG_MEM
#include "myMemory.h"
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
