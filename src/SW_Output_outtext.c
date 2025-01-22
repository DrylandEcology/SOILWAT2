/********************************************************/
/********************************************************/
/**
@file
@brief Output functionality for processing of text outputs that are written
to disk files

See the \ref out_algo "output algorithm documentation" for details.

History:
2018 June 15 (drs) moved functions from `SW_Output.c`
*/
/********************************************************/
/********************************************************/


/* =================================================== */
/*                INCLUDES / DEFINES                   */
/* --------------------------------------------------- */

#include "include/SW_Output_outtext.h" // for SW_OUT_close_textfiles, SW_OU...
#include "include/filefuncs.h"         // for CloseFile, OpenFile, LogError
#include "include/generic.h"           // for Bool, swFALSE, SOILWAT, IntUS
#include "include/myMemory.h"          // for Mem_Malloc
#include "include/SW_datastructs.h"    // for LOG_INFO
#include "include/SW_Defines.h"        // for OUTSEP, OutPeriod, ForEachOu...
#include "include/SW_Output.h"         // for pd2longstr, ForEachOutKey
#include <stdio.h>                     // for snprintf, fflush, fprintf
#include <stdlib.h>                    // for free
#include <string.h>                    // for memccpy

#if (defined(SOILWAT) && !defined(SWNETCDF)) || defined(STEPWAT)
#include "include/SW_Files.h" // for eOutputDaily, eOutputDaily_soil
#endif


/**
@brief Formatted output string for aggregated output

Defined as char `sw_outstr_agg[MAX_LAYERS * OUTSTRLEN];`

Used for output as returned from any function `get_XXX_agg` which
aggregates output across iterations/repeats for STEPWAT2

Active if @ref SW_OUT_DOM.print_IterationSummary is TRUE
*/
#define sw_outstr_agg
#undef sw_outstr_agg

/* =================================================== */
/*                  Local Variables                    */
/* --------------------------------------------------- */


/* =================================================== */
/*             Local Function Definitions              */
/* --------------------------------------------------- */

static void create_csv_headers(
    SW_OUT_DOM *OutDom,
    OutPeriod pd,
    char *str_reg,
    char *str_soil,
    Bool does_agg,
    LyrIndex n_layers,
    LOG_INFO *LogInfo
) {

#ifdef SOILWAT
    if (does_agg) {
        LogError(
            LogInfo,
            LOGERROR,
            "'create_csv_headers': value TRUE for "
            "argument 'does_agg' is not implemented for SOILWAT2-standalone."
        );
        return; // Exit function prematurely due to error
    }
#endif

    unsigned int i;
    unsigned int k;
    int resSNP;
    char key[50];
    Bool isTrue = swFALSE;

    size_t size_help = (size_t) (n_layers) *OUTSTRLEN;
    char *str_help1;
    char *str_help2;

    size_t writeSizeHelp = size_help;
    size_t writeSizeReg = (size_t) (2 * OUTSTRLEN);
    size_t writeSizeSoil = (size_t) (n_layers) *OUTSTRLEN;
    char *writePtrHelp = NULL;
    char *endHelpPtr = NULL;
    char *writePtrSoil = NULL;
    char *endSoilPtr = NULL;
    char *writePtrReg = NULL;
    char *endRegPtr = NULL;
    Bool fullBuffer = swFALSE;

    str_help1 = (char *) Mem_Malloc(
        sizeof(char) * size_help, "create_csv_headers()", LogInfo
    );
    if (LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

    str_help2 = (char *) Mem_Malloc(
        sizeof(char) * size_help, "create_csv_headers()", LogInfo
    );
    if (LogInfo->stopRun) {
        free(str_help1);
        return; // Exit function prematurely due to error
    }

    // Initialize headers
    str_reg[0] = (char) '\0';
    str_soil[0] = (char) '\0';

    writePtrReg = str_reg;
    writePtrSoil = str_soil;
    endRegPtr = str_reg + writeSizeReg - 1;
    endSoilPtr = str_soil + writeSizeSoil - 1;

    ForEachOutKey(k) {
        writePtrHelp = str_help2;
        endHelpPtr = str_help2 + size_help - 1;

        isTrue = (Bool) (OutDom->use[k] && has_OutPeriod_inUse(
                                               pd,
                                               (OutKey) k,
                                               OutDom->used_OUTNPERIODS,
                                               OutDom->timeSteps
                                           ));
        if (isTrue) {
            (void) sw_memccpy(key, (char *) key2str[k], '\0', sizeof key);
            str_help2[0] = '\0';

            for (i = 0; i < OutDom->ncol_OUT[k]; i++) {
                if (does_agg) {
                    resSNP = snprintf(
                        str_help1,
                        size_help,
                        "%c%s_%s_Mean%c%s_%s_SD",
                        OUTSEP,
                        key,
                        OutDom->colnames_OUT[k][i],
                        OUTSEP,
                        key,
                        OutDom->colnames_OUT[k][i]
                    );
                } else {
                    resSNP = snprintf(
                        str_help1,
                        size_help,
                        "%c%s_%s",
                        OUTSEP,
                        key,
                        OutDom->colnames_OUT[k][i]
                    );
                }

                if (resSNP < 0 || (unsigned) resSNP >= size_help) {
                    LogError(LogInfo, LOGERROR, "csv-header is too long.");
                    goto freeMem; // Exit function prematurely due to error
                }

                fullBuffer = sw_memccpy_inc(
                    (void **) &writePtrHelp,
                    endHelpPtr,
                    (void *) str_help1,
                    '\0',
                    &writeSizeHelp
                );
                if (fullBuffer) {
                    goto freeMem;
                }
            }

            if (OutDom->has_sl[k]) {
                fullBuffer = sw_memccpy_inc(
                    (void **) &writePtrSoil,
                    endSoilPtr,
                    (void *) str_help2,
                    '\0',
                    &writeSizeSoil
                );
                if (fullBuffer) {
                    goto freeMem;
                }
            } else {
                fullBuffer = sw_memccpy_inc(
                    (void **) &writePtrReg,
                    endRegPtr,
                    (void *) str_help2,
                    '\0',
                    &writeSizeReg
                );
                if (fullBuffer) {
                    goto freeMem;
                }
            }
        }
    }

freeMem:
    if (fullBuffer) {
        reportFullBuffer(LOGERROR, LogInfo);
    }

    free(str_help1);
    free(str_help2);
}


#if defined(SOILWAT) && !defined(SWNETCDF)
/**
@brief Create `csv` output files for specified time step

@param[in,out] SW_PathOutputs Struct of type SW_PATH_OUTPUTS which
    holds basic information about output files and values
@param[in] pd The output time step.
@param[in] txtInFiles Array of program in/output files
@param[out] LogInfo Holds information on warnings and errors
*/
/***********************************************************/
static void create_csv_files(
    SW_PATH_OUTPUTS *SW_PathOutputs,
    OutPeriod pd,
    char *txtInFiles[],
    LOG_INFO *LogInfo
) {
    // PROGRAMMER Note: `eOutputDaily + pd` is not very elegant and assumes
    // a specific order of `SW_FileIndex` --> fix and create something that
    // allows subsetting such as `eOutputFile[pd]` or append time period to
    // a basename, etc.

    if (SW_PathOutputs->make_regular[pd]) {
        SW_PathOutputs->fp_reg[pd] =
            OpenFile(txtInFiles[eOutputDaily + pd], "w", LogInfo);
        if (LogInfo->stopRun) {
            return; // Exit function prematurely due to error
        }
    }

    if (SW_PathOutputs->make_soil[pd]) {
        SW_PathOutputs->fp_soil[pd] =
            OpenFile(txtInFiles[eOutputDaily_soil + pd], "w", LogInfo);
        if (LogInfo->stopRun) {
            return; // Exit function prematurely due to error
        }
    }
}
#endif


static void get_outstrheader(OutPeriod pd, char *str, size_t sizeof_str) {
    switch (pd) {
    case eSW_Day:
        (void) snprintf(
            str, sizeof_str, "%s%c%s", "Year", OUTSEP, pd2longstr[eSW_Day]
        );
        break;

    case eSW_Week:
        (void) snprintf(
            str, sizeof_str, "%s%c%s", "Year", OUTSEP, pd2longstr[eSW_Week]
        );
        break;

    case eSW_Month:
        (void) snprintf(
            str, sizeof_str, "%s%c%s", "Year", OUTSEP, pd2longstr[eSW_Month]
        );
        break;

    case eSW_Year:
        (void) sw_memccpy(str, "Year", '\0', sizeof_str);
        break;

    default:
        break;
    }
}


#if defined(STEPWAT)
/** Splits a filename such as `name.ext` into its two parts `name` and `ext`;
    appends `flag` and, if positive, `iteration` to `name` with `_`
    as separator; and returns the full name concatenated

\return `name_flagiteration.ext`
*/
static void create_filename_ST(
    char *str,
    char *flag,
    int iteration,
    char *filename,
    size_t sizeof_filename,
    LOG_INFO *LogInfo
) {
    size_t startIndex = 0;
    size_t strLen = 0; // For `sw_strtok()`
    int resSNP;

    char *basename;
    char *ext;
    char *fileDup = Str_Dup(str, LogInfo);
    if (LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

    // Determine basename and file extension
    basename = sw_strtok(fileDup, &startIndex, &strLen, ".");
    ext = sw_strtok(fileDup, &startIndex, &strLen, ".");

    // Put new file together
    if (iteration > 0) {
        resSNP = snprintf(
            filename,
            sizeof_filename,
            "%s_%s%d.%s",
            basename,
            flag,
            iteration,
            ext
        );
    } else {
        resSNP = snprintf(
            filename, sizeof_filename, "%s_%s.%s", basename, flag, ext
        );
    }

    free(fileDup);

    if (resSNP < 0 || (unsigned) resSNP >= sizeof_filename) {
        LogError(
            LogInfo, LOGERROR, "csv file name is too long: '%s'", filename
        );
    }
}

/**
@brief Creates `csv` output files for specified time step depending on
`-o` and `-i` flags, for `STEPWAT2`.

If `-i` flag is used, then this function creates a file for each `iteration`
with the file name containing the value of `iteration`.

If `-o` flag is used, then this function creates only one set of output files.

@param iteration Current iteration value (base1) that is used for the file
name if -i flag used in STEPWAT2. Set to a negative value otherwise.
@param pd The output time step.
@param SW_PathOutputs Struct of type SW_PATH_OUTPUTS which
holds basic information about output files and values
@param LogInfo Holds information on warnings and errors
*/
/***********************************************************/
static void create_csv_file_ST(
    int iteration,
    OutPeriod pd,
    char *txtInFiles[],
    SW_PATH_OUTPUTS *SW_PathOutputs,
    LOG_INFO *LogInfo
) {
    char filename[FILENAME_MAX];

    if (iteration <= 0) {
        // STEPWAT2: aggregated values over all iterations
        if (SW_PathOutputs->make_regular[pd]) {
            // PROGRAMMER Note: `eOutputDaily + pd` is not very elegant and
            // assumes a specific order of `SW_FileIndex` --> fix and create
            // something that allows subsetting such as `eOutputFile[pd]` or
            // append time period to a basename, etc.
            create_filename_ST(
                txtInFiles[eOutputDaily + pd],
                "agg",
                0,
                filename,
                FILENAME_MAX,
                LogInfo
            );
            if (LogInfo->stopRun) {
                return; // Exit function prematurely due to error
            }

            SW_PathOutputs->fp_reg_agg[pd] = OpenFile(filename, "w", LogInfo);
            if (LogInfo->stopRun) {
                return; // Exit function prematurely due to error
            }
        }

        if (SW_PathOutputs->make_soil[pd]) {
            create_filename_ST(
                txtInFiles[eOutputDaily_soil + pd],
                "agg",
                0,
                filename,
                FILENAME_MAX,
                LogInfo
            );
            if (LogInfo->stopRun) {
                return; // Exit function prematurely due to error
            }

            SW_PathOutputs->fp_soil_agg[pd] = OpenFile(filename, "w", LogInfo);
            if (LogInfo->stopRun) {
                return; // Exit function prematurely due to error
            }
        }

    } else {
        // STEPWAT2: storing values for every iteration
        if (iteration > 1) {
            // close files from previous iteration
            if (SW_PathOutputs->make_regular[pd]) {
                CloseFile(&SW_PathOutputs->fp_reg[pd], LogInfo);
            }
            if (SW_PathOutputs->make_soil[pd]) {
                CloseFile(&SW_PathOutputs->fp_soil[pd], LogInfo);
            }
        }

        if (SW_PathOutputs->make_regular[pd]) {
            create_filename_ST(
                txtInFiles[eOutputDaily + pd],
                "rep",
                iteration,
                filename,
                FILENAME_MAX,
                LogInfo
            );
            if (LogInfo->stopRun) {
                return; // Exit function prematurely due to error
            }

            SW_PathOutputs->fp_reg[pd] = OpenFile(filename, "w", LogInfo);
            if (LogInfo->stopRun) {
                return; // Exit function prematurely due to error
            }
        }

        if (SW_PathOutputs->make_soil[pd]) {
            create_filename_ST(
                txtInFiles[eOutputDaily_soil + pd],
                "rep",
                iteration,
                filename,
                FILENAME_MAX,
                LogInfo
            );
            if (LogInfo->stopRun) {
                return; // Exit function prematurely due to error
            }

            SW_PathOutputs->fp_soil[pd] = OpenFile(filename, "w", LogInfo);
        }
    }
}
#endif


/* =================================================== */
/*             Global Function Definitions             */
/* --------------------------------------------------- */


#if defined(SOILWAT) && !defined(SWNETCDF)

/**
@brief create all of the user-specified output text files.

@param[in] OutDom Struct of type SW_OUT_DOM that holds output
    information that do not change throughout simulation runs
@param[in,out] SW_PathOutputs Struct of type SW_PATH_OUTPUTS which
holds basic information about output files and values
@param[in] n_layers Number of layers of soil within the simulation run
@param[in] txtInFiles Array of program in/output files
@param[out] LogInfo Holds information on warnings and errors
*/
void SW_OUT_create_textfiles(
    SW_OUT_DOM *OutDom,
    SW_PATH_OUTPUTS *SW_PathOutputs,
    LyrIndex n_layers,
    char *txtInFiles[],
    LOG_INFO *LogInfo
) {

    OutPeriod pd;

    ForEachOutPeriod(pd) {
        if (OutDom->use_OutPeriod[pd]) {
            create_csv_files(SW_PathOutputs, pd, txtInFiles, LogInfo);
            if (LogInfo->stopRun) {
                return; // Exit function prematurely due to error
            }

            write_headers_to_csv(
                OutDom,
                pd,
                SW_PathOutputs->fp_reg[pd],
                SW_PathOutputs->fp_soil[pd],
                swFALSE,
                SW_PathOutputs->make_regular,
                SW_PathOutputs->make_soil,
                n_layers,
                LogInfo
            );
            if (LogInfo->stopRun) {
                return; // Exit function prematurely due to error
            }
        }
    }
}

#elif defined(STEPWAT)

void SW_OUT_create_summary_files(
    SW_OUT_DOM *OutDom,
    SW_PATH_OUTPUTS *SW_PathOutputs,
    char *txtInFiles[],
    LyrIndex n_layers,
    LOG_INFO *LogInfo
) {

    OutPeriod p;

    ForEachOutPeriod(p) {
        if (OutDom->use_OutPeriod[p]) {
            create_csv_file_ST(-1, p, txtInFiles, SW_PathOutputs, LogInfo);
            if (LogInfo->stopRun) {
                return; // Exit function prematurely due to error
            }

            write_headers_to_csv(
                OutDom,
                p,
                SW_PathOutputs->fp_reg_agg[p],
                SW_PathOutputs->fp_soil_agg[p],
                swTRUE,
                SW_PathOutputs->make_regular,
                SW_PathOutputs->make_soil,
                n_layers,
                LogInfo
            );
            if (LogInfo->stopRun) {
                return; // Exit function prematurely due to error
            }
        }
    }
}

void SW_OUT_create_iteration_files(
    SW_OUT_DOM *OutDom,
    SW_PATH_OUTPUTS *SW_PathOutputs,
    int iteration,
    char *txtInFiles[],
    LyrIndex n_layers,
    LOG_INFO *LogInfo
) {

    OutPeriod p;

    ForEachOutPeriod(p) {
        if (OutDom->use_OutPeriod[p]) {
            create_csv_file_ST(
                iteration, p, txtInFiles, SW_PathOutputs, LogInfo
            );
            if (LogInfo->stopRun) {
                return; // Exit function prematurely due to error
            }

            write_headers_to_csv(
                OutDom,
                p,
                SW_PathOutputs->fp_reg[p],
                SW_PathOutputs->fp_soil[p],
                swFALSE,
                SW_PathOutputs->make_regular,
                SW_PathOutputs->make_soil,
                n_layers,
                LogInfo
            );
            if (LogInfo->stopRun) {
                return; // Exit function prematurely due to error
            }
        }
    }
}

#endif


/** Periodic output for Month and/or Week are actually
    printing for the PREVIOUS month or week.
    Also, see note on test value in _write_today() for
    explanation of the +1.

@param[in] pd Time period in simulation output (day/week/month/year)
@param[in] sizeof_str Size of parameter "str"
@param[in] SW_Model Struct of type SW_MODEL holding basic time
    information about the simulation
@param[in] tOffset Offset describing with the previous or current period
@param[out] str String header buffer for every output row
*/
void get_outstrleader(
    OutPeriod pd,
    size_t sizeof_str,
    SW_MODEL *SW_Model,
    TimeInt tOffset,
    char *str
) {
    switch (pd) {
    case eSW_Day:
        (void) snprintf(
            str, sizeof_str, "%d%c%d", SW_Model->simyear, OUTSEP, SW_Model->doy
        );
        break;

    case eSW_Week:
        (void) snprintf(
            str,
            sizeof_str,
            "%d%c%d",
            SW_Model->simyear,
            OUTSEP,
            (SW_Model->week + 1) - tOffset
        );
        break;

    case eSW_Month:
        (void) snprintf(
            str,
            sizeof_str,
            "%d%c%d",
            SW_Model->simyear,
            OUTSEP,
            (SW_Model->month + 1) - tOffset
        );
        break;

    case eSW_Year:
        (void) snprintf(str, sizeof_str, "%d", SW_Model->simyear);
        break;

    default:
        break;
    }
}

/**
@brief Creates column headers for output files

write_headers_to_csv() is called only once for each set of output files and it
goes through all values and if the value is defined to be used it creates the
header in the output file.

@note The functions SW_OUT_set_ncol() and SW_OUT_set_colnames() must
be called before create_csv_headers(); otherwise, `ncol_OUT` and
`colnames_OUT` are not set.

@param OutDom Struct of type SW_OUT_DOM that holds output
    information that do not change throughout simulation runs
@param pd timeperiod so it can write headers to correct output file.
@param fp_reg name of file.
@param fp_soil name of file.
@param does_agg Indicate whether output is aggregated (`-o` option) or
    for each SOILWAT2 run (`-i` option)
@param make_regular Array of size SW_OUTNPERIODS which holds boolean values
    specifying to output "regular" header names
@param make_soil Array of size SW_OUTNPERIODS which holds boolean values
    specifying to output a soil-related header names
@param n_layers Number of soil layers being dealt with in a simulation
@param LogInfo Holds information on warnings and errors
*/
void write_headers_to_csv(
    SW_OUT_DOM *OutDom,
    OutPeriod pd,
    FILE *fp_reg,
    FILE *fp_soil,
    Bool does_agg,
    const Bool make_regular[],
    const Bool make_soil[],
    LyrIndex n_layers,
    LOG_INFO *LogInfo
) {

    char str_time[20];
    char
        // 3500 characters required for does_agg = TRUE
        header_reg[2 * OUTSTRLEN];

    // 26500 characters required for 25 soil layers and does_agg = TRUE
    size_t size_hs = (size_t) (n_layers) *OUTSTRLEN;
    char *header_soil;
    int fprintRes = 0;

    header_soil = (char *) Mem_Malloc(
        sizeof(char) * size_hs, "write_headers_to_csv()", LogInfo
    );
    if (LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }


    // Acquire headers
    get_outstrheader(pd, str_time, sizeof str_time);
    create_csv_headers(
        OutDom, pd, header_reg, header_soil, does_agg, n_layers, LogInfo
    );
    if (LogInfo->stopRun) {
        free(header_soil);
        return; // Exit function prematurely due to error
    }

    // Write headers to files
    if (make_regular[pd]) {
        fprintRes = fprintf(fp_reg, "%s%s\n", str_time, header_reg);

        if (fprintRes < 0) {
            LogError(
                LogInfo,
                LOGERROR,
                "Could not write headers to \"regular\" CSVs."
            );
            return; /* Exit prematurely due to error */
        }

        if (fflush(fp_reg) == EOF) {
            LogError(
                LogInfo,
                LOGERROR,
                "Could note write headers to \"regular\" CSVs."
            );
            return; /* Exit prematurely due to error */
        }
    }

    if (make_soil[pd]) {
        fprintRes = fprintf(fp_soil, "%s%s\n", str_time, header_soil);

        if (fprintRes < 0) {
            LogError(
                LogInfo, LOGERROR, "Could not write headers to \"soil\" CSVs."
            );
            return; /* Exit prematurely due to error */
        }

        if (fflush(fp_soil) == EOF) {
            LogError(
                LogInfo, LOGERROR, "Could note flush headers to \"soil\" CSVs."
            );
            return; /* Exit prematurely due to error */
        }
    }

    free(header_soil);
}

void find_TXToutputSoilReg_inUse(
    Bool make_soil[],
    Bool make_regular[],
    const Bool has_sl[],
    OutPeriod timeSteps[][SW_OUTNPERIODS],
    IntUS used_OUTNPERIODS
) {
    int i;
    int k;

    ForEachOutPeriod(i) {
        make_soil[i] = swFALSE;
        make_regular[i] = swFALSE;
    }

    ForEachOutKey(k) {
        for (i = 0; i < used_OUTNPERIODS; i++) {
            if (timeSteps[k][i] != eSW_NoTime) {
                if (has_sl[k]) {
                    make_soil[timeSteps[k][i]] = swTRUE;
                } else {
                    make_regular[timeSteps[k][i]] = swTRUE;
                }
            }
        }
    }
}

/**
@brief close all of the user-specified output text files.

call this routine at the end of the program run.

@param[in,out] SW_PathOutputs Struct of type SW_PATH_OUTPUTS which
holds basic information about output files and values
@param[in] OutDom Struct of type SW_OUT_DOM that holds output
    information that do not change throughout simulation runs
@param[out] LogInfo Holds information on warnings and errors
*/
void SW_OUT_close_textfiles(
    SW_PATH_OUTPUTS *SW_PathOutputs, SW_OUT_DOM *OutDom, LOG_INFO *LogInfo
) {

    Bool close_regular = swFALSE;
    Bool close_layers = swFALSE;
    Bool close_aggs = swFALSE;
    OutPeriod p;


    ForEachOutPeriod(p) {
#if defined(SOILWAT)
        close_regular = SW_PathOutputs->make_regular[p];
        close_layers = SW_PathOutputs->make_soil[p];
        close_aggs = swFALSE;

#elif defined(STEPWAT)
        close_regular = (Bool) (SW_PathOutputs->make_regular[p] &&
                                OutDom->storeAllIterations);
        close_layers =
            (Bool) (SW_PathOutputs->make_soil[p] && OutDom->storeAllIterations);
        close_aggs = (Bool) ((SW_PathOutputs->make_regular[p] ||
                              SW_PathOutputs->make_soil[p]) &&
                             OutDom->prepare_IterationSummary);

#endif

        if (OutDom->use_OutPeriod[p]) {
            if (close_regular) {
                CloseFile(&SW_PathOutputs->fp_reg[p], LogInfo);
            }

            if (close_layers) {
                CloseFile(&SW_PathOutputs->fp_soil[p], LogInfo);
            }

            if (close_aggs) {
#ifdef STEPWAT
                CloseFile(&SW_PathOutputs->fp_reg_agg[p], LogInfo);

                if (close_layers) {
                    CloseFile(&SW_PathOutputs->fp_soil_agg[p], LogInfo);
                }
#endif
            }
        }
    }
}
