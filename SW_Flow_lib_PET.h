/********************************************************/
/********************************************************/
/*
 Source file: SW_Flow_lib_PET.h
 Type: header
 Application: SOILWAT2 - soilwater dynamics simulator
*/
/********************************************************/
/********************************************************/

#ifndef SW_PET_H
#define SW_PET_H

#ifdef __cplusplus
extern "C" {
#endif


/* =================================================== */
/* =================================================== */
/*                Function Definitions                 */
/* --------------------------------------------------- */

double petfunc(unsigned int doy, double avgtemp, double rlat, double elev, double slope, double aspect, double reflec, double humid, double windsp, double cloudcov,
		double transcoeff);

double svapor(double temp);
double solar_declination(unsigned int doy);
double sunset_hourangle(double rlat, double declin);
double solar_radiation_TOA(double rlat, double slope, double aspect, double ahou, double declin);
double solar_radiation_surface(double Rs, double sunshine_fraction);
double blackbody_radiation(double T);

double slope_svp_to_t(double es_at_tmean, double tmean);
double atmospheric_pressure(double elev);
double psychrometric_constant(double pressure);

#ifdef __cplusplus
}
#endif

#endif
