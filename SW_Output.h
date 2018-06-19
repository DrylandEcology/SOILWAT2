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

#ifndef SW_OUTPUT_H
#define SW_OUTPUT_H

#include "Times.h"
#include "SW_Defines.h"
#include "SW_SoilWater.h"
#include "SW_Weather.h"
#include "SW_VegProd.h"


// Array-based output:
#if defined(RSOILWAT) || defined(STEPWAT)
#define SW_OUTARRAY
#endif

// Text-based output:
#if defined(SOILWAT) || defined(STEPWAT)
#define SW_OUTTEXT
#endif



#define OUTSTRLEN 3000 /* max output string length: in get_transp: 4*every soil layer with 14 chars */
#define OUT_DIGITS 6 // number of floating point decimal digits written to output files

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
#define SW_ALLVEG		"ALLVEG"		//27	5		0/* position and variable marker, not an output key */
#define SW_ESTAB		"ESTABL"		//28	5		0
#define SW_CO2EFFECTS		"CO2EFFECTS"		//29	?		?

#define SW_OUTNKEYS 30 /* must also match number of items in enum (minus eSW_NoKey and eSW_LastKey) */

/* these are the code analog of the above */
/* see also key2str[] in Output.c */
/* take note of boundary conditions in ForEach...() loops below */
typedef enum {
	eSW_NoKey = -1,
	/* weather/atmospheric quantities */
	eSW_AllWthr, /* includes all weather vars */
	eSW_Temp,
	eSW_Precip,
	eSW_SoilInf,
	eSW_Runoff,
	/* soil related water quantities */
	eSW_AllH2O,
	eSW_VWCBulk,
	eSW_VWCMatric,
	eSW_SWCBulk,
	eSW_SWABulk,
	eSW_SWAMatric,
	eSW_SWA,
	eSW_SWPMatric,
	eSW_SurfaceWater,
	eSW_Transp,
	eSW_EvapSoil,
	eSW_EvapSurface,
	eSW_Interception,
	eSW_LyrDrain,
	eSW_HydRed,
	eSW_ET,
	eSW_AET,
	eSW_PET, /* really belongs in wth, but for historical reasons we'll keep it here */
	eSW_WetDays,
	eSW_SnowPack,
	eSW_DeepSWC,
	eSW_SoilTemp,
	/* vegetation quantities */
	eSW_AllVeg,
	eSW_Estab,
	// vegetation other */
	eSW_CO2Effects,
	eSW_LastKey /* make sure this is the last one */
} OutKey;


/* summary methods */
#define SW_SUM_OFF "OFF"  /* don't output */
#define SW_SUM_SUM "SUM"  /* sum for period */
#define SW_SUM_AVG "AVG"  /* arith. avg for period */
#define SW_SUM_FNL "FIN"  /* value on last day in period */
#define SW_NSUMTYPES 4

typedef enum {
	eSW_Off, eSW_Sum, eSW_Avg, eSW_Fnl
} OutSum;

typedef struct {
	OutKey mykey;
	ObjType myobj;
	OutSum sumtype;
	Bool use,	// TRUE if output is requested
		has_sl;	// TRUE if output key/type produces output for each soil layer
	TimeInt first, last, 			/* updated for each year */
			first_orig, last_orig;
	#ifdef RSOILWAT
	char *outfile; /* point to name of output file */ //could probably be removed
	#endif
	void (*pfunc)(OutPeriod); /* pointer to output routine */
} SW_OUTPUT;


/* convenience loops for consistency.
 * k must be a defined variable, either of OutKey type
 * or int (IntU is better).
 */
#define ForEachOutKey(k)     for((k)=eSW_NoKey+1; (k)<eSW_LastKey;   (k)++)
#define ForEachSWC_OutKey(k) for((k)=eSW_AllH2O;  (k)<=eSW_SnowPack; (k)++)
#define ForEachWTH_OutKey(k) for((k)=eSW_AllWthr; (k)<=eSW_Precip;   (k)++)
#define ForEachVES_OutKey(k) for((k)=eSW_AllVeg;  (k)<=eSW_Estab;    (k)++)



// Function declarations
void SW_OUT_construct(void);
void SW_OUT_set_ncol(void);
void SW_OUT_set_colnames(void);
void SW_OUT_new_year(void);
int SW_OUT_read_onekey(OutKey *k, char keyname[], char sumtype[],
	char period[], int first, char last[], char outfile[], char msg[]);
void SW_OUT_read(void);
void SW_OUT_sum_today(ObjType otyp);
void SW_OUT_write_today(void);
void SW_OUT_write_year(void);
void SW_OUT_flush(void);
void _collect_values(void);
void _echo_outputs(void);

void find_OutPeriods_inUse(void);
Bool has_OutPeriod_inUse(OutPeriod pd, OutKey k);
Bool has_soillayers(const char *var);


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
 * 22 June-15 (akt)  Added code for adding surfaceTemp at output
 */
void get_none(OutPeriod pd); /* default until defined */
void get_temp(OutPeriod pd);
void get_precip(OutPeriod pd);
void get_vwcBulk(OutPeriod pd);
void get_vwcMatric(OutPeriod pd);
void get_swcBulk(OutPeriod pd);
void get_swpMatric(OutPeriod pd);
void get_swaBulk(OutPeriod pd);
void get_swaMatric(OutPeriod pd);
void get_swa(OutPeriod pd);
void get_surfaceWater(OutPeriod pd);
void get_runoffrunon(OutPeriod pd);
void get_transp(OutPeriod pd);
void get_evapSoil(OutPeriod pd);
void get_evapSurface(OutPeriod pd);
void get_interception(OutPeriod pd);
void get_soilinf(OutPeriod pd);
void get_lyrdrain(OutPeriod pd);
void get_hydred(OutPeriod pd);
void get_aet(OutPeriod pd);
void get_pet(OutPeriod pd);
void get_wetdays(OutPeriod pd);
void get_snowpack(OutPeriod pd);
void get_deepswc(OutPeriod pd);
void get_estab(OutPeriod pd);
void get_soiltemp(OutPeriod pd);
void get_co2effects(OutPeriod pd);


#ifdef DEBUG_MEM
	void SW_OUT_SetMemoryRefs(void);
#endif

#endif
