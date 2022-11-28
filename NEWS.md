# NEWS

# SOILWAT2 v7.0.0-9000
* The type of soil density is a new input.
  Soil density inputs can now represent either matric or bulk density;
  the code converts automatically as needed (issue #280; @dschlaep).

## Changes to inputs
* File `siteparam.in` gained new input for `type_soilDensityInput`.


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
