# SOILWAT2 Outputs

[SOILWAT2]: https://github.com/DrylandEcology/SOILWAT2

Note: this document is best viewed as part of the doxygen-built documentation
(there may be text artifacts if viewed as standalone-markdown).

<br>

### Example
  * The source code contains a complete example simulation project in
    `tests/example/`
  * Modify inputs as desired and run a simulation, e.g.,
```{.sh}
    make bin_run
```
    or, equivalently,
```{.sh}
    make bin
    bin/SOILWAT2 -d ./tests/example -f files.in
```

  * The inputs comprise the main file \ref filesin and the contents of the
    `Input/` folder and `Input_nc/` if in nc-based mode.
    They are explained in detail
    [here](doc/additional_pages/SOILWAT2_Inputs.md).
  * The user can turn on/off different types of outputs via the
    input file \ref outsetupin in text-mode and
    \ref SW2_netCDF_output_variables in nc-mode.
  * Warning and error messages, if any, are written to logfiles at `logs/`
    (file name and path is controlled by input from \ref filesin).
  * The outputs are written to the folder `Output/`
    (folder name is controlled by input from \ref outsetupin).
    Outputs are explained in detail \ref explain_outputs "below".

<br>


<hr>
\section explain_outputs Outputs

SOILWAT2 uses the standard calendar for simulations and outputs.
The standard calendar corresponds to the Gregorian calendar for dates after
1582-October-15. A year is a leap year, according to the Gregorian rule,
if either it is divisible by 4 but not by 100 or it is divisible by 400.
A leap year has 29 days in February (instead of 28) resulting in a year with
366 days (instead of 365).

The simulations have a daily resolution (time step); however, the code can
accumulate or average values for output also at weekly, monthly, seasonal, and
yearly time steps.
  * Output months and years represent standard calendar months and years.
  * Output weeks represent pentads (5-day) or heptads (7-day) that
    restart each year (default are heptads).
  * Output seasons represent four groups of three calendar months each
    * Seasons are
        (1) March, April, May;
        (2) June, July, August;
        (3) September, October, November;
        (4) December, January, February.
    * The output starts with season 1 in March of the first simulated year;
      thus, the values of January and February of the first year are not
      included in the seasonal output (because the season does not have values
      for December from the previous year).
    * The output ends with season 3 in November of the last simulated year;
      thus, the values of December of the last year are not in included in the
      seasonal output (because the season does not have values for January and
      February from the following year).
    * Strided `netCDF` output includes a season 4 in the first of the two
      strides which intersect the season.


### Selecting outputs
Two input files select and describe outputs.

\ref outsetupin is used in all modes and specifies
  * the output time steps (`TIMESTEP`: daily, weekly, monthly, seasonal,
    yearly),
  * for each output group (`KEY`) whether it is produced and how daily values
    are summarized across an output period (`SUMTYPE`: `OFF`, `SUM`, `AVG`,
    or `FIN`),
  * the output directory (`outputDir`), and
  * the names of the output files in text-mode.

The comments in \ref outsetupin describe the variables, and their units,
of each output group.

\ref SW2_netCDF_output_variables is used additionally in nc-mode
(and in mpi-mode) and specifies for each variable
  * whether it is produced (`Do output?`) and at which of the time steps
    requested by \ref outsetupin (`Active output period(s)`),
  * its name, attributes, units (converted from SOILWAT2 internal units),
    and storage type (`double` or packed) in the `netCDF` output files.

In nc-mode, an output group is produced only if it is turned on in
\ref outsetupin and if at least one of its variables is turned on in
\ref SW2_netCDF_output_variables.
The `netCDF` files contain descriptions of each variable as attributes
(e.g., `long_name`, `comment`, `units`, `cell_methods`).


### Output in text-mode
SOILWAT2 may produce up to eight output files (depending on the value of
`TIMESTEP` which is a user in input in file \ref outsetupin).
The output files are text files in a `comma-separated values` (`.csv`)
format.
The output variables and measurement units are explained in \ref outsetupin.
The names of the output files are user inputs in the file \ref outsetupin.

* Output files for soil layer specific variables
  * Output file for daily output time period
  * Output file for weekly output time period
  * Output file for monthly output time period
  * Output file for seasonal output time period
  * Output file for yearly output time period

* Output files for other variables
  * Output file for daily output time period
  * Output file for weekly output time period
  * Output file for monthly output time period
  * Output file for seasonal output time period
  * Output file for yearly output time period

### Output in nc-mode
SOILWAT2 may produce many `"netCDF"` output files. All details including
variable names, units, and file names are provided via
\ref SW2_netCDF_output_variables.

Variables can be stored as `"double"` type (default).
Alternatively, they can be packed (with loss of precision)
to `"integer"` (32-bit) or `"short"` (16-bit) types.

Two parameters `"add_offset"` and `"scale_factor"` determine packing via
```{.sh}
    packed = round((original - add_offset) / scale_factor)
```

Packing parameters may be calculated from the maximum and minimum value
of the variable to be packed. If maximum and minimum values are known, then
these equations result in the most effective packing
with minimal loss of precision as they spread the original values across
the entire range of the packed type.
```{.sh}
    scale_factor = (max - min) / (2^bits - 1)
    add_offset = (max + min) / 2
```

Values that cannot be represented by the packed type become missing.


### Derived metrics
Output groups `DERIVEDSUM` and `DERIVEDAVG` contain metrics that are derived
from daily simulated values and then summed or averaged across each output
period. Variable names encode thresholds and the soil depth range,
e.g., `ddd5C0SWE30bar000to100cm`:
  * `5C`: daily mean air temperature above 5 C
  * `0SWE`: no snowpack (snow water equivalent of zero)
  * `15bar`, `30bar`, `39bar`: soil water potential of -1.5, -3.0,
    and -3.9 MPa, respectively
  * `000to100cm`: soil depth range from 0 to 100 cm;
    `MAX` refers to the depth of the soil profile

Daily definitions:
  * Available soil water `swa` (cm): water held at less tension
    than the threshold, i.e., soil water content in excess of the soil water
    content at the threshold potential summed across soil layers;
    each soil layer is weighted by the fraction of its width that lies
    within the depth range.
  * Dry day: `swa` is zero, i.e., all soil moisture within the depth range is
    held at a tension at least as large as the threshold.
  * Wet day: `swa` is positive.
  * Dry degree-days `ddd` and wet degree-days `wdd` (C day):
    daily mean air temperature minus 5 C on dry and wet days, respectively,
    if daily mean air temperature is above 5 C
    (and if there is no snowpack for variables with `0SWE`).
  * Wet days `wet` (days): 1 on wet days, 0 otherwise.
  * Climatic water deficit `cwd` (cm): potential minus
    actual evapotranspiration.

A depth range ends at the soil profile depth if the soil profile is shallower.
Output of a depth range that no soil layer reaches is missing
(`NaN` in text output, in rSOILWAT2, and in STEPWAT2;
`_FillValue` in netCDF output),
i.e., `swa30bar020to060cm` if the soil profile is not deeper than 20 cm and
`swa30bar060toMAXcm` if the soil profile is not deeper than 60 cm.

Since v8.5.0, `ddd5C30bar000to100cm` and `wdd5C15bar000to100cm` no longer
require the absence of a snowpack; their definitions of v8.2.0 - v8.4.0
are now available as `ddd5C0SWE30bar000to100cm` and
`wdd5C0SWE15bar000to100cm`, respectively.


<br>


<hr>
Go back to the [main page](README.md) or
[user guide](doc/additional_pages/A_SOILWAT2_user_guide.md).
