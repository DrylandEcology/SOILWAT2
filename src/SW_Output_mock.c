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
#include "include/generic.h"
#include "include/filefuncs.h" // externs `_firstfile`
#include "include/myMemory.h"
#include "include/Times.h"

#include "include/SW_Carbon.h"
#include "include/SW_Defines.h"
#include "include/SW_Files.h"
#include "include/SW_Model.h"
#include "include/SW_Site.h"
#include "include/SW_SoilWater.h"
#include "include/Times.h"
#include "include/SW_Output.h"
#include "include/SW_Weather.h"
#include "include/SW_VegEstab.h"



/* =================================================== */
/*                  Global Variables                   */
/* --------------------------------------------------- */

// Mock definitions
char const *key2str[] = {"SW_MISSING"};
char const *pd2longstr[] = {"SW_MISSING"};



/* =================================================== */
/*             Global Function Definitions             */
/*             (declared in SW_Output.h)               */
/* --------------------------------------------------- */
void SW_OUT_set_colnames(int tLayers, SW_VEGESTAB_INFO** parms,
	LOG_INFO* LogInfo, IntUS ncol_OUT[],
	char *colnames_OUT[][5 * NVEGTYPES + MAX_LAYERS])
{
	/* Silence compiler */
	(void) tLayers;
	(void) parms;
	(void) LogInfo;
	(void) ncol_OUT;
	(void) colnames_OUT;
}

void SW_OUT_set_ncol(int tLayers, int n_evap_lyrs, int count,
					 IntUS ncol_OUT[])
{
	/* Silence compiler */
	(void) tLayers;
	(void) n_evap_lyrs;
	(void) count;
	(void) ncol_OUT;
}

void SW_OUT_construct(Bool make_soil[], Bool make_regular[],
		SW_OUTPUT_POINTERS* SW_OutputPtrs, SW_OUTPUT* SW_Output,
		LyrIndex n_layers, TimeInt *tOffset,
		OutPeriod timeSteps[][SW_OUTNPERIODS])
{
	/* Silence compiler */
	(void) make_soil;
	(void) make_regular;
	(void) SW_OutputPtrs;
	(void) SW_Output;
	(void) n_layers;
	(void) timeSteps;
	(void) tOffset;
}

void SW_OUT_deconstruct(Bool full_reset,
						char *colnames_OUT[][5 * NVEGTYPES + MAX_LAYERS])
{
	if (full_reset) {}
	(void) colnames_OUT;
}

void SW_OUT_new_year(TimeInt firstdoy, TimeInt lastdoy,
					 SW_OUTPUT* SW_Output)
{
	/* silence compiler warnings */
	if(EQ(0, firstdoy)) {}
	if(EQ(0, lastdoy)) {}

	(void) SW_Output;
}

void SW_OUT_read(SW_ALL* sw, LOG_INFO* LogInfo, char *InFiles[],
	OutPeriod timeSteps[][SW_OUTNPERIODS], IntUS *used_OUTNPERIODS)
{
	/* use sw to silence compiler warnings */
	(void) sw;
	(void) LogInfo;
	(void) InFiles;
	(void) timeSteps;
	(void) used_OUTNPERIODS;
}

void _collect_values(SW_ALL* sw, SW_OUTPUT_POINTERS* SW_OutputPtrs,
		LOG_INFO* LogInfo, Bool bFlush_output, TimeInt tOffset)
{
	/* silence compiler warnings */
	(void) sw;
	(void) SW_OutputPtrs;
	(void) LogInfo;
	(void) bFlush_output;
	(void) tOffset;
}

void SW_OUT_flush(SW_ALL* sw, SW_OUTPUT_POINTERS* SW_OutputPtrs,
				  LOG_INFO* LogInfo)
{
	TimeInt tOffset = 0;

	_collect_values(sw, SW_OutputPtrs, LogInfo, swFALSE, tOffset);
}

void SW_OUT_sum_today(SW_ALL* sw, LOG_INFO* LogInfo, ObjType otyp,
		Bool bFlush_output, TimeInt tOffset)
{
	ObjType x = otyp;
	if (x == eF) {}

	/* silence compiler warnings */
	(void) sw;
	(void) LogInfo;
	(void) bFlush_output;
	(void) tOffset;
}

void SW_OUT_write_today(SW_ALL* sw, SW_OUTPUT_POINTERS* SW_OutputPtrs,
						Bool bFlush_output)
{
	/* silence compiler warnings */
	(void) sw;
	(void) SW_OutputPtrs;
	(void) bFlush_output;
}

void get_none(OutPeriod pd, SW_ALL* sw)
{
	if (pd) {}
	(void) sw; // use sw to silence compiler warnings
}

void get_co2effects_text(OutPeriod pd, SW_ALL* sw)
{
	if (pd) {}
	(void) sw; // use sw to silence compiler warnings
}

void get_biomass_text(OutPeriod pd, SW_ALL* sw)
{
	if (pd) {}
	(void) sw; // use sw to silence compiler warnings
}

void get_estab_text(OutPeriod pd, SW_ALL* sw)
{
	if (pd) {}
	(void) sw; // use sw to silence compiler warnings
}

void get_temp_text(OutPeriod pd, SW_ALL* sw)
{
	if (pd) {}
	(void) sw; // use sw to silence compiler warnings
}

void get_precip_text(OutPeriod pd, SW_ALL* sw)
{
	if (pd) {}
	(void) sw; // use sw to silence compiler warnings
}

void get_vwcBulk_text(OutPeriod pd, SW_ALL* sw)
{
	if (pd) {}
	(void) sw; // use sw to silence compiler warnings
}

void get_vwcMatric_text(OutPeriod pd, SW_ALL* sw)
{
	if (pd) {}
	(void) sw; // use sw to silence compiler warnings
}

void get_swcBulk_text(OutPeriod pd, SW_ALL* sw)
{
	if (pd) {}
	(void) sw; // use sw to silence compiler warnings
}

void get_swpMatric_text(OutPeriod pd, SW_ALL* sw)
{
	if (pd) {}
	(void) sw; // use sw to silence compiler warnings
}

void get_swaBulk_text(OutPeriod pd, SW_ALL* sw)
{
	if (pd) {}
	(void) sw; // use sw to silence compiler warnings
}

void get_swaMatric_text(OutPeriod pd, SW_ALL* sw)
{
	if (pd) {}
	(void) sw; // use sw to silence compiler warnings
}

void get_surfaceWater_text(OutPeriod pd, SW_ALL* sw)
{
	if (pd) {}
	(void) sw; // use sw to silence compiler warnings
}

void get_runoffrunon_text(OutPeriod pd, SW_ALL* sw)
{
	if (pd) {}
	(void) sw; // use sw to silence compiler warnings
}

void get_transp_text(OutPeriod pd, SW_ALL* sw)
{
	if (pd) {}
	(void) sw; // use sw to silence compiler warnings
}

void get_evapSoil_text(OutPeriod pd, SW_ALL* sw)
{
	if (pd) {}
	(void) sw; // use sw to silence compiler warnings
}

void get_evapSurface_text(OutPeriod pd, SW_ALL* sw)
{
	if (pd) {}
	(void) sw; // use sw to silence compiler warnings
}

void get_interception_text(OutPeriod pd, SW_ALL* sw)
{
	if (pd) {}
	(void) sw; // use sw to silence compiler warnings
}

void get_soilinf_text(OutPeriod pd, SW_ALL* sw)
{
	if (pd) {}
	(void) sw; // use sw to silence compiler warnings
}

void get_lyrdrain_text(OutPeriod pd, SW_ALL* sw)
{
	if (pd) {}
	(void) sw; // use sw to silence compiler warnings
}

void get_hydred_text(OutPeriod pd, SW_ALL* sw)
{
	if (pd) {}
	(void) sw; // use sw to silence compiler warnings
}

void get_aet_text(OutPeriod pd, SW_ALL* sw)
{
	if (pd) {}
	(void) sw; // use sw to silence compiler warnings
}

void get_pet_text(OutPeriod pd, SW_ALL* sw)
{
	if (pd) {}
	(void) sw; // use sw to silence compiler warnings
}

void get_wetdays_text(OutPeriod pd, SW_ALL* sw)
{
	if (pd) {}
	(void) sw; // use sw to silence compiler warnings
}

void get_snowpack_text(OutPeriod pd, SW_ALL* sw)
{
	if (pd) {}
	(void) sw; // use sw to silence compiler warnings
}

void get_deepswc_text(OutPeriod pd, SW_ALL* sw)
{
	if (pd) {}
	(void) sw; // use sw to silence compiler warnings
}

void get_soiltemp_text(OutPeriod pd, SW_ALL* sw)
{
	if (pd) {}
	(void) sw; // use sw to silence compiler warnings
}

void get_frozen_text(OutPeriod pd, SW_ALL* sw)
{
    if (pd) {}
	(void) sw; // use sw to silence compiler warnings
}

static void sumof_vpd(SW_VEGPROD *v, SW_VEGPROD_OUTPUTS *s, OutKey k, TimeInt doy)
{
	OutKey x = k;
	if ((int)x == 1) {}

	if (EQ(0., v->bare_cov.fCover)) {}
	if (EQ(0., s->veg[SW_GRASS].biomass_inveg)) {}
	if (EQ(0, doy)) {}
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


static void average_for(SW_ALL* sw, LOG_INFO* LogInfo,
		ObjType otyp, OutPeriod pd, Bool bFlush_output,
		TimeInt tOffset)
{
	if (pd == eSW_Day) {}
	SW_OUT_sum_today(sw, LogInfo, otyp, bFlush_output, tOffset);
}

static void collect_sums(SW_ALL* sw, ObjType otyp, OutPeriod op,
	LOG_INFO* LogInfo, OutPeriod timeSteps[][SW_OUTNPERIODS],
	IntUS used_OUTNPERIODS)
{
	TimeInt tOffset = 0;

	if (op == eSW_Day) {}
	SW_OUT_sum_today(sw, NULL, otyp, swFALSE, tOffset);

	(void) timeSteps;
	(void) used_OUTNPERIODS;
	(void) LogInfo;
}

/**
@brief Runs get commands for each eSW_Year.
*/
void _echo_outputs(SW_ALL* sw, LOG_INFO* LogInfo)
{
	OutPeriod pd = eSW_Year;

	get_none(pd, sw);
	get_estab_text(pd, sw);
	get_temp_text(pd, sw);
	get_precip_text(pd, sw);
	get_vwcBulk_text(pd, sw);
	get_vwcMatric_text(pd, sw);
	get_swcBulk_text(pd, sw);
	get_swpMatric_text(pd, sw);
	get_swaBulk_text(pd, sw);
	get_swaMatric_text(pd, sw);
	get_surfaceWater_text(pd, sw);
	get_runoffrunon_text(pd, sw);
	get_transp_text(pd, sw);
	get_evapSoil_text(pd, sw);
	get_evapSurface_text(pd, sw);
	get_interception_text(pd, sw);
	get_soilinf_text(pd, sw);
	get_lyrdrain_text(pd, sw);
	get_hydred_text(pd, sw);
	get_aet_text(pd, sw);
	get_pet_text(pd, sw);
	get_wetdays_text(pd, sw);
	get_snowpack_text(pd, sw);
	get_deepswc_text(pd, sw);
	get_soiltemp_text(pd, sw);
    get_frozen_text(pd, sw);
	get_co2effects_text(pd, sw);
	get_biomass_text(pd, sw);

	OutKey k = eSW_NoKey;
	SW_VEGPROD *vveg = NULL;
	SW_VEGPROD_OUTPUTS *sveg = NULL;
	SW_VEGESTAB *vestab = NULL;
	SW_VEGESTAB_OUTPUTS *sestab = NULL;
	SW_WEATHER *vweath = NULL;
	SW_WEATHER_OUTPUTS *sweath = NULL;
	SW_SOILWAT *vswc = NULL;
	SW_SOILWAT_OUTPUTS *sswc = NULL;

	sumof_vpd(vveg, sveg, k, sw->Model.doy);
	sumof_ves(vestab, sestab, k);
	sumof_wth(vweath, sweath, k);
	sumof_swc(vswc, sswc, k);

	ObjType otyp = eF;

	average_for(sw, LogInfo, otyp, pd, swFALSE, sw->GenOutput.tOffset);

	collect_sums(sw, otyp, pd, LogInfo, sw->GenOutput.timeSteps,
				 sw->GenOutput.used_OUTNPERIODS);
}


Bool has_OutPeriod_inUse(OutPeriod pd, OutKey k, IntUS used_OUTNPERIODS,
						 OutPeriod timeSteps[][SW_OUTNPERIODS])
{
	Bool res = swTRUE;

	if (k) {}
	if (pd) {}

	(void) used_OUTNPERIODS;
	(void) timeSteps;

	return res;
}
