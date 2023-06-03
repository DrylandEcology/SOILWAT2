#include <cmath>
#include "gtest/gtest.h"
#include "include/SW_datastructs.h"


#define length(array) (sizeof(array) / sizeof(*(array))) //get length of an array

static const double
  tol0 = 1e-0, tol1 = 1e-1, tol2 = 1e-2, tol3 = 1e-3, tol6 = 1e-6, tol9 = 1e-9;


/* SOILWAT2's macro `missing` uses `isfinite` which is C99; however,
   unit tests are compiled with C++ and the corresponding
   `std::isfinite` is C++11
   -> avoiding "error: use of undeclared identifier 'isfinite';
      did you mean 'std::isfinite'?"
*/
#undef missing
#define missing(x)  ( EQ( fabs( (x) ), SW_MISSING ) || !std::isfinite( (x) ) )

extern SW_ALL SW_All;
extern SW_OUTPUT_POINTERS SW_OutputPtrs;
extern LOG_INFO LogInfo;
extern PATH_INFO PathInfo;

/* Functions for unit tests */

void Reset_SOILWAT2_after_UnitTest(void);

void create_test_soillayers(unsigned int nlayers, char *InFiles[]);
