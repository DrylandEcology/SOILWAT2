
/* =================================================== */
/*                INCLUDES / DEFINES                   */
/* --------------------------------------------------- */
#include "include/SW_Domain.h"      // for SW_DOM_CheckProgress, SW_DOM_Cre...
#include "include/filefuncs.h"      // for LogError, CloseFile, key_to_id
#include "include/generic.h"        // for swTRUE, LOGERROR, swFALSE, Bool
#include "include/myMemory.h"       // for Str_Dup
#include "include/SW_datastructs.h" // for SW_DOMAIN, LOG_INFO
#include "include/SW_Defines.h"     // for LyrIndex, LARGE_VALUE, TimeInt
#include "include/SW_Files.h"       // for SW_F_deconstruct, SW_F_deepCopy
#include "include/SW_Output.h"      // for ForEachOutKey
#include "include/Times.h"          // for yearto4digit, Time_get_lastdoy_y
#include <stdio.h>                  // for sscanf, FILE
#include <stdlib.h>                 // for atoi, atof
#include <string.h>                 // for strcmp, memcpy, strcpy, memset

#if defined(SWNETCDF)
#include "include/SW_netCDF.h"
#endif

#if defined(SOILWAT)
#include "include/rands.h" // for RandSeed
#endif

/* converts an enum output key (OutKey type) to a module  */
/* or object type. see SW_Output.h for OutKey order.         */
/* MUST be SW_OUTNKEYS of these */
ObjType key2obj[] = {
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
/*                   Local Defines                     */
/* --------------------------------------------------- */

#define NUM_DOM_IN_KEYS 17 // Number of possible keys within `domain.in`

/* =================================================== */
/*             Private Function Declarations           */
/* --------------------------------------------------- */

/* =================================================== */
/*             Global Function Definitions             */
/* --------------------------------------------------- */

/**
@brief Calculate the suid for the start gridcell/site position

@param[in] SW_Domain Struct of type SW_DOMAIN holding constant
    temporal/spatial information for a set of simulation runs
@param[in] suid Unique identifier for a simulation run
@param[out] ncSuid Unique indentifier of the first suid to run
    in relation to netCDFs
*/
void SW_DOM_calc_ncSuid(
    SW_DOMAIN *SW_Domain, unsigned long suid, unsigned long ncSuid[]
) {

    if (strcmp(SW_Domain->DomainType, "s") == 0) {
        ncSuid[0] = suid;
        ncSuid[1] = 0;
    } else {
        ncSuid[0] = suid / SW_Domain->nDimX;
        ncSuid[1] = suid % SW_Domain->nDimX;
    }
}

/**
@brief Calculate the number of suids in the given domain

@param[in,out] SW_Domain Struct of type SW_DOMAIN holding constant
    temporal/spatial information for a set of simulation runs
*/
void SW_DOM_calc_nSUIDs(SW_DOMAIN *SW_Domain) {
    SW_Domain->nSUIDs = (strcmp(SW_Domain->DomainType, "s") == 0) ?
                            SW_Domain->nDimS :
                            SW_Domain->nDimX * SW_Domain->nDimY;
}

/**
@brief Check progress in domain

@param[in] progFileID Identifier of the progress netCDF file
@param[in] progVarID Identifier of the progress variable
@param[in] ncSuid Current simulation unit identifier for which is used
    to get data from netCDF
@param[in,out] LogInfo Holds information dealing with logfile output

@return
TRUE if simulation for \p ncSuid has not been completed yet;
FALSE if simulation for \p ncSuid has been completed (i.e., skip).
*/
Bool SW_DOM_CheckProgress(
    int progFileID, int progVarID, unsigned long ncSuid[], LOG_INFO *LogInfo
) {
#if defined(SWNETCDF)
    return SW_NC_check_progress(progFileID, progVarID, ncSuid, LogInfo);
#else
    (void) progFileID;
    (void) progVarID;
    (void) ncSuid;
    (void) LogInfo;
#endif

    // return TRUE (due to lack of capability to track progress)
    return swTRUE;
}

/**
@brief Create an empty progress netCDF

@param[in] SW_Domain Struct of type SW_DOMAIN holding constant
    temporal/spatial information for a set of simulation runs
@param[in] LogInfo Holds information dealing with logfile output
*/
void SW_DOM_CreateProgress(SW_DOMAIN *SW_Domain, LOG_INFO *LogInfo) {
#if defined(SWNETCDF)
    SW_NC_create_progress(SW_Domain, LogInfo);
#else
    (void) SW_Domain;
    (void) LogInfo;
#endif
}

/**
@brief Domain constructor for global variables which are constant between
    simulation runs.

@param[in] rng_seed Initial state for spinup RNG
@param[out] SW_Domain Struct of type SW_DOMAIN which
    holds constant temporal/spatial information for a set of simulation runs
*/
void SW_DOM_construct(unsigned long rng_seed, SW_DOMAIN *SW_Domain) {

    IntUS k;
    OutPeriod p;

    SW_OUT_DOM *OutDom = &SW_Domain->OutDom;

/* Set seed of `spinup_rng`
  - SOILWAT2: set seed here
  - STEPWAT2: `main()` uses `Globals.randseed` to (re-)set for each iteration
  - rSOILWAT2: R API handles RNGs
*/
#if defined(SOILWAT)
    RandSeed(rng_seed, 1u, &SW_Domain->SW_SpinUp.spinup_rng);
#else
    (void) rng_seed; // Silence compiler flag `-Wunused-parameter`
#endif

    SW_Domain->nMaxSoilLayers = 0;
    SW_Domain->nMaxEvapLayers = 0;
    SW_Domain->hasConsistentSoilLayerDepths = swFALSE;
    memset(
        &SW_Domain->depthsAllSoilLayers,
        0.,
        sizeof(&SW_Domain->depthsAllSoilLayers[0]) * MAX_LAYERS
    );

#if defined(SOILWAT)
    OutDom->print_SW_Output = swTRUE;
    OutDom->print_IterationSummary = swFALSE;
#elif defined(STEPWAT)
    OutDom->print_SW_Output = (Bool) OutDom->storeAllIterations;
// `print_IterationSummary` is set by STEPWAT2's `main` function
#endif

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

#if defined(SWNETCDF)
        OutDom->outputVarInfo[k] = NULL;
        OutDom->reqOutputVars[k] = NULL;
        OutDom->units_sw[k] = NULL;
        OutDom->uconv[k] = NULL;
#endif

        // assign `get_XXX` functions
        switch (k) {
        case eSW_Temp:
#if defined(SW_OUTTEXT)
            OutDom->pfunc_text[k] =
                (void (*)(OutPeriod, SW_RUN *)) get_temp_text;
#endif
#if defined(RSOILWAT) || defined(SWNETCDF)
            OutDom->pfunc_mem[k] =
                (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *)) get_temp_mem;
#elif defined(STEPWAT)
            OutDom->pfunc_agg[k] =
                (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *)) get_temp_agg;
            OutDom->pfunc_SXW[k] =
                (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *)) get_temp_SXW;
#endif
            break;

        case eSW_Precip:
#if defined(SW_OUTTEXT)
            OutDom->pfunc_text[k] =
                (void (*)(OutPeriod, SW_RUN *)) get_precip_text;
#endif
#if defined(RSOILWAT) || defined(SWNETCDF)
            OutDom->pfunc_mem[k] =
                (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *)) get_precip_mem;
#elif defined(STEPWAT)
            OutDom->pfunc_agg[k] =
                (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *)) get_precip_agg;
            OutDom->pfunc_SXW[k] =
                (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *)) get_precip_SXW;
#endif
            break;

        case eSW_VWCBulk:
#if defined(SW_OUTTEXT)
            OutDom->pfunc_text[k] =
                (void (*)(OutPeriod, SW_RUN *)) get_vwcBulk_text;
#endif
#if defined(RSOILWAT) || defined(SWNETCDF)
            OutDom->pfunc_mem[k] =
                (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *)) get_vwcBulk_mem;
#elif defined(STEPWAT)
            OutDom->pfunc_agg[k] =
                (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *)) get_vwcBulk_agg;
            OutDom->pfunc_SXW[k] =
                (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *)) get_none_outarray;
#endif
            break;

        case eSW_VWCMatric:
#if defined(SW_OUTTEXT)
            OutDom->pfunc_text[k] =
                (void (*)(OutPeriod, SW_RUN *)) get_vwcMatric_text;
#endif
#if defined(RSOILWAT) || defined(SWNETCDF)
            OutDom->pfunc_mem[k] =
                (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *)) get_vwcMatric_mem;
#elif defined(STEPWAT)
            OutDom->pfunc_agg[k] =
                (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *)) get_vwcMatric_agg;
            OutDom->pfunc_SXW[k] =
                (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *)) get_none_outarray;
#endif
            break;

        case eSW_SWCBulk:
#if defined(SW_OUTTEXT)
            OutDom->pfunc_text[k] =
                (void (*)(OutPeriod, SW_RUN *)) get_swcBulk_text;
#endif
#if defined(RSOILWAT) || defined(SWNETCDF)
            OutDom->pfunc_mem[k] =
                (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *)) get_swcBulk_mem;
#elif defined(STEPWAT)
            OutDom->pfunc_agg[k] =
                (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *)) get_swcBulk_agg;
            OutDom->pfunc_SXW[k] =
                (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *)) get_swcBulk_SXW;
#endif
            break;

        case eSW_SWPMatric:
#if defined(SW_OUTTEXT)
            OutDom->pfunc_text[k] =
                (void (*)(OutPeriod, SW_RUN *)) get_swpMatric_text;
#endif
#if defined(RSOILWAT) || defined(SWNETCDF)
            OutDom->pfunc_mem[k] =
                (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *)) get_swpMatric_mem;
#elif defined(STEPWAT)
            OutDom->pfunc_agg[k] =
                (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *)) get_swpMatric_agg;
            OutDom->pfunc_SXW[k] =
                (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *)) get_none_outarray;
#endif
            break;

        case eSW_SWABulk:
#if defined(SW_OUTTEXT)
            OutDom->pfunc_text[k] =
                (void (*)(OutPeriod, SW_RUN *)) get_swaBulk_text;
#endif
#if defined(RSOILWAT) || defined(SWNETCDF)
            OutDom->pfunc_mem[k] =
                (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *)) get_swaBulk_mem;
#elif defined(STEPWAT)
            OutDom->pfunc_agg[k] =
                (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *)) get_swaBulk_agg;
            OutDom->pfunc_SXW[k] =
                (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *)) get_none_outarray;
#endif
            break;

        case eSW_SWAMatric:
#if defined(SW_OUTTEXT)
            OutDom->pfunc_text[k] =
                (void (*)(OutPeriod, SW_RUN *)) get_swaMatric_text;
#endif
#if defined(RSOILWAT) || defined(SWNETCDF)
            OutDom->pfunc_mem[k] =
                (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *)) get_swaMatric_mem;
#elif defined(STEPWAT)
            OutDom->pfunc_agg[k] =
                (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *)) get_swaMatric_agg;
            OutDom->pfunc_SXW[k] =
                (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *)) get_none_outarray;
#endif
            break;

        case eSW_SWA:
#if defined(SW_OUTTEXT)
            OutDom->pfunc_text[k] =
                (void (*)(OutPeriod, SW_RUN *)) get_swa_text;
#endif
#if defined(RSOILWAT) || defined(SWNETCDF)
            OutDom->pfunc_mem[k] =
                (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *)) get_swa_mem;
#elif defined(STEPWAT)
            OutDom->pfunc_agg[k] =
                (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *)) get_swa_agg;
            OutDom->pfunc_SXW[k] =
                (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *)) get_none_outarray;
#endif
            break;

        case eSW_SurfaceWater:
#if defined(SW_OUTTEXT)
            OutDom->pfunc_text[k] =
                (void (*)(OutPeriod, SW_RUN *)) get_surfaceWater_text;
#endif
#if defined(RSOILWAT) || defined(SWNETCDF)
            OutDom->pfunc_mem[k] = (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *)
            ) get_surfaceWater_mem;
#elif defined(STEPWAT)
            OutDom->pfunc_agg[k] = (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *)
            ) get_surfaceWater_agg;
            OutDom->pfunc_SXW[k] =
                (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *)) get_none_outarray;
#endif
            break;

        case eSW_Runoff:
#if defined(SW_OUTTEXT)
            OutDom->pfunc_text[k] =
                (void (*)(OutPeriod, SW_RUN *)) get_runoffrunon_text;
#endif
#if defined(RSOILWAT) || defined(SWNETCDF)
            OutDom->pfunc_mem[k] = (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *)
            ) get_runoffrunon_mem;
#elif defined(STEPWAT)
            OutDom->pfunc_agg[k] = (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *)
            ) get_runoffrunon_agg;
            OutDom->pfunc_SXW[k] =
                (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *)) get_none_outarray;
#endif
            break;

        case eSW_Transp:
#if defined(SW_OUTTEXT)
            OutDom->pfunc_text[k] =
                (void (*)(OutPeriod, SW_RUN *)) get_transp_text;
#endif
#if defined(RSOILWAT) || defined(SWNETCDF)
            OutDom->pfunc_mem[k] =
                (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *)) get_transp_mem;
#elif defined(STEPWAT)
            OutDom->pfunc_agg[k] =
                (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *)) get_transp_agg;
            OutDom->pfunc_SXW[k] =
                (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *)) get_transp_SXW;
#endif
            break;

        case eSW_EvapSoil:
#if defined(SW_OUTTEXT)
            OutDom->pfunc_text[k] =
                (void (*)(OutPeriod, SW_RUN *)) get_evapSoil_text;
#endif
#if defined(RSOILWAT) || defined(SWNETCDF)
            OutDom->pfunc_mem[k] =
                (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *)) get_evapSoil_mem;
#elif defined(STEPWAT)
            OutDom->pfunc_agg[k] =
                (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *)) get_evapSoil_agg;
            OutDom->pfunc_SXW[k] =
                (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *)) get_none_outarray;
#endif
            break;

        case eSW_EvapSurface:
#if defined(SW_OUTTEXT)
            OutDom->pfunc_text[k] =
                (void (*)(OutPeriod, SW_RUN *)) get_evapSurface_text;
#endif
#if defined(RSOILWAT) || defined(SWNETCDF)
            OutDom->pfunc_mem[k] = (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *)
            ) get_evapSurface_mem;
#elif defined(STEPWAT)
            OutDom->pfunc_agg[k] = (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *)
            ) get_evapSurface_agg;
            OutDom->pfunc_SXW[k] =
                (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *)) get_none_outarray;
#endif
            break;

        case eSW_Interception:
#if defined(SW_OUTTEXT)
            OutDom->pfunc_text[k] =
                (void (*)(OutPeriod, SW_RUN *)) get_interception_text;
#endif
#if defined(RSOILWAT) || defined(SWNETCDF)
            OutDom->pfunc_mem[k] = (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *)
            ) get_interception_mem;
#elif defined(STEPWAT)
            OutDom->pfunc_agg[k] = (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *)
            ) get_interception_agg;
            OutDom->pfunc_SXW[k] =
                (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *)) get_none_outarray;
#endif
            break;

        case eSW_SoilInf:
#if defined(SW_OUTTEXT)
            OutDom->pfunc_text[k] =
                (void (*)(OutPeriod, SW_RUN *)) get_soilinf_text;
#endif
#if defined(RSOILWAT) || defined(SWNETCDF)
            OutDom->pfunc_mem[k] =
                (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *)) get_soilinf_mem;
#elif defined(STEPWAT)
            OutDom->pfunc_agg[k] =
                (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *)) get_soilinf_agg;
            OutDom->pfunc_SXW[k] =
                (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *)) get_none_outarray;
#endif
            break;

        case eSW_LyrDrain:
#if defined(SW_OUTTEXT)
            OutDom->pfunc_text[k] =
                (void (*)(OutPeriod, SW_RUN *)) get_lyrdrain_text;
#endif
#if defined(RSOILWAT) || defined(SWNETCDF)
            OutDom->pfunc_mem[k] =
                (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *)) get_lyrdrain_mem;
#elif defined(STEPWAT)
            OutDom->pfunc_agg[k] =
                (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *)) get_lyrdrain_agg;
            OutDom->pfunc_SXW[k] =
                (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *)) get_none_outarray;
#endif
            break;

        case eSW_HydRed:
#if defined(SW_OUTTEXT)
            OutDom->pfunc_text[k] =
                (void (*)(OutPeriod, SW_RUN *)) get_hydred_text;
#endif
#if defined(RSOILWAT) || defined(SWNETCDF)
            OutDom->pfunc_mem[k] =
                (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *)) get_hydred_mem;
#elif defined(STEPWAT)
            OutDom->pfunc_agg[k] =
                (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *)) get_hydred_agg;
            OutDom->pfunc_SXW[k] =
                (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *)) get_none_outarray;
#endif
            break;

        case eSW_AET:
#if defined(SW_OUTTEXT)
            OutDom->pfunc_text[k] =
                (void (*)(OutPeriod, SW_RUN *)) get_aet_text;
#endif
#if defined(RSOILWAT) || defined(SWNETCDF)
            OutDom->pfunc_mem[k] =
                (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *)) get_aet_mem;
#elif defined(STEPWAT)
            OutDom->pfunc_agg[k] =
                (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *)) get_aet_agg;
            OutDom->pfunc_SXW[k] =
                (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *)) get_aet_SXW;
#endif
            break;

        case eSW_PET:
#if defined(SW_OUTTEXT)
            OutDom->pfunc_text[k] =
                (void (*)(OutPeriod, SW_RUN *)) get_pet_text;
#endif
#if defined(RSOILWAT) || defined(SWNETCDF)
            OutDom->pfunc_mem[k] =
                (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *)) get_pet_mem;
#elif defined(STEPWAT)
            OutDom->pfunc_agg[k] =
                (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *)) get_pet_agg;
            OutDom->pfunc_SXW[k] =
                (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *)) get_none_outarray;
#endif
            break;

        case eSW_WetDays:
#if defined(SW_OUTTEXT)
            OutDom->pfunc_text[k] =
                (void (*)(OutPeriod, SW_RUN *)) get_wetdays_text;
#endif
#if defined(RSOILWAT) || defined(SWNETCDF)
            OutDom->pfunc_mem[k] =
                (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *)) get_wetdays_mem;
#elif defined(STEPWAT)
            OutDom->pfunc_agg[k] =
                (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *)) get_wetdays_agg;
            OutDom->pfunc_SXW[k] =
                (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *)) get_none_outarray;
#endif
            break;

        case eSW_SnowPack:
#if defined(SW_OUTTEXT)
            OutDom->pfunc_text[k] =
                (void (*)(OutPeriod, SW_RUN *)) get_snowpack_text;
#endif
#if defined(RSOILWAT) || defined(SWNETCDF)
            OutDom->pfunc_mem[k] =
                (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *)) get_snowpack_mem;
#elif defined(STEPWAT)
            OutDom->pfunc_agg[k] =
                (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *)) get_snowpack_agg;
            OutDom->pfunc_SXW[k] =
                (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *)) get_none_outarray;
#endif
            break;

        case eSW_DeepSWC:
#if defined(SW_OUTTEXT)
            OutDom->pfunc_text[k] =
                (void (*)(OutPeriod, SW_RUN *)) get_deepswc_text;
#endif
#if defined(RSOILWAT) || defined(SWNETCDF)
            OutDom->pfunc_mem[k] =
                (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *)) get_deepswc_mem;
#elif defined(STEPWAT)
            OutDom->pfunc_agg[k] =
                (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *)) get_deepswc_agg;
            OutDom->pfunc_SXW[k] =
                (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *)) get_none_outarray;
#endif
            break;

        case eSW_SoilTemp:
#if defined(SW_OUTTEXT)
            OutDom->pfunc_text[k] =
                (void (*)(OutPeriod, SW_RUN *)) get_soiltemp_text;
#endif
#if defined(RSOILWAT) || defined(SWNETCDF)
            OutDom->pfunc_mem[k] =
                (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *)) get_soiltemp_mem;
#elif defined(STEPWAT)
            OutDom->pfunc_agg[k] =
                (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *)) get_soiltemp_agg;
            OutDom->pfunc_SXW[k] =
                (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *)) get_none_outarray;
#endif
            break;

        case eSW_Frozen:
#if defined(SW_OUTTEXT)
            OutDom->pfunc_text[k] =
                (void (*)(OutPeriod, SW_RUN *)) get_frozen_text;
#endif
#if defined(RSOILWAT) || defined(SWNETCDF)
            OutDom->pfunc_mem[k] =
                (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *)) get_frozen_mem;
#elif defined(STEPWAT)
            OutDom->pfunc_agg[k] =
                (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *)) get_frozen_agg;
            OutDom->pfunc_SXW[k] =
                (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *)) get_none_outarray;
#endif
            break;

        case eSW_Estab:
#if defined(SW_OUTTEXT)
            OutDom->pfunc_text[k] =
                (void (*)(OutPeriod, SW_RUN *)) get_estab_text;
#endif
#if defined(RSOILWAT) || defined(SWNETCDF)
            OutDom->pfunc_mem[k] =
                (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *)) get_estab_mem;
#elif defined(STEPWAT)
            OutDom->pfunc_agg[k] =
                (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *)) get_estab_agg;
            OutDom->pfunc_SXW[k] =
                (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *)) get_none_outarray;
#endif
            break;

        case eSW_CO2Effects:
#if defined(SW_OUTTEXT)
            OutDom->pfunc_text[k] =
                (void (*)(OutPeriod, SW_RUN *)) get_co2effects_text;
#endif
#if defined(RSOILWAT) || defined(SWNETCDF)
            OutDom->pfunc_mem[k] = (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *)
            ) get_co2effects_mem;
#elif defined(STEPWAT)
            OutDom->pfunc_agg[k] = (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *)
            ) get_co2effects_agg;
            OutDom->pfunc_SXW[k] =
                (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *)) get_none_outarray;
#endif
            break;

        case eSW_Biomass:
#if defined(SW_OUTTEXT)
            OutDom->pfunc_text[k] =
                (void (*)(OutPeriod, SW_RUN *)) get_biomass_text;
#endif
#if defined(RSOILWAT) || defined(SWNETCDF)
            OutDom->pfunc_mem[k] =
                (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *)) get_biomass_mem;
#elif defined(STEPWAT)
            OutDom->pfunc_agg[k] =
                (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *)) get_biomass_agg;
            OutDom->pfunc_SXW[k] =
                (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *)) get_none_outarray;
#endif
            break;

        default:
#if defined(SW_OUTTEXT)
            OutDom->pfunc_text[k] =
                (void (*)(OutPeriod, SW_RUN *)) get_none_text;
#endif
#if defined(RSOILWAT) || defined(SWNETCDF)
            OutDom->pfunc_mem[k] =
                (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *)) get_none_outarray;
#elif defined(STEPWAT)
            OutDom->pfunc_agg[k] =
                (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *)) get_none_outarray;
            OutDom->pfunc_SXW[k] =
                (void (*)(OutPeriod, SW_RUN *, SW_OUT_DOM *)) get_none_outarray;
#endif
            break;
        }
    } // end of loop across output keys
}

/**
@brief Read `domain.in` and report any problems encountered when doing so

@param[in,out] SW_Domain Struct of type SW_DOMAIN holding constant
    temporal/spatial information for a set of simulation runs
@param[in] LogInfo Holds information on warnings and errors
*/
void SW_DOM_read(SW_DOMAIN *SW_Domain, LOG_INFO *LogInfo) {

    static const char *possibleKeys[NUM_DOM_IN_KEYS] = {
        "Domain",
        "nDimX",
        "nDimY",
        "nDimS",
        "StartYear",
        "EndYear",
        "StartDoy",
        "EndDoy",
        "crs_bbox",
        "xmin_bbox",
        "ymin_bbox",
        "xmax_bbox",
        "ymax_bbox",
        "SpinupMode",
        "SpinupScope",
        "SpinupDuration",
        "SpinupSeed"
    };
    static const Bool requiredKeys[NUM_DOM_IN_KEYS] = {
        swTRUE,
        swTRUE,
        swTRUE,
        swTRUE,
        swTRUE,
        swTRUE,
        swFALSE,
        swFALSE,
        swTRUE,
        swTRUE,
        swTRUE,
        swTRUE,
        swTRUE,
        swTRUE,
        swTRUE,
        swTRUE,
        swTRUE
    };
    Bool hasKeys[NUM_DOM_IN_KEYS] = {swFALSE};

    FILE *f;
    int y, keyID;
    char inbuf[LARGE_VALUE], *MyFileName;
    char key[15], value[LARGE_VALUE]; // 15 - Max key size

    MyFileName = SW_Domain->PathInfo.InFiles[eDomain];
    f = OpenFile(MyFileName, "r", LogInfo);
    if (LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

    // Set SW_DOMAIN
    while (GetALine(f, inbuf, LARGE_VALUE)) {
        sscanf(inbuf, "%14s %s", key, value);

        keyID = key_to_id(key, possibleKeys, NUM_DOM_IN_KEYS);

        set_hasKey(keyID, possibleKeys, hasKeys, LogInfo);
        // set_hasKey() produces never an error, only possibly warnings

        switch (keyID) {
        case 0: // Domain type
            if (strcmp(value, "xy") != 0 && strcmp(value, "s") != 0) {
                LogError(
                    LogInfo,
                    LOGERROR,
                    "%s: Incorrect domain type %s."
                    " Please select from \"xy\" and \"s\".",
                    MyFileName,
                    value
                );
                return; // Exit function prematurely due to error
            }
            strcpy(SW_Domain->DomainType, value);
            break;
        case 1: // Number of X slots
            SW_Domain->nDimX = atoi(value);
            break;
        case 2: // Number of Y slots
            SW_Domain->nDimY = atoi(value);
            break;
        case 3: // Number of S slots
            SW_Domain->nDimS = atoi(value);
            break;

        case 4: // Start year
            y = atoi(value);

            if (y < 0) {
                CloseFile(&f, LogInfo);
                LogError(
                    LogInfo,
                    LOGERROR,
                    "%s: Negative start year (%d)",
                    MyFileName,
                    y
                );
                return; // Exit function prematurely due to error
            }
            SW_Domain->startyr = yearto4digit((TimeInt) y);
            break;
        case 5: // End year
            y = atoi(value);

            if (y < 0) {
                CloseFile(&f, LogInfo);
                LogError(
                    LogInfo,
                    LOGERROR,
                    "%s: Negative ending year (%d)",
                    MyFileName,
                    y
                );
                return; // Exit function prematurely due to error
            }
            SW_Domain->endyr = yearto4digit((TimeInt) y);
            break;
        case 6: // Start day of year
            SW_Domain->startstart = atoi(value);
            break;
        case 7: // End day of year
            SW_Domain->endend = atoi(value);
            break;

        case 8: // CRS box
            // Re-scan and get the entire value (including spaces)
            sscanf(inbuf, "%9s %27[^\n]", key, value);
            strcpy(SW_Domain->crs_bbox, value);
            break;
        case 9: // Minimum x coordinate
            SW_Domain->min_x = atof(value);
            break;
        case 10: // Minimum y coordinate
            SW_Domain->min_y = atof(value);
            break;
        case 11: // Maximum x coordinate
            SW_Domain->max_x = atof(value);
            break;
        case 12: // Maximum y coordinate
            SW_Domain->max_y = atof(value);
            break;

        case 13: // Spinup Mode
            y = atoi(value);

            if (y != 1 && y != 2) {
                CloseFile(&f, LogInfo);
                LogError(
                    LogInfo,
                    LOGERROR,
                    "%s: Incorrect Mode (%d) for spinup"
                    " Please select \"1\" or \"2\"",
                    MyFileName,
                    y
                );
                return; // Exit function prematurely due to error
            }
            SW_Domain->SW_SpinUp.mode = y;
            break;
        case 14: // Spinup Scope
            SW_Domain->SW_SpinUp.scope = atoi(value);
            break;
        case 15: // Spinup Duration
            SW_Domain->SW_SpinUp.duration = atoi(value);

            // Set the spinup flag to true if duration > 0
            if (SW_Domain->SW_SpinUp.duration <= 0) {
                SW_Domain->SW_SpinUp.spinup = swFALSE;
            } else {
                SW_Domain->SW_SpinUp.spinup = swTRUE;
            }
            break;
        case 16: // Spinup Seed
            SW_Domain->SW_SpinUp.rng_seed = atoi(value);
            break;

        case KEY_NOT_FOUND: // Unknown key
            LogError(
                LogInfo,
                LOGWARN,
                "%s: Ignoring an unknown key, %s",
                MyFileName,
                key
            );
            break;
        }
    }

    CloseFile(&f, LogInfo);


    // Check if all required input was provided
    check_requiredKeys(
        hasKeys, requiredKeys, possibleKeys, NUM_DOM_IN_KEYS, LogInfo
    );
    if (LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

    if (SW_Domain->endyr < SW_Domain->startyr) {
        LogError(LogInfo, LOGERROR, "%s: Start Year > End Year", MyFileName);
        return; // Exit function prematurely due to error
    }

    // Check if start day of year was not found
    keyID = key_to_id("StartDoy", possibleKeys, NUM_DOM_IN_KEYS);
    if (!hasKeys[keyID]) {
        LogError(LogInfo, LOGWARN, "Domain.in: Missing Start Day - using 1\n");
        SW_Domain->startstart = 1;
    }

    // Check end day of year
    keyID = key_to_id("EndDoy", possibleKeys, NUM_DOM_IN_KEYS);
    if (SW_Domain->endend == 365 || !hasKeys[keyID]) {
        // Make sure last day is correct if last year is a leap year and
        // last day is last day of that year
        SW_Domain->endend = Time_get_lastdoy_y(SW_Domain->endyr);
    }
    if (!hasKeys[keyID]) {
        LogError(
            LogInfo,
            LOGWARN,
            "Domain.in: Missing End Day - using %d\n",
            SW_Domain->endend
        );
    }

    // Check bounding box coordinates
    if (GT(SW_Domain->min_x, SW_Domain->max_x)) {
        LogError(LogInfo, LOGERROR, "Domain.in: bbox x-axis min > max.");
        return; // Exit function prematurely due to error
    }

    if (GT(SW_Domain->min_y, SW_Domain->max_y)) {
        LogError(LogInfo, LOGERROR, "Domain.in: bbox y-axis min > max.");
        return; // Exit function prematurely due to error
    }

    // Check if scope value is out of range
    if (SW_Domain->SW_SpinUp.scope < 1 ||
        SW_Domain->SW_SpinUp.scope > (SW_Domain->endyr - SW_Domain->startyr)) {
        LogError(
            LogInfo,
            LOGERROR,
            "%s: Invalid Scope (N = %d) for spinup",
            MyFileName,
            SW_Domain->SW_SpinUp.scope
        );
        return; // Exit function prematurely due to error
    }
}

/**
@brief Mark completion status of simulation run

@param[in] isFailure Did simulation run fail or succeed?
@param[in] domType Type of domain in which simulations are running
    (gridcell/sites)
@param[in] progFileID Identifier of the progress netCDF file
@param[in] progVarID Identifier of the progress variable
@param[in] ncSuid Unique indentifier of the first suid to run
    in relation to netCDFs
@param[in,out] LogInfo
*/
void SW_DOM_SetProgress(
    Bool isFailure,
    const char *domType,
    int progFileID,
    int progVarID,
    unsigned long ncSuid[],
    LOG_INFO *LogInfo
) {

#if defined(SWNETCDF)
    SW_NC_set_progress(
        isFailure, domType, progFileID, progVarID, ncSuid, LogInfo
    );
#else
    (void) isFailure;
    (void) progFileID;
    (void) progVarID;
    (void) ncSuid;
    (void) LogInfo;
    (void) domType;
#endif
}

/**
@brief Calculate range of suids to run simulations for

@param[in,out] SW_Domain Struct of type SW_DOMAIN holding constant
    temporal/spatial information for a set of simulation runs
@param[in] userSUID Simulation Unit Identifier requested by the user (base1);
    0 indicates that all simulations units within domain are requested
@param[out] LogInfo Holds information on warnings and errors
*/
void SW_DOM_SimSet(
    SW_DOMAIN *SW_Domain, unsigned long userSUID, LOG_INFO *LogInfo
) {

    Bool progFound;
    unsigned long *startSimSet = &SW_Domain->startSimSet,
                  *endSimSet = &SW_Domain->endSimSet,
                  startSuid[2]; // 2 -> [y, x] or [0, s]
    int progFileID = 0; // Value does not matter if SWNETCDF is not defined
    int progVarID = 0;  // Value does not matter if SWNETCDF is not defined

#if defined(SWNETCDF)
    progFileID = SW_Domain->netCDFInfo.ncFileIDs[vNCprog];
    progVarID = SW_Domain->netCDFInfo.ncVarIDs[vNCprog];
#endif

    if (userSUID > 0) {
        if (userSUID > SW_Domain->nSUIDs) {
            LogError(
                LogInfo,
                LOGERROR,
                "User requested simulation unit (suid = %lu) "
                "does not exist in simulation domain (n = %lu).",
                userSUID,
                SW_Domain->nSUIDs
            );
            return; // Exit function prematurely due to error
        }

        *startSimSet = userSUID - 1;
        *endSimSet = userSUID;
    } else {
#if defined(SOILWAT)
        if (LogInfo->printProgressMsg) {
            sw_message("is identifying the simulation set ...");
        }
#endif

        *endSimSet = SW_Domain->nSUIDs;
        for (*startSimSet = 0; *startSimSet < *endSimSet; (*startSimSet)++) {
            SW_DOM_calc_ncSuid(SW_Domain, *startSimSet, startSuid);

            progFound =
                SW_DOM_CheckProgress(progFileID, progVarID, startSuid, LogInfo);

            if (progFound || LogInfo->stopRun) {
                return; // Found start suid or error occurred
            }
        }
    }
}

void SW_DOM_deepCopy(SW_DOMAIN *source, SW_DOMAIN *dest, LOG_INFO *LogInfo) {
    IntUS k, i;

    memcpy(dest, source, sizeof(*dest));

    SW_F_deepCopy(&dest->PathInfo, &source->PathInfo, LogInfo);

#if defined(SWNETCDF)
    SW_NC_deepCopy(&dest->netCDFInfo, &source->netCDFInfo, LogInfo);
#endif

    ForEachOutKey(k) {
        for (i = 0; i < 5 * NVEGTYPES + MAX_LAYERS; i++) {
            if (!isnull(source->OutDom.colnames_OUT[k][i])) {

                dest->OutDom.colnames_OUT[k][i] =
                    Str_Dup(source->OutDom.colnames_OUT[k][i], LogInfo);
                if (LogInfo->stopRun) {
                    return; // Exit function prematurely due to error
                }
            }
        }
    }
}

void SW_DOM_init_ptrs(SW_DOMAIN *SW_Domain) {
    IntUS key, column;

    ForEachOutKey(key) {
        for (column = 0; column < 5 * NVEGTYPES + MAX_LAYERS; column++) {
            SW_Domain->OutDom.colnames_OUT[key][column] = NULL;
        }
    }

    SW_F_init_ptrs(SW_Domain->PathInfo.InFiles);

#ifdef RSOILWAT
    ForEachOutKey(key) { SW_Domain->OutDom.outfile[key] = NULL; }
#endif

#if defined(SWNETCDF)
    SW_NC_init_ptrs(&SW_Domain->netCDFInfo);
#endif
}

void SW_DOM_deconstruct(SW_DOMAIN *SW_Domain) {
    IntUS k, i;

    SW_F_deconstruct(SW_Domain->PathInfo.InFiles);

#if defined(SWNETCDF)

    SW_NC_deconstruct(&SW_Domain->netCDFInfo);
    SW_NC_close_files(&SW_Domain->netCDFInfo);

    ForEachOutKey(k) {
        SW_NC_dealloc_outputkey_var_info(&SW_Domain->OutDom, k);
    }
#endif
    ForEachOutKey(k) {
        for (i = 0; i < 5 * NVEGTYPES + MAX_LAYERS; i++) {
            if (!isnull(SW_Domain->OutDom.colnames_OUT[k][i])) {
                free(SW_Domain->OutDom.colnames_OUT[k][i]);
                SW_Domain->OutDom.colnames_OUT[k][i] = NULL;
            }
        }
#ifdef RSOILWAT
        if (!isnull(SW_Domain->OutDom.outfile[k])) {
            free(SW_Domain->OutDom.outfile[k]);
            SW_Domain->OutDom.outfile[k] = NULL;
        }
#endif
    }
}

/** Identify soil profile information across simulation domain

    @param[out] hasConsistentSoilLayerDepths Flag indicating if all simulation
        run within domain have identical soil layer depths
        (though potentially variable number of soil layers)
    @param[out] nMaxSoilLayers Largest number of soil layers across
        simulation domain
    @param[out] nMaxEvapLayers Largest number of soil layers from which
        bare-soil evaporation may extract water across simulation domain
    @param[out] depthsAllSoilLayers Lower soil layer depths [cm] if
        consistent across simulation domain
    @param[in] default_n_layers Default number of soil layer
    @param[in] default_n_evap_lyrs Default number of soil layer used for
        bare-soil evaporation
    @param[in] default_depths Default values of soil layer depths [cm]
    @param[out] LogInfo Holds information on warnings and errors
*/
void SW_DOM_soilProfile(
    Bool *hasConsistentSoilLayerDepths,
    LyrIndex *nMaxSoilLayers,
    LyrIndex *nMaxEvapLayers,
    double depthsAllSoilLayers[],
    LyrIndex default_n_layers,
    LyrIndex default_n_evap_lyrs,
    double default_depths[],
    LOG_INFO *LogInfo
) {

#if defined(SWNETCDF)
    SW_NC_soilProfile(
        hasConsistentSoilLayerDepths,
        nMaxSoilLayers,
        nMaxEvapLayers,
        depthsAllSoilLayers,
        default_n_layers,
        default_n_evap_lyrs,
        default_depths,
        LogInfo
    );

#else

    // Assume default/template values are consistent
    *hasConsistentSoilLayerDepths = swTRUE;
    *nMaxSoilLayers = default_n_layers;
    *nMaxEvapLayers = default_n_evap_lyrs;

    memcpy(
        depthsAllSoilLayers,
        default_depths,
        sizeof(default_depths[0]) * default_n_layers
    );

    (void) LogInfo;

#endif // !SWNETCDF
}

/* =================================================== */
/*             Local Function Definitions              */
/* --------------------------------------------------- */
