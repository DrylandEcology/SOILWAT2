/********************************************************/
/********************************************************/
/*  Source file: SW_Files.h
 *  Type: header
 *  Application: SOILWAT - soilwater dynamics simulator
 *  Purpose: Define anything needed to support Files.c
 *           including function declarations.
 *  History:
 *     (8/28/01) -- INITIAL CODING - cwb
 *     (1/24/02) -- added logfile entry and rearranged order.
 09/30/2011	(drs)	added function SW_WeatherPrefix()
 09/30/2011	(drs)	added function SW_OutputPrefix()
 */
/********************************************************/
/********************************************************/
#ifndef SW_FILES_H
#define SW_FILES_H

#include "include/SW_datastructs.h" // for SW_PATH_INPUTS, LOG_INFO

#ifdef __cplusplus
extern "C" {
#endif

/* The number of enum elements between eNoFile and
 * eEndFile (not inclusive) must match SW_NFILES.
 * also, these elements must match the order of
 * input from `files.in`.
 */
typedef enum {
    eNoFile = -1,
    /* List of all input files */
    eFirst = 0,
    /* netCDF-related input files directory */
    eNCIn,
    eNCInAtt,
    eNCOutVars,
    /* Domain information */
    eDomain,
    /* Description of a model run */
    eModel,
    eLog,
    /* Description of simulated site */
    eSite,
    eLayers,
    eSWRCp,
    /* Weather and climate forcing */
    eWeather,
    eMarkovProb,
    eMarkovCov,
    eSky,
    /* Description of vegetation */
    eVegProd,
    eVegEstab,
    /* Description of CO2 effects */
    eCarbon,
    /* (optional) soil moisture measurements */
    eSoilwat,
    /* Simulation outputs */
    eOutput,
    eOutputDaily,
    eOutputWeekly,
    eOutputMonthly,
    eOutputYearly,
    eOutputDaily_soil,
    eOutputWeekly_soil,
    eOutputMonthly_soil,
    eOutputYearly_soil,
    eEndFile
} SW_FileIndex;

/* =================================================== */
/*             Global Function Declarations            */
/* --------------------------------------------------- */
void SW_F_read(SW_PATH_INPUTS *SW_PathInputs, LOG_INFO *LogInfo);

void SW_F_deepCopy(
    SW_PATH_INPUTS *source, SW_PATH_INPUTS *dest, LOG_INFO *LogInfo
);

void SW_F_init_ptrs(SW_PATH_INPUTS *SW_PathInputs);

void SW_F_construct(SW_PATH_INPUTS *SW_PathInputs, LOG_INFO *LogInfo);

void SW_F_deconstruct(SW_PATH_INPUTS *SW_PathInputs);

void SW_F_CleanOutDir(char *outDir, LOG_INFO *LogInfo);


#ifdef __cplusplus
}
#endif

#endif
