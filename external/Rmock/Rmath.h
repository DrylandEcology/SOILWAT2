/* Mocking Rmath header
  Goal: standalone compile SOILWAT2 library for rSOILWAT2 for test purposes
*/

#ifndef RMATH_RSOILWAT_H
#define RMATH_RSOILWAT_H

#ifdef __cplusplus
extern "C" {
#endif

double	rnorm(double, double);
double	runif(double, double);
double	unif_rand(void);

#ifdef __cplusplus
}
#endif

#endif
