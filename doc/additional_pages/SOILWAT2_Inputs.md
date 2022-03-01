# SOILWAT2 Inputs

[SOILWAT2]: https://github.com/DrylandEcology/SOILWAT2

Note: this document is best viewed as part of the doxygen-built documentation
(there may be text artifacts if viewed as standalone-markdown).

<br>

### Example
  * The source code contains a complete example simulation project in `testing/`
  * Copy the executable to the testing path, modify inputs as desired,
    and run a simulation, e.g.,
```{.sh}
    make bint bint_run
```
    or, equivalently,
```{.sh}
    make bin
    cp SOILWAT2 testing/
    cd testing/
    ./SOILWAT2
```

  * The inputs comprise the master file `files.in` and the content of the
    `Input/` folder. They are explained in detail
    \ref explain_inputs "below".
  * The outputs are written to `Output/` including a logfile that contains
    warnings and errors. Outputs are explained in detail
    [here](doc/additional_pages/SOILWAT2_Outputs.md).

<br>


<hr>
\section explain_inputs Input files

SOILWAT2 needs the following input files for a simulation run:
\secreflist
\refitem filesin files.in
\refitem yearsin years.in
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

<br>
<hr>


\section filesin files.in
\verbinclude files.in

Go back to the \ref explain_inputs "list of input files"
<hr>

\section yearsin years.in
\verbinclude testing/Input/years.in

Go back to the \ref explain_inputs "list of input files"
<hr>

\section siteparamin siteparam.in
\verbinclude testing/Input/siteparam.in

Go back to the \ref explain_inputs "list of input files"
<hr>

\section soilsin soils.in
\verbinclude testing/Input/soils.in

Go back to the \ref explain_inputs "list of input files"
<hr>

\section swrcpin swrc_params.in
\verbinclude testing/Input/swrc_params.in

Go back to the \ref explain_inputs "list of input files"
<hr>

\section weathsetupin weathsetup.in
\verbinclude testing/Input/weathsetup.in

Go back to the \ref explain_inputs "list of input files"
<hr>

\section mkvprobin mkv_prob.in
\verbinclude testing/Input/mkv_prob.in

Go back to the \ref explain_inputs "list of input files"
<hr>

\section mkvcovarin mkv_covar.in
\verbinclude testing/Input/mkv_covar.in

Go back to the \ref explain_inputs "list of input files"
<hr>

\section climatein climate.in
\verbinclude testing/Input/climate.in

Go back to the \ref explain_inputs "list of input files"
<hr>

\section vegin veg.in
\verbinclude testing/Input/veg.in

Go back to the \ref explain_inputs "list of input files"
<hr>

\section estabin estab.in
\verbinclude testing/Input/estab.in
<hr>
Go back to the \ref explain_inputs "list of input files"

\section carbonin carbon.in
\verbinclude testing/Input/carbon.in

Go back to the \ref explain_inputs "list of input files"
<hr>

\section swcsetupin swcsetup.in
\verbinclude testing/Input/swcsetup.in

Go back to the \ref explain_inputs "list of input files"
<hr>

\section outsetupin outsetup.in
\verbinclude testing/Input/outsetup.in

Go back to the \ref explain_inputs "list of input files"
<hr>



<hr>
Go back to the [main page](README.md) or
[user guide](doc/additional_pages/A_SOILWAT2_user_guide.md).
