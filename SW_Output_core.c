/********************************************************/
/********************************************************/
/*	Source file: Output.c
 Type: module
 Application: SOILWAT - soilwater dynamics simulator
 Purpose: Read / write and otherwise manage the
 user-specified output flags.

 COMMENTS: The algorithm for the summary bookkeeping
 is much more complicated than I'd like, but I don't
 see a cleaner way to address the need to keep
 running tabs without storing daily arrays for each
 output variable.  That might make somewhat
 simpler code, and perhaps slightly more efficient,
 but at a high cost of memory, and the original goal
 was to make this object oriented, so memory should
 be used sparingly.  Plus, much of the code is quite
 general, and the main loops are very simple indeed.

 Generally, adding a new output key is fairly simple,
 and much of the code need not be considered.
 Refer to the comment block at the very end of this
 file for details.

 Comment (06/23/2015, drs): In summary, the output of SOILWAT works as follows
 SW_OUT_flush() calls at end of year and SW_Control.c/_collect_values() calls daily
 1) SW_OUT_sum_today() that
 1.1) if end of an output period, call average_for(): converts the (previously summed values (by sumof_wth) of) SW_WEATHER_OUTPUTS portion of SW_Weather from the ‘Xsum' to the ‘Xavg'
 1.2) on each day calls collect_sums() that calls sumof_wth(): SW_Weather (simulation slot ’now') values are summed up during each output period and stored in the SW_WEATHER_OUTPUTS (output) slots 'Xsum' of SW_Weather

 2) SW_OUT_write_today() that
 2.1) calls the get_temp() etc functions via SW_Output.pfunc: the values stored in ‘Xavg’ by average_for are converted to text string 'sw_outstr’
 2.2) outputs the text string 'sw_outstr’ with the fresh values to a text file via SW_Output.fp_X



 History:
 9/11/01 cwb -- INITIAL CODING
 10-May-02 cwb -- Added conditionals for interfacing
 with STEPPE.  See SW_OUT_read(), SW_OUT_close_files(),
 SW_OUT_write_today(), get_temp() and get_transp().
 The changes prevent the model from opening, closing,
 and writing to files because SXW only requires periodic
 (eg, weekly) transpiration and yearly temperature
 from get_transp() and get_temp(), resp.
 --The output config file must be prepared by the SXW
 interface code such that only yearly temp and daily,
 weekly, or monthly transpiration are requested for
 periods (1,end).

 12/02 - IMPORTANT CHANGE - cwb
 refer to comments in Times.h regarding base0

 27-Aug-03 (cwb) Just a comment that this code doesn't
 handle missing values in the summaries, especially
 the averages.  This really needs to be addressed
 sometime, but for now it's the user's responsibility
 to make sure there are no missing values.  The
 model doesn't generate any on its own, but it
 still needs to be fixed, although that will take
 a bit of work to keep track of the number of
 missing days, etc.
 20090826 (drs) stricmp -> strcmp -> Str_CompareI; in SW_OUT_sum_today () added break; after default:
 20090827 (drs) changed output-strings str[12] to #define OUTSTRLEN 18; str[OUTSTRLEN]; because of sprintf(str, ...) overflow and memory corruption into for-index i
 20090909 (drs) strcmp -> Str_CompareI (not case-sensitive)
 20090915 (drs) wetdays output was not working: changed in get_wetdays() the output from sprintf(fmt, "%c%%3.0f", _Sep); to sprintf(str, "%c%i", _Sep, val);
 20091013 (drs) output period of static void get_swcBulk(void) was erroneously set to eSW_SWP instead of eSW_SWCBulk
 20091015 (drs) ppt is divided into rain and snow and all three values plust snowmelt are output into precip
 20100202 (drs) changed SW_CANOPY to SW_CANOPYEV and SW_LITTER to SW_LITTEREV;
 and eSW_Canopy to eSW_CanopyEv and eSW_Litter to eSW_LitterEv;
 added SWC_CANOPYINT, SW_LITTERINT, SW_SOILINF, SW_LYRDRAIN;
 added eSW_CanopyInt, eSW_LitterInt, eSW_SoilInf, eSW_LyrDrain;
 updated key2str, key2obj;
 added private functions get_canint(), get_litint(), get_soilinf(), get_lyrdrain();
 updated SW_OUT_construct(), sumof_swc() and average_for() with new functions and keys
 for layer drain use only for(i=0; i < SW_Site.n_layers-1; i++)
 04/16/2010 (drs) added SWC_SWA, eSW_SWABulk; updated key2str, key2obj; added private functions get_swaBulk();
 updated SW_OUT_construct(), sumof_swc()[->make calculation here] and average_for() with new functions and keys
 10/20/2010 (drs) added SW_HYDRED, eSW_HydRed; updated key2str, key2obj; added private functions get_hydred();
 updated SW_OUT_construct(), sumof_swc()[->make calculation here] and average_for() with new functions and keys
 11/16/2010 (drs) added forest intercepted water to canopy interception
 updated get_canint(), SW_OUT_construct(), sumof_swc()[->make calculation here] and average_for()
 01/03/2011	(drs) changed parameter type of str2period(), str2key(), and str2type() from 'const char *' to 'char *' to avoid 'discard qualifiers from pointer target type' in Str_ComparI()
 01/05/2011	(drs) made typecast explicit: changed in SW_OUT_write_today(): SW_Output[k].pfunc() to ((void (*)(OutPeriod))SW_Output[k].pfunc)(); -> doesn't solve problem under darwin10
 -> 'abort trap' was due to str[OUTSTRLEN] being too short (OUTSTRLEN 18), changed to OUTSTRLEN 100
 01/06/2011	(drs) changed all floating-point output from %7.4f to %7.6f to avoid rounding errors in post-run output calculations
 03/06/2011	(drs) in average_for() changed loop to average hydred from "for(i=0; i < SW_Site.n_layers-1; i++)" to "ForEachSoilLayer(i)" because deepest layer was not averaged
 07/22/2011 (drs) added SW_SURFACEW and SW_EVAPSURFACE, eSW_SurfaceWater and eSW_EvapSurface; updated key2str, key2obj; added private functions get_surfaceWater() and get_evapSurface();
 updated SW_OUT_construct(), sumof_swc()[->make calculation here] and average_for() with new functions and keys
 09/12/2011	(drs)	renamed SW_EVAP to SW_EVAPSOIL, and eSW_Evap to eSW_EvapSoil;
 deleted SW_CANOPYEV, eSW_CanopyEv, SW_LITTEREV, eSW_LitterEv, SW_CANOPYINT, eSW_CanopyInt, SW_LITTERINT, and eSW_LitterInt;
 added SW_INTERCEPTION and eSW_Interception
 09/12/2011	(drs)	renamed get_evap() to get_evapSoil(); renamed get_EvapsurfaceWater() to get_evapSurface()
 deleted get_canevap(), get_litevap(), get_canint(), and get_litint()
 added get_interception()
 09/12/2011	(drs)	increased #define OUTSTRLEN from 100 to 400 (since transpiration and hydraulic redistribution will be 4x as long)
 increased static char sw_outstr[0xff] from 0xff=255 to OUTSTRLEN
 09/12/2011	(drs)	updated SW_OUT_construct(), sumof_swc()[->make calculation here] and average_for() with new functions and keys
 09/12/2011	(drs)	added snowdepth as output to snowpack, i.e., updated updated get_snowpack(), sumof_swc()[->make calculation here] and average_for()
 09/30/2011	(drs)	in SW_OUT_read(): opening of output files: SW_Output[k].outfile is extended by SW_Files.in:SW_OutputPrefix()
 01/09/2012	(drs)	'abort trap' in get_transp due to sw_outstr[OUTSTRLEN] being too short (OUTSTRLEN 400), changed to OUTSTRLEN 1000
 05/25/2012  (DLM) added SW_SOILTEMP to keytostr, updated keytoobj;  wrote get_soiltemp(void) function, added code in the _construct() function to link to get_soiltemp() correctly;  added code to sumof and average_for() to handle the new key
 05/25/2012  (DLM) added a few lines of nonsense to sumof_ves() function in order to get rid of the annoying compiler warnings
 06/12/2012  (DLM) changed OUTSTRLEN from 1000 to 2000
 07/02/2012  (DLM) updated # of chars in keyname & upkey arrays in SW_OUT_READ() function to account for longer keynames... for some reason it would still read them in correctly on OS X without an error, but wouldn't on JANUS.
 11/30/2012	(clk)	changed get_surfaceWater() to ouput amound of surface water, surface runoff, and snowmelt runoff, respectively.
 12/13/2012	(clk, drs)	changed get_surfaceWater() to output amount of surface water
 added get_runoffrunon() to output surface runoff, and snowmelt runoff, respectively, in a separate file -> new version of outsetupin;
 updated key2str and OutKey
 12/14/2012 (drs)	changed OUTSTRLEN from 2000 to 3000 to prevent 'Abort trap: 6' at runtime
 01/10/2013	(clk)	in function SW_OUT_read, when creating the output files, needed to loop through this four times
 for each OutKey, giving us the output files for day, week, month, and year. Also, added
 a switch statement to acqurie the dy, wk, mo, or yr suffix based on the iteration of the for loop.
 When reading in lines from outsetup, removed PERIOD from the read in because it is unneeded in since
 we are doing all time steps.
 Removed the line SW_Output[k].period = str2period(Str_ToUpper(period, ext)), since we are no longer
 reading in a period.
 in function SW_OUT_close_files need to loop through and close all four time step files for each
 OutKey, determined which file to close based on the iteration of the for loop.
 in function SW_OUT_write_today needed to loop through the code four times for each OutKey, changing
 the period of the output for each iteration. With the for loop, I am also able to pick the
 proper FILE pointer to choose when outputting the data.
 in function average_for needed to loop through the code four times for each OutKey, changing
 the period of the output for each iteration.
 the function str2period is no longer used in the code, but will leave it in the code for now.
 01/17/2013	(clk)	in function SW_OUT_read, created an additional condition that if the keyname was TIMESTEP,
 the code would rescan the line looking for up to 5 strings. The first one would be saved
 back into keyname, while the other four would be stored in a matrix. These values would be
 the desired timesteps to output. Then you would convert these strings to periods, and store
 them in an array of integers and keep track of how many timesteps were actually read in.
 The other changes are modifications of the changes made on 01/10/2013. For all the for loops, instead of
 looping through 4 times, we now loop through for as many times as periods that we read in from the
 TIMESTEP line. Also, in all the switch cases where we increased that value which changed the period
 to the next period, we now use the array of integers that stored the period and move through that with
 each iteration.
 02/05/2013	(clk) in the function get_soiltemp(), when determing the period, the code was using SW_Output[eSW_SWCBulk].period,
 which needed to be SW_Output[eSW_SoilTemp].period, since we are looking at soil temp, not SWCM.
 04/16/2013	(clk)	Added two new ouputs: vwcMatric and swaMatric.
 in the get functions, the calculation is
 (the matric value) = (the bulk value)/(1-fractionVolBulk_gravel)
 Also changed the names of swp -> swpMatric, swc -> swcBulk, swcm -> vwcBulk, and swa -> swaBulk
 06/27/2013	(drs)	closed open files if LogError() with LOGFATAL is called in SW_OUT_read()

 07/01/2013	(clk)	Changed the outsetup file so that the TIMESTEP line could be used or not used, depending on the need
 If the TIMESTEP line is not used, need to have an additional column for the period again
 Within the code, changed the timesteps array to be two dimensional, and keep track of the periods to be used
 for each of the outputs. Then, at every for loop that was implemented for the TIMESTEP code, put in
 place a conditional that will determine which of the four possible periods are to be used for each output.
 07/09/2013	(clk)	Added forb to the outputs: transpiration, surface evaporation, interception, and hydraulic redistribution
 08/21/2013	(clk)	Modified the establisment output to actually output for each species and not just the last one in the group.
 06/23/2015 (akt)	Added output for surface temperature to get_temp()
 06/15/2017 Modified PPT values so daily, weekly, monthly, and yearly are run for both RSOILWAT and STEPWAT
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

/* Note: `get_XXX` functions are declared here for all and defined differently
         in the program-specific files for the specific uses:
    - for SOILWAT2-standalone: "../rSW_Output_rSOILWAT2.c"
    - for rSOILWAT2: "SW_Output_SOILWAT2.c"
    - for rSOILWAT2: "../rSW_Output_rSOILWAT2.c"
    - for STEPWAT2: "../SW_Output_STEPWAT2.c"
*/


/* =================================================== */
/*                  Global Variables                   */
/* --------------------------------------------------- */
extern SW_SITE SW_Site;
extern SW_SOILWAT SW_Soilwat;
extern SW_MODEL SW_Model;
extern SW_WEATHER SW_Weather;
extern SW_VEGPROD SW_VegProd;
extern SW_VEGESTAB SW_VegEstab;
extern Bool EchoInits;
extern SW_CARBON SW_Carbon;

SW_OUTPUT SW_Output[SW_OUTNKEYS]; /* defined here, externed in `SW_Output_ZZZ.c` */
SW_FILE_STATUS SW_File_Status;

char _Sep; /* output delimiter */
TimeInt tOffset; /* 1 or 0 means we're writing previous or current period */
Bool bFlush_output; /* process partial period ? */
char sw_outstr[OUTSTRLEN];

int used_OUTNPERIODS; // number of different time steps/periods that are used/requested
OutPeriod timeSteps[SW_OUTNKEYS][SW_OUTNPERIODS];// array to keep track of the periods that will be used for each output
int ncol_OUT[SW_OUTNKEYS]; // number of output columns for each output key
char *colnames_OUT[SW_OUTNKEYS][5 * NVEGTYPES + MAX_LAYERS]; // names of output columns for each output key; number is an expensive guess

#ifdef STEPWAT
extern ModelType Globals; // defined in `ST_Main.c`
Bool isPartialSoilwatOutput = FALSE;
Bool storeAllIterations = TRUE;
char outstr_all_iters[OUTSTRLEN];
#endif


/* =================================================== */
/*                Module-Level Variables               */
/* --------------------------------------------------- */
static char *MyFileName;

static int useTimeStep; /* flag to determine whether or not the line TIMESTEP exists */

/* These MUST be in the same order as enum OutKey in
 * SW_Output.h */
char const *key2str[] =
{ SW_WETHR, SW_TEMP, SW_PRECIP, SW_SOILINF, SW_RUNOFF, SW_ALLH2O, SW_VWCBULK,
		SW_VWCMATRIC, SW_SWCBULK, SW_SWABULK, SW_SWAMATRIC, SW_SWA, SW_SWPMATRIC,
		SW_SURFACEW, SW_TRANSP, SW_EVAPSOIL, SW_EVAPSURFACE, SW_INTERCEPTION,
		SW_LYRDRAIN, SW_HYDRED, SW_ET, SW_AET, SW_PET, SW_WETDAY, SW_SNOWPACK,
		SW_DEEPSWC, SW_SOILTEMP,
		SW_ALLVEG, SW_ESTAB, SW_CO2EFFECTS };
/* converts an enum output key (OutKey type) to a module  */
/* or object type. see SW_Output.h for OutKey order.         */
/* MUST be SW_OUTNKEYS of these */

static ObjType key2obj[] =
{ eWTH, eWTH, eWTH, eWTH, eWTH, eSWC, eSWC, eSWC, eSWC, eSWC, eSWC, eSWC, eSWC, eSWC,
		eSWC, eSWC, eSWC, eSWC, eSWC, eSWC, eSWC, eSWC, eSWC, eSWC, eSWC, eSWC,
		eSWC, eVES, eVES, eVPD };

static char *pd2str[] =
{ SW_DAY, SW_WEEK, SW_MONTH, SW_YEAR };

static char const *styp2str[] =
{ SW_SUM_OFF, SW_SUM_SUM, SW_SUM_AVG, SW_SUM_FNL };


/* =================================================== */
/* =================================================== */
/*             Private Function Declarations            */
/* --------------------------------------------------- */

void populate_output_values(char *reg_file_array, char *soil_file_array, int output_var, IntU year_out, int outstr_file);
#ifndef RSOILWAT
// the function `create_col_headers` should be really used by all applications for consistent naming of output
void create_col_headers(IntU outFileTimestep, FILE *regular_file, FILE *soil_file, int std_headers);
#endif

static OutPeriod str2period(char *s);
static OutKey str2key(char *s);
static OutSum str2stype(char *s);

static void collect_sums(ObjType otyp, OutPeriod op);
static void sumof_wth(SW_WEATHER *v, SW_WEATHER_OUTPUTS *s, OutKey k);
static void sumof_swc(SW_SOILWAT *v, SW_SOILWAT_OUTPUTS *s, OutKey k);
static void sumof_ves(SW_VEGESTAB *v, SW_VEGESTAB_OUTPUTS *s, OutKey k);
static void sumof_vpd(SW_VEGPROD *v, SW_VEGPROD_OUTPUTS *s, OutKey k);
static void average_for(ObjType otyp, OutPeriod pd);


/* =================================================== */
/* =================================================== */
/*             Private Function Definitions            */
/* --------------------------------------------------- */

static OutPeriod str2period(char *s)
{
	/* --------------------------------------------------- */
	IntUS pd;
	for (pd = 0; Str_CompareI(s, (char *)pd2str[pd]) && pd < SW_OUTNPERIODS; pd++);

	return (OutPeriod) pd;
}

static OutKey str2key(char *s)
{
	/* --------------------------------------------------- */
	IntUS key;

	for (key = 0; key < SW_OUTNKEYS && Str_CompareI(s, (char *)key2str[key]); key++) ;
	if (key == SW_OUTNKEYS)
	{
		LogError(logfp, LOGFATAL, "%s : Invalid key (%s) in %s", SW_F_name(eOutput), s);
	}
	return (OutKey) key;
}

static OutSum str2stype(char *s)
{
	/* --------------------------------------------------- */
	IntUS styp;

	for (styp = eSW_Off; styp < SW_NSUMTYPES && Str_CompareI(s, (char *)styp2str[styp]); styp++) ;
	if (styp == SW_NSUMTYPES)
	{
		LogError(logfp, LOGFATAL, "%s : Invalid summary type (%s)\n", SW_F_name(eOutput), s);
	}
	return (OutSum) styp;
}

static void sumof_vpd(SW_VEGPROD *v, SW_VEGPROD_OUTPUTS *s, OutKey k)
{
	int ik;

	switch (k)
	{
		case eSW_CO2Effects:
			ForEachVegType(ik) {
				s->veg[ik].biomass += v->veg[ik].biomass_daily[SW_Model.doy];
				s->veg[ik].biolive += v->veg[ik].biolive_daily[SW_Model.doy];
			}
			break;

		default:
			LogError(logfp, LOGFATAL, "PGMR: Invalid key in sumof_vpd(%s)", key2str[k]);
	}
}

static void sumof_ves(SW_VEGESTAB *v, SW_VEGESTAB_OUTPUTS *s, OutKey k)
{
	/* --------------------------------------------------- */
	/* k is always eSW_Estab, and this only gets called yearly */
	/* in fact, there's nothing to do here as the get_estab()
	 * function does everything needed.  This stub is here only
	 * to facilitate the loop everything else uses.
	 * That is, until we need to start outputting as-yet-unknown
	 * establishment variables.
	 */

// just a few lines of nonsense to supress the compile warnings
  if ((int)k == 1) {}
  if (0 == v->count) {}
  if (0 == s->days) {}
}

static void sumof_wth(SW_WEATHER *v, SW_WEATHER_OUTPUTS *s, OutKey k)
{
	/* --------------------------------------------------- */
	/* 	20091015 (drs) ppt is divided into rain and snow and all three values are output into precip */

	switch (k)
	{

	case eSW_Temp:
		s->temp_max += v->now.temp_max[Today];
		s->temp_min += v->now.temp_min[Today];
		s->temp_avg += v->now.temp_avg[Today];
		//added surfaceTemp for sum
		s->surfaceTemp += v->surfaceTemp;
		break;
	case eSW_Precip:
		s->ppt += v->now.ppt[Today];
		s->rain += v->now.rain[Today];
		s->snow += v->snow;
		s->snowmelt += v->snowmelt;
		s->snowloss += v->snowloss;
		break;
	case eSW_SoilInf:
		s->soil_inf += v->soil_inf;
		break;
	case eSW_Runoff:
		s->snowRunoff += v->snowRunoff;
		s->surfaceRunoff += v->surfaceRunoff;
		s->surfaceRunon += v->surfaceRunon;
		break;
	default:
		LogError(logfp, LOGFATAL, "PGMR: Invalid key in sumof_wth(%s)", key2str[k]);
	}

}

static void sumof_swc(SW_SOILWAT *v, SW_SOILWAT_OUTPUTS *s, OutKey k)
{
	/* --------------------------------------------------- */
	LyrIndex i;
	int j; // for use with ForEachVegType

	switch (k)
	{

	case eSW_VWCBulk: /* get swcBulk and convert later */
		ForEachSoilLayer(i)
			s->vwcBulk[i] += v->swcBulk[Today][i];
		break;

	case eSW_VWCMatric: /* get swcBulk and convert later */
		ForEachSoilLayer(i)
			s->vwcMatric[i] += v->swcBulk[Today][i];
		break;

	case eSW_SWCBulk:
		ForEachSoilLayer(i)
			s->swcBulk[i] += v->swcBulk[Today][i];
		break;

	case eSW_SWPMatric: /* can't avg swp so get swcBulk and convert later */
		ForEachSoilLayer(i)
			s->swpMatric[i] += v->swcBulk[Today][i];
		break;

	case eSW_SWABulk:
		ForEachSoilLayer(i)
			s->swaBulk[i] += fmax(
					v->swcBulk[Today][i] - SW_Site.lyr[i]->swcBulk_wiltpt, 0.);
		break;

	case eSW_SWAMatric: /* get swaBulk and convert later */
		ForEachSoilLayer(i)
			s->swaMatric[i] += fmax(
					v->swcBulk[Today][i] - SW_Site.lyr[i]->swcBulk_wiltpt, 0.);
		break;

	case eSW_SWA: /* get swaBulk and convert later */
		ForEachSoilLayer(i){
			ForEachVegType(j){
				s->SWA_VegType[j][i] += v->dSWA_repartitioned_sum[j][i];
				//printf("sum swa %f\n", s->SWA_VegType[j][i]);
			}
		}
		break;

	case eSW_SurfaceWater:
		s->surfaceWater += v->surfaceWater;
		break;

	case eSW_Transp:
		ForEachSoilLayer(i)
		{
			s->transp_total[i] += v->transpiration[SW_TREES][i]
					+ v->transpiration[SW_SHRUB][i] + v->transpiration[SW_FORBS][i]
					+ v->transpiration[SW_GRASS][i];
			s->transp[SW_TREES][i] += v->transpiration[SW_TREES][i];
			s->transp[SW_SHRUB][i] += v->transpiration[SW_SHRUB][i];
			s->transp[SW_FORBS][i] += v->transpiration[SW_FORBS][i];
			s->transp[SW_GRASS][i] += v->transpiration[SW_GRASS][i];
		}
		break;

	case eSW_EvapSoil:
		ForEachEvapLayer(i)
			s->evap[i] += v->evaporation[i];
		break;

	case eSW_EvapSurface:
		s->total_evap += v->evap_veg[SW_TREES] + v->evap_veg[SW_FORBS] + v->evap_veg[SW_SHRUB]
				+ v->evap_veg[SW_GRASS] + v->litter_evap + v->surfaceWater_evap;
		s->evap_veg[SW_TREES] += v->evap_veg[SW_TREES];
		s->evap_veg[SW_SHRUB] += v->evap_veg[SW_SHRUB];
		s->evap_veg[SW_FORBS] += v->evap_veg[SW_FORBS];
		s->evap_veg[SW_GRASS] += v->evap_veg[SW_GRASS];
		s->litter_evap += v->litter_evap;
		s->surfaceWater_evap += v->surfaceWater_evap;
		break;

	case eSW_Interception:
		s->total_int += v->int_veg[SW_TREES] + v->int_veg[SW_FORBS] + v->int_veg[SW_SHRUB] + v->int_veg[SW_GRASS]
				+ v->litter_int;
		s->int_veg[SW_TREES] += v->int_veg[SW_TREES];
		s->int_veg[SW_SHRUB] += v->int_veg[SW_SHRUB];
		s->int_veg[SW_FORBS] += v->int_veg[SW_FORBS];
		s->int_veg[SW_GRASS] += v->int_veg[SW_GRASS];
		s->litter_int += v->litter_int;
		break;

	case eSW_LyrDrain:
		for (i = 0; i < SW_Site.n_layers - 1; i++)
			s->lyrdrain[i] += v->drain[i];
		break;

	case eSW_HydRed:
		ForEachSoilLayer(i)
		{
			s->hydred_total[i] += v->hydred[SW_TREES][i] + v->hydred[SW_FORBS][i]
					+ v->hydred[SW_SHRUB][i] + v->hydred[SW_GRASS][i];
			s->hydred[SW_TREES][i] += v->hydred[SW_TREES][i];
			s->hydred[SW_SHRUB][i] += v->hydred[SW_SHRUB][i];
			s->hydred[SW_FORBS][i] += v->hydred[SW_FORBS][i];
			s->hydred[SW_GRASS][i] += v->hydred[SW_GRASS][i];
		}
		break;

	case eSW_AET:
		s->aet += v->aet;
		break;

	case eSW_PET:
		s->pet += v->pet;
		break;

	case eSW_WetDays:
		ForEachSoilLayer(i)
			if (v->is_wet[i])
				s->wetdays[i]++;
		break;

	case eSW_SnowPack:
		s->snowpack += v->snowpack[Today];
		s->snowdepth += v->snowdepth;
		break;

	case eSW_DeepSWC:
		s->deep += v->swcBulk[Today][SW_Site.deep_lyr];
		break;

	case eSW_SoilTemp:
		ForEachSoilLayer(i)
			s->sTemp[i] += v->sTemp[i];
		break;

	default:
		LogError(logfp, LOGFATAL, "PGMR: Invalid key in sumof_swc(%s)", key2str[k]);
	}
}

static void average_for(ObjType otyp, OutPeriod pd)
{
	/* --------------------------------------------------- */
	/* separates the task of obtaining a periodic average.
	 * no need to average days, so this should never be
	 * called with eSW_Day.
	 * Enter this routine just after the summary period
	 * is completed, so the current week and month will be
	 * one greater than the period being summarized.
	 */
	/* 	20091015 (drs) ppt is divided into rain and snow and all three values are output into precip */
	SW_SOILWAT_OUTPUTS *savg = NULL, *ssumof = NULL;
	SW_WEATHER_OUTPUTS *wavg = NULL, *wsumof = NULL;
	SW_VEGPROD_OUTPUTS *vpavg = NULL, *vpsumof = NULL;
	TimeInt curr_pd = 0;
	RealD div = 0.; /* if sumtype=AVG, days in period; if sumtype=SUM, 1 */
	OutKey k;
	LyrIndex i;
	int j;

	if (otyp == eVES)
		LogError(logfp, LOGFATAL, "Invalid object type 'eVES' in 'average_for()'.");

	ForEachOutKey(k)
	{
		if (!SW_Output[k].use) {
			continue;
		}

		switch (pd)
		{
		case eSW_Week:
			curr_pd = (SW_Model.week + 1) - tOffset;
			savg = (SW_SOILWAT_OUTPUTS *) &SW_Soilwat.wkavg;
			ssumof = (SW_SOILWAT_OUTPUTS *) &SW_Soilwat.wksum;
			wavg = (SW_WEATHER_OUTPUTS *) &SW_Weather.wkavg;
			wsumof = (SW_WEATHER_OUTPUTS *) &SW_Weather.wksum;
			vpavg = (SW_VEGPROD_OUTPUTS *) &SW_VegProd.wkavg;
			vpsumof = (SW_VEGPROD_OUTPUTS *) &SW_VegProd.wksum;
			div = (bFlush_output) ? SW_Model.lastdoy % WKDAYS : WKDAYS;
			break;

		case eSW_Month:
			curr_pd = (SW_Model.month + 1) - tOffset;
			savg = (SW_SOILWAT_OUTPUTS *) &SW_Soilwat.moavg;
			ssumof = (SW_SOILWAT_OUTPUTS *) &SW_Soilwat.mosum;
			wavg = (SW_WEATHER_OUTPUTS *) &SW_Weather.moavg;
			wsumof = (SW_WEATHER_OUTPUTS *) &SW_Weather.mosum;
			vpavg = (SW_VEGPROD_OUTPUTS *) &SW_VegProd.moavg;
			vpsumof = (SW_VEGPROD_OUTPUTS *) &SW_VegProd.mosum;
			div = Time_days_in_month(SW_Model.month - tOffset);
			break;

		case eSW_Year:
			curr_pd = SW_Output[k].first;
			savg = (SW_SOILWAT_OUTPUTS *) &SW_Soilwat.yravg;
			ssumof = (SW_SOILWAT_OUTPUTS *) &SW_Soilwat.yrsum;
			wavg = (SW_WEATHER_OUTPUTS *) &SW_Weather.yravg;
			wsumof = (SW_WEATHER_OUTPUTS *) &SW_Weather.yrsum;
			vpavg = (SW_VEGPROD_OUTPUTS *) &SW_VegProd.yravg;
			vpsumof = (SW_VEGPROD_OUTPUTS *) &SW_VegProd.yrsum;
			div = SW_Output[k].last - SW_Output[k].first + 1;
			break;

		default:
			LogError(logfp, LOGFATAL, "Programmer: Invalid period in average_for().");
		} /* end switch(pd) */

		if (SW_Output[k].myobj != otyp
				|| curr_pd < SW_Output[k].first
				|| curr_pd > SW_Output[k].last)
			continue;

		if (SW_Output[k].sumtype == eSW_Sum)
			div = 1.;

		/* notice that all valid keys are in this switch */
		switch (k)
		{

		case eSW_Temp:
			wavg->temp_max = wsumof->temp_max / div;
			wavg->temp_min = wsumof->temp_min / div;
			wavg->temp_avg = wsumof->temp_avg / div;
			//added surfaceTemp for avg operation
			wavg->surfaceTemp = wsumof->surfaceTemp / div;
			break;

		case eSW_Precip:
			wavg->ppt = wsumof->ppt / div;
			wavg->rain = wsumof->rain / div;
			wavg->snow = wsumof->snow / div;
			wavg->snowmelt = wsumof->snowmelt / div;
			wavg->snowloss = wsumof->snowloss / div;
			break;

		case eSW_SoilInf:
			wavg->soil_inf = wsumof->soil_inf / div;
			break;

		case eSW_Runoff:
			wavg->snowRunoff = wsumof->snowRunoff / div;
			wavg->surfaceRunoff = wsumof->surfaceRunoff / div;
			wavg->surfaceRunon = wsumof->surfaceRunon / div;
			break;

		case eSW_SoilTemp:
			ForEachSoilLayer(i)
				savg->sTemp[i] =
						(SW_Output[k].sumtype == eSW_Fnl) ?
								SW_Soilwat.sTemp[i] :
								ssumof->sTemp[i] / div;
			break;

		case eSW_VWCBulk:
			ForEachSoilLayer(i)
				/* vwcBulk at this point is identical to swcBulk */
				savg->vwcBulk[i] =
						(SW_Output[k].sumtype == eSW_Fnl) ?
								SW_Soilwat.swcBulk[Yesterday][i] :
								ssumof->vwcBulk[i] / div;
			break;

		case eSW_VWCMatric:
			ForEachSoilLayer(i)
				/* vwcMatric at this point is identical to swcBulk */
				savg->vwcMatric[i] =
						(SW_Output[k].sumtype == eSW_Fnl) ?
								SW_Soilwat.swcBulk[Yesterday][i] :
								ssumof->vwcMatric[i] / div;
			break;

		case eSW_SWCBulk:
			ForEachSoilLayer(i)
				savg->swcBulk[i] =
						(SW_Output[k].sumtype == eSW_Fnl) ?
								SW_Soilwat.swcBulk[Yesterday][i] :
								ssumof->swcBulk[i] / div;
			break;

		case eSW_SWPMatric:
			ForEachSoilLayer(i)
				/* swpMatric at this point is identical to swcBulk */
				savg->swpMatric[i] =
						(SW_Output[k].sumtype == eSW_Fnl) ?
								SW_Soilwat.swcBulk[Yesterday][i] :
								ssumof->swpMatric[i] / div;
			break;

		case eSW_SWABulk:
			ForEachSoilLayer(i)
				savg->swaBulk[i] =
						(SW_Output[k].sumtype == eSW_Fnl) ?
								fmax(
										SW_Soilwat.swcBulk[Yesterday][i]
												- SW_Site.lyr[i]->swcBulk_wiltpt,
										0.) :
								ssumof->swaBulk[i] / div;
			break;

		case eSW_SWAMatric: /* swaMatric at this point is identical to swaBulk */
			ForEachSoilLayer(i)
				savg->swaMatric[i] =
						(SW_Output[k].sumtype == eSW_Fnl) ?
								fmax(
										SW_Soilwat.swcBulk[Yesterday][i]
												- SW_Site.lyr[i]->swcBulk_wiltpt,
										0.) :
								ssumof->swaMatric[i] / div;
			break;

		case eSW_SWA:
			ForEachSoilLayer(i){
				ForEachVegType(j){
					savg->SWA_VegType[j][i] =
							(SW_Output[k].sumtype == eSW_Fnl) ?
									SW_Soilwat.dSWA_repartitioned_sum[j][i] :
									ssumof->SWA_VegType[j][i] / div;
					//printf("swa: %f\n", savg->SWA_VegType[j][i]);
				}
			}
			break;

		case eSW_DeepSWC:
			savg->deep =
					(SW_Output[k].sumtype == eSW_Fnl) ?
							SW_Soilwat.swcBulk[Yesterday][SW_Site.deep_lyr] :
							ssumof->deep / div;
			break;

		case eSW_SurfaceWater:
			savg->surfaceWater = ssumof->surfaceWater / div;
			break;

		case eSW_Transp:
			ForEachSoilLayer(i)
			{
				savg->transp_total[i] = ssumof->transp_total[i] / div;
				savg->transp[SW_TREES][i] = ssumof->transp[SW_TREES][i] / div;
				savg->transp[SW_SHRUB][i] = ssumof->transp[SW_SHRUB][i] / div;
				savg->transp[SW_FORBS][i] = ssumof->transp[SW_FORBS][i] / div;
				savg->transp[SW_GRASS][i] = ssumof->transp[SW_GRASS][i] / div;
			}
			break;

		case eSW_EvapSoil:
			ForEachEvapLayer(i)
				savg->evap[i] = ssumof->evap[i] / div;
			break;

		case eSW_EvapSurface:
			savg->total_evap = ssumof->total_evap / div;
			savg->evap_veg[SW_TREES] = ssumof->evap_veg[SW_TREES] / div;
			savg->evap_veg[SW_SHRUB] = ssumof->evap_veg[SW_SHRUB] / div;
			savg->evap_veg[SW_FORBS] = ssumof->evap_veg[SW_FORBS] / div;
			savg->evap_veg[SW_GRASS] = ssumof->evap_veg[SW_GRASS] / div;
			savg->litter_evap = ssumof->litter_evap / div;
			savg->surfaceWater_evap = ssumof->surfaceWater_evap / div;
			break;

		case eSW_Interception:
			savg->total_int = ssumof->total_int / div;
			savg->int_veg[SW_TREES] = ssumof->int_veg[SW_TREES] / div;
			savg->int_veg[SW_SHRUB] = ssumof->int_veg[SW_SHRUB] / div;
			savg->int_veg[SW_FORBS] = ssumof->int_veg[SW_FORBS] / div;
			savg->int_veg[SW_GRASS] = ssumof->int_veg[SW_GRASS] / div;
			savg->litter_int = ssumof->litter_int / div;
			break;

		case eSW_AET:
			savg->aet = ssumof->aet / div;
			break;

		case eSW_LyrDrain:
			for (i = 0; i < SW_Site.n_layers - 1; i++)
				savg->lyrdrain[i] = ssumof->lyrdrain[i] / div;
			break;

		case eSW_HydRed:
			ForEachSoilLayer(i)
			{
				savg->hydred_total[i] = ssumof->hydred_total[i] / div;
				savg->hydred[SW_TREES][i] = ssumof->hydred[SW_TREES][i] / div;
				savg->hydred[SW_SHRUB][i] = ssumof->hydred[SW_SHRUB][i] / div;
				savg->hydred[SW_FORBS][i] = ssumof->hydred[SW_FORBS][i] / div;
				savg->hydred[SW_GRASS][i] = ssumof->hydred[SW_GRASS][i] / div;
			}
			break;

		case eSW_PET:
			savg->pet = ssumof->pet / div;
			break;

		case eSW_WetDays:
			ForEachSoilLayer(i)
				savg->wetdays[i] = ssumof->wetdays[i] / div;
			break;

		case eSW_SnowPack:
			savg->snowpack = ssumof->snowpack / div;
			savg->snowdepth = ssumof->snowdepth / div;
			break;

		case eSW_Estab: /* do nothing, no averaging required */
			break;

		case eSW_CO2Effects:
			ForEachVegType(i) {
				vpavg->veg[i].biomass = vpsumof->veg[i].biomass / div;
				vpavg->veg[i].biolive = vpsumof->veg[i].biolive / div;
			}
			break;

		default:
			LogError(logfp, LOGFATAL, "PGMR: Invalid key in average_for(%s)", key2str[k]);
		}

	} /* end ForEachKey */
}

static void collect_sums(ObjType otyp, OutPeriod op)
{
	/* --------------------------------------------------- */

	SW_SOILWAT *s = &SW_Soilwat;
	SW_SOILWAT_OUTPUTS *ssum = NULL;
	SW_WEATHER *w = &SW_Weather;
	SW_WEATHER_OUTPUTS *wsum = NULL;
	SW_VEGESTAB *v = &SW_VegEstab; /* vegestab only gets summed yearly */
	SW_VEGESTAB_OUTPUTS *vsum = NULL;
	SW_VEGPROD *vp = &SW_VegProd;
	SW_VEGPROD_OUTPUTS *vpsum = NULL;

	TimeInt pd = 0;
	OutKey k;
	int i;
	Bool use_KeyPeriodCombo;

	switch (op)
	{
	case eSW_Day:
		pd = SW_Model.doy;
		break;
	case eSW_Week:
		pd = SW_Model.week + 1;
		break;
	case eSW_Month:
		pd = SW_Model.month + 1;
		break;
	case eSW_Year:
		pd = SW_Model.doy;
		break;
	default:
		LogError(logfp, LOGFATAL, "PGMR: Invalid outperiod in collect_sums()");
	}


	ForEachOutKey(k)
	{
		if (otyp != SW_Output[k].myobj || !SW_Output[k].use)
			continue;

		/* determine whether output period op is active for current output key k */
		use_KeyPeriodCombo = swFALSE;
		for (i = 0; i < used_OUTNPERIODS; i++)
		{
			if (op == timeSteps[k][i])
			{
				use_KeyPeriodCombo = swTRUE;
				break;
			}
		}

		if (use_KeyPeriodCombo && pd >= SW_Output[k].first && pd <= SW_Output[k].last)
		{
			switch (otyp)
			{
			case eSWC:
				switch (op) {
					case eSW_Day:
						ssum = &s->dysum;
						break;
					case eSW_Week:
						ssum = &s->wksum;
						break;
					case eSW_Month:
						ssum = &s->mosum;
						break;
					case eSW_Year:
						ssum = &s->yrsum;
						break;
				}
				sumof_swc(s, ssum, k);
				break;

			case eWTH:
				switch (op) {
					case eSW_Day:
						wsum = &w->dysum;
						break;
					case eSW_Week:
						wsum = &w->wksum;
						break;
					case eSW_Month:
						wsum = &w->mosum;
						break;
					case eSW_Year:
						wsum = &w->yrsum;
						break;
				}
				sumof_wth(w, wsum, k);
				break;

			case eVES:
				switch (op) {
					case eSW_Day:
						vsum = NULL;
						break;
					case eSW_Week:
						vsum = NULL;
						break;
					case eSW_Month:
						vsum = NULL;
						break;
					case eSW_Year:
						vsum = &v->yrsum; /* yearly, y'see */
						break;
				}
				sumof_ves(v, vsum, k);
				break;

			case eVPD:
				switch (op) {
					case eSW_Day:
						vpsum = &vp->dysum;
						break;
					case eSW_Week:
						vpsum = &vp->wksum;
						break;
					case eSW_Month:
						vpsum = &vp->mosum;
						break;
					case eSW_Year:
						vpsum = &vp->yrsum;
						break;
				}
				sumof_vpd(vp, vpsum, k);
				break;

			default:
				break;
			}
		}

	} /* end ForEachOutKey */
}




/* =================================================== */
/* =================================================== */
/*             Public Function Definitions             */
/* --------------------------------------------------- */

#ifndef RSOILWAT
void get_outstrleader(TimeInt pd)
{
	/* --------------------------------------------------- */
	/* this is called from each of the remaining get_ funcs
	 * to set up the first (date) columns of the output
	 * string.  It's the same for all and easier to put in
	 * one place.
	 * Periodic output for Month and/or Week are actually
	 * printing for the PREVIOUS month or week.
	 * Also, see note on test value in _write_today() for
	 * explanation of the +1.
	 */
	switch (pd)
	{
	case eSW_Day:
		sprintf(sw_outstr, "%d%c%d", SW_Model.simyear, _Sep, SW_Model.doy);
		break;
	case eSW_Week:
		sprintf(sw_outstr, "%d%c%d", SW_Model.simyear, _Sep,
				(SW_Model.week + 1) - tOffset);
		break;
	case eSW_Month:
		sprintf(sw_outstr, "%d%c%d", SW_Model.simyear, _Sep,
				(SW_Model.month + 1) - tOffset);
		break;
	case eSW_Year:
		sprintf(sw_outstr, "%d", SW_Model.simyear);
		break;
	}
}
#endif


void get_none(OutPeriod pd) // not static because other `get_XXX` are not
{
	/* --------------------------------------------------- */
	/* output routine for quantities that aren't yet implemented
	 * this just gives the main output loop something to call,
	 * rather than an empty pointer.
	 */
	sw_outstr[0] = '\0';
	if (pd) {}
}


void SW_OUT_construct(void)
{
	/* =================================================== */
	OutKey k;
	SW_SOILWAT_OUTPUTS *s = NULL;
	LyrIndex i;
	int j;

	// for use in creating the column headers for the output files
  SW_File_Status.finalValue_dy = -1;
  SW_File_Status.finalValue_wk = -1;
  SW_File_Status.finalValue_mo = -1;
  SW_File_Status.finalValue_yr = -1;

  SW_File_Status.lastMonth = 0;
  SW_File_Status.lastWeek = 0;

	SW_File_Status.make_soil = 0;
  SW_File_Status.make_regular = 0;

	SW_File_Status.col_status_dy = 0;
	SW_File_Status.col_status_wk = 0;
	SW_File_Status.col_status_mo = 0;
	SW_File_Status.col_status_yr = 0;

	ForEachSoilLayer(i){
		ForEachVegType(j){
			s->SWA_VegType[j][i] = 0.;
		}
	}

	/* note that an initializer that is called during
	 * execution (better called clean() or something)
	 * will need to free all allocated memory first
	 * before clearing structure.
	 */
	ForEachOutKey(k)
	{
		if (!isnull(SW_Output[k].outfile))
		{	//Clear memory before setting it
			Mem_Free(SW_Output[k].outfile);
			SW_Output[k].outfile = NULL;
		}
	}
	memset(&SW_Output, 0, sizeof(SW_Output));

	/* attach the printing functions for each output
	 * quantity to the appropriate element in the
	 * output structure.  Using a loop makes it convenient
	 * to simply add a line as new quantities are
	 * implemented and leave the default case for every
	 * thing else.
	 */

	ForEachOutKey(k)
	{
#ifdef RSOILWAT
		SW_Output[k].yr_row = 0;
		SW_Output[k].mo_row = 0;
		SW_Output[k].wk_row = 0;
		SW_Output[k].dy_row = 0;
#endif

		switch (k)
		{
		case eSW_Temp:
			SW_Output[k].pfunc = (void (*)(OutPeriod)) get_temp;
			break;
		case eSW_Precip:
			SW_Output[k].pfunc = (void (*)(OutPeriod)) get_precip;
			break;
		case eSW_VWCBulk:
			SW_Output[k].pfunc = (void (*)(OutPeriod)) get_vwcBulk;
			break;
		case eSW_VWCMatric:
			SW_Output[k].pfunc = (void (*)(OutPeriod)) get_vwcMatric;
			break;
		case eSW_SWCBulk:
			SW_Output[k].pfunc = (void (*)(OutPeriod)) get_swcBulk;
			break;
		case eSW_SWPMatric:
			SW_Output[k].pfunc = (void (*)(OutPeriod)) get_swpMatric;
			break;
		case eSW_SWABulk:
			SW_Output[k].pfunc = (void (*)(OutPeriod)) get_swaBulk;
			break;
		case eSW_SWAMatric:
			SW_Output[k].pfunc = (void (*)(OutPeriod)) get_swaMatric;
			break;
		case eSW_SWA:
			SW_Output[k].pfunc = (void (*)(OutPeriod)) get_swa;
			break;
		case eSW_SurfaceWater:
			SW_Output[k].pfunc = (void (*)(OutPeriod)) get_surfaceWater;
			break;
		case eSW_Runoff:
			SW_Output[k].pfunc = (void (*)(OutPeriod)) get_runoffrunon;
			break;
		case eSW_Transp:
			SW_Output[k].pfunc = (void (*)(OutPeriod)) get_transp;
			break;
		case eSW_EvapSoil:
			SW_Output[k].pfunc = (void (*)(OutPeriod)) get_evapSoil;
			break;
		case eSW_EvapSurface:
			SW_Output[k].pfunc = (void (*)(OutPeriod)) get_evapSurface;
			break;
		case eSW_Interception:
			SW_Output[k].pfunc = (void (*)(OutPeriod)) get_interception;
			break;
		case eSW_SoilInf:
			SW_Output[k].pfunc = (void (*)(OutPeriod)) get_soilinf;
			break;
		case eSW_LyrDrain:
			SW_Output[k].pfunc = (void (*)(OutPeriod)) get_lyrdrain;
			break;
		case eSW_HydRed:
			SW_Output[k].pfunc = (void (*)(OutPeriod)) get_hydred;
			break;
		case eSW_AET:
			SW_Output[k].pfunc = (void (*)(OutPeriod)) get_aet;
			break;
		case eSW_PET:
			SW_Output[k].pfunc = (void (*)(OutPeriod)) get_pet;
			break;
		case eSW_WetDays:
			SW_Output[k].pfunc = (void (*)(OutPeriod)) get_wetdays;
			break;
		case eSW_SnowPack:
			SW_Output[k].pfunc = (void (*)(OutPeriod)) get_snowpack;
			break;
		case eSW_DeepSWC:
			SW_Output[k].pfunc = (void (*)(OutPeriod)) get_deepswc;
			break;
		case eSW_SoilTemp:
			SW_Output[k].pfunc = (void (*)(OutPeriod)) get_soiltemp;
			break;
		case eSW_Estab:
			SW_Output[k].pfunc = (void (*)(OutPeriod)) get_estab;
			break;
		case eSW_CO2Effects:
			SW_Output[k].pfunc = (void (*)(OutPeriod)) get_co2effects;
			break;
		default:
			SW_Output[k].pfunc = (void (*)(OutPeriod)) get_none;
			break;

		}
	}
	bFlush_output = swFALSE;
	tOffset = 1;
}


void SW_OUT_set_ncol(void) {
	int tLayers = SW_Site.n_layers;

	ncol_OUT[eSW_AllWthr] = 0;
	ncol_OUT[eSW_Temp] = 4;
	ncol_OUT[eSW_Precip] = 5;
	ncol_OUT[eSW_SoilInf] = 1;
	ncol_OUT[eSW_Runoff] = 4;
	ncol_OUT[eSW_AllH2O] = 0;
	ncol_OUT[eSW_VWCBulk] = tLayers;
	ncol_OUT[eSW_VWCMatric] = tLayers;
	ncol_OUT[eSW_SWCBulk] = tLayers;
	ncol_OUT[eSW_SWABulk] = tLayers;
	ncol_OUT[eSW_SWAMatric] = tLayers;
	ncol_OUT[eSW_SWA] = tLayers * NVEGTYPES;
	ncol_OUT[eSW_SWPMatric] = tLayers;
	ncol_OUT[eSW_SurfaceWater] = 1;
	ncol_OUT[eSW_Transp] = tLayers * (NVEGTYPES + 1); // NVEGTYPES plus totals
	ncol_OUT[eSW_EvapSoil] = SW_Site.n_evap_lyrs;
	ncol_OUT[eSW_EvapSurface] = NVEGTYPES + 3; // NVEGTYPES plus totals, litter, surface water
	ncol_OUT[eSW_Interception] = NVEGTYPES + 2; // NVEGTYPES plus totals, litter
	ncol_OUT[eSW_LyrDrain] = tLayers - 1;
	ncol_OUT[eSW_HydRed] = tLayers * (NVEGTYPES + 1); // NVEGTYPES plus totals
	ncol_OUT[eSW_ET] = 0;
	ncol_OUT[eSW_AET] = 1;
	ncol_OUT[eSW_PET] = 1;
	ncol_OUT[eSW_WetDays] = tLayers;
	ncol_OUT[eSW_SnowPack] = 2;
	ncol_OUT[eSW_DeepSWC] = 1;
	ncol_OUT[eSW_SoilTemp] = tLayers;
	ncol_OUT[eSW_AllVeg] = 0;
	ncol_OUT[eSW_Estab] = SW_VegEstab.count;
	ncol_OUT[eSW_CO2Effects] = 2 * (NVEGTYPES + 1) + 2 * NVEGTYPES;

}

#ifdef RSOILWAT
// this function should be really used by all applications for consistent naming of output
void SW_OUT_set_colnames(void) {
	int i, j, tLayers = SW_Site.n_layers;
  #ifdef SWDEBUG
  int debug = 0;
  #endif

	char ctemp[50];
	const char *Layers_names[MAX_LAYERS] = { "Lyr_1", "Lyr_2", "Lyr_3", "Lyr_4", "Lyr_5",
		"Lyr_6", "Lyr_7", "Lyr_8", "Lyr_9", "Lyr_10", "Lyr_11", "Lyr_12", "Lyr_13", "Lyr_14",
		"Lyr_15", "Lyr_16", "Lyr_17", "Lyr_18", "Lyr_19", "Lyr_20", "Lyr_21", "Lyr_22",
		"Lyr_23", "Lyr_24", "Lyr_25"};
	const char *cnames_VegTypes[NVEGTYPES + 2] = { "total", "tree", "shrub", "forbs",
		"grass", "litter" };

	const char *cnames_eSW_Temp[] = { "max_C", "min_C", "avg_C", "surfaceTemp_C" };
	const char *cnames_eSW_Precip[] = { "ppt", "rain", "snow_fall", "snowmelt", "snowloss" };
	const char *cnames_eSW_SoilInf[] = { "soil_inf" };
	const char *cnames_eSW_Runoff[] = { "net", "ponded_runoff", "snowmelt_runoff", "ponded_runon" };
	const char *cnames_eSW_SurfaceWater[] = { "surfaceWater_cm" };
	const char *cnames_add_eSW_EvapSurface[] = { "evap_surfaceWater" };
	const char *cnames_eSW_AET[] = { "evapotr_cm" };
	const char *cnames_eSW_PET[] = { "pet_cm" };
	const char *cnames_eSW_SnowPack[] = { "snowpackWaterEquivalent_cm", "snowdepth_cm" };
	const char *cnames_eSW_DeepSWC[] = { "lowLayerDrain_cm" };
	const char *cnames_eSW_CO2Effects[] = { // uses different order of vegtypes than others ...
		"GrassBiomass", "ShrubBiomass", "TreeBiomass", "ForbBiomass", "TotalBiomass",
		"GrassBiolive", "ShrubBiolive", "TreeBiolive", "ForbBiolive", "TotalBiolive",
		"GrassBioMult", "ShrubBioMult", "TreeBioMult", "ForbBioMult",
		"GrassWUEMult", "ShrubWUEMult", "TreeWUEMult", "ForbWUEMult" };

	#ifdef SWDEBUG
	if (debug) swprintf("SW_OUT_set_colnames: set columns for 'eSW_Temp' ...");
	#endif
	for (i = 0; i < ncol_OUT[eSW_Temp]; i++) {
		colnames_OUT[eSW_Temp][i] = Str_Dup(cnames_eSW_Temp[i]);
	}
	#ifdef SWDEBUG
	if (debug) swprintf(" 'eSW_Precip' ...");
	#endif
	for (i = 0; i < ncol_OUT[eSW_Precip]; i++) {
		colnames_OUT[eSW_Precip][i] = Str_Dup(cnames_eSW_Precip[i]);
	}
	#ifdef SWDEBUG
	if (debug) swprintf(" 'eSW_SoilInf' ...");
	#endif
	for (i = 0; i < ncol_OUT[eSW_SoilInf]; i++) {
		colnames_OUT[eSW_SoilInf][i] = Str_Dup(cnames_eSW_SoilInf[i]);
	}
	#ifdef SWDEBUG
	if (debug) swprintf(" 'eSW_Runoff' ...");
	#endif
	for (i = 0; i < ncol_OUT[eSW_Runoff]; i++) {
		colnames_OUT[eSW_Runoff][i] = Str_Dup(cnames_eSW_Runoff[i]);
	}
	#ifdef SWDEBUG
	if (debug) swprintf(" 'eSW_VWCBulk' ...");
	#endif
	for (i = 0; i < ncol_OUT[eSW_VWCBulk]; i++) {
		colnames_OUT[eSW_VWCBulk][i] = Str_Dup(Layers_names[i]);
	}
	#ifdef SWDEBUG
	if (debug) swprintf(" 'eSW_VWCMatric' ...");
	#endif
	for (i = 0; i < ncol_OUT[eSW_VWCMatric]; i++) {
		colnames_OUT[eSW_VWCMatric][i] = Str_Dup(Layers_names[i]);
	}
	#ifdef SWDEBUG
	if (debug) swprintf(" 'eSW_SWCBulk' ...");
	#endif
	for (i = 0; i < ncol_OUT[eSW_SWCBulk]; i++) {
		colnames_OUT[eSW_SWCBulk][i] = Str_Dup(Layers_names[i]);
	}
	#ifdef SWDEBUG
	if (debug) swprintf(" 'eSW_SWABulk' ...");
	#endif
	for (i = 0; i < ncol_OUT[eSW_SWABulk]; i++) {
		colnames_OUT[eSW_SWABulk][i] = Str_Dup(Layers_names[i]);
	}
	#ifdef SWDEBUG
	if (debug) swprintf(" 'eSW_SWA' ...");
	#endif
	for (i = 0; i < tLayers; i++) {
		for (j = 0; j < NVEGTYPES; j++) {
			strcpy(ctemp, "swa_");
			strcat(ctemp, cnames_VegTypes[j+1]); // j+1 since no total column for swa.
			strcat(ctemp, "_");
			strcat(ctemp, Layers_names[i]);

			colnames_OUT[eSW_SWA][i + j * tLayers] = Str_Dup(ctemp);
		}
	}
	#ifdef SWDEBUG
	if (debug) swprintf(" 'eSW_SWAMatric' ...");
	#endif
	for (i = 0; i < ncol_OUT[eSW_SWAMatric]; i++) {
		colnames_OUT[eSW_SWAMatric][i] = Str_Dup(Layers_names[i]);
	}
	#ifdef SWDEBUG
	if (debug) swprintf(" 'eSW_SWPMatric' ...");
	#endif
	for (i = 0; i < ncol_OUT[eSW_SWPMatric]; i++) {
		colnames_OUT[eSW_SWPMatric][i] = Str_Dup(Layers_names[i]);
	}
	#ifdef SWDEBUG
	if (debug) swprintf(" 'eSW_SurfaceWater' ...");
	#endif
	for (i = 0; i < ncol_OUT[eSW_SurfaceWater]; i++) {
		colnames_OUT[eSW_SurfaceWater][i] = Str_Dup(cnames_eSW_SurfaceWater[i]);
	}
	#ifdef SWDEBUG
	if (debug) swprintf(" 'eSW_Transp' ...");
	#endif
	for (i = 0; i < tLayers; i++) {
		for (j = 0; j < NVEGTYPES + 1; j++) {
			strcpy(ctemp, "transp_");
			strcat(ctemp, cnames_VegTypes[j]);
			strcat(ctemp, "_");
			strcat(ctemp, Layers_names[i]);

			colnames_OUT[eSW_Transp][i + j * tLayers] = Str_Dup(ctemp);
		}
	}
	#ifdef SWDEBUG
	if (debug) swprintf(" 'eSW_EvapSoil' ...");
	#endif
	for (i = 0; i < ncol_OUT[eSW_EvapSoil]; i++) {
		colnames_OUT[eSW_EvapSoil][i] = Str_Dup(Layers_names[i]);
	}
	#ifdef SWDEBUG
	if (debug) swprintf(" 'eSW_EvapSurface' ...");
	#endif
	for (i = 0; i < NVEGTYPES + 2; i++) {
		strcpy(ctemp, "evap_");
		strcat(ctemp, cnames_VegTypes[i]);
		colnames_OUT[eSW_EvapSurface][i] = Str_Dup(ctemp);
	}
	for (i = 0; i < ncol_OUT[eSW_EvapSurface] - (NVEGTYPES + 2); i++) {
		colnames_OUT[eSW_EvapSurface][NVEGTYPES + 2 + i] = Str_Dup(cnames_add_eSW_EvapSurface[i]);
	}
	#ifdef SWDEBUG
	if (debug) swprintf(" 'eSW_Interception' ...");
	#endif
	for (i = 0; i < NVEGTYPES + 2; i++) {
		strcpy(ctemp, "int_");
		strcat(ctemp, cnames_VegTypes[i]);
		colnames_OUT[eSW_Interception][i] = Str_Dup(ctemp);
	}
	#ifdef SWDEBUG
	if (debug) swprintf(" 'eSW_LyrDrain' ...");
	#endif
	for (i = 0; i < ncol_OUT[eSW_LyrDrain]; i++) {
		colnames_OUT[eSW_LyrDrain][i] = Str_Dup(Layers_names[i]);
	}
	#ifdef SWDEBUG
	if (debug) swprintf(" 'eSW_HydRed' ...");
	#endif
	for (i = 0; i < tLayers; i++) {
		for (j = 0; j < NVEGTYPES + 1; j++) {
			strcpy(ctemp, cnames_VegTypes[j]);
			strcat(ctemp, "_");
			strcat(ctemp, Layers_names[i]);
			colnames_OUT[eSW_HydRed][i + j * tLayers] = Str_Dup(ctemp);
		}
	}
	#ifdef SWDEBUG
	if (debug) swprintf(" 'eSW_AET' ...");
	#endif
	for (i = 0; i < ncol_OUT[eSW_AET]; i++) {
		colnames_OUT[eSW_AET][i] = Str_Dup(cnames_eSW_AET[i]);
	}
	#ifdef SWDEBUG
	if (debug) swprintf(" 'eSW_PET' ...");
	#endif
	for (i = 0; i < ncol_OUT[eSW_PET]; i++) {
		colnames_OUT[eSW_PET][i] = Str_Dup(cnames_eSW_PET[i]);
	}
	#ifdef SWDEBUG
	if (debug) swprintf(" 'eSW_WetDays' ...");
	#endif
	for (i = 0; i < ncol_OUT[eSW_WetDays]; i++) {
		colnames_OUT[eSW_WetDays][i] = Str_Dup(Layers_names[i]);
	}
	#ifdef SWDEBUG
	if (debug) swprintf(" 'eSW_SnowPack' ...");
	#endif
	for (i = 0; i < ncol_OUT[eSW_SnowPack]; i++) {
		colnames_OUT[eSW_SnowPack][i] = Str_Dup(cnames_eSW_SnowPack[i]);
	}
	#ifdef SWDEBUG
	if (debug) swprintf(" 'eSW_DeepSWC' ...");
	#endif
	for (i = 0; i < ncol_OUT[eSW_DeepSWC]; i++) {
		colnames_OUT[eSW_DeepSWC][i] = Str_Dup(cnames_eSW_DeepSWC[i]);
	}
	#ifdef SWDEBUG
	if (debug) swprintf(" 'eSW_SoilTemp' ...");
	#endif
	for (i = 0; i < ncol_OUT[eSW_SoilTemp]; i++) {
		colnames_OUT[eSW_SoilTemp][i] = Str_Dup(Layers_names[i]);
	}
	#ifdef SWDEBUG
	if (debug) swprintf(" 'eSW_Estab' ...");
	#endif
	for (i = 0; i < ncol_OUT[eSW_Estab]; i++) {
		colnames_OUT[eSW_Estab][i] = Str_Dup(SW_VegEstab.parms[i]->sppname);
	}
	#ifdef SWDEBUG
	if (debug) swprintf(" 'eSW_CO2Effects' ...");
	#endif
	for (i = 0; i < ncol_OUT[eSW_CO2Effects]; i++) {
		colnames_OUT[eSW_CO2Effects][i] = Str_Dup(cnames_eSW_CO2Effects[i]);
	}
	#ifdef SWDEBUG
	if (debug) swprintf(" completed.\n");
	#endif
}
#endif

void SW_OUT_new_year(void)
{
	/* =================================================== */
	/* reset the terminal output days each year  */

	OutKey k;

	ForEachOutKey(k)
	{
		if (!SW_Output[k].use)
			continue;

		if (SW_Output[k].first_orig <= SW_Model.firstdoy)
			SW_Output[k].first = SW_Model.firstdoy;
		else
			SW_Output[k].first = SW_Output[k].first_orig;

		if (SW_Output[k].last_orig >= SW_Model.lastdoy)
			SW_Output[k].last = SW_Model.lastdoy;
		else
			SW_Output[k].last = SW_Output[k].last_orig;

	}

}

/** Read output setup from file `outsetup.in`.

    Output can be generated for four different time steps: daily (DY), weekly (WK),
    monthly (MO), and yearly (YR).

    We have two options to specify time steps:
        - The same time step(s) for every output: Add a line with the tag `TIMESTEP`,
          e.g., `TIMESTEP dy mo yr` will generate daily, monthly, and yearly output for
          every output variable. If there is a line with this tag, then this will
          override information provided in the column `PERIOD`.
        - A different time step for each output: Specify the time step in the column
          `PERIOD` for each output variable. Note: only one time step per output variable
          can be specified.
 */
void SW_OUT_read(void)
{
	/* =================================================== */
	/* read input file for output parameter setup info.
	 * 5-Nov-01 -- now disregard the specified file name's
	 *             extension and instead use the specified
	 *             period as the extension.
	 * 10-May-02 - Added conditional for interfacing to STEPPE.
	 *             We want no output when running from STEPPE
	 *             so the code to open the file is blocked out.
	 *             In fact, the only keys to process are
	 *             TRANSP, PRECIP, and TEMP.
	 */
	FILE *f;
	OutKey k;
	int x, i, itemno;

	/* these dims come from the orig format str */
	/* except for the uppercase space. */
	char ext[10];
	char timeStep[SW_OUTNPERIODS][10], // matrix to capture all the periods entered in outsetup.in
			keyname[50], upkey[50], /* space for uppercase conversion */
			sumtype[4], upsum[4], period[10], /* should be 2 chars, but we don't want overflow from user typos */
			last[4], /* last doy for output, if "end", ==366 */
			outfile[MAX_FILENAMESIZE];
	int outfile_periods[4] = {0};
	int first; /* first doy for output */

	MyFileName = SW_F_name(eOutput);
	f = OpenFile(MyFileName, "r");
	itemno = 0;

	_Sep = ','; /* default in case it doesn't show up in the file */
	used_OUTNPERIODS = 1; // if 'TIMESTEP' is not specified in input file, then only one time step = period can be specified
	useTimeStep = 0;

	while (GetALine(f, inbuf))
	{
		itemno++; /* note extra lines will cause an error */

		x = sscanf(inbuf, "%s %s %s %d %s %s", keyname, sumtype, period, &first,
				last, outfile);

		// checking which files need to be created. If only soil values desired then dont want
		// to create regular files with no values.
		if((strcmp(keyname, "VWCBULK")==0 || strcmp(keyname, "VWCMATRIC")==0 || strcmp(keyname, "SWCBULK")==0
			|| strcmp(keyname, "SWABULK")==0
			|| strcmp(keyname, "EVAPSOIL")==0 || strcmp(keyname, "TRANSP")==0 || strcmp(keyname, "WETDAY")==0
			|| strcmp(keyname, "LYRDRAIN")==0 || strcmp(keyname, "SOILTEMP")==0 || strcmp(keyname, "HYDRED")==0
			|| strcmp(keyname, "SWAMATRIC")==0 || strcmp(keyname, "SWPMATRIC")==0 || strcmp(keyname, "SWA")==0))
		{
			SW_File_Status.make_soil = 1;

			// set use_SWA to TRUE if defined.
			// Used in SW_Control to run the functions to get the recalculated values only if SWA is used
			// This function is run prior to the control functions so thats why it is here.
			if(strcmp(keyname, "SWA")==0)
				SW_VegProd.use_SWA = swTRUE;
		}
		else if (strcmp(keyname, "TIMESTEP")==0 || strcmp(keyname, "OUTSEP")==0){

		}
		else
			SW_File_Status.make_regular = 1;

		// condition to read in the TIMESTEP line in outsetup.in
		if (Str_CompareI(keyname, (char *)"TIMESTEP") == 0)
		{
			// need to rescan the line because you are looking for all strings, unlike the original scan
			used_OUTNPERIODS = sscanf(inbuf, "%s %s %s %s %s", keyname, timeStep[0],
					timeStep[1], timeStep[2], timeStep[3]);	// maximum number of possible timeStep is SW_OUTNPERIODS
			used_OUTNPERIODS--; // decrement the count to make sure to not count keyname in the number of periods

			useTimeStep = 1;

			// get timestep period
			char *dayCheck = strstr(inbuf, "dy");
			char *weekCheck = strstr(inbuf, "wk");
			char *monthCheck = strstr(inbuf, "mo");
			char *yearCheck = strstr(inbuf, "yr");

			// store time periods to use in array
			if(dayCheck != NULL && outfile_periods[0] == 0){
				outfile_periods[0] = 1;
			}
			if(weekCheck != NULL && outfile_periods[1] == 0){
				outfile_periods[1] = 1;
			}
			if(monthCheck != NULL && outfile_periods[2] == 0){
				outfile_periods[2] = 1;
			}
			if(yearCheck != NULL && outfile_periods[3] == 0){
				outfile_periods[3] = 1;
			}

			continue;
		}
		else
		{ // If the line TIMESTEP is NOT present, only need to read in five variables not six, so re read line.
			if (x < 6)
			{
				if (Str_CompareI(keyname, (char *)"OUTSEP") == 0)
				{
					switch ((int) *sumtype)
					{
					case 't':
						_Sep = '\t';
						break;
					case 's':
						_Sep = ' ';
						break;
					case 'c':
						_Sep = ',';
						break;
					default:
						_Sep = *sumtype;
					}
					continue;
				}
				else
				{

					CloseFile(&f);
					LogError(logfp, LOGFATAL,
							"%s : Insufficient key parameters for item %d.",
							MyFileName, itemno);
					continue;
				}
			}

			k = str2key(Str_ToUpper(keyname, upkey));

			if (useTimeStep) {
				for (i = 0; i < used_OUTNPERIODS; i++) {
					timeSteps[k][i] = str2period(Str_ToUpper(timeStep[i], ext));
				}

			} else {
				timeSteps[k][0] = str2period(Str_ToUpper(period, ext));
			}
		}
		if(useTimeStep == 0){
			if((Str_CompareI(period, "DY") == 0) && outfile_periods[0] == 0){
				outfile_periods[0] = 1;
			}
			if((Str_CompareI(period, "WK") == 0) && outfile_periods[1] == 0){
				outfile_periods[1] = 1;
			}
			if((Str_CompareI(period, "MO") == 0) && outfile_periods[2] == 0){
				outfile_periods[2] = 1;
			}
			if((Str_CompareI(period, "YR") == 0) && outfile_periods[3] == 0){
				outfile_periods[3] = 1;
			}
		}

			/* Check validity of output key */
			if (k == eSW_Estab)
			{
				strcpy(sumtype, "SUM");
				first = 1;
				strcpy(period, "YR");
				strcpy(last, "end");

			} else if ((k == eSW_AllVeg || k == eSW_ET || k == eSW_AllWthr
					|| k == eSW_AllH2O))
			{
				SW_Output[k].use = swFALSE;
				LogError(logfp, LOGNOTE, "%s : Output key %s is currently unimplemented.", MyFileName, key2str[k]);
				continue;
			}

			/* check validity of summary type */
			SW_Output[k].sumtype = str2stype(Str_ToUpper(sumtype, upsum));
			if (SW_Output[k].sumtype == eSW_Fnl
					&& !(k == eSW_VWCBulk || k == eSW_VWCMatric
							|| k == eSW_SWPMatric || k == eSW_SWCBulk
							|| k == eSW_SWABulk || k == eSW_SWA || k == eSW_SWAMatric
							|| k == eSW_DeepSWC))
			{
				LogError(logfp, LOGWARN, "%s : Summary Type FIN with key %s is meaningless.\n" "  Using type AVG instead.", MyFileName, key2str[k]);
				SW_Output[k].sumtype = eSW_Avg;
			}

			/* verify deep drainage parameters */
			if (k == eSW_DeepSWC && SW_Output[k].sumtype != eSW_Off
					&& !SW_Site.deepdrain)
			{
				LogError(logfp, LOGWARN, "%s : DEEPSWC cannot be output if flag not set in %s.", MyFileName, SW_F_name(eOutput));
				continue;
			}

			//Set the values
			SW_Output[k].use = (SW_Output[k].sumtype == eSW_Off) ? swFALSE : swTRUE;
			if (SW_Output[k].use)
			{
				SW_Output[k].mykey = k;
				SW_Output[k].myobj = key2obj[k];
				SW_Output[k].first_orig = first;
				SW_Output[k].last_orig =
						!Str_CompareI("END", (char *)last) ? 366 : atoi(last);
				if (SW_Output[k].last_orig == 0)
				{
					CloseFile(&f);
					LogError(logfp, LOGFATAL, "%s : Invalid ending day (%s), key=%s.", MyFileName, last, keyname);
				}
			}

			//Set the outputs for the Periods
#ifdef RSOILWAT
		SW_Output[k].outfile = (char *) Str_Dup(outfile); //not really applicable
#endif

	}


  #ifdef STEPWAT
  /* Check that STEPWAT2 receives monthly transpiration */
  Bool has_monT = swFALSE;

  for (i = 0; i < used_OUTNPERIODS; i++) {
    if (timeSteps[eSW_Transp][i] == eSW_Month) {
      has_monT = swTRUE;
      break;
    }
  }

  if (!has_monT) {
    CloseFile(&f);
    LogError(logfp, LOGFATAL, "STEPWAT2 requires monthly transpiration, " \
      "but this is currently turned off.");
  }
  #endif


	//printf("make soil: %d\n", make_soil);
	//printf("make regular: %d\n", make_regular);

	// creating files here instead of in loop so we can check periods
	// Also check if need to create both soil and regular or just one
	// TODO: make make_soil and make_regular global to check in col_header and populate_output_values
	// functions to make sure not trying to write to file not created
	if(useTimeStep == 0)
		used_OUTNPERIODS = 1;
	#if defined(SOILWAT)
		if(outfile_periods[0] == 1)
			stat_Output_Daily_CSV_Summary(-1);
		if(outfile_periods[1] == 1)
			stat_Output_Weekly_CSV_Summary(-1);
		if(outfile_periods[2] == 1)
			stat_Output_Monthly_CSV_Summary(-1);
		if(outfile_periods[3] == 1)
			stat_Output_Yearly_CSV_Summary(-1);

	#elif defined(STEPWAT)
		// create output files if flag turned on and only for last iteration
		if (isPartialSoilwatOutput == FALSE || storeAllIterations)
		{
			if(isPartialSoilwatOutput == FALSE && Globals.currIter == Globals.runModelIterations-1)
			{
				// create file for defined timesteps
				if(outfile_periods[0] == 1)
					stat_Output_Daily_CSV_Summary(-1);
				if(outfile_periods[1] == 1)
					stat_Output_Weekly_CSV_Summary(-1);
				if(outfile_periods[2] == 1)
					stat_Output_Monthly_CSV_Summary(-1);
				if(outfile_periods[3] == 1)
					stat_Output_Yearly_CSV_Summary(-1);
			}
			if(storeAllIterations){
				// create file for defined timesteps
				if(outfile_periods[0] == 1)
					stat_Output_Daily_CSV_Summary(Globals.currIter+1);
				if(outfile_periods[1] == 1)
					stat_Output_Weekly_CSV_Summary(Globals.currIter+1);
				if(outfile_periods[2] == 1)
					stat_Output_Monthly_CSV_Summary(Globals.currIter+1);
				if(outfile_periods[3] == 1)
					stat_Output_Yearly_CSV_Summary(Globals.currIter+1);
			}
		}
	#endif

	CloseFile(&f);

	if (EchoInits)
		_echo_outputs();
}


void SW_OUT_close_files(void)
{
	/* --------------------------------------------------- */
	/* close all of the user-specified output files.
	 * call this routine at the end of the program run.
	 */

// check all timeperiods and which files created. only close created files.
// no code for RSOILWAT (because no files)

#if defined(SOILWAT) || defined(STEPWAT)
  Bool close_regular, close_layers, close_AllIters;
#endif

#if defined(SOILWAT)
  close_regular = SW_File_Status.make_regular ? swTRUE : swFALSE;
  close_layers = SW_File_Status.make_soil ? swTRUE : swFALSE;
  close_AllIters = swFALSE;

#elif defined(STEPWAT)
  close_regular = (SW_File_Status.make_regular && !isPartialSoilwatOutput) ? swTRUE : swFALSE;
  close_layers = (SW_File_Status.make_soil && !isPartialSoilwatOutput) ? swTRUE : swFALSE;
  close_AllIters = (SW_File_Status.make_regular && storeAllIterations) ? swTRUE : swFALSE;
#endif

#if defined(SOILWAT) || defined(STEPWAT)
  if (SW_File_Status.use_Day) {
    if (close_regular) {
      CloseFile(&SW_File_Status.fp_dy_avg);
    }

    if (close_layers) {
      CloseFile(&SW_File_Status.fp_dy_soil_avg);
    }

    if (close_AllIters) {
      CloseFile(&SW_File_Status.fp_dy);
    }
  }

  if (SW_File_Status.use_Week){
    if (close_regular) {
      CloseFile(&SW_File_Status.fp_wk_avg);
    }

    if (close_layers) {
      CloseFile(&SW_File_Status.fp_wk_soil_avg);
    }

    if (close_AllIters) {
      CloseFile(&SW_File_Status.fp_wk);
    }
  }

  if (SW_File_Status.use_Month) {
    if (close_regular) {
      CloseFile(&SW_File_Status.fp_mo_avg);
    }

    if (close_layers) {
      CloseFile(&SW_File_Status.fp_mo_soil_avg);
    }

    if (close_AllIters) {
      CloseFile(&SW_File_Status.fp_mo);
    }
  }

  if(SW_File_Status.use_Year){
    if (close_regular) {
      CloseFile(&SW_File_Status.fp_yr_avg);
    }

    if (close_layers) {
      CloseFile(&SW_File_Status.fp_yr_soil_avg);
    }

    if (close_AllIters) {
      CloseFile(&SW_File_Status.fp_yr);
    }
  }
#endif
}


void _collect_values(void) {
	/*=======================================================*/

	SW_OUT_sum_today(eSWC);
	SW_OUT_sum_today(eWTH);
	SW_OUT_sum_today(eVES);
	SW_OUT_sum_today(eVPD);

	SW_OUT_write_today();
}


void SW_OUT_flush(void)
{
	/* --------------------------------------------------- */
	/* called at year end to process the remainder of the output
	 * period.  This sets two module-level flags: bFlush_output and
	 * tOffset to be used in the appropriate subs.
	 */
	bFlush_output = swTRUE;
	tOffset = 0;

	_collect_values();

	bFlush_output = swFALSE;
	tOffset = 1;
}

void SW_OUT_sum_today(ObjType otyp)
{
	/* =================================================== */
	/* adds today's output values to week, month and year
	 * accumulators and puts today's values in yesterday's
	 * registers. This is different from the Weather.c approach
	 * which updates Yesterday's registers during the _new_day()
	 * function. It's more logical to update yesterday just
	 * prior to today's calculations, but there's no logical
	 * need to perform _new_day() on the soilwater.
	 */
	SW_SOILWAT *s = &SW_Soilwat;
	SW_WEATHER *w = &SW_Weather;
	SW_VEGPROD *vp = &SW_VegProd;
	/*  SW_VEGESTAB *v = &SW_VegEstab;  -> we don't need to sum daily for this */

	OutPeriod pd;
	IntU size = 0;

	switch (otyp)
	{
	case eSWC:
		size = sizeof(SW_SOILWAT_OUTPUTS);
		break;
	case eWTH:
		size = sizeof(SW_WEATHER_OUTPUTS);
		break;
	case eVES:
		return; /* a stub; we don't do anything with ves until get_() */
	case eVPD:
		size = sizeof(SW_VEGPROD_OUTPUTS);
		break;
	default:
		LogError(logfp, LOGFATAL,
				"Invalid object type in SW_OUT_sum_today().");
	}

	/* do this every day (kinda expensive but more general than before)*/
	switch (otyp)
	{
	case eSWC:
		memset(&s->dysum, 0, size);
		break;
	case eWTH:
		memset(&w->dysum, 0, size);
		break;
	case eVPD:
		memset(&vp->dysum, 0, size);
		break;
	default:
		break;
	}

	/* the rest only get done if new period */
	if (SW_Model.newweek || bFlush_output)
	{
		average_for(otyp, eSW_Week);
		switch (otyp)
		{
		case eSWC:
			memset(&s->wksum, 0, size);
			break;
		case eWTH:
			memset(&w->wksum, 0, size);
			break;
		case eVPD:
			memset(&vp->wksum, 0, size);
			break;
		default:
			break;
		}
	}

	if (SW_Model.newmonth || bFlush_output)
	{
		average_for(otyp, eSW_Month);
		switch (otyp)
		{
		case eSWC:
			memset(&s->mosum, 0, size);
			break;
		case eWTH:
			memset(&w->mosum, 0, size);
			break;
		case eVPD:
			memset(&vp->mosum, 0, size);
			break;
		default:
			break;
		}
	}

	if (SW_Model.newyear || bFlush_output)
	{
		average_for(otyp, eSW_Year);
		switch (otyp)
		{
		case eSWC:
			memset(&s->yrsum, 0, size);
			break;
		case eWTH:
			memset(&w->yrsum, 0, size);
			break;
		case eVPD:
			memset(&vp->yrsum, 0, size);
			break;
		default:
			break;
		}
	}

	if (!bFlush_output)
	{
		ForEachOutPeriod(pd)
			collect_sums(otyp, pd);
	}
}

void SW_OUT_write_today(void)
{
	/* --------------------------------------------------- */
	/* all output values must have been summed, averaged or
	 * otherwise completed before this is called [now done
	 * by SW_*_sum_*<daily|yearly>()] prior.
	 * This subroutine organizes only the calling loop and
	 * sending the string to output.
	 * Each output quantity must have a print function
	 * defined and linked to SW_Output.pfunc (currently all
	 * starting with 'get_').  Those funcs return a properly
	 * formatted string to be output via the module variable
	 * 'sw_outstr'. Furthermore, those funcs must know their
	 * own time period.  This version of the program only
	 * prints one period for each quantity.
	 *
	 * The t value tests whether the current model time is
	 * outside the output time range requested by the user.
	 * Recall that times are based at 0 rather than 1 for
	 * array indexing purposes but the user request is in
	 * natural numbers, so we add one before testing.
	 */
	/* 10-May-02 (cwb) Added conditional to interface with STEPPE.
	 *           We want no output if running from STEPPE.
	 * July 12, 2017: Added functionality for writing outputs for STEPPE and SOILWAT since we now want output for STEPPE
	 */
	TimeInt t = 0xffff;
	OutKey k;
	Bool writeit;
	int i;
	#ifdef SWDEBUG
  int debug = 0;
  #endif

	#ifndef RSOILWAT
		// timestep output vars
		char *soil_file_vals_day[500]; // store
		char *reg_file_vals_day[500]; // store

		char *soil_file_vals_week[500]; // store
		char *reg_file_vals_week[500]; // store

		char *soil_file_vals_month[500]; // store
		char *reg_file_vals_month[500]; // store

		char *soil_file_vals_year[500]; // store
		char *reg_file_vals_year[500]; // store
	#endif

	#ifdef STEPWAT
		char *soil_file_vals_day_iters[500]; // store
		char *reg_file_vals_day_iters[500]; // store

		char *soil_file_vals_week_iters[500]; // store
		char *reg_file_vals_week_iters[500]; // store

		char *soil_file_vals_month_iters[500]; // store
		char *reg_file_vals_month_iters[500]; // store

		char *soil_file_vals_year_iters[500]; // store
		char *reg_file_vals_year_iters[500]; // store
	#endif


	// get final value to be used for each timeperiod
	if(SW_File_Status.finalValue_dy == -1){
		SW_File_Status.finalValue_dy = SW_File_Status.finalValue_wk = SW_File_Status.finalValue_mo = SW_File_Status.finalValue_yr = -2; // set to -2 so dont redo this check
		ForEachOutKey(k){
				if(SW_Output[k].use){
					if(useTimeStep == 0){
						if(timeSteps[k][0] == 0){ // change from period to timesteps
							if(k > SW_File_Status.finalValue_dy){
								SW_File_Status.finalValue_dy = k;
						}
					}
						if(timeSteps[k][0] == 1){
							if(k > SW_File_Status.finalValue_wk){
								SW_File_Status.finalValue_wk = k;
						}
					}
						if(timeSteps[k][0] == 2){
							if(k > SW_File_Status.finalValue_mo){
								SW_File_Status.finalValue_mo = k;
						}
					}
						if(timeSteps[k][0] == 3){
							if(k > SW_File_Status.finalValue_yr){
								SW_File_Status.finalValue_yr = k;
						}
					}
				}
				else{ // final value of all if using timestep
					if(k > SW_File_Status.finalValue_dy){
						SW_File_Status.finalValue_dy = k;
						SW_File_Status.finalValue_wk = k;
						SW_File_Status.finalValue_mo = k;
						SW_File_Status.finalValue_yr = k;
				}
			}
		}
	}
}

 #ifdef SWDEBUG
  if (debug) swprintf("'SW_OUT_write_today': %dyr-%dmon-%dwk-%ddoy: ",
    SW_Model.year, SW_Model.month, SW_Model.week, SW_Model.doy);
  #endif

	ForEachOutKey(k)
	{
		#ifdef SWDEBUG
		if (debug) swprintf("key=%d=%s: ", k, key2str[k]);
		#endif

		if (!SW_Output[k].use) {
			continue;
		}

		for (i = 0; i < used_OUTNPERIODS; i++)
		{
			#ifdef SWDEBUG
			if (debug) swprintf("/%d=%s", timeSteps[k][i], pd2str[timeSteps[k][i]]);
			#endif

			writeit = swTRUE;
			switch (timeSteps[k][i])
			{
			case eSW_Day:
				t = SW_Model.doy;
				break;
			case eSW_Week:
				writeit = (Bool) (SW_Model.newweek || bFlush_output);
				t = (SW_Model.week + 1) - tOffset;
				break;
			case eSW_Month:
				writeit = (Bool) (SW_Model.newmonth || bFlush_output);
				t = (SW_Model.month + 1) - tOffset;
				break;
			case eSW_Year:
				writeit = (Bool) (SW_Model.newyear || bFlush_output);
				t = SW_Output[k].first; /* always output this period */
				break;
			#ifndef RSOILWAT // RSOILWAT sets off variables to SW_missing so this is not invalid for rSOILWAT2
			default: // e.g., SW_MISSING
				LogError(logfp, LOGWARN,
					"'SW_OUT_write_today': Invalid period = %d for key = %s",
					timeSteps[k][i], key2str[k]);
				continue;
			#endif
			}
			#ifdef SWDEBUG
			if (debug) swprintf("-t=%d", t);
			#endif

			if (!writeit || t < SW_Output[k].first || t > SW_Output[k].last)
				continue;

			#ifdef SWDEBUG
			if (debug) swprintf(" call pfunc");
			#endif

			((void (*)(OutPeriod)) SW_Output[k].pfunc)(timeSteps[k][i]);
			#ifdef SWDEBUG
			if (debug) swprintf(" ... ok");
			#endif

#if defined(SOILWAT)
				/*-----------------------------------------------------------
				writing values to output files
				-----------------------------------------------------------*/
				switch (timeSteps[k][i])
				{ // based on iteration of for loop, determines which file to output to
				case eSW_Day:
					if(SW_File_Status.col_status_dy == 0)
					{
						memset(&reg_file_vals_day[0], 0, sizeof(reg_file_vals_day));
						memset(&soil_file_vals_day[0], 0, sizeof(soil_file_vals_day));
						create_col_headers(1, SW_File_Status.fp_dy_avg, SW_File_Status.fp_dy_soil_avg, 0);
						SW_File_Status.col_status_dy++;
					}

					populate_output_values((char*)reg_file_vals_day, (char*)soil_file_vals_day, k, 1, 0);

					if(k == SW_File_Status.finalValue_dy){
						if(reg_file_vals_day[0] != 0 && SW_File_Status.make_regular){
							fprintf(SW_File_Status.fp_dy_avg, "%d%c%d%c%s\n", SW_Model.simyear, _Sep, SW_Model.doy, _Sep, (char*)reg_file_vals_day);
							memset(&reg_file_vals_day[0], 0, sizeof(reg_file_vals_day));
						}
						if(soil_file_vals_day[0] != 0 && SW_File_Status.make_soil){
							fprintf(SW_File_Status.fp_dy_soil_avg, "%d%c%d%c%s\n", SW_Model.simyear, _Sep, SW_Model.doy, _Sep, (char*)soil_file_vals_day);
							memset(&soil_file_vals_day[0], 0, sizeof(soil_file_vals_day));
						}
					}
					break;

				case eSW_Week:
					if(SW_File_Status.col_status_wk == 0)
					{
						memset(&reg_file_vals_week[0], 0, sizeof(reg_file_vals_week));
						memset(&soil_file_vals_week[0], 0, sizeof(soil_file_vals_week));
						create_col_headers(2, SW_File_Status.fp_wk_avg, SW_File_Status.fp_wk_soil_avg, 0);
						SW_File_Status.col_status_wk++;
					}

					populate_output_values((char*)reg_file_vals_week, (char*)soil_file_vals_week, k, 2, 0);

					if(k == SW_File_Status.finalValue_wk){
						// need to check if repeat 52 since repeats 52 in output file.
						if(SW_Model.week == 52 && SW_File_Status.lastWeek == 1){
							SW_Model.week = 53;
							SW_File_Status.lastWeek = 0;
						}
						else if(SW_Model.week == 52 && SW_File_Status.lastWeek == 0) SW_File_Status.lastWeek = 1;
						if(soil_file_vals_week[0] != 0 && SW_File_Status.make_soil){
							fprintf(SW_File_Status.fp_wk_soil_avg, "%d%c%d%c%s\n", SW_Model.simyear, _Sep, SW_Model.week, _Sep, (char*)soil_file_vals_week);
							memset(&soil_file_vals_week[0], 0, sizeof(soil_file_vals_week));
						}
						if(reg_file_vals_week[0] != 0 && SW_File_Status.make_regular){
							fprintf(SW_File_Status.fp_wk_avg, "%d%c%d%c%s\n", SW_Model.simyear, _Sep, SW_Model.week, _Sep, (char*)reg_file_vals_week);
							memset(&reg_file_vals_week[0], 0, sizeof(reg_file_vals_week));
						}
					}
					break;

				case eSW_Month:
					if(SW_File_Status.col_status_mo == 0)
					{
						memset(&reg_file_vals_month[0], 0, sizeof(reg_file_vals_month));
						memset(&soil_file_vals_month[0], 0, sizeof(soil_file_vals_month));
						create_col_headers(3, SW_File_Status.fp_mo_avg, SW_File_Status.fp_mo_soil_avg, 0);
						SW_File_Status.col_status_mo++;
					}

					populate_output_values((char*)reg_file_vals_month, (char*)soil_file_vals_month, k, 3, 0);

					if(k == SW_File_Status.finalValue_mo){
						// need to check if repeat 11 since repeats 11 in output file.
						if(SW_Model.month == 11 && SW_File_Status.lastMonth == 1){
							SW_Model.month = 12;
							SW_File_Status.lastMonth = 0;
						}
						else if(SW_Model.month == 11 && SW_File_Status.lastMonth == 0) SW_File_Status.lastMonth = 1;

						if(soil_file_vals_month[0] != 0 && SW_File_Status.make_soil){
							fprintf(SW_File_Status.fp_mo_soil_avg, "%d%c%d%c%s\n", SW_Model.simyear, _Sep, SW_Model.month, _Sep, (char*)soil_file_vals_month);
							memset(&soil_file_vals_month[0], 0, sizeof(soil_file_vals_month));
						}
						if(reg_file_vals_month[0] != 0 && SW_File_Status.make_regular){
							fprintf(SW_File_Status.fp_mo_avg, "%d%c%d%c%s\n", SW_Model.simyear, _Sep, SW_Model.month, _Sep, (char*)reg_file_vals_month);
							memset(&reg_file_vals_month[0], 0, sizeof(reg_file_vals_month));
						}
					}
					break;

				case eSW_Year:
					if(SW_File_Status.col_status_yr == 0)
					{
						memset(&reg_file_vals_year[0], 0, sizeof(reg_file_vals_year));
						memset(&soil_file_vals_year[0], 0, sizeof(soil_file_vals_year));
						create_col_headers(4, SW_File_Status.fp_yr_avg, SW_File_Status.fp_yr_soil_avg, 0);
						SW_File_Status.col_status_yr++;
					}

					populate_output_values((char*)reg_file_vals_year, (char*)soil_file_vals_year, k, 4, 0);

					if(k == SW_File_Status.finalValue_yr){
						if(soil_file_vals_year[0] != 0 && SW_File_Status.make_soil){
							fprintf(SW_File_Status.fp_yr_soil_avg, "%d%c%s\n", SW_Model.simyear, _Sep, (char*)soil_file_vals_year);
							memset(&soil_file_vals_year[0], 0, sizeof(soil_file_vals_year));
						}
						if(reg_file_vals_year[0] != 0 && SW_File_Status.make_regular){
							fprintf(SW_File_Status.fp_yr_avg, "%d%c%s\n", SW_Model.simyear, _Sep, (char*)reg_file_vals_year);
							memset(&reg_file_vals_year[0], 0, sizeof(reg_file_vals_year));
						}
					}
					break;
				}

#elif defined(STEPWAT)
	if ((isPartialSoilwatOutput == FALSE && Globals.currIter == Globals.runModelIterations) || storeAllIterations)
	{
		switch (timeSteps[k][i])
		{ // based on iteration of for loop, determines which file to output to
		case eSW_Day:
			if(SW_File_Status.col_status_dy == 0) // if first run through need to make column headers
			{
				memset(&reg_file_vals_day[0], 0, sizeof(reg_file_vals_day));
				memset(&soil_file_vals_day[0], 0, sizeof(soil_file_vals_day));
				if(storeAllIterations){
					memset(&reg_file_vals_day_iters[0], 0, sizeof(reg_file_vals_day_iters));
					memset(&soil_file_vals_day_iters[0], 0, sizeof(soil_file_vals_day_iters));
				}
				if(isPartialSoilwatOutput == FALSE && Globals.currIter == Globals.runModelIterations)
					create_col_headers(1, SW_File_Status.fp_dy_avg, SW_File_Status.fp_dy_soil_avg, 1);
				if(storeAllIterations)
					create_col_headers(1, SW_File_Status.fp_dy, SW_File_Status.fp_dy_soil, 0);
				SW_File_Status.col_status_dy++;
			}

			if(isPartialSoilwatOutput == FALSE && Globals.currIter == Globals.runModelIterations){
				populate_output_values((char*)reg_file_vals_day, (char*)soil_file_vals_day, k, 1, 0); // function to put all the values together for output
			}
			if(storeAllIterations)
				populate_output_values((char*)reg_file_vals_day_iters, (char*)soil_file_vals_day_iters, k, 1, 1); // function to put all the values together for output


			if(k == SW_File_Status.finalValue_dy){ // if last value to be used then write to files
				if(SW_File_Status.make_regular){
					if(isPartialSoilwatOutput == FALSE && Globals.currIter == Globals.runModelIterations && reg_file_vals_day[0] != 0){
						fprintf(SW_File_Status.fp_dy_avg, "%d%c%d%c%s\n", SW_Model.simyear, _Sep, SW_Model.doy, _Sep, (char*)reg_file_vals_day);
						memset(&reg_file_vals_day[0], 0, sizeof(reg_file_vals_day)); // set arrays to empty
					}
					if(storeAllIterations && reg_file_vals_day_iters[0] != 0){
						fprintf(SW_File_Status.fp_dy, "%d%c%d%c%s\n", SW_Model.simyear, _Sep, SW_Model.doy, _Sep, (char*)reg_file_vals_day_iters);
						memset(&reg_file_vals_day_iters[0], 0, sizeof(reg_file_vals_day_iters)); // set arrays to empty
					}
				}
				if(SW_File_Status.make_soil){
					if(isPartialSoilwatOutput == FALSE && Globals.currIter == Globals.runModelIterations && soil_file_vals_day[0] != 0){
						fprintf(SW_File_Status.fp_dy_soil_avg, "%d%c%d%c%s\n", SW_Model.simyear, _Sep, SW_Model.doy, _Sep, (char*)soil_file_vals_day);
						memset(&soil_file_vals_day[0], 0, sizeof(soil_file_vals_day));
					}
					if(storeAllIterations && soil_file_vals_day_iters[0] != 0){
						fprintf(SW_File_Status.fp_dy_soil, "%d%c%d%c%s\n", SW_Model.simyear, _Sep, SW_Model.doy, _Sep, (char*)soil_file_vals_day_iters);
						memset(&soil_file_vals_day_iters[0], 0, sizeof(soil_file_vals_day_iters));
					}
				}
			}
			break;

		case eSW_Week:
			if(SW_File_Status.col_status_wk == 0)
			{
				memset(&reg_file_vals_week[0], 0, sizeof(reg_file_vals_week));
				memset(&soil_file_vals_week[0], 0, sizeof(soil_file_vals_week));
				if(storeAllIterations){
					memset(&reg_file_vals_week_iters[0], 0, sizeof(reg_file_vals_week_iters));
					memset(&soil_file_vals_week_iters[0], 0, sizeof(soil_file_vals_week_iters));
				}
				if(isPartialSoilwatOutput == FALSE && Globals.currIter == Globals.runModelIterations)
					create_col_headers(2, SW_File_Status.fp_wk_avg, SW_File_Status.fp_wk_soil_avg, 1);
				if(storeAllIterations)
					create_col_headers(2, SW_File_Status.fp_wk, SW_File_Status.fp_wk_soil, 0);
				SW_File_Status.col_status_wk++;
			}

			if(isPartialSoilwatOutput == FALSE && Globals.currIter == Globals.runModelIterations)
				populate_output_values((char*)reg_file_vals_week, (char*)soil_file_vals_week, k, 2, 0);
			if(storeAllIterations)
				populate_output_values((char*)reg_file_vals_week_iters, (char*)soil_file_vals_week_iters, k, 2, 1);

			if(k == SW_File_Status.finalValue_wk){
				if(SW_File_Status.make_soil){
					if(isPartialSoilwatOutput == FALSE && Globals.currIter == Globals.runModelIterations && soil_file_vals_week[0] != 0){
						fprintf(SW_File_Status.fp_wk_soil_avg, "%d%c%d%c%s\n", SW_Model.simyear, _Sep, SW_Model.week, _Sep, (char*)soil_file_vals_week);
						memset(&soil_file_vals_week[0], 0, sizeof(soil_file_vals_week));
					}
					if(storeAllIterations && soil_file_vals_week_iters[0] != 0){
						fprintf(SW_File_Status.fp_wk_soil, "%d%c%d%c%s\n", SW_Model.simyear, _Sep, SW_Model.week, _Sep, (char*)soil_file_vals_week_iters);
						memset(&soil_file_vals_week_iters[0], 0, sizeof(soil_file_vals_week_iters));
					}
				}
				if(SW_File_Status.make_regular){
					if(isPartialSoilwatOutput == FALSE && Globals.currIter == Globals.runModelIterations && reg_file_vals_week[0] != 0){
						fprintf(SW_File_Status.fp_wk_avg, "%d%c%d%c%s\n", SW_Model.simyear, _Sep, SW_Model.week, _Sep, (char*)reg_file_vals_week);
						memset(&reg_file_vals_week[0], 0, sizeof(reg_file_vals_week));
					}
					if(storeAllIterations && reg_file_vals_week_iters[0] != 0){
						fprintf(SW_File_Status.fp_wk, "%d%c%d%c%s\n", SW_Model.simyear, _Sep, SW_Model.week, _Sep, (char*)reg_file_vals_week_iters);
						memset(&reg_file_vals_week_iters[0], 0, sizeof(reg_file_vals_week_iters));
					}
				}
			}
			break;

		case eSW_Month:
			if(SW_File_Status.col_status_mo == 0)
			{
				memset(&reg_file_vals_month, 0, sizeof(reg_file_vals_month));
				memset(&soil_file_vals_month, 0, sizeof(soil_file_vals_month));
				if(storeAllIterations){
					memset(&reg_file_vals_month_iters[0], 0, sizeof(reg_file_vals_month_iters));
					memset(&soil_file_vals_month_iters[0], 0, sizeof(soil_file_vals_month_iters));
				}
				if(isPartialSoilwatOutput == FALSE && Globals.currIter == Globals.runModelIterations)
					create_col_headers(3, SW_File_Status.fp_mo_avg, SW_File_Status.fp_mo_soil_avg, 1);
				if(storeAllIterations)
					create_col_headers(3, SW_File_Status.fp_mo, SW_File_Status.fp_mo_soil, 0);
				SW_File_Status.col_status_mo++;
			}

			if(isPartialSoilwatOutput == FALSE && Globals.currIter == Globals.runModelIterations)
				populate_output_values((char*)reg_file_vals_month, (char*)soil_file_vals_month, k, 3, 0);
			if(storeAllIterations)
				populate_output_values((char*)reg_file_vals_month_iters, (char*)soil_file_vals_month_iters, k, 3, 1);

			if(k == SW_File_Status.finalValue_mo){
				if(SW_File_Status.make_soil){
					if(isPartialSoilwatOutput == FALSE && Globals.currIter == Globals.runModelIterations && soil_file_vals_month[0] != 0){
						fprintf(SW_File_Status.fp_mo_soil_avg, "%d%c%d%c%s\n", SW_Model.simyear, _Sep, SW_Model.month, _Sep, (char*)soil_file_vals_month);
						memset(&soil_file_vals_month, 0, sizeof(soil_file_vals_month));
					}
					if(storeAllIterations && soil_file_vals_month_iters[0] != 0){
						fprintf(SW_File_Status.fp_mo_soil, "%d%c%d%c%s\n", SW_Model.simyear, _Sep, SW_Model.month, _Sep, (char*)soil_file_vals_month_iters);
						memset(&soil_file_vals_month_iters, 0, sizeof(soil_file_vals_month_iters));
					}
				}
				if(SW_File_Status.make_regular){
					if(isPartialSoilwatOutput == FALSE && Globals.currIter == Globals.runModelIterations && reg_file_vals_month[0] != 0){
						fprintf(SW_File_Status.fp_mo_avg, "%d%c%d%c%s\n", SW_Model.simyear, _Sep, SW_Model.month, _Sep, (char*)reg_file_vals_month);
						memset(&reg_file_vals_month, 0, sizeof(reg_file_vals_month));
					}
					if(storeAllIterations && reg_file_vals_month_iters[0] != 0){
						fprintf(SW_File_Status.fp_mo, "%d%c%d%c%s\n", SW_Model.simyear, _Sep, SW_Model.month, _Sep, (char*)reg_file_vals_month_iters);
						memset(&reg_file_vals_month_iters, 0, sizeof(reg_file_vals_month_iters));
					}
				}
			}
			break;

		case eSW_Year:
			if(SW_File_Status.col_status_yr == 0)
			{
				memset(&reg_file_vals_year[0], 0, sizeof(reg_file_vals_year));
				memset(&soil_file_vals_year[0], 0, sizeof(soil_file_vals_year));
				if(storeAllIterations){
					memset(&reg_file_vals_year_iters[0], 0, sizeof(reg_file_vals_year_iters));
					memset(&soil_file_vals_year_iters[0], 0, sizeof(soil_file_vals_year_iters));
				}
				if(isPartialSoilwatOutput == FALSE && Globals.currIter == Globals.runModelIterations)
					create_col_headers(4, SW_File_Status.fp_yr_avg, SW_File_Status.fp_yr_soil_avg, 1);
				if(storeAllIterations)
					create_col_headers(4, SW_File_Status.fp_yr, SW_File_Status.fp_yr_soil, 0);
				SW_File_Status.col_status_yr++;
			}

			if(isPartialSoilwatOutput == FALSE && Globals.currIter == Globals.runModelIterations)
				populate_output_values((char*)reg_file_vals_year, (char*)soil_file_vals_year, k, 4, 0);
			if(storeAllIterations){
				populate_output_values((char*)reg_file_vals_year_iters, (char*)soil_file_vals_year_iters, k, 4, 1);
			}

			if(k == SW_File_Status.finalValue_yr){
				if(SW_File_Status.make_soil){
					if(isPartialSoilwatOutput == FALSE && Globals.currIter == Globals.runModelIterations && soil_file_vals_year[0] != 0){
						fprintf(SW_File_Status.fp_yr_soil_avg, "%d%c%s\n", SW_Model.simyear, _Sep, (char*)soil_file_vals_year);
						memset(&soil_file_vals_year[0], 0, sizeof(soil_file_vals_year));
					}
					if(storeAllIterations && soil_file_vals_year_iters[0] != 0){
						fprintf(SW_File_Status.fp_yr_soil, "%d%c%s\n", SW_Model.simyear, _Sep, (char*)soil_file_vals_year_iters);
						memset(&soil_file_vals_year_iters[0], 0, sizeof(soil_file_vals_year_iters));
					}
				}
				if(SW_File_Status.make_regular){
					if(isPartialSoilwatOutput == FALSE && Globals.currIter == Globals.runModelIterations && reg_file_vals_year[0] != 0){
						fprintf(SW_File_Status.fp_yr_avg, "%d%c%s\n", SW_Model.simyear, _Sep, (char*)reg_file_vals_year);
						memset(&reg_file_vals_year[0], 0, sizeof(reg_file_vals_year));
					}
					if(storeAllIterations && reg_file_vals_year_iters[0] != 0){
						fprintf(SW_File_Status.fp_yr, "%d%c%s\n", SW_Model.simyear, _Sep, (char*)reg_file_vals_year_iters);
						memset(&reg_file_vals_year_iters[0], 0, sizeof(reg_file_vals_year_iters));
					}
				}
			}
			break;
		}

	}
#endif
		}
	}

  #ifdef SWDEBUG
  if (debug) swprintf("'SW_OUT_write_today': completed\n");
  #endif
}

void _echo_outputs(void)
{
	/* --------------------------------------------------- */

	OutKey k;

	strcpy(errstr, "\n===============================================\n"
			"  Output Configuration:\n");
	ForEachOutKey(k)
	{
		if (!SW_Output[k].use)
			continue;
		strcat(errstr, "---------------------------\nKey ");
		strcat(errstr, key2str[k]);
		strcat(errstr, "\n\tSummary Type: ");
		strcat(errstr, styp2str[SW_Output[k].sumtype]);
		sprintf(sw_outstr, "\n\tStart period: %d", SW_Output[k].first_orig);
		strcat(errstr, sw_outstr);
		sprintf(sw_outstr, "\n\tEnd period  : %d", SW_Output[k].last_orig);
		strcat(errstr, sw_outstr);
		strcat(errstr, "\n\tOutput File: ");
		strcat(errstr, SW_Output[k].outfile);
		strcat(errstr, "\n");
	}

	strcat(errstr, "\n----------  End of Output Configuration ---------- \n");
	LogError(logfp, LOGNOTE, errstr);

}

/**
  \fn void populate_output_values(char *reg_file_array, char *soil_file_array, int output_var, int year_out)
  \brief Populates arrays with output in correct format.

  populate_output_values is called for all of the variables for each timeperiod and these values are parsed
	to the proper format defined in the .in file for output file type.

  \param reg_file_array. stores output for non-soil variables.
  \param soil_file_array. stores output for variables with layers.
	\param output_var. Tells function which value its using.
	\param year_out. Tells us which timeperiod we are using for display purposes.

  \return void.
*/
void populate_output_values(char *reg_file_array, char *soil_file_array, int output_var, IntU year_out, int outstr_file){
	char  _SepSplit[5]; // store seperator to parse data correctly
	char read_data[OUTSTRLEN];
	size_t destination_size = sizeof(read_data);
	if(_Sep == ' ') strcpy(_SepSplit, " ");
	else if(_Sep == ',') strcpy(_SepSplit, ",");
	else strcpy(_SepSplit, "\t");

	if(outstr_file == 0){
		strncpy(read_data, sw_outstr, destination_size);
		read_data[destination_size-1] = '\0'; // null terminate so no memory problem
	}
	#ifdef STEPWAT
		if(outstr_file == 1){
			strncpy(read_data, outstr_all_iters, destination_size);
			read_data[destination_size-1] = '\0'; // null terminate so no memory problem
		}
	#endif

	// check if a soil variable (has layers)
	if((strcmp(key2str[output_var], "VWCBULK")==0 || strcmp(key2str[output_var], "VWCMATRIC")==0 || strcmp(key2str[output_var], "SWCBULK")==0
		|| strcmp(key2str[output_var], "SWABULK")==0
		|| strcmp(key2str[output_var], "EVAPSOIL")==0 || strcmp(key2str[output_var], "TRANSP")==0 || strcmp(key2str[output_var], "WETDAY")==0
		|| strcmp(key2str[output_var], "LYRDRAIN")==0 || strcmp(key2str[output_var], "SOILTEMP")==0 || strcmp(key2str[output_var], "HYDRED")==0
		|| strcmp(key2str[output_var], "SWAMATRIC")==0 || strcmp(key2str[output_var], "SWPMATRIC")==0 || strcmp(key2str[output_var], "SWA")==0))
	{
		// if usetimestep == 0 then need to check period for output files
		if((useTimeStep == 0 && timeSteps[output_var][0] == year_out-1) || useTimeStep == 1){
			char *pt;
			int counter = 0;
			pt = strtok (read_data, _SepSplit);

			while (pt != NULL) {
				if(year_out == 4){
					if(counter >= 1){ // want to skip year string
						strcat(soil_file_array, pt);
						strcat(soil_file_array, _SepSplit);
					}
				}
				else{
						if(counter >= 2){ // dont want to parse year and timeperiod
							strcat(soil_file_array, pt);
							strcat(soil_file_array, _SepSplit);
						}
				}
					pt = strtok (NULL, _SepSplit);
					counter++;
			}
		}
	}
	else
	{
		if((useTimeStep == 0 && timeSteps[output_var][0] == year_out-1) || useTimeStep == 1){
			char *reg_pt;
			int reg_counter = 0;
			reg_pt = strtok (read_data,_SepSplit);
			while (reg_pt != NULL) {
				if(year_out == 4){
					if(reg_counter >= 1){
						strcat(reg_file_array, reg_pt);
						strcat(reg_file_array, _SepSplit);
					}
				}
				else{
						if(reg_counter >= 2 ){
							strcat(reg_file_array, reg_pt);
							strcat(reg_file_array, _SepSplit);
						}
				}
					reg_pt = strtok (NULL, _SepSplit);
					reg_counter++;
			}
		}
	}
	return;
}

/**
  \fn void create_col_headers(int outFileTimestep, FILE *regular_file, FILE *soil_file)
  \brief Creates column headers for output files

  create_col_headers is called only once for each set of output files and it goes through
	all values and if the value is defined to be used it creates the header in the output file

  \param outFileTimestep. timeperiod so it can write headers to correct output file.
  \param regular_file. name of file.
	\param soil_file. name of file.

  \return void.
*/
#ifndef RSOILWAT // function not for use with RSOILWAT since RSOILWAT has its own column header function. Planning on combining the two functions at a later date.
void create_col_headers(IntU outFileTimestep, FILE *regular_file, FILE *soil_file, int std_headers){
	int i, j, tLayers = SW_Site.n_layers;
	SW_VEGESTAB *v = &SW_VegEstab; // for use to check estab

	OutKey colHeadersLoop;
	char *colHeaders[5000]; // generous estimation of max size
	char *colHeadersSoil[5000];
	memset(&colHeaders, 0, sizeof(colHeaders));
	memset(&colHeadersSoil, 0, sizeof(colHeadersSoil));

	char  _SepSplit[5];
	if(_Sep == ' ') strcpy(_SepSplit, " ");
	if(_Sep == ',') strcpy(_SepSplit, ",");
	else strcpy(_SepSplit, "\t");

	char *col1Head;
	char *col2Head;

	#ifndef STEPWAT
		if(std_headers != 0)
			std_headers = 0; // make sure set to 0 if not STEPWAT
	#endif

	const char *Layers_names[MAX_LAYERS] = { "Lyr_1", "Lyr_2", "Lyr_3", "Lyr_4", "Lyr_5",
		"Lyr_6", "Lyr_7", "Lyr_8", "Lyr_9", "Lyr_10", "Lyr_11", "Lyr_12", "Lyr_13", "Lyr_14",
		"Lyr_15", "Lyr_16", "Lyr_17", "Lyr_18", "Lyr_19", "Lyr_20", "Lyr_21", "Lyr_22",
		"Lyr_23", "Lyr_24", "Lyr_25"};

	const char *cnames_VegTypes[6] = { "Total", "Tree", "Shrub", "Forbs",
		"Grass", "Litter" };

	// commented out column names are not used, uncomment when merge the column header function with that for rSOILWAT2
	const char *cnames_eSW_Temp[] = { "Temp_max", "Temp_min", "Temp_avg", "SurfaceTemp" };
	const char *cnames_eSW_Precip[] = { "ppt", "rain", "snow_fall", "snowmelt", "snowloss" };
	//const char *cnames_eSW_SoilInf[] = { "soil_inf" };
	const char *cnames_eSW_Runoff[] = { "net", "ponded_runoff", "snowmelt_runoff", "ponded_runon" };
	//const char *cnames_eSW_SurfaceWater[] = { "surfaceWater_cm" };
	//const char *cnames_add_eSW_EvapSurface[] = { "evap_surfaceWater" };
	const char *cnames_eSW_AET[] = { "evapotr_cm" };
	//const char *cnames_eSW_PET[] = { "pet_cm" };
	const char *cnames_eSW_SnowPack[] = { "snowpackWaterEquivalent_cm", "snowdepth_cm" };
	//const char *cnames_eSW_DeepSWC[] = { "lowLayerDrain_cm" };
	const char *cnames_eSW_CO2Effects[] = { // uses different order of vegtypes than others ...
		"GrassBiomass", "ShrubBiomass", "TreeBiomass", "ForbBiomass", "TotalBiomass",
		"GrassBiolive", "ShrubBiolive", "TreeBiolive", "ForbBiolive", "TotalBiolive",
		"GrassBioMult", "ShrubBioMult", "TreeBioMult", "ForbBioMult",
		"GrassWUEMult", "ShrubWUEMult", "TreeWUEMult", "ForbWUEMult"};


	ForEachOutKey(colHeadersLoop)
	{
			if((SW_Output[colHeadersLoop].use && useTimeStep == 0 && timeSteps[colHeadersLoop][0] == outFileTimestep-1) || (SW_Output[colHeadersLoop].use && useTimeStep == 1))
		{
			if(strcmp(key2str[colHeadersLoop], "VWCBULK")==0 || strcmp(key2str[colHeadersLoop], "VWCMATRIC")==0 || strcmp(key2str[colHeadersLoop], "SWCBULK")==0
				|| strcmp(key2str[colHeadersLoop], "EVAPSOIL")==0 || strcmp(key2str[colHeadersLoop], "TRANSP")==0 || strcmp(key2str[colHeadersLoop], "SWABULK")==0
				|| strcmp(key2str[colHeadersLoop], "LYRDRAIN")==0 || strcmp(key2str[colHeadersLoop], "SOILTEMP")==0 || strcmp(key2str[colHeadersLoop], "HYDRED")==0
				|| strcmp(key2str[colHeadersLoop], "SWAMATRIC")==0 || strcmp(key2str[colHeadersLoop], "SWA")==0 || strcmp(key2str[colHeadersLoop], "SWPMATRIC")==0
				|| strcmp(key2str[colHeadersLoop], "WETDAY")==0)
			{
				int q;
				char convertq[10] = {0};
				char storeCol[5000] = {0}; // need to make this a pretty large file to accommodate all the header names up to max layer size

				// swa, trasp, and hydred are all the same col headers algorithm just need different index for swa
				if(strcmp(key2str[colHeadersLoop], "SWA")==0 || strcmp(key2str[colHeadersLoop], "HYDRED")==0 || strcmp(key2str[colHeadersLoop], "TRANSP")==0)
				{
					int start_index;
					if(strcmp(key2str[colHeadersLoop], "SWA")==0)
						start_index = 1;
					else
						start_index = 0;
					for (i = 0; i < tLayers; i++) {
						for (j = start_index; j < NVEGTYPES + 1; j++) { // only want the veg types, dont need 'total' or 'litter'
							strcat(storeCol, key2str[colHeadersLoop]); // store value name in new string
							strcat(storeCol, cnames_VegTypes[j]);
							strcat(storeCol, "_");
							strcat(storeCol, Layers_names[i]);
							strcat(storeCol, _SepSplit);
							#ifdef STEPWAT
								if(std_headers){
									strcat(storeCol, key2str[colHeadersLoop]); // store value name in new string
									strcat(storeCol, cnames_VegTypes[j]);
									strcat(storeCol, "_STD_");
									strcat(storeCol, Layers_names[i]);
									strcat(storeCol, _SepSplit);
								}
							#endif
						}
					}
				}
				else if(strcmp(key2str[colHeadersLoop], "EVAPSOIL")==0){
					for (i = 0; i < ncol_OUT[eSW_EvapSoil]; i++) {
						strcat(storeCol, key2str[colHeadersLoop]); // store value name in new string
						// concatenate layer number
						strcat(storeCol, "_");
						strcat(storeCol, Layers_names[i]);
						strcat(storeCol, _SepSplit);
						#ifdef STEPWAT
							if(std_headers){
								strcat(storeCol, key2str[colHeadersLoop]); // store value name in new string
								strcat(storeCol, "_STD_");
								strcat(storeCol, Layers_names[i]);
								strcat(storeCol, _SepSplit);
							}
						#endif
					}
				}
				else if(strcmp(key2str[colHeadersLoop], "LYRDRAIN")==0){
					for (i = 0; i < ncol_OUT[eSW_LyrDrain]; i++) {
						strcat(storeCol, key2str[colHeadersLoop]); // store value name in new string
						// concatenate layer number
						strcat(storeCol, "_");
						strcat(storeCol, Layers_names[i]);
						strcat(storeCol, _SepSplit);
						#ifdef STEPWAT
							if(std_headers){
								strcat(storeCol, key2str[colHeadersLoop]); // store value name in new string
								strcat(storeCol, "_STD_");
								strcat(storeCol, Layers_names[i]);
								strcat(storeCol, _SepSplit);
							}
						#endif
					}
				}
				else{
					// make variable header for each layer possible
					for(q = 1; q <= (int)SW_Site.n_layers; q++){
						sprintf(convertq, "%d", q); // cast q to string
						strcat(storeCol, key2str[colHeadersLoop]); // store value name in new string
						// concatenate layer number
						strcat(storeCol, "_");
						strcat(storeCol, convertq);
						strcat(storeCol, _SepSplit);
						#ifdef STEPWAT
							if(std_headers){
								strcat(storeCol, key2str[colHeadersLoop]); // store value name in new string
								strcat(storeCol, "_STD_");
								strcat(storeCol, convertq);
								strcat(storeCol, _SepSplit);
							}
						#endif
					}
				}
				strcat((char*)colHeadersSoil, storeCol); // concatenate variable to string
			}

			else
			{
				char storeRegCol[2000] = {0};
				if(strcmp(key2str[colHeadersLoop], "TEMP")==0){
					for (i = 0; i < ncol_OUT[eSW_Temp]; i++) {
						strcat(storeRegCol, cnames_eSW_Temp[i]);
						strcat(storeRegCol, _SepSplit);
						#ifdef STEPWAT
							if(std_headers){
								strcat(storeRegCol, cnames_eSW_Temp[i]);
								strcat(storeRegCol, "_STD");
								strcat(storeRegCol, _SepSplit);
							}
						#endif
					}
				}
				else if(strcmp(key2str[colHeadersLoop], "PRECIP")==0){
					for (i = 0; i < ncol_OUT[eSW_Precip]; i++) {
						strcat(storeRegCol, cnames_eSW_Precip[i]);
						strcat(storeRegCol, _SepSplit);
						#ifdef STEPWAT
							if(std_headers){
								strcat(storeRegCol, cnames_eSW_Precip[i]);
								strcat(storeRegCol, "_STD");
								strcat(storeRegCol, _SepSplit);
							}
						#endif
					}
				}
				else if(strcmp(key2str[colHeadersLoop], "ESTABL")==0){
					if(v->count > 0){ // only create col if estab has values
						strcat(storeRegCol, key2str[colHeadersLoop]); // concatenate variable to string
						strcat(storeRegCol, _SepSplit);
						#ifdef STEPWAT
							if(std_headers){
								strcat(storeRegCol, key2str[colHeadersLoop]); // store value name in new string
								strcat(storeRegCol, "_STD");
								strcat(storeRegCol, _SepSplit);
							}
						#endif
					}
				}
				else if(strcmp(key2str[colHeadersLoop], "RUNOFF")==0){
					for (i = 0; i < ncol_OUT[eSW_Runoff]; i++) {
						strcat(storeRegCol, cnames_eSW_Runoff[i]);
						strcat(storeRegCol, _SepSplit);
						#ifdef STEPWAT
							if(std_headers){
								strcat(storeRegCol, cnames_eSW_Runoff[i]);
								strcat(storeRegCol, "_STD");
								strcat(storeRegCol, _SepSplit);
							}
						#endif
					}
				}
				else if(strcmp(key2str[colHeadersLoop], "AET")==0){
					for (i = 0; i < ncol_OUT[eSW_AET]; i++) {
						strcat(storeRegCol, cnames_eSW_AET[i]);
						strcat(storeRegCol, _SepSplit);
						#ifdef STEPWAT
							if(std_headers){
								strcat(storeRegCol, cnames_eSW_AET[i]); // store value name in new string
								strcat(storeRegCol, "_STD");
								strcat(storeRegCol, _SepSplit);
							}
						#endif
					}
				}
				else if(strcmp(key2str[colHeadersLoop], "EVAPSURFACE")==0){
					for (i = 0; i < NVEGTYPES + 2; i++){
						strcat(storeRegCol, "EvapSurface_");
						strcat(storeRegCol, cnames_VegTypes[i]);
						strcat(storeRegCol, _SepSplit);
						#ifdef STEPWAT
							if(std_headers){
								strcat(storeRegCol, "EvapSurface_"); // store value name in new string
								strcat(storeRegCol, cnames_VegTypes[i]);
								strcat(storeRegCol, "_STD");
								strcat(storeRegCol, _SepSplit);
							}
						#endif
					}
					strcat(storeRegCol, "EvapSurface_Water");
					strcat(storeRegCol, _SepSplit);
					#ifdef STEPWAT
						if(std_headers){
							strcat(storeRegCol, "EvapSurface_Water_STD"); // store value name in new string
							strcat(storeRegCol, _SepSplit);
						}
					#endif
				}
				else if(strcmp(key2str[colHeadersLoop], "INTERCEPTION")==0){
					for (i = 0; i < NVEGTYPES + 2; i++){
						strcat(storeRegCol, "Interception_");
						strcat(storeRegCol, cnames_VegTypes[i]);
						strcat(storeRegCol, _SepSplit);
						#ifdef STEPWAT
							if(std_headers){
								strcat(storeRegCol, "Interception_"); // store value name in new string
								strcat(storeRegCol, cnames_VegTypes[i]);
								strcat(storeRegCol, "_STD");
								strcat(storeRegCol, _SepSplit);
							}
						#endif
					}
				}
				else if(strcmp(key2str[colHeadersLoop], "SNOWPACK")==0){
					for (i = 0; i < ncol_OUT[eSW_SnowPack]; i++) {
						strcat(storeRegCol, cnames_eSW_SnowPack[i]);
						strcat(storeRegCol, _SepSplit);
						#ifdef STEPWAT
							if(std_headers){
								strcat(storeRegCol, cnames_eSW_SnowPack[i]); // store value name in new string
								strcat(storeRegCol, "_STD");
								strcat(storeRegCol, _SepSplit);
							}
						#endif
					}
				}
				else if(strcmp(key2str[colHeadersLoop], "CO2EFFECTS")==0){
					for (i = 0; i < ncol_OUT[eSW_CO2Effects]; i++) {
						strcat(storeRegCol, cnames_eSW_CO2Effects[i]);
						strcat(storeRegCol, _SepSplit);
						#ifdef STEPWAT
							if(std_headers){
								strcat(storeRegCol, cnames_eSW_CO2Effects[i]); // store value name in new string
								strcat(storeRegCol, "_STD");
								strcat(storeRegCol, _SepSplit);
							}
						#endif
					}
				}
				else{
					strcat(storeRegCol, key2str[colHeadersLoop]); // concatenate variable to string
					strcat(storeRegCol, _SepSplit);
					#ifdef STEPWAT
						if(std_headers){
							strcat(storeRegCol, key2str[colHeadersLoop]); // store value name in new string
							strcat(storeRegCol, "_STD");
							strcat(storeRegCol, _SepSplit);
						}
					#endif
				}
				strcat((char*)colHeaders, storeRegCol); // concatenate variable to string
			}
		}
	}
		switch(outFileTimestep)
		{
			case(1):
				col1Head = "Year";
				col2Head = "Day";
				if(SW_File_Status.make_soil)
					fprintf(soil_file, "%s%c%s%c%s\n", col1Head, _Sep, col2Head, _Sep, (char*)colHeadersSoil); // write columns to file
				if(SW_File_Status.make_regular)
					fprintf(regular_file, "%s%c%s%c%s\n", col1Head, _Sep, col2Head, _Sep, (char*)colHeaders); // write columns to file
				break;
			case(2):
				col1Head = "Year";
				col2Head = "Week";
				if(SW_File_Status.make_soil)
					fprintf(soil_file, "%s%c%s%c%s\n", col1Head, _Sep, col2Head, _Sep, (char*)colHeadersSoil); // write columns to file
				if(SW_File_Status.make_regular)
					fprintf(regular_file, "%s%c%s%c%s\n", col1Head, _Sep, col2Head, _Sep, (char*)colHeaders); // write columns to file
				break;
			case(3):
				col1Head = "Year";
				col2Head = "Month";
				if(SW_File_Status.make_soil)
					fprintf(soil_file, "%s%c%s%c%s\n", col1Head, _Sep, col2Head, _Sep, (char*)colHeadersSoil); // write columns to file
				if(SW_File_Status.make_regular)
					fprintf(regular_file, "%s%c%s%c%s\n", col1Head, _Sep, col2Head, _Sep, (char*)colHeaders); // write columns to file
				break;
			case(4):
				col1Head = "Year";
				if(SW_File_Status.make_soil)
					fprintf(soil_file, "%s%c%s\n", col1Head, _Sep, (char*)colHeadersSoil); // write columns to file
				if(SW_File_Status.make_regular)
					fprintf(regular_file, "%s%c%s\n", col1Head, _Sep, (char*)colHeaders); // write columns to file
				break;
		}
}
#endif
/**
  \fn void stat_Output_Daily_CSV_Summary(int iteration)
  Creates daily files for SOILWAT standalone and for STEPWAT depending on defined flags
	for STEPWAT. If -i flag is used it creates file for each iteration naming file based on iteration.
	If -o flag is used then only 1 set of files is created, not individual iterations.
  \param iteration. Current iteration for file name if -i flag used in STEPWAT
*/
/***********************************************************/
void stat_Output_Daily_CSV_Summary(int iteration)
{
	if(iteration == -1){ // just storing average values over all iterations or soilwat standalone
    if(SW_File_Status.make_regular)
      SW_File_Status.fp_dy_avg = OpenFile(SW_F_name(eOutputDaily), "w");
    if(SW_File_Status.make_soil)
      SW_File_Status.fp_dy_soil_avg = OpenFile(SW_F_name(eOutputDaily_soil), "w");
	}
	else{ // storing values for every iteration
		if(iteration > 1){
      if(SW_File_Status.make_regular)
			   CloseFile(&SW_File_Status.fp_dy);
      if(SW_File_Status.make_soil)
			   CloseFile(&SW_File_Status.fp_dy_soil);
		}

    char *extension; // extension to add to end of file
    char iterationToString[10];

    if(SW_File_Status.make_regular){
        char *newFile_split; // new file to create
        char newFile[80];
        char *fileDup = (char *)malloc(strlen(SW_F_name(eOutputDaily))+1);

        strcpy(fileDup, SW_F_name(eOutputDaily)); // copy file name to new variable

        sprintf(iterationToString, "%d", iteration); // convert iteration from int to string
    		extension = strtok(fileDup, ".");
    		extension = strtok(NULL, "."); // get the extension to add to new file (not hardcoding since can have different extensions)

    		newFile_split = strtok(SW_F_name(eOutputDaily), "."); // get filename up to but not including ".csv"
    		strcpy(newFile, newFile_split);
    		strcat(newFile, "_");
    		strcat(newFile, iterationToString);
    		strcat(newFile, ".");
    		strcat(newFile, extension);
    		SW_File_Status.fp_dy = OpenFile(newFile, "w"); // open new file

        free(fileDup);
    }
    if(SW_File_Status.make_soil){
  		char *newFile_soil_split;
  		char newFile_soil[80];
  		char *fileDup_soil = (char *)malloc(strlen(SW_F_name(eOutputDaily_soil))+1);

  		strcpy(fileDup_soil, SW_F_name(eOutputDaily_soil)); // copy file name to new variable

  		sprintf(iterationToString, "%d", iteration); // convert iteration from int to string
  		extension = strtok(fileDup_soil, ".");
  		extension = strtok(NULL, "."); // get the extension to add to new file (not hardcoding since can have different extensions)

  		newFile_soil_split = strtok(SW_F_name(eOutputDaily_soil), "."); // get filename up to but not including ".csv"
  		strcpy(newFile_soil, newFile_soil_split);
  		strcat(newFile_soil, "_");
  		strcat(newFile_soil, iterationToString);
  		strcat(newFile_soil, ".");
  		strcat(newFile_soil, extension);
  		SW_File_Status.fp_dy_soil = OpenFile(newFile_soil, "w"); // open new file

  		free(fileDup_soil);
    }
	}

	SW_File_Status.use_Day = swTRUE;
}

/**
  \fn void stat_Output_Weekly_CSV_Summary(int iteration)
  Creates weekly files for SOILWAT standalone and for STEPWAT depending on defined flags
	for STEPWAT. If -i flag is used it creates file for each iteration naming file based on iteration.
	If -o flag is used then only 1 set of files is created, not individual iterations.
  \param iteration. Current iteration for file name if -i flag used in STEPWAT
*/
/***********************************************************/
void stat_Output_Weekly_CSV_Summary(int iteration)
{
	if(iteration == -1){ // just storing average values over all iterations
    if(SW_File_Status.make_regular)
		  SW_File_Status.fp_wk_avg = OpenFile(SW_F_name(eOutputWeekly), "w");
    if(SW_File_Status.make_soil)
		  SW_File_Status.fp_wk_soil_avg = OpenFile(SW_F_name(eOutputWeekly_soil), "w");
	}
	else{ // storing values for every iteration
		if(iteration > 1){
      if(SW_File_Status.make_regular)
			   CloseFile(&SW_File_Status.fp_wk);
      if(SW_File_Status.make_soil)
        CloseFile(&SW_File_Status.fp_wk_soil);
		}

    char *extension; // extension to add to end of file
    char iterationToString[10];

    if(SW_File_Status.make_regular){
        char *newFile_split; // new file to create
        char newFile[80];
        char *fileDup = (char *)malloc(strlen(SW_F_name(eOutputWeekly))+1);
        strcpy(fileDup, SW_F_name(eOutputWeekly)); // copy file name to new variable

        sprintf(iterationToString, "%d", iteration); // convert iteration from int to string
    		extension = strtok(fileDup, ".");
    		extension = strtok(NULL, "."); // get the extension to add to new file (not hardcoding since can have different extensions)

        newFile_split = strtok(SW_F_name(eOutputWeekly), "."); // get filename up to but not including ".csv"
    		strcpy(newFile, newFile_split);
    		strcat(newFile, "_");
    		strcat(newFile, iterationToString);
    		strcat(newFile, ".");
    		strcat(newFile, extension);
    		SW_File_Status.fp_wk = OpenFile(newFile, "w"); // open new file

        free(fileDup);
    }

    if(SW_File_Status.make_soil){
  		char *newFile_soil_split;
  		char newFile_soil[80];

  		char *fileDup_soil = (char *)malloc(strlen(SW_F_name(eOutputWeekly_soil))+1);

  		strcpy(fileDup_soil, SW_F_name(eOutputWeekly_soil)); // copy file name to new variable

  		sprintf(iterationToString, "%d", iteration); // convert iteration from int to string
  		extension = strtok(fileDup_soil, ".");
  		extension = strtok(NULL, "."); // get the extension to add to new file (not hardcoding since can have different extensions)

  		newFile_soil_split = strtok(SW_F_name(eOutputWeekly_soil), "."); // get filename up to but not including ".csv"
  		strcpy(newFile_soil, newFile_soil_split);
  		strcat(newFile_soil, "_");
  		strcat(newFile_soil, iterationToString);
  		strcat(newFile_soil, ".");
  		strcat(newFile_soil, extension);
  		SW_File_Status.fp_wk_soil = OpenFile(newFile_soil, "w"); // open new file

  		free(fileDup_soil);
    }
	}
	SW_File_Status.use_Week = swTRUE;
}

/**
  \fn void stat_Output_Monthly_CSV_Summary(int iteration)
  Creates monthly files for SOILWAT standalone and for STEPWAT depending on defined flags
	for STEPWAT. If -i flag is used it creates file for each iteration naming file based on iteration.
	If -o flag is used then only 1 set of files is created, not individual iterations.
  \param iteration. Current iteration for file name if -i flag used in STEPWAT
*/
/***********************************************************/
void stat_Output_Monthly_CSV_Summary(int iteration)
{
	if(iteration == -1){ // just storing average values over all iterations
    if(SW_File_Status.make_regular)
		  SW_File_Status.fp_mo_avg = OpenFile(SW_F_name(eOutputMonthly), "w");
    if(SW_File_Status.make_soil)
		  SW_File_Status.fp_mo_soil_avg = OpenFile(SW_F_name(eOutputMonthly_soil), "w");
	}
	else{ // storing values for every iteration
		if(iteration > 1){
      if(SW_File_Status.make_regular)
			   CloseFile(&SW_File_Status.fp_mo);
      if(SW_File_Status.make_soil)
			   CloseFile(&SW_File_Status.fp_mo_soil);
		}

    char *extension; // extension to add to end of file
    char iterationToString[10];

    if(SW_File_Status.make_regular){
      char *newFile_split; // new file to create
      char newFile[80];
      char *fileDup = (char *)malloc(strlen(SW_F_name(eOutputMonthly))+1);

      strcpy(fileDup, SW_F_name(eOutputMonthly)); // copy file name to new variable
      sprintf(iterationToString, "%d", iteration); // convert iteration from int to string
  		extension = strtok(fileDup, ".");
  		extension = strtok(NULL, "."); // get the extension to add to new file (not hardcoding since can have different extensions)

      newFile_split = strtok(SW_F_name(eOutputMonthly), "."); // get filename up to but not including ".csv"
  		strcpy(newFile, newFile_split);
  		strcat(newFile, "_");
  		strcat(newFile, iterationToString);
  		strcat(newFile, ".");
  		strcat(newFile, extension);
  		SW_File_Status.fp_mo = OpenFile(newFile, "w"); // open new file

      free(fileDup);
    }
    if(SW_File_Status.make_soil){
  		char *newFile_soil_split;
  		char newFile_soil[80];
  		char *fileDup_soil = (char *)malloc(strlen(SW_F_name(eOutputMonthly_soil))+1);

  		strcpy(fileDup_soil, SW_F_name(eOutputMonthly_soil)); // copy file name to new variable
  		sprintf(iterationToString, "%d", iteration); // convert iteration from int to string

  		extension = strtok(fileDup_soil, ".");
  		extension = strtok(NULL, "."); // get the extension to add to new file (not hardcoding since can have different extensions)

  		newFile_soil_split = strtok(SW_F_name(eOutputMonthly_soil), "."); // get filename up to but not including ".csv"
  		strcpy(newFile_soil, newFile_soil_split);
  		strcat(newFile_soil, "_");
  		strcat(newFile_soil, iterationToString);
  		strcat(newFile_soil, ".");
  		strcat(newFile_soil, extension);
  		SW_File_Status.fp_mo_soil = OpenFile(newFile_soil, "w"); // open new file

  		free(fileDup_soil);
    }
	}
	SW_File_Status.use_Month = swTRUE;
}

/**
  \fn void stat_Output_Yearly_CSV_Summary(int iteration)
  Creates yearly files for SOILWAT standalone and for STEPWAT depending on defined flags
	for STEPWAT. If -i flag is used it creates file for each iteration naming file based on iteration.
	If -o flag is used then only 1 set of files is created, not individual iterations.
  \param iteration. Current iteration for file name if -i flag used in STEPWAT
*/
/***********************************************************/
void stat_Output_Yearly_CSV_Summary(int iteration)
{
	if(iteration == -1){ // just storing average values over all iterations
    if(SW_File_Status.make_regular)
		  SW_File_Status.fp_yr_avg = OpenFile(SW_F_name(eOutputYearly), "w");
    if(SW_File_Status.make_soil)
      SW_File_Status.fp_yr_soil_avg = OpenFile(SW_F_name(eOutputYearly_soil), "w");
	}
	else{ // storing values for every iteration
		if(iteration > 1){
      if(SW_File_Status.make_regular)
			   CloseFile(&SW_File_Status.fp_yr);
      if(SW_File_Status.make_soil)
        CloseFile(&SW_File_Status.fp_yr_soil);
		}

    char *extension; // extension to add to end of file
    char iterationToString[10];

    if(SW_File_Status.make_regular){
      char *newFile_split; // new file to create
      char newFile[80];
      char *fileDup = (char *)malloc(strlen(SW_F_name(eOutputYearly))+1);

      strcpy(fileDup, SW_F_name(eOutputYearly)); // copy file name to new variable
      sprintf(iterationToString, "%d", iteration); // convert iteration from int to string

      extension = strtok(fileDup, ".");
  		extension = strtok(NULL, "."); // get the extension to add to new file (not hardcoding since can have different extensions)

  		newFile_split = strtok(SW_F_name(eOutputYearly), "."); // get filename up to but not including ".csv"
  		strcpy(newFile, newFile_split);
  		strcat(newFile, "_");
  		strcat(newFile, iterationToString);
  		strcat(newFile, ".");
  		strcat(newFile, extension);
  		SW_File_Status.fp_yr = OpenFile(newFile, "w"); // open new file

      free(fileDup);
    }

    if(SW_File_Status.make_soil){
  		char *newFile_soil_split;
  		char newFile_soil[80];
  		char *fileDup_soil = (char *)malloc(strlen(SW_F_name(eOutputYearly_soil))+1);

  		strcpy(fileDup_soil, SW_F_name(eOutputYearly_soil)); // copy file name to new variable
  		sprintf(iterationToString, "%d", iteration); // convert iteration from int to string

      extension = strtok(fileDup_soil, ".");
      extension = strtok(NULL, "."); // get the extension to add to new file (not hardcoding since can have different extensions)

  		newFile_soil_split = strtok(SW_F_name(eOutputYearly_soil), "."); // get filename up to but not including ".csv"
  		strcpy(newFile_soil, newFile_soil_split);
  		strcat(newFile_soil, "_");
  		strcat(newFile_soil, iterationToString);
  		strcat(newFile_soil, ".");
  		strcat(newFile_soil, extension);
  		SW_File_Status.fp_yr_soil = OpenFile(newFile_soil, "w"); // open new file

  		free(fileDup_soil);
    }
	}
	SW_File_Status.use_Year = swTRUE;
}


#ifdef DEBUG_MEM
#include "myMemory.h"
/*======================================================*/
void SW_OUT_SetMemoryRefs( void)
{
	/* when debugging memory problems, use the bookkeeping
	 code in myMemory.c
	 This routine sets the known memory refs in this module
	 so they can be  checked for leaks, etc.  Includes
	 malloc-ed memory in SOILWAT.  All refs will have been
	 cleared by a call to ClearMemoryRefs() before this, and
	 will be checked via CheckMemoryRefs() after this, most
	 likely in the main() function.
	 */
	OutKey k;

	ForEachOutKey(k)
	{
		if (SW_Output[k].use)
		NoteMemoryRef(SW_Output[k].outfile);
	}

}

#endif

/*==================================================================

 Description of the algorithm.

 There is a structure array (SW_OUTPUT) that contains the
 information from the outsetup.in file. This structure is filled in
 the initialization process by matching defined macros of valid keys
 with enumeration variables used as indices into the structure
 array.  A similar combination of text macros and enumeration
 constants handles the TIMEPERIOD conversion from text to numeric
 index.

 Each structure element of the array contains the output period
 code, start and end values, output file name, opened file pointer
 for output, on/off status, and a pointer to the function that
 prepares a complete line of formatted output per output period.

 A _construct() function clears the entire structure array to set
 values and flags to zero. Those output objects that are
 turned off are ignored.
 Thus, to add a new output variable, a new get_function must be added to
 in addition to adding the new macro and enumeration keys
 for it.  Oh, and a line or two of summarizing code.

 After initialization, each valid output key has an element in the
 structure array that "knows" its parameters and whether it is on or
 off.  There is still space allocated for the "off" keys but they
 are ignored by the use flag.

 During the daily execution loop of the model, values for each of
 the output objects are accumulated via a call to
 SW_OUT_sum_today(x) function with x being a special enumeration
 code that defines the actual module object to be summed (see
 SW_Output.h).  This enumeration code breaks up the many output
 variables into a few simple types so that adding a new output
 variable is simplified by putting it into its proper category.

 When the _sum_today() function is called, it calls the averaging
 function which puts the sum, average, etc into the output
 accumulators--(dy|wk|mo|yr)avg--then conditionally clears the
 summary accumulators--(dy|wk|mo|yr)sum--if a new period has
 occurred (in preparation for the new period), then calls the
 function to handle collecting the summaries called collect_sums().

 The collect_sums() function needs the object type (eg, eSWC, eWTH)
 and the output period (eg, dy, wk, etc) and then, for each valid
 output key, it assigns a pointer to the appropriate object's
 summary sub-structure.  (This is where the complexity of this
 approach starts to become a bit clumsy, but it nonetheless tends to
 keep the overall code size down.) After assigning the pointer to
 the summary structure, the pointers are passed to a routine to
 actually do the accumulation for the various output objects
 (currently SWC and WTH).  No other arithmetic is performed here.
 This routine is only called, however, if the current day or period
 falls within the range specified by the user.  Otherwise, the
 accumulators will remain zero.  Also, the period check is used in
 other places to determine whether to bother with averaging and
 printing.

 Once a period other than daily has passed, the accumulated values
 are averaged or summed as appropriate within the average_for()
 subroutine as mentioned above.

 After the averaging function, the values are ready to format for
 output.  The SW_OUT_write_today() routine is called from the
 end_day() function in main(). Throughout the run for each period
 all used values are appended to a string and at the end of the period
 the string is written to the proper output file. The SW_OUT_write_today()
 function goes through each key and if in use, it calls populate_output_values()
 function to parse the output string and format it properly. After the string
 is formatted it is added to an output string which is written to the output File
 at the end of the period.

 So to summarize, adding another output quantity requires several steps.
 - Add an appropriate element to the SW_*_OUTPUTS substructure of the
 main object (eg SW_Soilwat) to hold the output value.
 - Define a new key string and add a macro definition and enumeration
 to the appropriate list in Output.h.  Be sure the new key's position
 in the list doesn't interfere with the ForEach*() loops.
 - Increase the value of SW_OUTNKEYS macro in Output.h.
 - Add the macro and enum keys to the key2str and key2obj lists in
 SW_Output.c as appropriate, IN THE SAME LIST POSITION.
 - Create and declare a get_*() function that returns the correctly
 formatted string for output.
 - Add a line to link the get_ function to the appropriate element in
 the SW_OUTPUT array in _construct().
 - Add new code to the switch statement in sumof_*() to handle the new
 key.
 - Add new code to the switch statement in average_for() to do the
 summarizing.
 - Add new code to create_col_headers to make proper columns for new value
 - if variable is a soil variable (has layers) add name to SW_OUT_read, create_col_headers
 		and populate_output_values in the if block checking for SOIL variables
		looks like below code
		if((strcmp(key2str[output_var], "VWCBULK")==0 || strcmp(key2str[output_var], "VWCMATRIC")==0 || strcmp(key2str[output_var], "SWCBULK")==0
			|| strcmp(key2str[output_var], "SWABULK")==0
			|| strcmp(key2str[output_var], "EVAPSOIL")==0 || strcmp(key2str[output_var], "TRANSP")==0 || strcmp(key2str[output_var], "WETDAY")==0
			|| strcmp(key2str[output_var], "LYRDRAIN")==0 || strcmp(key2str[output_var], "SOILTEMP")==0 || strcmp(key2str[output_var], "HYDRED")==0
			|| strcmp(key2str[output_var], "SWAMATRIC")==0 || strcmp(key2str[output_var], "SWPMATRIC")==0 || strcmp(key2str[output_var], "SWA")==0))
		{
	----------------------------
	To make new values work with STEPWAT do the following
	- add average storage variable to sxw.h in the soilwat_average structure
	- add memory allocation to _make_soil_arrays function in sxw.c
	- add call to mem_Free for variable in free_all_sxw_memory in sxw.c
	- add #ifdef STEPWAT code to the get_* function that calculates the average over iterations



 That should do it.  However, new code is about to be added to Output.c
 and outsetup.in that will allow quantities to be summarized by summing
 or averaging.  Possibly in the future, more types of options will be
 added (eg, geometric average, stddev, who knows).  Thus, new keys will
 be needed to handle those operations within the average_for()
 function, but the rest of the code will be the same.


 Comment (06/23/2015, akt): Adding Output at SOILWAT for further using at RSOILWAT and STEP as well

 Above details is good enough for knowing how to add a new output at soilwat.
 However here we are adding some more details about how we can add this output for further using that to RSOILWAT and STEP side as well.

 At the top with Comment (06/23/2015, drs): details about how output of SOILWAT works.

 Example : Adding extra place holder at existing output of SOILWAT for both STEP and RSOILWAT:
 - Adding extra place holder for existing output for both STEP and RSOILWAT: example adding extra output surfaceTemp at SW_WEATHER.
 We need to modified SW_Weather.h with adding a placeholder at SW_WEATHER and at inner structure SW_WEATHER_OUTPUTS.
 - Then somewhere this surfaceTemp value need to set at SW_WEATHER placeholder, here we add this atSW_Flow.c
 - Further modify file SW_Output.c ; add sum of surfaceTemp at function sumof_wth(). Then use this
 sum value to calculate average of surfaceTemp at function average_for().
 - Then go to function get_temp(), add extra placeholder like surfaceTempVal that will store this average surfaceTemp value.
 Add this value to both STEP and RSOILWAT side code of this function for all the periods like weekly, monthly and yearly (for
 daily set day sum value of surfaceTemp not avg), add this surfaceTempVal at end of this get_Temp() function for finally
 printing in output file.
 - Pass this surfaceTempVal to sxw.h file from STEP, by adding extra placeholder at sxw.h so that STEP model can use this value there.
 - For using this surfaceTemp value in RSOILWAT side of function get_Temp(), increment index of p_Rtemp output array
 by one and add this sum value  for daily and avg value for other periods at last index.
 - Further need to modify SW_R_lib.c, for newOutput we need to add new pointers;
 functions start() and onGetOutput() will need to be modified. For this example adding extra placeholder at existing TEMP output so
 only function onGetOutput() need to be modified; add placeholder name for surfaceTemp at array Ctemp_names[] and then 	increment
 number of columns for Rtemp outputs (Rtemp_columns) by one.
 - At RSOILWAT further we will need to modify L_swOutput.R and G_swOut.R. At L_swOutput.R increment number of columns for swOutput_TEMP.

 So to summarize, adding extra place holder at existing output of SOILWAT for both STEP and RSOILWAT side code above steps are useful.

 However, adding another new output quantity requires several steps for SOILWAT and both STEP and RSOILWAT side code as well.
 So adding more information to above details (for adding  another new output quantity that can further use in both STEP and RSOILWAT) :
 - We need to modify SW_R_lib.c of SOILWAT; add new pointers; functions start()  and onGetOutput() will need to be modified.
 - The sw_output.c of SOILWAT will need to be modified for new output quantity; add new pointers here too for RSOILWAT.
 - We will need to also read in the new config params from outputsetup_v30.in ; then we  will need to accumulate the new values ;
 write them out to file and assign the values to the RSOILWAT pointers.
 - At RSOILWAT we will need to modify L_swOutput.R and G_swOut.R

 */
