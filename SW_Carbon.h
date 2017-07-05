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

  typedef struct {
    int
    addtl_yr,                  // Added to SW_Model.year to get the future year we're simulating
    RCP,                       // The RCP that we are extracting ppm data from
    use_sto_mult,              // Determine which multipliers we will be calculating...
    use_bio_mult;

    double
    carbon[2],                 // Hold misc. data
    co2_biomass_mult,          // The biomass multiplier (yearly)
    co2_wue_mult,              // The stomatal multiplier (yearly)
    co2_multipliers[2][3000];  // Holds the above multipliers for every year, accessed directly (e.g. biomass multiplier for 1982 is co2_multipliers[1][1982])

  } SW_CARBON;

  /* Function Declarations */
  #ifdef RSOILWAT
    SEXP onGet_SW_CARBON(void);
    void onSet_swCarbon(SEXP object);

  #endif

  void apply_CO2(double* new_biomass, double *biomass);
  void calculate_CO2_multipliers(void);

#endif
