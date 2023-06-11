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

#include "include/SW_datastructs.h"

#ifdef __cplusplus
extern "C" {
#endif


/* =================================================== */
/*             Global Function Declarations            */
/* --------------------------------------------------- */
void SW_PET_init_run(SW_ATMD *SW_AtmDem);

double sun_earth_distance_squaredinverse(unsigned int doy);
double solar_declination(unsigned int doy);
double sunset_hourangle(double lat, double declin);

void sun_hourangles(SW_ATMD* SW_AtmDem, unsigned int doy, double lat,
  double slope, double aspect, double sun_angles[], double int_cos_theta[],
  double int_sin_beta[]);

void solar_radiation_extraterrestrial(double memoized_G_o[][TWO_DAYS],
  unsigned int doy, double int_cos_theta[], double G_o[]);

double overcast_attenuation_KastenCzeplak1980(double cloud_cover);
double overcast_attenuation_Angstrom1924(double sunshine_fraction);
double clearsky_directbeam(double P, double e_a, double int_sin_beta);
double clearnessindex_diffuse(double K_b);
double actual_horizontal_transmissivityindex(double tau);

double solar_radiation(
  SW_ATMD *SW_AtmDem, unsigned int doy, double lat,
  double elev, double slope, double aspect,
  double albedo, double *cloud_cover, double e_a,
  double rsds, unsigned int desc_rsds,
  double *H_oh, double *H_ot, double *H_gh,
  LOG_INFO* LogInfo
);


double blackbody_radiation(double T);


double petfunc(double H_g, double avgtemp, double elev, double reflec,
            double humid, double windsp, double cloudcov, LOG_INFO* LogInfo);

double svp(double T, double *slope_svp_to_t);
double svp2(double temp);
double atmospheric_pressure(double elev);
double psychrometric_constant(double pressure);

double actualVaporPressure1(double hurs, double meanTemp);
double actualVaporPressure2(double maxHurs, double minHurs, double maxTemp, double minTemp);
double actualVaporPressure3(double dewpointTemp);

#ifdef __cplusplus
}
#endif

#endif
