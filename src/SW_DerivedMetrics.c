
/* =================================================== */
/*                INCLUDES / DEFINES                   */
/* --------------------------------------------------- */
#include "include/SW_DerivedMetrics.h"
#include "include/generic.h"    // for fmax, GT, LE, ZRO
#include "include/SW_Defines.h" // for LyrIndex, ForEachSoilLayer

/* =================================================== */
/*             Global Function Definitions             */
/* --------------------------------------------------- */

/**
@brief Climatic water deficit

@param[in] pet Potential evapotranspiration
@param[in] aet Actual evapotranspiration

@return Climatic water deficit
*/
double metric_CWD(double pet, double aet) { return pet - aet; }

/**
@brief Available soil water

@param[in] swcBulk Bulk soil water content
@param[in] baseSWC Base bulk soil water content that is held at a fixed tension
@param[in] layerWeights Weights of how much each soil layer width (thickness)
    contributes to the soil depth over which swa is summed
@param[in] n_layers Number of soil layers

@return Available soil water content that is held below a specified tension
    and summed across a specified soil depth

@note This function returns 0 if all `layerWeights` are 0, e.g., if the
    soil profile does not reach the specified soil depth.
    Output of such undefined depth ranges is set to missing on the output side
    by `valueInDepthRange()` (see `SW_Output_get_functions.c`), not here,
    because daily values are accumulated and averaged across output periods,
    and because the missing value depends on the output format.
*/
double metric_totalSWA(
    double const swcBulk[],
    double const baseSWC[],
    double const layerWeights[],
    LyrIndex n_layers
) {
    LyrIndex i;
    double swa = 0.0;

    ForEachSoilLayer(i, n_layers) {
        swa += fmax(0., (swcBulk[i] - baseSWC[i]) * layerWeights[i]);
    }

    return swa;
}

/**
@brief Dry degree-days

@param[in] tmean Daily mean air temperature
@param[in] baseTmean Base temperature above which degree-days accumulate
@param[in] swe Snow water equivalent of the snowpack
@param[in] baseSWE Maximum amount of snow below which degree-days accumulate
@param[in] swcBulk Bulk soil water content
@param[in] baseSWC Base bulk soil water content that is held at a fixed tension
@param[in] layerWeights Weights of how much each soil layer width (thickness)
    contributes to the soil depth over which swa is summed
@param[in] n_layers Number of soil layers

@return Degrees above base temperature on days when
    the soil across a specified soil depth is dry and
    there is sufficiently little snowpack
*/
double metric_DDD(
    double tmean,
    double baseTmean,
    double swe,
    double baseSWE,
    double const swcBulk[],
    double const baseSWC[],
    double const layerWeights[],
    LyrIndex n_layers
) {
    double ddd = 0.0;
    double swa = 0.0;

    if (GT(tmean, baseTmean) && LE(swe, baseSWE)) {
        swa = metric_totalSWA(swcBulk, baseSWC, layerWeights, n_layers);
        if (ZRO(swa)) {
            ddd = fmax(0., tmean - baseTmean);
        }
    }

    return ddd;
}

/**
@brief Wet degree-days

@param[in] tmean Daily mean air temperature
@param[in] baseTmean Base temperature above which degree-days accumulate
@param[in] swe Snow water equivalent of the snowpack
@param[in] baseSWE Maximum amount of snow below which degree-days accumulate
@param[in] swcBulk Bulk soil water content
@param[in] baseSWC Base bulk soil water content that is held at a fixed tension
@param[in] layerWeights Weights of how much each soil layer width (thickness)
    contributes to the soil depth over which swa is summed
@param[in] n_layers Number of soil layers

@return Degrees above base temperature on days when
    the soil across a specified soil depth is wet and
    there is sufficiently little snowpack
*/
double metric_WDD(
    double tmean,
    double baseTmean,
    double swe,
    double baseSWE,
    double const swcBulk[],
    double const baseSWC[],
    double const layerWeights[],
    LyrIndex n_layers
) {
    double wdd = 0.0;
    double swa = 0.0;

    if (GT(tmean, baseTmean) && LE(swe, baseSWE)) {
        swa = metric_totalSWA(swcBulk, baseSWC, layerWeights, n_layers);
        if (GT(swa, 0.)) {
            wdd = fmax(0., tmean - baseTmean);
        }
    }

    return wdd;
}

/**
@brief Wet day indicator

A day is wet if soil water content exceeds the base soil water content
in at least one soil layer that contributes to the soil depth of interest,
i.e., if available soil water (see metric_totalSWA()) is positive.
This is the same soil moisture criterion that metric_WDD() uses to
accumulate wet degree-days.

@param[in] swcBulk Bulk soil water content [cm / layer]
@param[in] baseSWC Base bulk soil water content that is held at a fixed tension
    [cm / layer], e.g., at -1.5 MPa (wilting point)
@param[in] layerWeights Weights of how much each soil layer width (thickness)
    contributes to the soil depth over which swa is summed
@param[in] n_layers Number of soil layers

@return 1 if available soil water summed across the soil depth of interest
    is positive (i.e., soil moisture held at less tension than represented
    by `baseSWC`), otherwise 0
*/
double metric_WetDay(
    double const swcBulk[],
    double const baseSWC[],
    double const layerWeights[],
    LyrIndex n_layers
) {
    double const swa =
        metric_totalSWA(swcBulk, baseSWC, layerWeights, n_layers);

    return GT(swa, 0.) ? 1. : 0.;
}
