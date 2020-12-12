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
#include "Times.h"
#include "filefuncs.h"
#include "rands.h"
#include "SW_Defines.h"
#include "SW_Files.h"
#include "SW_Control.h"
#include "SW_Model.h"
#include "SW_Output.h"
#include "SW_Site.h"
#include "SW_Flow_lib.h"
#include "SW_Flow_lib_PET.h"
#include "SW_Flow.h"
#include "SW_SoilWater.h"
#include "SW_VegEstab.h"
#include "SW_VegProd.h"
#include "SW_Weather.h"
#include "SW_Markov.h"
#include "SW_Sky.h"
#include "SW_Carbon.h"

/* =================================================== */
/*                  Global Declarations                */
/* --------------------------------------------------- */
extern SW_MODEL SW_Model;
extern SW_VEGESTAB SW_VegEstab;
extern SW_SITE SW_Site;
extern SW_VEGPROD SW_VegProd;
extern SW_WEATHER SW_Weather;

/* =================================================== */
/*                Module-Level Declarations            */
/* --------------------------------------------------- */
static void _begin_year(void);
static void _begin_day(void);
static void _end_day(void);


/*******************************************************/
/***************** Begin Main Code *********************/
/**
@brief Calls 'SW_CTL_run_current_year' for each year
          which calls 'SW_SWC_water_flow' for each day.
*/

void SW_CTL_main(void) {
  #ifdef SWDEBUG
  int debug = 0;
  #endif

  TimeInt *cur_yr = &SW_Model.year;

  for (*cur_yr = SW_Model.startyr; *cur_yr <= SW_Model.endyr; (*cur_yr)++) {
    #ifdef SWDEBUG
    if (debug) swprintf("\n'SW_CTL_main': simulate year = %d\n", *cur_yr);
    #endif

    SW_CTL_run_current_year();
  }
} /******* End Main Loop *********/

/** @brief Setup and construct model (independent of inputs)
 */
void SW_CTL_setup_model(const char *firstfile) {

	SW_F_construct(firstfile);
	SW_MDL_construct();
	SW_WTH_construct();
	// delay SW_MKV_construct() until we know from inputs whether we need it
	// SW_SKY_construct() not need
	SW_SIT_construct();
	SW_VES_construct();
	SW_VPD_construct();
	// SW_FLW_construct() not needed
	SW_OUT_construct();
	SW_SWC_construct();
	SW_CBN_construct();
}


/** @brief Free allocated memory
		@param full_reset
			* If `FALSE`, de-allocate memory for `SOILWAT2` variables, but
					* do not reset output arrays `p_OUT` and `p_OUTsd` which are used under
						`SW_OUTARRAY` to pass output in-memory to `rSOILWAT2` and to
						`STEPWAT2`
			* if `TRUE`, de-allocate all memory including output arrays.
*/
void SW_CTL_clear_model(Bool full_reset) {
	SW_F_deconstruct();
	SW_MDL_deconstruct();
	SW_WTH_deconstruct(); // calls SW_MKV_deconstruct() if needed
	// SW_SKY_deconstruct() not needed
	SW_SIT_deconstruct();
	SW_VES_deconstruct();
	SW_VPD_deconstruct();
	// SW_FLW_deconstruct() not needed
	SW_OUT_deconstruct(full_reset);
	SW_SWC_deconstruct();
	SW_CBN_deconstruct();
}

/** @brief Initialize simulation run (based on user inputs)
  Note: Time will only be set up correctly while carrying out a
  simulation year, i.e., after calling _begin_year()
*/
void SW_CTL_init_run(void) {

	// SW_F_init_run() not needed
	// SW_MDL_init_run() not needed
	SW_WTH_init_run();
	// SW_MKV_init_run() not needed
	SW_PET_init_run();
	SW_SKY_init_run();
	SW_SIT_init_run();
	// SW_VES_init_run() not needed
	SW_VPD_init_run();
	SW_FLW_init_run();
	SW_ST_init_run();
	// SW_OUT_init_run() handled separately so that SW_CTL_init_run() can be
	//   useful for unit tests, rSOILWAT2, and STEPWAT2 applications
	SW_SWC_init_run();
	SW_CBN_init_run();
}


/**
@brief Calls 'SW_SWC_water_flow' for each day.
*/
void SW_CTL_run_current_year(void) {
  /*=======================================================*/
  TimeInt *doy = &SW_Model.doy; // base1
  #ifdef SWDEBUG
  int debug = 0;
  #endif

  #ifdef SWDEBUG
  if (debug) swprintf("\n'SW_CTL_run_current_year': begin new year\n");
  #endif
  _begin_year();

  for (*doy = SW_Model.firstdoy; *doy <= SW_Model.lastdoy; (*doy)++) {
    #ifdef SWDEBUG
    if (debug) swprintf("\t: begin doy = %d ... ", *doy);
    #endif
    _begin_day();

    #ifdef SWDEBUG
    if (debug) swprintf("simulate water ... ");
    #endif
    SW_SWC_water_flow();

    // Only run this function if SWA output is asked for
    if (SW_VegProd.use_SWA) {
      calculate_repartitioned_soilwater();
    }

    if (SW_VegEstab.use) {
      SW_VES_checkestab();
    }

    #ifdef SWDEBUG
    if (debug) swprintf("ending day ... ");
    #endif
    _end_day();

    #ifdef SWDEBUG
    if (debug) swprintf("doy = %d completed.\n", *doy);
    #endif
  }

  #ifdef SWDEBUG
  if (debug) swprintf("'SW_CTL_run_current_year': flush output\n");
  #endif
  SW_OUT_flush();

  #ifdef SWDEBUG
  if (debug) swprintf("'SW_CTL_run_current_year': completed.\n");
  #endif
}

/**
@brief Initiate/update variables for a new simulation year.
      In addition to the timekeeper (Model), usually only modules
      that read input yearly or produce output need to have this call.
*/
static void _begin_year(void) {

	// SW_F_new_year() not needed
	SW_MDL_new_year(); // call first to set up time-related arrays for this year
	SW_WTH_new_year();
	// SW_MKV_new_year() not needed
	SW_SKY_new_year(); // Update daily climate variables from monthly values
	//SW_SIT_new_year() not needed
	SW_VES_new_year();
	SW_VPD_new_year(); // Dynamic CO2 effects on vegetation
	// SW_FLW_new_year() not needed
	SW_SWC_new_year();
	// SW_CBN_new_year() not needed
	SW_OUT_new_year();
}

static void _begin_day(void) {
	SW_MDL_new_day();
	SW_WTH_new_day();
}

static void _end_day(void) {
	_collect_values();
	SW_WTH_end_day();
	SW_SWC_end_day();
}

/**
@brief Reads inputs from disk and makes a print statement if there is an error
        in doing so.
*/
void SW_CTL_read_inputs_from_disk(void) {
  #ifdef SWDEBUG
  int debug = 0;
  #endif

  #ifdef SWDEBUG
  if (debug) swprintf("'SW_CTL_read_inputs_from_disk': Read input from disk:");
  #endif

  SW_F_read(NULL);
  #ifdef SWDEBUG
  if (debug) swprintf(" 'files'");
  #endif

  SW_MDL_read();
  #ifdef SWDEBUG
  if (debug) swprintf(" > 'model'");
  #endif

  SW_WTH_read();
  #ifdef SWDEBUG
  if (debug) swprintf(" > 'weather'");
  #endif

  SW_SKY_read();
  #ifdef SWDEBUG
  if (debug) swprintf(" > 'climate'");
  #endif

  if (SW_Weather.use_weathergenerator) {
    SW_MKV_setup();
    #ifdef SWDEBUG
    if (debug) swprintf(" > 'weather generator'");
    #endif
  }

  SW_VPD_read();
  #ifdef SWDEBUG
  if (debug) swprintf(" > 'veg'");
  #endif

  SW_SIT_read(); // inputs also soil layer data
  #ifdef SWDEBUG
  if (debug) swprintf(" > 'site' + 'soils'");
  #endif

  SW_VES_read();
  #ifdef SWDEBUG
  if (debug) swprintf(" > 'establishment'");
  #endif

  SW_OUT_read();
  #ifdef SWDEBUG
  if (debug) swprintf(" > 'ouput'");
  #endif

  SW_CBN_read();
  #ifdef SWDEBUG
  if (debug) swprintf(" > 'CO2'");
  #endif

  SW_SWC_read();
  #ifdef SWDEBUG
  if (debug) swprintf(" > 'swc'");
  if (debug) swprintf(" completed.\n");
  #endif
}



#ifdef DEBUG_MEM
#include "SW_Markov.h"  /* for setmemrefs function */

/**
@brief This routine sets the known memory refs so they can be
      checked for leaks, etc.  Includes malloc-ed memory in
      SOILWAT.  All refs will have been cleared
      by a call to ClearMemoryRefs() before this, and will be
      checked via CheckMemoryRefs() after this, most likely in
      the main() function.
*/
void SW_CTL_SetMemoryRefs( void) {
	/* when debugging memory problems, use the bookkeeping
	 code in myMemory.c

	 */

	SW_F_SetMemoryRefs();
	SW_OUT_SetMemoryRefs();
	SW_SWC_SetMemoryRefs();
	SW_SIT_SetMemoryRefs();
	SW_WTH_SetMemoryRefs();
	SW_MKV_SetMemoryRefs();
}

#endif
