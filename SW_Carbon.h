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
  #define MAX_CO2_YEAR 3000            // Include all years that our PPM sources might provide data for
  #define BIO_INDEX 0                  // Use these indices to access the respective values
  #define WUE_INDEX 1
  
  /* Main structure */
  typedef struct {  
    int
    addtl_yr,                          // Added to SW_Model.year to get the future year we're simulating
    use_wue_mult,                      // Determine which multipliers we will be calculating...
    use_bio_mult;

    double
    carbon[2],                         // Holds misc. data
    co2_multipliers[2][MAX_CO2_YEAR],  // Holds the above multipliers for every year, accessed directly (e.g. biomass multiplier for 1982 is co2_multipliers[1][1982])
    co2_bio_mult,                      // The biomass multiplier (yearly), which for tree is instead applied to the percent live
    co2_wue_mult;                      // The Water-use efficiency multiplier (yearly)
  
    char
    scenario[64];                      // The scenario we are extracting PPM from
  } SW_CARBON;

  /* Function Declarations */
  #ifdef RSOILWAT
    SEXP onGet_SW_CARBON(void);
    void onSet_swCarbon(SEXP object);
  #endif
  
  void SW_CBN_construct(void);
  void apply_CO2(double* new_biomass, double *biomass);
  void calculate_CO2_multipliers(void);
#endif
