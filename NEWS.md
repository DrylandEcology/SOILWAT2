# NEWS

# SOILWAT2 v7.0.0-9000
* Soil density inputs can now represent either matric or bulk density
  (issue #280; @dschlaep).
    * Automatic conversion between matric and bulk density as needed
      using the new input `type_soilDensityInput`.

## Changes to inputs
* SOILWAT2 gains `type_soilDensityInput` as new user input (`siteparam.in`)
  with default value 0 (matric soil density) that reproduces previous behavior.


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
