/********************************************************/
/********************************************************/
/*	Source file: SW_SoilWater.h
 Type: header
 Application: SOILWAT - soilwater dynamics simulator
 Purpose: Definitions for actual soil water content,
 the reason for the model.
 History:

 9/11/01 -- INITIAL CODING - cwb

 1/25/02 - cwb- removed SW_TIMES dy element from hist.

 10/9/2009	-	(drs) added snow accumulation, snow sublimation and snow
 melt

 20100202 (drs) added lyrdrain[MAX_LAYERS], crop_int, litt_int, soil_inf
    to SW_SOILWAT_OUTPUTS;
    added standcrop_int, litter_int, soil_inf to SW_SOILWTAT;

 04/16/2010	(drs) added swa[MAX_LAYERS] to SW_SOILWAT_OUTPUTS:
    available soil water (cm/layer) = swc - (wilting point)

 10/04/2010	(drs) added snowMAUS snow accumulation, sublimation and melt
    algorithm: Trnka, M., et al. (2010) Simple snow cover model for
    agrometeorological applications. Agricultural and Forest Meteorology,
    150, 1115-1127.
    added snowMAUS model parameters, replaced SW_SWC_snow_accumulation,
    SW_SWC_snow_sublimation, and SW_SWC_snow_melt with
    SW_SWC_adjust_snow(temp_min, temp_max, *ppt, *rain, *snow, *snowmelt)

 10/15/2010	(drs) replaced snowMAUS parameters with optimized SWAT2K
    parameters: Neitsch S, Arnold J, Kiniry J, Williams J. 2005.
    Soil and water assessment tool (SWAT) theoretical documentation.
    version 2005. Blackland Research Center,
    Texas Agricultural Experiment Station: Temple, TX.

 11/02/2010	(drs) moved snow parameters to SW_Site.h/c to be read in from
    siteparam.in

 10/19/2010 (drs)	added for hydraulic redistribution: hydred [MAX_LAYERS]
 to SW_SOILWAT_OUTPUTS and SW_SOILWAT

 11/16/2010	(drs)	added for_int to SW_SOILWAT_OUTPUTS, and forest_int to
    SW_SOILWAT
    renamed crop_evap -> veg_evap, standcrop_evap -> vegetation_evap

 01/04/2011	(drs) added parameter '*snowloss' to function
    SW_SWC_adjust_snow()

 02/19/2011	(drs) moved soil_inf from SW_SOILWAT and SW_SOILWAT_OUTPUTS to
    SW_WEATHER and SW_WEATHER_OUTPUTS

 07/22/2011	(drs) added for saturated conditions: surfaceWater and
    surfaceWater_evap to SW_SOILWAT_OUTPUTS and SW_SOILWAT

 08/22/2011	(drs) added function RealD
    SW_SnowDepth( RealD SWE, RealD snowdensity)

 09/08/2011	(drs) replaced in both struct SW_SOILWAT_OUTPUTS and
    SW_SOILWAT RealD crop_int, forest_int with RealD int_veg[SW_TREES],
    int_veg[SW_SHRUB], int_veg[SW_GRASS]

 09/09/2011	(drs) replaced in both struct SW_SOILWAT_OUTPUTS and SW_SOILWAT
    RealD veg_evap with RealD evap_veg[SW_TREES], evap_veg[SW_SHRUB],
    evap_veg[SW_GRASS]

 09/09/2011	(drs) replaced in both struct SW_SOILWAT_OUTPUTS and SW_SOILWAT
    RealD transpiration and hydred with RealD transpiration_xx, hydred_xx
    for each vegetation type (tree, shrub, grass)

 09/12/2011	(drs) added RealD snowdepth [TWO_DAYS] to struct
    SW_SOILWAT_OUTPUTS and SW_SOILWAT

 02/03/2012	(drs)	added function
    'RealD SW_SWC_SWCres(RealD sand, RealD clay, RealD porosity)':
    which calculates 'Brooks-Corey' residual volumetric soil water
    based on Rawls & Brakensiek (1985)

 05/25/2012  (DLM) added avgLyrTemp[MAX_LAYERS] var to SW_SOILWAT_OUTPUTS
    struct & SW_SOILWAT struct to keep track of the soil temperatures

 04/16/2013	(clk)	Added the variables vwcMatric, and swaMatric to
    SW_SOILWAT_OUTPUTS
    Also, renamed a few of the other variables to better reflect
    MATRIC vs BULK values and SWC vs VWC.
    modified the use of these variables throughout the rest of the code.

 07/09/2013	(clk)	Added the variables transp_forb, evap_veg[SW_FORBS],
    hydred[SW_FORBS], and int_veg[SW_FORBS] to SW_SOILWAT_OUTPUTS
    Added the variables transpiration_forb, hydred[SW_FORBS],
    evap_veg[SW_FORBS], and int_veg[SW_FORBS] to SW_SOILWAT
 */
/********************************************************/
/********************************************************/

#ifndef SW_SOILWATER_H
#define SW_SOILWATER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "include/generic.h"        // for RealD
#include "include/SW_datastructs.h" // for SW_SOILWAT, SW_SITE, LOG_INFO
#include "include/SW_Defines.h"     // for TimeInt, LyrIndex, NVEGTYPES, MAX_...

/* =================================================== */
/*                 Local Defines/enum                  */
/* --------------------------------------------------- */

typedef enum { SW_Adjust_Avg = 1, SW_Adjust_StdErr } SW_AdjustMethods;

/** Pressure head at zero water content [cm] of FWX SWRC */
#define FXW_h0 6.3e6

/** Pressure head at residual water content [cm] of FXW SWRC */
#define FXW_hr 1500.

/* =================================================== */
/*             Global Function Declarations            */
/* --------------------------------------------------- */
void SW_SWC_init_ptrs(SW_SOILWAT *SW_SoilWat);

void SW_SWC_construct(SW_SOILWAT *SW_SoilWat);

void SW_SWC_alloc_outptrs(SW_SOILWAT *SW_SoilWat, LOG_INFO *LogInfo);

void SW_SWC_deconstruct(SW_SOILWAT *SW_SoilWat);

void SW_SWC_new_year(
    SW_SOILWAT *SW_SoilWat, SW_SITE *SW_Site, TimeInt year, LOG_INFO *LogInfo
);

void SW_SWC_read(
    SW_SOILWAT *SW_SoilWat, TimeInt endyr, char *InFiles[], LOG_INFO *LogInfo
);

void SW_SWC_init_run(
    SW_SOILWAT *SW_SoilWat, SW_SITE *SW_Site, RealD *temp_snow
);

void _read_swc_hist(
    SW_SOILWAT_HIST *SoilWat_hist, TimeInt year, LOG_INFO *LogInfo
);

void SW_SWC_water_flow(SW_RUN *sw, LOG_INFO *LogInfo);

void calculate_repartitioned_soilwater(
    SW_SOILWAT *SW_SoilWat,
    RealD swcBulk_atSWPcrit[][MAX_LAYERS],
    SW_VEGPROD *SW_VegProd,
    LyrIndex n_layers
);

void SW_SWC_adjust_swc(
    RealD swcBulk[][MAX_LAYERS],
    RealD swcBulk_min[],
    TimeInt doy,
    SW_SOILWAT_HIST SoilWat_hist,
    LyrIndex n_layers,
    LOG_INFO *LogInfo
);

void SW_SWC_adjust_snow(
    RealD *temp_snow,
    RealD snowpack[],
    SW_SITE *SW_Site,
    RealD temp_min,
    RealD temp_max,
    RealD ppt,
    TimeInt doy,
    RealD *rain,
    RealD *snow,
    RealD *snowmelt
);

RealD SW_SWC_snowloss(RealD pet, RealD *snowpack);

RealD SW_SnowDepth(RealD SWE, RealD snowdensity);

void SW_SWC_end_day(SW_SOILWAT *SW_SoilWat, LyrIndex n_layers);

void get_dSWAbulk(
    int i,
    SW_VEGPROD *SW_VegProd,
    RealF swa_master[][NVEGTYPES][MAX_LAYERS],
    RealF dSWA_repart_sum[][MAX_LAYERS]
);

RealD SW_SWRC_SWCtoSWP(
    RealD swcBulk, SW_SITE *SW_Site, LyrIndex layerno, LOG_INFO *LogInfo
);

double SWRC_SWCtoSWP(
    double swcBulk,
    unsigned int swrc_type,
    double *swrcp,
    double gravel,
    double width,
    const int errmode,
    LOG_INFO *LogInfo
);

double SWRC_SWCtoSWP_Campbell1974(
    double swcBulk,
    double *swrcp,
    double gravel,
    double width,
    const int errmode,
    LOG_INFO *LogInfo
);
double SWRC_SWCtoSWP_vanGenuchten1980(
    double swcBulk,
    double *swrcp,
    double gravel,
    double width,
    const int errmode,
    LOG_INFO *LogInfo
);

double SWRC_SWCtoSWP_FXW(
    double swcBulk,
    double *swrcp,
    double gravel,
    double width,
    const int errmode,
    LOG_INFO *LogInfo
);

RealD SW_SWRC_SWPtoSWC(
    RealD swpMatric, SW_SITE *SW_Site, LyrIndex layerno, LOG_INFO *LogInfo
);

double SWRC_SWPtoSWC(
    double swpMatric,
    unsigned int swrc_type,
    double *swrcp,
    double gravel,
    double width,
    const int errmode,
    LOG_INFO *LogInfo
);

double SWRC_SWPtoSWC_Campbell1974(
    double swpMatric, double *swrcp, double gravel, double width
);

double SWRC_SWPtoSWC_vanGenuchten1980(
    double swpMatric, const double *swrcp, double gravel, double width
);

double SWRC_SWPtoSWC_FXW(
    double swpMatric, double *swrcp, double gravel, double width
);

#ifdef SWDEBUG
void SW_WaterBalance_Checks(SW_RUN *sw, LOG_INFO *LogInfo);
#endif


#ifdef __cplusplus
}
#endif

#endif
