/* =================================================== */
/*                INCLUDES / DEFINES                   */
/* --------------------------------------------------- */
#include "include/SW_netCDF_Output.h" // for SW_NCOUT_read_out_vars, ...
#include "include/filefuncs.h"        // for LogError, FileExists, CloseFile
#include "include/generic.h"          // for Bool, swFALSE, LOGERROR, swTRUE
#include "include/myMemory.h"         // for Str_Dup, Mem_Malloc
#include "include/SW_datastructs.h"   // for LOG_INFO, SW_NETCDF_OUT, SW_DOMAIN
#include "include/SW_Defines.h"       // for MAX_FILENAMESIZE, OutPeriod
#include "include/SW_Domain.h"        // for SW_DOM_calc_ncSuid
#include "include/SW_Files.h"         // for eNCInAtt, eNCIn, eNCOutVars
#include "include/SW_netCDF_General.h"
#include "include/SW_netCDF_Input.h"    // for SW_NCOUT_read_out_vars, ...
#include "include/SW_Output.h"          // for ForEachOutKey, SW_ESTAB, pd2...
#include "include/SW_Output_outarray.h" // for iOUTnc
#include "include/SW_VegProd.h"         // for key2veg
#include "include/Times.h"              // for isleapyear, timeStringISO8601
#include <math.h>                       // for NAN, ceil, isnan
#include <netcdf.h>                     // for NC_NOERR, nc_close, NC_DOUBLE
#include <stdio.h>                      // for size_t, NULL, snprintf, sscanf
#include <stdlib.h>                     // for free, strtod
#include <string.h>                     // for strcmp, strlen, strstr, memcpy


/* =================================================== */
/*                   Local Defines                     */
/* --------------------------------------------------- */

/** Number of columns in 'Input_nc/SW2_netCDF_output_variables.tsv' */
#define NOUT_VAR_INPUTS 12

/** Number of columns within the output variable netCDF of interest */
#define NUM_OUTPUT_INFO 6

#define MAX_ATTVAL_SIZE 256

// Indices to second dimension of `outputVarInfo[varIndex][attIndex]`
#define DIM_INDEX 0 // unused
#define VARNAME_INDEX 1
#define LONGNAME_INDEX 2
#define COMMENT_INDEX 3
#define UNITS_INDEX 4
#define CELLMETHOD_INDEX 5

const int times[] = {MAX_DAYS - 1, MAX_WEEKS, MAX_MONTHS, 1};

static const char *const SWVarUnits[SW_OUTNKEYS][SW_OUTNMAXVARS] = {
    {NULL},                                           /* WTHR */
    {"degC", "degC", "degC", "degC", "degC", "degC"}, /* TEMP */
    {"cm", "cm", "cm", "cm", "cm"},                   /* PRECIP */
    {"cm"},                                           /* SOILINFILT */
    {"cm", "cm", "cm", "cm"},                         /* RUNOFF */
    {NULL},                                           /* ALLH2O */
    {"cm cm-1"},                                      /* VWCBULK */
    {"cm cm-1"},                                      /* VWCMATRIC */
    {"cm"},                                           /* SWCBULK */
    {"cm"},                                           /* SWABULK */
    {"cm"},                                           /* SWAMATRIC */
    {"cm"},                                           /* SWA */
    {"-1bar"},                                        /* SWPMATRIC */
    {"cm"},                                           /* SURFACEWATER */
    {"cm", "cm"},                                     /* TRANSP */
    {"cm"},                                           /* EVAPSOIL */
    {"cm", "cm", "cm", "cm"},                         /* EVAPSURFACE */
    {"cm", "cm", "cm"},                               /* INTERCEPTION */
    {"cm"},                                           /* LYRDRAIN */
    {"cm", "cm"},                                     /* HYDRED */
    {NULL},                                           /* ET */
    {"cm", "cm", "cm", "cm", "cm", "cm"},             /* AET */
    {"cm", "MJ m-2", "MJ m-2", "MJ m-2", "MJ m-2"},   /* PET */
    {"1"},                                            /* WETDAY */
    {"cm", "cm"},                                     /* SNOWPACK */
    {"cm"},                                           /* DEEPSWC */
    {"degC", "degC", "degC"},                         /* SOILTEMP */
    {"1"},                                            /* FROZEN */
    {NULL},                                           /* ALLVEG */
    {"1"},                                            /* ESTABL */
    {"1", "1"},                                       /* CO2EFFECTS */

    /* BIOMASS */
    {"1", "1", "g m-2", "g m-2", "g m-2", "g m-2", "g m-2", "m m-2"}
};

static const char *const possKeys[SW_OUTNKEYS][SW_OUTNMAXVARS] = {
    {NULL}, // WTHR
    {"TEMP__temp_max",
     "TEMP__temp_min",
     "TEMP__temp_avg",
     "TEMP__surfaceMax",
     "TEMP__surfaceMin",
     "TEMP__surfaceAvg"},

    {"PRECIP__ppt",
     "PRECIP__rain",
     "PRECIP__snow",
     "PRECIP__snowmelt",
     "PRECIP__snowloss"},

    {"SOILINFILT__soil_inf"},

    {"RUNOFF__net",
     "RUNOFF__surfaceRunoff",
     "RUNOFF__snowRunoff",
     "RUNOFF__surfaceRunon"},

    {NULL}, // ALLH2O

    // VWCBULK -> SURFACEWATER
    {"VWCBULK__vwcBulk"},
    {"VWCMATRIC__vwcMatric"},
    {"SWCBULK__swcBulk"},
    {"SWABULK__swaBulk"},
    {"SWAMATRIC__swaMatric"},
    {"SWA__SWA_VegType"},
    {"SWPMATRIC__swpMatric"},
    {"SURFACEWATER__surfaceWater"},

    {"TRANSP__transp_total", "TRANSP__transp"},

    {"EVAPSOIL__evap_baresoil"},

    {"EVAPSURFACE__total_evap",
     "EVAPSURFACE__evap_veg",
     "EVAPSURFACE__litter_evap",
     "EVAPSURFACE__surfaceWater_evap"},

    {"INTERCEPTION__total_int",
     "INTERCEPTION__int_veg",
     "INTERCEPTION__litter_int"},

    {"LYRDRAIN__lyrdrain"},

    {"HYDRED__hydred_total", "HYDRED__hydred"},

    {NULL}, // ET

    {"AET__aet",
     "AET__tran",
     "AET__esoil",
     "AET__ecnw",
     "AET__esurf",
     "AET__snowloss"},

    {"PET__pet", "PET__H_oh", "PET__H_ot", "PET__H_gh", "PET__H_gt"},

    {"WETDAY__is_wet"},

    {"SNOWPACK__snowpack", "SNOWPACK__snowdepth"},

    {"DEEPSWC__deep"},

    {"SOILTEMP__maxLyrTemperature",
     "SOILTEMP__minLyrTemperature",
     "SOILTEMP__avgLyrTemp"},

    {"FROZEN__lyrFrozen"},

    {NULL}, // ALLVEG

    {NULL}, // ESTABL -- handled differently

    {"CO2EFFECTS__veg.co2_multipliers[BIO_INDEX]",
     "CO2EFFECTS__veg.co2_multipliers[WUE_INDEX]"},

    {"BIOMASS__bare_cov.fCover",
     "BIOMASS__veg.cov.fCover",
     "BIOMASS__biomass_total",
     "BIOMASS__veg.biomass_inveg",
     "BIOMASS__litter_total",
     "BIOMASS__biolive_total",
     "BIOMASS__veg.biolive_inveg",
     "BIOMASS__LAI"}
};

/* =================================================== */
/*             Local Function Definitions              */
/* --------------------------------------------------- */

/**
@brief Convert a read-in key (<OUTKEY>__<SW2 Variable Name>) into
it's respective numeric values
*/
static void get_2d_output_key(
    char *varKey, OutKey *outKey, int *outVarNum, const IntUS nvar_OUT[]
) {

    int k;
    int varNum;
    const int establSize = 6;

    *outKey = eSW_NoKey;
    *outVarNum = KEY_NOT_FOUND;

    if (strncmp(varKey, SW_ESTAB, establSize) == 0) {
        *outKey = eSW_Estab;
    } else {
        ForEachOutKey(k) {
            if (k != eSW_Estab) {
                for (varNum = 0; varNum < nvar_OUT[k]; varNum++) {
                    if (!isnull(possKeys[k][varNum])) {
                        if (strcmp(possKeys[k][varNum], varKey) == 0) {

                            *outKey = (OutKey) k;
                            *outVarNum = varNum;

                            return;
                        }
                    }
                }
            }
        }
    }
}

/**
@brief Calculate time size in days

@param[in] rangeStart Start year for the current output file
@param[in] rangeEnd End year for the current output file
@param[in] baseTime Base number of output periods in a year
    (e.g., 60 months in 5 years, or 731 days in 1980-1981)
@param[in] pd Current output netCDF period
*/
static unsigned int calc_timeSize(
    unsigned int rangeStart,
    unsigned int rangeEnd,
    unsigned int baseTime,
    OutPeriod pd
) {

    unsigned int numLeapYears = 0;
    unsigned int year;

    if (pd == eSW_Day) {
        for (year = rangeStart; year < rangeEnd; year++) {
            if (isleapyear(year)) {
                numLeapYears++;
            }
        }
    }

    return baseTime * (rangeEnd - rangeStart) + numLeapYears;
}

/**
@brief Helper function to `fill_dimVar()`; fully creates/fills
the variable "time_bnds" and fills the variable "time"

@param[in] ncFileID Identifier of the netCDF in which the information
    will be written
@param[in] dimIDs Dimension identifiers for "vertical" and "bnds"
@param[in] size Size of the vertical dimension/variable
@param[in] dimVarID "time" dimension identifier
@param[in] startYr Start year of the simulation
@param[in,out] startTime Start number of days when dealing with
    years between netCDF files
@param[in] pd Current output netCDF period
@param[in] deflateLevel Level of deflation that will be used for the created
variable
@param[out] LogInfo Holds information dealing with logfile output
*/
static void create_time_vars(
    int ncFileID,
    int dimIDs[],
    unsigned int size,
    int dimVarID,
    unsigned int startYr,
    double *startTime,
    OutPeriod pd,
    int deflateLevel,
    LOG_INFO *LogInfo
) {

    double *bndsVals = NULL;
    double *dimVarVals = NULL;
    const int numBnds = 2;
    size_t start[] = {0, 0};
    size_t count[] = {(size_t) size, 0};
    unsigned int currYear = startYr;
    unsigned int month = 0;
    unsigned int week = 0;
    unsigned int numDays = 0;
    int bndsID = 0;


    SW_NC_create_netCDF_var(
        &bndsID,
        "time_bnds",
        dimIDs,
        &ncFileID,
        NC_DOUBLE,
        numBnds,
        NULL,
        deflateLevel,
        LogInfo
    );
    if (LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

    dimVarVals = (double *) Mem_Malloc(
        size * sizeof(double), "create_time_vars", LogInfo
    );
    if (LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

    bndsVals = (double *) Mem_Malloc(
        (size_t) (size * numBnds) * sizeof(double), "create_time_vars", LogInfo
    );
    if (LogInfo->stopRun) {
        free(dimVarVals);
        return; // Exit function prematurely due to error
    }

    for (size_t index = 0; index < (size_t) size; index++) {

        switch (pd) {
        case eSW_Day:
            numDays = 1;
            break;

        case eSW_Week:
            if (week == MAX_WEEKS - 1) {
                // last "week" (7-day period) is either 1 or 2 days long
                numDays = isleapyear(currYear) ? 2 : 1;
            } else {
                numDays = WKDAYS;
            }

            currYear += (index % MAX_WEEKS == 0) ? 1 : 0;
            week = (week + 1) % MAX_WEEKS;
            break;

        case eSW_Month:
            if (month == Feb) {
                numDays = isleapyear(currYear) ? 29 : 28;
            } else {
                numDays = monthdays[month];
            }

            currYear += (index % MAX_MONTHS == 0) ? 1 : 0;
            month = (month + 1) % MAX_MONTHS;
            break;

        case eSW_Year:
            numDays = Time_get_lastdoy_y(currYear);
            currYear++;
            break;

        default:
            break;
        }

        bndsVals[index * 2] = *startTime;
        bndsVals[index * 2 + 1] = *startTime + numDays;

        // time value = mid-time of bounds
        dimVarVals[index] =
            (bndsVals[index * 2] + bndsVals[index * 2 + 1]) / 2.0;

        *startTime += numDays;
    }

    SW_NC_write_vals(
        &dimVarID, ncFileID, NULL, dimVarVals, start, count, "double", LogInfo
    );
    free(dimVarVals);
    if (LogInfo->stopRun) {
        free(bndsVals);
        return; // Exit function prematurely due to error
    }

    count[1] = numBnds;

    SW_NC_write_vals(
        &bndsID, ncFileID, NULL, bndsVals, start, count, "double", LogInfo
    );

    free(bndsVals);
}

/**
@brief Helper function to `fill_dimVar()`; fully creates/fills
the variable "vertical_bnds" and fills the variable "vertical"

@param[in] ncFileID Identifier of the netCDF in which the information
    will be written
@param[in] dimIDs Dimension identifiers for "vertical" and "bnds"
@param[in] size Size of the vertical dimension/variable
@param[in] dimVarID "vertical" dimension identifier
@param[in] hasConsistentSoilLayerDepths Flag indicating if all simulation
    run within domain have identical soil layer depths
    (though potentially variable number of soil layers)
@param[in] lyrDepths Depths of soil layers (cm)
@param[in] deflateLevel Level of deflation that will be used for the created
variable
@param[out] LogInfo Holds information dealing with logfile output
*/
static void create_vert_vars(
    int ncFileID,
    int dimIDs[],
    unsigned int size,
    int dimVarID,
    Bool hasConsistentSoilLayerDepths,
    const double lyrDepths[],
    int deflateLevel,
    LOG_INFO *LogInfo
) {

    double *dimVarVals = NULL;
    double *bndVals = NULL;
    double lyrStart = 0.0;
    const int numBnds = 2;
    size_t start[] = {0, 0};
    size_t count[] = {(size_t) size, 0};
    int bndIndex = 0;

    SW_NC_create_netCDF_var(
        &bndIndex,
        "vertical_bnds",
        dimIDs,
        &ncFileID,
        NC_DOUBLE,
        numBnds,
        NULL,
        deflateLevel,
        LogInfo
    );
    if (LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

    dimVarVals = (double *) Mem_Malloc(
        size * sizeof(double), "create_vert_vars", LogInfo
    );
    if (LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

    bndVals = (double *) Mem_Malloc(
        (size_t) (size * numBnds) * sizeof(double), "create_vert_vars", LogInfo
    );
    if (LogInfo->stopRun) {
        free(dimVarVals);
        return; // Exit function prematurely due to error
    }

    for (size_t index = 0; index < (size_t) size; index++) {
        // if hasConsistentSoilLayerDepths,
        // then use soil layer depth, else soil layer number
        dimVarVals[index] = (hasConsistentSoilLayerDepths) ?
                                lyrDepths[index] :
                                (double) (index + 1);

        bndVals[index * 2] = lyrStart;
        bndVals[index * 2 + 1] = dimVarVals[index];

        lyrStart = bndVals[index * 2 + 1];
    }

    SW_NC_write_vals(
        &dimVarID, ncFileID, NULL, dimVarVals, start, count, "double", LogInfo
    );
    free(dimVarVals);
    if (LogInfo->stopRun) {
        free(bndVals);
        return; // Exit function prematurely due to error
    }

    count[1] = numBnds;

    SW_NC_write_vals(
        &bndIndex,
        ncFileID,
        "vertical_bnds",
        bndVals,
        start,
        count,
        "double",
        LogInfo
    );

    free(bndVals);
}

/**
@brief Helper function to `SW_NCOUT_create_output_dimVar()`; fills
the dimension variable plus the respective "*_bnds" variable
if needed

@param[in] ncFileID Identifier of the netCDF in which the information
    will be written
@param[in] dimIDs Identifiers of the dimensions the parent dimension has
    (i.e., "time", "vertical", and "pft") in case of the need when creating
    the "bnds" dimension for a "*_bnds" variable
@param[in] size Size of the dimension/original variable dimension
@param[in] varID Identifier of the new variable respectively named
    from the created dimension
@param[in] hasConsistentSoilLayerDepths Flag indicating if all simulation
    run within domain have identical soil layer depths
    (though potentially variable number of soil layers)
@param[in] lyrDepths Depths of soil layers (cm)
@param[in,out] startTime Start number of days when dealing with
    years between netCDF files
@param[in] startYr Start year of the simulation
@param[in] pd Current output netCDF period
@param[in] deflateLevel Level of deflation that will be used for the created
variable
@param[out] LogInfo Holds information dealing with logfile output
*/
static void fill_dimVar(
    int ncFileID,
    int dimIDs[],
    unsigned int size,
    int varID,
    Bool hasConsistentSoilLayerDepths,
    double lyrDepths[],
    double *startTime,
    int dimNum,
    unsigned int startYr,
    OutPeriod pd,
    int deflateLevel,
    LOG_INFO *LogInfo
) {

    const int vertInd = 0;
    const int timeInd = 1;
    const int pftInd = 2;
    const int numBnds = 2;

    if (dimNum == pftInd) {
        SW_NC_write_string_vals(ncFileID, varID, key2veg, LogInfo);
    } else {
        if (!SW_NC_dimExists("bnds", ncFileID)) {
            SW_NC_create_netCDF_dim(
                "bnds", numBnds, &ncFileID, &dimIDs[1], LogInfo
            );
        } else {
            SW_NC_get_dim_identifier(ncFileID, "bnds", &dimIDs[1], LogInfo);
        }
        if (LogInfo->stopRun) {
            return; // Exit function prematurely due to error
        }

        if (dimNum == vertInd) {
            if (!SW_NC_varExists(ncFileID, "vertical_bnds")) {

                create_vert_vars(
                    ncFileID,
                    dimIDs,
                    size,
                    varID,
                    hasConsistentSoilLayerDepths,
                    lyrDepths,
                    deflateLevel,
                    LogInfo
                );
            }
        } else if (dimNum == timeInd) {
            if (!SW_NC_varExists(ncFileID, "time_bnds")) {
                create_time_vars(
                    ncFileID,
                    dimIDs,
                    size,
                    varID,
                    startYr,
                    startTime,
                    pd,
                    deflateLevel,
                    LogInfo
                );
            }
        }
    }
}

/**
@brief Concentrate the important output variable attributes
into one location to write out

@param[in] varInfo Attribute information on the current variable
    that will help create the attributes
@param[in] key Specifies what output key is currently being allocated
    (i.e., temperature, precipitation, etc.)
@param[in] pd Current output netCDF period
@param[in] varNum Designated variable placement within the list of output
    variable information
@param[out] resAtts Resulting attributes to write out
@param[in] sumType Sum type of the output key
@param[out] LogInfo Holds information on warnings and errors
*/
static int gather_var_attributes(
    char **varInfo,
    OutKey key,
    OutPeriod pd,
    int varNum,
    char *resAtts[],
    OutSum sumType,
    LOG_INFO *LogInfo
) {
    int fillSize = 0;
    int varIndex;
    int resSNP;
    char cellRedef[MAX_FILENAMESIZE];
    char establOrginName[MAX_FILENAMESIZE];

    // Determine attribute 'original_name'
    if (key == eSW_Estab) {
        resSNP = snprintf(
            establOrginName,
            sizeof establOrginName,
            "%s__%s",
            SW_ESTAB,
            varInfo[VARNAME_INDEX]
        );

        if (resSNP < 0 || (unsigned) resSNP >= (sizeof establOrginName)) {
            LogError(
                LogInfo,
                LOGWARN,
                "attribute 'original_name' of variable '%s' was truncated.",
                varInfo[VARNAME_INDEX]
            );
        }

        resAtts[fillSize] = Str_Dup(establOrginName, LogInfo);
        if (LogInfo->stopRun) {
            return 0; // Exit function prematurely due to error
        }
    } else {
        resAtts[fillSize] = (char *) possKeys[key][varNum];
    }
    fillSize++;

    // Transfer the variable info into the result array (ignore the variable
    // name and dimensions)
    for (varIndex = LONGNAME_INDEX; varIndex <= CELLMETHOD_INDEX; varIndex++) {
        resAtts[fillSize] = varInfo[varIndex];
        fillSize++;
    }

    if (pd > eSW_Day) {
        resSNP = snprintf(
            cellRedef,
            sizeof cellRedef,
            "%s within days time: %s over days",
            resAtts[fillSize - 1],
            styp2longstr[sumType]
        );

        if (resSNP < 0 || (unsigned) resSNP >= (sizeof cellRedef)) {
            LogError(
                LogInfo,
                LOGWARN,
                "attribute 'cell_methods' of variable '%s' was truncated.",
                varInfo[VARNAME_INDEX]
            );
        }

        Str_ToLower(cellRedef, cellRedef);
        resAtts[fillSize - 1] = Str_Dup(cellRedef, LogInfo);
        if (LogInfo->stopRun) {
            return 0; // Exit function prematurely due to error
        }
    }

    if (key == eSW_Temp || key == eSW_SoilTemp) {
        resAtts[fillSize] = (char *) "temperature: on_scale";
        fillSize++;
    }

    return fillSize;
}

/**
@brief Collect the write dimensions/sizes for the current output slice

@param[in] domType Type of domain in which simulations are running
    (gridcell/sites)
@param[in] timeSize Number of time steps in current output slice
@param[in] nsl Number of soil layers
@param[in] npft Number of plant functional types
@param[out] count Array storing the output dimensions
@param[out] countTotal Total size (count) of output values
 */
static void get_vardim_write_counts(
    const char *domType,
    size_t timeSize,
    IntUS nsl,
    IntUS npft,
    size_t count[],
    size_t *countTotal
) {
    int dimIndex;
    int ndimsp;
    int nSpaceDims = (strcmp(domType, "s") == 0) ? 1 : 2;

    /* Fill 1s into space dimensions (we write one site/xy-gridcell per run) */
    /* We assume here that the first dimension(s) are space */
    for (dimIndex = 0; dimIndex < nSpaceDims; dimIndex++) {
        count[dimIndex] = 1;
    }

    *countTotal = 1;

    /* Fill in time dimension */
    if (timeSize > 0) {
        count[dimIndex] = timeSize;
        *countTotal *= timeSize;
        dimIndex++;
    }

    /* Fill in vertical (if present) */
    if (nsl > 0) {
        count[dimIndex] = nsl;
        *countTotal *= nsl;
        dimIndex++;
    }

    /* Fill in pft (if present) */
    if (npft > 0) {
        count[dimIndex] = npft;
        *countTotal *= npft;
        dimIndex++;
    }

    /* Zero remaining unused slots */
    ndimsp = dimIndex;
    for (dimIndex = ndimsp; dimIndex < MAX_NUM_DIMS; dimIndex++) {
        count[dimIndex] = 0;
    }
}

#if defined(SWDEBUG)
/**
@brief Check that count matches with existing variable in netCDF

@param[in] fileName Name of output netCDF file
@param[in] varName Name of output netCDF variable
@param[in] ncFileID Output netCDF file ID
@param[in] varID Output netCDF variable ID
@param[in] count Array with established the output dimensions
@param[out] LogInfo Holds information on warnings and errors
*/
static void check_counts_against_vardim(
    const char *fileName,
    const char *varName,
    int ncFileID,
    int varID,
    size_t count[],
    LOG_INFO *LogInfo
) {

    int dimIndex;
    int ndimsp;
    int nSpaceDims = SW_NC_dimExists("site", ncFileID) ? 1 : 2;
    int dimidsp[MAX_NUM_DIMS] = {0};
    char dimname[NC_MAX_NAME + 1];
    size_t ccheck[MAX_NUM_DIMS] = {0};


    /* Fill 1s into space dimensions (we write one site/xy-gridcell per run) */
    /* We assume here that the first dimension(s) are space */
    for (dimIndex = 0; dimIndex < nSpaceDims; dimIndex++) {
        ccheck[dimIndex] = 1;
    }

    /* Query number of dimensions of variable */
    if (nc_inq_varndims(ncFileID, varID, &ndimsp) != NC_NOERR) {
        LogError(
            LogInfo,
            LOGERROR,
            "%s / variable = %s: "
            "could not read number of dimensions.",
            fileName,
            varName
        );
        return; // Exit function prematurely due to error
    }

    if (ndimsp > MAX_NUM_DIMS) {
        LogError(
            LogInfo,
            LOGERROR,
            "%s / variable = %s: "
            "found n = %d dimensions (more than maximum of %d).",
            fileName,
            varName,
            ndimsp,
            MAX_NUM_DIMS
        );
        return; // Exit function prematurely due to error
    }

    /* Query dimension IDs associated with variable (skip space dimensions) */
    if (nc_inq_vardimid(ncFileID, varID, dimidsp) != NC_NOERR) {
        LogError(
            LogInfo,
            LOGERROR,
            "%s / variable = %s: "
            "could not read name of dimension identifiers.",
            fileName,
            varName
        );
        return; // Exit function prematurely due to error
    }


    /* Query size and name of other (non-space) dimensions */
    for (dimIndex = nSpaceDims; dimIndex < ndimsp; dimIndex++) {
        SW_NC_get_dimlen_from_dimid(
            ncFileID, dimidsp[dimIndex], &ccheck[dimIndex], LogInfo
        );
        if (LogInfo->stopRun) {
            return; // Exit function prematurely due to error
        }
    }

    /* Loop through dimensions and check that counts match */
    for (dimIndex = 0; dimIndex < ndimsp; dimIndex++) {
        if (count[dimIndex] != ccheck[dimIndex]) {

            if (nc_inq_dimname(ncFileID, dimidsp[dimIndex], dimname) !=
                NC_NOERR) {
                LogError(
                    LogInfo,
                    LOGERROR,
                    "%s / variable = %s: "
                    "could not read name of dimension %d.",
                    fileName,
                    varName,
                    dimidsp[dimIndex]
                );
                return; // Exit function prematurely due to error
            }

            LogError(
                LogInfo,
                LOGERROR,
                "%s / variable = %s: "
                "provided value (%d) does not match expected "
                "size of dimension '%s' (%d).",
                fileName,
                varName,
                dimname,
                count[dimIndex],
                ccheck[dimIndex]
            );
            return; // Exit function prematurely due to error
        }
    }
}
#endif // SWDEBUG

/**
@brief Create and fill a new output netCDF file

\p hasConsistentSoilLayerDepths determines if vertical dimension (soil depth)
is represented by
    - soil layer depths (if entire domain has the same soil layer profile)
    - soil layer number (if soil layer profile varies across domain)

@param[in] OutDom Struct of type SW_OUT_DOM that holds output
    information that do not change throughout simulation runs
@param[in] domFile Domain netCDF file name
@param[in] domType Type of domain in which simulations are running
    (gridcell/sites)
@param[in] newFileName Name of the new file that will be created
@param[in] key Specifies what output key is currently being allocated
    (i.e., temperature, precipitation, etc.)
@param[in] pd Current output netCDF period
@param[in] nVar Number of variables available for current output key
@param[in] nvar_OUT Number of output variables (array of length
SW_OUTNPERIODS).
@param[in] nsl Number of output soil layer per variable
    (array of size SW_OUTNMAXVARS).
@param[in] npft Number of output vegtypes per variable
    (array of size SW_OUTNMAXVARS).
@param[in] hasConsistentSoilLayerDepths Flag indicating if all simulation
    run within domain have identical soil layer depths
    (though potentially variable number of soil layers)
@param[in] lyrDepths Depths of soil layers (cm)
@param[in] originTimeSize Original "time" dimension size (that will
    not be overwritten in the function)
@param[in] startYr Start year of the simulation
@param[in] baseCalendarYear First year of the entire simulation
@param[in,out] startTime Start number of days when dealing with
    years between netCDF files (returns updated value)
@param[in] deflateLevel Level of deflation that will be used for the created
variable
@param[in] LogInfo Holds information on warnings and errors
*/
static void create_output_file(
    SW_OUT_DOM *OutDom,
    const char *domFile,
    const char *domType,
    const char *newFileName,
    OutKey key,
    OutPeriod pd,
    int nVar,
    IntUS nsl[],
    IntUS npft[],
    Bool hasConsistentSoilLayerDepths,
    double lyrDepths[],
    unsigned int originTimeSize,
    unsigned int startYr,
    int baseCalendarYear,
    double *startTime,
    int deflateLevel,
    LOG_INFO *LogInfo
) {

    int index;
    char frequency[10];
    const char *attNames[] = {
        "original_name",
        "long_name",
        "comment",
        "units",
        "cell_method",
        "units_metadata"
    };
    char *attVals[MAX_NATTS] = {NULL};
    OutSum sumType = OutDom->sumtype[key];

    int numAtts = 0;
    const int nameAtt = 0;

    int newFileID = -1; // Default to not created
    int cellMethAttInd = 0;
    char *varName;
    char **varInfo;

    (void) sw_memccpy(frequency, (char *) pd2longstr[pd], '\0', 9);
    Str_ToLower(frequency, frequency);


    // Create file
    if (!FileExists(newFileName)) {
        // Create a new output file
        SW_NC_create_template(
            domType,
            domFile,
            newFileName,
            &newFileID,
            swFALSE,
            frequency,
            LogInfo
        );
        if (LogInfo->stopRun) {
            return; // Exit function prematurely due to error
        }
    }


    // Add output variables
    for (index = 0; index < nVar; index++) {
        if (OutDom->netCDFOutput.reqOutputVars[key][index]) {
            varInfo = OutDom->netCDFOutput.outputVarInfo[key][index];
            varName =
                OutDom->netCDFOutput.outputVarInfo[key][index][VARNAME_INDEX];

            numAtts = gather_var_attributes(
                varInfo, key, pd, index, attVals, sumType, LogInfo
            );
            if (LogInfo->stopRun) {
                return; // Exit function prematurely due to error
            }

            SW_NC_create_full_var(
                &newFileID,
                domType,
                NC_DOUBLE,
                originTimeSize,
                nsl[index],
                npft[index],
                varName,
                attNames,
                (const char **) attVals,
                numAtts,
                hasConsistentSoilLayerDepths,
                lyrDepths,
                startTime,
                baseCalendarYear,
                startYr,
                pd,
                deflateLevel,
                LogInfo
            );

            if (pd > eSW_Day) {
                if (newFileID > -1) {
                    // new file was created
                    cellMethAttInd = (key == eSW_Temp || key == eSW_SoilTemp) ?
                                         numAtts - 2 :
                                         numAtts - 1;
                }
                if (!isnull(attVals[cellMethAttInd])) {
                    free(attVals[cellMethAttInd]);
                    attVals[cellMethAttInd] = NULL;
                }
            }
            if (key == eSW_Estab && !isnull(attVals[nameAtt])) {
                free(attVals[nameAtt]);
            }
            if (LogInfo->stopRun && FileExists(newFileName)) {
                nc_close(newFileID);
                return; // Exit function prematurely due to error
            }
        }
    }

    // Only close the file if it was created
    if (newFileID > -1) {
        nc_close(newFileID);
    }
}

/* =================================================== */
/*             Global Function Definitions             */
/* --------------------------------------------------- */

/**
@brief Create a "time", "vertical", or "pft" dimension variable and the
respective "*_bnds" variables (plus "bnds" dimension)
and fill the variable with the respective information

@param[in] name Name of the new dimension
@param[in] size Size of the new dimension
@param[in] ncFileID Identifier of the netCDF in which the information
    will be written
@param[in,out] dimID New dimenion identifier within the given netCDF
@param[in] hasConsistentSoilLayerDepths Flag indicating if all simulation
    run within domain have identical soil layer depths
    (though potentially variable number of soil layers)
@param[in] lyrDepths Depths of soil layers (cm)
@param[in,out] startTime Start number of days when dealing with
    years between netCDF files (returns updated value)
@param[in] baseCalendarYear First year of the entire simulation
@param[in] startYr Start year of the simulation
@param[in] pd Current output netCDF period
@param[in] deflateLevel Level of deflation that will be used for the created
variable
@param[out] LogInfo Holds information dealing with logfile output
*/
void SW_NCOUT_create_output_dimVar(
    char *name,
    unsigned int size,
    int ncFileID,
    int *dimID,
    Bool hasConsistentSoilLayerDepths,
    double lyrDepths[],
    double *startTime,
    unsigned int baseCalendarYear,
    unsigned int startYr,
    OutPeriod pd,
    int deflateLevel,
    LOG_INFO *LogInfo
) {

    char *dimNames[3] = {(char *) "vertical", (char *) "time", (char *) "pft"};
    const int vertIndex = 0;
    const int timeIndex = 1;
    const int pftIndex = 2;
    const int timeUnitIndex = 2;
    int dimNum;
    int varID;
    int index;
    int dimIDs[2] = {0, 0};
    int varType;
    double tempVal = 1.0;
    double *startFillTime;
    const int numDims = 1;

    const char *outAttNames[][6] = {
        {"long_name", "standard_name", "units", "positive", "axis", "bounds"},
        {"long_name", "standard_name", "units", "axis", "calendar", "bounds"},
        {"standard_name"}
    };

    char outAttVals[][6][MAX_FILENAMESIZE] = {
        {"soil depth", "depth", "centimeter", "down", "Z", "vertical_bnds"},
        {"time", "time", "", "T", "standard", "time_bnds"},
        {"biological_taxon_name"}
    };

    const int numVarAtts[] = {6, 6, 1};

    for (dimNum = 0; dimNum < 3; dimNum++) {
        if (Str_CompareI(dimNames[dimNum], name) == 0) {
            break;
        }
    }
    if (dimNum >= 3) {
        LogError(
            LogInfo,
            LOGERROR,
            "SW_NCOUT_create_output_dimVar() does not support requested "
            "dimension '%s'.",
            name
        );
        return; // Exit function prematurely due to error
    }

    varType = (dimNum == pftIndex) ? NC_STRING : NC_DOUBLE;

    startFillTime = (dimNum == timeIndex) ? startTime : &tempVal;

    SW_NC_create_netCDF_dim(name, size, &ncFileID, dimID, LogInfo);
    if (LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

    if (!SW_NC_varExists(ncFileID, name)) {
        dimIDs[0] = *dimID;

        SW_NC_create_netCDF_var(
            &varID,
            name,
            dimIDs,
            &ncFileID,
            varType,
            numDims,
            NULL,
            deflateLevel,
            LogInfo
        );
        if (LogInfo->stopRun) {
            return; // Exit function prematurely due to error
        }

        fill_dimVar(
            ncFileID,
            dimIDs,
            size,
            varID,
            hasConsistentSoilLayerDepths,
            lyrDepths,
            startFillTime,
            dimNum,
            startYr,
            pd,
            deflateLevel,
            LogInfo
        );

        if (dimNum == timeIndex) {
            (void) snprintf(
                outAttVals[timeIndex][timeUnitIndex],
                MAX_FILENAMESIZE,
                "days since %d-01-01 00:00:00",
                baseCalendarYear
            );
        }

        if (dimNum == vertIndex && !hasConsistentSoilLayerDepths) {
            // Use soil layers as dimension variable values
            // because soil layer depths are not consistent across domain
            (void) sw_memccpy(
                outAttVals[vertIndex][0], "soil layer", '\0', MAX_FILENAMESIZE
            );
            (void
            ) sw_memccpy(outAttVals[vertIndex][2], "1", '\0', MAX_FILENAMESIZE);
        }

        for (index = 0; index < numVarAtts[dimNum]; index++) {
            SW_NC_write_string_att(
                outAttNames[dimNum][index],
                outAttVals[dimNum][index],
                varID,
                ncFileID,
                LogInfo
            );
            if (LogInfo->stopRun) {
                return; // Exit function prematurely due to error
            }
        }
    }
}

/**
@brief Read the user-provided tsv file that contains information about
output variables in netCDFs

If a user turns off all variables of an outkey group, then
the entire outkey group is turned off.

Lack of information for a variable in the tsv file is equivalent to
turning off the output of that variable. For instance, an empty tsv file
results in no output produced.

This function requires previous calls to
    - SW_VES_read2() to set parms
    - SW_OUT_setup_output() to set GenOutput.nvar_OUT

@param[in,out] OutDom Struct of type SW_OUT_DOM that holds output
    information that do not change throughout simulation runs
@param[in] InFiles Array of program in/output files
@param[in] parms Array of type SW_VEGESTAB_INFO holding information about
    species
@param[out] LogInfo Holds information on warnings and errors
*/
void SW_NCOUT_read_out_vars(
    SW_OUT_DOM *OutDom,
    char *InFiles[],
    SW_VEGESTAB_INFO **parms,
    LOG_INFO *LogInfo
) {

    FILE *f;
    OutKey currOutKey;
    char inbuf[MAX_FILENAMESIZE];
    char *MyFileName;
    char varKey[MAX_FILENAMESIZE + 1];
    int varNum = 0;
    int lineno = 0;

    Bool estabFound = swFALSE;
    Bool used_OutKeys[SW_OUTNKEYS] = {swFALSE};
    int varNumUnits;
    int index;
    int estVar;
    int resSNP;
    char *copyStr = NULL;
    char input[NOUT_VAR_INPUTS][MAX_ATTVAL_SIZE] = {"\0"};
    char establn[MAX_ATTVAL_SIZE] = {"\0"};
    int scanRes = 0;
    int defToLocalInd = 0;
    // in readLineFormat: 255 must be equal to MAX_ATTVAL_SIZE - 1
    const char *readLineFormat =
        "%13[^\t]\t%50[^\t]\t%50[^\t]\t%10[^\t]\t%4[^\t]\t%1[^\t]\t"
        "%255[^\t]\t%255[^\t]\t%255[^\t]\t%255[^\t]\t%255[^\t]\t%255[^\t]";
    int doOutputVal;

    // Column indices
    const int keyInd = 0;
    const int SWVarNameInd = 1;
    const int SWTxtNameInd = 2;
    const int SWUnitsInd = 3;
    const int dimInd = 4;
    const int doOutInd = 5;
    const int outVarNameInd = 6;
    const int longNameInd = 7;
    const int commentInd = 8;
    const int outUnits = 9;
    const int cellMethodInd = 10;
    const int usercommentInd = 11;

    MyFileName = InFiles[eNCOutVars];
    f = OpenFile(MyFileName, "r", LogInfo);
    if (LogInfo->stopRun) {
        return; // Exit prematurely due to error
    }

    SW_NCOUT_alloc_output_var_info(OutDom, LogInfo);
    if (LogInfo->stopRun) {
        goto closeFile; // Exit prematurely due to error
    }

    GetALine(f, inbuf, MAX_FILENAMESIZE); // Ignore the first row (column names)

    while (GetALine(f, inbuf, MAX_FILENAMESIZE)) {
        lineno++;

        // Ignore additional columns
        scanRes = sscanf(
            inbuf,
            readLineFormat,
            input[keyInd],
            input[SWVarNameInd],
            input[SWTxtNameInd],
            input[SWUnitsInd],
            input[dimInd],
            input[doOutInd],
            input[outVarNameInd],
            input[longNameInd],
            input[commentInd],
            input[outUnits],
            input[cellMethodInd],
            input[usercommentInd]
        );

        if (scanRes != NOUT_VAR_INPUTS) {
            LogError(
                LogInfo,
                LOGERROR,
                "%s [row %d]: %d instead of %d columns found. "
                "Enter 'NA' if value should be blank "
                "(e.g., for 'long_name' or 'comment').",
                MyFileName,
                lineno + 1,
                scanRes,
                NOUT_VAR_INPUTS
            );
            goto closeFile; // Exit function prematurely due to error
        }

        // Check if the variable was requested to be output
        // Store attribute information for each variable (including names)

        doOutputVal = sw_strtoi(input[doOutInd], MyFileName, LogInfo);
        if (LogInfo->stopRun) {
            goto closeFile; // Exit function prematurely due to error
        }

        if (doOutputVal) {
            resSNP = snprintf(
                varKey,
                sizeof varKey,
                "%s__%s",
                input[keyInd],
                input[SWVarNameInd]
            );

            if (resSNP < 0 || (unsigned) resSNP >= (sizeof varKey)) {
                LogError(
                    LogInfo,
                    LOGERROR,
                    "nc-output variable name '%s' is too long.",
                    varKey
                );
                goto closeFile; // Exit function prematurely due to error
            }

            get_2d_output_key(varKey, &currOutKey, &varNum, OutDom->nvar_OUT);

            if (currOutKey == eSW_NoKey) {
                LogError(
                    LogInfo,
                    LOGWARN,
                    "%s [row %d]: Could not interpret the input combination "
                    "of key/variable: '%s'/'%s'.",
                    MyFileName,
                    lineno + 1,
                    input[keyInd],
                    input[SWVarNameInd]
                );
                continue;
            }

            if (!OutDom->use[currOutKey]) {
                // key not in use
                // don't output any of the variables within that outkey group
                continue;
            }

            // check SOILWAT2 (internal) units
            if (currOutKey == eSW_Estab) {
                // estab: one unit for every species' output
                varNumUnits = 0;
            } else {
                varNumUnits = varNum;
            }

            if (Str_CompareI(
                    (char *) SWVarUnits[currOutKey][varNumUnits],
                    input[SWUnitsInd]
                ) != 0) {
                LogError(
                    LogInfo,
                    LOGWARN,
                    "%s: Found an input variable (%s) with a 'SW2 units' "
                    "value that does not match the unit SOILWAT2 uses. "
                    "This will be ignored, and '%s' will be used.",
                    MyFileName,
                    input[SWVarNameInd],
                    SWVarUnits[currOutKey][varNumUnits]
                );
            }

            // track if any variable is requested
            used_OutKeys[currOutKey] = swTRUE;

            if (currOutKey == eSW_Estab) {
                // Handle establishment different since it is "dynamic"
                // (SW_VEGESTAB'S "count")
                if (estabFound) {
                    LogError(
                        LogInfo,
                        LOGWARN,
                        "%s: Found more than one row for "
                        "the key ESTABL, only one is expected, "
                        "ignoring...",
                        MyFileName
                    );
                    continue;
                }

                estabFound = swTRUE;
                varNum = 0;

                if (OutDom->nvar_OUT[currOutKey] == 0) {
                    // outsetup.in and nc-out request ESTAB but no taxon
                    // available
                    continue;
                }
            }

            OutDom->netCDFOutput.reqOutputVars[currOutKey][varNum] = swTRUE;

            // Read in the rest of the attributes
            // Output variable name, long name, comment, units, and cell_method
            for (index = VARNAME_INDEX; index <= CELLMETHOD_INDEX; index++) {
                defToLocalInd = index + doOutInd;

                if (strcmp(input[defToLocalInd], "NA") == 0) {
                    copyStr = (char *) "";
                } else {
                    copyStr = input[defToLocalInd];
                }
                if (strlen(copyStr) >= MAX_ATTVAL_SIZE - 1) {
                    LogError(
                        LogInfo,
                        LOGWARN,
                        "%s [row %d, field %d]: maximum string length reached "
                        "or exceeded (%d); content may be truncated: '%s'",
                        MyFileName,
                        lineno + 1,
                        defToLocalInd + 1,
                        MAX_ATTVAL_SIZE - 1,
                        copyStr
                    );
                }

                // Handle ESTAB differently by storing all attributes
                // into `count` amount of variables and give the
                // correct <sppname>'s
                if (currOutKey == eSW_Estab) {
                    for (estVar = 0; estVar < OutDom->nvar_OUT[currOutKey];
                         estVar++) {

                        switch (index) {
                        case VARNAME_INDEX:
                            OutDom->netCDFOutput
                                .reqOutputVars[currOutKey][estVar] = swTRUE;
                            OutDom->netCDFOutput
                                .outputVarInfo[currOutKey][estVar][index] =
                                Str_Dup(parms[estVar]->sppname, LogInfo);
                            break;

                        case LONGNAME_INDEX:
                            (void) sw_memccpy(
                                establn, copyStr, '\0', MAX_ATTVAL_SIZE
                            );
                            OutDom->netCDFOutput
                                .outputVarInfo[currOutKey][estVar][index] =
                                Str_Dup(establn, LogInfo);
                            break;

                        default:
                            OutDom->netCDFOutput
                                .outputVarInfo[currOutKey][estVar][index] =
                                Str_Dup(copyStr, LogInfo);
                            break;
                        }

                        if (LogInfo->stopRun) {
                            /* Exit function prematurely due to error */
                            goto closeFile;
                        }
                    }
                } else {
                    OutDom->netCDFOutput
                        .outputVarInfo[currOutKey][varNum][index] =
                        Str_Dup(copyStr, LogInfo);
                    if (LogInfo->stopRun) {
                        /* Exit function prematurely due to error */
                        goto closeFile;
                    }
                }
            }


            // Copy SW units for later use
            if (currOutKey == eSW_Estab) {
                for (estVar = 0; estVar < OutDom->nvar_OUT[currOutKey];
                     estVar++) {
                    OutDom->netCDFOutput.units_sw[currOutKey][estVar] =
                        Str_Dup(SWVarUnits[currOutKey][varNumUnits], LogInfo);
                    if (LogInfo->stopRun) {
                        /* Exit function prematurely due to error */
                        goto closeFile;
                    }
                }
            } else {
                OutDom->netCDFOutput.units_sw[currOutKey][varNum] =
                    Str_Dup(SWVarUnits[currOutKey][varNumUnits], LogInfo);
                if (LogInfo->stopRun) {
                    goto closeFile; // Exit function prematurely due to error
                }
            }
        }
    }


    // Update "use": turn off if no variable of an outkey group is requested
    ForEachOutKey(index) {
        if (!used_OutKeys[index]) {
            OutDom->use[index] = swFALSE;
        }
    }

closeFile: { CloseFile(&f, LogInfo); }
}

/**
@brief Initializes pointers only pertaining to netCDF output information

@param[in,out] SW_netCDFOut Constant netCDF output file information
*/
void SW_NCOUT_init_ptrs(SW_NETCDF_OUT *SW_netCDFOut) {

    int index;
    const int numAllocVars = 13;
    char **allocArr[] = {
        &SW_netCDFOut->title,
        &SW_netCDFOut->author,
        &SW_netCDFOut->institution,
        &SW_netCDFOut->comment,
        &SW_netCDFOut->coordinate_system,

        &SW_netCDFOut->crs_geogsc.long_name,
        &SW_netCDFOut->crs_geogsc.grid_mapping_name,
        &SW_netCDFOut->crs_geogsc
             .crs_wkt, // geogsc does not use datum and units

        &SW_netCDFOut->crs_projsc.long_name,
        &SW_netCDFOut->crs_projsc.grid_mapping_name,
        &SW_netCDFOut->crs_projsc.crs_wkt,
        &SW_netCDFOut->crs_projsc.datum,
        &SW_netCDFOut->crs_projsc.units
    };

    SW_netCDFOut->crs_projsc.standard_parallel[0] = NAN;
    SW_netCDFOut->crs_projsc.standard_parallel[1] = NAN;

    SW_netCDFOut->strideOutYears = -1;
    SW_netCDFOut->deflateLevel = 0;

    for (index = 0; index < numAllocVars; index++) {
        *allocArr[index] = NULL;
    }

#if defined(SWNETCDF)
    int key;

    ForEachOutKey(key) {
        SW_netCDFOut->outputVarInfo[key] = NULL;
        SW_netCDFOut->reqOutputVars[key] = NULL;
        SW_netCDFOut->units_sw[key] = NULL;
        SW_netCDFOut->uconv[key] = NULL;
    }
#endif

    (void) allocArr; // Silence compiler
}

/**
@brief Wrapper function to allocate output request variables
and output variable information

@param[out] OutDom Struct of type SW_OUT_DOM that holds output
    information that do not change throughout simulation runs
@param[out] LogInfo Holds information on warnings and errors
*/
void SW_NCOUT_alloc_output_var_info(SW_OUT_DOM *OutDom, LOG_INFO *LogInfo) {
    int key;

    ForEachOutKey(key) {
        SW_NCOUT_alloc_outputkey_var_info(OutDom, key, LogInfo);
        if (LogInfo->stopRun) {
            return; // Exit function prematurely due to error
        }
    }
}

void SW_NCOUT_alloc_outputkey_var_info(
    SW_OUT_DOM *OutDom, int key, LOG_INFO *LogInfo
) {

    SW_NETCDF_OUT *netCDFOutput = &OutDom->netCDFOutput;

    SW_NC_alloc_req(
        &netCDFOutput->reqOutputVars[key], OutDom->nvar_OUT[key], LogInfo
    );
    if (LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

    SW_NC_alloc_vars(
        &netCDFOutput->outputVarInfo[key],
        OutDom->nvar_OUT[key],
        NUM_OUTPUT_INFO,
        LogInfo
    );
    if (LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

    SW_NC_alloc_unitssw(
        &netCDFOutput->units_sw[key], OutDom->nvar_OUT[key], LogInfo
    );
    if (LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

    SW_NC_alloc_uconv(
        &netCDFOutput->uconv[key], OutDom->nvar_OUT[key], LogInfo
    );
    if (LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }
}

void SW_NCOUT_dealloc_outputkey_var_info(SW_OUT_DOM *OutDom, IntUS k) {
    if (!isnull(OutDom->netCDFOutput.outputVarInfo[k])) {

        for (int varNum = 0; varNum < OutDom->nvar_OUT[k]; varNum++) {

            if (!isnull(OutDom->netCDFOutput.outputVarInfo[k][varNum])) {

                for (int attNum = 0; attNum < NUM_OUTPUT_INFO; attNum++) {

                    if (!isnull(OutDom->netCDFOutput
                                    .outputVarInfo[k][varNum][attNum])) {
                        free(OutDom->netCDFOutput
                                 .outputVarInfo[k][varNum][attNum]);
                        OutDom->netCDFOutput.outputVarInfo[k][varNum][attNum] =
                            NULL;
                    }
                }

                free((void *) OutDom->netCDFOutput.outputVarInfo[k][varNum]);
                OutDom->netCDFOutput.outputVarInfo[k][varNum] = NULL;
            }
        }

        free((void *) OutDom->netCDFOutput.outputVarInfo[k]);
        OutDom->netCDFOutput.outputVarInfo[k] = NULL;
    }

    if (!isnull(OutDom->netCDFOutput.units_sw[k])) {
        for (int varNum = 0; varNum < OutDom->nvar_OUT[k]; varNum++) {
            if (!isnull(OutDom->netCDFOutput.units_sw[k][varNum])) {
                free(OutDom->netCDFOutput.units_sw[k][varNum]);
                OutDom->netCDFOutput.units_sw[k][varNum] = NULL;
            }
        }

        free((void *) OutDom->netCDFOutput.units_sw[k]);
        OutDom->netCDFOutput.units_sw[k] = NULL;
    }

    if (!isnull(OutDom->netCDFOutput.uconv[k])) {
        for (int varNum = 0; varNum < OutDom->nvar_OUT[k]; varNum++) {
            if (!isnull(OutDom->netCDFOutput.uconv[k][varNum])) {
#if defined(SWNETCDF) && defined(SWUDUNITS)
                cv_free(OutDom->netCDFOutput.uconv[k][varNum]);
#else
                free(OutDom->netCDFOutput.uconv[k][varNum]);
#endif
                OutDom->netCDFOutput.uconv[k][varNum] = NULL;
            }
        }

        free((void *) OutDom->netCDFOutput.uconv[k]);
        OutDom->netCDFOutput.uconv[k] = NULL;
    }

    if (!isnull(OutDom->netCDFOutput.reqOutputVars[k])) {
        free(OutDom->netCDFOutput.reqOutputVars[k]);
        OutDom->netCDFOutput.reqOutputVars[k] = NULL;
    }
}

/**
@brief Allocate memory for files within SW_PATH_OUTPUTS for future
functions to write to/create

@param[out] ncOutFiles Output file names storage array
@param[in] numFiles Number of file names to store/allocate memory for
@param[out] LogInfo Holds information on warnings and errors
*/
void SW_NCOUT_alloc_files(
    char ***ncOutFiles, unsigned int numFiles, LOG_INFO *LogInfo
) {

    unsigned int varNum;

    *ncOutFiles = (char **) Mem_Malloc(
        numFiles * sizeof(char *), "SW_NC_create_output_files()", LogInfo
    );
    if (LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

    for (varNum = 0; varNum < numFiles; varNum++) {
        (*ncOutFiles)[varNum] = NULL;
    }
}

/**
@brief Generate all requested netCDF output files that will be written to
instead of CSVs

\p hasConsistentSoilLayerDepths determines if vertical dimension (soil depth)
is represented by
    - soil layer depths (if entire domain has the same soil layer profile)
    - soil layer number (if soil layer profile varies across domain)

@param[in] domFile Name of the domain netCDF
@param[in] domType Type of domain in which simulations are running
    (gridcell/sites)
@param[in] output_prefix Directory path of output files.
@param[in] SW_Domain Struct of type SW_DOMAIN holding constant
    temporal/spatial information for a set of simulation runs
@param[in] timeSteps Requested time steps
@param[in] used_OUTNPERIODS Determine which output periods to output
@param[in] nvar_OUT Number of output variables (array of length
SW_OUTNPERIODS).
@param[in] nsl_OUT Number of output soil layer per variable
    (array of size SW_OUTNKEYS by SW_OUTNMAXVARS).
@param[in] npft_OUT Number of output vegtypes per variable
    (array of size SW_OUTNKEYS by SW_OUTNMAXVARS).
@param[in] hasConsistentSoilLayerDepths Flag indicating if all simulation
    run within domain have identical soil layer depths
    (though potentially variable number of soil layers)
@param[in] lyrDepths Depths of soil layers (cm)
@param[in] strideOutYears Number of years to write into an output file
@param[in] startYr Start year of the simulation
@param[in] endYr End year of the simulation
@param[in] baseCalendarYear First year of the entire simulation
@param[out] numFilesPerKey Number of output netCDFs each output key will
    have (same amount for each key)
@param[out] ncOutFileNames A list of the generated output netCDF file names
@param[out] LogInfo Holds information on warnings and errors
*/
void SW_NCOUT_create_output_files(
    const char *domFile,
    const char *domType,
    const char *output_prefix,
    SW_DOMAIN *SW_Domain,
    OutPeriod timeSteps[][SW_OUTNPERIODS],
    IntUS used_OUTNPERIODS,
    IntUS nvar_OUT[],
    IntUS nsl_OUT[][SW_OUTNMAXVARS],
    IntUS npft_OUT[][SW_OUTNMAXVARS],
    Bool hasConsistentSoilLayerDepths,
    double lyrDepths[],
    int strideOutYears,
    unsigned int startYr,
    unsigned int endYr,
    int baseCalendarYear,
    unsigned int *numFilesPerKey,
    char **ncOutFileNames[][SW_OUTNPERIODS],
    LOG_INFO *LogInfo
) {

    int key;
    int ip;
    int resSNP;
    OutPeriod pd;
    unsigned int rangeStart;
    unsigned int rangeEnd;
    unsigned int fileNum;

    unsigned int numYears = endYr - startYr + 1;
    unsigned int yearOffset;
    char fileNameBuf[MAX_FILENAMESIZE];
    char yearBuff[10]; // 10 - hold up to YYYY-YYYY
    unsigned int timeSize = 0;
    unsigned int baseTime = 0;
    double startTime[SW_OUTNPERIODS];

    char periodSuffix[10];
    char *yearFormat;

    *numFilesPerKey =
        (strideOutYears == -1) ?
            1 :
            (unsigned int) ceil((double) numYears / strideOutYears);

    yearOffset =
        (strideOutYears == -1) ? numYears : (unsigned int) strideOutYears;

    yearFormat = (strideOutYears == 1) ? (char *) "%d" : (char *) "%d-%d";

    ForEachOutKey(key) {
        if (nvar_OUT[key] > 0 && SW_Domain->OutDom.use[key]) {

            // Loop over requested output periods (which may vary for each
            // outkey)
            for (ip = 0; ip < used_OUTNPERIODS; ip++) {
                pd = timeSteps[key][ip];

                if (pd != eSW_NoTime) {
                    startTime[pd] = 0;
                    baseTime = times[pd];
                    rangeStart = startYr;

                    (void) sw_memccpy(
                        periodSuffix, (char *) pd2longstr[pd], '\0', 9
                    );
                    Str_ToLower(periodSuffix, periodSuffix);

                    SW_NCOUT_alloc_files(
                        &ncOutFileNames[key][pd], *numFilesPerKey, LogInfo
                    );
                    if (LogInfo->stopRun) {
                        return; // Exit prematurely due to error
                    }
                    for (fileNum = 0; fileNum < *numFilesPerKey; fileNum++) {
                        if (rangeStart + yearOffset > endYr) {
                            rangeEnd = rangeStart + (endYr - rangeStart) + 1;
                        } else {
                            rangeEnd = rangeStart + yearOffset;
                        }

                        (void) snprintf(
                            yearBuff, 10, yearFormat, rangeStart, rangeEnd - 1
                        );
                        resSNP = snprintf(
                            fileNameBuf,
                            sizeof fileNameBuf,
                            "%s%s_%s_%s.nc",
                            output_prefix,
                            key2str[key],
                            yearBuff,
                            periodSuffix
                        );

                        if (resSNP < 0 ||
                            (unsigned) resSNP >= (sizeof fileNameBuf)) {
                            LogError(
                                LogInfo,
                                LOGERROR,
                                "nc-output file name '%s' is too long.",
                                fileNameBuf
                            );
                            return; // Exit function prematurely due to error
                        }

                        ncOutFileNames[key][pd][fileNum] =
                            Str_Dup(fileNameBuf, LogInfo);
                        if (LogInfo->stopRun) {
                            return; // Exit function prematurely due to error
                        }

                        if (FileExists(fileNameBuf)) {
                            SW_NC_check(SW_Domain, -1, fileNameBuf, LogInfo);
                            if (LogInfo->stopRun) {
                                return; // Exit function prematurely due to
                                        // error
                            }

                        } else {
                            timeSize = calc_timeSize(
                                rangeStart, rangeEnd, baseTime, pd
                            );

                            create_output_file(
                                &SW_Domain->OutDom,
                                domFile,
                                domType,
                                fileNameBuf,
                                (OutKey) key,
                                pd,
                                nvar_OUT[key],
                                nsl_OUT[key],
                                npft_OUT[key],
                                hasConsistentSoilLayerDepths,
                                lyrDepths,
                                timeSize,
                                rangeStart,
                                baseCalendarYear,
                                &startTime[pd],
                                SW_Domain->OutDom.netCDFOutput.deflateLevel,
                                LogInfo
                            );
                            if (LogInfo->stopRun) {
                                return; // Exit function prematurely due to
                                        // error
                            }
                        }

                        rangeStart = rangeEnd;
                    }
                }
            }
        }
    }
}

/** Create unit converters for output variables

This function requires previous calls to
    - SW_NCOUT_alloc_output_var_info() to initialize
      SW_Output[key].uconv[varIndex] to NULL
    - SW_NCOUT_read_out_vars() to obtain user requested output units
    - SW_OUT_setup_output() to set GenOutput.nvar_OUT for argument nVars

@param[in,out] OutDom Struct of type SW_OUT_DOM that holds output
    information that do not change throughout simulation runs
@param[out] LogInfo Holds information on warnings and errors
*/
void SW_NCOUT_create_units_converters(SW_OUT_DOM *OutDom, LOG_INFO *LogInfo) {
    int varIndex;
    int key;

    SW_NETCDF_OUT *netCDFOutput = &OutDom->netCDFOutput;

#if defined(SWUDUNITS)
    ut_system *system;
    ut_unit *unitFrom;
    ut_unit *unitTo;

    /* silence udunits2 error messages */
    ut_set_error_message_handler(ut_ignore);

    /* Load unit system database */
    system = ut_read_xml(NULL);
#endif

    ForEachOutKey(key) {
        if (!OutDom->use[key]) {
            continue; // Skip key iteration
        }

        for (varIndex = 0; varIndex < OutDom->nvar_OUT[key]; varIndex++) {
            if (!netCDFOutput->reqOutputVars[key][varIndex]) {
                continue; // Skip variable iteration
            }

#if defined(SWUDUNITS)
            if (!isnull(netCDFOutput->units_sw[key][varIndex])) {
                unitFrom = ut_parse(
                    system, netCDFOutput->units_sw[key][varIndex], UT_UTF8
                );
                unitTo = ut_parse(
                    system,
                    OutDom->netCDFOutput
                        .outputVarInfo[key][varIndex][UNITS_INDEX],
                    UT_UTF8
                );

                if (ut_are_convertible(unitFrom, unitTo)) {
                    // netCDFOutput.uconv[key][varIndex] was previously
                    // to NULL initialized
                    netCDFOutput->uconv[key][varIndex] =
                        ut_get_converter(unitFrom, unitTo);
                }

                if (isnull(netCDFOutput->uconv[key][varIndex])) {
                    // ut_are_convertible() is false or ut_get_converter()
                    // failed
                    LogError(
                        LogInfo,
                        LOGWARN,
                        "Units of variable '%s' cannot get converted from "
                        "internal '%s' to requested '%s'. "
                        "Output will use internal units.",
                        netCDFOutput
                            ->outputVarInfo[key][varIndex][VARNAME_INDEX],
                        netCDFOutput->units_sw[key][varIndex],
                        netCDFOutput->outputVarInfo[key][varIndex][UNITS_INDEX]
                    );

                    /* converter is not available: output in internal units */
                    free(netCDFOutput->outputVarInfo[key][varIndex][UNITS_INDEX]
                    );
                    netCDFOutput->outputVarInfo[key][varIndex][UNITS_INDEX] =
                        Str_Dup(netCDFOutput->units_sw[key][varIndex], LogInfo);
                }

                ut_free(unitFrom);
                ut_free(unitTo);
            }

#else
            /* udunits2 is not available: output in internal units */
            free(netCDFOutput->outputVarInfo[key][varIndex][UNITS_INDEX]);
            if (!isnull(netCDFOutput->units_sw[key][varIndex])) {
                netCDFOutput->outputVarInfo[key][varIndex][UNITS_INDEX] =
                    Str_Dup(netCDFOutput->units_sw[key][varIndex], LogInfo);
            }
#endif

            if (LogInfo->stopRun) {
                return; // Exit function prematurely due to error
            }
        }
    }


#if defined(SWUDUNITS)
    ut_free_system(system);
#endif
}

/**
@brief Write values to output variables in previously-created
output netCDF files

@param[in] OutDom Struct of type SW_OUT_DOM that holds output
    information that do not change throughout simulation runs
@param[in] p_OUT Array of accumulated output values throughout
    simulation years
@param[in] numFilesPerKey Number of output netCDFs each output key will
    have (same amount for each key)
@param[in] ncOutFileNames A list of the generated output netCDF file names
@param[in] ncSuid Unique indentifier of the current suid being simulated
@param[in] domType Type of domain in which simulations are running
    (gridcell/sites)
@param[out] LogInfo Holds information on warnings and errors
*/
void SW_NCOUT_write_output(
    SW_OUT_DOM *OutDom,
    double *p_OUT[][SW_OUTNPERIODS],
    unsigned int numFilesPerKey,
    char **ncOutFileNames[][SW_OUTNPERIODS],
    const size_t ncSuid[],
    const char *domType,
    LOG_INFO *LogInfo
) {

    int key;
    OutPeriod pd;
    double *p_OUTValPtr = NULL;
    unsigned int fileNum;
    int currFileID = 0;
    int varNum;
    int varID = -1;

    char *fileName;
    char *varName;
    size_t count[MAX_NUM_DIMS] = {0};
    size_t start[MAX_NUM_DIMS] = {0};
    size_t pOUTIndex;
    size_t startTime;
    size_t timeSize = 0;
    size_t countTotal = 0;
    int vertSize;
    int pftSize;

    start[0] = ncSuid[0];
    start[1] = ncSuid[1];


    ForEachOutPeriod(pd) {
        if (!OutDom->use_OutPeriod[pd]) {
            continue; // Skip period iteration
        }

        ForEachOutKey(key) {
            if (OutDom->nvar_OUT[key] == 0 || !OutDom->use[key]) {
                continue; // Skip key iteration
            }

            // Loop over output time-slices

            // keep track of time across time-sliced files per outkey
            startTime = 0;

            for (fileNum = 0; fileNum < numFilesPerKey; fileNum++) {
                fileName = ncOutFileNames[key][pd][fileNum];

                if (isnull(fileName)) {
                    // this outperiod x outkey combination was not requested
                    continue;
                }

                if (nc_open(fileName, NC_WRITE, &currFileID) != NC_NOERR) {
                    LogError(
                        LogInfo,
                        LOGERROR,
                        "Could not open output file %s.",
                        fileName
                    );
                    return; // Exit function prematurely due to error
                }

                // Get size of the "time" dimension
                SW_NC_get_dimlen_from_dimname(
                    currFileID, "time", &timeSize, LogInfo
                );
                if (LogInfo->stopRun) {
                    goto closeFile; // Exit function prematurely due to error
                }


                for (varNum = 0; varNum < OutDom->nvar_OUT[key]; varNum++) {
                    if (!OutDom->netCDFOutput.reqOutputVars[key][varNum]) {
                        continue; // Skip variable iteration
                    }

                    varName = OutDom->netCDFOutput
                                  .outputVarInfo[key][varNum][VARNAME_INDEX];

                    // Locate correct slice in netCDF to write to
                    SW_NC_get_var_identifier(
                        currFileID, varName, &varID, LogInfo
                    );
                    if (LogInfo->stopRun) {
                        /* Exit function prematurely due to error */
                        goto closeFile;
                    }

                    get_vardim_write_counts(
                        domType,
                        timeSize,
                        OutDom->nsl_OUT[key][varNum],
                        OutDom->npft_OUT[key][varNum],
                        count,
                        &countTotal
                    );

#if defined(SWDEBUG)
                    check_counts_against_vardim(
                        fileName, varName, currFileID, varID, count, LogInfo
                    );
                    if (LogInfo->stopRun) {
                        /* Exit function prematurely due to error*/
                        goto closeFile;
                    }
#endif // SWDEBUG


                    /* Point to contiguous memory where values change fastest
                       for vegtypes, then soil layers, then time, then variables
                    */
                    pOUTIndex =
                        OutDom->netCDFOutput.iOUToffset[key][pd][varNum];
                    if (startTime > 0) {
                        // 1 if no soil layers
                        vertSize = (OutDom->nsl_OUT[key][varNum] > 0) ?
                                       OutDom->nsl_OUT[key][varNum] :
                                       1;
                        // 1 if no vegtypes
                        pftSize = (OutDom->npft_OUT[key][varNum] > 0) ?
                                      OutDom->npft_OUT[key][varNum] :
                                      1;

                        pOUTIndex += iOUTnc(startTime, 0, 0, vertSize, pftSize);
                    }

                    p_OUTValPtr = &p_OUT[key][pd][pOUTIndex];


/* Convert units if udunits2 and if converter available */
#if defined(SWUDUNITS)
                    if (!isnull(OutDom->netCDFOutput.uconv[key][varNum])) {
                        cv_convert_doubles(
                            OutDom->netCDFOutput.uconv[key][varNum],
                            p_OUTValPtr,
                            countTotal,
                            p_OUTValPtr
                        );
                    }
#endif

                    /* For current variable x output period,
                       write out all values across vegtypes and soil layers (if
                       any) for current time-chunk
                    */
                    SW_NC_write_vals(
                        &varID,
                        currFileID,
                        NULL,
                        p_OUTValPtr,
                        start,
                        count,
                        "double",
                        LogInfo
                    );
                    if (LogInfo->stopRun) {
                        goto closeFile; // Exit function prematurely due to
                                        // error
                    }
                }

                // Update startTime
                startTime += timeSize;

                nc_close(currFileID);
            }
        }
    }

closeFile: { nc_close(currFileID); }
}

/**
@brief Deconstruct output netCDF information

@param[in,out] SW_netCDFOut Constant netCDF output file information
*/
void SW_NCOUT_deconstruct(SW_NETCDF_OUT *SW_netCDFOut) {

    int index;
    const int numFreeVars = 13;
    char *freeArr[] = {
        SW_netCDFOut->title,
        SW_netCDFOut->author,
        SW_netCDFOut->institution,
        SW_netCDFOut->comment,
        SW_netCDFOut->coordinate_system,

        SW_netCDFOut->crs_geogsc.long_name,
        SW_netCDFOut->crs_geogsc.grid_mapping_name,
        SW_netCDFOut->crs_geogsc.crs_wkt, // geogsc does not use datum and units

        SW_netCDFOut->crs_projsc.long_name,
        SW_netCDFOut->crs_projsc.grid_mapping_name,
        SW_netCDFOut->crs_projsc.crs_wkt,
        SW_netCDFOut->crs_projsc.datum,
        SW_netCDFOut->crs_projsc.units
    };

    for (index = 0; index < numFreeVars; index++) {
        if (!isnull(freeArr[index])) {
            free(freeArr[index]);
            freeArr[index] = NULL;
        }
    }
}

/**
@brief Deep copy a source instance of output netCDF information

@param[in] source_output Source output netCDF information to copy
@param[out] dest_output Destination output netCDF information to be copied
into from it's source counterpart
@param[out] LogInfo Holds information on warnings and errors
*/
void SW_NCOUT_deepCopy(
    SW_NETCDF_OUT *source_output, SW_NETCDF_OUT *dest_output, LOG_INFO *LogInfo
) {

    int index;
    int numIndivCopy = 5;

    char *srcStrs[] = {
        source_output->title,
        source_output->author,
        source_output->institution,
        source_output->comment,
        source_output->coordinate_system
    };

    char **destStrs[] = {
        &dest_output->title,
        &dest_output->author,
        &dest_output->institution,
        &dest_output->comment,
        &dest_output->coordinate_system
    };

    for (index = 0; index < numIndivCopy; index++) {
        *destStrs[index] = Str_Dup(srcStrs[index], LogInfo);
        if (LogInfo->stopRun) {
            return; // Exit function prematurely due to
        }
    }
}

/**
@brief Read invariant netCDF information (attributes/CRS) from input file

@param[in,out] SW_netCDFOut Constant netCDF output file information
@param[in,out] SW_PathInputs Struct holding all information about the programs
    path/files
@param[out] LogInfo Holds information on warnings and errors
*/
void SW_NCOUT_read_atts(
    SW_NETCDF_OUT *SW_netCDFOut,
    SW_PATH_INPUTS *SW_PathInputs,
    LOG_INFO *LogInfo
) {

    static const char *possibleKeys[NUM_ATT_IN_KEYS] = {
        "title",
        "author",
        "institution",
        "comment",
        "coordinate_system",
        "primary_crs",

        "geo_long_name",
        "geo_grid_mapping_name",
        "geo_crs_wkt",
        "geo_longitude_of_prime_meridian",
        "geo_semi_major_axis",
        "geo_inverse_flattening",

        "proj_long_name",
        "proj_grid_mapping_name",
        "proj_crs_wkt",
        "proj_longitude_of_prime_meridian",
        "proj_semi_major_axis",
        "proj_inverse_flattening",
        "proj_datum",
        "proj_units",
        "proj_standard_parallel",
        "proj_longitude_of_central_meridian",
        "proj_latitude_of_projection_origin",
        "proj_false_easting",
        "proj_false_northing",

        "strideOutYears",
        "baseCalendarYear",
        "deflateLevel"
    };
    static const Bool requiredKeys[NUM_ATT_IN_KEYS] = {
        swTRUE,  swTRUE,  swTRUE,  swFALSE, swFALSE, swTRUE,  swTRUE,
        swTRUE,  swTRUE,  swTRUE,  swTRUE,  swTRUE,  swFALSE, swFALSE,
        swFALSE, swFALSE, swFALSE, swFALSE, swFALSE, swFALSE, swFALSE,
        swFALSE, swFALSE, swFALSE, swFALSE, swFALSE, swTRUE
    };
    Bool hasKeys[NUM_ATT_IN_KEYS] = {swFALSE};

    FILE *f;
    char inbuf[LARGE_VALUE];
    char value[LARGE_VALUE];
    char key[35]; // 35 - Max key size
    char *MyFileName;
    int keyID;
    int n;
    int scanRes;
    double num1 = 0;
    double num2 = 0;
    Bool geoCRSFound = swFALSE;
    Bool projCRSFound = swFALSE;
    Bool infVal = swFALSE;

    double inBufdoubleRes = 0.;
    int inBufintRes = 0;
    char numOneStr[20];
    char numTwoStr[20];

    Bool doIntConv;
    Bool doDoubleConv;

    MyFileName = SW_PathInputs->InFiles[eNCInAtt];
    f = OpenFile(MyFileName, "r", LogInfo);
    if (LogInfo->stopRun) {
        LogError(
            LogInfo,
            LOGERROR,
            "Could not open the required file %s",
            SW_PathInputs->InFiles[eNCInAtt]
        );
        return; // Exit function prematurely due to error
    }

    while (GetALine(f, inbuf, LARGE_VALUE)) {

        scanRes = sscanf(inbuf, "%34s %s", key, value);

        if (scanRes < 2) {
            LogError(
                LogInfo,
                LOGERROR,
                "Not enough values for a valid key-value pair in %s.",
                MyFileName
            );
            goto closeFile;
        }

        // Check if the key is "long_name" or "crs_wkt"
        if (strstr(key, "long_name") != NULL ||
            strstr(key, "crs_wkt") != NULL) {

            // Reread the like and get the entire value (includes spaces)
            scanRes = sscanf(inbuf, "%34s %[^\n]", key, value);
            if (scanRes < 2) {
                LogError(
                    LogInfo,
                    LOGERROR,
                    "Not enough values for a valid key-value pair in %s.",
                    MyFileName
                );
                goto closeFile;
            }
        }

        keyID = key_to_id(key, possibleKeys, NUM_ATT_IN_KEYS);
        set_hasKey(keyID, possibleKeys, hasKeys, LogInfo);
        // set_hasKey() does not produce errors, only warnings possible

        /* Check to see if the line number contains a double or integer value */
        doIntConv = (Bool) (keyID >= 23 && keyID <= 27);
        doDoubleConv = (Bool) ((keyID >= 9 && keyID <= 11) ||
                               (keyID >= 15 && keyID <= 17) ||
                               (keyID >= 21 && keyID <= 22));

        if (doIntConv || doDoubleConv) {
            if (doIntConv) {
                infVal = (Bool) (Str_CompareI(value, (char *) "Inf") == 0);

                if (!infVal) {
                    inBufintRes = sw_strtoi(value, MyFileName, LogInfo);
                }
            } else {
                inBufdoubleRes = sw_strtod(value, MyFileName, LogInfo);
            }

            if (LogInfo->stopRun) {
                goto closeFile;
            }
        }

        switch (keyID) {
        case 0:
            SW_netCDFOut->title = Str_Dup(value, LogInfo);
            break;
        case 1:
            SW_netCDFOut->author = Str_Dup(value, LogInfo);
            break;
        case 2:
            SW_netCDFOut->institution = Str_Dup(value, LogInfo);
            break;
        case 3:
            SW_netCDFOut->comment = Str_Dup(value, LogInfo);
            break;
        case 4: // coordinate_system is calculated
            break;

        case 5:
            if (strcmp(value, "geographic") == 0) {
                SW_netCDFOut->primary_crs_is_geographic = swTRUE;
            } else if (strcmp(value, "projected") == 0) {
                SW_netCDFOut->primary_crs_is_geographic = swFALSE;
            } else {
                LogError(
                    LogInfo,
                    LOGERROR,
                    "The read-in primary CRS "
                    "(%s) is not a valid one. Please choose between "
                    "geographic and projected.",
                    value
                );
                goto closeFile;
            }
            break;
        case 6:
            SW_netCDFOut->crs_geogsc.long_name = Str_Dup(value, LogInfo);
            geoCRSFound = swTRUE;
            break;
        case 7:
            SW_netCDFOut->crs_geogsc.grid_mapping_name =
                Str_Dup(value, LogInfo);
            break;
        case 8:
            SW_netCDFOut->crs_geogsc.crs_wkt = Str_Dup(value, LogInfo);
            break;
        case 9:
            SW_netCDFOut->crs_geogsc.longitude_of_prime_meridian =
                inBufdoubleRes;
            break;
        case 10:
            SW_netCDFOut->crs_geogsc.semi_major_axis = inBufdoubleRes;
            break;
        case 11:
            SW_netCDFOut->crs_geogsc.inverse_flattening = inBufdoubleRes;
            break;
        case 12:
            SW_netCDFOut->crs_projsc.long_name = Str_Dup(value, LogInfo);
            projCRSFound = swTRUE;
            break;
        case 13:
            SW_netCDFOut->crs_projsc.grid_mapping_name =
                Str_Dup(value, LogInfo);
            break;
        case 14:
            SW_netCDFOut->crs_projsc.crs_wkt = Str_Dup(value, LogInfo);
            break;
        case 15:
            SW_netCDFOut->crs_projsc.longitude_of_prime_meridian =
                inBufdoubleRes;
            break;
        case 16:
            SW_netCDFOut->crs_projsc.semi_major_axis = inBufdoubleRes;
            break;
        case 17:
            SW_netCDFOut->crs_projsc.inverse_flattening = inBufdoubleRes;
            break;
        case 18:
            SW_netCDFOut->crs_projsc.datum = Str_Dup(value, LogInfo);
            break;
        case 19:
            SW_netCDFOut->crs_projsc.units = Str_Dup(value, LogInfo);
            break;
        case 20:
            // Re-scan for 1 or 2 values of standard parallel(s)
            // the user may separate values by white-space, comma, etc.
            n = sscanf(
                inbuf,
                "%34s %19s%*[^-.0123456789]%19s",
                key,
                numOneStr,
                numTwoStr
            );

            if (n < 2) {
                LogError(
                    LogInfo,
                    LOGERROR,
                    "Not enough values to read in for the standard parallel(s)."
                );
                goto closeFile;
            }

            num1 = sw_strtod(numOneStr, MyFileName, LogInfo);
            if (LogInfo->stopRun) {
                goto closeFile;
            }

            num2 = sw_strtod(numTwoStr, MyFileName, LogInfo);
            if (LogInfo->stopRun) {
                goto closeFile;
            }

            SW_netCDFOut->crs_projsc.standard_parallel[0] = num1;
            SW_netCDFOut->crs_projsc.standard_parallel[1] =
                (n == 3) ? num2 : NAN;
            break;
        case 21:
            SW_netCDFOut->crs_projsc.longitude_of_central_meridian =
                inBufdoubleRes;
            break;
        case 22:
            SW_netCDFOut->crs_projsc.latitude_of_projection_origin =
                inBufdoubleRes;
            break;
        case 23:
            SW_netCDFOut->crs_projsc.false_easting = inBufintRes;
            break;
        case 24:
            SW_netCDFOut->crs_projsc.false_northing = inBufintRes;
            break;
        case 25:
            if (!infVal) {
                SW_netCDFOut->strideOutYears = inBufintRes;

                if (SW_netCDFOut->strideOutYears <= 0) {
                    LogError(
                        LogInfo, LOGERROR, "The value for 'strideOutYears' <= 0"
                    );
                    goto closeFile;
                }
            }
            break;
        case 26:
            SW_netCDFOut->baseCalendarYear = inBufintRes;
            break;
        case 27:
            SW_netCDFOut->deflateLevel = inBufintRes;
            break;
        case KEY_NOT_FOUND:
        default:
            LogError(
                LogInfo,
                LOGWARN,
                "Ignoring unknown key in %s - %s",
                MyFileName,
                key
            );
            break;
        }

        if (LogInfo->stopRun) {
            goto closeFile;
        }
    }


    // Check if all required input was provided
    check_requiredKeys(
        hasKeys, requiredKeys, possibleKeys, NUM_ATT_IN_KEYS, LogInfo
    );
    if (LogInfo->stopRun) {
        goto closeFile;
    }


    if ((SW_netCDFOut->primary_crs_is_geographic && !geoCRSFound) ||
        (!SW_netCDFOut->primary_crs_is_geographic && !projCRSFound)) {
        LogError(
            LogInfo,
            LOGERROR,
            "'%s': type of primary CRS is '%s' but "
            "attributes (including '*_long_name') for such a CRS are missing.",
            SW_PathInputs->InFiles[eNCInAtt],
            (SW_netCDFOut->primary_crs_is_geographic) ? "geographic" :
                                                        "projected"
        );
        goto closeFile;
    }

    if (projCRSFound && !geoCRSFound) {
        LogError(
            LogInfo,
            LOGERROR,
            "Program found a projected CRS "
            "in %s but not a geographic CRS. "
            "SOILWAT2 requires either a primary CRS of "
            "type 'geographic' CRS or a primary CRS of "
            "'projected' with a geographic CRS.",
            SW_PathInputs->InFiles[eNCInAtt]
        );
        goto closeFile;
    }

    SW_netCDFOut->coordinate_system =
        (SW_netCDFOut->primary_crs_is_geographic) ?
            Str_Dup(SW_netCDFOut->crs_geogsc.long_name, LogInfo) :
            Str_Dup(SW_netCDFOut->crs_projsc.long_name, LogInfo);

closeFile: { CloseFile(&f, LogInfo); }
}
