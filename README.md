<!-- badges: start -->
[![gh nix build status][1]][2]
[![gh win build status][3]][2]
[![github release][5]][6]
[![DOI][7]][8]
[![license][9]][10]
[![codecov status][11]][12]
[![doc status][4]][2]
<!-- badges: end -->


[1]: https://github.com/DrylandEcology/SOILWAT2/actions/workflows/main_nix.yml/badge.svg?branch=master
[2]: https://github.com/DrylandEcology/SOILWAT2/actions/workflows
[3]: https://github.com/DrylandEcology/SOILWAT2/actions/workflows/main_win.yml/badge.svg?branch=master
[4]: https://github.com/DrylandEcology/SOILWAT2/actions/workflows/check_doc.yml/badge.svg?branch=master

[5]: https://img.shields.io/github/release/DrylandEcology/SOILWAT2.svg
[6]: https://github.com/DrylandEcology/SOILWAT2/releases
[7]: https://zenodo.org/badge/9551524.svg
[8]: https://zenodo.org/badge/latestdoi/9551524
[9]: https://img.shields.io/github/license/DrylandEcology/SOILWAT2.svg
[10]: https://www.gnu.org/licenses/gpl.html
[11]: https://codecov.io/gh/DrylandEcology/SOILWAT2/branch/master/graph/badge.svg
[12]: https://codecov.io/gh/DrylandEcology/SOILWAT2

[SOILWAT2]: https://github.com/DrylandEcology/SOILWAT2
[rSOILWAT2]: https://github.com/DrylandEcology/rSOILWAT2
[STEPWAT2]: https://github.com/DrylandEcology/STEPWAT2
[issues]: https://github.com/DrylandEcology/SOILWAT2/issues
[pull request]: https://github.com/DrylandEcology/SOILWAT2/pulls
[doxygen]: https://github.com/doxygen/doxygen
[netCDF]: https://downloads.unidata.ucar.edu/netcdf/
[udunits2]: https://downloads.unidata.ucar.edu/udunits/

<br>


# SOILWAT2

SOILWAT2 is an ecosystem water balance simulation model.

This repository of `SOILWAT2` contains the same code that is
used by [rSOILWAT2][] and [STEPWAT2][].

If you utilize this model, please cite appropriate references, and we would
like to hear about your particular study (especially a copy of any published
paper).


Some references

* Bradford, J. B., D. R. Schlaepfer, and W. K. Lauenroth. 2014. Ecohydrology of
  adjacent sagebrush and lodgepole pine ecosystems: The consequences of climate
  change and disturbance. Ecosystems 17:590-605.
* Palmquist, K.A., Schlaepfer, D.R., Bradford, J.B., and Lauenroth, W.K. 2016.
  Mid-latitude shrub steppe plant communities: climate change consequences for
  soil water resources. Ecology 97:2342–2354.
* Schlaepfer, D. R., W. K. Lauenroth, and J. B. Bradford. 2012. Ecohydrological
  niche of sagebrush ecosystems. Ecohydrology 5:453-466.

<br>


## Table of contents

1. [How to get started](#get_started)
    1. [Compilation](#compile)
    2. [Documentation](#get_documentation)
2. [How to contribute](#contribute)

<br>



<a name="get_started"></a>
## How to get started

SOILWAT2 comes with a
[detailed manual](doc/additional_pages/A_SOILWAT2_user_guide.md)
and short overviews of
[inputs](doc/additional_pages/SOILWAT2_Inputs.md)
and [outputs](doc/additional_pages/SOILWAT2_Outputs.md).
A full code documentation may be built, see [here](#get_documentation).


<a name="compile"></a>
### Compilation
  * Requirements:
    - the `gcc` or `clang/llvm` toolchains compliant with `C99`
      - for unit tests (using `googletest`)
        - toolchains compliant with `C++17`
        - `POSIX API`
    - GNU-compliant `make`
    - On Windows OS: an installation of `cygwin`
    - the `netCDF-C` library (if compiled with [netCDF][] support)
    - the `udunits2` library (if compiled with [udunits2][] support)

  * Clone the repository
    (details can be found in the
    [manual](doc/additional_pages/A_SOILWAT2_user_guide.md)), for instance,
```{.sh}
        git clone --recursive https://github.com/DrylandEcology/SOILWAT2.git SOILWAT2
```

  * Build with `make` (see `make help` to print information about all
    available targets). For instance,
```{.sh}
        make CPPFLAGS=-DSWTXT   # text-based mode (equivalent to `make`)
        make CPPFLAGS=-DSWNC    # netCDF-based mode with units
```
<br>


<a name="get_documentation"></a>
### Documentation
  * Use [doxygen][] to generate help pages (locally) on the command-line with
    `make doc` (which basically runs `doxygen doc/Doxyfile`)

  * View documentation in your browser with `make doc_open`

<br>


<a name="contribute"></a>
## How to contribute
You can help us in different ways

1. Reporting [issues][]
2. Contributing code and sending a [pull request][]

Thank you!

Please follow our code development and contribution guidelines
[here](doc/additional_pages/code_contribution_guidelines.md)

<br>
