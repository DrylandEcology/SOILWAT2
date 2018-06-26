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
#include <math.h>
#include "../sxw.h"
#include "../ST_defines.h"
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

extern IntUS ncol_OUT[];

#ifdef STEPWAT
extern Bool prepare_IterationSummary;
extern ModelType Globals; // defined in `ST_Main.c`
extern SXW_t SXW; // structure to store values in and pass back to STEPPE
extern TimeInt tOffset; // defined in `SW_Output.c`
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
extern size_t nrow_OUT[];
extern size_t irow_OUT[];
#endif
#ifdef STEPWAT
extern RealD *p_OUTsd[SW_OUTNKEYS][SW_OUTNPERIODS];
#endif


/* =================================================== */
/* =================================================== */
/*             Private Function Declarations           */
/* --------------------------------------------------- */

#ifdef STEPWAT
static void format_IterationSummary(RealD *p, RealD *psd, OutPeriod pd,
	IntUS N);
static void format_IterationSummary2(RealD *p, RealD *psd, OutPeriod pd,
	IntUS N1, IntUS N2, IntUS offset);
#endif

/* =================================================== */
/* =================================================== */
/*             Private Function Definitions            */
/* --------------------------------------------------- */

#ifdef STEPWAT
static void format_IterationSummary(RealD *p, RealD *psd, OutPeriod pd, IntUS N)
{
	IntUS i;
	size_t n;
	RealD sd;
	char str[OUTSTRLEN];

	for (i = 0; i < N; i++)
	{
		n = iOUT(i, pd);
		sd = final_running_sd(Globals.runModelIterations, psd[n]);

		sprintf(str, "%c%.*f%c%.*f",
			_Sep, OUT_DIGITS, p[n], _Sep, OUT_DIGITS, sd);
		strcat(sw_outstr_agg, str);
	}
}

static void format_IterationSummary2(RealD *p, RealD *psd, OutPeriod pd,
	IntUS N1, IntUS N2, IntUS offset)
{
	IntUS k, i;
	size_t n;
	RealD sd;
	char str[OUTSTRLEN];

	for (k = 0; k < N1; k++)
	{
		for (i = 0; i < N2; i++)
		{
			n = iOUT2(i, k + offset, pd);
			sd = final_running_sd(Globals.runModelIterations, psd[n]);

			sprintf(str, "%c%.*f%c%.*f",
				_Sep, OUT_DIGITS, p[n], _Sep, OUT_DIGITS, sd);
			strcat(sw_outstr_agg, str);
		}
	}
}

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


//------ eSW_CO2Effects
// NOTE: `get_co2effects` uses a different order of vegetation types than the rest of SoilWat!!!
#ifdef SW_OUTTEXT
void get_co2effects_text(OutPeriod pd) {
	int k;
	RealD biomass_total = 0., biolive_total = 0.;
	SW_VEGPROD *v = &SW_VegProd;
	SW_VEGPROD_OUTPUTS *vo = SW_VegProd.p_oagg[pd];

	ForEachVegType(k)
	{
		biomass_total += vo->veg[k].biomass;
		biolive_total += vo->veg[k].biolive;
	}

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
}
#endif

#if defined(RSOILWAT)
void get_co2effects_mem(OutPeriod pd) {
	int k;
	RealD biomass_total = 0., biolive_total = 0.;
	SW_VEGPROD *v = &SW_VegProd;
	SW_VEGPROD_OUTPUTS *vo = SW_VegProd.p_oagg[pd];

	RealD *p = p_OUT[eSW_CO2Effects][pd];
	get_outvalleader(p, pd);

	ForEachVegType(k)
	{
		biomass_total += vo->veg[k].biomass;
		biolive_total += vo->veg[k].biolive;
	}

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
}

#elif defined(STEPWAT)
void get_co2effects_agg(OutPeriod pd) {
	int k;
	RealD biomass_total = 0., biolive_total = 0.;
	SW_VEGPROD *v = &SW_VegProd;
	SW_VEGPROD_OUTPUTS *vo = SW_VegProd.p_oagg[pd];

	RealD
		*p = p_OUT[eSW_CO2Effects][pd],
		*psd = p_OUTsd[eSW_CO2Effects][pd];

	ForEachVegType(k)
	{
		biomass_total += vo->veg[k].biomass;
		biolive_total += vo->veg[k].biolive;
	}

	do_running_agg(p, psd, iOUT(0, pd), Globals.currIter, vo->veg[SW_GRASS].biomass);
	do_running_agg(p, psd, iOUT(1, pd), Globals.currIter, vo->veg[SW_SHRUB].biomass);
	do_running_agg(p, psd, iOUT(2, pd), Globals.currIter, vo->veg[SW_TREES].biomass);
	do_running_agg(p, psd, iOUT(3, pd), Globals.currIter, vo->veg[SW_FORBS].biomass);
	do_running_agg(p, psd, iOUT(4, pd), Globals.currIter, biomass_total);
	do_running_agg(p, psd, iOUT(5, pd), Globals.currIter, vo->veg[SW_GRASS].biolive);
	do_running_agg(p, psd, iOUT(6, pd), Globals.currIter, vo->veg[SW_SHRUB].biolive);
	do_running_agg(p, psd, iOUT(7, pd), Globals.currIter, vo->veg[SW_TREES].biolive);
	do_running_agg(p, psd, iOUT(8, pd), Globals.currIter, vo->veg[SW_FORBS].biolive);
	do_running_agg(p, psd, iOUT(9, pd), Globals.currIter, biolive_total);

	do_running_agg(p, psd, iOUT(10, pd), Globals.currIter,
		v->veg[SW_GRASS].co2_multipliers[BIO_INDEX][SW_Model.simyear]);
	do_running_agg(p, psd, iOUT(11, pd), Globals.currIter,
		v->veg[SW_SHRUB].co2_multipliers[BIO_INDEX][SW_Model.simyear]);
	do_running_agg(p, psd, iOUT(12, pd), Globals.currIter,
		v->veg[SW_TREES].co2_multipliers[BIO_INDEX][SW_Model.simyear]);
	do_running_agg(p, psd, iOUT(13, pd), Globals.currIter,
		v->veg[SW_FORBS].co2_multipliers[BIO_INDEX][SW_Model.simyear]);
	do_running_agg(p, psd, iOUT(14, pd), Globals.currIter,
		v->veg[SW_GRASS].co2_multipliers[WUE_INDEX][SW_Model.simyear]);
	do_running_agg(p, psd, iOUT(15, pd), Globals.currIter,
		v->veg[SW_SHRUB].co2_multipliers[WUE_INDEX][SW_Model.simyear]);
	do_running_agg(p, psd, iOUT(16, pd), Globals.currIter,
		v->veg[SW_TREES].co2_multipliers[WUE_INDEX][SW_Model.simyear]);
	do_running_agg(p, psd, iOUT(17, pd), Globals.currIter,
		v->veg[SW_FORBS].co2_multipliers[WUE_INDEX][SW_Model.simyear]);

	if (print_IterationSummary) {
		sw_outstr_agg[0] = '\0';
		format_IterationSummary(p, psd, pd, 17);
	}
}
#endif


//------ eSW_Estab
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
#ifdef SW_OUTTEXT
void get_estab_text(OutPeriod pd)
{
	SW_VEGESTAB *v = &SW_VegEstab;
	IntU i;

	char str[OUTSTRLEN];
	sw_outstr[0] = '\0';

	i = (IntU) pd; // silence `-Wunused-parameter`

	for (i = 0; i < v->count; i++)
	{
		sprintf(str, "%c%d", _Sep, v->parms[i]->estab_doy);
		strcat(sw_outstr, str);
	}
}
#endif

#if defined(RSOILWAT)
void get_estab_mem(OutPeriod pd)
{
	SW_VEGESTAB *v = &SW_VegEstab;
	IntU i;

	RealD *p = p_OUT[eSW_Estab][pd];
	get_outvalleader(p, pd);

	for (i = 0; i < v->count; i++)
	{
		p[iOUT(i, pd)] = v->parms[i]->estab_doy;
	}
}

#elif defined(STEPWAT)
void get_estab_agg(OutPeriod pd)
{
	SW_VEGESTAB *v = &SW_VegEstab;
	IntU i;

	RealD
		*p = p_OUT[eSW_Estab][pd],
		*psd = p_OUTsd[eSW_Estab][pd];

	for (i = 0; i < v->count; i++)
	{
		do_running_agg(p, psd, iOUT(i, pd), Globals.currIter,
			v->parms[i]->estab_doy);
	}

	if (print_IterationSummary) {
		sw_outstr_agg[0] = '\0';
		format_IterationSummary(p, psd, pd, v->count);
	}
}
#endif


//------ eSW_Temp
#ifdef SW_OUTTEXT
void get_temp_text(OutPeriod pd)
{
	SW_WEATHER_OUTPUTS *vo = SW_Weather.p_oagg[pd];

	sw_outstr[0] = '\0';
	sprintf(sw_outstr, "%c%.*f%c%.*f%c%.*f%c%.*f",
		_Sep, OUT_DIGITS, vo->temp_max,
		_Sep, OUT_DIGITS, vo->temp_min,
		_Sep, OUT_DIGITS, vo->temp_avg,
		_Sep, OUT_DIGITS, vo->surfaceTemp);
}
#endif

#if defined(RSOILWAT)
void get_temp_mem(OutPeriod pd)
{
	SW_WEATHER_OUTPUTS *vo = SW_Weather.p_oagg[pd];

	RealD *p = p_OUT[eSW_Temp][pd];
	get_outvalleader(p, pd);

	p[iOUT(0, pd)] = vo->temp_max;
	p[iOUT(1, pd)] = vo->temp_min;
	p[iOUT(2, pd)] = vo->temp_avg;
	p[iOUT(3, pd)] = vo->surfaceTemp;
}

#elif defined(STEPWAT)
void get_temp_agg(OutPeriod pd)
{
	SW_WEATHER_OUTPUTS *vo = SW_Weather.p_oagg[pd];

	RealD
		*p = p_OUT[eSW_Temp][pd],
		*psd = p_OUTsd[eSW_Temp][pd];

	do_running_agg(p, psd, iOUT(0, pd), Globals.currIter, vo->temp_max);
	do_running_agg(p, psd, iOUT(1, pd), Globals.currIter, vo->temp_min);
	do_running_agg(p, psd, iOUT(2, pd), Globals.currIter, vo->temp_avg);
	do_running_agg(p, psd, iOUT(3, pd), Globals.currIter, vo->surfaceTemp);

	if (print_IterationSummary) {
		sw_outstr_agg[0] = '\0';
		format_IterationSummary(p, psd, pd, 4);
	}
}

// STEPWAT2 expects annual mean air temperature
void get_temp_SXW(OutPeriod pd)
{
	if (pd == eSW_Year) {
		SW_WEATHER_OUTPUTS *vo = SW_Weather.p_oagg[pd];

		SXW.temp = vo->temp_avg;
	}
}
#endif


//------ eSW_Precip
#ifdef SW_OUTTEXT
void get_precip_text(OutPeriod pd)
{
	SW_WEATHER_OUTPUTS *vo = SW_Weather.p_oagg[pd];

	sw_outstr[0] = '\0';
	sprintf(sw_outstr, "%c%.*f%c%.*f%c%.*f%c%.*f%c%.*f",
		_Sep, OUT_DIGITS, vo->ppt,
		_Sep, OUT_DIGITS, vo->rain,
		_Sep, OUT_DIGITS, vo->snow,
		_Sep, OUT_DIGITS, vo->snowmelt,
		_Sep, OUT_DIGITS, vo->snowloss);
}
#endif

#if defined(RSOILWAT)
void get_precip_mem(OutPeriod pd)
{
	SW_WEATHER_OUTPUTS *vo = SW_Weather.p_oagg[pd];

	RealD *p = p_OUT[eSW_Precip][pd];
	get_outvalleader(p, pd);

	p[iOUT(0, pd)] = vo->ppt;
	p[iOUT(1, pd)] = vo->rain;
	p[iOUT(2, pd)] = vo->snow;
	p[iOUT(3, pd)] = vo->snowmelt;
	p[iOUT(4, pd)] = vo->snowloss;
}

#elif defined(STEPWAT)
void get_precip_agg(OutPeriod pd)
{
	SW_WEATHER_OUTPUTS *vo = SW_Weather.p_oagg[pd];

	RealD
		*p = p_OUT[eSW_Precip][pd],
		*psd = p_OUTsd[eSW_Precip][pd];

	do_running_agg(p, psd, iOUT(0, pd), Globals.currIter, vo->ppt);
	do_running_agg(p, psd, iOUT(1, pd), Globals.currIter, vo->rain);
	do_running_agg(p, psd, iOUT(2, pd), Globals.currIter, vo->snow);
	do_running_agg(p, psd, iOUT(3, pd), Globals.currIter, vo->snowmelt);
	do_running_agg(p, psd, iOUT(4, pd), Globals.currIter, vo->snowloss);

	if (print_IterationSummary) {
		sw_outstr_agg[0] = '\0';
		format_IterationSummary(p, psd, pd, 5);
	}
}

// STEPWAT2 expects monthly and annual sum of precipitation
void get_precip_SXW(OutPeriod pd)
{
	if (pd == eSW_Month || pd == eSW_Year) {
		SW_WEATHER_OUTPUTS *vo = SW_Weather.p_oagg[pd];

		if (pd == eSW_Month) {
			SXW.ppt_monthly[SW_Model.month - tOffset] = vo->ppt;
		}
		else if (pd == eSW_Year) {
			SXW.ppt = vo->ppt;
		}
	}
}
#endif


//------ eSW_VWCBulk
#ifdef SW_OUTTEXT
void get_vwcBulk_text(OutPeriod pd)
{
	LyrIndex i;
	SW_SOILWAT_OUTPUTS *vo = SW_Soilwat.p_oagg[pd];

	char str[OUTSTRLEN];
	sw_outstr[0] = '\0';

	ForEachSoilLayer(i) {
		/* vwcBulk at this point is identical to swcBulk */
		sprintf(str, "%c%.*f",
			_Sep, OUT_DIGITS, vo->vwcBulk[i] / SW_Site.lyr[i]->width);
		strcat(sw_outstr, str);
	}
}
#endif

#if defined(RSOILWAT)
void get_vwcBulk_mem(OutPeriod pd)
{
	LyrIndex i;
	SW_SOILWAT_OUTPUTS *vo = SW_Soilwat.p_oagg[pd];

	RealD *p = p_OUT[eSW_VWCBulk][pd];
	get_outvalleader(p, pd);

	ForEachSoilLayer(i) {
		/* vwcBulk at this point is identical to swcBulk */
		p[iOUT(i, pd)] = vo->vwcBulk[i] / SW_Site.lyr[i]->width;
	}
}

#elif defined(STEPWAT)
void get_vwcBulk_agg(OutPeriod pd)
{
	LyrIndex i;
	SW_SOILWAT_OUTPUTS *vo = SW_Soilwat.p_oagg[pd];

	RealD
		*p = p_OUT[eSW_VWCBulk][pd],
		*psd = p_OUTsd[eSW_VWCBulk][pd];

	ForEachSoilLayer(i) {
		/* vwcBulk at this point is identical to swcBulk */
		do_running_agg(p, psd, iOUT(i, pd), Globals.currIter,
			vo->vwcBulk[i] / SW_Site.lyr[i]->width);
	}

	if (print_IterationSummary) {
		sw_outstr_agg[0] = '\0';
		format_IterationSummary(p, psd, pd, SW_Site.n_layers);
	}
}
#endif


//------ eSW_VWCMatric
#ifdef SW_OUTTEXT
void get_vwcMatric_text(OutPeriod pd)
{
	LyrIndex i;
	RealD convert;
	SW_SOILWAT_OUTPUTS *vo = SW_Soilwat.p_oagg[pd];

	char str[OUTSTRLEN];
	sw_outstr[0] = '\0';

	ForEachSoilLayer(i) {
		/* vwcMatric at this point is identical to swcBulk */
		convert = 1. / (1. - SW_Site.lyr[i]->fractionVolBulk_gravel) / SW_Site.lyr[i]->width;

		sprintf(str, "%c%.*f",
			_Sep, OUT_DIGITS, vo->vwcMatric[i] * convert);
		strcat(sw_outstr, str);
	}
}
#endif

#if defined(RSOILWAT)
void get_vwcMatric_mem(OutPeriod pd)
{
	LyrIndex i;
	RealD convert;
	SW_SOILWAT_OUTPUTS *vo = SW_Soilwat.p_oagg[pd];

	RealD *p = p_OUT[eSW_VWCMatric][pd];
	get_outvalleader(p, pd);

	ForEachSoilLayer(i) {
		/* vwcMatric at this point is identical to swcBulk */
		convert = 1. / (1. - SW_Site.lyr[i]->fractionVolBulk_gravel) / SW_Site.lyr[i]->width;
		p[iOUT(i, pd)] = vo->vwcMatric[i] * convert;
	}
}

#elif defined(STEPWAT)
void get_vwcMatric_agg(OutPeriod pd)
{
	LyrIndex i;
	RealD convert;
	SW_SOILWAT_OUTPUTS *vo = SW_Soilwat.p_oagg[pd];

	RealD
		*p = p_OUT[eSW_VWCMatric][pd],
		*psd = p_OUTsd[eSW_VWCMatric][pd];

	ForEachSoilLayer(i) {
		/* vwcMatric at this point is identical to swcBulk */
		convert = 1. / (1. - SW_Site.lyr[i]->fractionVolBulk_gravel) / SW_Site.lyr[i]->width;

		do_running_agg(p, psd, iOUT(i, pd), Globals.currIter,
			vo->vwcMatric[i] * convert);
	}

	if (print_IterationSummary) {
		sw_outstr_agg[0] = '\0';
		format_IterationSummary(p, psd, pd, SW_Site.n_layers);
	}
}
#endif


//------ eSW_SWA
#ifdef SW_OUTTEXT
void get_swa_text(OutPeriod pd)
{
	/* added 21-Oct-03, cwb */
	LyrIndex i;
	int k;
	SW_SOILWAT_OUTPUTS *vo = SW_Soilwat.p_oagg[pd];

	char str[OUTSTRLEN];
	sw_outstr[0] = '\0';

	ForEachVegType(k)
	{
		ForEachSoilLayer(i)
		{
			sprintf(str, "%c%.*f", _Sep, OUT_DIGITS, vo->SWA_VegType[k][i]);
			strcat(sw_outstr, str);
		}
	}
}
#endif

#if defined(RSOILWAT)
void get_swa_mem(OutPeriod pd)
{
	LyrIndex i;
	int k;
	SW_SOILWAT_OUTPUTS *vo = SW_Soilwat.p_oagg[pd];

	RealD *p = p_OUT[eSW_SWA][pd];
	get_outvalleader(p, pd);

	ForEachVegType(k)
	{
		ForEachSoilLayer(i)
		{
			p[iOUT2(i, k, pd)] = vo->SWA_VegType[k][i];
		}
	}
}

#elif defined(STEPWAT)
void get_swa_agg(OutPeriod pd)
{
	LyrIndex i;
	int k;
	SW_SOILWAT_OUTPUTS *vo = SW_Soilwat.p_oagg[pd];

	RealD
		*p = p_OUT[eSW_SWA][pd],
		*psd = p_OUTsd[eSW_SWA][pd];

	ForEachVegType(k)
	{
		ForEachSoilLayer(i)
		{
			do_running_agg(p, psd, iOUT2(i, k, pd), Globals.currIter,
				vo->SWA_VegType[k][i]);
		}
	}

	if (print_IterationSummary) {
		sw_outstr_agg[0] = '\0';
		format_IterationSummary2(p, psd, pd, NVEGTYPES, SW_Site.n_layers, 0);
	}
}
#endif


//------ eSW_SWCBulk
#ifdef SW_OUTTEXT
void get_swcBulk_text(OutPeriod pd)
{
	/* added 21-Oct-03, cwb */
	LyrIndex i;
	SW_SOILWAT_OUTPUTS *vo = SW_Soilwat.p_oagg[pd];

	char str[OUTSTRLEN];
	sw_outstr[0] = '\0';

	ForEachSoilLayer(i)
	{
		sprintf(str, "%c%.*f", _Sep, OUT_DIGITS, vo->swcBulk[i]);
		strcat(sw_outstr, str);
	}
}
#endif

#if defined(RSOILWAT)
void get_swcBulk_mem(OutPeriod pd)
{
	LyrIndex i;
	SW_SOILWAT_OUTPUTS *vo = SW_Soilwat.p_oagg[pd];

	RealD *p = p_OUT[eSW_SWCBulk][pd];
	get_outvalleader(p, pd);

	ForEachSoilLayer(i)
	{
		p[iOUT(i, pd)] = vo->swcBulk[i];
	}
}

#elif defined(STEPWAT)
void get_swcBulk_agg(OutPeriod pd)
{
	LyrIndex i;
	SW_SOILWAT_OUTPUTS *vo = SW_Soilwat.p_oagg[pd];

	RealD
		*p = p_OUT[eSW_SWCBulk][pd],
		*psd = p_OUTsd[eSW_SWCBulk][pd];

	ForEachSoilLayer(i)
	{
		do_running_agg(p, psd, iOUT(i, pd), Globals.currIter, vo->swcBulk[i]);
	}

	if (print_IterationSummary) {
		sw_outstr_agg[0] = '\0';
		format_IterationSummary(p, psd, pd, SW_Site.n_layers);
	}
}

// STEPWAT2 expects monthly mean SWCbulk by soil layer
void get_swcBulk_SXW(OutPeriod pd)
{
	if (pd == eSW_Month) {
		LyrIndex i;
		SW_SOILWAT_OUTPUTS *vo = SW_Soilwat.p_oagg[pd];

		ForEachSoilLayer(i)
		{
			SXW.swc[Ilp(i, SW_Model.month - tOffset)] = vo->swcBulk[i];
		}
	}
}
#endif


//------ eSW_SWPMatric
/* can't take arithmetic average of swp because it's
 * exponential.  At this time (until I remember to look
 * up whether harmonic or some other average is better
 * and fix this) we're not averaging swp but converting
 * the averaged swc.  This also avoids converting for
 * each day.
 *
 * added 12-Oct-03, cwb */

#ifdef SW_OUTTEXT
void get_swpMatric_text(OutPeriod pd)
{
	RealD val;
	LyrIndex i;
	SW_SOILWAT_OUTPUTS *vo = SW_Soilwat.p_oagg[pd];

	char str[OUTSTRLEN];
	sw_outstr[0] = '\0';

	ForEachSoilLayer(i)
	{
		/* swpMatric at this point is identical to swcBulk */
		val = SW_SWCbulk2SWPmatric(SW_Site.lyr[i]->fractionVolBulk_gravel,
			vo->swpMatric[i], i);

		sprintf(str, "%c%.*f", _Sep, OUT_DIGITS, val);
		strcat(sw_outstr, str);
	}
}
#endif

#if defined(RSOILWAT)
void get_swpMatric_mem(OutPeriod pd)
{
	LyrIndex i;
	SW_SOILWAT_OUTPUTS *vo = SW_Soilwat.p_oagg[pd];

	RealD *p = p_OUT[eSW_SWPMatric][pd];
	get_outvalleader(p, pd);

	ForEachSoilLayer(i)
	{
		/* swpMatric at this point is identical to swcBulk */
		p[iOUT(i, pd)] = SW_SWCbulk2SWPmatric(
			SW_Site.lyr[i]->fractionVolBulk_gravel, vo->swpMatric[i], i);
	}
}

#elif defined(STEPWAT)
void get_swpMatric_agg(OutPeriod pd)
{
	RealD val;
	LyrIndex i;
	SW_SOILWAT_OUTPUTS *vo = SW_Soilwat.p_oagg[pd];

	RealD
		*p = p_OUT[eSW_SWPMatric][pd],
		*psd = p_OUTsd[eSW_SWPMatric][pd];

	ForEachSoilLayer(i)
	{
		/* swpMatric at this point is identical to swcBulk */
		val = SW_SWCbulk2SWPmatric(
			SW_Site.lyr[i]->fractionVolBulk_gravel, vo->swpMatric[i], i);
		do_running_agg(p, psd, iOUT(i, pd), Globals.currIter, val);
	}

	if (print_IterationSummary) {
		sw_outstr_agg[0] = '\0';
		format_IterationSummary(p, psd, pd, SW_Site.n_layers);
	}
}
#endif


//------ eSW_SWABulk
#ifdef SW_OUTTEXT
void get_swaBulk_text(OutPeriod pd)
{
	LyrIndex i;
	SW_SOILWAT_OUTPUTS *vo = SW_Soilwat.p_oagg[pd];

	char str[OUTSTRLEN];
	sw_outstr[0] = '\0';

	ForEachSoilLayer(i)
	{
		sprintf(str, "%c%.*f", _Sep, OUT_DIGITS, vo->swaBulk[i]);
		strcat(sw_outstr, str);
	}
}
#endif

#if defined(RSOILWAT)
void get_swaBulk_mem(OutPeriod pd)
{
	LyrIndex i;
	SW_SOILWAT_OUTPUTS *vo = SW_Soilwat.p_oagg[pd];

	RealD *p = p_OUT[eSW_SWABulk][pd];
	get_outvalleader(p, pd);

	ForEachSoilLayer(i)
	{
		p[iOUT(i, pd)] = vo->swaBulk[i];
	}
}

#elif defined(STEPWAT)
void get_swaBulk_agg(OutPeriod pd)
{
	LyrIndex i;
	SW_SOILWAT_OUTPUTS *vo = SW_Soilwat.p_oagg[pd];

	RealD
		*p = p_OUT[eSW_SWABulk][pd],
		*psd = p_OUTsd[eSW_SWABulk][pd];

	ForEachSoilLayer(i)
	{
		do_running_agg(p, psd, iOUT(i, pd), Globals.currIter, vo->swaBulk[i]);
	}

	if (print_IterationSummary) {
		sw_outstr_agg[0] = '\0';
		format_IterationSummary(p, psd, pd, SW_Site.n_layers);
	}
}
#endif


//------ eSW_SWAMatric
#ifdef SW_OUTTEXT
void get_swaMatric_text(OutPeriod pd)
{
	LyrIndex i;
	RealD convert;
	SW_SOILWAT_OUTPUTS *vo = SW_Soilwat.p_oagg[pd];

	char str[OUTSTRLEN];
	sw_outstr[0] = '\0';

	ForEachSoilLayer(i)
	{
		/* swaMatric at this point is identical to swaBulk */
		convert = 1. / (1. - SW_Site.lyr[i]->fractionVolBulk_gravel);

		sprintf(str, "%c%.*f", _Sep, OUT_DIGITS, vo->swaMatric[i] * convert);
		strcat(sw_outstr, str);
	}
}
#endif

#if defined(RSOILWAT)
void get_swaMatric_mem(OutPeriod pd)
{
	LyrIndex i;
	RealD convert;
	SW_SOILWAT_OUTPUTS *vo = SW_Soilwat.p_oagg[pd];

	RealD *p = p_OUT[eSW_SWAMatric][pd];
	get_outvalleader(p, pd);

	ForEachSoilLayer(i)
	{
		/* swaMatric at this point is identical to swaBulk */
		convert = 1. / (1. - SW_Site.lyr[i]->fractionVolBulk_gravel);
		p[iOUT(i, pd)] = vo->swaMatric[i] * convert;
	}
}

#elif defined(STEPWAT)
void get_swaMatric_agg(OutPeriod pd)
{
	LyrIndex i;
	RealD convert;
	SW_SOILWAT_OUTPUTS *vo = SW_Soilwat.p_oagg[pd];

	RealD
		*p = p_OUT[eSW_SWAMatric][pd],
		*psd = p_OUTsd[eSW_SWAMatric][pd];

	ForEachSoilLayer(i)
	{
		/* swaMatric at this point is identical to swaBulk */
		convert = 1. / (1. - SW_Site.lyr[i]->fractionVolBulk_gravel);
		do_running_agg(p, psd, iOUT(i, pd), Globals.currIter,
			vo->swaMatric[i] * convert);
	}

	if (print_IterationSummary) {
		sw_outstr_agg[0] = '\0';
		format_IterationSummary(p, psd, pd, SW_Site.n_layers);
	}
}
#endif


//------ eSW_SurfaceWater
#ifdef SW_OUTTEXT
void get_surfaceWater_text(OutPeriod pd)
{
	SW_SOILWAT_OUTPUTS *vo = SW_Soilwat.p_oagg[pd];

	sw_outstr[0] = '\0';
	sprintf(sw_outstr, "%c%.*f", _Sep, OUT_DIGITS, vo->surfaceWater);
}
#endif

#if defined(RSOILWAT)
void get_surfaceWater_mem(OutPeriod pd)
{
	SW_SOILWAT_OUTPUTS *vo = SW_Soilwat.p_oagg[pd];

	RealD *p = p_OUT[eSW_SurfaceWater][pd];
	get_outvalleader(p, pd);

	p[iOUT(0, pd)] = vo->surfaceWater;
}

#elif defined(STEPWAT)
void get_surfaceWater_agg(OutPeriod pd)
{
	SW_SOILWAT_OUTPUTS *vo = SW_Soilwat.p_oagg[pd];

	RealD
		*p = p_OUT[eSW_SurfaceWater][pd],
		*psd = p_OUTsd[eSW_SurfaceWater][pd];

	do_running_agg(p, psd, iOUT(0, pd), Globals.currIter, vo->surfaceWater);

	if (print_IterationSummary) {
		sw_outstr_agg[0] = '\0';
		format_IterationSummary(p, psd, pd, 1);
	}
}
#endif


//------ eSW_Runoff
#ifdef SW_OUTTEXT
void get_runoffrunon_text(OutPeriod pd)
{
	RealD net;
	SW_WEATHER_OUTPUTS *vo = SW_Weather.p_oagg[pd];

	net = vo->surfaceRunoff + vo->snowRunoff - vo->surfaceRunon;

	sw_outstr[0] = '\0';
	sprintf(sw_outstr, "%c%.*f%c%.*f%c%.*f%c%.*f",
		_Sep, OUT_DIGITS, net,
		_Sep, OUT_DIGITS, vo->surfaceRunoff,
		_Sep, OUT_DIGITS, vo->snowRunoff,
		_Sep, OUT_DIGITS, vo->surfaceRunon);
}
#endif

#if defined(RSOILWAT)
void get_runoffrunon_mem(OutPeriod pd)
{
	RealD net;
	SW_WEATHER_OUTPUTS *vo = SW_Weather.p_oagg[pd];

	RealD *p = p_OUT[eSW_Runoff][pd];
	get_outvalleader(p, pd);

	net = vo->surfaceRunoff + vo->snowRunoff - vo->surfaceRunon;

	p[iOUT(0, pd)] = net;
	p[iOUT(1, pd)] = vo->surfaceRunoff;
	p[iOUT(2, pd)] = vo->snowRunoff;
	p[iOUT(3, pd)] = vo->surfaceRunon;
}

#elif defined(STEPWAT)
void get_runoffrunon_agg(OutPeriod pd)
{
	RealD net;
	SW_WEATHER_OUTPUTS *vo = SW_Weather.p_oagg[pd];

	RealD
		*p = p_OUT[eSW_Runoff][pd],
		*psd = p_OUTsd[eSW_Runoff][pd];

	net = vo->surfaceRunoff + vo->snowRunoff - vo->surfaceRunon;

	do_running_agg(p, psd, iOUT(0, pd), Globals.currIter, net);
	do_running_agg(p, psd, iOUT(1, pd), Globals.currIter, vo->surfaceRunoff);
	do_running_agg(p, psd, iOUT(2, pd), Globals.currIter, vo->snowRunoff);
	do_running_agg(p, psd, iOUT(3, pd), Globals.currIter, vo->surfaceRunon);

	if (print_IterationSummary) {
		sw_outstr_agg[0] = '\0';
		format_IterationSummary(p, psd, pd, 4);
	}
}
#endif


//------ eSW_Transp
#ifdef SW_OUTTEXT
void get_transp_text(OutPeriod pd)
{
	LyrIndex i;
	int k;
	SW_SOILWAT_OUTPUTS *vo = SW_Soilwat.p_oagg[pd];

	char str[OUTSTRLEN];
	sw_outstr[0] = '\0';

	/* total transpiration */
	ForEachSoilLayer(i)
	{
		sprintf(str, "%c%.*f", _Sep, OUT_DIGITS, vo->transp_total[i]);
		strcat(sw_outstr, str);
	}

	/* transpiration for each vegetation type */
	ForEachVegType(k)
	{
		ForEachSoilLayer(i)
		{
			sprintf(str, "%c%.*f", _Sep, OUT_DIGITS, vo->transp[k][i]);
			strcat(sw_outstr, str);
		}
	}
}
#endif

#if defined(RSOILWAT)
void get_transp_mem(OutPeriod pd)
{
	LyrIndex i;
	int k;
	SW_SOILWAT_OUTPUTS *vo = SW_Soilwat.p_oagg[pd];

	RealD *p = p_OUT[eSW_Transp][pd];
	get_outvalleader(p, pd);

	/* total transpiration */
	ForEachSoilLayer(i)
	{
		p[iOUT(i, pd)] = vo->transp_total[i];
	}

	/* transpiration for each vegetation type */
	ForEachVegType(k)
	{
		ForEachSoilLayer(i)
		{
			p[iOUT2(i, k + 1, pd)] = vo->transp[k][i]; // k + 1 because of total transp.
		}
	}
}

#elif defined(STEPWAT)
void get_transp_agg(OutPeriod pd)
{
	LyrIndex i;
	int k;
	SW_SOILWAT_OUTPUTS *vo = SW_Soilwat.p_oagg[pd];

	RealD
		*p = p_OUT[eSW_Transp][pd],
		*psd = p_OUTsd[eSW_Transp][pd];

	/* total transpiration */
	ForEachSoilLayer(i)
	{
		do_running_agg(p, psd, iOUT(i, pd), Globals.currIter, vo->transp_total[i]);
	}

	if (print_IterationSummary) {
		sw_outstr_agg[0] = '\0';
		format_IterationSummary(p, psd, pd, SW_Site.n_layers);
	}

	/* transpiration for each vegetation type */
	ForEachVegType(k)
	{
		ForEachSoilLayer(i)
		{
			// k + 1 because of total transp.
			do_running_agg(p, psd, iOUT2(i, k + 1, pd), Globals.currIter,
				vo->transp[k][i]);
		}
	}

	if (print_IterationSummary) {
		format_IterationSummary2(p, psd, pd, NVEGTYPES, SW_Site.n_layers, 1);
	}
}

// STEPWAT2 expects monthly sum of transpiration by soil layer
// see function `_transp_contribution_by_group`
void get_transp_SXW(OutPeriod pd)
{
	if (pd == eSW_Month) {
		LyrIndex i;
		int k;
		SW_SOILWAT_OUTPUTS *vo = SW_Soilwat.p_oagg[pd];

		/* total transpiration */
		ForEachSoilLayer(i)
		{
			SXW.transpTotal[Ilp(i, SW_Model.month - tOffset)] = vo->transp_total[i];
		}

		/* transpiration for each vegetation type */
		ForEachVegType(k)
		{
			ForEachSoilLayer(i)
			{
				SXW.transpVeg[k][Ilp(i, SW_Model.month - tOffset)] = vo->transp[k][i];
			}
		}
	}
}
#endif


//------ eSW_EvapSoil
#ifdef SW_OUTTEXT
void get_evapSoil_text(OutPeriod pd)
{
	LyrIndex i;
	SW_SOILWAT_OUTPUTS *vo = SW_Soilwat.p_oagg[pd];

	char str[OUTSTRLEN];
	sw_outstr[0] = '\0';

	ForEachEvapLayer(i)
	{
		sprintf(str, "%c%.*f", _Sep, OUT_DIGITS, vo->evap[i]);
		strcat(sw_outstr, str);
	}
}
#endif

#if defined(RSOILWAT)
void get_evapSoil_mem(OutPeriod pd)
{
	LyrIndex i;
	SW_SOILWAT_OUTPUTS *vo = SW_Soilwat.p_oagg[pd];

	RealD *p = p_OUT[eSW_EvapSoil][pd];
	get_outvalleader(p, pd);

	ForEachEvapLayer(i)
	{
		p[iOUT(i, pd)] = vo->evap[i];
	}
}

#elif defined(STEPWAT)
void get_evapSoil_agg(OutPeriod pd)
{
	LyrIndex i;
	SW_SOILWAT_OUTPUTS *vo = SW_Soilwat.p_oagg[pd];

	RealD
		*p = p_OUT[eSW_EvapSoil][pd],
		*psd = p_OUTsd[eSW_EvapSoil][pd];

	ForEachEvapLayer(i)
	{
		do_running_agg(p, psd, iOUT(i, pd), Globals.currIter, vo->evap[i]);
	}

	if (print_IterationSummary) {
		sw_outstr_agg[0] = '\0';
		format_IterationSummary(p, psd, pd, SW_Site.n_evap_lyrs);
	}
}
#endif


//------ eSW_EvapSurface
#ifdef SW_OUTTEXT
void get_evapSurface_text(OutPeriod pd)
{
	int k;
	SW_SOILWAT_OUTPUTS *vo = SW_Soilwat.p_oagg[pd];

	char str[OUTSTRLEN];
	sw_outstr[0] = '\0';

	sprintf(sw_outstr, "%c%.*f", _Sep, OUT_DIGITS, vo->total_evap);

	ForEachVegType(k) {
		sprintf(str, "%c%.*f", _Sep, OUT_DIGITS, vo->evap_veg[k]);
		strcat(sw_outstr, str);
	}

	sprintf(str, "%c%.*f%c%.*f",
		_Sep, OUT_DIGITS, vo->litter_evap,
		_Sep, OUT_DIGITS, vo->surfaceWater_evap);
	strcat(sw_outstr, str);
}
#endif

#if defined(RSOILWAT)
void get_evapSurface_mem(OutPeriod pd)
{
	int k;
	SW_SOILWAT_OUTPUTS *vo = SW_Soilwat.p_oagg[pd];

	RealD *p = p_OUT[eSW_EvapSurface][pd];
	get_outvalleader(p, pd);

	p[iOUT(0, pd)] = vo->total_evap;

	ForEachVegType(k) {
		p[iOUT(k + 1, pd)] = vo->evap_veg[k];
	}

	p[iOUT(NVEGTYPES + 1, pd)] = vo->litter_evap;
	p[iOUT(NVEGTYPES + 2, pd)] = vo->surfaceWater_evap;
}

#elif defined(STEPWAT)
void get_evapSurface_agg(OutPeriod pd)
{
	int k;
	SW_SOILWAT_OUTPUTS *vo = SW_Soilwat.p_oagg[pd];

	RealD
		*p = p_OUT[eSW_EvapSurface][pd],
		*psd = p_OUTsd[eSW_EvapSurface][pd];

	do_running_agg(p, psd, iOUT(0, pd), Globals.currIter, vo->total_evap);

	ForEachVegType(k) {
		do_running_agg(p, psd, iOUT(k + 1, pd), Globals.currIter,
			vo->evap_veg[k]);
	}

	do_running_agg(p, psd, iOUT(NVEGTYPES + 1, pd), Globals.currIter,
		vo->litter_evap);
	do_running_agg(p, psd, iOUT(NVEGTYPES + 2, pd), Globals.currIter,
		vo->surfaceWater_evap);

	if (print_IterationSummary) {
		sw_outstr_agg[0] = '\0';
		format_IterationSummary(p, psd, pd, NVEGTYPES + 3);
	}
}
#endif


//------ eSW_Interception
#ifdef SW_OUTTEXT
void get_interception_text(OutPeriod pd)
{
	int k;
	SW_SOILWAT_OUTPUTS *vo = SW_Soilwat.p_oagg[pd];

	char str[OUTSTRLEN];
	sw_outstr[0] = '\0';

	sprintf(sw_outstr, "%c%.*f", _Sep, OUT_DIGITS, vo->total_int);

	ForEachVegType(k) {
		sprintf(str, "%c%.*f", _Sep, OUT_DIGITS, vo->int_veg[k]);
		strcat(sw_outstr, str);
	}

	sprintf(str, "%c%.*f", _Sep, OUT_DIGITS, vo->litter_int);
	strcat(sw_outstr, str);
}
#endif

#if defined(RSOILWAT)
void get_interception_mem(OutPeriod pd)
{
	int k;
	SW_SOILWAT_OUTPUTS *vo = SW_Soilwat.p_oagg[pd];

	RealD *p = p_OUT[eSW_Interception][pd];
	get_outvalleader(p, pd);

	p[iOUT(0, pd)] = vo->total_int;

	ForEachVegType(k) {
		p[iOUT(k + 1, pd)] = vo->int_veg[k];
	}

	p[iOUT(NVEGTYPES + 1, pd)] = vo->litter_int;
}

#elif defined(STEPWAT)
void get_interception_agg(OutPeriod pd)
{
	int k;
	SW_SOILWAT_OUTPUTS *vo = SW_Soilwat.p_oagg[pd];

	RealD
		*p = p_OUT[eSW_Interception][pd],
		*psd = p_OUTsd[eSW_Interception][pd];

	do_running_agg(p, psd, iOUT(0, pd), Globals.currIter, vo->total_int);

	ForEachVegType(k) {
		do_running_agg(p, psd, iOUT(k + 1, pd), Globals.currIter, vo->int_veg[k]);
	}

	do_running_agg(p, psd, iOUT(NVEGTYPES + 1, pd), Globals.currIter,
		vo->litter_int);

	if (print_IterationSummary) {
		sw_outstr_agg[0] = '\0';
		format_IterationSummary(p, psd, pd, NVEGTYPES + 2);
	}
}
#endif


//------ eSW_SoilInf
#ifdef SW_OUTTEXT
void get_soilinf_text(OutPeriod pd)
{
	/* 20100202 (drs) added */
	/* 20110219 (drs) added runoff */
	/* 12/13/2012	(clk)	moved runoff, now named snowRunoff, to get_runoffrunon(); */
	SW_WEATHER_OUTPUTS *vo = SW_Weather.p_oagg[pd];

	sw_outstr[0] = '\0';
	sprintf(sw_outstr, "%c%.*f", _Sep, OUT_DIGITS, vo->soil_inf);
}
#endif

#if defined(RSOILWAT)
void get_soilinf_mem(OutPeriod pd)
{
	SW_WEATHER_OUTPUTS *vo = SW_Weather.p_oagg[pd];

	RealD *p = p_OUT[eSW_SoilInf][pd];
	get_outvalleader(p, pd);

	p[iOUT(0, pd)] = vo->soil_inf;
}

#elif defined(STEPWAT)
void get_soilinf_agg(OutPeriod pd)
{
	SW_WEATHER_OUTPUTS *vo = SW_Weather.p_oagg[pd];

	RealD
		*p = p_OUT[eSW_SoilInf][pd],
		*psd = p_OUTsd[eSW_SoilInf][pd];

	do_running_agg(p, psd, iOUT(0, pd), Globals.currIter, vo->soil_inf);

	if (print_IterationSummary) {
		sw_outstr_agg[0] = '\0';
		format_IterationSummary(p, psd, pd, 1);
	}
}
#endif


//------ eSW_LyrDrain
#ifdef SW_OUTTEXT
void get_lyrdrain_text(OutPeriod pd)
{
	/* 20100202 (drs) added */
	LyrIndex i;
	SW_SOILWAT_OUTPUTS *vo = SW_Soilwat.p_oagg[pd];

	char str[OUTSTRLEN];
	sw_outstr[0] = '\0';

	for (i = 0; i < SW_Site.n_layers - 1; i++)
	{
		sprintf(str, "%c%.*f", _Sep, OUT_DIGITS, vo->lyrdrain[i]);
		strcat(sw_outstr, str);
	}
}
#endif

#if defined(RSOILWAT)
void get_lyrdrain_mem(OutPeriod pd)
{
	LyrIndex i;
	SW_SOILWAT_OUTPUTS *vo = SW_Soilwat.p_oagg[pd];

	RealD *p = p_OUT[eSW_LyrDrain][pd];
	get_outvalleader(p, pd);

	for (i = 0; i < SW_Site.n_layers - 1; i++)
	{
		p[iOUT(i, pd)] = vo->lyrdrain[i];
	}
}

#elif defined(STEPWAT)
void get_lyrdrain_agg(OutPeriod pd)
{
	LyrIndex i;
	SW_SOILWAT_OUTPUTS *vo = SW_Soilwat.p_oagg[pd];

	RealD
		*p = p_OUT[eSW_LyrDrain][pd],
		*psd = p_OUTsd[eSW_LyrDrain][pd];

	for (i = 0; i < SW_Site.n_layers - 1; i++)
	{
		do_running_agg(p, psd, iOUT(i, pd), Globals.currIter, vo->lyrdrain[i]);
	}

	if (print_IterationSummary) {
		sw_outstr_agg[0] = '\0';
		format_IterationSummary(p, psd, pd, SW_Site.n_layers - 1);
	}
}
#endif


//------ eSW_HydRed
#ifdef SW_OUTTEXT
void get_hydred_text(OutPeriod pd)
{
	/* 20101020 (drs) added */
	LyrIndex i;
	int k;
	SW_SOILWAT_OUTPUTS *vo = SW_Soilwat.p_oagg[pd];

	char str[OUTSTRLEN];
	sw_outstr[0] = '\0';

	/* total hydraulic redistribution */
	ForEachSoilLayer(i)
	{
		sprintf(str, "%c%.*f", _Sep, OUT_DIGITS, vo->hydred_total[i]);
		strcat(sw_outstr, str);
	}

	/* hydraulic redistribution for each vegetation type */
	ForEachVegType(k)
	{
		ForEachSoilLayer(i)
		{
			sprintf(str, "%c%.*f", _Sep, OUT_DIGITS, vo->hydred[k][i]);
			strcat(sw_outstr, str);
		}
	}
}
#endif

#if defined(RSOILWAT)
void get_hydred_mem(OutPeriod pd)
{
	LyrIndex i;
	int k;
	SW_SOILWAT_OUTPUTS *vo = SW_Soilwat.p_oagg[pd];

	RealD *p = p_OUT[eSW_HydRed][pd];
	get_outvalleader(p, pd);

	/* total hydraulic redistribution */
	ForEachSoilLayer(i)
	{
		p[iOUT(i, pd)] = vo->hydred_total[i];
	}

	/* hydraulic redistribution for each vegetation type */
	ForEachVegType(k)
	{
		ForEachSoilLayer(i)
		{
			p[iOUT2(i, k + 1, pd)] = vo->hydred[k][i]; // k + 1 because of total hydred
		}
	}
}

#elif defined(STEPWAT)
void get_hydred_agg(OutPeriod pd)
{
	LyrIndex i;
	int k;
	SW_SOILWAT_OUTPUTS *vo = SW_Soilwat.p_oagg[pd];

	RealD
		*p = p_OUT[eSW_HydRed][pd],
		*psd = p_OUTsd[eSW_HydRed][pd];

	/* total hydraulic redistribution */
	ForEachSoilLayer(i)
	{
		do_running_agg(p, psd, iOUT(i, pd), Globals.currIter, vo->hydred_total[i]);
	}

	if (print_IterationSummary) {
		sw_outstr_agg[0] = '\0';
		format_IterationSummary(p, psd, pd, SW_Site.n_layers);
	}

	/* hydraulic redistribution for each vegetation type */
	ForEachVegType(k)
	{
		ForEachSoilLayer(i)
		{
			// k + 1 because of total hydred
			do_running_agg(p, psd, iOUT2(i, k + 1, pd), Globals.currIter,
				vo->hydred[k][i]);
		}
	}

	if (print_IterationSummary) {
		format_IterationSummary2(p, psd, pd, NVEGTYPES, SW_Site.n_layers, 1);
	}
}
#endif


//------ eSW_AET
#ifdef SW_OUTTEXT
void get_aet_text(OutPeriod pd)
{
	SW_SOILWAT_OUTPUTS *vo = SW_Soilwat.p_oagg[pd];

	sw_outstr[0] = '\0';
	sprintf(sw_outstr, "%c%.*f", _Sep, OUT_DIGITS, vo->aet);
}
#endif

#if defined(RSOILWAT)
void get_aet_mem(OutPeriod pd)
{
	SW_SOILWAT_OUTPUTS *vo = SW_Soilwat.p_oagg[pd];

	RealD *p = p_OUT[eSW_AET][pd];
	get_outvalleader(p, pd);

	p[iOUT(0, pd)] = vo->aet;
}

#elif defined(STEPWAT)
void get_aet_agg(OutPeriod pd)
{
	SW_SOILWAT_OUTPUTS *vo = SW_Soilwat.p_oagg[pd];

	RealD
		*p = p_OUT[eSW_AET][pd],
		*psd = p_OUTsd[eSW_AET][pd];

	do_running_agg(p, psd, iOUT(0, pd), Globals.currIter, vo->aet);

	if (print_IterationSummary) {
		sw_outstr_agg[0] = '\0';
		format_IterationSummary(p, psd, pd, 1);
	}
}

// STEPWAT2 expects annual sum of actual evapotranspiration
void get_aet_SXW(OutPeriod pd)
{
	if (pd == eSW_Year) {
		SW_SOILWAT_OUTPUTS *vo = SW_Soilwat.p_oagg[pd];

		SXW.aet = vo->aet;
	}
}
#endif


//------ eSW_PET
#ifdef SW_OUTTEXT
void get_pet_text(OutPeriod pd)
{
	SW_SOILWAT_OUTPUTS *vo = SW_Soilwat.p_oagg[pd];

	sw_outstr[0] = '\0';
	sprintf(sw_outstr, "%c%.*f", _Sep, OUT_DIGITS, vo->pet);
}
#endif

#if defined(RSOILWAT)
void get_pet_mem(OutPeriod pd)
{
	SW_SOILWAT_OUTPUTS *vo = SW_Soilwat.p_oagg[pd];

	RealD *p = p_OUT[eSW_PET][pd];
	get_outvalleader(p, pd);

	p[iOUT(0, pd)] = vo->pet;
}

#elif defined(STEPWAT)
void get_pet_agg(OutPeriod pd)
{
	SW_SOILWAT_OUTPUTS *vo = SW_Soilwat.p_oagg[pd];

	RealD
		*p = p_OUT[eSW_PET][pd],
		*psd = p_OUTsd[eSW_PET][pd];

	do_running_agg(p, psd, iOUT(0, pd), Globals.currIter, vo->pet);

	if (print_IterationSummary) {
		sw_outstr_agg[0] = '\0';
		format_IterationSummary(p, psd, pd, 1);
	}
}
#endif


//------ eSW_WetDays
#ifdef SW_OUTTEXT
void get_wetdays_text(OutPeriod pd)
{
	LyrIndex i;

	char str[OUTSTRLEN];
	sw_outstr[0] = '\0';

	if (pd == eSW_Day)
	{
		ForEachSoilLayer(i) {
			sprintf(str, "%c%i", _Sep, (SW_Soilwat.is_wet[i]) ? 1 : 0);
			strcat(sw_outstr, str);
		}

	} else
	{
		SW_SOILWAT_OUTPUTS *vo = SW_Soilwat.p_oagg[pd];

		ForEachSoilLayer(i) {
			sprintf(str, "%c%i", _Sep, (int) vo->wetdays[i]);
			strcat(sw_outstr, str);
		}
	}
}
#endif

#if defined(RSOILWAT)
void get_wetdays_mem(OutPeriod pd)
{
	LyrIndex i;

	RealD *p = p_OUT[eSW_WetDays][pd];
	get_outvalleader(p, pd);

	if (pd == eSW_Day)
	{
		ForEachSoilLayer(i) {
			p[iOUT(i, pd)] = (SW_Soilwat.is_wet[i]) ? 1 : 0;
		}

	} else
	{
		SW_SOILWAT_OUTPUTS *vo = SW_Soilwat.p_oagg[pd];

		ForEachSoilLayer(i) {
			p[iOUT(i, pd)] = (int) vo->wetdays[i];
		}
	}
}

#elif defined(STEPWAT)
void get_wetdays_agg(OutPeriod pd)
{
	LyrIndex i;

	RealD
		*p = p_OUT[eSW_WetDays][pd],
		*psd = p_OUTsd[eSW_WetDays][pd];

	if (pd == eSW_Day)
	{
		ForEachSoilLayer(i) {
			do_running_agg(p, psd, iOUT(i, pd), Globals.currIter,
				(SW_Soilwat.is_wet[i]) ? 1 : 0);
		}

	} else
	{
		SW_SOILWAT_OUTPUTS *vo = SW_Soilwat.p_oagg[pd];

		ForEachSoilLayer(i) {
			do_running_agg(p, psd, iOUT(i, pd), Globals.currIter, vo->wetdays[i]);
		}
	}

	if (print_IterationSummary) {
		sw_outstr_agg[0] = '\0';
		format_IterationSummary(p, psd, pd, SW_Site.n_layers);
	}
}
#endif


//------ eSW_SnowPack
#ifdef SW_OUTTEXT
void get_snowpack_text(OutPeriod pd)
{
	SW_SOILWAT_OUTPUTS *vo = SW_Soilwat.p_oagg[pd];

	sw_outstr[0] = '\0';
	sprintf(sw_outstr, "%c%.*f%c%.*f",
		_Sep, OUT_DIGITS, vo->snowpack,
		_Sep, OUT_DIGITS, vo->snowdepth);
}
#endif

#if defined(RSOILWAT)
void get_snowpack_mem(OutPeriod pd)
{
	SW_SOILWAT_OUTPUTS *vo = SW_Soilwat.p_oagg[pd];

	RealD *p = p_OUT[eSW_SnowPack][pd];
	get_outvalleader(p, pd);

	p[iOUT(0, pd)] = vo->snowpack;
	p[iOUT(1, pd)] = vo->snowdepth;
}

#elif defined(STEPWAT)
void get_snowpack_agg(OutPeriod pd)
{
	SW_SOILWAT_OUTPUTS *vo = SW_Soilwat.p_oagg[pd];

	RealD
		*p = p_OUT[eSW_SnowPack][pd],
		*psd = p_OUTsd[eSW_SnowPack][pd];

	do_running_agg(p, psd, iOUT(0, pd), Globals.currIter, vo->snowpack);
	do_running_agg(p, psd, iOUT(1, pd), Globals.currIter, vo->snowdepth);

	if (print_IterationSummary) {
		sw_outstr_agg[0] = '\0';
		format_IterationSummary(p, psd, pd, 2);
	}
}
#endif


//------ eSW_DeepSWC
#ifdef SW_OUTTEXT
void get_deepswc_text(OutPeriod pd)
{
	SW_SOILWAT_OUTPUTS *vo = SW_Soilwat.p_oagg[pd];

	sw_outstr[0] = '\0';
	sprintf(sw_outstr, "%c%.*f", _Sep, OUT_DIGITS, vo->deep);
}
#endif

#if defined(RSOILWAT)
void get_deepswc_mem(OutPeriod pd)
{
	SW_SOILWAT_OUTPUTS *vo = SW_Soilwat.p_oagg[pd];

	RealD *p = p_OUT[eSW_DeepSWC][pd];
	get_outvalleader(p, pd);

	p[iOUT(0, pd)] = vo->deep;
}

#elif defined(STEPWAT)
void get_deepswc_agg(OutPeriod pd)
{
	SW_SOILWAT_OUTPUTS *vo = SW_Soilwat.p_oagg[pd];

	RealD
		*p = p_OUT[eSW_DeepSWC][pd],
		*psd = p_OUTsd[eSW_DeepSWC][pd];

	do_running_agg(p, psd, iOUT(0, pd), Globals.currIter, vo->deep);

	if (print_IterationSummary) {
		sw_outstr_agg[0] = '\0';
		format_IterationSummary(p, psd, pd, 1);
	}
}
#endif


//------ eSW_SoilTemp
#ifdef SW_OUTTEXT
void get_soiltemp_text(OutPeriod pd)
{
	LyrIndex i;
	SW_SOILWAT_OUTPUTS *vo = SW_Soilwat.p_oagg[pd];

	char str[OUTSTRLEN];
	sw_outstr[0] = '\0';

	ForEachSoilLayer(i)
	{
		sprintf(str, "%c%.*f", _Sep, OUT_DIGITS, vo->sTemp[i]);
		strcat(sw_outstr, str);
	}
}
#endif

#if defined(RSOILWAT)
void get_soiltemp_mem(OutPeriod pd)
{
	LyrIndex i;
	SW_SOILWAT_OUTPUTS *vo = SW_Soilwat.p_oagg[pd];

	RealD *p = p_OUT[eSW_SoilTemp][pd];
	get_outvalleader(p, pd);

	ForEachSoilLayer(i)
	{
		p[iOUT(i, pd)] = vo->sTemp[i];
	}
}

#elif defined(STEPWAT)
void get_soiltemp_agg(OutPeriod pd)
{
	LyrIndex i;
	SW_SOILWAT_OUTPUTS *vo = SW_Soilwat.p_oagg[pd];

	RealD
		*p = p_OUT[eSW_SoilTemp][pd],
		*psd = p_OUTsd[eSW_SoilTemp][pd];

	ForEachSoilLayer(i)
	{
		do_running_agg(p, psd, iOUT(i, pd), Globals.currIter, vo->sTemp[i]);
	}

	if (print_IterationSummary) {
		sw_outstr_agg[0] = '\0';
		format_IterationSummary(p, psd, pd, SW_Site.n_layers);
	}
}
#endif
