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

/* =================================================== */
/*                   Local Defines                     */
/* --------------------------------------------------- */

#define NUM_NC_IN_KEYS 1 // Number of possible keys within `files_nc.in`
#define NUM_ATT_IN_KEYS 25 // Number of possible keys within `attributes_nc.in`

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
            "proj_false_northing"
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
            swFALSE};
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
 * @brief Get a dimension identifier within a given netCDF
 *
 * @param[in] ncFileID Identifier of the open netCDF file to access
 * @param[in] dimName Name of the new dimension
 * @param[out] dimID Identifier of the dimension
 * @param[out] LogInfo Holds information on warnings and errors
*/
static void get_dim_identifier(int ncFileID, char* dimName, int* dimID,
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
 * @brief Get a dimension value from a given netCDF file
 *
 * @param[in] ncFileID Identifier of the open netCDF file to access
 * @param[in] dimName Name of the dimension to access
 * @param[out] dimVal String buffer to hold the resulting value
 * @param[out] LogInfo Holds information on warnings and errors
*/
static void get_dim_val(int ncFileID, char* dimName, size_t* dimVal,
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
    static char* wgs84_synonyms[] = {"WGS84", "WGS 84", "EPSG:4326",
                                     "WGS_1984", "World Geodetic System 1984"};

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
 * @brief Write an attribute of type unsigned integer to a variable
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
 * @param[in] attVal Attribute value to write out
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
    if(strcmp(varName, "crs_geogsc") != 0 && strcmp(varName, "crs_projsc") != 0) {

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
 * @param[out] latVals Array to hold all latitude coordinate values
 * @param[out] lonVals Array to hold all longitude coordinate values
 * @param[out] latBndsVals Array to hold all "lat_bnds" values
 * @param[out] lonBndsVals Array to hold all "lon_bnds" values
 * @param[out] yBndsVals Array to hold all "y_bnds" values
 * @param[out] xBndsVals Array to hold all "x_bnds" values
 * @param[out] domVals Array to hold all values for the domain variable
*/
static void dealloc_netCDF_domain_vars(double **latVals, double **lonVals,
            double **latBndsVals, double **lonBndsVals, double **yBndsVals,
            double **xBndsVals, unsigned int **domVals) {

    const int numVars = 6;
    double **vars[] = {latVals, lonVals, latBndsVals, lonBndsVals,
                       yBndsVals, xBndsVals};
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
 * @brief Helper function to `fill_domain_netCDF_vars()` to allocate
 *  memory for writing out values
 *
 * @param[in] domTypeIsSite Specifies if the domain type is "s"
 * @param[in] fillGeo Specifies if the geographic CRS values will be filled
 * @param[in] nSUIDs Number of sites or gridcells
 * @param[in] numLatVals Number of latitude values
 * @param[in] numLonVals Number of longitude values
 * @param[out] latVals Array to hold all latitude coordinate values
 * @param[out] lonVals Array to hold all longitude coordinate values
 * @param[out] latBndsVals Array to hold all "lat_bnds" values
 * @param[out] lonBndsVals Array to hold all "lon_bnds" values
 * @param[out] yBndsVals Array to hold all "y_bnds" values
 * @param[out] xBndsVals Array to hold all "x_bnds" values
 * @param[out] domVals Array to hold all values for the domain variable
 * @param[out] LogInfo Holds information on warnings and errors
*/
static void alloc_netCDF_domain_vars(Bool domTypeIsSite, Bool fillGeo,
            int nSUIDs, unsigned int numLatVals, unsigned int numLonVals,
            double **latVals, double **lonVals, double **latBndsVals,
            double **lonBndsVals, double **yBndsVals, double **xBndsVals,
            unsigned int **domVals, LOG_INFO *LogInfo) {

    double **vars[] = {latVals, lonVals};
    double **bndsVars[] = {latBndsVals, lonBndsVals, yBndsVals, xBndsVals};
    const int startBndVar = (fillGeo) ? 0 : 2, numBndsVars = 4;
    const int numVars = 2, numBnds = 2;
    int varNum, bndVarNum, numVals;

    for(varNum = 0; varNum < numVars; varNum++) {
        numVals = (varNum % 2 == 0) ? numLatVals : numLonVals;
        *(vars[varNum]) = (double *) Mem_Malloc(numVals * sizeof(double),
                                                "alloc_netCDF_domain_vars()",
                                                LogInfo);
        if(LogInfo->stopRun) {
            return; // Exit function prematurely due to error
        }
    }

    if(!domTypeIsSite) {
        for(bndVarNum = startBndVar; bndVarNum < numBndsVars; bndVarNum++) {
            numVals = (bndVarNum % 2 == 0) ? numLatVals : numLonVals;

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
 * @param[in] latVarID Horizontal coordinate "lat" or "y" variable identifier
 * @param[in] lonVarID Horizontal coordinate "lon" or "x" variable identifier
 * @param[in] latBndsID Bounding variable "lat_bnds" or "y_bnds" variable identifier
 * @param[in] lonBndsID Bounding variable "lon_bnds" or "x_bnds" variable identifier
 * @param[out] yBndsID Bounding variable "y_bnds" variable identifier
 * @param[out] xBndsID Bounding variable "x_bnds" variable identifier
 * @param[out] LogInfo Holds information on warnings and errors
*/
static void fill_domain_netCDF_vals(SW_DOMAIN* SW_Domain, int domFileID,
        int domID, int siteID, int latVarID, int lonVarID, int latBndsID,
        int lonBndsID, int yBndsID, int xBndsID, LOG_INFO* LogInfo) {

    Bool domTypeIsSite = (Bool)(strcmp(SW_Domain->DomainType, "s") == 0);
    unsigned int suidNum, gridNum = 0, latComp, *domVals = NULL, bndsIndex;
    double *latVals = NULL, *lonVals = NULL;
    double *latBndsVals = NULL, *lonBndsVals = NULL;
    double *yBndsVals = NULL, *xBndsVals = NULL;
    size_t start[] = {0, 0}, domCount[2]; // 2 - [#lat dim, #lon dim] or [#sites, 0]
    size_t latFillCount[1], lonFillCount[1];
    size_t latBndsFillCount[] = {SW_Domain->nDimY, 2};
    size_t lonBndsFillCount[] = {SW_Domain->nDimX, 2};
    double resY, resX;
    unsigned int numLonVals = (domTypeIsSite) ? SW_Domain->nDimS : SW_Domain->nDimX;
    unsigned int numLatVals = (domTypeIsSite) ? SW_Domain->nDimS : SW_Domain->nDimY;
    char* geo_long_name = SW_Domain->netCDFInfo.crs_geogsc.long_name;
    Bool fillGeo = Str_CompareI(SW_Domain->crs_bbox, geo_long_name) == 0 ||
                    (is_wgs84(SW_Domain->crs_bbox) && is_wgs84(geo_long_name));

    int fillVarIDs[4] = {latVarID, lonVarID}; // Fill other slots later if needed
    double **fillVals[4] = {&latVals, &lonVals}; // Fill other slots later if needed
    size_t *fillCounts[] = {latFillCount, lonFillCount, latBndsFillCount,
                            lonBndsFillCount};
    int numVars, varNum;

    alloc_netCDF_domain_vars(domTypeIsSite, fillGeo, SW_Domain->nSUIDs,
                             numLatVals, numLonVals, &latVals, &lonVals,
                             &latBndsVals, &lonBndsVals, &yBndsVals, &xBndsVals,
                             &domVals, LogInfo);
    if(LogInfo->stopRun) {
        dealloc_netCDF_domain_vars(&latVals, &lonVals, &latBndsVals, &lonBndsVals,
                                   &yBndsVals, &xBndsVals, &domVals);
        return; // Exit function prematurely due to error
    }

    for(suidNum = 0; suidNum < SW_Domain->nSUIDs; suidNum++) {
        domVals[suidNum] = suidNum + 1;
    }

    // --- Setup domain write size, number of variables to fill, fill values,
    // fill variable IDs, and lat/lonFillCount for the section
    // `Write out all values to file`
    if(domTypeIsSite) {
        // Handle lat/lon variables differently due to domain now being 1D (sites)
        latFillCount[0] = SW_Domain->nDimS;
        lonFillCount[0] = SW_Domain->nDimS;
        domCount[0] = SW_Domain->nDimS;
        domCount[1] = 0;
        numVars = 2;

        // Sites should be fill with the same information as the domain variable
        fill_netCDF_var_uint(domFileID, siteID, domVals, start, domCount, LogInfo);
        if(LogInfo->stopRun) {
            dealloc_netCDF_domain_vars(&latVals, &lonVals, &latBndsVals, &lonBndsVals,
                                       &yBndsVals, &xBndsVals, &domVals);
            return; // Exit function prematurely due to error
        }
    } else {
        domCount[1] = lonFillCount[0] = SW_Domain->nDimX;
        domCount[0] = latFillCount[0] = SW_Domain->nDimY;
        numVars = 4;

        if(fillGeo) {
            fillVals[2] = &latBndsVals;
            fillVals[3] = &lonBndsVals;
            fillVarIDs[2] = latBndsID;
            fillVarIDs[3] = lonBndsID;
        } else {
            fillVals[2] = &yBndsVals;
            fillVals[3] = &xBndsVals;
            fillVarIDs[2] = yBndsID;
            fillVarIDs[3] = xBndsID;
        }
    }

    // --- Fill horitzontal coordinate values

    // Calculate resolution for y and x and allocate value arrays
    resY = (SW_Domain->max_y - SW_Domain->min_y) / numLatVals;
    resX = (SW_Domain->max_x - SW_Domain->min_x) / numLonVals;

    // Generate/fill the horizontal variable values
    // Lon values increase from minimum to maximum
    // Lat values are reversed, i.e., [max, ..., min]
    // These values are within a bounding box given by the user
    // I.e., lat values are within [ymin, ymax] and lon are within [xmin, xmax]
    for(gridNum = 0; gridNum < numLonVals; gridNum++) {
        lonVals[gridNum] = SW_Domain->min_x + (gridNum + .5) * resX;

        if(!domTypeIsSite) {
            bndsIndex = gridNum * 2;
            if(fillGeo) {
                lonBndsVals[bndsIndex] = SW_Domain->min_x + gridNum * resX;
                lonBndsVals[bndsIndex + 1] = SW_Domain->min_x + (gridNum + 1) * resX;
            } else {
                xBndsVals[bndsIndex] = SW_Domain->min_x + gridNum * resX;
                xBndsVals[bndsIndex + 1] = SW_Domain->min_x + (gridNum + 1) * resX;
            }
        }
    }

    for(gridNum = 0; gridNum < numLatVals; gridNum++) {
        latComp = numLatVals - gridNum - 1;
        latVals[latComp] = SW_Domain->min_y + (gridNum + .5) * resY;

        if(!domTypeIsSite) {
            bndsIndex = latComp * 2;
            if(fillGeo) {
                latBndsVals[bndsIndex + 1] = SW_Domain->min_y + gridNum * resY;
                latBndsVals[bndsIndex] = SW_Domain->min_y + (gridNum + 1) * resY;
            } else {
                yBndsVals[bndsIndex + 1] = SW_Domain->min_y + gridNum * resY;
                yBndsVals[bndsIndex] = SW_Domain->min_y + (gridNum + 1) * resY;
            }
        }
    }

    // --- Write out all values to file
    for(varNum = 0; varNum < numVars; varNum++) {
        fill_netCDF_var_double(domFileID, fillVarIDs[varNum], *fillVals[varNum],
                               start, fillCounts[varNum], LogInfo);

        if(LogInfo->stopRun) {
            dealloc_netCDF_domain_vars(&latVals, &lonVals, &latBndsVals,
                                       &lonBndsVals, &yBndsVals, &xBndsVals,
                                       &domVals);
            return; // Exit function prematurely due to error
        }
    }

    // Fill domain variable with SUIDs
    fill_netCDF_var_uint(domFileID, domID, domVals, start, domCount, LogInfo);

    dealloc_netCDF_domain_vars(&latVals, &lonVals, &latBndsVals, &lonBndsVals,
                               &yBndsVals, &xBndsVals, &domVals);
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
 * @param[out] latVarID Horizontal coordinate "lat" or "y" variable identifier
 * @param[out] lonVarID Horizontal coordinate "lon" or "x" variable identifier
 * @param[out] LogInfo Holds information on warnings and errors
*/
static void fill_domain_netCDF_s(SW_DOMAIN* SW_Domain, int* domFileID,
                    int* sDimID, int* sVarID, int* latVarID, int* lonVarID,
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

        *latVarID = varIDs[1]; // lat
        *lonVarID = varIDs[2]; // lon
    } else if(!primCRSIsGeo &&
              Str_CompareI(SW_Domain->crs_bbox, proj_long_name) == 0) {

        *latVarID = varIDs[3]; // y
        *lonVarID = varIDs[4]; // x
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
 * @param[out] latDimID Horizontal coordinate "lat" or "y" dimension identifier
 * @param[out] lonDimID Horizontal coordinate "lon" or "x" dimension identifier
 * @param[out] latVarID Horizontal coordinate "lat" or "y" variable identifier
 * @param[out] lonVarID Horizontal coordinate "lon" or "x" variable identifier
 * @param[out] latBndsID Bounding variable "lat_bnds" variable identifier
 * @param[out] lonBndsID Bounding variable "lon_bnds" variable identifier
 * @param[out] yBndsID Bounding variable "y_bnds" variable identifier
 * @param[out] xBndsID Bounding variable "x_bnds" variable identifier
 * @param[out] LogInfo Holds information on warnings and errors
*/
static void fill_domain_netCDF_xy(SW_DOMAIN* SW_Domain, int* domFileID,
                    int* latDimID, int* lonDimID, int* latVarID, int* lonVarID,
                    int* latBndsID, int* lonBndsID, int* yBndsID, int* xBndsID,
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
    const char* latDimName = (primCRSIsGeo) ? "lat" : "y";
    const char* lonDimName = (primCRSIsGeo) ? "lon" : "x";

    const char *dimNames[] = {latDimName, lonDimName, "bnds"};
    const unsigned long dimVals[] = {SW_Domain->nDimY, SW_Domain->nDimX, 2};
    int *dimIDs[] = {latDimID, lonDimID, &bndsID};
    int dimIDIndex;

    int varIDs[4];
    int *varBndIDs[] = {latBndsID, lonBndsID, yBndsID, xBndsID};

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

        create_netCDF_var(varBndIDs[varNum], bndVarNames[varNum], bndVarDims,
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

        *latVarID = varIDs[0]; // lat
        *lonVarID = varIDs[1]; // lon
    } else if(!primCRSIsGeo &&
              Str_CompareI(SW_Domain->crs_bbox, proj_long_name) == 0) {

        *latVarID = varIDs[2]; // y
        *lonVarID = varIDs[3]; // x
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
    time_t t = time(NULL);

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
    strftime(creationDateStr, sizeof creationDateStr, "%FT%TZ", gmtime(&t));

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

/* =================================================== */
/*             Global Function Definitions             */
/* --------------------------------------------------- */

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
    char* impliedDomType = (dimExists("site", ncFileID)) ? "s" : "xy";
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

    char* attFailMsg = "The attribute '%s' of the variable '%s' "
                       "within the file %s does not match the one "
                       "in the domain input file. Please make sure "
                       "these match.";

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

        dimMismatch = SDimVal != SW_Domain->nDimS;
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

        dimMismatch = latDimVal != SW_Domain->nDimY ||
                      lonDimVal != SW_Domain->nDimX;
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
    int* domFileID = &SW_Domain->netCDFInfo.ncFileIDs[DOMAIN_NC];
    int sDimID = 0, latDimID = 0, lonDimID = 0;
    int domDims[2]; // Either [latDimID, lonDimID] or [sDimID, 0]
    int nDomainDims, domVarID = 0, latVarID = 0, lonVarID = 0, sVarID = 0;
    int latBndsID = 0, lonBndsID = 0, yBndsID = 0, xBndsID = 0;

    if(FileExists(DOMAIN_TEMP)) {
        LogError(LogInfo, LOGERROR, "Could not create new domain template. "
                                    "This is due to the fact that it already "
                                    "exists. Please modify it and change the name.");
        return; // Exit prematurely due to error
    }

    if(nc_create(DOMAIN_TEMP, NC_NETCDF4, domFileID) != NC_NOERR) {
        LogError(LogInfo, LOGERROR, "Could not create new domain template due "
                                    "to something internal.");
        return; // Exit prematurely due to error
    }

    if(strcmp(SW_Domain->DomainType, "s") == 0) {
        nDomainDims = 1;

        // Create s dimension/domain variables
        fill_domain_netCDF_s(SW_Domain, domFileID, &sDimID, &sVarID,
                             &latVarID, &lonVarID, LogInfo);

        if(LogInfo->stopRun) {
            nc_close(*domFileID);
            return; // Exit prematurely due to error
        }

        domDims[0] = sDimID;
        domDims[1] = 0;
    } else {
        nDomainDims = 2;

        fill_domain_netCDF_xy(SW_Domain, domFileID, &latDimID, &lonDimID,
                              &latVarID, &lonVarID, &latBndsID, &lonBndsID,
                              &yBndsID, &xBndsID, LogInfo);

        if(LogInfo->stopRun) {
            nc_close(*domFileID);
            return; // Exit prematurely due to error
        }

        domDims[0] = latDimID;
        domDims[1] = lonDimID;
    }

    // Create domain variable
    fill_domain_netCDF_domain(SW_netCDF->varNC[DOMAIN_NC], &domVarID,
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
                            latVarID, lonVarID, latBndsID, lonBndsID,
                            yBndsID, xBndsID, LogInfo);

    nc_close(*domFileID);
}

/**
 * @brief Copy domain netCDF to a new file and make it ready for a new variable
 *
 * @param[in] fileName Name of the netCDF file to create
 * @param[in] timeSize Size of "time" dimension
 * @param[in] vertSize Size of "vertical" dimension
 * @param[in] varName Name of variable to write
 * @param[in] varAttributes Attributes that the new variable will contain
 * @param[in,out] LogInfo  Holds information on warnings and errors
*/
void SW_NC_create_template(const char* fileName, unsigned long timeSize,
                           unsigned long vertSize, const char* varName,
                           char* varAttributes[], LOG_INFO* LogInfo) {
    (void) fileName;
    (void) timeSize;
    (void) vertSize;
    (void) varName;
    (void) varAttributes;
    (void) LogInfo;
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
    char* geo_long_name = SW_Domain->netCDFInfo.crs_geogsc.long_name;
    Bool geoFilled = Str_CompareI(SW_Domain->crs_bbox, geo_long_name) == 0 ||
                     (is_wgs84(SW_Domain->crs_bbox) && is_wgs84(geo_long_name));
    Bool domTypeS = Str_CompareI(SW_Domain->DomainType, "s") == 0;
    const int numDomVals = 2;
    const int numVals[] = {numDomVals};
    const int ncFileIDs[] = {SW_Domain->netCDFInfo.ncFileIDs[DOMAIN_NC]};
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
    for(file = 0; file < SW_NVARNC; file++) {
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
    static const char* possibleKeys[NUM_NC_IN_KEYS] = {"domain"};
    static const Bool requiredKeys[NUM_NC_IN_KEYS] =
            {swTRUE};
    Bool hasKeys[NUM_NC_IN_KEYS] = {swFALSE};

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
            case DOMAIN_NC:
                SW_netCDF->varNC[DOMAIN_NC] = Str_Dup(varName, LogInfo);
                SW_netCDF->InFilesNC[DOMAIN_NC] = Str_Dup(path, LogInfo);
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
 * @brief Open all netCDF files that should be open throughout the program
 *
 * @param[in,out] SW_netCDF Struct of type SW_NETCDF holding constant
 *  netCDF file information
 * @param[out] LogInfo Struct of type SW_NETCDF holding constant
 *  netCDF file information
*/
void SW_NC_open_files(SW_NETCDF* SW_netCDF, LOG_INFO* LogInfo) {
    int fileNum;

    for(fileNum = 0; fileNum < SW_NVARNC; fileNum++) {
        if(FileExists(SW_netCDF->InFilesNC[fileNum])) {
            if(nc_open(SW_netCDF->InFilesNC[fileNum], NC_NOWRITE,
                                    &SW_netCDF->ncFileIDs[fileNum]) != NC_NOERR) {

                LogError(LogInfo, LOGERROR, "An error occurred when opening %s.",
                                            SW_netCDF->InFilesNC[fileNum]);

                return; // Exit function prematurely due to error
            }
        }
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
