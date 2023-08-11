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

void SW_NC_read_files(PATH_INFO* PathInfo, LOG_INFO* LogInfo);
void SW_NC_create_domain(SW_DOMAIN* SW_Domain, char* DomainName,
                         LOG_INFO* LogInfo);

#ifdef __cplusplus
}
#endif

#endif // SWNETCDF_H
