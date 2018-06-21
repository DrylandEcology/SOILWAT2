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
#include "generic.h"
#include "filefuncs.h"
#include "rands.h"
#include "myMemory.h"
#include "SW_Defines.h"
#include "SW_Files.h"
#include "SW_Weather.h"
#include "SW_Model.h"
#include "SW_Markov.h"

/* =================================================== */
/*                  Global Variables                   */
/* --------------------------------------------------- */
extern SW_MODEL SW_Model;
SW_MARKOV SW_Markov; /* declared here, externed elsewhere */

/* =================================================== */
/*                Module-Level Variables               */
/* --------------------------------------------------- */

static char *MyFileName;
static RealD _vcov[2][2], _ucov[2];

/* =================================================== */
/* =================================================== */
/*             Private Function Definitions            */
/* --------------------------------------------------- */

static void temp_correct(TimeInt doy,RealD *tmax, RealD *tmin, RealD *rain) {

    RealD tx, tn,cfxw,cfxd,cfnw,cfnd,rn;

    rn = *rain;
    cfxw = SW_Markov.cfxw[doy];
    cfnw = SW_Markov.cfnw[doy];
    cfxd = SW_Markov.cfxd[doy];
    cfnd = SW_Markov.cfnd[doy];
    tx = *tmax;
    tn = *tmin;

    if (rn > 0.) {
            if (tx < 0. ) { //if temp is less than 0 special case
                tx = tx*((1.0 - cfxw)+1.0);
            }
            else { tx = tx*cfxw;
            }


            if (tn < 0) {//if temp is less than 0 special case
                tn = tn*((1.0 - cfnw)+1.0);
            }
            else {
            tn = tn*cfnw;
            }
    }

 //apply correction factor to temperature
        if (rn <= 0.) {
            if (tx < 0. ) {//if temp is less than 0 special case
                tx = tx*((1.0 - cfxd)+1.0);
            }
            else {
                tx = tx*cfxd;
            }

            if (tn < 0.) { //if temp is less than 0 special case
                tn = tn*((1.0 - cfnd)+1.0);
            }
           else {
            tn = tn*cfnd;
           }
        }


        *tmin = tn;
        *tmax = tx;

}

static void mvnorm(RealD *tmax, RealD *tmin) {
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
	RealD s, z1, z2, vc00 = _vcov[0][0], vc10 = _vcov[1][0], vc11 = _vcov[1][1];

	vc00 = sqrt(vc00);
	vc10 = (GT(vc00, 0.)) ? vc10 / vc00 : 0;
	s = vc10 * vc10;
	if (GT(s,vc11))
		LogError(logfp, LOGFATAL, "\nBad covariance matrix in mvnorm()");
	vc11 = (EQ(vc11, s)) ? 0. : sqrt(vc11 -s);

	z1 = RandNorm(0., 3.5);
	z2 = RandNorm(0., 3.5);
	*tmin = (vc10 * z1) + (vc11 * z2) + _ucov[1];
	*tmax = vc00 * z1 + _ucov[0];

}

/* =================================================== */
/* =================================================== */
/*             Public Function Definitions             */
/* --------------------------------------------------- */

void SW_MKV_construct(void) {
	/* =================================================== */
	SW_MARKOV *m = &SW_Markov;
	size_t s = sizeof(RealD);

	m->wetprob = (RealD *) Mem_Calloc(MAX_DAYS, s, "SW_MKV_construct");
	m->dryprob = (RealD *) Mem_Calloc(MAX_DAYS, s, "SW_MKV_construct");
	m->avg_ppt = (RealD *) Mem_Calloc(MAX_DAYS, s, "SW_MKV_construct");
	m->std_ppt = (RealD *) Mem_Calloc(MAX_DAYS, s, "SW_MKV_construct");
    m->cfxw = (RealD *) Mem_Calloc(MAX_DAYS, s, "SW_MKV_construct");
	m->cfxd = (RealD *) Mem_Calloc(MAX_DAYS, s, "SW_MKV_construct");
	m->cfnw = (RealD *) Mem_Calloc(MAX_DAYS, s, "SW_MKV_construct");
	m->cfnd = (RealD *) Mem_Calloc(MAX_DAYS, s, "SW_MKV_construct");
}

void SW_MKV_deconstruct(void)
{
	if (!isnull(SW_Markov.wetprob)) {
		Mem_Free(SW_Markov.wetprob);
		SW_Markov.wetprob = NULL;
	}

	if (!isnull(SW_Markov.dryprob)) {
		Mem_Free(SW_Markov.dryprob);
		SW_Markov.dryprob = NULL;
	}

	if (!isnull(SW_Markov.avg_ppt)) {
		Mem_Free(SW_Markov.avg_ppt);
		SW_Markov.avg_ppt = NULL;
	}

	if (!isnull(SW_Markov.std_ppt)) {
		Mem_Free(SW_Markov.std_ppt);
		SW_Markov.std_ppt = NULL;
	}

	if (!isnull(SW_Markov.cfxw)) {
		Mem_Free(SW_Markov.cfxw);
		SW_Markov.cfxw = NULL;
	}

	if (!isnull(SW_Markov.cfxd)) {
		Mem_Free(SW_Markov.cfxd);
		SW_Markov.cfxd = NULL;
	}

	if (!isnull(SW_Markov.cfnw)) {
		Mem_Free(SW_Markov.cfnw);
		SW_Markov.cfnw = NULL;
	}

	if (!isnull(SW_Markov.cfnd)) {
		Mem_Free(SW_Markov.cfnd);
		SW_Markov.cfnd = NULL;
	}
}


void SW_MKV_today(TimeInt doy, RealD *tmax, RealD *tmin, RealD *rain) {
	/* =================================================== */
	/* enter with rain == yesterday's ppt, doy as array index
	 * leave with rain == today's ppt
	 */
	TimeInt week;
	RealF prob, p, x;

	/* Calculate Precip */
	prob = (GT(*rain, 0.0)) ? SW_Markov.wetprob[doy] : SW_Markov.dryprob[doy];

	p = RandUni();
	if (LE(p,prob)) {
		x = RandNorm(SW_Markov.avg_ppt[doy], SW_Markov.std_ppt[doy]);
		*rain = max(0., x);
	} else {
		*rain = 0.;
	}

	if (!ZRO(*rain))
		SW_Markov.ppt_events++;

	/* Calculate temperature */
	week = Doy2Week(doy+1);
	memcpy(_vcov, &SW_Markov.v_cov[week], 4 * sizeof(RealD));
	_ucov[0] = SW_Markov.u_cov[week][0];
	_ucov[1] = SW_Markov.u_cov[week][1];
	mvnorm(tmax, tmin);
    temp_correct(doy,tmax,tmin,rain);
}

Bool SW_MKV_read_prob(void) {
	/* =================================================== */
	SW_MARKOV *v = &SW_Markov;
	const int nitems = 5;
	FILE *f;
	int lineno = 0, day, x;
	RealF wet, dry, avg, std, cfxw, cfxd, cfnw, cfnd;

	/* note that Files.read() must be called prior to this. */
	MyFileName = SW_F_name(eMarkovProb);

	if (NULL == (f = fopen(MyFileName, "r")))
		return swFALSE;

	while (GetALine(f, inbuf)) {
		if (++lineno == MAX_DAYS)
			break; /* skip extra lines */

		x = sscanf(inbuf, "%d %f %f %f %f %f %f %f %f", &day, &wet, &dry, &avg, &std, &cfxw, &cfxd, &cfnw, &cfnd);
		if (x < nitems) {
			CloseFile(&f);
			LogError(logfp, LOGFATAL, "\nToo few values in line %d file %s\n", lineno, MyFileName);
		}
		day--;
		v->wetprob[day] = wet;
		v->dryprob[day] = dry;
		v->avg_ppt[day] = avg;
		v->std_ppt[day] = std;
        v->cfxw[day] = cfxw;
		v->cfxd[day] = cfxd;
		v->cfnw[day] = cfnw;
		v->cfnd[day] = cfnd;

	}
	CloseFile(&f);

	return swTRUE;
}


Bool SW_MKV_read_cov(void) {
	/* =================================================== */
	SW_MARKOV *v = &SW_Markov;
	const int nitems = 7;
	FILE *f;
	int lineno = 0, week, x;
	RealF t1, t2, t3, t4, t5, t6;

	MyFileName = SW_F_name(eMarkovCov);

	if (NULL == (f = fopen(MyFileName, "r")))
		return swFALSE;

	while (GetALine(f, inbuf)) {
		if (++lineno == MAX_WEEKS)
			break; /* skip extra lines */

		x = sscanf(inbuf, "%d %f %f %f %f %f %f", &week, &t1, &t2, &t3, &t4, &t5, &t6);
		if (x < nitems) {
			CloseFile(&f);
			LogError(logfp, LOGFATAL, "\nToo few values in line %d file %s\n", lineno, MyFileName);
		}
		week--;
		v->u_cov[week][0] = t1;
		v->u_cov[week][1] = t2;
		v->v_cov[week][0][0] = t3;
		v->v_cov[week][0][1] = t4;
		v->v_cov[week][1][0] = t5;
		v->v_cov[week][1][1] = t6;
	}
	CloseFile(&f);

	return swTRUE;
}


#ifdef DEBUG_MEM
#include "myMemory.h"
/*======================================================*/
void SW_MKV_SetMemoryRefs( void) {
	/* when debugging memory problems, use the bookkeeping
	 code in myMemory.c
	 This routine sets the known memory refs in this module
	 so they can be  checked for leaks, etc.  All refs will
	 have been cleared by a call to ClearMemoryRefs() before
	 this, and will be checked via CheckMemoryRefs() after
	 this, most likely in the main() function.
	 */

	NoteMemoryRef(SW_Markov.wetprob);
	NoteMemoryRef(SW_Markov.dryprob);
	NoteMemoryRef(SW_Markov.avg_ppt);
	NoteMemoryRef(SW_Markov.std_ppt);
	NoteMemoryRef(SW_Markov.cfxw);
	NoteMemoryRef(SW_Markov.cfxd);
	NoteMemoryRef(SW_Markov.cfnw);
	NoteMemoryRef(SW_Markov.cfnd);

}

#endif
