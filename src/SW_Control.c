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
#include "include/filefuncs.h"       // for LogError, sw_message via SW_MSG_ROOT
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
#include <signal.h>                  // for signal, SIGINT, SIGTERM
#include <stdio.h>                   // for NULL, snprintf
#include <stdlib.h>                  // for free
#include <string.h>                  // for memcpy, NULL


#if defined(SWNETCDF)
#include "include/SW_netCDF_General.h"
#include "include/SW_netCDF_Input.h"
#include "include/SW_netCDF_Output.h"
#include "include/SW_Output_outarray.h"
#if defined(SWMPI)
#include "include/SW_MPI.h" // for SW_MPI_setup_fail, SW_MPI_Bcast
#include <mpi.h>            // for MPI_COMM_WORLD, MPI_Datatype
#endif
#endif

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
volatile sig_atomic_t runSims = 1;

/* =================================================== */
/*             Local Function Definitions              */
/* --------------------------------------------------- */

/**
@brief Initialize all simulation run log (LOG_INFO) instances

@param[in] nActiveSites Number of active sites to initialize log
information for
@param[in] logfp The log file pointer used by the programs main instance
of LOG_INFO
@param[out] LogInfos A list of LOG_INFO instances of size "nActiveSites" that
will be returned with all instances initialized
*/
static void init_all_logs(
    size_t nActiveSites, FILE *logfp, LOG_INFO *LogInfos
) {
    size_t site;

    for (site = 0; site < nActiveSites; site++) {
        sw_init_logs(logfp, &LogInfos[site]);
    }
}

/**
@brief Initialize all site simulation information

@param[in] copyWeatherHist Specifies if the weather data should be copied;
this only has the chance to be false when the program is dealing with
nc inputs
@param[in] nActiveSites Number of active sites to initialize log
information for
@param[in] SW_Domain Struct of type SW_DOMAIN holding constant
temporal/spatial information for a set of simulation runs
@param[in] sw_template Template SW_RUN for the function to use as a
reference for local versions of SW_RUN
@param[out] SW_Runs A list of SW_RUN instances of size "nActiveSites" that
will be returned with deep copied information from the main SW_RUN
and initialized runs
@param[out] main_LogInfo The main LOG_INFO instance for the program
*/
static void init_all_runs(
    Bool copyWeatherHist,
    size_t nActiveSites,
    SW_DOMAIN *SW_Domain,
    SW_RUN *sw_template,
    SW_RUN *SW_Runs,
    LOG_INFO *main_LogInfo
) {
    size_t site;

#if defined(SWNETCDF)
    Bool sDom = SW_Domain->netCDFInput.siteDoms[eSW_InDomain];
    size_t nSites = SW_Domain->domCounts[eSW_InDomain][0];

    nSites *= (sDom ? 1 : SW_Domain->domCounts[eSW_InDomain][1]);

    SW_OUT_construct_outarray(
        nSites, &SW_Domain->OutDom, sw_template->OutRun, main_LogInfo
    );
#else
    (void) SW_Domain;
#endif

    for (site = 0; site < nActiveSites; site++) {
        SW_RUN_deepCopy(
            sw_template, &SW_Runs[site], copyWeatherHist, main_LogInfo
        );
        if (main_LogInfo->stopRun) {
            return;
        }

#if defined(SWNETCDF)
        SW_Runs[site].SiteSim.site_has_swrcpMineralSoil =
            sw_template->SiteIn->inputsProvideSWRCp;
#endif

        SW_CTL_init_run(&SW_Runs[site], main_LogInfo);
        if (main_LogInfo->stopRun) {
            return;
        }

        SW_Runs[site].RunInfo.siteIndex = site;
        SW_Runs[site].RunInfo.nSites = SW_Domain->nActiveSuidsProc;
    }
}

/**
@brief Allocate/deallocate information necessary for running simulations, i.e.,
SW_RUN, SW_RUN_INPUTS, LOG_INFO for each active site

@param[in] allocate A flag specifying if the structs should be allocated
(swTRUE) or deallocated (swFALSE)
@param[in] nActiveSites Number of active sites to allocate for
@param[out] SW_Runs A list of SW_RUN instances of size "nActiveSites"
@param[out] LogInfos A list of LOG_INFO instances of size "nActiveSites"
@param[out] mainLogInfo The main LOG_INFO instance for the program
*/
static void handle_sim_structs_mem(
    Bool allocate,
    size_t nActiveSites,
    SW_RUN **SW_Runs,
    LOG_INFO **LogInfos,
    LOG_INFO *main_LogInfo
) {
    const int numDeallocArrays = 2;
    void **deallocArrays[] = {(void **) SW_Runs, (void **) LogInfos};

    int arr;

    if (allocate) {
        *SW_Runs = (SW_RUN *) Mem_Malloc(
            sizeof(SW_RUN) * nActiveSites, "alloc_sim_structs", main_LogInfo
        );
        checkReturn(main_LogInfo->stopRun);

        *LogInfos = (LOG_INFO *) Mem_Malloc(
            sizeof(LOG_INFO) * nActiveSites, "alloc_sim_structs", main_LogInfo
        );
        checkReturn(main_LogInfo->stopRun);
    } else {
        for (arr = 0; arr < numDeallocArrays; arr++) {
            if (!isnull(*(deallocArrays[arr]))) {
                free((void *) *(deallocArrays[arr]));
                *(deallocArrays[arr]) = NULL;
            }
        }
    }
}

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
@brief Wrapper function to report that the program is running simulations
across the domain; this is handled differently when SWMPI is enabled

@note The message reported with SWMPI enabled resembles the format:
    "is running simulations across the domain (<n active sites> active sites)
    with <n spawned processes> process(es)..."

@param[in] SW_Domain Struct of type SW_DOMAIN holding constant
    temporal/spatial information for a set of simulation runs
@param[in] rank Process number known to MPI for the current process (aka rank);
defaults to 0 (main process) if we are running sequentially
@param[in] worldSize Total number of processes that the MPI run has created
*/
static void report_sim_start(SW_DOMAIN *SW_Domain, int rank, int worldSize) {
#if !defined(SWMPI)
    SW_MSG_ROOT("is running simulations across the domain...", rank);

    (void) SW_Domain;
    (void) rank;
    (void) worldSize;
#else
    char reportStr[MAX_FILENAMESIZE] = "\0";

    if (rank == ROOT_PROC) {
        snprintf(
            reportStr,
            MAX_FILENAMESIZE,
            "is running simulations across the domain (%zu active sites) with "
            "%d %s...",
            SW_Domain->nActiveSuidsTot,
            worldSize,
            (worldSize > 1) ? "processes" : "process"
        );

        SW_MSG_ROOT(reportStr, rank);
    }
#endif
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
static void begin_year(
    SW_RUN *sw, SW_OUT_DOM *OutDom, Bool updateConstInfo, LOG_INFO *LogInfo
) {
    // SW_F_new_year() not needed

    // call SW_MDL_new_year() first to set up time-related arrays for this year
    if (updateConstInfo) {
        SW_MDL_new_year(sw->ModelIn, sw->ModelSim);
    }

    // SW_MKV_new_year() not needed

    // SW_SKY_new_year(): Update daily climate variables from monthly values
    SW_SKY_new_year(
        sw->ModelSim,
        sw->ModelSim->yearIdxSpinSim,
        sw->RunIn.SkyRunIn.snow_density,
        sw->RunIn.SkyRunIn.snow_density_daily
    );

    SW_VES_new_year(sw->VegEstabIn->count);

    // SW_VPD_new_year(): Dynamic CO2 effects on vegetation
    SW_VPD_new_year(
        sw->RunIn.weathRunAllHist,
        sw->ModelSim,
        &sw->VegProdSim,
        &sw->SoilSim,
        sw->VegProdIn->isBiomAsIf100Cover,
        sw->VegProdIn->veg_method,
        sw->ModelSim->inputYearIdx,
        sw->VegProdIn->nYearsDynamicShort,
        sw->VegProdIn->nYearsDynamicLong,
        sw->SiteIn->methodMaxDepthSoilTemperature,
        &sw->RunIn.VegProdRunIn,
        &sw->VegProdSim.veg,
        &sw->VegProdIn->veg
    );

    SW_SIT_new_year(
        sw->SiteIn->methodMaxDepthSoilTemperature,
        sw->VegProdSim.annTempLongAvg,
        &sw->RunIn.SiteRunIn.Tsoil_constant
    );

    // SW_FLW_new_year() not needed

    if (updateConstInfo) {
        SW_SWC_new_year(
            sw->SoilWatIn,
            &sw->SoilWatSim,
            &sw->SiteSim,
            sw->ModelSim->year,
            sw->SiteIn->reset_yr,
            updateConstInfo,
            sw->RunIn.SiteRunIn.n_layers,
            LogInfo
        );
        if (LogInfo->stopRun) {
            return; // Exit function prematurely due to error
        }

        // SW_CBN_new_year() not needed
        SW_OUT_new_year(
            sw->ModelSim->firstdoy,
            sw->ModelSim->lastdoy,
            OutDom,
            sw->OutRun->first,
            sw->OutRun->last
        );
    }
}

static void begin_day(SW_RUN *sw, LOG_INFO *LogInfo) {
    SW_MDL_new_day(sw->ModelSim);
    SW_WTH_new_day(
        sw->WeatherIn,
        &sw->WeatherSim,
        sw->RunIn.weathRunAllHist,
        sw->SiteIn,
        sw->SoilWatSim.snowpack,
        sw->ModelSim->doy,
        sw->ModelSim->year,
        sw->ModelSim->inputYearIdx,
        sw->ModelSim->lastdoy,
        LogInfo
    );
}

static void end_day(SW_RUN *sw, SW_OUT_DOM *OutDom, LOG_INFO *LogInfo) {
    TimeInt localTOffset = 1; // tOffset is one when called from this function

    if (sw->ModelSim->doOutput) {
        collect_values(sw, OutDom, swFALSE, localTOffset, LogInfo);
        if (LogInfo->stopRun) {
            return; // Exit function prematurely due to error
        }
    }

    SW_SWC_end_day(&sw->SoilWatSim, sw->RunIn.SiteRunIn.n_layers);
}

/**
@brief Copy dynamic memory from a template SW_RUN to a new instance

@param[in] source Source struct of type SW_RUN to copy
@param[out] dest Destination struct of type SW_RUN to be copied into
@param[in] copyWeatherHist Specifies if the weather data should be copied;
this only has the chance to be false when the program is dealing with
nc inputs
@param[out] LogInfo Holds information on warnings and errors
*/
void SW_RUN_deepCopy(
    SW_RUN *source, SW_RUN *dest, Bool copyWeatherHist, LOG_INFO *LogInfo
) {
    const TimeInt n_weathYears = 1;
    const IntU prevEstabCount = source->VegEstabIn->count;

    memcpy(dest, source, sizeof(*dest));

    dest->SoilWatIn->hist.file_prefix = NULL; /* currently unused */

    /* Allocate memory and copy daily weather */
    dest->RunIn.weathRunAllHist = NULL;

    if (copyWeatherHist) {
        SW_WTH_allocateAllWeather(
            &dest->RunIn.weathRunAllHist, n_weathYears, LogInfo
        );
        if (LogInfo->stopRun) {
            return;
        }
    }

    SW_VPD_init_ptrs(&dest->VegProdSim);
    SW_VES_init_ptrs(dest->VegEstabIn, dest->ves_p_accu, dest->ves_p_oagg);

    /* Copy vegetation establishment parameters */
    source->VegEstabIn->count = prevEstabCount;
    memcpy(
        &dest->VegEstabSim.parms,
        &source->VegEstabSim.parms,
        sizeof(dest->VegEstabSim.parms)
    );

    SW_VegEstab_alloc_outptrs(
        dest->ves_p_accu, dest->ves_p_oagg, source->VegEstabIn->count, LogInfo
    );
    if (LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }
}

/**
@brief Prepare necessary information for the next day of the simulation

@param[in] sw_template Template SW_RUN for the function to use as a
reference for local versions of SW_RUN
@param[in] SW_Domain Struct of type SW_DOMAIN holding constant
temporal/spatial information for a set of simulation runs
@param[in] LogInfos A list of LOG_INFO instances of size "nActiveSites" that
will be used to store site log information through all daily runs
@param[in] tempVals An allocated space to store temporary input values
for converting and setting into proper location
@param[in] initFirstYear A flag specifying if it is the first day of the
program run and we need to read and/or setup weather
@param[in,out] SW_Runs A list of SW_RUN instances of size "nActiveSites" that
will be used for holding all information for the simulation
@param[out] SW_WallTime Struct of type SW_WALLTIME that holds timing
information for the program run
@param[out] newSoils A temporary list of SW_SOIL_RUN_INPUTS instances to
store input values
@param[out] main_LogInfo Holds information on warnings and errors
*/
static void prepare_next_day(
    SW_RUN *sw_template,
    SW_DOMAIN *SW_Domain,
    LOG_INFO *LogInfos,
    double *tempVals,
    Bool initFirstYear,
    SW_RUN *SW_Runs,
    SW_WALLTIME *SW_WallTime,
    SW_SOIL_RUN_INPUTS *newSoils,
    LOG_INFO *main_LogInfo
) {
#if defined(SWNETCDF)
    const Bool readWeather =
        SW_Domain->netCDFInput.readInVars[eSW_InWeather][0];
    const Bool readConstInfo = swFALSE;

    WallTimeSpec tsr;
    Bool ok_tsr = swFALSE;
#endif

    size_t site;
    size_t nActiveSites = SW_Domain->nActiveSuidsProc;
    TimeInt inputYearIdx = SW_Domain->SW_ConstInfo.ModelSim.inputYearIdx;
    size_t baseSuid[NC_DIMS] = {0};
    size_t *suid = baseSuid;

    TimeInt doy = SW_Domain->SW_ConstInfo.ModelSim.doy;
    TimeInt firstDoy = SW_Domain->SW_ConstInfo.ModelSim.firstdoy;
    TimeInt lastDoy = SW_Domain->SW_ConstInfo.ModelSim.lastdoy;

#ifdef SWDEBUG
    if (debug) {
        sw_printf("\t: begin day = %d ... ", doy);
    }
#endif

    if (doy == firstDoy || doy == lastDoy || initFirstYear) {
        if (!initFirstYear && doy == lastDoy) {
            SW_Domain->SW_ConstInfo.ModelSim.year++;
        }

#if defined(SWNETCDF)
        if (!SW_Domain->SW_SpinUp.spinup) {
            if (readWeather) {
                set_walltime(&tsr, &ok_tsr);
                SW_NCIN_read_inputs(
                    SW_Runs,
                    SW_Domain,
                    readConstInfo,
                    SW_Domain->domStartIndex,
                    SW_Domain->domCounts,
                    SW_Domain->SW_PathInputs.openInFileIDs,
                    tempVals,
                    SW_Domain->nActiveSuidsProc,
                    newSoils,
                    LogInfos,
                    main_LogInfo
                );
                SW_WT_TimeRun(tsr, ok_tsr, TIME_IO_IN, SW_WallTime);
                checkJumpToLabel(main_LogInfo->stopRun, handleLogs);
            } else {
                for (site = 0; site < nActiveSites; site++) {
                    memcpy(
                        &SW_Runs[site].RunIn.weathRunAllHist[0],
                        &sw_template->RunIn.weathRunAllHist[inputYearIdx],
                        sizeof(SW_WEATHER_HIST)
                    );
                }
            }
        }
#endif

        for (site = 0; site < nActiveSites; site++) {
            if (!LogInfos[site].stopRun) {
                suid = SW_Domain->globDomSuids[site];

                // finalize daily weather
                SW_WTH_finalize_yearly_weather(
                    SW_Runs[site].MarkovIn,
                    SW_Runs[site].WeatherIn,
                    &SW_Runs[site].RunIn.weathRunAllHist[inputYearIdx],
                    &SW_Runs[site].WeatherSim,
                    SW_Runs[site].ModelSim->cum_monthdays,
                    SW_Runs[site].ModelSim->days_in_month,
                    suid,
                    SW_Runs[site].ModelSim->year,
                    SW_Runs[site].WeatherSim.trivialScaling,
                    swFALSE, // Does not matter
                    &LogInfos[site]
                );
            }
        }
    }

#if defined(SWNETCDF)
handleLogs:
#endif
    SW_F_handle_log_counts(
        LogInfos, SW_Domain->netCDFInput.progVals, main_LogInfo
    );

    SW_F_check_fatal_log(SW_Domain, nActiveSites, main_LogInfo);
    checkReturn(main_LogInfo->stopRun);

#if !defined(SWNETCDF)
    (void) sw_template;
    (void) tempVals;
    (void) newSoils;
    (void) main_LogInfo;
    (void) SW_WallTime;
#endif
}

/**
@brief Attempt to output values if necessary (SWNETCDF mode only)

@param[in] rank Process number known to MPI for the current process (aka rank)
@param[in] sw_template Template SW_RUN for the function to use as a
reference for local versions of SW_RUN
@param[in] SW_Domain Struct of type SW_DOMAIN holding constant
temporal/spatial information for a set of simulation runs
@param[in] nActiveSites Number of active sites to initialize log
information for
@param[out] SW_WallTime Struct of type SW_WALLTIME that holds timing
information for the program run
@param[out] main_LogInfo Holds information on warnings and errors
*/
static void finalize_sites_day(
    int rank,
    SW_RUN *sw_template,
    SW_DOMAIN *SW_Domain,
    SW_WALLTIME *SW_WallTime,
    LOG_INFO *main_LogInfo
) {
#if defined(SWNETCDF)
    WallTimeSpec tsr;
    Bool ok_tsr = swFALSE;

    Bool forceWriteOut = (Bool) (!runSims || main_LogInfo->stopRun);

    set_walltime(&tsr, &ok_tsr);
    SW_NCOUT_write_output(
        &SW_Domain->OutDom,
        sw_template->OutRun->p_OUT,
        SW_Domain->SW_ConstInfo.OutRun.irow_OUT,
        sw_template->SW_PathOutputs->numOutFiles,
        SW_Domain->nActiveSuidsProc,
        SW_Domain->domStartIndex[eSW_InDomain],
        SW_Domain->domCounts[eSW_InDomain],
        sw_template->SW_PathOutputs->openOutFileIDs,
        sw_template->SW_PathOutputs->ncOutVarIDs,
        SW_Domain->netCDFInput.siteDoms[eSW_InDomain],
        forceWriteOut,
        sw_template->SW_PathOutputs->outTimeSizes,
        main_LogInfo
    );
    SW_WT_TimeRun(tsr, ok_tsr, TIME_IO_OUT, SW_WallTime);
#else
    (void) rank;
    (void) sw_template;
    (void) SW_Domain;
    (void) main_LogInfo;
    (void) SW_WallTime;
#endif

    SW_Domain->SW_ConstInfo.ModelSim.doy++;
}

/* =================================================== */
/*             Global Function Definitions             */
/* --------------------------------------------------- */

/**
@brief Calls 'SW_CTL_run_current_day' for each day in
the simulation start/end dates

@param[in] sw_template Template SW_RUN for the function to use as a
reference for local versions of SW_RUN
@param[in] SW_Domain Struct of type SW_DOMAIN holding constant
temporal/spatial information for a set of simulation runs
@param[in,out] SW_Runs A list of SW_RUN instances of size "nActiveSites" that
will be used for holding all information for the simulation
@param[in] LogInfos A list of LOG_INFO instances of size "nActiveSites" that
will be used to store site log information through all daily runs
@param[out] main_LogInfo Holds information on warnings and errors
*/
void SW_CTL_sim_sites(
    SW_RUN *sw_template,
    SW_DOMAIN *SW_Domain,
    SW_RUN *SW_Runs,
    LOG_INFO *LogInfos,
    LOG_INFO *main_LogInfo
) {
#ifdef SWDEBUG
    int debug = 0;
#endif

    size_t nActiveSites = SW_Domain->nActiveSuidsProc;

    size_t site;
    TimeInt doy = SW_Domain->SW_ConstInfo.ModelSim.doy;
    TimeInt lastDoy = SW_Domain->SW_ConstInfo.ModelSim.lastdoy;
    signed char *runStatus = NULL;
    Bool updateConstInfo = swTRUE;

#if defined(SWNETCDF)
    signed char *progVals = SW_Domain->netCDFInput.progVals;
    size_t actSiteIdx;
#else
    (void) sw_template;
#endif

    for (site = 0; site < nActiveSites; site++) {

#if defined(SWNETCDF)
        actSiteIdx = SW_Domain->actSiteIdx[eSW_InDomain][site];
        runStatus = &progVals[actSiteIdx];

        if (*runStatus == PRGRSS_FAIL) {
            continue;
        }
#endif

        if (doy == lastDoy) {
            begin_year(
                &SW_Runs[site],
                &SW_Domain->OutDom,
                updateConstInfo,
                main_LogInfo
            );

            updateConstInfo = swFALSE;
            if (main_LogInfo->stopRun) {
                goto handleLogs;
            }
        }

#ifdef SWDEBUG
        if (debug) {
            sw_printf("\n'SW_CTL_sim_sites': simulate site = %zu\n", site);
        }
#endif

        SW_CTL_run_current_day(
            &SW_Runs[site], &SW_Domain->OutDom, &LogInfos[site]
        );

    handleLogs:
        SW_F_handle_log_counts(&LogInfos[site], runStatus, main_LogInfo);

#if defined(SWNETCDF)
        if (LogInfos[site].stopRun) {
            SW_NCOUT_reset_failed_sites(
                SW_Domain, actSiteIdx, sw_template->OutRun->p_OUT
            );
        }
#endif
    }

    SW_F_check_fatal_log(SW_Domain, nActiveSites, main_LogInfo);
}

/**
@brief Run through all daily time steps requested by the user, simulating
every site at once per daily time step

@param[in] rank Process number known to MPI for the current process (aka rank)
@param[in] sw_template Template SW_RUN for the function to use as a
reference for local versions of SW_RUN
@param[in] startDay Start day of the simulation
@param[in] endDay End day of the simulation
@param[in] tempVals An allocated space to store temporary input values
for converting and setting into proper location
@param[in] newSoils A temporary list of SW_SOIL_RUN_INPUTS instances to
store input values
@param[in] SW_Domain Struct of type SW_DOMAIN holding constant
temporal/spatial information for a set of simulation runs
@param[in,out] SW_Runs A list of SW_RUN instances of size "nActiveSites" that
will be used for holding all information for the simulation
@param[in] LogInfos A list of LOG_INFO instances of size "nActiveSites" that
will be used to store site log information through all daily runs
@param[out] SW_WallTime Struct of type SW_WALLTIME that holds timing
information for the program run
@param[out] main_LogInfo Holds information on warnings and errors
*/
void SW_CTL_run_daily_timesteps(
    int rank,
    SW_RUN *sw_template,
    TimeInt startDay,
    TimeInt endDay,
    double *tempVals,
    SW_SOIL_RUN_INPUTS *newSoils,
    SW_DOMAIN *SW_Domain,
    SW_RUN *SW_Runs,
    LOG_INFO *LogInfos,
    SW_WALLTIME *SW_WallTime,
    LOG_INFO *main_LogInfo
) {
    TimeInt day;
    Bool initFirstYear = swFALSE;

#if defined(SWNETCDF)
    WallTimeSpec tsr;
    Bool ok_tsr = swFALSE;
#endif

    for (day = startDay; day <= endDay && !runSims; day++) {
        initFirstYear = (Bool) (day == startDay);
        prepare_next_day(
            sw_template,
            SW_Domain,
            LogInfos,
            tempVals,
            initFirstYear,
            SW_Runs,
            SW_WallTime,
            newSoils,
            main_LogInfo
        );
        checkJumpToLabel(main_LogInfo->stopRun, handleOutput);

        if (runSims) {
#if defined(SWNETCDF)
            set_walltime(&tsr, &ok_tsr);
#endif
            SW_CTL_sim_sites(
                sw_template, SW_Domain, SW_Runs, LogInfos, main_LogInfo
            );
#if defined(SWNETCDF)
            SW_WT_TimeRun(tsr, ok_tsr, TIME_COMPUTE, SW_WallTime);
#endif
        }

    handleOutput:
        finalize_sites_day(
            rank, sw_template, SW_Domain, SW_WallTime, main_LogInfo
        );
        checkReturn(main_LogInfo->stopRun);
    }
}

/**
@brief Run through simulations in a space-before-time approach, meaning instead
of simulating one site at a time, we simulate one day at a time through all
active sites

@param[in] rank Process number known to MPI for the current process (aka rank);
    defaults to 0 (main process) if we are running sequentially
@param[in] worldSize Total number of processes that the MPI run has created
@param[in] sw_template Template SW_RUN for the function to use as a
    reference for local versions of SW_RUN
@param[in] SW_Domain Struct of type SW_DOMAIN holding constant
    temporal/spatial information for a set of simulation runs
@param[out] SW_WallTime Struct of type SW_WALLTIME that holds timing
    information for the program run
@param[out] main_LogInfo Holds information on warnings and errors
*/
void SW_CTL_RunSimSet(
    int rank,
    int worldSize,
    SW_RUN *sw_template,
    SW_DOMAIN *SW_Domain,
    SW_WALLTIME *SW_WallTime,
    LOG_INFO *main_LogInfo
) {
    const Bool alloc = swTRUE;
    const Bool dealloc = swFALSE;

    const size_t nActiveSites = SW_Domain->nActiveSuidsProc;

#if defined(SWNETCDF)
    const TimeInt newSimStartDay = 1;
    const Bool freshRun = (Bool) (SW_Domain->startSimDay == newSimStartDay);
#else
    const Bool freshRun = swTRUE;
#endif

    Bool readFromCacheFile = swFALSE;
    Bool copyWeatherHist = swTRUE;

    SW_SOIL_RUN_INPUTS *newSoils = NULL;
    SW_RUN *siteRuns = NULL;
    LOG_INFO *siteLogs = NULL;
    double *tempVals = NULL;

    (void) signal(SIGINT, handle_interrupt);
    (void) signal(SIGTERM, handle_interrupt);

    if (main_LogInfo->printProgressMsg) {
        report_sim_start(SW_Domain, rank, worldSize);
    }

    handle_sim_structs_mem(
        alloc, nActiveSites, &siteRuns, &siteLogs, main_LogInfo
    );
    checkReturn(main_LogInfo->stopRun);

#if defined(SWNETCDF)
    const Bool readConstInfo = swTRUE;
    const Bool readCache = swTRUE;
    const char *cacheFileName = SW_Domain->SW_PathInputs.txtInFiles[eNCCache];

    size_t site;
    size_t siteIdx;
    Bool cacheAtEnd = swFALSE;

    copyWeatherHist =
        (Bool) !SW_Domain->netCDFInput.readInVars[eSW_InWeather][0];

    readFromCacheFile = FileExists(cacheFileName);

#if defined(SWMPI)
    MPI_Barrier(MPI_COMM_WORLD);
#endif

    if (!readFromCacheFile && rank == ROOT_PROC) {
        SW_NCIN_create_cache_file(SW_Domain, sw_template, main_LogInfo);
    }
    checkReturn(main_LogInfo->stopRun);

    SW_NCIN_handle_temp_inputs(
        alloc, SW_Domain, &tempVals, &newSoils, main_LogInfo
    );
    checkJumpToLabel(main_LogInfo->stopRun, freeMem);
#endif

    init_all_logs(nActiveSites, main_LogInfo->logfp, siteLogs);

#if defined(SWNETCDF)
    SW_NCIN_read_inputs(
        siteRuns,
        SW_Domain,
        readConstInfo,
        SW_Domain->domStartIndex,
        SW_Domain->domCounts,
        SW_Domain->SW_PathInputs.openInFileIDs,
        tempVals,
        nActiveSites,
        newSoils,
        siteLogs,
        main_LogInfo
    );
    checkJumpToLabel(main_LogInfo->stopRun, freeMem);
#endif

    SW_Domain->SW_ConstInfo.ModelSim.progRestarted =
        (Bool) (readFromCacheFile && !freshRun);
    init_all_runs(
        copyWeatherHist,
        nActiveSites,
        SW_Domain,
        sw_template,
        siteRuns,
        main_LogInfo
    );
    checkJumpToLabel(main_LogInfo->stopRun, freeMem);

#if defined(SWNETCDF)
    // Check if any sites failed when reading initial values before
    // running simulations
    for (site = 0; site < nActiveSites; site++) {
        siteIdx = SW_Domain->actSiteIdx[eSW_InDomain][site];
        SW_F_handle_log_counts(
            &siteLogs[site],
            &SW_Domain->netCDFInput.progVals[siteIdx],
            main_LogInfo
        );
    }
    SW_F_check_fatal_log(SW_Domain, nActiveSites, main_LogInfo);
    checkJumpToLabel(main_LogInfo->stopRun, freeMem);

    if (readFromCacheFile && !freshRun) {
        SW_NCIN_handle_cache_vals(
            rank, readCache, SW_Domain, sw_template, siteRuns, main_LogInfo
        );
        checkJumpToLabel(main_LogInfo->stopRun, freeMem);
    }

    cacheAtEnd = swTRUE;
#endif

    SW_CTL_run_daily_timesteps(
        rank,
        sw_template,
        SW_Domain->startSimDay,
        SW_Domain->endSimDay,
        tempVals,
        newSoils,
        SW_Domain,
        siteRuns,
        siteLogs,
        SW_WallTime,
        main_LogInfo
    );

freeMem:
#if defined(SWNETCDF)
    SW_NCIN_write_cache(
        rank,
        SW_Domain,
        sw_template,
        siteRuns,
        siteLogs,
        cacheAtEnd,
        main_LogInfo
    );

    SW_NCIN_update_progress_info(SW_Domain, siteRuns, main_LogInfo);

    SW_F_report_logs(SW_Domain, siteLogs, nActiveSites);

    SW_NCIN_handle_temp_inputs(
        alloc, SW_Domain, &tempVals, &newSoils, main_LogInfo
    );
#endif

    handle_sim_structs_mem(
        dealloc, nActiveSites, &siteRuns, &siteLogs, main_LogInfo
    );
}

/**
@brief Initialize all possible pointers to NULL incase of an unexpected
program exit

@param[in] SW_Domain Struct of type SW_DOMAIN holding constant
    temporal/spatial information for a set of simulation runs
@param[in,out] sw Comprehensive struct of type SW_RUN containing
    all information in the simulation
*/
void SW_CTL_init_ptrs(SW_DOMAIN *SW_Domain, SW_RUN *sw) {
    // Initialize pointers to structs held within SW_DOMAIN
    sw->WeatherIn = &SW_Domain->SW_ConstInfo.WeatherIn;
    sw->CarbonIn = &SW_Domain->SW_ConstInfo.CarbonIn;
    sw->MarkovIn = &SW_Domain->SW_ConstInfo.MarkovIn;
    sw->VegProdIn = &SW_Domain->SW_ConstInfo.VegProdIn;
    sw->ModelIn = &SW_Domain->SW_ConstInfo.ModelIn;
    sw->VegEstabIn = &SW_Domain->SW_ConstInfo.VegEstabIn;
    sw->SoilWatIn = &SW_Domain->SW_ConstInfo.SoilWatIn;
    sw->SiteIn = &SW_Domain->SW_ConstInfo.SiteIn;
    sw->ModelSim = &SW_Domain->SW_ConstInfo.ModelSim;

    // Initialize pointers within substructs
    SW_WTH_init_ptrs(&sw->RunIn.weathRunAllHist);
    SW_MKV_init_ptrs(sw->MarkovIn);
    SW_VPD_init_ptrs(&sw->VegProdSim);
    SW_VES_init_ptrs(sw->VegEstabIn, sw->ves_p_accu, sw->ves_p_oagg);
    SW_OUT_init_ptrs(sw->OutRun, sw->SW_PathOutputs);
    SW_SWC_init_ptrs(sw->SoilWatIn, &sw->SoilWatSim);
    SW_CBN_init_ptrs(sw->CarbonIn);
}

/**
@brief Construct, setup, and obtain inputs for SW_DOMAIN

@param[in] rank Process number known to MPI for the current process (aka rank)
@param[in] worldSize Total number of processes that the MPI run has created
(only relevant with SWMPI enabled)
@param[in] renameDomainTemp Specifies if the created domain netCDF file
will automatically be renamed
@param[out] SW_Domain Struct of type SW_DOMAIN holding constant
    temporal/spatial information for a set of simulation runs
@param[out] LogInfo Holds information on warnings and errors
*/
void SW_CTL_setup_domain(
    int rank,
    int worldSize,
    Bool renameDomainTemp,
    SW_DOMAIN *SW_Domain,
    LOG_INFO *LogInfo
) {
#if defined(SWNETCDF)
    /* Combine both prog vars into one in this case */
    const int nUniqueDomVars = 2;
    const Bool openInPar = swFALSE;
    const int openMode = NC_NOWRITE;
    const Bool domProgFileExists[] = {
        FileExists(SW_Domain->SW_PathInputs.ncInFiles[eSW_InDomain][vNCdom]),
        FileExists(
            SW_Domain->SW_PathInputs.ncInFiles[eSW_InDomain][vNCprogStatus]
        ),
        FileExists(SW_Domain->SW_PathInputs.ncInFiles[eSW_InDomain][vNCprogDay])
    };

    int file;
#endif

    SW_F_construct(&SW_Domain->SW_PathInputs);

    SW_F_read(rank, &SW_Domain->SW_PathInputs, LogInfo);
    if (LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

    SW_DOM_read(SW_Domain, LogInfo);
    if (LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

    SW_DOM_construct(SW_Domain->SW_SpinUp.rng_seed, SW_Domain);

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

    for (file = 0; file < nUniqueDomVars; file++) {
        if (rank == ROOT_PROC && !domProgFileExists[file]) {
            switch (file) {
            case vNCdom:
                SW_NCIN_create_domain_template(
                    SW_Domain, fnameDomainTemplateNC, LogInfo
                );

                if (!LogInfo->stopRun && !renameDomainTemp) {
                    LogError(
                        LogInfo,
                        LOGERROR,
                        "Domain netCDF template has been created. "
                        "Please modify it and rename it to "
                        "'domain.nc' when done and try again. "
                        "The template path is: %s",
                        DOMAIN_TEMP
                    );
                }
                break;
            case vNCprogStatus: /* vNCprogStatus & vNCprogDay */
                SW_DOM_CreateProgress(SW_Domain, LogInfo);
                break;
            default:
                LogError(
                    LogInfo,
                    LOGERROR,
                    "Programmer: Unkown file when setting up domain."
                );
                break;
            }
        }
        checkReturn(LogInfo->stopRun);
    }

    // Open necessary netCDF input files and check for consistency with
    // domain
    SW_NCIN_open_dom_prog_files(
        &SW_Domain->netCDFInput, &SW_Domain->SW_PathInputs, LogInfo
    );
    checkReturn(LogInfo->stopRun);

    if (rank == ROOT_PROC) {
        SW_NC_check(
            SW_Domain,
            &SW_Domain->SW_PathInputs.ncDomFileIDs[vNCdom],
            SW_Domain->SW_PathInputs.ncInFiles[eSW_InDomain][vNCdom],
            openInPar,
            openMode,
            LogInfo
        );
    }
    checkReturn(LogInfo->stopRun);
#else
    (void) renameDomainTemp;
#endif

    SW_DOM_SimSet(rank, worldSize, SW_Domain, LogInfo);
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
    SW_MDL_construct(sw->ModelSim);
    SW_WTH_construct(
        sw->WeatherIn, &sw->WeatherSim, sw->weath_p_accu, sw->weath_p_oagg
    );

    // delay SW_MKV_construct() until we know from inputs whether we need it
    // SW_SKY_construct() not need
    SW_SIT_construct(sw->SiteIn, &sw->SiteSim, &sw->RunIn.SiteRunIn.n_layers);
    SW_VES_construct(
        sw->VegEstabIn, &sw->VegEstabSim, sw->ves_p_oagg, sw->ves_p_accu
    );
    SW_VPD_construct(
        sw->VegProdIn, &sw->RunIn.VegProdRunIn, sw->vp_p_oagg, sw->vp_p_accu
    );
    // SW_FLW_construct() not needed
    SW_OUT_construct(zeroOutInfo, sw->SW_PathOutputs, sw->OutRun);
    if (LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }
    SW_SWC_construct(
        sw->SoilWatIn, &sw->SoilWatSim, sw->sw_p_accu, sw->sw_p_oagg
    );
    SW_CBN_construct(sw->CarbonIn);
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
    SW_MKV_deconstruct(sw->MarkovIn);
    // SW_SKY_INPUTS_deconstruct() not needed
    // SW_SIT_deconstruct() not needed
    SW_VES_deconstruct(sw->VegEstabIn->count, sw->ves_p_accu, sw->ves_p_oagg);
    SW_VPD_deconstruct(&sw->VegProdSim);
    // SW_FLW_deconstruct() not needed
    SW_SWC_deconstruct(sw->SoilWatIn, &sw->SoilWatSim);
    SW_CBN_deconstruct(sw->CarbonIn);
}

/**
@brief Initialize simulation run (based on user inputs)

Note: Time will only be set up correctly while carrying out a simulation year,
i.e., after calling begin_year()

@param[in,out] sw Comprehensive structure holding all information
    dealt with in SOILWAT2
@param[out] LogInfo Holds information on warnings and errors
*/
void SW_CTL_init_run(SW_RUN *sw, LOG_INFO *LogInfo) {

    // SW_F_init_run() not needed
    SW_MDL_init_run(sw->ModelSim, sw->ModelIn->startyr);
    SW_WTH_init_run(sw->WeatherIn, &sw->WeatherSim);
    // SW_MKV_init_run() not needed
    SW_PET_init_run(&sw->AtmDemSim);

    SW_SKY_init_run(&sw->RunIn.SkyRunIn, LogInfo);
    if (LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

    SW_SIT_init_run(
        sw->VegProdIn,
        sw->SiteIn,
        &sw->SiteSim,
        &sw->RunIn.SoilRunIn,
        &sw->VegProdIn->veg,
        sw->RunIn.SiteRunIn.n_layers,
        LogInfo
    );
    if (LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

    // SW_VES_init_run() must be called after `SW_SIT_init_run()`
    SW_VES_init_run(
        &sw->VegEstabIn->parms,
        &sw->RunIn.SoilRunIn,
        &sw->SiteSim,
        sw->SiteSim.n_transp_lyrs,
        sw->VegEstabIn->count,
        LogInfo
    );
    if (LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

    SW_VPD_init_run(sw, LogInfo);
    if (LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

    SW_FLW_init_run(&sw->SoilWatSim);
    SW_ST_init_run(&sw->StRegSimVals);
    // SW_OUT_init_run() handled separately so that SW_CTL_init_run() can be
    //   useful for unit tests, rSOILWAT2, and STEPWAT2 applications
    SW_SWC_init_run(
        &sw->SoilWatSim,
        &sw->SiteSim,
        &sw->WeatherSim.temp_snow,
        sw->RunIn.SiteRunIn.n_layers
    );
    SW_CBN_init_run(
        &sw->VegProdIn->veg,
        &sw->VegProdSim.veg,
        sw->CarbonIn,
        sw->ModelIn->startyr,
        sw->ModelIn->endyr,
        LogInfo
    );
}

/**
@brief Calls 'SW_SWC_water_flow' for current day.

@param[in,out] sw Comprehensive struct of type SW_RUN containing
  all information in the simulation
@param[in,out] OutDom Struct of type SW_OUT_DOM that holds output
    information that do not change throughout simulation runs
@param[out] LogInfo Holds information on warnings and errors
*/
void SW_CTL_run_current_day(SW_RUN *sw, SW_OUT_DOM *OutDom, LOG_INFO *LogInfo) {
    /*=======================================================*/
#ifdef SWDEBUG
    int debug = 0;
#endif

#ifdef SWDEBUG
    if (debug) {
        sw_printf("\n'SW_CTL_run_current_day': begin year\n");
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
    if (sw->VegProdIn->use_SWA) {
        calculate_repartitioned_soilwater(
            &sw->SoilWatSim,
            sw->SiteSim.swcBulk_atSWPcrit,
            sw->VegProdIn,
            &sw->RunIn.VegProdRunIn.veg,
            sw->RunIn.SiteRunIn.n_layers
        );
    }

    if (sw->VegEstabIn->use) {
        SW_VES_checkestab(
            &sw->VegEstabIn->parms,
            &sw->VegEstabSim.parms,
            sw->WeatherSim.temp_avg,
            sw->SoilWatSim.swcBulk,
            sw->ModelSim->doy,
            sw->ModelSim->firstdoy,
            sw->VegEstabIn->count
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

#ifdef SWDEBUG
    if (debug) {
        sw_printf("'SW_CTL_run_current_day': flush output\n");
    }
#endif
    if (sw->ModelSim->doOutput) {
        SW_OUT_flush(sw, OutDom, LogInfo);
    }

#ifdef SWDEBUG
    if (LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

    if (debug) {
        sw_printf("'SW_CTL_run_current_day': completed.\n");
    }
#endif
}

/**
@brief Run a spin-up

  Calls 'SW_CTL_run_daily_timesteps' over an array of simulated years
          as specified by the given spinup scope and duration which
          then calls 'SW_SWC_water_flow' for each day.

  No output is produced during the spin-up; state variables including
  soil moisture and soil temperature are updated and handed off to the
  simulation run.

  A spin-up duration of 0 returns immediately (no spin-up).

@param[in] SW_Domain Struct of type SW_DOMAIN holding constant
temporal/spatial information for a set of simulation runs
@param[in,out] sw Comprehensive struct of type SW_RUN containing all
  information in the simulation
@param[out] LogInfo Holds information dealing with logfile output
*/
void SW_CTL_run_spinup(SW_DOMAIN *SW_Domain, SW_RUN *sw, LOG_INFO *LogInfo) {
    if (sw->ModelIn->SW_SpinUp.duration <= 0) {
        return;
    }

#ifdef SWDEBUG
    int debug = 0;
#endif

    unsigned int i;
    unsigned int k;
    unsigned int quotient = 0;
    unsigned int remainder = 0;
    int mode = sw->ModelIn->SW_SpinUp.mode;
    TimeInt yr;
    TimeInt duration = sw->ModelIn->SW_SpinUp.duration;
    TimeInt scope = sw->ModelIn->SW_SpinUp.scope;
    TimeInt finalyr = sw->ModelIn->startyr + scope - 1;
    TimeInt *years;
    Bool prev_doOut = sw->ModelSim->doOutput;
    years = (TimeInt *) Mem_Malloc(
        sizeof(TimeInt) * duration, "SW_CTL_run_spinup", LogInfo
    );
    if (LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }
    TimeInt daysInYear;

#ifdef SWDEBUG
    if (debug) {
        sw_printf(
            "'SW_CTL_run_spinup': "
            "mode = %d, duration = %d (# years), "
            "scope = %d [# calendar years out of %d-%d]\n",
            mode,
            duration,
            scope,
            sw->ModelIn->startyr,
            finalyr
        );
    }
#endif

    switch (mode) {
    case 2:
        // initialize structured array
        if (duration <= scope) {
            // 1:m
            yr = sw->ModelIn->startyr;
            for (i = 0; i < duration; i++) {
                years[i] = yr + i;
            }
        } else {
            // { {1:n}_(m//n), 1:(m%n) }
            quotient = duration / scope;
            remainder = duration % scope;
            yr = sw->ModelIn->startyr;
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
            yr = (TimeInt) RandUniIntRange(
                sw->ModelIn->startyr,
                finalyr,
                &sw->ModelIn->SW_SpinUp.spinup_rng
            );
            years[i] = yr;
        }
        break;
    }

    TimeInt *cur_yr = &sw->ModelSim->year;
    TimeInt yrIdx;

    sw->ModelSim->doOutput = swFALSE; // turn output temporarily off

    for (yrIdx = 0; yrIdx < duration; yrIdx++) {
        *cur_yr = years[yrIdx];
        daysInYear = Time_get_lastdoy_y(*cur_yr);

#ifdef SWDEBUG
        if (debug) {
            sw_printf(
                "'SW_CTL_run_spinup': simulate year = %d | %d\n",
                yrIdx + 1,
                *cur_yr
            );
        }
#endif

        // SW_CTL_run_daily_timesteps(
        //     daysInYear,
        //     NULL, // Temp vals
        //     SW_Domain,
        //     sw,
        //     LogInfo,
        //     NULL, // Wall time
        //     LogInfo
        // );
        if (LogInfo->stopRun) {
            goto reSet; // Exit function prematurely due to error
        }
    }

reSet: {
    sw->ModelSim->doOutput = prev_doOut; // reset doOutput to original value
    /* Note: don't reset sw->ModelSim->yearIdxSpinSim which is a
    continuous index across spinup and simulation years) */

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
        sw->WeatherIn,
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

    if (sw->WeatherIn->generateWeatherMethod == 2) {
        SW_MKV_setup(
            sw->MarkovIn,
            sw->WeatherIn->rng_seed,
            sw->WeatherIn->generateWeatherMethod,
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
        sw->WeatherIn,
        &sw->RunIn.weathRunAllHist,
        &sw->RunIn.SkyRunIn,
        sw->ModelIn,
        sw->RunIn.ModelRunIn.elevation,
        readTextInputs,
        sw->ModelSim->cum_monthdays,
        sw->ModelSim->days_in_month,
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
        sw->VegProdIn,
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
        sw->SiteIn,
        SW_PathInputs->txtInFiles,
        sw->CarbonIn,
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
        &sw->RunIn.SiteRunIn.n_layers,
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
        sw->RunIn.SiteRunIn.n_layers,
        SW_PathInputs->txtInFiles,
        sw->SiteIn->inputsProvideSWRCp,
        sw->RunIn.SoilRunIn.swrcpMineralSoil,
        sw->SiteIn->swrcpOM,
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
        sw->VegEstabIn,
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

    SW_CBN_setup(
        sw->CarbonIn,
        sw->ModelIn->startyr,
        sw->ModelIn->endyr,
        SW_PathInputs->txtInFiles,
        sw->VegProdIn->vegYear,
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
        sw->SoilWatIn, sw->ModelIn->endyr, SW_PathInputs->txtInFiles, LogInfo
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
