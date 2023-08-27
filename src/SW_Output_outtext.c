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
#include "include/filefuncs.h"

#include "include/SW_Files.h"
#include "include/SW_Model.h"
#include "include/SW_Site.h"

// externs `key2str`, `pd2longstr`
#include "include/SW_Output.h"
#include "include/SW_Output_outtext.h"


/** \brief Formatted output string for aggregated output

  Defined as char `sw_outstr_agg[MAX_LAYERS * OUTSTRLEN];`

  Used for output as returned from any function `get_XXX_agg` which
  aggregates output across iterations/repeats for STEPWAT2

  Active if \ref SW_GEN_OUT.print_IterationSummary is TRUE
*/
#define sw_outstr_agg
#undef sw_outstr_agg



/* =================================================== */
/*                  Local Variables                    */
/* --------------------------------------------------- */



/* =================================================== */
/*             Local Function Definitions              */
/* --------------------------------------------------- */

static void _create_csv_headers(OutPeriod pd, char *str_reg, char *str_soil,
		Bool does_agg, LyrIndex n_layers, SW_OUTPUT* SW_Output,
		SW_GEN_OUT* GenOutput, LOG_INFO* LogInfo) {

	unsigned int i;
	char key[50],
		str_help1[n_layers * OUTSTRLEN],
		str_help2[n_layers * OUTSTRLEN];
	OutKey k;

	// Initialize headers
	str_reg[0] = (char)'\0';
	str_soil[0] = (char)'\0';

	#ifdef SOILWAT
		if (does_agg) {
			LogError(LogInfo, LOGFATAL, "'_create_csv_headers': value TRUE for "\
				"argument 'does_agg' is not implemented for SOILWAT2-standalone.");
		}
	#else
		(void) LogInfo;
	#endif

	ForEachOutKey(k)
	{
		if (SW_Output[k].use && has_OutPeriod_inUse(pd, k,
						GenOutput->used_OUTNPERIODS, GenOutput->timeSteps))
		{
			strcpy(key, key2str[k]);
			str_help2[0] = '\0';

			for (i = 0; i < GenOutput->ncol_OUT[k]; i++) {
				if (does_agg) {
						snprintf(
							str_help1,
							sizeof str_help1,
							"%c%s_%s_Mean%c%s_%s_SD",
							_OUTSEP, key, GenOutput->colnames_OUT[k][i], _OUTSEP,
							key, GenOutput->colnames_OUT[k][i]
						);
						strcat(str_help2, str_help1);
				} else {
					snprintf(str_help1, sizeof str_help1, "%c%s_%s", _OUTSEP,
							 key, GenOutput->colnames_OUT[k][i]);
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

  \param[in,out] SW_FileStatus SW_FileStatus Struct of type
	SW_FILE_STATUS which holds basic information about output files
	and values
  \param[in] pd The output time step.
  \param[in] InFiles Array of program in/output files
  \param[in] LogInfo Holds information dealing with logfile output
*/
/***********************************************************/
static void _create_csv_files(SW_FILE_STATUS* SW_FileStatus, OutPeriod pd,
							  char *InFiles[], LOG_INFO* LogInfo)
{
	// PROGRAMMER Note: `eOutputDaily + pd` is not very elegant and assumes
	// a specific order of `SW_FileIndex` --> fix and create something that
	// allows subsetting such as `eOutputFile[pd]` or append time period to
	// a basename, etc.

	if (SW_FileStatus->make_regular[pd]) {
		SW_FileStatus->fp_reg[pd] =
			OpenFile(InFiles[eOutputDaily + pd], "w", LogInfo);
	}

	if (SW_FileStatus->make_soil[pd]) {
		SW_FileStatus->fp_soil[pd] =
			OpenFile(InFiles[eOutputDaily_soil + pd], "w", LogInfo);
	}
}
#endif


static void get_outstrheader(OutPeriod pd, char *str, size_t sizeof_str) {
	switch (pd) {
		case eSW_Day:
			snprintf(str, sizeof_str, "%s%c%s", "Year", _OUTSEP, pd2longstr[eSW_Day]);
			break;

		case eSW_Week:
			snprintf(str, sizeof_str, "%s%c%s", "Year", _OUTSEP, pd2longstr[eSW_Week]);
			break;

		case eSW_Month:
			snprintf(str, sizeof_str, "%s%c%s", "Year", _OUTSEP, pd2longstr[eSW_Month]);
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
static void _create_filename_ST(char *str, char *flag, int iteration,
			char *filename, size_t sizeof_filename, LOG_INFO* LogInfo) {
	char *basename;
	char *ext;
	char *fileDup = (char *)Mem_Malloc(strlen(str) + 1,
							"_create_filename_ST", LogInfo);

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
  \param FileStatus Struct of type
		SW_FILE_STATUS which holds basic information about output files
		and values
  \param LogInfo Holds information dealing with logfile output
*/
/***********************************************************/
static void _create_csv_file_ST(int iteration, OutPeriod pd, char *InFiles[],
								SW_FILE_STATUS *FileStatus, LOG_INFO *LogInfo)
{
	char filename[FILENAME_MAX];

	if (iteration <= 0)
	{ // STEPWAT2: aggregated values over all iterations
		if (FileStatus->make_regular[pd]) {
			// PROGRAMMER Note: `eOutputDaily + pd` is not very elegant and assumes
			// a specific order of `SW_FileIndex` --> fix and create something that
			// allows subsetting such as `eOutputFile[pd]` or append time period to
			// a basename, etc.
			_create_filename_ST(InFiles[eOutputDaily + pd], "agg", 0,
								filename, FILENAME_MAX, LogInfo);
			FileStatus->fp_reg_agg[pd] = OpenFile(filename, "w", LogInfo);
		}

		if (FileStatus->make_soil[pd]) {
			_create_filename_ST(InFiles[eOutputDaily_soil + pd], "agg", 0,
								filename, FILENAME_MAX, LogInfo);
			FileStatus->fp_soil_agg[pd] = OpenFile(filename, "w", LogInfo);
		}

	} else
	{ // STEPWAT2: storing values for every iteration
		if (iteration > 1) {
			// close files from previous iteration
			if (FileStatus->make_regular[pd]) {
				CloseFile(&FileStatus->fp_reg[pd], LogInfo);
			}
			if (FileStatus->make_soil[pd]) {
				CloseFile(&FileStatus->fp_soil[pd], LogInfo);
			}
		}

		if (FileStatus->make_regular[pd]) {
			_create_filename_ST(InFiles[eOutputDaily + pd], "rep", iteration,
								filename, FILENAME_MAX, LogInfo);
			FileStatus->fp_reg[pd] = OpenFile(filename, "w", LogInfo);
		}

		if (FileStatus->make_soil[pd]) {
			_create_filename_ST(InFiles[eOutputDaily_soil + pd], "rep", iteration,
								filename, FILENAME_MAX, LogInfo);
			FileStatus->fp_soil[pd] = OpenFile(filename, "w", LogInfo);
		}
	}
}
#endif



/* =================================================== */
/*             Global Function Definitions             */
/* --------------------------------------------------- */




#if defined(SOILWAT)

/** @brief create all of the user-specified output files.
 *
 * @param[in,out] SW_FileStatus Struct of type
 *	SW_FILE_STATUS which holds basic information about output files
 *	and values
 * @param[in] SW_Output SW_OUTPUT array of size SW_OUTNKEYS which holds
 * 	basic output information for all output keys
 * @param[in] n_layers Number of layers of soil within the simulation run
 * @param[in] InFiles Array of program in/output files
 * @param[in] GenOutput Holds general variables that deal with output
 * @param[in] LogInfo Holds information dealing with logfile output
 *
 *  @note Call this routine at the beginning of the main program run, but
 *  after SW_OUT_read() which sets the global variable use_OutPeriod.
*/
void SW_OUT_create_files(SW_FILE_STATUS* SW_FileStatus, SW_OUTPUT* SW_Output,
	LyrIndex n_layers, char *InFiles[], SW_GEN_OUT* GenOutput,
	LOG_INFO* LogInfo) {

	OutPeriod pd;

	ForEachOutPeriod(pd) {
		if (GenOutput->use_OutPeriod[pd]) {
			_create_csv_files(SW_FileStatus, pd, InFiles, LogInfo);

			write_headers_to_csv(pd, SW_FileStatus->fp_reg[pd],
				SW_FileStatus->fp_soil[pd], swFALSE, SW_FileStatus->make_regular,
				SW_FileStatus->make_soil, SW_Output, n_layers, GenOutput,
				LogInfo);
		}
	}
}


#elif defined(STEPWAT)

void SW_OUT_create_summary_files(SW_FILE_STATUS* SW_FileStatus,
		SW_OUTPUT* SW_Output, SW_GEN_OUT *GenOutput,
		char *InFiles[], LyrIndex n_layers, LOG_INFO* LogInfo) {

	OutPeriod p;

	ForEachOutPeriod(p) {
		if (GenOutput->use_OutPeriod[p]) {
			_create_csv_file_ST(-1, p, InFiles, SW_FileStatus, LogInfo);

			write_headers_to_csv(p, SW_FileStatus->fp_reg_agg[p],
				SW_FileStatus->fp_soil_agg[p], swTRUE, SW_FileStatus->make_regular,
				SW_FileStatus->make_soil, SW_Output, n_layers, GenOutput,
				LogInfo);
		}
	}
}

void SW_OUT_create_iteration_files(SW_FILE_STATUS* SW_FileStatus,
		SW_OUTPUT* SW_Output, int iteration, SW_GEN_OUT *GenOutput,
		char *InFiles[], LyrIndex n_layers, LOG_INFO* LogInfo) {

	OutPeriod p;

	ForEachOutPeriod(p) {
		if (GenOutput->use_OutPeriod[p]) {
			_create_csv_file_ST(iteration, p, InFiles, SW_FileStatus, LogInfo);

			write_headers_to_csv(p, SW_FileStatus->fp_reg[p],
				SW_FileStatus->fp_soil[p], swFALSE, SW_FileStatus->make_regular,
				SW_FileStatus->make_soil, SW_Output, n_layers, GenOutput,
				LogInfo);
		}
	}
}

#endif



/** Periodic output for Month and/or Week are actually
		printing for the PREVIOUS month or week.
		Also, see note on test value in _write_today() for
		explanation of the +1.

	@param[in] pd Time period in simulation output (day/week/month/year)
	@param[in] sizeof_str Size of parameter "str"
	@param[in] SW_Model Struct of type SW_MODEL holding basic time information
		about the simulation
	@param[in] tOffset Offset describing with the previous or current period
	@param[out] str String header buffer for every output row
*/
void get_outstrleader(OutPeriod pd, size_t sizeof_str,
			SW_MODEL* SW_Model, TimeInt tOffset, char *str) {
	switch (pd) {
		case eSW_Day:
			snprintf(str, sizeof_str, "%d%c%d", SW_Model->simyear, _OUTSEP, SW_Model->doy);
			break;

		case eSW_Week:
			snprintf(str, sizeof_str, "%d%c%d", SW_Model->simyear, _OUTSEP,
				(SW_Model->week + 1) - tOffset);
			break;

		case eSW_Month:
			snprintf(str, sizeof_str, "%d%c%d", SW_Model->simyear, _OUTSEP,
				(SW_Model->month + 1) - tOffset);
			break;

		case eSW_Year:
			snprintf(str, sizeof_str, "%d", SW_Model->simyear);
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
  \param n_layers Number of soil layers being dealt with in a simulation
  \param make_regular Array of size SW_OUTNPERIODS which holds boolean values
	specifying to output "regular" header names
  \param make_soil Array of size SW_OUTNPERIODS which holds boolean values
	specifying to output a soil-related header names
  \param SW_Output SW_OUTPUT array of size SW_OUTNKEYS which holds basic output
	information for all output keys
  \param GenOutput Holds general variables that deal with output
  \param LogInfo Holds information dealing with logfile output

*/
void write_headers_to_csv(OutPeriod pd, FILE *fp_reg, FILE *fp_soil,
	Bool does_agg, Bool make_regular[], Bool make_soil[], SW_OUTPUT* SW_Output,
	LyrIndex n_layers, SW_GEN_OUT* GenOutput, LOG_INFO* LogInfo) {

	char str_time[20];
	char
		// 3500 characters required for does_agg = TRUE
		header_reg[2 * OUTSTRLEN],
		// 26500 characters required for 25 soil layers and does_agg = TRUE
		header_soil[n_layers * OUTSTRLEN];

	// Acquire headers
	get_outstrheader(pd, str_time, sizeof str_time);
	_create_csv_headers(pd, header_reg, header_soil, does_agg,
			n_layers, SW_Output, GenOutput, LogInfo);

	// Write headers to files
	if (make_regular[pd]) {
		fprintf(fp_reg, "%s%s\n", str_time, header_reg);
		fflush(fp_reg);
	}

	if (make_soil[pd]) {
		fprintf(fp_soil, "%s%s\n", str_time, header_soil);
		fflush(fp_soil);
	}
}

void find_TXToutputSoilReg_inUse(Bool make_soil[], Bool make_regular[],
		SW_OUTPUT* SW_Output, OutPeriod timeSteps[][SW_OUTNPERIODS],
		IntUS used_OUTNPERIODS)
{
	IntUS i;
	OutKey k;

	ForEachOutPeriod(i)
	{
		make_soil[i] = swFALSE;
		make_regular[i] = swFALSE;
	}

	ForEachOutKey(k)
	{
		for (i = 0; i < used_OUTNPERIODS; i++)
		{
			if (timeSteps[k][i] != eSW_NoTime)
			{
				if (SW_Output[k].has_sl)
				{
					make_soil[timeSteps[k][i]] = swTRUE;
				} else
				{
					make_regular[timeSteps[k][i]] = swTRUE;
				}
			}
		}
	}
}


/** @brief close all of the user-specified output files.
    call this routine at the end of the program run.

	@param[in,out] SW_FileStatus Struct of type
		SW_FILE_STATUS which holds basic information about output files
		and values
	@param[in] GenOutput Holds general variables that deal with output
	@param[in] LogInfo Holds information dealing with logfile output
*/
void SW_OUT_close_files(SW_FILE_STATUS* SW_FileStatus, SW_GEN_OUT* GenOutput,
						LOG_INFO* LogInfo) {

	Bool close_regular, close_layers, close_aggs;
	OutPeriod p;


	ForEachOutPeriod(p) {
		#if defined(SOILWAT)
		close_regular = SW_FileStatus->make_regular[p];
		close_layers = SW_FileStatus->make_soil[p];
		close_aggs = swFALSE;

		#elif defined(STEPWAT)
		close_regular = (Bool) (SW_FileStatus->make_regular[p] && GenOutput->storeAllIterations);
		close_layers = (Bool) (SW_FileStatus->make_soil[p] && GenOutput->storeAllIterations);
		close_aggs = (Bool) ((SW_FileStatus->make_regular[p] || SW_FileStatus->make_soil[p])
			&& GenOutput->prepare_IterationSummary);
		#endif

		if (GenOutput->use_OutPeriod[p]) {
			if (close_regular) {
				CloseFile(&SW_FileStatus->fp_reg[p], LogInfo);
			}

			if (close_layers) {
				CloseFile(&SW_FileStatus->fp_soil[p], LogInfo);
			}

			if (close_aggs) {
				#ifdef STEPWAT
				CloseFile(&SW_FileStatus->fp_reg_agg[p], LogInfo);

				if (close_layers) {
					CloseFile(&SW_FileStatus->fp_soil_agg[p], LogInfo);
				}
				#endif
			}
		}
	}
}
