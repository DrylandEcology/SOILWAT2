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

#include "include/SW_datastructs.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =================================================== */
/*            Externed Global Variables                */
/* --------------------------------------------------- */
extern Bool print_IterationSummary;
extern Bool print_SW_Output;
extern char sw_outstr[MAX_LAYERS * OUTSTRLEN];

#ifdef STEPWAT
  extern char sw_outstr_agg[MAX_LAYERS * OUTSTRLEN];
#endif


/* =================================================== */
/*             Global Function Declarations            */
/* --------------------------------------------------- */
#if defined(SOILWAT)
void SW_OUT_create_files(SW_FILE_STATUS* SW_FileStatus,
                         SW_OUTPUT* SW_Output, LyrIndex n_layers);

#elif defined(STEPWAT)
void SW_OUT_create_summary_files(void);
void SW_OUT_create_iteration_files(int iteration);
#endif

void get_outstrleader(OutPeriod pd, size_t sizeof_str,
					            SW_MODEL* SW_Model, char *str);
void write_headers_to_csv(OutPeriod pd, FILE *fp_reg, FILE *fp_soil,
	Bool does_agg, Bool make_regular[], Bool make_soil[], SW_OUTPUT* SW_Output,
	LyrIndex n_layers);
void find_TXToutputSoilReg_inUse(Bool make_soil[], Bool make_regular[],
								                 SW_OUTPUT* SW_Output);
void SW_OUT_close_files(SW_FILE_STATUS* SW_FileStatus);


#ifdef __cplusplus
}
#endif

#endif
