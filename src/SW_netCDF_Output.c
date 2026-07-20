/* =================================================== */
/*                INCLUDES / DEFINES                   */
/* --------------------------------------------------- */
#include "include/SW_netCDF_Output.h"   // for SW_NCOUT_read_out_vars, ...
#include "include/filefuncs.h"          // for LogError, FileExists, sw_strtod
#include "include/generic.h"            // for swFALSE, swTRUE, Bool, isnull
#include "include/myMemory.h"           // for Str_Dup, Mem_Malloc, sw_memc...
#include "include/SW_datastructs.h"     // for LOG_INFO, eSW_Estab, SW_NETC...
#include "include/SW_Defines.h"         // for MAX_FILENAMESIZE, OutPeriod
#include "include/SW_Files.h"           // for eNCInAtt, eNCOutVars
#include "include/SW_netCDF_General.h"  // for SW_NC_write_vals, SW_NC_crea...
#include "include/SW_Output.h"          // for ForEachOutKey, SW_ESTAB, pd2...
#include "include/SW_Output_outarray.h" // for iOUTnc
#include "include/SW_VegProd.h"         // for key2veg
#include "include/Times.h"              // for isleapyear, Time_get_lastdoy_y
#include <math.h>                       // for NAN, ceil, isnan
#include <netcdf.h>                     // for NC_NOERR, nc_close, NC_DOUBLE
#include <stdio.h>                      // for size_t, NULL, snprintf, sscanf
#include <stdlib.h>                     // for free, strtod
#include <string.h>                     // for strcmp, strlen, strstr, memcpy

#if defined(SWUDUNITS)
#include <udunits2.h> // for cv_free, cv_convert_double
#endif

#if defined(SWMPI)
#include "include/SW_MPI.h"
#include <netcdf_par.h> // for NC_NOERR, nc_close, NC_DOUBLE
#endif


/* =================================================== */
/*                   Local Defines                     */
/* --------------------------------------------------- */

/** Number of columns in 'Input_nc/SW2_netCDF_output_variables.tsv' */
#define NOUT_VAR_INPUTS 15

#define MAX_ATTVAL_SIZE 256

/** Maximum number of characters in a PFT name */
#define MAX_PFT_NAME_LENGTH 8

// Indices to second dimension of `outputVarInfo[varIndex][attIndex]`
#define DIM_INDEX 0 // unused
#define VARNAME_INDEX 1
#define LONGNAME_INDEX 2
#define COMMENT_INDEX 3
#define UNITS_INDEX 4
#define OUTPUT_TYPE 5
#define SCALE_FACTOR 6
#define ADD_OFFSET 7
#define CELLMETHOD_INDEX 8

/** Relative position of coordinate values at left boundary of cells */
#define COORDS_AT_LEFTBOUND (-1)

/** Relative position of coordinate values at midpoint of cells */
#define COORDS_AT_MIDPOINT 0

/** Relative position of coordinate values at right boundary of cells */
#define COORDS_AT_RIGHTBOUND 1

const unsigned int outTimes[] = {MAX_DAYS - 1, MAX_WEEKS, MAX_MONTHS, 1};

static const char *const expectedColNames[] = {
    "SW2 output group",
    "SW2 variable",
    "SW2 txt output",
    "SW2 units",
    "XY+Dim",
    "Do output?",
    "netCDF variable name",
    "netCDF long_name",
    "netCDF comment",
    "netCDF units",
    "Output type",
    "Scale factor",
    "Add offset",
    "netCDF cell_method",
    "User comment"
};

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
    {"1", "1", "g m-2", "g m-2", "g m-2", "g m-2", "g m-2", "m m-2"},

    /* DERIVEDSUM */
    {"cm", "degC day", "degC day"},

    /* DERIVEDAVG */
    {"cm", "cm"},

    /* ENERGYAVG */
    {"1"}
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
     "BIOMASS__LAI"},

    {"DERIVEDSUM__cwd",
     "DERIVEDSUM__ddd5C30bar000to100cm",
     "DERIVEDSUM__wdd5C15bar000to100cm"},

    {"DERIVEDAVG__swa30bar000to100cm", "DERIVEDAVG__swa39bar000to100cm"},

    {"ENERGYAVG__surfaceAlbedo"}
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
@brief Write pft labels (SOILWAT2 vegetation types)

@param[in] ncFileID Identifier of the open netCDF file to write the attribute
@param[in] varID Variable identifier within the given netCDF
@param[out] LogInfo Holds information on warnings and errors
*/
static void write_pft_labels(int ncFileID, int varID, LOG_INFO *LogInfo) {
    size_t start[] = {0, 0};
    size_t count[] = {NVEGTYPES, MAX_PFT_NAME_LENGTH};

    char filePath[MAX_FILENAMESIZE] = "\0";
    char *fileName = (char *) "\0";
    char varName[MAX_LOG_SIZE] = "\0";

    char *pftLabels = (char *) Mem_Calloc(
        (size_t) (NVEGTYPES * MAX_PFT_NAME_LENGTH),
        sizeof(char),
        "write_pft_labels",
        LogInfo
    );
    if (LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

    for (int k = 0; k < NVEGTYPES; k++) {
        (void) snprintf(
            &pftLabels[(size_t) (k * MAX_PFT_NAME_LENGTH)],
            MAX_PFT_NAME_LENGTH,
            "%s",
            key2veg[k]
        );
    }

    if (nc_put_vara_text(ncFileID, varID, start, count, pftLabels) !=
        NC_NOERR) {

        SW_NC_get_nc_filename_for_msg(ncFileID, &fileName, filePath, LogInfo);
        if (LogInfo->stopRun) {
            goto freeMem;
        }

        SW_NC_get_nc_varname_for_msg(ncFileID, varID, varName, LogInfo);
        if (LogInfo->stopRun) {
            goto freeMem;
        }

        LogError(
            LogInfo,
            LOGERROR,
            "Could not write pft labels (Variable: %s | File: %s).",
            varName,
            fileName
        );
    }

freeMem:
    free(pftLabels);
}

/**
@brief Calculate the number of days within a given time size

@param[in] timeSize Number of time steps in current output slice
@param[in] pd Current output netCDF period
@param[in] startYr Start year of the simulation
@param[in] posTimeInBnds Position of time coordinate values relative to bounds
@param[out] bndsVals Start/end bounds for "time" variable; can be NULL
    if we are calculating the number of days from the base calendar year
@param[out] dimVarVals Values of the "time" dimension; can be NULL
    if we are calculating the number of days from the base calendar year
@param[in,out] startTime Start number of days when dealing with
    years between netCDF files
*/
static void calc_num_timedays(
    size_t timeSize,
    OutPeriod pd,
    unsigned int startYr,
    int posTimeInBnds,
    double *bndsVals,
    double *dimVarVals,
    double *startTime
) {
    unsigned int month = 0;
    unsigned int week = 0;
    unsigned int numDays = 0;
    unsigned int currYear = startYr;

    for (size_t index = 0; index < timeSize; index++) {
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

            currYear += ((index + 1) % MAX_WEEKS == 0) ? 1 : 0;
            week = (week + 1) % MAX_WEEKS;
            break;

        case eSW_Month:
            if (month == Feb) {
                numDays = isleapyear(currYear) ? 29 : 28;
            } else {
                numDays = monthdays[month];
            }

            currYear += ((index + 1) % MAX_MONTHS == 0) ? 1 : 0;
            month = (month + 1) % MAX_MONTHS;
            break;

        default: // eSW_Year
            numDays = Time_get_lastdoy_y(currYear);
            currYear++;
            break;
        }

        if (!isnull(bndsVals)) {
            bndsVals[index * 2] = *startTime;
            bndsVals[index * 2 + 1] = *startTime + numDays;
        }

        if (!isnull(dimVarVals)) {
            switch (posTimeInBnds) {

            case COORDS_AT_LEFTBOUND:
                /* time value at start of bound */
                dimVarVals[index] = bndsVals[index * 2];
                break;

            case COORDS_AT_RIGHTBOUND:
                /* time value at end of bound */
                dimVarVals[index] = bndsVals[index * 2 + 1];
                break;

            default:
                /* COORDS_AT_MIDPOINT: time value at midpoint of bounds */
                dimVarVals[index] =
                    (bndsVals[index * 2] + bndsVals[index * 2 + 1]) / 2.0;
                break;
            }
        }

        *startTime += (double) numDays;
    }
}

/**
@brief Helper function to `fill_dimVar()`; fully creates/fills
the variable "time_bnds" and fills the variable "time"

@param[in] ncFileID Identifier of the netCDF in which the information
    will be written
@param[in] dimIDs Dimension identifiers for "vertical" and "bnds"
@param[in] size Size of the vertical dimension/variable
@param[in] dimVarID "time" dimension identifier
@param[in] posTimeInBnds Position of time coordinate values relative to bounds
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
    int posTimeInBnds,
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

    calc_num_timedays(
        (size_t) size,
        pd,
        startYr,
        posTimeInBnds,
        bndsVals,
        dimVarVals,
        startTime
    );

    SW_NC_write_vals(
        &dimVarID, ncFileID, NULL, dimVarVals, start, count, LogInfo
    );
    free(dimVarVals);
    if (LogInfo->stopRun) {
        free(bndsVals);
        return; // Exit function prematurely due to error
    }

    count[1] = numBnds;

    SW_NC_write_vals(&bndsID, ncFileID, NULL, bndsVals, start, count, LogInfo);

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
@param[in] posVerticalInBnds Position of vertical coordinate values
    relative to bounds
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
    int posVerticalInBnds,
    const double lyrDepths[],
    int deflateLevel,
    LOG_INFO *LogInfo
) {

    double *dimVarVals = NULL;
    double *bndsVals = NULL;
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

    bndsVals = (double *) Mem_Malloc(
        (size_t) (size * numBnds) * sizeof(double), "create_vert_vars", LogInfo
    );
    if (LogInfo->stopRun) {
        free(dimVarVals);
        return; // Exit function prematurely due to error
    }

    for (size_t index = 0; index < (size_t) size; index++) {

        bndsVals[index * 2] = lyrStart;

        // if hasConsistentSoilLayerDepths,
        // then use soil layer depth, else soil layer number
        bndsVals[index * 2 + 1] = (hasConsistentSoilLayerDepths) ?
                                      lyrDepths[index] :
                                      (double) (index + 1);

        lyrStart = bndsVals[index * 2 + 1];

        switch (posVerticalInBnds) {

        case COORDS_AT_LEFTBOUND:
            /* vertical value at shallow/top of bound */
            dimVarVals[index] = bndsVals[index * 2];
            break;

        case COORDS_AT_MIDPOINT:
            /* vertical value at midpoint of bounds */
            dimVarVals[index] =
                (bndsVals[index * 2] + bndsVals[index * 2 + 1]) / 2.0;
            break;

        default:
            /* COORDS_AT_RIGHTBOUND: vertical value at deep/bottom of bound */
            dimVarVals[index] = bndsVals[index * 2 + 1];
            break;
        }
    }

    SW_NC_write_vals(
        &dimVarID, ncFileID, NULL, dimVarVals, start, count, LogInfo
    );
    free(dimVarVals);
    if (LogInfo->stopRun) {
        free(bndsVals);
        return; // Exit function prematurely due to error
    }

    count[1] = numBnds;

    SW_NC_write_vals(
        &bndIndex, ncFileID, "vertical_bnds", bndsVals, start, count, LogInfo
    );

    free(bndsVals);
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
@param[in] posVerticalInBnds Position of vertical coordinate values
    relative to bounds
@param[in] lyrDepths Depths of soil layers (cm)
@param[in,out] startTime Start number of days when dealing with
    years between netCDF files
@param[in] posTimeInBnds Position of time coordinate values relative to bounds
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
    int posVerticalInBnds,
    double lyrDepths[],
    double *startTime,
    int dimNum,
    int posTimeInBnds,
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
        write_pft_labels(ncFileID, varID, LogInfo);
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
                    posVerticalInBnds,
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
                    posTimeInBnds,
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
@param[in] readinYName User-provided geographical y-axis name
@param[in] readinXName User-provided geographical x-axis name
@param[out] LogInfo Holds information on warnings and errors
*/
static int gather_var_attributes(
    char **varInfo,
    OutKey key,
    OutPeriod pd,
    int varNum,
    char *resAtts[],
    OutSum sumType,
    const char *readinYName,
    const char *readinXName,
    LOG_INFO *LogInfo
) {
    int fillSize = 0;
    int varIndex;
    int resSNP = 0;
    char cellRedef[MAX_FILENAMESIZE];
    char establOrginName[MAX_FILENAMESIZE];
    char coordsAtt[MAX_FILENAMESIZE];

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
        if (varIndex < OUTPUT_TYPE || varIndex > ADD_OFFSET) {
            resAtts[fillSize] = varInfo[varIndex];
            fillSize++;
        }
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

    /* Fill coordinates attribute */
    resSNP = snprintf(
        coordsAtt, MAX_FILENAMESIZE, "%s %s", readinYName, readinXName
    );
    if (resSNP < 0 || (unsigned) resSNP >= (sizeof coordsAtt)) {
        LogError(
            LogInfo,
            LOGWARN,
            "attribute 'coordinates' of variable '%s' was truncated.",
            varInfo[VARNAME_INDEX]
        );
    }

    resAtts[fillSize] = Str_Dup(coordsAtt, LogInfo);
    if (LogInfo->stopRun) {
        return 0; // Exit function prematurely due to error
    }
    fillSize++;

    if (key == eSW_Temp || key == eSW_SoilTemp) {
        resAtts[fillSize] = (char *) "temperature: on_scale";
        fillSize++;
    }

    return fillSize;
}

/**
@brief Get the time size for every output file but the last, then
    get the last time size; files [0, num out files - 1] have repeating
    time sizes

@param[in] openOutFileIDs A list of size [numFiles] that contains the
    file ID for each output file in question
@param[in] numFiles Number of output files being created per output key
@param[out] outKeyTimes An array of size "numFiles" to hold the time sizes
    for every output file for a specific output period
@param[out] LogInfo Holds information on warnings and errors
*/
static void store_time_sizes(
    const int openOutFileIDs[],
    unsigned int numFiles,
    size_t **outKeyTimes,
    LOG_INFO *LogInfo
) {
    int fileID;
    unsigned int file;

    for (file = 0; file < numFiles; file++) {
        fileID = openOutFileIDs[file];

        if (fileID > -1) {
            SW_NC_get_dimlen_from_dimname(
                fileID, "time", &((*outKeyTimes)[file]), LogInfo
            );
            if (LogInfo->stopRun) {
                return;
            }
        }
    }
}

/**
@brief Calculate the start file and number of files for each output period
to know which files to write out to

@param[in] numOutFiles Number of output files for each output key/period
@param[in] startFiles Output file to start writing in for each output period
@param[in] pdOutTimeSizes Holds "numOutFiles" output file running time size sum
to know which output file to write to for a set time size
@param[in] periodIndices Array of size SW_OUTNPERIODS holding the starting
indices for each output period
@param[in] nrow_OUT Number of output rows for each output period
@param[out] numFiles Number of output files to write to in order to write
all outputs for this output call for every output period
@param[out] newStartIndices A list of size SW_OUTNPERIODS specifying the
expected index within the final write for a period within a single output
cycle

@return Flag specifying if the current output period is already written out
*/
static Bool get_num_out_files(
    IntU numOutFiles,
    const IntU startFile,
    const size_t pdOutTimeSizes[],
    const size_t periodIndex,
    const size_t nrow_OUT,
    IntU *numFiles,
    size_t *newStartIndices
) {
    IntU file;

    size_t totFileSizes;
    size_t timeSize;

    file = startFile;

    *numFiles = 1;

    if (file == numOutFiles) {
        return swTRUE;
    }

    totFileSizes = pdOutTimeSizes[file] - periodIndex;
    while (nrow_OUT > totFileSizes && file < numOutFiles - 1) {
        (*numFiles)++;
        file++;

        timeSize = pdOutTimeSizes[file];
        totFileSizes += timeSize;
    }

    if (*numFiles == 1) {
        *newStartIndices = periodIndex + nrow_OUT;
    } else {
        *newStartIndices = pdOutTimeSizes[file] - (totFileSizes - nrow_OUT);
    }
    *newStartIndices %= pdOutTimeSizes[file];

    return swFALSE;
}

/**
@brief Collect the write & start dimensions/sizes for the current output slice

@param[in] isSimDomDiscrete Is simulation domain discrete (site-based)?
    Otherwise, the simulation domain is gridded.
@param[in] spatialCounts An array of size NC_DIMS holding the sizes of
each possible spatial dimension (site or latitude & longitude)
@param[in] spatialStarts An array of size NC_DIMS holding the starting
indices of the program's subdomain (eSW_InDomain)
@param[in] startTime Starting time index
@param[in] timeSize Number of time steps in current output slice
@param[in] nsl Number of soil layers
@param[in] npft Number of plant functional types
@param[out] count Array storing the output dimensions
@param[out] start Array storing the starting indices of the subdomain
to writeout
@param[out] countTotal Total size (count) of output values
 */
static void get_vardim_write_start_counts(
    Bool isSimDomDiscrete,
    const size_t spatialCounts[],
    const size_t spatialStarts[],
    const size_t startTime,
    size_t timeSize,
    IntUS nsl,
    IntUS npft,
    size_t count[],
    size_t start[],
    size_t *countTotal
) {
    const int maxNonSpatDims = 3;

    int dimIndex = 0;
    int loopDim;
    size_t countSizes[] = {timeSize, nsl, npft};
    size_t size;

    /* Zero all slots in start/count */
    memset(count, 0, sizeof(size_t) * MAX_NUM_DIMS);
    memset(start, 0, sizeof(size_t) * MAX_NUM_DIMS);

    *countTotal = 1;

    start[dimIndex] = startTime;
    for (loopDim = 0; loopDim < maxNonSpatDims; loopDim++) {
        size = countSizes[loopDim];

        if (size > 0) {
            count[dimIndex] = size;
            *countTotal *= size;
            dimIndex++;
        }
    }

    /*
       - Fill spatial indices with sizes within `count` (we write
            entire subdomain per run)
       - Fill starting spatial indices within `start` (conceptually
            the upper- and left-most site is the starting indices)
       - We assume here that the last dimension(s) are space
    */
    count[dimIndex] = spatialCounts[0];
    count[dimIndex + 1] = (isSimDomDiscrete) ? 0 : spatialCounts[1];

    start[dimIndex] = spatialStarts[0];
    start[dimIndex + 1] = (isSimDomDiscrete) ? 0 : spatialStarts[1];
}

/**
@brief Check that the dimensions within output netCDF files
match with expected program-known sizes

@param[in] fileName Name of output netCDF file
@param[in] varName Name of output netCDF variable
@param[in] ncFileID Output netCDF file ID
@param[in] varID Output netCDF variable ID
@param[in] timeSize Size of the expected time dimension
@param[in] pftSize Size of the expected vegetation types
@param[in] lyrSize Size of the expected layer size
@param[out] LogInfo Holds information on warnings and errors
*/
static void check_counts_against_vardim(
    const char *fileName,
    const char *varName,
    int ncFileID,
    int varID,
    size_t timeSize,
    size_t pftSize,
    size_t lyrSize,
    LOG_INFO *LogInfo
) {
    const int nTestDims = 3; // Ignore spatial dimensions
    const size_t possSizes[] = {timeSize, lyrSize, pftSize};

    int possSizeIdx = 0;
    int dimIndex = 0;
    int ndimsp;
    int dimidsp[MAX_NUM_DIMS] = {0};
    char dimname[NC_MAX_NAME + 1];
    size_t ccheckSize = 0;

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
    }
    checkReturn(LogInfo->stopRun);

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
    }
    checkReturn(LogInfo->stopRun);

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
    }
    checkReturn(LogInfo->stopRun);

    /* Query sizes of all non-space dimensions and check that counts match */
    for (possSizeIdx = 0; possSizeIdx < nTestDims; possSizeIdx++) {
        if (possSizes[possSizeIdx] > 0) {
            SW_NC_get_dimlen_from_dimid(
                ncFileID, dimidsp[dimIndex], &ccheckSize, LogInfo
            );
            checkReturn(LogInfo->stopRun);

            if (possSizes[possSizeIdx] != ccheckSize) {
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
                }
                checkReturn(LogInfo->stopRun);

                LogError(
                    LogInfo,
                    LOGERROR,
                    "%s / variable = %s: "
                    "provided value (%d) does not match expected "
                    "size of dimension '%s' (%d).",
                    fileName,
                    varName,
                    possSizes[possSizeIdx],
                    dimname,
                    ccheckSize
                );
            }
            checkReturn(LogInfo->stopRun);

            dimIndex++;
        }
    }
}

/**
@brief Wrapper function to check within netCDF output files that all
variable dimensions are as expected

@param[in] SW_Domain Struct of type SW_DOMAIN holding constant
temporal/spatial information for a set of simulation runs
@param[in] SW_PathOutputs Struct of type SW_PATH_OUTPUTS which
holds basic information about output files and values
@param[in] baseTime Base number of output periods in a year
    (e.g., 60 months in 5 years, or 731 days in 1980-1981)
@param[in] yearOffset Increment of output file years
@param[in] outKey Target output key to test the file(s) of
@param[in] outPd Target output period to test the file(s) of
@param[in] numDaysInMonth Number of days in each month of the last
year of the simulation
@param[in] cumDaysInMonth Running sum of total days at the end of
each month of the last year of simulation
@param[out] LogInfo Holds information on warnings and errors
*/
static void check_output_file_vars(
    SW_DOMAIN *SW_Domain,
    SW_PATH_OUTPUTS *SW_PathOutputs,
    unsigned int baseTime,
    unsigned int yearOffset,
    int outKey,
    int outPd,
    TimeInt numDaysInMonth[],
    TimeInt cumDaysInMonth[],
    LOG_INFO *LogInfo
) {
    SW_OUT_DOM *OutDom = &SW_Domain->OutDom;

    const unsigned int endyr = SW_Domain->endyr;
    const unsigned int lastFile = SW_PathOutputs->numOutFiles;

    char ***varInfo;

    unsigned int rangeStart = SW_Domain->startyr;
    unsigned int rangeEnd;

    unsigned int file;
    int var;

    size_t expectedTimeSize;

    for (var = 0; var < OutDom->nvar_OUT[outKey]; var++) {
        for (file = 0; file < lastFile; file++) {
            rangeEnd = rangeStart + yearOffset;
            rangeEnd = (rangeEnd > endyr) ? endyr + 1 : rangeEnd;

            expectedTimeSize = SW_NCOUT_calc_timeSize(
                SW_Domain,
                rangeStart,
                rangeEnd,
                baseTime,
                outPd,
                numDaysInMonth,
                cumDaysInMonth
            );

            for (var = 0; var < OutDom->nvar_OUT[outKey]; var++) {
                if (OutDom->netCDFOutput.reqOutputVars[outKey][var]) {
                    varInfo = OutDom->netCDFOutput.outputVarInfo[outKey];

                    check_counts_against_vardim(
                        SW_PathOutputs->ncOutFiles[outKey][outPd][file],
                        varInfo[var][VARNAME_INDEX],
                        SW_PathOutputs->openOutFileIDs[outKey][outPd][file],
                        SW_PathOutputs->ncOutVarIDs[outKey][var],
                        expectedTimeSize,
                        SW_Domain->OutDom.npft_OUT[outKey][var],
                        SW_Domain->OutDom.nsl_OUT[outKey][var],
                        LogInfo
                    );
                    checkReturn(LogInfo->stopRun);
                }
            }

            rangeStart = rangeEnd;
        }
    }
}

/**
@brief Get the identifiers of variables within output files

@param[in] outputVarInfo A list of a key's output variable information that
    will be used to get the variable name
@param[in] numVars Number of variables created within an output key
@param[in] outFileIDs List of all netCDF file identifiers for a current output
key
@param[in] numOutFiles Number of output files
@param[out] ncOutVarIDs A list of size SW_OUTNKEYS holding lists of output
    variable IDs
@param[out] LogInfo Holds information on warnings and errors
*/
static void get_outvar_ids(
    char ***outputVarInfo,
    IntUS numVars,
    int outFileIDs[],
    unsigned int numOutFiles,
    int *ncOutVarIDs,
    LOG_INFO *LogInfo
) {
    const int firstOutFile = 0;
    char *varName;
    int var;

#if defined(SWMPI)
    unsigned int file;
    int varID;
    int fileID;
#endif

    for (var = 0; var < numVars && outFileIDs[firstOutFile] > -1; var++) {
        varName = outputVarInfo[var][VARNAME_INDEX];

        SW_NC_get_var_identifier(
            outFileIDs[firstOutFile], varName, &ncOutVarIDs[var], LogInfo
        );
        if (LogInfo->stopRun) {
            return;
        }

#if defined(SWMPI)
        for (file = 0; file < numOutFiles; file++) {
            varID = ncOutVarIDs[var];
            fileID = outFileIDs[file];

            if (varID > -1) {
                SW_NC_toggle_par_access(fileID, varID, NC_COLLECTIVE, LogInfo);

                if (LogInfo->stopRun) {
                    return;
                }
            }
        }
#else
        (void) numOutFiles;
#endif
    }
}

/**
@brief Create and fill a new output netCDF file

\p hasConsistentSoilLayerDepths determines if vertical dimension (soil depth)
is represented by
    - soil layer depths (if entire domain has the same soil layer profile)
    - soil layer number (if soil layer profile varies across domain)

@param[in] OutDom Struct of type SW_OUT_DOM that holds output
    information that do not change throughout simulation runs
@param[in] latLonChunkSize A list of size NC_DIMS that holds the
chunking information for latitude and longitude or just sites
@param[in] timeChunkSize Size of the temporal dimension chunk size
@param[in] domFile Domain netCDF file name
@param[in] isSimDomDiscrete Is simulation domain discrete (site-based)?
    Otherwise, the simulation domain is gridded.
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
@param[in] yName User-provided latitude/y name
@param[in] xName User-provided longitude/x name
@param[out] newFileID New identifer for the newly created output file
@param[out] LogInfo Holds information on warnings and errors
*/
static void create_output_file(
    SW_OUT_DOM *OutDom,
    size_t latLonChunkSize[],
    size_t timeChunkSize,
    const char *domFile,
    Bool isSimDomDiscrete,
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
    const char *yName,
    const char *xName,
    int *newFileID,
    LOG_INFO *LogInfo
) {
    const Bool useDefaultChunking = swFALSE;
    const Bool addFillValAtt = swTRUE;

    int index;
    char frequency[10];
    const char *attNames[] = {
        "original_name",
        "long_name",
        "comment",
        "units",
        "cell_method",
        "coordinates",
        "units_metadata"
    };
    char *attVals[MAX_NATTS] = {NULL};
    OutSum sumType = OutDom->sumtype[key];

    int numAtts = 0;
    nc_type varType;
    char *typeStr;
    const int nameAtt = 0;
    const int coordAttInd = 5;
    double scaleFactor;
    double addOffset;

    int cellMethAttInd = 0;
    char *varName;
    char **varInfo;

    /* If SWMPI is not enabled, then this is not used in
       `SW_NC_create_template()` */
    Bool openInPar = swFALSE;
    const Bool isInput = swFALSE;

    (void) sw_memccpy(frequency, (char *) pd2longstr[pd], '\0', 10);
    Str_ToLower(frequency, frequency);

    // Create a new output file - if this function is called,
    // it means it did not already exist
    SW_NC_create_template(
        isSimDomDiscrete,
        domFile,
        newFileName,
        newFileID,
        isInput,
        frequency,
        openInPar,
        LogInfo
    );
    if (LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

    // Add output variables
    for (index = 0; index < nVar; index++) {
        if (OutDom->netCDFOutput.reqOutputVars[key][index]) {
            varInfo = OutDom->netCDFOutput.outputVarInfo[key][index];
            varName =
                OutDom->netCDFOutput.outputVarInfo[key][index][VARNAME_INDEX];

            numAtts = gather_var_attributes(
                varInfo, key, pd, index, attVals, sumType, yName, xName, LogInfo
            );
            if (LogInfo->stopRun) {
                return; // Exit function prematurely due to error
            }

            typeStr =
                OutDom->netCDFOutput.outputVarInfo[key][index][OUTPUT_TYPE];
            varType = NC_DOUBLE;
            if (Str_CompareI(typeStr, (char *) "short") == 0) {
                varType = NC_SHORT;
            } else if (Str_CompareI(typeStr, (char *) "integer") == 0) {
                varType = NC_INT;
            }

            scaleFactor = OutDom->netCDFOutput.scaleFactors[key][index];
            addOffset = OutDom->netCDFOutput.addOffsets[key][index];

            SW_NC_create_full_var(
                newFileID,
                isSimDomDiscrete,
                varType,
                originTimeSize,
                nsl[index],
                npft[index],
                latLonChunkSize[0],
                latLonChunkSize[1],
                timeChunkSize,
                varName,
                attNames,
                (const char **) attVals,
                numAtts,
                hasConsistentSoilLayerDepths,
                OutDom->netCDFOutput.posVerticalInBnds,
                lyrDepths,
                OutDom->netCDFOutput.posTimeInBnds,
                startTime,
                scaleFactor,
                addOffset,
                baseCalendarYear,
                startYr,
                pd,
                deflateLevel,
                yName,
                xName,
                OutDom->netCDFOutput.siteName,
                coordAttInd,
                useDefaultChunking,
                addFillValAtt,
                LogInfo
            );

            if (pd > eSW_Day) {
                if (*newFileID > -1) {
                    // new file was created
                    cellMethAttInd = (key == eSW_Temp || key == eSW_SoilTemp) ?
                                         numAtts - 3 :
                                         numAtts - 2;
                }
                if (!isnull(attVals[cellMethAttInd])) {
                    free(attVals[cellMethAttInd]);
                    attVals[cellMethAttInd] = NULL;
                }
            }
            if (key == eSW_Estab && !isnull(attVals[nameAtt])) {
                free(attVals[nameAtt]);
                attVals[nameAtt] = NULL;
            }
            if (!isnull(attVals[coordAttInd])) {
                free(attVals[coordAttInd]);
                attVals[coordAttInd] = NULL;
            }
            if (LogInfo->stopRun && FileExists(newFileName)) {
                return;
            }
        }
    }
}

/**
@brief Allocate memory, convert and store read-in scale factor and
add offsets for output variables

@param[in,out] OutDom Struct of type SW_OUT_DOM that holds output
information that do not change throughout simulation runs; return
with allocated scale factor and add offset arrays
@param[out] scaleFactors A list of size SW_OUTNKEYS holding lists of scale
factors for each output variable
@param[out] addOffsets A list of size SW_OUTNKEYS holding lists of add offsets
    for each output variable
@param[out] LogInfo Holds information on warnings and errors
*/
static void store_scale_add_attributes(SW_OUT_DOM *OutDom, LOG_INFO *LogInfo) {
    char *funcName = (char *) "store_scale_add_attributes()";
    int key;
    IntUS var;
    char *type;
    char *varName;

    char *scaleFactor;
    char *addOffset;

    ForEachOutKey(key) {
        if (!OutDom->use[key]) {
            continue;
        }

        OutDom->netCDFOutput.scaleFactors[key] = (double *) Mem_Malloc(
            OutDom->nvar_OUT[key] * sizeof(double), funcName, LogInfo
        );
        checkReturn(LogInfo->stopRun);

        OutDom->netCDFOutput.addOffsets[key] = (double *) Mem_Malloc(
            OutDom->nvar_OUT[key] * sizeof(double), funcName, LogInfo
        );
        checkReturn(LogInfo->stopRun);

        for (var = 0; var < OutDom->nvar_OUT[key]; var++) {
            if (!OutDom->netCDFOutput.reqOutputVars[key][var]) {
                continue;
            }

            scaleFactor =
                OutDom->netCDFOutput.outputVarInfo[key][var][SCALE_FACTOR];
            addOffset =
                OutDom->netCDFOutput.outputVarInfo[key][var][ADD_OFFSET];

            OutDom->netCDFOutput.scaleFactors[key][var] =
                sw_strtod(scaleFactor, funcName, LogInfo);
            checkReturn(LogInfo->stopRun);

            OutDom->netCDFOutput.addOffsets[key][var] =
                sw_strtod(addOffset, funcName, LogInfo);
            checkReturn(LogInfo->stopRun);

            type = OutDom->netCDFOutput.outputVarInfo[key][var][OUTPUT_TYPE];
            varName =
                OutDom->netCDFOutput.outputVarInfo[key][var][VARNAME_INDEX];

            if (EQ(OutDom->netCDFOutput.scaleFactors[key][var], 0.0)) {
                LogError(
                    LogInfo,
                    LOGERROR,
                    "Scale factor cannot be 0 (Variable: %s).",
                    varName
                );
            } else if (Str_CompareI(type, (char *) "double") != 0 &&
                       Str_CompareI(type, (char *) "short") != 0 &&
                       Str_CompareI(type, (char *) "integer") != 0) {

                LogError(
                    LogInfo,
                    LOGERROR,
                    "Invalid target type for output variable '%s'. "
                    "Valid types are 'double', 'short', or 'integer'.",
                    varName
                );
            }
            if (LogInfo->stopRun) {
                return;
            }
        }
    }
}

/**
@brief Convert all output values (type double) to the target packed (if
enabled) type (short or integer)

@param[in] type Target type to convert to (short or integer)
@param[in] nVals Number of values to convert
@param[in] scale_factor Scale factor to use for conversion
@param[in] add_offset Add offset to use for conversion
@param[in] doubleVals Array of type double holding the values to convert
@param[out] shortVals Array of type short to hold the converted values
@param[out] intVals Array of type integer to hold the converted values
*/
static void pack_output_values(
    char *type,
    size_t nVals,
    double scale_factor,
    double add_offset,
    double *doubleVals,
    short *shortVals,
    int *intVals
) {
    size_t index;
    double valToPack;

    if (Str_CompareI(type, (char *) "short") == 0) {
        for (index = 0; index < nVals; index++) {
            if (!EQ(doubleVals[index], NC_FILL_DOUBLE)) {
                valToPack = (doubleVals[index] - add_offset) / scale_factor;
                shortVals[index] = (short) nearbyint(valToPack);
            } else {
                shortVals[index] = NC_FILL_SHORT;
            }
        }
    } else if (Str_CompareI(type, (char *) "integer") == 0) {
        for (index = 0; index < nVals; index++) {
            if (!EQ(doubleVals[index], NC_FILL_DOUBLE)) {
                valToPack = (doubleVals[index] - add_offset) / scale_factor;
                intVals[index] = (int) nearbyint(valToPack);
            } else {
                intVals[index] = NC_FILL_INT;
            }
        }
    }
}

/* =================================================== */
/*             Global Function Definitions             */
/* --------------------------------------------------- */

/**
@brief Handle packed arrays for output variables by finding the most
a single output array would need when outputting (allocate) to take up
as little memory as possible, or free the allocated memory

@param[in] allocate Flag indicating if memory should be allocated
    (swTRUE) or freed (swFALSE)
@param[in] OutDom Struct of type SW_OUT_DOM that holds output
    information that do not change throughout simulation runs
@param[in] nP_OUT Total number of bytes to be written out within each
output key/period given one site
@param[out] tempShortVals Pointer to the temporary array of shorts will be
allocated to
@param[out] tempIntVals Pointer to the temporary array of integers will be
allocated to
@param[out] LogInfo Holds information on warnings and errors
*/
void SW_NCOUT_handle_packed_arrs(
    Bool allocate,
    SW_OUT_DOM *OutDom,
    size_t nP_OUT[][SW_OUTNPERIODS],
    short **tempShortVals,
    int **tempIntVals,
    LOG_INFO *LogInfo
) {
    const char *funcName = "SW_NCOUT_handle_packed_arrs()";
    size_t maxSize = 0;
    size_t nElem = 0;
    int key;
    int pd;

    if (allocate) {
        ForEachOutKey(key) {
            if (!OutDom->use[key]) {
                continue;
            }

            ForEachOutPeriod(pd) {
                if (!OutDom->use_OutPeriod[pd]) {
                    continue;
                }

                nElem = nP_OUT[key][pd] * OutDom->nrow_OUT[key][pd];
                if (nElem > maxSize) {
                    maxSize = nElem;
                }
            }
        }

        *tempShortVals =
            (short *) Mem_Malloc(maxSize * sizeof(short), funcName, LogInfo);
        checkReturn(LogInfo->stopRun);

        *tempIntVals =
            (int *) Mem_Malloc(maxSize * sizeof(int), funcName, LogInfo);
        checkReturn(LogInfo->stopRun);
    } else {
        free((void *) *tempShortVals);
        checkReturn(LogInfo->stopRun);

        free((void *) *tempIntVals);
        checkReturn(LogInfo->stopRun);
    }
}

/**
@brief Calculate time size in days

The count includes only days of complete output periods
(weeks, months, and years), i.e., time periods that are not affected by an
early simulation end (before December 31 of the last year).

See also \ref SW_MODEL_SIM.endperiod which is updated by SW_MDL_new_day().

For example, a simulation with 300 as the last day of year produces a
monthly output that does not contain November (incomplete) and December
in the last year.

This function ignores a delayed simulation start
(after January 1 of the first year) unless only one year is simulated.

No output file is created for a time size of 0.

@param[in] SW_Domain Struct of type SW_DOMAIN holding constant
    temporal/spatial information for a set of simulation runs
@param[in] rangeStart Start year for the current output file
@param[in] rangeEnd End year for the current output file
@param[in] baseTime Base number of output periods in a year
    (e.g., 60 months in 5 years, or 731 days in 1980-1981)
@param[in] pd Current output netCDF period
@param[in] numDaysInMonth Number of days in each month of the last
year of the simulation
@param[in] cumDaysInMonth Running sum of total days at the end of
each month of the last year of simulation

@return Time size for the provided year range and output period
*/
unsigned int SW_NCOUT_calc_timeSize(
    SW_DOMAIN *SW_Domain,
    unsigned int rangeStart,
    unsigned int rangeEnd,
    unsigned int baseTime,
    OutPeriod pd,
    TimeInt numDaysInMonth[],
    TimeInt cumDaysInMonth[]
) {
    const TimeInt endYr = SW_Domain->endyr;
    unsigned int numPdInDays = 0;

    unsigned int timeSize = baseTime * (rangeEnd - rangeStart);
    unsigned int year;
    TimeInt nWeeks;
    Bool fullTStep;
    Bool fullLastWeek;
    TimeInt lastDoy;

    if (pd == eSW_Day) {
        if (SW_Domain->startyr == SW_Domain->endyr &&
            rangeStart == SW_Domain->startyr) {

            timeSize = SW_Domain->endend - SW_Domain->startstart + 1;
        } else {
            timeSize = 0;
            for (year = rangeStart; year < rangeEnd; year++) {
                if (year < endYr) {
                    timeSize += Time_get_lastdoy_y(year);
                } else if (year == endYr) {
                    timeSize += SW_Domain->endend;
                }
            }
        }
    } else {
        if (rangeEnd - 1 == endYr) {
            lastDoy = Time_get_lastdoy_y(SW_Domain->endyr);

            switch (pd) {
            case eSW_Week:
                nWeeks = doy2week(SW_Domain->endend) + 1;
                fullLastWeek = (Bool) (nWeeks == MAX_WEEKS &&
                                       SW_Domain->endend == lastDoy);
                fullTStep =
                    (Bool) (SW_Domain->endend % WKDAYS == 0 || fullLastWeek);
                nWeeks -= (!fullTStep) ? 1 : 0;
                numPdInDays = MAX_WEEKS - nWeeks;
                break;
            case eSW_Month:
                numPdInDays = MAX_MONTHS;
                Time_new_year(endYr, numDaysInMonth, cumDaysInMonth);
                while (numPdInDays - 1 > 0 &&
                       cumDaysInMonth[numPdInDays - 1] > SW_Domain->endend) {

                    numPdInDays--;
                }

                fullTStep = (Bool) (SW_Domain->endend ==
                                    cumDaysInMonth[numPdInDays - 1]);
                numPdInDays -= (numPdInDays - 1 == 0 && !fullTStep) ? 1 : 0;
                numPdInDays = MAX_MONTHS - numPdInDays;
                break;
            default: /* eSW_Year */
                numPdInDays = (SW_Domain->endend == lastDoy) ? 0 : 1;
                break;
            };
            timeSize -= numPdInDays;
        }
    }

    return timeSize;
}

/**
@brief Zero-out failed site output values

@param[in] SW_Domain Struct of type SW_DOMAIN holding constant
temporal/spatial information for a set of simulation runs
@param[in] siteIndex Index of the site that is being zeroed relative
to the total number of sites in the subdomain
@param[out] p_OUT Array of accumulated output values throughout
simulation years
*/
void SW_NCOUT_reset_failed_sites(
    SW_DOMAIN *SW_Domain, size_t siteIndex, double *p_OUT[][SW_OUTNPERIODS]
) {
    const size_t nSites = SW_Domain->nSitesInSubDom;

    SW_OUT_DOM *OutDom = &SW_Domain->OutDom;

    size_t time;
    int key;
    int pd;
    int var;
    size_t sl;
    size_t pft;

    size_t nSl;
    size_t npft;

    Bool hasSl;
    Bool hasPFT;

    size_t targetIdx;

    ForEachOutKey(key) {
        if (!OutDom->use[key]) {
            continue;
        }

        ForEachOutPeriod(pd) {
            if (!OutDom->use_OutPeriod[pd]) {
                continue;
            }

            for (time = 0; time < OutDom->nrow_OUT[key][pd]; time++) {
                for (var = 0; var < OutDom->nvar_OUT[key]; var++) {
                    if (!OutDom->netCDFOutput.reqOutputVars[key][var]) {
                        continue;
                    }

                    hasSl = (Bool) (OutDom->nsl_OUT[key][var] == 0);
                    hasPFT = (Bool) (OutDom->npft_OUT[key][var] == 0);

                    nSl = hasSl ? OutDom->nsl_OUT[key][var] : 1;
                    npft = hasPFT ? OutDom->npft_OUT[key][var] : 1;

                    for (pft = 0; pft < npft; pft++) {
                        ForEachSoilLayer(sl, nSl) {
                            targetIdx =
                                OutDom->netCDFOutput.iOUToffset[key][pd][var];

                            targetIdx += iOUTnc(
                                time, sl, siteIndex, pft, nSl, nSites, npft
                            );

                            p_OUT[key][pd][targetIdx] = NC_FILL_DOUBLE;
                        }
                    }
                }
            }
        }
    }
}

/**
@brief Create a "time", "vertical", or "pft" dimension variable and the
respective "*_bnds" variables (plus "bnds" dimension)
and fill the variable with the respective information

@param[in] name Name of the new dimension
@param[in] size Size of the new dimension
@param[in] ncFileID Identifier of the netCDF in which the information
    will be written
@param[in,out] dimID New dimension identifier within the given netCDF
@param[in] hasConsistentSoilLayerDepths Flag indicating if all simulation
    run within domain have identical soil layer depths
    (though potentially variable number of soil layers)
@param[in] posVerticalInBnds Position of vertical coordinate values
    relative to bounds
@param[in] lyrDepths Depths of soil layers (cm)
@param[in] posTimeInBnds Position of time coordinate values relative to bounds
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
    size_t size,
    int ncFileID,
    int *dimID,
    Bool hasConsistentSoilLayerDepths,
    int posVerticalInBnds,
    double lyrDepths[],
    int posTimeInBnds,
    double *startTime,
    unsigned int baseCalendarYear,
    unsigned int startYr,
    OutPeriod pd,
    int deflateLevel,
    LOG_INFO *LogInfo
) {

    char *dimNames[3] = {(char *) "vertical", (char *) "time", (char *) "pft"};
    char *dimVarNames[3] = {
        (char *) "vertical", (char *) "time", (char *) "pft_label"
    };
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
    int numDims = 1;

    const char *outAttNames[][6] = {
        {"long_name", "standard_name", "units", "positive", "axis", "bounds"},
        {"long_name", "standard_name", "units", "axis", "calendar", "bounds"},
        {"standard_name"}
    };

    char outAttVals[][6][MAX_FILENAMESIZE] = {
        {"soil depth", "depth", "centimeter", "down", "Z", "vertical_bnds"},
        {"time", "time", "", "T", "standard", "time_bnds"},
        {"SOILWAT2 vegetation type"}
    };

    char *soilWritePtr = outAttVals[vertIndex][0];
    char *centiWritePtr = outAttVals[vertIndex][2];
    char *endSoilDepthPtr =
        outAttVals[vertIndex][0] + sizeof outAttVals[vertIndex][0] - 1;
    char *endCentiPtr =
        outAttVals[vertIndex][2] + sizeof outAttVals[vertIndex][2] - 1;
    size_t soilDepthSize = MAX_FILENAMESIZE - strlen(outAttVals[vertIndex][0]);
    size_t centiSize = MAX_FILENAMESIZE - strlen(outAttVals[vertIndex][2]);
    Bool fullBuffer = swFALSE;

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

    varType = (dimNum == pftIndex) ? NC_CHAR : NC_DOUBLE;

    startFillTime = (dimNum == timeIndex) ? startTime : &tempVal;

    /* Create the dimension and get its identifier */
    SW_NC_create_netCDF_dim(
        dimNames[dimNum], size, &ncFileID, &dimIDs[0], LogInfo
    );
    *dimID = dimIDs[0];
    if (LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

    if (dimNum == pftIndex) {
        /* Create extra dimension for character length of PFT labels */
        numDims = 2;
        SW_NC_create_netCDF_dim(
            "pft_label_nchar",
            MAX_PFT_NAME_LENGTH,
            &ncFileID,
            &dimIDs[1],
            LogInfo
        );
        if (LogInfo->stopRun) {
            return; // Exit function prematurely due to error
        }
    }

    /* Create the dimension variable and fill it with values */
    if (!SW_NC_varExists(ncFileID, dimVarNames[dimNum])) {
        SW_NC_create_netCDF_var(
            &varID,
            dimVarNames[dimNum],
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
            posVerticalInBnds,
            lyrDepths,
            startFillTime,
            dimNum,
            posTimeInBnds,
            startYr,
            pd,
            deflateLevel,
            LogInfo
        );
        if (LogInfo->stopRun) {
            return;
        }

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
            fullBuffer = sw_memccpy_inc(
                (void **) &soilWritePtr,
                endSoilDepthPtr,
                (void *) "soil layer",
                '\0',
                &soilDepthSize
            );
            if (fullBuffer) {
                goto reportFullBuffer;
            }

            fullBuffer = sw_memccpy_inc(
                (void **) &centiWritePtr,
                endCentiPtr,
                (void *) "1",
                '\0',
                &centiSize
            );
            if (fullBuffer) {
                goto reportFullBuffer;
            }
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

reportFullBuffer:
    if (fullBuffer) {
        reportFullBuffer(LOGERROR, LogInfo);
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
    - SW_OUT_set_out_counts() to set GenOutput.nvar_OUT

@param[in,out] OutDom Struct of type SW_OUT_DOM that holds output
    information that do not change throughout simulation runs
@param[in] txtInFiles Array of program in/output files
@param[in] parmsIn Array of type SW_VEGESTAB_INFO_INPUTS holding input
    information about species
@param[out] LogInfo Holds information on warnings and errors
*/
void SW_NCOUT_read_out_vars(
    SW_OUT_DOM *OutDom,
    char *txtInFiles[],
    SW_VEGESTAB_INFO_INPUTS *parmsIn,
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
    int newIndex;
    int estVar;
    int resSNP;
    char *copyStr = NULL;
    char *tempStr = NULL;
    char input[NOUT_VAR_INPUTS][MAX_ATTVAL_SIZE] = {"\0"};
    char establn[MAX_ATTVAL_SIZE] = {"\0"};
    int scanRes = 0;
    int defToLocalInd = 0;
    /* readLineFormat:
        (NOUT_VAR_INPUTS - 1) times `%255[^\t]\t`
        followed by one final `%255[^\t]` without the tab at the end.
        255 must be equal to MAX_ATTVAL_SIZE - 1 */
    const char *readLineFormat =
        "%255[^\t]\t%255[^\t]\t%255[^\t]\t%255[^\t]\t%255[^\t]\t%255[^\t]\t"
        "%255[^\t]\t%255[^\t]\t%255[^\t]\t%255[^\t]\t%255[^\t]\t%255[^\t]\t"
        "%255[^\t]\t%255[^\t]\t%255[^\t]";
    int doOutputVal;

#if defined(SWDEBUG)
    /* 9 = length of each `%255[^\t]\t` specifier in readLineFormat */
    if ((NOUT_VAR_INPUTS * 9 - 1) != strlen(readLineFormat)) {
        LogError(
            LogInfo,
            LOGERROR,
            "Programmer: SW_NCOUT_read_out_vars(): "
            "NOUT_VAR_INPUTS = %d must match the number of specifiers "
            "'%%255[^\\t]\\t' (estimated n = %d) in readLineFormat",
            NOUT_VAR_INPUTS,
            (strlen(readLineFormat) + 1) / 9
        );
        return; // Exit prematurely due to error
    }
#endif

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
    const int outType = 10;
    const int outScaleFactor = 11;
    const int outAddOffset = 12;
    const int cellMethodInd = 13;
    const int usercommentInd = 14;

    MyFileName = txtInFiles[eNCOutVars];
    f = OpenFile(MyFileName, "r", LogInfo);
    if (LogInfo->stopRun) {
        return; // Exit prematurely due to error
    }

    SW_NCOUT_alloc_output_var_info(OutDom, LogInfo);
    if (LogInfo->stopRun) {
        goto closeFile; // Exit prematurely due to error
    }

    while (GetALine(f, inbuf, MAX_FILENAMESIZE)) {
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
            input[outType],
            input[outScaleFactor],
            input[outAddOffset],
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

        if (lineno == 0) {
            for (index = keyInd; index <= cellMethodInd; index++) {
                tempStr = (char *) expectedColNames[index];

                if (strcmp(input[index], tempStr) != 0) {
                    LogError(
                        LogInfo,
                        LOGERROR,
                        "Column '%s' was found instead of '%s' in the "
                        "input file '%s'.",
                        input[index],
                        expectedColNames[index],
                        MyFileName
                    );
                    goto closeFile; /* Exit function prematurely due to error */
                }
            }

            lineno++;
            continue;
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
            // Output variable name, long name, comment, units, output type,
            // scale_factor, add_offset and cell_method
            for (index = 0; index <= cellMethodInd - dimInd; index++) {
                defToLocalInd = index + dimInd;
                newIndex = (defToLocalInd > doOutInd) ? index - 1 : index;

                if (defToLocalInd == doOutInd) {
                    continue;
                }

                if (strcmp(input[defToLocalInd], "NA") == 0) {
                    if (newIndex > VARNAME_INDEX) {
                        copyStr = (char *) "";
                    } else if (newIndex == VARNAME_INDEX) {
                        LogError(
                            LogInfo,
                            LOGERROR,
                            "Output variable names cannot be 'NA' (line %d).",
                            lineno + 1
                        );
                    } else {
                        LogError(
                            LogInfo,
                            LOGERROR,
                            "Output dimensions cannot be 'NA' (line %d).",
                            lineno + 1
                        );
                    }
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
                if (LogInfo->stopRun) {
                    return;
                }

                // Handle ESTAB differently by storing all attributes
                // into `count` amount of variables and give the
                // correct <sppname>'s
                if (currOutKey == eSW_Estab) {
                    for (estVar = 0; estVar < OutDom->nvar_OUT[currOutKey];
                         estVar++) {

                        switch (newIndex) {
                        case VARNAME_INDEX:
                            OutDom->netCDFOutput
                                .reqOutputVars[currOutKey][estVar] = swTRUE;
                            OutDom->netCDFOutput
                                .outputVarInfo[currOutKey][estVar][newIndex] =
                                Str_Dup(parmsIn->sppname[estVar], LogInfo);
                            break;

                        case LONGNAME_INDEX:
                            (void) sw_memccpy(
                                establn, copyStr, '\0', MAX_ATTVAL_SIZE
                            );
                            OutDom->netCDFOutput
                                .outputVarInfo[currOutKey][estVar][newIndex] =
                                Str_Dup(establn, LogInfo);
                            break;

                        default:
                            OutDom->netCDFOutput
                                .outputVarInfo[currOutKey][estVar][newIndex] =
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
                        .outputVarInfo[currOutKey][varNum][newIndex] =
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

        lineno++;
    }

    store_scale_add_attributes(OutDom, LogInfo);

    // Update "use": turn off if no variable of an outkey group is requested
    ForEachOutKey(index) {
        if (!used_OutKeys[index]) {
            OutDom->use[index] = swFALSE;
        }
    }
    checkReturn(LogInfo->stopRun);

closeFile: { CloseFile(&f, LogInfo); }
}

/**
@brief Initializes pointers only pertaining to netCDF output information

@param[in,out] SW_netCDFOut Constant netCDF output file information
*/
void SW_NCOUT_init_ptrs(SW_NETCDF_OUT *SW_netCDFOut) {

    int index;
    const int numAllocVars = 20;
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
        &SW_netCDFOut->crs_projsc.units,
        &SW_netCDFOut->geo_XAxisName,
        &SW_netCDFOut->geo_YAxisName,
        &SW_netCDFOut->proj_XAxisName,
        &SW_netCDFOut->proj_YAxisName,
        &SW_netCDFOut->siteName,
        &SW_netCDFOut->crs_geogsc.crs_name,
        &SW_netCDFOut->crs_projsc.crs_name
    };

    SW_netCDFOut->crs_projsc.standard_parallel[0] = NAN;
    SW_netCDFOut->crs_projsc.standard_parallel[1] = NAN;

    SW_netCDFOut->strideOutYears = -1;
    SW_netCDFOut->deflateLevel = 0;

    SW_netCDFOut->posTimeInBnds = 0;     /* default: centered */
    SW_netCDFOut->posVerticalInBnds = 1; /* default: bottom bound */

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
        SW_netCDFOut->scaleFactors[key] = NULL;
        SW_netCDFOut->addOffsets[key] = NULL;
    }
#endif

    memset(
        SW_netCDFOut->fileTimeChunk,
        0,
        sizeof(size_t) * SW_OUTNKEYS * SW_OUTNPERIODS
    );

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

    if (!isnull(OutDom->netCDFOutput.scaleFactors[k])) {
        free((void *) OutDom->netCDFOutput.scaleFactors[k]);
        OutDom->netCDFOutput.scaleFactors[k] = NULL;
    }

    if (!isnull(OutDom->netCDFOutput.addOffsets[k])) {
        free((void *) OutDom->netCDFOutput.addOffsets[k]);
        OutDom->netCDFOutput.addOffsets[k] = NULL;
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
        numFiles * sizeof(char *), "SW_NCOUT_alloc_files", LogInfo
    );
    if (LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

    for (varNum = 0; varNum < numFiles; varNum++) {
        (*ncOutFiles)[varNum] = NULL;
    }
}

/**
@brief Allocate memory to store output variable identifiers

@param[out] ncVarIDs Output variable identifiers contained within every
    output file that is created for a key
@param[in] numVars Number of variables within an output key
@param[out] LogInfo Holds information on warnings and errors
*/
void SW_NCOUT_alloc_varids(int **ncVarIDs, IntUS numVars, LOG_INFO *LogInfo) {
    IntUS varNum;

    *ncVarIDs = (int *) Mem_Malloc(
        numVars * sizeof(int), "SW_NCOUT_alloc_varids", LogInfo
    );
    if (LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

    for (varNum = 0; varNum < numVars; varNum++) {
        (*ncVarIDs)[varNum] = -1;
    }
}

/**
@brief Allocate memory to store the time sizes of each output file

@param[in] numFiles Number of values to allocate for (one for every output file
    in an output period)
@param[out] timeSizes An array of size two to hold the time sizes for every
    output file for a specific output period
@param[out] LogInfo Holds information on warnings and errors
*/
void SW_NCOUT_alloc_timeSizes(
    unsigned int numFiles, size_t **timeSizes, LOG_INFO *LogInfo
) {
    *timeSizes = (size_t *) Mem_Malloc(
        sizeof(size_t) * numFiles, "SW_NCOUT_alloc_timeSizes", LogInfo
    );
    if (LogInfo->stopRun) {
        return;
    }

    Mem_Set(*timeSizes, 0, sizeof(size_t) * numFiles);
}

/**
@brief Allocate memory to store the output file IDs

@param[in] numFiles Number of values to allocate for (one for every output file
    in an output period)
@param[out] fileIDs An array of size [numFiles] to hold all the enabled
    output files' netCDF IDs
@param[out] LogInfo Holds information on warnings and errors
*/
void SW_NCOUT_alloc_outfile_ids(
    unsigned int numFiles, int **fileIDs, LOG_INFO *LogInfo
) {
    unsigned int file;

    *fileIDs = (int *) Mem_Malloc(
        sizeof(int) * numFiles, "SW_NCOUT_alloc_timeSizes", LogInfo
    );

    for (file = 0; file < numFiles; file++) {
        (*fileIDs)[file] = -1;
    }
}

/**
@brief Close all opened output netCDF files

@param[in] openOutFileIDs A list of open output netCDF file IDs
@param[in] numOutFiles Number of output files for each
    output key/period
*/
void SW_NCOUT_close_out_files(
    int *openOutFileIDs[][SW_OUTNPERIODS], IntU numOutFiles
) {
    int outKey;
    OutPeriod pd;
    IntU file;

    ForEachOutKey(outKey) {
        ForEachOutPeriod(pd) {
            if (!isnull(openOutFileIDs[outKey][pd])) {
                for (file = 0; file < numOutFiles; file++) {
                    nc_close(openOutFileIDs[outKey][pd][file]);
                }
            }
        }
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
@param[in] isSimDomDiscrete Is simulation domain discrete (site-based)?
    Otherwise, the simulation domain is gridded.
@param[in] outputPrefix Directory path of output files.
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
@param[out] SW_PathOutputs Struct of type SW_PATH_OUTPUTS which
holds basic information about output files and values
@param[out] LogInfo Holds information on warnings and errors
*/
void SW_NCOUT_create_output_files(
    const char *domFile,
    Bool isSimDomDiscrete,
    const char *outputPrefix,
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
    SW_PATH_OUTPUTS *SW_PathOutputs,
    LOG_INFO *LogInfo
) {
    TimeInt numDaysInMonth[MAX_MONTHS] = {0};
    TimeInt cumDaysInMonth[MAX_MONTHS] = {0};

    Bool primCRSIsGeo =
        SW_Domain->OutDom.netCDFOutput.primary_crs_is_geographic;

    /* Get latitude/longitude names that were read-in from input file */
    char *readinYName = (primCRSIsGeo) ?
                            SW_Domain->OutDom.netCDFOutput.geo_YAxisName :
                            SW_Domain->OutDom.netCDFOutput.proj_YAxisName;
    char *readinXName = (primCRSIsGeo) ?
                            SW_Domain->OutDom.netCDFOutput.geo_XAxisName :
                            SW_Domain->OutDom.netCDFOutput.proj_XAxisName;

    int key;
    int ip;
    int resSNP = 0;
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
    double baseStartTime[SW_OUTNPERIODS] = {0};
    unsigned int *numOutFiles = &SW_PathOutputs->numOutFiles;
    const Bool openInPar = swTRUE;
    const int openMode = NC_WRITE;
    int *fileID;
    Bool fileExists = swFALSE;

    char periodSuffix[10];
    char *yearFormat;

    yearOffset =
        (strideOutYears == -1) ? numYears : (unsigned int) strideOutYears;

    yearFormat = (strideOutYears == 1) ? (char *) "%d" : (char *) "%d-%d";

    Time_init_model(numDaysInMonth);
    Time_new_year(SW_Domain->endyr, numDaysInMonth, cumDaysInMonth);

    // Calculate base offset (in days) from the base calendar year to
    // the start year to set the start time values from base calendar year
    // instead of start year (0)
    if ((IntU) baseCalendarYear < startYr) {
        ForEachOutPeriod(pd) {
            timeSize = SW_NCOUT_calc_timeSize(
                SW_Domain,
                (IntU) baseCalendarYear,
                (IntU) startYr,
                outTimes[pd],
                pd,
                numDaysInMonth,
                cumDaysInMonth
            );

            calc_num_timedays(
                timeSize,
                pd,
                (IntU) baseCalendarYear,
                0,    // unused
                NULL, // unused
                NULL, // unused
                &baseStartTime[pd]
            );
        }
    }

    ForEachOutKey(key) {
        if (nvar_OUT[key] > 0 && SW_Domain->OutDom.use[key]) {
            SW_NCOUT_alloc_varids(
                &SW_PathOutputs->ncOutVarIDs[key], nvar_OUT[key], LogInfo
            );
            checkReturn(LogInfo->stopRun);

            // Loop over requested output periods (which may vary for each
            // outkey)
            for (ip = 0; ip < used_OUTNPERIODS; ip++) {
                pd = timeSteps[key][ip];

                if (pd != eSW_NoTime) {
                    startTime[pd] = baseStartTime[pd];
                    baseTime = outTimes[pd];
                    rangeStart = startYr;

                    (void) sw_memccpy(
                        periodSuffix, (char *) pd2longstr[pd], '\0', 10
                    );
                    Str_ToLower(periodSuffix, periodSuffix);

                    SW_NCOUT_alloc_outfile_ids(
                        *numOutFiles,
                        &SW_PathOutputs->openOutFileIDs[key][pd],
                        LogInfo
                    );
                    checkReturn(LogInfo->stopRun);

                    SW_NCOUT_alloc_files(
                        &SW_PathOutputs->ncOutFiles[key][pd],
                        *numOutFiles,
                        LogInfo
                    );
                    checkReturn(LogInfo->stopRun);

                    for (fileNum = 0; fileNum < *numOutFiles; fileNum++) {
                        rangeEnd = rangeStart + yearOffset;
                        rangeEnd = (rangeEnd > endYr) ? endYr + 1 : rangeEnd;

                        (void) snprintf(
                            yearBuff, 10, yearFormat, rangeStart, rangeEnd - 1
                        );
                        resSNP = snprintf(
                            fileNameBuf,
                            sizeof fileNameBuf,
                            "%s%s_%s_%s.nc",
                            outputPrefix,
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

                        SW_PathOutputs->ncOutFiles[key][pd][fileNum] =
                            Str_Dup(fileNameBuf, LogInfo);
                        checkReturn(LogInfo->stopRun);

                        fileID =
                            &SW_PathOutputs->openOutFileIDs[key][pd][fileNum];
                        fileExists = FileExists(fileNameBuf);
#if defined(SWMPI)
                        MPI_Barrier(MPI_COMM_WORLD);
#endif
                        if (fileExists) {
                            SW_NC_check(
                                SW_Domain,
                                fileID,
                                fileNameBuf,
                                openInPar,
                                openMode,
                                LogInfo
                            );
                        } else {
                            timeSize = SW_NCOUT_calc_timeSize(
                                SW_Domain,
                                rangeStart,
                                rangeEnd,
                                baseTime,
                                pd,
                                numDaysInMonth,
                                cumDaysInMonth
                            );

                            if (SW_Domain->rank == ROOT_PROC && timeSize > 0) {
                                create_output_file(
                                    &SW_Domain->OutDom,
                                    SW_Domain->spaceChunk,
                                    SW_Domain->OutDom.netCDFOutput
                                        .fileTimeChunk[key][pd],
                                    domFile,
                                    isSimDomDiscrete,
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
                                    readinYName,
                                    readinXName,
                                    fileID,
                                    LogInfo
                                );
                            }
#if defined(SWMPI)
                            checkReturn(LogInfo->stopRun);

                            if (*fileID > -1 && SW_Domain->rank == ROOT_PROC) {
                                nc_close(*fileID);
                            }

                            SW_MPI_Barrier(MPI_COMM_WORLD);

                            if (fileExists || timeSize > 0) {
                                SW_NC_open_par(
                                    fileNameBuf,
                                    NC_WRITE,
                                    MPI_COMM_WORLD,
                                    fileID,
                                    LogInfo
                                );
                                checkReturn(LogInfo->stopRun);
                            }
#endif
                        }
                        checkReturn(LogInfo->stopRun);

                        rangeStart = rangeEnd;
                    }

                    if (isnull(SW_PathOutputs->outTimeSizes[pd])) {
                        SW_NCOUT_alloc_timeSizes(
                            *numOutFiles,
                            &SW_PathOutputs->outTimeSizes[pd],
                            LogInfo
                        );
                        checkReturn(LogInfo->stopRun);

                        store_time_sizes(
                            SW_PathOutputs->openOutFileIDs[key][pd],
                            *numOutFiles,
                            &SW_PathOutputs->outTimeSizes[pd],
                            LogInfo
                        );
                        checkReturn(LogInfo->stopRun);
                    }
                }

                get_outvar_ids(
                    SW_Domain->OutDom.netCDFOutput.outputVarInfo[key],
                    nvar_OUT[key],
                    SW_PathOutputs->openOutFileIDs[key][pd],
                    *numOutFiles,
                    SW_PathOutputs->ncOutVarIDs[key],
                    LogInfo
                );
                checkReturn(LogInfo->stopRun);

                if (pd != eSW_NoTime && fileExists) {
                    check_output_file_vars(
                        SW_Domain,
                        SW_PathOutputs,
                        baseTime,
                        yearOffset,
                        key,
                        pd,
                        numDaysInMonth,
                        cumDaysInMonth,
                        LogInfo
                    );
                    checkReturn(LogInfo->stopRun);
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
    - SW_OUT_set_out_counts() to set GenOutput.nvar_OUT for argument nVars

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
@param[in] nSites Total number of sites in the process' subdomain that will
    be written out
@param[in] nActiveSites Number of active sites in the process' subdomain
@param[in] starts A list of size NC_DIMS specifying the start
    indices used when writing the program's/process' subdomain
    using the netCDF library
@param[in] counts A list of size NC_DIMS specifying the count
    indices used when writing the program's/process' subdomain
    using the netCDF library
@param[in] tempShortVals An allocated space to store temporary packed output
variables of type short
@param[in] tempIntVals An allocated space to store temporary packed output
variables of type int
@param[in] openOutFileIDs Lists of file IDs of open output netCDF files;
    only used if SWMPI is enabled, otherwise is NULL
@param[in] outVarIDs A list of size SW_OUTNKEYS holding lists of
    output variable IDs
@param[in] isSimDomDiscrete Is simulation domain discrete (site-based)?
    Otherwise, the simulation domain is gridded.
@param[in] forceWriteOut Specifies if this function call will be writing
    out all information no matter if we have stored enough information
    to write out for any active key/output period; this is true if
    a fatal error occurred or a signal was received by the process to stop,
    i.e., anytime the program stops prematurely
@param[in] endperiod Array of size SW_OUTNPERIODS specifying if an output period
    is ready to be written out
@param[in] irow_OUT Current time step
@param[in] timeSizes An array of size two to hold the time sizes for every
    output file for a specific output period
@param[out] LogInfo Holds information on warnings and errors
*/
void SW_NCOUT_write_output(
    SW_OUT_DOM *OutDom,
    double *p_OUT[][SW_OUTNPERIODS],
    unsigned int numFilesPerKey,
    size_t nSites,
    size_t nActiveSites,
    size_t starts[],
    size_t counts[],
    const short *tempShortVals,
    const int *tempIntVals,
    int *openOutFileIDs[][SW_OUTNPERIODS],
    int *outVarIDs[],
    Bool isSimDomDiscrete,
    Bool forceWriteOut,
    const Bool endperiod[],
    size_t irow_OUT[][SW_OUTNPERIODS],
    size_t *timeSizes[],
    LOG_INFO *LogInfo
) {
    const size_t startSiteIndex = 0;

    int key;
    OutPeriod pd;
    double *p_OUTValPtr = NULL;
    void *writePtr = NULL;
    char *varType;
    double scale_factor;
    double add_offset;
    unsigned int fileNum;
    int currFileID = 0;
    int varNum;
    int varID = -1;

    size_t count[MAX_NUM_DIMS] = {0};
    size_t start[MAX_NUM_DIMS] = {0};
    size_t pOUTIndex;
    size_t timeSize = 0;
    size_t countTotal = 0;
    int vertSize;
    int pftSize;
    size_t startTime;
    size_t startFile;
    size_t finalFile;
    size_t totTimeSize;
    Bool writtenOutAlready;
    size_t startTimeIndex;
    size_t timeLeft;

    IntU numFilesToWrite[SW_OUTNKEYS][SW_OUTNPERIODS] = {{0}};
    size_t newStartIndices[SW_OUTNKEYS][SW_OUTNPERIODS] = {{0}};

    size_t numElem;

#if defined(SWUDUNITS)
    size_t valNum;
#endif

    ForEachOutPeriod(pd) {
        if (!OutDom->use_OutPeriod[pd] || (!endperiod[pd] && !forceWriteOut)) {
            continue; // Skip period iteration
        }

        ForEachOutKey(key) {
            /* If nrow_OUT[key] = 0, that means we have stored
                enough output to write out, otherwise, we are still in
                the process of storing */
            if (OutDom->nvar_OUT[key] == 0 || !OutDom->use[key] ||
                (OutDom->nrow_OUT[key][pd] > irow_OUT[key][pd] + 1 &&
                 !forceWriteOut)) {

                continue; // Skip key iteration
            }

            writtenOutAlready = get_num_out_files(
                numFilesPerKey,
                OutDom->netCDFOutput.runOutFileIndex[key][pd],
                timeSizes[pd],
                OutDom->netCDFOutput.outTempStart[key][pd],
                OutDom->nrow_OUT[key][pd],
                &numFilesToWrite[key][pd],
                &newStartIndices[key][pd]
            );
            if (writtenOutAlready) {
                continue;
            }

            startTime = OutDom->netCDFOutput.outTempStart[key][pd];
            startTimeIndex = 0;

            // Loop over output time-slices
            // Keep track of time across time-sliced files per outkey
            startFile = OutDom->netCDFOutput.runOutFileIndex[key][pd];
            finalFile = startFile + numFilesToWrite[key][pd] - 1;
            totTimeSize = OutDom->nrow_OUT[key][pd];
            for (fileNum = startFile; fileNum <= finalFile; fileNum++) {
                currFileID = openOutFileIDs[key][pd][fileNum];

                // Get size of the "time" dimension
                timeSize = timeSizes[pd][fileNum];
                if (timeSize == 0) {
                    continue;
                }

                if (startFile == finalFile) {
                    timeLeft = timeSizes[pd][fileNum] - startTime;

                    if (fileNum == numFilesPerKey - 1 &&
                        timeLeft <= OutDom->nrow_OUT[key][pd]) {

                        timeSize = timeLeft;
                    } else {
                        timeSize = OutDom->nrow_OUT[key][pd];
                    }
                } else {
                    if (fileNum != startFile && fileNum != finalFile) {
                        timeSize = timeSizes[pd][fileNum];
                    } else if (fileNum == startFile) {
                        timeSize = timeSizes[pd][fileNum] - startTime;
                    } else {
                        timeSize = (totTimeSize > timeSizes[pd][fileNum]) ?
                                       timeSizes[pd][fileNum] :
                                       totTimeSize;
                    }
                }

                for (varNum = 0; varNum < OutDom->nvar_OUT[key]; varNum++) {
                    if (!OutDom->netCDFOutput.reqOutputVars[key][varNum]) {
                        continue; // Skip variable iteration
                    }

                    // Locate correct slice in netCDF to write to
                    varID = outVarIDs[key][varNum];

                    get_vardim_write_start_counts(
                        isSimDomDiscrete,
                        counts,
                        starts,
                        startTime,
                        timeSize,
                        OutDom->nsl_OUT[key][varNum],
                        OutDom->npft_OUT[key][varNum],
                        count,
                        start,
                        &countTotal
                    );

                    pOUTIndex =
                        OutDom->netCDFOutput.iOUToffset[key][pd][varNum];

                    // 1 if no soil layers
                    vertSize = (OutDom->nsl_OUT[key][varNum] > 0) ?
                                   OutDom->nsl_OUT[key][varNum] :
                                   1;

                    // 1 if no vegtypes
                    pftSize = (OutDom->npft_OUT[key][varNum] > 0) ?
                                  OutDom->npft_OUT[key][varNum] :
                                  1;

                    pOUTIndex += iOUTnc(
                        startTimeIndex,
                        0,
                        startSiteIndex,
                        0,
                        vertSize,
                        nSites,
                        pftSize
                    );

                    if (nActiveSites > 0) {
                        p_OUTValPtr = &p_OUT[key][pd][pOUTIndex];

                        /* Convert units if udunits2 and if converter available
                         */
                        numElem = countTotal * nSites;
#if defined(SWUDUNITS)
                        if (!isnull(OutDom->netCDFOutput.uconv[key][varNum])) {
                            for (valNum = 0; valNum < numElem; valNum++) {
                                if (p_OUTValPtr[valNum] != FILL_DOUBLE) {
                                    p_OUTValPtr[valNum] = cv_convert_double(
                                        OutDom->netCDFOutput.uconv[key][varNum],
                                        p_OUTValPtr[valNum]
                                    );
                                }
                            }
                        }
#endif
                    } else {
                        // Don't write output if a process has no active sites;
                        // this seems to increase the file size even if we are
                        // just writing fill values (NC_FILL_DOUBLE)
                        count[0] = 0;
                    }

                    if (nActiveSites == 0) {
                        writePtr = NULL;
                    } else {
                        writePtr = (void *) p_OUTValPtr;

                        varType = OutDom->netCDFOutput
                                      .outputVarInfo[key][varNum][OUTPUT_TYPE];
                        if (Str_CompareI(varType, (char *) "double") != 0) {
                            scale_factor =
                                OutDom->netCDFOutput.scaleFactors[key][varNum];
                            add_offset =
                                OutDom->netCDFOutput.addOffsets[key][varNum];

                            pack_output_values(
                                varType,
                                numElem,
                                scale_factor,
                                add_offset,
                                p_OUTValPtr,
                                (short *) tempShortVals,
                                (int *) tempIntVals
                            );

                            if (Str_CompareI(varType, (char *) "short") == 0) {
                                writePtr = (void *) tempShortVals;
                            } else {
                                writePtr = (void *) tempIntVals;
                            }
                        }
                    }

                    /* For current variable x output period,
                        write out all values across vegtypes and soil layers
                        (if any) for current time-chunk
                    */
                    SW_NC_write_vals(
                        &varID,
                        currFileID,
                        NULL,
                        writePtr,
                        start,
                        count,
                        LogInfo
                    );

                    /*
                        Sync after every write to decrease the likelihood
                        of a deadlock due to parallel coordination done by
                        the netCDF-C library
                    */
                    nc_sync(currFileID);
                    checkReturn(LogInfo->stopRun);
                }

                if (startFile != finalFile) {
                    startTime = 0;
                    if (fileNum == finalFile) {
                        startTime = totTimeSize;
                    }
                }

                totTimeSize -= timeSize;
                startTimeIndex += timeSize;
            }

            if (irow_OUT[key][pd] + 1 == OutDom->nrow_OUT[key][pd]) {
                OutDom->netCDFOutput.runOutFileIndex[key][pd] = finalFile;

                // If the write goes to the very last time step of a file,
                // increment the index to the next file to start next write

                // If it's the last file of the simulation, we will skip any
                // more writes because <start file index> == <number of output
                // files in key/pd>
                if (newStartIndices[key][pd] == 0) {
                    OutDom->netCDFOutput.runOutFileIndex[key][pd]++;
                }
            }

            OutDom->netCDFOutput.outTempStart[key][pd] =
                newStartIndices[key][pd];
        }
    }

#if !defined(SWUDUNITS)
    (void) countTotal;
#endif
}

/**
@brief Deconstruct output netCDF information

@param[in,out] SW_netCDFOut Constant netCDF output file information
*/
void SW_NCOUT_deconstruct(SW_NETCDF_OUT *SW_netCDFOut) {

    int index;
    const int numFreeVars = 20;
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
        SW_netCDFOut->crs_projsc.units,
        SW_netCDFOut->geo_XAxisName,
        SW_netCDFOut->geo_YAxisName,
        SW_netCDFOut->proj_XAxisName,
        SW_netCDFOut->proj_YAxisName,
        SW_netCDFOut->siteName,
        SW_netCDFOut->crs_geogsc.crs_name,
        SW_netCDFOut->crs_projsc.crs_name
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

@param[in] startYr Start year of the simulation
@param[in,out] SW_netCDFOut Constant netCDF output file information
@param[in,out] SW_PathInputs Struct holding all information about the programs
    path/files
@param[out] LogInfo Holds information on warnings and errors
*/
void SW_NCOUT_read_atts(
    TimeInt startYr,
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

        "geo_crs_name",
        "geo_long_name",
        "geo_grid_mapping_name",
        "geo_crs_wkt",
        "geo_longitude_of_prime_meridian",
        "geo_semi_major_axis",
        "geo_inverse_flattening",

        "proj_crs_name",
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
        "deflateLevel",
        "geo_XAxisName",
        "geo_YAxisName",
        "proj_XAxisName",
        "proj_YAxisName",
        "siteName",
        "posTimeInBnds",
        "posVerticalInBnds"
    };
    static const Bool requiredKeys[NUM_ATT_IN_KEYS] = {
        swTRUE,  swTRUE,  swTRUE,  swFALSE, swFALSE, swTRUE,  swTRUE,  swTRUE,
        swTRUE,  swTRUE,  swTRUE,  swTRUE,  swTRUE,  swTRUE,  swFALSE, swFALSE,
        swFALSE, swFALSE, swFALSE, swFALSE, swFALSE, swFALSE, swFALSE, swFALSE,
        swFALSE, swFALSE, swFALSE, swFALSE, swTRUE,  swTRUE,  swTRUE,  swTRUE,
        swTRUE,  swTRUE,  swFALSE, swFALSE
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

    MyFileName = SW_PathInputs->txtInFiles[eNCInAtt];
    f = OpenFile(MyFileName, "r", LogInfo);
    if (LogInfo->stopRun) {
        LogError(
            LogInfo,
            LOGERROR,
            "Could not open the required file %s",
            SW_PathInputs->txtInFiles[eNCInAtt]
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
        doIntConv = (Bool) ((keyID >= 25 && keyID <= 29) ||
                            (keyID >= 35 && keyID <= 36));
        doDoubleConv = (Bool) ((keyID >= 10 && keyID <= 12) ||
                               (keyID >= 17 && keyID <= 19) ||
                               (keyID >= 23 && keyID <= 24));

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
            SW_netCDFOut->crs_geogsc.crs_name = Str_Dup(value, LogInfo);
            break;
        case 7:
            SW_netCDFOut->crs_geogsc.long_name = Str_Dup(value, LogInfo);
            geoCRSFound = swTRUE;
            break;
        case 8:
            SW_netCDFOut->crs_geogsc.grid_mapping_name =
                Str_Dup(value, LogInfo);
            break;
        case 9:
            SW_netCDFOut->crs_geogsc.crs_wkt = Str_Dup(value, LogInfo);
            break;
        case 10:
            SW_netCDFOut->crs_geogsc.longitude_of_prime_meridian =
                inBufdoubleRes;
            break;
        case 11:
            SW_netCDFOut->crs_geogsc.semi_major_axis = inBufdoubleRes;
            break;
        case 12:
            SW_netCDFOut->crs_geogsc.inverse_flattening = inBufdoubleRes;
            break;
        case 13:
            SW_netCDFOut->crs_projsc.crs_name = Str_Dup(value, LogInfo);
            break;
        case 14:
            SW_netCDFOut->crs_projsc.long_name = Str_Dup(value, LogInfo);
            projCRSFound = swTRUE;
            break;
        case 15:
            SW_netCDFOut->crs_projsc.grid_mapping_name =
                Str_Dup(value, LogInfo);
            break;
        case 16:
            SW_netCDFOut->crs_projsc.crs_wkt = Str_Dup(value, LogInfo);
            break;
        case 17:
            SW_netCDFOut->crs_projsc.longitude_of_prime_meridian =
                inBufdoubleRes;
            break;
        case 18:
            SW_netCDFOut->crs_projsc.semi_major_axis = inBufdoubleRes;
            break;
        case 19:
            SW_netCDFOut->crs_projsc.inverse_flattening = inBufdoubleRes;
            break;
        case 20:
            SW_netCDFOut->crs_projsc.datum = Str_Dup(value, LogInfo);
            break;
        case 21:
            SW_netCDFOut->crs_projsc.units = Str_Dup(value, LogInfo);
            break;
        case 22:
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
        case 23:
            SW_netCDFOut->crs_projsc.longitude_of_central_meridian =
                inBufdoubleRes;
            break;
        case 24:
            SW_netCDFOut->crs_projsc.latitude_of_projection_origin =
                inBufdoubleRes;
            break;
        case 25:
            SW_netCDFOut->crs_projsc.false_easting = inBufintRes;
            break;
        case 26:
            SW_netCDFOut->crs_projsc.false_northing = inBufintRes;
            break;
        case 27:
            if (!infVal) {
                SW_netCDFOut->strideOutYears = inBufintRes;

                if (SW_netCDFOut->strideOutYears <= 0) {
                    LogError(
                        LogInfo, LOGERROR, "The value for 'strideOutYears' <= 0"
                    );
                    goto closeFile;
                }
            } else {
                SW_netCDFOut->strideOutYears = -1;
            }
            break;
        case 28:
            SW_netCDFOut->baseCalendarYear = inBufintRes;
            break;
        case 29:
            SW_netCDFOut->deflateLevel = inBufintRes;
            break;
        case 30:
            SW_netCDFOut->geo_XAxisName = Str_Dup(value, LogInfo);
            break;
        case 31:
            SW_netCDFOut->geo_YAxisName = Str_Dup(value, LogInfo);
            break;
        case 32:
            SW_netCDFOut->proj_XAxisName = Str_Dup(value, LogInfo);
            break;
        case 33:
            SW_netCDFOut->proj_YAxisName = Str_Dup(value, LogInfo);
            break;
        case 34:
            SW_netCDFOut->siteName = Str_Dup(value, LogInfo);
            break;
        case 35:
            SW_netCDFOut->posTimeInBnds = inBufintRes;
            break;
        case 36:
            SW_netCDFOut->posVerticalInBnds = inBufintRes;
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
            SW_PathInputs->txtInFiles[eNCInAtt],
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
            SW_PathInputs->txtInFiles[eNCInAtt]
        );
        goto closeFile;
    }

    if ((TimeInt) SW_netCDFOut->baseCalendarYear > startYr) {
        LogError(
            LogInfo,
            LOGERROR,
            "Option 'baseCalendarYear' cannot be greater than start year."
        );
        goto closeFile;
    }

    if (SW_netCDFOut->posTimeInBnds != COORDS_AT_LEFTBOUND &&
        SW_netCDFOut->posTimeInBnds != COORDS_AT_MIDPOINT &&
        SW_netCDFOut->posTimeInBnds != COORDS_AT_RIGHTBOUND) {
        LogError(
            LogInfo,
            LOGERROR,
            "Option 'posTimeInBnds' must select a position at "
            "the start (-1), midpoint (0), or end (1) of time cells "
            "but the value is %d.",
            SW_netCDFOut->posTimeInBnds
        );
        goto closeFile;
    }

    if (SW_netCDFOut->posVerticalInBnds != COORDS_AT_LEFTBOUND &&
        SW_netCDFOut->posVerticalInBnds != COORDS_AT_MIDPOINT &&
        SW_netCDFOut->posVerticalInBnds != COORDS_AT_RIGHTBOUND) {
        LogError(
            LogInfo,
            LOGERROR,
            "Option 'posVerticalInBnds' must select a position at "
            "the top (-1), midpoint (0), or bottom (1) of soil layer cells "
            "but the value is %d.",
            SW_netCDFOut->posVerticalInBnds
        );
        goto closeFile;
    }


    SW_netCDFOut->coordinate_system =
        (SW_netCDFOut->primary_crs_is_geographic) ?
            Str_Dup(SW_netCDFOut->crs_geogsc.long_name, LogInfo) :
            Str_Dup(SW_netCDFOut->crs_projsc.long_name, LogInfo);

closeFile: { CloseFile(&f, LogInfo); }
}

/**
@brief Calculate
    1) Total number of characters in dynamically allocated arrays
       found within SW_PATH_OUTPUTS
    2) Total size of each enabled output variable for a single site
    3) Total size of SW_NETCDF_OUT

@param[in] SW_Domain Struct of type SW_DOMAIN holding constant
    temporal/spatial information for a set of simulation runs

@return Estimated required memory for output information that's netCDF-related
*/
size_t SW_NCOUT_calc_output_sizes(SW_DOMAIN *SW_Domain) {
    SW_OUT_DOM *OutDom = &SW_Domain->OutDom;
    SW_OUT_RUN *OutRun = &SW_Domain->SW_ConstInfo.OutRun;
    SW_PATH_OUTPUTS *SW_PathOutputs = &SW_Domain->SW_ConstInfo.SW_PathOutputs;

    const IntU numOutFiles = SW_PathOutputs->numOutFiles;
    const size_t nSites = SW_Domain->nSitesInSubDom;
    const char *outPrefix = SW_Domain->SW_PathInputs.outputPrefix;
    const size_t maxOutBufferLen = 10;

    int outKey;
    int outVar;
    size_t largestPOUT = 0;
    OutPeriod outPd;

    size_t totSize = 0;

    // Note: colnames_OUT within SW_OUT_DOM is not allocated in SWNETCDF mode

    ForEachOutKey(outKey) {
        // reqOutputVars
        totSize +=
            (OutDom->nvar_OUT[outKey] *
             sizeof(SW_Domain->OutDom.netCDFOutput.reqOutputVars[outKey]));

        if (!OutDom->use[outKey]) {
            continue;
        }

        ForEachOutPeriod(outPd) {
            if (!OutDom->use_OutPeriod[outPd]) {
                continue;
            }

            // Estimate the output string lengths
            totSize +=
                (sizeof(char) * (strlen(key2str[outKey]) + 1) * numOutFiles);
            totSize += (sizeof(char) * (strlen(outPrefix) + 1) * numOutFiles);
            totSize += (sizeof(char) * maxOutBufferLen * numOutFiles);
            totSize += (sizeof(char) * (strlen(pd2longstr[outPd]) + 1));

            // Total memory for each output variable
            totSize +=
                (sizeof(double) * ((OutRun->nP_OUT[outKey][outPd] + 1) * nSites)
                );
            largestPOUT = (OutRun->nP_OUT[outKey][outPd] > largestPOUT) ?
                              OutRun->nP_OUT[outKey][outPd] :
                              largestPOUT;

            // Number of output file IDs in output key/pd
            totSize += (sizeof(int) * numOutFiles);
        }

        for (outVar = 0; outVar < OutDom->nvar_OUT[outKey]; outVar++) {
            if (OutDom->netCDFOutput.reqOutputVars[outKey][outVar]) {
                // Include add_offset and scale_factor sizes
                totSize += (sizeof(double) * 2);
            }
        }

        // Number of variables within output key
        totSize += (sizeof(int) * OutDom->nvar_OUT[outKey]);
    }

    // Number of output file sizes
    totSize += (sizeof(size_t) * OutDom->used_OUTNPERIODS * numOutFiles);

    // Size of each temporary packed output value arrays
    totSize += (sizeof(short) * largestPOUT);
    totSize += (sizeof(int) * largestPOUT);

    return totSize;
}
