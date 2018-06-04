/********************************************************/
/********************************************************/
/*  Source file: SW_Output_SOILWAT2.c
  Type: module
  Application: SOILWAT - soilwater dynamics simulator
  Purpose: define `get_XXX` functions for SOILWAT2-standalone
    see SW_Output_core.c and SW_Output.h

  History:
  2018 June 04 (drs) moved output formatter `get_XXX` functions from
    previous `SW_Output.c` to dedicated `SW_Output_SOILWAT2.c`
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
#include <ctype.h>
#include "generic.h"
#include "filefuncs.h"
#include "myMemory.h"
#include "Times.h"

#include "SW_Carbon.h"
#include "SW_Defines.h"
#include "SW_Files.h"
#include "SW_Model.h"
#include "SW_Site.h"
#include "SW_SoilWater.h"
#include "SW_Times.h"
#include "SW_Weather.h"
#include "SW_VegEstab.h"
#include "SW_VegProd.h"

#include "SW_Output.h"

#ifdef RSOILWAT
#include <R.h>
#include <Rdefines.h>
#include <Rinternals.h>
#include "../rSW_Output.h"
#endif

#ifdef STEPWAT
#include "../sxw.h"
#include "../ST_globals.h"
#endif

/* =================================================== */
/*                  Global Variables                   */
/* --------------------------------------------------- */
extern SW_SITE SW_Site;
extern SW_SOILWAT SW_Soilwat;
extern SW_MODEL SW_Model;
extern SW_WEATHER SW_Weather;
extern SW_VEGPROD SW_VegProd;
extern SW_VEGESTAB SW_VegEstab;
extern SW_CARBON SW_Carbon;
extern SW_OUTPUT SW_Output[SW_OUTNKEYS]; // defined in `SW_Output_core.c`

#ifdef RSOILWAT
extern RealD *p_rOUT[SW_OUTNKEYS][SW_OUTNPERIODS];
extern unsigned int yr_nrow, mo_nrow, wk_nrow, dy_nrow;
#endif

#ifdef STEPWAT
extern SXW_t SXW; // structure to store values in and pass back to STEPPE
extern SXW_avg SXW_AVG;
extern Bool isPartialSoilwatOutput;
extern Bool storeAllIterations;
extern char outstr_all_iters[OUTSTRLEN];
#endif

extern char _Sep; // defined in `SW_Output_core.c`: output delimiter
extern TimeInt tOffset; // defined in `SW_Output_core.c`: 1 or 0 means we're writing previous or current period
extern Bool bFlush_output; // defined in `SW_Output_core.c`: process partial period ?
extern char sw_outstr[OUTSTRLEN]; // defined in `SW_Output_core.c`: string with formatted output which is to be written to output files


/* =================================================== */
/* =================================================== */
/*             Function Definitions                    */
/*             (declared in SW_Output.h)               */
/* --------------------------------------------------- */


#ifndef RSOILWAT
void get_outstrleader(TimeInt pd)
{
	/* --------------------------------------------------- */
	/* this is called from each of the remaining get_ funcs
	 * to set up the first (date) columns of the output
	 * string.  It's the same for all and easier to put in
	 * one place.
	 * Periodic output for Month and/or Week are actually
	 * printing for the PREVIOUS month or week.
	 * Also, see note on test value in _write_today() for
	 * explanation of the +1.
	 */
	switch (pd)
	{
	case eSW_Day:
		sprintf(sw_outstr, "%d%c%d", SW_Model.simyear, _Sep, SW_Model.doy);
		#ifdef STEPWAT
			if(storeAllIterations)
				sprintf(outstr_all_iters, "%d%c%d", SW_Model.simyear, _Sep, SW_Model.doy);
		#endif
		break;
	case eSW_Week:
		sprintf(sw_outstr, "%d%c%d", SW_Model.simyear, _Sep,
				(SW_Model.week + 1) - tOffset);
		#ifdef STEPWAT
			if(storeAllIterations)
				sprintf(outstr_all_iters, "%d%c%d", SW_Model.simyear, _Sep,
						(SW_Model.week + 1) - tOffset);
		#endif
		break;
	case eSW_Month:
		sprintf(sw_outstr, "%d%c%d", SW_Model.simyear, _Sep,
				(SW_Model.month + 1) - tOffset);

		#ifdef STEPWAT
			if(storeAllIterations)
				sprintf(outstr_all_iters, "%d%c%d", SW_Model.simyear, _Sep,
						(SW_Model.month + 1) - tOffset);
		#endif
		break;
	case eSW_Year:
		sprintf(sw_outstr, "%d", SW_Model.simyear);
		#ifdef STEPWAT
			if(storeAllIterations)
				sprintf(outstr_all_iters, "%d", SW_Model.simyear);
		#endif
		break;
	}
}
#endif

/* --------------------------------------------------- */
/* each of these get_<envparm> -type funcs return a
 * formatted string of the appropriate type and are
 * pointed to by SW_Output[k].pfunc so they can be called
 * anonymously by looping over the Output[k] list
 * (see _output_today() for usage.)
 * they all use the global-level string sw_outstr[].
 */
/* 10-May-02 (cwb) Added conditionals for interfacing with STEPPE
 * 05-Mar-03 (cwb) Added code for max,min,avg. Previously, only avg was output.
 * 22 June-15 (akt)  Added code for adding surfaceTemp at output
 */

void get_co2effects(OutPeriod pd) {
	SW_VEGPROD *v = &SW_VegProd;

	#ifdef RSOILWAT
		int delta;
		RealD *p;
	#endif

	RealD biomass_total = SW_MISSING, biolive_total = SW_MISSING;
	RealD biomass_grass = SW_MISSING, biomass_shrub = SW_MISSING,
		biomass_tree = SW_MISSING, biomass_forb = SW_MISSING;
	RealD biolive_grass = SW_MISSING, biolive_shrub = SW_MISSING,
		biolive_tree = SW_MISSING, biolive_forb = SW_MISSING;

	// Grab the multipliers that were just used
	// No averaging or summing required
	RealD bio_mult_grass = v->veg[SW_GRASS].co2_multipliers[BIO_INDEX][SW_Model.simyear];
	RealD bio_mult_shrub = v->veg[SW_SHRUB].co2_multipliers[BIO_INDEX][SW_Model.simyear];
	RealD bio_mult_tree = v->veg[SW_TREES].co2_multipliers[BIO_INDEX][SW_Model.simyear];
	RealD bio_mult_forb = v->veg[SW_FORBS].co2_multipliers[BIO_INDEX][SW_Model.simyear];
	RealD wue_mult_grass = v->veg[SW_GRASS].co2_multipliers[WUE_INDEX][SW_Model.simyear];
	RealD wue_mult_shrub = v->veg[SW_SHRUB].co2_multipliers[WUE_INDEX][SW_Model.simyear];
	RealD wue_mult_tree = v->veg[SW_TREES].co2_multipliers[WUE_INDEX][SW_Model.simyear];
	RealD wue_mult_forb = v->veg[SW_FORBS].co2_multipliers[WUE_INDEX][SW_Model.simyear];


	#if !defined(STEPWAT) && !defined(RSOILWAT)
	char str[OUTSTRLEN];
	get_outstrleader(pd);

	#elif defined(STEPWAT)
		char str[OUTSTRLEN];
		char str_iters[OUTSTRLEN];
		TimeInt p = 0;
		if ((isPartialSoilwatOutput == FALSE && Globals.currIter == Globals.runModelIterations) || storeAllIterations)
			get_outstrleader(pd);
#endif

	switch(pd) {
		case eSW_Day:
			biomass_grass = v->dysum.veg[SW_GRASS].biomass;
			biomass_shrub = v->dysum.veg[SW_SHRUB].biomass;
			biomass_tree = v->dysum.veg[SW_TREES].biomass;
			biomass_forb = v->dysum.veg[SW_FORBS].biomass;
			biolive_grass = v->dysum.veg[SW_GRASS].biolive;
			biolive_shrub = v->dysum.veg[SW_SHRUB].biolive;
			biolive_tree = v->dysum.veg[SW_TREES].biolive;
			biolive_forb = v->dysum.veg[SW_FORBS].biolive;
			biomass_total = biomass_grass + biomass_shrub + biomass_tree + biomass_forb;
			biolive_total = biolive_grass + biolive_shrub + biolive_tree + biolive_forb;

			#ifdef RSOILWAT
				delta = SW_Output[eSW_CO2Effects].dy_row;
				p = p_rOUT[eSW_CO2Effects][eSW_Day]; // set shorthand copy of 'p_rOUT' pointer
				p[delta + dy_nrow * 0] = SW_Model.simyear;
				p[delta + dy_nrow * 1] = SW_Model.doy;
				p[delta + dy_nrow * 2] = biomass_grass;
				p[delta + dy_nrow * 3] = biomass_shrub;
				p[delta + dy_nrow * 4] = biomass_tree;
				p[delta + dy_nrow * 5] = biomass_forb;
				p[delta + dy_nrow * 6] = biomass_total;
				p[delta + dy_nrow * 7] = biolive_grass;
				p[delta + dy_nrow * 8] = biolive_shrub;
				p[delta + dy_nrow * 9] = biolive_tree;
				p[delta + dy_nrow * 10] = biolive_forb;
				p[delta + dy_nrow * 11] = biolive_total;
				p[delta + dy_nrow * 12] = bio_mult_grass;
				p[delta + dy_nrow * 13] = bio_mult_shrub;
				p[delta + dy_nrow * 14] = bio_mult_tree;
				p[delta + dy_nrow * 15] = bio_mult_forb;
				p[delta + dy_nrow * 16] = wue_mult_grass;
				p[delta + dy_nrow * 17] = wue_mult_shrub;
				p[delta + dy_nrow * 18] = wue_mult_tree;
				p[delta + dy_nrow * 19] = wue_mult_forb;
				SW_Output[eSW_CO2Effects].dy_row++;
			#endif
			break;

		case eSW_Week:
			biomass_grass = v->wkavg.veg[SW_GRASS].biomass;
			biomass_shrub = v->wkavg.veg[SW_SHRUB].biomass;
			biomass_tree = v->wkavg.veg[SW_TREES].biomass;
			biomass_forb = v->wkavg.veg[SW_FORBS].biomass;
			biolive_grass = v->wkavg.veg[SW_GRASS].biolive;
			biolive_shrub = v->wkavg.veg[SW_SHRUB].biolive;
			biolive_tree = v->wkavg.veg[SW_TREES].biolive;
			biolive_forb = v->wkavg.veg[SW_FORBS].biolive;
			biomass_total = biomass_grass + biomass_shrub + biomass_tree + biomass_forb;
			biolive_total = biolive_grass + biolive_shrub + biolive_tree + biolive_forb;

			#ifdef RSOILWAT
				delta = SW_Output[eSW_CO2Effects].wk_row;
				p = p_rOUT[eSW_CO2Effects][eSW_Week]; // set shorthand copy of 'p_rOUT' pointer
				p[delta + wk_nrow * 0] = SW_Model.simyear;
				p[delta + wk_nrow * 1] = (SW_Model.week + 1) - tOffset;
				p[delta + wk_nrow * 2] = biomass_grass;
				p[delta + wk_nrow * 3] = biomass_shrub;
				p[delta + wk_nrow * 4] = biomass_tree;
				p[delta + wk_nrow * 5] = biomass_forb;
				p[delta + wk_nrow * 6] = biomass_total;
				p[delta + wk_nrow * 7] = biolive_grass;
				p[delta + wk_nrow * 8] = biolive_shrub;
				p[delta + wk_nrow * 9] = biolive_tree;
				p[delta + wk_nrow * 10] = biomass_forb;
				p[delta + wk_nrow * 11] = biolive_total;
				p[delta + wk_nrow * 12] = bio_mult_grass;
				p[delta + wk_nrow * 13] = bio_mult_shrub;
				p[delta + wk_nrow * 14] = bio_mult_tree;
				p[delta + wk_nrow * 15] = bio_mult_forb;
				p[delta + wk_nrow * 16] = wue_mult_grass;
				p[delta + wk_nrow * 17] = wue_mult_shrub;
				p[delta + wk_nrow * 18] = wue_mult_tree;
				p[delta + wk_nrow * 19] = wue_mult_forb;
				SW_Output[eSW_CO2Effects].wk_row++;
			#endif
			break;

		case eSW_Month:
			biomass_grass = v->moavg.veg[SW_GRASS].biomass;
			biomass_shrub = v->moavg.veg[SW_SHRUB].biomass;
			biomass_tree = v->moavg.veg[SW_TREES].biomass;
			biomass_forb = v->moavg.veg[SW_FORBS].biomass;
			biolive_grass = v->moavg.veg[SW_GRASS].biolive;
			biolive_shrub = v->moavg.veg[SW_SHRUB].biolive;
			biolive_tree = v->moavg.veg[SW_TREES].biolive;
			biolive_forb = v->moavg.veg[SW_FORBS].biolive;
			biomass_total = biomass_grass + biomass_shrub + biomass_tree + biomass_forb;
			biolive_total = biolive_grass + biolive_shrub + biolive_tree + biolive_forb;

			#ifdef RSOILWAT
				delta = SW_Output[eSW_CO2Effects].mo_row;
				p = p_rOUT[eSW_CO2Effects][eSW_Month]; // set shorthand copy of 'p_rOUT' pointer
				p[delta + mo_nrow * 0] = SW_Model.simyear;
				p[delta + mo_nrow * 1] = (SW_Model.month) - tOffset + 1;
				p[delta + mo_nrow * 2] = biomass_grass;
				p[delta + mo_nrow * 3] = biomass_shrub;
				p[delta + mo_nrow * 4] = biomass_tree;
				p[delta + mo_nrow * 5] = biomass_forb;
				p[delta + mo_nrow * 6] = biomass_total;
				p[delta + mo_nrow * 8] = biolive_grass;
				p[delta + mo_nrow * 7] = biolive_shrub;
				p[delta + mo_nrow * 9] = biolive_tree;
				p[delta + mo_nrow * 10] = biolive_forb;
				p[delta + mo_nrow * 11] = biolive_total;
				p[delta + mo_nrow * 12] = bio_mult_grass;
				p[delta + mo_nrow * 13] = bio_mult_shrub;
				p[delta + mo_nrow * 14] = bio_mult_tree;
				p[delta + mo_nrow * 15] = bio_mult_forb;
				p[delta + mo_nrow * 16] = wue_mult_grass;
				p[delta + mo_nrow * 17] = wue_mult_shrub;
				p[delta + mo_nrow * 18] = wue_mult_tree;
				p[delta + mo_nrow * 19] = wue_mult_forb;
				SW_Output[eSW_CO2Effects].mo_row++;
			#endif
			break;

		case eSW_Year:
			biomass_grass = v->yravg.veg[SW_GRASS].biomass;
			biomass_shrub = v->yravg.veg[SW_SHRUB].biomass;
			biomass_tree = v->yravg.veg[SW_TREES].biomass;
			biomass_forb = v->yravg.veg[SW_FORBS].biomass;
			biolive_grass = v->yravg.veg[SW_GRASS].biolive;
			biolive_shrub = v->yravg.veg[SW_SHRUB].biolive;
			biolive_tree = v->yravg.veg[SW_TREES].biolive;
			biolive_forb = v->yravg.veg[SW_FORBS].biolive;
			biomass_total = biomass_grass + biomass_shrub + biomass_tree + biomass_forb;
			biolive_total = biolive_grass + biolive_shrub + biolive_tree + biolive_forb;

			#ifdef RSOILWAT
				delta = SW_Output[eSW_CO2Effects].yr_row;
				p = p_rOUT[eSW_CO2Effects][eSW_Year]; // set shorthand copy of 'p_rOUT' pointer
				p[delta + yr_nrow * 0] = SW_Model.simyear;
				p[delta + yr_nrow * 1] = biomass_grass;
				p[delta + yr_nrow * 2] = biomass_shrub;
				p[delta + yr_nrow * 3] = biomass_tree;
				p[delta + yr_nrow * 4] = biomass_forb;
				p[delta + yr_nrow * 5] = biomass_total;
				p[delta + yr_nrow * 6] = biolive_grass;
				p[delta + yr_nrow * 7] = biolive_shrub;
				p[delta + yr_nrow * 8] = biolive_tree;
				p[delta + yr_nrow * 9] = biolive_forb;
				p[delta + yr_nrow * 10] = biolive_total;
				p[delta + yr_nrow * 11] = bio_mult_grass;
				p[delta + yr_nrow * 12] = bio_mult_shrub;
				p[delta + yr_nrow * 13] = bio_mult_tree;
				p[delta + yr_nrow * 14] = bio_mult_forb;
				p[delta + yr_nrow * 15] = wue_mult_grass;
				p[delta + yr_nrow * 16] = wue_mult_shrub;
				p[delta + yr_nrow * 17] = wue_mult_tree;
				p[delta + yr_nrow * 18] = wue_mult_forb;
				SW_Output[eSW_CO2Effects].yr_row++;
			#endif
			break;
	}

	#if !defined(STEPWAT) && !defined(RSOILWAT)
		sprintf(str, "%c%f%c%f%c%f%c%f%c%f%c%f%c%f%c%f%c%f%c%f%c%f%c%f%c%f%c%f%c%f%c%f%c%f%c%f",
		_Sep, biomass_grass,
		_Sep, biomass_shrub,
		_Sep, biomass_tree,
		_Sep, biomass_forb,
		_Sep, biomass_total,
		_Sep, biolive_grass,
		_Sep, biolive_shrub,
		_Sep, biolive_tree,
		_Sep, biolive_forb,
		_Sep, biolive_total,
		_Sep, bio_mult_grass,
		_Sep, bio_mult_shrub,
		_Sep, bio_mult_tree,
		_Sep, bio_mult_forb,
		_Sep, wue_mult_grass,
		_Sep, wue_mult_shrub,
		_Sep, wue_mult_tree,
		_Sep, wue_mult_forb);
		strcat(sw_outstr, str);

	#elif defined(STEPWAT)
		switch (pd)
		{
			case eSW_Day:
				p = SW_Model.doy-1;
				break;
			case eSW_Week:
				p = SW_Model.week-tOffset;
				break;
			case eSW_Month:
				p = SW_Model.month-tOffset;
				break;
			case eSW_Year:
				p = Globals.currYear-1;
				break;
		}

		if (isPartialSoilwatOutput == FALSE)
		{
			int
				ind0 = Iypc(Globals.currYear - 1, p, 0, pd), // index for mean
				ind1 = Iypc(Globals.currYear - 1, p, 1, pd); // index for sd

			float old_biomass_grass = SXW_AVG.biomass_grass_avg[ind0];
			float old_biomass_shrub = SXW_AVG.biomass_shrub_avg[ind0];
			float old_biomass_tree = SXW_AVG.biomass_tree_avg[ind0];
			float old_biomass_forb = SXW_AVG.biomass_forb_avg[ind0];
			float old_biomass_total = SXW_AVG.biomass_total_avg[ind0];

			float old_biolive_grass = SXW_AVG.biolive_grass_avg[ind0];
			float old_biolive_shrub = SXW_AVG.biolive_shrub_avg[ind0];
			float old_biolive_tree = SXW_AVG.biolive_tree_avg[ind0];
			float old_biolive_forb = SXW_AVG.biolive_forb_avg[ind0];
			float old_biolive_total = SXW_AVG.biolive_total_avg[ind0];

			float old_bio_mult_grass = SXW_AVG.bio_mult_grass_avg[ind0];
			float old_bio_mult_shrub = SXW_AVG.bio_mult_shrub_avg[ind0];
			float old_bio_mult_tree = SXW_AVG.bio_mult_tree_avg[ind0];
			float old_bio_mult_forb = SXW_AVG.bio_mult_forb_avg[ind0];

			float old_wue_mult_grass = SXW_AVG.wue_mult_grass_avg[ind0];
			float old_wue_mult_shrub = SXW_AVG.wue_mult_shrub_avg[ind0];
			float old_wue_mult_tree = SXW_AVG.wue_mult_tree_avg[ind0];
			float old_wue_mult_forb = SXW_AVG.wue_mult_forb_avg[ind0];


			SXW_AVG.biomass_grass_avg[ind0] = get_running_avg(SXW_AVG.biomass_grass_avg[ind0], biomass_grass);
			SXW_AVG.biomass_shrub_avg[ind0] = get_running_avg(SXW_AVG.biomass_shrub_avg[ind0], biomass_shrub);
			SXW_AVG.biomass_tree_avg[ind0] = get_running_avg(SXW_AVG.biomass_tree_avg[ind0], biomass_tree);
			SXW_AVG.biomass_forb_avg[ind0] = get_running_avg(SXW_AVG.biomass_forb_avg[ind0], biomass_forb);
			SXW_AVG.biomass_total_avg[ind0] = get_running_avg(SXW_AVG.biomass_total_avg[ind0], biomass_total);

			SXW_AVG.biolive_grass_avg[ind0] = get_running_avg(SXW_AVG.biolive_grass_avg[ind0], biolive_grass);
			SXW_AVG.biolive_shrub_avg[ind0] = get_running_avg(SXW_AVG.biolive_shrub_avg[ind0], biolive_shrub);
			SXW_AVG.biolive_tree_avg[ind0] = get_running_avg(SXW_AVG.biolive_tree_avg[ind0], biolive_tree);
			SXW_AVG.biolive_forb_avg[ind0] = get_running_avg(SXW_AVG.biolive_forb_avg[ind0], biolive_forb);
			SXW_AVG.biolive_total_avg[ind0] = get_running_avg(SXW_AVG.biolive_total_avg[ind0], biolive_total);

			SXW_AVG.bio_mult_grass_avg[ind0] = get_running_avg(SXW_AVG.bio_mult_grass_avg[ind0], bio_mult_grass);
			SXW_AVG.bio_mult_shrub_avg[ind0] = get_running_avg(SXW_AVG.bio_mult_shrub_avg[ind0], bio_mult_shrub);
			SXW_AVG.bio_mult_tree_avg[ind0] = get_running_avg(SXW_AVG.bio_mult_tree_avg[ind0], bio_mult_tree);
			SXW_AVG.bio_mult_forb_avg[ind0] = get_running_avg(SXW_AVG.bio_mult_forb_avg[ind0], bio_mult_forb);

			SXW_AVG.wue_mult_grass_avg[ind0] = get_running_avg(SXW_AVG.wue_mult_grass_avg[ind0], wue_mult_grass);
			SXW_AVG.wue_mult_shrub_avg[ind0] = get_running_avg(SXW_AVG.wue_mult_shrub_avg[ind0], wue_mult_shrub);
			SXW_AVG.wue_mult_tree_avg[ind0] = get_running_avg(SXW_AVG.wue_mult_tree_avg[ind0], wue_mult_tree);
			SXW_AVG.wue_mult_forb_avg[ind0] = get_running_avg(SXW_AVG.wue_mult_forb_avg[ind0], wue_mult_forb);

			// ---------------------------

			SXW_AVG.biomass_grass_avg[ind1] += get_running_sqr(old_biomass_grass, biomass_grass, SXW_AVG.biomass_grass_avg[ind0]);
			SXW_AVG.biomass_shrub_avg[ind1] += get_running_sqr(old_biomass_shrub, biomass_shrub, SXW_AVG.biomass_shrub_avg[ind0]);
			SXW_AVG.biomass_tree_avg[ind1] += get_running_sqr(old_biomass_tree, biomass_tree, SXW_AVG.biomass_tree_avg[ind0]);
			SXW_AVG.biomass_forb_avg[ind1] += get_running_sqr(old_biomass_forb, biomass_forb, SXW_AVG.biomass_forb_avg[ind0]);
			SXW_AVG.biomass_total_avg[ind1] += get_running_sqr(old_biomass_total, biomass_total, SXW_AVG.biomass_total_avg[ind0]);

			SXW_AVG.biolive_grass_avg[ind1] += get_running_sqr(old_biolive_grass, biolive_grass, SXW_AVG.biolive_grass_avg[ind0]);
			SXW_AVG.biolive_shrub_avg[ind1] += get_running_sqr(old_biolive_shrub, biolive_shrub, SXW_AVG.biolive_shrub_avg[ind0]);
			SXW_AVG.biolive_tree_avg[ind1] += get_running_sqr(old_biolive_tree, biolive_tree, SXW_AVG.biolive_tree_avg[ind0]);
			SXW_AVG.biolive_forb_avg[ind1] += get_running_sqr(old_biolive_forb, biolive_forb, SXW_AVG.biolive_forb_avg[ind0]);
			SXW_AVG.biolive_total_avg[ind1] += get_running_sqr(old_biolive_total, biolive_total, SXW_AVG.biolive_total_avg[ind0]);

			SXW_AVG.bio_mult_grass_avg[ind1] += get_running_sqr(old_bio_mult_grass, bio_mult_grass, SXW_AVG.bio_mult_grass_avg[ind0]);
			SXW_AVG.bio_mult_shrub_avg[ind1] += get_running_sqr(old_bio_mult_shrub, bio_mult_shrub, SXW_AVG.bio_mult_shrub_avg[ind0]);
			SXW_AVG.bio_mult_tree_avg[ind1] += get_running_sqr(old_bio_mult_tree, bio_mult_tree, SXW_AVG.bio_mult_tree_avg[ind0]);
			SXW_AVG.bio_mult_forb_avg[ind1] += get_running_sqr(old_bio_mult_forb, bio_mult_forb, SXW_AVG.bio_mult_forb_avg[ind0]);

			SXW_AVG.wue_mult_grass_avg[ind1] += get_running_sqr(old_wue_mult_grass, wue_mult_grass, SXW_AVG.wue_mult_grass_avg[ind0]);
			SXW_AVG.wue_mult_shrub_avg[ind1] += get_running_sqr(old_wue_mult_shrub, wue_mult_shrub, SXW_AVG.wue_mult_shrub_avg[ind0]);
			SXW_AVG.wue_mult_tree_avg[ind1] += get_running_sqr(old_wue_mult_tree, wue_mult_tree, SXW_AVG.wue_mult_tree_avg[ind0]);
			SXW_AVG.wue_mult_forb_avg[ind1] += get_running_sqr(old_wue_mult_forb, wue_mult_forb, SXW_AVG.wue_mult_forb_avg[ind0]);


			if(Globals.currIter == Globals.runModelIterations){
				float std_biomass_grass = sqrt(SXW_AVG.biomass_grass_avg[ind1] / Globals.currIter);
				float std_biomass_shrub = sqrt(SXW_AVG.biomass_shrub_avg[ind1] / Globals.currIter);
				float std_biomass_tree = sqrt(SXW_AVG.biomass_tree_avg[ind1] / Globals.currIter);
				float std_biomass_forb = sqrt(SXW_AVG.biomass_forb_avg[ind1] / Globals.currIter);
				float std_biomass_total = sqrt(SXW_AVG.biomass_total_avg[ind1] / Globals.currIter);

				float std_biolive_grass = sqrt(SXW_AVG.biolive_grass_avg[ind1] / Globals.currIter);
				float std_biolive_shrub = sqrt(SXW_AVG.biolive_shrub_avg[ind1] / Globals.currIter);
				float std_biolive_tree = sqrt(SXW_AVG.biolive_tree_avg[ind1] / Globals.currIter);
				float std_biolive_forb = sqrt(SXW_AVG.biolive_forb_avg[ind1] / Globals.currIter);
				float std_biolive_total = sqrt(SXW_AVG.biolive_total_avg[ind1] / Globals.currIter);

				float std_bio_mult_grass = sqrt(SXW_AVG.bio_mult_grass_avg[ind1] / Globals.currIter);
				float std_bio_mult_shrub = sqrt(SXW_AVG.bio_mult_shrub_avg[ind1] / Globals.currIter);
				float std_bio_mult_tree = sqrt(SXW_AVG.bio_mult_tree_avg[ind1] / Globals.currIter);
				float std_bio_mult_forb = sqrt(SXW_AVG.bio_mult_forb_avg[ind1] / Globals.currIter);

				float std_wue_mult_grass = sqrt(SXW_AVG.wue_mult_grass_avg[ind1] / Globals.currIter);
				float std_wue_mult_shrub = sqrt(SXW_AVG.wue_mult_shrub_avg[ind1] / Globals.currIter);
				float std_wue_mult_tree = sqrt(SXW_AVG.wue_mult_tree_avg[ind1] / Globals.currIter);
				float std_wue_mult_forb = sqrt(SXW_AVG.wue_mult_forb_avg[ind1] / Globals.currIter);

				sprintf(str, "%c%f%c%f%c%f%c%f%c%f%c%f%c%f%c%f%c%f%c%f%c%f%c%f%c%f%c%f%c%f%c%f%c%f%c%f%c%f%c%f%c%f%c%f%c%f%c%f%c%f%c%f%c%f%c%f%c%f%c%f%c%f%c%f%c%f%c%f%c%f%c%f",
				_Sep, SXW_AVG.biomass_grass_avg[ind0], _Sep, std_biomass_grass,
				_Sep, SXW_AVG.biomass_shrub_avg[ind0], _Sep, std_biomass_shrub,
				_Sep, SXW_AVG.biomass_tree_avg[ind0], _Sep, std_biomass_tree,
				_Sep, SXW_AVG.biomass_forb_avg[ind0], _Sep, std_biomass_forb,
				_Sep, SXW_AVG.biomass_total_avg[ind0], _Sep, std_biomass_total,
				_Sep, SXW_AVG.biolive_grass_avg[ind0], _Sep, std_biolive_grass,
				_Sep, SXW_AVG.biolive_shrub_avg[ind0], _Sep, std_biolive_shrub,
				_Sep, SXW_AVG.biolive_tree_avg[ind0], _Sep, std_biolive_tree,
				_Sep, SXW_AVG.biolive_forb_avg[ind0], _Sep, std_biolive_forb,
				_Sep, SXW_AVG.biolive_total_avg[ind0], _Sep, std_biolive_total,
				_Sep, SXW_AVG.bio_mult_grass_avg[ind0], _Sep, std_bio_mult_grass,
				_Sep, SXW_AVG.bio_mult_shrub_avg[ind0], _Sep, std_bio_mult_shrub,
				_Sep, SXW_AVG.bio_mult_tree_avg[ind0], _Sep, std_bio_mult_tree,
				_Sep, SXW_AVG.bio_mult_forb_avg[ind0], _Sep, std_bio_mult_forb,
				_Sep, SXW_AVG.wue_mult_grass_avg[ind0], _Sep, std_wue_mult_grass,
				_Sep, SXW_AVG.wue_mult_shrub_avg[ind0], _Sep, std_wue_mult_shrub,
				_Sep, SXW_AVG.wue_mult_tree_avg[ind0], _Sep, std_wue_mult_tree,
				_Sep, SXW_AVG.wue_mult_forb_avg[ind0], _Sep, std_wue_mult_forb);
				strcat(sw_outstr, str);
			}
		}
		if(storeAllIterations){
			sprintf(str_iters, "%c%f%c%f%c%f%c%f%c%f%c%f%c%f%c%f%c%f%c%f%c%f%c%f%c%f%c%f%c%f%c%f%c%f%c%f",
			_Sep, biomass_grass,
			_Sep, biomass_shrub,
			_Sep, biomass_tree,
			_Sep, biomass_forb,
			_Sep, biomass_total,
			_Sep, biolive_grass,
			_Sep, biolive_shrub,
			_Sep, biolive_tree,
			_Sep, biolive_forb,
			_Sep, biolive_total,
			_Sep, bio_mult_grass,
			_Sep, bio_mult_shrub,
			_Sep, bio_mult_tree,
			_Sep, bio_mult_forb,
			_Sep, wue_mult_grass,
			_Sep, wue_mult_shrub,
			_Sep, wue_mult_tree,
			_Sep, wue_mult_forb);
			strcat(outstr_all_iters, str_iters);
		}
	#endif
}

void get_estab(OutPeriod pd)
{
	/* --------------------------------------------------- */
	/* the establishment check produces, for each species in
	 * the given set, a day of year >=0 that the species
	 * established itself in the current year.  The output
	 * will be a single row of numbers for each year.  Each
	 * column represents a species in the order it was entered
	 * in the estabs.in file.  The value will be the day that
	 * the species established, or 0 if it didn't establish
	 * this year.
	 */
	SW_VEGESTAB *v = &SW_VegEstab;
	IntU i;

	#if !defined(STEPWAT) && !defined(RSOILWAT)
		get_outstrleader(pd);
		char str[OUTSTRLEN];

	#elif defined(STEPWAT)
		char str[OUTSTRLEN];
		char str_iters[OUTSTRLEN];
		TimeInt p = 0;
		if ((isPartialSoilwatOutput == FALSE && Globals.currIter == Globals.runModelIterations) || storeAllIterations)
			get_outstrleader(pd);

#endif

#ifdef RSOILWAT
	switch(pd)
	{
		case eSW_Day:
		p_rOUT[eSW_Estab][eSW_Day][SW_Output[eSW_Estab].dy_row + dy_nrow * 0] = SW_Model.simyear;
		p_rOUT[eSW_Estab][eSW_Day][SW_Output[eSW_Estab].dy_row + dy_nrow * 1] = SW_Model.doy;
		break;
		case eSW_Week:
		p_rOUT[eSW_Estab][eSW_Week][SW_Output[eSW_Estab].wk_row + wk_nrow * 0] = SW_Model.simyear;
		p_rOUT[eSW_Estab][eSW_Week][SW_Output[eSW_Estab].wk_row + wk_nrow * 1] = (SW_Model.week + 1) - tOffset;
		break;
		case eSW_Month:
		p_rOUT[eSW_Estab][eSW_Month][SW_Output[eSW_Estab].mo_row + mo_nrow * 0] = SW_Model.simyear;
		p_rOUT[eSW_Estab][eSW_Month][SW_Output[eSW_Estab].mo_row + mo_nrow * 1] = (SW_Model.month + 1) - tOffset;
		break;
		case eSW_Year:
		p_rOUT[eSW_Estab][eSW_Year][SW_Output[eSW_Estab].yr_row + yr_nrow * 0] = SW_Model.simyear;
		break;
	}
#endif
	for (i = 0; i < v->count; i++)
	{
		#if !defined(STEPWAT) && !defined(RSOILWAT)
			sprintf(str, "%c%d", _Sep, v->parms[i]->estab_doy);
			strcat(sw_outstr, str);

		#elif defined(STEPWAT)
			switch (pd)
			{
				case eSW_Day:
					p = SW_Model.doy-1;
					break;
				case eSW_Week:
					p = SW_Model.week-tOffset;
					break;
				case eSW_Month:
					p = SW_Model.month-tOffset;
					break;
				case eSW_Year:
					p = 0; // Iypc requires 0 for yearly timeperiod
					break;
			}

			if (isPartialSoilwatOutput == FALSE)
			{
				int
					ind0 = Iypc(Globals.currYear - 1, p, 0, pd), // index for mean
					ind1 = Iypc(Globals.currYear - 1, p, 1, pd); // index for sd
				float old_val = SXW_AVG.estab_avg[ind0];

				SXW_AVG.estab_avg[ind0] = get_running_avg(SXW_AVG.estab_avg[ind0], v->parms[i]->estab_doy);

				SXW_AVG.estab_avg[ind1] += get_running_sqr(old_val, v->parms[i]->estab_doy, SXW_AVG.estab_avg[ind0]);
				if(Globals.currIter == Globals.runModelIterations){
					float std_estab = sqrt(SXW_AVG.estab_avg[ind1] / Globals.currIter);

					sprintf(str, "%c%7.6f%c%7.6f", _Sep, SXW_AVG.estab_avg[ind0], _Sep, std_estab);
					strcat(sw_outstr, str);
				}
			}
			if(storeAllIterations){
				sprintf(str_iters, "%c%d", _Sep, v->parms[i]->estab_doy);
				strcat(outstr_all_iters, str_iters);
			}
		#else
		switch(pd)
		{
			case eSW_Day:
			p_rOUT[eSW_Estab][eSW_Day][SW_Output[eSW_Estab].dy_row + dy_nrow * (i + 2)] = v->parms[i]->estab_doy;
			break;
			case eSW_Week:
			p_rOUT[eSW_Estab][eSW_Week][SW_Output[eSW_Estab].wk_row + wk_nrow * (i + 2)] = v->parms[i]->estab_doy;
			break;
			case eSW_Month:
			p_rOUT[eSW_Estab][eSW_Month][SW_Output[eSW_Estab].mo_row + mo_nrow * (i + 2)] = v->parms[i]->estab_doy;
			break;
			case eSW_Year:
			p_rOUT[eSW_Estab][eSW_Year][SW_Output[eSW_Estab].yr_row + yr_nrow * (i + 1)] = v->parms[i]->estab_doy;
			break;
		}
		#endif
	}
#ifdef RSOILWAT
	switch(pd)
	{
		case eSW_Day:
		SW_Output[eSW_Estab].dy_row++;
		break;
		case eSW_Week:
		SW_Output[eSW_Estab].wk_row++;
		break;
		case eSW_Month:
		SW_Output[eSW_Estab].mo_row++;
		break;
		case eSW_Year:
		SW_Output[eSW_Estab].yr_row++;
		break;
	}
#endif

}

void get_temp(OutPeriod pd)
{

  #ifdef SWDEBUG
  int debug = 0;
  #endif

	#ifdef RSOILWAT
		int delta;
		RealD *p;
	#endif

	SW_WEATHER *v = &SW_Weather;

  #ifdef SWDEBUG
  if (debug) swprintf("'get_temp': start for %s ... ", pd);
  #endif

#ifndef RSOILWAT
	RealD v_avg = SW_MISSING;
	RealD v_min = SW_MISSING, v_max = SW_MISSING;
	RealD surfaceTempVal = SW_MISSING;
#endif

#if !defined(STEPWAT) && !defined(RSOILWAT)
	char str[OUTSTRLEN];
	get_outstrleader(pd);

#elif defined(STEPWAT)
	char str[OUTSTRLEN];
	char str_iters[OUTSTRLEN];
	TimeInt p = 0;
	if ((isPartialSoilwatOutput == FALSE && Globals.currIter == Globals.runModelIterations) || storeAllIterations)
	{
		get_outstrleader(pd);
	}
#endif

	switch (pd)
	{
	case eSW_Day:
		#ifdef SWDEBUG
		if (debug) swprintf("%ddoy ... ", SW_Model.doy);
		#endif
#ifndef RSOILWAT
		v_max = v->dysum.temp_max;
		v_min = v->dysum.temp_min;
		v_avg = v->dysum.temp_avg;
		surfaceTempVal = v->dysum.surfaceTemp;
#else
		delta = SW_Output[eSW_Temp].dy_row;
		p = p_rOUT[eSW_Temp][eSW_Day]; // set shorthand copy of 'p_rOUT' pointer
		p[delta + dy_nrow * 0] = SW_Model.simyear;
		p[delta + dy_nrow * 1] = SW_Model.doy;
		p[delta + dy_nrow * 2] = v->dysum.temp_max;
		p[delta + dy_nrow * 3] = v->dysum.temp_min;
		p[delta + dy_nrow * 4] = v->dysum.temp_avg;
		p[delta + dy_nrow * 5] = v->dysum.surfaceTemp;
		SW_Output[eSW_Temp].dy_row++;
#endif
		break;
	case eSW_Week:
		#ifdef SWDEBUG
		if (debug) swprintf("%dwk ... ", (SW_Model.week + 1) - tOffset);
		#endif
#ifndef RSOILWAT
		v_max = v->wkavg.temp_max;
		v_min = v->wkavg.temp_min;
		v_avg = v->wkavg.temp_avg;
		surfaceTempVal = v->wkavg.surfaceTemp;
#else
		delta = SW_Output[eSW_Temp].wk_row;
		p = p_rOUT[eSW_Temp][eSW_Week]; // set shorthand copy of 'p_rOUT' pointer
		p[delta + wk_nrow * 0] = SW_Model.simyear;
		p[delta + wk_nrow * 1] = (SW_Model.week + 1) - tOffset;
		p[delta + wk_nrow * 2] = v->wkavg.temp_max;
		p[delta + wk_nrow * 3] = v->wkavg.temp_min;
		p[delta + wk_nrow * 4] = v->wkavg.temp_avg;
		p[delta + wk_nrow * 5] = v->wkavg.surfaceTemp;
		SW_Output[eSW_Temp].wk_row++;
#endif
		break;
	case eSW_Month:
		#ifdef SWDEBUG
		if (debug) swprintf("%dmon ... ", (SW_Model.month + 1) - tOffset);
		#endif
#ifndef RSOILWAT
		v_max = v->moavg.temp_max;
		v_min = v->moavg.temp_min;
		v_avg = v->moavg.temp_avg;
		surfaceTempVal = v->moavg.surfaceTemp;
#else
		delta = SW_Output[eSW_Temp].mo_row;
		p = p_rOUT[eSW_Temp][eSW_Month]; // set shorthand copy of 'p_rOUT' pointer
		p[delta + mo_nrow * 0] = SW_Model.simyear;
		p[delta + mo_nrow * 1] = (SW_Model.month + 1) - tOffset;
		p[delta + mo_nrow * 2] = v->moavg.temp_max;
		p[delta + mo_nrow * 3] = v->moavg.temp_min;
		p[delta + mo_nrow * 4] = v->moavg.temp_avg;
		p[delta + mo_nrow * 5] = v->moavg.surfaceTemp;
		SW_Output[eSW_Temp].mo_row++;
#endif
		break;
	case eSW_Year:
		#ifdef SWDEBUG
		if (debug) swprintf("%dyr ... ", SW_Model.simyear);
		#endif
#ifndef RSOILWAT
		v_max = v->yravg.temp_max;
		v_min = v->yravg.temp_min;
		v_avg = v->yravg.temp_avg;
		surfaceTempVal = v->yravg.surfaceTemp;
#else
		delta = SW_Output[eSW_Temp].yr_row;
		p = p_rOUT[eSW_Temp][eSW_Year]; // set shorthand copy of 'p_rOUT' pointer
		p[delta + yr_nrow * 0] = SW_Model.simyear;
		p[delta + yr_nrow * 1] = v->yravg.temp_max;
		p[delta + yr_nrow * 2] = v->yravg.temp_min;
		p[delta + yr_nrow * 3] = v->yravg.temp_avg;
		p[delta + yr_nrow * 4] = v->yravg.surfaceTemp;
		SW_Output[eSW_Temp].yr_row++;
#endif
		break;
	}

#if !defined(STEPWAT) && !defined(RSOILWAT)
	sprintf(str, "%c%7.6f%c%7.6f%c%7.6f%c%7.6f", _Sep, v_max, _Sep, v_min, _Sep,
		v_avg, _Sep, surfaceTempVal);
	strcat(sw_outstr, str);
#elif defined(STEPWAT)
	switch (pd)
	{
		case eSW_Day:
			p = SW_Model.doy-1;
			break;
		case eSW_Week:
			p = SW_Model.week-tOffset;
			break;
		case eSW_Month:
			p = SW_Model.month-tOffset;
			break;
		case eSW_Year:
			p = Globals.currYear-1;
			break;
	}

	if (isPartialSoilwatOutput == FALSE)
	{
		int
			ind0 = Iypc(Globals.currYear - 1, p, 0, pd), // index for mean
			ind1 = Iypc(Globals.currYear - 1, p, 1, pd); // index for sd
		float old_val_temp_max = SXW_AVG.max_temp_avg[ind0];
		float old_val_temp_min = SXW_AVG.min_temp_avg[ind0];
		float old_val_temp_avg = SXW_AVG.avg_temp_avg[ind0];
		int old_val_surface = SXW_AVG.surfaceTemp_avg[ind0];

		SXW_AVG.max_temp_avg[ind0] = get_running_avg(SXW_AVG.max_temp_avg[ind0], v_max);
		SXW_AVG.min_temp_avg[ind0] = get_running_avg(SXW_AVG.min_temp_avg[ind0], v_min);
		SXW_AVG.avg_temp_avg[ind0] = get_running_avg(SXW_AVG.avg_temp_avg[ind0], v_avg);
		SXW_AVG.surfaceTemp_avg[ind0] = get_running_avg(SXW_AVG.surfaceTemp_avg[ind0], surfaceTempVal);

		SXW_AVG.max_temp_avg[ind1] += get_running_sqr(old_val_temp_max, v_max, SXW_AVG.max_temp_avg[ind0]);
		SXW_AVG.min_temp_avg[ind1] += get_running_sqr(old_val_temp_min, v_min, SXW_AVG.min_temp_avg[ind0]);
		SXW_AVG.avg_temp_avg[ind1] += get_running_sqr(old_val_temp_avg, v_avg, SXW_AVG.avg_temp_avg[ind0]);
		SXW_AVG.surfaceTemp_avg[ind1] += get_running_sqr(old_val_surface, surfaceTempVal, SXW_AVG.surfaceTemp_avg[ind0]);


   if (Globals.currIter == Globals.runModelIterations){
		 float std_temp_max = sqrt(SXW_AVG.max_temp_avg[ind1] / Globals.currIter);
		 float std_temp_min = sqrt(SXW_AVG.min_temp_avg[ind1] / Globals.currIter);
		 float std_temp_avg = sqrt(SXW_AVG.avg_temp_avg[ind1] / Globals.currIter);
		 float std_surface = sqrt(SXW_AVG.surfaceTemp_avg[ind1] / Globals.currIter);

	   sprintf(str, "%c%7.6f%c%7.6f%c%7.6f%c%7.6f%c%7.6f%c%7.6f%c%7.6f%c%7.6f", _Sep, SXW_AVG.max_temp_avg[ind0], _Sep, std_temp_max,
		 	_Sep, SXW_AVG.min_temp_avg[ind0], _Sep, std_temp_min,
		  _Sep, SXW_AVG.avg_temp_avg[ind0], _Sep, std_temp_avg,
			_Sep, SXW_AVG.surfaceTemp_avg[ind0], _Sep, std_surface);
	   strcat(sw_outstr, str);
    }
	}

	if(storeAllIterations){
		sprintf(str_iters, "%c%7.6f%c%7.6f%c%7.6f%c%7.6f", _Sep, v_max, _Sep, v_min, _Sep,
			v_avg, _Sep, surfaceTempVal);
		strcat(outstr_all_iters, str_iters);
	}

	SXW.temp = v_avg;
	SXW.surfaceTemp = surfaceTempVal;
#endif
	#ifdef SWDEBUG
		if (debug) swprintf("completed\n");
	#endif
}

void get_precip(OutPeriod pd)
{
	/* --------------------------------------------------- */
	/* 	20091015 (drs) ppt is divided into rain and snow and all three values are output into precip */
	SW_WEATHER *v = &SW_Weather;

#ifndef RSOILWAT
	RealD val_ppt = SW_MISSING, val_rain = SW_MISSING, val_snow = SW_MISSING,
			val_snowmelt = SW_MISSING, val_snowloss = SW_MISSING;
#endif
	#ifdef RSOILWAT
		int delta;
		RealD *p;
	#endif

#if !defined(STEPWAT) && !defined(RSOILWAT)
	char str[OUTSTRLEN];
	get_outstrleader(pd);

#elif defined(STEPWAT)
	TimeInt p = 0;
	char str[OUTSTRLEN];
	char str_iters[OUTSTRLEN];
	if ((isPartialSoilwatOutput == FALSE && Globals.currIter == Globals.runModelIterations) || storeAllIterations)
	{
		get_outstrleader(pd);
	}
#endif

	switch(pd)
	{
	case eSW_Day:
#ifndef RSOILWAT
		val_ppt = v->dysum.ppt;
		val_rain = v->dysum.rain;
		val_snow = v->dysum.snow;
		val_snowmelt = v->dysum.snowmelt;
		val_snowloss = v->dysum.snowloss;
#else
		delta = SW_Output[eSW_Precip].dy_row;
		p = p_rOUT[eSW_Precip][eSW_Day]; // set shorthand copy of 'p_rOUT' pointer
		p[delta + dy_nrow * 0] = SW_Model.simyear;
		p[delta + dy_nrow * 1] = SW_Model.doy;
		p[delta + dy_nrow * 2] = v->dysum.ppt;
		p[delta + dy_nrow * 3] = v->dysum.rain;
		p[delta + dy_nrow * 4] = v->dysum.snow;
		p[delta + dy_nrow * 5] = v->dysum.snowmelt;
		p[delta + dy_nrow * 6] = v->dysum.snowloss;
		SW_Output[eSW_Precip].dy_row++;
#endif
		break;
	case eSW_Week:
#ifndef RSOILWAT
		val_ppt = v->wkavg.ppt;
		val_rain = v->wkavg.rain;
		val_snow = v->wkavg.snow;
		val_snowmelt = v->wkavg.snowmelt;
		val_snowloss = v->wkavg.snowloss;
#else
		delta = SW_Output[eSW_Precip].wk_row;
		p = p_rOUT[eSW_Precip][eSW_Week]; // set shorthand copy of 'p_rOUT' pointer
		p[delta + wk_nrow * 0] = SW_Model.simyear;
		p[delta + wk_nrow * 1] = (SW_Model.week + 1) - tOffset;
		p[delta + wk_nrow * 2] = v->wkavg.ppt;
		p[delta + wk_nrow * 3] = v->wkavg.rain;
		p[delta + wk_nrow * 4] = v->wkavg.snow;
		p[delta + wk_nrow * 5] = v->wkavg.snowmelt;
		p[delta + wk_nrow * 6] = v->wkavg.snowloss;
		SW_Output[eSW_Precip].wk_row++;
#endif
		break;
	case eSW_Month:
#ifndef RSOILWAT
		val_ppt = v->moavg.ppt;
		val_rain = v->moavg.rain;
		val_snow = v->moavg.snow;
		val_snowmelt = v->moavg.snowmelt;
		val_snowloss = v->moavg.snowloss;
#else
		delta = SW_Output[eSW_Precip].mo_row;
		p = p_rOUT[eSW_Precip][eSW_Month]; // set shorthand copy of 'p_rOUT' pointer
		p[delta + mo_nrow * 0] = SW_Model.simyear;
		p[delta + mo_nrow * 1] = (SW_Model.month + 1) - tOffset;
		p[delta + mo_nrow * 2] = v->moavg.ppt;
		p[delta + mo_nrow * 3] = v->moavg.rain;
		p[delta + mo_nrow * 4] = v->moavg.snow;
		p[delta + mo_nrow * 5] = v->moavg.snowmelt;
		p[delta + mo_nrow * 6] = v->moavg.snowloss;
		SW_Output[eSW_Precip].mo_row++;
#endif
		break;
	case eSW_Year:
#ifndef RSOILWAT
		val_ppt = v->yravg.ppt;
		val_rain = v->yravg.rain;
		val_snow = v->yravg.snow;
		val_snowmelt = v->yravg.snowmelt;
		val_snowloss = v->yravg.snowloss;
		break;
#else
		delta = SW_Output[eSW_Precip].yr_row;
		p = p_rOUT[eSW_Precip][eSW_Year]; // set shorthand copy of 'p_rOUT' pointer
		p[delta + yr_nrow * 0] = SW_Model.simyear;
		p[delta + yr_nrow * 1] = v->yravg.ppt;
		p[delta + yr_nrow * 2] = v->yravg.rain;
		p[delta + yr_nrow * 3] = v->yravg.snow;
		p[delta + yr_nrow * 4] = v->yravg.snowmelt;
		p[delta + yr_nrow * 5] = v->yravg.snowloss;
		SW_Output[eSW_Precip].yr_row++;
#endif
	}

#if !defined(STEPWAT) && !defined(RSOILWAT)
	sprintf(str, "%c%7.6f%c%7.6f%c%7.6f%c%7.6f%c%7.6f", _Sep, val_ppt, _Sep,
		val_rain, _Sep, val_snow, _Sep, val_snowmelt, _Sep, val_snowloss);
	strcat(sw_outstr, str);

#elif defined(STEPWAT)
	switch (pd)
	{
		case eSW_Day:
			p = SW_Model.doy-1;
			break;
		case eSW_Week:
			p = SW_Model.week-tOffset;
			break;
		case eSW_Month:
			p = SW_Model.month-tOffset;
			break;
		case eSW_Year:
			p = 0; // Iypc requires 0 for yearly timeperiod
			break;
	}
	if(isPartialSoilwatOutput == FALSE)
	{
		int
			ind0 = Iypc(Globals.currYear - 1, p, 0, pd), // index for mean
			ind1 = Iypc(Globals.currYear - 1, p, 1, pd); // index for sd
		float old_ppt = SXW_AVG.ppt_avg[ind0];
		float old_rain = SXW_AVG.val_rain_avg[ind0];
		float old_snow = SXW_AVG.val_snow_avg[ind0];
		float old_snowmelt = SXW_AVG.val_snowmelt_avg[ind0];
		float old_snowloss = SXW_AVG.val_snowloss_avg[ind0];

		SXW_AVG.ppt_avg[ind0] = get_running_avg(SXW_AVG.ppt_avg[ind0], val_ppt);
		SXW_AVG.val_rain_avg[ind0] = get_running_avg(SXW_AVG.val_rain_avg[ind0], val_rain);
		SXW_AVG.val_snow_avg[ind0] = get_running_avg(SXW_AVG.val_snow_avg[ind0], val_snow);
		SXW_AVG.val_snowmelt_avg[ind0] = get_running_avg(SXW_AVG.val_snowmelt_avg[ind0], val_snowmelt);
		SXW_AVG.val_snowloss_avg[ind0] = get_running_avg(SXW_AVG.val_snowloss_avg[ind0], val_snowloss);

		SXW_AVG.ppt_avg[ind1] += get_running_sqr(old_ppt, val_ppt, SXW_AVG.ppt_avg[ind0]);
		SXW_AVG.val_rain_avg[ind1] += get_running_sqr(old_rain, val_rain, SXW_AVG.val_rain_avg[ind0]);
		SXW_AVG.val_snow_avg[ind1] += get_running_sqr(old_snow, val_snow, SXW_AVG.val_snow_avg[ind0]);
		SXW_AVG.val_snowmelt_avg[ind1] += get_running_sqr(old_snowmelt, val_snowmelt, SXW_AVG.val_snowmelt_avg[ind0]);
		SXW_AVG.val_snowloss_avg[ind1] += get_running_sqr(old_snowloss, val_snowloss, SXW_AVG.val_snowloss_avg[ind0]);

		if(Globals.currIter == Globals.runModelIterations){
			float std_ppt = sqrt(SXW_AVG.ppt_avg[ind1] / Globals.currIter);
			float std_rain = sqrt(SXW_AVG.val_rain_avg[ind1] / Globals.currIter);
			float std_snow = sqrt(SXW_AVG.val_snow_avg[ind1] / Globals.currIter);
			float std_snowmelt = sqrt(SXW_AVG.val_snowmelt_avg[ind1] / Globals.currIter);
			float std_snowloss = sqrt(SXW_AVG.val_snowloss_avg[ind1] / Globals.currIter);

			sprintf(str, "%c%7.6f%c%7.6f%c%7.6f%c%7.6f%c%7.6f%c%7.6f%c%7.6f%c%7.6f%c%7.6f%c%7.6f",
			  _Sep, SXW_AVG.ppt_avg[ind0], _Sep, std_ppt, _Sep,
				SXW_AVG.val_rain_avg[ind0], _Sep, std_rain, _Sep, SXW_AVG.val_snow_avg[ind0], _Sep, std_snow,
				_Sep, SXW_AVG.val_snowmelt_avg[ind0],
				_Sep, std_snowmelt, _Sep, SXW_AVG.val_snowloss_avg[ind0], _Sep, std_snowloss);
			strcat(sw_outstr, str);
		}
	}
	if(storeAllIterations){
		sprintf(str_iters, "%c%7.6f%c%7.6f%c%7.6f%c%7.6f%c%7.6f", _Sep, val_ppt, _Sep,
			val_rain, _Sep, val_snow, _Sep, val_snowmelt, _Sep, val_snowloss);
		strcat(outstr_all_iters, str_iters);
	}
	SXW.ppt = val_ppt;
#endif
}

void get_vwcBulk(OutPeriod pd)
{
	/* --------------------------------------------------- */
	LyrIndex i;
	SW_SOILWAT *v = &SW_Soilwat;
	RealD *val = (RealD *) malloc(sizeof(RealD) * SW_Site.n_layers);
	ForEachSoilLayer(i)
		val[i] = SW_MISSING;

	#if !defined(STEPWAT) && !defined(RSOILWAT)
	  char str[OUTSTRLEN];
	  get_outstrleader(pd);

	#elif defined(STEPWAT)
	  char str[OUTSTRLEN];
		char str_iters[OUTSTRLEN];
		TimeInt p = 0;
	  if ((isPartialSoilwatOutput == FALSE && Globals.currIter == Globals.runModelIterations) || storeAllIterations)
	  {
	  	get_outstrleader(pd);
	  }
	#endif
	#ifdef RSOILWAT
		int delta;
		RealD *p;
	#endif

	switch (pd)
	{ /* vwcBulk at this point is identical to swcBulk */
	case eSW_Day:
#ifndef RSOILWAT
		ForEachSoilLayer(i)
			val[i] = v->dysum.vwcBulk[i] / SW_Site.lyr[i]->width;
#else
		delta = SW_Output[eSW_VWCBulk].dy_row;
		p = p_rOUT[eSW_VWCBulk][eSW_Day]; // set shorthand copy of 'p_rOUT' pointer
		p[delta + dy_nrow * 0] = SW_Model.simyear;
		p[delta + dy_nrow * 1] = SW_Model.doy;
		ForEachSoilLayer(i)
			p[delta + dy_nrow * (i + 2)] = v->dysum.vwcBulk[i] / SW_Site.lyr[i]->width;
		SW_Output[eSW_VWCBulk].dy_row++;
#endif
		break;
	case eSW_Week:
#ifndef RSOILWAT
		ForEachSoilLayer(i)
			val[i] = v->wkavg.vwcBulk[i] / SW_Site.lyr[i]->width;
#else
		delta = SW_Output[eSW_VWCBulk].wk_row;
		p = p_rOUT[eSW_VWCBulk][eSW_Week]; // set shorthand copy of 'p_rOUT' pointer
		p[delta + wk_nrow * 0] = SW_Model.simyear;
		p[delta + wk_nrow * 1] = (SW_Model.week + 1) - tOffset;
		ForEachSoilLayer(i)
			p[delta + wk_nrow * (i + 2)] = v->wkavg.vwcBulk[i] / SW_Site.lyr[i]->width;
		SW_Output[eSW_VWCBulk].wk_row++;
#endif
		break;
	case eSW_Month:
#ifndef RSOILWAT
		ForEachSoilLayer(i)
			val[i] = v->moavg.vwcBulk[i] / SW_Site.lyr[i]->width;
#else
		delta = SW_Output[eSW_VWCBulk].mo_row;
		p = p_rOUT[eSW_VWCBulk][eSW_Month]; // set shorthand copy of 'p_rOUT' pointer
		p[delta + mo_nrow * 0] = SW_Model.simyear;
		p[delta + mo_nrow * 1] = (SW_Model.month + 1) - tOffset;
		ForEachSoilLayer(i)
			p[delta + mo_nrow * (i + 2)] = v->moavg.vwcBulk[i] / SW_Site.lyr[i]->width;
		SW_Output[eSW_VWCBulk].mo_row++;
#endif
		break;
	case eSW_Year:
#ifndef RSOILWAT
		ForEachSoilLayer(i)
			val[i] = v->yravg.vwcBulk[i] / SW_Site.lyr[i]->width;
#else
		delta = SW_Output[eSW_VWCBulk].yr_row;
		p = p_rOUT[eSW_VWCBulk][eSW_Year]; // set shorthand copy of 'p_rOUT' pointer
		p[delta + yr_nrow * 0] = SW_Model.simyear;
		ForEachSoilLayer(i)
			p[delta + yr_nrow * (i + 1)] = v->yravg.vwcBulk[i] / SW_Site.lyr[i]->width;
		SW_Output[eSW_VWCBulk].yr_row++;
#endif
		break;
	}
#if !defined(STEPWAT) && !defined(RSOILWAT)
	ForEachSoilLayer(i)
	{
		sprintf(str, "%c%7.6f", _Sep, val[i]);
		strcat(sw_outstr, str);
	}
#elif defined(STEPWAT)
ForEachSoilLayer(i){
	switch (pd)
	{
		case eSW_Day:
			p = SW_Model.doy-1;
			break;
		case eSW_Week:
			p = SW_Model.week-tOffset;
			break;
		case eSW_Month:
			p = SW_Model.month-tOffset;
			break;
		case eSW_Year:
			p = 0; // Iypc/Iylp require 0 for yearly timeperiod
			break;
	}
		if (isPartialSoilwatOutput == FALSE)
		{
			int
				indl0 = Iylp(Globals.currYear - 1, i, p, pd, 0), // index for mean
				indl1 = Iylp(Globals.currYear - 1, i, p, pd, 1); // index for sd
			float old_val = SXW_AVG.vwcbulk_avg[indl0];

			SXW_AVG.vwcbulk_avg[indl0] = get_running_avg(SXW_AVG.vwcbulk_avg[indl0], val[i]);
			SXW_AVG.vwcbulk_avg[indl1] += get_running_sqr(old_val, val[i], SXW_AVG.vwcbulk_avg[indl0]);

			if(Globals.currIter == Globals.runModelIterations){
				float std_vwcbulk = sqrt(SXW_AVG.vwcbulk_avg[indl1] / Globals.currIter);
				sprintf(str, "%c%7.6f%c%7.6f", _Sep, SXW_AVG.vwcbulk_avg[indl0], _Sep, std_vwcbulk);
				strcat(sw_outstr, str);
			}
		}
		if(storeAllIterations){
			sprintf(str_iters, "%c%7.6f", _Sep, val[i]);
			strcat(outstr_all_iters, str_iters);
		}
	}

#endif
	free(val);
}

void get_vwcMatric(OutPeriod pd)
{
	/* --------------------------------------------------- */
	LyrIndex i;
	SW_SOILWAT *v = &SW_Soilwat;
	RealD convert;
	RealD *val = (RealD *) malloc(sizeof(RealD) * SW_Site.n_layers);
	ForEachSoilLayer(i)
		val[i] = SW_MISSING;

	#if !defined(STEPWAT) && !defined(RSOILWAT)
		char str[OUTSTRLEN];
		get_outstrleader(pd);

	#elif defined(STEPWAT)
		char str[OUTSTRLEN];
		char str_iters[OUTSTRLEN];
		TimeInt p = 0;
		if ((isPartialSoilwatOutput == FALSE && Globals.currIter == Globals.runModelIterations) || storeAllIterations)
		{
			get_outstrleader(pd);
		}
	#endif
	#ifdef RSOILWAT
		int delta;
		RealD *p;
	#endif

	/* vwcMatric at this point is identical to swcBulk */
	switch (pd)
	{
	case eSW_Day:
#ifndef RSOILWAT
		ForEachSoilLayer(i)
		{
			convert = 1. / (1. - SW_Site.lyr[i]->fractionVolBulk_gravel)
					/ SW_Site.lyr[i]->width;
			val[i] = v->dysum.vwcMatric[i] * convert;
		}
#else
		delta = SW_Output[eSW_VWCMatric].dy_row;
		p = p_rOUT[eSW_VWCMatric][eSW_Day]; // set shorthand copy of 'p_rOUT' pointer
		p[delta + dy_nrow * 0] = SW_Model.simyear;
		p[delta + dy_nrow * 1] = SW_Model.doy;
		ForEachSoilLayer(i)
		{
			convert = 1. / (1. - SW_Site.lyr[i]->fractionVolBulk_gravel) / SW_Site.lyr[i]->width;
			p[delta + dy_nrow * (i + 2)] = v->dysum.vwcMatric[i] * convert;
		}
		SW_Output[eSW_VWCMatric].dy_row++;
#endif
		break;
	case eSW_Week:
#ifndef RSOILWAT
		ForEachSoilLayer(i)
		{
			convert = 1. / (1. - SW_Site.lyr[i]->fractionVolBulk_gravel)
					/ SW_Site.lyr[i]->width;
			val[i] = v->wkavg.vwcMatric[i] * convert;
		}
#else
		delta = SW_Output[eSW_VWCMatric].wk_row;
		p = p_rOUT[eSW_VWCMatric][eSW_Week]; // set shorthand copy of 'p_rOUT' pointer
		p[delta + wk_nrow * 0] = SW_Model.simyear;
		p[delta + wk_nrow * 1] = (SW_Model.week + 1) - tOffset;
		ForEachSoilLayer(i)
		{
			convert = 1. / (1. - SW_Site.lyr[i]->fractionVolBulk_gravel) / SW_Site.lyr[i]->width;
			p[delta + wk_nrow * (i + 2)] = v->wkavg.vwcMatric[i] * convert;
		}
		SW_Output[eSW_VWCMatric].wk_row++;
#endif
		break;
	case eSW_Month:
#ifndef RSOILWAT
		ForEachSoilLayer(i)
		{
			convert = 1. / (1. - SW_Site.lyr[i]->fractionVolBulk_gravel)
					/ SW_Site.lyr[i]->width;
			val[i] = v->moavg.vwcMatric[i] * convert;
		}
#else
		delta = SW_Output[eSW_VWCMatric].mo_row;
		p = p_rOUT[eSW_VWCMatric][eSW_Month]; // set shorthand copy of 'p_rOUT' pointer
		p[delta + mo_nrow * 0] = SW_Model.simyear;
		p[delta + mo_nrow * 1] = (SW_Model.month + 1) - tOffset;
		ForEachSoilLayer(i)
		{
			convert = 1. / (1. - SW_Site.lyr[i]->fractionVolBulk_gravel) / SW_Site.lyr[i]->width;
			p[delta + mo_nrow * (i + 2)] = v->moavg.vwcMatric[i] * convert;
		}
		SW_Output[eSW_VWCMatric].mo_row++;
#endif
		break;
	case eSW_Year:
#ifndef RSOILWAT
		ForEachSoilLayer(i)
		{
			convert = 1. / (1. - SW_Site.lyr[i]->fractionVolBulk_gravel)
					/ SW_Site.lyr[i]->width;
			val[i] = v->yravg.vwcMatric[i] * convert;
		}
#else
		delta = SW_Output[eSW_VWCMatric].yr_row;
		p = p_rOUT[eSW_VWCMatric][eSW_Year]; // set shorthand copy of 'p_rOUT' pointer
		p[delta + yr_nrow * 0] = SW_Model.simyear;
		ForEachSoilLayer(i)
		{
			convert = 1. / (1. - SW_Site.lyr[i]->fractionVolBulk_gravel) / SW_Site.lyr[i]->width;
      p[delta + yr_nrow * (i + 1)] = v->yravg.vwcMatric[i] * convert;
		}
		SW_Output[eSW_VWCMatric].yr_row++;
#endif
		break;
	}
#if !defined(STEPWAT) && !defined(RSOILWAT)
	ForEachSoilLayer(i)
	{
		sprintf(str, "%c%7.6f", _Sep, val[i]);
		strcat(sw_outstr, str);
	}
#elif defined(STEPWAT)
switch (pd)
{
	case eSW_Day:
		p = SW_Model.doy-1;
		break;
	case eSW_Week:
		p = SW_Model.week-tOffset;
		break;
	case eSW_Month:
		p = SW_Model.month-tOffset;
		break;
	case eSW_Year:
		p = 0; // Iypc requires 0 for yearly timeperiod
		break;
}
ForEachSoilLayer(i){
	if (isPartialSoilwatOutput == FALSE)
	{
		int
			indl0 = Iylp(Globals.currYear - 1, i, p, pd, 0), // index for mean
			indl1 = Iylp(Globals.currYear - 1, i, p, pd, 1); // index for sd
		float old_val = SXW_AVG.vwcmatric_avg[indl0];

		SXW_AVG.vwcmatric_avg[indl0] = get_running_avg(SXW_AVG.vwcmatric_avg[indl0], val[i]);
		SXW_AVG.vwcmatric_avg[indl1] += get_running_sqr(old_val, val[i], SXW_AVG.vwcmatric_avg[indl0]);

		if(Globals.currIter == Globals.runModelIterations){
			float std_vwcmatric = sqrt(SXW_AVG.vwcmatric_avg[indl1] / Globals.currIter);
			sprintf(str, "%c%7.6f%c%7.6f", _Sep, SXW_AVG.vwcmatric_avg[indl0], _Sep, std_vwcmatric);
			strcat(sw_outstr, str);
		}
	}
	if(storeAllIterations){
		sprintf(str_iters, "%c%7.6f", _Sep, val[i]);
		strcat(outstr_all_iters, str_iters);
	}
}
#endif
	free(val);
}

/**
**/
void get_swa(OutPeriod pd)
{
	/* --------------------------------------------------- */
	/* added 21-Oct-03, cwb */
	#ifdef STEPWAT
		TimeInt p = 0;
		int j = 0;
		char str[OUTSTRLEN];
		char str_iters[OUTSTRLEN];
		RealF val[NVEGTYPES][MAX_LAYERS]; // need 2D array for values
	#endif

	LyrIndex i;
	SW_SOILWAT *v = &SW_Soilwat;

	#if !defined(STEPWAT) && !defined(RSOILWAT)
		int j = 0;
		RealF val[NVEGTYPES][MAX_LAYERS]; // need 2D array for values
		char str[OUTSTRLEN];
		get_outstrleader(pd);
		ForEachSoilLayer(i)
		{
			ForEachVegType(j){ // need to go over all veg types for each layer
				switch (pd)
				{
				case eSW_Day:
					val[j][i] = v->dysum.SWA_VegType[j][i];
					break;
				case eSW_Week:
					val[j][i] = v->wkavg.SWA_VegType[j][i];
					break;
				case eSW_Month:
					val[j][i] = v->moavg.SWA_VegType[j][i];
					break;
				case eSW_Year:
					val[j][i] = v->yravg.SWA_VegType[j][i];
					break;
				}
			}
				// write values to string
				sprintf(str, "%c%7.6f%c%7.6f%c%7.6f%c%7.6f",_Sep, val[0][i], _Sep, val[1][i], _Sep, val[2][i], _Sep, val[3][i]);
				strcat(sw_outstr, str);
		}
	#elif defined(RSOILWAT)
		switch (pd)
		{
			case eSW_Day:
			p_rOUT[eSW_SWA][eSW_Day][SW_Output[eSW_SWA].dy_row + dy_nrow * 0] = SW_Model.simyear;
			p_rOUT[eSW_SWA][eSW_Day][SW_Output[eSW_SWA].dy_row + dy_nrow * 1] = SW_Model.doy;
			break;
			case eSW_Week:
			p_rOUT[eSW_SWA][eSW_Week][SW_Output[eSW_SWA].wk_row + wk_nrow * 0] = SW_Model.simyear;
			p_rOUT[eSW_SWA][eSW_Week][SW_Output[eSW_SWA].wk_row + wk_nrow * 1] = (SW_Model.week + 1) - tOffset;
			break;
			case eSW_Month:
			p_rOUT[eSW_SWA][eSW_Month][SW_Output[eSW_SWA].mo_row + mo_nrow * 0] = SW_Model.simyear;
			p_rOUT[eSW_SWA][eSW_Month][SW_Output[eSW_SWA].mo_row + mo_nrow * 1] = (SW_Model.month + 1) - tOffset;
			break;
			case eSW_Year:
			p_rOUT[eSW_SWA][eSW_Year][SW_Output[eSW_SWA].yr_row + yr_nrow * 0] = SW_Model.simyear;
			break;
		}
		switch (pd)
		{
			// tree
			case eSW_Day:
			ForEachSoilLayer(i){
				p_rOUT[eSW_SWA][eSW_Day][SW_Output[eSW_SWA].dy_row + dy_nrow * (i + 2)] = v->dysum.SWA_VegType[0][i];
				//printf("tree: %f\n", v->dysum.SWA_VegType[0][i]);
			}
			break;
			case eSW_Week:
			ForEachSoilLayer(i)
				p_rOUT[eSW_SWA][eSW_Week][SW_Output[eSW_SWA].wk_row + wk_nrow * (i + 2)] = v->wkavg.SWA_VegType[0][i];
			break;
			case eSW_Month:
			ForEachSoilLayer(i)
				p_rOUT[eSW_SWA][eSW_Month][SW_Output[eSW_SWA].mo_row + mo_nrow * (i + 2)] = v->moavg.SWA_VegType[0][i];
			break;
			case eSW_Year:
			ForEachSoilLayer(i)
				p_rOUT[eSW_SWA][eSW_Year][SW_Output[eSW_SWA].yr_row + yr_nrow * (i + 1)] = v->yravg.SWA_VegType[0][i];
			break;
		}
		switch (pd)
		{
			// shrub
			case eSW_Day:
			ForEachSoilLayer(i){
			p_rOUT[eSW_SWA][eSW_Day][SW_Output[eSW_SWA].dy_row + dy_nrow * (i + 2) + (dy_nrow * SW_Site.n_layers * 1)] = v->dysum.SWA_VegType[1][i];
				//printf("shrub: %f\n", v->dysum.SWA_VegType[1][i]);
			}
			break;
			case eSW_Week:
			ForEachSoilLayer(i)
			p_rOUT[eSW_SWA][eSW_Week][SW_Output[eSW_SWA].wk_row + wk_nrow * (i + 2) + (wk_nrow * SW_Site.n_layers * 1)] = v->wkavg.SWA_VegType[1][i];
			break;
			case eSW_Month:
			ForEachSoilLayer(i)
			p_rOUT[eSW_SWA][eSW_Month][SW_Output[eSW_SWA].mo_row + mo_nrow * (i + 2) + (mo_nrow * SW_Site.n_layers * 1)] = v->moavg.SWA_VegType[1][i];
			break;
			case eSW_Year:
			ForEachSoilLayer(i)
			p_rOUT[eSW_SWA][eSW_Year][SW_Output[eSW_SWA].yr_row + yr_nrow * (i + 1) + (yr_nrow * SW_Site.n_layers * 1)] = v->yravg.SWA_VegType[1][i];
			break;
		}
		switch (pd)
		{
			// forbs
			case eSW_Day:
			ForEachSoilLayer(i){
				p_rOUT[eSW_SWA][eSW_Day][SW_Output[eSW_SWA].dy_row + dy_nrow * (i + 2) + (dy_nrow * SW_Site.n_layers * 2)] = v->dysum.SWA_VegType[2][i];
				//printf("v->dysum.SWA_VegType[2][%d]: %f\n", i, v->dysum.SWA_VegType[2][i]);
				//printf("p_rOUT[eSW_SWA][eSW_Day][SW_Output[eSW_SWA].dy_row + dy_nrow * (%d) + (%d)]: %f\n", i + 2, (dy_nrow * SW_Site.n_layers * 2), p_rOUT[eSW_SWA][eSW_Day][SW_Output[eSW_SWA].dy_row + dy_nrow * (i + 2) + (dy_nrow * SW_Site.n_layers * 2)]);
				//printf("2: %f\n",v->dysum.SWA_VegType[2][i]);
			}
			break;
			case eSW_Week:
			ForEachSoilLayer(i)
			p_rOUT[eSW_SWA][eSW_Week][SW_Output[eSW_SWA].wk_row + wk_nrow * (i + 2) + (wk_nrow * SW_Site.n_layers * 2)] = v->wkavg.SWA_VegType[2][i];
			break;
			case eSW_Month:
			ForEachSoilLayer(i)
			p_rOUT[eSW_SWA][eSW_Month][SW_Output[eSW_SWA].mo_row + mo_nrow * (i + 2) + (mo_nrow * SW_Site.n_layers * 2)] = v->moavg.SWA_VegType[2][i];
			break;
			case eSW_Year:
			ForEachSoilLayer(i)
			p_rOUT[eSW_SWA][eSW_Year][SW_Output[eSW_SWA].yr_row + yr_nrow * (i + 1) + (yr_nrow * SW_Site.n_layers * 2)] = v->yravg.SWA_VegType[2][i];
			break;
		}
		switch (pd)
		{
			// grass
			case eSW_Day:
			ForEachSoilLayer(i){
			p_rOUT[eSW_SWA][eSW_Day][SW_Output[eSW_SWA].dy_row + dy_nrow * (i + 2) + (dy_nrow * SW_Site.n_layers * 3)] = v->dysum.SWA_VegType[3][i];
			//printf("grass: %f\n", p_rOUT[eSW_SWA][eSW_Day][SW_Output[eSW_SWA].dy_row + dy_nrow * (i + 2) + (dy_nrow * SW_Site.n_layers * 3)]);
			}
			SW_Output[eSW_SWA].dy_row++;
			break;
			case eSW_Week:
			ForEachSoilLayer(i)
			p_rOUT[eSW_SWA][eSW_Week][SW_Output[eSW_SWA].wk_row + wk_nrow * (i + 2) + (wk_nrow * SW_Site.n_layers * 3)] = v->wkavg.SWA_VegType[3][i];
			SW_Output[eSW_SWA].wk_row++;
			break;
			case eSW_Month:
			ForEachSoilLayer(i)
			p_rOUT[eSW_SWA][eSW_Month][SW_Output[eSW_SWA].mo_row + mo_nrow * (i + 2) + (mo_nrow * SW_Site.n_layers * 3)] = v->moavg.SWA_VegType[3][i];
			SW_Output[eSW_SWA].mo_row++;
			break;
			case eSW_Year:
			ForEachSoilLayer(i)
			p_rOUT[eSW_SWA][eSW_Year][SW_Output[eSW_SWA].yr_row + yr_nrow * (i + 1) + (yr_nrow * SW_Site.n_layers * 3)] = v->yravg.SWA_VegType[3][i];
			SW_Output[eSW_SWA].yr_row++;
			break;
		}
	#elif defined(STEPWAT)
		if ((isPartialSoilwatOutput == FALSE && Globals.currIter == Globals.runModelIterations) || storeAllIterations)
			get_outstrleader(pd);

		ForEachSoilLayer(i)
		{
			ForEachVegType(j){
				switch (pd)
				{
					case eSW_Day:
						p = SW_Model.doy-1;
						val[j][i] = v->dysum.SWA_VegType[j][i];
						break;
					case eSW_Week:
						p = SW_Model.week-tOffset;
						val[j][i] = v->wkavg.SWA_VegType[j][i];
						break;
					case eSW_Month:
						p = SW_Model.month-tOffset;
						val[j][i] = v->moavg.SWA_VegType[j][i];
						break;
					case eSW_Year:
						p = Globals.currYear - 1;
						val[j][i] = v->yravg.SWA_VegType[j][i];
						break;
				}
				SXW.sum_dSWA_repartitioned[Ivlp(j,i,p)] = val[j][i];
			}

			if(storeAllIterations){
				sprintf(str_iters, "%c%7.6f%c%7.6f%c%7.6f%c%7.6f",_Sep, val[0][i], _Sep, val[1][i], _Sep, val[2][i], _Sep, val[3][i]);
				strcat(outstr_all_iters, str_iters);
			}

			if (isPartialSoilwatOutput == FALSE)
			{
				int
					indl0 = Iylp(Globals.currYear - 1, i, p, pd, 0), // index for mean
					indl1 = Iylp(Globals.currYear - 1, i, p, pd, 1); // index for sd
				// get old average for use in running square
				float old_tree = SXW.SWA_tree_avg[indl0];
				float old_shrub = SXW.SWA_shrub_avg[indl0];
				float old_forb = SXW.SWA_forb_avg[indl0];
				float old_grass = SXW.SWA_grass_avg[indl0];

				// get running average over all iterations
				SXW.SWA_tree_avg[indl0] = get_running_avg(SXW.SWA_tree_avg[indl0], val[0][i]);
				SXW.SWA_shrub_avg[indl0] = get_running_avg(SXW.SWA_shrub_avg[indl0], val[1][i]);
				SXW.SWA_forb_avg[indl0] = get_running_avg(SXW.SWA_forb_avg[indl0], val[2][i]);
				SXW.SWA_grass_avg[indl0] = get_running_avg(SXW.SWA_grass_avg[indl0], val[3][i]);
				//if(i == 7 && p == 0 && Globals.currYear-1 == 0)
					//printf("SXW.sum_dSWA_repartitioned[Ivlp(3,i,p)]: %f\n", val[3][i]);
				// get running square over all iterations. Going to be used to get standard deviation
				SXW.SWA_tree_avg[indl1] = get_running_sqr(old_tree, val[0][i], SXW.SWA_tree_avg[indl0]);
				SXW.SWA_shrub_avg[indl1] = get_running_sqr(old_shrub, val[1][i], SXW.SWA_shrub_avg[indl0]);
				SXW.SWA_forb_avg[indl1] = get_running_sqr(old_forb, val[2][i], SXW.SWA_forb_avg[indl0]);
				SXW.SWA_grass_avg[indl1] = get_running_sqr(old_grass, val[3][i], SXW.SWA_grass_avg[indl0]);

				// divide by number of iterations at end to store average
				if(Globals.currIter == Globals.runModelIterations){
					//if(i == 7 && p == 0 && Globals.currYear-1 == 0)
						//printf("SXW.SWA_grass_avg[Iylp(%d,%d,%d,0)]: %f\n", Globals.currYear-1,i,p, SXW.SWA_grass_avg[indl0]);
					// get standard deviation
					float std_forb = sqrt(SXW.SWA_forb_avg[indl1] / Globals.currIter);
					float std_tree = sqrt(SXW.SWA_tree_avg[indl1] / Globals.currIter);
					float std_shrub = sqrt(SXW.SWA_shrub_avg[indl1] / Globals.currIter);
					float std_grass = sqrt(SXW.SWA_grass_avg[indl1] / Globals.currIter);

					sprintf(str, "%c%7.6f%c%7.6f%c%7.6f%c%7.6f%c%7.6f%c%7.6f%c%7.6f%c%7.6f",_Sep, SXW.SWA_tree_avg[indl0],
					 	_Sep, std_tree, _Sep, SXW.SWA_shrub_avg[indl0], _Sep, std_shrub, _Sep, SXW.SWA_forb_avg[indl0],
						_Sep, std_forb, _Sep, SXW.SWA_grass_avg[indl0], _Sep, std_grass);
					strcat(sw_outstr, str);
				}
				if (bFlush_output) p++;
			}
		}
	#endif
}


void get_swcBulk(OutPeriod pd)
{
	/* --------------------------------------------------- */
	/* added 21-Oct-03, cwb */
#ifdef STEPWAT
	TimeInt p = 0;
	RealD val = SW_MISSING;
#endif

#ifdef RSOILWAT
		int delta;
		RealD *p;
#endif
	LyrIndex i;
	SW_SOILWAT *v = &SW_Soilwat;
#if !defined(STEPWAT) && !defined(RSOILWAT)
	RealD val = SW_MISSING;
	char str[OUTSTRLEN];
	get_outstrleader(pd);
	ForEachSoilLayer(i)
	{
		switch (pd)
		{
		case eSW_Day:
			val = v->dysum.swcBulk[i];
			break;
		case eSW_Week:
			val = v->wkavg.swcBulk[i];
			break;
		case eSW_Month:
			val = v->moavg.swcBulk[i];
			break;
		case eSW_Year:
			val = v->yravg.swcBulk[i];
			break;
		}
		sprintf(str, "%c%7.6f", _Sep, val);
		strcat(sw_outstr, str);
	}
#elif defined(RSOILWAT)
	switch (pd)
	{
		case eSW_Day:
		delta = SW_Output[eSW_SWCBulk].dy_row;
		p = p_rOUT[eSW_SWCBulk][eSW_Day]; // set shorthand copy of 'p_rOUT' pointer
		p[delta + dy_nrow * 0] = SW_Model.simyear;
		p[delta + dy_nrow * 1] = SW_Model.doy;
		ForEachSoilLayer(i)
			p[delta + dy_nrow * (i + 2)] = v->dysum.swcBulk[i];
		SW_Output[eSW_SWCBulk].dy_row++;
		break;
		case eSW_Week:
		delta = SW_Output[eSW_SWCBulk].wk_row;
		p = p_rOUT[eSW_SWCBulk][eSW_Week]; // set shorthand copy of 'p_rOUT' pointer
		p[delta + wk_nrow * 0] = SW_Model.simyear;
		p[delta + wk_nrow * 1] = (SW_Model.week + 1) - tOffset;
		ForEachSoilLayer(i)
			p[delta + wk_nrow * (i + 2)] = v->wkavg.swcBulk[i];
		SW_Output[eSW_SWCBulk].wk_row++;
		break;
		case eSW_Month:
		delta = SW_Output[eSW_SWCBulk].mo_row;
		p = p_rOUT[eSW_SWCBulk][eSW_Month]; // set shorthand copy of 'p_rOUT' pointer
		p[delta + mo_nrow * 0] = SW_Model.simyear;
		p[delta + mo_nrow * 1] = (SW_Model.month + 1) - tOffset;
		ForEachSoilLayer(i)
			p[delta + mo_nrow * (i + 2)] = v->moavg.swcBulk[i];
		SW_Output[eSW_SWCBulk].mo_row++;
		break;
		case eSW_Year:
		delta = SW_Output[eSW_SWCBulk].yr_row;
		p = p_rOUT[eSW_SWCBulk][eSW_Year]; // set shorthand copy of 'p_rOUT' pointer
		p[delta + yr_nrow * 0] = SW_Model.simyear;
		ForEachSoilLayer(i)
			p[delta + yr_nrow * (i + 1)] = v->yravg.swcBulk[i];
		SW_Output[eSW_SWCBulk].yr_row++;
		break;
	}
#elif defined(STEPWAT)
	char str[OUTSTRLEN];
	char str_iters[OUTSTRLEN];

	if ((isPartialSoilwatOutput == FALSE && Globals.currIter == Globals.runModelIterations) || storeAllIterations)
		get_outstrleader(pd);


	ForEachSoilLayer(i)
	{
		switch (pd)
		{
			case eSW_Day:
				p = SW_Model.doy - 1;
				val = v->dysum.swcBulk[i];
				break; // print current but as index
			case eSW_Week:
				p = SW_Model.week - 1;
				val = v->wkavg.swcBulk[i];
				break;// print previous to current
			case eSW_Month:
				p = SW_Model.month-1;
				val = v->moavg.swcBulk[i];
				break;// print previous to current
			case eSW_Year:
				p = Globals.currYear - 1;
				val = v->yravg.swcBulk[i];
				break;
		}
		if(storeAllIterations){
			sprintf(str_iters, "%c%7.6f", _Sep, val);
			strcat(outstr_all_iters, str_iters);
		}
		if (isPartialSoilwatOutput == FALSE)
		{
			int
				indl0 = Iylp(Globals.currYear - 1, i, p, pd, 0), // index for mean
				indl1 = Iylp(Globals.currYear - 1, i, p, pd, 1); // index for sd
			float old_val = 0.;
			old_val = SXW_AVG.swc_avg[indl0];

			SXW_AVG.swc_avg[indl0] = get_running_avg(SXW_AVG.swc_avg[indl0], val);
			SXW_AVG.swc_avg[indl1] += get_running_sqr(old_val, val, SXW_AVG.swc_avg[indl0]);

			if(Globals.currIter == Globals.runModelIterations){
				float swc_std = sqrt(SXW_AVG.swc_avg[indl1] / Globals.currIter);

				sprintf(str, "%c%7.6f%c%7.6f", _Sep, SXW_AVG.swc_avg[indl0], _Sep, swc_std);
				strcat(sw_outstr, str);
			}

		}
		if (bFlush_output) p++;
		SXW.swc[Ilp(i,p)] = val; // SXW.swc[Ilp(layer,timeperiod)]
	}

#endif
}

void get_swpMatric(OutPeriod pd)
{
	/* --------------------------------------------------- */
	/* can't take arithmetic average of swp because it's
	 * exponential.  At this time (until I remember to look
	 * up whether harmonic or some other average is better
	 * and fix this) we're not averaging swp but converting
	 * the averaged swc.  This also avoids converting for
	 * each day.
	 *
	 * added 12-Oct-03, cwb */

	LyrIndex i;
	SW_SOILWAT *v = &SW_Soilwat;

	#if !defined(STEPWAT) && !defined(RSOILWAT)
	  char str[OUTSTRLEN];
		RealD val = SW_MISSING;
	  get_outstrleader(pd);

	#elif defined(STEPWAT)
	  char str[OUTSTRLEN];
		char str_iters[OUTSTRLEN];
		RealD val = SW_MISSING;
		TimeInt p = 0;
	  if ((isPartialSoilwatOutput == FALSE && Globals.currIter == Globals.runModelIterations) || storeAllIterations)
	  {
	  	get_outstrleader(pd);
	  }
	#endif


	#if !defined(STEPWAT) && !defined(RSOILWAT)
	ForEachSoilLayer(i)
	{
			switch (pd)
			{ /* swpMatric at this point is identical to swcBulk */
			case eSW_Day:
				val = SW_SWCbulk2SWPmatric(SW_Site.lyr[i]->fractionVolBulk_gravel,
						v->dysum.swpMatric[i], i);
				break;
			case eSW_Week:
				val = SW_SWCbulk2SWPmatric(SW_Site.lyr[i]->fractionVolBulk_gravel,
						v->wkavg.swpMatric[i], i);
				break;
			case eSW_Month:
				val = SW_SWCbulk2SWPmatric(SW_Site.lyr[i]->fractionVolBulk_gravel,
						v->moavg.swpMatric[i], i);
				break;
			case eSW_Year:
				val = SW_SWCbulk2SWPmatric(SW_Site.lyr[i]->fractionVolBulk_gravel,
						v->yravg.swpMatric[i], i);
				break;
			}
			sprintf(str, "%c%7.6f", _Sep, val);
			strcat(sw_outstr, str);
		}

	#elif defined(STEPWAT)
	ForEachSoilLayer(i)
	{
	switch (pd)
	{ /* swpMatric at this point is identical to swcBulk */
	case eSW_Day:
		val = SW_SWCbulk2SWPmatric(SW_Site.lyr[i]->fractionVolBulk_gravel,
				v->dysum.swpMatric[i], i);
		p = SW_Model.doy-1;
		break;
	case eSW_Week:
		p = SW_Model.week-tOffset;
		val = SW_SWCbulk2SWPmatric(SW_Site.lyr[i]->fractionVolBulk_gravel,
				v->wkavg.swpMatric[i], i);
		break;
	case eSW_Month:
		p = SW_Model.month-tOffset;
		val = SW_SWCbulk2SWPmatric(SW_Site.lyr[i]->fractionVolBulk_gravel,
				v->moavg.swpMatric[i], i);
		break;
	case eSW_Year:
		p = Globals.currYear - 1;
		val = SW_SWCbulk2SWPmatric(SW_Site.lyr[i]->fractionVolBulk_gravel,
				v->yravg.swpMatric[i], i);
		break;
	}
	if (isPartialSoilwatOutput == FALSE)
	{
		int
			indl0 = Iylp(Globals.currYear - 1, i, p, pd, 0), // index for mean
			indl1 = Iylp(Globals.currYear - 1, i, p, pd, 1); // index for sd
		float old_val = SXW_AVG.swpmatric_avg[indl0];

		SXW_AVG.swpmatric_avg[indl0] = get_running_avg(SXW_AVG.swpmatric_avg[indl0], val);
		SXW_AVG.swpmatric_avg[indl1] += get_running_sqr(old_val, val, SXW_AVG.swpmatric_avg[indl0]);

		if(Globals.currIter == Globals.runModelIterations){
			float std = sqrt(SXW_AVG.swpmatric_avg[indl1] / Globals.currIter);

			sprintf(str, "%c%7.6f%c%7.6f", _Sep, SXW_AVG.swpmatric_avg[indl0], _Sep, std);
			strcat(sw_outstr, str);
		}
	}
	if(storeAllIterations){
		sprintf(str_iters, "%c%7.6f", _Sep, val);
		strcat(outstr_all_iters, str_iters);
	}
}

#else
	int delta;
	RealD *p;

	switch (pd)
	{
		case eSW_Day:
		delta = SW_Output[eSW_SWPMatric].dy_row;
		p = p_rOUT[eSW_SWPMatric][eSW_Day]; // set shorthand copy of 'p_rOUT' pointer
		p[delta + dy_nrow * 0] = SW_Model.simyear;
		p[delta + dy_nrow * 1] = SW_Model.doy;
		ForEachSoilLayer(i)
			p[delta + dy_nrow * (i + 2)] = SW_SWCbulk2SWPmatric(SW_Site.lyr[i]->fractionVolBulk_gravel, v->dysum.swpMatric[i], i);
		SW_Output[eSW_SWPMatric].dy_row++;
		break;
		case eSW_Week:
		delta = SW_Output[eSW_SWPMatric].wk_row;
		p = p_rOUT[eSW_SWPMatric][eSW_Week]; // set shorthand copy of 'p_rOUT' pointer
		p[delta + wk_nrow * 0] = SW_Model.simyear;
		p[delta + wk_nrow * 1] = (SW_Model.week + 1) - tOffset;
		ForEachSoilLayer(i)
			p[delta + wk_nrow * (i + 2)] = SW_SWCbulk2SWPmatric(SW_Site.lyr[i]->fractionVolBulk_gravel, v->wkavg.swpMatric[i], i);
		SW_Output[eSW_SWPMatric].wk_row++;
		break;
		case eSW_Month:
		delta = SW_Output[eSW_SWPMatric].mo_row;
		p = p_rOUT[eSW_SWPMatric][eSW_Month]; // set shorthand copy of 'p_rOUT' pointer
		p[delta + mo_nrow * 0] = SW_Model.simyear;
		p[delta + mo_nrow * 1] = (SW_Model.month + 1) - tOffset;
		ForEachSoilLayer(i)
			p[delta + mo_nrow * (i + 2)] = SW_SWCbulk2SWPmatric(SW_Site.lyr[i]->fractionVolBulk_gravel, v->moavg.swpMatric[i], i);
		SW_Output[eSW_SWPMatric].mo_row++;
		break;
		case eSW_Year:
		delta = SW_Output[eSW_SWPMatric].yr_row;
		p = p_rOUT[eSW_SWPMatric][eSW_Year]; // set shorthand copy of 'p_rOUT' pointer
		p[delta + yr_nrow * 0] = SW_Model.simyear;
		ForEachSoilLayer(i)
			p[delta + yr_nrow * (i + 1)] = SW_SWCbulk2SWPmatric(SW_Site.lyr[i]->fractionVolBulk_gravel, v->yravg.swpMatric[i], i);
		SW_Output[eSW_SWPMatric].yr_row++;
		break;
	}
#endif
}

void get_swaBulk(OutPeriod pd)
{
	/* --------------------------------------------------- */

	LyrIndex i;
	SW_SOILWAT *v = &SW_Soilwat;
	#if !defined(STEPWAT) && !defined(RSOILWAT)
	  char str[OUTSTRLEN];
		RealD val = SW_MISSING;
	  get_outstrleader(pd);

	#elif defined(STEPWAT)
		RealD val = SW_MISSING;
	  char str[OUTSTRLEN];
		char str_iters[OUTSTRLEN];
		TimeInt p = 0;
	  if ((isPartialSoilwatOutput == FALSE && Globals.currIter == Globals.runModelIterations) || storeAllIterations)
	  {
	  	get_outstrleader(pd);
	  }
	#endif

#if !defined(STEPWAT) && !defined(RSOILWAT)
	ForEachSoilLayer(i)
	{
		switch (pd)
		{
		case eSW_Day:
			val = v->dysum.swaBulk[i];
			break;
		case eSW_Week:
			val = v->wkavg.swaBulk[i];
			break;
		case eSW_Month:
			val = v->moavg.swaBulk[i];
			break;
		case eSW_Year:
			val = v->yravg.swaBulk[i];
			break;
		}

		sprintf(str, "%c%7.6f", _Sep, val);
		strcat(sw_outstr, str);
	}
	#elif defined(STEPWAT)
	ForEachSoilLayer(i)
	{
		switch (pd)
		{
			case eSW_Day:
				p = SW_Model.doy-1;
				val = v->dysum.swaBulk[i];
				break;
			case eSW_Week:
				val = v->wkavg.swaBulk[i];
				p = SW_Model.week-tOffset;
				break;
			case eSW_Month:
				p = SW_Model.month-tOffset;
				val = v->moavg.swaBulk[i];
				break;
			case eSW_Year:
				val = v->yravg.swaBulk[i];
				p = Globals.currYear - 1;
				break;
		}
		if (isPartialSoilwatOutput == FALSE)
		{
			int
				indl0 = Iylp(Globals.currYear - 1, i, p, pd, 0), // index for mean
				indl1 = Iylp(Globals.currYear - 1, i, p, pd, 1); // index for sd
			float old_val = SXW_AVG.swabulk_avg[indl0];

			SXW_AVG.swabulk_avg[indl0] = get_running_avg(SXW_AVG.swabulk_avg[indl0], val);
			SXW_AVG.swabulk_avg[indl1] += get_running_sqr(old_val, val, SXW_AVG.swabulk_avg[indl0]);

			if(Globals.currIter == Globals.runModelIterations){
				float std = sqrt(SXW_AVG.swabulk_avg[indl1] / Globals.currIter);

				sprintf(str, "%c%7.6f%c%7.6f", _Sep, SXW_AVG.swabulk_avg[indl0], _Sep, std);
				strcat(sw_outstr, str);
			}
		}
		if(storeAllIterations){
			sprintf(str_iters, "%c%7.6f", _Sep, val);
			strcat(outstr_all_iters, str_iters);
		}

	}
#else
	int delta;
	RealD *p;

	switch (pd)
	{
		case eSW_Day:
		delta = SW_Output[eSW_SWABulk].dy_row;
		p = p_rOUT[eSW_SWABulk][eSW_Day]; // set shorthand copy of 'p_rOUT' pointer
		p[delta + dy_nrow * 0] = SW_Model.simyear;
		p[delta + dy_nrow * 1] = SW_Model.doy;
		ForEachSoilLayer(i)
			p[delta + dy_nrow * (i + 2)] = v->dysum.swaBulk[i];
		SW_Output[eSW_SWABulk].dy_row++;
		break;
		case eSW_Week:
		delta = SW_Output[eSW_SWABulk].wk_row;
		p = p_rOUT[eSW_SWABulk][eSW_Week]; // set shorthand copy of 'p_rOUT' pointer
		p[delta + wk_nrow * 0] = SW_Model.simyear;
		p[delta + wk_nrow * 1] = (SW_Model.week + 1) - tOffset;
		ForEachSoilLayer(i)
			p[delta + wk_nrow * (i + 2)] = v->wkavg.swaBulk[i];
		SW_Output[eSW_SWABulk].wk_row++;
		break;
		case eSW_Month:
		delta = SW_Output[eSW_SWABulk].mo_row;
		p = p_rOUT[eSW_SWABulk][eSW_Month]; // set shorthand copy of 'p_rOUT' pointer
		p[delta + mo_nrow * 0] = SW_Model.simyear;
		p[delta + mo_nrow * 1] = (SW_Model.month + 1) - tOffset;
		ForEachSoilLayer(i)
			p[delta + mo_nrow * (i + 2)] = v->moavg.swaBulk[i];
		SW_Output[eSW_SWABulk].mo_row++;
		break;
		case eSW_Year:
		delta = SW_Output[eSW_SWABulk].yr_row;
		p = p_rOUT[eSW_SWABulk][eSW_Year]; // set shorthand copy of 'p_rOUT' pointer
		p[delta + yr_nrow * 0] = SW_Model.simyear;
		ForEachSoilLayer(i)
			p[delta + yr_nrow * (i + 1)] = v->yravg.swaBulk[i];
		SW_Output[eSW_SWABulk].yr_row++;
		break;
	}
#endif
}

void get_swaMatric(OutPeriod pd)
{
	/* --------------------------------------------------- */

	LyrIndex i;
	SW_SOILWAT *v = &SW_Soilwat;
	RealD convert;

	#if !defined(STEPWAT) && !defined(RSOILWAT)
	  char str[OUTSTRLEN];
		RealD val = SW_MISSING;
	  get_outstrleader(pd);

	#elif defined(STEPWAT)
	  char str[OUTSTRLEN];
		char str_iters[OUTSTRLEN];
		RealD val = SW_MISSING;
		TimeInt p = 0;
	  if ((isPartialSoilwatOutput == FALSE && Globals.currIter == Globals.runModelIterations) || storeAllIterations)
	  {
	  	get_outstrleader(pd);
	  }
	#endif

#if !defined(STEPWAT) && !defined(RSOILWAT)
	ForEachSoilLayer(i)
	{ /* swaMatric at this point is identical to swaBulk */
		convert = 1. / (1. - SW_Site.lyr[i]->fractionVolBulk_gravel);
		switch (pd)
		{
		case eSW_Day:
			val = v->dysum.swaMatric[i] * convert;
			break;
		case eSW_Week:
			val = v->wkavg.swaMatric[i] * convert;
			break;
		case eSW_Month:
			val = v->moavg.swaMatric[i] * convert;
			break;
		case eSW_Year:
			val = v->yravg.swaMatric[i] * convert;
			break;
		}
		sprintf(str, "%c%7.6f", _Sep, val);
		strcat(sw_outstr, str);
	}
	#elif defined(STEPWAT)
	ForEachSoilLayer(i)
	{
		convert = 1. / (1. - SW_Site.lyr[i]->fractionVolBulk_gravel);
		switch (pd)
		{
			case eSW_Day:
				p = SW_Model.doy-1;
				val = v->dysum.swaMatric[i] * convert;
				break;
			case eSW_Week:
				val = v->wkavg.swaMatric[i] * convert;
				p = SW_Model.week-tOffset;
				break;
			case eSW_Month:
				val = v->moavg.swaMatric[i] * convert;
				p = SW_Model.month-tOffset;
				break;
			case eSW_Year:
				val = v->yravg.swaMatric[i] * convert;
				p = Globals.currYear - 1;
				break;
		}
		if (isPartialSoilwatOutput == FALSE)
		{
			int
				indl0 = Iylp(Globals.currYear - 1, i, p, pd, 0), // index for mean
				indl1 = Iylp(Globals.currYear - 1, i, p, pd, 1); // index for sd
			float old_val = SXW_AVG.swamatric_avg[indl0];

			SXW_AVG.swamatric_avg[indl0] = get_running_avg(SXW_AVG.swamatric_avg[indl0], val);
			SXW_AVG.swamatric_avg[indl1] += get_running_sqr(old_val, val, SXW_AVG.swamatric_avg[indl0]);

			if(Globals.currIter == Globals.runModelIterations){
				float std = sqrt(SXW_AVG.swamatric_avg[indl1] / Globals.currIter);

				sprintf(str, "%c%7.6f%c%7.6f", _Sep, SXW_AVG.swamatric_avg[indl0], _Sep, std);
				strcat(sw_outstr, str);
			}
		}
		if(storeAllIterations){
			sprintf(str_iters, "%c%7.6f", _Sep, val);
			strcat(outstr_all_iters, str_iters);
		}
	}
#else
	int delta;
	RealD *p;

	switch (pd)
	{
		case eSW_Day:
		delta = SW_Output[eSW_SWAMatric].dy_row;
		p = p_rOUT[eSW_SWAMatric][eSW_Day]; // set shorthand copy of 'p_rOUT' pointer
		p[delta + dy_nrow * 0] = SW_Model.simyear;
		p[delta + dy_nrow * 1] = SW_Model.doy;
		ForEachSoilLayer(i)
		{
			convert = 1. / (1. - SW_Site.lyr[i]->fractionVolBulk_gravel);
			p[delta + dy_nrow * (i + 2)] = v->dysum.swaMatric[i] * convert;
		}
		SW_Output[eSW_SWAMatric].dy_row++;
		break;
		case eSW_Week:
		delta = SW_Output[eSW_SWAMatric].wk_row;
		p = p_rOUT[eSW_SWAMatric][eSW_Week]; // set shorthand copy of 'p_rOUT' pointer
		p[delta + wk_nrow * 0] = SW_Model.simyear;
		p[delta + wk_nrow * 1] = (SW_Model.week + 1) - tOffset;
		ForEachSoilLayer(i)
		{
			convert = 1. / (1. - SW_Site.lyr[i]->fractionVolBulk_gravel);
			p[delta + wk_nrow * (i + 2)] = v->wkavg.swaMatric[i] * convert;
		}
		SW_Output[eSW_SWAMatric].wk_row++;
		break;
		case eSW_Month:
		delta = SW_Output[eSW_SWAMatric].mo_row;
		p = p_rOUT[eSW_SWAMatric][eSW_Month]; // set shorthand copy of 'p_rOUT' pointer
		p[delta + mo_nrow * 0] = SW_Model.simyear;
		p[delta + mo_nrow * 1] = (SW_Model.month + 1) - tOffset;
		ForEachSoilLayer(i)
		{
			convert = 1. / (1. - SW_Site.lyr[i]->fractionVolBulk_gravel);
			p[delta + mo_nrow * (i + 2)] = v->moavg.swaMatric[i] * convert;
		}
		SW_Output[eSW_SWAMatric].mo_row++;
		break;
		case eSW_Year:
		delta = SW_Output[eSW_SWAMatric].yr_row;
		p = p_rOUT[eSW_SWAMatric][eSW_Year]; // set shorthand copy of 'p_rOUT' pointer
		p[delta + yr_nrow * 0] = SW_Model.simyear;
		ForEachSoilLayer(i)
		{
			convert = 1. / (1. - SW_Site.lyr[i]->fractionVolBulk_gravel);
			p[delta + yr_nrow * (i + 1)] = v->yravg.swaMatric[i] * convert;
		}
		SW_Output[eSW_SWAMatric].yr_row++;
		break;
	}
#endif
}

void get_surfaceWater(OutPeriod pd)
{
	/* --------------------------------------------------- */
	SW_SOILWAT *v = &SW_Soilwat;
	#if !defined(STEPWAT) && !defined(RSOILWAT)
		char str[OUTSTRLEN];
		RealD val_surfacewater = SW_MISSING;
		get_outstrleader(pd);

	#elif defined(STEPWAT)
		char str[OUTSTRLEN];
		char str_iters[OUTSTRLEN];
		RealD val_surfacewater = SW_MISSING;
		TimeInt p = 0;
		if ((isPartialSoilwatOutput == FALSE && Globals.currIter == Globals.runModelIterations) || storeAllIterations)
		{
			get_outstrleader(pd);
		}
	#endif

	#if !defined(STEPWAT) && !defined(RSOILWAT)
	switch (pd)
	{
	case eSW_Day:
		val_surfacewater = v->dysum.surfaceWater;
		break;
	case eSW_Week:
		val_surfacewater = v->wkavg.surfaceWater;
		break;
	case eSW_Month:
		val_surfacewater = v->moavg.surfaceWater;
		break;
	case eSW_Year:
		val_surfacewater = v->yravg.surfaceWater;
		break;
	}
	sprintf(str, "%c%7.6f", _Sep, val_surfacewater);
	strcat(sw_outstr, str);

	#elif defined(STEPWAT)
		switch (pd)
		{
			case eSW_Day:
				p = SW_Model.doy-1;
				val_surfacewater = v->dysum.surfaceWater;
				break;
			case eSW_Week:
				val_surfacewater = v->wkavg.surfaceWater;
				p = SW_Model.week-tOffset;
				break;
			case eSW_Month:
				p = SW_Model.month-tOffset;
				val_surfacewater = v->moavg.surfaceWater;
				break;
			case eSW_Year:
				val_surfacewater = v->yravg.surfaceWater;
				p = Globals.currYear - 1;
				break;
		}

		if (isPartialSoilwatOutput == FALSE)
		{
			int
				ind0 = Iypc(Globals.currYear - 1, p, 0, pd), // index for mean
				ind1 = Iypc(Globals.currYear - 1, p, 1, pd); // index for sd
			float old_val = SXW_AVG.surfacewater_avg[ind0];

			SXW_AVG.surfacewater_avg[ind0] = get_running_avg(SXW_AVG.surfacewater_avg[ind0], val_surfacewater);
			SXW_AVG.surfacewater_avg[ind1] += get_running_sqr(old_val, val_surfacewater, SXW_AVG.surfacewater_avg[ind0]);

			if(Globals.currIter == Globals.runModelIterations){
				float std = sqrt(SXW_AVG.surfacewater_avg[ind1] / Globals.currIter);

				sprintf(str, "%c%7.6f%c%7.6f", _Sep, SXW_AVG.surfacewater_avg[ind0], _Sep, std);
				strcat(sw_outstr, str);
			}
		}
		if(storeAllIterations){
			sprintf(str_iters, "%c%7.6f", _Sep, val_surfacewater);
			strcat(outstr_all_iters, str_iters);
		}

#else
	int delta;
	RealD *p;

	switch (pd)
	{
		case eSW_Day:
		delta = SW_Output[eSW_SurfaceWater].dy_row;
		p = p_rOUT[eSW_SurfaceWater][eSW_Day]; // set shorthand copy of 'p_rOUT' pointer
		p[delta + dy_nrow * 0] = SW_Model.simyear;
		p[delta + dy_nrow * 1] = SW_Model.doy;
		p[delta + dy_nrow * 2] = v->dysum.surfaceWater;
		SW_Output[eSW_SurfaceWater].dy_row++;
		break;
		case eSW_Week:
		delta = SW_Output[eSW_SurfaceWater].wk_row;
		p = p_rOUT[eSW_SurfaceWater][eSW_Week]; // set shorthand copy of 'p_rOUT' pointer
		p[delta + wk_nrow * 0] = SW_Model.simyear;
		p[delta + wk_nrow * 1] = (SW_Model.week + 1) - tOffset;
		p[delta + wk_nrow * 2] = v->wkavg.surfaceWater;
		SW_Output[eSW_SurfaceWater].wk_row++;
		break;
		case eSW_Month:
		delta = SW_Output[eSW_SurfaceWater].mo_row;
		p = p_rOUT[eSW_SurfaceWater][eSW_Month]; // set shorthand copy of 'p_rOUT' pointer
		p[delta + mo_nrow * 0] = SW_Model.simyear;
		p[delta + mo_nrow * 1] = (SW_Model.month + 1) - tOffset;
		p[delta + mo_nrow * 2] = v->moavg.surfaceWater;
		SW_Output[eSW_SurfaceWater].mo_row++;
		break;
		case eSW_Year:
		delta = SW_Output[eSW_SurfaceWater].yr_row;
		p = p_rOUT[eSW_SurfaceWater][eSW_Year]; // set shorthand copy of 'p_rOUT' pointer
		p[delta + yr_nrow * 0] = SW_Model.simyear;
		p[delta + yr_nrow * 1] = v->yravg.surfaceWater;
		SW_Output[eSW_SurfaceWater].yr_row++;
		break;
	}
#endif
}

void get_runoffrunon(OutPeriod pd) {
  /* --------------------------------------------------- */
  /* (12/13/2012) (clk) Added function to output runoff variables */

  SW_WEATHER *w = &SW_Weather;
  RealD val_netRunoff = SW_MISSING, val_surfaceRunoff = SW_MISSING,
      val_surfaceRunon = SW_MISSING, val_snowRunoff = SW_MISSING;

	#if !defined(STEPWAT) && !defined(RSOILWAT)
		char str[OUTSTRLEN];
		get_outstrleader(pd);

	#elif defined(STEPWAT)
		char str[OUTSTRLEN];
		char str_iters[OUTSTRLEN];
		TimeInt p = 0;
		if ((isPartialSoilwatOutput == FALSE && Globals.currIter == Globals.runModelIterations) || storeAllIterations)
		{
			get_outstrleader(pd);
		}
	#endif

	#ifndef STEPWAT
	switch (pd)
	{
	case eSW_Day:
		val_surfaceRunoff = w->dysum.surfaceRunoff;
		val_surfaceRunon = w->dysum.surfaceRunon;
		val_snowRunoff = w->dysum.snowRunoff;
		break;
	case eSW_Week:
		val_surfaceRunoff = w->wkavg.surfaceRunoff;
		val_surfaceRunon = w->wkavg.surfaceRunon;
		val_snowRunoff = w->wkavg.snowRunoff;
		break;
	case eSW_Month:
		val_surfaceRunoff = w->moavg.surfaceRunoff;
		val_surfaceRunon = w->moavg.surfaceRunon;
		val_snowRunoff = w->moavg.snowRunoff;
		break;
	case eSW_Year:
		val_surfaceRunoff = w->yravg.surfaceRunoff;
		val_surfaceRunon = w->yravg.surfaceRunon;
		val_snowRunoff = w->yravg.snowRunoff;
		break;
	}
	val_netRunoff = val_surfaceRunoff + val_snowRunoff - val_surfaceRunon;
	#endif

	#if !defined(STEPWAT) && !defined(RSOILWAT)
	sprintf(str, "%c%7.6f%c%7.6f%c%7.6f%c%7.6f", _Sep, val_netRunoff,
      _Sep, val_surfaceRunoff, _Sep, val_snowRunoff, _Sep, val_surfaceRunon);
    strcat(sw_outstr, str);

	#elif defined(STEPWAT)
		switch (pd)
		{
			case eSW_Day:
				p = SW_Model.doy-1;
				val_surfaceRunoff = w->dysum.surfaceRunoff;
				val_surfaceRunon = w->dysum.surfaceRunon;
				val_snowRunoff = w->dysum.snowRunoff;
				break;
			case eSW_Week:
				val_surfaceRunoff = w->wkavg.surfaceRunoff;
				val_surfaceRunon = w->wkavg.surfaceRunon;
				val_snowRunoff = w->wkavg.snowRunoff;
				p = SW_Model.week-tOffset;
				break;
			case eSW_Month:
				p = SW_Model.month-tOffset;
				val_surfaceRunoff = w->moavg.surfaceRunoff;
				val_surfaceRunon = w->moavg.surfaceRunon;
				val_snowRunoff = w->moavg.snowRunoff;
				break;
			case eSW_Year:
				p = Globals.currYear-1;
				val_surfaceRunoff = w->yravg.surfaceRunoff;
				val_surfaceRunon = w->yravg.surfaceRunon;
				val_snowRunoff = w->yravg.snowRunoff;
				break;
		}
		val_netRunoff = val_surfaceRunoff + val_snowRunoff - val_surfaceRunon;

		if (isPartialSoilwatOutput == FALSE)
		{
			int
				ind0 = Iypc(Globals.currYear - 1, p, 0, pd), // index for mean
				ind1 = Iypc(Globals.currYear - 1, p, 1, pd); // index for sd
			float old_val_total = SXW_AVG.runoff_total_avg[ind0];
			float old_val_surface_runoff = SXW_AVG.surface_runoff_avg[ind0];
			float old_val_surface_runon = SXW_AVG.surface_runon_avg[ind0];
			float old_val_snow = SXW_AVG.runoff_snow_avg[ind0];

			SXW_AVG.runoff_total_avg[ind0] = get_running_avg(SXW_AVG.runoff_total_avg[ind0], val_netRunoff);
			SXW_AVG.runoff_total_avg[ind1] += get_running_sqr(old_val_total, val_netRunoff, SXW_AVG.runoff_total_avg[ind0]);

			SXW_AVG.surface_runoff_avg[ind0] = get_running_avg(SXW_AVG.surface_runoff_avg[ind0], val_surfaceRunoff);
			SXW_AVG.surface_runoff_avg[ind1] += get_running_sqr(old_val_surface_runoff, val_surfaceRunoff, SXW_AVG.surface_runoff_avg[ind0]);

			SXW_AVG.surface_runon_avg[ind0] = get_running_avg(SXW_AVG.surface_runon_avg[ind0], val_surfaceRunon);
			SXW_AVG.surface_runon_avg[ind1] += get_running_sqr(old_val_surface_runon, val_surfaceRunon, SXW_AVG.surface_runon_avg[ind0]);


			SXW_AVG.runoff_snow_avg[ind0] = get_running_avg(SXW_AVG.runoff_snow_avg[ind0], val_snowRunoff);
			SXW_AVG.runoff_snow_avg[ind1] += get_running_sqr(old_val_snow, val_snowRunoff, SXW_AVG.runoff_snow_avg[ind0]);

			if(Globals.currIter == Globals.runModelIterations){
				float std_total = sqrt(SXW_AVG.runoff_total_avg[ind1] / Globals.currIter);
				float std_surface_runoff = sqrt(SXW_AVG.surface_runoff_avg[ind1] / Globals.currIter);
				float std_surface_runon = sqrt(SXW_AVG.surface_runon_avg[ind1] / Globals.currIter);
				float std_snow = sqrt(SXW_AVG.runoff_snow_avg[ind1] / Globals.currIter);

				sprintf(str, "%c%7.6f%c%7.6f%c%7.6f%c%7.6f%c%7.6f%c%7.6f%c%7.6f%c%7.6f", _Sep, SXW_AVG.runoff_total_avg[ind0], _Sep, std_total,
								_Sep, SXW_AVG.surface_runoff_avg[ind0], _Sep, std_surface_runoff,
								_Sep, SXW_AVG.runoff_snow_avg[ind0], _Sep, std_snow,
								_Sep, SXW_AVG.surface_runon_avg[ind0], _Sep, std_surface_runon);
				strcat(sw_outstr, str);
			}
		}
		if(storeAllIterations){
			sprintf(str_iters, "%c%7.6f%c%7.6f%c%7.6f%c%7.6f", _Sep, val_netRunoff,
		      _Sep, val_surfaceRunoff, _Sep, val_snowRunoff, _Sep, val_surfaceRunon);
			strcat(outstr_all_iters, str_iters);
		}

#else
	int delta;
	RealD *p;
	switch (pd) {
      case eSW_Day:
        delta = SW_Output[eSW_Runoff].dy_row;
        p = p_rOUT[eSW_Runoff][eSW_Day]; // set shorthand copy of 'p_rOUT' pointer
        p[delta + dy_nrow * 0] = SW_Model.simyear;
        p[delta + dy_nrow * 1] = SW_Model.doy;
        p[delta + dy_nrow * 2] = val_netRunoff;
        p[delta + dy_nrow * 3] = val_surfaceRunoff;
        p[delta + dy_nrow * 4] = val_snowRunoff;
        p[delta + dy_nrow * 5] = val_surfaceRunon;
        SW_Output[eSW_Runoff].dy_row++;
        break;
      case eSW_Week:
        delta = SW_Output[eSW_Runoff].wk_row;
        p = p_rOUT[eSW_Runoff][eSW_Week]; // set shorthand copy of 'p_rOUT' pointer
        p[delta + wk_nrow * 0] = SW_Model.simyear;
        p[delta + wk_nrow * 1] = (SW_Model.week + 1) - tOffset;
        p[delta + wk_nrow * 2] = val_netRunoff;
        p[delta + wk_nrow * 3] = val_surfaceRunoff;
        p[delta + wk_nrow * 4] = val_snowRunoff;
        p[delta + wk_nrow * 5] = val_surfaceRunon;
        SW_Output[eSW_Runoff].wk_row++;
        break;
      case eSW_Month:
        delta = SW_Output[eSW_Runoff].mo_row;
        p = p_rOUT[eSW_Runoff][eSW_Month]; // set shorthand copy of 'p_rOUT' pointer
        p[delta + mo_nrow * 0] = SW_Model.simyear;
        p[delta + mo_nrow * 1] = (SW_Model.month + 1) - tOffset;
        p[delta + mo_nrow * 2] = val_netRunoff;
        p[delta + mo_nrow * 3] = val_surfaceRunoff;
        p[delta + mo_nrow * 4] = val_snowRunoff;
        p[delta + mo_nrow * 5] = val_surfaceRunon;
        SW_Output[eSW_Runoff].mo_row++;
        break;
      case eSW_Year:
        delta = SW_Output[eSW_Runoff].yr_row;
        p = p_rOUT[eSW_Runoff][eSW_Year]; // set shorthand copy of 'p_rOUT' pointer
        p[delta + yr_nrow * 0] = SW_Model.simyear;
        p[delta + yr_nrow * 1] = val_netRunoff;
        p[delta + yr_nrow * 2] = val_surfaceRunoff;
        p[delta + yr_nrow * 3] = val_snowRunoff;
        p[delta + yr_nrow * 4] = val_surfaceRunon;
        SW_Output[eSW_Runoff].yr_row++;
        break;
    }
  #endif
}

void get_transp(OutPeriod pd)
{
	/* --------------------------------------------------- */
	/* 10-May-02 (cwb) Added conditional code to interface
	 *           with STEPPE.
	 */
	LyrIndex i;
	SW_SOILWAT *v = &SW_Soilwat;
	RealF *val = (RealF *) malloc(sizeof(RealF) * SW_Site.n_layers); // changed val_total to val

	#if !defined(STEPWAT) && !defined(RSOILWAT)
	char str[OUTSTRLEN];
	get_outstrleader(pd);

	#elif defined(STEPWAT)
	char str[OUTSTRLEN];
	char str_iters[OUTSTRLEN];
	TimeInt p = 0;
	if ((isPartialSoilwatOutput == FALSE && Globals.currIter == Globals.runModelIterations) || storeAllIterations)
		get_outstrleader(pd);

	RealF *val_total = (RealF *) malloc(sizeof(RealF) * SW_Site.n_layers);
	RealF *val_tree = (RealF *) malloc(sizeof(RealF) * SW_Site.n_layers);
	RealF *val_forb = (RealF *) malloc(sizeof(RealF) * SW_Site.n_layers);
	RealF *val_grass = (RealF *) malloc(sizeof(RealF) * SW_Site.n_layers);
	RealF *val_shrub = (RealF *) malloc(sizeof(RealF) * SW_Site.n_layers);

	#endif
	ForEachSoilLayer(i){
		val[i] = 0;
		#ifdef STEPWAT
		val_total[i] = 0;
		val_tree[i] = 0;
		val_forb[i] = 0;
		val_grass[i] = 0;
		val_shrub[i] = 0;
		#endif
	}

#ifdef RSOILWAT
	switch (pd)
	{
		case eSW_Day:
		p_rOUT[eSW_Transp][eSW_Day][SW_Output[eSW_Transp].dy_row + dy_nrow * 0] = SW_Model.simyear;
		p_rOUT[eSW_Transp][eSW_Day][SW_Output[eSW_Transp].dy_row + dy_nrow * 1] = SW_Model.doy;
		break;
		case eSW_Week:
		p_rOUT[eSW_Transp][eSW_Week][SW_Output[eSW_Transp].wk_row + wk_nrow * 0] = SW_Model.simyear;
		p_rOUT[eSW_Transp][eSW_Week][SW_Output[eSW_Transp].wk_row + wk_nrow * 1] = (SW_Model.week + 1) - tOffset;
		break;
		case eSW_Month:
		p_rOUT[eSW_Transp][eSW_Month][SW_Output[eSW_Transp].mo_row + mo_nrow * 0] = SW_Model.simyear;
		p_rOUT[eSW_Transp][eSW_Month][SW_Output[eSW_Transp].mo_row + mo_nrow * 1] = (SW_Model.month + 1) - tOffset;
		break;
		case eSW_Year:
		p_rOUT[eSW_Transp][eSW_Year][SW_Output[eSW_Transp].yr_row + yr_nrow * 0] = SW_Model.simyear;
		break;
	}
#endif

#ifndef RSOILWAT
	get_outstrleader(pd);
	/* total transpiration */
	ForEachSoilLayer(i)
	{
		switch (pd)
		{
		case eSW_Day:
			val[i] = v->dysum.transp_total[i];
			break;
		case eSW_Week:
			val[i] = v->wkavg.transp_total[i];
			break;
		case eSW_Month:
			val[i] = v->moavg.transp_total[i];
			break;
		case eSW_Year:
			val[i] = v->yravg.transp_total[i];
			break;
		}
	}
#else
	switch (pd)
	{
		case eSW_Day:
		ForEachSoilLayer(i)
			p_rOUT[eSW_Transp][eSW_Day][SW_Output[eSW_Transp].dy_row + dy_nrow * (i + 2)] = v->dysum.transp_total[i];
		break;
		case eSW_Week:
		ForEachSoilLayer(i)
			p_rOUT[eSW_Transp][eSW_Week][SW_Output[eSW_Transp].wk_row + wk_nrow * (i + 2)] = v->wkavg.transp_total[i];
		break;
		case eSW_Month:
		ForEachSoilLayer(i)
			p_rOUT[eSW_Transp][eSW_Month][SW_Output[eSW_Transp].mo_row + mo_nrow * (i + 2)] = v->moavg.transp_total[i];
		break;
		case eSW_Year:
		ForEachSoilLayer(i)
			p_rOUT[eSW_Transp][eSW_Year][SW_Output[eSW_Transp].yr_row + yr_nrow * (i + 1)] = v->yravg.transp_total[i];
		break;
	}
#endif

#if !defined(STEPWAT) && !defined(RSOILWAT)
	ForEachSoilLayer(i)
	{
		sprintf(str, "%c%7.6f", _Sep, val[i]);
		strcat(sw_outstr, str);
	}
#elif defined(STEPWAT)
	ForEachSoilLayer(i)
	{
		val_total[i] = val[i];
	}
#endif

#ifndef RSOILWAT
	/* tree-component transpiration */ForEachSoilLayer(i)
	{
		switch (pd)
		{
		case eSW_Day:
			val[i] = v->dysum.transp[SW_TREES][i];
			break;
		case eSW_Week:
			val[i] = v->wkavg.transp[SW_TREES][i];
			break;
		case eSW_Month:
			val[i] = v->moavg.transp[SW_TREES][i];
			break;
		case eSW_Year:
			val[i] = v->yravg.transp[SW_TREES][i];
			break;
		}
	}
#else
	switch (pd)
	{
		case eSW_Day:
		ForEachSoilLayer(i)
		p_rOUT[eSW_Transp][eSW_Day][SW_Output[eSW_Transp].dy_row + dy_nrow * (i + 2) + (dy_nrow * SW_Site.n_layers * 1)] = v->dysum.transp[SW_TREES][i];
		break;
		case eSW_Week:
		ForEachSoilLayer(i)
		p_rOUT[eSW_Transp][eSW_Week][SW_Output[eSW_Transp].wk_row + wk_nrow * (i + 2) + (wk_nrow * SW_Site.n_layers * 1)] = v->wkavg.transp[SW_TREES][i];
		break;
		case eSW_Month:
		ForEachSoilLayer(i)
		p_rOUT[eSW_Transp][eSW_Month][SW_Output[eSW_Transp].mo_row + mo_nrow * (i + 2) + (mo_nrow * SW_Site.n_layers * 1)] = v->moavg.transp[SW_TREES][i];
		break;
		case eSW_Year:
		ForEachSoilLayer(i)
		p_rOUT[eSW_Transp][eSW_Year][SW_Output[eSW_Transp].yr_row + yr_nrow * (i + 1) + (yr_nrow * SW_Site.n_layers * 1)] = v->yravg.transp[SW_TREES][i];
		break;
	}
#endif

#if !defined(STEPWAT) && !defined(RSOILWAT)
	ForEachSoilLayer(i)
	{
		sprintf(str, "%c%7.6f", _Sep, val[i]);
		strcat(sw_outstr, str);
	}
#elif defined(STEPWAT)
ForEachSoilLayer(i)
{
	val_tree[i] = val[i];
}
#endif

#ifndef RSOILWAT
	/* shrub-component transpiration */ForEachSoilLayer(i)
	{
		switch (pd)
		{
		case eSW_Day:
			val[i] = v->dysum.transp[SW_SHRUB][i];
			break;
		case eSW_Week:
			val[i] = v->wkavg.transp[SW_SHRUB][i];
			break;
		case eSW_Month:
			val[i] = v->moavg.transp[SW_SHRUB][i];
			break;
		case eSW_Year:
			val[i] = v->yravg.transp[SW_SHRUB][i];
			break;
		}
	}
#else
	switch (pd)
	{
		case eSW_Day:
		ForEachSoilLayer(i)
		p_rOUT[eSW_Transp][eSW_Day][SW_Output[eSW_Transp].dy_row + dy_nrow * (i + 2) + (dy_nrow * SW_Site.n_layers * 2)] = v->dysum.transp[SW_SHRUB][i];
		break;
		case eSW_Week:
		ForEachSoilLayer(i)
		p_rOUT[eSW_Transp][eSW_Week][SW_Output[eSW_Transp].wk_row + wk_nrow * (i + 2) + (wk_nrow * SW_Site.n_layers * 2)] = v->wkavg.transp[SW_SHRUB][i];
		break;
		case eSW_Month:
		ForEachSoilLayer(i)
		p_rOUT[eSW_Transp][eSW_Month][SW_Output[eSW_Transp].mo_row + mo_nrow * (i + 2) + (mo_nrow * SW_Site.n_layers * 2)] = v->moavg.transp[SW_SHRUB][i];
		break;
		case eSW_Year:
		ForEachSoilLayer(i)
		p_rOUT[eSW_Transp][eSW_Year][SW_Output[eSW_Transp].yr_row + yr_nrow * (i + 1) + (yr_nrow * SW_Site.n_layers * 2)] = v->yravg.transp[SW_SHRUB][i];
		break;
	}
#endif

#if !defined(STEPWAT) && !defined(RSOILWAT)
	ForEachSoilLayer(i)
	{
		sprintf(str, "%c%7.6f", _Sep, val[i]);
		strcat(sw_outstr, str);
	}
#elif defined(STEPWAT)
ForEachSoilLayer(i)
{
	val_shrub[i] = val[i];
}
#endif

#ifndef RSOILWAT
	/* forb-component transpiration */ForEachSoilLayer(i)
	{
		switch (pd)
		{
		case eSW_Day:
			val[i] = v->dysum.transp[SW_FORBS][i];
			break;
		case eSW_Week:
			val[i] = v->wkavg.transp[SW_FORBS][i];
			break;
		case eSW_Month:
			val[i] = v->moavg.transp[SW_FORBS][i];
			break;
		case eSW_Year:
			val[i] = v->yravg.transp[SW_FORBS][i];
			break;
		}
	}
#else
	switch (pd)
	{
		case eSW_Day:
		ForEachSoilLayer(i)
		p_rOUT[eSW_Transp][eSW_Day][SW_Output[eSW_Transp].dy_row + dy_nrow * (i + 2) + (dy_nrow * SW_Site.n_layers * 3)] = v->dysum.transp[SW_FORBS][i];
		break;
		case eSW_Week:
		ForEachSoilLayer(i)
		p_rOUT[eSW_Transp][eSW_Week][SW_Output[eSW_Transp].wk_row + wk_nrow * (i + 2) + (wk_nrow * SW_Site.n_layers * 3)] = v->wkavg.transp[SW_FORBS][i];
		break;
		case eSW_Month:
		ForEachSoilLayer(i)
		p_rOUT[eSW_Transp][eSW_Month][SW_Output[eSW_Transp].mo_row + mo_nrow * (i + 2) + (mo_nrow * SW_Site.n_layers * 3)] = v->moavg.transp[SW_FORBS][i];
		break;
		case eSW_Year:
		ForEachSoilLayer(i)
		p_rOUT[eSW_Transp][eSW_Year][SW_Output[eSW_Transp].yr_row + yr_nrow * (i + 1) + (yr_nrow * SW_Site.n_layers * 3)] = v->yravg.transp[SW_FORBS][i];
		break;
	}
#endif

#if !defined(STEPWAT) && !defined(RSOILWAT)
	ForEachSoilLayer(i)
	{
		sprintf(str, "%c%7.6f", _Sep, val[i]);
		strcat(sw_outstr, str);
	}
#elif defined(STEPWAT)
ForEachSoilLayer(i)
{
	val_forb[i] = val[i];
}
#endif

#ifndef RSOILWAT
	/* grass-component transpiration */
	ForEachSoilLayer(i)
	{
		switch (pd)
		{
		case eSW_Day:
			val[i] = v->dysum.transp[SW_GRASS][i];
			break;
		case eSW_Week:
			val[i] = v->wkavg.transp[SW_GRASS][i];
			break;
		case eSW_Month:
			val[i] = v->moavg.transp[SW_GRASS][i];
			break;
		case eSW_Year:
			val[i] = v->yravg.transp[SW_GRASS][i];
			break;
		}
	}
#else
	switch (pd)
	{
		case eSW_Day:
		ForEachSoilLayer(i)
		p_rOUT[eSW_Transp][eSW_Day][SW_Output[eSW_Transp].dy_row + dy_nrow * (i + 2) + (dy_nrow * SW_Site.n_layers * 4)] = v->dysum.transp[SW_GRASS][i];
		SW_Output[eSW_Transp].dy_row++;
		break;
		case eSW_Week:
		ForEachSoilLayer(i)
		p_rOUT[eSW_Transp][eSW_Week][SW_Output[eSW_Transp].wk_row + wk_nrow * (i + 2) + (wk_nrow * SW_Site.n_layers * 4)] = v->wkavg.transp[SW_GRASS][i];
		SW_Output[eSW_Transp].wk_row++;
		break;
		case eSW_Month:
		ForEachSoilLayer(i)
		p_rOUT[eSW_Transp][eSW_Month][SW_Output[eSW_Transp].mo_row + mo_nrow * (i + 2) + (mo_nrow * SW_Site.n_layers * 4)] = v->moavg.transp[SW_GRASS][i];
		SW_Output[eSW_Transp].mo_row++;
		break;
		case eSW_Year:
		ForEachSoilLayer(i)
		p_rOUT[eSW_Transp][eSW_Year][SW_Output[eSW_Transp].yr_row + yr_nrow * (i + 1) + (yr_nrow * SW_Site.n_layers * 4)] = v->yravg.transp[SW_GRASS][i];
		SW_Output[eSW_Transp].yr_row++;
		break;
	}
#endif

#if !defined(STEPWAT) && !defined(RSOILWAT)
	ForEachSoilLayer(i)
	{
		sprintf(str, "%c%7.6f", _Sep, val[i]);
		strcat(sw_outstr, str);
	}
#elif defined(STEPWAT)
ForEachSoilLayer(i)
{
	val_grass[i] = val[i];
}
#endif

#if defined(STEPWAT)
	switch (pd)
	{
		case eSW_Day:
			p = SW_Model.doy - 1;
			break; /* print current but as index */
		case eSW_Week:
			p = SW_Model.week - tOffset;
			break; /* print previous to current */
		case eSW_Month:
			p = SW_Model.month - tOffset;
			break; /* print previous to current */
		case eSW_Year:
			p = 0; // Iypc requires 0 for yearly timeperiod
			break;
	}
	if (bFlush_output) p++;

  ForEachSoilLayer(i)
  {
  /* Pass monthly transpiration values to STEPWAT2 as resources: the
     function `_transp_contribution_by_group` deals with these monthly x layer
     values */
  if (pd == eSW_Month) {
    SXW.transpTotal[Ilp(i,p)] = val_total[i];
    SXW.transpTrees[Ilp(i,p)] = val_tree[i];
    SXW.transpShrubs[Ilp(i,p)] = val_shrub[i];
    SXW.transpForbs[Ilp(i,p)] = val_forb[i];
    SXW.transpGrasses[Ilp(i,p)] = val_grass[i];

    //printf("Tshrubs: bFlush_output=%d pd=%d t=%d lyr=%d ilp=%d T=%.3f\n", bFlush_output, pd, p, i, Ilp(i,p), SXW.transpShrubs[Ilp(i,p)]);
  }

	if (isPartialSoilwatOutput == FALSE)
	{
		int
			indl0 = Iylp(Globals.currYear - 1, i, p, pd, 0), // index for mean
			indl1 = Iylp(Globals.currYear - 1, i, p, pd, 1); // index for sd
		float old_total = SXW.transpTotal_avg[indl0];
		float old_tree = SXW.transpTrees_avg[indl0];
		float old_shrub = SXW.transpShrubs_avg[indl0];
		float old_forb = SXW.transpForbs_avg[indl0];
		float old_grass = SXW.transpGrasses_avg[indl0];

		// for the average over iteration we need to include year too so values are not overlapping.
		// [Iylp(Globals.currYear-1,i,p)] is a new macro defined in sxw.h that represents year, layer, timeperiod
		SXW.transpTotal_avg[indl0] = get_running_avg(SXW.transpTotal_avg[indl0], SXW.transpTotal[Ilp(i,p)]);
		SXW.transpTrees_avg[indl0] = get_running_avg(SXW.transpTrees_avg[indl0], SXW.transpTrees[Ilp(i,p)]);
		SXW.transpShrubs_avg[indl0] = get_running_avg(SXW.transpShrubs_avg[indl0], SXW.transpShrubs[Ilp(i,p)]);
		SXW.transpForbs_avg[indl0] = get_running_avg(SXW.transpForbs_avg[indl0], SXW.transpForbs[Ilp(i,p)]);
		SXW.transpGrasses_avg[indl0] = get_running_avg(SXW.transpGrasses_avg[indl0], SXW.transpGrasses[Ilp(i,p)]);

		SXW.transpTotal_avg[indl1] = get_running_sqr(old_total, SXW.transpTotal[Ilp(i,p)], SXW.transpTotal_avg[indl0]);
		SXW.transpTrees_avg[indl1] = get_running_sqr(old_tree, SXW.transpTrees[Ilp(i,p)], SXW.transpTrees_avg[indl0]);
		SXW.transpShrubs_avg[indl1] = get_running_sqr(old_shrub, SXW.transpShrubs[Ilp(i,p)], SXW.transpShrubs_avg[indl0]);
		SXW.transpForbs_avg[indl1] = get_running_sqr(old_forb, SXW.transpForbs[Ilp(i,p)], SXW.transpForbs_avg[indl0]);
		SXW.transpGrasses_avg[indl1] = get_running_sqr(old_grass, SXW.transpGrasses[Ilp(i,p)], SXW.transpGrasses_avg[indl0]);

		// if last iteration need to divide by number of iterations to get average over all iterations
		if(Globals.currIter == Globals.runModelIterations){
			float std_total = sqrt(SXW.transpTotal_avg[indl1] / Globals.currIter);
			float std_trees = sqrt(SXW.transpTrees_avg[indl1] / Globals.currIter);
			float std_shrubs = sqrt(SXW.transpShrubs_avg[indl1] / Globals.currIter);
			float std_forbs = sqrt(SXW.transpForbs_avg[indl1] / Globals.currIter);
			float std_grasses = sqrt(SXW.transpGrasses_avg[indl1] / Globals.currIter);

			sprintf(str, "%c%7.6f%c%7.6f%c%7.6f%c%7.6f%c%7.6f%c%7.6f%c%7.6f%c%7.6f%c%7.6f%c%7.6f",
				_Sep, SXW.transpTotal_avg[indl0], _Sep, std_total, _Sep, SXW.transpTrees_avg[indl0], _Sep,
				std_trees, _Sep, SXW.transpShrubs_avg[indl0], _Sep, std_shrubs, _Sep, SXW.transpForbs_avg[indl0],
				_Sep, std_forbs, _Sep, SXW.transpGrasses_avg[indl0], _Sep, std_grasses);
			strcat(sw_outstr, str);
		}
	}

	if(storeAllIterations){
		sprintf(str_iters, "%c%7.6f%c%7.6f%c%7.6f%c%7.6f%c%7.6f",
			_Sep, val_total[i], _Sep, val_tree[i], _Sep, val_shrub[i], _Sep, val_forb[i], _Sep, val_grass[i]);
		strcat(outstr_all_iters, str_iters);
	}
}
free(val_total);
free(val_tree);
free(val_forb);
free(val_grass);
free(val_shrub);
#endif
	free(val);
}


void get_evapSoil(OutPeriod pd)
{
	/* --------------------------------------------------- */
	LyrIndex i;
	SW_SOILWAT *v = &SW_Soilwat;

	#if !defined(STEPWAT) && !defined(RSOILWAT)
		char str[OUTSTRLEN];
		RealD val = SW_MISSING;
		get_outstrleader(pd);

	#elif defined(STEPWAT)
		char str[OUTSTRLEN];
		char str_iters[OUTSTRLEN];
		RealD val = SW_MISSING;
		TimeInt p = 0;
		if ((isPartialSoilwatOutput == FALSE && Globals.currIter == Globals.runModelIterations) || storeAllIterations)
		{
			get_outstrleader(pd);
		}
	#endif

	#if !defined(STEPWAT) && !defined(RSOILWAT)
	ForEachEvapLayer(i)
	{
		switch (pd)
		{
		case eSW_Day:
			val = v->dysum.evap[i];
			break;
		case eSW_Week:
			val = v->wkavg.evap[i];
			break;
		case eSW_Month:
			val = v->moavg.evap[i];
			break;
		case eSW_Year:
			val = v->yravg.evap[i];
			break;
		}
		sprintf(str, "%c%7.6f", _Sep, val);
		strcat(sw_outstr, str);
	}

	#elif defined(STEPWAT)
	ForEachEvapLayer(i)
	{
		switch (pd)
		{
			case eSW_Day:
				p = SW_Model.doy-1;
				val = v->dysum.evap[i];
				break;
			case eSW_Week:
				val = v->wkavg.evap[i];
				p = SW_Model.week-tOffset;
				break;
			case eSW_Month:
				val = v->moavg.evap[i];
				p = SW_Model.month-tOffset;
				break;
			case eSW_Year:
				val = v->yravg.evap[i];
				p = Globals.currYear - 1;
				break;
		}
		if (isPartialSoilwatOutput == FALSE)
		{
			int
				indl0 = Iylp(Globals.currYear - 1, i, p, pd, 0), // index for mean
				indl1 = Iylp(Globals.currYear - 1, i, p, pd, 1); // index for sd
			float old_val = SXW_AVG.evapsoil_avg[indl0];

			SXW_AVG.evapsoil_avg[indl0] = get_running_avg(SXW_AVG.evapsoil_avg[indl0], val);
			SXW_AVG.evapsoil_avg[indl1] += get_running_sqr(old_val, val, SXW_AVG.evapsoil_avg[indl0]);

			if(Globals.currIter == Globals.runModelIterations){
				float std = sqrt(SXW_AVG.evapsoil_avg[indl1] / Globals.currIter);

				sprintf(str, "%c%7.6f%c%7.6f", _Sep, SXW_AVG.evapsoil_avg[indl0], _Sep, std);
				strcat(sw_outstr, str);
			}
		}
		if(storeAllIterations){
			sprintf(str_iters, "%c%7.6f", _Sep, val);
			strcat(outstr_all_iters, str_iters);
		}
	}

#else
	switch (pd)
	{
		case eSW_Day:
		p_rOUT[eSW_EvapSoil][eSW_Day][SW_Output[eSW_EvapSoil].dy_row + dy_nrow * 0] = SW_Model.simyear;
		p_rOUT[eSW_EvapSoil][eSW_Day][SW_Output[eSW_EvapSoil].dy_row + dy_nrow * 1] = SW_Model.doy;
		ForEachEvapLayer(i)
			p_rOUT[eSW_EvapSoil][eSW_Day][SW_Output[eSW_EvapSoil].dy_row + dy_nrow * (i + 2)] = v->dysum.evap[i];
		SW_Output[eSW_EvapSoil].dy_row++;
		break;
		case eSW_Week:
		p_rOUT[eSW_EvapSoil][eSW_Week][SW_Output[eSW_EvapSoil].wk_row + wk_nrow * 0] = SW_Model.simyear;
		p_rOUT[eSW_EvapSoil][eSW_Week][SW_Output[eSW_EvapSoil].wk_row + wk_nrow * 1] = (SW_Model.week + 1) - tOffset;
		ForEachEvapLayer(i)
			p_rOUT[eSW_EvapSoil][eSW_Week][SW_Output[eSW_EvapSoil].wk_row + wk_nrow * (i + 2)] = v->wkavg.evap[i];
		SW_Output[eSW_EvapSoil].wk_row++;
		break;
		case eSW_Month:
		p_rOUT[eSW_EvapSoil][eSW_Month][SW_Output[eSW_EvapSoil].mo_row + mo_nrow * 0] = SW_Model.simyear;
		p_rOUT[eSW_EvapSoil][eSW_Month][SW_Output[eSW_EvapSoil].mo_row + mo_nrow * 1] = (SW_Model.month + 1) - tOffset;
		ForEachEvapLayer(i)
			p_rOUT[eSW_EvapSoil][eSW_Month][SW_Output[eSW_EvapSoil].mo_row + mo_nrow * (i + 2)] = v->moavg.evap[i];
		SW_Output[eSW_EvapSoil].mo_row++;
		break;
		case eSW_Year:
		p_rOUT[eSW_EvapSoil][eSW_Year][SW_Output[eSW_EvapSoil].yr_row + yr_nrow * 0] = SW_Model.simyear;
		ForEachEvapLayer(i)
			p_rOUT[eSW_EvapSoil][eSW_Year][SW_Output[eSW_EvapSoil].yr_row + yr_nrow * (i + 1)] = v->yravg.evap[i];
		SW_Output[eSW_EvapSoil].yr_row++;
		break;
	}
#endif
}

void get_evapSurface(OutPeriod pd)
{
	/* --------------------------------------------------- */
	SW_SOILWAT *v = &SW_Soilwat;

	#if !defined(STEPWAT) && !defined(RSOILWAT)
		char str[OUTSTRLEN];
		RealD val_tot = SW_MISSING, val_tree = SW_MISSING, val_forb = SW_MISSING,
				val_shrub = SW_MISSING, val_grass = SW_MISSING, val_litter =
						SW_MISSING, val_water = SW_MISSING;
		get_outstrleader(pd);

	#elif defined(STEPWAT)
		char str[OUTSTRLEN];
		char str_iters[OUTSTRLEN];
		RealD val_tot = SW_MISSING, val_tree = SW_MISSING, val_forb = SW_MISSING,
				val_shrub = SW_MISSING, val_grass = SW_MISSING, val_litter =
						SW_MISSING, val_water = SW_MISSING;
		TimeInt p = 0;
		if ((isPartialSoilwatOutput == FALSE && Globals.currIter == Globals.runModelIterations) || storeAllIterations)
		{
			get_outstrleader(pd);
		}
	#endif

	#if !defined(STEPWAT) && !defined(RSOILWAT)
	switch (pd)
	{
	case eSW_Day:
		val_tot = v->dysum.total_evap;
		val_tree = v->dysum.evap_veg[SW_TREES];
		val_forb = v->dysum.evap_veg[SW_FORBS];
		val_shrub = v->dysum.evap_veg[SW_SHRUB];
		val_grass = v->dysum.evap_veg[SW_GRASS];
		val_litter = v->dysum.litter_evap;
		val_water = v->dysum.surfaceWater_evap;
		break;
	case eSW_Week:
		val_tot = v->wkavg.total_evap;
		val_tree = v->wkavg.evap_veg[SW_TREES];
		val_forb = v->wkavg.evap_veg[SW_FORBS];
		val_shrub = v->wkavg.evap_veg[SW_SHRUB];
		val_grass = v->wkavg.evap_veg[SW_GRASS];
		val_litter = v->wkavg.litter_evap;
		val_water = v->wkavg.surfaceWater_evap;
		break;
	case eSW_Month:
		val_tot = v->moavg.total_evap;
		val_tree = v->moavg.evap_veg[SW_TREES];
		val_forb = v->moavg.evap_veg[SW_FORBS];
		val_shrub = v->moavg.evap_veg[SW_SHRUB];
		val_grass = v->moavg.evap_veg[SW_GRASS];
		val_litter = v->moavg.litter_evap;
		val_water = v->moavg.surfaceWater_evap;
		break;
	case eSW_Year:
		val_tot = v->yravg.total_evap;
		val_tree = v->yravg.evap_veg[SW_TREES];
		val_forb = v->yravg.evap_veg[SW_FORBS];
		val_shrub = v->yravg.evap_veg[SW_SHRUB];
		val_grass = v->yravg.evap_veg[SW_GRASS];
		val_litter = v->yravg.litter_evap;
		val_water = v->yravg.surfaceWater_evap;
		break;
	}
	sprintf(str, "%c%7.6f%c%7.6f%c%7.6f%c%7.6f%c%7.6f%c%7.6f%c%7.6f",
		_Sep, val_tot, _Sep, val_tree, _Sep, val_shrub, _Sep, val_forb, _Sep, val_grass, _Sep, val_litter, _Sep, val_water);
	strcat(sw_outstr, str);

#elif defined(STEPWAT)
		switch (pd)
		{
			case eSW_Day:
				p = SW_Model.doy-1;
				val_tot = v->dysum.total_evap;
				val_tree = v->dysum.evap_veg[SW_TREES];
				val_forb = v->dysum.evap_veg[SW_FORBS];
				val_shrub = v->dysum.evap_veg[SW_SHRUB];
				val_grass = v->dysum.evap_veg[SW_GRASS];
				val_litter = v->dysum.litter_evap;
				val_water = v->dysum.surfaceWater_evap;
				break;
			case eSW_Week:
				p = SW_Model.week-tOffset;
				val_tot = v->wkavg.total_evap;
				val_tree = v->wkavg.evap_veg[SW_TREES];
				val_forb = v->wkavg.evap_veg[SW_FORBS];
				val_shrub = v->wkavg.evap_veg[SW_SHRUB];
				val_grass = v->wkavg.evap_veg[SW_GRASS];
				val_litter = v->wkavg.litter_evap;
				val_water = v->wkavg.surfaceWater_evap;
				break;
			case eSW_Month:
				p = SW_Model.month-tOffset;
				val_tot = v->moavg.total_evap;
				val_tree = v->moavg.evap_veg[SW_TREES];
				val_forb = v->moavg.evap_veg[SW_FORBS];
				val_shrub = v->moavg.evap_veg[SW_SHRUB];
				val_grass = v->moavg.evap_veg[SW_GRASS];
				val_litter = v->moavg.litter_evap;
				val_water = v->moavg.surfaceWater_evap;
				break;
			case eSW_Year:
				p = Globals.currYear - 1;
				val_tot = v->yravg.total_evap;
				val_tree = v->yravg.evap_veg[SW_TREES];
				val_forb = v->yravg.evap_veg[SW_FORBS];
				val_shrub = v->yravg.evap_veg[SW_SHRUB];
				val_grass = v->yravg.evap_veg[SW_GRASS];
				val_litter = v->yravg.litter_evap;
				val_water = v->yravg.surfaceWater_evap;
				break;
		}

		if (isPartialSoilwatOutput == FALSE)
		{
			int
				ind0 = Iypc(Globals.currYear - 1, p, 0, pd), // index for mean
				ind1 = Iypc(Globals.currYear - 1, p, 1, pd); // index for sd
			float old_val_total = SXW_AVG.evapsurface_total_avg[ind0];
			float old_val_tree = SXW_AVG.evapsurface_tree_avg[ind0];
			float old_val_forb = SXW_AVG.evapsurface_forb_avg[ind0];
			float old_val_shrub = SXW_AVG.evapsurface_shrub_avg[ind0];
			float old_val_grass = SXW_AVG.evapsurface_grass_avg[ind0];
			float old_val_litter = SXW_AVG.evapsurface_litter_avg[ind0];
			float old_val_water = SXW_AVG.evapsurface_water_avg[ind0];


			SXW_AVG.evapsurface_total_avg[ind0] = get_running_avg(SXW_AVG.evapsurface_total_avg[ind0], val_tot);
			SXW_AVG.evapsurface_total_avg[ind1] += get_running_sqr(old_val_total, val_tot, SXW_AVG.evapsurface_total_avg[ind0]);

			SXW_AVG.evapsurface_tree_avg[ind0] = get_running_avg(SXW_AVG.evapsurface_tree_avg[ind0], val_tree);
			SXW_AVG.evapsurface_tree_avg[ind1] += get_running_sqr(old_val_tree, val_tree, SXW_AVG.evapsurface_tree_avg[ind0]);

			SXW_AVG.evapsurface_forb_avg[ind0] = get_running_avg(SXW_AVG.evapsurface_forb_avg[ind0], val_forb);
			SXW_AVG.evapsurface_forb_avg[ind1] += get_running_sqr(old_val_forb, val_forb, SXW_AVG.evapsurface_forb_avg[ind0]);

			SXW_AVG.evapsurface_shrub_avg[ind0] = get_running_avg(SXW_AVG.evapsurface_shrub_avg[ind0], val_shrub);
			SXW_AVG.evapsurface_shrub_avg[ind1] += get_running_sqr(old_val_shrub, val_shrub, SXW_AVG.evapsurface_shrub_avg[ind0]);

			SXW_AVG.evapsurface_grass_avg[ind0] = get_running_avg(SXW_AVG.evapsurface_grass_avg[ind0], val_grass);
			SXW_AVG.evapsurface_grass_avg[ind1] += get_running_sqr(old_val_grass, val_grass, SXW_AVG.evapsurface_grass_avg[ind0]);

			SXW_AVG.evapsurface_litter_avg[ind0] = get_running_avg(SXW_AVG.evapsurface_litter_avg[ind0], val_litter);
			SXW_AVG.evapsurface_litter_avg[ind1] += get_running_sqr(old_val_litter, val_litter, SXW_AVG.evapsurface_litter_avg[ind0]);

			SXW_AVG.evapsurface_water_avg[ind0] = get_running_avg(SXW_AVG.evapsurface_water_avg[ind0], val_water);
			SXW_AVG.evapsurface_water_avg[ind1] += get_running_sqr(old_val_water, val_water, SXW_AVG.evapsurface_water_avg[ind0]);

			if(Globals.currIter == Globals.runModelIterations){
				float std_total = sqrt(SXW_AVG.evapsurface_total_avg[ind1] / Globals.currIter);
				float std_tree = sqrt(SXW_AVG.evapsurface_tree_avg[ind1] / Globals.currIter);
				float std_forb = sqrt(SXW_AVG.evapsurface_forb_avg[ind1] / Globals.currIter);
				float std_shrub = sqrt(SXW_AVG.evapsurface_shrub_avg[ind1] / Globals.currIter);
				float std_grass = sqrt(SXW_AVG.evapsurface_grass_avg[ind1] / Globals.currIter);
				float std_litter = sqrt(SXW_AVG.evapsurface_litter_avg[ind1] / Globals.currIter);
				float std_water = sqrt(SXW_AVG.evapsurface_water_avg[ind1] / Globals.currIter);

				sprintf(str, "%c%7.6f%c%7.6f%c%7.6f%c%7.6f%c%7.6f%c%7.6f%c%7.6f%c%7.6f%c%7.6f%c%7.6f%c%7.6f%c%7.6f%c%7.6f%c%7.6f",
				 				_Sep, SXW_AVG.evapsurface_total_avg[ind0], _Sep, std_total,
								_Sep, SXW_AVG.evapsurface_tree_avg[ind0], _Sep, std_tree,
								_Sep, SXW_AVG.evapsurface_shrub_avg[ind0], _Sep, std_shrub,
								_Sep, SXW_AVG.evapsurface_forb_avg[ind0], _Sep, std_forb,
								_Sep, SXW_AVG.evapsurface_grass_avg[ind0], _Sep, std_grass,
								_Sep, SXW_AVG.evapsurface_litter_avg[ind0], _Sep, std_litter,
								_Sep, SXW_AVG.evapsurface_water_avg[ind0], _Sep, std_water);
				strcat(sw_outstr, str);
			}
		}
		if(storeAllIterations){
			sprintf(str_iters, "%c%7.6f%c%7.6f%c%7.6f%c%7.6f%c%7.6f%c%7.6f%c%7.6f",
				_Sep, val_tot, _Sep, val_tree, _Sep, val_shrub, _Sep, val_forb, _Sep, val_grass, _Sep, val_litter, _Sep, val_water);
			strcat(outstr_all_iters, str_iters);
		}

#else
	int delta;
	RealD *p;

	switch (pd)
	{
		case eSW_Day:
		delta = SW_Output[eSW_EvapSurface].dy_row;
		p = p_rOUT[eSW_EvapSurface][eSW_Day]; // set shorthand copy of 'p_rOUT' pointer
		p[delta + dy_nrow * 0] = SW_Model.simyear;
		p[delta + dy_nrow * 1] = SW_Model.doy;
		p[delta + dy_nrow * 2] = v->dysum.total_evap;
		p[delta + dy_nrow * 3] = v->dysum.evap_veg[SW_TREES];
		p[delta + dy_nrow * 4] = v->dysum.evap_veg[SW_SHRUB];
		p[delta + dy_nrow * 5] = v->dysum.evap_veg[SW_FORBS];
		p[delta + dy_nrow * 6] = v->dysum.evap_veg[SW_GRASS];
		p[delta + dy_nrow * 7] = v->dysum.litter_evap;
		p[delta + dy_nrow * 8] = v->dysum.surfaceWater_evap;
		SW_Output[eSW_EvapSurface].dy_row++;
		break;
		case eSW_Week:
		delta = SW_Output[eSW_EvapSurface].wk_row;
		p = p_rOUT[eSW_EvapSurface][eSW_Week]; // set shorthand copy of 'p_rOUT' pointer
		p[delta + wk_nrow * 0] = SW_Model.simyear;
		p[delta + wk_nrow * 1] = (SW_Model.week + 1) - tOffset;
		p[delta + wk_nrow * 2] = v->wkavg.total_evap;
		p[delta + wk_nrow * 3] = v->wkavg.evap_veg[SW_TREES];
		p[delta + wk_nrow * 4] = v->wkavg.evap_veg[SW_SHRUB];
		p[delta + wk_nrow * 5] = v->wkavg.evap_veg[SW_FORBS];
		p[delta + wk_nrow * 6] = v->wkavg.evap_veg[SW_GRASS];
		p[delta + wk_nrow * 7] = v->wkavg.litter_evap;
		p[delta + wk_nrow * 8] = v->wkavg.surfaceWater_evap;
		SW_Output[eSW_EvapSurface].wk_row++;
		break;
		case eSW_Month:
		delta = SW_Output[eSW_EvapSurface].mo_row;
		p = p_rOUT[eSW_EvapSurface][eSW_Month]; // set shorthand copy of 'p_rOUT' pointer
		p[delta + mo_nrow * 0] = SW_Model.simyear;
		p[delta + mo_nrow * 1] = (SW_Model.month + 1) - tOffset;
		p[delta + mo_nrow * 2] = v->moavg.total_evap;
		p[delta + mo_nrow * 3] = v->moavg.evap_veg[SW_TREES];
		p[delta + mo_nrow * 4] = v->moavg.evap_veg[SW_SHRUB];
		p[delta + mo_nrow * 5] = v->moavg.evap_veg[SW_FORBS];
		p[delta + mo_nrow * 6] = v->moavg.evap_veg[SW_GRASS];
		p[delta + mo_nrow * 7] = v->moavg.litter_evap;
		p[delta + mo_nrow * 8] = v->moavg.surfaceWater_evap;
		SW_Output[eSW_EvapSurface].mo_row++;
		break;
		case eSW_Year:
		delta = SW_Output[eSW_EvapSurface].yr_row;
		p = p_rOUT[eSW_EvapSurface][eSW_Year]; // set shorthand copy of 'p_rOUT' pointer
		p[delta + yr_nrow * 0] = SW_Model.simyear;
		p[delta + yr_nrow * 1] = v->yravg.total_evap;
		p[delta + yr_nrow * 2] = v->yravg.evap_veg[SW_TREES];
		p[delta + yr_nrow * 3] = v->yravg.evap_veg[SW_SHRUB];
		p[delta + yr_nrow * 4] = v->yravg.evap_veg[SW_FORBS];
		p[delta + yr_nrow * 5] = v->yravg.evap_veg[SW_GRASS];
		p[delta + yr_nrow * 6] = v->yravg.litter_evap;
		p[delta + yr_nrow * 7] = v->yravg.surfaceWater_evap;
		SW_Output[eSW_EvapSurface].yr_row++;
		break;
	}
#endif
}

void get_interception(OutPeriod pd)
{
	/* --------------------------------------------------- */
	SW_SOILWAT *v = &SW_Soilwat;

	#if !defined(STEPWAT) && !defined(RSOILWAT)
		char str[OUTSTRLEN];
		RealD val_tot = SW_MISSING, val_tree = SW_MISSING, val_forb = SW_MISSING,
				val_shrub = SW_MISSING, val_grass = SW_MISSING, val_litter =
						SW_MISSING;
		get_outstrleader(pd);

	#elif defined(STEPWAT)
		char str[OUTSTRLEN];
		char str_iters[OUTSTRLEN];
		RealD val_tot = SW_MISSING, val_tree = SW_MISSING, val_forb = SW_MISSING,
				val_shrub = SW_MISSING, val_grass = SW_MISSING, val_litter =
						SW_MISSING;
		TimeInt p = 0;
		if ((isPartialSoilwatOutput == FALSE && Globals.currIter == Globals.runModelIterations) || storeAllIterations)
		{
			get_outstrleader(pd);
		}
	#endif

	#if !defined(STEPWAT) && !defined(RSOILWAT)
	switch (pd)
	{
	case eSW_Day:
		val_tot = v->dysum.total_int;
		val_tree = v->dysum.int_veg[SW_TREES];
		val_forb = v->dysum.int_veg[SW_FORBS];
		val_shrub = v->dysum.int_veg[SW_SHRUB];
		val_grass = v->dysum.int_veg[SW_GRASS];
		val_litter = v->dysum.litter_int;
		break;
	case eSW_Week:
		val_tot = v->wkavg.total_int;
		val_tree = v->wkavg.int_veg[SW_TREES];
		val_forb = v->wkavg.int_veg[SW_FORBS];
		val_shrub = v->wkavg.int_veg[SW_SHRUB];
		val_grass = v->wkavg.int_veg[SW_GRASS];
		val_litter = v->wkavg.litter_int;
		break;
	case eSW_Month:
		val_tot = v->moavg.total_int;
		val_tree = v->moavg.int_veg[SW_TREES];
		val_forb = v->moavg.int_veg[SW_FORBS];
		val_shrub = v->moavg.int_veg[SW_SHRUB];
		val_grass = v->moavg.int_veg[SW_GRASS];
		val_litter = v->moavg.litter_int;
		break;
	case eSW_Year:
		val_tot = v->yravg.total_int;
		val_tree = v->yravg.int_veg[SW_TREES];
		val_forb = v->yravg.int_veg[SW_FORBS];
		val_shrub = v->yravg.int_veg[SW_SHRUB];
		val_grass = v->yravg.int_veg[SW_GRASS];
		val_litter = v->yravg.litter_int;
		break;
	}
	sprintf(str, "%c%7.6f%c%7.6f%c%7.6f%c%7.6f%c%7.6f%c%7.6f", _Sep, val_tot, _Sep, val_tree, _Sep, val_shrub, _Sep, val_forb, _Sep, val_grass, _Sep, val_litter);
	strcat(sw_outstr, str);

	#elif defined(STEPWAT)
			switch (pd)
			{
				case eSW_Day:
					p = SW_Model.doy-1;
					val_tot = v->dysum.total_int;
					val_tree = v->dysum.int_veg[SW_TREES];
					val_forb = v->dysum.int_veg[SW_FORBS];
					val_shrub = v->dysum.int_veg[SW_SHRUB];
					val_grass = v->dysum.int_veg[SW_GRASS];
					val_litter = v->dysum.litter_int;
					break;
				case eSW_Week:
					p = SW_Model.week-tOffset;
					val_tot = v->wkavg.total_int;
					val_tree = v->wkavg.int_veg[SW_TREES];
					val_forb = v->wkavg.int_veg[SW_FORBS];
					val_shrub = v->wkavg.int_veg[SW_SHRUB];
					val_grass = v->wkavg.int_veg[SW_GRASS];
					val_litter = v->wkavg.litter_int;
					break;
				case eSW_Month:
					p = SW_Model.month-tOffset;
					val_tot = v->moavg.total_int;
					val_tree = v->moavg.int_veg[SW_TREES];
					val_forb = v->moavg.int_veg[SW_FORBS];
					val_shrub = v->moavg.int_veg[SW_SHRUB];
					val_grass = v->moavg.int_veg[SW_GRASS];
					val_litter = v->moavg.litter_int;
					break;
				case eSW_Year:
					val_tot = v->yravg.total_int;
					val_tree = v->yravg.int_veg[SW_TREES];
					val_forb = v->yravg.int_veg[SW_FORBS];
					val_shrub = v->yravg.int_veg[SW_SHRUB];
					val_grass = v->yravg.int_veg[SW_GRASS];
					val_litter = v->yravg.litter_int;
					break;
			}

			if (isPartialSoilwatOutput == FALSE)
			{
				int
					ind0 = Iypc(Globals.currYear - 1, p, 0, pd), // index for mean
					ind1 = Iypc(Globals.currYear - 1, p, 1, pd); // index for sd
				float old_val_total, old_val_tree, old_val_forb, old_val_shrub, old_val_grass, old_val_litter = 0.;

				old_val_total = SXW_AVG.interception_total_avg[ind0];
				old_val_tree = SXW_AVG.interception_tree_avg[ind0];
				old_val_shrub = SXW_AVG.interception_shrub_avg[ind0];
				old_val_forb = SXW_AVG.interception_forb_avg[ind0];
				old_val_grass = SXW_AVG.interception_grass_avg[ind0];
				old_val_litter = SXW_AVG.interception_litter_avg[ind0];

				SXW_AVG.interception_total_avg[ind0] = get_running_avg(SXW_AVG.interception_total_avg[ind0], val_tot);
				SXW_AVG.interception_total_avg[ind1] += get_running_sqr(old_val_total, val_tot, SXW_AVG.interception_total_avg[ind0]);

				SXW_AVG.interception_tree_avg[ind0] = get_running_avg(SXW_AVG.interception_tree_avg[ind0], val_tree);
				SXW_AVG.interception_tree_avg[ind1] += get_running_sqr(old_val_tree, val_tree, SXW_AVG.interception_tree_avg[ind0]);

				SXW_AVG.interception_forb_avg[ind0] = get_running_avg(SXW_AVG.interception_forb_avg[ind0], val_forb);
				SXW_AVG.interception_forb_avg[ind1] += get_running_sqr(old_val_forb, val_forb, SXW_AVG.interception_forb_avg[ind0]);

				SXW_AVG.interception_shrub_avg[ind0] = get_running_avg(SXW_AVG.interception_shrub_avg[ind0], val_shrub);
				SXW_AVG.interception_shrub_avg[ind1] += get_running_sqr(old_val_shrub, val_shrub, SXW_AVG.interception_shrub_avg[ind0]);

				SXW_AVG.interception_grass_avg[ind0] = get_running_avg(SXW_AVG.interception_grass_avg[ind0], val_grass);
				SXW_AVG.interception_grass_avg[ind1] += get_running_sqr(old_val_grass, val_grass, SXW_AVG.interception_grass_avg[ind0]);

				SXW_AVG.interception_litter_avg[ind0] = get_running_avg(SXW_AVG.interception_litter_avg[ind0], val_litter);
				SXW_AVG.interception_litter_avg[ind1] += get_running_sqr(old_val_litter, val_litter, SXW_AVG.interception_litter_avg[ind0]);

				if(Globals.currIter == Globals.runModelIterations){
					float std_total = 0., std_tree = 0., std_forb = 0., std_shrub = 0., std_grass = 0., std_litter = 0.;

					std_total = sqrt(SXW_AVG.interception_total_avg[ind1] / Globals.currIter);
					std_tree = sqrt(SXW_AVG.interception_tree_avg[ind1] / Globals.currIter);
					std_forb = sqrt(SXW_AVG.interception_forb_avg[ind1] / Globals.currIter);
					std_shrub = sqrt(SXW_AVG.interception_shrub_avg[ind1] / Globals.currIter);
					std_grass = sqrt(SXW_AVG.interception_grass_avg[ind1] / Globals.currIter);
					std_litter = sqrt(SXW_AVG.interception_litter_avg[ind1] / Globals.currIter);

					sprintf(str, "%c%7.6f%c%7.6f%c%7.6f%c%7.6f%c%7.6f%c%7.6f%c%7.6f%c%7.6f%c%7.6f%c%7.6f%c%7.6f%c%7.6f",
					 				_Sep, SXW_AVG.interception_total_avg[ind0], _Sep, std_total,
									_Sep, SXW_AVG.interception_tree_avg[ind0], _Sep, std_tree,
									_Sep, SXW_AVG.interception_shrub_avg[ind0], _Sep, std_shrub,
									_Sep, SXW_AVG.interception_forb_avg[ind0], _Sep, std_forb,
									_Sep, SXW_AVG.interception_grass_avg[ind0], _Sep, std_grass,
									_Sep, SXW_AVG.interception_litter_avg[ind0], _Sep, std_litter);
					strcat(sw_outstr, str);
				}
			}
			if(storeAllIterations){
				sprintf(str_iters, "%c%7.6f%c%7.6f%c%7.6f%c%7.6f%c%7.6f%c%7.6f", _Sep, val_tot, _Sep, val_tree, _Sep, val_shrub, _Sep, val_forb, _Sep, val_grass, _Sep, val_litter);
				strcat(outstr_all_iters, str_iters);
			}

#else
	int delta;
	RealD *p;

	switch (pd)
	{
		case eSW_Day:
		delta = SW_Output[eSW_Interception].dy_row;
		p = p_rOUT[eSW_Interception][eSW_Day]; // set shorthand copy of 'p_rOUT' pointer
		p[delta + dy_nrow * 0] = SW_Model.simyear;
		p[delta + dy_nrow * 1] = SW_Model.doy;
		p[delta + dy_nrow * 2] = v->dysum.total_int;
		p[delta + dy_nrow * 3] = v->dysum.int_veg[SW_TREES];
		p[delta + dy_nrow * 4] = v->dysum.int_veg[SW_SHRUB];
		p[delta + dy_nrow * 5] = v->dysum.int_veg[SW_FORBS];
		p[delta + dy_nrow * 6] = v->dysum.int_veg[SW_GRASS];
		p[delta + dy_nrow * 7] = v->dysum.litter_int;
		SW_Output[eSW_Interception].dy_row++;
		break;
		case eSW_Week:
		delta = SW_Output[eSW_Interception].wk_row;
		p = p_rOUT[eSW_Interception][eSW_Week]; // set shorthand copy of 'p_rOUT' pointer
		p[delta + wk_nrow * 0] = SW_Model.simyear;
		p[delta + wk_nrow * 1] = (SW_Model.week + 1) - tOffset;
		p[delta + wk_nrow * 2] = v->wkavg.total_int;
		p[delta + wk_nrow * 3] = v->wkavg.int_veg[SW_TREES];
		p[delta + wk_nrow * 4] = v->wkavg.int_veg[SW_SHRUB];
		p[delta + wk_nrow * 5] = v->wkavg.int_veg[SW_FORBS];
		p[delta + wk_nrow * 6] = v->wkavg.int_veg[SW_GRASS];
		p[delta + wk_nrow * 7] = v->wkavg.litter_int;
		SW_Output[eSW_Interception].wk_row++;
		break;
		case eSW_Month:
		delta = SW_Output[eSW_Interception].mo_row;
		p = p_rOUT[eSW_Interception][eSW_Month]; // set shorthand copy of 'p_rOUT' pointer
		p[delta + mo_nrow * 0] = SW_Model.simyear;
		p[delta + mo_nrow * 1] = (SW_Model.month + 1) - tOffset;
		p[delta + mo_nrow * 2] = v->moavg.total_int;
		p[delta + mo_nrow * 3] = v->moavg.int_veg[SW_TREES];
		p[delta + mo_nrow * 4] = v->moavg.int_veg[SW_SHRUB];
		p[delta + mo_nrow * 5] = v->moavg.int_veg[SW_FORBS];
		p[delta + mo_nrow * 6] = v->moavg.int_veg[SW_GRASS];
		p[delta + mo_nrow * 7] = v->moavg.litter_int;
		SW_Output[eSW_Interception].mo_row++;
		break;
		case eSW_Year:
		delta = SW_Output[eSW_Interception].yr_row;
		p = p_rOUT[eSW_Interception][eSW_Year]; // set shorthand copy of 'p_rOUT' pointer
		p[delta + yr_nrow * 0] = SW_Model.simyear;
		p[delta + yr_nrow * 1] = v->yravg.total_int;
		p[delta + yr_nrow * 2] = v->yravg.int_veg[SW_TREES];
		p[delta + yr_nrow * 3] = v->yravg.int_veg[SW_SHRUB];
		p[delta + yr_nrow * 4] = v->yravg.int_veg[SW_FORBS];
		p[delta + yr_nrow * 5] = v->yravg.int_veg[SW_GRASS];
		p[delta + yr_nrow * 6] = v->yravg.litter_int;
		SW_Output[eSW_Interception].yr_row++;
		break;
	}
#endif
}

void get_soilinf(OutPeriod pd)
{
	/* --------------------------------------------------- */
	/* 20100202 (drs) added */
	/* 20110219 (drs) added runoff */
	/* 12/13/2012	(clk)	moved runoff, now named snowRunoff, to get_runoffrunon(); */
	SW_WEATHER *v = &SW_Weather;

	#if !defined(STEPWAT) && !defined(RSOILWAT)
		char str[OUTSTRLEN];
		RealD val_inf = SW_MISSING;
		get_outstrleader(pd);

	#elif defined(STEPWAT)
		char str[OUTSTRLEN];
		char str_iters[OUTSTRLEN];
		RealD val_inf = SW_MISSING;
		TimeInt p = 0;
		if ((isPartialSoilwatOutput == FALSE && Globals.currIter == Globals.runModelIterations) || storeAllIterations)
		{
			get_outstrleader(pd);
		}
	#endif

	#if !defined(STEPWAT) && !defined(RSOILWAT)
	switch (pd)
	{
	case eSW_Day:
		val_inf = v->dysum.soil_inf;
		break;
	case eSW_Week:
		val_inf = v->wkavg.soil_inf;
		break;
	case eSW_Month:
		val_inf = v->moavg.soil_inf;
		break;
	case eSW_Year:
		val_inf = v->yravg.soil_inf;
		break;
	}
	sprintf(str, "%c%7.6f", _Sep, val_inf);
	strcat(sw_outstr, str);

	#elif defined(STEPWAT)

		switch (pd)
		{
			case eSW_Day:
				p = SW_Model.doy-1;
				val_inf = v->dysum.soil_inf;
				break;
			case eSW_Week:
				val_inf = v->wkavg.soil_inf;
				p = SW_Model.week-tOffset;
				break;
			case eSW_Month:
				p = SW_Model.month-tOffset;
				val_inf = v->moavg.soil_inf;
				break;
			case eSW_Year:
				p = Globals.currYear - 1;
				val_inf = v->yravg.soil_inf;
				break;
		}

		if (isPartialSoilwatOutput == FALSE)
		{
			int
				ind0 = Iypc(Globals.currYear - 1, p, 0, pd), // index for mean
				ind1 = Iypc(Globals.currYear - 1, p, 1, pd); // index for sd
			float old_val = SXW_AVG.soilinfilt_avg[ind0];

			SXW_AVG.soilinfilt_avg[ind0] = get_running_avg(SXW_AVG.soilinfilt_avg[ind0], val_inf);
			SXW_AVG.soilinfilt_avg[ind1] += get_running_sqr(old_val, val_inf, SXW_AVG.soilinfilt_avg[ind0]);

			if(Globals.currIter == Globals.runModelIterations){
				float std = sqrt(SXW_AVG.soilinfilt_avg[ind1] / Globals.currIter);

				sprintf(str, "%c%7.6f%c%7.6f", _Sep, SXW_AVG.soilinfilt_avg[ind0], _Sep, std);
				strcat(sw_outstr, str);
			}
		}
		if(storeAllIterations){
			sprintf(str_iters, "%c%7.6f", _Sep, val_inf);
			strcat(outstr_all_iters, str_iters);
		}

#else
	int delta;
	RealD *p;

	switch (pd)
	{
		case eSW_Day:
		delta = SW_Output[eSW_SoilInf].dy_row;
		p = p_rOUT[eSW_SoilInf][eSW_Day]; // set shorthand copy of 'p_rOUT' pointer
		p[delta + dy_nrow * 0] = SW_Model.simyear;
		p[delta + dy_nrow * 1] = SW_Model.doy;
		p[delta + dy_nrow * 2] = v->dysum.soil_inf;
		SW_Output[eSW_SoilInf].dy_row++;
		break;
		case eSW_Week:
		delta = SW_Output[eSW_SoilInf].wk_row;
		p = p_rOUT[eSW_SoilInf][eSW_Week]; // set shorthand copy of 'p_rOUT' pointer
		p[delta + wk_nrow * 0] = SW_Model.simyear;
		p[delta + wk_nrow * 1] = (SW_Model.week + 1) - tOffset;
		p[delta + wk_nrow * 2] = v->wkavg.soil_inf;
		SW_Output[eSW_SoilInf].wk_row++;
		break;
		case eSW_Month:
		delta = SW_Output[eSW_SoilInf].mo_row;
		p = p_rOUT[eSW_SoilInf][eSW_Month]; // set shorthand copy of 'p_rOUT' pointer
		p[delta + mo_nrow * 0] = SW_Model.simyear;
		p[delta + mo_nrow * 1] = (SW_Model.month + 1) - tOffset;
		p[delta + mo_nrow * 2] = v->moavg.soil_inf;
		SW_Output[eSW_SoilInf].mo_row++;
		break;
		case eSW_Year:
		delta = SW_Output[eSW_SoilInf].yr_row;
		p_rOUT[eSW_SoilInf][eSW_Year][delta + yr_nrow * 0] = SW_Model.simyear;
		p_rOUT[eSW_SoilInf][eSW_Year][delta + yr_nrow * 1] = v->yravg.soil_inf;
		SW_Output[eSW_SoilInf].yr_row++;
		break;
	}
#endif
}

void get_lyrdrain(OutPeriod pd)
{
	/* --------------------------------------------------- */
	/* 20100202 (drs) added */
	LyrIndex i;
	SW_SOILWAT *v = &SW_Soilwat;

	#if !defined(STEPWAT) && !defined(RSOILWAT)
		char str[OUTSTRLEN];
		RealD val = SW_MISSING;
		get_outstrleader(pd);

	#elif defined(STEPWAT)
		char str[OUTSTRLEN];
		char str_iters[OUTSTRLEN];
		RealD val = SW_MISSING;
		TimeInt p = 0;
		if ((isPartialSoilwatOutput == FALSE && Globals.currIter == Globals.runModelIterations) || storeAllIterations)
		{
			get_outstrleader(pd);
		}
	#endif

	#if !defined(STEPWAT) && !defined(RSOILWAT)
	for (i = 0; i < SW_Site.n_layers - 1; i++)
	{
		switch (pd)
		{
		case eSW_Day:
			val = v->dysum.lyrdrain[i];
			break;
		case eSW_Week:
			val = v->wkavg.lyrdrain[i];
			break;
		case eSW_Month:
			val = v->moavg.lyrdrain[i];
			break;
		case eSW_Year:
			val = v->yravg.lyrdrain[i];
			break;
		}
		sprintf(str, "%c%7.6f", _Sep, val);
		strcat(sw_outstr, str);
	}

	#elif defined(STEPWAT)
	for (i = 0; i < SW_Site.n_layers - 1; i++)
	{
		switch (pd)
		{
			case eSW_Day:
				p = SW_Model.doy-1;
				val = v->dysum.lyrdrain[i];
				break;
			case eSW_Week:
				val = v->wkavg.lyrdrain[i];
				p = SW_Model.week-tOffset;
				break;
			case eSW_Month:
				val = v->moavg.lyrdrain[i];
				p = SW_Model.month-tOffset;
				break;
			case eSW_Year:
				p = Globals.currYear - 1;
				val = v->yravg.lyrdrain[i];
				break;
		}
		if (isPartialSoilwatOutput == FALSE)
		{
			int
				indl0 = Iylp(Globals.currYear - 1, i, p, pd, 0), // index for mean
				indl1 = Iylp(Globals.currYear - 1, i, p, pd, 1); // index for sd
			float old_val = SXW_AVG.lyrdrain_avg[indl0];

			SXW_AVG.lyrdrain_avg[indl0] = get_running_avg(SXW_AVG.lyrdrain_avg[indl0], val);
			SXW_AVG.lyrdrain_avg[indl1] += get_running_sqr(old_val, val, SXW_AVG.lyrdrain_avg[indl0]);

			if(Globals.currIter == Globals.runModelIterations){
				float std = sqrt(SXW_AVG.lyrdrain_avg[indl1] / Globals.currIter);

				sprintf(str, "%c%7.6f%c%7.6f", _Sep, SXW_AVG.lyrdrain_avg[indl0], _Sep, std);
				strcat(sw_outstr, str);
			}
		}
		if(storeAllIterations){
			sprintf(str_iters, "%c%7.6f", _Sep, val);
			strcat(outstr_all_iters, str_iters);
		}
	}

#else
	int delta;
	RealD *p;

	switch (pd)
	{
		case eSW_Day:
		delta = SW_Output[eSW_LyrDrain].dy_row;
		p = p_rOUT[eSW_LyrDrain][eSW_Day]; // set shorthand copy of 'p_rOUT' pointer
		p[delta + dy_nrow * 0] = SW_Model.simyear;
		p[delta + dy_nrow * 1] = SW_Model.doy;
		for (i = 0; i < SW_Site.n_layers - 1; i++)
		{
			p[delta + dy_nrow * (i + 2)] = v->dysum.lyrdrain[i];
		}
		SW_Output[eSW_LyrDrain].dy_row++;
		break;
		case eSW_Week:
		delta = SW_Output[eSW_LyrDrain].wk_row;
		p = p_rOUT[eSW_LyrDrain][eSW_Week]; // set shorthand copy of 'p_rOUT' pointer
		p[delta + wk_nrow * 0] = SW_Model.simyear;
		p[delta + wk_nrow * 1] = (SW_Model.week + 1) - tOffset;
		for (i = 0; i < SW_Site.n_layers - 1; i++)
		{
			p[delta + wk_nrow * (i + 2)] = v->wkavg.lyrdrain[i];
		}
		SW_Output[eSW_LyrDrain].wk_row++;
		break;
		case eSW_Month:
		delta = SW_Output[eSW_LyrDrain].mo_row;
		p = p_rOUT[eSW_LyrDrain][eSW_Month]; // set shorthand copy of 'p_rOUT' pointer
		p[delta + mo_nrow * 0] = SW_Model.simyear;
		p[delta + mo_nrow * 1] = (SW_Model.month + 1) - tOffset;
		for (i = 0; i < SW_Site.n_layers - 1; i++)
		{
			p[delta + mo_nrow * (i + 2)] = v->moavg.lyrdrain[i];
		}
		SW_Output[eSW_LyrDrain].mo_row++;
		break;
		case eSW_Year:
		delta = SW_Output[eSW_LyrDrain].yr_row;
		p = p_rOUT[eSW_LyrDrain][eSW_Year]; // set shorthand copy of 'p_rOUT' pointer
		p[delta + yr_nrow * 0] = SW_Model.simyear;
		for (i = 0; i < SW_Site.n_layers - 1; i++)
		{
			p[delta + yr_nrow * (i + 1)] = v->yravg.lyrdrain[i];
		}
		SW_Output[eSW_LyrDrain].yr_row++;
		break;
	}
#endif
}

void get_hydred(OutPeriod pd)
{
	/* --------------------------------------------------- */
	/* 20101020 (drs) added */
	LyrIndex i;
	SW_SOILWAT *v = &SW_Soilwat;

	#if !defined(STEPWAT) && !defined(RSOILWAT)
		char str[OUTSTRLEN];
		RealD val_total = SW_MISSING;
		RealD val_tree = SW_MISSING;
		RealD val_shrub = SW_MISSING;
		RealD val_forb = SW_MISSING;
		RealD val_grass = SW_MISSING;
		get_outstrleader(pd);

	#elif defined(STEPWAT)
		char str[OUTSTRLEN];
		char str_iters[OUTSTRLEN];
		RealD val_total = SW_MISSING;
		RealD val_tree = SW_MISSING;
		RealD val_shrub = SW_MISSING;
		RealD val_forb = SW_MISSING;
		RealD val_grass = SW_MISSING;
		TimeInt p = 0;
		if ((isPartialSoilwatOutput == FALSE && Globals.currIter == Globals.runModelIterations) || storeAllIterations)
		{
			get_outstrleader(pd);
		}
	#endif

	#if !defined(STEPWAT) && !defined(RSOILWAT)
	/* total output */
	ForEachSoilLayer(i)
	{
		switch (pd)
		{
		case eSW_Day:
			val_total = v->dysum.hydred_total[i];
			val_tree = v->dysum.hydred[SW_TREES][i];
			val_shrub = v->dysum.hydred[SW_SHRUB][i];
			val_grass = v->dysum.hydred[SW_GRASS][i];
			val_forb = v->dysum.hydred[SW_FORBS][i];
			break;
		case eSW_Week:
			val_total = v->wkavg.hydred_total[i];
			val_tree = v->wkavg.hydred[SW_TREES][i];
			val_shrub = v->wkavg.hydred[SW_SHRUB][i];
			val_grass = v->wkavg.hydred[SW_GRASS][i];
			val_forb = v->wkavg.hydred[SW_FORBS][i];
			break;
		case eSW_Month:
			val_total = v->moavg.hydred_total[i];
			val_tree = v->moavg.hydred[SW_TREES][i];
			val_shrub = v->moavg.hydred[SW_SHRUB][i];
			val_grass = v->moavg.hydred[SW_GRASS][i];
			val_forb = v->moavg.hydred[SW_FORBS][i];
			break;
		case eSW_Year:
			val_total = v->yravg.hydred_total[i];
			val_tree = v->yravg.hydred[SW_TREES][i];
			val_shrub = v->yravg.hydred[SW_SHRUB][i];
			val_grass = v->yravg.hydred[SW_GRASS][i];
			val_forb = v->yravg.hydred[SW_FORBS][i];
			break;
		}

		sprintf(str, "%c%7.6f%c%7.6f%c%7.6f%c%7.6f%c%7.6f", _Sep, val_total, _Sep, val_tree, _Sep, val_shrub, _Sep, val_forb, _Sep, val_grass);
		strcat(sw_outstr, str);
	}

	#elif defined(STEPWAT)
	ForEachSoilLayer(i)
	{
			switch (pd)
			{
			case eSW_Day:
				val_total = v->dysum.hydred_total[i];
				val_tree = v->dysum.hydred[SW_TREES][i];
				val_shrub = v->dysum.hydred[SW_SHRUB][i];
				val_grass = v->dysum.hydred[SW_GRASS][i];
				val_forb = v->dysum.hydred[SW_FORBS][i];
				p = SW_Model.doy-1;
				break;
			case eSW_Week:
				val_total = v->wkavg.hydred_total[i];
				val_tree = v->wkavg.hydred[SW_TREES][i];
				val_shrub = v->wkavg.hydred[SW_SHRUB][i];
				val_grass = v->wkavg.hydred[SW_GRASS][i];
				val_forb = v->wkavg.hydred[SW_FORBS][i];
				p = SW_Model.week-tOffset;
				break;
			case eSW_Month:
				val_total = v->moavg.hydred_total[i];
				val_tree = v->moavg.hydred[SW_TREES][i];
				val_shrub = v->moavg.hydred[SW_SHRUB][i];
				val_grass = v->moavg.hydred[SW_GRASS][i];
				val_forb = v->moavg.hydred[SW_FORBS][i];
				p = SW_Model.month-tOffset;
				break;
			case eSW_Year:
				val_total = v->yravg.hydred_total[i];
				val_tree = v->yravg.hydred[SW_TREES][i];
				val_shrub = v->yravg.hydred[SW_SHRUB][i];
				val_grass = v->yravg.hydred[SW_GRASS][i];
				val_forb = v->yravg.hydred[SW_FORBS][i];
				p = Globals.currYear - 1;
				break;
			}
		if (isPartialSoilwatOutput == FALSE)
		{
			int
				indl0 = Iylp(Globals.currYear - 1, i, p, pd, 0), // index for mean
				indl1 = Iylp(Globals.currYear - 1, i, p, pd, 1); // index for sd
			float old_val_total = SXW_AVG.hydred_total_avg[indl0];
			float old_val_tree = SXW_AVG.hydred_tree_avg[indl0];
			float old_val_forb = SXW_AVG.hydred_forb_avg[indl0];
			float old_val_shrub = SXW_AVG.hydred_shrub_avg[indl0];
			float old_val_grass = SXW_AVG.hydred_grass_avg[indl0];

			SXW_AVG.hydred_total_avg[indl0] = get_running_avg(SXW_AVG.hydred_total_avg[indl0], val_total);
			SXW_AVG.hydred_tree_avg[indl0] = get_running_avg(SXW_AVG.hydred_tree_avg[indl0], val_tree);
			SXW_AVG.hydred_shrub_avg[indl0] = get_running_avg(SXW_AVG.hydred_shrub_avg[indl0], val_shrub);
			SXW_AVG.hydred_forb_avg[indl0] = get_running_avg(SXW_AVG.hydred_forb_avg[indl0], val_forb);
			SXW_AVG.hydred_grass_avg[indl0] = get_running_avg(SXW_AVG.hydred_grass_avg[indl0], val_grass);

			SXW_AVG.hydred_total_avg[indl1] += get_running_sqr(old_val_total, val_total, SXW_AVG.hydred_total_avg[indl0]);
			SXW_AVG.hydred_tree_avg[indl1] += get_running_sqr(old_val_tree, val_tree, SXW_AVG.hydred_tree_avg[indl0]);
			SXW_AVG.hydred_shrub_avg[indl1] += get_running_sqr(old_val_shrub, val_shrub, SXW_AVG.hydred_shrub_avg[indl0]);
			SXW_AVG.hydred_forb_avg[indl1] += get_running_sqr(old_val_forb, val_forb, SXW_AVG.hydred_forb_avg[indl0]);
			SXW_AVG.hydred_grass_avg[indl1] += get_running_sqr(old_val_grass, val_grass, SXW_AVG.hydred_grass_avg[indl0]);

			if(Globals.currIter == Globals.runModelIterations){
				float std_total = sqrt(SXW_AVG.hydred_total_avg[indl1] / Globals.currIter);
				float std_tree = sqrt(SXW_AVG.hydred_tree_avg[indl1] / Globals.currIter);
				float std_forb = sqrt(SXW_AVG.hydred_forb_avg[indl1] / Globals.currIter);
				float std_shrub = sqrt(SXW_AVG.hydred_shrub_avg[indl1] / Globals.currIter);
				float std_grass = sqrt(SXW_AVG.hydred_grass_avg[indl1] / Globals.currIter);


				sprintf(str, "%c%7.6f%c%7.6f%c%7.6f%c%7.6f%c%7.6f%c%7.6f%c%7.6f%c%7.6f%c%7.6f%c%7.6f",
								_Sep, SXW_AVG.hydred_total_avg[indl0], _Sep, std_total,
								_Sep, SXW_AVG.hydred_tree_avg[indl0], _Sep, std_tree,
								_Sep, SXW_AVG.hydred_shrub_avg[indl0], _Sep, std_shrub,
								_Sep, SXW_AVG.hydred_forb_avg[indl0], _Sep, std_forb,
								_Sep, SXW_AVG.hydred_grass_avg[indl0], _Sep, std_grass
							);
				strcat(sw_outstr, str);
			}
		}
		if(storeAllIterations){
			sprintf(str_iters, "%c%7.6f%c%7.6f%c%7.6f%c%7.6f%c%7.6f", _Sep, val_total, _Sep, val_tree, _Sep, val_shrub, _Sep, val_forb, _Sep, val_grass);
			strcat(outstr_all_iters, str_iters);
		}
	}

#else
	int delta;
	RealD *p;
	/* Date Info output */
	switch (pd)
	{
		case eSW_Day:
		delta = SW_Output[eSW_HydRed].dy_row;
		p = p_rOUT[eSW_HydRed][eSW_Day]; // set shorthand copy of 'p_rOUT' pointer
		p[delta + dy_nrow * 0] = SW_Model.simyear;
		p[delta + dy_nrow * 1] = SW_Model.doy;
		break;
		case eSW_Week:
		delta = SW_Output[eSW_HydRed].wk_row;
		p = p_rOUT[eSW_HydRed][eSW_Week]; // set shorthand copy of 'p_rOUT' pointer
		p[delta + wk_nrow * 0] = SW_Model.simyear;
		p[delta + wk_nrow * 1] = (SW_Model.week + 1) - tOffset;
		break;
		case eSW_Month:
		delta = SW_Output[eSW_HydRed].mo_row;
		p = p_rOUT[eSW_HydRed][eSW_Month]; // set shorthand copy of 'p_rOUT' pointer
		p[delta + mo_nrow * 0] = SW_Model.simyear;
		p[delta + mo_nrow * 1] = (SW_Model.month + 1) - tOffset;
		break;
		case eSW_Year:
		delta = SW_Output[eSW_HydRed].yr_row;
		p = p_rOUT[eSW_HydRed][eSW_Year]; // set shorthand copy of 'p_rOUT' pointer
		p[delta + yr_nrow * 0] = SW_Model.simyear;
		break;
	}

	/* total output */
	switch (pd)
	{
		case eSW_Day:
		ForEachSoilLayer(i)
		p[delta + dy_nrow * (i + 2) + (dy_nrow * SW_Site.n_layers * 0)] = v->dysum.hydred_total[i];
		break;
		case eSW_Week:
		ForEachSoilLayer(i)
		p[delta + wk_nrow * (i + 2) + (wk_nrow * SW_Site.n_layers * 0)] = v->wkavg.hydred_total[i];
		break;
		case eSW_Month:
		ForEachSoilLayer(i)
		p[delta + mo_nrow * (i + 2) + (mo_nrow * SW_Site.n_layers * 0)] = v->moavg.hydred_total[i];
		break;
		case eSW_Year:
		ForEachSoilLayer(i)
		p[delta + yr_nrow * (i + 1) + (yr_nrow * SW_Site.n_layers * 0)] = v->yravg.hydred_total[i];
		break;
	}

	/* tree output */
	switch (pd)
	{
		case eSW_Day:
		ForEachSoilLayer(i)
		p[delta + dy_nrow * (i + 2) + (dy_nrow * SW_Site.n_layers * 1)] = v->dysum.hydred[SW_TREES][i];
		break;
		case eSW_Week:
		ForEachSoilLayer(i)
		p[delta + wk_nrow * (i + 2) + (wk_nrow * SW_Site.n_layers * 1)] = v->wkavg.hydred[SW_TREES][i];
		break;
		case eSW_Month:
		ForEachSoilLayer(i)
		p[delta + mo_nrow * (i + 2) + (mo_nrow * SW_Site.n_layers * 1)] = v->moavg.hydred[SW_TREES][i];
		break;
		case eSW_Year:
		ForEachSoilLayer(i)
		p[delta + yr_nrow * (i + 1) + (yr_nrow * SW_Site.n_layers * 1)] = v->yravg.hydred[SW_TREES][i];
		break;
	}

	/* shrub output */
	switch (pd)
	{
		case eSW_Day:
		ForEachSoilLayer(i)
		p[delta + dy_nrow * (i + 2) + (dy_nrow * SW_Site.n_layers * 2)] = v->dysum.hydred[SW_SHRUB][i];
		break;
		case eSW_Week:
		ForEachSoilLayer(i)
		p[delta + wk_nrow * (i + 2) + (wk_nrow * SW_Site.n_layers * 2)] = v->wkavg.hydred[SW_SHRUB][i];
		break;
		case eSW_Month:
		ForEachSoilLayer(i)
		p[delta + mo_nrow * (i + 2) + (mo_nrow * SW_Site.n_layers * 2)] = v->moavg.hydred[SW_SHRUB][i];
		break;
		case eSW_Year:
		ForEachSoilLayer(i)
		p[delta + yr_nrow * (i + 1) + (yr_nrow * SW_Site.n_layers * 2)] = v->yravg.hydred[SW_SHRUB][i];
		break;
	}

	/* forb output */
	switch (pd)
	{
		case eSW_Day:
		ForEachSoilLayer(i)
		p[delta + dy_nrow * (i + 2) + (dy_nrow * SW_Site.n_layers * 3)] = v->dysum.hydred[SW_FORBS][i];
		break;
		case eSW_Week:
		ForEachSoilLayer(i)
		p[delta + wk_nrow * (i + 2) + (wk_nrow * SW_Site.n_layers * 3)] = v->wkavg.hydred[SW_FORBS][i];
		break;
		case eSW_Month:
		ForEachSoilLayer(i)
		p[delta + mo_nrow * (i + 2) + (mo_nrow * SW_Site.n_layers * 3)] = v->moavg.hydred[SW_FORBS][i];
		break;
		case eSW_Year:
		ForEachSoilLayer(i)
		p[delta + yr_nrow * (i + 1) + (yr_nrow * SW_Site.n_layers * 3)] = v->yravg.hydred[SW_FORBS][i];
		break;
	}

	/* grass output */
	switch (pd)
	{
		case eSW_Day:
		ForEachSoilLayer(i)
		p[delta + dy_nrow * (i + 2) + (dy_nrow * SW_Site.n_layers * 4)] = v->dysum.hydred[SW_GRASS][i];
		SW_Output[eSW_HydRed].dy_row++;
		break;
		case eSW_Week:
		ForEachSoilLayer(i)
		p[delta + wk_nrow * (i + 2) + (wk_nrow * SW_Site.n_layers * 4)] = v->wkavg.hydred[SW_GRASS][i];
		SW_Output[eSW_HydRed].wk_row++;
		break;
		case eSW_Month:
		ForEachSoilLayer(i)
		p[delta + mo_nrow * (i + 2) + (mo_nrow * SW_Site.n_layers * 4)] = v->moavg.hydred[SW_GRASS][i];
		SW_Output[eSW_HydRed].mo_row++;
		break;
		case eSW_Year:
		ForEachSoilLayer(i)
		p[delta + yr_nrow * (i + 1) + (yr_nrow * SW_Site.n_layers * 4)] = v->yravg.hydred[SW_GRASS][i];
		SW_Output[eSW_HydRed].yr_row++;
		break;
	}
#endif
}

void get_aet(OutPeriod pd)
{
	/* --------------------------------------------------- */
	SW_SOILWAT *v = &SW_Soilwat;
#if !defined(STEPWAT) && !defined(RSOILWAT)
	char str[OUTSTRLEN];
	RealD val = SW_MISSING;
	get_outstrleader(pd);

#elif defined(STEPWAT)
	char str[OUTSTRLEN];
	char str_iters[OUTSTRLEN];
	RealD val = SW_MISSING;
	TimeInt p = 0;
	if ((isPartialSoilwatOutput == FALSE && Globals.currIter == Globals.runModelIterations) || storeAllIterations)
		get_outstrleader(pd);

#endif

#ifndef RSOILWAT
	switch (pd)
	{
	case eSW_Day:
		val = v->dysum.aet;
		break;
	case eSW_Week:
		val = v->wkavg.aet;
		break;
	case eSW_Month:
		val = v->moavg.aet;
		break;
	case eSW_Year:
		val = v->yravg.aet;
		break;
	}
#else
	switch (pd)
	{
		case eSW_Day:
		p_rOUT[eSW_AET][eSW_Day][SW_Output[eSW_AET].dy_row + dy_nrow * 0] = SW_Model.simyear;
		p_rOUT[eSW_AET][eSW_Day][SW_Output[eSW_AET].dy_row + dy_nrow * 1] = SW_Model.doy;
		p_rOUT[eSW_AET][eSW_Day][SW_Output[eSW_AET].dy_row + dy_nrow * 2] = v->dysum.aet;
		SW_Output[eSW_AET].dy_row++;
		break;
		case eSW_Week:
		p_rOUT[eSW_AET][eSW_Week][SW_Output[eSW_AET].wk_row + wk_nrow * 0] = SW_Model.simyear;
		p_rOUT[eSW_AET][eSW_Week][SW_Output[eSW_AET].wk_row + wk_nrow * 1] = (SW_Model.week + 1) - tOffset;
		p_rOUT[eSW_AET][eSW_Week][SW_Output[eSW_AET].wk_row + wk_nrow * 2] = v->wkavg.aet;
		SW_Output[eSW_AET].wk_row++;
		break;
		case eSW_Month:
		p_rOUT[eSW_AET][eSW_Month][SW_Output[eSW_AET].mo_row + mo_nrow * 0] = SW_Model.simyear;
		p_rOUT[eSW_AET][eSW_Month][SW_Output[eSW_AET].mo_row + mo_nrow * 1] = (SW_Model.month + 1) - tOffset;
		p_rOUT[eSW_AET][eSW_Month][SW_Output[eSW_AET].mo_row + mo_nrow * 2] = v->moavg.aet;
		SW_Output[eSW_AET].mo_row++;
		break;
		case eSW_Year:
		p_rOUT[eSW_AET][eSW_Year][SW_Output[eSW_AET].yr_row + yr_nrow * 0] = SW_Model.simyear;
		p_rOUT[eSW_AET][eSW_Year][SW_Output[eSW_AET].yr_row + yr_nrow * 1] = v->yravg.aet;
		SW_Output[eSW_AET].yr_row++;
		break;
	}
#endif

#if !defined(STEPWAT) && !defined(RSOILWAT)
	sprintf(str, "%c%7.6f", _Sep, val);
	strcat(sw_outstr, str);
#elif defined(STEPWAT)
	switch (pd)
	{
		case eSW_Day:
			p = SW_Model.doy-1;
			break;
		case eSW_Week:
			p = SW_Model.week-tOffset;
			break;
		case eSW_Month:
			p = SW_Model.month-tOffset;
			break;
		case eSW_Year:
			p = Globals.currYear - 1;
			break;
	}

	if (isPartialSoilwatOutput == FALSE)
	{
		int
			ind0 = Iypc(Globals.currYear - 1, p, 0, pd), // index for mean
			ind1 = Iypc(Globals.currYear - 1, p, 1, pd); // index for sd
		float old_val = SXW_AVG.aet_avg[ind0];

		//running aet_avg
		SXW_AVG.aet_avg[ind0] = get_running_avg(SXW_AVG.aet_avg[ind0], val);
		SXW_AVG.aet_avg[ind1] += get_running_sqr(old_val, val, SXW_AVG.aet_avg[ind0]);

		if(Globals.currIter == Globals.runModelIterations){
			float std_aet = sqrt(SXW_AVG.aet_avg[ind1] / Globals.currIter);

			sprintf(str, "%c%7.6f%c%7.6f", _Sep, SXW_AVG.aet_avg[ind0], _Sep, std_aet);
			strcat(sw_outstr, str);
		}
	}

	if(storeAllIterations){
		sprintf(str_iters, "%c%7.6f", _Sep, val);
		strcat(outstr_all_iters, str_iters);
	}
		SXW.aet += val;

#endif
}

void get_pet(OutPeriod pd)
{
	/* --------------------------------------------------- */
	SW_SOILWAT *v = &SW_Soilwat;

	#if !defined(STEPWAT) && !defined(RSOILWAT)
		char str[OUTSTRLEN];
		RealD val = SW_MISSING;
		get_outstrleader(pd);


	#elif defined(STEPWAT)
		char str[OUTSTRLEN];
		char str_iters[OUTSTRLEN];
		RealD val = SW_MISSING;
		TimeInt p = 0;
		if ((isPartialSoilwatOutput == FALSE && Globals.currIter == Globals.runModelIterations) || storeAllIterations)
		{
			get_outstrleader(pd);
		}
	#endif

	#if !defined(STEPWAT) && !defined(RSOILWAT)
	switch (pd)
	{
	case eSW_Day:
		val = v->dysum.pet;
		break;
	case eSW_Week:
		val = v->wkavg.pet;
		break;
	case eSW_Month:
		val = v->moavg.pet;
		break;
	case eSW_Year:
		val = v->yravg.pet;
		break;
	}
	sprintf(str, "%c%7.6f", _Sep, val);
	strcat(sw_outstr, str);

	#elif defined(STEPWAT)
		switch (pd)
		{
			case eSW_Day:
				p = SW_Model.doy-1;
				val = v->dysum.pet;
				break;
			case eSW_Week:
				val = v->wkavg.pet;
				p = SW_Model.week-tOffset;
				break;
			case eSW_Month:
				p = SW_Model.month-tOffset;
				val = v->moavg.pet;
				break;
			case eSW_Year:
				val = v->yravg.pet;
				p = Globals.currYear-1;
				break;
		}

		if (isPartialSoilwatOutput == FALSE)
		{
			int
				ind0 = Iypc(Globals.currYear - 1, p, 0, pd), // index for mean
				ind1 = Iypc(Globals.currYear - 1, p, 1, pd); // index for sd
			float old_val = SXW_AVG.pet_avg[ind0];

			SXW_AVG.pet_avg[ind0] = get_running_avg(SXW_AVG.pet_avg[ind0], val);
			SXW_AVG.pet_avg[ind1] += get_running_sqr(old_val, val, SXW_AVG.pet_avg[ind0]);

			if(Globals.currIter == Globals.runModelIterations){
				float std = sqrt(SXW_AVG.pet_avg[ind1] / Globals.currIter);

				sprintf(str, "%c%7.6f%c%7.6f", _Sep, SXW_AVG.pet_avg[ind0], _Sep, std);
				strcat(sw_outstr, str);
			}
		}
		if(storeAllIterations){
			sprintf(str_iters, "%c%7.6f", _Sep, val);
			strcat(outstr_all_iters, str_iters);
		}

#else
	switch (pd)
	{
		case eSW_Day:
		p_rOUT[eSW_PET][eSW_Day][SW_Output[eSW_PET].dy_row + dy_nrow * 0] = SW_Model.simyear;
		p_rOUT[eSW_PET][eSW_Day][SW_Output[eSW_PET].dy_row + dy_nrow * 1] = SW_Model.doy;
		p_rOUT[eSW_PET][eSW_Day][SW_Output[eSW_PET].dy_row + dy_nrow * 2] = v->dysum.pet;
		SW_Output[eSW_PET].dy_row++;
		break;
		case eSW_Week:
		p_rOUT[eSW_PET][eSW_Week][SW_Output[eSW_PET].wk_row + wk_nrow * 0] = SW_Model.simyear;
		p_rOUT[eSW_PET][eSW_Week][SW_Output[eSW_PET].wk_row + wk_nrow * 1] = (SW_Model.week + 1) - tOffset;
		p_rOUT[eSW_PET][eSW_Week][SW_Output[eSW_PET].wk_row + wk_nrow * 2] = v->wkavg.pet;
		SW_Output[eSW_PET].wk_row++;
		break;
		case eSW_Month:
		p_rOUT[eSW_PET][eSW_Month][SW_Output[eSW_PET].mo_row + mo_nrow * 0] = SW_Model.simyear;
		p_rOUT[eSW_PET][eSW_Month][SW_Output[eSW_PET].mo_row + mo_nrow * 1] = (SW_Model.month + 1) - tOffset;
		p_rOUT[eSW_PET][eSW_Month][SW_Output[eSW_PET].mo_row + mo_nrow * 2] = v->moavg.pet;
		SW_Output[eSW_PET].mo_row++;
		break;
		case eSW_Year:
		p_rOUT[eSW_PET][eSW_Year][SW_Output[eSW_PET].yr_row + yr_nrow * 0] = SW_Model.simyear;
		p_rOUT[eSW_PET][eSW_Year][SW_Output[eSW_PET].yr_row + yr_nrow * 1] = v->yravg.pet;
		SW_Output[eSW_PET].yr_row++;
		break;
	}
#endif
}

void get_wetdays(OutPeriod pd)
{
	/* --------------------------------------------------- */
	LyrIndex i;
	SW_SOILWAT *v = &SW_Soilwat;

	#if !defined(STEPWAT) && !defined(RSOILWAT)
		char str[OUTSTRLEN];
		get_outstrleader(pd);

	#elif defined(STEPWAT)
		char str[OUTSTRLEN];
		char str_iters[OUTSTRLEN];
		TimeInt p = 0;
		if ((isPartialSoilwatOutput == FALSE && Globals.currIter == Globals.runModelIterations) || storeAllIterations)
		{
			get_outstrleader(pd);
		}
	#endif

	#if !defined(STEPWAT) && !defined(RSOILWAT)
	int val = 99;
	ForEachSoilLayer(i)
	{
		switch (pd)
		{
		case eSW_Day:
			val = (v->is_wet[i]) ? 1 : 0;
			break;
		case eSW_Week:
			val = (int) v->wkavg.wetdays[i];
			break;
		case eSW_Month:
			val = (int) v->moavg.wetdays[i];
			break;
		case eSW_Year:
			val = (int) v->yravg.wetdays[i];
			break;
		}
		sprintf(str, "%c%i", _Sep, val);
		strcat(sw_outstr, str);
	}

	#elif defined(STEPWAT)
	int val = 99;
	ForEachSoilLayer(i){
		switch (pd)
		{
			case eSW_Day:
				p = SW_Model.doy-1;
				val = (v->is_wet[i]) ? 1 : 0;
				break;
			case eSW_Week:
				val = (int) v->wkavg.wetdays[i];
				p = SW_Model.week-tOffset;
				break;
			case eSW_Month:
				p = SW_Model.month-tOffset;
				val = (int) v->moavg.wetdays[i];
				break;
			case eSW_Year:
				val = (int) v->yravg.wetdays[i];
				p = Globals.currYear - 1;
				break;
		}

		if (isPartialSoilwatOutput == FALSE)
		{
			int
				indl0 = Iylp(Globals.currYear - 1, i, p, pd, 0), // index for mean
				indl1 = Iylp(Globals.currYear - 1, i, p, pd, 1); // index for sd
			float old_val = SXW_AVG.wetday_avg[indl0];

			SXW_AVG.wetday_avg[indl0] = get_running_avg(SXW_AVG.wetday_avg[indl0], val);
			SXW_AVG.wetday_avg[indl1] += get_running_sqr(old_val, val, SXW_AVG.wetday_avg[indl0]);

			if(Globals.currIter == Globals.runModelIterations){
				float std = sqrt(SXW_AVG.wetday_avg[indl1] / Globals.currIter);

				sprintf(str, "%c%i%c%i", _Sep, (int)SXW_AVG.wetday_avg[indl0], _Sep, (int)std); // cast to int for proper output format
				strcat(sw_outstr, str);
			}
		}
		if(storeAllIterations){
			sprintf(str_iters, "%c%i", _Sep, val);
			strcat(outstr_all_iters, str_iters);
		}
	}

#else
	switch (pd)
	{
		case eSW_Day:
		p_rOUT[eSW_WetDays][eSW_Day][SW_Output[eSW_WetDays].dy_row + dy_nrow * 0] = SW_Model.simyear;
		p_rOUT[eSW_WetDays][eSW_Day][SW_Output[eSW_WetDays].dy_row + dy_nrow * 1] = SW_Model.doy;
		ForEachSoilLayer(i)
		{
			p_rOUT[eSW_WetDays][eSW_Day][SW_Output[eSW_WetDays].dy_row + dy_nrow * (i + 2)] = (v->is_wet[i]) ? 1 : 0;
		}
		SW_Output[eSW_WetDays].dy_row++;
		break;
		case eSW_Week:
		p_rOUT[eSW_WetDays][eSW_Week][SW_Output[eSW_WetDays].wk_row + wk_nrow * 0] = SW_Model.simyear;
		p_rOUT[eSW_WetDays][eSW_Week][SW_Output[eSW_WetDays].wk_row + wk_nrow * 1] = (SW_Model.week + 1) - tOffset;
		ForEachSoilLayer(i)
		{
			p_rOUT[eSW_WetDays][eSW_Week][SW_Output[eSW_WetDays].wk_row + wk_nrow * (i + 2)] = (int) v->wkavg.wetdays[i];
		}
		SW_Output[eSW_WetDays].wk_row++;
		break;
		case eSW_Month:
		p_rOUT[eSW_WetDays][eSW_Month][SW_Output[eSW_WetDays].mo_row + mo_nrow * 0] = SW_Model.simyear;
		p_rOUT[eSW_WetDays][eSW_Month][SW_Output[eSW_WetDays].mo_row + mo_nrow * 1] = (SW_Model.month + 1) - tOffset;
		ForEachSoilLayer(i)
		{
			p_rOUT[eSW_WetDays][eSW_Month][SW_Output[eSW_WetDays].mo_row + mo_nrow * (i + 2)] = (int) v->moavg.wetdays[i];
		}
		SW_Output[eSW_WetDays].mo_row++;
		break;
		case eSW_Year:
		p_rOUT[eSW_WetDays][eSW_Year][SW_Output[eSW_WetDays].yr_row + yr_nrow * 0] = SW_Model.simyear;
		ForEachSoilLayer(i)
		{
			p_rOUT[eSW_WetDays][eSW_Year][SW_Output[eSW_WetDays].yr_row + yr_nrow * (i + 1)] = (int) v->yravg.wetdays[i];
		}
		SW_Output[eSW_WetDays].yr_row++;
		break;
	}
#endif
}

void get_snowpack(OutPeriod pd)
{
	/* --------------------------------------------------- */
	SW_SOILWAT *v = &SW_Soilwat;

	#if !defined(STEPWAT) && !defined(RSOILWAT)
		char str[OUTSTRLEN];
		RealD val_swe = SW_MISSING, val_depth = SW_MISSING;
		get_outstrleader(pd);

	#elif defined(STEPWAT)
		char str[OUTSTRLEN];
		char str_iters[OUTSTRLEN];
		RealD val_swe = SW_MISSING, val_depth = SW_MISSING;
		TimeInt p = 0;
		if ((isPartialSoilwatOutput == FALSE && Globals.currIter == Globals.runModelIterations) || storeAllIterations)
		{
			get_outstrleader(pd);
		}
	#endif

	#if !defined(STEPWAT) && !defined(RSOILWAT)
		switch (pd)
		{
		case eSW_Day:
			val_swe = v->dysum.snowpack;
			val_depth = v->dysum.snowdepth;
			break;
		case eSW_Week:
			val_swe = v->wkavg.snowpack;
			val_depth = v->wkavg.snowdepth;
			break;
		case eSW_Month:
			val_swe = v->moavg.snowpack;
			val_depth = v->moavg.snowdepth;
			break;
		case eSW_Year:
			val_swe = v->yravg.snowpack;
			val_depth = v->yravg.snowdepth;
			break;
		}
		sprintf(str, "%c%7.6f%c%7.6f", _Sep, val_swe, _Sep, val_depth);
		strcat(sw_outstr, str);

	#elif defined(STEPWAT)
		switch (pd)
		{
			case eSW_Day:
				p = SW_Model.doy-1;
				val_swe = v->dysum.snowpack;
				val_depth = v->dysum.snowdepth;
				break;
			case eSW_Week:
				val_swe = v->wkavg.snowpack;
				val_depth = v->wkavg.snowdepth;
				p = SW_Model.week-tOffset;
				break;
			case eSW_Month:
				p = SW_Model.month-tOffset;
				val_swe = v->moavg.snowpack;
				val_depth = v->moavg.snowdepth;
				break;
			case eSW_Year:
				val_swe = v->yravg.snowpack;
				val_depth = v->yravg.snowdepth;
				p = Globals.currYear - 1;
				break;
		}

		if (isPartialSoilwatOutput == FALSE)
		{
			int
				ind0 = Iypc(Globals.currYear - 1, p, 0, pd), // index for mean
				ind1 = Iypc(Globals.currYear - 1, p, 1, pd); // index for sd
			float old_val_swe = SXW_AVG.snowpack_water_eqv_avg[ind0];
			float old_val_depth = SXW_AVG.snowpack_depth_avg[ind0];

			SXW_AVG.snowpack_water_eqv_avg[ind0] = get_running_avg(SXW_AVG.snowpack_water_eqv_avg[ind0], val_swe);
			SXW_AVG.snowpack_water_eqv_avg[ind1] += get_running_sqr(old_val_swe, val_swe, SXW_AVG.snowpack_water_eqv_avg[ind0]);

			SXW_AVG.snowpack_depth_avg[ind0] = get_running_avg(SXW_AVG.snowpack_depth_avg[ind0], val_depth);
			SXW_AVG.snowpack_depth_avg[ind1] += get_running_sqr(old_val_depth, val_depth, SXW_AVG.snowpack_depth_avg[ind0]);

			if(Globals.currIter == Globals.runModelIterations){
				float std_swe = sqrt(SXW_AVG.snowpack_water_eqv_avg[ind1] / Globals.currIter);
				float std_depth = sqrt(SXW_AVG.snowpack_depth_avg[ind1] / Globals.currIter);

				sprintf(str, "%c%7.6f%c%7.6f%c%7.6f%c%7.6f", _Sep, SXW_AVG.snowpack_water_eqv_avg[ind0], _Sep, std_swe,
								_Sep, SXW_AVG.snowpack_depth_avg[ind0], _Sep, std_depth);
				strcat(sw_outstr, str);
			}
		}
		if(storeAllIterations){
			sprintf(str_iters, "%c%7.6f%c%7.6f", _Sep, val_swe, _Sep, val_depth);
			strcat(outstr_all_iters, str_iters);
		}

#else
	int delta;
	RealD *p;

	switch (pd)
	{
		case eSW_Day:
		delta = SW_Output[eSW_SnowPack].dy_row;
		p = p_rOUT[eSW_SnowPack][eSW_Day]; // set shorthand copy of 'p_rOUT' pointer
		p[delta + dy_nrow * 0] = SW_Model.simyear;
		p[delta + dy_nrow * 1] = SW_Model.doy;
		p[delta + dy_nrow * 2] = v->dysum.snowpack;
		p[delta + dy_nrow * 3] = v->dysum.snowdepth;
		SW_Output[eSW_SnowPack].dy_row++;
		break;
		case eSW_Week:
		delta = SW_Output[eSW_SnowPack].wk_row;
		p = p_rOUT[eSW_SnowPack][eSW_Week]; // set shorthand copy of 'p_rOUT' pointer
		p[delta + wk_nrow * 0] = SW_Model.simyear;
		p[delta + wk_nrow * 1] = (SW_Model.week + 1) - tOffset;
		p[delta + wk_nrow * 2] = v->wkavg.snowpack;
		p[delta + wk_nrow * 3] = v->wkavg.snowdepth;
		SW_Output[eSW_SnowPack].wk_row++;
		break;
		case eSW_Month:
		delta = SW_Output[eSW_SnowPack].mo_row;
		p = p_rOUT[eSW_SnowPack][eSW_Month]; // set shorthand copy of 'p_rOUT' pointer
		p[delta + mo_nrow * 0] = SW_Model.simyear;
		p[delta + mo_nrow * 1] = (SW_Model.month + 1) - tOffset;
		p[delta + mo_nrow * 2] = v->moavg.snowpack;
		p[delta + mo_nrow * 3] = v->moavg.snowdepth;
		SW_Output[eSW_SnowPack].mo_row++;
		break;
		case eSW_Year:
		delta = SW_Output[eSW_SnowPack].yr_row;
		p = p_rOUT[eSW_SnowPack][eSW_Year]; // set shorthand copy of 'p_rOUT' pointer
		p[delta + yr_nrow * 0] = SW_Model.simyear;
		p[delta + yr_nrow * 1] = v->yravg.snowpack;
		p[delta + yr_nrow * 2] = v->yravg.snowdepth;
		SW_Output[eSW_SnowPack].yr_row++;
		break;
	}
#endif
}

void get_deepswc(OutPeriod pd)
{
	/* --------------------------------------------------- */
	SW_SOILWAT *v = &SW_Soilwat;


	#if !defined(STEPWAT) && !defined(RSOILWAT)
		char str[OUTSTRLEN];
		RealD val = SW_MISSING;
		get_outstrleader(pd);

	#elif defined(STEPWAT)
		char str[OUTSTRLEN];
		char str_iters[OUTSTRLEN];
		RealD val = SW_MISSING;
		TimeInt p = 0;
		if ((isPartialSoilwatOutput == FALSE && Globals.currIter == Globals.runModelIterations) || storeAllIterations)
		{
			get_outstrleader(pd);
		}
	#endif

	#if !defined(STEPWAT) && !defined(RSOILWAT)
	switch (pd)
	{
	case eSW_Day:
		val = v->dysum.deep;
		break;
	case eSW_Week:
		val = v->wkavg.deep;
		break;
	case eSW_Month:
		val = v->moavg.deep;
		break;
	case eSW_Year:
		val = v->yravg.deep;
		break;
	}
	sprintf(str, "%c%7.6f", _Sep, val);
	strcat(sw_outstr, str);

	#elif defined(STEPWAT)
		switch (pd)
		{
			case eSW_Day:
				p = SW_Model.doy-1;
				val = v->dysum.deep;
				break;
			case eSW_Week:
				val = v->wkavg.deep;
				p = SW_Model.week-tOffset;
				break;
			case eSW_Month:
				p = SW_Model.month-tOffset;
				val = v->moavg.pet;
				break;
			case eSW_Year:
				val = v->yravg.deep;
				p = Globals.currYear - 1;
				break;
		}

		if (isPartialSoilwatOutput == FALSE)
		{
			int
				ind0 = Iypc(Globals.currYear - 1, p, 0, pd), // index for mean
				ind1 = Iypc(Globals.currYear - 1, p, 1, pd); // index for sd
			float old_val = SXW_AVG.deepswc_avg[ind0];

			SXW_AVG.deepswc_avg[ind0] = get_running_avg(SXW_AVG.deepswc_avg[ind0], val);
			SXW_AVG.deepswc_avg[ind1] += get_running_sqr(old_val, val, SXW_AVG.deepswc_avg[ind0]);

			if(Globals.currIter == Globals.runModelIterations){
				float std = sqrt(SXW_AVG.deepswc_avg[ind1] / Globals.currIter);

				sprintf(str, "%c%7.6f%c%7.6f", _Sep, SXW_AVG.deepswc_avg[ind0], _Sep, std);
				strcat(sw_outstr, str);
			}
		}
		if(storeAllIterations){
			sprintf(str_iters, "%c%7.6f", _Sep, val);
			strcat(outstr_all_iters, str_iters);
		}

#else
	switch (pd)
	{
		case eSW_Day:
		p_rOUT[eSW_DeepSWC][eSW_Day][SW_Output[eSW_DeepSWC].dy_row + dy_nrow * 0] = SW_Model.simyear;
		p_rOUT[eSW_DeepSWC][eSW_Day][SW_Output[eSW_DeepSWC].dy_row + dy_nrow * 1] = SW_Model.doy;
		p_rOUT[eSW_DeepSWC][eSW_Day][SW_Output[eSW_DeepSWC].dy_row + dy_nrow * 2] = v->dysum.deep;
		SW_Output[eSW_DeepSWC].dy_row++;
		break;
		case eSW_Week:
		p_rOUT[eSW_DeepSWC][eSW_Week][SW_Output[eSW_DeepSWC].wk_row + wk_nrow * 0] = SW_Model.simyear;
		p_rOUT[eSW_DeepSWC][eSW_Week][SW_Output[eSW_DeepSWC].wk_row + wk_nrow * 1] = (SW_Model.week + 1) - tOffset;
		p_rOUT[eSW_DeepSWC][eSW_Week][SW_Output[eSW_DeepSWC].wk_row + wk_nrow * 2] = v->wkavg.deep;
		SW_Output[eSW_DeepSWC].wk_row++;
		break;
		case eSW_Month:
		p_rOUT[eSW_DeepSWC][eSW_Month][SW_Output[eSW_DeepSWC].mo_row + mo_nrow * 0] = SW_Model.simyear;
		p_rOUT[eSW_DeepSWC][eSW_Month][SW_Output[eSW_DeepSWC].mo_row + mo_nrow * 1] = (SW_Model.month + 1) - tOffset;
		p_rOUT[eSW_DeepSWC][eSW_Month][SW_Output[eSW_DeepSWC].mo_row + mo_nrow * 2] = v->moavg.deep;
		SW_Output[eSW_DeepSWC].mo_row++;
		break;
		case eSW_Year:
		p_rOUT[eSW_DeepSWC][eSW_Year][SW_Output[eSW_DeepSWC].yr_row + yr_nrow * 0] = SW_Model.simyear;
		p_rOUT[eSW_DeepSWC][eSW_Year][SW_Output[eSW_DeepSWC].yr_row + yr_nrow * 1] = v->yravg.deep;
		SW_Output[eSW_DeepSWC].yr_row++;
		break;
	}
#endif
}

void get_soiltemp(OutPeriod pd)
{
	/* --------------------------------------------------- */
	LyrIndex i;
	SW_SOILWAT *v = &SW_Soilwat;

	#if !defined(STEPWAT) && !defined(RSOILWAT)
		char str[OUTSTRLEN];
		RealD val = SW_MISSING;
		get_outstrleader(pd);

	#elif defined(STEPWAT)
		char str[OUTSTRLEN];
		char str_iters[OUTSTRLEN];
		RealD val = SW_MISSING;
		TimeInt p = 0;
		if ((isPartialSoilwatOutput == FALSE && Globals.currIter == Globals.runModelIterations) || storeAllIterations)
		{
			get_outstrleader(pd);
		}
	#endif

	#if !defined(STEPWAT) && !defined(RSOILWAT)
	ForEachSoilLayer(i)
	{
		switch (pd)
		{
		case eSW_Day:
			val = v->dysum.sTemp[i];
			break;
		case eSW_Week:
			val = v->wkavg.sTemp[i];
			break;
		case eSW_Month:
			val = v->moavg.sTemp[i];
			break;
		case eSW_Year:
			val = v->yravg.sTemp[i];
			break;
		}
		sprintf(str, "%c%7.6f", _Sep, val);
		strcat(sw_outstr, str);
	}

	#elif defined(STEPWAT)
	ForEachSoilLayer(i){
		switch (pd)
		{
			case eSW_Day:
				p = SW_Model.doy-1;
				val = v->dysum.sTemp[i];
				break;
			case eSW_Week:
				val = v->wkavg.sTemp[i];
				p = SW_Model.week-tOffset;
				break;
			case eSW_Month:
				p = SW_Model.month-tOffset;
				val = v->moavg.sTemp[i];
				break;
			case eSW_Year:
				val = v->yravg.sTemp[i];
				p = Globals.currYear - 1;
				break;
		}

		if (isPartialSoilwatOutput == FALSE)
		{
			int
				indl0 = Iylp(Globals.currYear - 1, i, p, pd, 0), // index for mean
				indl1 = Iylp(Globals.currYear - 1, i, p, pd, 1); // index for sd
			float old_val = SXW_AVG.soiltemp_avg[indl0];

			SXW_AVG.soiltemp_avg[indl0] = get_running_avg(SXW_AVG.soiltemp_avg[indl0], val);
			SXW_AVG.soiltemp_avg[indl1] += get_running_sqr(old_val, val, SXW_AVG.soiltemp_avg[indl0]);

			if(Globals.currIter == Globals.runModelIterations){
				float std = sqrt(SXW_AVG.soiltemp_avg[indl1] / Globals.currIter);

				sprintf(str, "%c%7.6f%c%7.6f", _Sep, SXW_AVG.soiltemp_avg[indl0], _Sep, std);
				strcat(sw_outstr, str);
			}
		}
		if(storeAllIterations){
			sprintf(str_iters, "%c%7.6f", _Sep, val);
			strcat(outstr_all_iters, str_iters);
		}
	}

#else
	switch (pd)
	{
		case eSW_Day:
		p_rOUT[eSW_SoilTemp][eSW_Day][SW_Output[eSW_SoilTemp].dy_row + dy_nrow * 0] = SW_Model.simyear;
		p_rOUT[eSW_SoilTemp][eSW_Day][SW_Output[eSW_SoilTemp].dy_row + dy_nrow * 1] = SW_Model.doy;
		ForEachSoilLayer(i)
		{
			p_rOUT[eSW_SoilTemp][eSW_Day][SW_Output[eSW_SoilTemp].dy_row + dy_nrow * (i + 2)] = v->dysum.sTemp[i];
		}
		SW_Output[eSW_SoilTemp].dy_row++;
		break;
		case eSW_Week:
		p_rOUT[eSW_SoilTemp][eSW_Week][SW_Output[eSW_SoilTemp].wk_row + wk_nrow * 0] = SW_Model.simyear;
		p_rOUT[eSW_SoilTemp][eSW_Week][SW_Output[eSW_SoilTemp].wk_row + wk_nrow * 1] = (SW_Model.week + 1) - tOffset;
		ForEachSoilLayer(i)
		{
			p_rOUT[eSW_SoilTemp][eSW_Week][SW_Output[eSW_SoilTemp].wk_row + wk_nrow * (i + 2)] = v->wkavg.sTemp[i];
		}
		SW_Output[eSW_SoilTemp].wk_row++;
		break;
		case eSW_Month:
		p_rOUT[eSW_SoilTemp][eSW_Month][SW_Output[eSW_SoilTemp].mo_row + mo_nrow * 0] = SW_Model.simyear;
		p_rOUT[eSW_SoilTemp][eSW_Month][SW_Output[eSW_SoilTemp].mo_row + mo_nrow * 1] = (SW_Model.month + 1) - tOffset;
		ForEachSoilLayer(i)
		{
			p_rOUT[eSW_SoilTemp][eSW_Month][SW_Output[eSW_SoilTemp].mo_row + mo_nrow * (i + 2)] = v->moavg.sTemp[i];
		}
		SW_Output[eSW_SoilTemp].mo_row++;
		break;
		case eSW_Year:
		p_rOUT[eSW_SoilTemp][eSW_Year][SW_Output[eSW_SoilTemp].yr_row + yr_nrow * 0] = SW_Model.simyear;
		ForEachSoilLayer(i)
		{
			p_rOUT[eSW_SoilTemp][eSW_Year][SW_Output[eSW_SoilTemp].yr_row + yr_nrow * (i + 1)] = v->yravg.sTemp[i];
		}
		SW_Output[eSW_SoilTemp].yr_row++;
		break;
	}
#endif
}
