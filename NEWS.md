# NEWS

# SOILWAT2 v8.1.1-devel

* Soil water retention curve parameters that are non-finite now throw an error.

* Fix scaling of daily meteorological variables (#454).

* Warning and error messages are now, by default, written to logs/logfile.log

* nc-based SOILWAT2 can now use a `"netCDF"` file to provide inputs for
  constant soil temperature at depth `"Tsoil_constant"`
  (which now can vary across the simulation domain).
  The new example input is `"inSite/tas-clim.nc"` reproducing the
  corresponding value from text-based `"siteparam.in"`.


# SOILWAT2 v8.1.0
* This version produces similar but not identical simulation output
  as previously because of the following changes:
    * Small deviations arise from replacing all remaining variables of
      type float with type double (see commit 62237ae on 2024-July-30).
    * Saturated percolation is now limited which leads to different outcomes
      during periods of high infiltration (e.g., snowmelt) and during conditions
      of low hydraulic conductivity (e.g., frozen soils, sapric organic matter).
    * Depth of snowpack is now consistent with snowpack water content.
    * Surface temperature extremes are now less sensitive to high biomass, and
      average surface temperature is now more consistent with daily extremes.

* The two modes of SOILWAT2 can now be compiled with the following flags:
    * `make CPPFLAGS=-DSWTXT` (or as previously `make all`) for txt-based
    * `make CPPFLAGS=-DSWNC` for nc-based SOILWAT2.

* SOILWAT2 now ends gracefully with termination (SIGTERM) and
  interrupt (SIGINT, commonly CTRL+C on the keyboard) signals.

* nc-based SOILWAT2 can now use a large variety of `"netCDF"` data sources
  as inputs (#389; @N1ckP3rsl3y, @dschlaep).
  The user identifies in `"SW2_netCDF_input_variables.tsv"` which
  input variables are provided by `"netCDF"`
  (and vary among simulation runs, i.e., sites or grid cells);
  remaining inputs are obtained from the same text files as in txt-based mode
  (and are constant among simulation runs).

* `"ncTestRuns"` provide a new framework that comprehensively tests
  nc-based SOILWAT2 via the script `"tools/check_ncTestRuns.sh"` by
  creating, executing, and checking more than 40 test runs.
    * Comprehensive set of spatial configurations of simulation domains and
      input netCDFs (see `"tests/ncTestRuns/data-raw/metadata_testRuns.csv"`)
    * Runs with daily inputs from external data sources
      including `"Daymet"`, `"gridMET"`, and `"MACAv2METDATA"` (if available)
    * One site/grid cell in the simulation domain is set up to correspond
      to the reference run (which, by default, is equivalent to
      `"tests/example"` in txt-based mode); output at that site/grid cell is
      compared against the reference output

* Tests now require `c++17` and utilize `googletest` `v1.15.2` (issue #427).

* SOILWAT2 can now represent the influence of soil organic matter on the
  soil water retention curve and the saturated hydraulic conductivity
  parameter (#397; @dschlaep). The implemented approach first determines
  organic matter properties for the soil layers assuming fibric peat
  characteristics at the soil surface and characteristics of sapric peat
  at a user-specified depth. Then, bulk soil parameters of the soil water
  retention curve are estimated as linear combinations of properties for
  the mineral soil component and of properties for the organic matter soil
  component using the proportion of organic matter in the bulk soil as weights.
  The bulk soil saturated hydraulic conductivity parameter accounts for
  flow pathways through organic matter above a threshold and assumes
  conductivities through mineral and organic components in series outside of
  those pathways.

* Saturated percolation is now limited. The upper bound is a function based on
  the saturated hydraulic conductivity parameter
  (which includes effects of organic matter), frozen soils, and a
  user-specified `"permeability"` factor.

* Fix the calculation of depth of snowpack (#441; @dschlaep).
  Depth of snowpack is now calculated after sublimation occurred and
  is based on snowpack density that now is always larger than 0; this now makes
  depth of snowpack and snowpack water content consistent.

* Fix the estimation of surface temperature (#440; @dschlaep).
  Biomass effects are now capped at a value at which
  cooling and heating effects on minimum and maximum surface temperature
  result, across average conditions, in no change for mean surface temperature.
  Effects of maximum air temperature on maximum surface temperature are limited
  to air temperatures above freezing.
  Minimum and maximum surface temperature are now set to their average
  (with a warning) if the initial estimate of minimum surface temperature is
  larger than the initial estimate of maximum surface temperature.
  Average surface temperature is now (by default) estimated from minimum and
  maximum surface temperature (instead of independently as previously), but
  the old method can be selected.

## Changes to inputs
* New input via `"siteparam.in"` to specify the depth at which characteristics
  of organic matter have completely switched from fibric to sapric peat
  (default is 50 cm).
* New input via `"siteparam.in"` to select the method for estimating
  surface temperature (with a new default).
* New input via `"soils.in"` to provide the proportion of soil organic matter
  to bulk soil by weight for each soil layer.
* New input via `"swrc_params*.in"` to provide parameter values of the
  soil water retention curve representing fibric and sapric peat.
  Note: Some parameter values for the `"FXW"` SWRC are missing.
* `"climate.in"`: new snow density values for July, August and September
  estimated with linear interpolation from June and October values.
* New command line option `"-p"` to prepare the domain/progress,
  index, and output files; with this flag, no simulations will be run.

## Changes to inputs for nc-based SOILWAT2
* New tab-separated value `"tsv"` input file `"SW2_netCDF_input_variables.tsv"`
  that lists, activates, and describes each input from `"netCDF"` files.
  This file replaces `"files_nc.in"`.
* `"desc_nc.in"` provides new inputs for the names of
  geographic and projected `"CRS"` variables as well as names for the spatial
  coordinate axes and variables.
* New example inputs as `"netCDF"` files that exactly txt-based inputs
    * `"inClimate/monthlyClimate.nc"` corresponding to `"climate.in"`
    * `"inSoil/soil.nc"` and `"inSoil/swrcp.nc"`
      corresponding to `"soils.in"` and `"swrc_params.in"`
    * `"inTopo/topo.nc"` corresponding to `"modelrun.in"`
    * `"inVeg/veg2.nc"` and `"inVeg/vegPFT.nc"`
      corresponding to `"veg.in"`
    * `"inWeather/weather.nc"` corresponding to `"data_weather/weath.*"`


# SOILWAT2 v8.0.1
* Simulation output remains the same as the previous version unless
  relative humidity is calculated from vapor pressure or specific humidity.

* Fix the calculation of relative humidity (#435; @dschlaep).
  Previously, relative humidity was incorrectly calculated if based on
  vapor pressure or specific humidity.
  The calculation of relative humidity from specific humidity now
  also uses elevation (to estimate air pressure).

* Fix the count of days on which a missing weather value was replaced by a
  non-missing value from the preceding day for the method `"LOCF"`
  (last observation carried forward; #437; @dschlaep). Previously, any day
  with a missing weather value was counted.

## Changes to inputs
* Units of specific humidity inputs changed from `"%"` to `"g kg-1"`.


# SOILWAT2 v8.0.0
* Simulation output remains the same as the previous version.
  However, output of establishment/recruitment for two species is now
  generated by default by the example simulation run.

* SOILWAT2 can now be compiled in one of two modes: `"nc"` and `"text"`
  (#361, #362, #363; @N1ckP3rsl3y, @dschlaep):
    * `"nc"`-based SOILWAT2: output files are produced in `"netCDF"` format;
      input files in `"netCDF"` format are currently limited to the `"domain"`.
      This mode is compiled if the new preprocessor definition `"SWNETCDF"`
      and the `"netCDF-c"` library are available,
      e.g., `make CPPFLAGS=-DSWNETCDF all`
    * `"text"`-based SOILWAT2 (as previously): output files are plain text
      that are formatted as comma-separated values `"csv"`;
      input files are plain text.

* SOILWAT2 now represents a simulation `"domain"`, i.e.,
  a set of (independent) simulation units/runs (#360; @N1ckP3rsl3y, @dschlaep).
  A `"domain"` is defined by either a set of `"sites"` or by a `"xy"`-grid.
  The current simulation set consists of simulation units within a `"domain"`
  that have not yet been simulated.
  Simulation units are identified via `"suid"` (simulation unit identifier).
    * Input text files are read once and now populate a `"template"`
      that is used for each simulation unit.
    * New `SW_CTL_RunSimSet()` loops over the simulation set or
      runs one user specified simulation unit.
      It writes warning and error messages from all simulation units to the
      `logfile` but does not exit if a simulation unit fails
      (`main()` will exit with an error if it fails or
      if every simulation unit produced an error).
    * New `SW_CTL_run_sw()` takes a deep copy of the `"template"` as basis
      for each simulation unit.
    * For `"netCDF"`-based SOILWAT2, information about the `"domain"`
      (i.e., simulation units and their geographic locations) is obtained from
      `"domain.nc"` (#361; @N1ckP3rsl3y, @dschlaep).
    * For `"nc"`-based SOILWAT2, simulation progress (success/failure) is
      tracked by `"progress.nc"` (#387; @N1ckP3rsl3y, @dschlaep);
      progress tracking makes re-starts after partial completion of the
      simulation set by an earlier execution possible.

* SOILWAT2 can now be compiled with `"udunits2"` to convert output to user
  requested units (#392; @dschlaep, @N1ckP3rsl3y).
    * Unit conversion is available only in `"nc"`-mode and if compiled
      with the new preprocessor definition `"SWUDUNITS"`
      and if the `"udunits2"` library is available,
      e.g., `make CPPFLAGS='-DSWNETCDF -DSWUDUNITS' all`.
    * Users request output units via field `"netCDF units"` of the
      input file `"SW2_netCDF_output_variables.tsv"`.

* Tests now utilize the same template/deep-copy approach (@dschlaep),
  i.e., input files from `test/example/` populate a `"template"` and
  test fixture deep copy the `"template"` (avoiding repeated reading from disk).

* SOILWAT2 gained basic time tracking and reporting (#380; @dschlaep).
  Temporal resolution may only be full seconds (for C99) but
  could be sub-seconds (for C11 or later).
  The runs over the simulation set end gracefully
  if a user specified wall time limit is (nearly) reached
  (nearly is defined by `SW_WRAPUPTIME`).
  A report is written to `stdout` or (silently) to the `logfile` if user
  requested quiet mode; the report includes
    * Overall wall time and proportion of wall time for the simulation set
    * Time of simulation units and reports average, standard deviation
      minimum and maximum time.

* SOILWAT2 gained spin-up functionality (#379; @niteflyunicorns, @dschlaep).
  A user-requested sequence of (random) years is simulated (without output)
  before the actual simulation run to provide better starting values
  (e.g., soil moisture, soil temperature).
  User inputs are obtained from `"domain.in"`.

* The random number generator `"pcg"` is now a submodule of a forked copy
  of the previous repository `imneme/pcg-c-basic` (#353; @dschlaep).
  This allowed us to fix a function declaration without a prototype.

* Specified a consistent code style and formatted code,
  header include directives, doxygen documentation, and
  updated code contribution guidelines (#316; @dschlaep).

## Changes to inputs
* New input file `"domain.in"` with input variables that specify
  type and spatial dimensions of the simulation `"domain"`
  (with a backwards compatible default of one site),
  start and end year (the latter previously in `"years.in"`), and
  spin-up information (mode, duration, scope).
  This input file uses a key-value pair approach, i.e., inputs must use
  the correct key while they don't have to be on a specific line
  (as they have to be in other input files).
* Input file `"years.in"` was removed (content moved to `"domain.in"`).
* New input file `"modelrun.in"` with inputs for geographic coordinates and
  topography (previously in `"siteparam.in"`).
* Input file `"siteparam.in"` lost inputs for geographic coordinates and
  topography (content moved to `"modelrun.in"`).
* Input file `"files.in"` gained two entries for `"nc"`-based SOILWAT2.
* New command line option `"-s X"` where `X` is a simulation unit identifier;
  if the option `"-s X"` is absent or `X` is `0`, then all simulation units
  in the simulation `"domain"` are run;
  otherwise, only the simulation unit `X` is run.
* New command line option `"-t X"` where `X` is the wall time limit in seconds.
  The code gracefully ends early if the wall time reaches a limit of
  `X - SW_WRAPUPTIME` seconds; if the option `"-t X"` is absent,
  then there is (practically) no wall time limit.
* New command line option `"-r"` to rename netCDF domain template file
  to domain file name provided in `"Input_nc/files_nc.in"`.

## Changes to outputs
* Output of establishment/recruitment for two species is now
  generated by default by the example simulation run (#411; @dschlaep).

## Changes to inputs and outputs for `"nc"`-based SOILWAT2
* `"nc"`-based SOILWAT2 is compiled if the new preprocessor definition
  `"SWNETCDF"` is available, e.g., `make CPPFLAGS=-DSWNETCDF all`;
  the capability to convert units is compiled with the new preprocessor
  definition `"SWUDUNITS"`, e.g., `make CPPFLAGS='-DSWNETCDF -DSWUDUNITS' all`
* User-specified paths to external headers and libraries can be provided
    * `"netCDF-c"` headers: `NC_CFLAGS="-I/path/to/include"`
    * `"netCDF-c"` library: `NC_LIBS="-I/path/to/lib"`
    * `"udunits2"` headers: `UD_CFLAGS="-I/path/to/include"`
    * `"udunits2"` library: `UD_LIBS="-I/path/to/lib"`
    * `"expat"` headers: `EX_CFLAGS="-I/path/to/include"`
    * `"expat"` library: `EX_LIBS="-I/path/to/lib"`
* New input directory `"Input_nc/"` that describes `"nc"`-based inputs
  (paths provided by new entries in `"files.in"`).
* New text input file `"files_nc.in"` that lists for each input purpose
  (currently, `"domain"` and `"progress"` are implemented)
  the path to the `"netCDF"` input file and associated variable name.
* New text input file `"attribues_nc.in"` to provide global attributes and a
  geographic (and optionally a projected) `"CRS"` (coordinate reference system)
  that describe the `"domain.nc"`.
* A user provided `"domain.nc"` that describes the simulation `"domain"`.
  Specifications must be consistent with `"domain.in"`.
  If absent, a template is automatically generated based on `"domain.in"`.
* A user provided `"progress.nc"` that describes the simulation `"progress"`.
  Specifications must be consistent with `"domain.nc"`.
  The `"progress"` variable can optionally be contained in `"domain.nc"`
  instead of a separate file.
  If absent, it is automatically generated based on `"domain.nc"`.
* New tab-separated value `"tsv"` input file `"SW2_netCDF_output_variables.tsv"`
  that lists, activates, and describes each output variable in `"netCDF"` mode.
* All outputs are written to `"netCDF"` files based on user requests from
  `"SW2_netCDF_output_variables.tsv"`.
  Each `"netCDF"` output file contains the output variables from one output
  group `"outkey"` and one output period (daily, weekly, monthly, yearly).


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
