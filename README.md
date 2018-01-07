| Unix | Windows | Release | License | Coverage | Downloads |
| :---- | :---- | :---- | :---- | :---- | :---- |
[ ![Travis build status][1]][2] | [![Appveyor build status][3]][4] | [ ![github release][5]][6] | [![license][7]][8] | [![codecov status][9]][10] | [![github downloads][11]][12] |

[1]: https://travis-ci.org/DrylandEcology/SOILWAT2.svg?branch=master
[2]: https://travis-ci.org/DrylandEcology/SOILWAT2
[3]: https://ci.appveyor.com/api/projects/status/noes9lralyjhen3t/branch/master?svg=true
[4]: https://ci.appveyor.com/project/dschlaep/soilwat2/branch/master
[5]: https://img.shields.io/github/release/DrylandEcology/SOILWAT2.svg?label=current+release
[6]: https://github.com/DrylandEcology/SOILWAT2/releases
[7]: https://img.shields.io/github/license/DrylandEcology/SOILWAT2.svg
[8]: https://www.gnu.org/licenses/gpl.html
[9]: https://codecov.io/gh/DrylandEcology/SOILWAT2/branch/master/graph/badge.svg
[10]: https://codecov.io/gh/DrylandEcology/SOILWAT2
[11]: https://img.shields.io/github/downloads/DrylandEcology/SOILWAT2/total.svg
[12]: https://github.com/DrylandEcology/SOILWAT2


# SOILWAT2

This version of SoilWat brings new features. This is the same
code that is used by [rSOILWAT2](https://github.com/DrylandEcology/rSOILWAT2) and
[STEPWAT2](https://github.com/DrylandEcology/STEPWAT2).

If you make use of this model, please cite appropriate references, and we would like to
hear about your particular study (especially a copy of any published paper).


Some recent references

* Bradford, J. B., D. R. Schlaepfer, and W. K. Lauenroth. 2014. Ecohydrology of adjacent
  sagebrush and lodgepole pine ecosystems: The consequences of climate change and
  disturbance. Ecosystems 17:590-605.
* Palmquist, K.A., Schlaepfer, D.R., Bradford, J.B., and Lauenroth, W.K. 2016.
  Mid-latitude shrub steppe plant communities: climate change consequences for soil water
  resources. Ecology 97:2342–2354.
* Schlaepfer, D. R., W. K. Lauenroth, and J. B. Bradford. 2012. Ecohydrological niche of
  sagebrush ecosystems. Ecohydrology 5:453-466.


## How to contribute
You can help us in different ways:

1. Reporting [issues](https://github.com/DrylandEcology/SOILWAT2/issues)
2. Contributing code and sending a [pull request](https://github.com/DrylandEcology/SOILWAT2/pulls)

__SOILWAT2 code is used as part of three applications__: stand-alone, as part of [STEPWAT2](https://github.com/DrylandEcology/STEPWAT2),
and as part of the R package [rSOILWAT2](https://github.com/DrylandEcology/rSOILWAT2)
* The files 'Makevars', 'SW_R_lib.c' and 'SW_R_lib.h' are used when compiling for
  [rSOILWAT2](https://github.com/DrylandEcology/rSOILWAT2) and ignored otherwise.
* The file 'SW_Main_Function.c' is used when compiling with
  [STEPWAT2](https://github.com/DrylandEcology/STEPWAT2) and ignored otherwise.

__Follow our guidelines__ as detailed [here](https://github.com/DrylandEcology/workflow_guidelines)

__Tests, documentation, and code__ form a trinity
- Code documentation
  * Use [doxygen](http://www.stack.nl/~dimitri/doxygen/) to write inline code documentation
  * Update help pages on the command-line with `doxygen Doxyfile`
- Code tests
  * Use [GoogleTest](https://github.com/google/googletest/blob/master/googletest/docs/Documentation.md)
  to add unit tests to the existing framework
  * Run unit tests locally on the command-line with
    ```
    make test     # compiles the unit-test binary/executable
    make test_run # executes the unit-test binary
    make cleaner
    ```
  * Development/feature branches can only be merged into master if they pass all checks


## Note

__Version numbers__

We attempt to follow guidelines of [semantic versioning](http://semver.org/) with version
numbers of MAJOR.MINOR.PATCH.


__Organization renamed from Burke-Lauenroth-Lab to DrylandEcology on Dec 22, 2017__

All existing information should [automatically be redirected](https://help.github.com/articles/renaming-a-repository/) to the new name.
Contributors are encouraged, however, to update local clones to [point to the new URL](https://help.github.com/articles/changing-a-remote-s-url/), i.e.,
```
git remote set-url origin https://github.com/DrylandEcology/SOILWAT2.git
```


__Repository renamed from SOILWAT to SOILWAT2 on Feb 23, 2017__

All existing information should [automatically be redirected](https://help.github.com/articles/renaming-a-repository/) to the new name.

Contributors are encouraged, however, to update local clones to [point to the new URL](https://help.github.com/articles/changing-a-remote-s-url/), i.e.,
```
git remote set-url origin https://github.com/DrylandEcology/SOILWAT2.git
```
