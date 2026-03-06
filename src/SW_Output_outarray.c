/********************************************************/
/********************************************************/
/**
@file
@brief Output functionality for in-memory processing of outputs

See the \ref out_algo "output algorithm documentation" for details.

History:
2018 June 15 (drs) moved functions from `SW_Output.c` and `rSW_Output.c`
*/
/********************************************************/
/********************************************************/


/* =================================================== */
/*                INCLUDES / DEFINES                   */
/* --------------------------------------------------- */

#include "include/SW_Output_outarray.h" // for SW_OUT_calc_iOUToffset, SW_O...
#include "include/generic.h"            // for IntUS, IntU, Bool
#include "include/SW_datastructs.h"     // for LOG_INFO, SW_MODEL
#include "include/SW_Defines.h"         // for eSW_Day, SW_OUTNMAXVARS, SW_...
#include "include/SW_Output.h"          // for ForEachOutKey
#include "include/Times.h"              // for Time_get_lastdoy_y
#include <stdio.h>                      // for size_t

#if defined(SW_OUTARRAY)

#if defined(SWNETCDF)
#include <netcdf.h>
#endif

#include "include/myMemory.h" // for Mem_Calloc
#include <stdlib.h>           // for free
#endif


/* =================================================== */
/*                  Global Variables                   */
/* --------------------------------------------------- */

/**
@brief A 2-dim array of pointers to output arrays of standard deviations.

@note This should be initialized to NULL because they are defined globally
and thus have `static storage duration`.

The variable p_OUTsd is used by STEPWAT2 for standard-deviation during
aggregation. See also `SW_OUT_RUN.p_OUT`
*/
#define p_OUTsd
#undef p_OUTsd

#if defined(SWNETCDF)
// time header not used for netCDFs -> set to 0s
const IntUS ncol_TimeOUT[SW_OUTNPERIODS] = {0};
#else
/** number of time header columns for each output period, i.e.,
Year and Week/Month/Day */
const IntUS ncol_TimeOUT[SW_OUTNPERIODS] = {2, 2, 2, 1};
#endif


/* =================================================== */
/*             Global Function Definitions             */
/* --------------------------------------------------- */

/**
@brief Determine number of years/months/weeks/days used in simulation period

@param[in] SW_ModelIn Struct of type SW_MODEL_INPUTS holding basic input
    time information about the simulation
@param[in] use_OutPeriod Describes which time period is currently active
@param[out] nrow_OUT Number of output rows for each output period
*/
void SW_OUT_set_nrow(
    SW_MODEL_INPUTS *SW_ModelIn,
    const Bool use_OutPeriod[],
    size_t nrow_OUT[][SW_OUTNPERIODS]
) {
#ifdef SWDEBUG
    int debug = 0;
#endif

    size_t n_yrs;
    IntU startyear = SW_ModelIn->startyr;
    IntU endyear;

    OutKey outKey;

#ifdef STEPWAT
    n_yrs = SW_ModelIn->runModelYears;
    endyear = startyear + n_yrs + 1;
#elif !defined(SWNETCDF)
    n_yrs = SW_ModelIn->endyr - SW_ModelIn->startyr + 1;
    endyear = SW_ModelIn->endyr;
#else
    OutPeriod outPd;

    (void) n_yrs;
    (void) startyear;
    (void) endyear;
#endif

#if defined(SWNETCDF)
    ForEachOutKey(outKey) {
        ForEachOutPeriod(outPd) {
            nrow_OUT[outKey][outPd] = (size_t) use_OutPeriod[outPd];
        }
    }
#else
    TimeInt i;

    ForEachOutKey(outKey) {
        nrow_OUT[outKey][eSW_Year] = n_yrs * use_OutPeriod[eSW_Year];
        nrow_OUT[outKey][eSW_Month] =
            n_yrs * MAX_MONTHS * use_OutPeriod[eSW_Month];
        nrow_OUT[outKey][eSW_Week] =
            n_yrs * MAX_WEEKS * use_OutPeriod[eSW_Week];
        nrow_OUT[outKey][eSW_Day] = 0;

        if (use_OutPeriod[eSW_Day]) {
            if (n_yrs == 1) {
                nrow_OUT[outKey][eSW_Day] =
                    SW_ModelIn->endend - SW_ModelIn->startstart + 1;

            } else {
                // Calculate the start day of first year
                nrow_OUT[outKey][eSW_Day] =
                    Time_get_lastdoy_y(startyear) - SW_ModelIn->startstart + 1;
                // and last day of last year.
                nrow_OUT[outKey][eSW_Day] += SW_ModelIn->endend;

                // Cumulate days of years between first and last year
                for (i = startyear + 1; i < endyear; i++) {
                    nrow_OUT[outKey][eSW_Day] += Time_get_lastdoy_y(i);
                }
            }
        }
    }

#endif

#ifdef SWDEBUG
    if (debug) {
        sw_printf(
            "n(year) = %zu, n(month) = %zu, n(week) = %zu, n(day) = %zu\n",
            nrow_OUT[eSW_Temp][eSW_Year],
            nrow_OUT[eSW_Temp][eSW_Month],
            nrow_OUT[eSW_Temp][eSW_Week],
            nrow_OUT[eSW_Temp][eSW_Day]
        );
    }
#endif
}

/**
@brief For each out key, the p_OUT array is set to NULL.

@param[in,out] OutRun Struct of type SW_OUT_RUN that holds output
    information that may change throughout simulation runs
*/
void SW_OUT_deconstruct_outarray(SW_OUT_RUN *OutRun) {
    int i;
    int k;

    ForEachOutKey(k) {
        for (i = 0; i < SW_OUTNPERIODS; i++) {
#if defined(SW_OUTARRAY)
            if (!isnull(OutRun->p_OUT[k][i])) {
                free(OutRun->p_OUT[k][i]);
                OutRun->p_OUT[k][i] = NULL;
            }
#endif

#ifdef STEPWAT
            if (!isnull(OutRun->p_OUTsd[k][i])) {
                free(OutRun->p_OUTsd[k][i]);
                OutRun->p_OUTsd[k][i] = NULL;
            }
#endif
        }
    }

#if !defined(SW_OUTARRAY) && !defined(STEPWAT)
    (void) *OutRun;
#endif
}


#ifdef RSOILWAT
/**
@brief Corresponds to function `get_outstrleader` of `SOILWAT2-standalone

@param[in] SW_ModelSim Struct of type SW_MODEL_SIM holding basic
    intermediate time information about the simulation run
@param[in] pd Time period in simulation output (day/week/month/year)
@param[in] irow_OUT Current time step
@param[in] nrow_OUT Number of output rows for each output period
@param[out] p Allocated array to hold output periods for every output key
*/
void get_outvalleader(
    SW_MODEL_SIM *SW_ModelSim,
    OutPeriod pd,
    const size_t irow_OUT[],
    const size_t nrow_OUT[],
    double *p
) {

    p[irow_OUT[pd] + nrow_OUT[pd] * 0] = SW_ModelSim->year;

    switch (pd) {
    case eSW_Day:
        p[irow_OUT[eSW_Day] + nrow_OUT[eSW_Day] * 1] =
            SW_ModelSim->doy; // base1
        break;

    case eSW_Week:
        p[irow_OUT[eSW_Week] + nrow_OUT[eSW_Week] * 1] =
            SW_ModelSim->week + 1; // base0
        break;

    case eSW_Month:
        p[irow_OUT[eSW_Month] + nrow_OUT[eSW_Month] * 1] =
            SW_ModelSim->month + 1; // base0
        break;

    case eSW_Year:
    default:
        break;
    }
}
#endif


#if defined(STEPWAT)
/**
@brief Handle the cumulative running mean and standard deviation

@param[in,out] p Running mean
@param[in,out] psd Running standard deviation
@param[in] k The index (base0) for subsetting `p` and `psd`,
    e.g., as calculated by macro `iOUT` or `iOUT2`.
@param[in] n The current iteration/repetition number (base1).
@param[in] x The new value to add to the running mean and
    running standard deviation
*/
void do_running_agg(double *p, double *psd, size_t k, IntU n, double x) {
    double prev_val = p[k];

    p[k] = get_running_mean(n, prev_val, x);
    psd[k] =
        psd[k] + get_running_sqr(prev_val, p[k], x); // += didn't work with *psd
}
#endif


/** Allocate p_OUT and p_OUTsd

@param[in] sizeMult A scalar value to multiply the normal single-site
    size for each piece of active input; should be set to 1 if no
    extra space is needed
@param[in] OutDom Struct of type SW_OUT_DOM that holds output
    information that do not change throughout simulation runs
@param[out] OutRun Struct of type SW_OUT_RUN that holds output
    information that may change throughout simulation runs
@param[out] LogInfo Holds information on warnings and errors

Note: Compare with function `setGlobalrSOILWAT2_OutputVariables` in
`rSW_Output.c`

@sideeffect: `OutRun->p_OUT` and `OutRun->p_OUTsd` pointing to
    allocated arrays for each output period and output key.
*/
void SW_OUT_construct_outarray(
    size_t sizeMult, SW_OUT_DOM *OutDom, SW_OUT_RUN *OutRun, LOG_INFO *LogInfo
) {
    int i;
    int k;
    size_t size;
    size_t s = sizeof(double);
    OutPeriod timeStepOutPeriod;

#if defined(SWNETCDF)
    size_t val;
#else
    (void) sizeMult;
#endif

    ForEachOutKey(k) {
        for (i = 0; i < OutDom->used_OUTNPERIODS; i++) {
            timeStepOutPeriod = OutDom->timeSteps[k][i];

            if (OutDom->use[k] && timeStepOutPeriod != eSW_NoTime) {

#if defined(SW_OUTARRAY)
                size = OutRun->nP_OUT[k][timeStepOutPeriod];
                size *= sizeMult;

#if defined(SWNETCDF)
                /* Size must be + 1 to hold a space for disabled variables to
                   write junk values */
                size++;
#endif

                OutRun->p_OUT[k][timeStepOutPeriod] = (double *) Mem_Calloc(
                    size, s, "SW_OUT_construct_outarray()", LogInfo
                );
                if (LogInfo->stopRun) {
                    return; // Exit function prematurely due to error
                }

#if defined(SWNETCDF)
                for (val = 0; val < size; val++) {
                    OutRun->p_OUT[k][timeStepOutPeriod][val] = NC_FILL_DOUBLE;
                }
#endif
#endif

#if defined(STEPWAT)
                OutRun->p_OUTsd[k][timeStepOutPeriod] = (double *) Mem_Calloc(
                    size, s, "SW_OUT_construct_outarray()", LogInfo
                );
                if (LogInfo->stopRun) {
                    return; // Exit function prematurely due to error
                }
#endif
            }
        }
    }


#if !defined(SW_OUTARRAY) && !defined(STEPWAT)
    (void) *LogInfo;
    (void) s;
    (void) size;
    (void) OutRun;
#endif
}

/** Calculate offset positions of output variables for indexing p_OUT

@param[in] nrow_OUT Number of output time steps
    (double array of length SW_OUTNKEYS x SW_OUTNPERIODS).
@param[in] nvar_OUT Number of output variables
    (array of length SW_OUTNPERIODS).
@param[in] totNSites Total number of sites in the process' subdomain
@param[in] useKey A list of size SW_OUTNKEYS specifying if an output key
    should be output at all
@param[in] nsl_OUT Number of output soil layer per variable
    (array of size SW_OUTNKEYS by SW_OUTNMAXVARS).
@param[in] npft_OUT Number of output vegtypes per variable
    (array of size SW_OUTNKEYS by SW_OUTNMAXVARS).
@param[in] reqOutVars Do/don't output a variable in the netCDF output
    files (dynamically allocated array over output variables)
@param[out] iOUToffset Offset positions of output variables for indexing
    p_OUT (array of size SW_OUTNKEYS by SW_OUTNPERIODS by SW_OUTNMAXVARS).
*/
void SW_OUT_calc_iOUToffset(
    const size_t nrow_OUT[][SW_OUTNPERIODS],
    const IntUS nvar_OUT[],
    const size_t totNSites,
    const Bool useKey[],
    IntUS nsl_OUT[][SW_OUTNMAXVARS],
    IntUS npft_OUT[][SW_OUTNMAXVARS],
    Bool *reqOutVars[],
    size_t iOUToffset[][SW_OUTNPERIODS][SW_OUTNMAXVARS]
) {
    int key;
    int ivar;
    int iprev = 0;
    int pd;
    size_t tmp;
    size_t tmp_nsl;
    size_t tmp_npft;

    ForEachOutPeriod(pd) {
        ForEachOutKey(key) {
            for (ivar = 0; ivar < SW_OUTNMAXVARS; ivar++) {
                iOUToffset[key][pd][ivar] = 0;
            }

            if (!useKey[key]) {
                continue;
            }

            iprev = -1;
            for (ivar = 0; ivar < nvar_OUT[key]; ivar++) {
                if (!reqOutVars[key][ivar]) {
                    continue;
                }

                if (iprev >= 0) {
                    tmp_nsl =
                        (nsl_OUT[key][iprev] > 0) ? nsl_OUT[key][iprev] : 1;
                    tmp_npft =
                        (npft_OUT[key][iprev] > 0) ? npft_OUT[key][iprev] : 1;
                }

                tmp = iOUTnc(
                    nrow_OUT[key][pd] - 1,
                    tmp_nsl - 1,
                    totNSites - 1,
                    tmp_npft - 1,
                    tmp_nsl,
                    totNSites,
                    tmp_npft
                );

                if (iprev >= 0) {
                    iOUToffset[key][pd][ivar] =
                        iOUToffset[key][pd][iprev] + 1 + tmp;
                }

                iprev = ivar;
            }

            for (ivar = 0; ivar < nvar_OUT[key]; ivar++) {
                if (!reqOutVars[key][ivar]) {
                    iOUToffset[key][pd][ivar] = iOUToffset[key][pd][iprev] + 1;
                }
            }
        }
    }
}
