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
[guidelines]: https://github.com/DrylandEcology/workflow_guidelines
[doxygen]: https://github.com/doxygen/doxygen
[GoogleTest]: https://github.com/google/googletest
[semantic versioning]: https://semver.org/

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
    1. [SOILWAT2 code](#SOILWAT2_code)
    2. [Code guidelines](#follow_guidelines)
    3. [Code documentation](#code_documentation)
    4. [Code tests](#code_tests)
    5. [Code debugging](#code_debugging)
    6. [Code versioning](#code_versioning)
    7. [Reverse dependencies](#revdep)
3. [Some additional notes](#more_notes)

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
    - the `gcc` or `clang/llvm` toolchains compliant with `C11`
      - for unit tests (using `googletest`), additionally,
        - `g++ >= v5.0` or `clang++ >= v5.0` compliant with `C++11`
        - `POSIX API`
    - GNU-compliant `make`
    - On Windows OS: an installation of `cygwin`

  * Clone the repository
    (details can be found in the
    [manual](doc/additional_pages/A_SOILWAT2_user_guide.md)), for instance,
    ```{.sh}
        git clone --recursive https://github.com/DrylandEcology/SOILWAT2.git SOILWAT2
    ```

  * Build with `make` (see `make help` to print information about all
    available targets), for instance,
    ```{.sh}
        cd SOILWAT2/
        make bin
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
You can help us in different ways:

1. Reporting [issues][]
2. Contributing code and sending a [pull request][]

<br>


<a name="SOILWAT2_code"></a>
### `SOILWAT2` code is used as part of three applications
  * Stand-alone,

  * Part/submodule of [STEPWAT2][] (code flag `STEPWAT`), and

  * Part/submodule of the R package [rSOILWAT2][] (code flag `RSOILWAT`)

Changes in `SOILWAT2` must be reflected by updates to `STEPWAT2` or `rSOILWAT2`;
please see section [reverse dependencies](#revdep).

<br>


<a name="follow_guidelines"></a>
### Follow our guidelines as detailed [here][guidelines]

<br>


### Code development, documentation, and tests go together

We develop code on development branches and,
after they are reviewed and pass our checks,
merge them into the master branch for release.


<a name="code_documentation"></a>
#### Code documentation
  * Document new code with [doxygen][] inline documentation

  * Check that new documentation renders correctly and does not
    generate `doxygen` warnings, i.e.,
    run `make doc` and check that it returns successfully
    (it checks that `doxygen doc/Doxyfile | grep warning` is empty).
    Also check that new or amended documentation displays
    as intended by opening `doc/html/index.html` and navigating to the item
    in question

  * Keep `doc/html/` local, i.e., don't push to the repository

  * Use regular c-style comments for additional code documentation

<br>


<a name="code_tests"></a>
#### Code tests
__Testing framework__

The goal is to cover all code development with new or amended tests.
`SOILWAT2` comes with unit tests and integration tests.
Additionally, the github repository runs continuous integration checks.

<br>

__Unit tests__

We use [GoogleTest][] for unit tests
to check that individual units of code, e.g., functions, work as expected.

These tests are organized in the folder `test/`
in files with the naming scheme `test_*.cc`.

Note: `SOILWAT2` is written in C whereas `GoogleTest` is a C++ framework. This
causes some complications, see `makefile`.


Run unit tests locally on the command-line with
```{.sh}
      make test test_run         # compiles and executes the unit-tests
      make test_severe test_run  # compiles/executes with strict/severe flags
      make clean                 # cleans build artifacts
```


__Miscellaneous scripts for tests__

Users of `SOILWAT2` work with a variety of compilers across different platforms
and we aim to test that this works across a reasonable selection.
We can do that manually or use the bash-script `tools/check_SOILWAT2.sh`
which runs tests with different compiler versions.
Please note that this script currently works only with `macports`.


`SOILWAT2` is a deterministic simulation model; however, running unit tests
repeatedly may be helpful for debugging in rare situations. For that,
the bash-script `tools/many_test_runs.sh` will run `N` number of times and
only reports unit test failures, e.g.,
```{.sh}
      ./tools/many_test_runs.sh        # will run a default (currently, 10) number of times
      N=3 ./tool/many_test_runs.sh     # will run 3 replicates
```

<br>

__Integration tests__

We use integration tests to check that the entire simulation model works
as expected when used in a real-world application setting.

The folder `tests/example/` contains all necessary inputs to run `SOILWAT2`
for one generic location
(it is a relatively wet and cool site in the sagebrush steppe).

```{.sh}
      make bin_run
```

The simulated output is stored at `tests/example/Output/`.


Another use case is to compare output of a new (development) branch to output
from a previous (reference) release.

Depending on the purpose of the development branch
the new output should be exactly the same as reference output or
differ in specific ways in specific variables.

The following steps provide a starting point for such comparisons:

```{.sh}
      # Simulate on refernce branch and copy output to "Output_ref"
      git checkout master
      make bin_run
      cp -r tests/example/Output tests/example/Output_ref

      # Switch to development branch <branch_xxx> and run the same simulation
      git checkout <branch_xxx>
      make bin_run

      # Compare the two sets of outputs
      #   * Lists all output files and determine if they are exactly they same
      diff tests/example/Output/ tests/example/Output_ref/ -qs
```


__Additional tests__

Additional output can be generated by passing appropriate flags when running
unit tests. Scripts are available to analyze such output.
Currently, the following is implemented:

  - Sun hour angles plots for horizontal and tilted surfaces

    1. Numbers of daylight hours and of sunrise(s)/sunset(s)
       for each latitude and day of year for some slope/aspect combinations
    2. Sunrise(s)/sunset(s) hour angles for each latitude
       and some slope/aspect/day of year combinations

```{.sh}
      CPPFLAGS=-DSW2_SolarPosition_Test__hourangles_by_lat_and_doy make test_run
      Rscript tools/plot__SW2_SolarPosition_Test__hourangles_by_lat_and_doy.R

      CPPFLAGS=-DSW2_SolarPosition_Test__hourangles_by_lats make test_run
      Rscript tools/plot__SW2_SolarPosition_Test__hourangles_by_lats.R
```

  - PET plots as function of radiation, relative humidity, wind speed, and cover

```{.sh}
      CPPFLAGS=-DSW2_PET_Test__petfunc_by_temps make test_run
      Rscript tools/plot__SW2_PET_Test__petfunc_by_temps.R
```

<br>


__Continous integration checks__

Development/feature branches can only be merged into master if they pass
all checks on the continuous integration servers.
Running the following tests locally helps to increase chances that
they will work well on the servers as well:
```{.sh}
      make clean bin_debug_severe bint_run
      make clean test_severe test_run
```

<br>

__Sanitizer__

Running tests with the `severe` targets may require excluding known memory leaks
(see [issue #205](https://github.com/DrylandEcology/SOILWAT2/issues/205)):

```{.sh}
      ASAN_OPTIONS=detect_leaks=1 LSAN_OPTIONS=suppressions=.LSAN_suppr.txt make clean test_severe test_run
```

The address sanitizer may not work correctly and/or fail when used with the
`Apple-clang` version that is shipped with macOS X
(see [Sanitizer issue #1026](https://github.com/google/sanitizers/issues/1026)).
A separate installation of `clang` may be required,
e.g., via `homebrew` or `macports`.


If `clang` is installed in a non-default location and
if shared dynamic libraries are not picked up correctly, then
the test executable may throw an error `... dyld: Library not loaded ...`.
This can be fixed, for instance, with the following steps
(details depend on the specific setup, below is for `macports` and `clang-8.0`):

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

  * Alternatively, use the pre-configured debugging targets
    `bin_debug` and `bin_debug_severe`, for instance, with

```{.sh}
    make bin_debug_severe bint_run
```

  * If **valgrind** is installed, then call the target `bind_valgrind`
    (see description in `makefile`) with

```
    make bind_valgrind
```

<br>



<a name="code_versioning"></a>
#### Version numbers

We attempt to follow guidelines of [semantic versioning][] with version
numbers of `MAJOR.MINOR.PATCH`;
however, our version number updates are focusing
on simulated output (e.g., identical output -> increase patch number) and
on dependencies `STEPWAT2` and `rSOILWAT2`
(e.g., no updates required -> increase patch number).

We create a new release for each update to the master branch.
The master branch is updated via pull requests from development branches
after they are reviewed and pass required checks.


<br>


<a name="revdep"></a>
## Reverse dependencies

`STEPWAT2` and `rSOILWAT2` depend on `SOILWAT2`;
they utilize the master branch of `SOILWAT2` as a submodule.
Thus, changes in `SOILWAT2` need to be propagated to `STEPWAT2` and `rSOILWAT2`.

The following steps can serve as starting point to resolve
the cross-repository reverse dependencies:

  1. Create development branch `branch_*` in `SOILWAT2`
  1. Create respective development branches in `STEPWAT2` and `rSOILWAT2`
  1. Update the `SOILWAT2` submodule of `STEPWAT2` and `rSOILWAT2` as first
     commit on these new development branches:
      * Have `.gitmodules` point to the new `SOILWAT2` branch `branch_*`
      * Update the submodule `git submodule update --remote`
  1. Develop and test code and follow guidelines of `STEPWAT2` and `rSOILWAT2`
  1. Create pull requests for each development branch
  1. Merge pull request `SOILWAT2` once development is finalized, reviewed, and
     sufficiently tested across all three repositories;
     create new `SOILWAT2` [release](#code_versioning)
  1. Finalize development branches in `STEPWAT2` and `rSOILWAT2`
      * Have `.gitmodules` point to the new `SOILWAT2` release on `master`
      * Update the submodule `git submodule update --remote`
  1. Handle pull requests for `STEPWAT2` and `rSOILWAT2` according to
     their guidelines

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
