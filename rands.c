#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <time.h>
#include "generic.h"
#include "rands.h"
#include "myMemory.h"
#include "filefuncs.h"

#include "pcg/pcg_basic.h"


#ifndef RSOILWAT
  uint64_t stream = 1u; //stream id. this is given out to a pcg_rng then incremented.

#else
  // R-API requires that we use it's own random number implementation
  // see https://cran.r-project.org/doc/manuals/R-exts.html#Writing-portable-packages
  // and https://cran.r-project.org/doc/manuals/R-exts.html#Random-numbers
  #include <R_ext/Random.h> // for the random number generators
  #include <Rmath.h> // for rnorm()
#endif





/*****************************************************/
/**
  \brief Sets the random number seed.

  \param seed The initial state of the system; if 0 then use system time.
  \param[in,out] pcg_rng The random number generator to set.

  \note If using this function with STEPWAT2, then call RandSeed() only once
    per iteration.

  \sideeffect Increment the stream so that no two generators have the same
    sequence.
*/
void RandSeed(signed long seed, pcg32_random_t* pcg_rng) {
//we don't need to set a random seed if RSOILWAT is used
#ifndef RSOILWAT

  if (seed == 0) {
    //seed with a random value. Uses the system time to generate
    //a pseudo-random seed.
    pcg32_srandom_r(pcg_rng, time(NULL), stream);
  }
  else {
    //Seed with a specific value.
    pcg32_srandom_r(pcg_rng, (int) seed, stream);
  }

  //Increment the stream so no two generators have the same sequence.
  stream++;

#else
  // silence compile warnings [-Wunused-parameter]
  if (pcg_rng == NULL && seed > 0) {}

#endif
}



/*****************************************************/
/**
  \brief A pseudo-random number from the uniform distribution.

  \param[in,out] *pcg_rng The random number generator to use.

  @return A pseudo-random number between 0 and 1.
*/
double RandUni(pcg32_random_t* pcg_rng) {

  double res;

  #ifndef RSOILWAT
    // get a random double and bit shift is 32 bits (less that 1)
    res = ldexp(pcg32_random_r(pcg_rng), -32);

  #else
    if (pcg_rng == NULL) {} // silence compile warnings [-Wunused-parameter]

    GetRNGstate();
    res = unif_rand();
    PutRNGstate();
  #endif

  return res;
}



/*****************************************************/
/**
	\brief A pseudo-random number from a bounded uniform distribution.

	\param first One bound of the range between two numbers.
	\param last One bound of the range between two numbers.
  \param[in,out] *pcg_rng The random number generator to use.

	\return Random number between the two bounds defined.
*/
int RandUniIntRange(const long first, const long last, pcg32_random_t* pcg_rng) {
/* History:
	Return a randomly selected integer between
	first and last, inclusive.

	cwb - 12/5/00

	cwb - 12/8/03 - just noticed that the previous
	version only worked with positive numbers
	and when first < last.  Now it works with
	negative numbers as well as reversed order.

	Examples:
	- first = 1, last = 10, result = 6
	- first = 5, last = -1, result = 2
	- first = -5, last = 5, result = 0
*/

  long f, l, res;

  if (first == last)
    return first;

  if (first > last) {
    l = first + 1;
    f = last;
  } else {
    f = first;
    l = last + 1;
  }

  #ifndef RSOILWAT
    long r = l - f;
    //pcg32_boundedrand_r returns a random number between 0 and r.
    res = pcg32_boundedrand_r(pcg_rng, r) + f;

  #else
    if (pcg_rng == NULL) {} // silence compile warnings [-Wunused-parameter]

    GetRNGstate();
    res = (long) runif(f, l);
    PutRNGstate();
  #endif

  return res;
}



/*****************************************************/
/**
	\brief Generate a random float between two numbers.

	\param min One bound of the range between two numbers.
	\param max One bound of the range between two numbers.
  \param[in,out] *pcg_rng The random number generator to use.

	\return Random number between first and last, inclusive.
*/
float RandUniFloatRange(const float min, const float max, pcg32_random_t* pcg_rng) {
/* History:

	cwb - 12/5/00

	cwb - 12/8/03 - just noticed that the previous
	version only worked with positive numbers
	and when first < last.  Now it works with
	negative numbers as well as reversed order.

	Examples:
	- first = .2, last = .7, result = .434
	- first = 4.5, last = -1.1, result = -.32
	- first = -5, last = 5, result = 0
*/
	float f, l, r;

	if (max == min)
		return min;

	if (min > max) {
		l = min;
		f = max;
	} else {
		f = min;
		l = max;
	}

	r = l - f;

	return (float) (RandUni(pcg_rng) * r) + f;
}

/*****************************************************/
/**
	 \brief Create a list of random integers from a uniform distribution.

	 There are count
	 numbers put into the list array, and
	 the numbers fall between first and last,
	 inclusive.  The numbers are non-repeating,
	 and not necessarily ordered.  This method
	 only works for a uniform distribution, but
	 it should be fast for any number of count items.

	 cwb - 6/27/00

	\param count Number of values to generate.
	\param first Lower bound of the values.
	\param last Upper bound of the values.
	\param[out] list Upon return this array will be filled with random values.
  \param[in,out] *pcg_rng The random number generator to use.
*/
void RandUniList(long count, long first, long last, RandListType list[], pcg32_random_t* pcg_rng) {

	long i, j, c, range, *klist;

	range = last - first + 1;

	if (count > range || range <= 0) {
    sw_error(-1, "Error in RandUniList: count > range || range <= 0\n");
	}

	/* if count == range for some weird reason, just
	 fill the array and return */
	if (count == range) {
		for (i = 0; i < count; i++)
			list[i] = first + i;
		return;
	}

	/* if count <= 2, handle things directly */
	/* for less complexity and more speed    */
	if (count <= 2) {
		list[0] = (long) RandUniIntRange(first, last, pcg_rng);

		if (count == 2) {
			while ((list[1] = RandUniIntRange(first, last, pcg_rng)) == list[0]);
		}
		return;
	}

	/* otherwise, go through the whole groovy algorithm */

	/* allocate space for the temp list */
	klist = (long *) Mem_Malloc(sizeof(long) * range, "RandUniList");

	/* populate the list with valid numbers */
	for (i = 0, j = first; j <= last; klist[i++] = j++)
		;

	/* now randomize the list */
	for (i = 0; i < range; i++) {
		while ((j = RandUniIntRange(0, range - 1, pcg_rng)) == i)
			;
		c = klist[i];
		klist[i] = klist[j];
		klist[j] = c;
	}

	/* remove count items from the top of the */
	/* shuffled list */
	for (i = 0; i < count; i++) {
		list[i] = klist[i];
	}

	Mem_Free(klist);
}

/*****************************************************/
/**
	 \brief A pseudo-random number from a normal distribution.

	 This routine is
	 adapted from FUNCTION GASDEV in
	 Press, et al., 1986, Numerical Recipes,
	 p203, Press Syndicate, NY.
	 To reset the random number sequence,
	 set _randseed to any negative number
	 prior to calling any function, that
	 depends on RandUni().

	\param mean The mean of the distribution
	\param stddev Standard deviation of the distribution
  \param[in,out] *pcg_rng The random number generator to use.
*/
double RandNorm(double mean, double stddev, pcg32_random_t* pcg_rng) {
/* History:
	 cwb - 6/20/00
	 cwb - 09-Dec-2002 -- FINALLY noticed that
	 gasdev and gset have to be static!
	 might as well set the others.
*/
	double res;

	#ifndef RSOILWAT
		static short set = 0;

		static double v1, v2, r, fac, gset, gasdev;

		if (!set) {
			do {
				v1 = 2.0 * RandUni(pcg_rng) - 1.0;
				v2 = 2.0 * RandUni(pcg_rng) - 1.0;
				r = v1 * v1 + v2 * v2;
			} while (r >= 1.0);
			fac = sqrt(-2.0 *log(r)/r);
			gset = v1 * fac;
			gasdev = v2 * fac;
			set = 1;
		} else {
			gasdev = gset;
			set = 0;
		}

		res = mean + gasdev * stddev;

	#else
		if (pcg_rng == NULL) {} // silence compile warnings [-Wunused-parameter]

		GetRNGstate();
		res = rnorm(mean, stddev);
		PutRNGstate();

	#endif

	return res;
}

/**
  \brief Generate a beta random variate.

  The beta distribution has two shape parameters \f$a\f$ and \f$b\f$.
  The density is
      \f[\frac{x ^ (a - 1) * (1 - x) ^ (b - 1)}{Beta(a, b)}\f]
  for \f$0 < x < 1\f$

	The code for RandBeta was taken from ranlib, a FORTRAN77 library. Original
	FORTRAN77 version by Barry Brown, James Lovato. C version by John Burkardt.
	\cite Cheng1978

	This code is distributed under the GNU LGPL license.

  [More info can be found here](http://people.sc.fsu.edu/~jburkardt/f77_src/ranlib/ranlib.html)

  \param aa The first shape parameter of the beta distribution with
         \f$0.0 < aa\f$.
  \param bb The second shape parameter of the beta distribution with
         \f$0.0 < bb\f$.
  \param[in,out] *pcg_rng The random number generator to use.
  \return A random variate of a beta distribution.
*/
float RandBeta ( float aa, float bb, pcg32_random_t* pcg_rng) {
  float a;
  float alpha;
  float b;
  float beta;
  float delta;
  float gamma;
  float k1;
  float k2;
  const float log4 = 1.3862943611198906188;
  const float log5 = 1.6094379124341003746;
  float r;
  float s;
  float t;
  float u1;
  float u2;
  float v;
  float value;
  float w;
  float y;
  float z;

  if ( aa <= 0.0 ) {
    sw_error(1, "RandBeta - Fatal error: AA <= 0.0\n");
  }

  if ( bb <= 0.0 ) {
    sw_error(1, "RandBeta - Fatal error: BB <= 0.0\n");
  }

  //------  Algorithm BB
  if ( 1.0 < aa && 1.0 < bb ) {
    a = min( aa, bb );
    b = max( aa, bb );
    alpha = a + b;
    beta = sqrt( ( alpha - 2.0 ) / ( 2.0 * a * b - alpha ) );
    gamma = a + 1.0 / beta;

    for ( ; ; ) {
      u1 = RandUni(pcg_rng);
      u2 = RandUni(pcg_rng);

      v = beta * log ( u1 / ( 1.0 - u1 ) );
      // exp ( v ) replaced by r4_exp ( v )
      w = a * exp ( v );

      z = u1 * u1 * u2;
      r = gamma * v - log4;
      s = a + r - w;

      if ( 5.0 * z <= s + 1.0 + log5 ) {
        break;
      }

      t = log ( z );
      if ( t <= s ) {
        break;
      }

      if ( t <= ( r + alpha * log ( alpha / ( b + w ) ) ) ) {
        break;
      }
    }
  }


  //------  Algorithm BC
  else {
    a = max( aa, bb );
    b = min( aa, bb );
    alpha = a + b;
    beta = 1.0 / b;
    delta = 1.0 + a - b;
    k1 = delta * ( 1.0 / 72.0 + b / 24.0 ) / ( a / b - 7.0 / 9.0 );
    k2 = 0.25 + ( 0.5 + 0.25 / delta ) * b;

    for ( ; ; ) {
      u1 = RandUni(pcg_rng);
      u2 = RandUni(pcg_rng);

      if ( u1 < 0.5 ) {
        y = u1 * u2;
        z = u1 * y;

        if ( k1 <= 0.25 * u2 + z - y ) {
          continue;
        }
      }

      else {
        z = u1 * u1 * u2;

        if ( z <= 0.25 ) {
          v = beta * log ( u1 / ( 1.0 - u1 ) );
          w = a * exp ( v );

          if ( aa == a ) {
            value = w / ( b + w );
          }
          else {
            value = b / ( b + w );
          }

          return value;
        }

        if ( k2 < z ) {
          continue;
        }
      }

      v = beta * log( u1 / ( 1.0 - u1 ) );
      w = a * exp( v );

      if ( log( z ) <= alpha * ( log( alpha / ( b + w ) ) + v ) - log4 ) {
        break;
      }
    }
  }

  if ( aa == a ) {
    value = w / ( b + w );
  }
  else {
    value = b / ( b + w );
  }

  return value;
}
