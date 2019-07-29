\page page_manual A SOILWAT2 user guide


[SOILWAT2]: https://github.com/DrylandEcology/SOILWAT2
[issues]: https://github.com/DrylandEcology/SOILWAT2/issues
[rSOILWAT2]: https://github.com/DrylandEcology/rSOILWAT2
[cygwin]: https://www.cygwin.com
[docker]: https://www.docker.com
[xcode]: https://developer.apple.com/xcode
[doxygen]: https://github.com/doxygen/doxygen


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
    it with a minimal reproducible example as an [issues][].
  * However, we do not have the resources to offer support.

<br>

### Minimal requirements
  - on Windows OS, a `*nix` emulator, e.g., an
    installation of [cygwin][] or [docker][]
  - on Mac OSX, [xcode][], xcode command line tools `xcode-select --install`,
    and having agreed to the xcode license `xcodebuild -license`
  - additionally,
    - the `gcc` or `clang/llvm` toolchains;
      ideally, `gcc >= v4.9` or `clang >= v3.3`
    - POSIX- or GNU-compliant `make`
    - `git` to download the code
  - optional: `doxgyen` (ideally `>= v1.8.15`) to generate a local copy of the
    documentation

<br>




### Download
  * Downloading the `SOILWAT2` code as `zip` file (via green button
    on website) or directly from
    [here](https://github.com/DrylandEcology/SOILWAT2/archive/master.zip)
    doesn't work out of the box because this wouldn't include code for the
    "submodules".

  * Use `git` to download/clone the [SOILWAT2][] from `github`, e.g.,
```{.sh}
    git clone -b master --single-branch --recursive https://github.com/DrylandEcology/SOILWAT2.git SOILWAT2
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
  * Generate help pages (locally)
```{.sh}
      cd SOILWAT2/
      make doc
```

  * View documentation in your browser by opening `doc/html/index.html`
    - on Mac OSX, e.g., `open doc/html/index.html`
    - on a Linux-style platform, e.g., `xdg-open doc/html/index.html`

<br>



### Example
  * The source code contains a complete example simulation project in `testing/`
  * Copy the executable to the testing path and run the simulation, e.g.,
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

