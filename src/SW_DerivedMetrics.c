
/* =================================================== */
/*                INCLUDES / DEFINES                   */
/* --------------------------------------------------- */
#include "include/SW_DerivedMetrics.h"
#include "include/generic.h"    // for fmax, GT, ZRO
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

    if (tmean > baseTmean && swe <= baseSWE) {
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

    if (tmean > baseTmean && swe <= baseSWE) {
        swa = metric_totalSWA(swcBulk, baseSWC, layerWeights, n_layers);
        if (GT(swa, 0.)) {
            wdd = fmax(0., tmean - baseTmean);
        }
    }

    return wdd;
}
