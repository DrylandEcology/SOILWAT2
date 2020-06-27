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

void SW_PET_init_run(void);

double sun_earth_distance_squaredinverse(unsigned int doy);
double solar_declination(unsigned int doy);
double sunset_hourangle(double lat, double declin);

void sun_hourangles(unsigned int doy, double lat, double slope, double aspect,
  double sun_angles[], double int_cos_theta[], double int_sin_beta[]);

void solar_radiation_extraterrestrial(unsigned int doy, double int_cos_theta[],
  double G_o[]);

double atmospheric_attenuation_cloudfactor(double cloud_cover);
double clearsky_directbeam(double P, double e_a, double int_sin_beta);
double clearnessindex_diffuse(double K_b);

double solar_radiation(unsigned int doy,
  double lat, double elev, double slope, double aspect,
  double albedo, double cloud_cover, double rel_humidity, double air_temp_mean,
  double *H_oh, double *H_ot, double *H_gh);


double blackbody_radiation(double T);


double petfunc(double H_g, double avgtemp, double elev,
  double reflec, double humid, double windsp, double cloudcov);

double svapor(double temp);
double slope_svp_to_t(double es_at_tmean, double tmean);
double atmospheric_pressure(double elev);
double psychrometric_constant(double pressure);

#ifdef __cplusplus
}
#endif

#endif
