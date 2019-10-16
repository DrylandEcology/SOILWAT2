\page page_manual A SOILWAT2 user guide


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


The `SOILWAT2` program must be downloaded as source code and compiled from the
command line. The program is written in `C` and has no graphical user interface.
If you are more familiar with `R`, then you may prefer to use our R package
[rSOILWAT2][].

We are continuously checking the successful installation, compilation,
and passing of logical tests of `SOILWAT2` both on a `*nix` as well as on a
Windows OS platform. You can confirm yourself that these checks are passing
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
    - the [gcc][] or [clang/llvm][] toolchains;
      ideally, `gcc >= v4.9` or `clang >= v3.3`
    - POSIX- [make](https://pubs.opengroup.org/onlinepubs/9699919799/) or
      GNU-compliant [make](https://www.gnu.org/software/make/)
    - [git][] to download the code
  - additionally, on Windows OS:
    - a `*nix` emulator, e.g., an installation of [cygwin][] or [docker][]
  - on Mac OSX:
    - xcode command line tools (run `xcode-select --install` on the command line)
    - having agreed to the xcode license (run `xcodebuild -license`)
    - or, alternatively, the full [xcode][] installation
  - optional:
    - [doxygen][] (ideally `>= v1.8.16`) and a minimal `latex` installation (see below)
      to generate a local copy of the documentation



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
  * Compile an executable binary, e.g.,
```{.sh}
    cd SOILWAT2/
    make bin
```

<br>


### Documentation
  * Generate help pages (locally) and open them in your browser
```{.sh}
    cd SOILWAT2/
    make doc
    make doc_open
```

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
    `Input/` folder. They are explained in detail \subpage page_inputs "here".
  * The outputs are written to `Output/` including a logfile that contains
    warnings and errors. The output files are in `.csv` format and can be
    opened by a spreadsheet program (e.g., `LibreOffice` or `Excel`) or
    imported into `R`
    (e.g., `data <- read.csv("testing/Output/sw2_yearly.csv")`).
    Outputs are explained in detail \subpage page_outputs "here".

<br>

<hr>
Go back to the \ref index "main page".

