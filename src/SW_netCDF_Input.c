/* =================================================== */
/*                INCLUDES / DEFINES                   */
/* --------------------------------------------------- */
#include "include/SW_netCDF_Input.h"   // for SW_NCIN_soilProfile, ...
#include "include/filefuncs.h"         // for LogError, FileExists, sw_message
#include "include/generic.h"           // for Bool, LOGERROR, swFALSE, isnull
#include "include/myMemory.h"          // for Mem_Malloc, Str_Dup, sw_memcc...
#include "include/SW_datastructs.h"    // for LOG_INFO, eSW_InWeather, eSW_...
#include "include/SW_Defines.h"        // for MAX_FILENAMESIZE, NVEGTYPES
#include "include/SW_Domain.h"         // for SW_DOM_calc_ncSuid
#include "include/SW_Files.h"          // for eNCIn
#include "include/SW_netCDF_General.h" // for SW_NC_open, SW_NC_get_var_ide...
#include "include/SW_Site.h"           // for SW_SOIL_construct
#include "include/SW_Weather.h"        // for clear_hist_weather, SW_WTH_al...
#include "include/Times.h"             // for Time_get_lastdoy_y, timeStrin...
#include <float.h>                     // for DBL_MAX
#include <math.h>                      // for NAN, ceil, isnan
#include <netcdf.h>                    // for NC_NOERR, nc_close, NC_DOUBLE
#include <stdio.h>                     // for size_t, NULL, snprintf, sscanf
#include <stdlib.h>                    // for free, strtod
#include <string.h>                    // for strcmp, strlen, strstr, memcpy

#if defined(SWUDUNITS)
#include <udunits2.h> // for cv_free, cv_convert_double
#endif

#if defined(SWMPI)
#include "include/SW_MPI.h"
#endif


/* =================================================== */
/*                   Local Defines                     */
/* --------------------------------------------------- */

#define NIN_VAR_INPUTS 23

/* Maximum number of variables per input key */
#define SW_INNMAXVARS 22

/* Indices within `inWeathStrideInfo` for stride year and stride start */
#define SW_INSTRIDEYR 0
#define SW_INSTRIDESTART 1

/** The tsv nc-input file must contain the following column names */
static const char *const expectedColNames[] = {
    "SW2 input group",
    "SW2 variable",
    "SW2 units",
    "Do nc-input?",
    "ncFileName",
    "ncVarName",
    "ncVarUnits",
    "ncDomainType",
    "ncSiteName",
    "ncCRSName",
    "ncCRSGridMappingName",
    "ncXAxisName",
    "ncXDimName",
    "ncYAxisName",
    "ncYDimName",
    "ncZAxisName",
    "ncTAxisName",
    "ncStrideYears",
    "ncStrideStart",
    "ncStridePattern",
    "ncCalendarOverride",
    "ncVAxisName",
    "Comment"
};

/** Values of the column "SW2 units" of the tsv nc-input file */
static const char *const swInVarUnits[SW_NINKEYSNC][SW_INNMAXVARS] = {
    /* inDomain */
    {"1", "1"},
    /* inSpatial */
    {"1", "radian", "radian"},
    /* inTopo */
    {"1", "m", "radian", "radian"},
    /* inSoil */
    {"1",        "cm", "cm",   "g cm-3", "cm3 cm-3", "g g-1", "g g-1", "g g-1",
     "cm3 cm-3", "1",  "degC", "1",      "1",        "1",     "1",     "1",
     "NA",       "NA", "NA",   "NA",     "NA",       "NA"},
    /* inSite */
    {"1", "degC"},
    /*inVeg*/
    {"1",     "m2 m-2", "m2 m-2", "g m-2", "g m-2",  "1",     "g m-2", "m2 m-2",
     "g m-2", "g m-2",  "1",      "g m-2", "m2 m-2", "g m-2", "g m-2", "1",
     "g m-2", "m2 m-2", "g m-2",  "g m-2", "1",      "g m-2"},
    /* inWeather */
    {"1",
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
     "NA"},
    /* inClimate */
    {"1", "%", "m s-1", "%", "kg m-3", "1"}
};

/** Values of the column "SW2 variable" of the tsv nc-input file */
static const char *const possVarNames[SW_NINKEYSNC][SW_INNMAXVARS] = {
    /* inDomain */
    {"domain", "progress"},

    /* inSpatial */
    {"indexSpatial", "latitude", "longitude"},

    /* inTopo */
    {"indexSpatial", "elevation", "slope", "aspect"},

    /* inSoil */
    {"indexSpatial",
     "layerDepth",
     "layerWidth",
     "soilDensityInput",
     "fractionVolBulk_gravel",
     "fractionWeightMatric_sand",
     "fractionWeightMatric_clay",
     "fractionWeightMatric_silt",
     "fractionWeight_om",
     "impermeability",
     "avgLyrTempInit",
     "evap_coeff",

     "Trees.transp_coeff",
     "Shrubs.transp_coeff",
     "Forbs.transp_coeff",
     "Grasses.transp_coeff",

     "swrcpMineralSoil[1]",
     "swrcpMineralSoil[2]",
     "swrcpMineralSoil[3]",
     "swrcpMineralSoil[4]",
     "swrcpMineralSoil[5]",
     "swrcpMineralSoil[6]"},

    /* inSite */
    {"indexSpatial", "Tsoil_constant"},

    /* inVeg */
    {"indexSpatial",     "bareGround.fCover",

     "Trees.fCover",     "Trees.litter",      "Trees.biomass",
     "Trees.pct_live",   "Trees.lai_conv",

     "Shrubs.fCover",    "Shrubs.litter",     "Shrubs.biomass",
     "Shrubs.pct_live",  "Shrubs.lai_conv",

     "Forbs.fCover",     "Forbs.litter",      "Forbs.biomass",
     "Forbs.pct_live",   "Forbs.lai_conv",

     "Grasses.fCover",   "Grasses.litter",    "Grasses.biomass",
     "Grasses.pct_live", "Grasses.lai_conv"},

    /* inWeather */
    {"indexSpatial",
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
     "shortWaveRad"},

    /* inClimate */
    {"indexSpatial",
     "cloudcov",
     "windspeed",
     "r_humidity",
     "snow_density",
     "n_rain_per_day"}
};


/** @defgroup eiv Indices to netCDF input variables
The `eiv_*` provide the index to variable positions in possVarNames,
#SW_NETCDF_IN.inVarInfo and others, e.g.,
    `possVarNames[eSW_InDomain][eiv_domain]`.
However, we need to add one to correctly index #SW_NETCDF_IN.readInVars, e.g.,
    `readInVars[eSW_InDomain][eiv_domain + 1]`.
*  @{
*/
/** The index to the variable to the spatial index is 0 for each `inkey`
(exception is #eSW_InDomain which does not have an spatial index)
*/
static const int eiv_indexSpatial = 0;
/* inDomain (no indexSpatial) */
static const int eiv_domain = 0;
static const int eiv_progress = 1;
/* inSpatial */
static const int eiv_latitude = 1;
static const int eiv_longitude = 2;
/* inTopo */
// static const int eiv_elevation = 1;
// static const int eiv_slope = 2;
// static const int eiv_aspect = 3;
/* inSoil */
static const int eiv_soilLayerDepth = 1;
static const int eiv_soilLayerWidth = 2;
static const int eiv_soilDensity = 3;
static const int eiv_gravel = 4;
static const int eiv_sand = 5;
static const int eiv_clay = 6;
static const int eiv_silt = 7;
static const int eiv_som = 8;
static const int eiv_impermeability = 9;
static const int eiv_avgLyrTempInit = 10;
static const int eiv_evapCoeff = 11;
static const int eiv_transpCoeff[NVEGTYPES] = {12, 13, 14, 15};
static const int eiv_swrcpMS[SWRC_PARAM_NMAX] = {16, 17, 18, 19, 20, 21};
/* inVeg */
static const int eiv_bareGroundfCover = 1;
static const int eiv_vegfCover[NVEGTYPES] = {2, 7, 12, 17};
static const int eiv_vegLitter[NVEGTYPES] = {3, 8, 13, 18};
static const int eiv_vegBiomass[NVEGTYPES] = {4, 9, 14, 19};
static const int eiv_vegPctlive[NVEGTYPES] = {5, 10, 15, 20};
static const int eiv_vegLAIconv[NVEGTYPES] = {6, 11, 16, 21};
/* inWeather */
// static const int eiv_temp_max = 1 + TEMP_MAX;
// static const int eiv_temp_min = 1 + TEMP_MIN;
// static const int eiv_ppt = 1 + PPT;
// static const int eiv_cloudcov = 1 + CLOUD_COV;
// static const int eiv_windspeed = 1 + WIND_SPEED;
// static const int eiv_windspeed_east = 1 + WIND_EAST;
// static const int eiv_windspeed_north = 1 + WIND_NORTH;
// static const int eiv_r_humidity = 1 + REL_HUMID;
// static const int eiv_rmax_humidity = 1 + REL_HUMID_MAX;
// static const int eiv_rmin_humidity = 1 + REL_HUMID_MIN;
// static const int eiv_spec_humidity = 1 + SPEC_HUMID;
// static const int eiv_temp_dewpoint = 1 + TEMP_DEWPOINT;
// static const int eiv_actualVaporPressure = 1 + ACTUAL_VP;
static const int eiv_shortWaveRad = 1 + SHORT_WR;
/* inClimate */
// static const int eiv_monthlyCloudcov = 1;
// static const int eiv_monthlyWindspeed = 2;
// static const int eiv_monthlyRHumidity = 3;
// static const int eiv_monthlySnowDensity = 4;
// static const int eiv_monthlyNRainPerDay = 5;
// static const int eiv_tsoilConst = 1;
/** @} */ // end of documentation of eiv

static const char *const generalVegNames[] = {
    "<veg>.fCover",
    "<veg>.litter",
    "<veg>.biomass",
    "<veg>.pct_live",
    "<veg>.lai_conv"
};

static const char *const generalSoilNames[] = {"<veg>.transp_coeff"};

/** Possible values of the column "SW2 input group" of the tsv nc-input file */
static const char *const possInKeys[] = {
    "inDomain",
    "inSpatial",
    "inTopo",
    "inSoil",
    "inSite",
    "inVeg",
    "inWeather",
    "inClimate"
};

/* =================================================== */
/*             Local Function Definitions              */
/* --------------------------------------------------- */

/**
@brief Read in more than one value from an nc input file
when given a start index of a variable with how many values to
read

@param[in] ncFileID Identifier of the open nc file to read all information
@param[in] varID Identifier of the nc variable to read
@param[in] start List of numbers specifying the start index
of every dimension of the variable
@param[in] count List of numbers specifying the number of
values per dimension of the variable to read in
@param[in] varName Name of the variable we are trying to read the
values of
@param[out] valPtr Pointer holding the results of the read-in
values
@param[out] LogInfo Holds information on warnings and errors
*/
static void get_values_multiple(
    int ncFileID,
    int varID,
    size_t start[],
    size_t count[],
    const char *varName,
    double *valPtr,
    LOG_INFO *LogInfo
) {
    if (nc_get_vara_double(ncFileID, varID, start, count, valPtr) != NC_NOERR) {
        LogError(
            LogInfo,
            LOGERROR,
            "Could not read values from the variable '%s'.",
            varName
        );
    }
}

/**
@brief Gets the type of an attribute

@param[in] ncFileID File identifier of the file to get information from
@param[in] varID Variable identifier to get the attribute value from
@param[in] attName Attribute name to get the type of
@param[out] attType Resulting attribute type gathered
@param[out] LogInfo Holds information on warnings and errors
*/
static void get_att_type(
    int ncFileID,
    int varID,
    const char *attName,
    nc_type *attType,
    LOG_INFO *LogInfo
) {
    if (nc_inq_atttype(ncFileID, varID, attName, attType) != NC_NOERR) {
        LogError(
            LogInfo,
            LOGERROR,
            "Could not get the type of attribute '%s'.",
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
@brief Calculate the number of days within a given year based on the
calendar provided within nc files

@param[in] year Current year of the simulation to calculate the number of
days for
@param[in] allLeap Flag specifying if the nc-provided calendar is all
366 days
@param[in] noLeap Flag specifying if the nc-provided calendar is all
365 days
*/
static TimeInt num_nc_days_in_year(
    unsigned int year, Bool allLeap, Bool noLeap
) {
    TimeInt result = 0;

    if (allLeap) {
        result = MAX_DAYS;
    } else if (noLeap) {
        result = MAX_DAYS - 1;
    } else {
        result = Time_get_lastdoy_y(year);
    }

    return result;
}

/**
@brief Check that the read-in spreadsheet conains the necessary
domain and progress file inputs, fail if not

@param[in] readDomInVars A list of flags specifying if the variables
    within the key 'inDomain' are turned on or off. Position 0 indicates
    if any are turned on.
@param[out] LogInfo Holds information on warnings and errors
*/
static void check_for_input_domain(
    const Bool readDomInVars[], LOG_INFO *LogInfo
) {
    if (!readDomInVars[0]) {
        LogError(
            LogInfo,
            LOGERROR,
            "Both domain and progress variables were not provided."
        );
    } else if (!readDomInVars[eiv_domain + 1] ||
               !readDomInVars[eiv_progress + 1]) {
        LogError(
            LogInfo,
            LOGERROR,
            "The '%s' input variable is not turned on.",
            (!readDomInVars[eiv_domain + 1]) ?
                possVarNames[eSW_InDomain][eiv_domain] :
                possVarNames[eSW_InDomain][eiv_progress]
        );
    }
}

/**
@brief Check to see the input spreadsheet variable has the same units
as that provided in the provided nc file

@param[in] ncVarUnit User-provided variable unit from input spreadsheet
@param[in] ncUnit Nc file-provided variable unit
@param[out] LogInfo Holds information on warnings and errors
*/
static void invalid_conv(char *ncVarUnit, char *ncUnit, LOG_INFO *LogInfo) {
    Bool sameUnit = (Bool) (strcmp(ncVarUnit, ncUnit) == 0);

#if defined(SWUDUNITS)
    ut_system *system = NULL;
    ut_unit *unitFrom = NULL;
    ut_unit *unitTo = NULL;
    cv_converter *conv = NULL;
    ut_status status;

    Bool convertible;
    double testVal = 1;
    double res = 0;

    /* silence udunits2 error messages */
    ut_set_error_message_handler(ut_ignore);

    /* Load unit system database */
    system = ut_read_xml(NULL);

    unitFrom = ut_parse(system, ncVarUnit, UT_UTF8);
    unitTo = ut_parse(system, ncUnit, UT_UTF8);

    if (!sameUnit) {
        status = ut_get_status();

        if (status == UT_UNKNOWN) {
            LogError(
                LogInfo,
                LOGWARN,
                "The units '%s' are unknown and '%s' will be used instead.",
                ncUnit,
                ncVarUnit
            );
        } else {
            convertible = (Bool) (ut_are_convertible(unitFrom, unitTo) != 0);

            if (convertible) {
                conv = ut_get_converter(unitFrom, unitTo);
                res = cv_convert_double(conv, testVal);

                if (res != testVal) {
                    LogError(
                        LogInfo,
                        LOGERROR,
                        "The units '%s' are not equivalent to '%s'.",
                        ncVarUnit,
                        ncUnit
                    );
                }
            } else {
                LogError(
                    LogInfo,
                    LOGWARN,
                    "The units '%s' and '%s' cannot be converted. The unit "
                    "'%s' will be used.",
                    ncVarUnit,
                    ncUnit,
                    ncVarUnit
                );
            }
        }
    }

    ut_free(unitFrom);
    ut_free(unitTo);
    ut_free_system(system);

    if (!isnull(conv)) {
        cv_free(conv);
    }
#endif
}

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

    for (keyNum = 0; keyNum < SW_NINKEYSNC; keyNum++) {
        if (strcmp(varKey, possInKeys[keyNum]) == 0) {
            *inKey = keyNum;
            break;
        }
    }

    if (*inKey != eSW_NoInKey) {
        for (varNum = 0; varNum < numVarsInKey[*inKey]; varNum++) {
            if (strcmp(possVarNames[*inKey][varNum], varName) == 0) {
                if (varNum == eiv_indexSpatial && keyNum != eSW_InDomain) {
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
    char *geoCRS = SW_netCDFOut->crs_geogsc.crs_name;
    char *projCRS = SW_netCDFOut->crs_projsc.crs_name;
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

    /* Test that the provided domain CRS in the input spreadsheet match */
    if ((primCRSIsGeo && strcmp(geoCRS, ncCRSName) != 0) ||
        (!primCRSIsGeo && strcmp(projCRS, ncCRSName) != 0)) {
        LogError(
            LogInfo,
            LOGERROR,
            "Input spreadsheet domain CRS name(s) ('%s' versus '%s') do "
            "not match.",
            (primCRSIsGeo) ? geoCRS : projCRS,
            ncCRSName
        );
        return;
    }

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
    } else if (strcmp(ncCRSName, "NA") != 0 &&
               ((!primCRSIsGeo && strcmp(ncCRSName, "crs_geogsc") == 0) ||
                (primCRSIsGeo && strcmp(ncCRSName, "crs_projsc") == 0))) {
        LogError(
            LogInfo,
            LOGERROR,
            "Mismatch column 'ncCRSName' value compared to the primary "
            "CRS found in `desc_nc.in`."
        );
    } else if (strcmp(inputInfo[INSITENAME], "s") == 0 &&
               strcmp(siteName, inputInfo[INSITENAME]) != 0) {
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
@brief There are certain combinations of domain/inputs the program
accepts, this function checks the input spreadsheet that the
given configurations is an acceptable one

@param[in] primCRSIsGeo Specifies if the current CRS type is geographic
@param[in] inputInfo List of information pertaining to a specific input key
@param[in] varName First active input variable name within an input key
@param[in] domDomType Specifies the domain type that the program is
making use of
@param[out] LogInfo Holds information on warnings and errors
*/
static void check_correct_spatial_config(
    Bool primCRSIsGeo,
    char **inputInfo,
    char *varName,
    char *domDomType,
    LOG_INFO *LogInfo
) {
    Bool siteDom = (Bool) (strcmp(domDomType, "s") == 0);
    Bool inSiteDom = (Bool) (strcmp(inputInfo[INDOMTYPE], "s") == 0);
    Bool inGeoCRS =
        (Bool) (strcmp(inputInfo[INGRIDMAPPING], "latitude_longitude") == 0);
    Bool failedCaseOne = swFALSE;
    Bool failedCaseTwo = swFALSE;
    Bool failedCaseThree = swFALSE;
    Bool failedCaseFour = swFALSE;
    Bool failedCaseFive = swFALSE;
    Bool failedCaseSix = swFALSE;
    Bool failedCaseSeven = swFALSE;

    /* Gather information if fail case 1 - 3 is true; these cases cover:
        - Site, geo CRS domain with site, proj CRS input
        - Site, geo CRS domain with xy (gridded), proj CRS input
        - XY (gridded), geo CRS domain with xy (gridded), proj CRS input */
    failedCaseOne = (Bool) (siteDom && primCRSIsGeo && inSiteDom && !inGeoCRS);
    failedCaseTwo = (Bool) (siteDom && primCRSIsGeo && !inSiteDom && !inGeoCRS);
    failedCaseThree =
        (Bool) (!siteDom && primCRSIsGeo && !inSiteDom && !inGeoCRS);

    if (failedCaseOne || failedCaseTwo || failedCaseThree) {
        LogError(
            LogInfo,
            LOGERROR,
            "Simulation domain has geographic CRS but input file containing "
            "'%s' has a projected CRS.",
            varName
        );
        return;
    }

    /* Gather information if fail case 4 - 7 is true; these cases cover:
        - XY (gridded), geo CRS domain with site, geo CRS input
        - XY (gridded), geo CRS domain with site, proj CRS input
        - XY (gridded), proj CRS domain with site, geo CRS input
        - XY (gridded), proj CRS domain with site, proj CRS input */
    if (!siteDom) {
        failedCaseFour = (Bool) (primCRSIsGeo && inSiteDom && inGeoCRS);
        failedCaseFive = (Bool) (primCRSIsGeo && inSiteDom && !inGeoCRS);
        failedCaseSix = (Bool) (!primCRSIsGeo && inSiteDom && inGeoCRS);
        failedCaseSeven = (Bool) (!primCRSIsGeo && inSiteDom && !inGeoCRS);

        if (failedCaseFour || failedCaseFive || failedCaseSix ||
            failedCaseSeven) {

            LogError(
                LogInfo,
                LOGERROR,
                "Simulation domain is 'xy' but input domain of the file "
                "containing '%s' is 's'.",
                varName
            );
        }
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
    int k;

    /* Indices are based on the global array `possVarNames` under `inVeg` */
    Bool isLitter = swFALSE;
    Bool isBio = swFALSE;
    Bool isPctLive = swFALSE;
    Bool isLAI = swFALSE;

    Bool testVeg = swFALSE;
    Bool canBeNA;

    Bool isIndex = (Bool) (key > eSW_InDomain && varNum == eiv_indexSpatial);
    Bool inputDomIsSite =
        (Bool) (strcmp(inputInfo[varNum][INDOMTYPE], "s") == 0);

    char *varName = inputInfo[varNum][INNCVARNAME];


    ForEachVegType(k) {
        isLitter = (Bool) (isLitter || varNum == eiv_vegLitter[k]);
        isBio = (Bool) (isBio || varNum == eiv_vegBiomass[k]);
        isPctLive = (Bool) (isPctLive || varNum == eiv_vegPctlive[k]);
        isLAI = (Bool) (isLAI || varNum == eiv_vegLAIconv[k]);
    }

    /* Make sure that the universally required attributes are filled in
       skip the testing of the nc var units (can be NA) */
    for (attNum = 0; attNum < mustTestAtts; attNum++) {
        testInd = mustTestAttInd[attNum];
        canBeNA = (Bool) ((testInd == INSITENAME && !inputDomIsSite) ||
                          testInd == INCRSNAME);

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
@param[in] key Current input key the variable is in
@param[out] LogInfo Holds information on warnings and errors
*/
static void check_inputkey_columns(
    char ***inputInfo, const Bool readInVars[], int key, LOG_INFO *LogInfo
) {

    int compIndex = -1;
    int varNum;
    int varStart = (key > eSW_InDomain) ? 1 : 0;
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
                        ignoreAtt = (Bool) ((varNum > eiv_bareGroundfCover &&
                                             attNum == attEnd) ||
                                            (attNum == INTAXIS));
                    } else if (key == eSW_InSoil) {
                        ignoreAtt =
                            (Bool) (varNum >= eiv_transpCoeff[0] &&
                                    varNum <= eiv_transpCoeff[NVEGTYPES - 1] &&
                                    attNum == attEnd);
                    }

                    if (!ignoreAtt && strcmp(currAtt, cmpAtt) != 0) {
                        LogError(
                            LogInfo,
                            LOGERROR,
                            "The variable '%s' within the input key '%s' "
                            "has a column that does not match the others "
                            "from 'ncDomType' to 'ncVAxisName' with a "
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
            varStart = (key > eSW_InDomain) ? 1 : 0;

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
            } else {
                check_correct_spatial_config(
                    SW_netCDFOut->primary_crs_is_geographic,
                    inputInfo[key][testVarIndex],
                    inputInfo[key][testVarIndex][INNCVARNAME],
                    inputInfo[eSW_InDomain][0][INDOMTYPE],
                    LogInfo
                );
            }
            if (LogInfo->stopRun) {
                return; /* Exit function prematurely due to failed test */
            }
        }
    }
}

/**
@brief Helper function to check for availability of required soil inputs

Required soil properties if not constant soils:
    1. one out of {width, depth}
    2. soil density
    3. gravel
    4. two out of {sand, silt, clay}
    5. soil organic matter
    6. evaporation coefficients
    7. transpiration coefficients
    8. SWRCp

Soil properties that are not required (value of 0 will be assumed if missing):
    9. impermeability
    10. initial soil temperature

@param[in] readInVarsSoils Specifies which variables are to be read-in as
    soil inputs
@param[in] hasConsistentSoilLayerDepths Flag indicating if all simulation
    run within domain have identical soil layer depths
    (though potentially variable number of soil layers)
@param[in] inputsProvideSWRCp Are SWRC parameters obtained from
    input files (TRUE) or estimated with a PTF (FALSE)
@param[out] LogInfo Holds information on warnings and errors
*/
static void checkRequiredSoils(
    const Bool readInVarsSoils[],
    Bool hasConsistentSoilLayerDepths,
    Bool inputsProvideSWRCp,
    LOG_INFO *LogInfo
) {
    char soilErrorMsg[MAX_FILENAMESIZE] = "";
    char tmpStr[100] = "";
    char *writePtr = soilErrorMsg;
    char *endWritePtr = soilErrorMsg + sizeof soilErrorMsg - 1;
    size_t writeSize = MAX_FILENAMESIZE;
    int tmp;
    int nSWRCInputs;
    int k;
    const int nRequired1Var = 4;
    int required1Vars[4] = {
        eiv_soilDensity, eiv_gravel, eiv_som, eiv_evapCoeff
    };
    const int nSuggested1Vars = 2;
    int suggested1Vars[2] = {eiv_impermeability, eiv_avgLyrTempInit};
    Bool fullBuffer = swFALSE;


    /* Count number of input SWRCp */
    nSWRCInputs = 0;
    for (k = 0; k < SWRC_PARAM_NMAX; k++) {
        nSWRCInputs += (int) readInVarsSoils[eiv_swrcpMS[k] + 1];
    }

    /* Warn user if both PTF and any SWRCp nc-input are turned on */
    if (!inputsProvideSWRCp && nSWRCInputs > 0) {
        LogError(
            LogInfo,
            LOGWARN,
            "Requested pedotransfer function will overwrite "
            "provided SWRCp inputs: activate one or the other but not both."
        );
    }


    /* Check that we have sufficient soil inputs */
    if (hasConsistentSoilLayerDepths) {
        /* Note: SW_NCIN_soilProfile() warns if any soil nc-input has
          fewer layers than nMaxSoilLayers
        */
        /* Note: read_soil_inputs() will check that input
           layerDepth and layerWidth are consistent with depthsAllSoilLayers
           for each site/gridcell
        */

    } else {
        // Check that we have sufficient netCDF inputs to create complete soils

        // Check conditions that are warnings
        for (k = 0; k < nSuggested1Vars; k++) {
            if (!readInVarsSoils[suggested1Vars[k] + 1]) {
                LogError(
                    LogInfo,
                    LOGWARN,
                    "'%s' is suggested but not provided as soil input: "
                    "a default value of 0 will be used",
                    possVarNames[eSW_InSoil][suggested1Vars[k]]
                );
            }
        }

        // Check conditions that are errors
        for (k = 0; k < nRequired1Var; k++) {
            if (!readInVarsSoils[required1Vars[k] + 1]) {
                (void) snprintf(
                    tmpStr,
                    sizeof tmpStr,
                    "'%s' is required; ",
                    possVarNames[eSW_InSoil][required1Vars[k]]
                );

                fullBuffer = sw_memccpy_inc(
                    (void **) &writePtr,
                    endWritePtr,
                    (void *) tmpStr,
                    '\0',
                    &writeSize
                );
                if (fullBuffer) {
                    goto reportFullBuffer;
                }
            }
        }

        // Required: one out of {width, depth}
        tmp = (int) readInVarsSoils[eiv_soilLayerDepth + 1] +
              (int) readInVarsSoils[eiv_soilLayerWidth + 1];
        if (tmp < 1) {
            fullBuffer = sw_memccpy_inc(
                (void **) &writePtr,
                endWritePtr,
                (void *) "either layer depth or layer width is required; ",
                '\0',
                &writeSize
            );
            if (fullBuffer) {
                goto reportFullBuffer;
            }
        }

        // Required: two out of {sand, silt, clay}
        tmp = (int) readInVarsSoils[eiv_sand + 1] +
              (int) readInVarsSoils[eiv_silt + 1] +
              (int) readInVarsSoils[eiv_clay + 1];
        if (tmp < 2) {
            fullBuffer = sw_memccpy_inc(
                (void **) &writePtr,
                endWritePtr,
                (void *) "two out of sand, silt, clay are required; ",
                '\0',
                &writeSize
            );
            if (fullBuffer) {
                goto reportFullBuffer;
            }
        }

        // Required: all transpiration coefficients
        tmp = 0;
        ForEachVegType(k) {
            tmp += (int) readInVarsSoils[eiv_transpCoeff[k] + 1];
        }
        if (tmp != NVEGTYPES) {
            fullBuffer = sw_memccpy_inc(
                (void **) &writePtr,
                endWritePtr,
                (void *) "all transpiration coefficients are required; ",
                '\0',
                &writeSize
            );
            if (fullBuffer) {
                goto reportFullBuffer;
            }
        }

        // Required: all SWRCp required unless estimated via PTF
        if (inputsProvideSWRCp && nSWRCInputs != SWRC_PARAM_NMAX) {
            fullBuffer = sw_memccpy_inc(
                (void **) &writePtr,
                endWritePtr,
                (void *) "all SWRC parameters are required; ",
                '\0',
                &writeSize
            );
            if (fullBuffer) {
                goto reportFullBuffer;
            }
        }

        if (writeSize != MAX_FILENAMESIZE) {
            LogError(
                LogInfo, LOGERROR, "Incomplete soil inputs: %s", soilErrorMsg
            );
        }
    }

reportFullBuffer:
    if (fullBuffer) {
        reportFullBuffer(LOGWARN, LogInfo);
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
        (void) snprintf(
            coordStr,
            MAX_FILENAMESIZE,
            coordVal,
            readinGeoYName,
            readinGeoXName,
            siteName
        );
    } else {
        (void) snprintf(
            coordStr, MAX_FILENAMESIZE, coordVal, readinGeoYName, readinGeoXName
        );
    }
    strAttVals[numAtts - 1] = coordStr;

    if (!primCRSIsGeo) {
        (void) snprintf(
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
        {"latitude", "latitude", "degree_north", "Y"},
        {"longitude", "longitude", "degree_east", "X"},
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
        {"latitude", "latitude", "degree_north", "Y", "lat_bnds"},
        {"longitude", "longitude", "degree_east", "X", "lon_bnds"},
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
@brief Dynamically get the value of the attribute '_FillValue' from the
domain variable

@param[in] domFileID Identifier of the domain file
@param[in] domVarID Identifier of the domain variable within the domain file
@param[out] LogInfo Holds information on warnings and errors
*/
static long get_dom_fill_value(int domFileID, int domVarID, LOG_INFO *LogInfo) {
    signed char byteVal;
    short twoByteVal;
    unsigned short uTwoByteVal;
    int fourByteVal;
    unsigned int uFourByteVal;
    float floatVal;
    double doubleVal;

    nc_type fillValType = 0;
    long result = 0;
    int callResult = NC_NOERR;

    get_att_type(domFileID, domVarID, "_FillValue", &fillValType, LogInfo);
    if (LogInfo->stopRun) {
        return (long) (NC_FILL_UINT);
    }

    switch (fillValType) {
    case NC_BYTE:
        callResult =
            nc_get_att_schar(domFileID, domVarID, "_FillValue", &byteVal);
        result = (long) byteVal;
        break;
    case NC_SHORT:
        callResult =
            nc_get_att_short(domFileID, domVarID, "_FillValue", &twoByteVal);
        result = (long) twoByteVal;
        break;
    case NC_USHORT:
        callResult =
            nc_get_att_ushort(domFileID, domVarID, "_FillValue", &uTwoByteVal);
        result = (long) uTwoByteVal;
        break;
    case NC_INT:
        callResult =
            nc_get_att_int(domFileID, domVarID, "_FillValue", &fourByteVal);
        result = (long) fourByteVal;
        break;
    case NC_UINT:
        callResult =
            nc_get_att_uint(domFileID, domVarID, "_FillValue", &uFourByteVal);
        result = (long) uFourByteVal;
        break;
    case NC_FLOAT:
        callResult =
            nc_get_att_float(domFileID, domVarID, "_FillValue", &floatVal);
        result = (long) floatVal;
        break;
    default: /* NC_DOUBLE */
        callResult =
            nc_get_att_double(domFileID, domVarID, "_FillValue", &doubleVal);
        result = (long) doubleVal;
        break;
    }

    if (callResult != NC_NOERR) {
        LogError(
            LogInfo,
            LOGERROR,
            "Could not read the value of the attribute '_FillValue' for "
            "the domain variable."
        );
        return (long) NC_FILL_UINT;
    }

    if ((fillValType == NC_DOUBLE || fillValType == NC_FLOAT) &&
        (!EQ(fmod(fillValType, 1.0), 0.0))) {
        LogError(
            LogInfo,
            LOGERROR,
            "Domain variable attribute '_FillValue' must be a whole number "
            "when holding a floating-point type."
        );
    }

    return result;
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
    Bool siteDom = (Bool) (strcmp(SW_Domain->DomainType, "s") == 0);
    unsigned int subVal;
    size_t chunkSizes[2] = {1, 1};
    int storageType;
    size_t startRead[2] = {0, 0};
    size_t countRead[2] = {0, 0};
    size_t numChunkReads = 0;
    size_t numChunkInYAxis;
    size_t numChunkInXAxis;

    long *readDomVals = NULL;
    signed char *vals = (signed char *) Mem_Malloc(
        nSUIDs * sizeof(signed char), "fill_prog_netCDF_vals()", LogInfo
    );
    if (LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

    size_t attSize = 0; /* Not used */
    Bool domFillAttExists = swFALSE;
    long fillVal = (long) (NC_FILL_UINT);

    att_exists(
        domFileID, domVarID, "_FillValue", &attSize, &domFillAttExists, LogInfo
    );
    if (LogInfo->stopRun) {
        goto freeMem;
    }

    fillVal = get_dom_fill_value(domFileID, domVarID, LogInfo);
    if (LogInfo->stopRun) {
        goto freeMem;
    }

    if (nc_inq_var_chunking(domFileID, domVarID, &storageType, chunkSizes) !=
        NC_NOERR) {
        LogError(
            LogInfo,
            LOGERROR,
            "Could not get chunking information on domain variable."
        );
        goto freeMem;
    }

    readDomVals = (long *) Mem_Malloc(
        sizeof(long) * chunkSizes[0] * chunkSizes[1],
        "fill_prog_netCDF_vals()",
        LogInfo
    );
    if (LogInfo->stopRun) {
        goto freeMem;
    }

    // Calculate how many chunks are held within the entirety of
    // the vertical (or site) axis for the program's domain
    countRead[0] = chunkSizes[0];
    numChunkInYAxis = (siteDom) ? SW_Domain->nDimS : SW_Domain->nDimY;
    numChunkInYAxis /= chunkSizes[0];
    if (!siteDom) {
        countRead[1] = chunkSizes[1];
        numChunkInXAxis = SW_Domain->nDimX / chunkSizes[1];
    }

    for (suid = 0; suid < nSUIDs;) {
        SW_DOM_calc_ncSuid(SW_Domain, suid, ncSuid);

        // Set the new chunks start position(s) for the next read
        startRead[0] = (numChunkReads / numChunkInYAxis) * chunkSizes[0];
        if (!siteDom) {
            startRead[1] = (numChunkReads % numChunkInXAxis) * chunkSizes[1];
        }

        if (nc_get_vara_long(
                domFileID, domVarID, startRead, countRead, readDomVals
            ) != NC_NOERR) {
            LogError(
                LogInfo,
                LOGERROR,
                "Could not read domain status for SUIDs #%lu - #%lu.",
                suid,
                suid + (chunkSizes[0] * chunkSizes[1])
            );
        }

        for (subVal = 0;
             subVal < chunkSizes[0] * chunkSizes[1] && suid < nSUIDs;
             subVal++) {
            vals[suid] =
                (readDomVals[subVal] == fillVal) ? NC_FILL_BYTE : PRGRSS_READY;
            suid++;
        }

        numChunkReads++;
    }

    SW_NC_write_vals(
        &progVarID, progFileID, "progress", vals, start, count, "byte", LogInfo
    );
    nc_sync(progFileID);

// Free allocated memory
freeMem:
    if (!isnull(vals)) {
        free(vals);
    }

    if (!isnull(readDomVals)) {
        free(readDomVals);
    }
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
@param[in] readInVars A list of flags specifying the variables that are
to be read in within the weather input key
@param[out] outWeathFileNames Generated input file names based on stride
informatoin
@param[out] ncWeatherInStartEndYrs Specifies that start/end years of each input
weather file
@param[out] numncWeatherInFiles Specifies the number of input files each
weather variable has
@param[out] weathStartFileIndex Specifies the index, or file number,
the generated list of files should start (i.e., we should not assume
all lists of files start at index 0)
@param[out] LogInfo LogInfo Holds information on warnings and errors
*/
static void generate_weather_filenames(
    char **weathNameFormat,
    const int strideInfo[],
    char ***weatherInputInfo,
    TimeInt startYr,
    TimeInt endYr,
    const Bool readInVars[],
    char ****outWeathFileNames,
    unsigned int ***ncWeatherInStartEndYrs,
    unsigned int *numncWeatherInFiles,
    unsigned int *weathStartFileIndex,
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
            if (endFileYr + numStYr <= endYr) {
                endFileYr += numStYr;
            } else {
                endFileYr = endYr;
            }

            inFileNum++;
        }
    }

    inFileNum = 0;
    while (inFileNum < (int) *numncWeatherInFiles &&
           (*ncWeatherInStartEndYrs)[inFileNum][1] < startYr) {
        inFileNum++;
    }
    *weathStartFileIndex = inFileNum;

    if (inFileNum == (int) *numncWeatherInFiles) {
        LogError(
            LogInfo,
            LOGERROR,
            "Could not find a weather input file that overlaps with the "
            "start year."
        );
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
    int domFileID,
    Bool allocArrays[],
    char *varNames[],
    LOG_INFO *LogInfo
) {
    size_t coordArr;
    size_t coordIndex;
    size_t allocSize;
    const int numGeo = 2;

    for (coordArr = 0; coordArr < (size_t) numCoords; coordArr++) {
        allocArrays[coordArr] = SW_NC_varExists(domFileID, varNames[coordArr]);

        if (allocArrays[coordArr]) {
            allocSize = *domCoordSizes[coordArr];
            if (numCoords > numGeo && coordArr < (size_t) numGeo) {
                allocSize *= *domCoordSizes[coordArr + 1];
            }

            *domCoordArrs[coordArr] = (double *) Mem_Malloc(
                sizeof(double) * allocSize, "alloc_dom_coord_info()", LogInfo
            );

            for (coordIndex = 0; coordIndex < allocSize; coordIndex++) {
                (*domCoordArrs[coordArr])[coordIndex] = 0.0;
            }
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
    Bool siteDom = (Bool) (strcmp("s", domType) == 0);
    Bool allocArrays[] = {swFALSE, swFALSE, swFALSE, swFALSE};
    Bool validY;
    int numDimsInVar = 0;
    int dimIDs[] = {-1, -1};
    int numIter;
    int varIDs[] = {-1, -1};
    int result;
    int dimIter;
    int firstDimID;
    size_t temp;

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
    if (siteDom) {
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
        SW_NC_get_var_identifier(
            domFileID, domCoordNames[index], &varIDs[index], LogInfo
        );
        if (LogInfo->stopRun) {
            return;
        }
    }

    if (nc_inq_varndims(domFileID, varIDs[0], &numDimsInVar) != NC_NOERR) {
        LogError(
            LogInfo,
            LOGERROR,
            "Could not get the number of dimensions from the variable '%s'.",
            domCoordNames[0]
        );
        return;
    }
    if (numDimsInVar > 2) {
        LogError(
            LogInfo,
            LOGERROR,
            "Domain file contains domain variables with more than two "
            "dimensions."
        );
        return;
    }

    numIter = (numDimsInVar == 1) ? numDims : 1;
    for (index = 0; index < numIter; index++) {
        result = nc_inq_vardimid(
            domFileID, varIDs[index], (numIter == 1) ? dimIDs : &dimIDs[index]
        );
        if (result != NC_NOERR) {
            LogError(
                LogInfo,
                LOGERROR,
                "Could not get the dimension IDs of the variable '%s'.",
                domCoordNames[index]
            );
            return;
        }

        for (dimIter = 0; dimIter < numDimsInVar; dimIter++) {
            SW_NC_get_dimlen_from_dimid(
                domFileID,
                dimIDs[dimIter + index],
                domCoordSizes[dimIter + index],
                LogInfo
            );
        }
    }

    SW_NC_get_dim_identifier(domFileID, domCoordNames[0], &firstDimID, LogInfo);
    if (LogInfo->stopRun) {
        return;
    }

    if (numDimsInVar == 2 && firstDimID == dimIDs[1]) {
        temp = *(domCoordSizes[0]);
        *(domCoordSizes[0]) = *(domCoordSizes[1]);
        *(domCoordSizes[1]) = temp;
    }

    if (!primCRSIsGeo) {
        *(domCoordSizes[2]) = *(domCoordSizes[0]);
        *(domCoordSizes[3]) = *(domCoordSizes[1]);
    }

    alloc_dom_coord_info(
        domCoordArrs,
        domCoordSizes,
        numReadInDims,
        domFileID,
        allocArrays,
        domCoordVarNames,
        LogInfo
    );
    if (LogInfo->stopRun) {
        return; /* Exit function prematurely due to error */
    }

    for (index = 0; index < numReadInDims; index++) {
        varID = -1;

        if (allocArrays[index]) {
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

            if (primCRSIsGeo && index == 0) {
                /* Check first and laste value of latitude */
                validY =
                    (Bool) ((GE(*(domCoordArrs[index])[0], -90.0) &&
                             LE(*(domCoordArrs[index])[0], 90.0)) ||
                            (GE(*(domCoordArrs[index])[*domCoordSizes[0] - 1],
                                -90.0) &&
                             LE(*(domCoordArrs[index])[*domCoordSizes[0] - 1],
                                90.0)));

                if (!validY) {
                    LogError(
                        LogInfo,
                        LOGERROR,
                        "Coordinate value(s) do not fit within the "
                        "range [-90, 90] for the variable '%s'.",
                        domCoordVarNames[index]
                    );
                    return;
                }
            }
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
    int varNum;
    int varID;

    double **yxVals[] = {readinYVals, readinXVals};
    double *domYVals = (inPrimCRSIsGeo) ? SW_netCDFIn->domYCoordsGeo :
                                          SW_netCDFIn->domYCoordsProj;
    double *domXVals = (inPrimCRSIsGeo) ? SW_netCDFIn->domXCoordsGeo :
                                          SW_netCDFIn->domXCoordsProj;

    size_t *ySize = dimSizes[0];
    size_t *xSize = dimSizes[1];
    size_t start[] = {0};
    size_t count[] = {0};

    for (varNum = 0; varNum < numReadInDims; varNum++) {
        get_var_dimsizes(
            ncFileID, 1, &dimSizes[varNum], yxVarNames[varNum], &varID, LogInfo
        );
        if (LogInfo->stopRun) {
            return;
        }
    }

    for (varNum = 0; varNum < numReadInDims; varNum++) {
        varID = -1;
        count[0] = *dimSizes[varNum];

        *(yxVals[varNum]) = (double *) Mem_Malloc(
            sizeof(double) * *dimSizes[varNum],
            "get_1D_input_coordinates()",
            LogInfo
        );
        if (LogInfo->stopRun) {
            return;
        }

        SW_NC_get_var_identifier(ncFileID, yxVarNames[varNum], &varID, LogInfo);
        if (LogInfo->stopRun) {
            return;
        }

        get_values_multiple(
            ncFileID,
            varID,
            start,
            count,
            yxVarNames[varNum],
            *(yxVals[varNum]),
            LogInfo
        );
        if (LogInfo->stopRun) {
            return; /* Exit prematurely due to error */
        }
    }

    if (compareCoords) {
        *useIndexFile = (Bool) ((inPrimCRSIsGeo &&
                                 (*ySize != SW_netCDFIn->domYCoordGeoSize ||
                                  *xSize != SW_netCDFIn->domXCoordGeoSize)) ||
                                (!inPrimCRSIsGeo &&
                                 (*ySize != SW_netCDFIn->domYCoordProjSize ||
                                  *xSize != SW_netCDFIn->domXCoordProjSize)));

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
@param[in] yDimName A user-provided name of the y-dimension, in
general this is separate from the coordinate variable name
but it can be the exact same
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
    char *yDimName,
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
    double *domYVals = (inPrimCRSIsGeo) ? SW_netCDFIn->domYCoordsGeo :
                                          SW_netCDFIn->domYCoordsProj;
    double *domXVals = (inPrimCRSIsGeo) ? SW_netCDFIn->domXCoordsGeo :
                                          SW_netCDFIn->domXCoordsProj;

    int varIDs[2] = {-1, -1};
    int varNum;
    size_t start[] = {0, 0};
    size_t count[] = {0, 0};
    int firstDimID = 0;
    int varDimIDs[2] = {0};

    double **xyVals[] = {readinYVals, readinXVals};

    /* === Latitude/longitude sizes === */
    get_var_dimsizes(
        ncFileID, 2, allDimSizes, yxVarNames[0], &varIDs[0], LogInfo
    );
    if (LogInfo->stopRun) {
        return; /* Exit function prematurely due to error */
    }

    SW_NC_get_var_identifier(ncFileID, yxVarNames[1], &varIDs[1], LogInfo);
    if (LogInfo->stopRun) {
        return;
    }

    /* Get information to decide the order of lat/lon or y/x dimensions */
    SW_NC_get_dim_identifier(ncFileID, yDimName, &firstDimID, LogInfo);
    if (LogInfo->stopRun) {
        return;
    }

    if (nc_inq_vardimid(ncFileID, varIDs[0], varDimIDs) != NC_NOERR) {
        LogError(
            LogInfo,
            LOGERROR,
            "Could not get the dimension IDs of the variable '%s'.",
            yxVarNames[0]
        );
    }

    *dimSizes[0] = yDimSize;
    *dimSizes[1] = xDimSize;
    count[0] = (varDimIDs[0] == firstDimID) ? yDimSize : xDimSize;
    count[1] = (varDimIDs[0] == firstDimID) ? xDimSize : yDimSize;
    numPoints = yDimSize * xDimSize;

    for (varNum = 0; varNum < numReadInDims; varNum++) {
        (*xyVals[varNum]) = (double *) Mem_Malloc(
            sizeof(double) * numPoints, "get_2D_input_coordinates()", LogInfo
        );
        if (LogInfo->stopRun) {
            return; /* Exit function prematurely due to error */
        }

        get_values_multiple(
            ncFileID,
            varIDs[varNum],
            start,
            count,
            yxVarNames[varNum],
            *(xyVals[varNum]),
            LogInfo
        );
        if (LogInfo->stopRun) {
            return;
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
@param[in] yDimName User-provided name for the y-dimension (may or
may not be the same as the coordinate variable)
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
    char *yDimName,
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

    if (inPrimCRSIsGeo && (isnull(SW_netCDFIn->domYCoordsGeo) ||
                           isnull(SW_netCDFIn->domXCoordsGeo))) {

        LogError(
            LogInfo,
            LOGERROR,
            "Programs domain does not provide geographic coordinates to "
            "use for geographic input domains."
        );
    }

    if (*coordVarIs2D) {
        get_2D_input_coordinates(
            SW_netCDFIn,
            *ncFileID,
            readinYVals,
            readinXVals,
            dimSizes,
            yxVarNames,
            yDimName,
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
@param[in] calIsAllLeap Specifies if the calendar only contains 366 days
@param[in] fileName Weather input file name
@param[out] LogInfo Holds information dealing with logfile output
*/
static void determine_valid_cal(
    char *calType,
    char *calUnit,
    Bool *calIsNoLeap,
    Bool *calIsAllLeap,
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
        "allleap",
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
                    LOGWARN,
                    "The usage of the calendar '%s' has been detected. "
                    "When not a leap year, this will result in the 366th "
                    "value within the year being ignored.",
                    calType
                );
                *calIsAllLeap = swTRUE;
            } else if (index >= 7 && index <= 11) { /* No leap calendars */
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
@param[in] numDaysInYear A list of values specifying the number
of days within every year of the simulation
@param[in] numYears Number of years within the simulation
@param[out] LogInfo Holds information dealing with logfile output
*/
static void alloc_weather_indices_years(
    unsigned int ***ncWeatherStartEndIndices,
    unsigned int numStartEndIndices,
    unsigned int **numDaysInYear,
    unsigned int numYears,
    LOG_INFO *LogInfo
) {
    unsigned int index;

    (*ncWeatherStartEndIndices) = (unsigned int **) Mem_Malloc(
        sizeof(unsigned int *) * numStartEndIndices,
        "alloc_weather_indices_years()",
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
            sizeof(unsigned int) * 2, "alloc_weather_indices_years()", LogInfo
        );
        if (LogInfo->stopRun) {
            return; /* Exit function prematurely due to error */
        }
    }

    (*numDaysInYear) = (unsigned int *) Mem_Malloc(
        sizeof(unsigned int) * numYears,
        "alloc_weather_indices_years()",
        LogInfo
    );

    for (index = 0; index < numYears; index++) {
        (*numDaysInYear)[index] = 0;
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
    int varID = -1;
    nc_type ncVarType = 0;
    size_t *timeSizeArr[] = {timeSize};
    char *timeNameArr[] = {timeName};
    size_t start[] = {0};
    size_t count[] = {0};

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

    count[0] = *timeSize;

    get_values_multiple(
        ncFileID, varID, start, count, timeNameArr[0], *timeVals, LogInfo
    );
    if (LogInfo->stopRun) {
        return;
    }
}

/**
@brief Convert number of days since the input-provided start date to
file-specific indices to know the start/end indices of the temporal
dimension for weather

@param[out] ncWeatherStartEndIndices Start/end indices for the current
weather input file
@param[in] timeVals List of time values from the current weather input file
@param[in] numDays A value specifying the number of days within the current
year we are calculating indices for (base0)
@param[in] timeSize Time dimension size
@param[in] target Start temporal value that we will search for to get the
index
@param[in] fileName Weather input file name that is being searched
@param[in] timeName User-provided time variable/dimension name
@param[out] LogInfo Holds information dealing with logfile output
*/
static void get_startend_indices(
    unsigned int *ncWeatherStartEndIndices,
    const double *timeVals,
    unsigned int numDays,
    size_t timeSize,
    double target,
    char *fileName,
    char *timeName,
    LOG_INFO *LogInfo
) {
    int left = 0;
    int right = (int) timeSize - 1;
    int middle;

    while (left <= right) {
        middle = left + (right - left) / 2;

        // NOLINTNEXTLINE(clang-analyzer-core.NullDereference)
        if (GT(timeVals[middle], target)) {
            right = middle - 1;
        } else if (LT(timeVals[middle], target)) {
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
    unsigned int fileIndex = SW_PathInputs->weathStartFileIndex;
    int probeIndex = -1;
    int varIndex = 1;
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
    Bool calIsAllLeap = swFALSE;
    double *timeVals = NULL;
    size_t timeSize = 0;
    int tempStart = -1;

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

    alloc_weather_indices_years(
        &SW_PathInputs->ncWeatherStartEndIndices,
        numWeathFiles,
        &SW_PathInputs->numDaysInYear,
        endYr - startYr + 1,
        LogInfo
    );
    if (LogInfo->stopRun) {
        return; /* Exit function prematurely due to error */
    }

    /* Go through each year and get the indices within the input file(s) */
    for (year = startYr; year <= endYr; year++) {
        weatherEnd = SW_PathInputs->ncWeatherInStartEndYrs[fileIndex][1];

        /* Increment file and reset all information for a new file */
        if (year > weatherEnd) {
            SW_PathInputs->ncWeatherStartEndIndices[fileIndex][0] =
                (unsigned int) tempStart;
            fileIndex++;
            currCalType[0] = currCalUnit[0] = newCalUnit[0] = '\0';
            nc_close(ncFileID);

            ncFileID = -1;
            tempStart = -1;
        }

        fileName = weathInFiles[fileIndex];

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
                    weatherCal,
                    currCalUnit,
                    &calIsNoLeap,
                    &calIsAllLeap,
                    fileName,
                    LogInfo
                );
                if (LogInfo->stopRun) {
                    goto freeMem;
                }

                checkedCal = swTRUE;
            }
        }

        (void) snprintf(
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
        SW_PathInputs->numDaysInYear[year - startYr] =
            num_nc_days_in_year(year, calIsAllLeap, calIsNoLeap);

        get_startend_indices(
            SW_PathInputs->ncWeatherStartEndIndices[fileIndex],
            timeVals,
            SW_PathInputs->numDaysInYear[year - startYr] - 1, /* base0 */
            timeSize,
            valDoy1,
            fileName,
            timeName,
            LogInfo
        );
        // NOLINTEND(clang-analyzer-core.NullDereference)
        if (LogInfo->stopRun) {
            goto freeMem;
        }

        if (tempStart == -1) {
            tempStart =
                (int) SW_PathInputs->ncWeatherStartEndIndices[fileIndex][0];
        }

        free(timeVals);
        timeVals = NULL;
    }

    if (tempStart > -1) {
        SW_PathInputs->ncWeatherStartEndIndices[fileIndex][0] =
            (unsigned int) tempStart;
    }

freeMem: {
    if (!isnull(timeVals)) {
        free(timeVals);
        timeVals = NULL;
    }

    if (ncFileID > -1) {
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
@brief Similar to what is done with text weather, find out the flags
for each weather input, this uses the "read in variable" flags instead
of those in the weather `weathsetup.in`

@param[in] SW_netCDFIn Constant netCDF input file information
@param[out] SW_WeatherIn Struct of type SW_WEATHER_INPUTS holding all relevant
information pretaining to meteorological input data
@param[out] LogInfo Holds information dealing with logfile output
*/
static void get_weather_flags(
    SW_NETCDF_IN *SW_netCDFIn,
    SW_WEATHER_INPUTS *SW_WeatherIn,
    LOG_INFO *LogInfo
) {
    int varNum;
    Bool *weathVarFlags = SW_netCDFIn->readInVars[eSW_InWeather];

    for (varNum = 1; varNum < numVarsInKey[eSW_InWeather]; varNum++) {
        SW_WeatherIn->dailyInputFlags[varNum - 1] = weathVarFlags[varNum + 1];
    }

    check_and_update_dailyInputFlags(
        SW_WeatherIn->use_cloudCoverMonthly,
        SW_WeatherIn->use_humidityMonthly,
        SW_WeatherIn->use_windSpeedMonthly,
        SW_WeatherIn->dailyInputFlags,
        LogInfo
    );
}

/**
@brief Determine if the spatial coordinates within the input file
being tested is the same as that of which the program understands
(our own domain generated in `domain.nc`)

@param[in,out] SW_netCDFIn Constant netCDF input file information
@param[in] SW_PathInputs Struct of type SW_PATH_INPUTS which
holds basic information about input files and values
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
    char *yDimName;
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
    unsigned int weathFileIndex = SW_PathInputs->weathStartFileIndex;

    ForEachNCInKey(k) {
        if (SW_netCDFIn->readInVars[k][0] && k > eSW_InDomain) {
            fIndex = 1;
            ncFileID = -1;

            /* Find the first input file within the key */
            while (!SW_netCDFIn->readInVars[k][fIndex + 1]) {
                fIndex++;
            }

            if (k == eSW_InWeather) {
                fileName =
                    SW_PathInputs->ncWeatherInFiles[fIndex][weathFileIndex];
            } else {
                fileName = SW_PathInputs->ncInFiles[k][fIndex];
            }

            axisNames[0] = SW_netCDFIn->inVarInfo[k][fIndex][INYAXIS];
            axisNames[1] = SW_netCDFIn->inVarInfo[k][fIndex][INXAXIS];
            yDimName = SW_netCDFIn->inVarInfo[k][fIndex][INYDIM];

            if (strcmp(yDimName, "NA") == 0) {
                yDimName = axisNames[0];
            }

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
                yDimName,
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
            if (LogInfo->stopRun) {
                return;
            }
        } else {
            SW_netCDFIn->useIndexFile[k] = swFALSE;
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
            (void) snprintf(
                tempCoord,
                MAX_FILENAMESIZE,
                coordString,
                geoYCoordName,
                geoXCoordName,
                domSiteName
            );
        } else {
            (void) snprintf(
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
                (void) snprintf(
                    tempIndexVal,
                    MAX_FILENAMESIZE,
                    indexVarAttVals[attNum][varNum],
                    possInKeys[key]
                );
                break;
            case 1:
                (void) snprintf(
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
@param[in] yxConvs A list of two unit converters for the coordinate variables
(one for y and one for x)
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
    sw_converter_t *yxConvs[],
    LOG_INFO *LogInfo
) {
    SW_KD_NODE *treeRoot = NULL;
    SW_KD_NODE *nearNeighbor = NULL;

    double bestDist;
    double queryCoords[2] = {0};

    size_t yIndex;
    size_t xIndex;
    size_t syWritePos[] = {0, 0};
    size_t xWritePos[] = {0, 0};
    size_t writeCount[] = {1, 0};

    SW_DATA_create_tree(
        &treeRoot,
        yCoords,
        xCoords,
        *(inFileDimSizes[0]),
        *(inFileDimSizes[1]),
        inIsGridded,
        has2DCoordVars,
        inPrimCRSIsGeo,
        yxConvs,
        LogInfo
    );
    if (LogInfo->stopRun) {
        goto freeTree;
    }

    writeCount[1] = (inIsGridded) ? 1 : 0;

    for (yIndex = 0UL; yIndex < yDomSize; yIndex++) {
        queryCoords[0] = domYCoords[yIndex];
        syWritePos[0] = xWritePos[0] = yIndex;

        for (xIndex = 0UL; xIndex < xDomSize; xIndex++) {
            queryCoords[1] =
                (inPrimCRSIsGeo) ?
                    fmod(180.0 + domXCoords[xIndex], 360.0) - 180.0 :
                    domXCoords[xIndex];

            bestDist = DBL_MAX;
            nearNeighbor = NULL;

            if (siteDom) {
                queryCoords[0] = domYCoords[xIndex];
                xWritePos[0] = syWritePos[0] = xIndex;
            } else {
                xWritePos[1] = syWritePos[1] = xIndex;
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
                    &indexVarIDs[0],
                    templateID,
                    indexVarName[0],
                    &nearNeighbor->indices[0],
                    syWritePos,
                    writeCount,
                    "unsigned int",
                    LogInfo
                );
                if (LogInfo->stopRun) {
                    goto freeTree;
                }

                if (inIsGridded) {
                    SW_NC_write_vals(
                        &indexVarIDs[1],
                        templateID,
                        indexVarName[1],
                        &nearNeighbor->indices[1],
                        xWritePos,
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
    int inFileID,
    int *nDims,
    int templateID,
    char *domYName,
    char *domXName,
    int dimIDs[][2],
    Bool inHasSite,
    char *siteName,
    char *indexVarNames[],
    char *domName,
    int *numVars,
    LOG_INFO *LogInfo
) {
    int varNum;
    char *varNames[] = {domYName, domXName};
    char *varName;

    if (inHasSite && !SW_NC_dimExists(siteName, inFileID)) {
        LogError(
            LogInfo,
            LOGERROR,
            "Input spreadsheet claims to have site variable '%s' "
            "but it is not seen in the input nc file itself.",
            siteName
        );
    }

    indexVarNames[0] = (inHasSite) ? (char *) "site_index" : (char *) "y_index";
    indexVarNames[1] = (inHasSite) ? (char *) "" : (char *) "x_index";
    *numVars = (inHasSite) ? 1 : 2;

    for (varNum = 0; varNum < *numVars; varNum++) {
        varName = (inHasSite) ? varNames[varNum] : domName;

        SW_NC_get_vardimids(
            templateID, -1, varName, dimIDs[varNum], nDims, LogInfo
        );
        if (LogInfo->stopRun) {
            return;
        }
    }
}

/**
@brief Get attribute values of any type

@param[in] ncFileID File identifier to get value(s) from
@param[in] varID Variable identifier to get value(s) from
@param[in] attName Attribute name to get the value(s) of
@param[out] vals List of read-in values from the variable's attribute
@param[out] LogInfo Holds information on warnings and errors
*/
static void get_att_vals(
    int ncFileID, int varID, const char *attName, void *vals, LOG_INFO *LogInfo
) {
    if (nc_get_att(ncFileID, varID, attName, vals) != NC_NOERR) {
        LogError(
            LogInfo,
            LOGERROR,
            "Could not get values from attribute '%s'.",
            attName
        );
    }
}

/**
@brief Compare a user-provided input file against the program-generated/
user-provided index file to make sure the following criteria matches:
    * (Auxilary) Spatial coordinate variable names
    * Certain attributes of the CRS that we should expect

@param[in] inVarInfo List of lists that holds variable information for the
varible being checked within the given nc file
@param[in] indexFileID Index file identifier
@param[in] testFileID Testing file (input) identifier
@param[in] numSpatialVars The number of spatial variables to test for
@param[in] indexCRSName The name of the CRS variable within the index file
@param[in] testCRSName The name of the CRS variable within the input file
@param[in,out] LogInfo Holds information on warnings and errors
*/
static void check_input_file_against_index(
    char **inVarInfo,
    int indexFileID,
    int testFileID,
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
    char unitsAtt[MAX_FILENAMESIZE];
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
                        get_att_vals(
                            fileIDs[index],
                            *varIDs[index],
                            crsAttNames[att],
                            (void *) doubleVals[index],
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

    /* Check input variable units */
    SW_NC_get_str_att_val(
        testFileID, inVarInfo[INNCVARNAME], "units", unitsAtt, LogInfo
    );
    if (LogInfo->stopRun) {
        return;
    }

    if (strcmp(inVarInfo[INVARUNITS], "NA") != 0) {
        invalid_conv(inVarInfo[INVARUNITS], unitsAtt, LogInfo);
    }
}

#if !defined(SWMPI)
/*
@brief Helper function to set the first one or two dimensional
start indices to read inputs from, this mainly comes into play
when dealing with an index file

@param[in] useIndexFile Flag specifying if the current input key
must use the respective index file
@param[in] indexFileName Name of the respective index file to use
@param[in] inSiteDom Flag specifying if the input variable has a domain
of sites or is gridded
@param[in] ncSUID Current simulation unit identifier for which is used
to get data from netCDF
@param[out] start Start indices to figure out and write to
@param[out] LogInfo Holds information on warnings and errors
*/
static void get_read_start(
    Bool useIndexFile,
    char *indexFileName,
    Bool inSiteDom,
    const size_t ncSUID[],
    size_t start[],
    LOG_INFO *LogInfo
) {
    char *indexVarNames[] = {NULL, NULL};
    int indexFileID = -1;
    int varNum;
    int indexVarID;
    int numIndexVars = (inSiteDom) ? 1 : 2;

    /* Get the closest site from the index file if needed */
    if (useIndexFile) {
        indexVarNames[0] =
            (inSiteDom) ? (char *) "site_index" : (char *) "y_index";
        indexVarNames[1] = (inSiteDom) ? (char *) "" : (char *) "x_index";

        SW_NC_open(indexFileName, NC_NOWRITE, &indexFileID, LogInfo);
        if (LogInfo->stopRun) {
            return;
        }

        for (varNum = 0; varNum < numIndexVars; varNum++) {
            indexVarID = -1;

            SW_NC_get_single_val(
                indexFileID,
                &indexVarID,
                indexVarNames[varNum],
                (inSiteDom) ? &ncSUID[0] : ncSUID,
                &start[varNum],
                LogInfo
            );
            if (LogInfo->stopRun) {
                goto closeFile;
            }
        }
    } else {
        start[0] = ncSUID[0];
        start[1] = ncSUID[1]; /* May not be used */
    }

closeFile:
    if (indexFileID > -1) {
        nc_close(indexFileID);
    }
}
#endif

/**
@brief Once we read-in values from input, we need to check if any values
are missing and set them to SW_MISSING, that's what this function does,
this function is called on one value at a time; this function also
does converting from user-provided units to SW2 units

@param[in] varType Type of the value that is being tested
@param[in] valHasMissing A list of flags that specify the methods
of determining a missing value that was provided by the user
@param[in] missingVals If there are any methods `valHasMissing` provided
by the user, this is a list of missing value(s) to compare the value
to and see if it is missing
@param[in] varNum Variable number within an input key that is in question
@param[in,out] value Updated value after either setting it to missing
or converted value
*/
static void set_missing_val(
    nc_type varType,
    const Bool *valHasMissing,
    double **missingVals,
    int varNum,
    double *value
) {
    const int missVal = 1;
    const int fillVal = 2;
    const int validMax = 3;
    const int validMin = 4;
    const int validRange = 5;
    double ncMissVal;
    double *ncMissValArr;
    Bool setMissing = swFALSE;

    if (valHasMissing[0] && !isnull(missingVals)) {
        ncMissValArr = missingVals[varNum];

        setMissing =
            (Bool) (((valHasMissing[missVal] || valHasMissing[fillVal]) &&
                     EQ(*value, ncMissValArr[0])) ||

                    (((valHasMissing[validMax] && valHasMissing[validMin]) ||
                      valHasMissing[validRange]) &&
                     (LT(*value, ncMissValArr[0]) || GT(*value, ncMissValArr[1])
                     )));
    } else {
        switch (varType) {
        case NC_BYTE:
            ncMissVal = (double) (NC_FILL_BYTE);
            break;
        case NC_SHORT:
            ncMissVal = (double) (NC_FILL_SHORT);
            break;
        case NC_INT:
            ncMissVal = (double) (NC_FILL_INT);
            break;
        case NC_FLOAT:
            ncMissVal = (double) (NC_FILL_FLOAT);
            break;
        case NC_DOUBLE:
            ncMissVal = NC_FILL_DOUBLE;
            break;
        case NC_UBYTE:
            ncMissVal = (double) (NC_FILL_UBYTE);
            break;
        case NC_USHORT:
            ncMissVal = (double) (NC_FILL_USHORT);
            break;
        default: /* NC_UINT */
            ncMissVal = (double) (NC_FILL_UINT);
            break;
        }

        setMissing = (Bool) EQ(*value, ncMissVal);
    }
    if (setMissing) {
        *value = SW_MISSING;
    }
}

/**
@brief When reading in values from an nc file, we cannot expect them
to be the same type of NC_DOUBLE, so this function makes use of
a provided void pointer and converts the read value(s) from
the provided type to double (helper) and if we are to "unpack" the values,
this function also does that calculation given a value of
"scale_factor" and "add_offset"

@param[in] valHasMissing A list of flags specifying what type(s)
(if any) of methods the user provided for specifying how to
detect a missing input value
@param[in] missingVals Value(s) that specify when to determine
if a read-in value is missing (can be a single value or a range)
@param[in] readVals Temporary list of read-in double values to
transfer to `resVals`
@param[in] numVals Number of values that were read in to transfer from
one array to another
@param[in] varNum Specifies the variable number within an input key
to access the missing value information if allocated
@param[in] varType Type of the variable that was read-in and will
be set with those value(s)
@param[in] scale_factor User-provided factor to multiply the read-in
values to "unpack" the value into a float/double (may be 0 if the
variable is not to be unpacked)
@param[in] add_offset User-provided offset to add to the product of
the read-in value(s) and scale_factor to "unpack" the value(s)
(may be 0 if the variable is not to be unpacked)
@param[in] unitConv Unit converter for the current variable that
we read in
@param[in] swrcpInput A flag specifying if the read values are swrcp; this
variable's value setting needs to be handled in a different manner
@param[in] swrcpIndex A value specifying the number of swrcp we are dealing
with, [1, SWRC_PARAM_NMAX] or [0, SWRC_PARAM_NMAX - 1] for this index
@param[in] swrcpLyr A value specifying the layer we are setting the values
of within swrcp
@param[in,out] resVals Resulting values which the actual destination
within a struct used within a simulation run, and values are scaled/set to
missing as needed
*/
static void set_read_vals(
    const Bool *valHasMissing,
    double **missingVals,
    const double *readVals,
    int numVals,
    int varNum,
    nc_type varType,
    double scale_factor,
    double add_offset,
    sw_converter_t *unitConv,
    Bool swrcpInput,
    int swrcpIndex,
    LyrIndex swrcpLyr,
    double *resVals
) {
    int valIndex;
    double *dest;
    Bool missingBefore;
    double readVal;

    for (valIndex = 0; valIndex < numVals; valIndex++) {
        dest = (!swrcpInput) ? &resVals[valIndex] : &resVals[swrcpIndex];

        missingBefore = (Bool) (missing(readVals[valIndex]));
        readVal = (!swrcpInput) ? readVals[valIndex] : readVals[swrcpLyr];
        set_missing_val(varType, valHasMissing, missingVals, varNum, &readVal);

        if (missingBefore || !missing(readVal)) {
            *dest = readVal;
            *dest *= scale_factor;
            *dest += add_offset;

#if defined(SWUDUNITS)
            if (!isnull(unitConv)) {
                *dest = cv_convert_double(unitConv, *dest);
            }
#endif
        } else {
            *dest = SW_MISSING;
        }
    }
}

/**
@brief Condensed function to read topographical, spatial,
and climate inputs and convert the units from input nc files,
rather than having separate functions, this will specifically read
    - Latitude/longitude                             (inSpatial)
    - Elevation, slope, and aspect                   (inTopo)
    - Cloud cover, wind speed, relative humidity,
      snow density, and number of days with rain     (inClimate)
    - Tsoil_constant                                 (inSite)

@important This function handles both defines SWNETCDF without
    SWMPI and SWNETCDF with SWMPI

@param[in] SW_Domain Struct of type SW_DOMAIN holding constant
temporal/spatial information for a set of simulation runs
@param[in] numInputs The number inputs we are attempting to fill
@param[in] numReads A list of size SW_NINKEYSNC holding how many
    contiguous reads it will take to read all the input for the specified
    input SUIDs
@param[in] inFiles List of all input files throughout all input keys
@param[in] ncSUID Current simulation unit identifier for which is used
to get data from netCDF
@param[in] starts A list of size SW_NINKEYSNC storing calculated
    start indices for netCDFs to read contiguous data
@param[in] counts A list of size SW_NINKEYSNC storing calculated
    count sizes for netCDFs to read contiguous data; placement of
    these sizes match those of `starts`
@param[in] convs List of all conversion systems throughout all
input keys
@param[in] tempVals An allocated space to store temporary input values
    for converting and setting into proper location
@param[in] openNCFileIDs A list of open netCDF file identifiers
@param[out] inputs A list of structs (SWMPI) or singlular struct
    (not SWMPI) of the type SW_RUN_INPUTS that will be filled with input
    to be distributed to compute processes later
@param[out] LogInfo Holds information on warnings and errors
*/
static void read_spatial_topo_climate_site_inputs(
    SW_DOMAIN *SW_Domain,
    int numInputs,
    int numReads[],
    char ***inFiles,
    const size_t ncSUID[],
    size_t **starts[],
    size_t **counts[],
    sw_converter_t ***convs,
    double tempVals[],
    int **openNCFileIDs[],
    SW_RUN_INPUTS *inputs,
    LOG_INFO *LogInfo
) {
    char ***inVarInfo;
    Bool *readInput;
    Bool useIndexFile;
    Bool sDom;

    size_t count[] = {0, 0, 0};
    size_t start[] = {0, 0, 0};
    Bool *keyAttFlags;
    Bool varHasAddScaleAtts;
    int varNum;
    int adjVarNum;
    int keyNum;
    int fIndex;
    int varID;
    int ncFileID = -1;
    char *varName;
    nc_type *varTypes;
    nc_type varType;
    const int numKeys = 4;
    int *varIDs;
    double scaleFactor;
    double addOffset;
    Bool **missValFlags;
    double **doubleMissVals;
    int **dimOrderInVar;
    int numVals;
    int latIndex;
    int lonIndex;
    int timeIndex;
    size_t defSetStart[2] = {0};
    size_t defSetCount[2] = {1, 1};
    Bool *sDoms = SW_Domain->netCDFInput.siteDoms;
#if !defined(SWMPI)
    char *fileName;
#endif

    double **scaleAddFactors;
    const InKeys keys[] = {
        eSW_InSpatial, eSW_InTopo, eSW_InClimate, eSW_InSite
    };
    InKeys currKey;
    int read;
    int site;
    int numSites = 1;
    int tempRead;
    int input;
    int inputOrigin;

    for (keyNum = 0; keyNum < numKeys; keyNum++) {
        currKey = keys[keyNum];
        inVarInfo = SW_Domain->netCDFInput.inVarInfo[currKey];
        readInput = SW_Domain->netCDFInput.readInVars[currKey];
        fIndex = 1;
        numVals = (currKey == eSW_InClimate) ? MAX_MONTHS : 1;

        if (!readInput[0]) {
            continue;
        }

        while (!readInput[fIndex + 1]) {
            fIndex++;
        }

        varIDs = SW_Domain->SW_PathInputs.inVarIDs[currKey];
        varTypes = SW_Domain->SW_PathInputs.inVarTypes[currKey];
        keyAttFlags = SW_Domain->SW_PathInputs.hasScaleAndAddFact[currKey];
        scaleAddFactors = SW_Domain->SW_PathInputs.scaleAndAddFactVals[currKey];
        missValFlags = SW_Domain->SW_PathInputs.missValFlags[currKey];
        doubleMissVals = SW_Domain->SW_PathInputs.doubleMissVals[currKey];
        dimOrderInVar = SW_Domain->netCDFInput.dimOrderInVar[currKey];
        start[0] = start[1] = start[2] = 0;
        count[0] = count[1] = count[2] = 0;

        sDom = sDoms[currKey];
#if !defined(SWMPI)
        useIndexFile = SW_Domain->netCDFInput.useIndexFile[currKey];

        /* Get the start indices based on if we need to use the respective
           index file */
        get_read_start(
            useIndexFile,
            inFiles[currKey][0],
            sDom,
            ncSUID,
            defSetStart,
            LogInfo
        );
        if (LogInfo->stopRun) {
            goto closeFile;
        }
#endif

        input = inputOrigin = 0;
        for (read = 0; read < numReads[keyNum]; read++) {
#if defined(SWMPI)
            defSetStart[0] = starts[currKey][read][0];
            defSetStart[1] = starts[currKey][read][1];

            defSetCount[0] = counts[currKey][read][0];
            defSetCount[1] = counts[currKey][read][1];
#endif

            numSites = (sDom) ? defSetCount[1] : defSetCount[0];

            for (varNum = fIndex; varNum < numVarsInKey[currKey]; varNum++) {
                adjVarNum = varNum + 1;
                if (!readInput[adjVarNum]) {
                    continue;
                }

#if !defined(SWMPI)
                fileName = inFiles[currKey][varNum];
#endif

                varID = varIDs[varNum];
                varType = varTypes[varNum];
                varName = inVarInfo[varNum][INNCVARNAME];
                varHasAddScaleAtts = keyAttFlags[varNum];
                latIndex = dimOrderInVar[varNum][0];
                lonIndex = dimOrderInVar[varNum][1];
                timeIndex = dimOrderInVar[varNum][3];

                /* Make sure longitude is handled properly by putting the
                   start/count into the first slot instead of the second,
                   which would result resulting in a segmentation fault
                   (latitide index = -1) or in the case where longitude
                   is 2D, treat it normally */
                if ((currKey != eSW_InSpatial ||
                     (currKey == eSW_InSpatial && varNum == eiv_latitude)) ||
                    (currKey == eSW_InSpatial && varNum == eiv_longitude &&
                     latIndex > -1)) {

                    start[latIndex] = defSetStart[0];
                    count[latIndex] = defSetCount[0];

                    if (lonIndex > -1) {
                        start[lonIndex] = defSetStart[1];
                        count[lonIndex] = defSetCount[1];
                    }
                } else {
                    start[lonIndex] = defSetStart[1];
                    count[lonIndex] = defSetCount[1];
                }

                /* Determine how many values we will be reading from the
                variables within this input key */
                if (timeIndex > -1) {
                    count[timeIndex] = MAX_MONTHS;
                }

#if defined(SWMPI)
                ncFileID = openNCFileIDs[currKey][varNum][0];
#else
                SW_NC_open(fileName, NC_NOWRITE, &ncFileID, LogInfo);
                if (LogInfo->stopRun) {
                    return;
                }
#endif

                if (varType == NC_CHAR || varType > NC_UINT) {
                    LogError(
                        LogInfo,
                        LOGERROR,
                        "Cannot understand types of variable '%s' other than "
                        "float and double for unpacked values or byte, "
                        "unsigned "
                        "byte, short, unsigned short, integer, or unsigned "
                        "integer "
                        "for packed values.",
                        varName
                    );
#if defined(SWMPI)
                    return;
#else
                    goto closeFile;
#endif
                }

                get_values_multiple(
                    ncFileID, varID, start, count, varName, tempVals, LogInfo
                );
                if (LogInfo->stopRun) {
                    return; // Exit function prematurely due to error
                }

                if (varHasAddScaleAtts) {
                    scaleFactor = scaleAddFactors[varNum][0];
                    addOffset = scaleAddFactors[varNum][1];
                } else {
                    scaleFactor = 1.0;
                    addOffset = 0.0;
                }

                for (site = 0; site < numSites; site++) {
                    double *values[][5] = {
                        /* must match possVarNames[eSW_InSpatial] */
                        {&inputs[input].ModelRunIn.latitude,
                         &inputs[input].ModelRunIn.longitude},
                        /* must match possVarNames[eSW_InTopo]
                           (without spatial index) */
                        {&inputs[input].ModelRunIn.elevation,
                         &inputs[input].ModelRunIn.slope,
                         &inputs[input].ModelRunIn.aspect},
                        /* must match possVarNames[eSW_InClimate]
                           (without spatial index) */
                        {inputs[input].SkyRunIn.cloudcov,
                         inputs[input].SkyRunIn.windspeed,
                         inputs[input].SkyRunIn.r_humidity,
                         inputs[input].SkyRunIn.snow_density,
                         inputs[input].SkyRunIn.n_rain_per_day},
                        {&inputs[input].SiteRunIn.Tsoil_constant}
                    };

                    tempRead = site * numVals;

                    set_read_vals(
                        missValFlags[varNum],
                        doubleMissVals,
                        &tempVals[tempRead],
                        numVals,
                        varNum,
                        varType,
                        scaleFactor,
                        addOffset,
                        convs[currKey][varNum],
                        swFALSE,
                        0,
                        0,
                        values[keyNum][varNum - 1]
                    );

                    input++;
                }
                input = inputOrigin;

#if !defined(SWMPI)
                nc_close(ncFileID);
                ncFileID = -1;
#endif
            }

            input += numSites;
            inputOrigin = input;
        }
    }

    for (input = 0; input < numInputs; input++) {
        inputs[input].ModelRunIn.isnorth =
            (Bool) (GT(inputs[input].ModelRunIn.latitude, 0.0));
    }

#if !defined(SWMPI)
closeFile:
    if (ncFileID > -1) {
        nc_close(ncFileID);
    }

    (void) starts;
    (void) counts;
    (void) openNCFileIDs;
#else
    (void) inFiles;
    (void) ncSUID;
    (void) useIndexFile;
    (void) sDom;
#endif
}

/**
@brief Read an attribute value(s) to specify a way to detect
a missing value when reading inputs later programs before running
simulations

@param[in] ncFileID Identifier of the file that contains the variable
with the attribute to read
@param[in] varID Identifier of the variable that has the attribute to read
@param[in] varNum Variable number within an input key
@param[in] attName Name of the attribute that will be read
@param[in] attType Type of the attribute to give proper storage
to
@param[out] doubleMissValsRes Array of two values for every
variable within an input key to specify missing values
@param[out] LogInfo Holds information dealing with logfile output
*/
static void read_miss_vals(
    int ncFileID,
    int varID,
    int varNum,
    char *attName,
    nc_type attType,
    double **doubleMissValsRes,
    LOG_INFO *LogInfo
) {
    int typeIndex = (attType != NC_BYTE) ? attType - 2 : attType - 1;
    char byteMissVals[] = {(char) 0, (char) 0};
    short shortMissVals[] = {0, 0};
    int intMissVals[] = {0, 0};
    float floatMissVals[] = {0.0f, 0.0f};
    double doubleMissVals[] = {0.0, 0.0};
    unsigned char uByteMissVals[] = {0, 0};
    unsigned short uShortMissVals[] = {0, 0};
    unsigned int uIntMissVals[] = {0, 0};
    void *valPtrs[] = {
        (void *) byteMissVals,
        (void *) shortMissVals,
        (void *) intMissVals,
        (void *) floatMissVals,
        (void *) doubleMissVals,
        (void *) uByteMissVals,
        (void *) uShortMissVals,
        (void *) uIntMissVals
    };
    void *valPtr = valPtrs[typeIndex];
    double tempMaxMissVal = SW_MISSING;

    if (attType < NC_BYTE || attType == NC_CHAR || attType > NC_DOUBLE) {
        LogError(
            LogInfo,
            LOGERROR,
            "Retrieved a type of attribute that is not supported. The "
            "attribute types for missing value specifiers ('missing_value', "
            "'range_max', 'range_min', 'valid_range', '_FillValue') are "
            "byte, ubyte, short, ushort, int, uint, float, and double."
        );
        return;
    }

    get_att_vals(ncFileID, varID, attName, valPtr, LogInfo);

    if (strcmp(attName, "valid_min") == 0) {
        tempMaxMissVal = doubleMissValsRes[varNum][0];
    }

    switch (attType) {
    case NC_BYTE:
        doubleMissValsRes[varNum][0] = (double) byteMissVals[0];
        doubleMissValsRes[varNum][1] = (double) byteMissVals[1];
        break;
    case NC_SHORT:
        doubleMissValsRes[varNum][0] = (double) shortMissVals[0];
        doubleMissValsRes[varNum][1] = (double) shortMissVals[1];
        break;
    case NC_INT:
        doubleMissValsRes[varNum][0] = (double) intMissVals[0];
        doubleMissValsRes[varNum][1] = (double) intMissVals[1];
        break;
    case NC_FLOAT:
        doubleMissValsRes[varNum][0] = (double) floatMissVals[0];
        doubleMissValsRes[varNum][1] = (double) floatMissVals[1];
        break;
    case NC_DOUBLE:
        doubleMissValsRes[varNum][0] = doubleMissVals[0];
        doubleMissValsRes[varNum][1] = doubleMissVals[1];
        break;
    case NC_UBYTE:
        doubleMissValsRes[varNum][0] = (double) uByteMissVals[0];
        doubleMissValsRes[varNum][1] = (double) uByteMissVals[1];
        break;
    case NC_USHORT:
        doubleMissValsRes[varNum][0] = (double) uShortMissVals[0];
        doubleMissValsRes[varNum][1] = (double) uShortMissVals[1];
        break;
    default: /* NC_UINT */
        doubleMissValsRes[varNum][0] = (double) uIntMissVals[0];
        doubleMissValsRes[varNum][1] = (double) uIntMissVals[1];
        break;
    }

    /* Set the variable max/min to be [min, max] in `doubleMissValsRes */
    if (strcmp(attName, "valid_min") == 0) {
        doubleMissValsRes[varNum][1] = tempMaxMissVal;
    }
}

/**
@brief Gather values (if any) that will specify how to know values
are missing when reading input through nc files

@param[in] ncFileID Identifier of the file that contains the variable
with the attribute to read
@param[in] varID Identifier of the variable that has the attribute to read
@param[in] varNum Variable number within an input key
@param[in] inKey Current input key we are gathering missing information for
@param[out] missValFlags A list of flags for every variable of the input key
that specify the attribute-provided missing values
@param[out] doubleMissVals A list of values for every variable of the input key
that specify if a read-in value from nc files is missing
@param[out] LogInfo Holds information dealing with logfile output
*/
static void gather_missing_information(
    int ncFileID,
    int varID,
    int varNum,
    int inKey,
    Bool **missValFlags,
    double ***doubleMissVals,
    LOG_INFO *LogInfo
) {
    const int numMissAtts = 5;
    const char *missAttNames[] = {
        (char *) "missing_value",
        (char *) "_FillValue",
        (char *) "valid_max",
        (char *) "valid_min",
        (char *) "valid_range"
    };

    int attNum;
    Bool *hasMissFlag;
    size_t attSize;
    nc_type missAttType;

    for (attNum = 0; attNum < numMissAtts; attNum++) {
        hasMissFlag = &missValFlags[varNum][attNum + 1];

        att_exists(
            ncFileID,
            varID,
            (char *) missAttNames[attNum],
            &attSize,
            hasMissFlag,
            LogInfo
        );
        if (LogInfo->stopRun) {
            return;
        }

        if (*hasMissFlag) {
            missValFlags[varNum][0] = swTRUE;

            get_att_type(
                ncFileID,
                varID,
                (char *) missAttNames[attNum],
                &missAttType,
                LogInfo
            );
            if (LogInfo->stopRun) {
                return;
            }

            SW_NCIN_alloc_miss_vals(
                numVarsInKey[inKey], doubleMissVals, LogInfo
            );
            if (LogInfo->stopRun) {
                return;
            }

            read_miss_vals(
                ncFileID,
                varID,
                varNum,
                (char *) missAttNames[attNum],
                missAttType,
                *doubleMissVals,
                LogInfo
            );
            if (LogInfo->stopRun) {
                return;
            }
        }
    }
}

/**
@brief Understand the ordering of a variable's dimensions to use
later for understanding how to read information from input files

@param[in] ncFileID File identifier of the nc file being read
@param[in] varID Identifier of the nc variable to read
@param[in] varInfo Variable information for the variable we are
gathering dimension information from
@param[out] indices A list of indices that specify the order of the
dimensions in the variable header
@param[out] LogInfo Holds information dealing with logfile output
*/
static void get_variable_dim_order(
    int ncFileID, int varID, char **varInfo, int *indices, LOG_INFO *LogInfo
) {
    int axisNum;
    int orderIndex = 0;
    const int maxNumDims = 5;
    int axisID;
    int readAxisID = 0;
    Bool varSiteDom = (Bool) (strcmp(varInfo[INDOMTYPE], "s") == 0);
    char *yDim = (strcmp(varInfo[INYDIM], "NA") == 0) ? varInfo[INYAXIS] :
                                                        varInfo[INYDIM];
    char *xDim = (strcmp(varInfo[INXDIM], "NA") == 0) ? varInfo[INXAXIS] :
                                                        varInfo[INXDIM];
    char *axisNames[] = {
        (varSiteDom) ? varInfo[INSITENAME] : yDim,
        xDim,
        varInfo[INZAXIS],
        varInfo[INTAXIS],
        varInfo[INVAXIS]
    };
    int varDimIndex = 0;
    int dimIDs[] = {-1, -1, -1, -1, -1};
    int readVarDimIDs[] = {-1, -1, -1, -1, -1};
    Bool hasDim;

    /* Get the global dimension information (IDs if they exist) */
    for (axisNum = 0; axisNum < maxNumDims; axisNum++) {
        hasDim = SW_NC_dimExists(axisNames[axisNum], ncFileID);

        if (hasDim) {
            SW_NC_get_dim_identifier(
                ncFileID, axisNames[axisNum], &dimIDs[axisNum], LogInfo
            );
            if (LogInfo->stopRun) {
                return;
            }
        }
    }

    /* Get a list of dimension IDs from the current variable
       and compare that to the order of `axisNames` to specify
       what index a dimension should be placed when reading
       values from the variable, see `dimOrderInVar` within SW_NETCDF_IN
       for more information */
    if (nc_inq_vardimid(ncFileID, varID, readVarDimIDs) != NC_NOERR) {
        LogError(
            LogInfo,
            LOGERROR,
            "Could not get dimension identifiers of variable from inputs."
        );
    }

    for (axisNum = 0; axisNum < maxNumDims; axisNum++) {
        axisID = dimIDs[axisNum];

        if (axisID > -1) {
            for (varDimIndex = 0; varDimIndex < maxNumDims; varDimIndex++) {
                readAxisID = readVarDimIDs[varDimIndex];

                if (readAxisID == axisID) {
                    indices[orderIndex] = varDimIndex;
                }
            }
        }

        orderIndex++;
    }
}

#if defined(SWNETCDF) && defined(SWUDUNITS)
/**
@brief Read the units of coordinate variables to convert from later
(if different than "m")

@param[in] ncFileID File identifier of the nc file being read
@param[in] yVarName Name of the y coordinate variable
@param[in] xVarName Name of the x coordinate variable
@param[out] varConv A list of two unit converters for the coordinate variables
(one for y and one for x)
@param[out] LogInfo Holds information dealing with logfile output
*/
static void get_proj_nc_units(
    int ncFileID,
    char *yVarName,
    char *xVarName,
    sw_converter_t *varConv[],
    LOG_INFO *LogInfo
) {
    int varNum;
    const int numVars = 2;
    char *varNames[] = {yVarName, xVarName};
    char varUnit[FILENAME_MAX] = {'\0'};
    const char *attName = "units";

    ut_system *system = NULL;
    ut_unit *unitFrom;
    ut_unit *unitTo;
    Bool convertible;

    /* silence udunits2 error messages */
    ut_set_error_message_handler(ut_ignore);

    /* Load unit system database */
    system = ut_read_xml(NULL);

    unitTo = ut_parse(system, "m", UT_UTF8);

    /* Get information about the projected variables so we
       can gather the attributes */
    for (varNum = 0; varNum < numVars; varNum++) {
        if (SW_NC_varExists(ncFileID, varNames[varNum])) {
            SW_NC_get_str_att_val(
                ncFileID, varNames[varNum], attName, varUnit, LogInfo
            );
            if (LogInfo->stopRun) {
                return;
            }

            unitFrom = ut_parse(system, varUnit, UT_UTF8);
            convertible = (Bool) (ut_are_convertible(unitFrom, unitTo) != 0);

            if (convertible) {
                varConv[varNum] = ut_get_converter(unitFrom, unitTo);
            } else {
                LogError(
                    LogInfo,
                    LOGWARN,
                    "The coordinate variable '%s' is of a unit that is not "
                    "convertible from '%s'. The unit '%s' will be used.",
                    varNames[varNum],
                    "m",
                    varUnit
                );
                return;
            }

            ut_free(unitFrom);
        }
    }

    ut_free(unitTo);
    ut_free_system(system);
}
#endif /* SWNETCDF & SWUDUNITS */

/**
@brief Before reading inputs, it is best to get certain information
to not have the need to query the information during the simulations
and being read many times; the information this function gathers is:
    - Variable identifiers
    - Variable types
    - Flags/values for having scale and add factor attribute per variable
    - Missing value specifiers
    - Size of the vertical dimension (number of soil layers) of each soil
      input

@param[in] SW_netCDFIn Constant netCDF input file information
@param[out] SW_PathInputs Struct of type SW_PATH_INPUTS which
holds basic information about input files and values
@param[out] LogInfo Holds information dealing with logfile output
*/
static void get_invar_information(
    SW_NETCDF_IN *SW_netCDFIn, SW_PATH_INPUTS *SW_PathInputs, LOG_INFO *LogInfo
) {
    int inKey;
    int attNum;
    const int numUnpackAtts = 2;
    int *varID;
    int ncFileID = -1;
    int varNum;
    char ***inVarInfo;
    Bool **readInVars = SW_netCDFIn->readInVars;
    char **ncInFiles;
    char *fileName;
    Bool *hasScaleAddAtts;
    char *varName;
    nc_type *varType;
    nc_type attType;
    size_t attSize = 0; /* Not used */
    Bool scaleAttExists = swFALSE;
    Bool addAttExists = swFALSE;
    int numScaleAddAtts;
    const int errorNumScaleAddAtt = 1;
    int startVar;
    Bool **missValFlags;
    size_t **numSoilVarLyrs = &SW_PathInputs->numSoilVarLyrs;
    unsigned int weathFileIndex = SW_PathInputs->weathStartFileIndex;
    Bool projCRS;

    double *attVal;

    Bool *attFlags[] = {&scaleAttExists, &addAttExists};
    char *unpackAttNames[] = {(char *) "scale_factor", (char *) "add_offset"};

    ForEachNCInKey(inKey) {
        if (!readInVars[inKey][0] || inKey == eSW_InDomain) {
            if (inKey == eSW_InDomain) {
                inVarInfo = SW_netCDFIn->inVarInfo[eSW_InDomain];

                SW_netCDFIn->siteDoms[eSW_InDomain] =
                    (Bool) (strcmp(inVarInfo[0][INDOMTYPE], "s") == 0);
            }
            continue;
        }

        inVarInfo = SW_netCDFIn->inVarInfo[inKey];
        ncInFiles = SW_PathInputs->ncInFiles[inKey];
        startVar = 1;

        while (!readInVars[inKey][startVar + 1]) {
            startVar++;
        }

        projCRS =
            (Bool) (strcmp(
                        inVarInfo[startVar][INGRIDMAPPING], "latitude_longitude"
                    ) != 0);

        // Store flags if each input key has a site domain so this
        // information is not calculated repeatedly
        SW_netCDFIn->siteDoms[inKey] =
            (Bool) (strcmp(inVarInfo[startVar][INDOMTYPE], "s") == 0);

        SW_NCIN_alloc_sim_var_information(
            numVarsInKey[inKey],
            inKey,
            swTRUE,
            &SW_PathInputs->inVarIDs[inKey],
            &SW_PathInputs->inVarTypes[inKey],
            &SW_PathInputs->hasScaleAndAddFact[inKey],
            &SW_PathInputs->scaleAndAddFactVals[inKey],
            &SW_PathInputs->missValFlags[inKey],
            &SW_netCDFIn->dimOrderInVar[inKey],
            numSoilVarLyrs,
            LogInfo
        );
        if (LogInfo->stopRun) {
            return;
        }

        missValFlags = SW_PathInputs->missValFlags[inKey];

        for (varNum = startVar; varNum < numVarsInKey[inKey]; varNum++) {
            /* Check if this variable is not input by the user */
            if (!readInVars[inKey][varNum + 1]) {
                continue;
            }

            varID = &SW_PathInputs->inVarIDs[inKey][varNum];
            varName = inVarInfo[varNum][INNCVARNAME];
            varType = &SW_PathInputs->inVarTypes[inKey][varNum];
            hasScaleAddAtts = &SW_PathInputs->hasScaleAndAddFact[inKey][varNum];
            numScaleAddAtts = 0;

            if (inKey != eSW_InWeather) {
                fileName = ncInFiles[varNum];
            } else {
                fileName =
                    SW_PathInputs->ncWeatherInFiles[varNum][weathFileIndex];
            }

            /* Open file */
            SW_NC_open(fileName, NC_NOWRITE, &ncFileID, LogInfo);
            if (LogInfo->stopRun) {
                return;
            }

            /* Get the variable identifier */
            SW_NC_get_var_identifier(ncFileID, varName, varID, LogInfo);
            if (LogInfo->stopRun) {
                goto closeFile;
            }

            /* Get variable type */
            get_var_type(ncFileID, *varID, varName, varType, LogInfo);
            if (LogInfo->stopRun) {
                goto closeFile;
            }

            /* Get flags for if the variable has scale and add factors */
            for (attNum = 0; attNum < numUnpackAtts; attNum++) {
                att_exists(
                    ncFileID,
                    *varID,
                    unpackAttNames[attNum],
                    &attSize,
                    attFlags[attNum],
                    LogInfo
                );
                if (LogInfo->stopRun) {
                    goto closeFile;
                }

                numScaleAddAtts += (attFlags[attNum]) ? 1 : 0;
            }

            *hasScaleAddAtts = (Bool) (scaleAttExists && addAttExists);

            /* If both flags exist, store the values */
            if (*hasScaleAddAtts) {
                for (attNum = 0; attNum < numUnpackAtts; attNum++) {
                    attVal = &SW_PathInputs
                                  ->scaleAndAddFactVals[inKey][varNum][attNum];

                    get_att_type(
                        ncFileID,
                        *varID,
                        unpackAttNames[attNum],
                        &attType,
                        LogInfo
                    );
                    if (LogInfo->stopRun) {
                        goto closeFile;
                    }

                    if (nc_get_att_double(
                            ncFileID, *varID, unpackAttNames[attNum], attVal
                        ) != NC_NOERR) {
                        LogError(
                            LogInfo,
                            LOGERROR,
                            "Could not get the attribute value of '%s'.",
                            unpackAttNames[attNum]
                        );
                        goto closeFile;
                    }
                }
            }

            if (numScaleAddAtts == errorNumScaleAddAtt &&
                (*varType == NC_BYTE || *varType == NC_UBYTE ||
                 *varType == NC_SHORT || *varType == NC_USHORT ||
                 *varType == NC_INT || *varType == NC_UINT)) {

                LogError(
                    LogInfo,
                    LOGERROR,
                    "Detected a variable ('%s') which has one out of the "
                    "two attributes 'scale_factor' or 'add_offset'.",
                    varName
                );
                goto closeFile;
            }

            /* Get missing value information (if any) */
            gather_missing_information(
                ncFileID,
                *varID,
                varNum,
                inKey,
                missValFlags,
                &SW_PathInputs->doubleMissVals[inKey],
                LogInfo
            );
            if (LogInfo->stopRun) {
                goto closeFile;
            }

            get_variable_dim_order(
                ncFileID,
                *varID,
                inVarInfo[varNum],
                SW_netCDFIn->dimOrderInVar[inKey][varNum],
                LogInfo
            );
            if (LogInfo->stopRun) {
                goto closeFile;
            }

            if (inKey == eSW_InSoil) {
                SW_NC_get_dimlen_from_dimname(
                    ncFileID,
                    inVarInfo[varNum][INZAXIS],
                    &(*numSoilVarLyrs)[varNum],
                    LogInfo
                );
                if (LogInfo->stopRun) {
                    goto closeFile;
                }
            }

#if defined(SWNETCDF) && defined(SWUDUNITS)
            if (projCRS && varNum == startVar) {
                get_proj_nc_units(
                    ncFileID,
                    inVarInfo[varNum][INYAXIS],
                    inVarInfo[varNum][INXAXIS],
                    SW_netCDFIn->projCoordConvs[inKey],
                    LogInfo
                );
                if (LogInfo->stopRun) {
                    goto closeFile;
                }
            }
#endif

            nc_close(ncFileID);
            ncFileID = -1;
        }
    }

closeFile:
    if (ncFileID > -1) {
        nc_close(ncFileID);
    }
}

/**
@brief Read user-provided vegetation variable values

@important This function handles both defines SWNETCDF without
    SWMPI and SWNETCDF with SWMPI

@param[in] SW_Domain Struct of type SW_DOMAIN holding constant
temporal/spatial information for a set of simulation runs
@param[in] starts A list of size SW_NINKEYSNC storing calculated
    start indices for netCDFs to read contiguous data
@param[in] counts A list of size SW_NINKEYSNC storing calculated
    count sizes for netCDFs to read contiguous data; placement of
    these sizes match those of `starts`
@param[out] SW_VegProdIn Struct of type SW_VEGPROD_INPUTS describing surface
cover conditions in the simulation
@param[in] vegInFiles List of input files pertaining to the vegetation
input key
@param[in] numReads A list of size SW_NINKEYSNC holding how many
    contiguous reads it will take to read all the input for the specified
    input SUIDs
@param[in] ncSUID simulation unit identifier for which is used
to get data from netCDF
@param[in] vegConv A list of UDUNITS2 converters that were created
to convert input data to units the program can understand within the
"inVeg" input key
@param[in] vegFileIDs A list of open vegetation netCDF file identifiers
@param[in] tempVals An allocated space to store temporary input values
    for converting and setting into proper location
@param[out] inputs A list of structs of the type SW_RUN_INPUTS that
    will be filled with input to be distributed to compute processes later
@param[out] LogInfo Holds information on warnings and errors
*/
static void read_veg_inputs(
    SW_DOMAIN *SW_Domain,
    size_t **starts,
    size_t **counts,
    char **vegInFiles,
    int numReads,
    const size_t ncSUID[],
    sw_converter_t **vegConv,
    int **vegFileIDs,
    double *tempVals,
    SW_RUN_INPUTS *inputs,
    LOG_INFO *LogInfo
) {
    char ***inVarInfo = SW_Domain->netCDFInput.inVarInfo[eSW_InVeg];
    Bool *readInput = SW_Domain->netCDFInput.readInVars[eSW_InVeg];

    int varNum;
    int fIndex = 1;
    int varID = -1;
    int ncFileID = -1;
    char *varName;
    nc_type varType;
    size_t count[4] = {0};
    size_t start[4] = {0};
    Bool varHasNotTime;
    Bool hasPFT;
    Bool varHasAddScaleAtts;
    double scaleFactor;
    double addOffset;
    int **dimOrderInVar = SW_Domain->netCDFInput.dimOrderInVar[eSW_InVeg];
    Bool *keyAttFlags = SW_Domain->SW_PathInputs.hasScaleAndAddFact[eSW_InVeg];
    double **scaleAddFactors =
        SW_Domain->SW_PathInputs.scaleAndAddFactVals[eSW_InVeg];
    Bool **missValFlags = SW_Domain->SW_PathInputs.missValFlags[eSW_InVeg];
    double **doubleMissVals =
        SW_Domain->SW_PathInputs.doubleMissVals[eSW_InVeg];
    int *varIDs = SW_Domain->SW_PathInputs.inVarIDs[eSW_InVeg];
    nc_type *varTypes = SW_Domain->SW_PathInputs.inVarTypes[eSW_InVeg];
    int latIndex;
    int lonIndex;
    int timeIndex;
    int pftIndex;
    int k;
    size_t defSetStart[2] = {0};
    size_t defSetCount[2] = {1, 1};
    int read;
    int numSites = 1;
    int site;
    int writeIndex;
    int input = 0;
    int inputOrigin = 0;
    Bool sDom = SW_Domain->netCDFInput.siteDoms[eSW_InVeg];

#if !defined(SWMPI)
    char *fileName;
    char **inFiles = SW_Domain->SW_PathInputs.ncInFiles[eSW_InVeg];
    Bool useIndexFile = SW_Domain->netCDFInput.useIndexFile[eSW_InVeg];
#endif

    int numSetVals;

    while (!readInput[fIndex + 1]) {
        fIndex++;
    }

#if !defined(SWMPI)
    /* Get the start indices based on if we need to use the respective
        index file */
    get_read_start(
        useIndexFile, inFiles[0], sDom, ncSUID, defSetStart, LogInfo
    );
    if (LogInfo->stopRun) {
        goto closeFile;
    }
#endif

    for (read = 0; read < numReads; read++) {
#if defined(SWMPI)
        defSetStart[0] = starts[read][0];
        defSetStart[1] = starts[read][1];

        defSetCount[0] = counts[read][0];
        defSetCount[1] = counts[read][1];

        numSites = (sDom) ? defSetCount[0] : defSetCount[1];
#endif

        for (varNum = fIndex; varNum < numVarsInKey[eSW_InVeg]; varNum++) {
            if (!readInput[varNum + 1]) {
                continue;
            }

            /* Bare ground and vegetation cover do not have time,
               otherwise, the current variable has a time dimension */
            varHasNotTime = (Bool) (varNum == eiv_bareGroundfCover);

            ForEachVegType(k) {
                varHasNotTime =
                    (Bool) (varHasNotTime || varNum == eiv_vegfCover[k]);
            }

            varID = varIDs[varNum];
            varType = varTypes[varNum];
            varName = inVarInfo[varNum][INNCVARNAME];
            hasPFT = (Bool) (strcmp(inVarInfo[varNum][INVAXIS], "NA") != 0);
            numSetVals = (varHasNotTime) ? 1 : MAX_MONTHS;
            latIndex = dimOrderInVar[varNum][0];
            lonIndex = dimOrderInVar[varNum][1];
            timeIndex = dimOrderInVar[varNum][3];
            pftIndex = dimOrderInVar[varNum][4];

#if !defined(SWMPI)
            fileName = vegInFiles[varNum];
#endif

            start[0] = start[1] = start[2] = start[3] = 0;
            count[0] = count[1] = count[2] = count[3] = 0;
            start[latIndex] = defSetStart[0];
            count[latIndex] = defSetCount[0];

            if (lonIndex > -1) {
                start[lonIndex] = defSetStart[1];
                count[lonIndex] = defSetCount[1];
            }

            if (!varHasNotTime && timeIndex > -1) {
                count[timeIndex] = MAX_MONTHS;
            }
            if (hasPFT && pftIndex > -1) {
                start[pftIndex] = ((varNum - 2) / (NVEGTYPES + 1));
                count[pftIndex] = 1;
            }

#if defined(SWMPI)
            ncFileID = vegFileIDs[varNum][0];
#else
            SW_NC_open(fileName, NC_NOWRITE, &ncFileID, LogInfo);
            if (LogInfo->stopRun) {
                return;
            }
#endif

            varHasAddScaleAtts = keyAttFlags[varNum];

            if (varHasAddScaleAtts) {
                scaleFactor = scaleAddFactors[varNum][0];
                addOffset = scaleAddFactors[varNum][1];
            } else {
                scaleFactor = 1.0;
                addOffset = 0.0;
            }

            /* Read current vegetation input */
            get_values_multiple(
                ncFileID, varID, start, count, varName, tempVals, LogInfo
            );
            if (LogInfo->stopRun) {
                goto closeFile; // Exit function prematurely due to error
            }

            for (site = 0; site < numSites; site++) {
                /* must match possVarNames[eSW_InVeg] (without spatial index) */
                double *values[] = {
                    &inputs[input].VegProdRunIn.bare_cov.fCover,

                    &inputs[input].VegProdRunIn.veg[SW_TREES].cov.fCover,
                    inputs[input].VegProdRunIn.veg[SW_TREES].litter,
                    inputs[input].VegProdRunIn.veg[SW_TREES].biomass,
                    inputs[input].VegProdRunIn.veg[SW_TREES].pct_live,
                    inputs[input].VegProdRunIn.veg[SW_TREES].lai_conv,

                    &inputs[input].VegProdRunIn.veg[SW_SHRUB].cov.fCover,
                    inputs[input].VegProdRunIn.veg[SW_SHRUB].litter,
                    inputs[input].VegProdRunIn.veg[SW_SHRUB].biomass,
                    inputs[input].VegProdRunIn.veg[SW_SHRUB].pct_live,
                    inputs[input].VegProdRunIn.veg[SW_SHRUB].lai_conv,

                    &inputs[input].VegProdRunIn.veg[SW_FORBS].cov.fCover,
                    inputs[input].VegProdRunIn.veg[SW_FORBS].litter,
                    inputs[input].VegProdRunIn.veg[SW_FORBS].biomass,
                    inputs[input].VegProdRunIn.veg[SW_FORBS].pct_live,
                    inputs[input].VegProdRunIn.veg[SW_FORBS].lai_conv,

                    &inputs[input].VegProdRunIn.veg[SW_GRASS].cov.fCover,
                    inputs[input].VegProdRunIn.veg[SW_GRASS].litter,
                    inputs[input].VegProdRunIn.veg[SW_GRASS].biomass,
                    inputs[input].VegProdRunIn.veg[SW_GRASS].pct_live,
                    inputs[input].VegProdRunIn.veg[SW_GRASS].lai_conv
                };

                writeIndex = site * numSetVals;

                set_read_vals(
                    missValFlags[varNum],
                    doubleMissVals,
                    &tempVals[writeIndex],
                    numSetVals,
                    varNum - 1,
                    varType,
                    scaleFactor,
                    addOffset,
                    vegConv[varNum - 1],
                    swFALSE,
                    0,
                    0,
                    values[varNum - 1]
                );

                input++;
            }
            input = inputOrigin;

#if !defined(SWMPI)
            nc_close(ncFileID);
            ncFileID = -1;
#endif
        }

        input += numSites;
        inputOrigin = input;
    }

closeFile:
#if defined(SWMPI)
    (void) vegInFiles;
    (void) ncSUID;
#else
    if (ncFileID > -1) {
        nc_close(ncFileID);
    }
    (void) starts;
    (void) counts;
    (void) vegFileIDs;
#endif
}

/** Derive missing soil properties from available properties and checks

The number of soil layers is inferred from the first n layers
with depth and width/thickness (if provided as input)
that are neither zero nor missing.

If \p hasConstSoilDepths, then the function checks that depth values are
consistent with the default soil depths (from text input "soils.in").

Otherwise (i.e., if not \p hasConstSoilDepths), missing properties are
calculated if possible
    - depth if width/thickness is provided as nc-input but not depth
    - width/thickness is provided as nc-input but not width/thickness
    - sand if clay and silt are provided but not sand
    - clay if sand and silt are provided but not clay
    - impermeability is set to 0 if not provided as nc-input
    - initial soil temperature is set to 0 if not provided as nc-input

The function also checks for consistency
    - between depth and width (if both provided as inputs)
    - between sand, silt, and clay (if all provided as nc-inputs)


@param[out] n_layers Number of layers of soil within the simulation run
@param[in,out] soilValues Array of pointers to soil variable arrays,
    see read_soil_inputs(), organized as #possVarNames[eSW_InSoil]
    (but without spatial index, transpiration coefficients, and SWRCp)
@param[in] readInVarsSoils Specifies which variables are to be read-in as
    soil inputs
@param[in] hasConstSoilDepths Specifies of all soil inputs provided
    by the user (if any) have the same depth profile
@param[in] depthsAllSoilLayers Depths of soil layers (cm),
    used if \p hasConstSoilDepths
@param[in] nMaxSoilLayers Largest number of soil layers across
    simulation domain
@param[out] LogInfo Holds information on warnings and errors
*/
static void derive_missing_soils(
    LyrIndex *n_layers,
    SW_SOIL_RUN_INPUTS *soilIn,
    const Bool *readInVarsSoils,
    Bool hasConstSoilDepths,
    const double depthsAllSoilLayers[],
    LyrIndex nMaxSoilLayers,
    double tempSilt[],
    LOG_INFO *LogInfo
) {
    Bool noDepth;
    Bool noWidth;
    double cumWidth = 0.;
    double sumTexture;

    for (int slNum = 0; slNum < MAX_LAYERS; slNum++) {
        // Note: SW_SIT_init_run() will determine:
        //       n_evap_lyrs, n_transp_lyrs, deep_lyr
        // Here, determine new number of soil layers:
        //      soil layers end if depth or width is missing or 0
        noDepth =
            (hasConstSoilDepths || readInVarsSoils[eiv_soilLayerDepth + 1]) ?
                (Bool) (missing(soilIn->depths[slNum]) ||
                        ZRO(soilIn->depths[slNum])) :
                swFALSE;

        noWidth =
            (hasConstSoilDepths || readInVarsSoils[eiv_soilLayerWidth + 1]) ?
                (Bool) (missing(soilIn->width[slNum]) ||
                        ZRO(soilIn->width[slNum])) :
                swFALSE;

        if (noDepth || noWidth) {
            break;
        }

        (*n_layers)++;

        if (hasConstSoilDepths) {
            // Check that depth is consistent with depthsAllSoilLayers
            if (!EQ(soilIn->depths[slNum], depthsAllSoilLayers[slNum])) {
                LogError(
                    LogInfo,
                    LOGERROR,
                    "Depth (%f cm) of soil layer %d disagrees with "
                    "expected depth (%f cm).",
                    soilIn->depths[slNum],
                    slNum,
                    depthsAllSoilLayers[slNum]
                );
                return; // Exit function prematurely due to error
            }

        } else {
            // Calculate depth if width provided but not depth
            if (!readInVarsSoils[eiv_soilLayerDepth + 1] &&
                readInVarsSoils[eiv_soilLayerWidth + 1]) {
                if (slNum == 0) {
                    soilIn->depths[slNum] = soilIn->width[slNum];
                } else {
                    soilIn->depths[slNum] += soilIn->width[slNum];
                }
            }

            // Calculate width if depth provided but not width
            if (readInVarsSoils[eiv_soilLayerDepth + 1] &&
                !readInVarsSoils[eiv_soilLayerWidth + 1]) {
                if (slNum == 0) {
                    soilIn->width[slNum] = soilIn->depths[slNum];
                } else {
                    soilIn->width[slNum] =
                        soilIn->depths[slNum] - soilIn->depths[slNum - 1];
                }
            }

            // Calculate sand if clay and silt provided but not sand
            if (!readInVarsSoils[eiv_sand + 1] &&
                readInVarsSoils[eiv_silt + 1] &&
                readInVarsSoils[eiv_clay + 1]) {

                soilIn->fractionWeightMatric_sand[slNum] =
                    1 - (tempSilt[slNum] +
                         soilIn->fractionWeightMatric_clay[slNum]);
            }

            // Calculate clay if sand and silt provided but not clay
            if (readInVarsSoils[eiv_sand + 1] &&
                readInVarsSoils[eiv_silt + 1] &&
                !readInVarsSoils[eiv_clay + 1]) {
                soilIn->fractionWeightMatric_clay[slNum] =
                    1 - (tempSilt[slNum] +
                         soilIn->fractionWeightMatric_sand[slNum]);
            }

            // Set impermeability to 0 if not provided
            if (!readInVarsSoils[eiv_impermeability + 1]) {
                soilIn->impermeability[slNum] = 0.;
            }

            // Set avgLyrTempInit to 0 if not provided
            if (!readInVarsSoils[eiv_avgLyrTempInit + 1]) {
                soilIn->avgLyrTempInit[slNum] = 0.;
            }
        }

        // Check consistency between depth and width if both provided
        // (depth is provided by default if hasConstSoilDepths)
        if ((readInVarsSoils[eiv_soilLayerDepth + 1] || hasConstSoilDepths) &&
            readInVarsSoils[eiv_soilLayerWidth + 1]) {
            cumWidth += soilIn->width[slNum];

            if (!EQ(soilIn->depths[slNum], cumWidth)) {
                LogError(
                    LogInfo,
                    LOGERROR,
                    "Soil layer depth (%f cm) and "
                    "width (%f cm, cumulative = %f) are provided as inputs, "
                    "but they disagree in soil layer %d.",
                    soilIn->depths[slNum],
                    soilIn->width[slNum],
                    cumWidth,
                    slNum
                );
                return; // Exit function prematurely due to error
            }
        }

        // Check consistency between sand, silt, and clay if all provided
        if (readInVarsSoils[eiv_sand + 1] && readInVarsSoils[eiv_silt + 1] &&
            readInVarsSoils[eiv_clay + 1]) {
            sumTexture = soilIn->fractionWeightMatric_sand[slNum] +
                         tempSilt[slNum] +
                         soilIn->fractionWeightMatric_clay[slNum];

            if (GT(sumTexture, 1.)) {
                LogError(
                    LogInfo,
                    LOGERROR,
                    "Sum of sand (%f), silt (%f) and clay (%f) is larger "
                    "than 1 in soil layer %d.",
                    soilIn->fractionWeightMatric_sand[slNum],
                    tempSilt[slNum],
                    soilIn->fractionWeightMatric_clay[slNum],
                    slNum + 1
                );
                return; // Exit function prematurely due to error
            }
        }
    }


    if (*n_layers > nMaxSoilLayers) {
        LogError(
            LogInfo,
            LOGERROR,
            "Number of soil layers (%d) is larger than "
            "domain-wide expected maximum number of soil layers (%d).",
            *n_layers,
            nMaxSoilLayers
        );
    }
}

/**
@brief Read inputs relating to the input key 'inSoil'

Uses soil information from the text input files if \p hasConstSoilDepths,
and replaces values of properties that are provided by nc-inputs.
Otherwise (i.e., if not \p hasConstSoilDepths), a new soil is created
from scratch and all information is obtained from nc-inputs.

See also derive_missing_soils() for determination of number of soil layers,
estimation of missing soil properties from available properties, and
consistency checks.

@param[in] SW_Domain Struct of type SW_DOMAIN holding constant
    temporal/spatial information for a set of simulation runs
@param[out] SW_SiteSim Struct of type SW_SITE_SIM describing the
    simulated site during a simulation run
@param[out] SW_SiteRunIn Struct of type SW_SITE_RUN_INPUTS describing the
    simulated site for a specific run
@param[in] soilInFiles List of input files the user provided for the
    input key 'inSoil'
@param[in] hasConstSoilDepths Specifies of all soil inputs provided
    by the user (if any) have the same depth profile
@param[in] depthsAllSoilLayers Depths of soil layers (cm),
    used if \p hasConstSoilDepths
@param[in] soilConv A UDUNITS2 converter used to convert user-provided
    units to units that SW2 understands
@param[in] ncSUID Current simulation unit identifier for which is used
    to get data from netCDF
@param[out] LogInfo Holds information on warnings and errors
*/
static void read_soil_inputs(
    SW_DOMAIN *SW_Domain,
    SW_SITE_SIM *SW_SiteSim,
    char **soilInFiles,
    Bool hasConstSoilDepths,
    const double depthsAllSoilLayers[],
    sw_converter_t **soilConv,
    const size_t ncSUID[],
    Bool inputsProvideSWRCp,
    int numInputs,
    int numReads,
    size_t **starts,
    size_t **counts,
    int **openSoilFileIDs,
    double *tempSilt,
    double *tempVals,
    SW_SOIL_RUN_INPUTS *newSoilBuff,
    SW_RUN_INPUTS *inputs,
    LOG_INFO *LogInfo
) {
    char ***inVarInfo = SW_Domain->netCDFInput.inVarInfo[eSW_InSoil];
    Bool *readInputs = SW_Domain->netCDFInput.readInVars[eSW_InSoil];
    int **dimOrderInVar = SW_Domain->netCDFInput.dimOrderInVar[eSW_InSoil];

    int *varIDs = SW_Domain->SW_PathInputs.inVarIDs[eSW_InSoil];
    nc_type *varTypes = SW_Domain->SW_PathInputs.inVarTypes[eSW_InSoil];
    Bool *keyAttFlags = SW_Domain->SW_PathInputs.hasScaleAndAddFact[eSW_InSoil];
    double **scaleAddFactors =
        SW_Domain->SW_PathInputs.scaleAndAddFactVals[eSW_InSoil];
    Bool **missValFlags = SW_Domain->SW_PathInputs.missValFlags[eSW_InSoil];
    double **doubleMissVals =
        SW_Domain->SW_PathInputs.doubleMissVals[eSW_InSoil];
    double *storePtr;
    int numVals;

    int ncFileID = -1;
    int varID;
    size_t start[4] = {0}; /* Maximum of four dimensions */
    size_t count[4] = {0}; /* Maximum of four dimensions */
    const int pftIndex = 4;
    Bool hasPFT;
    Bool inSiteDom = SW_Domain->netCDFInput.siteDoms[eSW_InSoil];
    Bool isSwrcpVar;
    int numVarsInSoilKey = numVarsInKey[eSW_InSoil];
    char *varName;
    int vegIndex = 0;
    int setIter;
    int loopIter;
    size_t defSetStart[2] = {0};
    size_t defSetCount[2] = {1, 1};
    LyrIndex numLyrs;
    int latIndex;
    int lonIndex;
    int vertIndex;
    int pftWriteIndex;
    int read;
    int site;
    int numSites = 1;
    int inputOrigin = 0;
    size_t writeIndex;
    int input = 0;
    double *readPtr;

    Bool varHasAddScaleAtts;
    double scaleFactor;
    double addOffset;

    int varNum;
    int fIndex = 1;

    SW_SOIL_RUN_INPUTS *soils = NULL;

#if !defined(SWMPI)
    Bool useIndexFile = SW_Domain->netCDFInput.useIndexFile[eSW_InSoil];
    char *fileName;
#endif

    while (!readInputs[fIndex + 1]) {
        fIndex++;
    }

#if !defined(SWMPI)
    get_read_start(
        useIndexFile, soilInFiles[0], inSiteDom, ncSUID, defSetStart, LogInfo
    );
    if (LogInfo->stopRun) {
        return;
    }
#endif

    /* Initialize soils */
    for (input = 0; input < numInputs; input++) {
        inputs[input].SiteRunIn.n_layers = 0;

        if (!hasConstSoilDepths) {
            SW_SOIL_construct(&newSoilBuff[input]);
        }
    }
    input = 0;

    for (read = 0; read < numReads; read++) {
#if defined(SWMPI)
        defSetStart[0] = starts[read][0];
        defSetStart[1] = starts[read][1];

        defSetCount[0] = counts[read][0];
        defSetCount[1] = counts[read][1];

        numSites = (inSiteDom) ? defSetCount[0] : defSetCount[1];
#endif

        for (varNum = fIndex; varNum < numVarsInSoilKey; varNum++) {
            if (!readInputs[varNum + 1] ||
                (varNum == 1 && hasConstSoilDepths)) {
                continue;
            }

#if !defined(SWMPI)
            fileName = soilInFiles[varNum];
#endif

            /* Don't read more than the max simulated number of soil layers */
            numLyrs =
                MIN(SW_Domain->SW_PathInputs.numSoilVarLyrs[varNum],
                    SW_Domain->nMaxSoilLayers);
            hasPFT = (Bool) (dimOrderInVar[varNum][pftIndex] > -1);
            varID = varIDs[varNum];
            varName = inVarInfo[varNum][INNCVARNAME];
            varHasAddScaleAtts = keyAttFlags[varNum];
            isSwrcpVar = (Bool) (varNum >= eiv_swrcpMS[0]);
            latIndex = dimOrderInVar[varNum][0];
            lonIndex = dimOrderInVar[varNum][1];
            vertIndex = dimOrderInVar[varNum][2];
            pftWriteIndex = dimOrderInVar[varNum][4];

            start[0] = start[1] = start[2] = start[3] = 0;
            count[0] = count[1] = count[2] = count[3] = 0;
            start[latIndex] = defSetStart[0];
            count[latIndex] = defSetCount[0];
            count[vertIndex] = numLyrs;

            if (lonIndex > -1) {
                start[lonIndex] = defSetStart[1];
                count[lonIndex] = defSetCount[1];
            }

            numVals = (int) numLyrs;

#if defined(SWMPI)
            ncFileID = openSoilFileIDs[varNum][0];
#else
            SW_NC_open(fileName, NC_NOWRITE, &ncFileID, LogInfo);
            if (LogInfo->stopRun) {
                return;
            }
#endif

            if (varHasAddScaleAtts) {
                scaleFactor = scaleAddFactors[varNum][0];
                addOffset = scaleAddFactors[varNum][1];
            } else {
                scaleFactor = 1.0;
                addOffset = 0.0;
            }

            for (site = 0; site < numSites; site++) {
                soils = (hasConstSoilDepths) ? &inputs[input].SoilRunIn :
                                               &newSoilBuff[input];

                /* must match possVarNames[eSW_InSoil] (without spatial index)
                 */
                double *values1D[] = {
                    soils->depths,
                    soils->width,
                    soils->soilDensityInput,
                    soils->fractionVolBulk_gravel,
                    soils->fractionWeightMatric_sand,
                    soils->fractionWeightMatric_clay,
                    &tempSilt[MAX_LAYERS * (site + input)],
                    soils->fractionWeight_om,
                    soils->impermeability,
                    soils->avgLyrTempInit,
                    soils->evap_coeff
                };

                double(*trans_coeff)[MAX_LAYERS] = soils->transp_coeff;
                double(*swrcpMS)[SWRC_PARAM_NMAX] = soils->swrcpMineralSoil;

                writeIndex = ((!isSwrcpVar) ? numVals : 1) * site;

                readPtr = tempVals;
                if (varNum >= eiv_transpCoeff[0] &&
                    varNum <= eiv_transpCoeff[NVEGTYPES - 1]) {
                    /* Set trans_coeff */
                    vegIndex = varNum - eiv_transpCoeff[0];
                    storePtr = (double *) trans_coeff[vegIndex];

                } else if (varNum >= eiv_swrcpMS[0] &&
                           varNum <= eiv_swrcpMS[SWRC_PARAM_NMAX - 1]) {
                    /* Set swrcp */
                    storePtr = NULL; // Deal with separtely in iter loop
                } else {
                    storePtr = values1D[varNum - 1];
                }

                if (hasPFT) {
                    count[pftWriteIndex] = 1;
                    start[pftWriteIndex] = vegIndex;
                }

                if (site == 0) {
                    get_values_multiple(
                        ncFileID, varID, start, count, varName, readPtr, LogInfo
                    );
                    if (LogInfo->stopRun) {
                        goto closeFile;
                    }
                }

                setIter = (isSwrcpVar) ? (int) numLyrs : 1;

                for (loopIter = 0; loopIter < setIter; loopIter++) {
                    set_read_vals(
                        missValFlags[varNum],
                        doubleMissVals,
                        &readPtr[writeIndex],
                        (!isSwrcpVar) ? numVals : 1,
                        varNum - 1,
                        varTypes[varNum],
                        scaleFactor,
                        addOffset,
                        soilConv[varNum - 1],
                        isSwrcpVar,
                        (!isSwrcpVar) ? 0 : (varNum - eiv_swrcpMS[0]),
                        loopIter,
                        (!isSwrcpVar) ? storePtr : swrcpMS[loopIter]
                    );
                }

                input++;
            }
            input = inputOrigin;

#if !defined(SWMPI)
            nc_close(ncFileID);
            ncFileID = -1;
#endif
        }

        input += numSites;
        inputOrigin = input;
    }

    for (input = 0; input < numInputs; input++) {
        soils = (hasConstSoilDepths) ? &inputs[input].SoilRunIn :
                                       &newSoilBuff[input];

        /* Derive missing soil properties and check others */
        derive_missing_soils(
            &inputs[input].SiteRunIn.n_layers,
            soils,
            readInputs,
            hasConstSoilDepths,
            depthsAllSoilLayers,
            SW_Domain->nMaxSoilLayers,
            &tempSilt[input * MAX_LAYERS],
            LogInfo
        );
        if (LogInfo->stopRun) {
            goto closeFile;
        }

        if (!hasConstSoilDepths) {
            memcpy(
                &inputs[input].SoilRunIn,
                &newSoilBuff[input],
                sizeof(SW_SOIL_RUN_INPUTS)
            );
        }
    }

    SW_SiteSim->site_has_swrcpMineralSoil = inputsProvideSWRCp;

closeFile:
#if defined(SWMPI)
    (void) soilInFiles;
    (void) ncSUID;
#else
    if (ncFileID > -1) {
        nc_close(ncFileID);
    }
    (void) starts;
    (void) counts;
    (void) openSoilFileIDs;
#endif
}

/**
@brief Compare the strings provided by the user contained in a PFT
variable against the expected values/order the program expects
(i.e., "Trees", "Shrubs", "Forbs", "Grasses")

@param[in] ncFileID File identifier of the nc file being read
@param[in] pftName Name of the PFT variable to test the values of
@param[out] LogInfo Holds information dealing with logfile output
*/
static void compare_pft_strings(
    int ncFileID, char *pftName, LOG_INFO *LogInfo
) {
    int varID;
    int pftStr;
    const char *const expPFTStrings[] = {"Trees", "Shrubs", "Forbs", "Grasses"};
    char *names[] = {NULL, NULL, NULL, NULL};

    SW_NC_get_var_identifier(ncFileID, pftName, &varID, LogInfo);
    if (LogInfo->stopRun) {
        return;
    }

    if (nc_get_var_string(ncFileID, varID, names) != NC_NOERR) {
        LogError(
            LogInfo,
            LOGERROR,
            "Could not get the string values of '%s'.",
            pftName
        );
        goto freeMem;
    }

    for (pftStr = 0; pftStr < NVEGTYPES; pftStr++) {
        if (strcmp(names[pftStr], expPFTStrings[pftStr]) != 0) {
            LogError(
                LogInfo,
                LOGERROR,
                "The variable '%s' does not match the ordering the "
                "program expects to have for a PFT variable. These values "
                "should match 'Trees', 'Shrubs', 'Forbs', 'Grasses'.",
                pftName
            );
            goto freeMem;
        }
    }

freeMem:
    for (pftStr = 0; pftStr < NVEGTYPES; pftStr++) {
        if (!isnull(names)) {
            free(names[pftStr]);
            names[pftStr] = NULL;
        }
    }
}

/* =================================================== */
/*             Global Function Definitions             */
/* --------------------------------------------------- */


/**
@brief Allocate space for values specifying how to detect if an input
through nc files are missing

@param[in] numVars Number of variables to allocate for within an input key
@param[out] doubleMissVals List to allocate space for to store the
missing values
@param[out] LogInfo Holds information dealing with logfile output
*/
void SW_NCIN_alloc_miss_vals(
    int numVars, double ***doubleMissVals, LOG_INFO *LogInfo
) {
    int varNum;
    int wkgVarNum;
    const size_t numVals = 2;

    if (isnull(*doubleMissVals)) {
        /*
            Allocate 2 values to store the maximum number of expected
            missing value specifying values
        */
        for (varNum = 0; varNum < numVars + 1; varNum++) {
            if (varNum > 0) {
                (*doubleMissVals)[varNum - 1] = (double *) Mem_Malloc(
                    sizeof(double) * numVals,
                    "SW_NCIN_alloc_miss_vals()",
                    LogInfo
                );
                (*doubleMissVals)[varNum - 1][0] = 0.0;
                (*doubleMissVals)[varNum - 1][1] = 0.0;
            } else {
                *doubleMissVals = (double **) Mem_Malloc(
                    sizeof(double *) * numVars,
                    "SW_NCIN_alloc_miss_vals()",
                    LogInfo
                );
                for (wkgVarNum = 0; wkgVarNum < numVars; wkgVarNum++) {
                    (*doubleMissVals)[wkgVarNum] = NULL;
                }
            }
        }
        if (LogInfo->stopRun) {
            return;
        }
    }
}

/**
@brief Allocate space for information pertaining to input variables
that will be used throughout simulations rather than gaining the same
information many times during said simulation runs

@param[in] numVars Number of variables to allocate for
@param[in] currKey Current input key being allocated for
@param[in] allocDimVars A flag specifying if the function should
    allocate dimension variable indices
@param[out] inVarIDs Identifiers of variables of a specific input
key within provide nc files
@param[out] inVarType Types of variables of a specific input
key within provide nc files
@param[out] hasScaleAndAddFact Flags specifying if a variable has both
attributes "scale_factor" and "add_offset"
@param[out] scaleAndAddFactVals A list that contains values of the attributes
"scale_factor" and "add_offset" if both are present
@param[out] missValFlags A list of flags specifying the user-provided
information to specify a missing value in input files
@param[out] dimOrderInVar A list of indices specifying the order
    of dimensions for each variable
@param[out] numSoilVarLyrs A list holding the number of soil layers for
all soil input key variables
@param[out] LogInfo Holds information dealing with logfile output
*/
void SW_NCIN_alloc_sim_var_information(
    int numVars,
    int currKey,
    Bool allocDimVars,
    int **inVarIDs,
    nc_type **inVarType,
    Bool **hasScaleAndAddFact,
    double ***scaleAndAddFactVals,
    Bool ***missValFlags,
    int ***dimOrderInVar,
    size_t **numSoilVarLyrs,
    LOG_INFO *LogInfo
) {
    int varNum;
    size_t val;
    const size_t numFactVals = 2;

    *inVarIDs = (int *) Mem_Malloc(
        sizeof(int) * numVars, "SW_NCIN_alloc_sim_var_information()", LogInfo
    );
    if (LogInfo->stopRun) {
        return;
    }
    *inVarType = (nc_type *) Mem_Malloc(
        sizeof(nc_type) * numVars,
        "SW_NCIN_alloc_sim_var_information()",
        LogInfo
    );
    if (LogInfo->stopRun) {
        return;
    }

    *hasScaleAndAddFact = (Bool *) Mem_Malloc(
        sizeof(Bool) * numVars, "SW_NCIN_alloc_sim_var_information()", LogInfo
    );
    if (LogInfo->stopRun) {
        return;
    }
    for (varNum = 0; varNum < numVars; varNum++) {
        (*hasScaleAndAddFact)[varNum] = swFALSE;
    }

    *scaleAndAddFactVals = (double **) Mem_Malloc(
        sizeof(double *) * numVars,
        "SW_NCIN_alloc_sim_var_information()",
        LogInfo
    );
    if (LogInfo->stopRun) {
        return;
    }

    for (varNum = 0; varNum < numVars; varNum++) {
        (*scaleAndAddFactVals)[varNum] = NULL;
    }

    *missValFlags = (Bool **) Mem_Malloc(
        sizeof(Bool *) * numVars, "SW_NCIN_alloc_sim_var_information()", LogInfo
    );
    if (LogInfo->stopRun) {
        return;
    }
    for (varNum = 0; varNum < numVars; varNum++) {
        (*missValFlags)[varNum] = NULL;
    }

    if (allocDimVars) {
        SW_NCIN_allocDimVar(numVars, dimOrderInVar, LogInfo);
    }

    for (varNum = 0; varNum < numVars; varNum++) {
        (*scaleAndAddFactVals)[varNum] = (double *) Mem_Malloc(
            sizeof(double) * numFactVals,
            "SW_NCIN_alloc_sim_var_information()",
            LogInfo
        );
        if (LogInfo->stopRun) {
            return;
        }

        (*missValFlags)[varNum] = (Bool *) Mem_Malloc(
            sizeof(Bool) * SIM_INFO_NFLAGS,
            "SW_NCIN_alloc_sim_var_information()",
            LogInfo
        );
        if (LogInfo->stopRun) {
            return;
        }

        for (val = 0; val < SIM_INFO_NFLAGS; val++) {
            if (val < numFactVals) {
                (*scaleAndAddFactVals)[varNum][val] = SW_MISSING;
            }

            (*missValFlags)[varNum][val] = swFALSE;
        }

        (*inVarIDs)[varNum] = -1;
        (*inVarType)[varNum] = 0;
        (*hasScaleAndAddFact)[varNum] = swFALSE;
    }

    if (currKey == eSW_InSoil) {
        *numSoilVarLyrs = (size_t *) Mem_Malloc(
            sizeof(size_t) * numVars,
            "SW_NCIN_alloc_sim_var_information()",
            LogInfo
        );
        if (LogInfo->stopRun) {
            return;
        }

        for (varNum = 0; varNum < numVars; varNum++) {
            (*numSoilVarLyrs)[varNum] = 0;
        }
    }
}

/**
@brief Allocate the dimension variable information for a key

@param[in] numVars Number of variables within key to allocate
@param[out] dimOrderInVar A list of indices specifying the order
    of dimensions for each variable
@param[out] LogInfo Holds information dealing with logfile output
*/
void SW_NCIN_allocDimVar(int numVars, int ***dimOrderInVar, LOG_INFO *LogInfo) {
    int varNum;
    int val;

    *dimOrderInVar = (int **) Mem_Malloc(
        sizeof(int *) * numVars, "SW_NCIN_allocDimVar()", LogInfo
    );
    if (LogInfo->stopRun) {
        return;
    }

    for (varNum = 0; varNum < numVars; varNum++) {
        (*dimOrderInVar)[varNum] = NULL;
    }

    for (varNum = 0; varNum < numVars; varNum++) {
        (*dimOrderInVar)[varNum] = (int *) Mem_Malloc(
            sizeof(int) * MAX_NDIMS, "SW_NCIN_allocDimVar()", LogInfo
        );
        if (LogInfo->stopRun) {
            return;
        }
    }

    for (varNum = 0; varNum < numVars; varNum++) {
        for (val = 0; val < MAX_NDIMS; val++) {
            (*dimOrderInVar)[varNum][val] = -1;
        }
    }
}

/**
@brief Mark a site/gridcell as completed (success/fail) in the progress file

@param[in] isFailure Did simulation run fail or succeed?
@param[in] progFileID Identifier of the progress netCDF file
@param[in] progVarID Identifier of the progress variable within the progress
    netCDF
@param[in] start A list of calculated start values for when dealing
    with the netCDF library; simply ncSUID if SWMPI is not enabled
@param[in] count A list of count parts used for accessing/writing to
    netCDF files; simply {1, 0} or {1, 1} if SWMPI is not enabled
@param[in,out] LogInfo Holds information dealing with logfile output
*/
void SW_NCIN_set_progress(
    Bool isFailure,
    int progFileID,
    int progVarID,
    size_t start[],
    size_t count[],
    LOG_INFO *LogInfo
) {
    const signed char mark = (!isFailure) ? PRGRSS_DONE : PRGRSS_FAIL;

    SW_NC_write_vals(
        &progVarID,
        progFileID,
        NULL,
        (void *) &mark,
        start,
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
    const char *projGridMap = "%s: %s %s %s: %s %s";
    const char *geoGridMap = SW_Domain->OutDom.netCDFOutput.crs_geogsc.crs_name;
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
    const signed char fillVal[] = {NC_FILL_BYTE};
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
    Bool progVarExists =
        (Bool) (progFileExists && SW_NC_varExists(*progFileID, progVarName));
    Bool createOrModFile =
        (Bool) (!progFileExists || (progFileIsiteDom && !progVarExists));
    Bool useDefaultChunking = swTRUE;

    /* Fill dynamic coordinate names */
    if (domTypeIsS) {
        (void) snprintf(
            coordStr,
            MAX_FILENAMESIZE,
            coord,
            readinGeoYName,
            readinGeoXName,
            siteName
        );
    } else {
        (void) snprintf(
            coordStr, MAX_FILENAMESIZE, coord, readinGeoYName, readinGeoXName
        );
    }
    attVals[numAtts - 1] = coordStr;

    if (!primCRSIsGeo) {
        (void) snprintf(
            gridMapStr,
            MAX_FILENAMESIZE,
            grid_map,
            SW_Domain->OutDom.netCDFOutput.crs_projsc.crs_name,
            SW_Domain->OutDom.netCDFOutput.proj_XAxisName,
            SW_Domain->OutDom.netCDFOutput.proj_YAxisName,
            geoGridMap,
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
            useDefaultChunking,
            (void *) fillVal,
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

/**
@brief Identify soil profile information across simulation domain from netCDF

If \p hasConsistentSoilLayerDepths, then
    - \p nMaxSoilLayers is set equal to \p default_n_layers
    - \p nMaxEvapLayers is set equal to \p nMaxSoilLayers
    - \p depthsAllSoilLayers is set equal to \p default_depths
    - a warning is produced if any soil nc-input has fewer layers than
      \p nMaxSoilLayers

If not \p hasConsistentSoilLayerDepths, then
    - \p nMaxSoilLayers is set to the size of the vertical dimension of
      soil layer inputs or soil layer width/thickness (if provided)
    - \p nMaxEvapLayers is set equal to \p nMaxSoilLayers
    - an error is produced if any soil nc-input has fewer layers than
      \p nMaxSoilLayers

An error is produced if \p *nMaxSoilLayers > MAX_LAYERS.

@param[in] SW_netCDFIn Constant netCDF input file information
@param[out] hasConsistentSoilLayerDepths Flag indicating if all simulation
    run within domain have identical soil layer depths
    (though potentially variable number of soil layers)
@param[out] nMaxSoilLayers Largest number of soil layers across
    simulation domain
@param[out] nMaxEvapLayers Largest number of soil layers from which
    bare-soil evaporation may extract water across simulation domain
    (nMaxEvapLayers is set to nMaxSoilLayers).
@param[out] depthsAllSoilLayers Lower soil layer depths [cm] if
    consistent across simulation domain
@param[in] numSoilVarLyrs An array with the number of soil layers for
    all soil input key variables
@param[in] default_n_layers Default number of soil layers
@param[in] default_depths Default values of soil layer depths [cm]
@param[out] LogInfo Holds information on warnings and errors
*/
void SW_NCIN_soilProfile(
    SW_NETCDF_IN *SW_netCDFIn,
    Bool hasConsistentSoilLayerDepths,
    LyrIndex *nMaxSoilLayers,
    LyrIndex *nMaxEvapLayers,
    double depthsAllSoilLayers[],
    const size_t numSoilVarLyrs[],
    LyrIndex default_n_layers,
    const double default_depths[],
    LOG_INFO *LogInfo
) {
    int varNum;
    Bool *readInVarsSoils = SW_netCDFIn->readInVars[eSW_InSoil];

    if (hasConsistentSoilLayerDepths) {
        /* Set maximum number of soil layers */
        *nMaxSoilLayers = default_n_layers;

        /* Set soil layer depths from default */
        memcpy(
            depthsAllSoilLayers,
            default_depths,
            sizeof(default_depths[0]) * default_n_layers
        );

        /* Note: read_soil_inputs() will check that input
           layerDepth and layerWidth are consistent with depthsAllSoilLayers
           for each site/gridcell
        */

    } else {
        /* Determine maximum number of soil layers */
        if (readInVarsSoils[eiv_soilLayerDepth + 1]) {
            *nMaxSoilLayers = numSoilVarLyrs[eiv_soilLayerDepth];

        } else if (readInVarsSoils[eiv_soilLayerWidth + 1]) {
            *nMaxSoilLayers = numSoilVarLyrs[eiv_soilLayerWidth];

        } else {
            LogError(
                LogInfo,
                LOGERROR,
                "User indicated that soil layer depth and width/thickness "
                "varies among sites/grid cells but neither depth "
                "nor width/thickness of soil layers is provided as input."
            );
            return; // Exit function prematurely due to error
        }
    }

    if (*nMaxSoilLayers > MAX_LAYERS) {
        LogError(
            LogInfo,
            LOGERROR,
            "Domain-wide maximum number of soil layers (%d) "
            "is larger than allowed (MAX_LAYERS = %d).",
            *nMaxSoilLayers,
            MAX_LAYERS
        );
        return; // Exit function prematurely due to error
    }

    /* Check that all soil nc-inputs have nVertical >= *nMaxSoilLayers */
    for (varNum = 1; varNum < numVarsInKey[eSW_InSoil]; varNum++) {
        if (readInVarsSoils[varNum + 1] &&
            *nMaxSoilLayers < numSoilVarLyrs[varNum]) {
            LogError(
                LogInfo,
                hasConsistentSoilLayerDepths ? LOGWARN : LOGERROR,
                "Expected %d soil layers but nc-input '%s' has only %d layers.",
                *nMaxSoilLayers,
                possVarNames[eSW_InSoil][varNum],
                numSoilVarLyrs[varNum]
            );
            return; // Exit function prematurely due to error
        }
    }

    /* nc-mode produces soil output for every soil layer */
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
    if (LogInfo->stopRun) {
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
        progFileID, &progVarID, "progress", ncSUID, (void *) &progVal, LogInfo
    );

    return (Bool) (!LogInfo->stopRun && progVal == PRGRSS_READY);
}

/**
@brief Read weather input from nc file(s) provided by the user and
store them for the next simulation run

@param[in] SW_Domain Struct of type SW_DOMAIN holding constant
temporal/spatial information for a set of simulation runs
@param[out] SW_WeatherIn Struct of type SW_WEATHER_INPUTS holding all relevant
information pretaining to meteorological input data
@param[in] weathInFiles List of expected input file names the
program generated based on user input
@param[in] indexFileName Name of the index file that may or may not
have been created for the input key 'inWeather'
@param[in] ncSUID Current simulation unit identifier for which is used
to get data from netCDF
@param[in] weathConv A list of UDUNITS2 converters that were created
to convert input data to units the program can understand within the
"inWeather" input key
@param[in] elevation Site elevation above sea level [m]
@param[out] LogInfo Holds information on warnings and errors
*/
static void read_weather_input(
    SW_DOMAIN *SW_Domain,
    SW_WEATHER_INPUTS *SW_WeatherIn,
    char ***weathInFiles,
    char *indexFileName,
    const size_t ncSUID[],
    sw_converter_t **weathConv,
    int numInputs,
    int numReads,
    size_t **starts,
    size_t **counts,
    int **weathFileIDs,
    double *elevation,
    double *tempVals,
    SW_RUN_INPUTS *inputs,
    LOG_INFO *LogInfo
) {
    unsigned int **weathStartEndYrs =
        SW_Domain->SW_PathInputs.ncWeatherInStartEndYrs;
    char ***inVarInfo = SW_Domain->netCDFInput.inVarInfo[eSW_InWeather];
    Bool *readInput = SW_Domain->netCDFInput.readInVars[eSW_InWeather];
    unsigned int numWeathFiles = SW_Domain->SW_PathInputs.ncNumWeatherInFiles;
    int varNum = 1;
    size_t start[4] = {0}; /* Up to four dimensions per variable */
    size_t count[4] = {0}; /* Up to four dimensions per variable */
    TimeInt numDays;
    TimeInt yearIndex;
    TimeInt year;
    Bool inSiteDom = SW_Domain->netCDFInput.siteDoms[eSW_InWeather];
    int fIndex = 1;
    int varID = -1;
    int ncFileID = -1;
    char *varName;
    unsigned int weathFileIndex = 0;
    unsigned int *numDaysInYears = SW_Domain->SW_PathInputs.numDaysInYear;
    Bool varHasAddScaleAtts;
    nc_type *varTypes = SW_Domain->SW_PathInputs.inVarTypes[eSW_InWeather];
    Bool *keyAttFlags =
        SW_Domain->SW_PathInputs.hasScaleAndAddFact[eSW_InWeather];
    double **scaleAddFactors =
        SW_Domain->SW_PathInputs.scaleAndAddFactVals[eSW_InWeather];
    Bool **missValFlags = SW_Domain->SW_PathInputs.missValFlags[eSW_InWeather];
    double **doubleMissVals =
        SW_Domain->SW_PathInputs.doubleMissVals[eSW_InWeather];
    int **dimOrderInVar = SW_Domain->netCDFInput.dimOrderInVar[eSW_InWeather];
    unsigned int **weatherIndices =
        SW_Domain->SW_PathInputs.ncWeatherStartEndIndices;
    double scaleFactor;
    double addOffset;
    unsigned int beforeFileIndex;
    size_t defSetStart[2] = {0};
    size_t defSetCount[2] = {1, 1};
    int latIndex;
    int lonIndex;
    int timeIndex;
    int read;
    int numSites = 1;
    int site;
    int input;
    double ***tempWeatherHist = NULL;
    size_t writeIndex = 0;

#if !defined(SWMPI)
    char *fileName;
    Bool useIndexFile = SW_Domain->netCDFInput.useIndexFile[eSW_InWeather];
#endif

    while (!readInput[fIndex + 1]) {
        fIndex++;
    }

    allocate_temp_weather(
        SW_WeatherIn->n_years, numInputs, &tempWeatherHist, LogInfo
    );
    if (LogInfo->stopRun) {
        goto closeFile;
    }

#if !defined(SWMPI)
    get_read_start(
        useIndexFile, indexFileName, inSiteDom, ncSUID, defSetStart, LogInfo
    );
    if (LogInfo->stopRun) {
        return;
    }
#endif
    for (varNum = fIndex; varNum < numVarsInKey[eSW_InWeather]; varNum++) {
        if (!readInput[varNum + 1]) {
            continue;
        }

        varHasAddScaleAtts = keyAttFlags[varNum];
        varID = SW_Domain->SW_PathInputs.inVarIDs[eSW_InWeather][varNum];
        latIndex = dimOrderInVar[varNum][0];
        lonIndex = dimOrderInVar[varNum][1];
        timeIndex = dimOrderInVar[varNum][3];

        start[timeIndex] = 0;

        weathFileIndex = SW_Domain->SW_PathInputs.weathStartFileIndex;
        for (yearIndex = 0; yearIndex < SW_WeatherIn->n_years; yearIndex++) {
            year = SW_Domain->startyr + yearIndex;

            if (varNum == fIndex) {
                clear_hist_weather(numInputs, NULL, tempWeatherHist[yearIndex]);
            }

            beforeFileIndex = weathFileIndex;
            while (weathFileIndex < numWeathFiles &&
                   weathStartEndYrs[weathFileIndex][1] < year) {
                weathFileIndex++;
            }

            numDays = numDaysInYears[yearIndex];
            count[timeIndex] = numDays;
            tempVals[MAX_DAYS - 1] = SW_MISSING;

#if !defined(SWMPI)
            fileName = weathInFiles[varNum][weathFileIndex];
#endif
            varName = inVarInfo[varNum][INNCVARNAME];

            /* Check to see if a different file has to be opened,
               if so, we need to make sure the correct start index
               is applied to the start index array */
            if (weathFileIndex > beforeFileIndex) {
                start[timeIndex] = weatherIndices[weathFileIndex][0];

#if !defined(SWMPI)
                if (ncFileID > -1) {
                    nc_close(ncFileID);
                    ncFileID = -1;
                }
#endif
            }

            if (varHasAddScaleAtts) {
                scaleFactor = scaleAddFactors[varNum][0];
                addOffset = scaleAddFactors[varNum][1];
            } else {
                scaleFactor = 1.0;
                addOffset = 0.0;
            }

            for (read = 0; read < numReads; read++) {
#if defined(SWMPI)
                defSetStart[0] = starts[read][0];
                defSetStart[1] = starts[read][1];

                defSetCount[0] = counts[read][0];
                defSetCount[1] = counts[read][1];
#endif

                start[latIndex] = defSetStart[0];
                count[latIndex] = defSetCount[0];

                if (lonIndex > -1) {
                    count[lonIndex] = defSetCount[1];
                    start[lonIndex] = defSetStart[1];
                }

#if defined(SWMPI)
                ncFileID = weathFileIDs[varNum][weathFileIndex];
#else
                if (ncFileID == -1) {
                    SW_NC_open(fileName, NC_NOWRITE, &ncFileID, LogInfo);
                    if (LogInfo->stopRun) {
                        return;
                    }
                }
#endif
                numSites = (inSiteDom) ? count[latIndex] : count[lonIndex];

                /* Read in an entire year's worth of weather data */
                get_values_multiple(
                    ncFileID, varID, start, count, varName, tempVals, LogInfo
                );
                if (LogInfo->stopRun) {
                    goto closeFile;
                }

                for (site = 0; site < numSites; site++) {
                    writeIndex = site * MAX_DAYS;

                    set_read_vals(
                        missValFlags[varNum],
                        doubleMissVals,
                        &tempVals[writeIndex],
                        MAX_DAYS,
                        varNum,
                        varTypes[varNum],
                        scaleFactor,
                        addOffset,
                        weathConv[varNum],
                        swFALSE,
                        0,
                        0,
                        &tempWeatherHist[yearIndex][varNum - 1][writeIndex]
                    );
                    if (LogInfo->stopRun) {
                        goto closeFile;
                    }
                }
            }

            start[timeIndex] += count[timeIndex];
#if !defined(SWMPI)
            nc_close(ncFileID);
            ncFileID = -1;
#endif
        }

#if !defined(SWMPI)
        if (ncFileID > -1) {
            nc_close(ncFileID);
            ncFileID = -1;
        }
#endif
    }

    for (input = 0; input < numInputs; input++) {
        SW_WTH_setWeatherValues(
            SW_Domain->startyr,
            SW_WeatherIn->n_years,
            SW_WeatherIn->dailyInputFlags,
            tempWeatherHist,
            elevation[input],
            MAX_DAYS * input,
            inputs[input].weathRunAllHist,
            LogInfo
        );
    }

closeFile:
#if defined(SWMPI)
    (void) weathInFiles;
    (void) indexFileName;
    (void) ncSUID;
#else
    if (ncFileID > -1) {
        nc_close(ncFileID);
    }
    (void) starts;
    (void) counts;
    (void) weathFileIDs;
#endif

    deallocate_temp_weather(SW_WeatherIn->n_years, &tempWeatherHist);
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
@param[in] starts A list of size SW_NINKEYSNC specifying the start
    indices used when reading/writing using the netCDF library;
    default size is `nSuids` but as mentioned in `numWrites`, it would
    be best to not fill this array; NULL if SWMPI is not defined
@param[in] counts A list of size SW_NINKEYSNC specifying the count
    indices used when reading/writing using the netCDF library;
    default size is `nSuids` but as mentioned in `numWrites`, it would
    be best to not fill this array; counts match placements with `start`
    indices for each key; NULL if SWMPI is not defined
@param[in] openNCFileIDs A list of open netCDF file identifiers; NULL if
    SWMPI is not defined
@param[in] numReads A list of size SW_NINKEYSNC holding how many
    contiguous reads it will take to read all the input for the specified
    input SUIDs
@param[in] numInputs Total number of site inputs that will be read-in
@param[in] tempMonthlyVals A list of lengths that will be used to specify
    how many inputs to send to a specific process
@param[in] elevations A list of elevations for each site we will read
@param[in] tempSiltVals A temporary buffer to store silt values
@param[in] tempVals A temporary buffer to store any soil variable in
@param[in] tempWeath A temporary buffer to store read weather input in
@param[in] newSoils A single (no SWMPI) or a list (SWMPI) of instances of
    SW_SOIL_RUN_INPUTS used as temporary storage when reading inputs
@param[in] inputs A single instance (no SWMPI) or a list (SWMPI) of
    SW_RUN_INPUTS that will be filled by a normal or I/O process
@param[out] LogInfo Holds information on warnings and errors
*/
void SW_NCIN_read_inputs(
    SW_RUN *sw,
    SW_DOMAIN *SW_Domain,
    const size_t ncSUID[],
    size_t ***starts,
    size_t ***counts,
    int **openNCFileIDs[],
    int numReads[],
    int numInputs,
    double *tempMonthlyVals,
    double *elevations,
    double *tempSiltVals,
    double *tempVals,
    double *tempWeath,
    SW_SOIL_RUN_INPUTS *newSoils,
    SW_RUN_INPUTS *inputs,
    LOG_INFO *LogInfo
) {
    SW_WEATHER_INPUTS *SW_WeatherIn = &sw->WeatherIn;
    char ***ncInFiles = SW_Domain->SW_PathInputs.ncInFiles;
    Bool **readInputs = SW_Domain->netCDFInput.readInVars;
    sw_converter_t ***convs = SW_Domain->netCDFInput.uconv;
    unsigned int yearIn;
    unsigned int year;
    int input;
    Bool readSpatial = readInputs[eSW_InSpatial][0];
    Bool readClimate = readInputs[eSW_InClimate][0];
    Bool readTopo = readInputs[eSW_InTopo][0];
    Bool readWeather = readInputs[eSW_InWeather][0];
    Bool readVeg = readInputs[eSW_InVeg][0];
    Bool readSoil = readInputs[eSW_InSoil][0];
    Bool readSite = readInputs[eSW_InSite][0];
    int **weathFileIDs = NULL;
    int **vegFileIDs = NULL;
    int **soilFileIDs = NULL;
    int inIndex = 0;

#if defined(SWMPI)
    weathFileIDs = openNCFileIDs[eSW_InWeather];
    vegFileIDs = openNCFileIDs[eSW_InVeg];
    soilFileIDs = openNCFileIDs[eSW_InSoil];
#endif

    /* Allocate information before gathering inputs */
    if (readWeather) {
#if !defined(SWMPI)
        SW_WTH_allocateAllWeather(
            &sw->RunIn.weathRunAllHist, SW_WeatherIn->n_years, LogInfo
        );
        if (LogInfo->stopRun) {
            return;
        }
#endif

        for (input = 0; input < numInputs; input++) {
            for (yearIn = 0; yearIn < SW_WeatherIn->n_years; yearIn++) {
                clear_hist_weather(
                    numInputs, &inputs[input].weathRunAllHist[yearIn], NULL
                );
            }
        }
    }

    /* Read all activated inputs */
    if (readSpatial || readTopo || readClimate || readSite) {
        read_spatial_topo_climate_site_inputs(
            SW_Domain,
            numInputs,
            numReads,
            ncInFiles,
            ncSUID,
            starts,
            counts,
            convs,
            tempMonthlyVals,
            openNCFileIDs,
            inputs,
            LogInfo
        );
        if (LogInfo->stopRun) {
            return;
        }

#if defined(SWMPI)
        for (inIndex = 0; inIndex < numInputs; inIndex++) {
#endif
            for (yearIn = 0; yearIn < SW_WeatherIn->n_years; yearIn++) {
                year = yearIn + SW_WeatherIn->startYear;

                SW_WTH_setWeathUsingClimate(
                    &inputs[inIndex].weathRunAllHist[yearIn],
                    year,
                    SW_WeatherIn->use_cloudCoverMonthly,
                    SW_WeatherIn->use_humidityMonthly,
                    SW_WeatherIn->use_windSpeedMonthly,
                    sw->ModelSim.cum_monthdays,
                    sw->ModelSim.days_in_month,
                    inputs[inIndex].SkyRunIn.cloudcov,
                    inputs[inIndex].SkyRunIn.windspeed,
                    inputs[inIndex].SkyRunIn.r_humidity
                );
            }
#if defined(SWMPI)
        }
#endif
    }

    if (readWeather && !SW_WeatherIn->use_weathergenerator_only) {
        read_weather_input(
            SW_Domain,
            &sw->WeatherIn,
            SW_Domain->SW_PathInputs.ncWeatherInFiles,
            ncInFiles[eSW_InWeather][0],
            ncSUID,
            convs[eSW_InWeather],
            numInputs,
            numReads[eSW_InWeather],
            starts[eSW_InWeather],
            counts[eSW_InWeather],
            weathFileIDs,
            elevations,
            tempWeath,
            inputs,
            LogInfo
        );
        if (LogInfo->stopRun) {
            return;
        }

        for (input = 0; input < numInputs; input++) {
            SW_WTH_finalize_all_weather(
                &sw->MarkovIn,
                &sw->WeatherIn,
                inputs[input].weathRunAllHist,
                sw->ModelSim.cum_monthdays,
                sw->ModelSim.days_in_month,
                LogInfo
            );
            if (LogInfo->stopRun) {
                return;
            }
        }
    }

    if (readVeg) {
        read_veg_inputs(
            SW_Domain,
            starts[eSW_InVeg],
            counts[eSW_InVeg],
            ncInFiles[eSW_InVeg],
            numReads[eSW_InVeg],
            ncSUID,
            convs[eSW_InVeg],
            vegFileIDs,
            tempMonthlyVals,
            inputs,
            LogInfo
        );
        if (LogInfo->stopRun) {
            return;
        }
    }

    if (readSoil) {
        read_soil_inputs(
            SW_Domain,
            &sw->SiteSim,
            ncInFiles[eSW_InSoil],
            SW_Domain->hasConsistentSoilLayerDepths,
            SW_Domain->depthsAllSoilLayers,
            convs[eSW_InSoil],
            ncSUID,
            sw->SiteIn.inputsProvideSWRCp,
            numInputs,
            numReads[eSW_InSoil],
            starts[eSW_InSoil],
            counts[eSW_InSoil],
            soilFileIDs,
            tempSiltVals,
            tempVals,
            newSoils,
            inputs,
            LogInfo
        );
        if (LogInfo->stopRun) {
            return;
        }
    }
}

/**
@brief Additional checks on the netCDF input configuration

@param[in] SW_netCDFIn Constant netCDF input file information
@param[in] hasConsistentSoilLayerDepths Flag indicating if all simulation
    run within domain have identical soil layer depths
    (though potentially variable number of soil layers)
@param[in] inputsProvideSWRCp Are SWRC parameters obtained from
    input files (TRUE) or estimated with a PTF (FALSE)
@param[out] LogInfo Holds information on warnings and errors
*/
void SW_NCIN_check_input_config(
    SW_NETCDF_IN *SW_netCDFIn,
    Bool hasConsistentSoilLayerDepths,
    Bool inputsProvideSWRCp,
    LOG_INFO *LogInfo
) {
    /* Check inSoils for required inputs */
    checkRequiredSoils(
        SW_netCDFIn->readInVars[eSW_InSoil],
        hasConsistentSoilLayerDepths,
        inputsProvideSWRCp,
        LogInfo
    );
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
    int *fileIDs[] = {&indexFileID, &inFileID};
    const int numFiles = 2;

    char *fileName;
    char **fileNames;
    char **varInfo;
    char **indexVarInfo;
    Bool **readInVars = SW_Domain->netCDFInput.readInVars;
    Bool *useIndexFile = SW_Domain->netCDFInput.useIndexFile;
    Bool fileIsIndex;
    Bool primCRSIsGeo;
    char *crsName;
    unsigned int weathFileIndex = SW_Domain->SW_PathInputs.weathStartFileIndex;

    /* Check actual input files provided by the user */
    ForEachNCInKey(inKey) {
        if (readInVars[inKey][0] && inKey > eSW_InDomain) {
            indexVarInfo = SW_Domain->netCDFInput.inVarInfo[inKey][0];
            fileNames = SW_Domain->SW_PathInputs.ncInFiles[inKey];

            for (file = 0; file < numVarsInKey[inKey]; file++) {
                if (readInVars[inKey][file + 1] &&
                    (file > 0 || (file == 0 && useIndexFile[inKey]))) {

                    fileIsIndex = (Bool) (file == 0);
                    fileID = (fileIsIndex) ? &indexFileID : &inFileID;
                    varInfo = SW_Domain->netCDFInput.inVarInfo[inKey][file];
                    fileName = (inKey == eSW_InWeather && file > 0) ?
                                   SW_Domain->SW_PathInputs
                                       .ncWeatherInFiles[file][weathFileIndex] :
                                   fileNames[file];
                    primCRSIsGeo =
                        (Bool) (strcmp(
                                    varInfo[INGRIDMAPPING], "latitude_longitude"
                                ) == 0);

                    SW_NC_open(fileName, NC_NOWRITE, fileID, LogInfo);
                    if (LogInfo->stopRun) {
                        return;
                    }

                    /* Check the current input file either against the
                       domain (current file is an index file or the input
                       file's domain matches the programs domain exactly),
                       otherwise, compare the input file against the
                       provided index file */
                    if (fileIsIndex) {
                        SW_NC_check(
                            SW_Domain, *fileID, fileNames[file], LogInfo
                        );
                    } else if (readInVars[inKey][1] && useIndexFile[inKey]) {
                        if (primCRSIsGeo) {
                            crsName = varInfo[INCRSNAME];
                        } else {
                            crsName = indexVarInfo[INCRSNAME];
                        }

                        /* index file exists */
                        check_input_file_against_index(
                            SW_Domain->netCDFInput.inVarInfo[inKey][file],
                            indexFileID,
                            inFileID,
                            crsName,
                            varInfo[INCRSNAME],
                            LogInfo
                        );
                    }
                    if (LogInfo->stopRun) {
                        goto closeFile;
                    }

                    if (strcmp(varInfo[INVAXIS], "NA") != 0) {
                        compare_pft_strings(
                            inFileID, varInfo[INVAXIS], LogInfo
                        );
                        if (LogInfo->stopRun) {
                            goto closeFile;
                        }
                    }

                    if (file > 0) {
                        nc_close(inFileID);
                        inFileID = -1;
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

@param[in,out] SW_PathInputs Struct of type SW_PATH_INPUTS which
    holds basic information about input files and values
@param[in] readInVars Specifies which variables are to be read-in as input
@param[in] useIndexFile Specifies to create/use an index file
*/
void SW_NCIN_close_files(
    SW_PATH_INPUTS *SW_PathInputs, Bool **readInVars, Bool useIndexFile[]
) {
    int fileNum;

#if defined(SWMPI)
    SW_MPI_close_in_files(
        SW_PathInputs->openInFileIDs,
        readInVars,
        useIndexFile,
        SW_PathInputs->ncNumWeatherInFiles
    );
#else
    (void) readInVars;
    (void) useIndexFile;
#endif

    for (fileNum = 0; fileNum < SW_NVARDOM; fileNum++) {
        nc_close(SW_PathInputs->ncDomFileIDs[fileNum]);
    }
}

/**
@brief Initializes pointers only pertaining to netCDF input information

@param[in,out] SW_netCDFIn Constant netCDF input file information
*/
void SW_NCIN_init_ptrs(SW_NETCDF_IN *SW_netCDFIn) {
    int k;
    int coordNum;
    const int numCoords = 2;

    ForEachNCInKey(k) {
        SW_netCDFIn->inVarInfo[k] = NULL;
        SW_netCDFIn->units_sw[k] = NULL;
        SW_netCDFIn->uconv[k] = NULL;
        SW_netCDFIn->readInVars[k] = NULL;
        SW_netCDFIn->dimOrderInVar[k] = NULL;

        for (coordNum = 0; coordNum < numCoords; coordNum++) {
            SW_netCDFIn->projCoordConvs[k][coordNum] = NULL;
        }

        SW_netCDFIn->siteDoms[k] = swFALSE;
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
        &SW_netCDFIn->domYCoordsGeo,
        &SW_netCDFIn->domXCoordsGeo,
        &SW_netCDFIn->domYCoordsProj,
        &SW_netCDFIn->domXCoordsProj
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
    int coordNum;
    const int numCoords = 2;

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

    if (!isnull((void *) SW_netCDFIn->dimOrderInVar[key])) {
        for (varNum = 0; varNum < varsInKey; varNum++) {
            if (!isnull((void *) SW_netCDFIn->dimOrderInVar[key][varNum])) {
                free((void *) SW_netCDFIn->dimOrderInVar[key][varNum]);
                SW_netCDFIn->dimOrderInVar[key][varNum] = NULL;
            }
        }

        free((void *) SW_netCDFIn->dimOrderInVar[key]);
        SW_netCDFIn->dimOrderInVar[key] = NULL;
    }

    if (!isnull((void *) SW_netCDFIn->projCoordConvs[key])) {
        for (coordNum = 0; coordNum < numCoords; coordNum++) {
            if (!isnull((void *) SW_netCDFIn->projCoordConvs[key][coordNum])) {
#if defined(SWNETCDF) && defined(SWUDUNITS)
                cv_free(SW_netCDFIn->projCoordConvs[key][coordNum]);
#else
                free((void *) SW_netCDFIn->projCoordConvs[key][coordNum]);
#endif
                SW_netCDFIn->projCoordConvs[key][coordNum] = NULL;
            }
        }
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
        "%255[^\t]\t%255[^\t]\t%255[^\t]\t%255[^\t]\t%255[^\t]\t%255[^\t]";

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
    const int ncXDimInd = 12;
    const int ncYAxisInd = 13;
    const int ncYDimInd = 14;
    const int ncZAxisInd = 15;
    const int ncTAxisInd = 16;
    const int ncStYrInd = 17;
    const int ncStStartInd = 18;
    const int ncStPatInd = 19;
    const int ncCalendarInd = 20;
    const int ncVAxisInd = 21;
    const int userComInd = 22;

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
            input[ncXDimInd],
            input[ncYAxisInd],
            input[ncYDimInd],
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

            if ((isIndexFile && inVarNum == KEY_NOT_FOUND) ||
                (inKey == eSW_NoInKey || inVarNum == KEY_NOT_FOUND)) {

                if (isIndexFile && inVarNum == KEY_NOT_FOUND) {
                    LogError(
                        LogInfo,
                        LOGERROR,
                        "Could not find a match for the index name '%s'.",
                        input[SWVarNameInd]
                    );
                } else {
                    LogError(
                        LogInfo,
                        LOGERROR,
                        "Could not determine what the variable '%s' is "
                        "within the key '%s'.",
                        input[SWVarNameInd],
                        input[keyInd]
                    );
                }

                goto closeFile;
            }

            if (isAllVegVar) {
                maxVarIter = NVEGTYPES;

                if (inKey == eSW_InVeg) {
                    inVarNum = (inVarNum == 0) ? 2 : inVarNum + 2;
                } else {
                    /* Start variable at `Trees.trans_coeff` */
                    inVarNum = eiv_transpCoeff[0];
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
                                    "The variable '%s' has stride years <= 0.",
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
                        varInfoPtr[copyInfoIndex] =
                            Str_Dup(input[infoIndex], LogInfo);

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

    check_for_input_domain(SW_netCDFIn->readInVars[eSW_InDomain], LogInfo);
    if (LogInfo->stopRun) {
        goto closeFile;
    }

    /* Check columns of tsv input file */
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
            SW_netCDFIn->readInVars[eSW_InWeather],
            &SW_PathInputs->ncWeatherInFiles,
            &SW_PathInputs->ncWeatherInStartEndYrs,
            &SW_PathInputs->ncNumWeatherInFiles,
            &SW_PathInputs->weathStartFileIndex,
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
            if (!SW_netCDFIn->readInVars[key][varIndex + 1] ||
                strcmp(
                    SW_netCDFIn->inVarInfo[key][varIndex][INVARUNITS], "NA"
                ) == 0) {
                continue; // Skip variable iteration
            }

#if defined(SWUDUNITS)
            if (!isnull(SW_netCDFIn->units_sw[key][varIndex])) {
                unitFrom = ut_parse(
                    system,
                    SW_netCDFIn->inVarInfo[key][varIndex][INVARUNITS],
                    UT_UTF8
                );
                unitTo = ut_parse(
                    system, SW_netCDFIn->units_sw[key][varIndex], UT_UTF8
                );

                /* If the input variable is shortwave radiation or
                   swrcp, the internal units should be NA */
                if (!((key == eSW_InWeather && varIndex == eiv_shortWaveRad) ||
                      (key == eSW_InSoil && varIndex >= eiv_swrcpMS[0] &&
                       varIndex <= eiv_swrcpMS[SWRC_PARAM_NMAX - 1]))) {

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

                        /* converter is not available: output in internal units
                         */
                        free(SW_netCDFIn->inVarInfo[key][varIndex][INVARUNITS]);
                        SW_netCDFIn->inVarInfo[key][varIndex][INVARUNITS] =
                            Str_Dup(
                                SW_netCDFIn->units_sw[key][varIndex], LogInfo
                            );
                    }
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

    SW_NCIN_allocate_startEndYrs(ncWeatherInStartEndYrs, numWeathIn, LogInfo);
}

/**
@brief Allocate weather start and end years

@param[out] ncWeatherInStartEndYrs Start/end years of each weather input file
@param[in] numWeathIn Number of input weather files
@param[out] LogInfo Holds information on warnings and errors
*/
void SW_NCIN_allocate_startEndYrs(
    unsigned int ***ncWeatherInStartEndYrs,
    unsigned int numWeathIn,
    LOG_INFO *LogInfo
) {
    unsigned int inFileNum;

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
@param[out] SW_WeatherIn Struct of type SW_WEATHER_INPUTS holding all relevant
information pretaining to meteorological input data
@param[out] LogInfo Holds information on warnings and errors
*/
void SW_NCIN_precalc_lookups(
    SW_DOMAIN *SW_Domain, SW_WEATHER_INPUTS *SW_WeatherIn, LOG_INFO *LogInfo
) {

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
        if (LogInfo->stopRun) {
            return;
        }

        get_weather_flags(SW_netCDFIn, SW_WeatherIn, LogInfo);
        if (LogInfo->stopRun) {
            return;
        }
#else
        (void) SW_WeatherIn;

        LogError(
            LogInfo,
            LOGERROR,
            "SWUDUNITS is not enabled, so we cannot calculate temporal "
            "information."
        );
        return;
#endif
    }

    get_invar_information(SW_netCDFIn, &SW_Domain->SW_PathInputs, LogInfo);
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
    char *indexVarNames[2] = {NULL, NULL};
    char *yDimName;
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
    char *domVarName =
        SW_Domain->netCDFInput.inVarInfo[eSW_InDomain][0][INNCVARNAME];
    unsigned int weatherFileIndex =
        SW_Domain->SW_PathInputs.weathStartFileIndex;
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
                yDimName = SW_netCDFIn->inVarInfo[k][fIndex][INYDIM];

                if (strcmp(yDimName, "NA") == 0) {
                    yDimName = yxVarNames[0];
                }

                if (k == eSW_InWeather) {
                    fileName = SW_Domain->SW_PathInputs
                                   .ncWeatherInFiles[fIndex][weatherFileIndex];
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

                inHasSite = SW_Domain->netCDFInput.siteDoms[k];

                get_index_vars_info(
                    ncFileID,
                    &indexVarNDims,
                    templateID,
                    domYName,
                    domXName,
                    dimIDs,
                    inHasSite,
                    varInfo[fIndex][INSITENAME],
                    indexVarNames,
                    domVarName,
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
                    yDimName,
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
                    SW_netCDFIn->projCoordConvs[k],
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
