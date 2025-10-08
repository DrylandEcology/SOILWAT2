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

 03/12/2010	(drs) "DAYLAST_NORTH [==366]  : DAYLAST_SOUTH;" --> "m->endend =
 (m->isnorth) ? Time_get_lastdoy_y(m->endyr)  : DAYLAST_SOUTH;"

 2011/01/27	(drs) when 'ending day of last year' in years.in was set to 366
 and 'last year' was not a leap year, SoilWat would still calculate 366 days for
 the last (non-leap) year improved code to calculate SW_Model.endend in
 SW_MDL_read() in case strcmp(enddyval, "end")!=0 with d = atoi(enddyval);
 m->endend = (d < 365) ? d : Time_get_lastdoy_y(m->endyr);

 06/27/2013	(drs)	closed open files if LogError() with LOGFATAL is called
 in SW_MDL_read()
 */
/********************************************************/
/********************************************************/

/* =================================================== */
/*                INCLUDES / DEFINES                   */
/* --------------------------------------------------- */
#include "include/SW_Model.h"       // for SW_MDL_construct, SW_MDL_deconst...
#include "include/filefuncs.h"      // for CloseFile, GetALine, LogError
#include "include/generic.h"        // for swFALSE, swTRUE, GT, LOGERROR
#include "include/SW_datastructs.h" // for SW_MODEL, LOG_INFO, SW_DOMAIN
#include "include/SW_Defines.h"     // for deg_to_rad, ForEachOutPeriod
#include "include/SW_Files.h"       // for eModel
#include "include/Times.h"          // for Time_get_lastdoy_y, Time_init_model
#include <stdio.h>                  // for FILE
#include <string.h>                 // for memcpy


/* =================================================== */
/*                   Local Variable                    */
/* --------------------------------------------------- */

const TimeInt notime = 0xffff; /* init value for _prev* */

/* =================================================== */
/*             Global Function Definitions             */
/* --------------------------------------------------- */

/**
@brief MDL constructor for global variables.

@param[in,out] SW_ModelSim Struct of type SW_MODEL_SIM holding basic
intermediate time information about the simulation run
*/
void SW_MDL_construct(SW_MODEL_SIM *SW_ModelSim) {
    /* =================================================== */
    /* note that an initializer that is called during
     * execution (better called clean() or something)
     * will need to free all allocated memory first
     * before clearing structure.
     */
    OutPeriod pd;

    // values of time are correct only after Time_new_year()
    Time_init_model(SW_ModelSim->days_in_month);

    ForEachOutPeriod(pd) { SW_ModelSim->newperiod[pd] = swFALSE; }
    SW_ModelSim->newperiod[eSW_Day] = swTRUE; // every day is a new day

    SW_ModelSim->addtl_yr = 0;
    SW_ModelSim->doOutput = swTRUE;
}

/**
@brief This is a blank function.
*/
void SW_MDL_deconstruct(void) {}

/**
@brief Reads in `modelrun.in` and displays error message if file is incorrect.

@param[in,out] ModelRunIn Struct of type SW_MODEL_RUN_INPUTS holding basic
    input time information about the simulation
@param[in] txtInFiles Array of program in/output files
@param[out] LogInfo Holds information on warnings and errors
*/
void SW_MDL_read(
    SW_MODEL_RUN_INPUTS *ModelRunIn, char *txtInFiles[], LOG_INFO *LogInfo
) {
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
    char *MyFileName;
    char inbuf[MAX_FILENAMESIZE];
    double value;

    MyFileName = txtInFiles[eModel];
    f = OpenFile(MyFileName, "r", LogInfo);
    if (LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

    /* ----- Start checking for model time parameters */
    /*   input should be in order of startdy, enddy, hemisphere,
     but if hemisphere occurs first, skip checking for the rest
     and assume they're not there.
     */
    lineno = 0;
    while (GetALine(f, inbuf, MAX_FILENAMESIZE)) {
        if (lineno <= 4) {
            value = sw_strtod(inbuf, MyFileName, LogInfo);
            if (LogInfo->stopRun) {
                goto closeFile;
            }
        }

        switch (lineno) {
        case 0: // Longitude
            // longitude is currently not used by the code, but may be used in
            // the future it is present in the `siteparam.in` input file to
            // completely document site location
            ModelRunIn->longitude = value * deg_to_rad;
            break;

        case 1: // Latitude
            ModelRunIn->latitude = value * deg_to_rad;
            // Calculate hemisphere based on latitude
            ModelRunIn->isnorth =
                (GT(ModelRunIn->latitude, 0.)) ? swTRUE : swFALSE;
            break;

        case 2: // Elevation
            ModelRunIn->elevation = value;
            break;

        case 3: // Slope
            ModelRunIn->slope = value * deg_to_rad;
            break;

        case 4: // Aspect
            ModelRunIn->aspect = missing(value) ? value : value * deg_to_rad;
            break;

        default: // More lines than expected
            LogError(
                LogInfo,
                LOGERROR,
                "More lines read than expected."
                "Please double check your `modelrun.in` file."
            );
            goto closeFile;

            break;
        }
        lineno++;
    }

closeFile: { CloseFile(&f, LogInfo); }
}

/**
@brief Sets up time structures and calls modules that have yearly init routines.

@param[in,out] SW_ModelIn Struct of type SW_MODEL_INPUTS holding basic input
    time information about the simulation
@param[in,out] SW_ModelSim Struct of type SW_MODEL_SIM holding basic
intermediate time information about the simulation run
*/
void SW_MDL_new_year(SW_MODEL_INPUTS *SW_ModelIn, SW_MODEL_SIM *SW_ModelSim) {
    /* =================================================== */
    /* 1/24/02 - added code for partial start and end years
     */
    TimeInt year = SW_ModelSim->year;

    SW_ModelSim->prevweek = SW_ModelSim->prevmonth = SW_ModelSim->prevyear =
        notime;

    Time_new_year(year, SW_ModelSim->days_in_month, SW_ModelSim->cum_monthdays);
    SW_ModelSim->simyear = SW_ModelSim->year + SW_ModelSim->addtl_yr;

    SW_ModelSim->firstdoy =
        (year == SW_ModelIn->startyr && !SW_ModelIn->SW_SpinUp.spinup) ?
            SW_ModelIn->startstart :
            1;

    SW_ModelSim->lastdoy =
        (year == SW_ModelIn->endyr && !SW_ModelIn->SW_SpinUp.spinup) ?
            SW_ModelIn->endend :
            Time_get_lastdoy_y(year);
}

/**
@brief Sets the output period elements of SW_Model based on current day.

@param[in,out] SW_ModelSim Struct of type SW_MODEL_SIM holding basic
intermediate time information about the simulation run
*/
void SW_MDL_new_day(SW_MODEL_SIM *SW_ModelSim) {

    OutPeriod pd;

    SW_ModelSim->month =
        doy2month(SW_ModelSim->doy, SW_ModelSim->cum_monthdays); /* base0 */
    SW_ModelSim->week =
        doy2week(SW_ModelSim->doy); /* base0; more often an index */

    /* in this case, we've finished the daily loop and are about
     * to flush the output */
    if (SW_ModelSim->doy > SW_ModelSim->lastdoy) {
        ForEachOutPeriod(pd) { SW_ModelSim->newperiod[pd] = swTRUE; }

        return;
    }

    if (SW_ModelSim->month != SW_ModelSim->prevmonth) {
        SW_ModelSim->newperiod[eSW_Month] =
            (SW_ModelSim->prevmonth != notime) ? swTRUE : swFALSE;
        SW_ModelSim->prevmonth = SW_ModelSim->month;
    } else {
        SW_ModelSim->newperiod[eSW_Month] = swFALSE;
    }

    /*  if (SW_ModelSim.week != prevweek || SW_ModelSim.month == NoMonth) { */
    if (SW_ModelSim->week != SW_ModelSim->prevweek) {
        SW_ModelSim->newperiod[eSW_Week] =
            (SW_ModelSim->prevweek != notime) ? swTRUE : swFALSE;
        SW_ModelSim->prevweek = SW_ModelSim->week;
    } else {
        SW_ModelSim->newperiod[eSW_Week] = swFALSE;
    }
}

/**
@brief Obtain information from domain for one model run based on
user inputted suid

@param[in,out] SW_ModelIn Struct of type SW_MODEL_INPUTS holding basic input
    time information about the simulation
@param[in] SW_Domain Struct of type SW_DOMAIN holding constant
    temporal/spatial information for a set of simulation runs
@param[in] fileNames Input netCDF files
@param[out] LogInfo Holds information on warnings and errors
*/
void SW_MDL_get_ModelRun(
    SW_MODEL_INPUTS *SW_ModelIn,
    SW_DOMAIN *SW_Domain,
    char *fileNames[],
    LOG_INFO *LogInfo
) {

    SW_ModelIn->startyr = SW_Domain->startyr;       // Copy start year
    SW_ModelIn->endyr = SW_Domain->endyr;           // Copy end year
    SW_ModelIn->startstart = SW_Domain->startstart; // Copy start doy
    SW_ModelIn->endend = SW_Domain->endend;         // Copy end doy

    memcpy(
        &SW_ModelIn->SW_SpinUp,
        &SW_Domain->SW_SpinUp,
        sizeof(SW_ModelIn->SW_SpinUp)
    );

    (void) LogInfo;
    (void) fileNames;
}
