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

long _randseed = 0L;
uint64_t stream = 1u;//stream id. this is given out to a pcg_rng then incremented.

#ifdef RSOILWAT
  #include <R_ext/Random.h> // for the random number generators
  #include <Rmath.h> // for rnorm()
#endif


/*****************************************************/
/**
  \brief Sets the random number seed. This should only be done ONCE per iteration.

  \param Seed is the initial state of the system. 0 indicates a random seed.
  \param pcg_rng is the random number generator to set.

*/
void RandSeed(signed long seed, pcg32_random_t* pcg_rng) {

  if(seed == 0){
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

#ifndef RSOILWAT
#if RAND_FAST
  srand(labs(_randseed));
#endif
#endif

}

#define BUCKETSIZE 97

/*****************************************************/
/**
  Random number generator using the specified rng.

  \parameters: pcg32_random_t* that specifies which stream to use.
 
  \returns: a double between 0 and 1.
*/
double RandUni(pcg32_random_t* pcg_rng) {
  // get a random unsigned int and cast it to a signed int.
  int number = (int) pcg32_random_r(pcg_rng);
  // negative values are possible. This takes the absolute value.
  number = labs(number); 
//printf("%f\n", number/(double)RAND_MAX);
  // divide by RAND_MAX to get a value between 0 and 1.
  return (double) number / RAND_MAX;
}

/*****************************************************/
/**
	\fn int RandUniIntRange(const long first, const long last)
	\brief Generate a random integer between two numbers.

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

	\param first. One bound of the range between two numbers. A const long argument.
	\param last. One bound of the range between two numbers. A const long argument.
	\param pcg_rng. random number generator to use.

	\return integer. Random number between the two bounds defined.
*/
int RandUniIntRange(const long first, const long last, pcg32_random_t* pcg_rng) {
	long f, l, r;

	if (first == last)
		return first;

	if (first > last) {
		l = first;
		f = last;
	} else {
		f = first;
		l = last;
	}

  r = l - f + 1;
  //pcg32_boundedrand_r returns a random number between 0 and r.
	return (long) pcg32_boundedrand_r(pcg_rng, r) + f;
}

/*****************************************************/
/**
	\fn int RandUniIntRange(const long first, const long last)
	\brief Generate a random integer between two numbers.

	Return a randomly selected integer between
	first and last, inclusive.

	cwb - 12/5/00

	cwb - 12/8/03 - just noticed that the previous
	version only worked with positive numbers
	and when first < last.  Now it works with
	negative numbers as well as reversed order.

	Examples:
	- first = .2, last = .7, result = .434
	- first = 4.5, last = -1.1, result = -.32
	- first = -5, last = 5, result = 0

	\param min. One bound of the range between two numbers. A const float argument.
	\param max. One bound of the range between two numbers. A const float argument.
	\param pcg_rng. random number generator to use.

	\return float. Random number between the two bounds defined.
*/
float RandUniFloatRange(const float min, const float max, pcg32_random_t* pcg_rng) {
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
	 create a list of random integers from
	 uniform distribution.  There are count
	 numbers put into the list array, and
	 the numbers fall between first and last,
	 inclusive.  The numbers are non-repeating,
	 and not necessarily ordered.  This method
	 only works for a uniform distribution, but
	 it should be fast for any number of count items.

	 cwb - 6/27/00

	\param count. number of values to generate.
	\param first. lower bound of the values.
	\param last. upper bound of the values.
	\param list. Upon return this array will be filled with random values.
	\param pcg_rng. random number generator to pull values from.
*/
void RandUniList(long count, long first, long last, RandListType list[], pcg32_random_t* pcg_rng) {

	long i, j, c, range, *klist;

	range = last - first + 1;

	if (count > range || range <= 0) {
    sw_error(-1, "Programmer error in RandUniList: count > range || range <= 0\n");
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
		if (count == 2)
			while ((list[1] = RandUniIntRange(first, last, pcg_rng)) == list[0]);
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
	 return a random number from
	 normal distribution with mean and stddev
	 characteristics supplied by the user.
	 This routine is
	 adapted from FUNCTION GASDEV in
	 Press, et al., 1986, Numerical Recipes,
	 p203, Press Syndicate, NY.
	 To reset the random number sequence,
	 set _randseed to any negative number
	 prior to calling any function, that
	 depends on RandUni().

	 cwb - 6/20/00
	 cwb - 09-Dec-2002 -- FINALLY noticed that
	 gasdev and gset have to be static!
	 might as well set the others.

	\param mean. the mean of the distribution
	\param stddev. standard deviation of the distribution
	\param pcg_rng. the random number generator to pull values from
*/
double RandNorm(double mean, double stddev, pcg32_random_t* pcg_rng) {

	double y;

	#ifdef RSOILWAT
		GetRNGstate();
		y = rnorm(mean, stddev);
		PutRNGstate();

	#else
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

		y = mean + gasdev * stddev;
	#endif

	return y;
}

/**
  \fn float RandBeta ( float aa, float bb )
  \brief Generates a beta random variate.

  RandBeta returns a single random variate from the beta distribution
  with shape parameters a and b. The density is
      x^(a-1) * (1-x)^(b-1) / Beta(a,b) for 0 < x < 1

	The code for RandBeta was taken from ranlib, a FORTRAN77 library. Original
	FORTRAN77 version by Barry Brown, James Lovato. C version by John Burkardt.
	\cite Cheng1978

	This code is distributed under the GNU LGPL license.

  [More info can be found here](http://people.sc.fsu.edu/~jburkardt/f77_src/ranlib/ranlib.html)

  \param aa. The first shape parameter of the beta distribution with 0.0 < aa.
  \param bb. The second shape parameter of the beta distribution with 0.0 < bb.
  \param pcg_rng. the random number generator to pull values from.
  \return A random variate of a beta distribution.
*/
float RandBeta ( float aa, float bb, pcg32_random_t* pcg_rng)
{
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

  if ( aa <= 0.0 )
  {
    sw_error(1, "GENBET - Fatal error: AA <= 0.0\n");
  }

  if ( bb <= 0.0 )
  {
    sw_error(1, "GENBET - Fatal error: BB <= 0.0\n");
  }
/*
  Algorithm BB
*/
  if ( 1.0 < aa && 1.0 < bb )
  {
    a = min ( aa, bb );
    b = max ( aa, bb );
    alpha = a + b;
    beta = sqrt ( ( alpha - 2.0 ) / ( 2.0 * a * b - alpha ) );
    gamma = a + 1.0 / beta;

    for ( ; ; )
    {
      u1 = RandUni (pcg_rng);
      u2 = RandUni (pcg_rng);
      v = beta * log ( u1 / ( 1.0 - u1 ) );
/*
  exp ( v ) replaced by r4_exp ( v )
*/
      w = a * exp ( v );

      z = u1 * u1 * u2;
      r = gamma * v - log4;
      s = a + r - w;

      if ( 5.0 * z <= s + 1.0 + log5 )
      {
        break;
      }

      t = log ( z );
      if ( t <= s )
      {
        break;
      }

      if ( t <= ( r + alpha * log ( alpha / ( b + w ) ) ) )
      {
        break;
      }
    }
  }
/*
  Algorithm BC
*/
  else
  {
    a = max ( aa, bb );
    b = min ( aa, bb );
    alpha = a + b;
    beta = 1.0 / b;
    delta = 1.0 + a - b;
    k1 = delta * ( 1.0 / 72.0 + b / 24.0 )
      / ( a / b - 7.0 / 9.0 );
    k2 = 0.25 + ( 0.5 + 0.25 / delta ) * b;

    for ( ; ; )
    {
      u1 = RandUni (pcg_rng);
      u2 = RandUni (pcg_rng);

      if ( u1 < 0.5 )
      {
        y = u1 * u2;
        z = u1 * y;

        if ( k1 <= 0.25 * u2 + z - y )
        {
          continue;
        }
      }
      else
      {
        z = u1 * u1 * u2;

        if ( z <= 0.25 )
        {
          v = beta * log ( u1 / ( 1.0 - u1 ) );
          w = a * exp ( v );

          if ( aa == a )
          {
            value = w / ( b + w );
          }
          else
          {
            value = b / ( b + w );
          }
          return value;
        }

        if ( k2 < z )
        {
          continue;
        }
      }

      v = beta * log ( u1 / ( 1.0 - u1 ) );
      w = a * exp ( v );

      if ( log ( z ) <= alpha * ( log ( alpha / ( b + w ) ) + v ) - log4 )
      {
        break;
      }
    }
  }

  if ( aa == a )
  {
    value = w / ( b + w );
  }
  else
  {
    value = b / ( b + w );
  }
  return value;
}
