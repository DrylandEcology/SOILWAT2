/********************************************************/
/********************************************************/
/*  Source file: SW_Defines.h
 *  Type: header
 *  Application: SOILWAT - soilwater dynamics simulator
 *  Purpose: Define all the commonly used constants,
 *           looping constructs, and enumeration types
 *           that are used by most of the model code.
 *  History:
 *     (8/28/01) -- INITIAL CODING - cwb
	09/09/2011	(drs) added #define ForEachXXXTranspLayer() for each vegetation type (XXX = tree, shrub, grass)
	05/31/2012  (DLM) added MAX_ST_RGR definition for the maximum amount of soil temperature regressions allowed...
	07/09/2013	(clk) added the ForEachForbTranspLayer(i) function
 	01/02/2015	(drs) changed MAX_ST_RGR from 30 to 100 (to allow a depth of 10 m and 10 cm intervals)
*/
/********************************************************/
/********************************************************/

#ifndef SOILW_DEF_H
#define SOILW_DEF_H

#include <math.h>  /* >= C99; for: atan(), isfinite() */
#include "include/generic.h"

#if !defined(RSOILWAT) /* rSOILWAT2 uses R's RNGs */
#include "external/pcg/pcg_basic.h" // see https://github.com/imneme/pcg-c-basic
#endif


#ifdef __cplusplus
extern "C" {
#endif

/* SOILWAT2 version and compile time metadata */
#ifndef SW2_VERSION
  #define SW2_VERSION "unset"
#endif

#ifndef USERNAME
  #define USERNAME "unset"
#endif

#ifndef HOSTNAME
  #define HOSTNAME "unset"
#endif

#define BUILD_DATE __DATE__
#define BUILD_TIME __TIME__


/* Not sure if this parameter is variable or a consequence of algebra,
 * but it's different in the FORTRAN version than in the ELM doc.
 * If deemed to need changing, might as well recompile rather than
 * confuse users with an unchanging parameter.
 */
#define SLOW_DRAIN_DEPTH 15. /* numerator over depth in slow drain equation */

/* some basic constants */
#define MAX_LAYERS  25 /**< Maximum number of soil layers */
#define MAX_TRANSP_REGIONS 4 /**< Maximum number of transpiration regions */
#define MAX_ST_RGR 100 /**< Maximum number of soil temperature nodes */

#define MAX_NYEAR 2500  /**< An integer representing the max calendar year that is supported. The number just needs to be reasonable, it is an artifical limit. */

#define SW_MISSING     999.     /**< Value to use as MISSING */

// Euler's constant
#ifdef M_E
  #define swE M_E
#else
  #define swE         2.71828182845904523536028747135266249
#endif


/* M_PI and M_PI_2 from <math.h> if implementation conforms to POSIX extension
   but may not be defined for any implementation that conforms to the C standard
*/
#ifdef M_PI
  #define swPI M_PI
#else
  #define swPI        3.141592653589793238462643383279502884197169399375
#endif

#define swPI2         6.28318530717958647692528676655900577

#ifdef M_PI_2
  #define swPI_half M_PI_2
#else
  #define swPI_half   1.57079632679489661923132169163975144
#endif

#define deg_to_rad    0.0174532925199433 /**< Convert arc-degrees to radians, i.e., x * deg_to_rad with deg_to_rad = pi / 180 */
#define rad_to_deg    57.29577951308232 /**< Convert radians to arc-degrees, i.e., x * rad_to_deg with rad_to_deg = 180 / pi */

#define SEC_PER_DAY	86400. // the # of seconds in a day... (24 hrs * 60 mins/hr * 60 sec/min = 86400 seconds)

#define OUTSTRLEN 3000 /* max output string length: in get_transp: 4*every soil layer with 14 chars */
#define OUT_DIGITS 6 // number of floating point decimal digits written to output files
#define _OUTSEP ',' // Separator used when generating output files

//was 256 & 1024...
#define MAX_FILENAMESIZE 512
#define MAX_PATHSIZE 2048

/* Maximum number of messages to store in LOG_INFO */
#define MAX_MSGS 10

/* Maximum number of characters for a single message in LOG_INFO */
#define MAX_LOG_SIZE 300

/* this could be defined by STEPWAT */
#ifndef DFLT_FIRSTFILE
  #define DFLT_FIRSTFILE "files.in"
#endif

#define MAX_SPECIESNAMELEN   4  /* for vegestab */

/* Constant for a key not found when reading in a key-value style input file */
#define KEY_NOT_FOUND -1

/* convenience indices to arrays in the model */
#define TWO_DAYS   2
#define SW_TOP 0
#define SW_BOT 1
#define SW_MIN 0
#define SW_MAX 1

/* indices to vegetation types */
#define NVEGTYPES 4 /**< Number of vegetation types implemented */
#define SW_TREES 0
#define SW_SHRUB 1
#define SW_FORBS 2
#define SW_GRASS 3

/* Constants for number of months, weeks, and days in a year */
  /* number of days in each week. unlikely to change, but
  * useful as a readable indicator of usage where it occurs.
  * On the other hand, it is conceivable that one might be
  * interested in 4, 5, or 6 day periods, but redefine it
  * in specific programs and take responsibility there,
  * not here.
  */
#define MAX_MONTHS 12
#define MAX_WEEKS 53
#define MAX_DAYS 366

#define SWRC_PARAM_NMAX 6 /**< Maximal number of SWRC parameters implemented */

/*
   Indices to daily input flags/indices (dailyInputFlags & dailyInputIndices in SW_WEATHER)
   The order of these indices must match the order of weather input flags within `weathsetup.in`
*/
#define MAX_INPUT_COLUMNS 14    /**< Maximum number of columns that can be input in a weath.YYYY file*/
#define TEMP_MAX      0
#define TEMP_MIN      1
#define PPT           2
#define CLOUD_COV     3
#define WIND_SPEED    4
#define WIND_EAST     5
#define WIND_NORTH    6
#define REL_HUMID     7
#define REL_HUMID_MAX 8
#define REL_HUMID_MIN 9
#define SPEC_HUMID    10
#define TEMP_DEWPOINT 11
#define ACTUAL_VP     12
#define SHORT_WR      13


/* output period specifiers */
#define SW_DAY   "DY"
#define SW_WEEK  "WK"
#define SW_MONTH "MO"
#define SW_YEAR  "YR"

#define SW_DAY_LONG   "Day"
#define SW_WEEK_LONG  "Week"
#define SW_MONTH_LONG "Month"
#define SW_YEAR_LONG  "Year"

#define SW_OUTNPERIODS 4  // must match with defines below except `eSW_NoTime`
#define eSW_Day 0
#define eSW_Week 1
#define eSW_Month 2
#define eSW_Year 3
#define eSW_NoTime 999 // no time period
// c++ doesn't support (pd)++ for pd as a typedef enum OutPeriod in
// macro `ForEachOutPeriod` --> instead, define as type `IntUS`
typedef IntUS OutPeriod;

/*
  * Number of output keys

  * Must match number of items in output enum (minus eSW_NoKey and eSW_LastKey)
*/
#define SW_OUTNKEYS 32

/*------------ DON'T CHANGE ANYTHING BELOW THIS LINE ------------*/
/* Macros to simplify and add consistency to common tasks */
/* Note the loop var must be declared as LyrIndex */
#define ForEachSoilLayer(i, n_layers)     for((i)=0; (i) < (n_layers);      (i)++)
#define ForEachEvapLayer(i, n_evap_lyrs)     for((i)=0; (i) < (n_evap_lyrs);   (i)++)
#ifdef STEPWAT
#define ForEachTranspLayer(i, n_transp_lyrs, veg_index)   for((i)=0; (i) < (n_transp_lyrs)[(veg_index)]; (i)++)
#endif
#define ForEachTranspRegion(r, n_transp_rgn)  for((r)=0; (r) < ((unsigned int)n_transp_rgn);  (r)++)
#define ForEachVegType(k)  for ((k) = 0; (k) < NVEGTYPES; (k)++)
#define ForEachVegTypeBottomUp(k)  for ((k) = NVEGTYPES - 1; (k) >= 0; (k)--)
/* define m as Months */
#define ForEachMonth(m)         for((m)=Jan; (m) <= Dec;  (m)++)
#define ForEachOutPeriod(k)  for((k)=eSW_Day;     (k)<=eSW_Year;     (k)++)


/* The ARCTANGENT function req'd by the original fortran produces
 * a highly configurable logistic curve. It was unfortunately
 * named tanfunc() in the original model, so I'm keeping it to
 * reduce confusion.  This is a very important function used in
 * lots of places.  It is described in detail in
 *    Parton, W.J., Innis, G.S.  1972 (July).  Some Graphs and Their
 *      Functional Forms.  U.S. International Biological Program,
 *      Grassland Biome, Tech. Rpt. No. 153.
 * The req'd parameters are (from Parton & Innis):
 *   z - the X variable
 *   a - X value of inflection point
 *   b - Y value of inflection point
 *   c - step size (diff of max point to min point)
 *   d - slope of line at inflection point
 */
#define tanfunc(z,a,b,c,d)  ((b)+((c)/swPI)*atan(swPI*(d)*((z)-(a))) )

/* To facilitate providing parameters to tanfunc() from the model,
 * this typedef can be used.  The parameters are analagous to a-d
 * above.  Some older C versions (of mine) name these differently
 * based on my own experiments with the behavior of the function
 * before I got the documentation.
 */
typedef struct { RealF xinflec, yinflec, range, slope; } tanfunc_t;


/* standardize the test for missing */
/* isfinite is C99 and std::isfinite is C++11 */
#define missing(x)  ( EQ( fabs( (x) ), SW_MISSING ) || !isfinite( (x) ) )


/* types to identify the various modules/objects */
typedef enum { eF,   /* file management */
               eMDL, /* model */
               eWTH, /* weather */
               eSIT, /* site */
               eSWC, /* soil water */
               eVES, /* vegetation establishement */
               eVPD, /* vegetation production */
               eOUT  /* output */
} ObjType;

/* Types to use for time and layer-related values and make a custom flag */
typedef unsigned int TimeInt;
typedef unsigned int LyrIndex;
typedef signed char flag;


/* =================================================== */
/*                   RNG structs                    */
/* --------------------------------------------------- */
#if defined(RSOILWAT)
typedef int sw_random_t; /* not used by rSOILWAT2; it uses instead R's RNG */
#else
typedef pcg32_random_t sw_random_t;
#endif


#ifdef __cplusplus
}
#endif

#endif
