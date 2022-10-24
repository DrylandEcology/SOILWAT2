# SOILWAT2 v7.0.0
* Moved `calc_SiteClimate()` and `estimate_PotNatVeg_composition()` 
    functionality from rSOILWAT2 to SOILWAT2.
* Yesterday values for variables in SW_WEATHER_2DAYS struct are no longer used, 
    which motivated the struct being renamed to SW_WEATHER_NOW.
* Weather is now read in all at once, instead of reading one year at a time, by the new function
    `readAllWeather()`.
* SOILWAT2 gains `veg_method` as new user input (`"veg.in"`).

# SOILWAT2 v6.6.0
* Random number generators now produce sequences that can be exactly reproduced.
* `RandSeed()` gains arguments "initstate" and "initseq" (and lost "seed") to
  fully seed a `pcg32` random number generator.
* `RandNorm()` is now re-entrant and discards one of the two generated values.
  Compilation with `"RANDNORMSTATIC"` re-produces the old, not re-entrant
  implementation.
* SOILWAT2 gains `rng_seed` as new user input (`"weathsetup.in"`).
* `SW_MKV_construct()` now only seeds `markov_rng` (the random number generator
  of the weather generator) if run as `SOILWAT2` using the new input `rng_seed`;
  `SW_MKV_construct()` does not seed `markov_rng` when run as part of `STEPWAT2`
  or `rSOILWAT2` (both of which use their own `RNG` initialization procedures).
* `SW_WTH_init_run()` now also initializes yesterday's weather values.