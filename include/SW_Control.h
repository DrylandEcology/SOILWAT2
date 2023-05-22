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
#include "include/SW_SoilWater.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =================================================== */
/*             Global Function Declarations            */
/* --------------------------------------------------- */
void SW_CTL_setup_model(SW_ALL* sw, SW_OUTPUT_POINTERS* SW_OutputPtrs,
                        const char *firstfile);
void SW_CTL_clear_model(Bool full_reset, SW_ALL* sw);
void SW_CTL_init_run(SW_ALL* sw);
void SW_CTL_read_inputs_from_disk(SW_ALL* sw);
void SW_CTL_main(SW_ALL* sw, SW_OUTPUT_POINTERS* SW_OutputPtrs); /* main controlling loop for SOILWAT  */
void SW_CTL_run_current_year(SW_ALL* sw, SW_OUTPUT_POINTERS* SW_OutputPtrs);

#ifdef DEBUG_MEM
void SW_CTL_SetMemoryRefs(void);
#endif


#ifdef __cplusplus
}
#endif

#endif
