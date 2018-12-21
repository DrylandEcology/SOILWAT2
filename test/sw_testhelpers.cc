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
extern SW_SITE SW_Site;

extern SW_MODEL SW_Model;

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



/**
  @brief Creates soil layers based on function arguments (instead of reading
    them from an input file as `_read_layers` does)

  @param nlyrs The number of soil layers to create.
  @sideeffect After deleting any previous data in the soil layer array
    `SW_Site.lyr`, it creates new soil layers based on the argument inputs.

  @note
    - This function is a modified version of the function `_read_layers` in
      `SW_Site.c`.
    - This function does not re-calculate `n_transp_rgn` and
      `_TranspRgnBounds`, i.e., the values from the previous and new soil layers
      must match (see function `SW_SIT_read`).
*/
void _set_layers(LyrIndex nlyrs, RealF *dmax, RealF *matricd, RealF *f_gravel,
  RealF *evco, RealF *trco_grass, RealF *trco_shrub, RealF *trco_tree,
  RealF *trco_forb, RealF *psand, RealF *pclay, RealF *imperm, RealF *soiltemp)
{

  RealF dmin = 0.0;
  SW_SITE *v = &SW_Site;

  LyrIndex lyrno;
  unsigned int i, k;

  // De-allocate and delete previous soil layers
  SW_SIT_clear_layers();
  v->n_layers = 0;
  v->n_evap_lyrs = 0;

  ForEachVegType(k)
  {
    v->n_transp_lyrs[k] = 0;
  }


  // Create new soil
  for (i = 0; i < nlyrs; i++)
  {
    // Create the next soil layer
    lyrno = _newlayer();

    v->lyr[lyrno]->width = dmax[i] - dmin;
    dmin = dmax[i];
    v->lyr[lyrno]->soilMatric_density = matricd[i];
    v->lyr[lyrno]->fractionVolBulk_gravel = f_gravel[i];
    v->lyr[lyrno]->evap_coeff = evco[i];

    ForEachVegType(k)
    {
      switch (k)
      {
        case SW_TREES:
          v->lyr[lyrno]->transp_coeff[k] = trco_tree[i];
          break;
        case SW_SHRUB:
          v->lyr[lyrno]->transp_coeff[k] = trco_shrub[i];
          break;
        case SW_FORBS:
          v->lyr[lyrno]->transp_coeff[k] = trco_forb[i];
          break;
        case SW_GRASS:
          v->lyr[lyrno]->transp_coeff[k] = trco_grass[i];
          break;
      }

      v->lyr[lyrno]->my_transp_rgn[k] = 0;

      if (GT(v->lyr[lyrno]->transp_coeff[k], 0.0))
      {
        v->n_transp_lyrs[k]++;
      }
    }

    v->lyr[lyrno]->fractionWeightMatric_sand = psand[i];
    v->lyr[lyrno]->fractionWeightMatric_clay = pclay[i];
    v->lyr[lyrno]->impermeability = imperm[i];
    v->lyr[lyrno]->sTemp = soiltemp[i];

    if (GT(v->lyr[lyrno]->evap_coeff, 0.0))
    {
      v->n_evap_lyrs++;
    }

    water_eqn(f_gravel[i], psand[i], pclay[i], lyrno);

    v->lyr[lyrno]->swcBulk_fieldcap = SW_SWPmatric2VWCBulk(f_gravel[i], 0.333,
      lyrno) * v->lyr[lyrno]->width;
    v->lyr[lyrno]->swcBulk_wiltpt = SW_SWPmatric2VWCBulk(f_gravel[i], 15,
      lyrno) * v->lyr[lyrno]->width;
    calculate_soilBulkDensity(matricd[i], f_gravel[i], lyrno);

    swprintf("L: %d/%d depth=%3.1f, width=%3.1f\n", i, lyrno, dmax[i],
      v->lyr[lyrno]->width);
  }

  if (v->deepdrain)
  {
    lyrno = _newlayer();
    v->lyr[lyrno]->width = 1.0;
  }
  swprintf("Last: %d/%d width=%3.1f\n", i, lyrno, v->lyr[lyrno]->width);

  // Re-initialize site parameters based on new soil layers
  init_site_info();
}
