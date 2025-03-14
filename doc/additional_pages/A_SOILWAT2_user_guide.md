# A SOILWAT2 user guide

[clang/llvm]: https://clang.llvm.org
[cygwin]: https://www.cygwin.com
[docker]: https://www.docker.com
[doxygen]: https://github.com/doxygen/doxygen
[gcc]: https://gcc.gnu.org
[git]: https://git-scm.com
[issue]: https://github.com/DrylandEcology/SOILWAT2/issues
[rSOILWAT2]: https://github.com/DrylandEcology/rSOILWAT2
[SOILWAT2]: https://github.com/DrylandEcology/SOILWAT2
[tinytex]: https://yihui.name/tinytex/
[xcode]: https://developer.apple.com/xcode
[netCDF]: https://downloads.unidata.ucar.edu/netcdf/
[udunits2]: https://downloads.unidata.ucar.edu/udunits/


Note: this document is best viewed as part of the doxygen-built documentation
(there may be text artifacts if viewed as standalone-markdown).

<br>

The `SOILWAT2` program must be downloaded as source code and compiled from the
command line. The program is written in `C` and has no graphical user interface.
If you are more familiar with `R`, then you may prefer to use our R package
[rSOILWAT2][].

We are continuously checking the successful installation, compilation,
and passing of logical tests of `SOILWAT2` both on a `*nix` as well as on a
Windows OS platform (using `cygwin`).
You can confirm yourself that these checks are passing
by visiting the online `README` page of [SOILWAT2][] and double check that the
badges are green. Thus, if installation
doesn't work for you and our checks are passing, then it is likely a problem
on your side.
  * If you believe that you have found a bug in our code, then please report
    it with a minimal reproducible example as an [issue][].
  * However, we do not have the resources to offer support.

<br>

### Minimal requirements
  - on any platform:
    - to compile the `SOILWAT2` simulation program
      - `C99` compliant [gcc][] or [clang/llvm][] toolchains
    - to compile the `SOILWAT2` tests program (using `googletest`)
      - `C++14` compliant [gcc][] or [clang/llvm][] toolchains
         (see [googletest cxx support](https://github.com/google/oss-policies-info/blob/main/foundational-cxx-support-matrix.md));
      - `POSIX API` (needs to be activated on `cygwin`, see `makefile`)
    - GNU-compliant [make](https://www.gnu.org/software/make/)
    - [git][] to download the code from the `github` repository
  - additionally, on Windows OS:
    - a `*nix` emulator, e.g., an installation of [cygwin][] or [docker][]
  - on Mac OSX:
    - xcode command line tools (run `xcode-select --install` on the command line)
    - having agreed to the xcode license (run `xcodebuild -license`)
    - or, alternatively, the full [xcode][] installation
  - to build the documentation (optional)
    - [doxygen][] (ideally `>= v1.9.3`) and
    - a minimal `latex` installation (see below)
  - to build with [netCDF][] support (optional)
    - the `netCDF-C` library
  - to build with [udunits2][] support (optional)
    - the `udunits2` library


#### Example instructions for a minimal `latex` installation
  * details on [tinytex][]
  * install the R package `tinytex`
```{.r}
    install.packages("tinytex")
    tinytex::install_tinytex()
```
  * if you don't have write permission to `/usr/local/bin`, then appropriate symlinks
    are not generated; thus, locate the path to `tlmgr`,
    e.g., with help of `tinytex::tinytex_root()`, and fix symlinks with
    escalated privileges
```{.sh}
    sudo [path/to/]tlmgr path add
```

<br>




### Download
  * Note: Downloading the `SOILWAT2` code as `zip` file (via green button
    on website) or directly from
    [here](https://github.com/DrylandEcology/SOILWAT2/archive/master.zip)
    doesn't work out of the box because this will not download required code for the
    "submodules".

  * Use `git` to download the latest version of [SOILWAT2][] from our repository on
    [github](https://github.com), e.g.,
```{.sh}
    git clone -b master --single-branch --recursive https://github.com/DrylandEcology/SOILWAT2.git SOILWAT2
```

  * Download a specific version of [SOILWAT2][], e.g., version 5.0.0,
```{.sh}
    git clone -b v5.0.0 --single-branch --recursive https://github.com/DrylandEcology/SOILWAT2.git SOILWAT2
```

  * If you are a developer, then clone the repository (instead of a single branch), e.g.,
  ```{.sh}
    git clone --recursive https://github.com/DrylandEcology/SOILWAT2.git SOILWAT2
```
<br>


### Compilation
  * Compile an executable binary with text-based input/output, e.g.,
```{.sh}
    make
```
    or
```{.sh}
    make CPPFLAGS=-DSWTXT
```

  * Compile an executable binary with [netCDF][] support, e.g.,
```{.sh}
    make CPPFLAGS=-DSWNC
```

  * User-specified paths to [netCDF][] header and library can be provided, e.g.,
```{.sh}
    make CPPFLAGS=-DSWNETCDF NC_CFLAGS="-I/path/to/include" NC_LIBS="-L/path/to/lib"
```

  * User-specified username and hostname, e.g.,
```{.sh}
    make USERNAME=nobody HOSTNAME=nowhere
```

<br>


### Documentation
  * Code documentation
```{.sh}
    cd SOILWAT2/
    make doc
    make doc_open
```

  * Documentation of user inputs and outputs
    * \subpage doc/additional_pages/SOILWAT2_Inputs.md "SOILWAT2 Inputs"
    * \subpage doc/additional_pages/SOILWAT2_Outputs.md "SOILWAT2 Outputs"
<br>



### Example
  * The source code contains a complete example simulation project in
    `tests/example/`

  * The inputs comprise the configuration file `files.in` and
    the content of the `Input/` folder and `Input_nc/` folder if in nc-mode.
    Inputs are explained in detail
    [here](doc/additional_pages/SOILWAT2_Inputs.md).

  * Run the example simulation in text-mode, e.g.,
```{.sh}
    make bin_run
```
    or, equivalently,
```{.sh}
    make
    bin/SOILWAT2 -d ./tests/example -f files.in
```

  * Run the example simulation in nc-mode, e.g.,
```{.sh}
    make CPPFLAGS=-DSWNC bin_run
```
    or, equivalently,
```{.sh}
    make CPPFLAGS=-DSWNC
    bin/SOILWAT2 -d ./tests/example -f files.in
```

  * Warning and error messages, if any, are written to a
    logfile `logs/logfile.log`
    (the file name and path is controlled by input from `"files.in"`).

  * Simulation outputs are written to `Output/`.
        * In text-mode, the output files are in `.csv` format
        * In nc-mode, the output files are in `.nc` format
  * Outputs are explained in detail
    [here](doc/additional_pages/SOILWAT2_Inputs.md).

<br>

<hr>
Go back to the [main page](README.md).

