/********************************************************/
/********************************************************/
/*  Source file: Markov.c
 *  Type: module; used by Weather.c
 *
 *  Application: SOILWAT - soilwater dynamics simulator
 *  Purpose: Read / write and otherwise manage the markov
 *           weather generation information.
 *  History:
 *     (8/28/01) -- INITIAL CODING - cwb
 *    12/02 - IMPORTANT CHANGE - cwb
 *          refer to comments in Times.h regarding base0
 06/27/2013	(drs)	closed open files if LogError() with LOGFATAL is called in SW_MKV_read_prob(), SW_MKV_read_cov()
 */
/********************************************************/
/********************************************************/

/* =================================================== */
/* =================================================== */
/*                INCLUDES / DEFINES                   */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "include/filefuncs.h"
#include "include/rands.h"
#include "include/Times.h"
#include "include/myMemory.h"
#include "include/SW_Files.h"
#include "include/SW_Model.h"
#include "include/SW_Markov.h"


/* =================================================== */
/*             Local Function Definitions              */
/* --------------------------------------------------- */

/**
  @brief Adjust average maximum/minimum daily temperature for whether day is
         wet or dry

  This function accounts for the situation such as daily maximum temperature
  being reduced on wet days compared to average (e.g., clouds prevent warming
  during daylight hours) and such as daily minimum temperature being increased
  on wet days compared to average (e.g., clouds prevent radiative losses
  during the night).

  All temperature values are in units of degree Celsius.

  @param tmax The average daily maximum temperature to be corrected
    (as produced by `mvnorm`).
  @param tmin The average daily minimum temperature to be corrected
    (as produced by `mvnorm`).
  @param rain Today's rain amount.
  @param cfmax_wet The additive correction factor for maximum temperature on a
    wet day.
  @param cfmax_dry The additive correction factor for maximum temperature on a
    dry day.
  @param cfmin_wet The additive correction factor for minimum temperature on a
    wet day.
  @param cfmin_dry The additive correction factor for minimum temperature on a
    dry day.

  @return tmax The corrected maximum temperature, i.e., tmax + cfmax.
  @return tmin The corrected minimum temperature, i.e., tmin + cfmin.
*/
static void temp_correct_wetdry(RealD *tmax, RealD *tmin, RealD rain,
  RealD cfmax_wet, RealD cfmax_dry, RealD cfmin_wet, RealD cfmin_dry) {

  if (GT(rain, 0.)) {
    // today is wet: use wet-day correction factors
    *tmax = *tmax + cfmax_wet;
    *tmin = fmin(*tmax, *tmin + cfmin_wet);

  } else {
    // today is dry: use dry-day correction factors
    *tmax = *tmax + cfmax_dry;
    *tmin = fmin(*tmax, *tmin + cfmin_dry);
  }
}


#ifdef SWDEBUG
  // since `temp_correct_wetdry` is static we cannot do unit tests unless we set it up
  // as an externed function pointer
  void (*test_temp_correct_wetdry)(RealD *, RealD *, RealD, RealD, RealD, RealD, RealD) = &temp_correct_wetdry;
#endif



/** Calculate multivariate normal variates for a set of
    minimum and maximum temperature means, variances, and their covariance
    for a specific day

    @param wTmax Mean weekly maximum daily temperature (degree Celsius);
           previously `_ucov[0]`
    @param wTmin Mean weekly minimum daily temperature (degree Celsius);
           previously `_ucov[1]`
    @param wTmax_var Mean weekly variance of maximum daily temperature;
           previously `vc00 = _vcov[0][0]`
    @param wTmin_var Mean weekly variance of minimum daily temperature;
           previously `vc11 = _vcov[1][1]`
    @param wT_covar Mean weekly covariance between maximum and minimum
           daily temperature; previously `vc10 = _vcov[1][0]`
	@param markov_rng Random number generator of the weather
		   generator
	@param LogInfo Holds information dealing with logfile output

    @return Daily minimum (*tmin) and maximum (*tmax) temperature.
*/
static void mvnorm(RealD *tmax, RealD *tmin, RealD wTmax, RealD wTmin,
	RealD wTmax_var, RealD wTmin_var, RealD wT_covar, pcg32_random_t *markov_rng,
	LOG_INFO* LogInfo) {
	/* --------------------------------------------------- */
	/* This proc is distilled from a much more general function
	 * in the original fortran version which was prepared to
	 * handle any number of variates.  In our case, there are
	 * only two, tmax and tmin, so there can be many fewer
	 * lines.  The purpose is to compute a random normal tmin
	 * value that covaries with tmax based on the covariance
	 * file read in at startup.
	 *
	 * cwb - 09-Dec-2002 -- This used to be two functions but
	 *       after some extensive debugging in this and the
	 *       RandNorm() function, it seems silly to maintain
	 *       the extra function call.
	 * cwb - 24-Oct-03 -- Note the switch to double (RealD).
	 *       C converts the floats transparently.
	 */
	RealD s, z1, z2, wTmax_sd, vc10, vc11;

	// Gentle, J. E. 2009. Computational statistics. Springer, Dordrecht; New York.
	// ==> mvnorm = mean + A * z
	// where
	//   z = vector of standard normal variates
	//   A = Cholesky factor or the square root of the variance-covariance matrix

	// 2-dimensional case:
	//   mvnorm1 = mean1 + sd1 * z1
	//   mvnorm2 = mean2 + sd2 * (rho * z1 + sqrt(1 - rho^2) * z2)
	//      where rho(Pearson) = covariance / (sd1 * sd2)

	// mvnorm2 = sd2 * rho * z1 + sd2 * sqrt(1 - rho^2) * z2
	// mvnorm2 = covar / sd1 * z1 + sd2 * sqrt(1 - covar ^ 2 / (var1 * var2)) * z2
	// mvnorm2 = covar / sd1 * z1 + sqrt(var2 - covar ^ 2 / var1) * z2
	// mvnorm2 = vc10 * z1 + vc11 * z2
	// with
	//   vc10 = covar / sd1
	//   s = covar ^ 2 / var1
	//   vc11 = sqrt(var2 - covar ^ 2 / var1)

	// Generate two independent standard normal random numbers
	z1 = RandNorm(0., 1., markov_rng);
	z2 = RandNorm(0., 1., markov_rng);

	wTmax_sd = sqrt(wTmax_var);
	vc10 = (GT(wTmax_sd, 0.)) ? wT_covar / wTmax_sd : 0;
	s = vc10 * vc10;

	if (GT(s, wTmin_var)) {
		LogError(LogInfo, LOGERROR, "\nBad covariance matrix in mvnorm()");
        return; // Exit function prematurely due to error
	}

	/* Apparently, it's possible for some but not all setups that
	   for some values of `s` and `wTmin_var` (e.g., 99.264050000, 99.264050000)
	   that both `GT(s, wTmin_var)` and `EQ(wTmin_var, s)` are FALSE;
	   and thus, `vc11` becomes `NaN`
  */
	vc11 = (LE(wTmin_var, s)) ? 0. : sqrt(wTmin_var - s);

	// mvnorm = mean + A * z
	*tmax = wTmax_sd * z1 + wTmax;
	*tmin = fmin(*tmax, (vc10 * z1) + (vc11 * z2) + wTmin);
}

#ifdef SWDEBUG
  // since `mvnorm` is static we cannot do unit tests unless we set it up
  // as an externed function pointer
  void (*test_mvnorm)(RealD *, RealD *, RealD, RealD, RealD, RealD,
  				      RealD, pcg32_random_t*, LOG_INFO*) = &mvnorm;
#endif



/* =================================================== */
/*             Global Function Definitions             */
/* --------------------------------------------------- */

/**
 * @brief Initialize all possible pointers in the array, SW_OUTPUT, and
 *		SW_GEN_OUT to NULL
 *
 * @param[in,out] SW_Markov Struct of type SW_MARKOV which holds values
 * 		related to temperature and weather generator
*/
void SW_MKV_init_ptrs(SW_MARKOV* SW_Markov) {
	SW_Markov->wetprob = NULL;
	SW_Markov->dryprob = NULL;
	SW_Markov->avg_ppt = NULL;
	SW_Markov->std_ppt = NULL;
	SW_Markov->cfxw = NULL;
	SW_Markov->cfxd = NULL;
	SW_Markov->cfnw = NULL;
	SW_Markov->cfnd = NULL;
}

/**
@brief Markov constructor for global variables.

@param[in] rng_seed Initial state for Markov
@param[out] SW_Markov Struct of type SW_MARKOV which holds values
	related to temperature and weather generator
@param[in,out] LogInfo Holds information dealing with logfile output
*/
void SW_MKV_construct(unsigned long rng_seed, SW_MARKOV* SW_Markov,
					  LOG_INFO* LogInfo) {
	/* =================================================== */
	size_t s = sizeof(RealD);

	/* Set seed of `markov_rng`
	  - SOILWAT2: set seed here
	  - STEPWAT2: `main()` uses `Globals.randseed` to (re-)set for each iteration
	  - rSOILWAT2: R API handles RNGs
	*/
	#if defined(SOILWAT)
		RandSeed(rng_seed, 1u, &SW_Markov->markov_rng);
	#else
		(void) rng_seed; // Silence compiler flag `-Wunused-parameter`
	#endif

	SW_Markov->ppt_events = 0;

	SW_Markov->wetprob = (RealD *)
						Mem_Calloc(MAX_DAYS, s, "SW_MKV_construct", LogInfo);
    if(LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }
	SW_Markov->dryprob = (RealD *)
						Mem_Calloc(MAX_DAYS, s, "SW_MKV_construct", LogInfo);
    if(LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }
	SW_Markov->avg_ppt = (RealD *)
						Mem_Calloc(MAX_DAYS, s, "SW_MKV_construct", LogInfo);
    if(LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }
	SW_Markov->std_ppt = (RealD *)
						Mem_Calloc(MAX_DAYS, s, "SW_MKV_construct", LogInfo);
    if(LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }
	SW_Markov->cfxw = (RealD *)
						Mem_Calloc(MAX_DAYS, s, "SW_MKV_construct", LogInfo);
    if(LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }
	SW_Markov->cfxd = (RealD *)
						Mem_Calloc(MAX_DAYS, s, "SW_MKV_construct", LogInfo);
    if(LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }
	SW_Markov->cfnw = (RealD *)
						Mem_Calloc(MAX_DAYS, s, "SW_MKV_construct", LogInfo);
    if(LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }
	SW_Markov->cfnd = (RealD *)
						Mem_Calloc(MAX_DAYS, s, "SW_MKV_construct", LogInfo);
}

/**
@brief Markov deconstructor; frees up memory space.

@param[in,out] SW_Markov Struct of type SW_MARKOV which holds values
	related to temperature and weather generator
*/
void SW_MKV_deconstruct(SW_MARKOV* SW_Markov)
{
	if (!isnull(SW_Markov->wetprob)) {
		Mem_Free(SW_Markov->wetprob);
		SW_Markov->wetprob = NULL;
	}

	if (!isnull(SW_Markov->dryprob)) {
		Mem_Free(SW_Markov->dryprob);
		SW_Markov->dryprob = NULL;
	}

	if (!isnull(SW_Markov->avg_ppt)) {
		Mem_Free(SW_Markov->avg_ppt);
		SW_Markov->avg_ppt = NULL;
	}

	if (!isnull(SW_Markov->std_ppt)) {
		Mem_Free(SW_Markov->std_ppt);
		SW_Markov->std_ppt = NULL;
	}

	if (!isnull(SW_Markov->cfxw)) {
		Mem_Free(SW_Markov->cfxw);
		SW_Markov->cfxw = NULL;
	}

	if (!isnull(SW_Markov->cfxd)) {
		Mem_Free(SW_Markov->cfxd);
		SW_Markov->cfxd = NULL;
	}

	if (!isnull(SW_Markov->cfnw)) {
		Mem_Free(SW_Markov->cfnw);
		SW_Markov->cfnw = NULL;
	}

	if (!isnull(SW_Markov->cfnd)) {
		Mem_Free(SW_Markov->cfnd);
		SW_Markov->cfnd = NULL;
	}
}

/**
@brief Calculates precipitation and temperature.

@param[in,out] SW_Markov Struct of type SW_MARKOV which holds values
	related to temperature and weather generator
@param[in] doy0 Day of the year (base0).
@param[in] year Current year in simulation
@param[out] *tmax Maximum temperature (&deg;C).
@param[out] *tmin Mininum temperature (&deg;C).
@param[out] *rain Rainfall (cm).
@param[in,out] LogInfo Holds information dealing with logfile output

@sideeffect *tmax Updated maximum temperature (&deg;C).
@sideeffect *tmin Updated minimum temperature (&deg;C).
@sideeffect *rain Updated rainfall (cm).
*/
void SW_MKV_today(SW_MARKOV* SW_Markov, TimeInt doy0, TimeInt year,
			RealD *tmax, RealD *tmin, RealD *rain, LOG_INFO* LogInfo) {
	/* =================================================== */
	/* enter with rain == yesterday's ppt, doy0 as array index: [0, 365] = doy - 1
	 * leave with rain == today's ppt
	 */
	TimeInt week;
	RealF prob, p, x;

	#ifdef SWDEBUG
	short debug = 0;
	#endif

	#ifdef SWDEBUG
	if (debug) {
		swprintf(
			"mkv(before): yr=%u/doy0=%u: ppt=%.3f, tmax=%.3f, tmin=%.3f\n",
			year, doy0, *rain, *tmax, *tmin
		);
	}
	#endif

	/* Calculate Precipitation:
		prop = probability that it precipitates today depending on whether it
			was wet (precipitated) yesterday `wetprob` or
			whether it was dry yesterday `dryprob` */
	prob = (GT(*rain, 0.0)) ? SW_Markov->wetprob[doy0] : SW_Markov->dryprob[doy0];

	p = RandUni(&SW_Markov->markov_rng);
	if (LE(p, prob)) {
		x = RandNorm(SW_Markov->avg_ppt[doy0],
							SW_Markov->std_ppt[doy0], &SW_Markov->markov_rng);
		*rain = fmax(0., x);
	} else {
		*rain = 0.;
	}

	if (GT(*rain, 0.)) {
		SW_Markov->ppt_events++;
	}

	/* Calculate temperature */
	week = doy2week(doy0 + 1);

	mvnorm(tmax, tmin,
		SW_Markov->u_cov[week][0],     // mean weekly maximum daily temp
		SW_Markov->u_cov[week][1],     // mean weekly minimum daily temp
		SW_Markov->v_cov[week][0][0],  // mean weekly variance of maximum daily temp
		SW_Markov->v_cov[week][1][1],  // mean weekly variance of minimum daily temp
		SW_Markov->v_cov[week][1][0],  // mean weekly covariance of min/max daily temp
		&SW_Markov->markov_rng,
		LogInfo
	);
    if(LogInfo->stopRun) {
        return; // Exit the function prematurely due to error
    }

	temp_correct_wetdry(tmax, tmin, *rain,
		SW_Markov->cfxw[week],  // correction factor for tmax for wet days
		SW_Markov->cfxd[week],  // correction factor for tmax for dry days
		SW_Markov->cfnw[week],  // correction factor for tmin for wet days
		SW_Markov->cfnd[week]   // correction factor for tmin for dry days
	);

	#ifdef SWDEBUG
	if (debug) {
		swprintf(
			"mkv(after): yr=%u/doy0=%u/week=%u: ppt=%.3f, tmax=%.3f, tmin=%.3f\n",
			year, doy0, week, *rain, *tmax, *tmin
		);
	}
	#else
		/* Silence compiler for year not being used if SWDEBUG isn't defined */
		(void) year;
	#endif

}
/**
@brief Reads prob file in and checks input variables for errors, then stores files in SW_Markov.

@param[in] InFiles Array of program in/output files
@param[out] SW_Markov Struct of type SW_MARKOV which holds values
	related to temperature and weather generator
@param[in,out] LogInfo Holds information dealing with logfile output

@return swTRUE Returns true if prob file is correctly opened and closed.
*/
Bool SW_MKV_read_prob(char *InFiles[], SW_MARKOV* SW_Markov,
					  LOG_INFO* LogInfo) {
	/* =================================================== */
	const int nitems = 5;
	FILE *f;
	int lineno = 0, day, x;
	RealF wet, dry, avg, std;
	char inbuf[MAX_FILENAMESIZE];

	/* note that Files.read() must be called prior to this. */
	char *MyFileName = InFiles[eMarkovProb];

	if (NULL == (f = fopen(MyFileName, "r")))
		return swFALSE;

	while (GetALine(f, inbuf)) {
		if (lineno++ == MAX_DAYS)
			break; /* skip extra lines */

		x = sscanf(inbuf, "%d %f %f %f %f",
			&day, &wet, &dry, &avg, &std);

		// Check that text file is ok:
		if (x < nitems) {
			CloseFile(&f, LogInfo);
			LogError(LogInfo, LOGERROR, "Too few values in"\
					" line %d of file %s\n",
					lineno,
					MyFileName);
            return swFALSE; // Exit function prematurely due to error
		}

		// Check that input values meet requirements:

		// day is a real calendar day
		if (!isfinite((float) day) || day < 1 || day > MAX_DAYS)
		{
			CloseFile(&f, LogInfo);
			LogError(LogInfo, LOGERROR, "'day' = %d is out of range"\
					" in line %d of file %s\n",
					day, lineno, MyFileName);
            return swFALSE; // Exit function prematurely due to error
		}

		// Probabilities are in [0, 1]
		if (!isfinite(wet) || LT(wet, 0.) || GT(wet, 1.) ||
				!isfinite(dry) || LT(dry, 0.) || GT(dry, 1.))
		{
			CloseFile(&f, LogInfo);
			LogError(LogInfo, LOGERROR, "Probabilities of being wet = %f"\
					" and/or of being dry = %f are out of range in line"\
					" %d of file %s\n",
					wet, dry, lineno, MyFileName);
            return swFALSE; // Exit function prematurely due to error
		}

		// Mean and SD of daily precipitation are >= 0
		if (!isfinite(avg) || LT(avg, 0.) || !isfinite(std) || LT(std, 0.))
		{
			CloseFile(&f, LogInfo);
			LogError(LogInfo, LOGERROR, "Mean daily precipitation"\
					" = %f and/or SD = %f are out of range in line"\
					" %d of file %s\n",
					avg, std, lineno, MyFileName);
            return swFALSE; // Exit function prematurely due to error
		}

		// Store values in `SW_Markov`
		day--; // base1 -> base0

		SW_Markov->wetprob[day] = wet; // probability of being wet today given a wet yesterday
		SW_Markov->dryprob[day] = dry; // probability of being wet today given a dry yesterday
		SW_Markov->avg_ppt[day] = avg; // mean precip (cm) of wet days
		SW_Markov->std_ppt[day] = std; // std dev. for precip of wet days
	}

	CloseFile(&f, LogInfo);

	return swTRUE;
}

/**
@brief Reads cov file in and checks input variables for errors, then stores files in SW_Markov.

@param[in] InFiles Array of program in/output files
@param[out] SW_Markov Struct of type SW_MARKOV which holds values
	related to temperature and weather generator
@param[in,out] LogInfo Holds information dealing with logfile output

@return Returns true if cov file is correctly opened and closed.
*/
Bool SW_MKV_read_cov(char *InFiles[], SW_MARKOV* SW_Markov, LOG_INFO* LogInfo) {
	/* =================================================== */
	const int nitems = 11;
	FILE *f;
	int lineno = 0, week, x;
	char inbuf[MAX_FILENAMESIZE];
	RealF t1, t2, t3, t4, t5, t6, cfxw, cfxd, cfnw, cfnd;

	char *MyFileName = InFiles[eMarkovCov];

	if (NULL == (f = fopen(MyFileName, "r")))
		return swFALSE;

	while (GetALine(f, inbuf)) {
		if (lineno++ == MAX_WEEKS)
			break; /* skip extra lines */

		x = sscanf(inbuf, "%d %f %f %f %f %f %f %f %f %f %f",
			&week, &t1, &t2, &t3, &t4, &t5, &t6, &cfxw, &cfxd, &cfnw, &cfnd);

		// Check that text file is ok:
		if (x < nitems) {
			CloseFile(&f, LogInfo);
			LogError(LogInfo, LOGERROR, "Too few values in line"\
					" %d of file %s\n",
					lineno, MyFileName);
            return swFALSE; // Exit function prematurely due to error
		}

		// week is a real calendar week
		if (!isfinite((float) week) || week < 1 || week > MAX_WEEKS)
		{
			CloseFile(&f, LogInfo);
			LogError(LogInfo, LOGERROR, "'week' = %d is out of range"\
					" in line %d of file %s\n",
					week, lineno, MyFileName);
            return swFALSE; // Exit function prematurely due to error
		}

		// Mean weekly temperature values are real numbers
		if (!isfinite(t1) || !isfinite(t2))
		{
			CloseFile(&f, LogInfo);
			LogError(LogInfo, LOGERROR, "Mean weekly temperature"\
					" (max = %f and/or min = %f) are not real numbers"\
					" in line %d of file %s\n",
					t1, t2, lineno, MyFileName);
            return swFALSE; // Exit function prematurely due to error
		}

		// Covariance values are finite
		if (!isfinite(t3) || !isfinite(t4) || !isfinite(t5) || !isfinite(t6))
		{
			CloseFile(&f, LogInfo);
			LogError(LogInfo, LOGERROR, "One of the covariance values is"\
					" not a real number (t3 = %f; t4 = %f; t5 = %f; t6 = %f)"\
					" in line %d of file %s\n",
					t3, t4, t5, t6, lineno, MyFileName);
            return swFALSE; // Exit function prematurely due to error
		}

		// Correction factors are real numbers
		if (!isfinite(cfxw) || !isfinite(cfxd) ||
				!isfinite(cfnw) || !isfinite(cfnd))
		{
			CloseFile(&f, LogInfo);
			LogError(LogInfo, LOGERROR, "One of the correction factor is not"\
					" a real number (cfxw = %f; cfxd = %f; cfnw = %f; cfnd = %f)"\
					" in line %d of file %s\n",
					cfxw, cfxd, cfnw, cfnd, lineno, MyFileName);
            return swFALSE; // Exit function prematurely due to error
		}

		// Store values in `SW_Markov`
		week--; // base1 -> base0

		SW_Markov->u_cov[week][0] = t1;    // mean weekly maximum daily temp
		SW_Markov->u_cov[week][1] = t2;    // mean weekly minimum daily temp
		SW_Markov->v_cov[week][0][0] = t3; // mean weekly variance of maximum daily temp
		SW_Markov->v_cov[week][0][1] = t4; // mean weekly covariance of min/max daily temp
		SW_Markov->v_cov[week][1][0] = t5; // mean weekly covariance of min/max daily temp
		SW_Markov->v_cov[week][1][1] = t6; // mean weekly variance of minimum daily temp
		SW_Markov->cfxw[week] = cfxw;      // correction factor for tmax for wet days
		SW_Markov->cfxd[week] = cfxd;      // correction factor for tmax for dry days
		SW_Markov->cfnw[week] = cfnw;      // correction factor for tmin for wet days
		SW_Markov->cfnd[week] = cfnd;      // correction factor for tmin for dry days
	}

	CloseFile(&f, LogInfo);

	return swTRUE;
}


void SW_MKV_setup(SW_MARKOV* SW_Markov, unsigned long Weather_rng_seed,
	int Weather_genWeathMethod, char *InFiles[], LOG_INFO* LogInfo) {

  SW_MKV_construct(Weather_rng_seed, SW_Markov, LogInfo);
  if(LogInfo->stopRun) {
    return; // Exit function prematurely due to error
  }

  if (!SW_MKV_read_prob(InFiles, SW_Markov, LogInfo) && Weather_genWeathMethod == 2) {
    LogError(
      LogInfo,
      LOGERROR,
      "Weather generator requested but could not open %s",
      InFiles[eMarkovProb]
    );
    return; // Exit function prematurely due to error
  }

  if (!SW_MKV_read_cov(InFiles, SW_Markov, LogInfo) && Weather_genWeathMethod == 2) {
    LogError(
      LogInfo,
      LOGERROR,
      "Weather generator requested but could not open %s",
      InFiles[eMarkovCov]
    );
  }
}
