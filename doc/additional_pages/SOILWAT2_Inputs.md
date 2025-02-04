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


  * The inputs comprise the main file `files.in` and the content of the
    `Input/` folder and `Input_nc/` if in nc-based mode.
    Inputs are explained in detail \ref explain_inputs "below".
  * The outputs are written to `Output/` including a logfile that contains
    warnings and errors. Outputs are explained in detail
    [here](doc/additional_pages/SOILWAT2_Outputs.md).

<br>

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
      day, week, month and year

\includedoc doc/additional_pages/Description__SW2_netCDF_output_variables.md

**The file SW2_netCDF_output_variables.tsv:**
\verbinclude tests/example/Input_nc/SW2_netCDF_output_variables.tsv

Go back to the \ref explain_inputs "list of input files"
<hr>


<hr>
Go back to the [main page](README.md) or
[user guide](doc/additional_pages/A_SOILWAT2_user_guide.md).
