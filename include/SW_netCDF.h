#ifndef SWNETCDF_H
#define SWNETCDF_H

#include "include/SW_datastructs.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =================================================== */
/*             Global Function Declarations            */
/* --------------------------------------------------- */
void SW_NC_read_domain(SW_DOMAIN* SW_Domain, const char* domFileName,
                       LOG_INFO* LogInfo);
void SW_NC_check(SW_DOMAIN* SW_Domain, const char* fileName,
                 LOG_INFO* LogInfo);
void SW_NC_create_domain_template(SW_DOMAIN* SW_Domain, const char* domFileName,
                                  LOG_INFO* LogInfo);
void SW_NC_create_template(const char* fileName, unsigned long timeSize,
                           unsigned long vertSize, const char* varName,
                           char* varAttributes[], LOG_INFO* LogInfo);
void SW_NC_read_inputs(SW_ALL* sw, SW_DOMAIN* SW_Domain, unsigned long ncSUID,
                       LOG_INFO* LogInfo);
void SW_NC_check_input_files(SW_DOMAIN* SW_Domain, LOG_INFO* LogInfo);

#ifdef __cplusplus
}
#endif

#endif // SWNETCDF_H
