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
/* =================================================== */
/*                INCLUDES / DEFINES                   */

#include <stdio.h>
#include <stdlib.h>

#include "include/filefuncs.h"
#include "include/generic.h"
#include "include/SW_Files.h"
#include "include/SW_Model.h"
#include "include/SW_Sky.h"
#include "include/Times.h"

/* =================================================== */
/*             Global Function Definitions             */
/* --------------------------------------------------- */

/**
@brief Reads in file for sky.

@param[in] InFiles Array of program in/output files
@param[out] SW_Sky Struct of type SW_SKY which describes sky conditions
    over the simulated site
@param[out] LogInfo Holds information on warnings and errors
*/
void SW_SKY_read(char *InFiles[], SW_SKY *SW_Sky, LOG_INFO *LogInfo) {
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
    int lineno = 0, x = 0;
    char *MyFileName, inbuf[MAX_FILENAMESIZE];

    MyFileName = InFiles[eSky];
    f = OpenFile(MyFileName, "r", LogInfo);
    if (LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

    while (GetALine(f, inbuf, MAX_FILENAMESIZE)) {
        switch (lineno) {
        case 0:
            x = sscanf(
                inbuf,
                "%lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf",
                &SW_Sky->cloudcov[0],
                &SW_Sky->cloudcov[1],
                &SW_Sky->cloudcov[2],
                &SW_Sky->cloudcov[3],
                &SW_Sky->cloudcov[4],
                &SW_Sky->cloudcov[5],
                &SW_Sky->cloudcov[6],
                &SW_Sky->cloudcov[7],
                &SW_Sky->cloudcov[8],
                &SW_Sky->cloudcov[9],
                &SW_Sky->cloudcov[10],
                &SW_Sky->cloudcov[11]
            );
            break;
        case 1:
            x = sscanf(
                inbuf,
                "%lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf",
                &SW_Sky->windspeed[0],
                &SW_Sky->windspeed[1],
                &SW_Sky->windspeed[2],
                &SW_Sky->windspeed[3],
                &SW_Sky->windspeed[4],
                &SW_Sky->windspeed[5],
                &SW_Sky->windspeed[6],
                &SW_Sky->windspeed[7],
                &SW_Sky->windspeed[8],
                &SW_Sky->windspeed[9],
                &SW_Sky->windspeed[10],
                &SW_Sky->windspeed[11]
            );
            break;
        case 2:
            x = sscanf(
                inbuf,
                "%lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf",
                &SW_Sky->r_humidity[0],
                &SW_Sky->r_humidity[1],
                &SW_Sky->r_humidity[2],
                &SW_Sky->r_humidity[3],
                &SW_Sky->r_humidity[4],
                &SW_Sky->r_humidity[5],
                &SW_Sky->r_humidity[6],
                &SW_Sky->r_humidity[7],
                &SW_Sky->r_humidity[8],
                &SW_Sky->r_humidity[9],
                &SW_Sky->r_humidity[10],
                &SW_Sky->r_humidity[11]
            );
            break;
        case 3:
            x = sscanf(
                inbuf,
                "%lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf",
                &SW_Sky->snow_density[0],
                &SW_Sky->snow_density[1],
                &SW_Sky->snow_density[2],
                &SW_Sky->snow_density[3],
                &SW_Sky->snow_density[4],
                &SW_Sky->snow_density[5],
                &SW_Sky->snow_density[6],
                &SW_Sky->snow_density[7],
                &SW_Sky->snow_density[8],
                &SW_Sky->snow_density[9],
                &SW_Sky->snow_density[10],
                &SW_Sky->snow_density[11]
            );
            break;
        case 4:
            x = sscanf(
                inbuf,
                "%lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf",
                &SW_Sky->n_rain_per_day[0],
                &SW_Sky->n_rain_per_day[1],
                &SW_Sky->n_rain_per_day[2],
                &SW_Sky->n_rain_per_day[3],
                &SW_Sky->n_rain_per_day[4],
                &SW_Sky->n_rain_per_day[5],
                &SW_Sky->n_rain_per_day[6],
                &SW_Sky->n_rain_per_day[7],
                &SW_Sky->n_rain_per_day[8],
                &SW_Sky->n_rain_per_day[9],
                &SW_Sky->n_rain_per_day[10],
                &SW_Sky->n_rain_per_day[11]
            );
            break;
        }

        if (x < 12) {
            CloseFile(&f, LogInfo);
            LogError(
                LogInfo,
                LOGERROR,
                "%s : invalid record %d.\n",
                MyFileName,
                lineno
            );
            return; // Exit function prematurely due to error
        }

        x = 0;
        lineno++;
    }

    CloseFile(&f, LogInfo);
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
    RealD snow_density[MAX_MONTHS],
    RealD snow_density_daily[MAX_MONTHS]
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
