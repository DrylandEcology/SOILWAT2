/**
 * @file   SW_Carbon.h
 * @author Zachary Kramer, Charles Duso
 * @brief  Defines functions, constants, and variables that deal with the effects of CO2 on transpiration and biomass.
 * @date   23 January 2017
 */
#ifndef CARBON
#define CARBON

#include "include/SW_Defines.h"
#include "include/SW_VegProd.h"


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief The main structure holding all CO2-related data.
 */
typedef struct {
  int
    use_wue_mult,                      /**< A boolean integer indicating if WUE multipliers should be calculated. */
    use_bio_mult;                      /**< A boolean integer indicating if biomass multipliers should be calculated. */

  char
    scenario[64];                      /**< A 64-char array holding the scenario name for which we are extracting CO2 data from the carbon.in file. */

  double
    ppm[MAX_NYEAR];                 /**< A 1D array holding atmospheric CO2 concentration values (units ppm) that are indexed by calendar year. Is typically only populated for the years that are being simulated. `ppm[index]` is the CO2 value for the calendar year `index + 1` */

} SW_CARBON;


/* =================================================== */
/*            Externed Global Variables                */
/* --------------------------------------------------- */
extern SW_CARBON SW_Carbon;


/* =================================================== */
/*             Global Function Declarations            */
/* --------------------------------------------------- */
void SW_CBN_construct(void);
void SW_CBN_deconstruct(void);
void SW_CBN_read(void);
void SW_CBN_init_run(SW_VEGPROD* SW_VegProd);


#ifdef __cplusplus
}
#endif

#endif
