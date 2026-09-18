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

  * The inputs comprise the main file `files.in` and the content of the
    `Input/` folder. They are explained in detail
    [here](doc/additional_pages/SOILWAT2_Inputs.md).
  * The user can turn on/off different types of outputs via the
    input file \ref outsetupin in text-mode and
    \ref SW2_netCDF_output_variables in nc-mode.
  * Warning and error messages, if any, are written to a
    logfile `logs/logfile.log`
    (the file name and path is controlled by input from `"files.in"`).
  * The outputs are written to the folder `Output/`.
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
  * Output weeks represent 7-day periods that restart each year.
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


### Output in text-mode
SOILWAT2 may produce up to eight output files (depending on the value of
`TIMESTEP` which is a user in input in file \ref outsetupin).
The output files are text files in a `comma-separated values` (`.csv`)
format (e.g., you can open them directly into a spreadsheet program or
import them into a data analysis program).
The output variables and measurement units are explained in \ref outsetupin.
The names of the output files are user inputs in the file \ref filesin.

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


<br>


<hr>
Go back to the [main page](README.md) or
[user guide](doc/additional_pages/A_SOILWAT2_user_guide.md).
