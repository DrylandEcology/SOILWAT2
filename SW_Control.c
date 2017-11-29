/********************************************************/
/********************************************************/
/*  Source file: Control.c
 *  Type: module
 *  Application: SOILWAT - soilwater dynamics simulator
 *  Purpose: This module controls the flow of the model.
 *           Previously this was done in main() but to
 *           combine the model with other code (eg STEPPE)
 *           there needs to be separate callable routines
 *           for initializing, model flow, and output.
 *
 *  History:
 *     (10-May-02) -- INITIAL CODING - cwb
 02/04/2012	(drs)	in function '_read_inputs()' moved order of 'SW_VPD_read' from after 'SW_VES_read' to before 'SW_SIT_read': SWPcrit is read in in 'SW_VPD_read' and then calculated SWC_atSWPcrit is assigned to each layer in 'SW_SIT_read'
 06/24/2013	(rjm)	added call to SW_FLW_construct() in function SW_CTL_init_model()
 */
/********************************************************/
/********************************************************/

/* =================================================== */
/*                INCLUDES / DEFINES                   */
/* --------------------------------------------------- */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "generic.h"
#include "filefuncs.h"
#include "rands.h"
#include "SW_Defines.h"
#include "SW_Files.h"
#include "SW_Control.h"
#include "SW_Model.h"
#include "SW_Output.h"
#include "SW_Site.h"
#include "SW_SoilWater.h"
#include "SW_VegEstab.h"
#include "SW_VegProd.h"
#include "SW_Weather.h"

/* =================================================== */
/*                  Global Declarations                */
/* --------------------------------------------------- */
extern SW_MODEL SW_Model;
extern SW_VEGESTAB SW_VegEstab;
extern SW_VEGPROD SW_VegProd;
#ifdef RSOILWAT
	extern Bool useFiles;
	extern SEXP InputData;
	void SW_FLW_construct(void);
#endif

SW_OUTPUT SW_Output_Files; // need to store the filenames when created in the stat_Output_timestep_CSV_Summary functions for use in SW_Output.c
char *InFiles[SW_NFILES];

/* =================================================== */
/*                Module-Level Declarations            */
/* --------------------------------------------------- */
static void _read_inputs(void);
static void _begin_year(void);
static void _begin_day(void);
static void _end_day(void);
static void _collect_values(void);

/* Dummy declaration for Flow constructor defined in SW_Flow.c */
void SW_FLW_construct(void);

/*******************************************************/
/***************** Begin Main Code *********************/

void SW_CTL_main(void) {
	TimeInt *cur_yr = &SW_Model.year;

	/*----------------------------------------------------------
    Get proper order for rank_SWPcrits
  ----------------------------------------------------------*/
  int outerLoop, innerLoop;
  float key;

  RealF tempArray[4], tempArrayUnsorted[4]; // need two temp arrays equal to critSoilWater since we dont want to alter the original at all
  tempArray[0] = SW_VegProd.critSoilWater[0];
  tempArray[1] = SW_VegProd.critSoilWater[1];
  tempArray[2] = SW_VegProd.critSoilWater[2];
  tempArray[3] = SW_VegProd.critSoilWater[3];
  tempArrayUnsorted[0] = SW_VegProd.critSoilWater[0];
  tempArrayUnsorted[1] = SW_VegProd.critSoilWater[1];
  tempArrayUnsorted[2] = SW_VegProd.critSoilWater[2];
  tempArrayUnsorted[3] = SW_VegProd.critSoilWater[3];

  // insertion sort to rank the veg types and store them in their proper order
  for (outerLoop = 1; outerLoop < 4; outerLoop++)
   {
       key = tempArray[outerLoop]; // set key equal to critical value
       innerLoop = outerLoop-1;
       while (innerLoop >= 0 && tempArray[innerLoop] < key)
       {
         // code to switch values
         tempArray[innerLoop+1] = tempArray[innerLoop];
         innerLoop = innerLoop-1;
       }
       tempArray[innerLoop+1] = key;
   }

   // loops to compare sorted v unsorted array and find proper index
   for(outerLoop = 0; outerLoop < 4; outerLoop++){
     for(innerLoop = 0; innerLoop < 4; innerLoop++){
       if(tempArray[outerLoop] == tempArrayUnsorted[innerLoop]){
         SW_VegProd.rank_SWPcrits[outerLoop] = innerLoop;
         tempArrayUnsorted[innerLoop] = 100; // set value to something impossible so if a duplicate a different index is picked next
         break;
       }
     }
   }
	 /*printf("%d = %f\n", SW_VegProd.rank_SWPcrits[0], SW_VegProd.critSoilWater[SW_VegProd.rank_SWPcrits[0]]);
   printf("%d = %f\n", SW_VegProd.rank_SWPcrits[1], SW_VegProd.critSoilWater[SW_VegProd.rank_SWPcrits[1]]);
   printf("%d = %f\n", SW_VegProd.rank_SWPcrits[2], SW_VegProd.critSoilWater[SW_VegProd.rank_SWPcrits[2]]);
   printf("%d = %f\n\n", SW_VegProd.rank_SWPcrits[3], SW_VegProd.critSoilWater[SW_VegProd.rank_SWPcrits[3]]);*/
   /*----------------------------------------------------------
     End of rank_SWPcrits
   ----------------------------------------------------------*/

	for (*cur_yr = SW_Model.startyr; *cur_yr <= SW_Model.endyr; (*cur_yr)++) {
		SW_CTL_run_current_year();
	}

#ifndef RSOILWAT
	SW_OUT_close_files();
#endif
} /******* End Main Loop *********/
/*******************************************************/

void SW_CTL_init_model(const char *firstfile) {
	/*=======================================================*/
	/* initialize all structures, simulating
	 * a constructor call */

	SW_F_construct(firstfile);
	SW_MDL_construct();
	SW_WTH_construct();
	SW_SIT_construct();
	SW_VES_construct();
	SW_VPD_construct();
	SW_OUT_construct();
	SW_SWC_construct();
	SW_FLW_construct();

	_read_inputs();

}

void SW_CTL_run_current_year(void) {
	/*=======================================================*/
	TimeInt *doy = &SW_Model.doy;

	_begin_year();

	for (*doy = SW_Model.firstdoy; *doy <= SW_Model.lastdoy; (*doy)++) {
		_begin_day();

		SW_SWC_water_flow();

		if (SW_VegEstab.use)
			SW_VES_checkestab();

		_end_day();
	}
	SW_OUT_flush();


}

/**
  \fn void stat_Output_Daily_CSV_Summary(int iteration)
  \brief Creates daily files

  Creates daily files for SOILWAT standalone and for STEPWAT depending on defined flags
	for STEPWAT if -i flag is used it creates file for each iteration naming file based on iteration

  \param iteration. Current iteration for file name if -i flag used in STEPWAT
*/
///This function will create daily
/***********************************************************/
void stat_Output_Daily_CSV_Summary(int iteration)
{
	if(iteration == -1){ // just storing average values over all iterations or soilwat standalone
		SW_Output_Files.fp_dy_avg = OpenFile(SW_F_name(eOutputDaily), "w");
		SW_Output_Files.fp_dy_soil_avg = OpenFile(SW_F_name(eOutputDaily_soil), "w");
	}
	else{ // storing values for every iteration
		char *newFile; // new file to create
		char * newFile_soil;
		char *extension; // extension to add to end of file
		char *fileDup = malloc(strlen(SW_F_name(eOutputDaily))+1);
		char *fileDup_soil = malloc(strlen(SW_F_name(eOutputDaily_soil))+1);
		char iterationToString[10];

		strcpy(fileDup, SW_F_name(eOutputDaily)); // copy file name to new variable
		strcpy(fileDup_soil, SW_F_name(eOutputDaily_soil)); // copy file name to new variable

		sprintf(iterationToString, "%d", iteration); // convert iteration from int to string
		extension = strtok(fileDup, ".");
		extension = strtok(NULL, "."); // get the extension to add to new file (not hardcoding since can have different extensions)

		newFile = strtok(SW_F_name(eOutputDaily), "."); // get filename up to but not including ".csv"
		strcat(newFile, "_");
		strcat(newFile, iterationToString);
		strcat(newFile, ".");
		strcat(newFile, extension);
		SW_Output_Files.fp_dy = OpenFile(newFile, "w"); // open new file

		newFile_soil = strtok(SW_F_name(eOutputDaily_soil), "."); // get filename up to but not including ".csv"
		strcat(newFile_soil, "_");
		strcat(newFile_soil, iterationToString);
		strcat(newFile_soil, ".");
		strcat(newFile_soil, extension);
		SW_Output_Files.fp_dy_soil = OpenFile(newFile_soil, "w"); // open new file

		free(fileDup);
		free(fileDup_soil);
	}
}

/**
  \fn void stat_Output_Weekly_CSV_Summary(int iteration)
  \brief Creates weekly files

  Creates weekly files for SOILWAT standalone and for STEPWAT depending on defined flags
	for STEPWAT if -i flag is used it creates file for each iteration naming file based on iteration

  \param iteration. Current iteration for file name if -i flag used in STEPWAT
*/
//This function will create Weekly
/***********************************************************/
void stat_Output_Weekly_CSV_Summary(int iteration)
{
	if(iteration == -1){ // just storing average values over all iterations
		SW_Output_Files.fp_wk_avg = OpenFile(SW_F_name(eOutputWeekly), "w");
		SW_Output_Files.fp_wk_soil_avg = OpenFile(SW_F_name(eOutputWeekly_soil), "w");
	}
	else{ // storing values for every iteration
		char *newFile; // new file to create
		char * newFile_soil;
		char *extension; // extension to add to end of file
		char *fileDup = malloc(strlen(SW_F_name(eOutputWeekly))+1);
		char *fileDup_soil = malloc(strlen(SW_F_name(eOutputWeekly_soil))+1);
		char iterationToString[10];

		strcpy(fileDup, SW_F_name(eOutputWeekly)); // copy file name to new variable
		strcpy(fileDup_soil, SW_F_name(eOutputWeekly_soil)); // copy file name to new variable

		sprintf(iterationToString, "%d", iteration); // convert iteration from int to string
		extension = strtok(fileDup, ".");
		extension = strtok(NULL, "."); // get the extension to add to new file (not hardcoding since can have different extensions)

		newFile = strtok(SW_F_name(eOutputWeekly), "."); // get filename up to but not including ".csv"
		strcat(newFile, "_");
		strcat(newFile, iterationToString);
		strcat(newFile, ".");
		strcat(newFile, extension);
		SW_Output_Files.fp_wk = OpenFile(newFile, "w"); // open new file

		newFile_soil = strtok(SW_F_name(eOutputWeekly_soil), "."); // get filename up to but not including ".csv"
		strcat(newFile_soil, "_");
		strcat(newFile_soil, iterationToString);
		strcat(newFile_soil, ".");
		strcat(newFile_soil, extension);
		SW_Output_Files.fp_wk_soil = OpenFile(newFile_soil, "w"); // open new file

		free(fileDup);
		free(fileDup_soil);
	}
}

/**
  \fn void stat_Output_Monthly_CSV_Summary(int iteration)
  \brief Creates montly files

  Creates monthly files for SOILWAT standalone and for STEPWAT depending on defined flags
	for STEPWAT if -i flag is used it creates file for each iteration naming file based on iteration

  \param iteration. Current iteration for file name if -i flag used in STEPWAT
*/
//This function will create Monthly
/***********************************************************/
void stat_Output_Monthly_CSV_Summary(int iteration)
{
	if(iteration == -1){ // just storing average values over all iterations
		SW_Output_Files.fp_mo_avg = OpenFile(SW_F_name(eOutputMonthly), "w");
		SW_Output_Files.fp_mo_soil_avg = OpenFile(SW_F_name(eOutputMonthly_soil), "w");
	}
	else{ // storing values for every iteration
		char *newFile; // new file to create
		char * newFile_soil;
		char *extension; // extension to add to end of file
		char *fileDup = malloc(strlen(SW_F_name(eOutputMonthly))+1);
		char *fileDup_soil = malloc(strlen(SW_F_name(eOutputMonthly_soil))+1);
		char iterationToString[10];

		strcpy(fileDup, SW_F_name(eOutputMonthly)); // copy file name to new variable
		strcpy(fileDup_soil, SW_F_name(eOutputMonthly_soil)); // copy file name to new variable

		sprintf(iterationToString, "%d", iteration); // convert iteration from int to string
		extension = strtok(fileDup, ".");
		extension = strtok(NULL, "."); // get the extension to add to new file (not hardcoding since can have different extensions)

		newFile = strtok(SW_F_name(eOutputMonthly), "."); // get filename up to but not including ".csv"
		strcat(newFile, "_");
		strcat(newFile, iterationToString);
		strcat(newFile, ".");
		strcat(newFile, extension);
		SW_Output_Files.fp_mo = OpenFile(newFile, "w"); // open new file

		newFile_soil = strtok(SW_F_name(eOutputMonthly_soil), "."); // get filename up to but not including ".csv"
		strcat(newFile_soil, "_");
		strcat(newFile_soil, iterationToString);
		strcat(newFile_soil, ".");
		strcat(newFile_soil, extension);
		SW_Output_Files.fp_mo_soil = OpenFile(newFile_soil, "w"); // open new file

		free(fileDup);
		free(fileDup_soil);
	}
}

/**
  \fn void stat_Output_Yearly_CSV_Summary(int iteration)
  \brief Creates yearly files

  Creates yearly files for SOILWAT standalone and for STEPWAT depending on defined flags
	for STEPWAT if -i flag is used it creates file for each iteration naming file based on iteration

  \param iteration. Current iteration for file name if -i flag used in STEPWAT
*/
//This function will create Yearly
/***********************************************************/
void stat_Output_Yearly_CSV_Summary(int iteration)
{
	if(iteration == -1){ // just storing average values over all iterations
		SW_Output_Files.fp_yr_avg = OpenFile(SW_F_name(eOutputYearly), "w");
		SW_Output_Files.fp_yr_soil_avg = OpenFile(SW_F_name(eOutputYearly_soil), "w");
	}
	else{ // storing values for every iteration
		char *newFile; // new file to create
		char * newFile_soil;
		char *extension; // extension to add to end of file
		char *fileDup = malloc(strlen(SW_F_name(eOutputYearly))+1);
		char *fileDup_soil = malloc(strlen(SW_F_name(eOutputYearly_soil))+1);
		char iterationToString[10];

		strcpy(fileDup, SW_F_name(eOutputYearly)); // copy file name to new variable
		strcpy(fileDup_soil, SW_F_name(eOutputYearly_soil)); // copy file name to new variable

		sprintf(iterationToString, "%d", iteration); // convert iteration from int to string
		extension = strtok(fileDup, ".");
		extension = strtok(NULL, "."); // get the extension to add to new file (not hardcoding since can have different extensions)

		newFile = strtok(SW_F_name(eOutputYearly), "."); // get filename up to but not including ".csv"
		strcat(newFile, "_");
		strcat(newFile, iterationToString);
		strcat(newFile, ".");
		strcat(newFile, extension);
		SW_Output_Files.fp_yr = OpenFile(newFile, "w"); // open new file

		newFile_soil = strtok(SW_F_name(eOutputYearly_soil), "."); // get filename up to but not including ".csv"
		strcat(newFile_soil, "_");
		strcat(newFile_soil, iterationToString);
		strcat(newFile_soil, ".");
		strcat(newFile_soil, extension);
		SW_Output_Files.fp_yr_soil = OpenFile(newFile_soil, "w"); // open new file

		free(fileDup);
		free(fileDup_soil);
	}
}

static void _begin_year(void) {
	/*=======================================================*/
	/* in addition to the timekeeper (Model), usually only
	 * modules that read input yearly or produce output need
	 * to have this call */
	SW_MDL_new_year();
	SW_WTH_new_year();
	SW_SWC_new_year();
	SW_VES_new_year();
	SW_OUT_new_year();

}

static void _begin_day(void) {
	/*=======================================================*/

	SW_MDL_new_day();
	SW_WTH_new_day();
}

static void _end_day(void) {
	/*=======================================================*/

	_collect_values();
	SW_WTH_end_day();
	SW_SWC_end_day();

}

static void _collect_values(void) {
	/*=======================================================*/

	SW_OUT_sum_today(eSWC);
	SW_OUT_sum_today(eWTH);
	SW_OUT_sum_today(eVES);

	SW_OUT_write_today();

}

static void _read_inputs(void) {
	/*=======================================================*/
#ifndef RSOILWAT
	SW_F_read(NULL );
	SW_MDL_read();
	SW_WTH_read();
	SW_VPD_read();
	SW_SIT_read();
	SW_VES_read();
	SW_OUT_read();
	SW_SWC_read();
#else
	if (useFiles) { //Read in the data and set it
		SW_F_read(NULL );
		SW_MDL_read();
		SW_WTH_read();
		SW_VPD_read();
		SW_SIT_read();
		SW_VES_read();
		SW_OUT_read();
		SW_SWC_read();
	} else { //Use R data to set the data
		onSet_SW_F(GET_SLOT(InputData,install("files")));
		//Rprintf("swFiles\n");
		onSet_SW_MDL(GET_SLOT(InputData,install("years")));
		//Rprintf("swYears\n");
		onSet_SW_WTH(GET_SLOT(InputData,install("weather")));
		//Rprintf("swWeather\n");
		onSet_SW_VPD(GET_SLOT(InputData,install("prod")));
		//Rprintf("swProd\n");
		onSet_SW_SIT(GET_SLOT(InputData,install("site")));
		//Rprintf("swSite\n");
		onSet_SW_VES(GET_SLOT(InputData,install("estab")));
		//Rprintf("swEstab\n");
		onSet_SW_OUT(GET_SLOT(InputData,install("output")));
		//Rprintf("swOutput\n");
		onSet_SW_SWC(GET_SLOT(InputData,install("swc")));
		//Rprintf("swSWC\n");
	}
#endif
}

#ifdef DEBUG_MEM
#include "SW_Markov.h"  /* for setmemrefs function */
/*======================================================*/
void SW_CTL_SetMemoryRefs( void) {
	/* when debugging memory problems, use the bookkeeping
	 code in myMemory.c
	 This routine sets the known memory refs so they can be
	 checked for leaks, etc.  Includes malloc-ed memory in
	 SOILWAT.  All refs will have been cleared
	 by a call to ClearMemoryRefs() before this, and will be
	 checked via CheckMemoryRefs() after this, most likely in
	 the main() function.
	 */

	SW_F_SetMemoryRefs();
	SW_OUT_SetMemoryRefs();
	SW_SWC_SetMemoryRefs();
	SW_SIT_SetMemoryRefs();
	SW_WTH_SetMemoryRefs();
	SW_MKV_SetMemoryRefs();
}

#endif
