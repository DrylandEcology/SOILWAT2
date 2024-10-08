/* =================================================== */
/*                INCLUDES / DEFINES                   */
/* --------------------------------------------------- */
#include "include/SW_netCDF_General.h" // for SW_NC_get_dimlen_from_dimid, ...
#include "include/filefuncs.h"         // for LogError, FileExists, CloseFile
#include "include/generic.h"           // for Bool, swFALSE, LOGERROR, swTRUE
#include "include/myMemory.h"          // for Str_Dup, Mem_Malloc
#include "include/SW_datastructs.h"    // for LOG_INFO, SW_NETCDF_OUT, SW_DOMAIN
#include "include/SW_Defines.h"        // for MAX_FILENAMESIZE, OutPeriod
#include "include/SW_Domain.h"         // for SW_DOM_calc_ncSuid
#include "include/SW_Files.h"          // for eNCInAtt, eNCIn, eNCOutVars
#include "include/SW_netCDF_Input.h"   // for
#include "include/SW_netCDF_Output.h"  // for
#include "include/SW_Output.h"         // for ForEachOutKey, SW_ESTAB, pd2...
#include "include/SW_Output_outarray.h" // for iOUTnc
#include "include/SW_VegProd.h"         // for key2veg
#include "include/Times.h"              // for isleapyear, timeStringISO8601
#include <math.h>                       // for NAN, ceil, isnan
#include <netcdf.h>                     // for NC_NOERR, nc_close, NC_DOUBLE
#include <stdio.h>                      // for size_t, NULL, snprintf, sscanf
#include <stdlib.h>                     // for free, strtod
#include <string.h>                     // for strcmp, strlen, strstr, memcpy

/* =================================================== */
/*                   Local Defines                     */
/* --------------------------------------------------- */

/** Number of possible keys within `files_nc.in` */
#define NUM_NC_IN_KEYS 2

/* =================================================== */
/*             Local Function Definitions              */
/* --------------------------------------------------- */

/**
@brief Get a double value from an attribute

@param[in] ncFileID Identifier of the open netCDF file to test
@param[in] varName Name of the variable to access
@param[in] attName Name of the attribute to access
@param[out] attVal String buffer to hold the resulting value
@param[out] LogInfo Holds information on warnings and errors
*/
static void get_double_att_val(
    int ncFileID,
    const char *varName,
    const char *attName,
    double *attVal,
    LOG_INFO *LogInfo
) {

    int varID = 0;
    int attCallRes;
    SW_NC_get_var_identifier(ncFileID, varName, &varID, LogInfo);
    if (LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

    attCallRes = nc_get_att_double(ncFileID, varID, attName, attVal);
    if (attCallRes == NC_ENOTATT) {
        LogError(
            LogInfo,
            LOGERROR,
            "Could not find an attribute %s "
            "for the variable %s.",
            attName,
            varName
        );
    } else if (attCallRes != NC_NOERR) {
        LogError(
            LogInfo,
            LOGERROR,
            "An error occurred when attempting to "
            "access the attribute %s of variable %s.",
            attName,
            varName
        );
    }
}

/**
@brief Overwrite specific global attributes into a new file

@param[in] ncFileID Identifier of the open netCDF file to write all information
@param[in] domType Type of domain in which simulations are running
    (gridcell/sites)
@param[in] freqAtt Value of a global attribute "frequency"
    * fixed (no time): "fx"
    * has time: "day", "week", "month", or "year"
@param[in] isInputFile Specifies if the file being written to is input
@param[in,out] LogInfo Holds information dealing with logfile output
*/
static void update_netCDF_global_atts(
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
    const int numGlobAtts = (strcmp(domType, "s") == 0) ? 5 : 4;
    const char *attNames[] = {
        "source", "creation_date", "product", "frequency", "featureType"
    };

    const char *productStr = (isInputFile) ? "model-input" : "model-output";
    const char *featureTypeStr;
    if (strcmp(domType, "s") == 0) {
        featureTypeStr = (strcmp(freqAtt, "fx") == 0) ? "point" : "timeSeries";
    } else {
        featureTypeStr = "";
    }

    const char *attVals[] = {
        sourceStr, creationDateStr, productStr, freqAtt, featureTypeStr
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

/* =================================================== */
/*             Global Function Definitions             */
/* --------------------------------------------------- */

/**
@brief Get a dimension value from a given netCDF file

@param[in] ncFileID Identifier of the open netCDF file to access
@param[in] dimID Identifier of the dimension to access
@param[out] dimVal String buffer to hold the resulting value
@param[out] LogInfo Holds information on warnings and errors
*/
void SW_NC_get_dimlen_from_dimid(
    int ncFileID, int dimID, size_t *dimVal, LOG_INFO *LogInfo
) {

    if (nc_inq_dimlen(ncFileID, dimID, dimVal) != NC_NOERR) {
        LogError(
            LogInfo,
            LOGERROR,
            "An error occurred when attempting "
            "to retrieve the dimension value of "
            "%d.",
            dimID
        );
    }
}

/**
@brief Get the dimension identifiers of a given variable within
a netCDF file

@param[in] ncFileID Identifier of the open netCDF file to access
@param[in] varID Variable identifier within the given netCDF
(-1 if it is unknown what the ID is before the call to this function)
@param[in] varName Name of the variable to get the dimension sizes of
@param[out] dimIDs Resulting IDs of the dimensions for the given variable
@param[out] nDims Number of dimensions the variable has
@param[out] LogInfo Holds information on warnings and errors
*/
void SW_NC_get_vardimids(
    int ncFileID,
    int varID,
    char *varName,
    int dimIDs[],
    int *nDims,
    LOG_INFO *LogInfo
) {
    if (varID == -1 && nc_inq_varid(ncFileID, varName, &varID) != NC_NOERR) {
        LogError(
            LogInfo,
            LOGERROR,
            "Could not get identifier of the variable '%s'.",
            "site"
        );
        return;
    }

    if (nc_inq_varndims(ncFileID, varID, nDims) != NC_NOERR) {
        LogError(
            LogInfo,
            LOGERROR,
            "Could not get the number of dimensions for the variable "
            "'%s'.",
            varName
        );
        return;
    }

    if (nc_inq_vardimid(ncFileID, varID, dimIDs) != NC_NOERR) {
        LogError(
            LogInfo,
            LOGERROR,
            "Could not get the identifiers of the dimension of the "
            "variable '%s'.",
            "site"
        );
    }
}

/**
@brief Get a dimension identifier within a given netCDF

@param[in] ncFileID Identifier of the open netCDF file to access
@param[in] dimName Name of the new dimension
@param[out] dimID Identifier of the dimension
@param[out] LogInfo Holds information on warnings and errors
*/
void SW_NC_get_dim_identifier(
    int ncFileID, const char *dimName, int *dimID, LOG_INFO *LogInfo
) {

    if (nc_inq_dimid(ncFileID, dimName, dimID) != NC_NOERR) {
        LogError(
            LogInfo,
            LOGERROR,
            "An error occurred attempting to retrieve the "
            "identifier of the dimension %s.",
            dimName
        );
    }
}

/**
@brief Check that the constant content is consistent between
domain.in and a given netCDF file

If ncFileID is negative, then the netCDF fileName will be temporarily
opened for read-access.

@param[in] SW_Domain Struct of type SW_DOMAIN holding constant
    temporal/spatial information for a set of simulation runs
@param[in] ncFileID Identifier of the open netCDF file to check
@param[in] fileName Name of netCDF file to test (used for error messages)
@param[out] LogInfo Holds information on warnings and errors
*/
void SW_NC_check(
    SW_DOMAIN *SW_Domain, int ncFileID, const char *fileName, LOG_INFO *LogInfo
) {

    Bool fileWasClosed = (ncFileID < 0) ? swTRUE : swFALSE;
    Bool geoIsPrimCRS =
        SW_Domain->OutDom.netCDFOutput.primary_crs_is_geographic;

    /* Get latitude/longitude or x/y names that were read-in from domain
       input file */
    char *readinYName = (geoIsPrimCRS) ?
                            SW_Domain->OutDom.netCDFOutput.geo_YAxisName :
                            SW_Domain->OutDom.netCDFOutput.proj_YAxisName;
    char *readinXName = (geoIsPrimCRS) ?
                            SW_Domain->OutDom.netCDFOutput.geo_XAxisName :
                            SW_Domain->OutDom.netCDFOutput.proj_XAxisName;

    if (fileWasClosed) {
        // "Once a netCDF dataset is opened, it is referred to by a netCDF ID,
        // which is a small non-negative integer"
        SW_NC_open(fileName, NC_NOWRITE, &ncFileID, LogInfo);
        if (LogInfo->stopRun) {
            return; /* Exit function prematurely due to error */
        }
    }

    SW_CRS *crs_geogsc = &SW_Domain->OutDom.netCDFOutput.crs_geogsc;
    SW_CRS *crs_projsc = &SW_Domain->OutDom.netCDFOutput.crs_projsc;
    char *siteName = SW_Domain->OutDom.netCDFOutput.siteName;
    char strAttVal[LARGE_VALUE];
    double doubleAttVal;
    const char *geoCRS = "crs_geogsc";
    const char *projCRS = "crs_projsc";
    Bool geoCRSExists = SW_NC_varExists(ncFileID, geoCRS);
    Bool projCRSExists = SW_NC_varExists(ncFileID, projCRS);
    const char *impliedDomType =
        (SW_NC_dimExists(siteName, ncFileID)) ? "s" : "xy";
    Bool dimMismatch = swFALSE;
    size_t latDimVal = 0;
    size_t lonDimVal = 0;
    size_t SDimVal = 0;

    const char *strAttsToComp[] = {"long_name", "grid_mapping_name", "crs_wkt"};
    const char *doubleAttsToComp[] = {
        "longitude_of_prime_meridian", "semi_major_axis", "inverse_flattening"
    };

    const char *strProjAttsToComp[] = {"datum", "units"};
    const char *doubleProjAttsToComp[] = {
        "longitude_of_central_meridian",
        "latitude_of_projection_origin",
        "false_easting",
        "false_northing"
    };
    const char *stdParallel = "standard_parallel";
    const double stdParVals[] = {
        crs_projsc->standard_parallel[0], crs_projsc->standard_parallel[1]
    };

    const char *geoStrAttVals[] = {
        crs_geogsc->long_name,
        crs_geogsc->grid_mapping_name,
        crs_geogsc->crs_wkt
    };
    const double geoDoubleAttVals[] = {
        crs_geogsc->longitude_of_prime_meridian,
        crs_geogsc->semi_major_axis,
        crs_geogsc->inverse_flattening
    };
    const char *projStrAttVals[] = {
        crs_projsc->long_name,
        crs_projsc->grid_mapping_name,
        crs_projsc->crs_wkt
    };
    const double projDoubleAttVals[] = {
        crs_projsc->longitude_of_prime_meridian,
        crs_projsc->semi_major_axis,
        crs_projsc->inverse_flattening
    };

    const char *strProjAttVals[] = {
        SW_Domain->OutDom.netCDFOutput.crs_projsc.datum,
        SW_Domain->OutDom.netCDFOutput.crs_projsc.units
    };
    const double doubleProjAttVals[] = {
        crs_projsc->longitude_of_central_meridian,
        crs_projsc->latitude_of_projection_origin,
        crs_projsc->false_easting,
        crs_projsc->false_northing,
    };

    const int numNormAtts = 3;
    const int numProjStrAtts = 2;
    const int numProjDoubleAtts = 4;
    double projStdParallel[2]; // Compare to standard_parallel is projected CRS
    int attNum;

    const char *attFailMsg =
        "The attribute '%s' of the variable '%s' "
        "within the file %s does not match the one in the domain input "
        "file. Please make sure these match.";

    /*
       Make sure the domain types are consistent
    */
    if (strcmp(SW_Domain->DomainType, impliedDomType) != 0) {
        LogError(
            LogInfo,
            LOGERROR,
            "The existing file ('%s') has a domain type '%s'; "
            "however, the current simulation uses a domain type '%s'. "
            "Please make sure these match.",
            fileName,
            impliedDomType,
            SW_Domain->DomainType
        );
        goto wrapUp; // Exit function prematurely due to error
    }

    /*
       Make sure the dimensions of the netCDF file is consistent with the
       domain input file
    */
    if (strcmp(impliedDomType, "s") == 0) {
        SW_NC_get_dimlen_from_dimname(ncFileID, siteName, &SDimVal, LogInfo);
        if (LogInfo->stopRun) {
            goto wrapUp; // Exit function prematurely due to error
        }

        dimMismatch = (Bool) (SDimVal != SW_Domain->nDimS);
    } else if (strcmp(impliedDomType, "xy") == 0) {
        SW_NC_get_dimlen_from_dimname(
            ncFileID, readinYName, &latDimVal, LogInfo
        );
        if (LogInfo->stopRun) {
            goto wrapUp; // Exit function prematurely due to error
        }
        SW_NC_get_dimlen_from_dimname(
            ncFileID, readinXName, &lonDimVal, LogInfo
        );
        if (LogInfo->stopRun) {
            goto wrapUp; // Exit function prematurely due to error
        }

        dimMismatch = (Bool) (latDimVal != SW_Domain->nDimY ||
                              lonDimVal != SW_Domain->nDimX);
    }

    if (dimMismatch) {
        LogError(
            LogInfo,
            LOGERROR,
            "The size of the dimensions in %s do "
            "not match the domain input file's. Please make sure "
            "these match.",
            fileName
        );
        goto wrapUp; // Exit function prematurely due to error
    }

    /*
       Make sure the geographic CRS information is consistent with the
       domain input file - both string and double values
    */
    if (geoCRSExists) {
        for (attNum = 0; attNum < numNormAtts; attNum++) {
            SW_NC_get_str_att_val(
                ncFileID, geoCRS, strAttsToComp[attNum], strAttVal, LogInfo
            );
            if (LogInfo->stopRun) {
                goto wrapUp; // Exit function prematurely due to error
            }

            if (strcmp(geoStrAttVals[attNum], strAttVal) != 0) {
                LogError(
                    LogInfo,
                    LOGERROR,
                    attFailMsg,
                    strAttsToComp[attNum],
                    geoCRS,
                    fileName
                );
                goto wrapUp; // Exit function prematurely due to error
            }
        }

        for (attNum = 0; attNum < numNormAtts; attNum++) {
            get_double_att_val(
                ncFileID,
                geoCRS,
                doubleAttsToComp[attNum],
                &doubleAttVal,
                LogInfo
            );
            if (LogInfo->stopRun) {
                goto wrapUp; // Exit function prematurely due to error
            }

            if (doubleAttVal != geoDoubleAttVals[attNum]) {
                LogError(
                    LogInfo,
                    LOGERROR,
                    attFailMsg,
                    doubleAttsToComp[attNum],
                    geoCRS,
                    fileName
                );
                goto wrapUp; // Exit function prematurely due to error
            }
        }
    } else {
        LogError(
            LogInfo,
            LOGERROR,
            "A geographic CRS was not found in %s. "
            "Please make sure one is provided.",
            fileName
        );
        goto wrapUp; // Exit function prematurely due to error
    }

    /*
       Test all projected CRS attributes - both string and double values -
       if applicable
    */
    if (!geoIsPrimCRS && projCRSExists) {
        // Normal attributes (same tested for in crs_geogsc)
        for (attNum = 0; attNum < numNormAtts; attNum++) {
            SW_NC_get_str_att_val(
                ncFileID, projCRS, strAttsToComp[attNum], strAttVal, LogInfo
            );
            if (LogInfo->stopRun) {
                goto wrapUp; // Exit function prematurely due to error
            }

            if (strcmp(projStrAttVals[attNum], strAttVal) != 0) {
                LogError(
                    LogInfo,
                    LOGERROR,
                    attFailMsg,
                    strAttsToComp[attNum],
                    projCRS,
                    fileName
                );
                goto wrapUp; // Exit function prematurely due to error
            }
        }

        for (attNum = 0; attNum < numNormAtts; attNum++) {
            get_double_att_val(
                ncFileID,
                projCRS,
                doubleAttsToComp[attNum],
                &doubleAttVal,
                LogInfo
            );
            if (LogInfo->stopRun) {
                goto wrapUp; // Exit function prematurely due to error
            }

            if (doubleAttVal != projDoubleAttVals[attNum]) {
                LogError(
                    LogInfo,
                    LOGERROR,
                    attFailMsg,
                    doubleAttsToComp[attNum],
                    projCRS,
                    fileName
                );
                goto wrapUp; // Exit function prematurely due to error
            }
        }

        // Projected CRS-only attributes
        for (attNum = 0; attNum < numProjStrAtts; attNum++) {
            SW_NC_get_str_att_val(
                ncFileID, projCRS, strProjAttsToComp[attNum], strAttVal, LogInfo
            );
            if (LogInfo->stopRun) {
                goto wrapUp; // Exit function prematurely due to error
            }

            if (strcmp(strAttVal, strProjAttVals[attNum]) != 0) {
                LogError(
                    LogInfo,
                    LOGERROR,
                    attFailMsg,
                    strProjAttsToComp[attNum],
                    projCRS,
                    fileName
                );
                goto wrapUp; // Exit function prematurely due to error
            }
        }

        for (attNum = 0; attNum < numProjDoubleAtts; attNum++) {
            get_double_att_val(
                ncFileID,
                projCRS,
                doubleProjAttsToComp[attNum],
                &doubleAttVal,
                LogInfo
            );
            if (LogInfo->stopRun) {
                goto wrapUp; // Exit function prematurely due to error
            }

            if (doubleAttVal != doubleProjAttVals[attNum]) {
                LogError(
                    LogInfo,
                    LOGERROR,
                    attFailMsg,
                    doubleProjAttsToComp[attNum],
                    projCRS,
                    fileName
                );
                goto wrapUp; // Exit function prematurely due to error
            }
        }

        // Test for standard_parallel
        get_double_att_val(
            ncFileID, projCRS, stdParallel, projStdParallel, LogInfo
        );
        if (LogInfo->stopRun) {
            goto wrapUp; // Exit function prematurely due to error
        }

        if (projStdParallel[0] != stdParVals[0] ||
            projStdParallel[1] != stdParVals[1]) {

            LogError(
                LogInfo, LOGERROR, attFailMsg, stdParallel, projCRS, fileName
            );
            goto wrapUp; // Exit function prematurely due to error
        }
    }


wrapUp:
    if (fileWasClosed) {
        nc_close(ncFileID);
    }
}

void SW_NC_get_single_val(
    int ncFileID,
    int *varID,
    const char *varName,
    size_t index[],
    void *value,
    const char *type,
    LOG_INFO *LogInfo
) {

    if (*varID < 0 && varName != NULL) {
        SW_NC_get_var_identifier(ncFileID, varName, varID, LogInfo);
        if (LogInfo->stopRun) {
            return; // Exit function prematurely due to error
        }
    }

    if (nc_get_var1(ncFileID, *varID, index, value) != NC_NOERR) {
        LogError(
            LogInfo,
            LOGERROR,
            "An error occurred when trying to "
            "read a value from a variable of type %s.",
            type
        );
    }
}

void SW_NC_write_att(
    const char *attName,
    void *attVal,
    int varID,
    int ncFileID,
    size_t numVals,
    int ncType,
    LOG_INFO *LogInfo
) {
    if (nc_put_att(
            ncFileID, varID, attName, (nc_type) ncType, numVals, attVal
        ) != NC_NOERR) {
        LogError(
            LogInfo, LOGERROR, "Could not create new attribute %s.", attName
        );
    }
}

/**
@brief Write a global attribute (text) to a netCDF file

@param[in] attName Name of the attribute to create
@param[in] attStr Attribute string to write out
@param[in] varID Identifier of the variable to add the attribute to
    (Note: NC_GLOBAL is acceptable and is a global attribute of the netCDF file)
@param[in] ncFileID Identifier of the open netCDF file to write the attribute
@param[out] LogInfo Holds information on warnings and errors
*/
void SW_NC_write_string_att(
    const char *attName,
    const char *attStr,
    int varID,
    int ncFileID,
    LOG_INFO *LogInfo
) {
    if (nc_put_att_text(ncFileID, varID, attName, strlen(attStr), attStr) !=
        NC_NOERR) {
        LogError(
            LogInfo,
            LOGERROR,
            "Could not create new global attribute %s",
            attName
        );
    }
}

/**
@brief Write values to a variable of type string

@param[in] ncFileID Identifier of the open netCDF file to write the attribute
@param[in] varID Variable identifier within the given netCDF
@param[in] varVals Attribute value(s) to write out
@param[out] LogInfo Holds information on warnings and errors
*/
void SW_NC_write_string_vals(
    int ncFileID, int varID, const char *const varVals[], LOG_INFO *LogInfo
) {

    if (nc_put_var_string(ncFileID, varID, (const char **) &varVals[0]) !=
        NC_NOERR) {
        LogError(LogInfo, LOGERROR, "Could not write string values.");
    }
}

/**
@brief Checks to see if a given netCDF has a specific dimension

@param[in] targetDim Dimension name to test for
@param[in] ncFileID Identifier of the open netCDF file to test

@return Whether or not the given dimension name exists in the netCDF file
*/
Bool SW_NC_dimExists(const char *targetDim, int ncFileID) {

    int dimID; // Not used

    // Attempt to get the dimension identifier
    int inquireRes = nc_inq_dimid(ncFileID, targetDim, &dimID);

    return (Bool) (inquireRes == NC_NOERR);
}

/**
@brief Check if a variable exists within a given netCDF and does not
throw an error if anything goes wrong

@param[in] ncFileID Identifier of the open netCDF file to test
@param[in] varName Name of the variable to test for

@return Whether or not the given variable name exists in the netCDF file
*/
Bool SW_NC_varExists(int ncFileID, const char *varName) {
    int varID;

    int inquireRes = nc_inq_varid(ncFileID, varName, &varID);

    return (Bool) (inquireRes != NC_ENOTVAR);
}

/*
@brief Write a dummy value to a newly created netCDF file so that
the first write does not occur during the first simulation run;
this function should only be called when there is no deflate
activated

@param[in] ncFileID Identifier of the open netCDF file to write to
@param[in] varType Type of the first variable being written to the file
@param[in] varID Identifier of the variable to write to
*/
static void writeDummyVal(int ncFileID, int varType, int varID) {
    size_t start[MAX_NUM_DIMS] = {0};
    size_t count[MAX_NUM_DIMS] = {1, 1, 1, 1, 1};
    double doubleFill[] = {NC_FILL_DOUBLE};
    unsigned char byteFill[] = {(unsigned char) NC_FILL_BYTE};

    switch (varType) {
    case NC_DOUBLE:
        nc_put_vara_double(ncFileID, varID, start, count, &doubleFill[0]);
        break;
    case NC_BYTE:
        nc_put_vara_ubyte(ncFileID, varID, start, count, &byteFill[0]);
        break;
    default:
        /* No other types should be expected */
        break;
    }
}

/**
@brief Write values to the given netCDF file of any type

@param[in,out] varID Identifier corresponding to varName.
    If negative, then will be queried using varName and returned.
@param[in] ncFileID Identifier of the open netCDF file
@param[in] varName Name of the variable to write;
    not used, unless varID is negative.
@param[in] values Value(s) to write out
@param[in] start Starting indices to write to for the given variable
@param[in] count Number of values to write out
@param[in] type Intended variable type that is being written to
@param[out] LogInfo Holds information on warnings and errors
*/
void SW_NC_write_vals(
    int *varID,
    int ncFileID,
    const char *varName,
    void *values,
    size_t start[],
    size_t count[],
    const char *type,
    LOG_INFO *LogInfo
) {

    if (*varID < 0 && varName != NULL) {
        SW_NC_get_var_identifier(ncFileID, varName, varID, LogInfo);
        if (LogInfo->stopRun) {
            return; // Exit function prematurely due to error
        }
    }

    if (nc_put_vara(ncFileID, *varID, start, count, values) != NC_NOERR) {
        LogError(
            LogInfo,
            LOGERROR,
            "Could not write values of type %s to a given variable.",
            type
        );
    }
}

/**
@brief Get a string value from an attribute

@param[in] ncFileID Identifier of the open netCDF file to test
@param[in] varName Name of the variable to access
@param[in] attName Name of the attribute to access
@param[out] strVal String buffer to hold the resulting value
@param[out] LogInfo Holds information on warnings and errors
*/
void SW_NC_get_str_att_val(
    int ncFileID,
    const char *varName,
    const char *attName,
    char *strVal,
    LOG_INFO *LogInfo
) {

    int varID = 0;
    int attCallRes;
    int attLenCallRes;
    size_t attLen = 0;

    SW_NC_get_var_identifier(ncFileID, varName, &varID, LogInfo);
    if (LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

    attLenCallRes = nc_inq_attlen(ncFileID, varID, attName, &attLen);
    if (attLenCallRes == NC_ENOTATT) {
        LogError(
            LogInfo,
            LOGERROR,
            "Could not find an attribute %s "
            "for the variable %s.",
            attName,
            varName
        );
    } else if (attLenCallRes != NC_NOERR) {
        LogError(
            LogInfo,
            LOGERROR,
            "An error occurred when attempting to "
            "get the length of the value of the "
            "attribute %s.",
            attName
        );
    }

    if (LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }


    attCallRes = nc_get_att_text(ncFileID, varID, attName, strVal);
    if (attCallRes != NC_NOERR) {
        LogError(
            LogInfo,
            LOGERROR,
            "An error occurred when attempting to "
            "access the attribute %s of variable %s.",
            attName,
            varName
        );
        return; // Exit function prematurely due to error
    }

    strVal[attLen] = '\0';
}

/**
@brief Create a dimension within a netCDF file

@param[in] dimName Name of the new dimension
@param[in] size Value/size of the dimension
@param[in] ncFileID Domain netCDF file ID
@param[in,out] dimID Dimension ID
@param[out] LogInfo Holds information on warnings and errors
*/
void SW_NC_create_netCDF_dim(
    const char *dimName,
    unsigned long size,
    const int *ncFileID,
    int *dimID,
    LOG_INFO *LogInfo
) {

    if (nc_def_dim(*ncFileID, dimName, size, dimID) != NC_NOERR) {
        LogError(
            LogInfo,
            LOGERROR,
            "Could not create dimension '%s' in "
            "netCDF.",
            dimName
        );
    }
}

/**
@brief Get a variable identifier within a given netCDF

@param[in] ncFileID Identifier of the open netCDF file to access
@param[in] varName Name of the new variable
@param[out] varID Identifier of the variable
@param[out] LogInfo Holds information on warnings and errors
*/
void SW_NC_get_var_identifier(
    int ncFileID, const char *varName, int *varID, LOG_INFO *LogInfo
) {

    int callRes = nc_inq_varid(ncFileID, varName, varID);

    if (callRes == NC_ENOTVAR) {
        LogError(LogInfo, LOGERROR, "Could not find variable %s.", varName);
    }
}

/**
@brief Get a dimension value from a given netCDF file

@param[in] ncFileID Identifier of the open netCDF file to access
@param[in] dimName Name of the dimension to access
@param[out] dimVal String buffer to hold the resulting value
@param[out] LogInfo Holds information on warnings and errors
*/
void SW_NC_get_dimlen_from_dimname(
    int ncFileID, const char *dimName, size_t *dimVal, LOG_INFO *LogInfo
) {

    int dimID = 0;

    SW_NC_get_dim_identifier(ncFileID, dimName, &dimID, LogInfo);
    if (LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

    SW_NC_get_dimlen_from_dimid(ncFileID, dimID, dimVal, LogInfo);
}

/**
@brief Create a new variable by calculating the dimensions
and writing attributes

@param[in] ncFileID Identifier of the netCDF file
@param[in] domType Type of domain in which simulations are running
    (gridcell/sites)
@param[in] newVarType Type of the variable to create
@param[in] timeSize Size of "time" dimension
@param[in] vertSize Size of "vertical" dimension
@param[in] pftSize Size of "pft" dimension
@param[in] varName Name of variable to write
@param[in] attNames Attribute names that the new variable will contain
@param[in] attVals Attribute values that the new variable will contain
@param[in] numAtts Number of attributes being sent in
@param[in] hasConsistentSoilLayerDepths Flag indicating if all simulation
    run within domain have identical soil layer depths
    (though potentially variable number of soil layers)
@param[in] lyrDepths Depths of soil layers (cm)
@param[in,out] startTime Start number of days when dealing with
    years between netCDF files (returns updated value)
@param[in] baseCalendarYear First year of the entire simulation
@param[in] startYr Start year of the simulation
@param[in] pd Current output netCDF period
@param[in] deflateLevel Level of deflation that will be used for the created
variable
@param[in] latName User-provided latitude name
@param[in] lonName User-provided longitude name
@param[in] coordAttIndex Specifies the coordinate attribute location
within the provided `attNames`/`attVals` (if there isn't an attribute
of this name, it's value should be -1)
@param[in] siteName User-provided site dimension/variable "site" name
@param[in,out] LogInfo Holds information dealing with logfile output
*/
void SW_NC_create_full_var(
    int *ncFileID,
    const char *domType,
    int newVarType,
    size_t timeSize,
    size_t vertSize,
    size_t pftSize,
    const char *varName,
    const char *attNames[],
    const char *attVals[],
    unsigned int numAtts,
    Bool hasConsistentSoilLayerDepths,
    double lyrDepths[],
    double *startTime,
    unsigned int baseCalendarYear,
    unsigned int startYr,
    OutPeriod pd,
    int deflateLevel,
    const char *latName,
    const char *lonName,
    const char *siteName,
    const int coordAttIndex,
    LOG_INFO *LogInfo
) {

    int dimArrSize = 0;
    int varID = 0;
    unsigned int index;
    int dimIDs[MAX_NUM_DIMS];
    Bool domTypeIsSites = (Bool) (strcmp(domType, "s") == 0);
    unsigned int numConstDims = (domTypeIsSites) ? 1 : 2;
    const char *thirdDim = (domTypeIsSites) ? siteName : latName;
    const char *constDimNames[] = {thirdDim, lonName};
    const char *timeVertVegNames[] = {"time", "vertical", "pft"};
    char *dimVarName;
    size_t timeVertVegVals[] = {timeSize, vertSize, pftSize};
    unsigned int numTimeVertVegVals = 3;
    unsigned int varVal = 0;
    size_t chunkSizes[MAX_NUM_DIMS] = {1, 1, 1, 1, 1};
    char coordValBuf[MAX_FILENAMESIZE] = "";
    char *writePtr = coordValBuf;
    char *tempCoordPtr;
    int writeSize = MAX_FILENAMESIZE;
    char finalCoordVal[MAX_FILENAMESIZE];

    for (index = 0; index < numConstDims; index++) {
        SW_NC_get_dim_identifier(
            *ncFileID, constDimNames[index], &dimIDs[dimArrSize], LogInfo
        );
        if (LogInfo->stopRun) {
            return; // Exit function prematurely due to error
        }

        dimArrSize++;
    }

    for (index = 0; index < numTimeVertVegVals; index++) {
        dimVarName = (char *) timeVertVegNames[index];
        varVal = timeVertVegVals[index];
        if (varVal > 0) {
            if (!SW_NC_dimExists(dimVarName, *ncFileID)) {
                SW_NCOUT_create_output_dimVar(
                    dimVarName,
                    varVal,
                    *ncFileID,
                    &dimIDs[dimArrSize],
                    hasConsistentSoilLayerDepths,
                    lyrDepths,
                    startTime,
                    baseCalendarYear,
                    startYr,
                    pd,
                    deflateLevel,
                    LogInfo
                );
            } else {
                SW_NC_get_dim_identifier(
                    *ncFileID, dimVarName, &dimIDs[dimArrSize], LogInfo
                );
            }
            if (LogInfo->stopRun) {
                return; // Exit function prematurely due to error
            }

            tempCoordPtr = (char *) sw_memccpy(writePtr, " ", '\0', writeSize);
            writeSize--;
            writePtr = tempCoordPtr - 1;

            tempCoordPtr = (char *) sw_memccpy(
                writePtr, (char *) timeVertVegNames[index], '\0', writeSize
            );
            writeSize -= (int) (tempCoordPtr - coordValBuf - 1);
            writePtr = tempCoordPtr - 1;

            dimArrSize++;
        }
    }

    if (coordAttIndex > -1 && !isnull(attVals[coordAttIndex])) {
        snprintf(
            finalCoordVal,
            MAX_FILENAMESIZE,
            "%s%s",
            attVals[coordAttIndex],
            coordValBuf
        );

        free((void *) ((char *) attVals[coordAttIndex]));
        attVals[coordAttIndex] = Str_Dup(finalCoordVal, LogInfo);
        if (LogInfo->stopRun) {
            return;
        }
    }

    for (index = numConstDims; index < MAX_NUM_DIMS; index++) {
        if (index - numConstDims < 3) {
            varVal = timeVertVegVals[index - numConstDims];

            if (varVal > 0) {
                chunkSizes[index] = varVal;
            }
        }
    }

    SW_NC_create_netCDF_var(
        &varID,
        varName,
        dimIDs,
        ncFileID,
        newVarType,
        dimArrSize,
        chunkSizes,
        deflateLevel,
        LogInfo
    );
    if (LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

    for (index = 0; index < numAtts; index++) {
        SW_NC_write_string_att(
            attNames[index], attVals[index], varID, *ncFileID, LogInfo
        );
        if (LogInfo->stopRun) {
            return; // Exit function prematurely due to error
        }
    }

    if (deflateLevel == 0) {
        /* Write a dummy value so that the first write is not in the sim loop;
           otherwise, the first simulation loop takes an order of magnitude
           longer than following simulations */
        writeDummyVal(*ncFileID, newVarType, varID);
    }
}

/**
@brief Copy domain netCDF as a template

@param[in] domType Type of domain in which simulations are running
    (gridcell/sites)
@param[in] domFile Name of the domain netCDF
@param[in] fileName Name of the netCDF file to create
@param[in] newFileID Identifier of the netCDF file to create
@param[in] isInput Specifies if the created file will be input or output
@param[in] freq Value of the global attribute "frequency"
@param[out] LogInfo  Holds information dealing with logfile output
*/
void SW_NC_create_template(
    const char *domType,
    const char *domFile,
    const char *fileName,
    int *newFileID,
    Bool isInput,
    const char *freq,
    LOG_INFO *LogInfo
) {


    CopyFile(domFile, fileName, LogInfo);
    if (LogInfo->stopRun) {
        return; // Exit function prematurely due to error
    }

    SW_NC_open(fileName, NC_WRITE, newFileID, LogInfo);
    if (LogInfo->stopRun) {
        return; /* Exit function prematurely due to error */
    }

    update_netCDF_global_atts(newFileID, domType, freq, isInput, LogInfo);
}

/**
@brief Create a variable within a netCDF file

@param[out] varID Variable ID within the netCDF
@param[in] varName Name of the new variable
@param[in] dimIDs Dimensions of the variable
@param[in] ncFileID Domain netCDF file ID
@param[in] varType The type in which the new variable will be
@param[in] numDims Number of dimensions the new variable will hold
@param[in] chunkSizes Custom chunk sizes for the variable being created
@param[in] deflateLevel Level of deflation that will be used for the created
variable
@param[out] LogInfo Holds information on warnings and errors
*/
void SW_NC_create_netCDF_var(
    int *varID,
    const char *varName,
    int *dimIDs,
    const int *ncFileID,
    int varType,
    int numDims,
    size_t chunkSizes[],
    int deflateLevel,
    LOG_INFO *LogInfo
) {

    // Deflate information
    int shuffle = 1; // 0 or 1
    int deflate = 1; // 0 or 1

    if (nc_def_var(*ncFileID, varName, varType, numDims, dimIDs, varID) !=
        NC_NOERR) {
        LogError(
            LogInfo,
            LOGERROR,
            "Could not create '%s' variable in "
            "netCDF.",
            varName
        );
        return; // Exit prematurely due to error
    }

    if (!isnull(chunkSizes)) {
        if (nc_def_var_chunking(*ncFileID, *varID, NC_CHUNKED, chunkSizes) !=
            NC_NOERR) {

            LogError(
                LogInfo,
                LOGERROR,
                "Could not chunk variable '%s' when creating it in "
                "output netCDF.",
                varName
            );
            return; // Exit prematurely due to error
        }
    }

    // Do not compress the CRS variables
    if (deflateLevel > 0 && strcmp(varName, "crs_geogsc") != 0 &&
        strcmp(varName, "crs_projsc") != 0 && varType != NC_STRING) {

        if (nc_def_var_deflate(
                *ncFileID, *varID, shuffle, deflate, deflateLevel
            ) != NC_NOERR) {
            LogError(
                LogInfo,
                LOGERROR,
                "An error occurred when attempting to "
                "deflate the variable %s",
                varName
            );
        }
    }
}

/**
@brief Deconstruct netCDF-related information

@param[in,out] SW_netCDFOut Constant netCDF output file information
*/
void SW_NC_deconstruct(SW_NETCDF_OUT *SW_netCDFOut) {
    SW_NCOUT_deconstruct(SW_netCDFOut);
}

/**
@brief Deep copy a source of input/output netCDF information

@param[in] source_output Source output netCDF information to copy
@param[in] source_input Source input netCDF information to copy
@param[out] dest_output Destination output netCDF information to be copied
into from it's source counterpart
@param[out] dest_input Destination input netCDF information to be copied
into from it's source counterpart
@param[out] LogInfo Holds information on warnings and errors
*/
void SW_NC_deepCopy(
    SW_NETCDF_OUT *source_output,
    SW_NETCDF_IN *source_input,
    SW_NETCDF_OUT *dest_output,
    SW_NETCDF_IN *dest_input,
    LOG_INFO *LogInfo
) {

    memcpy(dest_output, source_output, sizeof(*dest_output));
    memcpy(dest_input, source_input, sizeof(*dest_input));

    SW_NCOUT_init_ptrs(dest_output);
    SW_NCIN_init_ptrs(dest_input);

    SW_NCOUT_deepCopy(source_output, dest_output, LogInfo);
    if (LogInfo->stopRun) {
        return; /* Exit function prematurely due to error */
    }

    SW_NCIN_deepCopy(source_input, dest_input, LogInfo);
}

/**
@brief Read input files for netCDF related actions

@param[in,out] SW_netCDFIn Constant netCDF input file information
@param[in,out] SW_netCDFOut Constant netCDF output file information
@param[in,out] SW_PathInputs Struct holding all information about the programs
    path/files
@param[in] startYr Start year of the simulation
@param[in] endYr End year of the simulation
@param[out] LogInfo Holds information on warnings and errors
*/
void SW_NC_read(
    SW_NETCDF_IN *SW_netCDFIn,
    SW_NETCDF_OUT *SW_netCDFOut,
    SW_PATH_INPUTS *SW_PathInputs,
    TimeInt startYr,
    TimeInt endYr,
    LOG_INFO *LogInfo
) {
    // Read CRS and attributes for netCDFs
    SW_NCOUT_read_atts(SW_netCDFOut, SW_PathInputs, LogInfo);
    if (LogInfo->stopRun) {
        return; /* Exit function prematurely due to error */
    }

    SW_NCIN_read_input_vars(
        SW_netCDFIn, SW_netCDFOut, SW_PathInputs, startYr, endYr, LogInfo
    );
}

/**
@brief Allocate memory for internal SOILWAT2 units

@param[out] units_sw Array of text representations of SOILWAT2 units
@param[in] nVar Number of variables available for current output key
@param[out] LogInfo Holds information on warnings and errors
*/
void SW_NC_alloc_unitssw(char ***units_sw, int nVar, LOG_INFO *LogInfo) {

    *units_sw = NULL;

    if (nVar > 0) {

        // Initialize the variable within SW_OUT_DOM
        *units_sw = (char **) Mem_Malloc(
            sizeof(char *) * nVar, "SW_NC_alloc_unitssw()", LogInfo
        );
        if (LogInfo->stopRun) {
            return; // Exit function prematurely due to error
        }

        for (int index = 0; index < nVar; index++) {
            (*units_sw)[index] = NULL;
        }
    }
}

/**
@brief Allocate memory for udunits2 unit converter

@param[out] uconv Array of pointers to udunits2 unit converter
@param[in] nVar Number of variables available for current output key
@param[out] LogInfo Holds information on warnings and errors
*/
void SW_NC_alloc_uconv(sw_converter_t ***uconv, int nVar, LOG_INFO *LogInfo) {

    *uconv = NULL;

    if (nVar > 0) {

        *uconv = (sw_converter_t **) Mem_Malloc(
            sizeof(sw_converter_t *) * nVar, "SW_NC_alloc_uconv()", LogInfo
        );
        if (LogInfo->stopRun) {
            return; // Exit function prematurely due to error
        }

        for (int index = 0; index < nVar; index++) {
            (*uconv)[index] = NULL;
        }
    }
}

/**
@brief Allocate information about whether or not a variable should be
output/input

@param[out] reqOutVar Specifies the number of variables that can be output
    for a given output key
@param[in] nVar Number of variables available for current output key
@param[out] LogInfo Holds information on warnings and errors
*/
void SW_NC_alloc_req(Bool **reqOutVar, int nVar, LOG_INFO *LogInfo) {

    *reqOutVar = NULL;

    if (nVar > 0) {

        // Initialize the variable within SW_OUT_DOM which specifies if a
        // variable is to be written out or not
        *reqOutVar = (Bool *) Mem_Malloc(
            sizeof(Bool) * nVar, "SW_NC_alloc_outReq()", LogInfo
        );
        if (LogInfo->stopRun) {
            return; // Exit function prematurely due to error
        }

        for (int index = 0; index < nVar; index++) {
            (*reqOutVar)[index] = swFALSE;
        }
    }
}

/**
@brief Allocate memory for information in regards to output/input variables

@param[out] keyVars Holds all information about output variables
    in netCDFs (e.g., output variable name)
@param[in] nVar Number of variables available for current output key
@param[in] numAtts Number of attributes that the variable(s) will contain
@param[out] LogInfo Holds information on warnings and errors
*/
void SW_NC_alloc_vars(
    char ****keyVars, int nVar, int numAtts, LOG_INFO *LogInfo
) {

    *keyVars = NULL;

    if (nVar > 0) {

        int index;
        int varNum;
        int attNum;

        // Allocate all memory for the variable information in the current
        // output key
        *keyVars = (char ***) Mem_Malloc(
            sizeof(char **) * nVar, "SW_NC_alloc_vars()", LogInfo
        );
        if (LogInfo->stopRun) {
            return; // Exit function prematurely due to error
        }

        for (index = 0; index < nVar; index++) {
            (*keyVars)[index] = NULL;
        }

        for (index = 0; index < nVar; index++) {
            (*keyVars)[index] = (char **) Mem_Malloc(
                sizeof(char *) * numAtts, "SW_NC_alloc_vars()", LogInfo
            );
            if (LogInfo->stopRun) {
                for (varNum = 0; varNum < index; varNum++) {
                    free((void *) (*keyVars)[varNum]);
                    (*keyVars)[varNum] = NULL;
                }
                free((void *) *keyVars);
                return; // Exit function prematurely due to error
            }

            for (attNum = 0; attNum < numAtts; attNum++) {
                (*keyVars)[index][attNum] = NULL;
            }
        }
    }
}

/**
@brief Generalized function to get values of any type from a netCDF
files

@param[in] ncFileID Identifier of the open netCDF file to test
@param[in] varID Variable identifier within the given netCDF
@param[in] varName Name of the variable to access
@param[out] values Value(s) to write in
@param[out] LogInfo Holds information on warnings and errors
*/
void SW_NC_get_vals(
    int ncFileID,
    int *varID,
    const char *varName,
    void *values,
    LOG_INFO *LogInfo
) {
    if (*varID < 0 && !isnull(varName)) {
        SW_NC_get_var_identifier(ncFileID, varName, varID, LogInfo);
        if (LogInfo->stopRun) {
            return; /* Exit function prematurely due to unknown variable */
        }
    }

    if (nc_get_var(ncFileID, *varID, values) != NC_NOERR) {
        LogError(
            LogInfo,
            LOGERROR,
            "Could not read the values of variable '%s'.",
            varName
        );
    }
}

void SW_NC_open(
    const char *ncFileName, int openMode, int *fileID, LOG_INFO *LogInfo
) {
    if (nc_open(ncFileName, openMode, fileID) != NC_NOERR) {
        LogError(LogInfo, LOGERROR, "Could not open file '%s'.", ncFileName);
    }
}
