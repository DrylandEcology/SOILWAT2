#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include <math.h>

/* int logged is to be declared in the main module of your program. */
/* global variable indicates logfile used: externed via generic.h */
/* so we can't make it Bool.  But we don't have to explicitly */
/* externed it in each module, just include generic.h. */
/* Just be sure to set logged = FALSE as the first step in main(). */

/*
 History:
 2011/01/27	(drs) renamed from "gen_funcs.c" to "generic.c"
 05/25/2012  (DLM) added interpolation() function
 05/29/2012  (DLM) added lobf(), lobfM(), & lobfB() function
 05/31/2012  (DLM) added st_getBounds() function for use in the soil_temperature function in SW_Flow_lib.c
 */

#include "include/generic.h"
#include "include/filefuncs.h"



/* =================================================== */
/*             Local Function Definitions              */
/* --------------------------------------------------- */

static void uncomment_cstyle(char *p) {
	/*-------------------------------------------
	 overwrite chars in a string pointed to by p with
	 characters after the end-comment delimiter.
	 p points to the first .
	 cwb - 9/11/01
	 -------------------------------------------*/

	char *e; /* end of comment */

	if ((e = strchr(p + 2, '*'))) {
		if (*(++e) == '/') {
			e++;
			while (*e)
				*(p++) = *(e++);
		}
	}
	*p = '\0';

}



/* =================================================== */
/*             Global Function Definitions             */
/* --------------------------------------------------- */

/*****************************************************/
char *Str_TrimLeft(char *s) {
	/*-------------------------------------------
	 * Trim left blanks from a string by moving
	 * leftmost non-blank chars to beginning of string.
	 * Return original pointer *s.

	 cwb - 18-Nov-02
	 -------------------------------------------*/

	char *q, *p;
	q = p = s;
	while (*q && isspace((int)*(q)))
		q++; /* goto nonblank */
	while (*q) {
		*(p++) = *(q++);
	}
	*p = '\0';

	return s;
}

/*****************************************************/
char *Str_TrimLeftQ(char *s) {
	/*-------------------------------------------
	 * "Quick" Trim left blanks from a string by simply
	 * returning a pointer to the first non-blank
	 * character in s.  Useful for Str_Dup().

	 cwb - 18-Nov-02
	 -------------------------------------------*/

	char *q = s;
	while (*q && isspace((int)*q))
		q++; /* goto nonblank */

	return q;
}

/*****************************************************/
char *Str_TrimRight(char *s) {
	/*-------------------------------------------
	 * Trim a string by setting first trailing space to '\0'
	 * and return the string s.

	 cwb - 6/15/00
	 cwb - 9/11/01 moved this from Uncomment
	 -------------------------------------------*/

	char *p = s + strlen(s);

	while ((--p) >= s && isspace((int)*p))
		;
	*(++p) = '\0';

	return s;
}

/*****************************************************/
char *Str_ToUpper(char *s, char *r) {
	/*-------------------------------------------
	 * Copy s as uppercase into space pointed to by r.
	 * Return r.

	 cwb - 10/5/01
	 -------------------------------------------*/
	char *p = s, *q = r;
	while (*p)
		*(q++) = (char) toupper((int) (*(p++)));
	*q = '\0';
	return r;
}

/*****************************************************/
char *Str_ToLower(char *s, char *r) {
	/*-------------------------------------------
	 * Copy s as lowercase into space pointed to by r.
	 * Return r.

	 cwb - 10/5/01
	 -------------------------------------------*/
	char *p = s, *q = r;
	while (*p)
		*(q++) = (char) tolower((int) (*(p++)));
	*q = '\0';
	return r;
}

/*****************************************************/
int Str_CompareI(char *t, char *s) {
	/*-------------------------------------------
	 * works like strcmp() except case-insensitive
	 * cwb 4-Sep-03
	 */

	char *t1 = (char *) malloc(strlen(t) + 1);
	char *s1 = (char *) malloc(strlen(s) + 1);
	int r = strcmp(Str_ToUpper(t, t1), Str_ToUpper(s, s1));
	free(t1);
	free(s1);
	return r;
}

/*****************************************************/
void UnComment(char *s) {
	/*-------------------------------------------
	 Decomments a string by :
	 a) putting a null char over the first '#'
	 b) handling c-style comments.  If the block
	 is in the middle of the non-blank part
	 of the string, then the whitespace is
	 unchanged.  THIS ROUTINE DOES NOT
	 UNCOMMENT MULTIPLE LINES however it will
	 delete from the first forward-slash - star delimiter to
	 the end of the string.  There CANNOT
	 be any characters between the / or *.


	 White space at the end of the string
	 part of the string is always removed.

	 cwb - 9/11/01 added c-style comment code and
	 split out the different tasks.
	 -------------------------------------------*/
	char *p;

	if ((p = strchr(s, '#')))
		*p = '\0';
	else if ((p = strstr(s, "/*")))
		uncomment_cstyle(p);

	Str_TrimRight(s);
}



/**************************************************************************************************************************************
 PURPOSE: Calculate a linear interpolation between two points, for use in soil_temperature function

 HISTORY:
 05/25/2012 (DLM) initial coding
 **************************************************************************************************************************************/
double interpolation(double x1, double x2, double y1, double y2, double deltaX) {
	return y1 + (((y2 - y1) / (x2 - x1)) * (deltaX - x1));
}

/**************************************************************************************************************************************
 PURPOSE: To get the index of the lower(x1) and upper bound(x2) at the depth, for use in soil_temperature function.  written in it's own separate function to reduce code duplication.  located here so it doesn't take up space in SW_Flow_lib.c

 *NOTE* Works with positive values.  Hasn't been tested with negative values, might not work correctly

 HISTORY:
 05/31/2012 (DLM) initial coding

 INPUTS:
 size - the size of the array bounds[]
 depth - the depth of the index you are looking for, should be less than the highest bound (ie. bounds[size - i]) or the function won't work properly
 bounds[] - the depths of the bounds (needs to be in order from lowest to highest)

 OUTPUTS:
 x1 - the index of the lower bound (-1 means below the lowest depth of bounds[] (aka UINT_MAX), in this case x2 will be 0)
 x2 - the index of the upper bound (-1 means above the highest depth of bounds[] (aka UINT_MAX), in this case x1 will be size - 1)
 equal - is this equals 1, then the depth is equal to the depth at bounds[x1]
 **************************************************************************************************************************************/
void st_getBounds(unsigned int *x1, unsigned int *x2, unsigned int *equal, unsigned int size, double depth, double bounds[]) {
	unsigned int i;
	*equal = 0;
	*x1 = -1; 			// -1 means below the lowest bound
	*x2 = size - 1; 	// size - 1 is the upmost bound...

	// makes sure the depth is within the bounds before starting the for loop... to save time if it's not in between the bounds
	if (LT(depth, bounds[0])) {
		*x2 = 0;
		return;
	} else if (GT(depth, bounds[size - 1])) {
		*x1 = size - 1;
		*x2 = -1;
		return;
	}

	for (i = 0; i < size; i++) {

		if (i < size - 1) { // to avoid going out of the bounds of the array and subsequently blowing up the program
			if (LE(bounds[i], depth) && (!(LE(bounds[i + 1], depth)))) {
				*x1 = i;
				if (EQ(bounds[i], depth)) {
					*equal = 1;
					*x2 = i;
					return; // return since they're equal & no more calculation is necessary
				}
			}
		}
		if (i > 0) {  // to avoid going out of the bounds of the array
			if (GE(bounds[i], depth) && (!(GE(bounds[i - 1], depth)))) {
				*x2 = i;
				if (EQ(bounds[i], depth)) {
					*equal = 1;
					*x1 = i;
				}
				return; // if it's found the upperbound, then the lowerbound has already been calculated, so return
			}
		}
	}
}

/**************************************************************************************************************************************
 PURPOSE: Calculate the slope of a line of best fit

 HISTORY:
 05/29/2012 (DLM) initial coding
 **************************************************************************************************************************************/
double lobfM(double xs[], double ys[], unsigned int n) {
	double sumX, sumY, sumXY, sumX2, temp;
	unsigned int i;

	sumX = sumY = sumXY = sumX2 = 0.0; // init values to 0
	for (i = 0; i < n; i++) {
		sumX += xs[i]; // sumX is the sum of all the x values
		sumY += ys[i]; // sumY is the sum of all the y values
		sumXY += (xs[i] * ys[i]); // sumXY is the sum of the product of all the x & y values (ie: x1 * y1 + x2 * y2 + ...... + xn * yn)
		sumX2 += squared(xs[i]); // sumX2 is the sum of the square of all the x values (ie: x1^2 + x2^2 + ....... + xn^2)
	}

	temp = n + 0.0;
	return ((sumX * sumY - temp * sumXY) / (squared(sumX) - n * sumX2));
}

/**************************************************************************************************************************************
 PURPOSE: Calculate the y-intercept of a line of best fit

 HISTORY:
 05/29/2012 (DLM) initial coding
 **************************************************************************************************************************************/
double lobfB(double xs[], double ys[], unsigned int n) {
	double sumX, sumY, temp;
	unsigned int i;

	sumX = sumY = 0.0;
	for (i = 0; i < n; i++) {
		sumX += xs[i];
		sumY += ys[i];
	}

	temp = n + 0.0;
	return ((sumY - lobfM(xs, ys, n) * sumX) / temp);
}

/**************************************************************************************************************************************
 PURPOSE: Calculate a line of best fit (when given correct parameters), for use in soil_temperature function
 returns m & b for an equation of the form y = mx + b, where m is the slope and b is the y-intercept
 n is the size of the arrays passed in.  currently not used, but i'll keep it in for a little while just in case

 HISTORY:
 05/29/2012 (DLM) initial coding
 **************************************************************************************************************************************/
void lobf(double *m, double *b, double xs[], double ys[], unsigned int n) {
	*m = lobfM(xs, ys, n); // m = slope
	*b = lobfB(xs, ys, n); // b = y-intercept
}

/** @brief Duplicate strings.

  Used if strdup() is not available. strdup() is not a ISO C standard,
  but included in the POSIX standard. That means that it is not available on
  some compilers, e.g., travis-ci with -std=c11 flag set.

  @see
    Code from <https://stackoverflow.com/questions/482375/strdup-function>
*/
char * sw_strdup(const char * s)
{
  size_t len = 1 + strlen(s);
  char *p = (char*) malloc(len); // explicit cast necessary for c++ compiler when unit testing against googletest

  return p ? (char*) memcpy(p, s, len) : NULL;
}


/** @brief Calculate running average online (in one pass)

		Calculate average \f$m\f$ across values \f$x[k]\f$ with
		\f$k = {0, ..., n}\f$ using
				\f$m[n] = m[n - 1] + (x[n] - m[n - 1]) / n\f$

		@param n Sequence position
		@param mean_prev Previous average value, i.e., \f$m[n - 1]\f$
		@param val_to_add New value that is added to the updated average,
					 i.e., \f$x[n]\f$

		@return The running average at sequence position \f$n\f$, i.e., \f$m[n]\f$

		@see Welford's algorithm based on
			<https://www.johndcook.com/blog/standard_deviation/> and
			<https://en.wikipedia.org/wiki/Algorithms_for_calculating_variance#Online>
*/
double get_running_mean(unsigned int n, double mean_prev, double val_to_add)
{
	return mean_prev + (val_to_add - mean_prev) / n;
}

/** @brief Calculate running standard deviation online (in one pass)

		There are two parts when calculating the standard deviation \f$sd\f$
		across values \f$x[k]\f$ for \f$k = {0, ..., n}\f$.

		1) The first part is to calculate the running sum of squares \f$S[n]\f$, i.e.,
			\f$S[n] = S[n - 1] + ss[n]\f$
		where
			\f$ss[n] = (x[n] - m[n - 1]) * (x[n] - m[n])\f$
		and where \f$m[n]\f$ is the running average from function
		get_running_mean().
		The function get_running_sqr() calculates \f$ss[n]\f$ which needs to be
		added up.

		2) The second part is to calculate \f$sd = \sqrt{\frac{S[n]}{n - 1}}\f$
		which is accomplished by function final_running_sd().

		@param mean_prev Previous average value, i.e., \f$m[n - 1]\f$
		@param mean_current Current average value, i.e., \f$m[n]\f$
		@param val_to_add New value that was added to the updated average,
					 i.e., \f$x[n]\f$

		@return The summand \f$ss[n]\f$. The sum of these values can be used as
						input to function final_running_sd().

		@see Welford's algorithm based on
			<https://www.johndcook.com/blog/standard_deviation/> and
			<https://en.wikipedia.org/wiki/Algorithms_for_calculating_variance#Online>
*/
double get_running_sqr(double mean_prev, double mean_current, double val_to_add)
{
	return (val_to_add - mean_prev) * (val_to_add - mean_current);
}

/** @brief Calculate running average online (in one pass)

		There are two parts when calculating the standard deviation \f$sd\f$
		across values \f$x[k]\f$ for \f$k = {0, ..., n}\f$.

		(1) The first part is to calculate the running sum of squares \f$S[n]\f$, i.e.,
			\f$S[n] = S[n - 1] + ss[n]\f$
		where
			\f$ss[n] = (x[n] - m[n - 1]) * (x[n] - m[n])\f$
		and where \f$m[n]\f$ is the running average from function
		get_running_mean().
		The function get_running_sqr() calculates \f$ss[n]\f$ which needs to be
		added up.

		(2) The second part is to calculate \f$sd = \sqrt{\frac{S[n]}{n - 1}}\f$
		which is accomplished by function final_running_sd().

		@param n Sequence position
		@param ssqr The value \f$S[n]\f$ which has been calculated as the
			sum of the return values of calls to the function get_running_sqr()

		@return The running standard deviation at sequence position \f$n\f$,
			i.e., \f$\sqrt{\frac{S[n]}{n - 1}}\f$.

		@see Welford's algorithm based on
			<https://www.johndcook.com/blog/standard_deviation/> and
			<https://en.wikipedia.org/wiki/Algorithms_for_calculating_variance#Online>
*/
double final_running_sd(unsigned int n, double ssqr)
{
	return (n > 1) ? sqrt(ssqr / (n - 1)) : 0.;
}

/**
 @brief Takes the average over an array of size length

 @param values Array of size length holding the values to be averaged
 @param length Size of input array may be years, months, days, etc.

 @note When a value is SW_MISSING, the function sees it as a value to skip and ignores it to not influence
 the mean.
 */
double mean(double values[], int length) {

    int index, finalLength = 0;
    double total = 0.0, currentVal;

    for(index = 0; index < length; index++) {
        currentVal = values[index];

        if(!missing(currentVal)) {
            total += currentVal;
            finalLength++;
        }
    }

    return total / finalLength;
}

/**
 @brief Takes the standard deviation of all values in an array all at once.

 @param inputArray Array containing values to find the standard deviation of
 @param length Size of the input array

 @note This function is preferred to be used when the set is small. If the standard deviation of a large set is needed,
 attempt a running standard deviation.

 @note When a value is SW_MISSING, the function sees it as a value to skip and ignores it to not influence
 the standard deviation.
 */
double standardDeviation(double inputArray[], int length) {

    int index, finalLength = 0;
    double arrayMean = mean(inputArray, length), total = 0.0, currentVal;

    for(index = 0; index < length; index++) {
        currentVal = inputArray[index];

        if(!missing(currentVal)) {
            total += (currentVal - arrayMean) * (currentVal - arrayMean);
            finalLength++;
        }
    }

    return sqrt(total / (finalLength - 1));

}