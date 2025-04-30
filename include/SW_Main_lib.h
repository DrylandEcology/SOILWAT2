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
    int rank,
    Bool *EchoInits,
    char **firstfile,
    unsigned long *userSUID,
    double *wallTimeLimit,
    Bool *renameDomainTemplateNC,
    Bool *prepareFiles,
    LOG_INFO *LogInfo
);

void sw_print_version(void);

void sw_fail_on_error(LOG_INFO *LogInfo);

void sw_init_logs(FILE *logInitPtr, LOG_INFO *LogInfo);

void sw_write_warnings(const char *header, LOG_INFO *LogInfo);

void sw_wrapup_logs(int rank, LOG_INFO *LogInfo);

void sw_setup_prog_data(
    int rank,
    int worldSize,
    char *procName,
    Bool prepareFiles,
    SW_RUN *sw_template,
    SW_DOMAIN *SW_Domain,
    LOG_INFO *LogInfo
);

void sw_finalize_program(
    int rank,
    int size,
    SW_DOMAIN *SW_Domain,
    SW_WALLTIME *SW_WallTime,
    Bool setupFailed,
    LOG_INFO *LogInfo
);

#ifdef __cplusplus
}
#endif

#endif
