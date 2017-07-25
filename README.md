[![Travis-CI Build Status](https://travis-ci.org/Burke-Lauenroth-Lab/SOILWAT2.svg?branch=master)](https://travis-ci.org/Burke-Lauenroth-Lab/SOILWAT2)

# SOILWAT2

This version of SoilWat brings new features. This is the same
code that is used by [rSOILWAT2](https://github.com/Burke-Lauenroth-Lab/rSOILWAT2) and
[STEPWAT2](https://github.com/Burke-Lauenroth-Lab/STEPWAT2).

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


## For code contributors only

* The files 'Makevars', 'SW_R_lib.c' and 'SW_R_lib.h' are used when compiling for
  [rSOILWAT2](https://github.com/Burke-Lauenroth-Lab/rSOILWAT2) and ignored otherwise.
* The file 'SW_Main_Function.c' is used when compiling with
  [STEPWAT2](https://github.com/Burke-Lauenroth-Lab/STEPWAT2) and ignored otherwise.


### Version numbers

We attempt to follow guidelines of [semantic versioning](http://semver.org/) with version
numbers of MAJOR.MINOR.PATCH.


### How to contribute
You can help us in different ways:

1. Reporting [issues](https://github.com/Burke-Lauenroth-Lab/SOILWAT2/issues)
2. Contributing code and sending a [pull request](https://github.com/Burke-Lauenroth-Lab/SOILWAT2/pulls)


# Notes

__Repository renamed from SOILWAT to SOILWAT2 on Feb 23, 2017__

All existing information should [automatically be redirected](https://help.github.com/articles/renaming-a-repository/) to the new name.

Contributors are encouraged, however, to update local clones to [point to the new URL](https://help.github.com/articles/changing-a-remote-s-url/), i.e.,
```
git remote set-url origin https://github.com/Burke-Lauenroth-Lab/SOILWAT2.git
```
