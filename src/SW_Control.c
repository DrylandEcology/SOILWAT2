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
@param[in] isSimDomDiscrete Is simulation domain discrete (site-based)?
    Otherwise, the simulation domain is gridded.
@param[in] globDomSuids A list of size nsites by NC_DIMS to
    hold precalculated global domain suids based on the assigned
    subdomain
@param[in] logfp The log file pointer used by the programs main instance
of LOG_INFO
@param[out] siteLogs A list of LOG_INFO instances of size "nActiveSites" that
will be returned with all instances initialized
*/
static void init_all_logs(
    size_t nActiveSites,
    Bool isSimDomDiscrete,
    size_t **globDomSuids,
    FILE *logfp,
    LOG_INFO *siteLogs
) {
    size_t site;

#if defined(SWNETCDF)
    size_t *suid;
#else
    size_t baseSuid[] = {0, 0}; // Zeroes for y/s and x dimensions
    size_t *suid = baseSuid;

    (void) globDomSuids;
#endif

    for (site = 0; site < nActiveSites; site++) {
#if defined(SWNETCDF)
        suid = globDomSuids[site];
#endif

        sw_init_logs(logfp, &siteLogs[site]);
        formatLogStage(
            siteLogs[site].logStage, sizeof siteLogs[site].logStage, "setup"
        );

        // NOLINTNEXTLINE(clang-analyzer-core.NullDereference)
        siteLogs[site].ncSUID[0] = suid[0];
        siteLogs[site].ncSUID[1] = isSimDomDiscrete ? 0 : suid[1];
    }
}

/**
@brief Initialize all site simulation information

@param[in] rank Process number known to MPI for the current process (aka rank);
defaults to 0 (main process) if we are running sequentially
@param[in] copyWeatherHist Specifies if the weather data should be copied;
this only has the chance to be false when the program is dealing with
nc inputs
@param[in] nActiveSites Number of active sites to initialize log
information for
@param[in] tempVals An allocated space to store temporary input values
for converting and setting into proper location
@param[in] SW_Domain Struct of type SW_DOMAIN holding constant
temporal/spatial information for a set of simulation runs
@param[in] sw_template Template SW_RUN for the function to use as a
reference for local versions of SW_RUN
@param[out] SW_Runs A list of SW_RUN instances of size "nActiveSites" that
will be returned with deep copied information from the main SW_RUN
and initialized runs
@param[out] newSoils A single (no SWMPI) or a list (SWMPI) of instances of
    SW_SOIL_RUN_INPUTS used as temporary storage when reading inputs
@param[out] siteLogs A list of LOG_INFO of size [n active sites] that will
be returned with any site-specific errors/warnings
@param[out] main_LogInfo The main LOG_INFO instance for the program
*/
static void init_all_runs(
    int rank,
    Bool copyWeatherHist,
    size_t nActiveSites,
    double *tempVals,
    SW_DOMAIN *SW_Domain,
    SW_RUN *sw_template,
    SW_RUN *SW_Runs,
    SW_SOIL_RUN_INPUTS *newSoils,
    LOG_INFO *siteLogs,
    LOG_INFO *main_LogInfo
) {
    size_t site;
    Bool fatalError = swTRUE;

#if defined(SWNETCDF)
    const Bool readConstInfo = swTRUE;

    SW_OUT_construct_outarray(
        &SW_Domain->OutDom, sw_template->OutRun, main_LogInfo
    );
#else
    (void) SW_Domain;
#endif

    /*
        Consideration for future development: Modify the program so that
        one allocation call is necessary for all site information, so we
        don't do many smaller allocations
    */
    for (site = 0; site < nActiveSites; site++) {
        SW_RUN_deepCopy(
            sw_template, &SW_Runs[site], copyWeatherHist, main_LogInfo
        );
        checkJumpToLabel(main_LogInfo->stopRun, checkLogs);

        SW_Runs[site].RunInfo.siteIndex =
            SW_Domain->actSiteIdx[eSW_InDomain][site];
        SW_Runs[site].RunInfo.nSites = SW_Domain->nSitesInSubDom;
    }

#if defined(SWNETCDF)
    SW_NCIN_read_inputs(
        SW_Runs,
        SW_Domain,
        readConstInfo,
        SW_Domain->SW_PathInputs.openInFileIDs,
        tempVals,
        nActiveSites,
        newSoils,
        siteLogs,
        main_LogInfo
    );
    checkJumpToLabel(main_LogInfo->stopRun, checkLogs);
#else
    (void) newSoils;
#endif

    for (site = 0; site < nActiveSites; site++) {
#if defined(SWNETCDF)
        SW_Runs[site].SiteSim.site_has_swrcpMineralSoil =
            sw_template->SiteIn->inputsProvideSWRCp;
#endif

        SW_CTL_init_run(&SW_Runs[site], &siteLogs[site], main_LogInfo);
        checkJumpToLabel(main_LogInfo->stopRun, checkLogs);

        if (SW_Domain->SW_SpinUp.spinup) {
            formatLogStage(
                siteLogs[site].logStage,
                sizeof siteLogs[site].logStage,
                "spinup"
            );
        }
    }
    if (SW_Domain->SW_SpinUp.spinup &&
        !SW_Domain->SW_ConstInfo.ModelSim.progRestarted) {

        SW_CTL_run_spinup(
            rank, SW_Domain, tempVals, SW_Runs, siteLogs, main_LogInfo
        );
    }

    fatalError = swFALSE;

checkLogs:
    SW_F_check_site_logs(fatalError, SW_Domain, siteLogs, main_LogInfo);
}

/**
@brief Allocate/deallocate information necessary for running simulations, i.e.,
SW_RUN, SW_RUN_INPUTS, LOG_INFO for each active site

@param[in] allocate A flag specifying if the structs should be allocated
(swTRUE) or deallocated (swFALSE)
@param[in] nActiveSites Number of active sites to allocate for
@param[out] SW_Runs A list of SW_RUN instances of size "nActiveSites"
@param[out] siteLogs A list of LOG_INFO instances of size "nActiveSites"
@param[out] mainLogInfo The main LOG_INFO instance for the program
*/
static void handle_sim_structs_mem(
    Bool allocate,
    size_t nActiveSites,
    SW_RUN **SW_Runs,
    LOG_INFO **siteLogs,
    LOG_INFO *main_LogInfo
) {
    const Bool fullReset = swFALSE;
    const int runIndex = 0;
    const int numDeallocArrays = 2;
    void **deallocArrays[] = {(void **) SW_Runs, (void **) siteLogs};

#if defined(SWNETCDF)
    size_t n_years = 1;
#endif

    int arr;
    size_t site;

    if (allocate) {
        *SW_Runs = (SW_RUN *) Mem_Malloc(
            sizeof(SW_RUN) * nActiveSites, "alloc_sim_structs", main_LogInfo
        );
        checkReturn(main_LogInfo->stopRun);

#if defined(SWNETCDF)
        for (site = 0; site < nActiveSites && !main_LogInfo->stopRun; site++) {
            SW_WTH_allocateAllWeather(
                &(*SW_Runs)[site].RunIn.weathRunAllHist, n_years, main_LogInfo
            );
        }
        checkReturn(main_LogInfo->stopRun);
#endif

        *siteLogs = (LOG_INFO *) Mem_Malloc(
            sizeof(LOG_INFO) * nActiveSites, "alloc_sim_structs", main_LogInfo
        );
    } else {
        for (arr = 0; arr < numDeallocArrays; arr++) {
            if (!isnull(*(deallocArrays[arr]))) {
                if (arr == runIndex) {
                    for (site = 0; site < nActiveSites; site++) {
                        SW_CTL_clear_model(fullReset, &((*SW_Runs)[site]));
                    }
                }

                free(*(deallocArrays[arr]));
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
    SW_MSG_ROOT("is running simulations across the domain ...", rank);

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
            "%d %s ...",
            SW_Domain->nActiveSuidsTot,
            worldSize,
            (worldSize > 1) ? "processes" : "process"
        );

        SW_MSG_ROOT(reportStr, rank);
    }
#endif
}

#if defined(SOILWAT) && defined(SWNETCDF)
/**
@brief Display the number of years that have been simulated by using
a displace of consecutive periods

@param[in] rank Process number known to MPI for the current process (aka rank);
    defaults to 0 (main process) if we are running sequentially
@param[in] nYears Number of years to display
@param[in] startup A flag specifying if the program is starting up
@param[in] inSpinup Whether the simulation is currently in spinup
@param[in] finalYear A flag specifying if the last year that was simulated
is the last in the simulation
@param[in] displayYears A flag specifying if it is the end of the program
and thus should print out how many years should be printed out
@param[in] finalSpinupYear Specifies if the current year dot is going to be
meant for the last year of the spinup year
*/
static void display_yearly_progress(
    int rank,
    TimeInt nYears,
    Bool startup,
    Bool inSpinup,
    Bool finalYear,
    Bool displayYears,
    Bool finalSpinupYear
) {
    TimeInt year;
    TimeInt numPrintYears = (finalYear || finalSpinupYear) ? 1 : nYears;

    if (rank == ROOT_PROC) {
        if (startup) {
            if (inSpinup) {
                SW_MSG_ROOT("Spinup status", ROOT_PROC);
            } else {
                SW_MSG_ROOT("Yearly simulation status", ROOT_PROC);
            }
        }

        if (!displayYears || finalYear) {
            for (year = 0; year < numPrintYears; year++) {
#if defined(SWMPI)
                sw_printf(".\n");
#else
                sw_printf(".");
#endif
            }
        }

        if (displayYears || finalSpinupYear) {
            sw_printf(
                " (%u %s completed)\n", nYears, (nYears == 1) ? "year" : "years"
            );
        } else if (finalSpinupYear) {
            // Create new line for following dots representing simulation years
            sw_printf("\n");
        }

        (void) fflush(stdout);
    }
}
#endif

/**
@brief Initiate/update variables for a new simulation year. This
function only updates constant simulation information once for
all sites. In addition to the timekeeper (Model), usually only modules
that read input yearly or produce output need to have this call.

@param[in,out] sw Comprehensive struct of type SW_RUN containing all
  information in the simulation
@param[in] textSkyVals A flag specifying if the sky values that will
be used during simulation are text-based (swTRUE) or through
netCDFs (swFALSE)
@param[out] LogInfo Holds information on warnings and errors
*/
static void begin_year_const(SW_RUN *sw, Bool textSkyVals, LOG_INFO *LogInfo) {
    // SW_F_new_year() not needed

    // call SW_MDL_new_year() first to set up time-related arrays for this
    // year
    SW_MDL_new_year(sw->ModelIn, sw->ModelSim);

    // SW_MKV_new_year() not needed

    SW_VES_new_year(sw->VegEstabIn.count);

    SW_SWC_new_year_const(sw->SoilWatIn, sw->ModelSim->year, LogInfo);
    if (LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

    // SW_CBN_new_year() not needed
    // SW_OUT_new_year() not needed

    if (textSkyVals) {
        // SW_SKY_new_year(): Update daily climate variables from monthly values
        SW_SKY_new_year(
            sw->ModelSim,
            sw->ModelSim->yearIdxSpinSim,
            sw->RunIn.SkyRunIn.snow_density,
            sw->RunIn.SkyRunIn.snow_density_daily
        );
    }
}

/**
@brief Initiate/update variables for a new simulation year. This
function only updates site-based simulation information. This usually
only modules that read input yearly or produce output need to have this call.

@param[in,out] sw Comprehensive struct of type SW_RUN containing all
  information in the simulation
@param[in] SW_SkyRunIn Instance of SW_SKY_INPUTS which resides in template
  instance of SW_RUN
@param[in] textSkyVals A flag specifying if the sky values that will
be used during simulation are text-based (swTRUE) or through
netCDFs (swFALSE)
@param[out] siteLog Site-specific instance of LOG_INFO
*/
static void begin_year_site(
    SW_RUN *sw, SW_SKY_INPUTS *SW_SkyRunIn, Bool textSkyVals, LOG_INFO *siteLog
) {
    SW_SWC_new_year_site(
        &sw->SoilWatSim,
        &sw->SiteSim,
        sw->SiteIn->reset_yr,
        sw->RunIn.SiteRunIn.n_layers
    );

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
        &sw->VegProdIn->veg,
        siteLog
    );

    if (textSkyVals) {
        Mem_Copy(
            sw->RunIn.SkyRunIn.snow_density_daily,
            SW_SkyRunIn->snow_density_daily,
            sizeof(double) * (MAX_DAYS + 1)
        );
    } else {
        // SW_SKY_new_year(): Update daily climate variables from monthly values
        SW_SKY_new_year(
            sw->ModelSim,
            sw->ModelSim->yearIdxSpinSim,
            sw->RunIn.SkyRunIn.snow_density,
            sw->RunIn.SkyRunIn.snow_density_daily
        );
    }

    // SW_FLW_new_year() not needed
    SW_SIT_new_year(
        sw->SiteIn->methodMaxDepthSoilTemperature,
        sw->VegProdSim.annTempLongAvg,
        &sw->RunIn.SiteRunIn.Tsoil_constant
    );
}

static void begin_day_const(SW_MODEL_SIM *SW_ModelSim, SW_OUT_RUN *OutRun) {
    SW_MDL_new_day(SW_ModelSim);
    SW_OUT_new_day(SW_ModelSim, OutRun);
}

static void begin_day_site(SW_RUN *sw, LOG_INFO *LogInfo) {
    SW_WTH_new_day(
        sw->WeatherIn,
        &sw->WeatherSim,
        sw->RunIn.weathRunAllHist,
        sw->SiteIn,
        sw->SoilWatSim.snowpack,
        sw->ModelSim->doy,
        sw->ModelSim->inputYearIdx,
        sw->ModelSim->lastdoy,
        LogInfo
    );
}

static void end_day(SW_RUN *sw, SW_OUT_DOM *OutDom, LOG_INFO *LogInfo) {
    if (sw->ModelSim->doOutput) {
        collect_values(sw, OutDom, LogInfo);
        if (LogInfo->stopRun) {
            return; // Exit function prematurely due to error
        }
    }

    SW_SWC_end_day(&sw->SoilWatSim, sw->RunIn.SiteRunIn.n_layers);
}

/**
@brief Interface function to reduce the complexity of running multiple sites
if only one site is needed, namely when using STEPWAT2/rSOILWAT2 and testing

@param[in] startYear Start year of the simulation
@param[in] endYear End year of the simulation (inclusive)
@param[in] SW_Domain Struct of type SW_DOMAIN holding constant
temporal/spatial information for a set of simulation runs
@param[out] SW_Run Single instance of SW_RUN to simulate
@param[out] LogInfo Holds information on warnings and errors
*/
void SW_CTL_run_single_site(
    TimeInt startYear,
    TimeInt endYear,
    SW_DOMAIN *SW_Domain,
    SW_RUN *SW_Run,
    LOG_INFO *LogInfo
) {
    const TimeInt startDay = 0;

    double *tempVals = NULL;
    SW_WALLTIME *wt = NULL;

    TimeInt year;
    TimeInt nDays = 0;
    TimeInt nDaysInYear;

    for (year = startYear; year <= endYear; year++) {
        nDaysInYear = Time_get_lastdoy_y(year);

        if (year > SW_Domain->startyr && year < SW_Domain->endyr) {
            nDays += nDaysInYear;
        } else if (year == SW_Domain->startyr) {
            nDays += (nDaysInYear - SW_Domain->startstart);
        } else { /* End year */
            nDays += SW_Domain->endend;
        }
    }

    SW_CTL_run_daily_timesteps(
        ROOT_PROC,
        SW_Run,
        startDay,
        nDays,
        NO_IO_TIMING,
        tempVals,
        SW_Domain,
        SW_Run,
        LogInfo,
        wt,
        LogInfo
    );
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
    TimeInt year;

#if defined(SWNETCDF)
    const TimeInt n_weathYears = 1;
    SW_WEATHER_HIST *weathPtr = dest->RunIn.weathRunAllHist;
#else
    const TimeInt n_weathYears =
        source->ModelIn->endyr - source->ModelIn->startyr + 1;
#endif
    const IntU prevEstabCount = source->VegEstabIn.count;

    memcpy(dest, source, sizeof(*dest));

    dest->SoilWatIn->hist.file_prefix = NULL; /* currently unused */

#if defined(SWNETCDF)
    dest->RunIn.weathRunAllHist = weathPtr;
#else
    /* Allocate memory and copy daily weather */
    dest->RunIn.weathRunAllHist = NULL;

    SW_WTH_allocateAllWeather(
        &dest->RunIn.weathRunAllHist, n_weathYears, LogInfo
    );
    checkReturn(LogInfo->stopRun);
#endif

    if (copyWeatherHist) {
        for (year = 0; year < n_weathYears; year++) {
            Mem_Copy(
                &dest->RunIn.weathRunAllHist[year],
                &source->RunIn.weathRunAllHist[year],
                sizeof(SW_WEATHER_HIST)
            );
        }
    }

    /* Copy weather generator parameters */
    if (dest->WeatherIn->generateWeatherMethod == wgMKV) {
        allocateMKV(&dest->MarkovIn, LogInfo);
        if (LogInfo->stopRun) {
            return;
        }

        copyMKV(&dest->MarkovIn, &source->MarkovIn);
    }

    /* Copy vegetation parameters */
    SW_VPD_init_ptrs(&dest->VegProdSim);

    /* Copy vegetation establishment parameters */
    SW_VES_init_ptrs(&dest->VegEstabIn, dest->ves_p_accu, dest->ves_p_oagg);
    dest->VegEstabIn.count = prevEstabCount;

    memcpy(
        &dest->VegEstabSim.parms,
        &source->VegEstabSim.parms,
        sizeof(dest->VegEstabSim.parms)
    );

    SW_VegEstab_alloc_outptrs(
        dest->ves_p_accu, dest->ves_p_oagg, source->VegEstabIn.count, LogInfo
    );
}

/**
@brief Prepare necessary information for the next day of the simulation

@param[in] sw_template Template SW_RUN for the function to use as a
reference for local versions of SW_RUN
@param[in] SW_Domain Struct of type SW_DOMAIN holding constant
temporal/spatial information for a set of simulation runs
@param[in] siteLogs A list of LOG_INFO instances of size "nActiveSites" that
will be used to store site log information through all daily runs
@param[in] tempVals An allocated space to store temporary input values
for converting and setting into proper location
@param[in] initYear A flag specifying if it is the first day of the
program run and we need to read and/or setup weather
@param[in] doIOPlusTiming A flag specifying that this function is to perform
I/O and timing operations
@param[in,out] SW_Runs A list of SW_RUN instances of size "nActiveSites" that
will be used for holding all information for the simulation
@param[out] SW_WallTime Struct of type SW_WALLTIME that holds timing
information for the program run
@param[out] main_LogInfo Holds information on warnings and errors
*/
static void prepare_next_day(
    SW_RUN *sw_template,
    SW_DOMAIN *SW_Domain,
    LOG_INFO *siteLogs,
    const double *tempVals,
    Bool initYear,
    Bool doIOPlusTiming,
    SW_RUN *SW_Runs,
    SW_WALLTIME *SW_WallTime,
    LOG_INFO *main_LogInfo
) {
#ifdef SWDEBUG
    int debug = 0;
#endif
    const TimeInt n_years = 1;

    Bool textSkyVals = swTRUE;

#if defined(SWNETCDF)
    const Bool readWeather =
        SW_Domain->netCDFInput.readInVars[eSW_InWeather][0];
    const Bool readConstInfo = swFALSE;

    WallTimeSpec tsr;
    Bool ok_tsr = swFALSE;
    SW_SOIL_RUN_INPUTS *nullSoils = NULL;
    TimeInt inputIdx;

    textSkyVals = (Bool) !SW_Domain->netCDFInput.readInVars[eSW_InClimate][0];
#endif

    size_t site;
    size_t nActiveSites = SW_Domain->nActiveSuidsProc;
    TimeInt inputYearIdx = SW_Domain->SW_ConstInfo.ModelSim.inputYearIdx;

    TimeInt *doy = &SW_Domain->SW_ConstInfo.ModelSim.doy;
    TimeInt lastDoy = SW_Domain->SW_ConstInfo.ModelSim.lastdoy;
    TimeInt *year = &SW_Domain->SW_ConstInfo.ModelSim.year;
    Bool fatalError = swTRUE;
    Bool newYear = (Bool) (initYear || *doy == lastDoy + 1);
    Bool initVPD = initYear;

#ifdef SWDEBUG
    if (debug) {
        sw_printf("\t: begin day = %d ... ", *doy);
    }
#endif

    if (newYear) {
        if (!initYear && *doy == lastDoy + 1) {
            (*year)++;
        }

        begin_year_const(sw_template, textSkyVals, main_LogInfo);
        checkReturn(main_LogInfo->stopRun);

#if defined(SWNETCDF)
        for (site = 0; site < nActiveSites; site++) {
            formatLogStage(
                siteLogs[site].logStage, sizeof siteLogs[site].logStage, "input"
            );
        }

        if (readWeather) {
            if (doIOPlusTiming) {
                set_walltime(&tsr, &ok_tsr);
            }
            SW_NCIN_read_inputs(
                SW_Runs,
                SW_Domain,
                readConstInfo,
                SW_Domain->SW_PathInputs.openInFileIDs,
                (double *) tempVals,
                SW_Domain->nActiveSuidsProc,
                nullSoils,
                siteLogs,
                main_LogInfo
            );
            if (doIOPlusTiming) {
                SW_WT_TimeRun(tsr, ok_tsr, TIME_IO_IN, SW_WallTime);
            }
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
#endif

        for (site = 0; site < nActiveSites; site++) {
#if !defined(SWNETCDF)
            formatLogStage(
                siteLogs[site].logStage, sizeof siteLogs[site].logStage, "input"
            );
#endif

            if (!siteLogs[site].stopRun) {
                // finalize daily weather
                SW_WTH_finalize_yearly_weather(
                    &SW_Runs[site].MarkovIn,
                    SW_Runs[site].WeatherIn,
                    &SW_Runs[site].RunIn.weathRunAllHist[inputYearIdx],
                    &SW_Runs[site].WeatherSim,
                    SW_Runs[site].ModelSim->cum_monthdays,
                    SW_Runs[site].ModelSim->days_in_month,
                    SW_Runs[site].ModelSim->year,
                    n_years,
                    SW_Domain->startstart,
                    SW_Domain->endend,
                    SW_Domain->startyr,
                    SW_Domain->endyr,
                    SW_Runs[site].WeatherSim.trivialScaling,
                    &siteLogs[site]
                );
            }

            // Initialize VPD since it has to have the first year of weather
            // (assuming the simulation just started)
            if (initVPD) {
                SW_VPD_init_run_calc(&SW_Runs[site], &siteLogs[site]);
            }
        }
    }
    fatalError = swFALSE;

    begin_day_const(
        &SW_Domain->SW_ConstInfo.ModelSim, &SW_Domain->SW_ConstInfo.OutRun
    );

#if defined(SWNETCDF)
handleLogs:
#endif

    SW_F_check_site_logs(fatalError, SW_Domain, siteLogs, main_LogInfo);

#if !defined(SWNETCDF)
    (void) doIOPlusTiming;
    (void) sw_template;
    (void) tempVals;
    (void) main_LogInfo;
    (void) SW_WallTime;
#endif
}

/**
@brief Attempt to output values if necessary (SWNETCDF mode only)

@param[in] rank Process number known to MPI for the current process (aka rank)
@param[in] forceOutput A flag specifying if output should be forced due to it
being the last day of the simulation
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
    Bool forceOutput,
    SW_RUN *sw_template,
    SW_DOMAIN *SW_Domain,
    SW_WALLTIME *SW_WallTime,
    LOG_INFO *main_LogInfo
) {
    const TimeInt *doy = &SW_Domain->SW_ConstInfo.ModelSim.doy;
    const TimeInt lastDoy = SW_Domain->SW_ConstInfo.ModelSim.lastdoy;

#if defined(SW_OUTARRAY)
    const Bool inSpinup = SW_Domain->SW_ConstInfo.ModelSim.inSpinup;

    SW_OUT_DOM *OutDom = &SW_Domain->OutDom;
    SW_OUT_RUN *OutRun = &SW_Domain->SW_ConstInfo.OutRun;
    OutPeriod p;

    OutKey outKey;
#endif

#if defined(SWNETCDF)
    const TimeInt input_n_years = 1;
    const Bool displayNYears = swFALSE;
    const Bool startupPrint = swFALSE;
    const Bool finalYear = swFALSE;
    const Bool finalSpinupYear =
        (Bool) (inSpinup && SW_Domain->SW_ConstInfo.ModelSim.yearIdxSpinSim ==
                                (int) SW_Domain->SW_SpinUp.duration - 1);
    const TimeInt nYears =
        (finalSpinupYear) ? SW_Domain->SW_SpinUp.duration : 1;

    WallTimeSpec tsr;
    Bool ok_tsr = swFALSE;

    Bool forceWriteOut =
        (Bool) (forceOutput || !runSims || main_LogInfo->stopRun);

    if (!inSpinup) {
        formatLogStage(
            main_LogInfo->logStage, sizeof main_LogInfo->logStage, "output"
        );

        set_walltime(&tsr, &ok_tsr);
        SW_NCOUT_write_output(
            &SW_Domain->OutDom,
            sw_template->OutRun->p_OUT,
            sw_template->SW_PathOutputs->numOutFiles,
            SW_Domain->nSitesInSubDom,
            SW_Domain->domStartIndex[eSW_InDomain],
            SW_Domain->domCounts[eSW_InDomain],
            sw_template->SW_PathOutputs->openOutFileIDs,
            sw_template->SW_PathOutputs->ncOutVarIDs,
            SW_Domain->isSimDomDiscrete,
            forceWriteOut,
            SW_Domain->SW_ConstInfo.ModelSim.endperiod,
            SW_Domain->SW_ConstInfo.OutRun.irow_OUT,
            sw_template->SW_PathOutputs->outTimeSizes,
            main_LogInfo
        );
        SW_WT_TimeRun(tsr, ok_tsr, TIME_IO_OUT, SW_WallTime);
    }
#endif

    if (*doy == lastDoy) {
        SW_Domain->SW_ConstInfo.ModelSim.inputYearIdx++;

#if defined(SWNETCDF)
        SW_Domain->SW_ConstInfo.ModelSim.inputYearIdx %= input_n_years;
#endif
    }

    SW_Domain->SW_ConstInfo.ModelSim.doy++;

#if defined(SW_OUTARRAY)
    if (!inSpinup) {
        // increment row counts
        ForEachOutKey(outKey) {
            ForEachOutPeriod(p) {
                if (OutDom->use_OutPeriod[p] && OutRun->writeit[p]) {
#if defined(SWNETCDF)
                    OutRun->irow_OUT[outKey][p] =
                        (OutRun->irow_OUT[outKey][p] + 1) %
                        OutDom->nrow_OUT[outKey][p];
#else
                    OutRun->irow_OUT[outKey][p]++;
#endif
                }
            }
        }
    }
#endif

#if defined(SWNETCDF)
    if (*doy == lastDoy + 1) {
        display_yearly_progress(
            rank,
            nYears,
            startupPrint,
            inSpinup,
            finalYear,
            displayNYears,
            finalSpinupYear
        );
    }
#else
    (void) rank;
    (void) forceOutput;
    (void) sw_template;
    (void) SW_Domain;
    (void) main_LogInfo;
    (void) SW_WallTime;
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
@param[in] SW_Domain Struct of type SW_DOMAIN holding constant
temporal/spatial information for a set of simulation runs
@param[in,out] SW_Runs A list of SW_RUN instances of size "nActiveSites" that
will be used for holding all information for the simulation
@param[in] siteLogs A list of LOG_INFO instances of size "nActiveSites" that
will be used to store site log information through all daily runs
@param[in] initYear A flag specifying if it is the first day of the
program run and we need to read and/or setup weather
@param[out] main_LogInfo Holds information on warnings and errors
*/
void SW_CTL_sim_sites(
    SW_RUN *sw_template,
    SW_DOMAIN *SW_Domain,
    SW_RUN *SW_Runs,
    Bool initYear,
    LOG_INFO *siteLogs,
    LOG_INFO *main_LogInfo
) {
#ifdef SWDEBUG
    int debug = 0;
#endif

    size_t nActiveSites = SW_Domain->nActiveSuidsProc;

    size_t site;
    TimeInt doy = SW_Domain->SW_ConstInfo.ModelSim.doy;
    TimeInt firstDoy = SW_Domain->SW_ConstInfo.ModelSim.firstdoy;
    signed char *runStatus = NULL;
    Bool textSkyVals = swTRUE;

#if defined(SWNETCDF)
    const Bool spinup = SW_Domain->SW_SpinUp.spinup;
    signed char *progVals = SW_Domain->netCDFInput.progVals;
    size_t siteIdx;

    textSkyVals = (Bool) !SW_Domain->netCDFInput.readInVars[eSW_InClimate][0];
#else
    (void) sw_template;
#endif

    for (site = 0; site < nActiveSites; site++) {
        formatLogStage(
            main_LogInfo->logStage, sizeof main_LogInfo->logStage, "simulation"
        );

#if defined(SWNETCDF)
        siteIdx = SW_Runs[site].RunInfo.siteIndex;
        runStatus = &progVals[siteIdx];

        if (*runStatus == PRGRSS_FAIL) {
            continue;
        }
#endif

        if (initYear || doy == firstDoy) {
            begin_year_site(
                &SW_Runs[site],
                &sw_template->RunIn.SkyRunIn,
                textSkyVals,
                &siteLogs[site]
            );
            if (siteLogs[site].stopRun) {
                goto countLogs;
            }
        }

#ifdef SWDEBUG
        if (debug) {
            sw_printf("\n'SW_CTL_sim_sites': simulate site = %zu\n", site);
        }
#endif

        SW_CTL_run_current_day(
            &SW_Runs[site], &SW_Domain->OutDom, &siteLogs[site]
        );

    countLogs:
        SW_F_handle_log_counts(&siteLogs[site], runStatus, main_LogInfo);

#if defined(SWNETCDF)
        if (siteLogs[site].stopRun && !spinup) {
            SW_NCOUT_reset_failed_sites(
                SW_Domain, siteIdx, sw_template->OutRun->p_OUT
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
@param[in] doIOPlusTiming A flag specifying that this function is to perform
I/O and timing operations
@param[in] tempVals An allocated space to store temporary input values
for converting and setting into proper location
@param[in] SW_Domain Struct of type SW_DOMAIN holding constant
temporal/spatial information for a set of simulation runs
@param[in,out] SW_Runs A list of SW_RUN instances of size "nActiveSites" that
will be used for holding all information for the simulation
@param[in] siteLogs A list of LOG_INFO instances of size "nActiveSites" that
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
    Bool doIOPlusTiming,
    double *tempVals,
    SW_DOMAIN *SW_Domain,
    SW_RUN *SW_Runs,
    LOG_INFO *siteLogs,
    SW_WALLTIME *SW_WallTime,
    LOG_INFO *main_LogInfo
) {
#ifdef SWDEBUG
    int debug = 0;
#endif

    TimeInt day;
    Bool initYear = swFALSE;

#if defined(SWNETCDF)
    WallTimeSpec tsr;
    Bool ok_tsr = swFALSE;
#endif

    for (day = startDay; day <= endDay && runSims; day++) {
        initYear = (Bool) (day == startDay);
        prepare_next_day(
            sw_template,
            SW_Domain,
            siteLogs,
            tempVals,
            initYear,
            doIOPlusTiming,
            SW_Runs,
            SW_WallTime,
            main_LogInfo
        );
        checkJumpToLabel(main_LogInfo->stopRun || !runSims, handleOutput);

        if (runSims) {
#if defined(SWNETCDF)
            if (doIOPlusTiming) {
                set_walltime(&tsr, &ok_tsr);
            }
#endif
            SW_CTL_sim_sites(
                sw_template,
                SW_Domain,
                SW_Runs,
                initYear,
                siteLogs,
                main_LogInfo
            );
#if defined(SWNETCDF)
            if (doIOPlusTiming) {
                SW_WT_TimeRun(tsr, ok_tsr, TIME_COMPUTE, SW_WallTime);
            }
#endif

#ifdef SWDEBUG
            if (debug) {
                sw_printf(
                    "doy = %d completed.\n",
                    SW_Domain->SW_ConstInfo.ModelSim.doy
                );
            }
#endif
        }

    handleOutput:
        finalize_sites_day(
            rank,
            (Bool) (day == endDay),
            sw_template,
            SW_Domain,
            SW_WallTime,
            main_LogInfo
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

    SW_SOIL_RUN_INPUTS *newSoils = NULL;
    SW_RUN *siteRuns = NULL;
    LOG_INFO *siteLogs = NULL;
    double *tempVals = NULL;
    Bool copyWeatherHist = swTRUE;

    Bool progRestart = swFALSE;
    Bool isSimDomDiscrete = SW_Domain->isSimDomDiscrete;

#if defined(SWNETCDF)
    const Bool readCache = swTRUE;
    const char *cacheFileName = SW_Domain->SW_PathInputs.txtInFiles[eNCCache];
    const Bool displayNYearsBeforeSim = swFALSE;
    const Bool displayNYearsAfterSim = swTRUE;
    const Bool finalSpinUpYr = swFALSE;
    const Bool inSpinup = swFALSE;
    Bool startupPrint;
    Bool freshRun = (Bool) (SW_Domain->startSimDay == SW_Domain->startstart);
    Bool readFromCacheFile = FileExists(cacheFileName);
    Bool fullFinalYear = swFALSE;
    Bool cacheAtEnd = swFALSE;
    TimeInt *year = &SW_Domain->SW_ConstInfo.ModelSim.year;
    TimeInt nYears;
    TimeInt *doy = &SW_Domain->SW_ConstInfo.ModelSim.doy;
    TimeInt *lastDoy = &SW_Domain->SW_ConstInfo.ModelSim.lastdoy;

    copyWeatherHist =
        (Bool) !SW_Domain->netCDFInput.readInVars[eSW_InWeather][0];

    progRestart = (Bool) (readFromCacheFile && !freshRun);
    startupPrint = (Bool) (progRestart || !inSpinup);

#if defined(SWMPI)
    MPI_Barrier(MPI_COMM_WORLD);
#endif
#endif

    (void) signal(SIGINT, handle_interrupt);
    (void) signal(SIGTERM, handle_interrupt);

    handle_sim_structs_mem(
        alloc, nActiveSites, &siteRuns, &siteLogs, main_LogInfo
    );
    checkReturn(main_LogInfo->stopRun);

#if defined(SWNETCDF)
    if (!readFromCacheFile && rank == ROOT_PROC) {
        SW_NCIN_create_cache_file(SW_Domain, sw_template, main_LogInfo);
    }
    checkReturn(main_LogInfo->stopRun);

    SW_NCIN_handle_temp_inputs(
        alloc, SW_Domain, &tempVals, &newSoils, main_LogInfo
    );
    checkJumpToLabel(main_LogInfo->stopRun, freeMem);
#endif

    init_all_logs(
        nActiveSites,
        isSimDomDiscrete,
        SW_Domain->globDomSuids,
        main_LogInfo->logfp,
        siteLogs
    );

    SW_Domain->SW_ConstInfo.ModelSim.progRestarted = progRestart;
    init_all_runs(
        rank,
        copyWeatherHist,
        nActiveSites,
        tempVals,
        SW_Domain,
        sw_template,
        siteRuns,
        newSoils,
        siteLogs,
        main_LogInfo
    );
    checkJumpToLabel(main_LogInfo->stopRun, freeMem);

#if defined(SWNETCDF)
    if (progRestart) {
        SW_NCIN_handle_cache_vals(
            rank, readCache, SW_Domain, sw_template, siteRuns, main_LogInfo
        );
        checkJumpToLabel(main_LogInfo->stopRun, freeMem);
    }
#endif

    if (main_LogInfo->printProgressMsg) {
        report_sim_start(SW_Domain, rank, worldSize);
    }

#if defined(SWNETCDF)
    nYears = *year - SW_Domain->startyr;
    display_yearly_progress(
        rank,
        nYears,
        startupPrint,
        inSpinup,
        fullFinalYear,
        displayNYearsBeforeSim,
        finalSpinUpYr
    );
#endif

    SW_CTL_run_daily_timesteps(
        rank,
        sw_template,
        SW_Domain->startSimDay,
        SW_Domain->endSimDay,
        DO_IO_TIMING,
        tempVals,
        SW_Domain,
        siteRuns,
        siteLogs,
        SW_WallTime,
        main_LogInfo
    );

freeMem:
    SW_F_report_logs(SW_Domain, siteLogs, nActiveSites);

#if defined(SWNETCDF)
    nYears = *year - SW_Domain->startyr + 1;

    formatLogStage(
        main_LogInfo->logStage, sizeof main_LogInfo->logStage, "wrapup"
    );

    // Don't include partial last year
    fullFinalYear = (Bool) (*doy == *lastDoy + 1);
    nYears -= (!fullFinalYear) ? 1 : 0;
    startupPrint = swFALSE;
    display_yearly_progress(
        rank,
        nYears,
        startupPrint,
        inSpinup,
        fullFinalYear,
        displayNYearsAfterSim,
        finalSpinUpYr
    );

    cacheAtEnd = (Bool) (*year != SW_Domain->endyr || !fullFinalYear);
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

    SW_NCIN_handle_temp_inputs(
        dealloc, SW_Domain, &tempVals, &newSoils, main_LogInfo
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
    sw->VegProdIn = &SW_Domain->SW_ConstInfo.VegProdIn;
    sw->ModelIn = &SW_Domain->SW_ConstInfo.ModelIn;
    sw->SoilWatIn = &SW_Domain->SW_ConstInfo.SoilWatIn;
    sw->SiteIn = &SW_Domain->SW_ConstInfo.SiteIn;
    sw->ModelSim = &SW_Domain->SW_ConstInfo.ModelSim;
    sw->OutRun = &SW_Domain->SW_ConstInfo.OutRun;
    sw->SW_PathOutputs = &SW_Domain->SW_ConstInfo.SW_PathOutputs;

    // Initialize pointers within substructs
    SW_WTH_init_ptrs(&sw->RunIn.weathRunAllHist);
    SW_MKV_init_ptrs(&sw->MarkovIn);
    SW_VPD_init_ptrs(&sw->VegProdSim);
    SW_VES_init_ptrs(&sw->VegEstabIn, sw->ves_p_accu, sw->ves_p_oagg);
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
@param[in] runSimDayLen The number of days the simulations are to be run for
@param[out] SW_Domain Struct of type SW_DOMAIN holding constant
    temporal/spatial information for a set of simulation runs
@param[out] LogInfo Holds information on warnings and errors
*/
void SW_CTL_setup_domain(
    int rank,
    int worldSize,
    Bool renameDomainTemp,
    TimeInt runSimDayLen,
    SW_DOMAIN *SW_Domain,
    LOG_INFO *LogInfo
) {
#if defined(SWNETCDF)
    /* Combine both prog vars into one in this case */
    const int nUniqueDomVars = 2;
    const Bool openInPar = swFALSE;
    const int openMode = NC_NOWRITE;

    Bool domProgFileExists[vNCNumDomFiles] = {swFALSE};
    char ***ncInFiles = &SW_Domain->SW_PathInputs.ncInFiles[eSW_InDomain];
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
    SW_NC_read(SW_Domain, LogInfo);
    checkReturn(LogInfo->stopRun);

    domProgFileExists[vNCdom] = FileExists((*ncInFiles)[vNCdom]);
    domProgFileExists[vNCprogStatus] = FileExists((*ncInFiles)[vNCprogStatus]);
    domProgFileExists[vNCprogTime] = FileExists((*ncInFiles)[vNCprogTime]);

    SW_NCIN_create_units_converters(&SW_Domain->netCDFInput, LogInfo);
    checkReturn(LogInfo->stopRun);

    // Create domain template if it does not exist (and exit)
    char *fnameDomainTemplateNC;

    fnameDomainTemplateNC =
        (renameDomainTemp) ?
            SW_Domain->SW_PathInputs.ncInFiles[eSW_InDomain][vNCdom] :
            NULL;

    for (file = 0; file < nUniqueDomVars; file++) {
        if (rank == ROOT_PROC) {
            if (!domProgFileExists[file]) {
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

                    if (!LogInfo->stopRun) {
                        // Close domain file to be reopened in the next function
                        // call (if it needs to be opened for parallel access)
                        nc_close(SW_Domain->SW_PathInputs.ncDomFileIDs[vNCdom]);
                    }
                    break;
                case vNCprogStatus: /* vNCprogStatus & vNCprogTime */
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
            } else if (file == vNCdom) {
                SW_NCIN_open_dom_temp(SW_Domain, LogInfo);
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

    SW_DOM_SimSet(rank, worldSize, runSimDayLen, SW_Domain, LogInfo);
}

/**
@brief Setup and construct model (independent of inputs)

@param[in,out] sw Comprehensive struct of type SW_RUN containing all
    information in the simulation
@param[in] zeroOutInfo Specifies if SW_OUT_RUN should be zeroed-out
@param[out] LogInfo Holds information on warnings and errors
*/
void SW_CTL_setup_model(SW_RUN *sw, Bool zeroOutInfo, LOG_INFO *LogInfo) {
    SW_MDL_construct(sw->ModelSim);
    SW_WTH_construct(
        sw->WeatherIn, &sw->WeatherSim, sw->weath_p_accu, sw->weath_p_oagg
    );

    // delay SW_MKV_construct() until we know from inputs whether we need it
    // SW_SKY_construct() not need
    SW_SIT_construct(sw->SiteIn, &sw->SiteSim, &sw->RunIn.SiteRunIn.n_layers);
    SW_VES_construct(
        &sw->VegEstabIn, &sw->VegEstabSim, sw->ves_p_oagg, sw->ves_p_accu
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
    SW_MKV_deconstruct(&sw->MarkovIn);
    // SW_SKY_INPUTS_deconstruct() not needed
    // SW_SIT_deconstruct() not needed
    SW_VES_deconstruct(sw->VegEstabIn.count, sw->ves_p_accu, sw->ves_p_oagg);
    SW_VPD_deconstruct(&sw->VegProdSim);
    // SW_FLW_deconstruct() not needed
    SW_SWC_deconstruct(sw->SoilWatIn, &sw->SoilWatSim);
    SW_CBN_deconstruct(sw->CarbonIn);
}

/**
@brief Initialize simulation run except for VPD, which is done in
`prepare_next_day()` (based on user inputs)

Note: Time will only be set up correctly while carrying out a simulation year,
i.e., after calling begin_year()

@param[in,out] sw Comprehensive structure holding all information
    dealt with in SOILWAT2
@param[out] siteLog Site-specific instance of LOG_INFO
@param[out] main_LogInfo The main LOG_INFO instance for the program
*/
void SW_CTL_init_run(SW_RUN *sw, LOG_INFO *siteLog, LOG_INFO *main_LogInfo) {
    TimeInt n_years = sw->ModelIn->endyr - sw->ModelIn->startyr + 1;

    // SW_F_init_run() not needed
    SW_MDL_init_run(sw->ModelSim, sw->ModelIn->startyr);
    SW_WTH_init_run(sw->WeatherIn, &sw->WeatherSim);
    // SW_MKV_init_run() not needed
    SW_PET_init_run(&sw->AtmDemSim);

    SW_SKY_init_run(&sw->RunIn.SkyRunIn, siteLog);
    if (siteLog->stopRun) {
        return; // Exit function prematurely due to error
    }

    SW_SIT_init_run(
        sw->VegProdIn,
        sw->SiteIn,
        &sw->SiteSim,
        &sw->RunIn.SoilRunIn,
        &sw->VegProdIn->veg,
        sw->RunIn.SiteRunIn.n_layers,
        siteLog
    );
    if (siteLog->stopRun) {
        return; // Exit function prematurely due to error
    }

    // SW_VES_init_run() must be called after `SW_SIT_init_run()`
    SW_VES_init_run(
        &sw->VegEstabIn.parms,
        &sw->RunIn.SoilRunIn,
        &sw->SiteSim,
        sw->SiteSim.n_transp_lyrs,
        sw->VegEstabIn.count,
        siteLog
    );
    if (siteLog->stopRun) {
        return; // Exit function prematurely due to error
    }

    /*
        Note: `SW_VPD_init_run_calc()` is called in `prepare_next_day()` because
        VPD needs the first year of weather to initialize the run
    */
    SW_VPD_init_run_mem(
        sw->VegProdIn->veg_method,
        sw->SiteIn->methodMaxDepthSoilTemperature,
        n_years,
        sw->ModelIn->SW_SpinUp.duration,
        &sw->VegProdSim,
        main_LogInfo
    );
    if (main_LogInfo->stopRun) {
        return;
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
        siteLog
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

    updateLogDate(LogInfo, sw->ModelSim->year, sw->ModelSim->doy);

    begin_day_site(sw, LogInfo);
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

    if (sw->VegEstabIn.use) {
        SW_VES_checkestab(
            &sw->VegEstabIn.parms,
            &sw->VegEstabSim.parms,
            sw->WeatherSim.temp_avg,
            sw->SoilWatSim.swcBulk,
            sw->ModelSim->doy,
            sw->ModelSim->firstdoy,
            sw->VegEstabIn.count
        );
    }

    LogInfo->hasLogDate = swFALSE;

#ifdef SWDEBUG
    if (debug) {
        sw_printf("ending day ... ");
    }
#endif

    end_day(sw, OutDom, LogInfo);

#ifdef SWDEBUG
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

@param[in] rank Process number known to MPI for the current process (aka rank);
defaults to 0 (main process) if we are running sequentially
@param[in] SW_Domain Struct of type SW_DOMAIN holding constant
temporal/spatial information for a set of simulation runs
@param[in] tempVals An allocated space to store temporary input values
for converting and setting into proper location
@param[in,out] sw Comprehensive struct of type SW_RUN containing all
  information in the simulation
@param[out] siteLogs A list of LOG_INFO instances of size n active sites that
will be returned with any warnings/errors that occurred in spinup
@param[out] main_LogInfo Holds information dealing with logfile output
*/
void SW_CTL_run_spinup(
    int rank,
    SW_DOMAIN *SW_Domain,
    double *tempVals,
    SW_RUN *sw,
    LOG_INFO *siteLogs,
    LOG_INFO *main_LogInfo
) {
#ifdef SWDEBUG
    int debug = 0;
#endif

    SW_WALLTIME *SW_WallTime = NULL;

#if defined(SWNETCDF)
    const IntU nYears = 0;
    const Bool startupPrint = swTRUE;
    const Bool fullFinalYear = swFALSE;
    const Bool displayNYearsBeforeSim = swFALSE;
    const Bool finalSpinUpYr = swFALSE;
    const Bool inSpinup = swTRUE;
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
    TimeInt startDay = 1;
    TimeInt endDay = 0;
    years = (TimeInt *) Mem_Malloc(
        sizeof(TimeInt) * duration, "SW_CTL_run_spinup", main_LogInfo
    );
    if (main_LogInfo->stopRun) {
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
            sw->ModelIn->startyr,
            finalyr
        );
    }
#endif

#if defined(SWNETCDF)
    if (SW_Domain->SW_ConstInfo.ModelSim.doOutput) {
        display_yearly_progress(
            rank,
            nYears,
            startupPrint,
            inSpinup,
            fullFinalYear,
            displayNYearsBeforeSim,
            finalSpinUpYr
        );
    }
#else
    (void) rank;
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
    sw->ModelSim->inSpinup = swTRUE;

    for (yrIdx = 0; yrIdx < duration; yrIdx++) {
        *cur_yr = years[yrIdx];
        endDay = Time_get_lastdoy_y(*cur_yr);

#ifdef SWDEBUG
        if (debug) {
            sw_printf(
                "'SW_CTL_run_spinup': simulate year = %d | %d\n",
                yrIdx + 1,
                *cur_yr
            );
        }
#endif
        // Timing and output to terminal operations do not occur
        // so sending default rank, temporary values/storage
        // is okay
#if defined(SWNETCDF)
        SW_Domain->SW_ConstInfo.ModelSim.inputYearIdx = 0;
#else
        SW_Domain->SW_ConstInfo.ModelSim.inputYearIdx =
            *cur_yr - SW_Domain->startyr;
#endif
        SW_CTL_run_daily_timesteps(
            ROOT_PROC,
            sw,
            startDay,
            endDay,
            NO_IO_TIMING,
            tempVals,
            SW_Domain,
            sw,
            siteLogs,
            SW_WallTime,
            main_LogInfo
        );
        if (main_LogInfo->stopRun) {
            goto reSet; // Exit function prematurely due to error
        }
    }

reSet: {
    SW_Domain->SW_ConstInfo.ModelSim.year = SW_Domain->startyr;
    SW_Domain->SW_ConstInfo.ModelSim.yearIdx = 0;
    SW_Domain->SW_ConstInfo.ModelSim.inputYearIdx = 0;

#if defined(SWNETCDF)
    SW_Domain->SW_PathInputs.weathStartFileIndex = 0;
#endif

    sw->ModelSim->inSpinup = swFALSE;
    sw->ModelSim->doOutput = swTRUE;
    /* Note: don't reset sw->ModelSim.yearIdxSpinSim which is a
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

    if (sw->WeatherIn->generateWeatherMethod == wgMKV) {
        SW_MKV_setup(
            &sw->MarkovIn,
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
