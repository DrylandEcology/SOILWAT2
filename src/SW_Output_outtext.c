/********************************************************/
/********************************************************/
/**
  @file
  @brief Output functionality for processing of text outputs that are written
  to disk files

  See the \ref out_algo "output algorithm documentation" for details.

  History:
  2018 June 15 (drs) moved functions from `SW_Output.c`
*/
/********************************************************/
/********************************************************/


/* =================================================== */
/*                INCLUDES / DEFINES                   */
/* --------------------------------------------------- */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "include/generic.h"
#include "include/filefuncs.h"
#include "include/myMemory.h"
#include "include/Times.h"

#include "include/SW_Defines.h"
#include "include/SW_Files.h"
#include "include/SW_Model.h" // externs SW_Model
#include "include/SW_Site.h" // externs SW_Site

// externs `SW_Output`, `_Sep`, `tOffset`, `use_OutPeriod`, `used_OUTNPERIODS`,
//         `timeSteps`, `colnames_OUT`, `ncol_OUT`, `key2str`, `pd2longstr`,
//         `prepare_IterationSummary`, `storeAllIterations`
#include "include/SW_Output.h"
#include "include/SW_Output_outtext.h"



/* =================================================== */
/*                  Global Variables                   */
/* --------------------------------------------------- */
SW_FILE_STATUS SW_OutFiles;


Bool
  /** `print_IterationSummary` is TRUE if STEPWAT2 is called with `-o` flag
      and if STEPWAT2 is currently in its last iteration/repetition */
  print_IterationSummary,
  /** `print_SW_Output` is TRUE for SOILWAT2 and
      if STEPWAT2 is called with `-i` flag */
  print_SW_Output;


/** \brief Formatted output string for single run output

  Used for output as returned from any function `get_XXX_text` which are used
  for SOILWAT2-standalone and for a single iteration/repeat for STEPWAT2
*/
char sw_outstr[MAX_LAYERS * OUTSTRLEN];


/** \brief Formatted output string for aggregated output

  Defined as char `sw_outstr_agg[MAX_LAYERS * OUTSTRLEN];`

  Used for output as returned from any function `get_XXX_agg` which
  aggregates output across iterations/repeats for STEPWAT2

  Active if \ref print_IterationSummary is TRUE
*/
#define sw_outstr_agg
#undef sw_outstr_agg

#ifdef STEPWAT
char sw_outstr_agg[MAX_LAYERS * OUTSTRLEN];
#endif



/* =================================================== */
/*                  Local Variables                    */
/* --------------------------------------------------- */



/* =================================================== */
/*             Local Function Definitions              */
/* --------------------------------------------------- */

static void _create_csv_headers(OutPeriod pd, char *str_reg, char *str_soil, Bool does_agg) {
	unsigned int i;
	char key[50],
		str_help1[SW_Site.n_layers * OUTSTRLEN],
		str_help2[SW_Site.n_layers * OUTSTRLEN];
	OutKey k;

	// Initialize headers
	str_reg[0] = (char)'\0';
	str_soil[0] = (char)'\0';

	#ifdef SOILWAT
	if (does_agg) {
		LogError(logfp, LOGFATAL, "'_create_csv_headers': value TRUE for "\
			"argument 'does_agg' is not implemented for SOILWAT2-standalone.");
	}
	#endif

	ForEachOutKey(k)
	{
		if (SW_Output[k].use && has_OutPeriod_inUse(pd, k))
		{
			strcpy(key, key2str[k]);
			str_help2[0] = '\0';

			for (i = 0; i < ncol_OUT[k]; i++) {
				if (does_agg) {
						snprintf(
							str_help1,
							sizeof str_help1,
							"%c%s_%s_Mean%c%s_%s_SD",
							_Sep, key, colnames_OUT[k][i], _Sep, key, colnames_OUT[k][i]
						);
						strcat(str_help2, str_help1);
				} else {
					snprintf(str_help1, sizeof str_help1, "%c%s_%s", _Sep, key, colnames_OUT[k][i]);
					strcat(str_help2, str_help1);
				}
			}

			if (SW_Output[k].has_sl) {
				strcat((char*)str_soil, str_help2);
			} else {
				strcat((char*)str_reg, str_help2);
			}
		}
	}
}


#if defined(SOILWAT)
/**
  \brief Create `csv` output files for specified time step

  \param pd The output time step.
*/
/***********************************************************/
static void _create_csv_files(OutPeriod pd)
{
	// PROGRAMMER Note: `eOutputDaily + pd` is not very elegant and assumes
	// a specific order of `SW_FileIndex` --> fix and create something that
	// allows subsetting such as `eOutputFile[pd]` or append time period to
	// a basename, etc.

	if (SW_OutFiles.make_regular[pd]) {
		SW_OutFiles.fp_reg[pd] = OpenFile(SW_F_name(eOutputDaily + pd), "w");
	}

	if (SW_OutFiles.make_soil[pd]) {
		SW_OutFiles.fp_soil[pd] = OpenFile(SW_F_name(eOutputDaily_soil + pd), "w");
	}
}
#endif


static void get_outstrheader(OutPeriod pd, char *str, size_t sizeof_str) {
	switch (pd) {
		case eSW_Day:
			snprintf(str, sizeof_str, "%s%c%s", "Year", _Sep, pd2longstr[eSW_Day]);
			break;

		case eSW_Week:
			snprintf(str, sizeof_str, "%s%c%s", "Year", _Sep, pd2longstr[eSW_Week]);
			break;

		case eSW_Month:
			snprintf(str, sizeof_str, "%s%c%s", "Year", _Sep, pd2longstr[eSW_Month]);
			break;

		case eSW_Year:
			snprintf(str, sizeof_str, "%s", "Year");
			break;
	}
}


#if defined(STEPWAT)
/** Splits a filename such as `name.ext` into its two parts `name` and `ext`;
		appends `flag` and, if positive, `iteration` to `name` with `_` as
		separator; and returns the full name concatenated

		\return `name_flagiteration.ext`
*/
static void _create_filename_ST(char *str, char *flag, int iteration, char *filename, size_t sizeof_filename) {
	char *basename;
	char *ext;
	char *fileDup = (char *)malloc(strlen(str) + 1);

	// Determine basename and file extension
	strcpy(fileDup, str); // copy file name to new variable
	basename = strtok(fileDup, ".");
	ext = strtok(NULL, ".");

	// Put new file together
	if (iteration > 0) {
		snprintf(filename, sizeof_filename, "%s_%s%d.%s", basename, flag, iteration, ext);
	} else {
		snprintf(filename, sizeof_filename, "%s_%s.%s", basename, flag, ext);
	}

	free(fileDup);
}


/**
  \brief Creates `csv` output files for specified time step depending on
    `-o` and `-i` flags, for `STEPWAT2`.

  If `-i` flag is used, then this function creates a file for each `iteration`
  with the file name containing the value of `iteration`.

  If `-o` flag is used, then this function creates only one set of output files.

  \param iteration Current iteration value (base1) that is used for the file
    name if -i flag used in STEPWAT2. Set to a negative value otherwise.
  \param pd The output time step.
*/
/***********************************************************/
static void _create_csv_file_ST(int iteration, OutPeriod pd)
{
	char filename[FILENAME_MAX];

	if (iteration <= 0)
	{ // STEPWAT2: aggregated values over all iterations
		if (SW_OutFiles.make_regular[pd]) {
			// PROGRAMMER Note: `eOutputDaily + pd` is not very elegant and assumes
			// a specific order of `SW_FileIndex` --> fix and create something that
			// allows subsetting such as `eOutputFile[pd]` or append time period to
			// a basename, etc.
			_create_filename_ST(SW_F_name(eOutputDaily + pd), "agg", 0, filename, FILENAME_MAX);
			SW_OutFiles.fp_reg_agg[pd] = OpenFile(filename, "w");
		}

		if (SW_OutFiles.make_soil[pd]) {
			_create_filename_ST(SW_F_name(eOutputDaily_soil + pd), "agg", 0, filename, FILENAME_MAX);
			SW_OutFiles.fp_soil_agg[pd] = OpenFile(filename, "w");
		}

	} else
	{ // STEPWAT2: storing values for every iteration
		if (iteration > 1) {
			// close files from previous iteration
			if (SW_OutFiles.make_regular[pd]) {
				CloseFile(&SW_OutFiles.fp_reg[pd]);
			}
			if (SW_OutFiles.make_soil[pd]) {
				CloseFile(&SW_OutFiles.fp_soil[pd]);
			}
		}

		if (SW_OutFiles.make_regular[pd]) {
			_create_filename_ST(SW_F_name(eOutputDaily + pd), "rep", iteration, filename, FILENAME_MAX);
			SW_OutFiles.fp_reg[pd] = OpenFile(filename, "w");
		}

		if (SW_OutFiles.make_soil[pd]) {
			_create_filename_ST(SW_F_name(eOutputDaily_soil + pd), "rep", iteration, filename, FILENAME_MAX);
			SW_OutFiles.fp_soil[pd] = OpenFile(filename, "w");
		}
	}
}
#endif



/* =================================================== */
/*             Global Function Definitions             */
/* --------------------------------------------------- */




#if defined(SOILWAT)

/** @brief create all of the user-specified output files.
    @note Call this routine at the beginning of the main program run, but
    after SW_OUT_read() which sets the global variable use_OutPeriod.
*/
void SW_OUT_create_files(void) {
	OutPeriod pd;

	ForEachOutPeriod(pd) {
		if (use_OutPeriod[pd]) {
			_create_csv_files(pd);

			write_headers_to_csv(pd, SW_OutFiles.fp_reg[pd], SW_OutFiles.fp_soil[pd],
				swFALSE);
		}
	}
}


#elif defined(STEPWAT)

void SW_OUT_create_summary_files(void) {
	OutPeriod p;

	ForEachOutPeriod(p) {
		if (use_OutPeriod[p]) {
			_create_csv_file_ST(-1, p);

			write_headers_to_csv(p, SW_OutFiles.fp_reg_agg[p],
				SW_OutFiles.fp_soil_agg[p], swTRUE);
		}
	}
}

void SW_OUT_create_iteration_files(int iteration) {
	OutPeriod p;

	ForEachOutPeriod(p) {
		if (use_OutPeriod[p]) {
			_create_csv_file_ST(iteration, p);

			write_headers_to_csv(p, SW_OutFiles.fp_reg[p],
				SW_OutFiles.fp_soil[p], swFALSE);
		}
	}
}

#endif



/** Periodic output for Month and/or Week are actually
		printing for the PREVIOUS month or week.
		Also, see note on test value in _write_today() for
		explanation of the +1.
*/
void get_outstrleader(OutPeriod pd, char *str, size_t sizeof_str) {
	switch (pd) {
		case eSW_Day:
			snprintf(str, sizeof_str, "%d%c%d", SW_Model.simyear, _Sep, SW_Model.doy);
			break;

		case eSW_Week:
			snprintf(str, sizeof_str, "%d%c%d", SW_Model.simyear, _Sep,
				(SW_Model.week + 1) - tOffset);
			break;

		case eSW_Month:
			snprintf(str, sizeof_str, "%d%c%d", SW_Model.simyear, _Sep,
				(SW_Model.month + 1) - tOffset);
			break;

		case eSW_Year:
			snprintf(str, sizeof_str, "%d", SW_Model.simyear);
			break;
	}
}



/**
  \brief Creates column headers for output files

  write_headers_to_csv() is called only once for each set of output files and it goes through
	all values and if the value is defined to be used it creates the header in the output file.

	\note The functions SW_OUT_set_ncol() and SW_OUT_set_colnames() must
	  be called before _create_csv_headers(); otherwise, `ncol_OUT` and
	  `colnames_OUT` are not set.

  \param pd timeperiod so it can write headers to correct output file.
  \param fp_reg name of file.
  \param fp_soil name of file.
  \param does_agg Indicate whether output is aggregated (`-o` option) or
    for each SOILWAT2 run (`-i` option)

*/
void write_headers_to_csv(OutPeriod pd, FILE *fp_reg, FILE *fp_soil, Bool does_agg) {
	char str_time[20];
	char
		// 3500 characters required for does_agg = TRUE
		header_reg[2 * OUTSTRLEN],
		// 26500 characters required for 25 soil layers and does_agg = TRUE
		header_soil[SW_Site.n_layers * OUTSTRLEN];

	// Acquire headers
	get_outstrheader(pd, str_time, sizeof str_time);
	_create_csv_headers(pd, header_reg, header_soil, does_agg);

	// Write headers to files
	if (SW_OutFiles.make_regular[pd]) {
		fprintf(fp_reg, "%s%s\n", str_time, header_reg);
		fflush(fp_reg);
	}

	if (SW_OutFiles.make_soil[pd]) {
		fprintf(fp_soil, "%s%s\n", str_time, header_soil);
		fflush(fp_soil);
	}
}



void find_TXToutputSoilReg_inUse(void)
{
	IntUS i;
	OutKey k;

	ForEachOutPeriod(i)
	{
		SW_OutFiles.make_soil[i] = swFALSE;
		SW_OutFiles.make_regular[i] = swFALSE;
	}

	ForEachOutKey(k)
	{
		for (i = 0; i < used_OUTNPERIODS; i++)
		{
			if (timeSteps[k][i] != eSW_NoTime)
			{
				if (SW_Output[k].has_sl)
				{
					SW_OutFiles.make_soil[timeSteps[k][i]] = swTRUE;
				} else
				{
					SW_OutFiles.make_regular[timeSteps[k][i]] = swTRUE;
				}
			}
		}
	}
}


/** @brief close all of the user-specified output files.
    call this routine at the end of the program run.
*/
void SW_OUT_close_files(void) {
	Bool close_regular, close_layers, close_aggs;
	OutPeriod p;


	ForEachOutPeriod(p) {
		#if defined(SOILWAT)
		close_regular = SW_OutFiles.make_regular[p];
		close_layers = SW_OutFiles.make_soil[p];
		close_aggs = swFALSE;

		#elif defined(STEPWAT)
		close_regular = (Bool) (SW_OutFiles.make_regular[p] && storeAllIterations);
		close_layers = (Bool) (SW_OutFiles.make_soil[p] && storeAllIterations);
		close_aggs = (Bool) ((SW_OutFiles.make_regular[p] || SW_OutFiles.make_soil[p])
			&& prepare_IterationSummary);
		#endif

		if (use_OutPeriod[p]) {
			if (close_regular) {
				CloseFile(&SW_OutFiles.fp_reg[p]);
			}

			if (close_layers) {
				CloseFile(&SW_OutFiles.fp_soil[p]);
			}

			if (close_aggs) {
				#ifdef STEPWAT
				CloseFile(&SW_OutFiles.fp_reg_agg[p]);

				if (close_layers) {
					CloseFile(&SW_OutFiles.fp_soil_agg[p]);
				}
				#endif
			}
		}
	}
}
