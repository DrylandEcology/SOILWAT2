/**
 * @file   SW_Main_lib.h
 * @brief  Declares functions and variables handle main-related tasks
 * @date   18 November 2021
 */
#ifndef SW_MAIN_LIB_H
#define SW_MAIN_LIB_H

#include "include/SW_datastructs.h"

#ifdef __cplusplus
extern "C" {
#endif


/* =================================================== */
/*             Global Function Declarations            */
/* --------------------------------------------------- */
void sw_init_args(int argc, char **argv, Bool *EchoInits,
                  char **_firstfile, int *userSUID, LOG_INFO* LogInfo);
void sw_print_version(void);
void sw_fail_on_error(LOG_INFO* LogInfo);
void sw_init_logs(FILE* logInitPtr, LOG_INFO* LogInfo);
void sw_write_warnings(LOG_INFO* LogInfo);

#ifdef __cplusplus
}
#endif

#endif
