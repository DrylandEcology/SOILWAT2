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
 01/12/2010	(drs) removed pressure (used for snow sublimation) in SW_SKY_read(void) as case 4
 06/16/2010	(drs) all cloud.in input files contain on line 1 cloud cover, line 2 wind speed, line 3 rel. humidity, and line 4 transmissivity, but SW_SKY_read() was reading rel. humidity from line 1 and cloud cover from line 3 instead -> SW_SKY_read() is now reading as the input files are formatted
 08/22/2011	(drs) new 5th line in cloud.in containing snow densities (kg/m3): read  in SW_SKY_read(void) as case 4
 09/26/2011	(drs) added calls to Times.c:interpolate_monthlyValues() to SW_SKY_init() for each monthly input variable
 06/27/2013	(drs)	closed open files if LogError() with LOGFATAL is called in SW_SKY_read()
 */
/********************************************************/
/********************************************************/

/* =================================================== */
/* =================================================== */
/*                INCLUDES / DEFINES                   */

#include <stdio.h>
#include <stdlib.h>
#include "include/generic.h"
#include "include/Times.h"
#include "include/filefuncs.h"
#include "include/SW_Defines.h"
#include "include/SW_Files.h"
#include "include/SW_Model.h" // externs SW_Model
#include "include/SW_Sky.h"
#include "include/SW_Weather.h"  // externs SW_Weather



/* =================================================== */
/*                  Global Variables                   */
/* --------------------------------------------------- */

SW_SKY SW_Sky;


/* =================================================== */
/*                  Local Variables                    */
/* --------------------------------------------------- */
static char *MyFileName;



/* =================================================== */
/*             Global Function Definitions             */
/* --------------------------------------------------- */

/**
@brief Reads in file for sky.
*/
void SW_SKY_read(void) {
	/* =================================================== */
	/* 6-Oct-03 (cwb) - all this time I had lines 1 & 3
	 *                  switched!
	 * 06/16/2010	(drs) all cloud.in input files contain on line 1 cloud cover, line 2 wind speed, line 3 rel. humidity, and line 4 transmissivity, but SW_SKY_read() was reading rel. humidity from line 1 and cloud cover from line 3 instead -> SW_SKY_read() is now reading as the input files are formatted
	 */
	SW_SKY *v = &SW_Sky;
	FILE *f;
	int lineno = 0, x = 0;

	MyFileName = SW_F_name(eSky);
	f = OpenFile(MyFileName, "r");

	while (GetALine(f, inbuf)) {
		switch (lineno) {
		case 0:
			x = sscanf(inbuf, "%lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf", &v->cloudcov[0], &v->cloudcov[1], &v->cloudcov[2], &v->cloudcov[3], &v->cloudcov[4],
					&v->cloudcov[5], &v->cloudcov[6], &v->cloudcov[7], &v->cloudcov[8], &v->cloudcov[9], &v->cloudcov[10], &v->cloudcov[11]);
			break;
		case 1:
			x = sscanf(inbuf, "%lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf", &v->windspeed[0], &v->windspeed[1], &v->windspeed[2], &v->windspeed[3], &v->windspeed[4],
					&v->windspeed[5], &v->windspeed[6], &v->windspeed[7], &v->windspeed[8], &v->windspeed[9], &v->windspeed[10], &v->windspeed[11]);
			break;
		case 2:
			x = sscanf(inbuf, "%lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf", &v->r_humidity[0], &v->r_humidity[1], &v->r_humidity[2], &v->r_humidity[3], &v->r_humidity[4],
					&v->r_humidity[5], &v->r_humidity[6], &v->r_humidity[7], &v->r_humidity[8], &v->r_humidity[9], &v->r_humidity[10], &v->r_humidity[11]);
			break;
		case 3:
			x = sscanf(inbuf, "%lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf", &v->snow_density[0], &v->snow_density[1], &v->snow_density[2], &v->snow_density[3],
					&v->snow_density[4], &v->snow_density[5], &v->snow_density[6], &v->snow_density[7], &v->snow_density[8], &v->snow_density[9], &v->snow_density[10],
					&v->snow_density[11]);
			break;
		case 4:
      x = sscanf(inbuf, "%lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf",
          &v->n_rain_per_day[0], &v->n_rain_per_day[1], &v->n_rain_per_day[2],
          &v->n_rain_per_day[3], &v->n_rain_per_day[4], &v->n_rain_per_day[5],
          &v->n_rain_per_day[6], &v->n_rain_per_day[7], &v->n_rain_per_day[8],
          &v->n_rain_per_day[9], &v->n_rain_per_day[10], &v->n_rain_per_day[11]);
			break;
		}

		if (x < 12) {
			CloseFile(&f);
			snprintf(errstr, MAX_ERROR, "%s : invalid record %d.\n", MyFileName, lineno);
			LogError(logfp, LOGFATAL, errstr);
		}

		x = 0;
		lineno++;
	}

	CloseFile(&f);
}

/**
  @brief Interpolate monthly input values to daily records
  (depends on "current" year)

  Note: time must be set with SW_MDL_new_year() or Time_new_year()
  prior to this function.
*/
void SW_SKY_new_year(void) {
	TimeInt year = SW_Model.year;
	SW_SKY *v = &SW_Sky;
    Bool interpAsBase1 = swTRUE;

  /* We only need to re-calculate values if this is first year or
     if previous year was different from current year in leap/noleap status
  */

  if (year == SW_Model.startyr || isleapyear(year) != isleapyear(year - 1)) {
    interpolate_monthlyValues(v->snow_density, interpAsBase1, v->snow_density_daily);
  }
}
