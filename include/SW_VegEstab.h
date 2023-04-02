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
/*            Externed Global Variables                */
/* --------------------------------------------------- */
extern SW_VEGESTAB SW_VegEstab;


/* =================================================== */
/*             Global Function Declarations            */
/* --------------------------------------------------- */
void SW_VES_read(void);
void SW_VES_read2(Bool use_VegEstab, Bool consider_InputFlag);
void SW_VES_construct(void);
void SW_VES_deconstruct(void);
void SW_VES_init_run(void);
void SW_VegEstab_construct(void);
void SW_VES_checkestab(SW_WEATHER* SW_Weather);
void SW_VES_new_year(void);
void _spp_init(unsigned int sppnum);
unsigned int _new_species(void);
void _echo_VegEstab(void);


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
