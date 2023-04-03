/********************************************************/
/********************************************************/
/*	Source file: Veg_Estab.c
 Type: module
 Application: SOILWAT - soilwater dynamics simulator
 Purpose: Reads/writes vegetation establishment info.
 History:
 (8/28/01) -- INITIAL CODING - cwb

 8-Sep-03 -- Establishment code works as follows.
 More than one species can be tested per year.
 No more than one establishment per species per year may occur.
 If germination occurs, check environmental conditions
 for establishment.  If a dry period (>max_drydays_postgerm)
 occurs, or temp out of range, kill the plant and
 start over from pregermination state.  Thus, if the
 early estab fails, start over and try again if
 enough time is available.  This is simple but not
 realistic.  Better would be to count and report the
 number of days that would allow establishment which
 would give an index to the number of seedlings
 established in a year.
 20090826 (drs) added return; after LBL_Normal_Exit:
 06/26/2013	(rjm)	closed open files in function SW_VES_read() or if LogError() with LOGFATAL is called in _read_spp()
 08/21/2013	(clk)	changed the line v = SW_VegEstab.parms[ _new_species() ]; -> v = SW_VegEstab.parms[ count ], where count = _new_species();
 for some reason, without this change, a segmenation fault was occuring
 */
/********************************************************/
/********************************************************/

/* =================================================== */
/* =================================================== */
/*                INCLUDES / DEFINES                   */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "include/generic.h" // externs `QuietMode`, `EchoInits`
#include "include/filefuncs.h" // externs `_firstfile`, `inbuf`
#include "include/myMemory.h"
#include "include/SW_Times.h"
#include "include/SW_Files.h"
#include "include/SW_Site.h" // externs SW_Site
#include "include/SW_Model.h" // externs SW_Model
#include "include/SW_SoilWater.h" // externs SW_Soilwat
#include "include/SW_VegProd.h" // externs `key2veg[]`
#include "include/SW_VegEstab.h"



/* =================================================== */
/*                  Global Variables                   */
/* --------------------------------------------------- */
SW_VEGESTAB SW_VegEstab;


/* =================================================== */
/*                  Local Variables                    */
/* --------------------------------------------------- */
static char *MyFileName;



/* =================================================== */
/*             Private Function Declarations           */
/* --------------------------------------------------- */
static void _sanity_check(unsigned int sppnum);
static void _read_spp(const char *infile);
static void _checkit(TimeInt doy, unsigned int sppnum,
					 SW_WEATHER* SW_Weather, SW_SOILWAT* SW_SoilWat);
static void _zero_state(unsigned int sppnum);



/* =================================================== */
/*             Global Function Definitions             */
/* --------------------------------------------------- */

/**
@brief Constructor for SW_VegEstab.
*/
void SW_VES_construct(void) {
	/* =================================================== */
	/* note that an initializer that is called during
	 * execution (better called clean() or something)
	 * will need to free all allocated memory first
	 * before clearing structure.
	 */
	OutPeriod pd;

	// Clear the module structure:
	memset(&SW_VegEstab, 0, sizeof(SW_VegEstab));

	// Allocate output structures:
	ForEachOutPeriod(pd)
	{
		SW_VegEstab.p_accu[pd] = (SW_VEGESTAB_OUTPUTS *) Mem_Calloc(1,
			sizeof(SW_VEGESTAB_OUTPUTS), "SW_VES_construct()");
		if (pd > eSW_Day) {
			SW_VegEstab.p_oagg[pd] = (SW_VEGESTAB_OUTPUTS *) Mem_Calloc(1,
				sizeof(SW_VEGESTAB_OUTPUTS), "SW_VES_construct()");
		}
	}
}

/**
@brief Deconstructor for SW_VegEstab for each period, pd.
*/
void SW_VES_deconstruct(void)
{
	OutPeriod pd;
	IntU i;

	// De-allocate parameters
	if (SW_VegEstab.count > 0)
	{
		for (i = 0; i < SW_VegEstab.count; i++)
		{
			Mem_Free(SW_VegEstab.parms[i]);
			SW_VegEstab.parms[i] = NULL;
		}

		Mem_Free(SW_VegEstab.parms);
		SW_VegEstab.parms = NULL;
	}


	ForEachOutPeriod(pd)
	{
		// De-allocate days and parameters
		if (SW_VegEstab.count > 0)
		{
			if (pd > eSW_Day && !isnull(SW_VegEstab.p_oagg[pd]->days)) {
				Mem_Free(SW_VegEstab.p_oagg[eSW_Year]->days);
			}

			if (!isnull(SW_VegEstab.p_accu[pd]->days)) {
				Mem_Free(SW_VegEstab.p_accu[eSW_Year]->days);
			}
		}

		// De-allocate output structures
		if (pd > eSW_Day && !isnull(SW_VegEstab.p_oagg[pd])) {
			Mem_Free(SW_VegEstab.p_oagg[pd]);
			SW_VegEstab.p_oagg[pd] = NULL;
		}

		if (!isnull(SW_VegEstab.p_accu[pd])) {
			Mem_Free(SW_VegEstab.p_accu[pd]);
			SW_VegEstab.p_accu[pd] = NULL;
		}
	}
}

/**
@brief We can use the debug memset because we allocated days, that is, it
			wasn't allocated by the compiler.
*/
void SW_VES_new_year(void) {

	if (0 == SW_VegEstab.count)
		return;
}

/**
@brief Reads in file for SW_VegEstab and species establishment parameters
*/
void SW_VES_read(void) {
	SW_VES_read2(swTRUE, swTRUE);
}

/**
@brief Reads in file for SW_VegEstab and species establishment parameters

@param use_VegEstab Overall decision if user inputs for vegetation establishment
  should be processed.
@param consider_InputFlag Should the user input flag read from `"estab.in"` be
  considered for turning on/off calculations of vegetation establishment.

@note
  - Establishment is calculated under the following conditions
    - there are input files with species establishment parameters
    - at least one of those files is correctly listed in `"estab.in"`
    - `use_VegEstab` is turned on (`swTRUE`) and
      - `consider_InputFlag` is off
      - OR `consider_InputFlag` is on and the input flag in `"estab.in"` is on
  - Establishment results are included in the output files only
    if `"ESTABL"` is turned on in `"outsetup.in"`
*/
void SW_VES_read2(Bool use_VegEstab, Bool consider_InputFlag) {

	SW_VES_deconstruct();
	SW_VES_construct();

	SW_VegEstab.count = 0;
	SW_VegEstab.use = use_VegEstab;

	if (SW_VegEstab.use) {
		FILE *f;
		char buf[FILENAME_MAX];

		MyFileName = SW_F_name(eVegEstab);
		f = OpenFile(MyFileName, "r");

		if (!GetALine(f, inbuf) || (consider_InputFlag && *inbuf == '0')) {
			/* turn off vegetation establishment if either
					 * no species listed
					 * if user input flag is set to 0 and we don't ignore that input,
						 i.e.,`consider_InputFlag` is set to `swTRUE`
			*/
			SW_VegEstab.use = swFALSE;
			if (EchoInits) {
				LogError(logfp, LOGNOTE, "Establishment not used.\n");
			}

		} else {
			/* read file names with species establishment parameters
				 and read those files one by one
			*/
			while (GetALine(f, inbuf)) {
				strcpy(buf, _ProjDir); // add `_ProjDir` to path, e.g., for STEPWAT2
				strcat(buf, inbuf);
				_read_spp(buf);
			}

			SW_VegEstab_construct();
		}

		CloseFile(&f);
	}
}


/**
@brief Construct SW_VegEstab output variables
*/
void SW_VegEstab_construct(void)
{
	if (SW_VegEstab.count > 0) {
		SW_VegEstab.p_oagg[eSW_Year]->days = (TimeInt *) Mem_Calloc(
			SW_VegEstab.count, sizeof(TimeInt), "SW_VegEstab_construct()");

		SW_VegEstab.p_accu[eSW_Year]->days = (TimeInt *) Mem_Calloc(
			SW_VegEstab.count, sizeof(TimeInt), "SW_VegEstab_construct()");
	}
}


/**
	@brief Initialization and checks of species establishment parameters

	This works correctly only after
		* species establishment parameters are read from file by `SW_VES_read()`
		* soil layers are initialized by `SW_SIT_init_run()`
*/
void SW_VES_init_run(void) {
	IntU i;

	for (i = 0; i < SW_VegEstab.count; i++) {
		_spp_init(i);
	}

	if (EchoInits) {
		_echo_VegEstab();
	}
}



/**
@brief Check that each count coincides with a day of the year.
*/
void SW_VES_checkestab(SW_WEATHER* SW_Weather, SW_SOILWAT* SW_SoilWat) {
	/* =================================================== */
	IntUS i;

	for (i = 0; i < SW_VegEstab.count; i++)
		_checkit(SW_Model.doy, i, SW_Weather, SW_SoilWat);
}



/* =================================================== */
/*            Local Function Definitions               */
/* --------------------------------------------------- */

static void _checkit(TimeInt doy, unsigned int sppnum,
					 SW_WEATHER* SW_Weather, SW_SOILWAT* SW_SoilWat) {

	SW_VEGESTAB_INFO *v = SW_VegEstab.parms[sppnum];
	SW_WEATHER_NOW *wn = &SW_Weather->now;

	IntU i;
	RealF avgtemp = wn->temp_avg, /* avg of today's min/max temp */
	avgswc; /* avg_swc today */

	if (doy == SW_Model.firstdoy) {
		_zero_state(sppnum);
	}

	if (v->no_estab || v->estab_doy > 0)
		goto LBL_Normal_Exit;

	/* keep up with germinating wetness regardless of current state */
	if (GT(SW_SoilWat->swcBulk[Today][0], v->min_swc_germ))
		v->wetdays_for_germ++;
	else
		v->wetdays_for_germ = 0;

	if (doy < v->min_pregerm_days)
		goto LBL_Normal_Exit;

	/* ---- check for germination, establishment */
	if (!v->germd && v->wetdays_for_germ >= v->min_wetdays_for_germ) {

		if (doy < v->min_pregerm_days)
			goto LBL_Normal_Exit;
		if (doy > v->max_pregerm_days) {
			v->no_estab = swTRUE;
			goto LBL_Normal_Exit;
		}
		/* temp doesn't affect wetdays */
		if (LT(avgtemp, v->min_temp_germ) || GT(avgtemp, v->max_temp_germ))
			goto LBL_Normal_Exit;

		v->germd = swTRUE;
		goto LBL_Normal_Exit;

	} else { /* continue monitoring sprout's progress */

		/* any dry period (> max_drydays) or temp out of range
		 * after germination means restart */
		for (i = avgswc = 0; i < v->estab_lyrs;)
			avgswc += SW_SoilWat->swcBulk[Today][i++];
		avgswc /= (RealF) v->estab_lyrs;
		if (LT(avgswc, v->min_swc_estab)) {
			v->drydays_postgerm++;
			v->wetdays_for_estab = 0;
		} else {
			v->drydays_postgerm = 0;
			v->wetdays_for_estab++;
		}

		if (v->drydays_postgerm > v->max_drydays_postgerm || LT(avgtemp, v->min_temp_estab) || GT(avgtemp, v->max_temp_estab)) {
			/* too bad: discontinuity in environment, plant dies, start over */
			goto LBL_EstabFailed_Exit;
		}

		v->germ_days++;

		if (v->wetdays_for_estab < v->min_wetdays_for_estab || v->germ_days < v->min_days_germ2estab) {
			goto LBL_Normal_Exit;
			/* no need to zero anything */
		}

		if (v->germ_days > v->max_days_germ2estab)
			goto LBL_EstabFailed_Exit;

		v->estab_doy = SW_Model.doy;
		goto LBL_Normal_Exit;
	}

	LBL_EstabFailed_Exit:
	/* allows us to try again if not too late */
	v->wetdays_for_estab = 0;
	v->germ_days = 0;
	v->germd = swFALSE;

	LBL_Normal_Exit: return;
}

static void _zero_state(unsigned int sppnum) {
	/* =================================================== */
	/* zero any values that need it for the new growing season */

	SW_VEGESTAB_INFO *v = SW_VegEstab.parms[sppnum];

	v->no_estab = v->germd = swFALSE;
	v->estab_doy = v->germ_days = v->drydays_postgerm = 0;
	v->wetdays_for_germ = v->wetdays_for_estab = 0;
}

static void _read_spp(const char *infile) {
	/* =================================================== */
	SW_VEGESTAB_INFO *v;
	const int nitems = 16;
	FILE *f;
	int lineno = 0;
	char name[80]; /* only allow 4 char sppnames */

	f = OpenFile(infile, "r");

	unsigned int count;

	count = _new_species();
	v = SW_VegEstab.parms[count];

	strcpy(v->sppFileName, inbuf); //have to copy before the pointer infile gets reset below by getAline

	while (GetALine(f, inbuf)) {
		switch (lineno) {
		case 0:
			strcpy(name, inbuf);
			break;
		case 1:
			v->vegType = atoi(inbuf);
			break;
		case 2:
			v->estab_lyrs = atoi(inbuf);
			break;
		case 3:
			v->bars[SW_GERM_BARS] = fabs(atof(inbuf));
			break;
		case 4:
			v->bars[SW_ESTAB_BARS] = fabs(atof(inbuf));
			break;
		case 5:
			v->min_pregerm_days = atoi(inbuf);
			break;
		case 6:
			v->max_pregerm_days = atoi(inbuf);
			break;
		case 7:
			v->min_wetdays_for_germ = atoi(inbuf);
			break;
		case 8:
			v->max_drydays_postgerm = atoi(inbuf);
			break;
		case 9:
			v->min_wetdays_for_estab = atoi(inbuf);
			break;
		case 10:
			v->min_days_germ2estab = atoi(inbuf);
			break;
		case 11:
			v->max_days_germ2estab = atoi(inbuf);
			break;
		case 12:
			v->min_temp_germ = atof(inbuf);
			break;
		case 13:
			v->max_temp_germ = atof(inbuf);
			break;
		case 14:
			v->min_temp_estab = atof(inbuf);
			break;
		case 15:
			v->max_temp_estab = atof(inbuf);
			break;
		}
		/* check for valid name first */
		if (0 == lineno) {
			if (strlen(name) > MAX_SPECIESNAMELEN) {
				CloseFile(&f);
				LogError(logfp, LOGFATAL, "%s: Species name <%s> too long (> %d chars).\n Try again.\n", infile, name, MAX_SPECIESNAMELEN);
			} else {
				strcpy(v->sppname, name);
			}
		}

		lineno++; /*only increments when there's a value */
	}

	if (lineno < nitems) {
		CloseFile(&f);
		LogError(logfp, LOGFATAL, "%s : Too few input parameters.\n", infile);
	}

	CloseFile(&f);
}

/**
@brief Initializations performed after acquiring parameters after read() or some
			other function call.

@param sppnum Index for which paramater is beign initialized.
*/
void _spp_init(unsigned int sppnum) {

	SW_VEGESTAB_INFO *v = SW_VegEstab.parms[sppnum];
	SW_LAYER_INFO **lyr = SW_Site.lyr;
	IntU i;

	/* The thetas and psis etc should be initialized by now */
	/* because init_layers() must be called prior to this routine */
	/* (see watereqn() ) */
	v->min_swc_germ = SW_SWRC_SWPtoSWC(v->bars[SW_GERM_BARS], lyr[0]);

	/* due to possible differences in layer textures and widths, we need
	 * to average the estab swc across the given layers to peoperly
	 * compare the actual swc average in the checkit() routine */
	v->min_swc_estab = 0.;
	for (i = 0; i < v->estab_lyrs; i++) {
		v->min_swc_estab += SW_SWRC_SWPtoSWC(v->bars[SW_ESTAB_BARS], lyr[i]);
	}
	v->min_swc_estab /= v->estab_lyrs;

	_sanity_check(sppnum);

}

static void _sanity_check(unsigned int sppnum) {
	/* =================================================== */
	SW_LAYER_INFO **lyr = SW_Site.lyr;
	SW_VEGESTAB_INFO *v = SW_VegEstab.parms[sppnum];

	double mean_wiltpt;
	unsigned int i;

	if (v->vegType >= NVEGTYPES) {
		LogError(
			logfp,
			LOGFATAL,
			"%s (%s) : Specified vegetation type (%d) is not implemented.",
			MyFileName,
			v->sppname,
			v->vegType
		);
	}

	if (v->estab_lyrs > SW_Site.n_transp_lyrs[v->vegType]) {
		LogError(
			logfp,
			LOGFATAL,
			"%s (%s) : Layers requested (estab_lyrs = %d) > "
			"(# transpiration layers = %d).",
			MyFileName,
			v->sppname,
			v->estab_lyrs,
			SW_Site.n_transp_lyrs[v->vegType]
		);
	}

	if (v->min_pregerm_days > v->max_pregerm_days) {
		LogError(
			logfp,
			LOGFATAL,
			"%s (%s) : First day of germination > last day of germination.",
			MyFileName,
			v->sppname
		);
	}

	if (v->min_wetdays_for_estab > v->max_days_germ2estab) {
		LogError(
			logfp,
			LOGFATAL,
			"%s (%s) : Minimum wetdays after germination (%d) > "
			"maximum days allowed for establishment (%d).",
			MyFileName,
			v->sppname,
			v->min_wetdays_for_estab,
			v->max_days_germ2estab
		);
	}

	if (v->min_swc_germ < lyr[0]->swcBulk_wiltpt) {
		LogError(
			logfp,
			LOGFATAL,
			"%s (%s) : Minimum swc for germination (%.4f) < wiltpoint (%.4f)",
			MyFileName,
			v->sppname,
			v->min_swc_germ,
			lyr[0]->swcBulk_wiltpt
		);
	}

	mean_wiltpt = 0.;
	for (i = 0; i < v->estab_lyrs; i++) {
		mean_wiltpt += lyr[i]->swcBulk_wiltpt;
	}
	mean_wiltpt /= v->estab_lyrs;

	if (LT(v->min_swc_estab, mean_wiltpt)) {
		LogError(
			logfp,
			LOGFATAL,
			"%s (%s) : Minimum swc for establishment (%.4f) < wiltpoint (%.4f)",
			MyFileName,
			v->sppname,
			v->min_swc_estab,
			mean_wiltpt
		);
	}

}

/**
@brief First time called with no species defined so SW_VegEstab.count == 0 and
			SW_VegEstab.parms is not initialized yet, malloc() required.  For each
			species thereafter realloc() is called.

@return (++v->count) - 1
*/
unsigned int _new_species(void) {

	const char *me = "SW_VegEstab_newspecies()";
	SW_VEGESTAB *v = &SW_VegEstab;

	v->parms =
			(!v->count) ?
					(SW_VEGESTAB_INFO **) Mem_Calloc(v->count + 1, sizeof(SW_VEGESTAB_INFO *), me) :
					(SW_VEGESTAB_INFO **) Mem_ReAlloc(v->parms, sizeof(SW_VEGESTAB_INFO *) * (v->count + 1));
	v->parms[v->count] = (SW_VEGESTAB_INFO *) Mem_Calloc(1, sizeof(SW_VEGESTAB_INFO), me);

	return (++v->count) - 1;
}

/**
@brief Text output for VegEstab.
*/
void _echo_VegEstab(void) {
	/* --------------------------------------------------- */
	SW_VEGESTAB_INFO **v = SW_VegEstab.parms;
	SW_LAYER_INFO **lyr = SW_Site.lyr;
	IntU i;
	char outstr[2048];

	snprintf(
		errstr,
		MAX_ERROR,
		"\n=========================================================\n\n"
		"Parameters for the SoilWat Vegetation Establishment Check.\n"
		"----------------------------------------------------------\n"
		"Number of species to be tested: %d\n",
		SW_VegEstab.count
	);

	strcpy(outstr, errstr);
	for (i = 0; i < SW_VegEstab.count; i++) {
		snprintf(
			errstr,
			MAX_ERROR,
			"Species: %s (vegetation type %s [%d])\n----------------\n"
			"Germination parameters:\n"
			"\tMinimum SWP (bars)  : -%.4f\n"
			"\tMinimum SWC (cm/cm) : %.4f\n"
			"\tMinimum SWC (cm/lyr): %.4f\n"
			"\tMinimum temperature : %.1f\n"
			"\tMaximum temperature : %.1f\n"
			"\tFirst possible day  : %d\n"
			"\tLast  possible day  : %d\n"
			"\tMinimum consecutive wet days (after first possible day): %d\n",
			v[i]->sppname,
			key2veg[v[i]->vegType],
			v[i]->vegType,
			v[i]->bars[SW_GERM_BARS],
			v[i]->min_swc_germ / lyr[0]->width,
			v[i]->min_swc_germ,
			v[i]->min_temp_germ,
			v[i]->max_temp_germ,
			v[i]->min_pregerm_days,
			v[i]->max_pregerm_days,
			v[i]->min_wetdays_for_germ
		);

		snprintf(
			errstr,
			MAX_ERROR,
			"Establishment parameters:\n"
			"\tNumber of layers affecting successful establishment: %d\n"
			"\tMinimum SWP (bars) : -%.4f\n"
			"\tMinimum SWC (cm/layer) averaged across top %d layers: %.4f\n"
			"\tMinimum temperature : %.1f\n"
			"\tMaximum temperature : %.1f\n"
			"\tMinimum number of days after germination      : %d\n"
			"\tMaximum number of days after germination      : %d\n"
			"\tMinimum consecutive wet days after germination: %d\n"
			"\tMaximum consecutive dry days after germination: %d\n"
			"---------------------------------------------------------------\n\n",
			v[i]->estab_lyrs, v[i]->bars[SW_ESTAB_BARS], v[i]->estab_lyrs,
			v[i]->min_swc_estab, v[i]->min_temp_estab, v[i]->max_temp_estab,
			v[i]->min_days_germ2estab, v[i]->max_days_germ2estab, v[i]->min_wetdays_for_estab,
			v[i]->max_drydays_postgerm
		);

		strcat(outstr, errstr);
	}
	strcat(outstr, "\n-----------------  End of Establishment Parameters ------------\n");

	LogError(logfp, LOGNOTE, outstr);
}
