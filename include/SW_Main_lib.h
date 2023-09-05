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
void sw_init_args(int argc, char **argv, Bool *QuietMode,
	Bool *EchoInits, char **_firstfile, LOG_INFO* LogInfo);
void sw_print_version(void);
void sw_check_log(Bool QuietMode, LOG_INFO* LogInfo);
void sw_init_logs(FILE* logInitPtr, LOG_INFO* LogInfo);
void sw_write_logs(Bool QuietMode, LOG_INFO* LogInfo);

#ifdef __cplusplus
}
#endif

#endif
