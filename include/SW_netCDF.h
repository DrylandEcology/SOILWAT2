#ifndef SWNETCDF_H
#define SWNETCDF_H

#include "include/SW_datastructs.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =================================================== */
/*                  Local Definitions                  */
/* --------------------------------------------------- */

#define DOMAIN_NC 0 // Domain netCDF index within `InFilesNC` (PATH_INFO) and `varNC` (SW_DOMAIN)

#define DOMAIN_TEMP "Input_nc/domain_template.nc"

/* =================================================== */
/*             Global Function Declarations            */
/* --------------------------------------------------- */
void SW_NC_read_domain(SW_DOMAIN* SW_Domain, const char* domFileName,
                       LOG_INFO* LogInfo);
void SW_NC_check(SW_DOMAIN* SW_Domain, const char* fileName,
                 LOG_INFO* LogInfo);
void SW_NC_create_domain_template(SW_DOMAIN* SW_Domain, LOG_INFO* LogInfo);
void SW_NC_create_template(const char* fileName, unsigned long timeSize,
                           unsigned long vertSize, const char* varName,
                           char* varAttributes[], LOG_INFO* LogInfo);
void SW_NC_read_inputs(SW_ALL* sw, SW_DOMAIN* SW_Domain, unsigned long ncSUID,
                       LOG_INFO* LogInfo);
void SW_NC_check_input_files(SW_DOMAIN* SW_Domain, LOG_INFO* LogInfo);
void SW_NC_read(SW_DOMAIN* SW_Domain, LOG_INFO* LogInfo);
void SW_NC_init_ptrs(char* InFilesNC[], char* varNC[]);
void SW_NC_deconstruct(char* InFilesNC[], char* varNC[]);

#ifdef __cplusplus
}
#endif

#endif // SWNETCDF_H
