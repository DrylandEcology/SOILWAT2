/********************************************************/
/********************************************************/
/*	Source file: Veg_Estab.c
 Type: module
 Application: SOILWAT - soilwater dynamics simulator
 Purpose: Reads/writes vegetation establishment info.
 History:
 (8/28/01) -- INITIAL CODING - cwb

 8-Sep-03 -- Establishment code works as follows.
 More than one species can be tested per year.
 No more than one establishment per species per year may occur.
 If germination occurs, check environmental conditions
 for establishment.  If a dry period (>max_drydays_postgerm)
 occurs, or temp out of range, kill the plant and
 start over from pregermination state.  Thus, if the
 early estab fails, start over and try again if
 enough time is available.  This is simple but not
 realistic.  Better would be to count and report the
 number of days that would allow establishment which
 would give an index to the number of seedlings
 established in a year.

 20090826 (drs) added return; after LBL_Normal_Exit:

 06/26/2013	(rjm)	closed open files in function SW_VES_read() or if
 LogError() with LOGERROR is called in read_spp()

 08/21/2013	(clk)	changed the line v = SW_VegEstab.parms[ _new_species()
 ]; -> v = SW_VegEstab.parms[ count ], where count = _new_species(); for some
 reason, without this change, a segmenation fault was occuring
 */
/********************************************************/
/********************************************************/


/* =================================================== */
/*                INCLUDES / DEFINES                   */
/* --------------------------------------------------- */

#include "include/SW_VegEstab.h"    // for SW_ESTAB_BARS, SW_GERM_BARS, SW_...
#include "include/filefuncs.h"      // for LogError, CloseFile, GetALine
#include "include/generic.h"        // for IntU, LOGERROR, RealD, isnull, LT
#include "include/myMemory.h"       // for Mem_Calloc, Mem_ReAlloc
#include "include/SW_datastructs.h" // for SW_VEGESTAB_INFO, LOG_INFO, SW_V...
#include "include/SW_Defines.h"     // for TimeInt, eSW_Year, MAX_FILENAMESIZE
#include "include/SW_Files.h"       // for eVegEstab
#include "include/SW_SoilWater.h"   // for SW_SWRC_SWPtoSWC
#include "include/SW_Times.h"       // for Today
#include "include/SW_VegProd.h"     // for key2veg
#include <errno.h>                  // for errno
#include <math.h>                   // for fabs
#include <stdio.h>                  // for NULL, snprintf, FILE, printf
#include <stdlib.h>                 // for free
#include <string.h>                 // for strcpy, strcat, strlen, memset


/* =================================================== */
/*             Private Function Declarations           */
/* --------------------------------------------------- */
static void sanity_check(
    unsigned int sppnum,
    RealD swcBulk_wiltpt[],
    LyrIndex n_transp_lyrs[],
    SW_VEGESTAB_INFO **parms,
    LOG_INFO *LogInfo
);

static void read_spp(
    const char *infile, SW_VEGESTAB *SW_VegEstab, LOG_INFO *LogInfo
);

static void checkit(
    TimeInt doy,
    unsigned int sppnum,
    SW_WEATHER_NOW *wn,
    RealD swcBulk[][MAX_LAYERS],
    TimeInt firstdoy,
    SW_VEGESTAB_INFO **parms
);

static void zero_state(unsigned int sppnum, SW_VEGESTAB_INFO **parms);

/* =================================================== */
/*             Global Function Definitions             */
/* --------------------------------------------------- */

/**
@brief Initialize all possible pointers in SW_VEGESTAB to NULL

@param[in,out] SW_VegEstab Struct of type SW_VEGESTAB holding all
    information about vegetation establishment within the simulation
*/
void SW_VES_init_ptrs(SW_VEGESTAB *SW_VegEstab) {
    OutPeriod pd;

    // Initalize output structures:
    ForEachOutPeriod(pd) {
        SW_VegEstab->p_accu[pd] = NULL;
        SW_VegEstab->p_oagg[pd] = NULL;
    }

    SW_VegEstab->parms = NULL;
    SW_VegEstab->count = 0;
}

/**
@brief Constructor for SW_VegEstab.

@param[out] SW_VegEstab Struct of type SW_VEGESTAB holding all
    information about vegetation within the simulation
*/
void SW_VES_construct(SW_VEGESTAB *SW_VegEstab) {
    /* =================================================== */
    /* note that an initializer that is called during
     * execution (better called clean() or something)
     * will need to free all allocated memory first
     * before clearing structure.
     */

    // Clear the module structure:
    memset(SW_VegEstab, 0, sizeof(SW_VEGESTAB));
}

/**
@brief Allocate dynamic memory for output pointers in the SW_VEGESTAB struct

@param[out] SW_VegEstab SW_VegEstab Struct of type SW_VEGESTAB holding all
    information about vegetation within the simulation
@param[out] LogInfo Holds information on warnings and errors
*/
void SW_VES_alloc_outptrs(SW_VEGESTAB *SW_VegEstab, LOG_INFO *LogInfo) {
    OutPeriod pd;

    // Allocate output structures:
    ForEachOutPeriod(pd) {
        SW_VegEstab->p_accu[pd] = (SW_VEGESTAB_OUTPUTS *) Mem_Calloc(
            1, sizeof(SW_VEGESTAB_OUTPUTS), "SW_VES_alloc_outptrs()", LogInfo
        );

        if (LogInfo->stopRun) {
            return; // Exit function prematurely due to error
        }

        // Intiailize p_accu to NULL to eliminate the chance of
        // deallocating unallocated memory
        SW_VegEstab->p_accu[pd]->days = NULL;

        if (pd > eSW_Day) {
            SW_VegEstab->p_oagg[pd] = (SW_VEGESTAB_OUTPUTS *) Mem_Calloc(
                1,
                sizeof(SW_VEGESTAB_OUTPUTS),
                "SW_VES_alloc_outptrs()",
                LogInfo
            );

            if (LogInfo->stopRun) {
                return; // Exit function prematurely due to error
            }

            // Intiailize p_accu to NULL to eliminate the chance of
            // deallocating unallocated memory
            SW_VegEstab->p_oagg[pd]->days = NULL;
        }
    }
}

/**
@brief Deconstructor for SW_VegEstab for each period, pd.

@param[in,out] SW_VegEstab Struct of type SW_VEGESTAB holding all
    information about vegetation within the simulation
*/
void SW_VES_deconstruct(SW_VEGESTAB *SW_VegEstab) {
    OutPeriod pd;
    IntU i;

    // De-allocate parameters
    if (SW_VegEstab->count > 0) {
        for (i = 0; i < SW_VegEstab->count; i++) {
            free(SW_VegEstab->parms[i]);
            SW_VegEstab->parms[i] = NULL;
        }

        free((void *) SW_VegEstab->parms);
        SW_VegEstab->parms = NULL;
    }


    ForEachOutPeriod(pd) {
        // De-allocate days and parameters
        if (SW_VegEstab->count > 0) {
            if (pd > eSW_Day && !isnull(SW_VegEstab->p_oagg[pd]->days)) {
                free(SW_VegEstab->p_oagg[eSW_Year]->days);
                SW_VegEstab->p_oagg[eSW_Year]->days = NULL;
            }

            if (!isnull(SW_VegEstab->p_accu[pd]->days)) {
                free(SW_VegEstab->p_accu[eSW_Year]->days);
                SW_VegEstab->p_accu[eSW_Year]->days = NULL;
            }
        }

        // De-allocate output structures
        if (pd > eSW_Day && !isnull(SW_VegEstab->p_oagg[pd])) {
            free(SW_VegEstab->p_oagg[pd]);
            SW_VegEstab->p_oagg[pd] = NULL;
        }

        if (!isnull(SW_VegEstab->p_accu[pd])) {
            free(SW_VegEstab->p_accu[pd]);
            SW_VegEstab->p_accu[pd] = NULL;
        }
    }
}

/**
@brief We can use the debug memset because we allocated days, that is, it
wasn't allocated by the compiler.

@param[in] count Held within type SW_VEGESTAB to determine
    how many species to check
*/
void SW_VES_new_year(IntU count) {

    if (0 == count) {
        return;
    }
}

/**
@brief Reads in file for SW_VegEstab and species establishment parameters

@param[in,out] SW_VegEstab Struct of type SW_VEGESTAB holding all information
    about vegetation establishment within the simulation
@param[in] InFiles Array of program in/output files
@param[in] SW_ProjDir Project directory
@param[out] LogInfo Holds information on warnings and errors
*/
void SW_VES_read(
    SW_VEGESTAB *SW_VegEstab,
    char *InFiles[],
    char *SW_ProjDir,
    LOG_INFO *LogInfo
) {

    SW_VES_read2(SW_VegEstab, swTRUE, swTRUE, InFiles, SW_ProjDir, LogInfo);
}

/**
@brief Reads in file for SW_VegEstab and species establishment parameters

@param[in,out] SW_VegEstab Struct of type SW_VEGESTAB holding all information
    about vegetation establishment within the simulation
@param[in] use_VegEstab Overall decision if user inputs for vegetation
    establishment should be processed.
@param[in] consider_InputFlag Should the user input flag read from `"estab.in"`
    be considered for turning on/off calculations of vegetation establishment.
@param[in] InFiles Array of program in/output files
@param[in] SW_ProjDir Project directory
@param[out] LogInfo Holds information on warnings and errors

@note
    - Establishment is calculated under the following conditions
    - there are input files with species establishment parameters
    - at least one of those files is correctly listed in `"estab.in"`
    - `use_VegEstab` is turned on (`swTRUE`) and
        - `consider_InputFlag` is off
        - OR `consider_InputFlag` is on and the input flag in `"estab.in"` is on
    - Establishment results are included in the output files only
      if `"ESTABL"` is turned on in `"outsetup.in"`
*/
void SW_VES_read2(
    SW_VEGESTAB *SW_VegEstab,
    Bool use_VegEstab,
    Bool consider_InputFlag,
    char *InFiles[],
    char *SW_ProjDir,
    LOG_INFO *LogInfo
) {

    SW_VES_deconstruct(SW_VegEstab);
    SW_VES_construct(SW_VegEstab);
    SW_VES_alloc_outptrs(SW_VegEstab, LogInfo);
    if (LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

    SW_VegEstab->use = use_VegEstab;

    char buf[FILENAME_MAX], inbuf[MAX_FILENAMESIZE];
    FILE *f;

    if (SW_VegEstab->use) {
        char *MyFileName = InFiles[eVegEstab];
        f = OpenFile(MyFileName, "r", LogInfo);
        if (LogInfo->stopRun) {
            return; // Exit function prematurely due to error
        }

        if (!GetALine(f, inbuf, MAX_FILENAMESIZE) ||
            (consider_InputFlag && *inbuf == '0')) {
            /* turn off vegetation establishment if either
                 * no species listed
                 * if user input flag is set to 0 and we don't ignore that
                 input, i.e.,`consider_InputFlag` is set to `swTRUE`
            */
            SW_VegEstab->use = swFALSE;

        } else {
            /* read file names with species establishment parameters
                     and read those files one by one
            */
            while (GetALine(f, inbuf, MAX_FILENAMESIZE)) {
                // add `SW_ProjDir` to path, e.g., for STEPWAT2
                strcpy(buf, SW_ProjDir);
                strcat(buf, inbuf);
                read_spp(buf, SW_VegEstab, LogInfo);
                if (LogInfo->stopRun) {
                    CloseFile(&f, LogInfo);
                    return; // Exit function prematurely due to error
                }
            }

            SW_VegEstab_alloc_outptrs(SW_VegEstab, LogInfo);
            if (LogInfo->stopRun) {
                CloseFile(&f, LogInfo);
                return; // Exit function prematurely due to error
            }
        }

        CloseFile(&f, LogInfo);
    }
}

/**
@brief Allocates element `day` for SW_VegEstab output variables

@param[in,out] SW_VegEstab SW_VegEstab SW_VegEstab Struct of type SW_VEGESTAB
    holding all information about vegetation within the simulation
@param[out] LogInfo Holds information on warnings and errors
*/
void SW_VegEstab_alloc_outptrs(SW_VEGESTAB *SW_VegEstab, LOG_INFO *LogInfo) {
    if (SW_VegEstab->count > 0) {

        if (isnull(SW_VegEstab->p_oagg[eSW_Year])) {
            LogError(
                LogInfo,
                LOGERROR,
                "SW_VegEstab_alloc_outptrs: 'p_oagg[eSW_Year]' is unexpectedly "
                "NULL."
            );
            return; // Exit function prematurely due to error
        }

        if (isnull(SW_VegEstab->p_accu[eSW_Year])) {
            LogError(
                LogInfo,
                LOGERROR,
                "SW_VegEstab_alloc_outptrs: 'p_accu[eSW_Year]' is unexpectedly "
                "NULL."
            );
            return; // Exit function prematurely due to error
        }

        SW_VegEstab->p_oagg[eSW_Year]->days = (TimeInt *) Mem_Calloc(
            SW_VegEstab->count,
            sizeof(TimeInt),
            "SW_VegEstab_alloc_outptrs()",
            LogInfo
        );
        if (LogInfo->stopRun) {
            return; // Exit function prematurely due to error
        }

        SW_VegEstab->p_accu[eSW_Year]->days = (TimeInt *) Mem_Calloc(
            SW_VegEstab->count,
            sizeof(TimeInt),
            "SW_VegEstab_alloc_outptrs()",
            LogInfo
        );
    }
}

/**
@brief Initialization and checks of species establishment parameters

This works correctly only after
    * species establishment parameters are read from file by `SW_VES_read()`
    * soil layers are initialized by `SW_SIT_init_run()`

@param[in,out] **parms List of structs of type SW_VEGESTAB_INFO holding
    information about every vegetation species
@param[in] SW_Site Struct of type SW_SITE describing the simulated site
@param[in] n_transp_lyrs Index of the deepest transp. region
@param[in] count Held within type SW_VEGESTAB to determine
    how many species to check
@param[out] LogInfo Holds information on warnings and errors
*/
void SW_VES_init_run(
    SW_VEGESTAB_INFO **parms,
    SW_SITE *SW_Site,
    LyrIndex n_transp_lyrs[],
    IntU count,
    LOG_INFO *LogInfo
) {

    IntU i;

    for (i = 0; i < count; i++) {
        _spp_init(parms, i, SW_Site, n_transp_lyrs, LogInfo);
        if (LogInfo->stopRun) {
            return; // Exit function prematurely due to error
        }
    }
}

/**
@brief Check that each count coincides with a day of the year.

@param[in,out] **parms List of structs of type SW_VEGESTAB_INFO holding
    information about every vegetation species
@param[in] SW_Weather Struct of type SW_WEATHER holding all relevant
    information pretaining to meteorological input data
@param[in] swcBulk Soil water content in the layer [cm]
@param[in] doy Day of the year (base1) [1-366]
@param[in] firstdoy First day of current year
@param[in] count Held within type SW_VEGESTAB to determine
    how many species to check
*/
void SW_VES_checkestab(
    SW_VEGESTAB_INFO **parms,
    SW_WEATHER *SW_Weather,
    RealD swcBulk[][MAX_LAYERS],
    TimeInt doy,
    TimeInt firstdoy,
    IntU count
) {
    /* =================================================== */
    IntU i;

    for (i = 0; i < count; i++) {
        checkit(doy, i, &SW_Weather->now, swcBulk, firstdoy, parms);
    }
}

/* =================================================== */
/*            Local Function Definitions               */
/* --------------------------------------------------- */

static void checkit(
    TimeInt doy,
    unsigned int sppnum,
    SW_WEATHER_NOW *wn,
    RealD swcBulk[][MAX_LAYERS],
    TimeInt firstdoy,
    SW_VEGESTAB_INFO **parms
) {

    SW_VEGESTAB_INFO *v = parms[sppnum];

    IntU i;
    RealF avgtemp = wn->temp_avg; /* avg of today's min/max temp */
    RealF avgswc;                 /* avg_swc today */

    if (doy == firstdoy) {
        zero_state(sppnum, parms);
    }

    if (v->no_estab || v->estab_doy > 0) {
        goto LBL_Normal_Exit;
    }

    /* keep up with germinating wetness regardless of current state */
    if (GT(swcBulk[Today][0], v->min_swc_germ)) {
        v->wetdays_for_germ++;
    } else {
        v->wetdays_for_germ = 0;
    }

    if (doy < v->min_pregerm_days) {
        goto LBL_Normal_Exit;
    }

    /* ---- check for germination, establishment */
    if (!v->germd && v->wetdays_for_germ >= v->min_wetdays_for_germ) {

        if (doy < v->min_pregerm_days) {
            goto LBL_Normal_Exit;
        }
        if (doy > v->max_pregerm_days) {
            v->no_estab = swTRUE;
            goto LBL_Normal_Exit;
        }
        /* temp doesn't affect wetdays */
        if (LT(avgtemp, v->min_temp_germ) || GT(avgtemp, v->max_temp_germ)) {
            goto LBL_Normal_Exit;
        }

        v->germd = swTRUE;
        goto LBL_Normal_Exit;

    } else {
        /* continue monitoring sprout's progress */

        /* any dry period (> max_drydays) or temp out of range
         * after germination means restart */
        for (i = avgswc = 0; i < v->estab_lyrs;) {
            avgswc += swcBulk[Today][i++];
        }
        avgswc /= (RealF) v->estab_lyrs;
        if (LT(avgswc, v->min_swc_estab)) {
            v->drydays_postgerm++;
            v->wetdays_for_estab = 0;
        } else {
            v->drydays_postgerm = 0;
            v->wetdays_for_estab++;
        }

        if (v->drydays_postgerm > v->max_drydays_postgerm ||
            LT(avgtemp, v->min_temp_estab) || GT(avgtemp, v->max_temp_estab)) {
            /* too bad: discontinuity in environment, plant dies, start over */
            goto LBL_EstabFailed_Exit;
        }

        v->germ_days++;

        if (v->wetdays_for_estab < v->min_wetdays_for_estab ||
            v->germ_days < v->min_days_germ2estab) {
            goto LBL_Normal_Exit;
            /* no need to zero anything */
        }

        if (v->germ_days > v->max_days_germ2estab) {
            goto LBL_EstabFailed_Exit;
        }

        v->estab_doy = doy;
        goto LBL_Normal_Exit;
    }

LBL_EstabFailed_Exit:
    /* allows us to try again if not too late */
    v->wetdays_for_estab = 0;
    v->germ_days = 0;
    v->germd = swFALSE;

LBL_Normal_Exit:
    return;
}

static void zero_state(unsigned int sppnum, SW_VEGESTAB_INFO **parms) {
    /* =================================================== */
    /* zero any values that need it for the new growing season */

    SW_VEGESTAB_INFO *parms_sppnum = parms[sppnum];

    parms_sppnum->no_estab = parms_sppnum->germd = swFALSE;
    parms_sppnum->estab_doy = parms_sppnum->germ_days =
        parms_sppnum->drydays_postgerm = 0;
    parms_sppnum->wetdays_for_germ = parms_sppnum->wetdays_for_estab = 0;
}

static void read_spp(
    const char *infile, SW_VEGESTAB *SW_VegEstab, LOG_INFO *LogInfo
) {
    /* =================================================== */

    SW_VEGESTAB_INFO *v;
    const int nitems = 16;
    FILE *f;
    int lineno = 0;
    char name[80]; /* only allow 4 char sppnames */
    char inbuf[MAX_FILENAMESIZE], *endPtr;
    int inBufintRes;
    double inBufFloatVal;

    Bool doIntConv;

    IntU count;

    count = _new_species(SW_VegEstab, LogInfo);
    if (LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }
    v = SW_VegEstab->parms[count];

    // have to copy before the pointer infile gets reset below by getAline
    strcpy(v->sppFileName, infile);

    f = OpenFile(infile, "r", LogInfo);
    if (LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

    while (GetALine(f, inbuf, MAX_FILENAMESIZE)) {

        if (lineno >= 1 && lineno <= 15) {
            /* Check to see if the line number contains an integer or float
             * value */
            doIntConv = (Bool) ((lineno >= 1 && lineno <= 2) ||
                                (lineno >= 5 && lineno <= 11));

            if (doIntConv) {
                inBufintRes = strtol(inbuf, &endPtr, 10);
            } else {
                inBufFloatVal = strtof(inbuf, &endPtr);
            }

            check_errno(infile, inbuf, endPtr, LogInfo);
            if (LogInfo->stopRun) {
                return; // Exit function prematurely due to error
            }
        }

        switch (lineno) {
        case 0:
            strcpy(name, inbuf);
            break;
        case 1:
            v->vegType = inBufintRes;
            break;
        case 2:
            v->estab_lyrs = inBufintRes;
            break;
        case 3:
            v->bars[SW_GERM_BARS] = fabs(inBufFloatVal);
            break;
        case 4:
            v->bars[SW_ESTAB_BARS] = fabs(inBufFloatVal);
            break;
        case 5:
            v->min_pregerm_days = inBufintRes;
            break;
        case 6:
            v->max_pregerm_days = inBufintRes;
            break;
        case 7:
            v->min_wetdays_for_germ = inBufintRes;
            break;
        case 8:
            v->max_drydays_postgerm = inBufintRes;
            break;
        case 9:
            v->min_wetdays_for_estab = inBufintRes;
            break;
        case 10:
            v->min_days_germ2estab = inBufintRes;
            break;
        case 11:
            v->max_days_germ2estab = inBufintRes;
            break;
        case 12:
            v->min_temp_germ = inBufFloatVal;
            break;
        case 13:
            v->max_temp_germ = inBufFloatVal;
            break;
        case 14:
            v->min_temp_estab = inBufFloatVal;
            break;
        case 15:
            v->max_temp_estab = inBufFloatVal;
            break;
        default:
            LogError(
                LogInfo,
                LOGERROR,
                "read_spp(): incorrect format of input file '%s'.",
                infile
            );
            return; // Exit function prematurely due to error
            break;
        }

        /* check for valid name first */
        if (0 == lineno) {
            if (strlen(name) > MAX_SPECIESNAMELEN) {
                CloseFile(&f, LogInfo);
                LogError(
                    LogInfo,
                    LOGERROR,
                    "%s: Species name <%s> too long (> %d chars).\n Try "
                    "again.\n",
                    infile,
                    name,
                    MAX_SPECIESNAMELEN
                );
                return; // Exit function prematurely due to error
            }

            strcpy(v->sppname, name);
        }

        lineno++; /*only increments when there's a value */
    }

    CloseFile(&f, LogInfo);

    if (lineno != nitems) {
        LogError(
            LogInfo, LOGERROR, "%s : Too few/many input parameters.\n", infile
        );
        return; // Exit function prematurely due to error
    }
}

/**
@brief Initializations performed after acquiring parameters after read() or some
other function call.

@param[in,out] **parms List of structs of type SW_VEGESTAB_INFO holding
    information about every vegetation species
@param[in] sppnum Index for which paramater is beign initialized.
@param[in] SW_Site Struct of type SW_SITE describing the simulated site
@param[in] n_transp_lyrs Layer index of deepest transp. region.
@param[out] LogInfo Holds information on warnings and errors
*/
void _spp_init(
    SW_VEGESTAB_INFO **parms,
    unsigned int sppnum,
    SW_SITE *SW_Site,
    LyrIndex n_transp_lyrs[],
    LOG_INFO *LogInfo
) {

    SW_VEGESTAB_INFO *parms_sppnum = parms[sppnum];
    IntU i;

    /* The thetas and psis etc should be initialized by now */
    /* because init_layers() must be called prior to this routine */
    /* (see watereqn() ) */
    parms_sppnum->min_swc_germ =
        SW_SWRC_SWPtoSWC(parms_sppnum->bars[SW_GERM_BARS], SW_Site, 0, LogInfo);
    if (LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

    /* due to possible differences in layer textures and widths, we need
     * to average the estab swc across the given layers to peoperly
     * compare the actual swc average in the checkit() routine */
    parms_sppnum->min_swc_estab = 0.;
    for (i = 0; i < parms_sppnum->estab_lyrs; i++) {
        parms_sppnum->min_swc_estab += SW_SWRC_SWPtoSWC(
            parms_sppnum->bars[SW_ESTAB_BARS], SW_Site, i, LogInfo
        );
        if (LogInfo->stopRun) {
            return; // Exit function prematurely due to error
        }
    }
    parms_sppnum->min_swc_estab /= parms_sppnum->estab_lyrs;

    sanity_check(
        sppnum, SW_Site->swcBulk_wiltpt, n_transp_lyrs, parms, LogInfo
    );
}

static void sanity_check(
    unsigned int sppnum,
    RealD swcBulk_wiltpt[],
    LyrIndex n_transp_lyrs[],
    SW_VEGESTAB_INFO **parms,
    LOG_INFO *LogInfo
) {
    /* =================================================== */
    SW_VEGESTAB_INFO *parms_sppnum = parms[sppnum];

    double mean_wiltpt;
    unsigned int i;

    if (parms_sppnum->vegType >= NVEGTYPES) {
        LogError(
            LogInfo,
            LOGERROR,
            "%s (%s) : Specified vegetation type (%d) is not implemented.",
            "VegEstab",
            parms_sppnum->sppname,
            parms_sppnum->vegType
        );
        return; // Exit function prematurely due to error
    }

    if (parms_sppnum->estab_lyrs > n_transp_lyrs[parms_sppnum->vegType]) {
        LogError(
            LogInfo,
            LOGERROR,
            "%s (%s) : Layers requested (estab_lyrs = %d) > "
            "(# transpiration layers = %d).",
            "VegEstab",
            parms_sppnum->sppname,
            parms_sppnum->estab_lyrs,
            n_transp_lyrs[parms_sppnum->vegType]
        );
        return; // Exit function prematurely due to error
    }

    if (parms_sppnum->min_pregerm_days > parms_sppnum->max_pregerm_days) {
        LogError(
            LogInfo,
            LOGERROR,
            "%s (%s) : First day of germination > last day of germination.",
            "VegEstab",
            parms_sppnum->sppname
        );
        return; // Exit function prematurely due to error
    }

    if (parms_sppnum->min_wetdays_for_estab >
        parms_sppnum->max_days_germ2estab) {
        LogError(
            LogInfo,
            LOGERROR,
            "%s (%s) : Minimum wetdays after germination (%d) > "
            "maximum days allowed for establishment (%d).",
            "VegEstab",
            parms_sppnum->sppname,
            parms_sppnum->min_wetdays_for_estab,
            parms_sppnum->max_days_germ2estab
        );
        return; // Exit function prematurely due to error
    }

    if (parms_sppnum->min_swc_germ < swcBulk_wiltpt[0]) {
        LogError(
            LogInfo,
            LOGERROR,
            "%s (%s) : Minimum swc for germination (%.4f) < wiltpoint (%.4f)",
            "VegEstab",
            parms_sppnum->sppname,
            parms_sppnum->min_swc_germ,
            swcBulk_wiltpt[0]
        );
        return; // Exit function prematurely due to error
    }

    mean_wiltpt = 0.;
    for (i = 0; i < parms_sppnum->estab_lyrs; i++) {
        mean_wiltpt += swcBulk_wiltpt[i];
    }
    mean_wiltpt /= parms_sppnum->estab_lyrs;

    if (LT(parms_sppnum->min_swc_estab, mean_wiltpt)) {
        LogError(
            LogInfo,
            LOGERROR,
            "%s (%s) : Minimum swc for establishment (%.4f) < wiltpoint (%.4f)",
            "VegEstab",
            parms_sppnum->sppname,
            parms_sppnum->min_swc_estab,
            mean_wiltpt
        );
    }
}

/**
@brief New species for establishment

First time called with no species defined so SW_VegEstab.count == 0 and
SW_VegEstab.parms is not initialized yet, malloc()
required.  For each species thereafter realloc() is called.

@param[in,out] SW_VegEstab SW_VegEstab Struct of type SW_VEGESTAB holding all
    information about vegetation within the simulation
@param[out] LogInfo Holds information on warnings and errors

@return (++SW_VegEstab->count) - 1
*/
IntU _new_species(SW_VEGESTAB *SW_VegEstab, LOG_INFO *LogInfo) {

    const char *me = "SW_VegEstab_newspecies()";

    SW_VegEstab->parms =
        (!SW_VegEstab->count) ?
            (SW_VEGESTAB_INFO **) Mem_Calloc(
                SW_VegEstab->count + 1, sizeof(SW_VEGESTAB_INFO *), me, LogInfo
            ) :
            (SW_VEGESTAB_INFO **) Mem_ReAlloc(
                (void *) SW_VegEstab->parms,
                sizeof(SW_VEGESTAB_INFO *) * (SW_VegEstab->count + 1),
                LogInfo
            );
    if (LogInfo->stopRun) {
        return SW_VegEstab->count; // Return function prematurely due to error
    }

    SW_VegEstab->parms[SW_VegEstab->count] = (SW_VEGESTAB_INFO *) Mem_Calloc(
        1, sizeof(SW_VEGESTAB_INFO), me, LogInfo
    );

    return (++SW_VegEstab->count) - 1;
}

/**
@brief Text output for VegEstab.

@param[in] width Width of the soil layer (cm)
@param[in] **parms List of structs of type SW_VEGESTAB_INFO holding
    information about every vegetation species
@param[in] count Held within type SW_VEGESTAB to determine
    how many species to check
*/
void echo_VegEstab(const RealD width[], SW_VEGESTAB_INFO **parms, IntU count) {
    /* --------------------------------------------------- */
    IntU i;
    char outstr[2048], errstr[MAX_ERROR];

    snprintf(
        errstr,
        MAX_ERROR,
        "\n=========================================================\n\n"
        "Parameters for the SoilWat Vegetation Establishment Check.\n"
        "----------------------------------------------------------\n"
        "Number of species to be tested: %d\n",
        count
    );

    strcpy(outstr, errstr);
    for (i = 0; i < count; i++) {
        snprintf(
            errstr,
            MAX_ERROR,
            "Species: %s (vegetation type %s [%d])\n----------------\n"
            "Germination parameters:\n"
            "\tMinimum SWP (bars)  : -%.4f\n"
            "\tMinimum SWC (cm/cm) : %.4f\n"
            "\tMinimum SWC (cm/lyr): %.4f\n"
            "\tMinimum temperature : %.1f\n"
            "\tMaximum temperature : %.1f\n"
            "\tFirst possible day  : %d\n"
            "\tLast  possible day  : %d\n"
            "\tMinimum consecutive wet days (after first possible day): %d\n",
            parms[i]->sppname,
            key2veg[parms[i]->vegType],
            parms[i]->vegType,
            parms[i]->bars[SW_GERM_BARS],
            parms[i]->min_swc_germ / width[0],
            parms[i]->min_swc_germ,
            parms[i]->min_temp_germ,
            parms[i]->max_temp_germ,
            parms[i]->min_pregerm_days,
            parms[i]->max_pregerm_days,
            parms[i]->min_wetdays_for_germ
        );

        snprintf(
            errstr,
            MAX_ERROR,
            "Establishment parameters:\n"
            "\tNumber of layers affecting successful establishment: %d\n"
            "\tMinimum SWP (bars) : -%.4f\n"
            "\tMinimum SWC (cm/layer) averaged across top %d layers: %.4f\n"
            "\tMinimum temperature : %.1f\n"
            "\tMaximum temperature : %.1f\n"
            "\tMinimum number of days after germination      : %d\n"
            "\tMaximum number of days after germination      : %d\n"
            "\tMinimum consecutive wet days after germination: %d\n"
            "\tMaximum consecutive dry days after germination: %d\n"
            "---------------------------------------------------------------"
            "\n\n",
            parms[i]->estab_lyrs,
            parms[i]->bars[SW_ESTAB_BARS],
            parms[i]->estab_lyrs,
            parms[i]->min_swc_estab,
            parms[i]->min_temp_estab,
            parms[i]->max_temp_estab,
            parms[i]->min_days_germ2estab,
            parms[i]->max_days_germ2estab,
            parms[i]->min_wetdays_for_estab,
            parms[i]->max_drydays_postgerm
        );

        strcat(outstr, errstr);
    }
    strcat(
        outstr,
        "\n-----------------  End of Establishment Parameters ------------\n"
    );

    printf("%s\n", outstr);
}
