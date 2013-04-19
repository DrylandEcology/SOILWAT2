/********************************************************/
/********************************************************/
/*	Application: SOILWAT - soilwater dynamics simulator
	Source file: Site.c
	Type: module
	Purpose: Read / write and otherwise manage the
	         site specific information.  See also the
	          Layer module.
	History:
	(8/28/01) -- INITIAL CODING - cwb
	(10/12/2009) - (drs) added altitude
	11/02/2010	(drs) added 5 snow parameters to SW_SITE to be read in from siteparam.in
	10/19/2010	(drs) added HydraulicRedistribution flag, and maxCondroot, swp50, and shapeCond parameters to SW_SIT_read()  and _echo_inputs()
	07/20/2011	(drs) updated _read_layers() to read impermeability values from each soil layer from soils.in file
						added calculation for saturated swc in water_eqn()
						updated _echo_inputs() to print impermeability and saturated swc values
	09/08/2011	(drs) moved all hydraulic redistribution parameters to SW_VegProd.h struct VegType
	09/15/2011	(drs)	deleted albedo from SW_SIT_read() and _echo_inputs(): moved it to SW_VegProd.h to make input vegetation type dependent
	02/03/2012	(drs)	if input of SWCmin < 0 then estimate SWCmin with 'SW_SWC_SWCres' for each soil layer
	02/04/2012	(drs)	included SW_VegProd.h and created global variable extern SW_VegProd: to access vegetation-specific SWPcrit
	02/04/2012	(drs)	added calculation of swc at SWPcrit for each vegetation type and layer to function '_init_site_info()'
						added vwc/swc at SWPcrit to '_echo_inputs()'
	05/24/2012  (DLM) edited SW_SIT_read(void) function to be able to read in Soil Temperature constants from siteparam.in file
	05/24/2012  (DLM) edited _echo_inputs(void) function to echo the Soil Temperature constants to the logfile
	05/25/2012  (DLM) edited _read_layers( void) function to read in the initial soil temperature for each layer
	05/25/2012  (DLM) edited _echo_inputs( void) function to echo the read in soil temperatures for each layer
	05/30/2012  (DLM) edited _read_layers & _echo_inputs functions to read in/echo the deltaX parameter
	05/31/2012  (DLM) edited _read_layers & _echo_inputs functions to read in/echo stMaxDepth & use_soil_temp variables
	05/31/2012  (DLM) edited _init_site_info(void) to check if stMaxDepth & stDeltaX values are usable, if not it resets them to the defaults (180 & 15).
	11/06/2012	(clk)	In SW_SIT_read(void), added lines to read in aspect and slope from siteparam.in
	11/06/2012	(clk)	In _echo_inputs(void), added lines to echo aspect and slope to logfile
	11/30/2012	(clk)	In SW_SIT_read(void), added lines to read in percentRunoff from siteparam.in
	11/30/2012	(clk)	In _echo_inputs(void), added lines to echo percentRunoff to logfile
*/
/********************************************************/
/********************************************************/

/* =================================================== */
/*                INCLUDES / DEFINES                   */
/* --------------------------------------------------- */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "generic.h"
#include "filefuncs.h"
#include "myMemory.h"
#include "SW_Defines.h"

#include "SW_Files.h"
#include "SW_Site.h"
#include "SW_SoilWater.h"

#include "SW_VegProd.h"

/* =================================================== */
/*                  Global Variables                   */
/* --------------------------------------------------- */

extern SW_VEGPROD SW_VegProd;
SW_SITE SW_Site;  /* declared here, externed elsewhere */

extern Bool EchoInits;

/* =================================================== */
/*                Module-Level Variables               */
/* --------------------------------------------------- */
static char *MyFileName;

/* transpiration regions  shallow, moderately shallow,  */
/* deep and very deep. units are in layer numbers. */
static LyrIndex _TranspRgnBounds[MAX_TRANSP_REGIONS];

/* for these three, units are cm/cm if < 1, -bars if >= 1 */
static RealD _SWCInitVal, /* initialization value for swc */
             _SWCWetVal,  /* value for a "wet" day,       */
             _SWCMinVal;  /* lower bound on swc.          */

/* =================================================== */
/* =================================================== */
/*             Private Function Definitions            */
/* --------------------------------------------------- */

static void _init_site_info(void);
static void _read_layers( void);
static void _echo_inputs( void);

static void water_eqn( RealD sand, RealD clay, LyrIndex n) {
/* --------------------------------------------------- */
RealD theta33, theta33t, OM = 0., thetaS33, thetaS33t; /* Saxton et al. auxiliary variables */

  SW_Site.lyr[n]->thetas = -14.2* sand - 3.7* clay + 50.5;
  SW_Site.lyr[n]->psis   = pow(10.0,
                               (-1.58* sand
                                - 0.63*clay
                                + 2.17));
  SW_Site.lyr[n]->b      = -0.3*sand + 15.7*clay + 3.10;

  if ( ZRO(SW_Site.lyr[n]->b) ) {
    LogError(stdout, LOGFATAL, "Value of beta in SW_SIT_read() = %f\n"
                    "Possible division by zero.  Exiting",
                    SW_Site.lyr[n]->b);
  }

  SW_Site.lyr[n]->binverse = 1.0 / SW_Site.lyr[n]->b;
  
	/* saturated soil water content: Saxton, K. E. and W. J. Rawls. 2006. Soil water characteristic estimates by texture and organic matter for hydrologic solutions. Soil Science Society of America Journal 70:1569-1578. */
	theta33t = -0.251*sand+0.195*clay+0.011*OM+0.006*(sand*OM)-0.027*(clay*OM)+0.452*(sand*clay)+0.299;
	theta33 = theta33t + (1.283 * pow(theta33t, 2) - 0.374 * theta33t - 0.015);
	
	thetaS33t = 0.278*sand+0.034*clay+0.022*OM-0.018*sand*OM-0.027*clay*OM-0.584*sand*clay+0.078;
	thetaS33 = thetaS33t + (0.636 * thetaS33t - 0.107);
	
	SW_Site.lyr[n]->swc_saturated = SW_Site.lyr[n]->width * (theta33 + thetaS33 - 0.097 * sand + 0.043);

}

static LyrIndex _newlayer( void) {
/* --------------------------------------------------- */
/* first time called with no layers so SW_Site.lyr
   not initialized yet, malloc() required.  For each
   layer thereafter realloc() is called.
*/
  SW_SITE *v = &SW_Site;
  v->n_layers++;

  v->lyr = (!v->lyr) /* if not yet defined */
         ? (SW_LAYER_INFO **)    /* malloc() it  */
            Mem_Calloc( v->n_layers,
                        sizeof(SW_LAYER_INFO *),
                        "_newlayer()")

         : (SW_LAYER_INFO **)  /* else realloc() */
            Mem_ReAlloc(v->lyr,
                        sizeof(SW_LAYER_INFO *)
                        * (v->n_layers));
  v->lyr[v->n_layers-1] = (SW_LAYER_INFO *)
                         Mem_Calloc(1, sizeof(SW_LAYER_INFO),
                                        "_newlayer()");
  return v->n_layers-1;
}

static void _clear_layer(LyrIndex n) {
/* --------------------------------------------------- */

  memset(SW_Site.lyr[n], 0, sizeof(SW_LAYER_INFO));

}


/* =================================================== */
/* =================================================== */
/*             Public Function Definitions             */
/* --------------------------------------------------- */

void SW_SIT_construct(void) {
/* =================================================== */
  /* note that an initializer that is called during
   * execution (better called clean() or something)
   * will need to free all allocated memory first
   * before clearing structure.
   */
  memset(&SW_Site, 0, sizeof(SW_Site));


}


void SW_SIT_read(void) {
/* =================================================== */
/* 5-Feb-2002 (cwb) Removed rgntop requirement in
 *    transpiration regions section of input
 */
   SW_SITE *v = &SW_Site;
   FILE *f;
   int lineno=0, x;
   LyrIndex  r,
             region, /* transp region definition number */
             rgnlow; /* lower layer of region */
   Bool too_many_regions=FALSE;

   /* note that Files.read() must be called prior to this. */
   MyFileName = SW_F_name(eSite);

   f = OpenFile(MyFileName, "r");

   v->n_transp_rgn = 0;
   while( GetALine(f, inbuf) ) {
      switch(lineno) {
        case 0:  _SWCMinVal    = atof(inbuf);       break;
        case 1:  _SWCInitVal   = atof(inbuf);       break;
        case 2:  _SWCWetVal    = atof(inbuf);       break;
        case 3:  v->reset_yr  = itob(atoi(inbuf)); break;
        case 4:  v->deepdrain = itob(atoi(inbuf)); break;
        case 5:  v->pet_scale = atof(inbuf);       break;
	case 6:	 v->percentRunoff = atof(inbuf);   break;
        case 7:  v->TminAccu2 = atof(inbuf);break;
        case 8:  v->TmaxCrit = atof(inbuf);break;
        case 9:  v->lambdasnow = atof(inbuf);break;
        case 10:  v->RmeltMin = atof(inbuf);break;
        case 11:  v->RmeltMax = atof(inbuf);break;
        case 12:  v->slow_drain_coeff = atof(inbuf);break;
        case 13:  v->evap.xinflec     = atof(inbuf);  break;
        case 14:  v->evap.slope     = atof(inbuf);  break;
        case 15:  v->evap.yinflec   = atof(inbuf);  break;
        case 16:  v->evap.range    = atof(inbuf);  break;
        case 17:  v->transp.xinflec  = atof(inbuf);  break;
        case 18:  v->transp.slope  = atof(inbuf);  break;
        case 19:  v->transp.yinflec = atof(inbuf);  break;
        case 20:  v->transp.range  = atof(inbuf);  break;
        case 21:  v->latitude      = atof(inbuf);  break;
        case 22:  v->altitude        = atof(inbuf);  break;
	case 23:  v->slope		= atof(inbuf); break;
	case 24:  v->aspect		= atof(inbuf); break;
        case 25:  v->bmLimiter		= atof(inbuf); break;
        case 26:  v->t1Param1		= atof(inbuf); break;
        case 27:  v->t1Param2		= atof(inbuf); break;
        case 28:  v->t1Param3		= atof(inbuf); break;
        case 29:  v->csParam1		= atof(inbuf); break;
        case 30:  v->csParam2		= atof(inbuf); break;
        case 31:  v->shParam		= atof(inbuf); break;
        case 32:  v->meanAirTemp	= atof(inbuf); break;
        case 33:  v->stDeltaX		= atof(inbuf); break;
        case 34:  v->stMaxDepth     = atof(inbuf); break;
        case 35:  v->use_soil_temp  = itob(atoi(inbuf)); break;
        default:
            if (lineno > 35+MAX_TRANSP_REGIONS) break; /* skip extra lines */

            if (MAX_TRANSP_REGIONS < v->n_transp_rgn) {
              too_many_regions = TRUE;
              goto Label_End_Read;
            }
            x = sscanf(inbuf, "%d %d",
                       &region, &rgnlow);
            if (x < 2) {
              LogError(logfp, LOGFATAL, "%s : Bad record %d.\n",
                        MyFileName, lineno);
            }
            _TranspRgnBounds[region-1] = rgnlow-1;
            v->n_transp_rgn++;
      }

      lineno++;
   }

Label_End_Read:

   CloseFile(&f);

   if (too_many_regions ) {
     LogError(logfp, LOGFATAL, "%s : Number of transpiration regions"
                    " exceeds maximum allowed (%d > %d)\n",
                     MyFileName, v->n_transp_rgn, MAX_TRANSP_REGIONS);
   }

  /* check for any discontinuities (reversals) in the transpiration regions */
   for(r=1; r< v->n_transp_rgn; r++) {
     if ( _TranspRgnBounds[r-1] >=  _TranspRgnBounds[r] ) {
       LogError(logfp, LOGFATAL,
                "%s : Discontinuity/reversal in transpiration regions.\n",
                SW_F_name(eSite));

     }
   }

   _read_layers();
   _init_site_info();
   if (EchoInits) _echo_inputs();
 }


static void _read_layers( void) {
/* =================================================== */
/* 5-Feb-2002 (cwb) removed dmin requirement in input file */

   SW_SITE *v = &SW_Site;
   FILE *f;
   Bool evap_ok = TRUE, /* mitigate gaps in layers' evap coeffs */
        transp_ok_tree = TRUE, /* same for transpiration coefficients */
        transp_ok_shrub = TRUE, /* same for transpiration coefficients */
        transp_ok_grass = TRUE, /* same for transpiration coefficients */
       fail = FALSE;
   LyrIndex lyrno;
   int x;
   char *errtype ='\0';
   RealF fieldcap,wiltpt,dmin=0.0, dmax,
        evco, trco_tree, trco_shrub, trco_grass, psand, pclay, bulkd, imperm, soiltemp,
        fval=0;

  /* note that Files.read() must be called prior to this. */
  MyFileName = SW_F_name(eLayers);

   f = OpenFile(MyFileName, "r");

   while( GetALine(f, inbuf) ) {
      lyrno = _newlayer();

      x=sscanf( inbuf, "%f %f %f %f %f %f %f %f %f %f %f %f",
                      &dmax,
                      &bulkd,
                      &fieldcap,
                      &wiltpt,
                      &evco,
                      &trco_grass,
                      &trco_shrub,
                      &trco_tree,
                      &psand,
                      &pclay,
                      &imperm,
                      &soiltemp);
      if (x<10) {
        LogError(logfp, LOGFATAL, "%s : Incomplete record %d.\n",
                 MyFileName, lyrno+1);
      }
      if        (LE(bulkd,0.)) {
        fail=TRUE; fval = bulkd;  errtype = Str_Dup("bulk density");
      } else if (LE(fieldcap,0.)) {
        fail=TRUE; fval = fieldcap; errtype = Str_Dup("field capacity");
      } else if (LE(wiltpt,0.)) {
        fail=TRUE; fval = wiltpt; errtype = Str_Dup("wilting point");
      } else if (LE(psand,0.)) {
        fail=TRUE; fval = psand;  errtype = Str_Dup("sand proportion");
      } else if (LE(pclay,0.)) {
        fail=TRUE; fval = pclay;  errtype = Str_Dup("clay proportion");
      } else if (LT(imperm,0.)) {
        fail=TRUE; fval = imperm;  errtype = Str_Dup("impermeability");
      }
      if (fail) {
        LogError(logfp, LOGFATAL, "%s : Invalid %s (%5.4f) in layer %d.\n",
                        MyFileName, errtype, fval, lyrno+1);
      }

      v->lyr[lyrno]->width = dmax - dmin; dmin = dmax;
      v->lyr[lyrno]->bulk_density  = bulkd;
      v->lyr[lyrno]->swc_fieldcap = fieldcap * v->lyr[lyrno]->width;
      v->lyr[lyrno]->swc_wiltpt   = wiltpt   * v->lyr[lyrno]->width;
      v->lyr[lyrno]->evap_coeff   = evco;
      v->lyr[lyrno]->transp_coeff_tree = trco_tree;
      v->lyr[lyrno]->transp_coeff_shrub = trco_shrub;
      v->lyr[lyrno]->transp_coeff_grass = trco_grass;
      v->lyr[lyrno]->pct_sand     = psand;
      v->lyr[lyrno]->pct_clay     = pclay;
      v->lyr[lyrno]->impermeability     = imperm;
      v->lyr[lyrno]->my_transp_rgn_tree = 0;
      v->lyr[lyrno]->my_transp_rgn_shrub = 0;
      v->lyr[lyrno]->my_transp_rgn_grass = 0;
      v->lyr[lyrno]->sTemp = soiltemp;

      if ( evap_ok ) {
        if ( GT(v->lyr[lyrno]->evap_coeff, 0.0) ) {
          v->n_evap_lyrs++;
        } else
          evap_ok = FALSE;
      }
      if ( transp_ok_tree ) {
        if ( GT(v->lyr[lyrno]->transp_coeff_tree, 0.0) )
          v->n_transp_lyrs_tree++;
        else
          transp_ok_tree = FALSE;
      }
      if ( transp_ok_shrub ) {
        if ( GT(v->lyr[lyrno]->transp_coeff_shrub, 0.0) )
          v->n_transp_lyrs_shrub++;
        else
          transp_ok_shrub = FALSE;
      }
      if ( transp_ok_grass ) {
        if ( GT(v->lyr[lyrno]->transp_coeff_grass, 0.0) )
          v->n_transp_lyrs_grass++;
        else
          transp_ok_grass = FALSE;
      }

      water_eqn( psand, pclay, lyrno);


      if (lyrno == MAX_LAYERS) {
        LogError(logfp, LOGFATAL,
                        "%s : Too many layers specified (%d).\n"
                        "Maximum number of layers is %d\n",
                        MyFileName, lyrno+1, MAX_LAYERS);
      }

   }
   CloseFile(&f);

   /* n_layers set in _newlayer() */

   if (v->deepdrain) {
     lyrno = _newlayer();
     v->lyr[lyrno]->width = 1.0;
   }


}

static void _init_site_info(void) {
/* =================================================== */
/* potentially this routine can be called whether the
 * layer data came from a file or a function call which
 * still requires initialization.
 */
 /* 5-Mar-2002 (cwb) added normalization for ev and tr coefficients */
 /* 1-Oct-03 (cwb) removed sum_evap_coeff and sum_transp_coeff  */

   SW_SITE  *sp = &SW_Site;
   SW_LAYER_INFO *lyr;
   LyrIndex s, r, curregion;
   int wiltminflag=0, initminflag=0;
   RealD  evsum=0., trsum_tree=0., trsum_shrub=0., trsum_grass=0., swcmin_help1, swcmin_help2;

/* sp->deepdrain indicates an extra (dummy) layer for deep drainage
 * has been added, so n_layers really should be n_layers -1
 * otherwise, the bottom layer is functional, so don't decrement n_layers
 * and set deep_layer to zero as a flag.
 * NOTE: deep_lyr is base0, n_layers is BASE1
 */
   sp->deep_lyr = (sp->deepdrain) ? --sp->n_layers: 0;

   ForEachSoilLayer(s) {
     lyr = sp->lyr[s];
   /* sum ev and tr coefficients for later */
     evsum += lyr->evap_coeff;
     trsum_tree += lyr->transp_coeff_tree;
     trsum_shrub += lyr->transp_coeff_shrub;
     trsum_grass += lyr->transp_coeff_grass;
     
	/* calculate soil water content at SWPcrit for each vegetation type */
	lyr->swc_atSWPcrit_tree = SW_SWC_bars2vol(SW_VegProd.tree.SWPcrit, s) * lyr->width;
	lyr->swc_atSWPcrit_shrub = SW_SWC_bars2vol(SW_VegProd.shrub.SWPcrit, s) * lyr->width;
	lyr->swc_atSWPcrit_grass = SW_SWC_bars2vol(SW_VegProd.grass.SWPcrit, s) * lyr->width;

  /* Find which transpiration region the current soil layer
   * is in and check validity of result. Region bounds are
   * base1 but s is base0.
   */
   /* for trees */
		curregion = 0;
		ForEachTranspRegion(r) {
			if(  s < _TranspRgnBounds[r] ) {
				if (ZRO(lyr->transp_coeff_tree) ) break; /* end of transpiring layers */
				curregion = r+1;
				break;
			}
		}
	
		if (curregion) {
			lyr->my_transp_rgn_tree = curregion;
			sp->n_transp_lyrs_tree = max(sp->n_transp_lyrs_tree, s);
	
		} else if (s == 0) {
			LogError(logfp, LOGFATAL, "%s : Top soil layer must be included\n"
					 "  in your tree tranpiration regions.\n",
					 SW_F_name(eSite));
		} else if (r < sp->n_transp_rgn) {
			LogError(logfp, LOGFATAL, "%s : Transpiration region %d \n"
						  "  is deeper than the deepest layer with a\n"
						  "  tree transpiration coefficient > 0 (%d) in '%s'.\n"
						  "  Please fix the discrepancy and try again.\n",
						  SW_F_name(eSite), r+1, s, SW_F_name(eLayers));
		}
	
   /* for shrubs */
		curregion = 0;
		ForEachTranspRegion(r) {
			if(  s < _TranspRgnBounds[r] ) {
				if (ZRO(lyr->transp_coeff_shrub) ) break; /* end of transpiring layers */
				curregion = r+1;
				break;
			}
		}
	
		if (curregion) {
			lyr->my_transp_rgn_shrub = curregion;
			sp->n_transp_lyrs_shrub = max(sp->n_transp_lyrs_shrub, s);
	
		} else if (s == 0) {
			LogError(logfp, LOGFATAL, "%s : Top soil layer must be included\n"
					 "  in your shrub tranpiration regions.\n",
					 SW_F_name(eSite));
		} else if (r < sp->n_transp_rgn) {
			LogError(logfp, LOGFATAL, "%s : Transpiration region %d \n"
						  "  is deeper than the deepest layer with a\n"
						  "  shrub transpiration coefficient > 0 (%d) in '%s'.\n"
						  "  Please fix the discrepancy and try again.\n",
						  SW_F_name(eSite), r+1, s, SW_F_name(eLayers));
		}
   /* for grasses */
		curregion = 0;
		ForEachTranspRegion(r) {
			if(  s < _TranspRgnBounds[r] ) {
				if (ZRO(lyr->transp_coeff_grass) ) break; /* end of transpiring layers */
				curregion = r+1;
				break;
			}
		}
	
		if (curregion) {
			lyr->my_transp_rgn_grass = curregion;
			sp->n_transp_lyrs_grass = max(sp->n_transp_lyrs_grass, s);
	
		} else if (s == 0) {
			LogError(logfp, LOGFATAL, "%s : Top soil layer must be included\n"
					 "  in your grass tranpiration regions.\n",
					 SW_F_name(eSite));
		} else if (r < sp->n_transp_rgn) {
			LogError(logfp, LOGFATAL, "%s : Transpiration region %d \n"
						  "  is deeper than the deepest layer with a\n"
						  "  grass transpiration coefficient > 0 (%d) in '%s'.\n"
						  "  Please fix the discrepancy and try again.\n",
						  SW_F_name(eSite), r+1, s, SW_F_name(eLayers));
		}

  /* Compute swc wet and dry limits and init value */
	if(	LT(_SWCMinVal, 0.0) ){ /* estimate swc_min for each layer based on residual SWC from an equation in Rawls WJ, Brakensiek DL (1985) Prediction of soil water properties for hydrological modeling. In Watershed management in the Eighties (eds Jones EB, Ward TJ), pp. 293-299. American Society of Civil Engineers, New York.
									or based on SWC at -3 MPa if smaller (= suction at residual SWC from Fredlund DG, Xing AQ (1994) EQUATIONS FOR THE SOIL-WATER CHARACTERISTIC CURVE. Canadian Geotechnical Journal, 31, 521-532.) */
		swcmin_help1 = SW_SWC_SWCres(lyr->pct_sand, lyr->pct_clay, lyr->swc_saturated/lyr->width) * lyr->width;
		swcmin_help2 = SW_SWC_bars2vol(30., s) * lyr->width;
		lyr->swc_min = fmax(0., fmin(swcmin_help1, swcmin_help2));
	} else if(	GE(_SWCMinVal, 1.0) ){ /* assume that unit(_SWCMinVal) == -bar */
		lyr->swc_min = SW_SWC_bars2vol(_SWCMinVal, s) * lyr->width;
	} else { /* assume that unit(_SWCMinVal) == cm/cm */
		lyr->swc_min = _SWCMinVal * lyr->width;
	}

     lyr->swc_wet = GE(_SWCWetVal, 1.0) ? SW_SWC_bars2vol(_SWCWetVal, s) * lyr->width
                                       : _SWCWetVal * lyr->width;
     lyr->swc_init = GE(_SWCInitVal, 1.0) ? SW_SWC_bars2vol(_SWCInitVal, s) * lyr->width
                                       : _SWCInitVal * lyr->width;
                                       
  /* test validity of values */
     if ( LT(lyr->swc_init,   lyr->swc_min) )  initminflag++;
     if ( LT(lyr->swc_wiltpt, lyr->swc_min) )  wiltminflag++;
     if ( LE(lyr->swc_wet, lyr->swc_min) ) {
       LogError( logfp, LOGFATAL, "%s : Layer %d\n"
                       "  calculated swc_wet (%7.4f) <= swc_min (%7.4f).\n"
                       "  Recheck parameters and try again.",
                       MyFileName, s+1, lyr->swc_wet, lyr->swc_min);
     }

   } /*end ForEachSoilLayer */


   if (wiltminflag) {
     LogError(logfp, LOGWARN,
                     "%s : %d layers were found in which wiltpoint < swc_min.\n"
                     "  You should reconsider wiltpoint or swc_min.\n"
                     "  See site parameter file for swc_min and site.log for swc details.",
                     MyFileName, wiltminflag);
   }

   if (initminflag) {
     LogError(logfp, LOGWARN,
                     "%s : %d layers were found in which swc_init < swc_min.\n"
                     "  You should reconsider swc_init or swc_min.\n"
                     "  See site parameter file for swc_init and site.log for swc details.",
                     MyFileName, initminflag);
   }

   /* normalize the evap and transp coefficients separately
    * to avoid obfuscation in the above loop */
    if (!EQ(evsum, 1.0) ) {
      LogError(logfp, LOGWARN, "%s : Evap coefficients were normalized, "
                      "ev_co sum (%5.4f) != 1.0.\nNew coefficients are:",
                     MyFileName, evsum);
      ForEachEvapLayer(s) {
        SW_Site.lyr[s]->evap_coeff /= evsum;
        LogError(logfp, LOGNOTE, "  Layer %d : %5.4f", s+1, SW_Site.lyr[s]->evap_coeff);
      }
    }
	if (!EQ(trsum_tree, 1.0) ) {
		LogError(logfp, LOGWARN, "%s : Transp coefficients for trees were normalized, "
					"tr_co_tree sum (%5.4f) != 1.0.\nNew coefficients are:",
					MyFileName, trsum_tree);
		ForEachTreeTranspLayer(s) {
			SW_Site.lyr[s]->transp_coeff_tree /= trsum_tree;
			LogError(logfp, LOGNOTE, "  Layer %d : %5.4f", s+1, SW_Site.lyr[s]->transp_coeff_tree);
		}
	}
	if (!EQ(trsum_shrub, 1.0) ) {
		LogError(logfp, LOGWARN, "%s : Transp coefficients for shrubs were normalized, "
					"tr_co_shrub sum (%5.4f) != 1.0.\nNew coefficients are:",
					MyFileName, trsum_shrub);
		ForEachShrubTranspLayer(s) {
			SW_Site.lyr[s]->transp_coeff_shrub /= trsum_shrub;
			LogError(logfp, LOGNOTE, "  Layer %d : %5.4f", s+1, SW_Site.lyr[s]->transp_coeff_shrub);
		}
	}
	if (!EQ(trsum_grass, 1.0) ) {
		LogError(logfp, LOGWARN, "%s : Transp coefficients for grasses were normalized, "
					"tr_co_grass sum (%5.4f) != 1.0.\nNew coefficients are:",
					MyFileName, trsum_grass);
		ForEachGrassTranspLayer(s) {
			SW_Site.lyr[s]->transp_coeff_grass /= trsum_grass;
			LogError(logfp, LOGNOTE, "  Layer %d : %5.4f", s+1, SW_Site.lyr[s]->transp_coeff_grass);
		}
	}	 
	
	sp->stNRGR = (sp->stMaxDepth / sp->stDeltaX) - 1; // getting the number of regressions, for use in the soil_temperature function
	if( !EQ(fmod(sp->stMaxDepth, sp->stDeltaX), 0.0) || (sp->stNRGR > MAX_ST_RGR) ) { 
	// resets it to the default values if the remainder of the division != 0.  fmod is like the % symbol except it works with double values
	// without this reset, then there wouldn't be a whole number of regressions in the soil_temperature function (ie there is a remainder from the division), so this way I don't even have to deal with that possibility
		if(sp->stNRGR > MAX_ST_RGR)
			fprintf(logfp, "\nSOIL_TEMP FUNCTION ERROR: the number of regressions is > the maximum number of regressions.  resetting max depth, deltaX, nRgr values to 180, 15, & 11 respectively\n");
		else
			fprintf(logfp, "\nSOIL_TEMP FUNCTION ERROR: max depth is not evenly divisible by deltaX (ie the remainder != 0).  resetting max depth, deltaX, nRgr values to 180, 15, & 11 respectively\n");
		
		sp->stMaxDepth = 180.0;
		sp->stNRGR = 11;
		sp->stDeltaX = 15.0;
	}

}


void SW_SIT_clear_layers( void) {
/* =================================================== */
  LyrIndex i;

  ForEachSoilLayer(i) _clear_layer(i);

}


static void _echo_inputs(void) {
/* =================================================== */
  SW_SITE *s = &SW_Site;
  LyrIndex i;

  fprintf(logfp,"\n\n=====================================================\n"
                "Site Related Parameters:\n"
                "---------------------\n");
  fprintf(logfp,"  Site File: %s\n", SW_F_name(eSite));
  fprintf(logfp,"  Reset SWC values each year: %s\n", (s->reset_yr)?"TRUE":"FALSE");
  fprintf(logfp,"  Use deep drainage reservoir: %s\n", (s->deepdrain)?"TRUE":"FALSE");
  fprintf(logfp,"  Slow Drain Coefficient: %5.4f\n", s->slow_drain_coeff);
  fprintf(logfp,"  PET Scale: %5.4f\n", s->pet_scale);
  fprintf(logfp,"  Proportion of surface water lost: %5.4f\n", s->percentRunoff);
  fprintf(logfp,"  Latitude (radians): %4.2f\n", s->latitude);
  fprintf(logfp,"  Altitude (m a.s.l.): %4.2f \n", s->altitude);
  fprintf(logfp,"  Slope (degrees): %4.2f\n", s->slope);
  fprintf(logfp,"  Aspect (degrees): %4.2f\n", s->aspect);


  fprintf(logfp,"\nSnow simulation parameters (SWAT2K model):\n----------------------\n");
  fprintf(logfp,"  Avg. air temp below which ppt is snow ( C): %5.4f\n", s->TminAccu2);
  fprintf(logfp,"  Snow temperature at which snow melt starts ( C): %5.4f\n", s->TmaxCrit);
  fprintf(logfp,"  Relative contribution of avg. air temperature to todays snow temperture vs. yesterday's snow temperature (0-1): %5.4f\n", s->lambdasnow);
  fprintf(logfp,"  Minimum snow melt rate on winter solstice (cm/day/C): %5.4f\n", s->RmeltMin);
  fprintf(logfp,"  Maximum snow melt rate on summer solstice (cm/day/C): %5.4f\n", s->RmeltMax);
  
  fprintf(logfp,"\nSoil Temperature Constants:\n----------------------\n");
  fprintf(logfp,"  Biomass Limiter constant: %5.4f\n", s->bmLimiter);
  fprintf(logfp,"  T1Param1: %5.4f\n", s->t1Param1);
  fprintf(logfp,"  T1Param2: %5.4f\n", s->t1Param2);
  fprintf(logfp,"  T1Param3: %5.4f\n", s->t1Param3);
  fprintf(logfp,"  csParam1: %5.4f\n", s->csParam1);
  fprintf(logfp,"  csParam2: %5.4f\n", s->csParam2);
  fprintf(logfp,"  shParam: %5.4f\n", s->shParam);
  fprintf(logfp,"  meanAirTemp: %5.4f\n", s->meanAirTemp);
  fprintf(logfp,"  deltaX: %5.4f\n", s->stDeltaX);
  fprintf(logfp,"  max depth: %5.4f\n", s->stMaxDepth);
  fprintf(logfp,"  Make soil temperature calculations: %s\n", (s->use_soil_temp) ? "TRUE" : "FALSE");
  fprintf(logfp,"  Number of regressions for the soil temperature function: %d\n", s->stNRGR);


  fprintf(logfp,"\nLayer Related Values:\n----------------------\n");
  fprintf(logfp,"  Soils File: %s\n", SW_F_name(eLayers));
  fprintf(logfp,"  Number of soil layers: %d\n", s->n_layers);
  fprintf(logfp,"  Number of evaporation layers: %d\n", s->n_evap_lyrs);
  fprintf(logfp,"  Number of tree transpiration layers: %d\n", s->n_transp_lyrs_tree);
  fprintf(logfp,"  Number of shrub transpiration layers: %d\n", s->n_transp_lyrs_shrub);
  fprintf(logfp,"  Number of grass transpiration layers: %d\n", s->n_transp_lyrs_grass);
  fprintf(logfp,"  Number of transpiration regions: %d\n", s->n_transp_rgn);


  fprintf(logfp,"\nLayer Specific Values:\n----------------------\n");
  fprintf(logfp,"\n  Layer information on a per centimeter depth basis:\n");
  fprintf(logfp,"  Lyr Width   BulkD     FieldC   WiltPt   %%Sand  %%Clay  VWC at Tree-critSWP	VWC at Shrub-critSWP	VWC at Grass-critSWP	EvCo   TrCo_Tree  TrCo_Shrub  TrCo_Grass   TrRgn_Tree   TrRgn_Shrub   TrRgn_Grass         Wet     Min      Init  Saturated    Impermeability\n");
  fprintf(logfp,"       (cm)  (g/cm^3)  (cm/cm)  (cm/cm)   (prop) (prop)   (cm/cm)            (cm/cm)            (cm/cm)            (prop)    (prop)     (prop)    (prop)         (int) 	      	(int) 		(int) 	    (cm/cm)    (cm/cm)   (cm/cm)   (cm/cm)      (frac)\n");
  fprintf(logfp,"  --- -----  --------   ------   ------   -----  ------   ------            ------            ------            ------    ------      ------   ------       ------   		   -----          -----   	  ----       ----      ----      ----         ----\n");
  ForEachSoilLayer(i) {
    fprintf(logfp,"  %3d %5.1f %9.5f %8.5f %8.5f %6.2f %6.2f %6.2f %6.2f %6.2f %9.2f %9.2f %9.2f %9.2f %10d %15d %15d %15.4f %9.4f %9.4f %9.4f %9.4f\n",
        i+1, s->lyr[i]->width, s->lyr[i]->bulk_density,
        s->lyr[i]->swc_fieldcap/s->lyr[i]->width, s->lyr[i]->swc_wiltpt/s->lyr[i]->width,
        s->lyr[i]->pct_sand, s->lyr[i]->pct_clay,
        s->lyr[i]->swc_atSWPcrit_tree/s->lyr[i]->width, s->lyr[i]->swc_atSWPcrit_shrub/s->lyr[i]->width, s->lyr[i]->swc_atSWPcrit_grass/s->lyr[i]->width, 
        s->lyr[i]->evap_coeff,
        s->lyr[i]->transp_coeff_tree, s->lyr[i]->transp_coeff_shrub, s->lyr[i]->transp_coeff_grass,
        s->lyr[i]->my_transp_rgn_tree, s->lyr[i]->my_transp_rgn_shrub, s->lyr[i]->my_transp_rgn_grass,
        s->lyr[i]->swc_wet/s->lyr[i]->width, s->lyr[i]->swc_min/s->lyr[i]->width,
        s->lyr[i]->swc_init/s->lyr[i]->width, s->lyr[i]->swc_saturated/s->lyr[i]->width,
        s->lyr[i]->impermeability);

  }
  fprintf(logfp,"\n  Actual per-layer values:\n");
  fprintf(logfp,"  Lyr Width   BulkD     FieldC   WiltPt %%Sand  %%Clay	SWC at Tree-critSWP	SWC at Shrub-critSWP	SWC at Grass-critSWP	 Wet    Min      Init  Saturated	SoilTemp\n");
  fprintf(logfp,"       (cm)  (g/cm^3)    (cm)     (cm)  (prop) (prop)   (cm)            (cm)            (cm)            (cm)   (cm)      (cm)     (cm)		(celcius)\n");
  fprintf(logfp,"  --- -----  --------   ------   ------ ------ ------   ------            ------            ------             ----     ----     ----    ----		----\n");

  ForEachSoilLayer(i) {
    fprintf(logfp,"  %3d %5.1f %9.5f %8.5f %8.5f %6.2f %6.2f %7.4f %7.4f %7.4f %7.4f %7.4f %8.4f %7.4f %5.4f\n",
        i+1, s->lyr[i]->width, s->lyr[i]->bulk_density,
        s->lyr[i]->swc_fieldcap, s->lyr[i]->swc_wiltpt,
        s->lyr[i]->pct_sand, s->lyr[i]->pct_clay,
        s->lyr[i]->swc_atSWPcrit_tree, s->lyr[i]->swc_atSWPcrit_shrub, s->lyr[i]->swc_atSWPcrit_grass, 
        s->lyr[i]->swc_wet, s->lyr[i]->swc_min, s->lyr[i]->swc_init, s->lyr[i]->swc_saturated,
        s->lyr[i]->sTemp);

  }

  fprintf(logfp,"\n  Water Potential values:\n");
  fprintf(logfp,"  Lyr         FieldCap            WiltPt              Tree-critSWP              Shrub-critSWP              Grass-critSWP              Wet            Min            Init\n");
  fprintf(logfp,"                (bars)            (bars)            (bars)            (bars)            (bars)            (bars)         (bars)          (bars)\n");
  fprintf(logfp,"  ---       -----------      ------------      -----------      -----------      -----------      -----------      -----------    --------------    --------------\n");

  ForEachSoilLayer(i) {
    fprintf(logfp,"  %3d   %15.4f   %15.4f  %15.4f  %15.4f  %15.4f  %15.4f   %15.4f   %15.4f\n",
        i+1,
        SW_SWC_vol2bars(s->lyr[i]->swc_fieldcap, i),
        SW_SWC_vol2bars(s->lyr[i]->swc_wiltpt ,i),
        SW_SWC_vol2bars(s->lyr[i]->swc_atSWPcrit_tree ,i),
        SW_SWC_vol2bars(s->lyr[i]->swc_atSWPcrit_shrub ,i),
        SW_SWC_vol2bars(s->lyr[i]->swc_atSWPcrit_grass ,i),
        SW_SWC_vol2bars(s->lyr[i]->swc_wet,    i),
        SW_SWC_vol2bars(s->lyr[i]->swc_min,    i),
        SW_SWC_vol2bars(s->lyr[i]->swc_init,   i)  );

  }



  fprintf(logfp, "\n------------ End of Site Parameters ------------------\n");
  fflush(logfp);

}


#ifdef DEBUG_MEM
#include "myMemory.h"
/*======================================================*/
void SW_SIT_SetMemoryRefs( void) {
/* when debugging memory problems, use the bookkeeping
   code in myMemory.c
 This routine sets the known memory refs in this module
 so they can be  checked for leaks, etc.  All refs will
 have been cleared by a call to ClearMemoryRefs() before
 this, and will be checked via CheckMemoryRefs() after
 this, most likely in the main() function.
*/
 LyrIndex l;

 NoteMemoryRef(SW_Site.lyr);
 ForEachSoilLayer(l) {
   NoteMemoryRef(SW_Site.lyr[l]);
 }
}

#endif
