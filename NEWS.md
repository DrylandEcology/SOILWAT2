# NEWS

# SOILWAT2 v7.2.0
* Simulation output remains the same as the previous version.

* SOILWAT2 now handles errors more gracefully (#346; @N1ckP3rsl3y, @dschlaep).
    * Code no longer crashes on error immediately (except for STEPWAT2).
    * Code stores messages of warnings and error status.
    * All functions now check for errors and return early
      (after cleaning up memory).

* SOILWAT2 now defines its own structure for random numbers (#373; @dschlaep).
  This allows to hide implementation details, i.e., STEPWAT2 will no
  longer need to interact directly with `pcg` and rSOILWAT2 will no longer
  depend on `pcg` which it never has used.

* New `sw_strtok()` is thread-safe and replaces not thread-safe `strtok()`
  (#376; @N1ckP3rsl3y).

* Obsolete code in DEBUG_MEM* sections is removed (#369; @dschlaep).


# SOILWAT2 v7.1.0
* Simulation output remains the same as the previous version.

* Prepare for SOILWAT2 to become thread-safe and reentrant
  (#346; @N1ckP3rsl3y, @dschlaep)
    * Definition clarifications
        * Thread-safe - Multiple threads (a future SOILWAT2 development)
          will not influence each other unintentionally. Here, we implemented
          structures to enable thread-local storage, i.e., each thread operates
          on local data structures or with static data.
        * Reentrant - The ability to correctly execute any part of the program
          by multiple threads simultaneously but independently from others.
    * All non-static variables are replaced by local variables;
      functions gained arguments to pass local variables by reference
      (replacing global variables).
    * New main abstract types
        * SW_ALL - Contains the existing structures that are relevant
          for the simulation itself, e.g., SW_MODEL, SW_SOILWAT, SW_WEATHER.
        * PATH_INFO - Holds information about location of input and output data,
          e.g., directories, file paths to input data.
        * SW_OUTPUT_POINTERS - Points to requested output subroutines.
        * LOG_INFO - Manages information for logging warnings and errors.

* Tests now require `c++14` and utilize `googletest` `v1.14.0` (issue #339).

* Bugfixes
    * Fix an error where a pointer was freed even though it was not allocated
      (issue #356; @dschlaep).
    * Fix memory leak in test of `SW_VPD_construct()` (issue #205; @dschlaep).

## Changes to inputs
* The output separator `OUTSEP` has been unused since the switch (`v4.0.0`)
  from writing free-form text files to `"csv"` spreadsheets
  (where the separator is fixed to a comma); the occurrence of
  `OUTSEP` in `"outsetup.in"` is now deprecated and triggers a warning.


# SOILWAT2 v7.0.0
* This version produces nearly identical simulation output
  as the previous release under default values for the new inputs.
  Small deviations arise due to a fix in the handling of soil moisture values
  when between field capacity and saturation.

* Multiple soil water release curves (`SWRC`) are now implemented and can be
  selected with new input `swrc_name`. Implemented `SWRCs` currently include
  `"Campbell1974"`, `"vanGenuchten1980"`, and `"FXW"`. New input `has_swrcp`
  determines if parameters for a `SWRC` are estimated at run-time via an
  implemented pedotransfer function (`PTF`) based on new input `ptf_name` or
  if they are provided as inputs via new input file `"swrc_params.in"`.
  `rSOILWAT2` implements additional pedotransfer functions. See documentation
  entry of `"swrc_ptf"` for additional details and for guidance on how to
  implement additional `SWRCs` and `PTFs` (issue #315; @dschlaep).

* Soil density inputs can now represent either matric or bulk density
  (issue #280; @dschlaep).
    * Automatic conversion between matric and bulk density as needed
      using the new input `type_soilDensityInput`.

* Daily weather inputs that force a simulation are now processed
  all at once; previously, values were processed for one year at a time during
  the main simulation loop (issue #311; @dschlaep, @N1ckP3rsl3y).
    * Daily weather inputs are now obtained by `readAllWeather()`
      via `SW_WTH_read()` during `SW_CTL_read_inputs_from_disk()`, i.e.,
      the same time as other inputs are read from files.
    * Then, weather values are "finalized", i.e., missing values are imputed
      (e.g., by the weather generator) and scaled with monthly parameters,
      by `finalizeAllWeather()` via `SW_WTH_finalize_all_weather()`;
      this must occur before the simulation is "initialized"
      by `SW_CTL_init_run()`.
    * Imputation of missing daily weather values by `generateMissingWeather()`
      can be done either by last-observation-carried-forward `"LOCF"`
      (which handles all daily weather variables) or by the Markov weather
      generator (which handles only temperature and precipitation).

* Daily weather inputs, in addition to the previous variables
  maximum air temperature, minimum air temperature, and precipitation amount,
  can now process the following variables (issue #341; @dschlaep, @N1ckP3rsl3y):
    * Cloud cover (can be replaced by shortwave radiation)
    * Wind speed (can be replaced by wind components)
    * Wind speed eastward component (optional)
    * Wind speed northward component (optional)
    * Relative humidity (can be replaced by max/min humidity, specific humidity,
      dew point temperature, or vapor pressure)
    * Maximum relative humidity (optional)
    * Minimum relative humidity (optional)
    * Specific humidity (optional)
    * Dew point temperature (optional)
    * Actual vapor pressure (optional)
    * Downward surface shortwave radiation (optional)

* SOILWAT2 gains the ability to calculate long-term climate summaries
  (issue #317; @N1ckP3rsl3y, @dschlaep).
    * New `calcSiteClimate()` calculates monthly and annual time
      series of climate variables from daily weather.
    * New `averageClimateAcrossYears()` calculates long-term climate summaries.
    * Both functions are based on `rSOILWAT2::calc_SiteClimate()`
      which was previously coded in R.
    * This version fixes issues from the previous R version:
       * Mean annual temperature is now the mean across years of
         means across days within year of mean daily temperature.
       * Years at locations in the southern hemisphere are now adjusted to start
         on July 1 of the previous calendar year.
       * The cheatgrass-related variables, i.e., `Month7th_PPT_mm`,
         `MeanTemp_ofDriestQuarter_C`, and `MinTemp_of2ndMonth_C`,
         are now adjusted for location by hemisphere.

* SOILWAT2 gains the ability to estimate fractional land cover
  representing a potential natural vegetation based on climate relationships
  (using new input `veg_method`) instead of reading land cover values
  from input files (issue #318; @N1ckP3rsl3y, @dschlaep).
    * New `estimatePotNatVegComposition()` estimates
      fractional land cover representing a potential natural vegetation
      based on climate relationships.
      This function is based on `rSOILWAT2::estimate_PotNatVeg_composition()`
      which was previously coded in R.
    * New `estimateVegetationFromClimate()`
      (which is called by `SW_VPD_init_run()`) uses `veg_method` to determine
      at run time if a simulation utilizes `averageClimateAcrossYears()` and
      `estimatePotNatVegComposition()` to set land cover values
      instead of using the cover values provided in the input file.
    * This version fixes issues from the previous R version:
       * The `C4` grass correction based on Teeri & Stowe 1976 is now applied
         as documented (`rSOILWAT2` issue #218).
       * The sum of all grass components, if fixed, is now incorporated into
         the total sum of all fixed components (`rSOILWAT2` issue #219).


## Changes to inputs
* New inputs via `"weathsetup.in"` determine whether monthly or daily inputs
  for cloud cover, relative humidity, and wind speed are utilized;
  describe which daily weather variables are contained in the weather input
  files `weath.YYYY`; and describe units of (optional) input shortwave
  radiation.
* New (optional) variables (columns) in weather input files `weath.YYYY` that
  are described via `"weathsetup.in"`.
* New inputs via `"siteparam.in"` select a soil water release curve `swrc_name`
  and determine parameter source `has_swrcp`, i.e.,
  either estimated via selected pedotransfer function `ptf_name` or
  values obtained from new input file `"swrc_params.in"`.
  Default values `"Campbell1974"`, `"Cosby1984AndOthers"`, and 0
  (i.e., use `PTF` to estimate paramaters) reproduce previous behavior.
* New input file `"swrc_params.in"` to provide parameters of the selected
  soil water release curve (if not estimated via a pedotransfer function)
  for each soil layer.
* SOILWAT2 gains `type_soilDensityInput` as new user input (`siteparam.in`)
  with default value 0 (i.e., matric soil density)
  that reproduces previous behavior.
* SOILWAT2 gains `veg_method` as new user input (`"veg.in"`)
  with default value 0 (i.e., land cover are obtained from input files)
  that reproduces previous behavior.


# SOILWAT2 v6.7.0
* This version produces exactly the same simulation output
  as the previous release under default values
  (i.e., vegetation establishment is turned off).

* Functionality to calculate and output establishment/recruitment of species
  now works again and is now covered by tests (issue #336, @dschlaep).
  Note that this functionality assesses yearly the chances of
  species to recruit/establish based on simulated daily conditions;
  however, establishment/recruitment outcomes are not utilized to inform the
  simulation.

## Changes to inputs
* New input via `"<species>.estab"` sets the vegetation type of
  a species establishment parameters'.


# SOILWAT2 v6.6.0
* Random number generators now produce sequences that can be exactly reproduced
  (@dschlaep).
    * `RandSeed()` gains arguments "initstate" and "initseq" (and lost "seed")
      to fully seed a `pcg32` random number generator.
    * `RandNorm()` is now re-entrant and discards one of the two generated
      values. Compilation with `"RANDNORMSTATIC"` re-produces the old,
      not re-entrant implementation.
    * `SW_MKV_construct()` now only seeds `markov_rng` (the random number
      generator of the weather generator) if run as `SOILWAT2` using the new
      input `rng_seed`; `SW_MKV_construct()` does not seed `markov_rng`
      when run as part of `STEPWAT2` or `rSOILWAT2`
      (both of which use their own `RNG` initialization procedures).
* `SW_WTH_init_run()` now also initializes yesterday's weather values
  (@dschlaep).

## Changes to inputs
* SOILWAT2 gains `rng_seed` as new user input (`"weathsetup.in"`).
