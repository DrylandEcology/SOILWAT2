#include <cmath>
#include "gtest/gtest.h"


#define length(array) (sizeof(array) / sizeof(*(array))) //get length of an array

const double tol3 = 1e-3, tol6 = 1e-6, tol9 = 1e-9;


/* SOILWAT2's macro `missing` uses `isfinite` which is C99; however,
   unit tests are compiled with C++ and the corresponding
   `std::isfinite` is C++11
   -> avoiding "error: use of undeclared identifier 'isfinite';
      did you mean 'std::isfinite'?"
*/
#undef missing
#define missing(x)  ( EQ( fabs( (x) ), SW_MISSING ) || !std::isfinite( (x) ) )


/* Functions for unit tests */

void Reset_SOILWAT2_after_UnitTest(void);

void create_test_soillayers(unsigned int nlayers);
