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
10/17/2011	(drs)	in SW_VPD_init(): v->veg[SW_TREES].total_agb_daily[doy] = v->veg[SW_TREES].litter_daily[doy] + v->veg[SW_TREES].biolive_daily[doy] instead of = v->veg[SW_TREES].litter_daily[doy] + v->veg[SW_TREES].biomass_daily[doy] to adjust for better scaling of potential bare-soil evaporation
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
#include "myMemory.h"
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

#ifdef STEPWAT
	#include "../sxw.h"
	extern SXW_t SXW; // structure to store values in and pass back to STEPPE
#endif

SW_VEGPROD SW_VegProd; /* declared here, externed elsewhere */

/* =================================================== */
/*                Module-Level Variables               */
/* --------------------------------------------------- */
static char *MyFileName;

// key2veg must be in the same order as the indices to vegetation types defined in SW_Defines.h
char const *key2veg[] = {"Trees", "Shrubs", "Forbs", "Grasses"};

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
	int x, k, lineno = 0;
	const int line_help = 27; // last case line number before monthly biomass densities
	RealF help_veg[NVEGTYPES], help_bareGround, litt, biom, pctl, laic;

	MyFileName = SW_F_name(eVegProd);
	f = OpenFile(MyFileName, "r");

	while (GetALine(f, inbuf)) {
		if (lineno++ < line_help) {
			switch (lineno) {
			/* fractions of vegetation types */
			case 1:
				x = sscanf(inbuf, "%f %f %f %f %f", &help_veg[SW_GRASS], &help_veg[SW_SHRUB],
					&help_veg[SW_TREES], &help_veg[SW_FORBS], &help_bareGround);
				if (x < NVEGTYPES + 1) {
					sprintf(errstr, "ERROR: invalid record in vegetation type components in %s\n", MyFileName);
					CloseFile(&f);
					LogError(logfp, LOGFATAL, errstr);
				}
				ForEachVegType(k) {
					v->veg[k].cov.fCover = help_veg[k];
				}
				v->bare_cov.fCover = help_bareGround;
				break;

			/* albedo */
			case 2:
				x = sscanf(inbuf, "%f %f %f %f %f", &help_veg[SW_GRASS], &help_veg[SW_SHRUB],
					&help_veg[SW_TREES], &help_veg[SW_FORBS], &help_bareGround);
				if (x < NVEGTYPES + 1) {
					sprintf(errstr, "ERROR: invalid record in albedo values in %s\n", MyFileName);
					CloseFile(&f);
					LogError(logfp, LOGFATAL, errstr);
				}
				ForEachVegType(k) {
					v->veg[k].cov.albedo = help_veg[k];
				}
				v->bare_cov.albedo = help_bareGround;
				break;

			/* canopy height */
			case 3:
				x = sscanf(inbuf, "%f %f %f %f", &help_veg[SW_GRASS], &help_veg[SW_SHRUB],
					&help_veg[SW_TREES], &help_veg[SW_FORBS]);
				if (x < NVEGTYPES) {
					sprintf(errstr, "ERROR: invalid record in canopy xinflec in %s\n", MyFileName);
					CloseFile(&f);
					LogError(logfp, LOGFATAL, errstr);
				}
				ForEachVegType(k) {
					v->veg[k].cnpy.xinflec = help_veg[k];
				}
				break;
			case 4:
				x = sscanf(inbuf, "%f %f %f %f", &help_veg[SW_GRASS], &help_veg[SW_SHRUB],
					&help_veg[SW_TREES], &help_veg[SW_FORBS]);
				if (x < NVEGTYPES) {
					sprintf(errstr, "ERROR: invalid record in canopy yinflec in %s\n", MyFileName);
					CloseFile(&f);
					LogError(logfp, LOGFATAL, errstr);
				}
				ForEachVegType(k) {
					v->veg[k].cnpy.yinflec = help_veg[k];
				}
				break;
			case 5:
				x = sscanf(inbuf, "%f %f %f %f", &help_veg[SW_GRASS], &help_veg[SW_SHRUB],
					&help_veg[SW_TREES], &help_veg[SW_FORBS]);
				if (x < NVEGTYPES) {
					sprintf(errstr, "ERROR: invalid record in canopy range in %s\n", MyFileName);
					CloseFile(&f);
					LogError(logfp, LOGFATAL, errstr);
				}
				ForEachVegType(k) {
					v->veg[k].cnpy.range = help_veg[k];
				}
				break;
			case 6:
				x = sscanf(inbuf, "%f %f %f %f", &help_veg[SW_GRASS], &help_veg[SW_SHRUB],
					&help_veg[SW_TREES], &help_veg[SW_FORBS]);
				if (x < NVEGTYPES) {
					sprintf(errstr, "ERROR: invalid record in canopy slope in %s\n", MyFileName);
					CloseFile(&f);
					LogError(logfp, LOGFATAL, errstr);
				}
				ForEachVegType(k) {
					v->veg[k].cnpy.slope = help_veg[k];
				}
				break;
			case 7:
				x = sscanf(inbuf, "%f %f %f %f", &help_veg[SW_GRASS], &help_veg[SW_SHRUB],
					&help_veg[SW_TREES], &help_veg[SW_FORBS]);
				if (x < NVEGTYPES) {
					sprintf(errstr, "ERROR: invalid record in canopy height constant option in %s\n", MyFileName);
					CloseFile(&f);
					LogError(logfp, LOGFATAL, errstr);
				}
				ForEachVegType(k) {
					v->veg[k].canopy_height_constant = help_veg[k];
				}
				break;

			/* vegetation interception parameters */
			case 8:
				x = sscanf(inbuf, "%f %f %f %f", &help_veg[SW_GRASS], &help_veg[SW_SHRUB],
					&help_veg[SW_TREES], &help_veg[SW_FORBS]);
				if (x < NVEGTYPES) {
					sprintf(errstr, "ERROR: invalid record in interception parameter kSmax(veg) in %s\n", MyFileName);
					CloseFile(&f);
					LogError(logfp, LOGFATAL, errstr);
				}
				ForEachVegType(k) {
					v->veg[k].veg_kSmax = help_veg[k];
				}
				break;
			case 9:
				x = sscanf(inbuf, "%f %f %f %f", &help_veg[SW_GRASS], &help_veg[SW_SHRUB],
					&help_veg[SW_TREES], &help_veg[SW_FORBS]);
				if (x < NVEGTYPES) {
					sprintf(errstr, "ERROR: invalid record in interception parameter kdead(veg) in %s\n", MyFileName);
					CloseFile(&f);
					LogError(logfp, LOGFATAL, errstr);
				}
				ForEachVegType(k) {
					v->veg[k].veg_kdead = help_veg[k];
				}
				break;

			/* litter interception parameters */
			case 10:
				x = sscanf(inbuf, "%f %f %f %f", &help_veg[SW_GRASS], &help_veg[SW_SHRUB],
					&help_veg[SW_TREES], &help_veg[SW_FORBS]);
				if (x < NVEGTYPES) {
					sprintf(errstr, "ERROR: invalid record in litter interception parameter kSmax(litter) in %s\n", MyFileName);
					CloseFile(&f);
					LogError(logfp, LOGFATAL, errstr);
				}
				ForEachVegType(k) {
					v->veg[k].lit_kSmax = help_veg[k];
				}
				break;

			/* parameter for partitioning of bare-soil evaporation and transpiration */
			case 11:
				x = sscanf(inbuf, "%f %f %f %f", &help_veg[SW_GRASS], &help_veg[SW_SHRUB],
					&help_veg[SW_TREES], &help_veg[SW_FORBS]);
				if (x < NVEGTYPES) {
					sprintf(errstr, "ERROR: invalid record in parameter for partitioning of bare-soil evaporation and transpiration in %s\n", MyFileName);
					CloseFile(&f);
					LogError(logfp, LOGFATAL, errstr);
				}
				ForEachVegType(k) {
					v->veg[k].EsTpartitioning_param = help_veg[k];
				}
				break;

			/* Parameter for scaling and limiting bare soil evaporation rate */
			case 12:
				x = sscanf(inbuf, "%f %f %f %f", &help_veg[SW_GRASS], &help_veg[SW_SHRUB],
					&help_veg[SW_TREES], &help_veg[SW_FORBS]);
				if (x < NVEGTYPES) {
					sprintf(errstr, "ERROR: invalid record in parameter for Parameter for scaling and limiting bare soil evaporation rate in %s\n", MyFileName);
					CloseFile(&f);
					LogError(logfp, LOGFATAL, errstr);
				}
				ForEachVegType(k) {
					v->veg[k].Es_param_limit = help_veg[k];
				}
				break;

			/* shade effects */
			case 13:
				x = sscanf(inbuf, "%f %f %f %f", &help_veg[SW_GRASS], &help_veg[SW_SHRUB],
					&help_veg[SW_TREES], &help_veg[SW_FORBS]);
				if (x < NVEGTYPES) {
					sprintf(errstr, "ERROR: invalid record in shade scale in %s\n", MyFileName);
					CloseFile(&f);
					LogError(logfp, LOGFATAL, errstr);
				}
				ForEachVegType(k) {
					v->veg[k].shade_scale = help_veg[k];
				}
				break;
			case 14:
				x = sscanf(inbuf, "%f %f %f %f", &help_veg[SW_GRASS], &help_veg[SW_SHRUB],
					&help_veg[SW_TREES], &help_veg[SW_FORBS]);
				if (x < NVEGTYPES) {
					sprintf(errstr, "ERROR: invalid record in shade max dead biomass in %s\n", MyFileName);
					CloseFile(&f);
					LogError(logfp, LOGFATAL, errstr);
				}
				ForEachVegType(k) {
					v->veg[k].shade_deadmax = help_veg[k];
				}
				break;
			case 15:
				x = sscanf(inbuf, "%f %f %f %f", &help_veg[SW_GRASS], &help_veg[SW_SHRUB],
					&help_veg[SW_TREES], &help_veg[SW_FORBS]);
				if (x < NVEGTYPES) {
					sprintf(errstr, "ERROR: invalid record in shade xinflec in %s\n", MyFileName);
					CloseFile(&f);
					LogError(logfp, LOGFATAL, errstr);
				}
				ForEachVegType(k) {
					v->veg[k].tr_shade_effects.xinflec = help_veg[k];
				}
				break;
			case 16:
				x = sscanf(inbuf, "%f %f %f %f", &help_veg[SW_GRASS], &help_veg[SW_SHRUB],
					&help_veg[SW_TREES], &help_veg[SW_FORBS]);
				if (x < NVEGTYPES) {
					sprintf(errstr, "ERROR: invalid record in shade yinflec in %s\n", MyFileName);
					CloseFile(&f);
					LogError(logfp, LOGFATAL, errstr);
				}
				ForEachVegType(k) {
					v->veg[k].tr_shade_effects.yinflec = help_veg[k];
				}
				break;
			case 17:
				x = sscanf(inbuf, "%f %f %f %f", &help_veg[SW_GRASS], &help_veg[SW_SHRUB],
					&help_veg[SW_TREES], &help_veg[SW_FORBS]);
				if (x < NVEGTYPES) {
					sprintf(errstr, "ERROR: invalid record in shade range in %s\n", MyFileName);
					CloseFile(&f);
					LogError(logfp, LOGFATAL, errstr);
				}
				ForEachVegType(k) {
					v->veg[k].tr_shade_effects.range = help_veg[k];
				}
				break;
			case 18:
				x = sscanf(inbuf, "%f %f %f %f", &help_veg[SW_GRASS], &help_veg[SW_SHRUB],
					&help_veg[SW_TREES], &help_veg[SW_FORBS]);
				if (x < NVEGTYPES) {
					sprintf(errstr, "ERROR: invalid record in shade slope in %s\n", MyFileName);
					CloseFile(&f);
					LogError(logfp, LOGFATAL, errstr);
				}
				ForEachVegType(k) {
					v->veg[k].tr_shade_effects.slope = help_veg[k];
				}
				break;

			/* Hydraulic redistribution */
			case 19:
				x = sscanf(inbuf, "%f %f %f %f", &help_veg[SW_GRASS], &help_veg[SW_SHRUB],
					&help_veg[SW_TREES], &help_veg[SW_FORBS]);
				if (x < NVEGTYPES) {
					sprintf(errstr, "ERROR: invalid record in hydraulic redistribution: flag in %s\n", MyFileName);
					CloseFile(&f);
					LogError(logfp, LOGFATAL, errstr);
				}
				ForEachVegType(k) {
					v->veg[k].flagHydraulicRedistribution = (Bool) EQ(help_veg[k], 1.);
				}
				break;
			case 20:
				x = sscanf(inbuf, "%f %f %f %f", &help_veg[SW_GRASS], &help_veg[SW_SHRUB],
					&help_veg[SW_TREES], &help_veg[SW_FORBS]);
				if (x < NVEGTYPES) {
					sprintf(errstr, "ERROR: invalid record in hydraulic redistribution: maxCondroot in %s\n", MyFileName);
					CloseFile(&f);
					LogError(logfp, LOGFATAL, errstr);
				}
				ForEachVegType(k) {
					v->veg[k].maxCondroot = help_veg[k];
				}
				break;
			case 21:
				x = sscanf(inbuf, "%f %f %f %f", &help_veg[SW_GRASS], &help_veg[SW_SHRUB],
					&help_veg[SW_TREES], &help_veg[SW_FORBS]);
				if (x < NVEGTYPES) {
					sprintf(errstr, "ERROR: invalid record in hydraulic redistribution: swpMatric50 in %s\n", MyFileName);
					CloseFile(&f);
					LogError(logfp, LOGFATAL, errstr);
				}
				ForEachVegType(k) {
					v->veg[k].swpMatric50 = help_veg[k];
				}
				break;
			case 22:
				x = sscanf(inbuf, "%f %f %f %f", &help_veg[SW_GRASS], &help_veg[SW_SHRUB],
					&help_veg[SW_TREES], &help_veg[SW_FORBS]);
				if (x < NVEGTYPES) {
					sprintf(errstr, "ERROR: invalid record in hydraulic redistribution: shapeCond in %s\n", MyFileName);
					CloseFile(&f);
					LogError(logfp, LOGFATAL, errstr);
				}
				ForEachVegType(k) {
					v->veg[k].shapeCond = help_veg[k];
				}
				break;

			/* Critical soil water potential */
			case 23:
				x = sscanf(inbuf, "%f %f %f %f", &help_veg[SW_GRASS], &help_veg[SW_SHRUB],
					&help_veg[SW_TREES], &help_veg[SW_FORBS]);
				if (x < NVEGTYPES) {
					sprintf(errstr, "ERROR: invalid record in critical soil water potentials: flag in %s\n", MyFileName);
					CloseFile(&f);
					LogError(logfp, LOGFATAL, errstr);
				}
				ForEachVegType(k) {
					v->veg[k].SWPcrit = -10. * help_veg[k];
					SW_VegProd.critSoilWater[k] = help_veg[k]; // for use with get_swa for properly partitioning available soilwater
				}
				get_critical_rank();
				break;

			/* CO2 Biomass Power Equation */
			// Coefficient 1
			case 24:
				x = sscanf(inbuf, "%f %f %f %f", &help_veg[SW_GRASS], &help_veg[SW_SHRUB],
					&help_veg[SW_TREES], &help_veg[SW_FORBS]);
				if (x < NVEGTYPES) {
					sprintf(errstr, "ERROR: Not enough arguments for CO2 Biomass Coefficient 1 in %s\n", MyFileName);
					CloseFile(&f);
					LogError(logfp, LOGFATAL, errstr);
				}
				ForEachVegType(k) {
					v->veg[k].co2_bio_coeff1 = help_veg[k];
				}
				break;
			// Coefficient 2
			case 25:
				x = sscanf(inbuf, "%f %f %f %f", &help_veg[SW_GRASS], &help_veg[SW_SHRUB],
					&help_veg[SW_TREES], &help_veg[SW_FORBS]);
				if (x < NVEGTYPES) {
					sprintf(errstr, "ERROR: Not enough arguments for CO2 Biomass Coefficient 2 in %s\n", MyFileName);
					CloseFile(&f);
					LogError(logfp, LOGFATAL, errstr);
				}
				ForEachVegType(k) {
					v->veg[k].co2_bio_coeff2 = help_veg[k];
				}
				break;

			/* CO2 WUE Power Equation */
			// Coefficient 1
			case 26:
				x = sscanf(inbuf, "%f %f %f %f", &help_veg[SW_GRASS], &help_veg[SW_SHRUB],
					&help_veg[SW_TREES], &help_veg[SW_FORBS]);
				if (x < NVEGTYPES) {
					sprintf(errstr, "ERROR: Not enough arguments for CO2 WUE Coefficient 1 in %s\n", MyFileName);
					CloseFile(&f);
					LogError(logfp, LOGFATAL, errstr);
				}
				ForEachVegType(k) {
					v->veg[k].co2_wue_coeff1 = help_veg[k];
				}
				break;
			// Coefficient 2
			case 27:
				x = sscanf(inbuf, "%f %f %f %f", &help_veg[SW_GRASS], &help_veg[SW_SHRUB],
					&help_veg[SW_TREES], &help_veg[SW_FORBS]);
				if (x < NVEGTYPES) {
					sprintf(errstr, "ERROR: Not enough arguments for CO2 WUE Coefficient 2 in %s\n", MyFileName);
					CloseFile(&f);
					LogError(logfp, LOGFATAL, errstr);
				}
				ForEachVegType(k) {
					v->veg[k].co2_wue_coeff2 = help_veg[k];
				}
				break;

			default:
				break;
			}
		}
		else {
			if (lineno == line_help + 1 || lineno == line_help + 1 + 12 || lineno == line_help + 1 + 12 * 2 || lineno == line_help + 1 + 12 * 3)
				mon = Jan;

			x = sscanf(inbuf, "%f %f %f %f", &litt, &biom, &pctl, &laic);
			if (x < NVEGTYPES) {
				sprintf(errstr, "ERROR: invalid record %d in %s\n", mon + 1, MyFileName);
				CloseFile(&f);
				LogError(logfp, LOGFATAL, errstr);
			}
			if (lineno > line_help + 12 * 3 && lineno <= line_help + 12 * 4) {
				v->veg[SW_FORBS].litter[mon] = litt;
				v->veg[SW_FORBS].biomass[mon] = biom;
				v->veg[SW_FORBS].pct_live[mon] = pctl;
				v->veg[SW_FORBS].lai_conv[mon] = laic;
			}
			else if (lineno > line_help + 12 * 2 && lineno <= line_help + 12 * 3) {
				v->veg[SW_TREES].litter[mon] = litt;
				v->veg[SW_TREES].biomass[mon] = biom;
				v->veg[SW_TREES].pct_live[mon] = pctl;
				v->veg[SW_TREES].lai_conv[mon] = laic;
			}
			else if (lineno > line_help + 12 && lineno <= line_help + 12 * 2) {
				v->veg[SW_SHRUB].litter[mon] = litt;
				v->veg[SW_SHRUB].biomass[mon] = biom;
				v->veg[SW_SHRUB].pct_live[mon] = pctl;
				v->veg[SW_SHRUB].lai_conv[mon] = laic;
			}
			else if (lineno > line_help && lineno <= line_help + 12) {
				v->veg[SW_GRASS].litter[mon] = litt;
				v->veg[SW_GRASS].biomass[mon] = biom;
				v->veg[SW_GRASS].pct_live[mon] = pctl;
				v->veg[SW_GRASS].lai_conv[mon] = laic;
			}

			mon++;

		}
	}

	if (mon < Dec) {
		sprintf(errstr, "No Veg Production values after month %d", mon + 1);
		LogError(logfp, LOGWARN, errstr);
	}

  SW_VPD_fix_cover();

	CloseFile(&f);
#ifdef RSOILWAT
	if (!collectInData)
#endif

	SW_VPD_init();

	if (EchoInits)
		_echo_VegProd();
}

/**
  \brief Check that sum of land cover is 1; adjust if not.

  \sideeffect
    - Adjusts `SW_VegProd->bare_cov.fCover` and `SW_VegProd->veg[k].cov.fCover`
      to sum to 1.
    - Print a warning that values are adjusted and notes with the new values.
*/
void SW_VPD_fix_cover(void)
{
  SW_VEGPROD *v = &SW_VegProd;
  int k;
  RealD fraction_sum = 0.;

  fraction_sum = v->bare_cov.fCover;
  ForEachVegType(k) {
    fraction_sum += v->veg[k].cov.fCover;
  }

  if (!EQ_w_tol(fraction_sum, 1.0, 1e-4)) {
    // inputs are never more precise than at most 3-4 digits

    LogError(logfp, LOGWARN,
      "Fractions of land cover components were normalized:\n" \
      "\tSum of fractions was %.4f instead of 1.0. " \
      "New coefficients are:", fraction_sum);

    v->bare_cov.fCover /= fraction_sum;
    LogError(logfp, LOGNOTE, "Bare ground fraction = %.4f", v->bare_cov.fCover);

    ForEachVegType(k) {
      v->veg[k].cov.fCover /= fraction_sum;
      LogError(logfp, LOGNOTE, "%s fraction = %.4f",
        key2veg[k], v->veg[k].cov.fCover);
    }

    fprintf(logfp, "\n");
  }
}


void SW_VPD_construct(void) {
	/* =================================================== */
	int year, k;
	OutPeriod pd;

	// Clear the module structure:
	memset(&SW_VegProd, 0, sizeof(SW_VegProd));

	// Allocate output structures:
	ForEachOutPeriod(pd)
	{
		SW_VegProd.p_accu[pd] = (SW_VEGPROD_OUTPUTS *) Mem_Calloc(1,
			sizeof(SW_VEGPROD_OUTPUTS), "SW_VPD_construct()");
		if (pd > eSW_Day) {
			SW_VegProd.p_oagg[pd] = (SW_VEGPROD_OUTPUTS *) Mem_Calloc(1,
				sizeof(SW_VEGPROD_OUTPUTS), "SW_VPD_construct()");
		}
	}

	/* initialize the co2-multipliers */
  for (year = 0; year < MAX_NYEAR; year++)
  {
    ForEachVegType(k)
    {
      SW_VegProd.veg[k].co2_multipliers[BIO_INDEX][year] = 1.;
      SW_VegProd.veg[k].co2_multipliers[WUE_INDEX][year] = 1.;
    }
  }
}

void SW_VPD_deconstruct(void)
{
	OutPeriod pd;

	// De-allocate output structures:
	ForEachOutPeriod(pd)
	{
		if (pd > eSW_Day && !isnull(SW_VegProd.p_oagg[pd])) {
			Mem_Free(SW_VegProd.p_oagg[pd]);
			SW_VegProd.p_oagg[pd] = NULL;
		}

		if (!isnull(SW_VegProd.p_accu[pd])) {
			Mem_Free(SW_VegProd.p_accu[pd]);
			SW_VegProd.p_accu[pd] = NULL;
		}
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
	int k;


	// Grab the real year so we can access CO2 data
	ForEachVegType(k)
	{
		if (GT(v->veg[k].cov.fCover, 0.))
		{
			if (k == SW_TREES)
			{
				// CO2 effects on biomass restricted to percent live biomass
				apply_biomassCO2effect(v->veg[k].CO2_pct_live, v->veg[k].pct_live,
					v->veg[k].co2_multipliers[BIO_INDEX][SW_Model.simyear]);

				interpolate_monthlyValues(v->veg[k].CO2_pct_live, v->veg[k].pct_live_daily);
				interpolate_monthlyValues(v->veg[k].biomass, v->veg[k].biomass_daily);

			} else {
				// CO2 effects on biomass applied to total biomass
				apply_biomassCO2effect(v->veg[k].CO2_biomass, v->veg[k].biomass,
					v->veg[k].co2_multipliers[BIO_INDEX][SW_Model.simyear]);

				interpolate_monthlyValues(v->veg[k].CO2_biomass, v->veg[k].biomass_daily);
				interpolate_monthlyValues(v->veg[k].pct_live, v->veg[k].pct_live_daily);
			}

			// Interpolation of remaining variables from monthly to daily values
			interpolate_monthlyValues(v->veg[k].litter, v->veg[k].litter_daily);
			interpolate_monthlyValues(v->veg[k].lai_conv, v->veg[k].lai_conv_daily);
		}
	}

	for (doy = 1; doy <= MAX_DAYS; doy++)
	{
		ForEachVegType(k) {
			if (GT(v->veg[k].cov.fCover, 0.))
			{
        /* vegetation height = 'veg_height_daily' is used for 'snowdepth_scale'; historically, also for 'vegcov' */
				if (GT(v->veg[k].canopy_height_constant, 0.))
				{
					v->veg[k].veg_height_daily[doy] = v->veg[k].canopy_height_constant;

				} else {
					v->veg[k].veg_height_daily[doy] = tanfunc(v->veg[k].biomass_daily[doy],
						v->veg[k].cnpy.xinflec,
						v->veg[k].cnpy.yinflec,
						v->veg[k].cnpy.range,
						v->veg[k].cnpy.slope);
				}

        /* live biomass = 'biolive_daily' is used for canopy-interception, transpiration, bare-soil evaporation, and hydraulic redistribution */
        v->veg[k].biolive_daily[doy] = v->veg[k].biomass_daily[doy] * v->veg[k].pct_live_daily[doy];

        /* dead biomass = 'biodead_daily' is used for canopy-interception and transpiration */
        v->veg[k].biodead_daily[doy] = v->veg[k].biomass_daily[doy] - v->veg[k].biolive_daily[doy];

        /* live leaf area index = 'lai_live_daily' is used for E-T partitioning */
        v->veg[k].lai_live_daily[doy] = v->veg[k].biolive_daily[doy] / v->veg[k].lai_conv_daily[doy];

        /* compound leaf area index = 'bLAI_total_daily' is used for canopy-interception */
        v->veg[k].bLAI_total_daily[doy] = v->veg[k].lai_live_daily[doy] +
          v->veg[k].veg_kdead * v->veg[k].biodead_daily[doy] / v->veg[k].lai_conv_daily[doy];

        /* total above-ground biomass = 'total_agb_daily' is used for bare-soil evaporation */
				if (k == SW_TREES)
				{
					v->veg[k].total_agb_daily[doy] = v->veg[k].litter_daily[doy] + v->veg[k].biolive_daily[doy];
				} else {
					v->veg[k].total_agb_daily[doy] = v->veg[k].litter_daily[doy] + v->veg[k].biomass_daily[doy];
				}

			} else {
				v->veg[k].lai_live_daily[doy] = 0.;
				v->veg[k].bLAI_total_daily[doy] = 0.;
				v->veg[k].biolive_daily[doy] = 0.;
				v->veg[k].biodead_daily[doy] = 0.;
				v->veg[k].total_agb_daily[doy] = 0.;
			}
		}
	}
}


/**
  @brief Sum up values across vegetation types

  @param[in] *x Array of size \ref NVEGTYPES
  @return Sum across `*x`
*/
RealD sum_across_vegtypes(RealD *x)
{
  unsigned int k;
  RealD sum = 0.;

  ForEachVegType(k)
  {
    sum += x[k];
  }

  return sum;
}


void _echo_VegProd(void) {
	/* ================================================== */

	SW_VEGPROD *v = &SW_VegProd; /* convenience */
	char outstr[1500];
	int k;

	sprintf(errstr, "\n==============================================\n"
		"Vegetation Production Parameters\n\n");
	strcpy(outstr, errstr);
	LogError(logfp, LOGNOTE, outstr);

	ForEachVegType(k) {
		sprintf(errstr,
			"%s component\t= %1.2f\n"
			"\tAlbedo\t= %1.2f\n"
			"\tHydraulic redistribution flag\t= %d\n",
			key2veg[k], v->veg[k].cov.fCover,
			v->veg[k].cov.albedo,
			v->veg[k].flagHydraulicRedistribution);
		strcpy(outstr, errstr);
		LogError(logfp, LOGNOTE, outstr);
	}

	sprintf(errstr, "Bare Ground component\t= %1.2f\n"
		"\tAlbedo\t= %1.2f\n", v->bare_cov.fCover, v->bare_cov.albedo);
	strcpy(outstr, errstr);
	LogError(logfp, LOGNOTE, outstr);
}


/** @brief Determine vegetation type of decreasingly ranked the critical SWP

  @sideeffect Sets `SW_VegProd.rank_SWPcrits[]` based on
    `SW_VegProd.critSoilWater[]`
*/
void get_critical_rank(void){
	/*----------------------------------------------------------
		Get proper order for rank_SWPcrits
	----------------------------------------------------------*/
	int i, outerLoop, innerLoop;
	float key;

	RealF tempArray[NVEGTYPES], tempArrayUnsorted[NVEGTYPES]; // need two temp arrays equal to critSoilWater since we dont want to alter the original at all

	ForEachVegType(i){
		tempArray[i] = SW_VegProd.critSoilWater[i];
		tempArrayUnsorted[i] = SW_VegProd.critSoilWater[i];
	}

	// insertion sort to rank the veg types and store them in their proper order
	for (outerLoop = 1; outerLoop < NVEGTYPES; outerLoop++)
	 {
			 key = tempArray[outerLoop]; // set key equal to critical value
			 innerLoop = outerLoop-1;
			 while (innerLoop >= 0 && tempArray[innerLoop] < key)
			 {
				 // code to switch values
				 tempArray[innerLoop+1] = tempArray[innerLoop];
				 innerLoop = innerLoop-1;
			 }
			 tempArray[innerLoop+1] = key;
	 }

	 // loops to compare sorted v unsorted array and find proper index
	 for(outerLoop = 0; outerLoop < NVEGTYPES; outerLoop++){
		 for(innerLoop = 0; innerLoop < NVEGTYPES; innerLoop++){
			 if(tempArray[outerLoop] == tempArrayUnsorted[innerLoop]){
				 SW_VegProd.rank_SWPcrits[outerLoop] = innerLoop;
				 tempArrayUnsorted[innerLoop] = SW_MISSING; // set value to something impossible so if a duplicate a different index is picked next
				 break;
			 }
		 }
	 }
	 /*printf("%d = %f\n", SW_VegProd.rank_SWPcrits[0], SW_VegProd.critSoilWater[SW_VegProd.rank_SWPcrits[0]]);
	 printf("%d = %f\n", SW_VegProd.rank_SWPcrits[1], SW_VegProd.critSoilWater[SW_VegProd.rank_SWPcrits[1]]);
	 printf("%d = %f\n", SW_VegProd.rank_SWPcrits[2], SW_VegProd.critSoilWater[SW_VegProd.rank_SWPcrits[2]]);
	 printf("%d = %f\n\n", SW_VegProd.rank_SWPcrits[3], SW_VegProd.critSoilWater[SW_VegProd.rank_SWPcrits[3]]);*/
	 /*----------------------------------------------------------
		 End of rank_SWPcrits
	 ----------------------------------------------------------*/
}
