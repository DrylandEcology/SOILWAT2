/* Mocking R header
  Goal: standalone compile SOILWAT2 library for rSOILWAT2 for test purposes
*/

#ifndef R_RSOILWAT_H
#define R_RSOILWAT_H

#ifdef __cplusplus
extern "C" {
#endif

// From <R_ext/Print.h>
void Rprintf(const char *, ...);

// From <R_ext/Random.h>
void GetRNGstate(void);
void PutRNGstate(void);

// From <R_ext/Error.h>
void error(const char *, ...);
void warning(const char *, ...);

#ifdef __cplusplus
}
#endif

#endif
