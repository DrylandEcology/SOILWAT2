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
 * @param[in,out] ncInfo Struct of type SW_NETCDF holding constant
 *  netCDF file information
 * @param[in,out] PathInfo Struct holding all information about the programs path/files
 * @param[in,out] LogInfo Holds information dealing with logfile output
*/
static void nc_read_atts(SW_NETCDF* ncInfo, PATH_INFO* PathInfo,
                         LOG_INFO* LogInfo) {

    static const char* possibleKeys[] = {
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

        // Check if the key is a "long_name", "crs_wkt", or "coordinate_system"
        if(strstr(key, "long_name") != NULL || strstr(key, "crs_wkt") != NULL ||
           strcmp(key, "coordinate_system") == 0) {

            // Reread the like and get the entire value (includes spaces)
            sscanf(inbuf, "%34s %[^\n]", key, value);
        }

        keyID = key_to_id(key, possibleKeys, NUM_ATT_IN_KEYS);

        switch(keyID)
        {
            case 0:
                ncInfo->title = Str_Dup(value, LogInfo);
                break;
            case 1:
                ncInfo->author = Str_Dup(value, LogInfo);
                break;
            case 2:
                ncInfo->institution = Str_Dup(value, LogInfo);
                break;
            case 3:
                ncInfo->comment = Str_Dup(value, LogInfo);
                break;
            case 4:
                ncInfo->coordinate_system = Str_Dup(value, LogInfo);
                break;
            case 5:
                if(strcmp(value, "geographic") == 0) {
                    ncInfo->primary_crs_is_geographic = swTRUE;
                } else if(strcmp(value, "projected") == 0) {
                    ncInfo->primary_crs_is_geographic = swFALSE;
                } else {
                    LogError(LogInfo, LOGERROR, "The read-in primary CRS "
                             "(%s) is not a valid one. Please choose between "
                             "geographic and projected.", value);
                    return; // Exit function prematurely due to error
                }
                break;
            case 6:
                ncInfo->crs_geogsc.long_name = Str_Dup(value, LogInfo);
                break;
            case 7:
                ncInfo->crs_geogsc.grid_mapping_name = Str_Dup(value, LogInfo);
                break;
            case 8:
                ncInfo->crs_geogsc.crs_wkt = Str_Dup(value, LogInfo);
                geoCRSFound = swTRUE;
                break;
            case 9:
                ncInfo->crs_geogsc.longitude_of_prime_meridian = atof(value);
                break;
            case 10:
                ncInfo->crs_geogsc.semi_major_axis = atof(value);
                break;
            case 11:
                ncInfo->crs_geogsc.inverse_flattening = atof(value);
                break;
            case 12:
                ncInfo->crs_projsc.long_name = Str_Dup(value, LogInfo);
                break;
            case 13:
                ncInfo->crs_projsc.grid_mapping_name = Str_Dup(value, LogInfo);
                break;
            case 14:
                ncInfo->crs_projsc.crs_wkt = Str_Dup(value, LogInfo);
                projCRSFound = swTRUE;
                break;
            case 15:
                ncInfo->crs_projsc.longitude_of_prime_meridian = atof(value);
                break;
            case 16:
                ncInfo->crs_projsc.semi_major_axis = atof(value);
                break;
            case 17:
                ncInfo->crs_projsc.inverse_flattening = atof(value);
                break;
            case 18:
                ncInfo->crs_projsc.datum = Str_Dup(value, LogInfo);
                break;
            case 19:
                ncInfo->crs_projsc.units = Str_Dup(value, LogInfo);
                break;
            case 20:
                // Re-scan for 1 or 2 values of standard parallel(s)
                // the user may separate values by white-space, comma, etc.
                n = sscanf(inbuf, "%34s %f%*[^-.0123456789]%f", key, &num1, &num2);

                ncInfo->crs_projsc.standard_parallel[0] = num1;
                ncInfo->crs_projsc.standard_parallel[1] = (n == 3) ? num2 : NAN;
                break;
            case 21:
                ncInfo->crs_projsc.longitude_of_central_meridian = atof(value);
                break;
            case 22:
                ncInfo->crs_projsc.latitude_of_projection_origin = atof(value);
                break;
            case 23:
                ncInfo->crs_projsc.false_easting = atoi(value);
                break;
            case 24:
                ncInfo->crs_projsc.false_northing = atoi(value);
                break;
            case KEY_NOT_FOUND:
                LogError(LogInfo, LOGWARN, "Ignoring unknown key in %s - %s",
                                            MyFileName, key);
                break;
        }

        if(LogInfo->stopRun) {
            return; // Exist function prematurely due to error
        }
    }

    if(projCRSFound && !geoCRSFound) {
        LogError(LogInfo, LOGERROR, "Program found a projected CRS "
                                    "in %s but not a geographic CRS. "
                                    "SOILWAT2 requires either a primary CRS of "
                                    "type 'geographic' CRS or a primary CRS of "
                                    "'projected' with a geographic CRS.",
                                    PathInfo->InFiles[eNCInAtt]);
    }
}

/**
 * @brief Checks to see if a given netCDF has a specific dimention
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
 * @brief Write a global attribute (text) to a netCDF file
 *
 * @param[in] attName Name of the attribute to create
 * @param[in] attStr Attribute string to write out
 * @param[in] varID Identifier of the variable to add the attribute to
 *  (Note: NC_GLOBAL is acceptable and is a global attribute of the netCDF file)
 * @param[in] ncFileID Identifier of the open netCDF file to write the attribute to
 * @param[in,out] LogInfo Holds information dealing with logfile output
*/
static void write_str_att(const char* attName, const char* attStr,
                          int varID, int ncFileID, LOG_INFO* LogInfo) {

    if(nc_put_att_text(ncFileID, varID, attName, strlen(attStr), attStr) != NC_NOERR) {
        LogError(LogInfo, LOGERROR, "Could not create new global attribute %s",
                                    attName);
    }
}

/**
 * @brief Write a global attribute (double) to a netCDF file
 *
 * @param[in] attName Name of the attribute to create
 * @param[in] attVal Attribute value to write out
 * @param[in] varID Identifier of the variable to add the attribute to
 *  (Note: NC_GLOBAL is acceptable and is a global attribute of the netCDF file)
 * @param[in] ncFileID Identifier of the open netCDF file to write the attribute to
 * @param[in] numVals Number of values to write to the single attribute
 * @param[in,out] LogInfo Holds information dealing with logfile output
*/
static void write_double_att(const char* attName, double* attVal, int varID,
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
 * @param[in,out] LogInfo Holds information dealing with logfile output
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
 * @param[in,out] varID Variable ID within the netCDF
 * @param[in] varName Name of the new variable
 * @param[in] dimIDs Dimensions of the variable
 * @param[in] ncFileID Domain netCDF file ID
 * @param[in] varType The type in which the new variable will be
 * @param[in] numDims Number of dimensions the new variable will hold
 * @param[in,out] LogInfo Holds information dealing with logfile output
*/
static void create_netCDF_var(int* varID, const char* varName, int* dimIDs,
                int* ncFileID, int varType, int numDims, LOG_INFO* LogInfo) {

    if(nc_def_var(*ncFileID, varName, varType, numDims, dimIDs, varID) != NC_NOERR) {
        LogError(LogInfo, LOGERROR, "Could not create '%s' variable in "
                                    "netCDF.", varName);
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
 * @param[in,out] LogInfo Holds information dealing with logfile output
*/
static void fill_domain_netCDF_domain(const char* domainVarName, int* domID,
                        int domDims[], int domFileID, int nDomainDims, Bool primCRSIsGeo,
                        const char* domType, LOG_INFO* LogInfo) {

    char* gridMapVal = (primCRSIsGeo) ? "crs_geogsc" : "crs_projsc: x y crs_geogsc: lat lon";
    char* coordVal = (strcmp(domType, "s") == 0) ? "lat lon site" : "lat lon";
    char* strAttNames[] = {"long_name", "units", "grid_mapping", "coordinates"};
    char* strAttVals[] = {"simulation domain", "1", gridMapVal, coordVal};
    int attNum;
    const int numAtts = 4;
    char *currAtt, *currAttVal;

    create_netCDF_var(domID, domainVarName, domDims,
                      &domFileID, NC_UINT, nDomainDims, LogInfo);

    write_uint_att("_FillValue", NC_FILL_UINT, *domID, domFileID, LogInfo);
    if(LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

    // Write all attributes to the domain variable
    for(attNum = 0; attNum < numAtts; attNum++) {
        currAtt = strAttNames[attNum];
        currAttVal = strAttVals[attNum];
        write_str_att(currAtt, currAttVal, *domID, domFileID, LogInfo);

        if(LogInfo->stopRun) {
            return; // Exit function prematurely due to error
        }
    }
}

/**
 * @brief Fill the domain netCDF file with variables that are for domain type "s"
 *
 * @param[in] nDimS Size of the 's' dimension (number of sites)
 * @param[in] domFileID Domain netCDF file ID
 * @param[in] primCRSIsGeo Specifies if the primary CRS type is geographic
 * @param[out] sDimID 'site' dimension identifier
 * @param[out] sDimID 's' dimension ID
 * @param[out] xDimID 'x' dimension ID
 * @param[out] yDimID 'y' dimension ID
 * @param[in,out] LogInfo Holds information dealing with logfile output
*/
static void fill_domain_netCDF_s(unsigned long nDimS, int* domFileID,
                                 Bool primCRSIsGeo, int* sDimID,
                                 LOG_INFO* LogInfo) {
    const int numSiteAtt = 3, numLatAtt = 4, numLonAtt = 4;
    const int numYAtt = 3, numXAtt = 3;
    int numVarsToWrite = (primCRSIsGeo) ? 3 : 5; // Do or do not write "x" and "y"
    char* attNames[][4] = {{"long_name", "units", "cf_role"},
                           {"long_name", "standard_name", "units", "axis"},
                           {"long_name", "standard_name", "units", "axis"},
                           {"long_name", "standard_name", "units"},
                           {"long_name", "standard_name", "units"}};

    char* attVals[][4] = {{"simulation site", "1", "timeseries_id"},
                          {"latitude", "latitude", "degrees_north", "Y"},
                          {"longitude", "longitude", "degrees_east", "X"},
                          {"y coordinate of projection", "projection_y_coordinate", "degrees_north"},
                          {"x coordinate of projection", "projection_x_coordinate", "degrees_east"}};

    const char* varNames[] = {"site", "lat", "lon", "x", "y"};
    int varIDs[5]; // 5 - Maximum number of variables to create
    const int numAtts[] = {numSiteAtt, numLatAtt, numLonAtt, numYAtt, numXAtt};

    char *currAtt, *currAttVal;
    int currVarID, varNum, attNum;

    create_netCDF_dim("site", nDimS, domFileID, sDimID, LogInfo);
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

        currVarID = varIDs[varNum];

        for(attNum = 0; attNum < numAtts[varNum]; attNum++) {
            currAtt = attNames[varNum][attNum];
            currAttVal = attVals[varNum][attNum];

            write_str_att(currAtt, currAttVal, currVarID, *domFileID, LogInfo);
            if(LogInfo->stopRun) {
                return; // Exit function prematurely due to error
            }
        }
    }
}

/**
 * @brief Fill the domain netCDF file with variables that are for domain type "xy"
 *
 * @param[in] nDimX Size of the 'x' dimension
 * @param[in] nDimY Size of the 'y' dimension
 * @param[in] domFileID Domain netCDF file ID
 * @param[out] xDimID 'x' dimension ID
 * @param[out] yDimID 'y' dimension ID
 * @param[in,out] LogInfo Holds information dealing with logfile output
*/
static void fill_domain_netCDF_xy(unsigned long nDimX, unsigned long nDimY,
                int* domFileID, int* xDimID, int* yDimID, LOG_INFO* LogInfo) {

    int bndsID = 0;
    int bndVarDims[2]; // Used for x and y bound variables
    int varID = 0;

    // Create x, y, and bnds dimension
    create_netCDF_dim("x", nDimX, domFileID, xDimID, LogInfo);
    if(LogInfo->stopRun) {
        return; // Exit prematurely due to error
    }

    create_netCDF_dim("y", nDimY, domFileID, yDimID, LogInfo);
    if(LogInfo->stopRun) {
        return; // Exit prematurely due to error
    }

    create_netCDF_dim("bnds", 2, domFileID, &bndsID, LogInfo);
    if(LogInfo->stopRun) {
        return; // Exit prematurely due to error
    }

    // Create x, y, x_bnds, and y_bnds variables
    create_netCDF_var(&varID, "x", xDimID, domFileID, NC_DOUBLE, 1, LogInfo);
    if(LogInfo->stopRun) {
        return; // Exit prematurely due to error
    }

    create_netCDF_var(&varID, "y", yDimID, domFileID, NC_DOUBLE, 1, LogInfo);
    if(LogInfo->stopRun) {
        return; // Exit prematurely due to error
    }

    bndVarDims[0] = *xDimID;
    bndVarDims[1] = bndsID;

    create_netCDF_var(&varID, "x_bnds", bndVarDims, domFileID, NC_DOUBLE, 2, LogInfo);
    if(LogInfo->stopRun) {
        return; // Exit prematurely due to error
    }

    bndVarDims[0] = *yDimID;

    create_netCDF_var(&varID, "y_bnds", bndVarDims, domFileID, NC_DOUBLE, 2, LogInfo);
}

/**
 * @brief Fill the desired netCDF with a projected CRS
 *
 * @param[in] ncInfo Struct of type SW_NETCDF holding constant
 *  netCDF file information
 * @param[in] ncFileID Identifier of the open netCDF file to write all information to
 * @param[in] proj_id Projected CRS variable identifier within the netCDF we are writing to
 * @param[in] LogInfo Holds information dealing with logfile output
*/
static void fill_netCDF_with_proj_CRS_atts(SW_NETCDF* ncInfo, int* ncFileID,
                                           int proj_id, LOG_INFO* LogInfo) {

    write_str_att("long_name", ncInfo->crs_projsc.long_name,
                  proj_id, *ncFileID, LogInfo);
    if(LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }
    write_str_att("grid_mapping_name", ncInfo->crs_projsc.grid_mapping_name,
                proj_id, *ncFileID, LogInfo);
    if(LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }
    write_str_att("datum", ncInfo->crs_projsc.datum,
                proj_id, *ncFileID, LogInfo);
    if(LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }
    write_str_att("units", ncInfo->crs_projsc.units,
                proj_id, *ncFileID, LogInfo);
    if(LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }
    write_str_att("crs_wkt", ncInfo->crs_projsc.crs_wkt,
                proj_id, *ncFileID, LogInfo);
    if(LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }
    write_double_att("standard_parallel", ncInfo->crs_projsc.standard_parallel,
                    proj_id, *ncFileID, 2, LogInfo);
    if(LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }
    write_double_att("longitude_of_central_meridian",
                        &ncInfo->crs_projsc.longitude_of_central_meridian,
                        proj_id, *ncFileID, 1, LogInfo);
    if(LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }
    write_double_att("latitude_of_projection_origin",
                        &ncInfo->crs_projsc.latitude_of_projection_origin,
                        proj_id, *ncFileID, 1, LogInfo);
    if(LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }
    write_double_att("false_easting",
                        &ncInfo->crs_projsc.false_easting,
                        proj_id, *ncFileID, 1, LogInfo);
    if(LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }
    write_double_att("false_northing",
                        &ncInfo->crs_projsc.false_northing,
                        proj_id, *ncFileID, 1, LogInfo);
    if(LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }
    write_double_att("longitude_of_prime_meridian",
                    &ncInfo->crs_projsc.longitude_of_prime_meridian,
                    proj_id, *ncFileID, 1, LogInfo);
    if(LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }
    write_double_att("semi_major_axis", &ncInfo->crs_projsc.semi_major_axis,
                    proj_id, *ncFileID, 1, LogInfo);
    if(LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }
    write_double_att("inverse_flattening", &ncInfo->crs_projsc.inverse_flattening,
                    proj_id, *ncFileID, 1, LogInfo);
}

/**
 * @brief Fill the desired netCDF with a projected CRS
 *
 * @param[in] ncInfo Struct of type SW_NETCDF holding constant
 *  netCDF file information
 * @param[in] ncFileID Identifier of the open netCDF file to write all information to
 * @param[in] geo_id Projected CRS variable identifier within the netCDF we are writing to
 * @param[in] LogInfo Holds information dealing with logfile output
*/
static void fill_netCDF_with_geo_CRS_atts(SW_NETCDF* ncInfo, int* ncFileID,
                                          int geo_id, LOG_INFO* LogInfo) {

    if(strcmp(ncInfo->coordinate_system, "Absent") != 0) {
        write_str_att("grid_mapping_name", ncInfo->crs_geogsc.grid_mapping_name,
                      geo_id, *ncFileID, LogInfo);
        if(LogInfo->stopRun) {
            return; // Exit function prematurely due to error
        }
    }

    write_str_att("long_name", ncInfo->crs_geogsc.long_name,
                geo_id, *ncFileID, LogInfo);
    if(LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }
    write_str_att("crs_wkt", ncInfo->crs_geogsc.crs_wkt,
                geo_id, *ncFileID, LogInfo);
    if(LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }
    write_double_att("longitude_of_prime_meridian",
                    &ncInfo->crs_geogsc.longitude_of_prime_meridian,
                    geo_id, *ncFileID, 1, LogInfo);
    if(LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }
    write_double_att("semi_major_axis", &ncInfo->crs_geogsc.semi_major_axis,
                    geo_id, *ncFileID, 1, LogInfo);
    if(LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }
    write_double_att("inverse_flattening", &ncInfo->crs_geogsc.inverse_flattening,
                    geo_id, *ncFileID, 1, LogInfo);
}

/**
 * @brief Fill the given netCDF with global attributes
 *
 * @param[in] ncInfo Struct of type SW_NETCDF holding constant
 *  netCDF file information
 * @param[in] ncFileID Identifier of the open netCDF file to write all information to
 * @param[in] domType Type of domain in which simulations are running
 *  (gridcell/sites)
 * @param[in] freqAtt Value of a global attribute "frequency" (may be "fx",
 *  "day", "week", "month", or "year")
 * @param[in] LogInfo Holds information dealing with logfile output
*/
static void fill_netCDF_with_global_atts(SW_NETCDF* ncInfo, int* ncFileID,
                                         const char* domType, char* freqAtt,
                                         Bool isInputFile, LOG_INFO* LogInfo) {

    char sourceStr[40]; // 40 - valid size of the SOILWAT2 global `SW2_VERSION` + "SOILWAT2"
    char creationDateStr[21]; // 21 - valid size to hold a string of format YYYY-MM-DDTHH:MM:SSZ
    char featureTypeStr[11]; // 11 - valid size to hold "timeSeries" or "point" (may not be used)
    time_t t = time(NULL);

    const int numGlobAtts = (strcmp(domType, "s") == 0) ? 14 : 13; // Do or do not include "featureType"
    char* productStr = (isInputFile) ? "model-input" : "model-output";
    char* attNames[] = {"title", "author", "institution", "comment",
                        "coordinate_system", "Conventions", "source",
                        "source_id", "further_info_url", "creation_date",
                        "history", "product", "frequency", "featureType"};

    char* attVals[] = {ncInfo->title, ncInfo->author, ncInfo->institution,
                       ncInfo->comment, ncInfo->coordinate_system, "CF-1.10",
                       sourceStr, "SOILWAT2", "https://github.com/DrylandEcology/SOILWAT2",
                       creationDateStr, "No revisions.", productStr, freqAtt,
                       featureTypeStr};
    int attNum;
    char *currAtt, *currAttVal;

    // Fill `sourceStr` and `creationDateStr`
    snprintf(sourceStr, 40, "SOILWAT2%s", SW2_VERSION);
    strftime(creationDateStr, sizeof creationDateStr, "%FT%TZ", gmtime(&t));

    // Determine if the netCDF variable "featureType" is to be written out
    // If so, copy a value to the correct spot in `attNames` (last index)
    if(strcmp(domType, "s") == 0) {
        if(dimExists("time", *ncFileID)) {
            strcpy(attVals[numGlobAtts - 1], "timeSeries");
        } else {
            strcpy(attVals[numGlobAtts - 1], "point");
        }
    }

    // Write out the necessary global attributes that are listed above
    for(attNum = 0; attNum < numGlobAtts; attNum++) {
        currAtt = attNames[attNum];
        currAttVal = attVals[attNum];

        write_str_att(currAtt, currAttVal, NC_GLOBAL, *ncFileID, LogInfo);
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
 * @param[in] SW_Domain Struct of type SW_DOMAIN holding constant
 *  temporal/spatial information for a set of simulation runs
 * @param[in] ncFileID Identifier of the open netCDF file to write all information to
 * @param[in] isInputFile Specifies whether the file being written to is input
 * @param[in,out] LogInfo Holds information dealing with logfile output
*/
static void fill_netCDF_with_invariants(SW_DOMAIN* SW_Domain, int* ncFileID,
                                        Bool isInputFile, LOG_INFO* LogInfo) {

    SW_NETCDF* ncInfo = &SW_Domain->netCDFInfo;
    int geo_id = 0, proj_id = 0;

    create_netCDF_var(&geo_id, "crs_geogsc", NULL, ncFileID, NC_BYTE, 0, LogInfo);
    if(LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

    // Geographic CRS attributes
    fill_netCDF_with_geo_CRS_atts(ncInfo, ncFileID, geo_id, LogInfo);
    if(LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

    // Projected CRS variable/attributes
    if(!ncInfo->primary_crs_is_geographic) {
        create_netCDF_var(&proj_id, "crs_projsc", NULL, ncFileID, NC_BYTE, 0, LogInfo);
        if(LogInfo->stopRun) {
            return; // Exit function prematurely due to error
        }

        fill_netCDF_with_proj_CRS_atts(ncInfo, ncFileID, proj_id, LogInfo);
        if(LogInfo->stopRun) {
            return; // Exit function prematurely due to error
        }
    }

    // Write global attributes
    fill_netCDF_with_global_atts(ncInfo, ncFileID, SW_Domain->DomainType,
                                 "fx", isInputFile, LogInfo);
}

/* =================================================== */
/*             Global Function Definitions             */
/* --------------------------------------------------- */

/**
 * @brief Read domain netCDF, obtain CRS-related information,
 *  and check that content is consistent with "domain.in"
 *
 * @param[out] SW_Domain Struct of type SW_DOMAIN holding constant
 *  temporal/spatial information for a set of simulation runs
 * @param[in] domFileName Name of the domain netCDF file
 * @param[in,out] LogInfo Holds information dealing with logfile output
*/
void SW_NC_read_domain(SW_DOMAIN* SW_Domain, const char* domFileName,
                       LOG_INFO* LogInfo) {

    (void) SW_Domain;
    (void) domFileName;
    (void) LogInfo;
}

/**
 * @brief Check that content is consistent with domain
 *
 * @param[in] SW_Domain Struct of type SW_DOMAIN holding constant
 *  temporal/spatial information for a set of simulation runs
 * @param[in] fileName Name of netCDF file to test
 * @param[in,out] LogInfo Holds information dealing with logfile output
*/
void SW_NC_check(SW_DOMAIN* SW_Domain, const char* fileName,
                 LOG_INFO* LogInfo) {

    (void) SW_Domain;
    (void) fileName;
    (void) LogInfo;
}

/**
 * @brief Create domain netCDF template if it does not already exists
 *
 * @param[in] SW_Domain Struct of type SW_DOMAIN holding constant
 *  temporal/spatial information for a set of simulation runs
 * @param[in,out] LogInfo Holds information dealing with logfile output
*/
void SW_NC_create_domain_template(SW_DOMAIN* SW_Domain, LOG_INFO* LogInfo) {

    int* domFileID = &SW_Domain->netCDFInfo.ncFileIDs[DOMAIN_NC];
    int sDimID = 0, xDimID = 0, yDimID = 0; // varID is not used
    int domDims[2]; // Either [yDimID, xDimID] or [sDimID, 0]
    int nDomainDims;
    int varID; // Not used

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
        fill_domain_netCDF_s(SW_Domain->nDimS, domFileID,
                             SW_Domain->netCDFInfo.primary_crs_is_geographic,
                             &sDimID, LogInfo);
        if(LogInfo->stopRun) {
            nc_close(*domFileID);
            return; // Exit prematurely due to error
        }

        domDims[0] = sDimID;
        domDims[1] = 0;
    } else {
        nDomainDims = 2;

        fill_domain_netCDF_xy(SW_Domain->nDimX, SW_Domain->nDimY, domFileID,
                              &xDimID, &yDimID, LogInfo);
        if(LogInfo->stopRun) {
            nc_close(*domFileID);
            return; // Exit prematurely due to error
        }

        domDims[0] = yDimID;
        domDims[1] = xDimID;
    }

    // Create domain variable
    fill_domain_netCDF_domain(SW_Domain->netCDFInfo.varNC[DOMAIN_NC], &domVarID,
                              domDims, *domFileID, nDomainDims,
                              SW_Domain->netCDFInfo.primary_crs_is_geographic,
                              SW_Domain->DomainType, LogInfo);
    if(LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

    fill_netCDF_with_invariants(SW_Domain, domFileID, swTRUE, LogInfo);
    if(LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

    nc_enddef(*domFileID);
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
 * @param[in,out] LogInfo  Holds information dealing with logfile output
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
 * @param[in,out] LogInfo Holds information dealing with logfile output
*/
void SW_NC_read_inputs(SW_ALL* sw, SW_DOMAIN* SW_Domain, unsigned long ncSUID,
                       LOG_INFO* LogInfo) {

    (void) sw;
    (void) SW_Domain;
    (void) ncSUID;
    (void) LogInfo;
}

/**
 * @brief Check that all available netCDF input files are consistent with domain
 *
 * @param[in] SW_Domain Struct of type SW_DOMAIN holding constant
 *  temporal/spatial information for a set of simulation runs
 * @param[in,out] LogInfo Holds information dealing with logfile output
*/
void SW_NC_check_input_files(SW_DOMAIN* SW_Domain, LOG_INFO* LogInfo) {
    int file;

    for(file = 0; file < SW_NVARNC; file++) {
        SW_NC_check(SW_Domain, SW_Domain->netCDFInfo.InFilesNC[file], LogInfo);
        if(LogInfo->stopRun) {
            return; // Exit function prematurely due to inconsistent data
        }
    }

}

/**
 * @brief Read input files for netCDF related actions
 *
 * @param[in,out] ncInfo Struct of type SW_NETCDF holding constant
 *  netCDF file information
 * @param[in,out] PathInfo Struct holding all information about the programs path/files
 * @param[in] LogInfo Holds information dealing with logfile output
*/
void SW_NC_read(SW_NETCDF* ncInfo, PATH_INFO* PathInfo, LOG_INFO* LogInfo) {
    static const char* possibleKeys[] = {"domain"};

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
        switch(keyID) {
            case DOMAIN_NC:
                ncInfo->varNC[DOMAIN_NC] = Str_Dup(varName, LogInfo);
                ncInfo->InFilesNC[DOMAIN_NC] = Str_Dup(path, LogInfo);
                break;
            default:
                LogError(LogInfo, LOGWARN, "Ignoring unknown key in %s, %s",
                         MyFileName, key);
                break;
        }
    }

    nc_read_atts(ncInfo, PathInfo, LogInfo);
}
