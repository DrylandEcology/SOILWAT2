#include "include/SW_netCDF.h"

#include <stdio.h>
#include <string.h>
#include <netcdf.h>

#include "include/filefuncs.h"

/* =================================================== */
/*             Local Function Definitions              */
/* --------------------------------------------------- */

/**
 * @brief Create a dimension within a netCDF file
 *
 * @param[in] dimName Name of the new dimension
 * @param[in] size Value/size of the dimension
 * @param[in] domFileID Domain netCDF file ID
 * @param[in,out] dimID Dimension ID
 * @param[in,out] LogInfo Holds information dealing with logfile output
*/
static void create_netCDF_dim(const char* dimName, unsigned long size,
                    int* domFileID, int* dimID, LOG_INFO* LogInfo) {
    char errorStr[MAX_LOG_SIZE];

    if(nc_def_dim(*domFileID, dimName, size, dimID) != NC_NOERR) {
        sprintf(errorStr, "Could not create dimension '%s' in "
                          "domain netCDF.", dimName);

        LogError(LogInfo, LOGERROR, errorStr);
    }
}

/**
 * @brief Create a variable within a netCDF file
 *
 * @param[in] varName Name of the new variable
 * @param[in] dimIDs Dimensions of the variable
 * @param[in] domFileID Domain netCDF file ID
 * @param[in] varType The type in which the new variable will be
 * @param[in] numDims Number of dimensions the new variable will hold
 * @param[in,out] LogInfo Holds information dealing with logfile output
*/
static void create_netCDF_var(const char* varName, int* dimIDs, int* domFileID,
                              int varType, int numDims, LOG_INFO* LogInfo) {
    int varID; // Not used
    char errorStr[MAX_LOG_SIZE];

    if(nc_def_var(*domFileID, varName, varType, numDims, dimIDs, &varID) != NC_NOERR) {
        sprintf(errorStr, "Could not create '%s' variable in "
                          "domain netCDF.", varName);

        LogError(LogInfo, LOGERROR, errorStr);
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
    bndVarDims[1] = bndsID;

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

    // Create x, x_bnds, y, and y_bnds variables
    create_netCDF_var("x", xDimID, domFileID, NC_DOUBLE, 1, LogInfo);
    if(LogInfo->stopRun) {
        return; // Exit prematurely due to error
    }

    bndVarDims[0] = *xDimID;

    create_netCDF_var("x_bnds", bndVarDims, domFileID, NC_DOUBLE, 2, LogInfo);
    if(LogInfo->stopRun) {
        return; // Exit prematurely due to error
    }

    create_netCDF_var("y", yDimID, domFileID, NC_DOUBLE, 1, LogInfo);
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
 * @brief Create “domain_template.nc” if “domain.nc” does not exists
 *
 * @param[in] SW_Domain Struct of type SW_DOMAIN holding constant
 *  temporal/spatial information for a set of simulation runs
 * @param[in] domFileName File name in which the new netCDF file will have
 * @param[in,out] LogInfo Holds information dealing with logfile output
*/
void SW_NC_create_domain_template(SW_DOMAIN* SW_Domain, const char* domFileName,
                                  LOG_INFO* LogInfo) {

    int* domFileID = &SW_Domain->PathInfo.domainFileID;
    int sDimID = 0, xDimID = 0, yDimID = 0; // varID is not used
    int domDims[2]; // Either [&yDimID, &xDimID] or [&sDimID, 0]
    int nDomainDims;

    if(FileExists(domFileName)) {
        LogError(LogInfo, LOGERROR, "Could not create new domain template. "
                                    "This is due to the fact that it already "
                                    " exists. Please modify it and change the name.");
        return; // Exit prematurely due to error
    }

    if(nc_create(domFileName, NC_NETCDF4, domFileID) != NC_NOERR) {
        LogError(LogInfo, LOGERROR, "Could not create new domain template due "
                                    "to something internal.");
        return; // Exit prematurely due to error
    }

    if(strcmp(SW_Domain->DomainType, "s") == 0) {
        nDomainDims = 1;
        domDims[0] = sDimID;
        domDims[1] = 0;

        // Create s dimension/domain variables
        fill_domain_netCDF_s(SW_Domain->nDimS, domFileID, &sDimID,
                             &xDimID, &yDimID, LogInfo);
        if(LogInfo->stopRun) {
            nc_close(*domFileID);
            return; // Exit prematurely due to error
        }

    } else {
        nDomainDims = 2;
        domDims[0] = yDimID;
        domDims[1] = xDimID;

        fill_domain_netCDF_xy(SW_Domain->nDimX, SW_Domain->nDimY, domFileID,
                              &xDimID, &yDimID, LogInfo);
        if(LogInfo->stopRun) {
            nc_close(*domFileID);
            return; // Exit prematurely due to error
        }
    }

    // Create domain variable
    create_netCDF_var("domain", domDims, domFileID, NC_FLOAT, nDomainDims, LogInfo);

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
    (void) SW_Domain;
    (void) LogInfo;
}

