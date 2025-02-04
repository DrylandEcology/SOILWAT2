/********************************************************/
/********************************************************/
/**
@file
@brief Format user-specified output for text and/or in-memory processing

See the \ref out_algo "output algorithm documentation" for details.

History:
2018 June 04 (drs) moved output formatter `get_XXX` functions from
`SW_Output.c` to dedicated `SW_Output_get_functions.c`
*/
/********************************************************/
/********************************************************/


/* =================================================== */
/*                INCLUDES / DEFINES                   */
/* --------------------------------------------------- */

#include "include/generic.h"        // for IntU
#include "include/myMemory.h"       // for sw_memccpy_custom
#include "include/SW_datastructs.h" // for SW_RUN, SW_OUTTEXT
#include "include/SW_Defines.h"     // for OUTSEP, OUT_DIGITS, OUTSTRLEN
#include "include/SW_Output.h"      // for get_aet_text, get_biomass_text
#include "include/SW_SoilWater.h"   // for SW_SWRC_SWCtoSWP
#include "include/SW_VegProd.h"     // for BIO_INDEX, WUE_INDEX

#if defined(SWNETCDF)
#include <netcdf.h> // defines NC_FILL_DOUBLE
#endif

#if defined(SWNETCDF) || defined(RSOILWAT)
#include <stddef.h> // for NULL
#endif

// Array-based output declarations:
#if defined(SW_OUTARRAY)
#include "include/SW_Output_outarray.h" // for iOUT, ncol_TimeOUT, get_outv...
#endif

#if defined(SW_OUTTEXT)
#include <stdio.h>  // for snprintf, NULL
#include <string.h> // for memccpy
#endif


/* =================================================== */
/*             Local Function Definitions              */
/* --------------------------------------------------- */

#ifdef STEPWAT
static void format_IterationSummary(
    double *p,
    double *psd,
    OutPeriod pd,
    IntUS N,
    SW_RUN *sw,
    const size_t nrow_OUT[],
    LOG_INFO *LogInfo
) {
    IntUS i;
    size_t n;
    double sd;
    char str[OUTSTRLEN];
    size_t writeSize = OUTSTRLEN;
    char *writePtr = sw->OutRun.sw_outstr_agg;
    char *endOutstrAgg =
        sw->OutRun.sw_outstr_agg + sizeof sw->OutRun.sw_outstr_agg - 1;
    SW_OUT_RUN *OutRun = &sw->OutRun;
    Bool fullBuffer = swFALSE;

    for (i = 0; i < N; i++) {
        n = iOUT(i, OutRun->irow_OUT[pd], nrow_OUT[pd], ncol_TimeOUT[pd]);
        sd = final_running_sd(sw->Model.runModelIterations, psd[n]);

        (void) snprintf(
            str,
            OUTSTRLEN,
            "%c%.*f%c%.*f",
            OUTSEP,
            OUT_DIGITS,
            p[n],
            OUTSEP,
            OUT_DIGITS,
            sd
        );
        fullBuffer = sw_memccpy_inc(
            (void **) &writePtr, endOutstrAgg, (void *) str, '\0', &writeSize
        );
        if (fullBuffer) {
            reportFullBuffer(LOGERROR, LogInfo);
            return;
        }
    }
}

static void format_IterationSummary2(
    double *p,
    double *psd,
    OutPeriod pd,
    IntUS N1,
    IntUS offset,
    SW_RUN *sw,
    const size_t nrow_OUT[],
    LOG_INFO *LogInfo
) {
    int k;
    LyrIndex i;
    size_t n;
    double sd;
    char str[OUTSTRLEN];
    size_t strLen = strlen(sw->OutRun.sw_outstr_agg);
    size_t writeSize = OUTSTRLEN - strLen;
    char *writePtr = sw->OutRun.sw_outstr_agg + strLen;
    char *endOutstrAgg =
        sw->OutRun.sw_outstr_agg + sizeof sw->OutRun.sw_outstr_agg - 1;
    SW_OUT_RUN *OutRun = &sw->OutRun;
    Bool fullBuffer = swFALSE;

    for (k = 0; k < N1; k++) {
        for (i = 0; i < sw->Site.n_layers; i++) {
            n = iOUT2(
                i, k + offset, pd, OutRun->irow_OUT, nrow_OUT, sw->Site.n_layers
            );
            sd = final_running_sd(sw->Model.runModelIterations, psd[n]);

            (void) snprintf(
                str,
                OUTSTRLEN,
                "%c%.*f%c%.*f",
                OUTSEP,
                OUT_DIGITS,
                p[n],
                OUTSEP,
                OUT_DIGITS,
                sd
            );
            fullBuffer = sw_memccpy_inc(
                (void **) &writePtr,
                endOutstrAgg,
                (void *) str,
                '\0',
                &writeSize
            );
            if (fullBuffer) {
                reportFullBuffer(LOGERROR, LogInfo);
                return;
            }
        }
    }
}

#endif


/* =================================================== */
/*             Global Function Definitions             */
/*             (declared in SW_Output.h)               */
/* --------------------------------------------------- */

#if defined(RSOILWAT) || defined(SWNETCDF)
/**
@brief Output routine for quantities that aren't yet implemented
for rSOILWAT2 or netCDF outarray output.

This just gives the main output loop something to call,
rather than an empty pointer.

@param[in] pd Time period in simulation output (day/week/month/year)
@param[in] sw Comprehensive struct of type SW_RUN containing all information
    in the simulation.
@param[in] OutDom Struct of type SW_OUT_DOM that holds output
    information that do not change throughout simulation runs
*/
void get_none_outarray_mem(OutPeriod pd, SW_RUN *sw, SW_OUT_DOM *OutDom) {
    (void) pd;
    (void) sw; // Coerce to void to silence compiler
    (void) OutDom;
}
#else
/**
@brief Output routine for quantities that aren't yet implemented
for STEPWAT2 outarray output.

This just gives the main output loop something to call,
rather than an empty pointer.

@param[in] pd Time period in simulation output (day/week/month/year)
@param[in] sw Comprehensive struct of type SW_RUN containing all information
    in the simulation.
@param[in] OutDom Struct of type SW_OUT_DOM that holds output
    information that do not change throughout simulation runs
@param[in] LogInfo Holds information on warnings and errors
*/
void get_none_outarray(
    OutPeriod pd, SW_RUN *sw, SW_OUT_DOM *OutDom, LOG_INFO *LogInfo
) {
    (void) pd;
    (void) sw; // Coerce to void to silence compiler
    (void) OutDom;
    (void) LogInfo;
}
#endif

/**
@brief Output routine for quantities that aren't yet implemented
for text output.

This just gives the main output loop something to call,
rather than an empty pointer.

@param[in] pd Time period in simulation output (day/week/month/year)
@param[in] sw Comprehensive struct of type SW_RUN containing all information
    in the simulation.
@param[in] LogInfo Holds information on warnings and errors
*/
void get_none_text(OutPeriod pd, SW_RUN *sw, LOG_INFO *LogInfo) {
    (void) pd;
    (void) sw; // Coerce to void to silence compiler
    (void) LogInfo;
}

//------ eSW_CO2Effects
#ifdef SW_OUTTEXT

/**
@brief Gets CO<SUB>2</SUB> effects by running through each vegetation type if
dealing with OUTTEXT.

@param[in] pd Time period in simulation output (day/week/month/year)
@param[in] sw Comprehensive struct of type SW_RUN containing all information
    in the simulation.
@param[out] LogInfo Holds information on warnings and errors
*/
void get_co2effects_text(OutPeriod pd, SW_RUN *sw, LOG_INFO *LogInfo) {
    int k;
    SW_OUT_RUN *OutRun = &sw->OutRun;

    char str[OUTSTRLEN];
    OutRun->sw_outstr[0] = '\0';
    char *endOutstr = OutRun->sw_outstr + sizeof OutRun->sw_outstr - 1;
    TimeInt simyear = sw->Model.simyear;
    size_t writeSize = (size_t) (MAX_LAYERS * OUTSTRLEN);
    char *writePtr = OutRun->sw_outstr;
    Bool fullBuffer = swFALSE;

    (void) pd; // hack to silence "-Wunused-parameter"

    ForEachVegType(k) {
        (void) snprintf(
            str,
            OUTSTRLEN,
            "%c%.*f",
            OUTSEP,
            OUT_DIGITS,
            sw->VegProd.veg[k].co2_multipliers[BIO_INDEX][simyear]
        );
        fullBuffer = sw_memccpy_inc(
            (void **) &writePtr, endOutstr, (void *) str, '\0', &writeSize
        );
        if (fullBuffer) {
            goto reportFullBuffer;
        }
    }
    ForEachVegType(k) {
        (void) snprintf(
            str,
            OUTSTRLEN,
            "%c%.*f",
            OUTSEP,
            OUT_DIGITS,
            sw->VegProd.veg[k].co2_multipliers[WUE_INDEX][simyear]
        );
        fullBuffer = sw_memccpy_inc(
            (void **) &writePtr, endOutstr, (void *) str, '\0', &writeSize
        );
        if (fullBuffer) {
            goto reportFullBuffer;
        }
    }

reportFullBuffer:
    if (fullBuffer) {
        reportFullBuffer(LOGERROR, LogInfo);
    }
}
#endif

#if defined(RSOILWAT) || defined(SWNETCDF)
void get_co2effects_mem(OutPeriod pd, SW_RUN *sw, SW_OUT_DOM *OutDom) {
    int k;
    size_t iOUTIndex = 0;
    SW_OUT_RUN *OutRun = &sw->OutRun;
    double *p = OutRun->p_OUT[eSW_CO2Effects][pd];

#if defined(RSOILWAT)
    get_outvalleader(
        &sw->Model, pd, OutRun->irow_OUT, OutDom->nrow_OUT, OutRun->tOffset, p
    );
#endif

    // No averaging OutRun summing required:
    ForEachVegType(k) {
#if defined(RSOILWAT)
        iOUTIndex = iOUT(
            k, OutRun->irow_OUT[pd], OutDom->nrow_OUT[pd], ncol_TimeOUT[pd]
        );

#elif defined(SWNETCDF)
        iOUTIndex = OutDom->netCDFOutput.iOUToffset[eSW_CO2Effects][pd][0] +
                    iOUTnc(
                        OutRun->irow_OUT[pd],
                        0,
                        k,
                        1,
                        OutDom->npft_OUT[eSW_CO2Effects][0]
                    );
#endif

        p[iOUTIndex] =
            sw->VegProd.veg[k].co2_multipliers[BIO_INDEX][sw->Model.simyear];


#if defined(RSOILWAT)
        iOUTIndex = iOUT(
            k + NVEGTYPES,
            OutRun->irow_OUT[pd],
            OutDom->nrow_OUT[pd],
            ncol_TimeOUT[pd]
        );

#elif defined(SWNETCDF)
        iOUTIndex = OutDom->netCDFOutput.iOUToffset[eSW_CO2Effects][pd][1] +
                    iOUTnc(
                        OutRun->irow_OUT[pd],
                        0,
                        k,
                        1,
                        OutDom->npft_OUT[eSW_CO2Effects][1]
                    );
#endif

        p[iOUTIndex] =
            sw->VegProd.veg[k].co2_multipliers[WUE_INDEX][sw->Model.simyear];
    }
}

#elif defined(STEPWAT)
void get_co2effects_agg(
    OutPeriod pd, SW_RUN *sw, SW_OUT_DOM *OutDom, LOG_INFO *LogInfo
) {
    int k;
    size_t iOUTIndex = 0;
    SW_OUT_RUN *OutRun = &sw->OutRun;

    double *p = OutRun->p_OUT[eSW_CO2Effects][pd];
    double *psd = OutRun->p_OUTsd[eSW_CO2Effects][pd];

    ForEachVegType(k) {
        iOUTIndex = iOUT(
            k, OutRun->irow_OUT[pd], OutDom->nrow_OUT[pd], ncol_TimeOUT[pd]
        );
        do_running_agg(
            p,
            psd,
            iOUTIndex,
            OutRun->currIter,
            sw->VegProd.veg[k].co2_multipliers[BIO_INDEX][sw->Model.simyear]
        );

        iOUTIndex = iOUT(
            k + NVEGTYPES,
            OutRun->irow_OUT[pd],
            OutDom->nrow_OUT[pd],
            ncol_TimeOUT[pd]
        );
        do_running_agg(
            p,
            psd,
            iOUTIndex,
            OutRun->currIter,
            sw->VegProd.veg[k].co2_multipliers[WUE_INDEX][sw->Model.simyear]
        );
    }

    if (OutDom->print_IterationSummary) {
        OutRun->sw_outstr_agg[0] = '\0';
        format_IterationSummary(
            p,
            psd,
            pd,
            OutDom->ncol_OUT[eSW_CO2Effects],
            sw,
            OutDom->nrow_OUT,
            LogInfo
        );
    }
}
#endif


//------ eSW_Biomass
#ifdef SW_OUTTEXT
void get_biomass_text(OutPeriod pd, SW_RUN *sw, LOG_INFO *LogInfo) {
    int k;
    SW_VEGPROD_OUTPUTS *vo = sw->VegProd.p_oagg[pd];
    SW_OUT_RUN *OutRun = &sw->OutRun;
    size_t writeSize = (size_t) (MAX_LAYERS * OUTSTRLEN);
    char *writePtr = OutRun->sw_outstr;
    char *endOutstr = OutRun->sw_outstr + sizeof OutRun->sw_outstr - 1;
    Bool fullBuffer = swFALSE;

    char str[OUTSTRLEN];
    OutRun->sw_outstr[0] = '\0';

    // fCover for NVEGTYPES plus bare-ground
    (void) snprintf(
        str,
        OUTSTRLEN,
        "%c%.*f",
        OUTSEP,
        OUT_DIGITS,
        sw->VegProd.bare_cov.fCover
    );
    (void) sw_memccpy_inc(
        (void **) &writePtr, endOutstr, (void *) str, '\0', &writeSize
    );
    if (fullBuffer) {
        goto reportFullBuffer;
    }

    ForEachVegType(k) {
        (void) snprintf(
            str,
            OUTSTRLEN,
            "%c%.*f",
            OUTSEP,
            OUT_DIGITS,
            sw->VegProd.veg[k].cov.fCover
        );
        (void) sw_memccpy_inc(
            (void **) &writePtr, endOutstr, (void *) str, '\0', &writeSize
        );
        if (fullBuffer) {
            goto reportFullBuffer;
        }
    }

    // biomass (g/m2 as component of total) for NVEGTYPES plus totals and litter
    (void
    ) snprintf(str, OUTSTRLEN, "%c%.*f", OUTSEP, OUT_DIGITS, vo->biomass_total);
    fullBuffer = sw_memccpy_inc(
        (void **) &writePtr, endOutstr, (void *) str, '\0', &writeSize
    );
    if (fullBuffer) {
        goto reportFullBuffer;
    }

    ForEachVegType(k) {
        (void) snprintf(
            str,
            OUTSTRLEN,
            "%c%.*f",
            OUTSEP,
            OUT_DIGITS,
            vo->veg[k].biomass_inveg
        );
        fullBuffer = sw_memccpy_inc(
            (void **) &writePtr, endOutstr, (void *) str, '\0', &writeSize
        );
        if (fullBuffer) {
            goto reportFullBuffer;
        }
    }
    (void
    ) snprintf(str, OUTSTRLEN, "%c%.*f", OUTSEP, OUT_DIGITS, vo->litter_total);
    fullBuffer = sw_memccpy_inc(
        (void **) &writePtr, endOutstr, (void *) str, '\0', &writeSize
    );
    if (fullBuffer) {
        goto reportFullBuffer;
    }

    // biolive (g/m2 as component of total) for NVEGTYPES plus totals
    (void
    ) snprintf(str, OUTSTRLEN, "%c%.*f", OUTSEP, OUT_DIGITS, vo->biolive_total);
    fullBuffer = sw_memccpy_inc(
        (void **) &writePtr, endOutstr, (void *) str, '\0', &writeSize
    );
    if (fullBuffer) {
        goto reportFullBuffer;
    }

    ForEachVegType(k) {
        (void) snprintf(
            str,
            OUTSTRLEN,
            "%c%.*f",
            OUTSEP,
            OUT_DIGITS,
            vo->veg[k].biolive_inveg
        );
        fullBuffer = sw_memccpy_inc(
            (void **) &writePtr, endOutstr, (void *) str, '\0', &writeSize
        );
        if (fullBuffer) {
            goto reportFullBuffer;
        }
    }

    // leaf area index [m2/m2]
    (void) snprintf(str, OUTSTRLEN, "%c%.*f", OUTSEP, OUT_DIGITS, vo->LAI);
    fullBuffer = sw_memccpy_inc(
        (void **) &writePtr, endOutstr, (void *) str, '\0', &writeSize
    );

reportFullBuffer:
    if (fullBuffer) {
        reportFullBuffer(LOGERROR, LogInfo);
    }
}
#endif

#if defined(RSOILWAT) || defined(SWNETCDF)
void get_biomass_mem(OutPeriod pd, SW_RUN *sw, SW_OUT_DOM *OutDom) {
    int k;
    SW_VEGPROD_OUTPUTS *vo = sw->VegProd.p_oagg[pd];
    size_t iOUTIndex = 0;
    SW_OUT_RUN *OutRun = &sw->OutRun;

    double *p = OutRun->p_OUT[eSW_Biomass][pd];

#if defined(RSOILWAT)
    int i;
    get_outvalleader(
        &sw->Model, pd, OutRun->irow_OUT, OutDom->nrow_OUT, OutRun->tOffset, p
    );
#endif

// fCover of bare-ground
#if defined(RSOILWAT)
    iOUTIndex =
        iOUT(0, OutRun->irow_OUT[pd], OutDom->nrow_OUT[pd], ncol_TimeOUT[pd]);

#elif defined(SWNETCDF)
    iOUTIndex = OutDom->netCDFOutput.iOUToffset[eSW_Biomass][pd][0] +
                iOUTnc(OutRun->irow_OUT[pd], 0, 0, 1, 1);
#endif

    p[iOUTIndex] = sw->VegProd.bare_cov.fCover;


// fCover for NVEGTYPES
#if defined(RSOILWAT)
    i = 1;
#endif

    ForEachVegType(k) {
#if defined(RSOILWAT)
        iOUTIndex = iOUT(
            i + k, OutRun->irow_OUT[pd], OutDom->nrow_OUT[pd], ncol_TimeOUT[pd]
        );

#elif defined(SWNETCDF)
        iOUTIndex =
            OutDom->netCDFOutput.iOUToffset[eSW_Biomass][pd][1] +
            iOUTnc(
                OutRun->irow_OUT[pd], 0, k, 1, OutDom->npft_OUT[eSW_Biomass][1]
            );
#endif

        p[iOUTIndex] = sw->VegProd.veg[k].cov.fCover;
    }

// biomass (g/m2 as component of total) totals
#if defined(RSOILWAT)
    iOUTIndex = iOUT(
        i + NVEGTYPES,
        OutRun->irow_OUT[pd],
        OutDom->nrow_OUT[pd],
        ncol_TimeOUT[pd]
    );

#elif defined(SWNETCDF)
    iOUTIndex = OutDom->netCDFOutput.iOUToffset[eSW_Biomass][pd][2] +
                iOUTnc(OutRun->irow_OUT[pd], 0, 0, 1, 1);
#endif

    p[iOUTIndex] = vo->biomass_total;


// biomass (g/m2 as component of total) for NVEGTYPES
#if defined(RSOILWAT)
    i += NVEGTYPES + 1;
#endif

    ForEachVegType(k) {
#if defined(RSOILWAT)
        iOUTIndex = iOUT(
            i + k, OutRun->irow_OUT[pd], OutDom->nrow_OUT[pd], ncol_TimeOUT[pd]
        );

#elif defined(SWNETCDF)
        iOUTIndex =
            OutDom->netCDFOutput.iOUToffset[eSW_Biomass][pd][3] +
            iOUTnc(
                OutRun->irow_OUT[pd], 0, k, 1, OutDom->npft_OUT[eSW_Biomass][3]
            );
#endif

        p[iOUTIndex] = vo->veg[k].biomass_inveg;
    }


// biomass (g/m2 as component of total) of litter
#if defined(RSOILWAT)
    iOUTIndex = iOUT(
        i + NVEGTYPES,
        OutRun->irow_OUT[pd],
        OutDom->nrow_OUT[pd],
        ncol_TimeOUT[pd]
    );

#elif defined(SWNETCDF)
    iOUTIndex = OutDom->netCDFOutput.iOUToffset[eSW_Biomass][pd][4] +
                iOUTnc(OutRun->irow_OUT[pd], 0, 0, 1, 1);
#endif

    p[iOUTIndex] = vo->litter_total;


// biolive (g/m2 as component of total) total
#if defined(RSOILWAT)
    iOUTIndex = iOUT(
        i + NVEGTYPES + 1,
        OutRun->irow_OUT[pd],
        OutDom->nrow_OUT[pd],
        ncol_TimeOUT[pd]
    );

#elif defined(SWNETCDF)
    iOUTIndex = OutDom->netCDFOutput.iOUToffset[eSW_Biomass][pd][5] +
                iOUTnc(OutRun->irow_OUT[pd], 0, 0, 1, 1);
#endif

    p[iOUTIndex] = vo->biolive_total;


// biolive (g/m2 as component of total) for NVEGTYPES
#if defined(RSOILWAT)
    i += NVEGTYPES + 2;
#endif

    ForEachVegType(k) {
#if defined(RSOILWAT)
        iOUTIndex = iOUT(
            i + k, OutRun->irow_OUT[pd], OutDom->nrow_OUT[pd], ncol_TimeOUT[pd]
        );

#elif defined(SWNETCDF)
        iOUTIndex =
            OutDom->netCDFOutput.iOUToffset[eSW_Biomass][pd][6] +
            iOUTnc(
                OutRun->irow_OUT[pd], 0, k, 1, OutDom->npft_OUT[eSW_Biomass][6]
            );
#endif

        p[iOUTIndex] = vo->veg[k].biolive_inveg;
    }


// leaf area index [m2/m2]
#if defined(RSOILWAT)
    iOUTIndex = iOUT(
        i + NVEGTYPES,
        OutRun->irow_OUT[pd],
        OutDom->nrow_OUT[pd],
        ncol_TimeOUT[pd]
    );

#elif defined(SWNETCDF)
    iOUTIndex = OutDom->netCDFOutput.iOUToffset[eSW_Biomass][pd][7] +
                iOUTnc(OutRun->irow_OUT[pd], 0, 0, 1, 1);
#endif

    p[iOUTIndex] = vo->LAI;
}

#elif defined(STEPWAT)
void get_biomass_agg(
    OutPeriod pd, SW_RUN *sw, SW_OUT_DOM *OutDom, LOG_INFO *LogInfo
) {
    int k;
    int i;
    SW_VEGPROD_OUTPUTS *vo = sw->VegProd.p_oagg[pd];
    size_t iOUTIndex = 0;
    SW_OUT_RUN *OutRun = &sw->OutRun;

    double *p = OutRun->p_OUT[eSW_Biomass][pd];
    double *psd = OutRun->p_OUTsd[eSW_Biomass][pd];

    // fCover for NVEGTYPES plus bare-ground
    iOUTIndex =
        iOUT(0, OutRun->irow_OUT[pd], OutDom->nrow_OUT[pd], ncol_TimeOUT[pd]);
    do_running_agg(
        p, psd, iOUTIndex, OutRun->currIter, sw->VegProd.bare_cov.fCover
    );

    i = 1;
    ForEachVegType(k) {
        iOUTIndex = iOUT(
            i + k, OutRun->irow_OUT[pd], OutDom->nrow_OUT[pd], ncol_TimeOUT[pd]
        );
        do_running_agg(
            p, psd, iOUTIndex, OutRun->currIter, sw->VegProd.veg[k].cov.fCover
        );
    }

    // biomass (g/m2 as component of total) for NVEGTYPES plus totals and litter
    iOUTIndex = iOUT(
        i + NVEGTYPES,
        OutRun->irow_OUT[pd],
        OutDom->nrow_OUT[pd],
        ncol_TimeOUT[pd]
    );
    do_running_agg(p, psd, iOUTIndex, OutRun->currIter, vo->biomass_total);
    i += NVEGTYPES + 1;

    ForEachVegType(k) {
        iOUTIndex = iOUT(
            i + k, OutRun->irow_OUT[pd], OutDom->nrow_OUT[pd], ncol_TimeOUT[pd]
        );
        do_running_agg(
            p, psd, iOUTIndex, OutRun->currIter, vo->veg[k].biomass_inveg
        );
    }

    iOUTIndex = iOUT(
        i + NVEGTYPES,
        OutRun->irow_OUT[pd],
        OutDom->nrow_OUT[pd],
        ncol_TimeOUT[pd]
    );
    do_running_agg(p, psd, iOUTIndex, OutRun->currIter, vo->litter_total);

    // biolive (g/m2 as component of total) for NVEGTYPES plus totals
    iOUTIndex = iOUT(
        i + NVEGTYPES + 1,
        OutRun->irow_OUT[pd],
        OutDom->nrow_OUT[pd],
        ncol_TimeOUT[pd]
    );
    do_running_agg(p, psd, iOUTIndex, OutRun->currIter, vo->biolive_total);
    i += NVEGTYPES + 2;
    ForEachVegType(k) {
        iOUTIndex = iOUT(
            i + k, OutRun->irow_OUT[pd], OutDom->nrow_OUT[pd], ncol_TimeOUT[pd]
        );
        do_running_agg(
            p, psd, iOUTIndex, OutRun->currIter, vo->veg[k].biolive_inveg
        );
    }

    // leaf area index [m2/m2]
    iOUTIndex = iOUT(
        i + NVEGTYPES,
        OutRun->irow_OUT[pd],
        OutDom->nrow_OUT[pd],
        ncol_TimeOUT[pd]
    );
    do_running_agg(p, psd, iOUTIndex, OutRun->currIter, vo->LAI);

    if (OutDom->print_IterationSummary) {
        OutRun->sw_outstr_agg[0] = '\0';
        format_IterationSummary(
            p,
            psd,
            pd,
            OutDom->ncol_OUT[eSW_Biomass],
            sw,
            OutDom->nrow_OUT,
            LogInfo
        );
    }
}
#endif


//------ eSW_Estab
/* --------------------------------------------------- */

#ifdef SW_OUTTEXT

/**
@brief The establishment check produces, for each species in the given set,
a day of year >= 0 that the species established itself in the current year.

The output will be a single row of numbers for each year.
Each column represents a species in order it was entered in the stabs.in file.
The value will be the day that the species established, OutRun - if it didn't
establish this year.  This check is for OUTTEXT.

@param[in] pd Time period in simulation output (day/week/month/year)
@param[in] sw Comprehensive struct of type SW_RUN containing all information
    in the simulation.
@param[out] LogInfo Holds information on warnings and errors
*/
void get_estab_text(OutPeriod pd, SW_RUN *sw, LOG_INFO *LogInfo) {
    IntU i;
    SW_OUT_RUN *OutRun = &sw->OutRun;
    size_t writeSize = (size_t) (MAX_LAYERS * OUTSTRLEN);
    char *writePtr = OutRun->sw_outstr;
    char *endOutstr = OutRun->sw_outstr + sizeof OutRun->sw_outstr - 1;
    Bool fullBuffer = swFALSE;

    char str[OUTSTRLEN];
    OutRun->sw_outstr[0] = '\0';

    (void) pd; // silence `-Wunused-parameter`

    for (i = 0; i < sw->VegEstab.count; i++) {
        (void) snprintf(
            str, OUTSTRLEN, "%c%d", OUTSEP, sw->VegEstab.parms[i]->estab_doy
        );
        fullBuffer = sw_memccpy_inc(
            (void **) &writePtr, endOutstr, (void *) str, '\0', &writeSize
        );
        if (fullBuffer) {
            goto reportFullBuffer;
        }
    }

reportFullBuffer:
    if (fullBuffer) {
        reportFullBuffer(LOGERROR, LogInfo);
    }
}
#endif

#if defined(RSOILWAT) || defined(SWNETCDF)
/**
@brief The establishment check produces, for each species in the given set,
a day of year >= 0 that the species established itself in the current year.

The output will be a single row of numbers for each year.
Each column represents a species in order it was entered in the stabs.in file.
The value will be the day that the species established, OutRun - if it didn't
establish this year.  This check is for RSOILWAT.

@param[in] pd Time period in simulation output (day/week/month/year)
@param[in] sw Comprehensive struct of type SW_RUN containing all information
    in the simulation.
@param[in] OutDom Struct of type SW_OUT_DOM that holds output
    information that do not change throughout simulation runs
*/
void get_estab_mem(OutPeriod pd, SW_RUN *sw, SW_OUT_DOM *OutDom) {
    IntU i;
    size_t iOUTIndex = 0;
    SW_OUT_RUN *OutRun = &sw->OutRun;

    double *p = OutRun->p_OUT[eSW_Estab][pd];

#if defined(RSOILWAT)
    get_outvalleader(
        &sw->Model, pd, OutRun->irow_OUT, OutDom->nrow_OUT, OutRun->tOffset, p
    );
#endif

    for (i = 0; i < sw->VegEstab.count; i++) {
#if defined(RSOILWAT)
        iOUTIndex = iOUT(
            i, OutRun->irow_OUT[pd], OutDom->nrow_OUT[pd], ncol_TimeOUT[pd]
        );

#elif defined(SWNETCDF)
        iOUTIndex = OutDom->netCDFOutput.iOUToffset[eSW_Estab][pd][i] +
                    iOUTnc(OutRun->irow_OUT[pd], 0, 0, 1, 1);
#endif

        p[iOUTIndex] = sw->VegEstab.parms[i]->estab_doy;
    }
}

#elif defined(STEPWAT)
/**
@brief The establishment check produces, for each species in the given set,
a day of year >= 0 that the species established itself in the current year.

The output will be a single row of numbers for each year.
Each column represents a species in order it was entered in the stabs.in file.
The value will be the day that the species established, OutRun - if it didn't
establish this year.  This check is for STEPWAT.

@param[in] pd Time period in simulation output (day/week/month/year)
@param[in] sw Comprehensive struct of type SW_RUN containing all information
    in the simulation.
@param[in] OutDom Struct of type SW_OUT_DOM that holds output
    information that do not change throughout simulation runs
@param[out] LogInfo Holds information on warnings and errors
*/
void get_estab_agg(
    OutPeriod pd, SW_RUN *sw, SW_OUT_DOM *OutDom, LOG_INFO *LogInfo
) {
    IntU i;
    size_t iOUTIndex = 0;
    SW_OUT_RUN *OutRun = &sw->OutRun;

    double *p = OutRun->p_OUT[eSW_Estab][pd];
    double *psd = OutRun->p_OUTsd[eSW_Estab][pd];

    for (i = 0; i < sw->VegEstab.count; i++) {
        iOUTIndex = iOUT(
            i, OutRun->irow_OUT[pd], OutDom->nrow_OUT[pd], ncol_TimeOUT[pd]
        );
        do_running_agg(
            p,
            psd,
            iOUTIndex,
            OutRun->currIter,
            sw->VegEstab.parms[i]->estab_doy
        );
    }

    if (OutDom->print_IterationSummary) {
        OutRun->sw_outstr_agg[0] = '\0';
        format_IterationSummary(
            p,
            psd,
            pd,
            OutDom->ncol_OUT[eSW_Estab],
            sw,
            OutDom->nrow_OUT,
            LogInfo
        );
    }
}
#endif


//------ eSW_Temp
#ifdef SW_OUTTEXT

/**
@brief Gets temp text from SW_WEATHER_OUTPUTS when dealing with OUTTEXT.

@param[in] pd Time period in simulation output (day/week/month/year)
@param[in] sw Comprehensive struct of type SW_RUN containing all information
  in the simulation
@param[out] LogInfo Holds information on warnings and errors
*/
void get_temp_text(OutPeriod pd, SW_RUN *sw, LOG_INFO *LogInfo) {
    SW_WEATHER_OUTPUTS *vo = sw->Weather.p_oagg[pd];
    SW_OUT_RUN *OutRun = &sw->OutRun;

    OutRun->sw_outstr[0] = '\0';
    (void) snprintf(
        OutRun->sw_outstr,
        sizeof OutRun->sw_outstr,
        "%c%.*f%c%.*f%c%.*f%c%.*f%c%.*f%c%.*f",
        OUTSEP,
        OUT_DIGITS,
        vo->temp_max,
        OUTSEP,
        OUT_DIGITS,
        vo->temp_min,
        OUTSEP,
        OUT_DIGITS,
        vo->temp_avg,
        OUTSEP,
        OUT_DIGITS,
        vo->surfaceMax,
        OUTSEP,
        OUT_DIGITS,
        vo->surfaceMin,
        OUTSEP,
        OUT_DIGITS,
        vo->surfaceAvg
    );

    (void) LogInfo;
}
#endif

#if defined(RSOILWAT) || defined(SWNETCDF)

/**
@brief Gets temp text from SW_WEATHER_OUTPUTS when dealing with RSOILWAT.

@param pd Period.
@param[in] sw Comprehensive struct of type SW_RUN containing all information
    in the simulation
@param[in] OutDom Struct of type SW_OUT_DOM that holds output
    information that do not change throughout simulation runs
*/
void get_temp_mem(OutPeriod pd, SW_RUN *sw, SW_OUT_DOM *OutDom) {
    SW_WEATHER_OUTPUTS *vo = sw->Weather.p_oagg[pd];
    size_t iOUTIndex = 0;
    SW_OUT_RUN *OutRun = &sw->OutRun;

    double *p = OutRun->p_OUT[eSW_Temp][pd];

#if defined(RSOILWAT)
    get_outvalleader(
        &sw->Model, pd, OutRun->irow_OUT, OutDom->nrow_OUT, OutRun->tOffset, p
    );
#endif

#if defined(RSOILWAT)
    iOUTIndex =
        iOUT(0, OutRun->irow_OUT[pd], OutDom->nrow_OUT[pd], ncol_TimeOUT[pd]);

#elif defined(SWNETCDF)
    iOUTIndex = OutDom->netCDFOutput.iOUToffset[eSW_Temp][pd][0] +
                iOUTnc(OutRun->irow_OUT[pd], 0, 0, 1, 1);
#endif

    p[iOUTIndex] = vo->temp_max;


#if defined(RSOILWAT)
    iOUTIndex =
        iOUT(1, OutRun->irow_OUT[pd], OutDom->nrow_OUT[pd], ncol_TimeOUT[pd]);

#elif defined(SWNETCDF)
    iOUTIndex = OutDom->netCDFOutput.iOUToffset[eSW_Temp][pd][1] +
                iOUTnc(OutRun->irow_OUT[pd], 0, 0, 1, 1);
#endif

    p[iOUTIndex] = vo->temp_min;


#if defined(RSOILWAT)
    iOUTIndex =
        iOUT(2, OutRun->irow_OUT[pd], OutDom->nrow_OUT[pd], ncol_TimeOUT[pd]);

#elif defined(SWNETCDF)
    iOUTIndex = OutDom->netCDFOutput.iOUToffset[eSW_Temp][pd][2] +
                iOUTnc(OutRun->irow_OUT[pd], 0, 0, 1, 1);
#endif

    p[iOUTIndex] = vo->temp_avg;


#if defined(RSOILWAT)
    iOUTIndex =
        iOUT(3, OutRun->irow_OUT[pd], OutDom->nrow_OUT[pd], ncol_TimeOUT[pd]);

#elif defined(SWNETCDF)
    iOUTIndex = OutDom->netCDFOutput.iOUToffset[eSW_Temp][pd][3] +
                iOUTnc(OutRun->irow_OUT[pd], 0, 0, 1, 1);
#endif

    p[iOUTIndex] = vo->surfaceMax;


#if defined(RSOILWAT)
    iOUTIndex =
        iOUT(4, OutRun->irow_OUT[pd], OutDom->nrow_OUT[pd], ncol_TimeOUT[pd]);

#elif defined(SWNETCDF)
    iOUTIndex = OutDom->netCDFOutput.iOUToffset[eSW_Temp][pd][4] +
                iOUTnc(OutRun->irow_OUT[pd], 0, 0, 1, 1);
#endif

    p[iOUTIndex] = vo->surfaceMin;


#if defined(RSOILWAT)
    iOUTIndex =
        iOUT(5, OutRun->irow_OUT[pd], OutDom->nrow_OUT[pd], ncol_TimeOUT[pd]);

#elif defined(SWNETCDF)
    iOUTIndex = OutDom->netCDFOutput.iOUToffset[eSW_Temp][pd][5] +
                iOUTnc(OutRun->irow_OUT[pd], 0, 0, 1, 1);
#endif

    p[iOUTIndex] = vo->surfaceAvg;
}

#elif defined(STEPWAT)

/**
@brief Gets temp text from SW_WEATHER_OUTPUTS when dealing with STEPWAT.

@param[in] pd Time period in simulation output (day/week/month/year)
@param[in] sw Comprehensive struct of type SW_RUN containing all information
    in the simulation.
@param[in] OutDom Struct of type SW_OUT_DOM that holds output
    information that do not change throughout simulation runs
@param[out] LogInfo Holds information on warnings and errors
*/
void get_temp_agg(
    OutPeriod pd, SW_RUN *sw, SW_OUT_DOM *OutDom, LOG_INFO *LogInfo
) {
    SW_WEATHER_OUTPUTS *vo = sw->Weather.p_oagg[pd];
    size_t iOUTIndex = 0;
    SW_OUT_RUN *OutRun = &sw->OutRun;

    double *p = OutRun->p_OUT[eSW_Temp][pd];
    double *psd = OutRun->p_OUTsd[eSW_Temp][pd];

    iOUTIndex =
        iOUT(0, OutRun->irow_OUT[pd], OutDom->nrow_OUT[pd], ncol_TimeOUT[pd]);
    do_running_agg(p, psd, iOUTIndex, OutRun->currIter, vo->temp_max);

    iOUTIndex =
        iOUT(1, OutRun->irow_OUT[pd], OutDom->nrow_OUT[pd], ncol_TimeOUT[pd]);
    do_running_agg(p, psd, iOUTIndex, OutRun->currIter, vo->temp_min);

    iOUTIndex =
        iOUT(2, OutRun->irow_OUT[pd], OutDom->nrow_OUT[pd], ncol_TimeOUT[pd]);
    do_running_agg(p, psd, iOUTIndex, OutRun->currIter, vo->temp_avg);

    iOUTIndex =
        iOUT(3, OutRun->irow_OUT[pd], OutDom->nrow_OUT[pd], ncol_TimeOUT[pd]);
    do_running_agg(p, psd, iOUTIndex, OutRun->currIter, vo->surfaceMax);

    iOUTIndex =
        iOUT(4, OutRun->irow_OUT[pd], OutDom->nrow_OUT[pd], ncol_TimeOUT[pd]);
    do_running_agg(p, psd, iOUTIndex, OutRun->currIter, vo->surfaceMin);

    iOUTIndex =
        iOUT(5, OutRun->irow_OUT[pd], OutDom->nrow_OUT[pd], ncol_TimeOUT[pd]);
    do_running_agg(p, psd, iOUTIndex, OutRun->currIter, vo->surfaceAvg);

    if (OutDom->print_IterationSummary) {
        OutRun->sw_outstr_agg[0] = '\0';
        format_IterationSummary(
            p,
            psd,
            pd,
            OutDom->ncol_OUT[eSW_Temp],
            sw,
            OutDom->nrow_OUT,
            LogInfo
        );
    }
}

/**
@brief STEPWAT2 expects annual mean air temperature

@param[in] pd Time period in simulation output (day/week/month/year)
@param[in] sw Comprehensive struct of type SW_RUN containing all information
    in the simulation.
@param[in] OutDom Struct of type SW_OUT_DOM that holds output
    information that do not change throughout simulation runs
@param[out] LogInfo Holds information on warnings and errors
*/
void get_temp_SXW(
    OutPeriod pd, SW_RUN *sw, SW_OUT_DOM *OutDom, LOG_INFO *LogInfo
) {
    TimeInt tOffset;

    if (pd == eSW_Month || pd == eSW_Year) {
        SW_WEATHER_OUTPUTS *vo = sw->Weather.p_oagg[pd];
        SW_OUT_RUN *OutRun = &sw->OutRun;
        tOffset = OutRun->tOffset;

        if (pd == eSW_Month) {
            OutRun->temp_monthly[sw->Model.month - tOffset] = vo->temp_avg;
        } else if (pd == eSW_Year) {
            OutRun->temp = vo->temp_avg;
        }
    }

    (void) OutDom;
    (void) LogInfo;
}
#endif


//------ eSW_Precip
#ifdef SW_OUTTEXT

/**
@brief Gets precipitation text from SW_WEATHER_OUTPUTS when dealing with
OUTTEXT.

@param[in] pd Time period in simulation output (day/week/month/year)
@param[in] sw Comprehensive struct of type SW_RUN containing all information
    in the simulation.
@param[in] OutDom Struct of type SW_OUT_DOM that holds output
    information that do not change throughout simulation runs
@param[out] LogInfo Holds information on warnings and errors
*/
void get_precip_text(OutPeriod pd, SW_RUN *sw, LOG_INFO *LogInfo) {
    SW_WEATHER_OUTPUTS *vo = sw->Weather.p_oagg[pd];
    SW_OUT_RUN *OutRun = &sw->OutRun;

    OutRun->sw_outstr[0] = '\0';
    (void) snprintf(
        OutRun->sw_outstr,
        sizeof OutRun->sw_outstr,
        "%c%.*f%c%.*f%c%.*f%c%.*f%c%.*f",
        OUTSEP,
        OUT_DIGITS,
        vo->ppt,
        OUTSEP,
        OUT_DIGITS,
        vo->rain,
        OUTSEP,
        OUT_DIGITS,
        vo->snow,
        OUTSEP,
        OUT_DIGITS,
        vo->snowmelt,
        OUTSEP,
        OUT_DIGITS,
        vo->snowloss
    );

    (void) LogInfo;
}
#endif

#if defined(RSOILWAT) || defined(SWNETCDF)

/**
@brief Gets precipitation text from SW_WEATHER_OUTPUTS when dealing with
RSOILWAT.

@param[in] pd Time period in simulation output (day/week/month/year)
@param[in] sw Comprehensive struct of type SW_RUN containing all information
    in the simulation.
@param[in] OutDom Struct of type SW_OUT_DOM that holds output
    information that do not change throughout simulation runs
*/
void get_precip_mem(OutPeriod pd, SW_RUN *sw, SW_OUT_DOM *OutDom) {
    SW_WEATHER_OUTPUTS *vo = sw->Weather.p_oagg[pd];
    size_t iOUTIndex = 0;
    SW_OUT_RUN *OutRun = &sw->OutRun;

    double *p = OutRun->p_OUT[eSW_Precip][pd];

#if defined(RSOILWAT)
    get_outvalleader(
        &sw->Model, pd, OutRun->irow_OUT, OutDom->nrow_OUT, OutRun->tOffset, p
    );
#endif

#if defined(RSOILWAT)
    iOUTIndex =
        iOUT(0, OutRun->irow_OUT[pd], OutDom->nrow_OUT[pd], ncol_TimeOUT[pd]);

#elif defined(SWNETCDF)
    iOUTIndex = OutDom->netCDFOutput.iOUToffset[eSW_Precip][pd][0] +
                iOUTnc(OutRun->irow_OUT[pd], 0, 0, 1, 1);
#endif

    p[iOUTIndex] = vo->ppt;


#if defined(RSOILWAT)
    iOUTIndex =
        iOUT(1, OutRun->irow_OUT[pd], OutDom->nrow_OUT[pd], ncol_TimeOUT[pd]);

#elif defined(SWNETCDF)
    iOUTIndex = OutDom->netCDFOutput.iOUToffset[eSW_Precip][pd][1] +
                iOUTnc(OutRun->irow_OUT[pd], 0, 0, 1, 1);
#endif

    p[iOUTIndex] = vo->rain;


#if defined(RSOILWAT)
    iOUTIndex =
        iOUT(2, OutRun->irow_OUT[pd], OutDom->nrow_OUT[pd], ncol_TimeOUT[pd]);

#elif defined(SWNETCDF)
    iOUTIndex = OutDom->netCDFOutput.iOUToffset[eSW_Precip][pd][2] +
                iOUTnc(OutRun->irow_OUT[pd], 0, 0, 1, 1);
#endif

    p[iOUTIndex] = vo->snow;


#if defined(RSOILWAT)
    iOUTIndex =
        iOUT(3, OutRun->irow_OUT[pd], OutDom->nrow_OUT[pd], ncol_TimeOUT[pd]);

#elif defined(SWNETCDF)
    iOUTIndex = OutDom->netCDFOutput.iOUToffset[eSW_Precip][pd][3] +
                iOUTnc(OutRun->irow_OUT[pd], 0, 0, 1, 1);
#endif

    p[iOUTIndex] = vo->snowmelt;


#if defined(RSOILWAT)
    iOUTIndex =
        iOUT(4, OutRun->irow_OUT[pd], OutDom->nrow_OUT[pd], ncol_TimeOUT[pd]);

#elif defined(SWNETCDF)
    iOUTIndex = OutDom->netCDFOutput.iOUToffset[eSW_Precip][pd][4] +
                iOUTnc(OutRun->irow_OUT[pd], 0, 0, 1, 1);
#endif

    p[iOUTIndex] = vo->snowloss;
}

#elif defined(STEPWAT)

/**
@brief Gets precipitation text from SW_WEATHER_OUTPUTS when dealing with
STEPWAT.

@param[in] pd Time period in simulation output (day/week/month/year)
@param[in] sw Comprehensive struct of type SW_RUN containing all information
    in the simulation.
@param[in] OutDom Struct of type SW_OUT_DOM that holds output
    information that do not change throughout simulation runs
@param[out] LogInfo Holds information on warnings and errors
*/
void get_precip_agg(
    OutPeriod pd, SW_RUN *sw, SW_OUT_DOM *OutDom, LOG_INFO *LogInfo
) {
    SW_WEATHER_OUTPUTS *vo = sw->Weather.p_oagg[pd];
    size_t iOUTIndex = 0;
    SW_OUT_RUN *OutRun = &sw->OutRun;

    double *p = OutRun->p_OUT[eSW_Precip][pd];
    double *psd = OutRun->p_OUTsd[eSW_Precip][pd];

    iOUTIndex =
        iOUT(0, OutRun->irow_OUT[pd], OutDom->nrow_OUT[pd], ncol_TimeOUT[pd]);
    do_running_agg(p, psd, iOUTIndex, OutRun->currIter, vo->ppt);

    iOUTIndex =
        iOUT(1, OutRun->irow_OUT[pd], OutDom->nrow_OUT[pd], ncol_TimeOUT[pd]);
    do_running_agg(p, psd, iOUTIndex, OutRun->currIter, vo->rain);

    iOUTIndex =
        iOUT(2, OutRun->irow_OUT[pd], OutDom->nrow_OUT[pd], ncol_TimeOUT[pd]);
    do_running_agg(p, psd, iOUTIndex, OutRun->currIter, vo->snow);

    iOUTIndex =
        iOUT(3, OutRun->irow_OUT[pd], OutDom->nrow_OUT[pd], ncol_TimeOUT[pd]);
    do_running_agg(p, psd, iOUTIndex, OutRun->currIter, vo->snowmelt);

    iOUTIndex =
        iOUT(4, OutRun->irow_OUT[pd], OutDom->nrow_OUT[pd], ncol_TimeOUT[pd]);
    do_running_agg(p, psd, iOUTIndex, OutRun->currIter, vo->snowloss);

    if (OutDom->print_IterationSummary) {
        OutRun->sw_outstr_agg[0] = '\0';
        format_IterationSummary(
            p,
            psd,
            pd,
            OutDom->ncol_OUT[eSW_Precip],
            sw,
            OutDom->nrow_OUT,
            LogInfo
        );
    }
}

/**
@brief STEPWAT2 expects monthly and annual sum of precipitation

@param[in] pd Time period in simulation output (day/week/month/year)
@param[in] sw Comprehensive struct of type SW_RUN containing all information
    in the simulation.
@param[in] OutDom Struct of type SW_OUT_DOM that holds output
    information that do not change throughout simulation runs
@param[out] LogInfo Holds information on warnings and errors
*/
void get_precip_SXW(
    OutPeriod pd, SW_RUN *sw, SW_OUT_DOM *OutDom, LOG_INFO *LogInfo
) {
    TimeInt tOffset;

    if (pd == eSW_Month || pd == eSW_Year) {
        SW_WEATHER_OUTPUTS *vo = sw->Weather.p_oagg[pd];
        SW_OUT_RUN *OutRun = &sw->OutRun;
        tOffset = OutRun->tOffset;

        if (pd == eSW_Month) {
            OutRun->ppt_monthly[sw->Model.month - tOffset] = vo->ppt;
        } else if (pd == eSW_Year) {
            OutRun->ppt = vo->ppt;
        }
    }

    (void) OutDom;
    (void) LogInfo;
}
#endif

//------ eSW_VWCBulk
#ifdef SW_OUTTEXT

/**
@brief Gets vwcBulk text from SW_SOILWAT_OUTPUTS when dealing with OUTTEXT.

@param[in] pd Time period in simulation output (day/week/month/year)
@param[in] sw Comprehensive struct of type SW_RUN containing all information
    in the simulation.
@param[out] LogInfo Holds information on warnings and errors
*/
void get_vwcBulk_text(OutPeriod pd, SW_RUN *sw, LOG_INFO *LogInfo) {
    LyrIndex i;
    SW_SOILWAT_OUTPUTS *vo = sw->SoilWat.p_oagg[pd];
    SW_OUT_RUN *OutRun = &sw->OutRun;
    size_t writeSize = (size_t) (MAX_LAYERS * OUTSTRLEN);
    char *writePtr = OutRun->sw_outstr;
    char *endOutstr = OutRun->sw_outstr + sizeof OutRun->sw_outstr - 1;
    Bool fullBuffer = swFALSE;

    char str[OUTSTRLEN];
    OutRun->sw_outstr[0] = '\0';

    ForEachSoilLayer(i, sw->Site.n_layers) {
        /* vwcBulk at this point is identical to swcBulk */
        (void) snprintf(
            str,
            OUTSTRLEN,
            "%c%.*f",
            OUTSEP,
            OUT_DIGITS,
            vo->vwcBulk[i] / sw->Site.soils.width[i]
        );
        fullBuffer = sw_memccpy_inc(
            (void **) &writePtr, endOutstr, (void *) str, '\0', &writeSize
        );
        if (fullBuffer) {
            goto reportFullBuffer;
        }
    }

reportFullBuffer:
    if (fullBuffer) {
        reportFullBuffer(LOGERROR, LogInfo);
    }
}
#endif

#if defined(RSOILWAT) || defined(SWNETCDF)

/**
@brief Gets vwcBulk text from SW_SOILWAT_OUTPUTS when dealing with RSOILWAT.

@param[in] pd Time period in simulation output (day/week/month/year)
@param[in] sw Comprehensive struct of type SW_RUN containing all information
    in the simulation.
@param[in] OutDom Struct of type SW_OUT_DOM that holds output
    information that do not change throughout simulation runs
*/
void get_vwcBulk_mem(OutPeriod pd, SW_RUN *sw, SW_OUT_DOM *OutDom) {
    LyrIndex i;
    SW_SOILWAT_OUTPUTS *vo = sw->SoilWat.p_oagg[pd];
    size_t iOUTIndex = 0;
    SW_OUT_RUN *OutRun = &sw->OutRun;

    double *p = OutRun->p_OUT[eSW_VWCBulk][pd];

#if defined(RSOILWAT)
    get_outvalleader(
        &sw->Model, pd, OutRun->irow_OUT, OutDom->nrow_OUT, OutRun->tOffset, p
    );
#endif

    ForEachSoilLayer(i, sw->Site.n_layers) {
#if defined(RSOILWAT)
        iOUTIndex = iOUT(
            i, OutRun->irow_OUT[pd], OutDom->nrow_OUT[pd], ncol_TimeOUT[pd]
        );

#elif defined(SWNETCDF)
        iOUTIndex =
            OutDom->netCDFOutput.iOUToffset[eSW_VWCBulk][pd][0] +
            iOUTnc(
                OutRun->irow_OUT[pd], i, 0, OutDom->nsl_OUT[eSW_VWCBulk][0], 1
            );
#endif

        /* vwcBulk at this point is identical to swcBulk */
        p[iOUTIndex] = vo->vwcBulk[i] / sw->Site.soils.width[i];
    }

#if defined(SWNETCDF)
    /* Set extra soil layers to missing/fill value (up to domain-wide max) */
    for (i = sw->Site.n_layers; i < OutDom->nsl_OUT[eSW_VWCBulk][0]; i++) {
        iOUTIndex =
            OutDom->netCDFOutput.iOUToffset[eSW_VWCBulk][pd][0] +
            iOUTnc(
                OutRun->irow_OUT[pd], i, 0, OutDom->nsl_OUT[eSW_VWCBulk][0], 1
            );
        p[iOUTIndex] = NC_FILL_DOUBLE;
    }
#endif // SWNETCDF
}

#elif defined(STEPWAT)

/**
@brief Gets vwcBulk text from SW_SOILWAT_OUTPUTS when dealing with STEPWAT.

@param[in] pd Time period in simulation output (day/week/month/year)
@param[in] sw Comprehensive struct of type SW_RUN containing all information
    in the simulation.
@param[in] OutDom Struct of type SW_OUT_DOM that holds output
    information that do not change throughout simulation runs
@param[out] LogInfo Holds information on warnings and errors
*/
void get_vwcBulk_agg(
    OutPeriod pd, SW_RUN *sw, SW_OUT_DOM *OutDom, LOG_INFO *LogInfo
) {
    LyrIndex i;
    SW_SOILWAT_OUTPUTS *vo = sw->SoilWat.p_oagg[pd];
    size_t iOUTIndex = 0;
    SW_OUT_RUN *OutRun = &sw->OutRun;

    double *p = OutRun->p_OUT[eSW_VWCBulk][pd];
    double *psd = OutRun->p_OUTsd[eSW_VWCBulk][pd];

    ForEachSoilLayer(i, sw->Site.n_layers) {
        /* vwcBulk at this point is identical to swcBulk */
        iOUTIndex = iOUT(
            i, OutRun->irow_OUT[pd], OutDom->nrow_OUT[pd], ncol_TimeOUT[pd]
        );
        do_running_agg(
            p,
            psd,
            iOUTIndex,
            OutRun->currIter,
            vo->vwcBulk[i] / sw->Site.soils.width[i]
        );
    }

    if (OutDom->print_IterationSummary) {
        OutRun->sw_outstr_agg[0] = '\0';
        format_IterationSummary(
            p,
            psd,
            pd,
            OutDom->ncol_OUT[eSW_VWCBulk],
            sw,
            OutDom->nrow_OUT,
            LogInfo
        );
    }
}
#endif


//------ eSW_VWCMatric
#ifdef SW_OUTTEXT

/**
@brief Gets vwcMatric text from SW_SOILWAT_OUTPUTS when dealing with OUTTEXT.

@param[in] pd Time period in simulation output (day/week/month/year)
@param[in] sw Comprehensive struct of type SW_RUN containing all information
    in the simulation.
@param[out] LogInfo Holds information on warnings and errors
*/
void get_vwcMatric_text(OutPeriod pd, SW_RUN *sw, LOG_INFO *LogInfo) {
    LyrIndex i;
    double convert;
    SW_SOILWAT_OUTPUTS *vo = sw->SoilWat.p_oagg[pd];
    SW_OUT_RUN *OutRun = &sw->OutRun;
    size_t writeSize = (size_t) (MAX_LAYERS * OUTSTRLEN);
    char *writePtr = OutRun->sw_outstr;
    char *endOutstr = OutRun->sw_outstr + sizeof OutRun->sw_outstr - 1;
    Bool fullBuffer = swFALSE;

    char str[OUTSTRLEN];
    OutRun->sw_outstr[0] = '\0';

    ForEachSoilLayer(i, sw->Site.n_layers) {
        /* vwcMatric at this point is identical to swcBulk */
        convert = 1. / (1. - sw->Site.soils.fractionVolBulk_gravel[i]) /
                  sw->Site.soils.width[i];

        (void) snprintf(
            str,
            OUTSTRLEN,
            "%c%.*f",
            OUTSEP,
            OUT_DIGITS,
            vo->vwcMatric[i] * convert
        );
        fullBuffer = sw_memccpy_inc(
            (void **) &writePtr, endOutstr, (void *) str, '\0', &writeSize
        );
        if (fullBuffer) {
            goto reportFullBuffer;
        }
    }

reportFullBuffer:
    if (fullBuffer) {
        reportFullBuffer(LOGERROR, LogInfo);
    }
}
#endif

#if defined(RSOILWAT) || defined(SWNETCDF)

/**
@brief Gets vwcMatric text from SW_SOILWAT_OUTPUTS when dealing with RSOILWAT.

@param[in] pd Time period in simulation output (day/week/month/year)
@param[in] sw Comprehensive struct of type SW_RUN containing all information
    in the simulation.
@param[in] OutDom Struct of type SW_OUT_DOM that holds output
    information that do not change throughout simulation runs
*/
void get_vwcMatric_mem(OutPeriod pd, SW_RUN *sw, SW_OUT_DOM *OutDom) {
    LyrIndex i;
    double convert;
    SW_SOILWAT_OUTPUTS *vo = sw->SoilWat.p_oagg[pd];
    size_t iOUTIndex = 0;
    SW_OUT_RUN *OutRun = &sw->OutRun;

    double *p = OutRun->p_OUT[eSW_VWCMatric][pd];

#if defined(RSOILWAT)
    get_outvalleader(
        &sw->Model, pd, OutRun->irow_OUT, OutDom->nrow_OUT, OutRun->tOffset, p
    );
#endif

    ForEachSoilLayer(i, sw->Site.n_layers) {
#if defined(RSOILWAT)
        iOUTIndex = iOUT(
            i, OutRun->irow_OUT[pd], OutDom->nrow_OUT[pd], ncol_TimeOUT[pd]
        );

#elif defined(SWNETCDF)
        iOUTIndex =
            OutDom->netCDFOutput.iOUToffset[eSW_VWCMatric][pd][0] +
            iOUTnc(
                OutRun->irow_OUT[pd], i, 0, OutDom->nsl_OUT[eSW_VWCMatric][0], 1
            );
#endif

        /* vwcMatric at this point is identical to swcBulk */
        convert = 1. / (1. - sw->Site.soils.fractionVolBulk_gravel[i]) /
                  sw->Site.soils.width[i];
        p[iOUTIndex] = vo->vwcMatric[i] * convert;
    }

#if defined(SWNETCDF)
    /* Set extra soil layers to missing/fill value (up to domain-wide max) */
    for (i = sw->Site.n_layers; i < OutDom->nsl_OUT[eSW_VWCMatric][0]; i++) {
        iOUTIndex =
            OutDom->netCDFOutput.iOUToffset[eSW_VWCMatric][pd][0] +
            iOUTnc(
                OutRun->irow_OUT[pd], i, 0, OutDom->nsl_OUT[eSW_VWCMatric][0], 1
            );
        p[iOUTIndex] = NC_FILL_DOUBLE;
    }
#endif // SWNETCDF
}

#elif defined(STEPWAT)

/**
@brief Gets vwcMatric text from SW_SOILWAT_OUTPUTS when dealing with STEPWAT.

@param[in] pd Time period in simulation output (day/week/month/year)
@param[in] sw Comprehensive struct of type SW_RUN containing all information
    in the simulation.
@param[in] OutDom Struct of type SW_OUT_DOM that holds output
    information that do not change throughout simulation runs
@param[out] LogInfo Holds information on warnings and errors
*/
void get_vwcMatric_agg(
    OutPeriod pd, SW_RUN *sw, SW_OUT_DOM *OutDom, LOG_INFO *LogInfo
) {
    LyrIndex i;
    double convert;
    SW_SOILWAT_OUTPUTS *vo = sw->SoilWat.p_oagg[pd];
    size_t iOUTIndex = 0;
    SW_OUT_RUN *OutRun = &sw->OutRun;

    double *p = OutRun->p_OUT[eSW_VWCMatric][pd];
    double *psd = OutRun->p_OUTsd[eSW_VWCMatric][pd];

    ForEachSoilLayer(i, sw->Site.n_layers) {
        /* vwcMatric at this point is identical to swcBulk */
        convert = 1. / (1. - sw->Site.soils.fractionVolBulk_gravel[i]) /
                  sw->Site.soils.width[i];

        iOUTIndex = iOUT(
            i, OutRun->irow_OUT[pd], OutDom->nrow_OUT[pd], ncol_TimeOUT[pd]
        );
        do_running_agg(
            p, psd, iOUTIndex, OutRun->currIter, vo->vwcMatric[i] * convert
        );
    }

    if (OutDom->print_IterationSummary) {
        OutRun->sw_outstr_agg[0] = '\0';
        format_IterationSummary(
            p,
            psd,
            pd,
            OutDom->ncol_OUT[eSW_VWCMatric],
            sw,
            OutDom->nrow_OUT,
            LogInfo
        );
    }
}
#endif


//------ eSW_SWA
#ifdef SW_OUTTEXT

/**
@brief Gets SWA text from SW_SOILWAT_OUTPUTS when dealing with OUTTEXT.

@param[in] pd Time period in simulation output (day/week/month/year)
@param[in] sw Comprehensive struct of type SW_RUN containing all information
    in the simulation.
@param[out] LogInfo Holds information on warnings and errors
*/
void get_swa_text(OutPeriod pd, SW_RUN *sw, LOG_INFO *LogInfo) {
    /* added 21-Oct-03, cwb */
    LyrIndex i;
    int k;
    SW_SOILWAT_OUTPUTS *vo = sw->SoilWat.p_oagg[pd];
    SW_OUT_RUN *OutRun = &sw->OutRun;
    size_t writeSize = (size_t) (MAX_LAYERS * OUTSTRLEN);
    char *writePtr = OutRun->sw_outstr;
    char *endOutstr = OutRun->sw_outstr + sizeof OutRun->sw_outstr - 1;
    Bool fullBuffer = swFALSE;

    char str[OUTSTRLEN];
    OutRun->sw_outstr[0] = '\0';

    ForEachVegType(k) {
        ForEachSoilLayer(i, sw->Site.n_layers) {
            (void) snprintf(
                str,
                OUTSTRLEN,
                "%c%.*f",
                OUTSEP,
                OUT_DIGITS,
                vo->SWA_VegType[k][i]
            );
            fullBuffer = sw_memccpy_inc(
                (void **) &writePtr, endOutstr, (void *) str, '\0', &writeSize
            );
            if (fullBuffer) {
                goto reportFullBuffer;
            }
        }
    }

reportFullBuffer:
    if (fullBuffer) {
        reportFullBuffer(LOGERROR, LogInfo);
    }
}
#endif

#if defined(RSOILWAT) || defined(SWNETCDF)

/**
@brief Gets SWA text from SW_SOILWAT_OUTPUTS when dealing with RSOILWAT.

@param[in] pd Time period in simulation output (day/week/month/year)
@param[in] sw Comprehensive struct of type SW_RUN containing all information
    in the simulation.
@param[in] OutDom Struct of type SW_OUT_DOM that holds output
    information that do not change throughout simulation runs
*/
void get_swa_mem(OutPeriod pd, SW_RUN *sw, SW_OUT_DOM *OutDom) {
    LyrIndex i;
    int k;
    SW_SOILWAT_OUTPUTS *vo = sw->SoilWat.p_oagg[pd];
    size_t iOUTIndex = 0;
    SW_OUT_RUN *OutRun = &sw->OutRun;

    double *p = OutRun->p_OUT[eSW_SWA][pd];

#if defined(RSOILWAT)
    get_outvalleader(
        &sw->Model, pd, OutRun->irow_OUT, OutDom->nrow_OUT, OutRun->tOffset, p
    );
#endif

    ForEachVegType(k) {
        ForEachSoilLayer(i, sw->Site.n_layers) {
#if defined(RSOILWAT)
            iOUTIndex = iOUT2(
                i, k, pd, OutRun->irow_OUT, OutDom->nrow_OUT, sw->Site.n_layers
            );

#elif defined(SWNETCDF)
            iOUTIndex = OutDom->netCDFOutput.iOUToffset[eSW_SWA][pd][0] +
                        iOUTnc(
                            OutRun->irow_OUT[pd],
                            i,
                            k,
                            OutDom->nsl_OUT[eSW_SWA][0],
                            OutDom->npft_OUT[eSW_SWA][0]
                        );
#endif

            p[iOUTIndex] = vo->SWA_VegType[k][i];
        }

#if defined(SWNETCDF)
        /* Set extra soil layers to missing/fill value (up to domain-wide max)
         */
        for (i = sw->Site.n_layers; i < OutDom->nsl_OUT[eSW_SWA][0]; i++) {
            iOUTIndex = OutDom->netCDFOutput.iOUToffset[eSW_SWA][pd][0] +
                        iOUTnc(
                            OutRun->irow_OUT[pd],
                            i,
                            k,
                            OutDom->nsl_OUT[eSW_SWA][0],
                            OutDom->npft_OUT[eSW_SWA][0]
                        );
            p[iOUTIndex] = NC_FILL_DOUBLE;
        }
#endif // SWNETCDF
    }
}

#elif defined(STEPWAT)

/**
@brief Gets SWA text from SW_SOILWAT_OUTPUTS when dealing with STEPWAT.

@param[in] pd Time period in simulation output (day/week/month/year)
@param[in] sw Comprehensive struct of type SW_RUN containing all information
    in the simulation.
@param[in] OutDom Struct of type SW_OUT_DOM that holds output
    information that do not change throughout simulation runs
@param[out] LogInfo Holds information on warnings and errors
*/
void get_swa_agg(
    OutPeriod pd, SW_RUN *sw, SW_OUT_DOM *OutDom, LOG_INFO *LogInfo
) {
    LyrIndex i;
    int k;
    SW_SOILWAT_OUTPUTS *vo = sw->SoilWat.p_oagg[pd];
    size_t iOUTIndex = 0;
    SW_OUT_RUN *OutRun = &sw->OutRun;

    double *p = OutRun->p_OUT[eSW_SWA][pd];
    double *psd = OutRun->p_OUTsd[eSW_SWA][pd];

    ForEachVegType(k) {
        ForEachSoilLayer(i, sw->Site.n_layers) {
            iOUTIndex = iOUT2(
                i, k, pd, OutRun->irow_OUT, OutDom->nrow_OUT, sw->Site.n_layers
            );
            do_running_agg(
                p, psd, iOUTIndex, OutRun->currIter, vo->SWA_VegType[k][i]
            );
        }
    }

    if (OutDom->print_IterationSummary) {
        OutRun->sw_outstr_agg[0] = '\0';
        format_IterationSummary2(
            p, psd, pd, NVEGTYPES, 0, sw, OutDom->nrow_OUT, LogInfo
        );
    }
}
#endif

//------ eSW_SWCBulk
#ifdef SW_OUTTEXT

/**
@brief Gets swcBulk text from SW_SOILWAT_OUTPUTS when dealing with OUTTEXT.

@param[in] pd Time period in simulation output (day/week/month/year)
@param[in] sw Comprehensive struct of type SW_RUN containing all information
    in the simulation.
@param[out] LogInfo Holds information on warnings and errors
*/
void get_swcBulk_text(OutPeriod pd, SW_RUN *sw, LOG_INFO *LogInfo) {
    /* added 21-Oct-03, cwb */
    LyrIndex i;
    SW_SOILWAT_OUTPUTS *vo = sw->SoilWat.p_oagg[pd];
    SW_OUT_RUN *OutRun = &sw->OutRun;
    size_t writeSize = (size_t) (MAX_LAYERS * OUTSTRLEN);
    char *writePtr = OutRun->sw_outstr;
    char *endOutstr = OutRun->sw_outstr + sizeof OutRun->sw_outstr - 1;
    Bool fullBuffer = swFALSE;

    char str[OUTSTRLEN];
    OutRun->sw_outstr[0] = '\0';

    ForEachSoilLayer(i, sw->Site.n_layers) {
        (void) snprintf(
            str, OUTSTRLEN, "%c%.*f", OUTSEP, OUT_DIGITS, vo->swcBulk[i]
        );
        fullBuffer = sw_memccpy_inc(
            (void **) &writePtr, endOutstr, (void *) str, '\0', &writeSize
        );
        if (fullBuffer) {
            goto reportFullBuffer;
        }
    }

reportFullBuffer:
    if (fullBuffer) {
        reportFullBuffer(LOGERROR, LogInfo);
    }
}
#endif

#if defined(RSOILWAT) || defined(SWNETCDF)

/**
@brief Gets swcBulk text from SW_SOILWAT_OUTPUTS when dealing with RSOILWAT.

@param[in] pd Time period in simulation output (day/week/month/year)
@param[in] sw Comprehensive struct of type SW_RUN containing all information
    in the simulation.
@param[in] OutDom Struct of type SW_OUT_DOM that holds output
    information that do not change throughout simulation runs
*/
void get_swcBulk_mem(OutPeriod pd, SW_RUN *sw, SW_OUT_DOM *OutDom) {
    LyrIndex i;
    SW_SOILWAT_OUTPUTS *vo = sw->SoilWat.p_oagg[pd];
    size_t iOUTIndex = 0;
    SW_OUT_RUN *OutRun = &sw->OutRun;

    double *p = OutRun->p_OUT[eSW_SWCBulk][pd];

#if defined(RSOILWAT)
    get_outvalleader(
        &sw->Model, pd, OutRun->irow_OUT, OutDom->nrow_OUT, OutRun->tOffset, p
    );
#endif

    ForEachSoilLayer(i, sw->Site.n_layers) {
#if defined(RSOILWAT)
        iOUTIndex = iOUT(
            i, OutRun->irow_OUT[pd], OutDom->nrow_OUT[pd], ncol_TimeOUT[pd]
        );

#elif defined(SWNETCDF)
        iOUTIndex =
            OutDom->netCDFOutput.iOUToffset[eSW_SWCBulk][pd][0] +
            iOUTnc(
                OutRun->irow_OUT[pd], i, 0, OutDom->nsl_OUT[eSW_SWCBulk][0], 1
            );
#endif

        p[iOUTIndex] = vo->swcBulk[i];
    }

#if defined(SWNETCDF)
    /* Set extra soil layers to missing/fill value (up to domain-wide max) */
    for (i = sw->Site.n_layers; i < OutDom->nsl_OUT[eSW_SWCBulk][0]; i++) {
        iOUTIndex =
            OutDom->netCDFOutput.iOUToffset[eSW_SWCBulk][pd][0] +
            iOUTnc(
                OutRun->irow_OUT[pd], i, 0, OutDom->nsl_OUT[eSW_SWCBulk][0], 1
            );
        p[iOUTIndex] = NC_FILL_DOUBLE;
    }
#endif // SWNETCDF
}

#elif defined(STEPWAT)

/**
@brief Gets swcBulk text from SW_SOILWAT_OUTPUTS when dealing with STEPWAT.

@param[in] pd Time period in simulation output (day/week/month/year)
@param[in] sw Comprehensive struct of type SW_RUN containing all information
    in the simulation.
@param[in] OutDom Struct of type SW_OUT_DOM that holds output
    information that do not change throughout simulation runs
@param[out] LogInfo Holds information on warnings and errors
*/
void get_swcBulk_agg(
    OutPeriod pd, SW_RUN *sw, SW_OUT_DOM *OutDom, LOG_INFO *LogInfo
) {
    LyrIndex i;
    SW_SOILWAT_OUTPUTS *vo = sw->SoilWat.p_oagg[pd];
    size_t iOUTIndex = 0;
    SW_OUT_RUN *OutRun = &sw->OutRun;

    double *p = OutRun->p_OUT[eSW_SWCBulk][pd];
    double *psd = OutRun->p_OUTsd[eSW_SWCBulk][pd];

    ForEachSoilLayer(i, sw->Site.n_layers) {
        iOUTIndex = iOUT(
            i, OutRun->irow_OUT[pd], OutDom->nrow_OUT[pd], ncol_TimeOUT[pd]
        );
        do_running_agg(p, psd, iOUTIndex, OutRun->currIter, vo->swcBulk[i]);
    }

    if (OutDom->print_IterationSummary) {
        OutRun->sw_outstr_agg[0] = '\0';
        format_IterationSummary(
            p,
            psd,
            pd,
            OutDom->ncol_OUT[eSW_SWCBulk],
            sw,
            OutDom->nrow_OUT,
            LogInfo
        );
    }

    (void) LogInfo;
}

/**
@brief STEPWAT2 expects monthly mean SWCbulk by soil layer.

@param[in] pd Time period in simulation output (day/week/month/year)
@param[in] sw Comprehensive struct of type SW_RUN containing all information
    in the simulation.
@param[in] OutDom Struct of type SW_OUT_DOM that holds output
    information that do not change throughout simulation runs
@param[out] LogInfo Holds information on warnings and errors
*/
void get_swcBulk_SXW(
    OutPeriod pd, SW_RUN *sw, SW_OUT_DOM *OutDom, LOG_INFO *LogInfo
) {
    TimeInt month;

    if (pd == eSW_Month) {
        LyrIndex i;
        SW_SOILWAT_OUTPUTS *vo = sw->SoilWat.p_oagg[pd];
        SW_OUT_RUN *OutRun = &sw->OutRun;
        month = sw->Model.month - OutRun->tOffset;

        ForEachSoilLayer(i, sw->Site.n_layers) {
            OutRun->swc[i][month] = vo->swcBulk[i];
        }
    }

    (void) OutDom;
    (void) LogInfo;
}
#endif


//------ eSW_SWPMatric
#ifdef SW_OUTTEXT

/**
@brief eSW_SWPMatric Can't take arithmetic average of swp vecause its
exponentail. At this time (until I rewmember to look up whether harmonic OutRun
some other average is better and fix this) we're not averaging swp but
converting the averged swc.  This also avoids converting for each day. added
12-Oct-03, cwb

@param[in] pd Time period in simulation output (day/week/month/year)
@param[in] sw Comprehensive struct of type SW_RUN containing all information
    in the simulation.
@param[out] LogInfo Holds information on warnings and errors
*/
void get_swpMatric_text(OutPeriod pd, SW_RUN *sw, LOG_INFO *LogInfo) {
    double val;
    LyrIndex i;
    SW_SOILWAT_OUTPUTS *vo = sw->SoilWat.p_oagg[pd];
    SW_OUT_RUN *OutRun = &sw->OutRun;
    size_t writeSize = (size_t) (MAX_LAYERS * OUTSTRLEN);
    char *writePtr = OutRun->sw_outstr;
    char *endOutstr = OutRun->sw_outstr + sizeof OutRun->sw_outstr - 1;
    Bool fullBuffer = swFALSE;

    /* Local LOG_INFO only because `SW_SWRC_SWCtoSWP()` requires it */
    LOG_INFO local_log;
    local_log.logfp = NULL;

    char str[OUTSTRLEN];
    OutRun->sw_outstr[0] = '\0';

    ForEachSoilLayer(i, sw->Site.n_layers) {
        /* swpMatric at this point is identical to swcBulk */
        val = SW_SWRC_SWCtoSWP(vo->swpMatric[i], &sw->Site, i, &local_log);


        (void) snprintf(str, OUTSTRLEN, "%c%.*f", OUTSEP, OUT_DIGITS, val);
        fullBuffer = sw_memccpy_inc(
            (void **) &writePtr, endOutstr, (void *) str, '\0', &writeSize
        );
        if (fullBuffer) {
            goto reportFullBuffer;
        }
    }

reportFullBuffer:
    if (fullBuffer) {
        reportFullBuffer(LOGERROR, LogInfo);
    }
}
#endif

#if defined(RSOILWAT) || defined(SWNETCDF)

/**
@brief Gets swpMatric when dealing with RSOILWAT

@param[in] pd Time period in simulation output (day/week/month/year)
@param[in] sw Comprehensive struct of type SW_RUN containing all information
    in the simulation.
@param[in] OutDom Struct of type SW_OUT_DOM that holds output
    information that do not change throughout simulation runs
*/
void get_swpMatric_mem(OutPeriod pd, SW_RUN *sw, SW_OUT_DOM *OutDom) {
    LyrIndex i;
    SW_SOILWAT_OUTPUTS *vo = sw->SoilWat.p_oagg[pd];
    LOG_INFO local_log;
    size_t iOUTIndex = 0;
    SW_OUT_RUN *OutRun = &sw->OutRun;

    double *p = OutRun->p_OUT[eSW_SWPMatric][pd];

#if defined(RSOILWAT)
    get_outvalleader(
        &sw->Model, pd, OutRun->irow_OUT, OutDom->nrow_OUT, OutRun->tOffset, p
    );
#endif

    ForEachSoilLayer(i, sw->Site.n_layers) {
#if defined(RSOILWAT)
        iOUTIndex = iOUT(
            i, OutRun->irow_OUT[pd], OutDom->nrow_OUT[pd], ncol_TimeOUT[pd]
        );

#elif defined(SWNETCDF)
        iOUTIndex =
            OutDom->netCDFOutput.iOUToffset[eSW_SWPMatric][pd][0] +
            iOUTnc(
                OutRun->irow_OUT[pd], i, 0, OutDom->nsl_OUT[eSW_SWPMatric][0], 1
            );
#endif

        /* swpMatric at this point is identical to swcBulk */
        p[iOUTIndex] =
            SW_SWRC_SWCtoSWP(vo->swpMatric[i], &sw->Site, i, &local_log);
    }

#if defined(SWNETCDF)
    /* Set extra soil layers to missing/fill value (up to domain-wide max) */
    for (i = sw->Site.n_layers; i < OutDom->nsl_OUT[eSW_SWPMatric][0]; i++) {
        iOUTIndex =
            OutDom->netCDFOutput.iOUToffset[eSW_SWPMatric][pd][0] +
            iOUTnc(
                OutRun->irow_OUT[pd], i, 0, OutDom->nsl_OUT[eSW_SWPMatric][0], 1
            );
        p[iOUTIndex] = NC_FILL_DOUBLE;
    }
#endif // SWNETCDF
}

#elif defined(STEPWAT)

/**
@brief Gets swpMatric when dealing with STEPWAT.

@param[in] pd Time period in simulation output (day/week/month/year)
@param[in] sw Comprehensive struct of type SW_RUN containing all information
    in the simulation.
@param[in] OutDom Struct of type SW_OUT_DOM that holds output
    information that do not change throughout simulation runs
@param[out] LogInfo Holds information on warnings and errors
*/
void get_swpMatric_agg(
    OutPeriod pd, SW_RUN *sw, SW_OUT_DOM *OutDom, LOG_INFO *LogInfo
) {
    double val;
    LyrIndex i;
    SW_SOILWAT_OUTPUTS *vo = sw->SoilWat.p_oagg[pd];
    LOG_INFO local_log;
    size_t iOUTIndex = 0;
    SW_OUT_RUN *OutRun = &sw->OutRun;

    double *p = OutRun->p_OUT[eSW_SWPMatric][pd];
    double *psd = OutRun->p_OUTsd[eSW_SWPMatric][pd];

    ForEachSoilLayer(i, sw->Site.n_layers) {
        /* swpMatric at this point is identical to swcBulk */
        val = SW_SWRC_SWCtoSWP(vo->swpMatric[i], &sw->Site, i, &local_log);

        iOUTIndex = iOUT(
            i, OutRun->irow_OUT[pd], OutDom->nrow_OUT[pd], ncol_TimeOUT[pd]
        );
        do_running_agg(p, psd, iOUTIndex, OutRun->currIter, val);
    }

    if (OutDom->print_IterationSummary) {
        OutRun->sw_outstr_agg[0] = '\0';
        format_IterationSummary(
            p,
            psd,
            pd,
            OutDom->ncol_OUT[eSW_SWPMatric],
            sw,
            OutDom->nrow_OUT,
            LogInfo
        );
    }
}
#endif


//------ eSW_SWABulk
#ifdef SW_OUTTEXT

/**
@brief gets swaBulk when dealing with OUTTEXT.

@param[in] pd Time period in simulation output (day/week/month/year)
@param[in] sw Comprehensive struct of type SW_RUN containing all information
    in the simulation.
@param[out] LogInfo Holds information on warnings and errors
*/
void get_swaBulk_text(OutPeriod pd, SW_RUN *sw, LOG_INFO *LogInfo) {
    LyrIndex i;
    SW_SOILWAT_OUTPUTS *vo = sw->SoilWat.p_oagg[pd];
    SW_OUT_RUN *OutRun = &sw->OutRun;
    size_t writeSize = (size_t) (MAX_LAYERS * OUTSTRLEN);
    char *writePtr = OutRun->sw_outstr;
    char *endOutstr = OutRun->sw_outstr + sizeof OutRun->sw_outstr - 1;
    Bool fullBuffer = swFALSE;

    char str[OUTSTRLEN];
    OutRun->sw_outstr[0] = '\0';

    ForEachSoilLayer(i, sw->Site.n_layers) {
        (void) snprintf(
            str, OUTSTRLEN, "%c%.*f", OUTSEP, OUT_DIGITS, vo->swaBulk[i]
        );
        fullBuffer = sw_memccpy_inc(
            (void **) &writePtr, endOutstr, (void *) str, '\0', &writeSize
        );
        if (fullBuffer) {
            goto reportFullBuffer;
        }
    }

reportFullBuffer:
    if (fullBuffer) {
        reportFullBuffer(LOGERROR, LogInfo);
    }
}
#endif

#if defined(RSOILWAT) || defined(SWNETCDF)

/**
@brief Gets swaBulk when dealing with RSOILWAT.

@param[in] pd Time period in simulation output (day/week/month/year)
@param[in] sw Comprehensive struct of type SW_RUN containing all information
    in the simulation.
@param[in] OutDom Struct of type SW_OUT_DOM that holds output
    information that do not change throughout simulation runs
*/
void get_swaBulk_mem(OutPeriod pd, SW_RUN *sw, SW_OUT_DOM *OutDom) {
    LyrIndex i;
    SW_SOILWAT_OUTPUTS *vo = sw->SoilWat.p_oagg[pd];
    size_t iOUTIndex = 0;
    SW_OUT_RUN *OutRun = &sw->OutRun;

    double *p = OutRun->p_OUT[eSW_SWABulk][pd];

#if defined(RSOILWAT)
    get_outvalleader(
        &sw->Model, pd, OutRun->irow_OUT, OutDom->nrow_OUT, OutRun->tOffset, p
    );
#endif

    ForEachSoilLayer(i, sw->Site.n_layers) {
#if defined(RSOILWAT)
        iOUTIndex = iOUT(
            i, OutRun->irow_OUT[pd], OutDom->nrow_OUT[pd], ncol_TimeOUT[pd]
        );

#elif defined(SWNETCDF)
        iOUTIndex =
            OutDom->netCDFOutput.iOUToffset[eSW_SWABulk][pd][0] +
            iOUTnc(
                OutRun->irow_OUT[pd], i, 0, OutDom->nsl_OUT[eSW_SWABulk][0], 1
            );
#endif

        p[iOUTIndex] = vo->swaBulk[i];
    }

#if defined(SWNETCDF)
    /* Set extra soil layers to missing/fill value (up to domain-wide max) */
    for (i = sw->Site.n_layers; i < OutDom->nsl_OUT[eSW_SWABulk][0]; i++) {
        iOUTIndex =
            OutDom->netCDFOutput.iOUToffset[eSW_SWABulk][pd][0] +
            iOUTnc(
                OutRun->irow_OUT[pd], i, 0, OutDom->nsl_OUT[eSW_SWABulk][0], 1
            );
        p[iOUTIndex] = NC_FILL_DOUBLE;
    }
#endif // SWNETCDF
}

#elif defined(STEPWAT)

/**
@brief Gets swaBulk when dealing with STEPWAT.

@param[in] pd Time period in simulation output (day/week/month/year)
@param[in] sw Comprehensive struct of type SW_RUN containing all information
    in the simulation.
@param[in] OutDom Struct of type SW_OUT_DOM that holds output
    information that do not change throughout simulation runs
@param[out] LogInfo Holds information on warnings and errors
*/
void get_swaBulk_agg(
    OutPeriod pd, SW_RUN *sw, SW_OUT_DOM *OutDom, LOG_INFO *LogInfo
) {
    LyrIndex i;
    SW_SOILWAT_OUTPUTS *vo = sw->SoilWat.p_oagg[pd];
    size_t iOUTIndex = 0;
    SW_OUT_RUN *OutRun = &sw->OutRun;

    double *p = OutRun->p_OUT[eSW_SWABulk][pd];
    double *psd = OutRun->p_OUTsd[eSW_SWABulk][pd];

    ForEachSoilLayer(i, sw->Site.n_layers) {
        iOUTIndex = iOUT(
            i, OutRun->irow_OUT[pd], OutDom->nrow_OUT[pd], ncol_TimeOUT[pd]
        );
        do_running_agg(p, psd, iOUTIndex, OutRun->currIter, vo->swaBulk[i]);
    }

    if (OutDom->print_IterationSummary) {
        OutRun->sw_outstr_agg[0] = '\0';
        format_IterationSummary(
            p,
            psd,
            pd,
            OutDom->ncol_OUT[eSW_SWABulk],
            sw,
            OutDom->nrow_OUT,
            LogInfo
        );
    }
}
#endif

//------ eSW_SWAMatric
#ifdef SW_OUTTEXT

/**
@brief Gets swaMatric when dealing with OUTTEXT.

@param[in] pd Time period in simulation output (day/week/month/year)
@param[in] sw Comprehensive struct of type SW_RUN containing all information
    in the simulation.
@param[out] LogInfo Holds information on warnings and errors
*/
void get_swaMatric_text(OutPeriod pd, SW_RUN *sw, LOG_INFO *LogInfo) {
    LyrIndex i;
    double convert;
    SW_SOILWAT_OUTPUTS *vo = sw->SoilWat.p_oagg[pd];
    SW_OUT_RUN *OutRun = &sw->OutRun;
    size_t writeSize = (size_t) (MAX_LAYERS * OUTSTRLEN);
    char *writePtr = OutRun->sw_outstr;
    char *endOutstr = OutRun->sw_outstr + sizeof OutRun->sw_outstr - 1;
    Bool fullBuffer = swFALSE;

    char str[OUTSTRLEN];
    OutRun->sw_outstr[0] = '\0';

    ForEachSoilLayer(i, sw->Site.n_layers) {
        /* swaMatric at this point is identical to swaBulk */
        convert = 1. / (1. - sw->Site.soils.fractionVolBulk_gravel[i]);

        (void) snprintf(
            str,
            OUTSTRLEN,
            "%c%.*f",
            OUTSEP,
            OUT_DIGITS,
            vo->swaMatric[i] * convert
        );
        fullBuffer = sw_memccpy_inc(
            (void **) &writePtr, endOutstr, (void *) str, '\0', &writeSize
        );
        if (fullBuffer) {
            goto reportFullBuffer;
        }
    }

reportFullBuffer:
    if (fullBuffer) {
        reportFullBuffer(LOGERROR, LogInfo);
    }
}
#endif

#if defined(RSOILWAT) || defined(SWNETCDF)

/**
@brief Gets swaMatric when dealing with RSOILWAT.

@param[in] pd Time period in simulation output (day/week/month/year)
@param[in] sw Comprehensive struct of type SW_RUN containing all information
    in the simulation.
@param[in] OutDom Struct of type SW_OUT_DOM that holds output
    information that do not change throughout simulation runs
*/
void get_swaMatric_mem(OutPeriod pd, SW_RUN *sw, SW_OUT_DOM *OutDom) {
    LyrIndex i;
    double convert;
    SW_SOILWAT_OUTPUTS *vo = sw->SoilWat.p_oagg[pd];
    size_t iOUTIndex = 0;
    SW_OUT_RUN *OutRun = &sw->OutRun;

    double *p = OutRun->p_OUT[eSW_SWAMatric][pd];

#if defined(RSOILWAT)
    get_outvalleader(
        &sw->Model, pd, OutRun->irow_OUT, OutDom->nrow_OUT, OutRun->tOffset, p
    );
#endif

    ForEachSoilLayer(i, sw->Site.n_layers) {
#if defined(RSOILWAT)
        iOUTIndex = iOUT(
            i, OutRun->irow_OUT[pd], OutDom->nrow_OUT[pd], ncol_TimeOUT[pd]
        );

#elif defined(SWNETCDF)
        iOUTIndex =
            OutDom->netCDFOutput.iOUToffset[eSW_SWAMatric][pd][0] +
            iOUTnc(
                OutRun->irow_OUT[pd], i, 0, OutDom->nsl_OUT[eSW_SWAMatric][0], 1
            );
#endif

        /* swaMatric at this point is identical to swaBulk */
        convert = 1. / (1. - sw->Site.soils.fractionVolBulk_gravel[i]);
        p[iOUTIndex] = vo->swaMatric[i] * convert;
    }

#if defined(SWNETCDF)
    /* Set extra soil layers to missing/fill value (up to domain-wide max) */
    for (i = sw->Site.n_layers; i < OutDom->nsl_OUT[eSW_SWAMatric][0]; i++) {
        iOUTIndex =
            OutDom->netCDFOutput.iOUToffset[eSW_SWAMatric][pd][0] +
            iOUTnc(
                OutRun->irow_OUT[pd], i, 0, OutDom->nsl_OUT[eSW_SWAMatric][0], 1
            );
        p[iOUTIndex] = NC_FILL_DOUBLE;
    }
#endif // SWNETCDF
}

#elif defined(STEPWAT)

/**
@brief Gets swaMatric when dealing with STEPWAT.

@param[in] pd Time period in simulation output (day/week/month/year)
@param[in] sw Comprehensive struct of type SW_RUN containing all information
    in the simulation.
@param[in] OutDom Struct of type SW_OUT_DOM that holds output
    information that do not change throughout simulation runs
@param[out] LogInfo Holds information on warnings and errors
*/
void get_swaMatric_agg(
    OutPeriod pd, SW_RUN *sw, SW_OUT_DOM *OutDom, LOG_INFO *LogInfo
) {
    LyrIndex i;
    double convert;
    SW_SOILWAT_OUTPUTS *vo = sw->SoilWat.p_oagg[pd];
    size_t iOUTIndex = 0;
    SW_OUT_RUN *OutRun = &sw->OutRun;

    double *p = OutRun->p_OUT[eSW_SWAMatric][pd];
    double *psd = OutRun->p_OUTsd[eSW_SWAMatric][pd];

    ForEachSoilLayer(i, sw->Site.n_layers) {
        /* swaMatric at this point is identical to swaBulk */
        convert = 1. / (1. - sw->Site.soils.fractionVolBulk_gravel[i]);

        iOUTIndex = iOUT(
            i, OutRun->irow_OUT[pd], OutDom->nrow_OUT[pd], ncol_TimeOUT[pd]
        );
        do_running_agg(
            p, psd, iOUTIndex, OutRun->currIter, vo->swaMatric[i] * convert
        );
    }

    if (OutDom->print_IterationSummary) {
        OutRun->sw_outstr_agg[0] = '\0';
        format_IterationSummary(
            p,
            psd,
            pd,
            OutDom->ncol_OUT[eSW_SWAMatric],
            sw,
            OutDom->nrow_OUT,
            LogInfo
        );
    }
}
#endif


//------ eSW_SurfaceWater
#ifdef SW_OUTTEXT

/**
@brief Gets surfaceWater when dealing with OUTTEXT.

@param[in] pd Time period in simulation output (day/week/month/year)
@param[in] sw Comprehensive struct of type SW_RUN containing all information
    in the simulation.
@param[out] LogInfo Holds information on warnings and errors
*/
void get_surfaceWater_text(OutPeriod pd, SW_RUN *sw, LOG_INFO *LogInfo) {
    SW_SOILWAT_OUTPUTS *vo = sw->SoilWat.p_oagg[pd];
    SW_OUT_RUN *OutRun = &sw->OutRun;

    OutRun->sw_outstr[0] = '\0';
    (void) snprintf(
        OutRun->sw_outstr,
        sizeof OutRun->sw_outstr,
        "%c%.*f",
        OUTSEP,
        OUT_DIGITS,
        vo->surfaceWater
    );

    (void) LogInfo;
}
#endif

#if defined(RSOILWAT) || defined(SWNETCDF)

/**
@brief Gets surfaceWater when dealing with RSOILWAT.

@param[in] pd Time period in simulation output (day/week/month/year)
@param[in] sw Comprehensive struct of type SW_RUN containing all information
    in the simulation.
@param[in] OutDom Struct of type SW_OUT_DOM that holds output
    information that do not change throughout simulation runs
*/
void get_surfaceWater_mem(OutPeriod pd, SW_RUN *sw, SW_OUT_DOM *OutDom) {
    SW_SOILWAT_OUTPUTS *vo = sw->SoilWat.p_oagg[pd];
    size_t iOUTIndex = 0;
    SW_OUT_RUN *OutRun = &sw->OutRun;

    double *p = OutRun->p_OUT[eSW_SurfaceWater][pd];

#if defined(RSOILWAT)
    get_outvalleader(
        &sw->Model, pd, OutRun->irow_OUT, OutDom->nrow_OUT, OutRun->tOffset, p
    );
#endif

#if defined(RSOILWAT)
    iOUTIndex =
        iOUT(0, OutRun->irow_OUT[pd], OutDom->nrow_OUT[pd], ncol_TimeOUT[pd]);

#elif defined(SWNETCDF)
    iOUTIndex = OutDom->netCDFOutput.iOUToffset[eSW_SurfaceWater][pd][0] +
                iOUTnc(OutRun->irow_OUT[pd], 0, 0, 1, 1);
#endif

    p[iOUTIndex] = vo->surfaceWater;
}

#elif defined(STEPWAT)

/**
@brief Gets surfaceWater when dealing with STEPWAT.

@param[in] pd Time period in simulation output (day/week/month/year)
@param[in] sw Comprehensive struct of type SW_RUN containing all information
    in the simulation.
@param[in] OutDom Struct of type SW_OUT_DOM that holds output
    information that do not change throughout simulation runs
@param[out] LogInfo Holds information on warnings and errors
*/
void get_surfaceWater_agg(
    OutPeriod pd, SW_RUN *sw, SW_OUT_DOM *OutDom, LOG_INFO *LogInfo
) {
    SW_SOILWAT_OUTPUTS *vo = sw->SoilWat.p_oagg[pd];
    size_t iOUTIndex = 0;
    SW_OUT_RUN *OutRun = &sw->OutRun;

    double *p = OutRun->p_OUT[eSW_SurfaceWater][pd];
    double *psd = OutRun->p_OUTsd[eSW_SurfaceWater][pd];

    iOUTIndex =
        iOUT(0, OutRun->irow_OUT[pd], OutDom->nrow_OUT[pd], ncol_TimeOUT[pd]);
    do_running_agg(p, psd, iOUTIndex, OutRun->currIter, vo->surfaceWater);

    if (OutDom->print_IterationSummary) {
        OutRun->sw_outstr_agg[0] = '\0';
        format_IterationSummary(
            p,
            psd,
            pd,
            OutDom->ncol_OUT[eSW_SurfaceWater],
            sw,
            OutDom->nrow_OUT,
            LogInfo
        );
    }
}
#endif


//------ eSW_Runoff
#ifdef SW_OUTTEXT

/**
@brief Gets surfaceRunon, surfaceRunoff, and snowRunoff when dealing with
OUTTEXT.

@param[in] pd Time period in simulation output (day/week/month/year)
@param[in] sw Comprehensive struct of type SW_RUN containing all information
    in the simulation.
@param[out] LogInfo Holds information on warnings and errors
*/
void get_runoffrunon_text(OutPeriod pd, SW_RUN *sw, LOG_INFO *LogInfo) {
    double net;
    SW_WEATHER_OUTPUTS *vo = sw->Weather.p_oagg[pd];
    SW_OUT_RUN *OutRun = &sw->OutRun;

    net = vo->surfaceRunoff + vo->snowRunoff - vo->surfaceRunon;

    OutRun->sw_outstr[0] = '\0';
    (void) snprintf(
        OutRun->sw_outstr,
        sizeof OutRun->sw_outstr,
        "%c%.*f%c%.*f%c%.*f%c%.*f",
        OUTSEP,
        OUT_DIGITS,
        net,
        OUTSEP,
        OUT_DIGITS,
        vo->surfaceRunoff,
        OUTSEP,
        OUT_DIGITS,
        vo->snowRunoff,
        OUTSEP,
        OUT_DIGITS,
        vo->surfaceRunon
    );

    (void) LogInfo;
}
#endif

#if defined(RSOILWAT) || defined(SWNETCDF)

/**
@brief Gets surfaceRunon, surfaceRunoff, and snowRunoff when dealing with
RSOILWAT.

@param[in] pd Time period in simulation output (day/week/month/year)
@param[in] sw Comprehensive struct of type SW_RUN containing all information
    in the simulation.
@param[in] OutDom Struct of type SW_OUT_DOM that holds output
    information that do not change throughout simulation runs
*/
void get_runoffrunon_mem(OutPeriod pd, SW_RUN *sw, SW_OUT_DOM *OutDom) {
    SW_WEATHER_OUTPUTS *vo = sw->Weather.p_oagg[pd];
    size_t iOUTIndex = 0;
    SW_OUT_RUN *OutRun = &sw->OutRun;

    double *p = OutRun->p_OUT[eSW_Runoff][pd];

#if defined(RSOILWAT)
    get_outvalleader(
        &sw->Model, pd, OutRun->irow_OUT, OutDom->nrow_OUT, OutRun->tOffset, p
    );
#endif

#if defined(RSOILWAT)
    iOUTIndex =
        iOUT(0, OutRun->irow_OUT[pd], OutDom->nrow_OUT[pd], ncol_TimeOUT[pd]);

#elif defined(SWNETCDF)
    iOUTIndex = OutDom->netCDFOutput.iOUToffset[eSW_Runoff][pd][0] +
                iOUTnc(OutRun->irow_OUT[pd], 0, 0, 1, 1);
#endif

    p[iOUTIndex] = vo->surfaceRunoff + vo->snowRunoff - vo->surfaceRunon;


#if defined(RSOILWAT)
    iOUTIndex =
        iOUT(1, OutRun->irow_OUT[pd], OutDom->nrow_OUT[pd], ncol_TimeOUT[pd]);

#elif defined(SWNETCDF)
    iOUTIndex = OutDom->netCDFOutput.iOUToffset[eSW_Runoff][pd][1] +
                iOUTnc(OutRun->irow_OUT[pd], 0, 0, 1, 1);
#endif

    p[iOUTIndex] = vo->surfaceRunoff;


#if defined(RSOILWAT)
    iOUTIndex =
        iOUT(2, OutRun->irow_OUT[pd], OutDom->nrow_OUT[pd], ncol_TimeOUT[pd]);

#elif defined(SWNETCDF)
    iOUTIndex = OutDom->netCDFOutput.iOUToffset[eSW_Runoff][pd][2] +
                iOUTnc(OutRun->irow_OUT[pd], 0, 0, 1, 1);
#endif

    p[iOUTIndex] = vo->snowRunoff;


#if defined(RSOILWAT)
    iOUTIndex =
        iOUT(3, OutRun->irow_OUT[pd], OutDom->nrow_OUT[pd], ncol_TimeOUT[pd]);

#elif defined(SWNETCDF)
    iOUTIndex = OutDom->netCDFOutput.iOUToffset[eSW_Runoff][pd][3] +
                iOUTnc(OutRun->irow_OUT[pd], 0, 0, 1, 1);
#endif

    p[iOUTIndex] = vo->surfaceRunon;
}

#elif defined(STEPWAT)

/**
@brief Gets surfaceRunon, surfaceRunoff, and snowRunoff when dealing with
STEPWAT.

@param[in] pd Time period in simulation output (day/week/month/year)
@param[in] sw Comprehensive struct of type SW_RUN containing all information
    in the simulation.
@param[in] OutDom Struct of type SW_OUT_DOM that holds output
    information that do not change throughout simulation runs
@param[out] LogInfo Holds information on warnings and errors
*/
void get_runoffrunon_agg(
    OutPeriod pd, SW_RUN *sw, SW_OUT_DOM *OutDom, LOG_INFO *LogInfo
) {
    double net;
    SW_WEATHER_OUTPUTS *vo = sw->Weather.p_oagg[pd];
    size_t iOUTIndex = 0;
    SW_OUT_RUN *OutRun = &sw->OutRun;

    double *p = OutRun->p_OUT[eSW_Runoff][pd];
    double *psd = OutRun->p_OUTsd[eSW_Runoff][pd];

    net = vo->surfaceRunoff + vo->snowRunoff - vo->surfaceRunon;

    iOUTIndex =
        iOUT(0, OutRun->irow_OUT[pd], OutDom->nrow_OUT[pd], ncol_TimeOUT[pd]);
    do_running_agg(p, psd, iOUTIndex, OutRun->currIter, net);

    iOUTIndex =
        iOUT(1, OutRun->irow_OUT[pd], OutDom->nrow_OUT[pd], ncol_TimeOUT[pd]);
    do_running_agg(p, psd, iOUTIndex, OutRun->currIter, vo->surfaceRunoff);

    iOUTIndex =
        iOUT(2, OutRun->irow_OUT[pd], OutDom->nrow_OUT[pd], ncol_TimeOUT[pd]);
    do_running_agg(p, psd, iOUTIndex, OutRun->currIter, vo->snowRunoff);

    iOUTIndex =
        iOUT(3, OutRun->irow_OUT[pd], OutDom->nrow_OUT[pd], ncol_TimeOUT[pd]);
    do_running_agg(p, psd, iOUTIndex, OutRun->currIter, vo->surfaceRunon);

    if (OutDom->print_IterationSummary) {
        OutRun->sw_outstr_agg[0] = '\0';
        format_IterationSummary(
            p,
            psd,
            pd,
            OutDom->ncol_OUT[eSW_Runoff],
            sw,
            OutDom->nrow_OUT,
            LogInfo
        );
    }
}
#endif

//------ eSW_Transp
#ifdef SW_OUTTEXT

/**
@brief Gets transp_total when dealing with OUTTEXT.

@param[in] pd Time period in simulation output (day/week/month/year)
@param[in] sw Comprehensive struct of type SW_RUN containing all information
    in the simulation.
@param[out] LogInfo Holds information on warnings and errors
*/
void get_transp_text(OutPeriod pd, SW_RUN *sw, LOG_INFO *LogInfo) {
    LyrIndex i;
    LyrIndex n_layers = sw->Site.n_layers;
    int k;
    size_t writeSize = (size_t) (MAX_LAYERS * OUTSTRLEN);
    SW_SOILWAT_OUTPUTS *vo = sw->SoilWat.p_oagg[pd];
    SW_OUT_RUN *OutRun = &sw->OutRun;
    char *writePtr = OutRun->sw_outstr;
    char *endOutstr = OutRun->sw_outstr + sizeof OutRun->sw_outstr - 1;
    Bool fullBuffer = swFALSE;

    char str[OUTSTRLEN] = {'\0'};
    OutRun->sw_outstr[0] = '\0';

    /* total transpiration */
    ForEachSoilLayer(i, n_layers) {
        (void) snprintf(
            str, OUTSTRLEN, "%c%.*f", OUTSEP, OUT_DIGITS, vo->transp_total[i]
        );
        fullBuffer = sw_memccpy_inc(
            (void **) &writePtr, endOutstr, (void *) str, '\0', &writeSize
        );
        if (fullBuffer) {
            goto reportFullBuffer;
        }
    }

    /* transpiration for each vegetation type */
    ForEachVegType(k) {
        ForEachSoilLayer(i, n_layers) {
            (void) snprintf(
                str, OUTSTRLEN, "%c%.*f", OUTSEP, OUT_DIGITS, vo->transp[k][i]
            );
            fullBuffer = sw_memccpy_inc(
                (void **) &writePtr, endOutstr, (void *) str, '\0', &writeSize
            );
            if (fullBuffer) {
                goto reportFullBuffer;
            }
        }
    }

reportFullBuffer:
    if (fullBuffer) {
        reportFullBuffer(LOGERROR, LogInfo);
    }
}
#endif

#if defined(RSOILWAT) || defined(SWNETCDF)

/**
@brief Gets transp_total when dealing with RSOILWAT.

@param[in] pd Time period in simulation output (day/week/month/year)
@param[in] sw Comprehensive struct of type SW_RUN containing all information
    in the simulation.
@param[in] OutDom Struct of type SW_OUT_DOM that holds output
    information that do not change throughout simulation runs
*/
void get_transp_mem(OutPeriod pd, SW_RUN *sw, SW_OUT_DOM *OutDom) {
    LyrIndex i;
    LyrIndex n_layers = sw->Site.n_layers;
    int k;
    SW_SOILWAT_OUTPUTS *vo = sw->SoilWat.p_oagg[pd];
    size_t iOUTIndex = 0;
    SW_OUT_RUN *OutRun = &sw->OutRun;

    double *p = OutRun->p_OUT[eSW_Transp][pd];

#if defined(RSOILWAT)
    get_outvalleader(
        &sw->Model, pd, OutRun->irow_OUT, OutDom->nrow_OUT, OutRun->tOffset, p
    );
#endif

    /* total transpiration */
    ForEachSoilLayer(i, n_layers) {
#if defined(RSOILWAT)
        iOUTIndex = iOUT(
            i, OutRun->irow_OUT[pd], OutDom->nrow_OUT[pd], ncol_TimeOUT[pd]
        );

#elif defined(SWNETCDF)
        iOUTIndex =
            OutDom->netCDFOutput.iOUToffset[eSW_Transp][pd][0] +
            iOUTnc(
                OutRun->irow_OUT[pd], i, 0, OutDom->nsl_OUT[eSW_Transp][0], 1
            );
#endif

        p[iOUTIndex] = vo->transp_total[i];
    }

#if defined(SWNETCDF)
    /* Set extra soil layers to missing/fill value (up to domain-wide max) */
    for (i = sw->Site.n_layers; i < OutDom->nsl_OUT[eSW_Transp][0]; i++) {
        iOUTIndex =
            OutDom->netCDFOutput.iOUToffset[eSW_Transp][pd][0] +
            iOUTnc(
                OutRun->irow_OUT[pd], i, 0, OutDom->nsl_OUT[eSW_Transp][0], 1
            );
        p[iOUTIndex] = NC_FILL_DOUBLE;
    }
#endif // SWNETCDF


    /* transpiration for each vegetation type */
    ForEachVegType(k) {
        ForEachSoilLayer(i, n_layers) {
#if defined(RSOILWAT)
            // k + 1 because of total transp.
            iOUTIndex = iOUT2(
                i, k + 1, pd, OutRun->irow_OUT, OutDom->nrow_OUT, n_layers
            );

#elif defined(SWNETCDF)
            iOUTIndex = OutDom->netCDFOutput.iOUToffset[eSW_Transp][pd][1] +
                        iOUTnc(
                            OutRun->irow_OUT[pd],
                            i,
                            k,
                            OutDom->nsl_OUT[eSW_Transp][1],
                            OutDom->npft_OUT[eSW_Transp][1]
                        );
#endif

            p[iOUTIndex] = vo->transp[k][i];
        }

#if defined(SWNETCDF)
        /* Set extra soil layers to missing/fill value (up to domain-wide max)
         */
        for (i = sw->Site.n_layers; i < OutDom->nsl_OUT[eSW_Transp][1]; i++) {
            iOUTIndex = OutDom->netCDFOutput.iOUToffset[eSW_Transp][pd][1] +
                        iOUTnc(
                            OutRun->irow_OUT[pd],
                            i,
                            k,
                            OutDom->nsl_OUT[eSW_Transp][1],
                            OutDom->npft_OUT[eSW_Transp][1]
                        );
            p[iOUTIndex] = NC_FILL_DOUBLE;
        }
#endif // SWNETCDF
    }
}

#elif defined(STEPWAT)

/**
@brief Gets transp_total when dealing with STEPWAT.

@param[in] pd Time period in simulation output (day/week/month/year)
@param[in] sw Comprehensive struct of type SW_RUN containing all information
    in the simulation.
@param[in] OutDom Struct of type SW_OUT_DOM that holds output
    information that do not change throughout simulation runs
@param[out] LogInfo Holds information on warnings and errors
*/
void get_transp_agg(
    OutPeriod pd, SW_RUN *sw, SW_OUT_DOM *OutDom, LOG_INFO *LogInfo
) {
    LyrIndex i;
    LyrIndex n_layers = sw->Site.n_layers;
    int k;
    SW_SOILWAT_OUTPUTS *vo = sw->SoilWat.p_oagg[pd];
    size_t iOUTIndex = 0;
    SW_OUT_RUN *OutRun = &sw->OutRun;

    double *p = OutRun->p_OUT[eSW_Transp][pd];
    double *psd = OutRun->p_OUTsd[eSW_Transp][pd];

    /* total transpiration */
    ForEachSoilLayer(i, n_layers) {
        iOUTIndex = iOUT(
            i, OutRun->irow_OUT[pd], OutDom->nrow_OUT[pd], ncol_TimeOUT[pd]
        );
        do_running_agg(
            p, psd, iOUTIndex, OutRun->currIter, vo->transp_total[i]
        );
    }

    if (OutDom->print_IterationSummary) {
        OutRun->sw_outstr_agg[0] = '\0';
        format_IterationSummary(
            p, psd, pd, n_layers, sw, OutDom->nrow_OUT, LogInfo
        );
        if (LogInfo->stopRun) {
            return;
        }
    }

    /* transpiration for each vegetation type */
    ForEachVegType(k) {
        ForEachSoilLayer(i, n_layers) {
            // k + 1 because of total transp.
            iOUTIndex = iOUT2(
                i, k + 1, pd, OutRun->irow_OUT, OutDom->nrow_OUT, n_layers
            );
            do_running_agg(
                p, psd, iOUTIndex, OutRun->currIter, vo->transp[k][i]
            );
        }
    }

    if (OutDom->print_IterationSummary) {
        format_IterationSummary2(
            p, psd, pd, NVEGTYPES, 1, sw, OutDom->nrow_OUT, LogInfo
        );
    }
}

/**
@brief STEPWAT2 expects monthly sum of transpiration by soil layer. <BR>
                                see function '_transp_contribution_by_group'

@param[in] pd Time period in simulation output (day/week/month/year)
@param[in] sw Comprehensive struct of type SW_RUN containing all information
    in the simulation.
@param[in] OutDom Struct of type SW_OUT_DOM that holds output
    information that do not change throughout simulation runs
@param[out] LogInfo Holds information on warnings and errors
*/
void get_transp_SXW(
    OutPeriod pd, SW_RUN *sw, SW_OUT_DOM *OutDom, LOG_INFO *LogInfo
) {
    TimeInt month;

    if (pd == eSW_Month) {
        LyrIndex i;
        int k;
        SW_SOILWAT_OUTPUTS *vo = sw->SoilWat.p_oagg[pd];
        SW_OUT_RUN *OutRun = &sw->OutRun;
        month = sw->Model.month - OutRun->tOffset;

        /* total transpiration */
        ForEachSoilLayer(i, sw->Site.n_layers) {
            OutRun->transpTotal[i][month] = vo->transp_total[i];
        }

        /* transpiration for each vegetation type */
        ForEachVegType(k) {
            ForEachSoilLayer(i, sw->Site.n_layers) {
                OutRun->transpVeg[k][i][month] = vo->transp[k][i];
            }
        }
    }

    (void) OutDom;
    (void) LogInfo;
}
#endif


//------ eSW_EvapSoil
#ifdef SW_OUTTEXT

/**
@brief Gets evap when dealing with OUTTEXT.

@brief pd Period.
@param[in] sw Comprehensive struct of type SW_RUN containing all information
    in the simulation.
@param[out] LogInfo Holds information on warnings and errors
*/
void get_evapSoil_text(OutPeriod pd, SW_RUN *sw, LOG_INFO *LogInfo) {
    LyrIndex i;
    SW_SOILWAT_OUTPUTS *vo = sw->SoilWat.p_oagg[pd];
    SW_OUT_RUN *OutRun = &sw->OutRun;
    size_t writeSize = (size_t) (MAX_LAYERS * OUTSTRLEN);
    char *writePtr = OutRun->sw_outstr;
    char *endOutstr = OutRun->sw_outstr + sizeof OutRun->sw_outstr - 1;
    Bool fullBuffer = swFALSE;

    char str[OUTSTRLEN];
    OutRun->sw_outstr[0] = '\0';

    ForEachEvapLayer(i, sw->Site.n_evap_lyrs) {
        (void) snprintf(
            str, OUTSTRLEN, "%c%.*f", OUTSEP, OUT_DIGITS, vo->evap_baresoil[i]
        );
        fullBuffer = sw_memccpy_inc(
            (void **) &writePtr, endOutstr, (void *) str, '\0', &writeSize
        );
        if (fullBuffer) {
            goto reportFullBuffer;
        }
    }

reportFullBuffer:
    if (fullBuffer) {
        reportFullBuffer(LOGERROR, LogInfo);
    }
}
#endif

#if defined(RSOILWAT) || defined(SWNETCDF)

/**
@brief Gets evap when dealing with RSOILWAT.

@param[in] pd Time period in simulation output (day/week/month/year)
@param[in] sw Comprehensive struct of type SW_RUN containing all information
    in the simulation.
@param[in] OutDom Struct of type SW_OUT_DOM that holds output
    information that do not change throughout simulation runs
*/
void get_evapSoil_mem(OutPeriod pd, SW_RUN *sw, SW_OUT_DOM *OutDom) {
    LyrIndex i;
    SW_SOILWAT_OUTPUTS *vo = sw->SoilWat.p_oagg[pd];
    size_t iOUTIndex = 0;
    SW_OUT_RUN *OutRun = &sw->OutRun;

    double *p = OutRun->p_OUT[eSW_EvapSoil][pd];

#if defined(RSOILWAT)
    get_outvalleader(
        &sw->Model, pd, OutRun->irow_OUT, OutDom->nrow_OUT, OutRun->tOffset, p
    );
#endif

    ForEachEvapLayer(i, sw->Site.n_evap_lyrs) {
#if defined(RSOILWAT)
        iOUTIndex = iOUT(
            i, OutRun->irow_OUT[pd], OutDom->nrow_OUT[pd], ncol_TimeOUT[pd]
        );

#elif defined(SWNETCDF)
        iOUTIndex =
            OutDom->netCDFOutput.iOUToffset[eSW_EvapSoil][pd][0] +
            iOUTnc(
                OutRun->irow_OUT[pd], i, 0, OutDom->nsl_OUT[eSW_EvapSoil][0], 1
            );
#endif

        p[iOUTIndex] = vo->evap_baresoil[i];
    }

#if defined(SWNETCDF)
    /* Set extra soil layers to missing/fill value (up to domain-wide max) */
    for (i = sw->Site.n_evap_lyrs; i < OutDom->nsl_OUT[eSW_EvapSoil][0]; i++) {
        iOUTIndex =
            OutDom->netCDFOutput.iOUToffset[eSW_EvapSoil][pd][0] +
            iOUTnc(
                OutRun->irow_OUT[pd], i, 0, OutDom->nsl_OUT[eSW_EvapSoil][0], 1
            );
        p[iOUTIndex] = NC_FILL_DOUBLE;
    }
#endif // SWNETCDF
}

#elif defined(STEPWAT)

/**
@brief Gets evap when dealing with STEPWAT.

@param[in] pd Time period in simulation output (day/week/month/year)
@param[in] sw Comprehensive struct of type SW_RUN containing all information
    in the simulation.
@param[in] OutDom Struct of type SW_OUT_DOM that holds output
    information that do not change throughout simulation runs
@param[out] LogInfo Holds information on warnings and errors
*/
void get_evapSoil_agg(
    OutPeriod pd, SW_RUN *sw, SW_OUT_DOM *OutDom, LOG_INFO *LogInfo
) {
    LyrIndex i;
    SW_SOILWAT_OUTPUTS *vo = sw->SoilWat.p_oagg[pd];
    size_t iOUTIndex = 0;
    SW_OUT_RUN *OutRun = &sw->OutRun;

    double *p = OutRun->p_OUT[eSW_EvapSoil][pd];
    double *psd = OutRun->p_OUTsd[eSW_EvapSoil][pd];

    ForEachEvapLayer(i, sw->Site.n_evap_lyrs) {
        iOUTIndex = iOUT(
            i, OutRun->irow_OUT[pd], OutDom->nrow_OUT[pd], ncol_TimeOUT[pd]
        );
        do_running_agg(
            p, psd, iOUTIndex, OutRun->currIter, vo->evap_baresoil[i]
        );
    }

    if (OutDom->print_IterationSummary) {
        OutRun->sw_outstr_agg[0] = '\0';
        format_IterationSummary(
            p,
            psd,
            pd,
            OutDom->ncol_OUT[eSW_EvapSoil],
            sw,
            OutDom->nrow_OUT,
            LogInfo
        );
    }
}
#endif


//------ eSW_EvapSurface
#ifdef SW_OUTTEXT

/**
@brief Gets evapSurface when dealing with OUTTEXT.

@param[in] pd Time period in simulation output (day/week/month/year)
@param[in] sw Comprehensive struct of type SW_RUN containing all information
    in the simulation.
@param[out] LogInfo Holds information on warnings and errors
*/
void get_evapSurface_text(OutPeriod pd, SW_RUN *sw, LOG_INFO *LogInfo) {
    int k;
    SW_SOILWAT_OUTPUTS *vo = sw->SoilWat.p_oagg[pd];
    SW_OUT_RUN *OutRun = &sw->OutRun;
    size_t writeSize = (size_t) (MAX_LAYERS * OUTSTRLEN);
    char *writePtr = OutRun->sw_outstr;
    char *endOutstr = OutRun->sw_outstr + sizeof OutRun->sw_outstr - 1;
    Bool fullBuffer = swFALSE;

    char str[OUTSTRLEN];
    OutRun->sw_outstr[0] = '\0';

    (void
    ) snprintf(str, sizeof str, "%c%.*f", OUTSEP, OUT_DIGITS, vo->total_evap);
    fullBuffer = sw_memccpy_inc(
        (void **) &writePtr, endOutstr, (void *) str, '\0', &writeSize
    );
    if (fullBuffer) {
        goto reportFullBuffer;
    }

    ForEachVegType(k) {
        (void) snprintf(
            str, OUTSTRLEN, "%c%.*f", OUTSEP, OUT_DIGITS, vo->evap_veg[k]
        );
        fullBuffer = sw_memccpy_inc(
            (void **) &writePtr, endOutstr, (void *) str, '\0', &writeSize
        );
        if (fullBuffer) {
            goto reportFullBuffer;
        }
    }

    (void) snprintf(
        str,
        OUTSTRLEN,
        "%c%.*f%c%.*f",
        OUTSEP,
        OUT_DIGITS,
        vo->litter_evap,
        OUTSEP,
        OUT_DIGITS,
        vo->surfaceWater_evap
    );
    fullBuffer = sw_memccpy_inc(
        (void **) &writePtr, endOutstr, (void *) str, '\0', &writeSize
    );

reportFullBuffer:
    if (fullBuffer) {
        reportFullBuffer(LOGERROR, LogInfo);
    }
}
#endif

#if defined(RSOILWAT) || defined(SWNETCDF)

/**
@brief Gets evapSurface when dealing with RSOILWAT.

@param[in] pd Time period in simulation output (day/week/month/year)
@param[in] sw Comprehensive struct of type SW_RUN containing all information
    in the simulation.
@param[in] OutDom Struct of type SW_OUT_DOM that holds output
    information that do not change throughout simulation runs
*/
void get_evapSurface_mem(OutPeriod pd, SW_RUN *sw, SW_OUT_DOM *OutDom) {
    int k;
    SW_SOILWAT_OUTPUTS *vo = sw->SoilWat.p_oagg[pd];
    size_t iOUTIndex = 0;
    SW_OUT_RUN *OutRun = &sw->OutRun;

    double *p = OutRun->p_OUT[eSW_EvapSurface][pd];

#if defined(RSOILWAT)
    get_outvalleader(
        &sw->Model, pd, OutRun->irow_OUT, OutDom->nrow_OUT, OutRun->tOffset, p
    );
#endif

#if defined(RSOILWAT)
    iOUTIndex =
        iOUT(0, OutRun->irow_OUT[pd], OutDom->nrow_OUT[pd], ncol_TimeOUT[pd]);

#elif defined(SWNETCDF)
    iOUTIndex = OutDom->netCDFOutput.iOUToffset[eSW_EvapSurface][pd][0] +
                iOUTnc(OutRun->irow_OUT[pd], 0, 0, 1, 1);
#endif

    p[iOUTIndex] = vo->total_evap;


    ForEachVegType(k) {
#if defined(RSOILWAT)
        iOUTIndex = iOUT(
            k + 1, OutRun->irow_OUT[pd], OutDom->nrow_OUT[pd], ncol_TimeOUT[pd]
        );

#elif defined(SWNETCDF)
        iOUTIndex = OutDom->netCDFOutput.iOUToffset[eSW_EvapSurface][pd][1] +
                    iOUTnc(
                        OutRun->irow_OUT[pd],
                        0,
                        k,
                        1,
                        OutDom->npft_OUT[eSW_EvapSurface][1]
                    );
#endif

        p[iOUTIndex] = vo->evap_veg[k];
    }


#if defined(RSOILWAT)
    iOUTIndex = iOUT(
        NVEGTYPES + 1,
        OutRun->irow_OUT[pd],
        OutDom->nrow_OUT[pd],
        ncol_TimeOUT[pd]
    );

#elif defined(SWNETCDF)
    iOUTIndex = OutDom->netCDFOutput.iOUToffset[eSW_EvapSurface][pd][2] +
                iOUTnc(OutRun->irow_OUT[pd], 0, 0, 1, 1);
#endif

    p[iOUTIndex] = vo->litter_evap;


#if defined(RSOILWAT)
    iOUTIndex = iOUT(
        NVEGTYPES + 2,
        OutRun->irow_OUT[pd],
        OutDom->nrow_OUT[pd],
        ncol_TimeOUT[pd]
    );

#elif defined(SWNETCDF)
    iOUTIndex = OutDom->netCDFOutput.iOUToffset[eSW_EvapSurface][pd][3] +
                iOUTnc(OutRun->irow_OUT[pd], 0, 0, 1, 1);
#endif

    p[iOUTIndex] = vo->surfaceWater_evap;
}

#elif defined(STEPWAT)

/**
@brief Gets evapSurface when dealing with STEPWAT.

@param[in] pd Time period in simulation output (day/week/month/year)
@param[in] sw Comprehensive struct of type SW_RUN containing all information
    in the simulation.
@param[in] OutDom Struct of type SW_OUT_DOM that holds output
    information that do not change throughout simulation runs
@param[out] LogInfo Holds information on warnings and errors
*/
void get_evapSurface_agg(
    OutPeriod pd, SW_RUN *sw, SW_OUT_DOM *OutDom, LOG_INFO *LogInfo
) {
    int k;
    SW_SOILWAT_OUTPUTS *vo = sw->SoilWat.p_oagg[pd];
    size_t iOUTIndex = 0;
    SW_OUT_RUN *OutRun = &sw->OutRun;

    double *p = OutRun->p_OUT[eSW_EvapSurface][pd];
    double *psd = OutRun->p_OUTsd[eSW_EvapSurface][pd];

    iOUTIndex =
        iOUT(0, OutRun->irow_OUT[pd], OutDom->nrow_OUT[pd], ncol_TimeOUT[pd]);
    do_running_agg(p, psd, iOUTIndex, OutRun->currIter, vo->total_evap);

    ForEachVegType(k) {
        iOUTIndex = iOUT(
            k + 1, OutRun->irow_OUT[pd], OutDom->nrow_OUT[pd], ncol_TimeOUT[pd]
        );
        do_running_agg(p, psd, iOUTIndex, OutRun->currIter, vo->evap_veg[k]);
    }

    iOUTIndex = iOUT(
        NVEGTYPES + 1,
        OutRun->irow_OUT[pd],
        OutDom->nrow_OUT[pd],
        ncol_TimeOUT[pd]
    );
    do_running_agg(p, psd, iOUTIndex, OutRun->currIter, vo->litter_evap);

    iOUTIndex = iOUT(
        NVEGTYPES + 2,
        OutRun->irow_OUT[pd],
        OutDom->nrow_OUT[pd],
        ncol_TimeOUT[pd]
    );
    do_running_agg(p, psd, iOUTIndex, OutRun->currIter, vo->surfaceWater_evap);

    if (OutDom->print_IterationSummary) {
        OutRun->sw_outstr_agg[0] = '\0';
        format_IterationSummary(
            p,
            psd,
            pd,
            OutDom->ncol_OUT[eSW_EvapSurface],
            sw,
            OutDom->nrow_OUT,
            LogInfo
        );
    }
}
#endif

//------ eSW_Interception
#ifdef SW_OUTTEXT

/**
@brief Gets total_int, int_veg, and litter_int when dealing with OUTTEXT.

@param[in] pd Time period in simulation output (day/week/month/year)
@param[in] sw Comprehensive struct of type SW_RUN containing all information
    in the simulation.
@param[out] LogInfo Holds information on warnings and errors
*/
void get_interception_text(OutPeriod pd, SW_RUN *sw, LOG_INFO *LogInfo) {
    int k;
    SW_SOILWAT_OUTPUTS *vo = sw->SoilWat.p_oagg[pd];
    SW_OUT_RUN *OutRun = &sw->OutRun;
    size_t writeSize = (size_t) (MAX_LAYERS * OUTSTRLEN);
    char *writePtr = OutRun->sw_outstr;
    char *endOutstr = OutRun->sw_outstr + sizeof OutRun->sw_outstr - 1;
    Bool fullBuffer = swFALSE;

    char str[OUTSTRLEN];
    OutRun->sw_outstr[0] = '\0';

    (void
    ) snprintf(str, sizeof str, "%c%.*f", OUTSEP, OUT_DIGITS, vo->total_int);
    fullBuffer = sw_memccpy_inc(
        (void **) &writePtr, endOutstr, (void *) str, '\0', &writeSize
    );
    if (fullBuffer) {
        goto reportFullBuffer;
    }

    ForEachVegType(k) {
        (void) snprintf(
            str, OUTSTRLEN, "%c%.*f", OUTSEP, OUT_DIGITS, vo->int_veg[k]
        );
        fullBuffer = sw_memccpy_inc(
            (void **) &writePtr, endOutstr, (void *) str, '\0', &writeSize
        );
        if (fullBuffer) {
            goto reportFullBuffer;
        }
    }

    (void
    ) snprintf(str, OUTSTRLEN, "%c%.*f", OUTSEP, OUT_DIGITS, vo->litter_int);
    fullBuffer = sw_memccpy_inc(
        (void **) &writePtr, endOutstr, (void *) str, '\0', &writeSize
    );

reportFullBuffer:
    if (fullBuffer) {
        reportFullBuffer(LOGERROR, LogInfo);
    }
}
#endif

#if defined(RSOILWAT) || defined(SWNETCDF)

/**
@brief Gets total_int, int_veg, and litter_int when dealing with RSOILWAT.

@param[in] pd Time period in simulation output (day/week/month/year)
@param[in] sw Comprehensive struct of type SW_RUN containing all information
    in the simulation.
@param[in] OutDom Struct of type SW_OUT_DOM that holds output
    information that do not change throughout simulation runs
*/
void get_interception_mem(OutPeriod pd, SW_RUN *sw, SW_OUT_DOM *OutDom) {
    int k;
    SW_SOILWAT_OUTPUTS *vo = sw->SoilWat.p_oagg[pd];
    size_t iOUTIndex = 0;
    SW_OUT_RUN *OutRun = &sw->OutRun;

    double *p = OutRun->p_OUT[eSW_Interception][pd];

#if defined(RSOILWAT)
    get_outvalleader(
        &sw->Model, pd, OutRun->irow_OUT, OutDom->nrow_OUT, OutRun->tOffset, p
    );
#endif

#if defined(RSOILWAT)
    iOUTIndex =
        iOUT(0, OutRun->irow_OUT[pd], OutDom->nrow_OUT[pd], ncol_TimeOUT[pd]);

#elif defined(SWNETCDF)
    iOUTIndex = OutDom->netCDFOutput.iOUToffset[eSW_Interception][pd][0] +
                iOUTnc(OutRun->irow_OUT[pd], 0, 0, 1, 1);
#endif

    p[iOUTIndex] = vo->total_int;


    ForEachVegType(k) {
#if defined(RSOILWAT)
        iOUTIndex = iOUT(
            k + 1, OutRun->irow_OUT[pd], OutDom->nrow_OUT[pd], ncol_TimeOUT[pd]
        );

#elif defined(SWNETCDF)
        iOUTIndex = OutDom->netCDFOutput.iOUToffset[eSW_Interception][pd][1] +
                    iOUTnc(
                        OutRun->irow_OUT[pd],
                        0,
                        k,
                        1,
                        OutDom->npft_OUT[eSW_Interception][1]
                    );
#endif

        p[iOUTIndex] = vo->int_veg[k];
    }


#if defined(RSOILWAT)
    iOUTIndex = iOUT(
        NVEGTYPES + 1,
        OutRun->irow_OUT[pd],
        OutDom->nrow_OUT[pd],
        ncol_TimeOUT[pd]
    );

#elif defined(SWNETCDF)
    iOUTIndex = OutDom->netCDFOutput.iOUToffset[eSW_Interception][pd][2] +
                iOUTnc(OutRun->irow_OUT[pd], 0, 0, 1, 1);
#endif

    p[iOUTIndex] = vo->litter_int;
}

#elif defined(STEPWAT)

/**
@brief Gets total_int, int_veg, and litter_int when dealing with STEPWAT.

@param[in] pd Time period in simulation output (day/week/month/year)
@param[in] sw Comprehensive struct of type SW_RUN containing all information
    in the simulation.
@param[in] OutDom Struct of type SW_OUT_DOM that holds output
    information that do not change throughout simulation runs
@param[out] LogInfo Holds information on warnings and errors
*/
void get_interception_agg(
    OutPeriod pd, SW_RUN *sw, SW_OUT_DOM *OutDom, LOG_INFO *LogInfo
) {
    int k;
    SW_SOILWAT_OUTPUTS *vo = sw->SoilWat.p_oagg[pd];
    size_t iOUTIndex = 0;
    SW_OUT_RUN *OutRun = &sw->OutRun;

    double *p = OutRun->p_OUT[eSW_Interception][pd];
    double *psd = OutRun->p_OUTsd[eSW_Interception][pd];

    iOUTIndex =
        iOUT(0, OutRun->irow_OUT[pd], OutDom->nrow_OUT[pd], ncol_TimeOUT[pd]);
    do_running_agg(p, psd, iOUTIndex, OutRun->currIter, vo->total_int);

    ForEachVegType(k) {
        iOUTIndex = iOUT(
            k + 1, OutRun->irow_OUT[pd], OutDom->nrow_OUT[pd], ncol_TimeOUT[pd]
        );
        do_running_agg(p, psd, iOUTIndex, OutRun->currIter, vo->int_veg[k]);
    }

    iOUTIndex = iOUT(
        NVEGTYPES + 1,
        OutRun->irow_OUT[pd],
        OutDom->nrow_OUT[pd],
        ncol_TimeOUT[pd]
    );
    do_running_agg(p, psd, iOUTIndex, OutRun->currIter, vo->litter_int);

    if (OutDom->print_IterationSummary) {
        OutRun->sw_outstr_agg[0] = '\0';
        format_IterationSummary(
            p,
            psd,
            pd,
            OutDom->ncol_OUT[eSW_Interception],
            sw,
            OutDom->nrow_OUT,
            LogInfo
        );
    }
}
#endif

//------ eSW_SoilInf
#ifdef SW_OUTTEXT

/**
@brief Gets soil_inf when dealing with OUTTEXT.

@param[in] pd Time period in simulation output (day/week/month/year)
@param[in] sw Comprehensive struct of type SW_RUN containing all information
    in the simulation.
@param[out] LogInfo Holds information on warnings and errors
*/
void get_soilinf_text(OutPeriod pd, SW_RUN *sw, LOG_INFO *LogInfo) {
    /* 20100202 (drs) added */
    /* 20110219 (drs) added runoff */
    /* 12/13/2012	(clk)	moved runoff, now named snowRunoff, to
     * get_runoffrunon(); */
    SW_WEATHER_OUTPUTS *vo = sw->Weather.p_oagg[pd];
    SW_OUT_RUN *OutRun = &sw->OutRun;

    OutRun->sw_outstr[0] = '\0';
    (void) snprintf(
        OutRun->sw_outstr,
        sizeof OutRun->sw_outstr,
        "%c%.*f",
        OUTSEP,
        OUT_DIGITS,
        vo->soil_inf
    );

    (void) LogInfo;
}
#endif

#if defined(RSOILWAT) || defined(SWNETCDF)

/**
@brief Gets soil_inf when dealing with RSOILWAT.

@param[in] pd Time period in simulation output (day/week/month/year)
@param[in] sw Comprehensive struct of type SW_RUN containing all information
    in the simulation.
@param[in] OutDom Struct of type SW_OUT_DOM that holds output
    information that do not change throughout simulation runs
*/
void get_soilinf_mem(OutPeriod pd, SW_RUN *sw, SW_OUT_DOM *OutDom) {
    SW_WEATHER_OUTPUTS *vo = sw->Weather.p_oagg[pd];
    size_t iOUTIndex = 0;
    SW_OUT_RUN *OutRun = &sw->OutRun;

    double *p = OutRun->p_OUT[eSW_SoilInf][pd];

#if defined(RSOILWAT)
    get_outvalleader(
        &sw->Model, pd, OutRun->irow_OUT, OutDom->nrow_OUT, OutRun->tOffset, p
    );
#endif

#if defined(RSOILWAT)
    iOUTIndex =
        iOUT(0, OutRun->irow_OUT[pd], OutDom->nrow_OUT[pd], ncol_TimeOUT[pd]);

#elif defined(SWNETCDF)
    iOUTIndex = OutDom->netCDFOutput.iOUToffset[eSW_SoilInf][pd][0] +
                iOUTnc(OutRun->irow_OUT[pd], 0, 0, 1, 1);
#endif

    p[iOUTIndex] = vo->soil_inf;
}

#elif defined(STEPWAT)

/**
@brief Gets soil_inf when dealing with STEPWAT.

@param[in] pd Time period in simulation output (day/week/month/year)
@param[in] sw Comprehensive struct of type SW_RUN containing all information
    in the simulation.
@param[in] OutDom Struct of type SW_OUT_DOM that holds output
    information that do not change throughout simulation runs
@param[out] LogInfo Holds information on warnings and errors
*/
void get_soilinf_agg(
    OutPeriod pd, SW_RUN *sw, SW_OUT_DOM *OutDom, LOG_INFO *LogInfo
) {
    SW_WEATHER_OUTPUTS *vo = sw->Weather.p_oagg[pd];
    size_t iOUTIndex = 0;
    SW_OUT_RUN *OutRun = &sw->OutRun;

    double *p = OutRun->p_OUT[eSW_SoilInf][pd];
    double *psd = OutRun->p_OUTsd[eSW_SoilInf][pd];

    iOUTIndex =
        iOUT(0, OutRun->irow_OUT[pd], OutDom->nrow_OUT[pd], ncol_TimeOUT[pd]);
    do_running_agg(p, psd, iOUTIndex, OutRun->currIter, vo->soil_inf);

    if (OutDom->print_IterationSummary) {
        OutRun->sw_outstr_agg[0] = '\0';
        format_IterationSummary(
            p,
            psd,
            pd,
            OutDom->ncol_OUT[eSW_SoilInf],
            sw,
            OutDom->nrow_OUT,
            LogInfo
        );
    }
}
#endif


//------ eSW_LyrDrain
#ifdef SW_OUTTEXT

/**
@brief Gets lyrdrain when dealing with OUTTEXT.

@param[in] pd Time period in simulation output (day/week/month/year)
@param[in] sw Comprehensive struct of type SW_RUN containing all information
    in the simulation.
@param[out] LogInfo Holds information on warnings and errors
*/
void get_lyrdrain_text(OutPeriod pd, SW_RUN *sw, LOG_INFO *LogInfo) {
    /* 20100202 (drs) added */
    LyrIndex i;
    SW_SOILWAT_OUTPUTS *vo = sw->SoilWat.p_oagg[pd];
    SW_OUT_RUN *OutRun = &sw->OutRun;
    size_t writeSize = (size_t) (MAX_LAYERS * OUTSTRLEN);
    char *writePtr = OutRun->sw_outstr;
    char *endOutstr = OutRun->sw_outstr + sizeof OutRun->sw_outstr - 1;
    Bool fullBuffer = swFALSE;

    char str[OUTSTRLEN];
    OutRun->sw_outstr[0] = '\0';

    for (i = 0; i < sw->Site.n_layers - 1; i++) {
        (void) snprintf(
            str, OUTSTRLEN, "%c%.*f", OUTSEP, OUT_DIGITS, vo->lyrdrain[i]
        );
        fullBuffer = sw_memccpy_inc(
            (void **) &writePtr, endOutstr, (void *) str, '\0', &writeSize
        );
        if (fullBuffer) {
            goto reportFullBuffer;
        }
    }

reportFullBuffer:
    if (fullBuffer) {
        reportFullBuffer(LOGERROR, LogInfo);
    }
}
#endif

#if defined(RSOILWAT) || defined(SWNETCDF)

/**
@brief Gets lyrdrain when dealing with RSOILWAT.

@param[in] pd Time period in simulation output (day/week/month/year)
@param[in] sw Comprehensive struct of type SW_RUN containing all information
    in the simulation.
@param[in] OutDom Struct of type SW_OUT_DOM that holds output
    information that do not change throughout simulation runs
*/
void get_lyrdrain_mem(OutPeriod pd, SW_RUN *sw, SW_OUT_DOM *OutDom) {
    LyrIndex i;
    SW_SOILWAT_OUTPUTS *vo = sw->SoilWat.p_oagg[pd];
    size_t iOUTIndex = 0;
    SW_OUT_RUN *OutRun = &sw->OutRun;

    double *p = OutRun->p_OUT[eSW_LyrDrain][pd];

#if defined(RSOILWAT)
    get_outvalleader(
        &sw->Model, pd, OutRun->irow_OUT, OutDom->nrow_OUT, OutRun->tOffset, p
    );
#endif

    for (i = 0; i < sw->Site.n_layers - 1; i++) {
#if defined(RSOILWAT)
        iOUTIndex = iOUT(
            i, OutRun->irow_OUT[pd], OutDom->nrow_OUT[pd], ncol_TimeOUT[pd]
        );

#elif defined(SWNETCDF)
        iOUTIndex =
            OutDom->netCDFOutput.iOUToffset[eSW_LyrDrain][pd][0] +
            iOUTnc(
                OutRun->irow_OUT[pd], i, 0, OutDom->nsl_OUT[eSW_LyrDrain][0], 1
            );
#endif

        p[iOUTIndex] = vo->lyrdrain[i];
    }

#if defined(SWNETCDF)
    /* Set extra soil layers to missing/fill value (up to domain-wide max)
     */
    for (i = sw->Site.n_layers - 1; i < OutDom->nsl_OUT[eSW_LyrDrain][0]; i++) {
        iOUTIndex =
            OutDom->netCDFOutput.iOUToffset[eSW_LyrDrain][pd][0] +
            iOUTnc(
                OutRun->irow_OUT[pd], i, 0, OutDom->nsl_OUT[eSW_LyrDrain][0], 1
            );
        p[iOUTIndex] = NC_FILL_DOUBLE;
    }
#endif // SWNETCDF
}

#elif defined(STEPWAT)

/**
@brief Gets lyrdrain when dealing with STEPWAT.

@param[in] pd Time period in simulation output (day/week/month/year)
@param[in] sw Comprehensive struct of type SW_RUN containing all information
    in the simulation.
@param[in] OutDom Struct of type SW_OUT_DOM that holds output
    information that do not change throughout simulation runs
@param[out] LogInfo Holds information on warnings and errors
*/
void get_lyrdrain_agg(
    OutPeriod pd, SW_RUN *sw, SW_OUT_DOM *OutDom, LOG_INFO *LogInfo
) {
    LyrIndex i;
    SW_SOILWAT_OUTPUTS *vo = sw->SoilWat.p_oagg[pd];
    size_t iOUTIndex = 0;
    SW_OUT_RUN *OutRun = &sw->OutRun;

    double *p = OutRun->p_OUT[eSW_LyrDrain][pd];
    double *psd = OutRun->p_OUTsd[eSW_LyrDrain][pd];

    for (i = 0; i < sw->Site.n_layers - 1; i++) {
        iOUTIndex = iOUT(
            i, OutRun->irow_OUT[pd], OutDom->nrow_OUT[pd], ncol_TimeOUT[pd]
        );
        do_running_agg(p, psd, iOUTIndex, OutRun->currIter, vo->lyrdrain[i]);
    }

    if (OutDom->print_IterationSummary) {
        OutRun->sw_outstr_agg[0] = '\0';
        format_IterationSummary(
            p,
            psd,
            pd,
            OutDom->ncol_OUT[eSW_LyrDrain],
            sw,
            OutDom->nrow_OUT,
            LogInfo
        );
    }
}
#endif


//------ eSW_HydRed
#ifdef SW_OUTTEXT

/**
@brief Gets hydred and hydred_total when dealing with OUTTEXT.

@param[in] pd Time period in simulation output (day/week/month/year)
@param[in] sw Comprehensive struct of type SW_RUN containing all information
    in the simulation.
@param[out] LogInfo Holds information on warnings and errors
*/
void get_hydred_text(OutPeriod pd, SW_RUN *sw, LOG_INFO *LogInfo) {
    /* 20101020 (drs) added */
    LyrIndex i;
    LyrIndex n_layers = sw->Site.n_layers;
    int k;
    SW_SOILWAT_OUTPUTS *vo = sw->SoilWat.p_oagg[pd];
    SW_OUT_RUN *OutRun = &sw->OutRun;
    size_t writeSize = (size_t) (MAX_LAYERS * OUTSTRLEN);
    char *writePtr = OutRun->sw_outstr;
    char *endOutstr = OutRun->sw_outstr + sizeof OutRun->sw_outstr - 1;
    Bool fullBuffer = swFALSE;

    char str[OUTSTRLEN];
    OutRun->sw_outstr[0] = '\0';

    /* total hydraulic redistribution */
    ForEachSoilLayer(i, n_layers) {
        (void) snprintf(
            str, OUTSTRLEN, "%c%.*f", OUTSEP, OUT_DIGITS, vo->hydred_total[i]
        );
        fullBuffer = sw_memccpy_inc(
            (void **) &writePtr, endOutstr, (void *) str, '\0', &writeSize
        );
        if (fullBuffer) {
            goto reportFullBuffer;
        }
    }

    /* hydraulic redistribution for each vegetation type */
    ForEachVegType(k) {
        ForEachSoilLayer(i, n_layers) {
            (void) snprintf(
                str, OUTSTRLEN, "%c%.*f", OUTSEP, OUT_DIGITS, vo->hydred[k][i]
            );
            fullBuffer = sw_memccpy_inc(
                (void **) &writePtr, endOutstr, (void *) str, '\0', &writeSize
            );
            if (fullBuffer) {
                goto reportFullBuffer;
            }
        }
    }

reportFullBuffer:
    if (fullBuffer) {
        reportFullBuffer(LOGERROR, LogInfo);
    }
}
#endif

#if defined(RSOILWAT) || defined(SWNETCDF)

/**
@brief Gets hydred and hydred_total when dealing with RSOILWAT.

@param[in] pd Time period in simulation output (day/week/month/year)
@param[in] sw Comprehensive struct of type SW_RUN containing all information
    in the simulation.
@param[in] OutDom Struct of type SW_OUT_DOM that holds output
    information that do not change throughout simulation runs
*/
void get_hydred_mem(OutPeriod pd, SW_RUN *sw, SW_OUT_DOM *OutDom) {
    LyrIndex i;
    LyrIndex n_layers = sw->Site.n_layers;
    int k;
    SW_SOILWAT_OUTPUTS *vo = sw->SoilWat.p_oagg[pd];
    size_t iOUTIndex = 0;
    SW_OUT_RUN *OutRun = &sw->OutRun;

    double *p = OutRun->p_OUT[eSW_HydRed][pd];

#if defined(RSOILWAT)
    get_outvalleader(
        &sw->Model, pd, OutRun->irow_OUT, OutDom->nrow_OUT, OutRun->tOffset, p
    );
#endif

    /* total hydraulic redistribution */
    ForEachSoilLayer(i, n_layers) {
#if defined(RSOILWAT)
        iOUTIndex = iOUT(
            i, OutRun->irow_OUT[pd], OutDom->nrow_OUT[pd], ncol_TimeOUT[pd]
        );

#elif defined(SWNETCDF)
        iOUTIndex =
            OutDom->netCDFOutput.iOUToffset[eSW_HydRed][pd][0] +
            iOUTnc(
                OutRun->irow_OUT[pd], i, 0, OutDom->nsl_OUT[eSW_HydRed][0], 1
            );
#endif

        p[iOUTIndex] = vo->hydred_total[i];
    }

#if defined(SWNETCDF)
    /* Set extra soil layers to missing/fill value (up to domain-wide max)
     */
    for (i = sw->Site.n_layers; i < OutDom->nsl_OUT[eSW_HydRed][0]; i++) {
        iOUTIndex =
            OutDom->netCDFOutput.iOUToffset[eSW_HydRed][pd][0] +
            iOUTnc(
                OutRun->irow_OUT[pd], i, 0, OutDom->nsl_OUT[eSW_HydRed][0], 1
            );
        p[iOUTIndex] = NC_FILL_DOUBLE;
    }
#endif // SWNETCDF


    /* hydraulic redistribution for each vegetation type */
    ForEachVegType(k) {
        ForEachSoilLayer(i, n_layers) {
#if defined(RSOILWAT)
            // k + 1 because of total hydred
            iOUTIndex = iOUT2(
                i,
                k + 1,
                pd,
                OutRun->irow_OUT,
                OutDom->nrow_OUT,
                sw->Site.n_layers
            );

#elif defined(SWNETCDF)
            iOUTIndex = OutDom->netCDFOutput.iOUToffset[eSW_HydRed][pd][1] +
                        iOUTnc(
                            OutRun->irow_OUT[pd],
                            i,
                            k,
                            OutDom->nsl_OUT[eSW_HydRed][1],
                            OutDom->npft_OUT[eSW_HydRed][1]
                        );
#endif

            p[iOUTIndex] = vo->hydred[k][i];
        }

#if defined(SWNETCDF)
        /* Set extra soil layers to missing/fill value (up to domain-wide
         * max)
         */
        for (i = sw->Site.n_layers; i < OutDom->nsl_OUT[eSW_HydRed][0]; i++) {
            iOUTIndex = OutDom->netCDFOutput.iOUToffset[eSW_HydRed][pd][1] +
                        iOUTnc(
                            OutRun->irow_OUT[pd],
                            i,
                            k,
                            OutDom->nsl_OUT[eSW_HydRed][1],
                            OutDom->npft_OUT[eSW_HydRed][1]
                        );
            p[iOUTIndex] = NC_FILL_DOUBLE;
        }
#endif // SWNETCDF
    }
}

#elif defined(STEPWAT)

/**
@brief Gets hydred and hydred_total when dealing with STEPWAT.

@param[in] pd Time period in simulation output (day/week/month/year)
@param[in] sw Comprehensive struct of type SW_RUN containing all information
    in the simulation.
@param[in] OutDom Struct of type SW_OUT_DOM that holds output
    information that do not change throughout simulation runs
@param[out] LogInfo Holds information on warnings and errors
*/
void get_hydred_agg(
    OutPeriod pd, SW_RUN *sw, SW_OUT_DOM *OutDom, LOG_INFO *LogInfo
) {
    LyrIndex i;
    LyrIndex n_layers = sw->Site.n_layers;
    int k;
    SW_SOILWAT_OUTPUTS *vo = sw->SoilWat.p_oagg[pd];
    size_t iOUTIndex = 0;
    SW_OUT_RUN *OutRun = &sw->OutRun;

    double *p = OutRun->p_OUT[eSW_HydRed][pd];
    double *psd = OutRun->p_OUTsd[eSW_HydRed][pd];

    /* total hydraulic redistribution */
    ForEachSoilLayer(i, n_layers) {
        iOUTIndex = iOUT(
            i, OutRun->irow_OUT[pd], OutDom->nrow_OUT[pd], ncol_TimeOUT[pd]
        );
        do_running_agg(
            p, psd, iOUTIndex, OutRun->currIter, vo->hydred_total[i]
        );
    }

    if (OutDom->print_IterationSummary) {
        OutRun->sw_outstr_agg[0] = '\0';
        format_IterationSummary(
            p, psd, pd, n_layers, sw, OutDom->nrow_OUT, LogInfo
        );
        if (LogInfo->stopRun) {
            return;
        }
    }

    /* hydraulic redistribution for each vegetation type */
    ForEachVegType(k) {
        ForEachSoilLayer(i, n_layers) {
            // k + 1 because of total hydred
            iOUTIndex = iOUT2(
                i, k + 1, pd, OutRun->irow_OUT, OutDom->nrow_OUT, n_layers
            );
            do_running_agg(
                p, psd, iOUTIndex, OutRun->currIter, vo->hydred[k][i]
            );
        }
    }

    if (OutDom->print_IterationSummary) {
        format_IterationSummary2(
            p, psd, pd, NVEGTYPES, 1, sw, OutDom->nrow_OUT, LogInfo
        );
    }
}
#endif


//------ eSW_AET
#ifdef SW_OUTTEXT

/**
@brief Gets actual evapotranspiration when dealing with OUTTEXT.

@param[in] pd Time period in simulation output (day/week/month/year)
@param[in] sw Comprehensive struct of type SW_RUN containing all information
    in the simulation.
@param[out] LogInfo Holds information on warnings and errors
*/
void get_aet_text(OutPeriod pd, SW_RUN *sw, LOG_INFO *LogInfo) {
    SW_SOILWAT_OUTPUTS *vo = sw->SoilWat.p_oagg[pd];
    SW_WEATHER_OUTPUTS *vo2 = sw->Weather.p_oagg[pd];
    SW_OUT_RUN *OutRun = &sw->OutRun;

    OutRun->sw_outstr[0] = '\0';
    (void) snprintf(
        OutRun->sw_outstr,
        sizeof OutRun->sw_outstr,
        "%c%.*f%c%.*f%c%.*f%c%.*f%c%.*f%c%.*f",
        OUTSEP,
        OUT_DIGITS,
        vo->aet,
        OUTSEP,
        OUT_DIGITS,
        vo->tran,
        OUTSEP,
        OUT_DIGITS,
        vo->esoil,
        OUTSEP,
        OUT_DIGITS,
        vo->ecnw,
        OUTSEP,
        OUT_DIGITS,
        vo->esurf,
        OUTSEP,
        OUT_DIGITS,
        vo2->snowloss // should be `vo->esnow`
    );

    (void) LogInfo;
}
#endif

#if defined(RSOILWAT) || defined(SWNETCDF)

/**
@brief Gets actual evapotranspiration when dealing with RSOILWAT.

@param[in] pd Time period in simulation output (day/week/month/year)
@param[in] sw Comprehensive struct of type SW_RUN containing all information
    in the simulation.
@param[in] OutDom Struct of type SW_OUT_DOM that holds output
    information that do not change throughout simulation runs
*/
void get_aet_mem(OutPeriod pd, SW_RUN *sw, SW_OUT_DOM *OutDom) {
    SW_SOILWAT_OUTPUTS *vo = sw->SoilWat.p_oagg[pd];
    SW_WEATHER_OUTPUTS *vo2 = sw->Weather.p_oagg[pd];
    size_t iOUTIndex = 0;
    SW_OUT_RUN *OutRun = &sw->OutRun;

    double *p = OutRun->p_OUT[eSW_AET][pd];

#if defined(RSOILWAT)
    get_outvalleader(
        &sw->Model, pd, OutRun->irow_OUT, OutDom->nrow_OUT, OutRun->tOffset, p
    );
#endif

#if defined(RSOILWAT)
    iOUTIndex =
        iOUT(0, OutRun->irow_OUT[pd], OutDom->nrow_OUT[pd], ncol_TimeOUT[pd]);

#elif defined(SWNETCDF)
    iOUTIndex = OutDom->netCDFOutput.iOUToffset[eSW_AET][pd][0] +
                iOUTnc(OutRun->irow_OUT[pd], 0, 0, 1, 1);
#endif

    p[iOUTIndex] = vo->aet;


#if defined(RSOILWAT)
    iOUTIndex =
        iOUT(1, OutRun->irow_OUT[pd], OutDom->nrow_OUT[pd], ncol_TimeOUT[pd]);

#elif defined(SWNETCDF)
    iOUTIndex = OutDom->netCDFOutput.iOUToffset[eSW_AET][pd][1] +
                iOUTnc(OutRun->irow_OUT[pd], 0, 0, 1, 1);
#endif

    p[iOUTIndex] = vo->tran;


#if defined(RSOILWAT)
    iOUTIndex =
        iOUT(2, OutRun->irow_OUT[pd], OutDom->nrow_OUT[pd], ncol_TimeOUT[pd]);

#elif defined(SWNETCDF)
    iOUTIndex = OutDom->netCDFOutput.iOUToffset[eSW_AET][pd][2] +
                iOUTnc(OutRun->irow_OUT[pd], 0, 0, 1, 1);
#endif

    p[iOUTIndex] = vo->esoil;


#if defined(RSOILWAT)
    iOUTIndex =
        iOUT(3, OutRun->irow_OUT[pd], OutDom->nrow_OUT[pd], ncol_TimeOUT[pd]);

#elif defined(SWNETCDF)
    iOUTIndex = OutDom->netCDFOutput.iOUToffset[eSW_AET][pd][3] +
                iOUTnc(OutRun->irow_OUT[pd], 0, 0, 1, 1);
#endif

    p[iOUTIndex] = vo->ecnw;


#if defined(RSOILWAT)
    iOUTIndex =
        iOUT(4, OutRun->irow_OUT[pd], OutDom->nrow_OUT[pd], ncol_TimeOUT[pd]);

#elif defined(SWNETCDF)
    iOUTIndex = OutDom->netCDFOutput.iOUToffset[eSW_AET][pd][4] +
                iOUTnc(OutRun->irow_OUT[pd], 0, 0, 1, 1);
#endif

    p[iOUTIndex] = vo->esurf;


#if defined(RSOILWAT)
    iOUTIndex =
        iOUT(5, OutRun->irow_OUT[pd], OutDom->nrow_OUT[pd], ncol_TimeOUT[pd]);

#elif defined(SWNETCDF)
    iOUTIndex = OutDom->netCDFOutput.iOUToffset[eSW_AET][pd][5] +
                iOUTnc(OutRun->irow_OUT[pd], 0, 0, 1, 1);
#endif

    p[iOUTIndex] = vo2->snowloss; // should be `vo->esnow`
}

#elif defined(STEPWAT)

/**
@brief Gets actual evapotranspiration when dealing with STEPWAT.

@param[in] pd Time period in simulation output (day/week/month/year)
@param[in] sw Comprehensive struct of type SW_RUN containing all information
    in the simulation.
@param[in] OutDom Struct of type SW_OUT_DOM that holds output
    information that do not change throughout simulation runs
@param[out] LogInfo Holds information on warnings and errors
*/
void get_aet_agg(
    OutPeriod pd, SW_RUN *sw, SW_OUT_DOM *OutDom, LOG_INFO *LogInfo
) {
    SW_SOILWAT_OUTPUTS *vo = sw->SoilWat.p_oagg[pd];
    SW_WEATHER_OUTPUTS *vo2 = sw->Weather.p_oagg[pd];
    size_t iOUTIndex = 0;
    SW_OUT_RUN *OutRun = &sw->OutRun;

    double *p = OutRun->p_OUT[eSW_AET][pd];
    double *psd = OutRun->p_OUTsd[eSW_AET][pd];

    iOUTIndex =
        iOUT(0, OutRun->irow_OUT[pd], OutDom->nrow_OUT[pd], ncol_TimeOUT[pd]);
    do_running_agg(p, psd, iOUTIndex, OutRun->currIter, vo->aet);

    iOUTIndex =
        iOUT(1, OutRun->irow_OUT[pd], OutDom->nrow_OUT[pd], ncol_TimeOUT[pd]);
    do_running_agg(p, psd, iOUTIndex, OutRun->currIter, vo->tran);

    iOUTIndex =
        iOUT(2, OutRun->irow_OUT[pd], OutDom->nrow_OUT[pd], ncol_TimeOUT[pd]);
    do_running_agg(p, psd, iOUTIndex, OutRun->currIter, vo->esoil);

    iOUTIndex =
        iOUT(3, OutRun->irow_OUT[pd], OutDom->nrow_OUT[pd], ncol_TimeOUT[pd]);
    do_running_agg(p, psd, iOUTIndex, OutRun->currIter, vo->ecnw);

    iOUTIndex =
        iOUT(4, OutRun->irow_OUT[pd], OutDom->nrow_OUT[pd], ncol_TimeOUT[pd]);
    do_running_agg(p, psd, iOUTIndex, OutRun->currIter, vo->esurf);

    // should be `vo->esnow`
    iOUTIndex =
        iOUT(5, OutRun->irow_OUT[pd], OutDom->nrow_OUT[pd], ncol_TimeOUT[pd]);
    do_running_agg(p, psd, iOUTIndex, OutRun->currIter, vo2->snowloss);

    if (OutDom->print_IterationSummary) {
        OutRun->sw_outstr_agg[0] = '\0';
        format_IterationSummary(
            p, psd, pd, OutDom->ncol_OUT[eSW_AET], sw, OutDom->nrow_OUT, LogInfo
        );
    }
}

/**
@brief STEPWAT2 expects annual sum of actual evapotranspiration

@param[in] pd Time period in simulation output (day/week/month/year)
@param[in] sw Comprehensive struct of type SW_RUN containing all information
    in the simulation.
@param[in] OutDom Struct of type SW_OUT_DOM that holds output
    information that do not change throughout simulation runs
@param[out] LogInfo Holds information on warnings and errors
*/
void get_aet_SXW(
    OutPeriod pd, SW_RUN *sw, SW_OUT_DOM *OutDom, LOG_INFO *LogInfo
) {
    if (pd == eSW_Year) {
        SW_SOILWAT_OUTPUTS *vo = sw->SoilWat.p_oagg[pd];
        SW_OUT_RUN *OutRun = &sw->OutRun;

        OutRun->aet = vo->aet;
    }

    (void) OutDom;
    (void) LogInfo;
}
#endif


//------ eSW_PET
#ifdef SW_OUTTEXT

/**
@brief Gets potential evapotranspiration and radiation when dealing with
OUTTEXT

@param[in] pd Time period in simulation output (day/week/month/year)
@param[in] sw Comprehensive struct of type SW_RUN containing all information
    in the simulation.
@param[out] LogInfo Holds information on warnings and errors
*/
void get_pet_text(OutPeriod pd, SW_RUN *sw, LOG_INFO *LogInfo) {
    SW_SOILWAT_OUTPUTS *vo = sw->SoilWat.p_oagg[pd];
    SW_OUT_RUN *OutRun = &sw->OutRun;

    OutRun->sw_outstr[0] = '\0';
    (void) snprintf(
        OutRun->sw_outstr,
        sizeof OutRun->sw_outstr,
        "%c%.*f%c%.*f%c%.*f%c%.*f%c%.*f",
        OUTSEP,
        OUT_DIGITS,
        vo->pet,
        OUTSEP,
        OUT_DIGITS,
        vo->H_oh,
        OUTSEP,
        OUT_DIGITS,
        vo->H_ot,
        OUTSEP,
        OUT_DIGITS,
        vo->H_gh,
        OUTSEP,
        OUT_DIGITS,
        vo->H_gt
    );

    (void) LogInfo;
}
#endif

#if defined(RSOILWAT) || defined(SWNETCDF)

/**
@brief Gets potential evapotranspiration and radiation

@param[in] pd Time period in simulation output (day/week/month/year)
@param[in] sw Comprehensive struct of type SW_RUN containing all information
    in the simulation.
@param[in] OutDom Struct of type SW_OUT_DOM that holds output
    information that do not change throughout simulation runs
*/
void get_pet_mem(OutPeriod pd, SW_RUN *sw, SW_OUT_DOM *OutDom) {
    SW_SOILWAT_OUTPUTS *vo = sw->SoilWat.p_oagg[pd];
    size_t iOUTIndex = 0;
    SW_OUT_RUN *OutRun = &sw->OutRun;

    double *p = OutRun->p_OUT[eSW_PET][pd];

#if defined(RSOILWAT)
    get_outvalleader(
        &sw->Model, pd, OutRun->irow_OUT, OutDom->nrow_OUT, OutRun->tOffset, p
    );
#endif

#if defined(RSOILWAT)
    iOUTIndex =
        iOUT(0, OutRun->irow_OUT[pd], OutDom->nrow_OUT[pd], ncol_TimeOUT[pd]);

#elif defined(SWNETCDF)
    iOUTIndex = OutDom->netCDFOutput.iOUToffset[eSW_PET][pd][0] +
                iOUTnc(OutRun->irow_OUT[pd], 0, 0, 1, 1);
#endif

    p[iOUTIndex] = vo->pet;


#if defined(RSOILWAT)
    iOUTIndex =
        iOUT(1, OutRun->irow_OUT[pd], OutDom->nrow_OUT[pd], ncol_TimeOUT[pd]);

#elif defined(SWNETCDF)
    iOUTIndex = OutDom->netCDFOutput.iOUToffset[eSW_PET][pd][1] +
                iOUTnc(OutRun->irow_OUT[pd], 0, 0, 1, 1);
#endif

    p[iOUTIndex] = vo->H_oh;


#if defined(RSOILWAT)
    iOUTIndex =
        iOUT(2, OutRun->irow_OUT[pd], OutDom->nrow_OUT[pd], ncol_TimeOUT[pd]);

#elif defined(SWNETCDF)
    iOUTIndex = OutDom->netCDFOutput.iOUToffset[eSW_PET][pd][2] +
                iOUTnc(OutRun->irow_OUT[pd], 0, 0, 1, 1);
#endif

    p[iOUTIndex] = vo->H_ot;


#if defined(RSOILWAT)
    iOUTIndex =
        iOUT(3, OutRun->irow_OUT[pd], OutDom->nrow_OUT[pd], ncol_TimeOUT[pd]);

#elif defined(SWNETCDF)
    iOUTIndex = OutDom->netCDFOutput.iOUToffset[eSW_PET][pd][3] +
                iOUTnc(OutRun->irow_OUT[pd], 0, 0, 1, 1);
#endif

    p[iOUTIndex] = vo->H_gh;


#if defined(RSOILWAT)
    iOUTIndex =
        iOUT(4, OutRun->irow_OUT[pd], OutDom->nrow_OUT[pd], ncol_TimeOUT[pd]);

#elif defined(SWNETCDF)
    iOUTIndex = OutDom->netCDFOutput.iOUToffset[eSW_PET][pd][4] +
                iOUTnc(OutRun->irow_OUT[pd], 0, 0, 1, 1);
#endif

    p[iOUTIndex] = vo->H_gt;
}

#elif defined(STEPWAT)

/**
@brief Gets potential evapotranspiration and radiation
*/
void get_pet_agg(
    OutPeriod pd, SW_RUN *sw, SW_OUT_DOM *OutDom, LOG_INFO *LogInfo
) {
    SW_SOILWAT_OUTPUTS *vo = sw->SoilWat.p_oagg[pd];
    size_t iOUTIndex = 0;
    SW_OUT_RUN *OutRun = &sw->OutRun;

    double *p = OutRun->p_OUT[eSW_PET][pd];
    double *psd = OutRun->p_OUTsd[eSW_PET][pd];

    iOUTIndex =
        iOUT(0, OutRun->irow_OUT[pd], OutDom->nrow_OUT[pd], ncol_TimeOUT[pd]);
    do_running_agg(p, psd, iOUTIndex, OutRun->currIter, vo->pet);

    iOUTIndex =
        iOUT(1, OutRun->irow_OUT[pd], OutDom->nrow_OUT[pd], ncol_TimeOUT[pd]);
    do_running_agg(p, psd, iOUTIndex, OutRun->currIter, vo->H_oh);

    iOUTIndex =
        iOUT(2, OutRun->irow_OUT[pd], OutDom->nrow_OUT[pd], ncol_TimeOUT[pd]);
    do_running_agg(p, psd, iOUTIndex, OutRun->currIter, vo->H_ot);

    iOUTIndex =
        iOUT(3, OutRun->irow_OUT[pd], OutDom->nrow_OUT[pd], ncol_TimeOUT[pd]);
    do_running_agg(p, psd, iOUTIndex, OutRun->currIter, vo->H_gh);

    iOUTIndex =
        iOUT(4, OutRun->irow_OUT[pd], OutDom->nrow_OUT[pd], ncol_TimeOUT[pd]);
    do_running_agg(p, psd, iOUTIndex, OutRun->currIter, vo->H_gt);

    if (OutDom->print_IterationSummary) {
        OutRun->sw_outstr_agg[0] = '\0';
        format_IterationSummary(
            p, psd, pd, OutDom->ncol_OUT[eSW_PET], sw, OutDom->nrow_OUT, LogInfo
        );
    }
}
#endif


//------ eSW_WetDays
#ifdef SW_OUTTEXT

/**
@brief Gets is_wet and wetdays when dealing with OUTTEXT.

@param[in] pd Time period in simulation output (day/week/month/year)
@param[in] sw Comprehensive struct of type SW_RUN containing all information
    in the simulation.
@param[out] LogInfo Holds information on warnings and errors
*/
void get_wetdays_text(OutPeriod pd, SW_RUN *sw, LOG_INFO *LogInfo) {
    LyrIndex i;
    LyrIndex n_layers = sw->Site.n_layers;
    SW_OUT_RUN *OutRun = &sw->OutRun;
    size_t writeSize = (size_t) (MAX_LAYERS * OUTSTRLEN);
    char *writePtr = OutRun->sw_outstr;
    char *endOutstr = OutRun->sw_outstr + sizeof OutRun->sw_outstr - 1;
    Bool fullBuffer = swFALSE;

    char str[OUTSTRLEN];
    OutRun->sw_outstr[0] = '\0';

    if (pd == eSW_Day) {
        ForEachSoilLayer(i, n_layers) {
            (void) snprintf(
                str, OUTSTRLEN, "%c%i", OUTSEP, (sw->SoilWat.is_wet[i]) ? 1 : 0
            );
            fullBuffer = sw_memccpy_inc(
                (void **) &writePtr, endOutstr, (void *) str, '\0', &writeSize
            );
            if (fullBuffer) {
                goto reportFullBuffer;
            }
        }

    } else {
        SW_SOILWAT_OUTPUTS *vo = sw->SoilWat.p_oagg[pd];

        ForEachSoilLayer(i, n_layers) {
            (void
            ) snprintf(str, OUTSTRLEN, "%c%i", OUTSEP, (int) vo->wetdays[i]);
            fullBuffer = sw_memccpy_inc(
                (void **) &writePtr, endOutstr, (void *) str, '\0', &writeSize
            );
            if (fullBuffer) {
                goto reportFullBuffer;
            }
        }
    }

reportFullBuffer:
    if (fullBuffer) {
        reportFullBuffer(LOGERROR, LogInfo);
    }
}
#endif

#if defined(RSOILWAT) || defined(SWNETCDF)

/**
@brief Gets is_wet and wetdays when dealing with RSOILWAT.

@param[in] pd Time period in simulation output (day/week/month/year)
@param[in] sw Comprehensive struct of type SW_RUN containing all information
    in the simulation.
@param[in] OutDom Struct of type SW_OUT_DOM that holds output
    information that do not change throughout simulation runs
*/
void get_wetdays_mem(OutPeriod pd, SW_RUN *sw, SW_OUT_DOM *OutDom) {
    LyrIndex i;
    size_t iOUTIndex = 0;
    SW_OUT_RUN *OutRun = &sw->OutRun;

    double *p = OutRun->p_OUT[eSW_WetDays][pd];
    SW_SOILWAT_OUTPUTS *vo = sw->SoilWat.p_oagg[pd];

#if defined(RSOILWAT)
    get_outvalleader(
        &sw->Model, pd, OutRun->irow_OUT, OutDom->nrow_OUT, OutRun->tOffset, p
    );
#endif

    ForEachSoilLayer(i, sw->Site.n_layers) {
#if defined(RSOILWAT)
        iOUTIndex = iOUT(
            i, OutRun->irow_OUT[pd], OutDom->nrow_OUT[pd], ncol_TimeOUT[pd]
        );

#elif defined(SWNETCDF)
        iOUTIndex =
            OutDom->netCDFOutput.iOUToffset[eSW_WetDays][pd][0] +
            iOUTnc(
                OutRun->irow_OUT[pd], i, 0, OutDom->nsl_OUT[eSW_WetDays][0], 1
            );
#endif

        if (pd == eSW_Day) {
            p[iOUTIndex] = (sw->SoilWat.is_wet[i]) ? 1 : 0;
        } else {
            p[iOUTIndex] = (int) vo->wetdays[i];
        }
    }

#if defined(SWNETCDF)
    /* Set extra soil layers to missing/fill value (up to domain-wide max)
     */
    for (i = sw->Site.n_layers; i < OutDom->nsl_OUT[eSW_WetDays][0]; i++) {
        iOUTIndex =
            OutDom->netCDFOutput.iOUToffset[eSW_WetDays][pd][0] +
            iOUTnc(
                OutRun->irow_OUT[pd], i, 0, OutDom->nsl_OUT[eSW_WetDays][0], 1
            );
        p[iOUTIndex] = NC_FILL_DOUBLE;
    }
#endif // SWNETCDF
}

#elif defined(STEPWAT)

/**
@brief Gets is_wet and wetdays when dealing with RSOILWAT.

@param[in] pd Time period in simulation output (day/week/month/year)
@param[in] sw Comprehensive struct of type SW_RUN containing all information
    in the simulation.
@param[in] OutDom Struct of type SW_OUT_DOM that holds output
    information that do not change throughout simulation runs
@param[out] LogInfo Holds information on warnings and errors
*/
void get_wetdays_agg(
    OutPeriod pd, SW_RUN *sw, SW_OUT_DOM *OutDom, LOG_INFO *LogInfo
) {
    LyrIndex i;
    size_t iOUTIndex = 0;
    SW_OUT_RUN *OutRun = &sw->OutRun;

    double *p = OutRun->p_OUT[eSW_WetDays][pd];
    double *psd = OutRun->p_OUTsd[eSW_WetDays][pd];

    if (pd == eSW_Day) {
        ForEachSoilLayer(i, sw->Site.n_layers) {
            iOUTIndex = iOUT(
                i, OutRun->irow_OUT[pd], OutDom->nrow_OUT[pd], ncol_TimeOUT[pd]
            );
            do_running_agg(
                p,
                psd,
                iOUTIndex,
                OutRun->currIter,
                (sw->SoilWat.is_wet[i]) ? 1 : 0
            );
        }

    } else {
        SW_SOILWAT_OUTPUTS *vo = sw->SoilWat.p_oagg[pd];

        ForEachSoilLayer(i, sw->Site.n_layers) {
            iOUTIndex = iOUT(
                i, OutRun->irow_OUT[pd], OutDom->nrow_OUT[pd], ncol_TimeOUT[pd]
            );
            do_running_agg(p, psd, iOUTIndex, OutRun->currIter, vo->wetdays[i]);
        }
    }

    if (OutDom->print_IterationSummary) {
        OutRun->sw_outstr_agg[0] = '\0';
        format_IterationSummary(
            p,
            psd,
            pd,
            OutDom->ncol_OUT[eSW_WetDays],
            sw,
            OutDom->nrow_OUT,
            LogInfo
        );
    }
}
#endif


//------ eSW_SnowPack
#ifdef SW_OUTTEXT

/**
@brief Gets snowpack and snowdepth when dealing with OUTTEXT.

@param[in] pd Time period in simulation output (day/week/month/year)
@param[in] sw Comprehensive struct of type SW_RUN containing all information
    in the simulation.
@param[out] LogInfo Holds information on warnings and errors
*/
void get_snowpack_text(OutPeriod pd, SW_RUN *sw, LOG_INFO *LogInfo) {
    SW_SOILWAT_OUTPUTS *vo = sw->SoilWat.p_oagg[pd];
    SW_OUT_RUN *OutRun = &sw->OutRun;

    OutRun->sw_outstr[0] = '\0';
    (void) snprintf(
        OutRun->sw_outstr,
        sizeof OutRun->sw_outstr,
        "%c%.*f%c%.*f",
        OUTSEP,
        OUT_DIGITS,
        vo->snowpack,
        OUTSEP,
        OUT_DIGITS,
        vo->snowdepth
    );

    (void) LogInfo;
}
#endif

#if defined(RSOILWAT) || defined(SWNETCDF)

/**
@brief Gets snowpack and snowdepth when dealing with OUTTEXT.

@param[in] pd Time period in simulation output (day/week/month/year)
@param[in] sw Comprehensive struct of type SW_RUN containing all information
    in the simulation.
@param[in] OutDom Struct of type SW_OUT_DOM that holds output
    information that do not change throughout simulation runs
*/
void get_snowpack_mem(OutPeriod pd, SW_RUN *sw, SW_OUT_DOM *OutDom) {
    SW_SOILWAT_OUTPUTS *vo = sw->SoilWat.p_oagg[pd];
    size_t iOUTIndex = 0;
    SW_OUT_RUN *OutRun = &sw->OutRun;

    double *p = OutRun->p_OUT[eSW_SnowPack][pd];

#if defined(RSOILWAT)
    get_outvalleader(
        &sw->Model, pd, OutRun->irow_OUT, OutDom->nrow_OUT, OutRun->tOffset, p
    );
#endif

#if defined(RSOILWAT)
    iOUTIndex =
        iOUT(0, OutRun->irow_OUT[pd], OutDom->nrow_OUT[pd], ncol_TimeOUT[pd]);

#elif defined(SWNETCDF)
    iOUTIndex = OutDom->netCDFOutput.iOUToffset[eSW_SnowPack][pd][0] +
                iOUTnc(OutRun->irow_OUT[pd], 0, 0, 1, 1);
#endif

    p[iOUTIndex] = vo->snowpack;


#if defined(RSOILWAT)
    iOUTIndex =
        iOUT(1, OutRun->irow_OUT[pd], OutDom->nrow_OUT[pd], ncol_TimeOUT[pd]);

#elif defined(SWNETCDF)
    iOUTIndex = OutDom->netCDFOutput.iOUToffset[eSW_SnowPack][pd][1] +
                iOUTnc(OutRun->irow_OUT[pd], 0, 0, 1, 1);
#endif

    p[iOUTIndex] = vo->snowdepth;
}

#elif defined(STEPWAT)

/**
@brief Gets snowpack and snowdepth when dealing with STEPWAT.

@param[in] pd Time period in simulation output (day/week/month/year)
@param[in] sw Comprehensive struct of type SW_RUN containing all information
    in the simulation.
@param[in] OutDom Struct of type SW_OUT_DOM that holds output
    information that do not change throughout simulation runs
@param[out] LogInfo Holds information on warnings and errors
*/
void get_snowpack_agg(
    OutPeriod pd, SW_RUN *sw, SW_OUT_DOM *OutDom, LOG_INFO *LogInfo
) {
    SW_SOILWAT_OUTPUTS *vo = sw->SoilWat.p_oagg[pd];
    size_t iOUTIndex = 0;
    SW_OUT_RUN *OutRun = &sw->OutRun;

    double *p = OutRun->p_OUT[eSW_SnowPack][pd];
    double *psd = OutRun->p_OUTsd[eSW_SnowPack][pd];

    iOUTIndex =
        iOUT(0, OutRun->irow_OUT[pd], OutDom->nrow_OUT[pd], ncol_TimeOUT[pd]);
    do_running_agg(p, psd, iOUTIndex, OutRun->currIter, vo->snowpack);

    iOUTIndex =
        iOUT(1, OutRun->irow_OUT[pd], OutDom->nrow_OUT[pd], ncol_TimeOUT[pd]);
    do_running_agg(p, psd, iOUTIndex, OutRun->currIter, vo->snowdepth);

    if (OutDom->print_IterationSummary) {
        OutRun->sw_outstr_agg[0] = '\0';
        format_IterationSummary(
            p,
            psd,
            pd,
            OutDom->ncol_OUT[eSW_SnowPack],
            sw,
            OutDom->nrow_OUT,
            LogInfo
        );
    }
}
#endif


//------ eSW_DeepSWC
#ifdef SW_OUTTEXT

/**
@brief Gets deep for when dealing with OUTTEXT.

@param[in] pd Time period in simulation output (day/week/month/year)
@param[in] sw Comprehensive struct of type SW_RUN containing all information
    in the simulation.
@param[out] LogInfo Holds information on warnings and errors
*/
void get_deepswc_text(OutPeriod pd, SW_RUN *sw, LOG_INFO *LogInfo) {
    SW_SOILWAT_OUTPUTS *vo = sw->SoilWat.p_oagg[pd];
    SW_OUT_RUN *OutRun = &sw->OutRun;

    OutRun->sw_outstr[0] = '\0';
    (void) snprintf(
        OutRun->sw_outstr,
        sizeof OutRun->sw_outstr,
        "%c%.*f",
        OUTSEP,
        OUT_DIGITS,
        vo->deep
    );

    (void) LogInfo;
}
#endif

#if defined(RSOILWAT) || defined(SWNETCDF)

/**
@brief Gets deep for when dealing with RSOILWAT.

@param[in] pd Time period in simulation output (day/week/month/year)
@param[in] sw Comprehensive struct of type SW_RUN containing all information
    in the simulation.
@param[in] OutDom Struct of type SW_OUT_DOM that holds output
    information that do not change throughout simulation runs
*/
void get_deepswc_mem(OutPeriod pd, SW_RUN *sw, SW_OUT_DOM *OutDom) {
    SW_SOILWAT_OUTPUTS *vo = sw->SoilWat.p_oagg[pd];
    size_t iOUTIndex = 0;
    SW_OUT_RUN *OutRun = &sw->OutRun;

    double *p = OutRun->p_OUT[eSW_DeepSWC][pd];

#if defined(RSOILWAT)
    get_outvalleader(
        &sw->Model, pd, OutRun->irow_OUT, OutDom->nrow_OUT, OutRun->tOffset, p
    );
#endif

#if defined(RSOILWAT)
    iOUTIndex =
        iOUT(0, OutRun->irow_OUT[pd], OutDom->nrow_OUT[pd], ncol_TimeOUT[pd]);

#elif defined(SWNETCDF)
    iOUTIndex = OutDom->netCDFOutput.iOUToffset[eSW_DeepSWC][pd][0] +
                iOUTnc(OutRun->irow_OUT[pd], 0, 0, 1, 1);
#endif

    p[iOUTIndex] = vo->deep;
}

#elif defined(STEPWAT)

/**
@brief Gets deep for when dealing with STEPWAT.

@param[in] pd Time period in simulation output (day/week/month/year)
@param[in] sw Comprehensive struct of type SW_RUN containing all information
    in the simulation.
@param[in] OutDom Struct of type SW_OUT_DOM that holds output
    information that do not change throughout simulation runs
@param[out] LogInfo Holds information on warnings and errors
*/
void get_deepswc_agg(
    OutPeriod pd, SW_RUN *sw, SW_OUT_DOM *OutDom, LOG_INFO *LogInfo
) {
    SW_SOILWAT_OUTPUTS *vo = sw->SoilWat.p_oagg[pd];
    size_t iOUTIndex = 0;
    SW_OUT_RUN *OutRun = &sw->OutRun;

    double *p = OutRun->p_OUT[eSW_DeepSWC][pd];
    double *psd = OutRun->p_OUTsd[eSW_DeepSWC][pd];

    iOUTIndex =
        iOUT(0, OutRun->irow_OUT[pd], OutDom->nrow_OUT[pd], ncol_TimeOUT[pd]);
    do_running_agg(p, psd, iOUTIndex, OutRun->currIter, vo->deep);

    if (OutDom->print_IterationSummary) {
        OutRun->sw_outstr_agg[0] = '\0';
        format_IterationSummary(
            p,
            psd,
            pd,
            OutDom->ncol_OUT[eSW_DeepSWC],
            sw,
            OutDom->nrow_OUT,
            LogInfo
        );
    }
}
#endif


//------ eSW_SoilTemp
#ifdef SW_OUTTEXT

/**
@brief Gets soil temperature for when dealing with OUTTEXT.

@param[in] pd Time period in simulation output (day/week/month/year)
@param[in] sw Comprehensive struct of type SW_RUN containing all information
    in the simulation.
@param[out] LogInfo Holds information on warnings and errors
*/
void get_soiltemp_text(OutPeriod pd, SW_RUN *sw, LOG_INFO *LogInfo) {
    LyrIndex i;
    SW_SOILWAT_OUTPUTS *vo = sw->SoilWat.p_oagg[pd];
    SW_OUT_RUN *OutRun = &sw->OutRun;
    size_t writeSize = (size_t) (MAX_LAYERS * OUTSTRLEN);
    char *writePtr = OutRun->sw_outstr;
    char *endOutstr = OutRun->sw_outstr + sizeof OutRun->sw_outstr - 1;
    Bool fullBuffer = swFALSE;

    char str[OUTSTRLEN];
    OutRun->sw_outstr[0] = '\0';

    ForEachSoilLayer(i, sw->Site.n_layers) {
        (void) snprintf(
            str,
            OUTSTRLEN,
            "%c%.*f",
            OUTSEP,
            OUT_DIGITS,
            vo->maxLyrTemperature[i]
        );
        fullBuffer = sw_memccpy_inc(
            (void **) &writePtr, endOutstr, (void *) str, '\0', &writeSize
        );
        if (fullBuffer) {
            goto reportFullBuffer;
        }

        (void) snprintf(
            str,
            OUTSTRLEN,
            "%c%.*f",
            OUTSEP,
            OUT_DIGITS,
            vo->minLyrTemperature[i]
        );
        fullBuffer = sw_memccpy_inc(
            (void **) &writePtr, endOutstr, (void *) str, '\0', &writeSize
        );
        if (fullBuffer) {
            goto reportFullBuffer;
        }

        (void) snprintf(
            str, OUTSTRLEN, "%c%.*f", OUTSEP, OUT_DIGITS, vo->avgLyrTemp[i]
        );
        fullBuffer = sw_memccpy_inc(
            (void **) &writePtr, endOutstr, (void *) str, '\0', &writeSize
        );
        if (fullBuffer) {
            goto reportFullBuffer;
        }
    }

reportFullBuffer:
    if (fullBuffer) {
        reportFullBuffer(LOGERROR, LogInfo);
    }
}
#endif

#if defined(RSOILWAT) || defined(SWNETCDF)

/**
@brief Gets soil temperature for when dealing with RSOILWAT.

@param[in] pd Time period in simulation output (day/week/month/year)
@param[in] sw Comprehensive struct of type SW_RUN containing all information
    in the simulation.
@param[in] OutDom Struct of type SW_OUT_DOM that holds output
    information that do not change throughout simulation runs
*/
void get_soiltemp_mem(OutPeriod pd, SW_RUN *sw, SW_OUT_DOM *OutDom) {
    LyrIndex i;
    SW_SOILWAT_OUTPUTS *vo = sw->SoilWat.p_oagg[pd];
    size_t iOUTIndex = 0;
    SW_OUT_RUN *OutRun = &sw->OutRun;

    double *p = OutRun->p_OUT[eSW_SoilTemp][pd];

#if defined(RSOILWAT)
    get_outvalleader(
        &sw->Model, pd, OutRun->irow_OUT, OutDom->nrow_OUT, OutRun->tOffset, p
    );
#endif

    ForEachSoilLayer(i, sw->Site.n_layers) {
#if defined(RSOILWAT)
        iOUTIndex = iOUT(
            (i * 3),
            OutRun->irow_OUT[pd],
            OutDom->nrow_OUT[pd],
            ncol_TimeOUT[pd]
        );

#elif defined(SWNETCDF)
        iOUTIndex =
            OutDom->netCDFOutput.iOUToffset[eSW_SoilTemp][pd][0] +
            iOUTnc(
                OutRun->irow_OUT[pd], i, 0, OutDom->nsl_OUT[eSW_SoilTemp][0], 1
            );
#endif

        p[iOUTIndex] = vo->maxLyrTemperature[i];


#if defined(RSOILWAT)
        iOUTIndex = iOUT(
            (i * 3) + 1,
            OutRun->irow_OUT[pd],
            OutDom->nrow_OUT[pd],
            ncol_TimeOUT[pd]
        );

#elif defined(SWNETCDF)
        iOUTIndex =
            OutDom->netCDFOutput.iOUToffset[eSW_SoilTemp][pd][1] +
            iOUTnc(
                OutRun->irow_OUT[pd], i, 0, OutDom->nsl_OUT[eSW_SoilTemp][1], 1
            );
#endif

        p[iOUTIndex] = vo->minLyrTemperature[i];


#if defined(RSOILWAT)
        iOUTIndex = iOUT(
            (i * 3) + 2,
            OutRun->irow_OUT[pd],
            OutDom->nrow_OUT[pd],
            ncol_TimeOUT[pd]
        );

#elif defined(SWNETCDF)
        iOUTIndex =
            OutDom->netCDFOutput.iOUToffset[eSW_SoilTemp][pd][2] +
            iOUTnc(
                OutRun->irow_OUT[pd], i, 0, OutDom->nsl_OUT[eSW_SoilTemp][2], 1
            );
#endif

        p[iOUTIndex] = vo->avgLyrTemp[i];
    }

#if defined(SWNETCDF)
    /* Set extra soil layers to missing/fill value (up to domain-wide max)
     */
    for (i = sw->Site.n_layers; i < OutDom->nsl_OUT[eSW_SoilTemp][0]; i++) {
        iOUTIndex =
            OutDom->netCDFOutput.iOUToffset[eSW_SoilTemp][pd][0] +
            iOUTnc(
                OutRun->irow_OUT[pd], i, 0, OutDom->nsl_OUT[eSW_SoilTemp][0], 1
            );
        p[iOUTIndex] = NC_FILL_DOUBLE;

        iOUTIndex =
            OutDom->netCDFOutput.iOUToffset[eSW_SoilTemp][pd][1] +
            iOUTnc(
                OutRun->irow_OUT[pd], i, 0, OutDom->nsl_OUT[eSW_SoilTemp][1], 1
            );
        p[iOUTIndex] = NC_FILL_DOUBLE;

        iOUTIndex =
            OutDom->netCDFOutput.iOUToffset[eSW_SoilTemp][pd][2] +
            iOUTnc(
                OutRun->irow_OUT[pd], i, 0, OutDom->nsl_OUT[eSW_SoilTemp][2], 1
            );
        p[iOUTIndex] = NC_FILL_DOUBLE;
    }
#endif // SWNETCDF
}

#elif defined(STEPWAT)

/**
@brief Gets soil temperature for when dealing with STEPWAT.

@param[in] pd Time period in simulation output (day/week/month/year)
@param[in] sw Comprehensive struct of type SW_RUN containing all information
    in the simulation.
@param[in] OutDom Struct of type SW_OUT_DOM that holds output
    information that do not change throughout simulation runs
@param[out] LogInfo Holds information on warnings and errors
*/
void get_soiltemp_agg(
    OutPeriod pd, SW_RUN *sw, SW_OUT_DOM *OutDom, LOG_INFO *LogInfo
) {
    LyrIndex i;
    SW_SOILWAT_OUTPUTS *vo = sw->SoilWat.p_oagg[pd];
    size_t iOUTIndex = 0;
    SW_OUT_RUN *OutRun = &sw->OutRun;

    double *p = OutRun->p_OUT[eSW_SoilTemp][pd];
    double *psd = OutRun->p_OUTsd[eSW_SoilTemp][pd];

    ForEachSoilLayer(i, sw->Site.n_layers) {
        iOUTIndex = iOUT(
            (i * 3),
            OutRun->irow_OUT[pd],
            OutDom->nrow_OUT[pd],
            ncol_TimeOUT[pd]
        );
        do_running_agg(
            p, psd, iOUTIndex, OutRun->currIter, vo->maxLyrTemperature[i]
        );

        iOUTIndex = iOUT(
            (i * 3) + 1,
            OutRun->irow_OUT[pd],
            OutDom->nrow_OUT[pd],
            ncol_TimeOUT[pd]
        );
        do_running_agg(
            p, psd, iOUTIndex, OutRun->currIter, vo->minLyrTemperature[i]
        );

        iOUTIndex = iOUT(
            (i * 3) + 2,
            OutRun->irow_OUT[pd],
            OutDom->nrow_OUT[pd],
            ncol_TimeOUT[pd]
        );
        do_running_agg(p, psd, iOUTIndex, OutRun->currIter, vo->avgLyrTemp[i]);
    }

    if (OutDom->print_IterationSummary) {
        OutRun->sw_outstr_agg[0] = '\0';
        format_IterationSummary(
            p,
            psd,
            pd,
            OutDom->ncol_OUT[eSW_SoilTemp],
            sw,
            OutDom->nrow_OUT,
            LogInfo
        );
    }
}
#endif

//------ eSW_Frozen
#ifdef SW_OUTTEXT

/**
@brief Gets state (frozen/unfrozen) for each layer for when dealing with
OUTTEXT.

@param[in] pd Time period in simulation output (day/week/month/year)
@param[in] sw Comprehensive struct of type SW_RUN containing all information
    in the simulation.
@param[out] LogInfo Holds information on warnings and errors
*/
void get_frozen_text(OutPeriod pd, SW_RUN *sw, LOG_INFO *LogInfo) {
    LyrIndex i;
    SW_SOILWAT_OUTPUTS *vo = sw->SoilWat.p_oagg[pd];
    SW_OUT_RUN *OutRun = &sw->OutRun;
    size_t writeSize = (size_t) (MAX_LAYERS * OUTSTRLEN);
    char *writePtr = OutRun->sw_outstr;
    char *endOutstr = OutRun->sw_outstr + sizeof OutRun->sw_outstr - 1;
    Bool fullBuffer = swFALSE;

    char str[OUTSTRLEN];
    OutRun->sw_outstr[0] = '\0';

    ForEachSoilLayer(i, sw->Site.n_layers) {
        (void) snprintf(
            str, OUTSTRLEN, "%c%.*f", OUTSEP, OUT_DIGITS, vo->lyrFrozen[i]
        );
        fullBuffer = sw_memccpy_inc(
            (void **) &writePtr, endOutstr, (void *) str, '\0', &writeSize
        );
        if (fullBuffer) {
            goto reportFullBuffer;
        }
    }

reportFullBuffer:
    if (fullBuffer) {
        reportFullBuffer(LOGERROR, LogInfo);
    }
}
#endif

#if defined(RSOILWAT) || defined(SWNETCDF)

/**
@brief Gets soil state (frozen/unfrozen) for when dealing with RSOILWAT.

@param[in] pd Time period in simulation output (day/week/month/year)
@param[in] sw Comprehensive struct of type SW_RUN containing all information
    in the simulation.
@param[in] OutDom Struct of type SW_OUT_DOM that holds output
    information that do not change throughout simulation runs
*/
void get_frozen_mem(OutPeriod pd, SW_RUN *sw, SW_OUT_DOM *OutDom) {
    LyrIndex i;
    SW_SOILWAT_OUTPUTS *vo = sw->SoilWat.p_oagg[pd];
    size_t iOUTIndex = 0;
    SW_OUT_RUN *OutRun = &sw->OutRun;

    double *p = OutRun->p_OUT[eSW_Frozen][pd];

#if defined(RSOILWAT)
    get_outvalleader(
        &sw->Model, pd, OutRun->irow_OUT, OutDom->nrow_OUT, OutRun->tOffset, p
    );
#endif

    ForEachSoilLayer(i, sw->Site.n_layers) {
#if defined(RSOILWAT)
        iOUTIndex = iOUT(
            i, OutRun->irow_OUT[pd], OutDom->nrow_OUT[pd], ncol_TimeOUT[pd]
        );

#elif defined(SWNETCDF)
        iOUTIndex =
            OutDom->netCDFOutput.iOUToffset[eSW_Frozen][pd][0] +
            iOUTnc(
                OutRun->irow_OUT[pd], i, 0, OutDom->nsl_OUT[eSW_Frozen][0], 1
            );
#endif

        p[iOUTIndex] = vo->lyrFrozen[i];
    }

#if defined(SWNETCDF)
    /* Set extra soil layers to missing/fill value (up to domain-wide max)
     */
    for (i = sw->Site.n_layers; i < OutDom->nsl_OUT[eSW_Frozen][0]; i++) {
        iOUTIndex =
            OutDom->netCDFOutput.iOUToffset[eSW_Frozen][pd][0] +
            iOUTnc(
                OutRun->irow_OUT[pd], i, 0, OutDom->nsl_OUT[eSW_Frozen][0], 1
            );
        p[iOUTIndex] = NC_FILL_DOUBLE;
    }
#endif // SWNETCDF
}

#elif defined(STEPWAT)

/**
@brief Gets soil temperature for when dealing with STEPWAT.

@param[in] pd Time period in simulation output (day/week/month/year)
@param[in] sw Comprehensive struct of type SW_RUN containing all information
    in the simulation.
@param[in] OutDom Struct of type SW_OUT_DOM that holds output
    information that do not change throughout simulation runs
@param[out] LogInfo Holds information on warnings and errors
*/
void get_frozen_agg(
    OutPeriod pd, SW_RUN *sw, SW_OUT_DOM *OutDom, LOG_INFO *LogInfo
) {
    LyrIndex i;
    SW_SOILWAT_OUTPUTS *vo = sw->SoilWat.p_oagg[pd];
    size_t iOUTIndex = 0;
    SW_OUT_RUN *OutRun = &sw->OutRun;

    double *p = OutRun->p_OUT[eSW_Frozen][pd];
    double *psd = OutRun->p_OUTsd[eSW_Frozen][pd];

    ForEachSoilLayer(i, sw->Site.n_layers) {
        iOUTIndex = iOUT(
            i, OutRun->irow_OUT[pd], OutDom->nrow_OUT[pd], ncol_TimeOUT[pd]
        );
        do_running_agg(p, psd, iOUTIndex, OutRun->currIter, vo->lyrFrozen[i]);
    }

    if (OutDom->print_IterationSummary) {
        OutRun->sw_outstr_agg[0] = '\0';
        format_IterationSummary(
            p,
            psd,
            pd,
            OutDom->ncol_OUT[eSW_Frozen],
            sw,
            OutDom->nrow_OUT,
            LogInfo
        );
    }
}
#endif
