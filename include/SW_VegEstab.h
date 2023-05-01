/********************************************************/
/********************************************************/
/*	Source file: Veg_Estab.h
	Type: header
	Application: SOILWAT - soilwater dynamics simulator
	Purpose: Supports Veg_Estab.c routines.
	History:
	(8/28/01) -- INITIAL CODING - cwb

	currently not used.
*/
/********************************************************/
/********************************************************/

#ifndef SW_VEGESTAB_H
#define SW_VEGESTAB_H

#include "include/SW_datastructs.h"


#ifdef __cplusplus
extern "C" {
#endif


/* indices to bars[] */
#define SW_GERM_BARS 0
#define SW_ESTAB_BARS 1

/* =================================================== */
/*             Global Function Declarations            */
/* --------------------------------------------------- */
void SW_VES_read(SW_VEGESTAB* SW_VegEstab);
void SW_VES_read2(
	SW_VEGESTAB* SW_VegEstab,
	Bool use_VegEstab,
	Bool consider_InputFlag
);
void SW_VES_construct(SW_VEGESTAB* SW_VegEstab);
void SW_VES_deconstruct(SW_VEGESTAB* SW_VegEstab);
void SW_VES_init_run(SW_LAYER_INFO** site_lyr, LyrIndex site_n_transp_lyrs[],
					 SW_VEGESTAB_INFO** parms, IntU count);
void SW_VegEstab_construct(SW_VEGESTAB* SW_VegEstab);
void SW_VES_checkestab(SW_VEGESTAB_INFO** parms, SW_WEATHER* SW_Weather,
					   RealD swcBulk[][MAX_LAYERS], TimeInt doy,
					   TimeInt firstdoy, IntU count);
void SW_VES_new_year(IntU count);
void _spp_init(SW_VEGESTAB_INFO** parms, unsigned int sppnum,
			   SW_LAYER_INFO** lyr, LyrIndex n_transp_lyrs[]);
IntU _new_species(SW_VEGESTAB* SW_VegEstab);
void _echo_VegEstab(SW_LAYER_INFO** lyr, SW_VEGESTAB_INFO** parms,
					IntU count);


/* COMMENT-1
 * There are a lot of things to keep track of during the period of
 * possible germination to possible establishment, and the original
 * fortran variables were badly named (effect of 8 char limit?)
 * Here are the current variables and their fortran counterparts
 * in case somebody has to go back to the old code.


 max_drydays_postgerm          ------  numwait
 min_days_germ2estab           ------  minint
 max_days_germ2estab           ------  maxint
 wet_days_before_germ         ------  mingdys
 max_days_4germ                ------  maxgwait
 num_wet_days_for_estab        ------  nestabdy
 num_wet_days_for_germ         ------  ngermdy

 min_temp_germ                 ------  tmpmin
 max_temp_germ                 ------  tmpmax

 wet_swc_germ[]                ------  wetger
 wet_swc_estab[]               ------  wetest
 roots_wet                     ------  rootswet

 */


#ifdef __cplusplus
}
#endif

#endif
