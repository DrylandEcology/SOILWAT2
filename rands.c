#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <time.h>
#include "generic.h"
#include "rands.h"
#include "myMemory.h"

long _randseed = 0L;

/*****************************************************/
/**
  \fn void RandSeed(signed long seed)
  \brief Resets the random number seed.

 The seed is set to negative when this routine
 is called, so the generator routines
 ( eg, RandUni()) can tell that it has
 changed.  If called with seed==0,
 _randseed is reset from process time.
 '% 0xffff' is due to a bug in RandUni()
 that conks if seed is too large; should
 be removed in the near future.

 \param seed The seed.

 cwb - 6/27/00
*/
void RandSeed(signed long seed) {

	if (seed == 0L) {
		_randseed = ((long) time(NULL ));
		if (_randseed == -1) {
#ifndef RSOILWAT
			fprintf(stderr, "ERROR: RandSeed(0) called, "
					"but time() not available\n");
			exit(-1);
#else
			Rprintf("ERROR: RandSeed(0) called, but time() not available\n");
     		Rprintf("EXIT -1");
     		error("@ RandSeed");
#endif
		}
		/*    _randseed %= 0xffff; */
		_randseed *= -1;
	} else {
		_randseed = abs(seed) * -1;
	}

#if RAND_FAST
	srand(abs(_randseed));
#endif

}

#define BUCKETSIZE 97

/*****************************************************/
/**
  \fn double RandUni_fast(void)
  \brief Generate a uniform random variate.

  "Fast" because it
  utilizes the system rand() but shuffles the results
  to make a less correlated sequence.  This code is
  based on FUNCTION RAN0 in Press, et al., 1986,
  Numerical Recipes, p196, Press Syndicate, NY.

  Of course, just how fast is "fast" depends on the
  implementation of the compiler.  Some older generators
  may be quite simple, and so would be faster than
  a more complicated algorithm.  Newer rand()'s
  are often fast and good.

  cwb 18-Dec-02

	\return double. A value between 0 and 1.
*/
double RandUni_fast(void) {
	static short first_time = 1;
	static int bucket[BUCKETSIZE];
	static double y, rmax = RAND_MAX;
	int i, j;

	if (first_time) {
		first_time = 0;
		for (j = 0; j < BUCKETSIZE; j++)
			rand();
		for (j = 0; j < BUCKETSIZE; j++)
			bucket[j] = rand();
		y = rand() / rmax;
	}
	i = 1 + (int) ((double) BUCKETSIZE * y);
	i = max(BUCKETSIZE -1, min(i,0));
	y = bucket[i] / rmax;
	bucket[i] = rand();

	return y;
}

/*****************************************************/
/**
	\fn double RandUni_good(void)
	\brief Generate a uniform random variate.

	Return a random number from
	uniform distribution.
	Result is between 0 and 1. This routine
	is adapted from FUNCTION RAN1 in
	Press, et al., 1986, Numerical Recipes,
	p196, Press Syndicate, NY.
	To reset the random number sequence,
	set _randseed to any negative number
	prior to calling this function, or one
	that depends on it (eg, RandNorm()).

	This code is preferable in terms of portability
	as well as consistency across compilers.

	cwb - 6/20/00

 	\return double. A value between 0 and 1.

*/
double RandUni_good(void) {


	long i;
	static short first_time = 1;
	static double bucket[BUCKETSIZE], y;
	static const long im1 = 259200, ia1 = 7141, ic1 = 54773, im2 = 134456,
		ia2 = 8121, ic2 = 28411, im3 = 243000, ia3 = 4561, ic3 = 51349;
	static const double rm1 = 3.8580247e-6, /* 1/im1 */
		rm2 = 7.4373773e-6; /* 1/im2 */

	static long ix1, ix2, ix3;

	if (_randseed == 0L) {
#ifndef RSOILWAT
		fprintf(stderr, "RandUni() error: seed not set\n");
		exit(-1);
#else
		Rprintf("RandUni() error: seed not set\n");
		Rprintf("EXIT -1");
		error("@ _randseed==0L");
#endif
	}
	if (first_time || _randseed < 0) {
		first_time = 0;
		ix1 = abs(ic1 - abs(_randseed)) % im1;
		ix1 = (ia1 * ix1 + ic1) % im1;
		ix2 = ix1 % im2;
		ix2 = (ia2 * ix2 + ic2) % im2; /* looks like a typo in the book */
		ix3 = ix1 % im3;
		for (i = 0; i < BUCKETSIZE; i++) {
			ix1 = (ia1 * ix1 + ic1) % im1;
			ix2 = (ia2 * ix2 + ic2) % im2;
			bucket[i] = ((double) ix1 + (double) ix2 * rm2) * rm1;
		}
		_randseed = 1;
	}

	/* start here if not initializing, */
	/* and make the numbers happen     */
	ix1 = (ia1 * ix1 + ic1) % im1;
	ix2 = (ia2 * ix2 + ic2) % im2;
	ix3 = (ia3 * ix3 + ic3) % im3;

	/* get a random index into the bucket */
	i = 1 + (ix3 * BUCKETSIZE) / im3;
	i = max(BUCKETSIZE -1, min( i,0));
	/*  i = (i > BUCKETSIZE -1 ) ? BUCKETSIZE -1 : i; */

	/* snatch a random number and replace it */
	y = bucket[i];
	bucket[i] = ((double) ix1 + (double) ix2 * rm2) * rm1;

	return y;
}

/*****************************************************/
/**
	\fn int RandUniRange(const long first, const long last)
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

	\return integer. Random number between the two bounds defined.

*/
int RandUniRange(const long first, const long last) {


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
	return (long) (RandUni() * r) + f;

}

/*****************************************************/
void RandUniList(long count, long first, long last, RandListType list[]) {
	/*-------------------------------------------
	 create a list of random integers from
	 uniform distribution.  There are count
	 numbers put into the list array, and
	 the numbers fall between first and last,
	 inclusive.  The numbers are non-repeating,
	 and not necessarily ordered.  This method
	 only works for a uniform distribution, but
	 it should be fast for any number of count items.

	 cwb - 6/27/00
	 -------------------------------------------*/
	long i, j, c, range, *klist;

	range = last - first + 1;

	if (count > range || range <= 0) {
#ifndef RSOILWAT
		fprintf(stderr, "Programmer error in RandUniList: "
				"count > range || range <= 0\n");
		exit(-1);
#else
		Rprintf("Programmer error in RandUniList: "
            "count > range || range <= 0\n");
		Rprintf("EXIT -1 RandUniList");
#endif
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
		list[0] = (long) RandUniRange(first, last);
		if (count == 2)
			while ((list[1] = RandUniRange(first, last)) == list[0])
				;
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
		while ((j = RandUniRange(0, range - 1)) == i)
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
double RandNorm(double mean, double stddev) {
	/*-------------------------------------------
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
	 -------------------------------------------*/
	static short set = 0;

	static double v1, v2, r, fac, gset, gasdev;

	if (!set) {
		do {
			v1 = 2.0 * RandUni() - 1.0;
			v2 = 2.0 * RandUni() - 1.0;
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

	return mean + gasdev * stddev;
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
  \return A random variate of a beta distribution.
*/
float RandBeta ( float aa, float bb )
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
    fprintf ( stderr, "\n" );
    fprintf ( stderr, "GENBET - Fatal error!\n" );
    fprintf ( stderr, "  AA <= 0.0\n" );
    exit ( 1 );
  }

  if ( bb <= 0.0 )
  {
    fprintf ( stderr, "\n" );
    fprintf ( stderr, "GENBET - Fatal error!\n" );
    fprintf ( stderr, "  BB <= 0.0\n" );
    exit ( 1 );
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
      u1 = RandUni ( );
      u2 = RandUni ( );
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
      u1 = RandUni ( );
      u2 = RandUni ( );

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
