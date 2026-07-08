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
#include <stdio.h>                  // for NULL, snprintf, FILE
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
    const char *infile, SW_VEGESTAB_INPUTS *SW_VegEstabIn, LOG_INFO *LogInfo
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

@param[in,out] SW_VegEstabIn Struct of type SW_VEGESTAB_INPUTS holding all
    input information about vegetation within the simulation
@param[in,out] ves_p_accu A list of output structs of type SW_VEGESTAB_OUTPUTS
    to accumulate output
@param[in,out] ves_p_oagg A list of output structs of type SW_VEGESTAB_OUTPUTS
    to aggregate output
*/
void SW_VES_init_ptrs(
    SW_VEGESTAB_INPUTS *SW_VegEstabIn,
    SW_VEGESTAB_OUTPUTS *ves_p_accu,
    SW_VEGESTAB_OUTPUTS *ves_p_oagg
) {
    OutPeriod pd;

    SW_VegEstabIn->count = 0;

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
        memset(&ves_p_oagg[pd], 0, sizeof(SW_VEGESTAB_OUTPUTS));
        memset(&ves_p_accu[pd], 0, sizeof(SW_VEGESTAB_OUTPUTS));
    }
}

/**
@brief Deconstructor for SW_VegEstab for each period, pd.

@param[in] count Held within type SW_VEGESTAB_INPUTS to determine
    how many species to check
@param[in,out] ves_p_accu A list of output structs of type SW_VEGESTAB_OUTPUTS
    to accumulate output
@param[in,out] ves_p_oagg A list of output structs of type SW_VEGESTAB_OUTPUTS
    to aggregate output
*/
void SW_VES_deconstruct(
    IntU count, SW_VEGESTAB_OUTPUTS *ves_p_accu, SW_VEGESTAB_OUTPUTS *ves_p_oagg
) {
    // De-allocate days
    if (count > 0) {
        if (!isnull(ves_p_oagg[eSW_Year].days)) {
            free(ves_p_oagg[eSW_Year].days);
            ves_p_oagg[eSW_Year].days = NULL;
        }

        if (!isnull(ves_p_accu[eSW_Year].days)) {
            free(ves_p_accu[eSW_Year].days);
            ves_p_accu[eSW_Year].days = NULL;
        }
    }
}

/**
@brief We can use the debug memset because we allocated days, that is, it
wasn't allocated by the compiler.

@param[in] count Held within type SW_VEGESTAB_INPUTS to determine
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

    SW_VES_deconstruct(SW_VegEstabIn->count, ves_p_accu, ves_p_oagg);
    SW_VES_construct(SW_VegEstabIn, SW_VegEstabSim, ves_p_oagg, ves_p_accu);

    SW_VegEstabIn->use = use_VegEstab;

    int resSNP;
    char buf[FILENAME_MAX];
    char inbuf[MAX_FILENAMESIZE];
    FILE *f;

    if (SW_VegEstabIn->use) {
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
            SW_VegEstabIn->use = swFALSE;

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

                read_spp(buf, SW_VegEstabIn, LogInfo);
                if (LogInfo->stopRun) {
                    goto closeFile;
                }
            }

            SW_VegEstab_alloc_outptrs(
                ves_p_accu, ves_p_oagg, SW_VegEstabIn->count, LogInfo
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
@param[in] count Held within type SW_VEGESTAB_INPUTS to determine
    how many species to check
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

@param[in,out] **parmsIn List of structs of type SW_VEGESTAB_INFO_INPUTS holding
    information about every vegetation species
@param[in] SW_SoilRunIn Struct of type SW_SOIL_RUN_INPUTS describing
    the simulated site's input values
@param[in] SW_SiteSim Struct of type SW_SITE_SIM describing the simulated site's
    simulation values
@param[in] n_transp_lyrs Number of soil layers with roots
    per plant functional type
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
@param[in] count Held within type SW_VEGESTAB_INPUTS to determine
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
    IntU i;
    double avgswc; /* avg_swc today */

    if (doy == firstdoy) {
        zero_state(sppnum, parmsSim);
    }

    if (parmsSim->no_estab[sppnum] || parmsSim->estab_doy[sppnum] > 0) {
        goto LBL_Normal_Exit;
    }

    /* keep up with germinating wetness regardless of current state */
    if (GT(swcBulk[Today][0], parmsIn->min_swc_germ[sppnum])) {
        parmsSim->wetdays_for_germ[sppnum]++;
    } else {
        parmsSim->wetdays_for_germ[sppnum] = 0;
    }

    if (doy < parmsIn->min_pregerm_days[sppnum]) {
        goto LBL_Normal_Exit;
    }

    /* ---- check for germination, establishment */
    if (!parmsSim->germd[sppnum] && parmsSim->wetdays_for_germ[sppnum] >=
                                        parmsIn->min_wetdays_for_germ[sppnum]) {

        if (doy < parmsIn->min_pregerm_days[sppnum]) {
            goto LBL_Normal_Exit;
        }
        if (doy > parmsIn->max_pregerm_days[sppnum]) {
            parmsSim->no_estab[sppnum] = swTRUE;
            goto LBL_Normal_Exit;
        }
        /* temp doesn't affect wetdays */
        if (LT(avgtemp, parmsIn->min_temp_germ[sppnum]) ||
            GT(avgtemp, parmsIn->max_temp_germ[sppnum])) {
            goto LBL_Normal_Exit;
        }

        parmsSim->germd[sppnum] = swTRUE;
        goto LBL_Normal_Exit;

    } else {
        /* continue monitoring sprout's progress */

        /* any dry period (> max_drydays) or temp out of range
         * after germination means restart */
        avgswc = 0.;
        for (i = 0; i < parmsIn->estab_lyrs[sppnum];) {
            avgswc += swcBulk[Today][i++];
        }
        avgswc /= (double) parmsIn->estab_lyrs[sppnum];
        if (LT(avgswc, parmsIn->min_swc_estab[sppnum])) {
            parmsSim->drydays_postgerm[sppnum]++;
            parmsSim->wetdays_for_estab[sppnum] = 0;
        } else {
            parmsSim->drydays_postgerm[sppnum] = 0;
            parmsSim->wetdays_for_estab[sppnum]++;
        }

        if (parmsSim->drydays_postgerm[sppnum] >
                parmsIn->max_drydays_postgerm[sppnum] ||
            LT(avgtemp, parmsIn->min_temp_estab[sppnum]) ||
            GT(avgtemp, parmsIn->max_temp_estab[sppnum])) {
            /* too bad: discontinuity in environment, plant dies, start over */
            goto LBL_EstabFailed_Exit;
        }

        parmsSim->germ_days[sppnum]++;

        if (parmsSim->wetdays_for_estab[sppnum] <
                parmsIn->min_wetdays_for_estab[sppnum] ||
            parmsSim->germ_days[sppnum] <
                parmsIn->min_days_germ2estab[sppnum]) {
            goto LBL_Normal_Exit;
            /* no need to zero anything */
        }

        if (parmsSim->germ_days[sppnum] >
            parmsIn->max_days_germ2estab[sppnum]) {
            goto LBL_EstabFailed_Exit;
        }

        parmsSim->estab_doy[sppnum] = doy;
        goto LBL_Normal_Exit;
    }

LBL_EstabFailed_Exit:
    /* allows us to try again if not too late */
    parmsSim->wetdays_for_estab[sppnum] = 0;
    parmsSim->germ_days[sppnum] = 0;
    parmsSim->germd[sppnum] = swFALSE;

LBL_Normal_Exit:
    return;
}

static void zero_state(unsigned int sppnum, SW_VEGESTAB_INFO_SIM *parmsSim) {
    /* =================================================== */
    /* zero any values that need it for the new growing season */

    parmsSim->no_estab[sppnum] = parmsSim->germd[sppnum] = swFALSE;
    parmsSim->estab_doy[sppnum] = parmsSim->germ_days[sppnum] =
        parmsSim->drydays_postgerm[sppnum] = 0;
    parmsSim->wetdays_for_germ[sppnum] = parmsSim->wetdays_for_estab[sppnum] =
        0;
}

static void read_spp(
    const char *infile, SW_VEGESTAB_INPUTS *SW_VegEstabIn, LOG_INFO *LogInfo
) {
    /* =================================================== */

    SW_VEGESTAB_INFO_INPUTS *parmsIn = &SW_VegEstabIn->parms;
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
    IntU count = SW_VegEstabIn->count;

    size_t sppWritesize = 0;

    if (SW_VegEstabIn->count == MAX_NSPECIES) {
        LogError(
            LogInfo,
            LOGERROR,
            "Too many species attempted to be created (maximum = %d).",
            MAX_NSPECIES
        );
        return;
    }

    SW_VegEstabIn->count++;

    endSppPtr = parmsIn->sppname[count] + sizeof parmsIn->sppname[count] - 1;
    sppWritesize = sizeof parmsIn->sppname[count];

    // have to copy before the pointer infile gets reset below by getAline
    resSNP = snprintf(
        parmsIn->sppFileName[count],
        sizeof parmsIn->sppFileName[count],
        "%s",
        infile
    );
    if (resSNP < 0 ||
        (unsigned) resSNP >= (sizeof parmsIn->sppFileName[count])) {
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
            parmsIn->vegType[count] = (unsigned int) inBufintRes;
            break;
        case 2:
            parmsIn->estab_lyrs[count] = (unsigned int) inBufintRes;
            break;
        case 3:
            parmsIn->bars[count][SW_GERM_BARS] = fabs(inBufDoubleVal);
            break;
        case 4:
            parmsIn->bars[count][SW_ESTAB_BARS] = fabs(inBufDoubleVal);
            break;
        case 5:
            parmsIn->min_pregerm_days[count] = (TimeInt) inBufintRes;
            break;
        case 6:
            parmsIn->max_pregerm_days[count] = (TimeInt) inBufintRes;
            break;
        case 7:
            parmsIn->min_wetdays_for_germ[count] = (TimeInt) inBufintRes;
            break;
        case 8:
            parmsIn->max_drydays_postgerm[count] = (TimeInt) inBufintRes;
            break;
        case 9:
            parmsIn->min_wetdays_for_estab[count] = (TimeInt) inBufintRes;
            break;
        case 10:
            parmsIn->min_days_germ2estab[count] = (TimeInt) inBufintRes;
            break;
        case 11:
            parmsIn->max_days_germ2estab[count] = (TimeInt) inBufintRes;
            break;
        case 12:
            parmsIn->min_temp_germ[count] = inBufDoubleVal;
            break;
        case 13:
            parmsIn->max_temp_germ[count] = inBufDoubleVal;
            break;
        case 14:
            parmsIn->min_temp_estab[count] = inBufDoubleVal;
            break;
        case 15:
            parmsIn->max_temp_estab[count] = inBufDoubleVal;
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

            sppPtr = parmsIn->sppname[count];

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
@param[in] n_transp_lyrs Number of soil layers with roots
    per plant functional type
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
    IntU i;

    /* The thetas and psis etc should be initialized by now */
    /* because init_layers() must be called prior to this routine */
    /* (see watereqn() ) */
    parmsIn->min_swc_germ[sppnum] = SW_SWRC_SWPtoSWC(
        parmsIn->bars[sppnum][SW_GERM_BARS],
        SW_SoilRunIn,
        SW_SiteSim,
        0,
        LogInfo
    );
    if (LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

    /* due to possible differences in layer textures and widths, we need
     * to average the estab swc across the given layers to peoperly
     * compare the actual swc average in the checkit() routine */
    parmsIn->min_swc_estab[sppnum] = 0.;
    for (i = 0; i < parmsIn->estab_lyrs[sppnum]; i++) {
        parmsIn->min_swc_estab[sppnum] += SW_SWRC_SWPtoSWC(
            parmsIn->bars[sppnum][SW_ESTAB_BARS],
            SW_SoilRunIn,
            SW_SiteSim,
            i,
            LogInfo
        );
        if (LogInfo->stopRun) {
            return; // Exit function prematurely due to error
        }
    }
    parmsIn->min_swc_estab[sppnum] /= (double) parmsIn->estab_lyrs[sppnum];

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
    double mean_wiltpt;
    unsigned int i;

    if (parmsIn->vegType[sppnum] >= NVEGTYPES) {
        LogError(
            LogInfo,
            LOGERROR,
            "%s (%s) : Specified vegetation type (%d) is not implemented.",
            "VegEstab",
            parmsIn->sppname[sppnum],
            parmsIn->vegType[sppnum]
        );
        return; // Exit function prematurely due to error
    }

    if (parmsIn->estab_lyrs[sppnum] > n_transp_lyrs[parmsIn->vegType[sppnum]]) {
        LogError(
            LogInfo,
            LOGERROR,
            "%s (%s) : Layers requested (estab_lyrs = %d) > "
            "(# transpiration layers = %d).",
            "VegEstab",
            parmsIn->sppname[sppnum],
            parmsIn->estab_lyrs[sppnum],
            n_transp_lyrs[parmsIn->vegType[sppnum]]
        );
        return; // Exit function prematurely due to error
    }

    if (parmsIn->min_pregerm_days[sppnum] > parmsIn->max_pregerm_days[sppnum]) {
        LogError(
            LogInfo,
            LOGERROR,
            "%s (%s) : First day of germination > last day of germination.",
            "VegEstab",
            parmsIn->sppname[sppnum]
        );
        return; // Exit function prematurely due to error
    }

    if (parmsIn->min_wetdays_for_estab[sppnum] >
        parmsIn->max_days_germ2estab[sppnum]) {
        LogError(
            LogInfo,
            LOGERROR,
            "%s (%s) : Minimum wetdays after germination (%d) > "
            "maximum days allowed for establishment (%d).",
            "VegEstab",
            parmsIn->sppname[sppnum],
            parmsIn->min_wetdays_for_estab[sppnum],
            parmsIn->max_days_germ2estab[sppnum]
        );
        return; // Exit function prematurely due to error
    }

    if (parmsIn->min_swc_germ[sppnum] < swcBulk_wiltpt[0]) {
        LogError(
            LogInfo,
            LOGERROR,
            "%s (%s) : Minimum swc for germination (%.4f) < wiltpoint (%.4f)",
            "VegEstab",
            parmsIn->sppname[sppnum],
            parmsIn->min_swc_germ[sppnum],
            swcBulk_wiltpt[0]
        );
        return; // Exit function prematurely due to error
    }

    mean_wiltpt = 0.;
    for (i = 0; i < parmsIn->estab_lyrs[sppnum]; i++) {
        mean_wiltpt += swcBulk_wiltpt[i];
    }
    mean_wiltpt /= (double) parmsIn->estab_lyrs[sppnum];

    if (LT(parmsIn->min_swc_estab[sppnum], mean_wiltpt)) {
        LogError(
            LogInfo,
            LOGERROR,
            "%s (%s) : Minimum swc for establishment (%.4f) < wiltpoint (%.4f)",
            "VegEstab",
            parmsIn->sppname[sppnum],
            parmsIn->min_swc_estab[sppnum],
            mean_wiltpt
        );
    }
}

/**
@brief Text output for VegEstab.

@param[in] width Width of the soil layer (cm)
@param[in] *parmsIn List of structs of type SW_VEGESTAB_INFO_INPUTS holding
    input information about every vegetation species
@param[in] count Held within type SW_VEGESTAB_INPUTS to determine
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
            parmsIn->sppname[i],
            key2veg[parmsIn->vegType[i]],
            parmsIn->vegType[i],
            parmsIn->bars[i][SW_GERM_BARS],
            parmsIn->min_swc_germ[i] / width[0],
            parmsIn->min_swc_germ[i],
            parmsIn->min_temp_germ[i],
            parmsIn->max_temp_germ[i],
            parmsIn->min_pregerm_days[i],
            parmsIn->max_pregerm_days[i],
            parmsIn->min_wetdays_for_germ[i]
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
            parmsIn->estab_lyrs[i],
            parmsIn->bars[i][SW_ESTAB_BARS],
            parmsIn->estab_lyrs[i],
            parmsIn->min_swc_estab[i],
            parmsIn->min_temp_estab[i],
            parmsIn->max_temp_estab[i],
            parmsIn->min_days_germ2estab[i],
            parmsIn->max_days_germ2estab[i],
            parmsIn->min_wetdays_for_estab[i],
            parmsIn->max_drydays_postgerm[i]
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

    sw_printf("%s\n", outstr);
}
