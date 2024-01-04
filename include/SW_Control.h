/********************************************************/
/********************************************************/
/*  Source file: SW_Control.h
 *  Type: header
 *  Application: SOILWAT - soilwater dynamics simulator
 *  Purpose: This module controls the flow of the model.
 *           Previously this was done in main() but to
 *           combine the model with other code (eg STEPPE)
 *           there needs to be separate callable routines
 *           for initializing, model flow, and output.
 *
 *  History:
 *     (10-May-02) -- INITIAL CODING - cwb
 */
/********************************************************/
/********************************************************/

#ifndef SW_CONTROL_H
#define SW_CONTROL_H

#include "include/generic.h" // for `Bool`, `swTRUE`, `swFALSE`
#include "include/SW_datastructs.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =================================================== */
/*             Global Function Declarations            */
/* --------------------------------------------------- */
void SW_CTL_init_ptrs(SW_ALL* sw) ;
void SW_CTL_alloc_outptrs(SW_ALL* sw, LOG_INFO* LogInfo);
void SW_ALL_deepCopy(SW_ALL* source, SW_ALL* dest, LOG_INFO* LogInfo);
void SW_CTL_setup_domain(unsigned long userSUID,
                         SW_DOMAIN* SW_Domain,
                         LOG_INFO* LogInfo);
void SW_CTL_setup_model(SW_ALL* sw, SW_OUTPUT_POINTERS* SW_OutputPtrs,
                        LOG_INFO* LogInfo);
void SW_CTL_clear_model(Bool full_reset, SW_ALL* sw);
void SW_CTL_init_run(SW_ALL* sw, LOG_INFO* LogInfo);
void SW_CTL_read_inputs_from_disk(SW_ALL* sw, PATH_INFO* PathInfo,
                                  LOG_INFO* LogInfo);
void SW_CTL_main(SW_ALL* sw, SW_OUTPUT_POINTERS* SW_OutputPtrs,
                 LOG_INFO* LogInfo); /* main controlling loop for SOILWAT  */
void SW_CTL_RunSimSet(SW_ALL *sw_template, SW_OUTPUT_POINTERS SW_OutputPtrs[],
                      SW_DOMAIN *SW_Domain,
                      SW_WALLTIME *SW_WallTime,
                      LOG_INFO *main_LogInfo);
void SW_CTL_run_current_year(SW_ALL* sw, SW_OUTPUT_POINTERS* SW_OutputPtrs,
                             LOG_INFO* LogInfo);
void SW_CTL_run_sw(SW_ALL* sw_template, SW_DOMAIN* SW_Domain, unsigned long ncSuid[],
                   SW_OUTPUT_POINTERS SW_OutputPtrs[],
                   RealD p_OUT[][SW_OUTNPERIODS], LOG_INFO* LogInfo);


#ifdef __cplusplus
}
#endif

#endif
