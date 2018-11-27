#include "gtest/gtest.h"


void Reset_SOILWAT2_after_UnitTest(void);

void _set_layers(RealF dmax[], RealF matricd[], RealF f_gravel[],
  RealF evco[], RealF trco_grass[], RealF trco_shrub[], RealF trco_tree[],
  RealF trco_forb[], RealF psand[], RealF pclay[], RealF imperm[], RealF soiltemp[]);
