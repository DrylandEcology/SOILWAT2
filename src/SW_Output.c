/********************************************************/
/********************************************************/
/**
@file
@brief Read / write and otherwise manage the user-specified output

See the \ref out_algo "output algorithm documentation" for details.

History:
- 9/11/01 cwb -- INITIAL CODING

- 10-May-02 cwb -- Added conditionals for interfacing
  with STEPPE

- 27-Aug-03 (cwb) Just a comment that this code doesn't
  handle missing values in the summaries, especially
  the averages.  This really needs to be addressed
  sometime, but for now it's the user's responsibility
  to make sure there are no missing values.  The
  model doesn't generate any on its own, but it
  still needs to be fixed, although that will take
  a bit of work to keep track of the number of
  missing days, etc.

- 2018 June 04 (drs) -- complete overhaul of output code
*/
/********************************************************/
/********************************************************/

/* =================================================== */
/*                INCLUDES / DEFINES                   */
/* --------------------------------------------------- */

#include "include/SW_Output.h"      // for ForEachOutKey, SW_EVAPSOIL
#include "include/filefuncs.h"      // for LogError, CloseFile, GetALine
#include "include/generic.h"        // for IntUS, Bool, LOGERROR, MAX, Str_Co...
#include "include/myMemory.h"       // for Str_Dup
#include "include/SW_datastructs.h" // for SW_RUN, SW_OUTTEXT, LOG_INFO
#include "include/SW_Defines.h"     // for eSWC, OutPeriod, NVEGTYPES
#include "include/SW_Files.h"       // for eOutput, eSite
#include "include/SW_Site.h"        // for echo_inputs
#include "include/SW_Times.h"       // for Today, Yesterday
#include "include/SW_VegEstab.h"    // for echo_VegEstab
#include "include/SW_VegProd.h"     // for echo_VegProd
#include "include/Times.h"          // for Time_days_in_month, WKDAYS
#include <stdio.h>                  // for snprintf, fprintf, printf
#include <string.h>                 // for strcmp, memccpy, memset

// Array-based output declarations:
#if defined(SW_OUTARRAY) || defined(SWNETCDF)
#include "include/SW_Output_outarray.h"
#endif

// Text-based output declarations:
#if defined(SW_OUTTEXT)
#include "include/SW_Output_outtext.h" // for SW_OUT_close_textfiles, SW_OU...
#endif

#if defined(SWNETCDF)
#include "include/SW_netCDF_General.h" // for vNCdom
#include "include/SW_netCDF_Output.h"  // for SW_NCOUT_alloc_files, SW_NCO...
#include <stdlib.h>                    // for free
#endif

/* Note: `get_XXX` functions are declared in `SW_Output.h`
    and defined/implemented in 'SW_Output_get_functions.c"
*/

/* converts an enum output key (OutKey type) to a module  */
/* or object type. see SW_Output.h for OutKey order.         */
/* MUST be SW_OUTNKEYS of these */
const ObjType key2obj[] = {
    // weather/atmospheric quantities:
    eWTH,
    eWTH,
    eWTH,
    eWTH,
    eWTH,
    // soil related water quantities:
    eSWC,
    eSWC,
    eSWC,
    eSWC,
    eSWC,
    eSWC,
    eSWC,
    eSWC,
    eSWC,
    eSWC,
    eSWC,
    eSWC,
    eSWC,
    eSWC,
    eSWC,
    eSWC,
    eSWC,
    eSWC,
    eSWC,
    eSWC,
    eSWC,
    eSWC,
    eSWC,
    // vegetation quantities:
    eVES,
    eVES,
    // vegetation other:
    eVPD,
    eVPD
};


/* =================================================== */
/*                  Global Variables                   */
/* --------------------------------------------------- */

// Convert from IDs to strings
/* These MUST be in the same order as enum OutKey in
 * SW_Output.h */
const char *const key2str[] = {
    // weather/atmospheric quantities:
    SW_WETHR,
    SW_TEMP,
    SW_PRECIP,
    SW_SOILINF,
    SW_RUNOFF,
    // soil related water quantities:
    SW_ALLH2O,
    SW_VWCBULK,
    SW_VWCMATRIC,
    SW_SWCBULK,
    SW_SWABULK,
    SW_SWAMATRIC,
    SW_SWA,
    SW_SWPMATRIC,
    SW_SURFACEW,
    SW_TRANSP,
    SW_EVAPSOIL,
    SW_EVAPSURFACE,
    SW_INTERCEPTION,
    SW_LYRDRAIN,
    SW_HYDRED,
    SW_ET,
    SW_AET,
    SW_PET,
    SW_WETDAY,
    SW_SNOWPACK,
    SW_DEEPSWC,
    SW_SOILTEMP,
    SW_FROZEN,
    // vegetation quantities:
    SW_ALLVEG,
    SW_ESTAB,
    // vegetation other:
    SW_CO2EFFECTS,
    SW_BIOMASS
};

const char *const pd2str[] = {SW_DAY, SW_WEEK, SW_MONTH, SW_YEAR};

const char *const pd2longstr[] = {
    SW_DAY_LONG, SW_WEEK_LONG, SW_MONTH_LONG, SW_YEAR_LONG
};

const char *const styp2str[] = {SW_SUM_OFF, SW_SUM_SUM, SW_SUM_AVG, SW_SUM_FNL};

const char *const styp2longstr[] = {
    SW_SUM_OFF, SW_SUM_SUM, SW_SUM_AVG_LONG, SW_SUM_FNL_LONG
};


/* =================================================== */
/*             Private Function Declarations            */
/* --------------------------------------------------- */

static OutPeriod str2period(char *s);
static OutKey str2key(char *s, LOG_INFO *LogInfo);
static OutSum str2stype(char *s, LOG_INFO *LogInfo);

static void collect_sums(
    SW_RUN *sw,
    SW_OUT_DOM *OutDom,
    ObjType otyp,
    OutPeriod op,
    LOG_INFO *LogInfo
);

static void sumof_wth(
    SW_WEATHER_SIM *v, SW_WEATHER_OUTPUTS *s, OutKey k, LOG_INFO *LogInfo
);

static void sumof_swc(
    SW_SOILWAT_SIM *v,
    SW_SOILWAT_OUTPUTS *s,
    OutKey k,
    SW_SITE_SIM *SW_SiteSim,
    LOG_INFO *LogInfo
);

static void sumof_ves(SW_VEGESTAB_SIM *v, SW_VEGESTAB_OUTPUTS *s, OutKey k);

static void sumof_vpd(
    SW_VEGPROD_OUTPUTS *s,
    VegType veg[],
    OutKey k,
    TimeInt doy,
    LOG_INFO *LogInfo
);

static void average_for(
    SW_RUN *sw,
    SW_OUT_DOM *OutDom,
    ObjType otyp,
    OutPeriod pd,
    Bool bFlush_output,
    TimeInt tOffset,
    LOG_INFO *LogInfo
);

#ifdef STEPWAT
static void set_SXWrequests_helper(
    SW_OUT_DOM *OutDom,
    OutKey k,
    OutPeriod pd,
    OutSum aggfun,
    const char *str,
    LOG_INFO *LogInfo
);
#endif


/* =================================================== */
/*             Local Function Definitions              */
/* --------------------------------------------------- */

/** Convert string representation of time period to `OutPeriod` value.
 */
static OutPeriod str2period(char *s) {
    IntUS pd;
    for (pd = 0; Str_CompareI(s, (char *) pd2str[pd]) && pd < SW_OUTNPERIODS;
         pd++)
        ;

    return (OutPeriod) pd;
}

/** Convert string representation of output type to `OutKey` value.
 */
static OutKey str2key(char *s, LOG_INFO *LogInfo) {
    IntUS key;

    for (key = 0; key < SW_OUTNKEYS && Str_CompareI(s, (char *) key2str[key]);
         key++)
        ;

    if (key == SW_OUTNKEYS) {
        LogError(LogInfo, LOGERROR, "Invalid key (%s) in 'outsetup.in'.\n", s);
    }

    return (OutKey) key;
}

/** Convert string representation of output aggregation function to `OutSum`
 * value.
 */
static OutSum str2stype(char *s, LOG_INFO *LogInfo) {
    IntUS styp;

    for (styp = eSW_Off;
         styp < SW_NSUMTYPES && Str_CompareI(s, (char *) styp2str[styp]);
         styp++)
        ;
    if (styp == SW_NSUMTYPES) {
        LogError(
            LogInfo, LOGERROR, "'outsetup.in : Invalid summary type (%s).\n", s
        );
    }
    return (OutSum) styp;
}

/** Checks whether a output variable (key) comes with soil layer or not.

See also function `has_keyname_soillayers`.

@param k The key of output variable (key), i.e., one of `OutKey`.
\return `TRUE` if `var` comes with soil layers; `FALSE` otherwise.
*/
Bool has_key_soillayers(OutKey k) {
    Bool has;

    has = (k == eSW_VWCBulk || k == eSW_VWCMatric || k == eSW_SWCBulk ||
           k == eSW_SWABulk || k == eSW_SWAMatric || k == eSW_SWA ||
           k == eSW_SWPMatric || k == eSW_Transp || k == eSW_EvapSoil ||
           k == eSW_LyrDrain || k == eSW_HydRed || k == eSW_WetDays ||
           k == eSW_SoilTemp || k == eSW_Frozen) ?
              swTRUE :
              swFALSE;

    return (has);
}

/** Checks whether a output variable (key) comes with soil layer or not

See also function `has_key_soillayers`.

@param var The name of an output variable (key), i.e., one of `key2str`.
\return `TRUE` if `var` comes with soil layers; `FALSE` otherwise.
*/
Bool has_keyname_soillayers(const char *var) {
    Bool has;

    has = (strcmp(var, SW_VWCBULK) == 0 || strcmp(var, SW_VWCMATRIC) == 0 ||
           strcmp(var, SW_SWCBULK) == 0 || strcmp(var, SW_SWABULK) == 0 ||
           strcmp(var, SW_SWAMATRIC) == 0 || strcmp(var, SW_SWA) == 0 ||
           strcmp(var, SW_SWPMATRIC) == 0 || strcmp(var, SW_TRANSP) == 0 ||
           strcmp(var, SW_EVAPSOIL) == 0 || strcmp(var, SW_LYRDRAIN) == 0 ||
           strcmp(var, SW_HYDRED) == 0 || strcmp(var, SW_WETDAY) == 0 ||
           strcmp(var, SW_SOILTEMP) == 0 || strcmp(var, SW_FROZEN) == 0) ?
              swTRUE :
              swFALSE;

    return (has);
}

static void sumof_vpd(
    SW_VEGPROD_OUTPUTS *s,
    VegType veg[],
    OutKey k,
    TimeInt doy,
    LOG_INFO *LogInfo
) {
    int ik;
    double tmp;

    switch (k) {
    case eSW_CO2Effects:
        break;

    // scale biomass by fCover to obtain biomass as observed in total vegetation
    case eSW_Biomass:
        ForEachVegType(ik) {
            tmp = veg[ik].biomass_daily[doy] * veg[ik].cov.fCover;
            s->veg[ik].biomass_inveg += tmp;
            s->biomass_total += tmp;

            tmp = veg[ik].litter_daily[doy] * veg[ik].cov.fCover;
            s->veg[ik].litter_inveg += tmp;
            s->litter_total += tmp;

            tmp = veg[ik].biolive_daily[doy] * veg[ik].cov.fCover;
            s->veg[ik].biolive_inveg += tmp;
            s->biolive_total += tmp;

            s->LAI += veg[ik].lai_live_daily[doy] * veg[ik].cov.fCover;
        }
        break;

    default:
        LogError(
            LogInfo, LOGERROR, "PGMR: Invalid key in sumof_vpd(%s)", key2str[k]
        );
        break;
    }
}

static void sumof_ves(SW_VEGESTAB_SIM *v, SW_VEGESTAB_OUTPUTS *s, OutKey k) {
    /* --------------------------------------------------- */
    /* k is always eSW_Estab, and this only gets called yearly */
    /* in fact, there's nothing to do here as the get_estab()
     * function does everything needed.  This stub is here only
     * to facilitate the loop everything else uses.
     * That is, until we need to start outputting as-yet-unknown
     * establishment variables.
     */

    // silence compile warnings [-Wunused-parameter]
    (void) *v;
    (void) *s;
    (void) k;
}

static void sumof_wth(
    SW_WEATHER_SIM *v, SW_WEATHER_OUTPUTS *s, OutKey k, LOG_INFO *LogInfo
) {
    switch (k) {

    case eSW_Temp:
        s->temp_max += v->temp_max;
        s->temp_min += v->temp_min;
        s->temp_avg += v->temp_avg;
        // added surfaceAvg for sum
        s->surfaceAvg += v->surfaceAvg;
        s->surfaceMax += v->surfaceMax;
        s->surfaceMin += v->surfaceMin;
        break;
    case eSW_Precip:
        s->ppt += v->ppt;
        s->rain += v->rain;
        s->snow += v->snow;
        s->snowmelt += v->snowmelt;
        s->snowloss += v->snowloss;
        break;
    case eSW_SoilInf:
        s->soil_inf += v->soil_inf;
        break;
    case eSW_Runoff:
        s->snowRunoff += v->snowRunoff;
        s->surfaceRunoff += v->surfaceRunoff;
        s->surfaceRunon += v->surfaceRunon;
        break;
    default:
        LogError(
            LogInfo, LOGERROR, "PGMR: Invalid key in sumof_wth(%s)", key2str[k]
        );
        break;
    }
}

static void sumof_swc(
    SW_SOILWAT_SIM *v,
    SW_SOILWAT_OUTPUTS *s,
    OutKey k,
    SW_SITE_SIM *SW_SiteSim,
    LOG_INFO *LogInfo
) {
    LyrIndex i;
    int j; // for use with ForEachVegType
    LyrIndex n_layers = SW_SiteSim->n_layers;
    LyrIndex n_evap_layers = SW_SiteSim->n_evap_lyrs;

    switch (k) {

    case eSW_VWCBulk:
        /* get swcBulk and convert later */
        ForEachSoilLayer(i, n_layers) s->vwcBulk[i] += v->swcBulk[Today][i];
        break;

    case eSW_VWCMatric:
        /* get swcBulk and convert later */
        ForEachSoilLayer(i, n_layers) s->vwcMatric[i] += v->swcBulk[Today][i];
        break;

    case eSW_SWCBulk:
        ForEachSoilLayer(i, n_layers) s->swcBulk[i] += v->swcBulk[Today][i];
        break;

    case eSW_SWPMatric:
        /* can't avg swp so get swcBulk and convert later */
        ForEachSoilLayer(i, n_layers) s->swpMatric[i] += v->swcBulk[Today][i];
        break;

    case eSW_SWABulk:
        ForEachSoilLayer(i, n_layers) s->swaBulk[i] +=
            fmax(v->swcBulk[Today][i] - SW_SiteSim->swcBulk_wiltpt[i], 0.);
        break;

    case eSW_SWAMatric:
        /* get swaBulk and convert later */
        ForEachSoilLayer(i, n_layers) s->swaMatric[i] +=
            fmax(v->swcBulk[Today][i] - SW_SiteSim->swcBulk_wiltpt[i], 0.);
        break;

    case eSW_SWA:
        /* get swaBulk and convert later */
        ForEachSoilLayer(i, n_layers) {
            ForEachVegType(j) {
                s->SWA_VegType[j][i] += v->dSWA_repartitioned_sum[j][i];
            }
        }
        break;

    case eSW_SurfaceWater:
        s->surfaceWater += v->surfaceWater;
        break;

    case eSW_Transp:
        ForEachSoilLayer(i, n_layers) {
            ForEachVegType(j) {
                s->transp_total[i] += v->transpiration[j][i];
                s->transp[j][i] += v->transpiration[j][i];
            }
        }
        break;

    case eSW_EvapSoil:
        ForEachEvapLayer(i, n_evap_layers) s->evap_baresoil[i] +=
            v->evap_baresoil[i];
        break;

    case eSW_EvapSurface:
        ForEachVegType(j) {
            s->total_evap += v->evap_veg[j];
            s->evap_veg[j] += v->evap_veg[j];
        }
        s->total_evap += v->litter_evap + v->surfaceWater_evap;
        s->litter_evap += v->litter_evap;
        s->surfaceWater_evap += v->surfaceWater_evap;
        break;

    case eSW_Interception:
        ForEachVegType(j) {
            s->total_int += v->int_veg[j];
            s->int_veg[j] += v->int_veg[j];
        }
        s->total_int += v->litter_int;
        s->litter_int += v->litter_int;
        break;

    case eSW_LyrDrain:
        for (i = 0; i < n_layers - 1; i++) {
            s->lyrdrain[i] += v->drain[i];
        }
        break;

    case eSW_HydRed:
        ForEachSoilLayer(i, n_layers) {
            ForEachVegType(j) {
                s->hydred_total[i] += v->hydred[j][i];
                s->hydred[j][i] += v->hydred[j][i];
            }
        }
        break;

    case eSW_AET:
        s->aet += v->aet;
        ForEachSoilLayer(i, n_layers) {
            ForEachVegType(j) { s->tran += v->transpiration[j][i]; }
        }
        ForEachEvapLayer(i, n_evap_layers) { s->esoil += v->evap_baresoil[i]; }
        ForEachVegType(j) { s->ecnw += v->evap_veg[j]; }
        s->esurf += v->litter_evap + v->surfaceWater_evap;
        // esnow: evaporation from snow (sublimation) should be handled here,
        // but values are stored in SW_WEATHER instead
        break;

    case eSW_PET:
        s->pet += v->pet;
        s->H_oh += v->H_oh;
        s->H_ot += v->H_ot;
        s->H_gh += v->H_gh;
        s->H_gt += v->H_gt;
        break;

    case eSW_WetDays:
        ForEachSoilLayer(i, n_layers) if (v->is_wet[i]) s->wetdays[i]++;
        break;

    case eSW_SnowPack:
        s->snowpack += v->snowpack[Today];
        s->snowdepth += v->snowdepth;
        break;

    case eSW_DeepSWC:
        // deepest percolation == deep drainage
        s->deep += v->drain[SW_SiteSim->deep_lyr];
        break;

    case eSW_SoilTemp:
        ForEachSoilLayer(i, n_layers) {
            s->avgLyrTemp[i] += v->avgLyrTemp[i];
            s->minLyrTemperature[i] += v->minLyrTemperature[i];
            s->maxLyrTemperature[i] += v->maxLyrTemperature[i];
        }
        break;

    case eSW_Frozen:
        ForEachSoilLayer(i, n_layers) s->lyrFrozen[i] += v->lyrFrozen[i];
        break;

    default:
        LogError(
            LogInfo, LOGERROR, "PGMR: Invalid key in sumof_swc(%s)", key2str[k]
        );
        break;
    }
}

/** separates the task of obtaining a periodic average.
no need to average days, so this should never be
called with eSW_Day.

Enter this routine just after the summary period
is completed, so the current week and month will be
one greater than the period being summarized.

@param[in,out] sw Comprehensive struct of type SW_RUN containing
    all information in the simulation.
@param[in] OutDom Struct of type SW_OUT_DOM that holds output
    information that do not change throughout simulation runs
@param[in] otyp Identifies the current module/object
@param[in] pd Time period in simulation output (day/week/month/year)
@param[in] bFlush_output Determines if output should be created for
    a specific output key
@param[in] tOffset Offset describing with the previous or current period
@param[out] LogInfo Holds information on warnings and errors
*/
static void average_for(
    SW_RUN *sw,
    SW_OUT_DOM *OutDom,
    ObjType otyp,
    OutPeriod pd,
    Bool bFlush_output,
    TimeInt tOffset,
    LOG_INFO *LogInfo
) {

    TimeInt curr_pd = 0;
    double div = 0.; /* if sumtype=AVG, days in period; if sumtype=SUM, 1 */
    LyrIndex i;
    int k;
    int j;
    LyrIndex n_layers = sw->SiteSim.n_layers;
    LyrIndex n_evap_layers = sw->SiteSim.n_evap_lyrs;

    if (otyp == eVES) {
        return;
    }

    // carefully aggregate for specific time period and aggregation type
    // (mean, sum, final value)
    ForEachOutKey(k) {
        if (!OutDom->use[k]) {
            continue;
        }

        switch (pd) {
        case eSW_Week:
            curr_pd = (sw->ModelSim.week + 1) - tOffset;
            div = (bFlush_output) ? sw->ModelSim.lastdoy % WKDAYS : WKDAYS;
            break;

        case eSW_Month:
            curr_pd = (sw->ModelSim.month + 1) - tOffset;
            div = Time_days_in_month(
                sw->ModelSim.month - tOffset, sw->ModelSim.days_in_month
            );
            break;

        case eSW_Year:
            curr_pd = sw->OutRun.first[k];
            div = sw->OutRun.last[k] - sw->OutRun.first[k] + 1;
            break;

        default:
            LogError(
                LogInfo,
                LOGERROR,
                "Programmer: Invalid period in average_for()."
            );
            return; // Exit function prematurely due to error
            break;
        } /* end switch(pd) */

        if (OutDom->myobj[k] != otyp || curr_pd < sw->OutRun.first[k] ||
            curr_pd > sw->OutRun.last[k]) {
            continue;
        }

        if (OutDom->sumtype[k] == eSW_Sum) {
            div = 1.;
        }

        /* notice that all valid keys are in this switch */
        switch (k) {

        case eSW_Temp:
            sw->weath_p_oagg[pd].temp_max = sw->weath_p_accu[pd].temp_max / div;
            sw->weath_p_oagg[pd].temp_min = sw->weath_p_accu[pd].temp_min / div;
            sw->weath_p_oagg[pd].temp_avg = sw->weath_p_accu[pd].temp_avg / div;
            sw->weath_p_oagg[pd].surfaceAvg =
                sw->weath_p_accu[pd].surfaceAvg / div;
            sw->weath_p_oagg[pd].surfaceMax =
                sw->weath_p_accu[pd].surfaceMax / div;
            sw->weath_p_oagg[pd].surfaceMin =
                sw->weath_p_accu[pd].surfaceMin / div;
            break;

        case eSW_Precip:
            sw->weath_p_oagg[pd].ppt = sw->weath_p_accu[pd].ppt / div;
            sw->weath_p_oagg[pd].rain = sw->weath_p_accu[pd].rain / div;
            sw->weath_p_oagg[pd].snow = sw->weath_p_accu[pd].snow / div;
            sw->weath_p_oagg[pd].snowmelt = sw->weath_p_accu[pd].snowmelt / div;
            sw->weath_p_oagg[pd].snowloss = sw->weath_p_accu[pd].snowloss / div;
            break;

        case eSW_SoilInf:
            sw->weath_p_oagg[pd].soil_inf = sw->weath_p_accu[pd].soil_inf / div;
            break;

        case eSW_Runoff:
            sw->weath_p_oagg[pd].snowRunoff =
                sw->weath_p_accu[pd].snowRunoff / div;
            sw->weath_p_oagg[pd].surfaceRunoff =
                sw->weath_p_accu[pd].surfaceRunoff / div;
            sw->weath_p_oagg[pd].surfaceRunon =
                sw->weath_p_accu[pd].surfaceRunon / div;
            break;

        case eSW_SoilTemp:
            ForEachSoilLayer(i, n_layers) {
                sw->sw_p_oagg[pd].avgLyrTemp[i] =
                    (OutDom->sumtype[k] == eSW_Fnl) ?
                        sw->SoilWatSim.avgLyrTemp[i] :
                        sw->sw_p_accu[pd].avgLyrTemp[i] / div;
                sw->sw_p_oagg[pd].maxLyrTemperature[i] =
                    (OutDom->sumtype[k] == eSW_Fnl) ?
                        sw->SoilWatSim.maxLyrTemperature[i] :
                        sw->sw_p_accu[pd].maxLyrTemperature[i] / div;
                sw->sw_p_oagg[pd].minLyrTemperature[i] =
                    (OutDom->sumtype[k] == eSW_Fnl) ?
                        sw->SoilWatSim.minLyrTemperature[i] :
                        sw->sw_p_accu[pd].minLyrTemperature[i] / div;
            }
            break;

        case eSW_Frozen:
            ForEachSoilLayer(i, n_layers) {
                sw->sw_p_oagg[pd].lyrFrozen[i] =
                    (OutDom->sumtype[k] == eSW_Fnl) ?
                        sw->SoilWatSim.lyrFrozen[i] :
                        sw->sw_p_accu[pd].lyrFrozen[i] / div;
            }
            break;

        case eSW_VWCBulk:
            /* vwcBulk at this point is identical to swcBulk */
            ForEachSoilLayer(i, n_layers) {
                sw->sw_p_oagg[pd].vwcBulk[i] =
                    (OutDom->sumtype[k] == eSW_Fnl) ?
                        sw->SoilWatSim.swcBulk[Yesterday][i] :
                        sw->sw_p_accu[pd].vwcBulk[i] / div;
            }
            break;

        case eSW_VWCMatric:
            /* vwcMatric at this point is identical to swcBulk */
            ForEachSoilLayer(i, n_layers) {
                sw->sw_p_oagg[pd].vwcMatric[i] =
                    (OutDom->sumtype[k] == eSW_Fnl) ?
                        sw->SoilWatSim.swcBulk[Yesterday][i] :
                        sw->sw_p_accu[pd].vwcMatric[i] / div;
            }
            break;

        case eSW_SWCBulk:
            ForEachSoilLayer(i, n_layers) {
                sw->sw_p_oagg[pd].swcBulk[i] =
                    (OutDom->sumtype[k] == eSW_Fnl) ?
                        sw->SoilWatSim.swcBulk[Yesterday][i] :
                        sw->sw_p_accu[pd].swcBulk[i] / div;
            }
            break;

        case eSW_SWPMatric:
            /* swpMatric at this point is identical to swcBulk */
            ForEachSoilLayer(i, n_layers) {
                sw->sw_p_oagg[pd].swpMatric[i] =
                    (OutDom->sumtype[k] == eSW_Fnl) ?
                        sw->SoilWatSim.swcBulk[Yesterday][i] :
                        sw->sw_p_accu[pd].swpMatric[i] / div;
            }
            break;

        case eSW_SWABulk:
            ForEachSoilLayer(i, n_layers) {
                sw->sw_p_oagg[pd].swaBulk[i] =
                    (OutDom->sumtype[k] == eSW_Fnl) ?
                        fmax(
                            sw->SoilWatSim.swcBulk[Yesterday][i] -
                                sw->SiteSim.swcBulk_wiltpt[i],
                            0.
                        ) :
                        sw->sw_p_accu[pd].swaBulk[i] / div;
            }
            break;

        case eSW_SWAMatric:
            /* swaMatric at this point is identical to swaBulk */
            ForEachSoilLayer(i, n_layers) {
                sw->sw_p_oagg[pd].swaMatric[i] =
                    (OutDom->sumtype[k] == eSW_Fnl) ?
                        fmax(
                            sw->SoilWatSim.swcBulk[Yesterday][i] -
                                sw->SiteSim.swcBulk_wiltpt[i],
                            0.
                        ) :
                        sw->sw_p_accu[pd].swaMatric[i] / div;
            }
            break;

        case eSW_SWA:
            ForEachSoilLayer(i, n_layers) {
                ForEachVegType(j) {
                    sw->sw_p_oagg[pd].SWA_VegType[j][i] =
                        (OutDom->sumtype[k] == eSW_Fnl) ?
                            sw->SoilWatSim.dSWA_repartitioned_sum[j][i] :
                            sw->sw_p_accu[pd].SWA_VegType[j][i] / div;
                }
            }
            break;

        case eSW_DeepSWC:
            // deepest percolation == deep drainage
            sw->sw_p_oagg[pd].deep =
                (OutDom->sumtype[k] == eSW_Fnl) ?
                    sw->SoilWatSim.drain[sw->SiteSim.deep_lyr] :
                    sw->sw_p_accu[pd].deep / div;
            break;

        case eSW_SurfaceWater:
            sw->sw_p_oagg[pd].surfaceWater =
                sw->sw_p_accu[pd].surfaceWater / div;
            break;

        case eSW_Transp:
            ForEachSoilLayer(i, n_layers) {
                sw->sw_p_oagg[pd].transp_total[i] =
                    sw->sw_p_accu[pd].transp_total[i] / div;
                ForEachVegType(j) {
                    sw->sw_p_oagg[pd].transp[j][i] =
                        sw->sw_p_accu[pd].transp[j][i] / div;
                }
            }
            break;

        case eSW_EvapSoil:
            ForEachEvapLayer(i, n_evap_layers) sw->sw_p_oagg[pd]
                .evap_baresoil[i] = sw->sw_p_accu[pd].evap_baresoil[i] / div;
            break;

        case eSW_EvapSurface:
            sw->sw_p_oagg[pd].total_evap = sw->sw_p_accu[pd].total_evap / div;
            ForEachVegType(j) {
                sw->sw_p_oagg[pd].evap_veg[j] =
                    sw->sw_p_accu[pd].evap_veg[j] / div;
            }
            sw->sw_p_oagg[pd].litter_evap = sw->sw_p_accu[pd].litter_evap / div;
            sw->sw_p_oagg[pd].surfaceWater_evap =
                sw->sw_p_accu[pd].surfaceWater_evap / div;
            break;

        case eSW_Interception:
            sw->sw_p_oagg[pd].total_int = sw->sw_p_accu[pd].total_int / div;
            ForEachVegType(j) {
                sw->sw_p_oagg[pd].int_veg[j] =
                    sw->sw_p_accu[pd].int_veg[j] / div;
            }
            sw->sw_p_oagg[pd].litter_int = sw->sw_p_accu[pd].litter_int / div;
            break;

        case eSW_AET:
            sw->sw_p_oagg[pd].aet = sw->sw_p_accu[pd].aet / div;
            sw->sw_p_oagg[pd].tran = sw->sw_p_accu[pd].tran / div;
            sw->sw_p_oagg[pd].esoil = sw->sw_p_accu[pd].esoil / div;
            sw->sw_p_oagg[pd].ecnw = sw->sw_p_accu[pd].ecnw / div;
            sw->sw_p_oagg[pd].esurf = sw->sw_p_accu[pd].esurf / div;
            // sw->sw_p_oagg[pd].esnow = sw->sw_p_accu[pd].esnow
            // / div;
            break;

        case eSW_LyrDrain:
            for (i = 0; i < n_layers - 1; i++) {
                sw->sw_p_oagg[pd].lyrdrain[i] =
                    sw->sw_p_accu[pd].lyrdrain[i] / div;
            }
            break;

        case eSW_HydRed:
            ForEachSoilLayer(i, n_layers) {
                sw->sw_p_oagg[pd].hydred_total[i] =
                    sw->sw_p_accu[pd].hydred_total[i] / div;
                ForEachVegType(j) {
                    sw->sw_p_oagg[pd].hydred[j][i] =
                        sw->sw_p_accu[pd].hydred[j][i] / div;
                }
            }
            break;

        case eSW_PET:
            sw->sw_p_oagg[pd].pet = sw->sw_p_accu[pd].pet / div;
            sw->sw_p_oagg[pd].H_oh = sw->sw_p_accu[pd].H_oh / div;
            sw->sw_p_oagg[pd].H_ot = sw->sw_p_accu[pd].H_ot / div;
            sw->sw_p_oagg[pd].H_gh = sw->sw_p_accu[pd].H_gh / div;
            sw->sw_p_oagg[pd].H_gt = sw->sw_p_accu[pd].H_gt / div;
            break;

        case eSW_WetDays:
            ForEachSoilLayer(i, n_layers) sw->sw_p_oagg[pd].wetdays[i] =
                sw->sw_p_accu[pd].wetdays[i] / div;
            break;

        case eSW_SnowPack:
            sw->sw_p_oagg[pd].snowpack = sw->sw_p_accu[pd].snowpack / div;
            sw->sw_p_oagg[pd].snowdepth = sw->sw_p_accu[pd].snowdepth / div;
            break;

        case eSW_Estab:
            /* do nothing, no averaging required */

        case eSW_CO2Effects:
            /* do nothing, no averaging required */
            break;

        case eSW_Biomass:
            ForEachVegType(i) {
                sw->vp_p_oagg[pd].veg[i].biomass_inveg =
                    sw->vp_p_accu[pd].veg[i].biomass_inveg / div;

                sw->vp_p_oagg[pd].veg[i].litter_inveg =
                    sw->vp_p_accu[pd].veg[i].litter_inveg / div;

                sw->vp_p_oagg[pd].veg[i].biolive_inveg =
                    sw->vp_p_accu[pd].veg[i].biolive_inveg / div;
            }

            sw->vp_p_oagg[pd].biomass_total =
                sw->vp_p_accu[pd].biomass_total / div;
            sw->vp_p_oagg[pd].litter_total =
                sw->vp_p_accu[pd].litter_total / div;
            sw->vp_p_oagg[pd].biolive_total =
                sw->vp_p_accu[pd].biolive_total / div;
            sw->vp_p_oagg[pd].LAI = sw->vp_p_accu[pd].LAI / div;
            break;

        default:
            LogError(
                LogInfo,
                LOGERROR,
                "PGMR: Invalid key in average_for(%SW_SoilWat)",
                key2str[k]
            );
            return; // Exit function prematurely due to error
        }

    } /* end ForEachKey */
}

static void collect_sums(
    SW_RUN *sw,
    SW_OUT_DOM *OutDom,
    ObjType otyp,
    OutPeriod op,
    LOG_INFO *LogInfo
) {
    TimeInt pd = 0;
    int i;
    int k;
    Bool use_help;
    Bool use_KeyPeriodCombo;

    switch (op) {
    case eSW_Day:
        pd = sw->ModelSim.doy;
        break;
    case eSW_Week:
        pd = sw->ModelSim.week + 1;
        break;
    case eSW_Month:
        pd = sw->ModelSim.month + 1;
        break;
    case eSW_Year:
        pd = sw->ModelSim.doy;
        break;
    default:
        LogError(
            LogInfo, LOGERROR, "PGMR: Invalid outperiod in collect_sums()"
        );
        break;
    }


    // call `sumof_XXX` for each output key x output period combination
    // for those output keys that belong to the output type `otyp` (eSWC, eWTH,
    // eVES, eVPD)
    ForEachOutKey(k) {
        if (otyp != OutDom->myobj[k] || !OutDom->use[k]) {
            continue;
        }

        /* determine whether output period op is active for current output key k
         */
        use_KeyPeriodCombo = swFALSE;
        for (i = 0; i < OutDom->used_OUTNPERIODS; i++) {
            use_help = (Bool) (op == OutDom->timeSteps[k][i]);

#ifdef STEPWAT
            use_help = (Bool) (use_help || op == OutDom->timeSteps_SXW[k][i]);
#endif

            if (use_help) {
                use_KeyPeriodCombo = swTRUE;
                break;
            }
        }

        if (use_KeyPeriodCombo && pd >= sw->OutRun.first[k] &&
            pd <= sw->OutRun.last[k]) {
            switch (otyp) {
            case eSWC:
                sumof_swc(
                    &sw->SoilWatSim,
                    &sw->sw_p_accu[op],
                    (OutKey) k,
                    &sw->SiteSim,
                    LogInfo
                );
                if (LogInfo->stopRun) {
                    return; // Exit function prematurely due to error
                }

                if (op == eSW_Day) {
                    sw->sw_p_oagg[op] = sw->sw_p_accu[op];
                }
                break;

            case eWTH:
                sumof_wth(
                    &sw->WeatherSim, &sw->weath_p_accu[op], (OutKey) k, LogInfo
                );
                if (LogInfo->stopRun) {
                    return; // Exit function prematurely due to error
                }

                if (op == eSW_Day) {
                    sw->weath_p_oagg[op] = sw->weath_p_accu[op];
                }
                break;
            case eVES:
                if (op == eSW_Year) {
                    sumof_ves(
                        &sw->VegEstabSim, &sw->ves_p_accu[eSW_Year], (OutKey) k
                    ); /* yearly, y'see */
                }
                break;

            case eVPD:
                sumof_vpd(
                    &sw->vp_p_accu[op],
                    sw->RunIn.VegProdRunIn.veg,
                    (OutKey) k,
                    sw->ModelSim.doy,
                    LogInfo
                );
                if (LogInfo->stopRun) {
                    return; // Exit function prematurely due to error
                }

                if (op == eSW_Day) {
                    sw->vp_p_oagg[op] = sw->vp_p_accu[op];
                }
                break;

            default:
                break;
            }
        }

    } /* end ForEachOutKey */
}


#ifdef STEPWAT
static void set_SXWrequests_helper(
    SW_OUT_DOM *OutDom,
    OutKey k,
    OutPeriod pd,
    OutSum aggfun,
    const char *str,
    LOG_INFO *LogInfo
) {
    Bool warn = OutDom->use[k];

    OutDom->timeSteps_SXW[k][0] = pd;
    OutDom->use[k] = swTRUE;
    OutDom->first_orig[k] = 1;
    OutDom->last_orig[k] = 366;

    if (OutDom->sumtype[k] != aggfun) {
        if (warn && OutDom->sumtype[k] != eSW_Off) {
            LogError(
                LogInfo,
                LOGWARN,
                "STEPWAT2 requires %s of %s, "
                "but this is currently set to '%s': changed to '%s'.",
                styp2str[aggfun],
                str,
                styp2str[OutDom->sumtype[k]],
                styp2str[aggfun]
            );
        }

        OutDom->sumtype[k] = aggfun;
    }
}
#endif


/* =================================================== */
/*             Global Function Definitions             */
/* --------------------------------------------------- */


/**
@brief Tally for which output time periods at least one output key/type
    is active

@param[in,out] OutDom Struct of type SW_OUT_DOM that holds output
    information that do not change throughout simulation runs

@sideeffect Uses global variables SW_Output.use and timeSteps to set
    elements of use_OutPeriod
*/
void find_OutPeriods_inUse(SW_OUT_DOM *OutDom) {
    OutPeriod p;
    unsigned int k;
    unsigned int i;
    unsigned int timeStepInd;

    ForEachOutPeriod(p) { OutDom->use_OutPeriod[p] = swFALSE; }

    ForEachOutKey(k) {
        for (i = 0; i < OutDom->used_OUTNPERIODS; i++) {
            if (OutDom->use[k]) {
                if (OutDom->timeSteps[k][i] != eSW_NoTime) {
                    timeStepInd = OutDom->timeSteps[k][i];

                    OutDom->use_OutPeriod[timeStepInd] = swTRUE;
                }
            }
        }
    }
}

/** Determine whether output period `pd` is active for output key `k`
 */
Bool has_OutPeriod_inUse(
    OutPeriod pd,
    OutKey k,
    IntUS used_OUTNPERIODS,
    OutPeriod timeSteps[][SW_OUTNPERIODS]
) {
    int i;
    Bool has_timeStep = swFALSE;

    for (i = 0; i < used_OUTNPERIODS; i++) {
        has_timeStep = (Bool) (has_timeStep || timeSteps[k][i] == pd);
    }

    return has_timeStep;
}

#ifdef STEPWAT
/** Determine whether output period `pd` is active for output key `k` while
    accounting for output needs of `SXW`
*/
Bool has_OutPeriod_inUse2(OutPeriod pd, OutKey k, SW_OUT_DOM *OutDom) {
    int i;
    Bool has_timeStep2 = has_OutPeriod_inUse(
        pd, k, OutDom->used_OUTNPERIODS, OutDom->timeSteps_SXW
    );

    if (!has_timeStep2) {
        for (i = 0; i < OutDom->used_OUTNPERIODS; i++) {
            has_timeStep2 =
                (Bool) (has_timeStep2 || OutDom->timeSteps_SXW[k][i] == pd);
        }
    }

    return has_timeStep2;
}

/**
@brief Specify the output requirements so that the correct values are
passed in-memory via `SXW` to STEPWAT2

These must match with STEPWAT2's `struct stepwat_st`.
Currently implemented:
    * monthly summed transpiration
    * monthly mean bulk soil water content
    * annual and monthly mean air temperature
    * annual and monthly precipitation sum
    * annual sum of AET

@sideeffect Sets elements of `timeSteps_SXW`, updates
`used_OUTNPERIODS`, and adjusts variables `use`, `sumtype` (with a warning),
`first_orig`, and `last_orig` of `SW_Output`.
*/
void SW_OUT_set_SXWrequests(SW_OUT_DOM *OutDom, LOG_INFO *LogInfo) {
    // Update `used_OUTNPERIODS`:
    // SXW uses up to 2 time periods for the same output key: monthly and yearly
    OutDom->used_OUTNPERIODS = MAX(2, OutDom->used_OUTNPERIODS);

    // STEPWAT2 requires monthly summed transpiration
    set_SXWrequests_helper(
        OutDom, eSW_Transp, eSW_Month, eSW_Sum, "monthly transpiration", LogInfo
    );
    if (LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

    // STEPWAT2 requires monthly mean bulk soil water content
    set_SXWrequests_helper(
        OutDom,
        eSW_SWCBulk,
        eSW_Month,
        eSW_Avg,
        "monthly bulk soil water content",
        LogInfo
    );
    if (LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

    // STEPWAT2 requires annual and monthly mean air temperature
    set_SXWrequests_helper(
        OutDom,
        eSW_Temp,
        eSW_Month,
        eSW_Avg,
        "annual and monthly air temperature",
        LogInfo
    );
    if (LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }
    OutDom->timeSteps_SXW[eSW_Temp][1] = eSW_Year;

    // STEPWAT2 requires annual and monthly precipitation sum
    set_SXWrequests_helper(
        OutDom,
        eSW_Precip,
        eSW_Month,
        eSW_Sum,
        "annual and monthly precipitation",
        LogInfo
    );
    if (LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }
    OutDom->timeSteps_SXW[eSW_Precip][1] = eSW_Year;

    // STEPWAT2 requires annual sum of AET
    set_SXWrequests_helper(
        OutDom, eSW_AET, eSW_Year, eSW_Sum, "annual AET", LogInfo
    );
    if (LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }
}
#endif

/**
@brief Initialize all possible pointers in the struct SW_OUT_RUN to NULL

@param[out] OutRun Struct of type SW_OUT_RUN that holds output
    information that may change throughout simulation runs
@param[out] SW_PathOutputs Struct of type SW_PATH_OUTPUTS which
    holds basic information about output files and values
*/
void SW_OUT_init_ptrs(SW_OUT_RUN *OutRun, SW_PATH_OUTPUTS *SW_PathOutputs) {

#if defined(SW_OUTARRAY)
    int key;
    int column;
    ForEachOutKey(key
    ){ForEachOutPeriod(column){OutRun->p_OUT[key][column] = NULL;
#if defined(STEPWAT)
    OutRun->p_OUTsd[key][column] = NULL;
#elif defined(SWNETCDF)
    SW_PathOutputs->ncOutFiles[key][column] = NULL;
#endif

#if !defined(SWNETCDF)
    (void) SW_PathOutputs;
#endif
}
}
#else
    (void) SW_PathOutputs;
    (void) OutRun;
#endif
}

void SW_OUTDOM_init_ptrs(SW_OUT_DOM *OutDom) {
    int key;
    int column;

    ForEachOutKey(key) {
        for (column = 0; column < 5 * NVEGTYPES + MAX_LAYERS; column++) {
            OutDom->colnames_OUT[key][column] = NULL;
        }
    }

#ifdef RSOILWAT
    ForEachOutKey(key) { OutDom->outfile[key] = NULL; }
#endif

#if defined(SWNETCDF)
    SW_NCOUT_init_ptrs(&OutDom->netCDFOutput);
#endif
}

/**
 * @brief Initialize output values that live in the domain-level
 * (contained in SW_OUT_DOM)
 *
 * @param[out] OutDom OutDom Struct of type SW_OUT_DOM that holds output
 * information that do not change throughout simulation runs
 */
void SW_OUTDOM_construct(SW_OUT_DOM *OutDom) {

    int k;
    OutPeriod p;

    memset(OutDom, 0, sizeof(SW_OUT_DOM));

    OutDom->print_SW_Output = swTRUE;

#if defined(SW_OUTARRAY)
    ForEachOutPeriod(p) { OutDom->nrow_OUT[p] = 0; }
#endif

    /* attach the printing functions for each output
     * quantity to the appropriate element in the
     * output structure.  Using a loop makes it convenient
     * to simply add a line as new quantities are
     * implemented and leave the default case for every
     * thing else.
     */
    ForEachOutKey(k) {
        ForEachOutPeriod(p) {
            OutDom->timeSteps[k][p] = eSW_NoTime;

#ifdef STEPWAT
            OutDom->timeSteps_SXW[k][p] = eSW_NoTime;
#endif
        }

        // default values for `SW_Output`:
        OutDom->use[k] = swFALSE;
        OutDom->mykey[k] = (OutKey) k;
        OutDom->myobj[k] = key2obj[k];
        OutDom->sumtype[k] = eSW_Off;
        OutDom->has_sl[k] = has_key_soillayers((OutKey) k);
        OutDom->first_orig[k] = 1;
        OutDom->last_orig[k] = 366;

        // assign `get_XXX` functions
        switch (k) {
        case eSW_Temp:
#if defined(SW_OUTTEXT)
            OutDom->pfunc_text[k] =
                (void (*)(OutPeriod, SW_RUN *, LOG_INFO *)) get_temp_text;
#endif
#if defined(RSOILWAT) || defined(SWNETCDF)
            OutDom->pfunc_mem[k] =
                (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *)) get_temp_mem;
#elif defined(STEPWAT)
            OutDom->pfunc_agg[k] =
                (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *, LOG_INFO *)
                ) get_temp_agg;
            OutDom->pfunc_SXW[k] =
                (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *, LOG_INFO *)
                ) get_temp_SXW;
#endif
            break;

        case eSW_Precip:
#if defined(SW_OUTTEXT)
            OutDom->pfunc_text[k] =
                (void (*)(OutPeriod, SW_RUN *, LOG_INFO *)) get_precip_text;
#endif
#if defined(RSOILWAT) || defined(SWNETCDF)
            OutDom->pfunc_mem[k] =
                (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *)) get_precip_mem;
#elif defined(STEPWAT)
            OutDom->pfunc_agg[k] =
                (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *, LOG_INFO *)
                ) get_precip_agg;
            OutDom->pfunc_SXW[k] =
                (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *, LOG_INFO *)
                ) get_precip_SXW;
#endif
            break;

        case eSW_VWCBulk:
#if defined(SW_OUTTEXT)
            OutDom->pfunc_text[k] =
                (void (*)(OutPeriod, SW_RUN *, LOG_INFO *)) get_vwcBulk_text;
#endif
#if defined(RSOILWAT) || defined(SWNETCDF)
            OutDom->pfunc_mem[k] =
                (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *)) get_vwcBulk_mem;
#elif defined(STEPWAT)
            OutDom->pfunc_agg[k] =
                (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *, LOG_INFO *)
                ) get_vwcBulk_agg;
            OutDom->pfunc_SXW[k] =
                (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *, LOG_INFO *)
                ) get_none_outarray;
#endif
            break;

        case eSW_VWCMatric:
#if defined(SW_OUTTEXT)
            OutDom->pfunc_text[k] =
                (void (*)(OutPeriod, SW_RUN *, LOG_INFO *)) get_vwcMatric_text;
#endif
#if defined(RSOILWAT) || defined(SWNETCDF)
            OutDom->pfunc_mem[k] =
                (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *)) get_vwcMatric_mem;
#elif defined(STEPWAT)
            OutDom->pfunc_agg[k] =
                (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *, LOG_INFO *)
                ) get_vwcMatric_agg;
            OutDom->pfunc_SXW[k] =
                (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *, LOG_INFO *)
                ) get_none_outarray;
#endif
            break;

        case eSW_SWCBulk:
#if defined(SW_OUTTEXT)
            OutDom->pfunc_text[k] =
                (void (*)(OutPeriod, SW_RUN *, LOG_INFO *)) get_swcBulk_text;
#endif
#if defined(RSOILWAT) || defined(SWNETCDF)
            OutDom->pfunc_mem[k] =
                (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *)) get_swcBulk_mem;
#elif defined(STEPWAT)
            OutDom->pfunc_agg[k] =
                (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *, LOG_INFO *)
                ) get_swcBulk_agg;
            OutDom->pfunc_SXW[k] =
                (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *, LOG_INFO *)
                ) get_swcBulk_SXW;
#endif
            break;

        case eSW_SWPMatric:
#if defined(SW_OUTTEXT)
            OutDom->pfunc_text[k] =
                (void (*)(OutPeriod, SW_RUN *, LOG_INFO *)) get_swpMatric_text;
#endif
#if defined(RSOILWAT) || defined(SWNETCDF)
            OutDom->pfunc_mem[k] =
                (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *)) get_swpMatric_mem;
#elif defined(STEPWAT)
            OutDom->pfunc_agg[k] =
                (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *, LOG_INFO *)
                ) get_swpMatric_agg;
            OutDom->pfunc_SXW[k] =
                (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *, LOG_INFO *)
                ) get_none_outarray;
#endif
            break;

        case eSW_SWABulk:
#if defined(SW_OUTTEXT)
            OutDom->pfunc_text[k] =
                (void (*)(OutPeriod, SW_RUN *, LOG_INFO *)) get_swaBulk_text;
#endif
#if defined(RSOILWAT) || defined(SWNETCDF)
            OutDom->pfunc_mem[k] =
                (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *)) get_swaBulk_mem;
#elif defined(STEPWAT)
            OutDom->pfunc_agg[k] =
                (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *, LOG_INFO *)
                ) get_swaBulk_agg;
            OutDom->pfunc_SXW[k] =
                (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *, LOG_INFO *)
                ) get_none_outarray;
#endif
            break;

        case eSW_SWAMatric:
#if defined(SW_OUTTEXT)
            OutDom->pfunc_text[k] =
                (void (*)(OutPeriod, SW_RUN *, LOG_INFO *)) get_swaMatric_text;
#endif
#if defined(RSOILWAT) || defined(SWNETCDF)
            OutDom->pfunc_mem[k] =
                (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *)) get_swaMatric_mem;
#elif defined(STEPWAT)
            OutDom->pfunc_agg[k] =
                (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *, LOG_INFO *)
                ) get_swaMatric_agg;
            OutDom->pfunc_SXW[k] =
                (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *, LOG_INFO *)
                ) get_none_outarray;
#endif
            break;

        case eSW_SWA:
#if defined(SW_OUTTEXT)
            OutDom->pfunc_text[k] =
                (void (*)(OutPeriod, SW_RUN *, LOG_INFO *)) get_swa_text;
#endif
#if defined(RSOILWAT) || defined(SWNETCDF)
            OutDom->pfunc_mem[k] =
                (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *)) get_swa_mem;
#elif defined(STEPWAT)
            OutDom->pfunc_agg[k] =
                (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *, LOG_INFO *)
                ) get_swa_agg;
            OutDom->pfunc_SXW[k] =
                (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *, LOG_INFO *)
                ) get_none_outarray;
#endif
            break;

        case eSW_SurfaceWater:
#if defined(SW_OUTTEXT)
            OutDom->pfunc_text[k] = (void (*)(OutPeriod, SW_RUN *, LOG_INFO *)
            ) get_surfaceWater_text;
#endif
#if defined(RSOILWAT) || defined(SWNETCDF)
            OutDom->pfunc_mem[k] = (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *)
            ) get_surfaceWater_mem;
#elif defined(STEPWAT)
            OutDom->pfunc_agg[k] =
                (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *, LOG_INFO *)
                ) get_surfaceWater_agg;
            OutDom->pfunc_SXW[k] =
                (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *, LOG_INFO *)
                ) get_none_outarray;
#endif
            break;

        case eSW_Runoff:
#if defined(SW_OUTTEXT)
            OutDom->pfunc_text[k] = (void (*)(OutPeriod, SW_RUN *, LOG_INFO *)
            ) get_runoffrunon_text;
#endif
#if defined(RSOILWAT) || defined(SWNETCDF)
            OutDom->pfunc_mem[k] = (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *)
            ) get_runoffrunon_mem;
#elif defined(STEPWAT)
            OutDom->pfunc_agg[k] =
                (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *, LOG_INFO *)
                ) get_runoffrunon_agg;
            OutDom->pfunc_SXW[k] =
                (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *, LOG_INFO *)
                ) get_none_outarray;
#endif
            break;

        case eSW_Transp:
#if defined(SW_OUTTEXT)
            OutDom->pfunc_text[k] =
                (void (*)(OutPeriod, SW_RUN *, LOG_INFO *)) get_transp_text;
#endif
#if defined(RSOILWAT) || defined(SWNETCDF)
            OutDom->pfunc_mem[k] =
                (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *)) get_transp_mem;
#elif defined(STEPWAT)
            OutDom->pfunc_agg[k] =
                (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *, LOG_INFO *)
                ) get_transp_agg;
            OutDom->pfunc_SXW[k] =
                (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *, LOG_INFO *)
                ) get_transp_SXW;
#endif
            break;

        case eSW_EvapSoil:
#if defined(SW_OUTTEXT)
            OutDom->pfunc_text[k] =
                (void (*)(OutPeriod, SW_RUN *, LOG_INFO *)) get_evapSoil_text;
#endif
#if defined(RSOILWAT) || defined(SWNETCDF)
            OutDom->pfunc_mem[k] =
                (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *)) get_evapSoil_mem;
#elif defined(STEPWAT)
            OutDom->pfunc_agg[k] =
                (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *, LOG_INFO *)
                ) get_evapSoil_agg;
            OutDom->pfunc_SXW[k] =
                (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *, LOG_INFO *)
                ) get_none_outarray;
#endif
            break;

        case eSW_EvapSurface:
#if defined(SW_OUTTEXT)
            OutDom->pfunc_text[k] = (void (*)(OutPeriod, SW_RUN *, LOG_INFO *)
            ) get_evapSurface_text;
#endif
#if defined(RSOILWAT) || defined(SWNETCDF)
            OutDom->pfunc_mem[k] = (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *)
            ) get_evapSurface_mem;
#elif defined(STEPWAT)
            OutDom->pfunc_agg[k] =
                (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *, LOG_INFO *)
                ) get_evapSurface_agg;
            OutDom->pfunc_SXW[k] =
                (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *, LOG_INFO *)
                ) get_none_outarray;
#endif
            break;

        case eSW_Interception:
#if defined(SW_OUTTEXT)
            OutDom->pfunc_text[k] = (void (*)(OutPeriod, SW_RUN *, LOG_INFO *)
            ) get_interception_text;
#endif
#if defined(RSOILWAT) || defined(SWNETCDF)
            OutDom->pfunc_mem[k] = (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *)
            ) get_interception_mem;
#elif defined(STEPWAT)
            OutDom->pfunc_agg[k] =
                (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *, LOG_INFO *)
                ) get_interception_agg;
            OutDom->pfunc_SXW[k] =
                (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *, LOG_INFO *)
                ) get_none_outarray;
#endif
            break;

        case eSW_SoilInf:
#if defined(SW_OUTTEXT)
            OutDom->pfunc_text[k] =
                (void (*)(OutPeriod, SW_RUN *, LOG_INFO *)) get_soilinf_text;
#endif
#if defined(RSOILWAT) || defined(SWNETCDF)
            OutDom->pfunc_mem[k] =
                (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *)) get_soilinf_mem;
#elif defined(STEPWAT)
            OutDom->pfunc_agg[k] =
                (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *, LOG_INFO *)
                ) get_soilinf_agg;
            OutDom->pfunc_SXW[k] =
                (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *, LOG_INFO *)
                ) get_none_outarray;
#endif
            break;

        case eSW_LyrDrain:
#if defined(SW_OUTTEXT)
            OutDom->pfunc_text[k] =
                (void (*)(OutPeriod, SW_RUN *, LOG_INFO *)) get_lyrdrain_text;
#endif
#if defined(RSOILWAT) || defined(SWNETCDF)
            OutDom->pfunc_mem[k] =
                (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *)) get_lyrdrain_mem;
#elif defined(STEPWAT)
            OutDom->pfunc_agg[k] =
                (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *, LOG_INFO *)
                ) get_lyrdrain_agg;
            OutDom->pfunc_SXW[k] =
                (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *, LOG_INFO *)
                ) get_none_outarray;
#endif
            break;

        case eSW_HydRed:
#if defined(SW_OUTTEXT)
            OutDom->pfunc_text[k] =
                (void (*)(OutPeriod, SW_RUN *, LOG_INFO *)) get_hydred_text;
#endif
#if defined(RSOILWAT) || defined(SWNETCDF)
            OutDom->pfunc_mem[k] =
                (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *)) get_hydred_mem;
#elif defined(STEPWAT)
            OutDom->pfunc_agg[k] =
                (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *, LOG_INFO *)
                ) get_hydred_agg;
            OutDom->pfunc_SXW[k] =
                (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *, LOG_INFO *)
                ) get_none_outarray;
#endif
            break;

        case eSW_AET:
#if defined(SW_OUTTEXT)
            OutDom->pfunc_text[k] =
                (void (*)(OutPeriod, SW_RUN *, LOG_INFO *)) get_aet_text;
#endif
#if defined(RSOILWAT) || defined(SWNETCDF)
            OutDom->pfunc_mem[k] =
                (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *)) get_aet_mem;
#elif defined(STEPWAT)
            OutDom->pfunc_agg[k] =
                (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *, LOG_INFO *)
                ) get_aet_agg;
            OutDom->pfunc_SXW[k] =
                (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *, LOG_INFO *)
                ) get_aet_SXW;
#endif
            break;

        case eSW_PET:
#if defined(SW_OUTTEXT)
            OutDom->pfunc_text[k] =
                (void (*)(OutPeriod, SW_RUN *, LOG_INFO *)) get_pet_text;
#endif
#if defined(RSOILWAT) || defined(SWNETCDF)
            OutDom->pfunc_mem[k] =
                (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *)) get_pet_mem;
#elif defined(STEPWAT)
            OutDom->pfunc_agg[k] =
                (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *, LOG_INFO *)
                ) get_pet_agg;
            OutDom->pfunc_SXW[k] =
                (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *, LOG_INFO *)
                ) get_none_outarray;
#endif
            break;

        case eSW_WetDays:
#if defined(SW_OUTTEXT)
            OutDom->pfunc_text[k] =
                (void (*)(OutPeriod, SW_RUN *, LOG_INFO *)) get_wetdays_text;
#endif
#if defined(RSOILWAT) || defined(SWNETCDF)
            OutDom->pfunc_mem[k] =
                (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *)) get_wetdays_mem;
#elif defined(STEPWAT)
            OutDom->pfunc_agg[k] =
                (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *, LOG_INFO *)
                ) get_wetdays_agg;
            OutDom->pfunc_SXW[k] =
                (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *, LOG_INFO *)
                ) get_none_outarray;
#endif
            break;

        case eSW_SnowPack:
#if defined(SW_OUTTEXT)
            OutDom->pfunc_text[k] =
                (void (*)(OutPeriod, SW_RUN *, LOG_INFO *)) get_snowpack_text;
#endif
#if defined(RSOILWAT) || defined(SWNETCDF)
            OutDom->pfunc_mem[k] =
                (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *)) get_snowpack_mem;
#elif defined(STEPWAT)
            OutDom->pfunc_agg[k] =
                (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *, LOG_INFO *)
                ) get_snowpack_agg;
            OutDom->pfunc_SXW[k] =
                (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *, LOG_INFO *)
                ) get_none_outarray;
#endif
            break;

        case eSW_DeepSWC:
#if defined(SW_OUTTEXT)
            OutDom->pfunc_text[k] =
                (void (*)(OutPeriod, SW_RUN *, LOG_INFO *)) get_deepswc_text;
#endif
#if defined(RSOILWAT) || defined(SWNETCDF)
            OutDom->pfunc_mem[k] =
                (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *)) get_deepswc_mem;
#elif defined(STEPWAT)
            OutDom->pfunc_agg[k] =
                (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *, LOG_INFO *)
                ) get_deepswc_agg;
            OutDom->pfunc_SXW[k] =
                (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *, LOG_INFO *)
                ) get_none_outarray;
#endif
            break;

        case eSW_SoilTemp:
#if defined(SW_OUTTEXT)
            OutDom->pfunc_text[k] =
                (void (*)(OutPeriod, SW_RUN *, LOG_INFO *)) get_soiltemp_text;
#endif
#if defined(RSOILWAT) || defined(SWNETCDF)
            OutDom->pfunc_mem[k] =
                (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *)) get_soiltemp_mem;
#elif defined(STEPWAT)
            OutDom->pfunc_agg[k] =
                (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *, LOG_INFO *)
                ) get_soiltemp_agg;
            OutDom->pfunc_SXW[k] =
                (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *, LOG_INFO *)
                ) get_none_outarray;
#endif
            break;

        case eSW_Frozen:
#if defined(SW_OUTTEXT)
            OutDom->pfunc_text[k] =
                (void (*)(OutPeriod, SW_RUN *, LOG_INFO *)) get_frozen_text;
#endif
#if defined(RSOILWAT) || defined(SWNETCDF)
            OutDom->pfunc_mem[k] =
                (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *)) get_frozen_mem;
#elif defined(STEPWAT)
            OutDom->pfunc_agg[k] =
                (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *, LOG_INFO *)
                ) get_frozen_agg;
            OutDom->pfunc_SXW[k] =
                (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *, LOG_INFO *)
                ) get_none_outarray;
#endif
            break;

        case eSW_Estab:
#if defined(SW_OUTTEXT)
            OutDom->pfunc_text[k] =
                (void (*)(OutPeriod, SW_RUN *, LOG_INFO *)) get_estab_text;
#endif
#if defined(RSOILWAT) || defined(SWNETCDF)
            OutDom->pfunc_mem[k] =
                (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *)) get_estab_mem;
#elif defined(STEPWAT)
            OutDom->pfunc_agg[k] =
                (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *, LOG_INFO *)
                ) get_estab_agg;
            OutDom->pfunc_SXW[k] =
                (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *, LOG_INFO *)
                ) get_none_outarray;
#endif
            break;

        case eSW_CO2Effects:
#if defined(SW_OUTTEXT)
            OutDom->pfunc_text[k] =
                (void (*)(OutPeriod, SW_RUN *, LOG_INFO *)) get_co2effects_text;
#endif
#if defined(RSOILWAT) || defined(SWNETCDF)
            OutDom->pfunc_mem[k] = (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *)
            ) get_co2effects_mem;
#elif defined(STEPWAT)
            OutDom->pfunc_agg[k] =
                (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *, LOG_INFO *)
                ) get_co2effects_agg;
            OutDom->pfunc_SXW[k] =
                (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *, LOG_INFO *)
                ) get_none_outarray;
#endif
            break;

        case eSW_Biomass:
#if defined(SW_OUTTEXT)
            OutDom->pfunc_text[k] =
                (void (*)(OutPeriod, SW_RUN *, LOG_INFO *)) get_biomass_text;
#endif
#if defined(RSOILWAT) || defined(SWNETCDF)
            OutDom->pfunc_mem[k] =
                (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *)) get_biomass_mem;
#elif defined(STEPWAT)
            OutDom->pfunc_agg[k] =
                (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *, LOG_INFO *)
                ) get_biomass_agg;
            OutDom->pfunc_SXW[k] =
                (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *, LOG_INFO *)
                ) get_none_outarray;
#endif
            break;

        default:
#if defined(SW_OUTTEXT)
            OutDom->pfunc_text[k] =
                (void (*)(OutPeriod, SW_RUN *, LOG_INFO *)) get_none_text;
#endif
#if defined(RSOILWAT) || defined(SWNETCDF)
            OutDom->pfunc_mem[k] = (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *)
            ) get_none_outarray_mem;
#elif defined(STEPWAT)
            OutDom->pfunc_agg[k] =
                (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *, LOG_INFO *)
                ) get_none_outarray;
            OutDom->pfunc_SXW[k] =
                (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *, LOG_INFO *)
                ) get_none_outarray;
#endif
            break;
        }
    } // end of loop across output keys
}

void SW_OUT_construct(
    Bool zeroOutStruct,
    SW_PATH_OUTPUTS *SW_PathOutputs,
    SW_OUT_DOM *OutDom,
    SW_OUT_RUN *OutRun,
    LOG_INFO *LogInfo
) {
    /* =================================================== */
    OutPeriod p;

    if (zeroOutStruct) {
        memset(OutRun, 0, sizeof(SW_OUT_RUN));
    }

#if defined(SW_OUTTEXT)
    ForEachOutPeriod(p) {
        SW_PathOutputs->make_soil[p] = swFALSE;
        SW_PathOutputs->make_regular[p] = swFALSE;
    }
#else
    /* Silence compiler */
    (void) SW_PathOutputs;
#endif

#if defined(SW_OUTARRAY)
    ForEachOutPeriod(p) { OutRun->irow_OUT[p] = 0; }
#else
    (void) OutRun;
#endif

#if defined(SWNETCDF)
    SW_OUT_construct_outarray(OutDom, OutRun, LogInfo);
    if (LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

#else
    (void) LogInfo;
    (void) OutDom;
#endif
}

void SW_OUT_deconstruct(Bool full_reset, SW_RUN *sw) {

#if defined(SW_OUTARRAY)
    if (full_reset) {
        SW_OUT_deconstruct_outarray(&sw->OutRun);
    }
#else
    (void) sw;
    (void) full_reset;
#endif

#if defined(SWNETCDF)
    OutPeriod pd;
    unsigned int k;
    unsigned int file;

    ForEachOutKey(k) {
        ForEachOutPeriod(pd) {
            if (!isnull(sw->SW_PathOutputs.ncOutFiles[k][pd])) {
                for (file = 0; file < sw->SW_PathOutputs.numOutFiles; file++) {
                    if (!isnull(sw->SW_PathOutputs.ncOutFiles[k][pd][file])) {

                        free(sw->SW_PathOutputs.ncOutFiles[k][pd][file]);
                        sw->SW_PathOutputs.ncOutFiles[k][pd][file] = NULL;
                    }
                }

                free((void *) sw->SW_PathOutputs.ncOutFiles[k][pd]);
                sw->SW_PathOutputs.ncOutFiles[k][pd] = NULL;
            }

            if (!isnull(sw->SW_PathOutputs.ncOutFiles[k][pd])) {
                free((void *) sw->SW_PathOutputs.ncOutFiles[k][pd]);
                sw->SW_PathOutputs.ncOutFiles[k][pd] = NULL;
            }
        }
    }
#endif
}

/** Specify output dimensions

Note to programmer: this function must match what `get_*()` implement.

@param[in] tLayers Number of soil layers
@param[in] n_evap_lyrs Number of soil layers with evaporation
@param[in] nTaxaEstabl Number of taxa used for establishment output.
@param[out] ncol_OUT Calculated number of output combinations across
    variables, soil layers, and plant functional types (vegtypes)
    as array of length SW_OUTNKEYS.
@param[out] nvar_OUT Specified number of output variables (per outkey)
    as array of length SW_OUTNKEYS.
@param[out] nsl_OUT Specified number of output soil layer per variable
    as array of size SW_OUTNKEYS by SW_OUTNMAXVARS.
@param[out] npft_OUT Specified number of output vegtypes per variable
    as array of size SW_OUTNKEYS by SW_OUTNMAXVARS.
*/
void SW_OUT_set_ncol(
    unsigned int tLayers,
    unsigned int n_evap_lyrs,
    unsigned int nTaxaEstabl,
    IntUS ncol_OUT[],
    IntUS nvar_OUT[],
    IntUS nsl_OUT[][SW_OUTNMAXVARS],
    IntUS npft_OUT[][SW_OUTNMAXVARS]
) {

    unsigned int key;
    unsigned int ivar;
    IntUS tmp;

    //--- Set number of output variables ------
    nvar_OUT[eSW_AllWthr] = 0;
    nvar_OUT[eSW_Temp] = 6;
    nvar_OUT[eSW_Precip] = 5;
    nvar_OUT[eSW_SoilInf] = 1;
    nvar_OUT[eSW_Runoff] = 4;
    nvar_OUT[eSW_AllH2O] = 0;
    nvar_OUT[eSW_VWCBulk] = 1;
    nvar_OUT[eSW_VWCMatric] = 1;
    nvar_OUT[eSW_SWCBulk] = 1;
    nvar_OUT[eSW_SWABulk] = 1;
    nvar_OUT[eSW_SWAMatric] = 1;
    nvar_OUT[eSW_SWA] = 1;
    nvar_OUT[eSW_SWPMatric] = 1;
    nvar_OUT[eSW_SurfaceWater] = 1;
    // eSW_Transp: NVEGTYPES plus totals
    nvar_OUT[eSW_Transp] = 2;
    nvar_OUT[eSW_EvapSoil] = 1;
    // eSW_EvapSurface: NVEGTYPES plus totals, litter, surface water
    nvar_OUT[eSW_EvapSurface] = 4;
    // eSW_Interception: NVEGTYPES plus totals, litter
    nvar_OUT[eSW_Interception] = 3;
    nvar_OUT[eSW_LyrDrain] = 1;
    // eSW_HydRed: NVEGTYPES plus totals
    nvar_OUT[eSW_HydRed] = 2;
    nvar_OUT[eSW_ET] = 0;
    nvar_OUT[eSW_AET] = 6;
    nvar_OUT[eSW_PET] = 5;
    nvar_OUT[eSW_WetDays] = 1;
    nvar_OUT[eSW_SnowPack] = 2;
    nvar_OUT[eSW_DeepSWC] = 1;
    nvar_OUT[eSW_SoilTemp] = 3;
    nvar_OUT[eSW_Frozen] = 1;
    nvar_OUT[eSW_AllVeg] = 0;
    nvar_OUT[eSW_Estab] = nTaxaEstabl;
    nvar_OUT[eSW_CO2Effects] = 2;
    // eSW_Biomass: fCover for NVEGTYPES plus bare-ground
    //    biomass for NVEGTYPES plus totals and litter
    //    biolive for NVEGTYPES plus totals
    //    LAI
    nvar_OUT[eSW_Biomass] = 8;


    //--- Set number of soil layer and/or pft (vegtype) for each variable ------

    // init
    ForEachOutKey(key) {
        for (ivar = 0; ivar < SW_OUTNMAXVARS; ivar++) {
            nsl_OUT[key][ivar] = 0;
            npft_OUT[key][ivar] = 0;
        }
    }

    nsl_OUT[eSW_VWCBulk][0] = tLayers;

    nsl_OUT[eSW_VWCMatric][0] = tLayers;

    nsl_OUT[eSW_SWCBulk][0] = tLayers;

    nsl_OUT[eSW_SWABulk][0] = tLayers;

    nsl_OUT[eSW_SWAMatric][0] = tLayers;

    nsl_OUT[eSW_SWAMatric][0] = tLayers;

    nsl_OUT[eSW_SWA][0] = tLayers;
    npft_OUT[eSW_SWA][0] = NVEGTYPES;

    nsl_OUT[eSW_SWPMatric][0] = tLayers;

    nsl_OUT[eSW_EvapSoil][0] = n_evap_lyrs;

    nsl_OUT[eSW_LyrDrain][0] = tLayers - 1;

    nsl_OUT[eSW_WetDays][0] = tLayers;

    nsl_OUT[eSW_SoilTemp][0] = tLayers;
    nsl_OUT[eSW_SoilTemp][1] = tLayers;
    nsl_OUT[eSW_SoilTemp][2] = tLayers;

    nsl_OUT[eSW_Frozen][0] = tLayers;

    npft_OUT[eSW_CO2Effects][0] = NVEGTYPES;
    npft_OUT[eSW_CO2Effects][1] = NVEGTYPES;


    // OutKeys that combine variables of mixed-dimensions
    nsl_OUT[eSW_Transp][0] = tLayers; // 0: TRANSP__transp_total
    nsl_OUT[eSW_Transp][1] = tLayers; // 1: TRANSP__transp
    npft_OUT[eSW_Transp][1] = NVEGTYPES;

    npft_OUT[eSW_EvapSurface][1] = NVEGTYPES; // 1: EVAPSURFACE__evap_veg

    npft_OUT[eSW_Interception][1] = NVEGTYPES; // 1: INTERCEPTION__int_veg

    nsl_OUT[eSW_HydRed][0] = tLayers; // 0: HYDRED__hydred_total
    nsl_OUT[eSW_HydRed][1] = tLayers; // 1: HYDRED__hydred
    npft_OUT[eSW_HydRed][1] = NVEGTYPES;

    npft_OUT[eSW_Biomass][1] = NVEGTYPES; // 1: BIOMASS__veg.cov.fCover
    npft_OUT[eSW_Biomass][3] = NVEGTYPES; // 3: BIOMASS__veg.biomass_inveg
    npft_OUT[eSW_Biomass][6] = NVEGTYPES; // 6: BIOMASS__veg.biolive_inveg


    //--- Sum up number of output combinations across variables - soil layers -
    // vegtypes ------
    ForEachOutKey(key) {
        ncol_OUT[key] = 0;

        for (ivar = 0; ivar < nvar_OUT[key]; ivar++) {
            tmp = 1; // variable has dimension T
            if (nsl_OUT[key][ivar] > 0) {
                // variable has dimension TZ
                tmp = nsl_OUT[key][ivar];
                if (npft_OUT[key][ivar] > 0) {
                    // variable has dimension TZV
                    tmp *= npft_OUT[key][ivar];
                }
            } else if (npft_OUT[key][ivar] > 0) {
                // variable has dimension TV
                tmp = npft_OUT[key][ivar];
            }

            ncol_OUT[key] += tmp;
        }
    }
}

/**
@brief Set column/variable names

Order of outputs must match up with all `get_XXX` functions and with
indexing macros iOUT and iOUT2; particularly, output variables with
values for each of `N` soil layers for `k` different (e.g., vegetation)
components (e.g., transpiration, SWA, and hydraulic redistribution) report
based on a loop over components within
which a loop over soil layers is nested, e.g.,
`C1_Lyr1, C1_Lyr2, ..., C1_LyrN, C2_Lyr1, ..., C2_LyrN, ...,
Ck_Lyr1, ..., Ck_LyrN`

@param[in] tLayers Total number of soil layers
@param[in] *parmsIn List of structs of type SW_VEGESTAB_INFO_INPUTS holding
    input information about every vegetation species
@param[in] ncol_OUT Number of output columns for each output key
@param[out] colnames_OUT Names of output columns for each output key
@param[out] LogInfo Holds information on warnings and errors

@sideeffect Set values of colnames_OUT
*/
void SW_OUT_set_colnames(
    unsigned int tLayers,
    SW_VEGESTAB_INFO_INPUTS *parmsIn,
    const IntUS ncol_OUT[],
    char *colnames_OUT[][5 * NVEGTYPES + MAX_LAYERS],
    LOG_INFO *LogInfo
) {
#ifdef SWDEBUG
    int debug = 0;
#endif

    unsigned int i;
    unsigned int j;
    char ctemp[50];
    const char *Layers_names[MAX_LAYERS] = {
        "Lyr_1",  "Lyr_2",  "Lyr_3",  "Lyr_4",  "Lyr_5",  "Lyr_6",  "Lyr_7",
        "Lyr_8",  "Lyr_9",  "Lyr_10", "Lyr_11", "Lyr_12", "Lyr_13", "Lyr_14",
        "Lyr_15", "Lyr_16", "Lyr_17", "Lyr_18", "Lyr_19", "Lyr_20", "Lyr_21",
        "Lyr_22", "Lyr_23", "Lyr_24", "Lyr_25"
    };
    const char *cnames_VegTypes[NVEGTYPES + 2] = {
        "total", "tree", "shrub", "forbs", "grass", "litter"
    };

    const char *cnames_eSW_Temp[] = {"max_C", "min_C", "avg_C", "surfaceTemp"};
    const char *cnames_eSW_Precip[] = {
        "ppt", "rain", "snow_fall", "snowmelt", "snowloss"
    };
    const char *cnames_eSW_SoilInf[] = {"soil_inf"};
    const char *cnames_eSW_Runoff[] = {
        "net", "ponded_runoff", "snowmelt_runoff", "ponded_runon"
    };
    const char *cnames_eSW_SurfaceWater[] = {"surfaceWater_cm"};
    const char *cnames_add_eSW_EvapSurface[] = {"evap_surfaceWater"};
    const char *cnames_eSW_AET[] = {
        "evapotr_cm", "tran_cm", "esoil_cm", "ecnw_cm", "esurf_cm", "esnow_cm"
    };
    const char *cnames_eSW_PET[] = {
        "pet_cm", "H_oh_MJm-2", "H_ot_MJm-2", "H_gh_MJm-2", "H_gt_MJm-2"
    };
    const char *cnames_eSW_SnowPack[] = {
        "snowpackWaterEquivalent_cm", "snowdepth_cm"
    };
    const char *cnames_eSW_DeepSWC[] = {"lowLayerDrain_cm"};
    const char *cnames_eSW_CO2Effects[] = {"BioMult", "WUEMult"};

#ifdef SWDEBUG
    if (debug) {
        sw_printf("SW_OUT_set_colnames: set columns for 'eSW_Temp' ...");
    }
#endif
    for (i = 0; i < ncol_OUT[eSW_Temp]; i++) {
        if (i < 3) {
            // Normal air temperature columns
            (void) sw_memccpy(
                ctemp, (char *) cnames_eSW_Temp[i], '\0', sizeof ctemp
            );
        } else {
            // Surface temperature columns
            (void) snprintf(
                ctemp,
                sizeof ctemp,
                "%s_%s",
                cnames_eSW_Temp[3],
                cnames_eSW_Temp[i % 3]
            );
        }

        colnames_OUT[eSW_Temp][i] = Str_Dup(ctemp, LogInfo);
        if (LogInfo->stopRun) {
            return; // Exit function prematurely due to error
        }
    }
#ifdef SWDEBUG
    if (debug) {
        sw_printf(" 'eSW_Precip' ...");
    }
#endif
    for (i = 0; i < ncol_OUT[eSW_Precip]; i++) {
        colnames_OUT[eSW_Precip][i] = Str_Dup(cnames_eSW_Precip[i], LogInfo);
        if (LogInfo->stopRun) {
            return; // Exit function prematurely due to error
        }
    }
#ifdef SWDEBUG
    if (debug) {
        sw_printf(" 'eSW_SoilInf' ...");
    }
#endif
    for (i = 0; i < ncol_OUT[eSW_SoilInf]; i++) {
        // NOLINTNEXTLINE(clang-analyzer-core.CallAndMessage)
        colnames_OUT[eSW_SoilInf][i] = Str_Dup(cnames_eSW_SoilInf[i], LogInfo);
        if (LogInfo->stopRun) {
            return; // Exit function prematurely due to error
        }
    }
#ifdef SWDEBUG
    if (debug) {
        sw_printf(" 'eSW_Runoff' ...");
    }
#endif
    for (i = 0; i < ncol_OUT[eSW_Runoff]; i++) {
        colnames_OUT[eSW_Runoff][i] = Str_Dup(cnames_eSW_Runoff[i], LogInfo);
        if (LogInfo->stopRun) {
            return; // Exit function prematurely due to error
        }
    }
#ifdef SWDEBUG
    if (debug) {
        sw_printf(" 'eSW_VWCBulk' ...");
    }
#endif
    for (i = 0; i < ncol_OUT[eSW_VWCBulk]; i++) {
        colnames_OUT[eSW_VWCBulk][i] = Str_Dup(Layers_names[i], LogInfo);
        if (LogInfo->stopRun) {
            return; // Exit function prematurely due to error
        }
    }
#ifdef SWDEBUG
    if (debug) {
        sw_printf(" 'eSW_VWCMatric' ...");
    }
#endif
    for (i = 0; i < ncol_OUT[eSW_VWCMatric]; i++) {
        colnames_OUT[eSW_VWCMatric][i] = Str_Dup(Layers_names[i], LogInfo);
        if (LogInfo->stopRun) {
            return; // Exit function prematurely due to error
        }
    }
#ifdef SWDEBUG
    if (debug) {
        sw_printf(" 'eSW_SWCBulk' ...");
    }
#endif
    for (i = 0; i < ncol_OUT[eSW_SWCBulk]; i++) {
        colnames_OUT[eSW_SWCBulk][i] = Str_Dup(Layers_names[i], LogInfo);
        if (LogInfo->stopRun) {
            return; // Exit function prematurely due to error
        }
    }
#ifdef SWDEBUG
    if (debug) {
        sw_printf(" 'eSW_SWABulk' ...");
    }
#endif
    for (i = 0; i < ncol_OUT[eSW_SWABulk]; i++) {
        colnames_OUT[eSW_SWABulk][i] = Str_Dup(Layers_names[i], LogInfo);
        if (LogInfo->stopRun) {
            return; // Exit function prematurely due to error
        }
    }
#ifdef SWDEBUG
    if (debug) {
        sw_printf(" 'eSW_SWA' ...");
    }
#endif
    for (i = 0; i < tLayers; i++) {
        for (j = 0; j < NVEGTYPES; j++) {
            (void) snprintf(
                ctemp,
                sizeof ctemp,
                "swa_%s_%s",
                cnames_VegTypes[j + 1], // j+1 since no total column for swa.
                Layers_names[i]
            );
            colnames_OUT[eSW_SWA][i + j * tLayers] = Str_Dup(ctemp, LogInfo);
            if (LogInfo->stopRun) {
                return; // Exit function prematurely due to error
            }
        }
    }
#ifdef SWDEBUG
    if (debug) {
        sw_printf(" 'eSW_SWAMatric' ...");
    }
#endif
    for (i = 0; i < ncol_OUT[eSW_SWAMatric]; i++) {
        colnames_OUT[eSW_SWAMatric][i] = Str_Dup(Layers_names[i], LogInfo);
        if (LogInfo->stopRun) {
            return; // Exit function prematurely due to error
        }
    }
#ifdef SWDEBUG
    if (debug) {
        sw_printf(" 'eSW_SWPMatric' ...");
    }
#endif
    for (i = 0; i < ncol_OUT[eSW_SWPMatric]; i++) {
        colnames_OUT[eSW_SWPMatric][i] = Str_Dup(Layers_names[i], LogInfo);
        if (LogInfo->stopRun) {
            return; // Exit function prematurely due to error
        }
    }
#ifdef SWDEBUG
    if (debug) {
        sw_printf(" 'eSW_SurfaceWater' ...");
    }
#endif
    for (i = 0; i < ncol_OUT[eSW_SurfaceWater]; i++) {
        // NOLINTBEGIN(clang-analyzer-core.CallAndMessage)
        colnames_OUT[eSW_SurfaceWater][i] =
            Str_Dup(cnames_eSW_SurfaceWater[i], LogInfo);
        // NOLINTEND(clang-analyzer-core.CallAndMessage)
        if (LogInfo->stopRun) {
            return; // Exit function prematurely due to error
        }
    }
#ifdef SWDEBUG
    if (debug) {
        sw_printf(" 'eSW_Transp' ...");
    }
#endif
    for (i = 0; i < tLayers; i++) {
        for (j = 0; j < NVEGTYPES + 1; j++) {
            (void) snprintf(
                ctemp,
                sizeof ctemp,
                "transp_%s_%s",
                cnames_VegTypes[j],
                Layers_names[i]
            );
            colnames_OUT[eSW_Transp][i + j * tLayers] = Str_Dup(ctemp, LogInfo);
            if (LogInfo->stopRun) {
                return; // Exit function prematurely due to error
            }
        }
    }
#ifdef SWDEBUG
    if (debug) {
        sw_printf(" 'eSW_EvapSoil' ...");
    }
#endif
    for (i = 0; i < ncol_OUT[eSW_EvapSoil]; i++) {
        colnames_OUT[eSW_EvapSoil][i] = Str_Dup(Layers_names[i], LogInfo);
        if (LogInfo->stopRun) {
            return; // Exit function prematurely due to error
        }
    }
#ifdef SWDEBUG
    if (debug) {
        sw_printf(" 'eSW_EvapSurface' ...");
    }
#endif
    for (i = 0; i < NVEGTYPES + 2; i++) {
        (void) snprintf(ctemp, sizeof ctemp, "evap_%s", cnames_VegTypes[i]);
        colnames_OUT[eSW_EvapSurface][i] = Str_Dup(ctemp, LogInfo);
        if (LogInfo->stopRun) {
            return; // Exit function prematurely due to error
        }
    }
    for (i = NVEGTYPES + 2; i < ncol_OUT[eSW_EvapSurface]; i++) {
        colnames_OUT[eSW_EvapSurface][i] =
            Str_Dup(cnames_add_eSW_EvapSurface[i - (NVEGTYPES + 2)], LogInfo);
        if (LogInfo->stopRun) {
            return; // Exit function prematurely due to error
        }
    }
#ifdef SWDEBUG
    if (debug) {
        sw_printf(" 'eSW_Interception' ...");
    }
#endif
    for (i = 0; i < NVEGTYPES + 2; i++) {
        (void) snprintf(ctemp, sizeof ctemp, "int_%s", cnames_VegTypes[i]);
        colnames_OUT[eSW_Interception][i] = Str_Dup(ctemp, LogInfo);
        if (LogInfo->stopRun) {
            return; // Exit function prematurely due to error
        }
    }
#ifdef SWDEBUG
    if (debug) {
        sw_printf(" 'eSW_LyrDrain' ...");
    }
#endif
    for (i = 0; i < ncol_OUT[eSW_LyrDrain]; i++) {
        colnames_OUT[eSW_LyrDrain][i] = Str_Dup(Layers_names[i], LogInfo);
        if (LogInfo->stopRun) {
            return; // Exit function prematurely due to error
        }
    }
#ifdef SWDEBUG
    if (debug) {
        sw_printf(" 'eSW_HydRed' ...");
    }
#endif
    for (i = 0; i < tLayers; i++) {
        for (j = 0; j < NVEGTYPES + 1; j++) {
            (void) snprintf(
                ctemp,
                sizeof ctemp,
                "%s_%s",
                cnames_VegTypes[j],
                Layers_names[i]
            );
            colnames_OUT[eSW_HydRed][i + j * tLayers] = Str_Dup(ctemp, LogInfo);
            if (LogInfo->stopRun) {
                return; // Exit function prematurely due to error
            }
        }
    }
#ifdef SWDEBUG
    if (debug) {
        sw_printf(" 'eSW_AET' ...");
    }
#endif
    for (i = 0; i < ncol_OUT[eSW_AET]; i++) {
        colnames_OUT[eSW_AET][i] = Str_Dup(cnames_eSW_AET[i], LogInfo);
        if (LogInfo->stopRun) {
            return; // Exit function prematurely due to error
        }
    }
#ifdef SWDEBUG
    if (debug) {
        sw_printf(" 'eSW_PET' ...");
    }
#endif
    for (i = 0; i < ncol_OUT[eSW_PET]; i++) {
        colnames_OUT[eSW_PET][i] = Str_Dup(cnames_eSW_PET[i], LogInfo);
        if (LogInfo->stopRun) {
            return; // Exit function prematurely due to error
        }
    }
#ifdef SWDEBUG
    if (debug) {
        sw_printf(" 'eSW_WetDays' ...");
    }
#endif
    for (i = 0; i < ncol_OUT[eSW_WetDays]; i++) {
        colnames_OUT[eSW_WetDays][i] = Str_Dup(Layers_names[i], LogInfo);
        if (LogInfo->stopRun) {
            return; // Exit function prematurely due to error
        }
    }
#ifdef SWDEBUG
    if (debug) {
        sw_printf(" 'eSW_SnowPack' ...");
    }
#endif
    for (i = 0; i < ncol_OUT[eSW_SnowPack]; i++) {
        colnames_OUT[eSW_SnowPack][i] =
            Str_Dup(cnames_eSW_SnowPack[i], LogInfo);
        if (LogInfo->stopRun) {
            return; // Exit function prematurely due to error
        }
    }
#ifdef SWDEBUG
    if (debug) {
        sw_printf(" 'eSW_DeepSWC' ...");
    }
#endif
    for (i = 0; i < ncol_OUT[eSW_DeepSWC]; i++) {
        colnames_OUT[eSW_DeepSWC][i] = Str_Dup(cnames_eSW_DeepSWC[i], LogInfo);
        if (LogInfo->stopRun) {
            return; // Exit function prematurely due to error
        }
    }
#ifdef SWDEBUG
    if (debug) {
        sw_printf(" 'eSW_SoilTemp' ...");
    }
#endif
    j = 0; // Layer variable for the next for-loop, 0 is first layer not surface
    for (i = 0; i < ncol_OUT[eSW_SoilTemp]; i++) {

        // Check if ready to go onto next layer (added all min/max/avg headers
        // for layer)
        if (i % 3 == 0 && i > 1) {
            j++;
        }

        // For layers 1 through ncol_OUT[eSW_SoilTemp]
        (void) snprintf(
            ctemp,
            sizeof ctemp,
            "%s_%s",
            Layers_names[j],
            cnames_eSW_Temp[i % 3]
        );

        colnames_OUT[eSW_SoilTemp][i] = Str_Dup(ctemp, LogInfo);
        if (LogInfo->stopRun) {
            return; // Exit function prematurely due to error
        }
    }

#ifdef SWDEBUG
    if (debug) {
        sw_printf(" 'eSW_Frozen' ...");
    }
#endif
    for (i = 0; i < ncol_OUT[eSW_Frozen]; i++) {
        colnames_OUT[eSW_Frozen][i] = Str_Dup(Layers_names[i], LogInfo);
        if (LogInfo->stopRun) {
            return; // Exit function prematurely due to error
        }
    }
#ifdef SWDEBUG
    if (debug) {
        sw_printf(" 'eSW_Estab' ...");
    }
#endif
    for (i = 0; i < ncol_OUT[eSW_Estab]; i++) {
        colnames_OUT[eSW_Estab][i] = Str_Dup(parmsIn[i].sppname, LogInfo);
        if (LogInfo->stopRun) {
            return; // Exit function prematurely due to error
        }
    }
#ifdef SWDEBUG
    if (debug) {
        sw_printf(" 'eSW_CO2Effects' ...");
    }
#endif
    for (i = 0; i < 2; i++) {
        for (j = 0; j < NVEGTYPES; j++) {
            (void) snprintf(
                ctemp,
                sizeof ctemp,
                "%s_%s",
                cnames_eSW_CO2Effects[i],
                cnames_VegTypes[j + 1] // j+1 since no total column
            );
            colnames_OUT[eSW_CO2Effects][j + i * NVEGTYPES] =
                Str_Dup(ctemp, LogInfo);
            if (LogInfo->stopRun) {
                return; // Exit function prematurely due to error
            }
        }
    }

#ifdef SWDEBUG
    if (debug) {
        sw_printf(" 'eSW_Biomass' ...");
    }
#endif
    i = 0;
    (void) sw_memccpy(ctemp, "fCover_BareGround", '\0', sizeof ctemp);
    colnames_OUT[eSW_Biomass][i] = Str_Dup(ctemp, LogInfo);
    if (LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }
    i = 1;
    for (j = 0; j < NVEGTYPES; j++) {
        (void) snprintf(
            ctemp,
            sizeof ctemp,
            "fCover_%s",
            cnames_VegTypes[j + 1] // j+1 since no total column
        );
        colnames_OUT[eSW_Biomass][j + i] = Str_Dup(ctemp, LogInfo);
        if (LogInfo->stopRun) {
            return; // Exit function prematurely due to error
        }
    }
    i += j;
    for (j = 0; j < NVEGTYPES + 2; j++) {
        (void) snprintf(ctemp, sizeof ctemp, "Biomass_%s", cnames_VegTypes[j]);
        colnames_OUT[eSW_Biomass][j + i] = Str_Dup(ctemp, LogInfo);
        if (LogInfo->stopRun) {
            return; // Exit function prematurely due to error
        }
    }
    i += j;
    for (j = 0; j < NVEGTYPES + 1; j++) {
        (void) snprintf(ctemp, sizeof ctemp, "Biolive_%s", cnames_VegTypes[j]);
        colnames_OUT[eSW_Biomass][j + i] = Str_Dup(ctemp, LogInfo);
        if (LogInfo->stopRun) {
            return; // Exit function prematurely due to error
        }
    }
    i += j;
    (void) snprintf(ctemp, sizeof ctemp, "%s", "LAI_total");
    colnames_OUT[eSW_Biomass][i] = Str_Dup(ctemp, LogInfo);

#ifdef SWDEBUG
    if (LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

    if (debug) {
        sw_printf(" completed.\n");
    }
#endif
}

/** Setup output information/description

@param[in] tLayers Number of soil layers
@param[in] n_evap_lyrs Number of soil layers with evaporation
@param[in] count Number of species to check
@param[in] parmsIn Struct for inputs of vegetation establishment for each
    species
@param[out] OutDom Struct of type SW_OUT_DOM that holds output
    information that do not change throughout simulation runs
@param[out] LogInfo Holds information on warnings and errors
*/
void SW_OUT_setup_output(
    unsigned int tLayers,
    unsigned int n_evap_lyrs,
    unsigned int count,
    SW_VEGESTAB_INFO_INPUTS *parmsIn,
    SW_OUT_DOM *OutDom,
    LOG_INFO *LogInfo
) {
    SW_OUT_set_ncol(
        tLayers,
        n_evap_lyrs,
        count,
        OutDom->ncol_OUT,
        OutDom->nvar_OUT,
        OutDom->nsl_OUT,
        OutDom->npft_OUT
    );

#if defined(SWNETCDF)
    SW_OUT_calc_iOUToffset(
        OutDom->nrow_OUT,
        OutDom->nvar_OUT,
        OutDom->nsl_OUT,
        OutDom->npft_OUT,
        OutDom->netCDFOutput.iOUToffset
    );
    (void) parmsIn;
    (void) LogInfo;

#else
    SW_OUT_set_colnames(
        tLayers, parmsIn, OutDom->ncol_OUT, OutDom->colnames_OUT, LogInfo
    );
#endif // !SWNETCDF
}

void SW_OUT_new_year(
    TimeInt firstdoy,
    TimeInt lastdoy,
    SW_OUT_DOM *OutDom,
    TimeInt first[],
    TimeInt last[]
) {
    /* =================================================== */
    /* reset the terminal output days each year  */

    int k;

    ForEachOutKey(k) {
        if (!OutDom->use[k]) {
            continue;
        }

        if (OutDom->first_orig[k] <= firstdoy) {
            first[k] = firstdoy;
        } else {
            first[k] = OutDom->first_orig[k];
        }

        if (OutDom->last_orig[k] >= lastdoy) {
            last[k] = lastdoy;
        } else {
            last[k] = OutDom->last_orig[k];
        }
    }
}

int SW_OUT_read_onekey(
    SW_OUT_DOM *OutDom,
    OutKey k,
    OutSum sumtype,
    int first,
    int last,
    char msg[],
    size_t sizeof_msg,
    Bool *VegProd_use_SWA,
    Bool deepdrain,
    char *txtInFiles[]
) {
    int res = 0; // return value indicating type of message if any

    char *MyFileName = txtInFiles[eOutput];
    msg[0] = '\0';

    // Convert strings to index numbers
    OutDom->sumtype[k] = sumtype;

    OutDom->use[k] = (Bool) (sumtype != eSW_Off);

    // Proceed to next line if output key/type is turned off
    if (!OutDom->use[k]) {
        return (-1); // return and read next line of `outsetup.in`
    }

    /* check validity of summary type */
    if (OutDom->sumtype[k] == eSW_Fnl && !OutDom->has_sl[k]) {
        OutDom->sumtype[k] = eSW_Avg;

        (void) snprintf(
            msg,
            sizeof_msg,
            "%s : Summary Type FIN with key %s is meaningless.\n"
            "  Using type AVG instead.",
            MyFileName,
            key2str[k]
        );
        res = LOGWARN;
    }

    // set use_SWA to TRUE if defined.
    // Used in SW_Control to run the functions to get the recalculated values
    // only if SWA is used This function is run prior to the control functions
    // so thats why it is here.
    if (k == eSW_SWA) {
        *VegProd_use_SWA = swTRUE;
    }

    /* Check validity of output key */
    if (k == eSW_Estab) {
        OutDom->sumtype[k] = eSW_Sum;
        first = 1;
        last = 366;

    } else if ((k == eSW_AllVeg || k == eSW_ET || k == eSW_AllWthr ||
                k == eSW_AllH2O)) {
        OutDom->use[k] = swFALSE;

        (void) snprintf(
            msg,
            sizeof_msg,
            "%s : Output key %s is currently unimplemented.",
            MyFileName,
            key2str[k]
        );
        return (LOGWARN);
    }

    /* verify deep drainage parameters */
    if (k == eSW_DeepSWC && OutDom->sumtype[k] != eSW_Off && !deepdrain) {
        OutDom->use[k] = swFALSE;

        (void) snprintf(
            msg,
            sizeof_msg,
            "%s : DEEPSWC cannot produce output if deep drainage is "
            "not simulated (flag not set in %s).",
            MyFileName,
            txtInFiles[eSite]
        );
        return (LOGWARN);
    }

    // Set remaining values of `OutDom->...[k]`
    OutDom->first_orig[k] = first;
    OutDom->last_orig[k] = last;

    if (OutDom->last_orig[k] == 0) {
        (void) snprintf(
            msg,
            sizeof_msg,
            "%s : Invalid ending day (%d), key=%s.",
            MyFileName,
            last,
            key2str[k]
        );
        return (LOGERROR);
    }

    return (res);
}

/**
Read output setup from file `outsetup.in`.

Output can be generated for four different time steps: daily (DY), weekly
(WK), monthly (MO), and yearly (YR).

We have two options to specify time steps:
    - The same time step(s) for every output: Add a line with the tag
      `TIMESTEP`, e.g., `TIMESTEP dy mo yr` will generate daily, monthly, and
      yearly output for every output variable. If there is a line with this
      tag, then this will override information provided in the column `PERIOD`.
    - A different time step for each output: Specify the time step in the
      column `PERIOD` for each output variable. Note: only one time step per
      output variable can be specified.

@param[in,out] sw Comprehensive structure holding all information
    dealt with in SOILWAT2
@param[in,out] OutDom Struct of type SW_OUT_DOM that holds output
    information that do not change throughout simulation runs
@param[in] txtInFiles Array of program in/output files
@param[in,out] outDir Directory output files will be written to;
return an updated name to this directory after reading output input file
@param[out] LogInfo Holds information on warnings and errors
 */
void SW_OUT_read(
    SW_RUN *sw,
    SW_OUT_DOM *OutDom,
    char *txtInFiles[],
    char outDir[],
    LOG_INFO *LogInfo
) {
    /* =================================================== */
    /* read input file for output parameter setup info.
     * 5-Nov-01 -- now disregard the specified file name's
     *             extension and instead use the specified
     *             period as the extension.
     * 10-May-02 - Added conditional for interfacing to STEPPE.
     *             We want no output when running from STEPPE
     *             so the code to open the file is blocked out.
     *             In fact, the only keys to process are
     *             TRANSP, PRECIP, and TEMP.
     */

    FILE *f;
    OutKey k = eSW_NoKey;
    int x;
    int itemno;
    int msg_type;
    IntUS i;
    Bool useTimeStep = swFALSE;
    IntUS *used_OUTNPERIODS = &OutDom->used_OUTNPERIODS;

    /* these dims come from the orig format str */
    /* except for the uppercase space. */
    // timeStep: matrix to capture all the periods entered in outsetup.in
    char timeStep[SW_OUTNPERIODS][10];
    char keyname[50];
    char ext[10];
    /* sumtype: should be 2 chars, but we don't  want overflow from user typos
     */
    char sumtype[4];
    char period[10
                /* last: last doy for output, if "end", ==366 */];
    char lastStr[4];
    char firstStr[4];
    char outfile[MAX_FILENAMESIZE];
    // message to print
    char msg[200];
    /* space for uppercase conversion */
    char upkey[50];
    char upsum[4];
    char inbuf[MAX_FILENAMESIZE];
    int first;
    int last = -1; /* first doy for output */
    const int numReadInKeys = 28;
    const int outDirLineNo = numReadInKeys + 2;
    char relOutFileName[MAX_FILENAMESIZE] = {'\0'};
    int resSNP;
    int outTxtIndex;

    char *MyFileName = txtInFiles[eOutput];
    f = OpenFile(MyFileName, "r", LogInfo);
    if (LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }
    itemno = 0;

    // if 'TIMESTEP' is not specified in input file, then only one time step =
    // period can be specified
    *used_OUTNPERIODS = 1;

    while (GetALine(f, inbuf, MAX_FILENAMESIZE)) {
        itemno++; /* note: extra lines will cause an error */
        if (itemno <= numReadInKeys + 1) {
            x = sscanf(
                inbuf,
                "%s %s %s %3s %3s %s",
                keyname,
                sumtype,
                period,
                firstStr,
                lastStr,
                outfile
            );

            // Check whether we have read in `TIMESTEP`, `OUTSEP`, or one of the
            // 'key' lines
            if (Str_CompareI(keyname, (char *) "TIMESTEP") == 0) {
                // condition to read in the TIMESTEP line in outsetup.in
                // need to rescan the line because you are looking for all
                // strings, unlike the original scan

                // maximum number of possible timeStep is SW_OUTNPERIODS
                *used_OUTNPERIODS = sscanf(
                    inbuf,
                    "%9s %9s %9s %9s %9s",
                    keyname,
                    timeStep[0],
                    timeStep[1],
                    timeStep[2],
                    timeStep[3]
                );

                // decrement the count to make sure to not count keyname in the
                // number of periods
                (*used_OUTNPERIODS)--;

                // make sure that `TIMESTEP` line did contain time periods;
                // otherwise, use values from the `period` column
                if (*used_OUTNPERIODS > 0) {
                    useTimeStep = swTRUE;

                    if (*used_OUTNPERIODS > SW_OUTNPERIODS) {
                        LogError(
                            LogInfo,
                            LOGERROR,
                            "SW_OUT_read: used_OUTNPERIODS = %d > "
                            "SW_OUTNPERIODS = %d which is illegal.\n",
                            *used_OUTNPERIODS,
                            SW_OUTNPERIODS
                        );
                        goto closeFile;
                    }
                }

                continue; // read next line of `outsetup.in`
            }

            if (Str_CompareI(keyname, (char *) "OUTSEP") == 0) {
                // Notify the user that this functionality has been removed
                LogError(
                    LogInfo,
                    LOGWARN,
                    "`outsetup.in`: The functionality to specify a separation "
                    "character in output files has been removed. Only CSV "
                    "files will be created. It is recommended to "
                    "remove the \'OUTSEP\' line from your file."
                );

                continue; // read next line of `outsetup.in`
            }

            // we have read a line that specifies an output key/type
            // make sure that we got enough input
            if (x < 6) {
                LogError(
                    LogInfo,
                    LOGERROR,
                    "%s : Insufficient input for key %s item %d.",
                    MyFileName,
                    keyname,
                    itemno
                );
                goto closeFile;
            }

            first = sw_strtoi(firstStr, MyFileName, LogInfo);
            if (LogInfo->stopRun) {
                goto closeFile;
            }

            if (Str_CompareI(lastStr, (char *) "END") != 0) {
                last = sw_strtoi(lastStr, MyFileName, LogInfo);
                if (LogInfo->stopRun) {
                    goto closeFile;
                }
            }

            // Convert strings to index numbers
            k = str2key(Str_ToUpper(keyname, upkey), LogInfo);
            if (LogInfo->stopRun) {
                goto closeFile;
            }

// For now: rSOILWAT2's function `onGet_SW_OUT` requires that
// `OutDom->outfile[k]` is allocated here
#if defined(RSOILWAT)
            OutDom->outfile[k] = Str_Dup(outfile, LogInfo);
            if (LogInfo->stopRun) {
                goto closeFile;
            }
#else
            outfile[0] = '\0';
#endif

            // Fill information into `sw->Output[k]`
            msg_type = SW_OUT_read_onekey(
                OutDom,
                k,
                str2stype(Str_ToUpper(sumtype, upsum), LogInfo),
                first,
                (Str_CompareI(lastStr, (char *) "END") == 0) ? 366 : last,
                msg,
                sizeof msg,
                &sw->VegProdIn.use_SWA,
                sw->SiteIn.deepdrain,
                txtInFiles
            );

            if (msg_type == LOGWARN || msg_type == LOGERROR) {
                LogError(LogInfo, msg_type, "%s", msg);

                if (msg_type == LOGERROR) {
                    goto closeFile;
                }
            }

            // Specify which output time periods are requested for this output
            // key/type
            if (OutDom->use[k]) {
                if (useTimeStep) {
                    // `timeStep` was read in earlier on the `TIMESTEP` line;
                    // ignore `period`
                    for (i = 0; i < *used_OUTNPERIODS; i++) {
                        OutDom->timeSteps[k][i] =
                            str2period(Str_ToUpper(timeStep[i], ext));
                    }

                } else {
                    OutDom->timeSteps[k][0] =
                        str2period(Str_ToUpper(period, ext));
                }
            }
        } else { // Read output file names
            if (itemno == outDirLineNo) {
                resSNP = snprintf(outDir, FILENAME_MAX, "%s", inbuf);
                if (resSNP < 0 || (unsigned) resSNP >= FILENAME_MAX) {
                    LogError(
                        LogInfo,
                        LOGERROR,
                        "output path name is too long: '%s'.",
                        inbuf
                    );
                }
            } else {
                if (!isnull(outDir)) {
                    resSNP = snprintf(
                        relOutFileName,
                        sizeof relOutFileName,
                        "%s%s",
                        outDir,
                        inbuf
                    );
                    if (resSNP < 0 ||
                        (unsigned) resSNP >= sizeof relOutFileName) {
                        LogError(
                            LogInfo,
                            LOGERROR,
                            "Output filename is too long: <output dir> + '%s'. "
                            "The output directory is automatically prepended "
                            "to the filename so the file is relative to it.",
                            inbuf
                        );
                    }
                    if (LogInfo->stopRun) {
                        return;
                    }

                    outTxtIndex = SW_NINFILES + itemno - outDirLineNo - 1;
                    txtInFiles[outTxtIndex] = Str_Dup(relOutFileName, LogInfo);
                }
            }
            if (LogInfo->stopRun) {
                return;
            }
        }
    } // end of while-loop

    // Determine which output periods are turned on for at least one output key
    find_OutPeriods_inUse(OutDom);

#if defined(SW_OUTTEXT)
    // Determine for which output periods text output per soil layer or
    // 'regular' is requested:
    find_TXToutputSoilReg_inUse(
        sw->SW_PathOutputs.make_soil,
        sw->SW_PathOutputs.make_regular,
        OutDom->has_sl,
        OutDom->timeSteps,
        OutDom->used_OUTNPERIODS
    );
#endif

#if defined(STEPWAT) || defined(SWNETCDF)
    // Determine number of used years/months/weeks/days in simulation period
    SW_OUT_set_nrow(&sw->ModelIn, OutDom->use_OutPeriod, OutDom->nrow_OUT);
#endif

    if (DirExists(outDir)) {
        SW_F_CleanOutDir(outDir, LogInfo);
    } else {
        MkDir(outDir, LogInfo);
    }
    if (LogInfo->stopRun) {
        goto closeFile;
    }

closeFile: { CloseFile(&f, LogInfo); }
}

void collect_values(
    SW_RUN *sw,
    SW_OUT_DOM *OutDom,
    Bool bFlush_output,
    TimeInt tOffset,
    LOG_INFO *LogInfo
) {
    SW_OUT_sum_today(sw, OutDom, eSWC, bFlush_output, tOffset, LogInfo);
    if (LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

    SW_OUT_sum_today(sw, OutDom, eWTH, bFlush_output, tOffset, LogInfo);
    if (LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

    SW_OUT_sum_today(sw, OutDom, eVES, bFlush_output, tOffset, LogInfo);
    if (LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

    SW_OUT_sum_today(sw, OutDom, eVPD, bFlush_output, tOffset, LogInfo);
    if (LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

    SW_OUT_write_today(sw, OutDom, bFlush_output, tOffset, LogInfo);
}

/** called at year end to process the remainder of the output period.

This sets two flags: bFlush_output and tOffset to be used in the appropriate
subs.

@param[in,out] sw Comprehensive struct of type SW_RUN containing
    all information in the simulation
@param[in] OutDom Struct of type SW_OUT_DOM that holds output
    information that do not change throughout simulation runs
@param[out] LogInfo Holds information on warnings and errors
*/
void SW_OUT_flush(SW_RUN *sw, SW_OUT_DOM *OutDom, LOG_INFO *LogInfo) {
    TimeInt localTOffset = 0; // tOffset is zero when called from this function

    collect_values(sw, OutDom, swTRUE, localTOffset, LogInfo);
}

/** adds today's output values to week, month and year
accumulators and puts today's values in yesterday's
registers.

This is different from the Weather.c approach
which updates Yesterday's registers during the _new_day()
function. It's more logical to update yesterday just
prior to today's calculations, but there's no logical
need to perform _new_day() on the soilwater.

@param[in,out] sw Comprehensive struct of type SW_RUN containing
    all information in the simulation
@param[in,out] OutDom Struct of type SW_OUT_DOM that holds output
    information that do not change throughout simulation runs
@param[in] otyp Identifies the current module/object
@param[in] bFlush_output Determines if output should be created for
    a specific output key
@param[in] tOffset Offset describing with the previous or current period
@param[out] LogInfo Holds information on warnings and errors
*/
void SW_OUT_sum_today(
    SW_RUN *sw,
    SW_OUT_DOM *OutDom,
    ObjType otyp,
    Bool bFlush_output,
    TimeInt tOffset,
    LOG_INFO *LogInfo
) {
    /*  SW_VEGESTAB *v = &SW_VegEstab;  -> we don't need to sum daily for this
     */
    OutPeriod pd;

    ForEachOutPeriod(pd) {
        // `newperiod[eSW_Day]` is always TRUE
        if (bFlush_output || sw->ModelSim.newperiod[pd]) {
            if (pd > eSW_Day) {
                average_for(
                    sw, OutDom, otyp, pd, bFlush_output, tOffset, LogInfo
                );
                if (LogInfo->stopRun) {
                    return; // Exit function prematurely due to error
                }
            }

            switch (otyp) {
            case eSWC:
                memset(&sw->sw_p_accu[pd], 0, sizeof(SW_SOILWAT_OUTPUTS));
                break;
            case eWTH:
                memset(&sw->weath_p_accu[pd], 0, sizeof(SW_WEATHER_OUTPUTS));
                break;
            case eVES:
                break;
            case eVPD:
                memset(&sw->vp_p_accu[pd], 0, sizeof(SW_VEGPROD_OUTPUTS));
                break;
            default:
                LogError(
                    LogInfo,
                    LOGERROR,
                    "Invalid object type in SW_OUT_sum_today()."
                );
                return; // Exit function prematurely due to error
            }
        }
    }

    if (!bFlush_output) {
        ForEachOutPeriod(pd) {
            collect_sums(sw, OutDom, otyp, pd, LogInfo);

            if (LogInfo->stopRun) {
                return; // Exit function prematurely due to error
            }
        }
    }
}

/** `SW_OUT_write_today` is called twice

    - `end_day` at the end of each day with values
      values of `bFlush_output` set to FALSE and `tOffset` set to 1
    - `SW_OUT_flush` at the end of every year with
      values of `bFlush_output` set to TRUE and `tOffset` set to 0

@param[in] sw Comprehensive struct of type SW_RUN containing all
    information in the simulation
@param[in] OutDom Struct of type SW_OUT_DOM that holds output
    information that do not change throughout simulation runs
@param[in] bFlush_output Determines if output should be created for
    a specific output key
@param[in] tOffset Offset describing with the previous or current period
@param[out] LogInfo Holds information on warnings and errors
*/
void SW_OUT_write_today(
    SW_RUN *sw,
    SW_OUT_DOM *OutDom,
    Bool bFlush_output,
    TimeInt tOffset,
    LOG_INFO *LogInfo
) {
    /* --------------------------------------------------- */
    /* all output values must have been summed, averaged or
     * otherwise completed before this is called [now done
     * by SW_*_sum_*<daily|yearly>()] prior.
     * This subroutine organizes only the calling loop and
     * sending the string to output.
     * Each output quantity must have a print function
     * defined and linked to SW_Output.pfunc (currently all
     * starting with 'get_').  Those funcs return a properly
     * formatted string to be output via the module variable
     * 'sw_outstr'. Furthermore, those funcs must know their
     * own time period.  This version of the program only
     * prints one period for each quantity.
     *
     * The t value tests whether the current model time is
     * outside the output time range requested by the user.
     * Recall that times are based at 0 rather than 1 for
     * array indexing purposes but the user request is in
     * natural numbers, so we add one before testing.
     */
    /* 10-May-02 (cwb) Added conditional to interface with STEPPE.
     *           We want no output if running from STEPPE.
     * July 12, 2017: Added functionality for writing outputs for STEPPE and
     * SOILWAT since we now want output for STEPPE
     */
#ifdef SWDEBUG
    int debug = 0;
#endif

    TimeInt t = 0xffff;
    OutPeriod p;
    Bool writeit[SW_OUTNPERIODS];
    Bool use_help;

    // Temporary string to hold sw_outstr before concatenating
    // to buf_soil/buf_reg
    // Silences -Wrestrict when compiling on Linux (found within -Wall)
    char tempstr[MAX_LAYERS * OUTSTRLEN];
    int k;
    int i;
    int outPeriod;

#ifdef SW_OUTTEXT
    Bool fullBuffer = swFALSE;

    char *soilWritePtr[SW_OUTNPERIODS] = {
        sw->SW_PathOutputs.buf_soil[0],
        sw->SW_PathOutputs.buf_soil[1],
        sw->SW_PathOutputs.buf_soil[2],
        sw->SW_PathOutputs.buf_soil[3]
    };
    char *regWritePtr[SW_OUTNPERIODS] = {
        sw->SW_PathOutputs.buf_reg[0],
        sw->SW_PathOutputs.buf_reg[1],
        sw->SW_PathOutputs.buf_reg[2],
        sw->SW_PathOutputs.buf_reg[3]
    };
#endif

#ifdef STEPWAT
    Bool use_help_txt;
    Bool use_help_SXW;

    char *soilAggWritePtr[SW_OUTNPERIODS] = {
        sw->SW_PathOutputs.buf_soil_agg[0],
        sw->SW_PathOutputs.buf_soil_agg[1],
        sw->SW_PathOutputs.buf_soil_agg[2],
        sw->SW_PathOutputs.buf_soil_agg[3]
    };
    char *regAggWritePtr[SW_OUTNPERIODS] = {
        sw->SW_PathOutputs.buf_reg_agg[0],
        sw->SW_PathOutputs.buf_reg_agg[1],
        sw->SW_PathOutputs.buf_reg_agg[2],
        sw->SW_PathOutputs.buf_reg_agg[3]
    };
#endif


    /* Update `tOffset` within SW_OUT_RUN for output functions */
    sw->OutRun.tOffset = tOffset;

#if defined(SW_OUTTEXT)
    char str_time[10]; // year and day/week/month header for each output row

    // We don't really need all of these buffers to init every day
    ForEachOutPeriod(p) {
        sw->SW_PathOutputs.buf_reg[p][0] = '\0';
        sw->SW_PathOutputs.buf_soil[p][0] = '\0';

#ifdef STEPWAT
        sw->SW_PathOutputs.buf_reg_agg[p][0] = '\0';
        sw->SW_PathOutputs.buf_soil_agg[p][0] = '\0';
#endif
    }
#else
    (void) LogInfo;
#endif

#ifdef SWDEBUG
    if (debug) {
        sw_printf(
            "'SW_OUT_write_today': %dyr-%dmon-%dwk-%ddoy: ",
            sw->ModelSim.year,
            sw->ModelSim.month,
            sw->ModelSim.week,
            sw->ModelSim.doy
        );
    }
#endif


    // Determine which output periods should get formatted and output (if they
    // are active)
    t = sw->ModelSim.doy;

    // `csv`-files assume anyhow that first/last are identical for every output
    // type/key
    writeit[eSW_Day] =
        (Bool) (t < sw->OutRun.first[0] || t > sw->OutRun.last[0]);
    writeit[eSW_Week] =
        (Bool) (writeit[eSW_Day] &&
                (sw->ModelSim.newperiod[eSW_Week] || bFlush_output));
    writeit[eSW_Month] =
        (Bool) (writeit[eSW_Day] &&
                (sw->ModelSim.newperiod[eSW_Month] || bFlush_output));
    writeit[eSW_Year] =
        (Bool) (sw->ModelSim.newperiod[eSW_Year] || bFlush_output);

    // update daily: don't process daily output if `bFlush_output` is TRUE
    // because `end_day` was already called and produced daily output
    writeit[eSW_Day] = (Bool) (writeit[eSW_Day] && !bFlush_output);


    // Loop over output types/keys, over used output time periods, call
    // formatting functions `get_XXX`, and concatenate for one row of
    // `csv`-output
    ForEachOutKey(k) {
#ifdef SW_OUTTEXT
        size_t writeSizeReg[SW_OUTNPERIODS] = {
            (size_t) (MAX_LAYERS * OUTSTRLEN),
            (size_t) (MAX_LAYERS * OUTSTRLEN),
            (size_t) (MAX_LAYERS * OUTSTRLEN),
            (size_t) (MAX_LAYERS * OUTSTRLEN)
        };

        size_t writeSizeSoil[SW_OUTNPERIODS] = {
            (size_t) (MAX_LAYERS * OUTSTRLEN),
            (size_t) (MAX_LAYERS * OUTSTRLEN),
            (size_t) (MAX_LAYERS * OUTSTRLEN),
            (size_t) (MAX_LAYERS * OUTSTRLEN)
        };
#endif

#ifdef STEPWAT
        size_t writeSizeSoilAgg[SW_OUTNPERIODS] = {
            (size_t) (MAX_LAYERS * OUTSTRLEN),
            (size_t) (MAX_LAYERS * OUTSTRLEN),
            (size_t) (MAX_LAYERS * OUTSTRLEN),
            (size_t) (MAX_LAYERS * OUTSTRLEN)
        };
        size_t writeSizeRegAgg[SW_OUTNPERIODS] = {
            (size_t) (MAX_LAYERS * OUTSTRLEN),
            (size_t) (MAX_LAYERS * OUTSTRLEN),
            (size_t) (MAX_LAYERS * OUTSTRLEN),
            (size_t) (MAX_LAYERS * OUTSTRLEN)
        };
#endif

#ifdef SWDEBUG
        if (debug) {
            sw_printf("key=%d=%s: ", k, key2str[k]);
        }
#endif

        if (!OutDom->use[k]) {
            continue;
        }

        for (i = 0; i < OutDom->used_OUTNPERIODS; i++) {

            outPeriod = OutDom->timeSteps[k][i];
            use_help = (Bool) (outPeriod != eSW_NoTime && writeit[outPeriod]);

#ifdef STEPWAT
            use_help_txt = use_help;
            use_help_SXW = (Bool) (OutDom->timeSteps_SXW[k][i] != eSW_NoTime &&
                                   writeit[OutDom->timeSteps_SXW[k][i]]);
            use_help = (Bool) (use_help_txt || use_help_SXW);
#endif

            if (!use_help) {
                continue; // don't call any `get_XXX` function
            }

#if defined(SOILWAT) && !defined(SWNETCDF)
#ifdef SWDEBUG
            if (debug) {
                sw_printf(
                    " call pfunc_text(%d=%s))", outPeriod, pd2str[outPeriod]
                );
            }
#endif
            OutDom->pfunc_text[k](outPeriod, sw, LogInfo);

#elif defined(RSOILWAT) || defined(SWNETCDF)
#ifdef SWDEBUG
            if (debug) {
                sw_printf(
                    " call pfunc_mem(%d=%s))", outPeriod, pd2str[outPeriod]
                );
            }
#endif
            OutDom->pfunc_mem[k](outPeriod, sw, OutDom);

#elif defined(STEPWAT)
            if (use_help_SXW) {
#ifdef SWDEBUG
                if (debug) {
                    sw_printf(
                        " call pfunc_SXW(%d=%s))",
                        OutDom->timeSteps_SXW[k][i],
                        pd2str[OutDom->timeSteps_SXW[k][i]]
                    );
                }
#endif
                OutDom->pfunc_SXW[k](
                    OutDom->timeSteps_SXW[k][i], sw, OutDom, LogInfo
                );
                if (LogInfo->stopRun) {
                    return;
                }
            }

            if (!use_help_txt) {
                continue; // SXW output complete; skip to next output period
            }

            if (OutDom->prepare_IterationSummary) {
#ifdef SWDEBUG
                if (debug) {
                    sw_printf(
                        " call pfunc_agg(%d=%s))", outPeriod, pd2str[outPeriod]
                    );
                }
#endif
                OutDom->pfunc_agg[k](outPeriod, sw, OutDom, LogInfo);
                if (LogInfo->stopRun) {
                    return;
                }
            }

            if (OutDom->print_SW_Output) {
                outPeriod = OutDom->timeSteps[k][i];
#ifdef SWDEBUG
                if (debug) {
                    sw_printf(
                        " call pfunc_text(%d=%s))", outPeriod, pd2str[outPeriod]
                    );
                }
#endif
                OutDom->pfunc_text[k](outPeriod, sw, LogInfo);
            }
#endif
            if (LogInfo->stopRun) {
                return;
            }

#ifdef SWDEBUG
            if (debug) {
                sw_printf(" ... ok");
            }
#endif

#if defined(SW_OUTTEXT)
            /* concatenate formatted output for one row of `csv`- files */
            if (OutDom->print_SW_Output) {
                (void) sw_memccpy(
                    tempstr,
                    sw->OutRun.sw_outstr,
                    '\0',
                    (size_t) (MAX_LAYERS * OUTSTRLEN)
                );

                if (OutDom->has_sl[k]) {
                    fullBuffer = sw_memccpy_inc(
                        (void **) &soilWritePtr[outPeriod],
                        (soilWritePtr[outPeriod] +
                         sizeof soilWritePtr[outPeriod] - 1),
                        (void *) tempstr,
                        '\0',
                        &writeSizeSoil[outPeriod]
                    );
                } else {
                    fullBuffer = sw_memccpy_inc(
                        (void **) &regWritePtr[outPeriod],
                        (regWritePtr[outPeriod] +
                         sizeof regWritePtr[outPeriod] - 1),
                        (void *) tempstr,
                        '\0',
                        &writeSizeReg[outPeriod]
                    );
                }
                if (fullBuffer) {
                    reportFullBuffer(LOGERROR, LogInfo);
                    return;
                }
            }


#ifdef STEPWAT
            if (OutDom->print_IterationSummary) {
                (void) sw_memccpy(
                    tempstr,
                    sw->OutRun.sw_outstr_agg,
                    '\0',
                    (size_t) (MAX_LAYERS * OUTSTRLEN)
                );

                if (OutDom->has_sl[k]) {
                    fullBuffer = sw_memccpy_inc(
                        (void **) &soilAggWritePtr[outPeriod],
                        (soilAggWritePtr[outPeriod] +
                         sizeof soilAggWritePtr[outPeriod] - 1),
                        (void *) tempstr,
                        '\0',
                        &writeSizeSoilAgg[outPeriod]
                    );
                } else {
                    fullBuffer = sw_memccpy_inc(
                        (void **) &regAggWritePtr[outPeriod],
                        (regAggWritePtr[outPeriod] +
                         sizeof regAggWritePtr[outPeriod] - 1),
                        (void *) tempstr,
                        '\0',
                        &writeSizeRegAgg[outPeriod]
                    );
                }
                if (fullBuffer) {
                    reportFullBuffer(LOGERROR, LogInfo);
                    return;
                }
            }
#endif
#else
            (void) tempstr;
#endif
        } // end of loop across `used_OUTNPERIODS`
    } // end of loop across output keys

#if defined(SW_OUTTEXT)
    int fprintRes = 0;

    // write formatted output to csv-files
    ForEachOutPeriod(p) {
        if (OutDom->use_OutPeriod[p] && writeit[p]) {
            get_outstrleader(
                p, sizeof str_time, &sw->ModelSim, tOffset, str_time
            );

            if (sw->SW_PathOutputs.make_regular[p]) {
                if (OutDom->print_SW_Output) {
                    fprintRes = fprintf(
                        sw->SW_PathOutputs.fp_reg[p],
                        "%s%s\n",
                        str_time,
                        sw->SW_PathOutputs.buf_reg[p]
                    );

                    if (fprintRes < 0) {
                        LogError(
                            LogInfo,
                            LOGERROR,
                            "Could not write values to \"regular\" CSVs."
                        );
                        return; /* Exit prematurely due to error */
                    }

                    // STEPWAT2 needs a fflush for yearly output;
                    // other time steps, the soil-layer files, and SOILWAT2 work
                    // fine without it...
                    if (fflush(sw->SW_PathOutputs.fp_reg[p]) == EOF) {
                        LogError(
                            LogInfo,
                            LOGERROR,
                            "Could not write flush values to \"regular\" CSVs."
                        );
                        return; /* Exit prematurely due to error */
                    }
                }

#ifdef STEPWAT
                if (OutDom->print_IterationSummary) {
                    fprintRes = fprintf(
                        sw->SW_PathOutputs.fp_reg_agg[p],
                        "%s%s\n",
                        str_time,
                        sw->SW_PathOutputs.buf_reg_agg[p]
                    );

                    if (fprintRes < 0) {
                        LogError(
                            LogInfo,
                            LOGERROR,
                            "Could not write values to \"regular\" aggregation "
                            "iteration files."
                        );
                        return; /* Exit prematurely due to error */
                    }
                }
#endif
            }

            if (sw->SW_PathOutputs.make_soil[p]) {
                if (OutDom->print_SW_Output) {
                    fprintRes = fprintf(
                        sw->SW_PathOutputs.fp_soil[p],
                        "%s%s\n",
                        str_time,
                        sw->SW_PathOutputs.buf_soil[p]
                    );

                    if (fprintRes < 0) {
                        LogError(
                            LogInfo,
                            LOGERROR,
                            "Could not write values to \"soil\" output files."
                        );
                        return; /* Exit prematurely due to error */
                    }
                }

#ifdef STEPWAT
                if (OutDom->print_IterationSummary) {
                    fprintRes = fprintf(
                        sw->SW_PathOutputs.fp_soil_agg[p],
                        "%s%s\n",
                        str_time,
                        sw->SW_PathOutputs.buf_soil_agg[p]
                    );

                    if (fprintRes < 0) {
                        LogError(
                            LogInfo,
                            LOGERROR,
                            "Could not write values to \"soil\" aggregation "
                            "iteration files."
                        );
                        return; /* Exit prematurely due to error */
                    }
                }
#endif
            }
        }
    }
#endif

#if defined(SW_OUTARRAY)
    // increment row counts
    ForEachOutPeriod(p) {
        if (OutDom->use_OutPeriod[p] && writeit[p]) {
            sw->OutRun.irow_OUT[p]++;
        }
    }
#endif

#ifdef SWDEBUG
    if (debug) {
        sw_printf("'SW_OUT_write_today': completed\n");
    }
#endif
}

/**
@brief create all of the user-specified output files.

@param[in,out] SW_PathOutputs Struct of type SW_PATH_OUTPUTS which
holds basic information about output files and values
@param[in] SW_Domain Struct of type SW_DOMAIN holding constant
    temporal/spatial information for a set of simulation runs
@param[out] LogInfo Holds information on warnings and errors

@note Call this routine at the beginning of the main program run, but
after SW_OUT_read() which sets the global variable use_OutPeriod.
*/
void SW_OUT_create_files(
    SW_PATH_OUTPUTS *SW_PathOutputs, SW_DOMAIN *SW_Domain, LOG_INFO *LogInfo
) {

#if defined(SOILWAT)
    if (LogInfo->printProgressMsg) {
        sw_message("is creating output files ...");
    }
#endif

#if defined(SOILWAT) && defined(SW_OUTTEXT)
    SW_OUT_create_textfiles(
        &SW_Domain->OutDom,
        SW_PathOutputs,
        SW_Domain->nMaxSoilLayers,
        SW_Domain->SW_PathInputs.txtInFiles,
        LogInfo
    );

#elif defined(SWNETCDF)
    SW_NCOUT_create_output_files(
        SW_Domain->SW_PathInputs.ncInFiles[eSW_InDomain][vNCdom],
        SW_Domain->DomainType,
        SW_Domain->SW_PathInputs.outputPrefix,
        SW_Domain,
        SW_Domain->OutDom.timeSteps,
        SW_Domain->OutDom.used_OUTNPERIODS,
        SW_Domain->OutDom.nvar_OUT,
        SW_Domain->OutDom.nsl_OUT,
        SW_Domain->OutDom.npft_OUT,
        SW_Domain->hasConsistentSoilLayerDepths,
        SW_Domain->depthsAllSoilLayers,
        SW_Domain->OutDom.netCDFOutput.strideOutYears,
        SW_Domain->startyr,
        SW_Domain->endyr,
        SW_Domain->OutDom.netCDFOutput.baseCalendarYear,
        &SW_PathOutputs->numOutFiles,
        SW_PathOutputs->ncOutFiles,
        LogInfo
    );

#else
    (void) SW_PathOutputs;
    (void) SW_Domain;
    (void) LogInfo;
#endif
}

/**
@brief close all of the user-specified output files.

call this routine at the end of the program run.

@param[in,out] SW_PathOutputs Struct of type SW_PATH_OUTPUTS which holds basic
    information about output files and values
@param[in] OutDom Struct of type SW_OUT_DOM that holds output
    information that do not change throughout simulation runs
@param[out] LogInfo Holds information on warnings and errors
*/
void SW_OUT_close_files(
    SW_PATH_OUTPUTS *SW_PathOutputs, SW_OUT_DOM *OutDom, LOG_INFO *LogInfo
) {

#if defined(SW_OUTTEXT)
    SW_OUT_close_textfiles(SW_PathOutputs, OutDom, LogInfo);
#else
    (void) SW_PathOutputs;
    (void) OutDom;
    (void) LogInfo;
#endif
}

void echo_outputs(SW_OUT_DOM *OutDom, LOG_INFO *LogInfo) {
    int k;
    int index;
    int writeValIndex = 0;
    char str[OUTSTRLEN];
    char errstr[MAX_ERROR];
    size_t writeSize = MAX_ERROR;
    char *writePtr = errstr;
    char *endErrstr = errstr + sizeof errstr - 1;
    char *cpyPtr = NULL;
    const int numWriteStrs = 6;
    Bool fullBuffer = swFALSE;

    const char *errStrHeader = "---------------------------\nKey ";

    const char *errStrFooter =
        "\n----------  End of Output Configuration ---------- \n";

    const char *errStrConf =
        "\n===============================================\n"
        "  Output Configuration:\n";

    char *writeStrs[] = {
        (char *) errStrHeader,
        (char *) key2str[0], /* Overwrite in loop below */
        (char *) "\n\tSummary Type: ",
        (char *) styp2str[0], /* Overwrite in loop below */
        (char *) "\n\tStart period: %d",
        (char *) "\n\tEnd period  : %d\n"
    };

    TimeInt writeVals[] = {
        OutDom->first_orig[0], /* Overwrite in loop below */
        OutDom->last_orig[0]   /* Overwrite in loop below */
    };

    fullBuffer = sw_memccpy_inc(
        (void **) &writePtr, endErrstr, (void *) errStrConf, '\0', &writeSize
    );
    if (fullBuffer) {
        goto printOutput;
    }

    ForEachOutKey(k) {
        if (OutDom->use[k]) {
            writeStrs[1] = (char *) key2str[k];
            writeStrs[3] = (char *) styp2str[OutDom->sumtype[k]];

            writeVals[0] = OutDom->first_orig[k];
            writeVals[0] = OutDom->last_orig[k];
        } else {
            continue;
        }

        for (index = 0; index < numWriteStrs; index++) {
            cpyPtr = writeStrs[index];

            if (index > 4) {
                (void
                ) snprintf(str, OUTSTRLEN, cpyPtr, writeVals[writeValIndex]);

                writeValIndex++;
                cpyPtr = str;
            }

            fullBuffer = sw_memccpy_inc(
                (void **) &writePtr,
                endErrstr,
                (void *) cpyPtr,
                '\0',
                &writeSize
            );
            if (fullBuffer) {
                goto printOutput;
            }
        }
    }

    fullBuffer = sw_memccpy_inc(
        (void **) &writePtr, endErrstr, (void *) errStrFooter, '\0', &writeSize
    );

printOutput:
    if (fullBuffer) {
        reportFullBuffer(LOGWARN, LogInfo);
    }

    printf("%s\n", errstr);
}

void echo_all_inputs(SW_RUN *sw, SW_OUT_DOM *OutDom, LOG_INFO *LogInfo) {

    if (!sw->VegEstabSim.use) {
        printf("Establishment not used.\n");
    }

    echo_inputs(
        &sw->SiteIn,
        &sw->SiteSim,
        &sw->RunIn.ModelRunIn,
        &sw->RunIn.SoilRunIn,
        sw->RunIn.SiteRunIn.Tsoil_constant
    );
    echo_VegEstab(
        sw->RunIn.SoilRunIn.width,
        sw->VegEstabIn.parms,
        sw->VegEstabSim.count,
        LogInfo
    );
    echo_VegProd(&sw->RunIn.VegProdRunIn);
    echo_outputs(OutDom, LogInfo);
}

#if defined(SWNETCDF)
/**
@brief Deep copy instances with information in regards to
netCDF output files stored in SW_PATH_OUTPUTS

@param[out] dest_files Destination instance of netCDF files (SW_PATH_OUTPUTS)
@param[in] source_files Source instance of netCDF files (SW_PATH_OUTPUTS)
@param[in] OutDom Struct of type SW_OUT_DOM that holds output
    information that do not change throughout simulation runs
@param[out] LogInfo Holds information on warnings and errors
*/
void SW_PATHOUT_deepCopy(
    SW_PATH_OUTPUTS *dest_files,
    SW_PATH_OUTPUTS *source_files,
    SW_OUT_DOM *OutDom,
    LOG_INFO *LogInfo
) {

    int key;
    OutPeriod pd;
    unsigned int fileNum;
    unsigned int numFiles = source_files->numOutFiles;
    char **destFile = NULL;
    char *srcFile = NULL;

    ForEachOutKey(key) {
        if (OutDom->nvar_OUT[key] > 0 && OutDom->use[key]) {
            ForEachOutPeriod(pd) {
                if (OutDom->use_OutPeriod[pd]) {
                    SW_NCOUT_alloc_files(
                        &dest_files->ncOutFiles[key][pd], numFiles, LogInfo
                    );
                    if (LogInfo->stopRun) {
                        return; // Exit function prematurely due to error
                    }
                    for (fileNum = 0; fileNum < numFiles; fileNum++) {
                        if (!isnull(source_files->ncOutFiles[key][pd])) {
                            srcFile =
                                source_files->ncOutFiles[key][pd][fileNum];

                            destFile =
                                &dest_files->ncOutFiles[key][pd][fileNum];
                            *destFile = Str_Dup(srcFile, LogInfo);

                            if (LogInfo->stopRun) {
                                return; // Exit function prematurley due to
                                        // error
                            }
                        }
                    }
                }
            }
        }
    }
}
#endif

/**
 * @brief Deep copy the struct SW_OUT_DOM
 *
 * @param[in] source Source instance of SW_OUT_DOM
 * @param[out] dest Destination instance of SW_OUT_DOM
 * @param[out] LogInfo Holds information on warnings and errors
 */
void SW_OUTDOM_deepCopy(
    SW_OUT_DOM *source, SW_OUT_DOM *dest, LOG_INFO *LogInfo
) {

    int k;
    int i;

    /* Copies output pointers as well */
    memcpy(dest, source, sizeof(*dest));

    ForEachOutKey(k) {
        for (i = 0; i < 5 * NVEGTYPES + MAX_LAYERS; i++) {
            if (!isnull(source->colnames_OUT[k][i])) {

                dest->colnames_OUT[k][i] =
                    Str_Dup(source->colnames_OUT[k][i], LogInfo);
                if (LogInfo->stopRun) {
                    return; // Exit function prematurely due to error
                }
            }
        }

#if defined(SWNETCDF)
        int varNum;
        int attNum;

        SW_NETCDF_OUT *netCDFOut_src = &source->netCDFOutput;
        SW_NETCDF_OUT *netCDFOut_dest = &dest->netCDFOutput;

        if (source->nvar_OUT[k] > 0 && source->use[k]) {

            SW_NCOUT_alloc_outputkey_var_info(dest, k, LogInfo);
            if (LogInfo->stopRun) {
                return; // Exit function prematurely due to error
            }

            if (!isnull(netCDFOut_src->reqOutputVars[k])) {
                for (varNum = 0; varNum < source->nvar_OUT[k]; varNum++) {
                    netCDFOut_dest->reqOutputVars[k][varNum] =
                        netCDFOut_src->reqOutputVars[k][varNum];

                    if (netCDFOut_dest->reqOutputVars[k][varNum]) {
                        for (attNum = 0; attNum < MAX_NATTS; attNum++) {
                            if (!isnull(netCDFOut_src->outputVarInfo[k][varNum]
                                                                    [attNum])) {
                                netCDFOut_dest
                                    ->outputVarInfo[k][varNum][attNum] =
                                    Str_Dup(
                                        netCDFOut_src
                                            ->outputVarInfo[k][varNum][attNum],
                                        LogInfo
                                    );
                                if (LogInfo->stopRun) {
                                    return; // Exit function prematurely due to
                                            // error
                                }
                            }
                        }

                        netCDFOut_dest->units_sw[k][varNum] = Str_Dup(
                            netCDFOut_src->units_sw[k][varNum], LogInfo
                        );
                        if (LogInfo->stopRun) {
                            return; // Exit function prematurely due
                                    // to error
                        }
                    }
                }
            }

        } else {
            netCDFOut_dest->reqOutputVars[k] = NULL;
            netCDFOut_dest->outputVarInfo[k] = NULL;
            netCDFOut_dest->units_sw[k] = NULL;
            netCDFOut_dest->uconv[k] = NULL;
        }
#endif
    }
}

/*==================================================================*/
/**
  @defgroup out_algo Description of the output algorithm


  __In summary:__

  The function SW_CTL_run_current_year() in file SW_Control.c calls:
    - the function end_day() in file SW_Control.c, for each day, which in turn
      calls collect_values() with (global) arguments `bFlush_output` = `FALSE`
      and `tOffset` = 1
    - the function SW_OUT_flush(), after the last day of each year, which in
      turn calls collect_values() with (global) arguments
      `bFlush_output` = TRUE and `tOffset` = 0

  The function collect_values()
    -# calls SW_OUT_sum_today() for each of the \ref ObjType `otype`
      that produce output, i.e., `eSWC`, `eWTH`, `eVES`, and `eVPD`.
      SW_OUT_sum_today() loops over each \ref OutPeriod `pd`
      - if today is the start of a new day/week/month/year period or if
        `bFlush_output`, then it
        -# calls average_for() with arguments `otype` and `pd` which
          -# loops over all output keys `k`
          -# divides the summed values by the duration of the specific output
             period
          -# fills the output aggregator `p_oagg[pd]` variables
        -# resets the memory of the output accumulator `p_accu[d]` variables
      - and, unless `bFlush_output` is `FALSE`, in a second loop over each
        \ref OutPeriod `pd` calls collect_sums() with arguments `otype` and `pd`
        which calls the output summing function corresponding to its
        \ref ObjType argument `otype`, i.e., one of the functions sumof_swc(),
        sumof_wth(), sumof_ves(), or sumof_vpd, in order to sum up the daily
        values in the corresponding output accumulator `p_accu[pd]` variables.

    -# calls SW_OUT_write_today() which loops over each \ref OutKey `k` and
      loops over each \ref OutPeriod `pd` and, depending on application (see
      details below):
      - calls the appropriate output formatter function `get_XXX` via its
        pointer stored in `OutDom.pfunc_XXX`
      - writes output to file(s) and/or passes output in-memory

  There are four types of outputs and thus four types of output formatter
  functions `get_XXX` in file \ref SW_Output_get_functions.c
    - output to text files of current simulation:
      - output formatter function such as `get_XXX_text` which prepares a
        formatted text string in the variable sw_outstr found in SW_OUT_RUN
  which is concatenated and written to the text files by SW_OUT_write_today()
      - these output formatter functions are assigned to pointers
        `OutDom.pfunc_text[k]` and called by SW_OUT_write_today()
      - currently used by `SOILWAT2-standalone` and by `STEPWAT2` if executed
        with its `-i flag`

    - output to text files of values that are aggregated across several
      simulations (mean and SD of values)
      - output formatter function such as `get_XXX_agg` which
        - calculate a cumulative running mean and SD for the output values in
          the pointer array variables `SW_OUT_RUN.p_OUT` and
  `SW_OUT_RUN.p_OUTsd`
        - if `print_IterationSummary` is `TRUE` (i.e., for the last simulation
          run = last iteration in `STEPWAT2` terminology),
          prepare a formatted text string in the global variable
          \ref sw_outstr_agg which is concatenated and written to the text
          files by SW_OUT_write_today()
      - these output formatter functions are assigned to pointers
        `OutDom.pfunc_agg[k]` and called by SW_OUT_write_today()
      - currently used by `STEPWAT2` if executed with its `-o flag`

    - in-memory output via `STEPWAT2` variable `SXW`
      - the variable `SXW` is defined by `STEPWAT2` in its struct `stepwat_st`
      - the function SW_OUT_set_SXWrequests() instructs the output code to
        pass these outputs independently of any text output requested by an user
      - output formatter function such as `get_XXX_SWX` which pass the correct
        values directly in the appropriate slots of `SXW` for the correct time
        step
      - these output formatter functions are assigned to pointers
        `OutDom.pfunc_SXW[k]` and called by SW_OUT_write_today()
      - currently used by `STEPWAT2` if executed with its `-s flag`, i.e.,
        whenever `STEPWAT2` is run with `SOILWAT2`

    - in-memory output via pointer array variable `p_OUT`
      - output formatter function such as `get_XXX_mem` which store the correct
        values directly in the appropriate elements of `SW_OUT_RUN.p_OUT`
      - these output formatter functions are assigned to pointers
        `OutDom.pfunc_mem[k]` and called by SW_OUT_write_today()
      - currently used by `rSOILWAT2`


  __Below text is outdated as of June 2018 (retained until updated):__

  In detail:

  There are two output structures - SW_OUT_DOM & SW_OUT_RUN.

  The main structure used in SOILWAT2 is SW_OUT_DOM which holds output
  information that is consistent through domain simulations. This includes:
  1) Information from outsetup.in (array, an element per output key)
  2) Output function pointers (array, an element per output key)
  3) Other output information like output column names and time steps

  SW_OUT_RUN holds a small amount of output information in SOILWAT2
  the main information is
  1) First and last day of the current year during simulation
  2) Formatted output string for output files

  The output arrays in SW_OUT_DOM (e.g., mykey) are filled in by
  initialization process by matching defined macros of valid keys
  with enumeration variables used as indices into the arrays of
  information it contains. A similar combination of text macros
  and enumeration constants handles the TIMEPERIOD conversion
  from text to numeric index.

  The arrays being spoke of hold the output period code, start
  and end values, output file name, opened file pointer for output,
  on/off status, and a pointer to the function that prepares a complete
  line of formatted output per output period.

  A _construct() function clears the SW_OUT_DOM in it's entirety to set
  values and flags to zero. Those output objects that are turned off
  are ignored. Thus, to add a new output variable, a new get_function
  must be added to in addition to adding the new macro and enumeration
  keys for it.  Oh, and a line or two of summarizing code.

  After initialization, each valid output key has an element in
  SW_OUT_DOM that "knows" its parameters and whether it is on or
  off.  There is still space allocated for the "off" keys but they
  are ignored by the use flag.

  During the daily execution loop of the model, values for each of
  the output objects are accumulated via a call to
  SW_OUT_sum_today(x) function with x being a special enumeration
  code that defines the actual module object to be summed (see
  SW_Output.h).  This enumeration code breaks up the many output
  variables into a few simple types so that adding a new output
  variable is simplified by putting it into its proper category.

  When the _sum_today() function is called, it calls the averaging
  function which puts the sum, average, etc into the output
  accumulators--(dy|wk|mo|yr)avg--then conditionally clears the
  summary accumulators--(dy|wk|mo|yr)sum--if a new period has
  occurred (in preparation for the new period), then calls the
  function to handle collecting the summaries called collect_sums().

  The collect_sums() function needs the object type (eg, eSWC, eWTH)
  and the output period (eg, dy, wk, etc) and then, for each valid
  output key, it assigns a pointer to the appropriate object's
  summary sub-structure.  (This is where the complexity of this
  approach starts to become a bit clumsy, but it nonetheless tends to
  keep the overall code size down.) After assigning the pointer to
  the summary structure, the pointers are passed to a routine to
  actually do the accumulation for the various output objects
  (currently SWC and WTH).  No other arithmetic is performed here.
  This routine is only called, however, if the current day or period
  falls within the range specified by the user.  Otherwise, the
  accumulators will remain zero.  Also, the period check is used in
  other places to determine whether to bother with averaging and
  printing.

  Once a period other than daily has passed, the accumulated values
  are averaged or summed as appropriate within the average_for()
  subroutine as mentioned above.

  After the averaging function, the values are ready to format for
  output.  The SW_OUT_write_today() routine is called from the
  end_day() function in main(). Throughout the run for each period
  all used values are appended to a string and at the end of the period
  the string is written to the proper output file. The SW_OUT_write_today()
  function goes through each key and if in use, it calls
  populate_output_values() function to parse the output string and format it
  properly. After the string is formatted it is added to an output string which
  is written to the output File at the end of the period.

  So to summarize, adding another output quantity requires several steps.
  - Add an appropriate element to the SW_*_OUTPUTS substructure of the
  main object (eg SW_Soilwat) to hold the output value.
  - Define a new key string and add a macro definition and enumeration
  to the appropriate list in Output.h.  Be sure the new key's position
  in the list doesn't interfere with the ForEach*() loops.
  - Increase the value of SW_OUTNKEYS macro in Output.h.
  - Add the macro and enum keys to the key2str and key2obj lists in
  SW_Output.c as appropriate, IN THE SAME LIST POSITION.
  - Create and declare a get_*() function that returns the correctly
  formatted string for output.
  - Add a line to link the get_ function to the appropriate element in
  the SW_OUT_DOM output function array in _construct().
  - Add new code to the switch statement in sumof_*() to handle the new
  key.
  - Add new code to the switch statement in average_for() to do the
  summarizing.
  - Add new code to create_col_headers to make proper columns for new value
  - if variable is a soil variable (has layers) add name to SW_OUT_read,
  create_col_headers and populate_output_values in the if block checking for
  SOIL variables looks like below code `if (has_key_soillayers(key)) {`



  Comment (06/23/2015, akt): Adding Output at SOILWAT for further using at
  RSOILWAT and STEP as well

  Above details is good enough for knowing how to add a new output at soilwat.
  However here we are adding some more details about how we can add this output
  for further using that to RSOILWAT and STEP side as well.

  At the top with Comment (06/23/2015, drs): details about how output of SOILWAT
  works.

  Example : Adding extra place holder at existing output of SOILWAT for both
  STEP and RSOILWAT:
  - Adding extra place holder for existing output for both STEP and RSOILWAT:
  example adding extra output surfaceAvg at SW_WEATHER. We need to modified
  SW_Weather.h with adding a placeholder at SW_WEATHER and at inner structure
  SW_WEATHER_OUTPUTS.
  - Then somewhere this surfaceAvg value need to set at SW_WEATHER placeholder,
  here we add this atSW_Flow.c
  - Further modify file SW_Output.c ; add sum of surfaceAvg at function
  sumof_wth(). Then use this sum value to calculate average of surfaceAvg at
  function average_for().
  - Then go to function get_temp(), add extra placeholder like surfaceAvgVal
  that will store this average surfaceAvg value. Add this value to both STEP and
  RSOILWAT side code of this function for all the periods like weekly, monthly
  and yearly (for daily set day sum value of surfaceAvg not avg), add this
  surfaceAvgVal at end of this get_Temp() function for finally printing in
  output file.
  - Pass this surfaceAvgVal to sxw.h file from STEP, by adding extra placeholder
  at sxw.h so that STEP model can use this value there.
  - For using this surfaceAvg value in RSOILWAT side of function get_Temp(),
  increment index of p_Rtemp output array by one and add this sum value  for
  daily and avg value for other periods at last index.
  - Further need to modify SW_R_lib.c, for newOutput we need to add new
  pointers; functions start() and onGetOutput() will need to be modified. For
  this example adding extra placeholder at existing TEMP output so only function
  onGetOutput() need to be modified; add placeholder name for surfaceAvg at
  array Ctemp_names[] and then 	increment number of columns for Rtemp outputs
  (Rtemp_columns) by one.
  - At RSOILWAT further we will need to modify L_swOutput.R and G_swOut.R. At
  L_swOutput.R increment number of columns for swOutput_TEMP.

  So to summarize, adding extra place holder at existing output of SOILWAT for
  both STEP and RSOILWAT side code above steps are useful.

  However, adding another new output quantity requires several steps for SOILWAT
  and both STEP and RSOILWAT side code as well. So adding more information to
  above details (for adding  another new output quantity that can further use in
  both STEP and RSOILWAT) :
  - We need to modify SW_R_lib.c of SOILWAT; add new pointers; functions start()
  and onGetOutput() will need to be modified.
  - The sw_output.c of SOILWAT will need to be modified for new output quantity;
  add new pointers here too for RSOILWAT.
  - We will need to also read in the new config params from outputsetup_v30.in ;
  then we  will need to accumulate the new values ; write them out to file and
  assign the values to the RSOILWAT pointers.
  - At RSOILWAT we will need to modify L_swOutput.R and G_swOut.R

*/
