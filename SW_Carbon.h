/**
 * @file   SW_Carbon.h
 * @author Zachary Kramer, Charles Duso
 * @brief  Contains functions, constants, and variables that deal with the effect of
 *         CO2 on transpiration and biomass.
 *
 * @data   04 January 2016
 */


/* Global Variables - Must include this header file to use these values */
extern double       co2_biomass_mult; // Biomass multiplier
extern double       co2_wue_mult;     // WUE multiplier
extern unsigned int calculate_co2;    // Logical to determine if co2 is used
