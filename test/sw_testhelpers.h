#include "gtest/gtest.h"


#define length(array) (sizeof(array) / sizeof(*(array))) //get length of an array


void Reset_SOILWAT2_after_UnitTest(void);

void _set_layers(LyrIndex nlyrs, RealF dmax[], RealF matricd[], RealF f_gravel[],
  RealF evco[], RealF trco_grass[], RealF trco_shrub[], RealF trco_tree[],
  RealF trco_forb[], RealF psand[], RealF pclay[], RealF imperm[], RealF soiltemp[]);
