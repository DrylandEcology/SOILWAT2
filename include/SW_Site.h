/********************************************************/
/********************************************************/
/*  Source file: SW_Site.h
Type: header
Application: SOILWAT - soilwater dynamics simulator
Purpose: Define a structure to hold parameters that
are read in Site::read() and passed to
Layers::init(*).  There are a couple of
parameters that belong at the site level
but it also makes sense to keep the layer
parms in the same site input file.
History:

(8/28/01) -- INITIAL CODING - cwb

1-Oct-03  (cwb) Removed variables sum_evap_coeff and
             *sum_transp_coeff.  sum_evap_coeff doesn't
             do anything since it's always 1.0 (due
             to normalization) and sum_transp_coeff[]
             can easily be calculated in the only
             function where it applies: transp_weighted_avg().

(10/12/2009) - (drs) added altitude

11/02/2010	(drs) added snow parameters to SW_SITE to be read in from
siteparam.in

10/19/2010	(drs) added Bool HydraulicRedistribution flag and RealD
maxCondroot, swp50, and shapeCond parameters to the structure SW_SITE

07/20/2011	(drs) added RealD impermeability to struct SW_LAYER_INFO:
fraction of how impermeable a layer is (0=permeable, 1=impermeable) added RealD
swc_saturated to struct SW_LAYER_INFO: saturated soil water content (cm) * width

09/08/2011	(drs) moved all hydraulic redistribution parameters to
SW_VegProd.h struct VegType

09/09/2011	(drs) added RealD transp_coeff_xx for each vegetation type
(tree, shrub, grass) to struct SW_LAYER_INFO

09/09/2011	(drs) added RealD my_transp_rgn_xx for each vegetation type
(tree, shrub, grass) to struct SW_LAYER_INFO added RealD n_transp_lyrs_xx for
each vegetation type (tree, shrub, grass) to struct SW_SITE

09/15/2011	(drs)	deleted RealD albedo in struct SW_SITE and moved it to
SW_VegProd.h to make input vegetation type dependent

02/04/2012	(drs)	added Reald swc_atSWPcrit_xx for each vegetation type
(tree, shrub, grass) to struct SW_LAYER_INFO: swc at the critical soil water
potential

05/24/2012  (DLM) added variables for Soil Temperature constants to SW_SITE
struct

05/25/2012  (DLM) added variable avgLyrTemp to SW_LAYER_INFO struct to keep
track of the soil temperature for each soil layer

05/30/2012  (DLM) added stDeltaX variable for soil_temperature function to
SW_SITE struct

05/31/2012  (DLM) added use_soil_temp, stMaxDepth, stNRGR variables to
SW_SITE struct

11/06/2012	(clk)	added slope and aspect to SW_SITE struct

11/30/2010	(clk)	added the variable 'percentRunoff' which is the percent
of surface water that is lost do to surface runoff

04/16/2013	(clk)	added the variables soilMatric_density and
fractionVolBulk_gravel soilMatric_density is the soil density excluding the
gravel component fractionVolBulk_gravel is the gravel content as a fraction of
the bulk soil renamed many of the of the variables in SW_LAYER_INFO to better
reflect BULK and MATRIC values due to the renaming, also had to update the use
of these variables in the other files.

07/09/2013	(clk)	added the variables transp_coeff[SW_FORBS],
swcBulk_atSWPcrit[SW_FORBS], and my_transp_rgn[SW_FORBS] to SW_LAYER_INFO

07/09/2013	(clk)	added the variable n_transp_lyrs_forb to SW_SITE
*/
/********************************************************/
/********************************************************/
#ifndef SW_SITE_H
#define SW_SITE_H

#include "include/generic.h"        // for Bool, RealD, RealF
#include "include/SW_datastructs.h" // for SW_SITE, SW_VEGPROD, LOG_INFO
#include "include/SW_Defines.h"     // for LyrIndex

#ifdef __cplusplus
extern "C" {
#endif

#define SW_MATRIC 0
#define SW_BULK 1


/**
  @defgroup swrc_ptf Soil Water Retention Curves

  __Soil Water Retention Curves (SWRCs) -- Pedotransfer functions (PTFs)__

  __Overview__

  Historically (before v7.0.0), `SOILWAT2` utilized a hard-coded SWRC
  by Campbell 1974 (\cite Campbell1974) and estimated SWRC parameters at
  run-time from soil texture using PTFs by Cosby et al. 1984 (\cite Cosby1984).
  This behavior can be reproduced with "Campbell1974" and "Cosby1984AndOthers"
  (see input file `siteparam.in`).

  Now, users of `SOILWAT2` can choose from a range of implemented SWRCs
  (see input file `siteparam.in`);
  SWRC parameters can be estimated at run-time from soil properties by
  selecting a matching PTF (see input file `siteparam.in`) or,
  alternatively (`has_swrcp`), provide adequate SWRC parameter values
  (see input file `swrc_params.in`).
  Please note that `rSOILWAT2` may provide additional PTF functionality.


  __Approach__

  -# User selections of SWRC and PTF are read in from input file `siteparam.in`
    by `SW_SIT_read()` and, if `has_swrcp`, SWRC parameters are read from
    input file `swrc_params.in` by `SW_SWRC_read()`.

  -# `SW_SIT_init_run()`
    - if not `has_swrcp`
      - calls `check_SWRC_vs_PTF()` to check that selected SWRC and PTF are
        compatible
      - calls `SWRC_PTF_estimate_parameters()` to estimate
        SWRC parameter values from soil properties based on selected PTF
    - calls `SWRC_check_parameters()` to check that SWRC parameter values
      are resonable for the selected SWRC.

  -# `SW_SWRC_SWCtoSWP()` and `SW_SWRC_SWPtoSWC()` are used during simulation
    runs to convert between soil water content and soil water potential.

  -# These high-level "wrapper" functions hide details of any specific SWRC/PTF
    implementations and are used by SOILWAT2 simulation code.
    Thus, most of SOILWAT2 is "unaware" about the selected SWRC/PTF
    and how to interpret SWRC parameters. Instead, these "wrapper" functions
    know how to call the appropriate SWRC and/or PTF specific functions
    which implement SWRC and/or PTF specific details.


  __Steps to implement a new SWRC "XXX" and corresponding PTF "YYY"__

  -# Update #N_SWRCs and #N_PTFs

  -# Add new names to #swrc2str and #ptf2str and
     add corresponding macros of indices

  -# Update input files `siteparam.in` and `swrc_params.in`

  -# Implement new XXX/YYY-specific functions
    - `SWRC_check_parameters_for_XXX()` to validate parameter values,
      e.g., `SWRC_check_parameters_for_Campbell1974()`
    - `SWRC_PTF_YYY_for_XXX()` to estimate parameter values (if implemented),
      e.g., `SWRC_PTF_Cosby1984_for_Campbell1974()`
    - `SWRC_SWCtoSWP_XXX()` to translate moisture content to water potential,
      e.g., `SWRC_SWCtoSWP_Campbell1974()`
    - `SWRC_SWPtoSWC_XXX()` to translate water potential to moisture content,
      e.g., `SWRC_SWPtoSWC_Campbell1974()`

  -# Update "wrapper" functions that select and call XXX/YYY-specific functions
    and/or parameters
    - `check_SWRC_vs_PTF()`
    - `SWRC_PTF_estimate_parameters()` (if PTF is implemented)
    - `SWRC_check_parameters()`
    - `SWRC_SWCtoSWP()`
    - `SWRC_SWPtoSWC()`
    - `SW_swcBulk_minimum()`
    - `SW_swcBulk_saturated()`

  -# Expand existing unit tests and add new unit tests
    to utilize new XXX/YYY functions
*/

#define N_SWRCs 3 /**< Number of SWRCs implemented by SOILWAT2 */
#define N_PTFs 2  /**< Number of PTFs implemented by SOILWAT2 */

// Indices of #swrc2str (for code readability)
#define sw_Campbell1974 0
#define sw_vanGenuchten1980 1
#define sw_FXW 2

// Indices of #swrc2str (for code readability)
#define sw_Cosby1984AndOthers 0
#define sw_Cosby1984 1

/* =================================================== */
/*            Externed Global Variables                */
/* --------------------------------------------------- */
extern const char *const swrc2str[];
extern const char *const ptf2str[];


/* =================================================== */
/*             Global Function Declarations            */
/* --------------------------------------------------- */

unsigned int encode_str2swrc(char *swrc_name, LOG_INFO *LogInfo);

unsigned int encode_str2ptf(char *ptf_name);

void SWRC_PTF_estimate_parameters(
    unsigned int ptf_type,
    double *swrcp,
    double sand,
    double clay,
    double gravel,
    double bdensity,
    LOG_INFO *LogInfo
);

void SWRC_PTF_Cosby1984_for_Campbell1974(
    double *swrcp, double sand, double clay
);


Bool check_SWRC_vs_PTF(char *swrc_name, char *ptf_name);

Bool SWRC_check_parameters(
    unsigned int swrc_type, double *swrcp, LOG_INFO *LogInfo
);

Bool SWRC_check_parameters_for_Campbell1974(double *swrcp, LOG_INFO *LogInfo);

Bool SWRC_check_parameters_for_vanGenuchten1980(
    double *swrcp, LOG_INFO *LogInfo
);

Bool SWRC_check_parameters_for_FXW(double *swrcp, LOG_INFO *LogInfo);

double SW_swcBulk_saturated(
    unsigned int swrc_type,
    double *swrcp,
    double gravel,
    double width,
    unsigned int ptf_type,
    double sand,
    double clay,
    LOG_INFO *LogInfo
);

double SW_swcBulk_minimum(
    unsigned int swrc_type,
    double *swrcp,
    double gravel,
    double width,
    unsigned int ptf_type,
    double ui_sm_min,
    double sand,
    double clay,
    double swcBulk_sat,
    RealD SWCMinVal,
    LOG_INFO *LogInfo
);

void PTF_Saxton2006(
    double *theta_sat, double sand, double clay, LOG_INFO *LogInfo
);

void PTF_RawlsBrakensiek1985(
    double *theta_min,
    double sand,
    double clay,
    double porosity,
    LOG_INFO *LogInfo
);


RealD calculate_soilBulkDensity(RealD matricDensity, RealD fractionGravel);

RealD calculate_soilMatricDensity(
    RealD bulkDensity, RealD fractionGravel, LOG_INFO *LogInfo
);

LyrIndex nlayers_bsevap(RealD *evap_coeff, LyrIndex n_layers);

void nlayers_vegroots(
    LyrIndex n_layers,
    LyrIndex n_transp_lyrs[],
    RealD transp_coeff[][MAX_LAYERS]
);

void SW_SIT_construct(SW_SITE *SW_Site);

void SW_SIT_init_counts(SW_SITE *SW_Site);

void SW_SIT_read(
    SW_SITE *SW_Site, char *InFiles[], SW_CARBON *SW_Carbon, LOG_INFO *LogInfo
);

void SW_SIT_init_run(
    SW_VEGPROD *SW_VegProd, SW_SITE *SW_Site, LOG_INFO *LogInfo
);

void _echo_inputs(SW_SITE *SW_Site, SW_MODEL *SW_Model);


/* these used to be in Layers */
void SW_LYR_read(SW_SITE *SW_Site, char *InFiles[], LOG_INFO *LogInfo);

void SW_SWRC_read(SW_SITE *SW_Site, char *InFiles[], LOG_INFO *LogInfo);

void add_deepdrain_layer(SW_SITE *SW_Site);

void set_soillayers(
    SW_VEGPROD *SW_VegProd,
    SW_SITE *SW_Site,
    LyrIndex nlyrs,
    RealF *dmax,
    RealF *bd,
    RealF *f_gravel,
    RealF *evco,
    RealF *trco_grass,
    RealF *trco_shrub,
    RealF *trco_tree,
    RealF *trco_forb,
    RealF *psand,
    RealF *pclay,
    RealF *imperm,
    RealF *soiltemp,
    int nRegions,
    RealD *regionLowerBounds,
    LOG_INFO *LogInfo
);

void derive_soilRegions(
    SW_SITE *SW_Site, int nRegions, RealD *regionLowerBounds, LOG_INFO *LogInfo
);


#ifdef __cplusplus
}
#endif

#endif
