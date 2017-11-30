/********************************************************/
/********************************************************/
/*  Source file: Control.c
 *  Type: module
 *  Application: SOILWAT - soilwater dynamics simulator
 *  Purpose: This module controls the flow of the model.
 *           Previously this was done in main() but to
 *           combine the model with other code (eg STEPPE)
 *           there needs to be separate callable routines
 *           for initializing, model flow, and output.
 *
 *  History:
 *     (10-May-02) -- INITIAL CODING - cwb
 02/04/2012	(drs)	in function '_read_inputs()' moved order of 'SW_VPD_read' from after 'SW_VES_read' to before 'SW_SIT_read': SWPcrit is read in in 'SW_VPD_read' and then calculated SWC_atSWPcrit is assigned to each layer in 'SW_SIT_read'
 06/24/2013	(rjm)	added call to SW_FLW_construct() in function SW_CTL_init_model()
 */
/********************************************************/
/********************************************************/

/* =================================================== */
/*                INCLUDES / DEFINES                   */
/* --------------------------------------------------- */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "generic.h"
#include "filefuncs.h"
#include "rands.h"
#include "SW_Defines.h"
#include "SW_Files.h"
#include "SW_Control.h"
#include "SW_Model.h"
#include "SW_Output.h"
#include "SW_Site.h"
#include "SW_SoilWater.h"
#include "SW_VegEstab.h"
#include "SW_VegProd.h"
#include "SW_Weather.h"
#include "SW_Carbon.h"

/* =================================================== */
/*                  Global Declarations                */
/* --------------------------------------------------- */
extern SW_MODEL SW_Model;
extern SW_VEGESTAB SW_VegEstab;
extern SW_SITE SW_Site;
extern SW_VEGPROD SW_VegProd;
#ifdef RSOILWAT
	extern Bool useFiles;
	extern SEXP InputData;
	void SW_FLW_construct(void);
#endif


/* =================================================== */
/*                Module-Level Declarations            */
/* --------------------------------------------------- */
static void _begin_year(void);
static void _begin_day(void);
static void _end_day(void);
static void _collect_values(void);

/* Dummy declaration for Flow constructor defined in SW_Flow.c */
void SW_FLW_construct(void);

/*******************************************************/
/***************** Begin Main Code *********************/

void SW_CTL_main(void) {

	TimeInt *cur_yr = &SW_Model.year;

	for (*cur_yr = SW_Model.startyr; *cur_yr <= SW_Model.endyr; (*cur_yr)++) {
		SW_CTL_run_current_year();
	}

#ifndef RSOILWAT
	SW_OUT_close_files();
#endif
} /******* End Main Loop *********/
/*******************************************************/

void SW_CTL_init_model(const char *firstfile) {
	/*=======================================================*/
	/* initialize all structures, simulating
	 * a constructor call */
	SW_F_construct(firstfile);
	SW_MDL_construct();
	SW_WTH_construct();
	SW_SIT_construct();
	SW_VES_construct();
	SW_VPD_construct();
	SW_OUT_construct();
	SW_SWC_construct();
	SW_FLW_construct();
	SW_CBN_construct();
}

void SW_CTL_run_current_year(void) {
	/*=======================================================*/
	TimeInt *doy = &SW_Model.doy;

	_begin_year();

	for (*doy = SW_Model.firstdoy; *doy <= SW_Model.lastdoy; (*doy)++) {
		_begin_day();

		SW_SWC_water_flow();

		if (SW_VegEstab.use)
			SW_VES_checkestab();

		_end_day();
	}
	SW_OUT_flush();
}

static void _begin_year(void) {
	/*=======================================================*/
	/* in addition to the timekeeper (Model), usually only
	 * modules that read input yearly or produce output need
	 * to have this call */
	 SW_MDL_new_year();
	 SW_WTH_new_year();
	 SW_SWC_new_year();
	 SW_VES_new_year();
	 SW_OUT_new_year();

	 // Dynamic CO2 effects
	 SW_VPD_init();

}

static void _begin_day(void) {
	/*=======================================================*/

	SW_MDL_new_day();
	SW_WTH_new_day();
}

static void _end_day(void) {
	/*=======================================================*/

	_collect_values();
	SW_WTH_end_day();
	SW_SWC_end_day();

}

static void _collect_values(void) {
	/*=======================================================*/

	SW_OUT_sum_today(eSWC);
	SW_OUT_sum_today(eWTH);
	SW_OUT_sum_today(eVES);
	SW_OUT_sum_today(eVPD);

	SW_OUT_write_today();

}

void SW_CTL_read_inputs_from_disk(void) {
  int debug = 0;

  if (debug) swprintf("'SW_CTL_read_inputs_from_disk': Read input from disk:");
  SW_F_read(NULL);
  if (debug) swprintf(" 'files'");

  SW_MDL_read();
  if (debug) swprintf(" > 'model'");

  SW_WTH_read();
  if (debug) swprintf(" > 'weather'");

  SW_VPD_read();
  if (debug) swprintf(" > 'veg'");

  SW_SIT_read();
  if (debug) swprintf(" > 'site'");

  SW_VES_read();
  if (debug) swprintf(" > 'establishment'");

  SW_OUT_read();
  if (debug) swprintf(" > 'ouput'");

  SW_CBN_read();
  if (debug) swprintf(" > 'CO2'");

  SW_SWC_read();
  if (debug) swprintf(" > 'swc'");
  if (debug) swprintf(" completed.\n");
}


void SW_CTL_obtain_inputs(void) {
	/*=======================================================*/

#ifndef RSOILWAT
  SW_CTL_read_inputs_from_disk();


#else
	if (useFiles) {
    SW_CTL_read_inputs_from_disk();

	} else { //Use R data to set the data
	  int debug = 0;

    if (debug) swprintf("'SW_CTL_obtain_inputs': Copy input from 'InputData':");

		onSet_SW_F(GET_SLOT(InputData, install("files")));
    if (debug) swprintf(" 'files'");

		onSet_SW_MDL(GET_SLOT(InputData, install("years")));
    if (debug) swprintf(" > 'model'");

		onSet_SW_WTH(GET_SLOT(InputData, install("weather")));
    if (debug) swprintf(" > 'weather'");

		onSet_SW_VPD(GET_SLOT(InputData, install("prod")));
    if (debug) swprintf(" > 'veg'");

		onSet_SW_SIT(GET_SLOT(InputData, install("site")));
    if (debug) swprintf(" > 'site'");

		onSet_SW_VES(GET_SLOT(InputData, install("estab")));
    if (debug) swprintf(" > 'establishment'");

		onSet_SW_OUT(GET_SLOT(InputData, install("output")));
    if (debug) swprintf(" > 'ouput'");

		onSet_swCarbon(GET_SLOT(InputData, install("carbon")));
    if (debug) swprintf(" > 'CO2'");

		onSet_SW_SWC(GET_SLOT(InputData, install("swc")));
    if (debug) swprintf(" > 'swc'");
    if (debug) swprintf(" completed.\n");
	}
#endif

  calculate_CO2_multipliers();

}

#ifdef DEBUG_MEM
#include "SW_Markov.h"  /* for setmemrefs function */
/*======================================================*/
void SW_CTL_SetMemoryRefs( void) {
	/* when debugging memory problems, use the bookkeeping
	 code in myMemory.c
	 This routine sets the known memory refs so they can be
	 checked for leaks, etc.  Includes malloc-ed memory in
	 SOILWAT.  All refs will have been cleared
	 by a call to ClearMemoryRefs() before this, and will be
	 checked via CheckMemoryRefs() after this, most likely in
	 the main() function.
	 */

	SW_F_SetMemoryRefs();
	SW_OUT_SetMemoryRefs();
	SW_SWC_SetMemoryRefs();
	SW_SIT_SetMemoryRefs();
	SW_WTH_SetMemoryRefs();
	SW_MKV_SetMemoryRefs();
}

#endif
