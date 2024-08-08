/*	generic.h -- contains some generic definitions
 that could be useful in any program

 USES: generic.c

 REQUIRES: none

 Chris Bennett @ LTER-CSU 6/15/2000

 5/19/2001  moved rand functions to rands.h

 10/22/2010	(drs)	replaced every occurence of F_DELTA, #define F_DELTA
 (10*FLT_EPSILON), with ( max( 10.*FLT_EPSILON, FLT_EPSILON*pow(10.,
 ceil(log10(max(fabs(x),max(fabs(y), FLT_EPSILON))+1.)) ) ) ) and similar for
 D_DELTA because earlier version worked only for 0<=x<fabs(31)

 01/03/2011	(drs) macro 'isless' renamed to 'isless2'

 05/25/2012	(DLM) added interpolation() function

 05/29/2012   (DLM) added lobf(), lobfM(), & lobfCB() functions

 05/29/2012   (DLM) added squared(x) definition, this squares the value x (ie.
 returns the value of x^2).  the definition simply calls pow(x, 2) in the cmath
 library.  x must be a double. added for convenience

 03/08/2013	(clk) added abs(x) definition, this returns the absolute value
 of x. If x < 0, returns -x, else returns x.

 06/19/2013	(DLM) replaced isless2(), ismore(), iszero(), isequal() macros
 with new MUCH faster ones... these new macros make the program run about 4x
 faster as a whole.

 06/26/2013 (dlm)	powe(): an alternate definition of pow(x, y) for x>0...
 this is faster (ca. 20%) then the one in math.h, but comes with a cost as the
 precision is slightly lower.  The measured precision drop I was getting was at
 max a relative error of about 100 billion percent (12 places after the decimal
 point) per calculation though so I don't think it's a big deal... (though it's
 hard to even accurately tell)
 */


#ifdef NDEBUG
#undef SWDEBUG
#endif

#ifdef DEBUG
#define SWDEBUG
#endif

#ifdef RSWDEBUG
#define SWDEBUG
#endif


#if !defined(STEPWAT) && !defined(RSOILWAT)
#define SOILWAT // SOILWAT2-standalone
#endif


#ifndef GENERIC_H
#define GENERIC_H

#include <float.h>  // for DBL_EPSILON, FLT_EPSILON
#include <math.h>   // for fabs, sqrt, sqrtf
#include <stddef.h> // for NULL

#ifdef RSOILWAT
#include <R.h> // for Rprintf() from <R_ext/Print.h>
#endif


#ifdef __cplusplus
extern "C" {
#endif

/***************************************************
 * Basic definitions
 ***************************************************/

/* ------ Convenience macros. ------ */
/* integer to boolean */
#define itob(i) ((i) ? swTRUE : swFALSE)

/* integer versions */
#ifndef MAX
#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#endif

#ifndef MIN
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#endif

/* floating point versions work for float or double */
#ifndef fmax
#define fmax(a, b) ((GT((a), (b))) ? (a) : (b))
#endif

#ifndef fmin
#define fmin(a, b) ((LT((a), (b))) ? (a) : (b))
#endif

#define isnull(a) (NULL == (a))

/* ---------   Redefine basic types to be more malleable ---- */
typedef int Int;
typedef unsigned int IntU;
typedef short int IntS;
typedef unsigned short IntUS;
typedef long IntL;

// cannot use 'TRUE' and 'FALSE' because they are defined by R
typedef enum { swFALSE = (1 != 1), swTRUE = (1 == 1) } Bool;

typedef unsigned char byte;


/* --------------------------------------------------*/
/* These are facilities for logging errors.          */

/** Print macro that works correctly both for rSOILWAT2 and for
 * SOILWAT2-standalone. Use instead of (R)printf */
#ifdef RSOILWAT
#define sw_printf Rprintf
#else
#define sw_printf printf
#endif


/* constants for LogError() mode */
#define LOGWARN 0x02
#define LOGERROR 0x04
#define MAX_ERROR 4096


/* --------------------------------------------------*/
/* --------------------------------------------------*/

/* The following tests account for imprecision in the
 floating point representation of either single
 or double real numbers.  Use these instead of
 the regular boolean operators.  Note that only
 the magnitude of the x operand is tested, so both
 operands (x & y) should be of the same type, or cast
 so.

 Update (DLM) : 6-19-2013 : These macros (iszero(), isequal(), isless2(),
    & ismore()) were taking a VERY long time to compute, so I wrote some new
    ones defined below.  I also rewrote the macros for LE() and GE() because
    they were making unnecessary calls.
 : These new macros cause the program to run about 4x faster as a whole.
    The old macros are still here, but commented out for now.
 : The reason the old macros were taking sooo long is because they make lots of
    function calls many of which are to performance intensive functions
    (such as log10() and pow()) and the macros are used EVERYWHERE in the code
    (I'm guessing they are being called 100's of thousands of times).
 : The new macros perform the function (to account for imprecision in floating
    points), but do so in a less complicated way (by checking them against
    absolute/relative error)... the perform VERY close to the old functions
    (which I'm not sure why they are even making some of the calls they are as
    the complexity of the math is seemingly unnecessary), but not 100%
    the same... this shouldn't be much of an issue though because there will
    be errors in the floating point arithmetic at some point anyways,
    because that is the nature of floating point.
 : To have floating point calculations without some error is impossible by
    definition.  The reason why there are errors in floating point is because
    some numbers cannot be 100% accurately represented (like repeating decimals,
    PI, and the canonical example 0.1) in the scheme that floating point uses.
    This makes sense if you think about it a little because numbers with a
    non-finite amount of digits after the decimal point cannot possibly be
    represented finitely by definition, so some numbers have to get cut off.
 : For the absolute error check I am using F_DELTA (D_DELTA for doubles) and
    for the relative error check I am using FLT_EPSILON
    (DBL_EPSILON for doubles).  F_DELTA is equal to 10*FLT_EPSILON, and
    DBL_DELTA is equal to 10*DBL_EPSILON.
 : FLT_EPSILON is defined in float.h (a part of the C STANDARD), and is equal
    to the smallest x such that "((1.0 + x) != 1.0)".
    DBL_EPSILON is the same except for the double data type.
 : Also note that these macros should never be called with a side-effecting
    expression (ie x++ or ++x) as it will end up incrementing the variable more
    then desired... this is a problem that macros have.
    This shouldn't be an issue though as these functions are only meant
    for floats/doubles, which are typically not used with side-effecting
    expressions.
 */
#define D_DELTA (10 * DBL_EPSILON)


#if defined(STEPWAT)
/* ------ STEPWAT2 uses floats and doubles ------ */
typedef float RealF;
typedef double RealD;

#define F_DELTA (10 * FLT_EPSILON)

// new definitions for these four macros (MUCH MUCH faster, by a factor of about
// 4)... just trying them out for now.  The idea behind how these work is that
// both an absolute error and relative error check are being used in conjunction
// with one another.  In this for now I'm using F_DELTA for the amount of
// absolute error allowed and FLT_EPSILON for the amount of relative error
// allowed.
#define GET_F_DELTA(x, y)                                      \
    ((sizeof(x) == sizeof(float)) ?                            \
         (MAX(F_DELTA, FLT_EPSILON * MAX(fabs(x), fabs(y)))) : \
         (MAX(D_DELTA, DBL_EPSILON * MAX(fabs(x), fabs(y)))))


/**< ZRO tests whether x is equal to zero while accounting for floating-point
 * arithmetic */
// for iszero(x) we just use an absolute error check, because a relative error
// check doesn't make sense for any number close enough to zero to be considered
// equal... it would be a waste of time.
#define ZRO(x) \
    ((sizeof(x) == sizeof(float)) ? (fabs(x) <= F_DELTA) : (fabs(x) <= D_DELTA))


/* redefine sqrt for double (default) or float */
#ifdef NO_SQRTF /* the case for Borland's compiler */
#define sqrtf sqrt
#endif

#define sqrt(x) ((sizeof(x) == sizeof(float)) ? sqrtf(x) : sqrt(x))


#else /* !defined(STEPWAT), i.e., SOILWAT2 and rSOILWAT2 */
/* ------ SOILWAT2 uses doubles (>= v8.1.0) ------ */

// new definitions for these four macros (MUCH MUCH faster, by a factor of about
// 4)... just trying them out for now.  The idea behind how these work is that
// both an absolute error and relative error check are being used in conjunction
// with one another.  In this for now I'm using F_DELTA for the amount of
// absolute error allowed and FLT_EPSILON for the amount of relative error
// allowed.
#define GET_F_DELTA(x, y) (MAX(D_DELTA, DBL_EPSILON * MAX(fabs(x), fabs(y))))

/**< ZRO tests whether x is equal to zero while accounting for floating-point
 * arithmetic */
// for iszero(x) we just use an absolute error check, because a relative error
// check doesn't make sense for any number close enough to zero to be considered
// equal... it would be a waste of time.
#define ZRO(x) (fabs(x) <= D_DELTA)

#endif


/**< LT tests whether x is less than y while accounting for floating-point
 * arithmetic */
#define LT(x, y) ((x) < ((y) - GET_F_DELTA(x, y)))

/**< GT tests whether x is greater than y while accounting for floating-point
 * arithmetic */
#define GT(x, y) ((x) > ((y) + GET_F_DELTA(x, y)))


/**< EQ tests whether x and y are equal based on a specified tolerance */
#define EQ_w_tol(x, y, tol) (fabs((x) - (y)) <= tol)

/**< EQ tests whether x and y are equal while accounting for floating-point
 * arithmetic */
#define EQ(x, y) (EQ_w_tol((x), (y), GET_F_DELTA((x), (y))))

/**< LE tests whether x is less than or equal to y while accounting for
 * floating-point arithmetic */
// changed from "(LT(x,y)||EQ(x,y))" because it evaluates to the same result
// (since EQ is already doing the checking for precision errors so use < instead
// of LT to avoid checking for precision errors twice) and this version is
// faster by avoiding the expensive LT() call.
#define LE(x, y) ((x) < (y) || EQ(x, y))

/**< LE tests whether x is greater than or equal to y while accounting for
 * floating-point arithmetic */
#define GE(x, y) ((x) > (y) || EQ(x, y))

// 06/26/2013 (dlm)	powe(): an alternate definition of pow(x, y) for x>0...
// this is faster (ca. 20%) then the one in math.h, but comes with a cost as the
// precision is slightly lower.  The measured precision drop I was getting was
// at max a relative error of about 100 billion percent (12 places after the
// decimal point) per calculation though so I don't think it's a big deal...
// (though it's hard to even accurately tell)
// x^y == exponential(y * ln(x)) or e^(y * ln(x)).  NOTE: this will only work
// when x > 0 I believe
#define powe(x, y) (exp((y) * log(x)))

#define squared(x) ((x) * (x)) // added for convenience


/* =================================================== */
/*             Global Function Declarations            */
/* --------------------------------------------------- */
char *Str_TrimRight(char *s);

char *Str_TrimLeft(char *s);

char *Str_TrimLeftQ(char *s);

char *Str_ToUpper(char *s, char *r);

char *Str_ToLower(char *s, char *r);

int Str_CompareI(char *t, char *s);

char *sw_strtok(
    char inputStr[], size_t *startIndex, size_t *strLen, const char *delim
);

Bool isDelim(char currChar, const char *delim);

void UnComment(char *s);

double interpolation(double x1, double x2, double y1, double y2, double deltaX);

void st_getBounds(
    unsigned int *x1,
    unsigned int *x2,
    unsigned int *equal,
    unsigned int size,
    double depth,
    double bounds[]
);

double lobfM(const double xs[], const double ys[], unsigned int n);

double lobfB(double xs[], double ys[], unsigned int n);

void lobf(double *m, double *b, double xs[], double ys[], unsigned int n);


double get_running_mean(unsigned int n, double mean_prev, double val_to_add);

double get_running_sqr(
    double mean_prev, double mean_current, double val_to_add
);

double final_running_sd(unsigned int n, double ssqr);

double mean(const double values[], unsigned int length);

double standardDeviation(double inputArray[], unsigned int length);


#ifdef __cplusplus
}
#endif

#define GENERIC_H
#endif
