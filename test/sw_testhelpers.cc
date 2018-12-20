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



	// This function is a modified version of the function _read_layers in SW_Site.c.
void _set_layers(RealF *dmax, RealF *matricd, RealF *f_gravel,
  RealF *evco, RealF *trco_grass, RealF *trco_shrub, RealF *trco_tree,
  RealF *trco_forb, RealF *psand, RealF *pclay, RealF *imperm, RealF *soiltemp) {

  RealF dmin = 0.0, trco_veg[NVEGTYPES];
	SW_SITE *v = &SW_Site;

		LyrIndex lyrno, k;

	for (int i = 0; i < MAX_LAYERS; i++) {
    //printf("i: %x\nmatricd1: %f\n", i, matricd[i]);
    printf("I: %d\ndmax1: %f\n",i, dmax[i]);

    lyrno = _newlayer();

		v->lyr[lyrno]->width = dmax[i] - dmin;
		dmin = dmax[i];
		v->lyr[lyrno]->soilMatric_density = matricd[i];
		v->lyr[lyrno]->fractionVolBulk_gravel = f_gravel[i];
		v->lyr[lyrno]->evap_coeff = evco[i];

    //printf("matricd2: %f\n", SW_Site.lyr[lyrno]->soilMatric_density);
    printf("dmax2(width): %f\n", SW_Site.lyr[lyrno]->width);
    ForEachVegType(k)
		{
      if(k == SW_TREES){
        trco_veg[k] = trco_tree[i];
      }
      if(k == SW_SHRUB){
        trco_veg[k] = trco_shrub[i];
      }
      if(k == SW_FORBS){
        trco_veg[k] = trco_forb[i];
      }
      if(k == SW_GRASS){
        trco_veg[k] = trco_grass[i];
      }
    }
    ForEachVegType(k)
    {
			v->lyr[lyrno]->transp_coeff[k] = trco_veg[k];
			v->lyr[lyrno]->my_transp_rgn[k] = 0;
		}
		v->lyr[lyrno]->fractionWeightMatric_sand = psand[i];
		v->lyr[lyrno]->fractionWeightMatric_clay = pclay[i];
		v->lyr[lyrno]->impermeability = imperm[i];
		v->lyr[lyrno]->sTemp = soiltemp[i];

		water_eqn(f_gravel[i], psand[i], pclay[i], lyrno);

		v->lyr[lyrno]->swcBulk_fieldcap = SW_SWPmatric2VWCBulk(f_gravel[i], 0.333, lyrno) * v->lyr[lyrno]->width;


  	v->lyr[lyrno]->swcBulk_wiltpt = SW_SWPmatric2VWCBulk(f_gravel[i], 15, lyrno) * v->lyr[lyrno]->width;

  	calculate_soilBulkDensity(matricd[i], f_gravel[i], lyrno);
    //printf("matricd4: %f\n", SW_Site.lyr[lyrno]->soilMatric_density);
    printf("dmax4(width): %f\n", SW_Site.lyr[lyrno]->width);
	   /* n_layers set in _newlayer() */

     //printf("matricd3: %f\n", matricd[i]);
     printf("dmax3: %f\n", dmax[i]);
     //printf("matricd5: %f\n", SW_Site.lyr[lyrno]->soilMatric_density);

  }

  if (v->deepdrain) {
    lyrno = _newlayer();
    v->lyr[lyrno]->width = 1.0;
  }
  printf("dmax5(width): %f\n", SW_Site.lyr[lyrno]->width);

}
