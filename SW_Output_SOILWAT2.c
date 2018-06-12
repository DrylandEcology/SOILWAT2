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

// defined in `SW_Output_core.c`:
extern SW_OUTPUT SW_Output[SW_OUTNKEYS];
extern SW_FILE_STATUS SW_OutFiles;

extern char _Sep;
extern TimeInt tOffset;
extern Bool bFlush_output;
extern char sw_outstr[OUTSTRLEN];
extern Bool use_OutPeriod[SW_OUTNPERIODS];


/* =================================================== */
/* =================================================== */
/*             Private Functions                       */
/* --------------------------------------------------- */

static void _create_csv_files(OutPeriod pd);

/**
  \fn static void _create_csv_files(OutPeriod pd)

  Creates `csv` output files for specified time step

  \param pd. The output time step.
*/
/***********************************************************/
static void _create_csv_files(OutPeriod pd)
{
	// PROGRAMMER Note: `eOutputDaily + pd` is not very elegant and assumes
	// a specific order of `SW_FileIndex` --> fix and create something that
	// allows subsetting such as `eOutputFile[pd]` or append time period to
	// a basename, etc.

	if (SW_OutFiles.make_regular) {
		SW_OutFiles.fp_reg[pd] = OpenFile(SW_F_name(eOutputDaily + pd), "w");
	}

	if (SW_OutFiles.make_soil) {
		SW_OutFiles.fp_soil[pd] = OpenFile(SW_F_name(eOutputDaily_soil + pd), "w");
	}
}




/* =================================================== */
/* =================================================== */
/*             Function Definitions                    */
/*             (declared in SW_Output.h)               */
/* --------------------------------------------------- */

/** create all of the user-specified output files.
    call this routine at the beginning of the main program run, but
    after `SW_OUT_read` which sets the global variable `use_OutPeriod`.
*/
void SW_OUT_create_files(void) {
	OutPeriod p;

	ForEachOutPeriod(p) {
		if (use_OutPeriod[p]) {
			_create_csv_files(p);

			write_headers_to_csv(p, SW_OutFiles.fp_reg[p], SW_OutFiles.fp_soil[p], swFALSE);
		}
	}
}


void get_co2effects(OutPeriod pd) {
	int k;
	RealD biomass_total = 0., biolive_total = 0.;
	SW_VEGPROD *v = &SW_VegProd;
	SW_VEGPROD_OUTPUTS *vo = NULL;

	set_VEGPROD_aggslot(pd, &vo);

	ForEachVegType(k)
	{
		biomass_total += vo->veg[k].biomass;
		biolive_total += vo->veg[k].biolive;
	}

	sw_outstr[0] = '\0';
	// NOTE: `get_co2effects` uses a different order of vegetation types than the rest of SoilWat!!!
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
	char str[OUTSTRLEN];

	i = (IntU) pd; // silence `-Wunused-parameter`

	sw_outstr[0] = '\0';
	for (i = 0; i < v->count; i++)
	{
		sprintf(str, "%c%d", _Sep, v->parms[i]->estab_doy);
		strcat(sw_outstr, str);
	}
}

void get_temp(OutPeriod pd)
{
	SW_WEATHER_OUTPUTS *vo = NULL;

  #ifdef SWDEBUG
  int debug = 0;
  #endif

  #ifdef SWDEBUG
  if (debug) swprintf("'get_temp': start for %d ... ", pd);
  #endif

	set_WEATHER_aggslot(pd, &vo);

	sw_outstr[0] = '\0';
	sprintf(sw_outstr, "%c%.*f%c%.*f%c%.*f%c%.*f",
		_Sep, OUT_DIGITS, vo->temp_max,
		_Sep, OUT_DIGITS, vo->temp_min,
		_Sep, OUT_DIGITS, vo->temp_avg,
		_Sep, OUT_DIGITS, vo->surfaceTemp);

	#ifdef SWDEBUG
		if (debug) swprintf("completed\n");
	#endif
}

void get_precip(OutPeriod pd)
{
	SW_WEATHER_OUTPUTS *vo = NULL;

	set_WEATHER_aggslot(pd, &vo);

	sw_outstr[0] = '\0';
	sprintf(sw_outstr, "%c%.*f%c%.*f%c%.*f%c%.*f%c%.*f",
		_Sep, OUT_DIGITS, vo->ppt,
		_Sep, OUT_DIGITS, vo->rain,
		_Sep, OUT_DIGITS, vo->snow,
		_Sep, OUT_DIGITS, vo->snowmelt,
		_Sep, OUT_DIGITS, vo->snowloss);
}

void get_vwcBulk(OutPeriod pd)
{
	LyrIndex i;
	SW_SOILWAT_OUTPUTS *vo = NULL;
	char str[OUTSTRLEN];

	set_SOILWAT_aggslot(pd, &vo);

	sw_outstr[0] = '\0';

	ForEachSoilLayer(i) {
		/* vwcBulk at this point is identical to swcBulk */
		sprintf(str, "%c%.*f",
			_Sep, OUT_DIGITS, vo->vwcBulk[i] / SW_Site.lyr[i]->width);
		strcat(sw_outstr, str);
	}
}

void get_vwcMatric(OutPeriod pd)
{
	LyrIndex i;
	RealD convert;
	SW_SOILWAT_OUTPUTS *vo = NULL;
	char str[OUTSTRLEN];

	set_SOILWAT_aggslot(pd, &vo);

	sw_outstr[0] = '\0';

	ForEachSoilLayer(i) {
		/* vwcMatric at this point is identical to swcBulk */
		convert = 1. / (1. - SW_Site.lyr[i]->fractionVolBulk_gravel) / SW_Site.lyr[i]->width;
		sprintf(str, "%c%.*f",
			_Sep, OUT_DIGITS, vo->vwcMatric[i] * convert);
		strcat(sw_outstr, str);
	}
}

void get_swa(OutPeriod pd)
{
	/* added 21-Oct-03, cwb */
	LyrIndex i;
	int k;
	SW_SOILWAT_OUTPUTS *vo = NULL;
	char str[OUTSTRLEN];

	set_SOILWAT_aggslot(pd, &vo);

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


void get_swcBulk(OutPeriod pd)
{
	/* added 21-Oct-03, cwb */
	LyrIndex i;
	SW_SOILWAT_OUTPUTS *vo = NULL;
	char str[OUTSTRLEN];

	set_SOILWAT_aggslot(pd, &vo);

	sw_outstr[0] = '\0';

	ForEachSoilLayer(i)
	{
		sprintf(str, "%c%.*f", _Sep, OUT_DIGITS, vo->swcBulk[i]);
		strcat(sw_outstr, str);
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
	char str[OUTSTRLEN];

	set_SOILWAT_aggslot(pd, &vo);

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

void get_swaBulk(OutPeriod pd)
{

	LyrIndex i;
	SW_SOILWAT_OUTPUTS *vo = NULL;
	char str[OUTSTRLEN];

	set_SOILWAT_aggslot(pd, &vo);

	sw_outstr[0] = '\0';

	ForEachSoilLayer(i)
	{
		sprintf(str, "%c%.*f", _Sep, OUT_DIGITS, vo->swaBulk[i]);
		strcat(sw_outstr, str);
	}
}

void get_swaMatric(OutPeriod pd)
{
	LyrIndex i;
	RealD convert;
	SW_SOILWAT_OUTPUTS *vo = NULL;
	char str[OUTSTRLEN];

	set_SOILWAT_aggslot(pd, &vo);

	sw_outstr[0] = '\0';

	ForEachSoilLayer(i)
	{
		/* swaMatric at this point is identical to swaBulk */
		convert = 1. / (1. - SW_Site.lyr[i]->fractionVolBulk_gravel);

		sprintf(str, "%c%.*f", _Sep, OUT_DIGITS, vo->swaMatric[i] * convert);
		strcat(sw_outstr, str);
	}
}

void get_surfaceWater(OutPeriod pd)
{
	SW_SOILWAT_OUTPUTS *vo = NULL;

	set_SOILWAT_aggslot(pd, &vo);

	sw_outstr[0] = '\0';
	sprintf(sw_outstr, "%c%.*f", _Sep, OUT_DIGITS, vo->surfaceWater);
}

void get_runoffrunon(OutPeriod pd) {
	SW_WEATHER_OUTPUTS *vo = NULL;

	set_WEATHER_aggslot(pd, &vo);

	sw_outstr[0] = '\0';
	sprintf(sw_outstr, "%c%.*f%c%.*f%c%.*f%c%.*f",
		_Sep, OUT_DIGITS, vo->surfaceRunoff + vo->snowRunoff - vo->surfaceRunon,
		_Sep, OUT_DIGITS, vo->surfaceRunoff,
		_Sep, OUT_DIGITS, vo->snowRunoff,
		_Sep, OUT_DIGITS, vo->surfaceRunon);
}

void get_transp(OutPeriod pd)
{
	LyrIndex i;
	int k;
	SW_SOILWAT_OUTPUTS *vo = NULL;
	char str[OUTSTRLEN];

	set_SOILWAT_aggslot(pd, &vo);

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


void get_evapSoil(OutPeriod pd)
{
	LyrIndex i;
	SW_SOILWAT_OUTPUTS *vo = NULL;
	char str[OUTSTRLEN];

	set_SOILWAT_aggslot(pd, &vo);

	sw_outstr[0] = '\0';

	ForEachEvapLayer(i)
	{
		sprintf(str, "%c%.*f", _Sep, OUT_DIGITS, vo->evap[i]);
		strcat(sw_outstr, str);
	}
}

void get_evapSurface(OutPeriod pd)
{
	int k;
	SW_SOILWAT_OUTPUTS *vo = NULL;
	char str[OUTSTRLEN];

	set_SOILWAT_aggslot(pd, &vo);

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

void get_interception(OutPeriod pd)
{
	int k;
	SW_SOILWAT_OUTPUTS *vo = NULL;
	char str[OUTSTRLEN];

	set_SOILWAT_aggslot(pd, &vo);

	sw_outstr[0] = '\0';
	sprintf(sw_outstr, "%c%.*f", _Sep, OUT_DIGITS, vo->total_int);

	ForEachVegType(k) {
		sprintf(str, "%c%.*f", _Sep, OUT_DIGITS, vo->int_veg[k]);
		strcat(sw_outstr, str);
	}

	sprintf(str, "%c%.*f", _Sep, OUT_DIGITS, vo->litter_int);
	strcat(sw_outstr, str);
}

void get_soilinf(OutPeriod pd)
{
	/* 20100202 (drs) added */
	/* 20110219 (drs) added runoff */
	/* 12/13/2012	(clk)	moved runoff, now named snowRunoff, to get_runoffrunon(); */
	SW_WEATHER_OUTPUTS *vo = NULL;

	set_WEATHER_aggslot(pd, &vo);

	sw_outstr[0] = '\0';
	sprintf(sw_outstr, "%c%.*f", _Sep, OUT_DIGITS, vo->soil_inf);
}

void get_lyrdrain(OutPeriod pd)
{
	/* 20100202 (drs) added */
	LyrIndex i;
	SW_SOILWAT_OUTPUTS *vo = NULL;
	char str[OUTSTRLEN];

	set_SOILWAT_aggslot(pd, &vo);

	sw_outstr[0] = '\0';

	for (i = 0; i < SW_Site.n_layers - 1; i++)
	{
		sprintf(str, "%c%.*f", _Sep, OUT_DIGITS, vo->lyrdrain[i]);
		strcat(sw_outstr, str);
	}
}

void get_hydred(OutPeriod pd)
{
	/* 20101020 (drs) added */
	LyrIndex i;
	int k;
	SW_SOILWAT_OUTPUTS *vo = NULL;
	char str[OUTSTRLEN];

	set_SOILWAT_aggslot(pd, &vo);

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

void get_aet(OutPeriod pd)
{
	SW_SOILWAT_OUTPUTS *vo = NULL;

	set_SOILWAT_aggslot(pd, &vo);

	sw_outstr[0] = '\0';
	sprintf(sw_outstr, "%c%.*f", _Sep, OUT_DIGITS, vo->aet);
}

void get_pet(OutPeriod pd)
{
	SW_SOILWAT_OUTPUTS *vo = NULL;

	set_SOILWAT_aggslot(pd, &vo);

	sw_outstr[0] = '\0';
	sprintf(sw_outstr, "%c%.*f", _Sep, OUT_DIGITS, vo->pet);
}

void get_wetdays(OutPeriod pd)
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
		SW_SOILWAT_OUTPUTS *vo = NULL;
		set_SOILWAT_aggslot(pd, &vo);

		ForEachSoilLayer(i) {
			sprintf(str, "%c%i", _Sep, (int) vo->wetdays[i]);
			strcat(sw_outstr, str);
		}
	}

}

void get_snowpack(OutPeriod pd)
{
	SW_SOILWAT_OUTPUTS *vo = NULL;

	set_SOILWAT_aggslot(pd, &vo);

	sw_outstr[0] = '\0';
	sprintf(sw_outstr, "%c%.*f%c%.*f",
		_Sep, OUT_DIGITS, vo->snowpack,
		_Sep, OUT_DIGITS, vo->snowdepth);
}

void get_deepswc(OutPeriod pd)
{
	SW_SOILWAT_OUTPUTS *vo = NULL;

	set_SOILWAT_aggslot(pd, &vo);

	sw_outstr[0] = '\0';
	sprintf(sw_outstr, "%c%.*f", _Sep, OUT_DIGITS, vo->deep);
}

void get_soiltemp(OutPeriod pd)
{
	LyrIndex i;
	SW_SOILWAT_OUTPUTS *vo = NULL;
	char str[OUTSTRLEN];

	set_SOILWAT_aggslot(pd, &vo);

	sw_outstr[0] = '\0';

	ForEachSoilLayer(i)
	{
		sprintf(str, "%c%.*f", _Sep, OUT_DIGITS, vo->sTemp[i]);
		strcat(sw_outstr, str);
	}
}
