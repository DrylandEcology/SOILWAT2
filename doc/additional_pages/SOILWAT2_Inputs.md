# SOILWAT2 Inputs

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
    make all
    bin/SOILWAT2 -d ./tests/example -f files.in
```


  * The inputs comprise the main file \ref filesin and the contents of the
    `Input/` folder and `Input_nc/` if in nc-based mode.
    Inputs are explained in detail \ref explain_inputs "below";
    see also \ref weather_inputs "weather inputs".
  * Warning and error messages, if any, are written to logfiles at `logs/`
    (file name and path is controlled by input from \ref filesin).
  * The outputs are written to the folder `Output/`
    (folder name is controlled by input from \ref outsetupin).
    Outputs are explained in detail
    [here](doc/additional_pages/SOILWAT2_Outputs.md).


<hr>
\section general_input_principles General input information

### Spatial configurations between simulation domain and nc-based input domain

#### Supported spatial combinations

| Simulation domain: type | Simulation domain: CRS | Input domain: type | Input domain: CRS |
|------------------------:|-----------------------:|-------------------:|------------------:|
|                    site |             geographic |               site |        geographic |
|                    site |             geographic |            gridded |        geographic |
|                    site |              projected |               site |         projected |
|                    site |              projected |            gridded |         projected |
|                 gridded |             geographic |            gridded |        geographic |
|                 gridded |              projected |            gridded |         projected |
|                    site |           projected`1` |               site |        geographic |
|                    site |           projected`1` |            gridded |        geographic |
|                 gridded |           projected`1` |            gridded |        geographic |

`1`, Possible if the simulation domain provides a secondary geographic CRS
in addition to the primary projected CRS (see \ref desc_nc).


#### Unsupported spatial combinations

The following combinations will fail.

| Simulation domain: type | Simulation domain: CRS | Input domain: type | Input domain: CRS |
|------------------------:|-----------------------:|-------------------:|------------------:|
|                    site |             geographic |               site |         projected |
|                    site |             geographic |            gridded |         projected |
|                 gridded |             geographic |               site |        geographic |
|                 gridded |             geographic |               site |         projected |
|                 gridded |              projected |               site |        geographic |
|                 gridded |              projected |               site |         projected |
|                 gridded |             geographic |            gridded |         projected |


<br>

### Supported calendars for input datasets

nc-based SOILWAT2 accepts a range of calendars for inputs;
however, they all are interpreted through the lense of a standard calendar
which is the only calendar currently implemented for simulations and outputs.

Supported calendars
  * standard, gregorian, proleptic_gregorian
  * no_leap
    * Accepted synonyms: noleap, 365day, 365_day
    * There are no leap days and every year has 365 days.
    * SOILWAT2 interprets inputs for days 60 and 365 during a year
      that has a leap day in the standard calendar (e.g., 1980)
      as February 29 and December 30;
      thus, there are no inputs to SOILWAT2's December 31 in leap years.
  * all_leap
    * Accepted synonyms: allleap, 366day, 366_day
    * Every year has a leap day (Feb-29) for a total of 366 days.
    * SOILWAT2 ignores inputs for day 366 during years that have no leap day
      in the standard calendar (e.g., 1981).

There are additional calendars accepted by the `"CF"` convention which are
not supported as SOILWAT2 inputs including 360_day, 360day, julian.


<br>

### Missing data

txt-based SOILWAT2 uses the value `999` to represent missing data
(see \ref SW_MISSING).

nc-based SOILWAT2 recognizes the approaches documented by the `"CF"` convention
to identify missing data and valid data ranges.
Each variable may use different value(s) to represent missing data.
The following variable attributes are recognized:
  * `_FillValue` (with netCDF library defaults)
  * `missing_value`
  * `valid_min` and `valid_max`, or `valid_range`


<hr>
\section weather_inputs Weather inputs

### Activation and sources of weather inputs

Daily weather:
  * txt-based: the flags for daily weather inputs in \ref weathsetupin
    activate variables, which are read from text files `weath.YYYY`
    (path prefix in \ref filesin).
  * nc-based: if at least one variable (other than `indexSpatial`) of the
    input group `inWeather` is enabled ("Do nc-input?" is `1`) in
    \ref SW2_netCDF_input_variables, then all daily weather is read from
    netCDF files; the enabled `inWeather` variables determine the daily
    weather inputs and the flags for daily weather inputs in
    \ref weathsetupin are ignored.
    Otherwise, daily weather is read from text files as for txt-based runs.

Mean monthly climate (cloud cover, wind speed, and relative humidity):
  * The flags for mean monthly inputs in \ref weathsetupin
    activate the use of mean monthly values (for all modes).
  * Values are read from \ref climatein (txt-based) or,
    for each variable of the input group `inClimate` that is enabled in
    \ref SW2_netCDF_input_variables, from netCDF files (nc-based).

All other settings in \ref weathsetupin (e.g., weather generator,
fixes to weather inputs, description of shortwave radiation, and scaling
parameters) apply to all modes.

### Daily weather variables

| Variable | SW2 units | SW2 variable | Requirement |
|:---------|:----------|:-------------|:------------|
| Maximum air temperature | C | `temp_max` | required |
| Minimum air temperature | C | `temp_min` | required |
| Precipitation | cm | `ppt` | required |
| Cloud cover | % | `cloudcov` | cloud cover or shortwave radiation |
| Wind speed | m s-1 | `windspeed` | wind speed or its components |
| Eastward and northward wind components | m s-1 | `windspeed_east`, `windspeed_north` | |
| Relative humidity | % | `r_humidity` | one humidity input |
| Maximum and minimum relative humidity | % | `rmax_humidity`, `rmin_humidity` | |
| Specific humidity | g kg-1 | `spec_humidity` | |
| Dew point temperature | C | `temp_dewpoint` | |
| Actual vapor pressure | kPa | `actualVaporPressure` | |
| Downward surface shortwave radiation | see \ref weathsetupin | `shortWaveRad` | cloud cover or shortwave radiation |

  * "SW2 units" are the units used internally by SOILWAT2.
    * txt-based: daily weather inputs must be provided in "SW2 units".
    * nc-based: daily weather inputs may use any units that can be converted
      to "SW2 units" by `udunits2`; the units of an input are specified by
      the column "ncVarUnits" of \ref SW2_netCDF_input_variables
      (which should match the attribute "units" of the netCDF variable);
      if "ncVarUnits" is `NA`, then no conversion is attempted and
      inputs are assumed to be in "SW2 units".
  * "SW2 variable" is the SOILWAT2 internal name of a variable
    (column "SW2 variable" of \ref SW2_netCDF_input_variables).
    * txt-based: the order of daily variables in `weath.YYYY` must match
      the order of the activated flags in \ref weathsetupin
      (variable names are not used).
    * nc-based: the name of the variable in a netCDF input file can differ
      from "SW2 variable"; it is specified by the column "ncVarName"
      of \ref SW2_netCDF_input_variables.

  * Daily mean air temperature is the mean of maximum and minimum
    air temperature.
  * Wind speed is calculated from its eastward and northward components
    if wind speed itself is not an input.
  * Mean monthly cloud cover, wind speed, and relative humidity are
    interpolated to daily values if the corresponding monthly flag in
    \ref weathsetupin is activated; a monthly flag overrides the
    corresponding daily inputs (for humidity: all daily humidity inputs),
    including enabled `inWeather` variables in nc-based runs.
  * Every simulated day requires values of air temperature, precipitation,
    wind speed, actual vapor pressure (from any humidity input), and
    cloud cover or shortwave radiation.
    Missing values may be imputed (see weather generator in
    \ref weathsetupin).

### Humidity

Actual vapor pressure is the humidity variable used by the simulation:
clear-sky solar radiation (via precipitable water) and
potential evapotranspiration (via vapor pressure deficit and
net longwave radiation) are calculated from it.

If more than one daily humidity input is provided, actual vapor pressure
is determined in the following order of precedence, which follows the
decreasing accuracy of the conversion to daily mean actual vapor pressure:
  1. Actual vapor pressure: no conversion.
  2. Dew point temperature:
     \f$e_a = e^{\circ}_w(T_{dew})\f$.
     Exact if vapor pressure is constant throughout the day;
     otherwise, a daily mean dew point slightly underestimates mean vapor
     pressure (typically by < 1 %).
  3. Specific humidity \f$q\f$, together with
     atmospheric pressure \f$p\f$ estimated from site elevation
     (site elevation is required on days when specific humidity is used):
     \f$e_a = q p / (0.622 + 0.378 q)\f$ (\f$q\f$ in kg kg-1).
     Independent of air temperature and nearly linear in \f$q\f$;
     the error is due to deviations of actual atmospheric pressure from
     the elevation-based estimate (e.g., synoptic variations of about
     \f$\pm\f$ 1.5 kPa result in errors of about \f$\pm\f$ 2 % at 1500 m).
  4. Maximum and minimum relative humidity, together with
     maximum and minimum air temperature
     (ASCE 2005 eq. 11 \cite ASCE2005):
     \f$e_a = [e^{\circ}(T_{min}) RH_{max} + e^{\circ}(T_{max}) RH_{min}] / 200\f$.
     Exact if vapor pressure is constant throughout the day and if the
     extremes of relative humidity coincide with those of air temperature;
     errors occur if these assumptions are violated.
  5. Relative humidity, together with daily mean air temperature:
     \f$e_a = e^{\circ}(T_{avg}) RH / 100\f$.
     Biased high even if vapor pressure is constant throughout the day:
     because \f$1 / e^{\circ}(T)\f$ is convex, daily mean relative humidity
     exceeds \f$100 e_a / e^{\circ}(T_{avg})\f$; for example, for a sinusoidal
     daily cycle of air temperature, vapor pressure is overestimated by
     about 3 % for a range of 0 - 10 C, 4 % for -10 - 0 C,
     11 % for 10 - 30 C, and 12 % for 5 - 25 C
     (twice that if daily mean relative humidity is the mean of the values
     at minimum and maximum air temperature).

This ranking agrees with Allen et al. (1998, ch. 3) \cite allen1998,
who recommend dew point temperature and caution against
daily mean relative humidity.
Each humidity input is used only on days when it is not missing;
otherwise, the next available humidity input is used for that day.
Relative humidity inputs are checked only on days when they are used.

Mean monthly relative humidity (if the monthly flag is activated) replaces
all daily humidity inputs and is converted to actual vapor pressure
as daily relative humidity.

Conventions:
  * Saturation vapor pressure \f$e^{\circ}\f$ follows
    Huang (2018) \cite huang2018JAMC;
    \f$e^{\circ}_w\f$ is saturation over liquid water at all temperatures
    (i.e., over supercooled water below 0 C).
  * Daily relative humidity inputs (mean, maximum, minimum) are,
    by default, interpreted as relative to saturation over
    liquid water at all temperatures (WMO convention).
    Some datasets (e.g., some climate model outputs) report relative humidity
    relative to saturation over ice at or below 0 C; such inputs are
    supported by setting the convention of daily relative humidity inputs
    in \ref weathsetupin to 1 (the convention applies to all daily
    relative humidity inputs and to all simulation units).
    Mean monthly relative humidity inputs are always interpreted as relative
    to saturation over liquid water;
    dew point temperature is defined over liquid water.
  * Relative humidity used internally (e.g., for imputation and checks)
    is derived from actual vapor pressure at daily mean air temperature
    relative to saturation over liquid water and limited to 100 %;
    thus, it is consistent with actual vapor pressure, except on days when
    actual vapor pressure exceeds saturation at daily mean air temperature.
    Actual vapor pressure is not limited to saturation at daily mean air
    temperature because daily mean vapor pressure can exceed it
    (e.g., from maximum and minimum relative humidity);
    potential evapotranspiration limits vapor pressure to saturation.
  * Relative humidity inputs > 100 % and relative humidity > 100 % that is
    derived from vapor pressure, dew point, or specific humidity inputs
    (e.g., dew point above daily mean air temperature) cause an error
    unless the fix "Reset percentages" in \ref weathsetupin is activated
    (which limits relative humidity to 100 % and leaves actual vapor pressure
    unchanged). Relative humidity > 100 % that is derived from maximum and
    minimum relative humidity is limited to 100 % without an error.
  * Humidity that cannot be calculated because air temperature is missing
    is calculated after imputation of air temperature (see weather generator
    in \ref weathsetupin); on such days, maximum and minimum relative humidity
    are represented by their mean (less accurate, see above);
    imputed relative humidity is derived from
    (observed or imputed) actual vapor pressure.
  * Potential evapotranspiration uses saturation vapor pressure over ice
    below 0 C (and over liquid water otherwise) and a vapor pressure deficit
    of at least 0.

### Scaling

Monthly scaling parameters in \ref weathsetupin modify daily weather values
for each calendar month (applied after imputation of missing values):
  * Additive: maximum and minimum air temperature, cloud cover,
    relative humidity (limited to 0 - 100 %)
  * Multiplicative: precipitation, wind speed, actual vapor pressure,
    shortwave radiation (the latter three limited to values \f$\geq 0\f$)

Humidity scaling is applied to actual vapor pressure, and relative humidity
follows (order: temperature, relative humidity, actual vapor pressure):
  * Temperature scaling changes actual vapor pressure in proportion to
    saturation vapor pressure at daily mean air temperature, i.e.,
    relative humidity (and not vapor pressure) is held constant.
    This applies to every humidity input, including vapor pressure,
    dew point, and specific humidity.
  * Relative humidity scaling is additive to relative humidity as calculated
    from actual vapor pressure and limited to 0 - 100 %; supersaturated
    vapor pressure (relative humidity > 100 %) is not increased further
    (i.e., the upper limit is the larger of 100 % and the unscaled value).
  * Actual vapor pressure scaling is multiplicative and is applied
    to the temperature-adjusted actual vapor pressure.
    Thus, if temperature is scaled as well, then actual vapor pressure scaling
    represents the change in relative humidity terms;
    a scaling factor that represents the total change of vapor pressure
    (e.g., a ratio of future to historical vapor pressure) needs to be divided
    by the ratio of saturation vapor pressure
    \f$e^{\circ}_w(T + \Delta T) / e^{\circ}_w(T)\f$
    to avoid double counting the effect of temperature.
  * Since version 8.5.0, all three humidity-related scaling parameters
    affect both potential evapotranspiration and solar radiation
    (previously, temperature scaling did not modify humidity,
    relative humidity scaling affected only potential evapotranspiration, and
    actual vapor pressure scaling affected only solar radiation).



<hr>
\section explain_inputs Input files

SOILWAT2 needs the following input files for a simulation run:
\secreflist
\refitem filesin files.in
\refitem modelrunin modelrun.in
\refitem domainin domain.in
\refitem siteparamin siteparam.in
\refitem soilsin soils.in
\refitem swrcpin swrc_params.in
\refitem weathsetupin weathsetup.in
\refitem mkvprobin mkv_prob.in
\refitem mkvcovarin mkv_covar.in
\refitem climatein climate.in
\refitem vegin veg.in
\refitem estabin estab.in
\refitem carbonin carbon.in
\refitem swcsetupin swcsetup.in
\refitem outsetupin outsetup.in
\endsecreflist

Additional inputs for nc-based runs include:
\secreflist
\refitem desc_nc desc_nc.in
\refitem SW2_netCDF_input_variables SW2_netCDF_input_variables.tsv
\refitem SW2_netCDF_output_variables SW2_netCDF_output_variables.tsv
\endsecreflist

and any identified `"netCDF"` input files.

<br>
<hr>


\section filesin files.in
\verbinclude files.in

Go back to the \ref explain_inputs "list of input files"
<hr>

\section modelrunin modelrun.in
\verbinclude tests/example/Input/modelrun.in

Go back to the \ref explain_inputs "list of input files"
<hr>

\section domainin domain.in
\verbinclude tests/example/Input/domain.in

Go back to the \ref explain_inputs "list of input files"
<hr>

\section siteparamin siteparam.in
\verbinclude tests/example/Input/siteparam.in

Go back to the \ref explain_inputs "list of input files"
<hr>

\section soilsin soils.in
\verbinclude tests/example/Input/soils.in

Go back to the \ref explain_inputs "list of input files"
<hr>

\section swrcpin swrc_params.in
\verbinclude tests/example/Input/swrc_params.in

Go back to the \ref explain_inputs "list of input files"
<hr>

\section weathsetupin weathsetup.in
See also \ref weather_inputs "weather inputs".
\verbinclude tests/example/Input/weathsetup.in

Go back to the \ref explain_inputs "list of input files"
<hr>

\section mkvprobin mkv_prob.in
\verbinclude tests/example/Input/mkv_prob.in

Go back to the \ref explain_inputs "list of input files"
<hr>

\section mkvcovarin mkv_covar.in
\verbinclude tests/example/Input/mkv_covar.in

Go back to the \ref explain_inputs "list of input files"
<hr>

\section climatein climate.in
\verbinclude tests/example/Input/climate.in

Go back to the \ref explain_inputs "list of input files"
<hr>

\section vegin veg.in
\verbinclude tests/example/Input/veg.in

Go back to the \ref explain_inputs "list of input files"
<hr>

\section estabin estab.in
\verbinclude tests/example/Input/estab.in
<hr>
Go back to the \ref explain_inputs "list of input files"

\section carbonin carbon.in
\verbinclude tests/example/Input/carbon.in

Go back to the \ref explain_inputs "list of input files"
<hr>

\section swcsetupin swcsetup.in
\verbinclude tests/example/Input/swcsetup.in

Go back to the \ref explain_inputs "list of input files"
<hr>

\section outsetupin outsetup.in
\verbinclude tests/example/Input/outsetup.in

Go back to the \ref explain_inputs "list of input files"
<hr>

Additional inputs for `"nc"`-based runs include:

\section desc_nc desc_nc.in
\verbinclude tests/example/Input_nc/desc_nc.in

Go back to the \ref explain_inputs "list of input files"
<hr>

\section SW2_netCDF_input_variables SW2_netCDF_input_variables.tsv

This input file lists, activates, and describes each input variable
from `"netCDF"` files.

Every cell of an input row must contain a value
(use "NA" to indicate a cell without a value);
however, an entire row may be empty.

`"indexSpatial"` contain spatial lookup indices that translate the spatial
setup (sites/grid cells) of the simulation domain to the spatial setup of the
input `"netCDFs"` if grid mappings are matching.
SOILWAT2 can generate these automatically
if needed and if not provided by the user.

The two variables `"domain"` and `"progress"` of the `"inDomain"` input group
are always required. The provided information must match corresponding inputs
from "desc_nc.in" (\ref desc_nc).

\includedoc doc/additional_pages/Description__SW2_netCDF_input_variables.md

**The file SW2_netCDF_input_variables.tsv:**
\verbinclude tests/example/Input_nc/SW2_netCDF_input_variables.tsv

Go back to the \ref explain_inputs "list of input files"
<hr>

\section SW2_netCDF_output_variables SW2_netCDF_output_variables.tsv

This input file lists, activates, and describes each output variable in nc-mode.

The file names of the output `"netCDF"` follows the pattern
    `outkey_years_timestep.nc`
where
    * `"outkey"` represents the `"SW2 output group"`
    * `"years"` represents calendar year(s), e.g., 1980 or 1980-1990
    * `"timestep"` represents the output time step with possible values of
      day, week, month, season and year

Note that a week can either be defined as a heptad (default) or a pentad.

\includedoc doc/additional_pages/Description__SW2_netCDF_output_variables.md

**The file SW2_netCDF_output_variables.tsv:**
\verbinclude tests/example/Input_nc/SW2_netCDF_output_variables.tsv

Go back to the \ref explain_inputs "list of input files"
<hr>


<hr>
Go back to the [main page](README.md) or
[user guide](doc/additional_pages/A_SOILWAT2_user_guide.md).
