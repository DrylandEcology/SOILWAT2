#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netcdf.h>
#include <time.h>

#include "include/SW_netCDF.h"
#include "include/SW_Files.h"
#include "include/myMemory.h"
#include "include/filefuncs.h"

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

    MyFileName = PathInfo->InFiles_csv[eNCIn];
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
 * @brief Helper function to `SW_NC_read_files()` to determine which input
 *        key has been read in
 * @param[in] key Read-in key from domain input files file
*/
int nc_key_to_ID(char* key) {
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
 * @brief Creates a new domain netCDF if one was not provided
 *
 * @param[in] SW_Domain Struct of type SW_DOMAIN holding constant
 *  temporal/spatial information for a set of simulation runs
 * @param[in] DomainName Domain netCDF name
 * @param[in] LogInfo Holds information dealing with logfile output
*/
void SW_NC_create_domain(SW_DOMAIN* SW_Domain, char* DomainName,
                         LOG_INFO* LogInfo) {

    nc_create(DomainName, NC_NETCDF4, NULL); // Don't store file ID (OPEN_NC_ID)

    create_var_crs(LogInfo);
    write_global_domain_atts(LogInfo);
    nc_close(OPEN_NC_ID);
}


/**
 * @brief Create the "crs" variable in a netCDF
 *
 * @param[in] LogInfo Holds information dealing with logfile output
*/
void create_var_crs(LOG_INFO* LogInfo) {
    int crsID;
    double stdParallel[] = {29.5, 45.5}, longCentralMerid[] = {-96.0},
        latProjOrigin[] = {23.0}, false_easting[] = {0.0},
        false_northing[] = {0.0}, longPrimeMerid[] = {0.0},
        semiMajorAxis[] = {6378137.0}, inverse_flattening[] = {298.257222101};
    size_t sizeOne = 1, sizeTwo = 2;

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

    write_att_val("standard_parallel", stdParallel, sizeTwo, crsID, LogInfo);
    write_att_val("longitude_of_central_meridian", longCentralMerid, sizeOne,
                  crsID, LogInfo);
    write_att_val("latitude_of_projection_origin", latProjOrigin, sizeOne,
                  crsID, LogInfo);
    write_att_val("false_easting", false_easting, sizeOne, crsID, LogInfo);
    write_att_val("false_northing", false_northing, sizeOne, crsID, LogInfo);
    write_att_val("longitude_of_prime_meridian", longPrimeMerid, sizeOne,
                  crsID, LogInfo);
    write_att_val("semi_major_axis", semiMajorAxis, sizeOne, crsID, LogInfo);
    write_att_val("inverse_flattening", inverse_flattening, sizeOne,
                  crsID, LogInfo);

    write_att_str("crs_wkt", crs_wktVal, crsID, LogInfo);
}
/**
 * @brief Fill a domain netCDF with global attributes (only when a domain
 *  netCDF is generated)
 *
 * @param[in] LogInfo Holds information dealing with logfile output
*/
void write_global_domain_atts(LOG_INFO* LogInfo) {

    // Time information
    int tFormatSize = 20, dateSize = 10, yearOffset = 1900, monOffset = 1;
    char timeStr[tFormatSize], dateStr[dateSize];
    time_t timeInfo = time(NULL);
    struct tm timeStruct = *localtime(&timeInfo);

    // Format date/version strings
    sprintf(timeStr, "%d-%02d-%02d %02d:%02d:%02d",
            timeStruct.tm_year + yearOffset, timeStruct.tm_mon + monOffset,
            timeStruct.tm_mday, timeStruct.tm_hour, timeStruct.tm_min,
            timeStruct.tm_sec);
    sprintf(dateStr, "v%d%02d%02d", timeStruct.tm_year + yearOffset,
            timeStruct.tm_mon + monOffset, timeStruct.tm_mday);

    // Fill global attributes
    write_att_str("Conventions", "CF-1.8", NC_GLOBAL, LogInfo);
    write_att_str("created_by", "SOILWAT2 v7.1.0", NC_GLOBAL, LogInfo);
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
void create_dimension(char* dimName, int* dimID, int size, LOG_INFO* LogInfo) {

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
void create_variable(char* varName, int numDims, int dims[], int varType,
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
void write_att_str(char* attName, char* attValue, int varID, LOG_INFO* LogInfo) {

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
void write_att_val(char* attName, double attValue[], int attSize,
                   int varID, LOG_INFO* LogInfo) {

    int res = nc_put_att_double(OPEN_NC_ID, varID, attName, NC_DOUBLE,
                                attSize, attValue);

    if(res != NC_NOERR) {
        LogError(LogInfo, LOGWARN, "A problem occurred when writing the" \
                 " attribute %s.", attName);
    }
}
