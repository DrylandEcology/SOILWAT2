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
#include "generic.h" // externs `QuietMode`, `EchoInits`
#include "filefuncs.h" // externs `_firstfile`, `inbuf`
#include "myMemory.h"
#include "SW_Defines.h"
#include "SW_Files.h"
#include "SW_Times.h"
#include "SW_VegProd.h"
#include "SW_Model.h" // externs SW_Model
#include "SW_Weather.h"



/* =================================================== */
/*                  Global Variables                   */
/* --------------------------------------------------- */
SW_VEGPROD SW_VegProd;

// key2veg must be in the same order as the indices to vegetation types defined in SW_Defines.h
char const *key2veg[NVEGTYPES] = {"Trees", "Shrubs", "Forbs", "Grasses"};



/* =================================================== */
/*                  Local Variables                    */
/* --------------------------------------------------- */
static char *MyFileName;


/* =================================================== */
/*             Global Function Definitions             */
/* --------------------------------------------------- */

/**
@brief Reads file for SW_VegProd.
*/
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

    LogError(logfp, LOGQUIET, "");
  }
}

/**
@brief Constructor for SW_VegProd.
*/
void SW_VPD_construct(void) {
	/* =================================================== */
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
}



void SW_VPD_init_run(void) {
    TimeInt year;
    int k;

    SW_VEGPROD *veg = &SW_VegProd;
    SW_MODEL *model = &SW_Model;

    /* Set co2-multipliers to default */
    for (year = 0; year < MAX_NYEAR; year++)
    {
        ForEachVegType(k)
        {
            SW_VegProd.veg[k].co2_multipliers[BIO_INDEX][year] = 1.;
            SW_VegProd.veg[k].co2_multipliers[WUE_INDEX][year] = 1.;
        }
    }

    estimateVegetationFromClimate(veg, model->startyr, model->endyr);
    
}


/**
@brief Deconstructor for SW_VegProd.
*/
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
 *                     conditions (i.e., 360 ppm CO<SUB>2</SUB>, currently).
 * @param multiplier   The biomass multiplier for this PFT.
 *
 * @sideeffect new_biomass Updated biomass.
 */
void apply_biomassCO2effect(double new_biomass[], double biomass[], double multiplier) {
  int i;
  for (i = 0; i < 12; i++) new_biomass[i] = (biomass[i] * multiplier);
}



/**
@brief Update vegetation parameters for new year
*/
void SW_VPD_new_year(void) {
	/* ================================================== */
	/*
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


/**
@brief Text output for VegProd.
*/
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

/**
 @brief Wrapper function for estimating natural vegetation. First, climate is calculated and averaged, then values are estimated

 @param[in] veg Structure holding all values for vegetation cover of simulation
 @param[in] startYear Starting year of the simulation
 @param[in] endYear Ending year of the simulation
 */

void estimateVegetationFromClimate(SW_VEGPROD *veg, int startYear, int endYear) {

    int numYears = endYear - startYear + 1, deallocate = 0, allocate = 1;

    SW_CLIMATE_YEARLY climateOutput;
    SW_CLIMATE_CLIM climateAverages;

    // NOTE: 8 = number of types, 5 = (number of types) - grasses

    double coverValues[8] = {SW_MISSING, SW_MISSING, SW_MISSING, SW_MISSING,
                                SW_MISSING, SW_MISSING, SW_MISSING, SW_MISSING};

    double SumGrassesFraction = SW_MISSING, C4Variables[3], grassOutput[3],
    RelAbundanceL0[8], RelAbundanceL1[5];

    Bool fillEmptyWithBareGround = swTRUE, inNorth = swTRUE, warnExtrapolation = swTRUE;

    // Allocate climate structs' memory
    allocDeallocClimateStructs(allocate, numYears, &climateOutput, &climateAverages);

    calcSiteClimate(SW_Weather.allHist, numYears, startYear, &climateOutput);

    averageClimateAcrossYears(&climateOutput, numYears, &climateAverages);

    C4Variables[0] = climateAverages.minTempJuly_C;
    C4Variables[1] = climateAverages.ddAbove65F_degday;
    C4Variables[2] = climateAverages.frostFree_days;

    esimatePotNatVegComposition(climateAverages.meanTemp_C, climateAverages.PPT_cm, climateAverages.meanTempMon_C,
                                climateAverages.PPTMon_cm, coverValues, SumGrassesFraction, C4Variables,
                                fillEmptyWithBareGround, inNorth, warnExtrapolation,
                                grassOutput, RelAbundanceL0, RelAbundanceL1);

    // Deallocate climate structs' memory
    allocDeallocClimateStructs(deallocate, numYears, &climateOutput, &climateAverages);

}

/**
 @brief Using the data calculated in `calcSiteCliamte()`, estimates the natural vegetation composition of a site

 @param[in] meanTemp_C Value containing the average of yearly temperatures [C]
 @param[in] PPT_cm Value containing the average of yearly precipitation [cm]
 @param[in] meanTempMon_C Array of size MAX_MONTHS containing sum of monthly mean temperatures [C]
 @param[in] PPTMon_cm Array of size MAX_MONTHS containing sum of monthly mean precipitation [cm]
 @param[in] inputValues Array of size eight that contains starting values for the function to start with
 @param[in] SumGrassesFraction Value holding sum of grass if user would like it to be fixed
 @param[in] C4Variables Array of size three holding C4 variables after being averaged by `averageClimateAcrossYears()`.
 The elements are: 0) July precipitation, 1) mean temperature of dry quarter, 2) mean minimum temperature of February
 @param[in] fillEmptyWithBareGround Bool value specifying whether or not to fill gaps in values with bare ground
 @param[in] inNorth Bool value specifying if the current site is in the northern hemisphere
 @param[in] warnExtrapolation Bool value specifying whether or not to warn the user when extrapolation happens
 @param[out] grassOutput Array of size three holding estimated grass values. The elements are: 0) C3, 1) C4, 2) annual grasses
 @param[out] RelAbundanceL0 Array of size eight holding all estimated values. The elements are:
 0) Succulents, 1) Forbs, 2) C3, 3) C4, 4) annual grasses, 5) Shrubs, 6) Trees, 7) Bare ground
 @param[out] RelAbundanceL1 Array of size five holding all estimated values aside from grasses (not including sum of grasses).
 The elements are: 0) trees, 1) shrubs 2) sum of forbs and succulents 3) overall sum of grasses 4) bare ground

 @note This function uses a process that is specified in Teeri JA, Stowe LG (1976) and equations from Paruelo & Lauenroth (1996)
 */

void esimatePotNatVegComposition(double meanTemp_C, double PPT_cm, double meanTempMon_C[],
    double PPTMon_cm[], double inputValues[], double SumGrassesFraction, double C4Variables[],
    Bool fillEmptyWithBareGround, Bool inNorth, Bool warnExtrapolation, double *grassOutput,
    double *RelAbundanceL0, double *RelAbundanceL1) {

    int nTypes = 8, idsToEstimateGrass[3], winterMonths[3], summerMonths[3];

    // Indices both single value and arrays
    int index, succIndex = 0, forbIndex = 1, C3Index = 2, C4Index = 3,
    grassAnn = 4, shrubIndex = 5, treeIndex = 6, bareGround = 7, grassEstimSize = 0,
    overallEstimSize = 0, julyMin = 0, frostFreeDays = 1, degreeAbove65 = 2,
    estimIndices[5] = {succIndex, forbIndex, C3Index, C4Index, shrubIndex},
    estimIndicesNotNA = 0, grassesEstim[3], overallEstim[nTypes], iFixed[nTypes],
    iFixedSize = 0, isetIndices[3] = {grassAnn, treeIndex, bareGround}, estimIndicesSize = 0;

    double inputSum = 0, totalSumGrasses = 0., inputSumGrasses = 0., tempDiffJanJul,
    summerMAP = 0., winterMAP = 0., C4Species = 0., C3Grassland, C3Shrubland, estimGrassSum = 0,
    finalVegSum = 0., estimCoverSum = 0., tempSumGrasses = 0., estimCover[nTypes], iFixSum = 0.;

    Bool fixGrasses, fixBareGround = (Bool) (inputValues[bareGround] != SW_MISSING);

    // Initialize estimCover and overallEstim
    for(index = 0; index < nTypes; index++) {
        if(inputValues[index] != SW_MISSING) {
            iFixed[iFixedSize] = index;
            iFixedSize++;
        } else {
            estimIndices[estimIndicesSize] = index;
            estimIndicesSize++;
        }
        estimCover[index] = 0.;
    }

    fixGrasses = (Bool) (inputValues[grassAnn] != SW_MISSING && inputValues[C3Index] != SW_MISSING
    && inputValues[C3Index] != SW_MISSING);

    // Check if grasses are fixed
    if(fixGrasses) {
        // Set SumGrassesFraction
            // If SumGrassesFraction < 0, set to zero, otherwise keep at value
        SumGrassesFraction = (SumGrassesFraction < 0) ? 0 : SumGrassesFraction;
        // Get sum of input grass values and set to inputSumGrasses
        for(index = C3Index; index < grassAnn; index++) {
            if(inputValues[index] != SW_MISSING) inputSumGrasses += inputValues[index];
        }

        // Get totalSumGrasses
        totalSumGrasses = SumGrassesFraction - inputSumGrasses;

        // Check if totalSumGrasses is less than zero
        if(totalSumGrasses < 0){
            // Throw error and stop
        }
        // Find indices to estimate related to grass (i.e., C3, C4 and annual grasses)
        for(index = C3Index; index < grassAnn; index++) {
            if(inputValues[index] == SW_MISSING) {
                grassesEstim[grassEstimSize] = index;
                grassEstimSize++;
            }
        }

        // Check if totalSumGrasses is greater than zero
        if(totalSumGrasses > 0) {

            // Check if there is only one grass index to be estimated
            if(grassEstimSize == 1) {
                // Set element to SumGrassesFraction - inputSumGrasses
                inputValues[grassesEstim[0]] = SumGrassesFraction - inputSumGrasses;
                // Set totalSumGrasses to zero
                totalSumGrasses = 0.;
            }
        } else {
            // Otherwise, totalSumGrasses is zero or below
            for(index = 0; index < grassEstimSize; index++) {
                // Set all found ids to estimate to zero
                estimCover[grassesEstim[index]] = 0.;
            }
        }
    }

    uniqueIndices(inputValues, isetIndices, estimIndices, 3, estimIndicesSize, iFixed, &iFixedSize);

    // Loop through inputValues
    for(index = 0; index < nTypes; index++) {
        // Check if the element isn't missing
        if(inputValues[index] != SW_MISSING) {
            // Add value to inputSum
            inputSum += inputValues[index];
        } else {
            // Put index in overallEstim
            overallEstim[overallEstimSize] = index;
            // Incrememnt overallEstim index
            overallEstimSize++;
        }
    }

    // Check if number of elements to estimate is less than or equal to 1
    if(overallEstimSize <= 1) {
        if(overallEstimSize == 0) {
            // Check if we want to fill gaps in data with bare ground
            if(fillEmptyWithBareGround) {
                // Set estimCover at index `bareGround` to 1 - (all values execpt
                // at index `bareGround`)
                estimCover[bareGround] = 1 - (inputSum - estimCover[bareGround]);
            } else if(inputSum < 1) {
                LogError(logfp, LOGFATAL, "'estimate_PotNatVeg_composition': "
                         "User defined relative abundance values are all fixed, "
                         "but their sum is smaller than 1 = full land cover.");
            }
        } else if(overallEstimSize == 1) {
            estimCover[overallEstim[0]] = 1 - inputSum;
        }
    } else {

        if(PPT_cm * 10 <= 1) {
            for(index = 0; index < nTypes - 1; index++) {
                estimCover[index] = 0.;
            }
            estimCover[bareGround] = 1.;
        } else {
            // Set months of winter and summer (northern/southern hemisphere)
            if(inNorth) {
                for(index = 0; index < 3; index++) {
                    winterMonths[index] = (index + 11) % 12;
                    summerMonths[index] = (index + 5);
                    summerMAP += PPTMon_cm[summerMonths[index]] * 10;
                    winterMAP += PPTMon_cm[winterMonths[index]] * 10;
                }
            } else {
                for(index = 0; index < 3; index++) {
                    summerMonths[index] = (index + 11) % 12;
                    winterMonths[index] = (index + 5);
                    summerMAP += PPTMon_cm[summerMonths[index]] * 10;
                    winterMAP += PPTMon_cm[winterMonths[index]] * 10;
                }
            }
        }
        // Set summer and winter precipitations in mm
        summerMAP /= (PPT_cm * 10);
        winterMAP /= (PPT_cm * 10);

        // Get the difference between July and Janurary
        tempDiffJanJul = meanTempMon_C[summerMonths[1]] -
                                        meanTempMon_C[winterMonths[1]];

        if(warnExtrapolation) {
            if(meanTemp_C < 1) {
                LogError(logfp, LOGWARN, "Equations used outside supported range"
                         "(2 - 21.2 C): MAT = %2f, C reset to 1C", meanTemp_C);

                meanTemp_C = 1;
            }

            if(meanTemp_C > 21.2) {
                LogError(logfp, LOGWARN, "Equations used outside supported range"
                         "(2 - 21.2 C): MAT = %2f C", meanTemp_C);
            }

            if(PPT_cm * 10 < 117 || PPT_cm * 10 > 1011) {
                LogError(logfp, LOGWARN, "Equations used outside supported range"
                         "(117 - 1011 mm): MAP = %3f mm", PPT_cm * 10);
            }
        }

        // Paruelo & Lauenroth (1996): shrub climate-relationship:
        if(PPT_cm * 10 < 1) {
            estimCover[shrubIndex] = 0.;
        } else {
            estimCover[shrubIndex] = cutZeroInf(1.7105 - (.2918 * log(PPT_cm * 10))
                                                + (1.5451 * winterMAP));
        }

        // Paruelo & Lauenroth (1996): C4-grass climate-relationship:
        if(meanTemp_C <= 0) {
            estimCover[C4Index] = 0;
        } else {
            estimCover[C4Index] = cutZeroInf(-0.9837 + (.000594 * (PPT_cm * 10))
                                             + (1.3528 * summerMAP) + (.2710 * log(meanTemp_C)));
        }

        // This equations give percent species/vegetation -> use to limit
        // Paruelo's C4 equation, i.e., where no C4 species => C4 abundance == 0
        // TODO: Check to see if we can add a check to see if C4Variables is sufficient
        if(C4Variables != NULL) {
            if(C4Variables[frostFreeDays] <= 0) {
                C4Species = 0;
            } else {
                C4Species = ((1.6 * (C4Variables[julyMin] * 9 / 5 + 32)) +
                                   (.0086 * (C4Variables[degreeAbove65] * 9 / 5))
                                   - (8.98 * log(C4Variables[frostFreeDays])) - 22.44) / 100;
            }
        }

        if(EQ(C4Species, 0.)) estimCover[C4Index] = 0;

        // Paruelo & Lauenroth (1996): C3-grass climate-relationship:
        if(winterMAP <= 0) {
            C3Grassland = C3Shrubland = 0;
        } else {
            C3Grassland = 1.1905 - .02909 * meanTemp_C + .1781 * log(winterMAP) - .2383;
            C3Shrubland = 1.1905 - .02909 * meanTemp_C + .1781 * log(winterMAP) - .2383 * 2;

            if(C3Grassland < 0.) C3Grassland = 0.;
            if(C3Shrubland < 0.) C3Shrubland = 0.;
        }

        if(estimCover[shrubIndex] != SW_MISSING) {
            estimCover[C3Index] = C3Shrubland;
        } else {
            estimCover[C3Index] = C3Grassland;
        }

        // Paruelo & Lauenroth (1996): forb climate-relationship:
        if(PPT_cm * 10 < 1 || meanTemp_C <= 0) {
            estimCover[forbIndex] = SW_MISSING;
        } else {
            estimCover[forbIndex] = cutZeroInf(-.2035 + (.07975 * log(PPT_cm * 10))
                                               - (.0623 * log(meanTemp_C)));
        }

        // Paruelo & Lauenroth (1996): succulent climate-relationship:
        if(tempDiffJanJul <= 0 || winterMAP <= 0) {
            estimCover[succIndex] = SW_MISSING;
        } else {
            estimCover[succIndex] = cutZeroInf(-1 + ((1.20246 * pow(tempDiffJanJul, -.0689)) * (pow(winterMAP, -.0322))));
        }
    }

    for(index = 0; index < 5; index++) {
        if(estimCover[estimIndices[index]] == SW_MISSING) {
            estimCover[estimIndices[index]] = 0.;
        } else {
            estimIndicesNotNA++;
        }
    }

    if(!fillEmptyWithBareGround && estimIndicesNotNA <= 1) {
        if(PPT_cm * 10 < 600) {
            estimCover[shrubIndex] += 1.;
        }
        if(meanTemp_C < 10) {
            estimCover[C3Index] += 1.;
        }
        if(meanTemp_C >= 10 && PPT_cm * 10 > 600) {
            estimCover[C4Index] += 1.;
        }
    }

    if(fixGrasses && totalSumGrasses > 0) {
        for(index = 0; index < grassEstimSize; index++) {
            estimGrassSum += estimCover[idsToEstimateGrass[index]];
        }
        for(index = 0; index < grassEstimSize; index++) {
            estimCover[grassesEstim[index]] *= (totalSumGrasses / estimGrassSum);
        }
    } else if(grassEstimSize > 0) {
        for(index = 0; index < grassEstimSize; index++) {
            estimCover[grassesEstim[index]] = (totalSumGrasses / estimGrassSum);
        }
        LogError(logfp, LOGWARN, "'estimate_PotNatVeg_composition': "
                 "Total grass cover set, but no grass cover estimated; "
                 "requested cover evenly divided among grass types.");
    }

    // TODO: FIX
    if(fixGrasses) {
        //uniqueIndices(iFixed, &iFixedSize, iFixed, grassIndices, iFixedSize, 3);
        //uniqueIndices(estimIndices, &idsToEstimateGrass)
    }

    for(index = 0; index < nTypes; index++) {
        finalVegSum += estimCover[index];
    }

    if(!EQ(finalVegSum, 1.)) {
        estimCoverSum = mean(estimCover, nTypes) * nTypes;

        for(index = 0; index < iFixedSize; index++) {
            iFixSum += estimCover[iFixed[index]];
        }

        if(estimCoverSum > 0) {
            for(index = 0; index < estimIndicesSize; index++) {
                estimCover[estimIndices[index]] *= (1 - iFixSum) / estimCoverSum;
            }
        } else {
            if(fillEmptyWithBareGround && !fixBareGround) {
                inputValues[index] = 1.;
                for(index = 0; index < nTypes - 1; index++) {
                    inputValues[index] -= inputValues[index];
                }
            } else {
                LogError(logfp, LOGFATAL, "'estimate_PotNatVeg_composition': "
                         "The estimated vegetation cover values are 0, "
                         "the user fixed relative abundance values sum to less than 1, "
                         "and bare-ground is fixed. "
                         "Thus, the function cannot compute "
                         "complete land cover composition.");
            }
        }
    }

    grassOutput[0] = estimCover[C3Index];
    grassOutput[1] = estimCover[C4Index];
    grassOutput[2] = estimCover[grassAnn];

    tempSumGrasses = mean(grassOutput, 3) * 3;
    if(tempSumGrasses > 0) {
        for(index = 0; index < 3; index++) {
            grassOutput[index] /= tempSumGrasses;
        }
    }

    for(index = 0; index < nTypes; index++) {
        RelAbundanceL0[index] = estimCover[index];
    }

    RelAbundanceL1[0] = estimCover[treeIndex];
    RelAbundanceL1[1] = estimCover[shrubIndex];
    RelAbundanceL1[2] = estimCover[forbIndex] + estimCover[succIndex];
    RelAbundanceL1[3] = tempSumGrasses;
    RelAbundanceL1[4] = estimCover[bareGround];
}

/**
 @brief Helper function to `esimatePotNatVegComposition()` that doesn't allow a value to go below zero

 @return A value that is either above or equal to zero
 */

double cutZeroInf(double value) {
    if(value < 0.) {
        return 0.;
    } else {
        return value;
    }
}

/**
 @brief Helper function to `esimatePotNatVegComposition()` that gets unique indices from two input arrays

 @param[in] inputValues Array of size eight that holds the input to `esimatePotNatVegComposition()`
 @param[in] arrayOne First array to search through to get indices inside of it
 @param[in] arrayTwo Second array to search through to get indices inside of it
 @param[in] arrayOneSize Size of first array
 @param[in] arrayTwoSize Size of second array
 @param[out] finalIndexArray Array of size finalIndexArraySize that holds all unique indices from both arrays
 @param[in,out] finalIndexArraySize Value holding the size of finalIndexArray both before and after the function is run
 */

void uniqueIndices(double inputValues[], int arrayOne[], int arrayTwo[], int arrayOneSize,
                   int arrayTwoSize, int *finalIndexArray, int *finalIndexArraySize) {

    int indexOne, indexTwo, finalArrayIndex = *finalIndexArraySize;

    Bool indexFound = swFALSE;

    // Loop through first array and check all of second array to make sure it's not a repeat
    for(indexOne = 0; indexOne < arrayOneSize; indexOne++) {
        for(indexTwo = 0; indexTwo < arrayTwoSize; indexTwo++) {
            if(arrayOne[indexOne] == arrayTwo[indexTwo]) indexFound = swTRUE;
        }
        if(!indexFound && inputValues[arrayOne[indexOne]] != SW_MISSING) {
            finalIndexArray[finalArrayIndex] = arrayOne[indexOne];
            finalArrayIndex++;
            indexFound = swFALSE;
        }
    }

    // Loop through the second array to make sure to add things that were missed
    for(indexTwo = 0; indexTwo < arrayTwoSize; indexTwo++) {
        for(indexOne = 0; indexOne < arrayOneSize; indexOne++) {
            if(arrayTwo[indexTwo] == arrayOne[indexOne]) indexFound = swTRUE;
        }
        if(!indexFound && inputValues[arrayTwo[indexTwo]] != SW_MISSING) {
            finalIndexArray[finalArrayIndex] = arrayOne[indexOne];
            finalArrayIndex++;
            indexFound = swFALSE;
        }
    }

    *finalIndexArraySize = finalArrayIndex;

}
