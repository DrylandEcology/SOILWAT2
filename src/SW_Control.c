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
#include "include/SW_Output_outarray.h"
#if defined(SWMPI)
#include "include/SW_MPI.h" // for SW_MPI_setup_fail, SW_MPI_Bcast
#include <mpi.h>            // for MPI_COMM_WORLD, MPI_Datatype
#else
#include "include/SW_netCDF_Output.h"
#endif
#endif

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
volatile sig_atomic_t runSims = 1;

/* =================================================== */
/*             Local Function Definitions              */
/* --------------------------------------------------- */

#if defined(SWNETCDF)
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
@param[in] OutDom Struct of type SW_OUT_DOM that holds output
information that do not change throughout simulation runs
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
    SW_OUT_DOM *OutDom,
    SW_RUN *sw_template,
    SW_RUN *SW_Runs,
    LOG_INFO *main_LogInfo
) {
    size_t site;

    for (site = 0; site < nActiveSites; site++) {
        SW_RUN_deepCopy(
            sw_template, &SW_Runs[site], OutDom, copyWeatherHist, main_LogInfo
        );
        if (main_LogInfo->stopRun) {
            return;
        }

        SW_CTL_init_run(&SW_Runs[site], main_LogInfo);
        if (main_LogInfo->stopRun) {
            return;
        }
    }
}

/**
@brief Allocate/deallocate information necessary for running simulations, i.e.,
SW_RUN, SW_RUN_INPUTS, LOG_INFO for each active site

@param[in] allocate A flag specifying if the structs should be allocated
(swTRUE) or deallocated (swFALSE)
@param[in] nActiveSites Number of active sites to allocate for
@param[out] SW_Runs A list of SW_RUN instances of size "nActiveSites"
@param[out] newSoils A temporary list of SW_SOIL_RUN_INPUTS instances to
store input values
@param[out] LogInfos A list of LOG_INFO instances of size "nActiveSites"
@param[out] mainLogInfo The main LOG_INFO instance for the program
*/
static void handle_sim_structs_mem(
    Bool allocate,
    Bool consistentSoilDepths,
    size_t nActiveSites,
    SW_RUN **SW_Runs,
    SW_SOIL_RUN_INPUTS **newSoils,
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

        if (!consistentSoilDepths) {
            *newSoils = (SW_SOIL_RUN_INPUTS *) Mem_Malloc(
                sizeof(SW_SOIL_RUN_INPUTS) * nActiveSites,
                "handle_sim_structs_mem",
                main_LogInfo
            );
        }
    } else {
        for (arr = 0; arr < numDeallocArrays; arr++) {
            if (!isnull(*(deallocArrays[arr]))) {
                free((void *) *(deallocArrays[arr]));
                *(deallocArrays[arr]) = NULL;
            }
        }
    }
}
#endif

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
@brief Go through all simulation logs and report them as needed

@param[in] SW_Domain Struct of type SW_DOMAIN holding constant
temporal/spatial information for a set of simulation runs
@param[in] simLogs A list of simulation logs (LOG_INFO) to be reported
@param[in] sDom Specifies the program's domain is site-oriented
@param[in] nSims Number of simulations that been run
*/
static void report_logs(
    SW_DOMAIN *SW_Domain, LOG_INFO *simLogs, Bool sDom, size_t nSims
) {
    /* tag_suid is 55:
       14 character for "(suid = [, ]) " + 40 character for 2 *
       ULONG_MAX + '\0' */
    char tag_suid[55] = "\0";

    size_t site;
    size_t ncSuid[NC_DIMS] = {0};

    for (site = 0; site < nSims; site++) {
#if defined(SWNETCDF)
        ncSuid[0] = SW_Domain->globDomSuids[site][0];
        ncSuid[1] = SW_Domain->globDomSuids[site][1];
#endif

        if (simLogs[site].stopRun || simLogs[site].numWarnings > 0) {
            // Write the error with the suid indices to have a universal
            // identifier; Put in the order of [x, y] or s
            if (sDom) {
                (void) snprintf(tag_suid, 55, "(suid = %lu) ", ncSuid[0] + 1);
            } else {
                (void) snprintf(
                    tag_suid,
                    55,
                    "(suid = [%lu, %lu]) ",
                    ncSuid[1] + 1,
                    ncSuid[0] + 1
                );
            }

            sw_write_warnings(tag_suid, &simLogs[site]);
        }
    }
}

/**
@brief Perform appropriate operations on any log information
after a simulation run

@param[in] simLog Log that has been gone through a simulation run
@param[in] SW_Domain Struct of type SW_DOMAIN holding constant
temporal/spatial information for a set of simulation runs
@param[in] nSims Number of simulations that been run
@param[out] runStatus Returns PRGRSS_FAIL (failed) if the respective
log information pertains to a failed site, otherwise, this value will
not be modified
@param[out] mainLog Main log information from the domain-level
*/
static void handle_logs(
    LOG_INFO *simLog,
    SW_DOMAIN *SW_Domain,
    size_t nSims,
    signed char *runStatus, // NOLINT(readability-non-const-parameter)
    LOG_INFO *mainLog
) {
    if (simLog->numWarnings > 0) {
        // Counter of simulation units with warnings
        mainLog->numDomainWarnings++;
    }

    /* Report errors and warnings for suid */
    if (simLog->stopRun) {
        // Counter of simulation units with error
        mainLog->numDomainErrors++;
#if defined(SWMPI)
        if (mainLog->numDomainErrors == (size_t) SW_Domain->maxSimErrors) {
            LogError(
                mainLog,
                LOGERROR,
                "Maximum number of allowed simulation errors reached "
                "(n = %d).",
                SW_Domain->maxSimErrors
            );
            return;
        }
    } else {
        *runStatus = PRGRSS_FAIL;
#endif
    }

    /* Produce global error if all suids failed */
    if (nSims > 0 && nSims == mainLog->numDomainErrors) {
#if defined(SWMPI)
        if (nSims == SW_Domain->nActiveSuidsProc) {
#endif
            LogError(
                mainLog,
                LOGERROR,
                "All simulated units (n = %zu) produced errors.",
                nSims
            );
#if defined(SWMPI)
        }
#endif
    }

#if !defined(SWMPI)
    (void) runStatus;
    (void) SW_Domain;
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
static void begin_year(SW_RUN *sw, SW_OUT_DOM *OutDom, LOG_INFO *LogInfo) {
    // SW_F_new_year() not needed

    // call SW_MDL_new_year() first to set up time-related arrays for this year
    SW_MDL_new_year(&sw->ModelIn, &sw->ModelSim);

    // SW_MKV_new_year() not needed

    // SW_SKY_new_year(): Update daily climate variables from monthly values
    SW_SKY_new_year(
        &sw->ModelSim,
        sw->ModelSim.yearIdxSpinSim,
        sw->RunIn.SkyRunIn.snow_density,
        sw->RunIn.SkyRunIn.snow_density_daily
    );

    SW_VES_new_year(sw->VegEstabIn.count);

    // SW_VPD_new_year(): Dynamic CO2 effects on vegetation
    SW_VPD_new_year(
        sw->RunIn.weathRunAllHist,
        &sw->ModelSim,
        &sw->VegProdSim,
        &sw->SoilSim,
        sw->VegProdIn.isBiomAsIf100Cover,
        sw->VegProdIn.veg_method,
        sw->ModelSim.inputYearIdx,
        sw->VegProdIn.nYearsDynamicShort,
        sw->VegProdIn.nYearsDynamicLong,
        sw->SiteIn.methodMaxDepthSoilTemperature,
        &sw->RunIn.VegProdRunIn,
        sw->VegProdSim.veg,
        sw->VegProdIn.veg
    );

    SW_SIT_new_year(
        sw->SiteIn.methodMaxDepthSoilTemperature,
        sw->VegProdSim.annTempLongAvg,
        &sw->RunIn.SiteRunIn.Tsoil_constant
    );

    // SW_FLW_new_year() not needed

    SW_SWC_new_year(
        &sw->SoilWatIn,
        &sw->SoilWatSim,
        &sw->SiteSim,
        sw->ModelSim.year,
        sw->SiteIn.reset_yr,
        sw->RunIn.SiteRunIn.n_layers,
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
        sw->ModelSim.inputYearIdx,
        sw->ModelSim.lastdoy,
        LogInfo
    );
}

static void end_day(SW_RUN *sw, SW_OUT_DOM *OutDom, LOG_INFO *LogInfo) {
    TimeInt localTOffset = 1; // tOffset is one when called from this function

    if (sw->ModelSim.doOutput) {
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
    const TimeInt n_weathYears = 1;
    TimeInt n_years = source->ModelIn.endyr - source->ModelIn.startyr + 1;
    int **dummyExistYears = NULL;

    memcpy(dest, source, sizeof(*dest));

    dest->SoilWatIn.hist.file_prefix = NULL; /* currently unused */

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

    /* Copy weather generator parameters */
    if (dest->WeatherIn.generateWeatherMethod == 2) {
        copyMKV(&dest->MarkovIn, &source->MarkovIn);
    }

    SW_VPD_init_ptrs(&dest->VegProdSim);
    SW_VES_init_ptrs(&dest->VegEstabIn, dest->ves_p_accu, dest->ves_p_oagg);

    /* Copy vegetation establishment parameters */
    dest->VegEstabIn.count = source->VegEstabIn.count;
    memcpy(
        &dest->VegEstabIn.parms,
        &source->VegEstabIn.parms,
        sizeof(dest->VegEstabIn.parms)
    );

    memcpy(
        &dest->VegEstabSim.parms,
        &source->VegEstabSim.parms,
        sizeof(dest->VegEstabSim.parms)
    );

    SW_VegEstab_alloc_outptrs(
        dest->ves_p_accu, dest->ves_p_oagg, source->VegEstabIn.count, LogInfo
    );
    if (LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

    SW_CBN_alloc_ppm_existing_years(
        n_years, &dest->CarbonIn.ppm, dummyExistYears, LogInfo
    );
    if (LogInfo->stopRun) {
        return;
    }

    memcpy(
        dest->CarbonIn.ppm,
        source->CarbonIn.ppm,
        sizeof(*dest->CarbonIn.ppm) * n_years
    );

#if defined(SWNETCDF)
    SW_PATHOUT_deepCopy(
        &dest->SW_PathOutputs, &source->SW_PathOutputs, OutDom, LogInfo
    );
    if (LogInfo->stopRun) {
        return; // Exit prematurely due to error
    }

    SW_OUT_construct_outarray(1, OutDom, &dest->OutRun, LogInfo);
#else
    (void) OutDom;
#endif
}

/* =================================================== */
/*             Global Function Definitions             */
/* --------------------------------------------------- */

/**
@brief Calls 'SW_CTL_run_current_day' for each day in
the simulation start/end dates

@param[in] sw_template Template SW_RUN for the function to use as a
reference for local versions of SW_RUN
@param[in] tempVals An allocated space to store temporary input values
for converting and setting into proper location
@param[in] newSoils A temporary list of SW_SOIL_RUN_INPUTS instances to
store input values
@param[in] nActiveSites Number of active sites to simulate within a subdomain
(SWMPI) or a single site (SWTXT)
@param[in] SW_Domain Struct of type SW_DOMAIN holding constant
temporal/spatial information for a set of simulation runs
@param[in] updateNewYear A flag specifying if this is the first day
of the first year being run, and if so, being the first year
@param[in,out] nDaysInYear Number of days within the current year
@param[in,out] SW_Runs A list of SW_RUN instances of size "nActiveSites" that
will be used for holding all information for the simulation
@param[in] LogInfos A list of LOG_INFO instances of size "nActiveSites" that
will be used to store site log information through all daily runs
@param[out] main_LogInfo Holds information on warnings and errors
*/
void SW_CTL_sim_sites(
    SW_RUN *sw_template,
    double *tempVals,
    SW_SOIL_RUN_INPUTS *newSoils,
    size_t nActiveSites,
    SW_DOMAIN *SW_Domain,
    Bool updateNewYear,
    TimeInt *nDaysInYear,
    SW_RUN *SW_Runs,
    LOG_INFO *LogInfos,
    LOG_INFO *main_LogInfo
) {
#ifdef SWDEBUG
    int debug = 0;
#endif

    Bool sDom = SW_Domain->netCDFInput.siteDoms[eSW_InDomain];
    size_t site;
    TimeInt *doy = NULL;
    signed char *runStatus = NULL;
    TimeInt inputYearIdx;
    size_t baseSuid[NC_DIMS] = {0};
    size_t *suid = baseSuid;

#if defined(SWNETCDF)
    signed char *progVals = SW_Domain->netCDFInput.progVals;
    size_t actSiteIdx;
    Bool readConstInfo = swFALSE;
    Bool readWeather = SW_Domain->netCDFInput.readInVars[eSW_InWeather][0];
#else
    (void) tempVals;
    (void) newSoils;
    (void) sw_template;
#endif

    for (site = 0; site < nActiveSites; site++) {
        doy = &SW_Runs[site].ModelSim.doy;

#if defined(SWNETCDF)
        actSiteIdx = SW_Domain->actSiteIdx[eSW_InDomain][site];
        runStatus = &progVals[actSiteIdx];

        if (*runStatus == PRGRSS_FAIL) {
            continue;
        }

        suid = SW_Domain->globDomSuids[site];
#endif

        if ((*doy == 1 && !updateNewYear) || *doy == *nDaysInYear) {
            if (updateNewYear) {
                SW_Runs[site].ModelSim.year++;
            }

            begin_year(&SW_Runs[site], &SW_Domain->OutDom, &LogInfos[site]);
            checkJumpToLabel(LogInfos[site].stopRun, handleLog);

            *nDaysInYear = SW_Runs[site].ModelSim.lastdoy -
                           SW_Runs[site].ModelSim.firstdoy;

            inputYearIdx = SW_Runs[0].ModelSim.inputYearIdx;

#if defined(SWNETCDF)
            if (!SW_Domain->SW_SpinUp.spinup && site == 0) {
                if (readWeather) {
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
                    checkJumpToLabel(main_LogInfo->stopRun, handleLog);
                } else {
                    memcpy(
                        &SW_Runs[site].RunIn.weathRunAllHist[0],
                        &sw_template->RunIn.weathRunAllHist[inputYearIdx],
                        sizeof(SW_WEATHER_HIST)
                    );
                }
            }
#endif

            // finalize daily weather
            SW_WTH_finalize_yearly_weather(
                &SW_Runs[0].MarkovIn,
                &SW_Runs[0].WeatherIn,
                &SW_Runs[site].RunIn.weathRunAllHist[inputYearIdx],
                &SW_Runs[site].WeatherSim,
                SW_Runs[0].ModelSim.cum_monthdays,
                SW_Runs[0].ModelSim.days_in_month,
                suid,
                SW_Runs[site].ModelSim.year,
                SW_Runs[site].WeatherSim.trivialScaling,
                swFALSE, // Does not matter
                &LogInfos[site]
            );
            checkJumpToLabel(LogInfos[site].stopRun, handleLog);
        }

#ifdef SWDEBUG
        if (debug) {
            sw_printf("\n'SW_CTL_sim_sites': simulate site = %zu\n", site);
        }
#endif

        SW_CTL_run_current_day(
            &SW_Runs[site], &SW_Domain->OutDom, &LogInfos[site]
        );

    handleLog:
        handle_logs(&LogInfos[site], SW_Domain, site, runStatus, main_LogInfo);
        checkJumpToLabel(main_LogInfo->stopRun, reportLogs);
    }

reportLogs:
    report_logs(SW_Domain, LogInfos, sDom, nActiveSites);
}

/**
@brief Run through all daily time steps requested by the user, simulating
every site at once per daily time step

@param[in] sw_template Template SW_RUN for the function to use as a
reference for local versions of SW_RUN
@param[in] nDays Total number of days to simulate across all sites
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
    SW_RUN *sw_template,
    TimeInt nDays,
    double *tempVals,
    SW_SOIL_RUN_INPUTS *newSoils,
    SW_DOMAIN *SW_Domain,
    SW_RUN *SW_Runs,
    LOG_INFO *LogInfos,
    SW_WALLTIME *SW_WallTime,
    LOG_INFO *main_LogInfo
) {
    TimeInt day;
    TimeInt nDaysInYear = 0;
    Bool initFirstYear = swFALSE;

    for (day = 0; day < nDays; day++) {
#ifdef SWDEBUG
        if (debug) {
            sw_printf("\t: begin day = %d ... ", SW_Run->ModelSim.doy);
        }
#endif

        SW_CTL_sim_sites(
            sw_template,
            tempVals,
            newSoils,
            SW_Domain->nActiveSuidsProc,
            SW_Domain,
            initFirstYear,
            &nDaysInYear,
            SW_Runs,
            LogInfos,
            main_LogInfo
        );
        checkReturn(main_LogInfo->stopRun);

        initFirstYear = swTRUE;
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
    (void) signal(SIGINT, handle_interrupt);
    (void) signal(SIGTERM, handle_interrupt);

    if (main_LogInfo->printProgressMsg) {
        report_sim_start(SW_Domain, rank, worldSize);
    }

    const TimeInt numDaysToSim = Times_years_to_days(
        sw_template->ModelIn.startyr,
        sw_template->ModelIn.endyr,
        sw_template->ModelIn.startstart,
        sw_template->ModelIn.endend
    );

#if defined(SWNETCDF)
    const Bool alloc = swTRUE;
    const Bool dealloc = swFALSE;
    const Bool readConstInfo = swTRUE;

    SW_SOIL_RUN_INPUTS *newSoils = NULL;
    SW_RUN *siteRuns = NULL;
    LOG_INFO *siteLogs = NULL;
    double *tempVals = NULL;
    size_t site;
    size_t siteIdx;

    const size_t nActiveSites = SW_Domain->nActiveSuidsProc;

    handle_sim_structs_mem(
        alloc,
        nActiveSites,
        SW_Domain->hasConsistentSoilLayerDepths,
        &siteRuns,
        &newSoils,
        &siteLogs,
        main_LogInfo
    );
    checkReturn(main_LogInfo->stopRun);

    SW_NCIN_alloc_temp_inputs(SW_Domain, &tempVals, main_LogInfo);
    checkReturn(main_LogInfo->stopRun);

    init_all_logs(nActiveSites, main_LogInfo->logfp, siteLogs);

    init_all_runs(
        swTRUE,
        nActiveSites,
        &SW_Domain->OutDom,
        sw_template,
        siteRuns,
        main_LogInfo
    );
    checkReturn(main_LogInfo->stopRun);

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
    checkReturn(main_LogInfo->stopRun);

    // Check if any sites failed when reading initial values before
    // running simulations
    for (site = 0; site < nActiveSites; site++) {
        siteIdx = SW_Domain->actSiteIdx[eSW_InDomain][site];
        handle_logs(
            &siteLogs[site],
            SW_Domain,
            nActiveSites,
            &SW_Domain->netCDFInput.progVals[siteIdx],
            main_LogInfo
        );

        siteRuns[site].SiteSim.site_has_swrcpMineralSoil =
            sw_template->SiteIn.inputsProvideSWRCp;
    }
    checkReturn(main_LogInfo->stopRun);

    SW_CTL_run_daily_timesteps(
        sw_template,
        numDaysToSim,
        tempVals,
        newSoils,
        SW_Domain,
        siteRuns,
        siteLogs,
        SW_WallTime,
        main_LogInfo
    );

    handle_sim_structs_mem(
        dealloc,
        nActiveSites,
        SW_Domain->hasConsistentSoilLayerDepths,
        &siteRuns,
        &newSoils,
        &siteLogs,
        main_LogInfo
    );
#else
    SW_CTL_run_daily_timesteps(
        sw_template,
        numDaysToSim,
        NULL,
        NULL,
        SW_Domain,
        sw_template,
        main_LogInfo,
        SW_WallTime,
        main_LogInfo
    );
#endif
}

/**
@brief Initialize all possible pointers to NULL incase of an unexpected
program exit

@param[in,out] sw Comprehensive struct of type SW_RUN containing
    all information in the simulation
*/
void SW_CTL_init_ptrs(SW_RUN *sw) {
    SW_WTH_init_ptrs(&sw->RunIn.weathRunAllHist);
    SW_MKV_init_ptrs(&sw->MarkovIn);
    SW_VPD_init_ptrs(&sw->VegProdSim);
    SW_VES_init_ptrs(&sw->VegEstabIn, sw->ves_p_accu, sw->ves_p_oagg);
    SW_OUT_init_ptrs(&sw->OutRun, &sw->SW_PathOutputs);
    SW_SWC_init_ptrs(&sw->SoilWatIn, &sw->SoilWatSim);
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
    const int nDomFiles = 2;
    const Bool openInPar = swFALSE;
    const int openMode = NC_NOWRITE;
    const Bool domProgFileExists[] = {
        FileExists(SW_Domain->SW_PathInputs.ncInFiles[eSW_InDomain][vNCdom]),
        FileExists(SW_Domain->SW_PathInputs.ncInFiles[eSW_InDomain][vNCprog])
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

    for (file = 0; file < nDomFiles; file++) {
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
            case vNCprog:
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
    SW_MDL_construct(&sw->ModelSim);
    SW_WTH_construct(
        &sw->WeatherIn, &sw->WeatherSim, sw->weath_p_accu, sw->weath_p_oagg
    );

    // delay SW_MKV_construct() until we know from inputs whether we need it
    // SW_SKY_construct() not need
    SW_SIT_construct(&sw->SiteIn, &sw->SiteSim, &sw->RunIn.SiteRunIn.n_layers);
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
    SW_MKV_deconstruct(&sw->MarkovIn);
    // SW_SKY_INPUTS_deconstruct() not needed
    // SW_SIT_deconstruct() not needed
    SW_VES_deconstruct(sw->VegEstabIn.count, sw->ves_p_accu, sw->ves_p_oagg);
    SW_VPD_deconstruct(&sw->VegProdSim);
    // SW_FLW_deconstruct() not needed
    SW_SWC_deconstruct(&sw->SoilWatIn, &sw->SoilWatSim);
    SW_CBN_deconstruct(&sw->CarbonIn);
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
    SW_MDL_init_run(&sw->ModelSim, sw->ModelIn.startyr);
    SW_WTH_init_run(&sw->WeatherIn, &sw->WeatherSim);
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
        sw->VegProdIn.veg,
        sw->RunIn.SiteRunIn.n_layers,
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
        sw->VegEstabIn.count,
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
        sw->VegProdIn.veg,
        sw->VegProdSim.veg,
        &sw->CarbonIn,
        sw->ModelIn.startyr,
        sw->ModelIn.endyr,
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
    if (sw->VegProdIn.use_SWA) {
        calculate_repartitioned_soilwater(
            &sw->SoilWatSim,
            sw->SiteSim.swcBulk_atSWPcrit,
            &sw->VegProdIn,
            sw->RunIn.VegProdRunIn.veg,
            sw->RunIn.SiteRunIn.n_layers
        );
    }

    if (sw->VegEstabIn.use) {
        SW_VES_checkestab(
            sw->VegEstabIn.parms,
            sw->VegEstabSim.parms,
            sw->WeatherSim.temp_avg,
            sw->SoilWatSim.swcBulk,
            sw->ModelSim.doy,
            sw->ModelSim.firstdoy,
            sw->VegEstabIn.count
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
    if (sw->ModelSim.doOutput) {
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
            yr = (TimeInt) RandUniIntRange(
                sw->ModelIn.startyr, finalyr, &sw->ModelIn.SW_SpinUp.spinup_rng
            );
            years[i] = yr;
        }
        break;
    }

    TimeInt *cur_yr = &sw->ModelSim.year;
    TimeInt yrIdx;

    sw->ModelSim.doOutput = swFALSE; // turn output temporarily off

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
    sw->ModelSim.doOutput = prev_doOut; // reset doOutput to original value
    /* Note: don't reset sw->ModelSim.yearIdxSpinSim which is a
    continuous index across spinup and simulation years) */

    free(years);
}
}

/**
@brief Reads inputs from disk and makes a print statement if there is an error
        in doing so.

@param[in] rank Process number known to MPI for the current process (aka rank)
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
    int rank,
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
        sw->SiteIn.inputsProvideSWRCp,
        sw->RunIn.SoilRunIn.swrcpMineralSoil,
        sw->SiteIn.swrcpOM,
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
        rank,
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
        sw->ModelIn.startyr,
        sw->ModelIn.endyr,
        SW_PathInputs->txtInFiles,
        sw->VegProdIn.vegYear,
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

The following operations are conditional on if SWMPI is enabled
    - Read inputs (no SWMPI)
    - Copy suid-dependent information from an external structure
        (not sw_template, SWMPI)
    - Store output values into a bigger `p_OUT` structure (SWMPI)
        or
    - Write outputs directly to output files (no SWMPI)

@param[in] runNum Numerical placement of which run is currently being
    simulated within a group of simulations (base 0)
@param[in] runInputs An instance of SW_RUN_INPUTS to copy into
    the new instance that will be created (only with MPI enabled)
@param[in] sw_template Template SW_RUN for the function to use as a
    reference for local versions of SW_RUN
@param[in] SW_Domain Struct of type SW_DOMAIN holding constant
    temporal/spatial information for a set of simulation runs
@param[in] ncSuid Unique indentifier of the first suid to run
    in relation to netCDF gridcells/sites
@param[in] copyWeather Specifies if weather should be copied from
    template information; if SWMPI, swFALSE will copy it from `runInputs`
@param[in] count Default count values for the netCDF library
@param[in] tempVals A list that holds the maximum amount of elements
of all input keys
@param[out] SW_WallTime Struct of type SW_WALLTIME that holds timing
    information for the program run including partitioning into
    I/O and compute times
@param[out] LogInfo Holds information on warnings and errors
*/
void SW_CTL_run_sw(
    size_t runNum,
    SW_RUN_INPUTS *runInputs,
    SW_RUN *sw_template,
    SW_DOMAIN *SW_Domain,
    size_t ncSuid[], // NOLINT(readability-non-const-parameter)
    Bool copyWeather,
    size_t count[][2],
    double *tempVals, // NOLINT(readability-non-const-parameter)
    SW_WALLTIME *SW_WallTime,
    LOG_INFO *LogInfo
) {

#ifdef SWDEBUG
    int debug = 0;
#endif

    SW_RUN local_sw;

#if defined(SWNETCDF) && !defined(SWMPI)
    SW_SOIL_RUN_INPUTS newSoil;
    size_t starts[SW_NINKEYSNC][NC_DIMS] = {{0}};
    size_t counts[SW_NINKEYSNC][NC_DIMS] = {{0}};
    size_t numReads[SW_NINKEYSNC] = {1, 1, 1, 1, 1, 1, 1, 1};
    size_t suid[N_SUID_ASSIGN][2] = {{ncSuid[0], ncSuid[1]}};
#else
    (void) count;
    (void) runNum;
    (void) tempVals;
#endif

#if defined(SWTXT)
    (void) SW_WallTime;
#else
    WallTimeSpec tsr;
    Bool ok_tsr = swFALSE;
#endif

#ifdef SWDEBUG
    if (debug) {
        if (SW_Domain->netCDFInput.siteDoms[eSW_InDomain]) {
            sw_printf("SW_CTL_run_sw(): suid = %zu", ncSuid[0] + 1);
        } else {
            sw_printf(
                "SW_CTL_run_sw(): suid = [%zu, %zu]",
                ncSuid[1] + 1,
                ncSuid[0] + 1
            );
        }
    }
#endif

    // Copy template SW_RUN to local instance
    SW_RUN_deepCopy(
        sw_template,
        &local_sw,
        &SW_Domain->OutDom,
        runInputs,
        copyWeather,
        LogInfo
    );
    if (LogInfo->stopRun) {
        goto freeMem; // Free memory and skip simulation run
    }

#if defined(SWNETCDF) && !defined(SWMPI)
    set_walltime(&tsr, &ok_tsr);
    // Obtain suid-specific inputs
    // SW_NCIN_read_inputs(
    //     &local_sw,
    //     SW_Domain,
    //     ncSuid,
    //     starts,
    //     counts,
    //     SW_Domain->SW_PathInputs.openInFileIDs,
    //     numReads,
    //     1,
    //     tempVals,
    //     suid,
    //     &newSoil,
    //     &local_sw.RunIn,
    //     LogInfo,
    //     LogInfo
    // );
    SW_WT_TimeRun(tsr, ok_tsr, TIME_IO_IN, SW_WallTime);
    if (LogInfo->stopRun || !runSims) {
        goto freeMem;
    }
#endif

#if defined(SWNETCDF)
    local_sw.SiteSim.site_has_swrcpMineralSoil =
        sw_template->SiteIn.inputsProvideSWRCp;
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
    SW_CTL_init_run(&local_sw, LogInfo);
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
        SW_CTL_run_spinup(SW_Domain, &local_sw, LogInfo);
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

#if !defined(SWTXT)
    set_walltime(&tsr, &ok_tsr);
#endif
    // SW_CTL_main(&local_sw, &SW_Domain->OutDom, LogInfo);
    if (LogInfo->stopRun) {
        goto freeMem; // Free memory and exit function prematurely due to error
    }
#if !defined(SWTXT)
    SW_WT_TimeRun(tsr, ok_tsr, TIME_COMPUTE, SW_WallTime);
#endif

#if defined(SWNETCDF)
#ifdef SWDEBUG
    if (debug) {
        sw_printf(" -- nc-output");
    }
#endif

#if defined(SWMPI)
    SW_MPI_store_outputs(
        runNum,
        &SW_Domain->OutDom,
        local_sw.OutRun.p_OUT,
        sw_template->OutRun.p_OUT
    );
#else
    set_walltime(&tsr, &ok_tsr);
    SW_NCOUT_write_output(
        &SW_Domain->OutDom,
        local_sw.OutRun.p_OUT,
        local_sw.SW_PathOutputs.numOutFiles,
        local_sw.SW_PathOutputs.ncOutFiles,
        ncSuid,
        numReads[0],
        numReads[0],
        NULL,
        count,
        local_sw.SW_PathOutputs.openOutFileIDs,
        local_sw.SW_PathOutputs.ncOutVarIDs,
        SW_Domain->netCDFInput.siteDoms[eSW_InDomain],
        NULL,
        local_sw.SW_PathOutputs.outTimeSizes,
        LogInfo
    );
    SW_WT_TimeRun(tsr, ok_tsr, TIME_IO_OUT, SW_WallTime);
    (void) runNum;
#endif
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
