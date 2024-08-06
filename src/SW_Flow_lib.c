/********************************************************/
/********************************************************/
/*	Source file: SW_Flow_lib.c
 Type: module
 Application: SOILWAT - soilwater dynamics simulator
 Purpose: Water flow subroutines that can be used
 as a more or less independent library of
 soil water flow routines.  These routines
 are designed to operate independently of
 the soilwater model's data structures.

 See Also: SoWat_flow.c  SoWat_flow_subs.c

History:
(4/10/2000) -- INITIAL CODING - cwb

10/19/2010	(drs) added function hydraulic_redistribution()

11/16/2010	(drs) added function forest_intercepted_water()
renamed evap_litter_stdcrop() -> evap_litter_veg()

01/06/2011	(drs) drainout is calculated in infiltrate_water_high() and
infiltrate_water_low(), but was not added up -> fixed it by changing (*drainout)
= drainlw to (*drainout) += drainlw in function infiltrate_water_low()

01/06/2011	(drs) drainlw was not set to 0. in the for-loop, i.e. a layer
with swc below pwp inherited incorrectly drainlw from layer above instead of
drainlw=0

02/22/2011	(drs) evap_litter_veg() set aet implicitely to 0. and then added
litter evap; changed such that litter evap is now added to whatever value aet
has previously

07/21/2011	(drs) infiltrate_water_high() & infiltrate_water_low();
- added variables: saturated water content - swcsat, impermeability, water
standing above soil - standingWater
- percolation is adjusted by (1-impermeability)
- if a lower soil layer becomes saturated then water is pushed back up even
above the soil surface into standingWater

07/22/2011	(drs) included evaporation from standingWater into
evap_litter_veg_surfaceWater() previously called evap_litter_veg()

07/22/2011	(drs) included evaporation from standingWater into
reduce_rates_by_surfaceEvaporation() previously called
reduce_rates_by_intercepted()

08/22/2011	(drs) renamed bs_ev_tr_loss() to EsT_partitioning()

08/22/2011	(drs) added EsT_partitioning_forest() to partition bare-soil
evaporation (Es) and transpiration (T) in forests

09/08/2011	(drs) renamed stcrp_intercepted_water() to
grass_intercepted_water(); added shrub_intercepted_water() as current copy from
grass_intercepted_water(); added double scale to each xx_intercepted_water()
function to scale for snowdepth effects and vegetation type fraction added
double a,b,c,d to each xx_intercepted_water() function for for parameters in
intercepted rain = (a + b*veg) + (c+d*veg) * ppt

09/09/2011	(drs) added xx_EsT_partitioning() for each vegetation type
(grass, shrub, tree); added double lai_param as parameter in exp-equation

09/09/2011	(drs) replaced evap_litter_veg_surfaceWater() with
evap_fromSurface() to be called for each surface water pool seperately

09/09/2011	(drs) replaced SHD_xx constanst with input parameters in
pot_transp()

09/09/2011	(drs) added double Es_param_limit to pot_soil_evap() to scale
and limit bare-soil evaporation rate (previously hidden constant in code)

09/09/2011	(drs) renamed reduce_rates_by_surfaceEvaporation() to
reduce_rates_by_unmetEvapDemand()

09/11/2011	(drs) added double scale to hydraulic_redistribution() function
to scale for vegetation type fraction

09/13/2011	(drs)	added double scale, a,b,c,d to each
litter_intercepted_water() function for parameters in litter intercepted rain =
((a + b*blitter) + (c+d*blitter) * ppt) * scale

09/21/2011	(drs)	reduce_rates_by_unmetEvapDemand() is obsolete, complete
E and T scaling in SW_Flow.c

01/28/2012	(drs)	unsaturated flow drains now to swcmin instead of swcwp:
changed infiltrate_water_low() accordingly

02/04/2012	(drs)	in 'remove_from_soil': control that swc_avail >= 0

03/23/2012	(drs)	excluded hydraulic redistribution from top soil layer
(assuming that this layer is <= 5 cm deep)

03/26/2012	(drs)	infiltrate_water_low(): allow unsaturated percolation in
first layer

05/24/2012  (DLM) started coding soil_temperature function

05/25/2012  (DLM)  added all this fun crazy linear regression stuff to
soil_temperature function

05/29/2012  (DLM) still working on soil_temperature function, linear regression
stuff should work now

05/30/2012  (DLM) got rid of nasty segmentation fault error in soil_temperature
function, also tested math seems correct after checking by hand.  added the
ability to change the value of deltaX

05/30/2012  (DLM) added soil_temp_error variable... it simply keeps track of
whether or not an error has been reported in the soil_temperature function.  0
for no, 1 for yes.

05/30/2012  (DLM) added # of lyrs check & maxdepth check at the beginning of
soil_temperature function to make sure code doesn't blow up... if there isn't
enough lyrs (ie < 2) or the maxdepth is too little (ie < deltaX * 2), the
function quits out and reports an error to the user

05/31/2012  (DLM) added theMaxDepth variable to soil_temperature function to
allow the changing of the maxdepth of the equation, also soil_temperature()
function now stores most regression data in a structure to reduce redundant
regression calculations & speeds things up

05/31/2012  (DLM) added soil_temperature_init() function for use in the
soil_temperature function to initialize the values for use in the regressions...
it is not apart of the header file, because it's not meant to be an external
function

06/01/2012  (DLM) edited soil_temperature function(), changed deltaT variable
from hours to seconds, also changed some of the regression calculations so that
swc, fc, & wp regressions are scaled properly... results are actually starting
to look usable!

06/13/2012  (DLM) soil_temperature function no longer extrapolates values for
regression layers that are out of the bounds of the soil layers... instead they
are now set to the last soil layers values.  extrapolating code is still in the
function and can be commented out and used if wishing to go back to
extrapolating the values... 10/11/2012	(drs) petfunc(): annotated all
equations, wind is now [m/s] at 2-m above ground (instead of miles/h);
11/06/2012	(clk) petfunc(): added slope and aspect into the calculation for
solar radiation

01/31/2013	(clk)	Added new function, pot_soil_evap_bs() to the code. This
function is similar to pot_soil_evap() but since it deals with bare ground as
the type of vegetation, doesn't need several parameters, i.e. totagb, which also
simplifies the function.

03/07/2013	(clk)	In the functions soil_temperature and
soil_temperature_init, added code to determine whether or not a soil layer was
frozen Used the Eitzinger 2000 article, equation 3 to calculate the fusion pool
that needed to be accounted for when freezing/thawing since, when
freezing/thawing something, the temperature will remain the same until all of
that material has froze/thawed. The amount of energy needed to do this is the
fusion pool. The the rest of the temperature change can be adjusted from there.
Also, now that we are incorporating freezing soil layers into the model, needed
to account for these frozen layers with the soil water content movement. Needed
to adjust the functions infiltrate_water_high(), evap_from_soil(),
infiltrate_water_low(), and hydraulic_redistribution() Adjusted these function
so that they only made changes to the output as long as the current soil layer
was not frozen. In the case of drainage though, needed to make sure that the
layer below the current layer was not frozen, to make sure that the drainage had
somewhere to go.

03/28/2013	(clk) 	Changed the soil_temperature function to use the actual
soil temp values when determining for freezing soils, instead of using the
regression values. Since the regression values were no longer being updated when
the temperature would change this way, made the function call
soil_temperature_init everytime that the soil temperature would change due to
freezing/thawing of the soil. Also moved the initialization of oldsFusionPool to
its own function becuase you are now possibly calling soil_temperature_init
multiple times, to adjust the temperatures, and don't want to also initialize
the fusion pools again.

04/04/2013	(clk)	In infiltrate_water_high(), needed to also change the
lines swc[0] += pptleft;
(*standingWater) = 0.;
to only happen when the top soil layer is not frozen.

06/24/2013	(rjm)	made 'soil_temp_error', 'soil_temp_init' and
'fusion_pool_init' into global variable (instead of module-level) and moved them
to SW_Flow.c: otherwise they will not get reset to 0 (in call to construct)
between consecutive calls as a dynamic library

07/09/2013	(clk)	initialized the two new functions:
forb_intercepted_water and forb_EsT_partitioning

01/05/2016 (drs)	added new function set_frozen_unfrozen() to determine
frozen/unfrozen status of soil layers based on criteria by Parton et al. 1998
GCB re-wrote code for consequences of a frozen soil layer: now based on Parton
et al. 1998 GCB
                                                - infiltrate_water_high(): if
frozen, saturated hydraulic conductivity is reduced to 1%; infiltration is not
impeded by a frozen soil
                                                - infiltrate_water_low(): if
frozen, unsaturated hydraulic conductivity is reduced to 1%
                                                - hydraulic_redistribution(): no
hd between two layers if at least one is frozen
                                                - remove_from_soil(): no
evaporation and transpiration from a frozen soil layer

02/08/2016 (CMA & CTD) In the function surface_temperature_under_snow(), used
Parton's Eq. 5 & 6 from 1998 paper instead of koren paper Adjusted function
calls to surface_temperature_under_snow to account for the new parameters

03/01/2016 (CTD) Added error check for Rsoilwat called tempError()
*/
/********************************************************/
/********************************************************/

/* =================================================== */
/*                INCLUDES / DEFINES                   */
/* --------------------------------------------------- */
#include "include/SW_Flow_lib.h"    // for EsT_partitioning, SW_ST_init_run
#include "include/filefuncs.h"      // for LogError
#include "include/generic.h"        // for interpolation, GT, LT, fmax, fmin
#include "include/SW_datastructs.h" // for LOG_INFO, SW_SITE, ST_RGR_VALUES
#include "include/SW_Defines.h"     // for MAX_LAYERS, MAX_ST_RGR, TimeInt
#include "include/SW_SoilWater.h"   // for SW_SWRC_SWCtoSWP
#include <math.h>                   // for fabs, exp, log10, copysign


/* =================================================== */
/*                  Local Variables                    */
/* --------------------------------------------------- */

// based on Eitzinger, J., W. J. Parton, and M. Hartman. 2000. Improvement and
// Validation of A Daily Soil Temperature Submodel for Freezing/Thawing Periods.
// Soil Science 165:525-534.

// correction factor for eq. 3 [unitless]; estimate based on data from CPER/SGS
// LTER -- * Currently not used*
// const double TCORRECTION = 0.02;

// Eitzinger et al. (2000): fusion energy of water; units = [cal cm-3] --
// Currently not used
// const double FUSIONHEAT_H2O = 80.;

/** freezing point of water in soil [C]; based on Parton 1984 */
const double FREEZING_TEMP_C = -1.;

// based on Parton, W. J., M. Hartman, D. Ojima, and D. Schimel. 1998. DAYCENT
// and its land surface submodel: description and testing. Global and Planetary
// Change 19:35-48.
const double MIN_VWC_TO_FREEZE = 0.13;

/* =================================================== */
/*             Global Function Definitions             */
/* --------------------------------------------------- */

/**********************************************************************
HISTORY:
4/30/92  (SLC)
7/1/92   (SLC) Reset pptleft to 0 if less than 0 (due to round off)
1/19/93  (SLC) Check if vegcov is zero (in case there was no biomass),
then no standing crop interception is possible.
15-Oct-03 (cwb) replaced Parton's original equations with new ones
developed by John Bradford based on Corbet and Crouse 1968.
Replaced the following code:
par1 = LE(vegcov, 8.5) ?  0.9 + 0.04 * vegcov
: 1.24 + (vegcov-8.5) *.35;
par2 = LE(vegcov, 3.0) ? vegcov * .33333333
: 1. + (vegcov-3.)*0.182;
*wintstcr = par1 * .026 * ppt + 0.094 * par2;

21-Oct-03 (cwb) added MAX_WINTLIT line
**********************************************************************/

/**
@brief Calculate rain interception by vegetation canopies.

Interception equations as of v4.3.0 (21 Nov 2018) are based on results by
Vegas Galdos et al. 2012 \cite VegasGaldos2012 and
Gerrits 2010 \cite Gerrits2010.

Previously, equations were derived from Bradford 2004 \cite Bradford2004
based on data from Corbet and Crouse (1968) \cite Corbett1968; these were
implemented on 15 Oct 2003. The original formulation by
Parton 1978 \cite Parton1978 was also based on
Corbet and Crouse (1968) \cite Corbett1968.

@param[in,out] *ppt_incident Amount of rain (cm) arriving at canopy.
@param[out] *int_veg Amount of rain intercepted by vegetation canopy (cm).
@param[in,out] *s_veg Current canopy storage of intercepted water (cm).
@param m Number of rain events per day.
@param kSmax Parameter (mm) to determine storage capacity based on LAI.
@param LAI Leaf-area index (m2 / m2).
@param scale The compound fraction of vegetation type coverage and canopy
extent above snow pack.

\sideeffect
- ppt_incident Amount of rain not intercepted, i.e., throughfall (cm).
- s_veg Updated canopy storage (cm).
*/
void veg_intercepted_water(
    double *ppt_incident,
    double *int_veg,
    double *s_veg,
    double m,
    double kSmax,
    double LAI,
    double scale
) {
    if (GT(LAI, 0.) && GT(*ppt_incident, 0.)) {
        double Dthreshold_cm = m * kSmax * log10(1 + LAI) / 10.;

        *int_veg =
            scale *
            fmin(*ppt_incident, fmax(0., Dthreshold_cm - *s_veg / scale));
        *s_veg += *int_veg;
        *ppt_incident -= *int_veg;

    } else {
        *int_veg = 0.;
    }
}

/**
@brief Calculate rain interception by litter layer.

Interception equations as of v4.3.0 (21 Nov 2018) are based on results by
Vegas Galdos et al. 2012 \cite VegasGaldos2012 and
Gerrits 2010 \cite Gerrits2010.

Previously, equations were derived from Bradford 2004 \cite Bradford2004
based on data from Corbet and Crouse (1968) \cite Corbett1968; these were
implemented on 15 Oct 2003. The original formulation by
Parton 1978 \cite Parton1978 was also based on
Corbet and Crouse (1968) \cite Corbett1968.

@param[in,out] *ppt_through Amount of rain (cm) arriving at litter layer.
@param[out] *int_lit Amount of rain intercepted by litter;
    added to previous value (cm).
@param[in,out] *s_lit Current litter storage of intercepted water (cm).
@param m Number of rain events per day.
@param kSmax Parameter (mm) to determine storage capacity based on litter
    biomass.
@param blitter Litter biomass density (g / m2).
@param scale The compound fraction of vegetation type coverage and litter
above snow pack.

\sideeffect
- ppt_through Amount of rain not intercepted, i.e., throughfall (cm).
- s_lit Updated litter storage (cm).
*/
void litter_intercepted_water(
    double *ppt_through,
    double *int_lit,
    double *s_lit,
    double m,
    double kSmax,
    double blitter,
    double scale
) {
    if (GT(blitter, 0.) && GT(*ppt_through, 0.)) {
        double intercepted = 0.0;
        double Dthreshold_cm = m * kSmax * log10(1 + blitter) / 10.;

        intercepted =
            scale *
            fmin(*ppt_through, fmax(0., Dthreshold_cm - *s_lit / scale));

        *int_lit += intercepted;
        *s_lit += intercepted;
        *ppt_through -= intercepted;
    }
}

/**
@brief Infiltrate water into soil layers under high water conditions, otherwise
      known as saturated percolation.

@param swc Soilwater content in each layer before drainage (cm H<SUB>2</SUB>O).
@param drain Drainage amount in each layer (cm/day).
@param *drainout Drainage from the last layer (cm H<SUB>2</SUB>O).
@param pptleft Daily precipitation available to the soil (cm/day).
@param nlyrs Number of layers available to drain from.
@param swcfc Soilwater content in each layer at field capacity
(cm H<SUB>2</SUB>O).
@param swcsat Soilwater content in each layer at saturation
    (m<SUP>3</SUP>H<SUB>2</SUB>O).
@param impermeability Impermeability measures for each layer.
@param *standingWater Remaining water on the surface
    (m<SUP>3</SUP>H<SUB>2</SUB>O).
@param lyrFrozen Frozen information at each layer.

@sideeffect
- drain Updated drainage amount in each layer (cm/day).
- swc Updated soilwater content in each layer after drainage (cm H<SUB>2</SUB>O)
- *standingWater Updated remaining water on the surface
  (m<SUP>3</SUP>H<SUB>2</SUB>O).
- *drainout Updated drainage from the previous layer (cm H<SUB>2</SUB>O)
*/
/**********************************************************************
 HISTORY:
 4/30/92  (SLC)
 1/14/02 - (cwb) fixed off by one error in loop.
 10/20/03 - (cwb) added drainout variable to return drainage
 out of lowest layer
 **********************************************************************/

void infiltrate_water_high(
    double swc[],
    double drain[],
    double *drainout,
    double pptleft,
    unsigned int nlyrs,
    const double swcfc[],
    double swcsat[],
    const double impermeability[],
    double *standingWater,
    double lyrFrozen[]
) {

    unsigned int i;
    unsigned int j;
    double d[MAX_LAYERS] = {0};
    double push;
    double ksat_rel;

    // Infiltration
    swc[0] += pptleft + *standingWater;
    (*standingWater) = 0.;

    // Saturated percolation
    for (i = 0; i < nlyrs; i++) {
        if (!ZRO(lyrFrozen[i])) {
            ksat_rel = 0.01; // roughly estimated from Parton et al. 1998 GCB
        } else {
            ksat_rel = 1.;
        }

        /* calculate potential saturated percolation */
        d[i] =
            fmax(0., ksat_rel * (1. - impermeability[i]) * (swc[i] - swcfc[i]));
        drain[i] = d[i];

        if (i < nlyrs - 1) {
            /* percolate up to next-to-last layer */
            swc[i + 1] += d[i];
            swc[i] -= d[i];
        } else {
            /* percolate last layer */
            (*drainout) = d[i];
            swc[i] -= (*drainout);
        }
    }

    /* adjust (i.e., push water upwards) if water content of a layer is now
     * above saturated water content */

    /* reverse for-loop with j in [nlyrs - 1, 0] */
    for (j = nlyrs; j-- > 0;) {
        if (GT(swc[j], swcsat[j])) {
            push = swc[j] - swcsat[j];
            swc[j] -= push;
            if (j > 0) {
                drain[j - 1] -= push;
                swc[j - 1] += push;
            } else {
                (*standingWater) = push;
            }
        }
    }
}

/**
@brief Compute weighted average of soilwater potential to be
used for transpiration calculations.

@param[in,out] *swp_avg Weighted average of soilwater potential and
    transpiration coefficients (-bar).
@param[in] SW_Site Struct of type SW_SITE describing the simulated site
@param[in] n_tr_rgns Array of n_lyrs elements of transpiration regions that each
    soil layer belongs to.
@param[in] n_layers Number of soil layers.
@param[in] tr_regions Number of layer regions used in weighted average,
    typically 3;<BR> to represent shallow, mid, & deep depths to compute
    transpiration rate.
@param[in] swc Soilwater content in each layer before drainage
    (m<SUP>3</SUP>H<SUB>2</SUB>O).
@param[in] VegType Current vegetation
@param[out] LogInfo Holds information on warnings and errors

@sideeffect *swp_avg Weighted average of soilwater potential and transpiration
coefficients (-bar).
*/
void transp_weighted_avg(
    double *swp_avg,
    SW_SITE *SW_Site,
    unsigned int n_tr_rgns,
    LyrIndex n_layers,
    const unsigned int tr_regions[],
    double swc[],
    int VegType,
    LOG_INFO *LogInfo
) {

    /**********************************************************************

     HISTORY:
     Original:
     4/30/92  SLC
     6/9/93   (SLC) check that (sum_tr_co <> 0) before dividing swp by this
     number
     4/10/2000 CWB -- began recoding in C
     9/21/01   cwb -- adjusted method for determining transpiration
     regions to reflect the new design.  removed
     tr_reg_min and max, added n_layers and tr_regions[].
     4-Mar-02  cwb -- moved this function after ppt enters soil.  Originally,
     it was the first function called, so evapotransp was
     based on swp_avg prior to wetting.  Also, set return
     value as a pointer argument to be more consistent with
     the rest of the code.
     1-Oct-03  cwb -- Removed sum_tr_coeff[] requirement as it might as well
     be calculated here and save the confusion of
     having to keep up with it in the rest of the code.
     1-Oct-03 - local sumco replaces previous sum_tr_coeff[]

     **********************************************************************/
    unsigned int r;
    unsigned int i;
    double swp;
    double sumco;

    *swp_avg = 0;
    for (r = 1; r <= n_tr_rgns; r++) {
        swp = sumco = 0.0;

        for (i = 0; i < n_layers; i++) {
            if (tr_regions[i] == r) {
                swp += SW_Site->transp_coeff[VegType][i] *
                       SW_SWRC_SWCtoSWP(swc[i], SW_Site, i, LogInfo);

                if (LogInfo->stopRun) {
                    return; // Exit function prematurely due to error
                }

                sumco += SW_Site->transp_coeff[VegType][i];
            }
        }

        swp /= GT(sumco, 0.) ? sumco : 1.;

        /* use smallest weighted average of regions */
        (*swp_avg) = (r == 1) ? swp : fmin(swp, (*swp_avg));
    }
}

/**
@brief Calculates the fraction of water lost from bare soil evaporation and
transpiration.

@author SLC

@param *fbse Fraction of water loss from bare soil evaporation.
@param *fbst Fraction of water loss from bare soil transpiration.
@param blivelai Live biomass leaf area index.
@param lai_param Leaf area index parameter.

@sideeffect
- fbse Updated fraction of water loss from bare soil evaporation.
- fbst Updated fraction of water loss from bare soil transpiration.
*/
void EsT_partitioning(
    double *fbse, double *fbst, double blivelai, double lai_param
) {
    /**********************************************************************
     PURPOSE: Calculate fraction of water loss from bare soil
     evaporation and transpiration

     HISTORY:
     4/30/92  (SLC)
     24-Oct-03 (cwb) changed exp(-blivelai*bsepar1) + bsepar2;
     to exp(-blivelai);
     08/22/2011	(drs)	For trees: according to a regression based on a review
by Daikoku, K., S. Hattori, A. Deguchi, Y. Aoki, M. Miyashita, K. Matsumoto, J.
Akiyama, S. Iida, T. Toba, Y. Fujita, and T. Ohta. 2008. Influence of
evaporation from the forest floor on evapotranspiration from the dry canopy.
Hydrological Processes 22:4083-4096. CWB- 4/00 Not sure what's the purpose of
bsepar2, unless it's a fudge-factor to be played with.
     **********************************************************************/
    double bsemax = 0.995;

    *fbse = exp(-lai_param * blivelai);

    *fbse = fmin(*fbse, bsemax);
    *fbst = 1. - (*fbse);
}

/**

@brief Calculates potential bare soil evaporation rate.

Based on equations from Parton 1978. @cite Parton1978

@param[in] SW_Site Struct of type SW_SITE describing the simulated site
@param[in] nelyrs Number of layers to consider in evaporation.
@param[in] totagb Sum of above ground biomass and litter.
@param[in] fbse Fraction of water loss from bare soil evaporation.
@param[in] petday Potential evapotranspiration rate (cm/day).
@param[in] shift Displacement of the inflection point in order to shift the
    function up, down, left, or right; in relation to the macro tanfunc.
@param[in] shape Slope of the line at the inflection point.
@param[in] inflec Y-value of the inflection point.
@param[in] range Max y-value - min y-value at the limits.
@param[in] swc Soilwater content in each layer before drainage
    (m<SUP>3</SUP>H<SUB>2</SUB>O).
@param[in] Es_param_limit Parameter to determine when soil surface is completely
    covered with litter and that bare soil evaporation is inhibited.
@param[out] *bserate Bare soil evaporation loss rate (cm/day).
@param[out] LogInfo Holds information on warnings and errors

@sideeffect *bserate Updated bare soil evaporation loss rate (cm/day).
*/
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
) {
    /**********************************************************************
     HISTORY:
     4/30/92  (SLC)
     8/27/92  (SLC) Put in a check so that bserate cannot become
     negative.  If total aboveground biomass (i.e.
     litter+bimoass) is > 999., bserate=0.
     6 Mar 02 (cwb) renamed watrate's parameters (see also SW_Site.h)
     shift,  shift the x-value of the inflection point
     shape,  slope of the line at the inflection point
     inflec, y-value of the inflection point
     range;  max y-val - min y-val at the limits
     1-Oct-03 - cwb - removed the sumecoeff variable as it should
     always be 1.0.  Also removed the line
     avswp = sumswp / sumecoeff;

     LOCAL:
     avswp     - average soil water potential over all layers
     evpar1    - input parameter to watrate.

     FUNCTION CALLS:
     watrate   - calculate evaporation rate.
     swpotentl - compute soilwater potential
     **********************************************************************/

    double x;
    double avswp = 0.0;
    double sumwidth = 0.0;
    unsigned int i;

    /* get the weighted average of swp in the evap layers */
    for (i = 0; i < nelyrs; i++) {
        if (ZRO(SW_Site->evap_coeff[i])) {
            break;
        }
        x = SW_Site->width[i] * SW_Site->evap_coeff[i];
        sumwidth += x;
        avswp += x * SW_SWRC_SWCtoSWP(swc[i], SW_Site, i, LogInfo);
        if (LogInfo->stopRun) {
            return; // Exit function prematurely due to error
        }
    }

    // Note: avswp = 0 if swc = 0 because that is the return value of
    // SW_SWRC_SWCtoSWP
    avswp /= (ZRO(sumwidth)) ? 1 : sumwidth;

    /*  8/27/92 (SLC) if totagb > Es_param_limit, assume soil surface is
     * completely covered with litter and that bare soil
     * evaporation is inhibited.
     */

    if (GE(totagb, Es_param_limit) || ZRO(avswp)) {
        *bserate = 0.;
    } else {
        *bserate = petday *
                   watrate(avswp, petday, shift, shape, inflec, range) *
                   (1. - (totagb / Es_param_limit)) * fbse;
    }
}

/**
@brief Calculate the potential bare soil evaporation rate of bare ground.

Based on equations from Parton 1978. @cite Parton1978

@param[in,out] *bserate Bare soil evaporation loss rate (cm/day).
@param[in] SW_Site Struct of type SW_SITE describing the simulated site
@param[in] nelyrs Number of layers to consider in evaporation.
@param[in] petday Potential evapotranspiration rate (cm/day).
@param[in] shift Displacement of the inflection point in order to shift the
    function up, down, left, or right.
@param[in] shape Slope of the line at the inflection point.
@param[in] inflec Y-value of the inflection point.
@param[in] range Max y-value - min y-value at the limits.
@param[in] swc Soilwater content in each layer before drainage
    (m<SUP>3</SUP>H<SUB>2</SUB>O).
@param[out] LogInfo Holds information on warnings and errors
*/
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
) {

    /**********************************************************************
     LOCAL:
     avswp     - average soil water potential over all layers
     evpar1    - input parameter to watrate.

     FUNCTION CALLS:
     watrate   - calculate evaporation rate.
     swpotentl - compute soilwater potential
     **********************************************************************/

    double x;
    double avswp = 0.0;
    double sumwidth = 0.0;
    unsigned int i;

    /* get the weighted average of swp in the evap layers */
    for (i = 0; i < nelyrs; i++) {
        x = SW_Site->width[i] * SW_Site->evap_coeff[i];
        sumwidth += x;
        avswp += x * SW_SWRC_SWCtoSWP(swc[i], SW_Site, i, LogInfo);
        if (LogInfo->stopRun) {
            return; // Exit function prematurely due to error
        }
    }

    avswp /= sumwidth;

    *bserate = petday * watrate(avswp, petday, shift, shape, inflec, range);
}

/**

@brief Calculates the potential transpiration rate.

Based on equations from Parton 1978. @cite Parton1978

@param *bstrate This is the bare soil evaporation loss rate (cm/day).
@param swpavg Weighted average of soil water potential (-bar).
@param biolive Living biomass (%).
@param biodead Dead biomass (%).
@param fbst Fraction of water loss from transpiration.
@param petday Potential evapotranspiration rate (cm/day).
@param swp_shift Shifts the x-value of the inflection point.
@param swp_shape Slope of the line at the inflection point.
@param swp_inflec Y-value of the inflection point.
@param swp_range Max y-value - min y-value at the limits.
@param shade_scale Scale for shade effect.
@param shade_deadmax Maximum biomass of dead, before shade has any affect.
@param shade_xinflex X-value of the inflection point, before shade has any
    effect.
@param shade_slope Slope of the line at the inflection point, before shade has
    any effect.
@param shade_yinflex Y-value of the inflection point, before shade has any
    effect.
@param shade_range Max y-value - min y-value at the limits, before shade has any
    effect.
@param co2_wue_multiplier Water-usage efficiency multiplier (CO<SUB>2</SUB>
ppm).

@sideeffect *bstrate Updated bare soil evaporation loss rate (cm/day).
*/
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
) {
    /**********************************************************************

     HISTORY:
     4/30/92  (SLC)
     7/1/92   (SLC) Fixed bug.  Equation for bstrate not right.
     9/1/92   (SLC) Allow transpiration, even if biodead is zero.  This
     was in the original model, we will compute shade if biodead
     is greater than deadmax.  Otherwise, shadeaf = 1.0
     6 Mar 02 (cwb) renamed watrate's parameters (see also SW_Site.h)
     shift,  shift the x-value of the inflection point
     shape,  slope of the line at the inflection point
     inflec, y-value of the inflection point
     range;  max y-val - min y-val at the limits

     LOCAL VARIABLES:
     shadeaf - shade affect on transpiration rate
     scale1  - scale for shade affect
     trpar1  - input paramter to watrate
     deadmax - maximum biomass of dead, before shade has any affect.

     FUNCTION CALLS:
     watrate - compute transpiration rate.
     tanfunc - tangent function
     **********************************************************************/

    double par1;
    double par2;
    double shadeaf;

    if (LE(biolive, 0.)) {
        *bstrate = 0.;

    } else {
        if (GE(biodead, shade_deadmax)) {
            par1 = tanfunc(
                biolive, shade_xinflex, shade_yinflex, shade_range, shade_slope
            );
            par2 = tanfunc(
                biodead, shade_xinflex, shade_yinflex, shade_range, shade_slope
            );
            shadeaf = (par1 / par2) * (1.0 - shade_scale) + shade_scale;
            shadeaf = fmin(shadeaf, 1.0);
        } else {
            shadeaf = 1.0;
        }

        *bstrate =
            watrate(
                swpavg, petday, swp_shift, swp_shape, swp_inflec, swp_range
            ) *
            shadeaf * petday * fbst * co2_wue_multiplier;
    }
}

/**
@brief Calculate the evaporation or transpiration rate as a function of
potential evapotranspiration and soil water potential.

Based on equations from Parton 1978. @cite Parton1978

@param swp Soil water potential (-bar).
@param petday Potential evapotranspiration rate (cm/day).
@param shift Displacement of the inflection point in order to shift the function
    up, down, left, or right.
@param shape Slope of the line at the inflection point.
@param inflec Y-value of the inflection point.
@param range Max y-value - min y-value at the limits.

@return watrate Rate of evaporation (or transpiration) from the soil (cm/day).
*/
double watrate(
    double swp,
    double petday,
    double shift,
    double shape,
    double inflec,
    double range
) {
    /**********************************************************************
     PURPOSE: Calculate the evaporation (or transpiration) rate, as
     a function of potential evapotranspiration and soil
     water potential. The ratio of evaporation (transpiration)
     rate to PET is inversely proportional to soil water
     poential (see Fig2.5a,b, pp.39, "Abiotic Section of ELM")

     HISTORY:
     4/30/92  (SLC)
     6 Mar 02 (cwb) - Changed arguments from a/b/c to shift,shape,
     inflec, range because I finally found the source
     for tanfunc.

     **********************************************************************/

    double par1;
    double par2;
    double result;

    if (LT(petday, .2)) {
        par1 = 3.0;
    } else if (LT(petday, .4)) {
        par1 = (.4 - petday) * -10. + 5.;
    } else if (LT(petday, .6)) {
        par1 = (.6 - petday) * -15. + 8.;
    } else {
        par1 = 8.;
    }

    par2 = shift - swp;

    result = tanfunc(par2, par1, inflec, range, shape);

    return (fmin(fmax(result, 0.0), 1.0));
}

/**
@brief Evaporates water from the surface water pool, i.e., intercepted water
(tree, shrub, grass, litter) or standingWater.

Note: call sperately for each pool.

@param *water_pool Pool of surface water minus evaporated water (cm/day).
@param *evap_rate Actual evaporation from this pool (cm/day).
@param *aet Actual evapotranspiration (cm/day).

@sideeffect
- *water_pool Updated pool of surface water minus evaporated water (cm/day).
- *evap_rate Updated evaporation from this pool (cm/day).
- *aet Updated evapotranspiration (cm/day).
*/

void evap_fromSurface(double *water_pool, double *evap_rate, double *aet) {

    if (GT(*water_pool, *evap_rate)) {
        /* potential rate is smaller than available water
           -> entire potential is evaporated */
        *water_pool -= *evap_rate;
        *aet += *evap_rate;
    } else {
        /* potential rate is larger than available water
           -> entire pool is evaporated */
        *evap_rate = *water_pool;
        *aet += *water_pool;
        *water_pool = 0.;
    }
}

/**
@brief Removes water from the soil

Replaces earlier versions' call to separate functions for evaporations and
transpiration and instead combines them into one function,
see Eqns. 2.12 - 2.18 in "Abiotic Section of ELM".

Based on equations from Parton 1978. @cite Parton1978

@param[in,out] swc Soilwater content in each layer before drainage
    (m<SUP>3</SUP> H<SUB>2</SUB>O).
@param[in,out] qty Updated removal quantity from each layer, evaporation or
    transpiration, added to input value (mm/day).
@param[in] SW_Site Struct of type SW_SITE describing the simulated site
@param[in,out] *aet Actual evapotranspiration, added to input value (cm/day).
@param[in] nlyrs Number of layers considered in water removal.
@param[in] coeff Coefficients of removal for removal layers.
@param[in] rate Removal rate, either soil_evap_rate or soil_transp_rate.
@param[in] swcmin Lower limit on soilwater content per layer.
@param[in] lyrFrozen Frozen information at each layer.
@param[out] LogInfo Holds information on warnings and errors

@note The variable `SW_Site` is only used as input for another function.
It will not affect other elements passed in from it.

@sideeffect
- swc Updated soil water content adjusted after evaporation.
- qty Updated removal quantity from each layer, evap or transp.
- *aet Updated evapotranspiration (cm/day).
*/
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
) {

    /**********************************************************************
HISTORY: 10 Jan 2002 - cwb - replaced two previous functions with
     this one.
     12 Jan 2002 - cwb - added aet arg.
     4 Dec 2002  - cwb - Adding STEPWAT code uncovered possible
     div/0 error. If no transp coeffs, return.

     FUNCTION CALLS:
     swpotentl - compute soilwater potential of the layer.
     **********************************************************************/

    unsigned int i;
    double swpfrac[MAX_LAYERS];
    double sumswp = 0.0;
    double swc_avail;
    double q;
    double tmpswp;
    double d = 0.;

    for (i = 0; i < nlyrs; i++) {
        tmpswp = SW_SWRC_SWCtoSWP(swc[i], SW_Site, i, LogInfo);
        if (LogInfo->stopRun) {
            return; // Exit function prematurely due to error
        }

        if (GT(tmpswp, 0.)) {
            swpfrac[i] = coeff[i] / tmpswp;
        } else {
            // saturated conditions -> divide by SWP [-bar] at field capacity
            swpfrac[i] = coeff[i] / 0.333;
        }
        sumswp += swpfrac[i];
    }

    if (ZRO(sumswp)) {
        return;
    }

    for (i = 0; i < nlyrs; i++) {
        if (!ZRO(lyrFrozen[i])) {
            // no water extraction, i.e., evaporation and transpiration, from a
            // frozen soil layer
            d = 0.;

        } else {
            q = (swpfrac[i] / sumswp) * rate;
            swc_avail = fmax(0., swc[i] - swcmin[i]);
            d = fmin(q, swc_avail);
            swc[i] -= d;
            *aet += d;
        }

        qty[i] += d;
    }
}

/**
@brief Calculate unsaturated percolation

Based on equation 2.9 by Parton 1978. @cite Parton1978

Here, we modified eq. 2.9:
    1. use scaled `swc`, i.e., vary from 0 (at `swc_min`) to
       `swc_fc` (instead from `swc_min` to `swc_fc`)

    2. modify `exp(.)` to vary
       from 0 (at `swc_min`) to 1 (at `swc_fc`)
       (instead from `exp(-15 * fc / width)` to 1)

This function was previously named `infiltrate_water_low()`.

@param[in,out] swc Soil water content in each layer [cm].
@param[in,out] percolate Drainage from each layer [cm / day].
@param[in,out] *drainout Drainage from the last layer [cm / day].
@param[in,out] *standingWater Remaining water on the surface [cm].
@param nlyrs Number of layers in the soil profile [1].
@param[in] *lyrFrozen Frozen/unfrozen status for each soil layer.
@param[in] SW_Site Struct of type SW_SITE describing the simulated site
@param slow_drain_coeff Slow drainage parameter [cm / day].
@param slow_drain_depth Slow drainage depth [cm].
*/
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
) {

    unsigned int i;
    unsigned int j;
    double drainlw = 0.0;
    double swc_avail;
    double drainpot;
    double d[MAX_LAYERS] = {0};
    double push;
    double kunsat_rel;
    double swcrel;
    double tmp1;
    double tmp2;


    /* Note that this function calculates percolation as if no gravel
                    (other than reduction of soil moisture via factor `1 - Rv`)
                    --> TODO: consider adjusting percolation for gravel effects
    */
    for (i = 0; i < nlyrs; i++) {
        /* calculate potential unsaturated percolation */
        swc_avail = fmax(0., swc[i] - SW_Site->swcBulk_min[i]);

        if (LE(swc_avail, 0.)) {
            d[i] = 0.;

        } else {

            // potential unsaturated percolation rate
            drainpot = slow_drain_coeff;

            /* Scale pot. unsat. percolation rate by frozen soil
                    roughly estimated from Parton et al. 1998 GCB
            */
            kunsat_rel = !ZRO(lyrFrozen[i]) ? 0.01 : 1.;

            /* Scale pot. unsat. percolation rate by soil moisture
                            Using Parton 1978, eq. 2.9:
                                    `slow drainage = 0.02 * exp(15. * (SWC -
               FieldCapacity) / width)`

                            which is reportedly based on Black et al. 1969, eq.
               14/17: `drainage rate = 0.35 * exp(0.7 * (storage - 15.))`
            */

            if (LT(swc[i], SW_Site->swcBulk_fieldcap[i])) {
                /* Here, we modified eq. 2.9:
                                (i) use scaled swc, i.e., vary from 0 (at
                   swc_min) to swc_fc (ii) modify `exp(.)` to vary from 0 (at
                   swc_min) to 1 (at swc_fc) (instead from `exp(-15*fc/width)`
                   to 1)
                */
                swcrel = fmax(
                    0.,
                    fmin(
                        1.,
                        swc_avail / (SW_Site->swcBulk_fieldcap[i] -
                                     SW_Site->swcBulk_min[i])
                    )
                );

                tmp1 = slow_drain_depth * SW_Site->swcBulk_fieldcap[i] /
                       SW_Site->width[i];
                tmp2 = exp(-tmp1);

                if (LT(tmp2, 1.)) {
                    drainpot *=
                        (exp(tmp1 * (swcrel - 1.)) - tmp2) / (1. - tmp2);
                } else {
                    drainpot = 0.;
                }
            }

            d[i] = kunsat_rel * (1. - SW_Site->impermeability[i]) *
                   fmin(swc_avail, fmax(0., drainpot));
        }

        percolate[i] += d[i];

        if (i < nlyrs - 1) {
            /* percolate up to next-to-last layer */
            swc[i + 1] += d[i];
            swc[i] -= d[i];
        } else {
            /* percolate last layer */
            drainlw = fmax(d[i], 0.0);
            (*drainout) += drainlw;
            swc[i] -= drainlw;
        }
    }


    /* adjust (i.e., push water upwards) if water content of a layer is
       now above saturated water content */

    /* reverse for-loop with j in [nlyrs - 1, 0] */
    for (j = nlyrs; j-- > 0;) {
        if (GT(swc[j], SW_Site->swcBulk_saturated[j])) {
            push = swc[j] - SW_Site->swcBulk_saturated[j];
            swc[j] -= push;
            if (j > 0) {
                percolate[j - 1] -= push;
                swc[j - 1] += push;
            } else {
                (*standingWater) += push;
            }
        }
    }
}

/**
@brief Calculate hydraulic redistribution.

Based on equations from Ryel 2002. @cite Ryel2002

@param[in,out] swc Soil water content in each layer
    (m<SUP>3</SUP>H<SUB>2</SUB>O).
@param[out] hydred Hydraulic redistribtion for each soil layer
    (cm/day/layer).
@param[in] SW_Site Struct of type SW_SITE describing the simulated site
@param[in] vegk Index to vegetation type (used to access rooting
    profile) [1].
@param[in] nlyrs Number of layers in the soil profile [1].
@param[in] *lyrFrozen Frozen/unfrozen status for each soil layer.
@param[in] maxCondroot Maximum radial soil-root conductance of the
    entire active root system for water (cm/-bar/day).
@param[in] swp50 Soil water potential (-bar) where conductance is
    reduced by 50%.
@param[in] shapeCond Shaping parameter for the empirical relationship
    from van Genuchten to model relative soil-root conductance for water.
@param[in] scale Fraction of vegetation type to scale hydred.
@param[in] year Current year in simulation
@param[in] doy  Day of the year (base1) [1-366]
@param[out] LogInfo Holds information on warnings and errors

@sideeffect
- swc Updated soilwater content in each layer after drainage
  (m<SUP>3</SUP> H<SUB>2</SUB>O).
- hydred Hydraulic redistribtion for each soil water layer (cm/day/layer).
*/
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
) {
    /**********************************************************************
     HISTORY:
     10/19/2010 (drs)
     11/13/2010 (drs) limited water extraction for hydred to swp above wilting
     point 03/23/2012 (drs) excluded hydraulic redistribution from top soil
     layer (assuming that this layer is <= 5 cm deep)
     **********************************************************************/
#ifdef SWDEBUG
    short debug = 0;
    double hdnet2;
#endif

    unsigned int i;
    unsigned int j;
    unsigned int idso;
    unsigned int idre;
    unsigned int nit;
    Bool is_hd_adj;
    double swa[MAX_LAYERS] = {0};
    double swp[MAX_LAYERS] = {0};
    double relCondroot[MAX_LAYERS] = {0};
    double mlyrRootCo[2] = {0.};
    double hydredmat[MAX_LAYERS][MAX_LAYERS] = {{0}};
    double hdnet;
    double hdin;
    double hdout;
    double tmp;

#ifdef SWDEBUG
    if (debug) {
        sw_printf("hydred[%d-%d/veg(%d)]: \n", year, doy, vegk);
    }
#endif

    /* Pre-calculate variables */
    for (i = 0; i < nlyrs; i++) {
        /* Set water extraction limit to moisture above SWPcrit by vegetation
                (unless wilting point is lower)
        */
        swa[i] = fmax(
            0.,
            swc[i] - fmin(
                         SW_Site->swcBulk_wiltpt[i],
                         SW_Site->swcBulk_atSWPcrit[vegk][i]
                     )
        );

        swp[i] = SW_SWRC_SWCtoSWP(swc[i], SW_Site, i, LogInfo);
        if (LogInfo->stopRun) {
            return; // Exit function prematurely due to error
        }

        /* Ryel et al. 2002: eq. 7 relative soil-root conductance */
        relCondroot[i] =
            fmin(1., fmax(0., 1. / (1. + powe(swp[i] / swp50, shapeCond))));

        hydredmat[0][i] = hydredmat[i][0] = 0.; /* no hydred in top layer */
        hydredmat[i][i] = 0.; /* no hydred within same layer */
    }

    /* Calculate hydraulic redistribution according to Ryel et al. 2002:
            layer i receives moisture moved from layer j: `hydredmat[i][j] > 0`
            layer i loses moisture to layer j: `hydredmat[i][j] < 0`
            net moisture change in layer i: `sum across j of hydredmat[i][j]`
                    which equals `sum across j of -hydredmat[j][i]`
    */

    for (i = 1; i < nlyrs; i++) {

        for (j = i + 1; j < nlyrs; j++) {

            /* hydred only occurs if at least one soil layer is wet
                    (more moisture than at wilting point)
                    and both soil layers are not frozen
            */
            if ((GT(swc[i], SW_Site->swcBulk_wiltpt[i]) ||
                 GT(swc[j], SW_Site->swcBulk_wiltpt[j])) &&
                (lyrFrozen[i] == swFALSE) && (lyrFrozen[j] == swFALSE)) {

                /* Identify source layer: from which water is removed */
                idso = LT(swp[i], swp[j]) ? i : j;
                /* Identify recipient layer: to which water is moved */
                idre = (idso == i) ? j : i;


                /* Correct rooting contributions for different layer widths
                        -> truncate to source layer width
                        (original equation assumed identical layer widths)
                */
                mlyrRootCo[0] = SW_Site->transp_coeff[vegk][idso];
                mlyrRootCo[1] = SW_Site->transp_coeff[vegk][idre];

                if (LT(SW_Site->width[idso], SW_Site->width[idre])) {
                    mlyrRootCo[1] *=
                        SW_Site->width[idso] / SW_Site->width[idre];
                }


                /* Ryel et al. 2002: eq. 6 */
                /* Convert hourly to daily: assume a fixed day with a 10-hour
                 * night */
                tmp = 10. / 24. * maxCondroot * (swp[j] - swp[i]) *
                      fmax(relCondroot[i], relCondroot[j]) * mlyrRootCo[0] *
                      mlyrRootCo[1] / (1. - mlyrRootCo[0]);

                /* limit hydred to moisture above swc_min */
                tmp = copysign(fmin(fabs(tmp), swa[idso]), tmp);

#ifdef SWDEBUG
                if (debug) {
                    sw_printf(
                        "hd[sl=%d,%d|so=%d,re=%d]=%+.6f cm: "
                        "so|re: w=%4.1f|%4.1f cm, "
                        "swc=%.4f|%.4f cm, "
                        "swp=%7.3f|%7.3f MPa, "
                        "c=%.4f|%.4f, "
                        "R=%.4f|%.4f"
                        "\n",
                        i,
                        j,
                        idso,
                        idre,
                        tmp,
                        SW_Site->width[idso],
                        SW_Site->width[idre],
                        swc[idso],
                        swc[idre],
                        -0.1 * swp[idso],
                        -0.1 * swp[idre],
                        relCondroot[idso],
                        relCondroot[idre],
                        mlyrRootCo[0],
                        mlyrRootCo[1]
                    );
                }
#endif

                hydredmat[i][j] = tmp;
                hydredmat[j][i] = -tmp;

            } else {
                hydredmat[i][j] = hydredmat[j][i] = 0.;
            }
        }
    }


    /* Restrict net hydred so that it does not remove moisture below swc_min */
    nit = 0;
    is_hd_adj = swTRUE;

    while (nit < nlyrs && is_hd_adj) {
        /* Iterate because the restriction of outgoing hydred in one layer
           affects incoming hydred in other layers which, in turn, may affect
           their ability for outgoing hydred
        */
        nit++;
        is_hd_adj = swFALSE; /* init for current iteration */

        for (i = 0; i < nlyrs; i++) {

            if (GT(swa[i], 0.)) {
                /* Calculate net hydred for layer i */
                hdin = hdout = 0.;
                for (j = 0; j < nlyrs; j++) {
                    if (GT(hydredmat[i][j], 0.)) {
                        hdin += hydredmat[i][j];
                    } else {
                        hdout += hydredmat[i][j];
                    }
                }
                hdnet = hdin + hdout;

                if (LT(hdnet, 0.) && GT(-hdnet, swa[i])) {
                    /* net hydred would extract more moisture than available
                       above swc_min
                            -> reduce moisture extractions so that -hdnet <= swa
                    */
                    tmp = -(swa[i] + hdin) / hdout;
                    is_hd_adj = swTRUE;

                    for (j = 0; j < nlyrs; j++) {
                        if (LT(hydredmat[i][j], 0.)) {
                            hydredmat[i][j] *= tmp;
                            hydredmat[j][i] *= tmp;
                        }
                    }
                }

#ifdef SWDEBUG
                if (debug) {
                    hdnet2 = 0.;
                    for (j = 0; j < nlyrs; j++) {
                        hdnet2 += hydredmat[i][j];
                    }
                    if (!EQ(hdnet, hdnet2)) {
                        sw_printf(
                            "hd[sl=%d|it=%d]: pre=%+.6f (hdin=%.6f, "
                            "hdout=%.6f), "
                            "post=%+.6f, swa=%.6f, swc=%.4f\n",
                            i,
                            nit,
                            hdnet,
                            hdin,
                            hdout,
                            hdnet2,
                            swa[i],
                            swc[i]
                        );
                    }
                }
#endif
            }
        }
    }

    if (is_hd_adj) {
        LogError(
            LogInfo,
            LOGERROR,
            "[%d-%d/veg(%d)]: "
            "hydraulic redistribution failed to constrain to swc_min.",
            year,
            doy,
            vegk
        );
        return; // Exit function prematurely due to error
    }


    /* Determine finalized hydraulic redistribution */

    hydred[0] = 0.; /* no hydred in top layer */

    for (i = 1; i < nlyrs; i++) {
        hydred[i] = 0.;
        for (j = 1; j < nlyrs; j++) {
            hydred[i] += hydredmat[i][j];
        }

        hydred[i] *= scale; /* scale by fractional vegetation contribution */
        swc[i] += hydred[i];
    }
}

/**
@brief Interpolate soil temperature layer temperature values to
input soil profile depths/layers.

@param[in] cor Two dimensional array containing soil temperature data.
@param[in] nlyrTemp The number of soil temperature layers.
@param[in] depth_Temp Depths of soil temperature layers (cm).
@param[in] avgLyrTempR Temperature values of soil temperature layers (&deg;C).
@param[in] nlyrSoil Number of soil layers.
@param[in] depth_Soil Depths of soil layers (cm).
@param[in] width_Soil Witdths of soil layers (cm).
@param[out] avgLyrTemp Temperature values of soil layers (&deg;C).
@param[in] temperatureRangeR Temperature ranges of each layers based on
    avgLyrTempR
@param[out] temperatureRange Temperature range values at each soil layer
*/
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
) {
#ifdef SWDEBUG
    int debug = 0;
#endif

    // i: index to soil temperature layer (i = 0 is surface)
    // j: index to soil layer (j = 0 is first soil layer)
    unsigned int i = 0;
    unsigned int j;
    unsigned int n;
    double acc;

    // interpolate soil temperature values for depth of soil profile layers
    for (j = 0; j < nlyrSoil; j++) {
        avgLyrTemp[j] = 0.0;
        temperatureRange[j] = 0.0;
        acc = 0.0;
        n = 0;
        while (LT(acc, width_Soil[j]) && i <= nlyrTemp + 1) {
            if (EQ(cor[i][j], 0.0)) {
                // zero cor values indicate next soil temperature layer
                i++;
            }

            if (GT(cor[i][j], 0.0)) {
                // there are soil layers to add; index i = 0 is soil surface
                if (!(i == 0 && LT(acc + cor[i][j], width_Soil[j]))) {
                    // don't use soil surface temperature if there is other
                    // sufficient soil temperature to interpolate
                    avgLyrTemp[j] += interpolation(
                        // depth of `avgLyrTempR[i]`:
                        (i > 0) ? depth_Temp[i - 1] : 0.0,
                        // depth of `avgLyrTempR[i + 1]`:
                        depth_Temp[i],
                        avgLyrTempR[i],
                        avgLyrTempR[i + 1],
                        depth_Soil[j]
                    );

                    // TODO: linear interpolation is not the most appropriate
                    // here because it leads easily to negative values which are
                    // impossible for a range
                    temperatureRange[j] += fmax(
                        0.,
                        interpolation(
                            (i > 0) ? depth_Temp[i - 1] : 0.0,
                            depth_Temp[i],
                            temperatureRangeR[i],
                            temperatureRangeR[i + 1],
                            depth_Soil[j]
                        )
                    );

                    n++; // add weighting by layer width
                }
                acc += cor[i][j];
                if (LT(acc, width_Soil[j])) {
                    i++;
                }

            } else if (LT(cor[i][j], 0.0)) {
                // negative cor values indicate copying values
                // from deepest soil layer
                break;
            }
        }

        if (n > 0) {
            avgLyrTemp[j] = avgLyrTemp[j] / n;
            temperatureRange[j] = temperatureRange[j] / n;
        }

#ifdef SWDEBUG
        if (debug) {
            sw_printf(
                "\nConf T : i=%i, j=%i, n=%i, avgLyrTemp[%i]=%2.2f, acc=%2.2f",
                i,
                j,
                n,
                j,
                avgLyrTemp[j],
                acc
            );
        }
#endif
    }
}

/**
@brief Interpolate soil layer temperature values to soil temperature profile
depths/layers.

@param[in] nlyrSoil Number of soil layers.
@param[in] depth_Soil Depths of soil layers (cm).
@param[in] avgLyrTemp Temperature values of soil layers (&deg;C).
@param[in] endTemp Final input for avgLyrTemp variables
@param[in] nlyrTemp Number of soil temperature layers.
@param[in] depth_Temp Depths of soil temperature layers (cm).
@param[in] maxTempDepth Maximum depth temperature (&deg;C).
@param[out] avgLyrTempR Array of today's (regression)-layer soil temperature
    values.
*/
void lyrSoil_to_lyrTemp_temperature(
    unsigned int nlyrSoil,
    const double depth_Soil[],
    const double avgLyrTemp[],
    double endTemp,
    unsigned int nlyrTemp,
    double depth_Temp[],
    double maxTempDepth,
    double avgLyrTempR[]
) {
#ifdef SWDEBUG
    int debug = 0;
#endif

    unsigned int i;
    unsigned int j1 = 0;
    unsigned int j2;
    double depth_Soil2[MAX_LAYERS + 1] = {0};
    double avgLyrTemp2[MAX_LAYERS + 1] = {0};

    // transfer data to include bottom conditions; do not include surface
    // temperature in interpolations
    for (i = 0; i < nlyrSoil; i++) {
        depth_Soil2[i] = depth_Soil[i];
        avgLyrTemp2[i] = avgLyrTemp[i];
    }
    depth_Soil2[nlyrSoil] = maxTempDepth;
    avgLyrTemp2[nlyrSoil] = endTemp;

    // interpolate soil temperature at soil temperature profile depths
    for (i = 0; i < nlyrTemp; i++) {
        while ((j1 + 1) < nlyrSoil && LT(depth_Soil2[j1 + 1], depth_Temp[i])) {
            j1++;
        }

        j2 = j1 + 1;
        while ((j2 + 1) < nlyrSoil + 1 && LE(depth_Soil2[j2 + 1], depth_Temp[i])
        ) {
            j2++;
        }

        avgLyrTempR[i + 1] = interpolation(
            depth_Soil2[j1],
            depth_Soil2[j2],
            avgLyrTemp2[j1],
            avgLyrTemp2[j2],
            depth_Temp[i]
        );

#ifdef SWDEBUG
        if (debug) {

            sw_printf(
                "\nConf T: i=%i, j1=%i, j2=%i, avgLyrTempR[%i]=%2.2f, "
                "avgLyrTemp2[%i]=%2.2f, avgLyrTemp2[%i]=%2.2f, "
                "depthT[%i]=%2.2f, depthS2[%i]=%2.2f, depthS2[%i]=%2.2f",
                i,
                j1,
                j2,
                i,
                avgLyrTempR[i],
                j1,
                avgLyrTemp2[j1],
                j2,
                avgLyrTemp2[j2],
                i,
                depth_Temp[i],
                j1,
                depth_Soil2[j1],
                j2,
                depth_Soil2[j2]
            );
        }

#endif
    }
    avgLyrTempR[nlyrTemp + 1] = endTemp;

#ifdef SWDEBUG
    if (debug) {
        sw_printf(
            "\nConf T: avgLyrTempR[%i]=%2.2f, avgLyrTempR[%i]=%2.2f",
            i,
            avgLyrTempR[i],
            i + 1,
            avgLyrTempR[i + 1]
        );
    }
#endif
}

/**
@brief Initialize soil temperature layer values by transfering soil layer values
to soil temperature layer values.

@param[in] cor Two dimensional array containing soil temperature data.
@param[in] nlyrSoil Number of soil layers.
@param[in] width_Soil Width of the soil layers.
@param[in] var Soil layer values to be interpolated.
@param[in] nlyrTemp Number of soil temperature layers.
@param[in] width_Temp Width of the soil temperature layers.
@param[out] res Values interpolated to soil temperature depths.
*/
void lyrSoil_to_lyrTemp(
    double cor[MAX_ST_RGR][MAX_LAYERS + 1],
    unsigned int nlyrSoil,
    const double width_Soil[],
    const double var[],
    unsigned int nlyrTemp,
    double width_Temp,
    double res[]
) {
#ifdef SWDEBUG
    int debug = 0;
#endif

    unsigned int i;
    unsigned int j = 0;
    double acc;
    double ratio;
    double sum;

    for (i = 0; i < nlyrTemp + 1; i++) {
        res[i] = 0.0;
        acc = 0.0;
        sum = 0.0;
        while (LT(acc, width_Temp) && j < nlyrSoil + 1) {
            if (GE(cor[i][j], 0.0) && j < nlyrSoil) {
                // there are soil layers to add
                ratio = cor[i][j] / width_Soil[j];
                res[i] += var[j] * ratio;
                sum += ratio;
                acc += cor[i][j];
                if (LT(acc, width_Temp)) {
                    j++;
                }
            } else if (LT(cor[i][j], 0.0)) {
                // negative cor values indicate end of soil layer profile
                // copying values from deepest soil layer
                // NOLINTBEGIN(clang-analyzer-core.UndefinedBinaryOperatorResult)
                ratio = -cor[i][j] / width_Soil[nlyrSoil - 1];
                res[i] += var[nlyrSoil - 1] * ratio;
                // NOLINTEND(clang-analyzer-core.UndefinedBinaryOperatorResult)
                sum += ratio;
                acc += (-cor[i][j]);
            }
        }
        res[i] = res[i] / sum;

#ifdef SWDEBUG
        if (debug) {
            sw_printf(
                "\n i = %u, j = %u, tempLyrVal=%2.2f,  soilLyrVal = %2.2f, "
                "cor[i][j] = %2.2f, ratio = %2.2f, acc=%2.2f,sum=%2.2f",
                i,
                j,
                res[i],
                var[j],
                cor[i][j],
                ratio,
                acc,
                sum
            );
        }
#endif
    }
}

/**
@brief Determine the average temperature of the soil surface under snow.

Based on equations from Parton et al. 1998. Equations 5 & 6. @cite Parton1998

@param[in] airTempAvg Average air temperature of the area (&deg;C).
@param[in] snow Snow-water-equivalent of the area (cm).

@return tSoilAvg Average temperature of the soil surface (&deg;C).
*/
double surface_temperature_under_snow(double airTempAvg, double snow) {
    double kSnow;          // the effect of snow based on swe
    double tSoilAvg = 0.0; // the average temeperature of the soil surface
                           // Parton et al. 1998. Equation 6.

    if (snow > 0) {
        if (airTempAvg >= 0) {
            tSoilAvg = -2.0;
        } else {
            // Parton et al. 1998. Equation 5.
            kSnow = fmax((-0.15 * snow + 1.0), 0.0);
            tSoilAvg = 0.3 * airTempAvg * kSnow + -2.0;
        }
    }

    return tSoilAvg;
}

void SW_ST_init_run(ST_RGR_VALUES *StRegValues) {
    StRegValues->soil_temp_init = swFALSE;
    StRegValues->fusion_pool_init = swFALSE;
    StRegValues->do_once_at_soiltempError = swTRUE;
    StRegValues->delta_time = SEC_PER_DAY;
}

/**
@brief Initialize soil temperature for a simulation run.

Calculate soil temperature layer profile and
translation matrix between soil layer profile and soil temperature layer
profile. Then, calculate initial values of members of SW_StRegValues across
soil temperature layer profile,
i.e., interpolate from initial site values across soil layer profile,
including soil temperature, soil density, field capacity, and wilting point.
Initialize soil temperature and frozen status across the soil layer profile.

@param[out] SW_StRegValues Struct of type ST_RGR_VALUES which keeps
    track of variables used within `soil_temperature()`
@param[in] SW_Site Struct of type SW_SITE describing the simulated site
@param[in,out] ptr_stError Boolean indicating whether there was an error.
@param[in,out] soil_temp_init Flag specifying if the values for
    `soil_temperature()` have been initialized
@param[in] airTemp Initial (today's) air temperature (&deg;C).
@param[in] swc Soilwater content in each layer before drainage
    (m<SUP>3</SUP> H<SUB>2</SUB>O).
@param[out] surfaceAvg Initialized surface temperature (&deg;C).
@param[out] avgLyrTemp Initialized soil temperature of the soil layer
    profile (&deg;C).
@param[out] lyrFrozen Frozen information at each soil layer.
@param[out] LogInfo Holds information on warnings and errors
*/
void SW_ST_setup_run(
    ST_RGR_VALUES *SW_StRegValues,
    SW_SITE *SW_Site,
    Bool *ptr_stError,
    Bool *soil_temp_init,
    double airTemp,
    double swc[],
    double *surfaceAvg,
    double avgLyrTemp[],
    double *lyrFrozen,
    LOG_INFO *LogInfo
) {
#ifdef SWDEBUG
    int debug = 0;
#endif

    LyrIndex i;

    if (!(*soil_temp_init)) {
#ifdef SWDEBUG
        if (debug) {
            sw_printf("\nCalling soil_temperature_setup\n");
        }
#endif

        /*
        Calculate soil temperature layer profile and
        translation matrix between soil layer profile and soil temperature layer
        profile. Then, calculate initial values of members of SW_StRegValues
        across soil temperature layer profile, i.e., interpolate from initial
        site values across soil layer profile, including soil temperature, soil
        density, field capacity, and wilting point.
        */
        soil_temperature_setup(
            SW_StRegValues,
            SW_Site->soilBulk_density,
            SW_Site->width,
            SW_Site->avgLyrTempInit,
            SW_Site->Tsoil_constant,
            SW_Site->n_layers,
            SW_Site->swcBulk_fieldcap,
            SW_Site->swcBulk_wiltpt,
            SW_Site->stDeltaX,
            SW_Site->stMaxDepth,
            SW_Site->stNRGR,
            SW_Site->depths,
            ptr_stError,
            soil_temp_init,
            LogInfo
        );
        if (LogInfo->stopRun) {
            return; // Exit function prematurely due to error
        }

        /* Initialize soil temperature and frozen status across the soil layer
         * profile. */
        ForEachSoilLayer(i, SW_Site->n_layers) {
            avgLyrTemp[i] = SW_Site->avgLyrTempInit[i];
        }

        set_frozen_unfrozen(
            SW_Site->n_layers,
            SW_Site->avgLyrTempInit,
            swc,
            SW_Site->swcBulk_saturated,
            SW_Site->width,
            lyrFrozen
        );

        /* Initialize surface temperature */
        *surfaceAvg = airTemp;
    }
}

/**
@brief Initialize soil structure and properties for soil temperature simulation.

@param[out] SW_StRegValues Struct of type ST_RGR_VALUES which keeps track of
    variables used within `soil_temperature()`.
@param[in] bDensity An array of the bulk density of the whole soil per soil
    layer, (g/cm3).
@param[in] width The width of the layers (cm).
@param[in] avgLyrTempInit An array of initial soil temperature values (&deg;C)
    utilized to interpolate initial soil temperature values at soil temperate
    layers.
@param[in] sTconst The soil temperature at a soil depth where it stays constant,
    i.e., `theMaxDepth`, as lower boundary condition (&deg;C).
@param[in] nlyrs The number of layers in the soil profile.
@param[in] fc An array of the field capacity of the soil layers (cm/layer).
@param[in] wp An array of the wilting point of the soil layers (cm/layer).
@param[in] deltaX The depth increment for the soil temperature calculations (cm)
@param[in] theMaxDepth the lower bound of the equation (cm).
@param[in] nRgr the number of regressions (1 extra value is needed for the
    avgLyrTempR).
@param[in] depths Depths of soil layers (cm)
@param[in,out] ptr_stError Booleans status of soil temperature error in
    *ptr_stError.
@param[out] soil_temp_init Flag specifying if the values for
    `soil_temperature()` have been initialized
@param[out] LogInfo Holds information on warnings and errors
*/
void soil_temperature_setup(
    ST_RGR_VALUES *SW_StRegValues,
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
) {
#ifdef SWDEBUG
    int debug = 0;
#endif

    // local vars
    unsigned int x1 = 0;
    unsigned int x2 = 0;
    unsigned int j = 0;
    unsigned int i;
    double d1 = 0.0;
    double d2 = 0.0;
    double acc = 0.0;
    // double fc_vwc[nlyrs], wp_vwc[nlyrs];
    double fc_vwc[MAX_LAYERS] = {0};
    double wp_vwc[MAX_LAYERS] = {0};

    // set `soil_temp_init` to 1 to indicate that this function was already
    // called and shouldn't be called again
    *soil_temp_init = swTRUE;


#ifdef SWDEBUG
    if (debug) {
        sw_printf(
            "\nInit soil layer profile: nlyrs=%i, sTconst=%2.2F;"
            "\nSoil temperature profile: deltaX=%F, theMaxDepth=%F, nRgr=%i\n",
            nlyrs,
            sTconst,
            deltaX,
            theMaxDepth,
            nRgr
        );
    }
#endif

    // if we have too many regression layers then quit
    if (nRgr + 1 >= MAX_ST_RGR) {
        if (!(*ptr_stError)) {
            (*ptr_stError) = swTRUE;

            // if the error hasn't been reported yet... print an error to the
            // logfile
            LogError(
                LogInfo,
                LOGERROR,
                "SOIL_TEMP FUNCTION ERROR: "
                "too many (n = %d) regression layers requested... "
                "soil temperature will NOT be calculated\n",
                nRgr
            );
        }
        return; // Exit function prematurely due to error
    }


    // init st
    for (i = 0; i < nRgr + 1; i++) {
        SW_StRegValues->fcR[i] = 0.0;
        SW_StRegValues->wpR[i] = 0.0;
        SW_StRegValues->bDensityR[i] = 0.0;
        SW_StRegValues->oldavgLyrTempR[i] = 0.0;
        for (j = 0; j < nlyrs + 1; j++) {
            // last column is used for soil temperature layers that are
            // deeper than the deepest soil profile layer
            SW_StRegValues->tlyrs_by_slyrs[i][j] = 0.0;
        }
    }
    SW_StRegValues->oldavgLyrTempR[nRgr + 1] = 0.0;

    // calculate evenly spaced depths of soil temperature profile
    for (i = 0; i < nRgr + 1; i++) {
        acc += deltaX;
        SW_StRegValues->depthsR[i] = acc;
#ifdef SWDEBUG
        if (debug) {
            sw_printf("\n i=%u, depthsR = %f", i, SW_StRegValues->depthsR[i]);
        }
#endif
    }

    // if soil temperature max depth is less than soil layer depth then quit
    if (LT(theMaxDepth, depths[nlyrs - 1])) {
        if (!(*ptr_stError)) {
            (*ptr_stError) = swTRUE;

            // if the error hasn't been reported yet... print an error to the
            // logfile
            LogError(
                LogInfo,
                LOGERROR,
                "SOIL_TEMP FUNCTION ERROR: soil temperature max depth (%5.2f "
                "cm) must be more than soil layer depth (%5.2f cm)... soil "
                "temperature will NOT be calculated\n",
                theMaxDepth,
                depths[nlyrs - 1]
            );
            return; // Exit function prematurely due to error
        }

        return; // exits the function
    }

    // calculate values of correspondance 'tlyrs_by_slyrs' between soil profile
    // layers and soil temperature layers
    for (i = 0; i < nRgr + 1; i++) {
        acc = 0.0; // cumulative sum towards deltaX
        while (x2 < nlyrs && acc < deltaX) {
            // there are soil layers to add
            if (GT(d1, 0.0)) {
                // add from previous (x1) soil layer
                j = x1;
                if (GT(d1, deltaX)) {
                    // soil temperatur layer ends within x1-th soil layer
                    d2 = deltaX;
                    d1 -= deltaX;
                } else {
                    d2 = d1;
                    d1 = 0.0;
                    x2++;
                }
            } else {
                // add from next (x2) soil layer
                j = x2;
                if (LT(SW_StRegValues->depthsR[i], depths[x2])) {
                    // soil temperatur layer ends within x2-th soil layer
                    d2 = fmax(deltaX - acc, 0.0);
                    d1 = width[x2] - d2;
                } else {
                    d2 = width[x2];
                    d1 = 0.0;
                    x2++;
                }
            }
            acc += d2;
            SW_StRegValues->tlyrs_by_slyrs[i][j] = d2;
        }
        x1 = x2;

        if (x2 >= nlyrs) {
            // soil temperature profile is deeper than deepest
            // soil layer; copy data from deepest soil layer
            SW_StRegValues->tlyrs_by_slyrs[i][x2] = -(deltaX - acc);
        }
    }
#ifdef SWDEBUG
    if (debug) {
        for (i = 0; i < nRgr + 1; i++) {
            sw_printf("\ntl_by_sl");
            for (j = 0; j < nlyrs + 1; j++) {
                sw_printf(
                    "[%i,%i]=%3.2f ", i, j, SW_StRegValues->tlyrs_by_slyrs[i][j]
                );
            }
        }
    }
#endif

    // calculate volumetric field capacity, volumetric wilting point,
    // bulk density of the whole soil, and
    // initial soil temperature for layers of the soil temperature profile
    lyrSoil_to_lyrTemp(
        SW_StRegValues->tlyrs_by_slyrs,
        nlyrs,
        width,
        bDensity,
        nRgr,
        deltaX,
        SW_StRegValues->bDensityR
    );
    lyrSoil_to_lyrTemp_temperature(
        nlyrs,
        depths,
        avgLyrTempInit,
        sTconst,
        nRgr,
        SW_StRegValues->depthsR,
        theMaxDepth,
        SW_StRegValues->oldavgLyrTempR
    );

    // Initial surface soil temperature `oldavgLyrTempR[0]` is not used;
    // `soil_temperature_today()` utilizes today's (not yesterday's) surface
    // temperatures here, set to missing so that it would produce
    // error/unreasonable values in case it would be used by mistake
    SW_StRegValues->oldavgLyrTempR[0] = SW_MISSING;

    // units of fc and wp are [cm H2O]; units of fcR and wpR are [m3/m3]
    for (i = 0; i < nlyrs; i++) {
        fc_vwc[i] = fc[i] / width[i];
        wp_vwc[i] = wp[i] / width[i];
    }

    lyrSoil_to_lyrTemp(
        SW_StRegValues->tlyrs_by_slyrs,
        nlyrs,
        width,
        fc_vwc,
        nRgr,
        deltaX,
        SW_StRegValues->fcR
    );
    lyrSoil_to_lyrTemp(
        SW_StRegValues->tlyrs_by_slyrs,
        nlyrs,
        width,
        wp_vwc,
        nRgr,
        deltaX,
        SW_StRegValues->wpR
    );

// SW_StRegValues->oldavgLyrTempR: index 0 is surface temperature
#ifdef SWDEBUG
    if (debug) {
        for (j = 0; j < nlyrs; j++) {
            sw_printf(
                "\nConv Soil depth[%i]=%2.2f, fc=%2.2f, wp=%2.2f, bDens=%2.2f, "
                "oldT=%2.2f",
                j,
                depths[j],
                fc[j],
                wp[j],
                bDensity[j],
                avgLyrTempInit[j]
            );
        }

        sw_printf(
            "\nConv ST oldSurfaceTR=%2.2f", SW_StRegValues->oldavgLyrTempR[0]
        );

        for (i = 0; i < nRgr + 1; i++) {
            sw_printf(
                "\nConv ST depth[%i]=%2.2f, fcR=%2.2f, wpR=%2.2f, "
                "bDensR=%2.2f, oldTR=%2.2f",
                i,
                SW_StRegValues->depthsR[i],
                SW_StRegValues->fcR[i],
                SW_StRegValues->wpR[i],
                SW_StRegValues->bDensityR[i],
                SW_StRegValues->oldavgLyrTempR[i + 1]
            );
        }
    }
#endif
}

/**
@brief Function to determine if a soil layer is frozen/unfrozen, as well as
change the frozen/unfrozen status of the layer.

Equations based on Parton 1998. @cite Parton1998

@param[in] nlyrs Number of layers.
@param[in] avgLyrTemp The temperature of the soil layers (&deg;C).
@param[in] swc The soil water content of the soil layers (cm/cm).
@param[in] swc_sat The satured soil water content of the soil layers (cm/cm).
@param[in] width The width of them soil layers (cm).
@param[out] lyrFrozen Frozen information at each layer.
*/
void set_frozen_unfrozen(
    unsigned int nlyrs,
    double avgLyrTemp[],
    double swc[],
    double swc_sat[],
    const double width[],
    double lyrFrozen[]
) {

    // 	TODO: freeze surfaceWater and restrict infiltration

    unsigned int i;

    for (i = 0; i < nlyrs; i++) {
        if (LE(avgLyrTemp[i], FREEZING_TEMP_C) &&
            GT(swc[i], swc_sat[i] - width[i] * MIN_VWC_TO_FREEZE)) {
            lyrFrozen[i] = swTRUE;
        } else {
            lyrFrozen[i] = swFALSE;
        }
    }
}

/**
@brief Calculate fusion pools based on soil profile layers, soil
freezing/thawing, and if freezing/thawing not completed during one day, then
adjust soil temperature.

@note: THIS FUNCTION IS CURRENTLY NOT OPERATIONAL

Based on equations from Eitzinger 2000. @cite Eitzinger2000

@param oldavgLyrTemp An array of yesterday's temperature values (&deg;C).
@param avgLyrTemp Temperature values soil layers (&deg;C).
@param shParam A constant for specific heat capacity equation.
@param nlyrs Number of layers available.
@param vwc An array of temperature-layer VWC values (cm/layer).
@param bDensity An array of the bulk density of the whole soil per soil layer
    (g/cm<SUP>3</SUP>).
@param[in,out] fusion_pool_init Specifies if the values for the soil fusion
    (thawing/freezing) section of `soil_temperature()` have been initialized
@param oldsFusionPool_actual Yesterdays actual fusion pool at each soil layer

@return sFadjusted_avgLyrTemp Adjusted soil layer temperature due to
freezing/thawing
*/
unsigned int adjust_Tsoil_by_freezing_and_thawing(
    const double oldavgLyrTemp[],
    const double avgLyrTemp[],
    double shParam,
    unsigned int nlyrs,
    const double vwc[],
    const double bDensity[],
    Bool *fusion_pool_init,
    double oldsFusionPool_actual[]
) {
    // Calculate fusion pools based on soil profile layers, soil
    // freezing/thawing, and if freezing/thawing not completed during one day,
    // then adjust soil temperature based on Eitzinger, J., W. J. Parton, and M.
    // Hartman. 2000. Improvement and Validation of A Daily Soil Temperature
    // Submodel for Freezing/Thawing Periods. Soil Science 165:525-534.

    // NOTE: THIS FUNCTION IS CURRENTLY NOT OPERATIONAL: DESCRIPTION BY
    // EITZINGER ET AL. 2000 SEEMS INSUFFICIENT

    //	double deltaTemp, Cis, sFusionPool[nlyrs], sFusionPool_actual[nlyrs];
    // To avoid compiler warnings "warning: parameter set but not used"
    (void) oldavgLyrTemp;
    (void) avgLyrTemp;
    (void) shParam;
    (void) nlyrs;
    (void) vwc;
    (void) bDensity;
    // end avoid compiler warnings

    unsigned int i;
    unsigned int sFadjusted_avgLyrTemp;

    /* local variables explained:
     debug - 1 to print out debug messages & then exit the program after
     completing the function, 0 to not.  default is 0.

     deltaTemp - the change in temperature for each day

     Cis - heat capacity of the i-th non-frozen soil layer (cal cm-3 K-1)

     sFusionPool[] - the fusion pool for each soil layer

     sFusionPool_actual[] - the actual fusion pool for each soil layer

     sFadjusted_avgLyrTemp - if soil layer temperature was changed due to
     freezing/thawing
     */


    if (!(*fusion_pool_init)) {
        for (i = 0; i < nlyrs; i++) {
            oldsFusionPool_actual[i] = 0.;
        }
        *fusion_pool_init = swTRUE;
    }

    sFadjusted_avgLyrTemp = 0;

    // clang-format off
/*
// THIS FUNCTION IS CURRENTLY NOT OPERATIONAL: DESCRIPTION BY EITZINGER ET AL. 2000 SEEMS INSUFFICIENT
	for (i = 0; i < nlyrs; i++){
		sFusionPool_actual[i] = 0.;

		// only need to do something if the soil temperature is at the freezing point, or the soil temperature is transitioning over the freezing point
		if (EQ(oldavgLyrTemp[i], FREEZING_TEMP_C) || (GT(oldavgLyrTemp[i], FREEZING_TEMP_C) && LT(avgLyrTemp[i], FREEZING_TEMP_C))|| (LT(oldavgLyrTemp[i], FREEZING_TEMP_C) && GT(avgLyrTemp[i], FREEZING_TEMP_C)) ){

			Cis = (vwc[i] + shParam * (1. - vwc[i])) * bDensity[i]; // Cis = sh * (bulk soil density): "Cis is the heat capacity of the i-th non-frozen soil layer (cal cm-3 K-1)" estimated based on Parton 1978 eq. 2.23 units(sh) = [cal g-1 C-1]; unit conversion factor = 'bulk soil density' with units [g/cm3]
			sFusionPool[i] = - FUSIONHEAT_H2O * vwc[i] / Cis * TCORRECTION; // Eitzinger et al. (2000): eq. 3 wherein sFusionPool[i] = Pi = the fusion energy pool of a soil layer given as a temperature equivalent (K), i.e., Pi = temperature change that must happen to freeze/thaw a soil layer

			// Calculate actual status of the fusion energy pool in [Celsius]
			// Eitzinger et al. (2000): eq. 6 wherein sFusionPool_actual[i] = Pai and avgLyrTemp[i] = T(sav-1)i + [delta]T(sav)i
			if( GT(oldavgLyrTemp[i], FREEZING_TEMP_C) && LE(avgLyrTemp[i], FREEZING_TEMP_C) ){ // Freezing?
				// soil above freezing yesterday; soil at or below freezing today
				sFusionPool_actual[i] = avgLyrTemp[i];
			} else {
				if( LT(st->oldsFusionPool_actual[i], FREEZING_TEMP_C) && (LE(avgLyrTemp[i], FREEZING_TEMP_C) || GT(avgLyrTemp[i], FREEZING_TEMP_C)) ){
					// Thawing?
					// actual fusion pool below freezing yesterday AND soil below or at freezing today
// TODO: I guess the first should be above (and not below freezing yesterday)?
					// actual fusion pool below freezing yesterday AND soil above freezing today
					deltaTemp = avgLyrTemp[i] - oldavgLyrTemp[i]; // deltaTemp = [delta]T(sav)i; oldavgLyrTemp[i] = T(sav-1)i
					sFusionPool_actual[i] = st->oldsFusionPool_actual[i] + deltaTemp;
				} else {
// TODO: What if not? This situation is not covered by Eitzinger et al. 2000
				}
			}

			// Eitzinger et al. (2000): eq. 4
			if( LT(sFusionPool_actual[i], 0.) && LT(sFusionPool[i], sFusionPool_actual[i]) ){
// TODO: No condition for thawing considered?
// TODO: If partial thawing/freezing, shouldn't avgLyrTemp[i] bet set to FREEZING_TEMP_C?
				// If the freezing process of a relevant soil layer is not finished within a day, it is assumed that no change in the daily average soil layer temperature (Eq. (4)) [...] can occur.
				// It implies that the state of the soil layer (frozen, partly frozen, or unfrozen) is not changed by the diurnal soil temperature change within the daily time step.
				avgLyrTemp[i] = oldavgLyrTemp[i];
				sFadjusted_avgLyrTemp = 1;
			}
		}

		// updating the values of yesterdays fusion pools for the next time the function is called...
		st->oldsFusionPool_actual[i] = sFusionPool_actual[i];

	}
*/
    // clang-format on

    return sFadjusted_avgLyrTemp;
}

/**
@brief Calculate today's soil temperature for each layer.

The function
    - calculates soil temperature values in arrays avgLyrTempR and
      temperatureRangeR
    - selects a shorter time step if required for a stable solution
      (@cite Parton1978, @cite Parton1984) and returns the updated value of
      *ptr_dTime
    - updates the status of soil temperature error in *ptr_stError.

@param[in,out] ptr_dTime Yesterday's successful time step in seconds.
@param[in] deltaX The depth increment for the soil temperature (regression)
    calculations (cm).
@param[in] sT1 The soil surface temperature as upper boundary condition (&deg;C)
@param[in] sTconst The soil temperature at a soil depth where it stays constant
    as lower boundary condition (&deg;C).
@param[in] nRgr The number of regressions (1 extra value is needed for the
    avgLyrTempR and oldavgLyrTempR for the last layer).
@param[in,out] avgLyrTempR An array of today's (regression)-layer soil
    temperature values (&deg;C).
@param[in] oldavgLyrTempR An array of yesterday's (regression)-layer soil
    temperature value (&deg;C).
@param[in] vwcR An array of temperature-layer VWC values (cm/layer).
@param[in] wpR An array of temperature-layer wilting point values (cm/layer).
@param[in] fcR An array of temperature-layer field capacity values (cm/layer).
@param[in] bDensityR temperature-layer bulk density of the whole soil
    (g/cm<SUP>3</SUP>).
@param[in] csParam1 A constant for the soil thermal conductivity equation.
@param[in] csParam2 A constant for the soil thermal conductivity equation.
@param[in] shParam A constant for specific heat capacity equation.
@param[in,out] *ptr_stError A boolean indicating whether there was an error.
@param[in] surface_range Temperature range at the surface (&deg;C)
@param[out] temperatureRangeR An array of temperature ranges at each
    (regression)-layer to be interpolated (&deg;C)
@param[in] depthsR Evenly spaced depths of the soil temperature profile (cm).
@param[in] year Current year being run in the simulation
@param[in] doy Day of the year (base1) [1-366]

@note
avgLyrTempR[0] and temperatureRangeR[0] represent soil surface conditions.
*/
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
) {
#ifdef SWDEBUG
    int debug = 0;
    if (year == 1981 && doy > 180 && doy < 191) {
        debug = 0;
    }
#else
    /* Silence compiler for year/doy not being used if SWDEBUG isn't defined */
    (void) year;
    (void) doy;
#endif

    unsigned int i;
    unsigned int k;
    unsigned int m;
    unsigned int Nsteps_per_day = 1;
    double pe;
    double cs;
    double sh;
    double dT_to_dX2;
    double alpha;
    double part2;
    double oldavgLyrTempR2[MAX_ST_RGR];

    Bool Tsoil_not_exploded = swTRUE;

    // upper boundary condition; index 0 indicates surface and not first layer
    // --> required for interpolation from soil temperature layers to soil
    // layers
    avgLyrTempR[0] = sT1;
    temperatureRangeR[0] = surface_range;

    // lower boundary condition; assuming that lowest layer is the depth of
    // constant soil temperature
    avgLyrTempR[nRgr + 1] = sTconst;


    //------ Mean soil temperature
    do {
        /* calculate mean soil temperature across layers and sub-daily
           timesteps; if unstable (ptr_stError), then reduce timestep and repeat
           calculation; however, abandon soil temperature calculation if more
           than 16 sub-time steps (90 min) are required or if soil temperatures
           go beyond ± 100 C)
        */

        dT_to_dX2 = (*ptr_dTime) / squared(deltaX);
        Nsteps_per_day = SEC_PER_DAY / *ptr_dTime;

        // reset previous soil temperature values to yesterday's
        for (i = 0; i <= nRgr + 1; i++) {
            oldavgLyrTempR2[i] = oldavgLyrTempR[i];
        }

        // Loop over sub-daily timesteps
        for (m = 0; m < Nsteps_per_day; m++) {

            // Loop over interpolation layers
            for (i = 1; i < nRgr + 1; i++) {
                // goes to nRgr, because the soil temp of the last interpolation
                // layer (nRgr) is the sTconst
                k = i - 1;

                // the units are volumetric!
                pe = (vwcR[k] - wpR[k]) / (fcR[k] - wpR[k]);

                // Parton (1978) eq. 2.22: soil thermal conductivity;
                // csParam1 = 0.0007, csParam2 = 0.0003
                cs = csParam1 + (pe * csParam2);

                // Parton (1978) eq. 2.22: specific
                // heat capacity; shParam = 0.18
                // TODO: adjust thermal conductivity
                // and heat capacity if layer is frozen
                sh = vwcR[k] + shParam * (1. - vwcR[k]);

                // thermal diffusivity
                alpha = cs / (sh * bDensityR[k]);

#ifdef SWDEBUG
                if (debug) {
                    sw_printf(
                        "step=%d/%d (dTime=%.0f), sl=%d/%d: \n"
                        "\t* pe(%.3f) = (%.3f) / (%.3f) = (vwcR(%.3f) - "
                        "wpR(%.3f)) / (fcR(%.3f) - wpR(%.3f))\n"
                        "\t* cs(%.3f) = csParam1(%.3f) + (pe * "
                        "csParam2(%.5f))(%.3f)\n"
                        "\t* sh(%.3f) = vwcR + shParam(%.3f) * (1. - "
                        "vwcR)(%.3f)\n"
                        "\t* alpha(%.4f) = cs / (sh * dens(%.2f))\n",
                        m,
                        Nsteps_per_day,
                        *ptr_dTime,
                        i,
                        k,
                        pe,
                        vwcR[k] - wpR[k],
                        fcR[k] - wpR[k],
                        vwcR[k],
                        wpR[k],
                        fcR[k],
                        wpR[k],
                        cs,
                        csParam1,
                        csParam2,
                        pe * csParam2,
                        sh,
                        shParam,
                        1. - vwcR[k],
                        alpha,
                        bDensityR[k]
                    );
                }
#endif

                // clang-format off
                /* Check that approximation is stable
                    - Derivation to confirm Parton 1984:
                        alpha * K * deltaT / deltaX ^ 2 <= 0.5

                    - Let f be a continuously differentiable function with
                        attractive fixpoint f(a) = a;
                        then, for x elements of basin of attraction:
                            iteration x[n+1] = f(x[n]) is stable
                            if spectral radius rho(f) < 1
                        with
                            rho(f) = max(abs(eigenvalues(iteration matrix)))

                    - Function f is here,
                        x[i; t+1] = f(x[i; t]) =
                        = x[i; t] + parts * (str[i-1; t+1] - 2 * x[i; t] + str[i+1; t]) =
                        = x[i; t] * (1 - 2 * parts) + parts * (str[i-1; t+1] + str[i+1; t])

                    - Fixpoint a is then, using f(a) = a,
                        a = a * (1 - 2 * parts) + parts * (str[i-1; t+1] + str[i+1; t])
                        ==> a = parts * (str[i-1; t+1] + str[i+1; t]) / (2 * parts) =
                                = (str[i-1; t+1] + str[i+1; t]) / 2
                    - Homogenous recurrence form of function f is then here,
                        (x[i; t+1] - a) = (x[i; t] - a) * (1 - 2 * parts)

                    - Iteration matrix is here C = (1 - 2 * parts)
                        with eigenvalue lambda from
                        det(C - lambda * 1) = 0 ==> lambda = C

                    - Thus, iteration is stable if abs(lambda) < 1, here
                            abs(1 - 2 * parts) < 1 ==> abs(parts) < 1/2
                */
                // clang-format on

                /* Flag whether an error has occurred */
                (*ptr_stError) = GE(alpha * dT_to_dX2, 0.5) ? swTRUE : swFALSE;
                if (*ptr_stError) {
                    *ptr_dTime = *ptr_dTime / 2;
                    /* step out of for-loop through regression soil layers and
                     * re-start with adjusted dTime */
                    break;
                }

                part2 = avgLyrTempR[i - 1] - 2 * oldavgLyrTempR2[i] +
                        oldavgLyrTempR2[i + 1];

                // Parton (1978) eq. 2.21:
                avgLyrTempR[i] = oldavgLyrTempR2[i] + alpha * dT_to_dX2 * part2;

#ifdef SWDEBUG
                if (debug) {
                    sw_printf(
                        "\t* d(Tsoil[%d]) = %.2f from\n"
                        "\t\t* dT/dX2 = %.1f = dt(%.0f) / dX^2(%.0f)\n"
                        "\t\t* p2 = %.4f = Ts[i-1, t](%.2f) - 2 * Ts[i, "
                        "t-1](%.2f) + Ts[i+1, t-1](%.2f)\n"
                        "\t-> Tsoil[now]=%.2f, Tsoil[yesterday]=%.2f\n\n",
                        i,
                        alpha * dT_to_dX2 * part2,
                        dT_to_dX2,
                        *ptr_dTime,
                        squared(deltaX),
                        part2,
                        avgLyrTempR[i - 1],
                        oldavgLyrTempR2[i],
                        oldavgLyrTempR2[i + 1],
                        avgLyrTempR[i],
                        oldavgLyrTempR[i]
                    );
                }
#endif

                // Sensibility check to short-cut exploding soil temperature
                // values
                if (GT(avgLyrTempR[i], 100.) || LT(avgLyrTempR[i], -100.)) {
                    Tsoil_not_exploded = swFALSE;
                    *ptr_stError = swTRUE;
                    break;
                }
            }

            if (*ptr_stError) {
                /* step out of for-loop through sub-timesteps */
                break;
            }

            // updating the values of soil temperature for the next sub-time
            // step
            for (i = 0; i < nRgr + 1; i++) {
                oldavgLyrTempR2[i] = avgLyrTempR[i];
            }
        }

        /* repeat calculation of mean soil temperature if
                        - unstable (ptr_stError)
                        - not more than 16 sub-time steps (90 min)
                        - soil temperatures within ± 100 C

                otherwise, return with error (and abandon soil temperature)
        */
    } while (*ptr_stError && Tsoil_not_exploded && Nsteps_per_day <= 16);


    //------ Range of soil temperature
    if (!(*ptr_stError)) {
        alpha = 0;

        for (i = 1; i < nRgr + 1; i++) {
            k = i - 1;

            // the units are volumetric!
            pe = (vwcR[k] - wpR[k]) / (fcR[k] - wpR[k]);

            // Parton (1978) eq. 2.22: soil thermal conductivity;
            // csParam1 = 0.0007, csParam2 = 0.0003
            cs = csParam1 + (pe * csParam2);

            // Parton (1978) eq. 2.22: specific heat
            // capacity; shParam = 0.18
            // TODO: adjust thermal conductivity and
            // heat capacity if layer is frozen
            sh = vwcR[k] + shParam * (1. - vwcR[k]);

            // Calculate the diurnal temperature range at depth for
            // interpolation Makes use of equation 6 from Parton 1984

            // sum of alpha across shallower layers
            alpha += cs / (sh * bDensityR[k]);

            // eq. 6 assumes that soil surface temperature follows a sine wave
            // with a daily period, i.e. 86400 seconds
            temperatureRangeR[i] =
                surface_range *
                exp(-depthsR[k] * sqrt(swPI / (86400. * alpha / (k + 1.))));

#ifdef SWDEBUG
            if (debug) {
                sw_printf(
                    "layer=%d/depth=%.0f: "
                    "range(Tsoil)=%.2f = range(Tsurface)(%.2f) * damp(%.4f) "
                    "(mean(alpha)=%.4f)\n",
                    k,
                    depthsR[k],
                    temperatureRangeR[i],
                    surface_range,
                    exp(-depthsR[k] * sqrt(swPI / (86400. * alpha / (k + 1.)))),
                    alpha / (k + 1.)
                );
            }
#endif
        }
    }
}

/**********************************************************************
PURPOSE: Calculate soil temperature for each layer
* based on Parton 1978, ch. 2.2.2 Temperature-profile Submodel
* interpolation values are from a mixture of interpolation & extrapolation
* soil freezing based on Eitzinger et al. 2000

@reference Eitzinger, J., W. J. Parton, and M. Hartman. 2000.
    Improvement and Validation of A Daily Soil Temperature Submodel for
    Freezing/Thawing Periods. Soil Science 165:525-534.
@reference Parton, W. J. 1978. Abiotic section of ELM. Pages 31–53 in G.
    S. Innis, editor. Grassland simulation model. Springer, New York, NY.
@reference Parton, W. J. 1984. Predicting Soil Temperatures in A
    Shortgrass Steppe. Soil Science 138:93-101.

*NOTE* There will be some degree of error because the original equation is
written for soil layers of 15 cm.  if soil layers aren't all 15 cm then linear
regressions are used to estimate the values (error should be relatively small
though). *NOTE* Function might not work correctly if the maxDepth of the soil
is > 180 cm, since Parton's equation goes only to 180 cm *NOTE* Function will
run if maxLyrDepth > maxDepth of the equation, but the results might be
slightly off...

HISTORY:
05/24/2012 (DLM) initial coding, still need to add to header file, handle if
the layer height is > 15 cm properly, & test

05/25/2012 (DLM) added all this 'fun' crazy linear interpolation stuff

05/29/2012 (DLM) still working on the function, linear interpolation stuff
should work now.  needs testing

05/30/2012 (DLM) got rid of nasty segmentation fault error, also
tested math seems correct after checking by hand.
added the ability to change the value of deltaX

05/30/2012 (DLM) added # of lyrs check & maxdepth check at the beginning to
make sure code doesn't blow up... if there isn't enough lyrs (ie < 2) or the
maxdepth is too little (ie < deltaX * 2), the function quits out and reports an
error to the user

05/31/2012 (DLM) added theMaxDepth variable to allow the changing of the
maxdepth of the equation, also now stores most interpolation
data in a structure to reduce redundant interpolation calculations & speeds
things up

06/01/2012 (DLM) changed deltaT variable from hours to seconds, also
changed some of the interpolation calculations so that swc, fc, & wp
regressions are scaled properly... results are actually starting to look
usable!

06/13/2012 (DLM) no longer extrapolating values for interpolation
layers that are out of the bounds of the soil layers... instead they are now
set to the last soil layers values.  extrapolating code is still in the
function and can be commented out and used if wishing to go back to
extrapolating the values...

03/28/2013 (clk) added a check to see if the soil was freezing/thawing and
adjusted the soil temperature correctly during this phase change.
If the temperature was in this area, also needed to re run
soil_temperature_init on next call because you need to also change the
regression temperatures to match the change in soil temperature.

12/23/2014 (drs) re-wrote soil temperature functions:
    - soil characteristics (fc, wp, bulk density) of soil temperature now
      integrated across all soil layers
      (instead of linear interpolation between the two extreme layers);
    - interpolation for top soil layer with surface temperature only
      if no other information available
    - mean of interpolations if multiple layers affect a soil profile layer
    - reporting of surface soil temperature
    - test iteration steps that they meet the criterion of a stable solution

11/23/2015 (drs) added call to surface_temperature_under_snow()

12/24/2015 (drs) fixed code following realization that equations in
Parton 1978 were using units of volumetric soil water [cm3/cm3] instead of
soil water content per layer [cm] - thanks to a
comparison with the soil temperature formulations in R by Caitlin Andrews and
Matt Petrie

01/04/2016 (drs) moved code for freezing/thawing of soil layers to
function adjust_Tsoil_by_freezing_and_thawing()

INPUTS:
airTemp - the average daily air temperature in celsius
pet - the potential evapotranspiration rate
aet - the actual evapotranspiration rate
biomass - the standing-crop biomass
swc - soil water content
bDensity - bulk density of the whole soil per soil layer
width - width of layers
oldavgLyrTemp - soil layer temperatures from the previous day in celsius
nlyrs - number of soil layers, must be greater than 1 or the function won't
work right
fc - field capacity for each soil layer
wp - wilting point for each soil layer
bmLimiter - biomass limiter constant (300 g/m^2)
t1Params - constants for the avg temp at the top of soil equation
(15, -4, 600) there is 3 of them
csParams - constants for the soil thermal conductivity equation
(0.00070, 0.00030) there is 2 of them
shParam - constant for the specific heat capacity equation (0.18)
sh -  specific heat capacity equation
snowdepth - the depth of snow cover (cm)
sTconst - the constant soil temperature in celsius
deltaX - the distance between profile points (default is 15 from Parton's
equation, wouldn't recommend changing the value from that).  180 must be evenly
divisible by this number.
theMaxDepth - the lower bound of the equation
(default is 180 from Parton's equation, wouldn't recommend changing the value
from that).
nRgr - the number of regressions
(1 extra value is needed for the avgLyrTempR and oldavgLyrTempR for the
last layer)
sFadjusted_avgLyrTemp - if soil layer temperature was changed due to
freezing/thawing; if so, then temperature of the soil temperature layers
need to updated as well

OUTPUT:
avgLyrTemp - soil layer temperatures in celsius
**********************************************************************/

/**
@brief Calculate soil temperature for each layer.

Equations based on Eitzinger, Parton, and Hartman 2000. @cite Eitzinger2000,
Parton 1978. @cite Parton1978, Parton 1984. @cite Parton1984

@param[in,out] SW_StRegValues Struct of type SW_StRegValues which keeps
     track of variables used within `soil_temperature()`
@param[out] *surface_max Maxmimum surface temperature (&deg;C)
@param[out] *surface_min Minimum surface temperature (&deg;C)
@param[out] lyrFrozen Frozen information at each layer.
@param[in] airTemp Average daily air temperature (&deg;C).
@param[in] pet Potential evapotranspiration rate (cm/day).
@param[in] aet Actual evapotranspiration (cm/day).
@param[in] biomass Standing-crop biomass (g/m<SUP>2</SUP>).
@param[in] swc Soilwater content in each layer before drainage
    (m<SUP>3</SUP>H<SUB>2</SUB>O).
@param[in] swc_sat The satured soil water content of the soil layers (cm/cm).
@param[in] bDensity An array of the bulk density of the whole soil per soil
    layer (g/cm<SUP>3</SUP>).
@param[in] width The width of the layers (cm).
@param[in,out] avgLyrTemp Temperature values of soil layers
    (yesterday's values for input; today's values as output)  (&deg;C).
@param[out] surfaceAvg Average daily surface air temperature
@param[in] nlyrs Number of layers in the soil profile.
@param[in] bmLimiter Biomass limiter constant (300 g/m<SUP>2</SUP>).
@param[in] t1Param1 Constant for the avg temp at the top of soil equation (15).
@param[in] t1Param2 Constant for the avg temp at the top of soil equation (-4).
@param[in] t1Param3 Constant for the avg temp at the top of soil equation (600).
@param[in] csParam1 Constant for the soil thermal conductivity equation
    (0.00070).
@param[in] csParam2 Constant for the soil thermal conductivity equation
    (0.00030).
@param[in] shParam Constant for the specific heat capacity equation (0.18).
@param[in] snowdepth Depth of snow cover (cm)
@param[in] sTconst Constant soil temperature (&deg;C).
@param[in] deltaX Distance between profile points (default is 15 cm from
    Parton's equation @cite Parton1984).
@param[in] theMaxDepth Lower bound of the equation (default is 180 cm from
    Parton's equation @cite Parton1984).
@param[in] nRgr Number of regressions (1 extra value is needed for the
    avgLyrTempR and oldavgLyrTempR for the last layer.
@param[in] snow Snow-water-equivalent of the area (cm).
@param[in] max_air_temp Maximum air temperature of Today (&deg;C)
@param[in] min_air_temp Minimum air temperature of Today (&deg;C)
@param[in] H_gt Daily global (tilted) irradiation [MJ / m2]
@param[in] year Current year in simulation
@param[in] doy Day of the year (base1) [1-366]
@param[in] depths Depths of soil layers (cm)
@param[out] maxLyrTemperature An array holding all of the layers maximum
    temperature (&deg;C)
@param[out] minLyrTemperature An array holding all of the layers minimum
    temperature (&deg;C)
@param[out] *ptr_stError Boolean indicating whether there was an error.
@param[out] LogInfo Holds information on warnings and errors
*/
void soil_temperature(
    ST_RGR_VALUES *SW_StRegValues,
    double *surface_max,
    double *surface_min,
    double lyrFrozen[],
    double airTemp,
    double pet,
    double aet,
    double biomass,
    double swc[],
    double swc_sat[],
    double bDensity[],
    double width[],
    double avgLyrTemp[],
    double *surfaceAvg,
    unsigned int nlyrs,
    double bmLimiter,
    double t1Param1,
    double t1Param2,
    double t1Param3,
    double csParam1,
    double csParam2,
    double shParam,
    double snowdepth,
    double sTconst,
    double deltaX,
    double theMaxDepth,
    unsigned int nRgr,
    double snow,
    double max_air_temp,
    double min_air_temp,
    double H_gt,
    TimeInt year,
    TimeInt doy,
    double depths[],
    double maxLyrTemperature[],
    double minLyrTemperature[],
    Bool *ptr_stError,
    LOG_INFO *LogInfo
) {
#ifdef SWDEBUG
    int debug = 0;
    if (year == 1980 && doy < 10) {
        debug = 0;
    }
#endif

    unsigned int i;
    unsigned int sFadjusted_avgLyrTemp;
    double oldavgLyrTemp[MAX_LAYERS];
    double vwc[MAX_LAYERS];
    double vwcR[MAX_ST_RGR];
    double avgLyrTempR[MAX_ST_RGR];
    double temperatureRangeR[MAX_ST_RGR];
    double temperatureRange[MAX_LAYERS];
    double surface_range;

    /* local variables explained:
     debug - 1 to print out debug messages & then exit the program after
     completing the function, 0 to not.  default is 0.

     T1 = surfaceAvg - the average daily temperature at the top of the soil in
     celsius

     vwc - volumetric soil-water content

     pe - ratio of the difference between volumetric soil-water content &
     soil-water content at the wilting point to the difference between soil
     water content at field capacity & soil-water content at wilting point.

     cs - soil thermal conductivity sh - specific heat capacity

     depths[nlyrs] - the depths of each layer of soil, calculated in the
     function vwcR[],

     avgLyrTempR[] - anything with a R at the end of the variable name stands
     for the interpolation of that array
     */

#ifdef SWDEBUG
    if (debug) {
        sw_printf("\n\nNew call to soil_temperature()");
    }
#endif

    if (!SW_StRegValues->soil_temp_init) {
        *ptr_stError = swTRUE;

        LogError(
            LogInfo,
            LOGERROR,
            "SOILWAT2 ERROR soil temperature module was not initialized.\n"
        );

        return; // Exit function prematurely due to error
    }

    // calculating min/mean/max surface temperature
    if (GT(snowdepth, 0.0)) {
        // underneath snow, based on Parton 1998
        *surfaceAvg = surface_temperature_under_snow(airTemp, snow);
        *surface_min = *surface_max = *surfaceAvg;

#ifdef SWDEBUG
        if (debug) {
            sw_printf(
                "\nTsurface (min/mean/max) = %f/%f/%f (underneath snowpack)\n",
                *surface_min,
                *surfaceAvg,
                *surface_max
            );
        }
#endif

    } else {
        // No snow cover

        // Calculate max/min surface temperature based on eqations 4 and 5 from
        // Parton 1984 surface_max uses effect on plant canopy (E_B) which uses
        // the equation in figure 1b Radiation calculation (second line of
        // calculation) is based off of figure 2 in Parton 1984
        *surface_max = exp(-.0048 * biomass) - .13;
        *surface_max *=
            ((0.35 * max_air_temp) +
             24.07 * (1.0 - exp(-.000038 * (H_gt * 1000))));
        *surface_max += max_air_temp;
        *surface_min = min_air_temp + (.006 * biomass) - 1.82;

        if (LE(biomass, bmLimiter)) {
            // bmLimiter = 300

            // t1Param1 = 15; drs (Dec 16, 2014): this interpretation of
            // Parton 1978's 2.20 equation (the printed version misses a
            // closing parenthesis) removes a jump of T1 for biomass =
            // bmLimiter
            *surfaceAvg = airTemp + (t1Param1 * pet * (1. - (aet / pet)) *
                                     (1. - (biomass / bmLimiter)));
#ifdef SWDEBUG
            if (debug) {
                sw_printf(
                    "\nTsurface (min/mean/max) = %f/%f/%f "
                    "\n  mean = %f + (%f * %f * (1 - (%f / %f)) * (1 - (%f / "
                    "%f)) ) )\n",
                    *surface_min,
                    *surfaceAvg,
                    *surface_max,
                    airTemp,
                    t1Param1,
                    pet,
                    aet,
                    pet,
                    biomass,
                    bmLimiter
                );
            }
#endif

        } else {
            // t1Param2 = -4, t1Param3 = 600; math is correct
            *surfaceAvg =
                airTemp + ((t1Param2 * (biomass - bmLimiter)) / t1Param3);
#ifdef SWDEBUG
            if (debug) {
                sw_printf(
                    "\nTsurface (min/mean/max) = %f/%f/%f "
                    "\n  mean= %f + ((%f * (%f - %f)) / %f)\n",
                    *surface_min,
                    *surfaceAvg,
                    *surface_max,
                    airTemp,
                    t1Param2,
                    biomass,
                    bmLimiter,
                    t1Param3
                );
            }
#endif
        }
    }

    surface_range = *surface_max - *surface_min;


    if (*ptr_stError) {
        /* we return early (but after calculating surface temperature) and
                        without attempt to calculate soil temperature again */
        if (SW_StRegValues->do_once_at_soiltempError) {
            for (i = 0; i < nlyrs; i++) {
                // reset soil temperature values
                avgLyrTemp[i] = SW_MISSING;
                // make sure that no soil layer is stuck in frozen status
                lyrFrozen[i] = swFALSE;
            }

            SW_StRegValues->do_once_at_soiltempError = swFALSE;
        }

#ifdef SWDEBUG
        if (debug) {
            sw_printf("\n");
        }
#endif

        return; // Exit function prematurely due to error
    }


    for (i = 0; i < nlyrs; i++) {
        // calculate volumetric soil water content for soil temperature layers
        vwc[i] = swc[i] / width[i];

        // save yesterday's values for later use
        oldavgLyrTemp[i] = avgLyrTemp[i];
    }

    lyrSoil_to_lyrTemp(
        SW_StRegValues->tlyrs_by_slyrs, nlyrs, width, vwc, nRgr, deltaX, vwcR
    );

#ifdef SWDEBUG
    if (debug) {
        sw_printf("\nregression values:");
        for (i = 0; i < nRgr; i++) {
            sw_printf(
                "\nk %2d width %f depth %f vwcR %f fcR %f wpR %f "
                "oldavgLyrTempR %f bDensityR %f",
                i,
                deltaX,
                SW_StRegValues->depthsR[i],
                vwcR[i],
                SW_StRegValues->fcR[i],
                SW_StRegValues->wpR[i],
                SW_StRegValues->oldavgLyrTempR[i],
                SW_StRegValues->bDensityR[i]
            );
        }

        sw_printf("\nlayer values:");
        for (i = 0; i < nlyrs; i++) {
            sw_printf(
                "\ni %2d width %f depth %f vwc %f bDensity %f",
                i,
                width[i],
                depths[i],
                vwc[i],
                bDensity[i]
            );
        }
        sw_printf("\n");
    }
#endif

    // calculate the new soil temperature for each layer
    soil_temperature_today(
        &SW_StRegValues->delta_time,
        deltaX,
        *surfaceAvg,
        sTconst,
        nRgr,
        avgLyrTempR,
        SW_StRegValues->oldavgLyrTempR,
        vwcR,
        SW_StRegValues->wpR,
        SW_StRegValues->fcR,
        SW_StRegValues->bDensityR,
        csParam1,
        csParam2,
        shParam,
        ptr_stError,
        surface_range,
        temperatureRangeR,
        SW_StRegValues->depthsR,
        year,
        doy
    );

    // question: should we ever reset delta_time to SEC_PER_DAY?

    if (*ptr_stError) {
        LogError(
            LogInfo,
            LOGWARN,
            "SOILWAT2 ERROR in soil temperature module: "
            "stability criterion failed despite reduced time step = %f "
            "seconds; "
            "soil temperature is being turned off\n",
            SW_StRegValues->delta_time
        );
    }

#ifdef SWDEBUG
    if (debug) {
        sw_printf("\nSoil temperature profile values:");
        for (i = 0; i <= nRgr + 1; i++) {
            sw_printf(
                "\nk %d oldavgLyrTempR %f avgLyrTempR %f depth %f",
                i,
                SW_StRegValues->oldavgLyrTempR[i],
                avgLyrTempR[i],
                (i * deltaX)
            );
        }
        sw_printf("\n");
    }
#endif


    // convert soil temperature of soil temperature profile 'avgLyrTempR' to
    // soil profile layers 'avgLyrTemp'
    lyrTemp_to_lyrSoil_temperature(
        SW_StRegValues->tlyrs_by_slyrs,
        nRgr,
        SW_StRegValues->depthsR,
        avgLyrTempR,
        nlyrs,
        depths,
        width,
        avgLyrTemp,
        temperatureRangeR,
        temperatureRange
    );

    // Calculate fusion pools based on soil profile layers, soil
    // freezing/thawing, and if freezing/thawing not completed during one day,
    // then adjust soil temperature
    sFadjusted_avgLyrTemp = adjust_Tsoil_by_freezing_and_thawing(
        oldavgLyrTemp,
        avgLyrTemp,
        shParam,
        nlyrs,
        vwc,
        bDensity,
        &SW_StRegValues->fusion_pool_init,
        SW_StRegValues->oldsFusionPool_actual
    );

    // update avgLyrTempR if avgLyrTemp were changed due to soil
    // freezing/thawing
    if (sFadjusted_avgLyrTemp) {
        lyrSoil_to_lyrTemp_temperature(
            nlyrs,
            depths,
            avgLyrTemp,
            sTconst,
            nRgr,
            SW_StRegValues->depthsR,
            theMaxDepth,
            avgLyrTempR
        );
    }

    // determine frozen/unfrozen status of soil layers
    set_frozen_unfrozen(nlyrs, avgLyrTemp, swc, swc_sat, width, lyrFrozen);

    // Add/subtract the interpolated range to/from average layer temperature
    for (i = 0; i < nlyrs; i++) {
        maxLyrTemperature[i] = avgLyrTemp[i] + (temperatureRange[i] / 2);
        minLyrTemperature[i] = avgLyrTemp[i] - (temperatureRange[i] / 2);
    }

#ifdef SWDEBUG
    if (debug) {
        sw_printf(
            "\navgLyrTemp %f surface; soil temperature adjusted by "
            "freeze/thaw: %i",
            *surfaceAvg,
            sFadjusted_avgLyrTemp
        );

        sw_printf("\nSoil temperature profile values:");
        for (i = 0; i <= nRgr + 1; i++) {
            sw_printf(
                "\nk %d oldavgLyrTempR %f avgLyrTempR %f depth %f",
                i,
                SW_StRegValues->oldavgLyrTempR[i],
                avgLyrTempR[i],
                (i * deltaX)
            );
        }

        sw_printf("\nSoil profile layer temperatures:");
        for (i = 0; i < nlyrs; i++) {
            sw_printf(
                "\ni %d oldTemp %f avgLyrTemp %f swc %f, swc_sat %f depth %f "
                "frozen %f",
                i,
                oldavgLyrTemp[i],
                avgLyrTemp[i],
                swc[i],
                swc_sat[i],
                depths[i],
                lyrFrozen[i]
            );
        }

        sw_printf("\n");
    }
#endif

    // updating the values of yesterdays temperature for the next time the
    // function is called...
    for (i = 0; i <= nRgr + 1; i++) {
        SW_StRegValues->oldavgLyrTempR[i] = avgLyrTempR[i];
    }

#ifdef SWDEBUG
    if (debug) {
        LogError(
            LogInfo, LOGERROR, "Stop at end of soil temperature calculations.\n"
        );
    }
#endif
}
