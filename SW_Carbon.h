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
  #define MAX_CO2_YEAR 3000            // Max integer year that is supported; arbitrary
  #define BIO_INDEX 0                  // Use these indices to access the respective values
  #define WUE_INDEX 1

  /* Helper structures */
  // Holds a double for each plant functional type, which reduces the number of needed variables
  typedef struct {
    double
      grass,
      shrub,
      tree,
      forb;
  } PFTs;

  /* Main structure */
  typedef struct {
    int
      addtl_yr,                          // Added to SW_Model.year to get the future year we're simulating
      use_wue_mult,                      // Determine which multipliers we will be calculating...
      use_bio_mult;

    char
      scenario[64];                      // The scenario we are extracting ppm from

	PFTs
      co2_bio_mult,                      // The biomass multiplier (yearly), which for tree is instead applied to the percent live
      co2_wue_mult,                      // The Water-use efficiency multiplier (yearly)
      co2_multipliers[2][MAX_CO2_YEAR];  // Holds 2 multipliers for each PFT per year

  } SW_CARBON;

  /* Function Declarations */
  #ifdef RSOILWAT
    SEXP onGet_SW_CARBON(void);
    void onSet_swCarbon(SEXP object);
  #endif

  void SW_CBN_construct(void);
  void apply_CO2(double* new_biomass, double *biomass, double multiplier);
  void calculate_CO2_multipliers(void);
#endif
