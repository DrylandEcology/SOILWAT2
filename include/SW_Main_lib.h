/**
@file   SW_Main_lib.h
@brief  Declares functions and variables handle main-related tasks
@date   18 November 2021
 */
#ifndef SW_MAIN_LIB_H
#define SW_MAIN_LIB_H

#include "include/generic.h"        // for Bool
#include "include/SW_datastructs.h" // for LOG_INFO
#include <stdio.h>                  // for FILE

#ifdef __cplusplus
extern "C" {
#endif


/* =================================================== */
/*             Global Function Declarations            */
/* --------------------------------------------------- */
void sw_init_args(
    int argc,
    char **argv,
    Bool *EchoInits,
    char **firstfile,
    unsigned long *userSUID,
    double *wallTimeLimit,
    Bool *renameDomainTemplateNC,
    LOG_INFO *LogInfo
);

void sw_print_version(void);

void sw_fail_on_error(LOG_INFO *LogInfo);

void sw_init_logs(FILE *logInitPtr, LOG_INFO *LogInfo);

void sw_write_warnings(const char *header, LOG_INFO *LogInfo);

void sw_wrapup_logs(LOG_INFO *LogInfo);


#ifdef __cplusplus
}
#endif

#endif
