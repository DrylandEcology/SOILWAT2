#include "include/SW_netCDF.h"

#include <stdio.h>
#include <string.h>
#include <netcdf.h>

#include "include/filefuncs.h"
#include "include/SW_Defines.h"
#include "include/SW_Files.h"
#include "include/myMemory.h"

/* =================================================== */
/*                   Local Defines                     */
/* --------------------------------------------------- */

#define NUM_NC_IN_KEYS 1 // Number of possible keys within `files_nc.in`

/* =================================================== */
/*             Local Function Definitions              */
/* --------------------------------------------------- */

/**
 * @brief Convert a key read-in from `files_nc.in` to an index
 *  the program can understand
 *
 * @param[in] key Key found within the file to test for
*/
static int nc_key_to_id(const char* key) {
    static const char* knownKeys[] = {"domain"};

    int id;

    for(id = 0; id < NUM_NC_IN_KEYS; id++) {
        if(strcmp(key, knownKeys[id]) == 0) {
            return id;
        }
    }

    return KEY_NOT_FOUND;
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
 * @param[in] SW_Domain Struct of type SW_DOMAIN holding constant
 *  temporal/spatial information for a set of simulation runs
 * @param[in] LogInfo Holds information dealing with logfile output
*/
void SW_NC_read(SW_DOMAIN* SW_Domain, LOG_INFO* LogInfo) {
    FILE *f;
    char inbuf[MAX_FILENAMESIZE], *MyFileName;
    char key[15]; // 15 - Max key size
    char varName[MAX_FILENAMESIZE], path[MAX_FILENAMESIZE];
    int keyID;

    MyFileName = SW_Domain->PathInfo.InFiles[eNCIn];
	f = OpenFile(MyFileName, "r", LogInfo);

    // Get domain file name
    while(GetALine(f, inbuf)) {
        sscanf(inbuf, "%s %s %s", key, varName, path);

        keyID = nc_key_to_id(key);
        switch(keyID) {
            case DOMAIN_NC:
                SW_Domain->netCDFInfo.varNC[DOMAIN_NC] = Str_Dup(varName, LogInfo);
                SW_Domain->netCDFInfo.InFilesNC[DOMAIN_NC] = Str_Dup(path, LogInfo);
                break;
            default:
                LogError(LogInfo, LOGWARN, "Ignoring unknown key in %s, %s",
                         MyFileName, key);
                break;
        }
    }
}

