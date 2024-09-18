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
static void check_spatial_information(
    SW_NETCDF_OUT *SW_netCDFOut, char **inputInfo, LOG_INFO *LogInfo
) {

    Bool primCRSIsGeo = SW_netCDFOut->primary_crs_is_geographic;
    char *ncCRSGridMapName = inputInfo[INGRIDMAPPING];
    char *ncYAxisName = inputInfo[INYAXIS];
    char *ncXAxisName = inputInfo[INXAXIS];
    char *geoGridMapName = SW_netCDFOut->crs_geogsc.grid_mapping_name;
    char *projGridMapName = SW_netCDFOut->crs_projsc.grid_mapping_name;
    char *geoYAxisName = SW_netCDFOut->geo_YAxisName;
    char *geoXAxisName = SW_netCDFOut->geo_XAxisName;
    char *projYAxisName = SW_netCDFOut->proj_YAxisName;
    char *projXAxisName = SW_netCDFOut->proj_XAxisName;

    Bool failedGeoTest =
        (Bool) (primCRSIsGeo &&
                (strcmp(ncCRSGridMapName, "latitude_longitude") != 0 ||
                 strcmp(ncCRSGridMapName, geoGridMapName) != 0 ||
                 strcmp(ncYAxisName, geoYAxisName) != 0 ||
                 strcmp(ncXAxisName, geoXAxisName) != 0));

    Bool failedProjTest =
        (Bool) (!failedGeoTest && !primCRSIsGeo &&
                (strcmp(ncCRSGridMapName, "latitude_longitude") == 0 ||
                 strcmp(ncCRSGridMapName, projGridMapName) != 0 ||
                 strcmp(ncYAxisName, projYAxisName) != 0 ||
                 strcmp(ncXAxisName, projXAxisName) != 0));

    if (failedGeoTest || failedProjTest) {
        LogError(
            LogInfo,
            LOGERROR,
            "The geographical spatial information provided for "
            "'ncCRSGridMappingName', 'ncXAxisName', and 'ncYAxisName' "
            "do not match expected values for a %s domain.",
            (failedGeoTest) ? "geographical" : "projected"
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
                        ignoreAtt = (Bool) (varNum > 1 && attNum == attEnd);
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

            check_spatial_information(
                SW_netCDFOut, inputInfo[key][testVarIndex], LogInfo
            );
            if (LogInfo->stopRun) {
                return; /* Exit function prematurely due to failed test */
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
            readinGeoXName,
            readinGeoYName,
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
@param[in] primCRSIsGeo Specifies if the domain CRS is geographical
@param[in] yGeoName Read-in geographical y-axis name
@param[in] xGeoName Read-in geographical x-axis name
@param[in] yProjName Read-in projected y-axis name
@param[in] xProjName Read-in projected x-axis name
@param[in] numVars Number of variables to create
@param[out] LogInfo Holds information on warnings and errors
*/
static void create_bnd_names(
    char bndNames[][MAX_FILENAMESIZE],
    Bool primCRSIsGeo,
    const char *yGeoName,
    const char *xGeoName,
    const char *yProjName,
    const char *xProjName,
    int numVars,
    LOG_INFO *LogInfo
) {

    int scanRes = 0;
    int varNum;

    char *bndLocs[] = {
        &bndNames[0][0], &bndNames[1][0], &bndNames[2][0], &bndNames[3][0]
    };

    const char *const writeBndsNames[] = {
        (primCRSIsGeo) ? yGeoName : yProjName,
        (primCRSIsGeo) ? xGeoName : xProjName,
        yGeoName,
        xGeoName
    };

    const char *const suffixes[] = {
        (primCRSIsGeo) ? "_bnds" : "",
        (primCRSIsGeo) ? "_bnds" : "",
        "_bnds",
        "_bnds"
    };

    for (varNum = 0; varNum < numVars; varNum++) {
        scanRes = snprintf(
            bndLocs[varNum],
            MAX_FILENAMESIZE,
            "%s%s",
            writeBndsNames[varNum],
            suffixes[varNum]
        );
        if (scanRes < 0) {
            if (scanRes < 0) {
                LogError(
                    LogInfo,
                    LOGERROR,
                    "A problem occurred when creating a 'bnds' variable."
                );
            }
            return; /* Exit function prematurely due to error */
        }
    }
}

/**
@brief Fill the domain netCDF file with variables that are for domain type "xy"

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
static void fill_domain_netCDF_xy(
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
    char bndVarNames[4][MAX_FILENAMESIZE] = {{'\0'}};
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
    int numLatAtt = 5;
    int numLonAtt = 5;
    int numYAtt = 4;
    int numXAtt = 4;
    int numAtts[] = {numLatAtt, numLonAtt, numYAtt, numXAtt};

    const int numDims = 3;
    const char *YDimName = readinGeoYName;
    const char *XDimName = readinGeoXName;

    const char *dimNames[] = {YDimName, XDimName, "bnds"};
    const unsigned long dimVals[] = {SW_Domain->nDimY, SW_Domain->nDimX, 2};
    int *dimIDs[] = {YDimID, XDimID, &bndsID};
    int dimIDIndex;

    int varIDs[4];
    int varBndIDs[4];

    create_bnd_names(
        bndVarNames,
        primCRSIsGeo,
        readinGeoYName,
        readinGeoXName,
        readinProjYName,
        readinProjXName,
        numVars,
        LogInfo
    );
    if (LogInfo->stopRun) {
        return; /* Exit function prematurely due to error */
    }

    // Create dimensions
    for (dimNum = 0; dimNum < numDims; dimNum++) {
        SW_NC_create_netCDF_dim(
            dimNames[dimNum],
            dimVals[dimNum],
            domFileID,
            dimIDs[dimNum],
            LogInfo
        );
        if (LogInfo->stopRun) {
            return; // Exit function prematurely due to error
        }
    }

    // Create variables - lat/lon and (if the primary CRS is
    // not geographic) y/x along with the corresponding *_bnds variable
    bndVarDims[1] = bndsID; // Set the second dimension of variable to bounds

    for (varNum = 0; varNum < numVars; varNum++) {
        dimIDIndex = varNum % 2; // 2 - get lat/lon dim ids only, not bounds

        SW_NC_create_netCDF_var(
            &varIDs[varNum],
            varNames[varNum],
            dimIDs[dimIDIndex],
            domFileID,
            NC_DOUBLE,
            1,
            NULL,
            deflateLevel,
            LogInfo
        );
        if (LogInfo->stopRun) {
            return; // Exit function prematurely due to error
        }

        // Set the first dimension of the variable
        bndVarDims[0] = *dimIDs[dimIDIndex];

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
    if (Str_CompareI(SW_Domain->crs_bbox, geo_long_name) == 0 ||
        (is_wgs84(SW_Domain->crs_bbox) && is_wgs84(geo_long_name))) {

        *YVarID = varIDs[0];     // lat
        *XVarID = varIDs[1];     // lon
        *YBndsID = varBndIDs[0]; // lat_bnds
        *XBndsID = varBndIDs[1]; // lon_bnds

    } else if (!primCRSIsGeo &&
               Str_CompareI(SW_Domain->crs_bbox, proj_long_name) == 0) {

        *YVarID = varIDs[2];     // y
        *XVarID = varIDs[3];     // x
        *YBndsID = varBndIDs[2]; // y_bnds
        *XBndsID = varBndIDs[3]; // x_bnds

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
@param[out] ncWeatherInStartEnd Specifies that start/end years of each input
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
    unsigned int ***ncWeatherInStartEnd,
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
            ncWeatherInStartEnd,
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

            (*ncWeatherInStartEnd)[0][0] = numStYr;
            (*ncWeatherInStartEnd)[0][1] = endYr;

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

            (*ncWeatherInStartEnd)[inFileNum][0] = beginFileYr;
            (*ncWeatherInStartEnd)[inFileNum][1] = endFileYr;

            beginFileYr += numStYr;
            endFileYr += numStYr;

            inFileNum++;
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
    char **inDomFileNames = SW_PathInputs->ncInFiles[eSW_InDomain];

    /* Get latitude/longitude names that were read-in from input file */
    char *readinGeoYName = SW_Domain->OutDom.netCDFOutput.geo_YAxisName;
    char *readinGeoXName = SW_Domain->OutDom.netCDFOutput.geo_XAxisName;
    char *siteName = SW_Domain->OutDom.netCDFOutput.siteName;

    Bool primCRSIsGeo =
        SW_Domain->OutDom.netCDFOutput.primary_crs_is_geographic;
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
        return; // Exit prematurely due to error
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

        fill_domain_netCDF_xy(
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
        SW_Domain->OutDom.netCDFOutput.geo_YAxisName,
        SW_Domain->OutDom.netCDFOutput.geo_XAxisName,
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

    /* Get latitude/longitude names that were read-in from input file */
    char *readinGeoYName = SW_Domain->OutDom.netCDFOutput.geo_YAxisName;
    char *readinGeoXName = SW_Domain->OutDom.netCDFOutput.geo_XAxisName;

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
    int file;

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
            if (nc_open(fileName, openType, fileID) != NC_NOERR) {
                LogError(
                    LogInfo,
                    LOGERROR,
                    "An error occurred when opening %s.",
                    fileName
                );
                return; // Exit function prematurely due to error
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
}

/**
@brief Deconstruct input netCDF information

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
@param[in] startYr End year of the simulation
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
                if (!SW_netCDFIn->readInVars[inKey][0]) {
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
                                       infoIndex != ncCalendarInd);

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
                }
                goto closeFile; /* Exit function prematurely due to error */
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
            &SW_PathInputs->ncWeatherInStartEnd,
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
@param[out] ncWeatherInStartEnd Start/end years of each weather input file
@param[in] numWeathIn Number of input weather files
@param[in] weathVar Weather variable index
@param[out] LogInfo Holds information on warnings and errors
*/
void SW_NCIN_alloc_weath_input_info(
    char ****outWeathFileNames,
    unsigned int ***ncWeatherInStartEnd,
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

    if (isnull(*ncWeatherInStartEnd)) {
        (*ncWeatherInStartEnd) = (unsigned int **) Mem_Malloc(
            sizeof(unsigned int *) * numWeathIn,
            "SW_NCIN_alloc_weath_input_info()",
            LogInfo
        );
        if (LogInfo->stopRun) {
            return; /* Exit function prematurely due to error */
        }

        for (inFileNum = 0; inFileNum < numWeathIn; inFileNum++) {
            (*ncWeatherInStartEnd)[inFileNum] = NULL;
        }

        for (inFileNum = 0; inFileNum < numWeathIn; inFileNum++) {
            (*ncWeatherInStartEnd)[inFileNum] = (unsigned int *) Mem_Malloc(
                sizeof(unsigned int) * 2,
                "SW_NCIN_alloc_weath_input_info()",
                LogInfo
            );
        }

        for (inFileNum = 0; inFileNum < numWeathIn; inFileNum++) {
            (*ncWeatherInStartEnd)[inFileNum][0] = 0;
            (*ncWeatherInStartEnd)[inFileNum][1] = 0;
        }
    }
}
