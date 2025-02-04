/********************************************************/
/********************************************************/
/*	Source file: Sky.c
 Type: module - used by Weather.c
 Application: SOILWAT - soilwater dynamics simulator
 Purpose: Read / write and otherwise manage the
 information about the sky.
 History:

 (8/28/01) -- INITIAL CODING - cwb

 (10/12/2009) - (drs) added pressure

 01/12/2010	(drs) removed pressure (used for snow sublimation) in
 SW_SKY_read(void) as case 4

 06/16/2010	(drs) all cloud.in input files contain on line 1 cloud cover,
 line 2 wind speed, line 3 rel. humidity, and line 4 transmissivity, but
 SW_SKY_read() was reading rel. humidity from line 1 and cloud cover from line 3
 instead -> SW_SKY_read() is now reading as the input files are formatted

 08/22/2011	(drs) new 5th line in cloud.in containing snow densities
 (kg/m3): read  in SW_SKY_read(void) as case 4

 09/26/2011	(drs) added calls to Times.c:interpolate_monthlyValues() to
 SW_SKY_init() for each monthly input variable

 06/27/2013	(drs)	closed open files if LogError() with LOGERROR is called
 in SW_SKY_read()
 */
/********************************************************/
/********************************************************/


/* =================================================== */
/*                INCLUDES / DEFINES                   */
/* --------------------------------------------------- */
#include "include/SW_Sky.h"         // for SW_SKY_new_year, SW_SKY_read
#include "include/filefuncs.h"      // for CloseFile, GetALine, LogError
#include "include/generic.h"        // for Bool, LOGERROR, swTRUE
#include "include/SW_datastructs.h" // for LOG_INFO, SW_MODEL, SW_SKY
#include "include/SW_Defines.h"     // for MAX_FILENAMESIZE, MAX_MONTHS
#include "include/SW_Files.h"       // for eSky
#include "include/Times.h"          // for isleapyear, interpolate_monthlyV...
#include <stdio.h>                  // for sscanf, FILE

/* =================================================== */
/*             Global Function Definitions             */
/* --------------------------------------------------- */

/**
@brief Reads in file for sky.

@param[in] txtInFiles Array of program in/output files
@param[out] SW_Sky Struct of type SW_SKY which describes sky conditions
    over the simulated site
@param[out] LogInfo Holds information on warnings and errors
*/
void SW_SKY_read(char *txtInFiles[], SW_SKY *SW_Sky, LOG_INFO *LogInfo) {
    /* =================================================== */
    /* 6-Oct-03 (cwb) - all this time I had lines 1 & 3
     *                  switched!
     * 06/16/2010	(drs) all cloud.in input files contain on line 1 cloud
     * cover, line 2 wind speed, line 3 rel. humidity, and line 4
     * transmissivity, but SW_SKY_read() was reading rel. humidity from line 1
     * and cloud cover from line 3 instead -> SW_SKY_read() is now reading as
     * the input files are formatted
     */

    FILE *f;
    int lineno = 0;
    int x = 0;
    int k;
    int index;
    double tmp[MAX_MONTHS];
    char *MyFileName;
    char inbuf[MAX_FILENAMESIZE];
    char tmpStrs[MAX_MONTHS][20] = {{'\0'}};

    MyFileName = txtInFiles[eSky];
    f = OpenFile(MyFileName, "r", LogInfo);
    if (LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

    while (GetALine(f, inbuf, MAX_FILENAMESIZE)) {
        x = sscanf(
            inbuf,
            "%19s %19s %19s %19s %19s %19s %19s %19s %19s %19s %19s %19s",
            tmpStrs[0],
            tmpStrs[1],
            tmpStrs[2],
            tmpStrs[3],
            tmpStrs[4],
            tmpStrs[5],
            tmpStrs[6],
            tmpStrs[7],
            tmpStrs[8],
            tmpStrs[9],
            tmpStrs[10],
            tmpStrs[11]
        );

        if (x != 12) {
            LogError(
                LogInfo,
                LOGERROR,
                "%s : invalid record %d.\n",
                MyFileName,
                lineno
            );
            goto closeFile;
        }

        for (index = 0; index < MAX_MONTHS; index++) {
            tmp[index] = sw_strtod(tmpStrs[index], MyFileName, LogInfo);
            if (LogInfo->stopRun) {
                goto closeFile;
            }
        }

        switch (lineno) {
        case 0:
            for (k = 0; k < MAX_MONTHS; k++) {
                SW_Sky->cloudcov[k] = tmp[k];
            }
            break;
        case 1:
            for (k = 0; k < MAX_MONTHS; k++) {
                SW_Sky->windspeed[k] = tmp[k];
            }
            break;
        case 2:
            for (k = 0; k < MAX_MONTHS; k++) {
                SW_Sky->r_humidity[k] = tmp[k];
            }
            break;
        case 3:
            for (k = 0; k < MAX_MONTHS; k++) {
                SW_Sky->snow_density[k] = tmp[k];
            }
            break;
        case 4:
            for (k = 0; k < MAX_MONTHS; k++) {
                SW_Sky->n_rain_per_day[k] = tmp[k];
            }
            break;

        default:
            LogError(
                LogInfo,
                LOGERROR,
                "%s : too many rows %d.\n",
                MyFileName,
                lineno
            );
            goto closeFile;

            break;
        }

        lineno++;
    }

closeFile: { CloseFile(&f, LogInfo); }
}

/**
@brief Interpolate monthly input values to daily records
(depends on "current" year)

@param[in] SW_Model Struct of type SW_MODEL holding basic time information
    about the simulation
@param[in] snow_density[] Snow density (kg/m3)
@param[out] snow_density_daily[] Interpolated daily snow density (kg/m3)

Note: time must be set with SW_MDL_new_year() or Time_new_year()
prior to this function.
*/
void SW_SKY_new_year(
    SW_MODEL *SW_Model,
    double snow_density[MAX_MONTHS],
    double snow_density_daily[MAX_MONTHS]
) {

    Bool interpAsBase1 = swTRUE;
    TimeInt year = SW_Model->year;

    /* We only need to re-calculate values if this is first year or
       if previous year was different from current year in leap/noleap status
    */

    if (year == SW_Model->startyr || isleapyear(year) != isleapyear(year - 1)) {
        interpolate_monthlyValues(
            snow_density,
            interpAsBase1,
            SW_Model->cum_monthdays,
            SW_Model->days_in_month,
            snow_density_daily
        );
    }
}

/**
@brief Validate mean monthly climate values

Note: This function does not check r_humidity, cloudcov, and windspeed;
these variables are checked by checkAllWeather() at a daily time step.

@param[in] SW_Sky Struct of type SW_SKY which describes sky conditions
    of the simulated site
@param[out] LogInfo Holds information on warnings and errors
*/
void checkSky(SW_SKY *SW_Sky, LOG_INFO *LogInfo) {
    unsigned int mon;

    for (mon = 0; mon < MAX_MONTHS; mon++) {
        if (SW_Sky->snow_density[mon] <= 0) {
            LogError(
                LogInfo,
                LOGERROR,
                "snowdensity (%.4f) is zero or negative in month %d.",
                SW_Sky->snow_density[mon],
                mon + 1
            );
            return;
        }

        if (SW_Sky->n_rain_per_day[mon] <= 0) {
            LogError(
                LogInfo,
                LOGERROR,
                "n_rain_per_day (%.4f) is zero or negative in month %d.",
                SW_Sky->n_rain_per_day[mon],
                mon + 1
            );
            return;
        }
    }
}

void SW_SKY_init_run(SW_SKY *SW_Sky, LOG_INFO *LogInfo) {
    checkSky(SW_Sky, LogInfo);
}
