/**
 * @file   SW_Carbon.h
 * @author Zachary Kramer, Charles Duso
 * @brief  Contains functions, constants, and variables that deal with the
 *         effect of CO2 on transpiration and biomass.
 *
 * @date   23 January 2017
 */
#ifndef CARBON
#define CARBON

/* Global Variables - Must include this header file to use these values */
extern unsigned int calculate_co2;             // Logical to determine if co2 is used
extern unsigned int addtl_yr;                  // Tracks the year
extern double       carbon[2];                 // Holds temporary data
extern double       co2_biomass_mult;          // Biomass multiplier
extern double       co2_wue_mult;              // WUE multiplier
extern double       co2_multipliers[2][5000];  // Y dim must have enough slots for all years
extern int          RCP;
extern int          use_future_bio_mult;
extern int          use_future_sto_mult;
extern int          use_retro_bio_mult;
extern int          use_retro_sto_mult;

/* Function Declarations */
void apply_CO2(double* new_biomass, double *biomass);
void SW_Carbon_Get(void);

#endif
