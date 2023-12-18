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

        // Check if the key is "long_name" or "crs_wkt"
        if(strstr(key, "long_name") != NULL || strstr(key, "crs_wkt") != NULL) {

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
            case 4: // coordinate_system is calculated
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
        return; // Exit function prematurely due to error
    }

    ncInfo->coordinate_system = (ncInfo->primary_crs_is_geographic) ?
        Str_Dup(ncInfo->crs_geogsc.long_name, LogInfo) :
        Str_Dup(ncInfo->crs_projsc.long_name, LogInfo);
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
 * @brief Fills a variable with value(s) of type unsigned integer
 *
 * @param[in] ncFileID Identifier of the open netCDF file to write the value(s) to
 * @param[in] varID Identifier to the variable within the given netCDF file
 * @param[in] values Individual or list of input variables
 * @param[in] startIndices Specification of where the C-provided netCDF
 *  should start writing values within the specified variable
 * @param[in] count How many values to write into the given variable
 * @param[in,out] LogInfo Holds information dealing with logfile output
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
 * @param[in,out] LogInfo Holds information dealing with logfile output
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
 * @brief Write a global attribute (text) to a netCDF file
 *
 * @param[in] attName Name of the attribute to create
 * @param[in] attVal Attribute string to write out
 * @param[in] varID Identifier of the variable to add the attribute to
 *  (Note: NC_GLOBAL is acceptable and is a global attribute of the netCDF file)
 * @param[in] ncFileID Identifier of the open netCDF file to write the attribute to
 * @param[in,out] LogInfo Holds information dealing with logfile output
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
 * @brief Fill horizontal coordinate variables, "domain"
 *  (or another user-specified name) variable and "sites" (if applicable)
 *  within the domain netCDF
 *
 * @param[in] SW_Domain Struct of type SW_DOMAIN holding constant
 *  temporal/spatial information for a set of simulation runs
 * @param[in] domFileID Domain netCDF file identifier
 * @param[in] domID Identifier of the "domain" (or another user-specified name) variable
 * @param[in] siteID Identifier of the "site" variable within the given netCDF
 *  (if the variable exists)
 * @param[in] latVarID Horizontal coordinate "lat" or "y" variable identifier
 * @param[in] lonVarID Horizontal coordinate "lon" or "x" variable identifier
 * @param[in,out] LogInfo Holds information dealing with logfile output
*/
void fill_domain_netCDF_vars(SW_DOMAIN* SW_Domain, int domFileID,
                                  int domID, int siteID, int latVarID,
                                  int lonVarID, LOG_INFO* LogInfo) {

    unsigned int suidNum, gridNum = 0, latValGridComp;
    unsigned int* domVals = (unsigned int*) Mem_Malloc(SW_Domain->nSUIDs *
                                                       sizeof(unsigned int),
                                                       "fill_domain_netCDF_domain_vals()",
                                                       LogInfo);
    double *latVals = NULL, *lonVals = NULL;
    size_t start[] = {0, 0}, domCount[2]; // 2 - [#lat dim, #lon dim] or [#sites, 0]
    size_t latFillCount[] = {SW_Domain->nDimY}, lonFillCount[] = {SW_Domain->nDimX};
    double resY, resX;
    unsigned int numSVals = SW_Domain->nDimS, numLonVals = SW_Domain->nDimX;
    unsigned int numLatVals = SW_Domain->nDimY;

    for(suidNum = 0; suidNum < SW_Domain->nSUIDs; suidNum++) {
        domVals[suidNum] = suidNum + 1;
    }

    if(LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

    if(strcmp(SW_Domain->DomainType, "s") == 0) {
        domCount[0] = SW_Domain->nDimS;
        domCount[1] = 0;

        // Sites should be fill with the same information as the domain variable
        fill_netCDF_var_uint(domFileID, siteID, domVals, start, domCount, LogInfo);
        if(LogInfo->stopRun) {
            free(domVals);
            return; // Exit function prematurely due to error
        }

        // Handle lat/lon variables differently due to domain now being 1D (sites)
        numLatVals = numSVals;
        numLonVals = numSVals;

        latFillCount[0] = numSVals;
        lonFillCount[0] = numSVals;
    } else {
        domCount[0] = SW_Domain->nDimY;
        domCount[1] = SW_Domain->nDimX;
    }

    // --- Write out the horitzontal coordinate values

    // Calculate resolution for y and x and allocate value arrays
    resY = (SW_Domain->max_y - SW_Domain->min_y) / numLatVals;
    resX = (SW_Domain->max_x - SW_Domain->min_x) / numLonVals;

    latVals = (double *) Mem_Malloc(SW_Domain->nDimY * sizeof(double),
                                    "fill_domain_netCDF_domain_vals()",
                                    LogInfo);
    if(LogInfo->stopRun) {
        free(domVals);
        return; // Exit function prematurely due to error
    }
    lonVals = (double *) Mem_Malloc(SW_Domain->nDimX * sizeof(double),
                                    "fill_domain_netCDF_domain_vals()",
                                    LogInfo);
    if(LogInfo->stopRun) {
        free(domVals);
        free(latVals);
        return; // Exit function prematurely due to error
    }

    // Generate/fill the horizontal variable values
    // Lon values increase from minimum to maximum
    // Lat values are reversed, i.e., [max, ..., min]
    // These values are within a bounding box given by the user
    // I.e., lat values are within [ymin, ymax] and lon are within [xmin, xmax]
    for(gridNum = 0; gridNum < numLonVals; gridNum++) {
        lonVals[gridNum] = SW_Domain->min_x + (gridNum + .5) * resX;
    }

    for(gridNum = 0; gridNum < numLatVals; gridNum++) {
        latValGridComp = numLatVals - gridNum - 1;
        latVals[latValGridComp] = SW_Domain->min_y + (gridNum + .5) * resY;
    }

    fill_netCDF_var_double(domFileID, latVarID, latVals, start, latFillCount, LogInfo);
    if(LogInfo->stopRun) {
        free(domVals);
        free(latVals);
        free(lonVals);
        return; // Exit function prematurely due to error
    }

    fill_netCDF_var_double(domFileID, lonVarID, lonVals, start, lonFillCount, LogInfo);

    free(latVals);
    free(lonVals);
    if(LogInfo->stopRun) {
        free(domVals);
        return; // Exit function prematurely due to error
    }

    // Fill domain variable with SUIDs
    fill_netCDF_var_uint(domFileID, domID, domVals, start, domCount, LogInfo);

    free(domVals);
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
 * @param[out] sVarID 'site' variable identifier
 * @param[out] latVarID Horizontal coordinate "lat" or "y" variable identifier
 * @param[out] lonVarID Horizontal coordinate "lon" or "x" variable identifier
 * @param[in,out] LogInfo Holds information dealing with logfile output
*/
static void fill_domain_netCDF_s(unsigned long nDimS, int* domFileID,
                                 Bool primCRSIsGeo, int* sDimID, int* sVarID,
                                 int* latVarID, int* lonVarID, LOG_INFO* LogInfo) {

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

    const char* varNames[] = {"site", "lat", "lon", "y", "x"};
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

    // Set lat/lon/site variable IDs to the values created above
    *sVarID = varIDs[0];

    if(primCRSIsGeo) {
        *latVarID = varIDs[1];
        *lonVarID = varIDs[2];
    } else {
        *latVarID = varIDs[3];
        *lonVarID = varIDs[4];
    }
}

/**
 * @brief Fill the domain netCDF file with variables that are for domain type "xy"
 *
 * @param[in] nDimX Size of the 'x'/'lon' dimension
 * @param[in] nDimY Size of the 'y'/'lat' dimension
 * @param[in] domFileID Domain netCDF file ID
 * @param[in] primCRSIsGeo Specifies if the primary CRS type is geographic
 * @param[out] latDimID Horizontal coordinate "lat" or "y" dimension identifier
 * @param[out] lonDimID Horizontal coordinate "lon" or "x" dimension identifier
 * @param[out] latVarID Horizontal coordinate "lat" or "y" variable identifier
 * @param[out] lonVarID Horizontal coordinate "lon" or "x" variable identifier
 * @param[in,out] LogInfo Holds information dealing with logfile output
*/
static void fill_domain_netCDF_xy(unsigned long nDimX, unsigned long nDimY,
                int* domFileID, Bool primCRSIsGeo, int* latDimID, int* lonDimID,
                int* latVarID, int* lonVarID, LOG_INFO* LogInfo) {

    int bndsID = 0;
    int bndVarDims[2]; // Used for bound variables in the netCDF file
    int dimNum, varNum, attNum;

    const int numVars = (primCRSIsGeo) ? 2 : 4; // lat/lon or lat/lon + x/y vars
    char* varNames[] = {"lat", "lon", "y", "x"};
    char* bndVarNames[] = {"lat_bnds", "lon_bnds", "y_bnds", "x_bnds"};
    char* varAttNames[][5] = {{"long_name", "standard_name", "units", "axis", "bounds"},
                              {"long_name", "standard_name", "units", "axis", "bounds"},
                              {"long_name", "standard_name", "units", "bounds"},
                              {"long_name", "standard_name", "units", "bounds"}};

    char* varAttVals[][5] = {{"latitude", "latitude", "degrees_north", "Y", "lat_bnds"},
                             {"longitude", "longitude", "degrees_east", "X", "lon_bnds"},
                             {"y coordinate of projection", "projection_y_coordinate",
                              "degrees_north", "y_bnds"},
                             {"x coordinate of projection", "projection_x_coordinate",
                              "degrees_east", "x_bnds"}};
    int numLatAtt = 5, numLonAtt = 5, numYAtt = 4, numXAtt = 4;
    int numAtts[] = {numLatAtt, numLonAtt, numYAtt, numXAtt};

    const int numDims = 3;
    char* latDimName = (primCRSIsGeo) ? "lat" : "y";
    char* lonDimName = (primCRSIsGeo) ? "lon" : "x";

    char *dimNames[] = {latDimName, lonDimName, "bnds"};
    unsigned long dimVals[] = {nDimY, nDimX, 2}, dimVal;
    int *dimIDs[] = {latDimID, lonDimID, &bndsID}, *dimID;
    int dimIDIndex, currDimID;
    char *dimName, *currVarName;
    char *currAtt, *currAttVal;

    int yVarID, xVarID, *varIDs[] = {latVarID, lonVarID, &yVarID, &xVarID};
    int varID = 0; // Set by *_bnds but is not used

    // Create dimensions
    for(dimNum = 0; dimNum < numDims; dimNum++) {
        dimName = dimNames[dimNum];
        dimVal = dimVals[dimNum];
        dimID = dimIDs[dimNum];

        create_netCDF_dim(dimName, dimVal, domFileID, dimID, LogInfo);
        if(LogInfo->stopRun) {
            return; // Exit function prematurely due to error
        }
    }

    // Create variables - lat/lon and (if the primary CRS is
    // not geographic) y/x along with the corresponding *_bnds variable
    bndVarDims[1] = bndsID; // Set the second dimension of variable to bounds

    for(varNum = 0; varNum < numVars; varNum++) {
        dimIDIndex = varNum % 2; // 2 - get lat/lon dim ids only, not bounds
        currDimID = *dimIDs[dimIDIndex];
        currVarName = varNames[varNum];

        create_netCDF_var(varIDs[varNum], currVarName, &currDimID, domFileID,
                          NC_DOUBLE, 1, LogInfo);
        if(LogInfo->stopRun) {
            return; // Exit function prematurely due to error
        }

        // Fill attributes
        for(attNum = 0; attNum < numAtts[varNum]; attNum++) {
            currAtt = varAttNames[varNum][attNum];
            currAttVal = varAttVals[varNum][attNum];
            write_str_att(currAtt, currAttVal, *varIDs[varNum], *domFileID, LogInfo);

            if(LogInfo->stopRun) {
                return; // Exit function prematurely due to error
            }
        }

        bndVarDims[0] = currDimID; // Set the first dimension of the variable
        currVarName = bndVarNames[varNum];

        create_netCDF_var(&varID, currVarName, bndVarDims, domFileID,
                          NC_DOUBLE, 2, LogInfo);
        if(LogInfo->stopRun) {
            return; // Exit function prematurely due to error
        }
    }

    // If the primary CRS is projected, update the lat/lon var IDs to x/y vars
    if(!primCRSIsGeo) {
        *latVarID = yVarID;
        *lonVarID = xVarID;
    }
}

/**
 * @brief Fill the desired netCDF with a projected CRS
 *
 * @param[in] crs_projsc Struct of type SW_CRS that holds all information
 *  regarding to the CRS type "projected"
 * @param[in] ncFileID Identifier of the open netCDF file to write all information to
 * @param[in] proj_id Projected CRS variable identifier within the netCDF we are writing to
 * @param[in] LogInfo Holds information dealing with logfile output
*/
static void fill_netCDF_with_proj_CRS_atts(SW_CRS* crs_projsc, int* ncFileID,
                                           int proj_id, LOG_INFO* LogInfo) {

    const int numStrAtts = 5, numDoubleAtts = 8;
    int strAttNum, doubleAttNum, numValsToWrite;
    char *currAttName, *currStrAttVal;
    double *currDoubleVal;
    char *strAttNames[] = {"long_name", "grid_mapping_name", "datum", "units",
                           "crs_wkt"};
    char *doubleAttNames[] = {"standard_parallel", "longitude_of_central_meridian",
                              "latitude_of_projection_origin", "false_easting",
                              "false_northing", "longitude_of_prime_meridian",
                              "semi_major_axis", "inverse_flattening"};

    char *strAttVals[] = {crs_projsc->long_name, crs_projsc->grid_mapping_name,
                          crs_projsc->datum, crs_projsc->units, crs_projsc->crs_wkt};
    double *doubleAttVals[] = {
        crs_projsc->standard_parallel, &crs_projsc->longitude_of_central_meridian,
        &crs_projsc->latitude_of_projection_origin, &crs_projsc->false_easting,
        &crs_projsc->false_northing, &crs_projsc->longitude_of_prime_meridian,
        &crs_projsc->semi_major_axis, &crs_projsc->inverse_flattening
    };

    for(strAttNum = 0; strAttNum < numStrAtts; strAttNum++) {
        currAttName = strAttNames[strAttNum];
        currStrAttVal = strAttVals[strAttNum];

        write_str_att(currAttName, currStrAttVal, proj_id, *ncFileID, LogInfo);
        if(LogInfo->stopRun) {
            return; // Exit function prematurely due to error
        }
    }

    for(doubleAttNum = 0; doubleAttNum < numDoubleAtts; doubleAttNum++) {
        currAttName = doubleAttNames[doubleAttNum];
        currDoubleVal = doubleAttVals[doubleAttNum];

        numValsToWrite = (doubleAttNum > 0) ? 1 : 2; // Index 0 has two values to write

        write_double_att(currAttName, currDoubleVal, proj_id, *ncFileID,
                         numValsToWrite, LogInfo);
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
 * @param[in] LogInfo Holds information dealing with logfile output
*/
static void fill_netCDF_with_geo_CRS_atts(SW_CRS* crs_geogsc, int* ncFileID,
                            char* coord_sys, int geo_id, LOG_INFO* LogInfo) {

    int attNum, numStrAtts = 3, numDoubleAtts = 3;
    const int numValsToWrite = 1;
    char *strAttNames[] = {"grid_mapping_name", "long_name", "crs_wkt"};
    char *doubleAttNames[] = {"longitude_of_prime_meridian", "semi_major_axis",
                              "inverse_flattening"};
    char *strAttVals[] = {crs_geogsc->grid_mapping_name, crs_geogsc->long_name,
                          crs_geogsc->crs_wkt};
    double *doubleAttVals[] = {&crs_geogsc->longitude_of_prime_meridian,
                               &crs_geogsc->semi_major_axis,
                               &crs_geogsc->inverse_flattening};
    char *currAttName, *currStrAttVal;
    double *currDoubleVal;

    if(strcmp(coord_sys, "Absent") == 0) {
        // Only write out `grid_mapping_name`
        currAttName = strAttNames[0];
        currStrAttVal = strAttVals[0];

        write_str_att(currAttName, currStrAttVal, geo_id, *ncFileID, LogInfo);
    } else {
        // Write out all attributes
        for(attNum = 0; attNum < numStrAtts; attNum++) {
            currAttName = strAttNames[attNum];
            currStrAttVal = strAttVals[attNum];

            write_str_att(currAttName, currStrAttVal, geo_id, *ncFileID, LogInfo);
            if(LogInfo->stopRun) {
                return; // Exit function prematurely due to error
            }
        }

        for(attNum = 0; attNum < numDoubleAtts; attNum++) {
            currAttName = doubleAttNames[attNum];
            currDoubleVal = doubleAttVals[attNum];

            write_double_att(currAttName, currDoubleVal, geo_id, *ncFileID,
                             numValsToWrite, LogInfo);
            if(LogInfo->stopRun) {
                return; // Exit function prematurely due to error
            }
        }
    }
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
 * @param[in] isInputFile Specifies if the file being written to is input
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
 * @param[in] ncInfo Struct of type SW_NETCDF holding constant
 *  netCDF file information
 * @param[in] domType Type of domain in which simulations are running
 *  (gridcell/sites)
 * @param[in] ncFileID Identifier of the open netCDF file to write all information to
 * @param[in] isInputFile Specifies whether the file being written to is input
 * @param[in,out] LogInfo Holds information dealing with logfile output
*/
static void fill_netCDF_with_invariants(SW_NETCDF* ncInfo, char* domType,
                                        int* ncFileID, Bool isInputFile,
                                        LOG_INFO* LogInfo) {

    int geo_id = 0, proj_id = 0;

    create_netCDF_var(&geo_id, "crs_geogsc", NULL, ncFileID, NC_BYTE, 0, LogInfo);
    if(LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

    // Geographic CRS attributes
    fill_netCDF_with_geo_CRS_atts(&ncInfo->crs_geogsc, ncFileID,
                                  ncInfo->coordinate_system, geo_id, LogInfo);
    if(LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

    // Projected CRS variable/attributes
    if(!ncInfo->primary_crs_is_geographic) {
        create_netCDF_var(&proj_id, "crs_projsc", NULL, ncFileID, NC_BYTE, 0, LogInfo);
        if(LogInfo->stopRun) {
            return; // Exit function prematurely due to error
        }

        fill_netCDF_with_proj_CRS_atts(&ncInfo->crs_projsc, ncFileID, proj_id, LogInfo);
        if(LogInfo->stopRun) {
            return; // Exit function prematurely due to error
        }
    }

    // Write global attributes
    fill_netCDF_with_global_atts(ncInfo, ncFileID, domType, "fx", isInputFile,
                                 LogInfo);
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
    int sDimID = 0, latDimID = 0, lonDimID = 0; // varID is not used
    int domDims[2]; // Either [latDimID, lonDimID] or [sDimID, 0]
    int nDomainDims, domVarID = 0, latVarID = 0, lonVarID = 0, sVarID = 0;

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
                             &sDimID, &sVarID, &latVarID, &lonVarID, LogInfo);
        if(LogInfo->stopRun) {
            nc_close(*domFileID);
            return; // Exit prematurely due to error
        }

        domDims[0] = sDimID;
        domDims[1] = 0;
    } else {
        nDomainDims = 2;

        fill_domain_netCDF_xy(SW_Domain->nDimX, SW_Domain->nDimY, domFileID,
                              SW_Domain->netCDFInfo.primary_crs_is_geographic,
                              &latDimID, &lonDimID, &latVarID, &lonVarID, LogInfo);
        if(LogInfo->stopRun) {
            nc_close(*domFileID);
            return; // Exit prematurely due to error
        }

        domDims[0] = latDimID;
        domDims[1] = lonDimID;
    }

    // Create domain variable
    fill_domain_netCDF_domain(SW_Domain->netCDFInfo.varNC[DOMAIN_NC], &domVarID,
                              domDims, *domFileID, nDomainDims,
                              SW_Domain->netCDFInfo.primary_crs_is_geographic,
                              SW_Domain->DomainType, LogInfo);
    if(LogInfo->stopRun) {
        nc_close(*domFileID);
        return; // Exit function prematurely due to error
    }

    fill_netCDF_with_invariants(&SW_Domain->netCDFInfo, SW_Domain->DomainType,
                                domFileID, swTRUE, LogInfo);
    if(LogInfo->stopRun) {
        nc_close(*domFileID);
        return; // Exit function prematurely due to error
    }

    nc_enddef(*domFileID);

    fill_domain_netCDF_vars(SW_Domain, *domFileID, domVarID, sVarID,
                            latVarID, lonVarID, LogInfo);

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
