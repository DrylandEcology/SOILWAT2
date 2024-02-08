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
/*             Global Function Declarations            */
/* --------------------------------------------------- */
#if defined(SOILWAT)
void SW_OUT_create_textfiles(SW_FILE_STATUS* SW_FileStatus, SW_OUTPUT* SW_Output,
	LyrIndex n_layers, char *InFiles[], SW_GEN_OUT* GenOutput,
	LOG_INFO* LogInfo);

#elif defined(STEPWAT)
void SW_OUT_create_summary_files(SW_FILE_STATUS* SW_FileStatus,
		SW_OUTPUT* SW_Output, SW_GEN_OUT *GenOutput,
		char *InFiles[], LyrIndex n_layers, LOG_INFO* LogInfo);
void SW_OUT_create_iteration_files(SW_FILE_STATUS* SW_FileStatus,
		SW_OUTPUT* SW_Output, int iteration, SW_GEN_OUT *GenOutput,
		char *InFiles[], LyrIndex n_layers, LOG_INFO* LogInfo) ;
#endif

void get_outstrleader(OutPeriod pd, size_t sizeof_str,
			SW_MODEL* SW_Model, TimeInt tOffset, char *str);
void write_headers_to_csv(OutPeriod pd, FILE *fp_reg, FILE *fp_soil,
	Bool does_agg, Bool make_regular[], Bool make_soil[], SW_OUTPUT* SW_Output,
	LyrIndex n_layers, SW_GEN_OUT* GenOutput, LOG_INFO* LogInfo);
void find_TXToutputSoilReg_inUse(Bool make_soil[], Bool make_regular[],
		SW_OUTPUT* SW_Output, OutPeriod timeSteps[][SW_OUTNPERIODS],
		IntUS used_OUTNPERIODS);
void SW_OUT_close_textfiles(SW_FILE_STATUS* SW_FileStatus, SW_GEN_OUT* GenOutput,
						LOG_INFO* LogInfo);


#ifdef __cplusplus
}
#endif

#endif
