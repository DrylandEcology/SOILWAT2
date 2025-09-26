#ifndef SW_DERIVEDMETRICS_H
#define SW_DERIVEDMETRICS_H

#include "include/SW_Defines.h" // for LyrIndex

#ifdef __cplusplus
extern "C" {
#endif

double metric_CWD(double pet, double aet);

double metric_totalSWA(
    double const swcBulk[],
    double const baseSWC[],
    double const layerWeights[],
    LyrIndex n_layers
);

double metric_DDD(
    double tmean,
    double baseTmean,
    double swe,
    double baseSWE,
    double const swcBulk[],
    double const baseSWC[],
    double const layerWeights[],
    LyrIndex n_layers
);

double metric_WDD(
    double tmean,
    double baseTmean,
    double swe,
    double baseSWE,
    double const swcBulk[],
    double const baseSWC[],
    double const layerWeights[],
    LyrIndex n_layers
);

#ifdef __cplusplus
}
#endif

#endif
