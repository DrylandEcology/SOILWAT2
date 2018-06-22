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

// Array-based output declarations:
#ifdef SW_OUTARRAY
#include "SW_Output_outarray.h"
#endif

// Text-based output declarations:
#ifdef SW_OUTTEXT
#include "SW_Output_outtext.h"
#endif

/* Note: `get_XXX` functions are declared in `SW_Output.h`
    and defined/implemented in 'SW_Output_get_functions.c"
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

SW_OUTPUT SW_Output[SW_OUTNKEYS];

char _Sep; /* output delimiter */
TimeInt tOffset; /* 1 or 0 means we're writing previous or current period */


// Global variables describing output periods:
OutPeriod timeSteps[SW_OUTNKEYS][SW_OUTNPERIODS];// array to keep track of the periods that will be used for each output
IntUS used_OUTNPERIODS; // number of different time steps/periods that are used/requested
Bool use_OutPeriod[SW_OUTNPERIODS]; // TRUE if time step/period is active for any output key


// Global variables describing size and names of output
char *colnames_OUT[SW_OUTNKEYS][5 * NVEGTYPES + MAX_LAYERS]; // names of output columns for each output key; number is an expensive guess
IntUS ncol_OUT[SW_OUTNKEYS]; // number of output columns for each output key


// Text-based output: defined in `SW_Output_outtext.c`:
#ifdef SW_OUTTEXT
extern SW_FILE_STATUS SW_OutFiles;
extern char sw_outstr[];
extern Bool print_IterationSummary;
extern Bool print_SW_Output;
#endif


// Array-based output: defined in `SW_Output_outarray.c`
#ifdef SW_OUTARRAY
extern IntUS ncol_TimeOUT[];
extern size_t nrow_OUT[];
extern size_t irow_OUT[];
#endif


#ifdef STEPWAT
extern char sw_outstr_agg[];

/** `storeAllIterations` is set to TRUE if STEPWAT2 is called with `-i` flag
     if TRUE, then write to disk the SOILWAT2 output
     for each STEPWAT2 iteration/repeat to separate files */
Bool storeAllIterations;

/** `prepare_IterationSummary` is set to TRUE if STEPWAT2 is called with
      `-o` flag; if TRUE, then calculate/write to disk the running mean and sd
      across iterations/repeats */
Bool prepare_IterationSummary;
#endif


// Convert from IDs to strings
/* These MUST be in the same order as enum OutKey in
 * SW_Output.h */
char const *key2str[] =
{ // weather/atmospheric quantities:
	SW_WETHR, SW_TEMP, SW_PRECIP, SW_SOILINF, SW_RUNOFF,
	// soil related water quantities:
	SW_ALLH2O, SW_VWCBULK, SW_VWCMATRIC, SW_SWCBULK, SW_SWABULK, SW_SWAMATRIC,
		SW_SWA, SW_SWPMATRIC, SW_SURFACEW, SW_TRANSP, SW_EVAPSOIL, SW_EVAPSURFACE,
		SW_INTERCEPTION, SW_LYRDRAIN, SW_HYDRED, SW_ET, SW_AET, SW_PET, SW_WETDAY,
		SW_SNOWPACK, SW_DEEPSWC, SW_SOILTEMP,
	// vegetation quantities:
	SW_ALLVEG, SW_ESTAB,
	// vegetation other:
	SW_CO2EFFECTS
};

/* converts an enum output key (OutKey type) to a module  */
/* or object type. see SW_Output.h for OutKey order.         */
/* MUST be SW_OUTNKEYS of these */
ObjType key2obj[] =
{ // weather/atmospheric quantities:
	eWTH, eWTH, eWTH, eWTH, eWTH,
	// soil related water quantities:
	eSWC, eSWC, eSWC, eSWC, eSWC, eSWC, eSWC, eSWC, eSWC, eSWC, eSWC, eSWC, eSWC,
		eSWC, eSWC, eSWC, eSWC, eSWC, eSWC, eSWC, eSWC, eSWC,
	// vegetation quantities:
	eVES, eVES,
	// vegetation other:
	eVPD
};

char const *pd2str[] =
	{ SW_DAY, SW_WEEK, SW_MONTH, SW_YEAR };

char const *pd2longstr[] =
	{ SW_DAY_LONG, SW_WEEK_LONG, SW_MONTH_LONG, SW_YEAR_LONG };

char const *styp2str[] =
{ SW_SUM_OFF, SW_SUM_SUM, SW_SUM_AVG, SW_SUM_FNL };


/* =================================================== */
/*                Module-Level Variables               */
/* --------------------------------------------------- */
static char *MyFileName;

static int useTimeStep; /* flag to determine whether or not the line TIMESTEP exists */
static Bool bFlush_output; /* process partial period ? */


/* =================================================== */
/* =================================================== */
/*             Private Function Declarations            */
/* --------------------------------------------------- */

static OutPeriod str2period(char *s);
static OutKey str2key(char *s);
static OutSum str2stype(char *s);

static void collect_sums(ObjType otyp, OutPeriod op);
static void sumof_wth(SW_WEATHER *v, SW_WEATHER_OUTPUTS *s, OutKey k);
static void sumof_swc(SW_SOILWAT *v, SW_SOILWAT_OUTPUTS *s, OutKey k);
static void sumof_ves(SW_VEGESTAB *v, SW_VEGESTAB_OUTPUTS *s, OutKey k);
static void sumof_vpd(SW_VEGPROD *v, SW_VEGPROD_OUTPUTS *s, OutKey k);
static void average_for(ObjType otyp, OutPeriod pd);

#ifdef STEPWAT
static int check_STEPWAT2_output_requirements(char msg[]);
#endif

/* =================================================== */
/* =================================================== */
/*             Private Function Definitions            */
/* --------------------------------------------------- */

/** Convert string representation of time period to `OutPeriod` value.
*/
static OutPeriod str2period(char *s)
{
	IntUS pd;
	for (pd = 0; Str_CompareI(s, (char *)pd2str[pd]) && pd < SW_OUTNPERIODS; pd++);

	return (OutPeriod) pd;
}

/** Convert string representation of output type to `OutKey` value.
*/
static OutKey str2key(char *s)
{
	IntUS key;

	for (key = 0; key < SW_OUTNKEYS && Str_CompareI(s, (char *)key2str[key]); key++) ;
	if (key == SW_OUTNKEYS)
	{
		LogError(logfp, LOGFATAL, "%s : Invalid key (%s) in %s", SW_F_name(eOutput), s);
	}
	return (OutKey) key;
}

/** Convert string representation of output aggregation function to `OutSum` value.
*/
static OutSum str2stype(char *s)
{
	IntUS styp;

	for (styp = eSW_Off; styp < SW_NSUMTYPES && Str_CompareI(s, (char *)styp2str[styp]); styp++) ;
	if (styp == SW_NSUMTYPES)
	{
		LogError(logfp, LOGFATAL, "%s : Invalid summary type (%s)\n", SW_F_name(eOutput), s);
	}
	return (OutSum) styp;
}


/** Checks whether a output variable (key) comes with soil layer or not.
		See also function `has_keyname_soillayers`.

    \param k. The key of output variable (key), i.e., one of `OutKey`.
    \return `TRUE` if `var` comes with soil layers; `FALSE` otherwise.
*/
Bool has_key_soillayers(OutKey k) {
	Bool has;

	has = (
			k == eSW_VWCBulk ||
			k == eSW_VWCMatric ||
			k == eSW_SWCBulk ||
			k == eSW_SWABulk ||
			k == eSW_SWAMatric ||
			k == eSW_SWA ||
			k == eSW_SWPMatric ||
			k == eSW_Transp ||
			k == eSW_EvapSoil ||
			k == eSW_LyrDrain ||
			k == eSW_HydRed ||
			k == eSW_WetDays ||
			k == eSW_SoilTemp
		) ? swTRUE : swFALSE;

	return(has);
}


/** Checks whether a output variable (key) comes with soil layer or not
		See also function `has_key_soillayers`.

    \param var. The name of an output variable (key), i.e., one of `key2str`.
    \return `TRUE` if `var` comes with soil layers; `FALSE` otherwise.
*/
Bool has_keyname_soillayers(const char *var) {
	Bool has;

	has = (
			strcmp(var, SW_VWCBULK) == 0 ||
			strcmp(var, SW_VWCMATRIC) == 0 ||
			strcmp(var, SW_SWCBULK) == 0 ||
			strcmp(var, SW_SWABULK) == 0 ||
			strcmp(var, SW_SWAMATRIC) == 0 ||
			strcmp(var, SW_SWA) == 0 ||
			strcmp(var, SW_SWPMATRIC) == 0 ||
			strcmp(var, SW_TRANSP) == 0 ||
			strcmp(var, SW_EVAPSOIL) == 0 ||
			strcmp(var, SW_LYRDRAIN) == 0 ||
			strcmp(var, SW_HYDRED) == 0 ||
			strcmp(var, SW_WETDAY) == 0 ||
			strcmp(var, SW_SOILTEMP) == 0
		) ? swTRUE : swFALSE;

	return(has);
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
		ForEachSoilLayer(i) {
			ForEachVegType(j) {
				s->SWA_VegType[j][i] += v->dSWA_repartitioned_sum[j][i];
			}
		}
		break;

	case eSW_SurfaceWater:
		s->surfaceWater += v->surfaceWater;
		break;

	case eSW_Transp:
		ForEachSoilLayer(i) {
			ForEachVegType(j) {
				s->transp_total[i] += v->transpiration[j][i];
				s->transp[j][i] += v->transpiration[j][i];
			}
		}
		break;

	case eSW_EvapSoil:
		ForEachEvapLayer(i)
			s->evap[i] += v->evaporation[i];
		break;

	case eSW_EvapSurface:
		ForEachVegType(j) {
			s->total_evap += v->evap_veg[j];
			s->evap_veg[j] += v->evap_veg[j];
		}
		s->total_evap += v->litter_evap + v->surfaceWater_evap;
		s->litter_evap += v->litter_evap;
		s->surfaceWater_evap += v->surfaceWater_evap;
		break;

	case eSW_Interception:
		ForEachVegType(j) {
			s->total_int += v->int_veg[j];
			s->int_veg[j] += v->int_veg[j];
		}
		s->total_int += v->litter_int;
		s->litter_int += v->litter_int;
		break;

	case eSW_LyrDrain:
		for (i = 0; i < SW_Site.n_layers - 1; i++)
			s->lyrdrain[i] += v->drain[i];
		break;

	case eSW_HydRed:
		ForEachSoilLayer(i) {
			ForEachVegType(j) {
				s->hydred_total[i] += v->hydred[j][i];
				s->hydred[j][i] += v->hydred[j][i];
			}
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


/** separates the task of obtaining a periodic average.
   no need to average days, so this should never be
   called with eSW_Day.
   Enter this routine just after the summary period
   is completed, so the current week and month will be
   one greater than the period being summarized.
*/
static void average_for(ObjType otyp, OutPeriod pd) {
	SW_SOILWAT *s = &SW_Soilwat;
	SW_WEATHER *w = &SW_Weather;
	SW_VEGPROD *vp = &SW_VegProd;
	TimeInt curr_pd = 0;
	RealD div = 0.; /* if sumtype=AVG, days in period; if sumtype=SUM, 1 */
	OutKey k;
	LyrIndex i;
	int j;

	if (otyp == eVES)
		return;

	if (pd == eSW_Day)
	{
		// direct day-aggregation pointers to day-accumulators, instead of
		// expensive copying as required for other time periods when possibly
		// !EQ(div, 1.)
		switch (otyp)
		{
			case eSWC:
				s->p_oagg[pd] = s->p_accu[pd];
				break;
			case eWTH:
				w->p_oagg[pd] = w->p_accu[pd];
				break;
			case eVPD:
				vp->p_oagg[pd] = vp->p_accu[pd];
				break;
			case eVES:
				break;
			default:
				LogError(logfp, LOGFATAL,
						"Invalid object type in average_for().");
		}

	}
	else {
		// carefully aggregate for specific time period and aggregation type (mean, sum, final value)
		ForEachOutKey(k)
		{
			if (!SW_Output[k].use) {
				continue;
			}

			switch (pd)
			{
				case eSW_Week:
					curr_pd = (SW_Model.week + 1) - tOffset;
					div = (bFlush_output) ? SW_Model.lastdoy % WKDAYS : WKDAYS;
					break;

				case eSW_Month:
					curr_pd = (SW_Model.month + 1) - tOffset;
					div = Time_days_in_month(SW_Model.month - tOffset);
					break;

				case eSW_Year:
					curr_pd = SW_Output[k].first;
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
				w->p_oagg[pd]->temp_max = w->p_accu[pd]->temp_max / div;
				w->p_oagg[pd]->temp_min = w->p_accu[pd]->temp_min / div;
				w->p_oagg[pd]->temp_avg = w->p_accu[pd]->temp_avg / div;
				w->p_oagg[pd]->surfaceTemp = w->p_accu[pd]->surfaceTemp / div;
				break;

			case eSW_Precip:
				w->p_oagg[pd]->ppt = w->p_accu[pd]->ppt / div;
				w->p_oagg[pd]->rain = w->p_accu[pd]->rain / div;
				w->p_oagg[pd]->snow = w->p_accu[pd]->snow / div;
				w->p_oagg[pd]->snowmelt = w->p_accu[pd]->snowmelt / div;
				w->p_oagg[pd]->snowloss = w->p_accu[pd]->snowloss / div;
				break;

			case eSW_SoilInf:
				w->p_oagg[pd]->soil_inf = w->p_accu[pd]->soil_inf / div;
				break;

			case eSW_Runoff:
				w->p_oagg[pd]->snowRunoff = w->p_accu[pd]->snowRunoff / div;
				w->p_oagg[pd]->surfaceRunoff = w->p_accu[pd]->surfaceRunoff / div;
				w->p_oagg[pd]->surfaceRunon = w->p_accu[pd]->surfaceRunon / div;
				break;

			case eSW_SoilTemp:
				ForEachSoilLayer(i) {
					s->p_oagg[pd]->sTemp[i] =
							(SW_Output[k].sumtype == eSW_Fnl) ?
									s->sTemp[i] :
									s->p_accu[pd]->sTemp[i] / div;
				}
				break;

			case eSW_VWCBulk:
				ForEachSoilLayer(i) {
					/* vwcBulk at this point is identical to swcBulk */
					s->p_oagg[pd]->vwcBulk[i] =
							(SW_Output[k].sumtype == eSW_Fnl) ?
									s->swcBulk[Yesterday][i] :
									s->p_accu[pd]->vwcBulk[i] / div;
				}
				break;

			case eSW_VWCMatric:
				ForEachSoilLayer(i) {
					/* vwcMatric at this point is identical to swcBulk */
					s->p_oagg[pd]->vwcMatric[i] =
							(SW_Output[k].sumtype == eSW_Fnl) ?
									s->swcBulk[Yesterday][i] :
									s->p_accu[pd]->vwcMatric[i] / div;
				}
				break;

			case eSW_SWCBulk:
				ForEachSoilLayer(i) {
					s->p_oagg[pd]->swcBulk[i] =
							(SW_Output[k].sumtype == eSW_Fnl) ?
									s->swcBulk[Yesterday][i] :
									s->p_accu[pd]->swcBulk[i] / div;
				}
				break;

			case eSW_SWPMatric:
				ForEachSoilLayer(i) {
					/* swpMatric at this point is identical to swcBulk */
					s->p_oagg[pd]->swpMatric[i] =
							(SW_Output[k].sumtype == eSW_Fnl) ?
									s->swcBulk[Yesterday][i] :
									s->p_accu[pd]->swpMatric[i] / div;
				}
				break;

			case eSW_SWABulk:
				ForEachSoilLayer(i) {
					s->p_oagg[pd]->swaBulk[i] =
							(SW_Output[k].sumtype == eSW_Fnl) ?
									fmax(
											s->swcBulk[Yesterday][i]
													- SW_Site.lyr[i]->swcBulk_wiltpt,
											0.) :
									s->p_accu[pd]->swaBulk[i] / div;
				}
				break;

			case eSW_SWAMatric: /* swaMatric at this point is identical to swaBulk */
				ForEachSoilLayer(i) {
					s->p_oagg[pd]->swaMatric[i] =
							(SW_Output[k].sumtype == eSW_Fnl) ?
									fmax(
											s->swcBulk[Yesterday][i]
													- SW_Site.lyr[i]->swcBulk_wiltpt,
											0.) :
									s->p_accu[pd]->swaMatric[i] / div;
				}
				break;

			case eSW_SWA:
				ForEachSoilLayer(i) {
					ForEachVegType(j) {
						s->p_oagg[pd]->SWA_VegType[j][i] =
								(SW_Output[k].sumtype == eSW_Fnl) ?
										s->dSWA_repartitioned_sum[j][i] :
										s->p_accu[pd]->SWA_VegType[j][i] / div;
					}
				}
				break;

			case eSW_DeepSWC:
				s->p_oagg[pd]->deep =
						(SW_Output[k].sumtype == eSW_Fnl) ?
								s->swcBulk[Yesterday][SW_Site.deep_lyr] :
								s->p_accu[pd]->deep / div;
				break;

			case eSW_SurfaceWater:
				s->p_oagg[pd]->surfaceWater = s->p_accu[pd]->surfaceWater / div;
				break;

			case eSW_Transp:
				ForEachSoilLayer(i)
				{
					s->p_oagg[pd]->transp_total[i] = s->p_accu[pd]->transp_total[i] / div;
					ForEachVegType(j) {
						s->p_oagg[pd]->transp[j][i] = s->p_accu[pd]->transp[j][i] / div;
					}
				}
				break;

			case eSW_EvapSoil:
				ForEachEvapLayer(i)
					s->p_oagg[pd]->evap[i] = s->p_accu[pd]->evap[i] / div;
				break;

			case eSW_EvapSurface:
				s->p_oagg[pd]->total_evap = s->p_accu[pd]->total_evap / div;
				ForEachVegType(j) {
					s->p_oagg[pd]->evap_veg[j] = s->p_accu[pd]->evap_veg[j] / div;
				}
				s->p_oagg[pd]->litter_evap = s->p_accu[pd]->litter_evap / div;
				s->p_oagg[pd]->surfaceWater_evap = s->p_accu[pd]->surfaceWater_evap / div;
				break;

			case eSW_Interception:
				s->p_oagg[pd]->total_int = s->p_accu[pd]->total_int / div;
				ForEachVegType(j) {
					s->p_oagg[pd]->int_veg[j] = s->p_accu[pd]->int_veg[j] / div;
				}
				s->p_oagg[pd]->litter_int = s->p_accu[pd]->litter_int / div;
				break;

			case eSW_AET:
				s->p_oagg[pd]->aet = s->p_accu[pd]->aet / div;
				break;

			case eSW_LyrDrain:
				for (i = 0; i < SW_Site.n_layers - 1; i++)
					s->p_oagg[pd]->lyrdrain[i] = s->p_accu[pd]->lyrdrain[i] / div;
				break;

			case eSW_HydRed:
				ForEachSoilLayer(i)
				{
					s->p_oagg[pd]->hydred_total[i] = s->p_accu[pd]->hydred_total[i] / div;
					ForEachVegType(j) {
						s->p_oagg[pd]->hydred[j][i] = s->p_accu[pd]->hydred[j][i] / div;
					}
				}
				break;

			case eSW_PET:
				s->p_oagg[pd]->pet = s->p_accu[pd]->pet / div;
				break;

			case eSW_WetDays:
				ForEachSoilLayer(i)
					s->p_oagg[pd]->wetdays[i] = s->p_accu[pd]->wetdays[i] / div;
				break;

			case eSW_SnowPack:
				s->p_oagg[pd]->snowpack = s->p_accu[pd]->snowpack / div;
				s->p_oagg[pd]->snowdepth = s->p_accu[pd]->snowdepth / div;
				break;

			case eSW_Estab: /* do nothing, no averaging required */
				break;

			case eSW_CO2Effects:
				ForEachVegType(i) {
					vp->p_oagg[pd]->veg[i].biomass = vp->p_accu[pd]->veg[i].biomass / div;
					vp->p_oagg[pd]->veg[i].biolive = vp->p_accu[pd]->veg[i].biolive / div;
				}
				break;

			default:
				LogError(logfp, LOGFATAL, "PGMR: Invalid key in average_for(%s)", key2str[k]);
			}

		} /* end ForEachKey */
	}
}


static void collect_sums(ObjType otyp, OutPeriod op)
{
	SW_SOILWAT *s = &SW_Soilwat;
	SW_WEATHER *w = &SW_Weather;
	SW_VEGESTAB *v = &SW_VegEstab;
	SW_VEGPROD *vp = &SW_VegProd;

	TimeInt pd = 0;
	OutKey k;
	IntUS i;
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


	// call `sumof_XXX` for each output key x output period combination
	// for those output keys that belong to the output type `otyp` (eSWC, eWTH, eVES, eVPD)
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
				sumof_swc(s, s->p_accu[op], k);
				break;

			case eWTH:
				sumof_wth(w, w->p_accu[op], k);
				break;

			case eVES:
				if (op == eSW_Year) {
					sumof_ves(v, v->p_accu[eSW_Year], k); /* yearly, y'see */
				}
				break;

			case eVPD:
				sumof_vpd(vp, vp->p_accu[op], k);
				break;

			default:
				break;
			}
		}

	} /* end ForEachOutKey */
}



#ifdef STEPWAT
/** @brief Check that STEPWAT2 will receive correct values in `SXW`
		@description These tests must match with STEPWAT2's `struct stepwat_st`.
			Currently implemented checks:
			* monthly summed transpiration
			* annual mean air temperature
			* annual and monthly precipitation sum
			* annual sum of AET
*/
static int check_STEPWAT2_output_requirements(char msg[])
{
	int i,
			res = 0; // return value indicating type of message if any

	msg[0] = '\0';

	// Check that STEPWAT2 receives monthly summed transpiration
	Bool has_monT = swFALSE;

	for (i = 0; i < used_OUTNPERIODS; i++) {
		if (timeSteps[eSW_Transp][i] == eSW_Month) {
			has_monT = swTRUE;
			break;
		}
	}

	if (!(has_monT && SW_Output[eSW_Transp].sumtype == eSW_Sum)) {
		sprintf(msg, "STEPWAT2 requires monthly transpiration sum, " \
			"but this is currently turned off and/or not the sum.");
		return(LOGFATAL);
	}

	// Check that STEPWAT2 receives annual mean air temperature
	Bool has_annualTemp = swFALSE;

	for (i = 0; i < used_OUTNPERIODS; i++) {
		if (timeSteps[eSW_Temp][i] == eSW_Year) {
			has_annualTemp = swTRUE;
			break;
		}
	}

	if (!(has_annualTemp && SW_Output[eSW_Temp].sumtype == eSW_Avg)) {
		sprintf(msg, "STEPWAT2 requires annual mean air temperature, " \
			"but this is currently turned off and/or not the mean.");
		return(LOGFATAL);
	}

	// Check that STEPWAT2 receives annual and monthly precipitation sum
	Bool has_annualPPT = swFALSE, has_monthlyPPT = swFALSE;

	for (i = 0; i < used_OUTNPERIODS; i++) {
		if (timeSteps[eSW_Precip][i] == eSW_Month) {
			has_monthlyPPT = swTRUE;
		}
		if (timeSteps[eSW_Precip][i] == eSW_Year) {
			has_annualPPT = swTRUE;
		}
	}

	if (!(has_monthlyPPT && has_annualPPT && SW_Output[eSW_Precip].sumtype == eSW_Sum)) {
		sprintf(msg, "STEPWAT2 requires monthly and annual " \
			"precipitation sum, but these are currently turned off and/or not the sum.");
		return(LOGFATAL);
	}

	// Check that STEPWAT2 receives annual sum of AET
	Bool has_annualAET = swFALSE;

	for (i = 0; i < used_OUTNPERIODS; i++) {
		if (timeSteps[eSW_AET][i] == eSW_Year) {
			has_annualAET = swTRUE;
			break;
		}
	}

	if (!(has_annualAET && SW_Output[eSW_AET].sumtype == eSW_Sum)) {
		sprintf(msg, "STEPWAT2 requires annual AET sum, " \
			"but this is currently turned off and/or not the sum.");
		return(LOGFATAL);
	}

	return(res);
}
#endif



/* =================================================== */
/* =================================================== */
/*             Public Function Definitions             */
/* --------------------------------------------------- */



/** Tally for which output time periods at least one output key/type is active
*/
void find_OutPeriods_inUse(void)
{
	OutKey k;
	OutPeriod p;
	IntUS i;

	ForEachOutPeriod(p) {
		use_OutPeriod[p] = swFALSE;
	}

	ForEachOutKey(k) {
		for (i = 0; i < used_OUTNPERIODS; i++) {
			use_OutPeriod[timeSteps[k][i]] = swTRUE;
		}
	}
}

/** Determine whether output period `pd` is active for output key `k`
*/
Bool has_OutPeriod_inUse(OutPeriod pd, OutKey k)
{
	int i;
	Bool has_timeStep = swFALSE;

	for (i = 0; i < used_OUTNPERIODS; i++)
	{
		has_timeStep = (Bool) has_timeStep || timeSteps[k][i] == pd;
	}

	return has_timeStep;
}



void SW_OUT_construct(void)
{
	/* =================================================== */
	OutKey k;
	#ifdef SW_OUTARRAY
	OutPeriod p;
	#endif
	LyrIndex i;
	SW_SOILWAT_OUTPUTS *s = NULL;
	int j;

	#if defined(SOILWAT)
	print_SW_Output = swTRUE;
	print_IterationSummary = swFALSE;
	#elif defined(STEPWAT)
	print_SW_Output = (Bool) storeAllIterations;
	// `print_IterationSummary` is set by STEPWAT2's `main` function
	#endif

	#ifdef SW_OUTTEXT
	SW_OutFiles.make_soil = swFALSE;
	SW_OutFiles.make_regular = swFALSE;
	#endif

	bFlush_output = swFALSE;
	tOffset = 1;

	ForEachSoilLayer(i) {
		ForEachVegType(j) {
			s->SWA_VegType[j][i] = 0.;
		}
	}

	#ifdef SW_OUTARRAY
	ForEachOutPeriod(p)
	{
		nrow_OUT[p] = 0;
		irow_OUT[p] = 0;
	}
	#endif

	/* note that an initializer that is called during
	 * execution (better called clean() or something)
	 * will need to free all allocated memory first
	 * before clearing structure.
	 */
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
		switch (k)
		{
		case eSW_Temp:
			#ifdef SW_OUTTEXT
			SW_Output[k].pfunc_text = (void (*)(OutPeriod)) get_temp_text;
			#endif
			#if defined(RSOILWAT)
			SW_Output[k].pfunc_mem = (void (*)(OutPeriod)) get_temp_mem;
			#elif defined(STEPWAT)
			SW_Output[k].pfunc_agg = (void (*)(OutPeriod)) get_temp_agg;
			SW_Output[k].pfunc_SXW = (void (*)(OutPeriod)) get_temp_SXW;
			#endif
			break;

		case eSW_Precip:
			#ifdef SW_OUTTEXT
			SW_Output[k].pfunc_text = (void (*)(OutPeriod)) get_precip_text;
			#endif
			#if defined(RSOILWAT)
			SW_Output[k].pfunc_mem = (void (*)(OutPeriod)) get_precip_mem;
			#elif defined(STEPWAT)
			SW_Output[k].pfunc_agg = (void (*)(OutPeriod)) get_precip_agg;
			SW_Output[k].pfunc_SXW = (void (*)(OutPeriod)) get_precip_SXW;
			#endif
			break;

		case eSW_VWCBulk:
			#ifdef SW_OUTTEXT
			SW_Output[k].pfunc_text = (void (*)(OutPeriod)) get_vwcBulk_text;
			#endif
			#if defined(RSOILWAT)
			SW_Output[k].pfunc_mem = (void (*)(OutPeriod)) get_vwcBulk_mem;
			#elif defined(STEPWAT)
			SW_Output[k].pfunc_agg = (void (*)(OutPeriod)) get_vwcBulk_agg;
			SW_Output[k].pfunc_SXW = (void (*)(OutPeriod)) get_none;
			#endif
			break;

		case eSW_VWCMatric:
			#ifdef SW_OUTTEXT
			SW_Output[k].pfunc_text = (void (*)(OutPeriod)) get_vwcMatric_text;
			#endif
			#if defined(RSOILWAT)
			SW_Output[k].pfunc_mem = (void (*)(OutPeriod)) get_vwcMatric_mem;
			#elif defined(STEPWAT)
			SW_Output[k].pfunc_agg = (void (*)(OutPeriod)) get_vwcMatric_agg;
			SW_Output[k].pfunc_SXW = (void (*)(OutPeriod)) get_none;
			#endif
			break;

		case eSW_SWCBulk:
			#ifdef SW_OUTTEXT
			SW_Output[k].pfunc_text = (void (*)(OutPeriod)) get_swcBulk_text;
			#endif
			#if defined(RSOILWAT)
			SW_Output[k].pfunc_mem = (void (*)(OutPeriod)) get_swcBulk_mem;
			#elif defined(STEPWAT)
			SW_Output[k].pfunc_agg = (void (*)(OutPeriod)) get_swcBulk_agg;
			SW_Output[k].pfunc_SXW = (void (*)(OutPeriod)) get_swcBulk_SXW;
			#endif
			break;

		case eSW_SWPMatric:
			#ifdef SW_OUTTEXT
			SW_Output[k].pfunc_text = (void (*)(OutPeriod)) get_swpMatric_text;
			#endif
			#if defined(RSOILWAT)
			SW_Output[k].pfunc_mem = (void (*)(OutPeriod)) get_swpMatric_mem;
			#elif defined(STEPWAT)
			SW_Output[k].pfunc_agg = (void (*)(OutPeriod)) get_swpMatric_agg;
			SW_Output[k].pfunc_SXW = (void (*)(OutPeriod)) get_none;
			#endif
			break;

		case eSW_SWABulk:
			#ifdef SW_OUTTEXT
			SW_Output[k].pfunc_text = (void (*)(OutPeriod)) get_swaBulk_text;
			#endif
			#if defined(RSOILWAT)
			SW_Output[k].pfunc_mem = (void (*)(OutPeriod)) get_swaBulk_mem;
			#elif defined(STEPWAT)
			SW_Output[k].pfunc_agg = (void (*)(OutPeriod)) get_swaBulk_agg;
			SW_Output[k].pfunc_SXW = (void (*)(OutPeriod)) get_none;
			#endif
			break;

		case eSW_SWAMatric:
			#ifdef SW_OUTTEXT
			SW_Output[k].pfunc_text = (void (*)(OutPeriod)) get_swaMatric_text;
			#endif
			#if defined(RSOILWAT)
			SW_Output[k].pfunc_mem = (void (*)(OutPeriod)) get_swaMatric_mem;
			#elif defined(STEPWAT)
			SW_Output[k].pfunc_agg = (void (*)(OutPeriod)) get_swaMatric_agg;
			SW_Output[k].pfunc_SXW = (void (*)(OutPeriod)) get_none;
			#endif
			break;

		case eSW_SWA:
			#ifdef SW_OUTTEXT
			SW_Output[k].pfunc_text = (void (*)(OutPeriod)) get_swa_text;
			#endif
			#if defined(RSOILWAT)
			SW_Output[k].pfunc_mem = (void (*)(OutPeriod)) get_swa_mem;
			#elif defined(STEPWAT)
			SW_Output[k].pfunc_agg = (void (*)(OutPeriod)) get_swa_agg;
			SW_Output[k].pfunc_SXW = (void (*)(OutPeriod)) get_none;
			#endif
			break;

		case eSW_SurfaceWater:
			#ifdef SW_OUTTEXT
			SW_Output[k].pfunc_text = (void (*)(OutPeriod)) get_surfaceWater_text;
			#endif
			#if defined(RSOILWAT)
			SW_Output[k].pfunc_mem = (void (*)(OutPeriod)) get_surfaceWater_mem;
			#elif defined(STEPWAT)
			SW_Output[k].pfunc_agg = (void (*)(OutPeriod)) get_surfaceWater_agg;
			SW_Output[k].pfunc_SXW = (void (*)(OutPeriod)) get_none;
			#endif
			break;

		case eSW_Runoff:
			#ifdef SW_OUTTEXT
			SW_Output[k].pfunc_text = (void (*)(OutPeriod)) get_runoffrunon_text;
			#endif
			#if defined(RSOILWAT)
			SW_Output[k].pfunc_mem = (void (*)(OutPeriod)) get_runoffrunon_mem;
			#elif defined(STEPWAT)
			SW_Output[k].pfunc_agg = (void (*)(OutPeriod)) get_runoffrunon_agg;
			SW_Output[k].pfunc_SXW = (void (*)(OutPeriod)) get_none;
			#endif
			break;

		case eSW_Transp:
			#ifdef SW_OUTTEXT
			SW_Output[k].pfunc_text = (void (*)(OutPeriod)) get_transp_text;
			#endif
			#if defined(RSOILWAT)
			SW_Output[k].pfunc_mem = (void (*)(OutPeriod)) get_transp_mem;
			#elif defined(STEPWAT)
			SW_Output[k].pfunc_agg = (void (*)(OutPeriod)) get_transp_agg;
			SW_Output[k].pfunc_SXW = (void (*)(OutPeriod)) get_transp_SXW;
			#endif
			break;

		case eSW_EvapSoil:
			#ifdef SW_OUTTEXT
			SW_Output[k].pfunc_text = (void (*)(OutPeriod)) get_evapSoil_text;
			#endif
			#if defined(RSOILWAT)
			SW_Output[k].pfunc_mem = (void (*)(OutPeriod)) get_evapSoil_mem;
			#elif defined(STEPWAT)
			SW_Output[k].pfunc_agg = (void (*)(OutPeriod)) get_evapSoil_agg;
			SW_Output[k].pfunc_SXW = (void (*)(OutPeriod)) get_none;
			#endif
			break;

		case eSW_EvapSurface:
			#ifdef SW_OUTTEXT
			SW_Output[k].pfunc_text = (void (*)(OutPeriod)) get_evapSurface_text;
			#endif
			#if defined(RSOILWAT)
			SW_Output[k].pfunc_mem = (void (*)(OutPeriod)) get_evapSurface_mem;
			#elif defined(STEPWAT)
			SW_Output[k].pfunc_agg = (void (*)(OutPeriod)) get_evapSurface_agg;
			SW_Output[k].pfunc_SXW = (void (*)(OutPeriod)) get_none;
			#endif
			break;

		case eSW_Interception:
			#ifdef SW_OUTTEXT
			SW_Output[k].pfunc_text = (void (*)(OutPeriod)) get_interception_text;
			#endif
			#if defined(RSOILWAT)
			SW_Output[k].pfunc_mem = (void (*)(OutPeriod)) get_interception_mem;
			#elif defined(STEPWAT)
			SW_Output[k].pfunc_agg = (void (*)(OutPeriod)) get_interception_agg;
			SW_Output[k].pfunc_SXW = (void (*)(OutPeriod)) get_none;
			#endif
			break;

		case eSW_SoilInf:
			#ifdef SW_OUTTEXT
			SW_Output[k].pfunc_text = (void (*)(OutPeriod)) get_soilinf_text;
			#endif
			#if defined(RSOILWAT)
			SW_Output[k].pfunc_mem = (void (*)(OutPeriod)) get_soilinf_mem;
			#elif defined(STEPWAT)
			SW_Output[k].pfunc_agg = (void (*)(OutPeriod)) get_soilinf_agg;
			SW_Output[k].pfunc_SXW = (void (*)(OutPeriod)) get_none;
			#endif
			break;

		case eSW_LyrDrain:
			#ifdef SW_OUTTEXT
			SW_Output[k].pfunc_text = (void (*)(OutPeriod)) get_lyrdrain_text;
			#endif
			#if defined(RSOILWAT)
			SW_Output[k].pfunc_mem = (void (*)(OutPeriod)) get_lyrdrain_mem;
			#elif defined(STEPWAT)
			SW_Output[k].pfunc_agg = (void (*)(OutPeriod)) get_lyrdrain_agg;
			SW_Output[k].pfunc_SXW = (void (*)(OutPeriod)) get_none;
			#endif
			break;

		case eSW_HydRed:
			#ifdef SW_OUTTEXT
			SW_Output[k].pfunc_text = (void (*)(OutPeriod)) get_hydred_text;
			#endif
			#if defined(RSOILWAT)
			SW_Output[k].pfunc_mem = (void (*)(OutPeriod)) get_hydred_mem;
			#elif defined(STEPWAT)
			SW_Output[k].pfunc_agg = (void (*)(OutPeriod)) get_hydred_agg;
			SW_Output[k].pfunc_SXW = (void (*)(OutPeriod)) get_none;
			#endif
			break;

		case eSW_AET:
			#ifdef SW_OUTTEXT
			SW_Output[k].pfunc_text = (void (*)(OutPeriod)) get_aet_text;
			#endif
			#if defined(RSOILWAT)
			SW_Output[k].pfunc_mem = (void (*)(OutPeriod)) get_aet_mem;
			#elif defined(STEPWAT)
			SW_Output[k].pfunc_agg = (void (*)(OutPeriod)) get_aet_agg;
			SW_Output[k].pfunc_SXW = (void (*)(OutPeriod)) get_aet_SXW;
			#endif
			break;

		case eSW_PET:
			#ifdef SW_OUTTEXT
			SW_Output[k].pfunc_text = (void (*)(OutPeriod)) get_pet_text;
			#endif
			#if defined(RSOILWAT)
			SW_Output[k].pfunc_mem = (void (*)(OutPeriod)) get_pet_mem;
			#elif defined(STEPWAT)
			SW_Output[k].pfunc_agg = (void (*)(OutPeriod)) get_pet_agg;
			SW_Output[k].pfunc_SXW = (void (*)(OutPeriod)) get_none;
			#endif
			break;

		case eSW_WetDays:
			#ifdef SW_OUTTEXT
			SW_Output[k].pfunc_text = (void (*)(OutPeriod)) get_wetdays_text;
			#endif
			#if defined(RSOILWAT)
			SW_Output[k].pfunc_mem = (void (*)(OutPeriod)) get_wetdays_mem;
			#elif defined(STEPWAT)
			SW_Output[k].pfunc_agg = (void (*)(OutPeriod)) get_wetdays_agg;
			SW_Output[k].pfunc_SXW = (void (*)(OutPeriod)) get_none;
			#endif
			break;

		case eSW_SnowPack:
			#ifdef SW_OUTTEXT
			SW_Output[k].pfunc_text = (void (*)(OutPeriod)) get_snowpack_text;
			#endif
			#if defined(RSOILWAT)
			SW_Output[k].pfunc_mem = (void (*)(OutPeriod)) get_snowpack_mem;
			#elif defined(STEPWAT)
			SW_Output[k].pfunc_agg = (void (*)(OutPeriod)) get_snowpack_agg;
			SW_Output[k].pfunc_SXW = (void (*)(OutPeriod)) get_none;
			#endif
			break;

		case eSW_DeepSWC:
			#ifdef SW_OUTTEXT
			SW_Output[k].pfunc_text = (void (*)(OutPeriod)) get_deepswc_text;
			#endif
			#if defined(RSOILWAT)
			SW_Output[k].pfunc_mem = (void (*)(OutPeriod)) get_deepswc_mem;
			#elif defined(STEPWAT)
			SW_Output[k].pfunc_agg = (void (*)(OutPeriod)) get_deepswc_agg;
			SW_Output[k].pfunc_SXW = (void (*)(OutPeriod)) get_none;
			#endif
			break;

		case eSW_SoilTemp:
			#ifdef SW_OUTTEXT
			SW_Output[k].pfunc_text = (void (*)(OutPeriod)) get_soiltemp_text;
			#endif
			#if defined(RSOILWAT)
			SW_Output[k].pfunc_mem = (void (*)(OutPeriod)) get_soiltemp_mem;
			#elif defined(STEPWAT)
			SW_Output[k].pfunc_agg = (void (*)(OutPeriod)) get_soiltemp_agg;
			SW_Output[k].pfunc_SXW = (void (*)(OutPeriod)) get_none;
			#endif
			break;

		case eSW_Estab:
			#ifdef SW_OUTTEXT
			SW_Output[k].pfunc_text = (void (*)(OutPeriod)) get_estab_text;
			#endif
			#if defined(RSOILWAT)
			SW_Output[k].pfunc_mem = (void (*)(OutPeriod)) get_estab_mem;
			#elif defined(STEPWAT)
			SW_Output[k].pfunc_agg = (void (*)(OutPeriod)) get_estab_agg;
			SW_Output[k].pfunc_SXW = (void (*)(OutPeriod)) get_none;
			#endif
			break;

		case eSW_CO2Effects:
			#ifdef SW_OUTTEXT
			SW_Output[k].pfunc_text = (void (*)(OutPeriod)) get_co2effects_text;
			#endif
			#if defined(RSOILWAT)
			SW_Output[k].pfunc_mem = (void (*)(OutPeriod)) get_co2effects_mem;
			#elif defined(STEPWAT)
			SW_Output[k].pfunc_agg = (void (*)(OutPeriod)) get_co2effects_agg;
			SW_Output[k].pfunc_SXW = (void (*)(OutPeriod)) get_none;
			#endif
			break;

		default:
			#ifdef SW_OUTTEXT
			SW_Output[k].pfunc_text = (void (*)(OutPeriod)) get_none;
			#endif
			#if defined(RSOILWAT)
			SW_Output[k].pfunc_mem = (void (*)(OutPeriod)) get_none;
			#elif defined(STEPWAT)
			SW_Output[k].pfunc_agg = (void (*)(OutPeriod)) get_none;
			SW_Output[k].pfunc_SXW = (void (*)(OutPeriod)) get_none;
			#endif
			break;
		}
	} // end of loop across output keys

}


void SW_OUT_deconstruct(Bool full_reset)
{
	OutKey k;
	IntU i;

	ForEachOutKey(k)
	{
		for (i = 0; i < 5 * NVEGTYPES + MAX_LAYERS; i++)
		{
			if (!isnull(colnames_OUT[k][i]))
			{
				Mem_Free(colnames_OUT[k][i]);
				colnames_OUT[k][i] = NULL;
			}
		}
	}

	#ifdef SW_OUTARRAY
	if (full_reset) {
		SW_OUT_deconstruct_outarray();
	}
	#else
	if (full_reset) {} // avoid ``-Wunused-parameter` warning
	#endif
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

/** @brief Set column/variable names in global array `colnames_OUT`
		@description
		Order of outputs must match up with all `get_XXX` functions and with
		indexing macros `iOUT` and `iOUT2`; particularly, output variables with
		values for each of `N` soil layers for `k` different (e.g., vegetation)
		components (e.g., transpiration, SWA, and hydraulic redistribution) report
		based on a loop over components within
		which a loop over soil layers is nested, e.g.,
		`C1_Lyr1, C1_Lyr2, ..., C1_LyrN, C2_Lyr1, ..., C2_LyrN, ...,
		Ck_Lyr1, ..., Ck_LyrN`
*/
void SW_OUT_set_colnames(void) {
	IntUS i, j;
	LyrIndex tLayers = SW_Site.n_layers;
  #ifdef SWDEBUG
  int debug = 0;
  #endif

	char ctemp[50];
	const char *Layers_names[MAX_LAYERS] = { "Lyr_1", "Lyr_2", "Lyr_3", "Lyr_4",
		"Lyr_5", "Lyr_6", "Lyr_7", "Lyr_8", "Lyr_9", "Lyr_10", "Lyr_11", "Lyr_12",
		"Lyr_13", "Lyr_14", "Lyr_15", "Lyr_16", "Lyr_17", "Lyr_18", "Lyr_19",
		"Lyr_20", "Lyr_21", "Lyr_22", "Lyr_23", "Lyr_24", "Lyr_25"};
	const char *cnames_VegTypes[NVEGTYPES + 2] = { "total", "tree", "shrub",
		"forbs", "grass", "litter" };

	const char *cnames_eSW_Temp[] = { "max_C", "min_C", "avg_C",
		"surfaceTemp_C" };
	const char *cnames_eSW_Precip[] = { "ppt", "rain", "snow_fall", "snowmelt",
		"snowloss" };
	const char *cnames_eSW_SoilInf[] = { "soil_inf" };
	const char *cnames_eSW_Runoff[] = { "net", "ponded_runoff", "snowmelt_runoff",
		"ponded_runon" };
	const char *cnames_eSW_SurfaceWater[] = { "surfaceWater_cm" };
	const char *cnames_add_eSW_EvapSurface[] = { "evap_surfaceWater" };
	const char *cnames_eSW_AET[] = { "evapotr_cm" };
	const char *cnames_eSW_PET[] = { "pet_cm" };
	const char *cnames_eSW_SnowPack[] = { "snowpackWaterEquivalent_cm",
		"snowdepth_cm" };
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


void SW_OUT_new_year(void)
{
	/* =================================================== */
	/* reset the terminal output days each year  */

	OutKey k;

	ForEachOutKey(k)
	{
		if (!SW_Output[k].use) {
			continue;
		}

		if (SW_Output[k].first_orig <= SW_Model.firstdoy) {
			SW_Output[k].first = SW_Model.firstdoy;
		} else {
			SW_Output[k].first = SW_Output[k].first_orig;
		}

		if (SW_Output[k].last_orig >= SW_Model.lastdoy) {
			SW_Output[k].last = SW_Model.lastdoy;
		} else {
			SW_Output[k].last = SW_Output[k].last_orig;
		}
	}

}



int SW_OUT_read_onekey(OutKey k, OutSum sumtype, char period[], int first,
	int last, char msg[])
{
	int res = 0; // return value indicating type of message if any

	MyFileName = SW_F_name(eOutput);
	msg[0] = '\0';

	// Convert strings to index numbers
	SW_Output[k].sumtype = sumtype;

	SW_Output[k].use = (Bool) (sumtype != eSW_Off);

	// Proceed to next line if output key/type is turned off
	if (!SW_Output[k].use)
	{
		return(-1); // return and read next line of `outsetup.in`
	}

	// Check whether output key/type generates output for each soil layers or not
	SW_Output[k].has_sl = has_key_soillayers(k);

	/* check validity of summary type */
	if (SW_Output[k].sumtype == eSW_Fnl && !SW_Output[k].has_sl)
	{
		SW_Output[k].sumtype = eSW_Avg;

		sprintf(msg, "%s : Summary Type FIN with key %s is meaningless.\n" \
			"  Using type AVG instead.", MyFileName, key2str[k]);
		res = LOGWARN;
	}

	// Check whether output per soil layer or 'regular' is requested
	#ifdef SW_OUTTEXT
	if (SW_Output[k].has_sl)
	{
		SW_OutFiles.make_soil = swTRUE;
	} else
	{
		SW_OutFiles.make_regular = swTRUE;
	}
	#endif

	// set use_SWA to TRUE if defined.
	// Used in SW_Control to run the functions to get the recalculated values only if SWA is used
	// This function is run prior to the control functions so thats why it is here.
	if (k == eSW_SWA) {
		SW_VegProd.use_SWA = swTRUE;
	}

	/* Check validity of output key */
	if (k == eSW_Estab) {
		SW_Output[k].sumtype = eSW_Sum;
		first = 1;
		last = 366;
		#ifndef RSOILWAT
		strcpy(period, "YR");
		#endif

	} else if ((k == eSW_AllVeg || k == eSW_ET || k == eSW_AllWthr || k == eSW_AllH2O))
	{
		SW_Output[k].use = swFALSE;

		sprintf(msg, "%s : Output key %s is currently unimplemented.",
			MyFileName, key2str[k]);
		return(LOGNOTE);
	}

	/* verify deep drainage parameters */
	if (k == eSW_DeepSWC && SW_Output[k].sumtype != eSW_Off && !SW_Site.deepdrain)
	{
		SW_Output[k].use = swFALSE;

		sprintf(msg, "%s : DEEPSWC cannot produce output if deep drainage is " \
			"not simulated (flag not set in %s).",
			MyFileName, SW_F_name(eSite));
		return(LOGWARN);
	}

	// Set remaining values of `SW_Output[k]`
	SW_Output[k].mykey = k;
	SW_Output[k].myobj = key2obj[k];
	SW_Output[k].first_orig = first;
	SW_Output[k].last_orig = last;

	if (SW_Output[k].last_orig == 0)
	{
		sprintf(msg, "%s : Invalid ending day (%d), key=%s.",
			MyFileName, last, key2str[k]);
		return(LOGFATAL);
	}

	return(res);
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
	int x, itemno, msg_type;
	IntUS i;

	/* these dims come from the orig format str */
	/* except for the uppercase space. */
	char timeStep[SW_OUTNPERIODS][10], // matrix to capture all the periods entered in outsetup.in
			keyname[50],
			ext[10],
			sumtype[4], /* should be 2 chars, but we don't want overflow from user typos */
			period[10],
			last[4], /* last doy for output, if "end", ==366 */
			outfile[MAX_FILENAMESIZE],
			msg[200], // message to print
			upkey[50], upsum[4]; /* space for uppercase conversion */
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

		// Check whether we have read in `TIMESTEP`, `OUTSEP`, or one of the 'key' lines
		if (Str_CompareI(keyname, (char *)"TIMESTEP") == 0)
		{
			// condition to read in the TIMESTEP line in outsetup.in
			// need to rescan the line because you are looking for all strings, unlike the original scan
			used_OUTNPERIODS = sscanf(inbuf, "%s %s %s %s %s", keyname, timeStep[0],
					timeStep[1], timeStep[2], timeStep[3]);	// maximum number of possible timeStep is SW_OUTNPERIODS
			used_OUTNPERIODS--; // decrement the count to make sure to not count keyname in the number of periods

			useTimeStep = 1;
			continue; // read next line of `outsetup.in`
		}

		if (Str_CompareI(keyname, (char *)"OUTSEP") == 0)
		{
			// condition to read in the OUTSEP line in outsetup.in
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

			continue; //read next line of `outsetup.in`
		}

		// we have read a line that specifies an output key/type
		// make sure that we got enough input
		if (x < 6)
		{
			CloseFile(&f);
			LogError(logfp, LOGFATAL, "%s : Insufficient input for key %s item %d.",
				MyFileName, keyname, itemno);

			continue; //read next line of `outsetup.in`
		}

		// Convert strings to index numbers
		k = str2key(Str_ToUpper(keyname, upkey));

		// Fill information into `SW_Output[k]`
		msg_type = SW_OUT_read_onekey(k,
			str2stype(Str_ToUpper(sumtype, upsum)),
			period, first, !Str_CompareI("END", (char *)last) ? 366 : atoi(last),
			msg);

		if (msg_type != 0) {
			if (msg_type > 0) {
				if (msg_type == LOGFATAL) {
					CloseFile(&f);
				}
				LogError(logfp, msg_type, "%s", msg);
			}

			continue;
		}

		// Specify which output time periods are requested for this output key/type
		if (useTimeStep) {
			// `timeStep` was read in earlier on the `TIMESTEP` line; ignore `period`
			for (i = 0; i < used_OUTNPERIODS; i++) {
				timeSteps[k][i] = str2period(Str_ToUpper(timeStep[i], ext));
			}

		} else {
			timeSteps[k][0] = str2period(Str_ToUpper(period, ext));
		}
	} //end of while-loop


	// Determine which output periods are turned on for at least one output key
	find_OutPeriods_inUse();

	#ifdef STEPWAT
	// Determine number of used years/months/weeks/days in simulation period
	SW_OUT_set_nrow();

	// Check that STEPWAT2 will receive correct values in `SXW`
	msg_type = check_STEPWAT2_output_requirements(msg);

	if (msg_type != 0) {
		if (msg_type == LOGFATAL) {
			CloseFile(&f);
		}

		LogError(logfp, msg_type, "%s", msg);
	}
	#endif

	CloseFile(&f);

	if (EchoInits)
		_echo_outputs();
}



void _collect_values(void) {
	SW_OUT_sum_today(eSWC);
	SW_OUT_sum_today(eWTH);
	SW_OUT_sum_today(eVES);
	SW_OUT_sum_today(eVPD);

	SW_OUT_write_today();
}


/** called at year end to process the remainder of the output
    period.  This sets two module-level flags: bFlush_output and
    tOffset to be used in the appropriate subs.*/
void SW_OUT_flush(void) {
	bFlush_output = swTRUE;
	tOffset = 0;

	_collect_values();

	bFlush_output = swFALSE;
	tOffset = 1;
}

/** adds today's output values to week, month and year
    accumulators and puts today's values in yesterday's
    registers. This is different from the Weather.c approach
    which updates Yesterday's registers during the _new_day()
    function. It's more logical to update yesterday just
    prior to today's calculations, but there's no logical
    need to perform _new_day() on the soilwater.
*/
void SW_OUT_sum_today(ObjType otyp)
{
	SW_SOILWAT *s = &SW_Soilwat;
	SW_WEATHER *w = &SW_Weather;
	SW_VEGPROD *vp = &SW_VegProd;
	/*  SW_VEGESTAB *v = &SW_VegEstab;  -> we don't need to sum daily for this */

	OutPeriod pd;

	ForEachOutPeriod(pd)
	{
		if (bFlush_output || SW_Model.newperiod[pd]) // `newperiod[eSW_Day]` is always TRUE
		{
			average_for(otyp, pd);

			switch (otyp)
			{
				case eSWC:
					memset(s->p_accu[pd], 0, sizeof(SW_SOILWAT_OUTPUTS));
					break;
				case eWTH:
					memset(w->p_accu[pd], 0, sizeof(SW_WEATHER_OUTPUTS));
					break;
				case eVES:
					break;
				case eVPD:
					memset(vp->p_accu[pd], 0, sizeof(SW_VEGPROD_OUTPUTS));
					break;
				default:
					LogError(logfp, LOGFATAL,
							"Invalid object type in SW_OUT_sum_today().");
			}
		}
	}

	if (!bFlush_output)
	{
		ForEachOutPeriod(pd)
		{
			collect_sums(otyp, pd);
		}
	}
}

/** `SW_OUT_write_today` is called twice: by
      - `_end_day` at the end of each day with values
        values of `bFlush_output` set to FALSE and `tOffset` set to 1
      - `SW_OUT_flush` at the end of every year with
        values of `bFlush_output` set to TRUE and `tOffset` set to 0
*/
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
	OutPeriod p;
	Bool writeit[SW_OUTNPERIODS];
	IntUS i;

	#ifdef SWDEBUG
  int debug = 0;
  #endif

	#ifdef SW_OUTTEXT
	char str_time[10]; // year and day/week/month header for each output row

	// We don't really need all of these buffers to init every day
	ForEachOutPeriod(p)
	{
		SW_OutFiles.buf_reg[p][0] = '\0';
		SW_OutFiles.buf_soil[p][0] = '\0';

		#ifdef STEPWAT
		SW_OutFiles.buf_reg_agg[p][0] = '\0';
		SW_OutFiles.buf_soil_agg[p][0] = '\0';
		#endif
	}
	#endif

  #ifdef SWDEBUG
  if (debug) swprintf("'SW_OUT_write_today': %dyr-%dmon-%dwk-%ddoy: ",
    SW_Model.year, SW_Model.month, SW_Model.week, SW_Model.doy);
  #endif


	// Determine which output periods should get formatted and output (if they are active)
	t = SW_Model.doy;
	writeit[eSW_Day] = (Bool) (t < SW_Output[0].first || t > SW_Output[0].last); // `csv`-files assume anyhow that first/last are identical for every output type/key
	writeit[eSW_Week] = (Bool) (writeit[eSW_Day] && (SW_Model.newperiod[eSW_Week] || bFlush_output));
	writeit[eSW_Month] = (Bool) (writeit[eSW_Day] && (SW_Model.newperiod[eSW_Month] || bFlush_output));
	writeit[eSW_Year] = (Bool) (SW_Model.newperiod[eSW_Year] || bFlush_output);
	// update daily: don't process daily output if `bFlush_output` is TRUE
	// because `_end_day` was already called and produced daily output
	writeit[eSW_Day] = (Bool) (writeit[eSW_Day] && !bFlush_output);


	// Loop over output types/keys, over used output time periods, call
	// formatting functions `get_XXX`, and concatenate for one row of `csv`-output
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

			#ifdef SW_OUTTEXT
			if (timeSteps[k][i] < eSW_Day || timeSteps[k][i] > eSW_Year) {
				// RSOILWAT sets off variables to SW_missing so this is not invalid for rSOILWAT2
				LogError(logfp, LOGWARN,
					"'SW_OUT_write_today': Invalid period = %d for key = %s",
					timeSteps[k][i], key2str[k]);
				continue;
			}
			#endif

			if (!writeit[timeSteps[k][i]]) {
				continue;
			}

			#ifdef SWDEBUG
			if (debug) swprintf(" call pfunc(s)");
			#endif

			#ifdef SOILWAT
			((void (*)(OutPeriod)) SW_Output[k].pfunc_text)(timeSteps[k][i]);

			#elif RSOILWAT
			((void (*)(OutPeriod)) SW_Output[k].pfunc_mem)(timeSteps[k][i]);

			#elif defined(STEPWAT)
			((void (*)(OutPeriod)) SW_Output[k].pfunc_SXW)(timeSteps[k][i]);

			if (prepare_IterationSummary)
			{
				((void (*)(OutPeriod)) SW_Output[k].pfunc_agg)(timeSteps[k][i]);
			}

			if (print_SW_Output)
			{
				((void (*)(OutPeriod)) SW_Output[k].pfunc_text)(timeSteps[k][i]);
			}
			#endif

			#ifdef SWDEBUG
			if (debug) swprintf(" ... ok");
			#endif

			#ifdef SW_OUTTEXT
			/* concatenate formatted output for one row of `csv`- files */
			if (print_SW_Output)
			{
				if (SW_Output[k].has_sl) {
					strcat(SW_OutFiles.buf_soil[timeSteps[k][i]], sw_outstr);
				} else {
					strcat(SW_OutFiles.buf_reg[timeSteps[k][i]], sw_outstr);
				}
			}

			#ifdef STEPWAT
			if (print_IterationSummary)
			{
				if (SW_Output[k].has_sl) {
					strcat(SW_OutFiles.buf_soil_agg[timeSteps[k][i]], sw_outstr_agg);
				} else {
					strcat(SW_OutFiles.buf_reg_agg[timeSteps[k][i]], sw_outstr_agg);
				}
			}
			#endif
			#endif
		} // end of loop across `used_OUTNPERIODS`
	} // end of loop across output keys


	#ifdef SW_OUTTEXT
	// write formatted output to csv-files
	ForEachOutPeriod(p)
	{
		if (use_OutPeriod[p] && writeit[p])
		{
			get_outstrleader(p, str_time);

			if (SW_OutFiles.make_regular)
			{
				if (print_SW_Output) {
					fprintf(SW_OutFiles.fp_reg[p], "%s%s\n",
						str_time, SW_OutFiles.buf_reg[p]);
					// TODO: STEPWAT2 needs a fflush for yearly output;
					// other time steps, the soil-layer files, and SOILWAT2 work fine without it...
					fflush(SW_OutFiles.fp_reg[p]);
				}

				#ifdef STEPWAT
				if (print_IterationSummary) {
					fprintf(SW_OutFiles.fp_reg_agg[p], "%s%s\n",
						str_time, SW_OutFiles.buf_reg_agg[p]);
				}
				#endif
			}

			if (SW_OutFiles.make_soil)
			{
				if (print_SW_Output) {
					fprintf(SW_OutFiles.fp_soil[p], "%s%s\n",
						str_time, SW_OutFiles.buf_soil[p]);
				}

				#ifdef STEPWAT
				if (print_IterationSummary) {
					fprintf(SW_OutFiles.fp_soil_agg[p], "%s%s\n",
						str_time, SW_OutFiles.buf_soil_agg[p]);
				}
				#endif

			}
		}
	}
	#endif

	#ifdef SW_OUTARRAY
	// increment row counts
	ForEachOutPeriod(p)
	{
		if (use_OutPeriod[p] && writeit[p])
		{
			irow_OUT[p]++;
		}
	}
	#endif

  #ifdef SWDEBUG
  if (debug) swprintf("'SW_OUT_write_today': completed\n");
  #endif
}


void _echo_outputs(void)
{
	OutKey k;
	char str[OUTSTRLEN];

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
		sprintf(str, "\n\tStart period: %d", SW_Output[k].first_orig);
		strcat(errstr, str);
		sprintf(str, "\n\tEnd period  : %d", SW_Output[k].last_orig);
		strcat(errstr, str);
		strcat(errstr, "\n");
	}

	strcat(errstr, "\n----------  End of Output Configuration ---------- \n");
	LogError(logfp, LOGNOTE, errstr);

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
		//NoteMemoryRef(SW_Output[k].outfile);
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
		looks like below code `if (has_key_soillayers(key)) {`
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
