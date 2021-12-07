| *nix | Windows | Release | DOI | License | Coverage |
| :---- | :---- | :---- | :---- | :---- | :---- |
[ ![Travis build status][1]][2] | [![Appveyor build status][3]][4] | [ ![github release][5]][6] | [ ![DOI][7]][8] | [![license][9]][10] | [![codecov status][11]][12] |


[1]: https://travis-ci.org/DrylandEcology/SOILWAT2.svg?branch=master
[2]: https://travis-ci.org/DrylandEcology/SOILWAT2
[3]: https://ci.appveyor.com/api/projects/status/noes9lralyjhen3t/branch/master?svg=true
[4]: https://ci.appveyor.com/project/DrylandEcologyGit/soilwat2/branch/master
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
[guidelines]: https://github.com/DrylandEcology/workflow_guidelines
[doxygen]: https://github.com/doxygen/doxygen
[GoogleTest]: https://github.com/google/googletest
[semantic versioning]: https://semver.org/

<br>


# SOILWAT2

SOILWAT2 is an ecosystem water balance simulation model.

This version of SoilWat brings new features. This is the same code that is
used by [rSOILWAT2][] and [STEPWAT2][].

If you make use of this model, please cite appropriate references, and we would
like to hear about your particular study (especially a copy of any published
paper).


Some recent references

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
    1. [SOILWAT2 code](#SOILWAT2_code)
    2. [Code guidelines](#follow_guidelines)
    3. [Code documentation](#code_documentation)
    4. [Code tests](#code_tests)
    5. [Code debugging](#code_debugging)
    6. [Code versioning](#code_versioning)
3. [Some additional notes](#more_notes)

<br>



<a name="get_started"></a>
## How to get started

A detailed manual can be found
[here](`doc/additional_pages/A SOILWAT2 user guide.md`) or here \ref page_manual.

<a name="compile"></a>
### Compilation
  * Requirements:
    - the `gcc` or `clang/llvm` toolchains compliant with `C11`
      - for unit tests: `gcc >= v5.0` or `clang >= v5.0` compliant with `C++11`
    - POSIX- or GNU-compliant `make`
    - On Windows OS: an installation of `cygwin`

  * Build with `make` (see `make help` to print information about all
    available targets)

<br>


<a name="get_documentation"></a>
### Documentation
  * Use [doxygen][] to generate help pages (locally) on the command-line with
    `make doc` (which basically runs `doxygen doc/Doxyfile`)

  * View documentation in your browser with `make doc_open`

<br>


<a name="contribute"></a>
## How to contribute
You can help us in different ways:

1. Reporting [issues][]
2. Contributing code and sending a [pull request][]

<br>


<a name="SOILWAT2_code"></a>
### SOILWAT2 code is used as part of three applications
  * Stand-alone
    (Programmer note: code flag `SOILWAT` is defined if neither `STEPWAT` nor
    `RSOILWAT` exist),

  * Part/submodule of [STEPWAT2][] (code flag `STEPWAT`), and

  * Part/submodule of the R package [rSOILWAT2][] (code flag `RSOILWAT`)

<br>


<a name="follow_guidelines"></a>
### Follow our guidelines as detailed [here][guidelines]

<br>


### Tests, documentation, and code form a trinity

<a name="code_documentation"></a>
#### Code documentation
  * Use [doxygen][] to write inline documentation

  * Make sure that new documentation results in `doxygen` documentation without
    warnings, i.e., `make doc` is successful
    (it basically checks that `doxygen doc/Doxyfile | grep warning` is empty).
    Make also sure that newly created and amended documentation displays
    as intended by opening `doc/html/index.html` and navigating to the item
    in question

  * Keep `doc/html/` local, i.e., don't push to the repository

  * Use regular c-style comments to additionally document code

<br>


<a name="code_tests"></a>
#### Code tests
__Testing framework__

Use [GoogleTest][] to add unit tests to the existing framework in the
folder `test/` where each unit test file uses the naming scheme
`test/test_*.cc`.

Note: `SOILWAT2` is written in C whereas `GoogleTest` is a C++ framework. This
causes some complications, see `makefile`.

<br>

__Running unit tests__

Run unit tests locally on the command-line with
```{.sh}
      make test test_run         # compiles and executes the unit-tests
      make test_severe test_run  # compiles/executes with strict/severe flags
      make clean                 # cleans build artifacts
```

If you want to run unit tests repeatedly (e.g., to sample a range of
random numbers), then you may use the bash-script `tools/many_test_runs.sh`
which runs `N` number of times and reports only unit test failures, e.g.,
```{.sh}
      ./tools/many_test_runs.sh        # will run a default (currently, 10) number of times
      N=3 ./tool/many_test_runs.sh     # will run 3 replicates
```

<br>

__Integration tests__

Informal and local integration tests example:
  1. Before coding, run the example in `testing/` to produce reference output

```{.sh}
      git checkout master
      make bin bint_run
      cp -r testing/Output testing/Output_ref
```

  2. Develop your code and keep "testing/Output_ref" locally, i.e., don't
  include "testing/Output_ref" in commits

  3. Regularly, e.g., before finalizing a commit, check that new code produces
  identical output (that is unless you work on output...)

```{.sh}
      make bin bint_run
      diff testing/Output/ testing/Output_ref/ -qs
```


__Additional tests__

Additional output can be generated by passing appropriate flags when running
unit tests. Scripts are available to analyze such output. Currently, the
following output is implemented:

  - Sun hour angles plots for horizontal and tilted surfaces

    1. Numbers of daylight hours and of sunrise(s)/sunset(s)
       for each latitude and day of year for some slope/aspect combinations
    2. Sunrise(s)/sunset(s) hour angles for each latitude
       and some slope/aspect/day of year combinations

```{.sh}
      CPPFLAGS=-DSW2_SolarPosition_Test__hourangles_by_lat_and_doy make test test_run
      Rscript tools/plot__SW2_SolarPosition_Test__hourangles_by_lat_and_doy.R

      CPPFLAGS=-DSW2_SolarPosition_Test__hourangles_by_lats make test test_run
      Rscript tools/plot__SW2_SolarPosition_Test__hourangles_by_lats.R
```

  - PET plots as function of radiation, relative humidity, wind speed, and cover

```{.sh}
      CPPFLAGS=-DSW2_PET_Test__petfunc_by_temps make test test_run
      Rscript tools/plot__SW2_PET_Test__petfunc_by_temps.R
```

<br>


__Continous integration checks__

Development/feature branches can only be merged into master if they pass
all checks on `appveyor` and `travis` continuous integration servers, i.e.,
run the following locally to prepare a pull-request or commit to be reviewed
```{.sh}
      make clean bin_debug_severe bint_run
      make clean test_severe test_run
```

<br>

__Sanitizer__

You may want/need to exclude known memory leaks from severe testing
(see [issue #205](https://github.com/DrylandEcology/SOILWAT2/issues/205)):

```{.sh}
      ASAN_OPTIONS=detect_leaks=1 LSAN_OPTIONS=suppressions=.LSAN_suppr.txt make clean test_severe test_run
```

The address sanitizer may not work correctly and/or fail, if you use the
`Apple-clang` version shipped with macOS X. You may need to build `clang`
yourself
(see [Sanitizer issue #1026](https://github.com/google/sanitizers/issues/1026)):
e.g.,

```{.sh}
      sudo port install clang-8.0
      sudo port select --set clang mp-clang-8.0
```

Revert to the default version

```{.sh}
      sudo port select --set clang none
```

If you installed `clang` in a non-default location, then you
may need to also fix names of shared dynamic libraries in the test
executable if you get the error `... dyld: Library not loaded ...`, e.g.,

```{.sh}
      # build test executable with clang and leak detection
      CC=clang CXX=clang++ ASAN_OPTIONS=detect_leaks=1 LSAN_OPTIONS=suppressions=.LSAN_suppr.txt make clean test_severe

      # check faulty library path
      otool -L sw_test

      # figure out correct library path and insert with: e.g.,
      install_name_tool -change /opt/local/libexec/llvm-8.0/lib/libclang_rt.asan_osx_dynamic.dylib /opt/local/libexec/llvm-8.0/lib/clang/8.0.0/lib/darwin/libclang_rt.asan_osx_dynamic.dylib sw_test

      # run tests
      make test_run
```

<br>


<a name="code_debugging"></a>
#### Debugging
  Debugging is controlled at two levels:
  * at the preprocessor (pass `-DSWDEBUG`):
    all debug code is wrapped by this flag so that it does not end up in
    production code; unit testing is compiled in debugging mode.

  * in functions with local debug variable flags (`int debug = 1;`):
    debug code can be conditional on such a variable, e.g.,

```{.c}
    void foo() {
      #ifdef SWDEBUG
      int debug = 1;
      #endif
      ...
      #ifdef SWDEBUG
      if (debug) swprintf("hello, this is debugging code\n");
      ...
      #endif
      ...
    }
```

  * Clean, compile and run optimized `SOILWAT2`-standalone in debugging mode
    with, e.g.,

```{.sh}
    make bin bint_run CPPFLAGS=-DSWDEBUG
```

  * Alternatively and potentially preferably, you can use the pre-configured
    debugging targets `bin_debug` and `bin_debug_severe`, for instance, with

```{.sh}
    make bin_debug_severe bint_run
```

  * If **valgrind** is installed, then you can call the target `bind_valgrind`
    (see description in `makefile`) with

```
    make bind_valgrind
```

<br>



<a name="code_versioning"></a>
#### Version numbers

We attempt to follow guidelines of [semantic versioning][] with version
numbers of `MAJOR.MINOR.PATCH`.

<br>


<a name="more_notes"></a>
## Notes

__Organization renamed from Burke-Lauenroth-Lab to DrylandEcology on Dec 22, 2017__

All existing information should
[automatically be redirected](https://help.github.com/articles/renaming-a-repository/)
to the new name.
Contributors are encouraged, however, to update local clones to
[point to the new URL](https://help.github.com/articles/changing-a-remote-s-url/),
i.e.,

```{.sh}
git remote set-url origin https://github.com/DrylandEcology/SOILWAT2.git
```


__Repository renamed from SOILWAT to SOILWAT2 on Feb 23, 2017__

All existing information should
[automatically be redirected](https://help.github.com/articles/renaming-a-repository/)
to the new name.
Contributors are encouraged, however, to update local clones to
[point to the new URL](https://help.github.com/articles/changing-a-remote-s-url/),
i.e.,

```{.sh}
git remote set-url origin https://github.com/DrylandEcology/SOILWAT2.git
```
