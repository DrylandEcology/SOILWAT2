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
#include "include/SW_Domain.h"
#include "include/SW_VegProd.h"
#include "include/SW_Defines.h"
#include "include/Times.h"
#include "include/filefuncs.h"
#include "include/SW_Files.h"
#include "include/SW_Control.h"
#include "include/SW_Model.h"
#include "include/SW_Output.h"
#include "include/SW_Site.h"
#include "include/SW_Flow_lib.h"
#include "include/SW_Flow_lib_PET.h"
#include "include/SW_Flow.h"
#include "include/SW_VegEstab.h"
#include "include/SW_Weather.h"
#include "include/SW_Markov.h"
#include "include/SW_Sky.h"
#include "include/SW_SoilWater.h"
#include "include/SW_Carbon.h"
#include "include/myMemory.h"
#include "include/SW_Main_lib.h"

#if defined(SWNETCDF)
#include "include/SW_netCDF.h"
#endif



/* =================================================== */
/*             Local Function Definitions              */
/* --------------------------------------------------- */
/**
@brief Initiate/update variables for a new simulation year.
      In addition to the timekeeper (Model), usually only modules
      that read input yearly or produce output need to have this call.

@param[in,out] sw Comprehensive struct of type SW_ALL containing all
  information in the simulation
@param[in,out] LogInfo Holds information dealing with logfile output
*/
static void _begin_year(SW_ALL* sw, LOG_INFO* LogInfo) {
	// SW_F_new_year() not needed
	SW_MDL_new_year(&sw->Model); // call first to set up time-related arrays for this year
	// SW_MKV_new_year() not needed
	SW_SKY_new_year(&sw->Model, sw->Sky.snow_density, sw->Sky.snow_density_daily); // Update daily climate variables from monthly values
	//SW_SIT_new_year() not needed
	SW_VES_new_year(sw->VegEstab.count);
	SW_VPD_new_year(&sw->VegProd, &sw->Model); // Dynamic CO2 effects on vegetation
	// SW_FLW_new_year() not needed
	SW_SWC_new_year(&sw->SoilWat, &sw->Site, sw->Model.year, LogInfo);
    if(LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

	// SW_CBN_new_year() not needed
	SW_OUT_new_year(sw->Model.firstdoy, sw->Model.lastdoy, sw->Output);
}

static void _begin_day(SW_ALL* sw, LOG_INFO* LogInfo) {
	SW_MDL_new_day(&sw->Model);
	SW_WTH_new_day(&sw->Weather, &sw->Site, sw->SoilWat.snowpack,
                sw->Model.doy, sw->Model.year, LogInfo);
}

static void _end_day(SW_ALL* sw, SW_OUTPUT_POINTERS* SW_OutputPtrs,
                     LOG_INFO* LogInfo) {
  int localTOffset = 1; // tOffset is one when called from this function

	_collect_values(sw, SW_OutputPtrs, swFALSE, localTOffset, LogInfo);
    if(LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

	SW_SWC_end_day(&sw->SoilWat, sw->Site.n_layers);
}

/**
 * @brief Copy dynamic memory from a template SW_ALL to a new instance
 *
 * @param[in] source Source struct of type SW_ALL to copy
 * @param[out] dest Destination struct of type SW_ALL to be copied into
 * @param[in,out] LogInfo Holds information dealing with logfile output
*/
void SW_ALL_deepCopy(SW_ALL* source, SW_ALL* dest, LOG_INFO* LogInfo)
{
    memcpy(dest, source, sizeof (*dest));

    /* Allocate memory for output pointers */
    SW_CTL_alloc_outptrs(dest, LogInfo);
    if(LogInfo->stopRun) {
        return; // Exit prematurely due to error
    }

    dest->SoilWat.hist.file_prefix = NULL; /* currently unused */

    /* Allocate memory and copy daily weather */
    dest->Weather.allHist = NULL;
    allocateAllWeather(&dest->Weather.allHist, source->Weather.n_years, LogInfo);
    if(LogInfo->stopRun) {
        return; // Exit prematurely due to error
    }
    for(unsigned int year = 0; year < source->Weather.n_years; year++) {
        memcpy(
            dest->Weather.allHist[year],
            source->Weather.allHist[year],
            sizeof (*dest->Weather.allHist[year])
        );
    }

    /* Allocate memory and copy weather generator parameters */
    SW_MKV_init_ptrs(&dest->Markov);
    if (dest->Weather.generateWeatherMethod == 2) {
        allocateMKV(&dest->Markov, LogInfo);
        if(LogInfo->stopRun) {
            return; // Exit prematurely due to error
        }

        copyMKV(&dest->Markov, &source->Markov);
    }

    /* Allocate memory and copy vegetation establishment parameters */
    SW_VES_init_ptrs(&dest->VegEstab);
    SW_VES_alloc_outptrs(&dest->VegEstab, LogInfo);
    if(LogInfo->stopRun) {
        return; // Exit prematurely due to error
    }

    for(IntU speciesNum = 0; speciesNum < source->VegEstab.count; speciesNum++) {
        _new_species(&dest->VegEstab, LogInfo);
        if(LogInfo->stopRun) {
            return; // Exit prematurely due to error
        }

        memcpy(
            dest->VegEstab.parms[speciesNum],
            source->VegEstab.parms[speciesNum],
            sizeof (*dest->VegEstab.parms[speciesNum])
        );
    }

    SW_VegEstab_alloc_outptrs(&dest->VegEstab, LogInfo);
}


/* =================================================== */
/*             Global Function Definitions             */
/* --------------------------------------------------- */
/**
@brief Calls 'SW_CTL_run_current_year' for each year
          which calls 'SW_SWC_water_flow' for each day.

@param[in,out] sw Comprehensive struct of type SW_ALL containing all
  information in the simulation
@param[in,out] SW_OutputPtrs SW_OUTPUT_POINTERS of size SW_OUTNKEYS which
  hold pointers to subroutines for output keys
@param[in,out] LogInfo Holds information dealing with logfile output
*/

void SW_CTL_main(SW_ALL* sw, SW_OUTPUT_POINTERS* SW_OutputPtrs,
                 LOG_INFO* LogInfo) {
  #ifdef SWDEBUG
  int debug = 0;
  #endif

  TimeInt *cur_yr = &sw->Model.year;

  for (*cur_yr = sw->Model.startyr; *cur_yr <= sw->Model.endyr; (*cur_yr)++) {
    #ifdef SWDEBUG
    if (debug) swprintf("\n'SW_CTL_main': simulate year = %d\n", *cur_yr);
    #endif

    SW_CTL_run_current_year(sw, SW_OutputPtrs, LogInfo);
    if(LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }
  }
} /******* End Main Loop *********/

void SW_CTL_RunSimSet(SW_ALL *sw_template, SW_OUTPUT_POINTERS SW_OutputPtrs[],
                      SW_DOMAIN *SW_Domain, SW_WALLTIME *SW_WallTime, LOG_INFO *main_LogInfo) {

    unsigned long suid, nSims = 0;
    unsigned long ncSuid[2]; // 2 -> [y, x] or [s, 0]
    char tag_suid[32]; /* 32 = 11 character for "(suid = ) " + 20 character for ULONG_MAX + '\0' */
    tag_suid[0] = '\0';
    WallTimeSpec tss, tsr;
    Bool ok_tss = swFALSE, ok_tsr = swFALSE;

    set_walltime(&tss, &ok_tss);

    for(suid = SW_Domain->startSimSet; suid < SW_Domain->endSimSet; suid++)
    {
        /* Check wall time against limit */
        if (
            SW_WallTime->has_walltime &&
            GT(
                diff_walltime(SW_WallTime->timeStart, swTRUE),
                SW_WallTime->wallTimeLimit - SW_WRAPUPTIME
            )
        ) {
            goto wrapUp; // wall time (nearly) exhausted, return early
        }

        LOG_INFO local_LogInfo;
        sw_init_logs(main_LogInfo->logfp, &local_LogInfo);

        SW_DOM_calc_ncSuid(SW_Domain, suid, ncSuid);
        if(SW_DOM_CheckProgress(SW_Domain->DomainType, ncSuid)) {

            nSims++; // Counter of simulation runs

            set_walltime(&tsr, &ok_tsr);
            SW_CTL_run_sw(sw_template, SW_Domain, ncSuid, NULL,
                          SW_OutputPtrs, NULL, &local_LogInfo);
            SW_WT_TimeRun(tsr, ok_tsr, SW_WallTime);

            if(local_LogInfo.stopRun) {
                main_LogInfo->numDomainErrors++; // Counter of simulation units with error

            } else {
                // Set simulation run progress
                SW_DOM_SetProgress(SW_Domain->DomainType, ncSuid);
            }

            if (local_LogInfo.numWarnings > 0) {
                main_LogInfo->numDomainWarnings++;  // Counter of simulation units with warnings
            }

            if (local_LogInfo.stopRun || local_LogInfo.numWarnings > 0) {
                snprintf(tag_suid, 32, "(suid = %lu) ", suid + 1);
                sw_write_warnings(tag_suid, &local_LogInfo);
            }
        }
    }

    if (nSims == main_LogInfo->numDomainErrors) {
        LogError(
            main_LogInfo,
            LOGERROR,
            "All simulated units produced errors."
        );
    }

    wrapUp: {
        SW_WallTime->timeSimSet = diff_walltime(tss, ok_tss);
    }
}

/**
 * @brief Initialize all possible pointers to NULL incase of an unexpected
 *  program exit
 *
 * @param[in,out] sw Comprehensive struct of type SW_ALL containing
 *  all information in the simulation
*/
void SW_CTL_init_ptrs(SW_ALL* sw) {
  SW_WTH_init_ptrs(&sw->Weather);
  SW_MKV_init_ptrs(&sw->Markov);
  SW_VES_init_ptrs(&sw->VegEstab);
  SW_VPD_init_ptrs(&sw->VegProd);
  SW_OUT_init_ptrs(sw);
  SW_SWC_init_ptrs(&sw->SoilWat);
}

/**
 * @brief Allocate dynamic memory for output aggregate and accumulation pointers
 *
 * @param[out] sw Comprehensive struct of type SW_ALL containing
 *  all information in the simulation
 * @param[in,out] LogInfo Holds information dealing with logfile output
*/
void SW_CTL_alloc_outptrs(SW_ALL* sw, LOG_INFO* LogInfo) {
    SW_VPD_alloc_outptrs(&sw->VegProd, LogInfo);
    if(LogInfo->stopRun) {
        return; // Exit prematurely due to error
    }
    SW_SWC_alloc_outptrs(&sw->SoilWat, LogInfo);
    if(LogInfo->stopRun) {
        return; // Exit prematurely due to error
    }
    SW_WTH_alloc_outptrs(&sw->Weather, LogInfo);
}

/**
 * @brief Construct, setup, and obtain inputs for SW_DOMAIN
 *
 * @param[in] userSUID Simulation Unit Identifier requested by the user (base1);
 *            0 indicates that all simulations units within domain are requested
 * @param[out] SW_Domain Struct of type SW_DOMAIN holding constant
 *  temporal/spatial information for a set of simulation runs
 * @param[in,out] LogInfo Holds information dealing with logfile output
*/
void SW_CTL_setup_domain(unsigned long userSUID,
                         SW_DOMAIN* SW_Domain,
                         LOG_INFO* LogInfo) {

    SW_F_construct(
        SW_Domain->PathInfo.InFiles[eFirst],
        SW_Domain->PathInfo._ProjDir,
        LogInfo
    );
    if(LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

    SW_F_read(&SW_Domain->PathInfo, LogInfo);
    if(LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

    #if defined(SWNETCDF)
    SW_NC_read(&SW_Domain->netCDFInfo, &SW_Domain->PathInfo, LogInfo);
    if(LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }
    #endif

    SW_DOM_read(SW_Domain, LogInfo);
    if(LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }
    SW_DOM_calc_nSUIDs(SW_Domain);

    #if defined(SWNETCDF)
    SW_NC_open_files(&SW_Domain->netCDFInfo, LogInfo);
    if(LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }
    if(FileExists(SW_Domain->netCDFInfo.InFilesNC[DOMAIN_NC])) {
        SW_NC_check(SW_Domain, SW_Domain->netCDFInfo.ncFileIDs[DOMAIN_NC],
                    SW_Domain->netCDFInfo.InFilesNC[DOMAIN_NC], LogInfo);
        if(LogInfo->stopRun) {
            return; // Exit function prematurely due to error
        }
    } else {
        SW_NC_create_domain_template(SW_Domain, LogInfo);
        if(LogInfo->stopRun) {
            return; // Exit prematurely due to error
        }

        LogError(LogInfo, LOGERROR, "Domain netCDF template has been created. "
                                    "Please modify it and rename it to "
                                    "'domain.nc' when done and try again. "
                                    "The template path is: %s",
                                    DOMAIN_TEMP);
        return; // Exit prematurely so the user can modify the domain template
    }
    #endif

    SW_DOM_SimSet(SW_Domain, userSUID, LogInfo);
}

/** @brief Setup and construct model (independent of inputs)
 *
 * @param[in,out] sw Comprehensive struct of type SW_ALL containing all
 *  information in the simulation
 * @param[in,out] SW_OutputPtrs SW_OUTPUT_POINTERS of size SW_OUTNKEYS which
 *  hold pointers to subroutines for output keys
 * @param[in,out] LogInfo Holds information dealing with logfile output
 */
void SW_CTL_setup_model(SW_ALL* sw, SW_OUTPUT_POINTERS* SW_OutputPtrs,
                        LOG_INFO* LogInfo) {
	SW_MDL_construct(sw->Model.newperiod, sw->Model.days_in_month);
	SW_WTH_construct(&sw->Weather);

	// delay SW_MKV_construct() until we know from inputs whether we need it
	// SW_SKY_construct() not need
	SW_SIT_construct(&sw->Site);
	SW_VES_construct(&sw->VegEstab);
	SW_VPD_construct(&sw->VegProd);
    if(LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }
	// SW_FLW_construct() not needed
	SW_OUT_construct(sw->FileStatus.make_soil, sw->FileStatus.make_regular,
      SW_OutputPtrs, sw->Output, sw->Site.n_layers, &sw->GenOutput);
	SW_SWC_construct(&sw->SoilWat);
	SW_CBN_construct(&sw->Carbon);
}


/**
  @brief Free allocated memory for an SW_ALL instance

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

	SW_OUT_deconstruct(full_reset, sw);

	SW_MDL_deconstruct();
	SW_WTH_deconstruct(&sw->Weather);
	SW_MKV_deconstruct(&sw->Markov);
	// SW_SKY_deconstruct() not needed
	// SW_SIT_deconstruct() not needed
	SW_VES_deconstruct(&sw->VegEstab);
	SW_VPD_deconstruct(&sw->VegProd);
	// SW_FLW_deconstruct() not needed
	SW_SWC_deconstruct(&sw->SoilWat);
	SW_CBN_deconstruct();
}

/** @brief Initialize simulation run (based on user inputs)
  Note: Time will only be set up correctly while carrying out a
  simulation year, i.e., after calling _begin_year()

  @param[in,out] sw Comprehensive structure holding all information
    dealt with in SOILWAT2
  @param[in,out] LogInfo Holds information dealing with logfile output
*/
void SW_CTL_init_run(SW_ALL* sw, LOG_INFO* LogInfo) {

	// SW_F_init_run() not needed
	// SW_MDL_init_run() not needed
	SW_WTH_init_run(&sw->Weather);
	// SW_MKV_init_run() not needed
	SW_PET_init_run(&sw->AtmDemand);
	// SW_SKY_init_run() not needed
	SW_SIT_init_run(&sw->VegProd, &sw->Site, LogInfo);
    if(LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

	SW_VES_init_run(sw->VegEstab.parms, &sw->Site, sw->Site.n_transp_lyrs,
                  sw->VegEstab.count, LogInfo); // must run after `SW_SIT_init_run()`
    if(LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

	SW_VPD_init_run(&sw->VegProd, &sw->Weather, &sw->Model,
                    LogInfo);
    if(LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

	SW_FLW_init_run(&sw->SoilWat);
	SW_ST_init_run(&sw->StRegValues);
	// SW_OUT_init_run() handled separately so that SW_CTL_init_run() can be
	//   useful for unit tests, rSOILWAT2, and STEPWAT2 applications
	SW_SWC_init_run(&sw->SoilWat, &sw->Site, &sw->Weather.temp_snow);
	SW_CBN_init_run(sw->VegProd.veg, &sw->Model, &sw->Carbon, LogInfo);
}


/**
@brief Calls 'SW_SWC_water_flow' for each day.

@param[in,out] sw Comprehensive struct of type SW_ALL containing
  all information in the simulation
@param[in,out] SW_OutputPtrs SW_OUTPUT_POINTERS of size SW_OUTNKEYS which
  hold pointers to subroutines for output keys
@param[in,out] LogInfo Holds information dealing with logfile output
*/
void SW_CTL_run_current_year(SW_ALL* sw, SW_OUTPUT_POINTERS* SW_OutputPtrs,
                             LOG_INFO* LogInfo) {
  /*=======================================================*/
  TimeInt *doy = &sw->Model.doy; // base1
  #ifdef SWDEBUG
  int debug = 0;
  #endif

  #ifdef SWDEBUG
  if (debug) swprintf("\n'SW_CTL_run_current_year': begin new year\n");
  #endif
  _begin_year(sw, LogInfo);
  if(LogInfo->stopRun) {
    return; // Exit function prematurely due to error
  }

  for (*doy = sw->Model.firstdoy; *doy <= sw->Model.lastdoy; (*doy)++) {
    #ifdef SWDEBUG
    if (debug) swprintf("\t: begin doy = %d ... ", *doy);
    #endif
    _begin_day(sw, LogInfo);
    if(LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

    #ifdef SWDEBUG
    if (debug) swprintf("simulate water ... ");
    #endif
    SW_SWC_water_flow(sw, LogInfo);
    if(LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

    // Only run this function if SWA output is asked for
    if (sw->VegProd.use_SWA) {
      calculate_repartitioned_soilwater(&sw->SoilWat, sw->Site.swcBulk_atSWPcrit,
                                        &sw->VegProd, sw->Site.n_layers);
    }

    if (sw->VegEstab.use) {
      SW_VES_checkestab(sw->VegEstab.parms, &sw->Weather, sw->SoilWat.swcBulk,
                        sw->Model.doy, sw->Model.firstdoy, sw->VegEstab.count);
    }

    #ifdef SWDEBUG
    if (debug) swprintf("ending day ... ");
    #endif
    _end_day(sw, SW_OutputPtrs, LogInfo);
    if(LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

    #ifdef SWDEBUG
    if (debug) swprintf("doy = %d completed.\n", *doy);
    #endif
  }

  #ifdef SWDEBUG
  if (debug) swprintf("'SW_CTL_run_current_year': flush output\n");
  #endif
  SW_OUT_flush(sw, SW_OutputPtrs, LogInfo);

  #ifdef SWDEBUG
  if(LogInfo->stopRun) {
    return; // Exit function prematurely due to error
  }

  if (debug) swprintf("'SW_CTL_run_current_year': completed.\n");
  #endif
}


/**
@brief Reads inputs from disk and makes a print statement if there is an error
        in doing so.

@param[in,out] sw Comprehensive struct of type SW_ALL containing
  all information in the simulation
@param[in,out] PathInfo Struct holding all information about the programs path/files
@param[in,out] LogInfo Holds information dealing with logfile output
*/
void SW_CTL_read_inputs_from_disk(SW_ALL* sw, PATH_INFO* PathInfo,
                                  LOG_INFO* LogInfo) {
  #ifdef SWDEBUG
  int debug = 0;
  #endif

  #ifdef SWDEBUG
  if (debug) swprintf("'SW_CTL_read_inputs_from_disk': Read input from disk:");
  #endif

  SW_MDL_read(&sw->Model, PathInfo->InFiles, LogInfo);
  if(LogInfo->stopRun) {
    return; // Exit function prematurely due to error
  }
  #ifdef SWDEBUG
  if (debug) swprintf(" > 'model'");
  #endif

  SW_WTH_setup(&sw->Weather, PathInfo->InFiles,
               PathInfo->weather_prefix, LogInfo);
  if(LogInfo->stopRun) {
    return; // Exit function prematurely due to error
  }
  #ifdef SWDEBUG
  if (debug) swprintf(" > 'weather setup'");
  #endif

  SW_SKY_read(PathInfo->InFiles, &sw->Sky, LogInfo);
  if(LogInfo->stopRun) {
    return; // Exit function prematurely due to error
  }
  #ifdef SWDEBUG
  if (debug) swprintf(" > 'climate'");
  #endif

  if (sw->Weather.generateWeatherMethod == 2) {
    SW_MKV_setup(&sw->Markov, sw->Weather.rng_seed,
                 sw->Weather.generateWeatherMethod, PathInfo->InFiles,
                 LogInfo);
    if(LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }
    #ifdef SWDEBUG
    if (debug) swprintf(" > 'weather generator'");
    #endif
  }

  SW_WTH_read(&sw->Weather, &sw->Sky, &sw->Model, LogInfo);
  if(LogInfo->stopRun) {
    return; // Exit function prematurely due to error
  }
  #ifdef SWDEBUG
  if (debug) swprintf(" > 'weather read'");
  #endif

  SW_VPD_read(&sw->VegProd, PathInfo->InFiles, LogInfo);
  if(LogInfo->stopRun) {
    return; // Exit function prematurely due to error
  }
  #ifdef SWDEBUG
  if (debug) swprintf(" > 'veg'");
  #endif

  SW_SIT_read(&sw->Site, PathInfo->InFiles, &sw->Carbon, LogInfo);
  if(LogInfo->stopRun) {
    return; // Exit function prematurely due to error
  }
  #ifdef SWDEBUG
  if (debug) swprintf(" > 'site'");
  #endif

  SW_LYR_read(&sw->Site, PathInfo->InFiles, LogInfo);
  if(LogInfo->stopRun) {
    return; // Exit function prematurely due to error
  }
  #ifdef RSWDEBUG
  if (debug) swprintf(" > 'soils'");
  #endif

  SW_SWRC_read(&sw->Site, PathInfo->InFiles, LogInfo);
  if(LogInfo->stopRun) {
    return; // Exit function prematurely due to error
  }
  #ifdef SWDEBUG
  if (debug) swprintf(" > 'swrc parameters'");
  #endif

  SW_VES_read(&sw->VegEstab, PathInfo->InFiles,
              PathInfo->_ProjDir, LogInfo);
  if(LogInfo->stopRun) {
    return; // Exit function prematurely due to error
  }
  #ifdef SWDEBUG
  if (debug) swprintf(" > 'establishment'");
  #endif

  SW_OUT_read(sw, PathInfo->InFiles, sw->GenOutput.timeSteps,
              &sw->GenOutput.used_OUTNPERIODS, LogInfo);
  if(LogInfo->stopRun) {
    return; // Exit function prematurely due to error
  }
  #ifdef SWDEBUG
  if (debug) swprintf(" > 'ouput'");
  #endif

  SW_CBN_read(&sw->Carbon, &sw->Model, PathInfo->InFiles, LogInfo);
  if(LogInfo->stopRun) {
    return; // Exit function prematurely due to error
  }
  #ifdef SWDEBUG
  if (debug) swprintf(" > 'CO2'");
  #endif

  SW_SWC_read(
    &sw->SoilWat,
    sw->Model.endyr,
    PathInfo->InFiles,
    LogInfo
  );
  #ifdef SWDEBUG
  if(LogInfo->stopRun) {
    return; // Exit function prematurely due to error
  }
  if (debug) swprintf(" > 'swc'");
  if (debug) swprintf(" completed.\n");
  #endif
}

/**
 * @brief Do an (independent) model simulation run; Don’t fail/crash
 *  on error but end early and report to caller
 *
 * @param[in] sw_template Template SW_ALL for the function to use as a
 *  reference for local versions of SW_ALL
 * @param[in] SW_Domain Struct of type SW_DOMAIN holding constant
 *  temporal/spatial information for a set of simulation runs
 * @param[in] ncSuid Unique indentifier of the first suid to run
 *  in relation to netCDF gridcells/sites
 * @param[in] ncInFiles Input netCDF files
 * @param[in,out] SW_OutputPtrs SW_OUTPUT_POINTERS of size SW_OUTNKEYS which
 *  hold pointers to subroutines for output keys
 * @param[in,out] p_OUT Data storage for simulation run values
 * @param[in,out] LogInfo Holds information dealing with logfile output
*/
void SW_CTL_run_sw(SW_ALL* sw_template, SW_DOMAIN* SW_Domain, unsigned long ncSuid[],
                   char* ncInFiles[], SW_OUTPUT_POINTERS SW_OutputPtrs[],
                   RealD p_OUT[][SW_OUTNPERIODS], LOG_INFO* LogInfo) {

    SW_ALL local_sw;

    // Copy template SW_ALL to local instance -- yet to be fully implemented
    SW_ALL_deepCopy(sw_template, &local_sw, LogInfo);
    if(LogInfo->stopRun) {
        goto freeMem; // Free memory and skip simulation run
    }

    #if defined(SWNETCDF)
    SW_NC_read_inputs(sw_template, SW_Domain, ncSuid, LogInfo);
    if(LogInfo->stopRun) {
        goto freeMem;
    }
    #endif

    SW_MDL_get_ModelRun(&local_sw.Model, SW_Domain, ncInFiles, LogInfo);
    if(LogInfo->stopRun) {
        goto freeMem; // Free memory and skip simulation run
    }

    SW_CTL_main(&local_sw, SW_OutputPtrs, LogInfo);

    // Clear local instance of SW_ALL
    freeMem: {
        SW_CTL_clear_model(swFALSE, &local_sw);
    }

    (void) ncInFiles;
    (void) ncSuid;
    (void) p_OUT;
}
