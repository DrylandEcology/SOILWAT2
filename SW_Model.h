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

#include "Times.h"
#include "SW_Defines.h"

typedef struct {
	TimeInt /* controlling dates for model run */
	startyr, /* beginning year for model run */
	endyr, /* ending year for model run */
	startstart, /* startday in start year */
	endend, /* end day in end year */
	daymid, /* mid year depends on hemisphere */
	/* current year dates */
	firstdoy, /* start day for this year */
	lastdoy, /* 366 if leapyear or endend if endyr */
	doy, week, month, year, simyear; /* current model time */
	/* however, week and month are base0 because they
	 * are used as array indices, so take care.
	 * doy and year are base1. */
	/* simyear = year + addtl_yr */

  int addtl_yr; /**< An integer representing how many years in the future we are simulating. Currently, only used to support rSFSW2 functionality where scenario runs are based on an 'ambient' run plus number of years in the future*/

	/* first day of new week/month is checked for
	 * printing and summing weekly/monthly values */
	Bool newperiod[SW_OUTNPERIODS];
	Bool isnorth;

} SW_MODEL;

void SW_MDL_read(void);
void SW_MDL_construct(void);
void SW_MDL_deconstruct(void);
void SW_MDL_new_year(void);
void SW_MDL_new_day(void);

#endif
