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
 06/27/2013	(drs)	closed open files if LogError() with LOGERROR is called
 in SW_MKV_read_prob(), SW_MKV_read_cov()
 */
/********************************************************/
/********************************************************/


/* =================================================== */
/*                INCLUDES / DEFINES                   */
/* --------------------------------------------------- */
#include "include/SW_Markov.h"      // for SW_MKV_construct, SW_MKV_deconst...
#include "include/filefuncs.h"      // for LogError, CloseFile, GetALine
#include "include/generic.h"        // for LOGERROR, swFALSE
#include "include/myMemory.h"       // for Mem_Calloc, Mem_Copy
#include "include/rands.h"          // for RandNorm, RandSeed, RandUni
#include "include/SW_datastructs.h" // for SW_MARKOV_INPUTS, LOG_INFO
#include "include/SW_Defines.h"     // for MAX_DAYS, MAX_FILENAMESIZE, TimeInt
#include "include/SW_Files.h"       // for eMarkovCov, eMarkovProb
#include "include/Times.h"          // for doy2week
#include <math.h>                   // for isfinite
#include <stdio.h>                  // for NULL, sscanf, FILE, size_t
#include <stdlib.h>                 // for free

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
static void temp_correct_wetdry(
    double *tmax,
    double *tmin,
    double rain,
    double cfmax_wet,
    double cfmax_dry,
    double cfmin_wet,
    double cfmin_dry
) {

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
// since `temp_correct_wetdry` is static we cannot do unit tests unless we set
// it up as an externed function pointer
// NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables)
void (*test_temp_correct_wetdry)(
    double *, double *, double, double, double, double, double
) = &temp_correct_wetdry;
// NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables)
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
        @param LogInfo Holds information on warnings and errors

    @return Daily minimum (*tmin) and maximum (*tmax) temperature.
*/
static void mvnorm(
    double *tmax,
    double *tmin,
    double wTmax,
    double wTmin,
    double wTmax_var,
    double wTmin_var,
    double wT_covar,
    sw_random_t *markov_rng,
    LOG_INFO *LogInfo
) {
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
     * cwb - 24-Oct-03 -- Note the switch to double (double).
     *       C converts the floats transparently.
     */
    double s;
    double z1;
    double z2;
    double wTmax_sd;
    double vc10;
    double vc11;

    // Gentle, J. E. 2009. Computational statistics. Springer, Dordrecht; New
    // York.
    // ==> mvnorm = mean + A * z
    // where
    //   z = vector of standard normal variates
    //   A = Cholesky factor or the square root of the variance-covariance
    //   matrix

    // 2-dimensional case:
    //   mvnorm1 = mean1 + sd1 * z1
    //   mvnorm2 = mean2 + sd2 * (rho * z1 + sqrt(1 - rho^2) * z2)
    //      where rho(Pearson) = covariance / (sd1 * sd2)

    // mvnorm2 = sd2 * rho * z1 + sd2 * sqrt(1 - rho^2) * z2
    // mvnorm2 = covar / sd1 * z1 + sd2 * sqrt(1 - covar ^ 2 / (var1 * var2)) *
    // z2 mvnorm2 = covar / sd1 * z1 + sqrt(var2 - covar ^ 2 / var1) * z2
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
       and thus, `vc11` becomes `NaN` */
    vc11 = (LE(wTmin_var, s)) ? 0. : sqrt(wTmin_var - s);

    // mvnorm = mean + A * z
    *tmax = wTmax_sd * z1 + wTmax;
    *tmin = fmin(*tmax, (vc10 * z1) + (vc11 * z2) + wTmin);
}

#ifdef SWDEBUG
// since `mvnorm` is static we cannot do unit tests unless we set it up
// as an externed function pointer
// NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables)
void (*test_mvnorm)(double *, double *, double, double, double, double, double, sw_random_t *, LOG_INFO *) =
    &mvnorm;
// NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables)
#endif


/* =================================================== */
/*             Global Function Definitions             */
/* --------------------------------------------------- */

/**
@brief Initialize all possible markov pointers to NULL

@param[in,out] SW_MarkovIn Struct of type SW_MARKOV_INPUTS which holds values
    related to temperature and weather generator
*/
void SW_MKV_init_ptrs(SW_MARKOV_INPUTS *SW_MarkovIn) {
    SW_MarkovIn->wetprob = NULL;
    SW_MarkovIn->dryprob = NULL;
    SW_MarkovIn->avg_ppt = NULL;
    SW_MarkovIn->std_ppt = NULL;
    SW_MarkovIn->cfxw = NULL;
    SW_MarkovIn->cfxd = NULL;
    SW_MarkovIn->cfnw = NULL;
    SW_MarkovIn->cfnd = NULL;
}

/**
@brief Markov constructor for global variables.

@param[in] rng_seed Initial state for MarkovIn
@param[out] SW_MarkovIn Struct of type SW_MARKOV_INPUTS which holds values
    related to temperature and weather generator
*/
void SW_MKV_construct(size_t rng_seed, SW_MARKOV_INPUTS *SW_MarkovIn) {
/* =================================================== */

/* Set seed of `markov_rng`
  - SOILWAT2: set seed here
  - STEPWAT2: `main()` uses `Globals.randseed` to (re-)set for each iteration
  - rSOILWAT2: R API handles RNGs
*/
#if defined(SOILWAT)
    RandSeed(rng_seed, 1u, &SW_MarkovIn->markov_rng);
#else
    (void) rng_seed; // Silence compiler flag `-Wunused-parameter`
#endif

    SW_MarkovIn->ppt_events = 0;
}

/**
@brief Dynamically allocate memory for SW_MARKOV elements

@param[out] SW_MarkovIn Struct of type SW_MARKOV_INPUTS which holds values
    related to temperature and weather generator
@param[out] LogInfo Holds information on warnings and errors
*/
void allocateMKV(SW_MARKOV_INPUTS *SW_MarkovIn, LOG_INFO *LogInfo) {
    size_t s = sizeof(double);

    SW_MarkovIn->wetprob =
        (double *) Mem_Calloc(MAX_DAYS, s, "allocateMKV", LogInfo);
    if (LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

    SW_MarkovIn->dryprob =
        (double *) Mem_Calloc(MAX_DAYS, s, "allocateMKV", LogInfo);
    if (LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

    SW_MarkovIn->avg_ppt =
        (double *) Mem_Calloc(MAX_DAYS, s, "allocateMKV", LogInfo);
    if (LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

    SW_MarkovIn->std_ppt =
        (double *) Mem_Calloc(MAX_DAYS, s, "allocateMKV", LogInfo);
    if (LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

    SW_MarkovIn->cfxw =
        (double *) Mem_Calloc(MAX_DAYS, s, "allocateMKV", LogInfo);
    if (LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

    SW_MarkovIn->cfxd =
        (double *) Mem_Calloc(MAX_DAYS, s, "allocateMKV", LogInfo);
    if (LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

    SW_MarkovIn->cfnw =
        (double *) Mem_Calloc(MAX_DAYS, s, "allocateMKV", LogInfo);
    if (LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

    SW_MarkovIn->cfnd =
        (double *) Mem_Calloc(MAX_DAYS, s, "allocateMKV", LogInfo);
}

/**
@brief Free dynamically allocated memory in SW_MARKOV

@param[in,out] SW_MarkovIn Struct of type SW_MARKOV_INPUTS which holds values
    related to temperature and weather generator
*/
void deallocateMKV(SW_MARKOV_INPUTS *SW_MarkovIn) {

    if (!isnull(SW_MarkovIn->wetprob)) {
        free(SW_MarkovIn->wetprob);
        SW_MarkovIn->wetprob = NULL;
    }

    if (!isnull(SW_MarkovIn->dryprob)) {
        free(SW_MarkovIn->dryprob);
        SW_MarkovIn->dryprob = NULL;
    }

    if (!isnull(SW_MarkovIn->avg_ppt)) {
        free(SW_MarkovIn->avg_ppt);
        SW_MarkovIn->avg_ppt = NULL;
    }

    if (!isnull(SW_MarkovIn->std_ppt)) {
        free(SW_MarkovIn->std_ppt);
        SW_MarkovIn->std_ppt = NULL;
    }

    if (!isnull(SW_MarkovIn->cfxw)) {
        free(SW_MarkovIn->cfxw);
        SW_MarkovIn->cfxw = NULL;
    }

    if (!isnull(SW_MarkovIn->cfxd)) {
        free(SW_MarkovIn->cfxd);
        SW_MarkovIn->cfxd = NULL;
    }

    if (!isnull(SW_MarkovIn->cfnw)) {
        free(SW_MarkovIn->cfnw);
        SW_MarkovIn->cfnw = NULL;
    }

    if (!isnull(SW_MarkovIn->cfnd)) {
        free(SW_MarkovIn->cfnd);
        SW_MarkovIn->cfnd = NULL;
    }
}

/**
@brief Markov deconstructor; frees up memory space.

@param[in,out] SW_MarkovIn Struct of type SW_MARKOV_INPUTS which holds values
    related to temperature and weather generator
*/
void SW_MKV_deconstruct(SW_MARKOV_INPUTS *SW_MarkovIn) {
    deallocateMKV(SW_MarkovIn);
}

/** Copy SW_MARKOV memory

 @param[out] dest_MKV Struct of type SW_MARKOV_INPUTS which holds parameters
 for the weather generator

 @param[in] template_MKV Struct of type SW_MARKOV_INPUTS which holds parameters
 for the weather generator
*/
void copyMKV(SW_MARKOV_INPUTS *dest_MKV, SW_MARKOV_INPUTS *template_MKV) {
    size_t s = sizeof(double) * MAX_DAYS; /* see `allocateMKV()` */

    Mem_Copy(dest_MKV->wetprob, template_MKV->wetprob, s);
    Mem_Copy(dest_MKV->dryprob, template_MKV->dryprob, s);
    Mem_Copy(dest_MKV->avg_ppt, template_MKV->avg_ppt, s);
    Mem_Copy(dest_MKV->std_ppt, template_MKV->std_ppt, s);
    Mem_Copy(dest_MKV->cfxw, template_MKV->cfxw, s);
    Mem_Copy(dest_MKV->cfxd, template_MKV->cfxd, s);
    Mem_Copy(dest_MKV->cfnw, template_MKV->cfnw, s);
    Mem_Copy(dest_MKV->cfnd, template_MKV->cfnd, s);
}

/**
@brief Calculates precipitation and temperature.

@param[in,out] SW_MarkovIn Struct of type SW_MARKOV_INPUTS which holds values
        related to temperature and weather generator
@param[in] doy0 Day of the year (base0).
@param[in] year Current year in simulation
@param[out] *tmax Maximum temperature (&deg;C).
@param[out] *tmin Mininum temperature (&deg;C).
@param[out] *rain Rainfall (cm).
@param[out] LogInfo Holds information on warnings and errors

@sideeffect *tmax Updated maximum temperature (&deg;C).
@sideeffect *tmin Updated minimum temperature (&deg;C).
@sideeffect *rain Updated rainfall (cm).
*/
void SW_MKV_today(
    SW_MARKOV_INPUTS *SW_MarkovIn,
    TimeInt doy0,
    TimeInt year,
    double *tmax,
    double *tmin,
    double *rain,
    LOG_INFO *LogInfo
) {
    /* =================================================== */
    /* enter with rain == yesterday's ppt, doy0 as array index: [0, 365] = doy -
     * 1 leave with rain == today's ppt
     */
#ifdef SWDEBUG
    short debug = 0;
#endif

    TimeInt week;
    double prob;
    double p;
    double x;

#ifdef SWDEBUG
    if (debug) {
        sw_printf(
            "mkv(before): yr=%u/doy0=%u: ppt=%.3f, tmax=%.3f, tmin=%.3f\n",
            year,
            doy0,
            *rain,
            *tmax,
            *tmin
        );
    }
#endif

    /* Calculate Precipitation:
            prop = probability that it precipitates today depending on whether
       it was wet (precipitated) yesterday `wetprob` or whether it was dry
       yesterday `dryprob` */
    prob = (GT(*rain, 0.0)) ? SW_MarkovIn->wetprob[doy0] :
                              SW_MarkovIn->dryprob[doy0];

    p = RandUni(&SW_MarkovIn->markov_rng);
    if (LE(p, prob)) {
        x = RandNorm(
            SW_MarkovIn->avg_ppt[doy0],
            SW_MarkovIn->std_ppt[doy0],
            &SW_MarkovIn->markov_rng
        );
        *rain = fmax(0., x);
    } else {
        *rain = 0.;
    }

    if (GT(*rain, 0.)) {
        SW_MarkovIn->ppt_events++;
    }

    /* Calculate temperature */
    week = doy2week(doy0 + 1);

    mvnorm(
        tmax,
        tmin,
        // mean weekly maximum daily temp
        SW_MarkovIn->u_cov[week][0],
        // mean weekly minimum daily temp
        SW_MarkovIn->u_cov[week][1],
        // mean weekly variance of maximum daily temp
        SW_MarkovIn->v_cov[week][0][0],
        // mean weekly variance of minimum daily temp
        SW_MarkovIn->v_cov[week][1][1],
        // mean weekly covariance of min/max daily temp
        SW_MarkovIn->v_cov[week][1][0],
        &SW_MarkovIn->markov_rng,
        LogInfo
    );
    if (LogInfo->stopRun) {
        return; // Exit the function prematurely due to error
    }

    temp_correct_wetdry(
        tmax,
        tmin,
        *rain,
        // correction factor for tmax for wet days
        SW_MarkovIn->cfxw[week],
        // correction factor for tmax for dry days
        SW_MarkovIn->cfxd[week],
        // correction factor for tmin for wet days
        SW_MarkovIn->cfnw[week],
        // correction factor for tmin for dry days
        SW_MarkovIn->cfnd[week]
    );

#ifdef SWDEBUG
    if (debug) {
        sw_printf(
            "mkv(after): yr=%u/doy0=%u/week=%u: ppt=%.3f, tmax=%.3f, "
            "tmin=%.3f\n",
            year,
            doy0,
            week,
            *rain,
            *tmax,
            *tmin
        );
    }
#else
    /* Silence compiler for year not being used if SWDEBUG isn't defined */
    (void) year;
#endif
}

/**
@brief Reads prob file in and checks input variables for errors, then stores
files in SW_MarkovIn.

@param[in] txtInFiles Array of program in/output files
@param[out] SW_MarkovIn Struct of type SW_MARKOV_INPUTS which holds values
        related to temperature and weather generator
@param[out] LogInfo Holds information on warnings and errors

@return swTRUE Returns true if prob file is correctly opened and closed.
*/
Bool SW_MKV_read_prob(
    char *txtInFiles[], SW_MARKOV_INPUTS *SW_MarkovIn, LOG_INFO *LogInfo
) {
    /* =================================================== */
    const int nitems = 5;
    const int numFloatInStrings = 4;

    FILE *f;
    int lineno = 0;
    int day;
    int x;
    int index;
    double wet;
    double dry;
    double avg;
    double std;
    double *doubleVals[4] = {&wet, &dry, &avg, &std};
    char inbuf[MAX_FILENAMESIZE];
    char dayStr[4] = {'\0'};
    char inDoubleStrs[4][20] = {{'\0'}};

    Bool result = swTRUE;

    /* note that Files.read() must be called prior to this. */
    char *MyFileName = txtInFiles[eMarkovProb];
    f = OpenFile(MyFileName, "r", LogInfo);
    if (LogInfo->stopRun) {
        return swFALSE;
    }

    while (GetALine(f, inbuf, MAX_FILENAMESIZE)) {
        if (lineno++ == MAX_DAYS) {
            break; /* skip extra lines */
        }

        x = sscanf(
            inbuf,
            "%3s %19s %19s %19s %19s",
            dayStr,
            inDoubleStrs[0],
            inDoubleStrs[1],
            inDoubleStrs[2],
            inDoubleStrs[3]
        );

        day = sw_strtoi(dayStr, MyFileName, LogInfo);
        if (LogInfo->stopRun) {
            result = swFALSE;
            goto closeFile;
        }

        for (index = 0; index < numFloatInStrings; index++) {
            *(doubleVals[index]) =
                sw_strtod(inDoubleStrs[index], MyFileName, LogInfo);
            if (LogInfo->stopRun) {
                result = swFALSE;
                goto closeFile;
            }
        }

        // Check that text file is ok:
        if (x < nitems) {
            LogError(
                LogInfo,
                LOGERROR,
                "Too few values in"
                " line %d of file %s\n",
                lineno,
                MyFileName
            );
            result = swFALSE;
            goto closeFile;
        }

        // Check that input values meet requirements:

        // day is a real calendar day
        if (!isfinite((float) day) || day < 1 || day > MAX_DAYS) {
            LogError(
                LogInfo,
                LOGERROR,
                "'day' = %d is out of range"
                " in line %d of file %s\n",
                day,
                lineno,
                MyFileName
            );
            result = swFALSE;
            goto closeFile;
        }

        // Probabilities are in [0, 1]
        if (!isfinite(wet) || LT(wet, 0.) || GT(wet, 1.) || !isfinite(dry) ||
            LT(dry, 0.) || GT(dry, 1.)) {
            LogError(
                LogInfo,
                LOGERROR,
                "Probabilities of being wet = %f"
                " and/or of being dry = %f are out of range in line"
                " %d of file %s\n",
                wet,
                dry,
                lineno,
                MyFileName
            );
            result = swFALSE;
            goto closeFile;
        }

        // Mean and SD of daily precipitation are >= 0
        if (!isfinite(avg) || LT(avg, 0.) || !isfinite(std) || LT(std, 0.)) {
            LogError(
                LogInfo,
                LOGERROR,
                "Mean daily precipitation"
                " = %f and/or SD = %f are out of range in line"
                " %d of file %s\n",
                avg,
                std,
                lineno,
                MyFileName
            );
            result = swFALSE;
            goto closeFile;
        }

        // Store values in `SW_MarkovIn`
        day--; // base1 -> base0

        // probability of being wet today given a wet yesterday
        SW_MarkovIn->wetprob[day] = wet;
        // probability of being wet today given a dry yesterday
        SW_MarkovIn->dryprob[day] = dry;
        // mean precip (cm) of wet days
        SW_MarkovIn->avg_ppt[day] = avg;
        // std dev. for precip of wet days
        SW_MarkovIn->std_ppt[day] = std;
    }

closeFile: { CloseFile(&f, LogInfo); }

    return result;
}

/**
@brief Reads cov file in and checks input variables for errors, then stores
files in SW_MarkovIn.

@param[in] txtInFiles Array of program in/output files
@param[out] SW_MarkovIn Struct of type SW_MARKOV_INPUTS which holds values
        related to temperature and weather generator
@param[out] LogInfo Holds information on warnings and errors

@return Returns true if cov file is correctly opened and closed.
*/
Bool SW_MKV_read_cov(
    char *txtInFiles[], SW_MARKOV_INPUTS *SW_MarkovIn, LOG_INFO *LogInfo
) {
    /* =================================================== */
    const int nitems = 11;
    FILE *f;
    int lineno = 0;
    int week;
    int x;
    int index;
    char inbuf[MAX_FILENAMESIZE];
    double t1;
    double t2;
    double t3;
    double t4;
    double t5;
    double t6;
    double cfxw;
    double cfxd;
    double cfnw;
    double cfnd;
    double *doubleVals[] = {
        &t1, &t2, &t3, &t4, &t5, &t6, &cfxw, &cfxd, &cfnw, &cfnd
    };
    char weekStr[3] = {'\0'};
    char inDoubleStrs[10][20] = {{'\0'}};
    const int numDoubleVals = 10;
    Bool result = swTRUE;

    char *MyFileName = txtInFiles[eMarkovCov];
    f = OpenFile(MyFileName, "r", LogInfo);
    if (LogInfo->stopRun) {
        return swFALSE;
    }

    while (GetALine(f, inbuf, MAX_FILENAMESIZE)) {
        if (lineno++ == MAX_WEEKS) {
            break; /* skip extra lines */
        }

        x = sscanf(
            inbuf,
            "%2s %19s %19s %19s %19s %19s %19s %19s %19s %19s %19s",
            weekStr,
            inDoubleStrs[0],
            inDoubleStrs[1],
            inDoubleStrs[2],
            inDoubleStrs[3],
            inDoubleStrs[4],
            inDoubleStrs[5],
            inDoubleStrs[6],
            inDoubleStrs[7],
            inDoubleStrs[8],
            inDoubleStrs[9]
        );

        // Check that text file is ok:
        if (x < nitems) {
            LogError(
                LogInfo,
                LOGERROR,
                "Too few values in line"
                " %d of file %s\n",
                lineno,
                MyFileName
            );
            result = swFALSE;
            goto closeFile;
        }

        week = sw_strtoi(weekStr, MyFileName, LogInfo);
        if (LogInfo->stopRun) {
            result = swFALSE;
            goto closeFile;
        }

        for (index = 0; index < numDoubleVals; index++) {
            *(doubleVals[index]) =
                sw_strtod(inDoubleStrs[index], MyFileName, LogInfo);
            if (LogInfo->stopRun) {
                result = swFALSE;
                goto closeFile;
            }
        }

        // week is a real calendar week
        if (!isfinite((float) week) || week < 1 || week > MAX_WEEKS) {
            LogError(
                LogInfo,
                LOGERROR,
                "'week' = %d is out of range"
                " in line %d of file %s\n",
                week,
                lineno,
                MyFileName
            );
            result = swFALSE;
            goto closeFile;
        }

        // Mean weekly temperature values are real numbers
        if (!isfinite(t1) || !isfinite(t2)) {
            LogError(
                LogInfo,
                LOGERROR,
                "Mean weekly temperature"
                " (max = %f and/or min = %f) are not real numbers"
                " in line %d of file %s\n",
                t1,
                t2,
                lineno,
                MyFileName
            );
            result = swFALSE;
            goto closeFile;
        }

        // Covariance values are finite
        if (!isfinite(t3) || !isfinite(t4) || !isfinite(t5) || !isfinite(t6)) {
            LogError(
                LogInfo,
                LOGERROR,
                "One of the covariance values is"
                " not a real number (t3 = %f; t4 = %f; t5 = %f; t6 = %f)"
                " in line %d of file %s\n",
                t3,
                t4,
                t5,
                t6,
                lineno,
                MyFileName
            );
            result = swFALSE;
            goto closeFile;
        }

        // Correction factors are real numbers
        if (!isfinite(cfxw) || !isfinite(cfxd) || !isfinite(cfnw) ||
            !isfinite(cfnd)) {
            LogError(
                LogInfo,
                LOGERROR,
                "One of the correction factor is not"
                " a real number (cfxw = %f; cfxd = %f; cfnw = %f; cfnd = %f)"
                " in line %d of file %s\n",
                cfxw,
                cfxd,
                cfnw,
                cfnd,
                lineno,
                MyFileName
            );
            result = swFALSE;
            goto closeFile;
        }

        // Store values in `SW_MarkovIn`
        week--; // base1 -> base0

        // mean weekly maximum daily temp
        SW_MarkovIn->u_cov[week][0] = t1;
        // mean weekly minimum daily temp
        SW_MarkovIn->u_cov[week][1] = t2;
        // mean weekly variance of maximum daily temp
        SW_MarkovIn->v_cov[week][0][0] = t3;
        // mean weekly covariance of min/max daily temp
        SW_MarkovIn->v_cov[week][0][1] = t4;
        // mean weekly covariance of min/max daily temp
        SW_MarkovIn->v_cov[week][1][0] = t5;
        // mean weekly variance of minimum daily temp
        SW_MarkovIn->v_cov[week][1][1] = t6;
        // correction factor for tmax for wet days
        SW_MarkovIn->cfxw[week] = cfxw;
        // correction factor for tmax for dry days
        SW_MarkovIn->cfxd[week] = cfxd;
        // correction factor for tmin for wet days
        SW_MarkovIn->cfnw[week] = cfnw;
        // correction factor for tmin for dry days
        SW_MarkovIn->cfnd[week] = cfnd;
    }

closeFile: { CloseFile(&f, LogInfo); }

    return result;
}

void SW_MKV_setup(
    SW_MARKOV_INPUTS *SW_MarkovIn,
    size_t Weather_rng_seed,
    unsigned int Weather_genWeathMethod,
    char *txtInFiles[],
    LOG_INFO *LogInfo
) {

    Bool read_prob;
    Bool read_cov;

    SW_MKV_construct(Weather_rng_seed, SW_MarkovIn);
    allocateMKV(SW_MarkovIn, LogInfo);
    if (LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

    read_prob = SW_MKV_read_prob(txtInFiles, SW_MarkovIn, LogInfo);
    if (LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

    if (!read_prob && Weather_genWeathMethod == 2) {
        LogError(
            LogInfo,
            LOGERROR,
            "Weather generator requested but could not open %s",
            txtInFiles[eMarkovProb]
        );
        return; // Exit function prematurely due to error
    }

    read_cov = SW_MKV_read_cov(txtInFiles, SW_MarkovIn, LogInfo);
    if (LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

    if (!read_cov && Weather_genWeathMethod == 2) {
        LogError(
            LogInfo,
            LOGERROR,
            "Weather generator requested but could not open %s",
            txtInFiles[eMarkovCov]
        );
    }
}
