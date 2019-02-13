#include "gtest/gtest.h"
#include <assert.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <float.h>
#include <math.h>
#include <memory.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "../generic.h"
#include "../myMemory.h"
#include "../filefuncs.h"
#include "../rands.h"
#include "../Times.h"
#include "../SW_Defines.h"
#include "../SW_Times.h"
#include "../SW_Files.h"
#include "../SW_Carbon.h"
#include "../SW_Site.h"
#include "../SW_VegProd.h"
#include "../SW_VegEstab.h"
#include "../SW_Model.h"
#include "../SW_SoilWater.h"
#include "../SW_Weather.h"
#include "../SW_Markov.h"
#include "../SW_Sky.h"
#include "../SW_Control.h"

#include "sw_testhelpers.h"

extern char _firstfile[];

/** Initialize SOILWAT2 variables and read values from example input file
 */
void Reset_SOILWAT2_after_UnitTest(void) {
  SW_CTL_clear_model(swFALSE);

  SW_CTL_init_model(_firstfile);
  SW_CTL_obtain_inputs();

  // Next two function calls will require SW_Output.c (see issue #85 'Make SW_Output.c comptabile with c++ to include in unit testing code')
  // SW_OUT_set_ncol();
  // SW_OUT_set_colnames();
}
/*
  Deallocate all pointers in a list.
*/
void Deallocate_pointers(double **ptr_list){
  unsigned int i;
  for (i = 0; i < length(ptr_list); i++){
    delete[] ptr_list[i];
  }
}
