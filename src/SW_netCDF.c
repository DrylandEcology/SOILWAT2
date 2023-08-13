#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netcdf.h>
#include <time.h>

#include "include/SW_netCDF.h"
#include "include/SW_Files.h"
#include "include/myMemory.h"
#include "include/filefuncs.h"
#include "include/SW_Domain.h"

/* =================================================== */
/*             Private Function Declarations           */
/* --------------------------------------------------- */
static int nc_key_to_ID(char* key);
static void create_dim_xy(int nDimX, int nDimY, int* yDimID, int* xDimID,
                          int* bndsDimID, LOG_INFO* LogInfo);
static void create_dim_s(int nDimS, int* sDimID, LOG_INFO* LogInfo);
static void create_xy_vars(int nDimX, int nDimY, int yDimID, int xDimID,
                           int bndsDimID, LOG_INFO* LogInfo);
static void create_s_vars(int nDimS, int sDimID, LOG_INFO* LogInfo);
static void create_var_x(int* xID, int xDim[], LOG_INFO* LogInfo);
static void create_var_y(int* yID, int yDim[], LOG_INFO* LogInfo);
static void create_var_crs(LOG_INFO* LogInfo);
static void fill_xy_vars(int nDimX, int nDimY, int domainID,
                         int xID, int yID, int x_bndsID, int y_bndsID);
static void fill_s_vars(int nDimS, int siteID, int domainID, int xID, int yID);
static void write_global_atts(LOG_INFO* LogInfo);
static void create_dimension(char* dimName, int* dimID, int size, LOG_INFO* LogInfo);
static void create_variable(char* varName, int numDims, int dims[], int varType,
                            int* varID, LOG_INFO* LogInfo);
static void write_att_str(char *attName, char *attValue, int varID, LOG_INFO* LogInfo);
static void write_att_val(char* attName, double attValue[], int attSize,
                          int varID, LOG_INFO* LogInfo);

/* =================================================== */
/*                   Local Defines                     */
/* --------------------------------------------------- */
// Constant sizes
#define NUMBNDS 2

#define SIZEONE 1
#define SIZETWO 2

// Constants for number of dimensions
#define ZERODIMS 0
#define ONEDIM 1
#define TWODIMS 2
#define THREEDIMS 3

/* =================================================== */
/*             Global Function Definitions             */
/* --------------------------------------------------- */
/**
 * @brief Read input files for netCDF related actions
 *
 * @param[in] PathInfo Struct holding all information about the programs path/files
 * @param[in] LogInfo Holds information dealing with logfile output
*/
void SW_NC_read_files(PATH_INFO* PathInfo, LOG_INFO* LogInfo) {

    FILE *f;
    char inbuf[MAX_FILENAMESIZE], *MyFileName;
    char key[15], value[MAX_FILENAMESIZE]; // 15 - Max key size
    int keyID;

    MyFileName = PathInfo->InFiles[eNCIn];
	f = OpenFile(MyFileName, "r", LogInfo);

    // Get domain file name
    while(GetALine(f, inbuf)) {
        sscanf(inbuf, "%s %s", key, value);

        keyID = nc_key_to_ID(key);
        switch(keyID) {
            case DOMAIN_NC:
                PathInfo->InFiles_nc[DOMAIN_NC] = Str_Dup(value, LogInfo);
                break;
            default:
                LogError(LogInfo, LOGFATAL, "Ignoring unknown key in %s, %s",
                         MyFileName, key);
                break;
        }
    }
}

/**
 * @brief Creates a new domain netCDF if one was not provided
 *
 * @param[in] SW_Domain Struct of type SW_DOMAIN holding constant
 *  temporal/spatial information for a set of simulation runs
 * @param[in] DomainName Domain netCDF name
 * @param[in] LogInfo Holds information dealing with logfile output
*/
void SW_NC_create_domain(SW_DOMAIN* SW_Domain, char* DomainName,
                         LOG_INFO* LogInfo) {

    int equalString = 0;
    int yDimID, xDimID, bndsDimID, siteDimID;

    nc_create(DomainName, NC_NETCDF4, NULL); // Don't store file ID (OPEN_NC_ID)

    create_var_crs(LogInfo);
    write_global_atts(LogInfo);

    if(strcmp(SW_Domain->DomainType, "xy") == equalString) {
        create_dim_xy(SW_Domain->nDimX, SW_Domain->nDimY, &yDimID,
                      &xDimID, &bndsDimID, LogInfo);
        create_xy_vars(SW_Domain->nDimX, SW_Domain->nDimY, yDimID,
                       xDimID, bndsDimID, LogInfo);
    } else {
        create_dim_s(SW_Domain->nDimS, &siteDimID, LogInfo);
        create_s_vars(SW_Domain->nDimS, siteDimID, LogInfo);
    }

    nc_close(OPEN_NC_ID);
}

/* =================================================== */
/*             Local Function Definitions              */
/* --------------------------------------------------- */

/**
 * @brief Helper function to `SW_NC_read_files()` to determine which input
 *        key has been read in
 * @param[in] key Read-in key from domain input files file
*/
static int nc_key_to_ID(char* key) {
    static const char *possibleKeys[] = {"Domain"};
    int keyIndex, result, sameString = 0;

    for(keyIndex = 0; keyIndex < SW_NFILESNC; keyIndex++) {
        result = strcmp(key, possibleKeys[keyIndex]);

        if(result == sameString) {
            return keyIndex;
        }
    }

    return KEY_NOT_FOUND;
}

/**
 * @brief Creates dimensions "x", "y", and "bnds" for the domain type "xy"
 *
 * @param[in] nDimX Size of dimension "x"
 * @param[in] nDimY Size of dimension "y"
 * @param[in] yDimID Dimension ID of "y" within the domain
 * @param[in] xDimID Dimension ID of "x" within the domain
 * @param[in] bndsDimID Dimension ID of "bnds" within the domain
 * @param[in] LogInfo Holds information dealing with logfile output
*/
static void create_dim_xy(int nDimX, int nDimY, int* yDimID, int* xDimID,
                   int* bndsDimID, LOG_INFO* LogInfo) {

    // Create dimensions "x", "y", and "bnds"
    create_dimension("x", xDimID, nDimX, LogInfo);
    create_dimension("y", yDimID, nDimY, LogInfo);
    create_dimension("bnds", bndsDimID, NUMBNDS, LogInfo);
}

/**
 * @brief Creates dimension "site" for the domain type "s" (sites)
 *
 * @param[in] nDimS Size of dimension "site"
 * @param[in] sDimID Dimension ID of "site" within the domain
 *
 * @param[in] LogInfo Holds information dealing with logfile output
*/
static void create_dim_s(int nDimS, int* sDimID, LOG_INFO* LogInfo) {

    // Create dimension "site"
    create_dimension("site", sDimID, nDimS, LogInfo);
}

/**
 * @brief Creates/files all variables related to domain "xy"
 *  in the new domain netCDF
 *
 * @param[in] nDimX Size of dimension "x"
 * @param[in] nDimY Size of dimension "y"
 * @param[in] yDimID Dimension ID of "y" with the domain
 * @param[in] xDimID Dimension ID of "x" with the domain
 * @param[in] bndsDimID Dimension ID of "bnds" with the domain
 * @param[in] LogInfo Holds information dealing with logfile output
*/
static void create_xy_vars(int nDimX, int nDimY, int yDimID, int xDimID,
                    int bndsDimID, LOG_INFO* LogInfo) {

    int xDim[] = {xDimID}, yDim[] = {yDimID}, xBndsDim[] = {xDimID, bndsDimID},
        yBndsDim[] = {yDimID, bndsDimID}, domainDims[] = {yDimID, xDimID};
    int xID, yID, xBndsID, yBndsID, domID;
    size_t numChunkVals = 2, numFillSize = 1;
    unsigned int xBndsAttVals[] = {nDimX, NUMBNDS},
        yBndsAttVals[] = {nDimY, NUMBNDS}, ChunkVals[] = {nDimY, nDimX};
    float fillVal[] = {NC_FILL_FLOAT};

    // "domain" variable
    create_variable("domain", TWODIMS, domainDims, NC_FLOAT, &domID, LogInfo);

    write_att_str("long_name", "Domain simulation unit identifier (suid)",
                  domID, LogInfo);
    write_att_str("grid_mapping", "crs: x y", domID, LogInfo);
    write_att_str("units", "1", domID, LogInfo);
    nc_put_att_float(OPEN_NC_ID, domID, "_FillValue", NC_FLOAT,
                     numFillSize, fillVal);
    nc_put_att_uint(OPEN_NC_ID, domID, "_ChunkSizes", NC_UINT,
                     numChunkVals, ChunkVals);

    // "x" variable
    create_var_x(&xID, xDim, LogInfo);
    write_att_str("axis", "X", xID, LogInfo);
    write_att_str("bounds", "x_bnds", xID, LogInfo);

    // "x_bnds variable"
    create_variable("x_bnds", TWODIMS, xBndsDim, NC_DOUBLE,
                    &xBndsID, LogInfo);
    nc_put_att_uint(OPEN_NC_ID, xBndsID, "_ChunkSizes", NC_UINT,
                    numChunkVals, xBndsAttVals);

    // "y" variable
    create_var_y(&yID, yDim, LogInfo);
    write_att_str("axis", "Y", yID, LogInfo);
    write_att_str("bounds", "y_bnds", yID, LogInfo);

    // "y_bnds" variable
    create_variable("y_bnds", TWODIMS, yBndsDim, NC_DOUBLE,
                    &yBndsID, LogInfo);
    nc_put_att_uint(OPEN_NC_ID, yBndsID, "_ChunkSizes", NC_UINT,
                    numChunkVals, yBndsAttVals);

    nc_enddef(OPEN_NC_ID); // End of defining variables, dimensions, etc.

    // Fill values
    fill_xy_vars(nDimX, nDimY, domID, xID, yID, xBndsID, yBndsID);
}

/**
 * @brief Creates all variables related to domain "s" in the new domain netCDF
 *
 * @param[in] nDimS Size of dimension "s"
 * @param[in] sDimID Dimension ID of "site" within the domain
 * @param[in] LogInfo Holds information dealing with logfile output
*/
static void create_s_vars(int nDimS, int sDimID, LOG_INFO* LogInfo) {
    int sID, yID, xID, domainID;
    int dims[] = {sDimID};
    size_t numChunkVals = 1, numFillSize = 1;
    float fillValFloat[] = {NC_FILL_FLOAT};
    double fillValDouble[] = {NC_FILL_DOUBLE};
    unsigned int ChunkVals[] = {nDimS};

    // "domain" variable
    create_variable("domain", ONEDIM, dims, NC_FLOAT, &domainID, LogInfo);

    write_att_str("long_name", "Domain simulation unit identifier (suid)",
                  domainID, LogInfo);
    write_att_str("grid_mapping", "crs: x y", domainID, LogInfo);
    write_att_str("coordinates", "y x", domainID, LogInfo);
    nc_put_att_float(OPEN_NC_ID, domainID, "_FillValue", NC_FLOAT,
                     numFillSize, fillValFloat);
    write_att_str("units", "1", domainID, LogInfo);
    nc_put_att_uint(OPEN_NC_ID, domainID, "_ChunkSizes", NC_UINT,
                     numChunkVals, ChunkVals);

    // "site" variable
    create_variable("site", ONEDIM, dims, NC_INT, &sID, LogInfo);
    write_att_str("units", "1", sID, LogInfo);
    write_att_str("long_name", "SOILWAT2 simulation sites", sID, LogInfo);

    // "x" variable
    create_var_x(&xID, dims, LogInfo);
    nc_put_att_uint(OPEN_NC_ID, xID, "_ChunkSize", NC_UINT, numChunkVals,
                    ChunkVals);
    write_att_val("_FillValue", fillValDouble, numFillSize, xID, LogInfo);

    // "y" variable
    create_var_y(&yID, dims, LogInfo);
    nc_put_att_uint(OPEN_NC_ID, yID, "_ChunkSize", NC_UINT, numChunkVals,
                    ChunkVals);
    write_att_val("_FillValue", fillValDouble, numFillSize, yID, LogInfo);

    nc_enddef(OPEN_NC_ID); // End of defining variables, dimensions, etc.

    fill_s_vars(nDimS, sID, domainID, xID, yID);
}

/**
 * @brief Create an "x" variable with common attributes between the domains
 *  "xy" and "site"
 *
 * @param[in] xID ID of the x variable within the domain netCDF
 * @param[in] xDim Dimensions of the x variable
 * @param[in] LogInfo Holds information dealing with logfile output
*/
static void create_var_x(int* xID, int xDim[], LOG_INFO* LogInfo) {

    create_variable("x", ONEDIM, xDim, NC_DOUBLE, xID, LogInfo);

    write_att_str("units", "m", *xID, LogInfo);
    write_att_str("long_name", "x coordinate of projection", *xID, LogInfo);
    write_att_str("standard_name", "projection_x_coordinate", *xID, LogInfo);
}

/**
 * @brief Create a "y" variable with common attributes between the domains
 *  "xy" and "site"
 *
 * @param[in] yID ID of the y variable within the domain netCDF
 * @param[in] yDim Dimensions of the y variable
 * @param[in] LogInfo Holds information dealing with logfile output
*/
static void create_var_y(int *yID, int yDim[], LOG_INFO* LogInfo) {

    create_variable("y", ONEDIM, yDim, NC_DOUBLE, yID, LogInfo);

    write_att_str("units", "m", *yID, LogInfo);
    write_att_str("long_name", "y coordinate of projection", *yID, LogInfo);
    write_att_str("standard_name", "projection_y_coordinate", *yID, LogInfo);
}

/**
 * @brief Create the "crs" variable in a netCDF
 *
 * @param[in] LogInfo Holds information dealing with logfile output
*/
static void create_var_crs(LOG_INFO* LogInfo) {
    int crsID;
    double stdParallel[] = {29.5, 45.5}, longCentralMerid[] = {-96.0},
        latProjOrigin[] = {23.0}, false_easting[] = {0.0},
        false_northing[] = {0.0}, longPrimeMerid[] = {0.0},
        semiMajorAxis[] = {6378137.0}, inverse_flattening[] = {298.257222101};

    // Constant attribute strings
    static char *crs_wktVal = "PROJCS[\"NAD83(2011) / Conus Albers\","\
        "GEOGCS[\"NAD83(2011)\",DATUM[\"NAD83_National_Spatial_Reference"\
        "_System_2011\",SPHEROID[\"GRS 1980\",6378137,298.257222101]],PRIMEM"\
        "[\"Greenwich\",0],UNIT[\"degree\",0.0174532925199433,AUTHORITY"\
        "[\"EPSG\",\"9122\"]],AUTHORITY[\"EPSG\",\"6318\"]],PROJECTION"\
        "[\"Albers_Conic_Equal_Area\"],PARAMETER[\"latitude_of_center\",23],"\
        "PARAMETER[\"longitude_of_center\",-96],PARAMETER[\"standard_parallel"\
        "_1\",29.5],PARAMETER[\"standard_parallel_2\",45.5],PARAMETER[\""\
        "false_easting\",0],PARAMETER[\"false_northing\",0],UNIT[\"metre\",1],"\
        "AXIS[\"Easting\",EAST],AXIS[\"Northing\",NORTH],AUTHORITY[\"EPSG\","\
        "\"6350\"]]";

    // Create "crs" and write attributes
    create_variable("crs", ZERODIMS, NULL, NC_INT, &crsID, LogInfo);

    write_att_str("grid_mapping_name", "albers_conical_equal_area",
                  crsID, LogInfo);

    write_att_val("standard_parallel", stdParallel, SIZETWO, crsID, LogInfo);
    write_att_val("longitude_of_central_meridian", longCentralMerid, SIZEONE,
                  crsID, LogInfo);
    write_att_val("latitude_of_projection_origin", latProjOrigin, SIZEONE,
                  crsID, LogInfo);
    write_att_val("false_easting", false_easting, SIZEONE, crsID, LogInfo);
    write_att_val("false_northing", false_northing, SIZEONE, crsID, LogInfo);
    write_att_val("longitude_of_prime_meridian", longPrimeMerid, SIZEONE,
                  crsID, LogInfo);
    write_att_val("semi_major_axis", semiMajorAxis, SIZEONE, crsID, LogInfo);
    write_att_val("inverse_flattening", inverse_flattening, SIZEONE,
                  crsID, LogInfo);

    write_att_str("crs_wkt", crs_wktVal, crsID, LogInfo);
}

/**
 * @brief Fill variables within domain "xy" with values
 *
 * @param[in] nDimX Size of dimension "x"
 * @param[in] nDimY Size of dimension "y"
 * @param[in] domainID ID of the domain variable within the domain netCDF
 * @param[in] xID ID of the x variable within the domain netCDF
 * @param[in] yID ID of the y variable within the domain netCDF
 * @param[in] x_bndsID ID of the x_bnds variable within the domain netCDF
 * @param[in] y_bndsID ID of the y_bnds variable within the domain netCDF
*/
static void fill_xy_vars(int nDimX, int nDimY, int domainID,
        int xID, int yID, int x_bndsID, int y_bndsID) {

    double setValsX[nDimX], setValsY[nDimY];
    double setValsXBnds[nDimX][NUMBNDS], setValsYBnds[nDimY][NUMBNDS];
    double Xval = 2000.0, xBndsVal = 0.0, xIncrement = 4000.0;
    double Yval = 2503826.1, yBndsVal = 2501326.1, yIncrement = 5000.0;
    float DomVal = 1.0, DomIncrement = 1.0, setValsDom[nDimY][nDimX];
    int indexX, indexY, indexBnds;

    // NOTE: startXYDom works for both 1D and 2D variables, it seems
    // the second element of the array is ignored when a 1D variable
    // is being written to
    size_t startXYDom[] = {0, 0}, Xcount[] = {nDimX}, Ycount[] = {nDimY},
        DomCount[2] = {nDimY, nDimX};
    size_t xBndsCount[] = {nDimX, NUMBNDS}, yBndsCount[] = {nDimY, NUMBNDS};

    // Fill "x" and "x_bnds"
    // Because "x_bnds" is a 2D variable, use a nested loop to fill the columns
    // "xBndsVal" is only incremented if it is not the last column
    for(indexX = 0; indexX < nDimX; indexX++) {
        setValsX[indexX] = Xval;
        Xval += xIncrement;

        for(indexBnds = 0; indexBnds < NUMBNDS; indexBnds++) {
            setValsXBnds[indexX][indexBnds] = xBndsVal;

            if(indexBnds == 0) {
                xBndsVal += xIncrement;
            }
        }
    }

    nc_put_vara_double(OPEN_NC_ID, xID, startXYDom, Xcount, &setValsX[0]);
    nc_put_vara_double(OPEN_NC_ID, x_bndsID, startXYDom, xBndsCount,
                       &setValsXBnds[0][0]);

    // Fill "y" and "y_bnds"
    // Because "y_bnds" is a 2D variable, use a nested loop to fill the columns
    // "yBndsVal" is only incremented if it is not the last column
    for(indexY = 0; indexY < nDimY; indexY++) {
        setValsY[indexY] = Yval;
        Yval += yIncrement;

        for(indexBnds = 0; indexBnds < NUMBNDS; indexBnds++) {
            setValsYBnds[indexY][indexBnds] = yBndsVal;

            if(indexBnds == 0) {
                yBndsVal += yIncrement;
            }
        }
    }

    nc_put_vara_double(OPEN_NC_ID, yID, startXYDom, Ycount, &setValsY[0]);
    nc_put_vara_double(OPEN_NC_ID, y_bndsID, startXYDom, yBndsCount,
                       &setValsYBnds[0][0]);

    // Fill "domain"
    for(indexY = 0; indexY < nDimY; indexY++) {
        for(indexX = 0; indexX < nDimX; indexX++) {
            setValsDom[indexY][indexX] = DomVal;
            DomVal += DomIncrement;
        }
    }

    nc_put_vara_float(OPEN_NC_ID, domainID, startXYDom, DomCount,
                      &setValsDom[0][0]);
}

/**
 * @brief Fill all values within the s domain netCDF
 *
 * @param[in] nDimS Size of dimension "site"
 * @param[in] siteID ID of the "site" variable within the domain netCDF
 * @param[in] domainID ID of the domain variable within the domain netCDF
 * @param[in] xID ID of the x variable within the domain netCDF
 * @param[in] yID ID of the y variable within the domain netCDF
*/
static void fill_s_vars(int nDimS, int siteID, int domainID, int xID, int yID) {

    double setValsX[nDimS], setValsY[nDimS];
    int setValsSite[nDimS];
    size_t start[] = {0}, count[] = {nDimS};
    double yVal = 2508826.1, yDecrement = 5000.0;
    int xValStart = 2000.0, xValTemp = xValStart, xValIncrement = 4000.0;
    float domVal = 1.0, domIncrement = 1.0, setValsDom[nDimS];
    int siteVal = 1, siteIncrement = 1;
    int valIndex;

    // Fill values
    for(valIndex = 0; valIndex < nDimS; valIndex++) {
        setValsX[valIndex] = xValTemp;
        setValsY[valIndex] = yVal;
        setValsDom[valIndex] = domVal;
        setValsSite[valIndex] = siteVal;

        siteVal += siteIncrement;
        domVal += domIncrement;
        xValTemp += xValIncrement;

        // Check if filling is half way through (base0)
        if(valIndex == (nDimS / 2) - 1) {
            // Reset "x" value and decrement "y" value
            xValTemp = xValStart;
            yVal -= yDecrement;
        }
    }

    nc_put_vara_int(OPEN_NC_ID, siteID, start, count, &setValsSite[0]);
    nc_put_vara_float(OPEN_NC_ID, domainID, start, count, &setValsDom[0]);
    nc_put_vara_double(OPEN_NC_ID, xID, start, count, &setValsX[0]);
    nc_put_vara_double(OPEN_NC_ID, yID, start, count, &setValsY[0]);
}

/**
 * @brief Fill a given netCDF file with global attributes
 *
 * @param[in] LogInfo Holds information dealing with logfile output
*/
static void write_global_atts(LOG_INFO* LogInfo) {

    // Time information
    int tFormatSize = 20, dateSize = 10, yearOffset = 1900, monOffset = 1;
    char timeStr[tFormatSize], dateStr[dateSize];
    time_t timeInfo;
    struct tm timeStruct;

    // Version information
    int versionStrSize = 40;
    char verstionStr[versionStrSize];

    timeInfo = time(NULL);
    timeStruct = *localtime(&timeInfo);

    // Format date/version strings
    sprintf(timeStr, "%d-%02d-%02d %02d:%02d:%02d",
            timeStruct.tm_year + yearOffset, timeStruct.tm_mon + monOffset,
            timeStruct.tm_mday, timeStruct.tm_hour, timeStruct.tm_min,
            timeStruct.tm_sec);
    sprintf(dateStr, "v%d%02d%02d", timeStruct.tm_year + yearOffset,
            timeStruct.tm_mon + monOffset, timeStruct.tm_mday);

    // Create version string
    sprintf(verstionStr, "SOILWAT2 %s", SW2_VERSION);

    // Fill global attributes
    write_att_str("Conventions", "CF-1.8", NC_GLOBAL, LogInfo);
    write_att_str("created_by", verstionStr, NC_GLOBAL, LogInfo);
    write_att_str("creation_date", timeStr, NC_GLOBAL, LogInfo);
    write_att_str("title", "Domain for SOILWAT2", NC_GLOBAL, LogInfo);
    write_att_str("version", dateStr, NC_GLOBAL, LogInfo);
    write_att_str("source_id", "SOILWAT2", NC_GLOBAL, LogInfo);
    write_att_str("further_info_url", "https://github.com/DrylandEcology/",
                    NC_GLOBAL, LogInfo);

    write_att_str("source_type", "LAND", NC_GLOBAL, LogInfo);
    write_att_str("realm", "land", NC_GLOBAL, LogInfo);
    write_att_str("product", "model-input", NC_GLOBAL, LogInfo);
    write_att_str("grid", "native Alberts projection grid with NAD83 datum",
                    NC_GLOBAL, LogInfo);

    write_att_str("grid_label", "gn", NC_GLOBAL, LogInfo);
    write_att_str("nominal_resolution", "1 m", NC_GLOBAL, LogInfo);
    write_att_str("featureType", "point", NC_GLOBAL, LogInfo);
    write_att_str("time_label", "None", NC_GLOBAL, LogInfo);
    write_att_str("time_title", "No temporal dimensions ... fixed field",
                    NC_GLOBAL, LogInfo);
}

/**
 * @brief Create a dimension within any netCDF
 *
 * @param[in] dimName Name of the dimension
 * @param[in] dimID Dimension identifier
 * @param[in] size Dimension size
 * @param[in] LogInfo Holds information dealing with logfile output
*/
static void create_dimension(char* dimName, int* dimID, int size, LOG_INFO* LogInfo) {

    int res = nc_def_dim(OPEN_NC_ID, dimName, size, dimID);

    if(res != NC_NOERR) {
        LogError(LogInfo, LOGFATAL, "A problem has occurred when creating"\
                 " dimension %s.", dimName);
    }
}

/**
 * @brief Create a variable within any netCDF
 *
 * @param[in] varName Name of the variable
 * @param[in] numDims Number of dimensions the new variable has
 * @param[in] dims Dimensions of the variable
 * @param[in] varType Type of written variable (e.g., NC_DOUBLE)
 * @param[in] varID New variable ID
 * @param[in] LogInfo Holds information dealing with logfile output
*/
static void create_variable(char* varName, int numDims, int dims[], int varType,
                     int* varID, LOG_INFO* LogInfo) {

    int res = nc_def_var(OPEN_NC_ID, varName, varType, numDims, dims, varID);

    if(res != NC_NOERR) {
        LogError(LogInfo, LOGFATAL, "A problem has occurred when creating"
                 " the %s variable.", varName);
    }
}

/**
 * @brief Write an attribute (string value) to a variable
 *
 * @param[in] attName Attribute name to write
 * @param[in] attValue String value to write
 * @param[in] varID Variable id within netCDF to write to
 * @param[in] LogInfo Holds information dealing with logfile output
*/
static void write_att_str(char* attName, char* attValue, int varID, LOG_INFO* LogInfo) {

    int res = nc_put_att_text(OPEN_NC_ID, varID, attName, strlen(attValue),
                              attValue);

    if(res != NC_NOERR) {
        LogError(LogInfo, LOGWARN, "A problem occurred when writing"\
                " attribute %s.", attName);
    }
}

/**
 * @brief Write an attribute (double value) to a variable
 *
 * @param[in] attName Attribute name to write
 * @param[in] attValue String value to write
 * @param[in] attSize Number of values that will be written to the attribute
 * @param[in] varID Variable id within netCDF to write to
 * @param[in] LogInfo Holds information dealing with logfile output
*/
static void write_att_val(char* attName, double attValue[], int attSize,
                   int varID, LOG_INFO* LogInfo) {

    int res = nc_put_att_double(OPEN_NC_ID, varID, attName, NC_DOUBLE,
                                attSize, attValue);

    if(res != NC_NOERR) {
        LogError(LogInfo, LOGWARN, "A problem occurred when writing the" \
                 " attribute %s.", attName);
    }
}
