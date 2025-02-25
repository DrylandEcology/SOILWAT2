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
 *
 * 02/04/2012	(drs)	in function '_read_inputs()' moved order of
 * 'SW_VPD_read' from after 'SW_VES_read' to before 'SW_SIT_read': SWPcrit is
 * read in in 'SW_VPD_read' and then calculated SWC_atSWPcrit is assigned to
 * each layer in 'SW_SIT_read'
 *
 * 06/24/2013	(rjm)	added call to SW_FLW_construct() in function
 * SW_CTL_init_model()
 */
/********************************************************/
/********************************************************/

/* =================================================== */
/*                INCLUDES / DEFINES                   */
/* --------------------------------------------------- */

#include "include/SW_Control.h"      // for SW_RUN_deepCopy, SW_CTL_RunSimSet
#include "include/filefuncs.h"       // for LogError, sw_message
#include "include/generic.h"         // for swTRUE, Bool, swFALSE, GT, IntU
#include "include/myMemory.h"        // for Mem_Malloc
#include "include/rands.h"           // for RandUniIntRange
#include "include/SW_Carbon.h"       // for SW_CBN_construct, SW_CBN_decons...
#include "include/SW_datastructs.h"  // for SW_RUN, LOG_INFO, SW_OUTPUT_POI...
#include "include/SW_Defines.h"      // for TimeInt, WallTimeSpec, SW_WRAPU...
#include "include/SW_Domain.h"       // for SW_DOM_CheckProgress, SW_DOM_Cr...
#include "include/SW_Files.h"        // for SW_F_construct, SW_F_read, eFirst
#include "include/SW_Flow.h"         // for SW_FLW_init_run
#include "include/SW_Flow_lib.h"     // for SW_ST_init_run
#include "include/SW_Flow_lib_PET.h" // for SW_PET_init_run
#include "include/SW_Main_lib.h"     // for sw_init_logs, sw_write_warnings
#include "include/SW_Markov.h"       // for SW_MKV_init_ptrs, SW_MKV_decons...
#include "include/SW_Model.h"        // for SW_MDL_construct, SW_MDL_decons...
#include "include/SW_Output.h"       // for SW_GENOUT_deepCopy, SW_GENOUT_i...
#include "include/SW_Site.h"         // for SW_LYR_read, SW_SIT_construct
#include "include/SW_Sky.h"          // for SW_SKY_new_year, SW_SKY_read
#include "include/SW_SoilWater.h"    // for SW_SWC_co...
#include "include/SW_VegEstab.h"     // for SW_VES_init_ptrs, SW_VES_alloc_...
#include "include/SW_VegProd.h"      // for SW_VPD_co...
#include "include/SW_Weather.h"      // for SW_WTH_co...
#include "include/Times.h"           // for diff_walltime, set_walltime
#include <signal.h>                  // for signal
#include <stdio.h>                   // for NULL, snprintf
#include <stdlib.h>                  // for free
#include <string.h>                  // for memcpy, NULL


#if defined(SWNETCDF)
#include "include/SW_netCDF_General.h"
#include "include/SW_netCDF_Input.h"
#include "include/SW_netCDF_Output.h"
#include "include/SW_Output_outarray.h"
#endif

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static volatile sig_atomic_t runSims = 1;

/* =================================================== */
/*             Local Function Definitions              */
/* --------------------------------------------------- */

/**
@brief Handle an interrupt provided by the OS to stop the program;
the current supported interrupts are terminations (SIGTERM) and
interrupts (SIGINT, commonly CTRL+C on the keyboard)

@param[in] signal Numerical value of the signal that was recieved
(currently not used)
*/
static void handle_interrupt(int signal) {
    (void) signal; /* Silence compiler */
    runSims = 0;
}

/**
@brief Initiate/update variables for a new simulation year.
      In addition to the timekeeper (Model), usually only modules
      that read input yearly or produce output need to have this call.

@param[in,out] sw Comprehensive struct of type SW_RUN containing all
  information in the simulation
@param[in] OutDom Struct of type SW_OUT_DOM that holds output
    information that do not change throughout simulation runs
@param[out] LogInfo Holds information on warnings and errors
*/
static void begin_year(SW_RUN *sw, SW_OUT_DOM *OutDom, LOG_INFO *LogInfo) {
    // SW_F_new_year() not needed

    // call SW_MDL_new_year() first to set up time-related arrays for this year
    SW_MDL_new_year(&sw->ModelIn, &sw->ModelSim);

    // SW_MKV_new_year() not needed

    // SW_SKY_new_year(): Update daily climate variables from monthly values
    SW_SKY_new_year(
        &sw->ModelSim,
        sw->ModelIn.startyr,
        sw->RunIn.SkyRunIn.snow_density,
        sw->RunIn.SkyRunIn.snow_density_daily
    );

    // SW_SIT_new_year() not needed

    SW_VES_new_year(sw->VegEstabSim.count);

    // SW_VPD_new_year(): Dynamic CO2 effects on vegetation
    SW_VPD_new_year(&sw->ModelSim, sw->RunIn.VegProdRunIn.veg);

    // SW_FLW_new_year() not needed

    SW_SWC_new_year(
        &sw->SoilWatIn,
        &sw->SoilWatSim,
        &sw->SiteSim,
        sw->ModelSim.year,
        sw->SiteIn.reset_yr,
        LogInfo
    );
    if (LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

    // SW_CBN_new_year() not needed
    SW_OUT_new_year(
        sw->ModelSim.firstdoy,
        sw->ModelSim.lastdoy,
        OutDom,
        sw->OutRun.first,
        sw->OutRun.last
    );
}

static void begin_day(SW_RUN *sw, LOG_INFO *LogInfo) {
    SW_MDL_new_day(&sw->ModelSim);
    SW_WTH_new_day(
        &sw->WeatherIn,
        &sw->WeatherSim,
        sw->RunIn.weathRunAllHist,
        &sw->SiteIn,
        sw->SoilWatSim.snowpack,
        sw->ModelSim.doy,
        sw->ModelSim.year,
        LogInfo
    );
}

static void end_day(SW_RUN *sw, SW_OUT_DOM *OutDom, LOG_INFO *LogInfo) {
    int localTOffset = 1; // tOffset is one when called from this function

    if (sw->ModelSim.doOutput) {
        collect_values(sw, OutDom, swFALSE, localTOffset, LogInfo);
        if (LogInfo->stopRun) {
            return; // Exit function prematurely due to error
        }
    }

    SW_SWC_end_day(&sw->SoilWatSim, sw->SiteSim.n_layers);
}

/**
@brief Copy dynamic memory from a template SW_RUN to a new instance

@param[in] source Source struct of type SW_RUN to copy
@param[out] dest Destination struct of type SW_RUN to be copied into
@param[in] OutDom Struct of type SW_OUT_DOM that holds output
    information that do not change throughout simulation runs
@param[in] copyWeatherHist Specifies if the weather data should be copied;
this only has the chance to be false when the program is dealing with
nc inputs
@param[out] LogInfo Holds information on warnings and errors
*/
void SW_RUN_deepCopy(
    SW_RUN *source,
    SW_RUN *dest,
    SW_OUT_DOM *OutDom,
    Bool copyWeatherHist,
    LOG_INFO *LogInfo
) {

    memcpy(dest, source, sizeof(*dest));

    dest->SoilWatIn.hist.file_prefix = NULL; /* currently unused */

    /* Allocate memory and copy daily weather */
    dest->RunIn.weathRunAllHist = NULL;
    if (copyWeatherHist) {
        SW_WTH_allocateAllWeather(
            &dest->RunIn.weathRunAllHist, source->WeatherIn.n_years, LogInfo
        );
        if (LogInfo->stopRun) {
            return; // Exit prematurely due to error
        }
        for (unsigned int year = 0; year < source->WeatherIn.n_years; year++) {
            memcpy(
                &dest->RunIn.weathRunAllHist[year],
                &source->RunIn.weathRunAllHist[year],
                sizeof(dest->RunIn.weathRunAllHist[year])
            );
        }
    }

    /* Copy weather generator parameters */
    if (dest->WeatherIn.generateWeatherMethod == 2) {
        copyMKV(&dest->MarkovIn, &source->MarkovIn);
    }

    /* Copy vegetation establishment parameters */
    SW_VES_init_ptrs(
        &dest->VegEstabIn,
        &dest->VegEstabSim,
        dest->ves_p_accu,
        dest->ves_p_oagg
    );
    if (LogInfo->stopRun) {
        return; // Exit prematurely due to error
    }

    for (IntU speciesNum = 0; speciesNum < source->VegEstabSim.count;
         speciesNum++) {
        new_species(&dest->VegEstabIn, &dest->VegEstabSim, LogInfo);
        if (LogInfo->stopRun) {
            return; // Exit prematurely due to error
        }

        memcpy(
            &dest->VegEstabIn.parms[speciesNum],
            &source->VegEstabIn.parms[speciesNum],
            sizeof(dest->VegEstabIn.parms[speciesNum])
        );

        memcpy(
            &dest->VegEstabSim.parms[speciesNum],
            &source->VegEstabSim.parms[speciesNum],
            sizeof(dest->VegEstabSim.parms[speciesNum])
        );
    }

    SW_VegEstab_alloc_outptrs(
        dest->ves_p_accu, dest->ves_p_oagg, source->VegEstabSim.count, LogInfo
    );
    if (LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

#ifdef SWNETCDF
    SW_PATHOUT_deepCopy(
        &dest->SW_PathOutputs, &source->SW_PathOutputs, OutDom, LogInfo
    );
    if (LogInfo->stopRun) {
        return; // Exit prematurely due to error
    }
    SW_OUT_construct_outarray(OutDom, &dest->OutRun, LogInfo);
#else
    (void) OutDom;
#endif
}

/* =================================================== */
/*             Global Function Definitions             */
/* --------------------------------------------------- */
/**
@brief Calls 'SW_CTL_run_current_year' for each year
          which calls 'SW_SWC_water_flow' for each day.

@param[in,out] sw Comprehensive struct of type SW_RUN containing all
  information in the simulation
@param[in,out] OutDom Struct of type SW_OUT_DOM that holds output
    information that do not change throughout simulation runs
@param[out] LogInfo Holds information on warnings and errors
*/

void SW_CTL_main(SW_RUN *sw, SW_OUT_DOM *OutDom, LOG_INFO *LogInfo) {
#ifdef SWDEBUG
    int debug = 0;
#endif

    TimeInt *cur_yr = &sw->ModelSim.year;

    for (*cur_yr = sw->ModelIn.startyr; *cur_yr <= sw->ModelIn.endyr;
         (*cur_yr)++) {
#ifdef SWDEBUG
        if (debug) {
            sw_printf("\n'SW_CTL_main': simulate year = %d\n", *cur_yr);
        }
#endif

        SW_CTL_run_current_year(sw, OutDom, LogInfo);
        fprintf(stderr, "After current year\n");
        if (LogInfo->stopRun) {
            return; // Exit function prematurely due to error
        }
    }
} /******* End Main Loop *********/

void SW_CTL_RunSimSet(
    SW_RUN *sw_template,
    SW_DOMAIN *SW_Domain,
    SW_WALLTIME *SW_WallTime,
    LOG_INFO *main_LogInfo
) {

    unsigned long suid;
    unsigned long nSims = 0;
    unsigned long ncSuid[2]; // 2 -> [y, x] or [s, 0]
    /* tag_suid is 32:
      11 character for "(suid = ) " + 20 character for ULONG_MAX + '\0' */
    char tag_suid[32];

    tag_suid[0] = '\0';
    WallTimeSpec tss;
    WallTimeSpec tsr;
    Bool ok_tss = swFALSE;
    Bool ok_tsr = swFALSE;
    Bool ok_suid;
    unsigned long startSim = SW_Domain->startSimSet;
    unsigned long endSim = SW_Domain->endSimSet;

    int progFileID = 0; // Value does not matter if SWNETCDF is not defined
    int progVarID = 0;  // Value does not matter if SWNETCDF is not defined

#if defined(SWNETCDF)
    progFileID = SW_Domain->SW_PathInputs.ncDomFileIDs[vNCprog];
    progVarID = SW_Domain->netCDFInput.ncDomVarIDs[vNCprog];
#endif

    set_walltime(&tss, &ok_tss);

#if defined(SOILWAT)
    if (main_LogInfo->printProgressMsg) {
        sw_message("is running simulations across the domain ...");
    }
#endif

    /* Set up interrupt handlers so if the program is interrupted
       during simulation, we can exit smoothly and not abruptly */
    (void) signal(SIGINT, handle_interrupt);
    (void) signal(SIGTERM, handle_interrupt);

    /* Loop over suids in simulation set of domain */
    for (suid = startSim; suid < endSim && runSims; suid++) {
        /* Check wall time against limit */
        if (SW_WallTime->has_walltime &&
            GT(diff_walltime(SW_WallTime->timeStart, swTRUE),
               SW_WallTime->wallTimeLimit - SW_WRAPUPTIME)) {
            goto wrapUp; // wall time (nearly) exhausted, return early
        }

        LOG_INFO local_LogInfo;
        sw_init_logs(main_LogInfo->logfp, &local_LogInfo);
        local_LogInfo.printProgressMsg = main_LogInfo->printProgressMsg;


        /* Check if suid needs to be simulated */
        SW_DOM_calc_ncSuid(SW_Domain, suid, ncSuid);
        fprintf(stderr, "After ncSuid calculation\n");

        ok_suid =
            SW_DOM_CheckProgress(progFileID, progVarID, ncSuid, &local_LogInfo);
        fprintf(stderr, "After check progress\n");

        if (ok_suid && !local_LogInfo.stopRun && runSims) {
            /* Count simulation run */
            nSims++;

            /* Simulate suid */
            set_walltime(&tsr, &ok_tsr);
            fprintf(stderr, "After set walltime\n");
            SW_CTL_run_sw(sw_template, SW_Domain, ncSuid, &local_LogInfo);
            fprintf(stderr, "After run sw\n");
            SW_WT_TimeRun(tsr, ok_tsr, SW_WallTime);
            fprintf(stderr, "After get wall time\n");

            /* Report progress for suid */
            SW_DOM_SetProgress(
                local_LogInfo.stopRun,
                SW_Domain->DomainType,
                progFileID,
                progVarID,
                ncSuid,
                &local_LogInfo
            );
            fprintf(stderr, "After set progress\n");
        }

        /* Report errors and warnings for suid */
        if (local_LogInfo.stopRun) {
            // Counter of simulation units with error
            main_LogInfo->numDomainErrors++;
        }

        if (local_LogInfo.numWarnings > 0) {
            // Counter of simulation units with warnings
            main_LogInfo->numDomainWarnings++;
        }

        if (local_LogInfo.stopRun || local_LogInfo.numWarnings > 0) {
            (void) snprintf(tag_suid, 32, "(suid = %lu) ", suid + 1);
            sw_write_warnings(tag_suid, &local_LogInfo);
        }
    }

    /* Produce global error if all suids failed */
    if (nSims > 0 && nSims == main_LogInfo->numDomainErrors) {
        LogError(
            main_LogInfo,
            LOGERROR,
            "All simulated units (n = %zu) produced errors.",
            nSims
        );
    }

wrapUp:
#if defined(SOILWAT)
    if (runSims == 0) {
        sw_message("Program was killed early. Shutting down...");
    }
#endif

    SW_WallTime->timeSimSet = diff_walltime(tss, ok_tss);
}

/**
@brief Initialize all possible pointers to NULL incase of an unexpected
program exit

@param[in,out] sw Comprehensive struct of type SW_RUN containing
    all information in the simulation
*/
void SW_CTL_init_ptrs(SW_RUN *sw) {
    SW_WTH_init_ptrs(&sw->RunIn.weathRunAllHist);
    // SW_MKV_init_ptrs() not needed
    SW_VES_init_ptrs(
        &sw->VegEstabIn, &sw->VegEstabSim, sw->ves_p_accu, sw->ves_p_oagg
    );
    // SW_VPD_init_ptrs() not needed
    SW_OUT_init_ptrs(&sw->OutRun, &sw->SW_PathOutputs);
    SW_SWC_init_ptrs(&sw->SoilWatIn, &sw->SoilWatSim);
}

/**
@brief Construct, setup, and obtain inputs for SW_DOMAIN

@param[in] userSUID Simulation Unit Identifier requested by the user (base1);
    0 indicates that all simulations units within domain are requested
@param[in] renameDomainTemp Specifies if the created domain netCDF file
will automatically be renamed
@param[out] SW_Domain Struct of type SW_DOMAIN holding constant
    temporal/spatial information for a set of simulation runs
@param[out] LogInfo Holds information on warnings and errors
*/
void SW_CTL_setup_domain(
    unsigned long userSUID,
    Bool renameDomainTemp,
    SW_DOMAIN *SW_Domain,
    LOG_INFO *LogInfo
) {

    SW_F_construct(&SW_Domain->SW_PathInputs, LogInfo);
    if (LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

    SW_F_read(&SW_Domain->SW_PathInputs, LogInfo);
    if (LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

    SW_DOM_construct(SW_Domain->SW_SpinUp.rng_seed, SW_Domain);

    SW_DOM_read(SW_Domain, LogInfo);
    if (LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

    SW_DOM_calc_nSUIDs(SW_Domain);

#if defined(SWNETCDF)
    SW_NC_read(
        &SW_Domain->netCDFInput,
        &SW_Domain->OutDom.netCDFOutput,
        &SW_Domain->SW_PathInputs,
        SW_Domain->startyr,
        SW_Domain->endyr,
        LogInfo
    );
    if (LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

    SW_NCIN_create_units_converters(&SW_Domain->netCDFInput, LogInfo);
    if (LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

    // Create domain template if it does not exist (and exit)
    char *fnameDomainTemplateNC;

    fnameDomainTemplateNC =
        (renameDomainTemp) ?
            SW_Domain->SW_PathInputs.ncInFiles[eSW_InDomain][vNCdom] :
            NULL;

    if (!FileExists(SW_Domain->SW_PathInputs.ncInFiles[eSW_InDomain][vNCdom])) {
        SW_NCIN_create_domain_template(
            SW_Domain, fnameDomainTemplateNC, LogInfo
        );
        if (LogInfo->stopRun) {
            return; // Exit prematurely due to error
        }

        if (!renameDomainTemp) {
            LogError(
                LogInfo,
                LOGERROR,
                "Domain netCDF template has been created. "
                "Please modify it and rename it to "
                "'domain.nc' when done and try again. "
                "The template path is: %s",
                DOMAIN_TEMP
            );
            return; // Exit prematurely: user modifies the domain template
        }
    }

    // Open necessary netCDF input files and check for consistency with domain
    SW_NCIN_open_dom_prog_files(
        &SW_Domain->netCDFInput, &SW_Domain->SW_PathInputs, LogInfo
    );
    if (LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

    SW_NC_check(
        SW_Domain,
        SW_Domain->SW_PathInputs.ncDomFileIDs[vNCdom],
        SW_Domain->SW_PathInputs.ncInFiles[eSW_InDomain][vNCdom],
        LogInfo
    );
    if (LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }
#else
    (void) renameDomainTemp;
#endif

    SW_DOM_CreateProgress(SW_Domain, LogInfo);
    if (LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

    SW_DOM_SimSet(SW_Domain, userSUID, LogInfo);
}

/**
@brief Setup and construct model (independent of inputs)

@param[in,out] sw Comprehensive struct of type SW_RUN containing all
    information in the simulation
@param[in,out] OutDom Struct of type SW_OUT_DOM that holds output
    information that do not change throughout simulation runs
@param[in] zeroOutInfo Specifies if SW_OUT_RUN should be zeroed-out
@param[out] LogInfo Holds information on warnings and errors
*/
void SW_CTL_setup_model(
    SW_RUN *sw, SW_OUT_DOM *OutDom, Bool zeroOutInfo, LOG_INFO *LogInfo
) {
    SW_MDL_construct(&sw->ModelSim);
    SW_WTH_construct(
        &sw->WeatherIn, &sw->WeatherSim, sw->weath_p_accu, sw->weath_p_oagg
    );

    // delay SW_MKV_construct() until we know from inputs whether we need it
    // SW_SKY_construct() not need
    SW_SIT_construct(&sw->SiteIn, &sw->SiteSim);
    SW_VES_construct(
        &sw->VegEstabIn, &sw->VegEstabSim, sw->ves_p_oagg, sw->ves_p_accu
    );
    SW_VPD_construct(
        &sw->VegProdIn, &sw->RunIn.VegProdRunIn, sw->vp_p_oagg, sw->vp_p_accu
    );
    // SW_FLW_construct() not needed
    SW_OUT_construct(
        zeroOutInfo, &sw->SW_PathOutputs, OutDom, &sw->OutRun, LogInfo
    );
    if (LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }
    SW_SWC_construct(
        &sw->SoilWatIn, &sw->SoilWatSim, sw->sw_p_accu, sw->sw_p_oagg
    );
    SW_CBN_construct(&sw->CarbonIn);
}

/**
@brief Free allocated memory for an SW_RUN instance

@param full_reset
    * If `FALSE`, de-allocate memory for `SOILWAT2` variables, but
        * do not reset output arrays `p_OUT` and `p_OUTsd` which are used under
          `SW_OUTARRAY` to pass output in-memory to `rSOILWAT2` and to
          `STEPWAT2`
    * if `TRUE`, de-allocate all memory including output arrays.
@param[in,out] sw Comprehensive structure holding all information
    dealt with in SOILWAT2
*/
void SW_CTL_clear_model(Bool full_reset, SW_RUN *sw) {

    SW_OUT_deconstruct(full_reset, sw);

    SW_MDL_deconstruct();
    SW_WTH_deconstruct(&sw->RunIn.weathRunAllHist);
    // SW_MKV_deconstruct() not needed
    // SW_SKY_INPUTS_deconstruct() not needed
    // SW_SIT_deconstruct() not needed
    SW_VES_deconstruct(
        &sw->VegEstabIn, &sw->VegEstabSim, sw->ves_p_accu, sw->ves_p_oagg
    );
    // SW_VPD_deconstruct() not needed
    // SW_FLW_deconstruct() not needed
    SW_SWC_deconstruct(&sw->SoilWatIn, &sw->SoilWatSim);
    SW_CBN_deconstruct();
}

/**
@brief Initialize simulation run (based on user inputs)

Note: Time will only be set up correctly while carrying out a simulation year,
i.e., after calling begin_year()

@param[in,out] sw Comprehensive structure holding all information
    dealt with in SOILWAT2
@param[in] estVeg Flag specifying if the vegetation should be
estimated
@param[out] LogInfo Holds information on warnings and errors
*/
void SW_CTL_init_run(SW_RUN *sw, Bool estVeg, LOG_INFO *LogInfo) {

    // SW_F_init_run() not needed
    // SW_MDL_init_run() not needed
    SW_WTH_init_run(&sw->WeatherSim);
    // SW_MKV_init_run() not needed
    SW_PET_init_run(&sw->AtmDemSim);

    SW_SKY_init_run(&sw->RunIn.SkyRunIn, LogInfo);
    if (LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

    SW_SIT_init_run(
        &sw->VegProdIn,
        &sw->SiteIn,
        &sw->SiteSim,
        &sw->RunIn.SoilRunIn,
        sw->RunIn.VegProdRunIn.veg,
        LogInfo
    );
    if (LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

    // SW_VES_init_run() must be called after `SW_SIT_init_run()`
    SW_VES_init_run(
        sw->VegEstabIn.parms,
        &sw->RunIn.SoilRunIn,
        &sw->SiteSim,
        sw->SiteSim.n_transp_lyrs,
        sw->VegEstabSim.count,
        LogInfo
    );
    if (LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

    SW_VPD_init_run(
        &sw->RunIn.VegProdRunIn,
        sw->RunIn.weathRunAllHist,
        &sw->ModelIn,
        &sw->ModelSim,
        estVeg,
        sw->RunIn.ModelRunIn.isnorth,
        sw->VegProdIn.veg_method,
        LogInfo
    );
    if (LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

    SW_FLW_init_run(&sw->SoilWatSim);
    SW_ST_init_run(&sw->StRegSimVals);
    // SW_OUT_init_run() handled separately so that SW_CTL_init_run() can be
    //   useful for unit tests, rSOILWAT2, and STEPWAT2 applications
    SW_SWC_init_run(&sw->SoilWatSim, &sw->SiteSim, &sw->WeatherSim.temp_snow);
    SW_CBN_init_run(
        sw->RunIn.VegProdRunIn.veg,
        &sw->CarbonIn,
        sw->ModelSim.addtl_yr,
        sw->ModelIn.startyr,
        sw->ModelIn.endyr,
        LogInfo
    );
}

/**
@brief Calls 'SW_SWC_water_flow' for each day.

@param[in,out] sw Comprehensive struct of type SW_RUN containing
  all information in the simulation
@param[in,out] OutDom Struct of type SW_OUT_DOM that holds output
    information that do not change throughout simulation runs
@param[out] LogInfo Holds information on warnings and errors
*/
void SW_CTL_run_current_year(
    SW_RUN *sw, SW_OUT_DOM *OutDom, LOG_INFO *LogInfo
) {
    /*=======================================================*/
#ifdef SWDEBUG
    int debug = 0;
#endif

    TimeInt *doy = &sw->ModelSim.doy; // base1

#ifdef SWDEBUG
    if (debug) {
        sw_printf("\n'SW_CTL_run_current_year': begin new year\n");
    }
#endif

    begin_year(sw, OutDom, LogInfo);
    if (LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

    for (*doy = sw->ModelSim.firstdoy; *doy <= sw->ModelSim.lastdoy; (*doy)++) {
        #ifdef SWDEBUG
        if (debug) {
            sw_printf("\t: begin doy = %d ... ", *doy);
        }
        #endif
        begin_day(sw, LogInfo);
        if (LogInfo->stopRun) {
            return; // Exit function prematurely due to error
        }

#ifdef SWDEBUG
        if (debug) {
            sw_printf("simulate water ... ");
        }
#endif
        SW_SWC_water_flow(sw, LogInfo);
        if (LogInfo->stopRun) {
            return; // Exit function prematurely due to error
        }

        // Only run this function if SWA output is asked for
        if (sw->VegProdIn.use_SWA) {
            calculate_repartitioned_soilwater(
                &sw->SoilWatSim,
                sw->SiteSim.swcBulk_atSWPcrit,
                &sw->VegProdIn,
                sw->RunIn.VegProdRunIn.veg,
                sw->SiteSim.n_layers
            );
        }

        if (sw->VegEstabSim.use) {
            SW_VES_checkestab(
                sw->VegEstabIn.parms,
                sw->VegEstabSim.parms,
                sw->WeatherSim.temp_avg,
                sw->SoilWatSim.swcBulk,
                sw->ModelSim.doy,
                sw->ModelSim.firstdoy,
                sw->VegEstabSim.count
            );
        }

#ifdef SWDEBUG
        if (debug) {
            sw_printf("ending day ... ");
        }
#endif
        end_day(sw, OutDom, LogInfo);
        if (LogInfo->stopRun) {
            return; // Exit function prematurely due to error
        }

#ifdef SWDEBUG
        if (debug) {
            sw_printf("doy = %d completed.\n", *doy);
        }
#endif
    }

#ifdef SWDEBUG
    if (debug) {
        sw_printf("'SW_CTL_run_current_year': flush output\n");
    }
#endif
    if (sw->ModelSim.doOutput) {
        SW_OUT_flush(sw, OutDom, LogInfo);
    }

#ifdef SWDEBUG
    if (LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

    if (debug) {
        sw_printf("'SW_CTL_run_current_year': completed.\n");
    }
#endif
}

/**
@brief Run a spin-up

  Calls 'SW_CTL_run_current_year' over an array of simulated years
          as specified by the given spinup scope and duration which
          then calls 'SW_SWC_water_flow' for each day.

  No output is produced during the spin-up; state variables including
  soil moisture and soil temperature are updated and handed off to the
  simulation run.

  A spin-up duration of 0 returns immediately (no spin-up).

@param[in,out] sw Comprehensive struct of type SW_RUN containing all
  information in the simulation
@param[in] OutDom Struct of type SW_OUT_DOM that holds output
    information that do not change throughout simulation runs
@param[out] LogInfo Holds information dealing with logfile output
*/
void SW_CTL_run_spinup(SW_RUN *sw, SW_OUT_DOM *OutDom, LOG_INFO *LogInfo) {

    if (sw->ModelIn.SW_SpinUp.duration <= 0) {
        return;
    }

#ifdef SWDEBUG
    int debug = 0;
#endif

    unsigned int i;
    unsigned int k;
    unsigned int quotient = 0;
    unsigned int remainder = 0;
    int mode = sw->ModelIn.SW_SpinUp.mode;
    TimeInt yr;
    TimeInt duration = sw->ModelIn.SW_SpinUp.duration;
    TimeInt scope = sw->ModelIn.SW_SpinUp.scope;
    TimeInt finalyr = sw->ModelIn.startyr + scope - 1;
    TimeInt *years;
    Bool prev_doOut = sw->ModelSim.doOutput;
    years = (TimeInt *) Mem_Malloc(
        sizeof(TimeInt) * duration, "SW_CTL_run_spinup()", LogInfo
    );
    if (LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

#ifdef SWDEBUG
    if (debug) {
        sw_printf(
            "'SW_CTL_run_spinup': "
            "mode = %d, duration = %d (# years), "
            "scope = %d [# calendar years out of %d-%d]\n",
            mode,
            duration,
            scope,
            sw->ModelIn.startyr,
            finalyr
        );
    }
#endif

    switch (mode) {
    case 2:
        // initialize structured array
        if (duration <= scope) {
            // 1:m
            yr = sw->ModelIn.startyr;
            for (i = 0; i < duration; i++) {
                years[i] = yr + i;
            }
        } else {
            // { {1:n}_(m//n), 1:(m%n) }
            quotient = duration / scope;
            remainder = duration % scope;
            yr = sw->ModelIn.startyr;
            for (i = 0; i < quotient * scope; i++) {
                years[i] = yr + (i % scope);
            }
            for (i = 0; i < remainder; i++) {
                k = i + (scope * quotient);
                years[k] = yr + i;
            }
        }

        break;
    default: // same as case 1
        // initialize random array
        for (i = 0; i < duration; i++) {
            yr = RandUniIntRange(
                sw->ModelIn.startyr, finalyr, &sw->ModelIn.SW_SpinUp.spinup_rng
            );
            years[i] = yr;
        }
        break;
    }

    TimeInt *cur_yr = &sw->ModelSim.year;
    TimeInt yrIdx;
    TimeInt startyr = sw->ModelIn.startyr;

    sw->ModelIn.startyr = years[0]; // set startyr for spinup

    sw->ModelSim.doOutput = swFALSE; // turn output temporarily off

    for (yrIdx = 0; yrIdx < duration; yrIdx++) {
        *cur_yr = years[yrIdx];

#ifdef SWDEBUG
        if (debug) {
            sw_printf(
                "'SW_CTL_run_spinup': simulate year = %d | %d\n",
                yrIdx + 1,
                *cur_yr
            );
        }
#endif

        SW_CTL_run_current_year(sw, OutDom, LogInfo);
        if (LogInfo->stopRun) {
            goto reSet; // Exit function prematurely due to error
        }
    }

reSet: {
    sw->ModelIn.startyr = startyr;      // reset startyr to original value
    sw->ModelSim.doOutput = prev_doOut; // reset doOutput to original value

    free(years);
}
}

/**
@brief Reads inputs from disk and makes a print statement if there is an error
        in doing so.

@param[in,out] sw Comprehensive struct of type SW_RUN containing
all information in the simulation
@param[in,out] SW_Domain Struct of type SW_DOMAIN holding constant
temporal/spatial information for a set of simulation runs
@param[out] hasConsistentSoilLayerDepths Holds the specification if the
input soil layers have the same depth throughout all inputs (only used
when dealing with nc inputs)
@param[out] LogInfo Holds information on warnings and errors
*/
void SW_CTL_read_inputs_from_disk(
    SW_RUN *sw,
    SW_DOMAIN *SW_Domain,
    Bool *hasConsistentSoilLayerDepths,
    LOG_INFO *LogInfo
) {
    SW_PATH_INPUTS *SW_PathInputs = &SW_Domain->SW_PathInputs;
#ifdef SWDEBUG
    int debug = 0;
#endif
    Bool readTextInputs = swTRUE;
#if defined(SWNETCDF)
    readTextInputs =
        (Bool) !SW_Domain->netCDFInput.readInVars[eSW_InWeather][0];
#endif

#ifdef SWDEBUG
    if (debug) {
        sw_printf("'SW_CTL_read_inputs_from_disk': Read input from disk:");
    }
#endif

    SW_MDL_read(&sw->RunIn.ModelRunIn, SW_PathInputs->txtInFiles, LogInfo);
    if (LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }
#ifdef SWDEBUG
    if (debug) {
        sw_printf(" > 'model'");
    }
#endif

    SW_WTH_setup(
        &sw->WeatherIn,
        SW_PathInputs->txtInFiles,
        SW_PathInputs->txtWeatherPrefix,
        LogInfo
    );
    if (LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

#ifdef SWDEBUG
    if (debug) {
        sw_printf(" > 'weather setup'");
    }
#endif
    SW_SKY_read(SW_PathInputs->txtInFiles, &sw->RunIn.SkyRunIn, LogInfo);
    if (LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }
#ifdef SWDEBUG
    if (debug) {
        sw_printf(" > 'climate'");
    }
#endif

    if (sw->WeatherIn.generateWeatherMethod == 2) {
        SW_MKV_setup(
            &sw->MarkovIn,
            sw->WeatherIn.rng_seed,
            sw->WeatherIn.generateWeatherMethod,
            SW_PathInputs->txtInFiles,
            LogInfo
        );
        if (LogInfo->stopRun) {
            return; // Exit function prematurely due to error
        }
#ifdef SWDEBUG
        if (debug) {
            sw_printf(" > 'weather generator'");
        }
#endif
    }

    SW_WTH_read(
        &sw->WeatherIn,
        &sw->RunIn.weathRunAllHist,
        &sw->RunIn.SkyRunIn,
        &sw->ModelIn,
        sw->RunIn.ModelRunIn.elevation,
        readTextInputs,
        sw->ModelSim.cum_monthdays,
        sw->ModelSim.days_in_month,
        LogInfo
    );
    if (LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

#ifdef SWDEBUG
    if (debug) {
        sw_printf(" > 'weather read'");
    }
#endif

    SW_VPD_read(
        &sw->VegProdIn,
        &sw->RunIn.VegProdRunIn,
        SW_PathInputs->txtInFiles,
        LogInfo
    );
    if (LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }
#ifdef SWDEBUG
    if (debug) {
        sw_printf(" > 'veg'");
    }
#endif

    SW_SIT_read(
        &sw->SiteIn,
        &sw->SiteSim,
        SW_PathInputs->txtInFiles,
        &sw->CarbonIn,
        hasConsistentSoilLayerDepths,
        &sw->RunIn.SiteRunIn.Tsoil_constant,
        LogInfo
    );
    if (LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }
#ifdef SWDEBUG
    if (debug) {
        sw_printf(" > 'site'");
    }
#endif

    SW_LYR_read(
        &sw->RunIn.SoilRunIn,
        &sw->SiteSim.n_evap_lyrs,
        &sw->SiteSim.n_layers,
        SW_PathInputs->txtInFiles,
        LogInfo
    );
    if (LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }
#ifdef RSWDEBUG
    if (debug) {
        sw_printf(" > 'soils'");
    }
#endif

    SW_SWRC_read(
        &sw->SiteSim,
        SW_PathInputs->txtInFiles,
        sw->SiteIn.inputsProvideSWRCp,
        sw->RunIn.SoilRunIn.swrcpMineralSoil,
        LogInfo
    );
    if (LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }
#ifdef SWDEBUG
    if (debug) {
        sw_printf(" > 'swrc parameters'");
    }
#endif

    SW_VES_read(
        &sw->VegEstabIn,
        &sw->VegEstabSim,
        sw->ves_p_accu,
        sw->ves_p_oagg,
        SW_PathInputs->txtInFiles,
        SW_PathInputs->SW_ProjDir,
        LogInfo
    );
    if (LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }
#ifdef SWDEBUG
    if (debug) {
        sw_printf(" > 'establishment'");
    }
#endif

    SW_OUT_read(
        sw,
        &SW_Domain->OutDom,
        SW_PathInputs->txtInFiles,
        SW_PathInputs->outputPrefix,
        LogInfo
    );
    if (LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }
#ifdef SWDEBUG
    if (debug) {
        sw_printf(" > 'ouput'");
    }
#endif

    SW_CBN_read(
        &sw->CarbonIn,
        sw->ModelSim.addtl_yr,
        sw->ModelIn.startyr,
        sw->ModelIn.endyr,
        SW_PathInputs->txtInFiles,
        LogInfo
    );
    if (LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }
#ifdef SWDEBUG
    if (debug) {
        sw_printf(" > 'CO2'");
    }
#endif

    SW_SWC_read(
        &sw->SoilWatIn, sw->ModelIn.endyr, SW_PathInputs->txtInFiles, LogInfo
    );
#ifdef SWDEBUG
    if (LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }
    if (debug) {
        sw_printf(" > 'swc'");
    }
    if (debug) {
        sw_printf(" completed.\n");
    }
#endif
}

/**
@brief Do an (independent) model simulation run; Don’t fail/crash
on error but end early and report to caller

@param[in] sw_template Template SW_RUN for the function to use as a
    reference for local versions of SW_RUN
@param[in] SW_Domain Struct of type SW_DOMAIN holding constant
    temporal/spatial information for a set of simulation runs
@param[in] ncSuid Unique indentifier of the first suid to run
    in relation to netCDF gridcells/sites
@param[out] LogInfo Holds information on warnings and errors
*/
void SW_CTL_run_sw(
    SW_RUN *sw_template,
    SW_DOMAIN *SW_Domain,
    unsigned long ncSuid[], // NOLINT(readability-non-const-parameter)
    LOG_INFO *LogInfo
) {

#ifdef SWDEBUG
    int debug = 0;
#endif

    SW_RUN local_sw;
    Bool copyWeather = swTRUE;
    Bool estVeg = swTRUE;

#if defined(SWNETCDF)
    copyWeather = (Bool) !SW_Domain->netCDFInput.readInVars[eSW_InWeather][0];
    estVeg = (Bool) (!SW_Domain->netCDFInput.readInVars[eSW_InWeather][0]);
#endif

#ifdef SWDEBUG
    if (debug) {
        sw_printf("SW_CTL_run_sw(): suid = %zu/%zu", ncSuid[0], ncSuid[1]);
    }
#endif

    // Copy template SW_RUN to local instance
    SW_RUN_deepCopy(
        sw_template, &local_sw, &SW_Domain->OutDom, copyWeather, LogInfo
    );
    if (LogInfo->stopRun) {
        goto freeMem; // Free memory and skip simulation run
    }

#if defined(SWNETCDF)
    // Obtain suid-specific inputs
    SW_NCIN_read_inputs(&local_sw, SW_Domain, ncSuid, LogInfo);
    if (LogInfo->stopRun) {
        goto freeMem;
    }
#endif

#ifdef SWDEBUG
    if (debug) {
        sw_printf(
            " -- inputs at lon/lat = (%f, %f)",
            local_sw.RunIn.ModelRunIn.longitude * rad_to_deg,
            local_sw.RunIn.ModelRunIn.latitude * rad_to_deg
        );
    }
#endif

    // Initialize run-time variables
    SW_CTL_init_run(&local_sw, estVeg, LogInfo);
    if (LogInfo->stopRun) {
        goto freeMem; // Exit function prematurely due to error
    }

    // Run spinup for suid
    if (SW_Domain->SW_SpinUp.spinup) {
#ifdef SWDEBUG
        if (debug) {
            sw_printf(" -- spinup");
        }
#endif
        SW_CTL_run_spinup(&local_sw, &SW_Domain->OutDom, LogInfo);
        if (LogInfo->stopRun) {
            goto freeMem; // Exit function prematurely due to error
        }
    }

    // Run simulation for suid
#ifdef SWDEBUG
    if (debug) {
        sw_printf(" -- run");
    }
#endif
    SW_CTL_main(&local_sw, &SW_Domain->OutDom, LogInfo);
    if (LogInfo->stopRun) {
        goto freeMem; // Free memory and exit function prematurely due to error
    }

#if defined(SWNETCDF)
#ifdef SWDEBUG
    if (debug) {
        sw_printf(" -- nc-output");
    }
#endif
    SW_NCOUT_write_output(
        &SW_Domain->OutDom,
        local_sw.OutRun.p_OUT,
        local_sw.SW_PathOutputs.numOutFiles,
        local_sw.SW_PathOutputs.ncOutFiles,
        ncSuid,
        SW_Domain->DomainType,
        LogInfo
    );
#endif

// Clear local instance of SW_RUN
freeMem:
#ifdef SWDEBUG
    if (debug) {
        sw_printf(" -- end.\n");
    }
#endif
    SW_CTL_clear_model(swTRUE, &local_sw);

    (void) SW_Domain;
    (void) ncSuid;
}
