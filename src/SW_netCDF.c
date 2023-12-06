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
#define NUM_ATT_IN_KEYS 27 // Number of possible keys within `attributes_nc.in`

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

            "proj_datum", "proj_units", "proj_long_name",
            "proj_grid_mapping_name", "proj_crs_wkt",
            "proj_longitude_of_prime_meridian", "proj_semi_major_axis",
            "proj_inverse_flattening", "proj_datum", "proj_units",
            "proj_standard_parallel", "proj_longitude_of_central_meridian",
            "proj_latitude_of_projection_origin", "proj_false_easting",
            "proj_false_northing"
            };

    FILE *f;
    char inbuf[MAX_FILENAMESIZE * 2], value[MAX_FILENAMESIZE * 2]; // * 2 - fit crs_wkt
    char key[35]; // 34 - Max key size
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

    while (GetALine(f, inbuf)) {
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
                ncInfo->primary_crs = Str_Dup(value, LogInfo);
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
                ncInfo->crs_projsc.datum = Str_Dup(value, LogInfo);
                break;
            case 13:
                ncInfo->crs_geogsc.long_name = Str_Dup(value, LogInfo);
                break;
            case 14:
                ncInfo->crs_geogsc.grid_mapping_name = Str_Dup(value, LogInfo);
                break;
            case 15:
                ncInfo->crs_geogsc.crs_wkt = Str_Dup(value, LogInfo);
                projCRSFound = swTRUE;
                break;
            case 16:
                ncInfo->crs_geogsc.longitude_of_prime_meridian = atof(value);
                break;
            case 17:
                ncInfo->crs_geogsc.semi_major_axis = atof(value);
                break;
            case 18:
                ncInfo->crs_geogsc.inverse_flattening = atof(value);
                break;
            case 19:
                ncInfo->crs_projsc.datum = Str_Dup(value, LogInfo);
                break;
            case 20:
                ncInfo->crs_projsc.units = Str_Dup(value, LogInfo);
                break;
            case 21:
                // Re-scan for 1 or 2 values of standard parallel(s)
                // the user may separate values by white-space, comma, etc.
                n = sscanf(inbuf, "%34s %f%*[^-.0123456789]%f", key, &num1, &num2);

                ncInfo->crs_projsc.standard_parallel[0] = num1;
                ncInfo->crs_projsc.standard_parallel[1] = (n == 3) ? num2 : NAN;
                break;
            case 22:
                ncInfo->crs_projsc.longitude_of_central_meridian = atof(value);
                break;
            case 23:
                ncInfo->crs_projsc.latitude_of_projection_origin = atof(value);
                break;
            case 24:
                ncInfo->crs_projsc.false_easting = atoi(value);
                break;
            case 25:
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
 * @param[in] varName Name of the new variable
 * @param[in] dimIDs Dimensions of the variable
 * @param[in] ncFileID Domain netCDF file ID
 * @param[in] varType The type in which the new variable will be
 * @param[in] numDims Number of dimensions the new variable will hold
 * @param[in,out] LogInfo Holds information dealing with logfile output
*/
static void create_netCDF_var(const char* varName, int* dimIDs, int* ncFileID,
                              int varType, int numDims, LOG_INFO* LogInfo) {
    int varID; // Not used

    if(nc_def_var(*ncFileID, varName, varType, numDims, dimIDs, &varID) != NC_NOERR) {
        LogError(LogInfo, LOGERROR, "Could not create '%s' variable in "
                                    "netCDF.", varName);
    }
    (void) varID;
}

/**
 * @brief Fill the domain netCDF file with variables that are for domain type "s"
 *
 * @param[in] nDimS Size of the 's' dimension (number of sites)
 * @param[in] domFileID Domain netCDF file ID
 * @param[out] sDimID 's' dimension ID
 * @param[out] xDimID 'x' dimension ID
 * @param[out] yDimID 'y' dimension ID
 * @param[in,out] LogInfo Holds information dealing with logfile output
*/
static void fill_domain_netCDF_s(unsigned long nDimS, int* domFileID, int* sDimID,
                                 int* xDimID, int* yDimID, LOG_INFO* LogInfo) {

    create_netCDF_dim("site", nDimS, domFileID, sDimID, LogInfo);
    if(LogInfo->stopRun) {
        return; // Exit prematurely due to error
    }

    create_netCDF_var("x", xDimID, domFileID, NC_DOUBLE, 1, LogInfo);
    if(LogInfo->stopRun) {
        return; // Exit prematurely due to error
    }

    create_netCDF_var("y", yDimID, domFileID, NC_DOUBLE, 1, LogInfo);
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
    create_netCDF_var("x", xDimID, domFileID, NC_DOUBLE, 1, LogInfo);
    if(LogInfo->stopRun) {
        return; // Exit prematurely due to error
    }

    create_netCDF_var("y", yDimID, domFileID, NC_DOUBLE, 1, LogInfo);
    if(LogInfo->stopRun) {
        return; // Exit prematurely due to error
    }

    bndVarDims[0] = *xDimID;
    bndVarDims[1] = bndsID;

    create_netCDF_var("x_bnds", bndVarDims, domFileID, NC_DOUBLE, 2, LogInfo);
    if(LogInfo->stopRun) {
        return; // Exit prematurely due to error
    }

    bndVarDims[0] = *yDimID;

    create_netCDF_var("y_bnds", bndVarDims, domFileID, NC_DOUBLE, 2, LogInfo);
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

    int* domFileID = &SW_Domain->PathInfo.ncFileIDs[DOMAIN_NC];
    int sDimID = 0, xDimID = 0, yDimID = 0; // varID is not used
    int domDims[2]; // Either [yDimID, xDimID] or [sDimID, 0]
    int nDomainDims;

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
        fill_domain_netCDF_s(SW_Domain->nDimS, domFileID, &sDimID,
                             &xDimID, &yDimID, LogInfo);
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
    create_netCDF_var(SW_Domain->netCDFInfo.varNC[DOMAIN_NC], domDims,
                      domFileID, NC_FLOAT, nDomainDims, LogInfo);

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
    while(GetALine(f, inbuf)) {
        sscanf(inbuf, "%s %s %s", key, varName, path);

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
