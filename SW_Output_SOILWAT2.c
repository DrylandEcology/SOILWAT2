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

extern char _Sep; // defined in `SW_Output_core.c`: output delimiter
extern TimeInt tOffset; // defined in `SW_Output_core.c`: 1 or 0 means we're writing previous or current period
extern Bool bFlush_output; // defined in `SW_Output_core.c`: process partial period ?
extern char sw_outstr[OUTSTRLEN]; // defined in `SW_Output_core.c`: string with formatted output which is to be written to output files


/* =================================================== */
/* =================================================== */
/*             Function Definitions                    */
/*             (declared in SW_Output.h)               */
/* --------------------------------------------------- */


void get_co2effects(OutPeriod pd) {
	SW_VEGPROD *v = &SW_VegProd;

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


	char str[OUTSTRLEN];
	get_outstrleader(pd);

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
			break;
	}

	biomass_total = biomass_grass + biomass_shrub + biomass_tree + biomass_forb;
	biolive_total = biolive_grass + biolive_shrub + biolive_tree + biolive_forb;

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
}

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
void get_estab(OutPeriod pd)
{
	SW_VEGESTAB *v = &SW_VegEstab;
	IntU i;

	get_outstrleader(pd);
	char str[OUTSTRLEN];

	for (i = 0; i < v->count; i++)
	{
		sprintf(str, "%c%d", _Sep, v->parms[i]->estab_doy);
		strcat(sw_outstr, str);
	}
}

void get_temp(OutPeriod pd)
{
	SW_WEATHER *v = &SW_Weather;
	RealD v_avg = SW_MISSING;
	RealD v_min = SW_MISSING, v_max = SW_MISSING;
	RealD surfaceTempVal = SW_MISSING;
	char str[OUTSTRLEN];

  #ifdef SWDEBUG
  int debug = 0;
  #endif

  #ifdef SWDEBUG
  if (debug) swprintf("'get_temp': start for %s ... ", pd);
  #endif

	get_outstrleader(pd);

	switch (pd)
	{
	case eSW_Day:
		#ifdef SWDEBUG
		if (debug) swprintf("%ddoy ... ", SW_Model.doy);
		#endif
		v_max = v->dysum.temp_max;
		v_min = v->dysum.temp_min;
		v_avg = v->dysum.temp_avg;
		surfaceTempVal = v->dysum.surfaceTemp;
		break;

	case eSW_Week:
		#ifdef SWDEBUG
		if (debug) swprintf("%dwk ... ", (SW_Model.week + 1) - tOffset);
		#endif
		v_max = v->wkavg.temp_max;
		v_min = v->wkavg.temp_min;
		v_avg = v->wkavg.temp_avg;
		surfaceTempVal = v->wkavg.surfaceTemp;
		break;

	case eSW_Month:
		#ifdef SWDEBUG
		if (debug) swprintf("%dmon ... ", (SW_Model.month + 1) - tOffset);
		#endif
		v_max = v->moavg.temp_max;
		v_min = v->moavg.temp_min;
		v_avg = v->moavg.temp_avg;
		surfaceTempVal = v->moavg.surfaceTemp;
		break;

	case eSW_Year:
		#ifdef SWDEBUG
		if (debug) swprintf("%dyr ... ", SW_Model.simyear);
		#endif
		v_max = v->yravg.temp_max;
		v_min = v->yravg.temp_min;
		v_avg = v->yravg.temp_avg;
		surfaceTempVal = v->yravg.surfaceTemp;
		break;
	}

	sprintf(str, "%c%7.6f%c%7.6f%c%7.6f%c%7.6f",
		_Sep, v_max, _Sep, v_min, _Sep, v_avg, _Sep, surfaceTempVal);
	strcat(sw_outstr, str);

	#ifdef SWDEBUG
		if (debug) swprintf("completed\n");
	#endif
}

void get_precip(OutPeriod pd)
{
	/* --------------------------------------------------- */
	/* 	20091015 (drs) ppt is divided into rain and snow and all three values are output into precip */
	SW_WEATHER *v = &SW_Weather;
	RealD val_ppt = SW_MISSING, val_rain = SW_MISSING, val_snow = SW_MISSING,
		val_snowmelt = SW_MISSING, val_snowloss = SW_MISSING;
	char str[OUTSTRLEN];

	get_outstrleader(pd);

	switch(pd)
	{
	case eSW_Day:
		val_ppt = v->dysum.ppt;
		val_rain = v->dysum.rain;
		val_snow = v->dysum.snow;
		val_snowmelt = v->dysum.snowmelt;
		val_snowloss = v->dysum.snowloss;
		break;

	case eSW_Week:
		val_ppt = v->wkavg.ppt;
		val_rain = v->wkavg.rain;
		val_snow = v->wkavg.snow;
		val_snowmelt = v->wkavg.snowmelt;
		val_snowloss = v->wkavg.snowloss;
		break;

	case eSW_Month:
		val_ppt = v->moavg.ppt;
		val_rain = v->moavg.rain;
		val_snow = v->moavg.snow;
		val_snowmelt = v->moavg.snowmelt;
		val_snowloss = v->moavg.snowloss;
		break;

	case eSW_Year:
		val_ppt = v->yravg.ppt;
		val_rain = v->yravg.rain;
		val_snow = v->yravg.snow;
		val_snowmelt = v->yravg.snowmelt;
		val_snowloss = v->yravg.snowloss;
		break;
	}

	sprintf(str, "%c%7.6f%c%7.6f%c%7.6f%c%7.6f%c%7.6f",
		_Sep, val_ppt, _Sep, val_rain, _Sep, val_snow, _Sep, val_snowmelt,
		_Sep, val_snowloss);
	strcat(sw_outstr, str);
}

void get_vwcBulk(OutPeriod pd)
{
	/* --------------------------------------------------- */
	LyrIndex i;
	SW_SOILWAT *v = &SW_Soilwat;
	char str[OUTSTRLEN];
	RealD val;

	get_outstrleader(pd);

	ForEachSoilLayer(i) {
		switch (pd)
		{
			case eSW_Day:
				val = v->dysum.vwcBulk[i];
				break;

			case eSW_Week:
					val = v->wkavg.vwcBulk[i];
				break;

			case eSW_Month:
					val = v->moavg.vwcBulk[i];
				break;

			case eSW_Year:
					val = v->yravg.vwcBulk[i];
				break;
		}

		/* vwcBulk at this point is identical to swcBulk */
		val = val / SW_Site.lyr[i]->width;

		sprintf(str, "%c%7.6f", _Sep, val);
		strcat(sw_outstr, str);
	}
}

void get_vwcMatric(OutPeriod pd)
{
	/* --------------------------------------------------- */
	LyrIndex i;
	SW_SOILWAT *v = &SW_Soilwat;
	RealD val;
	char str[OUTSTRLEN];

	get_outstrleader(pd);

	ForEachSoilLayer(i) {
		switch (pd)
		{
			case eSW_Day:
				val = v->dysum.vwcMatric[i];
				break;

			case eSW_Week:
				val = v->wkavg.vwcMatric[i];
				break;

			case eSW_Month:
				val = v->moavg.vwcMatric[i];
				break;

			case eSW_Year:
				val = v->yravg.vwcMatric[i];
				break;
		}

		/* vwcMatric at this point is identical to swcBulk */
		val = val / (1. - SW_Site.lyr[i]->fractionVolBulk_gravel)
			/ SW_Site.lyr[i]->width;

		sprintf(str, "%c%7.6f", _Sep, val);
		strcat(sw_outstr, str);
	}
}

void get_swa(OutPeriod pd)
{
	/* --------------------------------------------------- */
	/* added 21-Oct-03, cwb */
	LyrIndex i;
	int k;
	SW_SOILWAT *v = &SW_Soilwat;
	RealF val[NVEGTYPES];
	char str[OUTSTRLEN];

	get_outstrleader(pd);

	ForEachSoilLayer(i)
	{
		ForEachVegType(k) { // need to go over all veg types for each layer
			switch (pd)
			{
			case eSW_Day:
				val[k] = v->dysum.SWA_VegType[k][i];
				break;
			case eSW_Week:
				val[k] = v->wkavg.SWA_VegType[k][i];
				break;
			case eSW_Month:
				val[k] = v->moavg.SWA_VegType[k][i];
				break;
			case eSW_Year:
				val[k] = v->yravg.SWA_VegType[k][i];
				break;
			}
		}

		// write values to string
		sprintf(str, "%c%7.6f%c%7.6f%c%7.6f%c%7.6f",
			_Sep, val[SW_TREES], _Sep, val[SW_SHRUB],
			_Sep, val[SW_FORBS], _Sep, val[SW_GRASS]);

		strcat(sw_outstr, str);
	}
}


void get_swcBulk(OutPeriod pd)
{
	/* --------------------------------------------------- */
	/* added 21-Oct-03, cwb */
	LyrIndex i;
	SW_SOILWAT *v = &SW_Soilwat;
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

	char str[OUTSTRLEN];
	RealD val = SW_MISSING;
	get_outstrleader(pd);

	ForEachSoilLayer(i)
	{
		switch (pd)
		{
		case eSW_Day:
			val = v->dysum.swpMatric[i];
			break;
		case eSW_Week:
			val = v->wkavg.swpMatric[i];
			break;
		case eSW_Month:
			val = v->moavg.swpMatric[i];
			break;
		case eSW_Year:
			val = v->yravg.swpMatric[i];
			break;
		}

		/* swpMatric at this point is identical to swcBulk */
		val = SW_SWCbulk2SWPmatric(SW_Site.lyr[i]->fractionVolBulk_gravel, val, i);

		sprintf(str, "%c%7.6f", _Sep, val);
		strcat(sw_outstr, str);
	}
}

void get_swaBulk(OutPeriod pd)
{
	/* --------------------------------------------------- */

	LyrIndex i;
	SW_SOILWAT *v = &SW_Soilwat;
	char str[OUTSTRLEN];
	RealD val = SW_MISSING;

	get_outstrleader(pd);

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
}

void get_swaMatric(OutPeriod pd)
{
	/* --------------------------------------------------- */

	LyrIndex i;
	SW_SOILWAT *v = &SW_Soilwat;
	char str[OUTSTRLEN];
	RealD val = SW_MISSING;

	get_outstrleader(pd);

	ForEachSoilLayer(i)
	{
		switch (pd)
		{
			case eSW_Day:
				val = v->dysum.swaMatric[i];
				break;
			case eSW_Week:
				val = v->wkavg.swaMatric[i];
				break;
			case eSW_Month:
				val = v->moavg.swaMatric[i];
				break;
			case eSW_Year:
				val = v->yravg.swaMatric[i];
				break;
		}

		/* swaMatric at this point is identical to swaBulk */
		val = val / (1. - SW_Site.lyr[i]->fractionVolBulk_gravel);

		sprintf(str, "%c%7.6f", _Sep, val);
		strcat(sw_outstr, str);
	}
}

void get_surfaceWater(OutPeriod pd)
{
	/* --------------------------------------------------- */
	SW_SOILWAT *v = &SW_Soilwat;
	char str[OUTSTRLEN];
	RealD val_surfacewater = SW_MISSING;

	get_outstrleader(pd);

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

}

void get_runoffrunon(OutPeriod pd) {
  /* --------------------------------------------------- */
  /* (12/13/2012) (clk) Added function to output runoff variables */

  SW_WEATHER *w = &SW_Weather;
  RealD val_netRunoff = SW_MISSING, val_surfaceRunoff = SW_MISSING,
      val_surfaceRunon = SW_MISSING, val_snowRunoff = SW_MISSING;
	char str[OUTSTRLEN];

	get_outstrleader(pd);

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

	sprintf(str, "%c%7.6f%c%7.6f%c%7.6f%c%7.6f",
		_Sep, val_netRunoff, _Sep, val_surfaceRunoff, _Sep, val_snowRunoff,
		_Sep, val_surfaceRunon);
	strcat(sw_outstr, str);
}

void get_transp(OutPeriod pd)
{
	/* --------------------------------------------------- */
	/* 10-May-02 (cwb) Added conditional code to interface
	 *           with STEPPE.
	 */
	LyrIndex i;
	int k;
	SW_SOILWAT *v = &SW_Soilwat;
	RealF val;
	char str[OUTSTRLEN];

	get_outstrleader(pd);

	/* total transpiration */
	ForEachSoilLayer(i)
	{
		switch (pd)
		{
			case eSW_Day:
				val = v->dysum.transp_total[i];
				break;
			case eSW_Week:
				val = v->wkavg.transp_total[i];
				break;
			case eSW_Month:
				val = v->moavg.transp_total[i];
				break;
			case eSW_Year:
				val = v->yravg.transp_total[i];
				break;
		}

		sprintf(str, "%c%7.6f", _Sep, val);
		strcat(sw_outstr, str);
	}

	/* transpiration for each vegetation type */
	ForEachVegType(k)
	{
		ForEachSoilLayer(i)
		{
			switch (pd)
			{
				case eSW_Day:
					val = v->dysum.transp[k][i];
					break;
				case eSW_Week:
					val = v->wkavg.transp[k][i];
					break;
				case eSW_Month:
					val = v->moavg.transp[k][i];
					break;
				case eSW_Year:
					val = v->yravg.transp[k][i];
					break;
			}

			sprintf(str, "%c%7.6f", _Sep, val);
			strcat(sw_outstr, str);
		}
	}
}


void get_evapSoil(OutPeriod pd)
{
	/* --------------------------------------------------- */
	LyrIndex i;
	SW_SOILWAT *v = &SW_Soilwat;
	char str[OUTSTRLEN];
	RealD val = SW_MISSING;

	get_outstrleader(pd);

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
}

void get_evapSurface(OutPeriod pd)
{
	/* --------------------------------------------------- */
	SW_SOILWAT *v = &SW_Soilwat;
	char str[OUTSTRLEN];
	RealD val_tot = SW_MISSING, val_tree = SW_MISSING, val_forb = SW_MISSING,
		val_shrub = SW_MISSING, val_grass = SW_MISSING, val_litter =
		SW_MISSING, val_water = SW_MISSING;

	get_outstrleader(pd);

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
		_Sep, val_tot, _Sep, val_tree, _Sep, val_shrub, _Sep, val_forb,
		_Sep, val_grass, _Sep, val_litter, _Sep, val_water);
	strcat(sw_outstr, str);
}

void get_interception(OutPeriod pd)
{
	/* --------------------------------------------------- */
	SW_SOILWAT *v = &SW_Soilwat;
	char str[OUTSTRLEN];
	RealD val_tot = SW_MISSING, val_tree = SW_MISSING, val_forb = SW_MISSING,
		val_shrub = SW_MISSING, val_grass = SW_MISSING, val_litter =
				SW_MISSING;

	get_outstrleader(pd);

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

	sprintf(str, "%c%7.6f%c%7.6f%c%7.6f%c%7.6f%c%7.6f%c%7.6f",
		_Sep, val_tot, _Sep, val_tree, _Sep, val_shrub, _Sep, val_forb,
		_Sep, val_grass, _Sep, val_litter);
	strcat(sw_outstr, str);
}

void get_soilinf(OutPeriod pd)
{
	/* --------------------------------------------------- */
	/* 20100202 (drs) added */
	/* 20110219 (drs) added runoff */
	/* 12/13/2012	(clk)	moved runoff, now named snowRunoff, to get_runoffrunon(); */
	SW_WEATHER *v = &SW_Weather;
	char str[OUTSTRLEN];
	RealD val_inf = SW_MISSING;

	get_outstrleader(pd);

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
}

void get_lyrdrain(OutPeriod pd)
{
	/* --------------------------------------------------- */
	/* 20100202 (drs) added */
	LyrIndex i;
	SW_SOILWAT *v = &SW_Soilwat;
	char str[OUTSTRLEN];
	RealD val = SW_MISSING;

	get_outstrleader(pd);

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

}

void get_hydred(OutPeriod pd)
{
	/* --------------------------------------------------- */
	/* 20101020 (drs) added */
	LyrIndex i;
	SW_SOILWAT *v = &SW_Soilwat;

	char str[OUTSTRLEN];
	RealD val_total = SW_MISSING;
	RealD val_tree = SW_MISSING;
	RealD val_shrub = SW_MISSING;
	RealD val_forb = SW_MISSING;
	RealD val_grass = SW_MISSING;
	get_outstrleader(pd);

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

		sprintf(str, "%c%7.6f%c%7.6f%c%7.6f%c%7.6f%c%7.6f",
			_Sep, val_total,
			_Sep, val_tree, _Sep, val_shrub, _Sep, val_forb, _Sep, val_grass);
		strcat(sw_outstr, str);
	}
}

void get_aet(OutPeriod pd)
{
	/* --------------------------------------------------- */
	SW_SOILWAT *v = &SW_Soilwat;
	char str[OUTSTRLEN];
	RealD val = SW_MISSING;

	get_outstrleader(pd);

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

	sprintf(str, "%c%7.6f", _Sep, val);
	strcat(sw_outstr, str);
}

void get_pet(OutPeriod pd)
{
	/* --------------------------------------------------- */
	SW_SOILWAT *v = &SW_Soilwat;
	char str[OUTSTRLEN];
	RealD val = SW_MISSING;

	get_outstrleader(pd);

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
}

void get_wetdays(OutPeriod pd)
{
	/* --------------------------------------------------- */
	LyrIndex i;
	int val;
	SW_SOILWAT *v = &SW_Soilwat;
	char str[OUTSTRLEN];

	get_outstrleader(pd);

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
			default:
				val = SW_MISSING;
				break;
		}

		sprintf(str, "%c%i", _Sep, val);
		strcat(sw_outstr, str);
	}
}

void get_snowpack(OutPeriod pd)
{
	/* --------------------------------------------------- */
	SW_SOILWAT *v = &SW_Soilwat;
	char str[OUTSTRLEN];
	RealD val_swe = SW_MISSING, val_depth = SW_MISSING;

	get_outstrleader(pd);

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

	sprintf(str, "%c%7.6f%c%7.6f",
		_Sep, val_swe, _Sep, val_depth);
	strcat(sw_outstr, str);
}

void get_deepswc(OutPeriod pd)
{
	/* --------------------------------------------------- */
	SW_SOILWAT *v = &SW_Soilwat;
	char str[OUTSTRLEN];
	RealD val = SW_MISSING;

	get_outstrleader(pd);

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
}

void get_soiltemp(OutPeriod pd)
{
	/* --------------------------------------------------- */
	LyrIndex i;
	SW_SOILWAT *v = &SW_Soilwat;
	char str[OUTSTRLEN];
	RealD val = SW_MISSING;

	get_outstrleader(pd);

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
}
