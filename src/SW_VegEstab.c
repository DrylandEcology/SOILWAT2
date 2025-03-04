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

 08/21/2013	(clk)	changed the line v = SW_VegEstab.parms[ new_species()
 ]; -> v = SW_VegEstab.parms[ count ], where count = new_species(); for some
 reason, without this change, a segmenation fault was occuring
 */
/********************************************************/
/********************************************************/


/* =================================================== */
/*                INCLUDES / DEFINES                   */
/* --------------------------------------------------- */

#include "include/SW_VegEstab.h"    // for SW_ESTAB_BARS, SW_GERM_BARS, SW_...
#include "include/filefuncs.h"      // for LogError, CloseFile, GetALine
#include "include/generic.h"        // for IntU, LOGERROR, isnull, LT
#include "include/myMemory.h"       // for Mem_Calloc, Mem_ReAlloc
#include "include/SW_datastructs.h" // for SW_VEGESTAB_INFO, LOG_INFO, SW_V...
#include "include/SW_Defines.h"     // for TimeInt, eSW_Year, MAX_FILENAMESIZE
#include "include/SW_Files.h"       // for eVegEstab
#include "include/SW_SoilWater.h"   // for SW_SWRC_SWPtoSWC
#include "include/SW_Times.h"       // for Today
#include "include/SW_VegProd.h"     // for key2veg
#include <math.h>                   // for fabs
#include <stdio.h>                  // for NULL, snprintf, FILE, printf
#include <stdlib.h>                 // for free
#include <string.h>                 // for memccpy, strlen, memset


/* =================================================== */
/*             Private Function Declarations           */
/* --------------------------------------------------- */
static void sanity_check(
    unsigned int sppnum,
    double swcBulk_wiltpt[],
    LyrIndex n_transp_lyrs[],
    SW_VEGESTAB_INFO_INPUTS *parmsIn,
    LOG_INFO *LogInfo
);

static void read_spp(
    const char *infile,
    SW_VEGESTAB_INPUTS *SW_VegEstabIn,
    SW_VEGESTAB_SIM *SW_VegEstabSim,
    LOG_INFO *LogInfo
);

static void checkit(
    SW_VEGESTAB_INFO_INPUTS *parmsIn,
    SW_VEGESTAB_INFO_SIM *parmsSim,
    TimeInt doy,
    unsigned int sppnum,
    double avgtemp,
    double swcBulk[][MAX_LAYERS],
    TimeInt firstdoy
);

static void zero_state(unsigned int sppnum, SW_VEGESTAB_INFO_SIM *parmsSim);

/* =================================================== */
/*             Global Function Definitions             */
/* --------------------------------------------------- */

/**
@brief Initialize all possible pointers in SW_VEGESTAB to NULL

@param[in,out] SW_VegEstabSim Struct of type SW_VEGESTAB_SIM holding all
    simulation information about vegetation within the simulation
@param[in,out] ves_p_accu A list of output structs of type SW_VEGESTAB_OUTPUTS
    to accumulate output
@param[in,out] ves_p_oagg A list of output structs of type SW_VEGESTAB_OUTPUTS
    to aggregate output
*/
void SW_VES_init_ptrs(
    SW_VEGESTAB_SIM *SW_VegEstabSim,
    SW_VEGESTAB_OUTPUTS *ves_p_accu,
    SW_VEGESTAB_OUTPUTS *ves_p_oagg
) {
    OutPeriod pd;

    SW_VegEstabSim->count = 0;

    // Allocate output structures:
    ForEachOutPeriod(pd) {
        // Intiailize p_accu and p_oagg to NULL to eliminate the chance of
        // deallocating unallocated memory
        ves_p_accu[pd].days = ves_p_oagg[pd].days = NULL;
    }
}

/**
@brief Constructor for SW_VegEstab.

@param[out] SW_VegEstabIn Struct of type SW_VEGESTAB_INPUTS holding all
    input information about vegetation within the simulation
@param[out] SW_VegEstabSim Struct of type SW_VEGESTAB_SIM holding all
    simulation information about vegetation within the simulation
@param[out] ves_p_oagg A list of output structs of type SW_VEGESTAB_OUTPUTS
    to accumulate output
@param[out] ves_p_accu A list of output structs of type SW_VEGESTAB_OUTPUTS
    to aggregate output
*/
void SW_VES_construct(
    SW_VEGESTAB_INPUTS *SW_VegEstabIn,
    SW_VEGESTAB_SIM *SW_VegEstabSim,
    SW_VEGESTAB_OUTPUTS ves_p_oagg[],
    SW_VEGESTAB_OUTPUTS ves_p_accu[]
) {
    /* =================================================== */
    /* note that an initializer that is called during
     * execution (better called clean() or something)
     * will need to free all allocated memory first
     * before clearing structure.
     */
    OutPeriod pd;

    // Clear the module structure:
    memset(SW_VegEstabIn, 0, sizeof(SW_VEGESTAB_INPUTS));
    memset(SW_VegEstabSim, 0, sizeof(SW_VEGESTAB_SIM));

    ForEachOutPeriod(pd) {
        memset(ves_p_oagg, 0, sizeof(SW_VEGESTAB_OUTPUTS));
        memset(ves_p_accu, 0, sizeof(SW_VEGESTAB_OUTPUTS));
    }
}

/**
@brief Deconstructor for SW_VegEstab for each period, pd.

@param[in,out] SW_VegEstabSim Struct of type SW_VEGESTAB_SIM holding all
    simulation information about vegetation within the simulation
@param[in,out] ves_p_accu A list of output structs of type SW_VEGESTAB_OUTPUTS
    to accumulate output
@param[in,out] ves_p_oagg A list of output structs of type SW_VEGESTAB_OUTPUTS
    to aggregate output
*/
void SW_VES_deconstruct(
    SW_VEGESTAB_SIM *SW_VegEstabSim,
    SW_VEGESTAB_OUTPUTS *ves_p_accu,
    SW_VEGESTAB_OUTPUTS *ves_p_oagg
) {
    OutPeriod pd;

    ForEachOutPeriod(pd) {
        // De-allocate days
        if (SW_VegEstabSim->count > 0) {
            if (pd > eSW_Day && !isnull(ves_p_oagg[pd].days)) {
                free(ves_p_oagg[eSW_Year].days);
                ves_p_oagg[eSW_Year].days = NULL;
            }

            if (!isnull(ves_p_accu[pd].days)) {
                free(ves_p_accu[eSW_Year].days);
                ves_p_accu[eSW_Year].days = NULL;
            }
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

@param[in,out] SW_VegEstabIn Struct of type SW_VEGESTAB_INPUTS holding all
    input information about vegetation within the simulation
@param[in,out] SW_VegEstabSim Struct of type SW_VEGESTAB_SIM holding all
    simulation information about vegetation within the simulation
@param[in,out] ves_p_accu A list of output structs of type SW_VEGESTAB_OUTPUTS
    to accumulate output
@param[in,out] ves_p_oagg A list of output structs of type SW_VEGESTAB_OUTPUTS
    to aggregate output
@param[in] txtInFiles Array of program in/output files
@param[in] SW_ProjDir Project directory
@param[out] LogInfo Holds information on warnings and errors
*/
void SW_VES_read(
    SW_VEGESTAB_INPUTS *SW_VegEstabIn,
    SW_VEGESTAB_SIM *SW_VegEstabSim,
    SW_VEGESTAB_OUTPUTS *ves_p_accu,
    SW_VEGESTAB_OUTPUTS *ves_p_oagg,
    char *txtInFiles[],
    char *SW_ProjDir,
    LOG_INFO *LogInfo
) {

    SW_VES_read2(
        SW_VegEstabIn,
        SW_VegEstabSim,
        ves_p_accu,
        ves_p_oagg,
        swTRUE,
        swTRUE,
        txtInFiles,
        SW_ProjDir,
        LogInfo
    );
}

/**
@brief Reads in file for SW_VegEstab and species establishment parameters

@param[in,out] SW_VegEstabIn Struct of type SW_VEGESTAB_INPUTS holding all
    input information about vegetation within the simulation
@param[in,out] SW_VegEstabSim Struct of type SW_VEGESTAB_SIM holding all
    simulation information about vegetation within the simulation
@param[in,out] ves_p_accu A list of output structs of type SW_VEGESTAB_OUTPUTS
    to accumulate output
@param[in,out] ves_p_oagg A list of output structs of type SW_VEGESTAB_OUTPUTS
    to aggregate output
@param[in] use_VegEstab Overall decision if user inputs for vegetation
    establishment should be processed.
@param[in] consider_InputFlag Should the user input flag read from `"estab.in"`
    be considered for turning on/off calculations of vegetation establishment.
@param[in] txtInFiles Array of program in/output files
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
    SW_VEGESTAB_INPUTS *SW_VegEstabIn,
    SW_VEGESTAB_SIM *SW_VegEstabSim,
    SW_VEGESTAB_OUTPUTS *ves_p_accu,
    SW_VEGESTAB_OUTPUTS *ves_p_oagg,
    Bool use_VegEstab,
    Bool consider_InputFlag,
    char *txtInFiles[],
    char *SW_ProjDir,
    LOG_INFO *LogInfo
) {

    SW_VES_deconstruct(SW_VegEstabSim, ves_p_accu, ves_p_oagg);
    SW_VES_construct(SW_VegEstabIn, SW_VegEstabSim, ves_p_oagg, ves_p_accu);

    SW_VegEstabSim->use = use_VegEstab;

    int resSNP;
    char buf[FILENAME_MAX];
    char inbuf[MAX_FILENAMESIZE];
    FILE *f;

    if (SW_VegEstabSim->use) {
        char *MyFileName = txtInFiles[eVegEstab];
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
            SW_VegEstabSim->use = swFALSE;

        } else {
            /* read file names with species establishment parameters
                     and read those files one by one
            */
            while (GetALine(f, inbuf, MAX_FILENAMESIZE)) {
                // add `SW_ProjDir` to path, e.g., for STEPWAT2
                resSNP = snprintf(buf, sizeof buf, "%s%s", SW_ProjDir, inbuf);
                if (resSNP < 0 || (unsigned) resSNP >= (sizeof buf)) {
                    LogError(
                        LogInfo,
                        LOGERROR,
                        "Establishment parameter file name is too long: '%s'.",
                        inbuf
                    );
                    goto closeFile;
                }

                read_spp(buf, SW_VegEstabIn, SW_VegEstabSim, LogInfo);
                if (LogInfo->stopRun) {
                    goto closeFile;
                }
            }

            SW_VegEstab_alloc_outptrs(
                ves_p_accu, ves_p_oagg, SW_VegEstabSim->count, LogInfo
            );
            if (LogInfo->stopRun) {
                goto closeFile;
            }
        }

    closeFile: { CloseFile(&f, LogInfo); }
    }
}

/**
@brief Allocates element `day` for SW_VegEstab output variables

@param[in,out] ves_p_accu A list of output structs of type SW_VEGESTAB_OUTPUTS
    to accumulate output
@param[in,out] ves_p_oagg A list of output structs of type SW_VEGESTAB_OUTPUTS
    to aggregate output
@param[in,out] count Number of specifies
@param[out] LogInfo Holds information on warnings and errors
*/
void SW_VegEstab_alloc_outptrs(
    SW_VEGESTAB_OUTPUTS *ves_p_accu,
    SW_VEGESTAB_OUTPUTS *ves_p_oagg,
    IntU count,
    LOG_INFO *LogInfo
) {
    if (count > 0) {
        ves_p_oagg[eSW_Year].days = (TimeInt *) Mem_Calloc(
            count, sizeof(TimeInt), "SW_VegEstab_alloc_outptrs()", LogInfo
        );
        if (LogInfo->stopRun) {
            return; // Exit function prematurely due to error
        }

        ves_p_accu[eSW_Year].days = (TimeInt *) Mem_Calloc(
            count, sizeof(TimeInt), "SW_VegEstab_alloc_outptrs()", LogInfo
        );
    }
}

/**
@brief Initialization and checks of species establishment parameters

This works correctly only after
    * species establishment parameters are read from file by `SW_VES_read()`
    * soil layers are initialized by `SW_SIT_init_run()`

@param[in,out] *parmsIn List of structs of type SW_VEGESTAB_INFO_INPUTS holding
    input information about every vegetation species
@param[in] SW_SoilRunIn Struct of type SW_SOIL_RUN_INPUTS describing
    the simulated site's input values
@param[in] SW_SiteSim Struct of type SW_SITE_SIM describing the simulated site's
    simulation values
@param[in] n_transp_lyrs Index of the deepest transp. region
@param[in] count Held within type SW_VEGESTAB to determine
    how many species to check
@param[out] LogInfo Holds information on warnings and errors
*/
void SW_VES_init_run(
    SW_VEGESTAB_INFO_INPUTS *parmsIn,
    SW_SOIL_RUN_INPUTS *SW_SoilRunIn,
    SW_SITE_SIM *SW_SiteSim,
    LyrIndex n_transp_lyrs[],
    IntU count,
    LOG_INFO *LogInfo
) {

    IntU i;

    for (i = 0; i < count; i++) {
        spp_init(parmsIn, i, SW_SoilRunIn, SW_SiteSim, n_transp_lyrs, LogInfo);
        if (LogInfo->stopRun) {
            return; // Exit function prematurely due to error
        }
    }
}

/**
@brief Check that each count coincides with a day of the year.

@param[in,out] *parmsIn List of structs of type SW_VEGESTAB_INFO_INPUTS holding
    input information about every vegetation species
@param[in,out] *parmsSim List of structs of type SW_VEGESTAB_INFO_SIM holding
    simulation information about every vegetation species
@param[in] avgTemp Average of todays max/min temperatures
@param[in] swcBulk Soil water content in the layer [cm]
@param[in] doy Day of the year (base1) [1-366]
@param[in] firstdoy First day of current year
@param[in] count Held within type SW_VEGESTAB to determine
    how many species to check
*/
void SW_VES_checkestab(
    SW_VEGESTAB_INFO_INPUTS *parmsIn,
    SW_VEGESTAB_INFO_SIM *parmsSim,
    double avgTemp,
    double swcBulk[][MAX_LAYERS],
    TimeInt doy,
    TimeInt firstdoy,
    IntU count
) {
    /* =================================================== */
    IntU i;

    for (i = 0; i < count; i++) {
        checkit(parmsIn, parmsSim, doy, i, avgTemp, swcBulk, firstdoy);
    }
}

/* =================================================== */
/*            Local Function Definitions               */
/* --------------------------------------------------- */

static void checkit(
    SW_VEGESTAB_INFO_INPUTS *parmsIn,
    SW_VEGESTAB_INFO_SIM *parmsSim,
    TimeInt doy,
    unsigned int sppnum,
    double avgtemp,
    double swcBulk[][MAX_LAYERS],
    TimeInt firstdoy
) {

    SW_VEGESTAB_INFO_INPUTS *v = &parmsIn[sppnum];
    SW_VEGESTAB_INFO_SIM *s = &parmsSim[sppnum];

    IntU i;
    double avgswc; /* avg_swc today */

    if (doy == firstdoy) {
        zero_state(sppnum, parmsSim);
    }

    if (s->no_estab || s->estab_doy > 0) {
        goto LBL_Normal_Exit;
    }

    /* keep up with germinating wetness regardless of current state */
    if (GT(swcBulk[Today][0], v->min_swc_germ)) {
        s->wetdays_for_germ++;
    } else {
        s->wetdays_for_germ = 0;
    }

    if (doy < v->min_pregerm_days) {
        goto LBL_Normal_Exit;
    }

    /* ---- check for germination, establishment */
    if (!s->germd && s->wetdays_for_germ >= v->min_wetdays_for_germ) {

        if (doy < v->min_pregerm_days) {
            goto LBL_Normal_Exit;
        }
        if (doy > v->max_pregerm_days) {
            s->no_estab = swTRUE;
            goto LBL_Normal_Exit;
        }
        /* temp doesn't affect wetdays */
        if (LT(avgtemp, v->min_temp_germ) || GT(avgtemp, v->max_temp_germ)) {
            goto LBL_Normal_Exit;
        }

        s->germd = swTRUE;
        goto LBL_Normal_Exit;

    } else {
        /* continue monitoring sprout's progress */

        /* any dry period (> max_drydays) or temp out of range
         * after germination means restart */
        avgswc = 0.;
        for (i = 0; i < v->estab_lyrs;) {
            avgswc += swcBulk[Today][i++];
        }
        avgswc /= (double) v->estab_lyrs;
        if (LT(avgswc, v->min_swc_estab)) {
            s->drydays_postgerm++;
            s->wetdays_for_estab = 0;
        } else {
            s->drydays_postgerm = 0;
            s->wetdays_for_estab++;
        }

        if (s->drydays_postgerm > v->max_drydays_postgerm ||
            LT(avgtemp, v->min_temp_estab) || GT(avgtemp, v->max_temp_estab)) {
            /* too bad: discontinuity in environment, plant dies, start over */
            goto LBL_EstabFailed_Exit;
        }

        s->germ_days++;

        if (s->wetdays_for_estab < v->min_wetdays_for_estab ||
            s->germ_days < v->min_days_germ2estab) {
            goto LBL_Normal_Exit;
            /* no need to zero anything */
        }

        if (s->germ_days > v->max_days_germ2estab) {
            goto LBL_EstabFailed_Exit;
        }

        s->estab_doy = doy;
        goto LBL_Normal_Exit;
    }

LBL_EstabFailed_Exit:
    /* allows us to try again if not too late */
    s->wetdays_for_estab = 0;
    s->germ_days = 0;
    s->germd = swFALSE;

LBL_Normal_Exit:
    return;
}

static void zero_state(unsigned int sppnum, SW_VEGESTAB_INFO_SIM *parmsSim) {
    /* =================================================== */
    /* zero any values that need it for the new growing season */

    SW_VEGESTAB_INFO_SIM *parms_sppnum = &parmsSim[sppnum];

    parms_sppnum->no_estab = parms_sppnum->germd = swFALSE;
    parms_sppnum->estab_doy = parms_sppnum->germ_days =
        parms_sppnum->drydays_postgerm = 0;
    parms_sppnum->wetdays_for_germ = parms_sppnum->wetdays_for_estab = 0;
}

static void read_spp(
    const char *infile,
    SW_VEGESTAB_INPUTS *SW_VegEstabIn,
    SW_VEGESTAB_SIM *SW_VegEstabSim,
    LOG_INFO *LogInfo
) {
    /* =================================================== */

    SW_VEGESTAB_INFO_INPUTS *v;
    const int nitems = 16;
    FILE *f;
    int lineno = 0;
    int resSNP;
    char name[80]; /* only allow 4 char sppnames */
    char inbuf[MAX_FILENAMESIZE];
    int inBufintRes = 0;
    double inBufDoubleVal = 0.;
    char *endSppPtr = NULL;
    char *sppPtr = NULL;

    Bool doIntConv;
    Bool sppFull = swFALSE;

    size_t sppWritesize = 0;

    if (SW_VegEstabSim->count == MAX_NSPECIES) {
        LogError(
            LogInfo,
            LOGERROR,
            "Too many species attempted to be created (maximum = %d).",
            MAX_NSPECIES
        );
        return;
    }

    v = &SW_VegEstabIn->parms[SW_VegEstabSim->count];
    SW_VegEstabSim->count++;

    endSppPtr = v->sppname + sizeof v->sppname - 1;
    sppWritesize = sizeof v->sppname;

    // have to copy before the pointer infile gets reset below by getAline
    resSNP = snprintf(v->sppFileName, sizeof v->sppFileName, "%s", infile);
    if (resSNP < 0 || (unsigned) resSNP >= (sizeof v->sppFileName)) {
        LogError(
            LogInfo,
            LOGERROR,
            "Establishment parameter file name is too long: '%s'.",
            infile
        );
        return; // Exit function prematurely due to error
    }

    f = OpenFile(infile, "r", LogInfo);
    if (LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

    while (GetALine(f, inbuf, MAX_FILENAMESIZE)) {

        if (lineno >= 1 && lineno <= 15) {
            /* Check to see if the line number contains an integer or double
             * value */
            doIntConv = (Bool) ((lineno >= 1 && lineno <= 2) ||
                                (lineno >= 5 && lineno <= 11));

            if (doIntConv) {
                inBufintRes = sw_strtoi(inbuf, infile, LogInfo);
            } else {
                inBufDoubleVal = sw_strtod(inbuf, infile, LogInfo);
            }

            if (LogInfo->stopRun) {
                goto closeFile;
            }
        }

        switch (lineno) {
        case 0:
            resSNP = snprintf(name, sizeof name, "%s", inbuf);
            if (resSNP < 0 || (unsigned) resSNP >= (sizeof name)) {
                LogError(
                    LogInfo,
                    LOGERROR,
                    "Establishment species name is too long: '%s'.",
                    inbuf
                );
                goto closeFile;
            }
            break;
        case 1:
            v->vegType = inBufintRes;
            break;
        case 2:
            v->estab_lyrs = inBufintRes;
            break;
        case 3:
            v->bars[SW_GERM_BARS] = fabs(inBufDoubleVal);
            break;
        case 4:
            v->bars[SW_ESTAB_BARS] = fabs(inBufDoubleVal);
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
            v->min_temp_germ = inBufDoubleVal;
            break;
        case 13:
            v->max_temp_germ = inBufDoubleVal;
            break;
        case 14:
            v->min_temp_estab = inBufDoubleVal;
            break;
        case 15:
            v->max_temp_estab = inBufDoubleVal;
            break;
        default:
            LogError(
                LogInfo,
                LOGERROR,
                "read_spp(): incorrect format of input file '%s'.",
                infile
            );
            goto closeFile;
            break;
        }

        /* check for valid name first */
        if (0 == lineno) {
            if (strlen(name) > MAX_SPECIESNAMELEN) {
                LogError(
                    LogInfo,
                    LOGERROR,
                    "%s: Species name <%s> too long (> %d chars).\n Try "
                    "again.\n",
                    infile,
                    name,
                    MAX_SPECIESNAMELEN
                );
                goto closeFile;
            }

            sppPtr = v->sppname;

            sppFull = sw_memccpy_inc(
                (void **) &sppPtr, endSppPtr, (void *) name, '\0', &sppWritesize
            );
            if (sppFull) {
                reportFullBuffer(LOGERROR, LogInfo);
                goto closeFile;
            }
        }

        lineno++; /*only increments when there's a value */
    }

    if (lineno != nitems) {
        LogError(
            LogInfo, LOGERROR, "%s : Too few/many input parameters.\n", infile
        );
    }

closeFile: { CloseFile(&f, LogInfo); }
}

/**
@brief Initializations performed after acquiring parameters after read() or some
other function call.

@param[in,out] *parmsIn List of structs of type SW_VEGESTAB_INFO_INPUTS holding
    input information about every vegetation species
@param[in] sppnum Index for which paramater is beign initialized.
@param[in] SW_SoilRunIn Struct of type SW_SOIL_RUN_INPUTS describing
    the simulated site's input values
@param[in] SW_SiteSim Struct of type SW_SITE_SIM describing the simulated site's
    simulation values
@param[in] n_transp_lyrs Layer index of deepest transp. region.
@param[out] LogInfo Holds information on warnings and errors
*/
void spp_init(
    SW_VEGESTAB_INFO_INPUTS *parmsIn,
    unsigned int sppnum,
    SW_SOIL_RUN_INPUTS *SW_SoilRunIn,
    SW_SITE_SIM *SW_SiteSim,
    LyrIndex n_transp_lyrs[],
    LOG_INFO *LogInfo
) {

    SW_VEGESTAB_INFO_INPUTS *parms_sppnum = &parmsIn[sppnum];
    IntU i;

    /* The thetas and psis etc should be initialized by now */
    /* because init_layers() must be called prior to this routine */
    /* (see watereqn() ) */
    parms_sppnum->min_swc_germ = SW_SWRC_SWPtoSWC(
        parms_sppnum->bars[SW_GERM_BARS], SW_SoilRunIn, SW_SiteSim, 0, LogInfo
    );
    if (LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

    /* due to possible differences in layer textures and widths, we need
     * to average the estab swc across the given layers to peoperly
     * compare the actual swc average in the checkit() routine */
    parms_sppnum->min_swc_estab = 0.;
    for (i = 0; i < parms_sppnum->estab_lyrs; i++) {
        parms_sppnum->min_swc_estab += SW_SWRC_SWPtoSWC(
            parms_sppnum->bars[SW_ESTAB_BARS],
            SW_SoilRunIn,
            SW_SiteSim,
            i,
            LogInfo
        );
        if (LogInfo->stopRun) {
            return; // Exit function prematurely due to error
        }
    }
    parms_sppnum->min_swc_estab /= (double) parms_sppnum->estab_lyrs;

    sanity_check(
        sppnum, SW_SiteSim->swcBulk_wiltpt, n_transp_lyrs, parmsIn, LogInfo
    );
}

static void sanity_check(
    unsigned int sppnum,
    double swcBulk_wiltpt[],
    LyrIndex n_transp_lyrs[],
    SW_VEGESTAB_INFO_INPUTS *parmsIn,
    LOG_INFO *LogInfo
) {
    /* =================================================== */
    SW_VEGESTAB_INFO_INPUTS *parms_sppnum = &parmsIn[sppnum];

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
    mean_wiltpt /= (double) parms_sppnum->estab_lyrs;

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
@brief Text output for VegEstab.

@param[in] width Width of the soil layer (cm)
@param[in] *parmsIn List of structs of type SW_VEGESTAB_INFO_INPUTS holding
    input information about every vegetation species
@param[in] count Held within type SW_VEGESTAB to determine
    how many species to check
@param[out] LogInfo Holds information on warnings and errors
*/
void echo_VegEstab(
    const double width[],
    SW_VEGESTAB_INFO_INPUTS *parmsIn,
    IntU count,
    LOG_INFO *LogInfo
) {
    /* --------------------------------------------------- */
    IntU i;
    Bool fullBuffer = swFALSE;
    char outstr[MAX_ERROR];
    char errstr[MAX_ERROR];
    char *endOutstr = errstr + sizeof errstr - 1;

    const char *endDispStr =
        "\n-----------------  End of Establishment Parameters ------------\n";

    size_t writeSize = MAX_ERROR;
    char *writePtr = outstr;

    (void) snprintf(
        errstr,
        MAX_ERROR,
        "\n=========================================================\n\n"
        "Parameters for the SoilWat Vegetation Establishment Check.\n"
        "----------------------------------------------------------\n"
        "Number of species to be tested: %d\n",
        count
    );

    fullBuffer = sw_memccpy_inc(
        (void **) &writePtr, endOutstr, (void *) errstr, '\0', &writeSize
    );
    if (fullBuffer) {
        goto reportFullBuffer;
    }

    for (i = 0; i < count; i++) {
        (void) snprintf(
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
            parmsIn[i].sppname,
            key2veg[parmsIn[i].vegType],
            parmsIn[i].vegType,
            parmsIn[i].bars[SW_GERM_BARS],
            parmsIn[i].min_swc_germ / width[0],
            parmsIn[i].min_swc_germ,
            parmsIn[i].min_temp_germ,
            parmsIn[i].max_temp_germ,
            parmsIn[i].min_pregerm_days,
            parmsIn[i].max_pregerm_days,
            parmsIn[i].min_wetdays_for_germ
        );

        fullBuffer = sw_memccpy_inc(
            (void **) &writePtr, endOutstr, (void *) errstr, '\0', &writeSize
        );
        if (fullBuffer) {
            goto reportFullBuffer;
        }

        (void) snprintf(
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
            parmsIn[i].estab_lyrs,
            parmsIn[i].bars[SW_ESTAB_BARS],
            parmsIn[i].estab_lyrs,
            parmsIn[i].min_swc_estab,
            parmsIn[i].min_temp_estab,
            parmsIn[i].max_temp_estab,
            parmsIn[i].min_days_germ2estab,
            parmsIn[i].max_days_germ2estab,
            parmsIn[i].min_wetdays_for_estab,
            parmsIn[i].max_drydays_postgerm
        );

        fullBuffer = sw_memccpy_inc(
            (void **) &writePtr, endOutstr, (void *) errstr, '\0', &writeSize
        );
        if (fullBuffer) {
            goto reportFullBuffer;
        }
    }
    fullBuffer = sw_memccpy_inc(
        (void **) &writePtr, endOutstr, (void *) endDispStr, '\0', &writeSize
    );

reportFullBuffer:
    if (fullBuffer) {
        reportFullBuffer(LOGWARN, LogInfo);
    }

    printf("%s\n", outstr);
}
