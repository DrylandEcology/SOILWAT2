/**
 * @file   SW_Carbon.h
 * @author Zachary Kramer, Charles Duso
 * @brief  Defines functions, constants, and variables that deal with the effects of CO2 on transpiration and biomass.
 * @date   23 January 2017
 */
#ifndef CARBON
  #define CARBON             /**< A macro that only lets the variables in @f SW_Carbon.h be defined once. */
  #define MAX_CO2_YEAR 2500  /**< An integer representing the max year that is supported. The number just needs to be reasonable, it is an artifical limit. */
  #define BIO_INDEX 0        /**< An integer representing the index of the biomass multipliers in the SW_CARBON#co2_multipliers 2D array. */
  #define WUE_INDEX 1        /**< An integer representing the index of the WUE multipliers in the SW_CARBON#co2_multipliers 2D array. */

  /* Helper structures */
  /**
   * @brief Holds a double for each plant functional type.
   *
   * This reduces the number of needed variables in both CO2 calculations and output
   */
  typedef struct {
    double
      grass,
      shrub,
      tree,
      forb;
  } PFTs;

  /**
   * @brief The main structure holding all CO2-related data.
   *
   * Mostly replicates the swCarbon class defined in rSOILWAT2, but also
   * includes PFT variables, which store the results of the calculations.
   */
  typedef struct {
    int
      use_wue_mult,                      /**< A boolean integer indicating if WUE multipliers should be calculated. */
      use_bio_mult,                      /**< A boolean integer indicating if biomass multipliers should be calculated. */
      addtl_yr;                          /**< An integer representing how many years in the future we are simulating. */

    char
      scenario[64];                      /**< A 64-char array holding the scenario we are extracting ppm from. */

    PFTs
      co2_bio_mult,                      /**< A PFT struct that gets overridden yearly to hold the current biomass multiplier, which for tree is instead applied to the percent live. */
      co2_wue_mult,                      /**< A PFT struct that gets overridden yearly to hold the current Water-use efficiency multiplier. */
      co2_multipliers[2][MAX_CO2_YEAR];  /**< A 2D array of PFT structures. Column BIO_INDEX holds biomass multipliers. Column WUE_INDEX holds WUE multipliers. Rows represent years. */

    double
      ppm[MAX_CO2_YEAR];                 /**< A 1D array holding ppm values that are indexed by year. Is typically only populated for the years that are being simulated. */

  } SW_CARBON;

  /* Function Declarations */
  #ifdef RSOILWAT
    SEXP onGet_SW_CARBON(void);
    void onSet_swCarbon(SEXP object);
  #endif

  void SW_CBN_construct(void);
  void SW_CBN_read(void);
  void apply_CO2(double* new_biomass, double *biomass, double multiplier);
  void calculate_CO2_multipliers(void);
#endif
