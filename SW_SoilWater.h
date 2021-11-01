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
 10/9/2009	-	(drs) added snow accumulation, snow sublimation and snow melt
 20100202 (drs) added lyrdrain[MAX_LAYERS], crop_int, litt_int, soil_inf to SW_SOILWAT_OUTPUTS;
 added standcrop_int, litter_int, soil_inf to SW_SOILWTAT;
 04/16/2010	(drs) added swa[MAX_LAYERS] to SW_SOILWAT_OUTPUTS: available soil water (cm/layer) = swc - (wilting point)
 10/04/2010	(drs) added snowMAUS snow accumulation, sublimation and melt algorithm: Trnka, M., Kocmánková, E., Balek, J., Eitzinger, J., Ruget, F., Formayer, H., Hlavinka, P., Schaumberger, A., Horáková, V., Mozny, M. & Zalud, Z. (2010) Simple snow cover model for agrometeorological applications. Agricultural and Forest Meteorology, 150, 1115-1127.
 added snowMAUS model parameters, replaced SW_SWC_snow_accumulation, SW_SWC_snow_sublimation, and SW_SWC_snow_melt with SW_SWC_adjust_snow(temp_min, temp_max, *ppt, *rain, *snow, *snowmelt)
 10/15/2010	(drs) replaced snowMAUS parameters with optimized SWAT2K parameters: Neitsch S, Arnold J, Kiniry J, Williams J. 2005. Soil and water assessment tool (SWAT) theoretical documentation. version 2005. Blackland Research Center, Texas Agricultural Experiment Station: Temple, TX.
 11/02/2010	(drs) moved snow parameters to SW_Site.h/c to be read in from siteparam.in
 10/19/2010 (drs)	added for hydraulic redistribution: hydred [MAX_LAYERS] to SW_SOILWAT_OUTPUTS and SW_SOILWAT
 11/16/2010	(drs)	added for_int to SW_SOILWAT_OUTPUTS, and forest_int to SW_SOILWAT
 renamed crop_evap -> veg_evap, standcrop_evap -> vegetation_evap
 01/04/2011	(drs) added parameter '*snowloss' to function SW_SWC_adjust_snow()
 02/19/2011	(drs) moved soil_inf from SW_SOILWAT and SW_SOILWAT_OUTPUTS to SW_WEATHER and SW_WEATHER_OUTPUTS
 07/22/2011	(drs) added for saturated conditions: surfaceWater and surfaceWater_evap to SW_SOILWAT_OUTPUTS and SW_SOILWAT
 08/22/2011	(drs) added function RealD SW_SnowDepth( RealD SWE, RealD snowdensity)
 09/08/2011	(drs) replaced in both struct SW_SOILWAT_OUTPUTS and SW_SOILWAT RealD crop_int, forest_int with RealD int_veg[SW_TREES], int_veg[SW_SHRUB], int_veg[SW_GRASS]
 09/09/2011	(drs) replaced in both struct SW_SOILWAT_OUTPUTS and SW_SOILWAT RealD veg_evap with RealD evap_veg[SW_TREES], evap_veg[SW_SHRUB], evap_veg[SW_GRASS]
 09/09/2011	(drs) replaced in both struct SW_SOILWAT_OUTPUTS and SW_SOILWAT RealD transpiration and hydred with RealD transpiration_xx, hydred_xx for each vegetation type (tree, shrub, grass)
 09/12/2011	(drs) added RealD snowdepth [TWO_DAYS] to struct SW_SOILWAT_OUTPUTS and SW_SOILWAT
 02/03/2012	(drs)	added function 'RealD SW_SWC_SWCres(RealD sand, RealD clay, RealD porosity)': which calculates 'Brooks-Corey' residual volumetric soil water based on Rawls & Brakensiek (1985)
 05/25/2012  (DLM) added sTemp[MAX_LAYERS] var to SW_SOILWAT_OUTPUTS struct & SW_SOILWAT struct to keep track of the soil temperatures
 04/16/2013	(clk)	Added the variables vwcMatric, and swaMatric to SW_SOILWAT_OUTPUTS
 Also, renamed a few of the other variables to better reflect MATRIC vs BULK values and SWC vs VWC.
 modified the use of these variables throughout the rest of the code.
 07/09/2013	(clk)	Added the variables transp_forb, evap_veg[SW_FORBS], hydred[SW_FORBS], and int_veg[SW_FORBS] to SW_SOILWAT_OUTPUTS
 Added the variables transpiration_forb, hydred[SW_FORBS], evap_veg[SW_FORBS], and int_veg[SW_FORBS] to SW_SOILWAT
 */
/********************************************************/
/********************************************************/

#ifndef SW_SOILWATER_H
#define SW_SOILWATER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "generic.h"
#include "SW_Defines.h"
#include "SW_Times.h"
#include "SW_Site.h"

typedef enum {
	SW_Adjust_Avg = 1, SW_Adjust_StdErr
} SW_AdjustMethods;

/* parameters for historical (measured) swc values */
typedef struct {
	int method; /* method: 1=average; 2=hist+/- stderr */
	SW_TIMES yr;
	char *file_prefix; /* prefix to historical swc filenames */
	RealD swc[MAX_DAYS][MAX_LAYERS], std_err[MAX_DAYS][MAX_LAYERS];

} SW_SOILWAT_HIST;

/* accumulators for output values hold only the */
/* current period's values (eg, weekly or monthly) */
typedef struct {
	RealD wetdays[MAX_LAYERS], vwcBulk[MAX_LAYERS], /* soil water content cm/cm */
	vwcMatric[MAX_LAYERS], swcBulk[MAX_LAYERS], /* soil water content cm/layer */
	swpMatric[MAX_LAYERS], /* soil water potential */
	swaBulk[MAX_LAYERS], /* available soil water cm/layer, swc-(wilting point) */
	SWA_VegType[NVEGTYPES][MAX_LAYERS],
	swaMatric[MAX_LAYERS],
	transp_total[MAX_LAYERS], transp[NVEGTYPES][MAX_LAYERS],
	evap[MAX_LAYERS],
	lyrdrain[MAX_LAYERS],
	hydred_total[MAX_LAYERS], hydred[NVEGTYPES][MAX_LAYERS], /* hydraulic redistribution cm/layer */
	surfaceWater, surfaceWater_evap,
	total_evap, evap_veg[NVEGTYPES],
	litter_evap,
	total_int, int_veg[NVEGTYPES], litter_int,
			snowpack,
			snowdepth,
			et,
			aet, tran, esoil, ecnw, esurf, esnow,
			pet, H_oh, H_ot, H_gh, H_gt,
			deep,
			sTemp[MAX_LAYERS], // soil temperature in celcius for each layer
			surfaceTemp; // soil surface temperature
} SW_SOILWAT_OUTPUTS;


#ifdef SWDEBUG
  #define N_WBCHECKS 8 // number of water balance checks
#endif

typedef struct {
	/* current daily soil water related values */
	Bool is_wet[MAX_LAYERS]; /* swc sufficient to count as wet today */
	RealD swcBulk[TWO_DAYS][MAX_LAYERS],
		SWA_VegType[TWO_DAYS][MAX_LAYERS],
		snowpack[TWO_DAYS], /* swe of snowpack, if accumulation flag set */
		snowdepth,
		transpiration[NVEGTYPES][MAX_LAYERS],
		evaporation[MAX_LAYERS],
		drain[MAX_LAYERS], /* amt of swc able to drain from curr layer to next */
		hydred[NVEGTYPES][MAX_LAYERS], /* hydraulic redistribution cm/layer */
		surfaceWater, surfaceWater_evap,
		pet, H_oh, H_ot, H_gh, H_gt,
		aet,
		litter_evap, evap_veg[NVEGTYPES],
		litter_int, int_veg[NVEGTYPES], // todays intercepted rain by litter and by vegetation
		sTemp[MAX_LAYERS],
		surfaceTemp; // soil surface temperature

	RealF swa_master[NVEGTYPES][NVEGTYPES][MAX_LAYERS]; // veg_type, crit_val, layer
	RealF dSWA_repartitioned_sum[NVEGTYPES][MAX_LAYERS];

	Bool soiltempError; // soil temperature error indicator
	#ifdef SWDEBUG
	int wbError[N_WBCHECKS]; /* water balance and water cycling error indicators (currently 8)
	    0, no error detected; > 0, number of errors detected */
  char *wbErrorNames[N_WBCHECKS];
  Bool is_wbError_init;
  #endif

	SW_SOILWAT_OUTPUTS
		*p_accu[SW_OUTNPERIODS], // output accumulator: summed values for each time period
		*p_oagg[SW_OUTNPERIODS]; // output aggregator: mean or sum for each time periods
	Bool hist_use;
	SW_SOILWAT_HIST hist;

} SW_SOILWAT;

void SW_SWC_construct(void);
void SW_SWC_deconstruct(void);
void SW_SWC_new_year(void);
void SW_SWC_read(void);
void SW_SWC_init_run(void);
void _read_swc_hist(TimeInt year);
void SW_SWC_water_flow(void);
void calculate_repartitioned_soilwater(void);
void SW_SWC_adjust_swc(TimeInt doy);
void SW_SWC_adjust_snow(RealD temp_min, RealD temp_max, RealD ppt, RealD *rain,
  RealD *snow, RealD *snowmelt);
RealD SW_SWC_snowloss(RealD pet, RealD *snowpack);
RealD SW_SnowDepth(RealD SWE, RealD snowdensity);
void SW_SWC_end_day(void);
RealD SW_VWCBulkRes(RealD fractionGravel, RealD sand, RealD clay, RealD porosity);
void get_dSWAbulk(int i);

double SW_SWRC_SWCtoSWP(double swcBulk, SW_LAYER_INFO *lyr);
double SWRC_SWCtoSWP(
	double swcBulk,
	unsigned int swrc_type,
	double *swrcp,
	double gravel,
	double width
);
double SWRC_SWCtoSWP_Campbell1974(
	double swcBulk,
	double *swrcp,
	double gravel,
	double width
);

RealD SW_SWRC_SWPtoSWC(RealD swpMatric, SW_LAYER_INFO *lyr);
double SWRC_SWPtoSWC(
	double swpMatric,
	unsigned int swrc_type,
	double *swrcp,
	double gravel,
	double width
);
double SWRC_SWPtoSWC_Campbell1974(
	double swpMatric,
	double *swrcp,
	double gravel,
	double width
);

#ifdef SWDEBUG
void SW_WaterBalance_Checks(void);
#endif

#ifdef DEBUG_MEM
void SW_SWC_SetMemoryRefs(void);
#endif


#ifdef __cplusplus
}
#endif

#endif
