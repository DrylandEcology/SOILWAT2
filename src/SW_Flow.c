/********************************************************/
/********************************************************/
/*	Source file: SW_Flow.c
 Type: module
 Application: SOILWAT - soilwater dynamics simulator
 Purpose: This is the interesting part of the model--
 the flow of water through the soil.

 ORIGINAL NOTES/COMMENTS
 ********************************************************************
 PURPOSE: Water-flow submodel.  This submodel is a rewrite of a
 model originally written by William Parton.  It simulates
 the flow of water through the plant canopy and soil.
 See "Abiotic Section of ELM", as a reference.
 The subroutines called are listed in the file "subwatr.f"

 HISTORY:
 4/30/92  (not tested - first pass)

 7/2/92   (SLC) Reset swc to 0 if less than 0.  (Due to roundoff)
 See function "chkzero" in the subwatr.f file.  Each
 swc is checked for negative values.

 1/17/94  (SLC) Set daily array to zero when no transpiration or
 evaporation.

 1/27/94  (TLB) Added daily deep drainage variable

 (4/10/2000) -- INITIAL CODING - cwb

 9/21/01	I have to make some transformation from the record-oriented
 structure of the new design to the state array parameter
 structure of the old design.  I thought it would be worth
 trying to rewrite the routines, but with the current version
 of the record layouts, it required too much coupling with the
 other modules, and I refrained for two reasons.  First was
 the time involved, and second was the possiblity of leaving
 the code in SoWat_flow_subs.c as is to facilitate putting
 it into a library.

 10/09/2009	(drs) added snow accumulation, snow sublimation
 and snow melt to SW_Water_Flow()

 01/12/2010	(drs) turned not-fuctional snow sublimation to snow_sublimation
 (void)

 02/02/2010	(drs) added to SW_Water_Flow(): saving values for standcrop_int,
 litter_int and soil_inf

 02/08/2010	(drs) if there is a snowpack then
 - rain infiltrates directly to soil (no vegetation or litter interception of
 today)
 - no transpiration or evaporation (except evaporation of yesterdays
 interception) only
 - infiltrate water high
 - infiltrate water low

 10/04/2010	(drs) moved call to SW_SWC_adjust_snow() back to
 SW_WTH_new_day()

 10/19/2010	(drs) added call to hydraulic_redistribution() in
 SW_Water_Flow() after all the evap/transp/infiltration is computed added
 temporary array lyrHydRed to arrays2records() and records2arrays()

 11/16/2010	(drs) added call to forest_intercepted_water() depending on
 SW_VegProd.Type..., added forest_h2o to trace forest intercepted water renamed
 standcrop_h2o_qum -> veg_int_storage renamed totstcr_h2o -> totveg_h2o

 01/03/2011	(drs) changed type of lyrTrRegions[MAX_LAYERS] from int to IntU
 to avoid warning message that ' pointer targets differ in signedness' in
 function transp_weighted_avg()

 01/04/2011	(drs) added snowmelt to h2o_for_soil after interception,
 otherwise last day of snowmelt (when snowpack is gone) wasn't made available
 and aet became too small

 01/06/2011	(drs) layer drainage was incorrectly calculated if hydraulic
 redistribution pumped water into a layer below pwp such that its swc was
 finally higher than pwp -> fixed it by first calculating hydraulic
 redistribution and then as final step infiltrate_water_low()

 01/06/2011	(drs) call to infiltrate_water_low() has to be the last swc
 affecting calculation

 02/19/2011	(drs) calculate runoff as adjustment to snowmelt events before
 infiltration

 02/22/2011	(drs) init aet for the day in SW_Water_Flow(), instead
 implicitely in evap_litter_veg()

 02/22/2011	(drs) added snowloss to AET

 07/21/2011	(drs) added module variables 'lyrImpermeability' and
 'lyrSWCBulk_Saturated' and initialize them in records2arrays()

 07/22/2011	(drs) adjusted soil_infiltration for pushed back water to
 surface (difference in standingWater)

 07/22/2011	(drs) included evaporation from standingWater into
 evap_litter_veg_surfaceWater() and reduce_rates_by_surfaceEvaporation(): it
 includes it to AET

 08/22/2011	(drs) in SW_Water_Flow(void): added snowdepth_scale = 1 - snow
 depth/vegetation height
 - vegetation interception = only when snowdepth_scale > 0, scaled by
 snowdepth_scale
 - litter interception = only when no snow cover
 - transpiration = only when snowdepth_scale > 0, scaled by snowdepth_scale
 - bare-soil evaporation = only when no snow cover

 09/08/2011	(drs) interception, evaporation from intercepted, E-T
 partitioning, transpiration, and hydraulic redistribution for each vegetation
 type (tree, shrub, grass) of SW_VegProd separately, scaled by their fractions
 replaced PET with unevaped in pot_soil_evap() and pot_transp(): to simulate
 reduction of atmospheric demand underneath canopies

 09/09/2011	(drs) moved transp_weighted_avg() from before infiltration and
 percolation to directly before call to pot_transp()

 09/21/2011	(drs)	scaled all (potential) evaporation and transpiration
 flux rates with PET: SW_Flow_lib.h: reduce_rates_by_unmetEvapDemand() is
 obsolete

 09/26/2011	(drs)	replaced all uses of monthly SW_VegProd and SW_Sky
 records with the daily replacements

 02/03/2012	(drs)	added variable 'snow_evap_rate': snow loss is part of
 aet and needs accordingly also to be scaled so that sum of all et rates is not
 more than pet

 01/28/2012	(drs)	transpiration can only remove water from soil down to
 lyrSWCBulk_Wiltpts (instead lyrSWCBulkmin)

 02/03/2012	(drs)	added 'lyrSWCBulk_HalfWiltpts' = 0.5 * SWC at -1.5 MPa
 soil evaporation extracts water down to 'lyrSWCBulk_HalfWiltpts' according to
 the FAO-56 model, e.g., Burt CM, Mutziger AJ, Allen RG, Howell TA (2005)
 Evaporation Research: Review and Interpretation. Journal of Irrigation and
 Drainage Engineering, 131, 37-58.

 02/04/2012	(drs)	added 'lyrSWCBulkatSWPcrit_xx' for each vegetation type
 transpiration can only remove water from soil down to 'lyrSWCBulkatSWPcrit_xx'
 (instead lyrSWCBulkmin)

 02/04/2012	(drs)	snow loss is fixed and can also include snow
 redistribution etc., so don't scale to PET

 05/25/2012  (DLM) added module level variables lyroldavgLyrTemp [MAX_LAYERS] &
 lyravgLyrTemp [MAX_LAYERS] to keep track of soil temperatures, added
 lyrbDensity to keep track of the bulk density for each layer

 05/25/2012  (DLM) edited records2arrays(void); & arrays2records(void);
 functions to move values to / from lyroldavgLyrTemp & lyrTemp & lyrbDensity

 05/25/2012  (DLM) added call to soil_temperature function in
 SW_Water_Flow(void)

 11/06/2012	(clk)	added slope and aspect to the call to petfunc()

 11/30/2012	(clk)	added lines to calculate the surface runoff and to
 adjust the surface water level based on that value

 01/31/2013	(clk)	With the addition of a new type of vegetation, bare
 ground, needed to add a new function call to pot_soil_evap_bs() and create a
 few new variables: RealD lyrEvap_BareGround [MAX_LAYERS] was created, RealD
 soil_evap_rate_bs was created, and initialized at 1.0, new function,
 pot_soil_evap_bs(), is called if fractionBareGround is not zero and there is no
 snowpack on the ground, Added soil_evap_rate_bs to rate_help, and then adjusted
 soil_evap_rate_bs by rate_help if needed, Added call to remove bare-soil evap
 from swv, And added lyrEvap_BareGround into the calculation for
 SW_Soilwat.evaporation. Also, added
 SW_W_VegProd.bare_cov.albedo*SW_VegProd.fractionBareGround to the paramater in
 petfunc() that was originally
 SW_VegProd.veg[SW_GRASS].cov.albedo*SW_VegProd.fractionGrass +
 SW_VegProd.shrub.cov.albedo*SW_VegProd.shrub.cov.fCover +
 SW_VegProd.tree.cov.albedo*SW_VegProd.tree.cov.fCover

 04/16/2013	(clk)	Renamed a lot of the variables to better reflect BULK
 versus MATRIC values updated the use of these variables in all the files

 06/24/2013	(rjm)	added 'soil_temp_error', 'soil_temp_init' and
 'fusion_pool_init' as global variable added function SW_FLW_construct() to init
 global variables between consecutive calls to SoilWat as dynamic library

 07/09/2013	(clk)	with the addition of forbs as a vegtype, needed to add a
 lot of calls to this code and so basically just copied and pasted the code for
 the other vegtypes

 09/26/2013 (drs) records2arrays(): Init hydraulic redistribution to zero; if
 not used and not initialized, then there could be non-zero values resulting

 06/23/2015 (akt)	Added surfaceAvg[Today] value at structure SW_Weather so
 that we can add surfaceAvg[Today] in output from Sw_Outout.c get_tmp() function

 02/08/2016 (CMA & CTD) Added snowpack as an input argument to function call of
 soil_temperature()

 02/08/2016 (CMA & CTD) Modified biomass to use the live biomass as opposed to
 standing crop
 */
/********************************************************/
/********************************************************/

/* =================================================== */
/*                INCLUDES / DEFINES                   */
/* --------------------------------------------------- */
#include "include/SW_Flow.h"         // for SW_FLW_init_run, SW_Water_Flow
#include "include/generic.h"         // for GT, fmax, EQ, fmin
#include "include/SW_datastructs.h"  // for LOG_INFO, SW_RUN, SW_SOILWAT
#include "include/SW_Defines.h"      // for ForEachVegType, NVEGTYPES, ForE...
#include "include/SW_Flow_lib.h"     // for evap_fromSurface, remove_from_soil
#include "include/SW_Flow_lib_PET.h" // for petfunc, solar_radiation
#include "include/SW_SoilWater.h"    // for SW_SWC_snowloss, SW_SnowDepth
#include "include/SW_Times.h"        // for Today, Yesterday
#include "include/SW_VegProd.h"      // for WUE_INDEX

/* =================================================== */
/*             Global Function Definitions             */
/* --------------------------------------------------- */
/* There is only one external function here and it is
 * only called from SW_Soilwat, so it is declared there.
 * but the compiler may complain if not predeclared here
 * This is a specific option for the compiler and may
 * not always occur.
 */

/**
@brief Initialize global variables between consecutive calls to SOILWAT.

@param[out] SW_SoilWatSim Struct of type SW_SOILWAT containing
    soil water simulation values
*/
void SW_FLW_init_run(SW_SOILWAT_SIM *SW_SoilWatSim) {
    /* 06/26/2013	(rjm) added function SW_FLW_init_run() to init global
     * variables between consecutive calls to SoilWat as dynamic library */
    int lyr;
    int k;


    // These only have to be cleared if a loop is wrong in the code.
    ForEachSoilLayer(lyr, MAX_LAYERS) {
        ForEachVegType(k) {
            SW_SoilWatSim->transpiration[k][lyr] = 0.;
            SW_SoilWatSim->hydred[k][lyr] = 0;
        }

        SW_SoilWatSim->swcBulk[Today][lyr] = 0.;

        SW_SoilWatSim->avgLyrTemp[lyr] = 0.;
    }

    // When running as a library make sure these are set to zero.
    SW_SoilWatSim->standingWater[0] = SW_SoilWatSim->standingWater[1] = 0.;
    SW_SoilWatSim->litter_int_storage = 0.;

    ForEachVegType(k) { SW_SoilWatSim->veg_int_storage[k] = 0.; }
}

/* *************************************************** */
/* *************************************************** */
/*            The Water Flow                           */
/* --------------------------------------------------- */
void SW_Water_Flow(SW_RUN *sw, LOG_INFO *LogInfo) {
#ifdef SWDEBUG
    IntUS debug = 0;
    IntUS debug_year = 1980;
    IntUS debug_doy = 350;
    double Eveg;
    double Tveg;
    double HRveg;
#endif

    double swpot_avg[NVEGTYPES];
    double transp_veg[NVEGTYPES];
    double transp_rate[NVEGTYPES];
    double soil_evap[NVEGTYPES];
    double soil_evap_rate[NVEGTYPES];
    double soil_evap_rate_bs = 1.;
    double surface_evap_veg_rate[NVEGTYPES];
    double surface_evap_litter_rate = 1.;
    double surface_evap_standingWater_rate = 1.;
    double h2o_for_soil = 0.;
    double snowmelt;
    double scale_veg[NVEGTYPES];
    double pet2;
    double peti;
    double rate_help;
    double x;
    double drainout = 0;
    double *standingWaterToday = &sw->SoilWatSim.standingWater[Today];
    double *standingWaterYesterday = &sw->SoilWatSim.standingWater[Yesterday];
    double snowdepth0;

    TimeInt doy;
    TimeInt month;
    int k;
    LyrIndex i;
    LyrIndex n_layers = sw->SiteSim.n_layers;

    double UpNeigh_lyrSWCBulk[MAX_LAYERS];
    double UpNeigh_lyrDrain[MAX_LAYERS];
    double UpNeigh_drainout;
    double UpNeigh_standingWater;

    doy = sw->ModelSim.doy;     /* base1 */
    month = sw->ModelSim.month; /* base0 */

#ifdef SWDEBUG
    if (debug && sw->ModelSim.year == debug_year &&
        sw->ModelSim.doy == debug_doy) {
        sw_printf("Flow (%d-%d): start:", sw->ModelSim.year, sw->ModelSim.doy);
        ForEachSoilLayer(i, n_layers) {
            sw_printf(" swc[%i]=%1.3f", i, sw->SoilWatSim.swcBulk[Today][i]);
        }
        sw_printf("\n");
    }
#endif


    if (sw->SiteIn.use_soil_temp && !sw->StRegSimVals.soil_temp_init) {
        /* We initialize soil temperature (and un/frozen state of soil layers)
                 before water flow of first day because we use un/frozen
           states), but calculate soil temperature at end of each day
        */
        SW_ST_setup_run(
            &sw->StRegSimVals,
            &sw->SiteIn,
            &sw->SiteSim,
            &sw->SoilWatSim.soiltempError,
            &sw->StRegSimVals.soil_temp_init,
            sw->WeatherSim.temp_avg,
            sw->SoilWatSim.swcBulk[Today],
            &sw->WeatherSim.surfaceAvg,
            sw->SoilWatSim.avgLyrTemp,
            sw->SoilWatSim.lyrFrozen,
            LogInfo
        );

        if (LogInfo->stopRun) {
            return; // Exit function prematurely due to error
        }
    }


    /* Solar radiation and PET */
    x = sw->VegProdIn.bare_cov.albedo * sw->VegProdIn.bare_cov.fCover;
    ForEachVegType(k) {
        x += sw->VegProdIn.veg[k].cov.albedo * sw->VegProdIn.veg[k].cov.fCover;
    }

    sw->SoilWatSim.H_gt = solar_radiation(
        &sw->AtmDemSim,
        doy,
        sw->ModelIn.latitude,
        sw->ModelIn.elevation,
        sw->ModelIn.slope,
        sw->ModelIn.aspect,
        x,
        &sw->WeatherSim.cloudCover,
        sw->WeatherSim.actualVaporPressure,
        sw->WeatherSim.shortWaveRad,
        sw->WeatherIn.desc_rsds,
        &sw->SoilWatSim.H_oh,
        &sw->SoilWatSim.H_ot,
        &sw->SoilWatSim.H_gh,
        LogInfo
    );
    if (LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

    sw->SoilWatSim.pet = sw->SiteIn.pet_scale * petfunc(
                                                    sw->SoilWatSim.H_gt,
                                                    sw->WeatherSim.temp_avg,
                                                    sw->ModelIn.elevation,
                                                    x,
                                                    sw->WeatherSim.relHumidity,
                                                    sw->WeatherSim.windSpeed,
                                                    sw->WeatherSim.cloudCover,
                                                    LogInfo
                                                );
    if (LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }


    /* snowdepth scaling based on snowpack at start of day (before snowloss) */
    snowdepth0 = SW_SnowDepth(
        sw->SoilWatSim.snowpack[Today], sw->SkyIn.snow_density_daily[doy]
    );
    /* if snow depth is deeper than vegetation height then
     - rain and snowmelt infiltrates directly to soil (no vegetation or litter
     interception of today) only
     - evaporation of yesterdays interception
     - infiltrate water high
     - infiltrate water low */

    ForEachVegType(k) {
        scale_veg[k] = sw->VegProdIn.veg[k].cov.fCover;

        if (GT(sw->VegProdIn.veg[k].veg_height_daily[doy], 0.)) {
            scale_veg[k] *=
                1. - snowdepth0 / sw->VegProdIn.veg[k].veg_height_daily[doy];
        }
    }

    /* Rainfall interception */
    /* ppt is partioned into ppt = snow + rain */
    h2o_for_soil = sw->WeatherSim.rain;

    ForEachVegType(k) {
        if (GT(h2o_for_soil, 0.) && GT(scale_veg[k], 0.)) {
            /* canopy interception only if rainfall, if vegetation cover > 0,
               and if plants are taller than snowpack depth */
            veg_intercepted_water(
                &h2o_for_soil,
                &sw->SoilWatSim.int_veg[k],
                &sw->SoilWatSim.veg_int_storage[k],
                sw->SkyIn.n_rain_per_day[month],
                sw->VegProdIn.veg[k].veg_kSmax,
                sw->VegProdIn.veg[k].bLAI_total_daily[doy],
                scale_veg[k]
            );

        } else {
            sw->SoilWatSim.int_veg[k] = 0.;
        }
    }

    sw->SoilWatSim.litter_int = 0.;

    if (GT(h2o_for_soil, 0.) && EQ(sw->SoilWatSim.snowpack[Today], 0.)) {
        /* litter interception only when no snow and if rainfall reaches litter
         */
        ForEachVegType(k) {
            if (GT(sw->VegProdIn.veg[k].cov.fCover, 0.)) {
                litter_intercepted_water(
                    &h2o_for_soil,
                    &sw->SoilWatSim.litter_int,
                    &sw->SoilWatSim.litter_int_storage,
                    sw->SkyIn.n_rain_per_day[month],
                    sw->VegProdIn.veg[k].lit_kSmax,
                    sw->VegProdIn.veg[k].litter_daily[doy],
                    sw->VegProdIn.veg[k].cov.fCover
                );
            }
        }
    }

    /* End Interception */


    /* Surface water */
    *standingWaterToday = *standingWaterYesterday;

    /* Snow melt infiltrates un-intercepted */
    /* amount of snowmelt is changed by runon/off as percentage */
    snowmelt = fmax(
        0., sw->WeatherSim.snowmelt * (1. - sw->WeatherIn.pct_snowRunoff / 100.)
    );
    sw->WeatherSim.snowRunoff = sw->WeatherSim.snowmelt - snowmelt;
    h2o_for_soil += snowmelt;

    /*
    @brief Surface water runon:

    Proportion of water that arrives at surface added as daily
    runon from a hypothetical identical neighboring upslope site.

    @param percentRunon Value ranges between 0 and +inf;
        0 = no runon, >0 runon is occurring.
    */
    if (GT(sw->SiteIn.percentRunon, 0.)) {
        // Calculate 'rain + snowmelt - interception - infiltration' for upslope
        // neighbor Copy values to simulate identical upslope neighbor site
        ForEachSoilLayer(i, n_layers) {
            UpNeigh_lyrSWCBulk[i] = sw->SoilWatSim.swcBulk[Today][i];
            UpNeigh_lyrDrain[i] = sw->SoilWatSim.drain[i];
        }
        UpNeigh_drainout = drainout;
        UpNeigh_standingWater = *standingWaterToday;

        // Infiltrate for upslope neighbor under saturated soil conditions
        infiltrate_water_high(
            UpNeigh_lyrSWCBulk,
            UpNeigh_lyrDrain,
            &UpNeigh_drainout,
            h2o_for_soil,
            n_layers,
            sw->SiteSim.swcBulk_fieldcap,
            sw->SiteSim.swcBulk_saturated,
            sw->SiteSim.ksat,
            sw->SiteIn.soils.impermeability,
            &UpNeigh_standingWater,
            sw->SoilWatSim.lyrFrozen
        );

        // Runon as percentage from today's surface water addition on upslope
        // neighbor
        sw->WeatherSim.surfaceRunon = fmax(
            0.,
            (UpNeigh_standingWater - sw->SoilWatSim.standingWater[Yesterday]) *
                sw->SiteIn.percentRunon
        );
        sw->SoilWatSim.standingWater[Today] += sw->WeatherSim.surfaceRunon;

    } else {
        sw->WeatherSim.surfaceRunon = 0.;
    }

    /* Soil infiltration */
    sw->WeatherSim.soil_inf = h2o_for_soil;

    /* Percolation under saturated soil conditions */
    sw->WeatherSim.soil_inf += *standingWaterToday;
    infiltrate_water_high(
        sw->SoilWatSim.swcBulk[Today],
        sw->SoilWatSim.drain,
        &drainout,
        h2o_for_soil,
        n_layers,
        sw->SiteSim.swcBulk_fieldcap,
        sw->SiteSim.swcBulk_saturated,
        sw->SiteSim.ksat,
        sw->SiteIn.soils.impermeability,
        standingWaterToday,
        sw->SoilWatSim.lyrFrozen
    );

    // adjust soil_infiltration for not infiltrated surface water
    sw->WeatherSim.soil_inf -= *standingWaterToday;

#ifdef SWDEBUG
    if (debug && sw->ModelSim.year == debug_year &&
        sw->ModelSim.doy == debug_doy) {
        sw_printf(
            "Flow (%d-%d): satperc:", sw->ModelSim.year, sw->ModelSim.doy
        );
        ForEachSoilLayer(i, n_layers) {
            sw_printf(" swc[%i]=%1.3f", i, sw->SoilWatSim.swcBulk[Today][i]);
        }
        sw_printf("\n              : satperc:");
        ForEachSoilLayer(i, n_layers) {
            sw_printf(" perc[%d]=%1.3f", i, sw->SoilWatSim.drain[i]);
        }
        sw_printf("\n");
    }
#endif

    /*
    @brief Surface water runoff:

    Proportion of ponded surface water removed as daily runoff.

    @param percentRunoff Value ranges between 0 and 1;
        0 = no loss of surface water, 1 = all ponded water lost via runoff.
    */
    if (GT(sw->SiteIn.percentRunoff, 0.)) {
        sw->WeatherSim.surfaceRunoff =
            *standingWaterToday * sw->SiteIn.percentRunoff;
        *standingWaterToday =
            fmax(0., (*standingWaterToday - sw->WeatherSim.surfaceRunoff));

    } else {
        sw->WeatherSim.surfaceRunoff = 0.;
    }

    // end surface water and infiltration


    /* Potential bare-soil evaporation rates */
    if (GT(sw->VegProdIn.bare_cov.fCover, 0.) &&
        EQ(sw->SoilWatSim.snowpack[Today], 0.)) {
        /* bare ground present AND no snow on ground */
        pot_soil_evap_bs(
            &soil_evap_rate_bs,
            &sw->SiteIn,
            &sw->SiteSim,
            sw->SiteSim.n_evap_lyrs,
            sw->SoilWatSim.pet,
            sw->SiteIn.evap.xinflec,
            sw->SiteIn.evap.slope,
            sw->SiteIn.evap.yinflec,
            sw->SiteIn.evap.range,
            sw->SoilWatSim.swcBulk[Today],
            LogInfo
        );
        if (LogInfo->stopRun) {
            return; // Exit function prematurely due to error
        }

        soil_evap_rate_bs *= sw->VegProdIn.bare_cov.fCover;

    } else {
        soil_evap_rate_bs = 0;
    }


    /* Potential transpiration & bare-soil evaporation rates */
    ForEachVegType(k) {
        if (GT(scale_veg[k], 0.)) {
            /* vegetation type k present AND not fully covered in snow */
            EsT_partitioning(
                &soil_evap[k],
                &transp_veg[k],
                sw->VegProdIn.veg[k].lai_live_daily[doy],
                sw->VegProdIn.veg[k].EsTpartitioning_param
            );

            if (EQ(sw->SoilWatSim.snowpack[Today], 0.)) {
                /* bare-soil evaporation only when no snow */
                pot_soil_evap(
                    &sw->SiteIn,
                    &sw->SiteSim,
                    sw->SiteSim.n_evap_lyrs,
                    sw->VegProdIn.veg[k].total_agb_daily[doy],
                    soil_evap[k],
                    sw->SoilWatSim.pet,
                    sw->SiteIn.evap.xinflec,
                    sw->SiteIn.evap.slope,
                    sw->SiteIn.evap.yinflec,
                    sw->SiteIn.evap.range,
                    sw->SoilWatSim.swcBulk[Today],
                    sw->VegProdIn.veg[k].Es_param_limit,
                    &soil_evap_rate[k],
                    LogInfo
                );
                if (LogInfo->stopRun) {
                    return; // Exit function prematurely due to error
                }

                soil_evap_rate[k] *= sw->VegProdIn.veg[k].cov.fCover;

            } else {
                soil_evap_rate[k] = 0.;
            }

            transp_weighted_avg(
                &swpot_avg[k],
                &sw->SiteIn,
                &sw->SiteSim,
                sw->SiteSim.n_transp_rgn,
                sw->SiteSim.n_transp_lyrs[k],
                sw->SiteSim.my_transp_rgn[k],
                sw->SoilWatSim.swcBulk[Today],
                k,
                LogInfo
            );
            if (LogInfo->stopRun) {
                return; // Exit function prematurely due to error
            }

            pot_transp(
                &transp_rate[k],
                swpot_avg[k],
                sw->VegProdIn.veg[k].biolive_daily[doy],
                sw->VegProdIn.veg[k].biodead_daily[doy],
                transp_veg[k],
                sw->SoilWatSim.pet,
                sw->SiteIn.transp.xinflec,
                sw->SiteIn.transp.slope,
                sw->SiteIn.transp.yinflec,
                sw->SiteIn.transp.range,
                sw->VegProdIn.veg[k].shade_scale,
                sw->VegProdIn.veg[k].shade_deadmax,
                sw->VegProdIn.veg[k].tr_shade_effects.xinflec,
                sw->VegProdIn.veg[k].tr_shade_effects.slope,
                sw->VegProdIn.veg[k].tr_shade_effects.yinflec,
                sw->VegProdIn.veg[k].tr_shade_effects.range,
                sw->VegProdIn.veg[k]
                    .co2_multipliers[WUE_INDEX][sw->ModelSim.simyear]
            );

            transp_rate[k] *= scale_veg[k];

        } else {
            soil_evap_rate[k] = 0.;
            transp_rate[k] = 0.;
        }
    }


    /* Snow sublimation takes precedence over other ET fluxes:
            see functions `SW_SWC_adjust_snow` and `SW_SWC_snowloss`*/
    sw->WeatherSim.snowloss =
        SW_SWC_snowloss(sw->SoilWatSim.pet, &sw->SoilWatSim.snowpack[Today]);

    /* Calculate snowdepth for output based on today's final snowpack */
    sw->SoilWatSim.snowdepth = SW_SnowDepth(
        sw->SoilWatSim.snowpack[Today], sw->SkyIn.snow_density_daily[doy]
    );

    pet2 = fmax(0., sw->SoilWatSim.pet - sw->WeatherSim.snowloss);

    /* Potential evaporation rates of intercepted and surface water */
    peti = pet2;
    ForEachVegType(k) {
        surface_evap_veg_rate[k] = fmax(
            0., fmin(peti * scale_veg[k], sw->SoilWatSim.veg_int_storage[k])
        );
        peti -= surface_evap_veg_rate[k] / scale_veg[k];
    }

    surface_evap_litter_rate =
        fmax(0., fmin(peti, sw->SoilWatSim.litter_int_storage));
    peti -= surface_evap_litter_rate;
    surface_evap_standingWater_rate = fmax(0., fmin(peti, *standingWaterToday));

    /* Scale all (potential) evaporation and transpiration flux rates to (PET -
     * Esnow) */
    rate_help = surface_evap_litter_rate + surface_evap_standingWater_rate +
                soil_evap_rate_bs;
    ForEachVegType(k) {
        rate_help +=
            surface_evap_veg_rate[k] + soil_evap_rate[k] + transp_rate[k];
    }

    if (GT(rate_help, pet2)) {
        rate_help = pet2 / rate_help;

        ForEachVegType(k) {
            surface_evap_veg_rate[k] *= rate_help;
            soil_evap_rate[k] *= rate_help;
            transp_rate[k] *= rate_help;
        }

        surface_evap_litter_rate *= rate_help;
        surface_evap_standingWater_rate *= rate_help;
        soil_evap_rate_bs *= rate_help;
    }

    /* Start adding components to AET */
    sw->SoilWatSim.aet = sw->WeatherSim.snowloss; /* init aet for the day */

    /* Evaporation of intercepted and surface water */
    ForEachVegType(k) {
        evap_fromSurface(
            &sw->SoilWatSim.veg_int_storage[k],
            &surface_evap_veg_rate[k],
            &sw->SoilWatSim.aet
        );
        sw->SoilWatSim.evap_veg[k] = surface_evap_veg_rate[k];
    }

    evap_fromSurface(
        &sw->SoilWatSim.litter_int_storage,
        &surface_evap_litter_rate,
        &sw->SoilWatSim.aet
    );
    evap_fromSurface(
        &sw->SoilWatSim.standingWater[Today],
        &surface_evap_standingWater_rate,
        &sw->SoilWatSim.aet
    );

    sw->SoilWatSim.litter_evap = surface_evap_litter_rate;
    sw->SoilWatSim.surfaceWater_evap = surface_evap_standingWater_rate;


    /* bare-soil evaporation */
    ForEachEvapLayer(i, sw->SiteSim.n_evap_lyrs) {
        // init to zero for today
        sw->SoilWatSim.evap_baresoil[i] = 0;
    }

    if (GT(sw->VegProdIn.bare_cov.fCover, 0.) &&
        EQ(sw->SoilWatSim.snowpack[Today], 0.)) {
        /* remove bare-soil evap from swv */
        remove_from_soil(
            sw->SoilWatSim.swcBulk[Today],
            sw->SoilWatSim.evap_baresoil,
            &sw->SiteIn,
            &sw->SiteSim,
            &sw->SoilWatSim.aet,
            sw->SiteSim.n_evap_lyrs,
            sw->SiteIn.soils.evap_coeff,
            soil_evap_rate_bs,
            sw->SiteSim.swcBulk_halfwiltpt,
            sw->SoilWatSim.lyrFrozen,
            LogInfo
        );

        if (LogInfo->stopRun) {
            return; // Exit function prematurely due to error
        }
    }

#ifdef SWDEBUG
    if (debug && sw->ModelSim.year == debug_year &&
        sw->ModelSim.doy == debug_doy) {
        sw_printf("Flow (%d-%d): Esoil:", sw->ModelSim.year, sw->ModelSim.doy);
        ForEachSoilLayer(i, n_layers) {
            sw_printf(" swc[%i]=%1.3f", i, sw->SoilWatSim.swcBulk[Today][i]);
        }
        sw_printf("\n              : Esoil:");
        ForEachSoilLayer(i, n_layers) {
            sw_printf(" Esoil[%d]=%1.3f", i, sw->SoilWatSim.evap_baresoil[i]);
        }
        sw_printf("\n");
    }
#endif

    /* Vegetation transpiration and bare-soil evaporation */
    ForEachVegType(k) {
        ForEachSoilLayer(i, n_layers) {
            // init to zero for today
            sw->SoilWatSim.transpiration[k][i] = 0;
        }

        if (GT(scale_veg[k], 0.)) {
            /* remove bare-soil evap from swc */
            remove_from_soil(
                sw->SoilWatSim.swcBulk[Today],
                sw->SoilWatSim.evap_baresoil,
                &sw->SiteIn,
                &sw->SiteSim,
                &sw->SoilWatSim.aet,
                sw->SiteSim.n_evap_lyrs,
                sw->SiteIn.soils.evap_coeff,
                soil_evap_rate[k],
                sw->SiteSim.swcBulk_halfwiltpt,
                sw->SoilWatSim.lyrFrozen,
                LogInfo
            );
            if (LogInfo->stopRun) {
                return; // Exit function prematurely due to error
            }

            /* remove transp from swc */
            remove_from_soil(
                sw->SoilWatSim.swcBulk[Today],
                sw->SoilWatSim.transpiration[k],
                &sw->SiteIn,
                &sw->SiteSim,
                &sw->SoilWatSim.aet,
                sw->SiteSim.n_transp_lyrs[k],
                sw->SiteIn.soils.transp_coeff[k],
                transp_rate[k],
                sw->SiteSim.swcBulk_atSWPcrit[k],
                sw->SoilWatSim.lyrFrozen,
                LogInfo
            );
            if (LogInfo->stopRun) {
                return; // Exit function prematurely due to error
            }
        }
    }

#ifdef SWDEBUG
    if (debug && sw->ModelSim.year == debug_year &&
        sw->ModelSim.doy == debug_doy) {
        sw_printf("Flow (%d-%d): ETveg:", sw->ModelSim.year, sw->ModelSim.doy);
        ForEachSoilLayer(i, n_layers) {
            sw_printf(" swc[%i]=%1.3f", i, sw->SoilWatSim.swcBulk[Today][i]);
        }
        sw_printf("\n              : ETveg:");
        Eveg = 0.;
        ForEachSoilLayer(i, n_layers) {
            Tveg = 0.;
            Eveg += sw->SoilWatSim.evap_baresoil[i];
            ForEachVegType(k) { Tveg += sw->SoilWatSim.transpiration[k][i]; }
            sw_printf(" Tveg[%d]=%1.3f/Eveg=%1.3f", i, Tveg, Eveg);
        }
        sw_printf("\n");
    }
#endif


    /* Hydraulic redistribution */
    ForEachVegTypeBottomUp(k) {
        if (sw->VegProdIn.veg[k].flagHydraulicRedistribution &&
            GT(sw->VegProdIn.veg[k].cov.fCover, 0.) &&
            GT(sw->VegProdIn.veg[k].biolive_daily[doy], 0.)) {

            hydraulic_redistribution(
                sw->SoilWatSim.swcBulk[Today],
                sw->SoilWatSim.hydred[k],
                &sw->SiteIn,
                &sw->SiteSim,
                k,
                n_layers,
                sw->SoilWatSim.lyrFrozen,
                sw->VegProdIn.veg[k].maxCondroot,
                sw->VegProdIn.veg[k].swpMatric50,
                sw->VegProdIn.veg[k].shapeCond,
                sw->VegProdIn.veg[k].cov.fCover,
                sw->ModelSim.year,
                sw->ModelSim.doy,
                LogInfo
            );
            if (LogInfo->stopRun) {
                return; // Exit function prematurely due to error
            }

        } else {
            /* Set daily array to zero */
            ForEachSoilLayer(i, n_layers) { sw->SoilWatSim.hydred[k][i] = 0.; }
        }
    }

#ifdef SWDEBUG
    if (debug && sw->ModelSim.year == debug_year &&
        sw->ModelSim.doy == debug_doy) {
        sw_printf("Flow (%d-%d): HR:", sw->ModelSim.year, sw->ModelSim.doy);
        ForEachSoilLayer(i, n_layers) {
            sw_printf(" swc[%i]=%1.3f", i, sw->SoilWatSim.swcBulk[Today][i]);
        }
        sw_printf("\n              : HR:");
        ForEachSoilLayer(i, n_layers) {
            HRveg = 0.;
            ForEachVegType(k) { HRveg += sw->SoilWatSim.hydred[k][i]; }
            sw_printf(" HRveg[%d]=%1.3f", i, HRveg);
        }
        sw_printf("\n");
    }
#endif


    /* Calculate percolation for unsaturated soil water conditions. */
    /* 01/06/2011	(drs) call to percolate_unsaturated() has to be the last
       swc affecting calculation */

    sw->WeatherSim.soil_inf += *standingWaterToday;

    /* Unsaturated percolation based on Parton 1978, Black et al. 1969 */
    percolate_unsaturated(
        sw->SoilWatSim.swcBulk[Today],
        sw->SoilWatSim.drain,
        &drainout,
        standingWaterToday,
        n_layers,
        sw->SoilWatSim.lyrFrozen,
        &sw->SiteIn,
        &sw->SiteSim,
        sw->SiteIn.slow_drain_coeff,
        SLOW_DRAIN_DEPTH
    );

    // adjust soil_infiltration for water pushed back to surface
    sw->WeatherSim.soil_inf -= *standingWaterToday;

    sw->SoilWatSim.surfaceWater = *standingWaterToday;

#ifdef SWDEBUG
    if (debug && sw->ModelSim.year == debug_year &&
        sw->ModelSim.doy == debug_doy) {
        sw_printf(
            "Flow (%d-%d): unsatperc:", sw->ModelSim.year, sw->ModelSim.doy
        );
        ForEachSoilLayer(i, n_layers) {
            sw_printf(" swc[%i]=%1.3f", i, sw->SoilWatSim.swcBulk[Today][i]);
        }
        sw_printf("\n              : satperc:");
        ForEachSoilLayer(i, n_layers) {
            sw_printf(" perc[%d]=%1.3f", i, sw->SoilWatSim.drain[i]);
        }
        sw_printf("\n");
    }
#endif


    /* Soil Temperature starts here */

    // computing the live biomass real quickly to condense the call to
    // soil_temperature
    x = 0.;
    ForEachVegType(k) {
        if (k == SW_TREES || k == SW_SHRUB) {
            // changed to exclude tree biomass, bMatric/c it was breaking the
            // soil_temperature function
            x += sw->VegProdIn.veg[k].biolive_daily[doy] *
                 sw->VegProdIn.veg[k].cov.fCover;
        } else {
            x += sw->VegProdIn.veg[k].biomass_daily[doy] *
                 sw->VegProdIn.veg[k].cov.fCover;
        }
    }

    // soil_temperature function computes the soil temp for each layer and
    // stores it in lyravgLyrTemp doesn't affect SWC at all (yet), but needs it
    // for the calculation, so therefore the temperature is the last calculation
    // done
    if (sw->SiteIn.use_soil_temp) {
        soil_temperature(
            &sw->StRegSimVals,
            &sw->WeatherSim.surfaceMin,
            &sw->WeatherSim.surfaceAvg,
            &sw->WeatherSim.surfaceMax,
            sw->SoilWatSim.minLyrTemperature,
            sw->SoilWatSim.avgLyrTemp,
            sw->SoilWatSim.maxLyrTemperature,
            sw->SoilWatSim.lyrFrozen,
            sw->SiteIn.methodSurfaceTemperature,
            sw->SoilWatSim.snowpack[Today],
            sw->WeatherSim.temp_min,
            sw->WeatherSim.temp_avg,
            sw->WeatherSim.temp_max,
            sw->SoilWatSim.H_gt,
            sw->SoilWatSim.pet,
            sw->SoilWatSim.aet,
            x,
            sw->SoilWatSim.swcBulk[Today],
            sw->SiteSim.swcBulk_saturated,
            sw->SiteSim.soilBulk_density,
            sw->SiteIn.soils.width,
            sw->SiteIn.soils.depths,
            n_layers,
            sw->SiteIn.bmLimiter,
            sw->SiteIn.t1Param1,
            sw->SiteIn.t1Param2,
            sw->SiteIn.t1Param3,
            sw->SiteIn.csParam1,
            sw->SiteIn.csParam2,
            sw->SiteIn.shParam,
            sw->SiteIn.Tsoil_constant,
            sw->SiteIn.stDeltaX,
            sw->SiteIn.stMaxDepth,
            sw->SiteSim.stNRGR,
            sw->ModelSim.year,
            sw->ModelSim.doy,
            &sw->SoilWatSim.soiltempError,
            LogInfo
        );

        if (LogInfo->stopRun) {
            return; // Exit function prematurely due to error
        }
    }

/* Soil Temperature ends here */

/* Finalize "flow" of today */
#ifdef SWDEBUG
    if (debug) {
        if (sw->SiteIn.deepdrain) {
            if (!EQ(sw->SoilWatSim.drain[sw->SiteSim.deep_lyr], drainout)) {
                sw_printf(
                    "Percolation (%f) of last layer [%d] is not equal to deep "
                    "drainage (%f).\n",
                    sw->SoilWatSim.drain[sw->SiteSim.deep_lyr],
                    sw->SiteSim.deep_lyr + 1,
                    drainout
                );
            }
        }
    }
#endif

    *standingWaterYesterday = *standingWaterToday;

} /* END OF WATERFLOW */
