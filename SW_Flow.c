/********************************************************/
/********************************************************/
/*	Source file: SW_Flow.c
 Type: module
 Application: SOILWAT - soilwater dynamics simulator
 Purpose: This is the interesting part of the model--
 the flow of water through the soil.

 ORIGINAL NOTES/COMMENTS
 ********************************************************************
 PURPOSE: Water-flow submodel.  This submodel is a rewrite of a
 model originally written by William Parton.  It simulates
 the flow of water through the plant canopy and soil.
 See "Abiotic Section of ELM", as a reference.
 The subroutines called are listed in the file "subwatr.f"

 HISTORY:
 4/30/92  (not tested - first pass)
 7/2/92   (SLC) Reset swc to 0 if less than 0.  (Due to roundoff)
 See function "chkzero" in the subwatr.f file.  Each
 swc is checked for negative values.
 1/17/94  (SLC) Set daily array to zero when no transpiration or
 evaporation.
 1/27/94  (TLB) Added daily deep drainage variable
 (4/10/2000) -- INITIAL CODING - cwb
 9/21/01	I have to make some transformation from the record-oriented
 structure of the new design to the state array parameter
 structure of the old design.  I thought it would be worth
 trying to rewrite the routines, but with the current version
 of the record layouts, it required too much coupling with the
 other modules, and I refrained for two reasons.  First was
 the time involved, and second was the possiblity of leaving
 the code in SoWat_flow_subs.c as is to facilitate putting
 it into a library.
 10/09/2009	(drs) added snow accumulation, snow sublimation
 and snow melt to SW_Water_Flow()
 01/12/2010	(drs) turned not-fuctional snow sublimation to snow_sublimation (void)
 02/02/2010	(drs) added to SW_Water_Flow(): saving values for standcrop_int, litter_int and soil_inf
 02/08/2010	(drs) if there is a snowpack then
 - rain infiltrates directly to soil (no vegetation or litter interception of today)
 - no transpiration or evaporation (except evaporation of yesterdays interception)
 only
 - infiltrate water high
 - infiltrate water low
 10/04/2010	(drs) moved call to SW_SWC_adjust_snow() back to SW_WTH_new_day()
 10/19/2010	(drs) added call to hydraulic_redistribution() in SW_Water_Flow() after all the evap/transp/infiltration is computed
 added temporary array lyrHydRed to arrays2records() and records2arrays()
 11/16/2010	(drs) added call to forest_intercepted_water() depending on SW_VegProd.Type..., added forest_h2o to trace forest intercepted water
 renamed standcrop_h2o_qum -> veg_int_storage
 renamed totstcr_h2o -> totveg_h2o
 01/03/2011	(drs) changed type of lyrTrRegions[MAX_LAYERS] from int to IntU to avoid warning message that ' pointer targets differ in signedness' in function transp_weighted_avg()
 01/04/2011	(drs) added snowmelt to h2o_for_soil after interception, otherwise last day of snowmelt (when snowpack is gone) wasn't made available and aet became too small
 01/06/2011	(drs) layer drainage was incorrectly calculated if hydraulic redistribution pumped water into a layer below pwp such that its swc was finally higher than pwp -> fixed it by first calculating hydraulic redistribution and then as final step infiltrate_water_low()
 01/06/2011	(drs) call to infiltrate_water_low() has to be the last swc affecting calculation
 02/19/2011	(drs) calculate runoff as adjustment to snowmelt events before infiltration
 02/22/2011	(drs) init aet for the day in SW_Water_Flow(), instead implicitely in evap_litter_veg()
 02/22/2011	(drs) added snowloss to AET
 07/21/2011	(drs) added module variables 'lyrImpermeability' and 'lyrSWCBulk_Saturated' and initialize them in records2arrays()
 07/22/2011	(drs) adjusted soil_infiltration for pushed back water to surface (difference in standingWater)
 07/22/2011	(drs) included evaporation from standingWater into evap_litter_veg_surfaceWater() and reduce_rates_by_surfaceEvaporation(): it includes it to AET
 08/22/2011	(drs) in SW_Water_Flow(void): added snowdepth_scale = 1 - snow depth/vegetation height
 - vegetation interception = only when snowdepth_scale > 0, scaled by snowdepth_scale
 - litter interception = only when no snow cover
 - transpiration = only when snowdepth_scale > 0, scaled by snowdepth_scale
 - bare-soil evaporation = only when no snow cover
 09/08/2011	(drs) interception, evaporation from intercepted, E-T partitioning, transpiration, and hydraulic redistribution for each vegetation type (tree, shrub, grass) of SW_VegProd separately, scaled by their fractions
 replaced PET with unevaped in pot_soil_evap() and pot_transp(): to simulate reduction of atmospheric demand underneath canopies
 09/09/2011	(drs) moved transp_weighted_avg() from before infiltration and percolation to directly before call to pot_transp()
 09/21/2011	(drs)	scaled all (potential) evaporation and transpiration flux rates with PET: SW_Flow_lib.h: reduce_rates_by_unmetEvapDemand() is obsolete
 09/26/2011	(drs)	replaced all uses of monthly SW_VegProd and SW_Sky records with the daily replacements
 02/03/2012	(drs)	added variable 'snow_evap_rate': snow loss is part of aet and needs accordingly also to be scaled so that sum of all et rates is not more than pet
 01/28/2012	(drs)	transpiration can only remove water from soil down to lyrSWCBulk_Wiltpts (instead lyrSWCBulkmin)
 02/03/2012	(drs)	added 'lyrSWCBulk_HalfWiltpts' = 0.5 * SWC at -1.5 MPa
 soil evaporation extracts water down to 'lyrSWCBulk_HalfWiltpts' according to the FAO-56 model, e.g., Burt CM, Mutziger AJ, Allen RG, Howell TA (2005) Evaporation Research: Review and Interpretation. Journal of Irrigation and Drainage Engineering, 131, 37-58.
 02/04/2012	(drs)	added 'lyrSWCBulkatSWPcrit_xx' for each vegetation type
 transpiration can only remove water from soil down to 'lyrSWCBulkatSWPcrit_xx' (instead lyrSWCBulkmin)
 02/04/2012	(drs)	snow loss is fixed and can also include snow redistribution etc., so don't scale to PET
 05/25/2012  (DLM) added module level variables lyroldavgLyrTemp [MAX_LAYERS] & lyravgLyrTemp [MAX_LAYERS] to keep track of soil temperatures, added lyrbDensity to keep track of the bulk density for each layer
 05/25/2012  (DLM) edited records2arrays(void); & arrays2records(void); functions to move values to / from lyroldavgLyrTemp & lyrTemp & lyrbDensity
 05/25/2012  (DLM) added call to soil_temperature function in SW_Water_Flow(void)
 11/06/2012	(clk)	added slope and aspect to the call to petfunc()
 11/30/2012	(clk)	added lines to calculate the surface runoff and to adjust the surface water level based on that value
 01/31/2013	(clk)	With the addition of a new type of vegetation, bare ground, needed to add a new function call to pot_soil_evap_bs() and create a few new variables:
 RealD lyrEvap_BareGround [MAX_LAYERS] was created,
 RealD soil_evap_rate_bs was created, and initialized at 1.0,
 new function, pot_soil_evap_bs(), is called if fractionBareGround is not zero and there is no snowpack on the ground,
 Added soil_evap_rate_bs to rate_help, and then adjusted soil_evap_rate_bs by rate_help if needed,
 Added call to remove bare-soil evap from swv,
 And added lyrEvap_BareGround into the calculation for SW_Soilwat.evaporation.
 Also, added SW_W_VegProd.bare_cov.albedo*SW_VegProd.fractionBareGround to the paramater in petfunc() that was originally SW_VegProd.veg[SW_GRASS].cov.albedo*SW_VegProd.fractionGrass + SW_VegProd.shrub.cov.albedo*SW_VegProd.shrub.cov.fCover + SW_VegProd.tree.cov.albedo*SW_VegProd.tree.cov.fCover
 04/16/2013	(clk)	Renamed a lot of the variables to better reflect BULK versus MATRIC values
 updated the use of these variables in all the files
 06/24/2013	(rjm)	added 'soil_temp_error', 'soil_temp_init' and 'fusion_pool_init' as global variable
 added function SW_FLW_construct() to init global variables between consecutive calls to SoilWat as dynamic library
 07/09/2013	(clk)	with the addition of forbs as a vegtype, needed to add a lot of calls to this code and so basically just copied and pasted the code for the other vegtypes
 09/26/2013 (drs) records2arrays(): Init hydraulic redistribution to zero; if not used and not initialized, then there could be non-zero values resulting
 06/23/2015 (akt)	Added surfaceAvg[Today] value at structure SW_Weather so that we can add surfaceAvg[Today] in output from Sw_Outout.c get_tmp() function
 02/08/2016 (CMA & CTD) Added snowpack as an input argument to function call of soil_temperature()
 02/08/2016 (CMA & CTD) Modified biomass to use the live biomass as opposed to standing crop
 */
/********************************************************/
/********************************************************/

/* =================================================== */
/*                INCLUDES / DEFINES                   */
/* --------------------------------------------------- */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "generic.h"
#include "filefuncs.h"
#include "SW_Defines.h"
#include "SW_Model.h" // externs SW_Model
#include "SW_Site.h" // externs SW_Site
#include "SW_SoilWater.h" // externs SW_Soilwat
#include "SW_Flow_lib.h" // externs stValues, soil_temp_init
/*#include "SW_VegEstab.h" */
#include "SW_VegProd.h" // externs SW_VegProd, key2veg
#include "SW_Weather.h"  // externs SW_Weather
#include "SW_Sky.h" // externs SW_Sky

#include "SW_Flow_lib_PET.h"
#include "SW_Flow.h"


/* =================================================== */
/*                  Local Variables                    */
/* --------------------------------------------------- */

/* temporary arrays for SoWat_flow_subs.c subroutines.
 * array indexing in those routines will be from
 * zero rather than 1.  see records2arrays().
 */
static IntU lyrTrRegions[NVEGTYPES][MAX_LAYERS];

static RealD
	lyrSWCBulk[MAX_LAYERS],
	lyrDrain[MAX_LAYERS],

	lyrTransp[NVEGTYPES][MAX_LAYERS],
	lyrTranspCo[NVEGTYPES][MAX_LAYERS],
	lyrEvap[NVEGTYPES][MAX_LAYERS],
	lyrEvap_BareGround[MAX_LAYERS],
	lyrSWCBulk_atSWPcrit[NVEGTYPES][MAX_LAYERS],
	lyrHydRed[NVEGTYPES][MAX_LAYERS],

	lyrbDensity[MAX_LAYERS],
	lyrWidths[MAX_LAYERS],
	lyrEvapCo[MAX_LAYERS],
	lyrSumTrCo[MAX_TRANSP_REGIONS + 1],
	lyrImpermeability[MAX_LAYERS],
	lyrSWCBulk_FieldCaps[MAX_LAYERS],
	lyrSWCBulk_Saturated[MAX_LAYERS],
	lyrSWCBulk_Wiltpts[MAX_LAYERS],
	lyrSWCBulk_HalfWiltpts[MAX_LAYERS],
	lyrSWCBulk_Mins[MAX_LAYERS],

	lyroldavgLyrTemp[MAX_LAYERS],
	lyravgLyrTemp[MAX_LAYERS];


static RealD drainout; /* h2o drained out of deepest layer */

// variables to help calculate runon from a (hypothetical) upslope neighboring (UpNeigh) site
static RealD
	UpNeigh_lyrSWCBulk[MAX_LAYERS],
	UpNeigh_lyrDrain[MAX_LAYERS],
	UpNeigh_drainout,
	UpNeigh_standingWater;


static RealD
	surfaceAvg[TWO_DAYS],
	veg_int_storage[NVEGTYPES], // storage of intercepted rain by the vegetation
	litter_int_storage, // storage of intercepted rain by the litter layer
	standingWater[TWO_DAYS]; /* water on soil surface if layer below is saturated */



/* =================================================== */
/*             Local Function Definitions              */
/* --------------------------------------------------- */

static void records2arrays(void) {
	/* some values are unchanged by the water subs but
	 * are still required in an array format.
	 * Also, some arrays start out empty and are
	 * filled during the water flow.
	 * See arrays2records() for the modified arrays.
	 *
	 * 3/24/2003 - cwb - when running with steppe, the
	 *       static variable firsttime would only be set once
	 *       so the firsttime tasks were done only the first
	 *       year, but what we really want with stepwat is
	 *       to firsttime tasks on the first day of each year.
	 * 1-Oct-03 (cwb) - Removed references to sum_transp_coeff.
	 *       see also Site.c.
	 */
	LyrIndex i;
	int k;

	ForEachSoilLayer(i)
	{
		lyrSWCBulk[i] = SW_Soilwat.swcBulk[Today][i];
		lyroldavgLyrTemp[i] = SW_Soilwat.avgLyrTemp[i];
	}

	if (SW_Model.doy == SW_Model.firstdoy) {
		ForEachSoilLayer(i)
		{
			lyrSWCBulk_FieldCaps[i] = SW_Site.lyr[i]->swcBulk_fieldcap;
			lyrWidths[i] = SW_Site.lyr[i]->width;
			lyrSWCBulk_Wiltpts[i] = SW_Site.lyr[i]->swcBulk_wiltpt;
			lyrSWCBulk_HalfWiltpts[i] = SW_Site.lyr[i]->swcBulk_halfwiltpt;
			lyrSWCBulk_Mins[i] = SW_Site.lyr[i]->swcBulk_min;
			lyrImpermeability[i] = SW_Site.lyr[i]->impermeability;
			lyrSWCBulk_Saturated[i] = SW_Site.lyr[i]->swcBulk_saturated;
			lyrbDensity[i] = SW_Site.lyr[i]->soilBulk_density;

			ForEachVegType(k)
			{
				lyrTrRegions[k][i] = SW_Site.lyr[i]->my_transp_rgn[k];
				lyrSWCBulk_atSWPcrit[k][i] = SW_Site.lyr[i]->swcBulk_atSWPcrit[k];
				lyrTranspCo[k][i] = SW_Site.lyr[i]->transp_coeff[k];
			}
		}

		ForEachEvapLayer(i) {
			lyrEvapCo[i] = SW_Site.lyr[i]->evap_coeff;
		}

	} /* end firsttime stuff */

}

static void arrays2records(void) {
	/* move output quantities from arrays to
	 * the appropriate records.
	 */
	LyrIndex i;
	int k;

	ForEachSoilLayer(i)
	{
		SW_Soilwat.swcBulk[Today][i] = lyrSWCBulk[i];
		SW_Soilwat.drain[i] = lyrDrain[i];
		SW_Soilwat.avgLyrTemp[i] = lyravgLyrTemp[i];
		ForEachVegType(k)
		{
			SW_Soilwat.hydred[k][i] = lyrHydRed[k][i];
			SW_Soilwat.transpiration[k][i] = lyrTransp[k][i];
		}
	}
	SW_Weather.surfaceAvg = surfaceAvg[Today];

	if (SW_Site.deepdrain)
		SW_Soilwat.swcBulk[Today][SW_Site.deep_lyr] = drainout;


	ForEachEvapLayer(i)
	{
		SW_Soilwat.evaporation[i] = lyrEvap_BareGround[i];
		ForEachVegType(k)
		{
			SW_Soilwat.evaporation[i] += lyrEvap[k][i];
		}
	}

}



/* =================================================== */
/*             Global Function Definitions             */
/* --------------------------------------------------- */
/* There is only one external function here and it is
 * only called from SW_Soilwat, so it is declared there.
 * but the compiler may complain if not predeclared here
 * This is a specific option for the compiler and may
 * not always occur.
 */

/**
@brief Initialize global variables between consecutive calls to SOILWAT.
*/
void SW_FLW_init_run(void) {
	/* 06/26/2013	(rjm) added function SW_FLW_init_run() to init global variables between consecutive calls to SoilWat as dynamic library */
	int i, k;


	//These only have to be cleared if a loop is wrong in the code.
	for (i = 0; i < MAX_LAYERS; i++) {
		ForEachVegType(k) {
			lyrTrRegions[k][i] = 0;
			lyrTransp[k][i] = 0.;
			lyrTranspCo[k][i] = 0.;
			lyrEvap[k][i] = 0.;
			lyrSWCBulk_atSWPcrit[k][i] = 0.;
			lyrHydRed[k][i] = 0;
		}

		lyrSWCBulk[i] = lyrDrain[i] = 0.;
		lyrEvap_BareGround[i] = 0;
		lyrEvapCo[i] = lyrSWCBulk_FieldCaps[i] = 0;
		lyrWidths[i] = lyrSWCBulk_Wiltpts[i] = lyrSWCBulk_HalfWiltpts[i] = 0;
		lyrSWCBulk_Mins[i] = 0;
		lyrImpermeability[i] = lyrSWCBulk_Saturated[i] = 0;
		lyroldavgLyrTemp[i] = lyravgLyrTemp[i] = lyrbDensity[i] = 0;
	}

	for(i=0; i<= MAX_TRANSP_REGIONS; i++)
		lyrSumTrCo[i] = 0;

	//When running as a library make sure these are set to zero.
	drainout = 0;
	surfaceAvg[0] = surfaceAvg[1] = 0.;
	standingWater[0] = standingWater[1] = 0.;
	litter_int_storage = 0.;

	ForEachVegType(k) {
		veg_int_storage[k] = 0.;
	}
}


/* *************************************************** */
/* *************************************************** */
/*            The Water Flow                           */
/* --------------------------------------------------- */
void SW_Water_Flow(void) {
	#ifdef SWDEBUG
	IntUS debug = 0, debug_year = 1980, debug_doy = 350;
	double Eveg, Tveg, HRveg;
	#endif

  SW_VEGPROD *v = &SW_VegProd;
  SW_SOILWAT *sw = &SW_Soilwat;
  SW_WEATHER *w = &SW_Weather;

	RealD swpot_avg[NVEGTYPES],
		transp_veg[NVEGTYPES], transp_rate[NVEGTYPES],
		soil_evap[NVEGTYPES], soil_evap_rate[NVEGTYPES], soil_evap_rate_bs = 1.,
		surface_evap_veg_rate[NVEGTYPES],
		surface_evap_litter_rate = 1., surface_evap_standingWater_rate = 1.,
		h2o_for_soil = 0., snowmelt,
		scale_veg[NVEGTYPES],
		pet2, peti, rate_help, x;

	int doy, month, k;
	LyrIndex i;

	doy = SW_Model.doy; /* base1 */
	month = SW_Model.month; /* base0 */

	records2arrays();

	#ifdef SWDEBUG
	if (debug && SW_Model.year == debug_year && SW_Model.doy == debug_doy) {
		swprintf("Flow (%d-%d): start:", SW_Model.year, SW_Model.doy);
		ForEachSoilLayer(i) {
			swprintf(" swc[%i]=%1.3f", i, lyrSWCBulk[i]);
		}
		swprintf("\n");
	}
	#endif


	if (SW_Site.use_soil_temp && !soil_temp_init) {
		/* We initialize soil temperature (and un/frozen state of soil layers)
			 before water flow of first day because we use un/frozen states), but
			 calculate soil temperature at end of each day
		*/
		SW_ST_setup_run(
			w->now.temp_avg[Today],
			lyrSWCBulk,
			lyrSWCBulk_Saturated,
			lyrbDensity,
			lyrWidths,
			lyroldavgLyrTemp,
			surfaceAvg,
			SW_Site.n_layers,
			lyrSWCBulk_FieldCaps,
			lyrSWCBulk_Wiltpts,
			SW_Site.Tsoil_constant,
			SW_Site.stDeltaX,
			SW_Site.stMaxDepth,
			SW_Site.stNRGR,
			&SW_Soilwat.soiltempError
		);
	}


	/* Solar radiation and PET */
	x = v->bare_cov.albedo * v->bare_cov.fCover;
	ForEachVegType(k)
	{
		x += v->veg[k].cov.albedo * v->veg[k].cov.fCover;
	}

	sw->H_gt = solar_radiation(
		doy,
		SW_Site.latitude,
		SW_Site.altitude,
		SW_Site.slope,
		SW_Site.aspect,
		x,
		SW_Sky.cloudcov_daily[doy],
		SW_Sky.r_humidity_daily[doy],
		w->now.temp_avg[Today],
		&sw->H_oh,
		&sw->H_ot,
		&sw->H_gh
	);

	sw->pet = SW_Site.pet_scale * petfunc(
		sw->H_gt,
		w->now.temp_avg[Today],
		SW_Site.altitude,
		x,
		SW_Sky.r_humidity_daily[doy],
		SW_Sky.windspeed_daily[doy],
		SW_Sky.cloudcov_daily[doy]
	);


	/* snowdepth scaling */
	sw->snowdepth = SW_SnowDepth(sw->snowpack[Today], SW_Sky.snow_density_daily[doy]);
	/* if snow depth is deeper than vegetation height then
	 - rain and snowmelt infiltrates directly to soil (no vegetation or litter interception of today)
	 only
	 - evaporation of yesterdays interception
	 - infiltrate water high
	 - infiltrate water low */

	ForEachVegType(k)
	{
		scale_veg[k] = v->veg[k].cov.fCover;

		if (GT(v->veg[k].veg_height_daily[doy], 0.)) {
			scale_veg[k] *= 1. - sw->snowdepth / v->veg[k].veg_height_daily[doy];
		}
	}

	/* Rainfall interception */
	h2o_for_soil = w->now.rain[Today]; /* ppt is partioned into ppt = snow + rain */

  ForEachVegType(k)
  {
    if (GT(h2o_for_soil, 0.) && GT(scale_veg[k], 0.))
    {
      /* canopy interception only if rainfall, if vegetation cover > 0, and if
          plants are taller than snowpack depth */
      veg_intercepted_water(&h2o_for_soil,
        &sw->int_veg[k], &veg_int_storage[k],
        SW_Sky.n_rain_per_day[month], v->veg[k].veg_kSmax,
        v->veg[k].bLAI_total_daily[doy], scale_veg[k]);

    } else {
      sw->int_veg[k] = 0.;
    }
  }

  sw->litter_int = 0.;

  if (GT(h2o_for_soil, 0.) && EQ(sw->snowpack[Today], 0.)) {
    /* litter interception only when no snow and if rainfall reaches litter */
    ForEachVegType(k)
    {
      if (GT(v->veg[k].cov.fCover, 0.)) {
        litter_intercepted_water(&h2o_for_soil,
          &sw->litter_int, &litter_int_storage,
          SW_Sky.n_rain_per_day[month], v->veg[k].lit_kSmax,
          v->veg[k].litter_daily[doy], v->veg[k].cov.fCover);
      }
    }
  }

	/* End Interception */


	/* Surface water */
	standingWater[Today] = standingWater[Yesterday];

	/* Snow melt infiltrates un-intercepted */
	snowmelt = fmax( 0., w->snowmelt * (1. - w->pct_snowRunoff/100.) ); /* amount of snowmelt is changed by runon/off as percentage */
	w->snowRunoff = w->snowmelt - snowmelt;
	h2o_for_soil += snowmelt;

	/* @brief Surface water runon:
			Proportion of water that arrives at surface added as daily runon from a hypothetical
				identical neighboring upslope site.
			@param percentRunon Value ranges between 0 and +inf; 0 = no runon,
				>0 runon is occurring.
	*/
	if (GT(SW_Site.percentRunon, 0.)) {
		// Calculate 'rain + snowmelt - interception - infiltration' for upslope neighbor
		// Copy values to simulate identical upslope neighbor site
		ForEachSoilLayer(i) {
			UpNeigh_lyrSWCBulk[i] = lyrSWCBulk[i];
			UpNeigh_lyrDrain[i] = lyrDrain[i];
		}
		UpNeigh_drainout = drainout;
		UpNeigh_standingWater = standingWater[Today];

		// Infiltrate for upslope neighbor under saturated soil conditions
		infiltrate_water_high(UpNeigh_lyrSWCBulk, UpNeigh_lyrDrain, &UpNeigh_drainout,
			h2o_for_soil, SW_Site.n_layers, lyrSWCBulk_FieldCaps, lyrSWCBulk_Saturated,
			lyrImpermeability, &UpNeigh_standingWater);

		// Runon as percentage from today's surface water addition on upslope neighbor
		w->surfaceRunon = fmax(0., (UpNeigh_standingWater - standingWater[Yesterday]) * SW_Site.percentRunon);
		standingWater[Today] += w->surfaceRunon;

	} else {
		w->surfaceRunon = 0.;
	}

	/* Soil infiltration */
	w->soil_inf = h2o_for_soil;

	/* Percolation under saturated soil conditions */
	w->soil_inf += standingWater[Today];
	infiltrate_water_high(lyrSWCBulk, lyrDrain, &drainout, h2o_for_soil, SW_Site.n_layers,
		lyrSWCBulk_FieldCaps, lyrSWCBulk_Saturated, lyrImpermeability, &standingWater[Today]);
	w->soil_inf -= standingWater[Today]; // adjust soil_infiltration for not infiltrated surface water

	#ifdef SWDEBUG
	if (debug && SW_Model.year == debug_year && SW_Model.doy == debug_doy) {
		swprintf("Flow (%d-%d): satperc:", SW_Model.year, SW_Model.doy);
		ForEachSoilLayer(i) {
			swprintf(" swc[%i]=%1.3f", i, lyrSWCBulk[i]);
		}
		swprintf("\n              : satperc:");
		ForEachSoilLayer(i) {
			swprintf(" perc[%d]=%1.3f", i, lyrDrain[i]);
		}
		swprintf("\n");
	}
	#endif

	/* @brief Surface water runoff:
			Proportion of ponded surface water removed as daily runoff.
			@param percentRunoff Value ranges between 0 and 1; 0 = no loss of surface water,
			1 = all ponded water lost via runoff.
	*/
	if (GT(SW_Site.percentRunoff, 0.)) {
		w->surfaceRunoff = standingWater[Today] * SW_Site.percentRunoff;
		standingWater[Today] = fmax(0.0, (standingWater[Today] - w->surfaceRunoff));

	} else {
		w->surfaceRunoff = 0.;
	}

	// end surface water and infiltration


	/* Potential bare-soil evaporation rates */
	if (GT(v->bare_cov.fCover, 0.) && EQ(sw->snowpack[Today], 0.)) /* bare ground present AND no snow on ground */
	{
		pot_soil_evap_bs(&soil_evap_rate_bs, SW_Site.n_evap_lyrs, lyrEvapCo, sw->pet,
			SW_Site.evap.xinflec, SW_Site.evap.slope, SW_Site.evap.yinflec,
			SW_Site.evap.range, lyrWidths, lyrSWCBulk);
		soil_evap_rate_bs *= v->bare_cov.fCover;

	} else {
		soil_evap_rate_bs = 0;
	}


	/* Potential transpiration & bare-soil evaporation rates */
	ForEachVegType(k)
	{
		if (GT(scale_veg[k], 0.)) {
			/* vegetation type k present AND not fully covered in snow */
			EsT_partitioning(&soil_evap[k], &transp_veg[k], v->veg[k].lai_live_daily[doy],
				v->veg[k].EsTpartitioning_param);

			if (EQ(sw->snowpack[Today], 0.)) { /* bare-soil evaporation only when no snow */
				pot_soil_evap(&soil_evap_rate[k], SW_Site.n_evap_lyrs, lyrEvapCo,
					v->veg[k].total_agb_daily[doy], soil_evap[k], sw->pet,
					SW_Site.evap.xinflec, SW_Site.evap.slope, SW_Site.evap.yinflec, SW_Site.evap.range,
					lyrWidths, lyrSWCBulk, v->veg[k].Es_param_limit);

				soil_evap_rate[k] *= v->veg[k].cov.fCover;

			} else {
				soil_evap_rate[k] = 0.;
			}

			transp_weighted_avg(&swpot_avg[k], SW_Site.n_transp_rgn, SW_Site.n_transp_lyrs[k],
				lyrTrRegions[k], lyrTranspCo[k], lyrSWCBulk);

			pot_transp(&transp_rate[k], swpot_avg[k],
				v->veg[k].biolive_daily[doy], v->veg[k].biodead_daily[doy],
				transp_veg[k], sw->pet,
				SW_Site.transp.xinflec, SW_Site.transp.slope, SW_Site.transp.yinflec, SW_Site.transp.range,
				v->veg[k].shade_scale, v->veg[k].shade_deadmax,
				v->veg[k].tr_shade_effects.xinflec, v->veg[k].tr_shade_effects.slope,
				v->veg[k].tr_shade_effects.yinflec, v->veg[k].tr_shade_effects.range,
				v->veg[k].co2_multipliers[WUE_INDEX][SW_Model.simyear]);

			transp_rate[k] *= scale_veg[k];

		} else {
			soil_evap_rate[k] = 0.;
			transp_rate[k] = 0.;
		}
	}


	/* Snow sublimation takes precedence over other ET fluxes:
		see functions `SW_SWC_adjust_snow` and `SW_SWC_snowloss`*/
	w->snowloss = SW_SWC_snowloss(sw->pet, &sw->snowpack[Today]);
	pet2 = fmax(0., sw->pet - w->snowloss);

	/* Potential evaporation rates of intercepted and surface water */
	peti = pet2;
	ForEachVegType(k) {
		surface_evap_veg_rate[k] = fmax(0.,
		  fmin(peti * scale_veg[k], veg_int_storage[k]));
		peti -= surface_evap_veg_rate[k] / scale_veg[k];
	}

	surface_evap_litter_rate = fmax(0., fmin(peti, litter_int_storage));
	peti -= surface_evap_litter_rate;
	surface_evap_standingWater_rate = fmax(0., fmin(peti, standingWater[Today]));
	peti -= surface_evap_standingWater_rate;

	/* Scale all (potential) evaporation and transpiration flux rates to (PET - Esnow) */
	rate_help = surface_evap_litter_rate + surface_evap_standingWater_rate +
		soil_evap_rate_bs;
	ForEachVegType(k) {
		rate_help += surface_evap_veg_rate[k] + soil_evap_rate[k] + transp_rate[k];
	}

	if (GT(rate_help, pet2)) {
		rate_help = pet2 / rate_help;

		ForEachVegType(k)
		{
			surface_evap_veg_rate[k] *= rate_help;
			soil_evap_rate[k] *= rate_help;
			transp_rate[k] *= rate_help;
		}

		surface_evap_litter_rate *= rate_help;
		surface_evap_standingWater_rate *= rate_help;
		soil_evap_rate_bs *= rate_help;
	}

	/* Start adding components to AET */
	sw->aet = w->snowloss; /* init aet for the day */

	/* Evaporation of intercepted and surface water */
	ForEachVegType(k)
	{
		evap_fromSurface(&veg_int_storage[k], &surface_evap_veg_rate[k], &sw->aet);
		sw->evap_veg[k] = surface_evap_veg_rate[k];
	}

	evap_fromSurface(&litter_int_storage, &surface_evap_litter_rate, &sw->aet);
	evap_fromSurface(&standingWater[Today], &surface_evap_standingWater_rate, &sw->aet);

	sw->litter_evap = surface_evap_litter_rate;
	sw->surfaceWater_evap = surface_evap_standingWater_rate;

	/* bare-soil evaporation */
	if (GT(v->bare_cov.fCover, 0.) && EQ(sw->snowpack[Today], 0.)) {
		/* remove bare-soil evap from swv */
		remove_from_soil(lyrSWCBulk, lyrEvap_BareGround, &sw->aet, SW_Site.n_evap_lyrs,
			lyrEvapCo, soil_evap_rate_bs, lyrSWCBulk_HalfWiltpts);

	} else {
		/* Set daily array to zero, no evaporation */
		ForEachEvapLayer(i) {
			lyrEvap_BareGround[i] = 0.;
		}
	}

	#ifdef SWDEBUG
	if (debug && SW_Model.year == debug_year && SW_Model.doy == debug_doy) {
		swprintf("Flow (%d-%d): Esoil:", SW_Model.year, SW_Model.doy);
		ForEachSoilLayer(i) {
			swprintf(" swc[%i]=%1.3f", i, lyrSWCBulk[i]);
		}
		swprintf("\n              : Esoil:");
		ForEachSoilLayer(i) {
			swprintf(" Esoil[%d]=%1.3f", i, lyrEvap_BareGround[i]);
		}
		swprintf("\n");
	}
	#endif

	/* Vegetation transpiration and bare-soil evaporation */
	ForEachVegType(k)
	{
		if (GT(scale_veg[k], 0.)) {
			/* remove bare-soil evap from swc */
			remove_from_soil(lyrSWCBulk, lyrEvap[k], &sw->aet, SW_Site.n_evap_lyrs,
				lyrEvapCo, soil_evap_rate[k], lyrSWCBulk_HalfWiltpts);

			/* remove transp from swc */
			remove_from_soil(lyrSWCBulk, lyrTransp[k], &sw->aet, SW_Site.n_transp_lyrs[k],
				lyrTranspCo[k], transp_rate[k], lyrSWCBulk_atSWPcrit[k]);

		} else {
			/* Set daily array to zero, no evaporation or transpiration */
			ForEachSoilLayer(i) {
				lyrTransp[k][i] = lyrEvap[k][i] = 0.;
			}
		}
	}

	#ifdef SWDEBUG
	if (debug && SW_Model.year == debug_year && SW_Model.doy == debug_doy) {
		swprintf("Flow (%d-%d): ETveg:", SW_Model.year, SW_Model.doy);
		ForEachSoilLayer(i) {
			swprintf(" swc[%i]=%1.3f", i, lyrSWCBulk[i]);
		}
		swprintf("\n              : ETveg:");
		ForEachSoilLayer(i) {
			Eveg = Tveg = 0.;
			ForEachVegType(k) {
				Eveg += lyrEvap[k][i];
				Tveg += lyrTransp[k][i];
			}
			swprintf(" Tveg[%d]=%1.3f/Eveg=%1.3f", i, Tveg, Eveg);
		}
		swprintf("\n");
	}
	#endif


	/* Hydraulic redistribution */
	ForEachVegTypeBottomUp(k) {
		if (
			v->veg[k].flagHydraulicRedistribution &&
			GT(v->veg[k].cov.fCover, 0.) &&
			GT(v->veg[k].biolive_daily[doy], 0.)
		) {

			hydraulic_redistribution(
				lyrSWCBulk,
				lyrHydRed[k],
				k,
				SW_Site.n_layers,
				SW_Site.lyr,
				sw->lyrFrozen,
				v->veg[k].maxCondroot,
				v->veg[k].swpMatric50,
				v->veg[k].shapeCond,
				v->veg[k].cov.fCover
			);

		} else {
			/* Set daily array to zero */
			ForEachSoilLayer(i) {
				lyrHydRed[k][i] = 0.;
			}
		}
	}

	#ifdef SWDEBUG
	if (debug && SW_Model.year == debug_year && SW_Model.doy == debug_doy) {
		swprintf("Flow (%d-%d): HR:", SW_Model.year, SW_Model.doy);
		ForEachSoilLayer(i) {
			swprintf(" swc[%i]=%1.3f", i, lyrSWCBulk[i]);
		}
		swprintf("\n              : HR:");
		ForEachSoilLayer(i) {
			HRveg = 0.;
			ForEachVegType(k) {
				HRveg += lyrHydRed[k][i];
			}
			swprintf(" HRveg[%d]=%1.3f", i, HRveg);
		}
		swprintf("\n");
	}
	#endif


	/* Calculate percolation for unsaturated soil water conditions. */
	/* 01/06/2011	(drs) call to percolate_unsaturated() has to be the last swc
		 affecting calculation */

	w->soil_inf += standingWater[Today];

	/* Unsaturated percolation based on Parton 1978, Black et al. 1969 */
	percolate_unsaturated(
		lyrSWCBulk,
		lyrDrain,
		&drainout,
		&standingWater[Today],
		SW_Site.n_layers,
		SW_Site.lyr,
		sw->lyrFrozen,
		SW_Site.slow_drain_coeff,
		SLOW_DRAIN_DEPTH
	);

	// adjust soil_infiltration for water pushed back to surface
	w->soil_inf -= standingWater[Today];

	sw->surfaceWater = standingWater[Today];

	#ifdef SWDEBUG
	if (debug && SW_Model.year == debug_year && SW_Model.doy == debug_doy) {
		swprintf("Flow (%d-%d): unsatperc:", SW_Model.year, SW_Model.doy);
		ForEachSoilLayer(i) {
			swprintf(" swc[%i]=%1.3f", i, lyrSWCBulk[i]);
		}
		swprintf("\n              : satperc:");
		ForEachSoilLayer(i) {
			swprintf(" perc[%d]=%1.3f", i, lyrDrain[i]);
		}
		swprintf("\n");
	}
	#endif


	/* Soil Temperature starts here */

	// computing the live biomass real quickly to condense the call to soil_temperature
	x = 0.;
	ForEachVegType(k)
	{
		if (k == SW_TREES || k == SW_SHRUB) {
			// changed to exclude tree biomass, bMatric/c it was breaking the soil_temperature function
			x += v->veg[k].biolive_daily[doy] * v->veg[k].cov.fCover;
		} else {
			x += v->veg[k].biomass_daily[doy] * v->veg[k].cov.fCover;
		}
	}

	// soil_temperature function computes the soil temp for each layer and stores it in lyravgLyrTemp
	// doesn't affect SWC at all (yet), but needs it for the calculation, so therefore the temperature is the last calculation done
	if (SW_Site.use_soil_temp) {
		soil_temperature(w->now.temp_avg[Today], sw->pet, sw->aet, x, lyrSWCBulk,
			lyrSWCBulk_Saturated, lyrbDensity, lyrWidths, lyroldavgLyrTemp, lyravgLyrTemp, surfaceAvg,
			SW_Site.n_layers, SW_Site.bmLimiter,
			SW_Site.t1Param1, SW_Site.t1Param2, SW_Site.t1Param3, SW_Site.csParam1,
			SW_Site.csParam2, SW_Site.shParam, sw->snowdepth, SW_Site.Tsoil_constant,
			SW_Site.stDeltaX, SW_Site.stMaxDepth, SW_Site.stNRGR, sw->snowpack[Today],
			&SW_Soilwat.soiltempError, w->now.temp_max[Today], w->now.temp_min[Today],
            sw->H_gt, sw->maxLyrTemperature, sw->minLyrTemperature, &w->surfaceMax, &w->surfaceMin);
	}

	/* Soil Temperature ends here */

	/* Move local values into main arrays */
	arrays2records();

	standingWater[Yesterday] = standingWater[Today];

} /* END OF WATERFLOW */

