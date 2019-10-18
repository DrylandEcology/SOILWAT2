/********************************************************/
/********************************************************/
/**
  @file
  @brief Format user-specified output for text and/or in-memory processing

  See the \ref out_algo "output algorithm documentation" for details.

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

/**
@brief Output routine for quantities that aren't yet implemented.
			This just gives the main output loop something to call, rather than an
			empty pointer.
@param pd Period.
*/
void get_none(OutPeriod pd)
{

	if (pd) {}
}


//------ eSW_CO2Effects
#ifdef SW_OUTTEXT

/**
@brief Gets CO<SUB>2</SUB> effects by running through each vegetation type if dealing with OUTTEXT.

@param pd Period.
*/
void get_co2effects_text(OutPeriod pd) {
	int k;
	SW_VEGPROD *v = &SW_VegProd;

	char str[OUTSTRLEN];
	sw_outstr[0] = '\0';

	if (pd) {} // hack to silence "-Wunused-parameter"

	ForEachVegType(k) {
		sprintf(str, "%c%.*f", _Sep, OUT_DIGITS,
			v->veg[k].co2_multipliers[BIO_INDEX][SW_Model.simyear]);
		strcat(sw_outstr, str);
	}
	ForEachVegType(k) {
		sprintf(str, "%c%.*f", _Sep, OUT_DIGITS,
			v->veg[k].co2_multipliers[WUE_INDEX][SW_Model.simyear]);
		strcat(sw_outstr, str);
	}
}
#endif

#if defined(RSOILWAT)
void get_co2effects_mem(OutPeriod pd) {
	int k;
	SW_VEGPROD *v = &SW_VegProd;

	RealD *p = p_OUT[eSW_CO2Effects][pd];
	get_outvalleader(p, pd);

	// No averaging or summing required:
	ForEachVegType(k)
	{
		p[iOUT(k, pd)] = v->veg[k].co2_multipliers[BIO_INDEX][SW_Model.simyear];
		p[iOUT(k + NVEGTYPES, pd)] = v->veg[k].co2_multipliers[WUE_INDEX][SW_Model.simyear];
	}
}

#elif defined(STEPWAT)
void get_co2effects_agg(OutPeriod pd) {
	int k;
	SW_VEGPROD *v = &SW_VegProd;

	RealD
		*p = p_OUT[eSW_CO2Effects][pd],
		*psd = p_OUTsd[eSW_CO2Effects][pd];

	ForEachVegType(k)
	{
		do_running_agg(p, psd, iOUT(k, pd), Globals.currIter,
			v->veg[k].co2_multipliers[BIO_INDEX][SW_Model.simyear]);
		do_running_agg(p, psd, iOUT(k + NVEGTYPES, pd), Globals.currIter,
			v->veg[k].co2_multipliers[WUE_INDEX][SW_Model.simyear]);
	}

	if (print_IterationSummary) {
		sw_outstr_agg[0] = '\0';
		format_IterationSummary(p, psd, pd, ncol_OUT[eSW_CO2Effects]);
	}
}
#endif


//------ eSW_Biomass
#ifdef SW_OUTTEXT
void get_biomass_text(OutPeriod pd) {
	int k;
	RealD biomass_total = 0., litter_total = 0., biolive_total = 0.;
	SW_VEGPROD *v = &SW_VegProd;
	SW_VEGPROD_OUTPUTS *vo = SW_VegProd.p_oagg[pd];

	// scale total biomass by fCover to obtain 100% total cover biomass
	ForEachVegType(k)
	{
		biomass_total += vo->veg[k].biomass * v->veg[k].cov.fCover;
		litter_total += vo->veg[k].litter * v->veg[k].cov.fCover;
		biolive_total += vo->veg[k].biolive * v->veg[k].cov.fCover;
	}

	char str[OUTSTRLEN];
	sw_outstr[0] = '\0';

	// fCover for NVEGTYPES plus bare-ground
	sprintf(str, "%c%.*f", _Sep, OUT_DIGITS, v->bare_cov.fCover);
	strcat(sw_outstr, str);
	ForEachVegType(k) {
		sprintf(str, "%c%.*f", _Sep, OUT_DIGITS, v->veg[k].cov.fCover);
		strcat(sw_outstr, str);
	}

	// biomass (g/m2 as component of total) for NVEGTYPES plus totals and litter
	sprintf(str, "%c%.*f", _Sep, OUT_DIGITS, biomass_total);
	strcat(sw_outstr, str);
	ForEachVegType(k) {
		sprintf(str, "%c%.*f", _Sep, OUT_DIGITS,
			vo->veg[k].biomass * v->veg[k].cov.fCover);
		strcat(sw_outstr, str);
	}
	sprintf(str, "%c%.*f", _Sep, OUT_DIGITS, litter_total);
	strcat(sw_outstr, str);

	// biolive (g/m2 as component of total) for NVEGTYPES plus totals
	sprintf(str, "%c%.*f", _Sep, OUT_DIGITS, biolive_total);
	strcat(sw_outstr, str);
	ForEachVegType(k) {
		sprintf(str, "%c%.*f", _Sep, OUT_DIGITS,
			vo->veg[k].biolive * v->veg[k].cov.fCover);
		strcat(sw_outstr, str);
	}
}
#endif

#if defined(RSOILWAT)
void get_biomass_mem(OutPeriod pd) {
	int k, i;
	RealD biomass_total = 0., litter_total = 0., biolive_total = 0.;
	SW_VEGPROD *v = &SW_VegProd;
	SW_VEGPROD_OUTPUTS *vo = SW_VegProd.p_oagg[pd];

	RealD *p = p_OUT[eSW_Biomass][pd];
	get_outvalleader(p, pd);

	// scale total biomass by fCover to obtain 100% total cover biomass
	ForEachVegType(k)
	{
		biomass_total += vo->veg[k].biomass * v->veg[k].cov.fCover;
		litter_total += vo->veg[k].litter * v->veg[k].cov.fCover;
		biolive_total += vo->veg[k].biolive * v->veg[k].cov.fCover;
	}

	// fCover for NVEGTYPES plus bare-ground
	p[iOUT(0, pd)] = v->bare_cov.fCover;
	i = 1;
	ForEachVegType(k)
	{
		p[iOUT(i + k, pd)] = v->veg[k].cov.fCover;
	}

	// biomass (g/m2 as component of total) for NVEGTYPES plus totals and litter
	p[iOUT(i + NVEGTYPES, pd)] = biomass_total;
	i += NVEGTYPES + 1;
	ForEachVegType(k) {
		p[iOUT(i + k, pd)] = vo->veg[k].biomass * v->veg[k].cov.fCover;
	}
	p[iOUT(i + NVEGTYPES, pd)] = litter_total;

	// biolive (g/m2 as component of total) for NVEGTYPES plus totals
	p[iOUT(i + NVEGTYPES + 1, pd)] = biolive_total;
	i += NVEGTYPES + 2;
	ForEachVegType(k) {
		p[iOUT(i + k, pd)] = vo->veg[k].biolive * v->veg[k].cov.fCover;
	}
}

#elif defined(STEPWAT)
void get_biomass_agg(OutPeriod pd) {
	int k, i;
	RealD biomass_total = 0., litter_total = 0., biolive_total = 0.;
	SW_VEGPROD *v = &SW_VegProd;
	SW_VEGPROD_OUTPUTS *vo = SW_VegProd.p_oagg[pd];

	RealD
		*p = p_OUT[eSW_Biomass][pd],
		*psd = p_OUTsd[eSW_Biomass][pd];

	// scale total biomass by fCover to obtain 100% total cover biomass
	ForEachVegType(k)
	{
		biomass_total += vo->veg[k].biomass * v->veg[k].cov.fCover;
		litter_total += vo->veg[k].litter * v->veg[k].cov.fCover;
		biolive_total += vo->veg[k].biolive * v->veg[k].cov.fCover;
	}

	// fCover for NVEGTYPES plus bare-ground
	do_running_agg(p, psd, iOUT(0, pd), Globals.currIter, v->bare_cov.fCover);
	i = 1;
	ForEachVegType(k)
	{
		do_running_agg(p, psd, iOUT(i + k, pd), Globals.currIter,
			v->veg[k].cov.fCover);
	}

	// biomass (g/m2 as component of total) for NVEGTYPES plus totals and litter
	do_running_agg(p, psd, iOUT(i + NVEGTYPES, pd), Globals.currIter,
		biomass_total);
	i += NVEGTYPES + 1;
	ForEachVegType(k) {
		do_running_agg(p, psd, iOUT(i + k, pd), Globals.currIter,
			vo->veg[k].biomass * v->veg[k].cov.fCover);
	}
	do_running_agg(p, psd, iOUT(i + NVEGTYPES, pd), Globals.currIter,
		litter_total);

	// biolive (g/m2 as component of total) for NVEGTYPES plus totals
	do_running_agg(p, psd, iOUT(i + NVEGTYPES + 1, pd), Globals.currIter,
		biolive_total);
	i += NVEGTYPES + 2;
	ForEachVegType(k) {
		do_running_agg(p, psd, iOUT(i + k, pd), Globals.currIter,
			vo->veg[k].biolive * v->veg[k].cov.fCover);
	}

	if (print_IterationSummary) {
		sw_outstr_agg[0] = '\0';
		format_IterationSummary(p, psd, pd, ncol_OUT[eSW_Biomass]);
	}
}
#endif



//------ eSW_Estab
/* --------------------------------------------------- */

#ifdef SW_OUTTEXT

/**
@brief The establishment check produces, for each species in the given set,
			a day of year >= 0 that the species established itself in the current year.
			The output will be a single row of numbers for each year. Each column
			represents a species in order it was entered in the stabs.in file. The
			value will be the day that the species established, or - if it didn't
			establish this year.  This check is for OUTTEXT.

@param pd Period.
*/
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
/**
@brief The establishment check produces, for each species in the given set,
			a day of year >= 0 that the species established itself in the current year.
			The output will be a single row of numbers for each year. Each column
			represents a species in order it was entered in the stabs.in file. The
			value will be the day that the species established, or - if it didn't
			establish this year.  This check is for RSOILWAT.

@param pd Period.
*/
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
/**
@brief The establishment check produces, for each species in the given set,
			a day of year >= 0 that the species established itself in the current year.
			The output will be a single row of numbers for each year. Each column
			represents a species in order it was entered in the stabs.in file. The
			value will be the day that the species established, or - if it didn't
			establish this year.  This check is for STEPWAT.

@param pd Period.
*/
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
		format_IterationSummary(p, psd, pd, ncol_OUT[eSW_Estab]);
	}
}
#endif


//------ eSW_Temp
#ifdef SW_OUTTEXT

/**
@brief Gets temp text from SW_WEATHER_OUTPUTS when dealing with OUTTEXT.

@param pd Period.
*/
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

/**
@brief Gets temp text from SW_WEATHER_OUTPUTS when dealing with RSOILWAT.

@param pd Period.
*/
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

/**
@brief Gets temp text from SW_WEATHER_OUTPUTS when dealing with STEPWAT.

@param pd Period.
*/
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
		format_IterationSummary(p, psd, pd, ncol_OUT[eSW_Temp]);
	}
}

/**
@brief STEPWAT2 expects annual mean air temperature

@param pd Period.
*/
void get_temp_SXW(OutPeriod pd)
{
	if (pd == eSW_Month || pd == eSW_Year) {
		SW_WEATHER_OUTPUTS *vo = SW_Weather.p_oagg[pd];

		if (pd == eSW_Month) {
			SXW.temp_monthly[SW_Model.month - tOffset] = vo->temp_avg;
		}
		else if (pd == eSW_Year) {
			SXW.temp = vo->temp_avg;
		}
	}
}
#endif


//------ eSW_Precip
#ifdef SW_OUTTEXT

/**
@brief Gets precipitation text from SW_WEATHER_OUTPUTS when dealing with OUTTEXT.

@param pd Period.
*/
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

/**
@brief Gets precipitation text from SW_WEATHER_OUTPUTS when dealing with RSOILWAT.

@param pd Period.
*/
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

/**
@brief Gets precipitation text from SW_WEATHER_OUTPUTS when dealing with STEPWAT.

@param pd Period.
*/
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
		format_IterationSummary(p, psd, pd, ncol_OUT[eSW_Precip]);
	}
}

/**
@brief STEPWAT2 expects monthly and annual sum of precipitation

@param pd Period.
*/
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

/**
@brief Gets vwcBulk text from SW_SOILWAT_OUTPUTS when dealing with OUTTEXT.

@param pd Period.
*/
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

/**
@brief Gets vwcBulk text from SW_SOILWAT_OUTPUTS when dealing with RSOILWAT.

@param pd Period.
*/
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

/**
@brief Gets vwcBulk text from SW_SOILWAT_OUTPUTS when dealing with STEPWAT.

@param pd Period.
*/
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
		format_IterationSummary(p, psd, pd, ncol_OUT[eSW_VWCBulk]);
	}
}
#endif


//------ eSW_VWCMatric
#ifdef SW_OUTTEXT

/**
@brief Gets vwcMatric text from SW_SOILWAT_OUTPUTS when dealing with OUTTEXT.

@param pd Period.
*/
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

/**
@brief Gets vwcMatric text from SW_SOILWAT_OUTPUTS when dealing with RSOILWAT.

@param pd Period.
*/
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

/**
@brief Gets vwcMatric text from SW_SOILWAT_OUTPUTS when dealing with STEPWAT.

@param pd Period.
*/
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
		format_IterationSummary(p, psd, pd, ncol_OUT[eSW_VWCMatric]);
	}
}
#endif


//------ eSW_SWA
#ifdef SW_OUTTEXT

/**
@brief Gets SWA text from SW_SOILWAT_OUTPUTS when dealing with OUTTEXT.

@param pd Period.
*/
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

/**
@brief Gets SWA text from SW_SOILWAT_OUTPUTS when dealing with RSOILWAT.

@param pd Period.
*/
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

/**
@brief Gets SWA text from SW_SOILWAT_OUTPUTS when dealing with STEPWAT.

@param pd Period.
*/
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

/**
@brief Gets swcBulk text from SW_SOILWAT_OUTPUTS when dealing with OUTTEXT.

@param pd Period.
*/
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

/**
@brief Gets swcBulk text from SW_SOILWAT_OUTPUTS when dealing with RSOILWAT.

@param pd Period.
*/
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

/**
@brief Gets swcBulk text from SW_SOILWAT_OUTPUTS when dealing with STEPWAT.

@param pd Period.
*/
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
		format_IterationSummary(p, psd, pd, ncol_OUT[eSW_SWCBulk]);
	}
}

/**
@brief STEPWAT2 expects monthly mean SWCbulk by soil layer.

@param pd Period.
*/
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
#ifdef SW_OUTTEXT

/**
@brief eSW_SWPMatric Can't take arithmetic average of swp vecause its exponentail.
			At this time (until I rewmember to look up whether harmonic or some other
			average is better and fix this) we're not averaging swp but converting
			the averged swc.  This also avoids converting for each day. added 12-Oct-03, cwb

@param pd Period.
*/
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

/**
@brief Gets swpMatric when dealing with RSOILWAT

@param pd Period.
*/
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

/**
@brief Gets swpMatric when dealing with STEPWAT.

@param pd Period.
*/
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
		format_IterationSummary(p, psd, pd, ncol_OUT[eSW_SWPMatric]);
	}
}
#endif


//------ eSW_SWABulk
#ifdef SW_OUTTEXT

/**
@brief gets swaBulk when dealing with OUTTEXT.

@param pd Period.
*/
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

/**
@brief Gets swaBulk when dealing with RSOILWAT.

@param pd Period.
*/
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

/**
@brief Gets swaBulk when dealing with STEPWAT.

@param pd Period.
*/
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
		format_IterationSummary(p, psd, pd, ncol_OUT[eSW_SWABulk]);
	}
}
#endif

//------ eSW_SWAMatric
#ifdef SW_OUTTEXT

/**
@brief Gets swaMatric when dealing with OUTTEXT.

@param pd Period.
*/
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

/**
@brief Gets swaMatric when dealing with RSOILWAT.

@param pd Period.
*/
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

/**
@brief Gets swaMatric when dealing with STEPWAT.

@param pd Period.
*/
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
		format_IterationSummary(p, psd, pd, ncol_OUT[eSW_SWAMatric]);
	}
}
#endif


//------ eSW_SurfaceWater
#ifdef SW_OUTTEXT

/**
@brief Gets surfaceWater when dealing with OUTTEXT.

@param pd Period.
*/
void get_surfaceWater_text(OutPeriod pd)
{
	SW_SOILWAT_OUTPUTS *vo = SW_Soilwat.p_oagg[pd];

	sw_outstr[0] = '\0';
	sprintf(sw_outstr, "%c%.*f", _Sep, OUT_DIGITS, vo->surfaceWater);
}
#endif

#if defined(RSOILWAT)

/**
@brief Gets surfaceWater when dealing with RSOILWAT.

@param pd Period.
*/
void get_surfaceWater_mem(OutPeriod pd)
{
	SW_SOILWAT_OUTPUTS *vo = SW_Soilwat.p_oagg[pd];

	RealD *p = p_OUT[eSW_SurfaceWater][pd];
	get_outvalleader(p, pd);

	p[iOUT(0, pd)] = vo->surfaceWater;
}

#elif defined(STEPWAT)

/**
@brief Gets surfaceWater when dealing with STEPWAT.

@param pd Period.
*/
void get_surfaceWater_agg(OutPeriod pd)
{
	SW_SOILWAT_OUTPUTS *vo = SW_Soilwat.p_oagg[pd];

	RealD
		*p = p_OUT[eSW_SurfaceWater][pd],
		*psd = p_OUTsd[eSW_SurfaceWater][pd];

	do_running_agg(p, psd, iOUT(0, pd), Globals.currIter, vo->surfaceWater);

	if (print_IterationSummary) {
		sw_outstr_agg[0] = '\0';
		format_IterationSummary(p, psd, pd, ncol_OUT[eSW_SurfaceWater]);
	}
}
#endif


//------ eSW_Runoff
#ifdef SW_OUTTEXT

/**
@brief Gets surfaceRunon, surfaceRunoff, and snowRunoff when dealing with OUTTEXT.

@param pd Period.
*/
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

/**
@brief Gets surfaceRunon, surfaceRunoff, and snowRunoff when dealing with RSOILWAT.

@param pd Period.
*/
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

/**
@brief Gets surfaceRunon, surfaceRunoff, and snowRunoff when dealing with STEPWAT.

@param pd Period.
*/
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
		format_IterationSummary(p, psd, pd, ncol_OUT[eSW_Runoff]);
	}
}
#endif

//------ eSW_Transp
#ifdef SW_OUTTEXT

/**
@brief Gets transp_total when dealing with OUTTEXT.

@param pd Period.
*/
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

/**
@brief Gets transp_total when dealing with RSOILWAT.

@param pd Period.
*/
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

/**
@brief Gets transp_total when dealing with STEPWAT.

@param pd Period.
*/
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

/**
@brief STEPWAT2 expects monthly sum of transpiration by soil layer. <BR>
				see function '_transp_contribution_by_group'

@param pd Period.
*/
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

/**
@brief Gets evap when dealing with OUTTEXT.

@brief pd Period.
*/
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

/**
@brief Gets evap when dealing with RSOILWAT.

@param pd Period.
*/
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

/**
@brief Gets evap when dealing with STEPWAT.

@param pd Period.
*/
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
		format_IterationSummary(p, psd, pd, ncol_OUT[eSW_EvapSoil]);
	}
}
#endif


//------ eSW_EvapSurface
#ifdef SW_OUTTEXT

/**
@brief Gets evapSurface when dealing with OUTTEXT.

@param pd Period.
*/
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

/**
@brief Gets evapSurface when dealing with RSOILWAT.

@param pd Period.
*/
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

/**
@brief Gets evapSurface when dealing with STEPWAT.

@param pd Period.
*/
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
		format_IterationSummary(p, psd, pd, ncol_OUT[eSW_EvapSurface]);
	}
}
#endif

//------ eSW_Interception
#ifdef SW_OUTTEXT

/**
@brief Gets total_int, int_veg, and litter_int when dealing with OUTTEXT.

@param pd Period.
*/
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

/**
@brief Gets total_int, int_veg, and litter_int when dealing with RSOILWAT.

@param pd Period.
*/
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

/**
@brief Gets total_int, int_veg, and litter_int when dealing with STEPWAT.

@param pd Period.
*/
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
		format_IterationSummary(p, psd, pd, ncol_OUT[eSW_Interception]);
	}
}
#endif

//------ eSW_SoilInf
#ifdef SW_OUTTEXT

/**
@brief Gets soil_inf when dealing with OUTTEXT.

@param pd Period.
*/
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

/**
@brief Gets soil_inf when dealing with RSOILWAT.

@param pd Period.
*/
void get_soilinf_mem(OutPeriod pd)
{
	SW_WEATHER_OUTPUTS *vo = SW_Weather.p_oagg[pd];

	RealD *p = p_OUT[eSW_SoilInf][pd];
	get_outvalleader(p, pd);

	p[iOUT(0, pd)] = vo->soil_inf;
}

#elif defined(STEPWAT)

/**
@brief Gets soil_inf when dealing with STEPWAT.

@param pd Period.
*/
void get_soilinf_agg(OutPeriod pd)
{
	SW_WEATHER_OUTPUTS *vo = SW_Weather.p_oagg[pd];

	RealD
		*p = p_OUT[eSW_SoilInf][pd],
		*psd = p_OUTsd[eSW_SoilInf][pd];

	do_running_agg(p, psd, iOUT(0, pd), Globals.currIter, vo->soil_inf);

	if (print_IterationSummary) {
		sw_outstr_agg[0] = '\0';
		format_IterationSummary(p, psd, pd, ncol_OUT[eSW_SoilInf]);
	}
}
#endif


//------ eSW_LyrDrain
#ifdef SW_OUTTEXT

/**
@brief Gets lyrdrain when dealing with OUTTEXT.

@param pd Period.
*/
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

/**
@brief Gets lyrdrain when dealing with RSOILWAT.

@param pd Period.
*/
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

/**
@brief Gets lyrdrain when dealing with STEPWAT.

@param pd Period.
*/
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
		format_IterationSummary(p, psd, pd, ncol_OUT[eSW_LyrDrain]);
	}
}
#endif


//------ eSW_HydRed
#ifdef SW_OUTTEXT

/**
@brief Gets hydred and hydred_total when dealing with OUTTEXT.

@param pd Period.
*/
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

/**
@brief Gets hydred and hydred_total when dealing with RSOILWAT.

@param pd Period.
*/
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

/**
@brief Gets hydred and hydred_total when dealing with STEPWAT.

@param pd Period.
*/
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

/**
@brief Gets actual evapotranspiration when dealing with OUTTEXT.

@param pd Period.
*/
void get_aet_text(OutPeriod pd)
{
	SW_SOILWAT_OUTPUTS *vo = SW_Soilwat.p_oagg[pd];

	sw_outstr[0] = '\0';
	sprintf(sw_outstr, "%c%.*f", _Sep, OUT_DIGITS, vo->aet);
}
#endif

#if defined(RSOILWAT)

/**
@brief Gets actual evapotranspiration when dealing with OUTTEXT.

@param pd Period.
*/
void get_aet_mem(OutPeriod pd)
{
	SW_SOILWAT_OUTPUTS *vo = SW_Soilwat.p_oagg[pd];

	RealD *p = p_OUT[eSW_AET][pd];
	get_outvalleader(p, pd);

	p[iOUT(0, pd)] = vo->aet;
}

#elif defined(STEPWAT)

/**
@brief Gets actual evapotranspiration when dealing with STEPWAT.

@param pd Period.
*/
void get_aet_agg(OutPeriod pd)
{
	SW_SOILWAT_OUTPUTS *vo = SW_Soilwat.p_oagg[pd];

	RealD
		*p = p_OUT[eSW_AET][pd],
		*psd = p_OUTsd[eSW_AET][pd];

	do_running_agg(p, psd, iOUT(0, pd), Globals.currIter, vo->aet);

	if (print_IterationSummary) {
		sw_outstr_agg[0] = '\0';
		format_IterationSummary(p, psd, pd, ncol_OUT[eSW_AET]);
	}
}

/**
@brief STEPWAT2 expects annual sum of actual evapotranspiration

@param pd Period.
*/
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

/**
@brief Gets potential evapotranspiration when dealing with OUTTEXT.

@param pd Period.
*/
void get_pet_text(OutPeriod pd)
{
	SW_SOILWAT_OUTPUTS *vo = SW_Soilwat.p_oagg[pd];

	sw_outstr[0] = '\0';
	sprintf(sw_outstr, "%c%.*f", _Sep, OUT_DIGITS, vo->pet);
}
#endif

#if defined(RSOILWAT)

/**
@brief Gets potential evapotranspiration when dealing with OUTTEXT.

@param pd Period.
*/
void get_pet_mem(OutPeriod pd)
{
	SW_SOILWAT_OUTPUTS *vo = SW_Soilwat.p_oagg[pd];

	RealD *p = p_OUT[eSW_PET][pd];
	get_outvalleader(p, pd);

	p[iOUT(0, pd)] = vo->pet;
}

#elif defined(STEPWAT)

/**
@brief Gets potential evapotranspiration when dealing with OUTTEXT.
*/
void get_pet_agg(OutPeriod pd)
{
	SW_SOILWAT_OUTPUTS *vo = SW_Soilwat.p_oagg[pd];

	RealD
		*p = p_OUT[eSW_PET][pd],
		*psd = p_OUTsd[eSW_PET][pd];

	do_running_agg(p, psd, iOUT(0, pd), Globals.currIter, vo->pet);

	if (print_IterationSummary) {
		sw_outstr_agg[0] = '\0';
		format_IterationSummary(p, psd, pd, ncol_OUT[eSW_PET]);
	}
}
#endif


//------ eSW_WetDays
#ifdef SW_OUTTEXT

/**
@brief Gets is_wet and wetdays when dealing with OUTTEXT.

@param pd Period.
*/
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

/**
@brief Gets is_wet and wetdays when dealing with RSOILWAT.

@param pd Period.
*/
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

/**
@brief Gets is_wet and wetdays when dealing with RSOILWAT.

@param pd Period.
*/
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
		format_IterationSummary(p, psd, pd, ncol_OUT[eSW_WetDays]);
	}
}
#endif


//------ eSW_SnowPack
#ifdef SW_OUTTEXT

/**
@brief Gets snowpack and snowdepth when dealing with OUTTEXT.

@param pd Period.
*/
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

/**
@brief Gets snowpack and snowdepth when dealing with OUTTEXT.

@param pd Period.
*/
void get_snowpack_mem(OutPeriod pd)
{
	SW_SOILWAT_OUTPUTS *vo = SW_Soilwat.p_oagg[pd];

	RealD *p = p_OUT[eSW_SnowPack][pd];
	get_outvalleader(p, pd);

	p[iOUT(0, pd)] = vo->snowpack;
	p[iOUT(1, pd)] = vo->snowdepth;
}

#elif defined(STEPWAT)

/**
@brief Gets snowpack and snowdepth when dealing with STEPWAT.

@param pd Period.
*/
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
		format_IterationSummary(p, psd, pd, ncol_OUT[eSW_SnowPack]);
	}
}
#endif


//------ eSW_DeepSWC
#ifdef SW_OUTTEXT

/**
@brief Gets deep for when dealing with OUTTEXT.

@param pd Period.
*/
void get_deepswc_text(OutPeriod pd)
{
	SW_SOILWAT_OUTPUTS *vo = SW_Soilwat.p_oagg[pd];

	sw_outstr[0] = '\0';
	sprintf(sw_outstr, "%c%.*f", _Sep, OUT_DIGITS, vo->deep);
}
#endif

#if defined(RSOILWAT)

/**
@brief Gets deep for when dealing with RSOILWAT.

@param pd Period.
*/
void get_deepswc_mem(OutPeriod pd)
{
	SW_SOILWAT_OUTPUTS *vo = SW_Soilwat.p_oagg[pd];

	RealD *p = p_OUT[eSW_DeepSWC][pd];
	get_outvalleader(p, pd);

	p[iOUT(0, pd)] = vo->deep;
}

#elif defined(STEPWAT)

/**
@brief Gets deep for when dealing with STEPWAT.

@param pd Period.
*/
void get_deepswc_agg(OutPeriod pd)
{
	SW_SOILWAT_OUTPUTS *vo = SW_Soilwat.p_oagg[pd];

	RealD
		*p = p_OUT[eSW_DeepSWC][pd],
		*psd = p_OUTsd[eSW_DeepSWC][pd];

	do_running_agg(p, psd, iOUT(0, pd), Globals.currIter, vo->deep);

	if (print_IterationSummary) {
		sw_outstr_agg[0] = '\0';
		format_IterationSummary(p, psd, pd, ncol_OUT[eSW_DeepSWC]);
	}
}
#endif


//------ eSW_SoilTemp
#ifdef SW_OUTTEXT

/**
@brief Gets soil temperature for when dealing with OUTTEXT.

@param pd Period.
*/
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

/**
@brief Gets soil temperature for when dealing with RSOILWAT.

@param pd Period.
*/
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

/**
@brief Gets soil temperature for when dealing with STEPWAT.

@param pd Period.
*/
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
		format_IterationSummary(p, psd, pd, ncol_OUT[eSW_SoilTemp]);
	}
}
#endif
