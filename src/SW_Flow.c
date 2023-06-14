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

#include "include/SW_Model.h"
#include "include/SW_Site.h"
#include "include/SW_SoilWater.h"
#include "include/SW_Flow_lib.h"
#include "include/SW_VegProd.h"
#include "include/SW_Sky.h"
#include "include/SW_Times.h"

#include "include/SW_Flow_lib_PET.h"
#include "include/SW_Flow.h"


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

	@param[out] SW_SoilWat Struct of type SW_SOILWAT containing
		soil water related values
*/
void SW_FLW_init_run(SW_SOILWAT* SW_SoilWat) {
	/* 06/26/2013	(rjm) added function SW_FLW_init_run() to init global variables between consecutive calls to SoilWat as dynamic library */
	int i, k;


	//These only have to be cleared if a loop is wrong in the code.
	for (i = 0; i < MAX_LAYERS; i++) {
		ForEachVegType(k) {
			SW_SoilWat->transpiration[k][i] = 0.;
			SW_SoilWat->hydred[k][i] = 0;
		}

		SW_SoilWat->swcBulk[Today][i] = 0.;
		SW_SoilWat->drain[i] = 0.;
	}

	//When running as a library make sure these are set to zero.
	SW_SoilWat->standingWater[0] = SW_SoilWat->standingWater[1] = 0.;
	SW_SoilWat->litter_int_storage = 0.;

	ForEachVegType(k) {
		SW_SoilWat->veg_int_storage[k] = 0.;
	}
}


/* *************************************************** */
/* *************************************************** */
/*            The Water Flow                           */
/* --------------------------------------------------- */
void SW_Water_Flow(SW_ALL* sw, LOG_INFO* LogInfo) {
	#ifdef SWDEBUG
	IntUS debug = 0, debug_year = 1980, debug_doy = 350;
	double Eveg, Tveg, HRveg;
	#endif

	RealD swpot_avg[NVEGTYPES],
		transp_veg[NVEGTYPES], transp_rate[NVEGTYPES],
		soil_evap[NVEGTYPES], soil_evap_rate[NVEGTYPES], soil_evap_rate_bs = 1.,
		surface_evap_veg_rate[NVEGTYPES],
		surface_evap_litter_rate = 1., surface_evap_standingWater_rate = 1.,
		h2o_for_soil = 0., snowmelt,
		scale_veg[NVEGTYPES],
		pet2, peti, rate_help, x,
		drainout = 0,
		*standingWaterToday = &sw->SoilWat.standingWater[Today],
		*standingWaterYesterday = &sw->SoilWat.standingWater[Yesterday];

	int doy, month, k;
	LyrIndex i, n_layers = sw->Site.n_layers;

	RealD UpNeigh_lyrSWCBulk[MAX_LAYERS], UpNeigh_lyrDrain[MAX_LAYERS];
	RealD UpNeigh_drainout, UpNeigh_standingWater;

	doy = sw->Model.doy; /* base1 */
	month = sw->Model.month; /* base0 */

	#ifdef SWDEBUG
	if (debug && sw->Model.year == debug_year && sw->Model.doy == debug_doy) {
		swprintf("Flow (%d-%d): start:", sw->Model.year, sw->Model.doy);
		ForEachSoilLayer(i, n_layers) {
			swprintf(" swc[%i]=%1.3f", i, sw->SoilWat.swcBulk[Today][i]);
		}
		swprintf("\n");
	}
	#endif


	if (sw->Site.use_soil_temp && !sw->StRegValues.soil_temp_init) {
		/* We initialize soil temperature (and un/frozen state of soil layers)
			 before water flow of first day because we use un/frozen states), but
			 calculate soil temperature at end of each day
		*/
		SW_ST_setup_run(
			&sw->StRegValues,
			&sw->Site,
			&sw->SoilWat.soiltempError,
			&sw->StRegValues.soil_temp_init,
			sw->Weather.now.temp_avg,
			sw->SoilWat.swcBulk[Today],
			sw->SoilWat.avgLyrTemp, // yesterday's soil temperature values
			&sw->Weather.surfaceAvg, // yesterday's soil surface temperature
			sw->SoilWat.lyrFrozen,
			LogInfo
		);
	}


	/* Solar radiation and PET */
	x = sw->VegProd.bare_cov.albedo * sw->VegProd.bare_cov.fCover;
	ForEachVegType(k)
	{
		x += sw->VegProd.veg[k].cov.albedo * sw->VegProd.veg[k].cov.fCover;
	}

	sw->SoilWat.H_gt = solar_radiation(
		&sw->AtmDemand,
		doy,
		sw->Site.latitude,
		sw->Site.altitude,
		sw->Site.slope,
		sw->Site.aspect,
		x,
        &sw->Weather.now.cloudCover,
        sw->Weather.now.actualVaporPressure,
        sw->Weather.now.shortWaveRad,
        sw->Weather.desc_rsds,
		&sw->SoilWat.H_oh,
		&sw->SoilWat.H_ot,
		&sw->SoilWat.H_gh,
		LogInfo
	);

	sw->SoilWat.pet = sw->Site.pet_scale * petfunc(
		sw->SoilWat.H_gt,
		sw->Weather.now.temp_avg,
		sw->Site.altitude,
		x,
        sw->Weather.now.relHumidity,
        sw->Weather.now.windSpeed,
        sw->Weather.now.cloudCover,
		LogInfo
	);


	/* snowdepth scaling */
	sw->SoilWat.snowdepth = SW_SnowDepth(sw->SoilWat.snowpack[Today],
											sw->Sky.snow_density_daily[doy]);
	/* if snow depth is deeper than vegetation height then
	 - rain and snowmelt infiltrates directly to soil (no vegetation or litter interception of today)
	 only
	 - evaporation of yesterdays interception
	 - infiltrate water high
	 - infiltrate water low */

	ForEachVegType(k)
	{
		scale_veg[k] = sw->VegProd.veg[k].cov.fCover;

		if (GT(sw->VegProd.veg[k].veg_height_daily[doy], 0.)) {
			scale_veg[k] *= 1. - sw->SoilWat.snowdepth / sw->VegProd.veg[k].veg_height_daily[doy];
		}
	}

	/* Rainfall interception */
	h2o_for_soil = sw->Weather.now.rain; /* ppt is partioned into ppt = snow + rain */

  ForEachVegType(k)
  {
    if (GT(h2o_for_soil, 0.) && GT(scale_veg[k], 0.))
    {
      /* canopy interception only if rainfall, if vegetation cover > 0, and if
          plants are taller than snowpack depth */
      veg_intercepted_water(&h2o_for_soil,
        &sw->SoilWat.int_veg[k], &sw->SoilWat.veg_int_storage[k],
        sw->Sky.n_rain_per_day[month], sw->VegProd.veg[k].veg_kSmax,
        sw->VegProd.veg[k].bLAI_total_daily[doy], scale_veg[k]);

    } else {
      sw->SoilWat.int_veg[k] = 0.;
    }
  }

  sw->SoilWat.litter_int = 0.;

  if (GT(h2o_for_soil, 0.) && EQ(sw->SoilWat.snowpack[Today], 0.)) {
    /* litter interception only when no snow and if rainfall reaches litter */
    ForEachVegType(k)
    {
      if (GT(sw->VegProd.veg[k].cov.fCover, 0.)) {
        litter_intercepted_water(&h2o_for_soil,
          &sw->SoilWat.litter_int, &sw->SoilWat.litter_int_storage,
          sw->Sky.n_rain_per_day[month], sw->VegProd.veg[k].lit_kSmax,
          sw->VegProd.veg[k].litter_daily[doy], sw->VegProd.veg[k].cov.fCover);
      }
    }
  }

	/* End Interception */


	/* Surface water */
	*standingWaterToday = *standingWaterYesterday;

	/* Snow melt infiltrates un-intercepted */
	snowmelt = fmax( 0., sw->Weather.snowmelt * (1. - sw->Weather.pct_snowRunoff/100.) ); /* amount of snowmelt is changed by runon/off as percentage */
	sw->Weather.snowRunoff = sw->Weather.snowmelt - snowmelt;
	h2o_for_soil += snowmelt;

	/* @brief Surface water runon:
			Proportion of water that arrives at surface added as daily runon from a hypothetical
				identical neighboring upslope site.
			@param percentRunon Value ranges between 0 and +inf; 0 = no runon,
				>0 runon is occurring.
	*/
	if (GT(sw->Site.percentRunon, 0.)) {
		// Calculate 'rain + snowmelt - interception - infiltration' for upslope neighbor
		// Copy values to simulate identical upslope neighbor site
		ForEachSoilLayer(i, n_layers) {
			UpNeigh_lyrSWCBulk[i] = sw->SoilWat.swcBulk[Today][i];
			UpNeigh_lyrDrain[i] = sw->SoilWat.drain[i];
		}
		UpNeigh_drainout = drainout;
		UpNeigh_standingWater = *standingWaterToday;

		// Infiltrate for upslope neighbor under saturated soil conditions
		infiltrate_water_high(UpNeigh_lyrSWCBulk, UpNeigh_lyrDrain,
				&UpNeigh_drainout, h2o_for_soil, n_layers,
				sw->Site.swcBulk_fieldcap, sw->Site.swcBulk_saturated,
				sw->Site.impermeability, &UpNeigh_standingWater,
				sw->SoilWat.lyrFrozen);

		// Runon as percentage from today's surface water addition on upslope neighbor
		sw->Weather.surfaceRunon =
			fmax(0.,
			(UpNeigh_standingWater - sw->SoilWat.standingWater[Yesterday]) *
			 sw->Site.percentRunon);
		sw->SoilWat.standingWater[Today] += sw->Weather.surfaceRunon;

	} else {
		sw->Weather.surfaceRunon = 0.;
	}

	/* Soil infiltration */
	sw->Weather.soil_inf = h2o_for_soil;

	/* Percolation under saturated soil conditions */
	sw->Weather.soil_inf += *standingWaterToday;
	infiltrate_water_high(sw->SoilWat.swcBulk[Today], sw->SoilWat.drain,
		&drainout, h2o_for_soil, n_layers, sw->Site.swcBulk_fieldcap,
		sw->Site.swcBulk_saturated, sw->Site.impermeability,
		standingWaterToday, sw->SoilWat.lyrFrozen);

	sw->Weather.soil_inf -= *standingWaterToday; // adjust soil_infiltration for not infiltrated surface water

	#ifdef SWDEBUG
	if (debug && sw->Model.year == debug_year && sw->Model.doy == debug_doy) {
		swprintf("Flow (%d-%d): satperc:", sw->Model.year, sw->Model.doy);
		ForEachSoilLayer(i, n_layers) {
			swprintf(" swc[%i]=%1.3f", i, sw->SoilWat.swcBulk[Today][i]);
		}
		swprintf("\n              : satperc:");
		ForEachSoilLayer(i, n_layers) {
			swprintf(" perc[%d]=%1.3f", i, sw->SoilWat.drain[i]);
		}
		swprintf("\n");
	}
	#endif

	/* @brief Surface water runoff:
			Proportion of ponded surface water removed as daily runoff.
			@param percentRunoff Value ranges between 0 and 1; 0 = no loss of surface water,
			1 = all ponded water lost via runoff.
	*/
	if (GT(sw->Site.percentRunoff, 0.)) {
		sw->Weather.surfaceRunoff = *standingWaterToday * sw->Site.percentRunoff;
		*standingWaterToday =
			fmax(0.,
			(*standingWaterToday - sw->Weather.surfaceRunoff));

	} else {
		sw->Weather.surfaceRunoff = 0.;
	}

	// end surface water and infiltration


	/* Potential bare-soil evaporation rates */
	if (GT(sw->VegProd.bare_cov.fCover, 0.) && EQ(sw->SoilWat.snowpack[Today], 0.)) /* bare ground present AND no snow on ground */
	{
		pot_soil_evap_bs(&soil_evap_rate_bs, &sw->Site,
			sw->Site.n_evap_lyrs, sw->SoilWat.pet,
			sw->Site.evap.xinflec, sw->Site.evap.slope, sw->Site.evap.yinflec,
			sw->Site.evap.range, sw->SoilWat.swcBulk[Today], LogInfo);
		soil_evap_rate_bs *= sw->VegProd.bare_cov.fCover;

	} else {
		soil_evap_rate_bs = 0;
	}


	/* Potential transpiration & bare-soil evaporation rates */
	ForEachVegType(k)
	{
		if (GT(scale_veg[k], 0.)) {
			/* vegetation type k present AND not fully covered in snow */
			EsT_partitioning(&soil_evap[k], &transp_veg[k], sw->VegProd.veg[k].lai_live_daily[doy],
				sw->VegProd.veg[k].EsTpartitioning_param);

			if (EQ(sw->SoilWat.snowpack[Today], 0.)) { /* bare-soil evaporation only when no snow */
				pot_soil_evap(&sw->Site, sw->Site.n_evap_lyrs,
					sw->VegProd.veg[k].total_agb_daily[doy], soil_evap[k],
					sw->SoilWat.pet, sw->Site.evap.xinflec,
					sw->Site.evap.slope, sw->Site.evap.yinflec,
					sw->Site.evap.range, sw->SoilWat.swcBulk[Today],
					sw->VegProd.veg[k].Es_param_limit,
					&soil_evap_rate[k], LogInfo);

				soil_evap_rate[k] *= sw->VegProd.veg[k].cov.fCover;

			} else {
				soil_evap_rate[k] = 0.;
			}

			transp_weighted_avg(&swpot_avg[k], &sw->Site,
				sw->Site.n_transp_rgn, sw->Site.n_transp_lyrs[k],
				sw->Site.my_transp_rgn[k], sw->SoilWat.swcBulk[Today],
				k, LogInfo);

			pot_transp(&transp_rate[k], swpot_avg[k],
				sw->VegProd.veg[k].biolive_daily[doy], sw->VegProd.veg[k].biodead_daily[doy],
				transp_veg[k], sw->SoilWat.pet,
				sw->Site.transp.xinflec, sw->Site.transp.slope, sw->Site.transp.yinflec, sw->Site.transp.range,
				sw->VegProd.veg[k].shade_scale, sw->VegProd.veg[k].shade_deadmax,
				sw->VegProd.veg[k].tr_shade_effects.xinflec, sw->VegProd.veg[k].tr_shade_effects.slope,
				sw->VegProd.veg[k].tr_shade_effects.yinflec, sw->VegProd.veg[k].tr_shade_effects.range,
				sw->VegProd.veg[k].co2_multipliers[WUE_INDEX][sw->Model.simyear]);

			transp_rate[k] *= scale_veg[k];

		} else {
			soil_evap_rate[k] = 0.;
			transp_rate[k] = 0.;
		}
	}


	/* Snow sublimation takes precedence over other ET fluxes:
		see functions `SW_SWC_adjust_snow` and `SW_SWC_snowloss`*/
	sw->Weather.snowloss = SW_SWC_snowloss(sw->SoilWat.pet, &sw->SoilWat.snowpack[Today]);
	pet2 = fmax(0., sw->SoilWat.pet - sw->Weather.snowloss);

	/* Potential evaporation rates of intercepted and surface water */
	peti = pet2;
	ForEachVegType(k) {
		surface_evap_veg_rate[k] = fmax(0.,
		  fmin(peti * scale_veg[k], sw->SoilWat.veg_int_storage[k]));
		peti -= surface_evap_veg_rate[k] / scale_veg[k];
	}

	surface_evap_litter_rate = fmax(0., fmin(peti, sw->SoilWat.litter_int_storage));
	peti -= surface_evap_litter_rate;
	surface_evap_standingWater_rate = fmax(0., fmin(peti, *standingWaterToday));
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
	sw->SoilWat.aet = sw->Weather.snowloss; /* init aet for the day */

	/* Evaporation of intercepted and surface water */
	ForEachVegType(k)
	{
		evap_fromSurface(&sw->SoilWat.veg_int_storage[k], &surface_evap_veg_rate[k], &sw->SoilWat.aet);
		sw->SoilWat.evap_veg[k] = surface_evap_veg_rate[k];
	}

	evap_fromSurface(&sw->SoilWat.litter_int_storage, &surface_evap_litter_rate, &sw->SoilWat.aet);
	evap_fromSurface(&sw->SoilWat.standingWater[Today], &surface_evap_standingWater_rate, &sw->SoilWat.aet);

	sw->SoilWat.litter_evap = surface_evap_litter_rate;
	sw->SoilWat.surfaceWater_evap = surface_evap_standingWater_rate;


	/* bare-soil evaporation */
	ForEachEvapLayer(i, sw->Site.n_evap_lyrs) {
		sw->SoilWat.evap_baresoil[i] = 0; // init to zero for today
	}

	if (GT(sw->VegProd.bare_cov.fCover, 0.) && EQ(sw->SoilWat.snowpack[Today], 0.)) {
		/* remove bare-soil evap from swv */
		remove_from_soil(sw->SoilWat.swcBulk[Today], sw->SoilWat.evap_baresoil,
			&sw->Site, &sw->SoilWat.aet, sw->Site.n_evap_lyrs, sw->Site.evap_coeff,
			soil_evap_rate_bs, sw->Site.swcBulk_halfwiltpt, sw->SoilWat.lyrFrozen,
			LogInfo);
	}

	#ifdef SWDEBUG
	if (debug && sw->Model.year == debug_year && sw->Model.doy == debug_doy) {
		swprintf("Flow (%d-%d): Esoil:", sw->Model.year, sw->Model.doy);
		ForEachSoilLayer(i, n_layers) {
			swprintf(" swc[%i]=%1.3f", i, sw->SoilWat.swcBulk[Today][i]);
		}
		swprintf("\n              : Esoil:");
		ForEachSoilLayer(i, n_layers) {
			swprintf(" Esoil[%d]=%1.3f", i, sw->SoilWat.evap_baresoil[i]);
		}
		swprintf("\n");
	}
	#endif

	/* Vegetation transpiration and bare-soil evaporation */
	ForEachVegType(k)
	{
		ForEachSoilLayer(i, n_layers) {
			sw->SoilWat.transpiration[k][i] = 0; // init to zero for today
		}

		if (GT(scale_veg[k], 0.)) {
			/* remove bare-soil evap from swc */
			remove_from_soil(sw->SoilWat.swcBulk[Today], sw->SoilWat.evap_baresoil,
				&sw->Site, &sw->SoilWat.aet, sw->Site.n_evap_lyrs,
				sw->Site.evap_coeff, soil_evap_rate[k],
				sw->Site.swcBulk_halfwiltpt, sw->SoilWat.lyrFrozen,
				LogInfo);

			/* remove transp from swc */
			remove_from_soil(sw->SoilWat.swcBulk[Today], sw->SoilWat.transpiration[k],
				&sw->Site, &sw->SoilWat.aet, sw->Site.n_transp_lyrs[k],
				sw->Site.transp_coeff[k], transp_rate[k],
				sw->Site.swcBulk_atSWPcrit[k], sw->SoilWat.lyrFrozen,
				LogInfo);
		}
	}

	#ifdef SWDEBUG
	if (debug && sw->Model.year == debug_year && sw->Model.doy == debug_doy) {
		swprintf("Flow (%d-%d): ETveg:", sw->Model.year, sw->Model.doy);
		ForEachSoilLayer(i, n_layers) {
			swprintf(" swc[%i]=%1.3f", i, sw->SoilWat.swcBulk[Today][i]);
		}
		swprintf("\n              : ETveg:");
		Eveg = 0.;
		ForEachSoilLayer(i, n_layers) {
			Tveg = 0.;
			Eveg += sw->SoilWat.evap_baresoil[i];
			ForEachVegType(k) {
				Tveg += sw->SoilWat.transpiration[k][i];
			}
			swprintf(" Tveg[%d]=%1.3f/Eveg=%1.3f", i, Tveg, Eveg);
		}
		swprintf("\n");
	}
	#endif


	/* Hydraulic redistribution */
	ForEachVegTypeBottomUp(k) {
		if (
			sw->VegProd.veg[k].flagHydraulicRedistribution &&
			GT(sw->VegProd.veg[k].cov.fCover, 0.) &&
			GT(sw->VegProd.veg[k].biolive_daily[doy], 0.)
		) {

			hydraulic_redistribution(
				sw->SoilWat.swcBulk[Today],
				sw->SoilWat.hydred[k],
				&sw->Site,
				k,
				n_layers,
				sw->SoilWat.lyrFrozen,
				sw->VegProd.veg[k].maxCondroot,
				sw->VegProd.veg[k].swpMatric50,
				sw->VegProd.veg[k].shapeCond,
				sw->VegProd.veg[k].cov.fCover,
				sw->Model.year,
				sw->Model.doy,
				LogInfo
			);

		} else {
			/* Set daily array to zero */
			ForEachSoilLayer(i, n_layers) {
				sw->SoilWat.hydred[k][i] = 0.;
			}
		}
	}

	#ifdef SWDEBUG
	if (debug && sw->Model.year == debug_year && sw->Model.doy == debug_doy) {
		swprintf("Flow (%d-%d): HR:", sw->Model.year, sw->Model.doy);
		ForEachSoilLayer(i, n_layers) {
			swprintf(" swc[%i]=%1.3f", i, sw->SoilWat.swcBulk[Today][i]);
		}
		swprintf("\n              : HR:");
		ForEachSoilLayer(i, n_layers) {
			HRveg = 0.;
			ForEachVegType(k) {
				HRveg += sw->SoilWat.hydred[k][i];
			}
			swprintf(" HRveg[%d]=%1.3f", i, HRveg);
		}
		swprintf("\n");
	}
	#endif


	/* Calculate percolation for unsaturated soil water conditions. */
	/* 01/06/2011	(drs) call to percolate_unsaturated() has to be the last swc
		 affecting calculation */

	sw->Weather.soil_inf += *standingWaterToday;

	/* Unsaturated percolation based on Parton 1978, Black et al. 1969 */
	percolate_unsaturated(
		sw->SoilWat.swcBulk[Today],
		sw->SoilWat.drain,
		&drainout,
		standingWaterToday,
		n_layers,
		sw->SoilWat.lyrFrozen,
		&sw->Site,
		sw->Site.slow_drain_coeff,
		SLOW_DRAIN_DEPTH
	);

	// adjust soil_infiltration for water pushed back to surface
	sw->Weather.soil_inf -= *standingWaterToday;

	sw->SoilWat.surfaceWater = *standingWaterToday;

	#ifdef SWDEBUG
	if (debug && sw->Model.year == debug_year && sw->Model.doy == debug_doy) {
		swprintf("Flow (%d-%d): unsatperc:", sw->Model.year, sw->Model.doy);
		ForEachSoilLayer(i, n_layers) {
			swprintf(" swc[%i]=%1.3f", i, sw->SoilWat.swcBulk[Today][i]);
		}
		swprintf("\n              : satperc:");
		ForEachSoilLayer(i, n_layers) {
			swprintf(" perc[%d]=%1.3f", i, sw->SoilWat.drain[i]);
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
			x += sw->VegProd.veg[k].biolive_daily[doy] * sw->VegProd.veg[k].cov.fCover;
		} else {
			x += sw->VegProd.veg[k].biomass_daily[doy] * sw->VegProd.veg[k].cov.fCover;
		}
	}

	// soil_temperature function computes the soil temp for each layer and stores it in lyravgLyrTemp
	// doesn't affect SWC at all (yet), but needs it for the calculation, so therefore the temperature is the last calculation done
	if (sw->Site.use_soil_temp) {
		soil_temperature(&sw->StRegValues,
			&sw->Weather.surfaceMax, &sw->Weather.surfaceMin,
			sw->SoilWat.lyrFrozen, sw->Weather.now.temp_avg, sw->SoilWat.pet,
			sw->SoilWat.aet, x, sw->SoilWat.swcBulk[Today],
			sw->Site.swcBulk_saturated, sw->Site.soilBulk_density,
			sw->Site.width, sw->SoilWat.avgLyrTemp, &sw->Weather.surfaceAvg,
			n_layers, sw->Site.bmLimiter, sw->Site.t1Param1, sw->Site.t1Param2,
			sw->Site.t1Param3, sw->Site.csParam1, sw->Site.csParam2,
			sw->Site.shParam, sw->SoilWat.snowdepth, sw->Site.Tsoil_constant,
			sw->Site.stDeltaX, sw->Site.stMaxDepth, sw->Site.stNRGR,
			sw->SoilWat.snowpack[Today], sw->Weather.now.temp_max,
			sw->Weather.now.temp_min, sw->SoilWat.H_gt, sw->Model.year,
			sw->Model.doy, sw->SoilWat.maxLyrTemperature,
			sw->SoilWat.minLyrTemperature, &sw->SoilWat.soiltempError, LogInfo);
	}

	/* Soil Temperature ends here */

	/* Finalize "flow" of today */
	#ifdef SWDEBUG
	if (debug) {
		if (sw->Site.deepdrain) {
			if (!EQ(sw->SoilWat.drain[sw->Site.deep_lyr], drainout)) {
				swprintf(
					"Percolation (%f) of last layer [%d] is not equal to deep drainage (%f).\n",
					sw->SoilWat.drain[sw->Site.deep_lyr],
					sw->Site.deep_lyr + 1,
					drainout
				);
			}
		}
	}
	#endif

	*standingWaterYesterday = *standingWaterToday;

} /* END OF WATERFLOW */

