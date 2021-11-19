/**
 * @file   SW_Main_lib.h
 * @brief  Declares functions and variables handle main-related tasks
 * @date   18 November 2021
 */
#ifndef SW_MAIN_LIB_H
#define SW_MAIN_LIB_H


#ifdef __cplusplus
extern "C" {
#endif


/* =================================================== */
/*             Global Function Declarations            */
/* --------------------------------------------------- */
void sw_init_args(int argc, char **argv);
void sw_print_version(void);


#ifdef __cplusplus
}
#endif

#endif
