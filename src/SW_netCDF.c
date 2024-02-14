#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netcdf.h>

#include "include/SW_netCDF.h"
#include "include/generic.h"
#include "include/filefuncs.h"
#include "include/SW_Defines.h"
#include "include/SW_Files.h"
#include "include/myMemory.h"
#include "include/Times.h"
#include "include/SW_Domain.h"
#include "include/SW_Output.h"
#include "include/SW_Output_outarray.h"

/* =================================================== */
/*                   Local Defines                     */
/* --------------------------------------------------- */

#define NUM_NC_IN_KEYS 2 // Number of possible keys within `files_nc.in`
#define NUM_ATT_IN_KEYS 27 // Number of possible keys within `attributes_nc.in`

#define PRGRSS_READY ((signed char)0) // SUID is ready for simulation
#define PRGRSS_DONE ((signed char)1) // SUID has successfully been simulated
#define PRGRSS_FAIL ((signed char)-1) // SUID failed to simulate

#define MAX_NUM_DIMS 5

const int times[] = {MAX_DAYS - 1, MAX_WEEKS, MAX_MONTHS, 1};

// Matches order of the defines within `SW_Output.h`
// Index `eSW_Estab` is modified from `SW_VES_read2()`,
// specifying how many output variable there will be depending on the run
int numVarsPerKey[] = {
    NWETHR_VARS, NTEMP_VARS, NPRECIP_VARS, NSOILINF_VARS, NRUNOFF_VARS,
    NALLH2O_VARS, NVWCBULK_VARS, NVWCMATRIC_VARS, NSWCBULK_VARS,
    NSWABULK_VARS, NSWAMATRIC_VARS, NSWA_VARS, NSWPMATRIC_VARS,
    NSURFACEW_VARS, NTRANSP_VARS, NEVAPSOIL_VARS, NEVAPSURFACE,
    NINTERCEPTION_VARS, NLYRDRAIN_VARS, NHYDRED_VARS, NET_VARS,
    NAET_VARS, NPET_VARS, NWETDAY_VARS, NSNOWPACK_VARS, NDEEPSWC_VARS,
    NSOILTEMP_VARS, NFROZEN_VARS, NALLVEG_VARS, NESTAB_VARS, NCO2EFFECTS_VARS,
    NBIOMASS_VARS
};

static const char* possKeys[][8] = {
    {NULL}, // WTHR
    {"TEMP__temp_max", "TEMP__temp_min", "TEMP__temp_avg", "TEMP__surfaceMax",
    "TEMP__surfaceMin", "TEMP__surfaceAvg"},

    {"PRECIP__ppt", "PRECIP__rain", "PRECIP__snow", "PRECIP__snowmelt",
        "PRECIP__snowloss"},

    {"SOILINFILT__soil_inf"},

    {"RUNOFF__net", "RUNOFF__surfaceRunoff", "RUNOFF__snowRunoff",
    "RUNOFF__surfaceRunon"},

    {NULL}, // ALLH2O

    // VWCBULK -> SURFACEWATER
    {"VWCBULK__vwcMatric"}, {"VWCMATRIC__vwcBulk"}, {"SWCBULK__swcBulk"},
    {"SWABULK__swaBulk"}, {"SWAMATRIC__swaMatric"}, {"SWA__SWA_VegType"},
    {"SWPMATRIC__swpMatric"}, {"SURFACEWATER__surfaceWater"},

    {"TRANSP__transp_total", "TRANSP__transp"},

    {"EVAPSOIL__evap_baresoil"},

    {"EVAPSURFACE__total_evap", "EVAPSURFACE__evap_veg", "EVAPSURFACE__litter_evap",
    "EVAPSURFACE__surfaceWater_evap"},

    {"INTERCEPTION__total_int", "INTERCEPTION__int_veg", "INTERCEPTION__litter_int"},

    {"LYRDRAIN__lyrdrain"},

    {"HYDRED__hydred_total", "HYDRED__hydred"},

    {NULL}, // ET

    {"AET__aet", "AET__tran", "AET__esoil", "AET__ecnw", "AET__esurf", "AET__snowloss"},

    {"PET__pet", "PET__H_oh", "PET__H_ot", "PET__H_gh", "PET__H_gt"},

    {"WETDAY__is_wet"},

    {"SNOWPACK__snowpack", "SNOWPACK__snowdepth"},

    {"DEEPSWC__deep"},

    {"SOILTEMP__maxLyrTemperature", "SOILTEMP__minLyrTemperature",
        "SOILTEMP__avgLyrTemp"},

    {"FROZEN__lyrFrozen"},

    {NULL}, // ALLVEG

    {NULL}, // ESTABL -- handled differently

    {"CO2EFFECTS__veg.co2_multipliers[BIO_INDEX]",
     "CO2EFFECTS__veg.co2_multipliers[WUE_INDEX]"},

    {"BIOMASS__bare_cov.fCover", "BIOMASS__veg.cov.fCover",
        "BIOMASS__biomass_total", "BIOMASS__veg.biomass_inveg",
        "BIOMASS__litter_total", "BIOMASS__biolive_total",
        "BIOMASS__veg.biolive_inveg", "BIOMASS__LAI"}
};

/* =================================================== */
/*             Local Function Definitions              */
/* --------------------------------------------------- */

/**
 * @brief Read invariant netCDF information (attributes/CRS) from input file
 *
 * @param[in,out] SW_netCDF Struct of type SW_NETCDF holding constant
 *  netCDF file information
 * @param[in,out] PathInfo Struct holding all information about the programs path/files
 * @param[out] LogInfo Holds information on warnings and errors
*/
static void nc_read_atts(SW_NETCDF* SW_netCDF, PATH_INFO* PathInfo,
                         LOG_INFO* LogInfo) {

    static const char* possibleKeys[NUM_ATT_IN_KEYS] = {
            "title", "author", "institution", "comment",
            "coordinate_system", "primary_crs",

            "geo_long_name", "geo_grid_mapping_name", "geo_crs_wkt",
            "geo_longitude_of_prime_meridian", "geo_semi_major_axis",
            "geo_inverse_flattening",

            "proj_long_name", "proj_grid_mapping_name", "proj_crs_wkt",
            "proj_longitude_of_prime_meridian", "proj_semi_major_axis",
            "proj_inverse_flattening", "proj_datum", "proj_units",
            "proj_standard_parallel", "proj_longitude_of_central_meridian",
            "proj_latitude_of_projection_origin", "proj_false_easting",
            "proj_false_northing",

            "strideOutYears", "baseCalendarYear"
            };
    static const Bool requiredKeys[NUM_ATT_IN_KEYS] =
            {swTRUE, swTRUE, swTRUE, swFALSE,
            swFALSE, swTRUE,
            swTRUE, swTRUE, swTRUE,
            swTRUE, swTRUE,
            swTRUE,
            swFALSE, swFALSE, swFALSE,
            swFALSE, swFALSE,
            swFALSE, swFALSE, swFALSE,
            swFALSE, swFALSE,
            swFALSE, swFALSE,
            swFALSE,
            swFALSE, swTRUE};
    Bool hasKeys[NUM_ATT_IN_KEYS] = {swFALSE};

    FILE *f;
    char inbuf[LARGE_VALUE], value[LARGE_VALUE];
    char key[35]; // 35 - Max key size
    char *MyFileName;
    int keyID;
    int n;
    float num1 = 0, num2 = 0;
    Bool geoCRSFound = swFALSE, projCRSFound = swFALSE;

    MyFileName = PathInfo->InFiles[eNCInAtt];
	f = OpenFile(MyFileName, "r", LogInfo);
    if(LogInfo->stopRun) {
        LogError(LogInfo, LOGERROR, "Could not open the required file %s",
                                    PathInfo->InFiles[eNCInAtt]);
        return; // Exit function prematurely due to error
    }

    while (GetALine(f, inbuf, LARGE_VALUE)) {
        sscanf(inbuf, "%34s %s", key, value);

        // Check if the key is "long_name" or "crs_wkt"
        if(strstr(key, "long_name") != NULL || strstr(key, "crs_wkt") != NULL) {

            // Reread the like and get the entire value (includes spaces)
            sscanf(inbuf, "%34s %[^\n]", key, value);
        }

        keyID = key_to_id(key, possibleKeys, NUM_ATT_IN_KEYS);
        set_hasKey(keyID, possibleKeys, hasKeys, LogInfo); // no error, only warnings possible

        switch(keyID)
        {
            case 0:
                SW_netCDF->title = Str_Dup(value, LogInfo);
                break;
            case 1:
                SW_netCDF->author = Str_Dup(value, LogInfo);
                break;
            case 2:
                SW_netCDF->institution = Str_Dup(value, LogInfo);
                break;
            case 3:
                SW_netCDF->comment = Str_Dup(value, LogInfo);
                break;
            case 4: // coordinate_system is calculated
                break;

            case 5:
                if(strcmp(value, "geographic") == 0) {
                    SW_netCDF->primary_crs_is_geographic = swTRUE;
                } else if(strcmp(value, "projected") == 0) {
                    SW_netCDF->primary_crs_is_geographic = swFALSE;
                } else {
                    LogError(LogInfo, LOGERROR, "The read-in primary CRS "
                             "(%s) is not a valid one. Please choose between "
                             "geographic and projected.", value);
                    CloseFile(&f, LogInfo);
                    return; // Exit function prematurely due to error
                }
                break;
            case 6:
                SW_netCDF->crs_geogsc.long_name = Str_Dup(value, LogInfo);
                geoCRSFound = swTRUE;
                break;
            case 7:
                SW_netCDF->crs_geogsc.grid_mapping_name = Str_Dup(value, LogInfo);
                break;
            case 8:
                SW_netCDF->crs_geogsc.crs_wkt = Str_Dup(value, LogInfo);
                break;
            case 9:
                SW_netCDF->crs_geogsc.longitude_of_prime_meridian = atof(value);
                break;
            case 10:
                SW_netCDF->crs_geogsc.semi_major_axis = atof(value);
                break;
            case 11:
                SW_netCDF->crs_geogsc.inverse_flattening = atof(value);
                break;
            case 12:
                SW_netCDF->crs_projsc.long_name = Str_Dup(value, LogInfo);
                projCRSFound = swTRUE;
                break;
            case 13:
                SW_netCDF->crs_projsc.grid_mapping_name = Str_Dup(value, LogInfo);
                break;
            case 14:
                SW_netCDF->crs_projsc.crs_wkt = Str_Dup(value, LogInfo);
                break;
            case 15:
                SW_netCDF->crs_projsc.longitude_of_prime_meridian = atof(value);
                break;
            case 16:
                SW_netCDF->crs_projsc.semi_major_axis = atof(value);
                break;
            case 17:
                SW_netCDF->crs_projsc.inverse_flattening = atof(value);
                break;
            case 18:
                SW_netCDF->crs_projsc.datum = Str_Dup(value, LogInfo);
                break;
            case 19:
                SW_netCDF->crs_projsc.units = Str_Dup(value, LogInfo);
                break;
            case 20:
                // Re-scan for 1 or 2 values of standard parallel(s)
                // the user may separate values by white-space, comma, etc.
                n = sscanf(inbuf, "%34s %f%*[^-.0123456789]%f", key, &num1, &num2);

                SW_netCDF->crs_projsc.standard_parallel[0] = num1;
                SW_netCDF->crs_projsc.standard_parallel[1] = (n == 3) ? num2 : NAN;
                break;
            case 21:
                SW_netCDF->crs_projsc.longitude_of_central_meridian = atof(value);
                break;
            case 22:
                SW_netCDF->crs_projsc.latitude_of_projection_origin = atof(value);
                break;
            case 23:
                SW_netCDF->crs_projsc.false_easting = atoi(value);
                break;
            case 24:
                SW_netCDF->crs_projsc.false_northing = atoi(value);
                break;
            case 25:
                if(Str_CompareI(value, (char *) "Inf") != 0) {
                    SW_netCDF->strideOutYears = atoi(value);

                    if(SW_netCDF->strideOutYears <= 0) {
                        LogError(LogInfo, LOGERROR, "The value for 'strideOutYears' <= 0");
                        return; // Exit function due to invalid input
                    }
                }
                break;
            case 26:
                SW_netCDF->baseCalendarYear = atoi(value);
                break;
            case KEY_NOT_FOUND:
                LogError(LogInfo, LOGWARN, "Ignoring unknown key in %s - %s",
                                            MyFileName, key);
                break;
        }

        if(LogInfo->stopRun) {
            CloseFile(&f, LogInfo);
            return; // Exist function prematurely due to error
        }
    }

    CloseFile(&f, LogInfo);


    // Check if all required input was provided
    check_requiredKeys(hasKeys, requiredKeys, possibleKeys, NUM_ATT_IN_KEYS, LogInfo);
    if(LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }


    if (
        (SW_netCDF->primary_crs_is_geographic && !geoCRSFound) ||
        (!SW_netCDF->primary_crs_is_geographic && !projCRSFound)
    ) {
        LogError(
            LogInfo,
            LOGERROR,
            "'%s': type of primary CRS is '%s' but "
            "attributes (including '*_long_name') for such a CRS are missing.",
            PathInfo->InFiles[eNCInAtt],
            (SW_netCDF->primary_crs_is_geographic) ? "geographic" : "projected"
        );
        return; // Exit function prematurely due to error
    }

    if(projCRSFound && !geoCRSFound) {
        LogError(LogInfo, LOGERROR, "Program found a projected CRS "
                                    "in %s but not a geographic CRS. "
                                    "SOILWAT2 requires either a primary CRS of "
                                    "type 'geographic' CRS or a primary CRS of "
                                    "'projected' with a geographic CRS.",
                                    PathInfo->InFiles[eNCInAtt]);
        return; // Exit function prematurely due to error
    }

    SW_netCDF->coordinate_system = (SW_netCDF->primary_crs_is_geographic) ?
        Str_Dup(SW_netCDF->crs_geogsc.long_name, LogInfo) :
        Str_Dup(SW_netCDF->crs_projsc.long_name, LogInfo);
}

/**
 * @brief Convert a read-in key (<OUTKEY>__<SW2 Variable Name>) into
 *  it's respective numeric values
*/
static void get_2d_output_key(char* varKey, OutKey* outKey, int* outVarNum) {

    int k, varNum;
    const int establSize = 6;

    *outKey = eSW_NoKey;
    *outVarNum = KEY_NOT_FOUND;

    if(strncmp(varKey, SW_ESTAB, establSize) == 0) {
        *outKey = eSW_Estab;
    } else {
        ForEachOutKey(k) {
            if(k != eSW_Estab) {
                for(varNum = 0; varNum < numVarsPerKey[k]; varNum++) {
                    if(strcmp(possKeys[k][varNum], varKey) == 0) {

                        *outKey = (OutKey) k;
                        *outVarNum = varNum;

                        return;
                    }
                }
            }
        }
    }
}

/**
 * @brief Get a dimension identifier within a given netCDF
 *
 * @param[in] ncFileID Identifier of the open netCDF file to access
 * @param[in] dimName Name of the new dimension
 * @param[out] dimID Identifier of the dimension
 * @param[out] LogInfo Holds information on warnings and errors
*/
static void get_dim_identifier(int ncFileID, const char* dimName, int* dimID,
                               LOG_INFO* LogInfo) {

    if(nc_inq_dimid(ncFileID, dimName, dimID) != NC_NOERR) {
        LogError(LogInfo, LOGERROR, "An error occurred attempting to "
                                    "retrieve the dimension identifier "
                                    "of the dimension %s.",
                                    dimName);
    }
}

/**
 * @brief Get a variable identifier within a given netCDF
 *
 * @param[in] ncFileID Identifier of the open netCDF file to access
 * @param[in] varName Name of the new variable
 * @param[out] varID Identifier of the variable
 * @param[out] LogInfo Holds information on warnings and errors
*/
static void get_var_identifier(int ncFileID, const char* varName, int* varID,
                               LOG_INFO* LogInfo) {

    int callRes = nc_inq_varid(ncFileID, varName, varID);

    if(callRes == NC_ENOTVAR) {
        LogError(LogInfo, LOGERROR, "Could not find variable %s.",
                                    varName);
    }

}

/**
 * @brief Get a string value from an attribute
 *
 * @param[in] ncFileID Identifier of the open netCDF file to test
 * @param[in] varName Name of the variable to access
 * @param[in] attName Name of the attribute to access
 * @param[out] strVal String buffer to hold the resulting value
 * @param[out] LogInfo Holds information on warnings and errors
*/
static void get_str_att_val(int ncFileID, const char* varName,
                            const char* attName, char* strVal,
                            LOG_INFO* LogInfo) {

    int varID = 0, attCallRes, attLenCallRes;
    size_t attLen = 0;
    get_var_identifier(ncFileID, varName, &varID, LogInfo);
    if(LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

    attLenCallRes = nc_inq_attlen(ncFileID, varID, attName, &attLen);
    if(attLenCallRes == NC_ENOTATT) {
        LogError(LogInfo, LOGERROR, "Could not find an attribute %s "
                                    "for the variable %s.",
                                    attName, varName);
        return; // Exit function prematurely due to error
    } else if(attLenCallRes != NC_NOERR) {
        LogError(LogInfo, LOGERROR, "An error occurred when attempting to "
                                    "get the length of the value of the "
                                    "attribute %s.", attName);
    }

    attCallRes = nc_get_att_text(ncFileID, varID, attName, strVal);
    if(attCallRes != NC_NOERR) {
        LogError(LogInfo, LOGERROR, "An error occurred when attempting to "
                                    "access the attribute %s of variable %s.",
                                    attName, varName);
        return; // Exit function prematurely due to error
    }

    strVal[attLen] = '\0';
}

/**
 * @brief Get a double value from an attribute
 *
 * @param[in] ncFileID Identifier of the open netCDF file to test
 * @param[in] varName Name of the variable to access
 * @param[in] attName Name of the attribute to access
 * @param[out] attVal String buffer to hold the resulting value
 * @param[out] LogInfo Holds information on warnings and errors
*/
static void get_double_att_val(int ncFileID, const char* varName,
                               const char* attName, double* attVal,
                               LOG_INFO* LogInfo) {

    int varID = 0, attCallRes;
    get_var_identifier(ncFileID, varName, &varID, LogInfo);
    if(LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

    attCallRes = nc_get_att_double(ncFileID, varID, attName, attVal);
    if(attCallRes == NC_ENOTATT) {
        LogError(LogInfo, LOGERROR, "Could not find an attribute %s "
                                    "for the variable %s.",
                                    attName, varName);
    } else if(attCallRes != NC_NOERR) {
        LogError(LogInfo, LOGERROR, "An error occurred when attempting to "
                                    "access the attribute %s of variable %s.",
                                    attName, varName);
    }
}

/**
 * @brief Get a double value from a variable
 *
 * @param[in] ncFileID Identifier of the open netCDF file to access
 * @param[in] varName Name of the variable to access
 * @param[in] index Location of the value within the variable
 * @param[out] value String buffer to hold the resulting value
 * @param[out] LogInfo Holds information on warnings and errors
*/
static void get_single_double_val(int ncFileID, const char* varName,
                                  size_t index[], double* value,
                                  LOG_INFO* LogInfo) {
    int varID = 0;
    get_var_identifier(ncFileID, varName, &varID, LogInfo);
    if(LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

    if(nc_get_var1_double(ncFileID, varID, index, value) != NC_NOERR) {
        LogError(LogInfo, LOGERROR, "An error occurred when trying to "
                                    "get a value from variable %s.",
                                    varName);
    }
}

/**
 * @brief Get an unsigned integer value from a variable
 *
 * @param[in] ncFileID Identifier of the open netCDF file to access
 * @param[in] varID Identifier of the variable
 * @param[in] index Location of the value within the variable
 * @param[out] value String buffer to hold the resulting value
 * @param[in,out] LogInfo Holds information dealing with logfile output
*/
static void get_single_uint_val(int ncFileID, int varID, size_t index[],
                                unsigned int* value, LOG_INFO* LogInfo) {

    if(nc_get_var1_uint(ncFileID, varID, index, value) != NC_NOERR) {
        LogError(LogInfo, LOGERROR, "An error occurred when trying to "
                                    "get a value from a variable of type "
                                    "unsigned integer.");
    }
}

/**
 * @brief Get a byte value from a variable
 *
 * @param[in] ncFileID Identifier of the open netCDF file to access
 * @param[in] varID Identifier of the variable
 * @param[in] varName Name of the variable to access
 * @param[in] index Location of the value within the variable
 * @param[out] value String buffer to hold the resulting value
 * @param[in,out] LogInfo Holds information dealing with logfile output
*/
static void get_single_byte_val(int ncFileID, int varID, size_t index[],
                                signed char* value, LOG_INFO* LogInfo) {

    if(nc_get_var1_schar(ncFileID, varID, index, value) != NC_NOERR) {
        LogError(LogInfo, LOGERROR, "An error occurred when trying to "
                                    "get a value from a variable of type byte");
    }
}

/**
 * @brief Get a dimension value from a given netCDF file
 *
 * @param[in] ncFileID Identifier of the open netCDF file to access
 * @param[in] dimName Name of the dimension to access
 * @param[out] dimVal String buffer to hold the resulting value
 * @param[out] LogInfo Holds information on warnings and errors
*/
static void get_dim_val(int ncFileID, const char* dimName, size_t* dimVal,
                        LOG_INFO* LogInfo) {

    int dimID = 0;

    get_dim_identifier(ncFileID, dimName, &dimID, LogInfo);
    if(LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

    if(nc_inq_dimlen(ncFileID, dimID, dimVal) != NC_NOERR) {
        LogError(LogInfo, LOGERROR, "An error occurred when attempting "
                                    "to retrieve the dimension value of "
                                    "%s.", dimName);
    }
}

/**
 * @brief Determine if a given CRS name is wgs84
 *
 * @param[in] crs_name Name of the CRS to test
*/
static Bool is_wgs84(char* crs_name) {
    const int numPosSyns = 5;
    static char* wgs84_synonyms[] = {
        (char *)"WGS84", (char *)"WGS 84", (char *)"EPSG:4326",
        (char *)"WGS_1984", (char *)"World Geodetic System 1984"
    };

    for (int index = 0; index < numPosSyns; index++) {
        if (Str_CompareI(crs_name, wgs84_synonyms[index]) == 0) {
                return swTRUE;
        }
    }

    return swFALSE;
}

/**
 * @brief Checks to see if a given netCDF has a specific dimension
 *
 * @param[in] targetDim Dimension name to test for
 * @param[in] ncFileID Identifier of the open netCDF file to test
 *
 * @return Whether or not the given dimension name exists in the netCDF file
*/
static Bool dimExists(const char* targetDim, int ncFileID) {

    int dimID; // Not used

    // Attempt to get the dimension identifier
    int inquireRes = nc_inq_dimid(ncFileID, targetDim, &dimID);

    return (Bool) (inquireRes != NC_EBADDIM);
}

/**
 * @brief Check if a variable exists within a given netCDF and does not
 *  throw an error if anything goes wrong
 *
 * @param[in] ncFileID Identifier of the open netCDF file to test
 * @param[in] varName Name of the variable to test for
 *
 * @return Whether or not the given variable name exists in the netCDF file
*/
static Bool varExists(int ncFileID, const char* varName) {
    int varID;

    int inquireRes = nc_inq_varid(ncFileID, varName, &varID);

    return (Bool) (inquireRes != NC_ENOTVAR);
}



/**
 * @brief Fills a variable with value(s) of type unsigned integer
 *
 * @param[in] ncFileID Identifier of the open netCDF file to write the value(s) to
 * @param[in] varID Identifier to the variable within the given netCDF file
 * @param[in] values Individual or list of input variables
 * @param[in] startIndices Specification of where the C-provided netCDF
 *  should start writing values within the specified variable
 * @param[in] count How many values to write into the given variable
 * @param[out] LogInfo Holds information on warnings and errors
*/
static void fill_netCDF_var_uint(int ncFileID, int varID, unsigned int values[],
                                 size_t startIndices[], size_t count[],
                                 LOG_INFO* LogInfo) {

    if(nc_put_vara_uint(ncFileID, varID, startIndices, count, &values[0]) != NC_NOERR) {
        LogError(LogInfo, LOGERROR, "Could not fill variable (unsigned int) "
                                    "with the given value(s).");
    }
}

/**
 * @brief Fills a variable with value(s) of type byte
 *
 * @param[in] ncFileID Identifier of the open netCDF file to write the value(s) to
 * @param[in] varID Identifier to the variable within the given netCDF file
 * @param[in] values Individual or list of input variables
 * @param[in] startIndices Specification of where the C-provided netCDF
 *  should start writing values within the specified variable
 * @param[in] count How many values to write into the given variable
 * @param[out] LogInfo Holds information on warnings and errors
*/
static void fill_netCDF_var_byte(int ncFileID, int varID, const signed char values[],
                                 size_t startIndices[], size_t count[],
                                 LOG_INFO* LogInfo) {

    if(nc_put_vara_schar(ncFileID, varID, startIndices, count, &values[0]) != NC_NOERR) {
        LogError(LogInfo, LOGERROR, "Could not fill variable (byte) "
                                    "with the given value(s).");
    }
}

/**
 * @brief Fills a variable with value(s) of type double
 *
 * @param[in] ncFileID Identifier of the open netCDF file to write the value(s) to
 * @param[in] varID Identifier to the variable within the given netCDF file
 * @param[in] values Individual or list of input variables
 * @param[in] startIndices Specification of where the C-provided netCDF
 *  should start writing values within the specified variable
 * @param[in] count How many values to write into the given variable
 * @param[out] LogInfo Holds information on warnings and errors
*/
static void fill_netCDF_var_double(int ncFileID, int varID, double values[],
                                   size_t startIndices[], size_t count[],
                                   LOG_INFO* LogInfo) {

    if(nc_put_vara_double(ncFileID, varID, startIndices, count, &values[0]) != NC_NOERR) {
        LogError(LogInfo, LOGERROR, "Could not fill variable (double) "
                                    "with the given value(s).");
    }
}

/**
 * @brief Write a local attribute of type byte
 *
 * @param[in] attName Name of the attribute to create
 * @param[in] attVal Attribute value(s) to write out
 * @param[in] varID Identifier of the variable to add the attribute to
 * @param[in] ncFileID Identifier of the open netCDF file to write the attribute to
 * @param[in] numVals Number of values to write to the single attribute
 * @param[out] LogInfo Holds information on warnings and errors
*/
static void write_byte_att(const char* attName, const signed char* attVal,
                           int varID, int ncFileID, int numVals, LOG_INFO* LogInfo) {

    if(nc_put_att_schar(ncFileID, varID, attName, NC_BYTE, numVals, attVal) != NC_NOERR) {
        LogError(LogInfo, LOGERROR, "Could not create new attribute %s",
                                    attName);
    }
}

/**
 * @brief Write a global attribute (text) to a netCDF file
 *
 * @param[in] attName Name of the attribute to create
 * @param[in] attVal Attribute string to write out
 * @param[in] varID Identifier of the variable to add the attribute to
 *  (Note: NC_GLOBAL is acceptable and is a global attribute of the netCDF file)
 * @param[in] ncFileID Identifier of the open netCDF file to write the attribute to
 * @param[out] LogInfo Holds information on warnings and errors
*/
static void write_uint_att(const char* attName, unsigned int attVal,
                          int varID, int ncFileID, LOG_INFO* LogInfo) {

    if(nc_put_att_uint(ncFileID, varID, attName, NC_UINT, 1, &attVal) != NC_NOERR) {
        LogError(LogInfo, LOGERROR, "Could not create new global attribute %s",
                                    attName);
    }
}

/**
 * @brief Write a global attribute (text) to a netCDF file
 *
 * @param[in] attName Name of the attribute to create
 * @param[in] attStr Attribute string to write out
 * @param[in] varID Identifier of the variable to add the attribute to
 *  (Note: NC_GLOBAL is acceptable and is a global attribute of the netCDF file)
 * @param[in] ncFileID Identifier of the open netCDF file to write the attribute to
 * @param[out] LogInfo Holds information on warnings and errors
*/
static void write_str_att(const char* attName, const char* attStr,
                          int varID, int ncFileID, LOG_INFO* LogInfo) {

    if(nc_put_att_text(ncFileID, varID, attName, strlen(attStr), attStr) != NC_NOERR) {
        LogError(LogInfo, LOGERROR, "Could not create new global attribute %s",
                                    attName);
    }
}

/**
 * @brief Write an attribute of type double to a variable
 *
 * @param[in] attName Name of the attribute to create
 * @param[in] attVal Attribute value(s) to write out
 * @param[in] varID Identifier of the variable to add the attribute to
 *  (Note: NC_GLOBAL is acceptable and is a global attribute of the netCDF file)
 * @param[in] ncFileID Identifier of the open netCDF file to write the attribute to
 * @param[in] numVals Number of values to write to the single attribute
 * @param[out] LogInfo Holds information on warnings and errors
*/
static void write_double_att(const char* attName, const double* attVal, int varID,
                        int ncFileID, int numVals, LOG_INFO* LogInfo) {

    if(nc_put_att_double(ncFileID, varID, attName, NC_DOUBLE, numVals, attVal) != NC_NOERR) {
        LogError(LogInfo, LOGERROR, "Could not create new global attribute %s",
                                    attName);
    }
}

/**
 * @brief Write values to a variable of type double
 *
 * @param[in] varName Name of the attribute to create
 * @param[in] varVals Attribute value(s) to write out
 * @param[in] ncFileID Identifier of the open netCDF file to write the attribute to
 * @param[out] LogInfo Holds information on warnings and errors
*/
static void write_double_vals(const char* varName, const double* varVals,
        int ncFileID, size_t start[], size_t count[], LOG_INFO* LogInfo) {

    int varID = 0;

    get_var_identifier(ncFileID, varName, &varID, LogInfo);
    if(LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

    if(nc_put_vara_double(ncFileID, varID, start, count, varVals) != NC_NOERR) {
        LogError(LogInfo, LOGERROR, "Could not write values to the variable %s",
                                    varName);
    }
}

/**
 * @brief Write values to a variable of type string
 *
 * @param[in] ncFileID Identifier of the open netCDF file to write the attribute to
 * @param[in] varVals Attribute value(s) to write out
 * @param[out] LogInfo Holds information on warnings and errors
*/
static void write_string_vals(int ncFileID, int varID, const char **varVals,
                              LOG_INFO* LogInfo) {


    if(nc_put_var_string(ncFileID, varID, &varVals[0]) != NC_NOERR) {
        LogError(LogInfo, LOGERROR, "Could not write string values.");
    }
}

/**
 * @brief Fill the progress variable in the progress netCDF with values
 *
 * @param[in] SW_Domain Struct of type SW_DOMAIN holding constant
 *  temporal/spatial information for a set of simulation runs
 * @param[in,out] LogInfo Holds information on warnings and errors
*/
static void fill_prog_netCDF_vals(SW_DOMAIN* SW_Domain, LOG_INFO* LogInfo) {

    int domVarID = SW_Domain->netCDFInfo.ncVarIDs[vNCdom];
    int progVarID = SW_Domain->netCDFInfo.ncVarIDs[vNCprog];
    unsigned int domStatus;
    unsigned long suid, ncSuid[2], nSUIDs = SW_Domain->nSUIDs;
    unsigned long nDimY = SW_Domain->nDimY, nDimX = SW_Domain->nDimX;
    int progFileID = SW_Domain->netCDFInfo.ncFileIDs[vNCprog];
    int domFileID = SW_Domain->netCDFInfo.ncFileIDs[vNCdom];
    size_t start1D[] = {0}, start2D[] = {0, 0};
    size_t count1D[] = {nSUIDs}, count2D[] = {nDimY, nDimX};
    size_t* start = (strcmp(SW_Domain->DomainType, "s") == 0) ? start1D : start2D;
    size_t* count = (strcmp(SW_Domain->DomainType, "s") == 0) ? count1D : count2D;

    signed char* vals = (signed char*)Mem_Malloc(nSUIDs * sizeof(signed char),
                                                 "fill_prog_netCDF_vals()",
                                                 LogInfo);
    if(LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

    for(suid = 0; suid < nSUIDs; suid++) {
        SW_DOM_calc_ncSuid(SW_Domain, suid, ncSuid);

        get_single_uint_val(domFileID, domVarID, ncSuid, &domStatus, LogInfo);
        if(LogInfo->stopRun) {
            goto freeMem; // Exit function prematurely due to error
        }

        vals[suid] = (domStatus == NC_FILL_UINT) ? NC_FILL_BYTE : PRGRSS_READY;
    }

    fill_netCDF_var_byte(progFileID, progVarID, vals, start, count, LogInfo);
    nc_sync(progFileID);

    // Free allocated memory
    freeMem: {
        free(vals);
    }
}

/**
 * @brief Create a dimension within a netCDF file
 *
 * @param[in] dimName Name of the new dimension
 * @param[in] size Value/size of the dimension
 * @param[in] ncFileID Domain netCDF file ID
 * @param[in,out] dimID Dimension ID
 * @param[out] LogInfo Holds information on warnings and errors
*/
static void create_netCDF_dim(const char* dimName, unsigned long size,
                    int* ncFileID, int* dimID, LOG_INFO* LogInfo) {

    if(nc_def_dim(*ncFileID, dimName, size, dimID) != NC_NOERR) {
        LogError(LogInfo, LOGERROR, "Could not create dimension '%s' in "
                                    "netCDF.", dimName);
    }
}

/**
 * @brief Create a variable within a netCDF file
 *
 * @param[out] varID Variable ID within the netCDF
 * @param[in] varName Name of the new variable
 * @param[in] dimIDs Dimensions of the variable
 * @param[in] ncFileID Domain netCDF file ID
 * @param[in] varType The type in which the new variable will be
 * @param[in] numDims Number of dimensions the new variable will hold
 * @param[out] LogInfo Holds information on warnings and errors
*/
static void create_netCDF_var(int* varID, const char* varName, int* dimIDs,
                int* ncFileID, int varType, int numDims, LOG_INFO* LogInfo) {

    // Deflate information
    int shuffle = 1, deflate = 1; // 0 or 1
    int level = 5; // 0 to 9

    if(nc_def_var(*ncFileID, varName, varType, numDims, dimIDs, varID) != NC_NOERR) {
        LogError(LogInfo, LOGERROR, "Could not create '%s' variable in "
                                    "netCDF.", varName);
        return; // Exit prematurely due to error
    }

    // Do not compress the CRS variables
    if(strcmp(varName, "crs_geogsc") != 0 && strcmp(varName, "crs_projsc") != 0 &&
        varType != NC_STRING) {

        if(nc_def_var_deflate(*ncFileID, *varID, shuffle, deflate, level) != NC_NOERR) {
            LogError(LogInfo, LOGERROR, "An error occurred when attempting to "
                                        "deflate the variable %s", varName);
        }
    }
}


/**
 * @brief Helper function to `fill_domain_netCDF_vars()` to free memory
 *  that was allocated for the domain netCDF file variable values
 *
 * @param[out] valsY Array to hold coordinate values for the Y-axis (lat or y)
 * @param[out] valsX Array to hold coordinate values for the X-axis (lon or x)
 * @param[out] valsYBnds Array to hold coordinate values for the
 *   bounds of the Y-axis (lat_bnds or y_bnds)
 * @param[out] valsXBnds Array to hold coordinate values for the
 *   bounds of the X-axis (lon_bnds or x_bnds)
 * @param[out] domVals Array to hold all values for the domain variable
*/
static void dealloc_netCDF_domain_vars(double **valsY, double **valsX,
            double **valsYBnds, double **valsXBnds, unsigned int **domVals) {

    const int numVars = 4;
    double **vars[] = {valsY, valsX, valsYBnds, valsXBnds};
    int varNum;

    for(varNum = 0; varNum < numVars; varNum++) {
        if(!isnull(*vars[varNum])) {
            free(*vars[varNum]);
        }
    }

    if(!isnull(domVals)) {
        free(*domVals);
    }
}

/**
 * @brief Wrapper function to initialize output request variables
 *  and output variable information
 *
 * @param[out] SW_Output SW_OUTPUT array of size SW_OUTNKEYS which holds
 *  basic output information for all output keys
 * @param[out] LogInfo Holds information on warnings and errors
*/
static void init_output_var_info(SW_OUTPUT* SW_Output, LOG_INFO* LogInfo) {
    int key;

    ForEachOutKey(key) {
        SW_NC_init_outReq(&SW_Output[key].reqOutputVars, (OutKey)key, LogInfo);
        if(LogInfo->stopRun) {
            return; // Exit function prematurely due to error
        }

        SW_Output[key].outputVarInfo = NULL;
    }
}

/**
 * @brief Helper function to `fill_domain_netCDF_vars()` to allocate
 *  memory for writing out values
 *
 * @param[in] domTypeIsSite Specifies if the domain type is "s"
 * @param[in] nSUIDs Number of sites or gridcells
 * @param[in] numY Number of values along the Y-axis (lat or y)
 * @param[in] numX Number of values along the X-axis (lon or x)
 * @param[out] valsY Array to hold coordinate values for the Y-axis (lat or y)
 * @param[out] valsX Array to hold coordinate values for the X-axis (lon or x)
 * @param[out] valsYBnds Array to hold coordinate values for the
 *   bounds of the Y-axis (lat_bnds or y_bnds)
 * @param[out] valsXBnds Array to hold coordinate values for the
 *   bounds of the X-axis (lon_bnds or x_bnds)
 * @param[out] domVals Array to hold all values for the domain variable
 * @param[out] LogInfo Holds information on warnings and errors
*/
static void alloc_netCDF_domain_vars(Bool domTypeIsSite,
            int nSUIDs, unsigned int numY, unsigned int numX,
            double **valsY, double **valsX,
            double **valsYBnds, double **valsXBnds,
            unsigned int **domVals, LOG_INFO *LogInfo) {

    double **vars[] = {valsY, valsX};
    double **bndsVars[] = {valsYBnds, valsXBnds};
    const int numVars = 2, numBnds = 2;
    int varNum, bndVarNum, numVals;

    for(varNum = 0; varNum < numVars; varNum++) {
        numVals = (varNum % 2 == 0) ? numY : numX;
        *(vars[varNum]) = (double *) Mem_Malloc(numVals * sizeof(double),
                                                "alloc_netCDF_domain_vars()",
                                                LogInfo);
        if(LogInfo->stopRun) {
            return; // Exit function prematurely due to error
        }
    }

    if(!domTypeIsSite) {
        for(bndVarNum = 0; bndVarNum < numBnds; bndVarNum++) {
            numVals = (bndVarNum % 2 == 0) ? numY : numX;

            *(bndsVars[bndVarNum]) =
                    (double *) Mem_Malloc(numVals * numBnds * sizeof(double),
                                "alloc_netCDF_domain_vars()",
                                LogInfo);

            if(LogInfo->stopRun) {
                return; // Exit function prematurely due to error
            }
        }
    }

    *domVals = (unsigned int*) Mem_Malloc(nSUIDs * sizeof(unsigned int),
                                         "alloc_netCDF_domain_vars()",
                                         LogInfo);
}

/**
 * @brief Fill horizontal coordinate variables, "domain"
 *  (or another user-specified name) variable and "sites" (if applicable)
 *  within the domain netCDF
 *
 * @note A more descriptive explanation of the scenarios to write out the
 *  values of a variable would be
 *
 * CRS bounding box - geographic
 *      - Domain type "xy" (primary CRS - geographic & projected)
 *          - Write out values for "lat", "lat_bnds", "lon", and "lon_bnds"
 *      - Domain type "s" (primary CRS - geographic & projected)
 *          - Write out values for "site", "lat", and "lon"
 *
 * CRS bounding box - projected
 *      - Domain type "xy" (primary CRS - geographic)
 *          - Cannot occur
 *      - Domain type "xy" (primary CRS - projected)
 *          - Write out values for "y", "y_bnds", "x", and "x_bnds"
 *      - Domain type "s" (primary CRS - geographic)
 *          - Cannot occur
 *      - Domain type "s" (primary CRS - projected)
 *          - Write out values for "site", "y", and "x"
 *
 *  While all scenarios fill the domain variable
 *
 * @param[in] SW_Domain Struct of type SW_DOMAIN holding constant
 *  temporal/spatial information for a set of simulation runs
 * @param[in] domFileID Domain netCDF file identifier
 * @param[in] domID Identifier of the "domain" (or another user-specified name) variable
 * @param[in] siteID Identifier of the "site" variable within the given netCDF
 *  (if the variable exists)
 * @param[in] YVarID Variable identifier of the Y-axis horizontal coordinate
 *   variable (lat or y)
 * @param[in] XVarID Variable identifier of the X-axis horizontal coordinate
 *   variable (lon or x)
 * @param[in] YBndsID Variable identifier of the Y-axis horizontal coordinate
 *   bounds variable (lat_bnds or y_bnds)
 * @param[in] XVarID Variable identifier of the X-axis horizontal coordinate
 *   bounds variable (lon_bnds or x_bnds)
 * @param[out] LogInfo Holds information on warnings and errors
*/
static void fill_domain_netCDF_vals(SW_DOMAIN* SW_Domain, int domFileID,
        int domID, int siteID, int YVarID, int XVarID,
        int YBndsID, int XBndsID, LOG_INFO* LogInfo) {

    Bool domTypeIsSite = (Bool)(strcmp(SW_Domain->DomainType, "s") == 0);
    unsigned int suidNum, gridNum = 0, *domVals = NULL, bndsIndex;
    double *valsY = NULL, *valsX = NULL;
    double *valsYBnds = NULL, *valsXBnds = NULL;
    size_t start[] = {0, 0}, domCount[2]; // 2 - [#lat dim, #lon dim] or [#sites, 0]
    size_t fillCountY[1], fillCountX[1];
    size_t fillCountYBnds[] = {SW_Domain->nDimY, 2};
    size_t fillCountXBnds[] = {SW_Domain->nDimX, 2};
    double resY, resX;
    unsigned int numX = (domTypeIsSite) ? SW_Domain->nDimS : SW_Domain->nDimX;
    unsigned int numY = (domTypeIsSite) ? SW_Domain->nDimS : SW_Domain->nDimY;

    int fillVarIDs[4] = {YVarID, XVarID}; // Fill other slots later if needed
    double **fillVals[4] = {&valsY, &valsX}; // Fill other slots later if needed
    size_t *fillCounts[] = {fillCountY, fillCountX, fillCountYBnds,
                            fillCountXBnds};
    int numVars, varNum;

    alloc_netCDF_domain_vars(domTypeIsSite, SW_Domain->nSUIDs,
                             numY, numX, &valsY, &valsX,
                             &valsYBnds, &valsXBnds,
                             &domVals, LogInfo);
    if(LogInfo->stopRun) {
        goto freeMem; // Exit function prematurely due to error
    }

    for(suidNum = 0; suidNum < SW_Domain->nSUIDs; suidNum++) {
        domVals[suidNum] = suidNum + 1;
    }

    // --- Setup domain size, number of variables to fill, fill values,
    // fill variable IDs, and horizontal coordinate variables for the section
    // `Write out all values to file`
    if(domTypeIsSite) {
        // Handle variables differently due to domain being 1D (sites)
        fillCountY[0] = SW_Domain->nDimS;
        fillCountX[0] = SW_Domain->nDimS;
        domCount[0] = SW_Domain->nDimS;
        domCount[1] = 0;
        numVars = 2;

        // Sites are filled with the same values as the domain variable
        fill_netCDF_var_uint(domFileID, siteID, domVals, start, domCount, LogInfo);
        if(LogInfo->stopRun) {
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
    for(gridNum = 0; gridNum < numX; gridNum++) {
        valsX[gridNum] = SW_Domain->min_x + (gridNum + .5) * resX;

        if(!domTypeIsSite) {
            bndsIndex = gridNum * 2;
            valsXBnds[bndsIndex] = SW_Domain->min_x + gridNum * resX;
            valsXBnds[bndsIndex + 1] = SW_Domain->min_x + (gridNum + 1) * resX;
        }
    }

    for(gridNum = 0; gridNum < numY; gridNum++) {
        valsY[gridNum] = SW_Domain->min_y + (gridNum + .5) * resY;

        if(!domTypeIsSite) {
            bndsIndex = gridNum * 2;
            valsYBnds[bndsIndex] = SW_Domain->min_y + gridNum * resY;
            valsYBnds[bndsIndex + 1] = SW_Domain->min_y + (gridNum + 1) * resY;
        }
    }

    // --- Write out all values to file
    for(varNum = 0; varNum < numVars; varNum++) {
        fill_netCDF_var_double(domFileID, fillVarIDs[varNum], *fillVals[varNum],
                               start, fillCounts[varNum], LogInfo);

        if(LogInfo->stopRun) {
            goto freeMem; // Exit function prematurely due to error
        }
    }

    // Fill domain variable with SUIDs
    fill_netCDF_var_uint(domFileID, domID, domVals, start, domCount, LogInfo);

    // Free allocated memory
    freeMem: {
        dealloc_netCDF_domain_vars(&valsY, &valsX,
                                   &valsYBnds, &valsXBnds,
                                   &domVals);
    }
}

/**
 * @brief Fill the variable "domain" with it's attributes
 *
 * @param[in] domainVarName User-specified domain variable name
 * @param[in,out] domID Identifier of the "domain" (or another user-specified name) variable
 * @param[in] domDims Set dimensions for the domain variable
 * @param[in] domFileID Domain netCDF file identifier
 * @param[in] nDomainDims Number of dimensions the domain variable will have
 * @param[in] primCRSIsGeo Specifies if the current CRS type is geographic
 * @param[in] domType Type of domain in which simulations are running
 *  (gridcell/sites)
 * @param[out] LogInfo Holds information on warnings and errors
*/
static void fill_domain_netCDF_domain(const char* domainVarName, int* domID,
                        int domDims[], int domFileID, int nDomainDims, Bool primCRSIsGeo,
                        const char* domType, LOG_INFO* LogInfo) {

    const char* gridMapVal = (primCRSIsGeo) ? "crs_geogsc" : "crs_projsc: x y crs_geogsc: lat lon";
    const char* coordVal = (strcmp(domType, "s") == 0) ? "lat lon site" : "lat lon";
    const char* strAttNames[] = {"long_name", "units", "grid_mapping", "coordinates"};
    const char* strAttVals[] = {"simulation domain", "1", gridMapVal, coordVal};
    int attNum;
    const int numAtts = 4;

    create_netCDF_var(domID, domainVarName, domDims,
                      &domFileID, NC_UINT, nDomainDims, LogInfo);

    write_uint_att("_FillValue", NC_FILL_UINT, *domID, domFileID, LogInfo);
    if(LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

    // Write all attributes to the domain variable
    for(attNum = 0; attNum < numAtts; attNum++) {
        write_str_att(strAttNames[attNum], strAttVals[attNum], *domID, domFileID, LogInfo);

        if(LogInfo->stopRun) {
            return; // Exit function prematurely due to error
        }
    }
}

/**
 * @brief Fill the domain netCDF file with variables that are for domain type "s"
 *
 * @param[in] SW_Domain Struct of type SW_DOMAIN holding constant
 *  temporal/spatial information for a set of simulation runs
 * @param[in] domFileID Domain netCDF file ID
 * @param[out] sDimID 'site' dimension identifier
 * @param[out] sVarID 'site' variable identifier
 * @param[out] YVarID Variable identifier of the Y-axis horizontal coordinate
 *   variable (lat or y)
 * @param[out] XVarID Variable identifier of the X-axis horizontal coordinate
 *   variable (lon or x)
 * @param[out] LogInfo Holds information on warnings and errors
*/
static void fill_domain_netCDF_s(SW_DOMAIN* SW_Domain, int* domFileID,
                    int* sDimID, int* sVarID, int* YVarID, int* XVarID,
                    LOG_INFO* LogInfo) {

    char* geo_long_name = SW_Domain->netCDFInfo.crs_geogsc.long_name;
    char* proj_long_name = SW_Domain->netCDFInfo.crs_projsc.long_name;
    Bool primCRSIsGeo = SW_Domain->netCDFInfo.primary_crs_is_geographic;
    char* units = SW_Domain->netCDFInfo.crs_projsc.units;

    const int numSiteAtt = 3, numLatAtt = 4, numLonAtt = 4;
    const int numYAtt = 3, numXAtt = 3;
    int numVarsToWrite = (primCRSIsGeo) ? 3 : 5; // Do or do not write "x" and "y"
    const char* attNames[][4] = {
        {"long_name", "units", "cf_role"},
        {"long_name", "standard_name", "units", "axis"},
        {"long_name", "standard_name", "units", "axis"},
        {"long_name", "standard_name", "units"},
        {"long_name", "standard_name", "units"}
    };

    const char* attVals[][4] = {
        {"simulation site", "1", "timeseries_id"},
        {"latitude", "latitude", "degrees_north", "Y"},
        {"longitude", "longitude", "degrees_east", "X"},
        {"y coordinate of projection", "projection_y_coordinate", units},
        {"x coordinate of projection", "projection_x_coordinate", units}
    };

    const char* varNames[] = {"site", "lat", "lon", "y", "x"};
    int varIDs[5]; // 5 - Maximum number of variables to create
    const int numAtts[] = {numSiteAtt, numLatAtt, numLonAtt, numYAtt, numXAtt};

    int varNum, attNum;

    create_netCDF_dim("site", SW_Domain->nDimS, domFileID, sDimID, LogInfo);
    if(LogInfo->stopRun) {
        return; // Exit prematurely due to error
    }

    // Create all variables above (may or may not include "x" and "y")
    // Then fill them with their respective attributes
    for(varNum = 0; varNum < numVarsToWrite; varNum++) {
        create_netCDF_var(&varIDs[varNum], varNames[varNum],
                          sDimID, domFileID, NC_DOUBLE, 1, LogInfo);
        if(LogInfo->stopRun) {
            return; // Exit prematurely due to error
        }

        for(attNum = 0; attNum < numAtts[varNum]; attNum++) {
            write_str_att(attNames[varNum][attNum], attVals[varNum][attNum],
                          varIDs[varNum], *domFileID, LogInfo);
            if(LogInfo->stopRun) {
                return; // Exit function prematurely due to error
            }
        }
    }

    // Set site variable ID and lat/lon IDs created above depending on
    // which CRS long_name matches "crs_bbox"
    *sVarID = varIDs[0];

    if(Str_CompareI(SW_Domain->crs_bbox, geo_long_name) == 0 ||
        (is_wgs84(SW_Domain->crs_bbox) && is_wgs84(geo_long_name))) {

        *YVarID = varIDs[1]; // lat
        *XVarID = varIDs[2]; // lon
    } else if(!primCRSIsGeo &&
              Str_CompareI(SW_Domain->crs_bbox, proj_long_name) == 0) {

        *YVarID = varIDs[3]; // y
        *XVarID = varIDs[4]; // x
    } else {
        LogError(LogInfo, LOGERROR, "The given bounding box name within the domain "
                    "input file does not match either the geographic or projected "
                    "CRS (if provided) in the netCDF attributes file. Please "
                    "make sure the bounding box name matches the desired "
                    "CRS 'long_name' that is to be filled.");
    }
}

/**
 * @brief Fill the domain netCDF file with variables that are for domain type "xy"
 *
 * @param[in] SW_Domain Struct of type SW_DOMAIN holding constant
 *  temporal/spatial information for a set of simulation runs
 * @param[in] domFileID Domain netCDF file ID
 * @param[out] YDimID Dimension identifier of the Y-axis horizontal coordinate
 *   dimension (lat or y)
 * @param[out] XVarID Dimension identifier of the X-axis horizontal coordinate
 *   dimension (lon or x)
 * @param[out] YVarID Variable identifier of the Y-axis horizontal coordinate
 *   variable (lat or y)
 * @param[out] XVarID Variable identifier of the X-axis horizontal coordinate
 *   variable (lon or x)
 * @param[out] YBndsID Variable identifier of the Y-axis horizontal coordinate
 *   bounds variable (lat_bnds or y_bnds)
 * @param[out] XVarID Variable identifier of the X-axis horizontal coordinate
 *   bounds variable (lon_bnds or x_bnds)
 * @param[out] LogInfo Holds information on warnings and errors
*/
static void fill_domain_netCDF_xy(SW_DOMAIN* SW_Domain, int* domFileID,
                    int* YDimID, int* XDimID, int* YVarID, int* XVarID,
                    int* YBndsID, int* XBndsID,
                    LOG_INFO* LogInfo) {

    char* geo_long_name = SW_Domain->netCDFInfo.crs_geogsc.long_name;
    char* proj_long_name = SW_Domain->netCDFInfo.crs_projsc.long_name;
    Bool primCRSIsGeo = SW_Domain->netCDFInfo.primary_crs_is_geographic;
    char* units = SW_Domain->netCDFInfo.crs_projsc.units;

    int bndsID = 0;
    int bndVarDims[2]; // Used for bound variables in the netCDF file
    int dimNum, varNum, attNum;

    const int numVars = (primCRSIsGeo) ? 2 : 4; // lat/lon or lat/lon + x/y vars
    const char* varNames[] = {"lat", "lon", "y", "x"};
    const char* bndVarNames[] = {"lat_bnds", "lon_bnds", "y_bnds", "x_bnds"};
    const char* varAttNames[][5] = {
        {"long_name", "standard_name", "units", "axis", "bounds"},
        {"long_name", "standard_name", "units", "axis", "bounds"},
        {"long_name", "standard_name", "units", "bounds"},
        {"long_name", "standard_name", "units", "bounds"}
    };

    const char* varAttVals[][5] = {
        {"latitude", "latitude", "degrees_north", "Y", "lat_bnds"},
        {"longitude", "longitude", "degrees_east", "X", "lon_bnds"},
        {"y coordinate of projection", "projection_y_coordinate", units, "y_bnds"},
        {"x coordinate of projection", "projection_x_coordinate", units, "x_bnds"}
    };
    int numLatAtt = 5, numLonAtt = 5, numYAtt = 4, numXAtt = 4;
    int numAtts[] = {numLatAtt, numLonAtt, numYAtt, numXAtt};

    const int numDims = 3;
    const char* YDimName = (primCRSIsGeo) ? "lat" : "y";
    const char* XDimName = (primCRSIsGeo) ? "lon" : "x";

    const char *dimNames[] = {YDimName, XDimName, "bnds"};
    const unsigned long dimVals[] = {SW_Domain->nDimY, SW_Domain->nDimX, 2};
    int *dimIDs[] = {YDimID, XDimID, &bndsID};
    int dimIDIndex;

    int varIDs[4];
    int varBndIDs[4];

    // Create dimensions
    for(dimNum = 0; dimNum < numDims; dimNum++) {
        create_netCDF_dim(dimNames[dimNum], dimVals[dimNum], domFileID,
                          dimIDs[dimNum], LogInfo);
        if(LogInfo->stopRun) {
            return; // Exit function prematurely due to error
        }
    }

    // Create variables - lat/lon and (if the primary CRS is
    // not geographic) y/x along with the corresponding *_bnds variable
    bndVarDims[1] = bndsID; // Set the second dimension of variable to bounds

    for(varNum = 0; varNum < numVars; varNum++) {
        dimIDIndex = varNum % 2; // 2 - get lat/lon dim ids only, not bounds

        create_netCDF_var(&varIDs[varNum], varNames[varNum], dimIDs[dimIDIndex],
                          domFileID, NC_DOUBLE, 1, LogInfo);
        if(LogInfo->stopRun) {
            return; // Exit function prematurely due to error
        }

        bndVarDims[0] = *dimIDs[dimIDIndex]; // Set the first dimension of the variable

        create_netCDF_var(&varBndIDs[varNum], bndVarNames[varNum], bndVarDims,
                          domFileID, NC_DOUBLE, 2, LogInfo);
        if(LogInfo->stopRun) {
            return; // Exit function prematurely due to error
        }
        // Fill attributes
        for(attNum = 0; attNum < numAtts[varNum]; attNum++) {
            write_str_att(varAttNames[varNum][attNum], varAttVals[varNum][attNum],
                          varIDs[varNum], *domFileID, LogInfo);
            if(LogInfo->stopRun) {
                return; // Exit function prematurely due to error
            }
        }
    }

    // Set lat/lon IDs created above depending on which CRS long_name
    // matches "crs_bbox"
    if(Str_CompareI(SW_Domain->crs_bbox, geo_long_name) == 0 ||
        (is_wgs84(SW_Domain->crs_bbox) && is_wgs84(geo_long_name))) {

        *YVarID = varIDs[0]; // lat
        *XVarID = varIDs[1]; // lon
        *YBndsID = varBndIDs[0]; // lat_bnds
        *XBndsID = varBndIDs[1]; // lon_bnds

    } else if(!primCRSIsGeo &&
              Str_CompareI(SW_Domain->crs_bbox, proj_long_name) == 0) {

        *YVarID = varIDs[2]; // y
        *XVarID = varIDs[3]; // x
        *YBndsID = varBndIDs[2]; // y_bnds
        *XBndsID = varBndIDs[3]; // x_bnds

    } else {
        LogError(LogInfo, LOGERROR, "The given bounding box name within the domain "
                    "input file does not match either the geographic or projected "
                    "CRS (if provided) in the netCDF attributes file. Please "
                    "make sure the bounding box name matches the desired "
                    "CRS 'long_name' that is to be filled.");
    }
}

/**
 * @brief Fill the desired netCDF with a projected CRS
 *
 * @param[in] crs_projsc Struct of type SW_CRS that holds all information
 *  regarding to the CRS type "projected"
 * @param[in] ncFileID Identifier of the open netCDF file to write all information to
 * @param[in] proj_id Projected CRS variable identifier within the netCDF we are writing to
 * @param[out] LogInfo Holds information on warnings and errors
*/
static void fill_netCDF_with_proj_CRS_atts(SW_CRS* crs_projsc, int* ncFileID,
                                           int proj_id, LOG_INFO* LogInfo) {

    const int numStrAtts = 5, numDoubleAtts = 8;
    int strAttNum, doubleAttNum, numValsToWrite;
    const char *strAttNames[] = {
        "long_name", "grid_mapping_name", "datum", "units", "crs_wkt"
    };
    const char *doubleAttNames[] = {
        "standard_parallel", "longitude_of_central_meridian",
        "latitude_of_projection_origin", "false_easting", "false_northing",
        "longitude_of_prime_meridian",  "semi_major_axis", "inverse_flattening"
    };

    const char *strAttVals[] = {
        crs_projsc->long_name, crs_projsc->grid_mapping_name,
        crs_projsc->datum, crs_projsc->units, crs_projsc->crs_wkt
    };
    const double *doubleAttVals[] = {
        crs_projsc->standard_parallel, &crs_projsc->longitude_of_central_meridian,
        &crs_projsc->latitude_of_projection_origin, &crs_projsc->false_easting,
        &crs_projsc->false_northing, &crs_projsc->longitude_of_prime_meridian,
        &crs_projsc->semi_major_axis, &crs_projsc->inverse_flattening
    };

    for(strAttNum = 0; strAttNum < numStrAtts; strAttNum++) {
        write_str_att(strAttNames[strAttNum], strAttVals[strAttNum],
                      proj_id, *ncFileID, LogInfo);
        if(LogInfo->stopRun) {
            return; // Exit function prematurely due to error
        }
    }

    for(doubleAttNum = 0; doubleAttNum < numDoubleAtts; doubleAttNum++) {
        numValsToWrite = (doubleAttNum > 0 || isnan(crs_projsc->standard_parallel[1])) ? 1 : 2;

        write_double_att(doubleAttNames[doubleAttNum], doubleAttVals[doubleAttNum],
                         proj_id, *ncFileID, numValsToWrite, LogInfo);
        if(LogInfo->stopRun) {
            return; // Exit function prematurely due to error
        }
    }
}

/**
 * @brief Fill the desired netCDF with a projected CRS
 *
 * @param[in] crs_geogsc Struct of type SW_CRS that holds all information
 *  regarding to the CRS type "geographic"
 * @param[in] ncFileID Identifier of the open netCDF file to write all information to
 * @param[in] coord_sys Name of the coordinate system being used
 * @param[in] geo_id Projected CRS variable identifier within the netCDF we are writing to
 * @param[out] LogInfo Holds information on warnings and errors
*/
static void fill_netCDF_with_geo_CRS_atts(SW_CRS* crs_geogsc, int* ncFileID,
                            char* coord_sys, int geo_id, LOG_INFO* LogInfo) {

    int attNum, numStrAtts = 3, numDoubleAtts = 3;
    const int numValsToWrite = 1;
    const char *strAttNames[] = {"grid_mapping_name", "long_name", "crs_wkt"};
    const char *doubleAttNames[] = {
        "longitude_of_prime_meridian", "semi_major_axis", "inverse_flattening"
    };
    const char *strAttVals[] = {
        crs_geogsc->grid_mapping_name, crs_geogsc->long_name, crs_geogsc->crs_wkt
    };
    const double *doubleAttVals[] = {
        &crs_geogsc->longitude_of_prime_meridian, &crs_geogsc->semi_major_axis,
        &crs_geogsc->inverse_flattening
    };

    if(strcmp(coord_sys, "Absent") == 0) {
        // Only write out `grid_mapping_name`
        write_str_att(strAttNames[0], strAttVals[0], geo_id, *ncFileID, LogInfo);
    } else {
        // Write out all attributes
        for(attNum = 0; attNum < numStrAtts; attNum++) {
            write_str_att(strAttNames[attNum], strAttVals[attNum],
                          geo_id, *ncFileID, LogInfo);
            if(LogInfo->stopRun) {
                return; // Exit function prematurely due to error
            }
        }

        for(attNum = 0; attNum < numDoubleAtts; attNum++) {
            write_double_att(doubleAttNames[attNum], doubleAttVals[attNum],
                             geo_id, *ncFileID, numValsToWrite, LogInfo);
            if(LogInfo->stopRun) {
                return; // Exit function prematurely due to error
            }
        }
    }
}

/**
 * @brief Fill the given netCDF with global attributes
 *
 * @param[in] SW_netCDF Struct of type SW_NETCDF holding constant
 *  netCDF file information
 * @param[in] ncFileID Identifier of the open netCDF file to write all information to
 * @param[in] domType Type of domain in which simulations are running
 *  (gridcell/sites)
 * @param[in] freqAtt Value of a global attribute "frequency" (may be "fx",
 *  "day", "week", "month", or "year")
 * @param[in] isInputFile Specifies if the file being written to is input
 * @param[out] LogInfo Holds information on warnings and errors
*/
static void fill_netCDF_with_global_atts(SW_NETCDF* SW_netCDF, int* ncFileID,
                                         const char* domType, const char* freqAtt,
                                         Bool isInputFile, LOG_INFO* LogInfo) {

    char sourceStr[40]; // 40 - valid size of the SOILWAT2 global `SW2_VERSION` + "SOILWAT2"
    char creationDateStr[21]; // 21 - valid size to hold a string of format YYYY-MM-DDTHH:MM:SSZ

    int attNum;
    const int numGlobAtts = (strcmp(domType, "s") == 0) ? 14 : 13; // Do or do not include "featureType"
    const char* attNames[] = {
        "title", "author", "institution", "comment", "coordinate_system",
        "Conventions", "source", "source_id", "further_info_url", "creation_date",
        "history", "product", "frequency", "featureType"
    };

    const char* productStr = (isInputFile) ? "model-input" : "model-output";
    const char* featureTypeStr;
    if(strcmp(domType, "s") == 0) {
        featureTypeStr = (dimExists("time", *ncFileID)) ? "timeSeries" : "point";
    } else {
        featureTypeStr = "";
    }

    const char* attVals[] = {
        SW_netCDF->title, SW_netCDF->author, SW_netCDF->institution,
        SW_netCDF->comment, SW_netCDF->coordinate_system, "CF-1.10", sourceStr,
        "SOILWAT2", "https://github.com/DrylandEcology/SOILWAT2", creationDateStr,
        "No revisions.", productStr, freqAtt, featureTypeStr
    };

    // Fill `sourceStr` and `creationDateStr`
    snprintf(sourceStr, 40, "SOILWAT2%s", SW2_VERSION);
    timeStringISO8601(creationDateStr, sizeof creationDateStr);

    // Write out the necessary global attributes that are listed above
    for(attNum = 0; attNum < numGlobAtts; attNum++) {
        write_str_att(attNames[attNum], attVals[attNum], NC_GLOBAL, *ncFileID, LogInfo);
        if(LogInfo->stopRun) {
            return; // Exit function prematurely due to error
        }
    }
}

/**
 * @brief Overwrite specific global attributes into a new file
 *
 * @param[in] ncFileID Identifier of the open netCDF file to write all information to
 * @param[in] domType Type of domain in which simulations are running
 *  (gridcell/sites)
 * @param[in] freqAtt Value of a global attribute "frequency" (may be "fx",
 *  "day", "week", "month", or "year")
 * @param[in] isInputFile Specifies if the file being written to is input
 * @param[in,out] LogInfo Holds information dealing with logfile output
*/
static void update_netCDF_global_atts(int* ncFileID, const char* domType,
                    const char* freqAtt, Bool isInputFile, LOG_INFO* LogInfo) {

    char sourceStr[40]; // 40 - valid size of the SOILWAT2 global `SW2_VERSION` + "SOILWAT2"
    char creationDateStr[21]; // 21 - valid size to hold a string of format YYYY-MM-DDTHH:MM:SSZ

    int attNum;
    const int numGlobAtts = (strcmp(domType, "s") == 0) ? 5 : 4; // Do or do not include "featureType"
    const char* attNames[] = {
        "source", "creation_date", "product", "frequency", "featureType"
    };

    const char* productStr = (isInputFile) ? "model-input" : "model-output";
    const char* featureTypeStr;
    if(strcmp(domType, "s") == 0) {
        featureTypeStr = (dimExists("time", *ncFileID)) ? "timeSeries" : "point";
    } else {
        featureTypeStr = "";
    }

    const char* attVals[] = {
        sourceStr, creationDateStr, productStr, freqAtt, featureTypeStr
    };

    // Fill `sourceStr` and `creationDateStr`
    snprintf(sourceStr, 40, "SOILWAT2%s", SW2_VERSION);
    timeStringISO8601(creationDateStr, sizeof creationDateStr);

    // Write out the necessary global attributes that are listed above
    for(attNum = 0; attNum < numGlobAtts; attNum++) {
        write_str_att(attNames[attNum], attVals[attNum], NC_GLOBAL, *ncFileID, LogInfo);
        if(LogInfo->stopRun) {
            return; // Exit function prematurely due to error
        }
    }
}

/**
 * @brief Wrapper function to fill a netCDF with all the invariant information
 *  i.e., global attributes (including time created) and CRS information
 *  (including the creation of these variables)
 *
 * @param[in] SW_netCDF Struct of type SW_NETCDF holding constant
 *  netCDF file information
 * @param[in] domType Type of domain in which simulations are running
 *  (gridcell/sites)
 * @param[in] ncFileID Identifier of the open netCDF file to write all information to
 * @param[in] isInputFile Specifies whether the file being written to is input
 * @param[out] LogInfo Holds information on warnings and errors
*/
static void fill_netCDF_with_invariants(SW_NETCDF* SW_netCDF, char* domType,
                                        int* ncFileID, Bool isInputFile,
                                        LOG_INFO* LogInfo) {

    int geo_id = 0, proj_id = 0;
    const char *fx = "fx";

    create_netCDF_var(&geo_id, "crs_geogsc", NULL, ncFileID, NC_BYTE, 0, LogInfo);
    if(LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

    // Geographic CRS attributes
    fill_netCDF_with_geo_CRS_atts(&SW_netCDF->crs_geogsc, ncFileID,
                                  SW_netCDF->coordinate_system, geo_id, LogInfo);
    if(LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

    // Projected CRS variable/attributes
    if(!SW_netCDF->primary_crs_is_geographic) {
        create_netCDF_var(&proj_id, "crs_projsc", NULL, ncFileID, NC_BYTE,
                          0, LogInfo);
        if(LogInfo->stopRun) {
            return; // Exit function prematurely due to error
        }

        fill_netCDF_with_proj_CRS_atts(&SW_netCDF->crs_projsc,
                                       ncFileID, proj_id, LogInfo);
        if(LogInfo->stopRun) {
            return; // Exit function prematurely due to error
        }
    }

    // Write global attributes
    fill_netCDF_with_global_atts(SW_netCDF, ncFileID, domType, fx,
                                 isInputFile, LogInfo);
}

/**
 * @brief Calculate time size in days
 *
 * @param[in] rangeStart Start year for the current output file
 * @param[in] rangeEnd End year for the current output file
 * @param[in] baseTime Base number of output periods in a year
 *  (e.g., 60 moonths in 5 years, or 731 days in 1980-1981)
 * @param[in] pd Current output netCDF period
*/
static double calc_timeSize(int rangeStart, int rangeEnd, int baseTime,
                            OutPeriod pd) {

    double numLeapYears = 0.0;
    int year;

    if(pd == eSW_Day) {
        for(year = rangeStart; year < rangeEnd; year++) {
            if(isleapyear(year)) {
                numLeapYears++;
            }
        }
    }

    return (double) (baseTime * (rangeEnd - rangeStart)) + numLeapYears;
}

/**
 * @brief Gather the output dimension sizes when writing out values to
 *  the output netCDF
 *
 * @param[in] dimStr String holding the dimensions for a specific
 *  variable
 * @param[in] originTimeSize Original "time" dimension size (that will
 *  not be overwritten in the function)
 * @param[in] originVertSize Original "vertical" dimension size (that will
 *  not be overwritten in the function)
 * @param[in] originPFTSize Original "pft" dimension size (that will
 *  not be overwritten in the function)
 * @param[out] timeSize Output write size for dimension "time"
 * @param[out] vertSize Output write size for dimension "vertical"
 * @param[out] pftSize Output write size for dimension "pft"
 * @param[out] LogInfo Holds information dealing with logfile output
*/
static void determine_out_dim_sizes(char* dimStr, int originTimeSize,
        int originVertSize, int originPFTSize, int* timeSize, int* vertSize,
        int* pftSize, LOG_INFO* LogInfo) {

    char* wkgPtr = dimStr;

    *timeSize = *vertSize = *pftSize = 0;

    while(*wkgPtr != '\0') {
        switch(*wkgPtr) {
            case 'Z':
                *vertSize = originVertSize;
                break;
            case 'T':
                *timeSize = originTimeSize;
                break;
            case 'V':
                *pftSize = originPFTSize;
                break;
            default:
                LogError(LogInfo, LOGWARN, "Detected unknown dimension: %s.",
                                           *wkgPtr);
                break;
        }
        wkgPtr++;
    }
}

/**
 * @brief Helper function to `fill_dimVar()`; fully creates/fills
 *  the variable "time_bnds" and fills the variable "time"
 *
 * @param[in] ncFileID Identifier of the netCDF in which the information
 *  will be written
 * @param[in] dimIDs Dimension identifiers for "vertical" and "bnds"
 * @param[in] size Size of the vertical dimension/variable
 * @param[in] dimVarID "time" dimension identifier
 * @param[in] startYr Start year of the simulation
 * @param[in,out] startTime Start number of days when dealing with
 *  years between netCDF files
 * @param[in] pd Current output netCDF period
 * @param[out] LogInfo Holds information dealing with logfile output
*/
static void create_time_vars(int ncFileID, int dimIDs[], int size,
            int dimVarID, int startYr, double* startTime, OutPeriod pd,
            LOG_INFO* LogInfo) {

    double *bndsVals = NULL, *dimVarVals = NULL;
    size_t numBnds = 2;
    size_t start[] = {0, 0}, count[] = {(size_t)size, 0};
    int currYear = startYr;
    int month = 0, week = 0, numDays = 0;
    int bndsID = 0;


    create_netCDF_var(&bndsID, "time_bnds", dimIDs,
                        &ncFileID, NC_DOUBLE, numBnds, LogInfo);
    if(LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

    dimVarVals = (double *)Mem_Malloc(size * sizeof(double),
                        "create_full_var", LogInfo);
    if(LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

    bndsVals = (double *)Mem_Malloc(size * numBnds * sizeof(double),
                        "create_full_var", LogInfo);
    if(LogInfo->stopRun) {
        free(dimVarVals);
        return; // Exit function prematurely due to error
    }

    for(size_t index = 0; index < (size_t) size; index++) {

        switch(pd) {
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

                currYear += (index % MAX_WEEKS == 0) ? 1.0 : 0.0;
                week = (week + 1) % MAX_WEEKS;
                break;

            case eSW_Month:
                if(month == Feb) {
                    numDays = isleapyear(currYear) ? 29 : 28;
                } else {
                    numDays = monthdays[month];
                }

                currYear += (index % MAX_MONTHS == 0) ? 1.0 : 0.0;
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
        dimVarVals[index] = (bndsVals[index * 2] + bndsVals[index * 2 + 1]) / 2.0;

        *startTime += numDays;
    }

    fill_netCDF_var_double(ncFileID, dimVarID, dimVarVals,
                           start, count, LogInfo);
    free(dimVarVals);
    if(LogInfo->stopRun) {
        free(bndsVals);
        return; // Exit function prematurely due to error
    }

    count[1] = numBnds;

    fill_netCDF_var_double(ncFileID, bndsID, bndsVals,
                            start, count, LogInfo);

    free(bndsVals);
}

/**
 * @brief Helper function to `fill_dimVar()`; fully creates/fills
 *  the variable "vertical_bnds" and fills the variable "vertical"
 *
 * @param[in] ncFileID Identifier of the netCDF in which the information
 *  will be written
 * @param[in] dimIDs Dimension identifiers for "vertical" and "bnds"
 * @param[in] size Size of the vertical dimension/variable
 * @param[in] dimVarID "vertical" dimension identifier
 * @param[in] lyrDepths Depths of soil layers (cm)
 * @param[out] LogInfo Holds information dealing with logfile output
*/
static void create_vert_vars(int ncFileID, int dimIDs[], int size,
                int dimVarID, double lyrDepths[], LOG_INFO* LogInfo) {

    double *dimVarVals = NULL, *bndVals = NULL, lyrDepthStart = 0.0;
    size_t numBnds = 2;
    size_t start[] = {0, 0}, count[] = {(size_t)size, 0};
    int bndIndex = 0;

    create_netCDF_var(&bndIndex, "vertical_bnds", dimIDs,
                        &ncFileID, NC_DOUBLE, numBnds, LogInfo);
    if(LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

    dimVarVals = (double *)Mem_Malloc(size * sizeof(double),
                        "create_full_var", LogInfo);
    if(LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

    bndVals = (double *)Mem_Malloc(size * numBnds * sizeof(double),
                        "create_full_var", LogInfo);
    if(LogInfo->stopRun) {
        free(dimVarVals);
        return; // Exit function prematurely due to error
    }

    for(size_t index = 0; index < (size_t) size; index++) {
        dimVarVals[index] = lyrDepths[index];

        bndVals[index * 2] = lyrDepthStart;
        bndVals[index * 2 + 1] = dimVarVals[index];

        lyrDepthStart = bndVals[index * 2 + 1];
    }

    fill_netCDF_var_double(ncFileID, dimVarID, dimVarVals,
                            start, count, LogInfo);
    free(dimVarVals);
    if(LogInfo->stopRun) {
        free(bndVals);
        return; // Exit function prematurely due to error
    }

    count[1] = numBnds;

    fill_netCDF_var_double(ncFileID, bndIndex, bndVals,
                            start, count, LogInfo);

    free(bndVals);
}

/**
 * @brief Helper function to `create_output_dimVar()`; fills
 *  the dimension variable plus the respective "*_bnds" variable
 *  if needed
 *
 * @param[in] ncFileID Identifier of the netCDF in which the information
 *  will be written
 * @param[in] dimIDs Identifiers of the dimensions the parent dimension has
 *  (i.e., "time", "vertical", and "pft") in case of the need when creating
 *  the "bnds" dimension for a "*_bnds" variable
 * @param[in] size Size of the dimension/original variable dimension
 * @param[in] varID Identifier of the new variable respectively named
 *  from the created dimension
 * @param[in] lyrDepths Depths of soil layers (cm)
 * @param[in,out] startTime Start number of days when dealing with
 *  years between netCDF files
 * @param[in] dimNum Identifier to determine which position the given
 *  dimension is in out of: "vertical" (0), "time" (1), and "pft" (2)
 * @param[in] startYr Start year of the simulation
 * @param[in] pd Current output netCDF period
 * @param[out] LogInfo Holds information dealing with logfile output
*/
static void fill_dimVar(int ncFileID, int dimIDs[], int size, int varID,
            double lyrDepths[], double* startTime, int dimNum,
            int startYr, OutPeriod pd, LOG_INFO* LogInfo) {

    const int vertInd = 0, timeInd = 1, pftInd = 2;
    const int numBnds = 2;

    const char* vertVals[] = {"trees", "shrubs", "forbs", "grass"};

    if(dimNum == pftInd) {
        write_string_vals(ncFileID, varID, vertVals, LogInfo);
    } else {
        if(!dimExists("bnds", ncFileID)) {
            create_netCDF_dim("bnds", numBnds, &ncFileID, &dimIDs[1], LogInfo);
        } else {
            get_dim_identifier(ncFileID, "bnds", &dimIDs[1], LogInfo);
        }
        if(LogInfo->stopRun) {
            return; // Exit function prematurely due to error
        }

        if(dimNum == vertInd) {
            if(!varExists(ncFileID, "vertical_bnds")) {

                create_vert_vars(ncFileID, dimIDs, size, varID,
                                    lyrDepths, LogInfo);
            }
        } else if(dimNum == timeInd) {
            if(!varExists(ncFileID, "time_bnds")) {
                create_time_vars(ncFileID, dimIDs, size, varID,
                                    startYr, startTime, pd, LogInfo);
            }
        }
    }
}

/**
 * @brief Create a "time", "vertical", or "pft" dimension variable and the
 *  respective "*_bnds" variale (plus "bnds" dimension) for "time"
 *  and "vertical" and fill the variable with the respective information
 *
 * @param[in] name Name of the new dimension
 * @param[in] size Size of the new dimension
 * @param[in] ncFileID Identifier of the netCDF in which the information
 *  will be written
 * @param[in,out] dimID New dimenion identifier within the given netCDF
 * @param[in] dimNum Identifier to determine which position the given
 *  dimension is in out of: "vertical" (0), "time" (1), and "pft" (2)
 * @param[in] lyrDepths Depths of soil layers (cm)
 * @param[in,out] startTime Start number of days when dealing with
 *  years between netCDF files
 * @param[in] baseCalendarYear First year of the entire simulation
 * @param[in] startYr Start year of the simulation
 * @param[in] pd Current output netCDF period
 * @param[out] LogInfo Holds information dealing with logfile output
*/
static void create_output_dimVar(const char* name, int size, int ncFileID,
        int* dimID, int dimNum, double lyrDepths[], double* startTime,
        int baseCalendarYear, int startYr, OutPeriod pd, LOG_INFO* LogInfo) {

    const int timeIndex = 1, pftIndex = 2, timeUnitIndex = 2;

    int varID, index;
    int dimIDs[2] = {0,0};
    int varType = (dimNum == pftIndex) ? NC_STRING : NC_DOUBLE;
    double tempVal = 1.0;
    double* startFillTime = (dimNum == timeIndex) ? startTime : &tempVal;
    const int numDims = 1;

    const char* outAttNames[][6] = {
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

    create_netCDF_dim(name, size, &ncFileID, dimID, LogInfo);
    if(LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

    if(!varExists(ncFileID, name)) {
        dimIDs[0] = *dimID;

        create_netCDF_var(&varID, name, dimIDs, &ncFileID,
                            varType, numDims, LogInfo);
        if(LogInfo->stopRun) {
            return; // Exit function prematurely due to error
        }

        fill_dimVar(ncFileID, dimIDs, size, varID, lyrDepths,
                    startFillTime, dimNum, startYr, pd, LogInfo);

        if(dimNum == timeIndex) {
            snprintf(outAttVals[timeIndex][timeUnitIndex], MAX_FILENAMESIZE,
                     "days since %d-01-01 00:00:00", baseCalendarYear);
        }

        for(index = 0; index < numVarAtts[dimNum]; index++) {
            write_str_att(outAttNames[dimNum][index], outAttVals[dimNum][index],
                          varID, ncFileID, LogInfo);
            if(LogInfo->stopRun) {
                return; // Exit function prematurely due to error
            }
        }
    }
}

/**
 * @brief Create a new variable by calculating the dimensions
 *  and writing attributes
 *
 * @param[in] ncFileID Identifier of the netCDF file
 * @param[in] newVarType Type of the variable to create
 * @param[in] timeSize Size of "time" dimension
 * @param[in] vertSize Size of "vertical" dimension
 * @param[in] pftSize Size of "pft" dimension
 * @param[in] varName Name of variable to write
 * @param[in] attNames Attribute names that the new variable will contain
 * @param[in] attVals Attribute values that the new variable will contain
 * @param[in] numAtts Number of attributes being sent in
 * @param[in] lyrDepths Depths of soil layers (cm)
 * @param[in,out] startTime Start number of days when dealing with
 *  years between netCDF files
 * @param[in] baseCalendarYear First year of the entire simulation
 * @param[in] startYr Start year of the simulation
 * @param[in] pd Current output netCDF period
 * @param[in,out] LogInfo Holds information dealing with logfile output
*/
static void create_full_var(int* ncFileID, int newVarType,
    size_t timeSize, size_t vertSize, size_t pftSize, const char* varName,
    const char* attNames[], const char* attVals[], int numAtts,
    double lyrDepths[], double* startTime, int baseCalendarYear,
    int startYr, OutPeriod pd, LOG_INFO* LogInfo) {

    int dimArrSize = 0, index, varID = 0;
    int dimIDs[MAX_NUM_DIMS];
    Bool siteDimExists = dimExists("site", *ncFileID);
    const char* latName = (dimExists("lat", *ncFileID)) ? "lat" : "y";
    const char* lonName = (dimExists("lon", *ncFileID)) ? "lon" : "x";
    int numConstDims = (siteDimExists) ? 1 : 2;
    const char* thirdDim = (siteDimExists) ? "site" : latName;
    const char* constDimNames[] = {thirdDim, lonName};
    const char* timeVertVegNames[] = {"vertical", "time", "pft"};
    char* dimVarName;
    size_t timeVertVegVals[] = {vertSize, timeSize, pftSize};
    int numTimeVertVegVals = 3, varVal;


    for(index = 0; index < numConstDims; index++) {
        get_dim_identifier(*ncFileID, constDimNames[index],
                        &dimIDs[dimArrSize], LogInfo);
        if(LogInfo->stopRun) {
            return; // Exit function prematurely due to error
        }

        dimArrSize++;
    }

    for(index = 0; index < numTimeVertVegVals; index++) {
        dimVarName = (char *)timeVertVegNames[index];
        varVal = timeVertVegVals[index];
        if(varVal > 0) {
            if(!dimExists(dimVarName, *ncFileID)) {
                create_output_dimVar(dimVarName, varVal, *ncFileID,
                        &dimIDs[dimArrSize], index, lyrDepths, startTime,
                        baseCalendarYear, startYr, pd, LogInfo);
            } else {
                get_dim_identifier(*ncFileID, dimVarName, &dimIDs[dimArrSize],
                                   LogInfo);
            }
            if(LogInfo->stopRun) {
                return; // Exit function prematurely due to error
            }

            dimArrSize++;
        }
    }


    create_netCDF_var(&varID, varName, dimIDs, ncFileID, newVarType,
                      dimArrSize, LogInfo);
    if(LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

    for(index = 0; index < numAtts; index++) {
        write_str_att(attNames[index], attVals[index],
                    varID, *ncFileID, LogInfo);
        if(LogInfo->stopRun) {
            return; // Exit function prematurely due to error
        }
    }
}

/**
 * @brief Determine if the current output key contains information
 *  within layers
 *
 * @param[in] key Specifies what output key is currently being allocated
 *  (i.e., temperature, precipitation, etc.)
 * @param[in] n_layers Total number of soil layers
 * @param[in] n_evap_lyrs Number of layers in which evap is possible
 *
 * @return Number of layers to write out to the netCDFs in question
*/
static int getNLayers(OutKey key, int n_layers, int n_evap_lyrs) {
    int numLayers = 0;

    if((key >= eSW_VWCBulk && key <= eSW_SWPMatric) ||
        key == eSW_EvapSoil || key == eSW_HydRed    ||
        key == eSW_WetDays  || key == eSW_SoilTemp  ||
        key == eSW_Frozen   || key == eSW_Biomass   ||
        key == eSW_Transp) {

        numLayers = n_layers;
    } else if(key == eSW_LyrDrain) {
        numLayers = n_layers - 1;
    } else if(key == eSW_EvapSoil) {
        numLayers = n_evap_lyrs;
    }

    return numLayers;
}

/**
 * @brief Concentrate the important output variable attributes
 *  into one location to write out
 *
 * @param[in] varInfo Attribute information on the current variable
 *  that will help create the attributes
 * @param[in] key Specifies what output key is currently being allocated
 *  (i.e., temperature, precipitation, etc.)
 * @param[in] pd Current output netCDF period
 * @param[in] varNum Designated variable placement within the list of output
 *  variable information
 * @param[out] resAtts Resulting attributes to write out
 * @param[in] sumType Sum type of the output key
 * @param[out] LogInfo Holds information on warnings and errors
*/
static int gather_var_attributes(char** varInfo, OutKey key, OutPeriod pd,
                                 int varNum, char* resAtts[], OutSum sumType,
                                 LOG_INFO* LogInfo) {
    int fillSize = 0, varIndex;
    char cellRedef[MAX_FILENAMESIZE], establOrginName[MAX_FILENAMESIZE];

    if(key == eSW_Estab) {
        snprintf(establOrginName, MAX_FILENAMESIZE,
                 "%s__%s", SW_ESTAB, varInfo[VARNAME_INDEX]);

        resAtts[fillSize] = Str_Dup(establOrginName, LogInfo);
        if(LogInfo->stopRun) {
            return 0; // Exit function prematurely due to error
        }
    } else {
        resAtts[fillSize] = (char *)possKeys[key][varNum];
    }
    fillSize++;

    // Transfer the variable info into the result array (ignore the variable name and dimensions)
    for(varIndex = LONGNAME_INDEX; varIndex <= CELLMETHOD_INDEX; varIndex++) {
        resAtts[fillSize] = varInfo[varIndex];
        fillSize++;
    }

    if(pd > eSW_Day) {
        snprintf(cellRedef, MAX_FILENAMESIZE, "%s within days time: %s over days",
                 resAtts[fillSize - 1], styp2longstr[sumType]);
        Str_ToLower(cellRedef, cellRedef);
        resAtts[fillSize - 1] = Str_Dup(cellRedef, LogInfo);
        if(LogInfo->stopRun) {
            return 0; // Exit function prematurely due to error
        }
    }

    if(key == eSW_Temp || key == eSW_SoilTemp) {
        resAtts[fillSize] = (char *)"temperature: on_scale";
        fillSize++;
    }

    return fillSize;
}

/**
 * @brief Create and fill a new output netCDF file
 *
 * @param[in] SW_Output SW_OUTPUT array of size SW_OUTNKEYS which holds
 *  basic output information for all output keys
 * @param[in] domFile Domain netCDF file name
 * @param[in] domID Domain netCDF file identifier
 * @param[in] newFileName Name of the new file that will be created
 * @param[in] key Specifies what output key is currently being allocated
 *  (i.e., temperature, precipitation, etc.)
 * @param[in] pd Current output netCDF period
 * @param[in] originTimeSize Original "time" dimension size (that will
 *  not be overwritten in the function)
 * @param[in] originVertSize Original "vertical" dimension size (that will
 *  not be overwritten in the function)
 * @param[in] lyrDepths Depths of soil layers (cm)
 * @param[in,out] startTime Start number of days when dealing with
 *  years between netCDF files
 * @param[in] baseCalendarYear First year of the entire simulation
 * @param[in] startYr Start year of the simulation
 * @param[in] LogInfo Holds information on warnings and errors
*/
static void create_output_file(SW_OUTPUT* SW_Output,
        const char* domFile, int domID, const char* newFileName,
        OutKey key, OutPeriod pd, int originTimeSize, int originVertSize,
        double lyrDepths[], double* startTime, int baseCalendarYear,
        int startYr, LOG_INFO* LogInfo) {

    int index;
    const char* frequency[] = {"daily", "weekly", "monthly", "yearly"};
    const char* attNames[] = {
        "original_name", "long_name", "comment", "units", "cell_method",
        "units_metadata"
    };
    char* attVals[MAX_NATTS] = {NULL};
    OutSum sumType = SW_Output->sumtype;

    int numAtts = 0;
    int numOutVars = numVarsPerKey[key];
    int originNumVeg = NVEGTYPES;
    int numVegTypesOut = 0, timeSize = 0, vertSize = 0;
    const int nameAtt = 0;

    int newFileID = -1; // Default to not created
    int cellMethAttInd = 0;
    char* varName, *dimStr = NULL;
    char** varInfo;

    // Add the rest of the output variables for the current output file
    for(index = 0; index < numOutVars; index++) {
        if(SW_Output->reqOutputVars[index]) {
            varInfo = SW_Output->outputVarInfo[index];
            varName = SW_Output->outputVarInfo[index][VARNAME_INDEX];
            dimStr = SW_Output->outputVarInfo[index][DIM_INDEX];

            determine_out_dim_sizes(dimStr, originTimeSize, originVertSize,
                originNumVeg, &timeSize, &vertSize, &numVegTypesOut, LogInfo);
            numAtts = gather_var_attributes(varInfo, key, pd, index, attVals,
                                            sumType, LogInfo);
            if(LogInfo->stopRun) {
                return; // Exit function prematurely due to error
            }

            if(!FileExists(newFileName)) {
                if(pd > eSW_Day) {
                    cellMethAttInd = (key == eSW_Temp || key == eSW_SoilTemp) ?
                                                    numAtts - 2 : numAtts - 1;
                }

                // Create a new output file
                SW_NC_create_template(domFile, domID, newFileName,
                    &newFileID, swFALSE, frequency[pd], LogInfo);
            }

            create_full_var(&newFileID, NC_DOUBLE, timeSize, vertSize, numVegTypesOut,
                            varName, attNames, (const char**)attVals, numAtts,
                            lyrDepths, startTime, baseCalendarYear, startYr, pd,
                            LogInfo);


            if(pd > eSW_Day && !isnull(attVals[cellMethAttInd])) {
                free(attVals[cellMethAttInd]);
                attVals[cellMethAttInd] = NULL;
            }
            if(key == eSW_Estab && !isnull(attVals[nameAtt])) {
                free(attVals[nameAtt]);
            }
            if(LogInfo->stopRun && FileExists(newFileName)) {
                nc_close(newFileID);
                return; // Exit function prematurely due to error
            }
        }
    }

    // Only close the file if it was created
    if(newFileID > -1) {
        nc_close(newFileID);
    }
}

/**
 * @brief Determine the write dimensions/sizes for the current
 *  output variable/period
 *
 * @param[in] ncFileID Output netCDF file ID
 * @param[out] dimSizes Array storing the output dimensions
 * @param[in] dimStr String holding the dimensions for a specific
 *  variable
 * @param[out] LogInfo Holds information on warnings and errors
*/
static void get_write_dim_sizes(int ncFileID, size_t dimSizes[], char* dimStr,
                                int n_layers, LOG_INFO* LogInfo) {

    char* wkgDimPtr = dimStr, *dimName = NULL;
    Bool twoDimDom = (Bool) !dimExists("site", ncFileID);
    Bool readDimVal;
    int numDims = 0, index;

    dimSizes[numDims] = 1;
    dimSizes[numDims + 1] = (twoDimDom) ? 1 : 0;
    numDims += (twoDimDom) ? 2 : 1;

    while(*wkgDimPtr != '\0') {
        readDimVal = swTRUE;

        switch(*wkgDimPtr) {
            case 'T':
                dimName = (char *)"time";
                break;
            case 'V':
                dimName = (char *)"pft";
                break;
            case 'Z':
                dimSizes[numDims] = (size_t)n_layers;
                break;
            default:
                readDimVal = swFALSE;
                break;
        }

        if(readDimVal) {
            if(*wkgDimPtr != 'Z') {
                get_dim_val(ncFileID, dimName, &dimSizes[numDims], LogInfo);
                if(LogInfo->stopRun) {
                    return; // Exit function prematurely due to error
                }
            }

            numDims++;
        }
        wkgDimPtr++;
    }

    for(index = numDims; index < MAX_NUM_DIMS; index++) {
        dimSizes[index] = 0;
    }
}

/**
 * @brief Calculate the starting index that will be used to write from
 *  `p_OUT`
 *
 * @param[in] SW_GenOut Holds general variables that deal with output
 * @param[in] ncFileID Identifier of the open netCDF file to check
 * @param[in] varNum Variable number within the current key
 * @param[in] pd Current output netCDF period
 * @param[in] vertSize Size of "vertical" dimension (if any)
 * @param[in] startTime Start number of days when dealing with
 *  years between netCDF files
*/
static int calc_pOUTIndex(SW_GEN_OUT* SW_GenOut, int ncFileID, int varNum,
                          OutPeriod pd, int vertSize, int startTime) {
    int pOUTIndex = 0;

    if(dimExists("vertical", ncFileID) && dimExists("pft", ncFileID)) {
        pOUTIndex = startTime + SW_GenOut->nrow_OUT[pd] * (ncol_TimeOUT[pd] + varNum + vertSize * 0);
        //iOUT2(varNum, 0, pd, (*SW_GenOut.irow_OUT), (*SW_GenOut.nrow_OUT), vertSize);
    } else {
        pOUTIndex = iOUT(varNum, startTime, SW_GenOut->nrow_OUT[pd], ncol_TimeOUT[pd]);
    }

    return pOUTIndex;
}

/* =================================================== */
/*             Global Function Definitions             */
/* --------------------------------------------------- */

/**
 * @brief Write values to output variables in previously-created
 *  output netCDF files
 *
 * @param[in] SW_Output SW_OUTPUT array of size SW_OUTNKEYS which holds
 *  basic output information for all output keys
 * @param[in] SW_GenOut Holds general variables that deal with output
 * @param[in] n_layers Number of layers to write out
 * @param[in] n_evap_layers Number of layers in which evap is possible
 * @param[in] numFilesPerKey Number of output netCDFs each output key will
 *  have (same amount for each key)
 * @param[in] ncOutFileNames A list of the generated output netCDF file names
 * @param[in] ncSuid Unique indentifier of the current suid being simulated
 * @param[out] LogInfo Holds information on warnings and errors
*/
void SW_NC_write_output(SW_OUTPUT* SW_Output, SW_GEN_OUT* SW_GenOut,
        LyrIndex n_layers, int n_evap_layers, int numFilesPerKey,
        char** ncOutFileNames[][SW_OUTNPERIODS], size_t ncSuid[],
        LOG_INFO* LogInfo) {

    int key;
    OutPeriod pd;
    RealD *p_OUTValPtr = NULL;
    int fileNum, currFileID = 0, varNum;

    char* fileName, *varName, *dimStr = NULL;
    size_t dimSizes[MAX_NUM_DIMS] = {0};
    size_t start[MAX_NUM_DIMS] = {0};
    size_t pOUTIndex, startTime, timeSize = 0;
    int vertSize;

    start[0] = ncSuid[0];
    start[1] = ncSuid[1];

    ForEachOutPeriod(pd) {
        if(!SW_GenOut->use_OutPeriod[pd]) {
            continue; // Skip period iteration
        }

        ForEachOutKey(key) {
            if(numVarsPerKey[key] == 0 || !SW_Output[key].use) {
                continue; // Skip key iteration
            }

            startTime = 0; // keep track of time across time-sliced files per outkey
            vertSize = getNLayers((OutKey)key, n_layers, n_evap_layers);


            for(fileNum = 0; fileNum < numFilesPerKey; fileNum++) {
                fileName = ncOutFileNames[key][pd][fileNum];

                if(nc_open(fileName, NC_WRITE, &currFileID) != NC_NOERR) {
                    LogError(LogInfo, LOGWARN, "Could not open file %s, so "
                                            "no information was written to it.",
                                            fileName);
                    continue; // Skip key iteration
                }

                for(varNum = 0; varNum < numVarsPerKey[key]; varNum++) {
                    if(!SW_Output[key].reqOutputVars[varNum]) {
                        continue; // Skip variable iteration
                    }

                    varName = SW_Output[key].outputVarInfo[varNum][VARNAME_INDEX];
                    dimStr = SW_Output[key].outputVarInfo[varNum][DIM_INDEX];

                    pOUTIndex = calc_pOUTIndex(SW_GenOut, currFileID, varNum,
                                               pd, vertSize, startTime);

                    get_write_dim_sizes(currFileID, dimSizes, dimStr,
                                        vertSize, LogInfo);
                    if(LogInfo->stopRun) {
                        return; // Exit function prematurely due to error
                    }

                    p_OUTValPtr = &SW_GenOut->p_OUT[key][pd][pOUTIndex];

                    write_double_vals(varName, p_OUTValPtr,
                                currFileID, start, dimSizes, LogInfo);
                    if(LogInfo->stopRun) {
                        return; // Exit function prematurely due to error
                    }
                }

                // Get the time value from the "time" dimension
                get_dim_val(currFileID, "time", &timeSize, LogInfo);
                startTime += timeSize;

                nc_close(currFileID);
            }
        }
    }
}

/**
 * @brief Generate all requested netCDF output files that will be written to
 *  instead of CSVs
 *
 * @param[in] domFile Name of the domain netCDF
 * @param[in] domFileID Identifier of the domain netCDF file
 * @param[in] SW_Output SW_OUTPUT array of size SW_OUTNKEYS which holds
 *  basic output information for all output keys
 * @param[in] strideOutYears Number of years to write into an output file
 * @param[in] startYr Start year of the simulation
 * @param[in] endYr End year of the simulation
 * @param[in] n_layers Number of layers to write out
 * @param[in] n_evap_lyrs Number of layers in which evap is possible
 * @param[out] numFilesPerKey Number of output netCDFs each output key will
 *  have (same amount for each key)
 * @param[out] lyrDepths Depths of soil layers (cm)
 * @param[out] baseCalendarYear First year of the entire simulation
 * @param[out] useOutPeriods Determine which output periods to output
 * @param[out] ncOutFileNames A list of the generated output netCDF file names
 * @param[out] LogInfo Holds information on warnings and errors
*/
void SW_NC_create_output_files(const char* domFile, int domFileID,
        SW_OUTPUT* SW_Output, int strideOutYears, int startYr, int endYr,
        LyrIndex n_layers, int n_evap_lyrs, int* numFilesPerKey,
        double lyrDepths[], int baseCalendarYear, Bool useOutPeriods[],
        char** ncOutFileNames[][SW_OUTNPERIODS], LOG_INFO* LogInfo) {

    int key;
    OutPeriod pd;
    int rangeStart, rangeEnd, fileNum;
    int numYears = endYr - startYr + 1, yearOffset;
    char fileNameBuf[MAX_FILENAMESIZE], yearBuff[10]; // 10 - hold up to YYYY-YYYY
    int timeSize = 0, baseTime = 0, vertSize = 0;
    double startTime[SW_OUTNPERIODS];

    const char* outputDir = "Output/";
    const char* periodSuffix[] = {"_daily", "_weekly", "_monthly", "_yearly"};
    char* yearFormat;

    *numFilesPerKey = (strideOutYears == -1) ? 1 :
                            (int) ceil((double) numYears / strideOutYears);

    yearOffset = (strideOutYears == -1) ? numYears : strideOutYears;

    yearFormat = (strideOutYears == 1) ? (char *)"%d" : (char *)"%d-%d";

    ForEachOutKey(key) {
        if(numVarsPerKey[key] > 0 && SW_Output[key].use) {
            vertSize = getNLayers((OutKey)key, n_layers, n_evap_lyrs);

            ForEachOutPeriod(pd) {
                if(useOutPeriods[pd]) {
                    startTime[pd] = 0;
                    baseTime = times[pd];
                    rangeStart = startYr;

                    SW_NC_alloc_files(&ncOutFileNames[key][pd], *numFilesPerKey,
                                    LogInfo);
                    if(LogInfo->stopRun) {
                        return; // Exit prematurely due to error
                    }
                    for(fileNum = 0; fileNum < *numFilesPerKey; fileNum++) {
                        if(rangeStart + yearOffset > endYr) {
                            rangeEnd = rangeStart + (endYr - rangeStart) + 1;
                        } else {
                            rangeEnd = rangeStart + yearOffset;
                        }

                        snprintf(yearBuff, 10, yearFormat, rangeStart, rangeEnd - 1);
                        snprintf(fileNameBuf, MAX_FILENAMESIZE, "%s%s_%s%s.nc",
                                outputDir, key2str[key], yearBuff, periodSuffix[pd]);

                        ncOutFileNames[key][pd][fileNum] = Str_Dup(fileNameBuf, LogInfo);
                        if(LogInfo->stopRun) {
                            return; // Exit function prematurely due to error
                        }

                        if(!FileExists(fileNameBuf)) {
                            timeSize = calc_timeSize(rangeStart, rangeEnd,
                                                    baseTime, pd);

                            create_output_file(&SW_Output[key], domFile, domFileID,
                                fileNameBuf, (OutKey)key, pd, timeSize, vertSize, lyrDepths,
                                &startTime[pd], baseCalendarYear, rangeStart, LogInfo);
                            if(LogInfo->stopRun) {
                                return; // Exit function prematurely due to error
                            }
                        }

                        rangeStart = rangeEnd;
                    }
                }
            }
        }
    }
}

/**
 * @brief Retrieve the maximum number of layers to create in netCDF output files
 *
 * @param[in] readInNumLayers Value read-in from the input file `soils.in`
*/
int SW_NC_get_nMaxSoilLayers(int readInNumLayers) {
    return readInNumLayers;
}

/**
 * @brief Check that the constant content is consistent between
 *  domain.in and a given netCDF file
 *
 * @param[in] SW_Domain Struct of type SW_DOMAIN holding constant
 *  temporal/spatial information for a set of simulation runs
 * @param[in] ncFileID Identifier of the open netCDF file to check
 * @param[in] fileName Name of netCDF file to test (used for error messages)
 * @param[out] LogInfo Holds information on warnings and errors
*/
void SW_NC_check(SW_DOMAIN* SW_Domain, int ncFileID, const char* fileName,
                 LOG_INFO* LogInfo) {

    SW_CRS *crs_geogsc = &SW_Domain->netCDFInfo.crs_geogsc;
    SW_CRS *crs_projsc = &SW_Domain->netCDFInfo.crs_projsc;
    Bool geoIsPrimCRS = SW_Domain->netCDFInfo.primary_crs_is_geographic;
    char strAttVal[LARGE_VALUE];
    double doubleAttVal;
    const char *geoCRS = "crs_geogsc", *projCRS = "crs_projsc";
    Bool geoCRSExists = varExists(ncFileID, geoCRS);
    Bool projCRSExists = varExists(ncFileID, projCRS);
    const char* impliedDomType = (dimExists("site", ncFileID)) ? "s" : "xy";
    Bool dimMismatch = swFALSE;
    size_t latDimVal = 0, lonDimVal = 0, SDimVal = 0;

    const char* strAttsToComp[] = {"long_name", "grid_mapping_name", "crs_wkt"};
    const char* doubleAttsToComp[] = {
        "longitude_of_prime_meridian", "semi_major_axis", "inverse_flattening"
    };

    const char* strProjAttsToComp[] = {"datum", "units"};
    const char* doubleProjAttsToComp[] = {
        "longitude_of_central_meridian", "latitude_of_projection_origin",
        "false_easting", "false_northing"
    };
    const char* stdParallel = "standard_parallel";
    const double stdParVals[] = {crs_projsc->standard_parallel[0],
                                 crs_projsc->standard_parallel[1]};

    const char* geoStrAttVals[] = {
        crs_geogsc->long_name, crs_geogsc->grid_mapping_name, crs_geogsc->crs_wkt
    };
    const double geoDoubleAttVals[] = {
        crs_geogsc->longitude_of_prime_meridian, crs_geogsc->semi_major_axis,
        crs_geogsc->inverse_flattening
    };
    const char* projStrAttVals[] = {
        crs_projsc->long_name, crs_projsc->grid_mapping_name, crs_projsc->crs_wkt
    };
    const double projDoubleAttVals[] = {
        crs_projsc->longitude_of_prime_meridian, crs_projsc->semi_major_axis,
        crs_projsc->inverse_flattening
    };

    const char* strProjAttVals[] = {SW_Domain->netCDFInfo.crs_projsc.datum,
                                    SW_Domain->netCDFInfo.crs_projsc.units};
    const double doubleProjAttVals[] = {
        crs_projsc->longitude_of_central_meridian,
        crs_projsc->latitude_of_projection_origin,
        crs_projsc->false_easting,
        crs_projsc->false_northing,
    };

    const int numNormAtts = 3, numProjStrAtts = 2, numProjDoubleAtts = 4;
    double projStdParallel[2]; // Compare to standard_parallel is projected CRS
    int attNum;

    const char* attFailMsg = "The attribute '%s' of the variable '%s' "
            "within the file %s does not match the one in the domain input "
            "file. Please make sure these match.";

    /*
       Make sure the domain types are consistent
    */
    if(strcmp(SW_Domain->DomainType, impliedDomType) != 0) {
        LogError(LogInfo, LOGERROR, "The implied domain type within %s "
                    "does not match the one specified in the domain input "
                    "file ('%s'). Please make sure these match.",
                    fileName, SW_Domain->DomainType);
    }

    /*
       Make sure the dimensions of the netCDF file is consistent with the
       domain input file
    */
    if(strcmp(impliedDomType, "s") == 0) {
        get_dim_val(ncFileID, "site", &SDimVal, LogInfo);
        if(LogInfo->stopRun) {
            return; // Exit function prematurely due to error
        }

        dimMismatch = (Bool) (SDimVal != SW_Domain->nDimS);
    } else if(strcmp(impliedDomType, "xy") == 0) {
        if(geoIsPrimCRS && geoCRSExists) {
            get_dim_val(ncFileID, "lat", &latDimVal, LogInfo);
            if(LogInfo->stopRun) {
                return; // Exit function prematurely due to error
            }
            get_dim_val(ncFileID, "lon", &lonDimVal, LogInfo);
            if(LogInfo->stopRun) {
                return; // Exit function prematurely due to error
            }
        } else if(!geoIsPrimCRS && projCRSExists) {
            get_dim_val(ncFileID, "y", &latDimVal, LogInfo);
            if(LogInfo->stopRun) {
                return; // Exit function prematurely due to error
            }
            get_dim_val(ncFileID, "x", &lonDimVal, LogInfo);
            if(LogInfo->stopRun) {
                return; // Exit function prematurely due to error
            }
        } else {
            LogError(LogInfo, LOGERROR, "Could not find the proper CRS variable "
                                        "for the domain type/primary CRS.");
            return; // Exit function prematurely due to error
        }

        dimMismatch = (Bool) (latDimVal != SW_Domain->nDimY ||
                              lonDimVal != SW_Domain->nDimX);
    }

    if(dimMismatch) {
        LogError(LogInfo, LOGERROR, "The size of the dimensions in %s do "
                    "not match the domain input file's. Please make sure "
                    "these match.", fileName);
        return; // Exit function prematurely due to error
    }

    /*
       Make sure the geographic CRS information is consistent with the
       domain input file - both string and double values
    */
    if(geoCRSExists) {
        for(attNum = 0; attNum < numNormAtts; attNum++) {
            get_str_att_val(ncFileID, geoCRS, strAttsToComp[attNum],
                            strAttVal, LogInfo);
            if(LogInfo->stopRun) {
                return; // Exit function prematurely due to error
            }

            if(strcmp(geoStrAttVals[attNum], strAttVal) != 0) {
                LogError(LogInfo, LOGERROR, attFailMsg,
                        strAttsToComp[attNum], geoCRS, fileName);
                return; // Exit function prematurely due to error
            }
        }

        for(attNum = 0; attNum < numNormAtts; attNum++) {
            get_double_att_val(ncFileID, geoCRS, doubleAttsToComp[attNum],
                            &doubleAttVal, LogInfo);
            if(LogInfo->stopRun) {
                return; // Exit function prematurely due to error
            }

            if(doubleAttVal != geoDoubleAttVals[attNum]) {
                LogError(LogInfo, LOGERROR, attFailMsg,
                        doubleAttsToComp[attNum], geoCRS, fileName);
                return; // Exit function prematurely due to error
            }
        }
    } else {
        LogError(LogInfo, LOGERROR, "A geographic CRS was not found in %s. "
                                    "Please make sure one is provided.",
                                    fileName);
        return; // Exit function prematurely due to error
    }

    /*
       Test all projected CRS attributes - both string and double values -
       if applicable
    */
    if(!geoIsPrimCRS && projCRSExists) {
        // Normal attributes (same tested for in crs_geogsc)
        for(attNum = 0; attNum < numNormAtts; attNum++) {
            get_str_att_val(ncFileID, projCRS, strAttsToComp[attNum],
                            strAttVal, LogInfo);
            if(LogInfo->stopRun) {
                return; // Exit function prematurely due to error
            }

            if(strcmp(projStrAttVals[attNum], strAttVal) != 0) {
                LogError(LogInfo, LOGERROR, attFailMsg,
                        strAttsToComp[attNum], projCRS, fileName);
                return; // Exit function prematurely due to error
            }
        }

        for(attNum = 0; attNum < numNormAtts; attNum++) {
            get_double_att_val(ncFileID, projCRS, doubleAttsToComp[attNum],
                               &doubleAttVal, LogInfo);
            if(LogInfo->stopRun) {
                return; // Exit function prematurely due to error
            }

            if(doubleAttVal != projDoubleAttVals[attNum]) {
                LogError(LogInfo, LOGERROR, attFailMsg,
                        doubleAttsToComp[attNum], projCRS, fileName);
                return; // Exit function prematurely due to error
            }
        }

        // Projected CRS-only attributes
        for(attNum = 0; attNum < numProjStrAtts; attNum++) {
            get_str_att_val(ncFileID, projCRS, strProjAttsToComp[attNum],
                            strAttVal, LogInfo);
            if(LogInfo->stopRun) {
                return; // Exit function prematurely due to error
            }

            if(strcmp(strAttVal, strProjAttVals[attNum]) != 0) {
                LogError(LogInfo, LOGERROR, attFailMsg, strProjAttsToComp[attNum],
                         projCRS, fileName);
                return; // Exit function prematurely due to error
            }
        }

        for(attNum = 0; attNum < numProjDoubleAtts; attNum++) {
            get_double_att_val(ncFileID, projCRS, doubleProjAttsToComp[attNum],
                                &doubleAttVal, LogInfo);
            if(LogInfo->stopRun) {
                return; // Exit function prematurely due to error
            }

            if(doubleAttVal != doubleProjAttVals[attNum]) {
                LogError(LogInfo, LOGERROR, attFailMsg, doubleProjAttsToComp[attNum],
                         projCRS, fileName);
                return; // Exit function prematurely due to error
            }
        }

        // Test for standard_parallel
        get_double_att_val(ncFileID, projCRS, stdParallel, projStdParallel, LogInfo);
        if(LogInfo->stopRun) {
            return; // Exit function prematurely due to error
        }

        if(projStdParallel[0] != stdParVals[0] ||
           projStdParallel[1] != stdParVals[1]) {

            LogError(LogInfo, LOGERROR, attFailMsg, stdParallel,
                     projCRS, fileName);
        }
    }
}

/**
 * @brief Create domain netCDF template if it does not already exists
 *
 * @param[in] SW_Domain Struct of type SW_DOMAIN holding constant
 *  temporal/spatial information for a set of simulation runs
 * @param[out] LogInfo Holds information on warnings and errors
*/
void SW_NC_create_domain_template(SW_DOMAIN* SW_Domain, LOG_INFO* LogInfo) {

    SW_NETCDF* SW_netCDF = &SW_Domain->netCDFInfo;
    int* domFileID = &SW_Domain->netCDFInfo.ncFileIDs[vNCdom];
    int sDimID = 0, YDimID = 0, XDimID = 0;
    int domDims[2]; // Either [YDimID, XDimID] or [sDimID, 0]
    int nDomainDims, domVarID = 0, YVarID = 0, XVarID = 0, sVarID = 0;
    int YBndsID = 0, XBndsID = 0;

    if(FileExists(DOMAIN_TEMP)) {
        LogError(LogInfo, LOGERROR, "Could not create new domain template. "
                                    "This is due to the fact that it already "
                                    "exists. Please modify it and change the name.");
        return; // Exit prematurely due to error
    }

    #if defined(SOILWAT)
    if(LogInfo->printProgressMsg) {
        sw_message("is creating a domain template ...");
    }
    #endif

    if(nc_create(DOMAIN_TEMP, NC_NETCDF4, domFileID) != NC_NOERR) {
        LogError(LogInfo, LOGERROR, "Could not create new domain template due "
                                    "to something internal.");
        return; // Exit prematurely due to error
    }

    if(strcmp(SW_Domain->DomainType, "s") == 0) {
        nDomainDims = 1;

        // Create s dimension/domain variables
        fill_domain_netCDF_s(SW_Domain, domFileID, &sDimID, &sVarID,
                             &YVarID, &XVarID, LogInfo);

        if(LogInfo->stopRun) {
            nc_close(*domFileID);
            return; // Exit prematurely due to error
        }

        domDims[0] = sDimID;
        domDims[1] = 0;
    } else {
        nDomainDims = 2;

        fill_domain_netCDF_xy(SW_Domain, domFileID, &YDimID, &XDimID,
                              &YVarID, &XVarID, &YBndsID, &XBndsID,
                              LogInfo);

        if(LogInfo->stopRun) {
            nc_close(*domFileID);
            return; // Exit prematurely due to error
        }

        domDims[0] = YDimID;
        domDims[1] = XDimID;
    }

    // Create domain variable
    fill_domain_netCDF_domain(SW_netCDF->varNC[vNCdom], &domVarID,
                              domDims, *domFileID, nDomainDims,
                              SW_netCDF->primary_crs_is_geographic,
                              SW_Domain->DomainType, LogInfo);
    if(LogInfo->stopRun) {
        nc_close(*domFileID);
        return; // Exit function prematurely due to error
    }

    fill_netCDF_with_invariants(SW_netCDF, SW_Domain->DomainType,
                                domFileID, swTRUE, LogInfo);
    if(LogInfo->stopRun) {
        nc_close(*domFileID);
        return; // Exit function prematurely due to error
    }

    nc_enddef(*domFileID);

    fill_domain_netCDF_vals(SW_Domain, *domFileID, domVarID, sVarID,
                            YVarID, XVarID, YBndsID, XBndsID,
                            LogInfo);

    nc_close(*domFileID);
}

/**
 * @brief Copy domain netCDF as a template
 *
 * @param[in] domFile Name of the domain netCDF
 * @param[in] domFileID Identifier of the domain netCDF file
 * @param[in] fileName Name of the netCDF file to create
 * @param[in] newFileID Identifier of the netCDF file to create
 * @param[in] isInput Specifies if the created file will be input or output
 * @param[in] freq Value of the global attribute "frequency"
 * @param[out] LogInfo  Holds information dealing with logfile output
*/
void SW_NC_create_template(const char* domFile, int domFileID,
    const char* fileName, int* newFileID,
    Bool isInput, const char* freq,
    LOG_INFO* LogInfo) {

    Bool siteDimExists = dimExists("site", domFileID);
    const char* domType = (siteDimExists) ? "s" : "xy";


    CopyFile(domFile, fileName, LogInfo);
    if(LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

    if(nc_open(fileName, NC_WRITE, newFileID) != NC_NOERR) {
        LogError(LogInfo, LOGERROR, "An error occurred when attempting "
                                    "to access the new file %s.", fileName);
        return; // Exit function prematurely due to error
    }

    update_netCDF_global_atts(newFileID, domType, freq, isInput, LogInfo);
}

/**
 * @brief Create a progress netCDF file
 *
 * @param[in,out] SW_Domain Struct of type SW_DOMAIN holding constant
 *  temporal/spatial information for a set of simulation runs
 * @param[in,out] LogInfo Holds information dealing with logfile output
*/
void SW_NC_create_progress(SW_DOMAIN* SW_Domain, LOG_INFO* LogInfo) {

    SW_NETCDF* SW_netCDF = &SW_Domain->netCDFInfo;
    Bool primCRSIsGeo = SW_Domain->netCDFInfo.primary_crs_is_geographic;
    Bool domTypeIsS = (Bool) (strcmp(SW_Domain->DomainType, "s") == 0);
    const char* projGridMap = "crs_projsc: x y crs_geogsc: lat lon";
    const char* geoGridMap = "crs_geogsc";
    const char* sCoord = "lat lon site";
    const char* xyCoord = "lat lon";
    const char* coord = domTypeIsS ? sCoord : xyCoord;
    const char* grid_map = primCRSIsGeo ? geoGridMap : projGridMap;
    const char* attNames[] = {"long_name", "units", "grid_mapping",
                              "coordinates"};
    const char* attVals[] = {"simulation progress", "1", grid_map, coord};
    const int numAtts = 4;
    int numValsToWrite;
    const signed char fillVal = NC_FILL_BYTE;
    const signed char flagVals[] = {PRGRSS_FAIL, PRGRSS_READY, PRGRSS_DONE};
    const char* flagMeanings = "simulation_error ready_to_simulate simulation_complete";
    const char* progVarName = SW_netCDF->varNC[vNCprog];
    const char* freq = "fx";

    int domFileID = SW_netCDF->ncFileIDs[vNCdom];
    int* progFileID = &SW_netCDF->ncFileIDs[vNCprog];
    const char* domFileName = SW_netCDF->InFilesNC[vNCdom];
    const char* progFileName = SW_netCDF->InFilesNC[vNCprog];
    int* progVarID = &SW_netCDF->ncVarIDs[vNCprog];

    // create_full_var/SW_NC_create_template
    // No time variable/dimension
    double startTime = 0;

    Bool progFileIsDom = (Bool) (strcmp(progFileName, domFileName) == 0);
    Bool progFileExists = FileExists(progFileName);
    Bool progVarExists = varExists(*progFileID, progVarName);
    Bool createOrModFile = (Bool) (!progFileExists || (progFileIsDom && !progVarExists));

    /*
      If the progress file is not to be created or modified, check it

      See if the progress variable exists within it's file, also handling
      the case where the progress variable is in the domain netCDF

      In addition to making sure the file exists, make sure the progress
      variable is present
    */
    if(!createOrModFile) {
        SW_NC_check(SW_Domain, *progFileID, progFileName, LogInfo);
    } else {

        #if defined(SOILWAT)
        if(LogInfo->printProgressMsg) {
            sw_message("is creating a progress tracker ...");
        }
        #endif

        // No need for various information when creating the progress netCDF
        // like start year, start time, base calendar year, layer depths
        // and period
        if(progFileExists) {
            nc_redef(*progFileID);
        } else {
            SW_NC_create_template(domFileName, domFileID, progFileName,
                progFileID, swFALSE, freq, LogInfo);
        }

        create_full_var(progFileID, NC_BYTE, 0, 0, 0, progVarName,
                        attNames, attVals, numAtts, NULL,
                        &startTime, 0, 0, 0, LogInfo);

        if(LogInfo->stopRun) {
            return; // Exit function prematurely due to error
        }

        get_var_identifier(*progFileID, progVarName, progVarID, LogInfo);
        if(LogInfo->stopRun) {
            return; // Exit function prematurely due to error
        }

        // If the progress existed before this function was called,
        // do not set the new attributes
        if(!progVarExists) {
            // Add attribute "_FillValue" to the progress variable
            numValsToWrite = 1;
            write_byte_att("_FillValue", &fillVal, *progVarID, *progFileID, numValsToWrite, LogInfo);
            if(LogInfo->stopRun) {
                return; // Exit function prematurely due to error
            }

            // Add attributes "flag_values" and "flag_meanings"
            numValsToWrite = 3;
            write_byte_att("flag_values", flagVals, *progVarID, *progFileID, numValsToWrite, LogInfo);
            if(LogInfo->stopRun) {
                return; // Exit function prematurely due to error
            }

            write_str_att("flag_meanings", flagMeanings, *progVarID, *progFileID, LogInfo);
            if(LogInfo->stopRun) {
                return; // Exit function prematurely due to error
            }
        }


        nc_enddef(*progFileID);

        fill_prog_netCDF_vals(SW_Domain, LogInfo);
    }
}

/**
 * @brief Mark a site/gridcell as completed (success/fail) in the progress file
 *
 * @param[in] isFailure Did simulation run fail or succeed?
 * @param[in] domType Type of domain in which simulations are running
 *  (gridcell/sites)
 * @param[in] progFileID Identifier of the progress netCDF file
 * @param[in] progVarID Identifier of the progress variable within the progress netCDF
 * @param[in] ncSUID Current simulation unit identifier for which is used
 *  to get data from netCDF
 * @param[in,out] LogInfo Holds information dealing with logfile output
*/
void SW_NC_set_progress(Bool isFailure, const char* domType, int progFileID,
                        int progVarID, unsigned long ncSUID[],
                        LOG_INFO* LogInfo) {

    const signed char mark = (isFailure) ? PRGRSS_FAIL : PRGRSS_DONE;
    size_t count1D[] = {1}, count2D[] = {1, 1};
    size_t *count = (strcmp(domType, "s") == 0) ? count1D : count2D;

    fill_netCDF_var_byte(progFileID, progVarID, &mark, ncSUID, count, LogInfo);
    nc_sync(progFileID);
}

/**
 * @brief Check if a site/grid cell is marked to be run in the progress
 *  netCDF
 *
 * @param[in] progFileID Identifier of the progress netCDF file
 * @param[in] progVarID Identifier of the progress variable
 * @param[in] ncSUID Current simulation unit identifier for which is used
 *  to get data from netCDF
 * @param[in,out] LogInfo Holds information dealing with logfile output
*/
Bool SW_NC_check_progress(int progFileID, int progVarID,
                          unsigned long ncSUID[], LOG_INFO* LogInfo) {

    signed char progVal = 0;

    get_single_byte_val(progFileID, progVarID, ncSUID, &progVal, LogInfo);

    return (Bool) (!LogInfo->stopRun && progVal == PRGRSS_READY);
}

/**
 * @brief Read values from netCDF input files for available variables and copy to SW_All
 *
 * @param[in,out] sw Comprehensive struct of type SW_ALL containing
 *  all information in the simulation
 * @param[in] SW_Domain Struct of type SW_DOMAIN holding constant
 *  temporal/spatial information for a set of simulation runs
 * @param[in] ncSUID Current simulation unit identifier for which is used
 *  to get data from netCDF
 * @param[out] LogInfo Holds information on warnings and errors
*/
void SW_NC_read_inputs(SW_ALL* sw, SW_DOMAIN* SW_Domain, size_t ncSUID[],
                       LOG_INFO* LogInfo) {

    int file, varNum;
    Bool domTypeS = (Bool) (Str_CompareI(SW_Domain->DomainType, (char *)"s") == 0);
    const int numInFilesNC = 1;
    const int numDomVals = 2;
    const int numVals[] = {numDomVals};
    const int ncFileIDs[] = {SW_Domain->netCDFInfo.ncFileIDs[vNCdom]};
    const char* domLatVar = "lat", *domLonVar = "lon";
    const char* varNames[][2] = {{domLatVar, domLonVar}};
    int ncIndex;

    RealD *values[][2] = {{&sw->Model.latitude, &sw->Model.longitude}};

    /*
        Gather all values being requested within the array "values"
        The index within the netCDF file for domain type "s" is simply the
        first index in "ncSUID"
        For the domain type "xy", the index of the variable "y" is the first
        in "ncSUID" and the index of the variable "x" is the second in "ncSUID"
    */
    for(file = 0; file < numInFilesNC; file++) {
        for(varNum = 0; varNum < numVals[file]; varNum++) {
            ncIndex = (domTypeS) ? 0 : varNum % 2;

            get_single_double_val(ncFileIDs[file], varNames[file][varNum],
                                  &ncSUID[ncIndex], values[file][varNum], LogInfo);
            if(LogInfo->stopRun) {
                return; // Exit function prematurely due to error
            }
        }
    }
}

/**
 * @brief Check that all available netCDF input files are consistent with domain
 *
 * @param[in] SW_Domain Struct of type SW_DOMAIN holding constant
 *  temporal/spatial information for a set of simulation runs
 * @param[out] LogInfo Holds information on warnings and errors
*/
void SW_NC_check_input_files(SW_DOMAIN* SW_Domain, LOG_INFO* LogInfo) {
    int file;

    for(file = 0; file < SW_NVARNC; file++) {
        SW_NC_check(SW_Domain, SW_Domain->netCDFInfo.ncFileIDs[file],
                    SW_Domain->netCDFInfo.InFilesNC[file], LogInfo);
        if(LogInfo->stopRun) {
            return; // Exit function prematurely due to inconsistent data
        }
    }

}

/**
 * @brief Read input files for netCDF related actions
 *
 * @param[in,out] SW_netCDF Struct of type SW_NETCDF holding constant
 *  netCDF file information
 * @param[in,out] PathInfo Struct holding all information about the programs path/files
 * @param[out] LogInfo Holds information on warnings and errors
*/
void SW_NC_read(SW_NETCDF* SW_netCDF, PATH_INFO* PathInfo, LOG_INFO* LogInfo) {
    static const char* possibleKeys[NUM_NC_IN_KEYS] = {"domain", "progress"};
    static const Bool requiredKeys[NUM_NC_IN_KEYS] =
            {swTRUE, swTRUE};
    Bool hasKeys[NUM_NC_IN_KEYS] = {swFALSE, swFALSE};

    FILE *f;
    char inbuf[MAX_FILENAMESIZE], *MyFileName;
    char key[15]; // 15 - Max key size
    char varName[MAX_FILENAMESIZE], path[MAX_FILENAMESIZE];
    int keyID;

    MyFileName = PathInfo->InFiles[eNCIn];
	f = OpenFile(MyFileName, "r", LogInfo);

    // Get domain file name
    while(GetALine(f, inbuf, MAX_FILENAMESIZE)) {
        sscanf(inbuf, "%14s %s %s", key, varName, path);

        keyID = key_to_id(key, possibleKeys, NUM_NC_IN_KEYS);
        set_hasKey(keyID, possibleKeys, hasKeys, LogInfo); // no error, only warnings possible

        switch(keyID) {
            case vNCdom:
                SW_netCDF->varNC[vNCdom] = Str_Dup(varName, LogInfo);
                SW_netCDF->InFilesNC[vNCdom] = Str_Dup(path, LogInfo);
                break;
            case vNCprog:
                SW_netCDF->varNC[vNCprog] = Str_Dup(varName, LogInfo);
                SW_netCDF->InFilesNC[vNCprog] = Str_Dup(path, LogInfo);
                break;
            default:
                LogError(LogInfo, LOGWARN, "Ignoring unknown key in %s, %s",
                         MyFileName, key);
                break;
        }
    }

    CloseFile(&f, LogInfo);

    // Check if all required input was provided
    check_requiredKeys(hasKeys, requiredKeys, possibleKeys, NUM_NC_IN_KEYS, LogInfo);
    if(LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

    // Read CRS and attributes for netCDFs
    nc_read_atts(SW_netCDF, PathInfo, LogInfo);
}

/**
 * @brief Read the user-provided tsv file that contains information about
 *  output variables in netCDFs
 *
 * If a user turns off all variables of an outkey group, then
 * the entire outkey group is turned off.
 *
 * Lack of information for a variable in the tsv file is equivalent to
 * turning off the output of that variable. For instance, an empty tsv file
 * results in no output produced.
 *
 * @param[in,out] SW_Output SW_OUTPUT array of size SW_OUTNKEYS which holds
 *  basic output information for all output keys
 * @param[in] InFiles Array of program in/output files
 * @param[in] parms Array of type SW_VEGESTAB_INFO holding information about
 *  species
 * @param[out] LogInfo Holds information on warnings and errors
*/
void SW_NC_read_out_vars(SW_OUTPUT* SW_Output, char* InFiles[],
                    SW_VEGESTAB_INFO** parms, LOG_INFO* LogInfo) {

    FILE *f;
    OutKey currOutKey;
    SW_OUTPUT* currOut;
    char inbuf[MAX_FILENAMESIZE], *MyFileName;
    char varKey[MAX_FILENAMESIZE];
    int varNum = 0, lineno = 0;

    Bool estabFound = swFALSE;
    Bool used_OutKeys[SW_OUTNKEYS] = {swFALSE};
    int index, numVars, estVar;
    char* copyStr = NULL;
    char input[NOUT_VAR_INPUTS][MAX_ATTVAL_SIZE] = {"\0"};
    char establn[MAX_ATTVAL_SIZE] = {"\0"};
    int scanRes = 0, defToLocalInd = 0;
    const char* readLineFormat =
        "%13[^\t]\t%50[^\t]\t%10[^\t]\t%4[^\t]\t%1[^\t]\t"
        "%127[^\t]\t%127[^\t]\t%127[^\t]\t%127[^\t]\t%127[^\t]\t%127[^\t]";

    // Column indices
    const int keyInd = 0, SWVarNameInd = 1, SWUnitsInd = 2, dimInd = 3,
              doOutInd = 4, outVarNameInd = 5, longNameInd = 6,
              commentInd = 7, outUnits = 8, cellMethodInd = 9,
              usercommentInd = 10;

    MyFileName = InFiles[eNCOutVars];
	f = OpenFile(MyFileName, "r", LogInfo);
    if(LogInfo->stopRun) {
        return; // Exit prematurely due to error
    }

    init_output_var_info(SW_Output, LogInfo);
    if(LogInfo->stopRun) {
        return; // Exit prematurely due to error
    }

    GetALine(f, inbuf, MAX_FILENAMESIZE); // Ignore the first row (column names)

    while(GetALine(f, inbuf, MAX_FILENAMESIZE)) {
        lineno++;

        // Ignore columns after "cell_method"
        scanRes = sscanf(inbuf, readLineFormat, input[keyInd],
                input[SWVarNameInd], input[SWUnitsInd], input[dimInd],
                input[doOutInd], input[outVarNameInd], input[longNameInd],
                input[commentInd], input[outUnits], input[cellMethodInd],
                input[usercommentInd]);

        if(scanRes != NOUT_VAR_INPUTS) {
            LogError(LogInfo, LOGERROR,
                        "%s [row %d]: %d instead of %d columns found. "
                        "Enter 'NA' if value should be blank "
                        "(e.g., for 'long_name' or 'comment').",
                        MyFileName, lineno + 1, scanRes, NOUT_VAR_INPUTS);
            return; // Exit function prematurely due to error
        }

        // Check if the variable was requested to be output
        // Store attribute information for each variable (including names)
        if(atoi(input[doOutInd])) {

            snprintf(varKey, MAX_FILENAMESIZE, "%s__%s",
                     input[keyInd], input[SWVarNameInd]);

            get_2d_output_key(varKey, &currOutKey, &varNum);

            if(currOutKey == eSW_NoKey) {
                    LogError(LogInfo, LOGWARN,
                                    "%s [row %d]: Could not interpret the input combination "
                                    "of key/variable: '%s'/'%s'.",
                                    MyFileName, lineno + 1, input[keyInd], input[SWVarNameInd]);
                    continue;
            }

            currOut = &SW_Output[currOutKey];

            if (!currOut->use) {
                // key not in use
                // don't output any of the variables within that outkey group
                continue;
            }

            used_OutKeys[currOutKey] = swTRUE; // track if any variable is requested

            if(currOutKey == eSW_Estab) {
                // Handle establishment different since it is "dynamic"
                // (SW_VEGESTAB'S "count")
                if(estabFound) {
                    LogError(LogInfo, LOGWARN, "%s: Found more than one row for "
                                        "the key ESTABL, only one is expected, "
                                        "ignoring...", MyFileName);
                    continue;
                }

                estabFound = swTRUE;
                varNum = 0;
            }

            if(isnull(currOut->outputVarInfo)) {
                SW_NC_init_outvars(&currOut->outputVarInfo,
                                            currOutKey, LogInfo);
                if(LogInfo->stopRun) {
                    return; // Exit function prematurely due to error
                }
            }

            currOut->reqOutputVars[varNum] = swTRUE;
            currOut->outputVarInfo[varNum][DIM_INDEX] =
                                            Str_Dup(input[dimInd], LogInfo);
            if(LogInfo->stopRun) {
                return; // Exit function prematurely due to error
            }

            // Read in the rest of the attributes
            // Output variable name, long name, comment, units, and cell_method
           for(index = VARNAME_INDEX; index <= CELLMETHOD_INDEX; index++) {
                defToLocalInd = index + doOutInd;

                if(strcmp(input[defToLocalInd], "NA") == 0) {
                    copyStr = (char *)"";
                } else {
                    copyStr = input[defToLocalInd];
                }

                // Handle ESTAB differently by storing all attributes
                // into `count` amount of variables and give the
                // correct <sppname>'s
                if(currOutKey == eSW_Estab) {
                    numVars = numVarsPerKey[currOutKey];
                    for(estVar = 0; estVar < numVars; estVar++) {

                        switch(index) {
                            case VARNAME_INDEX:
                                currOut->reqOutputVars[estVar] = swTRUE;
                                currOut->outputVarInfo[estVar][index] =
                                                Str_Dup(parms[estVar]->sppname, LogInfo);
                                currOut->outputVarInfo[estVar][DIM_INDEX] =
                                                Str_Dup(input[dimInd], LogInfo);
                                break;

                            case LONGNAME_INDEX:
                                snprintf(establn, MAX_ATTVAL_SIZE - 1, copyStr, parms[estVar]->sppname);
                                currOut->outputVarInfo[estVar][index] =
                                                Str_Dup(establn, LogInfo);
                                break;

                            default:
                                currOut->outputVarInfo[estVar][index] =
                                                            Str_Dup(copyStr, LogInfo);
                                break;
                        }

                        if(LogInfo->stopRun) {
                            return; // Exit function prematurely due to error
                        }
                    }
                } else {
                    currOut->outputVarInfo[varNum][index] =
                                                Str_Dup(copyStr, LogInfo);
                    if(LogInfo->stopRun) {
                        return; // Exit function prematurely due to error
                    }
                }
            }

        }
    }


    // Update "use": turn off if no variable of an outkey group is requested
    ForEachOutKey(index) {
        if (!used_OutKeys[index]) {
            SW_Output[index].use = swFALSE;
        }
    }
}

/**
 * @brief Initializes pointers within the type SW_NETCDF and SW_CRS
 *
 * @param[in,out] SW_netCDF Struct of type SW_NETCDF holding constant
 *  netCDF input/output file information
*/
void SW_NC_init_ptrs(SW_NETCDF* SW_netCDF) {

    int index;
    const int numAllocVars = 13;
    char **allocArr[] = {
        &SW_netCDF->title, &SW_netCDF->author, &SW_netCDF->institution,
        &SW_netCDF->comment, &SW_netCDF->coordinate_system,

        &SW_netCDF->crs_geogsc.long_name, &SW_netCDF->crs_geogsc.grid_mapping_name,
        &SW_netCDF->crs_geogsc.crs_wkt, // geogsc does not use datum and units

        &SW_netCDF->crs_projsc.long_name, &SW_netCDF->crs_projsc.grid_mapping_name,
        &SW_netCDF->crs_projsc.crs_wkt, &SW_netCDF->crs_projsc.datum,
        &SW_netCDF->crs_projsc.units
    };

    SW_netCDF->crs_projsc.standard_parallel[0] = NAN;
    SW_netCDF->crs_projsc.standard_parallel[1] = NAN;

    SW_netCDF->strideOutYears = -1;

    for(index = 0; index < numAllocVars; index++) {
        *allocArr[index] = NULL;
    }

    // Files/variables
    for(index = 0; index < SW_NVARNC; index++) {

        SW_netCDF->varNC[index] = NULL;
        SW_netCDF->InFilesNC[index] = NULL;
    }

    (void) allocArr; // Silence compiler
}

/**
 * @brief Deconstruct netCDF-related information
 *
 * @param[in,out] SW_netCDF Struct of type SW_NETCDF holding constant
 *  netCDF input/output file information
*/
void SW_NC_deconstruct(SW_NETCDF* SW_netCDF) {

    int index;
    const int numFreeVars = 13;
    char *freeArr[] = {
        SW_netCDF->title, SW_netCDF->author, SW_netCDF->institution,
        SW_netCDF->comment, SW_netCDF->coordinate_system,

        SW_netCDF->crs_geogsc.long_name, SW_netCDF->crs_geogsc.grid_mapping_name,
        SW_netCDF->crs_geogsc.crs_wkt, // geogsc does not use datum and units

        SW_netCDF->crs_projsc.long_name, SW_netCDF->crs_projsc.grid_mapping_name,
        SW_netCDF->crs_projsc.crs_wkt, SW_netCDF->crs_projsc.datum,
        SW_netCDF->crs_projsc.units
    };

    for(index = 0; index < numFreeVars; index++) {
        if(!isnull(freeArr[index])) {
            free(freeArr[index]);
            freeArr[index] = NULL;
        }
    }

    for(index = 0; index < SW_NVARNC; index++) {
        if(!isnull(SW_netCDF->varNC[index])) {
            free(SW_netCDF->varNC[index]);
            SW_netCDF->varNC[index] = NULL;
        }

        if(!isnull(SW_netCDF->InFilesNC[index])) {
            free(SW_netCDF->InFilesNC[index]);
            SW_netCDF->InFilesNC[index] = NULL;
        }
    }
}

/**
 * @brief Open netCDF file(s) that contain(s) domain and progress variables
 *
 * These files are kept open during simulations
 *   * to read geographic coordinates from the domain
 *   * to identify and update progress
 *
 * @param[in,out] SW_netCDF Struct of type SW_NETCDF holding constant
 *  netCDF file information
 * @param[out] LogInfo Holds information on warnings and errors
*/
void SW_NC_open_dom_prog_files(SW_NETCDF* SW_netCDF, LOG_INFO* LogInfo) {
    int fileNum, openType = NC_WRITE, *fileID;
    char* fileName, *domFile = SW_netCDF->InFilesNC[vNCdom];
    char* progFile = SW_netCDF->InFilesNC[vNCprog];
    Bool progFileDomain = (Bool) (strcmp(domFile, progFile) == 0);

    // Open the domain/progress netCDF
    for(fileNum = vNCdom; fileNum <= vNCprog; fileNum++) {
        fileName = SW_netCDF->InFilesNC[fileNum];
        fileID = &SW_netCDF->ncFileIDs[fileNum];

        if(FileExists(fileName)) {
            if(nc_open(fileName, openType, fileID) != NC_NOERR) {
                LogError(LogInfo, LOGERROR, "An error occurred when opening %s.",
                                            fileName);
                return; // Exit function prematurely due to error
            }

            /*
              Get the ID for the domain variable and the progress variable if
              it is not in the domain netCDF or it exists in the domain netCDF
            */
            if(fileNum == vNCdom || !progFileDomain ||
                varExists(*fileID, SW_netCDF->varNC[fileNum])) {

                get_var_identifier(*fileID, SW_netCDF->varNC[fileNum],
                                    &SW_netCDF->ncVarIDs[fileNum], LogInfo);
                if(LogInfo->stopRun) {
                    return; // Exit function prematurely due to error
                }
            }
        }
    }

    // If the progress variable is contained in the domain netCDF, then
    // close the (redundant) progress file identifier
    // and use instead the (equivalent) domain file identifier
    if(progFileDomain) {
        nc_close(SW_netCDF->ncFileIDs[vNCprog]);
        SW_netCDF->ncFileIDs[vNCprog] = SW_netCDF->ncFileIDs[vNCdom];
    }
}

/**
 * @brief Close all netCDF files that have been opened while the program ran
 *
 * @param[in,out] SW_netCDF Struct of type SW_NETCDF holding constant
 *  netCDF file information
*/
void SW_NC_close_files(SW_NETCDF* SW_netCDF) {
    int fileNum;

    for(fileNum = 0; fileNum < SW_NVARNC; fileNum++) {
        nc_close(SW_netCDF->ncFileIDs[fileNum]);
    }
}

/**
 * @brief Deep copy a source instance of SW_NETCDF into a destination instance
 *
 * @param[in] source Source struct of type SW_NETCDF to copy
 * @param[out] dest Destination struct of type SW_NETCDF to be copied into
 * @param[out] LogInfo Holds information on warnings and errors
*/
void SW_NC_deepCopy(SW_NETCDF* source, SW_NETCDF* dest, LOG_INFO* LogInfo) {
    int index, numIndivCopy = 5;

    char* srcStrs[] = {
        source->title, source->author, source->institution, source->comment,
        source->coordinate_system
    };

    char** destStrs[] = {
        &dest->title, &dest->author, &dest->institution, &dest->comment,
        &dest->coordinate_system
    };

    memcpy(dest, source, sizeof(*dest));

    SW_NC_init_ptrs(dest);

    for(index = 0; index < numIndivCopy; index++) {
        *destStrs[index] = Str_Dup(srcStrs[index], LogInfo);
        if(LogInfo->stopRun) {
            return; // Exit function prematurely due to
        }
    }

    for(index = 0; index < SW_NVARNC; index++) {
        dest->varNC[index] = Str_Dup(source->varNC[index], LogInfo);
        if(LogInfo->stopRun) {
            return; // Exit function prematurely due to error
        }

        dest->InFilesNC[index] = Str_Dup(source->InFilesNC[index], LogInfo);
        if(LogInfo->stopRun) {
            return; // Exit function prematurely due to error
        }
    }
}

/**
 * @brief Initialize information in regards to output variables
 *  respective to SW_OUTPUT instances
 *
 * @param[out] outkeyVars Holds all information about output variables
 *  in netCDFs (e.g., output variable name)
 * @param[in] currOutKey Specifies what output key is currently being allocated
 *  (i.e., temperature, precipitation, etc.)
 * @param[out] LogInfo Holds information on warnings and errors
*/
void SW_NC_init_outvars(char**** outkeyVars, OutKey currOutKey,
                            LOG_INFO* LogInfo) {

    *outkeyVars = NULL;

    if (numVarsPerKey[currOutKey] > 0) {

        int index, varNum, attNum;

        // Allocate all memory for the variable information in the current
        // output key
        *outkeyVars =
            (char ***)Mem_Malloc(sizeof(char **) * numVarsPerKey[currOutKey],
                                "SW_NC_init_outvars()", LogInfo);
            if(LogInfo->stopRun) {
                return; // Exit function prematurely due to error
            }

        for(index = 0; index < numVarsPerKey[currOutKey]; index++) {
            (*outkeyVars)[index] = NULL;
        }

        for(index = 0; index < numVarsPerKey[currOutKey]; index++) {
            (*outkeyVars)[index] =
                (char **)Mem_Malloc(sizeof(char *) * NUM_OUTPUT_INFO,
                    "SW_NC_init_outvars()", LogInfo);
            if(LogInfo->stopRun) {
                for(varNum = 0; varNum < index; varNum++) {
                    free((*outkeyVars)[varNum]);
                }
                free(*outkeyVars);
                return; // Exit function prematurely due to error
            }

            for(attNum = 0; attNum < NUM_OUTPUT_INFO; attNum++) {
                (*outkeyVars)[index][attNum] = NULL;
            }
        }
    }
}

/**
 * @brief Initialize information about whether or not a variable should
 *  be output
 *
 * @param[out] reqOutVar Specifies the number of variables that can be output
 *  for a given output key
 * @param[in] currOutKey Specifies what output key is currently being allocated
 *  (i.e., temperature, precipitation, etc.)
 * @param[out] LogInfo Holds information on warnings and errors
*/
void SW_NC_init_outReq(Bool** reqOutVar, OutKey currOutKey, LOG_INFO* LogInfo) {

    *reqOutVar = NULL;

    if (numVarsPerKey[currOutKey] > 0) {

        int index;

        // Initialize the variable within SW_OUTPUT which specifies if a variable
        // is to be written out or not
        *reqOutVar =
            (Bool *)Mem_Malloc(sizeof(Bool) * numVarsPerKey[currOutKey],
                                "SW_NC_init_outReq()", LogInfo);
        if(LogInfo->stopRun) {
            return; // Exit function prematurely due to error
        }

        for(index = 0; index < numVarsPerKey[currOutKey]; index++) {
            (*reqOutVar)[index] = swFALSE;
        }
    }
}

/**
 * @brief Allocate memory for files within SW_FILE_STATUS for future
 *  functions to write to/create
 *
 * @param[out] ncOutFiles Output file names storage array
 * @param[in] numFiles Number of file names to store/allocate memory for
 * @param[out] LogInfo Holds information on warnings and errors
*/
void SW_NC_alloc_files(char*** ncOutFiles, int numFiles, LOG_INFO* LogInfo) {

    int varNum;

    *ncOutFiles = (char **) Mem_Malloc(numFiles * sizeof(char *),
                            "SW_NC_create_output_files()", LogInfo);
    if(LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

    for(varNum = 0; varNum < numFiles; varNum++) {
        (*ncOutFiles)[varNum] = NULL;
    }
}
