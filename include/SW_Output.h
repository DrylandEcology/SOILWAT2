/********************************************************/
/********************************************************/
/*  Source file: SW_Output.h
	Type: header
	Application: SOILWAT - soilwater dynamics simulator
	Purpose: Support for SW_Output.c and SW_Output_get_functions.c
	History:
	(9/11/01) -- INITIAL CODING - cwb
	2010/02/02	(drs) changed SW_CANOPY to SW_CANOPYEV and SW_LITTER to SW_LITTEREV
			and eSW_Canopy to eSW_CanopyEv and eSW_Litter to eSW_LitterEv;
			added SWC_CANOPYINT, SW_LITTERINT, SW_SOILINF, SW_LYRDRAIN;
			updated SW_OUTNKEYS from 19 to 23;
			added eSW_CanopyInt, eSW_LitterInt, eSW_SoilInf, eSW_LyrDrain to enum OutKey;
	04/16/2010	(drs) added SW_SWA, updated SW_OUTNKEYS to 24, added eSW_SWA to enum OutKey
	10/20/2010	(drs) added SW_HYDRED, updated SW_OUTNKEYS to 25, added eSW_HydRed to enum OutKey
	02/19/2011	(drs) moved soil_inf output from swc- to weather-output: updated enum Outkey
	07/22/2011	(drs) added SW_SURFACEW and SW_EVAPSURFACE, updated SW_OUTNKEYS to 27, added eSW_SurfaceWater and eSW_EvapSurface to enum
	09/12/2011	(drs)	renamed SW_EVAP to SW_EVAPSOIL, and eSW_Evap to eSW_EvapSoil;
			deleted SW_CANOPYEV, eSW_CanopyEv, SW_LITTEREV, eSW_LitterEv, SW_CANOPYINT, eSW_CanopyInt, SW_LITTERINT, and eSW_LitterInt;
			added SW_INTERCEPTION and eSW_Interception
	05/25/2012	(DLM) added SW_SOILTEMP, updated SW_OUTNKEYs to 28, added eSW_SoilTemp to enum
	12/13/2012	(clk) added SW_RUNOFF, updated SW_OUTNKEYs to 29, added eSW_Runoff to enum
	12/14/2012	(drs) updated SW_OUTNKEYs from 29 to 26 [incorrect number probably introduced 09/12/2011]
	01/10/2013	(clk)	instead of using one FILE pointer named fp, created four new
			FILE pointers; fp_reg_agg[eSW_Day], fp_reg_agg[eSW_Week], fp_reg_agg[eSW_Month], and fp_reg_agg[eSW_Year]. This allows us to keep track
			of all time steps for each OutKey.
*/
/********************************************************/
/********************************************************/



/* Inclusion of output-related files

   - SOILWAT2 binary (SW_OUTTEXT)
      - SW_Output.c
      - SW_Output_get_functions.c
      - SW_Output_outtext.c

   - SOILWAT2 test (SW_OUTTEXT)
      - SW_Output_mock.c + SW_Output.h

   - rSOILWAT2 package (SW_OUTARRAY)
      - SW_Output.c
      - SW_Output_get_functions.c
      - SW_Output_outarray.c

   - STEPWAT2 (SW_OUTTEXT, SW_OUTARRAY)
      - SW_Output.c
      - SW_Output_get_functions.c
      - SW_Output_outtext.c
      - SW_Output_outarray.c
*/


#ifndef SW_OUTPUT_H
#define SW_OUTPUT_H

#include "include/SW_datastructs.h"

#ifdef __cplusplus
extern "C" {
#endif

/* These are the keywords to be found in the output setup file */
/* some of them are from the old fortran model and are no longer */
/* implemented, but are retained for some tiny measure of backward */
/* compatibility */
						//KEY			INDEX	OBJT	SUMTYPE
#define SW_WETHR		"WTHR"			//0		2		0/* position and variable marker, not an output key */
#define SW_TEMP			"TEMP"			//1		2		2
#define SW_PRECIP		"PRECIP"		//2		2		1
#define SW_SOILINF		"SOILINFILT"	//3		2		1
#define SW_RUNOFF		"RUNOFF"		//4		2		1
#define SW_ALLH2O   	"ALLH2O"		//5		4		0/* position and variable marker, not an output key */
#define SW_VWCBULK  	"VWCBULK"		//6		4		2
#define SW_VWCMATRIC	"VWCMATRIC"		//7		4		2
#define SW_SWCBULK    	"SWCBULK"		//8		4		2
#define SW_SWABULK	 	"SWABULK"		//9		4		2
#define SW_SWAMATRIC	"SWAMATRIC"		//10	4		2
#define SW_SWA				"SWA"        //11
#define SW_SWPMATRIC    "SWPMATRIC"		//12	4		2
#define SW_SURFACEW		"SURFACEWATER"	//13	4		2
#define SW_TRANSP		"TRANSP"		//14	4		1
#define SW_EVAPSOIL		"EVAPSOIL"		//15	4		1
#define SW_EVAPSURFACE	"EVAPSURFACE"	//16	4		1
#define SW_INTERCEPTION	"INTERCEPTION"	//17	4		1
#define SW_LYRDRAIN		"LYRDRAIN"		//18	4		1
#define SW_HYDRED		"HYDRED"		//19	4		1
#define SW_ET			"ET"			//20	4		0/* position and variable marker, not an output key */
#define SW_AET			"AET"			//21	4		1
#define SW_PET			"PET"			//22	4		1
#define SW_WETDAY		"WETDAY"		//23	4		1
#define SW_SNOWPACK		"SNOWPACK"		//24	4		2
#define SW_DEEPSWC		"DEEPSWC"		//25	4		1
#define SW_SOILTEMP		"SOILTEMP"		//26	4		2
#define SW_FROZEN       "FROZEN"        //27    ?       ?
#define SW_ALLVEG		"ALLVEG"		//28	5		0/* position and variable marker, not an output key */
#define SW_ESTAB		"ESTABL"		//29	5		0
#define SW_CO2EFFECTS	"CO2EFFECTS"	//30	?		?
#define SW_BIOMASS		"BIOMASS"		//31	?		?

#define SW_OUTNKEYS 32 /* must also match number of items in enum (minus eSW_NoKey and eSW_LastKey) */

/* summary methods */
#define SW_SUM_OFF "OFF"  /* don't output */
#define SW_SUM_SUM "SUM"  /* sum for period */
#define SW_SUM_AVG "AVG"  /* arith. avg for period */
#define SW_SUM_FNL "FIN"  /* value on last day in period */
#define SW_NSUMTYPES 4

/* convenience loops for consistency.
 * k must be a defined variable, either of OutKey type
 * or int (IntU is better).
 */
#define ForEachOutKey(k)     for((k)=eSW_NoKey+1; (k)<eSW_LastKey;   (k)++)



/* =================================================== */
/*            Externed Global Variables                */
/* --------------------------------------------------- */

extern SW_OUTPUT SW_Output[SW_OUTNKEYS];

extern char _Sep;
extern TimeInt tOffset;

extern OutPeriod timeSteps[SW_OUTNKEYS][SW_OUTNPERIODS];
extern IntUS used_OUTNPERIODS;
extern Bool use_OutPeriod[SW_OUTNPERIODS];

extern char *colnames_OUT[SW_OUTNKEYS][5 * NVEGTYPES + MAX_LAYERS];
extern IntUS ncol_OUT[SW_OUTNKEYS];

#ifdef STEPWAT
  extern Bool prepare_IterationSummary;
  extern Bool storeAllIterations;
#endif

extern char const *key2str[];
extern char const *pd2longstr[];
extern char const *styp2str[];

/* =================================================== */
/*             Global Function Declarations            */
/* --------------------------------------------------- */
void SW_OUT_construct(void);
void SW_OUT_deconstruct(Bool full_reset);
void SW_OUT_set_ncol(void);
void SW_OUT_set_colnames(void);
void SW_OUT_new_year(void);
int SW_OUT_read_onekey(OutKey k, OutSum sumtype, int first, int last,
					   char msg[], size_t sizeof_msg, SW_VEGPROD* SW_VegProd);
void SW_OUT_read(SW_ALL* sw);
void SW_OUT_sum_today(ObjType otyp, SW_VEGPROD* SW_VegProd, SW_WEATHER* SW_Weather);
void SW_OUT_write_today(SW_ALL* sw);
void SW_OUT_write_year(void);
void SW_OUT_flush(SW_ALL* sw);
void _collect_values(SW_ALL* sw);
void _echo_outputs(SW_ALL* sw);

void find_OutPeriods_inUse(void);
Bool has_OutPeriod_inUse(OutPeriod pd, OutKey k);
Bool has_keyname_soillayers(const char *var);
Bool has_key_soillayers(OutKey k);

#ifdef STEPWAT
void find_OutPeriods_inUse2(void);
Bool has_OutPeriod_inUse2(OutPeriod pd, OutKey k);
void SW_OUT_set_SXWrequests(void);
#endif


// Functions that format the output in `sw_outstr` for printing
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
 * 22 June-15 (akt)  Added code for adding surfaceAvg at output
 */
void get_none(OutPeriod pd, SW_ALL* sw); /* default until defined */

#ifdef SW_OUTTEXT
void get_temp_text(OutPeriod pd, SW_ALL* sw);
void get_precip_text(OutPeriod pd, SW_ALL* sw);
void get_vwcBulk_text(OutPeriod pd, SW_ALL* sw);
void get_vwcMatric_text(OutPeriod pd, SW_ALL* sw);
void get_swcBulk_text(OutPeriod pd, SW_ALL* sw);
void get_swpMatric_text(OutPeriod pd, SW_ALL* sw);
void get_swaBulk_text(OutPeriod pd, SW_ALL* sw);
void get_swaMatric_text(OutPeriod pd, SW_ALL* sw);
void get_swa_text(OutPeriod pd, SW_ALL* sw);
void get_surfaceWater_text(OutPeriod pd, SW_ALL* sw);
void get_runoffrunon_text(OutPeriod pd, SW_ALL* sw);
void get_transp_text(OutPeriod pd, SW_ALL* sw);
void get_evapSoil_text(OutPeriod pd, SW_ALL* sw);
void get_evapSurface_text(OutPeriod pd, SW_ALL* sw);
void get_interception_text(OutPeriod pd, SW_ALL* sw);
void get_soilinf_text(OutPeriod pd, SW_ALL* sw);
void get_lyrdrain_text(OutPeriod pd, SW_ALL* sw);
void get_hydred_text(OutPeriod pd, SW_ALL* sw);
void get_aet_text(OutPeriod pd, SW_ALL* sw);
void get_pet_text(OutPeriod pd, SW_ALL* sw);
void get_wetdays_text(OutPeriod pd, SW_ALL* sw);
void get_snowpack_text(OutPeriod pd, SW_ALL* sw);
void get_deepswc_text(OutPeriod pd, SW_ALL* sw);
void get_estab_text(OutPeriod pd, SW_ALL* sw);
void get_soiltemp_text(OutPeriod pd, SW_ALL* sw);
void get_frozen_text(OutPeriod pd, SW_ALL* sw);
void get_co2effects_text(OutPeriod pd, SW_ALL* sw);
void get_biomass_text(OutPeriod pd, SW_ALL* sw);
#endif

#if defined(RSOILWAT)
void get_temp_mem(OutPeriod pd, SW_ALL* sw);
void get_precip_mem(OutPeriod pd, SW_ALL* sw);
void get_vwcBulk_mem(OutPeriod pd, SW_ALL* sw);
void get_vwcMatric_mem(OutPeriod pd, SW_ALL* sw);
void get_swcBulk_mem(OutPeriod pd, SW_ALL* sw);
void get_swpMatric_mem(OutPeriod pd, SW_ALL* sw);
void get_swaBulk_mem(OutPeriod pd, SW_ALL* sw);
void get_swaMatric_mem(OutPeriod pd, SW_ALL* sw);
void get_swa_mem(OutPeriod pd, SW_ALL* sw);
void get_surfaceWater_mem(OutPeriod pd, SW_ALL* sw);
void get_runoffrunon_mem(OutPeriod pd, SW_ALL* sw);
void get_transp_mem(OutPeriod pd, SW_ALL* sw);
void get_evapSoil_mem(OutPeriod pd, SW_ALL* sw);
void get_evapSurface_mem(OutPeriod pd, SW_ALL* sw);
void get_interception_mem(OutPeriod pd, SW_ALL* sw);
void get_soilinf_mem(OutPeriod pd, SW_ALL* sw);
void get_lyrdrain_mem(OutPeriod pd, SW_ALL* sw);
void get_hydred_mem(OutPeriod pd, SW_ALL* sw);
void get_aet_mem(OutPeriod pd, SW_ALL* sw);
void get_pet_mem(OutPeriod pd, SW_ALL* sw);
void get_wetdays_mem(OutPeriod pd, SW_ALL* sw);
void get_snowpack_mem(OutPeriod pd, SW_ALL* sw);
void get_deepswc_mem(OutPeriod pd, SW_ALL* sw);
void get_estab_mem(OutPeriod pd, SW_ALL* sw);
void get_soiltemp_mem(OutPeriod pd, SW_ALL* sw);
void get_frozen_mem(OutPeriod pd, SW_ALL* sw);
void get_co2effects_mem(OutPeriod pd, SW_ALL* sw);
void get_biomass_mem(OutPeriod pd, SW_ALL* sw);

#elif defined(STEPWAT)
void get_temp_agg(OutPeriod pd, SW_ALL* sw);
void get_precip_agg(OutPeriod pd, SW_ALL* sw);
void get_vwcBulk_agg(OutPeriod pd, SW_ALL* sw);
void get_vwcMatric_agg(OutPeriod pd, SW_ALL* sw);
void get_swcBulk_agg(OutPeriod pd, SW_ALL* sw);
void get_swpMatric_agg(OutPeriod pd, SW_ALL* sw);
void get_swaBulk_agg(OutPeriod pd, SW_ALL* sw);
void get_swaMatric_agg(OutPeriod pd, SW_ALL* sw);
void get_swa_agg(OutPeriod pd, SW_ALL* sw);
void get_surfaceWater_agg(OutPeriod pd, SW_ALL* sw);
void get_runoffrunon_agg(OutPeriod pd, SW_ALL* sw);
void get_transp_agg(OutPeriod pd, SW_ALL* sw);
void get_evapSoil_agg(OutPeriod pd, SW_ALL* sw);
void get_evapSurface_agg(OutPeriod pd, SW_ALL* sw);
void get_interception_agg(OutPeriod pd, SW_ALL* sw);
void get_soilinf_agg(OutPeriod pd, SW_ALL* sw);
void get_lyrdrain_agg(OutPeriod pd, SW_ALL* sw);
void get_hydred_agg(OutPeriod pd, SW_ALL* sw);
void get_aet_agg(OutPeriod pd, SW_ALL* sw);
void get_pet_agg(OutPeriod pd, SW_ALL* sw);
void get_wetdays_agg(OutPeriod pd, SW_ALL* sw);
void get_snowpack_agg(OutPeriod pd, SW_ALL* sw);
void get_deepswc_agg(OutPeriod pd, SW_ALL* sw);
void get_estab_agg(OutPeriod pd, SW_ALL* sw);
void get_soiltemp_agg(OutPeriod pd, SW_ALL* sw);
void get_frozen_agg(OutPeriod pd, SW_ALL* sw);
void get_co2effects_agg(OutPeriod pd, SW_ALL* sw);
void get_biomass_agg(OutPeriod pd, SW_ALL* sw);

void get_temp_SXW(OutPeriod pd, SW_ALL* sw);
void get_precip_SXW(OutPeriod pd, SW_ALL* sw);
void get_vwcBulk_SXW(OutPeriod pd, SW_ALL* sw);
void get_vwcMatric_SXW(OutPeriod pd, SW_ALL* sw);
void get_swcBulk_SXW(OutPeriod pd, SW_ALL* sw);
void get_swpMatric_SXW(OutPeriod pd, SW_ALL* sw);
void get_swaBulk_SXW(OutPeriod pd, SW_ALL* sw);
void get_swaMatric_SXW(OutPeriod pd, SW_ALL* sw);
void get_swa_SXW(OutPeriod pd, SW_ALL* sw);
void get_surfaceWater_SXW(OutPeriod pd, SW_ALL* sw);
void get_runoffrunon_SXW(OutPeriod pd, SW_ALL* sw);
void get_transp_SXW(OutPeriod pd, SW_ALL* sw);
void get_evapSoil_SXW(OutPeriod pd, SW_ALL* sw);
void get_evapSurface_SXW(OutPeriod pd, SW_ALL* sw);
void get_interception_SXW(OutPeriod pd, SW_ALL* sw);
void get_soilinf_SXW(OutPeriod pd, SW_ALL* sw);
void get_lyrdrain_SXW(OutPeriod pd, SW_ALL* sw);
void get_hydred_SXW(OutPeriod pd, SW_ALL* sw);
void get_aet_SXW(OutPeriod pd, SW_ALL* sw);
void get_pet_SXW(OutPeriod pd, SW_ALL* sw);
void get_wetdays_SXW(OutPeriod pd, SW_ALL* sw);
void get_snowpack_SXW(OutPeriod pd, SW_ALL* sw);
void get_deepswc_SXW(OutPeriod pd, SW_ALL* sw);
void get_estab_SXW(OutPeriod pd, SW_ALL* sw);
void get_soiltemp_SXW(OutPeriod pd, SW_ALL* sw);
void get_frozen_SXW(OutPeriod pd, SW_ALL* sw);
void get_co2effects_SXW(OutPeriod pd, SW_ALL* sw);
void get_biomass_SXW(OutPeriod pd, SW_ALL* sw);
#endif


#ifdef DEBUG_MEM
	void SW_OUT_SetMemoryRefs(void);
#endif


#ifdef __cplusplus
}
#endif

#endif
