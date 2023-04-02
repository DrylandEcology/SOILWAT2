/********************************************************/
/********************************************************/
/*  Source file: SW_Model.h
 *  Type: header
 *  Application: SOILWAT - soilwater dynamics simulator
 *  Purpose: Support for the Model.c routines and any others
 *           that need to reference that module.
 *
 *  History:
 *     (8/28/01) -- INITIAL CODING - cwb
 *    12/02 - IMPORTANT CHANGE - cwb
 *          refer to comments in Times.h regarding base0
 *
 *  2/14/03 - cwb - removed the days_in_month and
 *          cum_month_days arrays to common/Times.[ch]
 */
/********************************************************/
/********************************************************/

#ifndef SW_MODEL_H
#define SW_MODEL_H

#include "include/SW_datastructs.h"

#ifdef __cplusplus
extern "C" {
#endif


/* =================================================== */
/*            Externed Global Variables                */
/* --------------------------------------------------- */
extern SW_MODEL SW_Model;


/* =================================================== */
/*             Global Function Declarations            */
/* --------------------------------------------------- */
void SW_MDL_read(void);
void SW_MDL_construct(void);
void SW_MDL_deconstruct(void);
void SW_MDL_new_year(void);
void SW_MDL_new_day(void);


#ifdef __cplusplus
}
#endif

#endif
