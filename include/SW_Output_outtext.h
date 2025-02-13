/********************************************************/
/********************************************************/
/*  Source file: SW_Output_outtext.h
  Type: header
  Purpose: Support for SW_Output_outtext.c
  Application: SOILWAT - soilwater dynamics simulator
  Purpose: define functions to deal with text outputs; currently, used
    by SOILWAT2-standalone and STEPWAT2

  History:
  2018 June 15 (drs) moved functions from `SW_Output.c`
 */
/********************************************************/
/********************************************************/

#ifndef SW_OUTPUT_TXT_H
#define SW_OUTPUT_TXT_H

#include "include/generic.h"        // for Bool
#include "include/SW_datastructs.h" // for SW_PATH_OUTPUTS, LOG_INFO
#include "include/SW_Defines.h"     // for SW_OUTNPERIODS
#include <stdio.h>                  // for size_t

#ifdef __cplusplus
extern "C" {
#endif

/* =================================================== */
/*             Global Function Declarations            */
/* --------------------------------------------------- */
#if defined(SOILWAT)
void SW_OUT_create_textfiles(
    SW_OUT_DOM *OutDom,
    SW_PATH_OUTPUTS *SW_PathOutputs,
    LyrIndex n_layers,
    char *txtInFiles[],
    LOG_INFO *LogInfo
);

#elif defined(STEPWAT)
void SW_OUT_create_summary_files(
    SW_OUT_DOM *OutDom,
    SW_PATH_OUTPUTS *SW_PathOutputs,
    char *txtInFiles[],
    LyrIndex n_layers,
    LOG_INFO *LogInfo
);

void SW_OUT_create_iteration_files(
    SW_OUT_DOM *OutDom,
    SW_PATH_OUTPUTS *SW_PathOutputs,
    int iteration,
    char *txtInFiles[],
    LyrIndex n_layers,
    LOG_INFO *LogInfo
);
#endif

void get_outstrleader(
    OutPeriod pd,
    size_t sizeof_str,
    SW_MODEL *SW_Model,
    TimeInt tOffset,
    char *str
);

void write_headers_to_csv(
    SW_OUT_DOM *OutDom,
    OutPeriod pd,
    FILE *fp_reg,
    FILE *fp_soil,
    Bool does_agg,
    const Bool make_regular[],
    const Bool make_soil[],
    LyrIndex n_layers,
    LOG_INFO *LogInfo
);

void find_TXToutputSoilReg_inUse(
    Bool make_soil[],
    Bool make_regular[],
    const Bool has_sl[],
    OutPeriod timeSteps[][SW_OUTNPERIODS],
    IntUS used_OUTNPERIODS
);

void SW_OUT_close_textfiles(
    SW_PATH_OUTPUTS *SW_PathOutputs, SW_OUT_DOM *OutDom, LOG_INFO *LogInfo
);


#ifdef __cplusplus
}
#endif

#endif
