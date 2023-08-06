#ifndef SWNETCDF_H
#define SWNETCDF_H

#ifdef __cplusplus
extern "C" {
#endif

#include "include/SW_datastructs.h"

/* =================================================== */
/*                  Local Definitions                  */
/* --------------------------------------------------- */

#define DOMAIN_NC 0

void SW_NC_read_files(PATH_INFO* PathInfo, LOG_INFO* LogInfo);

#ifdef __cplusplus
}
#endif

#endif // SWNETCDF_H
