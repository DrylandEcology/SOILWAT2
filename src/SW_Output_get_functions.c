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

#include "include/Times.h"

#include "include/SW_Carbon.h"
#include "include/SW_Model.h"
#include "include/SW_Site.h"
#include "include/SW_SoilWater.h"
#include "include/SW_VegEstab.h"
#include "include/SW_VegProd.h"

#include "include/SW_Output.h"

#ifdef RSOILWAT
#include <R.h>
#include <Rdefines.h>
#include <Rinternals.h>
#endif

#ifdef STEPWAT
#include <math.h>
#endif

// Array-based output declarations:
#if defined(SW_OUTARRAY)
// externs `ncol_TimeOUT`
#include "include/SW_Output_outarray.h"
#endif

// Text-based output declarations:
#if defined(SW_OUTTEXT)

#include "include/SW_Output_outtext.h"
#endif



/* =================================================== */
/*             Local Function Definitions              */
/* --------------------------------------------------- */

#ifdef STEPWAT
static void format_IterationSummary(RealD *p, RealD *psd, OutPeriod pd, IntUS N,
									SW_ALL *sw)
{
	IntUS i;
	size_t n;
	RealD sd;
	char str[OUTSTRLEN];

	for (i = 0; i < N; i++)
	{
		n = iOUT(i, pd, sw->GenOutput);
		sd = final_running_sd(sw->Model.runModelIterations, psd[n]);

		snprintf(
			str,
			OUTSTRLEN,
			"%c%.*f%c%.*f",
			_OUTSEP, OUT_DIGITS, p[n], _OUTSEP, OUT_DIGITS, sd
		);
		strcat(sw->GenOutput.sw_outstr_agg, str);
	}
}

static void format_IterationSummary2(RealD *p, RealD *psd, OutPeriod pd,
	IntUS N1, IntUS offset, SW_ALL *sw)
{
	IntUS k, i;
	size_t n;
	RealD sd;
	char str[OUTSTRLEN];

	for (k = 0; k < N1; k++)
	{
		for (i = 0; i < sw->Site.n_layers; i++)
		{
			n = iOUT2(i, k + offset, pd, sw->GenOutput, sw->Site.n_layers);
			sd = final_running_sd(sw->Model.runModelIterations, psd[n]);

			snprintf(str, OUTSTRLEN, "%c%.*f%c%.*f",
				_OUTSEP, OUT_DIGITS, p[n], _OUTSEP, OUT_DIGITS, sd);
			strcat(sw->GenOutput.sw_outstr_agg, str);
		}
	}
}

#endif



/* =================================================== */
/*             Global Function Definitions             */
/*             (declared in SW_Output.h)               */
/* --------------------------------------------------- */

/**
@brief Output routine for quantities that aren't yet implemented.
			This just gives the main output loop something to call, rather than an
			empty pointer.

@param[in] pd Time period in simulation output (day/week/month/year)
@param[in] sw Comprehensive struct of type SW_ALL containing all information
  in the simulation.
*/
void get_none(OutPeriod pd, SW_ALL* sw)
{
	if (pd) {}

	(void) sw; // Coerce to void to silence compiler
}


//------ eSW_CO2Effects
#ifdef SW_OUTTEXT

/**
@brief Gets CO<SUB>2</SUB> effects by running through each vegetation type if dealing with OUTTEXT.

@param[in] pd Time period in simulation output (day/week/month/year)
@param[in] sw Comprehensive struct of type SW_ALL containing all information
  in the simulation.
*/
void get_co2effects_text(OutPeriod pd, SW_ALL* sw) {
	int k;

	char str[OUTSTRLEN];
	sw->GenOutput.sw_outstr[0] = '\0';
	TimeInt simyear = sw->Model.simyear;

	if (pd) {} // hack to silence "-Wunused-parameter"

	ForEachVegType(k) {
		snprintf(str, OUTSTRLEN, "%c%.*f", _OUTSEP, OUT_DIGITS,
			sw->VegProd.veg[k].co2_multipliers[BIO_INDEX][simyear]);
		strcat(sw->GenOutput.sw_outstr, str);
	}
	ForEachVegType(k) {
		snprintf(str, OUTSTRLEN, "%c%.*f", _OUTSEP, OUT_DIGITS,
			sw->VegProd.veg[k].co2_multipliers[WUE_INDEX][simyear]);
		strcat(sw->GenOutput.sw_outstr, str);
	}
}
#endif

#if defined(RSOILWAT) || defined(SWNETCDF)
void get_co2effects_mem(OutPeriod pd, SW_ALL* sw) {
	int k;
	size_t iOUTIndex;

	RealD *p = sw->GenOutput.p_OUT[eSW_CO2Effects][pd];

    #if defined(RSOILWAT)
	get_outvalleader(&sw->Model, pd, sw->GenOutput.irow_OUT,
					 sw->GenOutput.nrow_OUT, sw->GenOutput.tOffset, p);
    #endif

	// No averaging or summing required:
	ForEachVegType(k)
	{
		iOUTIndex = iOUT(k, pd, sw->GenOutput);
		p[iOUTIndex] =
			sw->VegProd.veg[k].co2_multipliers[BIO_INDEX][sw->Model.simyear];

		iOUTIndex = iOUT(k + NVEGTYPES, pd, sw->GenOutput);
		p[iOUTIndex] =
			sw->VegProd.veg[k].co2_multipliers[WUE_INDEX][sw->Model.simyear];
	}
}

#elif defined(STEPWAT)
void get_co2effects_agg(OutPeriod pd, SW_ALL* sw) {
	int k;
	size_t iOUTIndex;

	RealD
		*p = sw->GenOutput.p_OUT[eSW_CO2Effects][pd],
		*psd = sw->GenOutput.p_OUTsd[eSW_CO2Effects][pd];

	ForEachVegType(k)
	{
		iOUTIndex = iOUT(k, pd, sw->GenOutput);
		do_running_agg(p, psd, iOUTIndex, sw->GenOutput.currIter,
					  sw->VegProd.veg[k].co2_multipliers[BIO_INDEX][sw->Model.simyear]);

		iOUTIndex = iOUT(k + NVEGTYPES, pd, sw->GenOutput);
		do_running_agg(p, psd, iOUTIndex, sw->GenOutput.currIter,
					  sw->VegProd.veg[k].co2_multipliers[WUE_INDEX][sw->Model.simyear]);
	}

	if (sw->GenOutput.print_IterationSummary) {
		sw->GenOutput.sw_outstr_agg[0] = '\0';
		format_IterationSummary(p, psd, pd, sw->GenOutput.ncol_OUT[eSW_CO2Effects], sw);
	}
}
#endif


//------ eSW_Biomass
#ifdef SW_OUTTEXT
void get_biomass_text(OutPeriod pd, SW_ALL* sw) {
	int k;
	SW_VEGPROD_OUTPUTS *vo = sw->VegProd.p_oagg[pd];

	char str[OUTSTRLEN];
	sw->GenOutput.sw_outstr[0] = '\0';

	// fCover for NVEGTYPES plus bare-ground
	snprintf(str, OUTSTRLEN, "%c%.*f", _OUTSEP, OUT_DIGITS,
											sw->VegProd.bare_cov.fCover);
	strcat(sw->GenOutput.sw_outstr, str);
	ForEachVegType(k) {
		snprintf(str, OUTSTRLEN, "%c%.*f", _OUTSEP, OUT_DIGITS,
												sw->VegProd.veg[k].cov.fCover);
		strcat(sw->GenOutput.sw_outstr, str);
	}

	// biomass (g/m2 as component of total) for NVEGTYPES plus totals and litter
	snprintf(str, OUTSTRLEN, "%c%.*f", _OUTSEP, OUT_DIGITS, vo->biomass_total);
	strcat(sw->GenOutput.sw_outstr, str);
	ForEachVegType(k) {
		snprintf(str, OUTSTRLEN, "%c%.*f", _OUTSEP, OUT_DIGITS, vo->veg[k].biomass_inveg);
		strcat(sw->GenOutput.sw_outstr, str);
	}
	snprintf(str, OUTSTRLEN, "%c%.*f", _OUTSEP, OUT_DIGITS, vo->litter_total);
	strcat(sw->GenOutput.sw_outstr, str);

	// biolive (g/m2 as component of total) for NVEGTYPES plus totals
	snprintf(str, OUTSTRLEN, "%c%.*f", _OUTSEP, OUT_DIGITS, vo->biolive_total);
	strcat(sw->GenOutput.sw_outstr, str);
	ForEachVegType(k) {
		snprintf(str, OUTSTRLEN, "%c%.*f", _OUTSEP, OUT_DIGITS, vo->veg[k].biolive_inveg);
		strcat(sw->GenOutput.sw_outstr, str);
	}

	// leaf area index [m2/m2]
	snprintf(str, OUTSTRLEN, "%c%.*f", _OUTSEP, OUT_DIGITS, vo->LAI);
	strcat(sw->GenOutput.sw_outstr, str);
}
#endif

#if defined(RSOILWAT) || defined(SWNETCDF)
void get_biomass_mem(OutPeriod pd, SW_ALL* sw) {
	int k, i;
	SW_VEGPROD_OUTPUTS *vo = sw->VegProd.p_oagg[pd];
	size_t iOUTIndex;

	RealD *p = sw->GenOutput.p_OUT[eSW_Biomass][pd];

    #if defined(RSOILWAT)
	get_outvalleader(&sw->Model, pd, sw->GenOutput.irow_OUT,
					sw->GenOutput.nrow_OUT, sw->GenOutput.tOffset, p);
    #endif

	// fCover for NVEGTYPES plus bare-ground
	iOUTIndex = iOUT(0, pd, sw->GenOutput);
	p[iOUTIndex] = sw->VegProd.bare_cov.fCover;
	i = 1;
	ForEachVegType(k)
	{
		iOUTIndex = iOUT(i + k, pd, sw->GenOutput);
		p[iOUTIndex] = sw->VegProd.veg[k].cov.fCover;
	}

	// biomass (g/m2 as component of total) for NVEGTYPES plus totals and litter
	iOUTIndex = iOUT(i + NVEGTYPES, pd, sw->GenOutput);
	p[iOUTIndex] = vo->biomass_total;
	i += NVEGTYPES + 1;
	ForEachVegType(k) {
		iOUTIndex = iOUT(i + k, pd, sw->GenOutput);
		p[iOUTIndex] = vo->veg[k].biomass_inveg;
	}

	iOUTIndex = iOUT(i + NVEGTYPES, pd, sw->GenOutput);
	p[iOUTIndex] = vo->litter_total;

	// biolive (g/m2 as component of total) for NVEGTYPES plus totals
	iOUTIndex = iOUT(i + NVEGTYPES + 1, pd, sw->GenOutput);
	p[iOUTIndex] = vo->biolive_total;
	i += NVEGTYPES + 2;

	ForEachVegType(k) {
		iOUTIndex = iOUT(i + k, pd, sw->GenOutput);
		p[iOUTIndex] = vo->veg[k].biolive_inveg;
	}

	// leaf area index [m2/m2]
	iOUTIndex = iOUT(i + NVEGTYPES, pd, sw->GenOutput);
	p[iOUTIndex] = vo->LAI;
}

#elif defined(STEPWAT)
void get_biomass_agg(OutPeriod pd, SW_ALL* sw) {
	int k, i;
	SW_VEGPROD_OUTPUTS *vo = sw->VegProd.p_oagg[pd];
	size_t iOUTIndex;

	RealD
		*p = sw->GenOutput.p_OUT[eSW_Biomass][pd],
		*psd = sw->GenOutput.p_OUTsd[eSW_Biomass][pd];

	// fCover for NVEGTYPES plus bare-ground
	iOUTIndex = iOUT(0, pd, sw->GenOutput);
	do_running_agg(p, psd, iOUTIndex, sw->GenOutput.currIter,
				   sw->VegProd.bare_cov.fCover);
	i = 1;
	ForEachVegType(k)
	{
		iOUTIndex = iOUT(i + k, pd, sw->GenOutput);
		do_running_agg(p, psd, iOUTIndex, sw->GenOutput.currIter,
					   sw->VegProd.veg[k].cov.fCover);
	}

	// biomass (g/m2 as component of total) for NVEGTYPES plus totals and litter
	iOUTIndex = iOUT(i + NVEGTYPES, pd, sw->GenOutput);
	do_running_agg(p, psd, iOUTIndex, sw->GenOutput.currIter, vo->biomass_total);
	i += NVEGTYPES + 1;

	ForEachVegType(k) {
		iOUTIndex = iOUT(i + k, pd, sw->GenOutput);
		do_running_agg(p, psd, iOUTIndex, sw->GenOutput.currIter,
					   vo->veg[k].biomass_inveg);
	}

	iOUTIndex = iOUT(i + NVEGTYPES, pd, sw->GenOutput);
	do_running_agg(p, psd, iOUTIndex, sw->GenOutput.currIter, vo->litter_total);

	// biolive (g/m2 as component of total) for NVEGTYPES plus totals
	iOUTIndex = iOUT(i + NVEGTYPES + 1, pd, sw->GenOutput);
	do_running_agg(p, psd, iOUTIndex, sw->GenOutput.currIter, vo->biolive_total);
	i += NVEGTYPES + 2;
	ForEachVegType(k) {
		iOUTIndex = iOUT(i + k, pd, sw->GenOutput);
		do_running_agg(p, psd, iOUTIndex, sw->GenOutput.currIter,
					   vo->veg[k].biolive_inveg);
	}

	// leaf area index [m2/m2]
	iOUTIndex = iOUT(i + NVEGTYPES, pd, sw->GenOutput);
	do_running_agg(p, psd, iOUTIndex, sw->GenOutput.currIter, vo->LAI);

	if (sw->GenOutput.print_IterationSummary) {
		sw->GenOutput.sw_outstr_agg[0] = '\0';
		format_IterationSummary(p, psd, pd, sw->GenOutput.ncol_OUT[eSW_Biomass], sw);
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

@param[in] pd Time period in simulation output (day/week/month/year)
@param[in] sw Comprehensive struct of type SW_ALL containing all information
  in the simulation.
*/
void get_estab_text(OutPeriod pd, SW_ALL* sw)
{
	IntU i;

	char str[OUTSTRLEN];
	sw->GenOutput.sw_outstr[0] = '\0';

	i = (IntU) pd; // silence `-Wunused-parameter`

	for (i = 0; i < sw->VegEstab.count; i++)
	{
		snprintf(str, OUTSTRLEN, "%c%d", _OUTSEP, sw->VegEstab.parms[i]->estab_doy);
		strcat(sw->GenOutput.sw_outstr, str);
	}
}
#endif

#if defined(RSOILWAT) || defined(SWNETCDF)
/**
@brief The establishment check produces, for each species in the given set,
			a day of year >= 0 that the species established itself in the current year.
			The output will be a single row of numbers for each year. Each column
			represents a species in order it was entered in the stabs.in file. The
			value will be the day that the species established, or - if it didn't
			establish this year.  This check is for RSOILWAT.

@param[in] pd Time period in simulation output (day/week/month/year)
@param[in] sw Comprehensive struct of type SW_ALL containing all information
  in the simulation.
*/
void get_estab_mem(OutPeriod pd, SW_ALL* sw)
{
	IntU i;
	size_t iOUTIndex;

	RealD *p = sw->GenOutput.p_OUT[eSW_Estab][pd];

    #if defined(RSOILWAT)
	get_outvalleader(&sw->Model, pd, sw->GenOutput.irow_OUT,
					 sw->GenOutput.nrow_OUT, sw->GenOutput.tOffset, p);
    #endif

	for (i = 0; i < sw->VegEstab.count; i++)
	{
		iOUTIndex = iOUT(i, pd, sw->GenOutput);
		p[iOUTIndex] = sw->VegEstab.parms[i]->estab_doy;
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

@param[in] pd Time period in simulation output (day/week/month/year)
@param[in] sw Comprehensive struct of type SW_ALL containing all information
  in the simulation.
*/
void get_estab_agg(OutPeriod pd, SW_ALL* sw)
{
	IntU i;
	size_t iOUTIndex;

	RealD
		*p = sw->GenOutput.p_OUT[eSW_Estab][pd],
		*psd = sw->GenOutput.p_OUTsd[eSW_Estab][pd];

	for (i = 0; i < sw->VegEstab.count; i++)
	{
		iOUTIndex = iOUT(i, pd, sw->GenOutput);
		do_running_agg(p, psd, iOUTIndex, sw->GenOutput.currIter,
					   sw->VegEstab.parms[i]->estab_doy);
	}

	if (sw->GenOutput.print_IterationSummary) {
		sw->GenOutput.sw_outstr_agg[0] = '\0';
		format_IterationSummary(p, psd, pd, sw->GenOutput.ncol_OUT[eSW_Estab], sw);
	}
}
#endif


//------ eSW_Temp
#ifdef SW_OUTTEXT

/**
@brief Gets temp text from SW_WEATHER_OUTPUTS when dealing with OUTTEXT.

@param[in] pd Time period in simulation output (day/week/month/year)
@param[in] sw Comprehensive struct of type SW_ALL containing all information
  in the simulation
*/
void get_temp_text(OutPeriod pd, SW_ALL* sw)
{
	SW_WEATHER_OUTPUTS *vo = sw->Weather.p_oagg[pd];

	sw->GenOutput.sw_outstr[0] = '\0';
	snprintf(sw->GenOutput.sw_outstr, sizeof sw->GenOutput.sw_outstr,"%c%.*f%c%.*f%c%.*f%c%.*f%c%.*f%c%.*f",
		_OUTSEP, OUT_DIGITS, vo->temp_max,
		_OUTSEP, OUT_DIGITS, vo->temp_min,
		_OUTSEP, OUT_DIGITS, vo->temp_avg,
        _OUTSEP, OUT_DIGITS, vo->surfaceMax,
		_OUTSEP, OUT_DIGITS, vo->surfaceMin,
        _OUTSEP, OUT_DIGITS, vo->surfaceAvg);
}
#endif

#if defined(RSOILWAT) || defined(SWNETCDF)

/**
@brief Gets temp text from SW_WEATHER_OUTPUTS when dealing with RSOILWAT.

@param pd Period.
@param[in] sw Comprehensive struct of type SW_ALL containing all information
  in the simulation
*/
void get_temp_mem(OutPeriod pd, SW_ALL* sw)
{
	SW_WEATHER_OUTPUTS *vo = sw->Weather.p_oagg[pd];
	size_t iOUTIndex;

	RealD *p = sw->GenOutput.p_OUT[eSW_Temp][pd];

    #if defined(RSOILWAT)
	get_outvalleader(&sw->Model, pd, sw->GenOutput.irow_OUT,
					 sw->GenOutput.nrow_OUT, sw->GenOutput.tOffset, p);
    #endif

	iOUTIndex = iOUT(0, pd, sw->GenOutput);
	p[iOUTIndex] = vo->temp_max;

	iOUTIndex = iOUT(1, pd, sw->GenOutput);
	p[iOUTIndex] = vo->temp_min;

	iOUTIndex = iOUT(2, pd, sw->GenOutput);
	p[iOUTIndex] = vo->temp_avg;

	iOUTIndex = iOUT(3, pd, sw->GenOutput);
    p[iOUTIndex] = vo->surfaceMax;

	iOUTIndex = iOUT(4, pd, sw->GenOutput);
	p[iOUTIndex] = vo->surfaceMin;

	iOUTIndex = iOUT(5, pd, sw->GenOutput);
    p[iOUTIndex] = vo->surfaceAvg;
}

#elif defined(STEPWAT)

/**
@brief Gets temp text from SW_WEATHER_OUTPUTS when dealing with STEPWAT.

@param[in] pd Time period in simulation output (day/week/month/year)
@param[in] sw Comprehensive struct of type SW_ALL containing all information
  in the simulation.
*/
void get_temp_agg(OutPeriod pd, SW_ALL* sw)
{
	SW_WEATHER_OUTPUTS *vo = sw->Weather.p_oagg[pd];
	size_t iOUTIndex;

	RealD
		*p = sw->GenOutput.p_OUT[eSW_Temp][pd],
		*psd = sw->GenOutput.p_OUTsd[eSW_Temp][pd];

	iOUTIndex = iOUT(0, pd, sw->GenOutput);
	do_running_agg(p, psd, iOUTIndex, sw->GenOutput.currIter,
				   vo->temp_max);

	iOUTIndex = iOUT(1, pd, sw->GenOutput);
	do_running_agg(p, psd, iOUTIndex, sw->GenOutput.currIter,
				   vo->temp_min);

	iOUTIndex = iOUT(2, pd, sw->GenOutput);
	do_running_agg(p, psd, iOUTIndex, sw->GenOutput.currIter,
				   vo->temp_avg);

	iOUTIndex = iOUT(3, pd, sw->GenOutput);
    do_running_agg(p, psd, iOUTIndex, sw->GenOutput.currIter,
				   vo->surfaceMax);

	iOUTIndex = iOUT(4, pd, sw->GenOutput);
	do_running_agg(p, psd, iOUTIndex, sw->GenOutput.currIter,
				   vo->surfaceMin);

	iOUTIndex = iOUT(5, pd, sw->GenOutput);
    do_running_agg(p, psd, iOUTIndex, sw->GenOutput.currIter,
				   vo->surfaceAvg);

	if (sw->GenOutput.print_IterationSummary) {
		sw->GenOutput.sw_outstr_agg[0] = '\0';
		format_IterationSummary(p, psd, pd, sw->GenOutput.ncol_OUT[eSW_Temp], sw);
	}
}

/**
@brief STEPWAT2 expects annual mean air temperature

@param[in] pd Time period in simulation output (day/week/month/year)
@param[in] sw Comprehensive struct of type SW_ALL containing all information
  in the simulation.
*/
void get_temp_SXW(OutPeriod pd, SW_ALL* sw)
{
	TimeInt tOffset;

	if (pd == eSW_Month || pd == eSW_Year) {
		SW_WEATHER_OUTPUTS *vo = sw->Weather.p_oagg[pd];
		tOffset = sw->GenOutput.tOffset;

		if (pd == eSW_Month) {
			sw->GenOutput.temp_monthly[sw->Model.month - tOffset] = vo->temp_avg;
		}
		else if (pd == eSW_Year) {
			sw->GenOutput.temp = vo->temp_avg;
		}
	}
}
#endif


//------ eSW_Precip
#ifdef SW_OUTTEXT

/**
@brief Gets precipitation text from SW_WEATHER_OUTPUTS when dealing with OUTTEXT.

@param[in] pd Time period in simulation output (day/week/month/year)
@param[in] sw Comprehensive struct of type SW_ALL containing all information
  in the simulation.
*/
void get_precip_text(OutPeriod pd, SW_ALL* sw)
{
	SW_WEATHER_OUTPUTS *vo = sw->Weather.p_oagg[pd];

	sw->GenOutput.sw_outstr[0] = '\0';
	snprintf(sw->GenOutput.sw_outstr, sizeof sw->GenOutput.sw_outstr,"%c%.*f%c%.*f%c%.*f%c%.*f%c%.*f",
		_OUTSEP, OUT_DIGITS, vo->ppt,
		_OUTSEP, OUT_DIGITS, vo->rain,
		_OUTSEP, OUT_DIGITS, vo->snow,
		_OUTSEP, OUT_DIGITS, vo->snowmelt,
		_OUTSEP, OUT_DIGITS, vo->snowloss);
}
#endif

#if defined(RSOILWAT) || defined(SWNETCDF)

/**
@brief Gets precipitation text from SW_WEATHER_OUTPUTS when dealing with RSOILWAT.

@param[in] pd Time period in simulation output (day/week/month/year)
@param[in] sw Comprehensive struct of type SW_ALL containing all information
  in the simulation.
*/
void get_precip_mem(OutPeriod pd, SW_ALL* sw)
{
	SW_WEATHER_OUTPUTS *vo = sw->Weather.p_oagg[pd];
	size_t iOUTIndex;

	RealD *p = sw->GenOutput.p_OUT[eSW_Precip][pd];

    #if defined(RSOILWAT)
	get_outvalleader(&sw->Model, pd, sw->GenOutput.irow_OUT,
					 sw->GenOutput.nrow_OUT, sw->GenOutput.tOffset, p);
    #endif

	iOUTIndex = iOUT(0, pd, sw->GenOutput);
	p[iOUTIndex] = vo->ppt;

	iOUTIndex = iOUT(1, pd, sw->GenOutput);
	p[iOUTIndex] = vo->rain;

	iOUTIndex = iOUT(2, pd, sw->GenOutput);
	p[iOUTIndex] = vo->snow;

	iOUTIndex = iOUT(3, pd, sw->GenOutput);
	p[iOUTIndex] = vo->snowmelt;

	iOUTIndex = iOUT(4, pd, sw->GenOutput);
	p[iOUTIndex] = vo->snowloss;
}

#elif defined(STEPWAT)

/**
@brief Gets precipitation text from SW_WEATHER_OUTPUTS when dealing with STEPWAT.

@param[in] pd Time period in simulation output (day/week/month/year)
@param[in] sw Comprehensive struct of type SW_ALL containing all information
  in the simulation.
*/
void get_precip_agg(OutPeriod pd, SW_ALL* sw)
{
	SW_WEATHER_OUTPUTS *vo = sw->Weather.p_oagg[pd];
	size_t iOUTIndex;

	RealD
		*p = sw->GenOutput.p_OUT[eSW_Precip][pd],
		*psd = sw->GenOutput.p_OUTsd[eSW_Precip][pd];

	iOUTIndex = iOUT(0, pd, sw->GenOutput);
	do_running_agg(p, psd, iOUTIndex, sw->GenOutput.currIter,
				   vo->ppt);

	iOUTIndex = iOUT(1, pd, sw->GenOutput);
	do_running_agg(p, psd, iOUTIndex, sw->GenOutput.currIter,
				   vo->rain);

	iOUTIndex = iOUT(2, pd, sw->GenOutput);
	do_running_agg(p, psd, iOUTIndex, sw->GenOutput.currIter,
				   vo->snow);

	iOUTIndex = iOUT(3, pd, sw->GenOutput);
	do_running_agg(p, psd, iOUTIndex, sw->GenOutput.currIter,
				   vo->snowmelt);

	iOUTIndex = iOUT(4, pd, sw->GenOutput);
	do_running_agg(p, psd, iOUTIndex, sw->GenOutput.currIter,
				   vo->snowloss);

	if (sw->GenOutput.print_IterationSummary) {
		sw->GenOutput.sw_outstr_agg[0] = '\0';
		format_IterationSummary(p, psd, pd, sw->GenOutput.ncol_OUT[eSW_Precip], sw);
	}
}

/**
@brief STEPWAT2 expects monthly and annual sum of precipitation

@param[in] pd Time period in simulation output (day/week/month/year)
@param[in] sw Comprehensive struct of type SW_ALL containing all information
  in the simulation.
*/
void get_precip_SXW(OutPeriod pd, SW_ALL* sw)
{
	TimeInt tOffset;

	if (pd == eSW_Month || pd == eSW_Year) {
		SW_WEATHER_OUTPUTS *vo = sw->Weather.p_oagg[pd];
		tOffset = sw->GenOutput.tOffset;

		if (pd == eSW_Month) {
			sw->GenOutput.ppt_monthly[sw->Model.month - tOffset] = vo->ppt;
		}
		else if (pd == eSW_Year) {
			sw->GenOutput.ppt = vo->ppt;
		}
	}
}
#endif

//------ eSW_VWCBulk
#ifdef SW_OUTTEXT

/**
@brief Gets vwcBulk text from SW_SOILWAT_OUTPUTS when dealing with OUTTEXT.

@param[in] pd Time period in simulation output (day/week/month/year)
@param[in] sw Comprehensive struct of type SW_ALL containing all information
  in the simulation.
*/
void get_vwcBulk_text(OutPeriod pd, SW_ALL* sw)
{
	LyrIndex i;
	SW_SOILWAT_OUTPUTS *vo = sw->SoilWat.p_oagg[pd];

	char str[OUTSTRLEN];
	sw->GenOutput.sw_outstr[0] = '\0';

	ForEachSoilLayer(i, sw->Site.n_layers) {
		/* vwcBulk at this point is identical to swcBulk */
		snprintf(str, OUTSTRLEN, "%c%.*f",
			_OUTSEP, OUT_DIGITS, vo->vwcBulk[i] / sw->Site.width[i]);
		strcat(sw->GenOutput.sw_outstr, str);
	}
}
#endif

#if defined(RSOILWAT) || defined(SWNETCDF)

/**
@brief Gets vwcBulk text from SW_SOILWAT_OUTPUTS when dealing with RSOILWAT.

@param[in] pd Time period in simulation output (day/week/month/year)
@param[in] sw Comprehensive struct of type SW_ALL containing all information
  in the simulation.
*/
void get_vwcBulk_mem(OutPeriod pd, SW_ALL* sw)
{
	LyrIndex i;
	SW_SOILWAT_OUTPUTS *vo = sw->SoilWat.p_oagg[pd];
	size_t iOUTIndex;

	RealD *p = sw->GenOutput.p_OUT[eSW_VWCBulk][pd];

    #if defined(RSOILWAT)
	get_outvalleader(&sw->Model, pd, sw->GenOutput.irow_OUT,
					 sw->GenOutput.nrow_OUT, sw->GenOutput.tOffset, p);
    #endif

	ForEachSoilLayer(i, sw->Site.n_layers) {
		/* vwcBulk at this point is identical to swcBulk */
		iOUTIndex = iOUT(i, pd, sw->GenOutput);
		p[iOUTIndex] = vo->vwcBulk[i] / sw->Site.width[i];
	}
}

#elif defined(STEPWAT)

/**
@brief Gets vwcBulk text from SW_SOILWAT_OUTPUTS when dealing with STEPWAT.

@param[in] pd Time period in simulation output (day/week/month/year)
@param[in] sw Comprehensive struct of type SW_ALL containing all information
  in the simulation.
*/
void get_vwcBulk_agg(OutPeriod pd, SW_ALL* sw)
{
	LyrIndex i;
	SW_SOILWAT_OUTPUTS *vo = sw->SoilWat.p_oagg[pd];
	size_t iOUTIndex;

	RealD
		*p = sw->GenOutput.p_OUT[eSW_VWCBulk][pd],
		*psd = sw->GenOutput.p_OUTsd[eSW_VWCBulk][pd];

	ForEachSoilLayer(i, sw->Site.n_layers) {
		/* vwcBulk at this point is identical to swcBulk */
		iOUTIndex = iOUT(i, pd, sw->GenOutput);
		do_running_agg(p, psd, iOUTIndex, sw->GenOutput.currIter,
					   vo->vwcBulk[i] / sw->Site.width[i]);
	}

	if (sw->GenOutput.print_IterationSummary) {
		sw->GenOutput.sw_outstr_agg[0] = '\0';
		format_IterationSummary(p, psd, pd, sw->GenOutput.ncol_OUT[eSW_VWCBulk], sw);
	}
}
#endif


//------ eSW_VWCMatric
#ifdef SW_OUTTEXT

/**
@brief Gets vwcMatric text from SW_SOILWAT_OUTPUTS when dealing with OUTTEXT.

@param[in] pd Time period in simulation output (day/week/month/year)
@param[in] sw Comprehensive struct of type SW_ALL containing all information
  in the simulation.
*/
void get_vwcMatric_text(OutPeriod pd, SW_ALL* sw)
{
	LyrIndex i;
	RealD convert;
	SW_SOILWAT_OUTPUTS *vo = sw->SoilWat.p_oagg[pd];

	char str[OUTSTRLEN];
	sw->GenOutput.sw_outstr[0] = '\0';

	ForEachSoilLayer(i, sw->Site.n_layers) {
		/* vwcMatric at this point is identical to swcBulk */
		convert = 1. / (1. - sw->Site.fractionVolBulk_gravel[i]) /
														sw->Site.width[i];

		snprintf(str, OUTSTRLEN, "%c%.*f",
			_OUTSEP, OUT_DIGITS, vo->vwcMatric[i] * convert);
		strcat(sw->GenOutput.sw_outstr, str);
	}
}
#endif

#if defined(RSOILWAT) || defined(SWNETCDF)

/**
@brief Gets vwcMatric text from SW_SOILWAT_OUTPUTS when dealing with RSOILWAT.

@param[in] pd Time period in simulation output (day/week/month/year)
@param[in] sw Comprehensive struct of type SW_ALL containing all information
  in the simulation.
*/
void get_vwcMatric_mem(OutPeriod pd, SW_ALL* sw)
{
	LyrIndex i;
	RealD convert;
	SW_SOILWAT_OUTPUTS *vo = sw->SoilWat.p_oagg[pd];
	size_t iOUTIndex;

	RealD *p = sw->GenOutput.p_OUT[eSW_VWCMatric][pd];

    #if defined(RSOILWAT)
	get_outvalleader(&sw->Model, pd, sw->GenOutput.irow_OUT,
					 sw->GenOutput.nrow_OUT, sw->GenOutput.tOffset, p);
    #endif

	ForEachSoilLayer(i, sw->Site.n_layers) {
		/* vwcMatric at this point is identical to swcBulk */
		convert = 1. / (1. - sw->Site.fractionVolBulk_gravel[i]) / sw->Site.width[i];

		iOUTIndex = iOUT(i, pd, sw->GenOutput);
		p[iOUTIndex] = vo->vwcMatric[i] * convert;
	}
}

#elif defined(STEPWAT)

/**
@brief Gets vwcMatric text from SW_SOILWAT_OUTPUTS when dealing with STEPWAT.

@param[in] pd Time period in simulation output (day/week/month/year)
@param[in] sw Comprehensive struct of type SW_ALL containing all information
  in the simulation.
*/
void get_vwcMatric_agg(OutPeriod pd, SW_ALL* sw)
{
	LyrIndex i;
	RealD convert;
	SW_SOILWAT_OUTPUTS *vo = sw->SoilWat.p_oagg[pd];
	size_t iOUTIndex;

	RealD
		*p = sw->GenOutput.p_OUT[eSW_VWCMatric][pd],
		*psd = sw->GenOutput.p_OUTsd[eSW_VWCMatric][pd];

	ForEachSoilLayer(i, sw->Site.n_layers) {
		/* vwcMatric at this point is identical to swcBulk */
		convert = 1. / (1. - sw->Site.fractionVolBulk_gravel[i]) / sw->Site.width[i];

		iOUTIndex = iOUT(i, pd, sw->GenOutput);
		do_running_agg(p, psd, iOUTIndex, sw->GenOutput.currIter,
					   vo->vwcMatric[i] * convert);
	}

	if (sw->GenOutput.print_IterationSummary) {
		sw->GenOutput.sw_outstr_agg[0] = '\0';
		format_IterationSummary(p, psd, pd, sw->GenOutput.ncol_OUT[eSW_VWCMatric], sw);
	}
}
#endif


//------ eSW_SWA
#ifdef SW_OUTTEXT

/**
@brief Gets SWA text from SW_SOILWAT_OUTPUTS when dealing with OUTTEXT.

@param[in] pd Time period in simulation output (day/week/month/year)
@param[in] sw Comprehensive struct of type SW_ALL containing all information
  in the simulation.
*/
void get_swa_text(OutPeriod pd, SW_ALL* sw)
{
	/* added 21-Oct-03, cwb */
	LyrIndex i;
	int k;
	SW_SOILWAT_OUTPUTS *vo = sw->SoilWat.p_oagg[pd];

	char str[OUTSTRLEN];
	sw->GenOutput.sw_outstr[0] = '\0';

	ForEachVegType(k)
	{
		ForEachSoilLayer(i, sw->Site.n_layers)
		{
			snprintf(str, OUTSTRLEN, "%c%.*f", _OUTSEP, OUT_DIGITS, vo->SWA_VegType[k][i]);
			strcat(sw->GenOutput.sw_outstr, str);
		}
	}
}
#endif

#if defined(RSOILWAT) || defined(SWNETCDF)

/**
@brief Gets SWA text from SW_SOILWAT_OUTPUTS when dealing with RSOILWAT.

@param[in] pd Time period in simulation output (day/week/month/year)
@param[in] sw Comprehensive struct of type SW_ALL containing all information
  in the simulation.
*/
void get_swa_mem(OutPeriod pd, SW_ALL* sw)
{
	LyrIndex i;
	int k;
	SW_SOILWAT_OUTPUTS *vo = sw->SoilWat.p_oagg[pd];
	size_t iOUTIndex;

	RealD *p = sw->GenOutput.p_OUT[eSW_SWA][pd];

    #if defined(RSOILWAT)
	get_outvalleader(&sw->Model, pd, sw->GenOutput.irow_OUT,
					 sw->GenOutput.nrow_OUT, sw->GenOutput.tOffset, p);
    #endif

	ForEachVegType(k)
	{
		ForEachSoilLayer(i, sw->Site.n_layers)
		{
			iOUTIndex = iOUT2(i, k, pd, sw->GenOutput, sw->Site.n_layers);
			p[iOUTIndex] = vo->SWA_VegType[k][i];
		}
	}
}

#elif defined(STEPWAT)

/**
@brief Gets SWA text from SW_SOILWAT_OUTPUTS when dealing with STEPWAT.

@param[in] pd Time period in simulation output (day/week/month/year)
@param[in] sw Comprehensive struct of type SW_ALL containing all information
  in the simulation.
*/
void get_swa_agg(OutPeriod pd, SW_ALL* sw)
{
	LyrIndex i;
	int k;
	SW_SOILWAT_OUTPUTS *vo = sw->SoilWat.p_oagg[pd];
	size_t iOUTIndex;

	RealD
		*p = sw->GenOutput.p_OUT[eSW_SWA][pd],
		*psd = sw->GenOutput.p_OUTsd[eSW_SWA][pd];

	ForEachVegType(k)
	{
		ForEachSoilLayer(i, sw->Site.n_layers)
		{
			iOUTIndex = iOUT2(i, k, pd, sw->GenOutput, sw->Site.n_layers);
			do_running_agg(p, psd, iOUTIndex, sw->GenOutput.currIter,
						   vo->SWA_VegType[k][i]);
		}
	}

	if (sw->GenOutput.print_IterationSummary) {
		sw->GenOutput.sw_outstr_agg[0] = '\0';
		format_IterationSummary2(p, psd, pd, NVEGTYPES, 0, sw);
	}
}
#endif

//------ eSW_SWCBulk
#ifdef SW_OUTTEXT

/**
@brief Gets swcBulk text from SW_SOILWAT_OUTPUTS when dealing with OUTTEXT.

@param[in] pd Time period in simulation output (day/week/month/year)
@param[in] sw Comprehensive struct of type SW_ALL containing all information
  in the simulation.
*/
void get_swcBulk_text(OutPeriod pd, SW_ALL* sw)
{
	/* added 21-Oct-03, cwb */
	LyrIndex i;
	SW_SOILWAT_OUTPUTS *vo = sw->SoilWat.p_oagg[pd];

	char str[OUTSTRLEN];
	sw->GenOutput.sw_outstr[0] = '\0';

	ForEachSoilLayer(i, sw->Site.n_layers)
	{
		snprintf(str, OUTSTRLEN, "%c%.*f", _OUTSEP, OUT_DIGITS, vo->swcBulk[i]);
		strcat(sw->GenOutput.sw_outstr, str);
	}
}
#endif

#if defined(RSOILWAT) || defined(SWNETCDF)

/**
@brief Gets swcBulk text from SW_SOILWAT_OUTPUTS when dealing with RSOILWAT.

@param[in] pd Time period in simulation output (day/week/month/year)
@param[in] sw Comprehensive struct of type SW_ALL containing all information
  in the simulation.
*/
void get_swcBulk_mem(OutPeriod pd, SW_ALL* sw)
{
	LyrIndex i;
	SW_SOILWAT_OUTPUTS *vo = sw->SoilWat.p_oagg[pd];
	size_t iOUTIndex;

	RealD *p = sw->GenOutput.p_OUT[eSW_SWCBulk][pd];

    #if defined(RSOILWAT)
	get_outvalleader(&sw->Model, pd, sw->GenOutput.irow_OUT,
					 sw->GenOutput.nrow_OUT, sw->GenOutput.tOffset, p);
    #endif

	ForEachSoilLayer(i, sw->Site.n_layers)
	{
		iOUTIndex = iOUT(i, pd, sw->GenOutput);
		p[iOUTIndex] = vo->swcBulk[i];
	}
}

#elif defined(STEPWAT)

/**
@brief Gets swcBulk text from SW_SOILWAT_OUTPUTS when dealing with STEPWAT.

@param[in] pd Time period in simulation output (day/week/month/year)
@param[in] sw Comprehensive struct of type SW_ALL containing all information
  in the simulation.
*/
void get_swcBulk_agg(OutPeriod pd, SW_ALL* sw)
{
	LyrIndex i;
	SW_SOILWAT_OUTPUTS *vo = sw->SoilWat.p_oagg[pd];
	size_t iOUTIndex;

	RealD
		*p = sw->GenOutput.p_OUT[eSW_SWCBulk][pd],
		*psd = sw->GenOutput.p_OUTsd[eSW_SWCBulk][pd];

	ForEachSoilLayer(i, sw->Site.n_layers)
	{
		iOUTIndex = iOUT(i, pd, sw->GenOutput);
		do_running_agg(p, psd, iOUTIndex, sw->GenOutput.currIter,
					   vo->swcBulk[i]);
	}

	if (sw->GenOutput.print_IterationSummary) {
		sw->GenOutput.sw_outstr_agg[0] = '\0';
		format_IterationSummary(p, psd, pd, sw->GenOutput.ncol_OUT[eSW_SWCBulk], sw);
	}
}

/**
@brief STEPWAT2 expects monthly mean SWCbulk by soil layer.

@param[in] pd Time period in simulation output (day/week/month/year)
@param[in] sw Comprehensive struct of type SW_ALL containing all information
  in the simulation.
*/
void get_swcBulk_SXW(OutPeriod pd, SW_ALL* sw)
{
	TimeInt month;

	if (pd == eSW_Month) {
		LyrIndex i;
		SW_SOILWAT_OUTPUTS *vo = sw->SoilWat.p_oagg[pd];
		month = sw->Model.month - sw->GenOutput.tOffset;

		ForEachSoilLayer(i, sw->Site.n_layers)
		{
			sw->GenOutput.swc[i][month] = vo->swcBulk[i];
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

@param[in] pd Time period in simulation output (day/week/month/year)
@param[in] sw Comprehensive struct of type SW_ALL containing all information
  in the simulation.
*/
void get_swpMatric_text(OutPeriod pd, SW_ALL* sw)
{
	RealD val;
	LyrIndex i;
	SW_SOILWAT_OUTPUTS *vo = sw->SoilWat.p_oagg[pd];

	/* Local LOG_INFO only because `SW_SWRC_SWCtoSWP()` requires it */
	LOG_INFO local_log;
	local_log.logfp = NULL;

	char str[OUTSTRLEN];
	sw->GenOutput.sw_outstr[0] = '\0';

	ForEachSoilLayer(i, sw->Site.n_layers)
	{
		/* swpMatric at this point is identical to swcBulk */
		val = SW_SWRC_SWCtoSWP(vo->swpMatric[i], &sw->Site, i, &local_log);


		snprintf(str, OUTSTRLEN, "%c%.*f", _OUTSEP, OUT_DIGITS, val);
		strcat(sw->GenOutput.sw_outstr, str);
	}
}
#endif

#if defined(RSOILWAT) || defined(SWNETCDF)

/**
@brief Gets swpMatric when dealing with RSOILWAT

@param[in] pd Time period in simulation output (day/week/month/year)
@param[in] sw Comprehensive struct of type SW_ALL containing all information
  in the simulation.
*/
void get_swpMatric_mem(OutPeriod pd, SW_ALL* sw)
{
	LyrIndex i;
	SW_SOILWAT_OUTPUTS *vo = sw->SoilWat.p_oagg[pd];
	LOG_INFO local_log;
	size_t iOUTIndex;

	RealD *p = sw->GenOutput.p_OUT[eSW_SWPMatric][pd];

    #if defined(RSOILWAT)
	get_outvalleader(&sw->Model, pd, sw->GenOutput.irow_OUT,
					 sw->GenOutput.nrow_OUT, sw->GenOutput.tOffset, p);
    #endif

	ForEachSoilLayer(i, sw->Site.n_layers)
	{
		/* swpMatric at this point is identical to swcBulk */
		iOUTIndex = iOUT(i, pd, sw->GenOutput);
		p[iOUTIndex] = SW_SWRC_SWCtoSWP(vo->swpMatric[i], &sw->Site,
										i, &local_log);
	}
}

#elif defined(STEPWAT)

/**
@brief Gets swpMatric when dealing with STEPWAT.

@param[in] pd Time period in simulation output (day/week/month/year)
@param[in] sw Comprehensive struct of type SW_ALL containing all information
  in the simulation.
*/
void get_swpMatric_agg(OutPeriod pd, SW_ALL* sw)
{
	RealD val;
	LyrIndex i;
	SW_SOILWAT_OUTPUTS *vo = sw->SoilWat.p_oagg[pd];
	LOG_INFO local_log;
	size_t iOUTIndex;

	RealD
		*p = sw->GenOutput.p_OUT[eSW_SWPMatric][pd],
		*psd = sw->GenOutput.p_OUTsd[eSW_SWPMatric][pd];

	ForEachSoilLayer(i, sw->Site.n_layers)
	{
		/* swpMatric at this point is identical to swcBulk */
		val = SW_SWRC_SWCtoSWP(vo->swpMatric[i], &sw->Site, i, &local_log);

		iOUTIndex = iOUT(i, pd, sw->GenOutput);
		do_running_agg(p, psd, iOUTIndex, sw->GenOutput.currIter, val);
	}

	if (sw->GenOutput.print_IterationSummary) {
		sw->GenOutput.sw_outstr_agg[0] = '\0';
		format_IterationSummary(p, psd, pd, sw->GenOutput.ncol_OUT[eSW_SWPMatric], sw);
	}
}
#endif


//------ eSW_SWABulk
#ifdef SW_OUTTEXT

/**
@brief gets swaBulk when dealing with OUTTEXT.

@param[in] pd Time period in simulation output (day/week/month/year)
@param[in] sw Comprehensive struct of type SW_ALL containing all information
  in the simulation.
*/
void get_swaBulk_text(OutPeriod pd, SW_ALL* sw)
{
	LyrIndex i;
	SW_SOILWAT_OUTPUTS *vo = sw->SoilWat.p_oagg[pd];

	char str[OUTSTRLEN];
	sw->GenOutput.sw_outstr[0] = '\0';

	ForEachSoilLayer(i, sw->Site.n_layers)
	{
		snprintf(str, OUTSTRLEN, "%c%.*f", _OUTSEP, OUT_DIGITS, vo->swaBulk[i]);
		strcat(sw->GenOutput.sw_outstr, str);
	}
}
#endif

#if defined(RSOILWAT) || defined(SWNETCDF)

/**
@brief Gets swaBulk when dealing with RSOILWAT.

@param[in] pd Time period in simulation output (day/week/month/year)
@param[in] sw Comprehensive struct of type SW_ALL containing all information
  in the simulation.
*/
void get_swaBulk_mem(OutPeriod pd, SW_ALL* sw)
{
	LyrIndex i;
	SW_SOILWAT_OUTPUTS *vo = sw->SoilWat.p_oagg[pd];
	size_t iOUTIndex;

	RealD *p = sw->GenOutput.p_OUT[eSW_SWABulk][pd];

    #if defined(RSOILWAT)
	get_outvalleader(&sw->Model, pd, sw->GenOutput.irow_OUT,
					 sw->GenOutput.nrow_OUT, sw->GenOutput.tOffset, p);
    #endif

	ForEachSoilLayer(i, sw->Site.n_layers)
	{
		iOUTIndex = iOUT(i, pd, sw->GenOutput);
		p[iOUTIndex] = vo->swaBulk[i];
	}
}

#elif defined(STEPWAT)

/**
@brief Gets swaBulk when dealing with STEPWAT.

@param[in] pd Time period in simulation output (day/week/month/year)
@param[in] sw Comprehensive struct of type SW_ALL containing all information
  in the simulation.
*/
void get_swaBulk_agg(OutPeriod pd, SW_ALL* sw)
{
	LyrIndex i;
	SW_SOILWAT_OUTPUTS *vo = sw->SoilWat.p_oagg[pd];
	size_t iOUTIndex;

	RealD
		*p = sw->GenOutput.p_OUT[eSW_SWABulk][pd],
		*psd = sw->GenOutput.p_OUTsd[eSW_SWABulk][pd];

	ForEachSoilLayer(i, sw->Site.n_layers)
	{
		iOUTIndex = iOUT(i, pd, sw->GenOutput);
		do_running_agg(p, psd, iOUTIndex, sw->GenOutput.currIter,
					   vo->swaBulk[i]);
	}

	if (sw->GenOutput.print_IterationSummary) {
		sw->GenOutput.sw_outstr_agg[0] = '\0';
		format_IterationSummary(p, psd, pd, sw->GenOutput.ncol_OUT[eSW_SWABulk], sw);
	}
}
#endif

//------ eSW_SWAMatric
#ifdef SW_OUTTEXT

/**
@brief Gets swaMatric when dealing with OUTTEXT.

@param[in] pd Time period in simulation output (day/week/month/year)
@param[in] sw Comprehensive struct of type SW_ALL containing all information
  in the simulation.
*/
void get_swaMatric_text(OutPeriod pd, SW_ALL* sw)
{
	LyrIndex i;
	RealD convert;
	SW_SOILWAT_OUTPUTS *vo = sw->SoilWat.p_oagg[pd];

	char str[OUTSTRLEN];
	sw->GenOutput.sw_outstr[0] = '\0';

	ForEachSoilLayer(i, sw->Site.n_layers)
	{
		/* swaMatric at this point is identical to swaBulk */
		convert = 1. / (1. - sw->Site.fractionVolBulk_gravel[i]);

		snprintf(str, OUTSTRLEN, "%c%.*f", _OUTSEP, OUT_DIGITS, vo->swaMatric[i] * convert);
		strcat(sw->GenOutput.sw_outstr, str);
	}
}
#endif

#if defined(RSOILWAT) || defined(SWNETCDF)

/**
@brief Gets swaMatric when dealing with RSOILWAT.

@param[in] pd Time period in simulation output (day/week/month/year)
@param[in] sw Comprehensive struct of type SW_ALL containing all information
  in the simulation.
*/
void get_swaMatric_mem(OutPeriod pd, SW_ALL* sw)
{
	LyrIndex i;
	RealD convert;
	SW_SOILWAT_OUTPUTS *vo = sw->SoilWat.p_oagg[pd];
	size_t iOUTIndex;

	RealD *p = sw->GenOutput.p_OUT[eSW_SWAMatric][pd];

    #if defined(RSOILWAT)
	get_outvalleader(&sw->Model, pd, sw->GenOutput.irow_OUT,
					 sw->GenOutput.nrow_OUT, sw->GenOutput.tOffset, p);
    #endif

	ForEachSoilLayer(i, sw->Site.n_layers)
	{
		/* swaMatric at this point is identical to swaBulk */
		convert = 1. / (1. - sw->Site.fractionVolBulk_gravel[i]);

		iOUTIndex = iOUT(i, pd, sw->GenOutput);
		p[iOUTIndex] = vo->swaMatric[i] * convert;
	}
}

#elif defined(STEPWAT)

/**
@brief Gets swaMatric when dealing with STEPWAT.

@param[in] pd Time period in simulation output (day/week/month/year)
@param[in] sw Comprehensive struct of type SW_ALL containing all information
  in the simulation.
*/
void get_swaMatric_agg(OutPeriod pd, SW_ALL* sw)
{
	LyrIndex i;
	RealD convert;
	SW_SOILWAT_OUTPUTS *vo = sw->SoilWat.p_oagg[pd];
	size_t iOUTIndex;

	RealD
		*p = sw->GenOutput.p_OUT[eSW_SWAMatric][pd],
		*psd = sw->GenOutput.p_OUTsd[eSW_SWAMatric][pd];

	ForEachSoilLayer(i, sw->Site.n_layers)
	{
		/* swaMatric at this point is identical to swaBulk */
		convert = 1. / (1. - sw->Site.fractionVolBulk_gravel[i]);

		iOUTIndex = iOUT(i, pd, sw->GenOutput);
		do_running_agg(p, psd, iOUTIndex, sw->GenOutput.currIter,
					   vo->swaMatric[i] * convert);
	}

	if (sw->GenOutput.print_IterationSummary) {
		sw->GenOutput.sw_outstr_agg[0] = '\0';
		format_IterationSummary(p, psd, pd, sw->GenOutput.ncol_OUT[eSW_SWAMatric], sw);
	}
}
#endif


//------ eSW_SurfaceWater
#ifdef SW_OUTTEXT

/**
@brief Gets surfaceWater when dealing with OUTTEXT.

@param[in] pd Time period in simulation output (day/week/month/year)
@param[in] sw Comprehensive struct of type SW_ALL containing all information
  in the simulation.
*/
void get_surfaceWater_text(OutPeriod pd, SW_ALL* sw)
{
	SW_SOILWAT_OUTPUTS *vo = sw->SoilWat.p_oagg[pd];

	sw->GenOutput.sw_outstr[0] = '\0';
	snprintf(sw->GenOutput.sw_outstr, sizeof sw->GenOutput.sw_outstr,"%c%.*f", _OUTSEP, OUT_DIGITS, vo->surfaceWater);
}
#endif

#if defined(RSOILWAT) || defined(SWNETCDF)

/**
@brief Gets surfaceWater when dealing with RSOILWAT.

@param[in] pd Time period in simulation output (day/week/month/year)
@param[in] sw Comprehensive struct of type SW_ALL containing all information
  in the simulation.
*/
void get_surfaceWater_mem(OutPeriod pd, SW_ALL* sw)
{
	SW_SOILWAT_OUTPUTS *vo = sw->SoilWat.p_oagg[pd];
	size_t iOUTIndex;

	RealD *p = sw->GenOutput.p_OUT[eSW_SurfaceWater][pd];

    #if defined(RSOILWAT)
	get_outvalleader(&sw->Model, pd, sw->GenOutput.irow_OUT,
					 sw->GenOutput.nrow_OUT, sw->GenOutput.tOffset, p);
    #endif

	iOUTIndex = iOUT(0, pd, sw->GenOutput);
	p[iOUTIndex] = vo->surfaceWater;
}

#elif defined(STEPWAT)

/**
@brief Gets surfaceWater when dealing with STEPWAT.

@param[in] pd Time period in simulation output (day/week/month/year)
@param[in] sw Comprehensive struct of type SW_ALL containing all information
  in the simulation.
*/
void get_surfaceWater_agg(OutPeriod pd, SW_ALL* sw)
{
	SW_SOILWAT_OUTPUTS *vo = sw->SoilWat.p_oagg[pd];
	size_t iOUTIndex;

	RealD
		*p = sw->GenOutput.p_OUT[eSW_SurfaceWater][pd],
		*psd = sw->GenOutput.p_OUTsd[eSW_SurfaceWater][pd];

	iOUTIndex = iOUT(0, pd, sw->GenOutput);
	do_running_agg(p, psd, iOUTIndex, sw->GenOutput.currIter,
				   vo->surfaceWater);

	if (sw->GenOutput.print_IterationSummary) {
		sw->GenOutput.sw_outstr_agg[0] = '\0';
		format_IterationSummary(p, psd, pd,
									sw->GenOutput.ncol_OUT[eSW_SurfaceWater], sw);
	}
}
#endif


//------ eSW_Runoff
#ifdef SW_OUTTEXT

/**
@brief Gets surfaceRunon, surfaceRunoff, and snowRunoff when dealing with OUTTEXT.

@param[in] pd Time period in simulation output (day/week/month/year)
@param[in] sw Comprehensive struct of type SW_ALL containing all information
  in the simulation.
*/
void get_runoffrunon_text(OutPeriod pd, SW_ALL* sw)
{
	RealD net;
	SW_WEATHER_OUTPUTS *vo = sw->Weather.p_oagg[pd];

	net = vo->surfaceRunoff + vo->snowRunoff - vo->surfaceRunon;

	sw->GenOutput.sw_outstr[0] = '\0';
	snprintf(sw->GenOutput.sw_outstr, sizeof sw->GenOutput.sw_outstr,"%c%.*f%c%.*f%c%.*f%c%.*f",
		_OUTSEP, OUT_DIGITS, net,
		_OUTSEP, OUT_DIGITS, vo->surfaceRunoff,
		_OUTSEP, OUT_DIGITS, vo->snowRunoff,
		_OUTSEP, OUT_DIGITS, vo->surfaceRunon);
}
#endif

#if defined(RSOILWAT) || defined(SWNETCDF)

/**
@brief Gets surfaceRunon, surfaceRunoff, and snowRunoff when dealing with RSOILWAT.

@param[in] pd Time period in simulation output (day/week/month/year)
@param[in] sw Comprehensive struct of type SW_ALL containing all information
  in the simulation.
*/
void get_runoffrunon_mem(OutPeriod pd, SW_ALL* sw)
{
	RealD net;
	SW_WEATHER_OUTPUTS *vo = sw->Weather.p_oagg[pd];
	size_t iOUTIndex;

	RealD *p = sw->GenOutput.p_OUT[eSW_Runoff][pd];

    #if defined(RSOILWAT)
	get_outvalleader(&sw->Model, pd, sw->GenOutput.irow_OUT,
					 sw->GenOutput.nrow_OUT, sw->GenOutput.tOffset, p);
    #endif

	net = vo->surfaceRunoff + vo->snowRunoff - vo->surfaceRunon;

	iOUTIndex = iOUT(0, pd, sw->GenOutput);
	p[iOUTIndex] = net;

	iOUTIndex = iOUT(1, pd, sw->GenOutput);
	p[iOUTIndex] = vo->surfaceRunoff;

	iOUTIndex = iOUT(2, pd, sw->GenOutput);
	p[iOUTIndex] = vo->snowRunoff;

	iOUTIndex = iOUT(3, pd, sw->GenOutput);
	p[iOUTIndex] = vo->surfaceRunon;
}

#elif defined(STEPWAT)

/**
@brief Gets surfaceRunon, surfaceRunoff, and snowRunoff when dealing with STEPWAT.

@param[in] pd Time period in simulation output (day/week/month/year)
@param[in] sw Comprehensive struct of type SW_ALL containing all information
  in the simulation.
*/
void get_runoffrunon_agg(OutPeriod pd, SW_ALL* sw)
{
	RealD net;
	SW_WEATHER_OUTPUTS *vo = sw->Weather.p_oagg[pd];
	size_t iOUTIndex;

	RealD
		*p = sw->GenOutput.p_OUT[eSW_Runoff][pd],
		*psd = sw->GenOutput.p_OUTsd[eSW_Runoff][pd];

	net = vo->surfaceRunoff + vo->snowRunoff - vo->surfaceRunon;

	iOUTIndex = iOUT(0, pd, sw->GenOutput);
	do_running_agg(p, psd, iOUTIndex, sw->GenOutput.currIter,
				   net);

	iOUTIndex = iOUT(1, pd, sw->GenOutput);
	do_running_agg(p, psd, iOUTIndex, sw->GenOutput.currIter,
				   vo->surfaceRunoff);

	iOUTIndex = iOUT(2, pd, sw->GenOutput);
	do_running_agg(p, psd, iOUTIndex, sw->GenOutput.currIter,
				   vo->snowRunoff);

	iOUTIndex = iOUT(3, pd, sw->GenOutput);
	do_running_agg(p, psd, iOUTIndex, sw->GenOutput.currIter,
				   vo->surfaceRunon);

	if (sw->GenOutput.print_IterationSummary) {
		sw->GenOutput.sw_outstr_agg[0] = '\0';
		format_IterationSummary(p, psd, pd, sw->GenOutput.ncol_OUT[eSW_Runoff], sw);
	}
}
#endif

//------ eSW_Transp
#ifdef SW_OUTTEXT

/**
@brief Gets transp_total when dealing with OUTTEXT.

@param[in] pd Time period in simulation output (day/week/month/year)
@param[in] sw Comprehensive struct of type SW_ALL containing all information
  in the simulation.
*/
void get_transp_text(OutPeriod pd, SW_ALL* sw)
{
	LyrIndex i, n_layers = sw->Site.n_layers;
	int k;
	SW_SOILWAT_OUTPUTS *vo = sw->SoilWat.p_oagg[pd];

	char str[OUTSTRLEN];
	sw->GenOutput.sw_outstr[0] = '\0';

	/* total transpiration */
	ForEachSoilLayer(i, n_layers)
	{
		snprintf(str, OUTSTRLEN, "%c%.*f", _OUTSEP, OUT_DIGITS, vo->transp_total[i]);
		strcat(sw->GenOutput.sw_outstr, str);
	}

	/* transpiration for each vegetation type */
	ForEachVegType(k)
	{
		ForEachSoilLayer(i, n_layers)
		{
			snprintf(str, OUTSTRLEN, "%c%.*f", _OUTSEP, OUT_DIGITS, vo->transp[k][i]);
			strcat(sw->GenOutput.sw_outstr, str);
		}
	}
}
#endif

#if defined(RSOILWAT) || defined(SWNETCDF)

/**
@brief Gets transp_total when dealing with RSOILWAT.

@param[in] pd Time period in simulation output (day/week/month/year)
@param[in] sw Comprehensive struct of type SW_ALL containing all information
  in the simulation.
*/
void get_transp_mem(OutPeriod pd, SW_ALL* sw)
{
	LyrIndex i, n_layers = sw->Site.n_layers;
	int k;
	SW_SOILWAT_OUTPUTS *vo = sw->SoilWat.p_oagg[pd];
	size_t iOUTIndex;

	RealD *p = sw->GenOutput.p_OUT[eSW_Transp][pd];

    #if defined(RSOILWAT)
	get_outvalleader(&sw->Model, pd, sw->GenOutput.irow_OUT,
					 sw->GenOutput.nrow_OUT, sw->GenOutput.tOffset, p);
    #endif

	/* total transpiration */
	ForEachSoilLayer(i, n_layers)
	{
		iOUTIndex = iOUT(i, pd, sw->GenOutput);
		p[iOUTIndex] = vo->transp_total[i];
	}

	/* transpiration for each vegetation type */
	ForEachVegType(k)
	{
		ForEachSoilLayer(i, n_layers)
		{
			iOUTIndex = iOUT2(i, k + 1, pd, sw->GenOutput, n_layers);
			p[iOUTIndex] = vo->transp[k][i]; // k + 1 because of total transp.
		}
	}
}

#elif defined(STEPWAT)

/**
@brief Gets transp_total when dealing with STEPWAT.

@param[in] pd Time period in simulation output (day/week/month/year)
@param[in] sw Comprehensive struct of type SW_ALL containing all information
  in the simulation.
*/
void get_transp_agg(OutPeriod pd, SW_ALL* sw)
{
	LyrIndex i, n_layers = sw->Site.n_layers;
	int k;
	SW_SOILWAT_OUTPUTS *vo = sw->SoilWat.p_oagg[pd];
	size_t iOUTIndex;

	RealD
		*p = sw->GenOutput.p_OUT[eSW_Transp][pd],
		*psd = sw->GenOutput.p_OUTsd[eSW_Transp][pd];

	/* total transpiration */
	ForEachSoilLayer(i, n_layers)
	{
		iOUTIndex = iOUT(i, pd, sw->GenOutput);
		do_running_agg(p, psd, iOUTIndex, sw->GenOutput.currIter,
					   vo->transp_total[i]);
	}

	if (sw->GenOutput.print_IterationSummary) {
		sw->GenOutput.sw_outstr_agg[0] = '\0';
		format_IterationSummary(p, psd, pd, n_layers, sw);
	}

	/* transpiration for each vegetation type */
	ForEachVegType(k)
	{
		ForEachSoilLayer(i, n_layers)
		{
			// k + 1 because of total transp.
			iOUTIndex = iOUT2(i, k + 1, pd, sw->GenOutput, n_layers);
			do_running_agg(p, psd, iOUTIndex, sw->GenOutput.currIter,
						   vo->transp[k][i]);
		}
	}

	if (sw->GenOutput.print_IterationSummary) {
		format_IterationSummary2(p, psd, pd, NVEGTYPES, 1, sw);
	}
}

/**
@brief STEPWAT2 expects monthly sum of transpiration by soil layer. <BR>
				see function '_transp_contribution_by_group'

@param[in] pd Time period in simulation output (day/week/month/year)
@param[in] sw Comprehensive struct of type SW_ALL containing all information
  in the simulation.
*/
void get_transp_SXW(OutPeriod pd, SW_ALL* sw)
{
	TimeInt month;

	if (pd == eSW_Month) {
		LyrIndex i;
		int k;
		SW_SOILWAT_OUTPUTS *vo = sw->SoilWat.p_oagg[pd];
		month = sw->Model.month - sw->GenOutput.tOffset;

		/* total transpiration */
		ForEachSoilLayer(i, sw->Site.n_layers)
		{
			sw->GenOutput.transpTotal[i][month] = vo->transp_total[i];
		}

		/* transpiration for each vegetation type */
		ForEachVegType(k)
		{
			ForEachSoilLayer(i, sw->Site.n_layers)
			{
				sw->GenOutput.transpVeg[k][i][month] = vo->transp[k][i];
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
void get_evapSoil_text(OutPeriod pd, SW_ALL* sw)
{
	LyrIndex i;
	SW_SOILWAT_OUTPUTS *vo = sw->SoilWat.p_oagg[pd];

	char str[OUTSTRLEN];
	sw->GenOutput.sw_outstr[0] = '\0';

	ForEachEvapLayer(i, sw->Site.n_evap_lyrs)
	{
		snprintf(str, OUTSTRLEN, "%c%.*f", _OUTSEP, OUT_DIGITS, vo->evap_baresoil[i]);
		strcat(sw->GenOutput.sw_outstr, str);
	}
}
#endif

#if defined(RSOILWAT) || defined(SWNETCDF)

/**
@brief Gets evap when dealing with RSOILWAT.

@param[in] pd Time period in simulation output (day/week/month/year)
@param[in] sw Comprehensive struct of type SW_ALL containing all information
  in the simulation.
*/
void get_evapSoil_mem(OutPeriod pd, SW_ALL* sw)
{
	LyrIndex i;
	SW_SOILWAT_OUTPUTS *vo = sw->SoilWat.p_oagg[pd];
	size_t iOUTIndex;

	RealD *p = sw->GenOutput.p_OUT[eSW_EvapSoil][pd];

    #if defined(RSOILWAT)
	get_outvalleader(&sw->Model, pd, sw->GenOutput.irow_OUT,
					 sw->GenOutput.nrow_OUT, sw->GenOutput.tOffset, p);
    #endif

	ForEachEvapLayer(i, sw->Site.n_evap_lyrs)
	{
		iOUTIndex = iOUT(i, pd, sw->GenOutput);
		p[iOUTIndex] = vo->evap_baresoil[i];
	}
}

#elif defined(STEPWAT)

/**
@brief Gets evap when dealing with STEPWAT.

@param[in] pd Time period in simulation output (day/week/month/year)
@param[in] sw Comprehensive struct of type SW_ALL containing all information
  in the simulation.
*/
void get_evapSoil_agg(OutPeriod pd, SW_ALL* sw)
{
	LyrIndex i;
	SW_SOILWAT_OUTPUTS *vo = sw->SoilWat.p_oagg[pd];
	size_t iOUTIndex;

	RealD
		*p = sw->GenOutput.p_OUT[eSW_EvapSoil][pd],
		*psd = sw->GenOutput.p_OUTsd[eSW_EvapSoil][pd];

	ForEachEvapLayer(i, sw->Site.n_evap_lyrs)
	{
		iOUTIndex = iOUT(i, pd, sw->GenOutput);
		do_running_agg(p, psd, iOUTIndex, sw->GenOutput.currIter,
					   vo->evap_baresoil[i]);
	}

	if (sw->GenOutput.print_IterationSummary) {
		sw->GenOutput.sw_outstr_agg[0] = '\0';
		format_IterationSummary(p, psd, pd, sw->GenOutput.ncol_OUT[eSW_EvapSoil], sw);
	}
}
#endif


//------ eSW_EvapSurface
#ifdef SW_OUTTEXT

/**
@brief Gets evapSurface when dealing with OUTTEXT.

@param[in] pd Time period in simulation output (day/week/month/year)
@param[in] sw Comprehensive struct of type SW_ALL containing all information
  in the simulation.
*/
void get_evapSurface_text(OutPeriod pd, SW_ALL* sw)
{
	int k;
	SW_SOILWAT_OUTPUTS *vo = sw->SoilWat.p_oagg[pd];

	char str[OUTSTRLEN];
	sw->GenOutput.sw_outstr[0] = '\0';

	snprintf(sw->GenOutput.sw_outstr, sizeof sw->GenOutput.sw_outstr,"%c%.*f", _OUTSEP, OUT_DIGITS, vo->total_evap);

	ForEachVegType(k) {
		snprintf(str, OUTSTRLEN, "%c%.*f", _OUTSEP, OUT_DIGITS, vo->evap_veg[k]);
		strcat(sw->GenOutput.sw_outstr, str);
	}

	snprintf(str, OUTSTRLEN, "%c%.*f%c%.*f",
		_OUTSEP, OUT_DIGITS, vo->litter_evap,
		_OUTSEP, OUT_DIGITS, vo->surfaceWater_evap);
	strcat(sw->GenOutput.sw_outstr, str);
}
#endif

#if defined(RSOILWAT) || defined(SWNETCDF)

/**
@brief Gets evapSurface when dealing with RSOILWAT.

@param[in] pd Time period in simulation output (day/week/month/year)
@param[in] sw Comprehensive struct of type SW_ALL containing all information
  in the simulation.
*/
void get_evapSurface_mem(OutPeriod pd, SW_ALL* sw)
{
	int k;
	SW_SOILWAT_OUTPUTS *vo = sw->SoilWat.p_oagg[pd];
	size_t iOUTIndex;

	RealD *p = sw->GenOutput.p_OUT[eSW_EvapSurface][pd];

    #if defined(RSOILWAT)
	get_outvalleader(&sw->Model, pd, sw->GenOutput.irow_OUT,
					 sw->GenOutput.nrow_OUT, sw->GenOutput.tOffset, p);
    #endif

	iOUTIndex = iOUT(0, pd, sw->GenOutput);
	p[iOUTIndex] = vo->total_evap;

	ForEachVegType(k) {
		iOUTIndex = iOUT(k + 1, pd, sw->GenOutput);
		p[iOUTIndex] = vo->evap_veg[k];
	}

	iOUTIndex = iOUT(NVEGTYPES + 1, pd, sw->GenOutput);
	p[iOUTIndex] = vo->litter_evap;

	iOUTIndex = iOUT(NVEGTYPES + 2, pd, sw->GenOutput);
	p[iOUTIndex] = vo->surfaceWater_evap;
}

#elif defined(STEPWAT)

/**
@brief Gets evapSurface when dealing with STEPWAT.

@param[in] pd Time period in simulation output (day/week/month/year)
@param[in] sw Comprehensive struct of type SW_ALL containing all information
  in the simulation.
*/
void get_evapSurface_agg(OutPeriod pd, SW_ALL* sw)
{
	int k;
	SW_SOILWAT_OUTPUTS *vo = sw->SoilWat.p_oagg[pd];
	size_t iOUTIndex;

	RealD
		*p = sw->GenOutput.p_OUT[eSW_EvapSurface][pd],
		*psd = sw->GenOutput.p_OUTsd[eSW_EvapSurface][pd];

	iOUTIndex = iOUT(0, pd, sw->GenOutput);
	do_running_agg(p, psd, iOUTIndex, sw->GenOutput.currIter,
				   vo->total_evap);

	ForEachVegType(k) {
		iOUTIndex = iOUT(k + 1, pd, sw->GenOutput);
		do_running_agg(p, psd, iOUTIndex, sw->GenOutput.currIter,
					   vo->evap_veg[k]);
	}

	iOUTIndex = iOUT(NVEGTYPES + 1, pd, sw->GenOutput);
	do_running_agg(p, psd, iOUTIndex, sw->GenOutput.currIter,
				   vo->litter_evap);

	iOUTIndex = iOUT(NVEGTYPES + 2, pd, sw->GenOutput);
	do_running_agg(p, psd, iOUTIndex, sw->GenOutput.currIter,
				   vo->surfaceWater_evap);

	if (sw->GenOutput.print_IterationSummary) {
		sw->GenOutput.sw_outstr_agg[0] = '\0';
		format_IterationSummary(p, psd, pd,
								sw->GenOutput.ncol_OUT[eSW_EvapSurface], sw);
	}
}
#endif

//------ eSW_Interception
#ifdef SW_OUTTEXT

/**
@brief Gets total_int, int_veg, and litter_int when dealing with OUTTEXT.

@param[in] pd Time period in simulation output (day/week/month/year)
@param[in] sw Comprehensive struct of type SW_ALL containing all information
  in the simulation.
*/
void get_interception_text(OutPeriod pd, SW_ALL* sw)
{
	int k;
	SW_SOILWAT_OUTPUTS *vo = sw->SoilWat.p_oagg[pd];

	char str[OUTSTRLEN];
	sw->GenOutput.sw_outstr[0] = '\0';

	snprintf(sw->GenOutput.sw_outstr, sizeof sw->GenOutput.sw_outstr,"%c%.*f", _OUTSEP, OUT_DIGITS, vo->total_int);

	ForEachVegType(k) {
		snprintf(str, OUTSTRLEN, "%c%.*f", _OUTSEP, OUT_DIGITS, vo->int_veg[k]);
		strcat(sw->GenOutput.sw_outstr, str);
	}

	snprintf(str, OUTSTRLEN, "%c%.*f", _OUTSEP, OUT_DIGITS, vo->litter_int);
	strcat(sw->GenOutput.sw_outstr, str);
}
#endif

#if defined(RSOILWAT) || defined(SWNETCDF)

/**
@brief Gets total_int, int_veg, and litter_int when dealing with RSOILWAT.

@param[in] pd Time period in simulation output (day/week/month/year)
@param[in] sw Comprehensive struct of type SW_ALL containing all information
  in the simulation.
*/
void get_interception_mem(OutPeriod pd, SW_ALL* sw)
{
	int k;
	SW_SOILWAT_OUTPUTS *vo = sw->SoilWat.p_oagg[pd];
	size_t iOUTIndex;

	RealD *p = sw->GenOutput.p_OUT[eSW_Interception][pd];

    #if defined(RSOILWAT)
	get_outvalleader(&sw->Model, pd, sw->GenOutput.irow_OUT,
					 sw->GenOutput.nrow_OUT, sw->GenOutput.tOffset, p);
    #endif

	iOUTIndex = iOUT(0, pd, sw->GenOutput);
	p[iOUTIndex] = vo->total_int;

	ForEachVegType(k) {
		iOUTIndex = iOUT(k + 1, pd, sw->GenOutput);
		p[iOUTIndex] = vo->int_veg[k];
	}

	iOUTIndex = iOUT(NVEGTYPES + 1, pd, sw->GenOutput);
	p[iOUTIndex] = vo->litter_int;
}

#elif defined(STEPWAT)

/**
@brief Gets total_int, int_veg, and litter_int when dealing with STEPWAT.

@param[in] pd Time period in simulation output (day/week/month/year)
@param[in] sw Comprehensive struct of type SW_ALL containing all information
  in the simulation.
*/
void get_interception_agg(OutPeriod pd, SW_ALL* sw)
{
	int k;
	SW_SOILWAT_OUTPUTS *vo = sw->SoilWat.p_oagg[pd];
	size_t iOUTIndex;

	RealD
		*p = sw->GenOutput.p_OUT[eSW_Interception][pd],
		*psd = sw->GenOutput.p_OUTsd[eSW_Interception][pd];

	iOUTIndex = iOUT(0, pd, sw->GenOutput);
	do_running_agg(p, psd, iOUTIndex, sw->GenOutput.currIter,
				   vo->total_int);

	ForEachVegType(k) {
		iOUTIndex = iOUT(k + 1, pd, sw->GenOutput);
		do_running_agg(p, psd, iOUTIndex, sw->GenOutput.currIter,
					   vo->int_veg[k]);
	}

	iOUTIndex = iOUT(NVEGTYPES + 1, pd, sw->GenOutput);
	do_running_agg(p, psd, iOUTIndex, sw->GenOutput.currIter,
				   vo->litter_int);

	if (sw->GenOutput.print_IterationSummary) {
		sw->GenOutput.sw_outstr_agg[0] = '\0';
		format_IterationSummary(p, psd, pd, sw->GenOutput.ncol_OUT[eSW_Interception], sw);
	}
}
#endif

//------ eSW_SoilInf
#ifdef SW_OUTTEXT

/**
@brief Gets soil_inf when dealing with OUTTEXT.

@param[in] pd Time period in simulation output (day/week/month/year)
@param[in] sw Comprehensive struct of type SW_ALL containing all information
  in the simulation.
*/
void get_soilinf_text(OutPeriod pd, SW_ALL* sw)
{
	/* 20100202 (drs) added */
	/* 20110219 (drs) added runoff */
	/* 12/13/2012	(clk)	moved runoff, now named snowRunoff, to get_runoffrunon(); */
	SW_WEATHER_OUTPUTS *vo = sw->Weather.p_oagg[pd];

	sw->GenOutput.sw_outstr[0] = '\0';
	snprintf(sw->GenOutput.sw_outstr, sizeof sw->GenOutput.sw_outstr,"%c%.*f", _OUTSEP, OUT_DIGITS, vo->soil_inf);
}
#endif

#if defined(RSOILWAT) || defined(SWNETCDF)

/**
@brief Gets soil_inf when dealing with RSOILWAT.

@param[in] pd Time period in simulation output (day/week/month/year)
@param[in] sw Comprehensive struct of type SW_ALL containing all information
  in the simulation.
*/
void get_soilinf_mem(OutPeriod pd, SW_ALL* sw)
{
	SW_WEATHER_OUTPUTS *vo = sw->Weather.p_oagg[pd];
	size_t iOUTIndex;

	RealD *p = sw->GenOutput.p_OUT[eSW_SoilInf][pd];

    #if defined(RSOILWAT)
	get_outvalleader(&sw->Model, pd, sw->GenOutput.irow_OUT,
					 sw->GenOutput.nrow_OUT, sw->GenOutput.tOffset, p);
    #endif

	iOUTIndex = iOUT(0, pd, sw->GenOutput);
	p[iOUTIndex] = vo->soil_inf;
}

#elif defined(STEPWAT)

/**
@brief Gets soil_inf when dealing with STEPWAT.

@param[in] pd Time period in simulation output (day/week/month/year)
@param[in] sw Comprehensive struct of type SW_ALL containing all information
  in the simulation.
*/
void get_soilinf_agg(OutPeriod pd, SW_ALL* sw)
{
	SW_WEATHER_OUTPUTS *vo = sw->Weather.p_oagg[pd];
	size_t iOUTIndex;

	RealD
		*p = sw->GenOutput.p_OUT[eSW_SoilInf][pd],
		*psd = sw->GenOutput.p_OUTsd[eSW_SoilInf][pd];

	iOUTIndex = iOUT(0, pd, sw->GenOutput);
	do_running_agg(p, psd, iOUTIndex, sw->GenOutput.currIter,
				   vo->soil_inf);

	if (sw->GenOutput.print_IterationSummary) {
		sw->GenOutput.sw_outstr_agg[0] = '\0';
		format_IterationSummary(p, psd, pd,
										sw->GenOutput.ncol_OUT[eSW_SoilInf], sw);
	}
}
#endif


//------ eSW_LyrDrain
#ifdef SW_OUTTEXT

/**
@brief Gets lyrdrain when dealing with OUTTEXT.

@param[in] pd Time period in simulation output (day/week/month/year)
@param[in] sw Comprehensive struct of type SW_ALL containing all information
  in the simulation.
*/
void get_lyrdrain_text(OutPeriod pd, SW_ALL* sw)
{
	/* 20100202 (drs) added */
	LyrIndex i;
	SW_SOILWAT_OUTPUTS *vo = sw->SoilWat.p_oagg[pd];

	char str[OUTSTRLEN];
	sw->GenOutput.sw_outstr[0] = '\0';

	for (i = 0; i < sw->Site.n_layers - 1; i++)
	{
		snprintf(str, OUTSTRLEN, "%c%.*f", _OUTSEP, OUT_DIGITS, vo->lyrdrain[i]);
		strcat(sw->GenOutput.sw_outstr, str);
	}
}
#endif

#if defined(RSOILWAT) || defined(SWNETCDF)

/**
@brief Gets lyrdrain when dealing with RSOILWAT.

@param[in] pd Time period in simulation output (day/week/month/year)
@param[in] sw Comprehensive struct of type SW_ALL containing all information
  in the simulation.
*/
void get_lyrdrain_mem(OutPeriod pd, SW_ALL* sw)
{
	LyrIndex i;
	SW_SOILWAT_OUTPUTS *vo = sw->SoilWat.p_oagg[pd];
	size_t iOUTIndex;

	RealD *p = sw->GenOutput.p_OUT[eSW_LyrDrain][pd];

    #if defined(RSOILWAT)
	get_outvalleader(&sw->Model, pd, sw->GenOutput.irow_OUT,
					 sw->GenOutput.nrow_OUT, sw->GenOutput.tOffset, p);
    #endif

	for (i = 0; i < sw->Site.n_layers - 1; i++)
	{
		iOUTIndex = iOUT(i, pd, sw->GenOutput);
		p[iOUTIndex] = vo->lyrdrain[i];
	}
}

#elif defined(STEPWAT)

/**
@brief Gets lyrdrain when dealing with STEPWAT.

@param[in] pd Time period in simulation output (day/week/month/year)
@param[in] sw Comprehensive struct of type SW_ALL containing all information
  in the simulation.
*/
void get_lyrdrain_agg(OutPeriod pd, SW_ALL* sw)
{
	LyrIndex i;
	SW_SOILWAT_OUTPUTS *vo = sw->SoilWat.p_oagg[pd];
	size_t iOUTIndex;

	RealD
		*p = sw->GenOutput.p_OUT[eSW_LyrDrain][pd],
		*psd = sw->GenOutput.p_OUTsd[eSW_LyrDrain][pd];

	for (i = 0; i < sw->Site.n_layers - 1; i++)
	{
		iOUTIndex = iOUT(i, pd, sw->GenOutput);
		do_running_agg(p, psd, iOUTIndex, sw->GenOutput.currIter,
					   vo->lyrdrain[i]);
	}

	if (sw->GenOutput.print_IterationSummary) {
		sw->GenOutput.sw_outstr_agg[0] = '\0';
		format_IterationSummary(p, psd, pd,
								sw->GenOutput.ncol_OUT[eSW_LyrDrain], sw);
	}
}
#endif


//------ eSW_HydRed
#ifdef SW_OUTTEXT

/**
@brief Gets hydred and hydred_total when dealing with OUTTEXT.

@param[in] pd Time period in simulation output (day/week/month/year)
@param[in] sw Comprehensive struct of type SW_ALL containing all information
  in the simulation.
*/
void get_hydred_text(OutPeriod pd, SW_ALL* sw)
{
	/* 20101020 (drs) added */
	LyrIndex i, n_layers = sw->Site.n_layers;
	int k;
	SW_SOILWAT_OUTPUTS *vo = sw->SoilWat.p_oagg[pd];

	char str[OUTSTRLEN];
	sw->GenOutput.sw_outstr[0] = '\0';

	/* total hydraulic redistribution */
	ForEachSoilLayer(i, n_layers)
	{
		snprintf(str, OUTSTRLEN, "%c%.*f", _OUTSEP, OUT_DIGITS, vo->hydred_total[i]);
		strcat(sw->GenOutput.sw_outstr, str);
	}

	/* hydraulic redistribution for each vegetation type */
	ForEachVegType(k)
	{
		ForEachSoilLayer(i, n_layers)
		{
			snprintf(str, OUTSTRLEN, "%c%.*f", _OUTSEP, OUT_DIGITS, vo->hydred[k][i]);
			strcat(sw->GenOutput.sw_outstr, str);
		}
	}
}
#endif

#if defined(RSOILWAT) || defined(SWNETCDF)

/**
@brief Gets hydred and hydred_total when dealing with RSOILWAT.

@param[in] pd Time period in simulation output (day/week/month/year)
@param[in] sw Comprehensive struct of type SW_ALL containing all information
  in the simulation.
*/
void get_hydred_mem(OutPeriod pd, SW_ALL* sw)
{
	LyrIndex i, n_layers = sw->Site.n_layers;
	int k;
	SW_SOILWAT_OUTPUTS *vo = sw->SoilWat.p_oagg[pd];
	size_t iOUTIndex;

	RealD *p = sw->GenOutput.p_OUT[eSW_HydRed][pd];

    #if defined(RSOILWAT)
	get_outvalleader(&sw->Model, pd, sw->GenOutput.irow_OUT,
					 sw->GenOutput.nrow_OUT, sw->GenOutput.tOffset, p);
    #endif

	/* total hydraulic redistribution */
	ForEachSoilLayer(i, n_layers)
	{
		iOUTIndex = iOUT(i, pd, sw->GenOutput);
		p[iOUTIndex] = vo->hydred_total[i];
	}

	/* hydraulic redistribution for each vegetation type */
	ForEachVegType(k)
	{
		ForEachSoilLayer(i, n_layers)
		{
			iOUTIndex = iOUT2(i, k + 1, pd, sw->GenOutput, n_layers);
			p[iOUTIndex] = vo->hydred[k][i]; // k + 1 because of total hydred
		}
	}
}

#elif defined(STEPWAT)

/**
@brief Gets hydred and hydred_total when dealing with STEPWAT.

@param[in] pd Time period in simulation output (day/week/month/year)
@param[in] sw Comprehensive struct of type SW_ALL containing all information
  in the simulation.
*/
void get_hydred_agg(OutPeriod pd, SW_ALL* sw)
{
	LyrIndex i, n_layers = sw->Site.n_layers;
	int k;
	SW_SOILWAT_OUTPUTS *vo = sw->SoilWat.p_oagg[pd];
	size_t iOUTIndex;

	RealD
		*p = sw->GenOutput.p_OUT[eSW_HydRed][pd],
		*psd = sw->GenOutput.p_OUTsd[eSW_HydRed][pd];

	/* total hydraulic redistribution */
	ForEachSoilLayer(i, n_layers)
	{
		iOUTIndex = iOUT(i, pd, sw->GenOutput);
		do_running_agg(p, psd, iOUTIndex, sw->GenOutput.currIter,
					   vo->hydred_total[i]);
	}

	if (sw->GenOutput.print_IterationSummary) {
		sw->GenOutput.sw_outstr_agg[0] = '\0';
		format_IterationSummary(p, psd, pd, n_layers, sw);
	}

	/* hydraulic redistribution for each vegetation type */
	ForEachVegType(k)
	{
		ForEachSoilLayer(i, n_layers)
		{
			// k + 1 because of total hydred
			iOUTIndex = iOUT2(i, k + 1, pd, sw->GenOutput, n_layers);
			do_running_agg(p, psd, iOUTIndex, sw->GenOutput.currIter,
						   vo->hydred[k][i]);
		}
	}

	if (sw->GenOutput.print_IterationSummary) {
		format_IterationSummary2(p, psd, pd, NVEGTYPES, 1, sw);
	}
}
#endif


//------ eSW_AET
#ifdef SW_OUTTEXT

/**
@brief Gets actual evapotranspiration when dealing with OUTTEXT.

@param[in] pd Time period in simulation output (day/week/month/year)
@param[in] sw Comprehensive struct of type SW_ALL containing all information
  in the simulation.
*/
void get_aet_text(OutPeriod pd, SW_ALL* sw)
{
	SW_SOILWAT_OUTPUTS *vo = sw->SoilWat.p_oagg[pd];
	SW_WEATHER_OUTPUTS *vo2 = sw->Weather.p_oagg[pd];

	sw->GenOutput.sw_outstr[0] = '\0';
	snprintf(
		sw->GenOutput.sw_outstr,
		sizeof sw->GenOutput.sw_outstr,
		"%c%.*f%c%.*f%c%.*f%c%.*f%c%.*f%c%.*f",
		_OUTSEP, OUT_DIGITS, vo->aet,
		_OUTSEP, OUT_DIGITS, vo->tran,
		_OUTSEP, OUT_DIGITS, vo->esoil,
		_OUTSEP, OUT_DIGITS, vo->ecnw,
		_OUTSEP, OUT_DIGITS, vo->esurf,
		_OUTSEP, OUT_DIGITS, vo2->snowloss // should be `vo->esnow`
	);

}
#endif

#if defined(RSOILWAT) || defined(SWNETCDF)

/**
@brief Gets actual evapotranspiration when dealing with RSOILWAT.

@param[in] pd Time period in simulation output (day/week/month/year)
@param[in] sw Comprehensive struct of type SW_ALL containing all information
  in the simulation.
*/
void get_aet_mem(OutPeriod pd, SW_ALL* sw)
{
	SW_SOILWAT_OUTPUTS *vo = sw->SoilWat.p_oagg[pd];
	SW_WEATHER_OUTPUTS *vo2 = sw->Weather.p_oagg[pd];
	size_t iOUTIndex;

	RealD *p = sw->GenOutput.p_OUT[eSW_AET][pd];

    #if defined(RSOILWAT)
	get_outvalleader(&sw->Model, pd, sw->GenOutput.irow_OUT,
					 sw->GenOutput.nrow_OUT, sw->GenOutput.tOffset, p);
    #endif

	iOUTIndex = iOUT(0, pd, sw->GenOutput);
	p[iOUTIndex] = vo->aet;

	iOUTIndex = iOUT(1, pd, sw->GenOutput);
	p[iOUTIndex] = vo->tran;

	iOUTIndex = iOUT(2, pd, sw->GenOutput);
	p[iOUTIndex] = vo->esoil;

	iOUTIndex = iOUT(3, pd, sw->GenOutput);
	p[iOUTIndex] = vo->ecnw;

	iOUTIndex = iOUT(4, pd, sw->GenOutput);
	p[iOUTIndex] = vo->esurf;

	iOUTIndex = iOUT(5, pd, sw->GenOutput);
	p[iOUTIndex] = vo2->snowloss; // should be `vo->esnow`
}

#elif defined(STEPWAT)

/**
@brief Gets actual evapotranspiration when dealing with STEPWAT.

@param[in] pd Time period in simulation output (day/week/month/year)
@param[in] sw Comprehensive struct of type SW_ALL containing all information
  in the simulation.
*/
void get_aet_agg(OutPeriod pd, SW_ALL* sw)
{
	SW_SOILWAT_OUTPUTS *vo = sw->SoilWat.p_oagg[pd];
	SW_WEATHER_OUTPUTS *vo2 = sw->Weather.p_oagg[pd];
	size_t iOUTIndex;

	RealD
		*p = sw->GenOutput.p_OUT[eSW_AET][pd],
		*psd = sw->GenOutput.p_OUTsd[eSW_AET][pd];

	iOUTIndex = iOUT(0, pd, sw->GenOutput);
	do_running_agg(p, psd, iOUTIndex, sw->GenOutput.currIter,
				   vo->aet);

	iOUTIndex = iOUT(1, pd, sw->GenOutput);
	do_running_agg(p, psd, iOUTIndex, sw->GenOutput.currIter,
				   vo->tran);

	iOUTIndex = iOUT(2, pd, sw->GenOutput);
	do_running_agg(p, psd, iOUTIndex, sw->GenOutput.currIter,
				   vo->esoil);

	iOUTIndex = iOUT(3, pd, sw->GenOutput);
	do_running_agg(p, psd, iOUTIndex, sw->GenOutput.currIter,
				   vo->ecnw);

	iOUTIndex = iOUT(4, pd, sw->GenOutput);
	do_running_agg(p, psd, iOUTIndex, sw->GenOutput.currIter,
				   vo->esurf);

	// should be `vo->esnow`
	iOUTIndex = iOUT(5, pd, sw->GenOutput);
	do_running_agg(p, psd, iOUTIndex, sw->GenOutput.currIter,
				   vo2->snowloss);

	if (sw->GenOutput.print_IterationSummary) {
		sw->GenOutput.sw_outstr_agg[0] = '\0';
		format_IterationSummary(p, psd, pd, sw->GenOutput.ncol_OUT[eSW_AET], sw);
	}
}

/**
@brief STEPWAT2 expects annual sum of actual evapotranspiration

@param[in] pd Time period in simulation output (day/week/month/year)
@param[in] sw Comprehensive struct of type SW_ALL containing all information
  in the simulation.
*/
void get_aet_SXW(OutPeriod pd, SW_ALL* sw)
{
	if (pd == eSW_Year) {
		SW_SOILWAT_OUTPUTS *vo = sw->SoilWat.p_oagg[pd];

		sw->GenOutput.aet = vo->aet;
	}
}
#endif


//------ eSW_PET
#ifdef SW_OUTTEXT

/**
@brief Gets potential evapotranspiration and radiation
				when dealing with OUTTEXT.

@param[in] pd Time period in simulation output (day/week/month/year)
@param[in] sw Comprehensive struct of type SW_ALL containing all information
  in the simulation.
*/
void get_pet_text(OutPeriod pd, SW_ALL* sw)
{
	SW_SOILWAT_OUTPUTS *vo = sw->SoilWat.p_oagg[pd];

	sw->GenOutput.sw_outstr[0] = '\0';
	snprintf(
		sw->GenOutput.sw_outstr,
		sizeof sw->GenOutput.sw_outstr,
		"%c%.*f%c%.*f%c%.*f%c%.*f%c%.*f",
		_OUTSEP, OUT_DIGITS, vo->pet,
		_OUTSEP, OUT_DIGITS, vo->H_oh,
		_OUTSEP, OUT_DIGITS, vo->H_ot,
		_OUTSEP, OUT_DIGITS, vo->H_gh,
		_OUTSEP, OUT_DIGITS, vo->H_gt
	);
}
#endif

#if defined(RSOILWAT) || defined(SWNETCDF)

/**
@brief Gets potential evapotranspiration and radiation
			when dealing with OUTTEXT.

@param[in] pd Time period in simulation output (day/week/month/year)
@param[in] sw Comprehensive struct of type SW_ALL containing all information
  in the simulation.
*/
void get_pet_mem(OutPeriod pd, SW_ALL* sw)
{
	SW_SOILWAT_OUTPUTS *vo = sw->SoilWat.p_oagg[pd];
	size_t iOUTIndex;

	RealD *p = sw->GenOutput.p_OUT[eSW_PET][pd];

    #if defined(RSOILWAT)
	get_outvalleader(&sw->Model, pd, sw->GenOutput.irow_OUT,
					 sw->GenOutput.nrow_OUT, sw->GenOutput.tOffset, p);
    #endif

	iOUTIndex = iOUT(0, pd, sw->GenOutput);
	p[iOUTIndex] = vo->pet;

	iOUTIndex = iOUT(1, pd, sw->GenOutput);
	p[iOUTIndex] = vo->H_oh;

	iOUTIndex = iOUT(2, pd, sw->GenOutput);
	p[iOUTIndex] = vo->H_ot;

	iOUTIndex = iOUT(3, pd, sw->GenOutput);
	p[iOUTIndex] = vo->H_gh;

	iOUTIndex = iOUT(4, pd, sw->GenOutput);
	p[iOUTIndex] = vo->H_gt;
}

#elif defined(STEPWAT)

/**
@brief Gets potential evapotranspiration and radiation
			when dealing with OUTTEXT.
*/
void get_pet_agg(OutPeriod pd, SW_ALL* sw)
{
	SW_SOILWAT_OUTPUTS *vo = sw->SoilWat.p_oagg[pd];
	size_t iOUTIndex;

	RealD
		*p = sw->GenOutput.p_OUT[eSW_PET][pd],
		*psd = sw->GenOutput.p_OUTsd[eSW_PET][pd];

	iOUTIndex = iOUT(0, pd, sw->GenOutput);
	do_running_agg(p, psd, iOUTIndex, sw->GenOutput.currIter, vo->pet);

	iOUTIndex = iOUT(1, pd, sw->GenOutput);
	do_running_agg(p, psd, iOUTIndex, sw->GenOutput.currIter, vo->H_oh);

	iOUTIndex = iOUT(2, pd, sw->GenOutput);
	do_running_agg(p, psd, iOUTIndex, sw->GenOutput.currIter, vo->H_ot);

	iOUTIndex = iOUT(3, pd, sw->GenOutput);
	do_running_agg(p, psd, iOUTIndex, sw->GenOutput.currIter, vo->H_gh);

	iOUTIndex = iOUT(4, pd, sw->GenOutput);
	do_running_agg(p, psd, iOUTIndex, sw->GenOutput.currIter, vo->H_gt);

	if (sw->GenOutput.print_IterationSummary) {
		sw->GenOutput.sw_outstr_agg[0] = '\0';
		format_IterationSummary(p, psd, pd, sw->GenOutput.ncol_OUT[eSW_PET], sw);
	}
}
#endif


//------ eSW_WetDays
#ifdef SW_OUTTEXT

/**
@brief Gets is_wet and wetdays when dealing with OUTTEXT.

@param[in] pd Time period in simulation output (day/week/month/year)
@param[in] sw Comprehensive struct of type SW_ALL containing all information
  in the simulation.
*/
void get_wetdays_text(OutPeriod pd, SW_ALL* sw)
{
	LyrIndex i, n_layers = sw->Site.n_layers;

	char str[OUTSTRLEN];
	sw->GenOutput.sw_outstr[0] = '\0';

	if (pd == eSW_Day)
	{
		ForEachSoilLayer(i, n_layers) {
			snprintf(str, OUTSTRLEN, "%c%i", _OUTSEP, (sw->SoilWat.is_wet[i]) ? 1 : 0);
			strcat(sw->GenOutput.sw_outstr, str);
		}

	} else
	{
		SW_SOILWAT_OUTPUTS *vo = sw->SoilWat.p_oagg[pd];

		ForEachSoilLayer(i, n_layers) {
			snprintf(str, OUTSTRLEN, "%c%i", _OUTSEP, (int) vo->wetdays[i]);
			strcat(sw->GenOutput.sw_outstr, str);
		}
	}
}
#endif

#if defined(RSOILWAT) || defined(SWNETCDF)

/**
@brief Gets is_wet and wetdays when dealing with RSOILWAT.

@param[in] pd Time period in simulation output (day/week/month/year)
@param[in] sw Comprehensive struct of type SW_ALL containing all information
  in the simulation.
*/
void get_wetdays_mem(OutPeriod pd, SW_ALL* sw)
{
	LyrIndex i;
	size_t iOUTIndex;

	RealD *p = sw->GenOutput.p_OUT[eSW_WetDays][pd];

    #if defined(RSOILWAT)
	get_outvalleader(&sw->Model, pd, sw->GenOutput.irow_OUT,
					 sw->GenOutput.nrow_OUT, sw->GenOutput.tOffset, p);
    #endif

	if (pd == eSW_Day)
	{
		ForEachSoilLayer(i, sw->Site.n_layers) {
			iOUTIndex = iOUT(i, pd, sw->GenOutput);
			p[iOUTIndex] = (sw->SoilWat.is_wet[i]) ? 1 : 0;
		}

	} else
	{
		SW_SOILWAT_OUTPUTS *vo = sw->SoilWat.p_oagg[pd];

		ForEachSoilLayer(i, sw->Site.n_layers) {
			iOUTIndex = iOUT(i, pd, sw->GenOutput);
			p[iOUTIndex] = (int) vo->wetdays[i];
		}
	}
}

#elif defined(STEPWAT)

/**
@brief Gets is_wet and wetdays when dealing with RSOILWAT.

@param[in] pd Time period in simulation output (day/week/month/year)
@param[in] sw Comprehensive struct of type SW_ALL containing all information
  in the simulation.
*/
void get_wetdays_agg(OutPeriod pd, SW_ALL* sw)
{
	LyrIndex i;
	size_t iOUTIndex;

	RealD
		*p = sw->GenOutput.p_OUT[eSW_WetDays][pd],
		*psd = sw->GenOutput.p_OUTsd[eSW_WetDays][pd];

	if (pd == eSW_Day)
	{
		ForEachSoilLayer(i, sw->Site.n_layers) {
			iOUTIndex = iOUT(i, pd, sw->GenOutput);
			do_running_agg(p, psd, iOUTIndex,sw->GenOutput.currIter,
						   (sw->SoilWat.is_wet[i]) ? 1 : 0);
		}

	} else
	{
		SW_SOILWAT_OUTPUTS *vo = sw->SoilWat.p_oagg[pd];

		ForEachSoilLayer(i, sw->Site.n_layers) {
			iOUTIndex = iOUT(i, pd, sw->GenOutput);
			do_running_agg(p, psd, iOUTIndex, sw->GenOutput.currIter,
						   vo->wetdays[i]);
		}
	}

	if (sw->GenOutput.print_IterationSummary) {
		sw->GenOutput.sw_outstr_agg[0] = '\0';
		format_IterationSummary(p, psd, pd, sw->GenOutput.ncol_OUT[eSW_WetDays], sw);
	}
}
#endif


//------ eSW_SnowPack
#ifdef SW_OUTTEXT

/**
@brief Gets snowpack and snowdepth when dealing with OUTTEXT.

@param[in] pd Time period in simulation output (day/week/month/year)
@param[in] sw Comprehensive struct of type SW_ALL containing all information
  in the simulation.
*/
void get_snowpack_text(OutPeriod pd, SW_ALL* sw)
{
	SW_SOILWAT_OUTPUTS *vo = sw->SoilWat.p_oagg[pd];

	sw->GenOutput.sw_outstr[0] = '\0';
	snprintf(sw->GenOutput.sw_outstr, sizeof sw->GenOutput.sw_outstr,"%c%.*f%c%.*f",
		_OUTSEP, OUT_DIGITS, vo->snowpack,
		_OUTSEP, OUT_DIGITS, vo->snowdepth);
}
#endif

#if defined(RSOILWAT) || defined(SWNETCDF)

/**
@brief Gets snowpack and snowdepth when dealing with OUTTEXT.

@param[in] pd Time period in simulation output (day/week/month/year)
@param[in] sw Comprehensive struct of type SW_ALL containing all information
  in the simulation.
*/
void get_snowpack_mem(OutPeriod pd, SW_ALL* sw)
{
	SW_SOILWAT_OUTPUTS *vo = sw->SoilWat.p_oagg[pd];
	size_t iOUTIndex;

	RealD *p = sw->GenOutput.p_OUT[eSW_SnowPack][pd];

    #if defined(RSOILWAT)
	get_outvalleader(&sw->Model, pd, sw->GenOutput.irow_OUT,
					 sw->GenOutput.nrow_OUT, sw->GenOutput.tOffset, p);
    #endif

	iOUTIndex = iOUT(0, pd, sw->GenOutput);
	p[iOUTIndex] = vo->snowpack;

	iOUTIndex = iOUT(1, pd, sw->GenOutput);
	p[iOUTIndex] = vo->snowdepth;
}

#elif defined(STEPWAT)

/**
@brief Gets snowpack and snowdepth when dealing with STEPWAT.

@param[in] pd Time period in simulation output (day/week/month/year)
@param[in] sw Comprehensive struct of type SW_ALL containing all information
  in the simulation.
*/
void get_snowpack_agg(OutPeriod pd, SW_ALL* sw)
{
	SW_SOILWAT_OUTPUTS *vo = sw->SoilWat.p_oagg[pd];
	size_t iOUTIndex;

	RealD
		*p = sw->GenOutput.p_OUT[eSW_SnowPack][pd],
		*psd = sw->GenOutput.p_OUTsd[eSW_SnowPack][pd];

	iOUTIndex = iOUT(0, pd, sw->GenOutput);
	do_running_agg(p, psd, iOUTIndex, sw->GenOutput.currIter,
				   vo->snowpack);

	iOUTIndex = iOUT(1, pd, sw->GenOutput);
	do_running_agg(p, psd, iOUTIndex, sw->GenOutput.currIter,
				   vo->snowdepth);

	if (sw->GenOutput.print_IterationSummary) {
		sw->GenOutput.sw_outstr_agg[0] = '\0';
		format_IterationSummary(p, psd, pd, sw->GenOutput.ncol_OUT[eSW_SnowPack], sw);
	}
}
#endif


//------ eSW_DeepSWC
#ifdef SW_OUTTEXT

/**
@brief Gets deep for when dealing with OUTTEXT.

@param[in] pd Time period in simulation output (day/week/month/year)
@param[in] sw Comprehensive struct of type SW_ALL containing all information
  in the simulation.
*/
void get_deepswc_text(OutPeriod pd, SW_ALL* sw)
{
	SW_SOILWAT_OUTPUTS *vo = sw->SoilWat.p_oagg[pd];

	sw->GenOutput.sw_outstr[0] = '\0';
	snprintf(sw->GenOutput.sw_outstr, sizeof sw->GenOutput.sw_outstr,"%c%.*f", _OUTSEP, OUT_DIGITS, vo->deep);
}
#endif

#if defined(RSOILWAT) || defined(SWNETCDF)

/**
@brief Gets deep for when dealing with RSOILWAT.

@param[in] pd Time period in simulation output (day/week/month/year)
@param[in] sw Comprehensive struct of type SW_ALL containing all information
  in the simulation.
*/
void get_deepswc_mem(OutPeriod pd, SW_ALL* sw)
{
	SW_SOILWAT_OUTPUTS *vo = sw->SoilWat.p_oagg[pd];
	size_t iOUTIndex;

	RealD *p = sw->GenOutput.p_OUT[eSW_DeepSWC][pd];

    #if defined(RSOILWAT)
	get_outvalleader(&sw->Model, pd, sw->GenOutput.irow_OUT,
					 sw->GenOutput.nrow_OUT, sw->GenOutput.tOffset, p);
    #endif

	iOUTIndex = iOUT(0, pd, sw->GenOutput);
	p[iOUTIndex] = vo->deep;
}

#elif defined(STEPWAT)

/**
@brief Gets deep for when dealing with STEPWAT.

@param[in] pd Time period in simulation output (day/week/month/year)
@param[in] sw Comprehensive struct of type SW_ALL containing all information
  in the simulation.
*/
void get_deepswc_agg(OutPeriod pd, SW_ALL* sw)
{
	SW_SOILWAT_OUTPUTS *vo = sw->SoilWat.p_oagg[pd];
	size_t iOUTIndex;

	RealD
		*p = sw->GenOutput.p_OUT[eSW_DeepSWC][pd],
		*psd = sw->GenOutput.p_OUTsd[eSW_DeepSWC][pd];

	iOUTIndex = iOUT(0, pd, sw->GenOutput);
	do_running_agg(p, psd, iOUTIndex, sw->GenOutput.currIter,
				   vo->deep);

	if (sw->GenOutput.print_IterationSummary) {
		sw->GenOutput.sw_outstr_agg[0] = '\0';
		format_IterationSummary(p, psd, pd, sw->GenOutput.ncol_OUT[eSW_DeepSWC], sw);
	}
}
#endif


//------ eSW_SoilTemp
#ifdef SW_OUTTEXT

/**
@brief Gets soil temperature for when dealing with OUTTEXT.

@param[in] pd Time period in simulation output (day/week/month/year)
@param[in] sw Comprehensive struct of type SW_ALL containing all information
  in the simulation.
*/
void get_soiltemp_text(OutPeriod pd, SW_ALL* sw)
{
	LyrIndex i;
	SW_SOILWAT_OUTPUTS *vo = sw->SoilWat.p_oagg[pd];

	char str[OUTSTRLEN];
	sw->GenOutput.sw_outstr[0] = '\0';

	ForEachSoilLayer(i, sw->Site.n_layers)
	{
        snprintf(str, OUTSTRLEN, "%c%.*f", _OUTSEP, OUT_DIGITS, vo->maxLyrTemperature[i]);
        strcat(sw->GenOutput.sw_outstr, str);

        snprintf(str, OUTSTRLEN, "%c%.*f", _OUTSEP, OUT_DIGITS, vo->minLyrTemperature[i]);
        strcat(sw->GenOutput.sw_outstr, str);

        snprintf(str, OUTSTRLEN, "%c%.*f", _OUTSEP, OUT_DIGITS, vo->avgLyrTemp[i]);
        strcat(sw->GenOutput.sw_outstr, str);
	}
}
#endif

#if defined(RSOILWAT) || defined(SWNETCDF)

/**
@brief Gets soil temperature for when dealing with RSOILWAT.

@param[in] pd Time period in simulation output (day/week/month/year)
@param[in] sw Comprehensive struct of type SW_ALL containing all information
  in the simulation.
*/
void get_soiltemp_mem(OutPeriod pd, SW_ALL* sw)
{
	LyrIndex i;
	SW_SOILWAT_OUTPUTS *vo = sw->SoilWat.p_oagg[pd];
	size_t iOUTIndex;

	RealD *p = sw->GenOutput.p_OUT[eSW_SoilTemp][pd];

    #if defined(RSOILWAT)
	get_outvalleader(&sw->Model, pd, sw->GenOutput.irow_OUT,
					 sw->GenOutput.nrow_OUT, sw->GenOutput.tOffset, p);
    #endif

	ForEachSoilLayer(i, sw->Site.n_layers)
	{
		iOUTIndex = iOUT((i * 3), pd, sw->GenOutput);
        p[iOUTIndex] = vo->maxLyrTemperature[i];

		iOUTIndex = iOUT((i * 3) + 1, pd, sw->GenOutput);
        p[iOUTIndex] = vo->minLyrTemperature[i];

		iOUTIndex = iOUT((i * 3) + 2, pd, sw->GenOutput);
        p[iOUTIndex] = vo->avgLyrTemp[i];
	}
}

#elif defined(STEPWAT)

/**
@brief Gets soil temperature for when dealing with STEPWAT.

@param[in] pd Time period in simulation output (day/week/month/year)
@param[in] sw Comprehensive struct of type SW_ALL containing all information
  in the simulation.
*/
void get_soiltemp_agg(OutPeriod pd, SW_ALL* sw)
{
	LyrIndex i;
	SW_SOILWAT_OUTPUTS *vo = sw->SoilWat.p_oagg[pd];
	size_t iOUTIndex;

	RealD
		*p = sw->GenOutput.p_OUT[eSW_SoilTemp][pd],
		*psd = sw->GenOutput.p_OUTsd[eSW_SoilTemp][pd];

    ForEachSoilLayer(i, sw->Site.n_layers)
    {
		iOUTIndex = iOUT((i * 3), pd, sw->GenOutput);
        do_running_agg(p, psd, iOUTIndex, sw->GenOutput.currIter,
					   vo->maxLyrTemperature[i]);

		iOUTIndex = iOUT((i * 3) + 1, pd, sw->GenOutput);
        do_running_agg(p, psd, iOUTIndex, sw->GenOutput.currIter,
					   vo->minLyrTemperature[i]);

		iOUTIndex = iOUT((i * 3) + 2, pd, sw->GenOutput);
        do_running_agg(p, psd, iOUTIndex, sw->GenOutput.currIter,
					   vo->avgLyrTemp[i]);
    }

	if (sw->GenOutput.print_IterationSummary) {
		sw->GenOutput.sw_outstr_agg[0] = '\0';
		format_IterationSummary(p, psd, pd, sw->GenOutput.ncol_OUT[eSW_SoilTemp], sw);
	}
}
#endif

//------ eSW_Frozen
#ifdef SW_OUTTEXT

/**
@brief Gets state (frozen/unfrozen) for each layer for when dealing with OUTTEXT.

@param[in] pd Time period in simulation output (day/week/month/year)
@param[in] sw Comprehensive struct of type SW_ALL containing all information
  in the simulation.
*/
void get_frozen_text(OutPeriod pd, SW_ALL* sw)
{
    LyrIndex i;
    SW_SOILWAT_OUTPUTS *vo = sw->SoilWat.p_oagg[pd];

    char str[OUTSTRLEN];
    sw->GenOutput.sw_outstr[0] = '\0';

    ForEachSoilLayer(i, sw->Site.n_layers)
    {
        snprintf(str, OUTSTRLEN, "%c%.*f", _OUTSEP, OUT_DIGITS, vo->lyrFrozen[i]);
        strcat(sw->GenOutput.sw_outstr, str);
    }
}
#endif

#if defined(RSOILWAT) || defined(SWNETCDF)

/**
@brief Gets soil state (frozen/unfrozen) for when dealing with RSOILWAT.

@param[in] pd Time period in simulation output (day/week/month/year)
@param[in] sw Comprehensive struct of type SW_ALL containing all information
  in the simulation.
*/
void get_frozen_mem(OutPeriod pd, SW_ALL* sw)
{
    LyrIndex i;
    SW_SOILWAT_OUTPUTS *vo = sw->SoilWat.p_oagg[pd];
	size_t iOUTIndex;

    RealD *p = sw->GenOutput.p_OUT[eSW_Frozen][pd];

    #if defined(RSOILWAT)
    get_outvalleader(&sw->Model, pd, sw->GenOutput.irow_OUT,
					 sw->GenOutput.nrow_OUT, sw->GenOutput.tOffset, p);
    #endif

    ForEachSoilLayer(i, sw->Site.n_layers)
    {
		iOUTIndex = iOUT(i, pd, sw->GenOutput);
        p[iOUTIndex] = vo->lyrFrozen[i];
    }
}

#elif defined(STEPWAT)

/**
@brief Gets soil temperature for when dealing with STEPWAT.

@param[in] pd Time period in simulation output (day/week/month/year)
@param[in] sw Comprehensive struct of type SW_ALL containing all information
  in the simulation.
*/
void get_frozen_agg(OutPeriod pd, SW_ALL* sw)
{
    LyrIndex i;
    SW_SOILWAT_OUTPUTS *vo = sw->SoilWat.p_oagg[pd];
	size_t iOUTIndex;

    RealD
        *p = sw->GenOutput.p_OUT[eSW_Frozen][pd],
        *psd = sw->GenOutput.p_OUTsd[eSW_Frozen][pd];

    ForEachSoilLayer(i, sw->Site.n_layers)
    {
		iOUTIndex = iOUT(i, pd, sw->GenOutput);
        do_running_agg(p, psd, iOUTIndex, sw->GenOutput.currIter,
					   vo->lyrFrozen[i]);
    }

    if (sw->GenOutput.print_IterationSummary) {
        sw->GenOutput.sw_outstr_agg[0] = '\0';
        format_IterationSummary(p, psd, pd, sw->GenOutput.ncol_OUT[eSW_Frozen], sw);
    }
}
#endif
