/********************************************************/
/********************************************************/
/*	Source file: Veg_Prod.c
Type: module
Application: SOILWAT - soilwater dynamics simulator
Purpose: Read / write and otherwise manage the model's
vegetation production parameter information.
History:
(8/28/01) -- INITIAL CODING - cwb
11/16/2010	(drs) added LAIforest, biofoliage_for, lai_conv_for, TypeGrassOrShrub, TypeForest to SW_VEGPROD
lai_live/biolive/total_agb include now LAIforest, respectively biofoliage_for
updated SW_VPD_read(), SW_VPD_init(), and _echo_inits()
increased length of char outstr[1000] to outstr[1500] because of increased echo
02/22/2011	(drs) added scan for litter_for to SW_VPD_read()
02/22/2011	(drs) added litter_for to SW_VegProd.litter and to SW_VegProd.tot_agb
02/22/2011	(drs) if TypeGrassOrShrub is turned off, then its biomass, litter, etc. values are set to 0
08/22/2011	(drs) use variable veg_height [MAX_MONTHS] from SW_VEGPROD instead of static canopy_ht
09/08/2011	(drs) adapted SW_VPD_read() and SW_VPD_init() to reflect that now each vegetation type has own elements
09/08/2011	(drs) added input in SW_VPD_read() of tanfunc_t tr_shade_effects, and RealD shade_scale and shade_deadmax (they were previously hidden as constants in code in SW_Flow_lib.h)
09/08/2011	(drs) moved all input of hydraulic redistribution variables from SW_Site.c to SW_VPD_read() for each vegetation type
09/08/2011	(drs) added input in SW_VPD_read() of RealD veg_intPPT_a, veg_intPPT_b, veg_intPPT_c, veg_intPPT_d (they were previously hidden as constants in code in SW_Flow_lib.h)
09/09/2011	(drs) added input in SW_VPD_read() of RealD EsTpartitioning_param (it were previously hidden as constant in code in SW_Flow_lib.h)
09/09/2011	(drs) added input in SW_VPD_read() of RealD Es_param_limit (it was previously hidden as constant in code in SW_Flow_lib.h)
09/13/2011	(drs) added input in SW_VPD_read() of RealD litt_intPPT_a, litt_intPPT_b, litt_intPPT_c, litt_intPPT_d (they were previously hidden as constants in code in SW_Flow_lib.h)
09/13/2011	(drs) added input in SW_VPD_read() of RealD canopy_height_constant and updated SW_VPD_init() (as option: if > 0 then constant canopy height (cm) and overriding cnpy-tangens=f(biomass))
09/15/2011	(drs) added input in SW_VPD_read() of RealD albedo
09/26/2011	(drs)	added calls to Times.c:interpolate_monthlyValues() in SW_VPD_init() for each monthly input variable; replaced monthly loop with a daily loop for additional daily variables; adjusted _echo_inits()
10/17/2011	(drs)	in SW_VPD_init(): v->tree.total_agb_daily[doy] = v->tree.litter_daily[doy] + v->tree.biolive_daily[doy] instead of = v->tree.litter_daily[doy] + v->tree.biomass_daily[doy] to adjust for better scaling of potential bare-soil evaporation
02/04/2012	(drs) 	added input in SW_VPD_read() of RealD SWPcrit
01/29/2013	(clk)	changed the read in to account for the extra fractional component in total vegetation, bare ground
added the variable RealF help_bareGround as a place holder for the sscanf call.
01/31/2013	(clk)	changed the read in to account for the albedo for bare ground, storing the input in bare_cov.albedo
changed _echo_inits() to now display the bare ground components in logfile.log
06/27/2013	(drs)	closed open files if LogError() with LOGFATAL is called in SW_VPD_read()
07/09/2013	(clk)	added initialization of all the values of the new vegtype variable forb and forb.cov.fCover
*/
/********************************************************/
/********************************************************/

/* =================================================== */
/* =================================================== */
/*                INCLUDES / DEFINES                   */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "generic.h"
#include "filefuncs.h"
#include "SW_Defines.h"
#include "SW_Files.h"
#include "SW_Times.h"
#include "SW_VegProd.h"
#include "SW_Model.h"

/* =================================================== */
/*                  Global Variables                   */
/* --------------------------------------------------- */
extern Bool EchoInits;
extern SW_MODEL SW_Model;
#ifdef RSOILWAT
extern Bool collectInData;
#endif
SW_VEGPROD SW_VegProd; /* declared here, externed elsewhere */

/* =================================================== */
/*                Module-Level Variables               */
/* --------------------------------------------------- */
static char *MyFileName;

static RealD lai_standing; /*standing crop lai (g/m**2)     */

/* =================================================== */
/* =================================================== */
/*             Private Function Definitions            */
/* --------------------------------------------------- */


/* =================================================== */
/* =================================================== */
/*             Public Function Definitions             */
/* --------------------------------------------------- */

void SW_VPD_read(void) {
	/* =================================================== */
	SW_VEGPROD *v = &SW_VegProd;
	FILE *f;
	TimeInt mon = Jan;
	int x, lineno = 0;
	const int line_help = 33;
	RealF help_grass, help_shrub, help_tree, help_forb, help_bareGround, litt, biom, pctl, laic, co2_coeff_grass, co2_coeff_shrub, co2_coeff_tree, co2_coeff_forb;
	RealD fraction_sum = 0.;

	MyFileName = SW_F_name(eVegProd);
	f = OpenFile(MyFileName, "r");

	while (GetALine(f, inbuf)) {
		if (lineno++ < line_help) {
			switch (lineno) {
			/* fractions of vegetation types */
			case 1:
				x = sscanf(inbuf, "%f %f %f %f %f", &help_grass, &help_shrub, &help_tree, &help_forb, &help_bareGround);
				if (x < 5) {
					sprintf(errstr, "ERROR: invalid record in vegetation type components in %s\n", MyFileName);
					CloseFile(&f);
					LogError(logfp, LOGFATAL, errstr);
				}
				v->grass.cov.fCover = help_grass;
				v->shrub.cov.fCover = help_shrub;
				v->tree.cov.fCover = help_tree;
				v->forb.cov.fCover = help_forb;
				v->bare_cov.fCover = help_bareGround;
				break;

			/* albedo */
			case 2:
				x = sscanf(inbuf, "%f %f %f %f %f", &help_grass, &help_shrub, &help_tree, &help_forb, &help_bareGround);
				if (x < 5) {
					sprintf(errstr, "ERROR: invalid record in albedo values in %s\n", MyFileName);
					CloseFile(&f);
					LogError(logfp, LOGFATAL, errstr);
				}
				v->grass.cov.albedo = help_grass;
				v->shrub.cov.albedo = help_shrub;
				v->tree.cov.albedo = help_tree;
				v->forb.cov.albedo = help_forb;
				v->bare_cov.albedo = help_bareGround;
				break;

			/* LAI converter for % cover */
			case 3:
				x = sscanf(inbuf, "%f %f %f %f", &help_grass, &help_shrub, &help_tree, &help_forb);
				if (x < 4) {
					sprintf(errstr, "ERROR: invalid record in percent cover converting in %s\n", MyFileName);
					CloseFile(&f);
					LogError(logfp, LOGFATAL, errstr);
				}
				v->grass.conv_stcr = help_grass;
				v->shrub.conv_stcr = help_shrub;
				v->tree.conv_stcr = help_tree;
				v->forb.conv_stcr = help_forb;
				break;

			/* canopy height */
			case 4:
				x = sscanf(inbuf, "%f %f %f %f", &help_grass, &help_shrub, &help_tree, &help_forb);
				if (x < 4) {
					sprintf(errstr, "ERROR: invalid record in canopy xinflec in %s\n", MyFileName);
					CloseFile(&f);
					LogError(logfp, LOGFATAL, errstr);
				}
				v->grass.cnpy.xinflec = help_grass;
				v->shrub.cnpy.xinflec = help_shrub;
				v->tree.cnpy.xinflec = help_tree;
				v->forb.cnpy.xinflec = help_forb;
				break;
			case 5:
				x = sscanf(inbuf, "%f %f %f %f", &help_grass, &help_shrub, &help_tree, &help_forb);
				if (x < 4) {
					sprintf(errstr, "ERROR: invalid record in canopy yinflec in %s\n", MyFileName);
					CloseFile(&f);
					LogError(logfp, LOGFATAL, errstr);
				}
				v->grass.cnpy.yinflec = help_grass;
				v->shrub.cnpy.yinflec = help_shrub;
				v->tree.cnpy.yinflec = help_tree;
				v->forb.cnpy.yinflec = help_forb;
				break;
			case 6:
				x = sscanf(inbuf, "%f %f %f %f", &help_grass, &help_shrub, &help_tree, &help_forb);
				if (x < 4) {
					sprintf(errstr, "ERROR: invalid record in canopy range in %s\n", MyFileName);
					CloseFile(&f);
					LogError(logfp, LOGFATAL, errstr);
				}
				v->grass.cnpy.range = help_grass;
				v->shrub.cnpy.range = help_shrub;
				v->tree.cnpy.range = help_tree;
				v->forb.cnpy.range = help_forb;
				break;
			case 7:
				x = sscanf(inbuf, "%f %f %f %f", &help_grass, &help_shrub, &help_tree, &help_forb);
				if (x < 4) {
					sprintf(errstr, "ERROR: invalid record in canopy slope in %s\n", MyFileName);
					CloseFile(&f);
					LogError(logfp, LOGFATAL, errstr);
				}
				v->grass.cnpy.slope = help_grass;
				v->shrub.cnpy.slope = help_shrub;
				v->tree.cnpy.slope = help_tree;
				v->forb.cnpy.slope = help_forb;
				break;
			case 8:
				x = sscanf(inbuf, "%f %f %f %f", &help_grass, &help_shrub, &help_tree, &help_forb);
				if (x < 4) {
					sprintf(errstr, "ERROR: invalid record in canopy height constant option in %s\n", MyFileName);
					CloseFile(&f);
					LogError(logfp, LOGFATAL, errstr);
				}
				v->grass.canopy_height_constant = help_grass;
				v->shrub.canopy_height_constant = help_shrub;
				v->tree.canopy_height_constant = help_tree;
				v->forb.canopy_height_constant = help_forb;
				break;

			/* vegetation interception parameters */
			case 9:
				x = sscanf(inbuf, "%f %f %f %f", &help_grass, &help_shrub, &help_tree, &help_forb);
				if (x < 4) {
					sprintf(errstr, "ERROR: invalid record in interception parameter a in %s\n", MyFileName);
					CloseFile(&f);
					LogError(logfp, LOGFATAL, errstr);
				}
				v->grass.veg_intPPT_a = help_grass;
				v->shrub.veg_intPPT_a = help_shrub;
				v->tree.veg_intPPT_a = help_tree;
				v->forb.veg_intPPT_a = help_forb;
				break;
			case 10:
				x = sscanf(inbuf, "%f %f %f %f", &help_grass, &help_shrub, &help_tree, &help_forb);
				if (x < 4) {
					sprintf(errstr, "ERROR: invalid record in interception parameter b in %s\n", MyFileName);
					CloseFile(&f);
					LogError(logfp, LOGFATAL, errstr);
				}
				v->grass.veg_intPPT_b = help_grass;
				v->shrub.veg_intPPT_b = help_shrub;
				v->tree.veg_intPPT_b = help_tree;
				v->forb.veg_intPPT_b = help_forb;
				break;
			case 11:
				x = sscanf(inbuf, "%f %f %f %f", &help_grass, &help_shrub, &help_tree, &help_forb);
				if (x < 4) {
					sprintf(errstr, "ERROR: invalid record in interception parameter c in %s\n", MyFileName);
					CloseFile(&f);
					LogError(logfp, LOGFATAL, errstr);
				}
				v->grass.veg_intPPT_c = help_grass;
				v->shrub.veg_intPPT_c = help_shrub;
				v->tree.veg_intPPT_c = help_tree;
				v->forb.veg_intPPT_c = help_forb;
				break;
			case 12:
				x = sscanf(inbuf, "%f %f %f %f", &help_grass, &help_shrub, &help_tree, &help_forb);
				if (x < 4) {
					sprintf(errstr, "ERROR: invalid record in interception parameter d in %s\n", MyFileName);
					CloseFile(&f);
					LogError(logfp, LOGFATAL, errstr);
				}
				v->grass.veg_intPPT_d = help_grass;
				v->shrub.veg_intPPT_d = help_shrub;
				v->tree.veg_intPPT_d = help_tree;
				v->forb.veg_intPPT_d = help_forb;
				break;

			/* litter interception parameters */
			case 13:
				x = sscanf(inbuf, "%f %f %f %f", &help_grass, &help_shrub, &help_tree, &help_forb);
				if (x < 4) {
					sprintf(errstr, "ERROR: invalid record in litter interception parameter a in %s\n", MyFileName);
					CloseFile(&f);
					LogError(logfp, LOGFATAL, errstr);
				}
				v->grass.litt_intPPT_a = help_grass;
				v->shrub.litt_intPPT_a = help_shrub;
				v->tree.litt_intPPT_a = help_tree;
				v->forb.litt_intPPT_a = help_forb;
				break;
			case 14:
				x = sscanf(inbuf, "%f %f %f %f", &help_grass, &help_shrub, &help_tree, &help_forb);
				if (x < 4) {
					sprintf(errstr, "ERROR: invalid record in litter interception parameter b in %s\n", MyFileName);
					CloseFile(&f);
					LogError(logfp, LOGFATAL, errstr);
				}
				v->grass.litt_intPPT_b = help_grass;
				v->shrub.litt_intPPT_b = help_shrub;
				v->tree.litt_intPPT_b = help_tree;
				v->forb.litt_intPPT_b = help_forb;
				break;
			case 15:
				x = sscanf(inbuf, "%f %f %f %f", &help_grass, &help_shrub, &help_tree, &help_forb);
				if (x < 4) {
					sprintf(errstr, "ERROR: invalid record in litter interception parameter c in %s\n", MyFileName);
					CloseFile(&f);
					LogError(logfp, LOGFATAL, errstr);
				}
				v->grass.litt_intPPT_c = help_grass;
				v->shrub.litt_intPPT_c = help_shrub;
				v->tree.litt_intPPT_c = help_tree;
				v->forb.litt_intPPT_c = help_forb;
				break;
			case 16:
				x = sscanf(inbuf, "%f %f %f %f", &help_grass, &help_shrub, &help_tree, &help_forb);
				if (x < 4) {
					sprintf(errstr, "ERROR: invalid record in litter interception parameter d in %s\n", MyFileName);
					CloseFile(&f);
					LogError(logfp, LOGFATAL, errstr);
				}
				v->grass.litt_intPPT_d = help_grass;
				v->shrub.litt_intPPT_d = help_shrub;
				v->tree.litt_intPPT_d = help_tree;
				v->forb.litt_intPPT_d = help_forb;
				break;

			/* parameter for partitioning of bare-soil evaporation and transpiration */
			case 17:
				x = sscanf(inbuf, "%f %f %f %f", &help_grass, &help_shrub, &help_tree, &help_forb);
				if (x < 4) {
					sprintf(errstr, "ERROR: invalid record in parameter for partitioning of bare-soil evaporation and transpiration in %s\n", MyFileName);
					CloseFile(&f);
					LogError(logfp, LOGFATAL, errstr);
				}
				v->grass.EsTpartitioning_param = help_grass;
				v->shrub.EsTpartitioning_param = help_shrub;
				v->tree.EsTpartitioning_param = help_tree;
				v->forb.EsTpartitioning_param = help_forb;
				break;

			/* Parameter for scaling and limiting bare soil evaporation rate */
			case 18:
				x = sscanf(inbuf, "%f %f %f %f", &help_grass, &help_shrub, &help_tree, &help_forb);
				if (x < 4) {
					sprintf(errstr, "ERROR: invalid record in parameter for Parameter for scaling and limiting bare soil evaporation rate in %s\n", MyFileName);
					CloseFile(&f);
					LogError(logfp, LOGFATAL, errstr);
				}
				v->grass.Es_param_limit = help_grass;
				v->shrub.Es_param_limit = help_shrub;
				v->tree.Es_param_limit = help_tree;
				v->forb.Es_param_limit = help_forb;
				break;

			/* shade effects */
			case 19:
				x = sscanf(inbuf, "%f %f %f %f", &help_grass, &help_shrub, &help_tree, &help_forb);
				if (x < 4) {
					sprintf(errstr, "ERROR: invalid record in shade scale in %s\n", MyFileName);
					CloseFile(&f);
					LogError(logfp, LOGFATAL, errstr);
				}
				v->grass.shade_scale = help_grass;
				v->shrub.shade_scale = help_shrub;
				v->tree.shade_scale = help_tree;
				v->forb.shade_scale = help_forb;
				break;
			case 20:
				x = sscanf(inbuf, "%f %f %f %f", &help_grass, &help_shrub, &help_tree, &help_forb);
				if (x < 4) {
					sprintf(errstr, "ERROR: invalid record in shade max dead biomass in %s\n", MyFileName);
					CloseFile(&f);
					LogError(logfp, LOGFATAL, errstr);
				}
				v->grass.shade_deadmax = help_grass;
				v->shrub.shade_deadmax = help_shrub;
				v->tree.shade_deadmax = help_tree;
				v->forb.shade_deadmax = help_forb;
				break;
			case 21:
				x = sscanf(inbuf, "%f %f %f %f", &help_grass, &help_shrub, &help_tree, &help_forb);
				if (x < 4) {
					sprintf(errstr, "ERROR: invalid record in shade xinflec in %s\n", MyFileName);
					CloseFile(&f);
					LogError(logfp, LOGFATAL, errstr);
				}
				v->grass.tr_shade_effects.xinflec = help_grass;
				v->shrub.tr_shade_effects.xinflec = help_shrub;
				v->tree.tr_shade_effects.xinflec = help_tree;
				v->forb.tr_shade_effects.xinflec = help_forb;
				break;
			case 22:
				x = sscanf(inbuf, "%f %f %f %f", &help_grass, &help_shrub, &help_tree, &help_forb);
				if (x < 4) {
					sprintf(errstr, "ERROR: invalid record in shade yinflec in %s\n", MyFileName);
					CloseFile(&f);
					LogError(logfp, LOGFATAL, errstr);
				}
				v->grass.tr_shade_effects.yinflec = help_grass;
				v->shrub.tr_shade_effects.yinflec = help_shrub;
				v->tree.tr_shade_effects.yinflec = help_tree;
				v->forb.tr_shade_effects.yinflec = help_forb;
				break;
			case 23:
				x = sscanf(inbuf, "%f %f %f %f", &help_grass, &help_shrub, &help_tree, &help_forb);
				if (x < 4) {
					sprintf(errstr, "ERROR: invalid record in shade range in %s\n", MyFileName);
					CloseFile(&f);
					LogError(logfp, LOGFATAL, errstr);
				}
				v->grass.tr_shade_effects.range = help_grass;
				v->shrub.tr_shade_effects.range = help_shrub;
				v->tree.tr_shade_effects.range = help_tree;
				v->forb.tr_shade_effects.range = help_forb;
				break;
			case 24:
				x = sscanf(inbuf, "%f %f %f %f", &help_grass, &help_shrub, &help_tree, &help_forb);
				if (x < 4) {
					sprintf(errstr, "ERROR: invalid record in shade slope in %s\n", MyFileName);
					CloseFile(&f);
					LogError(logfp, LOGFATAL, errstr);
				}
				v->grass.tr_shade_effects.slope = help_grass;
				v->shrub.tr_shade_effects.slope = help_shrub;
				v->tree.tr_shade_effects.slope = help_tree;
				v->forb.tr_shade_effects.slope = help_forb;
				break;

			/* Hydraulic redistribution */
			case 25:
				x = sscanf(inbuf, "%f %f %f %f", &help_grass, &help_shrub, &help_tree, &help_forb);
				if (x < 4) {
					sprintf(errstr, "ERROR: invalid record in hydraulic redistribution: flag in %s\n", MyFileName);
					CloseFile(&f);
					LogError(logfp, LOGFATAL, errstr);
				}
				v->grass.flagHydraulicRedistribution = (Bool) help_grass;
				v->shrub.flagHydraulicRedistribution = (Bool) help_shrub;
				v->tree.flagHydraulicRedistribution = (Bool) help_tree;
				v->forb.flagHydraulicRedistribution = (Bool) help_forb;
				break;
			case 26:
				x = sscanf(inbuf, "%f %f %f %f", &help_grass, &help_shrub, &help_tree, &help_forb);
				if (x < 4) {
					sprintf(errstr, "ERROR: invalid record in hydraulic redistribution: maxCondroot in %s\n", MyFileName);
					CloseFile(&f);
					LogError(logfp, LOGFATAL, errstr);
				}
				v->grass.maxCondroot = help_grass;
				v->shrub.maxCondroot = help_shrub;
				v->tree.maxCondroot = help_tree;
				v->forb.maxCondroot = help_forb;
				break;
			case 27:
				x = sscanf(inbuf, "%f %f %f %f", &help_grass, &help_shrub, &help_tree, &help_forb);
				if (x < 4) {
					sprintf(errstr, "ERROR: invalid record in hydraulic redistribution: swpMatric50 in %s\n", MyFileName);
					CloseFile(&f);
					LogError(logfp, LOGFATAL, errstr);
				}
				v->grass.swpMatric50 = help_grass;
				v->shrub.swpMatric50 = help_shrub;
				v->tree.swpMatric50 = help_tree;
				v->forb.swpMatric50 = help_forb;
				break;
			case 28:
				x = sscanf(inbuf, "%f %f %f %f", &help_grass, &help_shrub, &help_tree, &help_forb);
				if (x < 4) {
					sprintf(errstr, "ERROR: invalid record in hydraulic redistribution: shapeCond in %s\n", MyFileName);
					CloseFile(&f);
					LogError(logfp, LOGFATAL, errstr);
				}
				v->grass.shapeCond = help_grass;
				v->shrub.shapeCond = help_shrub;
				v->tree.shapeCond = help_tree;
				v->forb.shapeCond = help_forb;
				break;

			/* Critical soil water potential */
			case 29:
				x = sscanf(inbuf, "%f %f %f %f", &help_grass, &help_shrub, &help_tree, &help_forb);
				if (x < 4) {
					sprintf(errstr, "ERROR: invalid record in critical soil water potentials: flag in %s\n", MyFileName);
					CloseFile(&f);
					LogError(logfp, LOGFATAL, errstr);
				}
				v->grass.SWPcrit = -10. * help_grass;
				v->shrub.SWPcrit = -10. * help_shrub;
				v->tree.SWPcrit = -10. * help_tree;
				v->forb.SWPcrit = -10. * help_forb;
				break;

			/* CO2 Biomass Power Equation */
			// Coefficient 1
			case 30:
				x = sscanf(inbuf, "%f %f %f %f", &co2_coeff_grass, &co2_coeff_shrub, &co2_coeff_tree, &co2_coeff_forb);
				if (x < 4) {
					sprintf(errstr, "ERROR: Not enough arguments for CO2 Biomass Coefficient 1 in %s\n", MyFileName);
					CloseFile(&f);
					LogError(logfp, LOGFATAL, errstr);
				}
				v->grass.co2_bio_coeff1 = co2_coeff_grass;
				v->shrub.co2_bio_coeff1 = co2_coeff_shrub;
				v->tree.co2_bio_coeff1 = co2_coeff_tree;
				v->forb.co2_bio_coeff1 = co2_coeff_forb;
				break;
			// Coefficient 2
			case 31:
				x = sscanf(inbuf, "%f %f %f %f", &co2_coeff_grass, &co2_coeff_shrub, &co2_coeff_tree, &co2_coeff_forb);
				if (x < 4) {
					sprintf(errstr, "ERROR: Not enough arguments for CO2 Biomass Coefficient 2 in %s\n", MyFileName);
					CloseFile(&f);
					LogError(logfp, LOGFATAL, errstr);
				}
				v->grass.co2_bio_coeff2 = co2_coeff_grass;
				v->shrub.co2_bio_coeff2 = co2_coeff_shrub;
				v->tree.co2_bio_coeff2 = co2_coeff_tree;
				v->forb.co2_bio_coeff2 = co2_coeff_forb;
				break;

			/* CO2 WUE Power Equation */
			// Coefficient 1
			case 32:
				x = sscanf(inbuf, "%f %f %f %f", &co2_coeff_grass, &co2_coeff_shrub, &co2_coeff_tree, &co2_coeff_forb);
				if (x < 4) {
					sprintf(errstr, "ERROR: Not enough arguments for CO2 WUE Coefficient 1 in %s\n", MyFileName);
					CloseFile(&f);
					LogError(logfp, LOGFATAL, errstr);
				}
				v->grass.co2_wue_coeff1 = co2_coeff_grass;
				v->shrub.co2_wue_coeff1 = co2_coeff_shrub;
				v->tree.co2_wue_coeff1 = co2_coeff_tree;
				v->forb.co2_wue_coeff1 = co2_coeff_forb;
				break;
			// Coefficient 2
			case 33:
				x = sscanf(inbuf, "%f %f %f %f", &co2_coeff_grass, &co2_coeff_shrub, &co2_coeff_tree, &co2_coeff_forb);
				if (x < 4) {
					sprintf(errstr, "ERROR: Not enough arguments for CO2 WUE Coefficient 2 in %s\n", MyFileName);
					CloseFile(&f);
					LogError(logfp, LOGFATAL, errstr);
				}
				v->grass.co2_wue_coeff2 = co2_coeff_grass;
				v->shrub.co2_wue_coeff2 = co2_coeff_shrub;
				v->tree.co2_wue_coeff2 = co2_coeff_tree;
				v->forb.co2_wue_coeff2 = co2_coeff_forb;
				break;

			default:
				break;
			}
		}
		else {
			if (lineno == line_help + 1 || lineno == line_help + 1 + 12 || lineno == line_help + 1 + 12 * 2 || lineno == line_help + 1 + 12 * 3)
				mon = Jan;

			x = sscanf(inbuf, "%f %f %f %f", &litt, &biom, &pctl, &laic);
			if (x < 4) {
				sprintf(errstr, "ERROR: invalid record %d in %s\n", mon + 1, MyFileName);
				CloseFile(&f);
				LogError(logfp, LOGFATAL, errstr);
			}
			if (lineno > line_help + 12 * 3 && lineno <= line_help + 12 * 4) {
				v->forb.litter[mon] = litt;
				v->forb.biomass[mon] = biom;
				v->forb.pct_live[mon] = pctl;
				v->forb.lai_conv[mon] = laic;
			}
			else if (lineno > line_help + 12 * 2 && lineno <= line_help + 12 * 3) {
				v->tree.litter[mon] = litt;
				v->tree.biomass[mon] = biom;
				v->tree.pct_live[mon] = pctl;
				v->tree.lai_conv[mon] = laic;
			}
			else if (lineno > line_help + 12 && lineno <= line_help + 12 * 2) {
				v->shrub.litter[mon] = litt;
				v->shrub.biomass[mon] = biom;
				v->shrub.pct_live[mon] = pctl;
				v->shrub.lai_conv[mon] = laic;
			}
			else if (lineno > line_help && lineno <= line_help + 12) {
				v->grass.litter[mon] = litt;
				v->grass.biomass[mon] = biom;
				v->grass.pct_live[mon] = pctl;
				v->grass.lai_conv[mon] = laic;
			}

			mon++;

		}
	}

	if (mon < Dec) {
		sprintf(errstr, "No Veg Production values after month %d", mon + 1);
		LogError(logfp, LOGWARN, errstr);
	}

	fraction_sum = v->grass.cov.fCover + v->shrub.cov.fCover + v->tree.cov.fCover + v->forb.cov.fCover + v->bare_cov.fCover;
	if (!EQ(fraction_sum, 1.0)) {
		LogError(logfp, LOGWARN, "%s : Fractions of vegetation components were normalized, "
			"sum of fractions (%5.4f) != 1.0.\nNew coefficients are:", MyFileName, fraction_sum);
		v->grass.cov.fCover /= fraction_sum;
		v->shrub.cov.fCover /= fraction_sum;
		v->tree.cov.fCover /= fraction_sum;
		v->bare_cov.fCover /= fraction_sum;
		v->forb.cov.fCover /= fraction_sum;
		LogError(logfp, LOGWARN, "  Grassland fraction : %5.4f", v->grass.cov.fCover);
		LogError(logfp, LOGWARN, "  Shrubland fraction : %5.4f", v->shrub.cov.fCover);
		LogError(logfp, LOGWARN, "  Forest/tree fraction : %5.4f", v->tree.cov.fCover);
		LogError(logfp, LOGWARN, "  FORB fraction : %5.4f", v->forb.cov.fCover);
		LogError(logfp, LOGWARN, "  Bare Ground fraction : %5.4f", v->bare_cov.fCover);
	}

	CloseFile(&f);
#ifdef RSOILWAT
	if (!collectInData)
#endif

	SW_VPD_init();

	if (EchoInits)
		_echo_VegProd();
}


void SW_VPD_construct(void) {
	/* =================================================== */

	memset(&SW_VegProd, 0, sizeof(SW_VegProd));


  SW_VEGPROD *v = &SW_VegProd;
  int year;

  for (year = 0; year < MAX_NYEAR; year++)
  {
    v->grass.co2_multipliers[BIO_INDEX][year] = 1.;
    v->grass.co2_multipliers[WUE_INDEX][year] = 1.;
    v->shrub.co2_multipliers[BIO_INDEX][year] = 1.;
    v->shrub.co2_multipliers[WUE_INDEX][year] = 1.;
    v->tree.co2_multipliers[BIO_INDEX][year] = 1.;
    v->tree.co2_multipliers[WUE_INDEX][year] = 1.;
    v->forb.co2_multipliers[BIO_INDEX][year] = 1.;
    v->forb.co2_multipliers[WUE_INDEX][year] = 1.;
  }

}


/**
 * @brief Applies CO2 effects to supplied biomass data.
 *
 * Two biomass parameters are needed so that we do not have a compound effect
 * on the biomass.
 *
 * @param new_biomass  The resulting biomass after applying the multiplier.
 * @param biomass      The biomass to be modified (representing the value under reference
 *                     conditions (i.e., 360 ppm CO2, currently).
 * @param multiplier   The biomass multiplier for this PFT.
 * @note Does not return a value, @p new_biomass is directly modified.
 */
void apply_biomassCO2effect(double new_biomass[], double biomass[], double multiplier) {
  int i;
  for (i = 0; i < 12; i++) new_biomass[i] = (biomass[i] * multiplier);
}


void SW_VPD_init(void) {
	/* ================================================== */
	/* set up vegetation parameters to be used in
	* the "watrflow" subroutine.
	*
	* History:
	*     Originally included in the FORTRAN model.
	*
	*     20-Oct-03 (cwb) removed the calculation of
	*        lai_corr and changed the lai_conv value of 80
	*        as found in the prod.in file.  The conversion
	*        factor is now simply a divisor rather than
	*        an equation.  Removed the following code:
	lai_corr = v->lai_conv[m] * (1. - pstem) + aconst * pstem;
	lai_standing    = v->biomass[m] / lai_corr;
	where pstem = 0.3,
	aconst = 464.0,
	conv_stcr = 3.0;
	*
	*
	*/

	SW_VEGPROD *v = &SW_VegProd; /* convenience */
	TimeInt doy; /* base1 */

	// Grab the real year so we can access CO2 data

	if (GT(v->grass.cov.fCover, 0.)) {
		// CO2
		apply_biomassCO2effect(v->grass.CO2_biomass, v->grass.biomass, v->grass.co2_multipliers[BIO_INDEX][SW_Model.simyear]);

		// Interpolation
		interpolate_monthlyValues(v->grass.litter, v->grass.litter_daily);
		interpolate_monthlyValues(v->grass.CO2_biomass, v->grass.biomass_daily);
		interpolate_monthlyValues(v->grass.pct_live, v->grass.pct_live_daily);
		interpolate_monthlyValues(v->grass.lai_conv, v->grass.lai_conv_daily);
	}

	if (GT(v->shrub.cov.fCover, 0.)) {
		// CO2
		apply_biomassCO2effect(v->shrub.CO2_biomass, v->shrub.biomass, v->shrub.co2_multipliers[BIO_INDEX][SW_Model.simyear]);

		// Interpolation
		interpolate_monthlyValues(v->shrub.litter, v->shrub.litter_daily);
		interpolate_monthlyValues(v->shrub.CO2_biomass, v->shrub.biomass_daily);
		interpolate_monthlyValues(v->shrub.pct_live, v->shrub.pct_live_daily);
		interpolate_monthlyValues(v->shrub.lai_conv, v->shrub.lai_conv_daily);
	}

	if (GT(v->tree.cov.fCover, 0.)) {
		// CO2
		apply_biomassCO2effect(v->tree.CO2_pct_live, v->tree.pct_live, v->tree.co2_multipliers[BIO_INDEX][SW_Model.simyear]);

		// Interpolation
		interpolate_monthlyValues(v->tree.litter, v->tree.litter_daily);
		interpolate_monthlyValues(v->tree.biomass, v->tree.biomass_daily);
		interpolate_monthlyValues(v->tree.CO2_pct_live, v->tree.pct_live_daily);
		interpolate_monthlyValues(v->tree.lai_conv, v->tree.lai_conv_daily);
	}

	if (GT(v->forb.cov.fCover, 0.)) {
		// CO2
		apply_biomassCO2effect(v->forb.CO2_biomass, v->forb.biomass, v->forb.co2_multipliers[BIO_INDEX][SW_Model.simyear]);

		// Interpolation
		interpolate_monthlyValues(v->forb.litter, v->forb.litter_daily);
		interpolate_monthlyValues(v->forb.CO2_biomass, v->forb.biomass_daily);
		interpolate_monthlyValues(v->forb.pct_live, v->forb.pct_live_daily);
		interpolate_monthlyValues(v->forb.lai_conv, v->forb.lai_conv_daily);
	}

	for (doy = 1; doy <= MAX_DAYS; doy++) {
		if (GT(v->grass.cov.fCover, 0.)) {
			lai_standing = v->grass.biomass_daily[doy] / v->grass.lai_conv_daily[doy];
			v->grass.pct_cover_daily[doy] = lai_standing / v->grass.conv_stcr;
			if (GT(v->grass.canopy_height_constant, 0.)) {
				v->grass.veg_height_daily[doy] = v->grass.canopy_height_constant;
			}
			else {
				v->grass.veg_height_daily[doy] = tanfunc(v->grass.biomass_daily[doy],
					v->grass.cnpy.xinflec,
					v->grass.cnpy.yinflec,
					v->grass.cnpy.range,
					v->grass.cnpy.slope); /* used for vegcov and for snowdepth_scale */
			}
			v->grass.lai_live_daily[doy] = lai_standing * v->grass.pct_live_daily[doy];
			v->grass.vegcov_daily[doy] = v->grass.pct_cover_daily[doy] * v->grass.veg_height_daily[doy]; /* used for vegetation interception */
			v->grass.biolive_daily[doy] = v->grass.biomass_daily[doy] * v->grass.pct_live_daily[doy];
			v->grass.biodead_daily[doy] = v->grass.biomass_daily[doy] - v->grass.biolive_daily[doy]; /* used for transpiration */
			v->grass.total_agb_daily[doy] = v->grass.litter_daily[doy] + v->grass.biomass_daily[doy]; /* used for bare-soil evaporation */
		}
		else {
			v->grass.lai_live_daily[doy] = 0.;
			v->grass.vegcov_daily[doy] = 0.;
			v->grass.biolive_daily[doy] = 0.;
			v->grass.biodead_daily[doy] = 0.;
			v->grass.total_agb_daily[doy] = 0.;
		}

		if (GT(v->shrub.cov.fCover, 0.)) {
			lai_standing = v->shrub.biomass_daily[doy] / v->shrub.lai_conv_daily[doy];
			v->shrub.pct_cover_daily[doy] = lai_standing / v->shrub.conv_stcr;
			if (GT(v->shrub.canopy_height_constant, 0.)) {
				v->shrub.veg_height_daily[doy] = v->shrub.canopy_height_constant;
			}
			else {
				v->shrub.veg_height_daily[doy] = tanfunc(v->shrub.biomass_daily[doy],
					v->shrub.cnpy.xinflec,
					v->shrub.cnpy.yinflec,
					v->shrub.cnpy.range,
					v->shrub.cnpy.slope); /* used for vegcov and for snowdepth_scale */
			}
			v->shrub.lai_live_daily[doy] = lai_standing * v->shrub.pct_live_daily[doy];
			v->shrub.vegcov_daily[doy] = v->shrub.pct_cover_daily[doy] * v->shrub.veg_height_daily[doy]; /* used for vegetation interception */
			v->shrub.biolive_daily[doy] = v->shrub.biomass_daily[doy] * v->shrub.pct_live_daily[doy];
			v->shrub.biodead_daily[doy] = v->shrub.biomass_daily[doy] - v->shrub.biolive_daily[doy]; /* used for transpiration */
			v->shrub.total_agb_daily[doy] = v->shrub.litter_daily[doy] + v->shrub.biomass_daily[doy]; /* used for bare-soil evaporation */
		}
		else {
			v->shrub.lai_live_daily[doy] = 0.;
			v->shrub.vegcov_daily[doy] = 0.;
			v->shrub.biolive_daily[doy] = 0.;
			v->shrub.biodead_daily[doy] = 0.;
			v->shrub.total_agb_daily[doy] = 0.;
		}

		if (GT(v->tree.cov.fCover, 0.)) {
			lai_standing = v->tree.biomass_daily[doy] / v->tree.lai_conv_daily[doy];
			v->tree.pct_cover_daily[doy] = lai_standing / v->tree.conv_stcr;
			if (GT(v->tree.canopy_height_constant, 0.)) {
				v->tree.veg_height_daily[doy] = v->tree.canopy_height_constant;
			}
			else {
				v->tree.veg_height_daily[doy] = tanfunc(v->tree.biomass_daily[doy],
					v->tree.cnpy.xinflec,
					v->tree.cnpy.yinflec,
					v->tree.cnpy.range,
					v->tree.cnpy.slope); /* used for vegcov and for snowdepth_scale */
			}
			v->tree.lai_live_daily[doy] = lai_standing * v->tree.pct_live_daily[doy]; /* used for vegetation interception */
			v->tree.vegcov_daily[doy] = v->tree.pct_cover_daily[doy] * v->tree.veg_height_daily[doy];
			v->tree.biolive_daily[doy] = v->tree.biomass_daily[doy] * v->tree.pct_live_daily[doy];
			v->tree.biodead_daily[doy] = v->tree.biomass_daily[doy] - v->tree.biolive_daily[doy]; /* used for transpiration */
			v->tree.total_agb_daily[doy] = v->tree.litter_daily[doy] + v->tree.biolive_daily[doy]; /* used for bare-soil evaporation */
		}
		else {
			v->tree.lai_live_daily[doy] = 0.;
			v->tree.vegcov_daily[doy] = 0.;
			v->tree.biolive_daily[doy] = 0.;
			v->tree.biodead_daily[doy] = 0.;
			v->tree.total_agb_daily[doy] = 0.;
		}

		if (GT(v->forb.cov.fCover, 0.)) {
			lai_standing = v->forb.biomass_daily[doy] / v->forb.lai_conv_daily[doy];
			v->forb.pct_cover_daily[doy] = lai_standing / v->forb.conv_stcr;
			if (GT(v->forb.canopy_height_constant, 0.)) {
				v->forb.veg_height_daily[doy] = v->forb.canopy_height_constant;
			}
			else {
				v->forb.veg_height_daily[doy] = tanfunc(v->forb.biomass_daily[doy],
					v->forb.cnpy.xinflec,
					v->forb.cnpy.yinflec,
					v->forb.cnpy.range,
					v->forb.cnpy.slope); /* used for vegcov and for snowdepth_scale */
			}
			v->forb.lai_live_daily[doy] = lai_standing * v->forb.pct_live_daily[doy]; /* used for vegetation interception */
			v->forb.vegcov_daily[doy] = v->forb.pct_cover_daily[doy] * v->forb.veg_height_daily[doy];
			v->forb.biolive_daily[doy] = v->forb.biomass_daily[doy] * v->forb.pct_live_daily[doy];
			v->forb.biodead_daily[doy] = v->forb.biomass_daily[doy] - v->forb.biolive_daily[doy]; /* used for transpiration */
			v->forb.total_agb_daily[doy] = v->forb.litter_daily[doy] + v->forb.biolive_daily[doy]; /* used for bare-soil evaporation */
		}
		else {
			v->forb.lai_live_daily[doy] = 0.;
			v->forb.vegcov_daily[doy] = 0.;
			v->forb.biolive_daily[doy] = 0.;
			v->forb.biodead_daily[doy] = 0.;
			v->forb.total_agb_daily[doy] = 0.;
		}
	}
}

void _echo_VegProd(void) {
	/* ================================================== */

	SW_VEGPROD *v = &SW_VegProd; /* convenience */
	char outstr[1500];

	sprintf(errstr, "\n==============================================\n"
		"Vegetation Production Parameters\n\n");
	strcpy(outstr, errstr);
	LogError(logfp, LOGNOTE, outstr);

	sprintf(errstr, "Grassland component\t= %1.2f\n"
		"\tAlbedo\t= %1.2f\n"
		"\tHydraulic redistribution flag\t= %d\n", v->grass.cov.fCover, v->grass.cov.albedo, v->grass.flagHydraulicRedistribution);
	strcpy(outstr, errstr);
	LogError(logfp, LOGNOTE, outstr);

	sprintf(errstr, "Shrubland component\t= %1.2f\n"
		"\tAlbedo\t= %1.2f\n"
		"\tHydraulic redistribution flag\t= %d\n", v->shrub.cov.fCover, v->shrub.cov.albedo, v->shrub.flagHydraulicRedistribution);
	strcpy(outstr, errstr);
	LogError(logfp, LOGNOTE, outstr);

	sprintf(errstr, "Forest-Tree component\t= %1.2f\n"
		"\tAlbedo\t= %1.2f\n"
		"\tHydraulic redistribution flag\t= %d\n", v->tree.cov.fCover, v->tree.cov.albedo, v->tree.flagHydraulicRedistribution);
	strcpy(outstr, errstr);
	LogError(logfp, LOGNOTE, outstr);

	sprintf(errstr, "FORB component\t= %1.2f\n"
		"\tAlbedo\t= %1.2f\n"
		"\tHydraulic redistribution flag\t= %d\n", v->forb.cov.fCover, v->forb.cov.albedo, v->forb.flagHydraulicRedistribution);
	strcpy(outstr, errstr);
	LogError(logfp, LOGNOTE, outstr);

	sprintf(errstr, "Bare Ground component\t= %1.2f\n"
		"\tAlbedo\t= %1.2f\n", v->bare_cov.fCover, v->bare_cov.albedo);
	strcpy(outstr, errstr);
	LogError(logfp, LOGNOTE, outstr);
}
