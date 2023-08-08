#ifndef SWNETCDF_H
#define SWNETCDF_H

#ifdef __cplusplus
extern "C" {
#endif

#include "include/SW_datastructs.h"

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

void SW_NC_read_files(PATH_INFO* PathInfo, LOG_INFO* LogInfo);
int nc_key_to_ID(char* key);
void SW_NC_create_domain(SW_DOMAIN* SW_Domain, char* DomainName,
                         LOG_INFO* LogInfo);
void write_att_str(char *attName, char *attValue, int varID, LOG_INFO* LogInfo);
void write_att_val(char* attName, double attValue[], int attSize,
                   int varID, LOG_INFO* LogInfo);

#ifdef __cplusplus
}
#endif

#endif // SWNETCDF_H
