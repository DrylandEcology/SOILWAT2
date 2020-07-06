/********************************************************/
/********************************************************/
/**
	@file
	@brief All content is for mock purposes only and to reduce compile warnings

	Our unit testing framework `googletest` is c++ which doesn't like some of
	the constructs we use in the output code. Thus, we currently cannot test
	any output code and use these mock functions instead as placeholders.
*/
/********************************************************/
/********************************************************/


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
#include "SW_Output.h"
#include "SW_Weather.h"
#include "SW_VegEstab.h"
#include "SW_VegProd.h"

// Global Variables
extern SW_SITE SW_Site;
extern SW_SOILWAT SW_Soilwat;
extern SW_MODEL SW_Model;
extern SW_WEATHER SW_Weather;
extern SW_VEGPROD SW_VegProd;
extern SW_VEGESTAB SW_VegEstab;
extern Bool EchoInits;
extern SW_CARBON SW_Carbon;

// Copy-paste from SW_Output.c (lines 81-103)
SW_OUTPUT SW_Output[SW_OUTNKEYS];

char _Sep; /* output delimiter */
TimeInt tOffset; /* 1 or 0 means we're writing previous or current period */


// Global variables describing output periods:
/** `timeSteps` is the array that keeps track of the output time periods that
    are required for `text` and/or `array`-based output for each output key. */
OutPeriod timeSteps[SW_OUTNKEYS][SW_OUTNPERIODS];
/** The number of different time steps/periods that are used/requested
		Note: Under STEPWAT2, this may be larger than the sum of `use_OutPeriod`
			because it also incorporates information from `timeSteps_SXW`. */
IntUS used_OUTNPERIODS;
/** TRUE if time step/period is active for any output key. */
Bool use_OutPeriod[SW_OUTNPERIODS];


// Global variables describing size and names of output
/** names of output columns for each output key; number is an expensive guess */
char *colnames_OUT[SW_OUTNKEYS][5 * NVEGTYPES + MAX_LAYERS];
/** number of output columns for each output key */
IntUS ncol_OUT[SW_OUTNKEYS];



// Mock definitions
char const *key2str[] = {"SW_MISSING"};
char const *pd2longstr[] = {"SW_MISSING"};



/* =================================================== */
/*             Public Function Definitions             */
/*             (declared in SW_Output.h)               */
/* --------------------------------------------------- */
void SW_OUT_set_colnames(void)
{}

void SW_OUT_set_ncol(void)
{}

void SW_OUT_construct(void)
{}

void SW_OUT_deconstruct(Bool full_reset)
{
	if (full_reset) {}
}

void SW_OUT_new_year(void)
{}

void SW_OUT_read(void)
{}

/**
@brief This is a blank function.
*/
void _collect_values(void)
{}

void SW_OUT_flush(void)
{
	_collect_values();
}

void SW_OUT_sum_today(ObjType otyp)
{
	ObjType x = otyp;
	if (x == eF) {}
}

void SW_OUT_write_today(void)
{}

void get_none(OutPeriod pd)
{
	if (pd) {}
}

void get_co2effects_text(OutPeriod pd)
{
	if (pd) {}
}

void get_biomass_text(OutPeriod pd)
{
	if (pd) {}
}

void get_estab_text(OutPeriod pd)
{
	if (pd) {}
}

void get_temp_text(OutPeriod pd)
{
	if (pd) {}
}

void get_precip_text(OutPeriod pd)
{
	if (pd) {}
}

void get_vwcBulk_text(OutPeriod pd)
{
	if (pd) {}
}

void get_vwcMatric_text(OutPeriod pd)
{
	if (pd) {}
}

void get_swcBulk_text(OutPeriod pd)
{
	if (pd) {}
}

void get_swpMatric_text(OutPeriod pd)
{
	if (pd) {}
}

void get_swaBulk_text(OutPeriod pd)
{
	if (pd) {}
}

void get_swaMatric_text(OutPeriod pd)
{
	if (pd) {}
}

void get_surfaceWater_text(OutPeriod pd)
{
	if (pd) {}
}

void get_runoffrunon_text(OutPeriod pd)
{
	if (pd) {}
}

void get_transp_text(OutPeriod pd)
{
	if (pd) {}
}

void get_evapSoil_text(OutPeriod pd)
{
	if (pd) {}
}

void get_evapSurface_text(OutPeriod pd)
{
	if (pd) {}
}

void get_interception_text(OutPeriod pd)
{
	if (pd) {}
}

void get_soilinf_text(OutPeriod pd)
{
	if (pd) {}
}

void get_lyrdrain_text(OutPeriod pd)
{
	if (pd) {}
}

void get_hydred_text(OutPeriod pd)
{
	if (pd) {}
}

void get_aet_text(OutPeriod pd)
{
	if (pd) {}
}

void get_pet_text(OutPeriod pd)
{
	if (pd) {}
}

void get_wetdays_text(OutPeriod pd)
{
	if (pd) {}
}

void get_snowpack_text(OutPeriod pd)
{
	if (pd) {}
}

void get_deepswc_text(OutPeriod pd)
{
	if (pd) {}
}

void get_soiltemp_text(OutPeriod pd)
{
	if (pd) {}
}

static void sumof_vpd(SW_VEGPROD *v, SW_VEGPROD_OUTPUTS *s, OutKey k)
{
	OutKey x = k;
	if ((int)x == 1) {}

	if (EQ(0., v->bare_cov.fCover)) {}
	if (EQ(0., s->veg[SW_GRASS].biomass)) {}
}

static void sumof_ves(SW_VEGESTAB *v, SW_VEGESTAB_OUTPUTS *s, OutKey k)
{
	if ((int)k == 1) {}
	if (0 == v->count) {}
	if (0 == s->days) {}
}


static void sumof_wth(SW_WEATHER *v, SW_WEATHER_OUTPUTS *s, OutKey k)
{
	OutKey x = k;
	if ((int)x == 1) {}

	if (EQ(0., v->pct_snowdrift)) {}
	if (EQ(0., s->temp_max)) {}
}


static void sumof_swc(SW_SOILWAT *v, SW_SOILWAT_OUTPUTS *s, OutKey k)
{
	OutKey x = k;
	if ((int)x == 1) {}

	if (EQ(0., v->snowdepth)) {}
	if (EQ(0., s->snowdepth)) {}
}


static void average_for(ObjType otyp, OutPeriod pd)
{
	if (pd == eSW_Day) {}
	SW_OUT_sum_today(otyp);
}

static void collect_sums(ObjType otyp, OutPeriod op)
{
	if (op == eSW_Day) {}
	SW_OUT_sum_today(otyp);
}

/**
@brief Runs get commands for each eSW_Year.
*/
void _echo_outputs(void)
{
	OutPeriod pd = eSW_Year;

	get_none(pd);
	get_estab_text(pd);
	get_temp_text(pd);
	get_precip_text(pd);
	get_vwcBulk_text(pd);
	get_vwcMatric_text(pd);
	get_swcBulk_text(pd);
	get_swpMatric_text(pd);
	get_swaBulk_text(pd);
	get_swaMatric_text(pd);
	get_surfaceWater_text(pd);
	get_runoffrunon_text(pd);
	get_transp_text(pd);
	get_evapSoil_text(pd);
	get_evapSurface_text(pd);
	get_interception_text(pd);
	get_soilinf_text(pd);
	get_lyrdrain_text(pd);
	get_hydred_text(pd);
	get_aet_text(pd);
	get_pet_text(pd);
	get_wetdays_text(pd);
	get_snowpack_text(pd);
	get_deepswc_text(pd);
	get_soiltemp_text(pd);
	get_co2effects_text(pd);
	get_biomass_text(pd);

	OutKey k = eSW_NoKey;
	SW_VEGPROD *vveg = NULL;
	SW_VEGPROD_OUTPUTS *sveg = NULL;
	SW_VEGESTAB *vestab = NULL;
	SW_VEGESTAB_OUTPUTS *sestab = NULL;
	SW_WEATHER *vweath = NULL;
	SW_WEATHER_OUTPUTS *sweath = NULL;
	SW_SOILWAT *vswc = NULL;
	SW_SOILWAT_OUTPUTS *sswc = NULL;

	sumof_vpd(vveg, sveg, k);
	sumof_ves(vestab, sestab, k);
	sumof_wth(vweath, sweath, k);
	sumof_swc(vswc, sswc, k);

	ObjType otyp = eF;

	average_for(otyp, pd);
	collect_sums(otyp, pd);
}


Bool has_OutPeriod_inUse(OutPeriod pd, OutKey k)
{
	Bool res = swTRUE;

	if (k) {}
	if (pd) {}

	return res;
}
