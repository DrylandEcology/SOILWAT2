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
