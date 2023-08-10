#ifndef SWNETCDF_H
#define SWNETCDF_H

#include "include/SW_datastructs.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =================================================== */
/*                  Local Definitions                  */
/* --------------------------------------------------- */

#define DOMAIN_NC 0 // Domain index

// Open netCDF identifer - this should always be 65536 because
// the program will always have at most one netCDF open
#define OPEN_NC_ID 65536

// Constants for number of dimensions
#define ZERODIMS 0
#define ONEDIM 1
#define TWODIMS 2
#define THREEDIMS 3

// Constant sizes
#define NUMBNDS 2

#define SIZEONE 1
#define SIZETWO 2

void SW_NC_read_files(PATH_INFO* PathInfo, LOG_INFO* LogInfo);
int nc_key_to_ID(char* key);
void SW_NC_create_domain(SW_DOMAIN* SW_Domain, char* DomainName,
                         LOG_INFO* LogInfo);
void create_dim_xy(int nDimX, int nDimY, int* yDimID, int* xDimID,
                   int* bndsDimID, LOG_INFO* LogInfo);
void create_dim_s(int nDimS, int* sDimID, LOG_INFO* LogInfo);
void create_xy_vars(int nDimX, int nDimY, int yDimID, int xDimID,
                    int bndsDimID, LOG_INFO* LogInfo);
void create_s_vars(int nDimS, int sDimID, LOG_INFO* LogInfo);
void create_var_x(int* xID, int xDim[], LOG_INFO* LogInfo);
void create_var_y(int* yID, int yDim[], LOG_INFO* LogInfo);
void create_var_domain(int* domID, int domDim[], int numDims,
        size_t chunkSize, unsigned int chunkVals[], LOG_INFO* LogInfo);
void create_var_crs(LOG_INFO* LogInfo);
void fill_xy_vars(int nDimX, int nDimY, int domainID,
        int xID, int yID, int x_bndsID, int y_bndsID);
void fill_s_vars(int nDimS, int siteID, int domainID, int xID, int yID);
void write_global_domain_atts(LOG_INFO* LogInfo);
void create_dimension(char* dimName, int* dimID, int size, LOG_INFO* LogInfo);
void create_variable(char* varName, int numDims, int dims[], int varType,
                     int* varID, LOG_INFO* LogInfo);
void write_att_str(char *attName, char *attValue, int varID, LOG_INFO* LogInfo);
void write_att_val(char* attName, double attValue[], int attSize,
                   int varID, LOG_INFO* LogInfo);

#ifdef __cplusplus
}
#endif

#endif // SWNETCDF_H
