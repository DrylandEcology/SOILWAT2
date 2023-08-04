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
@brief Reads in `modelrun.in` and displays error message if file is incorrect.

@param[in,out] SW_Model Struct of type SW_MODEL holding basic time information
	about the simulation
@param[in] InFiles Array of program in/output files
@param[in] LogInfo Holds information dealing with logfile output
*/
void SW_MDL_read(SW_MODEL* SW_Model, char *InFiles[], LOG_INFO* LogInfo) {
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
	int lineno;
	char *MyFileName, inbuf[MAX_FILENAMESIZE];
	double temp;
	Bool foundHem = swFALSE;

	MyFileName = InFiles[eModel];
	f = OpenFile(MyFileName, "r", LogInfo);

	/* ----- Start checking for model time parameters */
	/*   input should be in order of startdy, enddy, hemisphere,
	 but if hemisphere occurs first, skip checking for the rest
	 and assume they're not there.
	 */
	lineno = 0;
	while (GetALine(f, inbuf)) {
		switch(lineno) {
			case 0: // Hemisphere
				SW_Model->isnorth =  (Bool) (toupper((int) *inbuf) == 'N');
				foundHem = swTRUE;
				break;
			case 1: // Longitude
				// longitude is currently not used by the code, but may be used in the future
				// it is present in the `siteparam.in` input file to completely document
				// site location
				SW_Model->longitude = atof(inbuf) * deg_to_rad;
				break;
			case 2: // Latitude
				SW_Model->latitude = atof(inbuf) * deg_to_rad;
				break;
			case 3: // Elevation
				SW_Model->elevation = atof(inbuf);
				break;
			case 4: // Slope
				SW_Model->slope = atof(inbuf) * deg_to_rad;
				break;
			case 5: // Aspect
				temp = atof(inbuf);
				SW_Model->aspect = missing(temp) ? temp : temp * deg_to_rad;
				break;
			default: // More lines than expected
				LogError(LogInfo, LOGFATAL, "More lines read than expected." \
						 "Please double check your `modelrun.in` file.");
				break;
		}
		lineno++;
	}

	if(!foundHem) {
		LogError(LogInfo, LOGWARN, "\tHemisphere - using \"N\"\n");
		SW_Model->isnorth = swTRUE;
	}

	CloseFile(&f, LogInfo);

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

	@param[in,out] SW_Model Struct of type SW_MODEL holding basic time
		information about the simulation
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
