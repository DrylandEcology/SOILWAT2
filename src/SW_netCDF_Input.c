/* =================================================== */
/*                INCLUDES / DEFINES                   */
/* --------------------------------------------------- */
#include "include/SW_netCDF_Input.h"   // for SW_NCIN_soilProfile, ...
#include "include/filefuncs.h"         // for LogError, FileExists, CloseFile
#include "include/generic.h"           // for Bool, swFALSE, LOGERROR, swTRUE
#include "include/myMemory.h"          // for Str_Dup, Mem_Malloc
#include "include/SW_datastructs.h"    // for LOG_INFO, SW_NETCDF_OUT, SW_DOMAIN
#include "include/SW_Defines.h"        // for MAX_FILENAMESIZE, OutPeriod
#include "include/SW_Domain.h"         // for SW_DOM_calc_ncSuid
#include "include/SW_Files.h"          // for eNCInAtt, eNCIn, eNCOutVars
#include "include/SW_netCDF_General.h" // for vNCdom, vNCprog
#include "include/SW_Output.h"         // for ForEachOutKey, SW_ESTAB, pd2...
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

/** Progress status: SUID is ready for simulation */
#define PRGRSS_READY ((signed char) 0)

/** Progress status: SUID has successfully been simulated */
#define PRGRSS_DONE ((signed char) 1)

/** Progress status: SUID failed to simulate */
#define PRGRSS_FAIL ((signed char) -1)

#define NIN_VAR_INPUTS 21

/* Columns of interest, and excludes:
    - Input key and input name
    - "do input" flags in value
    - Input file name/pattern
    - St years and stride years start
    - Calendar override
    - User comment */
#define NUM_INPUT_INFO 14

/* Maximum number of variables per input key */
#define SW_INNMAXVARS 22

/* Indices within `inWeathStrideInfo` for stride year and stride start */
#define SW_INSTRIDEYR 0
#define SW_INSTRIDESTART 1

static const char *const expectedColNames[] = {
    "SW2 input group",    "SW2 variable",         "SW2 units",
    "Do nc-input?",       "ncFileName",           "ncVarName",
    "ncVarUnits",         "ncDomainType",         "ncSiteName",
    "ncCRSName",          "ncCRSGridMappingName", "ncXAxisName",
    "ncYAxisName",        "ncZAxisName",          "ncTAxisName",
    "ncStrideYears",      "ncStrideStart",        "ncStridePattern",
    "ncCalendarOverride", "ncVAxisName",          "Comment"
};

/* This array and `possVarNames` must line up the variables within each key */
static const char *const swInVarUnits[SW_NINKEYSNC][SW_INNMAXVARS] =
    {
        {"1", "1"},                      /* inDomain */
        {"degree_north", "degree_east"}, /* inSpatial */
        {"1", "m", "degree", "degree"},  /* inTopo */
        {"1",        "cm", "cm",   "g cm-3", "cm3 cm-3", "g g-1", "g g-1",
         "g g-1", /*inSoil*/
         "cm3 cm-3", "1",  "degC", "1",      "1",        "1",     "1",
         "1",        "1",  "1",    "1",      "1",        "1",     "1"},
        {"1",      "m2 m-2", "m2 m-2", "g m-2", "g m-2",
         "1",      "g m-2", /*inVeg*/
         "m2 m-2", "g m-2",  "g m-2",  "1",     "g m-2",
         "m2 m-2", "g m-2",  "g m-2",  "1",     "g m-2",
         "m2 m-2", "g m-2",  "g m-2",  "1",     "g m-2"},
        {"1", /* inWeather */
         "degC",
         "degC",
         "cm",
         "%",
         "m s-1",
         "m s-1",
         "m s-1",
         "%",
         "%",
         "%",
         "%",
         "degC",
         "kPa",
         "m s-1"},
        {"1", "%", "m s-1", "%", "kg m-3", "1"} /* inClimate */
};

static const char *const possVarNames[SW_NINKEYSNC][SW_INNMAXVARS] = {
    {/* inDomain */
     "domain",
     "progress"
    },

    {/* inSpatial */
     "latitude",
     "longitude"
    },

    {/* inTopo */
     "indexSpatial",
     "elevation",
     "slope",
     "aspect"
    },

    {
        /* inSoil */
        "indexSpatial",
        "depth",
        "width",
        "soilDensityInput",
        "fractionVolBulk_gravel",
        "fractionWeightMatric_sand",
        "fractionWeightMatric_clay",
        "fractionWeightMatric_silt",
        "soc",
        "impermeability",
        "avgLyrTempInit",
        "evap_coeff",

        "Trees.transp_coeff",
        "Shrubs.transp_coeff",
        "Forbs.transp_coeff",
        "Grasses.transp_coeff",

        "swrcp[1]",
        "swrcp[2]",
        "swrcp[3]",
        "swrcp[4]",
        "swrcp[5]",
        "swrcp[6]",
    },

    {/* inVeg */
     "indexSpatial",     "bareGround.fCover",

     "Trees.fCover",     "Trees.litter",      "Trees.biomass",
     "Trees.pct_live",   "Trees.lai_conv",

     "Shrubs.fCover",    "Shrubs.litter",     "Shrubs.biomass",
     "Shrubs.pct_live",  "Shrubs.lai_conv",

     "Forbs.fCover",     "Forbs.litter",      "Forbs.biomass",
     "Forbs.pct_live",   "Forbs.lai_conv",

     "Grasses.fCover",   "Grasses.litter",    "Grasses.biomass",
     "Grasses.pct_live", "Grasses.lai_conv"
    },

    {/* inWeather */
     "indexSpatial",
     "temp_max",
     "temp_min",
     "ppt",
     "cloudcov",
     "windspeed",
     "windspeed_east",
     "windspeed_north",
     "r_humidity",
     "rmax_humidity",
     "rmin_humidity",
     "spec_humidity",
     "temp_dewpoint",
     "actualVaporPressure",
     "shortWaveRad"
    },

    {/* inClimate */
     "indexSpatial",
     "cloudcov",
     "windspeed",
     "r_humidity",
     "snow_density",
     "n_rain_per_day"
    }
};

static const char *const generalVegNames[] = {
    "<veg>.fCover",
    "<veg>.litter",
    "<veg>.biomass",
    "<veg>.pct_live",
    "<veg>.lai_conv"
};

static const char *const generalSoilNames[] = {"<veg>.transp_coeff"};

static const char *const possInKeys[] = {
    "inDomain",
    "inSpatial",
    "inTopo",
    "inSoil",
    "inVeg",
    "inWeather",
    "inClimate"
};

/* =================================================== */
/*             Local Function Definitions              */
/* --------------------------------------------------- */

/*
@brief Translate an input keys into indices the program can understand

@param[in] varKey Read-in variable key/category from input file
@param[in] varName Read-in variable SW2 variable name from input file
@param[out] inKey Translated key from read-in values to local arrays
@param[out] inVarNum Translated key from read-in values to local arrays
@param[out] isIndex Specifies that the read-in values were related to an index
file
@param[out] isAllVegVar Specifies that the read-in values were a generalization
of veg or soil variables
*/
static void get_2d_input_key(
    char *varKey,
    char *varName,
    int *inKey,
    int *inVarNum,
    Bool *isIndex,
    Bool *isAllVegVar
) {

    int keyNum;
    int varNum;
    const int numGeneralVegNames = 5;

    *inKey = eSW_NoInKey;
    *inVarNum = KEY_NOT_FOUND;
    *isIndex = swFALSE;
    *isAllVegVar = swFALSE;

    for (keyNum = 0; keyNum < SW_NINKEYSNC && *inKey == eSW_NoInKey; keyNum++) {
        if (strcmp(varKey, possInKeys[keyNum]) == 0) {
            *inKey = keyNum;
        }
    }

    if (*inKey != eSW_NoInKey) {
        for (varNum = 0; varNum < numVarsInKey[*inKey]; varNum++) {
            if (strcmp(possVarNames[*inKey][varNum], varName) == 0) {
                if (varNum == 0 && keyNum != eSW_InDomain &&
                    keyNum != eSW_InSpatial) {

                    *isIndex = swTRUE;
                }

                *inVarNum = varNum;
                return;
            }
        }

        if (*inKey == eSW_InVeg) {
            for (varNum = 0; varNum < numGeneralVegNames; varNum++) {
                if (strcmp(generalVegNames[varNum], varName) == 0) {
                    *isAllVegVar = swTRUE;
                    *inVarNum = varNum;
                    return;
                }
            }
        } else if (*inKey == eSW_InSoil) {
            if (strcmp(generalSoilNames[0], varName) == 0) {
                *isAllVegVar = swTRUE;
                *inVarNum = 0;
            }
        }
    }
}

/**
@brief Test to see if the information grid and spatial information
provided in `desc_nc.in` is consistent with that of what is provided in
the input spreadsheet

@param[in] SW_netCDFOut Constant netCDF output file information
@param[in] inputInfo Attribute information for a specific input variable
@param[out] LogInfo Holds information on warnings and errors
*/
static void check_domain_information(
    SW_NETCDF_OUT *SW_netCDFOut, char **inputInfo, LOG_INFO *LogInfo
) {
    Bool primCRSIsGeo = SW_netCDFOut->primary_crs_is_geographic;
    char *ncCRSGridMapName = inputInfo[INGRIDMAPPING];
    char *ncYAxisName = inputInfo[INYAXIS];
    char *ncXAxisName = inputInfo[INXAXIS];
    char *ncCRSName = inputInfo[INCRSNAME];
    char *geoGridMapName = SW_netCDFOut->crs_geogsc.grid_mapping_name;
    char *projGridMapName = SW_netCDFOut->crs_projsc.grid_mapping_name;
    char *geoYAxisName = SW_netCDFOut->geo_YAxisName;
    char *geoXAxisName = SW_netCDFOut->geo_XAxisName;
    char *projYAxisName = SW_netCDFOut->proj_YAxisName;
    char *projXAxisName = SW_netCDFOut->proj_XAxisName;
    char *siteName = SW_netCDFOut->siteName;

    Bool incorrGeo =
        (Bool) (primCRSIsGeo &&
                (strcmp(ncCRSGridMapName, "latitude_longitude") != 0 ||
                 strcmp(ncCRSGridMapName, geoGridMapName) != 0 ||
                 strcmp(ncYAxisName, geoYAxisName) != 0 ||
                 strcmp(ncXAxisName, geoXAxisName) != 0));

    Bool incorrProj =
        (Bool) (!primCRSIsGeo &&
                (strcmp(ncCRSGridMapName, "latitude_longitude") == 0 ||
                 strcmp(ncCRSGridMapName, projGridMapName) != 0 ||
                 strcmp(ncYAxisName, projYAxisName) != 0 ||
                 strcmp(ncXAxisName, projXAxisName) != 0));

    /* Test that the following columns are consistent when testing the domain
       input:
       - ncCRSGridMappingName, ncXAxisName, ncYAxisName
       - ncCRSName
       - ncSiteName */
    if ((incorrGeo && primCRSIsGeo) || (incorrProj && !primCRSIsGeo)) {
        LogError(
            LogInfo,
            LOGERROR,
            "The geographical spatial information provided for "
            "'ncCRSGridMappingName', 'ncXAxisName', and 'ncYAxisName' "
            "do not match expected values for a %s domain.",
            (incorrGeo) ? "geographical" : "projected"
        );
    } else if ((!primCRSIsGeo && strcmp(ncCRSName, "crs_geogsc") == 0) ||
               (primCRSIsGeo && strcmp(ncCRSName, "crs_projsc") == 0)) {
        LogError(
            LogInfo,
            LOGERROR,
            "Mismatch column 'ncCRSName' value compared to the primary "
            "CRS found in `desc_nc.in`."
        );
    } else if (strcmp(siteName, inputInfo[INSITENAME]) != 0) {
        LogError(
            LogInfo,
            LOGERROR,
            "Site name of '%s' in `desc_nc.in` does not match that given "
            "in the input spreadsheet ('%s').",
            siteName,
            inputInfo[INSITENAME]
        );
    }
}

/**
@brief Check that the required variable information was input through
the input variable file

@param[in] inputInfo Attribute information for a specific input variable
@param[in] inWeathStrideInfo List of stride information for weather variables
@param[in] key Current category of input variables being tested
@param[in] varNum Variable number within the given key
@param[out] LogInfo Holds information on warnings and errors
*/
static void check_variable_for_required(
    char ***inputInfo,
    const int inWeathStrideInfo[],
    int key,
    int varNum,
    LOG_INFO *LogInfo
) {

    int attNum;
    int mustTestAttInd[] = {
        INNCVARNAME,
        INDOMTYPE,
        INSITENAME,
        INCRSNAME,
        INGRIDMAPPING,
        INXAXIS,
        INYAXIS
    };
    const char *mustTestAttNames[] = {
        "ncVarName",
        "ncDomType",
        "ncSiteName",
        "ncCRSName",
        "ncCRSGridMappingName",
        "ncXAxisName",
        "ncYAxisName"
    };
    int mustTestAtts = 7;
    int testInd;

    /* Indices are based on the global array `possVarNames` under `inVeg` */
    Bool isLitter =
        (Bool) (varNum == 3 || varNum == 8 || varNum == 13 || varNum == 18);
    Bool isBio =
        (Bool) (varNum == 4 || varNum == 9 || varNum == 14 || varNum == 19);
    Bool isPctLive =
        (Bool) (varNum == 5 || varNum == 10 || varNum == 15 || varNum == 20);
    Bool isLAI =
        (Bool) (varNum == 6 || varNum == 11 || varNum == 16 || varNum == 21);

    Bool testVeg = swFALSE;
    Bool canBeNA;

    Bool isIndex = (Bool) (key > eSW_InSpatial && varNum == 0);
    Bool inputDomIsSite =
        (Bool) (strcmp(inputInfo[varNum][INDOMTYPE], "s") == 0);

    char *varName = inputInfo[varNum][INNCVARNAME];

    /* Make sure that the universally required attributes are filled in
       skip the testing of the nc var units (can be NA) */
    for (attNum = 0; attNum < mustTestAtts; attNum++) {
        testInd = mustTestAttInd[attNum];
        canBeNA = (Bool) (testInd == INSITENAME && !inputDomIsSite);

        if (!canBeNA && strcmp(inputInfo[varNum][testInd], "NA") == 0) {
            LogError(
                LogInfo,
                LOGERROR,
                "The input column '%s' contains a value of 'NA' for the "
                "NC variable name '%s'. This is a required column and "
                "must have information.",
                mustTestAttNames[attNum],
                varName
            );
            return; /* Exit function prematurely due to error */
        }
    }

    if (!isIndex) {
        testVeg = (Bool) (key == eSW_InVeg &&
                          (isLitter || isBio || isPctLive || isLAI));

        if (key == eSW_InSoil) {
            if (strcmp(inputInfo[varNum][INZAXIS], "NA") == 0) {
                LogError(
                    LogInfo,
                    LOGERROR,
                    "The input column '%s' contains a value of 'NA' for the "
                    "NC variable name '%s'. This is a required column and "
                    "must have information.",
                    "ncZAxisName",
                    varName
                );
                return; /* Exit function prematurely due to error */
            }
        } else if (key == eSW_InWeather || key == eSW_InClimate || testVeg) {
            if (strcmp(inputInfo[varNum][INTAXIS], "NA") == 0) {
                LogError(
                    LogInfo,
                    LOGERROR,
                    "The input column '%s' contains a value of 'NA' for the "
                    "NC variable name '%s'. This is a required column and "
                    "must have information.",
                    "ncTAxisName",
                    varName
                );
                return; /* Exit function prematurely due to error */
            }
        }

        if (key == eSW_InWeather) {
            if (inWeathStrideInfo[SW_INSTRIDEYR] > -1) {
                if (inWeathStrideInfo[SW_INSTRIDESTART] == -1 ||
                    strcmp(inputInfo[varNum][INSTPATRN], "NA") == 0) {

                    LogError(
                        LogInfo,
                        LOGERROR,
                        "The input column 'ncStrideStart' and/or "
                        "'ncStridePattern' "
                        "contains a value of 'NA' for the NC variable name "
                        "'%s'. "
                        "These are required columns when 'ncStrideYears' is "
                        "not "
                        "'Inf' and must have information.",
                        varName
                    );
                    return; /* Exit function prematurely due to error */
                }
            }
        }
    }
}

/**
@brief Given a certain input key, make sure that specified input columns
are the same throughout each active input variable

@param[in] inputInfo Attribute information for all input variables
@param[in] readInVars Specifies which variables are to be read-in as input
@param[out] LogInfo Holds information on warnings and errors
*/
static void check_inputkey_columns(
    char ***inputInfo, const Bool readInVars[], int key, LOG_INFO *LogInfo
) {

    int compIndex = -1;
    int varNum;
    int varStart = (key > eSW_InSpatial) ? 1 : 0;
    int numVars = numVarsInKey[key];
    int attStart = INDOMTYPE;
    int attEnd = INVAXIS;
    int attNum;

    char *cmpAtt = NULL;
    char *currAtt = NULL;

    Bool ignoreAtt;

    for (varNum = varStart; varNum < numVars; varNum++) {
        if (readInVars[varNum + 1]) {
            if (compIndex == -1) {
                compIndex = varNum;
            } else {
                ignoreAtt = swFALSE;

                for (attNum = attStart; attNum <= attEnd; attNum++) {
                    currAtt = inputInfo[varNum][attNum];
                    cmpAtt = inputInfo[compIndex][attNum];

                    /* Ignore the checking of the V-axis of certain variables
                       in the input keys `eSW_InVeg` and `SW_InSoil` since
                       they were already checked in `SW_NCIN_read_input_vars()`
                    */
                    if (key == eSW_InVeg) {
                        ignoreAtt = (Bool) ((varNum > 1 && attNum == attEnd) ||
                                            (attNum == INTAXIS));
                    } else if (key == eSW_InSoil) {
                        ignoreAtt = (Bool) (varNum >= 12 && varNum <= 15 &&
                                            attNum == attEnd);
                    }

                    if (!ignoreAtt && strcmp(currAtt, cmpAtt) != 0) {
                        LogError(
                            LogInfo,
                            LOGERROR,
                            "The variable '%s' within the input key '%s' "
                            "has a column that does not match the others "
                            "from 'ncGridType' to 'ncVAxisName' with a "
                            "value of '%s' instead of '%s'.",
                            inputInfo[varNum][INNCVARNAME],
                            possInKeys[key],
                            currAtt,
                            cmpAtt
                        );
                        return; /* Exit function prematurely due to error */
                    }
                }
            }
        }
    }
}

/**
@brief Wrapper function to test all input variables for required
input columns and the same values for input columns within a given input key

@param[in] SW_netCDFOut Constant netCDF output file information
@param[in] inputInfo Attribute information for all input variables
@param[in] inWeathStrideInfo List of stride information for weather variables
@param[in] readInVars Specifies which variables are to be read-in as input
@param[out] LogInfo Holds information on warnings and errors
*/
static void check_input_variables(
    SW_NETCDF_OUT *SW_netCDFOut,
    char ****inputInfo,
    int inWeathStrideInfo[],
    Bool *readInVars[],
    LOG_INFO *LogInfo
) {
    int key;
    int varNum;
    int testVarIndex;
    int varStart;

    ForEachNCInKey(key) {
        if (readInVars[key][0]) {
            testVarIndex = -1;
            varStart = (key > eSW_InSpatial) ? 1 : 0;

            for (varNum = varStart; varNum < numVarsInKey[key]; varNum++) {
                if (readInVars[key][varNum + 1]) {
                    if (testVarIndex == -1) {
                        testVarIndex = varNum;
                    }

                    check_variable_for_required(
                        inputInfo[key], inWeathStrideInfo, key, varNum, LogInfo
                    );
                    if (LogInfo->stopRun) {
                        /* Exit function prematurely due to failed test */
                        return;
                    }
                }
            }

            check_inputkey_columns(
                inputInfo[key], readInVars[key], key, LogInfo
            );
            if (LogInfo->stopRun) {
                return; /* Exit function prematurely due to failed test */
            }

            if (key == eSW_InDomain) {
                check_domain_information(
                    SW_netCDFOut, inputInfo[key][testVarIndex], LogInfo
                );
                if (LogInfo->stopRun) {
                    return; /* Exit function prematurely due to failed test */
                }
            }
        }
    }
}

/**
@brief Helper function to `fill_domain_netCDF_vars()` to allocate
memory for writing out values

@param[in] domTypeIsSite Specifies if the domain type is "s"
@param[in] nSUIDs Number of sites or gridcells
@param[in] numY Number of values along the Y-axis (lat or y)
@param[in] numX Number of values along the X-axis (lon or x)
@param[out] valsY Array to hold coordinate values for the Y-axis (lat or y)
@param[out] valsX Array to hold coordinate values for the X-axis (lon or x)
@param[out] valsYBnds Array to hold coordinate values for the
    bounds of the Y-axis (lat_bnds or y_bnds)
@param[out] valsXBnds Array to hold coordinate values for the
    bounds of the X-axis (lon_bnds or x_bnds)
@param[out] domVals Array to hold all values for the domain variable
@param[out] LogInfo Holds information on warnings and errors
*/
static void alloc_netCDF_domain_vars(
    Bool domTypeIsSite,
    unsigned long nSUIDs,
    unsigned int numY,
    unsigned int numX,
    double **valsY,
    double **valsX,
    double **valsYBnds,
    double **valsXBnds,
    unsigned int **domVals,
    LOG_INFO *LogInfo
) {

    double **vars[] = {valsY, valsX};
    double **bndsVars[] = {valsYBnds, valsXBnds};
    const unsigned int numVars = 2;
    const unsigned int numBnds = 2;
    unsigned int varNum;
    unsigned int bndVarNum;
    unsigned int numVals;

    for (varNum = 0; varNum < numVars; varNum++) {
        numVals = (varNum % 2 == 0) ? numY : numX;
        *(vars[varNum]) = (double *) Mem_Malloc(
            numVals * sizeof(double), "alloc_netCDF_domain_vars()", LogInfo
        );
        if (LogInfo->stopRun) {
            return; // Exit function prematurely due to error
        }
    }

    if (!domTypeIsSite) {
        for (bndVarNum = 0; bndVarNum < numBnds; bndVarNum++) {
            numVals = (bndVarNum % 2 == 0) ? numY : numX;

            *(bndsVars[bndVarNum]) = (double *) Mem_Malloc(
                (size_t) (numVals * numBnds) * sizeof(double),
                "alloc_netCDF_domain_vars()",
                LogInfo
            );

            if (LogInfo->stopRun) {
                return; // Exit function prematurely due to error
            }
        }
    }

    *domVals = (unsigned int *) Mem_Malloc(
        nSUIDs * sizeof(unsigned int), "alloc_netCDF_domain_vars()", LogInfo
    );
}

/**
@brief Helper function to `fill_domain_netCDF_vars()` to free memory
that was allocated for the domain netCDF file variable values

@param[out] valsY Array to hold coordinate values for the Y-axis (lat or y)
@param[out] valsX Array to hold coordinate values for the X-axis (lon or x)
@param[out] valsYBnds Array to hold coordinate values for the
    bounds of the Y-axis (lat_bnds or y_bnds)
@param[out] valsXBnds Array to hold coordinate values for the
    bounds of the X-axis (lon_bnds or x_bnds)
@param[out] domVals Array to hold all values for the domain variable
*/
static void dealloc_netCDF_domain_vars(
    double **valsY,
    double **valsX,
    double **valsYBnds,
    double **valsXBnds,
    unsigned int **domVals
) {

    const int numVars = 4;
    double **vars[] = {valsY, valsX, valsYBnds, valsXBnds};
    int varNum;

    for (varNum = 0; varNum < numVars; varNum++) {
        if (!isnull(*vars[varNum])) {
            free(*vars[varNum]);
        }
    }

    if (!isnull(domVals)) {
        free(*domVals);
    }
}

/**
@brief Determine if a given CRS name is wgs84

@param[in] crs_name Name of the CRS to test
*/
static Bool is_wgs84(char *crs_name) {
    const int numPosSyns = 5;
    static char *wgs84_synonyms[] = {
        (char *) "WGS84",
        (char *) "WGS 84",
        (char *) "EPSG:4326",
        (char *) "WGS_1984",
        (char *) "World Geodetic System 1984"
    };

    for (int index = 0; index < numPosSyns; index++) {
        if (Str_CompareI(crs_name, wgs84_synonyms[index]) == 0) {
            return swTRUE;
        }
    }

    return swFALSE;
}

/**
@brief Fill horizontal coordinate variables, "domain"
(or another user-specified name) variable and "sites" (if applicable)
within the domain netCDF

@note A more descriptive explanation of the scenarios to write out the
values of a variable would be (names are user-provided, so the ones below
are not hard-coded)

CRS bounding box - geographic
    - Domain type "xy" (primary CRS - geographic & projected)
        - Write out values for "lat", "lat_bnds", "lon", and "lon_bnds"
    - Domain type "s" (primary CRS - geographic & projected)
        - Write out values for "site", "lat", and "lon"

CRS bounding box - projected
    - Domain type "xy" (primary CRS - geographic)
        - Cannot occur
    - Domain type "xy" (primary CRS - projected)
        - Write out values for "y", "y_bnds", "x", and "x_bnds"
    - Domain type "s" (primary CRS - geographic)
        - Cannot occur
    - Domain type "s" (primary CRS - projected)
        - Write out values for "site", "y", and "x"

While all scenarios fill the domain variable

@param[in] SW_Domain Struct of type SW_DOMAIN holding constant
    temporal/spatial information for a set of simulation runs
@param[in] domFileID Domain netCDF file identifier
@param[in] domID Identifier of the "domain" (or another user-specified name)
    variable
@param[in] siteID Identifier of the "site" variable within the given netCDF
    (if the variable exists)
@param[in] YVarID Variable identifier of the Y-axis horizontal coordinate
    variable (lat or y)
@param[in] XVarID Variable identifier of the X-axis horizontal coordinate
    variable (lon or x)
@param[in] YBndsID Variable identifier of the Y-axis horizontal coordinate
    bounds variable (lat_bnds or y_bnds)
@param[in] XVarID Variable identifier of the X-axis horizontal coordinate
    bounds variable (lon_bnds or x_bnds)
@param[out] LogInfo Holds information on warnings and errors
*/
static void fill_domain_netCDF_vals(
    SW_DOMAIN *SW_Domain,
    int domFileID,
    int domID,
    int siteID,
    int YVarID,
    int XVarID,
    int YBndsID,
    int XBndsID,
    LOG_INFO *LogInfo
) {

    Bool domTypeIsSite = (Bool) (strcmp(SW_Domain->DomainType, "s") == 0);
    unsigned int suidNum;
    unsigned int gridNum = 0;
    unsigned int *domVals = NULL;
    unsigned int bndsIndex;
    double *valsY = NULL;
    double *valsX = NULL;
    double *valsYBnds = NULL;
    double *valsXBnds = NULL;
    size_t start[] = {0, 0};
    size_t domCount[2]; // domCount: 2 - [#lat dim, #lon dim] or [#sites, 0]
    size_t fillCountY[1];
    size_t fillCountX[1];
    size_t fillCountYBnds[] = {SW_Domain->nDimY, 2};
    size_t fillCountXBnds[] = {SW_Domain->nDimX, 2};
    double resY;
    double resX;
    unsigned int numX = (domTypeIsSite) ? SW_Domain->nDimS : SW_Domain->nDimX;
    unsigned int numY = (domTypeIsSite) ? SW_Domain->nDimS : SW_Domain->nDimY;

    int fillVarIDs[4] = {YVarID, XVarID};    // Fill other slots later if needed
    double **fillVals[4] = {&valsY, &valsX}; // Fill other slots later if needed
    size_t *fillCounts[] = {
        fillCountY, fillCountX, fillCountYBnds, fillCountXBnds
    };
    int numVars;
    int varNum;

    alloc_netCDF_domain_vars(
        domTypeIsSite,
        SW_Domain->nSUIDs,
        numY,
        numX,
        &valsY,
        &valsX,
        &valsYBnds,
        &valsXBnds,
        &domVals,
        LogInfo
    );
    if (LogInfo->stopRun) {
        goto freeMem; // Exit function prematurely due to error
    }

    for (suidNum = 0; suidNum < SW_Domain->nSUIDs; suidNum++) {
        domVals[suidNum] = suidNum + 1;
    }

    // --- Setup domain size, number of variables to fill, fill values,
    // fill variable IDs, and horizontal coordinate variables for the section
    // `Write out all values to file`
    if (domTypeIsSite) {
        // Handle variables differently due to domain being 1D (sites)
        fillCountY[0] = SW_Domain->nDimS;
        fillCountX[0] = SW_Domain->nDimS;
        domCount[0] = SW_Domain->nDimS;
        domCount[1] = 0;
        numVars = 2;

        // Sites are filled with the same values as the domain variable
        SW_NC_write_vals(
            &siteID,
            domFileID,
            NULL,
            (void *) domVals,
            start,
            domCount,
            "unsigned int",
            LogInfo
        );

        if (LogInfo->stopRun) {
            goto freeMem; // Exit function prematurely due to error
        }

    } else {
        domCount[1] = fillCountX[0] = SW_Domain->nDimX;
        domCount[0] = fillCountY[0] = SW_Domain->nDimY;
        numVars = 4;

        fillVals[2] = &valsYBnds;
        fillVals[3] = &valsXBnds;
        fillVarIDs[2] = YBndsID;
        fillVarIDs[3] = XBndsID;
    }

    // --- Fill horizontal coordinate values

    // Calculate resolution for y and x and allocate value arrays
    resY = (SW_Domain->max_y - SW_Domain->min_y) / numY;
    resX = (SW_Domain->max_x - SW_Domain->min_x) / numX;

    // Generate/fill the horizontal coordinate variable values
    // These values are within a bounding box given by the user
    // I.e., lat values are within [ymin, ymax] and lon are within [xmin, xmax]
    // bounds are ordered consistently with the associated coordinate
    for (gridNum = 0; gridNum < numX; gridNum++) {
        valsX[gridNum] = SW_Domain->min_x + (gridNum + .5) * resX;

        if (!domTypeIsSite) {
            bndsIndex = gridNum * 2;
            valsXBnds[bndsIndex] = SW_Domain->min_x + gridNum * resX;
            valsXBnds[bndsIndex + 1] = SW_Domain->min_x + (gridNum + 1) * resX;
        }
    }

    for (gridNum = 0; gridNum < numY; gridNum++) {
        valsY[gridNum] = SW_Domain->min_y + (gridNum + .5) * resY;

        if (!domTypeIsSite) {
            bndsIndex = gridNum * 2;
            valsYBnds[bndsIndex] = SW_Domain->min_y + gridNum * resY;
            valsYBnds[bndsIndex + 1] = SW_Domain->min_y + (gridNum + 1) * resY;
        }
    }

    // --- Write out all values to file
    for (varNum = 0; varNum < numVars; varNum++) {
        SW_NC_write_vals(
            &fillVarIDs[varNum],
            domFileID,
            NULL,
            *fillVals[varNum],
            start,
            fillCounts[varNum],
            "double",
            LogInfo
        );

        if (LogInfo->stopRun) {
            goto freeMem; // Exit function prematurely due to error
        }
    }

    // Fill domain variable with SUIDs
    SW_NC_write_vals(
        &domID,
        domFileID,
        NULL,
        domVals,
        start,
        domCount,
        "unsigned integer",
        LogInfo
    );

// Free allocated memory
freeMem:
    dealloc_netCDF_domain_vars(
        &valsY, &valsX, &valsYBnds, &valsXBnds, &domVals
    );
}

/**
@brief Fill the variable "domain" with it's attributes

@param[in] domainVarName User-specified domain variable name
@param[in,out] domID Identifier of the "domain"
    (or another user-specified name) variable
@param[in] domDims Set dimensions for the domain variable
@param[in] readinGeoYName User-provided geographical y-axis name
@param[in] readinGeoXName User-provided geographical x-axis name
@param[in] readinProjYName User-provided projected y-axis name
@param[in] readinProjXName User-provided projected x-axis name
@param[in] siteName User-provided site dimension/variable "site" name
@param[in] domFileID Domain netCDF file identifier
@param[in] nDomainDims Number of dimensions the domain variable will have
@param[in] primCRSIsGeo Specifies if the current CRS type is geographic
@param[in] domType Type of domain in which simulations are running
    (gridcell/sites)
@param[in] deflateLevel Level of deflation that will be used for the created
variable
@param[out] LogInfo Holds information on warnings and errors
*/
static void fill_domain_netCDF_domain(
    const char *domainVarName,
    int *domVarID,
    int domDims[],
    const char *readinGeoYName,
    const char *readinGeoXName,
    const char *readinProjYName,
    const char *readinProjXName,
    const char *siteName,
    int domFileID,
    int nDomainDims,
    Bool primCRSIsGeo,
    const char *domType,
    int deflateLevel,
    LOG_INFO *LogInfo
) {

    char *gridMapVal = (primCRSIsGeo) ?
                           (char *) "crs_geogsc" :
                           (char *) "crs_projsc: %s %s crs_geogsc: %s %s";

    char *coordVal =
        (strcmp(domType, "s") == 0) ? (char *) "%s %s %s" : (char *) "%s %s";

    const char *strAttNames[] = {
        "long_name", "units", "grid_mapping", "coordinates"
    };

    char gridMapStr[MAX_FILENAMESIZE] = "\0";
    char coordStr[MAX_FILENAMESIZE] = "\0";
    const char *strAttVals[] = {"simulation domain", "1", gridMapVal, coordVal};
    unsigned int uintFillVal = NC_FILL_UINT;

    int attNum;
    const int numAtts = 4;

    /* Fill dynamic coordinate names and replace them in `strAttNames` */
    if (strcmp(domType, "s") == 0) {
        snprintf(
            coordStr,
            MAX_FILENAMESIZE,
            coordVal,
            readinGeoYName,
            readinGeoXName,
            siteName
        );
    } else {
        snprintf(
            coordStr, MAX_FILENAMESIZE, coordVal, readinGeoYName, readinGeoXName
        );
    }
    strAttVals[numAtts - 1] = coordStr;

    if (!primCRSIsGeo) {
        snprintf(
            gridMapStr,
            MAX_FILENAMESIZE,
            gridMapVal,
            readinProjXName,
            readinProjYName,
            readinGeoYName,
            readinGeoXName
        );
        strAttVals[numAtts - 2] = gridMapStr;
    }

    SW_NC_create_netCDF_var(
        domVarID,
        domainVarName,
        domDims,
        &domFileID,
        NC_UINT,
        nDomainDims,
        NULL,
        deflateLevel,
        LogInfo
    );

    SW_NC_write_att(
        "_FillValue",
        (void *) &uintFillVal,
        *domVarID,
        domFileID,
        1,
        NC_UINT,
        LogInfo
    );
    if (LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

    // Write all attributes to the domain variable
    for (attNum = 0; attNum < numAtts; attNum++) {
        SW_NC_write_string_att(
            strAttNames[attNum],
            strAttVals[attNum],
            *domVarID,
            domFileID,
            LogInfo
        );

        if (LogInfo->stopRun) {
            return; // Exit function prematurely due to error
        }
    }
}

/**
@brief Fill the domain netCDF file with variables that are for domain type "s"

@param[in] SW_Domain Struct of type SW_DOMAIN holding constant
    temporal/spatial information for a set of simulation runs
@param[in] domFileID Domain netCDF file ID
@param[out] sDimID 'site' dimension identifier
@param[out] sVarID 'site' variable identifier
@param[out] YVarID Variable identifier of the Y-axis horizontal coordinate
    variable (lat or y)
@param[out] XVarID Variable identifier of the X-axis horizontal coordinate
    variable (lon or x)
@param[in] deflateLevel Level of deflation that will be used for the created
variable
@param[out] LogInfo Holds information on warnings and errors
*/
static void fill_domain_netCDF_s(
    SW_DOMAIN *SW_Domain,
    int *domFileID,
    int *sDimID,
    int *sVarID,
    int *YVarID,
    int *XVarID,
    int deflateLevel,
    LOG_INFO *LogInfo
) {

    SW_NETCDF_OUT *netCDFOutput = &SW_Domain->OutDom.netCDFOutput;

    /* Get latitude/longitude names that were read-in from input file */
    char *readinGeoYName = SW_Domain->OutDom.netCDFOutput.geo_YAxisName;
    char *readinGeoXName = SW_Domain->OutDom.netCDFOutput.geo_XAxisName;
    char *readinProjYName = SW_Domain->OutDom.netCDFOutput.proj_YAxisName;
    char *readinProjXName = SW_Domain->OutDom.netCDFOutput.proj_XAxisName;
    char *siteName = SW_Domain->OutDom.netCDFOutput.siteName;

    char *geo_long_name = netCDFOutput->crs_geogsc.long_name;
    char *proj_long_name = netCDFOutput->crs_projsc.long_name;
    Bool primCRSIsGeo = netCDFOutput->primary_crs_is_geographic;
    char *units = netCDFOutput->crs_projsc.units;

    const int numSiteAtt = 3;
    const int numLatAtt = 4;
    const int numLonAtt = 4;
    const int numYAtt = 3;
    const int numXAtt = 3;
    // numVarsToWrite: Do or do not write "x" and "y"
    int numVarsToWrite = (primCRSIsGeo) ? 3 : 5;
    const char *attNames[][4] = {
        {"long_name", "units", "cf_role"},
        {"long_name", "standard_name", "units", "axis"},
        {"long_name", "standard_name", "units", "axis"},
        {"long_name", "standard_name", "units"},
        {"long_name", "standard_name", "units"}
    };

    const char *attVals[][4] = {
        {"simulation site", "1", "timeseries_id"},
        {"latitude", "latitude", "degrees_north", "Y"},
        {"longitude", "longitude", "degrees_east", "X"},
        {"y coordinate of projection", "projection_y_coordinate", units},
        {"x coordinate of projection", "projection_x_coordinate", units}
    };

    const char *varNames[] = {
        siteName,
        readinGeoYName,
        readinGeoXName,
        readinProjYName,
        readinProjXName
    };
    int varIDs[5]; // 5 - Maximum number of variables to create
    const int numAtts[] = {numSiteAtt, numLatAtt, numLonAtt, numYAtt, numXAtt};

    int varNum;
    int attNum;
    int ncType;

    SW_NC_create_netCDF_dim(
        siteName, SW_Domain->nDimS, domFileID, sDimID, LogInfo
    );
    if (LogInfo->stopRun) {
        return; // Exit prematurely due to error
    }

    // Create all variables above (may or may not include "x" and "y")
    // Then fill them with their respective attributes
    for (varNum = 0; varNum < numVarsToWrite; varNum++) {
        /* Make sure the "site" variable is an unsigned integer */
        ncType = (varNum == 0) ? NC_UINT : NC_DOUBLE;

        SW_NC_create_netCDF_var(
            &varIDs[varNum],
            varNames[varNum],
            sDimID,
            domFileID,
            ncType,
            1,
            NULL,
            deflateLevel,
            LogInfo
        );
        if (LogInfo->stopRun) {
            return; // Exit prematurely due to error
        }

        for (attNum = 0; attNum < numAtts[varNum]; attNum++) {
            SW_NC_write_string_att(
                attNames[varNum][attNum],
                attVals[varNum][attNum],
                varIDs[varNum],
                *domFileID,
                LogInfo
            );
            if (LogInfo->stopRun) {
                return; // Exit function prematurely due to error
            }
        }
    }

    // Set site variable ID and lat/lon IDs created above depending on
    // which CRS long_name matches "crs_bbox"
    *sVarID = varIDs[0];

    if (Str_CompareI(SW_Domain->crs_bbox, geo_long_name) == 0 ||
        (is_wgs84(SW_Domain->crs_bbox) && is_wgs84(geo_long_name))) {

        *YVarID = varIDs[1]; // lat
        *XVarID = varIDs[2]; // lon
    } else if (!primCRSIsGeo &&
               Str_CompareI(SW_Domain->crs_bbox, proj_long_name) == 0) {

        *YVarID = varIDs[3]; // y
        *XVarID = varIDs[4]; // x
    } else {
        LogError(
            LogInfo,
            LOGERROR,
            "The given bounding box name within the domain "
            "input file does not match either the geographic or projected "
            "CRS (if provided) in the netCDF attributes file. Please "
            "make sure the bounding box name matches the desired "
            "CRS 'long_name' that is to be filled."
        );
    }
}

/*
@brief Allocate the bound variables for the domain file when domain is "xy"

@param[out] bndNames Resulting bound names following the naming scheme "*_bnds"
@param[in] yName Read-in latitude/y-axis name
@param[in] xName Read-in longitude/x-axis name
@param[out] LogInfo Holds information on warnings and errors
*/
static void create_bnd_names(
    char bndNames[][MAX_FILENAMESIZE],
    const char *yName,
    const char *xName,
    LOG_INFO *LogInfo
) {

    int scanRes = 0;
    int varNum;
    const int numVars = 2;
    const char *const writeBndsNames[] = {yName, xName};

    for (varNum = 0; varNum < numVars; varNum++) {
        scanRes = snprintf(
            bndNames[varNum],
            MAX_FILENAMESIZE,
            "%s_bnds",
            writeBndsNames[varNum]
        );
        if (scanRes < 0 || scanRes > (int) sizeof bndNames[varNum]) {
            LogError(
                LogInfo,
                LOGERROR,
                "A problem occurred when creating a 'bnds' variable."
            );
            return; /* Exit function prematurely due to error */
        }
    }
}

/**
@brief Fill the domain netCDF file with variables that are for domain type
"xy", other in other words, gridded

@param[in] SW_Domain Struct of type SW_DOMAIN holding constant
    temporal/spatial information for a set of simulation runs
@param[in] domFileID Domain netCDF file ID
@param[out] YDimID Dimension identifier of the Y-axis horizontal coordinate
    dimension (lat or y)
@param[out] XVarID Dimension identifier of the X-axis horizontal coordinate
    dimension (lon or x)
@param[out] YVarID Variable identifier of the Y-axis horizontal coordinate
    variable (lat or y)
@param[out] XVarID Variable identifier of the X-axis horizontal coordinate
    variable (lon or x)
@param[out] YBndsID Variable identifier of the Y-axis horizontal coordinate
    bounds variable (lat_bnds or y_bnds)
@param[out] XVarID Variable identifier of the X-axis horizontal coordinate
    bounds variable (lon_bnds or x_bnds)
@param[in] deflateLevel Level of deflation that will be used for the created
variable
@param[out] LogInfo Holds information on warnings and errors
*/
static void fill_domain_netCDF_gridded(
    SW_DOMAIN *SW_Domain,
    int *domFileID,
    int *YDimID,
    int *XDimID,
    int *YVarID,
    int *XVarID,
    int *YBndsID,
    int *XBndsID,
    int deflateLevel,
    LOG_INFO *LogInfo
) {

    char *geo_long_name = SW_Domain->OutDom.netCDFOutput.crs_geogsc.long_name;
    char *proj_long_name = SW_Domain->OutDom.netCDFOutput.crs_projsc.long_name;
    Bool primCRSIsGeo =
        SW_Domain->OutDom.netCDFOutput.primary_crs_is_geographic;
    char *units = SW_Domain->OutDom.netCDFOutput.crs_projsc.units;

    /* Get latitude/longitude names that were read-in from input file */
    char *readinGeoYName = SW_Domain->OutDom.netCDFOutput.geo_YAxisName;
    char *readinGeoXName = SW_Domain->OutDom.netCDFOutput.geo_XAxisName;
    char *readinProjYName = SW_Domain->OutDom.netCDFOutput.proj_YAxisName;
    char *readinProjXName = SW_Domain->OutDom.netCDFOutput.proj_XAxisName;

    int bndsID = 0;
    int bndVarDims[2]; // Used for bound variables in the netCDF file
    int dimNum;
    int varNum;
    int attNum;

    const int numVars = (primCRSIsGeo) ? 2 : 4; // lat/lon or lat/lon + x/y vars
    const char *varNames[] = {
        readinGeoYName, readinGeoXName, readinProjYName, readinProjXName
    };
    char bndVarNames[2][MAX_FILENAMESIZE] = {"\0"};
    const char *varAttNames[][5] = {
        {"long_name", "standard_name", "units", "axis", "bounds"},
        {"long_name", "standard_name", "units", "axis", "bounds"},
        {"long_name", "standard_name", "units", "bounds"},
        {"long_name", "standard_name", "units", "bounds"}
    };

    const char *varAttVals[][5] = {
        {"latitude", "latitude", "degrees_north", "Y", "lat_bnds"},
        {"longitude", "longitude", "degrees_east", "X", "lon_bnds"},
        {"y coordinate of projection",
         "projection_y_coordinate",
         units,
         "y_bnds"},
        {"x coordinate of projection",
         "projection_x_coordinate",
         units,
         "x_bnds"}
    };
    int numLatAtt = (primCRSIsGeo) ? 5 : 4;
    int numLonAtt = (primCRSIsGeo) ? 5 : 4;
    int numYAtt = 4;
    int numXAtt = 4;
    int numAtts[] = {numLatAtt, numLonAtt, numYAtt, numXAtt};

    const int numDims = 3;
    const char *YDimName = (primCRSIsGeo) ? readinGeoYName : readinProjYName;
    const char *XDimName = (primCRSIsGeo) ? readinGeoXName : readinProjXName;

    const char *dimNames[] = {YDimName, XDimName, "bnds"};
    const unsigned long dimVals[] = {SW_Domain->nDimY, SW_Domain->nDimX, 2};
    int *createDimIDs[] = {YDimID, XDimID, &bndsID};
    int dimIDs[] = {0, 0, 0};
    int dimIDIndex;

    int varIDs[4];
    int varBndIDs[4];

    create_bnd_names(bndVarNames, YDimName, XDimName, LogInfo);
    if (LogInfo->stopRun) {
        return; /* Exit function prematurely due to error */
    }

    if (primCRSIsGeo) {
        varAttVals[0][4] = bndVarNames[0];
        varAttVals[1][4] = bndVarNames[1];
    } else {
        varAttVals[2][4] = bndVarNames[0];
        varAttVals[3][4] = bndVarNames[1];
    }

    // Create dimensions
    for (dimNum = 0; dimNum < numDims; dimNum++) {
        SW_NC_create_netCDF_dim(
            dimNames[dimNum],
            dimVals[dimNum],
            domFileID,
            createDimIDs[dimNum],
            LogInfo
        );
        if (LogInfo->stopRun) {
            return; // Exit function prematurely due to error
        }

        dimIDs[dimNum] = *createDimIDs[dimNum];
    }

    // Create variables - lat/lon and (if the primary CRS is
    // not geographic) y/x along with the corresponding *_bnds variable
    bndVarDims[1] = bndsID; // Set the second dimension of variable to bounds

    for (varNum = 0; varNum < numVars; varNum++) {
        dimIDIndex = varNum % 2; // 2 - get lat/lon dim ids only, not bounds

        SW_NC_create_netCDF_var(
            &varIDs[varNum],
            varNames[varNum],
            (!primCRSIsGeo && varNum < 2) ? dimIDs : &dimIDs[dimIDIndex],
            domFileID,
            NC_DOUBLE,
            (!primCRSIsGeo && varNum < 2) ? 2 : 1,
            NULL,
            deflateLevel,
            LogInfo
        );
        if (LogInfo->stopRun) {
            return; // Exit function prematurely due to error
        }

        // Set the first dimension of the variable
        if (varNum < 2) {
            bndVarDims[0] = dimIDs[dimIDIndex];

            SW_NC_create_netCDF_var(
                &varBndIDs[varNum],
                bndVarNames[varNum],
                bndVarDims,
                domFileID,
                NC_DOUBLE,
                2,
                NULL,
                deflateLevel,
                LogInfo
            );
            if (LogInfo->stopRun) {
                return; // Exit function prematurely due to error
            }
        }

        // Fill attributes
        for (attNum = 0; attNum < numAtts[varNum]; attNum++) {
            SW_NC_write_string_att(
                varAttNames[varNum][attNum],
                varAttVals[varNum][attNum],
                varIDs[varNum],
                *domFileID,
                LogInfo
            );
            if (LogInfo->stopRun) {
                return; // Exit function prematurely due to error
            }
        }
    }

    // Set lat/lon IDs created above depending on which CRS long_name
    // matches "crs_bbox"
    *YBndsID = varBndIDs[0]; /* lat_bnds/y_bnds */
    *XBndsID = varBndIDs[1]; /* lon_bnds/x_bnds */

    if (Str_CompareI(SW_Domain->crs_bbox, geo_long_name) == 0 ||
        (is_wgs84(SW_Domain->crs_bbox) && is_wgs84(geo_long_name))) {

        *YVarID = varIDs[0]; // lat
        *XVarID = varIDs[1]; // lon
    } else if (!primCRSIsGeo &&
               Str_CompareI(SW_Domain->crs_bbox, proj_long_name) == 0) {

        *YVarID = varIDs[2]; // y
        *XVarID = varIDs[3]; // x
    } else {
        LogError(
            LogInfo,
            LOGERROR,
            "The given bounding box name within the domain "
            "input file does not match either the geographic or projected "
            "CRS (if provided) in the netCDF attributes file. Please "
            "make sure the bounding box name matches the desired "
            "CRS 'long_name' that is to be filled."
        );
    }
}

/**
@brief Fill the given netCDF with global attributes

@param[in] SW_netCDFOut Constant netCDF output file information
@param[in] ncFileID Identifier of the open netCDF file to write all information
@param[in] domType Type of domain in which simulations are running
    (gridcell/sites)
@param[in] freqAtt Value of a global attribute "frequency"
    * fixed (no time): "fx"
    * has time: "day", "week", "month", or "year"
@param[in] isInputFile Specifies if the file being written to is input
@param[out] LogInfo Holds information on warnings and errors
*/
static void fill_netCDF_with_global_atts(
    SW_NETCDF_OUT *SW_netCDFOut,
    const int *ncFileID,
    const char *domType,
    const char *freqAtt,
    Bool isInputFile,
    LOG_INFO *LogInfo
) {

    char sourceStr[40]; // 40 - valid size of the SOILWAT2 global `SW2_VERSION`
                        // + "SOILWAT2"
    char creationDateStr[21]; // 21 - valid size to hold a string of format
                              // YYYY-MM-DDTHH:MM:SSZ

    int attNum;
    // Use "featureType" only if domainType is "s"
    const int numGlobAtts = (strcmp(domType, "s") == 0) ? 14 : 13;
    const char *attNames[] = {
        "title",
        "author",
        "institution",
        "comment",
        "coordinate_system",
        "Conventions",
        "source",
        "source_id",
        "further_info_url",
        "creation_date",
        "history",
        "product",
        "frequency",
        "featureType"
    };

    const char *productStr = (isInputFile) ? "model-input" : "model-output";
    const char *featureTypeStr;
    if (strcmp(domType, "s") == 0) {
        featureTypeStr = (strcmp(freqAtt, "fx") == 0) ? "point" : "timeSeries";
    } else {
        featureTypeStr = "";
    }

    const char *attVals[] = {
        SW_netCDFOut->title,
        SW_netCDFOut->author,
        SW_netCDFOut->institution,
        SW_netCDFOut->comment,
        SW_netCDFOut->coordinate_system,
        "CF-1.10",
        sourceStr,
        "SOILWAT2",
        "https://github.com/DrylandEcology/SOILWAT2",
        creationDateStr,
        "No revisions.",
        productStr,
        freqAtt,
        featureTypeStr
    };

    // Fill `sourceStr` and `creationDateStr`
    (void) snprintf(sourceStr, 40, "SOILWAT2%s", SW2_VERSION);
    timeStringISO8601(creationDateStr, sizeof creationDateStr);

    // Write out the necessary global attributes that are listed above
    for (attNum = 0; attNum < numGlobAtts; attNum++) {
        SW_NC_write_string_att(
            attNames[attNum], attVals[attNum], NC_GLOBAL, *ncFileID, LogInfo
        );
        if (LogInfo->stopRun) {
            return; // Exit function prematurely due to error
        }
    }
}

/**
@brief Fill the desired netCDF with a projected CRS

@param[in] crs_geogsc Struct of type SW_CRS that holds all information
    regarding to the CRS type "geographic"
@param[in] ncFileID Identifier of the open netCDF file to write all information
@param[in] coord_sys Name of the coordinate system being used
@param[in] geo_id Projected CRS variable identifier within the netCDF we are
    writing to
@param[out] LogInfo Holds information on warnings and errors
*/
static void fill_netCDF_with_geo_CRS_atts(
    SW_CRS *crs_geogsc,
    const int *ncFileID,
    char *coord_sys,
    int geo_id,
    LOG_INFO *LogInfo
) {

    int attNum;
    int numStrAtts = 3;
    int numDoubleAtts = 3;
    const int numValsToWrite = 1;
    const char *strAttNames[] = {"grid_mapping_name", "long_name", "crs_wkt"};
    const char *doubleAttNames[] = {
        "longitude_of_prime_meridian", "semi_major_axis", "inverse_flattening"
    };
    const char *strAttVals[] = {
        crs_geogsc->grid_mapping_name,
        crs_geogsc->long_name,
        crs_geogsc->crs_wkt
    };
    const double *doubleAttVals[] = {
        &crs_geogsc->longitude_of_prime_meridian,
        &crs_geogsc->semi_major_axis,
        &crs_geogsc->inverse_flattening
    };

    if (strcmp(coord_sys, "Absent") == 0) {
        // Only write out `grid_mapping_name`
        SW_NC_write_string_att(
            strAttNames[0], strAttVals[0], geo_id, *ncFileID, LogInfo
        );
    } else {
        // Write out all attributes
        for (attNum = 0; attNum < numStrAtts; attNum++) {
            SW_NC_write_string_att(
                strAttNames[attNum],
                strAttVals[attNum],
                geo_id,
                *ncFileID,
                LogInfo
            );
            if (LogInfo->stopRun) {
                return; // Exit function prematurely due to error
            }
        }

        for (attNum = 0; attNum < numDoubleAtts; attNum++) {
            SW_NC_write_att(
                doubleAttNames[attNum],
                (void *) doubleAttVals[attNum],
                geo_id,
                *ncFileID,
                numValsToWrite,
                NC_DOUBLE,
                LogInfo
            );
            if (LogInfo->stopRun) {
                return; // Exit function prematurely due to error
            }
        }
    }
}

/**
@brief Fill the desired netCDF with a projected CRS

@param[in] crs_projsc Struct of type SW_CRS that holds all information
    regarding to the CRS type "projected"
@param[in] ncFileID Identifier of the open netCDF file to write all information
@param[in] proj_id Projected CRS variable identifier within the netCDF we are
    writing to
@param[out] LogInfo Holds information on warnings and errors
*/
static void fill_netCDF_with_proj_CRS_atts(
    SW_CRS *crs_projsc, const int *ncFileID, int proj_id, LOG_INFO *LogInfo
) {

    const int numStrAtts = 5;
    const int numDoubleAtts = 8;
    int strAttNum;
    int doubleAttNum;
    int numValsToWrite;
    const char *strAttNames[] = {
        "long_name", "grid_mapping_name", "datum", "units", "crs_wkt"
    };
    const char *doubleAttNames[] = {
        "standard_parallel",
        "longitude_of_central_meridian",
        "latitude_of_projection_origin",
        "false_easting",
        "false_northing",
        "longitude_of_prime_meridian",
        "semi_major_axis",
        "inverse_flattening"
    };

    const char *strAttVals[] = {
        crs_projsc->long_name,
        crs_projsc->grid_mapping_name,
        crs_projsc->datum,
        crs_projsc->units,
        crs_projsc->crs_wkt
    };
    const double *doubleAttVals[] = {
        crs_projsc->standard_parallel,
        &crs_projsc->longitude_of_central_meridian,
        &crs_projsc->latitude_of_projection_origin,
        &crs_projsc->false_easting,
        &crs_projsc->false_northing,
        &crs_projsc->longitude_of_prime_meridian,
        &crs_projsc->semi_major_axis,
        &crs_projsc->inverse_flattening
    };

    for (strAttNum = 0; strAttNum < numStrAtts; strAttNum++) {
        SW_NC_write_string_att(
            strAttNames[strAttNum],
            strAttVals[strAttNum],
            proj_id,
            *ncFileID,
            LogInfo
        );
        if (LogInfo->stopRun) {
            return; // Exit function prematurely due to error
        }
    }

    for (doubleAttNum = 0; doubleAttNum < numDoubleAtts; doubleAttNum++) {
        numValsToWrite =
            (doubleAttNum > 0 || isnan(crs_projsc->standard_parallel[1])) ? 1 :
                                                                            2;

        SW_NC_write_att(
            doubleAttNames[doubleAttNum],
            (void *) doubleAttVals[doubleAttNum],
            proj_id,
            *ncFileID,
            numValsToWrite,
            NC_DOUBLE,
            LogInfo
        );
        if (LogInfo->stopRun) {
            return; // Exit function prematurely due to error
        }
    }
}

/**
@brief Wrapper function to fill a netCDF with all the invariant information
i.e., global attributes (including time created) and CRS information
(including the creation of these variables)

@param[in] SW_netCDFOut Constant netCDF output file information
@param[in] domType Type of domain in which simulations are running
    (gridcell/sites)
@param[in] ncFileID Identifier of the open netCDF file to write all information
@param[in] isInputFile Specifies whether the file being written to is input
@param[out] LogInfo Holds information on warnings and errors
*/
static void fill_netCDF_with_invariants(
    SW_NETCDF_OUT *SW_netCDFOut,
    char *domType,
    int *ncFileID,
    Bool isInputFile,
    LOG_INFO *LogInfo
) {

    int geo_id = 0;
    int proj_id = 0;
    const char *fx = "fx";

    /* Do not deflate crs_geogsc */
    SW_NC_create_netCDF_var(
        &geo_id, "crs_geogsc", NULL, ncFileID, NC_BYTE, 0, NULL, 0, LogInfo
    );
    if (LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

    // Geographic CRS attributes
    fill_netCDF_with_geo_CRS_atts(
        &SW_netCDFOut->crs_geogsc,
        ncFileID,
        SW_netCDFOut->coordinate_system,
        geo_id,
        LogInfo
    );
    if (LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

    // Projected CRS variable/attributes
    if (!SW_netCDFOut->primary_crs_is_geographic) {

        /* Do not deflate crs_projsc */
        SW_NC_create_netCDF_var(
            &proj_id, "crs_projsc", NULL, ncFileID, NC_BYTE, 0, NULL, 0, LogInfo
        );
        if (LogInfo->stopRun) {
            return; // Exit function prematurely due to error
        }

        fill_netCDF_with_proj_CRS_atts(
            &SW_netCDFOut->crs_projsc, ncFileID, proj_id, LogInfo
        );
        if (LogInfo->stopRun) {
            return; // Exit function prematurely due to error
        }
    }

    // Write global attributes
    fill_netCDF_with_global_atts(
        SW_netCDFOut, ncFileID, domType, fx, isInputFile, LogInfo
    );
}

/**
@brief Fill the progress variable in the progress netCDF with values

@param[in] SW_Domain Struct of type SW_DOMAIN holding constant
    temporal/spatial information for a set of simulation runs
@param[in,out] LogInfo Holds information on warnings and errors
*/
static void fill_prog_netCDF_vals(SW_DOMAIN *SW_Domain, LOG_INFO *LogInfo) {

    int domVarID = SW_Domain->netCDFInput.ncDomVarIDs[vNCdom];
    int progVarID = SW_Domain->netCDFInput.ncDomVarIDs[vNCprog];
    unsigned int domStatus;
    unsigned long suid;
    unsigned long ncSuid[2];
    unsigned long nSUIDs = SW_Domain->nSUIDs;
    unsigned long nDimY = SW_Domain->nDimY;
    unsigned long nDimX = SW_Domain->nDimX;
    int progFileID = SW_Domain->SW_PathInputs.ncDomFileIDs[vNCprog];
    int domFileID = SW_Domain->SW_PathInputs.ncDomFileIDs[vNCdom];
    size_t start1D[] = {0};
    size_t start2D[] = {0, 0};
    size_t count1D[] = {nSUIDs};
    size_t count2D[] = {nDimY, nDimX};
    size_t *start =
        (strcmp(SW_Domain->DomainType, "s") == 0) ? start1D : start2D;
    size_t *count =
        (strcmp(SW_Domain->DomainType, "s") == 0) ? count1D : count2D;

    signed char *vals = (signed char *) Mem_Malloc(
        nSUIDs * sizeof(signed char), "fill_prog_netCDF_vals()", LogInfo
    );
    if (LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

    for (suid = 0; suid < nSUIDs; suid++) {
        SW_DOM_calc_ncSuid(SW_Domain, suid, ncSuid);

        SW_NC_get_single_val(
            domFileID,
            &domVarID,
            NULL,
            ncSuid,
            (void *) &domStatus,
            "unsigned integer",
            LogInfo
        );
        if (LogInfo->stopRun) {
            goto freeMem; // Exit function prematurely due to error
        }

        vals[suid] = (domStatus == NC_FILL_UINT) ? NC_FILL_BYTE : PRGRSS_READY;
    }

    SW_NC_write_vals(
        &progVarID, progFileID, "progress", vals, start, count, "byte", LogInfo
    );
    nc_sync(progFileID);

// Free allocated memory
freeMem:
    free(vals);
}

/*
@brief Allocate space for input weather override calendars

@param[out] overrideCalendars Array holding a single override
calendar for every weather variable
@param[in] numInVars Maximum number of variables that are contained in
in the weather category
@param[out] LogInfo LogInfo Holds information on warnings and errors
*/
static void alloc_overrideCalendars(
    char ***overrideCalendars, int numInVars, LOG_INFO *LogInfo
) {
    int varNum;

    (*overrideCalendars) = (char **) Mem_Malloc(
        sizeof(char *) * numInVars, "alloc_overrideCalendars()", LogInfo
    );
    if (LogInfo->stopRun) {
        return; /* Exit function prematurely due to error */
    }

    for (varNum = 0; varNum < numInVars; varNum++) {
        (*overrideCalendars)[varNum] = NULL;
    }
}

/*
@brief Allocate all information that pertains to weather input files

@param[out] ncWeatherInFiles Weather input file formats
@param[in] numInVars Maximum number of variables that are contained in
in the weather category
@param[out] LogInfo LogInfo Holds information on warnings and errors
*/
static void alloc_weath_input_files(
    char ****ncWeatherInFiles, int numInVars, LOG_INFO *LogInfo
) {

    int varNum;

    /* Allocate/initialize weather input files */
    (*ncWeatherInFiles) = (char ***) Mem_Malloc(
        sizeof(char **) * numInVars, "alloc_input_files()", LogInfo
    );
    if (LogInfo->stopRun) {
        return; /* Exit functionality prematurely due to error */
    }

    for (varNum = 0; varNum < numInVars; varNum++) {
        (*ncWeatherInFiles)[varNum] = NULL;
    }
}

/*
@brief Generate expected input weather file names based on various
information provided by the user

@param[in] weathNameFormat User-inputted weather file name format for
every weather variable
@param[in] strideInfo Specifies the start/number of years that the
weather files will follow
@param[in] weatherInputInfo Specifies
@param[in] startYr Start year of the simulation
@param[in] endYr End year of the simulation
@param[out] outWeathFileNames Generated input file names based on stride
informatoin
@param[out] ncWeatherInStartEndYrs Specifies that start/end years of each input
weather file
@param[out] numncWeatherInFiles Specifies the number of input files each
weather variable has
@param[out] LogInfo LogInfo Holds information on warnings and errors
*/
static void generate_weather_filenames(
    char **weathNameFormat,
    const int strideInfo[],
    char ***weatherInputInfo,
    TimeInt startYr,
    TimeInt endYr,
    char ****outWeathFileNames,
    unsigned int ***ncWeatherInStartEndYrs,
    unsigned int *numncWeatherInFiles,
    const Bool readInVars[],
    LOG_INFO *LogInfo
) {

    int numStYr = strideInfo[SW_INSTRIDEYR];
    int numStStart = strideInfo[SW_INSTRIDESTART];
    int weathVar;
    char *stridePattern = NULL;
    unsigned int beginFileYr;
    unsigned int endFileYr;
    Bool doubleStrVal = swFALSE;
    Bool singleStrVal = swFALSE;
    Bool naStrVal = swFALSE;
    int sprintfRes = 0;
    int numWeathIn = -1;
    int inFileNum;
    const int infNAVal = -1;

    char newFileName[MAX_FILENAMESIZE] = {'\0'};

    for (weathVar = 1; weathVar < numVarsInKey[eSW_InWeather]; weathVar++) {
        if (!readInVars[weathVar + 1]) {
            continue;
        }

        stridePattern = weatherInputInfo[weathVar][INSTPATRN];

        doubleStrVal = (Bool) (strcmp(stridePattern, "%4d-%4d") == 0 ||
                               strcmp(stridePattern, "%4d_%4d") == 0);
        singleStrVal = (Bool) (strcmp(stridePattern, "%4d") == 0);
        naStrVal = (Bool) (strcmp(stridePattern, "NA") == 0);

        if (numStStart > (int) endYr) {
            LogError(
                LogInfo,
                LOGERROR,
                "Stride start year for weather variable '%s' is greater "
                "than the end year of the program (%d).",
                weatherInputInfo[weathVar][INNCVARNAME],
                endYr
            );
            return; /* Exit function prematurely due to error */
        }

        if (numStYr == infNAVal) {
            numStYr = (int) (endYr - startYr + 1);
            numStStart = (int) startYr;
        }

        /* Calculate the number of weather files and start/end year of
           the first one */
        if (numWeathIn == -1) {
            numWeathIn =
                (naStrVal) ?
                    1 :
                    (int) ceil((double) (endYr - numStStart + 1) / numStYr);
            *numncWeatherInFiles = numWeathIn;
        }

        beginFileYr = numStStart;
        endFileYr = beginFileYr + numStYr - 1;

        SW_NCIN_alloc_weath_input_info(
            outWeathFileNames,
            ncWeatherInStartEndYrs,
            numWeathIn,
            weathVar,
            LogInfo
        );
        if (LogInfo->stopRun) {
            return; /* Exit function prematurely due to error */
        }

        /* Handle if there is only one input file which does not
           have a year format */
        if (naStrVal) {
            (*outWeathFileNames)[weathVar][0] =
                Str_Dup(weathNameFormat[weathVar], LogInfo);
            if (LogInfo->stopRun) {
                return; /* Exit function prematurely due to error */
            }

            *numncWeatherInFiles = 1;

            (*ncWeatherInStartEndYrs)[0][0] = numStYr;
            (*ncWeatherInStartEndYrs)[0][1] = endYr;

            continue;
        }

        /* Generate every input file */
        inFileNum = 0;
        while (beginFileYr <= endYr) {
            if (doubleStrVal) {
                sprintfRes = snprintf(
                    newFileName,
                    MAX_FILENAMESIZE,
                    weathNameFormat[weathVar],
                    beginFileYr,
                    endFileYr
                );
            } else if (singleStrVal) {
                sprintfRes = snprintf(
                    newFileName,
                    MAX_FILENAMESIZE,
                    weathNameFormat[weathVar],
                    beginFileYr
                );
            } else {
                LogError(
                    LogInfo,
                    LOGERROR,
                    "Could not understand stride pattern '%s' for file "
                    "format '%s'.",
                    stridePattern,
                    weathNameFormat[weathVar]
                );
                return; /* Exit function prematurely due to error */
            }

            if (sprintfRes < 0) {
                LogError(
                    LogInfo,
                    LOGERROR,
                    "Could not create input file name from '%s'.",
                    weathNameFormat[weathVar]
                );
                return; /* Exit function prematurely due to error */
            }

            (*outWeathFileNames)[weathVar][inFileNum] =
                Str_Dup(newFileName, LogInfo);
            if (LogInfo->stopRun) {
                return; /* Exit function prematurely due to error */
            }

            (*ncWeatherInStartEndYrs)[inFileNum][0] = beginFileYr;
            (*ncWeatherInStartEndYrs)[inFileNum][1] = endFileYr;

            beginFileYr += numStYr;
            endFileYr += numStYr;

            inFileNum++;
        }
    }
}

/**
@brief Get the type of a given variable

@param[in] ncFileID Identifier of the open netCDF file to write all information
@param[in] varID Current variable identifier
@param[in] varName Variable to get the type of
@param[out] ncType Resulting nc type of the variable in question
@param[out] LogInfo LogInfo Holds information on warnings and errors
*/
static void get_var_type(
    int ncFileID, int varID, char *varName, nc_type *ncType, LOG_INFO *LogInfo
) {
    if (nc_inq_vartype(ncFileID, varID, ncType) != NC_NOERR) {
        LogError(
            LogInfo,
            LOGERROR,
            "Could not read the type of the variable '%s'.",
            varName
        );
    }
}

/**
@brief Determine if a spatial variable within an input file is
2-dimensional (1D and 2D are handled differently)

@param[in] ncFileID Identifier of the open netCDF file to write all information
@param[in] yName Latitude/y variable/dimension name
@param[out] LogInfo LogInfo Holds information on warnings and errors

@return Result of the check that the y variable has two dimensions
*/
static Bool spatial_var_is_2d(int ncFileID, char *yName, LOG_INFO *LogInfo) {

    int varID = -1;
    int nDims = 0;

    SW_NC_get_var_identifier(ncFileID, yName, &varID, LogInfo);
    if (LogInfo->stopRun) {
        return swFALSE;
    }

    if (nc_inq_varndims(ncFileID, varID, &nDims) != NC_NOERR) {
        LogError(
            LogInfo,
            LOGERROR,
            "Could not get the number of dimensions from the variable '%s'.",
            yName
        );
    }

    return (Bool) (nDims == 2);
}

/**
@brief Retrieve the dimension sizes of a given variable

@param[in] ncFileID Identifier of the open netCDF file to write all informatio
@param[in] numDims Number of dimensions to read
@param[out] dimSizes All dimension sizes that the variable contains
@param[in] varName Variable to get the dimension size(s) of
@param[in] varID Current variable identifier
@param[out] LogInfo Holds information dealing with logfile output
*/
static void get_var_dimsizes(
    int ncFileID,
    const int numDims,
    size_t **dimSizes,
    char *varName,
    int *varID,
    LOG_INFO *LogInfo
) {
    int index;
    int dimID[2] = {0};

    for (index = 0; index < numDims; index++) {
        if (nc_inq_varid(ncFileID, varName, varID) != NC_NOERR) {
            LogError(
                LogInfo,
                LOGERROR,
                "Could not get identifier of the variable '%s'.",
                varName
            );
            return;
        }

        if (nc_inq_vardimid(ncFileID, *varID, dimID) != NC_NOERR) {
            LogError(
                LogInfo,
                LOGERROR,
                "Could not get the identifiers of the dimension of the "
                "variable '%s'.",
                varName
            );
            return;
        }

        SW_NC_get_dimlen_from_dimid(
            ncFileID, dimID[index], dimSizes[index], LogInfo
        );
        if (LogInfo->stopRun) {
            return;
        }
    }
}

/**
@brief Helper function to allocate memory for storing the spatial coordinate
values of the programs domain

@param[out] domCoordArrs List holding the arrays to be allocated
@param[in] domCoordSizes List holding the sizes that the arrays should be
@param[in] numCoords Number of variables to allocate
@param[out] LogInfo Holds information dealing with logfile output
*/
static void alloc_dom_coord_info(
    double **domCoordArrs[],
    size_t *domCoordSizes[],
    const int numCoords,
    LOG_INFO *LogInfo
) {
    size_t coordArr;
    size_t coordIndex;
    size_t allocSize;

    for (coordArr = 0; coordArr < (size_t) numCoords; coordArr++) {
        allocSize = *domCoordSizes[coordArr % 2];
        *domCoordArrs[coordArr] = (double *) Mem_Malloc(
            sizeof(double) * allocSize, "alloc_dom_coord_info()", LogInfo
        );

        for (coordIndex = 0; coordIndex < allocSize; coordIndex++) {
            (*domCoordArrs[coordArr])[coordIndex] = 0.0;
        }
    }
}

/**
@brief Read/store the spatial coordinates that the program contains

@param[out] SW_netCDFIn Constant netCDF input file information
@param[in] domCoordVarNames A list of the two latitude/y and longitude/x
coordinate variable/dimension names
@param[in] siteName User-provided site dimension/variable "site" name
@param[in] domFileID Domain file identifier
@param[in] domType Type of domain - "s" or "xy"
@param[in] primCRSIsGeo Specifies if the current CRS type is geographic
@param[out] LogInfo Holds information dealing with logfile output
*/
static void read_domain_coordinates(
    SW_NETCDF_IN *SW_netCDFIn,
    char *domCoordVarNames[],
    char *siteName,
    int domFileID,
    char *domType,
    Bool primCRSIsGeo,
    LOG_INFO *LogInfo
) {
    int index;
    int varID;
    const int numReadInDims = (primCRSIsGeo) ? 2 : 4;
    const int numDims = 2;

    char *domCoordNames[2]; /* Set later */

    size_t *domCoordSizes[] = {
        &SW_netCDFIn->domYCoordGeoSize,
        &SW_netCDFIn->domXCoordGeoSize,
        &SW_netCDFIn->domYCoordProjSize,
        &SW_netCDFIn->domXCoordProjSize
    };
    double **domCoordArrs[] = {
        &SW_netCDFIn->domYCoordsGeo,
        &SW_netCDFIn->domXCoordsGeo,
        &SW_netCDFIn->domYCoordsProj,
        &SW_netCDFIn->domXCoordsProj
    };

    /* Determine the dimension lengths from the currect dimension
       names matching site/xy and geographical/projected */
    if (strcmp("s", domType) == 0) {
        domCoordNames[0] = siteName;
        domCoordNames[1] = siteName;
    } else {
        if (primCRSIsGeo) {
            domCoordNames[0] = domCoordVarNames[0];
            domCoordNames[1] = domCoordVarNames[1];
        } else {
            domCoordNames[0] = domCoordVarNames[2];
            domCoordNames[1] = domCoordVarNames[3];
        }
    }

    for (index = 0; index < numDims; index++) {
        SW_NC_get_dimlen_from_dimname(
            domFileID, domCoordNames[index], domCoordSizes[index], LogInfo
        );
        if (LogInfo->stopRun) {
            return; /* Exit function prematurely due to error */
        }

        /* Match the same for projected sizes (may not be used) */
        *(domCoordSizes[index + 2]) = *(domCoordSizes[index]);
    }

    alloc_dom_coord_info(domCoordArrs, domCoordSizes, numReadInDims, LogInfo);
    if (LogInfo->stopRun) {
        return; /* Exit function prematurely due to error */
    }

    for (index = 0; index < numReadInDims; index++) {
        varID = -1;

        SW_NC_get_vals(
            domFileID,
            &varID,
            domCoordVarNames[index],
            *domCoordArrs[index],
            LogInfo
        );
        if (LogInfo->stopRun) {
            return; /* Exit prematurely due to error */
        }
    }
}

/**
@brief Compare the coordinate values from an input file against those of
the domain input file and determine if the program should use an index file(s)

@param[in] domYCoordsGeo Domain latitude/y coordinates
@param[in] domXCoordsGeo Domain longitude/x coordinates
@param[in] readinY Read-in latitude/y values
@param[in] readinX Read-in longitude/x values
@param[in] ySize Size of the latitude/y dimension
@param[in] xSize Size of the longitude/x dimension
@param[in] spatialTol User-provided tolerance for comparing spatial coordinates
@param[out] useIndexFile Specifies if the program should use an index file(s)
*/
static void determine_index_file_use(
    const double *domYCoords,
    const double *domXCoords,
    double *readinY,
    double *readinX,
    size_t ySize,
    size_t xSize,
    double spatialTol,
    Bool *useIndexFile
) {
    size_t coordIndex = 0;
    double domCordVal = 0.0;

    /* Check to see if the read-in coordinates are the same
       as that of domain, if not set a flag staying that
       an index file must be used */
    while (!*useIndexFile && (coordIndex < ySize || coordIndex < xSize)) {
        if (coordIndex < ySize) {
            domCordVal = domYCoords[coordIndex];
            *useIndexFile =
                (Bool) (!EQ_w_tol(readinY[coordIndex], domCordVal, spatialTol));
        }

        if (!*useIndexFile && coordIndex < xSize) {
            domCordVal = domXCoords[coordIndex];
            *useIndexFile =
                (Bool) (!EQ_w_tol(readinX[coordIndex], domCordVal, spatialTol));
        }

        coordIndex++;
    }
}

/**
@brief Read one-dimensional input coordinate variables from an input file

@param[in] SW_netCDFIn Constant netCDF input file information
@param[in] ncFileID Identifier of the open netCDF file to write all information
@param[out] readinYVals Read-in latitude/y values
@param[out] readinXVals Read-in longitude/x values
@param[out] dimSizes Sizes of the dimensions of the variables to read
@param[in] yxVarNames A list of two variable names for the input variables
that contain latitude/y and longitude/x coordinate values
@param[in] numReadInDims Number of dimensions to read in
@param[out] useIndexFile Specifies to create/use an index file
@param[in] compareCoords Specifies if the function should compare the
coordinates to the domain coordinates
@param[in] spatialTol User-provided tolerance for comparing spatial coordinates
@param[in] inPrimCRSIsGeo Specifies if the input file has a primary
CRS of geographical
@param[out] LogInfo Holds information dealing with logfile output
*/
static void get_1D_input_coordinates(
    SW_NETCDF_IN *SW_netCDFIn,
    int ncFileID,
    double **readinYVals,
    double **readinXVals,
    size_t *dimSizes[],
    char **yxVarNames,
    const int numReadInDims,
    Bool *useIndexFile,
    Bool compareCoords,
    double spatialTol,
    Bool inPrimCRSIsGeo,
    LOG_INFO *LogInfo
) {
    int index;
    int varID;

    double **xyVals[] = {readinYVals, readinXVals};
    float *tempFVals = NULL;
    void *valPtr = NULL;
    nc_type varType;
    double *domYVals = (inPrimCRSIsGeo) ? SW_netCDFIn->domYCoordsGeo :
                                          SW_netCDFIn->domYCoordsProj;
    double *domXVals = (inPrimCRSIsGeo) ? SW_netCDFIn->domXCoordsGeo :
                                          SW_netCDFIn->domXCoordsProj;

    size_t *ySize = dimSizes[0];
    size_t *xSize = dimSizes[1];
    size_t copyIndex;

    get_var_dimsizes(
        ncFileID, numReadInDims, dimSizes, yxVarNames[0], &varID, LogInfo
    );
    if (LogInfo->stopRun) {
        return;
    }

    get_var_type(ncFileID, varID, yxVarNames[0], &varType, LogInfo);
    if (LogInfo->stopRun) {
        return;
    }

    for (index = 0; index < numReadInDims; index++) {
        varID = -1;

        *(xyVals[index]) = (double *) Mem_Malloc(
            sizeof(double) * *dimSizes[index],
            "get_1D_input_coordinates()",
            LogInfo
        );
        if (LogInfo->stopRun) {
            return;
        }
        valPtr = (void *) *(xyVals[index]);

        if (varType == NC_FLOAT) {
            tempFVals = (float *) Mem_Malloc(
                sizeof(float) * *dimSizes[index],
                "get_1D_input_coordinates()",
                LogInfo
            );
            if (LogInfo->stopRun) {
                return;
            }

            valPtr = (void *) tempFVals;
        }

        SW_NC_get_vals(ncFileID, &varID, yxVarNames[index], valPtr, LogInfo);
        if (LogInfo->stopRun) {
            goto freeTempVals; /* Exit prematurely due to error */
        }

        if (varType == NC_FLOAT) {
            for (copyIndex = 0; copyIndex < *dimSizes[index]; copyIndex++) {
                (*xyVals[index])[copyIndex] = (double) tempFVals[copyIndex];
            }
        }

        free((void *) tempFVals);
        tempFVals = NULL;
    }

    if (compareCoords) {
        *useIndexFile = (Bool) (*ySize != SW_netCDFIn->domYCoordGeoSize ||
                                *xSize != SW_netCDFIn->domXCoordGeoSize);

        if (!*useIndexFile) {
            determine_index_file_use(
                domYVals,
                domXVals,
                *readinYVals,
                *readinXVals,
                *ySize,
                *xSize,
                spatialTol,
                useIndexFile
            );
        }
    }

freeTempVals:
    if (!isnull(tempFVals)) {
        free((void *) tempFVals);
    }
}

/**
@brief Read two-dimensional input coordinate variables from an input file

@param[in] SW_netCDFIn Constant netCDF input file information
@param[in] ncFileID Identifier of the open netCDF file to write all information
@param[out] readinYVals Read-in latitude/y values to store
@param[out] readinXVals Read-in longitude/x values
@param[out] dimSizes Sizes of the dimensions of the variables to read
@param[in] yxVarNames A list of two variable names for the input variables
that contain latitude/y and longitude/x coordinate values
@param[in] numReadInDims Number of dimensions to read in
@param[out] useIndexFile Specifies to create/use an index file
@param[in] compareCoords Specifies if the function should compare the
coordinates to the domain coordinates
@param[in] spatialTol User-provided tolerance for comparing spatial coordinates
@param[in] inPrimCRSIsGeo Specifies if the input file has a primary
CRS of geographical
@param[out] LogInfo Holds information dealing with logfile output
*/
static void get_2D_input_coordinates(
    SW_NETCDF_IN *SW_netCDFIn,
    int ncFileID,
    double **readinYVals,
    double **readinXVals,
    size_t **dimSizes,
    char **yxVarNames,
    const int numReadInDims,
    Bool *useIndexFile,
    Bool compareCoords,
    double spatialTol,
    Bool inPrimCRSIsGeo,
    LOG_INFO *LogInfo
) {

    size_t yDimSize = 0UL;
    size_t xDimSize = 0UL;
    size_t *allDimSizes[2] = {&yDimSize, &xDimSize};
    size_t numPoints = 0UL;
    nc_type varType;
    float *tempFVals = NULL;
    size_t copyIndex;
    void *valPtr = NULL;
    double *domYVals = (inPrimCRSIsGeo) ? SW_netCDFIn->domYCoordsGeo :
                                          SW_netCDFIn->domYCoordsProj;
    double *domXVals = (inPrimCRSIsGeo) ? SW_netCDFIn->domXCoordsGeo :
                                          SW_netCDFIn->domXCoordsProj;

    int varIDs[2] = {-1, -1};
    int varNum;

    double **xyVals[] = {readinYVals, readinXVals};

    /* === Latitude/longitude sizes === */
    for (varNum = 0; varNum < numReadInDims; varNum++) {
        get_var_dimsizes(
            ncFileID,
            numReadInDims,
            allDimSizes,
            yxVarNames[varNum],
            &varIDs[varNum],
            LogInfo
        );
        if (LogInfo->stopRun) {
            return; /* Exit function prematurely due to error */
        }
    }

    *dimSizes[0] = yDimSize;
    *dimSizes[1] = xDimSize;
    numPoints = yDimSize * xDimSize;

    get_var_type(ncFileID, varIDs[0], yxVarNames[0], &varType, LogInfo);
    if (LogInfo->stopRun) {
        return;
    }

    for (varNum = 0; varNum < numReadInDims; varNum++) {
        valPtr = (void *) *(xyVals[varNum]);

        if (varType == NC_FLOAT) {
            tempFVals = (float *) Mem_Malloc(
                sizeof(float) * numPoints, "get_2D_input_coordinates()", LogInfo
            );
            if (LogInfo->stopRun) {
                return; /* Exit function prematurely due to error */
            }

            valPtr = (void *) tempFVals;
        }
        (*xyVals[varNum]) = (double *) Mem_Malloc(
            sizeof(double) * numPoints, "get_2D_input_coordinates()", LogInfo
        );
        if (LogInfo->stopRun) {
            free(tempFVals);
            return; /* Exit function prematurely due to error */
        }

        SW_NC_get_vals(
            ncFileID, &varIDs[varNum], yxVarNames[varNum], valPtr, LogInfo
        );
        if (LogInfo->stopRun) {
            free(tempFVals);
            return; /* Exit function prematurely due to error */
        }

        if (varType == NC_FLOAT) {
            for (copyIndex = 0; copyIndex < numPoints; copyIndex++) {
                (*xyVals[varNum])[copyIndex] = tempFVals[copyIndex];
            }
        }
    }

    if (compareCoords) {
        *useIndexFile = swFALSE;

        determine_index_file_use(
            domYVals,
            domXVals,
            *readinYVals,
            *readinXVals,
            numPoints,
            numPoints,
            spatialTol,
            useIndexFile
        );
    }
}

/**
@brief Read coordinates from the given input file and read them differently
depending on if they are 1D or 2D

@param[in,out] SW_netCDFIn Constant netCDF input file information
@param[in] ncFileID Identifier of the open netCDF file to write all information
@param[in] inFileName Name of the current file being read
@param[in] dimSizes Sizes of the dimensions of the variables to read
@param[out] coordVarIs2D Specifies if the variables are 2D
@param[in] k Input key that is being read in (e.g., weather, `eSW_InWeather`)
@param[in] spatialTol User-provided tolerance for comparing spatial coordinates
@param[out] readinYVals Read-in latitude/y values
@param[out] readinXVals Read-in longitude/x values
@param[in] yxVarNames A list of two variable names for the input variables
that contain latitude/y and longitude/x coordinate values
@param[in] compareCoords Specifies if the function should compare the
coordinates to the domain coordinates
@param[in] inPrimCRSIsGeo Specifies if the input file has a primary
CRS of geographical
@param[out] LogInfo Holds information dealing with logfile output
*/
static void get_input_coordinates(
    SW_NETCDF_IN *SW_netCDFIn,
    int *ncFileID,
    char *inFileName,
    size_t **dimSizes,
    Bool *coordVarIs2D,
    int k,
    double spatialTol,
    double **readinYVals,
    double **readinXVals,
    char **yxVarNames,
    Bool compareCoords,
    Bool inPrimCRSIsGeo,
    LOG_INFO *LogInfo
) {
    const int numReadInDims = 2;

    if (*ncFileID == -1 && !isnull(inFileName)) {
        SW_NC_open(inFileName, NC_NOWRITE, ncFileID, LogInfo);
        if (LogInfo->stopRun) {
            return;
        }
    }

    *coordVarIs2D = spatial_var_is_2d(*ncFileID, yxVarNames[0], LogInfo);
    if (LogInfo->stopRun) {
        return;
    }

    if (*coordVarIs2D) {
        get_2D_input_coordinates(
            SW_netCDFIn,
            *ncFileID,
            readinYVals,
            readinXVals,
            dimSizes,
            yxVarNames,
            numReadInDims,
            &SW_netCDFIn->useIndexFile[k],
            compareCoords,
            spatialTol,
            inPrimCRSIsGeo,
            LogInfo
        );
    } else {
        get_1D_input_coordinates(
            SW_netCDFIn,
            *ncFileID,
            readinYVals,
            readinXVals,
            dimSizes,
            yxVarNames,
            numReadInDims,
            &SW_netCDFIn->useIndexFile[k],
            compareCoords,
            spatialTol,
            inPrimCRSIsGeo,
            LogInfo
        );
    }
}

/**
@brief Make sure that a calendar for the input weather files is one that
we accept

@param[in] calType Input file provided calendar type
@param[in] calUnit Input file provided start date (unit) for the first
weather file
@param[in] calIsNoLeap Specifies if the calendar only contains 365 days
@param[in] fileName Weather input file name
@param[out] LogInfo Holds information dealing with logfile output
*/
static void determine_valid_cal(
    char *calType,
    char *calUnit,
    Bool *calIsNoLeap,
    char *fileName,
    LOG_INFO *LogInfo
) {

    const char *const acceptableCals[] = {
        /* 365 - 366 days */
        "standard",
        "gregorian",
        "proleptic_gregorian",

        /* Always has 366 days and will discard values */
        "all_leap",
        "alleap",
        "366day",
        "366_day",

        /* No leap day */
        "no_leap",
        "noleap",
        "365day",
        "365_day"
    };

    const int numPossCals = 11;
    int index;
    Bool matchFound = swFALSE;
    int year = 0;
    char monthDay[MAX_FILENAMESIZE] = {'\0'};
    char yearStr[5] = {'\0'}; /* Hold 4-digit year and '\0' */
    int scanRes;

    for (index = 0; index < numPossCals && !matchFound; index++) {
        if (Str_CompareI(calType, (char *) acceptableCals[index]) == 0) {
            /* Gregorian calendars that are not used before August 10, 1582 */
            /* days since <year>-01-01 00:00:00 */
            if (index == 0 || index == 1) {
                scanRes =
                    sscanf(calUnit, "days since %4s-%s", yearStr, monthDay);

                if (scanRes < 0 || scanRes > MAX_FILENAMESIZE) {
                    LogError(
                        LogInfo,
                        LOGERROR,
                        "Could not read input file calendar units."
                    );
                    return; /* Exit function prematurely due to error */
                }

                year = sw_strtoi(yearStr, fileName, LogInfo);
                if (LogInfo->stopRun) {
                    return; /* Exit function prematurely due to error */
                }

                if (year <= 1582) {
                    LogError(
                        LogInfo,
                        LOGERROR,
                        "Starting year <= 1582 is not supported."
                    );
                    return; /* Exit function prematurely due to error */
                }

            } else if (index >= 3 && index <= 6) { /* All leap calendars */
                LogError(
                    LogInfo,
                    LOGERROR,
                    "The usage of the calendar '%s' has been detected. "
                    "When not a leap year, this will result in the 366th "
                    "value within the year being ignored.",
                    calType
                );
            } else if (index >= 7 && index <= 11) { /* No leap calendars */
                LogError(
                    LogInfo,
                    LOGWARN,
                    "The usage of the calendar '%s' has been detected. "
                    "This results in missing values on leap days.",
                    calType
                );
                *calIsNoLeap = swTRUE;
            }

            matchFound = swTRUE;
        }
    }

    if (!matchFound) {
        LogError(
            LogInfo, LOGERROR, "Calendary type '%s' is not supported.", calType
        );
    }
}

/**
@brief Helper function to allocate weather input file indices

@param[out] ncWeatherStartEndIndices Start/end indices for the current
weather input file
@param[in] numStartEndIndices Number of start/end index pairs
@param[out] LogInfo Holds information dealing with logfile output
*/
static void alloc_weather_indices(
    unsigned int ***ncWeatherStartEndIndices,
    unsigned int numStartEndIndices,
    LOG_INFO *LogInfo
) {
    unsigned int index;

    (*ncWeatherStartEndIndices) = (unsigned int **) Mem_Malloc(
        sizeof(unsigned int *) * numStartEndIndices,
        "alloc_weather_indices()",
        LogInfo
    );
    if (LogInfo->stopRun) {
        return; /* Exit function prematurely due to error */
    }

    for (index = 0; index < numStartEndIndices; index++) {
        (*ncWeatherStartEndIndices)[index] = NULL;
    }

    for (index = 0; index < numStartEndIndices; index++) {
        (*ncWeatherStartEndIndices)[index] = (unsigned int *) Mem_Malloc(
            sizeof(unsigned int) * 2, "alloc_weather_indices()", LogInfo
        );
        if (LogInfo->stopRun) {
            return; /* Exit function prematurely due to error */
        }
    }
}

#if defined(SWUDUNITS)
/**
@brief Convert a given number of days from one start date to another

@param[in] system Conversion system used for translating from one
unit to another
@param[in] hasTimeUnit Start unit for number of days
@param[in] reqTimeUnit Destination unit for number of days
*/
static double conv_times(
    ut_system *system, char *hasTimeUnit, char *reqTimeUnit
) {
    double res;
    ut_unit *unitFrom = ut_parse(system, reqTimeUnit, UT_UTF8);
    ut_unit *unitTo = ut_parse(system, hasTimeUnit, UT_UTF8);
    cv_converter *conv = ut_get_converter(unitFrom, unitTo);

    res = cv_convert_double(conv, 1) - 1;

    ut_free(unitFrom);
    ut_free(unitTo);
    cv_free(conv);

    return res;
}
#endif

/**
@brief Read the temporal values of a weather input file

@param[in] ncFileID Identifier of the open netCDF file to write all information
@param[out] timeVals Temporal values that were contained in the input file
@param[in] timeName User-provided name for "time" dimension/variable
@param[in] timeSize Time dimension size
@param[out] LogInfo Holds information dealing with logfile output
*/
static void get_temporal_vals(
    int ncFileID,
    double **timeVals,
    char *timeName,
    size_t *timeSize,
    LOG_INFO *LogInfo
) {
    size_t valNum;
    int varID = -1;
    nc_type ncVarType = 0;
    size_t *timeSizeArr[] = {timeSize};
    char *timeNameArr[] = {timeName};
    int *tempIntVals = NULL;
    float *tempFloatVals = NULL;
    void *valPtr = NULL;

    get_var_dimsizes(ncFileID, 1, timeSizeArr, timeNameArr[0], &varID, LogInfo);
    if (LogInfo->stopRun) {
        return; /* Exit function prematurely due to error */
    }

    if (nc_inq_vartype(ncFileID, varID, &ncVarType) != NC_NOERR) {
        LogError(
            LogInfo,
            LOGERROR,
            "Could not get the type of the variable '%s'.",
            timeName
        );
        return;
    }

    *timeVals = (double *) Mem_Malloc(
        sizeof(double) * *timeSize, "get_temporal_vals()", LogInfo
    );
    if (LogInfo->stopRun) {
        return; /* Exit function prematurely due to error */
    }

    valPtr = (void *) *timeVals;

    switch (ncVarType) {
    case NC_INT:
        tempIntVals = (int *) Mem_Malloc(
            sizeof(int) * *timeSize, "get_temporal_vals()", LogInfo
        );
        valPtr = (void *) tempIntVals;
        break;
    case NC_FLOAT:
        tempFloatVals = (float *) Mem_Malloc(
            sizeof(float) * *timeSize, "get_temporal_vals()", LogInfo
        );
        valPtr = (void *) tempFloatVals;
        break;
    default: /* Do nothing - type is NC_FLOAT */
        break;
    }
    if (LogInfo->stopRun) {
        return; /* Exit function prematurely due to error */
    }

    SW_NC_get_vals(ncFileID, &varID, timeName, valPtr, LogInfo);
    if (LogInfo->stopRun) {
        goto freeTempMem;
    }

    if (ncVarType == NC_INT || ncVarType == NC_FLOAT) {
        for (valNum = 0; valNum < *timeSize; valNum++) {
            switch (ncVarType) {
            case NC_INT:
                (*timeVals)[valNum] = (double) tempIntVals[valNum];
                break;
            case NC_FLOAT:
                (*timeVals)[valNum] = (double) tempFloatVals[valNum];
                break;
            default:
                break;
            }
        }
    }

freeTempMem:
    if (!isnull(tempIntVals)) {
        free(tempIntVals);
        tempIntVals = NULL;
    }

    if (!isnull(tempFloatVals)) {
        free(tempFloatVals);
        tempFloatVals = NULL;
    }
}

/**
@brief Convert number of days since the input-provided start date to
file-specific indices to know the start/end indices of the temporal
dimension for weather

@param[out] ncWeatherStartEndIndices Start/end indices for the current
weather input file
@param[in] timeVals List of time values from the current weather input file
@param[in] timeSize Time dimension size
@param[in] target Start temporal value that we will search for to get the
index
@param[in] year Current year we are calculating the indices for
@param[in] fileName Weather input file name that is being searched
@param[in] timeName User-provided time variable/dimension name
@param[out] LogInfo Holds information dealing with logfile output
*/
static void get_startend_indices(
    unsigned int *ncWeatherStartEndIndices,
    const double *timeVals,
    size_t timeSize,
    double target,
    TimeInt year,
    char *fileName,
    char *timeName,
    LOG_INFO *LogInfo
) {
    int left = 0;
    int right = (int) timeSize - 1;
    int middle;

    /* base 0 */
    int numDays = (MAX_DAYS - 1) + (isleapyear(year) ? 1 : 0) - 1;

    while (left <= right) {
        middle = left + (right - left) / 2;

        // NOLINTNEXTLINE(clang-analyzer-core.NullDereference)
        if (timeVals[middle] > target) {
            right = middle - 1;
        } else if (timeVals[middle] < target) {
            left = middle + 1;
        } else {
            /* Set start/end indices */
            ncWeatherStartEndIndices[0] = middle;
            ncWeatherStartEndIndices[1] = middle + numDays;

            return; /* No longer search for results, they were found */
        }
    }

    LogError(
        LogInfo,
        LOGERROR,
        "Could not find the '%s' value '%f' in '%s'.",
        timeName,
        target,
        fileName
    );
}

/**
@brief Similar to that of the spatial coordinates, the temporal information
provided in weather input files will not be something that the program can
understand, so this function will generate indices that we can understand
and use to index through the time dimension in weather input files

@param[in] SW_netCDFIn Constant netCDF input file information
@param[out] SW_PathInputs Struct of type SW_PATH_INPUTS which
holds basic information about input files and values
@param[in] startYr Start year of the simulation
@param[in] endYr End year of the simulation
@param[out] LogInfo Holds information dealing with logfile output
*/
static void calc_temporal_weather_indices(
    SW_NETCDF_IN *SW_netCDFIn,
    SW_PATH_INPUTS *SW_PathInputs,
    TimeInt startYr,
    TimeInt endYr,
    LOG_INFO *LogInfo
) {

    TimeInt year;
    int fileIndex = 0;
    int probeIndex = -1;
    int varIndex = 1;
    unsigned int numStartEndIndices;
    unsigned int weatherEnd;
    Bool hasCalOverride = swFALSE;
    Bool checkedCal = swFALSE;
    char *weatherCal = NULL;
    char *fileName;
    double valDoy1 = 0.0;
    double valDoy1Add = 0.0;

    unsigned int numWeathFiles = SW_PathInputs->ncNumWeatherInFiles;
    char **weathInFiles = NULL;

    int ncFileID = -1;

    char newCalUnit[MAX_FILENAMESIZE] = "\0";
    char currCalType[MAX_FILENAMESIZE] = "\0";
    char currCalUnit[MAX_FILENAMESIZE] = "\0";
    char *timeName = NULL;
    Bool calIsNoLeap = swFALSE;
    double *timeVals = NULL;
    size_t timeSize = 0;

#if defined(SWUDUNITS)
    ut_system *system;

    /* silence udunits2 error messages */
    ut_set_error_message_handler(ut_ignore);

    /* Load unit system database */
    system = ut_read_xml(NULL);
#endif

    /* Get the first available list of input files */
    while (probeIndex == -1) {
        probeIndex = (SW_netCDFIn->readInVars[eSW_InWeather][varIndex + 1]) ?
                         varIndex :
                         -1;

        if (probeIndex == -1) {
            varIndex++;
        }
    }

    weathInFiles = SW_PathInputs->ncWeatherInFiles[varIndex];
    timeName = SW_netCDFIn->inVarInfo[eSW_InWeather][varIndex][INTAXIS];

    weatherCal = SW_netCDFIn->weathCalOverride[varIndex];
    hasCalOverride = (Bool) (strcmp(weatherCal, "NA") != 0);

    /* Determine the first weather input file to start with */
    while (SW_PathInputs->ncWeatherInStartEndYrs[fileIndex][1] < startYr) {
        fileIndex++;
    }

    numStartEndIndices = numWeathFiles - fileIndex;

    alloc_weather_indices(
        &SW_PathInputs->ncWeatherStartEndIndices, numStartEndIndices, LogInfo
    );
    if (LogInfo->stopRun) {
        return; /* Exit function prematurely due to error */
    }

    /* Go through each year and get the indices within the input file(s) */
    for (year = startYr; year <= endYr; year++) {
        weatherEnd = SW_PathInputs->ncWeatherInStartEndYrs[fileIndex][1];
        fileName = weathInFiles[fileIndex];

        /* Increment file and reset all information for a new file */
        if (year > weatherEnd) {
            fileIndex++;
            currCalType[0] = currCalUnit[0] = newCalUnit[0] = '\0';

            ncFileID = -1;
        }

        if (ncFileID == -1) {
            SW_NC_open(fileName, NC_NOWRITE, &ncFileID, LogInfo);
            if (LogInfo->stopRun) {
                goto freeMem;
            }
        }

        /* Get information from new file if it's newly opened */
        if (currCalUnit[0] == '\0') {
            if (!checkedCal) {
                if (!hasCalOverride) {
                    SW_NC_get_str_att_val(
                        ncFileID, timeName, "calendar", currCalType, LogInfo
                    );
                    if (LogInfo->stopRun) {
                        goto freeMem;
                    }
                    weatherCal = currCalType;
                }
            }
            SW_NC_get_str_att_val(
                ncFileID, timeName, "units", currCalUnit, LogInfo
            );
            if (LogInfo->stopRun) {
                goto freeMem;
            }

            if (!checkedCal) {
                determine_valid_cal(
                    weatherCal, currCalUnit, &calIsNoLeap, fileName, LogInfo
                );
                if (LogInfo->stopRun) {
                    goto freeMem;
                }

                checkedCal = swTRUE;
            }
        }

        snprintf(
            newCalUnit, MAX_FILENAMESIZE, "days since %u-01-01 00:00:00", year
        );

        get_temporal_vals(ncFileID, &timeVals, timeName, &timeSize, LogInfo);
        if (LogInfo->stopRun) {
            goto freeMem;
        }

        if (timeSize <= 0) {
            LogError(
                LogInfo,
                LOGERROR,
                "Time dimension size must be > 0 in '%s'.",
                fileName
            );
            goto freeMem;
        }

        // NOLINTBEGIN(clang-analyzer-core.NullDereference)
#if defined(SWUDUNITS)
        /* Calculate the first day of the year in nc file
           the provided time values may be double whole numbers
           rather than x.5, so check to see if you do the + 0.5
           at the end of the calculation */
        valDoy1Add = (fmod(timeVals[timeSize - 1], 1.0) == 0.0) ? 0.0 : 0.5;
        valDoy1 = conv_times(system, currCalUnit, newCalUnit) + valDoy1Add;
#endif

        get_startend_indices(
            SW_PathInputs->ncWeatherStartEndIndices[fileIndex],
            timeVals,
            timeSize,
            valDoy1,
            year,
            fileName,
            timeName,
            LogInfo
        );
        // NOLINTEND(clang-analyzer-core.NullDereference)
        if (LogInfo->stopRun) {
            goto freeMem;
        }

        free(timeVals);
        timeVals = NULL;
    }

freeMem: {
    if (!isnull(timeVals)) {
        free(timeVals);
        timeVals = NULL;
    }

    if (ncFileID == -1) {
        nc_close(ncFileID);
    }
#if defined(SWUDUNITS)
    ut_free_system(system);
#endif
}
}

/**
@brief Free provided temporary locations for coordinate values
and close open files

@param[out] tempCoords A list holding all of the temporary coordinate
lists to (possibly) be freed
@param[out] fileIDs A list holding all nc file identifiers to (possibly)
close
@param[in] numCoordVars Number of coordinate variables to free
@param[in] numFiles Number of nc files to close
*/
static void free_tempcoords_close_files(
    double ***tempCoords, int *fileIDs[], int numCoordVars, int numFiles
) {
    int index;

    for (index = 0; index < numCoordVars; index++) {
        if (!isnull(*(tempCoords[index]))) {
            free((void *) *(tempCoords[index]));
            *(tempCoords[index]) = NULL;
        }
    }

    for (index = 0; index < numFiles; index++) {
        if (*(fileIDs[index]) > -1) {
            nc_close(*(fileIDs[index]));
            *(fileIDs[index]) = -1;
        }
    }
}

/**
@brief Determine if the spatial coordinates within the input file
being tested is the same as that of which the program understands
(our own domain generated in `domain.nc`)

@param[in,out] SW_netCDFIn Constant netCDF input file information
@param[in] spatialTol User-provided tolerance for comparing spatial coordinates
@param[out] LogInfo Holds information dealing with logfile output
*/
static void determine_indexfile_use(
    SW_NETCDF_IN *SW_netCDFIn,
    SW_PATH_INPUTS *SW_PathInputs,
    const double spatialTol,
    LOG_INFO *LogInfo
) {
    int k;
    int ncFileID;
    int fIndex;

    char *fileName;
    char *axisNames[2] = {NULL, NULL}; /* Set later */
    char *gridMap;

    size_t ySize = 0;
    size_t xSize = 0;
    size_t *dimSizes[] = {&ySize, &xSize};
    Bool coordVarIs2D = swFALSE;
    Bool inPrimCRSIsGeo;

    double *tempY = NULL;
    double *tempX = NULL;
    double **freeArr[] = {&tempY, &tempX};
    const int numFreeArr = 2;
    const int numFileClose = 0;

    ForEachNCInKey(k) {
        if (k > eSW_InSpatial && SW_netCDFIn->readInVars[k][0]) {
            fIndex = 1;
            ncFileID = -1;

            /* Find the first input file within the key */
            while (!SW_netCDFIn->readInVars[k][fIndex + 1]) {
                fIndex++;
            }

            if (k == eSW_InWeather) {
                fileName = SW_PathInputs->ncWeatherInFiles[fIndex][0];
            } else {
                fileName = SW_PathInputs->ncInFiles[k][fIndex];
            }

            axisNames[0] = SW_netCDFIn->inVarInfo[k][fIndex][INYAXIS];
            axisNames[1] = SW_netCDFIn->inVarInfo[k][fIndex][INXAXIS];

            gridMap = SW_netCDFIn->inVarInfo[k][fIndex][INGRIDMAPPING];
            inPrimCRSIsGeo =
                (Bool) (strcmp(gridMap, (char *) "latitude_longitude") == 0);

            get_input_coordinates(
                SW_netCDFIn,
                &ncFileID,
                fileName,
                dimSizes,
                &coordVarIs2D,
                k,
                spatialTol,
                &tempY,
                &tempX,
                axisNames,
                swTRUE,
                inPrimCRSIsGeo,
                LogInfo
            );
            if (LogInfo->stopRun) {
                goto freeMem;
            }

            if (SW_netCDFIn->useIndexFile[k] &&
                !SW_netCDFIn->readInVars[k][1]) {

                LogError(
                    LogInfo,
                    LOGERROR,
                    "Detected need to use index file for the input key "
                    "'%s' but index file ('indexSpatial') input is "
                    "turned off.",
                    possInKeys[k]
                );
            }

        freeMem:
            nc_close(ncFileID);

            free_tempcoords_close_files(
                freeArr, NULL, numFreeArr, numFileClose
            );
        }
    }
}

/**
@brief Create the individual index variable(s) that were determined in
`get_index_vars_info()`

@param[out] varIDs
@param[in] numVarsToWrite Number of variables to write into the index file
@param[in] indexVarNames A list of all index variable names that are to be
created
@param[in] dimIDs Dimension identifiers for the index variables
@param[in] templateID New index file identifier to write to
@param[in] nDims Number of dimensions for each coordinate variable
@param[in] deflateLevel Level of deflation that will be used for the created
variable
@param[in] inDomIsSite Specifies if the input file has sites or is gridded
@param[in] siteDom Specifies that the programs domain has sites, otherwise
it is gridded
@param[in] numAtts Number of attributes to give to each index variable(s)
@param[in] key Current input key these variables/file is meant for
@param[in] indexFileName Name of the newly created index file
@param[in] geoYCoordName Name of the geographical coordinate variable/dimension
latitude/y (user-provided)
@param[in] geoXCoordName Name of the geographical coordinate variable/dimension
longitude/x (user-provided)
@param[in] domSiteName User-provided site variable/dimension name (if domain is
not gridded)
@param[out] LogInfo Holds information dealing with logfile output
*/
static void create_index_vars(
    int varIDs[],
    int numVarsToWrite,
    char *indexVarNames[],
    int dimIDs[][2],
    int templateID,
    int nDims,
    int deflateLevel,
    Bool inDomIsSite,
    Bool siteDom,
    int numAtts,
    int key,
    char *indexFileName,
    char *geoYCoordName,
    char *geoXCoordName,
    char *domSiteName,
    LOG_INFO *LogInfo
) {
    int varNum;
    int attNum;

    size_t chunkSizes[] = {1, 1};
    char *indexVarAttNames[] = {
        (char *) "long_name",
        (char *) "comment",
        (char *) "units",
        (char *) "coordinates"
    };
    char *coordString = (siteDom) ? (char *) "%s %s %s" : (char *) "%s %s";

    /* Size 3 - "long_name", "comment", "units" */
    /* Size 2 - maximum possible values to write out for variable(s) */
    char *indexVarAttVals[4][2] = {
        {/* long_name, set later dynamically */
         NULL,
         NULL
        },
        {/* comment */
         (char *) "Spatial index (base 0) between simulation domain and inputs "
                  "of %s"
        },
        {/* units */
         (char *) "1"
        },
        {/* coordinates, set later */
         NULL
        }
    };

    char tempIndexVal[MAX_FILENAMESIZE];
    char tempCoord[MAX_FILENAMESIZE];
    char *tempIndexValPtr;

    for (varNum = 0; varNum < numVarsToWrite; varNum++) {
        SW_NC_create_netCDF_var(
            &varIDs[varNum],
            indexVarNames[varNum],
            dimIDs[varNum],
            &templateID,
            NC_UINT,
            nDims,
            chunkSizes,
            deflateLevel,
            LogInfo
        );
        if (LogInfo->stopRun) {
            return;
        }

        if (inDomIsSite) {
            indexVarAttVals[0][0] = (char *) "site-position of %s";
        } else {
            indexVarAttVals[0][0] = (char *) "y-position of %s";
            indexVarAttVals[0][1] = (char *) "x-position of %s";
        }

        if (siteDom) {
            snprintf(
                tempCoord,
                MAX_FILENAMESIZE,
                coordString,
                geoYCoordName,
                geoXCoordName,
                domSiteName
            );
        } else {
            snprintf(
                tempCoord,
                MAX_FILENAMESIZE,
                coordString,
                geoYCoordName,
                geoXCoordName
            );
        }
        indexVarAttVals[3][0] = (char *) tempCoord;

        for (attNum = 0; attNum < numAtts; attNum++) {
            tempIndexVal[0] = '\0';

            tempIndexValPtr = tempIndexVal;
            switch (attNum) {
            case 0:
                snprintf(
                    tempIndexVal,
                    MAX_FILENAMESIZE,
                    indexVarAttVals[attNum][varNum],
                    possInKeys[key]
                );
                break;
            case 1:
                snprintf(
                    tempIndexVal,
                    MAX_FILENAMESIZE,
                    indexVarAttVals[attNum][0],
                    indexFileName
                );
                break;
            default:
                tempIndexValPtr = indexVarAttVals[attNum][0];
                break;
            }

            SW_NC_write_string_att(
                indexVarAttNames[attNum],
                tempIndexValPtr,
                varIDs[varNum],
                templateID,
                LogInfo
            );
            if (LogInfo->stopRun) {
                return;
            }
        }
    }
}

/**
@brief Determine and write indices for the given input key, in other
words, find and write out the translation index from what the programs
domain knows to the closest spatial point that the input file(s) provide

@param[in] domYCoords Latitude/y domain coordinates pertaining
@param[in] domXCoords Longitude/x domain coordinates pertaining
@param[in] yDomSize Latitude/y domain dimension size
@param[in] xDomSize Latitude/y domain dimension size
@param[in] yCoords Latitude/y coordinates read-in from the user-provided file
@param[in] xCoords Longitude/x coordinates read-in from the user-provided file
@param[in] inIsGridded Specifies that the provided input file is gridded
@param[in] siteDom Specifies that the programs domain has sites, otherwise
it is gridded
@param[in] inPrimCRSIsGeo Specifies if the CRS in the provided input file is
geographical or projected
@param[in] indexVarIDs Identifiers of the created variable(s) in the new
index file that will be written to
@param[in] templateID New index file identifier to write to
@param[in] indexVarName Variable name(s) within the new index file to
write to and will hold the indices to translate from the programs domain
to the input file's domain
@param[in] indexFileName Name of the newly created index file
@param[in] inFileDimSizes Dimension sizes of the spatial coordinates within
the input file that the index file is being created for
@param[in] has2DCoordVars Specifies if the read-in coordinates from an input
file came from a 2-dimensional variable
@param[in] spatialTol User-provided tolerance for comparing spatial coordinates
@param[out] LogInfo Holds information dealing with logfile output
*/
static void write_indices(
    const double *domYCoords,
    const double *domXCoords,
    size_t yDomSize,
    size_t xDomSize,
    double *yCoords,
    double *xCoords,
    Bool inIsGridded,
    Bool siteDom,
    Bool inPrimCRSIsGeo,
    int indexVarIDs[],
    int templateID,
    char *indexVarName[],
    char *indexFileName,
    size_t **inFileDimSizes,
    Bool has2DCoordVars,
    double spatialTol,
    LOG_INFO *LogInfo
) {
    SW_KD_NODE *treeRoot = NULL;
    SW_KD_NODE *nearNeighbor = NULL;

    double bestDist;
    double queryCoords[2] = {0};

    size_t yIndex;
    size_t xIndex;
    size_t syWritePos = 0;
    size_t xWritePos = 0;
    size_t writeCount[] = {1, 1};

    SW_DATA_create_tree(
        &treeRoot,
        yCoords,
        xCoords,
        *(inFileDimSizes[0]),
        *(inFileDimSizes[1]),
        inIsGridded,
        has2DCoordVars,
        inPrimCRSIsGeo,
        LogInfo
    );
    if (LogInfo->stopRun) {
        goto freeTree;
    }

    for (yIndex = 0UL; yIndex < yDomSize; yIndex++) {
        queryCoords[0] = domYCoords[yIndex];
        syWritePos = yIndex;

        for (xIndex = 0UL; xIndex < xDomSize; xIndex++) {
            queryCoords[1] = domXCoords[xIndex];
            xWritePos = xIndex;

            bestDist = DBL_MAX;
            nearNeighbor = NULL;

            if (siteDom) {
                queryCoords[0] = domYCoords[xIndex];
                syWritePos = xIndex;
            }

            SW_DATA_queryTree(
                treeRoot,
                queryCoords,
                0,
                inPrimCRSIsGeo,
                &nearNeighbor,
                &bestDist
            );

            if (!isnull(nearNeighbor)) {
                SW_NC_write_vals(
                    (siteDom) ? &indexVarIDs[1] : &indexVarIDs[0],
                    templateID,
                    (siteDom) ? indexVarName[1] : indexVarName[0],
                    &nearNeighbor->indices[1],
                    &syWritePos,
                    writeCount,
                    "unsigned int",
                    LogInfo
                );
                if (LogInfo->stopRun) {
                    goto freeTree;
                }

                if (siteDom) {
                    SW_NC_write_vals(
                        &indexVarIDs[0],
                        templateID,
                        indexVarName[0],
                        &nearNeighbor->indices[0],
                        &xWritePos,
                        writeCount,
                        "unsigned int",
                        LogInfo
                    );
                    if (LogInfo->stopRun) {
                        goto freeTree;
                    }
                }
            } else {
                LogError(
                    LogInfo,
                    LOGERROR,
                    "Could not find a best match for the "
                    "edge coordinates (%f, %f) when creating the "
                    "index file '%s'.",
                    queryCoords[0],
                    queryCoords[1],
                    indexFileName
                );
                goto freeTree;
            }

            if (siteDom && !inIsGridded) {
                if (!EQ_w_tol(
                        nearNeighbor->coords[0], queryCoords[0], spatialTol
                    ) ||
                    !EQ_w_tol(
                        nearNeighbor->coords[1], queryCoords[1], spatialTol
                    )) {

                    LogError(
                        LogInfo,
                        LOGERROR,
                        "Could not find a direct match within tolerance for "
                        "site with the coordinates of [%f, %f].",
                        queryCoords[0],
                        queryCoords[1]
                    );

                    goto freeTree;
                }
            }
        }

        if (siteDom) {
            goto freeTree;
        }
    }

freeTree:
    treeRoot = SW_DATA_destroyTree(treeRoot);
}

/**
@brief Get the names and dimension lengths of the index variables that
will be created as a translation between the spatial coordinates contained
in the programs domain and the input file domain(s)

@param[in] siteDom Specifies that the programs domain has sites, otherwise
it is gridded
@param[in] inFileID Input file identifier
@param[in] templateID Identifier of the newly created index file
@param[in] domYName Name of the programs domain latitude/y axis/variable
@param[in] domXName Name of the programs domain longitude/x axis/variable
@param[in] inHasSite Specifies if the input file has sites or is gridded
@param[out] nDims Number of dimensions for each coordinate variable
@param[out] dimIDs Identifier of the coordinate dimensions
@param[out] indexVarNames A list of all index variable names that are to be
created
@param[out] numVars Number of variables to create (1 - site, 2 - gridded)
@param[out] LogInfo Holds information dealing with logfile output
*/
static void get_index_vars_info(
    Bool siteDom,
    int inFileID,
    int *nDims,
    int templateID,
    char *domYName,
    char *domXName,
    int dimIDs[][2],
    Bool inHasSite,
    char *siteName,
    char *indexVarNames[],
    int *numVars,
    LOG_INFO *LogInfo
) {
    int varNum;
    char *varNames[] = {domYName, domXName};

    if (inHasSite && !SW_NC_dimExists(siteName, inFileID)) {
        LogError(
            LogInfo,
            LOGERROR,
            "Input spreadsheet claims to have site variable '%s' "
            "but it is not seen in the input nc file itself.",
            siteName
        );
    } else if (inHasSite && !siteDom) {
        LogError(
            LogInfo,
            LOGERROR,
            "Input file has sites while the domain is an xy domain."
        );
        return;
    }

    indexVarNames[0] = (inHasSite) ? (char *) "site_index" : (char *) "y_index";
    indexVarNames[1] = (inHasSite) ? (char *) "" : (char *) "x_index";
    *numVars = (inHasSite) ? 1 : 2;

    for (varNum = 0; varNum < *numVars; varNum++) {
        SW_NC_get_vardimids(
            templateID, -1, varNames[varNum], dimIDs[varNum], nDims, LogInfo
        );
        if (LogInfo->stopRun) {
            return;
        }
    }
}

/**
@brief Get attribute values of type double

@param[in] ncFileID File identifier to get value(s) from
@param[in] varID Variable identifier to get value(s) from
@param[in] attName Attribute name to get the value(s) of
@param[out] vals List of read-in values from the variable's attribute
@param[out] LogInfo Holds information on warnings and errors
*/
static void get_att_double(
    int ncFileID, int varID, const char *attName, double *vals, LOG_INFO *LogInfo
) {
    if (nc_get_att_double(ncFileID, varID, attName, vals) != NC_NOERR) {
        LogError(
            LogInfo,
            LOGERROR,
            "Could not get a value from the attribute '%s'.",
            attName
        );
    }
}

/**
@brief Helper function to test if a variable has an attribute by the
given name

@param[in] ncFileID File identifier to test within
@param[in] varID Variable identifier to test if it contains the queried
attribute
@param[in] attName Attribute name to test for
@param[out] attSize If the attribute exists, this will hold the number of
values the attribute contains
@param[out] attExists Specifies if the attribute we are querying exists within
the provided variable
@param[out] LogInfo Holds information on warnings and errors
*/
static void att_exists(
    int ncFileID,
    int varID,
    const char *attName,
    size_t *attSize,
    Bool *attExists,
    LOG_INFO *LogInfo
) {
    int result = nc_inq_attlen(ncFileID, varID, attName, attSize);

    if (result != NC_NOERR && result != NC_ENOTATT) {
        LogError(
            LogInfo,
            LOGERROR,
            "Could not get information on the attribute '%s'.",
            attName
        );
    }

    *attExists = (Bool) (result != NC_ENOTATT && !LogInfo->stopRun);
}

/**
@brief Compare a user-provided input file against the program-generated/
user-provided index file to make sure the following criteria matches:
    * (Auxilary) Spatial coordinate variable names
    * Certain attributes of the CRS that we should expect

@param[in] indexFileID Index file identifier
@param[in] testFileID Testing file (input) identifier
@param[in] spatialNames Spatial variable names to test for
@param[in] numSpatialVars The number of spatial variables to test for
@param[in] indexFileName The name of the index file
@param[in] testFileName The name of user-provided input
@param[in] indexCRSName The name of the CRS variable within the index file
@param[in] testCRSName The name of the CRS variable within the input file
@param[in,out] LogInfo Holds information on warnings and errors
*/
static void check_input_file_against_index(
    int indexFileID,
    int testFileID,
    char **spatialNames,
    int numSpatialVars,
    char *indexFileName,
    char *testFileName,
    char *indexCRSName,
    char *testCRSName,
    LOG_INFO *LogInfo
) {
    int index;
    int att;
    const int numDimsAndVars = 2;
    nc_type attType;

    size_t indexAttSize = 0;
    size_t testAttSize = 0;
    size_t *attSizes[] = {&indexAttSize, &testAttSize};
    Bool indexHasVar;
    Bool testHasVar;
    Bool indexAttExists;
    Bool testAttExists;
    Bool *attExists[] = {&indexAttExists, &testAttExists};
    Bool indexCRSExists = SW_NC_varExists(indexFileID, indexCRSName);
    Bool testCRSExists = SW_NC_varExists(testFileID, testCRSName);

    int fileIDs[] = {indexFileID, testFileID};
    int indexVarID = -1;
    int testVarID = -1;
    int *varIDs[] = {&indexVarID, &testVarID};
    char indexCRSAtt[MAX_FILENAMESIZE];
    char testCRSAtt[MAX_FILENAMESIZE];
    char *crsAttVals[] = {indexCRSAtt, testCRSAtt};
    const int numCrsAtts = 9;
    const char *crsAttNames[] = {
        "grid_mapping_name",
        "semi_major_axis",
        "inverse_flattening",
        "longitude_of_prime_meridian",
        "longitude_of_central_meridian",
        "latitude_of_projection_origin",
        "false_easting",
        "false_northing",
        "standard_parallel"
    };
    char *CRSNames[] = {indexCRSName, testCRSName};

    /* Standard parallel values 1 or 2 */
    double indexDoubleVals[2] = {0.0};
    double testDoubleVals[2] = {0.0};
    double *doubleVals[] = {indexDoubleVals, testDoubleVals};

    /* Identical spatial coordinate variable names */
    for (index = 0; index < numSpatialVars; index++) {
        indexHasVar = SW_NC_varExists(indexFileID, spatialNames[index]);
        testHasVar = SW_NC_varExists(testFileID, spatialNames[index]);

        if (!indexHasVar && !testHasVar) {
            LogError(
                LogInfo,
                LOGERROR,
                "Both the index file, '%s', and the input file, '%s',"
                "do not have the expected variable '%s'.",
                indexFileName,
                testFileName,
                spatialNames[index]
            );
        } else if (!indexHasVar || !testHasVar) {
            /* Example error message:
               "The input file, 'Input_nc/inWeather/weather.nc', does not have
                the spatial variable 'latitude' while the index counterpart,
                'Input_nc/inWeather/index_weather.nc', does." */
            LogError(
                LogInfo,
                LOGERROR,
                "The %s file, '%s', does not have the spatial "
                "variable '%s' while the %s counterpart, '%s', does.",
                (!indexHasVar) ? (char *) "index" : (char *) "input",
                (!indexHasVar) ? indexFileName : testFileName,
                spatialNames[index],
                (!indexHasVar) ? (char *) "input" : (char *) "index",
                (!indexHasVar) ? testFileName : indexFileName
            );
        }
        if (LogInfo->stopRun) {
            return;
        }
    }

    /* Identical CRS if present this includes testing for
       grid_mapping_name, semi_major_axis, inverse_flattening,
       longitude_of_central_meridian, latitude_of_projection_origin,
       false_easting, false_northing, and standard_parallel (if they exist);
       The for-loops that are contained within this conditional loop through
       the index file information (0) and the input file information (1),
       the specific type of information is dependent on the loop instance */
    if (indexCRSExists && testCRSExists) {
        for (index = 0; index < numDimsAndVars; index++) {
            SW_NC_get_var_identifier(
                fileIDs[index], CRSNames[index], varIDs[index], LogInfo
            );
            if (LogInfo->stopRun) {
                return;
            }
        }

        for (att = 0; att < numCrsAtts; att++) {
            for (index = 0; index < numDimsAndVars; index++) {
                att_exists(
                    fileIDs[index],
                    *varIDs[index],
                    crsAttNames[att],
                    attSizes[index],
                    attExists[index],
                    LogInfo
                );
                if (LogInfo->stopRun) {
                    return;
                }
            }

            if (indexAttExists && testAttExists) {
                if (nc_inq_atttype(
                        testFileID, testVarID, crsAttNames[att], &attType
                    ) != NC_NOERR) {
                    LogError(
                        LogInfo,
                        LOGERROR,
                        "Could not get type of attribute '%s' under the "
                        "variable '%s'.",
                        crsAttNames[att],
                        testCRSName
                    );
                    return;
                }

                if (attType == NC_CHAR || attType == NC_STRING) {
                    for (index = 0; index < numDimsAndVars; index++) {
                        SW_NC_get_str_att_val(
                            fileIDs[index],
                            CRSNames[index],
                            crsAttNames[att],
                            crsAttVals[index],
                            LogInfo
                        );
                        if (LogInfo->stopRun) {
                            return;
                        }
                    }

                    if (strcmp(indexCRSAtt, testCRSAtt) != 0) {
                        LogError(
                            LogInfo,
                            LOGERROR,
                            "The attribute '%s' under the CRS variables '%s' "
                            "and '%s' do not match.",
                            crsAttNames[att],
                            indexCRSName,
                            testCRSName
                        );
                        return;
                    }
                } else if (attType == NC_DOUBLE) {
                    /* Compare the values of attributes that are of type
                       NC_DOUBLE, "standard_parallel" may have one or
                       two values, others should have one */
                    for (index = 0; index < numDimsAndVars; index++) {
                        get_att_double(
                            fileIDs[index],
                            *varIDs[index],
                            crsAttNames[att],
                            doubleVals[index],
                            LogInfo
                        );
                        if (LogInfo->stopRun) {
                            return;
                        }
                    }

                    if (indexAttSize == testAttSize) {
                        if (!EQ(indexDoubleVals[0], testDoubleVals[0]) ||
                            (indexAttSize == 2 &&
                             !EQ(indexDoubleVals[1], testDoubleVals[1]))) {

                            LogError(
                                LogInfo,
                                LOGERROR,
                                "The value(s) for the attribute '%s' do not "
                                "match between the input file and index "
                                "file.",
                                indexCRSName,
                                testCRSName
                            );
                            return;
                        }
                    }
                }
            }
        }
    }
}

/* =================================================== */
/*             Global Function Definitions             */
/* --------------------------------------------------- */

/**
@brief Mark a site/gridcell as completed (success/fail) in the progress file

@param[in] isFailure Did simulation run fail or succeed?
@param[in] domType Type of domain in which simulations are running
    (gridcell/sites)
@param[in] progFileID Identifier of the progress netCDF file
@param[in] progVarID Identifier of the progress variable within the progress
    netCDF
@param[in] ncSUID Current simulation unit identifier for which is used
    to get data from netCDF
@param[in,out] LogInfo Holds information dealing with logfile output
*/
void SW_NCIN_set_progress(
    Bool isFailure,
    const char *domType,
    int progFileID,
    int progVarID,
    unsigned long ncSUID[],
    LOG_INFO *LogInfo
) {

    const signed char mark = (isFailure) ? PRGRSS_FAIL : PRGRSS_DONE;
    size_t count1D[] = {1};
    size_t count2D[] = {1, 1};
    size_t *count = (strcmp(domType, "s") == 0) ? count1D : count2D;

    SW_NC_write_vals(
        &progVarID,
        progFileID,
        NULL,
        (void *) &mark,
        ncSUID,
        count,
        "byte",
        LogInfo
    );
    nc_sync(progFileID);
}

/**
@brief Create a progress netCDF file

@param[in,out] SW_Domain Struct of type SW_DOMAIN holding constant
    temporal/spatial information for a set of simulation runs
@param[in,out] LogInfo Holds information dealing with logfile output
*/
void SW_NCIN_create_progress(SW_DOMAIN *SW_Domain, LOG_INFO *LogInfo) {

    SW_NETCDF_IN *SW_netCDFIn = &SW_Domain->netCDFInput;
    SW_PATH_INPUTS *SW_PathInputs = &SW_Domain->SW_PathInputs;
    Bool primCRSIsGeo =
        SW_Domain->OutDom.netCDFOutput.primary_crs_is_geographic;
    char **inDomFileNames = SW_PathInputs->ncInFiles[eSW_InDomain];

    /* Get latitude/longitude names that were read-in from input file */
    char *readinGeoYName = (primCRSIsGeo) ?
                               SW_Domain->OutDom.netCDFOutput.geo_YAxisName :
                               SW_Domain->OutDom.netCDFOutput.proj_YAxisName;
    char *readinGeoXName = (primCRSIsGeo) ?
                               SW_Domain->OutDom.netCDFOutput.geo_XAxisName :
                               SW_Domain->OutDom.netCDFOutput.proj_XAxisName;
    char *siteName = SW_Domain->OutDom.netCDFOutput.siteName;

    Bool domTypeIsS = (Bool) (strcmp(SW_Domain->DomainType, "s") == 0);
    const char *projGridMap = "crs_projsc: %s %s crs_geogsc: %s %s";
    const char *geoGridMap = "crs_geogsc";
    const char *sCoord = "%s %s %s";
    const char *xyCoord = "%s %s";
    const char *coord = domTypeIsS ? sCoord : xyCoord;
    const char *grid_map = primCRSIsGeo ? geoGridMap : projGridMap;
    char coordStr[MAX_FILENAMESIZE] = "\0";
    char gridMapStr[MAX_FILENAMESIZE] = "\0";
    const char *attNames[] = {
        "long_name", "units", "grid_mapping", "coordinates"
    };
    const char *attVals[] = {"simulation progress", "1", grid_map, coord};
    const int numAtts = 4;
    int numValsToWrite;
    const signed char fillVal = NC_FILL_BYTE;
    const signed char flagVals[] = {PRGRSS_FAIL, PRGRSS_READY, PRGRSS_DONE};
    const char *flagMeanings =
        "simulation_error ready_to_simulate simulation_complete";
    const char *progVarName =
        SW_netCDFIn->inVarInfo[eSW_InDomain][vNCprog][INNCVARNAME];
    const char *freq = "fx";

    int *progFileID = &SW_PathInputs->ncDomFileIDs[vNCprog];
    const char *domFileName = inDomFileNames[vNCdom];
    const char *progFileName = inDomFileNames[vNCprog];
    int *progVarID = &SW_netCDFIn->ncDomVarIDs[vNCprog];

    // SW_NC_create_full_var/SW_NC_create_template
    // No time variable/dimension
    double startTime = 0;

    Bool progFileIsiteDom = (Bool) (strcmp(progFileName, domFileName) == 0);
    Bool progFileExists = FileExists(progFileName);
    Bool progVarExists = SW_NC_varExists(*progFileID, progVarName);
    Bool createOrModFile =
        (Bool) (!progFileExists || (progFileIsiteDom && !progVarExists));

    /* Fill dynamic coordinate names */
    if (domTypeIsS) {
        snprintf(
            coordStr,
            MAX_FILENAMESIZE,
            coord,
            readinGeoYName,
            readinGeoXName,
            siteName
        );
    } else {
        snprintf(
            coordStr, MAX_FILENAMESIZE, coord, readinGeoYName, readinGeoXName
        );
    }
    attVals[numAtts - 1] = coordStr;

    if (!primCRSIsGeo) {
        snprintf(
            gridMapStr,
            MAX_FILENAMESIZE,
            grid_map,
            SW_Domain->OutDom.netCDFOutput.proj_XAxisName,
            SW_Domain->OutDom.netCDFOutput.proj_YAxisName,
            readinGeoYName,
            readinGeoXName
        );

        attVals[numAtts - 2] = gridMapStr;
    }

    /*
      If the progress file is not to be created or modified, check it

      See if the progress variable exists within it's file, also handling
      the case where the progress variable is in the domain netCDF

      In addition to making sure the file exists, make sure the progress
      variable is present
    */
    if (!createOrModFile) {
        SW_NC_check(SW_Domain, *progFileID, progFileName, LogInfo);
    } else {

#if defined(SOILWAT)
        if (LogInfo->printProgressMsg) {
            sw_message("is creating a progress tracker ...");
        }
#endif

        // No need for various information when creating the progress netCDF
        // like start year, start time, base calendar year, layer depths
        // and period
        if (progFileExists) {
            nc_redef(*progFileID);
        } else {
            SW_NC_create_template(
                SW_Domain->DomainType,
                domFileName,
                progFileName,
                progFileID,
                swFALSE,
                freq,
                LogInfo
            );
            if (LogInfo->stopRun) {
                return;
            }
        }
        SW_NC_create_full_var(
            progFileID,
            SW_Domain->DomainType,
            NC_BYTE,
            0,
            0,
            0,
            progVarName,
            attNames,
            attVals,
            numAtts,
            swFALSE,
            NULL,
            &startTime,
            0,
            0,
            0,
            SW_Domain->OutDom.netCDFOutput.deflateLevel,
            readinGeoYName,
            readinGeoXName,
            SW_Domain->OutDom.netCDFOutput.siteName,
            -1,
            LogInfo
        );
        if (LogInfo->stopRun) {
            return; // Exit function prematurely due to error
        }

        SW_NC_get_var_identifier(*progFileID, progVarName, progVarID, LogInfo);
        if (LogInfo->stopRun) {
            return; // Exit function prematurely due to error
        }

        // If the progress existed before this function was called,
        // do not set the new attributes
        if (!progVarExists) {
            // Add attribute "_FillValue" to the progress variable
            numValsToWrite = 1;
            SW_NC_write_att(
                "_FillValue",
                (void *) &fillVal,
                *progVarID,
                *progFileID,
                numValsToWrite,
                NC_BYTE,
                LogInfo
            );
            if (LogInfo->stopRun) {
                return; // Exit function prematurely due to error
            }

            // Add attributes "flag_values" and "flag_meanings"
            numValsToWrite = 3;
            SW_NC_write_att(
                "flag_values",
                (void *) flagVals,
                *progVarID,
                *progFileID,
                numValsToWrite,
                NC_BYTE,
                LogInfo
            );
            if (LogInfo->stopRun) {
                return; // Exit function prematurely due to error
            }

            SW_NC_write_string_att(
                "flag_meanings", flagMeanings, *progVarID, *progFileID, LogInfo
            );
            if (LogInfo->stopRun) {
                return; // Exit function prematurely due to error
            }
        }


        nc_enddef(*progFileID);

        fill_prog_netCDF_vals(SW_Domain, LogInfo);
    }
}

/** Identify soil profile information across simulation domain from netCDF

    nMaxEvapLayers is set to nMaxSoilLayers.

    @param[out] hasConsistentSoilLayerDepths Flag indicating if all simulation
        run within domain have identical soil layer depths
        (though potentially variable number of soil layers)
    @param[out] nMaxSoilLayers Largest number of soil layers across
        simulation domain
    @param[out] nMaxEvapLayers Largest number of soil layers from which
        bare-soil evaporation may extract water across simulation domain
    @param[out] depthsAllSoilLayers Lower soil layer depths [cm] if
        consistent across simulation domain
    @param[in] default_n_layers Default number of soil layer
    @param[in] default_n_evap_lyrs Default number of soil layer used for
        bare-soil evaporation
    @param[in] default_depths Default values of soil layer depths [cm]
    @param[out] LogInfo Holds information on warnings and errors
*/
void SW_NCIN_soilProfile(
    Bool *hasConsistentSoilLayerDepths,
    LyrIndex *nMaxSoilLayers,
    LyrIndex *nMaxEvapLayers,
    double depthsAllSoilLayers[],
    LyrIndex default_n_layers,
    LyrIndex default_n_evap_lyrs,
    double default_depths[],
    LOG_INFO *LogInfo
) {
    // TO IMPLEMENT:
    // if (has soils as nc-input) then
    //     investigate these soil nc-inputs and determine
    //     *nMaxSoilLayers = ...;
    if (*nMaxSoilLayers > MAX_LAYERS) {
        LogError(
            LogInfo,
            LOGERROR,
            "Domain-wide maximum number of soil layers (%d) "
            "is larger than allowed (MAX_LAYERS = %d).\n",
            *nMaxSoilLayers,
            MAX_LAYERS
        );
        return; // Exit function prematurely due to error
    }
    //     *hasConsistentSoilLayerDepths = ...;
    //     if (*hasConsistentSoilLayerDepths) depthsAllSoilLayers[k] = ...;
    // else

    *hasConsistentSoilLayerDepths = swTRUE;
    *nMaxSoilLayers = default_n_layers;
    (void) default_n_evap_lyrs;

    memcpy(
        depthsAllSoilLayers,
        default_depths,
        sizeof(default_depths[0]) * default_n_layers
    );

    // endif

    // Use total number of soil layers for bare-soil evaporation output
    *nMaxEvapLayers = *nMaxSoilLayers;
}

/**
@brief Create domain netCDF template if it does not already exists

@param[in] SW_Domain Struct of type SW_DOMAIN holding constant
    temporal/spatial information for a set of simulation runs
@param[in] fileName File name of the domain template netCDF file;
    if NULL, then use the default domain template file name.
@param[out] LogInfo Holds information on warnings and errors
*/
void SW_NCIN_create_domain_template(
    SW_DOMAIN *SW_Domain, char *fileName, LOG_INFO *LogInfo
) {

    /* Get latitude/longitude names that were read-in from input file */
    char *readinGeoYName = SW_Domain->OutDom.netCDFOutput.geo_YAxisName;
    char *readinGeoXName = SW_Domain->OutDom.netCDFOutput.geo_XAxisName;

    int *domFileID = &SW_Domain->SW_PathInputs.ncDomFileIDs[vNCdom];
    char *domVarName =
        SW_Domain->netCDFInput.inVarInfo[eSW_InDomain][vNCdom][INNCVARNAME];
    int sDimID = 0;
    int YDimID = 0;
    int XDimID = 0;
    int domDims[2]; // Either [YDimID, XDimID] or [sDimID, 0]
    int nDomainDims;
    int domVarID = 0;
    int YVarID = 0;
    int XVarID = 0;
    int sVarID = 0;
    int YBndsID = 0;
    int XBndsID = 0;

    if (isnull(fileName)) {
        fileName = (char *) DOMAIN_TEMP;
    }

    if (FileExists(fileName)) {
        LogError(
            LogInfo,
            LOGERROR,
            "Could not create new domain template. "
            "This is due to the fact that it already "
            "exists. Please modify it and change the name."
        );
    } else if (!SW_Domain->OutDom.netCDFOutput.primary_crs_is_geographic &&
               is_wgs84(SW_Domain->crs_bbox)) {

        LogError(
            LogInfo,
            LOGERROR,
            "Projected CRS with a geographical bounding box detected."
        );
    }
    if(LogInfo->stopRun) {
        return;
    }

#if defined(SOILWAT)
    if (LogInfo->printProgressMsg) {
        sw_message("is creating a domain template ...");
    }
#endif

    if (nc_create(fileName, NC_NETCDF4, domFileID) != NC_NOERR) {
        LogError(
            LogInfo,
            LOGERROR,
            "Could not create new domain template due "
            "to something internal."
        );
        return; // Exit prematurely due to error
    }

    if (strcmp(SW_Domain->DomainType, "s") == 0) {
        nDomainDims = 1;

        // Create s dimension/domain variables
        fill_domain_netCDF_s(
            SW_Domain,
            domFileID,
            &sDimID,
            &sVarID,
            &YVarID,
            &XVarID,
            SW_Domain->OutDom.netCDFOutput.deflateLevel,
            LogInfo
        );

        if (LogInfo->stopRun) {
            nc_close(*domFileID);
            return; // Exit prematurely due to error
        }

        domDims[0] = sDimID;
        domDims[1] = 0;
    } else {
        nDomainDims = 2;

        fill_domain_netCDF_gridded(
            SW_Domain,
            domFileID,
            &YDimID,
            &XDimID,
            &YVarID,
            &XVarID,
            &YBndsID,
            &XBndsID,
            SW_Domain->OutDom.netCDFOutput.deflateLevel,
            LogInfo
        );

        if (LogInfo->stopRun) {
            nc_close(*domFileID);
            return; // Exit prematurely due to error
        }

        domDims[0] = YDimID;
        domDims[1] = XDimID;
    }

    // Create domain variable
    fill_domain_netCDF_domain(
        domVarName,
        &domVarID,
        domDims,
        readinGeoYName,
        readinGeoXName,
        SW_Domain->OutDom.netCDFOutput.proj_YAxisName,
        SW_Domain->OutDom.netCDFOutput.proj_XAxisName,
        SW_Domain->OutDom.netCDFOutput.siteName,
        *domFileID,
        nDomainDims,
        SW_Domain->OutDom.netCDFOutput.primary_crs_is_geographic,
        SW_Domain->DomainType,
        SW_Domain->OutDom.netCDFOutput.deflateLevel,
        LogInfo
    );
    if (LogInfo->stopRun) {
        nc_close(*domFileID);
        return; // Exit function prematurely due to error
    }

    fill_netCDF_with_invariants(
        &SW_Domain->OutDom.netCDFOutput,
        SW_Domain->DomainType,
        domFileID,
        swTRUE,
        LogInfo
    );
    if (LogInfo->stopRun) {
        nc_close(*domFileID);
        return; // Exit function prematurely due to error
    }

    nc_enddef(*domFileID);

    fill_domain_netCDF_vals(
        SW_Domain,
        *domFileID,
        domVarID,
        sVarID,
        YVarID,
        XVarID,
        YBndsID,
        XBndsID,
        LogInfo
    );

    nc_close(*domFileID);
}

/**
@brief Check if a site/grid cell is marked to be run in the progress netCDF

@param[in] progFileID Identifier of the progress netCDF file
@param[in] progVarID Identifier of the progress variable
@param[in] ncSUID Current simulation unit identifier for which is used
    to get data from netCDF
@param[in,out] LogInfo Holds information dealing with logfile output
*/
Bool SW_NCIN_check_progress(
    int progFileID, int progVarID, unsigned long ncSUID[], LOG_INFO *LogInfo
) {

    signed char progVal = 0;

    SW_NC_get_single_val(
        progFileID, &progVarID, NULL, ncSUID, (void *) &progVal, "byte", LogInfo
    );

    return (Bool) (!LogInfo->stopRun && progVal == PRGRSS_READY);
}

/**
@brief Read values from netCDF input files for available variables and copy
to SW_Run

@param[in,out] sw Comprehensive struct of type SW_RUN containing
    all information in the simulation
@param[in] SW_Domain Struct of type SW_DOMAIN holding constant
    temporal/spatial information for a set of simulation runs
@param[in] ncSUID Current simulation unit identifier for which is used
    to get data from netCDF
@param[out] LogInfo Holds information on warnings and errors
*/
void SW_NCIN_read_inputs(
    SW_RUN *sw, SW_DOMAIN *SW_Domain, size_t ncSUID[], LOG_INFO *LogInfo
) {

    int file;
    int varNum;
    Bool domTypeS =
        (Bool) (Str_CompareI(SW_Domain->DomainType, (char *) "s") == 0);
    Bool primCRSIsGeo =
        SW_Domain->OutDom.netCDFOutput.primary_crs_is_geographic;

    /* Get latitude/longitude names that were read-in from input file */
    char *readinGeoYName = (primCRSIsGeo) ?
                               SW_Domain->OutDom.netCDFOutput.geo_YAxisName :
                               SW_Domain->OutDom.netCDFOutput.proj_YAxisName;
    char *readinGeoXName = (primCRSIsGeo) ?
                               SW_Domain->OutDom.netCDFOutput.geo_XAxisName :
                               SW_Domain->OutDom.netCDFOutput.proj_XAxisName;

    const int numInFilesNC = 1;
    const int numDomVals = 2;
    const int numVals[] = {numDomVals};
    const int ncFileIDs[] = {SW_Domain->SW_PathInputs.ncDomFileIDs[vNCdom]};
    const char *varNames[][2] = {{readinGeoYName, readinGeoXName}};
    int ncIndex;
    int varID;

    double *values[][2] = {{&sw->Model.latitude, &sw->Model.longitude}};

    /*
        Gather all values being requested within the array "values"
        The index within the netCDF file for domain type "s" is simply the
        first index in "ncSUID"
        For the domain type "xy", the index of the variable "y" is the first
        in "ncSUID" and the index of the variable "x" is the second in "ncSUID"
    */
    for (file = 0; file < numInFilesNC; file++) {
        for (varNum = 0; varNum < numVals[file]; varNum++) {
            varID = -1;
            ncIndex = (domTypeS) ? 0 : varNum % 2;

            SW_NC_get_single_val(
                ncFileIDs[file],
                &varID,
                varNames[file][varNum],
                &ncSUID[ncIndex],
                (void *) values[file][varNum],
                "double",
                LogInfo
            );
            if (LogInfo->stopRun) {
                return; // Exit function prematurely due to error
            }
        }
    }


    /* Convert units */
    /* Convert latitude/longitude to radians */
    sw->Model.latitude *= deg_to_rad;
    sw->Model.longitude *= deg_to_rad;
}

/**
@brief Check that all available netCDF input files are consistent with domain

@param[in] SW_Domain Struct of type SW_DOMAIN holding constant
    temporal/spatial information for a set of simulation runs
@param[out] LogInfo Holds information on warnings and errors
*/
void SW_NCIN_check_input_files(SW_DOMAIN *SW_Domain, LOG_INFO *LogInfo) {
    int inKey;
    int file;
    int indexFileID = -1;
    int inFileID = -1;
    int *fileID;
    int numSpatialVars;
    int *fileIDs[] = {&indexFileID, &inFileID};
    const int numFiles = 2;

    char *spatialNames[4];
    char **fileNames;
    char **varInfo;
    char **indexVarInfo;
    Bool **readInVars = SW_Domain->netCDFInput.readInVars;
    Bool *useIndexFile = SW_Domain->netCDFInput.useIndexFile;
    Bool fileIsIndex;
    Bool primCRSIsGeo;

    /* Check the domain files */
    for (file = 0; file < SW_NVARDOM; file++) {
        SW_NC_check(
            SW_Domain,
            SW_Domain->SW_PathInputs.ncDomFileIDs[file],
            SW_Domain->SW_PathInputs.ncInFiles[eSW_InDomain][file],
            LogInfo
        );
        if (LogInfo->stopRun) {
            return; // Exit function prematurely due to inconsistent data
        }
    }

    /* Check actual input files provided by the user */
    ForEachNCInKey(inKey) {
        if (readInVars[inKey][0]) {
            indexVarInfo = SW_Domain->netCDFInput.inVarInfo[inKey][0];
            fileNames = SW_Domain->SW_PathInputs.ncInFiles[inKey];

            for (file = 0; file < numVarsInKey[inKey]; file++) {
                if (readInVars[inKey][file + 1] &&
                    (file > 0 || (file == 0 && useIndexFile[inKey]))) {

                    fileIsIndex = (Bool) (file == 0);
                    fileID = (fileIsIndex) ? &indexFileID : &inFileID;
                    varInfo = SW_Domain->netCDFInput.inVarInfo[inKey][file];
                    primCRSIsGeo =
                        (Bool) (strcmp(
                                    varInfo[INGRIDMAPPING], "latitude_longitude"
                                ) == 0);

                    SW_NC_open(fileNames[file], NC_NOWRITE, fileID, LogInfo);
                    if (LogInfo->stopRun) {
                        return;
                    }

                    /* Get spatial coordinate names to make sure they're
                        identical between the index and input files including
                        auxilary coordinate variables */
                    if (primCRSIsGeo) {
                        spatialNames[0] = varInfo[INYAXIS];
                        spatialNames[1] = varInfo[INXAXIS];
                        numSpatialVars = 2;
                    } else {
                        spatialNames[0] =
                            SW_Domain->OutDom.netCDFOutput.geo_YAxisName;
                        spatialNames[1] =
                            SW_Domain->OutDom.netCDFOutput.geo_XAxisName;
                        spatialNames[2] = varInfo[INYAXIS];
                        spatialNames[3] = varInfo[INXAXIS];
                        numSpatialVars = 4;
                    }

                    /* Check the current input file either against the
                       domain (current file is an index file or the input
                       file's domain matches the programs domain exactly),
                       otherwise, compare the input file against the
                       provided index file */
                    if (fileIsIndex || !useIndexFile[inKey]) {
                        SW_NC_check(
                            SW_Domain, *fileID, fileNames[file], LogInfo
                        );
                    } else {
                        check_input_file_against_index(
                            indexFileID,
                            inFileID,
                            spatialNames,
                            numSpatialVars,
                            fileNames[0],
                            fileNames[file],
                            indexVarInfo[INCRSNAME],
                            varInfo[INCRSNAME],
                            LogInfo
                        );
                    }
                    if (LogInfo->stopRun) {
                        goto closeFile;
                    }

                    if (file > 0) {
                        free_tempcoords_close_files(NULL, fileIDs, 0, numFiles);
                    }
                }
            }

            nc_close(indexFileID);
            indexFileID = -1;
        }
    }

closeFile:
    free_tempcoords_close_files(NULL, fileIDs, 0, numFiles);
}

/**
@brief Open netCDF file(s) that contain(s) domain and progress variables

These files are kept open during simulations
    * to read geographic coordinates from the domain
    * to identify and update progress

@param[in,out] SW_netCDFIn Constant netCDF input file information
@param[in,out] SW_PathInputs Struct of type SW_PATH_INPUTS which
holds basic information about input files and values
@param[out] LogInfo Holds information on warnings and errors
*/
void SW_NCIN_open_dom_prog_files(
    SW_NETCDF_IN *SW_netCDFIn, SW_PATH_INPUTS *SW_PathInputs, LOG_INFO *LogInfo
) {

    char **inDomFileNames = SW_PathInputs->ncInFiles[eSW_InDomain];
    int *ncDomFileIDs = SW_PathInputs->ncDomFileIDs;

    int fileNum;
    int openType = NC_WRITE;
    int *fileID;
    char ***inDomVarInfo = SW_netCDFIn->inVarInfo[eSW_InDomain];
    char *fileName;
    char *domFile = inDomFileNames[vNCdom];
    char *progFile = inDomFileNames[vNCprog];
    char *varName;
    Bool progFileDomain = (Bool) (strcmp(domFile, progFile) == 0);

    // Open the domain/progress netCDF
    for (fileNum = vNCdom; fileNum <= vNCprog; fileNum++) {
        fileName = inDomFileNames[fileNum];
        fileID = &ncDomFileIDs[fileNum];
        varName = inDomVarInfo[fileNum][INNCVARNAME];

        if (FileExists(fileName)) {
            SW_NC_open(fileName, openType, fileID, LogInfo);
            if (LogInfo->stopRun) {
                return;
            }

            /*
              Get the ID for the domain variable and the progress variable if
              it is not in the domain netCDF or it exists in the domain netCDF
            */
            if (fileNum == vNCdom || !progFileDomain ||
                SW_NC_varExists(*fileID, varName)) {

                SW_NC_get_var_identifier(
                    *fileID,
                    varName,
                    &SW_netCDFIn->ncDomVarIDs[fileNum],
                    LogInfo
                );
                if (LogInfo->stopRun) {
                    return; // Exit function prematurely due to error
                }
            }
        }
    }

    // If the progress variable is contained in the domain netCDF, then
    // close the (redundant) progress file identifier
    // and use instead the (equivalent) domain file identifier
    if (progFileDomain) {
        nc_close(ncDomFileIDs[vNCprog]);
        ncDomFileIDs[vNCprog] = ncDomFileIDs[vNCdom];
    }
}

/**
@brief Close all netCDF files that have been opened while the program ran

@param[in,out] ncDomFileIDs List of all nc domain file IDs
*/
void SW_NCIN_close_files(int ncDomFileIDs[]) {
    int fileNum;

    for (fileNum = 0; fileNum < SW_NVARDOM; fileNum++) {
        nc_close(ncDomFileIDs[fileNum]);
    }
}

/**
@brief Initializes pointers only pertaining to netCDF input information

@param[in,out] SW_netCDFIn Constant netCDF input file information
*/
void SW_NCIN_init_ptrs(SW_NETCDF_IN *SW_netCDFIn) {
    int k;

    ForEachNCInKey(k) {
        SW_netCDFIn->inVarInfo[k] = NULL;
        SW_netCDFIn->units_sw[k] = NULL;
        SW_netCDFIn->uconv[k] = NULL;
        SW_netCDFIn->readInVars[k] = NULL;
    }

    SW_netCDFIn->weathCalOverride = NULL;
    SW_netCDFIn->domXCoordsGeo = NULL;
    SW_netCDFIn->domYCoordsGeo = NULL;
    SW_netCDFIn->domXCoordsProj = NULL;
    SW_netCDFIn->domYCoordsProj = NULL;
}

void SW_NCIN_deconstruct(SW_NETCDF_IN *SW_netCDFIn) {

    int k;
    int freeIndex;
    const int numFreeVals = 4;
    double **freeArray[] = {
        &SW_netCDFIn->domXCoordsGeo,
        &SW_netCDFIn->domYCoordsGeo,
        &SW_netCDFIn->domYCoordsProj,
        &SW_netCDFIn->domYCoordsProj
    };

    for (freeIndex = 0; freeIndex < numFreeVals; freeIndex++) {
        if (!isnull(*freeArray[freeIndex])) {
            free(*freeArray[freeIndex]);
            *freeArray[freeIndex] = NULL;
        }
    }

    ForEachNCInKey(k) { SW_NCIN_dealloc_inputkey_var_info(SW_netCDFIn, k); }
}

/**
@brief Deconstruct netCDF input variable information

@param[in,out] SW_netCDFIn Constant netCDF input file information
@param[in] key Category of input variables that is being deconstructed
*/
void SW_NCIN_dealloc_inputkey_var_info(SW_NETCDF_IN *SW_netCDFIn, int key) {
    int varNum;
    int varsInKey;
    int attNum;
    char *attStr;

    varsInKey = numVarsInKey[key];

    if (key == eSW_InWeather) {
        if (!isnull(SW_netCDFIn->weathCalOverride)) {
            for (varNum = 0; varNum < numVarsInKey[eSW_InWeather]; varNum++) {
                if (!isnull(SW_netCDFIn->weathCalOverride[varNum])) {
                    free((void *) SW_netCDFIn->weathCalOverride[varNum]);
                    SW_netCDFIn->weathCalOverride[varNum] = NULL;
                }
            }

            free((void *) SW_netCDFIn->weathCalOverride);
            SW_netCDFIn->weathCalOverride = NULL;
        }
    }

    if (!isnull(SW_netCDFIn->inVarInfo[key])) {
        for (varNum = 0; varNum < varsInKey; varNum++) {
            if (!isnull(SW_netCDFIn->inVarInfo[key][varNum])) {
                for (attNum = 0; attNum < NUM_INPUT_INFO; attNum++) {
                    attStr = SW_netCDFIn->inVarInfo[key][varNum][attNum];

                    if (!isnull(attStr)) {
                        free(
                            (void *) SW_netCDFIn->inVarInfo[key][varNum][attNum]
                        );
                        SW_netCDFIn->inVarInfo[key][varNum][attNum] = NULL;
                    }
                }

                free((void *) SW_netCDFIn->inVarInfo[key][varNum]);
                SW_netCDFIn->inVarInfo[key][varNum] = NULL;
            }
        }

        free((void *) SW_netCDFIn->inVarInfo[key]);
        SW_netCDFIn->inVarInfo[key] = NULL;
    }

    if (!isnull(SW_netCDFIn->units_sw[key])) {
        for (varNum = 0; varNum < varsInKey; varNum++) {
            if (!isnull((void *) SW_netCDFIn->units_sw[key][varNum])) {
                free((void *) SW_netCDFIn->units_sw[key][varNum]);
                SW_netCDFIn->units_sw[key][varNum] = NULL;
            }
        }

        free((void *) SW_netCDFIn->units_sw[key]);
        SW_netCDFIn->units_sw[key] = NULL;
    }

    if (!isnull(SW_netCDFIn->uconv[key])) {
        for (int varNum = 0; varNum < varsInKey; varNum++) {
            if (!isnull(SW_netCDFIn->uconv[key][varNum])) {
#if defined(SWNETCDF) && defined(SWUDUNITS)
                cv_free(SW_netCDFIn->uconv[key][varNum]);
#else
                free((void *) SW_netCDFIn->uconv[key][varNum]);
#endif
                SW_netCDFIn->uconv[key][varNum] = NULL;
            }
        }

        free((void *) SW_netCDFIn->uconv[key]);
        SW_netCDFIn->uconv[key] = NULL;
    }

    if (!isnull((void *) SW_netCDFIn->readInVars[key])) {
        free((void *) SW_netCDFIn->readInVars[key]);
        SW_netCDFIn->readInVars[key] = NULL;
    }
}

/**
@brief Deep copy a source instance of SW_NETCDF_IN into a destination instance

@param[in] source_input Source input netCDF information to copy
@param[out] dest_input Destination input netCDF information to be copied
into from it's source counterpart
@param[out] LogInfo Holds information on warnings and errors
*/
void SW_NCIN_deepCopy(
    SW_NETCDF_IN *source_input, SW_NETCDF_IN *dest_input, LOG_INFO *LogInfo
) {
    int k;
    int numVars;
    int varNum;
    int atNum;

    memcpy(dest_input, source_input, sizeof(*dest_input));

    ForEachNCInKey(k) {
        SW_NCIN_alloc_inputkey_var_info(dest_input, k, LogInfo);
        if (LogInfo->stopRun) {
            return; /* Exit function prematurely due to error */
        }
        if (!isnull(source_input->inVarInfo[k])) {
            numVars = numVarsInKey[k];

            dest_input->readInVars[k][0] = source_input->readInVars[k][0];

            for (varNum = 0; varNum < numVars; varNum++) {
                dest_input->readInVars[k][varNum + 1] =
                    source_input->readInVars[k][varNum + 1];

                if (!isnull(source_input->inVarInfo[k][varNum])) {
                    for (atNum = 0; atNum < NUM_INPUT_INFO; atNum++) {

                        if (!isnull(source_input->inVarInfo[k][varNum][atNum]
                            )) {

                            dest_input->inVarInfo[k][varNum][atNum] = Str_Dup(
                                source_input->inVarInfo[k][varNum][atNum],
                                LogInfo
                            );
                            if (LogInfo->stopRun) {
                                return; /* Exit function prematurely due
                                            to error */
                            }
                        }
                    }
                }

                if (!isnull(source_input->units_sw[k][varNum])) {
                    dest_input->units_sw[k][varNum] =
                        Str_Dup(source_input->units_sw[k][varNum], LogInfo);
                    if (LogInfo->stopRun) {
                        return; // Exit function prematurely due
                                // to error
                    }
                }
            }
        }
    }
}

/*
@brief Read input netCDF variables that the user will provide

@param[in,out] SW_netCDFIn Constant netCDF input file information
@param[in] SW_netCDFOut Constant netCDF output file information
@param[in] SW_PathInputs Struct of type SW_PATH_INPUTS which
holds basic information about input files and values
@param[in] startYr Start year of the simulation
@param[in] endYr End year of the simulation
@param[out] LogInfo Holds information on warnings and errors
*/
void SW_NCIN_read_input_vars(
    SW_NETCDF_IN *SW_netCDFIn,
    SW_NETCDF_OUT *SW_netCDFOut,
    SW_PATH_INPUTS *SW_PathInputs,
    TimeInt startYr,
    TimeInt endYr,
    LOG_INFO *LogInfo
) {

    FILE *f;
    int lineno = 0;
    char *MyFileName;
    int scanRes;
    int infoIndex;
    int copyInfoIndex = 0;
    int index;
    int strInfoInd;
    char *tempPtr;
    char inbuf[LARGE_VALUE] = {'\0'};
    char input[NIN_VAR_INPUTS][MAX_ATTVAL_SIZE] = {"\0"};
    const char *readLineFormat =
        "%255[^\t]\t%255[^\t]\t%255[^\t]\t%255[^\t]\t%255[^\t]\t%255[^\t]\t"
        "%255[^\t]\t%255[^\t]\t%255[^\t]\t%255[^\t]\t%255[^\t]\t%255[^\t]\t"
        "%255[^\t]\t%255[^\t]\t%255[^\t]\t%255[^\t]\t%255[^\t]\t%255[^\t]\t"
        "%255[^\t]\t%255[^\t]\t%255[^\t]\t%255[^\t]";

    /* Locally handle the weather stride information where -2 is
       the default value so we can tell that the value hasn't been set yet

       0: number of years within a stride
       1: stride start year */
    int inWeathStrideInfo[2] = {-2, -2};
    int tempStrideInfo[2];

    const int keyInd = 0;
    const int SWVarNameInd = 1;
    const int SWUnitInd = 2;
    const int doInputInd = 3;
    const int ncFileNameInd = 4;
    const int ncVarNameInd = 5;
    const int ncVarUnitsInd = 6;
    const int ncDomTypeInd = 7;
    const int ncSiteNameInd = 8;
    const int ncCRSNameInd = 9;
    const int ncGridMapInd = 10;
    const int ncXAxisInd = 11;
    const int ncYAxisInd = 12;
    const int ncZAxisInd = 13;
    const int ncTAxisInd = 14;
    const int ncStYrInd = 15;
    const int ncStStartInd = 16;
    const int ncStPatInd = 17;
    const int ncCalendarInd = 18;
    const int ncVAxisInd = 19;
    const int userComInd = 20;

    int inKey = -1;
    int inVarNum = -1;
    int doInput = 0;
    int maxVarIter = 1;
    int varIter;
    const int allVegInc = 5;

    Bool copyInfo = swFALSE;
    Bool isIndexFile = swFALSE;
    Bool isAllVegVar = swFALSE;

    /* Acceptable non-numerical values for stride year and stride start,
       respectively */
    const char *accStrVal[] = {"Inf", "NA"};
    char **varInfoPtr = NULL;

    MyFileName = SW_PathInputs->txtInFiles[eNCIn];
    f = OpenFile(MyFileName, "r", LogInfo);
    if (LogInfo->stopRun) {
        return; /* Exit function prematurely due to error */
    }

    SW_NCIN_alloc_input_var_info(SW_netCDFIn, LogInfo);
    if (LogInfo->stopRun) {
        return; /* Exit function prematurely due to error */
    }

    while (GetALine(f, inbuf, MAX_FILENAMESIZE)) {
        scanRes = sscanf(
            inbuf,
            readLineFormat,
            input[keyInd],
            input[SWVarNameInd],
            input[SWUnitInd],
            input[doInputInd],
            input[ncFileNameInd],
            input[ncVarNameInd],
            input[ncVarUnitsInd],
            input[ncDomTypeInd],
            input[ncSiteNameInd],
            input[ncCRSNameInd],
            input[ncGridMapInd],
            input[ncXAxisInd],
            input[ncYAxisInd],
            input[ncZAxisInd],
            input[ncTAxisInd],
            input[ncStYrInd],
            input[ncStStartInd],
            input[ncStPatInd],
            input[ncCalendarInd],
            input[ncVAxisInd],
            input[userComInd]
        );

        if (scanRes != NIN_VAR_INPUTS) {
            LogError(
                LogInfo,
                LOGERROR,
                "%s [row %d]: %d instead of %d columns found. "
                "Enter 'NA' if value should not have anything.",
                MyFileName,
                lineno + 1,
                scanRes,
                NIN_VAR_INPUTS
            );
            goto closeFile; /* Exit function prematurely due to error */
        }

        if (lineno == 0) {
            for (index = keyInd; index <= userComInd; index++) {
                tempPtr = (char *) expectedColNames[index];
                if (strcmp(input[index], tempPtr) != 0) {
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

        doInput = sw_strtoi(input[doInputInd], MyFileName, LogInfo);
        if (LogInfo->stopRun) {
            goto closeFile;
        }

        if (doInput) {
            get_2d_input_key(
                input[keyInd],
                input[SWVarNameInd],
                &inKey,
                &inVarNum,
                &isIndexFile,
                &isAllVegVar
            );

            if (isIndexFile) {
                if (inVarNum == KEY_NOT_FOUND) {
                    LogError(
                        LogInfo,
                        LOGWARN,
                        "Could not find a match for the index name '%s'."
                        "This will be skipped.",
                        input[SWVarNameInd]
                    );
                    continue;
                }
            } else if (inKey == eSW_NoInKey || inVarNum == KEY_NOT_FOUND) {
                LogError(
                    LogInfo,
                    LOGWARN,
                    "Could not determine what the variable '%s' is "
                    "within the key '%s', this will be skipped.",
                    input[SWVarNameInd],
                    input[keyInd]
                );
                continue;
            }

            if (isAllVegVar) {
                maxVarIter = NVEGTYPES;

                if (inKey == eSW_InVeg) {
                    inVarNum = (inVarNum == 0) ? 2 : inVarNum + 2;
                } else {
                    inVarNum = 12; /* Start variable at `Trees.trans_coeff` */
                }
            } else {
                maxVarIter = 1;
            }

            /* Copy various information including the weather/input
               variable information;
               If the current variable is a general one
               (see `generalVegNames`), loop through NVEGTYPES to copy
               the same information for the same variables across
               all vegetation types;
               if a variable is already seen, it's skipped */
            for (varIter = 0; varIter < maxVarIter; varIter++) {
                copyInfoIndex = 0;

                /* Ignore this entry if this input has been seen already */
                if (SW_netCDFIn->readInVars[inKey][inVarNum + 1]) {
                    LogError(
                        LogInfo,
                        LOGERROR,
                        "The SW2 input variable '%s' has more than one entry.",
                        input[SWVarNameInd]
                    );
                    goto closeFile;
                }

                /* Check to see if the key hasn't been input before and
                if correct, allocate file space for the key */
                if (!SW_netCDFIn->readInVars[inKey][0] &&
                    isnull(SW_PathInputs->ncInFiles[inKey])) {

                    SW_NCIN_alloc_file_information(
                        numVarsInKey[inKey],
                        inKey,
                        &SW_PathInputs->ncInFiles[inKey],
                        &SW_PathInputs->ncWeatherInFiles,
                        LogInfo
                    );
                    if (LogInfo->stopRun) {
                        /* Exit function prematurely due to error */
                        goto closeFile;
                    }
                }

                /* Handle weather-only information */
                if (inKey == eSW_InWeather && !isIndexFile) {
                    strInfoInd = SW_INSTRIDEYR;

                    /* Copy stride number of years and stride start year*/
                    for (index = ncStYrInd; index <= ncStStartInd; index++) {
                        if (strcmp(input[index], accStrVal[strInfoInd]) == 0) {
                            tempStrideInfo[strInfoInd] = -1;
                        } else {
                            tempStrideInfo[strInfoInd] =
                                sw_strtoi(input[index], MyFileName, LogInfo);

                            if (index == ncStYrInd &&
                                tempStrideInfo[strInfoInd] <= 0) {
                                LogError(
                                    LogInfo,
                                    LOGERROR,
                                    "Start year of weather input file for '%s'"
                                    " is <= 0.",
                                    input[ncVarNameInd]
                                );
                                goto closeFile;
                            }
                        }
                        if (LogInfo->stopRun) {
                            /* Exit function prematurely due to error */
                            goto closeFile;
                        }

                        strInfoInd++;
                    }

                    /* If the stride informatoin has never been set before,
                       set it; once it's set, check to make sure that the
                       succeeding weather input variables have the same
                       stride start/number of years */
                    if (inWeathStrideInfo[0] == -2) {
                        inWeathStrideInfo[0] = tempStrideInfo[0];
                        inWeathStrideInfo[1] = tempStrideInfo[1];
                    } else {
                        if (inWeathStrideInfo[0] != tempStrideInfo[0] ||
                            inWeathStrideInfo[1] != tempStrideInfo[1]) {

                            LogError(
                                LogInfo,
                                LOGERROR,
                                "Weather variable '%s' does not have the same "
                                "stride start year and/or length as the "
                                "other weather variable(s).",
                                input[ncVarNameInd]
                            );

                            /* Exit function prematurely due to error */
                            goto closeFile;
                        }
                    }

                    SW_netCDFIn->weathCalOverride[inVarNum] =
                        Str_Dup(input[ncCalendarInd], LogInfo);
                    if (LogInfo->stopRun) {
                        goto closeFile; /* Exit function prematurely due to
                                           error */
                    }
                }

                SW_netCDFIn->readInVars[inKey][inVarNum + 1] = swTRUE;

                /* Shortcut the flags in the first index to reduce checking
                   in the future */
                if (!isIndexFile) {
                    SW_netCDFIn->readInVars[inKey][0] = swTRUE;
                }

                /* Save file to input file list */
                SW_PathInputs->ncInFiles[inKey][inVarNum] =
                    Str_Dup(input[ncFileNameInd], LogInfo);
                if (LogInfo->stopRun) {
                    goto closeFile; /* Exit function prematurely due to error */
                }

                /* Copy variable unit for SW2 */
                if (strcmp(input[SWUnitInd], "NA") != 0 &&
                    strcmp(input[SWUnitInd], swInVarUnits[inKey][inVarNum]) !=
                        0) {

                    LogError(
                        LogInfo,
                        LOGWARN,
                        "Input column, 'SW2 units', value does not match "
                        "the units of SW2. The units '%s' will be "
                        "used instead of '%s' for the nc variable '%s'.",
                        swInVarUnits[inKey][inVarNum],
                        input[SWUnitInd],
                        input[ncVarNameInd]
                    );
                }
                SW_netCDFIn->units_sw[inKey][inVarNum] =
                    Str_Dup(swInVarUnits[inKey][inVarNum], LogInfo);
                if (LogInfo->stopRun) {
                    goto closeFile; /* Exit function prematurely due to error */
                }

                /* Copy other useful information that is not soley for
                weather or specifying if the weather variable is to be input
                */
                varInfoPtr = SW_netCDFIn->inVarInfo[inKey][inVarNum];
                for (infoIndex = SWUnitInd; infoIndex < userComInd;
                     infoIndex++) {
                    copyInfo = (Bool) (infoIndex != doInputInd &&
                                       infoIndex != ncStYrInd &&
                                       infoIndex != ncStStartInd &&
                                       infoIndex != ncCalendarInd &&
                                       infoIndex != ncFileNameInd);

                    if (copyInfo) {
                        if (infoIndex == ncVarNameInd) {
                            varInfoPtr[copyInfoIndex] =
                                Str_Dup(possVarNames[inKey][inVarNum], LogInfo);
                        } else {
                            varInfoPtr[copyInfoIndex] =
                                Str_Dup(input[infoIndex], LogInfo);
                        }

                        if (LogInfo->stopRun) {
                            goto closeFile; /* Exit function prematurely due to
                                               error */
                        }

                        copyInfoIndex++;
                    }
                }

                if (inKey == eSW_InSoil && !isIndexFile) {
                    if (strcmp(input[ncZAxisInd], "NA") == 0) {
                        LogError(
                            LogInfo,
                            LOGERROR,
                            "Soil variable '%s' does not have a name for the "
                            "Z-axis where one is required.",
                            input[SWVarNameInd]
                        );
                        goto closeFile;
                    }
                }

                /* Increment variable number based on:
                   1) If the current key is eSW_InSoil and/or the variable
                        is meant for all four vegetation types: 1
                   2) If the current key (i.e., eSW_InVeg) and use used for the
                        four vegetation types: 4
                   3) Otherwise, the increment does not matter since we do not
                        deal with all four variable types (which covers
                        multiple variables in the code) */
                inVarNum +=
                    (inKey == eSW_InSoil || !isAllVegVar) ? 1 : allVegInc;
            }

            if (isAllVegVar) {
                if (strcmp(input[ncVAxisInd], "NA") == 0) {
                    LogError(
                        LogInfo,
                        LOGERROR,
                        "The variable '%s' within the input key '%s' "
                        "has a value of 'NA' for the column 'ncVAxisName'.",
                        input[SWVarNameInd],
                        possInKeys[inKey]
                    );
                    goto closeFile; /* Exit function prematurely due to error */
                }
            } else if (inKey == eSW_InVeg || inKey == eSW_InSoil) {
                if (strcmp(input[ncVAxisInd], "NA") != 0) {
                    LogError(
                        LogInfo,
                        LOGERROR,
                        "The variable '%s' within the input key '%s' "
                        "has an unexpected value that is not 'NA' for "
                        "the column 'ncVAxisName'.",
                        input[SWVarNameInd],
                        possInKeys[inKey]
                    );
                    goto closeFile; /* Exit function prematurely due to error */
                }
            }
        }

        lineno++;
    }

    check_input_variables(
        SW_netCDFOut,
        SW_netCDFIn->inVarInfo,
        inWeathStrideInfo,
        SW_netCDFIn->readInVars,
        LogInfo
    );
    if (LogInfo->stopRun) {
        goto closeFile;
    }

    if (SW_netCDFIn->readInVars[eSW_InWeather][0]) {
        generate_weather_filenames(
            SW_PathInputs->ncInFiles[eSW_InWeather],
            inWeathStrideInfo,
            SW_netCDFIn->inVarInfo[eSW_InWeather],
            startYr,
            endYr,
            &SW_PathInputs->ncWeatherInFiles,
            &SW_PathInputs->ncWeatherInStartEndYrs,
            &SW_PathInputs->ncNumWeatherInFiles,
            SW_netCDFIn->readInVars[eSW_InWeather],
            LogInfo
        );
    }

closeFile: { CloseFile(&f, LogInfo); }
}

/**
@brief Wrapper function to allocate input request variables
and input variable information

@param[out] SW_netCDFIn Constant netCDF input file information
@param[out] LogInfo Holds information on warnings and errors
*/
void SW_NCIN_alloc_input_var_info(
    SW_NETCDF_IN *SW_netCDFIn, LOG_INFO *LogInfo
) {
    int key;

    ForEachNCInKey(key) {
        SW_NCIN_alloc_inputkey_var_info(SW_netCDFIn, key, LogInfo);
        if (LogInfo->stopRun) {
            return; // Exit function prematurely due to error
        }
    }
}

/*
@brief Allocate/initialize input information based on an input category

@param[out] SW_netCDFIn Constant netCDF input file information
@param[in] key Category of input information that is being initialized
@param[out] LogInfo Holds information on warnings and errors
*/
void SW_NCIN_alloc_inputkey_var_info(
    SW_NETCDF_IN *SW_netCDFIn, int key, LOG_INFO *LogInfo
) {

    if (key == eSW_InWeather) {
        alloc_overrideCalendars(
            &SW_netCDFIn->weathCalOverride, numVarsInKey[key], LogInfo
        );
        if (LogInfo->stopRun) {
            return; // Exit function prematurely due to error
        }
    }

    SW_NC_alloc_req(
        &SW_netCDFIn->readInVars[key], numVarsInKey[key] + 1, LogInfo
    );
    if (LogInfo->stopRun) {
        return; /* Exit function prematurely due to error */
    }

    SW_NC_alloc_vars(
        &SW_netCDFIn->inVarInfo[key], numVarsInKey[key], NUM_INPUT_INFO, LogInfo
    );
    if (LogInfo->stopRun) {
        return; /* Exit function prematurely due to error */
    }

    SW_NC_alloc_unitssw(
        &SW_netCDFIn->units_sw[key], numVarsInKey[key], LogInfo
    );
    if (LogInfo->stopRun) {
        return; /* Exit function prematurely due to error */
    }

    SW_NC_alloc_uconv(&SW_netCDFIn->uconv[key], numVarsInKey[key], LogInfo);
    if (LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }
}

/*
@brief Allocate/initialize all input file information

@param[in] numInVars Maximum number of variables within the given
input key
@param[in] key Category of input information that is being initialized
@param[out] inputFiles List of user-provided input files
@param[out] ncWeatherInFiles List of all weather input files
@param[out] LogInfo Holds information on warnings and errors
*/
void SW_NCIN_alloc_file_information(
    int numInVars,
    int key,
    char ***inputFiles,
    char ****ncWeatherInFiles,
    LOG_INFO *LogInfo
) {

    int varNum;

    /* Allocate/intiialize input and initialize index files */
    *inputFiles = (char **) Mem_Malloc(
        sizeof(char *) * numInVars, "alloc_input_files()", LogInfo
    );
    if (LogInfo->stopRun) {
        return; /* Exit function prematurely due to error */
    }

    for (varNum = 0; varNum < numInVars; varNum++) {
        (*inputFiles)[varNum] = NULL;
    }

    if (key == eSW_InWeather) {
        alloc_weath_input_files(ncWeatherInFiles, numInVars, LogInfo);
    }
}

/**
Create unit converters for input variables

This function requires previous calls to
    - SW_NCIN_alloc_output_var_info() to initialize
      SW_NETCDF_IN.uconv[varIndex] to NULL
    - SW_NCIN_read_input_vars() to obtain user requested input units

@param[in,out] SW_netCDFIn Constant netCDF input file information
@param[out] LogInfo Holds information on warnings and errors
*/
void SW_NCIN_create_units_converters(
    SW_NETCDF_IN *SW_netCDFIn, LOG_INFO *LogInfo
) {
    int varIndex;
    int key;

#if defined(SWUDUNITS)
    ut_system *system;
    ut_unit *unitFrom;
    ut_unit *unitTo;

    /* silence udunits2 error messages */
    ut_set_error_message_handler(ut_ignore);

    /* Load unit system database */
    system = ut_read_xml(NULL);
#endif

    ForEachNCInKey(key) {
        if (!SW_netCDFIn->readInVars[key][0]) {
            continue;
        }

        for (varIndex = 0; varIndex < numVarsInKey[key]; varIndex++) {
            if (!SW_netCDFIn->readInVars[key][varIndex + 1]) {
                continue; // Skip variable iteration
            }

#if defined(SWUDUNITS)
            if (!isnull(SW_netCDFIn->units_sw[key][varIndex])) {
                unitFrom = ut_parse(
                    system, SW_netCDFIn->units_sw[key][varIndex], UT_UTF8
                );
                unitTo = ut_parse(
                    system,
                    SW_netCDFIn->inVarInfo[key][varIndex][INVARUNITS],
                    UT_UTF8
                );

                if (ut_are_convertible(unitFrom, unitTo)) {
                    // SW_netCDFIn.uconv[key][varIndex] was previously
                    // to NULL initialized
                    SW_netCDFIn->uconv[key][varIndex] =
                        ut_get_converter(unitFrom, unitTo);
                }

                if (isnull(SW_netCDFIn->uconv[key][varIndex])) {
                    // ut_are_convertible() is false or ut_get_converter()
                    // failed
                    LogError(
                        LogInfo,
                        LOGWARN,
                        "Units of variable '%s' cannot get converted from "
                        "internal '%s' to requested '%s'. "
                        "Input will use internal units.",
                        SW_netCDFIn->inVarInfo[key][varIndex][INNCVARNAME],
                        SW_netCDFIn->units_sw[key][varIndex],
                        SW_netCDFIn->inVarInfo[key][varIndex][INVARUNITS]
                    );

                    /* converter is not available: output in internal units */
                    free(SW_netCDFIn->inVarInfo[key][varIndex][INVARUNITS]);
                    SW_netCDFIn->inVarInfo[key][varIndex][INVARUNITS] =
                        Str_Dup(SW_netCDFIn->units_sw[key][varIndex], LogInfo);
                }

                ut_free(unitFrom);
                ut_free(unitTo);
            }

#else
            /* udunits2 is not available: output in internal units */
            free(SW_netCDFIn->inVarInfo[key][varIndex][INVARUNITS]);
            if (!isnull(SW_netCDFIn->units_sw[key][varIndex])) {
                SW_netCDFIn->inVarInfo[key][varIndex][INVARUNITS] =
                    Str_Dup(SW_netCDFIn->units_sw[key][varIndex], LogInfo);
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
@brief Allocate input file information for individual weather
variables

@param[out] outWeathFileNames List of all weather input files for a variable
@param[out] ncWeatherInStartEndYrs Start/end years of each weather input file
@param[in] numWeathIn Number of input weather files
@param[in] weathVar Weather variable index
@param[out] LogInfo Holds information on warnings and errors
*/
void SW_NCIN_alloc_weath_input_info(
    char ****outWeathFileNames,
    unsigned int ***ncWeatherInStartEndYrs,
    unsigned int numWeathIn,
    int weathVar,
    LOG_INFO *LogInfo
) {

    unsigned int inFileNum;

    (*outWeathFileNames)[weathVar] = (char **) Mem_Malloc(
        sizeof(char *) * numWeathIn, "SW_NCIN_alloc_weath_input_info()", LogInfo
    );
    if (LogInfo->stopRun) {
        return; /* Exit function prematurely due to error */
    }

    for (inFileNum = 0; inFileNum < numWeathIn; inFileNum++) {
        (*outWeathFileNames)[weathVar][inFileNum] = NULL;
    }

    if (isnull(*ncWeatherInStartEndYrs)) {
        (*ncWeatherInStartEndYrs) = (unsigned int **) Mem_Malloc(
            sizeof(unsigned int *) * numWeathIn,
            "SW_NCIN_alloc_weath_input_info()",
            LogInfo
        );
        if (LogInfo->stopRun) {
            return; /* Exit function prematurely due to error */
        }

        for (inFileNum = 0; inFileNum < numWeathIn; inFileNum++) {
            (*ncWeatherInStartEndYrs)[inFileNum] = NULL;
        }

        for (inFileNum = 0; inFileNum < numWeathIn; inFileNum++) {
            (*ncWeatherInStartEndYrs)[inFileNum] = (unsigned int *) Mem_Malloc(
                sizeof(unsigned int) * 2,
                "SW_NCIN_alloc_weath_input_info()",
                LogInfo
            );
        }

        for (inFileNum = 0; inFileNum < numWeathIn; inFileNum++) {
            (*ncWeatherInStartEndYrs)[inFileNum][0] = 0;
            (*ncWeatherInStartEndYrs)[inFileNum][1] = 0;
        }
    }
}

/**
@brief Calculate information that will be used many times throughout
simulation runs so they do not need to be calculated again, more specifically,
storing the domain coordinates, if each input file key should use an
index file, and temporal indices for weather inputs

@param[in] SW_Domain Struct of type SW_DOMAIN holding constant
temporal/spatial information for a set of simulation runs
@param[out] LogInfo Holds information on warnings and errors
*/
void SW_NCIN_precalc_lookups(SW_DOMAIN *SW_Domain, LOG_INFO *LogInfo) {

    SW_NETCDF_IN *SW_netCDFIn = &SW_Domain->netCDFInput;

    int domFileID = SW_Domain->SW_PathInputs.ncDomFileIDs[vNCdom];
    Bool primCRSIsGeo =
        SW_Domain->OutDom.netCDFOutput.primary_crs_is_geographic;
    char *domCoordVarNamesNonSite[4] = {
        SW_Domain->OutDom.netCDFOutput.geo_YAxisName,
        SW_Domain->OutDom.netCDFOutput.geo_XAxisName,
        SW_Domain->OutDom.netCDFOutput.proj_YAxisName,
        SW_Domain->OutDom.netCDFOutput.proj_XAxisName
    };

    read_domain_coordinates(
        SW_netCDFIn,
        domCoordVarNamesNonSite,
        SW_Domain->OutDom.netCDFOutput.siteName,
        domFileID,
        SW_Domain->DomainType,
        primCRSIsGeo,
        LogInfo
    );
    if (LogInfo->stopRun) {
        return; /* Exit prematurely due to error */
    }

    determine_indexfile_use(
        SW_netCDFIn, &SW_Domain->SW_PathInputs, SW_Domain->spatialTol, LogInfo
    );
    if (LogInfo->stopRun) {
        return; /* Exit function prematurely due to error */
    }

    /* Precalculate temperature temporal nc indices */
    if (SW_netCDFIn->readInVars[eSW_InWeather][0]) {
#if defined(SWUDUNITS)
        calc_temporal_weather_indices(
            SW_netCDFIn,
            &SW_Domain->SW_PathInputs,
            SW_Domain->startyr,
            SW_Domain->endyr,
            LogInfo
        );
#else
        LogError(
            LogInfo,
            LOGERROR,
            "SWUDUNITS is not enabled, so we cannot calculate temporal "
            "information."
        );
#endif
    }
}

/**
@brief Create index files as an interface between the domain that
the program uses and the domain provided in input netCDFs

@param[in] SW_Domain Struct of type SW_DOMAIN holding constant
temporal/spatial information for a set of simulation runs
@param[out] LogInfo Holds information on warnings and errors
*/
void SW_NCIN_create_indices(SW_DOMAIN *SW_Domain, LOG_INFO *LogInfo) {
    SW_NETCDF_IN *SW_netCDFIn = &SW_Domain->netCDFInput;

    int k;
    int varIDs[2];
    char *indexName = NULL;
    int dimIDs[2][2] = {{0}}; /* Up to two dims for two variables */
    Bool inHasSite = swFALSE;
    Bool siteDom = (Bool) (strcmp(SW_Domain->DomainType, "s") == 0);
    char *siteName = SW_Domain->OutDom.netCDFOutput.siteName;
    char *domYName = SW_Domain->OutDom.netCDFOutput.geo_YAxisName;
    char *domXName = SW_Domain->OutDom.netCDFOutput.geo_XAxisName;
    char *indexVarNames[2] = {NULL};
    const int numAtts = 4;
    Bool inPrimCRSIsGeo;

    size_t ySize = 0UL;
    size_t xSize = 0UL;
    size_t *dimSizes[] = {&ySize, &xSize};
    size_t domYSize;
    size_t domXSize;

    int fIndex;
    int templateID = -1;
    int ncFileID = -1;
    char *frequency = (char *) "fx";
    const char *domFile =
        SW_Domain->SW_PathInputs.ncInFiles[eSW_InDomain][vNCdom];
    char *fileName;
    int indexVarNDims = 0;
    int numVarsToWrite = 0;
    Bool has2DCoordVars;

    char *yxVarNames[2];
    double *inputYVals = NULL;
    double *inputXVals = NULL;
    double *useDomYVals;
    double *useDomXVals;

    double **freeArr[] = {&inputYVals, &inputXVals};
    int *fileIDs[] = {&templateID, &ncFileID};
    const int numFree = 2;

    char ***varInfo = NULL;

#if defined(SOILWAT)
    if (LogInfo->printProgressMsg) {
        sw_message("is creating any necessary index files ...");
    }
#endif

    ForEachNCInKey(k) {
        if (SW_netCDFIn->readInVars[k][0] && SW_netCDFIn->useIndexFile[k]) {
            fIndex = 1;
            indexVarNames[0] = indexVarNames[1] = NULL;
            has2DCoordVars = swFALSE;
            varIDs[0] = varIDs[1] = -1;

            indexName = SW_Domain->SW_PathInputs.ncInFiles[k][0];

            if (!FileExists(indexName)) {
                /* Find the first input file within the key */
                while (!SW_netCDFIn->readInVars[k][fIndex + 1]) {
                    fIndex++;
                }

                varInfo = SW_netCDFIn->inVarInfo[k];

                yxVarNames[0] = varInfo[fIndex][INYAXIS];
                yxVarNames[1] = varInfo[fIndex][INXAXIS];

                if (k == eSW_InWeather) {
                    fileName =
                        SW_Domain->SW_PathInputs.ncWeatherInFiles[fIndex][0];
                } else {
                    fileName = SW_Domain->SW_PathInputs.ncInFiles[k][fIndex];
                }

                inPrimCRSIsGeo = (Bool) (strcmp(
                                             varInfo[fIndex][INGRIDMAPPING],
                                             "latitude_longitude"
                                         ) == 0);

                if (inPrimCRSIsGeo) {
                    useDomYVals = SW_netCDFIn->domYCoordsGeo;
                    useDomXVals = SW_netCDFIn->domXCoordsGeo;
                    domYSize = SW_netCDFIn->domYCoordGeoSize;
                    domXSize = SW_netCDFIn->domXCoordGeoSize;
                } else {
                    useDomYVals = SW_netCDFIn->domYCoordsProj;
                    useDomXVals = SW_netCDFIn->domXCoordsProj;
                    domYSize = SW_netCDFIn->domYCoordProjSize;
                    domXSize = SW_netCDFIn->domXCoordProjSize;
                }

                SW_NC_open(fileName, NC_NOWRITE, &ncFileID, LogInfo);
                if (LogInfo->stopRun) {
                    return;
                }

                SW_NC_create_template(
                    SW_Domain->DomainType,
                    domFile,
                    indexName,
                    &templateID,
                    swTRUE,
                    frequency,
                    LogInfo
                );
                if (LogInfo->stopRun) {
                    return; /* Exit function prematurely due to error */
                }

                inHasSite =
                    (Bool) (strcmp(varInfo[fIndex][INDOMTYPE], "s") == 0);

                get_index_vars_info(
                    siteDom,
                    ncFileID,
                    &indexVarNDims,
                    templateID,
                    domYName,
                    domXName,
                    dimIDs,
                    inHasSite,
                    varInfo[fIndex][INSITENAME],
                    indexVarNames,
                    &numVarsToWrite,
                    LogInfo
                );
                if (LogInfo->stopRun) {
                    return;
                }

                create_index_vars(
                    varIDs,
                    numVarsToWrite,
                    indexVarNames,
                    dimIDs,
                    templateID,
                    indexVarNDims,
                    SW_Domain->OutDom.netCDFOutput.deflateLevel,
                    inHasSite,
                    siteDom,
                    numAtts,
                    k,
                    indexName,
                    domYName,
                    domXName,
                    siteName,
                    LogInfo
                );
                if (LogInfo->stopRun) {
                    goto freeMem;
                }

                get_input_coordinates(
                    SW_netCDFIn,
                    &ncFileID,
                    NULL,
                    dimSizes,
                    &has2DCoordVars,
                    k,
                    SW_Domain->spatialTol,
                    &inputYVals,
                    &inputXVals,
                    yxVarNames,
                    swFALSE,
                    inPrimCRSIsGeo,
                    LogInfo
                );
                if (LogInfo->stopRun) {
                    goto freeMem;
                }

                nc_close(ncFileID);
                ncFileID = -1;

                /* Create and query the KD-tree then write to the index file */
                nc_enddef(templateID);

                write_indices(
                    useDomYVals,
                    useDomXVals,
                    domYSize,
                    domXSize,
                    inputYVals,
                    inputXVals,
                    (Bool) !inHasSite,
                    siteDom,
                    inPrimCRSIsGeo,
                    varIDs,
                    templateID,
                    indexVarNames,
                    indexName,
                    dimSizes,
                    has2DCoordVars,
                    SW_Domain->spatialTol,
                    LogInfo
                );
                if (LogInfo->stopRun) {
                    goto freeMem;
                }

                nc_close(templateID);
                templateID = -1;

                free_tempcoords_close_files(freeArr, fileIDs, numFree, numFree);
            }
        }
    }

freeMem:
    free_tempcoords_close_files(freeArr, fileIDs, numFree, numFree);
}
