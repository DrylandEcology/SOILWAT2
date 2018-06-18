/********************************************************/
/********************************************************/
/*  Source file: SW_Output_SOILWAT2.c
  Type: module
  Application: SOILWAT - soilwater dynamics simulator
  Purpose: define `get_XXX` functions

  History:
  2018 June 04 (drs) moved output formatter `get_XXX` functions from
     `SW_Output.c` to dedicated `SW_Output_get_functions.c`
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
#endif

#ifdef STEPWAT
#include "../sxw.h"
#include "../ST_globals.h"
#endif

// Array-based output declarations:
#ifdef SW_OUTARRAY
#include "SW_Output_outarray.h"
#endif

// Text-based output declarations:
#ifdef SW_OUTTEXT
#include "SW_Output_outtext.h"
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

extern SW_OUTPUT SW_Output[];
extern IntUS ncol_OUT[];

#ifdef STEPWAT
extern Bool prepare_IterationSummary;
extern SXW_t SXW; // structure to store values in and pass back to STEPPE
#endif

// Text-based output: defined in `SW_Output_outtext.c`:
#ifdef SW_OUTTEXT
extern SW_FILE_STATUS SW_OutFiles;
extern char _Sep;
extern char sw_outstr[];
extern Bool print_IterationSummary;
#endif
#ifdef STEPWAT
extern char sw_outstr_agg[];
#endif


// Array-based output: defined in `SW_Output_outarray.c`
#ifdef SW_OUTARRAY
extern RealD *p_OUT[SW_OUTNKEYS][SW_OUTNPERIODS];
extern IntUS ncol_TimeOUT[];
extern IntUS nrow_OUT[];
extern IntUS irow_OUT[];
#endif
#ifdef STEPWAT
extern RealD *p_OUTsd[SW_OUTNKEYS][SW_OUTNPERIODS];
#endif




/* =================================================== */
/* =================================================== */
/*             Function Definitions                    */
/*             (declared in SW_Output.h)               */
/* --------------------------------------------------- */

void get_none(OutPeriod pd)
{
	/* --------------------------------------------------- */
	/* output routine for quantities that aren't yet implemented
	 * this just gives the main output loop something to call,
	 * rather than an empty pointer.
	 */
	if (pd) {}
}


// NOTE: `get_co2effects` uses a different order of vegetation types than the rest of SoilWat!!!
void get_co2effects(OutPeriod pd) {
	int k;
	RealD biomass_total = 0., biolive_total = 0.;
	SW_VEGPROD *v = &SW_VegProd;
	SW_VEGPROD_OUTPUTS *vo = NULL;

	#ifdef SW_OUTARRAY
	RealD *p;
	get_outvalleader(eSW_CO2Effects, pd);
	p = p_OUT[eSW_CO2Effects][pd];
	#endif

	set_VEGPROD_aggslot(pd, &vo);

	ForEachVegType(k)
	{
		biomass_total += vo->veg[k].biomass;
		biolive_total += vo->veg[k].biolive;
	}

	#ifdef SW_OUTTEXT
	sw_outstr[0] = '\0';
	sprintf(sw_outstr,
		"%c%.*f%c%.*f%c%.*f%c%.*f%c%.*f%c%.*f%c%.*f%c%.*f%c%.*f%c%.*f%c%.*f%c%.*f%c%.*f%c%.*f%c%.*f%c%.*f%c%.*f%c%.*f",
		_Sep, OUT_DIGITS, vo->veg[SW_GRASS].biomass,
		_Sep, OUT_DIGITS, vo->veg[SW_SHRUB].biomass,
		_Sep, OUT_DIGITS, vo->veg[SW_TREES].biomass,
		_Sep, OUT_DIGITS, vo->veg[SW_FORBS].biomass,
		_Sep, OUT_DIGITS, biomass_total,
		_Sep, OUT_DIGITS, vo->veg[SW_GRASS].biolive,
		_Sep, OUT_DIGITS, vo->veg[SW_SHRUB].biolive,
		_Sep, OUT_DIGITS, vo->veg[SW_TREES].biolive,
		_Sep, OUT_DIGITS, vo->veg[SW_FORBS].biolive,
		_Sep, OUT_DIGITS, biolive_total,
		_Sep, OUT_DIGITS, v->veg[SW_GRASS].co2_multipliers[BIO_INDEX][SW_Model.simyear],
		_Sep, OUT_DIGITS, v->veg[SW_SHRUB].co2_multipliers[BIO_INDEX][SW_Model.simyear],
		_Sep, OUT_DIGITS, v->veg[SW_TREES].co2_multipliers[BIO_INDEX][SW_Model.simyear],
		_Sep, OUT_DIGITS, v->veg[SW_FORBS].co2_multipliers[BIO_INDEX][SW_Model.simyear],
		_Sep, OUT_DIGITS, v->veg[SW_GRASS].co2_multipliers[WUE_INDEX][SW_Model.simyear],
		_Sep, OUT_DIGITS, v->veg[SW_SHRUB].co2_multipliers[WUE_INDEX][SW_Model.simyear],
		_Sep, OUT_DIGITS, v->veg[SW_TREES].co2_multipliers[WUE_INDEX][SW_Model.simyear],
		_Sep, OUT_DIGITS, v->veg[SW_FORBS].co2_multipliers[WUE_INDEX][SW_Model.simyear]);
	#endif

	#ifdef SW_OUTARRAY
	p[iOUT(0, pd)] = vo->veg[SW_GRASS].biomass;
	p[iOUT(1, pd)] = vo->veg[SW_SHRUB].biomass;
	p[iOUT(2, pd)] = vo->veg[SW_TREES].biomass;
	p[iOUT(3, pd)] = vo->veg[SW_FORBS].biomass;
	p[iOUT(4, pd)] = biomass_total;
	p[iOUT(5, pd)] = vo->veg[SW_GRASS].biolive;
	p[iOUT(6, pd)] = vo->veg[SW_SHRUB].biolive;
	p[iOUT(7, pd)] = vo->veg[SW_TREES].biolive;
	p[iOUT(8, pd)] = vo->veg[SW_FORBS].biolive;
	p[iOUT(9, pd)] = biolive_total;

	// No averaging or summing required:
	p[iOUT(10, pd)] = v->veg[SW_GRASS].co2_multipliers[BIO_INDEX][SW_Model.simyear];
	p[iOUT(11, pd)] = v->veg[SW_SHRUB].co2_multipliers[BIO_INDEX][SW_Model.simyear];
	p[iOUT(12, pd)] = v->veg[SW_TREES].co2_multipliers[BIO_INDEX][SW_Model.simyear];
	p[iOUT(13, pd)] = v->veg[SW_FORBS].co2_multipliers[BIO_INDEX][SW_Model.simyear];
	p[iOUT(14, pd)] = v->veg[SW_GRASS].co2_multipliers[WUE_INDEX][SW_Model.simyear];
	p[iOUT(15, pd)] = v->veg[SW_SHRUB].co2_multipliers[WUE_INDEX][SW_Model.simyear];
	p[iOUT(16, pd)] = v->veg[SW_TREES].co2_multipliers[WUE_INDEX][SW_Model.simyear];
	p[iOUT(17, pd)] = v->veg[SW_FORBS].co2_multipliers[WUE_INDEX][SW_Model.simyear];
	#endif
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

	#ifdef SW_OUTTEXT
	char str[OUTSTRLEN];
	sw_outstr[0] = '\0';
	#endif

	#ifdef SW_OUTARRAY
	RealD *p;
	get_outvalleader(eSW_Estab, pd);
	p = p_OUT[eSW_Estab][pd];
	#endif

	i = (IntU) pd; // silence `-Wunused-parameter`

	for (i = 0; i < v->count; i++)
	{
		#ifdef SW_OUTTEXT
		sprintf(str, "%c%d", _Sep, v->parms[i]->estab_doy);
		strcat(sw_outstr, str);
		#endif

		#ifdef SW_OUTARRAY
		p[iOUT(i, pd)] = v->parms[i]->estab_doy;
		#endif
	}
}

void get_temp(OutPeriod pd)
{
	SW_WEATHER_OUTPUTS *vo = NULL;

	#ifdef SW_OUTARRAY
	RealD *p;
	get_outvalleader(eSW_Temp, pd);
	p = p_OUT[eSW_Temp][pd];
	#endif

	set_WEATHER_aggslot(pd, &vo);

	#ifdef SW_OUTTEXT
	sw_outstr[0] = '\0';
	sprintf(sw_outstr, "%c%.*f%c%.*f%c%.*f%c%.*f",
		_Sep, OUT_DIGITS, vo->temp_max,
		_Sep, OUT_DIGITS, vo->temp_min,
		_Sep, OUT_DIGITS, vo->temp_avg,
		_Sep, OUT_DIGITS, vo->surfaceTemp);
	#endif

	#ifdef SW_OUTARRAY
	p[iOUT(0, pd)] = vo->temp_max;
	p[iOUT(1, pd)] = vo->temp_min;
	p[iOUT(2, pd)] = vo->temp_avg;
	p[iOUT(3, pd)] = vo->surfaceTemp;
	#endif

	#if defined(STEPWAT)
	if (pd == eSW_Year) {
		// STEPWAT2 expects annual mean air temperature
		SXW.temp = vo->temp_avg;
	}
	#endif
}

void get_precip(OutPeriod pd)
{
	SW_WEATHER_OUTPUTS *vo = NULL;

	#ifdef SW_OUTARRAY
	RealD *p;
	get_outvalleader(eSW_Precip, pd);
	p = p_OUT[eSW_Precip][pd];
	#endif

	set_WEATHER_aggslot(pd, &vo);

	#ifdef SW_OUTTEXT
	sw_outstr[0] = '\0';
	sprintf(sw_outstr, "%c%.*f%c%.*f%c%.*f%c%.*f%c%.*f",
		_Sep, OUT_DIGITS, vo->ppt,
		_Sep, OUT_DIGITS, vo->rain,
		_Sep, OUT_DIGITS, vo->snow,
		_Sep, OUT_DIGITS, vo->snowmelt,
		_Sep, OUT_DIGITS, vo->snowloss);
	#endif

	#ifdef SW_OUTARRAY
	p[iOUT(0, pd)] = vo->ppt;
	p[iOUT(1, pd)] = vo->rain;
	p[iOUT(2, pd)] = vo->snow;
	p[iOUT(3, pd)] = vo->snowmelt;
	p[iOUT(4, pd)] = vo->snowloss;
	#endif

	#if defined(STEPWAT)
	// STEPWAT2 expects monthly and annual sum of precipitation
	if (pd == eSW_Month) {
		SXW.ppt_monthly[SW_Model.month - tOffset] = vo->ppt;
	}
	else if (pd == eSW_Year) {
		SXW.ppt = vo->ppt;
	}
	#endif
}

void get_vwcBulk(OutPeriod pd)
{
	LyrIndex i;
	SW_SOILWAT_OUTPUTS *vo = NULL;

	#ifdef SW_OUTTEXT
	char str[OUTSTRLEN];
	sw_outstr[0] = '\0';
	#endif

	#ifdef SW_OUTARRAY
	RealD *p;
	get_outvalleader(eSW_VWCBulk, pd);
	p = p_OUT[eSW_VWCBulk][pd];
	#endif

	set_SOILWAT_aggslot(pd, &vo);

	ForEachSoilLayer(i) {
		/* vwcBulk at this point is identical to swcBulk */
		#ifdef SW_OUTTEXT
		sprintf(str, "%c%.*f",
			_Sep, OUT_DIGITS, vo->vwcBulk[i] / SW_Site.lyr[i]->width);
		strcat(sw_outstr, str);
		#endif

		#ifdef SW_OUTARRAY
		p[iOUT(i, pd)] = vo->vwcBulk[i] / SW_Site.lyr[i]->width;
		#endif
	}
}

void get_vwcMatric(OutPeriod pd)
{
	LyrIndex i;
	RealD convert;
	SW_SOILWAT_OUTPUTS *vo = NULL;

	#ifdef SW_OUTTEXT
	char str[OUTSTRLEN];
	sw_outstr[0] = '\0';
	#endif

	#ifdef SW_OUTARRAY
	RealD *p;
	get_outvalleader(eSW_VWCMatric, pd);
	p = p_OUT[eSW_VWCMatric][pd];
	#endif

	set_SOILWAT_aggslot(pd, &vo);

	ForEachSoilLayer(i) {
		/* vwcMatric at this point is identical to swcBulk */
		convert = 1. / (1. - SW_Site.lyr[i]->fractionVolBulk_gravel) / SW_Site.lyr[i]->width;

		#ifdef SW_OUTTEXT
		sprintf(str, "%c%.*f",
			_Sep, OUT_DIGITS, vo->vwcMatric[i] * convert);
		strcat(sw_outstr, str);
		#endif

		#ifdef SW_OUTARRAY
		p[iOUT(i, pd)] = vo->vwcMatric[i] * convert;
		#endif
	}
}

void get_swa(OutPeriod pd)
{
	/* added 21-Oct-03, cwb */
	LyrIndex i;
	int k;
	SW_SOILWAT_OUTPUTS *vo = NULL;

	#ifdef SW_OUTTEXT
	char str[OUTSTRLEN];
	sw_outstr[0] = '\0';
	#endif

	#ifdef SW_OUTARRAY
	RealD *p;
	get_outvalleader(eSW_SWA, pd);
	p = p_OUT[eSW_SWA][pd];
	#endif

	set_SOILWAT_aggslot(pd, &vo);

	ForEachVegType(k)
	{
		ForEachSoilLayer(i)
		{
			#ifdef SW_OUTTEXT
			sprintf(str, "%c%.*f", _Sep, OUT_DIGITS, vo->SWA_VegType[k][i]);
			strcat(sw_outstr, str);
			#endif

			#ifdef SW_OUTARRAY
			p[iOUT2(i, k, pd)] = vo->SWA_VegType[k][i];
			#endif
		}
	}
}


void get_swcBulk(OutPeriod pd)
{
	/* added 21-Oct-03, cwb */
	LyrIndex i;
	SW_SOILWAT_OUTPUTS *vo = NULL;

	#ifdef SW_OUTTEXT
	char str[OUTSTRLEN];
	sw_outstr[0] = '\0';
	#endif

	#ifdef SW_OUTARRAY
	RealD *p;
	get_outvalleader(eSW_SWCBulk, pd);
	p = p_OUT[eSW_SWCBulk][pd];
	#endif

	set_SOILWAT_aggslot(pd, &vo);

	ForEachSoilLayer(i)
	{
		#ifdef SW_OUTTEXT
		sprintf(str, "%c%.*f", _Sep, OUT_DIGITS, vo->swcBulk[i]);
		strcat(sw_outstr, str);
		#endif

		#ifdef SW_OUTARRAY
		p[iOUT(i, pd)] = vo->swcBulk[i];
		#endif

		#if defined(STEPWAT)
		if (pd == eSW_Month) {
			// STEPWAT2 expects monthly mean SWCbulk by soil layer
			SXW.swc[Ilp(i, SW_Model.month - tOffset)] = vo->swcBulk[i];
		}
		#endif
	}
}

void get_swpMatric(OutPeriod pd)
{
	/* can't take arithmetic average of swp because it's
	 * exponential.  At this time (until I remember to look
	 * up whether harmonic or some other average is better
	 * and fix this) we're not averaging swp but converting
	 * the averaged swc.  This also avoids converting for
	 * each day.
	 *
	 * added 12-Oct-03, cwb */

	RealD val;
	LyrIndex i;
	SW_SOILWAT_OUTPUTS *vo = NULL;

	#ifdef SW_OUTTEXT
	char str[OUTSTRLEN];
	sw_outstr[0] = '\0';
	#endif

	#ifdef SW_OUTARRAY
	RealD *p;
	get_outvalleader(eSW_SWPMatric, pd);
	p = p_OUT[eSW_SWPMatric][pd];
	#endif

	set_SOILWAT_aggslot(pd, &vo);

	ForEachSoilLayer(i)
	{
		/* swpMatric at this point is identical to swcBulk */
		val = SW_SWCbulk2SWPmatric(SW_Site.lyr[i]->fractionVolBulk_gravel,
			vo->swpMatric[i], i);

		#ifdef SW_OUTTEXT
		sprintf(str, "%c%.*f", _Sep, OUT_DIGITS, val);
		strcat(sw_outstr, str);
		#endif

		#ifdef SW_OUTARRAY
		p[iOUT(i, pd)] = val;
		#endif
	}
}

void get_swaBulk(OutPeriod pd)
{

	LyrIndex i;
	SW_SOILWAT_OUTPUTS *vo = NULL;

	#ifdef SW_OUTTEXT
	char str[OUTSTRLEN];
	sw_outstr[0] = '\0';
	#endif

	#ifdef SW_OUTARRAY
	RealD *p;
	get_outvalleader(eSW_SWABulk, pd);
	p = p_OUT[eSW_SWABulk][pd];
	#endif

	set_SOILWAT_aggslot(pd, &vo);

	ForEachSoilLayer(i)
	{
		#ifdef SW_OUTTEXT
		sprintf(str, "%c%.*f", _Sep, OUT_DIGITS, vo->swaBulk[i]);
		strcat(sw_outstr, str);
		#endif

		#ifdef SW_OUTARRAY
		p[iOUT(i, pd)] = vo->swaBulk[i];
		#endif
	}
}

void get_swaMatric(OutPeriod pd)
{
	LyrIndex i;
	RealD convert;
	SW_SOILWAT_OUTPUTS *vo = NULL;

	#ifdef SW_OUTTEXT
	char str[OUTSTRLEN];
	sw_outstr[0] = '\0';
	#endif

	#ifdef SW_OUTARRAY
	RealD *p;
	get_outvalleader(eSW_SWAMatric, pd);
	p = p_OUT[eSW_SWAMatric][pd];
	#endif

	set_SOILWAT_aggslot(pd, &vo);

	ForEachSoilLayer(i)
	{
		/* swaMatric at this point is identical to swaBulk */
		convert = 1. / (1. - SW_Site.lyr[i]->fractionVolBulk_gravel);

		#ifdef SW_OUTTEXT
		sprintf(str, "%c%.*f", _Sep, OUT_DIGITS, vo->swaMatric[i] * convert);
		strcat(sw_outstr, str);
		#endif

		#ifdef SW_OUTARRAY
		p[iOUT(i, pd)] = vo->swaMatric[i] * convert;
		#endif
	}
}

void get_surfaceWater(OutPeriod pd)
{
	SW_SOILWAT_OUTPUTS *vo = NULL;

	#ifdef SW_OUTARRAY
	RealD *p;
	get_outvalleader(eSW_SurfaceWater, pd);
	p = p_OUT[eSW_SurfaceWater][pd];
	#endif

	set_SOILWAT_aggslot(pd, &vo);

	#ifdef SW_OUTTEXT
	sw_outstr[0] = '\0';
	sprintf(sw_outstr, "%c%.*f", _Sep, OUT_DIGITS, vo->surfaceWater);
	#endif

	#ifdef SW_OUTARRAY
	p[iOUT(0, pd)] = vo->surfaceWater;
	#endif
}

void get_runoffrunon(OutPeriod pd) {
	RealD net;
	SW_WEATHER_OUTPUTS *vo = NULL;

	#ifdef SW_OUTARRAY
	RealD *p;
	get_outvalleader(eSW_Runoff, pd);
	p = p_OUT[eSW_Runoff][pd];
	#endif

	set_WEATHER_aggslot(pd, &vo);

	net = vo->surfaceRunoff + vo->snowRunoff - vo->surfaceRunon;

	#ifdef SW_OUTTEXT
	sw_outstr[0] = '\0';
	sprintf(sw_outstr, "%c%.*f%c%.*f%c%.*f%c%.*f",
		_Sep, OUT_DIGITS, net,
		_Sep, OUT_DIGITS, vo->surfaceRunoff,
		_Sep, OUT_DIGITS, vo->snowRunoff,
		_Sep, OUT_DIGITS, vo->surfaceRunon);
	#endif

	#ifdef SW_OUTARRAY
	p[iOUT(0, pd)] = net;
	p[iOUT(1, pd)] = vo->surfaceRunoff;
	p[iOUT(2, pd)] = vo->snowRunoff;
	p[iOUT(3, pd)] = vo->surfaceRunon;
	#endif
}

void get_transp(OutPeriod pd)
{
	LyrIndex i;
	int k;
	SW_SOILWAT_OUTPUTS *vo = NULL;

	#ifdef SW_OUTTEXT
	char str[OUTSTRLEN];
	sw_outstr[0] = '\0';
	#endif

	#ifdef SW_OUTARRAY
	RealD *p;
	get_outvalleader(eSW_Transp, pd);
	p = p_OUT[eSW_Transp][pd];
	#endif

	set_SOILWAT_aggslot(pd, &vo);

	/* total transpiration */
	ForEachSoilLayer(i)
	{
		#ifdef SW_OUTTEXT
		sprintf(str, "%c%.*f", _Sep, OUT_DIGITS, vo->transp_total[i]);
		strcat(sw_outstr, str);
		#endif

		#ifdef SW_OUTARRAY
		p[iOUT(i, pd)] = vo->transp_total[i];
		#endif

		#if defined(STEPWAT)
		if (pd == eSW_Month) {
			// STEPWAT2 expects monthly sum of transpiration by soil layer
			// see function `_transp_contribution_by_group`
			SXW.transpTotal[Ilp(i, SW_Model.month - tOffset)] = vo->transp_total[i];
		}
		#endif
	}

	/* transpiration for each vegetation type */
	ForEachVegType(k)
	{
		ForEachSoilLayer(i)
		{
			#ifdef SW_OUTTEXT
			sprintf(str, "%c%.*f", _Sep, OUT_DIGITS, vo->transp[k][i]);
			strcat(sw_outstr, str);
			#endif

			#ifdef SW_OUTARRAY
			p[iOUT2(i, k + 1, pd)] = vo->transp[k][i]; // k + 1 because of total transp.
			#endif

			#if defined(STEPWAT)
			if (pd == eSW_Month) {
				// STEPWAT2 expects monthly sum of transpiration by soil layer
				// see function `_transp_contribution_by_group`
				SXW.transpVeg[k][Ilp(i, SW_Model.month - tOffset)] = vo->transp[k][i];
			}
			#endif
		}
	}
}


void get_evapSoil(OutPeriod pd)
{
	LyrIndex i;
	SW_SOILWAT_OUTPUTS *vo = NULL;

	#ifdef SW_OUTTEXT
	char str[OUTSTRLEN];
	sw_outstr[0] = '\0';
	#endif

	#ifdef SW_OUTARRAY
	RealD *p;
	get_outvalleader(eSW_EvapSoil, pd);
	p = p_OUT[eSW_EvapSoil][pd];
	#endif

	set_SOILWAT_aggslot(pd, &vo);

	ForEachEvapLayer(i)
	{
		#ifdef SW_OUTTEXT
		sprintf(str, "%c%.*f", _Sep, OUT_DIGITS, vo->evap[i]);
		strcat(sw_outstr, str);
		#endif

		#ifdef SW_OUTARRAY
		p[iOUT(i, pd)] = vo->evap[i];
		#endif
	}
}

void get_evapSurface(OutPeriod pd)
{
	int k;
	SW_SOILWAT_OUTPUTS *vo = NULL;

	#ifdef SW_OUTTEXT
	char str[OUTSTRLEN];
	sw_outstr[0] = '\0';
	#endif

	#ifdef SW_OUTARRAY
	RealD *p;
	get_outvalleader(eSW_EvapSurface, pd);
	p = p_OUT[eSW_EvapSurface][pd];
	#endif

	set_SOILWAT_aggslot(pd, &vo);

	#ifdef SW_OUTTEXT
	sprintf(sw_outstr, "%c%.*f", _Sep, OUT_DIGITS, vo->total_evap);
	#endif

	#ifdef SW_OUTARRAY
	p[iOUT(0, pd)] = vo->total_evap;
	#endif

	ForEachVegType(k) {
		#ifdef SW_OUTTEXT
		sprintf(str, "%c%.*f", _Sep, OUT_DIGITS, vo->evap_veg[k]);
		strcat(sw_outstr, str);
		#endif

		#ifdef SW_OUTARRAY
		p[iOUT(k + 1, pd)] = vo->evap_veg[k];
		#endif
	}

	#ifdef SW_OUTTEXT
	sprintf(str, "%c%.*f%c%.*f",
		_Sep, OUT_DIGITS, vo->litter_evap,
		_Sep, OUT_DIGITS, vo->surfaceWater_evap);
	strcat(sw_outstr, str);
	#endif

	#ifdef SW_OUTARRAY
	p[iOUT(NVEGTYPES + 1, pd)] = vo->litter_evap;
	p[iOUT(NVEGTYPES + 2, pd)] = vo->surfaceWater_evap;
	#endif
}

void get_interception(OutPeriod pd)
{
	int k;
	SW_SOILWAT_OUTPUTS *vo = NULL;

	#ifdef SW_OUTTEXT
	char str[OUTSTRLEN];
	sw_outstr[0] = '\0';
	#endif

	#ifdef SW_OUTARRAY
	RealD *p;
	get_outvalleader(eSW_Interception, pd);
	p = p_OUT[eSW_Interception][pd];
	#endif

	set_SOILWAT_aggslot(pd, &vo);

	#ifdef SW_OUTTEXT
	sprintf(sw_outstr, "%c%.*f", _Sep, OUT_DIGITS, vo->total_int);
	#endif

	#ifdef SW_OUTARRAY
	p[iOUT(0, pd)] = vo->total_int;
	#endif

	ForEachVegType(k) {
		#ifdef SW_OUTTEXT
		sprintf(str, "%c%.*f", _Sep, OUT_DIGITS, vo->int_veg[k]);
		strcat(sw_outstr, str);
		#endif

		#ifdef SW_OUTARRAY
		p[iOUT(k + 1, pd)] = vo->int_veg[k];
		#endif
	}

	#ifdef SW_OUTTEXT
	sprintf(str, "%c%.*f", _Sep, OUT_DIGITS, vo->litter_int);
	strcat(sw_outstr, str);
	#endif

	#ifdef SW_OUTARRAY
	p[iOUT(NVEGTYPES + 1, pd)] = vo->litter_int;
	#endif
}

void get_soilinf(OutPeriod pd)
{
	/* 20100202 (drs) added */
	/* 20110219 (drs) added runoff */
	/* 12/13/2012	(clk)	moved runoff, now named snowRunoff, to get_runoffrunon(); */
	SW_WEATHER_OUTPUTS *vo = NULL;

	#ifdef SW_OUTARRAY
	RealD *p;
	get_outvalleader(eSW_SoilInf, pd);
	p = p_OUT[eSW_SoilInf][pd];
	#endif

	set_WEATHER_aggslot(pd, &vo);

	#ifdef SW_OUTTEXT
	sw_outstr[0] = '\0';
	sprintf(sw_outstr, "%c%.*f", _Sep, OUT_DIGITS, vo->soil_inf);
	#endif

	#ifdef SW_OUTARRAY
	p[iOUT(0, pd)] = vo->soil_inf;
	#endif
}

void get_lyrdrain(OutPeriod pd)
{
	/* 20100202 (drs) added */
	LyrIndex i;
	SW_SOILWAT_OUTPUTS *vo = NULL;

	#ifdef SW_OUTTEXT
	char str[OUTSTRLEN];
	sw_outstr[0] = '\0';
	#endif

	#ifdef SW_OUTARRAY
	RealD *p;
	get_outvalleader(eSW_LyrDrain, pd);
	p = p_OUT[eSW_LyrDrain][pd];
	#endif

	set_SOILWAT_aggslot(pd, &vo);

	for (i = 0; i < SW_Site.n_layers - 1; i++)
	{
		#ifdef SW_OUTTEXT
		sprintf(str, "%c%.*f", _Sep, OUT_DIGITS, vo->lyrdrain[i]);
		strcat(sw_outstr, str);
		#endif

		#ifdef SW_OUTARRAY
		p[iOUT(i, pd)] = vo->lyrdrain[i];
		#endif
	}
}

void get_hydred(OutPeriod pd)
{
	/* 20101020 (drs) added */
	LyrIndex i;
	int k;
	SW_SOILWAT_OUTPUTS *vo = NULL;

	#ifdef SW_OUTTEXT
	char str[OUTSTRLEN];
	sw_outstr[0] = '\0';
	#endif

	#ifdef SW_OUTARRAY
	RealD *p;
	get_outvalleader(eSW_HydRed, pd);
	p = p_OUT[eSW_HydRed][pd];
	#endif

	set_SOILWAT_aggslot(pd, &vo);

	/* total hydraulic redistribution */
	ForEachSoilLayer(i)
	{
		#ifdef SW_OUTTEXT
		sprintf(str, "%c%.*f", _Sep, OUT_DIGITS, vo->hydred_total[i]);
		strcat(sw_outstr, str);
		#endif

		#ifdef SW_OUTARRAY
		p[iOUT(i, pd)] = vo->hydred_total[i];
		#endif
	}

	/* hydraulic redistribution for each vegetation type */
	ForEachVegType(k)
	{
		ForEachSoilLayer(i)
		{
			#ifdef SW_OUTTEXT
			sprintf(str, "%c%.*f", _Sep, OUT_DIGITS, vo->hydred[k][i]);
			strcat(sw_outstr, str);
			#endif

			#ifdef SW_OUTARRAY
			p[iOUT2(i, k + 1, pd)] = vo->hydred[k][i]; // k + 1 because of total hydred
			#endif
		}
	}
}

void get_aet(OutPeriod pd)
{
	SW_SOILWAT_OUTPUTS *vo = NULL;

	#ifdef SW_OUTARRAY
	RealD *p;
	get_outvalleader(eSW_AET, pd);
	p = p_OUT[eSW_AET][pd];
	#endif

	set_SOILWAT_aggslot(pd, &vo);

	#ifdef SW_OUTTEXT
	sw_outstr[0] = '\0';
	sprintf(sw_outstr, "%c%.*f", _Sep, OUT_DIGITS, vo->aet);
	#endif

	#ifdef SW_OUTARRAY
	p[iOUT(0, pd)] = vo->aet;
	#endif

	#if defined(STEPWAT)
	if (pd == eSW_Year) {
		// STEPWAT2 expects annual sum of actual evapotranspiration
		SXW.aet = vo->aet;
	}
	#endif
}

void get_pet(OutPeriod pd)
{
	SW_SOILWAT_OUTPUTS *vo = NULL;

	#ifdef SW_OUTARRAY
	RealD *p;
	get_outvalleader(eSW_PET, pd);
	p = p_OUT[eSW_PET][pd];
	#endif

	set_SOILWAT_aggslot(pd, &vo);

	#ifdef SW_OUTTEXT
	sw_outstr[0] = '\0';
	sprintf(sw_outstr, "%c%.*f", _Sep, OUT_DIGITS, vo->pet);
	#endif

	#ifdef SW_OUTARRAY
	p[iOUT(0, pd)] = vo->pet;
	#endif
}

void get_wetdays(OutPeriod pd)
{
	LyrIndex i;

	#ifdef SW_OUTTEXT
	char str[OUTSTRLEN];
	sw_outstr[0] = '\0';
	#endif

	#ifdef SW_OUTARRAY
	RealD *p;
	get_outvalleader(eSW_WetDays, pd);
	p = p_OUT[eSW_WetDays][pd];
	#endif

	if (pd == eSW_Day)
	{
		ForEachSoilLayer(i) {
			#ifdef SW_OUTTEXT
			sprintf(str, "%c%i", _Sep, (SW_Soilwat.is_wet[i]) ? 1 : 0);
			strcat(sw_outstr, str);
			#endif

			#ifdef SW_OUTARRAY
			p[iOUT(i, pd)] = (SW_Soilwat.is_wet[i]) ? 1 : 0;
			#endif
		}

	} else
	{
		SW_SOILWAT_OUTPUTS *vo = NULL;
		set_SOILWAT_aggslot(pd, &vo);

		ForEachSoilLayer(i) {
			#ifdef SW_OUTTEXT
			sprintf(str, "%c%i", _Sep, (int) vo->wetdays[i]);
			strcat(sw_outstr, str);
			#endif

			#ifdef SW_OUTARRAY
			p[iOUT(i, pd)] = (int) vo->wetdays[i];
			#endif
		}
	}

}

void get_snowpack(OutPeriod pd)
{
	SW_SOILWAT_OUTPUTS *vo = NULL;

	#ifdef SW_OUTARRAY
	RealD *p;
	get_outvalleader(eSW_SnowPack, pd);
	p = p_OUT[eSW_SnowPack][pd];
	#endif

	set_SOILWAT_aggslot(pd, &vo);

	#ifdef SW_OUTTEXT
	sw_outstr[0] = '\0';
	sprintf(sw_outstr, "%c%.*f%c%.*f",
		_Sep, OUT_DIGITS, vo->snowpack,
		_Sep, OUT_DIGITS, vo->snowdepth);
	#endif

	#ifdef SW_OUTARRAY
	p[iOUT(0, pd)] = vo->snowpack;
	p[iOUT(1, pd)] = vo->snowdepth;
	#endif
}

void get_deepswc(OutPeriod pd)
{
	SW_SOILWAT_OUTPUTS *vo = NULL;

	#ifdef SW_OUTARRAY
	RealD *p;
	get_outvalleader(eSW_DeepSWC, pd);
	p = p_OUT[eSW_DeepSWC][pd];
	#endif

	set_SOILWAT_aggslot(pd, &vo);

	#ifdef SW_OUTTEXT
	sw_outstr[0] = '\0';
	sprintf(sw_outstr, "%c%.*f", _Sep, OUT_DIGITS, vo->deep);
	#endif

	#ifdef SW_OUTARRAY
	p[iOUT(0, pd)] = vo->deep;
	#endif
}

void get_soiltemp(OutPeriod pd)
{
	LyrIndex i;
	SW_SOILWAT_OUTPUTS *vo = NULL;

	#ifdef SW_OUTTEXT
	char str[OUTSTRLEN];
	sw_outstr[0] = '\0';
	#endif

	#ifdef SW_OUTARRAY
	RealD *p;
	get_outvalleader(eSW_SoilTemp, pd);
	p = p_OUT[eSW_SoilTemp][pd];
	#endif

	set_SOILWAT_aggslot(pd, &vo);

	ForEachSoilLayer(i)
	{
		#ifdef SW_OUTTEXT
		sprintf(str, "%c%.*f", _Sep, OUT_DIGITS, vo->sTemp[i]);
		strcat(sw_outstr, str);
		#endif

		#ifdef SW_OUTARRAY
		p[iOUT(i, pd)] = vo->sTemp[i];
		#endif
	}
}
