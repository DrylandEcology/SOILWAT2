/********************************************************/
/********************************************************/
/*	Source file: Model.c
 Type: module
 Application: SOILWAT - soilwater dynamics simulator
 Purpose: Read / write and otherwise manage the model's
 parameter file information.  The SOILWAT model
 parameter design makes good objects, and STEPPE
 is almost object-ready and has a similar Model
 component.  So this is where the model-level things
 are kept, such as time (so far that's all).

 History:
 (8/28/01) -- INITIAL CODING - cwb
 12/02 - IMPORTANT CHANGE - cwb
 refer to comments in Times.h regarding base0
 2/14/03 - cwb - created the common/Times.[ch] code
 and modified this code to reflect that.
 20090826 (drs) stricmp -> strcmp
 03/12/2010	(drs) "DAYLAST_NORTH [==366]  : DAYLAST_SOUTH;" --> "m->endend = (m->isnorth) ? Time_get_lastdoy_y(m->endyr)  : DAYLAST_SOUTH;"
 2011/01/27	(drs) when 'ending day of last year' in years.in was set to 366 and 'last year' was not a leap year, SoilWat would still calculate 366 days for the last (non-leap) year
 improved code to calculate SW_Model.endend in SW_MDL_read() in case strcmp(enddyval, "end")!=0 with
 d = atoi(enddyval);
 m->endend = (d < 365) ? d : Time_get_lastdoy_y(m->endyr);
 06/27/2013	(drs)	closed open files if LogError() with LOGFATAL is called in SW_MDL_read()
 */
/********************************************************/
/********************************************************/

/* =================================================== */
/*                INCLUDES / DEFINES                   */
/* --------------------------------------------------- */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "include/generic.h"
#include "include/filefuncs.h"
#include "include/rands.h"
#include "include/Times.h"

#include "include/SW_Files.h"
#include "include/SW_Site.h"
#include "include/SW_SoilWater.h"  /* for setup_new_year() */
#include "include/SW_Times.h"
#include "include/SW_Model.h"


/* =================================================== */
/*                   Local Variable                    */
/* --------------------------------------------------- */

const TimeInt _notime = 0xffff; /* init value for _prev* */

/* =================================================== */
/*             Global Function Definitions             */
/* --------------------------------------------------- */

/**
@brief MDL constructor for global variables.

@param[out] newperiod newperiod[] Specifies when a new day/week/month/year
	has started
@param[out] days_in_month[] Number of days per month for "current" year
*/
void SW_MDL_construct(Bool newperiod[], TimeInt days_in_month[]) {
	/* =================================================== */
	/* note that an initializer that is called during
	 * execution (better called clean() or something)
	 * will need to free all allocated memory first
	 * before clearing structure.
	 */
	OutPeriod pd;

	Time_init_model(days_in_month); // values of time are correct only after Time_new_year()

	ForEachOutPeriod(pd)
	{
		newperiod[pd] = swFALSE;
	}
	newperiod[eSW_Day] = swTRUE; // every day is a new day
}

/**
@brief This is a blank function.
*/
void SW_MDL_deconstruct(void)
{}

/**
@brief Reads in MDL file and displays error message if file is incorrect.

@param[in,out] SW_Model Struct of type SW_MODEL holding basic time information
	about the simulation
*/
void SW_MDL_read(SW_MODEL* SW_Model) {
	/* =================================================== */
	/*
	 * 1/24/02 - added code for partial start and end years
	 *
	 * 28-Aug-03 (cwb) - N-S hemisphere flag logic changed.
	 *    Can now specify first day of first year, last
	 *    day of last year or hemisphere.  Code checks
	 *    whether the first value is [NnSs] or number to
	 *    make the decision.  If the value is numeric,
	 *    the hemisphere is assumed to be N.  This method
	 *    allows some degree of flexibility on the
	 *    starting year
	 */
	FILE *f;
	int y, cnt;
	TimeInt d;
	char *p, enddyval[6];
	Bool fstartdy = swFALSE, fenddy = swFALSE, fhemi = swFALSE;

	char *MyFileName = SW_F_name(eModel);
	f = OpenFile(MyFileName, "r");

	/* ----- beginning year */
	if (!GetALine(f, inbuf)) {
		CloseFile(&f);
		LogError(logfp, LOGFATAL, "%s: No input.", MyFileName);
	}
	y = atoi(inbuf);
	if (y < 0) {
		CloseFile(&f);
		LogError(logfp, LOGFATAL, "%s: Negative start year (%d)", MyFileName, y);
	}
	SW_Model->startyr = yearto4digit((TimeInt) y);
	SW_Model->addtl_yr = 0; // Could be done anywhere; SOILWAT2 runs don't need a delta year

	/* ----- ending year */
	if (!GetALine(f, inbuf)) {
		CloseFile(&f);
		LogError(logfp, LOGFATAL, "%s: Ending year not found.", MyFileName);
	}
	y = atoi(inbuf);
	//assert(y > 0);
	if (y < 0) {
		CloseFile(&f);
		LogError(logfp, LOGFATAL, "%s: Negative ending year (%d)", MyFileName, y);
	}
	SW_Model->endyr = yearto4digit((TimeInt) y);
	if (SW_Model->endyr < SW_Model->startyr) {
		CloseFile(&f);
		LogError(logfp, LOGFATAL, "%s: Start Year > End Year", MyFileName);
	}

	/* ----- Start checking for model time parameters */
	/*   input should be in order of startdy, enddy, hemisphere,
	 but if hemisphere occurs first, skip checking for the rest
	 and assume they're not there.
	 */
	cnt = 0;
	while (GetALine(f, inbuf)) {
		cnt++;
		if (isalpha((int) *inbuf) && strcmp(inbuf, "end")) { /* get hemisphere */
			SW_Model->isnorth = (Bool) (toupper((int) *inbuf) == 'N');
			fhemi = swTRUE;
			break;
		}//TODO: SHOULDN'T WE SKIP THIS BELOW IF ABOVE IS swTRUE
		switch (cnt) {
		case 1:
			SW_Model->startstart = atoi(inbuf);
			fstartdy = swTRUE;
			break;
		case 2:
			p = inbuf;
			cnt = 0;
			while (*p && cnt < 6) {
				enddyval[cnt++] = tolower((int) *(p++));
			}
			enddyval[cnt] = enddyval[5] = '\0';
			fenddy = swTRUE;
			break;
		case 3:
			SW_Model->isnorth = (Bool) (toupper((int) *inbuf) == 'N');
			fhemi = swTRUE;
			break;
		default:
			break; /* skip any extra lines */
		}
	}

	if (!(fstartdy && fenddy && fhemi)) {
		snprintf(errstr, MAX_ERROR, "\nNot found in %s:\n", MyFileName);
		if (!fstartdy) {
			strcat(errstr, "\tStart Day  - using 1\n");
			SW_Model->startstart = 1;
		}
		if (!fenddy) {
			strcat(errstr, "\tEnd Day    - using \"end\"\n");
			strcpy(enddyval, "end");
		}
		if (!fhemi) {
			strcat(errstr, "\tHemisphere - using \"N\"\n");
			SW_Model->isnorth = swTRUE;
		}
		strcat(errstr, "Continuing.\n");
		LogError(logfp, LOGWARN, errstr);
	}

	SW_Model->startstart += ((SW_Model->isnorth) ? DAYFIRST_NORTH : DAYFIRST_SOUTH) - 1;
	if (strcmp(enddyval, "end") == 0) {
		SW_Model->endend = (SW_Model->isnorth) ? Time_get_lastdoy_y(SW_Model->endyr) : DAYLAST_SOUTH;
	} else {
		d = atoi(enddyval);
		SW_Model->endend = (d == 365) ? Time_get_lastdoy_y(SW_Model->endyr) : 365;
	}

	SW_Model->daymid = (SW_Model->isnorth) ? DAYMID_NORTH : DAYMID_SOUTH;
	CloseFile(&f);

}

/**
@brief Sets up time structures and calls modules that have yearly init routines.

@param[in,out] SW_Model Struct of type SW_MODEL holding basic time information
		about the simulation
*/
void SW_MDL_new_year(SW_MODEL* SW_Model) {
	/* =================================================== */
	/* 1/24/02 - added code for partial start and end years
	 */
	TimeInt year = SW_Model->year;

	SW_Model->_prevweek = SW_Model->_prevmonth = SW_Model->_prevyear = _notime;

	Time_new_year(year, SW_Model->days_in_month, SW_Model->cum_monthdays);
	SW_Model->simyear = SW_Model->year + SW_Model->addtl_yr;

	SW_Model->firstdoy = (year == SW_Model->startyr) ? SW_Model->startstart : 1;
	SW_Model->lastdoy = (year == SW_Model->endyr) ? SW_Model->endend : Time_get_lastdoy_y(year);
}

/**
@brief Sets the output period elements of SW_Model based on current day.
*/
void SW_MDL_new_day(SW_MODEL* SW_Model) {

	OutPeriod pd;

	SW_Model->month = doy2month(SW_Model->doy, SW_Model->cum_monthdays); /* base0 */
	SW_Model->week = doy2week(SW_Model->doy); /* base0; more often an index */

	/* in this case, we've finished the daily loop and are about
	 * to flush the output */
	if (SW_Model->doy > SW_Model->lastdoy) {
		ForEachOutPeriod(pd)
		{
		SW_Model->newperiod[pd] = swTRUE;
		}

		return;
	}

	if (SW_Model->month != SW_Model->_prevmonth) {
		SW_Model->newperiod[eSW_Month] = (SW_Model->_prevmonth != _notime) ? swTRUE : swFALSE;
		SW_Model->_prevmonth = SW_Model->month;
	} else
		SW_Model->newperiod[eSW_Month] = swFALSE;

	/*  if (SW_Model.week != _prevweek || SW_Model.month == NoMonth) { */
	if (SW_Model->week != SW_Model->_prevweek) {
		SW_Model->newperiod[eSW_Week] = (SW_Model->_prevweek != _notime) ? swTRUE : swFALSE;
		SW_Model->_prevweek = SW_Model->week;
	} else
		SW_Model->newperiod[eSW_Week] = swFALSE;

}
