/********************************************************/
/********************************************************/
/*	Source file: SW_Flow_lib.h
 * Type: header
 * Application: SOILWAT - soilwater dynamics simulator
 * Purpose: Support definitions/declarations for
 * water flow subroutines in SoWat_flow_subs.h.
 * History:
 * 10-20-03 (cwb) Added drainout variable.
 * 10/19/2010	(drs) added function hydraulic_redistribution()
 * 11/16/2010	(drs) added function forest_intercepted_water() and MAX_WINTFOR
 *    renamed evap_litter_stdcrop() -> evap_litter_veg()
 * 08/22/2011	(drs) renamed bs_ev_tr_loss() to EsT_partitioning()
 * 08/22/2011	(drs) added forest_EsT_partitioning() to partition bare-soil
 *    evaporation (Es) and transpiration (T) in forests
 * 09/08/2011	(drs) renamed stcrp_intercepted_water() to
 *    grass_intercepted_water(); added shrub_intercepted_water() as current
 *    copy from grass_intercepted_water();
 *    added double scale to each xx_intercepted_water() function to scale for
 *    snowdepth effects and vegetation type fraction added double a,b,c,d to
 *    each xx_intercepted_water() function for for parameters in intercepted
 *    rain = (a + b*veg) + (c+d*veg) * ppt
 * 09/09/2011	(drs) added xx_EsT_partitioning() for each vegetation type
 *    (grass, shrub, tree);
 *    added double lai_param as parameter in exp-equation
 * 09/09/2011	(drs) replaced evap_litter_veg_surfaceWater() with
 *    evap_fromSurface() to be called for each surface water pool seperately
 * 09/09/2011	(drs) replaced SHD_xx constanst with input parameters in
 *    pot_transp()
 * 09/09/2011	(drs) added double Es_param_limit to pot_soil_evap() to
 *    scale and limit bare-soil evaporation rate
 *    (previously hidden constant in code)
 * 09/09/2011	(drs) renamed reduce_rates_by_surfaceEvaporation() to
 *    reduce_rates_by_unmetEvapDemand()
 * 09/11/2011	(drs) added double scale to hydraulic_redistribution() function
 *    to scale for vegetation type fraction
 * 09/13/2011	(drs)	added double scale, a,b,c,d to each
 *    litter_intercepted_water() function for parameters in
 *    litter intercepted rain = ((a + b*blitter) + (c+d*blitter) * ppt) * scale
 * 09/21/2011	(drs)	reduce_rates_by_unmetEvapDemand() is obsolete, complete
 *    E and T scaling in SW_Flow.c
 * 05/25/2012  (DLM) added function soil_temperature to header file
 * 05/31/2012  (DLM) added ST_RGR_VALUES struct
 *    to keep track of variables used in the soil_temperature function
 * 11/06/2012 (clk) added slope and aspect as parameters for petfunc()
 * 01/31/2013	(clk) added new function, pot_soil_evap_bs()
 * 03/07/2013	(clk) add new array, lyrFrozen to keep track of whether a
 *    certain soil layer is frozen. 1 = frozen, 0 = not frozen.
 * 07/09/2013	(clk) added two new functions:
 *    forb_intercepted_water and forb_EsT_partitioning
 */
/********************************************************/
/********************************************************/

#ifndef SW_WATERSUBS_H
#define SW_WATERSUBS_H

#include "include/generic.h"        // for Bool
#include "include/SW_datastructs.h" // for LOG_INFO, SW_SITE, SW_ST_SIM
#include "include/SW_Defines.h"     // for TimeInt, MAX_LAYERS, MAX_ST_RGR

#ifdef __cplusplus
extern "C" {
#endif


/* =================================================== */
/*             Global Function Declarations            */
/* --------------------------------------------------- */
void veg_intercepted_water(
    double *ppt_incident,
    double *int_veg,
    double *s_veg,
    double m,
    double kSmax,
    double LAI,
    double scale
);

void litter_intercepted_water(
    double *ppt_through,
    double *int_lit,
    double *s_lit,
    double m,
    double kSmax,
    double blitter,
    double scale
);

void infiltrate_water_high(
    double swc[],
    double drain[],
    double *drainout,
    double pptleft,
    unsigned int nlyrs,
    const double swcfc[],
    double swcsat[],
    double ksat[],
    const double impermeability[],
    double *standingWater,
    double lyrFrozen[]
);

void transp_weighted_avg(
    double *swp_avg,
    SW_SITE *SW_Site,
    unsigned int n_tr_rgns,
    LyrIndex n_layers,
    const unsigned int tr_regions[],
    double swc[],
    int VegType,
    LOG_INFO *LogInfo
);

void EsT_partitioning(
    double *fbse, double *fbst, double blivelai, double lai_param
);

void pot_soil_evap(
    SW_SITE *SW_Site,
    unsigned int nelyrs,
    double totagb,
    double fbse,
    double petday,
    double shift,
    double shape,
    double inflec,
    double range,
    double swc[],
    double Es_param_limit,
    double *bserate,
    LOG_INFO *LogInfo
);

void pot_soil_evap_bs(
    double *bserate,
    SW_SITE *SW_Site,
    unsigned int nelyrs,
    double petday,
    double shift,
    double shape,
    double inflec,
    double range,
    double swc[],
    LOG_INFO *LogInfo
);

void pot_transp(
    double *bstrate,
    double swpavg,
    double biolive,
    double biodead,
    double fbst,
    double petday,
    double swp_shift,
    double swp_shape,
    double swp_inflec,
    double swp_range,
    double shade_scale,
    double shade_deadmax,
    double shade_xinflex,
    double shade_slope,
    double shade_yinflex,
    double shade_range,
    double co2_wue_multiplier
);

double watrate(
    double swp,
    double petday,
    double shift,
    double shape,
    double inflec,
    double range
);

void evap_litter_veg_surfaceWater(
    double *cwlit,
    double *cwstcr,
    double *standingWater,
    double *wevap,
    double *aet,
    double petday
);

void evap_fromSurface(double *water_pool, double *evap_rate, double *aet);

void remove_from_soil(
    double swc[],
    double qty[],
    SW_SITE *SW_Site,
    double *aet,
    unsigned int nlyrs,
    const double coeff[],
    double rate,
    double swcmin[],
    double lyrFrozen[],
    LOG_INFO *LogInfo
);

void percolate_unsaturated(
    double swc[],
    double percolate[],
    double *drainout,
    double *standingWater,
    unsigned int nlyrs,
    double lyrFrozen[],
    SW_SITE *SW_Site,
    double slow_drain_coeff,
    double slow_drain_depth
);

void hydraulic_redistribution(
    double swc[],
    double hydred[],
    SW_SITE *SW_Site,
    unsigned int vegk,
    unsigned int nlyrs,
    const double lyrFrozen[],
    double maxCondroot,
    double swp50,
    double shapeCond,
    double scale,
    TimeInt year,
    TimeInt doy,
    LOG_INFO *LogInfo
);

void surface_temperature(
    double *minTempSurface,
    double *meanTempSurface,
    double *maxTempSurface,
    unsigned int method,
    double snow,
    double minTempAir,
    double meanTempAir,
    double maxTempAir,
    double H_gt,
    double pet,
    double aet,
    double biomass,
    double bmLimiter,
    double t1Param1,
    double t1Param2,
    double t1Param3,
    LOG_INFO *LogInfo
);

void soil_temperature(
    SW_ST_SIM *SW_StRegSimVals,
    double *minTempSurface,
    double *meanTempSurface,
    double *maxTempSurface,
    double minTempSoil[],
    double meanTempSoil[],
    double maxTempSoil[],
    double lyrFrozen[],
    unsigned int methodSurfaceTemperature,
    double snow,
    double minTempAir,
    double meanTempAir,
    double maxTempAir,
    double H_gt,
    double pet,
    double aet,
    double biomass,
    double swc[],
    double swc_sat[],
    double bDensity[],
    double width[],
    double depths[],
    unsigned int nlyrs,
    double bmLimiter,
    double t1Param1,
    double t1Param2,
    double t1Param3,
    double csParam1,
    double csParam2,
    double shParam,
    double sTconst,
    double deltaX,
    double theMaxDepth,
    unsigned int nRgr,
    TimeInt year,
    TimeInt doy,
    Bool *ptr_stError,
    LOG_INFO *LogInfo
);

void lyrTemp_to_lyrSoil_temperature(
    double cor[MAX_ST_RGR][MAX_LAYERS + 1],
    unsigned int nlyrTemp,
    double depth_Temp[],
    double avgLyrTempR[],
    unsigned int nlyrSoil,
    double depth_Soil[],
    double width_Soil[],
    double avgLyrTemp[],
    double temperatureRangeR[],
    double temperatureRange[]
);

void lyrSoil_to_lyrTemp_temperature(
    unsigned int nlyrSoil,
    const double depth_Soil[],
    const double avgLyrTemp[],
    double endTemp,
    unsigned int nlyrTemp,
    double depth_Temp[],
    double maxTempDepth,
    double avgLyrTempR[]
);

void lyrSoil_to_lyrTemp(
    double cor[MAX_ST_RGR][MAX_LAYERS + 1],
    unsigned int nlyrSoil,
    const double width_Soil[],
    const double var[],
    unsigned int nlyrTemp,
    double width_Temp,
    double res[]
);

double surface_temperature_under_snow(double airTempAvg, double snow);

void SW_ST_init_run(SW_ST_SIM *StRegSimVals);

void SW_ST_setup_run(
    SW_ST_SIM *SW_StRegSimVals,
    SW_SITE *SW_Site,
    Bool *ptr_stError,
    Bool *soil_temp_init,
    double airTemp,
    double swc[],
    double *surfaceAvg,
    double avgLyrTemp[],
    double *lyrFrozen,
    LOG_INFO *LogInfo
);

void soil_temperature_setup(
    SW_ST_SIM *SW_StRegSimVals,
    double bDensity[],
    double width[],
    double avgLyrTempInit[],
    double sTconst,
    unsigned int nlyrs,
    const double fc[],
    const double wp[],
    double deltaX,
    double theMaxDepth,
    unsigned int nRgr,
    double depths[],
    Bool *ptr_stError,
    Bool *soil_temp_init,
    LOG_INFO *LogInfo
);

void set_frozen_unfrozen(
    unsigned int nlyrs,
    double avgLyrTemp[],
    double swc[],
    double swc_sat[],
    const double width[],
    double lyrFrozen[]
);

unsigned int adjust_Tsoil_by_freezing_and_thawing(
    const double oldavgLyrTemp[],
    const double avgLyrTemp[],
    double shParam,
    unsigned int nlyrs,
    const double vwc[],
    const double bDensity[],
    Bool *fusion_pool_init,
    double oldsFusionPool_actual[]
);

void soil_temperature_today(
    double *ptr_dTime,
    double deltaX,
    double sT1,
    double sTconst,
    unsigned int nRgr,
    double avgLyrTempR[],
    const double oldavgLyrTempR[],
    const double vwcR[],
    const double wpR[],
    const double fcR[],
    const double bDensityR[],
    double csParam1,
    double csParam2,
    double shParam,
    Bool *ptr_stError,
    double surface_range,
    double temperatureRangeR[],
    const double depthsR[],
    TimeInt year,
    TimeInt doy
);

#ifdef __cplusplus
}
#endif

#endif
