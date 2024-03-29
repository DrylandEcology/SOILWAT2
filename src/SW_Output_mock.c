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
#include "include/filefuncs.h"
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
	IntUS ncol_OUT[], char *colnames_OUT[][5 * NVEGTYPES + MAX_LAYERS],
	LOG_INFO* LogInfo)
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

void SW_OUT_init_ptrs(SW_ALL* sw) {
	(void) sw;
}

void SW_OUT_construct(Bool make_soil[], Bool make_regular[],
		SW_OUTPUT_POINTERS* SW_OutputPtrs, SW_OUTPUT* SW_Output,
		LyrIndex n_layers, SW_GEN_OUT *GenOutput)
{
	/* Silence compiler */
	(void) make_soil;
	(void) make_regular;
	(void) SW_OutputPtrs;
	(void) SW_Output;
	(void) n_layers;
	(void) GenOutput;
}

void SW_OUT_deconstruct(Bool full_reset, SW_ALL *sw)
{
	if (full_reset) {}
	(void) sw;
}

void SW_OUT_new_year(TimeInt firstdoy, TimeInt lastdoy,
					 SW_OUTPUT* SW_Output)
{
	/* silence compiler warnings */
	if(EQ(0, firstdoy)) {}
	if(EQ(0, lastdoy)) {}

	(void) SW_Output;
}

void SW_OUT_read(SW_ALL* sw, char *InFiles[],
	OutPeriod timeSteps[][SW_OUTNPERIODS], IntUS *used_OUTNPERIODS,
	LOG_INFO* LogInfo)
{
	/* use sw to silence compiler warnings */
	(void) sw;
	(void) LogInfo;
	(void) InFiles;
	(void) timeSteps;
	(void) used_OUTNPERIODS;
}

void _collect_values(SW_ALL* sw, SW_OUTPUT_POINTERS* SW_OutputPtrs,
		Bool bFlush_output, TimeInt tOffset, LOG_INFO* LogInfo)
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

	_collect_values(sw, SW_OutputPtrs, swFALSE, tOffset, LogInfo);
}

void SW_OUT_sum_today(SW_ALL* sw, ObjType otyp,
		Bool bFlush_output, TimeInt tOffset, LOG_INFO* LogInfo)
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
						Bool bFlush_output, TimeInt tOffset)
{
	/* silence compiler warnings */
	(void) sw;
	(void) SW_OutputPtrs;
	(void) bFlush_output;
	(void) tOffset;
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


static void average_for(SW_ALL* sw, ObjType otyp, OutPeriod pd,
		Bool bFlush_output, TimeInt tOffset, LOG_INFO* LogInfo)
{
	if (pd == eSW_Day) {}
	SW_OUT_sum_today(sw, otyp, bFlush_output, tOffset, LogInfo);
}

static void collect_sums(SW_ALL* sw, ObjType otyp, OutPeriod op,
	OutPeriod timeSteps[][SW_OUTNPERIODS], IntUS used_OUTNPERIODS,
	LOG_INFO* LogInfo)
{
	TimeInt tOffset = 0;

	if (op == eSW_Day) {}
	SW_OUT_sum_today(sw, otyp, swFALSE, tOffset, LogInfo);

	(void) timeSteps;
	(void) used_OUTNPERIODS;
	(void) LogInfo;
}

/**
	@brief Runs get commands for each eSW_Year.

	@param sw Comprehensive structure holding all information
		dealt with in SOILWAT2
*/
void _echo_outputs(SW_ALL* sw)
{
	sumof_vpd(NULL, NULL, eSW_NoKey, sw->Model.doy);
	sumof_ves(NULL, NULL, eSW_NoKey);
	sumof_wth(NULL, NULL, eSW_NoKey);
	sumof_swc(NULL, NULL, eSW_NoKey);

	average_for(sw, eF, eSW_Year, swFALSE, sw->GenOutput.tOffset, NULL);

	collect_sums(sw, eF, eSW_Year, sw->GenOutput.timeSteps,
				 sw->GenOutput.used_OUTNPERIODS, NULL);
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
