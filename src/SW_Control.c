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
#include "include/SW_VegProd.h"
#include "include/Times.h"
#include "include/SW_Files.h"
#include "include/SW_Control.h"
#include "include/SW_Model.h" // externs SW_Model
#include "include/SW_Output.h"
#include "include/SW_Site.h" // externs SW_Site
#include "include/SW_Flow_lib.h"
#include "include/SW_Flow_lib_PET.h"
#include "include/SW_Flow.h"
#include "include/SW_VegEstab.h" // externs SW_VegEstab
#include "include/SW_Weather.h"  // externs SW_Weather
#include "include/SW_Markov.h"
#include "include/SW_Sky.h"
#include "include/SW_Carbon.h"



/* =================================================== */
/*             Local Function Definitions              */
/* --------------------------------------------------- */
/**
@brief Initiate/update variables for a new simulation year.
      In addition to the timekeeper (Model), usually only modules
      that read input yearly or produce output need to have this call.

@param[in,out] sw Comprehensive struct of type SW_ALL containing all
  information in the simulation
*/
static void _begin_year(SW_ALL* sw) {
	// SW_F_new_year() not needed
	SW_MDL_new_year(&sw->Model); // call first to set up time-related arrays for this year
	// SW_MKV_new_year() not needed
	SW_SKY_new_year(sw->Model.year, sw->Model.startyr, sw->Sky.snow_density,
                  sw->Sky.snow_density_daily); // Update daily climate variables from monthly values
	//SW_SIT_new_year() not needed
	SW_VES_new_year(sw->VegEstab.count);
	SW_VPD_new_year(&sw->VegProd, sw->Model.simyear); // Dynamic CO2 effects on vegetation
	// SW_FLW_new_year() not needed
	SW_SWC_new_year(&sw->SoilWat, &sw->Site, sw->Model.year);
	// SW_CBN_new_year() not needed
	SW_OUT_new_year(sw->Model.firstdoy, sw->Model.lastdoy);
}

static void _begin_day(SW_ALL* sw) {
	SW_MDL_new_day(&sw->Model);
	SW_WTH_new_day(&sw->Weather, &sw->Site, sw->SoilWat.snowpack,
                  sw->Model.doy, sw->Model.year);
}

static void _end_day(SW_ALL* sw) {
	_collect_values(sw);
	SW_SWC_end_day(&sw->SoilWat, sw->Site.n_layers);
}


/* =================================================== */
/*             Global Function Definitions             */
/* --------------------------------------------------- */
/**
@brief Calls 'SW_CTL_run_current_year' for each year
          which calls 'SW_SWC_water_flow' for each day.

@param[in,out] sw Comprehensive struct of type SW_ALL containing all
  information in the simulation
*/

void SW_CTL_main(SW_ALL* sw) {
  #ifdef SWDEBUG
  int debug = 0;
  #endif

  TimeInt *cur_yr = &sw->Model.year;

  for (*cur_yr = sw->Model.startyr; *cur_yr <= sw->Model.endyr; (*cur_yr)++) {
    #ifdef SWDEBUG
    if (debug) swprintf("\n'SW_CTL_main': simulate year = %d\n", *cur_yr);
    #endif

    SW_CTL_run_current_year(sw);
  }
} /******* End Main Loop *********/

/** @brief Setup and construct model (independent of inputs)
 *
 * @param[in,out] sw Comprehensive struct of type SW_ALL containing all
 * information in the simulation
 * @param[in] firstfile First input file
 */
void SW_CTL_setup_model(SW_ALL* sw, const char *firstfile) {

	SW_F_construct(firstfile);
	SW_MDL_construct(sw->Model.newperiod);
	SW_WTH_construct(&sw->Weather);
	// delay SW_MKV_construct() until we know from inputs whether we need it
	// SW_SKY_construct() not need
	SW_SIT_construct(&sw->Site);
	SW_VES_construct(&sw->VegEstab);
	SW_VPD_construct(&sw->VegProd);
	// SW_FLW_construct() not needed
	SW_OUT_construct(sw->Site.n_layers);
	SW_SWC_construct(&sw->SoilWat);
	SW_CBN_construct(&sw->Carbon);
}


/**
  @brief Free allocated memory

  @param full_reset
    * If `FALSE`, de-allocate memory for `SOILWAT2` variables, but
        * do not reset output arrays `p_OUT` and `p_OUTsd` which are used under
          `SW_OUTARRAY` to pass output in-memory to `rSOILWAT2` and to
          `STEPWAT2`
    * if `TRUE`, de-allocate all memory including output arrays.
  @param[in,out] sw Comprehensive structure holding all information
    dealt with in SOILWAT2
*/
void SW_CTL_clear_model(Bool full_reset, SW_ALL* sw) {
	SW_F_deconstruct();
	SW_MDL_deconstruct();
	SW_WTH_deconstruct(&sw->Weather); // calls SW_MKV_deconstruct() if needed
	// SW_SKY_deconstruct() not needed
	SW_SIT_deconstruct(&sw->Site);
	SW_VES_deconstruct(&sw->VegEstab);
	SW_VPD_deconstruct(&sw->VegProd);
	// SW_FLW_deconstruct() not needed
	SW_OUT_deconstruct(full_reset);
	SW_SWC_deconstruct(&sw->SoilWat);
	SW_CBN_deconstruct();
}

/** @brief Initialize simulation run (based on user inputs)
  Note: Time will only be set up correctly while carrying out a
  simulation year, i.e., after calling _begin_year()

  @param[in,out] sw Comprehensive structure holding all information
    dealt with in SOILWAT2
*/
void SW_CTL_init_run(SW_ALL* sw) {

	// SW_F_init_run() not needed
	// SW_MDL_init_run() not needed
	SW_WTH_init_run(&sw->Weather);
	// SW_MKV_init_run() not needed
	SW_PET_init_run();
	// SW_SKY_init_run() not needed
	SW_SIT_init_run(&sw->VegProd, &sw->Site);
	SW_VES_init_run(sw->VegEstab.parms, sw->Site.lyr,
                  sw->Site.n_transp_lyrs, sw->VegEstab.count); // must run after `SW_SIT_init_run()`
	SW_VPD_init_run(&sw->VegProd, &sw->Weather, sw->Model.startyr,
                  sw->Model.endyr, sw->Site.latitude);
	SW_FLW_init_run(&sw->SoilWat);
	SW_ST_init_run();
	// SW_OUT_init_run() handled separately so that SW_CTL_init_run() can be
	//   useful for unit tests, rSOILWAT2, and STEPWAT2 applications
	SW_SWC_init_run(&sw->SoilWat, &sw->Site);
	SW_CBN_init_run(sw->VegProd.veg, &sw->Model, &sw->Carbon);
}


/**
@brief Calls 'SW_SWC_water_flow' for each day.

@param[in,out] sw Comprehensive struct of type SW_ALL containing
  all information in the simulation
*/
void SW_CTL_run_current_year(SW_ALL* sw) {
  /*=======================================================*/
  TimeInt *doy = &sw->Model.doy; // base1
  #ifdef SWDEBUG
  int debug = 0;
  #endif

  #ifdef SWDEBUG
  if (debug) swprintf("\n'SW_CTL_run_current_year': begin new year\n");
  #endif
  _begin_year(sw);

  for (*doy = sw->Model.firstdoy; *doy <= sw->Model.lastdoy; (*doy)++) {
    #ifdef SWDEBUG
    if (debug) swprintf("\t: begin doy = %d ... ", *doy);
    #endif
    _begin_day(sw);

    #ifdef SWDEBUG
    if (debug) swprintf("simulate water ... ");
    #endif
    SW_SWC_water_flow(sw);

    // Only run this function if SWA output is asked for
    if (sw->VegProd.use_SWA) {
      calculate_repartitioned_soilwater(&sw->SoilWat, &sw->VegProd,
                                        sw->Site.lyr, sw->Site.n_layers);
    }

    if (sw->VegEstab.use) {
      SW_VES_checkestab(sw->VegEstab.parms, &sw->Weather, sw->SoilWat.swcBulk,
                        sw->Model.doy, sw->Model.firstdoy, sw->VegEstab.count);
    }

    #ifdef SWDEBUG
    if (debug) swprintf("ending day ... ");
    #endif
    _end_day(sw);

    #ifdef SWDEBUG
    if (debug) swprintf("doy = %d completed.\n", *doy);
    #endif
  }

  #ifdef SWDEBUG
  if (debug) swprintf("'SW_CTL_run_current_year': flush output\n");
  #endif
  SW_OUT_flush(sw);

  #ifdef SWDEBUG
  if (debug) swprintf("'SW_CTL_run_current_year': completed.\n");
  #endif
}


/**
@brief Reads inputs from disk and makes a print statement if there is an error
        in doing so.

@param[in,out] sw Comprehensive struct of type SW_ALL containing
  all information in the simulation
*/
void SW_CTL_read_inputs_from_disk(SW_ALL* sw) {
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

  SW_MDL_read(&sw->Model);
  #ifdef SWDEBUG
  if (debug) swprintf(" > 'model'");
  #endif

  SW_WTH_setup(&sw->Weather);
  #ifdef SWDEBUG
  if (debug) swprintf(" > 'weather setup'");
  #endif

  SW_SKY_read(&sw->Sky);
  #ifdef SWDEBUG
  if (debug) swprintf(" > 'climate'");
  #endif

  if (sw->Weather.generateWeatherMethod == 2) {
    SW_MKV_setup(sw->Weather.rng_seed, sw->Weather.generateWeatherMethod);
    #ifdef SWDEBUG
    if (debug) swprintf(" > 'weather generator'");
    #endif
  }

  SW_WTH_read(&sw->Weather, &sw->Sky, sw->Model.startyr, sw->Model.endyr);
  #ifdef SWDEBUG
  if (debug) swprintf(" > 'weather read'");
  #endif

  SW_VPD_read(&sw->VegProd);
  #ifdef SWDEBUG
  if (debug) swprintf(" > 'veg'");
  #endif

  SW_SIT_read(&sw->Site, &sw->Carbon);
  #ifdef SWDEBUG
  if (debug) swprintf(" > 'site'");
  #endif

  SW_LYR_read(&sw->Site);
  #ifdef RSWDEBUG
  if (debug) swprintf(" > 'soils'");
  #endif

  SW_SWRC_read(&sw->Site);
  #ifdef SWDEBUG
  if (debug) swprintf(" > 'swrc parameters'");
  #endif

  SW_VES_read(&sw->VegEstab);
  #ifdef SWDEBUG
  if (debug) swprintf(" > 'establishment'");
  #endif

  SW_OUT_read(sw);
  #ifdef SWDEBUG
  if (debug) swprintf(" > 'ouput'");
  #endif

  SW_CBN_read(&sw->Carbon, &sw->Model);
  #ifdef SWDEBUG
  if (debug) swprintf(" > 'CO2'");
  #endif

  SW_SWC_read(&sw->SoilWat, sw->Model.endyr, sw->Site.lyr, sw->Site.n_layers);
  #ifdef SWDEBUG
  if (debug) swprintf(" > 'swc'");
  if (debug) swprintf(" completed.\n");
  #endif
}



#ifdef DEBUG_MEM
#include "include/SW_Markov.h"  /* for setmemrefs function */

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
