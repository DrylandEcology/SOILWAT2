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


typedef struct {
	Bool make_soil, make_regular;

	#ifdef STEPWAT
	// average/sd across iteration/repetitions
	FILE *fp_reg_agg[SW_OUTNPERIODS];
	char buf_reg_agg[SW_OUTNPERIODS][OUTSTRLEN];
	// output file for variables with values for each soil layer
	FILE *fp_soil_agg[SW_OUTNPERIODS];
	char buf_soil_agg[SW_OUTNPERIODS][OUTSTRLEN];
	#endif

	// if SOILWAT: "regular" output file
	// if STEPWAT: "regular" output file; new file for each iteration/repetition
	FILE *fp_reg[SW_OUTNPERIODS];
	char buf_reg[SW_OUTNPERIODS][OUTSTRLEN];
	// if SOILWAT: output file for variables with values for each soil layer
	// if STEPWAT: new file for each iteration/repetition of STEPWAT
	FILE *fp_soil[SW_OUTNPERIODS];
	char buf_soil[SW_OUTNPERIODS][OUTSTRLEN];

} SW_FILE_STATUS;



// Function declarations
#if defined(SOILWAT)
void _create_csv_files(OutPeriod pd);
void SW_OUT_create_files(void);

#elif defined(STEPWAT)
void _create_filename_ST(char *str, char *flag, int iteration, char *filename);
void _create_csv_file_ST(int iteration, OutPeriod pd);
void SW_OUT_create_summary_files(void);
void SW_OUT_create_iteration_files(void);
#endif

void get_outstrleader(OutPeriod pd, char *str);
void write_headers_to_csv(OutPeriod pd, FILE *fp_reg, FILE *fp_soil, Bool does_agg);
void SW_OUT_close_files(void);

#endif
